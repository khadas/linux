# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
# Makefile for audiobridge

obj-$(CONFIG_AMLOGIC_AUDIO_BRIDGE)    +=	bridge_audio.o \
		ringbuffer.o \
		bridge_pcm_hal.o \
		bridge_arm_pcm.o

obj-$(CONFIG_AMLOGIC_BRIDGE_UAC)    +=	bridge_uac_pcm.o

obj-$(CONFIG_AMLOGIC_BRIDGE_DSP)    +=	bridge_dsp_pcm.o \
		dsp_client_api.o \
		bridge_dsp_card.o
ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif
