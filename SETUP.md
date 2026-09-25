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

Run it: `dev.sh -C mymod`, then `PKVM_MODULES=mymod scripts/run-qemu.sh`. Expect the module's own log lines (for `tests/hello_hvc`: `pkvm_load_el2_module ret=0` and `hvc returned 67 (want 67)`). `[permanent]` in `/proc/modules` only shows for a module with no `module_exit`, so it is no proof of the EL2 load. `dev.sh` also `modprobe`s it at `/init`, which is a harmless no-op once it is loaded.

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
scripts/pixel/build-device.sh pixel7 --lto=thin    # ~10 min -> ignore/pixel/kernels/.../out/pantah/dist/*.img
```

By default this is a **pKVM dev build**: the stock Pixel config plus `configs/pixel-pkvm-dev_defconfig` (`NVHE_EL2_DEBUG`, `PROTECTED_NVHE_STACKTRACE`), with KMI trimming off, so the kernel exports every `EXPORT_SYMBOL` (~15k) instead of only the Pixel KMI list (~2.9k), and fips140 is built from source. `STOCK=1` builds the unmodified config. `PKVM_DEBUG=0` leaves out the two debug options. `PKVM_TRIM=1` keeps Google's KMI trimming and exports only the symbols in `configs/pixel-kmi-extra.symbols` on top of the Pixel list (it adds them to `aosp/android/abi_gki_aarch64_pixel`, which makes the kernel release end in `-dirty`); use it with a module that needs a symbol outside the Pixel KMI. `--lto=thin` keeps CFI like the shipped kernel; `--lto=none` builds faster but drops CFI; full LTO (the default) may run out of RAM on 30 GB.

Check the build is right:

```sh
D=ignore/pixel/kernels/pantah-6.1-android16/out/pantah/dist
strings -a $D/Image | grep -m1 '^Linux version'      # ends in g<aosp HEAD sha>
sh ignore/pixel/kernels/pantah-6.1-android16/aosp/scripts/extract-ikconfig $D/Image | grep -E 'NVHE_EL2_DEBUG|TRIM_UNUSED'
wc -l $D/vmlinux.symvers                              # ~15000, not ~3000
```

Flash (done on a Pixel 7, Android 15 userspace, unlocked bootloader; slot `a` was unbootable, so there was no fallback slot). All kernel partitions come from the same build, so the vendor modules match the kernel. The first time only, also disable verification, since the stock hashes do not match your `*_dlkm` images: `avbtool make_vbmeta_image --flags 2 --padding_size 4096 --output vbmeta_off.img` (avbtool is under `prebuilts/kernel-build-tools/linux-x86/bin/` in the tree) and `fastboot --disable-verity --disable-verification flash vbmeta vbmeta_off.img`. Use the project's `fastboot` (`. scripts/pixel/env.sh`): a distro `fastboot` failed on that command with `Failed to find AVB_MAGIC`. Copy `dist/*.img` somewhere first, since every build overwrites it and a bad flash needs the last good images back.

```sh
cd $D
fastboot flash boot boot.img
fastboot flash dtbo dtbo.img
fastboot flash vendor_kernel_boot vendor_kernel_boot.img   # dtb + first-stage modules
fastboot reboot fastboot                                    # fastbootd, for the dynamic partitions
fastboot flash vendor_dlkm vendor_dlkm.img
fastboot flash system_dlkm system_dlkm.img
fastboot reboot
```

Things that went wrong, and why:

- **Reboot loop before fastbootd.** `CONFIG_PANIC_TIMEOUT=-1`, so any panic reboots at once. A module in `vendor_kernel_boot.modules.load` whose init fails looped the phone, so keep the module's init tolerant (`tests/hello_hvc/el1.c` logs the error and still registers). Recover from the bootloader (hold Volume Down through a reboot) by flashing the last good `boot`, `dtbo` and `vendor_kernel_boot`. `fastboot reboot fastboot` only works if `boot` and `vendor_kernel_boot` are from the same build.
- **"Cannot load Android system. Your data may be corrupt."** after flashing images whose `boot.img` has no OS patch level (`unpack_bootimg.py` shows `None`; Kleaf's does). The wrapped storage keys no longer match, so choose Factory data reset on that screen (or `fastboot -w`).
- The first `fastboot reboot fastboot` needs the new kernel to boot, so the bootloader-level images (`boot`, `dtbo`, `vendor_kernel_boot`) go first and the two `*_dlkm` images second.

- Always use `build-device.sh`. The tree's own `build_*.sh` defaults to Google's prebuilt kernel and ignores edits under `aosp/`.
- The tree is 6.1.124 (aosp HEAD from 2025-03). Before unlocking, note `adb shell uname -r`; if the phone runs a much newer kernel, flash an older Android 16 factory image first (flash.android.com), since vendor userspace (GPU, camera HALs) can depend on newer drivers. Keep a factory image on hand either way: a bad early hyp module stops the boot.
- EL2 modules must load at boot: after pKVM finalizes, the hypervisor rejects the module hypercalls (`hcall_min` in `hyp/nvhe/hyp-main.c`). They load through `kvm-arm.protected_modules=` (in `gs201.dtsi`, currently `exynos-pd,pkvm_s2mpu`) from the first-stage ramdisk in `vendor_kernel_boot.img`, so each EL2 change means reflashing that partition.
- EL1 modules can `insmod` at runtime, which needs root on a user build (e.g. Magisk patching the factory `init_boot.img`). Unsigned modules load (`MODULE_SIG_FORCE` off) but taint the kernel.
- Phone template module: Google's `pkvm-s2mpu` (`gs201/BUILD.bazel:223`).
- Your own modules ride along in the same build. Kleaf is Google's Bazel-based kernel build system (`build_pantah.sh` is a wrapper around it): a `kernel_module` rule builds a module against the exact kernel it belongs to, and `kernel_images` packs modules into the images. `build-device.sh` writes that rule for every dir in `PKVM_MODULES` (default `tests/hello_hvc`; `PKVM_MODULES=""` for none, `PKVM_MODULES="tests/a tests/b"` for several). Each dir is an out-of-tree module as in section 6 (its Makefile or Kbuild needs `obj-m := name.o`). Per module it copies the dir into the tree as `pkvmi/<dir>`, adds the module to the device package's `kernel_ext_modules` (`BUILD.bazel`, line marked `# pkvmi`) and to `vendor_ramdisk.modules.<codename>`, and appends `,<name>` to `kvm-arm.protected_modules=` in the SoC's `dts/gs201.dtsi` bootargs. It has to be the device tree: the kernel keeps the last `kvm-arm.protected_modules=` on its command line, and the built-in `CONFIG_CMDLINE` sits before the device tree's bootargs, so a built-in copy is silently overridden (seen on the phone). Without the name in that list the kernel does not load the module early, `init` loads it later from `modules.load`, and the EL2 load fails with `-95` (`-EOPNOTSUPP`, pKVM already finalized). All three edits (device package, ramdisk list, device tree) are undone at the start of every run, so `STOCK=1` leaves the tree as Google shipped it. The module then appears in `vendor_kernel_boot.img` (and `vendor_dlkm.img`). Check: `grep hello $D/vendor_kernel_boot.modules.load`, `hello.ko` in `$D`, and `hello` at the end of `kvm-arm.protected_modules=` in the built `$D/gs201-*.dtb` (`strings`). On the phone, `adb bugreport` carries the kernel log (`logcat -b kernel` and `dmesg` are not readable as `shell`; use `grep -a`, the file has binary bytes): look for `loading hello from /lib/modules/ failed, fallback to the default path`, then `hello: pkvm_load_el2_module ret=0` and `hello: hvc returned 67 (want 67)`. Verified on a Pixel 7 this way. A module-only rebuild takes ~2.5 min (kernel comes from the Bazel cache). A module's Makefile needs `modules` and `modules_install` targets that run `$(MAKE) -C $(KERNEL_SRC) M=$(M) $@`, which is how Kleaf calls it (see `tests/hello_hvc/Makefile`).
