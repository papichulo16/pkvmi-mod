#!/usr/bin/env bash
# Build src/ (pkvmi.ko as an early EL2 module + userspace) into flashable
# Pixel images, copy them to ignore/pixel/images/<name>/ and print the flash
# commands. It never touches the phone.
#
#   build-src.sh                     pixel7, image dir src-<git hash>
#   build-src.sh pixel7 mytest       image dir ignore/pixel/images/mytest
#   build-src.sh pixel7 mytest --lto=none    extra args go to build-device.sh
#
# Defaults are the config last verified on the phone (F-dtb-hello):
# PKVM_TRIM=1 PKVM_DEBUG=0 --lto=thin. Override them in the environment, and
# PKVM_MODULES="src tests/hello_hvc" to pack more modules alongside src.
set -euo pipefail

g_here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
g_repo=$(cd "$g_here/../.." && pwd)
HERE=$g_here  # lib.sh reads it
. "$g_here/env.sh"
. "$g_here/lib.sh"

g_device=${1:-pixel7}
g_name=${2:-src-$(git -C "$g_repo" rev-parse --short HEAD)$(git -C "$g_repo" diff --quiet -- src || echo -dirty)}
shift $(($# < 2 ? $# : 2))
g_extra=("$@")
[ ${#g_extra[@]} -gt 0 ] || g_extra=(--lto=thin)

resolve_device "$g_device" "${BRANCH:-}"
g_dist="$tree/out/$codename/dist"
g_out="$PIXEL_WORK/images/$g_name"
g_llvm="$tree/prebuilts/clang/host/linux-x86/clang-r487747c/bin"
g_images=(boot dtbo vendor_kernel_boot system_dlkm vendor_dlkm)

build_kernel() {
	PKVM_MODULES=${PKVM_MODULES-src} PKVM_TRIM=${PKVM_TRIM-1} PKVM_DEBUG=${PKVM_DEBUG-0} \
		"$g_here/build-device.sh" "$g_device" "${g_extra[@]}"
}

# Same flags as src/Makefile, but against the phone kernel's own uapi headers.
build_userspace() {
	local hdrs
	hdrs=$(mktemp -d)
	tar -C "$hdrs" -xzf "$g_dist/kernel-uapi-headers.tar.gz"
	"$g_llvm/clang" --target=aarch64-linux-gnu -O2 -static -nostdlib -ffreestanding \
		-fno-builtin -fuse-ld=lld -isystem "$hdrs/usr/include" \
		-I "$g_repo/src/intf" -o "$g_out/userspace" "$g_repo"/src/usr/*.c
	rm -rf "$hdrs"
}

check_dist() {
	grep -q '/pkvmi\.ko$' "$g_dist/vendor_kernel_boot.modules.load" ||
		die "pkvmi.ko is not in vendor_kernel_boot.modules.load; it would not load early"
	grep -aq 'kvm-arm.protected_modules=[^" ]*pkvmi' "$g_dist/dtb.img" ||
		die "pkvmi is not in the dtb's kvm-arm.protected_modules; it would load after pKVM finalizes"
}

copy_images() {
	local img
	mkdir -p "$g_out"
	for img in "${g_images[@]}"; do
		cp "$g_dist/$img.img" "$g_out/"
	done
}

print_flash() {
	cat <<-EOF

		images : $g_out
		flash it yourself (current slot):

		  . scripts/pixel/env.sh
		  cd $g_out
		  adb reboot bootloader
		  fastboot flash boot boot.img
		  fastboot flash dtbo dtbo.img
		  fastboot flash vendor_kernel_boot vendor_kernel_boot.img   # pkvmi.ko + dtb with protected_modules
		  fastboot reboot fastboot                                    # dlkm images live in super: need fastbootd
		  fastboot flash system_dlkm system_dlkm.img
		  fastboot flash vendor_dlkm vendor_dlkm.img
		  fastboot reboot

		then:  adb push $g_out/userspace /data/local/tmp/   (opening /dev/pkvmi needs root)
	EOF
}

[ ! -e "$g_out" ] || die "$g_out already exists; pick another name"
build_kernel
check_dist
copy_images
build_userspace
print_flash
