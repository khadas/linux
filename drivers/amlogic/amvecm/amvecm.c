/*
 * amvecm char device driver.
 *
 * Copyright (c) 2010 Frank Zhao<frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include <linux/amlogic/vout/vout_notify.h>

#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"

#include "amve.h"
#include "amcm.h"
#include "amvecm_vlock_regmap.h"

#define pr_amvecm_dbg(fmt, args...)\
	do {\
		if (debug_amvecm)\
			printk("AMVECM: " fmt, ## args);\
	} while (0)
#define pr_amvecm_error(fmt, args...)\
	printk("AMVECM: " fmt, ## args)


#define AMVECM_NAME               "amvecm"
#define AMVECM_DRIVER_NAME        "amvecm"
#define AMVECM_MODULE_NAME        "amvecm"
#define AMVECM_DEVICE_NAME        "amvecm"
#define AMVECM_CLASS_NAME         "amvecm"
struct amvecm_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
};

static int hue_pre;  /*-25~25*/
static int saturation_pre;  /*-128~127*/
static int hue_post;  /*-25~25*/
static int saturation_post;  /*-128~127*/
static signed int vd1_brightness = 0, vd1_contrast;
static struct amvecm_dev_s amvecm_dev;

static bool debug_amvecm;
module_param(debug_amvecm, bool, 0664);
MODULE_PARM_DESC(debug_amvecm, "\n debug_amvecm\n");

unsigned int vecm_latch_flag;
module_param(vecm_latch_flag, uint, 0664);
MODULE_PARM_DESC(vecm_latch_flag, "\n vecm_latch_flag\n");

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
#define VLOCK_MODE_ENC          0
#define VLOCK_MODE_PLL		1

unsigned int vlock_mode = VLOCK_MODE_PLL;/* 0:enc;1:pll */
module_param(vlock_mode, uint, 0664);
MODULE_PARM_DESC(vlock_mode, "\n vlock_mode\n");

unsigned int vlock_en = 0;
module_param(vlock_en, uint, 0664);
MODULE_PARM_DESC(vlock_en, "\n vlock_en\n");

/*
0:only support 50->50;60->60;24->24;30->30;
1:support 24/30/50/60/100/120 mix,such as 50->60;
*/
unsigned int vlock_adapt = 0;
module_param(vlock_adapt, uint, 0664);
MODULE_PARM_DESC(vlock_adapt, "\n vlock_adapt\n");

unsigned int vlock_dis_cnt_limit = 2;
module_param(vlock_dis_cnt_limit, uint, 0664);
MODULE_PARM_DESC(vlock_dis_cnt_limit, "\n vlock_dis_cnt_limit\n");

static unsigned int vlock_sync_limit_flag;
static enum vmode_e pre_vmode = VMODE_1080P;
static enum vframe_source_type_e pre_source_type =
		VFRAME_SOURCE_TYPE_OTHERS;
static enum vframe_source_mode_e pre_source_mode =
		VFRAME_SOURCE_MODE_OTHERS;
static unsigned int pre_input_freq;
static unsigned int pre_output_freq;
static unsigned int vlock_dis_cnt;


unsigned int sync_3d_h_start = 0;
module_param(sync_3d_h_start, uint, 0664);
MODULE_PARM_DESC(sync_3d_h_start, "\n sync_3d_h_start\n");

unsigned int sync_3d_h_end = 0;
module_param(sync_3d_h_end, uint, 0664);
MODULE_PARM_DESC(sync_3d_h_end, "\n sync_3d_h_end\n");

unsigned int sync_3d_v_start = 10;
module_param(sync_3d_v_start, uint, 0664);
MODULE_PARM_DESC(sync_3d_v_start, "\n sync_3d_v_start\n");

unsigned int sync_3d_v_end = 20;
module_param(sync_3d_v_end, uint, 0664);
MODULE_PARM_DESC(sync_3d_v_end, "\n sync_3d_v_end\n");

unsigned int sync_3d_polarity = 0;
module_param(sync_3d_polarity, uint, 0664);
MODULE_PARM_DESC(sync_3d_polarity, "\n sync_3d_polarity\n");

unsigned int sync_3d_out_inv = 0;
module_param(sync_3d_out_inv, uint, 0664);
MODULE_PARM_DESC(sync_3d_out_inv, "\n sync_3d_out_inv\n");

unsigned int sync_3d_black_color = 0x008080;/* yuv black */
module_param(sync_3d_black_color, uint, 0664);
MODULE_PARM_DESC(sync_3d_black_color, "\n sync_3d_black_color\n");

/* 3d sync to v by one enable/disable */
unsigned int sync_3d_sync_to_vbo = 0;
module_param(sync_3d_sync_to_vbo, uint, 0664);
MODULE_PARM_DESC(sync_3d_sync_to_vbo, "\n sync_3d_sync_to_vbo\n");
/* #endif */

unsigned int pq_load_en = 1;/* load pq table enable/disable */
module_param(pq_load_en, uint, 0664);
MODULE_PARM_DESC(pq_load_en, "\n pq_load_en\n");

/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
bool gamma_en = 0;  /* wb_gamma_en enable/disable */
/* #else */
/* bool gamma_en = 1; */
/* #endif */
module_param(gamma_en, bool, 0664);
MODULE_PARM_DESC(gamma_en, "\n gamma_en\n");

bool wb_en = 1;  /* wb_en enable/disable */
module_param(wb_en, bool, 0664);
MODULE_PARM_DESC(wb_en, "\n wb_en\n");


/* extern unsigned int cm_size; */
/* extern unsigned int ve_size; */
/* extern unsigned int cm2_patch_flag; */
/* extern struct ve_dnlp_s am_ve_dnlp; */
/* extern struct ve_dnlp_table_s am_ve_new_dnlp; */
/* extern int cm_en; //0:disabel;1:enable */
/* extern struct tcon_gamma_table_s video_gamma_table_r; */
/* extern struct tcon_gamma_table_s video_gamma_table_g; */
/* extern struct tcon_gamma_table_s video_gamma_table_b; */
/* extern struct tcon_gamma_table_s video_gamma_table_r_adj; */
/* extern struct tcon_gamma_table_s video_gamma_table_g_adj; */
/* extern struct tcon_gamma_table_s video_gamma_table_b_adj; */
/* extern struct tcon_rgb_ogo_s     video_rgb_ogo; */

static void amvecm_size_patch(void)
{
	unsigned int hs, he, vs, ve;
	/* #if (MESON_CPU_TYPE==MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_g9tv_cpu()) {
		hs = READ_VPP_REG_BITS(VPP_HSC_REGION12_STARTP, 16, 12);
		he = READ_VPP_REG_BITS(VPP_HSC_REGION4_ENDP, 0, 12);

		vs = READ_VPP_REG_BITS(VPP_VSC_REGION12_STARTP, 16, 12);
		ve = READ_VPP_REG_BITS(VPP_VSC_REGION4_ENDP, 0, 12);
	} else {
		/* #else */
		hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 16, 12);
		he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 0, 12);

		vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 16, 12);
		ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 0, 12);
	}
	/* #endif */
/* #if ((MESON_CPU_TYPE==MESON_CPU_TYPE_MESON8)|| */
/* (MESON_CPU_TYPE==MESON_CPU_TYPE_MESON8B)) */
/* if(cm_en) */
/* #endif */
/* cm2_frame_size_patch(he-hs+1,ve-vs+1); */
	if ((is_meson_m8_cpu() || is_meson_m8m2_cpu() ||
		is_meson_m8b_cpu() || is_meson_gxbb_cpu())
		&& cm_en)
		cm2_frame_size_patch(he-hs+1, ve-vs+1);
/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESON6TVD) */
	if (is_meson_g9tv_cpu())
		ve_frame_size_patch(he-hs+1, ve-vs+1);
/* #endif */

}

static void vd1_brightness_contrast(signed int brightness,
		signed int contrast)
{
	signed int ao0 =  -64, g00 = 1024, g01 =    0, g02 =    0, po0 =  64;
	signed int ao1 = -512, g10 =    0, g11 = 1024, g12 =    0, po1 = 512;
	signed int ao2 = -512, g20 =    0, g21 =    0, g22 = 1024, po2 = 512;
	unsigned int gc0 =    0, gc1 =    0, gc2 =    0, gc3 =    0, gc4 = 0;
	unsigned int a01 =    0, a_2 =    0, p01 =    0, p_2 =    0;
	/* enable vd0_csc */
	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	po0 += brightness >> 1;
	if (po0 >  1023)
		po0 =  1023;
	if (po0 < -1024)
		po0 = -1024;
	g00  *= contrast + 2048;
	g00 >>= 11;
	if (g00 >  4095)
		g00 =  4095;
	if (g00 < -4096)
		g00 = -4096;
	if (contrast < 0) {
		g11  *= contrast   + 2048;
		g11 >>= 11;
	}
	if (brightness < 0) {
		g11  += brightness >> 1;
		if (g11 >  4095)
			g11 =  4095;
		if (g11 < -4096)
			g11 = -4096;
	}
	if (contrast < 0) {
		g22  *= contrast   + 2048;
		g22 >>= 11;
	}
	if (brightness < 0) {
		g22  += brightness >> 1;
		if (g22 >  4095)
			g22 =  4095;
		if (g22 < -4096)
			g22 = -4096;
	}
	gc0 = ((g00 << 16) & 0x1fff0000) | ((g01 <<  0) & 0x00001fff);
	gc1 = ((g02 << 16) & 0x1fff0000) | ((g10 <<  0) & 0x00001fff);
	gc2 = ((g11 << 16) & 0x1fff0000) | ((g12 <<  0) & 0x00001fff);
	gc3 = ((g20 << 16) & 0x1fff0000) | ((g21 <<  0) & 0x00001fff);
	gc4 = ((g22 <<  0) & 0x00001fff);
	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_g9tv_cpu()) {
		a01 = ((ao0 << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<  0) & 0x00000fff);
		p01 = ((po0 << 16) & 0x0fff0000) |
				((po1 <<  0) & 0x00000fff);
		p_2 = ((po2 <<  0) & 0x00000fff);
	} else {
		/* #else */
		a01 = ((ao0 << 16) & 0x07ff0000) |
				((ao1 <<  0) & 0x000007ff);
		a_2 = ((ao2 <<  0) & 0x000007ff);
		p01 = ((po0 << 16) & 0x07ff0000) |
				((po1 <<  0) & 0x000007ff);
		p_2 = ((po2 <<  0) & 0x000007ff);
	}
	/* #endif */
	WRITE_VPP_REG(VPP_MATRIX_CTRL         , ctl);
	WRITE_VPP_REG(VPP_MATRIX_COEF00_01    , gc0);
	WRITE_VPP_REG(VPP_MATRIX_COEF02_10    , gc1);
	WRITE_VPP_REG(VPP_MATRIX_COEF11_12    , gc2);
	WRITE_VPP_REG(VPP_MATRIX_COEF20_21    , gc3);
	WRITE_VPP_REG(VPP_MATRIX_COEF22       , gc4);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , a_2);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1    , p01);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET2      , p_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL         , ori);
}

static void amvecm_bricon_process(void)
{
	if (vecm_latch_flag & FLAG_BRI_CON) {
		vecm_latch_flag &= ~FLAG_BRI_CON;
		vd1_brightness_contrast(vd1_brightness, vd1_contrast);
		pr_amvecm_dbg("\n[amvecm..] set vd1_brightness_contrast OK!!!\n");
	}
}

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
static unsigned int amvecm_vlock_check_input_hz(struct vframe_s *vf)
{
	unsigned int ret_hz = 0;
	enum tvin_sig_fmt_e fmt = vf->sig_fmt;

	if ((vf->source_type != VFRAME_SOURCE_TYPE_TUNER) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_CVBS) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_HDMI))
		ret_hz = 0;
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		if (
		((fmt >= TVIN_SIG_FMT_HDMI_640X480P_60HZ) &&
		(fmt <= TVIN_SIG_FMT_HDMI_1920X1080P_60HZ)) ||
		(fmt == TVIN_SIG_FMT_HDMI_2880X480P_60HZ) ||
		(fmt == TVIN_SIG_FMT_HDMI_2880X576P_60HZ) ||
		(fmt == TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_ALTERNATIVE) ||
		(fmt == TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING)
		)
			ret_hz = 60;
		else if (
		((fmt >= TVIN_SIG_FMT_HDMI_720X576P_50HZ) &&
		(fmt <= TVIN_SIG_FMT_HDMI_1920X1080P_50HZ)) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B) ||
		(fmt == TVIN_SIG_FMT_HDMI_1280X720P_50HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_ALTERNATIVE) ||
		(fmt == TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING)
		)
			ret_hz = 50;
		else if (
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080P_24HZ) ||
		(fmt == TVIN_SIG_FMT_HDMI_1280X720P_24HZ) ||
		(fmt == TVIN_SIG_FMT_HDMI_1280X720P_24HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_ALTERNATIVE)
		)
			ret_hz = 24;
		else if (
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080P_30HZ) ||
		(fmt == TVIN_SIG_FMT_HDMI_1280X720P_30HZ) ||
		(fmt == TVIN_SIG_FMT_HDMI_1280X720P_30HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_FRAME_PACKING) ||
		(fmt == TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_ALTERNATIVE)
		)
			ret_hz = 30;
		else
			ret_hz = 0;
	} else if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS)) {
		if (vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
			ret_hz = 60;
		else if ((vf->source_mode == VFRAME_SOURCE_MODE_PAL) ||
			(vf->source_mode == VFRAME_SOURCE_MODE_SECAM))
			ret_hz = 50;
		else
			ret_hz = 0;
	}
	return ret_hz;
}

static unsigned int
amvecm_vlock_check_output_hz(unsigned int sync_duration_num)
{
	unsigned int ret_hz = 0;
	switch (sync_duration_num) {
	case 24:
		ret_hz = 24;
		break;
	case 30:
		ret_hz = 30;
		break;
	case 50:
		ret_hz = 50;
		break;
	case 60:
		ret_hz = 60;
		break;
	case 100:
		ret_hz = 100;
		break;
	case 120:
		ret_hz = 120;
		break;
	default:
		ret_hz = 0;
		break;
	}
	return ret_hz;
}
static void amvecm_vlock_setting(struct vframe_s *vf,
		unsigned int input_hz, unsigned int output_hz)
{
	unsigned int freq_hz = 0;
	if (((input_hz != output_hz) && (vlock_adapt == 0)) ||
			(input_hz == 0) || (output_hz == 0)) {
		/* VLOCK_CNTL_EN disable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,0,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 0);
		vlock_dis_cnt = vlock_dis_cnt_limit;
		pr_info("[%s]auto disable vlock module for no support case!!!\n",
				__func__);
		return;
	}
	aml_write_cbus(HHI_VID_LOCK_CLK_CNTL, 0x80);
	if (vlock_mode == VLOCK_MODE_ENC) {
		am_set_regmap(&vlock_enc_lcd720x480);
		/* VLOCK_CNTL_EN disable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,0,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 0);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN enable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 1, 13, 1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 30, 1);
	}
	if (vlock_mode == VLOCK_MODE_PLL) {
		/* av pal in,1080p60 hdmi out as default */
		am_set_regmap(&vlock_pll_in50hz_out60hz);
		/*
		set input & output freq
		bit0~7:input freq
		bit8~15:output freq
		*/
		freq_hz = input_hz | (output_hz << 8);
		WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, freq_hz, 0, 16);
		/*
		Ifrm_cnt_mod:0x3001(bit23~16);
		(output_freq/input_freq)*Ifrm_cnt_mod must be integer
		*/
		if (vlock_adapt == 0)
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				1, 16, 8);
		else
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				input_hz, 16, 8);
		/*set PLL M_INT;PLL M_frac*/
		/* WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, */
		/* READ_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL,0,9),12,9); */
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096,
			(aml_read_cbus(HHI_HDMI_PLL_CNTL) & 0x3ff), 12, 9);
		/* WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, */
		/* READ_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL,0,12),0,12); */
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096,
			(aml_read_cbus(HHI_HDMI_PLL_CNTL) & 0x1fff), 0, 12);
		/* vlock module output goes to which module */
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 0, 26, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 2, 26, 2);
			break;
		case 2: /* ENCP */
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 1, 26, 2);
			break;
		default:
			break;
		}
		/*enable vlock to adj pll*/
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 0, 13, 1);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* VLOCK_CNTL_EN enable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,1,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 29, 1);
	}
	if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS))
		/* Input Vsync source select from tv-decoder */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 16, 3);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		/* Input Vsync source select from hdmi-rx */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 16, 3);
	WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 31, 1);
}
/* won't change this function internal seqence,
 * if really need change,please be carefull */
static void amvecm_vlock_process(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo;
	unsigned int input_hz, output_hz, input_vs_cnt;

	if (vecm_latch_flag & FLAG_VLOCK_DIS) {
		/* VLOCK_CNTL_EN disable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,0,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 0);
		vlock_dis_cnt = vlock_dis_cnt_limit;
		vlock_en = 0;
		vecm_latch_flag &= ~FLAG_VLOCK_DIS;
		return;
	}
	if (vlock_en == 1) {
		vinfo = get_current_vinfo();
		input_hz = amvecm_vlock_check_input_hz(vf);
		output_hz =
			amvecm_vlock_check_output_hz(vinfo->sync_duration_num);
		if ((vinfo->mode != pre_vmode) ||
			(vf->source_type != pre_source_type) ||
			(vf->source_mode != pre_source_mode) ||
			(input_hz != pre_input_freq) ||
			(output_hz != pre_output_freq)) {
			amvecm_vlock_setting(vf, input_hz, output_hz);
			pr_amvecm_dbg("%s:vmode/source_type/source_mode/",
					__func__);
			pr_amvecm_dbg("input_freq/output_freq change:");
			pr_amvecm_dbg("%d/%d/%d/%d/%d=>%d/%d/%d/%d/%d\n",
				pre_vmode, pre_source_type, pre_source_mode,
				pre_input_freq, pre_output_freq, vinfo->mode,
				vf->source_type, vf->source_mode,
				input_hz, output_hz);
			pre_vmode = vinfo->mode;
			pre_source_type = vf->source_type;
			pre_source_mode = vf->source_mode;
			pre_input_freq = input_hz;
			pre_output_freq = output_hz;
			vlock_sync_limit_flag = 0;
		}
		if (vlock_sync_limit_flag < 5) {
			vlock_sync_limit_flag++;
			if (vlock_sync_limit_flag == 5) {
				input_vs_cnt =
				READ_VPP_REG_BITS(VPU_VLOCK_RO_VS_I_DIST,
					0, 28);
				WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MAX,
						input_vs_cnt*103/100);
				WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MIN,
						input_vs_cnt*97/100);
			}
		}
		return;
	}
	if (vlock_dis_cnt > 0) {
		vlock_dis_cnt--;
		if (vlock_dis_cnt == 0) {
			/* disable to adjust pll */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
			/* CFG_VID_LOCK_ADJ_EN disable */
			WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT,
					0, 13, 1);
			/* disable to adjust pll */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
			/* disable vid_lock_en */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 31, 1);
		}
	}
	if (vecm_latch_flag & FLAG_VLOCK_EN) {
		vinfo = get_current_vinfo();
		/* pr_info("[%s]vinfo->name:%s\n",__func__,vinfo->name); */
		input_hz = amvecm_vlock_check_input_hz(vf);
		output_hz =
			amvecm_vlock_check_output_hz(vinfo->sync_duration_num);
		amvecm_vlock_setting(vf, input_hz, output_hz);
		pr_amvecm_dbg("%s:current vmode/source_type/source_mode/",
				__func__);
		pr_amvecm_dbg("input_freq/output_freq/sig_fmt is:");
		pr_amvecm_dbg("%d/%d/%d/%d/%d/0x%x\n",
				vinfo->mode, vf->source_type,
				vf->source_mode, input_hz,
				output_hz, vf->sig_fmt);
		vinfo = get_current_vinfo();
		pre_vmode = vinfo->mode;
		pre_source_type = vf->source_type;
		pre_source_mode = vf->source_mode;
		pre_input_freq = input_hz;
		pre_output_freq = output_hz;
		vlock_en = 1;
		vlock_sync_limit_flag = 0;
		vecm_latch_flag &= ~FLAG_VLOCK_EN;
	}
}

static void amvecm_3d_black_process(void)
{
	if (vecm_latch_flag & FLAG_3D_BLACK_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 0, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 0, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_BLACK_EN) {
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 1, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 1, 31, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				sync_3d_black_color&0xffffff, 0, 24);
		if (sync_3d_sync_to_vbo)
			WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_EN;
	}
}
static void amvecm_3d_sync_process(void)
{

	if (vecm_latch_flag & FLAG_3D_SYNC_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_SYNC_EN) {
		/*select vpu pwm source clock*/
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 0, 29, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 1, 29, 2);
			break;
		case 2:/* ENCP */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 2, 29, 2);
			break;
		case 3:/* ENCT */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 3, 29, 2);
			break;
		default:
			break;
		}
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_EN;
	}
}
static void parse_param_amvecm(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}
static ssize_t amvecm_3d_sync_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int sync_h_start, sync_h_end, sync_v_start,
		sync_v_end, sync_polarity,
		sync_out_inv, sync_en;

	if (!is_meson_g9tv_cpu()) {
		len += sprintf(buf+len,
				"\n chip does not support 3D sync process!!!\n");
		return len;
	}

	sync_h_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 13);
	sync_h_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 16, 13);
	sync_v_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13);
	sync_v_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 16, 13);
	sync_polarity = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 29, 1);
	sync_out_inv = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 15, 1);
	sync_en = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 31, 1);
	len += sprintf(buf+len, "\n current 3d sync state:\n");
	len += sprintf(buf+len, "sync_h_start:%d\n", sync_h_start);
	len += sprintf(buf+len, "sync_h_end:%d\n", sync_h_end);
	len += sprintf(buf+len, "sync_v_start:%d\n", sync_v_start);
	len += sprintf(buf+len, "sync_v_end:%d\n", sync_v_end);
	len += sprintf(buf+len, "sync_polarity:%d\n", sync_polarity);
	len += sprintf(buf+len, "sync_out_inv:%d\n", sync_out_inv);
	len += sprintf(buf+len, "sync_en:%d\n", sync_en);
	len += sprintf(buf+len,
			"echo hstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo hend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo vstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo vend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo pola val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo inv val(D) > /sys/class/amvecm/sync_3d\n");
	return len;
}

static ssize_t amvecm_3d_sync_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	if (!is_meson_g9tv_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "hstart", 6)) {
		sync_3d_h_start = (simple_strtol(parm[1], NULL, 10))&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
	} else if (!strncmp(parm[0], "hend", 4)) {
		sync_3d_h_end = (simple_strtol(parm[1], NULL, 10))&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
	} else if (!strncmp(parm[0], "vstart", 6)) {
		sync_3d_v_start = (simple_strtol(parm[1], NULL, 10))&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
	} else if (!strncmp(parm[0], "vend", 4)) {
		sync_3d_v_end = (simple_strtol(parm[1], NULL, 10))&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
	} else if (!strncmp(parm[0], "pola", 4)) {
		sync_3d_polarity = (simple_strtol(parm[1], NULL, 10))&0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
	} else if (!strncmp(parm[0], "inv", 3)) {
		sync_3d_out_inv = (simple_strtol(parm[1], NULL, 10))&0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
	}
	kfree(buf_orig);
	return count;
}

/* #endif */

void amvecm_video_latch(struct vframe_s *vf)
{
	/* if (pq_load_en == 0) */
	/* return; */
	cm_latch_process();
	amvecm_size_patch();
	ve_dnlp_latch_process();
	ve_lcd_gamma_process();
	amvecm_bricon_process();
	lvds_freq_process();
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_g9tv_cpu()) {
		amvecm_vlock_process(vf);
		amvecm_3d_sync_process();
		amvecm_3d_black_process();
	}
/* #endif */
}
void amvecm_on_vs(struct vframe_s *vf)
{
	amvecm_video_latch(vf);
	ve_on_vs(vf);
}
EXPORT_SYMBOL(amvecm_on_vs);

static int amvecm_open(struct inode *inode, struct file *file)
{
	struct amvecm_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvecm_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int amvecm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
static struct am_regs_s amregs_ext;

static long amvecm_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;

	pr_amvecm_dbg("[amvecm..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	switch (cmd) {
	case AMVECM_IOC_LOAD_REG:
		if (pq_load_en == 0) {
			ret = -EBUSY;
			pr_amvecm_dbg("[amvecm..] pq ioctl function disabled !!\n");
			return ret;
		}
		if ((vecm_latch_flag & FLAG_REG_MAP0) &&
			(vecm_latch_flag & FLAG_REG_MAP1) &&
			(vecm_latch_flag & FLAG_REG_MAP2) &&
			(vecm_latch_flag & FLAG_REG_MAP3) &&
			(vecm_latch_flag & FLAG_REG_MAP4) &&
			(vecm_latch_flag & FLAG_REG_MAP5)) {
			ret = -EBUSY;
			pr_amvecm_dbg("load regs error: loading regs, please wait\n");
			break;
		}
		if (copy_from_user(&amregs_ext,
				(void __user *)arg, sizeof(struct am_regs_s))) {
			pr_amvecm_dbg("0x%x load reg errors: can't get buffer lenght\n",
					FLAG_REG_MAP0);
			ret = -EFAULT;
		} else
			ret = cm_load_reg(&amregs_ext);
		break;
	case AMVECM_IOC_VE_DNLP_EN:
		vecm_latch_flag |= FLAG_VE_DNLP_EN;
		break;
	case AMVECM_IOC_VE_DNLP_DIS:
		vecm_latch_flag |= FLAG_VE_DNLP_DIS;
		break;
	case AMVECM_IOC_VE_DNLP:
		if (copy_from_user(&am_ve_dnlp,
				(void __user *)arg,
				sizeof(struct ve_dnlp_s)))
			ret = -EFAULT;
		else
			ve_dnlp_param_update();
		break;
	case AMVECM_IOC_VE_NEW_DNLP:
		if (copy_from_user(&am_ve_new_dnlp,
				(void __user *)arg,
				sizeof(struct ve_dnlp_table_s)))
			ret = -EFAULT;
		else
			ve_new_dnlp_param_update();
		break;
	case AMVECM_IOC_G_HIST_AVG:
		argp = (void __user *)arg;
		if ((video_ve_hist.height == 0) || (video_ve_hist.width == 0))
			ret = -EFAULT;
		else {
			video_ve_hist.ave =
				video_ve_hist.sum/(video_ve_hist.height*
						video_ve_hist.width);
			if (copy_to_user(argp,
					&video_ve_hist,
					sizeof(struct ve_hist_s)))
				ret = -EFAULT;
		}
		break;
	/**********************************************************************
	gamma ioctl
	**********************************************************************/
	case AMVECM_IOC_GAMMA_TABLE_EN:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_r,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		break;
	case AMVECM_IOC_GAMMA_TABLE_G:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_g,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		break;
	case AMVECM_IOC_GAMMA_TABLE_B:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_b,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		break;
	case AMVECM_IOC_S_RGB_OGO:
		if (!wb_en)
			return -EINVAL;

		if (copy_from_user(&video_rgb_ogo,
				(void __user *)arg,
				sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;
		else
			ve_ogo_param_update();
		break;
	case AMVECM_IOC_G_RGB_OGO:
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
				&video_rgb_ogo, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t amvecm_dnlp_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n",
			(am_ve_dnlp.en << 28) | (am_ve_dnlp.rt << 24) |
			(am_ve_dnlp.rl << 16) | (am_ve_dnlp.black << 8) |
			(am_ve_dnlp.white << 0));
}
/* [   28] en    0~1 */
/* [27:20] rt    0~16 */
/* [19:16] rl-1  0~15 */
/* [15: 8] black 0~16 */
/* [ 7: 0] white 0~16 */
static ssize_t amvecm_dnlp_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	s32 val;

	r = sscanf(buf, "0x%x", &val);
	if ((r != 1) || (vecm_latch_flag & FLAG_VE_DNLP))
		return -EINVAL;
	am_ve_dnlp.en    = (val & 0xf0000000) >> 28;
	am_ve_dnlp.rt    =  (val & 0x0f000000) >> 24;
	am_ve_dnlp.rl    = (val & 0x00ff0000) >> 16;
	am_ve_dnlp.black =  (val & 0x0000ff00) >>  8;
	am_ve_dnlp.white = (val & 0x000000ff) >>  0;
	if (am_ve_dnlp.en >  1)
		am_ve_dnlp.en    =  1;
	if (am_ve_dnlp.rl > 64)
		am_ve_dnlp.rl    = 64;
	if (am_ve_dnlp.black > 16)
		am_ve_dnlp.black = 16;
	if (am_ve_dnlp.white > 16)
		am_ve_dnlp.white = 16;
	vecm_latch_flag |= FLAG_VE_DNLP;
	return count;
}

static ssize_t amvecm_brightness_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_brightness);
}

static ssize_t amvecm_brightness_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -1024) || (val > 1024))
		return -EINVAL;

	vd1_brightness = val;
	vecm_latch_flag |= FLAG_BRI_CON;
	return count;
}

static ssize_t amvecm_contrast_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_contrast);
}

static ssize_t amvecm_contrast_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -1024) || (val > 1024))
		return -EINVAL;

	vd1_contrast = val;
	vecm_latch_flag |= FLAG_BRI_CON;
	return count;
}

static ssize_t amvecm_saturation_hue_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", READ_VPP_REG(VPP_VADJ1_MA_MB));
}

static ssize_t amvecm_saturation_hue_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	size_t r;
	s32 mab = 0;
	s16 mc = 0, md = 0;

	r = sscanf(buf, "0x%x", &mab);
	if ((r != 1) || (mab&0xfc00fc00))
		return -EINVAL;

	WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc <  -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =  ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	pr_amvecm_dbg("%s set video_saturation_hue OK!!!\n", __func__);
	return count;
}

static int parse_para_pq(const char *para, int para_num, int *result)
{
	char *endp;
	const char *startp = para;
	int *out = result;
	int len = 0, count = 0;

	if (!startp)
		return 0;
	len = strlen(startp);
	do {
		/* filter space out */
		while (startp && (isspace(*startp) ||
				!isgraph(*startp)) && len) {
			startp++;
			len--;
		}
		if (len == 0)
			break;

		*out++ = simple_strtol(startp, &endp, 0);
		len -= endp - startp;
		startp = endp;
		count++;
	} while ((endp) && (count < para_num) && (len > 0));
	return count;
}

static ssize_t amvecm_saturation_hue_pre_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_pre, hue_pre);
}

static ssize_t amvecm_saturation_hue_pre_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int parsed[2];
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
			/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245,
		/*13~25*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209
	};
	int hue_sin[] = {
		/*-25~-13*/
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,
		 -44,  -38,  -31,  -25, -19, -13,  -6,      /*-12~-1*/
		0,  /*0*/
		 /*1~12*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,  62,
		68,  74,   80,   86,   92,	98,  104,  109,  115,  121,
		126,  132, 137, 142, 147 /*13~25*/
	};
	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if ((parsed[0] < -128) || (parsed[0] > 128) ||
		(parsed[1] < -25) || (parsed[1] > 25)) {
		return -EINVAL;
	}
	saturation_pre = parsed[0];
	hue_pre = parsed[1];
	i = (hue_pre > 0) ? hue_pre : -hue_pre;
	ma = (hue_cos[i]*(saturation_pre + 128)) >> 7;
	mb = (hue_sin[25+hue_pre]*(saturation_pre + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_pre:%d hue_pre:%d mab:%x\n",
			saturation_pre, hue_pre, mab);
	WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =	ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	return count;
}

static ssize_t amvecm_saturation_hue_post_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_post, hue_post);
}

static ssize_t amvecm_saturation_hue_post_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int parsed[2];
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
		/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250,
		248, 247, 245, 243, 241, 239, 237, 234, 231, 229,
		226, 223, 220, 216, 213, 209  /*13~25*/
	};
	int hue_sin[] = {
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		-98, -92, -86, -80, /*-25~-13*/-74,  -68,  -62,  -56,
		-50,  -44,  -38,  -31,  -25, -19, -13,  -6, /*-12~-1*/
		0, /*0*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,
		62,	68,  74,      /*1~12*/	80,   86,   92,	98,  104,
		109,  115,  121,  126,  132, 137, 142, 147 /*13~25*/
	};
	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if ((parsed[0] < -128) ||
		(parsed[0] > 128) ||
		(parsed[1] < -25) ||
		(parsed[1] > 25)) {
		return -EINVAL;
	}
	saturation_post = parsed[0];
	hue_post = parsed[1];
	i = (hue_post > 0) ? hue_post : -hue_post;
	ma = (hue_cos[i]*(saturation_post + 128)) >> 7;
	mb = (hue_sin[25+hue_post]*(saturation_post + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_post:%d hue_post:%d mab:%x\n",
			saturation_post, hue_post, mab);
	WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =	ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ2_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	return count;
}

static ssize_t amvecm_cm2_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info(" echo wm addr data0 data1 data2 data3 data4 ");
	pr_info("> /sys/class/amvecm/cm2\n");
	pr_info(" echo rm addr > /sys/class/amvecm/cm2\n");
	return 0;
}

static ssize_t amvecm_cm2_store(struct class *cls,
		 struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr;
	int data[5] = {0};
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		addr = simple_strtol(parm[1], NULL, 16);
		addr = addr - addr%8;
		data[0] = simple_strtol(parm[2], NULL, 16);
		data[1] = simple_strtol(parm[3], NULL, 16);
		data[2] = simple_strtol(parm[4], NULL, 16);
		data[3] = simple_strtol(parm[5], NULL, 16);
		data[4] = simple_strtol(parm[6], NULL, 16);
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[0]);
		WRITE_VPP_REG(addr_port, addr + 1);
		WRITE_VPP_REG(data_port, data[1]);
		WRITE_VPP_REG(addr_port, addr + 2);
		WRITE_VPP_REG(data_port, data[2]);
		WRITE_VPP_REG(addr_port, addr + 3);
		WRITE_VPP_REG(data_port, data[3]);
		WRITE_VPP_REG(addr_port, addr + 4);
		WRITE_VPP_REG(data_port, data[4]);
		pr_info("wm: [0x%x] <-- 0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		addr = simple_strtol(parm[1], NULL, 16);
		addr = addr - addr%8;
		WRITE_VPP_REG(addr_port, addr);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+1);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+2);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+3);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+4);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		pr_info("rm:[0x%x]-->[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
				addr, data[0], data[1],
				data[2], data[3], data[4]);
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/bit");
	}
	kfree(buf_orig);
	return count;
}


static ssize_t amvecm_gamma_show(struct class *cls,
			struct class_attribute *attr,
			char *buf)
{
	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:");
	pr_info("	if the string xxx......xx is less than 256*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 256*3, ");
	pr_info("	then the remaining will be ignored\n");
	return 0;
}

static ssize_t amvecm_gamma_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{

	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned short *gammaR, *gammaG, *gammaB;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;

	/* to avoid the bellow warning message while compiling:
	 * warning: the frame size of 1576 bytes is larger than 1024 bytes
	 */
	gammaR = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	gammaG = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	gammaB = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((parm[0][0] == 's') && (parm[0][1] == 'g')) {
		memset(gammaR, 0, 256 * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > 256)
			gamma_count = 256;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			gammaR[i] = simple_strtol(gamma, NULL, 16);
		}

		switch (parm[0][2]) {
		case 'r':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_R);
			break;

		case 'g':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_G);
			break;

		case 'b':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_B);
			break;
		default:
			break;
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");

	}
	kfree(buf_orig);
	kfree(gammaR);
	kfree(gammaG);
	kfree(gammaB);
	return count;
}

/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
/* void init_sharpness(void) */
/* { */
/* WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1,1,1); */
/* WRITE_VPP_REG(NR_GAUSSIAN_MODE, 0x0); */
/* WRITE_VPP_REG(PK_HVCON_LPF_MODE, 0x11111111); */
/* WRITE_VPP_REG(PK_CON_2CIRHPGAIN_LIMIT, 0x05600500); */
/* WRITE_VPP_REG(PK_CON_2CIRBPGAIN_LIMIT, 0x05280500); */
/* WRITE_VPP_REG(PK_CON_2DRTHPGAIN_LIMIT, 0x05600500); */
/* WRITE_VPP_REG(PK_CON_2DRTBPGAIN_LIMIT, 0x05280500); */
/*  */
/* WRITE_VPP_REG(PK_CIRFB_BLEND_GAIN, 0x8f808f80); */
/* WRITE_VPP_REG(NR_ALP0_MIN_MAX, 0x003f003f); */
/* WRITE_VPP_REG(PK_ALP2_MIERR_CORING, 0x00010101); */
/* WRITE_VPP_REG(PK_ALP2_ERR2CURV_TH_RATE, 0x50504010); */
/* WRITE_VPP_REG(PK_FINALGAIN_HP_BP, 0x00002820); */
/* WRITE_VPP_REG(PK_OS_STATIC, 0x22014014); */
/* WRITE_VPP_REG(PK_DRT_SAD_MISC, 0x18180418); */
/* WRITE_VPP_REG(NR_TI_DNLP_BLEND, 0x00000406); */
/* WRITE_VPP_REG(LTI_CTI_DF_GAIN, 0x18181818); */
/* WRITE_VPP_REG(LTI_CTI_DIR_AC_DBG, 0x57ff0000); */
/* WRITE_VPP_REG(HCTI_FLT_CLP_DC, 0x1a555310); */
/* WRITE_VPP_REG(HCTI_BST_CORE, 0x05050503); */
/* WRITE_VPP_REG(HCTI_CON_2_GAIN_0, 0x28193c00); */
/* WRITE_VPP_REG(HLTI_FLT_CLP_DC, 0x19552104); */
/* WRITE_VPP_REG(HLTI_BST_GAIN, 0x20201c0c); */
/* WRITE_VPP_REG(HLTI_CON_2_GAIN_0, 0x24193c5a); */
/* WRITE_VPP_REG(VLTI_FLT_CON_CLP, 0x00006a90); */
/* WRITE_VPP_REG(VLTI_CON_2_GAIN_0, 0x193c0560); */
/* WRITE_VPP_REG(VCTI_FLT_CON_CLP, 0x00006a90); */
/* WRITE_VPP_REG(VCTI_BST_GAIN, 0x00101010); */
/* WRITE_VPP_REG(VCTI_BST_CORE, 0x00050503); */
/* WRITE_VPP_REG(PK_CIRFB_BP_CORING, 0x00043f04); */
/* WRITE_VPP_REG(PK_DRTFB_HP_CORING, 0x00043f04); */
/* WRITE_VPP_REG(SHARP_HVBLANK_NUM, 0x00003c3c); */
/* pr_info("**********sharpness init ok!*********\n"); */
/* } */
/* #endif */

static struct class_attribute amvecm_class_attrs[] = {
	__ATTR(dnlp, S_IRUGO | S_IWUSR,
		amvecm_dnlp_show, amvecm_dnlp_store),
	__ATTR(brightness, S_IRUGO | S_IWUSR,
		amvecm_brightness_show, amvecm_brightness_store),
	__ATTR(contrast, S_IRUGO | S_IWUSR,
		amvecm_contrast_show, amvecm_contrast_store),
	__ATTR(saturation_hue, S_IRUGO | S_IWUSR,
		amvecm_saturation_hue_show,
		amvecm_saturation_hue_store),
	__ATTR(saturation_hue_pre, S_IRUGO | S_IWUSR,
		amvecm_saturation_hue_pre_show,
		amvecm_saturation_hue_pre_store),
	__ATTR(saturation_hue_post, S_IRUGO | S_IWUSR,
		amvecm_saturation_hue_post_show,
		amvecm_saturation_hue_post_store),
	__ATTR(cm2, S_IRUGO | S_IWUSR,
		amvecm_cm2_show,
		amvecm_cm2_store),
	__ATTR(gamma, S_IRUGO | S_IWUSR,
		amvecm_gamma_show,
		amvecm_gamma_store),
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	__ATTR(sync_3d, S_IRUGO | S_IWUSR,
		amvecm_3d_sync_show,
		amvecm_3d_sync_store),
/* #endif */
	__ATTR_NULL
};

static const struct file_operations amvecm_fops = {
	.owner   = THIS_MODULE,
	.open    = amvecm_open,
	.release = amvecm_release,
	.unlocked_ioctl   = amvecm_ioctl,
};

static int __init amvecm_init(void)
{
	int ret = 0;
	int i = 0;
	struct amvecm_dev_s *devp = &amvecm_dev;

	memset(devp, 0, (sizeof(struct amvecm_dev_s)));
	pr_info("\n\n VECM init\n\n");
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVECM_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE, AMVECM_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp, &amvecm_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amvecm_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMVECM_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}
	/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
	/* if (is_meson_g9tv_cpu()) */
	/* init_sharpness(); */
	/* #endif */
	return 0;

fail_create_device:
	pr_info("[amvecm.] : amvecm device create error.\n");
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_info("[amvecm.] : amvecm add device error.\n");
	kfree(devp);
fail_class_create_file:
	pr_info("[amvecm.] : amvecm class create file error.\n");
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
		&amvecm_class_attrs[i]);
	}
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amvecm.] : amvecm class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amvecm.] : amvecm alloc error.\n");
	pr_info("[amvecm.] : amvecm_init.\n");
	return ret;
}

static void __exit amvecm_exit(void)
{
	struct amvecm_dev_s *devp = &amvecm_dev;
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	kfree(devp);
	pr_info("[amvecm.] : amvecm_exit.\n");
}

module_init(amvecm_init);
module_exit(amvecm_exit);

MODULE_DESCRIPTION("AMLOGIC amvecm driver");
MODULE_LICENSE("GPL");

