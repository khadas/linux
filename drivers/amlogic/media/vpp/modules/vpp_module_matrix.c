// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_matrix.h"

#define MTRX_OFFSET_CNT (3)
#define MTRX_COEF_MASK (0x1fff)

enum _matrix_size_e {
	EN_MTRX_SIZE_3X3 = 0,
	EN_MTRX_SIZE_3X5,
};

struct _matrix_bit_cfg_s {
	struct _bit_s bit_mtrx_en;
	struct _bit_s bit_mtrx_clmod;
	struct _bit_s bit_mtrx_rs;
	struct _bit_s bit_mtrx_offset[MTRX_OFFSET_CNT];
};

struct _matrix_reg_cfg_s {
	unsigned char page;
	unsigned char reg_mtrx_coef00_01;
	unsigned char reg_mtrx_coef02_10;
	unsigned char reg_mtrx_coef11_12;
	unsigned char reg_mtrx_coef20_21;
	unsigned char reg_mtrx_coef22;
	unsigned char reg_mtrx_coef13_14;
	unsigned char reg_mtrx_coef23_24;
	unsigned char reg_mtrx_coef15_25;
	unsigned char reg_mtrx_clip;
	unsigned char reg_mtrx_offset0_1;
	unsigned char reg_mtrx_offset2;
	unsigned char reg_mtrx_pre_offset0_1;
	unsigned char reg_mtrx_pre_offset2;
	unsigned char reg_mtrx_en_ctrl;
	unsigned char reg_mtrx_sat;
};

struct _matrix_coef_3x3_s {
	int pre_offset[MTRX_OFFSET_CNT];
	int coef[9];
	int offset[MTRX_OFFSET_CNT];
};

struct _matrix_coef_3x5_s {
	bool changed;
	int pre_offset[MTRX_OFFSET_CNT];
	int coef[15];
	int offset[MTRX_OFFSET_CNT];
};

static struct _matrix_reg_cfg_s mtrx_reg_cfg[EN_MTRX_MODE_MAX] = {0};

/*Default table from T3*/
static struct _matrix_bit_cfg_s mtrx_bit_cfg = {
	{0, 1},
	{3, 2},
	{5, 3},
	{
		{0, 12},
		{16, 12},
		{0, 12}
	}
};

static struct _matrix_coef_3x3_s mtrx_coef_3x3_yuv_bypass = {
	{-64, -512, -512},
	{
		COEFF_NORM(1.0), COEFF_NORM(0),   COEFF_NORM(0),
		COEFF_NORM(0),   COEFF_NORM(1.0), COEFF_NORM(0),
		COEFF_NORM(0),   COEFF_NORM(0),   COEFF_NORM(1.0)
	},
	{64, 512, 512}
};

static struct _matrix_coef_3x5_s cur_mtrx_data[EN_MTRX_MODE_MAX];

/*Internal functions*/
static int _set_matrix_ctrl(enum matrix_mode_e mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (mode == EN_MTRX_MODE_MAX)
		return 0;

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_en_ctrl);

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_matrix_clip(enum matrix_mode_e mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (mode == EN_MTRX_MODE_MAX)
		return 0;

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_clip);

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_matrix_pre_offset(enum io_mode_e io_mode,
	enum matrix_mode_e mode, int *pdata)
{
	unsigned int addr = 0;
	unsigned char start = mtrx_bit_cfg.bit_mtrx_offset[0].start;
	int val = 0;

	if (mode == EN_MTRX_MODE_MAX || !pdata)
		return 0;

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page,
		mtrx_reg_cfg[mode].reg_mtrx_pre_offset0_1);
	val = (pdata[0] << start) | pdata[1];
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page,
		mtrx_reg_cfg[mode].reg_mtrx_pre_offset2);

	val = pdata[2];
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	return 0;
}

static int _set_matrix_offset(enum io_mode_e io_mode,
	enum matrix_mode_e mode, int *pdata)
{
	unsigned int addr = 0;
	unsigned char start = mtrx_bit_cfg.bit_mtrx_offset[0].start;
	int val = 0;

	if (mode == EN_MTRX_MODE_MAX || !pdata)
		return 0;

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page,
		mtrx_reg_cfg[mode].reg_mtrx_offset0_1);
	val = (pdata[0] << start) | pdata[1];
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page,
		mtrx_reg_cfg[mode].reg_mtrx_offset2);

	val = pdata[2];
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	return 0;
}

static int _set_matrix_coef(enum io_mode_e io_mode,
	enum matrix_mode_e mode, enum _matrix_size_e size, int *pdata)
{
	unsigned int addr = 0;
	unsigned int val = 0;
	unsigned int coef00 = 0;
	unsigned int coef01 = 0;
	unsigned int coef02 = 0;
	unsigned int coef10 = 0;
	unsigned int coef11 = 0;
	unsigned int coef12 = 0;
	unsigned int coef20 = 0;
	unsigned int coef21 = 0;
	unsigned int coef22 = 0;
	unsigned int coef13 = 0;
	unsigned int coef14 = 0;
	unsigned int coef23 = 0;
	unsigned int coef24 = 0;
	unsigned int coef15 = 0;
	unsigned int coef25 = 0;

	if (mode == EN_MTRX_MODE_MAX)
		return 0;

	coef00 = pdata[0];
	coef01 = pdata[1];
	coef02 = pdata[2];
	coef10 = pdata[3];
	coef11 = pdata[4];
	coef12 = pdata[5];
	coef20 = pdata[6];
	coef21 = pdata[7];
	coef22 = pdata[8];

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef00_01);
	val = ((coef00 & MTRX_COEF_MASK) << 16) | (coef01 & MTRX_COEF_MASK);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef02_10);
	val = ((coef02 & MTRX_COEF_MASK) << 16) | (coef10 & MTRX_COEF_MASK);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef11_12);
	val = ((coef11 & MTRX_COEF_MASK) << 16) | (coef12 & MTRX_COEF_MASK);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef20_21);
	val = ((coef20 & MTRX_COEF_MASK) << 16) | (coef21 & MTRX_COEF_MASK);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef22);
	val = coef22 & MTRX_COEF_MASK;
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	if (size == EN_MTRX_SIZE_3X5) {
		coef13 = pdata[9];
		coef14 = pdata[10];
		coef23 = pdata[11];
		coef24 = pdata[12];
		coef15 = pdata[13];
		coef25 = pdata[14];

		addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef13_14);
		val = ((coef13 & MTRX_COEF_MASK) << 16) | (coef14 & MTRX_COEF_MASK);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

		addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef23_24);
		val = ((coef23 & MTRX_COEF_MASK) << 16) | (coef24 & MTRX_COEF_MASK);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

		addr = ADDR_PARAM(mtrx_reg_cfg[mode].page, mtrx_reg_cfg[mode].reg_mtrx_coef15_25);
		val = ((coef15 & MTRX_COEF_MASK) << 16) | (coef25 & MTRX_COEF_MASK);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
	}

	return 0;
}

static void _set_matrix_default_data(enum matrix_mode_e mode)
{
	int i = 0;

	cur_mtrx_data[mode].changed = true;

	for (i = 0; i < MTRX_OFFSET_CNT; i++) {
		cur_mtrx_data[mode].pre_offset[i] = 0;
		cur_mtrx_data[mode].offset[i] = 0;
	}

	for (i = 0; i < 15; i++) {
		if (i != 0 && i != 4 && i != 8)
			cur_mtrx_data[mode].coef[i] = 0;
		else
			cur_mtrx_data[mode].coef[i] = 0x400;
	}
}

static void _dump_matrix_reg_info(void)
{
	int i = 0;

	PR_DRV("mtrx_reg_cfg info:\n");
	for (i = 0; i < EN_MTRX_MODE_MAX; i++) {
		PR_DRV("matrix type = %d\n", i);
		PR_DRV("page = %x\n", mtrx_reg_cfg[i].page);
		PR_DRV("reg_mtrx_coef00_01 = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_coef00_01);
		PR_DRV("reg_mtrx_coef22 = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_coef22);
		PR_DRV("reg_mtrx_coef13_14 = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_coef13_14);
		PR_DRV("reg_mtrx_clip = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_clip);
		PR_DRV("reg_mtrx_offset0_1 = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_offset0_1);
		PR_DRV("reg_mtrx_pre_offset2 = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_pre_offset2);
		PR_DRV("reg_mtrx_en_ctrl = %x\n",
			mtrx_reg_cfg[i].reg_mtrx_en_ctrl);
	}
}

static void _dump_matrix_data_info(void)
{
	int i = 0;
	int j = 0;

	PR_DRV("cur_mtrx_data data info:\n");
	for (i = 0; i < EN_MTRX_MODE_MAX; i++) {
		for (j = 0; j < MTRX_OFFSET_CNT; j++)
			PR_DRV("%d\t", cur_mtrx_data[i].pre_offset[j]);
		PR_DRV("\n");
		for (j = 0; j < 15; j++)
			PR_DRV("%d\t", cur_mtrx_data[i].coef[j]);
		PR_DRV("\n");
		for (j = 0; j < MTRX_OFFSET_CNT; j++)
			PR_DRV("%d\t", cur_mtrx_data[i].offset[j]);
	}
}

/*External functions*/
int vpp_module_matrix_init(struct vpp_dev_s *pdev)
{
	int i = 0;

	mtrx_reg_cfg[EN_MTRX_MODE_VD1].page = 0x32;
	mtrx_reg_cfg[EN_MTRX_MODE_VD1].reg_mtrx_coef00_01 = 0x90;

	mtrx_reg_cfg[EN_MTRX_MODE_POST].page = 0x32;
	mtrx_reg_cfg[EN_MTRX_MODE_POST].reg_mtrx_coef00_01 = 0xb0;
	mtrx_reg_cfg[EN_MTRX_MODE_POST].reg_mtrx_sat = 0xc1;

	mtrx_reg_cfg[EN_MTRX_MODE_POST2].page = 0x39;
	mtrx_reg_cfg[EN_MTRX_MODE_POST2].reg_mtrx_coef00_01 = 0xa0;

	mtrx_reg_cfg[EN_MTRX_MODE_OSD2].page = 0x39;
	mtrx_reg_cfg[EN_MTRX_MODE_OSD2].reg_mtrx_coef00_01 = 0x20;

	mtrx_reg_cfg[EN_MTRX_MODE_WRAP_OSD1].page = 0x3d;
	mtrx_reg_cfg[EN_MTRX_MODE_WRAP_OSD1].reg_mtrx_coef00_01 = 0x60;

	mtrx_reg_cfg[EN_MTRX_MODE_WRAP_OSD2].page = 0x3d;
	mtrx_reg_cfg[EN_MTRX_MODE_WRAP_OSD2].reg_mtrx_coef00_01 = 0x70;

	mtrx_reg_cfg[EN_MTRX_MODE_WRAP_OSD3].page = 0x3d;
	mtrx_reg_cfg[EN_MTRX_MODE_WRAP_OSD3].reg_mtrx_coef00_01 = 0xb0;

	for (i = EN_MTRX_MODE_VD1; i < EN_MTRX_MODE_MAX; i++) {
		mtrx_reg_cfg[i].reg_mtrx_coef02_10     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 1;
		mtrx_reg_cfg[i].reg_mtrx_coef11_12     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 2;
		mtrx_reg_cfg[i].reg_mtrx_coef20_21     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 3;
		mtrx_reg_cfg[i].reg_mtrx_coef22        = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 4;
		mtrx_reg_cfg[i].reg_mtrx_coef13_14     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 5;
		mtrx_reg_cfg[i].reg_mtrx_coef23_24     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 6;
		mtrx_reg_cfg[i].reg_mtrx_coef15_25     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 7;
		mtrx_reg_cfg[i].reg_mtrx_clip          = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 8;
		mtrx_reg_cfg[i].reg_mtrx_offset0_1     = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 9;
		mtrx_reg_cfg[i].reg_mtrx_offset2       = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 10;
		mtrx_reg_cfg[i].reg_mtrx_pre_offset0_1 = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 11;
		mtrx_reg_cfg[i].reg_mtrx_pre_offset2   = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 12;
		mtrx_reg_cfg[i].reg_mtrx_en_ctrl       = mtrx_reg_cfg[i].reg_mtrx_coef00_01 + 13;
	}

	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].page = 0x1d;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_en_ctrl   = 0x5f;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef00_01 = 0x60;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef02_10 = 0x61;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef11_12 = 0x62;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef20_21 = 0x63;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef22    = 0x64;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef13_14 = 0xdb;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef23_24 = 0xdc;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_coef15_25 = 0xdd;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_clip      = 0xde;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_offset0_1     = 0x65;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_offset2       = 0x66;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_pre_offset0_1 = 0x67;
	mtrx_reg_cfg[EN_MTRX_MODE_CMPT].reg_mtrx_pre_offset2   = 0x68;

	_set_matrix_default_data(EN_MTRX_MODE_POST);

	return 0;
}

int vpp_module_matrix_en(enum matrix_mode_e mode, bool enable)
{
	unsigned char start = mtrx_bit_cfg.bit_mtrx_en.start;
	unsigned char len = mtrx_bit_cfg.bit_mtrx_en.len;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d, enable = %d\n",
		__func__, mode, enable);

	if (mode == EN_MTRX_MODE_CMPT)
		start = 6;

	return _set_matrix_ctrl(mode, enable, start, len);
}

int vpp_module_matrix_sel(enum matrix_mode_e mode, int val)
{
	int ret = 0;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d, val = %d\n",
		__func__, mode, val);

	if (mode == EN_MTRX_MODE_CMPT)
		ret = _set_matrix_ctrl(mode, val, 8, 2);

	return ret;
}

int vpp_module_matrix_clmod(enum matrix_mode_e mode, int val)
{
	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d, val = %d\n",
		__func__, mode, val);

	return _set_matrix_clip(mode, val,
		mtrx_bit_cfg.bit_mtrx_clmod.start, mtrx_bit_cfg.bit_mtrx_clmod.len);
}

int vpp_module_matrix_rs(enum matrix_mode_e mode, int val)
{
	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d, val = %d\n",
		__func__, mode, val);

	return _set_matrix_clip(mode, val,
		mtrx_bit_cfg.bit_mtrx_rs.start, mtrx_bit_cfg.bit_mtrx_rs.len);
}

int vpp_module_matrix_set_offset(enum matrix_mode_e mode,
	int *pdata)
{
	if (mode == EN_MTRX_MODE_MAX || !pdata)
		return 0;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d\n", __func__, mode);

	memcpy(&cur_mtrx_data[mode].offset[0], pdata,
		sizeof(int) * MTRX_OFFSET_CNT);
	cur_mtrx_data[mode].changed = true;

	return 0;
}

int vpp_module_matrix_set_pre_offset(enum matrix_mode_e mode,
	int *pdata)
{
	if (mode == EN_MTRX_MODE_MAX || !pdata)
		return 0;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d\n", __func__, mode);

	memcpy(&cur_mtrx_data[mode].pre_offset[0], pdata,
		sizeof(int) * MTRX_OFFSET_CNT);
	cur_mtrx_data[mode].changed = true;

	return 0;
}

int vpp_module_matrix_set_coef_3x3(enum matrix_mode_e mode, int *pdata)
{
	if (mode == EN_MTRX_MODE_MAX || !pdata)
		return 0;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d\n", __func__, mode);

	memcpy(&cur_mtrx_data[mode].coef[0], pdata, sizeof(int) * 9);
	cur_mtrx_data[mode].changed = true;

	return 0;
}

int vpp_module_matrix_set_coef_3x5(enum matrix_mode_e mode, int *pdata)
{
	if (mode == EN_MTRX_MODE_MAX || !pdata)
		return 0;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] mode = %d\n", __func__, mode);

	memcpy(&cur_mtrx_data[mode].coef[0], pdata, sizeof(int) * 15);
	cur_mtrx_data[mode].changed = true;

	return 0;
}

int vpp_module_matrix_set_contrast_uv(int val_u, int val_v)
{
	int i = 0;
	int data[9] = {0};
	enum io_mode_e io_mode = EN_MODE_DIR;
	enum matrix_mode_e mode = EN_MTRX_MODE_VD1;
	enum _matrix_size_e size = EN_MTRX_SIZE_3X3;

	pr_vpp(PR_DEBUG_MATRIX, "[%s] val_u = %d, val_v = %d\n",
		__func__, val_u, val_v);

	for (i = 0; i < 9; i++)
		data[i] = mtrx_coef_3x3_yuv_bypass.coef[i];

	data[4] = val_u;
	data[8] = val_v;
	_set_matrix_coef(io_mode, mode, size, data);
	_set_matrix_offset(io_mode, mode, &cur_mtrx_data[mode].offset[0]);
	_set_matrix_pre_offset(io_mode, mode, &cur_mtrx_data[mode].pre_offset[0]);

	return 0;
}

void vpp_module_matrix_on_vs(void)
{
	int i = 0;
	enum io_mode_e io_mode = EN_MODE_RDMA;
	enum _matrix_size_e size = EN_MTRX_SIZE_3X5;

	for (i = 0; i < EN_MTRX_MODE_MAX; i++) {
		if (!cur_mtrx_data[i].changed)
			continue;

		_set_matrix_coef(io_mode, i, size, &cur_mtrx_data[i].coef[0]);
		_set_matrix_offset(io_mode, i, &cur_mtrx_data[i].offset[0]);
		_set_matrix_pre_offset(io_mode, i, &cur_mtrx_data[i].pre_offset[0]);

		cur_mtrx_data[i].changed = false;
	}
}

void vpp_module_matrix_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_matrix_reg_info();
	else
		_dump_matrix_data_info();
}

