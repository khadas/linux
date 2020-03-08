# # Makefile for the Post Process Manager device #
#ifeq ($(TARGET_BUILD_VARIANT),userdebug)
#ccflags-y += -D DEBUG_SUPPORT
#ccflags-y += -DDEBUG
#else
#ccflags-y += -DDEBUG
#endif

ccflags-y += -I.

#CFLAGS_dim.o := -I$(src)
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/deinterlace.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/deinterlace_hw.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/deinterlace_dbg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/nr_downscale.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_pps.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_vframe.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_prc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_pre.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_post.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_reg_tab.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_dbg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_que.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_sys.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_task.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di_multi/di_api.o

ccflags-y += -Idrivers/amlogic/media/common/rdma/
ccflags-y += -I$(src)/di_multi/
