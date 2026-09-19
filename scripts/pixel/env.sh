# Source this: puts repo, adb and fastboot (in ignore/pixel/tools) on PATH.
#   . scripts/pixel/env.sh
_here=$(cd "$(dirname "${BASH_SOURCE[0]:-${(%):-%x}}")" && pwd)
export PIXEL_WORK="$(cd "$_here/../.." && pwd)/ignore/pixel"
case ":$PATH:" in
	*":$PIXEL_WORK/tools:"*) ;;
	*) export PATH="$PIXEL_WORK/tools:$PIXEL_WORK/tools/platform-tools:$PATH" ;;
esac
unset _here
