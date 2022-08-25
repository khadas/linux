// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_hdr.h"

struct _hdr_bit_cfg_s {
	struct _bit_s bit_cgain_en;
	struct _bit_s bit_ogain_en;
	struct _bit_s bit_oetf_en;
	struct _bit_s bit_eotf_en;
	struct _bit_s bit_in_fmt;
	struct _bit_s bit_in_shift;
	struct _bit_s bit_gamut_mode;
	struct _bit_s bit_cgain_mode;
	struct _bit_s bit_top_en;
	struct _bit_s bit_mtrxi_en;
	struct _bit_s bit_mtrxo_en;
	struct _bit_s bit_only_mat;
	struct _bit_s bit_out_fmt;
	struct _bit_s bit_hist_input_sel;
	struct _bit_s bit_hist_window_en;
	struct _bit_s bit_hist_read_index;
};

struct _hdr_reg_cfg_s {
	unsigned char page;
	unsigned char reg_ctrl;
	unsigned char reg_clk_gate;
	unsigned char reg_mtrxi_coef00_01;
	unsigned char reg_mtrxi_coef02_10;
	unsigned char reg_mtrxi_coef11_12;
	unsigned char reg_mtrxi_coef20_21;
	unsigned char reg_mtrxi_coef22;
	unsigned char reg_mtrxi_coef30_31;
	unsigned char reg_mtrxi_coef32_40;
	unsigned char reg_mtrxi_coef41_42;
	unsigned char reg_mtrxi_offset0_1;
	unsigned char reg_mtrxi_offset2;
	unsigned char reg_mtrxi_pre_offset0_1;
	unsigned char reg_mtrxi_pre_offset2;
	unsigned char reg_mtrxo_coef00_01;
	unsigned char reg_mtrxo_coef02_10;
	unsigned char reg_mtrxo_coef11_12;
	unsigned char reg_mtrxo_coef20_21;
	unsigned char reg_mtrxo_coef22;
	unsigned char reg_mtrxo_coef30_31;
	unsigned char reg_mtrxo_coef32_40;
	unsigned char reg_mtrxo_coef41_42;
	unsigned char reg_mtrxo_offset0_1;
	unsigned char reg_mtrxo_offset2;
	unsigned char reg_mtrxo_pre_offset0_1;
	unsigned char reg_mtrxo_pre_offset2;
	unsigned char reg_mtrxi_clip;
	unsigned char reg_mtrxo_clip;
	unsigned char reg_cgain_offset;
	unsigned char reg_hist_rd_2;
	unsigned char reg_eotf_lut_addr_port;
	unsigned char reg_eotf_lut_data_port;
	unsigned char reg_oetf_lut_addr_port;
	unsigned char reg_oetf_lut_data_port;
	unsigned char reg_cgain_lut_addr_port;
	unsigned char reg_cgain_lut_data_port;
	unsigned char reg_cgain_coef0;
	unsigned char reg_cgain_coef1;
	unsigned char reg_ogain_lut_addr_port;
	unsigned char reg_ogain_lut_data_port;
	unsigned char reg_adps_ctrl;
	unsigned char reg_adps_alpha0;
	unsigned char reg_adps_alpha1;
	unsigned char reg_adps_beta0;
	unsigned char reg_adps_beta1;
	unsigned char reg_adps_beta2;
	unsigned char reg_adps_coef0;
	unsigned char reg_adps_coef1;
	unsigned char reg_gmut_ctrl;
	unsigned char reg_gmut_coef0;
	unsigned char reg_gmut_coef1;
	unsigned char reg_gmut_coef2;
	unsigned char reg_gmut_coef3;
	unsigned char reg_gmut_coef4;
	unsigned char reg_pipe_ctrl1;
	unsigned char reg_pipe_ctrl2;
	unsigned char reg_pipe_ctrl3;
	unsigned char reg_proc_win1;
	unsigned char reg_proc_win2;
	unsigned char reg_matrixi_en_ctrl;
	unsigned char reg_matrixo_en_ctrl;
	unsigned char reg_hist_ctrl;
	unsigned char reg_hist_h_start_end;
	unsigned char reg_hist_v_start_end;
	unsigned char reg_hist_rd;
};

/*Default table from T3*/
static struct _hdr_reg_cfg_s hdr_reg_cfg[EN_MODULE_TYPE_MAX];
static struct _hdr_bit_cfg_s hdr_bit_cfg = {
	{0, 1},
	{1, 1},
	{2, 1},
	{3, 1},
	{4, 1},
	{5, 1},
	{6, 2},
	{12, 1},
	{13, 1},
	{14, 1},
	{15, 1},
	{16, 1},
	{17, 1},
	{0, 3},
	{4, 1},
	{16, 8},
};

static int cur_eotf_lut[EN_MODULE_TYPE_MAX][HDR_EOTF_LUT_SIZE];
static int cur_oetf_lut[EN_MODULE_TYPE_MAX][HDR_OETF_LUT_SIZE];
static int cur_cgain_lut[EN_MODULE_TYPE_MAX][HDR_CGAIN_LUT_SIZE];
static int cur_ogain_lut[EN_MODULE_TYPE_MAX][HDR_OGAIN_LUT_SIZE];
static int cur_cgain_offset[EN_MODULE_TYPE_MAX][3];

static struct hdr_matrix_data_s cur_matrixi[EN_MODULE_TYPE_MAX];
static struct hdr_matrix_data_s cur_matrixo[EN_MODULE_TYPE_MAX];
static struct hdr_gamut_data_s cur_gamut[EN_MODULE_TYPE_MAX];

static char data_update_flag[EN_MODULE_TYPE_MAX][EN_SUB_MODULE_MAX] = {0};

static struct hdr_hist_report_s hist_report;
static unsigned int max_rgb;

static unsigned char percent[HDR_HIST_PERCENT_BACKUP_COUNT] = {
	1, 5, 10, 25, 50, 75, 90, 95, 99
};

static unsigned int max_rgb_luminance[VPP_HDR_HIST_BIN_COUNT] = {
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1,
	1, 2, 2, 2, 3, 3, 4, 4,
	5, 5, 6, 7, 7, 8, 9, 10, 11,
	12, 14, 15, 17, 18, 20, 22, 24,
	26, 29, 31, 34, 37, 41, 44, 48,
	52, 57, 62, 67, 72, 78, 85, 92,
	99, 107, 116, 125, 135, 146, 158, 170,
	183, 198, 213, 229, 247, 266, 287, 308,
	332, 357, 384, 413, 445, 478, 514, 553, 594,
	639, 686, 737, 792, 851, 915, 983, 1056, 1134,
	1219, 1309, 1406, 1511, 1623, 1744, 1873, 2012,
	2162, 2323, 2496, 2683, 2883, 3098, 3330, 3580,
	3849, 4138, 4450, 4786, 5148, 5539, 5959, 6413,
	6903, 7431, 8001, 8616, 9281, 10000,
};

/*Internal functions*/
static int _set_hdr_ctrl(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static void _set_hdr_eotf_lut(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int i = 0;

	if (!pdata)
		return;

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_eotf_lut_addr_port);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x0);

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_eotf_lut_data_port);
	for (i = 0; i < HDR_EOTF_LUT_SIZE; i++)
		WRITE_VPP_REG_BY_MODE(io_mode, addr, pdata[i]);
}

static void _set_hdr_oetf_lut(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int i = 0;
	int val = 0;

	if (!pdata)
		return;

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_oetf_lut_addr_port);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x0);

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_oetf_lut_data_port);
	for (i = 0; i < HDR_OETF_LUT_SIZE / 2; i++) {
		val = (pdata[i * 2 + 1] << 16) + pdata[i * 2];
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
	}

	if (HDR_OETF_LUT_SIZE % 2 != 0)
		WRITE_VPP_REG_BY_MODE(io_mode, addr, pdata[HDR_OETF_LUT_SIZE - 1]);
}

static void _set_hdr_cgain_lut(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int i = 0;
	int val = 0;

	if (!pdata)
		return;

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_cgain_lut_addr_port);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x0);

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_cgain_lut_data_port);
	for (i = 0; i < HDR_CGAIN_LUT_SIZE / 2; i++) {
		val = (pdata[i * 2 + 1] << 16) + pdata[i * 2];
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
	}

	if (HDR_CGAIN_LUT_SIZE % 2 != 0)
		WRITE_VPP_REG_BY_MODE(io_mode, addr, pdata[HDR_CGAIN_LUT_SIZE - 1]);
}

static void _set_hdr_ogain_lut(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int i = 0;
	int val = 0;

	if (!pdata)
		return;

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_ogain_lut_addr_port);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x0);

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_ogain_lut_data_port);
	for (i = 0; i < HDR_OGAIN_LUT_SIZE / 2; i++) {
		val = (pdata[i * 2 + 1] << 16) + pdata[i * 2];
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
	}

	if (HDR_OGAIN_LUT_SIZE % 2 != 0)
		WRITE_VPP_REG_BY_MODE(io_mode, addr, pdata[HDR_OGAIN_LUT_SIZE - 1]);
}

static void _set_hdr_matrixi(enum hdr_module_type_e type,
	enum io_mode_e io_mode, struct hdr_matrix_data_s *pdata)
{
	unsigned int addr = 0;
	int val = 0;
	int coef00 = 0;
	int coef01 = 0;
	int coef02 = 0;
	int coef10 = 0;
	int coef11 = 0;
	int coef12 = 0;
	int coef20 = 0;
	int coef21 = 0;
	int coef22 = 0;

	if (!pdata)
		return;

	coef00 = pdata->coef[0] & 0x1fff;
	coef01 = pdata->coef[1] & 0x1fff;
	coef02 = pdata->coef[2] & 0x1fff;
	coef10 = pdata->coef[3] & 0x1fff;
	coef11 = pdata->coef[4] & 0x1fff;
	coef12 = pdata->coef[5] & 0x1fff;
	coef20 = pdata->coef[6] & 0x1fff;
	coef21 = pdata->coef[7] & 0x1fff;
	coef22 = pdata->coef[8] & 0x1fff;

	val = (coef00 << 16) | coef01;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_coef00_01);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef02 << 16) | coef10;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_coef02_10);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef11 << 16) | coef12;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_coef11_12);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef20 << 16) | coef21;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_coef20_21);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = coef22;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_coef22);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (pdata->offset[0] << 16) | (pdata->offset[1] & 0xfff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_offset0_1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = pdata->offset[2] & 0xfff;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_offset2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (pdata->pre_offset[0] << 16) | (pdata->pre_offset[1] & 0xfff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_pre_offset0_1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = pdata->pre_offset[2] & 0xfff;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxi_pre_offset2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_matrixo(enum hdr_module_type_e type,
	enum io_mode_e io_mode, struct hdr_matrix_data_s *pdata)
{
	unsigned int addr = 0;
	int val = 0;
	int coef00 = 0;
	int coef01 = 0;
	int coef02 = 0;
	int coef10 = 0;
	int coef11 = 0;
	int coef12 = 0;
	int coef20 = 0;
	int coef21 = 0;
	int coef22 = 0;

	if (!pdata)
		return;

	coef00 = pdata->coef[0] & 0x1fff;
	coef01 = pdata->coef[1] & 0x1fff;
	coef02 = pdata->coef[2] & 0x1fff;
	coef10 = pdata->coef[3] & 0x1fff;
	coef11 = pdata->coef[4] & 0x1fff;
	coef12 = pdata->coef[5] & 0x1fff;
	coef20 = pdata->coef[6] & 0x1fff;
	coef21 = pdata->coef[7] & 0x1fff;
	coef22 = pdata->coef[8] & 0x1fff;

	val = (coef00 << 16) | coef01;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_coef00_01);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef02 << 16) | coef10;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_coef02_10);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef11 << 16) | coef12;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_coef11_12);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef20 << 16) | coef21;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_coef20_21);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = coef22;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_coef22);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (pdata->offset[0] << 16) | (pdata->offset[1] & 0xfff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_offset0_1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = pdata->offset[2] & 0xfff;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_offset2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (pdata->pre_offset[0] << 16) | (pdata->pre_offset[1] & 0xfff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_pre_offset0_1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = pdata->pre_offset[2] & 0xfff;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_mtrxo_pre_offset2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_cgain_offset(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata)
		return;

	val = (pdata[1] << 16) | pdata[0];
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_cgain_offset);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_cgain_coef(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata)
		return;

	val = ((pdata[1] & 0xfff) << 16) | (pdata[0] & 0xfff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_cgain_coef0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = pdata[2] & 0xfff;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_cgain_coef1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_adps_alpha(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata_alpha, int *pdata_shift)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata_alpha | !pdata_shift)
		return;

	val = ((pdata_alpha[1] & 0x3fff) << 16) | (pdata_alpha[0] & 0x3fff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_alpha0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = ((pdata_shift[0] & 0xf) << 24) |
		((pdata_shift[1] & 0xf) << 20) |
		((pdata_shift[2] & 0xf) << 16) |
		(pdata_alpha[2] & 0x3fff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_alpha1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_adps_beta(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata, int *pdata_s)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata | !pdata_s)
		return;

	val = ((pdata_s[0] & 0x1) << 20) | (pdata[0] & 0xfffff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_beta0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = ((pdata_s[1] & 0x1) << 20) | (pdata[1] & 0xfffff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_beta1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = ((pdata_s[2] & 0x1) << 20) | (pdata[2] & 0xfffff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_beta2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_adps_coef(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata)
		return;

	val = ((pdata[1] & 0xfff) << 16) | (pdata[0] & 0xfff);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_coef0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = pdata[2] & 0xfff;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_adps_coef1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_gamut_coef(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int *pdata)
{
	unsigned int addr = 0;
	int val = 0;
	int coef00 = 0;
	int coef01 = 0;
	int coef02 = 0;
	int coef10 = 0;
	int coef11 = 0;
	int coef12 = 0;
	int coef20 = 0;
	int coef21 = 0;
	int coef22 = 0;

	if (!pdata)
		return;

	coef00 = pdata[0] & 0xffff;
	coef01 = pdata[1] & 0xffff;
	coef02 = pdata[2] & 0xffff;
	coef10 = pdata[3] & 0xffff;
	coef11 = pdata[4] & 0xffff;
	coef12 = pdata[5] & 0xffff;
	coef20 = pdata[6] & 0xffff;
	coef21 = pdata[7] & 0xffff;
	coef22 = pdata[8] & 0xffff;

	val = (coef01 << 16) | coef00;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_gmut_coef0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef10 << 16) | coef02;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_gmut_coef1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef12 << 16) | coef11;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_gmut_coef2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = (coef21 << 16) | coef20;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_gmut_coef3);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	val = coef22;
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_gmut_coef4);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_hdr_gamut_data(enum hdr_module_type_e type,
	enum io_mode_e io_mode, struct hdr_gamut_data_s *pdata)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata)
		return;

	_set_hdr_ctrl(type, io_mode, pdata->gamut_mode,
		hdr_bit_cfg.bit_gamut_mode.start,
		hdr_bit_cfg.bit_gamut_mode.len);

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_gmut_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pdata->gamut_shift);

	_set_hdr_gamut_coef(type, io_mode, &pdata->gamut_coef[0]);
	_set_hdr_cgain_coef(type, io_mode, &pdata->cgain_coef[0]);

	val = ((pdata->adpscl_bypass[2] & 0x1) << 6) |
		((pdata->adpscl_bypass[1] & 0x1) << 5) |
		((pdata->adpscl_bypass[0] & 0x1) << 4) |
		(pdata->adpscl_mode & 0x3);
	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
			hdr_reg_cfg[type].reg_adps_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	_set_hdr_adps_alpha(type, io_mode,
		&pdata->adpscl_alpha[0], &pdata->adpscl_shift[0]);
	_set_hdr_adps_beta(type, io_mode,
		&pdata->adpscl_beta[0], &pdata->adpscl_beta_s[0]);
	_set_hdr_adps_coef(type, io_mode, &pdata->adpscl_ys_coef[0]);
}

static int _set_hdr_hist_ctrl(enum hdr_module_type_e type,
	enum io_mode_e io_mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;

	addr = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_hist_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static void _get_hdr_hist_report(enum hdr_module_type_e type,
	enum hdr_hist_data_type_e data_type, int hist_width, int hist_height)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	unsigned int tmp = 0;
	unsigned int latest_buff = HDR_HIST_BACKUP_COUNT - 1;
	unsigned int latest_percent = HDR_HIST_PERCENT_BACKUP_COUNT - 1;
	unsigned int *pval = NULL;
	unsigned int total_pixel = 0;
	unsigned int cur_width = 0;
	unsigned int cur_height = 0;
	unsigned int cur_type = 0;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (type) {
	case EN_MODULE_TYPE_VDIN0:
	case EN_MODULE_TYPE_VDIN1:
	case EN_MODULE_TYPE_OSD1:
	case EN_MODULE_TYPE_OSD2:
	case EN_MODULE_TYPE_OSD3:
	case EN_MODULE_TYPE_DI:
	default:
		return;
	case EN_MODULE_TYPE_VD1:
	case EN_MODULE_TYPE_VD2:
	case EN_MODULE_TYPE_VD3:
		addr = ADDR_PARAM(hdr_reg_cfg[type].page,
			hdr_reg_cfg[type].reg_hist_rd_2);
		break;
	}

	/*Chech and sync histogram settings*/
	if (!hist_width || !hist_height)
		return;

	tmp = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_hist_h_start_end);
	cur_width = READ_VPP_REG_BY_MODE(io_mode, tmp) + 1;

	tmp = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_hist_v_start_end);
	cur_height = READ_VPP_REG_BY_MODE(io_mode, tmp) + 1;

	tmp = ADDR_PARAM(hdr_reg_cfg[type].page,
		hdr_reg_cfg[type].reg_hist_ctrl);
	cur_type = READ_VPP_REG_BY_MODE(io_mode, tmp) & 0x7;

	if (hist_width != cur_width ||
		hist_height != cur_height ||
		data_type != cur_type) {
		tmp = ADDR_PARAM(hdr_reg_cfg[type].page,
			hdr_reg_cfg[type].reg_hist_v_start_end);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, hist_width - 1);

		tmp = ADDR_PARAM(hdr_reg_cfg[type].page,
			hdr_reg_cfg[type].reg_hist_h_start_end);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, hist_height - 1);

		_set_hdr_hist_ctrl(type, io_mode, data_type,
			hdr_bit_cfg.bit_hist_input_sel.start,
			hdr_bit_cfg.bit_hist_input_sel.len);
		return;
	}

	/*Backup old histogram data*/
	for (i = 0; i < latest_buff; i++)
		memcpy(hist_report.hist_buff[i], hist_report.hist_buff[i + 1],
			VPP_HDR_HIST_BIN_COUNT * sizeof(unsigned int));

	/*Get latest histogram data*/
	for (i = 0; i < VPP_HDR_HIST_BIN_COUNT; i++) {
		_set_hdr_hist_ctrl(type, io_mode, i,
			hdr_bit_cfg.bit_hist_read_index.start,
			hdr_bit_cfg.bit_hist_read_index.len);
		tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
		total_pixel += tmp;
		hist_report.hist_buff[latest_buff][i] = tmp;
	}

	/*Calculate histogram percentile data*/
	tmp = 0;
	pval = &hist_report.percentile[0];
	memset(pval, 0,
		sizeof(unsigned int) * HDR_HIST_PERCENT_BACKUP_COUNT);

	if (total_pixel) {
		for (i = 0; i < VPP_HDR_HIST_BIN_COUNT; i++) {
			tmp += hist_report.hist_buff[latest_buff][i];
			for (j = latest_percent; j >= k; j--) {
				if (tmp * 100 / total_pixel >= percent[j]) {
					pval[j] = max_rgb_luminance[i];
					k = j + 1;

					if (k > latest_percent)
						k = latest_percent;
					break;
				}
			}

			if (hist_report.hist_buff[latest_buff][i])
				max_rgb = (i + 1) * 10000 / VPP_HDR_HIST_BIN_COUNT;

			if (pval[latest_percent] != 0)
				break;
		}

		if (pval[0] == 0)
			pval[0] = 1;

		for (i = 1; i < HDR_HIST_PERCENT_BACKUP_COUNT; i++) {
			if (pval[i] == 0)
				pval[i] = pval[i - 1] + 1;
		}

		pval[1] = pval[latest_percent];
	}
}

static void _default_data_init(void)
{
}

/*External functions*/
int vpp_module_hdr_init(struct vpp_dev_s *pdev)
{
	int i = 0;
	unsigned char page = 0x12;
	unsigned char start_reg = 0x80;
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;

	for (i = 0; i < EN_MODULE_TYPE_MAX; i++) {
		switch (i) {
		case EN_MODULE_TYPE_VDIN0:
		default:
			page = 0x12;
			start_reg = 0x80;
			break;
		case EN_MODULE_TYPE_VDIN1:
			page = 0x13;
			start_reg = 0x80;
			break;
		case EN_MODULE_TYPE_VD1:
			page = 0x38;
			start_reg = 0x00;
			break;
		case EN_MODULE_TYPE_VD2:
			page = 0x38;
			start_reg = 0x50;
			break;
		case EN_MODULE_TYPE_VD3:
			page = 0x59;
			start_reg = 0x30;
			break;
		case EN_MODULE_TYPE_OSD1:
			page = 0x38;
			start_reg = 0xa0;
			break;
		case EN_MODULE_TYPE_OSD2:
			page = 0x5b;
			start_reg = 0x00;
			break;
		case EN_MODULE_TYPE_OSD3:
			page = 0x5b;
			start_reg = 0x50;
			break;
		case EN_MODULE_TYPE_DI:
			page = 0x37;
			start_reg = 0x70;
			break;
		}

		hdr_reg_cfg[i].page = page;
		hdr_reg_cfg[i].reg_ctrl = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_clk_gate = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef00_01 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef02_10 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef11_12 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef20_21 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef22 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef30_31 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef32_40 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_coef41_42 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_offset0_1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_offset2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_pre_offset0_1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_pre_offset2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef00_01 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef02_10 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef11_12 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef20_21 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef22 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef30_31 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef32_40 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_coef41_42 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_offset0_1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_offset2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_pre_offset0_1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_pre_offset2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxi_clip = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_mtrxo_clip = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_cgain_offset = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_hist_rd_2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_eotf_lut_addr_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_eotf_lut_data_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_oetf_lut_addr_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_oetf_lut_data_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_cgain_lut_addr_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_cgain_lut_data_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_cgain_coef0 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_cgain_coef1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_ogain_lut_addr_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_ogain_lut_data_port = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_ctrl = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_alpha0 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_alpha1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_beta0 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_beta1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_beta2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_coef0 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_adps_coef1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_gmut_ctrl = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_gmut_coef0 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_gmut_coef1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_gmut_coef2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_gmut_coef3 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_gmut_coef4 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_pipe_ctrl1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_pipe_ctrl2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_pipe_ctrl3 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_proc_win1 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_proc_win2 = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_matrixi_en_ctrl = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_matrixo_en_ctrl = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_hist_ctrl = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_hist_h_start_end = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_hist_v_start_end = start_reg;
		start_reg++;
		hdr_reg_cfg[i].reg_hist_rd = start_reg;
	}

	_default_data_init();
	memset(&hist_report, 0, sizeof(hist_report));

	return 0;
}

int vpp_module_hdr_en(enum hdr_module_type_e type, bool enable)
{
	return _set_hdr_ctrl(type, EN_MODE_DIR, enable,
		hdr_bit_cfg.bit_top_en.start, hdr_bit_cfg.bit_top_en.len);
}

void vpp_module_hdr_sub_module_en(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, bool enable)
{
	unsigned char start = 0;
	unsigned char len = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (sub_module) {
	case EN_SUB_MODULE_CGAIN:
		start = hdr_bit_cfg.bit_cgain_en.start;
		len = hdr_bit_cfg.bit_cgain_en.len;
		break;
	case EN_SUB_MODULE_OGAIN:
		start = hdr_bit_cfg.bit_ogain_en.start;
		len = hdr_bit_cfg.bit_ogain_en.len;
		break;
	case EN_SUB_MODULE_OETF:
		start = hdr_bit_cfg.bit_oetf_en.start;
		len = hdr_bit_cfg.bit_oetf_en.len;
		break;
	case EN_SUB_MODULE_EOTF:
		start = hdr_bit_cfg.bit_eotf_en.start;
		len = hdr_bit_cfg.bit_eotf_en.len;
		break;
	case EN_SUB_MODULE_MTRXI:
		start = hdr_bit_cfg.bit_mtrxi_en.start;
		len = hdr_bit_cfg.bit_mtrxi_en.len;
		break;
	case EN_SUB_MODULE_MTRXO:
		start = hdr_bit_cfg.bit_mtrxo_en.start;
		len = hdr_bit_cfg.bit_mtrxo_en.len;
		break;
	default:
		return;
	}

	_set_hdr_ctrl(type, io_mode, enable, start, len);
}

void vpp_module_hdr_set_lut(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, int *pdata)
{
	if (!pdata ||
		type == EN_MODULE_TYPE_MAX ||
		sub_module == EN_SUB_MODULE_MAX)
		return;

	switch (sub_module) {
	case EN_SUB_MODULE_CGAIN:
		memcpy(&cur_cgain_lut[type][0], pdata,
			HDR_CGAIN_LUT_SIZE * sizeof(int));
		break;
	case EN_SUB_MODULE_OGAIN:
		memcpy(&cur_ogain_lut[type][0], pdata,
			HDR_OGAIN_LUT_SIZE * sizeof(int));
		break;
	case EN_SUB_MODULE_OETF:
		memcpy(&cur_oetf_lut[type][0], pdata,
			HDR_OETF_LUT_SIZE * sizeof(int));
		break;
	case EN_SUB_MODULE_EOTF:
		memcpy(&cur_eotf_lut[type][0], pdata,
			HDR_EOTF_LUT_SIZE * sizeof(int));
		break;
	case EN_SUB_MODULE_CGAIN_OFFSET:
		memcpy(&cur_cgain_offset[type][0], pdata,
			3 * sizeof(int));
		break;
	default:
		return;
	}

	data_update_flag[type][sub_module] = 1;
}

void vpp_module_hdr_set_matrix(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, struct hdr_matrix_data_s *pdata)
{
	if (!pdata ||
		type == EN_MODULE_TYPE_MAX ||
		sub_module == EN_SUB_MODULE_MAX)
		return;

	switch (sub_module) {
	case EN_SUB_MODULE_MTRXI:
		memcpy(&cur_matrixi[type], pdata,
			sizeof(struct hdr_matrix_data_s));
		break;
	case EN_SUB_MODULE_MTRXO:
		memcpy(&cur_matrixo[type], pdata,
			sizeof(struct hdr_matrix_data_s));
		break;
	default:
		return;
	}

	data_update_flag[type][sub_module] = 1;
}

void vpp_module_hdr_set_gamut(enum hdr_module_type_e type,
	struct hdr_gamut_data_s *pdata)
{
	if (!pdata ||
		type == EN_MODULE_TYPE_MAX)
		return;

	memcpy(&cur_gamut[type], pdata,
		sizeof(struct hdr_gamut_data_s));

	data_update_flag[type][EN_SUB_MODULE_GAMUT] = 1;
}

void vpp_module_hdr_hist_en(enum hdr_module_type_e type, bool enable)
{
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (type) {
	case EN_MODULE_TYPE_VDIN0:
	case EN_MODULE_TYPE_VDIN1:
	case EN_MODULE_TYPE_OSD1:
	case EN_MODULE_TYPE_OSD2:
	case EN_MODULE_TYPE_OSD3:
	case EN_MODULE_TYPE_DI:
	default:
		break;
	case EN_MODULE_TYPE_VD1:
	case EN_MODULE_TYPE_VD2:
	case EN_MODULE_TYPE_VD3:
		_set_hdr_hist_ctrl(type, io_mode, enable,
			hdr_bit_cfg.bit_hist_window_en.start,
			hdr_bit_cfg.bit_hist_window_en.len);
		break;
	}
}

void vpp_module_get_hdr_hist_report(enum hdr_module_type_e type,
	enum hdr_hist_data_type_e data_type, int hist_width, int hist_height,
	struct hdr_hist_report_s *pdata)
{
	if (!pdata)
		return;

	_get_hdr_hist_report(type, data_type,
		hist_width, hist_height);

	memcpy(pdata, &hist_report, sizeof(struct hdr_hist_report_s));
}

void vpp_module_hdr_on_vs(void)
{
	int i = 0;
	int j = 0;
	enum io_mode_e io_mode = EN_MODE_RDMA;

	for (i = 0; i < EN_MODULE_TYPE_MAX; i++) {
		for (j = 0; j < EN_SUB_MODULE_MAX; j++) {
			if (data_update_flag[i][j]) {
				data_update_flag[i][j] = 0;

				switch (j) {
				case EN_SUB_MODULE_CGAIN:
					_set_hdr_cgain_lut(i, io_mode,
						&cur_cgain_lut[i][0]);
					break;
				case EN_SUB_MODULE_OGAIN:
					_set_hdr_ogain_lut(i, io_mode,
						&cur_ogain_lut[i][0]);
					break;
				case EN_SUB_MODULE_OETF:
					_set_hdr_oetf_lut(i, io_mode,
						&cur_oetf_lut[i][0]);
					break;
				case EN_SUB_MODULE_EOTF:
					_set_hdr_eotf_lut(i, io_mode,
						&cur_eotf_lut[i][0]);
					break;
				case EN_SUB_MODULE_MTRXI:
					_set_hdr_matrixi(i, io_mode,
						&cur_matrixi[i]);
					break;
				case EN_SUB_MODULE_MTRXO:
					_set_hdr_matrixo(i, io_mode,
						&cur_matrixo[i]);
					break;
				case EN_SUB_MODULE_GAMUT:
					_set_hdr_gamut_data(i, io_mode,
						&cur_gamut[i]);
					break;
				case EN_SUB_MODULE_CGAIN_OFFSET:
					_set_hdr_cgain_offset(i, io_mode,
						&cur_cgain_offset[i][0]);
					break;
				}
			}
		}
	}
}

