obj-m += ci.o
#cimax-usb.o

ci-objs = $(amlci-objs) $(cimcu-objs)
#$(cimax-objs)

amlci-objs = aml_pcmcia.o  aml_ci.o  aml_ci_bus.o
#aml_spi.o
cimcu-objs = cimax/dvb_ringbuffer.o cimcu/dvb_ca_en50221_cimcu.o
#cimax-objs = cimax/dvb_ca_en50221_cimax.o cimax/aml_cimax.o  cimax/dvb_ringbuffer.o
KBUILD_CFLAGS += -Wno-implicit-fallthrough
#cimax-usb-objs += cimax/usb/SRC/cimax+usb-driver.o cimax/usb/SRC/cimax+usb_fw.o
#cimax-usb-objs += cimax/usb/SRC/cimax+usb_config.o
#cimax-objs += cimax/aml_cimax_usb.o

ccflags-y += -I$(srctree)/
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/drivers/gpio
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -I$(srctree)/include/media
ccflags-y += -I$(srctree)/drivers/media/pci/ttpci

