// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_go.h"

#define GO_PARAM_CNT (3)

struct _go_bit_cfg_s {
	struct _bit_s bit_go_ctrl_en;
	struct _bit_s bit_go_gain[GO_PARAM_CNT];
	struct _bit_s bit_go_offset[GO_PARAM_CNT];
	struct _bit_s bit_go_pre_offset[GO_PARAM_CNT];
};

struct _go_reg_cfg_s {
	unsigned char page;
	unsigned char reg_go_ctrl0;
	unsigned char reg_go_ctrl1;
	unsigned char reg_go_ctrl2;
	unsigned char reg_go_ctrl3;
	unsigned char reg_go_ctrl4;
};

/*Default table from T3*/
static struct _go_reg_cfg_s go_reg_cfg = {
	0x1d,
	0x6a,
	0x6b,
	0x6c,
	0x6d,
	0x6e
};

static struct _go_bit_cfg_s go_bit_cfg = {
	{31, 1},
	{
		{16, 11},
		{0, 11},
		{16, 11}
	},
	{
		{0, 11},
		{16, 11},
		{0, 11}
	},
	{
		{16, 11},
		{0, 11},
		{0, 11}
	}
};

static void _dump_go_reg_info(void)
{
	PR_DRV("go_reg_cfg info:\n");
	PR_DRV("page = %x\n", go_reg_cfg.page);
	PR_DRV("reg_gamma_ctrl = %x\n", go_reg_cfg.reg_go_ctrl0);
	PR_DRV("reg_gamma_addr = %x\n", go_reg_cfg.reg_go_ctrl1);
	PR_DRV("reg_gamma_data = %x\n", go_reg_cfg.reg_go_ctrl2);
	PR_DRV("reg_gamma_ctrl = %x\n", go_reg_cfg.reg_go_ctrl3);
	PR_DRV("reg_gamma_addr = %x\n", go_reg_cfg.reg_go_ctrl4);
}

static void _dump_go_data_info(void)
{
	PR_DRV("No more data\n");
}

/*External functions*/
int vpp_module_go_init(struct vpp_dev_s *pdev)
{
	int i = 0;
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;

	if (chip_id == CHIP_TXHD || chip_id == CHIP_TL1 || chip_id == CHIP_TM2)
		for (i = 0; i < GO_PARAM_CNT; i++) {
			go_bit_cfg.bit_go_gain[i].len = 13;
			go_bit_cfg.bit_go_offset[i].len = 13;
			go_bit_cfg.bit_go_pre_offset[i].len = 13;
		}

	return 0;
}

void vpp_module_go_en(bool enable)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned char start;
	unsigned char len;

	pr_vpp(PR_DEBUG_GO, "[%s] enable = %d\n", __func__, enable);

	addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl0);
	start = go_bit_cfg.bit_go_ctrl_en.start;
	len = go_bit_cfg.bit_go_ctrl_en.len;

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, enable, start, len);
}

void vpp_module_go_set_gain(unsigned char idx, int val)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned char start;
	unsigned char len;

	pr_vpp(PR_DEBUG_GO, "[%s] idx = %d, val = %d\n",
		__func__, idx, val);

	switch (idx) {
	case 0:
	case 1:
		addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl0);
		break;
	case 2:
		addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl1);
		break;
	default:
		return;
	}

	start = go_bit_cfg.bit_go_gain[idx].start;
	len = go_bit_cfg.bit_go_gain[idx].len;

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

void vpp_module_go_set_offset(unsigned char idx, int val)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned char start;
	unsigned char len;

	pr_vpp(PR_DEBUG_GO, "[%s] idx = %d, val = %d\n",
		__func__, idx, val);

	switch (idx) {
	case 0:
		addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl1);
		break;
	case 1:
	case 2:
		addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl2);
		break;
	default:
		return;
	}

	start = go_bit_cfg.bit_go_offset[idx].start;
	len = go_bit_cfg.bit_go_offset[idx].len;

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

void vpp_module_go_set_pre_offset(unsigned char idx, int val)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned char start;
	unsigned char len;

	pr_vpp(PR_DEBUG_GO, "[%s] idx = %d, val = %d\n",
		__func__, idx, val);

	switch (idx) {
	case 0:
	case 1:
		addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl3);
		break;
	case 2:
		addr = ADDR_PARAM(go_reg_cfg.page, go_reg_cfg.reg_go_ctrl4);
		break;
	default:
		return;
	}

	start = go_bit_cfg.bit_go_pre_offset[idx].start;
	len = go_bit_cfg.bit_go_pre_offset[idx].len;

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

void vpp_module_go_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_go_reg_info();
	else
		_dump_go_data_info();
}

