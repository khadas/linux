// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/gki_module.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"

struct dummy_venc_data_s {
	unsigned int vid_clk_ctrl_reg;
	unsigned int vid_clk_ctrl2_reg;
	unsigned int vid_clk_div_reg;
	unsigned int vid2_clk_ctrl_reg;
	unsigned int vid2_clk_div_reg;

	void (*clktree_probe)(struct device *dev);
	void (*clktree_remove)(struct device *dev);
	void (*encp_clk_gate_switch)(int flag);
	void (*enci_clk_gate_switch)(int flag);
	void (*encl_clk_gate_switch)(int flag);
};

static struct dummy_venc_data_s *dummy_venc_data;

struct dummy_venc_driver_s {
	struct device *dev;
	unsigned char status;
	unsigned char viu_sel;

	unsigned char clk_gate_state;

	struct clk *top_gate;
	struct clk *int_gate0;
	struct clk *int_gate1;
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *vpu_dev;
#endif
};

static struct dummy_venc_driver_s *dummy_encp_drv;
static struct dummy_venc_driver_s *dummy_enci_drv;
static struct dummy_venc_driver_s *dummy_encl_drv;

#define DUMMY_L_NAME    "dummy_encl"
#define DUMMY_I_NAME    "dummy_enci"
#define DUMMY_P_NAME    "dummy_encp"

/* **********************************************************
 * dummy_encp support
 * **********************************************************
 */
static unsigned int dummy_encp_index = 0xff;
static struct vinfo_s dummy_encp_vinfo[] = {
	{
		.name              = "dummy_panel",
		.mode              = VMODE_DUMMY_ENCP,
		.frac              = 0,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_RGB444,
		.viu_mux           = VIU_MUX_ENCP,
		.vout_device       = NULL,
	},
	{
		.name              = "dummy_p",
		.mode              = VMODE_DUMMY_ENCP,
		.frac              = 0,
		.width             = 720,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 25000000,
		.htotal            = 952,
		.vtotal            = 525,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCP,
		.vout_device       = NULL,
	}
};

static void dummy_encp_venc_set(void)
{
	unsigned int temp;

	VOUTPR("%s: dummy_encp_index=%d\n", __func__, dummy_encp_index);

	if (dummy_encp_index == 0) {
		vout_vcbus_write(ENCP_VIDEO_EN, 0);

		vout_vcbus_write(ENCP_VIDEO_MODE, 0x8000);
		vout_vcbus_write(ENCP_VIDEO_MODE_ADV, 0x0418);

		temp = vout_vcbus_read(ENCL_VIDEO_MAX_PXCNT);
		vout_vcbus_write(ENCP_VIDEO_MAX_PXCNT, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_MAX_LNCNT);
		vout_vcbus_write(ENCP_VIDEO_MAX_LNCNT, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_HAVON_BEGIN);
		vout_vcbus_write(ENCP_VIDEO_HAVON_BEGIN, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_HAVON_END);
		vout_vcbus_write(ENCP_VIDEO_HAVON_END, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VAVON_BLINE);
		vout_vcbus_write(ENCP_VIDEO_VAVON_BLINE, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VAVON_ELINE);
		vout_vcbus_write(ENCP_VIDEO_VAVON_ELINE, temp);

		temp = vout_vcbus_read(ENCL_VIDEO_HSO_BEGIN);
		vout_vcbus_write(ENCP_VIDEO_HSO_BEGIN, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_HSO_END);
		vout_vcbus_write(ENCP_VIDEO_HSO_END,   temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_BEGIN);
		vout_vcbus_write(ENCP_VIDEO_VSO_BEGIN, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_END);
		vout_vcbus_write(ENCP_VIDEO_VSO_END,   temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_BLINE);
		vout_vcbus_write(ENCP_VIDEO_VSO_BLINE, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_ELINE);
		vout_vcbus_write(ENCP_VIDEO_VSO_ELINE, temp);

		temp = vout_vcbus_read(ENCL_VIDEO_VSO_ELINE);
		vout_vcbus_write(ENCP_VIDEO_RGBIN_CTRL, temp);

		vout_vcbus_write(ENCP_VIDEO_EN, 1);
	} else {
		vout_vcbus_write(ENCP_VIDEO_EN, 0);

		vout_vcbus_write(ENCP_VIDEO_MODE, 0x8000);
		vout_vcbus_write(ENCP_VIDEO_MODE_ADV, 0x0418);

		vout_vcbus_write(ENCP_VIDEO_MAX_PXCNT, 951);
		vout_vcbus_write(ENCP_VIDEO_MAX_LNCNT, 524);
		vout_vcbus_write(ENCP_VIDEO_HAVON_BEGIN, 80);
		vout_vcbus_write(ENCP_VIDEO_HAVON_END, 799);
		vout_vcbus_write(ENCP_VIDEO_VAVON_BLINE, 22);
		vout_vcbus_write(ENCP_VIDEO_VAVON_ELINE, 501);

		vout_vcbus_write(ENCP_VIDEO_HSO_BEGIN, 0);
		vout_vcbus_write(ENCP_VIDEO_HSO_END,   20);
		vout_vcbus_write(ENCP_VIDEO_VSO_BEGIN, 0);
		vout_vcbus_write(ENCP_VIDEO_VSO_END,   0);
		vout_vcbus_write(ENCP_VIDEO_VSO_BLINE, 0);
		vout_vcbus_write(ENCP_VIDEO_VSO_ELINE, 5);

		vout_vcbus_write(ENCP_VIDEO_RGBIN_CTRL, 1);

		vout_vcbus_write(ENCP_VIDEO_EN, 1);
	}
}

static void dummy_encp_clk_ctrl(int flag)
{
	unsigned int temp;

	if (!dummy_venc_data)
		return;

	if (dummy_encp_index == 0) {
		if (flag) {
			temp = vout_clk_getb(dummy_venc_data->vid2_clk_div_reg,
					     ENCL_CLK_SEL, 4);
			vout_clk_setb(dummy_venc_data->vid_clk_div_reg,
				      temp, ENCP_CLK_SEL, 4);

			vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
				      1, ENCP_GATE_VCLK, 1);
		} else {
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
				      0, ENCP_GATE_VCLK, 1);
		}
	} else {
		if (flag) {
			/* clk source sel: fckl_div5 */
			vout_clk_setb(dummy_venc_data->vid_clk_div_reg,
				      0xf, VCLK_XD0, 8);
			usleep_range(5, 6);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      6, VCLK_CLK_IN_SEL, 3);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      1, VCLK_EN0, 1);
			usleep_range(5, 6);
			vout_clk_setb(dummy_venc_data->vid_clk_div_reg,
				      0, ENCP_CLK_SEL, 4);
			vout_clk_setb(dummy_venc_data->vid_clk_div_reg,
				      1, VCLK_XD_EN, 1);
			usleep_range(5, 6);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      1, VCLK_DIV1_EN, 1);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      1, VCLK_SOFT_RST, 1);
			usleep_range(10, 11);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      0, VCLK_SOFT_RST, 1);
			usleep_range(5, 6);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
				      1, ENCP_GATE_VCLK, 1);
		} else {
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
				      0, ENCP_GATE_VCLK, 1);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      0, VCLK_DIV1_EN, 1);
			vout_clk_setb(dummy_venc_data->vid_clk_ctrl_reg,
				      0, VCLK_EN0, 1);
			vout_clk_setb(dummy_venc_data->vid_clk_div_reg,
				      0, VCLK_XD_EN, 1);
		}
	}
}

static void dummy_encp_clk_gate_switch(int flag)
{
	int ret = 0;

	if (flag) {
		if (dummy_encp_drv->clk_gate_state)
			return;
		if (IS_ERR_OR_NULL(dummy_encp_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_prepare_enable(dummy_encp_drv->top_gate);
		if (IS_ERR_OR_NULL(dummy_encp_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_prepare_enable(dummy_encp_drv->int_gate0);
		if (IS_ERR_OR_NULL(dummy_encp_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_prepare_enable(dummy_encp_drv->int_gate1);
		dummy_encp_drv->clk_gate_state = 1;
	} else {
		if (dummy_encp_drv->clk_gate_state == 0)
			return;
		dummy_encp_drv->clk_gate_state = 0;
		if (IS_ERR_OR_NULL(dummy_encp_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_disable_unprepare(dummy_encp_drv->int_gate0);
		if (IS_ERR_OR_NULL(dummy_encp_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_disable_unprepare(dummy_encp_drv->int_gate1);
		if (IS_ERR_OR_NULL(dummy_encp_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_disable_unprepare(dummy_encp_drv->top_gate);
	}

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static void dummy_encp_vinfo_update(void)
{
	unsigned int lcd_viu_sel = 0;
	const struct vinfo_s *vinfo = NULL;

	/* only dummy_panel need update vinfo */
	if (dummy_encp_index != 0)
		return;

	if (dummy_encp_drv->viu_sel == 1) {
		vinfo = get_current_vinfo2();
		lcd_viu_sel = 2;
	} else if (dummy_encp_drv->viu_sel == 2) {
		vinfo = get_current_vinfo();
		lcd_viu_sel = 1;
	}

	if (vinfo) {
		if (vinfo->mode != VMODE_LCD) {
			VOUTERR("display%d is not panel\n", lcd_viu_sel);
			vinfo = NULL;
		}
	}
	if (!vinfo)
		return;

	dummy_encp_vinfo[0].width = vinfo->width;
	dummy_encp_vinfo[0].height = vinfo->height;
	dummy_encp_vinfo[0].field_height = vinfo->field_height;
	dummy_encp_vinfo[0].aspect_ratio_num = vinfo->aspect_ratio_num;
	dummy_encp_vinfo[0].aspect_ratio_den = vinfo->aspect_ratio_den;
	dummy_encp_vinfo[0].sync_duration_num = vinfo->sync_duration_num;
	dummy_encp_vinfo[0].sync_duration_den = vinfo->sync_duration_den;
	dummy_encp_vinfo[0].video_clk = vinfo->video_clk;
	dummy_encp_vinfo[0].htotal = vinfo->htotal;
	dummy_encp_vinfo[0].vtotal = vinfo->vtotal;
	dummy_encp_vinfo[0].viu_color_fmt = vinfo->viu_color_fmt;
}

static struct vinfo_s *dummy_encp_get_current_info(void)
{
	if (dummy_encp_index > 1)
		return NULL;

	return &dummy_encp_vinfo[dummy_encp_index];
}

static int dummy_encp_set_current_vmode(enum vmode_e mode)
{
	if (dummy_encp_index > 1)
		return -1;

	dummy_encp_vinfo_update();

#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(dummy_encp_drv->vpu_dev,
			    dummy_encp_vinfo[dummy_encp_index].video_clk);
	vpu_dev_mem_power_on(dummy_encp_drv->vpu_dev);
#endif
	if (dummy_venc_data && dummy_venc_data->encp_clk_gate_switch)
		dummy_venc_data->encp_clk_gate_switch(1);

	dummy_encp_clk_ctrl(1);
	dummy_encp_venc_set();

	dummy_encp_drv->status = 1;
	VOUTPR("%s finished\n", __func__);

	return 0;
}

static enum vmode_e dummy_encp_validate_vmode(char *name, unsigned int frac)
{
	enum vmode_e vmode = VMODE_MAX;
	int i;

	if (frac)
		return VMODE_MAX;

	for (i = 0; i < 2; i++) {
		if (strcmp(dummy_encp_vinfo[i].name, name) == 0) {
			vmode = dummy_encp_vinfo[i].mode;
			dummy_encp_index = i;
			break;
		}
	}

	return vmode;
}

static int dummy_encp_vmode_is_supported(enum vmode_e mode)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DUMMY_ENCP)
		return true;

	return false;
}

static int dummy_encp_disable(enum vmode_e cur_vmod)
{
	dummy_encp_drv->status = 0;
	dummy_encp_index = 0xff;

	vout_vcbus_write(ENCP_VIDEO_EN, 0); /* disable encp */
	dummy_encp_clk_ctrl(0);
	if (dummy_venc_data && dummy_venc_data->encp_clk_gate_switch)
		dummy_venc_data->encp_clk_gate_switch(0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(dummy_encp_drv->vpu_dev);
	vpu_dev_clk_release(dummy_encp_drv->vpu_dev);
#endif

	VOUTPR("%s finished\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int dummy_encp_lcd_suspend(void)
{
	dummy_encp_drv->status = 0;

	vout_vcbus_write(ENCP_VIDEO_EN, 0); /* disable encp */
	dummy_encp_clk_ctrl(0);
	if (dummy_venc_data && dummy_venc_data->encp_clk_gate_switch)
		dummy_venc_data->encp_clk_gate_switch(0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(dummy_encp_drv->vpu_dev);
	vpu_dev_clk_release(dummy_encp_drv->vpu_dev);
#endif

	return 0;
}

static int dummy_encp_lcd_resume(void)
{
	dummy_encp_set_current_vmode(VMODE_DUMMY_ENCP);
	return 0;
}
#endif

static int dummy_encp_vout_state;
static int dummy_encp_vout_set_state(int bit)
{
	dummy_encp_vout_state |= (1 << bit);
	dummy_encp_drv->viu_sel = bit;
	return 0;
}

static int dummy_encp_vout_clr_state(int bit)
{
	dummy_encp_vout_state &= ~(1 << bit);
	if (dummy_encp_drv->viu_sel == bit)
		dummy_encp_drv->viu_sel = 0;
	return 0;
}

static int dummy_encp_vout_get_state(void)
{
	return dummy_encp_vout_state;
}

static struct vout_server_s dummy_encp_vout_server = {
	.name = "dummy_encp_vout_server",
	.op = {
		.get_vinfo          = dummy_encp_get_current_info,
		.set_vmode          = dummy_encp_set_current_vmode,
		.validate_vmode     = dummy_encp_validate_vmode,
		.vmode_is_supported = dummy_encp_vmode_is_supported,
		.disable            = dummy_encp_disable,
		.set_state          = dummy_encp_vout_set_state,
		.clr_state          = dummy_encp_vout_clr_state,
		.get_state          = dummy_encp_vout_get_state,
		.set_bist           = NULL,
#ifdef CONFIG_PM
		.vout_suspend       = dummy_encp_lcd_suspend,
		.vout_resume        = dummy_encp_lcd_resume,
#endif
	},
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s dummy_encp_vout2_server = {
	.name = "dummy_encp_vout2_server",
	.op = {
		.get_vinfo          = dummy_encp_get_current_info,
		.set_vmode          = dummy_encp_set_current_vmode,
		.validate_vmode     = dummy_encp_validate_vmode,
		.vmode_is_supported = dummy_encp_vmode_is_supported,
		.disable            = dummy_encp_disable,
		.set_state          = dummy_encp_vout_set_state,
		.clr_state          = dummy_encp_vout_clr_state,
		.get_state          = dummy_encp_vout_get_state,
		.set_bist           = NULL,
#ifdef CONFIG_PM
		.vout_suspend       = dummy_encp_lcd_suspend,
		.vout_resume        = dummy_encp_lcd_resume,
#endif
	},
};
#endif

static void dummy_encp_vout_server_init(void)
{
	vout_register_server(&dummy_encp_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(&dummy_encp_vout2_server);
#endif
}

static void dummy_encp_vout_server_remove(void)
{
	vout_unregister_server(&dummy_encp_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&dummy_encp_vout2_server);
#endif
}

/* **********************************************************
 * dummy_enci support
 * **********************************************************
 */
static struct vinfo_s dummy_enci_vinfo = {
	.name              = "dummy_i",
	.mode              = VMODE_DUMMY_ENCI,
	.frac              = 0,
	.width             = 720,
	.height            = 480,
	.field_height      = 240,
	.aspect_ratio_num  = 4,
	.aspect_ratio_den  = 3,
	.sync_duration_num = 55,
	.sync_duration_den = 1,
	.video_clk         = 25000000,
	.htotal            = 1716,
	.vtotal            = 525,
	.fr_adj_type       = VOUT_FR_ADJ_NONE,
	.viu_color_fmt     = COLOR_FMT_YUV444,
	.viu_mux           = VIU_MUX_ENCI,
	.vout_device       = NULL,
};

static void dummy_enci_venc_set(void)
{
	vout_vcbus_write(ENCI_VIDEO_EN, 0);

	vout_vcbus_write(ENCI_CFILT_CTRL,                 0x12);
	vout_vcbus_write(ENCI_CFILT_CTRL2,                0x12);
	vout_vcbus_write(ENCI_VIDEO_MODE,                 0);
	vout_vcbus_write(ENCI_VIDEO_MODE_ADV,             0);
	vout_vcbus_write(ENCI_SYNC_HSO_BEGIN,             5);
	vout_vcbus_write(ENCI_SYNC_HSO_END,               129);
	vout_vcbus_write(ENCI_SYNC_VSO_EVNLN,             0x0003);
	vout_vcbus_write(ENCI_SYNC_VSO_ODDLN,             0x0104);
	vout_vcbus_write(ENCI_MACV_MAX_AMP,               0x810b);
	vout_vcbus_write(VENC_VIDEO_PROG_MODE,            0xf0);
	vout_vcbus_write(ENCI_VIDEO_MODE,                 0x08);
	vout_vcbus_write(ENCI_VIDEO_MODE_ADV,             0x26);
	vout_vcbus_write(ENCI_VIDEO_SCH,                  0x20);
	vout_vcbus_write(ENCI_SYNC_MODE,                  0x07);
	vout_vcbus_write(ENCI_YC_DELAY,                   0x333);
	vout_vcbus_write(ENCI_VFIFO2VD_PIXEL_START,       0xe3);
	vout_vcbus_write(ENCI_VFIFO2VD_PIXEL_END,         0x0683);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_TOP_START,    0x12);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_TOP_END,      0x102);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_BOT_START,    0x13);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_BOT_END,      0x103);
	vout_vcbus_write(VENC_SYNC_ROUTE,                 0);
	vout_vcbus_write(ENCI_DBG_PX_RST,                 0);
	vout_vcbus_write(VENC_INTCTRL,                    0x2);
	vout_vcbus_write(ENCI_VFIFO2VD_CTL,               0x4e01);
	vout_vcbus_write(VENC_UPSAMPLE_CTRL0,             0x0061);
	vout_vcbus_write(VENC_UPSAMPLE_CTRL1,             0x4061);
	vout_vcbus_write(VENC_UPSAMPLE_CTRL2,             0x5061);
	vout_vcbus_write(ENCI_DACSEL_0,                   0x0011);
	vout_vcbus_write(ENCI_DACSEL_1,                   0x11);
	vout_vcbus_write(ENCI_VIDEO_EN,                   1);
	vout_vcbus_write(ENCI_VIDEO_SAT,                  0x12);
	vout_vcbus_write(ENCI_SYNC_ADJ,                   0x9c00);
	vout_vcbus_write(ENCI_VIDEO_CONT,                 0x3);
}

static void dummy_enci_clk_ctrl(int flag)
{
	if (!dummy_venc_data)
		return;

	if (flag) {
		/* clk source sel: fckl_div5 */
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      0xf, VCLK2_XD, 8);
		usleep_range(5, 6);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      6, VCLK2_CLK_IN_SEL, 3);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      1, VCLK2_EN, 1);
		usleep_range(5, 6);

		vout_clk_setb(dummy_venc_data->vid_clk_div_reg,
			      8, ENCI_CLK_SEL, 4);
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      1, VCLK2_XD_EN, 1);
		usleep_range(5, 6);

		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      1, VCLK2_DIV1_EN, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      1, VCLK2_SOFT_RST, 1);
		usleep_range(10, 11);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      0, VCLK2_SOFT_RST, 1);
		usleep_range(5, 6);
		vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
			      1, ENCI_GATE_VCLK, 1);
	} else {
		vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
			      0, ENCI_GATE_VCLK, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      0, VCLK2_DIV1_EN, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      0, VCLK2_EN, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      0, VCLK2_XD_EN, 1);
	}
}

static void dummy_enci_clk_gate_switch(int flag)
{
	int ret = 0;

	if (flag) {
		if (dummy_enci_drv->clk_gate_state)
			return;
		if (IS_ERR_OR_NULL(dummy_enci_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_prepare_enable(dummy_enci_drv->top_gate);
		if (IS_ERR_OR_NULL(dummy_enci_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_prepare_enable(dummy_enci_drv->int_gate0);
		if (IS_ERR_OR_NULL(dummy_enci_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_prepare_enable(dummy_enci_drv->int_gate1);
		dummy_enci_drv->clk_gate_state = 1;
	} else {
		if (dummy_enci_drv->clk_gate_state == 0)
			return;
		dummy_enci_drv->clk_gate_state = 0;
		if (IS_ERR_OR_NULL(dummy_enci_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_disable_unprepare(dummy_enci_drv->int_gate0);
		if (IS_ERR_OR_NULL(dummy_enci_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_disable_unprepare(dummy_enci_drv->int_gate1);
		if (IS_ERR_OR_NULL(dummy_enci_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_disable_unprepare(dummy_enci_drv->top_gate);
	}

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static struct vinfo_s *dummy_enci_get_current_info(void)
{
	return &dummy_enci_vinfo;
}

static int dummy_enci_set_current_vmode(enum vmode_e mode)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(dummy_enci_drv->vpu_dev,
			    dummy_enci_vinfo.video_clk);
	vpu_dev_mem_power_on(dummy_enci_drv->vpu_dev);
#endif
	if (dummy_venc_data && dummy_venc_data->enci_clk_gate_switch)
		dummy_venc_data->enci_clk_gate_switch(1);

	dummy_enci_clk_ctrl(1);
	dummy_enci_venc_set();

	dummy_enci_drv->status = 1;
	VOUTPR("%s finished\n", __func__);

	return 0;
}

static enum vmode_e dummy_enci_validate_vmode(char *name, unsigned int frac)
{
	enum vmode_e vmode = VMODE_MAX;

	if (frac)
		return VMODE_MAX;

	if (strcmp(dummy_enci_vinfo.name, name) == 0)
		vmode = dummy_enci_vinfo.mode;

	return vmode;
}

static int dummy_enci_vmode_is_supported(enum vmode_e mode)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DUMMY_ENCI)
		return true;

	return false;
}

static int dummy_enci_disable(enum vmode_e cur_vmod)
{
	dummy_enci_drv->status = 0;

	vout_vcbus_write(ENCI_VIDEO_EN, 0); /* disable encp */
	dummy_enci_clk_ctrl(0);
	if (dummy_venc_data && dummy_venc_data->enci_clk_gate_switch)
		dummy_venc_data->enci_clk_gate_switch(0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(dummy_enci_drv->vpu_dev);
	vpu_dev_clk_release(dummy_enci_drv->vpu_dev);
#endif

	VOUTPR("%s finished\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int dummy_enci_lcd_suspend(void)
{
	dummy_enci_disable(VMODE_DUMMY_ENCI);
	return 0;
}

static int dummy_enci_lcd_resume(void)
{
	dummy_enci_set_current_vmode(VMODE_DUMMY_ENCI);
	return 0;
}

#endif

static int dummy_enci_vout_state;
static int dummy_enci_vout_set_state(int bit)
{
	dummy_enci_vout_state |= (1 << bit);
	dummy_enci_drv->viu_sel = bit;
	return 0;
}

static int dummy_enci_vout_clr_state(int bit)
{
	dummy_enci_vout_state &= ~(1 << bit);
	if (dummy_enci_drv->viu_sel == bit)
		dummy_enci_drv->viu_sel = 0;
	return 0;
}

static int dummy_enci_vout_get_state(void)
{
	return dummy_enci_vout_state;
}

static struct vout_server_s dummy_enci_vout_server = {
	.name = "dummy_enci_vout_server",
	.op = {
		.get_vinfo          = dummy_enci_get_current_info,
		.set_vmode          = dummy_enci_set_current_vmode,
		.validate_vmode     = dummy_enci_validate_vmode,
		.vmode_is_supported = dummy_enci_vmode_is_supported,
		.disable            = dummy_enci_disable,
		.set_state          = dummy_enci_vout_set_state,
		.clr_state          = dummy_enci_vout_clr_state,
		.get_state          = dummy_enci_vout_get_state,
		.set_bist           = NULL,
#ifdef CONFIG_PM
		.vout_suspend       = dummy_enci_lcd_suspend,
		.vout_resume        = dummy_enci_lcd_resume,
#endif
	},
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s dummy_enci_vout2_server = {
	.name = "dummy_enci_vout2_server",
	.op = {
		.get_vinfo          = dummy_enci_get_current_info,
		.set_vmode          = dummy_enci_set_current_vmode,
		.validate_vmode     = dummy_enci_validate_vmode,
		.vmode_is_supported = dummy_enci_vmode_is_supported,
		.disable            = dummy_enci_disable,
		.set_state          = dummy_enci_vout_set_state,
		.clr_state          = dummy_enci_vout_clr_state,
		.get_state          = dummy_enci_vout_get_state,
		.set_bist           = NULL,
#ifdef CONFIG_PM
		.vout_suspend       = dummy_enci_lcd_suspend,
		.vout_resume        = dummy_enci_lcd_resume,
#endif
	},
};
#endif

static void dummy_enci_vout_server_init(void)
{
	vout_register_server(&dummy_enci_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(&dummy_enci_vout2_server);
#endif
}

static void dummy_enci_vout_server_remove(void)
{
	vout_unregister_server(&dummy_enci_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&dummy_enci_vout2_server);
#endif
}

/* **********************************************************
 * dummy_encl support
 * **********************************************************
 */
static struct vinfo_s dummy_encl_vinfo = {
	.name              = "dummy_l",
	.mode              = VMODE_DUMMY_ENCL,
	.frac              = 0,
	.width             = 720,
	.height            = 480,
	.field_height      = 480,
	.aspect_ratio_num  = 4,
	.aspect_ratio_den  = 3,
	.sync_duration_num = 50,
	.sync_duration_den = 1,
	.video_clk         = 25000000,
	.htotal            = 952,
	.vtotal            = 525,
	.viu_color_fmt     = COLOR_FMT_RGB444,
	.viu_mux           = VIU_MUX_ENCL,
	.vout_device       = NULL,
};

static void dummy_encl_venc_set(void)
{
	vout_vcbus_write(ENCL_VIDEO_EN, 0);

	vout_vcbus_write(ENCL_VIDEO_MODE, 0x8000);
	vout_vcbus_write(ENCL_VIDEO_MODE_ADV, 0x18);
	vout_vcbus_write(ENCL_VIDEO_FILT_CTRL, 0x1000);

	vout_vcbus_write(ENCL_VIDEO_MAX_PXCNT, 951);
	vout_vcbus_write(ENCL_VIDEO_MAX_LNCNT, 524);
	vout_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, 80);
	vout_vcbus_write(ENCL_VIDEO_HAVON_END, 799);
	vout_vcbus_write(ENCL_VIDEO_VAVON_BLINE, 22);
	vout_vcbus_write(ENCL_VIDEO_VAVON_ELINE, 501);

	vout_vcbus_write(ENCL_VIDEO_HSO_BEGIN, 0);
	vout_vcbus_write(ENCL_VIDEO_HSO_END,   20);
	vout_vcbus_write(ENCL_VIDEO_VSO_BEGIN, 0);
	vout_vcbus_write(ENCL_VIDEO_VSO_END,   0);
	vout_vcbus_write(ENCL_VIDEO_VSO_BLINE, 0);
	vout_vcbus_write(ENCL_VIDEO_VSO_ELINE, 5);

	vout_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);

	vout_vcbus_write(ENCL_VIDEO_EN, 1);
}

static void dummy_encl_clk_ctrl(int flag)
{
	if (!dummy_venc_data)
		return;

	if (flag) {
		/* clk source sel: fckl_div5 */
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      0xf, VCLK2_XD, 8);
		usleep_range(5, 6);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      6, VCLK2_CLK_IN_SEL, 3);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      1, VCLK2_EN, 1);
		usleep_range(5, 6);
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      8, ENCL_CLK_SEL, 4);
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      1, VCLK2_XD_EN, 1);
		usleep_range(5, 6);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      1, VCLK2_DIV1_EN, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      1, VCLK2_SOFT_RST, 1);
		usleep_range(10, 11);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      0, VCLK2_SOFT_RST, 1);
		usleep_range(5, 6);
		vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
			      1, ENCL_GATE_VCLK, 1);
	} else {
		vout_clk_setb(dummy_venc_data->vid_clk_ctrl2_reg,
			      0, ENCL_GATE_VCLK, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      0, VCLK2_DIV1_EN, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_ctrl_reg,
			      0, VCLK2_EN, 1);
		vout_clk_setb(dummy_venc_data->vid2_clk_div_reg,
			      0, VCLK2_XD_EN, 1);
	}
}

static void dummy_encl_clk_gate_switch(int flag)
{
	int ret = 0;

	if (flag) {
		if (dummy_encl_drv->clk_gate_state)
			return;
		if (IS_ERR_OR_NULL(dummy_encl_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_prepare_enable(dummy_encl_drv->top_gate);
		if (IS_ERR_OR_NULL(dummy_encl_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_prepare_enable(dummy_encl_drv->int_gate0);
		dummy_encl_drv->clk_gate_state = 1;
	} else {
		if (dummy_encl_drv->clk_gate_state == 0)
			return;
		dummy_encl_drv->clk_gate_state = 0;
		if (IS_ERR_OR_NULL(dummy_encl_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_disable_unprepare(dummy_encl_drv->int_gate0);
		if (IS_ERR_OR_NULL(dummy_encl_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_disable_unprepare(dummy_encl_drv->top_gate);
	}

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static struct vinfo_s *dummy_encl_get_current_info(void)
{
	return &dummy_encl_vinfo;
}

static int dummy_encl_set_current_vmode(enum vmode_e mode)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(dummy_encl_drv->vpu_dev,
			    dummy_encl_vinfo.video_clk);
	vpu_dev_mem_power_on(dummy_encl_drv->vpu_dev);
#endif
	if (dummy_venc_data && dummy_venc_data->encl_clk_gate_switch)
		dummy_venc_data->encl_clk_gate_switch(1);

	dummy_encl_clk_ctrl(1);
	dummy_encl_venc_set();

	dummy_encl_drv->status = 1;
	VOUTPR("%s finished\n", __func__);

	return 0;
}

static enum vmode_e dummy_encl_validate_vmode(char *name, unsigned int frac)
{
	enum vmode_e vmode = VMODE_MAX;

	if (frac)
		return VMODE_MAX;

	if (strcmp(dummy_encl_vinfo.name, name) == 0)
		vmode = dummy_encl_vinfo.mode;

	return vmode;
}

static int dummy_encl_vmode_is_supported(enum vmode_e mode)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DUMMY_ENCL)
		return true;

	return false;
}

static int dummy_encl_disable(enum vmode_e cur_vmod)
{
	dummy_encl_drv->status = 0;

	vout_vcbus_write(ENCL_VIDEO_EN, 0);
	dummy_encl_clk_ctrl(0);
	if (dummy_venc_data && dummy_venc_data->encl_clk_gate_switch)
		dummy_venc_data->encl_clk_gate_switch(0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(dummy_encl_drv->vpu_dev);
	vpu_dev_clk_release(dummy_encl_drv->vpu_dev);
#endif

	VOUTPR("%s finished\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int dummy_encl_lcd_suspend(void)
{
	dummy_encl_disable(VMODE_DUMMY_ENCL);
	return 0;
}

static int dummy_encl_lcd_resume(void)
{
	dummy_encl_set_current_vmode(VMODE_DUMMY_ENCL);
	return 0;
}

#endif

static int dummy_encl_vout_state;
static int dummy_encl_vout_set_state(int bit)
{
	dummy_encl_vout_state |= (1 << bit);
	dummy_encl_drv->viu_sel = bit;
	return 0;
}

static int dummy_encl_vout_clr_state(int bit)
{
	dummy_encl_vout_state &= ~(1 << bit);
	if (dummy_encl_drv->viu_sel == bit)
		dummy_encl_drv->viu_sel = 0;
	return 0;
}

static int dummy_encl_vout_get_state(void)
{
	return dummy_encl_vout_state;
}

static struct vout_server_s dummy_encl_vout_server = {
	.name = "dummy_encl_vout_server",
	.op = {
		.get_vinfo          = dummy_encl_get_current_info,
		.set_vmode          = dummy_encl_set_current_vmode,
		.validate_vmode     = dummy_encl_validate_vmode,
		.vmode_is_supported = dummy_encl_vmode_is_supported,
		.disable            = dummy_encl_disable,
		.set_state          = dummy_encl_vout_set_state,
		.clr_state          = dummy_encl_vout_clr_state,
		.get_state          = dummy_encl_vout_get_state,
		.set_bist           = NULL,
#ifdef CONFIG_PM
		.vout_suspend       = dummy_encl_lcd_suspend,
		.vout_resume        = dummy_encl_lcd_resume,
#endif
	},
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s dummy_encl_vout2_server = {
	.name = "dummy_encl_vout2_server",
	.op = {
		.get_vinfo          = dummy_encl_get_current_info,
		.set_vmode          = dummy_encl_set_current_vmode,
		.validate_vmode     = dummy_encl_validate_vmode,
		.vmode_is_supported = dummy_encl_vmode_is_supported,
		.disable            = dummy_encl_disable,
		.set_state          = dummy_encl_vout_set_state,
		.clr_state          = dummy_encl_vout_clr_state,
		.get_state          = dummy_encl_vout_get_state,
		.set_bist           = NULL,
#ifdef CONFIG_PM
		.vout_suspend       = dummy_encl_lcd_suspend,
		.vout_resume        = dummy_encl_lcd_resume,
#endif
	},
};
#endif

static void dummy_encl_vout_server_init(void)
{
	vout_register_server(&dummy_encl_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(&dummy_encl_vout2_server);
#endif
}

static void dummy_encl_vout_server_remove(void)
{
	vout_unregister_server(&dummy_encl_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&dummy_encl_vout2_server);
#endif
}

/* ********************************************************* */
static int dummy_venc_vout_mode_update(void)
{
	enum vmode_e mode = VMODE_DUMMY_ENCP;

	VOUTPR("%s\n", __func__);

	if (dummy_encp_drv->viu_sel == 1)
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	else if (dummy_encp_drv->viu_sel == 2)
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	dummy_encp_set_current_vmode(mode);
	if (dummy_encp_drv->viu_sel == 1)
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
	else if (dummy_encp_drv->viu_sel == 2)
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}

static int dummy_encp_vout_notify_callback(struct notifier_block *block,
					   unsigned long cmd, void *para)
{
	const struct vinfo_s *vinfo;

	if (dummy_encp_drv->status == 0)
		return 0;
	if (dummy_encp_index != 0)
		return 0;

	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		vinfo = get_current_vinfo();
		if (!vinfo)
			break;
		if (vinfo->mode != VMODE_LCD)
			break;
		dummy_venc_vout_mode_update();
		break;
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static int dummy_encp_vout2_notify_callback(struct notifier_block *block,
					    unsigned long cmd, void *para)
{
	const struct vinfo_s *vinfo;

	if (dummy_encp_drv->status == 0)
		return 0;
	if (dummy_encp_index != 0)
		return 0;

	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		vinfo = get_current_vinfo2();
		if (!vinfo)
			break;
		if (vinfo->mode != VMODE_LCD)
			break;
		dummy_venc_vout_mode_update();
		break;
	default:
		break;
	}
	return 0;
}
#endif

static struct notifier_block dummy_encp_vout_notifier = {
	.notifier_call = dummy_encp_vout_notify_callback,
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct notifier_block dummy_encp_vout2_notifier = {
	.notifier_call = dummy_encp_vout2_notify_callback,
};
#endif

/* ********************************************************* */
static const char *dummy_venc_debug_usage_str = {
"Usage:\n"
"    echo reg > /sys/class/dummy_venc/encp ; dump regs for encp\n"
"    echo reg > /sys/class/dummy_venc/enci ; dump regs for enci\n"
"    echo reg > /sys/class/dummy_venc/encl ; dump regs for encl\n"
};

static ssize_t dummy_venc_debug_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", dummy_venc_debug_usage_str);
}

static unsigned int dummy_encp_reg_dump[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCP_VIDEO_EN,
	ENCP_VIDEO_MODE,
	ENCP_VIDEO_MODE_ADV,
	ENCP_VIDEO_MAX_PXCNT,
	ENCP_VIDEO_MAX_LNCNT,
	ENCP_VIDEO_HAVON_BEGIN,
	ENCP_VIDEO_HAVON_END,
	ENCP_VIDEO_VAVON_BLINE,
	ENCP_VIDEO_VAVON_ELINE,
	ENCP_VIDEO_HSO_BEGIN,
	ENCP_VIDEO_HSO_END,
	ENCP_VIDEO_VSO_BEGIN,
	ENCP_VIDEO_VSO_END,
	ENCP_VIDEO_VSO_BLINE,
	ENCP_VIDEO_VSO_ELINE,
	ENCP_VIDEO_RGBIN_CTRL,
	0xffff
};

static unsigned int dummy_enci_reg_dump[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCI_VIDEO_EN,
	ENCI_CFILT_CTRL,
	ENCI_CFILT_CTRL2,
	ENCI_VIDEO_MODE,
	ENCI_VIDEO_MODE_ADV,
	ENCI_SYNC_HSO_BEGIN,
	ENCI_SYNC_HSO_END,
	ENCI_SYNC_VSO_EVNLN,
	ENCI_SYNC_VSO_ODDLN,
	ENCI_MACV_MAX_AMP,
	VENC_VIDEO_PROG_MODE,
	ENCI_VIDEO_SCH,
	ENCI_SYNC_MODE,
	ENCI_YC_DELAY,
	ENCI_VFIFO2VD_PIXEL_START,
	ENCI_VFIFO2VD_PIXEL_END,
	ENCI_VFIFO2VD_LINE_TOP_START,
	ENCI_VFIFO2VD_LINE_TOP_END,
	ENCI_VFIFO2VD_LINE_BOT_START,
	ENCI_VFIFO2VD_LINE_BOT_END,
	VENC_SYNC_ROUTE,
	ENCI_DBG_PX_RST,
	VENC_INTCTRL,
	ENCI_VFIFO2VD_CTL,
	VENC_UPSAMPLE_CTRL0,
	VENC_UPSAMPLE_CTRL1,
	VENC_UPSAMPLE_CTRL2,
	ENCI_DACSEL_0,
	ENCI_DACSEL_1,
	ENCI_VIDEO_SAT,
	ENCI_SYNC_ADJ,
	ENCI_VIDEO_CONT,
	0xffff
};

static unsigned int dummy_encl_reg_dump[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCL_VIDEO_EN,
	ENCL_VIDEO_MODE,
	ENCL_VIDEO_MODE_ADV,
	ENCL_VIDEO_MAX_PXCNT,
	ENCL_VIDEO_MAX_LNCNT,
	ENCL_VIDEO_HAVON_BEGIN,
	ENCL_VIDEO_HAVON_END,
	ENCL_VIDEO_VAVON_BLINE,
	ENCL_VIDEO_VAVON_ELINE,
	ENCL_VIDEO_HSO_BEGIN,
	ENCL_VIDEO_HSO_END,
	ENCL_VIDEO_VSO_BEGIN,
	ENCL_VIDEO_VSO_END,
	ENCL_VIDEO_VSO_BLINE,
	ENCL_VIDEO_VSO_ELINE,
	ENCL_VIDEO_RGBIN_CTRL,
	0xffff
};

static ssize_t dummy_encp_debug_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int i;

	switch (buf[0]) {
	case 'r':
		pr_info("dummy_encp register dump:\n");
		i = 0;
		while (i < ARRAY_SIZE(dummy_encp_reg_dump)) {
			if (dummy_encp_reg_dump[i] == 0xffff)
				break;
			pr_info("vcbus   [0x%04x] = 0x%08x\n",
				dummy_encp_reg_dump[i],
				vout_vcbus_read(dummy_encp_reg_dump[i]));
			i++;
		}
		break;
	default:
		pr_info("invalid data\n");
		break;
	}

	return count;
}

static ssize_t dummy_enci_debug_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int i;

	switch (buf[0]) {
	case 'r':
		pr_info("dummy_enci register dump:\n");
		i = 0;
		while (i < ARRAY_SIZE(dummy_enci_reg_dump)) {
			if (dummy_enci_reg_dump[i] == 0xffff)
				break;
			pr_info("vcbus   [0x%04x] = 0x%08x\n",
				dummy_enci_reg_dump[i],
				vout_vcbus_read(dummy_enci_reg_dump[i]));
			i++;
		}
		break;
	default:
		pr_info("invalid data\n");
		break;
	}

	return count;
}

static ssize_t dummy_encl_debug_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int i;

	switch (buf[0]) {
	case 'r':
		pr_info("dummy_encl register dump:\n");
		i = 0;
		while (i < ARRAY_SIZE(dummy_encl_reg_dump)) {
			if (dummy_encl_reg_dump[i] == 0xffff)
				break;
			pr_info("vcbus   [0x%04x] = 0x%08x\n",
				dummy_encl_reg_dump[i],
				vout_vcbus_read(dummy_encl_reg_dump[i]));
			i++;
		}
		break;
	default:
		pr_info("invalid data\n");
		break;
	}

	return count;
}

static struct class_attribute dummy_venc_class_attrs[] = {
	__ATTR(encp, 0644,
	       dummy_venc_debug_show, dummy_encp_debug_store),
	__ATTR(enci, 0644,
	       dummy_venc_debug_show, dummy_enci_debug_store),
	__ATTR(encl, 0644,
	       dummy_venc_debug_show, dummy_encl_debug_store)
};

static struct class *debug_class;
static int dummy_venc_creat_class(void)
{
	int i;

	debug_class = class_create(THIS_MODULE, "dummy_venc");
	if (IS_ERR(debug_class)) {
		VOUTERR("create debug class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dummy_venc_class_attrs); i++) {
		if (class_create_file(debug_class,
				      &dummy_venc_class_attrs[i])) {
			VOUTERR("create debug attribute %s failed\n",
				dummy_venc_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int dummy_venc_remove_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dummy_venc_class_attrs); i++)
		class_remove_file(debug_class, &dummy_venc_class_attrs[i]);

	class_destroy(debug_class);
	debug_class = NULL;

	return 0;
}

/* ********************************************************* */
static void dummy_venc_clktree_probe(struct device *dev)
{
	int ret = 0;

	/* encp */
	dummy_encp_drv->clk_gate_state = 0;

	dummy_encp_drv->top_gate = devm_clk_get(dev, "encp_top_gate");
	if (IS_ERR_OR_NULL(dummy_encp_drv->top_gate))
		ret |= (1 << 0);

	dummy_encp_drv->int_gate0 = devm_clk_get(dev, "encp_int_gate0");
	if (IS_ERR_OR_NULL(dummy_encp_drv->int_gate0))
		ret |= (1 << 1);

	dummy_encp_drv->int_gate1 = devm_clk_get(dev, "encp_int_gate1");
	if (IS_ERR_OR_NULL(dummy_encp_drv->int_gate1))
		ret |= (1 << 2);

	/* enci */
	dummy_enci_drv->clk_gate_state = 0;

	dummy_enci_drv->top_gate = devm_clk_get(dev, "enci_top_gate");
	if (IS_ERR_OR_NULL(dummy_enci_drv->top_gate))
		ret |= (1 << 3);

	dummy_enci_drv->int_gate0 = devm_clk_get(dev, "enci_int_gate0");
	if (IS_ERR_OR_NULL(dummy_enci_drv->int_gate0))
		ret |= (1 << 4);

	dummy_enci_drv->int_gate1 = devm_clk_get(dev, "enci_int_gate1");
	if (IS_ERR_OR_NULL(dummy_enci_drv->int_gate1))
		ret |= (1 << 5);

	/* encl */
	dummy_encl_drv->clk_gate_state = 0;

	dummy_encl_drv->top_gate = devm_clk_get(dev, "encl_top_gate");
	if (IS_ERR_OR_NULL(dummy_encl_drv->top_gate))
		ret |= (1 << 6);

	dummy_encl_drv->int_gate0 = devm_clk_get(dev, "encl_int_gate");
	if (IS_ERR_OR_NULL(dummy_encl_drv->int_gate0))
		ret |= (1 << 7);

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static void dummy_venc_clktree_remove(struct device *dev)
{
	if (!IS_ERR_OR_NULL(dummy_encp_drv->top_gate))
		devm_clk_put(dev, dummy_encp_drv->top_gate);
	if (!IS_ERR_OR_NULL(dummy_encp_drv->int_gate0))
		devm_clk_put(dev, dummy_encp_drv->int_gate0);
	if (!IS_ERR_OR_NULL(dummy_encp_drv->int_gate1))
		devm_clk_put(dev, dummy_encp_drv->int_gate1);

	if (!IS_ERR_OR_NULL(dummy_enci_drv->top_gate))
		devm_clk_put(dev, dummy_enci_drv->top_gate);
	if (!IS_ERR_OR_NULL(dummy_enci_drv->int_gate0))
		devm_clk_put(dev, dummy_enci_drv->int_gate0);
	if (!IS_ERR_OR_NULL(dummy_enci_drv->int_gate1))
		devm_clk_put(dev, dummy_enci_drv->int_gate1);

	if (!IS_ERR_OR_NULL(dummy_encl_drv->top_gate))
		devm_clk_put(dev, dummy_encl_drv->top_gate);
	if (!IS_ERR_OR_NULL(dummy_encl_drv->int_gate0))
		devm_clk_put(dev, dummy_encl_drv->int_gate0);
}

static struct dummy_venc_data_s dummy_venc_match_data = {
	.vid_clk_ctrl_reg = HHI_VID_CLK_CNTL,
	.vid_clk_ctrl2_reg = HHI_VID_CLK_CNTL2,
	.vid_clk_div_reg = HHI_VID_CLK_DIV,
	.vid2_clk_ctrl_reg = HHI_VIID_CLK_CNTL,
	.vid2_clk_div_reg = HHI_VIID_CLK_DIV,

	.clktree_probe = dummy_venc_clktree_probe,
	.clktree_remove = dummy_venc_clktree_remove,
	.encp_clk_gate_switch = dummy_encp_clk_gate_switch,
	.enci_clk_gate_switch = dummy_enci_clk_gate_switch,
	.encl_clk_gate_switch = dummy_encl_clk_gate_switch,
};

static struct dummy_venc_data_s dummy_venc_match_data_sc2 = {
	.vid_clk_ctrl_reg = CLKCTRL_VID_CLK_CTRL,
	.vid_clk_ctrl2_reg = CLKCTRL_VID_CLK_CTRL2,
	.vid_clk_div_reg = CLKCTRL_VID_CLK_DIV,
	.vid2_clk_ctrl_reg = CLKCTRL_VIID_CLK_CTRL,
	.vid2_clk_div_reg = CLKCTRL_VIID_CLK_DIV,

	.clktree_probe = NULL,
	.clktree_remove = NULL,
	.encp_clk_gate_switch = NULL,
	.enci_clk_gate_switch = NULL,
	.encl_clk_gate_switch = NULL,
};

static const struct of_device_id dummy_venc_dt_match_table[] = {
	{
		.compatible = "amlogic, dummy_venc",
		.data = &dummy_venc_match_data,
	},
	{
		.compatible = "amlogic, dummy_venc_sc2",
		.data = &dummy_venc_match_data_sc2,
	},
	{
		.compatible = "amlogic, dummy_venc_s4",
		.data = &dummy_venc_match_data_sc2,
	},
	{}
};

static int dummy_venc_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_device(dummy_venc_dt_match_table, &pdev->dev);
	if (!match) {
		VOUTERR("%s: no match table\n", __func__);
		return -1;
	}
	dummy_venc_data = (struct dummy_venc_data_s *)match->data;
	if (!dummy_venc_data) {
		VOUTERR("%s: no match data\n", __func__);
		return -1;
	}

	dummy_encp_drv = kzalloc(sizeof(*dummy_encp_drv), GFP_KERNEL);
	if (!dummy_encp_drv) {
		VOUTERR("%s: dummy_venc driver no enough memory\n", __func__);
		return -ENOMEM;
	}
	dummy_encp_drv->dev = &pdev->dev;
#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for encp*/
	dummy_encp_drv->vpu_dev = vpu_dev_register(VPU_VENCP, DUMMY_P_NAME);
#endif

	dummy_enci_drv = kzalloc(sizeof(*dummy_enci_drv), GFP_KERNEL);
	if (!dummy_enci_drv) {
		VOUTERR("%s: dummy_venc driver no enough memory\n", __func__);
		kfree(dummy_encp_drv);
		return -ENOMEM;
	}
	dummy_enci_drv->dev = &pdev->dev;
#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for encp*/
	dummy_enci_drv->vpu_dev = vpu_dev_register(VPU_VENCI, DUMMY_I_NAME);
#endif

	dummy_encl_drv = kzalloc(sizeof(*dummy_encl_drv), GFP_KERNEL);
	if (!dummy_encl_drv) {
		VOUTERR("%s: dummy_venc driver no enough memory\n", __func__);
		kfree(dummy_encp_drv);
		kfree(dummy_enci_drv);
		return -ENOMEM;
	}
	dummy_encl_drv->dev = &pdev->dev;
#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for encp*/
	dummy_encl_drv->vpu_dev = vpu_dev_register(VPU_VENCL, DUMMY_L_NAME);
#endif

	if (dummy_venc_data->clktree_probe)
		dummy_venc_data->clktree_probe(&pdev->dev);

	dummy_encp_vout_server_init();
	dummy_enci_vout_server_init();
	dummy_encl_vout_server_init();

	/* only need for encp dummy_panel */
	vout_register_client(&dummy_encp_vout_notifier);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_client(&dummy_encp_vout2_notifier);
#endif

	dummy_venc_creat_class();

	VOUTPR("%s OK\n", __func__);

	return 0;
}

static int dummy_venc_remove(struct platform_device *pdev)
{
	if (!dummy_venc_data)
		return 0;
	if (!dummy_encp_drv)
		return 0;
	if (!dummy_enci_drv)
		return 0;
	if (!dummy_encl_drv)
		return 0;

	dummy_venc_remove_class();
	vout_unregister_client(&dummy_encp_vout_notifier);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_client(&dummy_encp_vout2_notifier);
#endif
	dummy_encp_vout_server_remove();
	dummy_enci_vout_server_remove();
	dummy_encl_vout_server_remove();
	if (dummy_venc_data->clktree_remove)
		dummy_venc_data->clktree_remove(&pdev->dev);

	kfree(dummy_encp_drv);
	dummy_encp_drv = NULL;
	kfree(dummy_enci_drv);
	dummy_enci_drv = NULL;
	kfree(dummy_encl_drv);
	dummy_encl_drv = NULL;

	VOUTPR("%s\n", __func__);
	return 0;
}

static void dummy_venc_shutdown(struct platform_device *pdev)
{
	if (!dummy_encp_drv)
		return;
	if (dummy_encp_drv->status)
		dummy_encp_disable(VMODE_DUMMY_ENCP);

	if (!dummy_enci_drv)
		return;
	if (dummy_enci_drv->status)
		dummy_enci_disable(VMODE_DUMMY_ENCI);

	if (!dummy_encl_drv)
		return;
	if (dummy_encl_drv->status)
		dummy_encl_disable(VMODE_DUMMY_ENCL);
}

static struct platform_driver dummy_venc_platform_driver = {
	.probe = dummy_venc_probe,
	.remove = dummy_venc_remove,
	.shutdown = dummy_venc_shutdown,
	.driver = {
		.name = "dummy_venc",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dummy_venc_dt_match_table,
#endif
	},
};

int __init dummy_venc_init(void)
{
	if (platform_driver_register(&dummy_venc_platform_driver)) {
		VOUTERR("failed to register dummy_venc driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit dummy_venc_exit(void)
{
	platform_driver_unregister(&dummy_venc_platform_driver);
}

