#!/usr/bin/env bash
# Build src/ (pkvmi.ko + userspace) and pack it into the 6.1 QEMU initramfs.
# Extra arguments go to dev.sh, e.g. `-r /userspace` to run it at boot.
set -euo pipefail

g_here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

exec "$g_here/dev.sh" -C "$g_here/../src" -b userspace "$@"
