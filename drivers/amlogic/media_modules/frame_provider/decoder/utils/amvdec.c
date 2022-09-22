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
#define DEBUG
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/utils/vformat.h>
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
#include "../../../stream_input/amports/amports_priv.h"

/* #include <mach/am_regs.h> */
/* #include <mach/power_gate.h> */
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "amvdec.h"
#include <linux/amlogic/media/utils/amports_config.h>
#include "firmware.h"
//#include <linux/amlogic/tee.h>
#include <uapi/linux/tee.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"

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
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8)
			WRITE_VREG(GCLK_EN, 0x3ff);
		/* #endif */
		CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 31);
	} else {

		AMVDEC_CLK_GATE_OFF(AMRISC);
		timeout = jiffies + HZ / 100;

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
		timeout = jiffies + HZ / 100;

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
		timeout = jiffies + HZ / 100;

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
		timeout = jiffies + HZ / 100;
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
		const char *name, char *def, s32(*load)(const u32 *))
{
	int err;

	if (!vdec->mc_loaded) {
		if (!def) {
			err = get_decoder_firmware_data(vdec->format,
						name, (u8 *)(vdec->mc),
						(4096 * 4 * 4));
			if (err <= 0)
				return -1;
		} else
			memcpy((char *)vdec->mc, def, sizeof(vdec->mc));

		vdec->mc_loaded = true;
	}

	err = (*load)(vdec->mc);
	if (err < 0) {
		pr_err("loading firmware %s to vdec ram  failed!\n", name);
		return err;
	}

	return err;
}

static s32 am_vdec_loadmc_buf_ex(struct vdec_s *vdec,
		char *buf, int size, s32(*load)(const u32 *))
{
	int err;

	if (!vdec->mc_loaded) {
		memcpy((u8 *)(vdec->mc), buf, size);
		vdec->mc_loaded = true;
	}

	err = (*load)(vdec->mc);
	if (err < 0) {
		pr_err("loading firmware to vdec ram  failed!\n");
		return err;
	}

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
		vfree(mc_addr);
		return err;
	}
	vfree(mc_addr);

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
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL | GFP_DMA32);
	}

	if (!mc_addr)
		return -ENOMEM;

	memcpy(mc_addr, p, MC_SIZE);

	mc_addr_map = dma_map_single(get_vdec_device(),
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

	dma_unmap_single(get_vdec_device(),
		mc_addr_map, MC_SIZE, DMA_TO_DEVICE);

#ifndef AMVDEC_USE_STATIC_MEMORY
	kfree(mc_addr);
	mc_addr = NULL;
#endif

	return ret;
}

s32 optee_load_fw(enum vformat_e type, const char *fw_name)
{
	s32 ret = -1;
	unsigned int format = FIRMWARE_MAX;
	unsigned int vdec = OPTEE_VDEC_LEGENCY;
	char *name = __getname();
	bool is_swap = false;

	sprintf(name, "%s", fw_name ? fw_name : "null");

	switch ((u32)type) {
	case VFORMAT_VC1:
		format = VIDEO_DEC_VC1;
		break;

	case VFORMAT_AVS:
		if (!strcmp(name, "avs_no_cabac"))
			format = VIDEO_DEC_AVS_NOCABAC;
		else if (!strcmp(name, "avs_multi"))
			format = VIDEO_DEC_AVS_MULTI;
		else
			format = VIDEO_DEC_AVS;
		break;

	case VFORMAT_MPEG12:
		if (!strcmp(name, "mpeg12"))
			format = VIDEO_DEC_MPEG12;
		else if (!strcmp(name, "mmpeg12"))
			format = VIDEO_DEC_MPEG12_MULTI;
		break;

	case VFORMAT_MJPEG:
		if (!strcmp(name, "mmjpeg"))
			format = VIDEO_DEC_MJPEG_MULTI;
		else
			format = VIDEO_DEC_MJPEG;
		break;

	case VFORMAT_VP9:
		if (!strcmp(name, "vp9_mc"))
			format = VIDEO_DEC_VP9;
		else
			format = VIDEO_DEC_VP9_MMU;
		break;

	case VFORMAT_AVS2:
		format = VIDEO_DEC_AVS2_MMU;
		vdec = OPTEE_VDEC_HEVC;
		break;

	case VFORMAT_AV1:
		format = VIDEO_DEC_AV1_MMU;
		vdec = OPTEE_VDEC_HEVC;
		break;

	case VFORMAT_HEVC:
		if (!strcmp(name, "h265_mmu"))
			format = VIDEO_DEC_HEVC_MMU;
		else if (!strcmp(name, "hevc_mmu_swap")) {
			format = VIDEO_DEC_HEVC_MMU_SWAP;
			vdec = OPTEE_VDEC_HEVC;
			is_swap = true;
		} else
			format = VIDEO_DEC_HEVC;
		break;

	case VFORMAT_REAL:
		if (!strcmp(name, "vreal_mc_8"))
			format = VIDEO_DEC_REAL_V8;
		else if (!strcmp(name, "vreal_mc_9"))
			format = VIDEO_DEC_REAL_V9;
		break;

	case VFORMAT_MPEG4:
		if (!strcmp(name, "mmpeg4_mc_5"))
			format = VIDEO_DEC_MPEG4_5_MULTI;
		else if ((!strcmp(name, "mh263_mc")))
			format = VIDEO_DEC_H263_MULTI;
		else if (!strcmp(name, "vmpeg4_mc_5"))
			format = VIDEO_DEC_MPEG4_5;
		else if (!strcmp(name, "h263_mc"))
			format = VIDEO_DEC_H263;
		/*not support now*/
		else if (!strcmp(name, "vmpeg4_mc_311"))
			format = VIDEO_DEC_MPEG4_3;
		else if (!strcmp(name, "vmpeg4_mc_4"))
			format = VIDEO_DEC_MPEG4_4;
		break;

	case VFORMAT_H264_4K2K:
		if (!strcmp(name, "single_core"))
			format = VIDEO_DEC_H264_4k2K_SINGLE;
		else
			format = VIDEO_DEC_H264_4k2K;
		break;

	case VFORMAT_H264MVC:
		format = VIDEO_DEC_H264_MVC;
		break;

	case VFORMAT_H264:
		if (!strcmp(name, "mh264"))
			format = VIDEO_DEC_H264_MULTI;
		else if (!strcmp(name, "mh264_mmu")) {
			format = VIDEO_DEC_H264_MULTI_MMU;
			vdec = OPTEE_VDEC_HEVC;
		} else
			format = VIDEO_DEC_H264;
		break;
	case VFORMAT_JPEG_ENC:
		format = VIDEO_ENC_JPEG;
		vdec = OPTEE_VDEC_HCDEC;
		break;
	case VFORMAT_H264_ENC:
		format = VIDEO_ENC_H264;
		vdec = OPTEE_VDEC_HCDEC;
		break;
	default:
		pr_info("Unknow vdec format: %u\n", (u32)type);
		break;
	}

	if (format < FIRMWARE_MAX) {
		if (is_swap)
			ret = tee_load_video_fw_swap(format, vdec, is_swap);
		else
			ret = tee_load_video_fw(format, vdec);
	}

	__putname(name);

	return ret;
}
EXPORT_SYMBOL(optee_load_fw);

s32 amvdec_loadmc_ex(enum vformat_e type, const char *name, char *def)
{
	if (tee_enabled())
		return optee_load_fw(type, name);
	else
		return am_loadmc_ex(type, name, def, &amvdec_loadmc);
}
EXPORT_SYMBOL(amvdec_loadmc_ex);

s32 amvdec_vdec_loadmc_ex(enum vformat_e type, const char *name,
	struct vdec_s *vdec, char *def)
{
	if (tee_enabled())
		return optee_load_fw(type, name);
	else
		return am_vdec_loadmc_ex(vdec, name, def, &amvdec_loadmc);
}
EXPORT_SYMBOL(amvdec_vdec_loadmc_ex);

s32 amvdec_vdec_loadmc_buf_ex(enum vformat_e type, const char *name,
	struct vdec_s *vdec, char *buf, int size)
{
	if (tee_enabled())
		return optee_load_fw(type, name);
	else
		return am_vdec_loadmc_buf_ex(vdec, buf, size, &amvdec_loadmc);
}
EXPORT_SYMBOL(amvdec_vdec_loadmc_buf_ex);

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
			mc_addr = kmalloc(MC_SIZE, GFP_KERNEL | GFP_DMA32);
		}

		if (!mc_addr)
			return -ENOMEM;

		memcpy(mc_addr, p, MC_SIZE);

		mc_addr_map = dma_map_single(get_vdec_device(),
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

		dma_unmap_single(get_vdec_device(),
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
EXPORT_SYMBOL(amvdec2_loadmc_ex);

s32 amhcodec_loadmc(const u32 *p)
{
#ifdef AMVDEC_USE_STATIC_MEMORY
	if (mc_addr == NULL) {
#else
	{
#endif
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL | GFP_DMA32);
	}

	if (!mc_addr)
		return -ENOMEM;

	memcpy(mc_addr, p, MC_SIZE);

	mc_addr_map = dma_map_single(get_vdec_device(),
			mc_addr, MC_SIZE, DMA_TO_DEVICE);

	WRITE_VREG(HCODEC_IMEM_DMA_ADR, mc_addr_map);
	WRITE_VREG(HCODEC_IMEM_DMA_COUNT, 0x100);
	WRITE_VREG(HCODEC_IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

	while (READ_VREG(HCODEC_IMEM_DMA_CTRL) & 0x8000)
		udelay(1000);

	dma_unmap_single(get_vdec_device(),
			mc_addr_map, MC_SIZE, DMA_TO_DEVICE);

#ifndef AMVDEC_USE_STATIC_MEMORY
	kfree(mc_addr);
#endif

	return 0;
}
EXPORT_SYMBOL(amhcodec_loadmc);

s32 amhcodec_loadmc_ex(enum vformat_e type, const char *name, char *def)
{
	return am_loadmc_ex(type, name, def, &amhcodec_loadmc);
}
EXPORT_SYMBOL(amhcodec_loadmc_ex);

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
			mc_addr = kmalloc(MC_SIZE, GFP_KERNEL | GFP_DMA32);
		}

		if (!mc_addr)
			return -ENOMEM;

		memcpy(mc_addr, p, MC_SIZE);

		mc_addr_map =
			dma_map_single(get_vdec_device(),
			mc_addr, MC_SIZE, DMA_TO_DEVICE);

		WRITE_VREG(HEVC_MPSR, 0);
		WRITE_VREG(HEVC_CPSR, 0);

		/* Read CBUS register for timing */
		timeout = READ_VREG(HEVC_MPSR);
		timeout = READ_VREG(HEVC_MPSR);

		timeout = jiffies + HZ;

		WRITE_VREG(HEVC_IMEM_DMA_ADR, mc_addr_map);
		WRITE_VREG(HEVC_IMEM_DMA_COUNT, 0x1000);
		if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3))
			WRITE_VREG(HEVC_IMEM_DMA_CTRL, (0x8000 | (0xf << 16)));
		else
			WRITE_VREG(HEVC_IMEM_DMA_CTRL, (0x8000 | (0x7 << 16)));

		while (READ_VREG(HEVC_IMEM_DMA_CTRL) & 0x8000) {
			if (time_before(jiffies, timeout))
				schedule();
			else {
				pr_err("hevc load mc error\n");
				ret = -EBUSY;
				break;
			}
		}

		dma_unmap_single(get_vdec_device(),
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
		if (tee_enabled())
			return optee_load_fw(type, name);
		else
			return am_loadmc_ex(type, name, def, &amhevc_loadmc);
	else
		return -1;
}
EXPORT_SYMBOL(amhevc_loadmc_ex);

s32 amhevc_vdec_loadmc_ex(enum vformat_e type, struct vdec_s *vdec,
	const char *name, char *def)
{
	if (has_hevc_vdec())
		if (tee_enabled())
			return optee_load_fw(type, name);
		else
			return am_vdec_loadmc_ex(vdec, name, def, &amhevc_loadmc);
	else
		return -1;
}
EXPORT_SYMBOL(amhevc_vdec_loadmc_ex);

void amvdec_start(void)
{
#ifdef CONFIG_WAKELOCK
	amvdec_wake_lock();
#endif

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
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
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);

		WRITE_RESET_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
	}
	/* #endif */

	WRITE_VREG(MPSR, 0x0001);
}
EXPORT_SYMBOL(amvdec_start);

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
EXPORT_SYMBOL(amvdec2_start);

void amhcodec_start(void)
{
	WRITE_VREG(HCODEC_MPSR, 0x0001);
}
EXPORT_SYMBOL(amhcodec_start);

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
EXPORT_SYMBOL(amhevc_start);

void amvdec_stop(void)
{
	ulong timeout = jiffies + HZ/10;

	WRITE_VREG(MPSR, 0);
	WRITE_VREG(CPSR, 0);

	while (READ_VREG(IMEM_DMA_CTRL) & 0x8000) {
		if (time_after(jiffies, timeout))
			break;
	}

	timeout = jiffies + HZ/10;
	while (READ_VREG(LMEM_DMA_CTRL) & 0x8000) {
		if (time_after(jiffies, timeout))
			break;
	}

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
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
		WRITE_RESET_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

		/* additional cbus dummy register reading for timing control */
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
		READ_RESET_REG(RESET0_REGISTER);
	}
	/* #endif */

#ifdef CONFIG_WAKELOCK
	amvdec_wake_unlock();
#endif
}
EXPORT_SYMBOL(amvdec_stop);

void amvdec2_stop(void)
{
	if (has_vdec2()) {
		ulong timeout = jiffies + HZ/10;

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
EXPORT_SYMBOL(amvdec2_stop);

void amhcodec_stop(void)
{
	WRITE_VREG(HCODEC_MPSR, 0);
}
EXPORT_SYMBOL(amhcodec_stop);

void amhevc_stop(void)
{
	if (has_hevc_vdec()) {
		ulong timeout = jiffies + HZ/10;

		WRITE_VREG(HEVC_MPSR, 0);
		WRITE_VREG(HEVC_CPSR, 0);

		while (READ_VREG(HEVC_IMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}

		timeout = jiffies + HZ/10;
		while (READ_VREG(HEVC_LMEM_DMA_CTRL) & 0x8000) {
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
EXPORT_SYMBOL(amhevc_stop);

void amvdec_enable(void)
{
	amvdec_pg_enable(true);
}
EXPORT_SYMBOL(amvdec_enable);

void amvdec_disable(void)
{
	amvdec_pg_enable(false);
}
EXPORT_SYMBOL(amvdec_disable);

void amvdec2_enable(void)
{
	if (has_vdec2())
		amvdec2_pg_enable(true);
}
EXPORT_SYMBOL(amvdec2_enable);

void amvdec2_disable(void)
{
	if (has_vdec2())
		amvdec2_pg_enable(false);
}
EXPORT_SYMBOL(amvdec2_disable);

void amhevc_enable(void)
{
	if (has_hevc_vdec())
		amhevc_pg_enable(true);
}
EXPORT_SYMBOL(amhevc_enable);

void amhevc_disable(void)
{
	if (has_hevc_vdec())
		amhevc_pg_enable(false);
}
EXPORT_SYMBOL(amhevc_disable);

#ifdef CONFIG_PM
int amvdec_suspend(struct platform_device *dev, pm_message_t event)
{
	struct vdec_s *vdec = *(struct vdec_s **)dev->dev.platform_data;;

	if (vdec) {
		wait_event_interruptible_timeout(vdec->idle_wait,
			(vdec->status != VDEC_STATUS_ACTIVE),
			msecs_to_jiffies(100));
	}

	amvdec_pg_enable(false);

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
	if (has_vdec2())
		amvdec2_pg_enable(false);
	/* #endif */

	if (has_hevc_vdec())
		amhevc_pg_enable(false);
	/*vdec_set_suspend_clk(1, 0);*//*DEBUG_TMP*/
	return 0;
}
EXPORT_SYMBOL(amvdec_suspend);

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
	/*vdec_set_suspend_clk(0, 0);*//*DEBUG_TMP*/
	return 0;
}
EXPORT_SYMBOL(amvdec_resume);

int amhevc_suspend(struct platform_device *dev, pm_message_t event)
{
	struct vdec_s *vdec = *(struct vdec_s **)dev->dev.platform_data;;

	if (vdec) {
		wait_event_interruptible_timeout(vdec->idle_wait,
			(vdec->status != VDEC_STATUS_ACTIVE),
			msecs_to_jiffies(100));
	}

	if (has_hevc_vdec()) {
		amhevc_pg_enable(false);
		/*vdec_set_suspend_clk(1, 1);*//*DEBUG_TMP*/
	}
	return 0;
}
EXPORT_SYMBOL(amhevc_suspend);

int amhevc_resume(struct platform_device *dev)
{
	if (has_hevc_vdec()) {
		amhevc_pg_enable(true);
		/*vdec_set_suspend_clk(0, 1);*//*DEBUG_TMP*/
	}
	return 0;
}
EXPORT_SYMBOL(amhevc_resume);


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
		(rp == old_rp && wp == old_wp && level == old_level)) {
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
EXPORT_SYMBOL(amvdev_pause);

int amvdev_resume(void)
{
	video_running = 1;
	video_stated_changed = 1;
	return 0;
}
EXPORT_SYMBOL(amvdev_resume);

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

int amvdec_init(void)
{
#ifdef CONFIG_WAKELOCK
	/*
	 *wake_lock_init(&amvdec_lock, WAKE_LOCK_IDLE, "amvdec_lock");
	 *tmp mark for compile, no "WAKE_LOCK_IDLE" definition in kernel 3.8
	 */
	wake_lock_init(&amvdec_lock, /*WAKE_LOCK_IDLE */ WAKE_LOCK_SUSPEND,
				   "amvdec_lock");

	init_timer(&amvdevtimer);

	amvdevtimer.data = (ulong) &amvdevtimer;
	amvdevtimer.function = vdec_paused_check_timer;
#endif
	return 0;
}
EXPORT_SYMBOL(amvdec_init);

void amvdec_exit(void)
{
#ifdef CONFIG_WAKELOCK
	del_timer_sync(&amvdevtimer);
#endif
}
EXPORT_SYMBOL(amvdec_exit);

#if 0
int __init amvdec_init(void)
{
#ifdef CONFIG_WAKELOCK
	/*
	 *wake_lock_init(&amvdec_lock, WAKE_LOCK_IDLE, "amvdec_lock");
	 *tmp mark for compile, no "WAKE_LOCK_IDLE" definition in kernel 3.8
	 */
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
}

module_init(amvdec_init);
module_exit(amvdec_exit);
#endif

MODULE_DESCRIPTION("Amlogic Video Decoder Utility Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
