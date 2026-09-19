ifneq ($(KERNELRELEASE),)
obj-m := hello.o
else
KDIR ?= $(abspath $(CURDIR)/../../ignore/out-qemu-6.1)
# the compiler the kernel was built with; 1 = the system LLVM
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
