ifneq ($(KERNELRELEASE),)
g_inc := $(abspath $(src)/intf)
subdir-ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include/ -I$(g_inc)
ccflags-y += -I$(g_inc)

g_kern_src := $(sort $(wildcard $(src)/kern/*.c))
g_kern_obj := $(patsubst $(src)/%.c,%.o,$(g_kern_src))

obj-m := pkvmi.o
pkvmi-y := $(g_kern_obj) hyp/kvm_nvhe.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S
else ifneq ($(KERNEL_SRC),)

# Kleaf (scripts/pixel/build-device.sh) runs
#   make -C <this dir> KERNEL_SRC=<kernel> M=<rel dir> O=<out> [modules_install]
# with the toolchain on the command line, like Google's own modules. Its first
# call names no goal, so `modules` must stay the first target.
modules modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@
else
KDIR ?= $(abspath $(CURDIR)/../ignore/out-qemu-6.1)
LLVM ?= 1

g_usr_src := $(sort $(wildcard $(CURDIR)/usr/*.c))

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64 LLVM=$(LLVM) modules
	clang --target=aarch64-linux-gnu -O2 -static -nostdlib -ffreestanding \
		-fno-builtin -fuse-ld=lld -isystem $(KDIR)/usr/include \
		-I intf -o userspace $(g_usr_src)

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64 LLVM=$(LLVM) clean
	rm -f userspace
endif
