// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/pattern_detection.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/module.h>

/* module headers */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
#include "../../vin/tvin/tvafe/tvafe.h"
#include "../../vin/tvin/tvafe/tvafe_vbi.h"
#endif
#include "arch/vpp_regs.h"
#include "amve.h"
#include "reg_helper.h"
#include "pattern_detection.h"
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
#include "pattern_detection_bar_settings.h"
#include "pattern_detection_face_settings.h"
#include "pattern_detection_corn_settings.h"
#endif

#define PATTERN_INDEX_MAX    PATTERN_MULTICAST

int pattern_detect_debug;
#define pr_pattern_detect_dbg(fmt, args...)\
	do {\
		if (pattern_detect_debug > 0)\
			pr_info("Detect debug: [%s]: " fmt, __func__, ## args);\
	} while (0)

int enable_pattern_detect = 1;
int detected_pattern = PATTERN_UNKNOWN;
int last_detected_pattern = PATTERN_UNKNOWN;
uint pattern_mask = PATTERN_MASK(PATTERN_75COLORBAR) |
					PATTERN_MASK(PATTERN_SKIN_TONE_FACE) |
					PATTERN_MASK(PATTERN_GREEN_CORN) |
					PATTERN_MASK(PATTERN_MULTICAST);

static uint pattern_param = PATTERN_PARAM_COUNT;

static uint pattern0_param_info[PATTERN_PARAM_COUNT] = {
	5, 100, 830, 30, 110, 0x70000, 0xd0000, 0x158000,
	400, 40, 650, 50, 0x300000, 0x500000, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static uint pattern1_param_info[PATTERN_PARAM_COUNT] = {
	850, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static uint pattern2_param_info[PATTERN_PARAM_COUNT] = {
	80, 730, 180, 6, /* pal hue hist 14 ~ 17 */
	80, 510, 340, 70, /* ntsc hue hist 14 ~ 17 */
	59, 112, 133, 223, /* pal sat hist 0 ~ 3 */
	290, 165, 16, 1, /* pal sat hist 4 ~ 7 */
	83, 143, 204, 316, /* ntsc sat hist 0 ~ 3 */
	221, 28, 3, 0, /* ntsc sat hist 4 ~ 7 */
	50, 55, 900, 0,
	0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/*saturation hist*/
static uint pattern3_param_info[PATTERN_PARAM_COUNT] = {
	0x7e9000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

module_param_array(pattern0_param_info, uint,
		   &pattern_param, 0664);
MODULE_PARM_DESC(pattern0_param_info, "\n pattern0_param_info\n");

module_param_array(pattern1_param_info, uint,
		   &pattern_param, 0664);
MODULE_PARM_DESC(pattern1_param_info, "\n pattern1_param_info\n");

module_param_array(pattern2_param_info, uint,
		   &pattern_param, 0664);
MODULE_PARM_DESC(pattern2_param_info, "\n pattern2_param_info\n");

unsigned int mltcast_ratio1 = 9;
unsigned int mltcast_ratio2 = 8;
int mltcast_skip_en;

static int default_pattern0_checker(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("check for pattern0\n");
	return 0;
}

static int default_pattern1_checker(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("check for pattern1\n");
	return 0;
}

static int default_pattern2_checker(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("check for pattern2\n");
	return 0;
}

static int default_pattern3_checker(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("check for pattern3\n");
	return 0;
}

static int default_pattern0_handler(struct vframe_s *vf, int flag)
{
	pr_pattern_detect_dbg("pattern0 detected and handled\n");
	return 0;
}

static int default_pattern1_handler(struct vframe_s *vf, int flag)
{
	pr_pattern_detect_dbg("pattern1 detected and handled\n");
	return 0;
}

static int default_pattern2_handler(struct vframe_s *vf, int flag)
{
	pr_pattern_detect_dbg("pattern2 detected and handled\n");
	return 0;
}

static int default_pattern3_handler(struct vframe_s *vf, int flag)
{
	pr_pattern_detect_dbg("pattern3 detected and handled\n");
	return 0;
}

static int default_pattern0_defaultloader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("pattern0 load default setting\n");
	return 0;
}

static int default_pattern1_defaultloader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("pattern1 load default setting\n");
	return 0;
}

static int default_pattern2_defaultloader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("pattern2 load default setting\n");
	return 0;
}

static int default_pattern3_defaultloader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("pattern3 load default setting\n");
	return 0;
}

static int pattern_index;
static struct pattern pattern_list[] = {
	{
		PATTERN_75COLORBAR,
		pattern0_param_info,
		NULL,
		NULL,
		default_pattern0_checker,
		default_pattern0_defaultloader,
		default_pattern0_handler,
	},
	{
		PATTERN_SKIN_TONE_FACE,
		pattern1_param_info,
		NULL,
		NULL,
		default_pattern1_checker,
		default_pattern1_defaultloader,
		default_pattern1_handler,
	},
	{
		PATTERN_GREEN_CORN,
		pattern2_param_info,
		NULL,
		NULL,
		default_pattern2_checker,
		default_pattern2_defaultloader,
		default_pattern2_handler,
	},
	{
		PATTERN_MULTICAST,
		pattern3_param_info,
		NULL,
		NULL,
		default_pattern3_checker,
		default_pattern3_defaultloader,
		default_pattern3_handler,
	},
};

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
static void tvafe_get_default_regmap(struct am_regs_s *p)
{
	unsigned short i;

	for (i = 0; i < p->length; i++) {
		switch (p->am_reg[i].type) {
		case REG_TYPE_APB:
			tvafe_reg_read(p->am_reg[i].addr << 2,
				       &p->am_reg[i].val);
			p->am_reg[i].val = p->am_reg[i].val &
					   p->am_reg[i].mask;
			break;
		default:
			break;
		}
		pr_info("Detect debug: [%s]: tvafe read default 0x%x = 0x%x\n",
			__func__, p->am_reg[i].addr,
			p->am_reg[i].val);
	}
}

static void am_get_default_regmap(struct am_regs_s *p)
{
	unsigned short i;

	for (i = 0; i < p->length; i++) {
		switch (p->am_reg[i].type) {
		case REG_TYPE_CBUS:
			p->am_reg[i].val =
			aml_read_cbus(p->am_reg[i].addr) &
					p->am_reg[i].mask;
			break;
		case REG_TYPE_INDEX_VPP_COEF:
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				      p->am_reg[i].addr);
			p->am_reg[i].val =
				READ_VPP_REG(VPP_CHROMA_DATA_PORT) &
				p->am_reg[i].mask;
			break;
		case REG_TYPE_OFFSET_VCBUS:
			p->am_reg[i].val =
				READ_VPP_REG(p->am_reg[i].addr) &
				p->am_reg[i].mask;
			break;
		default:
			break;
		}
		pr_info("Detect debug: [%s]: vpp read default 0x%x = 0x%x\n",
			__func__, p->am_reg[i].addr,
			p->am_reg[i].val);
	}
}

static void defaultsetting_get_regmap(int index)
{
	struct setting_regs_s *cvd2_set =
			pattern_list[pattern_index].cvd2_settings;
	struct setting_regs_s *vpp_set =
			pattern_list[pattern_index].vpp_settings;
	struct am_regs_s *regs;

	pr_pattern_detect_dbg("enter\n");

	if (cvd2_set && cvd2_set[index].length > 0) {
		/* default setting is not ready, need to update it */
		if (cvd2_set[index].am_reg[0].val == 0xffffffff) {
			regs = (struct am_regs_s *)(&cvd2_set[index]);
			tvafe_get_default_regmap(regs);
		}
	}

	if (vpp_set && vpp_set[index].length > 0) {
		/* default setting is not ready, need to update it */
		if (vpp_set[index].am_reg[0].val == 0xffffffff) {
			regs = (struct am_regs_s *)(&vpp_set[index]);
			am_get_default_regmap(regs);
		}
	}
	pr_pattern_detect_dbg("leave\n");
}

static void setting_set_regmap(int index)
{
	struct setting_regs_s *cvd2_set =
				pattern_list[pattern_index].cvd2_settings;
	struct setting_regs_s *vpp_set =
				pattern_list[pattern_index].vpp_settings;
	struct am_regs_s *regs;

	pr_pattern_detect_dbg("enter\n");

	/* check if default setting is ready*/

	if (cvd2_set) {
		regs = (struct am_regs_s *)(&cvd2_set[index]);
		tvafe_set_regmap(regs);
	}

	if (vpp_set) {
		regs = (struct am_regs_s *)(&vpp_set[index]);
		am_set_regmap(regs);
	}

	pr_pattern_detect_dbg("leave\n");
}

static int colorbar_hist_checker(struct vframe_s *vf)
{
	/* check cm2 hist and dnlp apl value */
	int flag = -1;
	int hue_hist[32], y_hist_total;
	int i, apl, hist_total = 0;
	int peak_hue_bin_sum = 0, bin2021, bin2526, bin32;
	int apl_diff, bin1, bin2, bin5, bin910, bin16;
	int bin8, bin24, bin26, bin3132, bin17;
	int tolerance_zero, tolerance_hue;
	int tolerance_hue2;
	int tolerance_hue3a, tolerance_hue3b;
	int tolerance_hue5;
	int tolerance_hue_all, tolerance_luma, target_apl;
	unsigned int tvafe_val, tolerance_tvafe, max_tvafe;
	int bin56, bin1718, bin2122;
	int peak_hue_bin_sum4 = 0;
	int peak_hue_bin_sum5 = 0;

	tolerance_zero = pattern_list[pattern_index].pattern_param[0];
	tolerance_hue = pattern_list[pattern_index].pattern_param[1];
	tolerance_hue_all = pattern_list[pattern_index].pattern_param[2];
	tolerance_luma = pattern_list[pattern_index].pattern_param[3];
	target_apl = pattern_list[pattern_index].pattern_param[4];
	tolerance_tvafe = pattern_list[pattern_index].pattern_param[5];
	tolerance_hue2 = pattern_list[pattern_index].pattern_param[8];
	tolerance_hue3a = pattern_list[pattern_index].pattern_param[9];
	tolerance_hue3b = pattern_list[pattern_index].pattern_param[10];
	tolerance_hue5 = pattern_list[pattern_index].pattern_param[11];
	max_tvafe = pattern_list[pattern_index].pattern_param[13];

	if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
	    vf->source_mode == VFRAME_SOURCE_MODE_PAL) {
		tolerance_tvafe = pattern_list[pattern_index].pattern_param[7];
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		   vf->source_mode == VFRAME_SOURCE_MODE_NTSC) {
		tolerance_tvafe = pattern_list[pattern_index].pattern_param[5];
		max_tvafe = pattern_list[pattern_index].pattern_param[12];
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		   vf->source_mode == VFRAME_SOURCE_MODE_PAL) {
		tolerance_tvafe = pattern_list[pattern_index].pattern_param[6];
		max_tvafe = pattern_list[pattern_index].pattern_param[13];
	}

	if (vf->source_type != VFRAME_SOURCE_TYPE_TUNER &&
	    vf->source_type != VFRAME_SOURCE_TYPE_CVBS)
		return flag;

	if (tvafe_clk_status == 0)
		return flag;

	flag = 0;

	/* check cm2 hue/sat hist */
	for (i = 0; i <= 31; i++) {
		hue_hist[i] = vf->prop.hist.vpp_hue_gamma[i];
		hist_total += hue_hist[i];
	}
	hist_total = hist_total / 1000;
	for (i = 0; i <= 31; i++) {
		hue_hist[i] = hue_hist[i] / hist_total;
		pr_pattern_detect_dbg("read hue_hist[%d]=%d, origin=%d\n",
				      i,
				      (int)hue_hist[i],
				      vf->prop.hist.vpp_hue_gamma[i]);
	}

	/*
	 * check hue hist bin 5, bin 9+10, bin 16
	 * bin 20+21, bin 25+26, bin 32 and sum
	 */
	bin1 = hue_hist[0];
	bin2 = hue_hist[1];
	bin5 = hue_hist[3] + hue_hist[4];
	bin56 = hue_hist[4] + hue_hist[5];
	bin8 = hue_hist[7];
	bin910 = hue_hist[8] + hue_hist[9] + hue_hist[10];
	bin16 = hue_hist[15];
	bin17 = hue_hist[16];
	bin1718 = hue_hist[16] + hue_hist[17];
	bin2021 = hue_hist[19] + hue_hist[20];
	bin2122 = hue_hist[20] + hue_hist[21];
	bin24 = hue_hist[23];
	bin2526 = hue_hist[24] + hue_hist[25] + hue_hist[26];
	bin26 = hue_hist[25];
	bin32 = hue_hist[31];
	bin3132 = hue_hist[29] + hue_hist[30] + hue_hist[31];
	peak_hue_bin_sum = hue_hist[1] + hue_hist[4] + hue_hist[8] +
				hue_hist[9] + hue_hist[15] + hue_hist[19] +
				hue_hist[20] + hue_hist[24] + hue_hist[25] +
				hue_hist[31] + hue_hist[3] + hue_hist[10] +
				hue_hist[26];
	peak_hue_bin_sum4 = bin2 + bin56 + bin910 +
				bin1718 + bin2122 + bin2526;
	peak_hue_bin_sum5 = bin1 + bin5 + bin8 +
				bin910 + bin16 + bin2021 + bin24 +
				bin26 + bin3132 + bin17;
	pr_pattern_detect_dbg("bin2=%d, bin5=%d, bin910=%d, bin16=%d\n",
			      bin2, bin5, bin910, bin16);
	pr_pattern_detect_dbg("bin2021=%d, bin2526=%d, bin32=%d\n",
			      bin2021, bin2526, bin32);
	pr_pattern_detect_dbg("peak_hue_bin_sum=%d\n",
			      peak_hue_bin_sum);

	/* check apl */
	apl = vf->prop.hist.luma_sum /
		(vf->prop.hist.height * vf->prop.hist.width);
	apl_diff = (apl - target_apl) * 1000 / target_apl;
	pr_pattern_detect_dbg("apl=%d, target_apl=%d\n",
			      apl, target_apl);

	/* check high 4 bin sum of dnlp histogram */
	y_hist_total = 0;
	for (i = 60; i <= 63; i++)
		y_hist_total += vf->prop.hist.vpp_gamma[i];
	pr_pattern_detect_dbg("high 4 luma bin sum=%d\n",
			      y_hist_total);

	/* read tvafe status register to check mixed pattern*/
	tvafe_reg_read(0x18d << 2, &tvafe_val);
	pr_pattern_detect_dbg("tvafe status reg val=0x%x, check value= 0x%x\n",
			      tvafe_val, tolerance_tvafe);

	/* judge type 1, normal / mixed colorbar 7 color */
	if (peak_hue_bin_sum >= tolerance_hue_all &&
	    bin5 >= tolerance_hue &&
	    bin910 >=  tolerance_hue &&
	    bin16 >=  tolerance_hue &&
	    bin2021 >=  tolerance_hue &&
	    bin2526 >=  tolerance_hue &&
	    bin32 >=  tolerance_hue) {
		if (tvafe_val <= tolerance_tvafe) {
			pr_pattern_detect_dbg("judge1 colorbar detected\n");
			flag = 2;
		} else if (tvafe_val < max_tvafe) {
			pr_pattern_detect_dbg("judge1 mixcolorbar detected\n");
			flag = 1;
		}
	}
	if (flag > 0)
		goto judge_finish;

	/* judge type 2, 4 grid, pink and blue*/
	/* judge2 is removed */

	/* judge type 3, ega-64blue color */
	pr_pattern_detect_dbg("judge3......\n");
	if (peak_hue_bin_sum >= tolerance_hue_all &&
	    bin5 >= tolerance_hue3a &&
	    bin910 >=  tolerance_hue3a &&
	    bin16 >=  tolerance_hue3a &&
	    bin2021 >=  tolerance_hue3a &&
	    bin2526 >=  tolerance_hue3a &&
	    bin32 >=  tolerance_hue3b) {
		pr_pattern_detect_dbg("judge3 ega-64blue pattern detected\n");
		flag = 2;
	}
	if (flag > 0)
		goto judge_finish;

	/* judge type 4, BDGH pattern */
	pr_pattern_detect_dbg("judge4......\n");
	pr_pattern_detect_dbg("bin2=%d, bin56=%d, bin910=%d, bin1718=%d\n",
			      bin2, bin56, bin910, bin1718);
	pr_pattern_detect_dbg("bin2122=%d, bin2526=%d\n",
			      bin2122, bin2526);
	pr_pattern_detect_dbg("peak_hue_bin_sum4=%d\n",
			      peak_hue_bin_sum4);
	if (peak_hue_bin_sum4 >= tolerance_hue_all &&
	    bin56 >= tolerance_hue &&
	    bin910 >=  tolerance_hue &&
	    bin1718 >=  tolerance_hue &&
	    bin2122 >=  tolerance_hue &&
	    bin2526 >=  tolerance_hue &&
	    bin2 >=  tolerance_hue) {
		pr_pattern_detect_dbg("judge4 BDGH pattern detected\n");
		flag = 2;
	}
	if (flag > 0)
		goto judge_finish;

	/* judge type 5, color BDGH pattern */
	pr_pattern_detect_dbg("judge5......\n");
	pr_pattern_detect_dbg("bin1=%d, bin5=%d, bin8=%d, bin910=%d\n",
			      bin1, bin5, bin8, bin910);
	pr_pattern_detect_dbg("bin16=%d, bin2021=%d, bin24=%d\n",
			      bin16, bin2021, bin24);
	pr_pattern_detect_dbg("bin26=%d, bin3132=%d\n",
			      bin26, bin3132);
	pr_pattern_detect_dbg("peak_hue_bin_sum5=%d\n",
			      peak_hue_bin_sum5);
	if (peak_hue_bin_sum5 >= tolerance_hue_all &&
	    bin5 >= tolerance_hue5 &&
	    bin8 >=  tolerance_hue5 &&
	    bin910 >=  tolerance_hue5 &&
	    bin16 >=  tolerance_hue5 &&
	    bin2021 >=  tolerance_hue5 &&
	    bin24 >= tolerance_hue5 &&
	    bin26 >=  tolerance_hue5 &&
	    bin3132 >=  tolerance_hue5 &&
	    bin1 >=  tolerance_hue5) {
		pr_pattern_detect_dbg("judge5 color BDGH pattern detected\n");
		flag = 2;
	}
	if (flag > 0)
		goto judge_finish;

judge_finish:
	if (flag <= 0)
		pr_pattern_detect_dbg("colorbar is not detected\n");

	return flag;
}

static int colorbar_default_loader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("enter\n");

	if (tvafe_clk_status == 0)
		return 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
	    vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
		defaultsetting_get_regmap(12);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
		 vf->source_mode == VFRAME_SOURCE_MODE_PAL)
		defaultsetting_get_regmap(13);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
		 vf->source_mode == VFRAME_SOURCE_MODE_SECAM)
		defaultsetting_get_regmap(14);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		 vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
		defaultsetting_get_regmap(15);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		 vf->source_mode == VFRAME_SOURCE_MODE_PAL)
		defaultsetting_get_regmap(16);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		 vf->source_mode == VFRAME_SOURCE_MODE_SECAM)
		defaultsetting_get_regmap(17);

	pr_pattern_detect_dbg("leave\n");
	return 0;
}

static int colorbar_handler(struct vframe_s *vf, int flag)
{
	static int last_flag = -1, last_type, last_mode;

	pr_pattern_detect_dbg("enter\n");

	/*
	 * flag == 0, color bar is not detected, restore
	 *            the default setting
	 * flag == 1, mixed color bar is detected, apply
	 *            the specific setting
	 * flag == 2, color bar is detected, apply
	 *            the specific setting
	 */

	/*
	 * do not keep programming register
	 * return if detection result is not changed
	 */
	if (last_type == vf->source_type &&
	    last_mode == vf->source_mode &&
	    flag == last_flag)
		return 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
	    vf->source_mode == VFRAME_SOURCE_MODE_NTSC) {
		if (flag <= 0)
			setting_set_regmap(12);
		else if (flag == 1)
			setting_set_regmap(6);
		else if (flag == 2)
			setting_set_regmap(0);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
		   vf->source_mode == VFRAME_SOURCE_MODE_PAL) {
		if (flag <= 0)
			setting_set_regmap(13);
		else if (flag == 1)
			setting_set_regmap(7);
		else if (flag == 2)
			setting_set_regmap(1);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER &&
		   vf->source_mode == VFRAME_SOURCE_MODE_SECAM) {
		if (flag <= 0)
			setting_set_regmap(14);
		else if (flag == 1)
			setting_set_regmap(8);
		else if (flag == 2)
			setting_set_regmap(2);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		   vf->source_mode == VFRAME_SOURCE_MODE_NTSC) {
		if (flag <= 0)
			setting_set_regmap(15);
		else if (flag == 1)
			setting_set_regmap(9);
		else if (flag == 2)
			setting_set_regmap(3);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		   vf->source_mode == VFRAME_SOURCE_MODE_PAL) {
		if (flag <= 0)
			setting_set_regmap(16);
		else if (flag == 1)
			setting_set_regmap(10);
		else if (flag == 2)
			setting_set_regmap(4);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		   vf->source_mode == VFRAME_SOURCE_MODE_SECAM) {
		if (flag <= 0)
			setting_set_regmap(17);
		else if (flag == 1)
			setting_set_regmap(11);
		else if (flag == 2)
			setting_set_regmap(5);
	}

	last_flag = flag;
	last_type = vf->source_type;
	last_mode = vf->source_mode;
	pr_pattern_detect_dbg("leave\n");
	return 0;
}

static int face_hist_checker(struct vframe_s *vf)
{
	/* check cm2 hue hist */
	int flag = -1;
	int hue_hist[32];
	int i, hist_total = 0;
	int skin_tone_hue_bin_sum = 0;
	int tolerance_face;

	tolerance_face = pattern_list[pattern_index].pattern_param[0];

	if (vf->source_type != VFRAME_SOURCE_TYPE_TUNER &&
	    vf->source_type != VFRAME_SOURCE_TYPE_CVBS)
		return flag;

	if (tvafe_clk_status == 0)
		return flag;

	flag = 0;

	/* check cm2 hue hist */
	for (i = 0; i <= 31; i++) {
		hue_hist[i] = vf->prop.hist.vpp_hue_gamma[i];
		hist_total += hue_hist[i];
	}
	hist_total = hist_total / 1000;
	for (i = 0; i <= 31; i++) {
		hue_hist[i] = hue_hist[i] / hist_total;
		pr_pattern_detect_dbg("read hue_hist[%d]=%d, origin=%d\n",
				      i,
				      (int)hue_hist[i],
				      vf->prop.hist.vpp_hue_gamma[i]);
	}

	/*
	 * check skin tone hue hist
	 */
	skin_tone_hue_bin_sum = hue_hist[9] + hue_hist[10] +
				hue_hist[11] + hue_hist[12];
	pr_pattern_detect_dbg("skin_tone_hue_bin_sum=%d\n",
			      skin_tone_hue_bin_sum);

	/* judge */
	if (skin_tone_hue_bin_sum >= tolerance_face) {
		pr_pattern_detect_dbg("skin tone pattern detected\n");
		flag = 1;
	}

	/* patch */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		    vf->source_mode == VFRAME_SOURCE_MODE_PAL)
			flag = 0;
	}

	if (flag <= 0)
		pr_pattern_detect_dbg("skin tone pattern is not detected\n");

	return flag;
}

static int face_default_loader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("enter\n");

	if (tvafe_clk_status == 0)
		return 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
	    vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
		defaultsetting_get_regmap(2);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		 vf->source_mode == VFRAME_SOURCE_MODE_PAL)
		defaultsetting_get_regmap(3);

	pr_pattern_detect_dbg("leave\n");
	return 0;
}

static int face_handler(struct vframe_s *vf, int flag)
{
	static int last_flag = -1, last_type, last_mode;

	pr_pattern_detect_dbg("enter\n");

	/*
	 * flag == 0, face is not detected, restore
	 *            the default setting
	 * flag == 1, face is detected, apply
	 *            the specific setting
	 */

	/*
	 * do not keep programming register
	 * return if detection result is not changed
	 */
	if (last_type == vf->source_type &&
	    last_mode == vf->source_mode &&
	    flag == last_flag)
		return 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
	    vf->source_mode == VFRAME_SOURCE_MODE_NTSC) {
		if (flag <= 0)
			setting_set_regmap(2);
		else if (flag == 1)
			setting_set_regmap(0);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		   vf->source_mode == VFRAME_SOURCE_MODE_PAL) {
		if (flag <= 0)
			setting_set_regmap(3);
		else if (flag == 1)
			setting_set_regmap(1);
	}

	last_flag = flag;
	last_type = vf->source_type;
	last_mode = vf->source_mode;
	pr_pattern_detect_dbg("leave\n");

	return 0;
}

static int corn_hist_checker(struct vframe_s *vf)
{
	/* check cm2 hue hist */
	int flag = -1;
	int hue_hist[32], sat_hist[32];
	int i, hist_total = 0, hist_sat_total = 0;
	int hue_bin_sum = 0, diff_hue_bin14, diff_hue_bin15;
	int diff_hue_bin16, diff_hue_bin17, diff_sat[8];
	int tolerance_hue, tolerance_sat, tolerance_sum;
	int hue_offset = 0; /* for pal default */
	int sat_offset = 0; /* for pal default */

	tolerance_hue = pattern_list[pattern_index].pattern_param[24];
	tolerance_sat = pattern_list[pattern_index].pattern_param[25];
	tolerance_sum = pattern_list[pattern_index].pattern_param[26];

	if (vf->source_mode == VFRAME_SOURCE_MODE_NTSC) {
		hue_offset = 4;
		sat_offset = 8;
	}

	if (vf->source_type != VFRAME_SOURCE_TYPE_TUNER &&
	    vf->source_type != VFRAME_SOURCE_TYPE_CVBS)
		return flag;

	if (tvafe_clk_status == 0)
		return flag;

	flag = 0;

	/* check cm2 hue hist */
	for (i = 0; i <= 31; i++) {
		hue_hist[i] = vf->prop.hist.vpp_hue_gamma[i];
		sat_hist[i] = vf->prop.hist.vpp_sat_gamma[i];
		hist_total += hue_hist[i];
		hist_sat_total += sat_hist[i];
	}
	hist_total = hist_total / 1000;
	hist_sat_total = hist_sat_total / 1000;

	for (i = 0; i <= 31; i++) {
		hue_hist[i] = hue_hist[i] / hist_total;
		pr_pattern_detect_dbg("read hue_hist[%d]=%d, origin=%d\n",
				      i,
				      (int)hue_hist[i],
				      vf->prop.hist.vpp_hue_gamma[i]);
	}

	for (i = 0; i <= 31; i++) {
		sat_hist[i] = sat_hist[i] / hist_sat_total;
		pr_pattern_detect_dbg("read sat_hist[%d]=%d, origin=%d\n",
				      i,
				      (int)sat_hist[i],
				      vf->prop.hist.vpp_sat_gamma[i]);
	}

	/*
	 * check hue hist
	 */
	diff_hue_bin14 = hue_hist[14] -
	pattern_list[pattern_index].pattern_param[0 + hue_offset];
	diff_hue_bin15 = hue_hist[15] -
	pattern_list[pattern_index].pattern_param[1 + hue_offset];
	diff_hue_bin16 = hue_hist[16] -
	pattern_list[pattern_index].pattern_param[2 + hue_offset];
	diff_hue_bin17 = hue_hist[17] -
	pattern_list[pattern_index].pattern_param[3 + hue_offset];
	hue_bin_sum = hue_hist[14] + hue_hist[15] +
			hue_hist[16];
	pr_pattern_detect_dbg("diff_hue_bin14=%d, diff_hue_bin15=%d\n",
			      diff_hue_bin14, diff_hue_bin15);
	pr_pattern_detect_dbg("diff_hue_bin16=%d, diff_hue_bin17=%d\n",
			      diff_hue_bin16, diff_hue_bin17);
	pr_pattern_detect_dbg("hue_bin_sum=%d\n",
			      hue_bin_sum);

	/*
	 * check sat hist
	 */
	for (i = 0; i <= 7; i++) {
		diff_sat[i] = sat_hist[i] -
		pattern_list[pattern_index].pattern_param[8 + i + sat_offset];
		pr_pattern_detect_dbg("diff_sat[%d]=%d\n",
				      i, diff_sat[i]);
	}

	/* judge */
	if (hue_bin_sum >= tolerance_sum) {
		pr_pattern_detect_dbg("green corn pattern detected\n");
		flag = 1;
	}

	/* patch */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		    vf->source_mode == VFRAME_SOURCE_MODE_PAL)
			flag = 0;
	}

	if (flag <= 0)
		pr_pattern_detect_dbg("green corn pattern is not detected\n");

	return flag;
}

static int corn_default_loader(struct vframe_s *vf)
{
	pr_pattern_detect_dbg("enter\n");

	if (tvafe_clk_status == 0)
		return 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
	    vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
		defaultsetting_get_regmap(2);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		 vf->source_mode == VFRAME_SOURCE_MODE_PAL)
		defaultsetting_get_regmap(3);

	pr_pattern_detect_dbg("leave\n");
	return 0;
}

static int corn_handler(struct vframe_s *vf, int flag)
{
	static int last_flag = -1, last_type, last_mode;

	pr_pattern_detect_dbg("enter\n");

	/*
	 * flag == 0, green corn is not detected, restore
	 *            the default setting
	 * flag == 1, green corn is detected, apply
	 *            the specific setting
	 */

	/*
	 * do not keep programming register
	 * return if detection result is not changed
	 */
	if (last_type == vf->source_type &&
	    last_mode == vf->source_mode &&
	    flag == last_flag)
		return 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
	    vf->source_mode == VFRAME_SOURCE_MODE_NTSC) {
		if (flag <= 0)
			setting_set_regmap(2);
		else if (flag == 1)
			setting_set_regmap(0);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		    vf->source_mode == VFRAME_SOURCE_MODE_PAL) {
		if (flag <= 0)
			setting_set_regmap(3);
		else if (flag == 1)
			setting_set_regmap(1);
	}

	last_flag = flag;
	last_type = vf->source_type;
	last_mode = vf->source_mode;
	pr_pattern_detect_dbg("leave\n");

	return 0;
}

static struct di_reg_s di_patch[MULTICAST_REG_LEV] = {
	{
		{0xbf201080, 0xffffffff, 0xbf201080, 0xffffffff,
			0xf7ffc, 0x8000},
	},
	{
		{0xbf3f10ff, 0xffffffff, 0xbf3f10ff, 0xffffffff,
			0xf7ffc, 0x8000},
	}
};

static int multicast_detcnt;
static int last_detflg;
static bool blue_mltcast_en;
static int multicast_color_checker(struct vframe_s *vf)
{
	int flag = -1;
	int hue_hist[32], sat_hist[32];
	int total_hue_hist = 0, total_sat_hist = 0;
	int sat_flg = 0;
	int hue_flg = 0;
	int i;
	int height, width, di_size;

	if (!(vf->di_pulldown & 0x8))
		return flag;

	height = vf->height;
	width = vf->width;

	di_size = height * width / 2;

	for (i = 0; i < 32; i++) {
		sat_hist[i] = vf->prop.hist.vpp_sat_gamma[i];
		hue_hist[i] = vf->prop.hist.vpp_hue_gamma[i];
		total_sat_hist += sat_hist[i];
		total_hue_hist += hue_hist[i];
	}

	pr_pattern_detect_dbg("total_sat_hist= %d, total_hue_hist= %d, ",
			      total_sat_hist, total_hue_hist);
	pr_pattern_detect_dbg("sat_hist[0]= %d, hue_hist[31]= %d\n",
			      sat_hist[0], hue_hist[31]);

	/*suggestion from vlsi-liuyanling,
	 *1. sat_hist[0] > 99% total_sat
	 *2. total_hue < 7/1000 * size
	 */
	sat_flg = sat_hist[0] * 100 - total_sat_hist * 99;
	hue_flg = 3840 * 2160 * 7 - total_hue_hist * 1000;

	if (sat_flg > 0 && hue_flg > 0 &&
	    (vf->di_gmv > di_size * mltcast_ratio1 / 16) &&
	    (vf->di_gmv < di_size * 15 / 16))
		flag = 1;
	else if ((sat_flg > 0) && (hue_flg > 0) &&
		 (vf->di_gmv > di_size * mltcast_ratio2 / 16) &&
		 (vf->di_gmv < di_size * 15 / 16))
		flag = 2;
	else if ((hue_hist[31] > total_hue_hist * 9 / 10) &&
		 (vf->di_cm_cnt > di_size * mltcast_ratio1 / 16) &&
		(vf->di_gmv < di_size * 15 / 16))
		flag = 3;
	else
		flag = 0;

	if (flag > 0 && flag == last_detflg)
		multicast_detcnt++;
	else
		multicast_detcnt = 0;

	last_detflg = flag;

	pr_pattern_detect_dbg("flag:%d, di_gmv:%d, di_size:%d, ",
			      flag, vf->di_gmv, di_size);
	pr_pattern_detect_dbg("di_cm_cnt:%d, sat_flg:%d, hue_flg: %d\n",
			      vf->di_cm_cnt, sat_flg, hue_flg);
	if (multicast_detcnt > 2)
		multicast_detcnt = 2;
	if (multicast_detcnt < 1 && mltcast_skip_en)
		return 0;

	return flag;
}

static int multicast_color_handler(struct vframe_s *vf, int flag)
{
	static int last_flag = -1;

	pr_pattern_detect_dbg("enter\n");

	if (flag == last_flag)
		return 0;

	if (flag == 1)
		di_api_mov_sel(1, &di_patch[1].val[0]);
	else if (flag == 2)
		di_api_mov_sel(1, &di_patch[0].val[0]);
	else if ((flag == 3) && blue_mltcast_en)
		di_api_mov_sel(1, &di_patch[1].val[0]);
	else
		di_api_mov_sel(0, NULL);

	last_flag = flag;

	pr_pattern_detect_dbg("leave\n");
	return 0;
}

#endif

/* public api */
int pattern_detect_add_checker(int id,
			       int (*checker)(struct vframe_s *vf))
{
	pattern_list[id].checker = checker;

	return 0;
}

int pattern_detect_add_defaultloader(int id,
				     int (*loader)(struct vframe_s *vf))
{
	pattern_list[id].default_loader = loader;

	return 0;
}

int pattern_detect_add_handler(int id,
			       int (*handler)(struct vframe_s *vf, int flag))
{
	pattern_list[id].handler = handler;

	return 0;
}

int pattern_detect_add_cvd2_setting_table(int id,
					  struct setting_regs_s *new_settings)
{
	pattern_list[id].cvd2_settings = new_settings;

	return 0;
}

int pattern_detect_add_vpp_setting_table(int id,
					 struct setting_regs_s *new_settings)
{
	pattern_list[id].vpp_settings = new_settings;

	return 0;
}

int init_pattern_detect(void)
{
	/* install callback for color bar */
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
	pattern_detect_add_checker(PATTERN_75COLORBAR,
				   colorbar_hist_checker);
	pattern_detect_add_defaultloader(PATTERN_75COLORBAR,
					 colorbar_default_loader);
	pattern_detect_add_handler(PATTERN_75COLORBAR,
				   colorbar_handler);
	pattern_detect_add_cvd2_setting_table(PATTERN_75COLORBAR,
					      colorbar_cvd2_settings);
	pattern_detect_add_vpp_setting_table(PATTERN_75COLORBAR,
					     colorbar_vpp_settings);

	/* install callback for face pattern */
	pattern_detect_add_checker(PATTERN_SKIN_TONE_FACE,
				   face_hist_checker);
	pattern_detect_add_handler(PATTERN_SKIN_TONE_FACE,
				   face_handler);
	pattern_detect_add_defaultloader(PATTERN_SKIN_TONE_FACE,
					 face_default_loader);
	pattern_detect_add_cvd2_setting_table(PATTERN_SKIN_TONE_FACE,
					      face_cvd2_settings);

	/* install callback for corn */
	pattern_detect_add_checker(PATTERN_GREEN_CORN,
				   corn_hist_checker);
	pattern_detect_add_handler(PATTERN_GREEN_CORN,
				   corn_handler);
	pattern_detect_add_defaultloader(PATTERN_GREEN_CORN,
					 corn_default_loader);
	pattern_detect_add_cvd2_setting_table(PATTERN_GREEN_CORN,
					      corn_cvd2_settings);
	/* install callback for multicast */
	pattern_detect_add_checker(PATTERN_MULTICAST,
				   multicast_color_checker);
	pattern_detect_add_handler(PATTERN_MULTICAST,
				   multicast_color_handler);
	pattern_detect_add_defaultloader(PATTERN_MULTICAST,
					 NULL);
	pattern_detect_add_cvd2_setting_table(PATTERN_MULTICAST,
					      NULL);
#endif

	return 0;
}

int pattern_detect(struct vframe_s *vf)
{
	int flag = 0, i, rc = PATTERN_UNKNOWN;

	if (!vf)
		return 0;

	if (enable_pattern_detect <= 0)
		return 0;

	for (pattern_index = PATTERN_START;
	     pattern_index <= PATTERN_INDEX_MAX;
	     pattern_index++) {
		if (!(pattern_mask & BIT(pattern_index)))
			continue;

		/* load the default setting */
		if (pattern_list[pattern_index].default_loader) {
			pr_pattern_detect_dbg("load default setting for %d:\n",
					      pattern_index);
			pattern_list[pattern_index].default_loader(vf);
		}
	}

	for (pattern_index = PATTERN_START;
		pattern_index <= PATTERN_INDEX_MAX;
		pattern_index++) {
		if (!(pattern_mask & (1 << pattern_index)))
			continue;

		/* check the pattern */
		if (pattern_list[pattern_index].checker) {
			pr_pattern_detect_dbg("check the pattern %d:\n",
					      pattern_index);
			flag =
			pattern_list[pattern_index].checker(vf);
		}

		detected_pattern =
			(flag > 0) ? pattern_index : PATTERN_UNKNOWN;

		/* handle the detected pattern */
		if (flag >= 0) {
			/* call the handler if present */
			if (pattern_list[pattern_index].handler) {
				pr_pattern_detect_dbg("process pattern %d:\n",
						      pattern_index);
				pattern_list[pattern_index].handler(vf, flag);
			} else {
				/* handler is not present, send a notify */
				if  (detected_pattern !=
				     last_detected_pattern) {
					pr_pattern_detect_dbg("notify user\n");
				}
			}

			if (flag > 0) {
				rc = pattern_index;
				goto finish_detect;
			}
		}
	}

finish_detect:
	for (i = pattern_index + 1;
	     i <= PATTERN_INDEX_MAX;
	     i++) {
		if (pattern_list[i].handler) {
			pr_pattern_detect_dbg("load default for %d:\n", i);
			pattern_list[i].handler(vf, 0);
		}
	}
	last_detected_pattern = detected_pattern;
	if (pattern_detect_debug > 0)
		pattern_detect_debug--;

	return rc;
}
