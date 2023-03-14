/* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "aml_isp_reg.h"
#include "aml_isp_hw.h"


static void cmpr_raw0_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_BASIS, 128, 4, 9);

	val = (16 << 0) | (15 << 8) | (14 << 16) | (13 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0, val);

	val = (13 << 0) | (12 << 8) | (12 << 16) | (11 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_1, val);

	val = (11 << 0) | (10 << 8) | (10 << 16) | (9 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_2, val);

	val = (9 << 0) | (8 << 8) | (8 << 16) | (7 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_3, val);

	val = (7 << 0) | (6 << 8) | (6 << 16) | (5 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_4, val);

	val = (5 << 0) | (4 << 8) | (4 << 16) | (3 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_5, val);

	val = (3 << 0) | (2 << 8) | (2 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_6, val);

	val = (1 << 0) | (1 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_7, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_8, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN0_9, val);


	val = (16 << 0) | (15 << 8) | (14 << 16) | (13 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1, val);

	val = (13 << 0) | (12 << 8) | (12 << 16) | (11 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_1, val);

	val = (11 << 0) | (10 << 8) | (10 << 16) | (9 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_2, val);

	val = (9 << 0) | (8 << 8) | (8 << 16) | (7 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_3, val);

	val = (7 << 0) | (6 << 8) | (6 << 16) | (5 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_4, val);

	val = (5 << 0) | (4 << 8) | (4 << 16) | (3 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_5, val);

	val = (3 << 0) | (2 << 8) | (2 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_6, val);

	val = (1 << 0) | (1 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_7, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_8, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN1_9, val);


	val = (16 << 0) | (15 << 8) | (14 << 16) | (13 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2, val);

	val = (13 << 0) | (12 << 8) | (12 << 16) | (11 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_1, val);

	val = (11 << 0) | (10 << 8) | (10 << 16) | (9 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_2, val);

	val = (9 << 0) | (8 << 8) | (8 << 16) | (7 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_3, val);

	val = (7 << 0) | (6 << 8) | (6 << 16) | (5 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_4, val);

	val = (5 << 0) | (4 << 8) | (4 << 16) | (3 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_5, val);

	val = (3 << 0) | (2 << 8) | (2 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_6, val);

	val = (1 << 0) | (1 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_7, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_8, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_QP_MAP_CHN2_9, val);

	val = (2048 << 0) | (256 << 16);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_FLATNESS_TH0, val);

	val = (2048 << 0) | (256 << 16);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_FLATNESS_TH1, val);

	val = (2048 << 0) | (256 << 16);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_FLATNESS_TH2, val);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_RC_BUDGET_5, 31, 24, 6);
	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_RC_BUDGET_5, 17, 16, 6);
}

static void cmpr_raw1_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_BASIS, 128, 4, 9);

	val = (16 << 0) | (15 << 8) | (14 << 16) | (13 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0, val);

	val = (13 << 0) | (12 << 8) | (12 << 16) | (11 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_1, val);

	val = (11 << 0) | (10 << 8) | (10 << 16) | (9 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_2, val);

	val = (9 << 0) | (8 << 8) | (8 << 16) | (7 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_3, val);

	val = (7 << 0) | (6 << 8) | (6 << 16) | (5 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_4, val);

	val = (5 << 0) | (4 << 8) | (4 << 16) | (3 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_5, val);

	val = (3 << 0) | (2 << 8) | (2 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_6, val);

	val = (1 << 0) | (1 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_7, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_8, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN0_9, val);


	val = (16 << 0) | (15 << 8) | (14 << 16) | (13 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1, val);

	val = (13 << 0) | (12 << 8) | (12 << 16) | (11 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_1, val);

	val = (11 << 0) | (10 << 8) | (10 << 16) | (9 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_2, val);

	val = (9 << 0) | (8 << 8) | (8 << 16) | (7 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_3, val);

	val = (7 << 0) | (6 << 8) | (6 << 16) | (5 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_4, val);

	val = (5 << 0) | (4 << 8) | (4 << 16) | (3 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_5, val);

	val = (3 << 0) | (2 << 8) | (2 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_6, val);

	val = (1 << 0) | (1 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_7, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_8, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN1_9, val);


	val = (16 << 0) | (15 << 8) | (14 << 16) | (13 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2, val);

	val = (13 << 0) | (12 << 8) | (12 << 16) | (11 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_1, val);

	val = (11 << 0) | (10 << 8) | (10 << 16) | (9 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_2, val);

	val = (9 << 0) | (8 << 8) | (8 << 16) | (7 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_3, val);

	val = (7 << 0) | (6 << 8) | (6 << 16) | (5 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_4, val);

	val = (5 << 0) | (4 << 8) | (4 << 16) | (3 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_5, val);

	val = (3 << 0) | (2 << 8) | (2 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_6, val);

	val = (1 << 0) | (1 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_7, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_8, val);

	val = (0 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_QP_MAP_CHN2_9, val);

	val = (2048 << 0) | (256 << 16);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_FLATNESS_TH0, val);

	val = (2048 << 0) | (256 << 16);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_FLATNESS_TH1, val);

	val = (2048 << 0) | (256 << 16);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_FLATNESS_TH2, val);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_RC_BUDGET_5, 31, 24, 6);
	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_RC_BUDGET_5, 17, 16, 6);
}

static void cmpr_raw0_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 val = 0;
	aisp_base_cfg_t *cfg_base = base;
	aisp_phase_setting_cfg_t *phase = &cfg_base->phase_cfg;

	val = (phase->losse_raw_phslut[8] << 0) | (phase->losse_raw_phslut[9] << 4) |
		(phase->losse_raw_phslut[10] << 8) | (phase->losse_raw_phslut[11] << 12) |
		(phase->losse_raw_phslut[12] << 16) | (phase->losse_raw_phslut[13] << 20) |
		(phase->losse_raw_phslut[14] << 24) | (phase->losse_raw_phslut[15] << 28);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_GLOBAL_PHASE_LUT, val);

	val = (phase->losse_raw_phslut[0] << 0) | (phase->losse_raw_phslut[1] << 4) |
		(phase->losse_raw_phslut[2] << 8) | (phase->losse_raw_phslut[3] << 12) |
		(phase->losse_raw_phslut[4] << 16) | (phase->losse_raw_phslut[5] << 20) |
		(phase->losse_raw_phslut[6] << 24) | (phase->losse_raw_phslut[7] << 28);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_GLOBAL_PHASE_LUT_1, val);
}

static void cmpr_raw1_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 val = 0;
	aisp_base_cfg_t *cfg_base = base;
	aisp_phase_setting_cfg_t *phase = &cfg_base->phase_cfg;

	val = (phase->losse_raw_phslut[8] << 0) | (phase->losse_raw_phslut[9] << 4) |
		(phase->losse_raw_phslut[10] << 8) | (phase->losse_raw_phslut[11] << 12) |
		(phase->losse_raw_phslut[12] << 16) | (phase->losse_raw_phslut[13] << 20) |
		(phase->losse_raw_phslut[14] << 24) | (phase->losse_raw_phslut[15] << 28);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_GLOBAL_PHASE_LUT, val);

	val = (phase->losse_raw_phslut[0] << 0) | (phase->losse_raw_phslut[1] << 4) |
		(phase->losse_raw_phslut[2] << 8) | (phase->losse_raw_phslut[3] << 12) |
		(phase->losse_raw_phslut[4] << 16) | (phase->losse_raw_phslut[5] << 20) |
		(phase->losse_raw_phslut[6] << 24) | (phase->losse_raw_phslut[7] << 28);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_GLOBAL_PHASE_LUT_1, val);
}

static void cmpr_raw_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	cmpr_raw0_cfg_base(isp_dev, base);
	cmpr_raw1_cfg_base(isp_dev, base);
}

void isp_cmpr_raw_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		cmpr_raw_cfg_base(isp_dev, &param->base_cfg);
}

void isp_cmpr_raw_init(struct isp_dev_t *isp_dev)
{
	cmpr_raw0_init(isp_dev);
	cmpr_raw1_init(isp_dev);
}

void isp_cmpr_raw_cfg_ratio(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_BASIS, isp_dev->tnr_bits * 16, 4, 9);
	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_BASIS, isp_dev->tnr_bits * 16, 4, 9);
}

