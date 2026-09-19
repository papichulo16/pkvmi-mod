#!/usr/bin/env bash
# Fetch the kernel source tree (a `repo` workspace) for a Pixel device.
#
#   setup-device.sh --list                 show known devices
#   setup-device.sh pixel7                 default branch from devices.conf
#   setup-device.sh pixel7 <branch>        explicit kernel/manifest branch
#   JOBS=16 setup-device.sh pixel7         parallel fetches (default 8)
#   FULL_HISTORY=1 setup-device.sh pixel7  don't shallow-clone the prebuilts
#
# The tree lands in ignore/pixel/kernels/<codename>-<release>/. Phones
# with the same codename share one tree. Re-running resumes/updates it.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. "$HERE/env.sh"

. "$HERE/lib.sh"
JOBS=${JOBS:-8}

if [ "${1:-}" = "--list" ]; then
	devices | awk -F'\t' '{ printf "%-14s android-gs-%s-%s\n%-14s   %s\n", $1, $2, $3, "", $4 }'
	exit 0
fi

[ $# -ge 1 ] || die "usage: $0 <device> [branch]   (see --list)"
resolve_device "$1" "${2:-}"
dir=$tree

command -v repo >/dev/null || die "repo not found; expected in $PIXEL_WORK/tools (see SETUP.md)"
echo "device : $device ($phones)"
echo "branch : $branch"
echo "tree   : $dir"

git ls-remote --exit-code --heads "$MANIFEST_URL" "refs/heads/$branch" >/dev/null 2>&1 ||
	die "no branch '$branch' on $MANIFEST_URL"

mkdir -p "$dir"
cd "$dir"

if [ ! -d .repo ]; then
	repo init -u "$MANIFEST_URL" -b "$branch" --no-tags </dev/null
fi

# The prebuilts (clang, gcc, jdk, ndk, bazel, ...) have enormous histories and
# only their checked-out files matter. Shallow-clone them; keep full history
# for the kernel and driver repos, which is what you actually work in.
if [ -z "${FULL_HISTORY:-}" ]; then
	mkdir -p .repo/local_manifests
	python3 - .repo/manifests/default.xml .repo/local_manifests/shallow-prebuilts.xml <<'EOF'
import sys
import xml.etree.ElementTree as ET

src, dst = sys.argv[1:]
out = ET.Element("manifest")
for p in ET.parse(src).getroot().iter("project"):
    path = p.get("path", p.get("name"))
    if not path.startswith(("prebuilts/", "toolchain/")):
        continue
    ET.SubElement(out, "remove-project", name=p.get("name"))
    q = ET.SubElement(out, "project", {**p.attrib, "clone-depth": "1"})
    q.extend(list(p))
ET.indent(out)
ET.ElementTree(out).write(dst, encoding="utf-8", xml_declaration=True)
EOF
fi

repo sync -c --no-tags -j"$JOBS"

echo
echo "done: $dir"
