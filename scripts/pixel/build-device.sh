#!/usr/bin/env bash
# Build a Pixel's kernel, modules and flashable images FROM THE SOURCE IN THE
# TREE (aosp/), with the tree's own hermetic toolchain.
#
#   build-device.sh pixel7 [bazel options...]
#   build-device.sh pixel7 --lto=none        faster dev build (no LTO/CFI)
#   BRANCH=<manifest branch> build-device.sh pixel7
#
# Output: ignore/pixel/kernels/<tree>/out/<codename>/dist   (boot.img, vendor_dlkm.img, ...)
#
# Why not just run the tree's build_<codename>.sh? Its default is
# --use_prebuilt_gki=true: it downloads Google's prebuilt GKI kernel and only
# builds the vendor modules, so edits to aosp/ (including the pKVM hypervisor)
# would silently not make it into the images. --config=use_source_tree_aosp
# turns that off.
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

cd "$tree"
exec "./build_$codename.sh" --config=use_source_tree_aosp "$@" -- \
	--dist_dir="out/$codename/dist"
