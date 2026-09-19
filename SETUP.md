# Setup

Host: Arch Linux x86_64, `/dev/kvm`. Disk: each Pixel tree 24 GB, `ignore/out-qemu-6.1` 3 GB (`~/ack` 10 GB and `ignore/out-qemu` 3 GB if you also build 6.12).

Everything below runs from the repo root.

## Layout

```
pkvmi-mod/
  SETUP.md
  configs/                  qemu-6.1.fragment  qemu.fragment (6.12)
  scripts/                  dev.sh  run-qemu.sh  mkinitramfs.py  fetch-rootfs.sh  pixel/
  tests/hello_ioctl/        EL1 module + userspace test
  ignore/                   NOT in git. Created by the steps below:
    rootfs/                   Alpine rootfs (step 2)
    pixel/{tools,kernels}/    Pixel tools and kernel trees (step 3)
    out-qemu-6.1/             6.1 QEMU kernel build (step 4); holds its own initramfs.cpio.gz
    out-qemu/                 6.12 QEMU kernel build (step 7, optional)
```

## Two kernels

The scripts boot and build for **6.1 by default**. To use 6.12, set `KOUT=$PWD/ignore/out-qemu` for every script. `run-qemu.sh` prints which kernel it is booting. Each kernel keeps its own `initramfs.cpio.gz` inside its build dir, so modules built for one can never boot on the other.

| | 6.1 (what the Pixel 7 runs) | 6.12 (optional) |
|---|---|---|
| source | Pixel tree `aosp/` | `~/ack` |
| compiler | the tree's Android clang 17 | system clang |
| config | `configs/qemu-6.1.fragment` | `configs/qemu.fragment` |
| build dir | `ignore/out-qemu-6.1` (default) | `ignore/out-qemu` (`KOUT=...`) |
| pKVM mode | nVHE | hVHE (`NVHE=1` for nVHE) |
| pKVM debug option | `NVHE_EL2_DEBUG` | `PKVM_DEBUG` |

Build modules for the phone against 6.1. The `pkvm_module_ops` API differs between the two.

## 1. Host packages

```sh
sudo pacman -S qemu-system-aarch64 clang lld llvm pahole bc flex bison dtc cpio curl python git kmod
```

## 2. Rootfs

```sh
scripts/fetch-rootfs.sh     # Alpine aarch64 minirootfs -> ignore/rootfs, sha256-checked
```

## 3. Pixel tree (kernel source and compiler for 6.1)

```sh
mkdir -p ignore/pixel/tools && cd ignore/pixel/tools
curl -Lo repo https://storage.googleapis.com/git-repo-downloads/repo && chmod +x repo
curl -Lo pt.zip https://dl.google.com/android/repository/platform-tools-latest-linux.zip && unzip -q pt.zip && rm pt.zip
cd ../../..

. scripts/pixel/env.sh                       # repo, adb, fastboot on PATH
scripts/pixel/setup-device.sh --list
scripts/pixel/setup-device.sh pixel7         # ~20 GB, ~10 min -> ignore/pixel/kernels/pantah-6.1-android16
```

Other phones: `setup-device.sh pixel8` etc.; add one in `scripts/pixel/devices.conf`. As of 2026-09 the newest production branch for every listed phone is `-6.1-android16`.

## 4. Build the 6.1 kernel for QEMU (~8 min, 12 cores)

Same source (`aosp/`) and compiler as the phone kernel, on a generic `virt` machine. `configs/qemu-6.1.fragment` adds virtio and pl011 to `gki_defconfig` and turns off KASAN, CFI and BTF, which only slow a TCG dev build down. The built-in command line already has `kvm-arm.mode=protected`, same as the phone.

```sh
T=$PWD/ignore/pixel/kernels/pantah-6.1-android16
export ARCH=arm64 LLVM=$T/prebuilts/clang/host/linux-x86/clang-r487747c/bin/
O=$PWD/ignore/out-qemu-6.1 FRAG=$PWD/configs/qemu-6.1.fragment
cd $T/aosp
make O=$O gki_defconfig
scripts/kconfig/merge_config.sh -m -O $O $O/.config $FRAG
make O=$O olddefconfig
make O=$O -j$(nproc) Image modules
echo "$LLVM" > $O/llvm       # dev.sh reads this so modules use the kernel's compiler
cd -
```

Check the options stuck: `grep NVHE_EL2_DEBUG ignore/out-qemu-6.1/.config`.

## 5. Build a test and boot it

```sh
cd tests/hello_ioctl
../../scripts/dev.sh -b hello_test       # compile with the kernel's compiler + pack initramfs
../../scripts/run-qemu.sh                # boot (prints the kernel it boots); hello.ko is already loaded
# in the guest:
/hello_test                              # kernel says: Hello from EL1!  PASS
```

`dev.sh`: `-C DIR` make dir (its Makefile must honor `KDIR` and `LLVM`, like `tests/hello_ioctl/Makefile`), `-b FILE` userspace binary put at `/<name>`, `-m FILE` module (default: every `.ko` in DIR), `-k DIR` kernel dir (default `$KOUT`, else 6.1). The module compiler is `$LLVM` if you export it, else the one recorded in `<kernel dir>/llvm`, else the system LLVM.

`run-qemu.sh` (Ctrl-A x quits):

```sh
GDB=1 scripts/run-qemu.sh                   # halt at reset, gdb on :1234
PKVM_MODULES=a,b scripts/run-qemu.sh        # early EL2 modules (kernel loads them before pKVM finalizes)
EXTRA="foo=1" scripts/run-qemu.sh           # extra kernel command line
NVHE=1 scripts/run-qemu.sh                  # 6.12 only: nVHE instead of hVHE
```

Boot succeeded if you see `Protected nVHE mode initialized successfully` (6.12: `hVHE`).
On 6.1 an early module also logs `loading <name> from /lib/modules/ failed, fallback to the default path`. That is expected: `CONFIG_PKVM_MODULE_PATH` is `/lib/modules/` there, as on the phone, and the fallback then loads it. I have not checked how the phone's ramdisk lays modules out.

QEMU: `-M virt,virtualization=on,gic-version=3 -cpu max` (TCG). It is not a Pixel: no Tensor SoC, no S2MPU or IOMMU, no bootloader or TrustZone.

## 6. Writing modules

### EL1

Ordinary out-of-tree module. Copy `tests/hello_ioctl/` and change it.

### EL2 (hyp)

Out-of-tree works on 6.1 and 6.12 (verified with a do-nothing module on both). The layout follows Google's `pkvm-s2mpu`:

```
mymod/
  Makefile      make -C $(KDIR) M=$(CURDIR) ARCH=arm64 LLVM=$(LLVM) modules   (KDIR ?= ignore/out-qemu-6.1)
  Kbuild
  mymod.c       EL1: pkvm_load_el2_module(__kvm_nvhe_<init>, &token)
  hyp/Kbuild
  hyp/hyp.c     EL2: int <init>(const struct pkvm_module_ops *ops)
```

`Kbuild`:

```make
subdir-ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include/
obj-m += mymod.o
$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o
clean-files := hyp/hyp.lds hyp/hyp-reloc.S
mymod-y := mymod.o hyp/kvm_nvhe.o
```

`hyp/Kbuild`:

```make
vpath %.S $(srctree)
hyp-obj-y := hyp.o
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module
```

The `vpath` line is needed on 6.12 (its external-module Kbuild cannot find `Makefile.module`'s source-relative `module.lds.S`) and harmless on 6.1.

EL1 declares the EL2 entry with the prefix: `int __kvm_nvhe_<init>(const struct pkvm_module_ops *ops);`.

Run it: `dev.sh -C mymod`, then `PKVM_MODULES=mymod scripts/run-qemu.sh`. Expect `[permanent]` in `/proc/modules`. `dev.sh` also `modprobe`s it at `/init`, which is a harmless no-op once it is loaded.

6.1 has no in-tree example; use Google's `pkvm-s2mpu` (`private/google-modules/soc/gs/drivers/soc/google/pkvm-s2mpu`). 6.12 has `~/ack/drivers/misc/pkvm-smc` (an SMC filter).

### EL2 rules

- EL2 code loads only at boot, before pKVM finalizes (`-EOPNOTSUPP` afterwards, and the hypervisor rejects `__pkvm_init_module`). No later load, no unload. So the module that loads and registers must itself be an early module.
- After boot, drive it through registered hypercalls (`pkvm_register_el2_mod_call` / `pkvm_el2_mod_call`) or SMC/trap handlers.
- **An EL2 init that returns non-zero kills the host on 6.1.** The loader unmaps the module and the hypervisor panics (`nVHE hyp BUG at arch/arm64/kvm/hyp/nvhe/mm.c:211`, then `Kernel panic - HYP panic`). Verified on the QEMU build (`NVHE_EL2_DEBUG=y`); not checked with the phone's config. On a phone that is a boot loop for an early module, so test in QEMU first.
- Hypercall handler signatures differ: 6.1 `void h(struct kvm_cpu_context *)`, 6.12 `void h(struct user_pt_regs *)`. On 6.1 the dispatcher sets `x0 = SMCCC_RET_SUCCESS` before your handler; on 6.12 your handler must. Setting it yourself is safe on both.
- 6.12's `pkvm_module_ops` has 66 callbacks, 6.1's has 33. Only use callbacks both have if you target the phone.
- Any EL2 change means rebuild, repack, reboot.
- Hyp code runs at hyp VAs, not the `vmlinux` addresses, so plain `break __kvm_nvhe_*` will not hit.

## 7. 6.12 kernel (optional)

```sh
git clone https://android.googlesource.com/kernel/common ~/ack
cd ~/ack && git checkout android16-6.12

export ARCH=arm64 LLVM=1
O=$OLDPWD/ignore/out-qemu FRAG=$OLDPWD/configs/qemu.fragment
make O=$O gki_defconfig
scripts/kconfig/merge_config.sh -m -O $O $O/.config $FRAG
make O=$O olddefconfig
make O=$O -j$(nproc) Image modules
cd -
```

`ignore/out-qemu` records the source path, so do not move `~/ack` afterwards. Then run step 5 with `KOUT=$PWD/ignore/out-qemu` set for every script (there is no `llvm` file for this build, so modules use the system LLVM). `dev.sh` also packs `pkvm_smc` as an early module, and a boot logs `pKVM SMC filter registered successfully`.

## 8. Phone images (Pixel)

```sh
scripts/pixel/build-device.sh pixel7 --lto=none    # ~10 min -> ignore/pixel/kernels/.../out/pantah/dist/*.img
```

- Always use `build-device.sh`. The tree's own `build_*.sh` defaults to Google's prebuilt kernel and ignores edits under `aosp/`. To check a build is yours: `strings -a .../dist/Image | grep -m1 '^Linux version'` ends in `g<aosp HEAD sha>`, not `-ab<number>`.
- Phone template module: Google's `pkvm-s2mpu` (`gs201/BUILD.bazel:223`). Boot args `kvm-arm.protected_modules=exynos-pd,pkvm_s2mpu` are in `gs201.dtsi`.
- The phone's installed Android build must match the branch's Android release; flash the matching factory image first (flash.android.com).
- Not done yet: a Kleaf rule for your own module, the ramdisk list, the boot args, flashing. Keep the factory image: a bad early hyp module can stop the boot.
