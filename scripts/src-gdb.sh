#!/usr/bin/env bash
# Attach gdb to a QEMU started with GDB=1 and break inside the EL2 module.
#
#   terminal 1: GDB=1 scripts/src-run.sh        (QEMU freezes at reset)
#   terminal 2: scripts/src-gdb.sh              (then `c`, run /userspace in 1)
#
# The kernel passes the hyp VA of the registered hvc handler to
# __pkvm_register_el2_call() before any hvc can reach it. gdb stops there,
# loads src/hyp/kvm_nvhe.o with .hyp.text rebased to the module's hyp VA, and
# sets hardware breakpoints on the EL2 functions in $HYP_FUNCS.
#
# Env: HYP_FUNCS  EL2 symbols to break on (default: the hvc handler)
#      HVC_FN     the one handed to pkvm_register_el2_mod_call (default as above)
#      GDB        gdb binary (default: gdb)
#      KOUT       kernel build dir
set -euo pipefail

g_here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
g_kout=${KOUT:-$g_here/../ignore/out-qemu-6.1}
g_obj=$(realpath "$g_here/../src/hyp/kvm_nvhe.o")
g_hvc=${HVC_FN:-pkvm_hello_hvc}
g_funcs=${HYP_FUNCS:-$g_hvc}
g_gdb=${GDB:-gdb}
g_script=$(mktemp)
trap 'rm -f "$g_script"' EXIT

[[ -f $g_obj ]] || { echo "src-gdb: $g_obj missing, run src-build.sh" >&2; exit 1; }

# offset of $1 inside the module's .hyp.text (the object is linked at 0)
hyp_offset() {
	llvm-nm "$g_obj" | awk -v s="__kvm_nvhe_$1" '$3 == s { print "0x" $1 }'
}

g_off=$(hyp_offset "$g_hvc")
[[ -n $g_off ]] || { echo "src-gdb: no hyp symbol $g_hvc" >&2; exit 1; }

{
	echo "set pagination off"
	echo "set confirm off"
	echo "file $g_kout/vmlinux"
	echo "target remote :1234"
	echo "hbreak __pkvm_register_el2_call"
	echo "commands"
	echo "  silent"
	echo "  set \$hyp_text = \$x0 - $g_off"
	echo "  printf \"pkvmi: EL2 module text at 0x%lx\\n\", \$hyp_text"
	# by address: needs no symbols, so a symbol-loading hiccup cannot lose them
	for fn in $g_funcs; do
		echo "  hbreak *(\$hyp_text + $(hyp_offset "$fn"))"
	done
	echo "  add-symbol-file $g_obj -s .hyp.text \$hyp_text"
	echo "  continue"
	echo "end"
} >"$g_script"

echo "src-gdb: breaking on EL2: $g_funcs (type c to boot)" >&2
# -nx: pwndbg hangs on the add-symbol-file below; plain gdb is fine
"$g_gdb" -nx -q -x "$g_script" "${@}"
