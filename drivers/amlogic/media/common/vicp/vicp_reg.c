// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_reg.c
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

#include <linux/io.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/registers/regs/ao_regs.h>
#include <linux/amlogic/power_domain.h>
#include "vicp_reg.h"
#include "vicp_log.h"

#define VICPBUS_REG_ADDR(reg) ((reg) << 2)

struct vicp_afbce_reg_s vicp_afbce_reg_array[VICP_SUPPORT_CHIP_MAX] = {
	{
		VID_CMPR_AFBCE_ENABLE,
		VID_CMPR_AFBCE_MODE,
		VID_CMPR_AFBCE_SIZE_IN,
		VID_CMPR_AFBCE_BLK_SIZE_IN,
		VID_CMPR_AFBCE_HEAD_BADDR,
		VID_CMPR_AFBCE_MIF_SIZE,
		VID_CMPR_AFBCE_PIXEL_IN_HOR_SCOPE,
		VID_CMPR_AFBCE_PIXEL_IN_VER_SCOPE,
		VID_CMPR_AFBCE_CONV_CTRL,
		VID_CMPR_AFBCE_MIF_HOR_SCOPE,
		VID_CMPR_AFBCE_MIF_VER_SCOPE,
		VID_CMPR_AFBCE_STAT1,
		VID_CMPR_AFBCE_STAT2,
		VID_CMPR_AFBCE_FORMAT,
		VID_CMPR_AFBCE_MODE_EN,
		VID_CMPR_AFBCE_DWSCALAR,
		VID_CMPR_AFBCE_DEFCOLOR_1,
		VID_CMPR_AFBCE_DEFCOLOR_2,
		VID_CMPR_AFBCE_QUANT_ENABLE,
		VID_CMPR_AFBCE_IQUANT_LUT_1,
		VID_CMPR_AFBCE_IQUANT_LUT_2,
		VID_CMPR_AFBCE_IQUANT_LUT_3,
		VID_CMPR_AFBCE_IQUANT_LUT_4,
		VID_CMPR_AFBCE_RQUANT_LUT_1,
		VID_CMPR_AFBCE_RQUANT_LUT_2,
		VID_CMPR_AFBCE_RQUANT_LUT_3,
		VID_CMPR_AFBCE_RQUANT_LUT_4,
		VID_CMPR_AFBCE_YUV_FORMAT_CONV_MODE,
		VID_CMPR_AFBCE_DUMMY_DATA,
		VID_CMPR_AFBCE_CLR_FLAG,
		VID_CMPR_AFBCE_STA_FLAGT,
		VID_CMPR_AFBCE_MMU_NUM,
		VID_CMPR_AFBCE_PIP_CTRL,
		VID_CMPR_AFBCE_ROT_CTRL,
		VID_CMPR_AFBCE_DIMM_CTRL,
		VID_CMPR_AFBCE_BND_DEC_MISC,
		VID_CMPR_AFBCE_RD_ARB_MISC,
		VID_CMPR_AFBCE_MMU_RMIF_CTRL1,
		VID_CMPR_AFBCE_MMU_RMIF_CTRL2,
		VID_CMPR_AFBCE_MMU_RMIF_CTRL3,
		VID_CMPR_AFBCE_MMU_RMIF_CTRL4,
		VID_CMPR_AFBCE_MMU_RMIF_SCOPE_X,
		VID_CMPR_AFBCE_MMU_RMIF_SCOPE_Y,
		VID_CMPR_AFBCE_MMU_RMIF_RO_STAT,
	},
	{
		T3X_VID_CMPR_AFBCE_ENABLE,
		T3X_VID_CMPR_AFBCE_MODE,
		T3X_VID_CMPR_AFBCE_SIZE_IN,
		T3X_VID_CMPR_AFBCE_BLK_SIZE_IN,
		T3X_VID_CMPR_AFBCE_HEAD_BADDR,
		T3X_VID_CMPR_AFBCE_MIF_SIZE,
		T3X_VID_CMPR_AFBCE_PIXEL_IN_HOR_SCOPE,
		T3X_VID_CMPR_AFBCE_PIXEL_IN_VER_SCOPE,
		T3X_VID_CMPR_AFBCE_CONV_CTRL,
		T3X_VID_CMPR_AFBCE_MIF_HOR_SCOPE,
		T3X_VID_CMPR_AFBCE_MIF_VER_SCOPE,
		T3X_VID_CMPR_AFBCE_STAT1,
		T3X_VID_CMPR_AFBCE_STAT2,
		T3X_VID_CMPR_AFBCE_FORMAT,
		T3X_VID_CMPR_AFBCE_MODE_EN,
		T3X_VID_CMPR_AFBCE_DWSCALAR,
		T3X_VID_CMPR_AFBCE_DEFCOLOR_1,
		T3X_VID_CMPR_AFBCE_DEFCOLOR_2,
		T3X_VID_CMPR_AFBCE_QUANT_ENABLE,
		T3X_VID_CMPR_AFBCE_IQUANT_LUT_1,
		T3X_VID_CMPR_AFBCE_IQUANT_LUT_2,
		T3X_VID_CMPR_AFBCE_IQUANT_LUT_3,
		T3X_VID_CMPR_AFBCE_IQUANT_LUT_4,
		T3X_VID_CMPR_AFBCE_RQUANT_LUT_1,
		T3X_VID_CMPR_AFBCE_RQUANT_LUT_2,
		T3X_VID_CMPR_AFBCE_RQUANT_LUT_3,
		T3X_VID_CMPR_AFBCE_RQUANT_LUT_4,
		T3X_VID_CMPR_AFBCE_YUV_FORMAT_CONV_MODE,
		T3X_VID_CMPR_AFBCE_DUMMY_DATA,
		T3X_VID_CMPR_AFBCE_CLR_FLAG,
		T3X_VID_CMPR_AFBCE_STA_FLAGT,
		T3X_VID_CMPR_AFBCE_MMU_NUM,
		T3X_VID_CMPR_AFBCE_PIP_CTRL,
		T3X_VID_CMPR_AFBCE_ROT_CTRL,
		T3X_VID_CMPR_AFBCE_DIMM_CTRL,
		T3X_VID_CMPR_AFBCE_BND_DEC_MISC,
		T3X_VID_CMPR_AFBCE_RD_ARB_MISC,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_CTRL1,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_CTRL2,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_CTRL3,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_CTRL4,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_SCOPE_X,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_SCOPE_Y,
		T3X_VID_CMPR_AFBCE_MMU_RMIF_RO_STAT,
	}
};

struct vicp_lossy_compress_reg_s vicp_loss_reg_array[VICP_SUPPORT_CHIP_MAX] = {
	{
	},
	{
		T3X_VID_CMPR_AFBCE_LOSS_CTRL,
		T3X_VID_CMPR_AFBCE_LOSS_BURST_NUM,
		T3X_VID_CMPR_AFBCE_LOSS_RC,
		T3X_VID_CMPR_AFBCE_LOSS_RC_FIFO_THD,
		T3X_VID_CMPR_AFBCE_LOSS_RC_FIFO_BUGET,
		T3X_VID_CMPR_AFBCE_LOSS_RC_ACCUM_THD_0,
		T3X_VID_CMPR_AFBCE_LOSS_RC_ACCUM_THD_1,
		T3X_VID_CMPR_AFBCE_LOSS_RC_ACCUM_THD_2,
		T3X_VID_CMPR_AFBCE_LOSS_RC_ACCUM_THD_3,
		T3X_VID_CMPR_AFBCE_LOSS_RC_ACCUM_BUGET_0,
		T3X_VID_CMPR_AFBCE_LOSS_RC_ACCUM_BUGET_1,
		T3X_VID_CMPR_AFBCE_LOSS_RO_ERROR_L_0,
		T3X_VID_CMPR_AFBCE_LOSS_RO_COUNT_0,
		T3X_VID_CMPR_AFBCE_LOSS_RO_ERROR_L_1,
		T3X_VID_CMPR_AFBCE_LOSS_RO_COUNT_1,
		T3X_VID_CMPR_AFBCE_LOSS_RO_ERROR_L_2,
		T3X_VID_CMPR_AFBCE_LOSS_RO_COUNT_2,
		T3X_VID_CMPR_AFBCE_LOSS_RO_ERROR_H_0,
		T3X_VID_CMPR_AFBCE_LOSS_RO_ERROR_H_1,
		T3X_VID_CMPR_AFBCE_LOSS_RO_MAX_ERROR_0,
		T3X_VID_CMPR_AFBCE_LOSS_RO_MAX_ERROR_1,
	}
};

static int check_map_flag(void)
{
	int ret = 0;

	if (vicp_reg_map) {
		ret = 1;
	} else {
		vicp_print(VICP_ERROR, "%s: reg map failed\n", __func__);
		ret = 0;
	}

	return ret;
}

u32 vicp_reg_read(u32 reg)
{
	u32 addr = 0;
	u32 val = 0;

	addr = VICPBUS_REG_ADDR(reg);
	if (check_map_flag())
		val = readl(vicp_reg_map + addr);

	return val;
}

void vicp_reg_write(u32 reg, u32 val)
{
	u32 addr = 0;

	addr = VICPBUS_REG_ADDR(reg);
	if (check_map_flag())
		writel(val, vicp_reg_map + addr);
}

u32 vicp_vcbus_read(u32 reg)
{
	u32 val = 0;

#ifdef CONFIG_AMLOGIC_IOMAP
	val = aml_read_vcbus(reg);
#endif
	return val;
};

void vicp_vcbus_write(u32 reg, u32 val)
{
#ifdef CONFIG_AMLOGIC_IOMAP
	aml_write_vcbus(reg, val);
#endif
}

u32 vicp_reg_get_bits(u32 reg, const u32 start, const u32 len)
{
	u32 val;

	val = (vicp_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	return val;
}

void vicp_reg_set_bits(u32 reg, const u32 value, const u32 start, const u32 len)
{
	u32 set_val = (vicp_reg_read(reg) & ~(((1L << len) - 1) << start)) |
			((value & ((1L << len) - 1)) << start);
	vicp_reg_write(reg, set_val);
}

void vicp_reg_write_addr(u64 addr, u64 data)
{
	writel(data, ((void __iomem *)(phys_to_virt(addr))));
}

u64 vicp_reg_read_addr(u64 addr)
{
	u32 val;

	val = readl((void __iomem *)(phys_to_virt(addr)));

	return (val & 0xffffffff);
}

u32 vicp_reg_array_init(enum vicp_support_chip_e chip, enum vicp_module_e module, void *array)
{
	u32 ret = 0;

	if (!array) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		ret = -1;
	} else {
		switch (module) {
		case VICP_MODULE_RDMIF:
		case VICP_MODULE_AFBCD:
		case VICP_MODULE_SCALER:
		case VICP_MODULE_HDR:
		case VICP_MODULE_CROP:
		case VICP_MODULE_SHRINK:
		case VICP_MODULE_WRMIF:
			vicp_print(VICP_ERROR, "%s: no need copy.\n", __func__);
			break;
		case VICP_MODULE_AFBCE:
			memcpy(array,
				&vicp_afbce_reg_array[chip],
				sizeof(struct vicp_afbce_reg_s));
			break;
		case VICP_MODULE_LOSSY_COMPRESS:
			memcpy(array,
				&vicp_loss_reg_array[chip],
				sizeof(struct vicp_lossy_compress_reg_s));
			break;
		case VICP_MODULE_MAX:
		default:
			vicp_print(VICP_ERROR, "%s: invalid module.\n", __func__);
			ret = -1;
			break;
		}
	}

	return ret;
}
