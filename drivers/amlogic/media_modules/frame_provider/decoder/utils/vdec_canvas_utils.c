 /*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/types.h>
#include "vdec_canvas_utils.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "vdec.h"

static struct canvas_status_s canvas_stat[CANVAS_MAX_SIZE];
static struct canvas_status_s mdec_cav_stat[MDEC_CAV_LUT_MAX];
static struct canvas_config_s *mdec_cav_pool = NULL;

extern u32 vdec_get_debug(void);


bool is_support_vdec_canvas(void)
{
	/* vdec canvas note:
	 * 1. canvas params config to display, do not use
	 *    vf->canvasxAddr, should use vf->canvasxconfig[].
	 * 2. the endian can not config with canvas. and hevc
	 *    core should not config canvas. config endian in
	 *    probe function like h265/vp9/av1/avs2.
	*/
	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5W))
		return true;
	return false;
}
EXPORT_SYMBOL(is_support_vdec_canvas);

static int get_canvas(unsigned int index, unsigned int base)
{
	int start;
	int canvas_index = index * base;
	int ret = 0;

	if ((base > 4) || (base == 0))
		return -1;

	if ((AMVDEC_CANVAS_START_INDEX + canvas_index + base - 1)
		<= AMVDEC_CANVAS_MAX1) {
		start = AMVDEC_CANVAS_START_INDEX + base * index;
	} else {
		canvas_index -= (AMVDEC_CANVAS_MAX1 -
			AMVDEC_CANVAS_START_INDEX + 1) / base * base;
		if (canvas_index <= AMVDEC_CANVAS_MAX2)
			start = canvas_index / base;
		else
			return -1;
	}

	if (base == 1) {
		ret = start;
	} else if (base == 2) {
		ret = ((start + 1) << 16) | ((start + 1) << 8) | start;
	} else if (base == 3) {
		ret = ((start + 2) << 16) | ((start + 1) << 8) | start;
	} else if (base == 4) {
		ret = (((start + 3) << 24) | (start + 2) << 16) |
			((start + 1) << 8) | start;
	}

	return ret;
}

static int get_canvas_ex(int type, int id)
{
	int i;
	unsigned long flags;

	flags = vdec_canvas_lock();

	for (i = 0; i < CANVAS_MAX_SIZE; i++) {
		/*0x10-0x15 has been used by rdma*/
		if ((i >= 0x10) && (i <= 0x15))
				continue;
		if ((canvas_stat[i].type == type) &&
			(canvas_stat[i].id & (1 << id)) == 0) {
			canvas_stat[i].canvas_used_flag++;
			canvas_stat[i].id |= (1 << id);
			if (vdec_get_debug() & 4)
				pr_debug("get used canvas %d\n", i);
			vdec_canvas_unlock(flags);
			if (i < AMVDEC_CANVAS_MAX2 + 1)
				return i;
			else
				return (i + AMVDEC_CANVAS_START_INDEX - AMVDEC_CANVAS_MAX2 - 1);
		}
	}

	for (i = 0; i < CANVAS_MAX_SIZE; i++) {
		/*0x10-0x15 has been used by rdma*/
		if ((i >= 0x10) && (i <= 0x15))
				continue;
		if (canvas_stat[i].type == 0) {
			canvas_stat[i].type = type;
			canvas_stat[i].canvas_used_flag = 1;
			canvas_stat[i].id = (1 << id);
			if (vdec_get_debug() & 4) {
				pr_debug("get canvas %d\n", i);
				pr_debug("canvas_used_flag %d\n",
					canvas_stat[i].canvas_used_flag);
				pr_debug("canvas_stat[i].id %d\n",
					canvas_stat[i].id);
			}
			vdec_canvas_unlock(flags);
			if (i < AMVDEC_CANVAS_MAX2 + 1)
				return i;
			else
				return (i + AMVDEC_CANVAS_START_INDEX - AMVDEC_CANVAS_MAX2 - 1);
		}
	}
	vdec_canvas_unlock(flags);

	pr_info("cannot get canvas\n");

	return -1;
}

static void free_canvas_ex(int index, int id)
{
	unsigned long flags;
	int offset;

	flags = vdec_canvas_lock();
	if (index >= 0 &&
		index < AMVDEC_CANVAS_MAX2 + 1)
		offset = index;
	else if ((index >= AMVDEC_CANVAS_START_INDEX) &&
		(index <= AMVDEC_CANVAS_MAX1))
		offset = index + AMVDEC_CANVAS_MAX2 + 1 - AMVDEC_CANVAS_START_INDEX;
	else {
		vdec_canvas_unlock(flags);
		return;
	}

	if ((canvas_stat[offset].canvas_used_flag > 0) &&
		(canvas_stat[offset].id & (1 << id))) {
		canvas_stat[offset].canvas_used_flag--;
		canvas_stat[offset].id &= ~(1 << id);
		if (canvas_stat[offset].canvas_used_flag == 0) {
			canvas_stat[offset].type = 0;
			canvas_stat[offset].id = 0;
		}
		if (vdec_get_debug() & 4) {
			pr_debug("free index %d used_flag %d, type = %d, id = %d\n",
				offset,
				canvas_stat[offset].canvas_used_flag,
				canvas_stat[offset].type,
				canvas_stat[offset].id);
		}
	}
	vdec_canvas_unlock(flags);

	return;
}


static int get_internal_cav_lut(unsigned int index, unsigned int base)
{
	int start;
	int canvas_index = index * base;
	int ret = 0;

	if ((base > 4) || (base == 0))
		return -1;

	if (canvas_index + base - 1 < MDEC_CAV_LUT_MAX)
		start = canvas_index;
	else
		return -1;

	if (base == 1) {
		ret = start;
	} else if (base == 2) {
		ret = ((start + 1) << 16) | ((start + 1) << 8) | start;
	} else if (base == 3) {
		ret = ((start + 2) << 16) | ((start + 1) << 8) | start;
	} else if (base == 4) {
		ret = (((start + 3) << 24) | (start + 2) << 16) |
			((start + 1) << 8) | start;
	}

	return ret;
}

static int get_internal_cav_lut_ex(int type, int id)
{
	int i;
	unsigned long flags;

	flags = vdec_canvas_lock();

	for (i = 0; i < MDEC_CAV_LUT_MAX; i++) {
		if ((mdec_cav_stat[i].type == type) &&
			(mdec_cav_stat[i].id & (1 << id)) == 0) {
			mdec_cav_stat[i].canvas_used_flag++;
			mdec_cav_stat[i].id |= (1 << id);
			if (vdec_get_debug() & 4)
				pr_debug("get used cav lut %d\n", i);
			vdec_canvas_unlock(flags);
			return i;
		}
	}

	for (i = 0; i < MDEC_CAV_LUT_MAX; i++) {
		if (mdec_cav_stat[i].type == 0) {
			mdec_cav_stat[i].type = type;
			mdec_cav_stat[i].canvas_used_flag = 1;
			mdec_cav_stat[i].id = (1 << id);
			if (vdec_get_debug() & 4)
				pr_debug("get cav lut %d\n", i);
			vdec_canvas_unlock(flags);
			return i;
		}
	}
	vdec_canvas_unlock(flags);

	pr_info("cannot get cav lut\n");

	return -1;
}

static void free_internal_cav_lut(int index, int id)
{
	unsigned long flags;
	int offset;

	flags = vdec_canvas_lock();
	if (index > 0 && index < MDEC_CAV_LUT_MAX)
		offset = index;
	else {
		vdec_canvas_unlock(flags);
		return;
	}
	if ((mdec_cav_stat[offset].canvas_used_flag > 0) &&
		(mdec_cav_stat[offset].id & (1 << id))) {
		mdec_cav_stat[offset].canvas_used_flag--;
		mdec_cav_stat[offset].id &= ~(1 << id);
		if (mdec_cav_stat[offset].canvas_used_flag == 0) {
			mdec_cav_stat[offset].type = 0;
			mdec_cav_stat[offset].id = 0;
		}
		if (vdec_get_debug() & 4) {
			pr_debug("free index %d used_flag %d, type = %d, id = %d\n",
				offset,
				mdec_cav_stat[offset].canvas_used_flag,
				mdec_cav_stat[offset].type,
				mdec_cav_stat[offset].id);
		}
	}
	vdec_canvas_unlock(flags);

	return;
}

unsigned long vdec_cav_get_addr(int index)
{
	if (index < 0 || index >= MDEC_CAV_LUT_MAX) {
		pr_err("%s, error index %d\n", __func__, index);
		return -1;
	}

	return mdec_cav_pool[index & MDEC_CAV_INDEX_MASK].phy_addr;
}
EXPORT_SYMBOL(vdec_cav_get_addr);

unsigned int vdec_cav_get_width(int index)
{
	if (index < 0 || index >= MDEC_CAV_LUT_MAX) {
		pr_err("%s, error index %d\n", __func__, index);
		return -1;
	}

	return mdec_cav_pool[index & MDEC_CAV_INDEX_MASK].width;
}
EXPORT_SYMBOL(vdec_cav_get_width);

unsigned int vdec_cav_get_height(int index)
{
	if (index < 0 || index >= MDEC_CAV_LUT_MAX) {
		pr_err("%s, error index %d\n", __func__, index);
		return -1;
	}
	return mdec_cav_pool[index & MDEC_CAV_INDEX_MASK].height;
}
EXPORT_SYMBOL(vdec_cav_get_height);

void cav_lut_info_store(u32 index, ulong addr, u32 width,
	u32 height, u32 wrap, u32 blkmode, u32 endian)
{
	struct canvas_config_s *pool = NULL;

	if (index < 0 || index >= MDEC_CAV_LUT_MAX) {
		pr_err("%s, error index %d\n", __func__, index);
		return;
	}
	if (mdec_cav_pool == NULL)
		mdec_cav_pool = vzalloc(sizeof(struct canvas_config_s)
			* (MDEC_CAV_LUT_MAX + 1));

	if (mdec_cav_pool == NULL) {
		pr_err("%s failed, mdec_cav_pool null\n", __func__);
		return;
	}
	pool = &mdec_cav_pool[index];
	pool->width = width;
	pool->height = height;
	pool->block_mode = blkmode;
	pool->endian = endian;
	pool->phy_addr = addr;
}

void config_cav_lut_ex(u32 index, ulong addr, u32 width,
	u32 height, u32 wrap, u32 blkmode,
	u32 endian, enum vdec_type_e core)
{
	unsigned long datah_temp, datal_temp;

	if (!is_support_vdec_canvas()) {
		canvas_config_ex(index, addr, width, height, wrap, blkmode, endian);
		if (vdec_get_debug() & 0x40000000) {
			pr_info("%s %2d) addr: %lx, width: %d, height: %d, blkm: %d, endian: %d\n",
				__func__, index, addr, width, height, blkmode, endian);
		}
	} else {
		/*
		datal_temp = (cav_lut.start_addr & 0x1fffffff) |
			((cav_lut.cav_width & 0x7 ) << 29 );
		datah_temp = ((cav_lut.cav_width  >> 3) & 0x1ff) |
			(( cav_lut.cav_hight & 0x1fff) <<9 ) |
			((cav_lut.x_wrap_en & 1) << 22 ) |
			(( cav_lut.y_wrap_en & 1) << 23) |
			(( cav_lut.blk_mode & 0x3) << 24);
		*/
		u32 addr_bits_l = ((((addr + 7) >> 3) & CANVAS_ADDR_LMASK) << CAV_WADDR_LBIT);
		u32 width_l     = ((((width    + 7) >> 3) & CANVAS_WIDTH_LMASK) << CAV_WIDTH_LBIT);
		u32 width_h     = ((((width    + 7) >> 3) >> CANVAS_WIDTH_LWID) << CAV_WIDTH_HBIT);
		u32 height_h    = (height & CANVAS_HEIGHT_MASK) << CAV_HEIGHT_HBIT;
		u32 blkmod_h    = (blkmode & CANVAS_BLKMODE_MASK) << CAV_BLKMODE_HBIT;
		u32 switch_bits_ctl = (endian & 0xf) << CAV_ENDIAN_HBIT;
		u32 wrap_h      = (0 << 23);
		datal_temp = addr_bits_l | width_l;
		datah_temp = width_h | height_h | wrap_h | blkmod_h | switch_bits_ctl;

		if (core == VDEC_1) {
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T3) && (endian == 7))
				WRITE_VREG(MDEC_CAV_CFG0, 0x1ff << 17);
			else
				WRITE_VREG(MDEC_CAV_CFG0, 0); //[0]canv_mode, by default is non-canv-mode
			WRITE_VREG(MDEC_CAV_LUT_DATAL, datal_temp);
			WRITE_VREG(MDEC_CAV_LUT_DATAH, datah_temp);
			WRITE_VREG(MDEC_CAV_LUT_ADDR,  index);
		}

		cav_lut_info_store(index, addr, width, height, wrap, blkmode, endian);

		if (vdec_get_debug() & 0x40000000) {
			pr_info("%s %2d) addr: %lx, width: %d, height: %d, blkm: %d, endian: %d\n",
				__func__, index, addr, width, height, blkmode, endian);
			pr_info("data(h,l): 0x%8lx, 0x%8lx\n", datah_temp, datal_temp);
	    }
	}
}
EXPORT_SYMBOL(config_cav_lut_ex);

void config_cav_lut(int index, struct canvas_config_s *cfg,
	enum vdec_type_e core)
{
	config_cav_lut_ex(index,
		cfg->phy_addr,
		cfg->width,
		cfg->height,
		CANVAS_ADDR_NOWRAP,
		cfg->block_mode,
		cfg->endian,
		core);
}
EXPORT_SYMBOL(config_cav_lut);


void vdec_canvas_port_register(struct vdec_s *vdec)
{
	if (is_support_vdec_canvas()) {
		vdec->get_canvas = get_internal_cav_lut;
		vdec->get_canvas_ex = get_internal_cav_lut_ex;
		vdec->free_canvas_ex = free_internal_cav_lut;
		if (mdec_cav_pool == NULL) {
			mdec_cav_pool = vzalloc(sizeof(struct canvas_config_s)
			* (MDEC_CAV_LUT_MAX + 1));
		}
	} else {
		vdec->get_canvas = get_canvas;
		vdec->get_canvas_ex = get_canvas_ex;
		vdec->free_canvas_ex = free_canvas_ex;
	}
}
EXPORT_SYMBOL(vdec_canvas_port_register);
