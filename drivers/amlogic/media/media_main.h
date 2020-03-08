/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MEDIA_MAIN_H__
#define _MEDIA_MAIN_H__

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
int codec_mm_module_init(void);
#else
static inline int codec_mm_module_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
int media_configs_system_init(void);
#else
static inline int media_configs_system_init(void)
{
	return 0;
}
#endif

int codec_io_init(void);

int vdec_reg_ops_init(void);

#ifdef CONFIG_AMLOGIC_VPU
int vpu_init(void);
#else
static inline int vpu_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
int amcanvas_init(void);
#else
static inline int amcanvas_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
int amrdma_init(void);
#else
static inline int amrdma_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_HDMITX
int amhdmitx_init(void);
#else
static inline int amhdmitx_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_FB
int osd_init_module(void);
#else
static inline int osd_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
int vout_init_module(void);
#else
static inline int vout_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_CVBS_OUTPUT
int cvbs_init_module(void);
#else
static inline int cvbs_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD
int lcd_init(void);
#else
static inline int lcd_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VDAC
int aml_vdac_init(void);
#else
static inline int aml_vdac_init(void)
{
	return 0;
}
#endif

int gp_pll_init(void);

#ifdef CONFIG_AMLOGIC_ION
int ion_init(void);
#else
static inline int ion_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VFM
int vfm_class_init(void);
#else
static inline int vfm_class_init(void)
{
	return 0;
}
#endif

int meson_cpu_version_init(void);

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
int ge2d_init_module(void);
#else
static inline int ge2d_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
int configs_init_devices(void);
#else
static inline int configs_init_devices(void)
{
	return 0;
}
#endif

int video_init(void);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
int vout2_init_module(void);
#else
static inline int vout2_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
int ppmgr_init_module(void);
#else
static inline int ppmgr_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEOSYNC
int videosync_init(void);
#else
static inline int videosync_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_PIC_DEC
int picdec_init_module(void);
#else
static inline int picdec_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
int amdolby_vision_init(void);
#else
static inline int amdolby_vision_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_FRAME_SYNC
int tsync_module_init(void);
#else
static inline int tsync_module_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_BL_EXTERN
int aml_bl_extern_i2c_init(void);
#else
static inline int aml_bl_extern_i2c_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEO_COMPOSER
int video_composer_module_init(void);
#else
static inline int video_composer_module_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_EXTERN
int aml_lcd_extern_i2c_dev_init(void);
#else
static inline int aml_lcd_extern_i2c_dev_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
int aml_vecm_init(void);
#else
static inline int aml_vecm_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_IONVIDEO
int ionvideo_init(void);
#else
static inline int ionvideo_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
int v4lvideo_init(void);
#else
static inline int v4lvideo_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_V4L_VIDEO
int amlvideo_init(void);
#else
static inline int amlvideo_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_V4L_VIDEO2
int amlvideo2_init(void);
#else
static inline int amlvideo2_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_BL_EXTERN
int aml_bl_extern_init(void);
#else
static inline int aml_bl_extern_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_GDC
int gdc_driver_init(void);
#else
static inline int gdc_driver_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_EXTERN
int aml_lcd_extern_init(void);
#else
static inline int aml_lcd_extern_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
int ldim_dev_init(void);
#else
static inline int ldim_dev_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_BACKLIGHT
int aml_bl_init(void);
#else
static inline int aml_bl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
int dil_init(void);
#else
static inline int dil_init(void)
{
	return 0;
}
#endif

#endif
