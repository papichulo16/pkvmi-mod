#!/usr/bin/env bash
# Download the Alpine aarch64 minirootfs (busybox) into ignore/rootfs,
# verified against the sha256 Alpine publishes.
set -euo pipefail

g_work=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/ignore
g_base=https://dl-cdn.alpinelinux.org/alpine/latest-stable/releases/aarch64

die() {
	echo "fetch-rootfs: $*" >&2
	exit 1
}

# prints "<file> <sha256>" of the current minirootfs release
rootfs_latest() {
	curl -fsS "$g_base/latest-releases.yaml" | awk '
		/flavor: alpine-minirootfs/ { found = 1 }
		found && /file:/ { file = $2 }
		found && /sha256:/ { print file, $2; exit }'
}

main() {
	local file sha

	[[ ! -e $g_work/rootfs ]] || die "$g_work/rootfs exists, delete it to refetch"

	read -r file sha < <(rootfs_latest)
	[[ -n ${file:-} && -n ${sha:-} ]] || die "could not read the release list"

	mkdir -p "$g_work/rootfs"
	curl -fsS -o "$g_work/$file" "$g_base/$file"
	echo "$sha  $g_work/$file" | sha256sum -c - || die "checksum mismatch"

	# dev/ is left empty: mkinitramfs.py creates the nodes it needs
	tar -xzf "$g_work/$file" -C "$g_work/rootfs" --exclude='dev/*'
	rm -f "$g_work/$file"
	echo "fetch-rootfs: $file -> $g_work/rootfs"
}

main "$@"
