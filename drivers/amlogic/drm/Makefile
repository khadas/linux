ifeq ($(CONFIG_DRM_MESON_USE_ION),y)
        meson-drm-y += meson_gem.o meson_fb.o
        ccflags-y += -Idrivers/staging/android/
endif

ifeq ($(CONFIG_DRM_MESON_EMULATE_FBDEV),y)
        meson-drm-y += meson_fbdev.o
endif

ifneq ($(CONFIG_DRM_MESON_VPU),)
        meson-drm-y += meson_vpu.o
endif

ifneq ($(CONFIG_DRM_MESON_HDMI),)
        meson-drm-y += meson_hdmi.o meson_hdcp.o
endif

ifneq ($(CONFIG_DRM_MESON_PANEL),)
        meson-drm-y += meson_lcd.o
endif

meson-drm-y += meson_drv.o meson_plane.o meson_vpu_pipeline_traverse.o \
                meson_crtc.o meson_vpu_pipeline.o meson_vpu_pipeline_private.o \
		meson_debugfs.o	meson_vpu_util.o \

meson-drm-y += \
                vpu-hw/meson_vpu_osd_mif.o \
                vpu-hw/meson_osd_afbc.o \
                vpu-hw/meson_osd_scaler.o \
                vpu-hw/meson_vpu_osdblend.o \
                vpu-hw/meson_vpu_hdr_dv.o \
                vpu-hw/meson_vpu_postblend.o

ifneq ($(CONFIG_DRM_MESON_V0), y)
	ccflags-y += -Idrivers/amlogic/media/osd/ -I$(src)/vpu-hw -I$(src)
	obj-y += meson-drm.o
else
	obj-y += drm-v0/
endif
