
# Makefile for tm2 hifi4dsp

hifi4dsp-objs =	hifi4dsp_module.o \
		hifi4dsp_firmware.o \
		hifi4dsp_dsp.o \
		hifi4dsp_ipc.o \
		tm2_dsp_top.o

#audiodsp-objs += pcmenc_module.o pcmenc_stream.o
#audiodsp-objs += spdif_module.o

ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif

obj-$(CONFIG_AMLOGIC_HIFI4DSP)    +=hifi4dsp.o

#EXTRA_CFLAGS = -DENABLE_WAIT_FORMAT
