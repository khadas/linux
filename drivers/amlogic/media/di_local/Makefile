# # Makefile for the Post Process Manager device #
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -D DEBUG_SUPPORT
ccflags-y := -DDEBUG
else
ccflags-y := -DDEBUG
endif
CFLAGS_dil.o := -I$(src)
obj-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += dil.o
dil-objs += di_local.o
#ccflags-y += -Idrivers/amlogic/media/deinterlace/

