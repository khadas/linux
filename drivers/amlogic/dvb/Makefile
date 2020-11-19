obj-$(CONFIG_AMLOGIC_AUCPU) += aucpu/
obj-$(CONFIG_AMLOGIC_DVB_DMX) += demux/
obj-$(CONFIG_AMLOGIC_DVB_EXTERN) += aml_dvb_extern_fe.o

aml_dvb_extern_fe-objs = \
		aml_dvb_extern.o \
		aml_demod_common.o \
		aml_tuner.o \
		aml_atvdemod.o \
		aml_dtvdemod.o

ccflags-y += -I.
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -I$(srctree)/include/media