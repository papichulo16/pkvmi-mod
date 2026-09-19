ifneq ($(KERNELRELEASE),)
subdir-ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include/

obj-m := hello.o
hello-y := el1.o hyp/kvm_nvhe.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S
else
KDIR ?= $(abspath $(CURDIR)/../../ignore/out-qemu-6.1)
LLVM ?= 1

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64 LLVM=$(LLVM) modules
	clang --target=aarch64-linux-gnu -O2 -static -nostdlib -ffreestanding \
		-fno-builtin -fuse-ld=lld -isystem $(KDIR)/usr/include \
		-o hello_test hello_test.c

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm64 LLVM=$(LLVM) clean
	rm -f hello_test
endif
