# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
# Makefile for hifi4dsp

MODULE_NAME = hifidsp
obj-$(CONFIG_AMLOGIC_HIFI4DSP) = $(MODULE_NAME).o
$(MODULE_NAME)-y   +=	hifi4dsp_module.o \
		hifi4dsp_firmware.o \
		hifi4dsp_dsp.o \
		dsp_top.o \
		hifi4dsp_syslog.o

ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif

#obj-$(CONFIG_AMLOGIC_HIFI4DSP)    +=hifi4dsp_test.o

#EXTRA_CFLAGS = -DENABLE_WAIT_FORMAT
