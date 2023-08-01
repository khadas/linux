#
#Makefile for the dvb dmx drivers
#

obj-$(CONFIG_AMLOGIC_DVB_DMX) += dvb_demux.o

ccflags-y += -I$(srctree)/include/media
ccflags-y += -I$(srctree)/include

dvb_demux-objs += aml_dvb.o
dvb_demux-objs += aml_dsc.o
dvb_demux-objs += aml_dmx.o

dvb_demux-objs += sw_demux/swdmx_cb_list.o
dvb_demux-objs += sw_demux/swdmx_crc32.o
dvb_demux-objs += sw_demux/swdmx_demux.o
dvb_demux-objs += sw_demux/swdmx_ts_parser.o

dvb_demux-objs += sc2_demux/sc2_control.o
dvb_demux-objs += sc2_demux/mem_desc.o
dvb_demux-objs += sc2_demux/ts_output.o
dvb_demux-objs += sc2_demux/ts_input.o
dvb_demux-objs += sc2_demux/frontend.o
dvb_demux-objs += sc2_demux/dvb_reg.o
