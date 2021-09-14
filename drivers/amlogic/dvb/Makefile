obj-$(CONFIG_AMLOGIC_AUCPU) += aucpu/
obj-$(CONFIG_AMLOGIC_DVB_DMX) += demux/
obj-$(CONFIG_AMLOGIC_DVB_DSM) += dsm/
obj-$(CONFIG_AMLOGIC_DVB_EXTERN) += aml_dvb_extern.o

aml_dvb_extern-objs = \
		aml_demod_common.o \
		aml_tuner.o \
		aml_atvdemod.o \
		aml_dtvdemod.o \
		aml_tuner_ops.o \
		aml_demod_ops.o \
		aml_dvb_extern_driver.o

ccflags-y += -I.
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -I$(srctree)/include/media

