
obj-$(CONFIG_AM_DTVDEMOD) += aml_fe.o

aml_fe-objs := demod_func.o  dvbc_func.o i2c_func.o tuner_func.o atsc_func.o#dvbt_func.o

aml_fe-objs += amlfrontend.o

aml_fe-objs	+= aml_demod.o

#aml_fe-objs	+= mxl661_frontend.o mxl661_func.o mxl661_i2c.o

ifeq ($(CONFIG_AM_DEMOD_FPGA_DEBUG), y)
aml_fe-objs	+= fpga_func.o si2176_func.o
ccflags-y += -DDEMOD_FPGA_VERSION
endif

ccflags-y += -I.
ccflags-y += -Idrivers/media/dvb-core
ccflags-y += -Idrivers/amlogic/dvb_tv/amldemod/include
ccflags-y += -Idrivers/amlogic/dvb_tv/

