/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _VICP_HDR_H_
#define _VICP_HDR_H_

#include <linux/amlogic/media/amvecm/hdr2_ext.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include "vicp_log.h"
#include "vicp_reg.h"

extern u32 print_flag;
/* *********************************************************************** */
/* ************************* enum definitions **************************.*/
/* *********************************************************************** */
enum VICP_HDR_REG_IDX {
	VICP_MATRIXI_COEF00_01 = 0,
	VICP_MATRIXI_COEF02_10,
	VICP_MATRIXI_COEF11_12,
	VICP_MATRIXI_COEF20_21,
	VICP_MATRIXI_COEF22,
	VICP_MATRIXI_COEF30_31,
	VICP_MATRIXI_COEF32_40,
	VICP_MATRIXI_COEF41_42,
	VICP_MATRIXI_OFFSET0_1,
	VICP_MATRIXI_OFFSET2,
	VICP_MATRIXI_PRE_OFFSET0_1,
	VICP_MATRIXI_PRE_OFFSET2,
	VICP_MATRIXI_CLIP,
	VICP_MATRIXI_EN_CTRL,
	VICP_MATRIXO_COEF00_01,
	VICP_MATRIXO_COEF02_10,
	VICP_MATRIXO_COEF11_12,
	VICP_MATRIXO_COEF20_21,
	VICP_MATRIXO_COEF22,
	VICP_MATRIXO_COEF30_31,
	VICP_MATRIXO_COEF32_40,
	VICP_MATRIXO_COEF41_42,
	VICP_MATRIXO_OFFSET0_1,
	VICP_MATRIXO_OFFSET2,
	VICP_MATRIXO_PRE_OFFSET0_1,
	VICP_MATRIXO_PRE_OFFSET2,
	VICP_MATRIXO_CLIP,
	VICP_MATRIXO_EN_CTRL,
	VICP_CGAIN_OFFT,
	VICP_CGAIN_COEF0,
	VICP_CGAIN_COEF1,
	VICP_ADPS_CTRL,
	VICP_ADPS_ALPHA0,
	VICP_ADPS_ALPHA1,
	VICP_ADPS_BETA0,
	VICP_ADPS_BETA1,
	VICP_ADPS_BETA2,
	VICP_ADPS_COEF0,
	VICP_ADPS_COEF1,
	VICP_GMUT_CTRL,
	VICP_GMUT_COEF0,
	VICP_GMUT_COEF1,
	VICP_GMUT_COEF2,
	VICP_GMUT_COEF3,
	VICP_GMUT_COEF4,
	VICP_HDR_CTRL,
};

/* *********************************************************************** */
/* ************************* struct definitions **************************.*/
/* *********************************************************************** */
struct vicp_hdr_data_s {
	int enable; /*1: en; 2: to disable; 0: disable*/
	bool para_done;
	bool n_update;
	bool pre_post;
	bool have_set;
	u32 w;
	u32 h;
	u32 last_sgn_type;
	enum hdr_process_sel last_prc_sel;
	enum hdr_module_sel module_sel;
	enum hdr_process_sel hdr_process_select;
	struct hdr_proc_setting_param_s hdr_para;
};

struct vicp_hdr_s {
	struct vicp_hdr_data_s hdr_data;
};

/* *********************************************************************** */
/* ************************* function definitions **************************.*/
/* *********************************************************************** */
struct vicp_hdr_s *vicp_hdr_prob(void);
void vicp_hdr_remove(struct vicp_hdr_s *vicp_hdr);
void vicp_hdr_set(struct vicp_hdr_s *vicp_hdr, int pre_post);
#endif
