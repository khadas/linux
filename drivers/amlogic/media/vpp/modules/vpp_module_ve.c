// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_ve.h"

struct _ve_bit_cfg_s {
	struct _bit_s bit_blkext_en;
	struct _bit_s bit_blkext_slope2;
	struct _bit_s bit_blkext_midpt;
	struct _bit_s bit_blkext_slope1;
	struct _bit_s bit_blkext_start;
	struct _bit_s bit_ccoring_en;
	struct _bit_s bit_ccoring_slope;
	struct _bit_s bit_ccoring_th;
	struct _bit_s bit_ccoring_bypass_yth;
	struct _bit_s bit_demo_center_bar_en;
};

struct _ve_reg_cfg_s {
	unsigned char page;
	unsigned char reg_ve_enable_ctrl;
	unsigned char reg_ve_blkext_ctrl;
	unsigned char reg_ve_ccoring_ctrl;
	unsigned char reg_ve_demo_center_bar;
	unsigned char reg_ve_demo_left_top_screen_width;
};

/*Default table from T3*/
static struct _ve_reg_cfg_s ve_reg_cfg = {
	0x1d,
	0xa1,
	0x80,
	0xa0,
	0xa3,
	0xa2
};

static struct _ve_bit_cfg_s ve_bit_cfg = {
	{3, 1},
	{0, 8},
	{8, 8},
	{16, 8},
	{24, 8},
	{4, 1},
	{0, 4},
	{8, 8},
	{31, 1}
};

/*Internal functions*/
static void _set_ve_enable_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_enable_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_ve_blkext_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_blkext_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_ve_ccoring_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_ccoring_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_ve_demo_center_bar(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_demo_center_bar);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

/*External functions*/
int vpp_module_ve_init(struct vpp_dev_s *pdev)
{
	return 0;
}

void vpp_module_ve_blkext_en(bool enable)
{
	_set_ve_enable_ctrl(enable,
		ve_bit_cfg.bit_blkext_en.start, ve_bit_cfg.bit_blkext_en.len);
}

void vpp_module_ve_set_blkext_param(enum ve_blkext_param_e type,
	int val)
{
	unsigned char start = 0;
	unsigned char len = 0;

	switch (type) {
	case EN_BLKEXT_START:
		start = ve_bit_cfg.bit_blkext_start.start;
		len = ve_bit_cfg.bit_blkext_start.len;
		break;
	case EN_BLKEXT_SLOPE1:
		start = ve_bit_cfg.bit_blkext_slope1.start;
		len = ve_bit_cfg.bit_blkext_slope1.len;
		break;
	case EN_BLKEXT_MIDPT:
		start = ve_bit_cfg.bit_blkext_midpt.start;
		len = ve_bit_cfg.bit_blkext_midpt.len;
		break;
	case EN_BLKEXT_SLOPE2:
		start = ve_bit_cfg.bit_blkext_slope2.start;
		len = ve_bit_cfg.bit_blkext_slope2.len;
		break;
	default:
		return;
	}

	_set_ve_blkext_ctrl(val, start, len);
}

void vpp_module_ve_ccoring_en(bool enable)
{
	_set_ve_enable_ctrl(enable,
		ve_bit_cfg.bit_ccoring_en.start, ve_bit_cfg.bit_ccoring_en.len);
}

void vpp_module_ve_set_ccoring_param(enum ve_ccoring_param_e type,
	int val)
{
	unsigned char start = 0;
	unsigned char len = 0;

	switch (type) {
	case EN_CCORING_SLOPE:
		start = ve_bit_cfg.bit_ccoring_slope.start;
		len = ve_bit_cfg.bit_ccoring_slope.len;
		break;
	case EN_CCORING_TH:
		start = ve_bit_cfg.bit_ccoring_th.start;
		len = ve_bit_cfg.bit_ccoring_th.len;
		break;
	case EN_CCORING_BYPASS_YTH:
		start = ve_bit_cfg.bit_ccoring_bypass_yth.start;
		len = ve_bit_cfg.bit_ccoring_bypass_yth.len;
		break;
	default:
		return;
	}

	_set_ve_ccoring_ctrl(val, start, len);
}

void vpp_module_ve_demo_center_bar_en(bool enable)
{
	_set_ve_demo_center_bar(enable,
		ve_bit_cfg.bit_demo_center_bar_en.start,
		ve_bit_cfg.bit_demo_center_bar_en.len);
}

void vpp_module_ve_set_demo_left_top_screen_width(int val)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page,
		ve_reg_cfg.reg_ve_demo_left_top_screen_width);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val & 0x1fff);
}

