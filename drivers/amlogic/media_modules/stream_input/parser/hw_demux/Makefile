obj-m	+= aml_hardware_dmx.o

ccflags-y += -I$(srctree)/drivers/media/dvb-core -I$(srctree)/drivers/gpio -I$(srctree)/include -DENABLE_DEMUX_DRIVER

aml_hardware_dmx-objs += aml_dvb.o
aml_hardware_dmx-objs += aml_dmx.o

#obj-y += dvb_ci/
