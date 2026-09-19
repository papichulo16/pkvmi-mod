# Shared helpers for the pixel scripts. Source this after env.sh; it expects
# $HERE to be scripts/pixel and $PIXEL_WORK to be set (ignore/pixel).

MANIFEST_URL=https://android.googlesource.com/kernel/manifest

die() { echo "error: $*" >&2; exit 1; }

# Emit "alias<TAB>codename<TAB>release<TAB>phones" per device in devices.conf.
devices() {
	awk -F'|' '!/^[[:space:]]*(#|$)/ {
		for (i = 1; i <= NF; i++) gsub(/^[ \t]+|[ \t]+$/, "", $i)
		print $1 "\t" $2 "\t" $3 "\t" $4
	}' "$HERE/devices.conf"
}

# resolve_device <alias> [branch]
# Sets: device codename release phones branch tree
resolve_device() {
	local row
	device=$1
	row=$(devices | awk -F'\t' -v d="$device" '$1 == d') || true
	[ -n "$row" ] || die "unknown device '$device' (see setup-device.sh --list)"
	IFS=$'\t' read -r _ codename release phones <<<"$row"
	branch=${2:-android-gs-$codename-$release}
	tree="$PIXEL_WORK/kernels/${branch#android-gs-}"
}
