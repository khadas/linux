#
# Makefile for HDMI_RX.
#

obj-$(CONFIG_TVIN_HDMI) += tvin_hdmirx.o
tvin_hdmirx-objs := hdmi_rx_wrapper.o hdmi_rx_hw.o hdmirx_drv.o

ifneq ($(CONFIG_TVIN_HDMI_CEC),)
tvin_hdmirx-objs += hdmirx_cec.o
ccflags-y += -DCEC_FUNC_ENABLE
endif
