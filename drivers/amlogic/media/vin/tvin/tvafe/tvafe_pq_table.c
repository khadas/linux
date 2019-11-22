/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_pq_table.c
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

#include "tvafe_regs.h"
#include "tvafe_general.h"
#include "tvafe_debug.h"
#include "tvafe.h"

/* ******************************************************
 *  txl pq table
 ********************************************************
 */
/* ************* cvbs(avin, port1/2) *************** */
static struct tvafe_reg_table_s cvbs_ntscm_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_SIGNAL_THRESHOLD, 0x7d, 0xff},
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_ntsc443_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_pali_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*chroma state adjust dynamicly*/
	{CVD2_CHROMA_LOOPFILTER_STATE, 0x0a, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_palm_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	/*for moonoscope pattern color flash*/
	{ACD_REG_22, 0x2020000, 0xffffffff},
	{CVD2_NOISE_THRESHOLD, 0xff, 0xff},
	{CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_pal60_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_palcn_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_secam_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_ntsc50_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

/* *************** rf(atv, port0/3) ********************* */
static struct tvafe_reg_table_s rf_ntscm_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_ntsc443_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_pali_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_palm_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	/*for moonoscope pattern color flash*/
	{ACD_REG_22, 0x2020000, 0xffffffff},
	{CVD2_NOISE_THRESHOLD, 0xff, 0xff},
	{CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20, 0xff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_pal60_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_palcn_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_secam_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_ntsc50_table_txl[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

/* ******************************************************
 *  txlx pq table
 ********************************************************
 */
/* ************* cvbs(avin, port1/2) *************** */
static struct tvafe_reg_table_s cvbs_ntscm_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_SIGNAL_THRESHOLD, 0x7d, 0xff},
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_ntsc443_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_pali_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*chroma state adjust dynamicly*/
	{CVD2_CHROMA_LOOPFILTER_STATE, 0x0a, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_palm_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	/*for moonoscope pattern color flash*/
	{ACD_REG_22, 0x2020000, 0xffffffff},
	{CVD2_NOISE_THRESHOLD, 0xff, 0xff},
	{CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_pal60_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_palcn_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_secam_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	{CVD2_REG_B2, 0x08, 0x18},
	{CVD2_CHROMA_EDGE_ENHANCEMENT, 0x22, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s cvbs_ntsc50_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0, 0xff},
	/*set for wipe off vertical stripes*/
	{ACD_REG_25, 0x00e941a8, 0xffffffff},
	{0xffffffff, 0, 0},
};

/* *************** rf(atv, port0/3) ********************* */
static struct tvafe_reg_table_s rf_ntscm_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_ntsc443_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_pali_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_palm_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	/*for moonoscope pattern color flash*/
	{ACD_REG_22, 0x2020000, 0xffffffff},
	{CVD2_NOISE_THRESHOLD, 0xff, 0xff},
	{CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 0x20, 0xff},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_pal60_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_palcn_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_secam_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

static struct tvafe_reg_table_s rf_ntsc50_table_txlx[] = {
	/* reg,     val,     mask */
	{CVD2_REG_B0, 0xf0, 0xff},
	{CVD2_REG_B2, 0x00, 0x18},
	{CVD2_CONTROL1, 0x00, 0x0c},
	{0xffffffff, 0, 0},
};

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
	/*for moonoscope pattern color flash*/
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
	/*for moonoscope pattern color flash*/
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
static struct tvafe_reg_table_s *cvbs_pq_config_txl[] = {
	cvbs_ntscm_table_txl,
	cvbs_ntsc443_table_txl,
	cvbs_pali_table_txl,
	cvbs_palm_table_txl,
	cvbs_pal60_table_txl,
	cvbs_palcn_table_txl,
	cvbs_secam_table_txl,
	cvbs_ntsc50_table_txl,
};

static struct tvafe_reg_table_s *rf_pq_config_txl[] = {
	rf_ntscm_table_txl,
	rf_ntsc443_table_txl,
	rf_pali_table_txl,
	rf_palm_table_txl,
	rf_pal60_table_txl,
	rf_palcn_table_txl,
	rf_secam_table_txl,
	rf_ntsc50_table_txl,
};

static struct tvafe_reg_table_s *cvbs_pq_config_txlx[] = {
	cvbs_ntscm_table_txlx,
	cvbs_ntsc443_table_txlx,
	cvbs_pali_table_txlx,
	cvbs_palm_table_txlx,
	cvbs_pal60_table_txlx,
	cvbs_palcn_table_txlx,
	cvbs_secam_table_txlx,
	cvbs_ntsc50_table_txlx,
};

static struct tvafe_reg_table_s *rf_pq_config_txlx[] = {
	rf_ntscm_table_txlx,
	rf_ntsc443_table_txlx,
	rf_pali_table_txlx,
	rf_palm_table_txlx,
	rf_pal60_table_txlx,
	rf_palcn_table_txlx,
	rf_secam_table_txlx,
	rf_ntsc50_table_txlx,
};

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
	case CPU_TYPE_TXL:
		tvafe_data->cvbs_pq_conf = cvbs_pq_config_txl;
		tvafe_data->rf_pq_conf = rf_pq_config_txl;
		break;
	case CPU_TYPE_TXLX:
		tvafe_data->cvbs_pq_conf = cvbs_pq_config_txlx;
		tvafe_data->rf_pq_conf = rf_pq_config_txlx;
		break;
	case CPU_TYPE_TL1:
	case CPU_TYPE_TM2:
		tvafe_data->cvbs_pq_conf = cvbs_pq_config_tl1;
		tvafe_data->rf_pq_conf = rf_pq_config_tl1;
		break;
	default:
		break;
	}

	return 0;
}
