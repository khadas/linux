MODULE_NAME := gdc
FW_SRC := src/fw_lib/acamera_gdc.c \
	src/platform/system_gdc_io.c \
	app/gdc_main.c \
	app/gdc_module.c \
	app/gdc_dmabuf.c \
	app/gdc_wq.c

FW_SRC_OBJ := $(FW_SRC:.c=.o)

obj-$(CONFIG_AMLOGIC_MEDIA_GDC) += $(MODULE_NAME).o
$(MODULE_NAME)-objs := $(FW_SRC_OBJ)

PWD=$(srctree)/$(src)
ccflags-y +=-I$(PWD)/app -I$(PWD)/inc
ccflags-y +=-I$(PWD)/inc/api -I$(PWD)/inc/gdc -I$(PWD)/inc/sys
ccflags-y +=-I$(PWD)/src/platform -I$(PWD)/src/fw_lib
ccflags-y += -I$(srctree)/drivers/amlogic/media/common/ion_dev/
ccflags-y += -I$(srctree)/drivers/staging/android/

ccflags-y += -Wno-declaration-after-statement
