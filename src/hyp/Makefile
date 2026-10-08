# 6.12's external-module build can't find Makefile.module's source-relative
# linker script; harmless on 6.1
vpath %.S $(srctree)

# Every .c in this directory is built into the EL2 object.
hyp-obj-y := $(patsubst $(src)/%.c,%.o,$(sort $(wildcard $(src)/*.c)))
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module

