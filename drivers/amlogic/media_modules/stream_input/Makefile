obj-m	+=	 stream_input.o

stream_input-objs	+=	amports/amstream.o
stream_input-objs	+=	amports/adec.o
stream_input-objs	+=	amports/thread_rw.o
stream_input-objs	+=	amports/streambuf.o
stream_input-objs	+=	amports/stream_buffer_base.o
stream_input-objs	+=	amports/stream_buffer_interface.o

ifndef CONFIG_AMLOGIC_MEDIA_NO_PARSER
stream_input-objs	+=	parser/esparser.o
stream_input-objs	+=	parser/tsdemux.o
stream_input-objs	+=	parser/psparser.o
stream_input-objs	+=	parser/rmparser.o
endif
stream_input-objs	+=	parser/dvb_common.o
stream_input-objs	+=	subtitle/subtitle.o

obj-$(CONFIG_AMLOGIC_DVB)   += parser/hw_demux/
obj-$(CONFIG_AMLOGIC_DVB)   += parser/dvb_ci/

ccflags-y += -I.
ccflags-y += -I$(srctree)/include/media

#obj-y 	+= 	tv_frontend/
# obj-y	+=	box-frontend/avl6211/
# obj-y	+=	box-frontend/atbm8881/
# obj-y	+=	box-frontend/avl68xx/
