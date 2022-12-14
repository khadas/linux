MESON_DRM_MODULE_NAME = aml_drm
obj-$(CONFIG_AMLOGIC_DRM) = $(MESON_DRM_MODULE_NAME).o
$(MESON_DRM_MODULE_NAME)-y = meson_drm_main.o
ifeq ($(CONFIG_DRM_MESON_USE_ION),y)
        $(MESON_DRM_MODULE_NAME)-y += meson_gem.o meson_fb.o
        ccflags-y += -I$(srctree)/drivers/staging/android/
endif

ifeq ($(CONFIG_DRM_MESON_EMULATE_FBDEV),y)
        $(MESON_DRM_MODULE_NAME)-y += meson_fbdev.o
endif

ifneq ($(CONFIG_DRM_MESON_VPU),)
        $(MESON_DRM_MODULE_NAME)-y += meson_vpu.o
endif

$(MESON_DRM_MODULE_NAME)-y += meson_drv.o meson_plane.o \
                              meson_atomic.o  meson_sysfs.o\
                              meson_writeback.o meson_logo.o\
                              meson_async_atomic.o \
                              meson_vpu_pipeline_traverse.o \
                              meson_crtc.o meson_vpu_pipeline.o \
                              meson_vpu_pipeline_private.o \
                              meson_debugfs.o meson_vpu_util.o \
                              meson_drm_bind.o meson_lcd.o\
                              meson_hdmi.o meson_cvbs.o\
                              vpu-hw/meson_vpu_video_wrapper.o \
                              vpu-hw/meson_vpu_osd_mif.o \
                              vpu-hw/meson_osd_afbc.o \
                              vpu-hw/meson_osd_scaler.o \
			      vpu-hw/meson_osd_proc.o \
                              vpu-hw/meson_vpu_osdblend.o \
                              vpu-hw/meson_vpu_hdr_dv.o \
                              vpu-hw/meson_vpu_postblend.o \

ccflags-y += -I$(srctree)/$(src)/vpu-hw -I$(srctree)/$(src) -I$(srctree)/drivers/amlogic/media/
