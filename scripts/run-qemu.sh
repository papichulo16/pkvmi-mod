#!/bin/sh
# Boot a pKVM (protected nVHE) kernel under QEMU TCG.
#
#   run-qemu.sh                 boot, hyp modules from $PKVM_MODULES
#   GDB=1 run-qemu.sh           freeze at reset and listen for gdb on :1234
#   NVHE=1 run-qemu.sh          plain nVHE instead of hVHE (see below)
#   EXTRA="foo=1" run-qemu.sh   append to the kernel command line
#
# In this tree kvm-arm.mode=protected implies hVHE when the CPU has VHE (which
# -cpu max does). NVHE=1 turns VHE off so pKVM runs as classic nVHE.
#
# Quit QEMU with Ctrl-A x.
HERE=$(cd "$(dirname "$0")" && pwd)
KOUT=${KOUT:-$HERE/../ignore/out-qemu}
INITRD=${INITRD:-$HERE/../ignore/initramfs.cpio.gz}
PKVM_MODULES=${PKVM_MODULES-pkvm_smc}
SMP=${SMP:-4}
MEM=${MEM:-2G}

[ -f "$KOUT/arch/arm64/boot/Image" ] ||
	{ echo "run-qemu: no kernel at $KOUT, build it first (SETUP.md)" >&2; exit 1; }
[ -f "$INITRD" ] ||
	{ echo "run-qemu: no initramfs at $INITRD, run scripts/dev.sh first" >&2; exit 1; }

# nokaslr: so vmlinux symbols match the running kernel in gdb.
CMDLINE="console=ttyAMA0 earlycon nokaslr loglevel=8 rdinit=/init"
CMDLINE="$CMDLINE kvm-arm.mode=protected kvm-arm.protected_modules=$PKVM_MODULES"
[ -n "$NVHE" ] && CMDLINE="$CMDLINE arm64_sw.hvhe=0 id_aa64mmfr1.vh=0"
CMDLINE="$CMDLINE $EXTRA"

[ -n "$GDB" ] && GDBFLAGS="-s -S"

# virtualization=on : boot the kernel at EL2 (required for KVM/pKVM)
# gic-version=3     : pKVM only supports GICv3
# -cpu max          : all the v8.x features the kernel probes for
exec qemu-system-aarch64 \
	-M virt,virtualization=on,gic-version=3 \
	-cpu max -smp "$SMP" -m "$MEM" \
	-accel tcg,thread=multi \
	-nographic -no-reboot \
	-kernel "$KOUT/arch/arm64/boot/Image" \
	-initrd "$INITRD" \
	-append "$CMDLINE" \
	$GDBFLAGS
