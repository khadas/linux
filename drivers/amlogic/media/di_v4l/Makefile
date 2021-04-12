#asflags-y=-mfloat-abi=softfp -mfpu=neon
ccflags-y += -I.

#obj-$(CONFIG_AMLOGIC_DI_V4L) += di_v4l.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_DI_V4L) += di_v4l/di_v4l.o
ccflags-y += -I$(src)/di_v4l/
