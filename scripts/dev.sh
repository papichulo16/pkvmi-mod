#!/usr/bin/env bash
# Compile a module dir and pack it into $KOUT/initramfs.cpio.gz.
# Then run ./run-qemu.sh yourself.
set -euo pipefail

g_here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
g_kout=${KOUT:-$g_here/../ignore/out-qemu-6.1}
g_dir=$PWD
g_bins=()
g_mods=()

usage() {
	cat <<'EOF'
usage: dev.sh [-C DIR] [-b FILE]... [-m FILE]... [-k DIR]

  -C DIR   directory to run `make` in (default: current dir)
  -b FILE  userspace binary, placed in the guest at /<name> (repeatable)
  -m FILE  module .ko loaded at boot (repeatable; default: every .ko in DIR)
  -k DIR   kernel build dir (default: $KOUT, else ../ignore/out-qemu-6.1)

Relative -b/-m paths are looked up in DIR first, then the current dir.
Afterwards run ./run-qemu.sh; the modules are already loaded at the shell.
EOF
}

die() {
	echo "dev.sh: $*" >&2
	exit 1
}

dev_parse_args() {
	while [[ $# -gt 0 ]]; do
		case $1 in
		-h|--help) usage; exit 0 ;;
		-C|-b|-m|-k) [[ $# -ge 2 ]] || die "$1 needs a value" ;;
		*) die "unknown option: $1 (-h for help)" ;;
		esac

		case $1 in
		-C) g_dir=$2 ;;
		-b) g_bins+=("$2") ;;
		-m) g_mods+=("$2") ;;
		-k) g_kout=$2 ;;
		esac
		shift 2
	done

	[[ -d $g_dir ]] || die "no such directory: $g_dir"
	[[ -d $g_kout ]] || die "no such kernel build dir: $g_kout"
	g_dir=$(realpath "$g_dir")
	g_kout=$(realpath "$g_kout")
}

dev_resolve() {
	local file=$1

	if [[ $file = /* ]]; then
		echo "$file"
	elif [[ -e $g_dir/$file ]]; then
		echo "$g_dir/$file"
	else
		echo "$PWD/$file"
	fi
}

dev_collect_files() {
	local i

	if [[ ${#g_mods[@]} -eq 0 ]]; then
		mapfile -t g_mods < <(find "$g_dir" -maxdepth 1 -name '*.ko' | sort)
	fi

	for i in "${!g_mods[@]}"; do
		g_mods[i]=$(dev_resolve "${g_mods[i]}")
		[[ -f ${g_mods[i]} ]] || die "module not found: ${g_mods[i]}"
	done

	for i in "${!g_bins[@]}"; do
		g_bins[i]=$(dev_resolve "${g_bins[i]}")
		[[ -f ${g_bins[i]} ]] || die "binary not found: ${g_bins[i]}"
	done
}

dev_guest_script() {
	local mod name

	for mod in "${g_mods[@]}"; do
		name=$(basename "$mod" .ko)
		echo "modprobe $name || echo 'dev.sh: modprobe $name failed'"
	done
}

# modules must be built with the compiler the kernel was built with; SETUP.md
# records it in <kernel dir>/llvm. Exporting LLVM overrides it.
dev_llvm() {
	if [[ -n ${LLVM:-} ]]; then
		echo "$LLVM"
	elif [[ -f $g_kout/llvm ]]; then
		cat "$g_kout/llvm"
	else
		echo 1
	fi
}

dev_pack_initramfs() {
	local args=(-k "$g_kout" -o "$g_kout/initramfs.cpio.gz")
	local early=()
	local bin

	for bin in "${g_bins[@]}"; do
		args+=(-x "$bin")
	done

	args+=(--run "$(dev_guest_script)")

	# pkvm_smc is what run-qemu.sh loads early by default, but only 6.12 has it
	if [[ -f $g_kout/drivers/misc/pkvm-smc/pkvm_smc.ko ]]; then
		early=(drivers/misc/pkvm-smc/pkvm_smc.ko)
	fi

	"$g_here/mkinitramfs.py" "${args[@]}" "${early[@]}" "${g_mods[@]}"
}

main() {
	dev_parse_args "$@"
	make -C "$g_dir" KDIR="$g_kout" LLVM="$(dev_llvm)"
	dev_collect_files
	dev_pack_initramfs
	echo "dev.sh: ready. run: KOUT=$g_kout $g_here/run-qemu.sh"
}

main "$@"
