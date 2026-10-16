# 6.12's external-module build can't find Makefile.module's source-relative
# linker script; harmless on 6.1
vpath %.S $(srctree)

# Every .c in this directory is built into the EL2 object.
g_hyp_dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
hyp-obj-y := $(patsubst $(g_hyp_dir)/%.c,%.o,$(sort $(wildcard $(g_hyp_dir)/*.c)))
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module

