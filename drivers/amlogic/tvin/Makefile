#
# Makefile for TVIN.
#

obj-$(CONFIG_TVIN) += tvin_common.o
tvin_common-objs := tvin_format_table.o tvin_frontend.o tvin_global.o
#obj-y += vdin_4v4l2.o
#vdin_4v4l2-objs := vdin_v4l2.o

obj-$(CONFIG_TVIN_VDIN)	     += vdin/
obj-$(CONFIG_TVIN_AFE)	     += tvafe/
obj-$(CONFIG_TVIN_HDMI)	+= hdmirx/
obj-$(CONFIG_TVIN_HDMI_EXT)	+= hdmirx_ext/
obj-$(CONFIG_TVIN_BT656)     += bt656/
#obj-$(CONFIG_TVIN_CSI)       += csi/
obj-$(CONFIG_VIUIN)		+= viu/
#obj-$(CONFIG_TVIN_IT660X)    += it660x/
#obj-$(CONFIG_TVIN_ISP)       += isp/
#obj-$(CONFIG_TVIN_VDIN_CTRL) += vdin_ctrl/

