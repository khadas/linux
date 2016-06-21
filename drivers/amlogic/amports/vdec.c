/*
 * drivers/amlogic/amports/vdec.c
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
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/iomap.h>
#include "vdec_reg.h"
#include "vdec.h"
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "amports_priv.h"

#include "amports_config.h"
#include "amvdec.h"


#include "arch/clk.h"
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

static DEFINE_MUTEX(vdec_mutex);

#define MC_SIZE (4096 * 4)
#define CMA_ALLOC_SIZE SZ_64M
#define MEM_NAME "vdec_prealloc"
#define SUPPORT_VCODEC_NUM  1
static int inited_vcodec_num;
static int poweron_clock_level;
static int keep_vdec_mem;
static unsigned int debug_trace_num = 16 * 20;
static unsigned int clk_config;

static int vdec_irq[VDEC_IRQ_MAX];
static struct platform_device *vdec_device;
static struct platform_device *vdec_core_device;
static struct page *vdec_cma_page;
int vdec_mem_alloced_from_codec, delay_release;
static unsigned long reserved_mem_start, reserved_mem_end;
static int hevc_max_reset_count;
static DEFINE_SPINLOCK(vdec_spin_lock);

#define HEVC_TEST_LIMIT 100
#define GXBB_REV_A_MINOR 0xA

struct am_reg {
	char *name;
	int offset;
};

static struct vdec_dev_reg_s vdec_dev_reg;

/*
 clk_config:
 0:default
 1:no gp0_pll;
 2:always used gp0_pll;
 >=10:fixed n M clk;
 == 100 , 100M clks;
*/
unsigned int get_vdec_clk_config_settings(void)
{
	return clk_config;
}
void update_vdec_clk_config_settings(unsigned int config)
{
	clk_config = config;
}

static bool hevc_workaround_needed(void)
{
	return (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) &&
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR)
			== GXBB_REV_A_MINOR);
}

struct device *get_codec_cma_device(void)
{
	return vdec_dev_reg.cma_dev;
}
static const char * const vdec_device_name[] = {
	"amvdec_mpeg12",
	"amvdec_mpeg4",
	"amvdec_h264",
	"amvdec_mjpeg",
	"amvdec_real",
	"amjpegdec",
	"amvdec_vc1",
	"amvdec_avs",
	"amvdec_yuv",
	"amvdec_h264mvc",
	"amvdec_h264_4k2k",
	"amvdec_h265",
	"amvenc_avc",
	"jpegenc",
	"amvdec_vp9"
};

static int vdec_default_buf_size[] = {
	32, /*"amvdec_mpeg12",*/
	32, /*"amvdec_mpeg4",*/
	48, /*"amvdec_h264",*/
	32, /*"amvdec_mjpeg",*/
	32, /*"amvdec_real",*/
	32, /*"amjpegdec",*/
	32, /*"amvdec_vc1",*/
	32, /*"amvdec_avs",*/
	32, /*"amvdec_yuv",*/
	64, /*"amvdec_h264mvc",*/
	64, /*"amvdec_h264_4k2k", else alloc on decoder*/
	48, /*"amvdec_h265", else alloc on decoder*/
	0,  /* avs encoder */
	0,  /* jpg encoder */
	32, /*"amvdec_vp9", else alloc on decoder*/
	0
};

void vdec_set_decinfo(struct dec_sysinfo *p)
{
	vdec_dev_reg.sys_info = p;
}

int vdec_set_resource(unsigned long start, unsigned long end, struct device *p)
{
	if (inited_vcodec_num != 0) {
		pr_info
		("ERROR:We can't support the change resource at running\n");
		return -1;
	}

	vdec_dev_reg.mem_start = ALIGN(start, SZ_64K);
	vdec_dev_reg.mem_end = end;
	vdec_dev_reg.cma_dev = p;

	reserved_mem_start = vdec_dev_reg.mem_start;
	reserved_mem_end = end;

	return 0;
}

s32 vdec_init(enum vformat_e vf, int is_4k)
{
	s32 r;
	int retry_num = 0;
	int more_buffers = 0;
	if (inited_vcodec_num >= SUPPORT_VCODEC_NUM) {
		pr_err("We only support the one video code at each time\n");
		return -EIO;
	}
	if (is_4k && vf < VFORMAT_H264) {
		/*old decoder don't support 4k
			but size is bigger;
			clear 4k flag, and used more buffers;
		*/
		more_buffers = 1;
		is_4k = 0;
	}
	if (vf == VFORMAT_H264_4K2K ||
		(vf == VFORMAT_HEVC && is_4k)) {
		try_free_keep_video();
	}
	inited_vcodec_num++;

	pr_debug("vdec_dev_reg.mem[0x%lx -- 0x%lx]\n",
		vdec_dev_reg.mem_start,
		vdec_dev_reg.mem_end);

/*retry alloc:*/
	while (vdec_dev_reg.mem_start == vdec_dev_reg.mem_end) {
		int alloc_size = vdec_default_buf_size[vf] * SZ_1M;
		if (alloc_size == 0)
			break;/*alloc end*/
		if (is_4k) {
			/*used 264 4k's setting for 265.*/
			int m4k_size =
				vdec_default_buf_size[VFORMAT_H264_4K2K] *
				SZ_1M;
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
				m4k_size = 32 * SZ_1M;
			if ((m4k_size > 0) && (m4k_size < 200 * SZ_1M))
				alloc_size = m4k_size;
		} else if (more_buffers) {
			alloc_size = alloc_size + 16 * SZ_1M;
		}
		vdec_dev_reg.mem_start = codec_mm_alloc_for_dma(MEM_NAME,
			alloc_size / PAGE_SIZE, 4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER);
		if (!vdec_dev_reg.mem_start) {
			if (retry_num < 1) {
				pr_err("vdec base CMA allocation failed,try again\\n");
				retry_num++;
				try_free_keep_video();
				continue;/*retry alloc*/
			}
			pr_err("vdec base CMA allocation failed.\n");
			inited_vcodec_num--;
			return -ENOMEM;
		}
		pr_debug("vdec base memory alloced %p\n",
		(void *)vdec_dev_reg.mem_start);

		vdec_dev_reg.mem_end = vdec_dev_reg.mem_start +
			alloc_size - 1;
		vdec_mem_alloced_from_codec = 1;
		break;/*alloc end*/
	}
/*alloc end:*/
	vdec_dev_reg.flag = 0;

	vdec_device =
		platform_device_register_data(&vdec_core_device->dev,
				vdec_device_name[vf], -1,
				&vdec_dev_reg, sizeof(vdec_dev_reg));

	if (IS_ERR(vdec_device)) {
		r = PTR_ERR(vdec_device);
		pr_err("vdec: Decoder device register failed (%d)\n", r);
		inited_vcodec_num--;
		goto error;
	}

	return 0;

error:
	vdec_device = NULL;

	inited_vcodec_num--;

	return r;
}

s32 vdec_release(enum vformat_e vf)
{

	if (vdec_device)
		platform_device_unregister(vdec_device);

	if (delay_release-- <= 0 &&
			!keep_vdec_mem &&
			vdec_mem_alloced_from_codec &&
			vdec_dev_reg.mem_start &&
			get_blackout_policy()) {
		codec_mm_free_for_dma(MEM_NAME, vdec_dev_reg.mem_start);
		vdec_cma_page = NULL;
		vdec_dev_reg.mem_start = reserved_mem_start;
		vdec_dev_reg.mem_end = reserved_mem_end;
	}


	inited_vcodec_num--;

	vdec_device = NULL;

	return 0;
}

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
static bool test_hevc(u32 decomp_addr, u32 us_delay)
{
	int i;

	/* SW_RESET IPP */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 1);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0);

	/* initialize all canvas table */
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
			0x1 | (i << 8) | decomp_addr);
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 1);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0<<1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

	/* Intialize mcrcc */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);
	WRITE_VREG(HEVCD_MCRCC_CTL2, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL3, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);

	/* Decomp initialize */
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x0);
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);

	/* Frame level initialization */
	WRITE_VREG(HEVCD_IPP_TOP_FRMCONFIG, 0x100 | (0x100 << 16));
	WRITE_VREG(HEVCD_IPP_TOP_TILECONFIG3, 0x0);
	WRITE_VREG(HEVCD_IPP_TOP_LCUCONFIG, 0x1 << 5);
	WRITE_VREG(HEVCD_IPP_BITDEPTH_CONFIG, 0x2 | (0x2 << 2));

	WRITE_VREG(HEVCD_IPP_CONFIG, 0x0);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, 0x0);

	/* Enable SWIMP mode */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CONFIG, 0x1);

	/* Enable frame */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x2);
	WRITE_VREG(HEVCD_IPP_TOP_FRMCTL, 0x1);

	/* Send SW-command CTB info */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CTBINFO, 0x1 << 31);

	/* Send PU_command */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO0, (0x4 << 9) | (0x4 << 16));
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO1, 0x1 << 3);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO2, 0x0);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO3, 0x0);

	udelay(us_delay);

	WRITE_VREG(HEVCD_IPP_DBG_SEL, 0x2 << 4);

	return (READ_VREG(HEVCD_IPP_DBG_DATA) & 3) == 1;
}

void vdec_poweron(enum vdec_type_e core)
{
	void *decomp_addr = NULL;
	dma_addr_t decomp_dma_addr;
	u32 decomp_addr_aligned = 0;
	int hevc_loop = 0;

	mutex_lock(&vdec_mutex);

	if (hevc_workaround_needed() &&
		(core == VDEC_HEVC)) {
		decomp_addr = dma_alloc_coherent(amports_get_dma_device(),
			SZ_64K + SZ_4K, &decomp_dma_addr, GFP_KERNEL);

		if (decomp_addr) {
			decomp_addr_aligned = ALIGN(decomp_dma_addr, SZ_64K);
			memset((u8 *)decomp_addr +
				(decomp_addr_aligned - decomp_dma_addr),
				0xff, SZ_4K);
		} else
			pr_err("vdec: alloc HEVC gxbb decomp buffer failed.\n");
	}

	if (core == VDEC_1) {
		/* vdec1 power on */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~0xc);
		/* wait 10uS */
		udelay(10);
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		/*add power on vdec clock level setting,only for m8 chip,
		   m8baby and m8m2 can dynamic adjust vdec clock,
		   power on with default clock level */
		vdec_clock_hi_enable();
		/* power up vdec memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0);
		/* remove vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~0xC0);
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
		if (get_cpu_type() >=
			MESON_CPU_MAJOR_ID_GXBB) {
			/*
			enable VDEC_1 DMC request
			*/
			unsigned long flags;
			spin_lock_irqsave(&vdec_spin_lock, flags);
			codec_dmcbus_write(DMC_REQ_CTRL,
				codec_dmcbus_read(DMC_REQ_CTRL) | (1 << 13));
			spin_unlock_irqrestore(&vdec_spin_lock, flags);
		}
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* vdec2 power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x30);
			/* wait 10uS */
			udelay(10);
			/* vdec2 soft reset */
			WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET2, 0);
			/* enable vdec1 clock */
			vdec2_clock_hi_enable();
			/* power up vdec memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0);
			/* remove vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x300);
			/* reset DOS top registers */
			WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* hcodec power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x3);
			/* wait 10uS */
			udelay(10);
			/* hcodec soft reset */
			WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET1, 0);
			/* enable hcodec clock */
			hcodec_clock_enable();
			/* power up hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0);
			/* remove hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x30);
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			bool hevc_fixed = false;

			while (!hevc_fixed) {
				/* hevc power on */
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0xc0);
				/* wait 10uS */
				udelay(10);
				/* hevc soft reset */
				WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
				WRITE_VREG(DOS_SW_RESET3, 0);
				/* enable hevc clock */
				hevc_clock_hi_enable();
				/* power up hevc memories */
				WRITE_VREG(DOS_MEM_PD_HEVC, 0);
				/* remove hevc isolation */
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0xc00);

				if (!hevc_workaround_needed())
					break;

				if (decomp_addr)
					hevc_fixed = test_hevc(
						decomp_addr_aligned, 20);

				if (!hevc_fixed) {
					hevc_loop++;

					mutex_unlock(&vdec_mutex);

					if (hevc_loop >= HEVC_TEST_LIMIT) {
						pr_warn("hevc power sequence over limit\n");
						pr_warn("=====================================================\n");
						pr_warn(" This chip is identified to have HW failure.\n");
						pr_warn(" Please contact sqa-platform to replace the platform.\n");
						pr_warn("=====================================================\n");

						panic("Force panic for chip detection !!!\n");

						break;
					}

					vdec_poweroff(VDEC_HEVC);

					mdelay(10);

					mutex_lock(&vdec_mutex);
				}
			}

			if (hevc_loop > hevc_max_reset_count)
				hevc_max_reset_count = hevc_loop;

			WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
			udelay(10);
			WRITE_VREG(DOS_SW_RESET3, 0);
		}
	}

	if (decomp_addr)
		dma_free_coherent(amports_get_dma_device(),
			SZ_64K + SZ_4K, decomp_addr, decomp_dma_addr);

	mutex_unlock(&vdec_mutex);
}

void vdec_poweroff(enum vdec_type_e core)
{
	mutex_lock(&vdec_mutex);
	if (core == VDEC_1) {
		if (get_cpu_type() >=
			MESON_CPU_MAJOR_ID_GXBB) {
			/* disable VDEC_1 DMC REQ
			*/
			unsigned long flags;
			spin_lock_irqsave(&vdec_spin_lock, flags);
			codec_dmcbus_write(DMC_REQ_CTRL,
				codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 13)));
			spin_unlock_irqrestore(&vdec_spin_lock, flags);
			udelay(10);
		}
		/* enable vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0xc0);
		/* power off vdec1 memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0xffffffffUL);
		/* disable vdec1 clock */
		vdec_clock_off();
		/* vdec1 power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0xc);
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* enable vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x300);
			/* power off vdec2 memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0xffffffffUL);
			/* disable vdec2 clock */
			vdec2_clock_off();
			/* vdec2 power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0x30);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* enable hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x30);
			/* power off hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
			/* disable hcodec clock */
			hcodec_clock_off();
			/* hcodec power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 3);
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			/* enable hevc isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0xc00);
			/* power off hevc memories */
			WRITE_VREG(DOS_MEM_PD_HEVC, 0xffffffffUL);
			/* disable hevc clock */
			hevc_clock_off();
			/* hevc power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0xc0);
		}
	}
	mutex_unlock(&vdec_mutex);
}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0xc) == 0) &&
			(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100))
			ret = true;
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x30) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100))
				ret = true;
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x3) == 0) &&
				(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0xc0) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	}

	return ret;
}

#elif 0				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
void vdec_poweron(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		vdec_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_2) {
		/* vdec2 soft reset */
		WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET2, 0);
		/* enable vdec2 clock */
		vdec2_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_HCODEC) {
		/* hcodec soft reset */
		WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET1, 0);
		/* enable hcodec clock */
		hcodec_clock_enable();
	}

	spin_unlock_irqrestore(&lock, flags);
}

void vdec_poweroff(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* disable vdec1 clock */
		vdec_clock_off();
	} else if (core == VDEC_2) {
		/* disable vdec2 clock */
		vdec2_clock_off();
	} else if (core == VDEC_HCODEC) {
		/* disable hcodec clock */
		hcodec_clock_off();
	}

	spin_unlock_irqrestore(&lock, flags);
}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_2) {
		if (READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_HCODEC) {
		if (READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000)
			ret = true;
	}

	return ret;
}
#endif

int vdec_source_changed(int format, int width, int height, int fps)
{
	/* todo: add level routines for clock adjustment per chips */
	int ret = -1;
	static int on_setting;
	if (on_setting > 0)
		return ret;/*on changing clk,ignore this change*/

	if (vdec_source_get(VDEC_1) == width * height * fps)
		return ret;


	on_setting = 1;
	ret = vdec_source_changed_for_clk_set(format, width, height, fps);
	pr_info("vdec1 video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_1));
	on_setting = 0;
	return ret;

}

int vdec2_source_changed(int format, int width, int height, int fps)
{
	int ret = -1;
	static int on_setting;

	if (has_vdec2()) {
		/* todo: add level routines for clock adjustment per chips */
		if (on_setting != 0)
			return ret;/*on changing clk,ignore this change*/

		if (vdec_source_get(VDEC_2) == width * height * fps)
			return ret;

		on_setting = 1;
		ret = vdec_source_changed_for_clk_set(format,
					width, height, fps);
		pr_info("vdec2 video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_2));
		on_setting = 0;
		return ret;
	}
	return 0;
}

int hevc_source_changed(int format, int width, int height, int fps)
{
	/* todo: add level routines for clock adjustment per chips */
	int ret = -1;
	static int on_setting;

	if (on_setting != 0)
			return ret;/*on changing clk,ignore this change*/

	if (vdec_source_get(VDEC_HEVC) == width * height * fps)
		return ret;

	on_setting = 1;
	ret = vdec_source_changed_for_clk_set(format, width, height, fps);
	pr_info("hevc video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_HEVC));
	on_setting = 0;

	return ret;
}

static enum vdec2_usage_e vdec2_usage = USAGE_NONE;
void set_vdec2_usage(enum vdec2_usage_e usage)
{
	if (has_vdec2()) {
		mutex_lock(&vdec_mutex);
		vdec2_usage = usage;
		mutex_unlock(&vdec_mutex);
	}
}

enum vdec2_usage_e get_vdec2_usage(void)
{
	if (has_vdec2())
		return vdec2_usage;
	else
		return 0;
}

static struct am_reg am_risc[] = {
	{"MSP", 0x300},
	{"MPSR", 0x301},
	{"MCPU_INT_BASE", 0x302},
	{"MCPU_INTR_GRP", 0x303},
	{"MCPU_INTR_MSK", 0x304},
	{"MCPU_INTR_REQ", 0x305},
	{"MPC-P", 0x306},
	{"MPC-D", 0x307},
	{"MPC_E", 0x308},
	{"MPC_W", 0x309},
	{"CSP", 0x320},
	{"CPSR", 0x321},
	{"CCPU_INT_BASE", 0x322},
	{"CCPU_INTR_GRP", 0x323},
	{"CCPU_INTR_MSK", 0x324},
	{"CCPU_INTR_REQ", 0x325},
	{"CPC-P", 0x326},
	{"CPC-D", 0x327},
	{"CPC_E", 0x328},
	{"CPC_W", 0x329},
	{"AV_SCRATCH_0", 0x09c0},
	{"AV_SCRATCH_1", 0x09c1},
	{"AV_SCRATCH_2", 0x09c2},
	{"AV_SCRATCH_3", 0x09c3},
	{"AV_SCRATCH_4", 0x09c4},
	{"AV_SCRATCH_5", 0x09c5},
	{"AV_SCRATCH_6", 0x09c6},
	{"AV_SCRATCH_7", 0x09c7},
	{"AV_SCRATCH_8", 0x09c8},
	{"AV_SCRATCH_9", 0x09c9},
	{"AV_SCRATCH_A", 0x09ca},
	{"AV_SCRATCH_B", 0x09cb},
	{"AV_SCRATCH_C", 0x09cc},
	{"AV_SCRATCH_D", 0x09cd},
	{"AV_SCRATCH_E", 0x09ce},
	{"AV_SCRATCH_F", 0x09cf},
	{"AV_SCRATCH_G", 0x09d0},
	{"AV_SCRATCH_H", 0x09d1},
	{"AV_SCRATCH_I", 0x09d2},
	{"AV_SCRATCH_J", 0x09d3},
	{"AV_SCRATCH_K", 0x09d4},
	{"AV_SCRATCH_L", 0x09d5},
	{"AV_SCRATCH_M", 0x09d6},
	{"AV_SCRATCH_N", 0x09d7},
};

static ssize_t amrisc_regs_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct am_reg *regs = am_risc;
	int rsize = sizeof(am_risc) / sizeof(struct am_reg);
	int i;
	unsigned val;
	ssize_t ret;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pbuf += sprintf(pbuf, "amrisc registers show:\n");
	for (i = 0; i < rsize; i++) {
		val = READ_VREG(regs[i].offset);
		pbuf += sprintf(pbuf, "%s(%#x)\t:%#x(%d)\n",
				regs[i].name, regs[i].offset, val, val);
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	ret = pbuf - buf;
	return ret;
}

static ssize_t dump_trace_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int i;
	char *pbuf = buf;
	ssize_t ret;
	u16 *trace_buf = kmalloc(debug_trace_num * 2, GFP_KERNEL);
	if (!trace_buf) {
		pbuf += sprintf(pbuf, "No Memory bug\n");
		ret = pbuf - buf;
		return ret;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			kfree(trace_buf);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pr_info("dump trace steps:%d start\n", debug_trace_num);
	i = 0;
	while (i <= debug_trace_num - 16) {
		trace_buf[i] = READ_VREG(MPC_E);
		trace_buf[i + 1] = READ_VREG(MPC_E);
		trace_buf[i + 2] = READ_VREG(MPC_E);
		trace_buf[i + 3] = READ_VREG(MPC_E);
		trace_buf[i + 4] = READ_VREG(MPC_E);
		trace_buf[i + 5] = READ_VREG(MPC_E);
		trace_buf[i + 6] = READ_VREG(MPC_E);
		trace_buf[i + 7] = READ_VREG(MPC_E);
		trace_buf[i + 8] = READ_VREG(MPC_E);
		trace_buf[i + 9] = READ_VREG(MPC_E);
		trace_buf[i + 10] = READ_VREG(MPC_E);
		trace_buf[i + 11] = READ_VREG(MPC_E);
		trace_buf[i + 12] = READ_VREG(MPC_E);
		trace_buf[i + 13] = READ_VREG(MPC_E);
		trace_buf[i + 14] = READ_VREG(MPC_E);
		trace_buf[i + 15] = READ_VREG(MPC_E);
		i += 16;
	};
	pr_info("dump trace steps:%d finished\n", debug_trace_num);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	for (i = 0; i < debug_trace_num; i++) {
		if (i % 4 == 0) {
			if (i % 16 == 0)
				pbuf += sprintf(pbuf, "\n");
			else if (i % 8 == 0)
				pbuf += sprintf(pbuf, "  ");
			else	/* 4 */
				pbuf += sprintf(pbuf, " ");
		}
		pbuf += sprintf(pbuf, "%04x:", trace_buf[i]);
	}
	while (i < debug_trace_num)
		;
	kfree(trace_buf);
	pbuf += sprintf(pbuf, "\n");
	ret = pbuf - buf;
	return ret;
}

static ssize_t clock_level_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	size_t ret;
	pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_1));

	if (has_vdec2())
		pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_2));

	if (has_hevc_vdec())
		pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_HEVC));

	ret = pbuf - buf;
	return ret;
}

static ssize_t store_poweron_clock_level(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned val;
	ssize_t ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	poweron_clock_level = val;
	return size;
}

static ssize_t show_poweron_clock_level(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", poweron_clock_level);
}

/*
if keep_vdec_mem == 1
always don't release
vdec 64 memory for fast play.
*/
static ssize_t store_keep_vdec_mem(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned val;
	ssize_t ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	keep_vdec_mem = val;
	return size;
}

static ssize_t show_keep_vdec_mem(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", keep_vdec_mem);
}


/*irq num as same as .dts*/
/*
	interrupts = <0 3 1
		0 23 1
		0 32 1
		0 43 1
		0 44 1
		0 45 1>;
	interrupt-names = "vsync",
		"demux",
		"parser",
		"mailbox_0",
		"mailbox_1",
		"mailbox_2";
*/
s32 vdec_request_irq(enum vdec_irq_num num, irq_handler_t handler,
				const char *devname, void *dev)
{
	s32 res_irq;
	s32 ret = 0;
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return -EINVAL;
	}
	res_irq = platform_get_irq(vdec_core_device, num);
	if (res_irq < 0) {
		pr_err("[%s] get irq error!", __func__);
		return -EINVAL;
	}
	vdec_irq[num] = res_irq;
	ret = request_irq(vdec_irq[num], handler,
	IRQF_SHARED, devname, dev);
	return ret;
}

void vdec_free_irq(enum vdec_irq_num num, void *dev)
{
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return;
	}
	free_irq(vdec_irq[num], dev);
}
static int dump_mode;
static ssize_t dump_risc_mem_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)/*set*/
{
	unsigned val;
	ssize_t ret;
	char dump_mode_str[4] = "PRL";
	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	dump_mode = val & 0x3;
	pr_info("set dump mode to %d,%c_mem\n",
		dump_mode, dump_mode_str[dump_mode]);
	return size;
}
static u32 read_amrisc_reg(int reg)
{
	WRITE_VREG(0x31b, reg);
	return READ_VREG(0x31c);
}

static void dump_pmem(void){
	int i;
	WRITE_VREG(0x301, 0x8000);
	WRITE_VREG(0x31d, 0);
	pr_info("start dump amrisc pmem of risc\n");
	for (i = 0; i < 0xfff; i++) {
		/*same as .o format*/
		pr_info("%08x // 0x%04x:\n", read_amrisc_reg(i), i);
	}
	return;
}


static void dump_lmem(void){
	int i;
	WRITE_VREG(0x301, 0x8000);
	WRITE_VREG(0x31d, 2);
	pr_info("start dump amrisc lmem\n");
	for (i = 0; i < 0x3ff; i++) {
		/*same as */
		pr_info("[%04x] = 0x%08x:\n", i, read_amrisc_reg(i));
	}
	return;
}

static ssize_t dump_risc_mem_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	int ret;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	/*start do**/
	switch (dump_mode) {
	case 0:
		dump_pmem();
		break;
	case 2:
		dump_lmem();
		break;
	default:
		break;
	}

	/*done*/
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	return sprintf(buf, "done\n");
}

static struct class_attribute vdec_class_attrs[] = {
	__ATTR_RO(amrisc_regs),
	__ATTR_RO(dump_trace),
	__ATTR_RO(clock_level),
	__ATTR(poweron_clock_level, S_IRUGO | S_IWUSR | S_IWGRP,
	show_poweron_clock_level, store_poweron_clock_level),
	__ATTR(dump_risc_mem, S_IRUGO | S_IWUSR | S_IWGRP,
	dump_risc_mem_show, dump_risc_mem_store),
	__ATTR(keep_vdec_mem, S_IRUGO | S_IWUSR | S_IWGRP,
	show_keep_vdec_mem, store_keep_vdec_mem),
	__ATTR_NULL
};

static struct class vdec_class = {
		.name = "vdec",
		.class_attrs = vdec_class_attrs,
	};


/*
pre alloced enough memory for decoder
fast start.
*/
void pre_alloc_vdec_memory(void)
{
	if (!keep_vdec_mem || vdec_dev_reg.mem_start)
		return;

	vdec_dev_reg.mem_start = codec_mm_alloc_for_dma(MEM_NAME,
		CMA_ALLOC_SIZE / PAGE_SIZE, 4 + PAGE_SHIFT,
		CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_FOR_VDECODER);
	if (!vdec_dev_reg.mem_start)
		return;
	pr_debug("vdec base memory alloced %p\n",
	(void *)vdec_dev_reg.mem_start);

	vdec_dev_reg.mem_end = vdec_dev_reg.mem_start +
		CMA_ALLOC_SIZE - 1;
	vdec_mem_alloced_from_codec = 1;
	delay_release = 3;
}

static int vdec_probe(struct platform_device *pdev)
{
	s32 r;

	r = class_register(&vdec_class);
	if (r) {
		pr_info("vdec class create fail.\n");
		return r;
	}

	vdec_core_device = pdev;

	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_info("vdec_probe done\n");

	vdec_dev_reg.cma_dev = &pdev->dev;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8) {
		/* default to 250MHz */
		vdec_clock_hi_enable();
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
		/* set vdec dmc request to urgent */
		WRITE_DMCREG(DMC_AM5_CHAN_CTRL, 0x3f203cf);
	}
	if (codec_mm_get_reserved_size() >= 48 * SZ_1M
		&& codec_mm_get_reserved_size() <=  96 * SZ_1M) {
		vdec_default_buf_size[VFORMAT_H264_4K2K] =
			codec_mm_get_reserved_size() / SZ_1M;
		/*all reserved size for prealloc*/
	}
	pre_alloc_vdec_memory();
	return 0;
}

static int vdec_remove(struct platform_device *pdev)
{
	class_unregister(&vdec_class);

	return 0;
}

static const struct of_device_id amlogic_vdec_dt_match[] = {
	{
		.compatible = "amlogic, vdec",
	},
	{},
};

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		.name = "vdec",
		.of_match_table = amlogic_vdec_dt_match,
	}
};

static int __init vdec_module_init(void)
{
	if (platform_driver_register(&vdec_driver)) {
		pr_info("failed to register vdec module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit vdec_module_exit(void)
{
	platform_driver_unregister(&vdec_driver);
	return;
}

static int vdec_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned long start, end;
	start = rmem->base;
	end = rmem->base + rmem->size - 1;
	pr_info("init vdec memsource %lx->%lx\n", start, end);

	vdec_set_resource(start, end, dev);
	return 0;
}

static const struct reserved_mem_ops rmem_vdec_ops = {
	.device_init = vdec_mem_device_init,
};

static int __init vdec_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vdec_ops;
	pr_info("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(vdec, "amlogic, vdec-memory", vdec_mem_setup);

module_param(debug_trace_num, uint, 0664);
module_param(hevc_max_reset_count, int, 0664);
module_param(clk_config, uint, 0664);

module_init(vdec_module_init);
module_exit(vdec_module_exit);

MODULE_DESCRIPTION("AMLOGIC vdec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
