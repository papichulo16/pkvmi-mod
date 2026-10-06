#!/usr/bin/env bash
# Build src/, then boot it in QEMU with pkvmi loaded as an early EL2 module.
# You get a shell; run /userspace by hand. Quit with Ctrl-A x.
# Extra environment (GDB=1, EXTRA=..., KOUT=...) is passed to run-qemu.sh.
set -euo pipefail

g_here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

"$g_here/src-build.sh"
PKVM_MODULES=pkvmi exec "$g_here/run-qemu.sh"
