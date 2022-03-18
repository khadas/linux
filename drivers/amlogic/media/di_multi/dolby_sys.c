// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/dolby_sys.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include "dolby_sys.h"
#include "register.h"
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#define OP_BYPASS_CVM				0x01
#define OP_BYPASS_CSC				0x02
#define OP_CLKGATE_WHEN_LOAD_LUT	0x04

#define DI_FLAG_CHANGE_TC			0x000010

#define DIFLAG_FORCE_CVM				0x01
#define DIFLAG_BYPASS_CVM				0x02
#define DIFLAG_BYPASS_VPP				0x04
#define DIFLAG_USE_SINK_MIN_MAX		0x08
#define DIFLAG_CLKGATE_WHEN_LOAD_LUT	0x10
#define DIFLAG_BYPASS_CSC				0x200
struct di_dolby_hw_s {
	/* SWAP_CTRL2 */
	u32 vsize;
	u32 hsize;
	/* SWAP_CTRL1 */
	u32 vtotal;
	u32 htotal;
	/* SWAP_CTRL3 */
	u32 vwidth;
	u32 hwidth;
	/* SWAP_CTRL4 */
	u32 vpotch;
	u32 hpotch;
	/* SWAP_CTRL0 */
	bool bl_enable;
	bool el_enable;
	bool el_41_mode;
	u8 bl_lay_line_delay;
	u8 el_lay_line_delay;
	bool roundup_output; //add 0.5 for 8bit mode

	/* SWAP_CTRL5 */
	u8 bl_uv_mode;
	u8 el_uv_mode;

	u32 dm_count;
	u32 comp_count;
	u32 lut_count;
	struct composer_reg_ipcore *comp_reg;
	struct dm_reg_ipcore1 *dm_reg;
	struct dm_lut_ipcore *dm_lut;

	struct composer_reg_ipcore *last_comp_reg;
	struct dm_reg_ipcore1 *last_dm_reg;
	struct dm_lut_ipcore *last_dm_lut;
};

enum disignal_format_e {
	FORMAT_INVALID = -1,
	FORMAT_DOVI = 0,
	FORMAT_HDR10 = 1,
	FORMAT_SDR = 2,
	FORMAT_DOVI_LL = 3,
	FORMAT_HLG = 4,
	FORMAT_HDR10PLUS = 5,
	FORMAT_SDR_2020 = 6,
	FORMAT_MVC = 7,
	FORMAT_CUVA = 8
};

struct di_dolby_dev_s {
	enum disignal_format_e src_fmt;

	bool bypass_csc;
	bool bypass_cvm;
	bool lut_endian;

	u8 next_tbl_id;
	u8 cur_tbl_id;

	//struct mutex di_mutex;

	u32 update_flag[2];
	struct composer_reg_ipcore comp_reg[2];
	struct dm_reg_ipcore1 dm_reg[2];
	struct dm_lut_ipcore dm_lut[2];
	struct di_dolby_hw_s hw;
};

static struct di_dolby_dev_s *di_dolby;

void di_dolby_enable(bool enable)
{
	if (enable) {
		DIM_DI_WR_REG_BITS(DI_SC_TOP_CTRL, 1, 24, 1);//dolby enable
		DIM_DI_WR_REG_BITS(DI_SC_TOP_CTRL, 1, 19, 1);//dither enable
	} else {
		DIM_DI_WR_REG_BITS(DI_SC_TOP_CTRL, 0, 24, 1);//dolby enable
		DIM_DI_WR_REG_BITS(DI_SC_TOP_CTRL, 0, 19, 1);//dither enable
	}
}

static int di_dolby_core_set(struct di_dolby_hw_s *hw,
			     bool lut_endian,
			     u32 op_flag,
			     u32 update_flag,
			     bool reset)
{
	u32 count;
	u32 bypass_flag = 0;
	int i;
	bool set_lut = false;
	u32 dm_count;
	u32 comp_count;
	u32 lut_count;
	u32 *dm_regs;
	u32 *comp_regs;
	u32 *lut_regs;
	u32 *last_dm;
	u32 *last_comp;

	if (!hw)
		return -1;

	dm_count = hw->dm_count;
	comp_count =  hw->comp_count;
	lut_count = hw->lut_count;
	dm_regs = (uint32_t *)hw->dm_reg;
	comp_regs = (uint32_t *)hw->comp_reg;
	lut_regs = (uint32_t *)hw->dm_lut;
	last_dm = (uint32_t *)hw->last_dm_reg;
	last_comp = (uint32_t *)hw->last_comp_reg;

	//reset = true;

	if (update_flag & DI_FLAG_CHANGE_TC)
		set_lut = true;

	DIM_DI_WR(AMDV_CORE1C_CLKGATE_CTRL, 0);
	DIM_DI_WR(AMDV_CORE1C_SWAP_CTRL1,
		  ((hw->hsize + hw->htotal) << 16) | (hw->vsize + hw->vtotal));
	DIM_DI_WR(AMDV_CORE1C_SWAP_CTRL3,
		  (hw->hwidth << 16) | hw->vwidth);
	DIM_DI_WR(AMDV_CORE1C_SWAP_CTRL4,
		  (hw->hpotch << 16) | hw->vpotch);
	DIM_DI_WR(AMDV_CORE1C_SWAP_CTRL2,
		  (hw->hsize << 16) | hw->vsize);

	DIM_DI_WR(AMDV_CORE1C_SWAP_CTRL5,
		  ((hw->el_uv_mode & 3) << 2) | ((hw->bl_uv_mode & 3) << 0));

	DIM_DI_WR(AMDV_CORE1C_DMA_CTRL, 0x0);
	DIM_DI_WR(AMDV_CORE1C_REG_START + 4, 4);
	DIM_DI_WR(AMDV_CORE1C_REG_START + 2, 1);

	if (!hw->el_enable)
		bypass_flag |= 1 << 3;
	if (op_flag & DIFLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (op_flag & DIFLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (hw->el_41_mode)
		bypass_flag |= 1 << 3;

	DIM_DI_WR(AMDV_CORE1C_REG_START + 1,
		  0x70 | bypass_flag); // bypass CVM and/or CSC
	DIM_DI_WR(AMDV_CORE1C_REG_START + 1,
		  0x70 | bypass_flag); // for delay

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset ||
		    dm_regs[i] != last_dm[i]) {
			DIM_DI_WR(AMDV_CORE1C_REG_START + 6 + i,
				  dm_regs[i]);
		}

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		if (reset ||
		    comp_regs[i] != last_comp[i]) {
			DIM_DI_WR(AMDV_CORE1C_REG_START + 6 + 44 + i,
				  comp_regs[i]);
		}
	/* metadata program done */
	DIM_DI_WR(AMDV_CORE1C_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
		if (op_flag & DIFLAG_CLKGATE_WHEN_LOAD_LUT) {
			DIM_DI_WR_REG_BITS(AMDV_CORE1C_CLKGATE_CTRL,
					   2, 2, 2);
		}
		DIM_DI_WR(AMDV_CORE1C_DMA_CTRL, 0x1401);
		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				DIM_DI_WR(AMDV_CORE1C_DMA_PORT,
					  lut_regs[i + 3]);
				DIM_DI_WR(AMDV_CORE1C_DMA_PORT,
					  lut_regs[i + 2]);
				DIM_DI_WR(AMDV_CORE1C_DMA_PORT,
					  lut_regs[i + 1]);
				DIM_DI_WR(AMDV_CORE1C_DMA_PORT,
					  lut_regs[i]);
			}
		} else {
			for (i = 0; i < count; i++)
				DIM_DI_WR(AMDV_CORE1C_DMA_PORT,
					  lut_regs[i]);
		}
		if (op_flag & DIFLAG_CLKGATE_WHEN_LOAD_LUT) {
			DIM_DI_WR_REG_BITS(AMDV_CORE1C_CLKGATE_CTRL,
					   0, 2, 2);
		}
	}

	DIM_DI_WR(DI_HDR_IN_HSIZE, hw->hsize);
	DIM_DI_WR(DI_HDR_IN_VSIZE, hw->vsize);

	DIM_DI_WR(AMDV_CORE1C_SWAP_CTRL0,
		  ((hw->roundup_output ? 1 : 0) << 12) |
		  ((hw->bl_lay_line_delay & 0xf) << 8) |
		  ((hw->el_lay_line_delay & 0xf) << 4) |
		  ((hw->el_41_mode ? 1 : 0) << 2) |
		  ((hw->el_enable ? 1 : 0) << 1) |
		  ((hw->bl_enable ? 1 : 0) << 0));
	return 0;
}

int di_dolby_do_setting(void /*struct di_dolby_dev_s *dev*/)
{
	u32 op_flag = 0;
	u32 update_flag = 0;
	int tmp;

	struct di_dolby_dev_s *dev = di_dolby;

	if (!dev)
		return -1;

	//mutex_lock(&dev->di_mutex);
	if (dev->cur_tbl_id == 0xff) {
		//mutex_unlock(&dev->di_mutex);
		return -2;
	}

	if (dev->bypass_csc)
		op_flag |= DIFLAG_BYPASS_CSC;
	if (dev->bypass_cvm)
		op_flag |= DIFLAG_BYPASS_CVM;

	dev->hw.dm_reg = &dev->dm_reg[dev->cur_tbl_id];
	dev->hw.comp_reg = &dev->comp_reg[dev->cur_tbl_id];
	dev->hw.dm_lut = &dev->dm_lut[dev->cur_tbl_id];
	update_flag = dev->update_flag[dev->cur_tbl_id];
	//mutex_unlock(&dev->di_mutex);

	tmp = di_dolby_core_set(&dev->hw, dev->lut_endian,
				op_flag, update_flag, true);
	if (tmp)
		return -3;
	dev->hw.last_dm_reg = dev->hw.dm_reg;
	dev->hw.last_comp_reg = dev->hw.comp_reg;
	dev->hw.last_dm_lut = dev->hw.dm_lut;
	return 0;
}

int di_dolby_update_setting(struct dm_reg_ipcore1 *dm_reg,
			    struct composer_reg_ipcore *comp_reg,
			    struct dm_lut_ipcore *dm_lut,
			    u32 dm_count,
			    u32 comp_count,
			    u32 lut_count,
			    u32 update_flag,
			    u32 hsize,
			    u32 vsize)
{
	struct dm_reg_ipcore1 *new_dm_reg;
	struct composer_reg_ipcore *new_comp_reg;
	struct dm_lut_ipcore *new_dm_lut;

	if (!dm_reg || !comp_reg || !dm_lut || !dm_count || !comp_count ||
	    !lut_count)
		return -1;

	di_dolby->hw.hsize = hsize;
	di_dolby->hw.vsize = vsize;
	//mutex_lock(&di_dolby->di_mutex);
	if (di_dolby->next_tbl_id == 0xff)
		di_dolby->next_tbl_id = di_dolby->cur_tbl_id ^ 1;
	new_dm_reg = &di_dolby->dm_reg[di_dolby->next_tbl_id];
	new_comp_reg = &di_dolby->comp_reg[di_dolby->next_tbl_id];
	new_dm_lut = &di_dolby->dm_lut[di_dolby->next_tbl_id];
	di_dolby->update_flag[di_dolby->next_tbl_id] = update_flag;
	di_dolby->cur_tbl_id = di_dolby->next_tbl_id;
	di_dolby->next_tbl_id = di_dolby->next_tbl_id ^ 1;

	if (dm_count > di_dolby->hw.dm_count)
		dm_count = di_dolby->hw.dm_count;

	if (comp_count > di_dolby->hw.comp_count)
		comp_count = di_dolby->hw.comp_count;

	if (lut_count > di_dolby->hw.lut_count)
		lut_count = di_dolby->hw.lut_count;

	memcpy(new_dm_reg, dm_reg, sizeof(uint32_t) * dm_count);
	memcpy(new_comp_reg, comp_reg, sizeof(uint32_t) * comp_count);
	memcpy(new_dm_lut, dm_lut, sizeof(uint32_t) * lut_count);
	//mutex_unlock(&di_dolby->di_mutex);
	return 0;
}

void di_dolby_sw_init(void)
{
	di_dolby = vmalloc(sizeof(*di_dolby));
	if (!di_dolby) {
		//pr_info("di_dolby_sw_init fail\n");
		return;
	}
	memset(di_dolby, 0, sizeof(struct di_dolby_dev_s));

	//mutex_init(&di_dolby->di_mutex);

	di_dolby->hw.hsize = 1920;
	di_dolby->hw.vsize = 1080;

	di_dolby->hw.vtotal = 0x60; //0x40
	di_dolby->hw.htotal = 0x160; //0x80
	di_dolby->hw.vwidth = 0x8;
	di_dolby->hw.hwidth = 0x8;
	di_dolby->hw.vpotch = 0x4; //0x10
	di_dolby->hw.hpotch = 0x8;
	di_dolby->hw.bl_enable = true;
	di_dolby->hw.el_enable = false;
	di_dolby->hw.el_41_mode = true;
	di_dolby->hw.bl_lay_line_delay = 0;
	di_dolby->hw.el_lay_line_delay = 0;
	di_dolby->hw.roundup_output = true;
	di_dolby->hw.bl_uv_mode = 2;
	di_dolby->hw.el_uv_mode = 1;
	di_dolby->hw.dm_count = 27;
	di_dolby->hw.comp_count = 173;
	di_dolby->hw.lut_count = 256 * 5;

	di_dolby->bypass_cvm = false;
	di_dolby->bypass_csc = false;
	di_dolby->src_fmt = FORMAT_INVALID;
	di_dolby->lut_endian = true;
	di_dolby->cur_tbl_id = 1;
	di_dolby->next_tbl_id = 0xff;
}
