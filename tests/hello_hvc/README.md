https://source.android.com/docs/core/virtualization/pkvm-modules

PKVM_MODULES=hello ../../scripts/run-qemu.sh

Phone: `scripts/pixel/build-device.sh pixel7 --lto=thin` builds this dir with the kernel and packs it into `vendor_kernel_boot.img` 
