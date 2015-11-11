/*
 * drivers/amlogic/display/vout2/tv_vout2.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/


/* Linux Headers */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/major.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/vinfo.h>
#ifdef CONFIG_AML_VPU
#include <linux/amlogic/vpu.h>
#endif

/* Local Headers */
#include <vout/vout_log.h>
#include <vout/vout_reg.h>
#include <vout/vout_io.h>
#include "tv_vout2.h"
#include "tvregs.h"

/* #include <linux/clk.h> */
/* #include <mach/mod_gate.h> */

static void parse_vdac_setting(char *para);
SET_TV2_CLASS_ATTR(vdac_setting, parse_vdac_setting)

static struct disp_module_info_s *info;
static u32 curr_vdac_setting = DEFAULT_VDAC_SEQUENCE;

int get_current_vdac_setting2(void)
{
	return curr_vdac_setting;
}

/* extern unsigned int clk_util_clk_msr(unsigned int clk_mux); */
void change_vdac_setting2(unsigned int vdec_setting, enum vmode_e  mode)
{
	unsigned int signal_set_index = 0;
	unsigned int idx = 0, bit = 5, i;

	switch (mode) {
	case VMODE_480I:
	case VMODE_576I:
		signal_set_index = 0;
		bit = 5;
		break;

	case VMODE_480CVBS:
	case VMODE_576CVBS:
		signal_set_index = 1;
		bit = 2;
		break;

	default:
		signal_set_index = 2;
		bit = 5;
		break;
	}

	for (i = 0; i < 3; i++) {
		idx = vdec_setting >> (bit << 2) & 0xf;
		vout_log_dbg("dac index:%d ,signal:%s\n", idx,
			     signal_table[signal_set[signal_set_index][i]]);
		vout_vcbus_write(VENC_VDAC_DACSEL0 + idx,
				signal_set[signal_set_index][i]);
		bit--;
	}

	curr_vdac_setting = vdec_setting;
}

static const struct reg_s *tvregs_get_clk_setting_by_mode(enum tvmode_e mode)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(tvregsTab2); i++) {
		if (mode == tvregsTab2[i].tvmode)
			return tvregsTab2[i].clk_reg_setting;
	}

	return NULL;
}

static const struct reg_s *tvregs_get_enc_setting_by_mode(enum tvmode_e mode)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(tvregsTab2); i++) {
		if (mode == tvregsTab2[i].tvmode)
			return tvregsTab2[i].enc_reg_setting;
	}

	return NULL;
}

static const struct tvinfo_s *tvinfo_mode(enum tvmode_e mode)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(tvinfoTab2); i++) {
		if (mode == tvinfoTab2[i].tvmode)
			return &tvinfoTab2[i];
	}

	return NULL;
}

int tvoutc_setmode2(enum tvmode_e mode)
{
	const struct reg_s *s;
	const struct tvinfo_s *tvinfo;

	if (mode >= TVMODE_MAX) {
		vout_log_err("Invalid video output modes.\n");
		return -ENODEV;
	}

	vout_log_dbg("TV mode %s selected.\n", tvinfoTab2[mode].id);
	tvinfo = tvinfo_mode(mode);

	if (!tvinfo) {
		vout_log_err("tvinfo %d not find\n", mode);
		return 0;
	}

	vout_log_dbg("TV mode %s selected.\n", tvinfo->id);


	/* init encoding */
	vout_vcbus_write(VENC_VDAC_SETTING, 0xff);

	/* setup clock regs */
	s = tvregs_get_clk_setting_by_mode(mode);

	if (!s) {
		vout_log_info("display mode %d get clk set NULL\n", mode);
	} else {
		while (MREG_END_MARKER != s->reg) {
			vout_cbus_write(s->reg, s->val);
			s++;
		}
	}

	/* setup encoding regs */
	s = tvregs_get_enc_setting_by_mode(mode);

	if (!s) {
		vout_log_info("display mode %d get enc set NULL\n", mode);
		return 0;
	} else {
		while (MREG_END_MARKER != s->reg) {
			vout_vcbus_write(s->reg, s->val);
			s++;
		}
	}

	/* enable_vsync_interrupt(); */
	switch (mode) {
	case TVMODE_480I:
	case TVMODE_480I_RPT:
	case TVMODE_480CVBS:
	case TVMODE_480P:
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	case TVMODE_480P_59HZ:
#endif
	case TVMODE_480P_RPT:
	case TVMODE_576I:
	case TVMODE_576CVBS:
	case TVMODE_576P:
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
			/* reg0x271a, select ENCT to VIU2 */
			vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL, 1, 2, 2);
			vout_cbus_set_bits(HHI_VIID_DIVIDER_CNTL, 0x03, 0, 2);
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL5, 1, 16, 1);
			/* --set vid2 pll < 1.7G */
			vout_cbus_write(HHI_VID2_PLL_CNTL, 0x41000032);
			vout_cbus_write(HHI_VID2_PLL_CNTL2, 0x0421a000);
			vout_cbus_write(HHI_VID2_PLL_CNTL3, 0xca45b823);
			vout_cbus_write(HHI_VID2_PLL_CNTL4, 0xd4000d67);
			vout_cbus_write(HHI_VID2_PLL_CNTL5, 0x01700001);
			/* clock tree N=1 */
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL, 0x01, 24, 5);
			/* clock tree M=54 */
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL, 0x36, 0, 9);
			/* clock tree OD=0 */
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL, 0x00, 9, 2);
			/* select CLK1 = 24x54 = 1296M */
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL5, 0x01, 24, 1);
			vout_cbus_set_bits(HHI_DSI_LVDS_EDP_CNTL1, 0x00, 4, 1);
			vout_cbus_set_bits(HHI_VIID_DIVIDER_CNTL, 0x03, 15, 2);
			/* select 6 */
			vout_cbus_set_bits(HHI_VIID_DIVIDER_CNTL, 0x05, 4, 3);
			/* select 0 */
			vout_cbus_set_bits(HHI_VIID_DIVIDER_CNTL, 0x00, 8, 2);
			/* select 0 clock = 1296/6 = 216M */
			vout_cbus_set_bits(HHI_VIID_DIVIDER_CNTL, 0x00, 12, 3);
			/* select vid2_pll_clk & open bit19 */
			vout_cbus_set_bits(HHI_VIID_CLK_CNTL, 0x0c, 16, 4);
			/* select 4 clock = 216/4 = 54M */
			vout_cbus_set_bits(HHI_VIID_CLK_DIV, 0x03, 0, 4);
			/* open div1 div2 div4 */
			vout_cbus_set_bits(HHI_VIID_CLK_CNTL, 0x07, 0, 5);
			/*
			 * reg 0x1059,
			 * select v2_clk_div2 for cts_enci_clk, 27MHz
			 */
			vout_cbus_set_bits(HHI_VID_CLK_DIV, 0x09, 28, 4);
			/* 0x104a, select v2_clk_div2 for cts_vdac_clk[0] */
			vout_cbus_set_bits(HHI_VIID_CLK_DIV, 0x09, 28, 4);
			/* 0x104a, select v2_clk_div2 for cts_vdac_clk[1] */
			vout_cbus_set_bits(HHI_VIID_CLK_DIV, 0x09, 24, 4);
			/* reset */
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL, 0x01, 29, 1);
			/* clean reset */
			vout_cbus_set_bits(HHI_VID2_PLL_CNTL, 0x00, 29, 1);
		}
		break;
	default:
		break;
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		vout_cbus_write(HHI_VDAC_CNTL0, 0x00000001);
		vout_cbus_write(HHI_VDAC_CNTL1, 0x00000000);
		vout_vcbus_write(ENCI_MACV_N0, 0x00000000);
		vout_cbus_write(HHI_GCLK_OTHER,
				vout_cbus_read(HHI_GCLK_OTHER) | (1 << 8));
	}
	vout_vcbus_write(VPP2_POSTBLEND_H_SIZE, tvinfo->xres);

	/* For debug only */
#if 0
	vout_log_dbg(" clk_util_clk_msr 6 = %d\n", clk_util_clk_msr(6));
	vout_log_dbg(" clk_util_clk_msr 7 = %d\n", clk_util_clk_msr(7));
	vout_log_dbg(" clk_util_clk_msr 8 = %d\n", clk_util_clk_msr(8));
	vout_log_dbg(" clk_util_clk_msr 9 = %d\n", clk_util_clk_msr(9));
	vout_log_dbg(" clk_util_clk_msr 10 = %d\n", clk_util_clk_msr(10));
	vout_log_dbg(" clk_util_clk_msr 27 = %d\n", clk_util_clk_msr(27));
	vout_log_dbg(" clk_util_clk_msr 29 = %d\n", clk_util_clk_msr(29));
#endif
	return 0;
}

static const struct file_operations am_tv_fops = {
	.open	= NULL,
	.read	= NULL,/* am_tv_read, */
	.write	= NULL,
	.unlocked_ioctl	= NULL,/* am_tv_ioctl, */
	.release	= NULL,
	.poll		= NULL,
};

static const struct vinfo_s *get_valid_vinfo(char *mode)
{
	const struct vinfo_s *vinfo = NULL;
	int  i, count = ARRAY_SIZE(tv_info);
	int mode_name_len = 0;

	for (i = 0; i < count; i++) {
		if (strncmp(tv_info[i].name, mode,
			    strlen(tv_info[i].name)) == 0) {
			if ((vinfo == NULL)
			    || (strlen(tv_info[i].name) > mode_name_len)) {
				vinfo = &tv_info[i];
				mode_name_len = strlen(tv_info[i].name);
			}
		}
	}

	return vinfo;
}

static const struct vinfo_s *tv_get_current_info(void)
{
	return info->vinfo;
}

#ifdef CONFIG_AML_VPU
static int tv_out_enci_is_required(enum vmode_e mode)
{
	if ((mode == VMODE_576I) ||
		(mode == VMODE_576I_RPT) ||
		(mode == VMODE_480I) ||
		(mode == VMODE_480I_RPT) ||
		(mode == VMODE_576CVBS) ||
		(mode == VMODE_480CVBS))
		return 1;
	return 0;
}

static void tv_out_vpu_power_ctrl(int status)
{
	int vpu_mod;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8)
		return;

	if (info->vinfo == NULL)
		return;
	vpu_mod = tv_out_enci_is_required(info->vinfo->mode);
	vpu_mod = (vpu_mod) ? VPU_VENCI : VPU_VENCP;
	if (status) {
		request_vpu_clk_vmod(info->vinfo->video_clk, vpu_mod);
		switch_vpu_mem_pd_vmod(vpu_mod, VPU_MEM_POWER_ON);
	} else {
		switch_vpu_mem_pd_vmod(vpu_mod, VPU_MEM_POWER_DOWN);
		release_vpu_clk_vmod(vpu_mod);
	}
}
#endif

static int tv_set_current_vmode(enum vmode_e mod)
{
	if ((mod & VMODE_MODE_BIT_MASK) > VMODE_SXGA)
		return -EINVAL;

	info->vinfo = &tv_info[mod];

	if (mod & VMODE_INIT_BIT_MASK)
		return 0;

#ifdef CONFIG_AML_VPU
	tv_out_vpu_power_ctrl(1);
#endif

	tvoutc_setmode2(vmode_tvmode_tab[mod]);
	/* change_vdac_setting2(get_current_vdac_setting(),mod); */
	return 0;
}

static enum vmode_e tv_validate_vmode(char *mode)
{
	const struct vinfo_s *info = get_valid_vinfo(mode);

	if (info)
		return info->mode;

	return VMODE_MAX;
}
static int tv_vmode_is_supported(enum vmode_e mode)
{
	int  i, count = ARRAY_SIZE(tv_info);
	mode &= VMODE_MODE_BIT_MASK;

	for (i = 0; i < count; i++) {
		if (tv_info[i].mode == mode)
			return true;
	}

	return false;
}
static int tv_module_disable(enum vmode_e cur_vmod)
{
#ifdef CONFIG_AML_VPU
	tv_out_vpu_power_ctrl(0);
#endif

	/* video_dac_disable(); */
	return 0;
}
#ifdef CONFIG_PM
static int tv_suspend(void)
{
	/* video_dac_disable(); */
	return 0;
}
static int tv_resume(void)
{
	/* TODO */
	/* video_dac_enable(0xff); */
	tv_set_current_vmode(info->vinfo->mode);
	return 0;
}
#endif

static struct vout_server_s tv_server = {
	.name = "vout2_tv_server",
	.op = {
		.get_vinfo = tv_get_current_info,
		.set_vmode = tv_set_current_vmode,
		.validate_vmode = tv_validate_vmode,
		.vmode_is_supported = tv_vmode_is_supported,
		.disable = tv_module_disable,
#ifdef CONFIG_PM
		.vout_suspend = tv_suspend,
		.vout_resume = tv_resume,
#endif
	},
};

/***************************************************
**
**	The first digit control component Y output DAC number
**	The 2nd digit control component U output DAC number
**	The 3rd digit control component V output DAC number
**	The 4th digit control composite CVBS output DAC number
**	The 5th digit control s-video Luma output DAC number
**	The 6th digit control s-video chroma output DAC number
**	examble :
**		echo  120120 > /sys/class/display/vdac_setting
**		the first digit from the left side .
******************************************************/
static void  parse_vdac_setting(char *para)
{
	int  i;
	char  *pt = strstrip(para);
	int len = strlen(pt);
	u32  vdac_sequence = get_current_vdac_setting2();

	vout_log_dbg("origin vdac setting:0x%x,strlen:%d\n",
			vdac_sequence, len);

	if (len != 6) {
		vout_log_err("can't parse vdac settings\n");
		return;
	}

	vdac_sequence = 0;

	for (i = 0; i < 6; i++) {
		vdac_sequence <<= 4;
		vdac_sequence |= *pt - '0';
		pt++;
	}

	vout_log_dbg("current vdac setting:0x%x\n", vdac_sequence);
	change_vdac_setting2(vdac_sequence, get_current_vmode2());
}
static  struct  class_attribute   *tv_attr[] = {
	&class_TV2_attr_vdac_setting,
};
static int  create_tv_attr(struct disp_module_info_s *info)
{
	/* create base class for display */
	int  i;

	info->base_class = class_create(THIS_MODULE, info->name);

	if (IS_ERR(info->base_class)) {
		vout_log_info("create tv display class fail\n");
		return  -1;
	}

	/* create  class attr */
	for (i = 0; i < ARRAY_SIZE(tv_attr); i++) {
		if (class_create_file(info->base_class, tv_attr[i]))
			vout_log_info("create disp attribute %s fail\n",
					tv_attr[i]->attr.name);
	}

	sprintf(vdac_setting, "%x", get_current_vdac_setting2());
	return   0;
}

static int __init tv_init_module(void)
{
	int  ret;

	info = kmalloc(sizeof(struct disp_module_info_s), GFP_KERNEL);

	if (!info) {
		vout_log_info("can't alloc display info struct\n");
		return -ENOMEM;
	}

	memset(info, 0, sizeof(struct disp_module_info_s));

	sprintf(info->name, TV_CLASS_NAME);
	ret = register_chrdev(0, info->name, &am_tv_fops);

	if (ret < 0) {
		vout_log_info("register char dev tv2 error\n");
		return  ret;
	}

	info->major = ret;
	vout_log_info("major number %d for disp\n", ret);

	if (vout2_register_server(&tv_server))
		vout_log_info("register tv module server fail\n");
	else
		vout_log_info("register tv module server ok\n");

	create_tv_attr(info);
	return 0;
}

static __exit void tv_exit_module(void)
{
	int i;

	if (info->base_class) {
		for (i = 0; i < ARRAY_SIZE(tv_attr); i++)
			class_remove_file(info->base_class, tv_attr[i]);

		class_destroy(info->base_class);
	}

	if (info) {
		unregister_chrdev(info->major, info->name);
		kfree(info);
	}

	vout2_unregister_server(&tv_server);

	vout_log_info("exit tv module\n");
}

arch_initcall(tv_init_module);
module_exit(tv_exit_module);

MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
MODULE_DESCRIPTION("TV Output2 Module");
MODULE_LICENSE("GPL");
