// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include "amdv.h"

#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

static unsigned int dv_ll_output_mode = AMDV_OUTPUT_MODE_HDR10;
static bool stb_core2_const_flag;

static unsigned int htotal_add = 0x140;
static unsigned int vtotal_add = 0x40;
static unsigned int vsize_add;
static unsigned int vwidth = 0x8;
static unsigned int hwidth = 0x8;
static unsigned int vpotch = 0x10;
static unsigned int hpotch = 0x8;
static unsigned int g_htotal_add = 0x40;
static unsigned int g_vtotal_add = 0x80;
static unsigned int g_vsize_add;
static unsigned int g_vwidth = 0x18;
static unsigned int g_hwidth = 0x10;
static unsigned int g_vpotch = 0x10;
static unsigned int g_hpotch = 0x10;
/*core reg must be set at first time. bit0 is for core2, bit1 is for core3*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static u32 first_reseted;
#endif
/* for core2 timing setup tuning */
/* g_vtotal_add << 24 | g_vsize_add << 16 */
/* | g_vwidth << 8 | g_vpotch */
static unsigned int g_vtiming;
static unsigned int dma_start_line = 0x400;
int debug_dma_start_line;
bool disable_aoi;
int debug_disable_aoi;

#define MAX_CORE3_MD_SIZE 128 /*512byte*/

static bool bypass_all_vpp_pq;
/*0: not debug mode; 1:force bypass vpp pq; 2:force enable vpp pq*/
static u32 debug_bypass_vpp_pq;

static bool force_reset_core2;/*reset total core2*/
static bool update_core2_reg;/*only set core2 reg*/

/*bit0:reset core1a reg; bit1:reset core2 reg;bit2:reset core3 reg*/
/*bit3: reset core1a lut; bit4: reset core2 lut*/
/*bit5: reset core1b reg; bit6: reset core1b lut*/
static unsigned int force_update_reg;

#define CP_FLAG_CHANGE_CFG		0x000001
#define CP_FLAG_CHANGE_MDS		0x000002
#define CP_FLAG_CHANGE_MDS_CFG		0x000004
#define CP_FLAG_CHANGE_GD		0x000010
#define CP_FLAG_CHANGE_GD_OLD		0x000008
#define CP_FLAG_CHANGE_AB		0x000020
#define CP_FLAG_CHANGE_TC		0x000100
#define CP_FLAG_CHANGE_TC_OLD		0x000010
#define CP_FLAG_CHANGE_TC2		0x000200
#define CP_FLAG_CHANGE_TC2_OLD		0x000020
#define CP_FLAG_CHANGE_L2NL		0x000400
#define CP_FLAG_CHANGE_L2NL_OLD		0x000040
#define CP_FLAG_CHANGE_3DLUT		0x000800
#define CP_FLAG_CHANGE_3DLUT_OLD	0x000080
#define CP_FLAG_CONST_TC		0x100000
#define CP_FLAG_CONST_TC2		0x200000
#define CP_FLAG_CHANGE_ALL		0xffffffff

/* update all core */
static u32 stb_core_setting_update_flag = CP_FLAG_CHANGE_ALL;
static unsigned int bypass_core1a_composer;
static unsigned int bypass_core1b_composer;
static int operate_mode;
bool force_bypass_from_prebld_to_vadj1;/* t3/t5w, 1d93 bit0 -> 1d26 bit8*/

bool get_force_bypass_from_prebld_to_vadj1(void)
{
	return force_bypass_from_prebld_to_vadj1;
}
EXPORT_SYMBOL(get_force_bypass_from_prebld_to_vadj1);

void adjust_vpotch(u32 graphics_w, u32 graphics_h)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	int sync_duration_num = 60;

	if (is_aml_txlx_stbmode()) {
		if (vinfo && vinfo->width >= 1920 &&
			vinfo->height >= 1080 &&
			vinfo->field_height >= 1080)
			dma_start_line = 0x400;
		else
			dma_start_line = 0x180;
		/* adjust core2 setting to work around*/
		/* fixing with 1080p24hz and 480p60hz */
		if (vinfo && vinfo->width < 1280 &&
			vinfo->height < 720 &&
			vinfo->field_height < 720)
			g_vpotch = 0x60;
		else
			g_vpotch = 0x20;
	} else if (is_aml_g12()) {
		if (vinfo) {
			if (vinfo->sync_duration_den)
				sync_duration_num = vinfo->sync_duration_num /
						    vinfo->sync_duration_den;
			if (debug_dolby & 2)
				pr_dv_dbg("vinfo %d %d %d %d %d %d\n",
					     vinfo->width,
					     vinfo->height,
					     vinfo->field_height,
					     vinfo->sync_duration_num,
					     vinfo->sync_duration_den,
					     sync_duration_num);
			if (vinfo->width < 1280 &&
				vinfo->height < 720 &&
				vinfo->field_height < 720)
				g_vpotch = 0x60;
			else if (vinfo->width == 1280 &&
				 vinfo->height == 720)
				g_vpotch = 0x38;
			else if (vinfo->width == 1280 &&
				 vinfo->height == 720 &&
				 vinfo->field_height < 720)
				g_vpotch = 0x60;
			else if (vinfo->width == 1920 &&
				 vinfo->height == 1080 &&
				 sync_duration_num < 30)
				g_vpotch = 0x60;
			else if (vinfo->width == 1920 &&
				 vinfo->height == 1080 &&
				 vinfo->field_height < 1080)
				g_vpotch = 0x60;
			else if (graphics_h > 1440)
				g_vpotch = 0x10;
			else
				g_vpotch = 0x20;
			if (vinfo->width > 1920)
				htotal_add = 0xc0;
			else
				htotal_add = 0x140;
		} else {
			g_vpotch = 0x20;
		}
	} else if (is_aml_tm2_stbmode() || is_aml_t7_stbmode() ||
		is_aml_sc2() || is_aml_s4d()) {
		if (vinfo) {
			if (debug_dolby & 2)
				pr_dv_dbg("vinfo %d %d %d, graphics_h %d\n",
					vinfo->width,
					vinfo->height,
					vinfo->field_height,
					graphics_h);
			if (vinfo->width < 1280 &&
				vinfo->height < 720 &&
				vinfo->field_height < 720)
				g_vpotch = 0x60;
			else if (vinfo->width <= 1920 &&
				vinfo->height <= 1080 &&
				vinfo->field_height <= 1080)
				g_vpotch = 0x50;
			else
				g_vpotch = 0x20;

			/* for 4k fb */
			if (graphics_h > 1440)
				g_vpotch = 0x10;

			if (vinfo->width > 1920)
				htotal_add = 0xc0;
			else
				htotal_add = 0x140;
		} else {
			g_vpotch = 0x20;
		}
	}
}

void adjust_vpotch_tv(void)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (is_aml_tm2() || is_aml_t7() ||
	    is_aml_t3() || is_aml_t5w()) {
		if (debug_dma_start_line) {
			dma_start_line = debug_dma_start_line;
		} else if (vinfo) {
			if (vinfo && vinfo->width >= 1920 &&
				vinfo->height >= 1080 &&
				vinfo->field_height >= 1080)
				dma_start_line = 0x400;
			else
				dma_start_line = 0x180;
		}
	}
}

static void amdv_core_reset(enum core_type type)
{
	switch (type) {
	case AMDV_TVCORE:
		if (is_aml_txlx())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 9);
		else if (is_aml_tm2() || is_aml_t7() ||
			 is_aml_t3() || is_aml_t5w())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 1);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		break;
	case AMDV_CORE1A:
		if (is_aml_txlx())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 10);
		else if (is_aml_g12())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 1);
		else if (is_aml_tm2() || is_aml_sc2() ||
			 is_aml_s4d() || is_aml_t7() ||
			 is_aml_t3())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 30);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		break;
	case AMDV_CORE1B:
		if (is_aml_txlx())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 11);
		else if (is_aml_g12())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 3);
		else if (is_aml_tm2() || is_aml_sc2() ||
			 is_aml_s4d() || is_aml_t7() ||
			 is_aml_t3())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 31);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		break;
	case AMDV_CORE1C:
		if (is_aml_t7() || is_aml_t3()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 1 << 2);
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 0);
		}
		break;
	case AMDV_CORE2A:
		if (is_aml_tm2() || is_aml_sc2() ||
		    is_aml_s4d() || is_aml_t7() ||
		    is_aml_t3() || is_aml_g12()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 2);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		}
		break;
	case AMDV_CORE2B:
		if (is_aml_tm2() || is_aml_sc2() ||
		    is_aml_s4d() || is_aml_t7() ||
		    is_aml_t3() || is_aml_g12()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 3);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		}
		break;
	case AMDV_CORE2C:
		if (is_aml_t7() || is_aml_t3()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 1 << 0);
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 0);
		}
		break;
	default:
		pr_debug("error core type %d\n", type);
	return;
	}
}

static u32 tv_run_mode(int vsize, bool hdmi, bool hdr10, int el_41_mode)
{
	u32 run_mode = 1;

	if (hdmi) {
		if (vsize > 1080)
			run_mode =
				0x00000043;
		else
			run_mode =
				0x00000042;
	} else {
		if (hdr10) {
			run_mode =
				0x0000004c;
		} else {
			if (el_41_mode)
				run_mode =
					0x0000004c;
			else
				run_mode =
					0x00000044;
		}
	}
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		run_mode |= 1 << 12; /* bypass CSC */
	if ((dolby_vision_flags & FLAG_BYPASS_CVM) &&
	    !(dolby_vision_flags & FLAG_FORCE_CVM))
		run_mode |= 1 << 13; /* bypass CVM */
	return run_mode;
}

int tv_dv_core1_set(u64 *dma_data,
			     dma_addr_t dma_paddr,
			     int hsize,
			     int vsize,
			     int bl_enable,
			     int el_enable,
			     int el_41_mode,
			     int src_chroma_format,
			     bool hdmi,
			     bool hdr10,
			     bool reset)
{
	u64 run_mode;
	int composer_enable = el_enable;
	bool bypass_core1 = (!hsize || !vsize || !(amdv_mask & 1));
	static int start_render;
	bool core1_on_flag = amdv_core1_on;
	int runmode_cnt = amdv_on_count;

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	/*for stb hdmi in mode*/
	if (multi_dv_mode && (is_aml_tm2_stbmode() || is_aml_t7_stbmode())) {
		core1_on_flag = dv_core1[0].core1_on || dv_core1[1].core1_on;
		runmode_cnt = dv_core1[hdmi_path_id].run_mode_count;
	}

	if (is_aml_t3() || is_aml_t5w()) {
		VSYNC_WR_DV_REG_BITS(VPP_TOP_VTRL, 0, 0, 1); //AMDV TV select
		//T3 enable tvcore clk
		if (!dolby_vision_on) {/*enable once*/
			vpu_module_clk_enable(0, DV_TVCORE, 1);
			vpu_module_clk_enable(0, DV_TVCORE, 0);
		}
	}

	adjust_vpotch_tv();
	if (is_aml_tm2() || is_aml_t7() ||
	    is_aml_t3() || is_aml_t5w()) {
		/* mempd for ipcore */
		if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
			if (get_dv_vpu_mem_power_status(VPU_DOLBY0) ==
			    VPU_MEM_POWER_ON)
				dv_mem_power_off(VPU_DOLBY0);
			VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL7, 0xf, 4, 4);

		} else {
			if (get_dv_vpu_mem_power_status(VPU_DOLBY0) ==
				VPU_MEM_POWER_DOWN ||
				get_dv_mem_power_flag(VPU_DOLBY0) ==
				VPU_MEM_POWER_DOWN)
				dv_mem_power_on(VPU_DOLBY0);
			VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL7, 0, 4, 9);
			if (is_aml_tm2revb() || is_aml_t7() ||
			    is_aml_t3() || is_aml_t5w()) {
				/* comp on, mempd on */
				VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL7,
						     0, 14, 4);
			}
		}
	}
	WRITE_VPP_DV_REG(AMDV_TV_CLKGATE_CTRL, 0x2800);
	if (reset) {
		amdv_core_reset(AMDV_TVCORE);
		VSYNC_WR_DV_REG
			(AMDV_TV_CLKGATE_CTRL, 0x2800);
	}

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL0,
		/* el dly 3, bl dly 1 after de*/
		(el_41_mode ? (0x3 << 4) : (0x1 << 8)) |
		bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL1,
		((hsize + 0x80) << 16 | (vsize + 0x40)));
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL2, (hsize << 16) | vsize);
	/*0x2c2d0:5-4-1-3-2-0*/
	VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL5, 0x2c2d0, 14, 18);
	VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL5, 0xa, 0, 4);

	if (hdmi && !hdr10) {
		/*hdmi DV STD and DV LL:  need detunnel*/
		VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL5, 1, 4, 1);
	} else {
		VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL5, 0, 4, 1);
	}

	/*set diag reg to 0xb can bypass dither, not need set swap ctrl6 */
	if (!is_aml_tm2() && !is_aml_t7() &&
	    !is_aml_t3() &&
	    !is_aml_t5w()) {
		VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL6, 1, 20, 1);
		/* bypass dither */
		VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL6, 1, 25, 1);
	}
	if (src_chroma_format == 2)
		VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL6, 1, 29, 1);
	else if (src_chroma_format == 1)
		VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL6, 0, 29, 1);
	/* input 12 or 10 bit */
	VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL7, 12, 0, 4);

	if (el_enable && (amdv_mask & 1))
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			/* vd2 to core1 */
			 0, 17, 1);
	else
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			/* vd2 to vpp */
			 1, 17, 1);

	if (core1_on_flag &&
	    !bypass_core1) {
		if (is_aml_tm2_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 1, 8, 2);
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 1, 10, 2);
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 1, 24, 2);
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 0, 16, 1);
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 0, 20, 1);
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 el_enable ? 0 : 2, 0, 2);
		} else if (is_aml_t7_tvmode() ||
			is_aml_t3_tvmode() || is_aml_t5w()) {
			/*enable tv core*/
			if (is_aml_t7_tvmode())
				VSYNC_WR_DV_REG_BITS
					(VPP_VD1_DSC_CTRL,
					 0, 4, 1);
			else
				VSYNC_WR_DV_REG_BITS
					(VIU_VD1_PATH_CTRL,
					 0, 16, 1);

			/*vd1 to tvcore*/
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_SWAP_CTRL1,
				 1, 0, 3);
			/*tvcore bl in sel vd1*/
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_SWAP_CTRL2,
				 0, 6, 2);
			/*tvcore el in sel null*/
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_SWAP_CTRL2,
				 3, 8, 2);
			/*tvcore bl to vd1*/
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_SWAP_CTRL2,
				 0, 20, 2);
			/* vd1 from tvcore*/
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_SWAP_CTRL1,
				 1, 12, 3);
		}  else {
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				/* enable core 1 */
				 0, 16, 1);
		}
	} else if (core1_on_flag &&
	    bypass_core1) {
		if (is_aml_tm2_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 3, 0, 2);
		} else if (is_aml_t7_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(VPP_VD1_DSC_CTRL,
				 1, 4, 1);
		} else if (is_aml_t3_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(VIU_VD1_PATH_CTRL,
				 1, 16, 1);
		} else if (is_aml_t5w()) {
			VSYNC_WR_DV_REG_BITS
				(VIU_VD1_PATH_CTRL,
				 1, 16, 1);
		} else {
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				/* bypass core 1 */
				 1, 16, 1);
		}
	}

	if (amdv_run_mode != 0xff) {
		run_mode = amdv_run_mode;
	} else {
		if (debug_dolby & 8)
			pr_dv_dbg("%s: amdv_on_count %d\n",
				     __func__, amdv_on_count);
		run_mode = tv_run_mode(vsize, hdmi, hdr10, el_41_mode);
		if (runmode_cnt < amdv_run_mode_delay) {
			run_mode = (run_mode & 0xfffffffc) | 1;
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
			start_render = 0;
		} else if (runmode_cnt ==
			amdv_run_mode_delay) {
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
			start_render = 0;
		} else {
			if (start_render == 0) {
				VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
				VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
					0);
			}
			start_render = 1;
		}
	}
	tv_dovi_setting->core1_reg_lut[1] =
		0x0000000100000000 | run_mode;
	if (debug_disable_aoi) {
		if (debug_disable_aoi == 1)
			tv_dovi_setting->core1_reg_lut[44] =
			0x0000002e00000000;
	} else if (disable_aoi) {
		tv_dovi_setting->core1_reg_lut[44] =
		0x0000002e00000000;
	}
	if (reset)
		VSYNC_WR_DV_REG(AMDV_TV_REG_START + 1, run_mode);
	if ((is_aml_tm2_stbmode() || is_aml_t7_stbmode()) && !core1_detunnel())
		VSYNC_WR_DV_REG(AMDV_TV_REG_START + 0xe7, 1);/*diag bypass*/
	else
		VSYNC_WR_DV_REG(AMDV_TV_REG_START + 0xe7, 0);

	if (!dolby_vision_on ||
	(!core1_on_flag &&
	(is_aml_tm2_stbmode() || is_aml_t7_stbmode()))) {
		WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL1, 0x6f666080);
		if (is_aml_t7() || is_aml_t3() || is_aml_t5w())
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL2, (u32)(dma_paddr >> 4));
		else
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL2, (u32)(dma_paddr));

		if (is_aml_t3() || is_aml_t5w())
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL3,
				 0x88000000 | dma_start_line);
		else
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL3,
				 0x80000000 | dma_start_line);
		if (is_aml_t7()) {
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x01000040);
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x80400040);
		} else {
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x01000042);
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x80400042);
		}
	}
	if (reset) {
		if (is_aml_t7() || is_aml_t3() || is_aml_t5w())
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL2, (u32)(dma_paddr >> 4));
		else
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL2, (u32)(dma_paddr));
		if (is_aml_t3() || is_aml_t5w())
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL3,
				0x88000000 | dma_start_line);
		else
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL3,
				0x80000000 | dma_start_line);
		if (is_aml_t7()) {
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x01000040);
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x80400040);
		} else {
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x01000042);
			VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x80400042);
		}
	}
	set_dovi_setting_update_flag(true);
	return 0;
}

static int stb_dv_core1_set(dma_addr_t dma_paddr,
				      u32 dm_count,
				      u32 comp_count,
				      u32 lut_count,
				      u32 *p_core1_dm_regs,
				      u32 *p_core1_comp_regs,
				      u32 *p_core1_lut,
				      int hsize,
				      int vsize,
				      int bl_enable,
				      int el_enable,
				      int el_41_mode,
				      int scramble_en,
				      bool amdv_src,
				      int lut_endian,
				      bool reset)
{
	u32 bypass_flag = 0;
	int composer_enable = el_enable;
	u32 run_mode = 0;
	int reg_size = 0;
	bool bypass_core1 = (!hsize || !vsize ||
			    !(amdv_mask & 1));

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	WRITE_VPP_DV_REG(AMDV_TV_CLKGATE_CTRL, 0x2800);
	if (reset) {
		if (!amdv_core1_on) {
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 9);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
			VSYNC_WR_DV_REG(AMDV_TV_CLKGATE_CTRL, 0x2800);
		} else {
			reset = 0;
		}
	}

	if (!bl_enable)
		VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL5, 0x446);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL0, 0);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL3,
		(hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL4,
		(hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL2,
		(hsize << 16) | vsize);
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL6, 0xba000000);
	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;
	VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL0,
		/* el dly 3, bl dly 1 after de*/
		(el_41_mode ? (0x3 << 4) : (0x1 << 8)) |
		bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);

	if (el_enable && (amdv_mask & 1))
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			/* vd2 to core1 */
			 0, 17, 1);
	else
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			/* vd2 to vpp */
			 1, 17, 1);
	if (amdv_core1_on && !bypass_core1)
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			/* enable core 1 */
			 0, 16, 1);
	else if (amdv_core1_on && bypass_core1)
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			/* bypass core 1 */
			 1, 16, 1);
	/* run mode = bypass, when fake frame */
	if (!bl_enable)
		bypass_flag |= 1;
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 12; /* bypass CSC */
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 13; /* bypass CVM */
	if (need_skip_cvm(0))
		bypass_flag |= 1 << 13; /* bypass CVM when tunnel out */

	if (amdv_run_mode != 0xff) {
		run_mode = amdv_run_mode;
	} else {
		run_mode = (0x7 << 6) |
			((el_41_mode ? 3 : 1) << 2) |
			bypass_flag;
		if (amdv_on_count < amdv_run_mode_delay) {
			run_mode |= 1;
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
		} else if (amdv_on_count ==
			amdv_run_mode_delay) {
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
				(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
				(0x200 << 10) | 0x200);
		} else {
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
				(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
				0);
		}
	}
	if (reset)
		VSYNC_WR_DV_REG(AMDV_TV_REG_START + 1, run_mode);

	/* 962e work around to fix the uv swap issue when bl:el = 1:1 */
	if (el_41_mode)
		VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL5, 0x6);
	else
		VSYNC_WR_DV_REG(AMDV_TV_SWAP_CTRL5, 0xa);

	/* axi dma for reg table */
	reg_size = prepare_stb_dvcore1_reg
		(run_mode, p_core1_dm_regs, p_core1_comp_regs);
	/* axi dma for lut table */
	prepare_stb_dvcore1_lut(reg_size, p_core1_lut);

	if (!dolby_vision_on) {
		/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
		WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL1,
			0x00000080 | (reg_size << 23));
		WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		/* dma3:23-12 cvm size */
		WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL3,
			0x80100000 | dma_start_line);
		WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x01000062);
		WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x80400042);
	}
	if (reset) {
		/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
		VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL1,
			0x00000080 | (reg_size << 23));
		VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		/* dma3:23-12 cvm size */
		VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL3,
			0x80100000 | dma_start_line);
		VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x01000062);
		VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0, 0x80400042);
	}

	set_dovi_setting_update_flag(true);
	return 0;
}

static int dv_core1_set(u32 dm_count,
				u32 comp_count,
				u32 lut_count,
				u32 *p_core1_dm_regs,
				u32 *p_core1_comp_regs,
				u32 *p_core1_lut,
				int hsize,
				int vsize,
				int bl_enable,
				int el_enable,
				int el_41_mode,
				int scramble_en,
				bool amdv_src,
				int lut_endian,
				bool reset)
{
	u32 count;
	u32 bypass_flag = 0;
	int composer_enable =
		bl_enable && el_enable && (amdv_mask & 1);
	int i;
	bool set_lut = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg1;
	u32 *last_comp = (u32 *)&dovi_setting.comp_reg;
	bool bypass_core1 = (!hsize || !vsize ||
			     !(amdv_mask & 1));
	int copy_core1a_to_core1b = ((copy_core1a & 1) &&
				(is_aml_tm2_stbmode() || is_aml_t7_stbmode()));
	int copy_core1a_to_core1c = ((copy_core1a & 2) && is_aml_t7_stbmode());

	/* G12A: make sure the BL is enable for the very 1st frame*/
	/* Register: dolby_path_ctrl[0] = 0 to enable BL*/
	/*           dolby_path_ctrl[1] = 0 to enable EL*/
	/*           dolby_path_ctrl[2] = 0 to enable OSD*/
	if (is_amdv_stb_mode() &&
		get_frame_count() == 1 && amdv_core1_on == 0) {
		pr_dv_dbg("((%s %d, register AMDV_PATH_CTRL: %x))\n",
			__func__, __LINE__,
			VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
		if ((VSYNC_RD_DV_REG(AMDV_PATH_CTRL) & 0x1) != 0) {
			pr_dv_dbg("BL is disable for 1st frame.Re-enable BL\n");
			VSYNC_WR_DV_REG_BITS(AMDV_PATH_CTRL, 0, 0, 1);
			pr_dv_dbg("((%s %d, enable_bl, AMDV_PATH_CTRL: %x))\n",
				__func__, __LINE__,
				VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
		}
		if (el_enable) {
			if ((VSYNC_RD_DV_REG(AMDV_PATH_CTRL) & 0x10) != 0) {
				pr_dv_dbg("((%s %d enable el))\n",
					__func__, __LINE__);
				VSYNC_WR_DV_REG_BITS(AMDV_PATH_CTRL,
					0, 1, 1);
				pr_dv_dbg("((%s %d, enable_el, AMDV_PATH_CTRL: %x))\n",
					__func__, __LINE__,
					VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
			}
		}
	}

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;

	if (force_update_reg & 8)
		set_lut = true;

	if (force_update_reg & 1)
		reset = true;

	if (amdv_on_count
		== amdv_run_mode_delay)
		reset = true;

	if ((!dolby_vision_on || reset) && bl_enable) {
		amdv_core_reset(AMDV_CORE1A);
		if (copy_core1a_to_core1b)
			amdv_core_reset(AMDV_CORE1B);
		if (copy_core1a_to_core1c)
			amdv_core_reset(AMDV_CORE1C);
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (bl_enable && amdv_core1_on_cnt < DV_CORE1_RECONFIG_CNT) {
		reset = true;
		amdv_core1_on_cnt++;
	}

	//if (reset)
		//update_core2_reg = true;

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC_OLD)
		set_lut = true;

	if ((bl_enable && el_enable && (amdv_mask & 1)) ||
	    (copy_core1a_to_core1b || copy_core1a_to_core1c)) {
		if (is_aml_g12() || is_aml_tm2_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 /* vd2 to core1a */
				 0, 1, 1);
		} else if (is_aml_t7_stbmode()) {
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG_BITS
					(VPP_VD2_DSC_CTRL,
					 /* vd2 to core1b */
					 0, 4, 1);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG_BITS
					(VPP_VD3_DSC_CTRL,
					 /* vd3 to core1c */
					 0, 4, 1);
		} else {
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				 /* vd2 to core1 */
				 0, 17, 1);
		}
	} else {
		if (is_aml_g12() || is_aml_sc2() ||
			is_aml_tm2_stbmode() || is_aml_s4d()) {
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				/* vd2 to vpp */
				1, 1, 1);
		} else if (is_aml_t7_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(VPP_VD2_DSC_CTRL,
				 /* vd2 bypass dv */
				 1, 4, 1);
			VSYNC_WR_DV_REG_BITS
				(VPP_VD3_DSC_CTRL,
				 /* vd3 bypass dv */
				 1, 4, 1);
		} else {
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				 /* vd2 to vpp */
				 1, 17, 1);
		}
	}

	if (is_amdv_stb_mode() && bl_enable) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY1A) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1A) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1A);
		if (get_dv_vpu_mem_power_status(VPU_PRIME_DOLBY_RAM) ==
			VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_PRIME_DOLBY_RAM) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
	}
	VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL, 0);
	/* VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL0, 0); */
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL2, (hsize << 16) | vsize);
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL5, 0xa);

	VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 4, 4);
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 2, 1);

	if (copy_core1a_to_core1b) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY1B) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1B) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1B);
		VSYNC_WR_DV_REG(AMDV_CORE1B_CLKGATE_CTRL, 0);
		/* VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL0, 0); */
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL1,
			((hsize + 0x80) << 16) | (vsize + 0x40));
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL3,
			(hwidth << 16) | vwidth);
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL4,
			(hpotch << 16) | vpotch);
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL2,
			(hsize << 16) | vsize);
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL5, 0xa);

		VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_CTRL, 0x0);
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 4, 4);
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 2, 1);
	}

	if (copy_core1a_to_core1c) {
		VSYNC_WR_DV_REG(AMDV_CORE1C_CLKGATE_CTRL, 0);
		/* VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL0, 0); */
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL1,
				((hsize + 0x80) << 16) | (vsize + 0x40));
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL3,
				(hwidth << 16) | vwidth);
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL4,
				(hpotch << 16) | vpotch);
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL2,
				(hsize << 16) | vsize);
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL5, 0xa);

		VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_CTRL, 0x0);
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 4, 4);
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 2, 1);
	}

	/*For HDMI input and OTT HDR10/HLG/SDR8/SDR10 inputs we bypass the composer*/
	if ((is_aml_stb_hdmimode() || !amdv_src) || bypass_core1a_composer)
		bypass_flag |= 1;
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (need_skip_cvm(0))
		bypass_flag |= 1 << 2;
	if (el_41_mode)
		bypass_flag |= 1 << 3;

	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 1,
		0x70 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 1,
		0x70 | bypass_flag); /* for delay */
	if (copy_core1a_to_core1b) {
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 1,
			0x70 | bypass_flag); /* bypass CVM and/or CSC */
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 1,
			0x70 | bypass_flag); /* for delay */
	}
	if (copy_core1a_to_core1c) {
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 1,
				0x70 | bypass_flag); /* bypass CVM and/or CSC */
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 1,
				0x70 | bypass_flag); /* for delay */
	}
	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1A_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG
				(AMDV_CORE1B_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG
				(AMDV_CORE1C_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
		}

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_comp_regs[i] != last_comp[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1A_REG_START + 6 + 44 + i,
				 p_core1_comp_regs[i]);
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG
					(AMDV_CORE1B_REG_START + 6 + 44 + i,
					 p_core1_comp_regs[i]);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG
					(AMDV_CORE1C_REG_START + 6 + 44 + i,
					 p_core1_comp_regs[i]);
		}
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 3, 1);
	if (copy_core1a_to_core1b)
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 3, 1);
	if (copy_core1a_to_core1c)
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_aml_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(AMDV_CORE1A_CLKGATE_CTRL,
					     2, 2, 2);
#endif
		VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_CTRL, 0x1401);
		if (copy_core1a_to_core1b)
			VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_CTRL, 0x1401);
		if (copy_core1a_to_core1c)
			VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_CTRL, 0x1401);
		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i + 3]);
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i + 2]);
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i + 1]);
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i]);
				if (copy_core1a_to_core1b) {
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i + 3]);
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i + 2]);
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i + 1]);
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i]);
				}
				if (copy_core1a_to_core1c) {
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i + 3]);
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i + 2]);
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i + 1]);
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i]);
				}
			}
		} else {
			for (i = 0; i < count; i++)
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i]);
			if (copy_core1a_to_core1b) {
				for (i = 0; i < count; i++)
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i]);
			}
			if (copy_core1a_to_core1c) {
				for (i = 0; i < count; i++)
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i]);
			}
		}
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_aml_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(AMDV_CORE1A_CLKGATE_CTRL,
					     0, 2, 2);
#endif
	}

	if (amdv_on_count
		< amdv_run_mode_delay) {
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC0,
			 (0x200 << 10) | 0x200);
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC1,
			 (0x200 << 10) | 0x200);
		if (is_aml_g12())
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 1,
				 0, 1);
		else
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				 1, 16, 1);
	} else {
		if (amdv_on_count >
			amdv_run_mode_delay) {
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC0,
				 (0x3ff << 20) |
				 (0x3ff << 10) |
				 0x3ff);
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC1,
				 0);
		}
		if (amdv_core1_on && !bypass_core1) {
			if (is_aml_g12()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 0, 1);
			} else if (is_aml_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 8, 2);
				if (copy_core1a_to_core1b)
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 10, 2);
				else
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 24, 2);
				if (copy_core1a_to_core1b) {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 19, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 23, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 26, 2);
				}
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					0, 0, 1); /* core1 */
			} else if (is_aml_t7_stbmode()) {
				/* vd1 to core1a*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 0, 0, 3);
				/*core1a bl in sel vd1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 0, 2, 2);
				/*core1a el in sel null*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 3, 4, 2);
				/*core1a out to vd1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 0, 18, 2);
				/* vd1 from core1a*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 0, 12, 3);
				/*enable core1a*/
				VSYNC_WR_DV_REG_BITS
					(VPP_VD1_DSC_CTRL,
					 0, 4, 1);
				if (copy_core1a_to_core1b) {
					/*vd2 to core1b*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 3, 4, 3);
					/*core1b in sel vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 1, 10, 2);
					/*core1b out to vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 1, 22, 2);
					/* vd2 from core1b*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 3, 16, 3);
					/*enable core1b*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 0, 4, 1);
				}

				if (copy_core1a_to_core1c) {
					/*core1c to vd3*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 4, 8, 3);
					/*core1c in sel vd3*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 2, 12, 2);
					/*core1c out to vd3*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 2, 24, 2);
					/*vd3 from core1c*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 4, 20, 3);

					/*enable core1c*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 0, 4, 1);
				}
			} else if (is_aml_sc2() || is_aml_s4d()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 8, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 24, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 0, 1); /* core1 */
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* enable core 1 */
					 0, 16, 1);
			}
		} else if (amdv_core1_on &&
			bypass_core1) {
			if (is_aml_g12() || is_aml_sc2() || is_aml_s4d()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 1, 0, 1);
			} else if (is_aml_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 3, 0, 2); /* core1 */
			} else if (is_aml_t7_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(VPP_VD3_DSC_CTRL,
					 1, 4, 1); /* core1a */
				VSYNC_WR_DV_REG_BITS
					(VPP_VD2_DSC_CTRL,
					 1, 4, 1); /* core1b */
				VSYNC_WR_DV_REG_BITS
					(VPP_VD3_DSC_CTRL,
					 1, 4, 1); /* core1c */
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* bypass core 1 */
					 1, 16, 1);
			}
		}
	}

	if (is_aml_g12() || is_aml_tm2_stbmode() ||
	    is_aml_t7_stbmode() || is_aml_sc2() ||
	    is_aml_s4d()) {
		VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL0,
			(el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
			bl_enable | composer_enable << 1 | el_41_mode << 2);
		if (copy_core1a_to_core1b) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1B_SWAP_CTRL0,
				 (el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
				 bl_enable | composer_enable << 1 |
				 el_41_mode << 2);
		}
		if (copy_core1a_to_core1c) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1C_SWAP_CTRL0,
				 (el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
				 bl_enable | composer_enable << 1 |
				 el_41_mode << 2);
		}
	} else {
	/* enable core1 */
		VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL0,
				bl_enable << 0 |
				composer_enable << 1 |
				el_41_mode << 2);
	}
	set_dovi_setting_update_flag(true);
	return 0;
}

static int dv_core1a_set(u32 dm_count,
				  u32 comp_count,
				  u32 lut_count,
				  u32 *p_core1_dm_regs,
				  u32 *p_core1_comp_regs,
				  u32 *p_core1_lut,
				  u32 *last_dm,
				  u32 *last_comp,
				  int hsize,
				  int vsize,
				  int core1a_enable,
				  int scramble_en,
				  bool amdv_src,
				  int lut_endian,
				  bool reset)
{
	u32 count;
	u32 bypass_flag = 0;
	int el_enable = 0;/*default 0?*/
	int el_41_mode = 0;/*default 0?*/
	int composer_enable =
		core1a_enable && el_enable && (amdv_mask & 1);
	int i;
	bool set_lut = false;
	bool bypass_core1 = (!hsize || !vsize ||
				!(amdv_mask & 1));
	int copy_core1a_to_core1b = ((copy_core1a & 1) &&
				(is_aml_tm2_stbmode() || is_aml_t7_stbmode()));
	int copy_core1a_to_core1c = ((copy_core1a & 2) && is_aml_t7_stbmode());

	/* G12A: make sure the BL is enable for the very 1st frame*/
	/* Register: dolby_path_ctrl[0] = 0 to enable BL*/
	/*	     dolby_path_ctrl[1] = 0 to enable EL*/
	/*	     dolby_path_ctrl[2] = 0 to enable OSD*/
	if ((is_aml_g12() || is_aml_tm2_stbmode()) &&
	    dv_core1[0].core1_on == 0) {
		pr_dv_dbg("((%s %d, register AMDV_PATH_CTRL: %x))\n",
			__func__, __LINE__,
			VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
		if ((VSYNC_RD_DV_REG(AMDV_PATH_CTRL) & 0x1) != 0) {
			pr_dv_dbg("core1a disable for 1st frame.Re-enable core1a\n");
			VSYNC_WR_DV_REG_BITS(AMDV_PATH_CTRL, 0, 0, 1);
			pr_dv_dbg("((%s %d, enable core1a, AMDV_PATH_CTRL: %x))\n",
				__func__, __LINE__,
				VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
		}
	}

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (force_update_reg & 0x1000)
		return 0;

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;

	if (force_update_reg & 8)
		set_lut = true;

	if (force_update_reg & 1)
		reset = true;

	if ((!dolby_vision_on || reset) && core1a_enable) {
		amdv_core_reset(AMDV_CORE1A);
		if (copy_core1a_to_core1b)
			amdv_core_reset(AMDV_CORE1B);
		if (copy_core1a_to_core1c)
			amdv_core_reset(AMDV_CORE1C);
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (core1a_enable &&
	    dv_core1[0].core1_on_cnt < DV_CORE1_RECONFIG_CNT) {
		reset = true;
		dv_core1[0].core1_on_cnt++;
	}
	//if (reset)
		//update_core2_reg = true;

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC)
		set_lut = true;

	if (debug_dolby & 2)
		pr_dv_dbg("core1a cnt %d %d,flag %d,reset %d,size %dx%d,%x\n",
			     dv_core1[0].core1_on_cnt, core1a_enable,
			     stb_core_setting_update_flag,
			     reset, hsize, vsize, p_core1_dm_regs[0]);

	if (is_amdv_stb_mode() && core1a_enable) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY1A) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1A) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1A);
		if (get_dv_vpu_mem_power_status(VPU_PRIME_DOLBY_RAM) ==
			VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_PRIME_DOLBY_RAM) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
	}
	VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL, 0);
	/* VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL0, 0); */
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL2, (hsize << 16) | vsize);

	/*tm2 reva: use tvcore do detunnel*/
	/*tm2 revb/T7: use core1 do detunnel*/
	if (is_aml_stb_hdmimode() && core1_detunnel() && amdv_src) {
		/*bit 14~bit31: detunnel bit swap, 0x2c2d0:5-4-1-3-2-0*/
		/*bit 29=1 disable el mem read/write, conflict with detunnel*/
		VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL5, 0xb0b4001a);
	} else {
		VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL5, 0xa);
	}

	VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 4, 4);
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 2, 1);

	if (copy_core1a_to_core1b) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY1B) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1B) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1B);
		VSYNC_WR_DV_REG(AMDV_CORE1B_CLKGATE_CTRL, 0);
		/* VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL0, 0); */
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL1,
			((hsize + 0x80) << 16) | (vsize + 0x40));
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL3,
			(hwidth << 16) | vwidth);
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL4,
			(hpotch << 16) | vpotch);
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL2,
			(hsize << 16) | vsize);
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL5, 0xa);

		VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_CTRL, 0x0);
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 4, 4);
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 2, 1);
	}

	if (copy_core1a_to_core1c) {
		VSYNC_WR_DV_REG(AMDV_CORE1C_CLKGATE_CTRL, 0);
		/* VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL0, 0); */
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL1,
				((hsize + 0x80) << 16) | (vsize + 0x40));
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL3,
				(hwidth << 16) | vwidth);
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL4,
				(hpotch << 16) | vpotch);
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL2,
				(hsize << 16) | vsize);
		VSYNC_WR_DV_REG(AMDV_CORE1C_SWAP_CTRL5, 0xa);

		VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_CTRL, 0x0);
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 4, 4);
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 2, 1);
	}
	/*For HDMI or OTT HDR10/HLG/SDR8/SDR10 inputs we bypass the composer*/
	if ((is_aml_stb_hdmimode() && hdmi_path_id == VD1_PATH) ||
	    !amdv_src || bypass_core1a_composer)
		bypass_flag |= 1;
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (need_skip_cvm(0))
		bypass_flag |= 1 << 2;
	if (el_41_mode)
		bypass_flag |= 1 << 3;

	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 1,
		0x70 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 1,
		0x70 | bypass_flag); /* for delay */
	if (copy_core1a_to_core1b) {
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 1,
			0x70 | bypass_flag); /* bypass CVM and/or CSC */
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 1,
			0x70 | bypass_flag); /* for delay */
	}
	if (copy_core1a_to_core1c) {
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 1,
				0x70 | bypass_flag); /* bypass CVM and/or CSC */
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 1,
				0x70 | bypass_flag); /* for delay */
	}
	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1A_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG
				(AMDV_CORE1B_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG
				(AMDV_CORE1C_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
		}

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_comp_regs[i] != last_comp[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1A_REG_START + 6 + 44 + i,
				 p_core1_comp_regs[i]);
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG
					(AMDV_CORE1B_REG_START + 6 + 44 + i,
					 p_core1_comp_regs[i]);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG
					(AMDV_CORE1C_REG_START + 6 + 44 + i,
					 p_core1_comp_regs[i]);
		}
	VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 3, 1);
	if (copy_core1a_to_core1b)
		VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 3, 1);
	if (copy_core1a_to_core1c)
		VSYNC_WR_DV_REG(AMDV_CORE1C_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
		VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_CTRL, 0x1401);
		if (copy_core1a_to_core1b)
			VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_CTRL, 0x1401);
		if (copy_core1a_to_core1c)
			VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_CTRL, 0x1401);
		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i + 3]);
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i + 2]);
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i + 1]);
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i]);
				if (copy_core1a_to_core1b) {
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i + 3]);
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i + 2]);
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i + 1]);
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i]);
				}
				if (copy_core1a_to_core1c) {
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i + 3]);
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i + 2]);
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i + 1]);
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i]);
				}
			}
		} else {
			for (i = 0; i < count; i++)
				VSYNC_WR_DV_REG(AMDV_CORE1A_DMA_PORT,
						p_core1_lut[i]);
			if (copy_core1a_to_core1b) {
				for (i = 0; i < count; i++)
					VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
							p_core1_lut[i]);
			}
			if (copy_core1a_to_core1c) {
				for (i = 0; i < count; i++)
					VSYNC_WR_DV_REG(AMDV_CORE1C_DMA_PORT,
							p_core1_lut[i]);
			}
		}
	}

	if (dv_core1[0].run_mode_count
		< amdv_run_mode_delay) {
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC0,
			 (0x200 << 10) | 0x200);
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC1,
			 (0x200 << 10) | 0x200);
		if (is_aml_g12() || is_aml_sc2() || is_aml_s4d() || is_aml_tm2_stbmode())
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 1,
				 0, 1);
		else if (is_aml_t7_stbmode())
			VSYNC_WR_DV_REG_BITS
				(VPP_VD3_DSC_CTRL,
				 1, 4, 1);

	} else {
		if (dv_core1[0].run_mode_count >
			amdv_run_mode_delay) {
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC0,
				 (0x3ff << 20) |
				 (0x3ff << 10) |
				 0x3ff);
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC1,
				 0);
		}
		if (dv_core1[0].core1_on && !bypass_core1) {
			if (is_aml_g12()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 0, 1);
			} else if (is_aml_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 8, 2);
				if (copy_core1a_to_core1b)
					/*vd2 to dolby_s1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 10, 2);
				else
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 24, 2);
				if (copy_core1a_to_core1b) {
					/*dolby_s1 in sel vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 19, 1);
					/*dolby_s1 out to vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 23, 1);
					/* vd2 from dolby_s1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 26, 2);
				}
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 0, 1); /*enable core1a*/
			} else if (is_aml_t7_stbmode()) {
				/* vd1 to core1a*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 0, 0, 3);
				/*core1a bl in sel vd1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 0, 2, 2);
				/*core1a el in sel null*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 3, 4, 2);
				/*core1a out to vd1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 0, 18, 2);
				/* vd1 from core1a*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 0, 12, 3);
				/*enable core1a*/
				VSYNC_WR_DV_REG_BITS
					(VPP_VD1_DSC_CTRL,
					 0, 4, 1);
				if (copy_core1a_to_core1b) {
					/*vd2 to core1b*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 3, 4, 3);
					/*core1b in sel vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 1, 10, 2);
					/*core1b out to vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 1, 22, 2);
					/*vd2 out sel pre core1b*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 3, 16, 3);
					/*enable core1b*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 0, 4, 1);
				}
				if (copy_core1a_to_core1c) {
					/*core1c to vd3*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 4, 8, 3);
					/*core1c in sel vd3*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 2, 12, 2);
					/*core1c out to vd3*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 2, 24, 2);
					/*vd3 from core1c*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 4, 20, 3);

					/*enable core1c*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 0, 4, 1);
				}
			} else if (is_aml_sc2() || is_aml_s4d()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 8, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 24, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 0, 1); /* core1 */
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* enable core 1 */
					 0, 16, 1);
			}
		} else if (dv_core1[0].core1_on &&
			bypass_core1) {
			if (is_aml_g12() || is_aml_sc2() || is_aml_s4d()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 1, 0, 1);/* core1a bypass */
			} else if (is_aml_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 1, 0, 1); /* core1a bypass */
			} else if (is_aml_t7_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(VPP_VD3_DSC_CTRL,
					 1, 4, 1); /* core1a bypass*/
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* bypass core 1 */
					 1, 16, 1);
			}
		}
	}

	if (is_aml_g12() || is_aml_tm2_stbmode() ||
	    is_aml_t7_stbmode() || is_aml_sc2() ||
	    is_aml_s4d()) {
		VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL0,
			(el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
			core1a_enable | composer_enable << 1 | el_41_mode << 2);
		if (copy_core1a_to_core1b) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1B_SWAP_CTRL0,
				 (el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
				 core1a_enable | composer_enable << 1 |
				 el_41_mode << 2);
		}
		if (copy_core1a_to_core1c) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1C_SWAP_CTRL0,
				 (el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
				 core1a_enable | composer_enable << 1 |
				 el_41_mode << 2);
		}
	} else {
	/* enable core1 */
		VSYNC_WR_DV_REG(AMDV_CORE1A_SWAP_CTRL0,
				core1a_enable << 0 |
				composer_enable << 1 |
				el_41_mode << 2);
	}

	#ifdef REMOVE_OLD_DV_FUNC
	/*tm2 revb: standard in, use core1 do detunnel, set core1 diag=3*/
	/*tm2 reva: standard in, use tvcore do detunnel, set core1 diag=0*/
	if (is_aml_stb_hdmimode() && core1_detunnel() && amdv_src)
		VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 0xe7, 3);
	else
		VSYNC_WR_DV_REG(AMDV_CORE1A_REG_START + 0xe7, 0);
	#endif
	set_dovi_setting_update_flag(true);

	if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
	    !(dolby_vision_flags & FLAG_DISABLE_CRC))
		VSYNC_WR_DV_REG(AMDV_CORE1_CRC_CTRL, 1);

	return 0;
}

static int dv_core1b_set(u32 dm_count,
				  u32 comp_count,
				  u32 lut_count,
				  u32 *p_core1_dm_regs,
				  u32 *p_core1_comp_regs,
				  u32 *p_core1_lut,
				  u32 *last_dm,
				  u32 *last_comp,
				  int hsize,
				  int vsize,
				  int core1b_enable,
				  int scramble_en,
				  bool amdv_src,
				  int lut_endian,
				  bool reset)
{
	u32 count;
	u32 bypass_flag = 0;
	int el_enable = 0;/*default 0?*/
	int el_41_mode = 0;/*default 0?*/
	int composer_enable =
		core1b_enable && el_enable && (amdv_mask & 1);
	int i;
	bool set_lut = false;
	bool bypass_core1 = (!hsize || !vsize ||
				!(amdv_mask & 1));

	if (!core1b_enable)
		return 0;
	/* G12A: make sure the BL is enable for the very 1st frame*/
	/* Register: dolby_path_ctrl[0] = 0 to enable BL*/
	/*	     dolby_path_ctrl[1] = 0 to enable EL*/
	/*	     dolby_path_ctrl[2] = 0 to enable OSD*/

#ifdef REMOVE_OLD_DV_FUNC
	if ((is_aml_g12() || is_aml_tm2_stbmode()) &&
	    dv_core1[1].core1_on == 0) {
		pr_dv_dbg("((%s %d, register AMDV_PATH_CTRL: %x))\n",
			__func__, __LINE__,
			VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
		if ((VSYNC_RD_DV_REG(AMDV_PATH_CTRL) & 0x2) != 0) {
			pr_dv_dbg("core1b is disable for 1st frame.Re-enable core1b\n");
			VSYNC_WR_DV_REG_BITS(AMDV_PATH_CTRL, 0, 1, 1);
			pr_dv_dbg("((%s %d, enable core1b, AMDV_PATH_CTRL: %x))\n",
				__func__, __LINE__,
				VSYNC_RD_DV_REG(AMDV_PATH_CTRL));
		}
	}
#endif

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (force_update_reg & 0x2000)
		return 0;

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;

	if (force_update_reg & 0x20)
		set_lut = true;

	if (force_update_reg & 0x40)
		reset = true;

	if ((!dolby_vision_on || reset) && core1b_enable) {
		amdv_core_reset(AMDV_CORE1B);
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (core1b_enable &&
	    dv_core1[1].core1_on_cnt < DV_CORE1_RECONFIG_CNT) {
		reset = true;
		dv_core1[1].core1_on_cnt++;
	}
	if (debug_dolby & 2)
		pr_dv_dbg("core1b cnt %d %d,reset %d,size %dx%d,%x\n",
			     dv_core1[1].core1_on_cnt, core1b_enable,
			     reset,  hsize, vsize, p_core1_dm_regs[0]);
	//if (reset)
		//update_core2_reg = true;

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC)
		set_lut = true;

	if ((core1b_enable && (amdv_mask & 1))) {
		if (is_aml_tm2_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				/* vd2 dolby enable, vd2 to core1b */
				 0, 1, 1);
		} else if (is_aml_t7_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(VPP_VD2_DSC_CTRL,
				/* vd2 dolby enable, vd2 to core1b */
				 0, 4, 1);
		}
	} else {
		if (is_aml_tm2_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				/* vd2 dolby disable, vd2 to vpp */
				 1, 1, 1);
		} else if (is_aml_t7_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(VPP_VD2_DSC_CTRL,
				/* vd2 dolby disable, vd2 to core1b */
				 1, 4, 1);
		}
	}

	if (core1b_enable) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY1B) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1B) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1B);
	}
	VSYNC_WR_DV_REG(AMDV_CORE1B_CLKGATE_CTRL, 0);
	/* VSYNC_WR_DV_REG(AMDV_CORE1_SWAP_CTRL0, 0); */
	VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL3,
		(hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL4,
		(hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL2,
		(hsize << 16) | vsize);
	VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL5, 0xa);

	VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 4, 4);
	VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 2, 1);

	/*For HDMI or OTT HDR10/HLG/SDR8/SDR10 inputs we bypass the composer*/
	if ((is_aml_stb_hdmimode() && hdmi_path_id == VD2_PATH) ||
	    !amdv_src || bypass_core1b_composer)
		bypass_flag |= 1;
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (need_skip_cvm(0))
		bypass_flag |= 1 << 2;
	if (el_41_mode)
		bypass_flag |= 1 << 3;

	VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 1,
		0x70 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 1,
		0x70 | bypass_flag); /* for delay */

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1B_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
		}

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_comp_regs[i] != last_comp[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE1B_REG_START + 6 + 44 + i,
				 p_core1_comp_regs[i]);
		}
	/* metadata program done */
	VSYNC_WR_DV_REG(AMDV_CORE1B_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
		VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_CTRL, 0x1401);
		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
					p_core1_lut[i + 3]);
				VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
					p_core1_lut[i + 2]);
				VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
					p_core1_lut[i + 1]);
				VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
					p_core1_lut[i]);
			}
		} else {
			for (i = 0; i < count; i++)
				VSYNC_WR_DV_REG(AMDV_CORE1B_DMA_PORT,
					p_core1_lut[i]);
		}
	}

	if (dv_core1[1].run_mode_count
		< amdv_run_mode_delay) {
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC0,
			 (0x200 << 10) | 0x200);
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC1,
			 (0x200 << 10) | 0x200);
		if (is_aml_tm2_stbmode())
			VSYNC_WR_DV_REG_BITS
				(AMDV_PATH_CTRL,
				 1, 0, 1);
		else if (is_aml_t7_stbmode())
			VSYNC_WR_DV_REG_BITS
				(VPP_VD2_DSC_CTRL,
				 1, 4, 1);
	} else {
		if (dv_core1[1].run_mode_count >
			amdv_run_mode_delay) {
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC0,
				 (0x3ff << 20) |
				 (0x3ff << 10) |
				 0x3ff);
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC1,
				 0);
		}
		if (dv_core1[1].core1_on && !bypass_core1) {
			if (is_aml_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 8, 2);
				/*vd2 to dolby_s1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 3, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 24, 2);
				/*dolby_s1 in sel vd2*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 1, 19, 1);
				/*dolby_s1 out to vd2*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 1, 23, 1);
				/* vd2 from dolby_s1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 3, 26, 2);
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 0, 1, 1); /* enable core1b */
			} else if (is_aml_t7_stbmode()) {
				/* vd1 to core1a*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 0, 0, 3);
				/*core1a bl in sel vd1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 0, 2, 2);
				/*core1a el in sel null*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 3, 4, 2);
				/*core1a out to vd1*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 0, 18, 2);
				/* vd1 from core1a*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 0, 12, 3);
				/*vd2 to core1b*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 3, 4, 3);
				/*core1b in sel vd2*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 1, 10, 2);
				/*core1b out to vd2*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL2,
					 1, 22, 2);
				/*vd2 out sel pre core1b*/
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_SWAP_CTRL1,
					 3, 16, 3);
				/*enable core1b*/
				VSYNC_WR_DV_REG_BITS
					(VPP_VD2_DSC_CTRL,
					 0, 4, 1);
			}
		} else if (dv_core1[1].core1_on &&
			bypass_core1) {
			if (is_aml_tm2_stbmode())
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 1, 1, 1); /* bypass core1b */
			else if (is_aml_t7_stbmode())
				VSYNC_WR_DV_REG_BITS
					(VPP_VD2_DSC_CTRL,
					 1, 4, 1); /* bypass core1b */
		}
	}

	if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
		VSYNC_WR_DV_REG(AMDV_CORE1B_SWAP_CTRL0,
			(el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
			core1b_enable | composer_enable << 1 |
			el_41_mode << 2);
	}
	set_dovi_setting_update_flag(true);
	return 0;
}

static int dv_core2c_set
	(u32 dm_count,
	 u32 lut_count,
	 u32 *p_core2_dm_regs,
	 u32 *p_core2_lut,
	 int hsize,
	 int vsize,
	 int amdv_enable,
	 int lut_endian)
{
	u32 count;
	int i;
	bool set_lut = false;
	bool reset = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg2;
	u32 *last_lut = (u32 *)&dovi_setting.dm_lut2;
	u32 bypass_flag = 0;
	int lut_size = sizeof(dovi_setting.dm_lut2) / sizeof(u32);
	u32 tmp_h = hsize;
	u32 tmp_v = vsize;

	if (multi_dv_mode) {
		last_dm = (u32 *)&m_dovi_setting.dm_reg2;
		last_lut = (u32 *)&m_dovi_setting.dm_lut2;
		lut_size = sizeof(m_dovi_setting.dm_lut2) / sizeof(u32);
	}

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (force_update_reg & 0x2000)
		return 0;

	if (!dolby_vision_on || force_reset_core2) {
		amdv_core_reset(AMDV_CORE2C);
		force_reset_core2 = false;
		reset = true;
		pr_dv_dbg("reset core2a\n");
	}

	if (amdv_enable &&
	    amdv_core2_on_cnt < DV_CORE2_RECONFIG_CNT) {
		reset = true;
		amdv_core2_on_cnt++;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (update_core2_reg) {
		update_core2_reg = false;
		reset = true;
	}

	if (force_update_reg & 0x10)
		set_lut = true;

	if (force_update_reg & 2)
		reset = true;

	if (is_amdv_stb_mode()) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY2) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY2) == VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY2);
	}

	VSYNC_WR_DV_REG(AMDV_CORE2C_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL0, 0);
	if (is_aml_t7_stbmode() || reset) {
		/*update timing to 1080p if size < 1080p, otherwise display color dot*/
		update_dvcore2_timing(&tmp_h, &tmp_v);
		VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL1,
			((tmp_h + g_htotal_add) << 16) | (tmp_v
			+ ((g_vtiming & 0xff000000) ?
				((g_vtiming >> 24) & 0xff) : g_vtotal_add)
			+ ((g_vtiming & 0xff0000) ?
				((g_vtiming >> 16) & 0xff) : g_vsize_add)));
		VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL2,
			(tmp_h << 16) | (tmp_v
			+ ((g_vtiming & 0xff0000) ?
				((g_vtiming >> 16) & 0xff) : g_vsize_add)));
	}
	VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL3,
		(g_hwidth << 16) | ((g_vtiming & 0xff00) ?
				((g_vtiming >> 8) & 0xff) : g_vwidth));
	VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL4,
		(g_hpotch << 16) | ((g_vtiming & 0xff) ?
				(g_vtiming & 0xff) : g_vpotch));
	VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL5,  0xa8000000);

	VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(AMDV_CORE2C_REG_START + 2, 1);
	if (need_skip_cvm(1))
		bypass_flag |= 1 << 0;
	VSYNC_WR_DV_REG(AMDV_CORE2C_REG_START + 2, 1);
	VSYNC_WR_DV_REG(AMDV_CORE2C_REG_START + 1,
		2 | bypass_flag);
	VSYNC_WR_DV_REG(AMDV_CORE2C_REG_START + 1,
		2 | bypass_flag);
	VSYNC_WR_DV_REG(AMDV_CORE2C_CTRL, 0);

	VSYNC_WR_DV_REG(AMDV_CORE2C_CTRL, 0);

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++) {
		if (reset ||
		    p_core2_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE2C_REG_START + 6 + i,
				 p_core2_dm_regs[i]);
			set_lut = true;
		}
	}
	/*CP_FLAG_CHANGE_TC2 is not set in idk2.6, need to check change*/
	if (multi_dv_mode && !set_lut) {
		for (i = 0; i < lut_size / 2; i++) {
			if (p_core2_lut[i] != last_lut[i] ||
			    p_core2_lut[lut_size - 1 - i] != last_lut[lut_size - 1 - i]) {
				set_lut = true;
				if ((debug_dolby & 2))
					pr_dv_dbg("core2 lut change lut[%d] %x->%x\n",
					i, last_lut[i], p_core2_lut[i]);
				break;
			}
		}
	}
	if (multi_dv_mode) {
		if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2)
			set_lut = true;
	} else {
		if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2_OLD)
			set_lut = true;
	}
	if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
		set_lut = false;

	if (debug_dolby & 2)
		pr_dv_dbg("core2c potch %x %x, reset %d, %d, flag %x, size %d %d\n",
			     g_hpotch, g_vpotch, reset, set_lut,
			     stb_core_setting_update_flag, hsize, vsize);

	/* core2 metadata program done */
	VSYNC_WR_DV_REG(AMDV_CORE2C_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset || force_set_lut)) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_aml_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(AMDV_CORE2C_CLKGATE_CTRL,
				2, 2, 2);
#endif
		VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_CTRL, 0x1401);

		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_PORT,
					p_core2_lut[i + 3]);
				VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_PORT,
					p_core2_lut[i + 2]);
				VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_PORT,
					p_core2_lut[i + 1]);
				VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_PORT,
					p_core2_lut[i]);
			}
		} else {
			for (i = 0; i < count; i++) {
				VSYNC_WR_DV_REG(AMDV_CORE2C_DMA_PORT,
					p_core2_lut[i]);
			}
		}
	}
	force_set_lut = false;

	/* enable core2 */
	VSYNC_WR_DV_REG(AMDV_CORE2C_SWAP_CTRL0, amdv_enable << 0);
	return 0;
}

static int dv_core2a_set
	(u32 dm_count,
	 u32 lut_count,
	 u32 *p_core2_dm_regs,
	 u32 *p_core2_lut,
	 int hsize,
	 int vsize,
	 int amdv_enable,
	 int lut_endian)
{
	u32 count;
	int i;
	bool set_lut = false;
	bool reset = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg2;
	u32 *last_lut = (u32 *)&dovi_setting.dm_lut2;
	u32 bypass_flag = 0;
	int lut_size = sizeof(dovi_setting.dm_lut2) / sizeof(u32);
	u32 tmp_h = hsize;
	u32 tmp_v = vsize;

	if (multi_dv_mode) {
		last_dm = (u32 *)&m_dovi_setting.dm_reg2;
		last_lut = (u32 *)&m_dovi_setting.dm_lut2;
		lut_size = sizeof(m_dovi_setting.dm_lut2) / sizeof(u32);
	}

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (force_update_reg & 0x2000)
		return 0;

	if (!dolby_vision_on || force_reset_core2) {
		amdv_core_reset(AMDV_CORE2A);
		force_reset_core2 = false;
		reset = true;
		pr_dv_dbg("reset core2a\n");
	}

	if (amdv_enable &&
	    amdv_core2_on_cnt < DV_CORE2_RECONFIG_CNT) {
		reset = true;
		amdv_core2_on_cnt++;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (update_core2_reg) {
		update_core2_reg = false;
		reset = true;
	}

	if (force_update_reg & 0x10)
		set_lut = true;

	if (force_update_reg & 2)
		reset = true;

	if (is_amdv_stb_mode()) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY2) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY2) == VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY2);
	}

	VSYNC_WR_DV_REG(AMDV_CORE2A_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL0, 0);
	if (is_amdv_stb_mode() || reset) {
		/*update timing to 1080p if size < 1080p, otherwise display color dot*/
		update_dvcore2_timing(&tmp_h, &tmp_v);
		VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL1,
			((tmp_h + g_htotal_add) << 16) | (tmp_v
			+ ((g_vtiming & 0xff000000) ?
				((g_vtiming >> 24) & 0xff) : g_vtotal_add)
			+ ((g_vtiming & 0xff0000) ?
				((g_vtiming >> 16) & 0xff) : g_vsize_add)));
		VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL2,
			(tmp_h << 16) | (tmp_v
			+ ((g_vtiming & 0xff0000) ?
				((g_vtiming >> 16) & 0xff) : g_vsize_add)));
	}
	VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL3,
		(g_hwidth << 16) | ((g_vtiming & 0xff00) ?
				((g_vtiming >> 8) & 0xff) : g_vwidth));
	VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL4,
		(g_hpotch << 16) | ((g_vtiming & 0xff) ?
				(g_vtiming & 0xff) : g_vpotch));
	if (is_aml_txlx_stbmode())
		VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL5, 0xf8000000);
	else if (is_aml_g12() || is_aml_tm2_stbmode() ||
		 is_aml_t7_stbmode() || is_aml_sc2() ||
		 is_aml_s4d())
		VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL5,  0xa8000000);
	else
		VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL5, 0x0);
	VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(AMDV_CORE2A_REG_START + 2, 1);
	if (need_skip_cvm(1))
		bypass_flag |= 1 << 0;
	VSYNC_WR_DV_REG(AMDV_CORE2A_REG_START + 2, 1);
	VSYNC_WR_DV_REG(AMDV_CORE2A_REG_START + 1,
		2 | bypass_flag);
	VSYNC_WR_DV_REG(AMDV_CORE2A_REG_START + 1,
		2 | bypass_flag);
	VSYNC_WR_DV_REG(AMDV_CORE2A_CTRL, 0);

	VSYNC_WR_DV_REG(AMDV_CORE2A_CTRL, 0);

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++) {
		if (reset ||
		    p_core2_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(AMDV_CORE2A_REG_START + 6 + i,
				 p_core2_dm_regs[i]);
			set_lut = true;
			if ((debug_dolby & 2))
				pr_dv_dbg("core2 dm change dm_regs[%d] %x->%x\n",
					     i, last_dm[i], p_core2_dm_regs[i]);
		}
	}
	/*CP_FLAG_CHANGE_TC2 is not set in idk2.6, need to check change*/
	if (multi_dv_mode && !set_lut) {
		for (i = 0; i < lut_size / 2; i++) {
			if (p_core2_lut[i] != last_lut[i] ||
			    p_core2_lut[lut_size - 1 - i] != last_lut[lut_size - 1 - i]) {
				set_lut = true;
				if ((debug_dolby & 2))
					pr_dv_dbg("core2 lut change lut[%d] %x->%x\n",
					i, last_lut[i], p_core2_lut[i]);
				break;
			}
		}
	}

	if (multi_dv_mode) {
		if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2)
			set_lut = true;
	} else {
		if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2_OLD)
			set_lut = true;
	}

	if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
		set_lut = false;

	if (debug_dolby & 2)
		pr_dv_dbg("core2a %x %x,reset %d,%d,%d,flag %x,size %d %d %d %d\n",
			     g_hpotch, g_vpotch, reset, set_lut, force_set_lut,
			     stb_core_setting_update_flag,
			     hsize, vsize, tmp_h, tmp_v);

	/* core2 metadata program done */
	VSYNC_WR_DV_REG(AMDV_CORE2A_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset || force_set_lut)) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_aml_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(AMDV_CORE2A_CLKGATE_CTRL,
				2, 2, 2);
#endif
		VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_CTRL, 0x1401);

		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_PORT,
					p_core2_lut[i + 3]);
				VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_PORT,
					p_core2_lut[i + 2]);
				VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_PORT,
					p_core2_lut[i + 1]);
				VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_PORT,
					p_core2_lut[i]);
			}
		} else {
			for (i = 0; i < count; i++) {
				VSYNC_WR_DV_REG(AMDV_CORE2A_DMA_PORT,
					p_core2_lut[i]);
			}
		}
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		/* core2 lookup table program done */
		if (is_aml_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS
				(AMDV_CORE2A_CLKGATE_CTRL, 0, 2, 2);
#endif
	}
	force_set_lut = false;

	/* enable core2 */
	VSYNC_WR_DV_REG(AMDV_CORE2A_SWAP_CTRL0, amdv_enable << 0);
	return 0;
}

bool is_core3_mute_reg(int index)
{
	return (index == 12) || /* ipt_scale for ipt*/
	(index >= 16 && index <= 17) || /* rgb2yuv scale for yuv */
	(index >= 5 && index <= 9); /* lms2rgb coeff for rgb */
}

static int dv_core3_set
	(u32 dm_count,
	 u32 md_count,
	 u32 *p_core3_dm_regs,
	 u32 *p_core3_md_regs,
	 int hsize,
	 int vsize,
	 int amdv_enable,
	 int scramble_en,
	 u8 pps_state)
{
	u32 count;
	int i;
	int vsize_hold = 0x10;
	u32 diag_mode = 0;
	u32 cur_dv_mode = dolby_vision_mode;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg3;
	bool reset = false;
	u32 diag_enable = 0;
	bool reset_post_table = false;
#ifdef V2_4_3
	const struct vinfo_s *vinfo = get_current_vinfo();
#endif
	u32 diagnostic_enable = new_dovi_setting.diagnostic_enable;
	u32 dovi_ll_enable = new_dovi_setting.dovi_ll_enable;
	u32 *mode_changed = &new_dovi_setting.mode_changed;
	u32 *vsvdb_changed = &new_dovi_setting.vsvdb_changed;

	if (multi_dv_mode) {
		last_dm = (u32 *)&m_dovi_setting.dm_reg3;
		diagnostic_enable = new_m_dovi_setting.diagnostic_enable;
		dovi_ll_enable = new_m_dovi_setting.dovi_ll_enable;
		mode_changed = &new_m_dovi_setting.mode_changed;
		vsvdb_changed = &new_m_dovi_setting.vsvdb_changed;
	}

	if (diagnostic_enable ||
	    dovi_ll_enable)
		diag_enable = 1;
	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on ||
		(dolby_vision_flags & FLAG_CERTIFICAION))
		reset = true;

	if (force_update_reg & 4)
		reset = true;

	if (is_aml_gxm()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if ((first_reseted & 0x2) == 0) {
			first_reseted = (first_reseted | 0x2);
			reset = true;
		}
#endif
	} else {
		if (dv_core1[0].run_mode_count == 0)
			reset = true;
	}
	if ((cur_dv_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL ||
	     cur_dv_mode == AMDV_OUTPUT_MODE_IPT) && diag_enable) {
		cur_dv_mode = dv_ll_output_mode & 0xff;

		if (is_amdv_stb_mode()) {
			if (diagnostic_enable)
				diag_mode = 3;/*LL rgb*/
			else
				diag_mode = 0x20;/*LL yuv*/
		} else {
			diag_mode = 3;
		}
	}
#ifdef V2_4_3
	else if (need_send_emp_meta(vinfo))
		diag_mode = 1;
#endif

	if (dolby_vision_on &&
		(last_dolby_vision_ll_policy !=
		dolby_vision_ll_policy ||
		*mode_changed ||
		*vsvdb_changed ||
		pps_state)) {
		last_dolby_vision_ll_policy =
			dolby_vision_ll_policy;
		*vsvdb_changed = 0;
		*mode_changed = 0;
		/* TODO: verify 962e case */
		if (is_amdv_stb_mode()) {
			if (dovi_ll_enable &&
				diagnostic_enable == 0) {
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL,
					 3, 6, 2); /* post matrix */
				VSYNC_WR_DV_REG_BITS
					(VPP_MATRIX_CTRL,
					 1, 0, 1); /* post matrix */
			} else {
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL,
					 0, 6, 2); /* post matrix */
				VSYNC_WR_DV_REG_BITS
					(VPP_MATRIX_CTRL,
					 0, 0, 1); /* post matrix */
			}
		} else if (is_aml_txlx_stbmode()) {
			if (pps_state == 2) {
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL,
					 1, 0, 1); /* skip pps/dither/cm */
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA0, 0x08000800);
			} else if (pps_state == 1) {
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL,
					 0, 0, 1); /* enable pps/dither/cm */
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA0, 0x20002000);
			}
			if (dovi_ll_enable &&
				diagnostic_enable == 0) {
				/*bypass gainoff to vks */
				/*enable wn tp vks*/
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL, 0, 2, 1);
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL, 1, 1, 1);
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA1, 0x8000800);
				VSYNC_WR_DV_REG_BITS
					(VPP_MATRIX_CTRL,
					 1, 0, 1); /* post matrix */
			} else {
				/* bypass wm tp vks*/
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL, 1, 2, 1);
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL, 0, 1, 1);
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA1, 0x20002000);
				if (is_aml_tvmode())
					enable_rgb_to_yuv_matrix_for_dvll
						(0, NULL, 12);
				else
					VSYNC_WR_DV_REG_BITS
						(VPP_MATRIX_CTRL,
						 0, 0, 1);
			}
		}
		reset_post_table = true;
	}
	/* flush post matrix table when ll mode on and setting changed */
	if (dovi_ll_enable &&
		diagnostic_enable == 0 &&
	    dolby_vision_on &&
	    (reset_post_table || reset ||
	    memcmp(&p_core3_dm_regs[18],
		   &last_dm[18], 32)))
		enable_rgb_to_yuv_matrix_for_dvll
			(1, &p_core3_dm_regs[18], 12);
	if (is_amdv_stb_mode()) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY_CORE3) ==
			VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY_CORE3) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY_CORE3);
	}

	VSYNC_WR_DV_REG(AMDV_CORE3_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL1,
			((hsize + htotal_add) << 16)
			| (vsize + vtotal_add + vsize_add + vsize_hold * 2));
	VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL2,
			(hsize << 16) | (vsize + vsize_add));
	VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL3,
			(0x80 << 16) | vsize_hold);
	VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL4,
			(0x04 << 16) | vsize_hold);
	VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL5, 0x0000);
	if (cur_dv_mode !=
		AMDV_OUTPUT_MODE_IPT_TUNNEL)
		VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL6, 0);
	else
		VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL6,
				0x10000000);  /* swap UV */
	VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 5, 7);
	VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 4, 4);
	VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 4, 2);
	VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 2, 1);
	/* Control Register, address 0x04 2:0 RW */
	/* Output_operating mode*/
	/*   00- IPT 12 bit 444 bypass DV output*/
	/*   01- IPT 12 bit tunnelled over RGB 8 bit 444, DV output*/
	/*   02- HDR10 output, RGB 10 bit 444 PQ*/
	/*   03- Deep color SDR, RGB 10 bit 444 Gamma*/
	/*   04- SDR, RGB 8 bit 444 Gamma*/
	/*   05- YCC 12bit tunneled over RGB 8 bit 444, DV output*/
	if (multi_dv_mode &&
	    dolby_vision_ll_policy == DOLBY_VISION_LL_DISABLE &&
	    (cur_dv_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL ||
	    cur_dv_mode == AMDV_OUTPUT_MODE_IPT)) {
		/*v2.5 sink-led: output operating mode =  5 */
		VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 1, operate_mode ? operate_mode : 5);
		VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 1, operate_mode ? operate_mode : 5);
	} else {
		VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 1, cur_dv_mode);
		VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 1, cur_dv_mode);
	}
	/* for delay */
	if (dm_count == 0)
		count = 26;
	else
		count = dm_count;
	for (i = 0; i < count; i++) {
		if (reset || p_core3_dm_regs[i] != last_dm[i] ||
		    is_core3_mute_reg(i)) {
			if ((dolby_vision_flags & FLAG_MUTE) &&
			    is_core3_mute_reg(i))
				VSYNC_WR_DV_REG
					(AMDV_CORE3_REG_START + 0x6 + i,
					 0);
			else
				VSYNC_WR_DV_REG
					(AMDV_CORE3_REG_START + 0x6 + i,
					 p_core3_dm_regs[i]);
		}
	}
	/* from addr 0x18 */

	if (scramble_en) {
		if (md_count > 204) {
			pr_dv_error("core3 metadata size %d > 204 !\n",
				       md_count);
		} else {
			count = md_count;
			for (i = 0; i < count; i++) {
#ifdef FORCE_HDMI_META
				if (i == 20 &&
				    p_core3_md_regs[i] == 0x5140a3e)
					VSYNC_WR_DV_REG
						(AMDV_CORE3_REG_START +
						 0x24 + i,
						 (p_core3_md_regs[i] &
						 0xffffff00) | 0x80);
				else
#endif
					VSYNC_WR_DV_REG
						(AMDV_CORE3_REG_START +
						 0x24 + i, p_core3_md_regs[i]);
			}
			for (; i < (MAX_CORE3_MD_SIZE + 1); i++)
				VSYNC_WR_DV_REG(AMDV_CORE3_REG_START +
					0x24 + i, 0);
		}
	}

	/* from addr 0x90 */
	/* core3 metadata program done */
	VSYNC_WR_DV_REG(AMDV_CORE3_REG_START + 3, 1);

	VSYNC_WR_DV_REG(AMDV_CORE3_DIAG_CTRL, diag_mode);

	if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
	    !(dolby_vision_flags & FLAG_DISABLE_CRC))
		VSYNC_WR_DV_REG(AMDV_CORE3_CRC_CTRL, 1);
	/* enable core3 */
	VSYNC_WR_DV_REG(AMDV_CORE3_SWAP_CTRL0, (amdv_enable << 0));
	return 0;
}

static int cur_mute_type;
static char mute_type_str[4][4] = {
	"NON",
	"YUV",
	"RGB",
	"IPT"
};

void apply_stb_core_settings(dma_addr_t dma_paddr,
				    bool enable_core1a,
				    bool enable_core1b,
				    unsigned int mask,
				    bool reset_core1a,
				    bool reset_core1b,
				    u32 frame_size,
				    u32 frame_size_2,
				    u8 pps_state,
				    u32 graphics_w,
				    u32 graphics_h)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	u32 h_size[NUM_IPCORE1];/*core1a core1b input size*/
	u32 v_size[NUM_IPCORE1];
	u32 v_height;
	u32 core1_dm_count = 27;
	u32 update_bk = stb_core_setting_update_flag;
	static u32 update_flag_more;
	int mute_type;
	int i;
	u32 *last_dm[NUM_IPCORE1];
	u32 *last_comp[NUM_IPCORE1];
	struct composer_reg_ipcore *p_comp1[NUM_IPCORE1];
	struct dm_reg_ipcore1 *p_dm_reg1[NUM_IPCORE1];
	struct dm_lut_ipcore *p_dm_lut1[NUM_IPCORE1];
	struct dm_lut_ipcore *p_dm_lut2 = &new_dovi_setting.dm_lut2;
	struct dm_lut_ipcore *p_dm_lut2_last = &dovi_setting.dm_lut2;
	struct dm_reg_ipcore2 *p_dm_reg2 = &new_dovi_setting.dm_reg2;
	struct dm_reg_ipcore3 *p_dm_reg3 = &new_dovi_setting.dm_reg3;
	struct md_reg_ipcore3 *p_md_reg3 = &new_dovi_setting.md_reg3;
	static int last_core_switch;
	int cur_core_switch = 0;
	bool dv_unique_drm = false;
	enum signal_format_enum format[NUM_IPCORE1];/*core1a core1b input fmt*/

	if (multi_dv_mode) {
		cur_core_switch = get_core1a_core1b_switch();
		if (cur_core_switch != last_core_switch) {
			if (debug_dolby & 2)
				pr_dv_dbg("switch status changed %d->%d\n",
					  last_core_switch, cur_core_switch);
			last_core_switch = cur_core_switch;
			reset_core1a = true;
			reset_core1b = true;
		}

		if (!support_multi_core1())
			enable_core1b = 0;

		for (i = 0; i < NUM_IPCORE1; i++) {
			if (cur_valid_video_num <= 1) {
				p_comp1[i] = &new_m_dovi_setting.core1[0].comp_reg;
				p_dm_reg1[i] = &new_m_dovi_setting.core1[0].dm_reg;
				p_dm_lut1[i] = &new_m_dovi_setting.core1[0].dm_lut;
				last_dm[i] = (u32 *)&m_dovi_setting.core1[0].dm_reg;
				last_comp[i] = (u32 *)&m_dovi_setting.core1[0].comp_reg;
			} else {
				p_comp1[i] = &new_m_dovi_setting.core1[i].comp_reg;
				p_dm_reg1[i] = &new_m_dovi_setting.core1[i].dm_reg;
				p_dm_lut1[i] = &new_m_dovi_setting.core1[i].dm_lut;
				last_dm[i] = (u32 *)&m_dovi_setting.core1[i].dm_reg;
				last_comp[i] = (u32 *)&m_dovi_setting.core1[i].comp_reg;
			}
		}
		p_dm_lut2 = &new_m_dovi_setting.dm_lut2;
		p_dm_lut2_last = &m_dovi_setting.dm_lut2;
		p_dm_reg2 = &new_m_dovi_setting.dm_reg2;
		p_dm_reg3 = &new_m_dovi_setting.dm_reg3;
		p_md_reg3 = &new_m_dovi_setting.md_reg3;
	}
	if (multi_dv_mode && cur_core_switch) {
		h_size[1] = (frame_size >> 16) & 0xffff;
		v_size[1] = frame_size & 0xffff;
		h_size[0] = (frame_size_2 >> 16) & 0xffff;
		v_size[0] = frame_size_2 & 0xffff;
		format[0] = new_m_dovi_setting.input[1].src_format;
		format[1] = new_m_dovi_setting.input[0].src_format;
		dv_unique_drm = dv_inst[1].dv_unique_drm;
	} else {
		h_size[0] = (frame_size >> 16) & 0xffff;
		v_size[0] = frame_size & 0xffff;
		h_size[1] = (frame_size_2 >> 16) & 0xffff;
		v_size[1] = frame_size_2 & 0xffff;
		if (multi_dv_mode) {
			format[0] = new_m_dovi_setting.input[0].src_format;
			format[1] = new_m_dovi_setting.input[1].src_format;
			dv_unique_drm = dv_inst[0].dv_unique_drm;
		}
	}

	for (i = 0; i < NUM_IPCORE1; i++) {
		if (h_size[i] == 0xffff)
			h_size[i] = 0;
		if (v_size[i] == 0xffff)
			v_size[i] = 0;
	}

	if (stb_core_setting_update_flag != update_flag_more &&
	    (debug_dolby & 2))
		pr_dv_dbg
			("apply_settings update setting again %x->%x\n",
			stb_core_setting_update_flag,
			stb_core_setting_update_flag | update_flag_more);

	stb_core_setting_update_flag |= update_flag_more;

	if (is_amdv_stb_mode() &&
	    (dolby_vision_flags & FLAG_CERTIFICAION)) {
		graphics_w = dv_cert_graphic_width;
		graphics_h = dv_cert_graphic_height;
	}
	adjust_vpotch(graphics_w, graphics_h);
	if (mask & 1) {
		if (is_aml_txlx_stbmode()) {
			stb_dv_core1_set
				(dma_paddr,
				 27, 173, 256 * 5,
				 (uint32_t *)&new_dovi_setting.dm_reg1,
				 (uint32_t *)&new_dovi_setting.comp_reg,
				 (uint32_t *)&new_dovi_setting.dm_lut1,
				 h_size[0],
				 v_size[0],
				/* BL enable */
				 enable_core1a,
				/* EL enable */
				 enable_core1a && new_dovi_setting.el_flag,
				 new_dovi_setting.el_halfsize_flag,
				 dolby_vision_mode ==
				 AMDV_OUTPUT_MODE_IPT_TUNNEL,
				 new_dovi_setting.src_format == FORMAT_DOVI,
				 1,
				 reset_core1a);
		} else {
			if (!multi_dv_mode) {
				dv_core1_set
					(core1_dm_count, 173, 256 * 5,
					 (uint32_t *)&new_dovi_setting.dm_reg1,
					 (uint32_t *)&new_dovi_setting.comp_reg,
					 (uint32_t *)&new_dovi_setting.dm_lut1,
					 h_size[0],
					 v_size[0],
					/* BL enable */
					 enable_core1a,
					/* EL enable */
					 enable_core1a && new_dovi_setting.el_flag,
					 new_dovi_setting.el_halfsize_flag,
					 dolby_vision_mode ==
					 AMDV_OUTPUT_MODE_IPT_TUNNEL,
					 new_dovi_setting.src_format == FORMAT_DOVI,
					 1,
					 reset_core1a);
			} else {
				if (cur_core_switch && support_multi_core1()) {
					dv_core1a_set
						(core1_dm_count, 173, 256 * 5,
						 (uint32_t *)p_dm_reg1[1],
						 (uint32_t *)p_comp1[1],
						 (uint32_t *)p_dm_lut1[1],
						 last_dm[1],
						 last_comp[1],
						 h_size[0],
						 v_size[0],
						/* core1a enable */
						 enable_core1a,
						 dolby_vision_mode ==
						 AMDV_OUTPUT_MODE_IPT_TUNNEL,
						 format[0] ==
						 FORMAT_DOVI ||
						 (format[0] ==
						 FORMAT_DOVI_LL &&
						 !dv_unique_drm),
						 1,
						 reset_core1a);
					dv_core1b_set
						(core1_dm_count, 173, 256 * 5,
						 (uint32_t *)p_dm_reg1[0],
						 (uint32_t *)p_comp1[0],
						 (uint32_t *)p_dm_lut1[0],
						 last_dm[0],
						 last_comp[0],
						 h_size[1],
						 v_size[1],
						/* core1b enable */
						 enable_core1b,
						 dolby_vision_mode ==
						 AMDV_OUTPUT_MODE_IPT_TUNNEL,
						 format[1] ==
						 FORMAT_DOVI ||
						 format[1] ==
						 FORMAT_DOVI_LL,
						 1,
						 reset_core1b);
				} else {
					dv_core1a_set
						(core1_dm_count, 173, 256 * 5,
						 (uint32_t *)p_dm_reg1[0],
						 (uint32_t *)p_comp1[0],
						 (uint32_t *)p_dm_lut1[0],
						 last_dm[0],
						 last_comp[0],
						 h_size[0],
						 v_size[0],
						/* core1a enable */
						 enable_core1a,
						 dolby_vision_mode ==
						 AMDV_OUTPUT_MODE_IPT_TUNNEL,
						 format[0] ==
						 FORMAT_DOVI ||
						 (format[0] ==
						 FORMAT_DOVI_LL &&
						 !dv_unique_drm),
						 1,
						 reset_core1a);
					dv_core1b_set
						(core1_dm_count, 173, 256 * 5,
						 (uint32_t *)p_dm_reg1[1],
						 (uint32_t *)p_comp1[1],
						 (uint32_t *)p_dm_lut1[1],
						 last_dm[1],
						 last_comp[1],
						 h_size[1],
						 v_size[1],
						/* core1b enable */
						 enable_core1b,
						 dolby_vision_mode ==
						 AMDV_OUTPUT_MODE_IPT_TUNNEL,
						 format[1] ==
						 FORMAT_DOVI ||
						 format[1] ==
						 FORMAT_DOVI_LL,
						 1,
						 reset_core1b);
				}
			}
		}
	}

	if (mask & 2) {
		if (stb_core_setting_update_flag != CP_FLAG_CHANGE_ALL) {
			/* when FLAG_CONST_TC2 is set, */
			/* set the stb_core_setting_update_flag */
			/* until only meeting the FLAG_CHANGE_TC2 */
			if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
				stb_core2_const_flag = true;
			else if (multi_dv_mode &&
				 (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2))
				stb_core2_const_flag = false;
			else if (!multi_dv_mode &&
				 (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2_OLD))
				stb_core2_const_flag = false;
		}

		/* revert the core2 lut as last corret one when const case */
		if (stb_core2_const_flag)
			memcpy(p_dm_lut2,
			       p_dm_lut2_last,
			       sizeof(struct dm_lut_ipcore));

		if (core2_sel & 1)
			dv_core2a_set
				(24, 256 * 5,
				 (u32 *)p_dm_reg2,
				 (u32 *)p_dm_lut2,
				 graphics_w, graphics_h, 1, 1);

		if (core2_sel & 2)
			dv_core2c_set
				(24, 256 * 5,
				 (u32 *)p_dm_reg2,
				 (u32 *)p_dm_lut2,
				 graphics_w, graphics_h, 1, 1);
	}

	if (mask & 4) {
		v_height = vinfo->height;
		if ((vinfo->width == 720 &&
		     vinfo->height == 480 &&
		     vinfo->height != vinfo->field_height) ||
		    (vinfo->width == 720 &&
		     vinfo->height == 576 &&
		     vinfo->height != vinfo->field_height) ||
		    (vinfo->width == 1920 &&
		     vinfo->height == 1080 &&
		     vinfo->height != vinfo->field_height) ||
		    (vinfo->width == 1920 &&
		     vinfo->height == 1080 &&
		     vinfo->height != vinfo->field_height &&
		     vinfo->sync_duration_num
		     / vinfo->sync_duration_den == 50))
			v_height = v_height / 2;
		mute_type = get_mute_type();
		if ((get_video_mute() == VIDEO_MUTE_ON_DV) &&
		    (!(dolby_vision_flags & FLAG_MUTE) ||
		    cur_mute_type != mute_type)) {
			pr_dv_dbg("mute %s\n", mute_type_str[mute_type]);
			/* unmute vpp and mute by core3 */
			VSYNC_WR_MPEG_REG(VPP_CLIP_MISC0,
					  (0x3ff << 20) |
					  (0x3ff << 10) |
					  0x3ff);
			VSYNC_WR_MPEG_REG(VPP_CLIP_MISC1,
					  (0x0 << 20) |
					  (0x0 << 10) | 0x0);
			cur_mute_type = mute_type;
			dolby_vision_flags |= FLAG_MUTE;
		} else if ((get_video_mute() == VIDEO_MUTE_OFF) &&
			(dolby_vision_flags & FLAG_MUTE)) {
			/* vpp unmuted when dv mute */
			/* clean flag to unmute core3 here*/
			pr_dv_dbg("unmute %s\n",
				     mute_type_str[cur_mute_type]);
			cur_mute_type = MUTE_TYPE_NONE;
			dolby_vision_flags &= ~FLAG_MUTE;
		}
		dv_core3_set
			(26, p_md_reg3->size,
			 (uint32_t *)p_dm_reg3,
			 p_md_reg3->raw_metadata,
			 vinfo->width, v_height,
			 1,
			 dolby_vision_mode ==
			 AMDV_OUTPUT_MODE_IPT_TUNNEL,
			 pps_state);
#ifdef V2_4_3
		if (need_send_emp_meta(vinfo)) {
			convert_hdmi_metadata
				(p_md_reg3->raw_metadata);
#if REMOVE_OLD_DV_FUNC
			send_dv_emp(EOTF_T_DOLBYVISION,
				dolby_vision_mode ==
				AMDV_OUTPUT_MODE_IPT_TUNNEL
				? RGB_8BIT : YUV422_BIT12,
				NULL,
				hdmi_metadata,
				hdmi_metadata_size,
				false);
#endif
		}
#endif
	}
	stb_core_setting_update_flag = 0;
	update_flag_more = update_bk;
}

static void osd_bypass(int bypass)
{
	static u32 osd_backup_ctrl;
	static u32 osd_backup_eotf;
	static u32 osd_backup_mtx;

	if (bypass) {
		osd_backup_ctrl = VSYNC_RD_DV_REG(VIU_OSD1_CTRL_STAT);
		osd_backup_eotf = VSYNC_RD_DV_REG(VIU_OSD1_EOTF_CTL);
		osd_backup_mtx = VSYNC_RD_DV_REG(VPP_MATRIX_CTRL);
		VSYNC_WR_DV_REG_BITS(VIU_OSD1_EOTF_CTL, 0, 31, 1);
		VSYNC_WR_DV_REG_BITS(VIU_OSD1_CTRL_STAT, 0, 3, 1);
		VSYNC_WR_DV_REG_BITS(VPP_MATRIX_CTRL, 0, 7, 1);
	} else {
		VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, osd_backup_mtx);
		VSYNC_WR_DV_REG(VIU_OSD1_CTRL_STAT, osd_backup_ctrl);
		VSYNC_WR_DV_REG(VIU_OSD1_EOTF_CTL, osd_backup_eotf);
	}
}

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static u32 viu_eotf_ctrl_backup;
static u32 xvycc_lut_ctrl_backup;
static u32 inv_lut_ctrl_backup;
static u32 vpp_vadj_backup;
/*static u32 vpp_vadj1_backup;*/
/* static u32 vpp_vadj2_backup; */
static u32 vpp_gainoff_backup;
static u32 vpp_ve_enable_ctrl_backup;
static u32 xvycc_vd1_rgb_ctrst_backup;
#endif
static bool is_video_effect_bypass;

void video_effect_bypass(int bypass)
{
	if (is_aml_tvmode()) {
		/*bypass vpp pq only for IDK cert or debug mode*/
		if (!debug_bypass_vpp_pq &&
		    !(dolby_vision_flags & FLAG_CERTIFICAION))
			return;
	}
	if (debug_bypass_vpp_pq == 1) {
		if ((dolby_vision_flags & FLAG_CERTIFICAION) ||
		    bypass_all_vpp_pq)
			dv_pq_ctl(DV_PQ_CERT);
		else if (is_aml_tvmode())
			dv_pq_ctl(DV_PQ_TV_BYPASS);
		else
			dv_pq_ctl(DV_PQ_STB_BYPASS);
		return;
	} else if (debug_bypass_vpp_pq == 2) {
		dv_pq_ctl(DV_PQ_REC);
		return;
	} else if (debug_bypass_vpp_pq == 3) {
		return;
	}

	if (bypass) {
		if (!is_video_effect_bypass) {
			if (is_aml_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				viu_eotf_ctrl_backup =
					VSYNC_RD_DV_REG(VIU_EOTF_CTL);
				xvycc_lut_ctrl_backup =
					VSYNC_RD_DV_REG(XVYCC_LUT_CTL);
				inv_lut_ctrl_backup =
					VSYNC_RD_DV_REG(XVYCC_INV_LUT_CTL);
				vpp_vadj_backup =
					VSYNC_RD_DV_REG(VPP_VADJ_CTRL);
				xvycc_vd1_rgb_ctrst_backup =
					VSYNC_RD_DV_REG(XVYCC_VD1_RGB_CTRST);
				vpp_ve_enable_ctrl_backup =
					VSYNC_RD_DV_REG(VPP_VE_ENABLE_CTRL);
				vpp_gainoff_backup =
					VSYNC_RD_DV_REG(VPP_GAINOFF_CTRL0);
#endif
			}
		}
		/*todo, there is a bug in amvecm, need to call dv_pq_ctl every vsync*/
		if (1/*!is_video_effect_bypass*/) {
			if (is_aml_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				VSYNC_WR_DV_REG(VIU_EOTF_CTL, 0);
				VSYNC_WR_DV_REG(XVYCC_LUT_CTL, 0);
				VSYNC_WR_DV_REG(XVYCC_INV_LUT_CTL, 0);
				VSYNC_WR_DV_REG(VPP_VADJ_CTRL, 0);
				VSYNC_WR_DV_REG(XVYCC_VD1_RGB_CTRST, 0);
				VSYNC_WR_DV_REG(VPP_VE_ENABLE_CTRL, 0);
				VSYNC_WR_DV_REG(VPP_GAINOFF_CTRL0, 0);
#endif
			} else {
				if ((dolby_vision_flags & FLAG_CERTIFICAION) ||
				    bypass_all_vpp_pq)
					dv_pq_ctl(DV_PQ_CERT);
				else if (is_aml_tvmode())
					dv_pq_ctl(DV_PQ_TV_BYPASS);
				else
					dv_pq_ctl(DV_PQ_STB_BYPASS);
			}
		}
		is_video_effect_bypass = true;
	} else if (is_video_effect_bypass) {
		if (is_aml_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			VSYNC_WR_DV_REG
				(VIU_EOTF_CTL,
				 viu_eotf_ctrl_backup);
			VSYNC_WR_DV_REG
				(XVYCC_LUT_CTL,
				 xvycc_lut_ctrl_backup);
			VSYNC_WR_DV_REG
				(XVYCC_INV_LUT_CTL,
				inv_lut_ctrl_backup);
			VSYNC_WR_DV_REG
				(VPP_VADJ_CTRL,
				 vpp_vadj_backup);
			VSYNC_WR_DV_REG
				(XVYCC_VD1_RGB_CTRST,
				 xvycc_vd1_rgb_ctrst_backup);
			VSYNC_WR_DV_REG
				(VPP_VE_ENABLE_CTRL,
				vpp_ve_enable_ctrl_backup);
			VSYNC_WR_DV_REG
				(VPP_GAINOFF_CTRL0,
				vpp_gainoff_backup);
#endif
		} else {
			dv_pq_ctl(DV_PQ_REC);
		}
		is_video_effect_bypass = false;
	}
}

void set_debug_bypass_vpp_pq(int val)
{
	debug_bypass_vpp_pq = val;
}

void set_bypass_all_vpp_pq(int flag)
{
	if (flag == 0)
		bypass_all_vpp_pq = 0;
	else
		bypass_all_vpp_pq = 1;
}

void set_force_reset_core2(bool flag)
{
	force_reset_core2 = flag;
}

/*flag bit0: bypass from preblend to VADJ1, skip sr/pps/cm2*/
/*flag bit1: skip OE/EO*/
/*flag bit2: bypass from dolby3 to vkeystone, skip vajd2/post/mtx/gainoff*/
static void bypass_pps_sr_gamma_gainoff(int flag)
{
	pr_dv_dbg("%s: %d\n", __func__, flag);

	if (flag & 1) {
		if (is_aml_t3() || is_aml_t5w()) {
			/*from t3, 1d93 bit0 change to 1d26 bit8*/
			VSYNC_WR_DV_REG_BITS(VPP_MISC, 1, 8, 1);
			force_bypass_from_prebld_to_vadj1 = true;
		} else {
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 1, 0, 1);
		}
	}
	if (flag & 2)
		VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 1, 1, 1);
	if (flag & 4)
		VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 1, 2, 1);
}

static void osd_path_enable(int on)
{
	u32 i = 0;
	u32 addr_port;
	u32 data_port;
	struct hdr_osd_lut_s *lut = &hdr_osd_reg.lut_val;

	if (!on) {
		enable_osd_path(0, 0);
		VSYNC_WR_DV_REG(VIU_OSD1_EOTF_CTL, 0);
		VSYNC_WR_DV_REG(VIU_OSD1_OETF_CTL, 0);
	} else {
		enable_osd_path(1, -1);
		if ((hdr_osd_reg.viu_osd1_eotf_ctl & 0x80000000) != 0) {
			addr_port = VIU_OSD1_EOTF_LUT_ADDR_PORT;
			data_port = VIU_OSD1_EOTF_LUT_DATA_PORT;
			VSYNC_WR_DV_REG
				(addr_port, 0);
			for (i = 0; i < 16; i++)
				VSYNC_WR_DV_REG
					(data_port,
					 lut->r_map[i * 2]
					 | (lut->r_map[i * 2 + 1] << 16));
			VSYNC_WR_DV_REG
				(data_port,
				 lut->r_map[EOTF_LUT_SIZE - 1]
				 | (lut->g_map[0] << 16));
			for (i = 0; i < 16; i++)
				VSYNC_WR_DV_REG
					(data_port,
					 lut->g_map[i * 2 + 1]
					 | (lut->b_map[i * 2 + 2] << 16));
			for (i = 0; i < 16; i++)
				VSYNC_WR_DV_REG
					(data_port,
					 lut->b_map[i * 2]
					 | (lut->b_map[i * 2 + 1] << 16));
			VSYNC_WR_DV_REG
				(data_port, lut->b_map[EOTF_LUT_SIZE - 1]);

			/* load eotf matrix */
			VSYNC_WR_DV_REG
				(VIU_OSD1_EOTF_COEF00_01,
				 hdr_osd_reg.viu_osd1_eotf_coef00_01);
			VSYNC_WR_DV_REG
				(VIU_OSD1_EOTF_COEF02_10,
				 hdr_osd_reg.viu_osd1_eotf_coef02_10);
			VSYNC_WR_DV_REG
				(VIU_OSD1_EOTF_COEF11_12,
				 hdr_osd_reg.viu_osd1_eotf_coef11_12);
			VSYNC_WR_DV_REG
				(VIU_OSD1_EOTF_COEF20_21,
				 hdr_osd_reg.viu_osd1_eotf_coef20_21);
			VSYNC_WR_DV_REG
				(VIU_OSD1_EOTF_COEF22_RS,
				 hdr_osd_reg.viu_osd1_eotf_coef22_rs);
			VSYNC_WR_DV_REG
				(VIU_OSD1_EOTF_CTL,
				 hdr_osd_reg.viu_osd1_eotf_ctl);
		}
		/* restore oetf lut */
		if ((hdr_osd_reg.viu_osd1_oetf_ctl & 0xe0000000) != 0) {
			addr_port = VIU_OSD1_OETF_LUT_ADDR_PORT;
			data_port = VIU_OSD1_OETF_LUT_DATA_PORT;
			for (i = 0; i < 20; i++) {
				VSYNC_WR_DV_REG
					(addr_port, i);
				VSYNC_WR_DV_REG
					(data_port,
					 lut->or_map[i * 2]
					 | (lut->or_map[i * 2 + 1] << 16));
			}
			VSYNC_WR_DV_REG
				(addr_port, 20);
			VSYNC_WR_DV_REG
				(data_port,
				 lut->or_map[41 - 1]
				 | (lut->og_map[0] << 16));
			for (i = 0; i < 20; i++) {
				VSYNC_WR_DV_REG
					(addr_port, 21 + i);
				VSYNC_WR_DV_REG
					(data_port,
					 lut->og_map[i * 2 + 1]
					 | (lut->og_map[i * 2 + 2] << 16));
			}
			for (i = 0; i < 20; i++) {
				VSYNC_WR_DV_REG
					(addr_port, 41 + i);
				VSYNC_WR_DV_REG
					(data_port,
					 lut->ob_map[i * 2]
					 | (lut->ob_map[i * 2 + 1] << 16));
			}
			VSYNC_WR_DV_REG
				(addr_port, 61);
			VSYNC_WR_DV_REG
				(data_port,
				 lut->ob_map[41 - 1]);
			VSYNC_WR_DV_REG
				(VIU_OSD1_OETF_CTL,
				 hdr_osd_reg.viu_osd1_oetf_ctl);
		}
	}
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_PRE_OFFSET0_1,
		 hdr_osd_reg.viu_osd1_matrix_pre_offset0_1);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_PRE_OFFSET2,
		 hdr_osd_reg.viu_osd1_matrix_pre_offset2);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF00_01,
		 hdr_osd_reg.viu_osd1_matrix_coef00_01);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF02_10,
		 hdr_osd_reg.viu_osd1_matrix_coef02_10);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF11_12,
		 hdr_osd_reg.viu_osd1_matrix_coef11_12);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF20_21,
		 hdr_osd_reg.viu_osd1_matrix_coef20_21);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF22_30,
		 hdr_osd_reg.viu_osd1_matrix_coef22_30);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF31_32,
		 hdr_osd_reg.viu_osd1_matrix_coef31_32);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COEF40_41,
		 hdr_osd_reg.viu_osd1_matrix_coef40_41);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_COLMOD_COEF42,
		 hdr_osd_reg.viu_osd1_matrix_colmod_coef42);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_OFFSET0_1,
		 hdr_osd_reg.viu_osd1_matrix_offset0_1);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_OFFSET2,
		 hdr_osd_reg.viu_osd1_matrix_offset2);
	VSYNC_WR_DV_REG
		(VIU_OSD1_MATRIX_CTRL,
		 hdr_osd_reg.viu_osd1_matrix_ctrl);
}

static void reset_dovi_setting(void)
{
	int i;

	if (is_aml_tvmode()) {
		if (tv_dovi_setting)
			tv_dovi_setting->src_format = FORMAT_SDR;
	} else if (multi_dv_mode) {
		memset(&m_dovi_setting, 0, sizeof(m_dovi_setting));
		for (i = 0; i < NUM_IPCORE1; i++)
			m_dovi_setting.input[i].src_format = FORMAT_SDR;
	}
}

static u32 amdv_ctrl_backup = 0x22000;
static u32 viu_misc_ctrl_backup;
static u32 vpp_matrix_backup;
static u32 vpp_dummy1_backup;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static u32 vpp_data_conv_para0_backup;
static u32 vpp_data_conv_para1_backup;
#endif

void enable_amdv_v1(int enable)
{
	u32 core_flag = 0;
	int gd_en = 0;
	u32 diagnostic_enable = dovi_setting.diagnostic_enable;
	bool dovi_ll_enable = dovi_setting.dovi_ll_enable;

	if (debug_dolby & 8)
		pr_dv_dbg("enable %d, dv on %d, mode %d %d\n",
			  enable, dolby_vision_on, dolby_vision_mode,
			  get_amdv_target_mode());
	if (enable) {
		if (!dolby_vision_on) {
			set_amdv_wait_on();
			amdv_ctrl_backup =
				VSYNC_RD_DV_REG(VPP_AMDV_CTRL);
			viu_misc_ctrl_backup =
				VSYNC_RD_DV_REG(VIU_MISC_CTRL1);
			vpp_matrix_backup =
				VSYNC_RD_DV_REG(VPP_MATRIX_CTRL);
			vpp_dummy1_backup =
				VSYNC_RD_DV_REG(VPP_DUMMY_DATA1);
			if (is_aml_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				vpp_data_conv_para0_backup =
					VSYNC_RD_DV_REG(VPP_DAT_CONV_PARA0);
				vpp_data_conv_para1_backup =
					VSYNC_RD_DV_REG(VPP_DAT_CONV_PARA1);
#endif
			}
			if (is_aml_tvmode()) {
				update_dma_buf();
				if (!amdv_core1_on)
					set_frame_count(0);
				if (is_aml_txlx_tvmode()) {
					if ((amdv_mask & 1) &&
					    amdv_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(VIU_MISC_CTRL1,
							 0,
							 16, 1); /* core1 */
						amdv_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(VIU_MISC_CTRL1,
							 1,
							 16, 1); /* core1 */
						amdv_core1_on = false;
					}
				} else if (is_aml_tm2_tvmode()) {
					/* common flow should */
					/* stop hdr core before */
					/* start dv core */
					if (dolby_vision_flags &
					FLAG_CERTIFICAION)
						hdr_vd1_off(VPP_TOP0);
					if ((amdv_mask & 1) &&
					    amdv_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1, 8, 2);
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1, 10, 2);
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1, 24, 2);
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0, 16, 1);
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0, 20, 1);
						VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 tv_dovi_setting->el_flag ?
						 0 : 2, 0, 2);
						amdv_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 3, 0, 2);
						VSYNC_WR_DV_REG
							(AMDV_TV_CLKGATE_CTRL,
							0x55555555);
						dv_mem_power_off(VPU_DOLBY0);
						amdv_core1_on = false;
					}
				} else if (is_aml_t7_tvmode()) {
					/* common flow should */
					/* stop hdr core before */
					/* start dv core */
					if (dolby_vision_flags &
						FLAG_CERTIFICAION)
						hdr_vd1_off(VPP_TOP0);
					if ((amdv_mask & 1) &&
						amdv_setting_video_flag) {
						/*enable tv core*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							0, 4, 1);
						/*vd1 to tvcore*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL1,
							 1, 0, 3);
						/*tvcore bl in sel vd1*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL2,
							 0, 6, 2);
						/*tvcore el in sel null*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL2,
							 3, 8, 2);
						/*tvcore bl to vd1*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL2,
							 0, 20, 2);
						/* vd1 from tvcore*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL1,
							 1, 12, 3);
						amdv_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							1, 4, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD2_DSC_CTRL,
							 1, 4, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD3_DSC_CTRL,
							 1, 4, 1);
						VSYNC_WR_DV_REG
							(AMDV_TV_CLKGATE_CTRL,
							0x55555555);
						dv_mem_power_off(VPU_DOLBY0);
						amdv_core1_on = false;
					}
				} else  if (is_aml_t3_tvmode() ||
					    is_aml_t5w()) {
					/* common flow should */
					/* stop hdr core before */
					/* start dv core */
					if (dolby_vision_flags &
						FLAG_CERTIFICAION)
						hdr_vd1_off(VPP_TOP0);
					if ((amdv_mask & 1) &&
						amdv_setting_video_flag) {
						/*enable tv core*/
						/* T3 is not the same as T7 */
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							0, 16, 1);
						/*vd1 to tvcore*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL1,
							 1, 0, 3);
						/*tvcore bl in sel vd1*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL2,
							 0, 6, 2);
						/*tvcore el in sel null*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL2,
							 3, 8, 2);
						/*tvcore bl to vd1*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL2,
							 0, 20, 2);
						/* vd1 from tvcore*/
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_SWAP_CTRL1,
							 1, 12, 3);
						amdv_core1_on = true;
					} else {
						/* T3 is not the same as T7 */
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							1, 16, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD2_DSC_CTRL,
							 1, 4, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD3_DSC_CTRL,
							 1, 4, 1);
						VSYNC_WR_DV_REG
							(AMDV_TV_CLKGATE_CTRL,
							0x55555555);
						dv_mem_power_off(VPU_DOLBY0);
						amdv_core1_on = false;
					}
				}
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM, EO/OE */
					bypass_pps_sr_gamma_gainoff(3);
					/* bypass all video effect */
					video_effect_bypass(1);
					if (is_aml_txlx_tvmode()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
						/* 12 bit unsigned to sign*/
						/*   before vadj1 */
						/* 12 bit sign to unsign*/
						/*   before post blend */
						VSYNC_WR_DV_REG
							(VPP_DAT_CONV_PARA0, 0x08000800);
						/* 12->10 before vadj2*/
						/*   10->12 after gainoff */
						VSYNC_WR_DV_REG
							(VPP_DAT_CONV_PARA1, 0x20002000);
#endif
					}
					WRITE_VPP_DV_REG(AMDV_TV_DIAG_CTRL,
							 0xb);
				} else {
					/* bypass all video effect */
					if (dolby_vision_flags &
					    FLAG_BYPASS_VPP)
						video_effect_bypass(1);
					if (is_aml_txlx_tvmode()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
						/* 12->10 before vadj1*/
						/*   10->12 before post blend */
						VSYNC_WR_DV_REG
							(VPP_DAT_CONV_PARA0,
							 0x20002000);
					/* 12->10 before vadj2*/
					/*   10->12 after gainoff */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1,
						 0x20002000);
#endif
					}
				}
				VSYNC_WR_DV_REG
					(VPP_DUMMY_DATA1,
					 0x80200);
				/* osd rgb to yuv, vpp out yuv to rgb */
				VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0x81);
				pr_info("DV TV core turn on\n");
			} else if (is_aml_txlx_stbmode()) {
				update_dma_buf();
				osd_bypass(1);
				if (amdv_mask & 4)
					VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 1, 3, 1);   /* core3 enable */
				if ((amdv_mask & 1) &&
				    amdv_setting_video_flag) {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 0,
						 16, 1); /* core1 */
					amdv_core1_on = true;
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 1,
						 16, 1); /* core1 */
					amdv_core1_on = false;
				}
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 (((amdv_mask & 1) &&
					 amdv_setting_video_flag)
					 ? 0 : 1),
					 16, 1); /* core1 */
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 ((amdv_mask & 2) ? 0 : 1),
					 18, 1); /* core2 */
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM*/
					/*   bypass EO/OE*/
					/*   bypass vadj2/mtx/gainoff */
					bypass_pps_sr_gamma_gainoff(7);
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12 bit unsigned to sign*/
					/*   before vadj1 */
					/* 12 bit sign to unsign*/
					/*   before post blend */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA0,
						 0x08000800);
					/* 12->10 before vadj2*/
					/*   10->12 after gainoff */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1,
						 0x20002000);
				} else {
					/* bypass all video effect */
					if (dolby_vision_flags &
					    FLAG_BYPASS_VPP)
						video_effect_bypass(1);
					/* 12->10 before vadj1*/
					/*   10->12 before post blend */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA0,
						 0x20002000);
					/* 12->10 before vadj2*/
					/*   10->12 after gainoff */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1,
						 0x20002000);
				}
				VSYNC_WR_DV_REG(VPP_DUMMY_DATA1, 0x80200);
				if (is_aml_tvmode())
					VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 1);
				else
					VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);

				if ((dolby_vision_mode ==
				     AMDV_OUTPUT_MODE_IPT_TUNNEL ||
				     dolby_vision_mode ==
				     AMDV_OUTPUT_MODE_IPT) &&
				     diagnostic_enable == 0 &&
				     dovi_ll_enable) {
					u32 *reg =
						(u32 *)&dovi_setting.dm_reg3;
					/* input u12 -0x800 to s12 */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1, 0x8000800);
					/* bypass vadj */
					VSYNC_WR_DV_REG
						(VPP_VADJ_CTRL, 0);
					/* bypass gainoff */
					VSYNC_WR_DV_REG
						(VPP_GAINOFF_CTRL0, 0);
					/* enable wm tp vks*/
					/* bypass gainoff to vks */
					VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL, 1, 1, 2);
					enable_rgb_to_yuv_matrix_for_dvll
						(1, &reg[18], 12);
				} else {
					enable_rgb_to_yuv_matrix_for_dvll
						(0, NULL, 12);
				}
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
				pr_info("DV STB cores turn on\n");
			} else if (is_aml_g12() ||
				   is_aml_tm2_stbmode() ||
				   is_aml_t7_stbmode() ||
				   is_aml_sc2() ||
				   is_aml_s4d()) {
				hdr_osd_off(VPP_TOP0);
				hdr_vd1_off(VPP_TOP0);
				set_hdr_module_status(VD1_PATH,
					HDR_MODULE_BYPASS);
				if (!amdv_core1_on)
					set_frame_count(0);
				if (is_aml_t7_stbmode()) {
					if (amdv_mask & 4)
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 1, 3, 1);   /* core3 enable */
					else
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 0, 3, 1);   /* bypass core3  */

					if ((amdv_mask & 2) && (core2_sel & 1)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD_TOP_CTRL,
							 0,
							 14, 1);/*core2a enable*/
					}
					if ((amdv_mask & 2) && (core2_sel & 2)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD1_TOP_CTRL,
							 0,
							 19, 1);/*core2c enable*/
					}
					if (!(amdv_mask & 2)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD_TOP_CTRL,
							 1,
							 14, 1);/*core2a bypass*/
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD1_TOP_CTRL,
							 1,
							 19, 1);/*core2c bypass*/
					}
				} else {
					if (amdv_mask & 4)
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 1, 3, 1);   /* core3 enable */
					else
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 0, 3, 1);   /* bypass core3  */

					if (amdv_mask & 2)
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0,
							 2, 1);/*core2 enable*/
					else
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1,
							 2, 1);/*core2 bypass*/
				}
				if (is_aml_g12() || is_aml_sc2() || is_aml_s4d()) {
					if ((amdv_mask & 1) &&
					    amdv_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0,
							 0, 1); /* core1 on*/
						amdv_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1,
							 0, 1); /* core1 off*/
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						amdv_core1_on = false;
					}
				} else if (is_aml_tm2_stbmode()) {
					if (is_aml_stb_hdmimode())
						core_flag = 1;
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						core_flag, 8, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						core_flag, 10, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						core_flag, 24, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						0, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						0, 17, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						0, 20, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						0, 21, 1);
					if ((amdv_mask & 1) &&
						amdv_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							0,
							0, 2); /* core1 on */
						amdv_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 3,
							 0, 2); /* core1 off*/
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						amdv_core1_on = false;
					}
					if (core_flag &&
					amdv_core1_on) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							3,
							0, 2); /* core1 off */
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					}
				} else if (is_aml_t7_stbmode()) {
					if (is_aml_stb_hdmimode())
						core_flag = 1;
					/* AMDV_PATH_SWAP_CTRL1 todo*/
					if ((amdv_mask & 1) &&
					    amdv_setting_video_flag) {
						/*vd1 core1 on*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 0, 4, 1);
						amdv_core1_on = true;
					} else {
						/*vd1 core1 off*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL,
								0x55555455);
						amdv_core1_on = false;
					}
					if (core_flag && amdv_core1_on) {
						/*vd1 core1 off*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL,
								0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					}
				}
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM*/
					/*   bypass EO/OE*/
					/*   bypass vadj2/mtx/gainoff */
					bypass_pps_sr_gamma_gainoff(7);
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12 bit unsigned to sign*/
					/*   before vadj1 */
					/* 12 bit sign to unsign*/
					/*   before post blend */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA0,
						 0x08000800);
					/* 12->10 before vadj2*/
					/*   10->12 after gainoff */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1,
						 0x20002000);
				} else {
					/* bypass all video effect */
					if (dolby_vision_flags &
					    FLAG_BYPASS_VPP)
						video_effect_bypass(1);
					/* 12->10 before vadj1*/
					/*   10->12 before post blend */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA0,
						 0x20002000);
					/* 12->10 before vadj2*/
					/*   10->12 after gainoff */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1,
						 0x20002000);
				}
				VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_DV_REG(VPP_DUMMY_DATA1, 0x80200);
				if ((dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_IPT_TUNNEL ||
				    dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_IPT) &&
				    dovi_setting.diagnostic_enable == 0 &&
				    dovi_setting.dovi_ll_enable) {
					u32 *reg =
						(u32 *)&dovi_setting.dm_reg3;
					/* input u12 -0x800 to s12 */
					VSYNC_WR_DV_REG
						(VPP_DAT_CONV_PARA1, 0x8000800);
					/* bypass vadj */
					VSYNC_WR_DV_REG
						(VPP_VADJ_CTRL, 0);
					/* bypass gainoff */
					VSYNC_WR_DV_REG
						(VPP_GAINOFF_CTRL0, 0);
					/* enable wm tp vks*/
					/* bypass gainoff to vks */
					VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL, 1, 1, 2);
					enable_rgb_to_yuv_matrix_for_dvll
						(1, &reg[18],
						 (dv_ll_output_mode >> 8)
						 & 0xff);
				} else {
					enable_rgb_to_yuv_matrix_for_dvll
						(0, NULL, 12);
				}
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
				pr_dv_dbg
					("DV G12a turn on%s\n",
					amdv_core1_on ?
					", core1 on" : "");
				if (!amdv_core1_on)
					set_frame_count(0);
			} else {
				VSYNC_WR_DV_REG
					(VPP_AMDV_CTRL,
					 /* cm_datx4_mode */
					 (0x0 << 21) |
					 /* reg_front_cti_bit_mode */
					 (0x0 << 20) |
					 /* vpp_clip_ext_mode 19:17 */
					 (0x0 << 17) |
					 /* vpp_dolby2_en core3 */
					 (((amdv_mask & 4) ?
					 (1 << 0) : (0 << 0)) << 16) |
					 /* mat_xvy_dat_mode */
					 (0x0 << 15) |
					 /* vpp_ve_din_mode */
					 (0x1 << 14) |
					 /* mat_vd2_dat_mode 13:12 */
					 (0x1 << 12) |
					 /* vpp_dpath_sel 10:8 */
					 (0x3 << 8) |
					 /* vpp_uns2s_mode 7:0 */
					 0x1f);
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* 23-20 ext mode */
					 (0 << 2) |
					 /* 19 osd2 enable */
					 ((dolby_vision_flags
					 & FLAG_CERTIFICAION)
					 ? (0 << 1) : (1 << 1)) |
					 /* 18 core2 bypass */
					 ((amdv_mask & 2) ?
					 0 : 1),
					 18, 6);
				if ((amdv_mask & 1) &&
				    amdv_setting_video_flag) {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 0,
						 16, 1); /* core1 */
					amdv_core1_on = true;
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 1,
						 16, 1); /* core1 */
					amdv_core1_on = false;
				}
				/* bypass all video effect */
				if ((dolby_vision_flags & FLAG_BYPASS_VPP) ||
				    (dolby_vision_flags & FLAG_CERTIFICAION))
					video_effect_bypass(1);
				VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_DV_REG(VPP_DUMMY_DATA1, 0x20000000);
				if ((dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_IPT_TUNNEL ||
				    dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_IPT) &&
				    dovi_setting.diagnostic_enable == 0 &&
				    dovi_setting.dovi_ll_enable) {
					u32 *reg =
						(u32 *)&dovi_setting.dm_reg3;
					VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						3, 6, 2); /* post matrix */
					enable_rgb_to_yuv_matrix_for_dvll
						(1, &reg[18], 12);
				} else {
					enable_rgb_to_yuv_matrix_for_dvll
						(0, NULL, 12);
				}
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
				/* disable osd effect and shadow mode */
				osd_path_enable(0);
				pr_dv_dbg("DV turn on%s\n",
					     amdv_core1_on ?
					     ", core1 on" : "");
			}
			amdv_core1_on_cnt = 0;
		} else {
			if (!amdv_core1_on &&
				(amdv_mask & 1) &&
				amdv_setting_video_flag) {
				if (is_aml_g12() ||
				    is_aml_tm2_stbmode() ||
				    is_aml_sc2() ||
				    is_aml_s4d()) {
					/* enable core1 with el */
					if (dovi_setting.el_flag)
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							0, 0, 2);
					else /* enable core1 without el */
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							0, 0, 1);
					if (is_aml_stb_hdmimode()) {
						/* core1 off */
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							3, 0, 2);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					} else {
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_aml_tm2_tvmode()) {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 8, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 10, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 24, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 20, 1);
					/* enable core1 */
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 /* enable core1 */
						 tv_dovi_setting->el_flag ?
						 0 : 2, 0, 2);
				} else if (is_aml_t7_stbmode()) {
					/* vd1 to core1a*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 0, 0, 3);
					/*core1a bl in sel vd1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 0, 2, 2);
					/*core1a el in sel null*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 3, 4, 2);
					/*core1a out to vd1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 0, 18, 2);
					/* vd1 from core1a*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 0, 12, 3);
					/*enable core1a*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD1_DSC_CTRL,
						 0, 4, 1);
					if (is_aml_stb_hdmimode()) {
						/* core1 off */
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					} else {
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_aml_t7_tvmode() ||
				is_aml_t3_tvmode() || is_aml_t5w()) {
					/* enable core1 */
					if (is_aml_t7_tvmode())
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 0, 4, 1);
					else
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							 0, 16, 1);
					/*vd1 to tvcore*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 1, 0, 3);
					/*tvcore bl in sel vd1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 0, 6, 2);
					/*tvcore el in sel null*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 3, 8, 2);
					/*tvcore bl to vd1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 0, 20, 2);
					/* vd1 from tvcore*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 1, 12, 3);
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 0,
						 16, 1); /* core1 */
				}
				amdv_core1_on = true;
				amdv_core1_on_cnt = 0;
				pr_dv_dbg("DV core1 turn on\n");
			} else if (amdv_core1_on &&
					   (!(amdv_mask & 1) ||
					    !amdv_setting_video_flag)) {
				if (is_aml_g12() ||
				    is_aml_tm2_stbmode() ||
				    is_aml_t7_stbmode() ||
				    is_aml_sc2() ||
				    is_aml_s4d()) {
					if (is_aml_t7_stbmode()) {
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 /* disable vd1 dv */
							 1, 4, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD2_DSC_CTRL,
							 /* disable vd2 dv */
							 1, 4, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD3_DSC_CTRL,
							 /* disable vd3 dv */
							 1, 4, 1);
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							3, 0, 2);
					}
					dv_mem_power_off(VPU_DOLBY1A);
					dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
					VSYNC_WR_DV_REG
						(AMDV_CORE1A_CLKGATE_CTRL,
						0x55555455);
					if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
						/* core1b */
						dv_mem_power_off(VPU_DOLBY1B);
						VSYNC_WR_DV_REG
							(AMDV_CORE1B_CLKGATE_CTRL,
							0x55555455);
						/* coretv */
						VSYNC_WR_DV_REG_BITS
							(AMDV_TV_SWAP_CTRL7,
							0xf, 4, 4);
						VSYNC_WR_DV_REG
							(AMDV_TV_CLKGATE_CTRL,
							0x55555455);
						dv_mem_power_off(VPU_DOLBY0);
						/* hdr core */
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_aml_tm2_tvmode()) {
					/* disable coretv */
					VSYNC_WR_DV_REG_BITS(AMDV_PATH_CTRL,
							     3, 0, 2);
					VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
							0x01000042);
					VSYNC_WR_DV_REG_BITS(AMDV_TV_SWAP_CTRL7,
							     0xf, 4, 4);
					VSYNC_WR_DV_REG(AMDV_TV_CLKGATE_CTRL,
							0x55555455);
					dv_mem_power_off(VPU_DOLBY0);
				} else if (is_aml_t7_tvmode() ||
					is_aml_t3_tvmode() ||
					is_aml_t5w()) {
					/* disable coretv */
					if (is_aml_t7_tvmode())
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 /* disable vd1 dv */
							 1, 4, 1);
					else
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							 /* disable vd1 dv */
							 1, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 /* disable vd2 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 /* disable vd3 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
						0x01000042);
					VSYNC_WR_DV_REG_BITS
						(AMDV_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(AMDV_TV_CLKGATE_CTRL,
						0x55555455);
					dv_mem_power_off(VPU_DOLBY0);
					if (is_aml_t3() || is_aml_t5w())
						vpu_module_clk_disable
							(0, DV_TVCORE, 0);
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 1,
						 16, 1); /* core1 */
				}
				amdv_core1_on = false;
				amdv_core1_on_cnt = 0;
				set_frame_count(0);
				set_vf_crc_valid(0);
				pr_dv_dbg("DV core1 turn off\n");
			}
		}
		dolby_vision_on = true;
		clear_dolby_vision_wait();
		set_vsync_count(0);
	} else {
		if (dolby_vision_on) {
			if (is_aml_tvmode()) {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* vd2 connect to vpp */
					 (1 << 1) |
					 /* 16 core1 bl bypass */
					 (1 << 0),
					 16, 2);
				if (is_aml_tm2_tvmode()) {
					VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL, 3, 0, 2);
				} else if (is_aml_t7_tvmode() ||
					   is_aml_t3_tvmode() ||
					   is_aml_t5w()) {
					if (is_aml_t7_tvmode())
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 /* disable vd1 dv */
							 1, 4, 1);
					else
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							 /* disable vd1 dv */
							 1, 16, 1);

					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 /* disable vd2 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 /* disable vd3 dv */
						 1, 4, 1);
				}
				if (is_aml_tm2_tvmode() || is_aml_t7_tvmode() ||
				    is_aml_t3_tvmode() ||
				    is_aml_t5w()) {
					VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
						0x01000042);
					VSYNC_WR_DV_REG_BITS
						(AMDV_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(AMDV_TV_CLKGATE_CTRL,
						0x55555555);
					dv_mem_power_off(VPU_DOLBY0);
					if (is_aml_t3() || is_aml_t5w())
						vpu_module_clk_disable
							(0, DV_TVCORE, 0);
				}
				if (p_funcs_tv) /* destroy ctx */
					p_funcs_tv->tv_control_path
						(FORMAT_INVALID, 0,
						NULL, 0,
						NULL, 0,
						0,	0,
						SIGNAL_RANGE_SMPTE,
						NULL, NULL,
						0,
						NULL,
						NULL,
						NULL, 0,
						NULL,
						NULL);
				pr_dv_dbg("DV TV core turn off\n");
				if (tv_dovi_setting)
					tv_dovi_setting->src_format =
					FORMAT_SDR;
			} else if (is_aml_txlx_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_DV_REG_BITS
					(VPP_AMDV_CTRL,
					 0, 3, 1);/* core3 disable */
				osd_bypass(0);
				if (p_funcs_stb) /* destroy ctx */
					p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 NULL, 0,
						 NULL, 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 NULL,
						 NULL);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
				stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
				stb_core2_const_flag = false;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				dovi_setting.src_format = FORMAT_SDR;
				pr_dv_dbg("DV STB cores turn off\n");
			} else if (is_aml_g12() ||
				   is_aml_tm2_stbmode() ||
				   is_aml_t7_stbmode() ||
				   is_aml_sc2() ||
				   is_aml_s4d()) {
				if (is_aml_t7_stbmode()) {
					VSYNC_WR_DV_REG_BITS
						(VPP_VD1_DSC_CTRL,
						 /* disable vd1 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 /* disable vd2 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 /* disable vd3 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(MALI_AFBCD_TOP_CTRL,
						 /* disable core2a */
						 1, 14, 1);
					VSYNC_WR_DV_REG_BITS
						(MALI_AFBCD1_TOP_CTRL,
						 /* disable core2c */
						 1, 19, 1);
				} else {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						(1 << 2) |	/* core2 bypass */
						(1 << 1) |	/* vd2 connect to vpp */
						(1 << 0),	/* core1 bl bypass */
						0, 3);
				}
				VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL,
						     0, 3, 1);   /* core3 disable */

				/* core1a */
				VSYNC_WR_DV_REG
					(AMDV_CORE1A_CLKGATE_CTRL,
					0x55555455);
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				/* core2 */
				VSYNC_WR_DV_REG
					(AMDV_CORE2A_CLKGATE_CTRL,
					0x55555555);
				dv_mem_power_off(VPU_DOLBY2);
				/* core3 */
				VSYNC_WR_DV_REG
					(AMDV_CORE3_CLKGATE_CTRL,
					0x55555555);
				dv_mem_power_off(VPU_DOLBY_CORE3);
				if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
					/* core1b */
					VSYNC_WR_DV_REG
						(AMDV_CORE1B_CLKGATE_CTRL,
						0x55555555);
					dv_mem_power_off(VPU_DOLBY1B);
					/* tv core */
					VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
						0x01000042);
					VSYNC_WR_DV_REG_BITS
						(AMDV_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(AMDV_TV_CLKGATE_CTRL,
						0x55555555);
					/* hdr core */
					hdr_vd1_off(VPP_TOP0);
					dv_mem_power_off(VPU_DOLBY0);
				}
				if (p_funcs_stb) {/* destroy ctx */
					p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 NULL, 0,
						 NULL, 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 NULL,
						 NULL);
				}
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
				stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
				stb_core2_const_flag = false;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				dovi_setting.src_format = FORMAT_SDR;
				pr_dv_dbg("DV G12a/TM2 turn off\n");
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL,
						     0, 16, 1);/*core3 disable*/
				/* enable osd effect and*/
				/*	use default shadow mode */
				osd_path_enable(1);
				if (p_funcs_stb) /* destroy ctx */
					p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 NULL, 0,
						 NULL, 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 NULL,
						 NULL);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
				stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
				stb_core2_const_flag = false;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				dovi_setting.src_format = FORMAT_SDR;
				pr_dv_dbg("DV turn off\n");
			}
			VSYNC_WR_DV_REG(VIU_SW_RESET, 3 << 9);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
			if (is_aml_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0,
						vpp_data_conv_para0_backup);
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA1,
						vpp_data_conv_para1_backup);
				VSYNC_WR_DV_REG(AMDV_TV_CLKGATE_CTRL,
						0x2414);
				VSYNC_WR_DV_REG(AMDV_CORE2A_CLKGATE_CTRL,
						0x4);
				VSYNC_WR_DV_REG(AMDV_CORE3_CLKGATE_CTRL,
						0x414);
				VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
						0x01000042);
#endif
			}
			if (is_aml_box() || is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
				VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL,
						0x55555555);
				VSYNC_WR_DV_REG(AMDV_CORE2A_CLKGATE_CTRL,
						0x55555555);
				VSYNC_WR_DV_REG(AMDV_CORE3_CLKGATE_CTRL,
						0x55555555);
			}
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1, 0);
			video_effect_bypass(0);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_aml_gxm())
				VSYNC_WR_DV_REG(VPP_AMDV_CTRL,
						amdv_ctrl_backup);
#endif
			/* always vd2 to vpp and bypass core 1 */
			viu_misc_ctrl_backup |=
				(VSYNC_RD_DV_REG(VIU_MISC_CTRL1) & 2);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_aml_gxm()) {
				if ((VSYNC_RD_DV_REG(VIU_MISC_CTRL1) &
					(0xff << 8)) != 0) {
					/*sometimes misc_ctrl_backup*/
					/*didn't record afbc bits, need */
					/*update afbc bit8~bit15 to 0x90*/
					viu_misc_ctrl_backup |=
						((viu_misc_ctrl_backup &
						 0xFFFF90FF) | 0x9000);
				}
			}
#endif
			VSYNC_WR_DV_REG(VIU_MISC_CTRL1,
					viu_misc_ctrl_backup | (3 << 16));
			VSYNC_WR_DV_REG(VPP_MATRIX_CTRL,
					vpp_matrix_backup);
			VSYNC_WR_DV_REG(VPP_DUMMY_DATA1,
					vpp_dummy1_backup);
		}
		force_reset_core2 = true;
		set_vf_crc_valid(0);
		reset_dv_param();
		clear_dolby_vision_wait();
		if (!is_aml_gxm() && !is_aml_txlx()) {
			hdr_osd_off(VPP_TOP0);
			hdr_vd1_off(VPP_TOP0);
		}
		/*dv release control of pwm*/
		if (is_aml_tvmode()) {
			gd_en = 0;
#ifdef CONFIG_AMLOGIC_LCD
			aml_lcd_atomic_notifier_call_chain
			(LCD_EVENT_BACKLIGHT_GD_SEL, &gd_en);
			set_dv_control_backlight_status(false);
#endif
		}
	}
}

#ifdef ADD_NEW_DV_FUNC
/*multi-inst tv mode,  todo*/
void enable_amdv_v2_tv(int enable)
{
}
#endif

void enable_amdv_v2_stb(int enable)
{
	u32 core_flag = 0;
	u32 diagnostic_enable = m_dovi_setting.diagnostic_enable;
	bool dovi_ll_enable = m_dovi_setting.dovi_ll_enable;
	/*int dv_id = 0;*/

	if (debug_dolby & 8)
		pr_dv_dbg("enable %d, dv on %d, mode %d %d\n",
			  enable, dolby_vision_on, dolby_vision_mode,
			  get_amdv_target_mode());

	if (enable) {
		if (!dolby_vision_on) {
			set_amdv_wait_on();
			/*amdv_ctrl_backup =*/
				/*VSYNC_RD_DV_REG(VPP_AMDV_CTRL);*/
			/*viu_misc_ctrl_backup =*/
				/*VSYNC_RD_DV_REG(VIU_MISC_CTRL1);*/
			/*vpp_matrix_backup =*/
				/*VSYNC_RD_DV_REG(VPP_MATRIX_CTRL);*/
			/*vpp_dummy1_backup =*/
				/*VSYNC_RD_DV_REG(VPP_DUMMY_DATA1);*/

			if (is_amdv_stb_mode()) {
				hdr_osd_off(VPP_TOP0);
				hdr_vd1_off(VPP_TOP0);
				set_hdr_module_status(VD1_PATH,
					HDR_MODULE_BYPASS);
				if (is_aml_t7_stbmode()) {
					if (amdv_mask & 4)
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 1, 3, 1);   /* core3 enable */
					else
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 0, 3, 1);   /* bypass core3  */

					if ((amdv_mask & 2) && (core2_sel & 1)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD_TOP_CTRL,
							 0,
							 14, 1);/*core2a enable*/
					}
					if ((amdv_mask & 2) && (core2_sel & 2)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD1_TOP_CTRL,
							 0,
							 19, 1);/*core2c enable*/
					}
					if (!(amdv_mask & 2)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD_TOP_CTRL,
							 1,
							 14, 1);/*core2a bypass*/
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD1_TOP_CTRL,
							 1,
							 19, 1);/*core2c bypass*/
					}
				} else {
					if (amdv_mask & 4)
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 1, 3, 1);   /* core3 enable */
					else
						VSYNC_WR_DV_REG_BITS
						(VPP_AMDV_CTRL,
						 0, 3, 1);   /* bypass core3  */

					if (amdv_mask & 2)
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0,
							 2, 1);/*core2 enable*/
					else
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1,
							 2, 1);/*core2 bypass*/
				}
				if (is_aml_g12() || is_aml_sc2() || is_aml_s4d()) {
					if ((amdv_mask & 1) &&
					    amdv_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0,
							 0, 1); /* core1 on*/
						dv_core1[0].core1_on = true;
						dv_core1[0].core1_on_cnt = 0;
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1,
							 0, 1); /* core1 off*/
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						dv_core1[0].core1_on = true;
						dv_core1[0].core1_on_cnt = 0;
					}
				} else if (is_aml_tm2_stbmode()) {
					if (is_aml_stb_hdmimode() && !core1_detunnel())
						core_flag = 1;
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 core_flag, 8, 2);
					/*vd2_in: dolby_s1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 10, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 17, 1);
					/*dolby_s1 in: vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 19, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 20, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 21, 1);
					/*dolby_s1 out: vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 23, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 core_flag, 24, 2);
					/* vd2_out: dolby_s1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 26, 2);
					if ((amdv_mask & 1) &&
					    dv_core1[0].amdv_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0, 0, 1); /* core1a on */
						dv_core1[0].core1_on = true;
						dv_core1[0].core1_on_cnt = 0;
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1, 0, 1); /* core1a off */
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							 0x55555455);
						dv_core1[0].core1_on = false;
					}
					if ((amdv_mask & 1) &&
					    dv_core1[1].amdv_setting_video_flag &&
					    support_multi_core1()) {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 0, 1, 1); /* core1b on */
						dv_core1[1].core1_on = true;
						dv_core1[1].core1_on_cnt = 0;
					} else {
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 1, 1, 1); /* core1b off */
						dv_mem_power_off(VPU_DOLBY1B);
						VSYNC_WR_DV_REG
							(AMDV_CORE1B_CLKGATE_CTRL,
							 0x55555455);
						dv_core1[1].core1_on = false;
					}
					pr_dv_dbg
						("DV tm2 turn on %s %s\n",
						 dv_core1[0].core1_on ?
						 "core1a" : "",
						 dv_core1[1].core1_on ?
						 "core1b" : "");
					if (core_flag &&
					(dv_core1[0].core1_on || dv_core1[1].core1_on)) {
						/* disable core1a core1b */
						VSYNC_WR_DV_REG_BITS
							(AMDV_PATH_CTRL,
							 3, 0, 2);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_DOLBY1B);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							 0x55555455);
						VSYNC_WR_DV_REG
							(AMDV_CORE1B_CLKGATE_CTRL,
							 0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					}
				} else if (is_aml_t7_stbmode()) {
					if (is_aml_stb_hdmimode() && !core1_detunnel())
						core_flag = 1;
					/* AMDV_PATH_SWAP_CTRL1 todo*/
					if ((amdv_mask & 1) &&
					    dv_core1[0].amdv_setting_video_flag) {
						/*core1a on*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 0, 4, 1);
						dv_core1[0].core1_on = true;
						dv_core1[0].core1_on_cnt = 0;
					} else {
						/*core1 off*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL,
								0x55555455);
						dv_core1[0].core1_on = false;
					}
					if ((amdv_mask & 1) &&
					    dv_core1[1].amdv_setting_video_flag &&
					    support_multi_core1()) {
						/* core1b on */
						VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL, 0, 4, 1);
						dv_core1[1].core1_on = true;
						dv_core1[1].core1_on_cnt = 0;
					} else {
						/* core1b off */
						VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1B);
						VSYNC_WR_DV_REG(AMDV_CORE1B_CLKGATE_CTRL,
								0x55555455);
						dv_core1[1].core1_on = false;
					}
					pr_dv_dbg
						("DV t7 turn on %s %s\n",
						 dv_core1[0].core1_on ?
						 "core1a" : "",
						 dv_core1[1].core1_on ?
						 "core1b" : "");
					if (core_flag &&
					(dv_core1[0].core1_on || dv_core1[1].core1_on)) {
						/*disable core1a core1b*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL,
								0x55555455);
						VSYNC_WR_DV_REG(AMDV_CORE1B_CLKGATE_CTRL,
								0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					}
				}
				/* bypass all video effect */
				if (dolby_vision_flags & FLAG_BYPASS_VPP)
					video_effect_bypass(1);

				VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_DV_REG(VPP_DUMMY_DATA1,
					0x80200);

				if ((dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_IPT_TUNNEL ||
				    dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_IPT) &&
				    diagnostic_enable == 0 &&
				    dovi_ll_enable) {
					/*u32 *reg = (u32 *)&m_dovi_setting.dm_reg3;*/
					/* input u12 -0x800 to s12 */
					VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA1, 0x8000800);
					/* bypass vadj */
					VSYNC_WR_DV_REG(VPP_VADJ_CTRL, 0);
					/* bypass gainoff */
					VSYNC_WR_DV_REG(VPP_GAINOFF_CTRL0, 0);
					/* enable wm tp vks*/
					/* bypass gainoff to vks */
					VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 1, 1, 2);
				} else {
					/*enable_rgb_to_yuv_matrix_for_dvll(*/
						/*0, NULL, 12);*/
				}
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
			}
		} else {
			if (!dv_core1[0].core1_on &&
			    (amdv_mask & 1) &&
			    dv_core1[0].amdv_setting_video_flag) {
				if (is_aml_g12() ||
				    is_aml_tm2_stbmode() ||
				    is_aml_sc2() ||
				    is_aml_s4d()) {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						/* enable core1a */
						 0, 0, 1);
					if (is_aml_stb_hdmimode() && !core1_detunnel()) {
						/* core1 off */
						VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 0, 2);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							 0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					} else {
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_aml_t7_stbmode()) {
					/* vd1 to core1a*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 0, 0, 3);
					/*core1a bl in sel vd1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 0, 2, 2);
					/*core1a el in sel null*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 3, 4, 2);
					/*core1a out to vd1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 0, 18, 2);
					/* vd1 from core1a*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 0, 12, 3);
					/*enable core1a*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD1_DSC_CTRL,
						 0, 4, 1);
					if (is_aml_stb_hdmimode() && !core1_detunnel()) {
						/* core1 off */
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(AMDV_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					} else {
						hdr_vd1_off(VPP_TOP0);
					}
				}
				dv_core1[0].core1_on = true;
				dv_core1[0].core1_on_cnt = 0;
				pr_dv_dbg("DV core1a turn on\n");
			} else if (!dv_core1[1].core1_on &&
			    (amdv_mask & 1) &&
			    dv_core1[1].amdv_setting_video_flag &&
			    support_multi_core1()) {
				if (is_aml_tm2_stbmode()) {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 8, 2);
					/*vd2_in: dolby_s1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 10, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 17, 1);
					/*dolby_s1 in: vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 19, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 20, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 21, 1);
					/*dolby_s1 out: vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 1, 23, 1);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 0, 24, 2);
					/* vd2_out: dolby_s1*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 3, 26, 2);
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						/* enable core1b */
						 0, 1, 1);
				} else if (is_aml_t7_stbmode()) {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 3, 4, 3);
					/*core1b in sel vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 1, 10, 2);
					/*core1b out to vd2*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL2,
						 1, 22, 2);
					/* vd2 from core1b*/
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_SWAP_CTRL1,
						 3, 16, 3);
					/*enable core1b*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 0, 4, 1);
				}
				dv_core1[1].core1_on = true;
				dv_core1[1].core1_on_cnt = 0;
				pr_dv_dbg("DV core1b turn on\n");
			} else if ((dv_core1[0].core1_on || dv_core1[1].core1_on) &&
				   !(amdv_mask & 1)) {
				if (is_aml_t7_stbmode()) {
					VSYNC_WR_DV_REG_BITS
						(VPP_VD1_DSC_CTRL,
						 /* disable vd1 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 /* disable vd2 dv */
						 1, 4, 1);
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 /* disable vd3 dv */
						 1, 4, 1);
				} else {
					VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL,
						 /* disable vd1 vd2 dv */
						 3, 0, 2);
				}
				/* core1a */
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				VSYNC_WR_DV_REG
					(AMDV_CORE1A_CLKGATE_CTRL,
					 0x55555455);
				if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
					/* core1b */
					dv_mem_power_off(VPU_DOLBY1B);
					VSYNC_WR_DV_REG
						(AMDV_CORE1B_CLKGATE_CTRL,
						0x55555455);
					/* coretv */
					VSYNC_WR_DV_REG_BITS
						(AMDV_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(AMDV_TV_CLKGATE_CTRL,
						0x55555455);
					dv_mem_power_off(VPU_DOLBY0);
					/* hdr core */
					hdr_vd1_off(VPP_TOP0);
				}
				dv_core1[0].core1_on = false;
				dv_core1[0].core1_on_cnt = 0;
				dv_core1[1].core1_on = false;
				dv_core1[1].core1_on_cnt = 0;
				dv_inst[0].frame_count = 0;
				dv_inst[1].frame_count = 0;
				pr_dv_dbg("DV core1 turn off\n");
			} else if (dv_core1[0].core1_on &&
				(!(amdv_mask & 1) ||
				!dv_core1[0].amdv_setting_video_flag)) {
				/* core1a */
				if (is_aml_t7_stbmode()) {
					VSYNC_WR_DV_REG_BITS
						(VPP_VD1_DSC_CTRL,
						 /* disable vd1 dv */
						 1, 4, 1);
				} else {
					VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL, 1, 0, 1);
				}
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				VSYNC_WR_DV_REG
					(AMDV_CORE1A_CLKGATE_CTRL,
					 0x55555455);
				dv_core1[0].core1_on = false;
				dv_core1[0].core1_on_cnt = 0;
				pr_dv_dbg("DV core1a turn off\n");
			} else if (dv_core1[1].core1_on &&
				(!(amdv_mask & 1) ||
				!dv_core1[1].amdv_setting_video_flag)) {
				if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
					if (is_aml_t7_stbmode()) {
						VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 /* disable vd2 dv */
						 1, 4, 1);
					} else {
						VSYNC_WR_DV_REG_BITS
						(AMDV_PATH_CTRL, 1, 1, 1);
					}
					/* core1b */
					dv_mem_power_off(VPU_DOLBY1B);
					VSYNC_WR_DV_REG
					(AMDV_CORE1B_CLKGATE_CTRL,
					 0x55555455);
					dv_core1[1].core1_on = false;
					dv_core1[1].core1_on_cnt = 0;
					pr_dv_dbg("DV core1b turn off\n");
				}
			}
		}
		if (dolby_vision_flags & FLAG_CERTIFICAION) {
			/* 1.not bypass dither/PPS/SR/CM here due to some hdmi-in*/
			/* case need pps scaler, handle in bypass_pps_path*/
			/* 2.bypass EO/OE*/
			/* 3.bypass vadj2/mtx/gainoff */
			VSYNC_WR_DV_REG_BITS
			(VPP_AMDV_CTRL, 3, 1, 2);
			/* bypass all video effect */
			video_effect_bypass(1);
			hdr_vd1_off(VPP_TOP0);
			hdr_vd2_off(VPP_TOP0);
		}
		dolby_vision_on = true;
		clear_dolby_vision_wait();
		set_vsync_count(0);
	} else {
		if (dolby_vision_on) {
			if (is_aml_g12() || is_aml_sc2() ||
			    is_aml_s4d() || is_aml_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(AMDV_PATH_CTRL,
					 (1 << 2) |	/* core2 bypass */
					 (1 << 1) |	/* vd2 connect to vpp */
					 (1 << 0),	/* core1 bl bypass */
					 0, 3);
			} else if (is_aml_t7_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(VPP_VD1_DSC_CTRL,
					 /* disable vd1 dv */
					 1, 4, 1);
				VSYNC_WR_DV_REG_BITS
					(VPP_VD2_DSC_CTRL,
					 /* disable vd2 dv */
					 1, 4, 1);
				VSYNC_WR_DV_REG_BITS
					(VPP_VD3_DSC_CTRL,
					 /* disable vd3 dv */
					 1, 4, 1);
				VSYNC_WR_DV_REG_BITS
					(MALI_AFBCD_TOP_CTRL,
					 /* disable core2a */
					 1, 14, 1);
				VSYNC_WR_DV_REG_BITS
					(MALI_AFBCD1_TOP_CTRL,
					 /* disable core2c */
					 1, 19, 1);
			}
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL,
				0, 3, 1);   /* core3 disable */
			/* core1a */
			VSYNC_WR_DV_REG
				(AMDV_CORE1A_CLKGATE_CTRL,
				 0x55555455);
			dv_mem_power_off(VPU_DOLBY1A);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
			/* core2 */
			VSYNC_WR_DV_REG
				(AMDV_CORE2A_CLKGATE_CTRL,
				 0x55555555);
			dv_mem_power_off(VPU_DOLBY2);
			/* core3 */
			VSYNC_WR_DV_REG
				(AMDV_CORE3_CLKGATE_CTRL,
				 0x55555555);
			dv_mem_power_off(VPU_DOLBY_CORE3);
			if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
				/* core1b */
				VSYNC_WR_DV_REG
					(AMDV_CORE1B_CLKGATE_CTRL,
					 0x55555555);
				dv_mem_power_off(VPU_DOLBY1B);
				/* tv core */
				VSYNC_WR_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
					0x01000042);
				VSYNC_WR_DV_REG_BITS
					(AMDV_TV_SWAP_CTRL7,
					 0xf, 4, 4);
				VSYNC_WR_DV_REG
					(AMDV_TV_CLKGATE_CTRL,
					 0x55555555);
				/* hdr core */
				hdr_vd1_off(VPP_TOP0);
				dv_mem_power_off(VPU_DOLBY0);
			}
			if (p_funcs_stb) {/* destroy ctx */
				p_funcs_stb->multi_control_path
					(&invalid_m_dovi_setting);
			}
			last_dolby_vision_ll_policy =
				DOLBY_VISION_LL_DISABLE;
			stb_core_setting_update_flag = CP_FLAG_CHANGE_ALL;
			stb_core2_const_flag = false;
			pr_dv_dbg("DV turn off\n");

			VSYNC_WR_DV_REG(VIU_SW_RESET, 3 << 9);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
			if (is_amdv_stb_mode()) {
				VSYNC_WR_DV_REG(AMDV_CORE1A_CLKGATE_CTRL,
						0x55555555);
				VSYNC_WR_DV_REG(AMDV_CORE2A_CLKGATE_CTRL,
						0x55555555);
				VSYNC_WR_DV_REG(AMDV_CORE3_CLKGATE_CTRL,
						0x55555555);
			}
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC0,
				 (0x3ff << 20)
				 | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC1,
				 0);
			video_effect_bypass(0);
			reset_dovi_setting();
		}
		force_reset_core2 = true;
		reset_dv_param();
		clear_dolby_vision_wait();
		if (!is_aml_gxm() && !is_aml_txlx()) {
			hdr_osd_off(VPP_TOP0);
			hdr_vd1_off(VPP_TOP0);
		}
	}
}

void enable_amdv(int enable)
{
	if (multi_dv_mode) {
		if (is_aml_tvmode())
			enable_amdv_v1(enable);
		else
			enable_amdv_v2_stb(enable);
	} else {
		enable_amdv_v1(enable);
	}
}

void update_stb_core_setting_flag(int flag)
{
	stb_core_setting_update_flag |= flag;
}

void set_operate_mode(int mode)
{
	operate_mode = mode;
}

int get_operate_mode(void)
{
	return operate_mode;
}

module_param(vtotal_add, uint, 0664);
MODULE_PARM_DESC(vtotal_add, "\n vtotal_add\n");

module_param(vpotch, uint, 0664);
MODULE_PARM_DESC(vpotch, "\n vpotch\n");

module_param(g_vtiming, uint, 0664);
MODULE_PARM_DESC(g_vtiming, "\n vpotch\n");

module_param(dma_start_line, uint, 0664);
MODULE_PARM_DESC(dma_start_line, "\n dma_start_line\n");

module_param(dv_ll_output_mode, uint, 0664);
MODULE_PARM_DESC(dv_ll_output_mode, "\n dv_ll_output_mode\n");

module_param(force_update_reg, uint, 0664);
MODULE_PARM_DESC(force_update_reg, "\n force_update_reg\n");

module_param(bypass_core1a_composer, uint, 0664);
MODULE_PARM_DESC(bypass_core1a_composer, "\n bypass_core1a_composer\n");

module_param(bypass_core1b_composer, uint, 0664);
MODULE_PARM_DESC(bypass_core1b_composer, "\n bypass_core1b_composer\n");

