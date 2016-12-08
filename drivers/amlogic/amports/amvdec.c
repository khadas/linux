/*
 * drivers/amlogic/amports/amvdec.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include "vdec.h"

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#endif
#include "amports_priv.h"

/* #include <mach/am_regs.h> */
/* #include <mach/power_gate.h> */
#include "vdec_reg.h"
#include "amvdec.h"
#include "amports_config.h"
#include "arch/firmware.h"

#define MC_SIZE (4096 * 16)

#ifdef CONFIG_WAKELOCK
static struct wake_lock amvdec_lock;
struct timer_list amvdevtimer;
#define WAKE_CHECK_INTERVAL (100*HZ/100)
#endif
#define AMVDEC_USE_STATIC_MEMORY
static void *mc_addr;
static dma_addr_t mc_addr_map;

#ifdef CONFIG_WAKELOCK
static int video_running;
static int video_stated_changed = 1;
#endif

static void amvdec_pg_enable(bool enable)
{
	ulong timeout;
	if (enable) {
		AMVDEC_CLK_GATE_ON(MDEC_CLK_PIC_DC);
		AMVDEC_CLK_GATE_ON(MDEC_CLK_DBLK);
		AMVDEC_CLK_GATE_ON(MC_CLK);
		AMVDEC_CLK_GATE_ON(IQIDCT_CLK);
		/* AMVDEC_CLK_GATE_ON(VLD_CLK); */
		AMVDEC_CLK_GATE_ON(AMRISC);
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
			WRITE_VREG(GCLK_EN, 0x3ff);
		/* #endif */
		CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 31);
	} else {

		AMVDEC_CLK_GATE_OFF(AMRISC);
		timeout = jiffies + HZ / 10;

		while (READ_VREG(MDEC_PIC_DC_STATUS) != 0) {
			if (time_after(jiffies, timeout)) {
				WRITE_VREG_BITS(MDEC_PIC_DC_CTRL, 1, 0, 1);
				WRITE_VREG_BITS(MDEC_PIC_DC_CTRL, 0, 0, 1);
				READ_VREG(MDEC_PIC_DC_STATUS);
				READ_VREG(MDEC_PIC_DC_STATUS);
				READ_VREG(MDEC_PIC_DC_STATUS);
				break;
			}
		}

		AMVDEC_CLK_GATE_OFF(MDEC_CLK_PIC_DC);
		timeout = jiffies + HZ / 10;

		while (READ_VREG(DBLK_STATUS) & 1) {
			if (time_after(jiffies, timeout)) {
				WRITE_VREG(DBLK_CTRL, 3);
				WRITE_VREG(DBLK_CTRL, 0);
				READ_VREG(DBLK_STATUS);
				READ_VREG(DBLK_STATUS);
				READ_VREG(DBLK_STATUS);
				break;
			}
		}
		AMVDEC_CLK_GATE_OFF(MDEC_CLK_DBLK);
		timeout = jiffies + HZ / 10;

		while (READ_VREG(MC_STATUS0) & 1) {
			if (time_after(jiffies, timeout)) {
				SET_VREG_MASK(MC_CTRL1, 0x9);
				CLEAR_VREG_MASK(MC_CTRL1, 0x9);
				READ_VREG(MC_STATUS0);
				READ_VREG(MC_STATUS0);
				READ_VREG(MC_STATUS0);
				break;
			}
		}
		AMVDEC_CLK_GATE_OFF(MC_CLK);
		timeout = jiffies + HZ / 10;
		while (READ_VREG(DCAC_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}
		AMVDEC_CLK_GATE_OFF(IQIDCT_CLK);
		/* AMVDEC_CLK_GATE_OFF(VLD_CLK); */
	}
}

static void amvdec2_pg_enable(bool enable)
{
	if (has_vdec2()) {
		ulong timeout;
		if (!vdec_on(VDEC_2))
			return;
		if (enable) {
			/* WRITE_VREG(VDEC2_GCLK_EN, 0x3ff); */
		} else {
			timeout = jiffies + HZ / 10;

			while (READ_VREG(VDEC2_MDEC_PIC_DC_STATUS) != 0) {
				if (time_after(jiffies, timeout)) {
					WRITE_VREG_BITS(VDEC2_MDEC_PIC_DC_CTRL,
							1, 0, 1);
					WRITE_VREG_BITS(VDEC2_MDEC_PIC_DC_CTRL,
							0, 0, 1);
					READ_VREG(VDEC2_MDEC_PIC_DC_STATUS);
					READ_VREG(VDEC2_MDEC_PIC_DC_STATUS);
					READ_VREG(VDEC2_MDEC_PIC_DC_STATUS);
					break;
				}
			}

			timeout = jiffies + HZ / 10;

			while (READ_VREG(VDEC2_DBLK_STATUS) & 1) {
				if (time_after(jiffies, timeout)) {
					WRITE_VREG(VDEC2_DBLK_CTRL, 3);
					WRITE_VREG(VDEC2_DBLK_CTRL, 0);
					READ_VREG(VDEC2_DBLK_STATUS);
					READ_VREG(VDEC2_DBLK_STATUS);
					READ_VREG(VDEC2_DBLK_STATUS);
					break;
				}
			}

			timeout = jiffies + HZ / 10;

			while (READ_VREG(VDEC2_DCAC_DMA_CTRL) & 0x8000) {
				if (time_after(jiffies, timeout))
					break;
			}
		}
	}
}

static void amhevc_pg_enable(bool enable)
{
	if (has_hevc_vdec()) {
		ulong timeout;
		if (!vdec_on(VDEC_HEVC))
			return;
		if (enable) {
			/* WRITE_VREG(VDEC2_GCLK_EN, 0x3ff); */
		} else {
			timeout = jiffies + HZ / 10;

			while (READ_VREG(HEVC_MDEC_PIC_DC_STATUS) != 0) {
				if (time_after(jiffies, timeout)) {
					WRITE_VREG_BITS(HEVC_MDEC_PIC_DC_CTRL,
						1, 0, 1);
					WRITE_VREG_BITS(HEVC_MDEC_PIC_DC_CTRL,
						0, 0, 1);
					READ_VREG(HEVC_MDEC_PIC_DC_STATUS);
					READ_VREG(HEVC_MDEC_PIC_DC_STATUS);
					READ_VREG(HEVC_MDEC_PIC_DC_STATUS);
					break;
				}
			}

			timeout = jiffies + HZ / 10;

			while (READ_VREG(HEVC_DBLK_STATUS) & 1) {
				if (time_after(jiffies, timeout)) {
					WRITE_VREG(HEVC_DBLK_CTRL, 3);
					WRITE_VREG(HEVC_DBLK_CTRL, 0);
					READ_VREG(HEVC_DBLK_STATUS);
					READ_VREG(HEVC_DBLK_STATUS);
					READ_VREG(HEVC_DBLK_STATUS);
					break;
				}
			}

			timeout = jiffies + HZ / 10;

			while (READ_VREG(HEVC_DCAC_DMA_CTRL) & 0x8000) {
				if (time_after(jiffies, timeout))
					break;
			}
		}
	}
}

#ifdef CONFIG_WAKELOCK
int amvdec_wake_lock(void)
{
	wake_lock(&amvdec_lock);
	return 0;
}

int amvdec_wake_unlock(void)
{
	wake_unlock(&amvdec_lock);
	return 0;
}
#else
#define amvdec_wake_lock()
#define amvdec_wake_unlock()
#endif

static s32 am_vdec_loadmc_ex(struct vdec_s *vdec,
		const char *name, s32(*load)(const u32 *))
{
	int err;

	if (!vdec->mc_loaded) {
		int loaded;
		loaded = get_decoder_firmware_data(vdec->format,
					name, (u8 *)(vdec->mc), (4096 * 4 * 4));
		if (loaded <= 0)
			return -1;

		vdec->mc_loaded = true;
	}

	err = (*load)(vdec->mc);
	if (err < 0) {
		pr_err("loading firmware %s to vdec ram  failed!\n", name);
		return err;
	}
	pr_debug("loading firmware %s to vdec ram  ok!\n", name);
	return err;
}

static s32 am_loadmc_ex(enum vformat_e type,
		const char *name, char *def, s32(*load)(const u32 *))
{
	char *mc_addr = vmalloc(4096 * 16);
	char *pmc_addr = def;
	int err;

	if (!def && mc_addr) {
		int loaded;
		loaded = get_decoder_firmware_data(type,
					name, mc_addr, (4096 * 16));
		if (loaded > 0)
			pmc_addr = mc_addr;
	}
	if (!pmc_addr) {
		vfree(mc_addr);
		return -1;
	}
	err = (*load)((u32 *) pmc_addr);
	if (err < 0) {
		pr_err("loading firmware %s to vdec ram  failed!\n", name);
		return err;
	}
	vfree(mc_addr);
	pr_debug("loading firmware %s to vdec ram  ok!\n", name);
	return err;
}

static s32 amvdec_loadmc(const u32 *p)
{
	ulong timeout;
	s32 ret = 0;

#ifdef AMVDEC_USE_STATIC_MEMORY
	if (mc_addr == NULL) {
#else
	{
#endif
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
	}

	if (!mc_addr)
		return -ENOMEM;

	memcpy(mc_addr, p, MC_SIZE);

	mc_addr_map = dma_map_single(amports_get_dma_device(),
		mc_addr, MC_SIZE, DMA_TO_DEVICE);

	WRITE_VREG(MPSR, 0);
	WRITE_VREG(CPSR, 0);

	/* Read CBUS register for timing */
	timeout = READ_VREG(MPSR);
	timeout = READ_VREG(MPSR);

	timeout = jiffies + HZ;

	WRITE_VREG(IMEM_DMA_ADR, mc_addr_map);
	WRITE_VREG(IMEM_DMA_COUNT, 0x1000);
	WRITE_VREG(IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

	while (READ_VREG(IMEM_DMA_CTRL) & 0x8000) {
		if (time_before(jiffies, timeout))
			schedule();
		else {
			pr_err("vdec load mc error\n");
			ret = -EBUSY;
			break;
		}
	}

	dma_unmap_single(amports_get_dma_device(),
		mc_addr_map, MC_SIZE, DMA_TO_DEVICE);

#ifndef AMVDEC_USE_STATIC_MEMORY
	kfree(mc_addr);
	mc_addr = NULL;
#endif

	return ret;
}

s32 amvdec_loadmc_ex(enum vformat_e type, const char *name, char *def)
{
	return am_loadmc_ex(type, name, def, &amvdec_loadmc);
}

s32 amvdec_vdec_loadmc_ex(struct vdec_s *vdec, const char *name)
{
	return am_vdec_loadmc_ex(vdec, name, &amvdec_loadmc);
}

static s32 amvdec2_loadmc(const u32 *p)
{
	if (has_vdec2()) {
		ulong timeout;
		s32 ret = 0;

#ifdef AMVDEC_USE_STATIC_MEMORY
		if (mc_addr == NULL) {
#else
		{
#endif
			mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
		}

		if (!mc_addr)
			return -ENOMEM;

		memcpy(mc_addr, p, MC_SIZE);

		mc_addr_map = dma_map_single(amports_get_dma_device(),
			mc_addr, MC_SIZE, DMA_TO_DEVICE);

		WRITE_VREG(VDEC2_MPSR, 0);
		WRITE_VREG(VDEC2_CPSR, 0);

		/* Read CBUS register for timing */
		timeout = READ_VREG(VDEC2_MPSR);
		timeout = READ_VREG(VDEC2_MPSR);

		timeout = jiffies + HZ;

		WRITE_VREG(VDEC2_IMEM_DMA_ADR, mc_addr_map);
		WRITE_VREG(VDEC2_IMEM_DMA_COUNT, 0x1000);
		WRITE_VREG(VDEC2_IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

		while (READ_VREG(VDEC2_IMEM_DMA_CTRL) & 0x8000) {
			if (time_before(jiffies, timeout))
				schedule();
			else {
				pr_err("vdec2 load mc error\n");
				ret = -EBUSY;
				break;
			}
		}

		dma_unmap_single(amports_get_dma_device(),
			mc_addr_map, MC_SIZE, DMA_TO_DEVICE);

#ifndef AMVDEC_USE_STATIC_MEMORY
		kfree(mc_addr);
		mc_addr = NULL;
#endif

		return ret;
	} else
		return 0;
}

s32 amvdec2_loadmc_ex(enum vformat_e type, const char *name, char *def)
{
	if (has_vdec2())
		return am_loadmc_ex(type, name, def, &amvdec2_loadmc);
	else
		return 0;
}

s32 amhcodec_loadmc(const u32 *p)
{
#ifdef AMVDEC_USE_STATIC_MEMORY
	if (mc_addr == NULL) {
#else
	{
#endif
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
	}

	if (!mc_addr)
		return -ENOMEM;

	memcpy(mc_addr, p, MC_SIZE);

	mc_addr_map = dma_map_single(amports_get_dma_device(),
			mc_addr, MC_SIZE, DMA_TO_DEVICE);

	WRITE_VREG(HCODEC_IMEM_DMA_ADR, mc_addr_map);
	WRITE_VREG(HCODEC_IMEM_DMA_COUNT, 0x100);
	WRITE_VREG(HCODEC_IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

	while (READ_VREG(HCODEC_IMEM_DMA_CTRL) & 0x8000)
		udelay(1000);

	dma_unmap_single(amports_get_dma_device(),
			mc_addr_map, MC_SIZE, DMA_TO_DEVICE);

#ifndef AMVDEC_USE_STATIC_MEMORY
	kfree(mc_addr);
#endif

	return 0;
}

s32 amhcodec_loadmc_ex(enum vformat_e type, const char *name, char *def)
{
	return am_loadmc_ex(type, name, def, &amhcodec_loadmc);
}

static s32 amhevc_loadmc(const u32 *p)
{
	ulong timeout;
	s32 ret = 0;

	if (has_hevc_vdec()) {
#ifdef AMVDEC_USE_STATIC_MEMORY
		if (mc_addr == NULL) {
#else
		{
#endif
			mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
		}

		if (!mc_addr)
			return -ENOMEM;

		memcpy(mc_addr, p, MC_SIZE);

		mc_addr_map =
			dma_map_single(amports_get_dma_device(),
			mc_addr, MC_SIZE, DMA_TO_DEVICE);

		WRITE_VREG(HEVC_MPSR, 0);
		WRITE_VREG(HEVC_CPSR, 0);

		/* Read CBUS register for timing */
		timeout = READ_VREG(HEVC_MPSR);
		timeout = READ_VREG(HEVC_MPSR);

		timeout = jiffies + HZ;

		WRITE_VREG(HEVC_IMEM_DMA_ADR, mc_addr_map);
		WRITE_VREG(HEVC_IMEM_DMA_COUNT, 0x1000);
		WRITE_VREG(HEVC_IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

		while (READ_VREG(HEVC_IMEM_DMA_CTRL) & 0x8000) {
			if (time_before(jiffies, timeout))
				schedule();
			else {
				pr_err("vdec2 load mc error\n");
				ret = -EBUSY;
				break;
			}
		}

		dma_unmap_single(amports_get_dma_device(),
				mc_addr_map, MC_SIZE, DMA_TO_DEVICE);

#ifndef AMVDEC_USE_STATIC_MEMORY
		kfree(mc_addr);
		mc_addr = NULL;
#endif
	}

	return ret;
}

s32 amhevc_loadmc_ex(enum vformat_e type, const char *name, char *def)
{
	if (has_hevc_vdec())
		return am_loadmc_ex(type, name, def, &amhevc_loadmc);
	else
		return 0;
}

s32 amhevc_vdec_loadmc_ex(struct vdec_s *vdec, const char *name)
{
	if (has_hevc_vdec())
		return am_vdec_loadmc_ex(vdec, name, &amhevc_loadmc);
	else
		return 0;
}

void amvdec_start(void)
{
#ifdef CONFIG_WAKELOCK
	amvdec_wake_lock();
#endif

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);

		WRITE_VREG(DOS_SW_RESET0, (1 << 12) | (1 << 11));
		WRITE_VREG(DOS_SW_RESET0, 0);

		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
	} else {
		/* #else */
		/* additional cbus dummy register reading for timing control */
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);

		WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
	}
	/* #endif */

	WRITE_VREG(MPSR, 0x0001);
}

void amvdec2_start(void)
{
	if (has_vdec2()) {
#ifdef CONFIG_WAKELOCK
		amvdec_wake_lock();
#endif

		READ_VREG(DOS_SW_RESET2);
		READ_VREG(DOS_SW_RESET2);
		READ_VREG(DOS_SW_RESET2);

		WRITE_VREG(DOS_SW_RESET2, (1 << 12) | (1 << 11));
		WRITE_VREG(DOS_SW_RESET2, 0);

		READ_VREG(DOS_SW_RESET2);
		READ_VREG(DOS_SW_RESET2);
		READ_VREG(DOS_SW_RESET2);

		WRITE_VREG(VDEC2_MPSR, 0x0001);
	}
}

void amhcodec_start(void)
{
	WRITE_VREG(HCODEC_MPSR, 0x0001);
}

void amhevc_start(void)
{

	if (has_hevc_vdec()) {
#ifdef CONFIG_WAKELOCK
		amvdec_wake_lock();
#endif

		READ_VREG(DOS_SW_RESET3);
		READ_VREG(DOS_SW_RESET3);
		READ_VREG(DOS_SW_RESET3);

		WRITE_VREG(DOS_SW_RESET3, (1 << 12) | (1 << 11));
		WRITE_VREG(DOS_SW_RESET3, 0);

		READ_VREG(DOS_SW_RESET3);
		READ_VREG(DOS_SW_RESET3);
		READ_VREG(DOS_SW_RESET3);

		WRITE_VREG(HEVC_MPSR, 0x0001);
	}
}

void amvdec_stop(void)
{
	ulong timeout = jiffies + HZ;

	WRITE_VREG(MPSR, 0);
	WRITE_VREG(CPSR, 0);

	while (READ_VREG(IMEM_DMA_CTRL) & 0x8000) {
		if (time_after(jiffies, timeout))
			break;
	}

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);

		WRITE_VREG(DOS_SW_RESET0, (1 << 12) | (1 << 11));
		WRITE_VREG(DOS_SW_RESET0, 0);

		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
	} else {
		/* #else */
		WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

		/* additional cbus dummy register reading for timing control */
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
		READ_MPEG_REG(RESET0_REGISTER);
	}
	/* #endif */

#ifdef CONFIG_WAKELOCK
	amvdec_wake_unlock();
#endif
}

void amvdec2_stop(void)
{
	if (has_vdec2()) {
		ulong timeout = jiffies + HZ;

		WRITE_VREG(VDEC2_MPSR, 0);
		WRITE_VREG(VDEC2_CPSR, 0);

		while (READ_VREG(VDEC2_IMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}

		READ_VREG(DOS_SW_RESET2);
		READ_VREG(DOS_SW_RESET2);
		READ_VREG(DOS_SW_RESET2);

#ifdef CONFIG_WAKELOCK
		amvdec_wake_unlock();
#endif
	}
}

void amhcodec_stop(void)
{
	WRITE_VREG(HCODEC_MPSR, 0);
}

void amhevc_stop(void)
{
	if (has_hevc_vdec()) {
		ulong timeout = jiffies + HZ;

		WRITE_VREG(HEVC_MPSR, 0);
		WRITE_VREG(HEVC_CPSR, 0);

		while (READ_VREG(HEVC_IMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}

		READ_VREG(DOS_SW_RESET3);
		READ_VREG(DOS_SW_RESET3);
		READ_VREG(DOS_SW_RESET3);

#ifdef CONFIG_WAKELOCK
		amvdec_wake_unlock();
#endif
	}
}

void amvdec_enable(void)
{
	amvdec_pg_enable(true);
}

void amvdec_disable(void)
{
	amvdec_pg_enable(false);
}

void amvdec2_enable(void)
{
	if (has_vdec2())
		amvdec2_pg_enable(true);
}

void amvdec2_disable(void)
{
	if (has_vdec2())
		amvdec2_pg_enable(false);
}

void amhevc_enable(void)
{
	if (has_hevc_vdec())
		amhevc_pg_enable(true);
}

void amhevc_disable(void)
{
	if (has_hevc_vdec())
		amhevc_pg_enable(false);
}

#ifdef CONFIG_PM
int amvdec_suspend(struct platform_device *dev, pm_message_t event)
{
	amvdec_pg_enable(false);

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
	if (has_vdec2())
		amvdec2_pg_enable(false);
	/* #endif */

	if (has_hevc_vdec())
		amhevc_pg_enable(false);

	return 0;
}

int amvdec_resume(struct platform_device *dev)
{
	amvdec_pg_enable(true);

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
	if (has_vdec2())
		amvdec2_pg_enable(true);
	/* #endif */

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec())
		amhevc_pg_enable(true);
	/* #endif */

	return 0;
}

int amhevc_suspend(struct platform_device *dev, pm_message_t event)
{
	if (has_hevc_vdec())
		amhevc_pg_enable(false);
	return 0;
}

int amhevc_resume(struct platform_device *dev)
{
	if (has_hevc_vdec())
		amhevc_pg_enable(true);
	return 0;
}


#endif

#ifdef CONFIG_WAKELOCK

static int vdec_is_paused(void)
{
	static unsigned long old_wp = -1, old_rp = -1, old_level = -1;
	unsigned long wp, rp, level;
	static int paused_time;

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec()) {
		if ((vdec_on(VDEC_HEVC))
			&& (READ_VREG(HEVC_STREAM_CONTROL) & 1)) {
			wp = READ_VREG(HEVC_STREAM_WR_PTR);
			rp = READ_VREG(HEVC_STREAM_RD_PTR);
			level = READ_VREG(HEVC_STREAM_LEVEL);
		} else {
			wp = READ_VREG(VLD_MEM_VIFIFO_WP);
			rp = READ_VREG(VLD_MEM_VIFIFO_RP);
			level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
		}
	} else
		/* #endif */
	{
		wp = READ_VREG(VLD_MEM_VIFIFO_WP);
		rp = READ_VREG(VLD_MEM_VIFIFO_RP);
		level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
	}
	/*have data,but output buffer is full */
	if ((rp == old_rp && level > 1024) ||
		(rp == old_rp && wp == old_wp && level == level)) {
		/*no write && not read */
		paused_time++;
	} else {
		paused_time = 0;
	}
	old_wp = wp; old_rp = rp; old_level = level;
	if (paused_time > 10)
		return 1;
	return 0;
}

int amvdev_pause(void)
{
	video_running = 0;
	video_stated_changed = 1;
	return 0;
}

int amvdev_resume(void)
{
	video_running = 1;
	video_stated_changed = 1;
	return 0;
}

static void vdec_paused_check_timer(unsigned long arg)
{
	if (video_stated_changed) {
		if (!video_running) {
			if (vdec_is_paused()) {
				pr_info("vdec paused and release wakelock now\n");
				amvdec_wake_unlock();
				video_stated_changed = 0;
			}
		} else {
			amvdec_wake_lock();
			video_stated_changed = 0;
		}
	}
	mod_timer(&amvdevtimer, jiffies + WAKE_CHECK_INTERVAL);
}
#else
int amvdev_pause(void)
{
	return 0;
}

int amvdev_resume(void)
{
	return 0;
}
#endif

int __init amvdec_init(void)
{
#ifdef CONFIG_WAKELOCK
	/* wake_lock_init(&amvdec_lock, WAKE_LOCK_IDLE, "amvdec_lock");
	*tmp mark for compile, no "WAKE_LOCK_IDLE" definition in kernel 3.8 */
	wake_lock_init(&amvdec_lock, /*WAKE_LOCK_IDLE */ WAKE_LOCK_SUSPEND,
				   "amvdec_lock");

	init_timer(&amvdevtimer);

	amvdevtimer.data = (ulong) &amvdevtimer;
	amvdevtimer.function = vdec_paused_check_timer;
#endif
	return 0;
}

static void __exit amvdec_exit(void)
{
#ifdef CONFIG_WAKELOCK
	del_timer_sync(&amvdevtimer);
#endif
	return;
}

module_init(amvdec_init);
module_exit(amvdec_exit);

MODULE_DESCRIPTION("Amlogic Video Decoder Utility Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
