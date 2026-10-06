#!/usr/bin/env bash
# Non-interactive test: build src/, boot QEMU, run /userspace, power off.
# Exit 0 only if the guest printed PASS. Full guest log: ignore/src-test.log
# TIMEOUT=<seconds> (default 300) bounds the boot; TCG is slow.
set -uo pipefail

g_here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
g_log=$g_here/../ignore/src-test.log
g_timeout=${TIMEOUT:-300}

"$g_here/src-build.sh" -r '/userspace; echo "userspace exit=$?"' -r 'poweroff -f' ||
	exit 2

PKVM_MODULES=pkvmi timeout "$g_timeout" "$g_here/run-qemu.sh" </dev/null >"$g_log" 2>&1
g_qemu=$?

grep -a -E 'pkvm|hypervisor says|PASS|FAIL|userspace exit|panic|BUG' "$g_log" | tail -n 20

if grep -aq '^PASS' "$g_log"; then
	echo "src-test: PASS"
	exit 0
fi

echo "src-test: FAIL (qemu exit $g_qemu, 124 = timeout), see $g_log" >&2
exit 1
