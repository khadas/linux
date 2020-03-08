# # Makefile for the Post Process Manager device #
#ifeq ($(TARGET_BUILD_VARIANT),userdebug)
#ccflags-y += -D DEBUG_SUPPORT
#ccflags-y += -DDEBUG
#else
#ccflags-y += -DDEBUG
#endif

#CFLAGS_deinterlace.o := -I$(src)

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/deinterlace.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/deinterlace_hw.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/deinterlace_mtn.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/deinterlace_dbg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/nr_drv.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/pulldown_drv.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/detect3d.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/nr_downscale.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/di_pps.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/film_mode_fmw/vof_soft_top.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/film_mode_fmw/flm_mod_xx.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += deinterlace/film_mode_fmw/film_fw1.o

ccflags-y += -Idrivers/amlogic/media/common/rdma/ -I$(src)/deinterlace/
