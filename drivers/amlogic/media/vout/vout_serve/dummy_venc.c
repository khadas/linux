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
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/gki_module.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"

enum dummy_venc_chip_e {
	DUMMY_VENC_DFT = 0,
	DUMMY_VENC_SC2, /* 1, stb single display */
	DUMMY_VENC_T7,  /* 2, triple display */
	DUMMY_VENC_T5W, /* 3, tv dual display */
	DUMMY_VENC_S5,  /* 4, stb new single display */
	DUMMY_VENC_MAX,
};

enum dummy_venc_sel_e {
	DUMMY_SEL_ENCL = 0,
	DUMMY_SEL_ENCI, /* 1 */
	DUMMY_SEL_ENCP,  /* 2 */
	DUMMY_SEL_MAX,
};

struct dummy_venc_driver_s;

struct venc_config_s {
	unsigned int vid_clk_ctrl_reg;
	unsigned int vid_clk_ctrl2_reg;
	unsigned int vid_clk_div_reg;
	unsigned int vid2_clk_ctrl_reg;
	unsigned int vid2_clk_div_reg;

	unsigned int venc_index;
	unsigned int venc_offset;
};

struct dummy_venc_data_s {
	struct venc_config_s *vconf;

	unsigned int chip_type;
	unsigned int default_venc_index;
	unsigned int projection_valid;

	void (*clktree_probe)(struct device *dev);
	void (*clktree_remove)(struct device *dev);
	void (*encp_clk_gate_switch)(struct dummy_venc_driver_s *venc_drv, int flag);
	void (*enci_clk_gate_switch)(struct dummy_venc_driver_s *venc_drv, int flag);
	void (*encl_clk_gate_switch)(struct dummy_venc_driver_s *venc_drv, int flag);
	void (*venc_sel)(struct dummy_venc_driver_s *venc_drv, unsigned int venc_sel);
};

struct dummy_venc_driver_s {
	struct device *dev;
	unsigned char status;
	unsigned char vout_valid;
	unsigned char viu_sel;
	unsigned char vinfo_index;

	unsigned char clk_gate_state;
	unsigned char projection_state;

	struct clk *top_gate;
	struct clk *int_gate0;
	struct clk *int_gate1;
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *vpu_dev;
#endif

	struct dummy_venc_data_s *vdata;
	struct vinfo_s *vinfo;
};

static struct dummy_venc_driver_s *dummy_encp_drv;
static struct dummy_venc_driver_s *dummy_enci_drv;
static struct dummy_venc_driver_s *dummy_encl_drv;

#define DUMMY_L_NAME    "dummy_encl"
#define DUMMY_I_NAME    "dummy_enci"
#define DUMMY_P_NAME    "dummy_encp"

/* **********************************************************
 * common function
 * **********************************************************
 */
static void dummy_venc_sel_t7(struct dummy_venc_driver_s *venc_drv, unsigned int venc_sel)
{
	unsigned int offset, reg_ctrl_sel = 0xff;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	offset = venc_drv->vdata->vconf->venc_offset;
	switch (venc_sel) {
	case DUMMY_SEL_ENCL:
		reg_ctrl_sel = 2;
		break;
	case DUMMY_SEL_ENCI:
		reg_ctrl_sel = 0;
		break;
	case DUMMY_SEL_ENCP:
		reg_ctrl_sel = 1;
		break;
	default:
		break;
	}
	if (reg_ctrl_sel == 0xff)
		return;

	vout_vcbus_setb(VPU_VENC_CTRL + offset, reg_ctrl_sel, 0, 2);
}

/* **********************************************************
 * dummy_encp support
 * **********************************************************
 */
static struct vinfo_s dummy_encp_vinfo[] = {
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
	},
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
	}
};

static struct vinfo_s dummy_encp_vinfo_t7[] = {
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
	},
	{//no dummy_panel for t7/t3, use projection function in lcd driver
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
		.viu_mux           = VIU_MUX_PROJECT,
		.vout_device       = NULL,
	}
};

static void dummy_encp_venc_set(struct dummy_venc_driver_s *venc_drv)
{
	unsigned int temp, offset;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	offset = venc_drv->vdata->vconf->venc_offset;

	VOUTPR("%s: dummy_encp vinfo_index=%d\n", __func__, venc_drv->vinfo_index);

	if (venc_drv->vinfo_index == 1) {
		vout_vcbus_write(ENCP_VIDEO_EN + offset, 0);

		vout_vcbus_write(ENCP_VIDEO_MODE + offset, 0x8000);
		vout_vcbus_write(ENCP_VIDEO_MODE_ADV + offset, 0x0418);

		temp = vout_vcbus_read(ENCL_VIDEO_MAX_PXCNT + offset);
		vout_vcbus_write(ENCP_VIDEO_MAX_PXCNT + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_MAX_LNCNT + offset);
		vout_vcbus_write(ENCP_VIDEO_MAX_LNCNT + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_HAVON_BEGIN + offset);
		vout_vcbus_write(ENCP_VIDEO_HAVON_BEGIN + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_HAVON_END + offset);
		vout_vcbus_write(ENCP_VIDEO_HAVON_END + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VAVON_BLINE + offset);
		vout_vcbus_write(ENCP_VIDEO_VAVON_BLINE + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VAVON_ELINE + offset);
		vout_vcbus_write(ENCP_VIDEO_VAVON_ELINE + offset, temp);

		temp = vout_vcbus_read(ENCL_VIDEO_HSO_BEGIN + offset);
		vout_vcbus_write(ENCP_VIDEO_HSO_BEGIN + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_HSO_END + offset);
		vout_vcbus_write(ENCP_VIDEO_HSO_END + offset,   temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_BEGIN + offset);
		vout_vcbus_write(ENCP_VIDEO_VSO_BEGIN + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_END + offset);
		vout_vcbus_write(ENCP_VIDEO_VSO_END + offset,   temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_BLINE + offset);
		vout_vcbus_write(ENCP_VIDEO_VSO_BLINE + offset, temp);
		temp = vout_vcbus_read(ENCL_VIDEO_VSO_ELINE + offset);
		vout_vcbus_write(ENCP_VIDEO_VSO_ELINE + offset, temp);

		temp = vout_vcbus_read(ENCL_VIDEO_VSO_ELINE + offset);
		vout_vcbus_write(ENCP_VIDEO_RGBIN_CTRL + offset, temp);

		vout_vcbus_write(ENCP_VIDEO_EN + offset, 1);
	} else {
		vout_vcbus_write(ENCP_VIDEO_EN + offset, 0);

		vout_vcbus_write(ENCP_VIDEO_MODE + offset, 0x8000);
		vout_vcbus_write(ENCP_VIDEO_MODE_ADV + offset, 0x0418);

		vout_vcbus_write(ENCP_VIDEO_MAX_PXCNT + offset, 951);
		vout_vcbus_write(ENCP_VIDEO_MAX_LNCNT + offset, 524);
		vout_vcbus_write(ENCP_VIDEO_HAVON_BEGIN + offset, 80);
		vout_vcbus_write(ENCP_VIDEO_HAVON_END + offset, 799);
		vout_vcbus_write(ENCP_VIDEO_VAVON_BLINE + offset, 22);
		vout_vcbus_write(ENCP_VIDEO_VAVON_ELINE + offset, 501);

		vout_vcbus_write(ENCP_VIDEO_HSO_BEGIN + offset, 0);
		vout_vcbus_write(ENCP_VIDEO_HSO_END + offset,   20);
		vout_vcbus_write(ENCP_VIDEO_VSO_BEGIN + offset, 0);
		vout_vcbus_write(ENCP_VIDEO_VSO_END + offset,   0);
		vout_vcbus_write(ENCP_VIDEO_VSO_BLINE + offset, 0);
		vout_vcbus_write(ENCP_VIDEO_VSO_ELINE + offset, 5);

		vout_vcbus_write(ENCP_VIDEO_RGBIN_CTRL + offset, 1);

		vout_vcbus_write(ENCP_VIDEO_EN + offset, 1);
	}

	if (venc_drv->vdata->venc_sel)
		venc_drv->vdata->venc_sel(venc_drv, DUMMY_SEL_ENCP);
}

static void dummy_panel_clear_mute(struct dummy_venc_driver_s *venc_drv)
{
	if (venc_drv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		if (get_output_mute()) {
			set_output_mute(false);
			VOUTPR("%s: clr mute for dummy_panel\n", __func__);
		}
#endif
	}
}

static void dummy_encp_clk_ctrl(struct dummy_venc_driver_s *venc_drv, int flag)
{
	struct venc_config_s *vconf;
	unsigned int temp;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	vconf = venc_drv->vdata->vconf;
	if (venc_drv->vinfo_index == 1) {
		if (flag) {
			temp = vout_clk_getb(vconf->vid2_clk_div_reg, ENCL_CLK_SEL, 4);
			vout_clk_setb(vconf->vid_clk_div_reg, temp, ENCP_CLK_SEL, 4);

			vout_clk_setb(vconf->vid_clk_ctrl2_reg, 1, ENCP_GATE_VCLK, 1);
		} else {
			vout_clk_setb(vconf->vid_clk_ctrl2_reg, 0, ENCP_GATE_VCLK, 1);
		}
	} else {
		if (flag) {
			/* clk source sel: fckl_div5 */
			vout_clk_setb(vconf->vid_clk_div_reg, 0xf, VCLK_XD0, 8);
			usleep_range(5, 6);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 6, VCLK_CLK_IN_SEL, 3);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 1, VCLK_EN0, 1);
			usleep_range(5, 6);
			vout_clk_setb(vconf->vid_clk_div_reg, 0, ENCP_CLK_SEL, 4);
			vout_clk_setb(vconf->vid_clk_div_reg, 1, VCLK_XD_EN, 1);
			usleep_range(5, 6);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 1, VCLK_DIV1_EN, 1);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 1, VCLK_SOFT_RST, 1);
			usleep_range(10, 11);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 0, VCLK_SOFT_RST, 1);
			usleep_range(5, 6);
			vout_clk_setb(vconf->vid_clk_ctrl2_reg, 1, ENCP_GATE_VCLK, 1);
		} else {
			vout_clk_setb(vconf->vid_clk_ctrl2_reg, 0, ENCP_GATE_VCLK, 1);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 0, VCLK_DIV1_EN, 1);
			vout_clk_setb(vconf->vid_clk_ctrl_reg, 0, VCLK_EN0, 1);
			vout_clk_setb(vconf->vid_clk_div_reg, 0, VCLK_XD_EN, 1);
		}
	}
}

static void dummy_encp_clk_gate_switch(struct dummy_venc_driver_s *venc_drv, int flag)
{
	int ret = 0;

	if (!venc_drv)
		return;

	if (flag) {
		if (venc_drv->clk_gate_state)
			return;
		if (IS_ERR_OR_NULL(venc_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_prepare_enable(venc_drv->top_gate);
		if (IS_ERR_OR_NULL(venc_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_prepare_enable(venc_drv->int_gate0);
		if (IS_ERR_OR_NULL(venc_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_prepare_enable(venc_drv->int_gate1);
		venc_drv->clk_gate_state = 1;
	} else {
		if (venc_drv->clk_gate_state == 0)
			return;
		venc_drv->clk_gate_state = 0;
		if (IS_ERR_OR_NULL(venc_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_disable_unprepare(venc_drv->int_gate0);
		if (IS_ERR_OR_NULL(venc_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_disable_unprepare(venc_drv->int_gate1);
		if (IS_ERR_OR_NULL(venc_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_disable_unprepare(venc_drv->top_gate);
	}

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static void dummy_encp_vinfo_update(struct dummy_venc_driver_s *venc_drv)
{
	unsigned int lcd_viu_sel = 0;
	const struct vinfo_s *vinfo = NULL;

	if (!venc_drv)
		return;

	/* only dummy_panel need update vinfo */
	if (venc_drv->vinfo_index == 0)
		return;
	if (!venc_drv->vinfo)
		return;

	if (venc_drv->viu_sel == 1) {
		vinfo = get_current_vinfo2();
		lcd_viu_sel = 2;
	} else if (venc_drv->viu_sel == 2) {
		vinfo = get_current_vinfo();
		lcd_viu_sel = 1;
	}
	if (!vinfo)
		return;

	if (vinfo->mode != VMODE_LCD) {
		VOUTERR("display%d is not panel\n", lcd_viu_sel);
		return;
	}

	venc_drv->vinfo->width = vinfo->width;
	venc_drv->vinfo->height = vinfo->height;
	venc_drv->vinfo->field_height = vinfo->field_height;
	venc_drv->vinfo->aspect_ratio_num = vinfo->aspect_ratio_num;
	venc_drv->vinfo->aspect_ratio_den = vinfo->aspect_ratio_den;
	venc_drv->vinfo->sync_duration_num = vinfo->sync_duration_num;
	venc_drv->vinfo->sync_duration_den = vinfo->sync_duration_den;
	venc_drv->vinfo->video_clk = vinfo->video_clk;
	venc_drv->vinfo->htotal = vinfo->htotal;
	venc_drv->vinfo->vtotal = vinfo->vtotal;
	venc_drv->vinfo->viu_color_fmt = vinfo->viu_color_fmt;
}

static struct vinfo_s *dummy_encp_get_current_info(void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vinfo)
		return NULL;

	return venc_drv->vinfo;
}

static int dummy_encp_set_current_vmode(enum vmode_e mode, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata || !venc_drv->vinfo)
		return -1;

	if (venc_drv->vinfo_index > 1)
		return -1;

	dummy_encp_vinfo_update(venc_drv);

	if (venc_drv->projection_state == 0) {
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_clk_request(venc_drv->vpu_dev, venc_drv->vinfo->video_clk);
		vpu_dev_mem_power_on(venc_drv->vpu_dev);
#endif
		if (venc_drv->vdata->encp_clk_gate_switch)
			venc_drv->vdata->encp_clk_gate_switch(venc_drv, 1);

		dummy_encp_clk_ctrl(venc_drv, 1);
		dummy_encp_venc_set(venc_drv);
	}

	if (venc_drv->vinfo_index == 1)
		dummy_panel_clear_mute(venc_drv);

	venc_drv->status = 1;
	venc_drv->vout_valid = 1;
	VOUTPR("%s finished\n", __func__);

	return 0;
}

static enum vmode_e dummy_encp_validate_vmode(char *name, unsigned int frac, void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	enum vmode_e vmode = VMODE_MAX;
	unsigned int venc_index;
	int i, find_mode = 0;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata)
		return VMODE_MAX;

	if (frac)
		return VMODE_MAX;

	venc_index = venc_drv->vdata->default_venc_index;
	if (venc_drv->vdata->projection_valid) {
		for (i = 0; i < 2; i++) {
			if (strcmp(dummy_encp_vinfo_t7[i].name, name) == 0) {
				venc_drv->vinfo = &dummy_encp_vinfo_t7[i];
				vmode = venc_drv->vinfo->mode;
				venc_drv->vinfo_index = i;
				find_mode = 1;
				if (venc_drv->vinfo_index == 1)
					venc_drv->projection_state = 1;
				else
					venc_drv->projection_state = 0;
				break;
			}
		}
	} else {
		for (i = 0; i < 2; i++) {
			if (strcmp(dummy_encp_vinfo[i].name, name) == 0) {
				venc_drv->vinfo = &dummy_encp_vinfo[i];
				vmode = venc_drv->vinfo->mode;
				venc_drv->vinfo_index = i;
				find_mode = 1;
				break;
			}
		}
	}

	if (!find_mode)
		return VMODE_MAX;

	venc_drv->vinfo->viu_mux &= ~(0xf << 4);
	venc_drv->vinfo->viu_mux |= (venc_index << 4);
	return vmode;
}

static int dummy_encp_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DUMMY_ENCP)
		return true;

	return false;
}

static int dummy_encp_disable(enum vmode_e cur_vmod, void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	unsigned int offset;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata)
		return 0;
	if (!venc_drv->vdata->vconf)
		return 0;

	offset = venc_drv->vdata->vconf->venc_offset;

	venc_drv->status = 0;
	venc_drv->vout_valid = 0;

	if (venc_drv->projection_state == 0) {
		vout_vcbus_write(ENCP_VIDEO_EN + offset, 0); /* disable encp */
		dummy_encp_clk_ctrl(venc_drv, 0);
		if (venc_drv->vdata->encp_clk_gate_switch)
			venc_drv->vdata->encp_clk_gate_switch(venc_drv, 0);
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_mem_power_down(venc_drv->vpu_dev);
		vpu_dev_clk_release(venc_drv->vpu_dev);
#endif
	}

	venc_drv->vinfo_index = 0xff;
	venc_drv->projection_state = 0;

	VOUTPR("%s finished\n", __func__);
	return 0;
}

static int dummy_encp_suspend(void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	unsigned int offset;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata)
		return 0;

	offset = venc_drv->vdata->vconf->venc_offset;

	venc_drv->status = 0;

	if (venc_drv->projection_state == 0) {
		vout_vcbus_write(ENCP_VIDEO_EN + offset, 0); /* disable encp */
		dummy_encp_clk_ctrl(venc_drv, 0);
		if (venc_drv->vdata->encp_clk_gate_switch)
			venc_drv->vdata->encp_clk_gate_switch(venc_drv, 0);
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_mem_power_down(venc_drv->vpu_dev);
		vpu_dev_clk_release(venc_drv->vpu_dev);
#endif
	}

	VOUTPR("%s finished\n", __func__);
	return 0;
}

static int dummy_encp_resume(void *data)
{
	dummy_encp_set_current_vmode(VMODE_DUMMY_ENCP, data);
	VOUTPR("%s finished\n", __func__);
	return 0;
}

static int dummy_encp_vout_state;
static int dummy_encp_vout_set_state(int bit, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv)
		return 0;

	dummy_encp_vout_state |= (1 << bit);
	venc_drv->viu_sel = bit;
	return 0;
}

static int dummy_encp_vout_clr_state(int bit, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv)
		return 0;

	dummy_encp_vout_state &= ~(1 << bit);
	if (venc_drv->viu_sel == bit)
		venc_drv->viu_sel = 0;
	return 0;
}

static int dummy_encp_vout_get_state(void *data)
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
};
#endif

static void dummy_encp_vout_server_init(struct dummy_venc_driver_s *venc_drv)
{
	venc_drv->vinfo_index = 0xff;
	venc_drv->vinfo = NULL;
	dummy_encp_vout_server.data = (void *)venc_drv;
	vout_register_server(&dummy_encp_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	dummy_encp_vout2_server.data = (void *)venc_drv;
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

static void dummy_enci_venc_set(struct dummy_venc_driver_s *venc_drv)
{
	unsigned int offset;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	offset = venc_drv->vdata->vconf->venc_offset;

	vout_vcbus_write(ENCI_VIDEO_EN + offset, 0);

	vout_vcbus_write(ENCI_CFILT_CTRL + offset,                 0x12);
	vout_vcbus_write(ENCI_CFILT_CTRL2 + offset,                0x12);
	vout_vcbus_write(ENCI_VIDEO_MODE + offset,                 0);
	vout_vcbus_write(ENCI_VIDEO_MODE_ADV + offset,             0);
	vout_vcbus_write(ENCI_SYNC_HSO_BEGIN + offset,             5);
	vout_vcbus_write(ENCI_SYNC_HSO_END + offset,               129);
	vout_vcbus_write(ENCI_SYNC_VSO_EVNLN + offset,             0x0003);
	vout_vcbus_write(ENCI_SYNC_VSO_ODDLN + offset,             0x0104);
	vout_vcbus_write(ENCI_MACV_MAX_AMP + offset,               0x810b);
	vout_vcbus_write(VENC_VIDEO_PROG_MODE + offset,            0xf0);
	vout_vcbus_write(ENCI_VIDEO_MODE + offset,                 0x08);
	vout_vcbus_write(ENCI_VIDEO_MODE_ADV + offset,             0x26);
	vout_vcbus_write(ENCI_VIDEO_SCH + offset,                  0x20);
	vout_vcbus_write(ENCI_SYNC_MODE + offset,                  0x07);
	vout_vcbus_write(ENCI_YC_DELAY + offset,                   0x333);
	vout_vcbus_write(ENCI_VFIFO2VD_PIXEL_START + offset,       0xe3);
	vout_vcbus_write(ENCI_VFIFO2VD_PIXEL_END + offset,         0x0683);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_TOP_START + offset,    0x12);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_TOP_END + offset,      0x102);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_BOT_START + offset,    0x13);
	vout_vcbus_write(ENCI_VFIFO2VD_LINE_BOT_END + offset,      0x103);
	vout_vcbus_write(VENC_SYNC_ROUTE + offset,                 0);
	vout_vcbus_write(ENCI_DBG_PX_RST + offset,                 0);
	vout_vcbus_write(VENC_INTCTRL + offset,                    0x2);
	vout_vcbus_write(ENCI_VFIFO2VD_CTL + offset,               0x4e01);
	vout_vcbus_write(VENC_UPSAMPLE_CTRL0 + offset,             0x0061);
	vout_vcbus_write(VENC_UPSAMPLE_CTRL1 + offset,             0x4061);
	vout_vcbus_write(VENC_UPSAMPLE_CTRL2 + offset,             0x5061);
	vout_vcbus_write(ENCI_DACSEL_0 + offset,                   0x0011);
	vout_vcbus_write(ENCI_DACSEL_1 + offset,                   0x11);
	vout_vcbus_write(ENCI_VIDEO_EN + offset,                   1);
	vout_vcbus_write(ENCI_VIDEO_SAT + offset,                  0x12);
	vout_vcbus_write(ENCI_SYNC_ADJ + offset,                   0x9c00);
	vout_vcbus_write(ENCI_VIDEO_CONT + offset,                 0x3);

	if (venc_drv->vdata->venc_sel)
		venc_drv->vdata->venc_sel(venc_drv, DUMMY_SEL_ENCI);
}

static void dummy_enci_clk_ctrl(struct dummy_venc_driver_s *venc_drv, int flag)
{
	struct venc_config_s *vconf;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	vconf = venc_drv->vdata->vconf;

	if (flag) {
		/* clk source sel: fckl_div5 */
		vout_clk_setb(vconf->vid2_clk_div_reg, 0xf, VCLK2_XD, 8);
		usleep_range(5, 6);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 6, VCLK2_CLK_IN_SEL, 3);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 1, VCLK2_EN, 1);
		usleep_range(5, 6);

		vout_clk_setb(vconf->vid_clk_div_reg, 8, ENCI_CLK_SEL, 4);
		vout_clk_setb(vconf->vid2_clk_div_reg, 1, VCLK2_XD_EN, 1);
		usleep_range(5, 6);

		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 1, VCLK2_DIV1_EN, 1);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 1, VCLK2_SOFT_RST, 1);
		usleep_range(10, 11);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 0, VCLK2_SOFT_RST, 1);
		usleep_range(5, 6);
		vout_clk_setb(vconf->vid_clk_ctrl2_reg, 1, ENCI_GATE_VCLK, 1);
	} else {
		vout_clk_setb(vconf->vid_clk_ctrl2_reg, 0, ENCI_GATE_VCLK, 1);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 0, VCLK2_DIV1_EN, 1);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 0, VCLK2_EN, 1);
		vout_clk_setb(vconf->vid2_clk_div_reg, 0, VCLK2_XD_EN, 1);
	}
}

static void dummy_enci_clk_gate_switch(struct dummy_venc_driver_s *venc_drv, int flag)
{
	int ret = 0;

	if (!venc_drv)
		return;

	if (flag) {
		if (venc_drv->clk_gate_state)
			return;
		if (IS_ERR_OR_NULL(venc_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_prepare_enable(venc_drv->top_gate);
		if (IS_ERR_OR_NULL(venc_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_prepare_enable(venc_drv->int_gate0);
		if (IS_ERR_OR_NULL(venc_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_prepare_enable(venc_drv->int_gate1);
		venc_drv->clk_gate_state = 1;
	} else {
		if (venc_drv->clk_gate_state == 0)
			return;
		venc_drv->clk_gate_state = 0;
		if (IS_ERR_OR_NULL(venc_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_disable_unprepare(venc_drv->int_gate0);
		if (IS_ERR_OR_NULL(venc_drv->int_gate1))
			ret |= (1 << 2);
		else
			clk_disable_unprepare(venc_drv->int_gate1);
		if (IS_ERR_OR_NULL(venc_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_disable_unprepare(venc_drv->top_gate);
	}

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static struct vinfo_s *dummy_enci_get_current_info(void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vinfo)
		return NULL;

	return venc_drv->vinfo;
}

static int dummy_enci_set_current_vmode(enum vmode_e mode, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata || !venc_drv->vinfo)
		return -1;

#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(venc_drv->vpu_dev, venc_drv->vinfo->video_clk);
	vpu_dev_mem_power_on(venc_drv->vpu_dev);
#endif
	if (venc_drv->vdata->enci_clk_gate_switch)
		venc_drv->vdata->enci_clk_gate_switch(venc_drv, 1);

	dummy_enci_clk_ctrl(venc_drv, 1);
	dummy_enci_venc_set(venc_drv);

	venc_drv->status = 1;
	venc_drv->vout_valid = 1;
	VOUTPR("%s finished\n", __func__);

	return 0;
}

static enum vmode_e dummy_enci_validate_vmode(char *name, unsigned int frac, void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	enum vmode_e vmode = VMODE_MAX;
	unsigned int venc_index;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata || !venc_drv->vinfo)
		return VMODE_MAX;
	if (!venc_drv->vdata->vconf)
		return VMODE_MAX;
	if (frac)
		return VMODE_MAX;

	venc_index = venc_drv->vdata->default_venc_index;
	if (strcmp(venc_drv->vinfo->name, name) == 0)
		vmode = venc_drv->vinfo->mode;

	venc_drv->vinfo->viu_mux &= ~(0xf << 4);
	venc_drv->vinfo->viu_mux |= (venc_index << 4);

	return vmode;
}

static int dummy_enci_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DUMMY_ENCI)
		return true;

	return false;
}

static int dummy_enci_disable(enum vmode_e cur_vmod, void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	unsigned int offset;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata)
		return 0;
	if (!venc_drv->vdata->vconf)
		return 0;

	offset = venc_drv->vdata->vconf->venc_offset;

	venc_drv->status = 0;
	venc_drv->vout_valid = 0;

	vout_vcbus_write(ENCI_VIDEO_EN + offset, 0); /* disable enci */
	dummy_enci_clk_ctrl(venc_drv, 0);
	if (venc_drv->vdata->enci_clk_gate_switch)
		venc_drv->vdata->enci_clk_gate_switch(venc_drv, 0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(venc_drv->vpu_dev);
	vpu_dev_clk_release(venc_drv->vpu_dev);
#endif

	VOUTPR("%s finished\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int dummy_enci_suspend(void *data)
{
	dummy_enci_disable(VMODE_DUMMY_ENCI, data);
	VOUTPR("%s finished\n", __func__);
	return 0;
}

static int dummy_enci_resume(void *data)
{
	dummy_enci_set_current_vmode(VMODE_DUMMY_ENCI, data);
	VOUTPR("%s finished\n", __func__);
	return 0;
}
#endif

static int dummy_enci_vout_state;
static int dummy_enci_vout_set_state(int bit, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv)
		return 0;

	dummy_enci_vout_state |= (1 << bit);
	venc_drv->viu_sel = bit;
	return 0;
}

static int dummy_enci_vout_clr_state(int bit, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv)
		return 0;

	dummy_enci_vout_state &= ~(1 << bit);
	if (venc_drv->viu_sel == bit)
		venc_drv->viu_sel = 0;
	return 0;
}

static int dummy_enci_vout_get_state(void *data)
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
};
#endif

static void dummy_enci_vout_server_init(struct dummy_venc_driver_s *venc_drv)
{
	venc_drv->vinfo = &dummy_enci_vinfo;
	dummy_enci_vout_server.data = (void *)venc_drv;
	vout_register_server(&dummy_enci_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	dummy_enci_vout2_server.data = (void *)venc_drv;
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

static void dummy_encl_venc_set(struct dummy_venc_driver_s *venc_drv)
{
	unsigned int offset;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	offset = venc_drv->vdata->vconf->venc_offset;

	if (venc_drv->vdata->chip_type == DUMMY_VENC_S5) {
		offset = 0;

		vout_vcbus_write(ENCP_VIDEO_EN + offset, 0);

		vout_vcbus_write(ENCP_VIDEO_MODE + offset, 0x8000);
		vout_vcbus_write(ENCP_VIDEO_MODE_ADV + offset, 0x0418);

		vout_vcbus_write(ENCP_VIDEO_MAX_PXCNT + offset, 800 - 1);
		vout_vcbus_write(ENCP_VIDEO_MAX_LNCNT + offset, 525 - 1);
		vout_vcbus_write(ENCP_VIDEO_HAVON_BEGIN + offset, 80);
		vout_vcbus_write(ENCP_VIDEO_HAVON_END + offset, 799);
		vout_vcbus_write(ENCP_VIDEO_VAVON_BLINE + offset, 22);
		vout_vcbus_write(ENCP_VIDEO_VAVON_ELINE + offset, 501);

		vout_vcbus_write(ENCP_VIDEO_HSO_BEGIN + offset, 0);
		vout_vcbus_write(ENCP_VIDEO_HSO_END + offset,   20);
		vout_vcbus_write(ENCP_VIDEO_VSO_BEGIN + offset, 0);
		vout_vcbus_write(ENCP_VIDEO_VSO_END + offset,   0);
		vout_vcbus_write(ENCP_VIDEO_VSO_BLINE + offset, 0);
		vout_vcbus_write(ENCP_VIDEO_VSO_ELINE + offset, 5);

		vout_vcbus_write(ENCP_VIDEO_RGBIN_CTRL + offset, 1);

		vout_vcbus_write(ENCP_VIDEO_EN + offset, 1);
		return;
	}

	vout_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	vout_vcbus_write(ENCL_VIDEO_MODE + offset, 0x8000);
	vout_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x18);
	vout_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000);

	vout_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, 951);
	vout_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, 524);
	vout_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, 80);
	vout_vcbus_write(ENCL_VIDEO_HAVON_END + offset, 799);
	vout_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, 22);
	vout_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset, 501);

	vout_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, 0);
	vout_vcbus_write(ENCL_VIDEO_HSO_END + offset,   20);
	vout_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, 0);
	vout_vcbus_write(ENCL_VIDEO_VSO_END + offset,   0);
	vout_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, 0);
	vout_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, 5);

	vout_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);

	vout_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	if (venc_drv->vdata->venc_sel)
		venc_drv->vdata->venc_sel(venc_drv, DUMMY_SEL_ENCL);
}

static void dummy_encl_clk_ctrl(struct dummy_venc_driver_s *venc_drv, int flag)
{
	struct venc_config_s *vconf;

	if (!venc_drv || !venc_drv->vdata)
		return;
	if (!venc_drv->vdata->vconf)
		return;

	vconf = venc_drv->vdata->vconf;

	if (flag) {
		/* clk source sel: fckl_div5 */
		vout_clk_setb(vconf->vid2_clk_div_reg, 0xf, VCLK2_XD, 8);
		usleep_range(5, 6);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 6, VCLK2_CLK_IN_SEL, 3);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 1, VCLK2_EN, 1);
		usleep_range(5, 6);
		vout_clk_setb(vconf->vid2_clk_div_reg, 8, ENCL_CLK_SEL, 4);
		vout_clk_setb(vconf->vid2_clk_div_reg, 1, VCLK2_XD_EN, 1);
		usleep_range(5, 6);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 1, VCLK2_DIV1_EN, 1);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 1, VCLK2_SOFT_RST, 1);
		usleep_range(10, 11);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 0, VCLK2_SOFT_RST, 1);
		usleep_range(5, 6);
		vout_clk_setb(vconf->vid_clk_ctrl2_reg, 1, ENCL_GATE_VCLK, 1);
	} else {
		vout_clk_setb(vconf->vid_clk_ctrl2_reg, 0, ENCL_GATE_VCLK, 1);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 0, VCLK2_DIV1_EN, 1);
		vout_clk_setb(vconf->vid2_clk_ctrl_reg, 0, VCLK2_EN, 1);
		vout_clk_setb(vconf->vid2_clk_div_reg, 0, VCLK2_XD_EN, 1);
	}
}

static void dummy_encl_clk_gate_switch(struct dummy_venc_driver_s *venc_drv, int flag)
{
	int ret = 0;

	if (!venc_drv)
		return;

	if (flag) {
		if (venc_drv->clk_gate_state)
			return;
		if (IS_ERR_OR_NULL(venc_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_prepare_enable(venc_drv->top_gate);
		if (IS_ERR_OR_NULL(venc_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_prepare_enable(venc_drv->int_gate0);
		venc_drv->clk_gate_state = 1;
	} else {
		if (venc_drv->clk_gate_state == 0)
			return;
		venc_drv->clk_gate_state = 0;
		if (IS_ERR_OR_NULL(venc_drv->int_gate0))
			ret |= (1 << 1);
		else
			clk_disable_unprepare(venc_drv->int_gate0);
		if (IS_ERR_OR_NULL(venc_drv->top_gate))
			ret |= (1 << 0);
		else
			clk_disable_unprepare(venc_drv->top_gate);
	}

	if (ret)
		VOUTERR("%s: ret=0x%x\n", __func__, ret);
}

static struct vinfo_s *dummy_encl_get_current_info(void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vinfo)
		return NULL;

	return venc_drv->vinfo;
}

static int dummy_encl_set_current_vmode(enum vmode_e mode, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata || !venc_drv->vinfo)
		return -1;

#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(venc_drv->vpu_dev, venc_drv->vinfo->video_clk);
	vpu_dev_mem_power_on(venc_drv->vpu_dev);
#endif
	if (venc_drv->vdata->encl_clk_gate_switch)
		venc_drv->vdata->encl_clk_gate_switch(venc_drv, 1);

	dummy_encl_clk_ctrl(venc_drv, 1);
	dummy_encl_venc_set(venc_drv);

	venc_drv->status = 1;
	venc_drv->vout_valid = 1;
	VOUTPR("%s finished\n", __func__);

	return 0;
}

static enum vmode_e dummy_encl_validate_vmode(char *name, unsigned int frac, void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	enum vmode_e vmode = VMODE_MAX;
	unsigned int venc_index;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata || !venc_drv->vinfo)
		return VMODE_MAX;
	if (frac)
		return VMODE_MAX;

	venc_index = venc_drv->vdata->default_venc_index;
	if (strcmp(venc_drv->vinfo->name, name) == 0)
		vmode = venc_drv->vinfo->mode;

	venc_drv->vinfo->viu_mux &= ~(0xf << 4);
	venc_drv->vinfo->viu_mux |= (venc_index << 4);

	return vmode;
}

static int dummy_encl_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DUMMY_ENCL)
		return true;

	return false;
}

static int dummy_encl_disable(enum vmode_e cur_vmod, void *data)
{
	struct dummy_venc_driver_s *venc_drv;
	unsigned int offset;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv || !venc_drv->vdata)
		return 0;
	if (!venc_drv->vdata->vconf)
		return 0;

	offset = venc_drv->vdata->vconf->venc_offset;

	venc_drv->status = 0;
	venc_drv->vout_valid = 0;

	vout_vcbus_write(ENCL_VIDEO_EN + offset, 0);
	dummy_encl_clk_ctrl(venc_drv, 0);
	if (venc_drv->vdata->encl_clk_gate_switch)
		venc_drv->vdata->encl_clk_gate_switch(venc_drv, 0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(venc_drv->vpu_dev);
	vpu_dev_clk_release(venc_drv->vpu_dev);
#endif

	VOUTPR("%s finished\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int dummy_encl_suspend(void *data)
{
	dummy_encl_disable(VMODE_DUMMY_ENCL, data);
	VOUTPR("%s finished\n", __func__);
	return 0;
}

static int dummy_encl_resume(void *data)
{
	dummy_encl_set_current_vmode(VMODE_DUMMY_ENCL, data);
	VOUTPR("%s finished\n", __func__);
	return 0;
}
#endif

static int dummy_encl_vout_state;
static int dummy_encl_vout_set_state(int bit, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv)
		return 0;

	dummy_encl_vout_state |= (1 << bit);
	venc_drv->viu_sel = bit;
	return 0;
}

static int dummy_encl_vout_clr_state(int bit, void *data)
{
	struct dummy_venc_driver_s *venc_drv;

	venc_drv = (struct dummy_venc_driver_s *)data;
	if (!venc_drv)
		return 0;

	dummy_encl_vout_state &= ~(1 << bit);
	if (venc_drv->viu_sel == bit)
		venc_drv->viu_sel = 0;
	return 0;
}

static int dummy_encl_vout_get_state(void *data)
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
};
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
static struct vout_server_s dummy_encl_vout3_server = {
	.name = "dummy_encl_vout3_server",
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
		.vout_suspend       = NULL,
		.vout_resume        = NULL,
#endif
	},
	.data = NULL,
};
#endif

static void dummy_encl_vout_server_init(struct dummy_venc_driver_s *venc_drv)
{
	venc_drv->vinfo = &dummy_encl_vinfo;
	dummy_encl_vout_server.data = (void *)venc_drv;
	vout_register_server(&dummy_encl_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	dummy_encl_vout2_server.data = (void *)venc_drv;
	vout2_register_server(&dummy_encl_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	dummy_encl_vout3_server.data = (void *)venc_drv;
	vout3_register_server(&dummy_encl_vout3_server);
#endif
}

static void dummy_encl_vout_server_remove(void)
{
	vout_unregister_server(&dummy_encl_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&dummy_encl_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_unregister_server(&dummy_encl_vout3_server);
#endif
}

/* ********************************************************* */
static int dummy_venc_vout_mode_update(struct dummy_venc_driver_s *venc_drv)
{
	enum vmode_e mode = VMODE_DUMMY_ENCP;

	if (!venc_drv)
		return -1;
	VOUTPR("%s\n", __func__);

	if (venc_drv->viu_sel == 1)
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	else if (venc_drv->viu_sel == 2)
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	dummy_encp_set_current_vmode(mode, (void *)venc_drv);
	if (venc_drv->viu_sel == 1)
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
	else if (venc_drv->viu_sel == 2)
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}

static int dummy_encp_vout_notify_callback(struct notifier_block *block,
					   unsigned long cmd, void *para)
{
	const struct vinfo_s *vinfo;

	if (!dummy_encp_drv)
		return -1;

	if (dummy_encp_drv->status == 0)
		return 0;
	if (dummy_encp_drv->vinfo_index == 0 || dummy_encp_drv->vinfo_index == 0xff)
		return 0;

	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		vinfo = get_current_vinfo();
		if (!vinfo)
			break;
		if (vinfo->mode != VMODE_LCD)
			break;
		dummy_venc_vout_mode_update(dummy_encp_drv);
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

	if (!dummy_encp_drv)
		return -1;

	if (dummy_encp_drv->status == 0)
		return 0;
	if (dummy_encp_drv->vinfo_index == 0 || dummy_encp_drv->vinfo_index == 0xff)
		return 0;

	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		vinfo = get_current_vinfo2();
		if (!vinfo)
			break;
		if (vinfo->mode != VMODE_LCD)
			break;
		dummy_venc_vout_mode_update(dummy_encp_drv);
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
	unsigned int offset, reg, i;

	if (!dummy_encp_drv || !dummy_encp_drv->vdata)
		return count;
	if (!dummy_encp_drv->vdata->vconf)
		return count;

	offset = dummy_encp_drv->vdata->vconf->venc_offset;

	switch (buf[0]) {
	case 'r':
		pr_info("dummy_encp index[%d] register dump:\n",
			dummy_encp_drv->vdata->vconf->venc_index);
		i = 0;
		while (i < ARRAY_SIZE(dummy_encp_reg_dump)) {
			if (dummy_encp_reg_dump[i] == 0xffff)
				break;
			reg = dummy_encp_reg_dump[i] + offset;
			pr_info("vcbus   [0x%04x] = 0x%08x\n",
				reg, vout_vcbus_read(reg));
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
	unsigned int offset, reg, i;

	if (!dummy_enci_drv || !dummy_enci_drv->vdata)
		return count;
	if (!dummy_enci_drv->vdata->vconf)
		return count;

	offset = dummy_enci_drv->vdata->vconf->venc_offset;

	switch (buf[0]) {
	case 'r':
		pr_info("dummy_enci index[%d] register dump:\n",
			dummy_enci_drv->vdata->vconf->venc_index);
		i = 0;
		while (i < ARRAY_SIZE(dummy_enci_reg_dump)) {
			if (dummy_enci_reg_dump[i] == 0xffff)
				break;
			reg = dummy_enci_reg_dump[i] + offset;
			pr_info("vcbus   [0x%04x] = 0x%08x\n",
				reg, vout_vcbus_read(reg));
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
	unsigned int offset, reg, i;

	if (!dummy_encl_drv || !dummy_encl_drv->vdata)
		return count;
	if (!dummy_encl_drv->vdata->vconf)
		return count;

	offset = dummy_encl_drv->vdata->vconf->venc_offset;

	switch (buf[0]) {
	case 'r':
		pr_info("dummy_encl index[%d] register dump:\n",
			dummy_encl_drv->vdata->vconf->venc_index);
		i = 0;
		while (i < ARRAY_SIZE(dummy_encl_reg_dump)) {
			if (dummy_encl_reg_dump[i] == 0xffff)
				break;
			reg = dummy_encl_reg_dump[i] + offset;
			pr_info("vcbus   [0x%04x] = 0x%08x\n",
				reg, vout_vcbus_read(reg));
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

/* ****************************************************
 * venc config
 * ****************************************************
 */
static struct venc_config_s dummy_venc_conf_dft = {
	.vid_clk_ctrl_reg = HHI_VID_CLK_CNTL,
	.vid_clk_ctrl2_reg = HHI_VID_CLK_CNTL2,
	.vid_clk_div_reg = HHI_VID_CLK_DIV,
	.vid2_clk_ctrl_reg = HHI_VIID_CLK_CNTL,
	.vid2_clk_div_reg = HHI_VIID_CLK_DIV,

	.venc_index = 0,
	.venc_offset = 0,
};

static struct venc_config_s dummy_venc_conf_sc2 = {
	.vid_clk_ctrl_reg = CLKCTRL_VID_CLK_CTRL,
	.vid_clk_ctrl2_reg = CLKCTRL_VID_CLK_CTRL2,
	.vid_clk_div_reg = CLKCTRL_VID_CLK_DIV,
	.vid2_clk_ctrl_reg = CLKCTRL_VIID_CLK_CTRL,
	.vid2_clk_div_reg = CLKCTRL_VIID_CLK_DIV,

	.venc_index = 0,
	.venc_offset = 0,
};

static struct venc_config_s dummy_venc_conf_s5 = {
	/* ENC0 */
	.vid_clk_ctrl_reg = CLKCTRL_VID_CLK0_CTRL,
	.vid_clk_ctrl2_reg = CLKCTRL_VID_CLK0_CTRL2,
	.vid_clk_div_reg = CLKCTRL_VID_CLK0_DIV,
	.vid2_clk_ctrl_reg = CLKCTRL_VID_CLK0_CTRL,
	.vid2_clk_div_reg = CLKCTRL_VID_CLK0_DIV,

	.venc_index = 0,
	.venc_offset = 0,
};

static struct venc_config_s dummy_venc_conf_t7_0 = {
	/* ENC0 */
	.vid_clk_ctrl_reg = CLKCTRL_VID_CLK0_CTRL,
	.vid_clk_ctrl2_reg = CLKCTRL_VID_CLK0_CTRL2,
	.vid_clk_div_reg = CLKCTRL_VID_CLK0_DIV,
	.vid2_clk_ctrl_reg = CLKCTRL_VIID_CLK0_CTRL,
	.vid2_clk_div_reg = CLKCTRL_VIID_CLK0_DIV,

	.venc_index = 0,
	.venc_offset = 0,
};

static struct venc_config_s dummy_venc_conf_t7_1 = {
	/* ENC1 */
	.vid_clk_ctrl_reg = CLKCTRL_VID_CLK1_CTRL,
	.vid_clk_ctrl2_reg = CLKCTRL_VID_CLK1_CTRL2,
	.vid_clk_div_reg = CLKCTRL_VID_CLK1_DIV,
	.vid2_clk_ctrl_reg = CLKCTRL_VIID_CLK1_CTRL,
	.vid2_clk_div_reg = CLKCTRL_VIID_CLK1_DIV,

	.venc_index = 1,
	.venc_offset = 0x600,
};

static struct venc_config_s dummy_venc_conf_t7_2 = {
	/* ENC2 */
	.vid_clk_ctrl_reg = CLKCTRL_VID_CLK2_CTRL,
	.vid_clk_ctrl2_reg = CLKCTRL_VID_CLK2_CTRL2,
	.vid_clk_div_reg = CLKCTRL_VID_CLK2_DIV,
	.vid2_clk_ctrl_reg = CLKCTRL_VIID_CLK2_CTRL,
	.vid2_clk_div_reg = CLKCTRL_VIID_CLK2_DIV,

	.venc_index = 2,
	.venc_offset = 0x800,
};

static struct venc_config_s dummy_venc_conf_t5w = {
	.vid_clk_ctrl_reg = HHI_VID_CLK0_CTRL,
	.vid_clk_ctrl2_reg = HHI_VID_CLK0_CTRL2,
	.vid_clk_div_reg = HHI_VID_CLK0_DIV,
	.vid2_clk_ctrl_reg = HHI_VIID_CLK0_CTRL,
	.vid2_clk_div_reg = HHI_VIID_CLK0_DIV,

	.venc_index = 0,
	.venc_offset = 0,
};

/* ****************************************************
 * dummy venc data
 * ****************************************************
 */
static struct dummy_venc_data_s dummy_venc_match_data_dft = {
	.vconf = &dummy_venc_conf_dft,

	.chip_type = DUMMY_VENC_DFT,
	.default_venc_index = 0,
	.projection_valid = 0,

	.clktree_probe = dummy_venc_clktree_probe,
	.clktree_remove = dummy_venc_clktree_remove,
	.encp_clk_gate_switch = dummy_encp_clk_gate_switch,
	.enci_clk_gate_switch = dummy_enci_clk_gate_switch,
	.encl_clk_gate_switch = dummy_encl_clk_gate_switch,
	.venc_sel = NULL,
};

static struct dummy_venc_data_s dummy_venc_match_data_sc2 = {
	.vconf = &dummy_venc_conf_sc2,

	.chip_type = DUMMY_VENC_SC2,
	.default_venc_index = 0,
	.projection_valid = 0,

	.clktree_probe = NULL,
	.clktree_remove = NULL,
	.encp_clk_gate_switch = NULL,
	.enci_clk_gate_switch = NULL,
	.encl_clk_gate_switch = NULL,
	.venc_sel = NULL,
};

static struct dummy_venc_data_s dummy_venc_match_data_t7 = {
	.vconf = &dummy_venc_conf_t7_0,

	.chip_type = DUMMY_VENC_T7,
	.default_venc_index = 0,
	.projection_valid = 1,

	.clktree_probe = NULL,
	.clktree_remove = NULL,
	.encp_clk_gate_switch = NULL,
	.enci_clk_gate_switch = NULL,
	.encl_clk_gate_switch = NULL,
	.venc_sel = dummy_venc_sel_t7,
};

static struct dummy_venc_data_s dummy_venc_match_data_t5w = {
	.vconf = &dummy_venc_conf_t5w,

	.chip_type = DUMMY_VENC_T5W,
	.default_venc_index = 0,
	.projection_valid = 1,

	.clktree_probe = NULL,
	.clktree_remove = NULL,
	.encp_clk_gate_switch = NULL,
	.enci_clk_gate_switch = NULL,
	.encl_clk_gate_switch = NULL,
	.venc_sel = dummy_venc_sel_t7,
};

static struct dummy_venc_data_s dummy_venc_match_data_s5 = {
	.vconf = &dummy_venc_conf_s5,

	.chip_type = DUMMY_VENC_S5,
	.default_venc_index = 0,
	.projection_valid = 0,

	.clktree_probe = NULL,
	.clktree_remove = NULL,
	.encp_clk_gate_switch = NULL,
	.enci_clk_gate_switch = NULL,
	.encl_clk_gate_switch = NULL,
};

static const struct of_device_id dummy_venc_dt_match_table[] = {
	{
		.compatible = "amlogic, dummy_venc",
		.data = &dummy_venc_match_data_dft,
	},
	{
		.compatible = "amlogic, dummy_venc_sc2",
		.data = &dummy_venc_match_data_sc2,
	},
	{
		.compatible = "amlogic, dummy_venc_s4",
		.data = &dummy_venc_match_data_sc2,
	},
	{
		.compatible = "amlogic, dummy_venc_t7",
		.data = &dummy_venc_match_data_t7,
	},
	{
		.compatible = "amlogic, dummy_venc_t3",
		.data = &dummy_venc_match_data_t7,
	},
	{
		.compatible = "amlogic, dummy_venc_t5w",
		.data = &dummy_venc_match_data_t5w,
	},
	{
		.compatible = "amlogic, dummy_venc_s5",
		.data = &dummy_venc_match_data_s5,
	},
	{}
};

static void dummy_venc_dts_config(struct device_node *of_node,
		struct dummy_venc_data_s *vdata)
{
	unsigned int temp;
	int ret;

	if (!of_node)
		return;
	if (!vdata)
		return;

	ret = of_property_read_u32(of_node, "default_venc_index", &temp);
	if (ret == 0) {
		vdata->default_venc_index = temp;
		switch (vdata->chip_type) {
		case DUMMY_VENC_T7:
			if (vdata->default_venc_index == 1)
				vdata->vconf = &dummy_venc_conf_t7_1;
			else if (vdata->default_venc_index == 2)
				vdata->vconf = &dummy_venc_conf_t7_2;
			else
				vdata->vconf = &dummy_venc_conf_t7_0;
			break;
		default:
			break;
		}
		VOUTPR("%s: default_venc_index = %d\n", __func__, temp);
	}
}

static int dummy_venc_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct dummy_venc_data_s *dummy_venc_data;

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

	dummy_venc_dts_config(pdev->dev.of_node, dummy_venc_data);

	dummy_encp_drv = kzalloc(sizeof(*dummy_encp_drv), GFP_KERNEL);
	if (!dummy_encp_drv) {
		VOUTERR("%s: dummy_venc driver no enough memory\n", __func__);
		return -ENOMEM;
	}
	dummy_encp_drv->vdata = dummy_venc_data;
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
	dummy_enci_drv->vdata = dummy_venc_data;
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
	dummy_encl_drv->vdata = dummy_venc_data;
	dummy_encl_drv->dev = &pdev->dev;
#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for encp*/
	dummy_encl_drv->vpu_dev = vpu_dev_register(VPU_VENCL, DUMMY_L_NAME);
#endif

	if (dummy_venc_data->clktree_probe)
		dummy_venc_data->clktree_probe(&pdev->dev);

	dummy_encp_vout_server_init(dummy_encp_drv);
	dummy_enci_vout_server_init(dummy_enci_drv);
	dummy_encl_vout_server_init(dummy_encl_drv);

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
	if (!dummy_encp_drv || !dummy_encp_drv->vdata)
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
	if (dummy_encp_drv->vdata->clktree_remove)
		dummy_encp_drv->vdata->clktree_remove(&pdev->dev);

	kfree(dummy_encp_drv);
	dummy_encp_drv = NULL;
	kfree(dummy_enci_drv);
	dummy_enci_drv = NULL;
	kfree(dummy_encl_drv);
	dummy_encl_drv = NULL;

	VOUTPR("%s\n", __func__);
	return 0;
}

static int dummy_venc_resume(struct platform_device *pdev)
{
	if (dummy_encp_drv && dummy_encp_drv->vout_valid)
		dummy_encp_resume((void *)dummy_encp_drv);
	if (dummy_enci_drv && dummy_enci_drv->vout_valid)
		dummy_enci_resume((void *)dummy_enci_drv);
	if (dummy_encl_drv && dummy_encl_drv->vout_valid)
		dummy_encl_resume((void *)dummy_encl_drv);

	return 0;
}

static int dummy_venc_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (dummy_encp_drv && dummy_encp_drv->status)
		dummy_encp_suspend((void *)dummy_encp_drv);
	if (dummy_enci_drv && dummy_enci_drv->status)
		dummy_enci_suspend((void *)dummy_enci_drv);
	if (dummy_encl_drv && dummy_encl_drv->status)
		dummy_encl_suspend((void *)dummy_encl_drv);

	return 0;
}

static void dummy_venc_shutdown(struct platform_device *pdev)
{
	if (!dummy_encp_drv)
		return;
	if (dummy_encp_drv->status)
		dummy_encp_disable(VMODE_DUMMY_ENCP, (void *)dummy_encp_drv);

	if (!dummy_enci_drv)
		return;
	if (dummy_enci_drv->status)
		dummy_enci_disable(VMODE_DUMMY_ENCI, (void *)dummy_enci_drv);

	if (!dummy_encl_drv)
		return;
	if (dummy_encl_drv->status)
		dummy_encl_disable(VMODE_DUMMY_ENCL, (void *)dummy_encl_drv);
}

static struct platform_driver dummy_venc_platform_driver = {
	.probe = dummy_venc_probe,
	.remove = dummy_venc_remove,
	.suspend = dummy_venc_suspend,
	.resume = dummy_venc_resume,
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

