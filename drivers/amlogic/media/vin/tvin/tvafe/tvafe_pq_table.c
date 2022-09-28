// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "tvafe_regs.h"
#include "tvafe_general.h"
#include "tvafe_debug.h"
#include "tvafe.h"

/* ******************************************************
 *  tl1 pq table
 ********************************************************
 */
/* ************* cvbs(avin, port1/2) *************** */
static struct tvafe_reg_table_s cvbs_ntscm_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_SIGNAL_THRESHOLD, 0x7d, 0xff},
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_CONTROL1, 0x08, 0xff},
	{CVD2_2DCOMB_NOISE_TH, 0x84, 0xff},
	{CVD2_REG_B0, 0x00, 0xff},
	{CVD2_3DCOMB_FILTER, 0x0f, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_ntsc443_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0xeafb4e8e, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_pali_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*chroma state adjust dynamicly*/
	{CVD2_CHROMA_LOOPFILTER_STATE, 0x0a, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	{ACD_REG_89, 0x80010004, 0xffffffff},
	{ACD_REG_8A, 0x100004, 0xffffffff},
	{ACD_REG_8C, 0x38000, 0xffffffff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_palm_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*for monoscope pattern color flash*/
	{ACD_REG_22, 0x2020000, 0xffffffff},
	{CVD2_NOISE_THRESHOLD, 0xff, 0xff},
	{CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0xeafb4e8e, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_pal60_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0xeafb4e8e, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_palcn_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0xeafb4e8e, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_secam_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0xeafb4e8e, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_ntsc50_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0xeafb4e8e, 0xffffffff},
	{0xffffffff, 0, 0},
};

/* *************** rf(atv, port0/3) ********************* */
static struct tvafe_reg_table_s rf_ntscm_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_ntsc443_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_pali_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_palm_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	/*for monoscope pattern color flash*/
	{ACD_REG_22, 0x2020000, 0xffffffff},
	{CVD2_NOISE_THRESHOLD, 0xff, 0xff},
	{CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20, 0xff},
	{CVD2_REG_87, 0xc0, 0xc0},
	{CVD2_REG_FA, 0x00, 0xa0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_pal60_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_palcn_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_secam_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_ntsc50_table_tl1[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{CVD2_REG_87, 0xc0, 0xc0},
	{0xffffffff, 0, 0},
};

/* ******************************************************
 *  pq table assemble
 ********************************************************
 */

static struct tvafe_reg_table_s *cvbs_pq_config_tl1[] = {
	cvbs_ntscm_table_tl1,
	cvbs_ntsc443_table_tl1,
	cvbs_pali_table_tl1,
	cvbs_palm_table_tl1,
	cvbs_pal60_table_tl1,
	cvbs_palcn_table_tl1,
	cvbs_secam_table_tl1,
	cvbs_ntsc50_table_tl1,
};

static struct tvafe_reg_table_s *rf_pq_config_tl1[] = {
	rf_ntscm_table_tl1,
	rf_ntsc443_table_tl1,
	rf_pali_table_tl1,
	rf_palm_table_tl1,
	rf_pal60_table_tl1,
	rf_palcn_table_tl1,
	rf_secam_table_tl1,
	rf_ntsc50_table_tl1,
};

int tvafe_pq_config_probe(struct meson_tvafe_data *tvafe_data)
{
	if (!tvafe_data) {
		tvafe_pr_err("%s: tvafe_data is null\n", __func__);
		return -1;
	}

	switch (tvafe_data->cpu_id) {
	case TVAFE_CPU_TYPE_TL1:
	case TVAFE_CPU_TYPE_TM2:
	case TVAFE_CPU_TYPE_TM2_B:
	case TVAFE_CPU_TYPE_T5:
	case TVAFE_CPU_TYPE_T5D:
	case TVAFE_CPU_TYPE_T3:
	case TVAFE_CPU_TYPE_T5W:
		tvafe_data->cvbs_pq_conf = cvbs_pq_config_tl1;
		tvafe_data->rf_pq_conf = rf_pq_config_tl1;
		break;
	default:
		break;
	}

	return 0;
}
