// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_lut3d.h"

#define SLEEP_MIN (16000)
#define SLEEP_MAX (16001)

struct _lut3d_bit_cfg_s {
	struct _bit_s bit_lut3d_en;
	struct _bit_s bit_lut3d_extend_en;
	struct _bit_s bit_lut3d_shadow;
	struct _bit_s bit_gated_clock_en;
};

struct _lut3d_reg_cfg_s {
	unsigned char page;
	unsigned char reg_lut3d_ctrl;
	unsigned char reg_lut3d_cbus2ram_ctrl;
	unsigned char reg_lut3d_ram_addr;
	unsigned char reg_lut3d_ram_data;
};

/*Default table from T3*/
static struct _lut3d_reg_cfg_s lut3d_reg_cfg = {
	0x39,
	0xd0,
	0xd1,
	0xd2,
	0xd3
};

static struct _lut3d_bit_cfg_s lut3d_bit_cfg = {
	{0, 1},
	{2, 1},
	{4, 3},
	{8, 2}
};

/*Internal functions*/
static void _set_lut3d_enable_ctrl(int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(lut3d_reg_cfg.page, lut3d_reg_cfg.reg_lut3d_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_lut3d_data(int *pdata)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	int i = 0;
	unsigned int tmp = 0;
	int data_size = LUT3D_UNIT_DATA_LEN *
		LUT3D_UNIT_DATA_LEN * LUT3D_UNIT_DATA_LEN * 3;

	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_ctrl);
	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, 0,
		lut3d_bit_cfg.bit_lut3d_en.start,
		lut3d_bit_cfg.bit_lut3d_en.len);

	usleep_range(SLEEP_MIN, SLEEP_MAX);

	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_cbus2ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 1);

	/*LUT3D_RAM_ADDR bit[20:16]=R, [12:8]=G, [4:0]=B*/
	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_ram_addr);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_ram_data);

	for (i = 0; i < data_size; i++) {/*write data, order RGB*/
		WRITE_VPP_REG_BY_MODE(io_mode, addr,
			((pdata[i * 3 + 1] & 0xfff) << 16) |
			(pdata[i * 3 + 2] & 0xfff));
		WRITE_VPP_REG_BY_MODE(io_mode, addr,
			(pdata[i * 3 + 0] & 0xfff));
	}

	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_cbus2ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);
}

/*External functions*/
int vpp_module_lut3d_init(struct vpp_dev_s *pdev)
{
	return 0;
}

void vpp_module_lut3d_en(bool enable)
{
	unsigned int addr = 0;

	addr = ADDR_PARAM(lut3d_reg_cfg.page,
		lut3d_reg_cfg.reg_lut3d_cbus2ram_ctrl);
	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, 0);

	_set_lut3d_enable_ctrl(0x7,
		lut3d_bit_cfg.bit_lut3d_extend_en.start,
		lut3d_bit_cfg.bit_lut3d_extend_en.len);

	_set_lut3d_enable_ctrl(enable,
		lut3d_bit_cfg.bit_lut3d_en.start,
		lut3d_bit_cfg.bit_lut3d_en.len);
}

void vpp_module_lut3d_set_data(int *pdata)
{
	if (!pdata)
		return;

	_set_lut3d_data(pdata);
}

