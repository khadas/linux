#
# Makefile for the remote control drivers
#

# Each configuration option enables a list of files.

MODULE_NAME = meson_ir
obj-$(CONFIG_AMLOGIC_MESON_IR) += $(MODULE_NAME).o
$(MODULE_NAME)-y += meson_ir_core.o \
		 meson_ir_main.o \
		 meson_ir_sysfs.o \
		 meson_ir_cdev.o \
		 meson_ir_raw.o \
		 meson_ir_regmap.o\
		 meson_ir_decoder_xmp.o
