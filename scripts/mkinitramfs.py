#!/usr/bin/env python3
"""Build a minimal aarch64 initramfs for pKVM module development.

Alpine minirootfs (busybox, musl) + the pKVM modules you name. The kernel
runs `modprobe -q -- <name>` out of this initramfs before it deprivileges the
host, so the modules only need to be in /lib/modules/<release>/ with a valid
modules.dep.

  mkinitramfs.py [-k KOUT] [-o OUT] [-x FILE[:DEST]] [--run CMD]
                 MODULE.ko [MODULE.ko ...]

MODULE.ko paths are relative to KOUT (e.g. drivers/misc/pkvm-smc/pkvm_smc.ko)
or absolute.
"""
import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALPINE = ROOT / "ignore/rootfs"

INIT = """#!/bin/sh
mount -t devtmpfs devtmpfs /dev 2>/dev/null
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
mount -t debugfs debugfs /sys/kernel/debug 2>/dev/null

echo
echo "=== $(uname -r) ==="
echo "--- pKVM / hyp messages ---"
dmesg | grep -i -E 'kvm|pkvm|hyp'
echo "---------------------------"
echo "loaded modules:"; cat /proc/modules
echo
@RUN@
exec setsid sh -c 'exec sh </dev/ttyAMA0 >/dev/ttyAMA0 2>&1'
"""

# (path, mode, kind, major, minor) -- created by gen_init_cpio, no root needed.
DEV_NODES = [
    ("/dev/console", 0o600, 5, 1),
    ("/dev/null", 0o666, 1, 3),
    ("/dev/kmsg", 0o600, 1, 11),   # pkvm early modprobe opens this
    ("/dev/ttyAMA0", 0o600, 204, 64),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-k", "--kout", default=str(ROOT / "ignore/out-qemu"))
    ap.add_argument("-o", "--out",
                    default=str(ROOT / "ignore/initramfs.cpio.gz"))
    ap.add_argument("-x", "--extra", action="append", default=[],
                    metavar="SRC[:DEST]",
                    help="copy a file in, executable; DEST defaults to /<name>")
    ap.add_argument("--run", default="", metavar="CMD",
                    help="shell commands to run in /init before the shell")
    ap.add_argument("modules", nargs="*")
    args = ap.parse_args()

    if not ALPINE.is_dir():
        sys.exit(f"{ALPINE} missing: run scripts/fetch-rootfs.sh")

    kout = Path(args.kout)
    release = (kout / "include/config/kernel.release").read_text().strip()
    gen_cpio = kout / "usr/gen_init_cpio"
    if not gen_cpio.exists():
        sys.exit(f"{gen_cpio} missing: build the kernel first")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "root"
        shutil.copytree(ALPINE, root, symlinks=True)

        # CONFIG_MODPROBE_PATH is /system/bin/modprobe in the GKI config.
        (root / "system/bin").mkdir(parents=True, exist_ok=True)
        (root / "system/bin/modprobe").symlink_to("/bin/busybox")

        moddir = root / "lib/modules" / release
        moddir.mkdir(parents=True, exist_ok=True)
        for m in args.modules:
            src = Path(m) if os.path.isabs(m) else kout / m
            if not src.exists():
                sys.exit(f"module not found: {src}")
            shutil.copy(src, moddir / src.name)
        # empty on purpose: depmod only needs the .ko files but warns without them
        for name in ("modules.order", "modules.builtin",
                     "modules.builtin.modinfo"):
            (moddir / name).touch()
        subprocess.run(["depmod", "-b", str(root), release], check=True)

        for spec in args.extra:
            src, _, dest = spec.partition(":")
            if not Path(src).is_file():
                sys.exit(f"extra file not found: {src}")
            target = root / (dest or "/" + Path(src).name).lstrip("/")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(src, target)
            target.chmod(0o755)

        init = root / "init"
        init.write_text(INIT.replace("@RUN@", args.run))
        init.chmod(0o755)

        lines = []
        for path in sorted(root.rglob("*")):
            rel = "/" + str(path.relative_to(root))
            st = path.lstat()
            mode = stat.S_IMODE(st.st_mode)
            if path.is_symlink():
                lines.append(f"slink {rel} {os.readlink(path)} {mode:o} 0 0")
            elif path.is_dir():
                lines.append(f"dir {rel} {mode:o} 0 0")
            elif path.is_file():
                lines.append(f"file {rel} {path} {mode:o} 0 0")
        for p, mode, maj, mnr in DEV_NODES:
            lines.append(f"nod {p} {mode:o} 0 0 c {maj} {mnr}")

        listfile = Path(tmp) / "list"
        listfile.write_text("\n".join(lines) + "\n")

        cpio = subprocess.run([str(gen_cpio), str(listfile)], check=True,
                              stdout=subprocess.PIPE).stdout
        gz = subprocess.run(["gzip", "-9"], input=cpio, check=True,
                            stdout=subprocess.PIPE).stdout
        Path(args.out).write_bytes(gz)

    print(f"{args.out}: {len(gz) >> 10} KiB, release {release}, "
          f"modules: {[Path(m).name for m in args.modules]}")


if __name__ == "__main__":
    main()
