#!/usr/bin/env bash
# Build a Pixel's kernel, modules and flashable images FROM THE SOURCE IN THE
# TREE (aosp/), with the tree's own hermetic toolchain.
#
#   build-device.sh pixel7 [bazel options...]
#   build-device.sh pixel7 --lto=none        faster dev build (no LTO/CFI)
#   BRANCH=<manifest branch> build-device.sh pixel7
#   STOCK=1 build-device.sh pixel7           stock config (no pKVM dev options)
#   PKVM_DEBUG=0 build-device.sh pixel7      leave out the hypervisor debug options
#                                            (NVHE_EL2_DEBUG etc.); the rest stays
#   PKVM_TRIM=1 build-device.sh pixel7       keep Google's KMI trimming (stock
#                                            exported symbols) and export only
#                                            what configs/pixel-kmi-extra.symbols
#                                            adds; default exports every symbol
#   PKVM_MODULES="tests/a tests/b" build-device.sh pixel7
#                                            your EL1+EL2 modules (default:
#                                            tests/hello_hvc; "" for none)
#
# Output: ignore/pixel/kernels/<tree>/out/<codename>/dist   (boot.img, vendor_dlkm.img, ...)
#
# Why not just run the tree's build_<codename>.sh? Its default is
# --use_prebuilt_gki=true: it downloads Google's prebuilt GKI kernel and only
# builds the vendor modules, so edits to aosp/ (including the pKVM hypervisor)
# would silently not make it into the images. --config=use_source_tree_aosp
# turns that off.
#
# Unless STOCK=1, the build is a pKVM dev build: configs/pixel-pkvm-dev_defconfig
# is applied, and the kernel exports every symbol instead of only the KMI list
# (--notrim), so your own EL1 modules can use any exported kernel function.
#
# Each dir in $PKVM_MODULES (repo-relative; an out-of-tree module laid out as in
# SETUP.md section 6) is built by Kleaf together with the kernel, packed into
# the first-stage ramdisk (vendor_kernel_boot.img) and added to
# kvm-arm.protected_modules, so the kernel loads its EL2 part before pKVM
# finalizes.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. "$HERE/env.sh"
. "$HERE/lib.sh"

[ $# -ge 1 ] || die "usage: $0 <device> [bazel options...]"
resolve_device "$1" "${BRANCH:-}"
shift

[ -d "$tree/.repo" ] || die "no tree at $tree; run setup-device.sh $device first"
script="$tree/build_$codename.sh"
[ -x "$script" ] || die "$script not found"

echo "device : $device ($phones)"
echo "tree   : $tree"
echo "dist   : $tree/out/$codename/dist"

repo=$(cd "$HERE/../.." && pwd)
pkg="$tree/private/devices/google/$codename"
ramdisk_list="$pkg/vendor_ramdisk.modules.$codename"

# The device tree of the SoC sets kvm-arm.protected_modules in its bootargs.
soc=$(sed -n 's|.*:\([a-z0-9]*\)_[a-z0-9]*_dist.*|\1|p' "$script")
dtsi=$(grep -rlm1 'kvm-arm.protected_modules=' "$tree/private/devices/google/$soc/dts" | head -1)
[ -n "$dtsi" ] || die "no kvm-arm.protected_modules in the $soc device tree"

# Module hookup edits three files: a dep in the device package's BUILD.bazel
# (marked "# pkvmi"), a line in its ramdisk module list, and the bootargs line of
# the device tree. Undo them on every run, so STOCK=1 or an empty PKVM_MODULES
# gives back the untouched tree.
sed -i '/# pkvmi$/d' "$pkg/BUILD.bazel"
sed -i '/^extra\/pkvmi\//d' "$ramdisk_list"
if ! git -C "${dtsi%/*}" diff --quiet -- "$dtsi"; then
	[ "$(git -C "${dtsi%/*}" diff -U0 -- "$dtsi" | grep -c '^[+-][^+-]')" = 2 ] ||
		die "$dtsi has edits besides pkvmi's; revert or commit them first"
	git -C "${dtsi%/*}" checkout -- "$dtsi"
fi

# PKVM_TRIM=1 adds a marked block of symbols to the GKI symbol list of the
# aosp tree; undo it on every run for the same reason.
abi="$tree/aosp/android/abi_gki_aarch64_pixel"
sed -i '/^# pkvmi begin$/,/^# pkvmi end$/d' "$abi"

# Bazel can only see files inside the workspace, so everything below is copied
# into a package of its own at the tree root (outside every git project there).
rm -rf "$tree/pkvmi"
dev_flags=()
config=stock
if [ -z "${STOCK:-}" ]; then
	config="pkvm dev"
	mkdir -p "$tree/pkvmi"
	echo 'exports_files(["pixel-pkvm-dev_defconfig"])' >"$tree/pkvmi/BUILD.bazel"
	if [ "${PKVM_DEBUG-1}" = 0 ]; then
		echo "# PKVM_DEBUG=0: no hypervisor debug options" >"$tree/pkvmi/pixel-pkvm-dev_defconfig"
		config="pkvm dev, no hyp debug options"
	else
		cp "$repo/configs/pixel-pkvm-dev_defconfig" "$tree/pkvmi/"
	fi
	dev_flags=(
		--nokmi_symbol_list_strict_mode
		--nokmi_symbol_list_violations_check
		--defconfig_fragment=//pkvmi:pixel-pkvm-dev_defconfig
		# the device's `kernel` target is private to its own package
		--check_visibility=false
	)
	if [ "${PKVM_TRIM-0}" = 1 ]; then
		{
			echo "# pkvmi begin"
			cat "$repo/configs/pixel-kmi-extra.symbols"
			echo "# pkvmi end"
		} >>"$abi"
		config="$config, trimmed KMI + extra symbols"
	else
		dev_flags+=(--notrim --download_prebuilt_gki_fips140=false)
	fi

	protected_modules=
	for dir in ${PKVM_MODULES-tests/hello_hvc}; do
		[ -d "$repo/$dir" ] || die "PKVM_MODULES: no directory $repo/$dir"
		name=$(basename "$dir")
		ko=$(sed -n 's/^obj-m *:\?= *\([A-Za-z0-9_-]*\)\.o.*/\1/p' \
			"$repo/$dir/Makefile" "$repo/$dir/Kbuild" 2>/dev/null | head -1 || true)
		[ -n "$ko" ] || die "$dir: no 'obj-m := <name>.o' in its Makefile or Kbuild"

		# tracked and untracked files, but not build output (.gitignore)
		mkdir -p "$tree/pkvmi/$name"
		(cd "$repo" && git ls-files -z --cached --others --exclude-standard -- "$dir") |
			tar -C "$repo" --null -T - -cf - --transform "s|^$dir/|pkvmi/$name/|" |
			tar -C "$tree" -xf -
		cat >"$tree/pkvmi/$name/BUILD.bazel" <<-EOF
			load("//build/kernel/kleaf:kernel.bzl", "kernel_module")

			kernel_module(
			    name = "$name",
			    srcs = glob(["**"], exclude = ["BUILD.bazel"]),
			    outs = ["$ko.ko"],
			    kernel_build = "//private/devices/google/$codename:kernel",
			    visibility = ["//visibility:public"],
			)
		EOF

		# built and installed with the device's modules ...
		sed -i "s|^\( *\)\"//private/google-modules/amplifiers/cs35l41\",|\1\"//pkvmi/$name:$name\",  # pkvmi\n&|" \
			"$pkg/BUILD.bazel"
		grep -q "//pkvmi/$name:$name.*# pkvmi\$" "$pkg/BUILD.bazel" ||
			die "could not add $name to $pkg/BUILD.bazel"
		# ... and put in the first-stage ramdisk (vendor_kernel_boot.img)
		echo "extra/pkvmi/$name/$ko.ko" >>"$ramdisk_list"

		protected_modules+=",${ko//-/_}"
		echo "module : $dir -> $ko.ko (EL2 early module)"
	done

	if [ -n "$protected_modules" ]; then
		# The kernel keeps the LAST kvm-arm.protected_modules on its command
		# line, and the device tree's bootargs come after CONFIG_CMDLINE, so a
		# built-in copy is overridden (seen on the phone). Extend the list here.
		sed -i "s|\(kvm-arm.protected_modules=[^\" ]*\)|\1$protected_modules|" "$dtsi"
		echo "dtsi   : $(grep -om1 'kvm-arm.protected_modules=[^" ]*' "$dtsi") (${dtsi#$tree/})"
	fi
fi
echo "config : $config"

cd "$tree"
exec "./build_$codename.sh" --config=use_source_tree_aosp "${dev_flags[@]}" "$@" -- \
	--dist_dir="out/$codename/dist"
