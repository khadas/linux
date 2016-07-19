#
# Makefile for the Post Process Manager device
#
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -D DEBUG_SUPPORT
else
ccflags-y := -DDEBUG
endif
obj-$(CONFIG_DEINTERLACE) += di.o
di-objs = deinterlace.o deinterlace_hw.o deinterlace_pd.o dnr.o vof_soft_top.o film_fw1.o flm_mod_xx.o detect3d.o tffbff_check.o


