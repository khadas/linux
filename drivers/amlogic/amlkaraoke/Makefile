#
# Makefile for sound control interface
#

# Toplevel Module Dependency
#AML usb audio in to i2s out mixed


# AML USB capture
snd-usb-capture-objs := aml_usb_capture.o aml_audio_resampler.o aml_reverb.o
obj-$(CONFIG_AMLOGIC_SND_USB_CAPTURE_DATA) += snd-usb-capture.o

aml-i2s-out-mix-objs := aml_i2s_out_mix.o
obj-$(CONFIG_AMLOGIC_SND_USB_CAPTURE_DATA) += aml-i2s-out-mix.o

amlogic_karaoke-objs := aml_karaoke.o
obj-$(CONFIG_AMLKARAOKE) += amlogic_karaoke.o
