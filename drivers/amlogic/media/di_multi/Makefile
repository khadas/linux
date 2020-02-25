# # Makefile for the Post Process Manager device #
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -D DEBUG_SUPPORT
ccflags-y := -DDEBUG
else
ccflags-y := -DDEBUG
endif
ccflags-y += -I.
CFLAGS_dim.o := -I$(src)
obj-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += dim.o
dim-objs += deinterlace.o
dim-objs += deinterlace_hw.o
#dim-objs += deinterlace_mtn.o
dim-objs += deinterlace_dbg.o
#dim-objs += nr_drv.o
#dim-objs += pulldown_drv.o
#dim-objs += detect3d.o
dim-objs += nr_downscale.o
dim-objs += di_pps.o
dim-objs += di_vframe.o
dim-objs += di_prc.o
dim-objs += di_pre.o
dim-objs += di_post.o
dim-objs += di_reg_tab.o
dim-objs += di_dbg.o
dim-objs += di_que.o
dim-objs += di_sys.o
dim-objs += di_task.o
dim-objs += di_api.o
#dim-objs += film_mode_fmw/vof_soft_top.o
#dim-objs += film_mode_fmw/flm_mod_xx.o
#dim-objs += film_mode_fmw/film_fw1.o
ccflags-y += -Idrivers/amlogic/media/common/rdma/
ccflags-y += -I$(src)