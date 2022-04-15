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
};

struct _ve_reg_cfg_s {
	unsigned char page;
	unsigned char reg_ve_enable_ctrl;
	unsigned char reg_ve_blkext_ctrl;
	unsigned char reg_ve_ccoring_ctrl;
};

/*Default table from T3*/
static struct _ve_reg_cfg_s ve_reg_cfg = {
	0x1d,
	0xa1,
	0x80,
	0xa0
};

static struct _ve_bit_cfg_s ve_bit_cfg = {
	{3, 1},
	{0, 8},
	{8, 8},
	{16, 8},
	{24, 8},
	{4, 1},
	{0, 4},
	{8, 8}
};

/*Internal functions*/
static int _set_ve_enable_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_enable_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_ve_blkext_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_blkext_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_ve_ccoring_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(ve_reg_cfg.page, ve_reg_cfg.reg_ve_ccoring_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

/*External functions*/
int vpp_module_ve_init(struct vpp_dev_s *pdev)
{
	return 0;
}

int vpp_module_ve_blkext_en(bool enable)
{
	return _set_ve_enable_ctrl(enable,
		ve_bit_cfg.bit_blkext_en.start, ve_bit_cfg.bit_blkext_en.len);
}

int vpp_module_ve_set_blkext_slope(int val)
{
	return _set_ve_blkext_ctrl(val,
		ve_bit_cfg.bit_blkext_slope1.start, ve_bit_cfg.bit_blkext_slope1.len);
}

int vpp_module_ve_set_blkext_start(int val)
{
	return _set_ve_blkext_ctrl(val,
		ve_bit_cfg.bit_blkext_start.start, ve_bit_cfg.bit_blkext_start.len);
}

int vpp_module_ve_ccoring_en(bool enable)
{
	return _set_ve_enable_ctrl(enable,
		ve_bit_cfg.bit_ccoring_en.start, ve_bit_cfg.bit_ccoring_en.len);
}

int vpp_module_ve_set_ccoring_slope(int val)
{
	return _set_ve_ccoring_ctrl(val,
		ve_bit_cfg.bit_ccoring_slope.start, ve_bit_cfg.bit_ccoring_slope.len);
}

int vpp_module_ve_set_ccoring_th(int val)
{
	return _set_ve_ccoring_ctrl(val,
		ve_bit_cfg.bit_ccoring_th.start, ve_bit_cfg.bit_ccoring_th.len);
}

