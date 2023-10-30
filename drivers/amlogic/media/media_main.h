/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MEDIA_MAIN_H__
#define _MEDIA_MAIN_H__

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
int codec_mm_module_init(void);
int dmabuf_manage_init(void);
#else
static inline int codec_mm_module_init(void)
{
	return 0;
}

static inline int dmabuf_manage_init(void)
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

#ifdef CONFIG_AMLOGIC_DSC
int dsc_enc_init(void);
#else
static inline int dsc_enc_init(void)
{
	return 0;
}
#endif

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

#ifdef CONFIG_AMLOGIC_HDMITX21
int amhdmitx21_init(void);
#else
static inline int amhdmitx21_init(void)
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

#ifdef CONFIG_AMLOGIC_ESM
int esm_init(void);
void esm_exit(void);
#else
static inline int esm_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
int hdmirx_init(void);
int hld_init(void);
#else
static inline int hdmirx_init(void)
{
	return 0;
}

static inline int hld_init(void)
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

#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
int aml_vclk_init_module(void);
#else
static inline int aml_vclk_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
int vout_mux_init(void);
int dummy_venc_init(void);
int vout_init_module(void);
int vout_sys_serve_init(void);
#else
static inline int vout_mux_init(void)
{
	return 0;
}
static inline int dummy_venc_init(void)
{
	return 0;
}
static inline int vout_init_module(void)
{
	return 0;
}

static inline int vout_sys_serve_init(void)
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

#ifdef CONFIG_AMLOGIC_PERIPHERAL_LCD
int peripheral_lcd_init(void);
#else
static inline int peripheral_lcd_init(void)
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

#ifdef CONFIG_AMLOGIC_MEDIA_VRR
int __init vrr_init(void);
#else
static int vrr_init(void)
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

#ifdef CONFIG_AMLOGIC_UVM_CORE
int mua_init(void);
#else
static inline int mua_init(void)
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

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
int ge2d_init_module(void);
#else
static inline int ge2d_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VICP
int vicp_init_module(void);
#else
static inline int vicp_init_module(void)
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

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
int video_init(void);
#else
static inline int video_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
int vout2_init_module(void);
#else
static inline int vout2_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
int vout3_init_module(void);
#else
static inline int vout3_init_module(void)
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
int tsync_pcr_init(void);
#else
static inline int tsync_module_init(void)
{
	return 0;
}

static inline int tsync_pcr_init(void)
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

#ifdef CONFIG_AMLOGIC_VIDEO_TUNNEL
int meson_videotunnel_init(void);
void meson_videotunnel_exit(void);
#else
static inline int meson_videotunnel_init(void)
{
	return 0;
}

static inline void meson_videotunnel_exit(void)
{
}
#endif

#ifdef CONFIG_AMLOGIC_VIDEOQUEUE
int videoqueue_init(void);
void videoqueue_exit(void);
#else
static inline int videoqueue_init(void)
{
	return 0;
}

static inline void videoqueue_exit(void)
{
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

#ifdef CONFIG_AMLOGIC_VDETECT
int vdetect_init(void);
void vdetect_exit(void);
#else
static inline int vdetect_init(void)
{
	return 0;
}

static inline void vdetect_exit(void)
{
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
int frc_init(void);
#else
static inline int frc_init(void)
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

#ifdef CONFIG_AMLOGIC_BL_LDIM
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

#ifdef CONFIG_AMLOGIC_DI_V4L
int di_v4l_init(void);
#else
static inline int di_v4l_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
int dil_init(void);
int di_module_init(void);
int dim_module_init(void);
#else
static inline int dil_init(void)
{
	return 0;
}

static inline int di_module_init(void)
{
	return 0;
}

static inline int dim_module_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_AO_CEC
int cec_init(void);
#else
static int cec_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
int vpu_security_init(void);
#else
static int vpu_security_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ADC
int adc_init(void);
#else
static int adc_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
int tvafe_drv_init(void);
#else
static int tvafe_drv_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_VBI
int vbi_init(void);
#else
static int vbi_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
int tvafe_avin_detect_init(void);
#else
static int tvafe_avin_detect_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_BT656
int __init amvdec_656in_init_module(void);
#else
static int amvdec_656in_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
int vdin_drv_init(void);
#else
static int vdin_drv_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIUIN
int viuin_init_module(void);
#else
static int viuin_init_module(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_RESMANAGE
int resman_init(void);
#else
static int resman_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
int __init lut_dma_init(void);
#else
static int lut_dma_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
int __init aml_atvdemod_init(void);
#else
static int aml_atvdemod_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_DTV_DEMOD
int __init aml_dtvdemod_init(void);
#else
static int aml_dtvdemod_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_MINFO
int __init minfo_init(void);
void __exit minfo_exit(void);
#else
static int minfo_init(void)
{
	return 0;
}

void __exit minfo_exit(void)
{
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_MSYNC
int __init msync_init(void);
void __exit msync_exit(void);
#else
static int msync_init(void)
{
	return 0;
}

void __exit msync_exit(void)
{
}
#endif

#ifdef CONFIG_AMLOGIC_SECURE_DMABUF
int __init amlogic_system_secure_dma_buf_init(void);
#else
static int amlogic_system_secure_dma_buf_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VPP
int __init vpp_drv_init(void);
#else
static int vpp_drv_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
int amprime_sl_init(void);
#else
static inline int amprime_sl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VPQ
int vpq_init(void);
#else
static inline int vpq_init(void)
{
	return 0;
}
#endif
#endif
