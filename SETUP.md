# Setup

Host: Arch Linux x86_64, `/dev/kvm`. Disk: `~/ack` 10 GB, `ignore/out-qemu` 3 GB, each Pixel tree 24 GB.

Everything below runs from the repo root.

## Layout

```
pkvmi-mod/
  SETUP.md
  configs/qemu.fragment     kernel config additions for the QEMU kernel
  scripts/                  dev.sh  run-qemu.sh  mkinitramfs.py  fetch-rootfs.sh  pixel/
  examples/hello_ioctl/     EL1 module + userspace test
  ignore/                   NOT in git. Created by the steps below:
    out-qemu/                 kernel build output (step 3)
    rootfs/                   Alpine rootfs (step 4)
    initramfs.cpio.gz         written by dev.sh
    pixel/{tools,kernels}/    Pixel tools and kernel trees (step 7)
```

The kernel source (`~/ack`) also lives outside the repo.

## 1. Host packages

```sh
sudo pacman -S qemu-system-aarch64 clang lld llvm pahole bc flex bison dtc cpio curl python git kmod
```

## 2. Kernel source

```sh
git clone https://android.googlesource.com/kernel/common ~/ack
cd ~/ack && git checkout android16-6.12
```

`ignore/out-qemu` records the source path (`Makefile`, `source` symlink), so do not move `~/ack` afterwards.

## 3. Build the kernel (~10 min, 12 cores)

Adds KVM/pKVM debug, virtio and pl011 to `gki_defconfig`, and turns off Rust, KASAN, CFI and BTF, which only slow a TCG dev build down.

```sh
export ARCH=arm64 LLVM=1
O=$PWD/ignore/out-qemu FRAG=$PWD/configs/qemu.fragment
cd ~/ack
make O=$O gki_defconfig
scripts/kconfig/merge_config.sh -m -O $O $O/.config $FRAG
make O=$O olddefconfig
make O=$O -j$(nproc) Image modules
cd -
```

Check the options stuck: `grep PKVM ignore/out-qemu/.config`.
6.12 uses `PKVM_DEBUG`; the 6.1 Pixel branch uses `NVHE_EL2_DEBUG`.

## 4. Rootfs

```sh
scripts/fetch-rootfs.sh     # Alpine aarch64 minirootfs -> ignore/rootfs, sha256-checked
```

## 5. Build an example and boot it

```sh
cd examples/hello_ioctl
../../scripts/dev.sh -b hello_test     # compile + pack ignore/initramfs.cpio.gz
../../scripts/run-qemu.sh              # boot; hello.ko is already loaded
# in the guest:
/hello_test                            # kernel says: Hello from EL1!  PASS
```

`dev.sh`: `-C DIR` make dir, `-b FILE` userspace binary put at `/<name>`, `-m FILE` module (default: every `.ko` in DIR), `-k DIR` kernel build dir.

`run-qemu.sh` (Ctrl-A x quits):

```sh
NVHE=1 scripts/run-qemu.sh                  # plain nVHE (default is hVHE)
GDB=1 scripts/run-qemu.sh                   # halt at reset, gdb on :1234
PKVM_MODULES=a,b scripts/run-qemu.sh        # early EL2 modules (default pkvm_smc)
EXTRA="foo=1" scripts/run-qemu.sh           # extra kernel command line
```

Boot succeeded if you see:

```
Protected hVHE mode initialized successfully
pKVM SMC filter registered successfully
```

QEMU: `-M virt,virtualization=on,gic-version=3 -cpu max` (TCG). Kernel cmdline: `kvm-arm.mode=protected kvm-arm.protected_modules=<name>`.

## 6. Writing modules

### EL1

Ordinary out-of-tree module. Copy `examples/hello_ioctl/` and change it.

### EL2 (hyp)

Must be in-tree. Template: `~/ack/drivers/misc/pkvm-smc/`

| Part | File |
|---|---|
| EL1 side | `pkvm-smc.c`: `pkvm_load_el2_module(kvm_nvhe_sym(<init>), &token)` |
| EL2 side | `pkvm/pkvm-smc.c`: `<init>(const struct pkvm_module_ops *ops)` |
| Build glue | `Makefile` (`pkvm/kvm_nvhe.o`), `pkvm/Makefile` (`hyp-obj-y`) |
| Kconfig | `drivers/misc/Kconfig` `PKVM_SMC_FILTER` (`tristate`, must be `=m`) |

Workflow (the template is verified; a new module of your own is not yet):

1. Copy the dir to `~/ack/drivers/misc/<name>/`, rename the symbols.
2. Add `obj-$(CONFIG_<X>) += <name>/` to `drivers/misc/Makefile` and a Kconfig entry.
3. Set `CONFIG_<X>=m` in `configs/qemu.fragment`, redo step 3.
4. Pack it as an early module: `scripts/mkinitramfs.py drivers/misc/<name>/<mod>.ko`, then `PKVM_MODULES=<mod> scripts/run-qemu.sh`.

Out-of-tree hyp modules do not work: the EL1 half builds, the hyp half fails with `No rule to make target arch/arm64/kvm/hyp/nvhe/module.lds.S`. `VPATH=`, running from `~/ack`, and a symlink with a relative `M=` all failed.

### EL2 rules

- EL2 code loads only at boot, before pKVM finalizes (`pkvm.c`: `-EOPNOTSUPP` once initialized, and the hypervisor rejects `__pkvm_init_module` after finalize). No later load, no unload.
- After boot, drive it through registered hypercalls (`__pkvm_register_hcall`) or SMC/trap handlers.
- Any EL2 change means rebuild, repack, reboot.
- Hyp code runs at hyp VAs, not the `vmlinux` addresses, so plain `break __kvm_nvhe_*` will not hit.

## 7. Pixel (optional, kernel 6.1)

Different kernel and pKVM API from `~/ack`. A module for the phone has to be built here.

```sh
mkdir -p ignore/pixel/tools && cd ignore/pixel/tools
curl -Lo repo https://storage.googleapis.com/git-repo-downloads/repo && chmod +x repo
curl -Lo pt.zip https://dl.google.com/android/repository/platform-tools-latest-linux.zip && unzip -q pt.zip && rm pt.zip
cd ../../..

. scripts/pixel/env.sh                       # repo, adb, fastboot on PATH
scripts/pixel/setup-device.sh --list
scripts/pixel/setup-device.sh pixel7         # ~20 GB, ~10 min -> ignore/pixel/kernels/pantah-6.1-android16
scripts/pixel/build-device.sh pixel7 --lto=none    # ~10 min -> .../out/pantah/dist/*.img
```

- Other phones: `setup-device.sh pixel8` etc. To add one, edit `scripts/pixel/devices.conf`. As of 2026-09 the newest production branch for every listed phone is `-6.1-android16`.
- Always use `build-device.sh`. The tree's own `build_*.sh` defaults to Google's prebuilt kernel and ignores edits under `aosp/`. To check a build is yours:
  `strings -a .../dist/Image | grep -m1 '^Linux version'` ends in `g<aosp HEAD sha>`, not `-ab<number>`.
- Phone template module: Google's `pkvm-s2mpu` (`gs201/BUILD.bazel:223`). Boot args `kvm-arm.protected_modules=exynos-pd,pkvm_s2mpu` are in `gs201.dtsi`.
- The phone's installed Android build must match the branch's Android release; flash the matching factory image first (flash.android.com).
- Not done yet: a Kleaf rule for your own module, the ramdisk list, the boot args, flashing. Keep the factory image: a bad early hyp module can stop the boot.
