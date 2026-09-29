# 6.12's external-module build can't find Makefile.module's source-relative
# linker script; harmless on 6.1
vpath %.S $(srctree)

hyp-obj-y := el2.o
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module
