// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amdolby_vision/amdolby_vision.c
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
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/registers/register_map.h>
//#include <linux/amlogic/cpu_version.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#endif
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include "amdolby_vision.h"

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/utils/am_com.h>
#include "../../common/vfm/vfm.h"
#include <linux/amlogic/media/utils/vdec_reg.h>

DEFINE_SPINLOCK(dovi_lock);

static unsigned int dolby_vision_probe_ok;
static const struct dolby_vision_func_s *p_funcs_stb;
static const struct dolby_vision_func_s *p_funcs_tv;

#define AMDOLBY_VISION_NAME               "amdolby_vision"
#define AMDOLBY_VISION_CLASS_NAME         "amdolby_vision"

#undef V2_4_3
/*#define V2_4_3*/
struct amdolby_vision_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
	wait_queue_head_t	dv_queue;
};
static struct amdolby_vision_dev_s amdolby_vision_dev;
struct dv_device_data_s dv_meson_dev;
static unsigned int dolby_vision_request_mode = 0xff;

static unsigned int dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mode, "\n dolby_vision_mode\n");

static unsigned int dolby_vision_target_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_target_mode, uint, 0444);
MODULE_PARM_DESC(dolby_vision_target_mode, "\n dolby_vision_target_mode\n");

static unsigned int dolby_vision_profile = 0xff;
module_param(dolby_vision_profile, uint, 0664);
MODULE_PARM_DESC(dolby_vision_profile, "\n dolby_vision_profile\n");

static unsigned int dolby_vision_level = 0xff;
module_param(dolby_vision_level, uint, 0664);
MODULE_PARM_DESC(dolby_vision_level, "\n dolby_vision_level\n");

static unsigned int primary_debug;
module_param(primary_debug, uint, 0664);
MODULE_PARM_DESC(primary_debug, "\n primary_debug\n");
/* STB: if sink support DV, always output DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV/HDR10/HLG, convert to SDR */
/* #define DOLBY_VISION_FOLLOW_SINK		0 */

/* STB: output DV only if source is DV*/
/*		and sink support DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV, convert to SDR */
/* #define DOLBY_VISION_FOLLOW_SOURCE		1 */

/* STB: always follow dolby_vision_mode */
/* TV:  if set dolby_vision_mode to SDR8,*/
/*		convert all format to SDR by TV core,*/
/*		else bypass Dolby Vision */
/* #define DOLBY_VISION_FORCE_OUTPUT_MODE	2 */

static unsigned int dolby_vision_policy;
module_param(dolby_vision_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_policy, "\n dolby_vision_policy\n");
static unsigned int last_dolby_vision_policy;

/* === HDR10 === */
/* bit0: follow sink 0: bypass hdr10 to vpp 1: process hdr10 by dolby core */
/* bit1: follow source 0: bypass hdr10 to vpp 1: process hdr10 by dolby core */
/* === HDR10+ === */
/* bit2: 0: bypass hdr10+ to vpp, 1: process hdr10+ as hdr10 by dolby core */
/* === HLG -- TV core 1.6 only === */
/* bit3: follow sink 0: bypass hlg to vpp, 1: process hlg by dolby core */
/* bit4: follow source 0: bypass hlg to vpp, 1: process hlg by dolby core */
/* === SDR === */
/* bit5: 0: bypass SDR to vpp, 1: process SDR by dolby core */
/* set by policy_process */
#define HDR_BY_DV_F_SINK 0x1
#define HDR_BY_DV_F_SRC 0x2
#define HDRP_BY_DV 0x4
#define HLG_BY_DV_F_SINK 0x8
#define HLG_BY_DV_F_SRC 0x10
#define SDR_BY_DV 0x20

static unsigned int dolby_vision_hdr10_policy = HDR_BY_DV_F_SINK;
module_param(dolby_vision_hdr10_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_hdr10_policy, "\n dolby_vision_hdr10_policy\n");
static unsigned int last_dolby_vision_hdr10_policy = HDR_BY_DV_F_SINK;

/* enable hdmi dv std to stb core */
static uint hdmi_to_stb_policy = 1;
module_param(hdmi_to_stb_policy, uint, 0664);
MODULE_PARM_DESC(hdmi_to_stb_policy, "\n hdmi_to_stb_policy\n");

static bool dolby_vision_enable;
module_param(dolby_vision_enable, bool, 0664);
MODULE_PARM_DESC(dolby_vision_enable, "\n dolby_vision_enable\n");

static bool dolby_vision_efuse_bypass;
module_param(dolby_vision_efuse_bypass, bool, 0664);
MODULE_PARM_DESC(dolby_vision_efuse_bypass, "\n dolby_vision_efuse_bypass\n");
static bool efuse_mode;

static bool el_mode;

static uint dolby_vision_mask = 7;
module_param(dolby_vision_mask, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mask, "\n dolby_vision_mask\n");

#define BYPASS_PROCESS 0
#define SDR_PROCESS 1
#define HDR_PROCESS 2
#define DV_PROCESS 3
#define HLG_PROCESS 4
static uint dolby_vision_status;
module_param(dolby_vision_status, uint, 0664);
MODULE_PARM_DESC(dolby_vision_status, "\n dolby_vision_status\n");

/* delay before first frame toggle when core off->on */
static uint dolby_vision_wait_delay = 2;
module_param(dolby_vision_wait_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_wait_delay, "\n dolby_vision_wait_delay\n");
static int dolby_vision_wait_count;

/* reset 1st fake frame (bit 0)*/
/*   and other fake frames (bit 1)*/
/*   and other toggle frames (bit 2) */
static uint dolby_vision_reset = (1 << 1) | (1 << 0);
module_param(dolby_vision_reset, uint, 0664);
MODULE_PARM_DESC(dolby_vision_reset, "\n dolby_vision_reset\n");

/* force run mode */
static uint dolby_vision_run_mode = 0xff; /* not force */
module_param(dolby_vision_run_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_run_mode, "\n dolby_vision_run_mode\n");

/* number of fake frame (run mode = 1) */
#define RUN_MODE_DELAY 2
#define RUN_MODE_DELAY_GXM 3

static uint dolby_vision_run_mode_delay = RUN_MODE_DELAY;
module_param(dolby_vision_run_mode_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_run_mode_delay, "\n dolby_vision_run_mode_delay\n");

/* reset control -- end << 8 | start */
static uint dolby_vision_reset_delay =
	(RUN_MODE_DELAY << 8) | RUN_MODE_DELAY;
module_param(dolby_vision_reset_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_reset_delay, "\n dolby_vision_reset_delay\n");

static unsigned int dolby_vision_tuning_mode;
module_param(dolby_vision_tuning_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_tuning_mode, "\n dolby_vision_tuning_mode\n");

static unsigned int dv_ll_output_mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
module_param(dv_ll_output_mode, uint, 0664);
MODULE_PARM_DESC(dv_ll_output_mode, "\n dv_ll_output_mode\n");

#define DOLBY_VISION_LL_DISABLE		0
#define DOLBY_VISION_LL_YUV422				1
#define DOLBY_VISION_LL_RGB444				2
static u32 dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;
module_param(dolby_vision_ll_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_ll_policy, "\n dolby_vision_ll_policy\n");
static u32 last_dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;

#ifdef V2_4_3
#define DOLBY_VISION_STD_RGB_TUNNEL	0
#define DOLBY_VISION_STD_YUV422		1
static u32 dolby_vision_std_policy = DOLBY_VISION_STD_RGB_TUNNEL;
module_param(dolby_vision_std_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_std_policy, "\n dolby_vision_ll_policy\n");
/*static u32 last_dolby_vision_std_policy = DOLBY_VISION_STD_RGB_TUNNEL;*/
static bool force_support_emp;
#endif

static unsigned int dolby_vision_src_format;
module_param(dolby_vision_src_format, uint, 0664);
MODULE_PARM_DESC(dolby_vision_src_format, "\n dolby_vision_src_format\n");

static unsigned int force_mel;
module_param(force_mel, uint, 0664);
MODULE_PARM_DESC(force_mel, "\n force_mel\n");

static unsigned int force_best_pq;
module_param(force_best_pq, uint, 0664);
MODULE_PARM_DESC(force_best_pq, "\n force_best_pq\n");

static unsigned int force_hdr_tonemapping;
module_param(force_hdr_tonemapping, uint, 0664);
MODULE_PARM_DESC(force_hdr_tonemapping, "\n force_hdr_tonemapping\n");

/*bit0: 0-> efuse, 1->no efuse; */
/*bit1: 1->ko loaded*/
/*bit2: 1-> value updated*/
static int support_info;
int get_dv_support_info(void)
{
	return support_info;
}
EXPORT_SYMBOL(get_dv_support_info);

static bool force_bypass_from_prebld_to_vadj1;/* t3/t5w, 1d93 bit0 -> 1d26 bit8*/
bool get_force_bypass_from_prebld_to_vadj1(void)
{
	return force_bypass_from_prebld_to_vadj1;
}
EXPORT_SYMBOL(get_force_bypass_from_prebld_to_vadj1);

char cur_crc[32] = "invalid";

static uint dolby_vision_on_count;
static bool dolby_vision_el_disable;

#define FLAG_FORCE_CVM				0x01
#define FLAG_BYPASS_CVM				0x02
#define FLAG_BYPASS_VPP				0x04
#define FLAG_USE_SINK_MIN_MAX		0x08
#define FLAG_CLKGATE_WHEN_LOAD_LUT	0x10
#define FLAG_SINGLE_STEP			0x20
#define FLAG_CERTIFICAION			0x40
#define FLAG_CHANGE_SEQ_HEAD		0x80
#define FLAG_DISABLE_COMPOSER		0x100
#define FLAG_BYPASS_CSC				0x200
#define FLAG_CHECK_ES_PTS			0x400
#define FLAG_DISABE_CORE_SETTING	0x800
#define FLAG_DISABLE_DMA_UPDATE		0x1000
#define FLAG_DISABLE_DOVI_OUT		0x2000
#define FLAG_FORCE_DOVI_LL			0x4000
#define FLAG_FORCE_RGB_OUTPUT		0x8000
#define FLAG_DOVI2HDR10_NOMAPPING	0x100000
#define FLAG_PRIORITY_GRAPHIC		0x200000
#define FLAG_DISABLE_LOAD_VSVDB		0x400000
#define FLAG_DISABLE_CRC			0x800000
#define FLAG_ENABLE_EL				0x1000000
#define FLAG_DEBUG_CORE2_TIMING			0x2000000
#define FLAG_MUTE				0x4000000
#define FLAG_FORCE_HDMI_PKT			0x8000000
#define FLAG_RX_EMP_VSEM			0x20000000
#define FLAG_TOGGLE_FRAME			0x80000000

#define FLAG_FRAME_DELAY_MASK	0xf
#define FLAG_FRAME_DELAY_SHIFT	16

static unsigned int dolby_vision_flags = FLAG_BYPASS_VPP | FLAG_FORCE_CVM;
module_param(dolby_vision_flags, uint, 0664);
MODULE_PARM_DESC(dolby_vision_flags, "\n dolby_vision_flags\n");

/*bit0:reset core1 reg; bit1:reset core2 reg;bit2:reset core3 reg*/
/*bit3: reset core1 lut; bit4: reset core2 lut*/
static unsigned int force_update_reg;
module_param(force_update_reg, uint, 0664);
MODULE_PARM_DESC(force_update_reg, "\n force_update_reg\n");

#define DV_NAME_LEN_MAX 32

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
/*dma size:1877x2x64 bit = 30032 byte*/
#define TV_DMA_TBL_SIZE 3754
static unsigned int dma_size = 30032;
static dma_addr_t dma_paddr;
static void *dma_vaddr;
static unsigned int dma_start_line = 0x400;

#define CRC_BUFF_SIZE (256 * 1024)
static char *crc_output_buf;
static u32 crc_output_buff_off;
static u32 crc_count;
static u32 crc_bypass_count;
static u32 setting_update_count;
static s32 crc_read_delay;
static u32 core1_disp_hsize;
static u32 core1_disp_vsize;
static u32 vsync_count;
#define FLAG_VSYNC_CNT 10
#define MAX_TRANSITION_DELAY 5
#define MIN_TRANSITION_DELAY 2
#define MAX_CORE3_MD_SIZE 128 /*512byte*/

static bool is_osd_off;
static bool force_reset_core2;
static int core1_switch;
static int core3_switch;
static bool force_set_lut;
static bool ambient_update;
static struct ambient_cfg_s ambient_config_new;

/*core reg must be set at first time. bit0 is for core2, bit1 is for core3*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static u32 first_reseted;
#endif

static char *ko_info;
static char total_chip_name[20];
static char chip_name[5] = "null";

/*bit0: copy core1a to core1b;  bit1: copy core1a to core1c*/
static int copy_core1a;
/*bit0: core2a, bit1: core2c*/
static int core2_sel = 1;

module_param(dma_start_line, uint, 0664);
MODULE_PARM_DESC(dma_start_line, "\n dma_start_line\n");

module_param(vtotal_add, uint, 0664);
MODULE_PARM_DESC(vtotal_add, "\n vtotal_add\n");
module_param(vpotch, uint, 0664);
MODULE_PARM_DESC(vpotch, "\n vpotch\n");

/* for core2 timing setup tuning */
/* g_vtotal_add << 24 | g_vsize_add << 16 */
/* | g_vwidth << 8 | g_vpotch */
static unsigned int g_vtiming;
module_param(g_vtiming, uint, 0664);
MODULE_PARM_DESC(g_vtiming, "\n vpotch\n");

static unsigned int dolby_vision_target_min = 50; /* 0.0001 */
static unsigned int dolby_vision_target_max[3][3] = {
	{ 4000, 1000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_default_max[3][3] = {
	{ 4000, 4000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_graphic_min = 50; /* 0.0001 */
static unsigned int dolby_vision_graphic_max; /* 100 */
static unsigned int old_dolby_vision_graphic_max;
module_param(dolby_vision_graphic_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_min, "\n dolby_vision_graphic_min\n");
module_param(dolby_vision_graphic_max, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_max, "\n dolby_vision_graphic_max\n");

static unsigned int dv_HDR10_graphics_max = 300;
static unsigned int dv_graphic_blend_test;
module_param(dv_graphic_blend_test, uint, 0664);
MODULE_PARM_DESC(dv_graphic_blend_test, "\n dv_graphic_blend_test\n");

static unsigned int dv_target_graphics_max[3][3] = {
	{ 300, 375, 380 }, /* DOVI => DOVI/HDR/SDR */
	{ 300, 375, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 300, 375, 100 }, /* SDR =>  DOVI/HDR/SDR */
};
static unsigned int dv_target_graphics_LL_max[3][3] = {
	{ 300, 375, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 210, 375, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 300, 375, 100 }, /* SDR =>  DOVI/HDR/SDR */
};

/*these two parameters form OSD*/
static unsigned int osd_graphic_width = 1920;
static unsigned int osd_graphic_height = 1080;
static unsigned int new_osd_graphic_width = 1920;
static unsigned int new_osd_graphic_height = 1080;

static unsigned int dv_cert_graphic_width = 1920;
static unsigned int dv_cert_graphic_height = 1080;
module_param(dv_cert_graphic_width, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_width, "\n dv_cert_graphic_width\n");
module_param(dv_cert_graphic_height, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_height, "\n dv_cert_graphic_height\n");

static unsigned int enable_tunnel;
static u32 vpp_data_422T0444_backup;

/* 0: video priority 1: graphic priority */
static unsigned int dolby_vision_graphics_priority;
module_param(dolby_vision_graphics_priority, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphics_priority, "\n dolby_vision_graphics_priority\n");

/*1:HDR10, 2:HLG, 3: DV LL*/
static int force_hdmin_fmt;

static unsigned int atsc_sei = 1;
module_param(atsc_sei, uint, 0664);
MODULE_PARM_DESC(atsc_sei, "\n atsc_sei\n");

static struct dv_atsc p_atsc_md;

u16 L2PQ_100_500[] = {
	2081, /* 100 */
	2157, /* 120 */
	2221, /* 140 */
	2277, /* 160 */
	2327, /* 180 */
	2372, /* 200 */
	2413, /* 220 */
	2450, /* 240 */
	2485, /* 260 */
	2517, /* 280 */
	2547, /* 300 */
	2575, /* 320 */
	2602, /* 340 */
	2627, /* 360 */
	2651, /* 380 */
	2673, /* 400 */
	2694, /* 420 */
	2715, /* 440 */
	2734, /* 460 */
	2753, /* 480 */
	2771, /* 500 */
};

u16 L2PQ_500_4000[] = {
	2771, /* 500 */
	2852, /* 600 */
	2920, /* 700 */
	2980, /* 800 */
	3032, /* 900 */
	3079, /* 1000 */
	3122, /* 1100 */
	3161, /* 1200 */
	3197, /* 1300 */
	3230, /* 1400 */
	3261, /* 1500 */
	3289, /* 1600 */
	3317, /* 1700 */
	3342, /* 1800 */
	3366, /* 1900 */
	3389, /* 2000 */
	3411, /* 2100 */
	3432, /* 2200 */
	3451, /* 2300 */
	3470, /* 2400 */
	3489, /* 2500 */
	3506, /* 2600 */
	3523, /* 2700 */
	3539, /* 2800 */
	3554, /* 2900 */
	3570, /* 3000 */
	3584, /* 3100 */
	3598, /* 3200 */
	3612, /* 3300 */
	3625, /* 3400 */
	3638, /* 3500 */
	3651, /* 3600 */
	3662, /* 3700 */
	3674, /* 3800 */
	3686, /* 3900 */
	3697, /* 4000 */
};

static u32 tv_max_lin = 200;
static u16 tv_max_pq = 2372;

static unsigned int panel_max_lumin = 350;
module_param(panel_max_lumin, uint, 0664);
MODULE_PARM_DESC(panel_max_lumin, "\n panel_max_lumin\n");

static unsigned int sdr_ref_mode;
module_param(sdr_ref_mode, uint, 0664);
MODULE_PARM_DESC(sdr_ref_mode, "\n sdr_ref_mode\n");

static unsigned int ambient_test_mode;
module_param(ambient_test_mode, uint, 0664);
MODULE_PARM_DESC(ambient_test_mode, "\n ambient_test_mode\n");

static int content_fps = 24;
static int gd_rf_adjust;
static int enable_vf_check;
static u32 last_vf_valid_crc;

#define MAX_DV_PICTUREMODES 40
struct pq_config *bin_to_cfg;

static struct dv_cfg_info_s cfg_info[MAX_DV_PICTUREMODES];

static s16 pq_center[MAX_DV_PICTUREMODES][4];
struct dv_pq_range_s pq_range[4];
u8 current_vsvdb[7];

#define USE_CENTER_0 0

int num_picture_mode;
static int default_pic_mode = 1;/*bright(standard) mode as default*/
static int cur_pic_mode;/*current picture mode id*/
static bool load_bin_config;
static bool need_update_cfg;

static const s16 EXTER_MIN_PQ = -256;
static const s16 EXTER_MAX_PQ = 256;
static const s16 INTER_MIN_PQ = -4096;
static const s16 INTER_MAX_PQ = 4095;

#define LEFT_RANGE_BRIGHTNESS  (-3074) /*limit the range*/
#define RIGHT_RANGE_BRIGHTNESS (3074)
#define LEFT_RANGE_CONTRAST    (-3074)
#define RIGHT_RANGE_CONTRAST   (3074)
#define LEFT_RANGE_COLORSHIFT  (-3074)
#define RIGHT_RANGE_COLORSHIFT (3074)
#define LEFT_RANGE_SATURATION  (-3074)
#define RIGHT_RANGE_SATURATION (3074)

const char *pq_item_str[] = {"brightness",
			     "contrast",
			     "colorshift",
			     "saturation",
			     "darkdetail"};

const char *eotf_str[] = {"bt1886", "pq", "power"};

static u32 cur_debug_tprimary[3][2];
static int debug_tprimary;

/*0: set exter pq [-256,256]. 1:set inter pq [-4096,4095]*/
static int set_inter_pq;
static DEFINE_SPINLOCK(cfg_lock);

/*update flag=>bit0: front,bit1: rear, bit2:whitexy, bit3:mode,bit4:darkdetail*/
#define AMBIENT_CFG_FRAMES 46
struct ambient_cfg_s ambient_test_cfg[AMBIENT_CFG_FRAMES] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{ 12, 65536, 0, 0, 0, 0, 0},
	{ 4, 0, 0, 0, 32768, 0, 0},
	{ 4, 0, 0, 0, 0, 32768, 0},
	{ 4, 0, 0, 0, 32768, 32768, 0},
	{ 6, 0, 0, 0, 10246, 10780, 0},
	{ 2, 0, 10000, 0, 0, 0, 0},
	{ 3, 0, 5, 0, 0, 0, 0},
	{ 1, 0, 0, 30000, 0, 0, 0},
	{ 8, 0, 0, 0, 0, 0, 0},
	{ 8, 65536, 0, 0, 0, 0, 0},
	{ 7, 0, 2204, 23229, 589, 30638, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 1, 0, 0, 24942, 0, 0, 0},
	{ 7, 0, 4646, 25899, 31686, 9764, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 4, 0, 0, 0, 491, 30113, 0},
	{ 4, 0, 0, 0, 11632, 13402, 0},
	{ 4, 0, 0, 0, 22282, 31162, 0},
	{ 6, 0, 6276, 0, 98, 16056, 0},
	{ 6, 0, 5276, 0, 8454, 6946, 0},
	{ 2, 0, 6558, 0, 0, 0, 0},
	{ 2, 0, 355, 0, 0, 0, 0},
	{ 7, 0, 6898, 9651, 30539, 17104, 0},
	{ 3, 0, 5558, 838, 0, 0, 0},
	{ 5, 0, 0, 8462, 7438, 655, 0},
	{ 8, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 9, 65536, 0, 12792, 0, 0, 0},
	{ 4, 0, 0, 0, 30179, 14909, 0},
	{ 3, 0, 1287, 16711, 0, 0, 0},
	{ 7, 0, 991, 9667, 14450, 9699, 0},
	{ 4, 0, 0, 0, 12746, 5636, 0},
	{ 2, 0, 1382, 0, 0, 0, 0},
	{ 1, 0, 0, 14647, 0, 0, 0},
	{ 2, 0, 9080, 0, 0, 0, 0},
	{ 4, 0, 0, 0, 21921, 16056, 0},
	{ 3, 0, 6840, 13834, 0, 0, 0},
	{ 5, 0, 0, 21597, 25755, 26673, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 5, 0, 0, 19217, 17825, 17989, 0},
	{ 6, 0, 2185, 0, 13828, 5406, 0},
	{ 2, 0, 3587, 0, 0, 0, 0},
	{ 5, 0, 0, 17788, 3571, 28508, 0},
	{ 4, 0, 0, 0, 32636, 5799, 0},
};

struct ambient_cfg_s ambient_test_cfg_2[AMBIENT_CFG_FRAMES] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{28, 65536, 5, 24942, 0, 0, 1},
	{31, 65536, 5, 0, 32768, 0, 1},
	{31, 65536, 5, 30000, 0, 32768, 1},
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{30, 65536, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{31, 65536, 2204, 23229, 589, 30638, 1},
	{31, 65536, 4646, 25899, 31686, 9764, 1},
	{28, 65536, 4646, 25899, 491, 30113, 1},
	{28, 65536, 4646, 25899, 11632, 13402, 1},
	{31, 65536, 6276, 23229, 98, 16056, 1},
	{14, 65536, 5276, 23229, 8454, 6946, 1},
	{18, 65536, 6558, 23229, 8454, 6946, 0},
	{18, 65536, 355, 23229, 8454, 6946, 1},
	{31, 65536, 6898, 9651, 30539, 17104, 1},
	{11, 65536, 5558, 838, 30539, 17104, 1},
	{29, 65536, 5558, 8462, 7438, 655, 1},
	{8, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{9, 65536, 5558, 12792, 7438, 655, 1},
	{4, 65536, 5558, 12792, 30179, 14909, 1},
	{11, 65536, 1287, 16711, 30179, 14909, 1},
	{15, 65536, 991, 9667, 14450, 9699, 1},
	{12, 65536, 991, 9667, 12746, 5636, 1},
	{10, 65536, 1382, 9667, 12746, 5636, 1},
	{9, 65536, 1382, 14647, 12746, 5636, 1},
	{10, 65536, 9080, 14647, 12746, 5636, 1},
	{12, 65536, 9080, 14647, 21921, 16056, 1},
	{11, 65536, 6840, 13834, 21921, 16056, 1},
	{13, 65536, 6840, 21597, 25755, 26673, 1},
	{5, 65536, 6840, 17788, 3571, 28508, 1},
};

struct ambient_cfg_s ambient_test_cfg_3[AMBIENT_CFG_FRAMES] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 0},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{28, 65536, 5, 24942, 0, 0, 1},
	{31, 65536, 5, 0, 32768, 0, 1},
	{31, 65536, 5, 30000, 0, 32768, 1},
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{30, 65536, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{31, 65536, 2204, 23229, 589, 30638, 1},
	{31, 65536, 4646, 25899, 31686, 9764, 1},
	{28, 65536, 4646, 25899, 491, 30113, 1},
	{28, 65536, 4646, 25899, 11632, 13402, 1},
	{31, 65536, 6276, 23229, 98, 16056, 1},
	{14, 65536, 5276, 23229, 8454, 6946, 1},
	{18, 65536, 6558, 23229, 8454, 6946, 0},
	{18, 65536, 355, 23229, 8454, 6946, 1},
	{31, 65536, 6898, 9651, 30539, 17104, 1},
	{11, 65536, 5558, 838, 30539, 17104, 1},
	{29, 65536, 5558, 8462, 7438, 655, 1},
	{8, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{9, 65536, 5558, 12792, 7438, 655, 1},
	{4, 65536, 5558, 12792, 30179, 14909, 1},
	{11, 65536, 1287, 16711, 30179, 14909, 1},
	{15, 65536, 991, 9667, 14450, 9699, 1},
	{12, 65536, 991, 9667, 12746, 5636, 1},
	{10, 65536, 1382, 9667, 12746, 5636, 1},
	{9, 65536, 1382, 14647, 12746, 5636, 1},
	{10, 65536, 9080, 14647, 12746, 5636, 1},
	{12, 65536, 9080, 14647, 21921, 16056, 1},
	{11, 65536, 6840, 13834, 21921, 16056, 1},
	{13, 65536, 6840, 21597, 25755, 26673, 1},
	{5, 65536, 6840, 17788, 3571, 28508, 1},
};

struct ambient_cfg_s ambient_darkdetail = {16, 0, 0, 0, 0, 0, 1};

struct target_config def_tgt_display_cfg_bestpq = {
	36045,
	2,
	0,
	2920,
	88,
	2920,
	2621,
	183500800,
	183500800,
	{
	45305196,
	20964810,
	17971754,
	43708004,
	10415296,
	3180960,
	20984942,
	22078816
	},
	2048,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1229,
	0,
	0,
	0,
	4095,
	0,
	0,
	0,
	{
		0,
		131,
		183500800,
		13107200,
		82897208,
		4095,
		2048,
		100,
		1,
		{
			{5961, 20053, 2024},
			{-3287, -11053, 14340},
			{14340, -13026, -1313}
		},
		15,
		1,
		{2048, 16384, 16384},
		580280,
		82897,
		0,
		17,
		2920,
		1803,
		0,
		0,
		50,
		3276,
		3276,
		200,
		444596224,
		{0, 0, 0}
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		{0}
	},
	{
		{0, 0, 0},
		0,
		0,
		65536,
		0,
		4096,
		5,
		4096,
		{10247, 10781},
		32768,
		3277,
		0,
		1365,
		1365
	},
	{4, 216, 163, 84, 218, 76, 153},
	0,
	2,
	0,
	{0},
	0,
	{
		{
			{18835, -15103, 363},
			{-3457, 8025, -472},
			{155, -605, 4546}
		},
		12,
		{0, 0, 0},
		0,
		{0, 0, 0}
	},
	4096,
	1,
	1,
	{50, 100, 300, 500, 800, 1000, 2000, 3000},
	{65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0},
	0,
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0} /* padding */
};

/*tv cert: DV LL case 5051 and 5055 use this cfg */
struct target_config def_tgt_display_cfg_ll = {
	36045,
	2,
	0,
	2920,
	88,
	2920,
	2621,
	183500800,
	183500800,
	{
	45305196,
	20964810,
	17971754,
	43708004,
	10415296,
	3180960,
	20984942,
	22078816
	},
	2048,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	{
		1,
		187,
		183500800,
		13107200,
		69356784,
		4095,
		2048,
		100,
		1,
		{
			{5961, 20053, 2024},
			{-3287, -11053, 14340},
			{14340, -13026, -1313}
		},
		15,
		1,
		{2048, 16384, 16384},
		693567,
		99081,
		0,
		21,
		2920,
		1803,
		0,
		0,
		50,
		3276,
		3276,
		200,
		1170210816u,
		{0, 0, 0}
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		{0}
	},
	{
		{0, 0, 0},
		0,
		0,
		0,
		0,
		4096,
		5,
		4096,
		{10247, 10781},
		32768,
		3277,
		0,
		1365,
		1365
	},
	{42, 25, 25, 241, 138, 78, 108},
	1,
	2,
	0,
	{0},
	0,
	{
		{
			{18835, -15103, 363},
			{-3457, 8025, -472},
			{155, -605, 4546}
		},
		12,
		{0, 0, 0},
		0,
		{0, 0, 0}
	},
	4096,
	1,
	1,
	{50, 100, 300, 500, 800, 1000, 2000, 3000},
	{65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0},
	0,
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0} /* padding */
};

unsigned int debug_dolby;
module_param(debug_dolby, uint, 0664);
MODULE_PARM_DESC(debug_dolby, "\n debug_dolby\n");

static unsigned int debug_dolby_frame = 0xffff;
module_param(debug_dolby_frame, uint, 0664);
MODULE_PARM_DESC(debug_dolby_frame, "\n debug_dolby_frame\n");

#define pr_dolby_dbg(fmt, args...)\
	do {\
		if (debug_dolby)\
			pr_info("DOLBY: " fmt, ## args);\
	} while (0)
#define pr_dolby_error(fmt, args...)\
	pr_info("DOLBY ERROR: " fmt, ## args)
#define dump_enable \
	((debug_dolby_frame >= 0xffff) || \
	(debug_dolby_frame + 1 == frame_count))

#define single_step_enable \
	(((debug_dolby_frame >= 0xffff) || \
	((debug_dolby_frame + 1) == frame_count)) && \
	(debug_dolby & 0x80))

#define DV_CORE1_RECONFIG_CNT 2
#define DV_CORE2_RECONFIG_CNT 120

static bool dolby_vision_on;
static bool dolby_vision_core1_on;
static u32 dolby_vision_core1_on_cnt;
static bool dolby_vision_wait_on;
static u32 dolby_vision_core2_on_cnt;

module_param(dolby_vision_wait_on, bool, 0664);
MODULE_PARM_DESC(dolby_vision_wait_on, "\n dolby_vision_wait_on\n");
static bool dolby_vision_on_in_uboot;
static bool dolby_vision_wait_init;
static unsigned int frame_count;
static struct hdr10_parameter hdr10_param;
static struct hdr10_parameter last_hdr10_param;
static struct master_display_info_s hdr10_data;

static char *md_buf[2];
static char *comp_buf[2];
static char *drop_md_buf[2];
static char *drop_comp_buf[2];

#define VSEM_IF_BUF_SIZE 4096
static char *vsem_if_buf;/* buffer for vsem or vsif */
static char *vsem_md_buf;
#define VSEM_PKT_SIZE 31

static int current_id = 1;
static int backup_comp_size;
static int backup_md_size;
static struct dovi_setting_s dovi_setting;
static struct dovi_setting_s new_dovi_setting;

void *pq_config_fake;
struct tv_dovi_setting_s *tv_dovi_setting;
static bool pq_config_set_flag;

static bool best_pq_config_set_flag;
struct ui_menu_params menu_param = {50, 50, 50};
static bool tv_dovi_setting_change_flag;
static bool tv_dovi_setting_update_flag;
static bool dovi_setting_video_flag;
static struct platform_device *dovi_pdev;
static bool vsvdb_config_set_flag;
static bool vfm_path_on;

#define CP_FLAG_CHANGE_CFG      0x000001
#define CP_FLAG_CHANGE_MDS      0x000002
#define CP_FLAG_CHANGE_MDS_CFG  0x000004
#define CP_FLAG_CHANGE_GD       0x000008
#define CP_FLAG_CHANGE_TC       0x000010
#define CP_FLAG_CHANGE_TC2      0x000020
#define CP_FLAG_CHANGE_L2NL     0x000040
#define CP_FLAG_CHANGE_3DLUT    0x000080
#define CP_FLAG_CONST_TC        0x100000
#define CP_FLAG_CONST_TC2       0x200000
#define CP_FLAG_CHANGE_ALL         0xffffffff
/* update all core */
static u32 stb_core_setting_update_flag = CP_FLAG_CHANGE_ALL;
static bool stb_core2_const_flag;

/* 256+(256*4+256*2)*4/8 64bit */
#define STB_DMA_TBL_SIZE (256 + (256 * 4 + 256 * 2) * 4 / 8)
static u64 stb_core1_lut[STB_DMA_TBL_SIZE];

static bool tv_mode;
static bool mel_mode;
static bool osd_update;
static bool enable_fel;
static bool enable_mel;
static u32 tv_backlight;
static bool tv_backlight_changed;
static bool tv_backlight_force_update;
static int force_disable_dv_backlight;
static bool dv_control_backlight_status;
#ifdef CONFIG_AMLOGIC_LCD
static bool use_12b_bl = true;/*12bit backlight interface*/
#endif
static bool enable_vpu_probe;
static bool bypass_all_vpp_pq;
static int use_target_lum_from_cfg;

/*0: not debug mode; 1:force bypass vpp pq; 2:force enable vpp pq*/
/*3: force do nothing*/
static u32 debug_bypass_vpp_pq;

static int bl_delay_cnt;
static int set_backlight_delay_vsync = 1;
module_param(set_backlight_delay_vsync, int, 0664);
MODULE_PARM_DESC(set_backlight_delay_vsync,    "\n set_backlight_delay_vsync\n");

static bool disable_aoi;
static int debug_cp_res;
static int debug_disable_aoi;
static int debug_dma_start_line;

s16 brightness_off[8][2] = {
	{0, 150},   /* DV OTT DM3, DM4  */
	{0, 150}, /* DV Sink-led DM3, DM4 */
	{0, 0},   /* DV Source-led DM3, DM4, actually no dm3 for source-led*/
	{0, 300},   /* HDR OTT DM3, DM4, actually no dm3 for HDR  */
	{0, 0},    /* HLG OTT DM3, DM4, actually no dm3 for HLG  */
	{0, 0},    /* reserved1 */
	{0, 0},    /* reserved2 */
	{0, 0},    /* reserved3 */
};

struct tv_input_info_s *tv_input_info;

static bool enable_vpu_probe;
static bool module_installed;
static bool recovery_mode;
static bool force_runmode;
static unsigned int hdmi_frame_count;/*changed when frame content update*/

static u32 mali_afbcd_top_ctrl;
static u32 mali_afbcd_top_ctrl_mask;
static u32 mali_afbcd1_top_ctrl;
static u32 mali_afbcd1_top_ctrl_mask;
static bool update_mali_top_ctrl;

#define MAX_PARAM   8
static bool is_meson_gxm(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_GXM)
		return true;
	else
		return false;
}

static bool is_meson_g12(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_G12)
		return true;
	else
		return false;
}

static bool is_meson_sc2(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_SC2)
		return true;
	else
		return false;
}

static bool is_meson_s4d(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_S4D)
		return true;
	else
		return false;
}

static bool is_meson_box(void)
{
	if (is_meson_gxm() || is_meson_g12() || is_meson_sc2() || is_meson_s4d())
		return true;
	else
		return false;
}

static bool is_meson_txlx(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TXLX)
		return true;
	else
		return false;
}

static bool is_meson_txlx_tvmode(void)
{
	if (is_meson_txlx() && tv_mode == 1)
		return true;
	else
		return false;
}

static bool is_meson_txlx_stbmode(void)
{
	if (is_meson_txlx() && tv_mode == 0)
		return true;
	else
		return false;
}

static bool is_meson_tm2(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2 ||
		dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2_REVB)
		return true;
	else
		return false;
}

static bool is_meson_tm2_revb(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2_REVB)
		return true;
	else
		return false;
}

static bool is_meson_tm2_tvmode(void)
{
	if (is_meson_tm2() && tv_mode == 1)
		return true;
	else
		return false;
}

static bool is_meson_tm2_stbmode(void)
{
	if (is_meson_tm2() && tv_mode == 0)
		return true;
	else
		return false;
}

static bool is_meson_t7(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_T7)
		return true;
	else
		return false;
}

static bool is_meson_t7_tvmode(void)
{
	if (is_meson_t7() && tv_mode == 1)
		return true;
	else
		return false;
}

static bool is_meson_t7_stbmode(void)
{
	if (is_meson_t7() && tv_mode == 0)
		return true;
	else
		return false;
}

static bool is_meson_t3(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_T3)
		return true;
	else
		return false;
}

static bool is_meson_t3_tvmode(void)
{
	if (is_meson_t3() && tv_mode == 1)
		return true;
	else
		return false;
}

static bool is_meson_t5w(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_T5W)
		return true;
	else
		return false;
}

static bool is_meson_tvmode(void)
{
	if (is_meson_tm2_tvmode() ||
	    is_meson_txlx_tvmode() ||
	    is_meson_t7_tvmode() ||
	    is_meson_t3_tvmode() ||
	    is_meson_t5w())
		return true;
	else
		return false;
}

static bool is_meson_stb_hdmimode(void)
{
	if ((is_meson_tm2_stbmode() || is_meson_t7_stbmode()) &&
	    module_installed && tv_dovi_setting &&
	    tv_dovi_setting->input_mode == IN_MODE_HDMI &&
	    hdmi_to_stb_policy)
		return true;
	else
		return false;
}

static int is_graphics_output_off(void)
{
	if (is_meson_g12() || is_meson_tm2_stbmode() ||
	    is_meson_t7_stbmode() || is_meson_sc2() ||
	    is_meson_s4d())
		return !(READ_VPP_REG(OSD1_BLEND_SRC_CTRL) & (0xf << 8)) &&
		!(READ_VPP_REG(OSD2_BLEND_SRC_CTRL) & (0xf << 8));
	else
		return (!(READ_VPP_REG(VPP_MISC) & (1 << 12)));
}

static u32 CORE1A_BASE;
static u32 CORE2A_BASE;
static u32 CORE3_BASE;
static u32 CORETV_BASE;
static u32 CORE1B_BASE;
static u32 CORE1C_BASE;
static u32 CORE2C_BASE;

static void dolby_vision_addr(void)
{
	if (is_meson_gxm() || is_meson_g12()) {
		CORE1A_BASE = 0x3300;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
	} else if (is_meson_txlx()) {
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
		CORETV_BASE = 0x3300;
	} else if (is_meson_tm2()) {
		CORE1A_BASE = 0x3300;
		CORE1B_BASE = 0x4400;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
		CORETV_BASE = 0x4300;
	} else if (is_meson_sc2()) {
		CORE1A_BASE = 0x3300;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
	} else if (is_meson_t7()) {
		CORE1A_BASE = 0x3300;
		CORE1B_BASE = 0x4400;
		CORE1C_BASE = 0x6000;
		CORE2A_BASE = 0x3400;
		CORE2C_BASE = 0x6100;
		CORE3_BASE = 0x3600;
		CORETV_BASE = 0x4300;
	} else if (is_meson_t3()) {
		CORETV_BASE = 0x4300;
	} else if (is_meson_s4d()) {
		CORE1A_BASE = 0x3300;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
	} else if (is_meson_t5w()) {
		CORETV_BASE = 0x4300;
	}

}
static u32 addr_map(u32 adr)
{
	if (adr & CORE1A_OFFSET)
		adr = (adr & 0xffff) + CORE1A_BASE;
	else if (adr & CORE1B_OFFSET)
		adr = (adr & 0xffff) + CORE1B_BASE;
	else if (adr & CORE1C_OFFSET)
		adr = (adr & 0xffff) + CORE1C_BASE;
	else if (adr & CORE2A_OFFSET)
		adr = (adr & 0xffff) + CORE2A_BASE;
	else if (adr & CORE2C_OFFSET)
		adr = (adr & 0xffff) + CORE2C_BASE;
	else if (adr & CORE3_OFFSET)
		adr = (adr & 0xffff) + CORE3_BASE;
	else if (adr & CORETV_OFFSET)
		adr = (adr & 0xffff) + CORETV_BASE;

	return adr;
}

static u32 VSYNC_RD_DV_REG(u32 adr)
{
	adr = addr_map(adr);
	return VSYNC_RD_MPEG_REG(adr);
}

static int VSYNC_WR_DV_REG(u32 adr, u32 val)
{
	adr = addr_map(adr);
	VSYNC_WR_MPEG_REG(adr, val);
	return 0;
}

static int VSYNC_WR_DV_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	adr = addr_map(adr);
	VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
	return 0;
}

static u32 READ_VPP_DV_REG(u32 adr)
{
	adr = addr_map(adr);
	return READ_VPP_REG(adr);
}

static int WRITE_VPP_DV_REG(u32 adr, const u32 val)
{
	adr = addr_map(adr);
	WRITE_VPP_REG(adr, val);
	return 0;
}

static int WRITE_VPP_DV_REG_BITS(u32 adr, const u32 val, u32 start, u32 len)
{
	adr = addr_map(adr);
	WRITE_VCBUS_REG_BITS(adr, val, start, len);
	return 0;
}

void amdolby_vision_wakeup_queue(void)
{
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;

	wake_up(&devp->dv_queue);
}

static unsigned int amdolby_vision_poll(struct file *file, poll_table *wait)
{
	struct amdolby_vision_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &devp->dv_queue, wait);
	mask = (POLLIN | POLLRDNORM);

	return mask;
}

static void dump_tv_setting
	(struct tv_dovi_setting_s *setting,
	 int frame_cnt, int debug_flag)
{
	int i;
	u64 *p;

	if ((debug_flag & 0x10) && dump_enable) {
		if (is_meson_txlx_tvmode()) {
			pr_info("txlx tv core1 reg: 0x1a07 val = 0x%x\n",
				READ_VPP_DV_REG(VIU_MISC_CTRL1));
		} else if (is_meson_t7_tvmode()) {
			pr_info("t7_tv reg: DOLBY SWAP_CTRL1 0x1a70 val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_SWAP_CTRL1));
			pr_info("t7_tv reg: DOLBY SWAP_CTRL2 0x1a71 val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_SWAP_CTRL2));
			pr_info("t7_tv reg: DOLBY tv core 0x1a83(bit4) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD1_DSC_CTRL));
			pr_info("t7_tv reg: vd2 dv 0x1a84(bit4) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD2_DSC_CTRL));
			pr_info("t7_tv reg: vd3 dv 0x1a85(bit4) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD3_DSC_CTRL));
		} else if (is_meson_t3_tvmode() ||
			is_meson_t5w()) {
			pr_info("t3/5w_tv reg: DOLBY TV select 0x2749(bit16) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_TOP_VTRL));
			pr_info("t3/5w_tv reg: DOLBY SWAP_CTRL1 0x1a70 val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_SWAP_CTRL1));
			pr_info("t3/5w_tv reg: DOLBY SWAP_CTRL2 0x1a71 val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_SWAP_CTRL2));
			pr_info("t3/5w_tv reg: tv core 0x1a73(bit16) val = 0x%x\n",
				READ_VPP_DV_REG(VIU_VD1_PATH_CTRL));
		} else if (is_meson_tm2_tvmode()) {
			pr_info("tm2_tv reg: tv core 0x1a0c(bit1/0) val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_CTRL));
		}

		pr_info("\nreg\n");
		p = (u64 *)&setting->core1_reg_lut[0];
		for (i = 0; i < 222; i += 2)
			pr_info("%016llx, %016llx,\n", p[i], p[i + 1]);
	}
	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ng2l_lut\n");
		p = (u64 *)&setting->core1_reg_lut[0];
		for (i = 222; i < 222 + 256; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i + 1]);
	}
	if ((debug_flag & 0x40) && dump_enable) {
		pr_info("\n3d_lut\n");
		p = (u64 *)&setting->core1_reg_lut[0];
		for (i = 222 + 256; i < 222 + 256 + 3276; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i + 1]);
		pr_info("\n");
	}
}

void dolby_vision_update_pq_config(char *pq_config_buf)
{
	memcpy((struct pq_config *)pq_config_fake,
		pq_config_buf, sizeof(struct pq_config));
	pr_info("update_pq_config[%zu] %x %x %x %x\n",
		sizeof(struct pq_config),
		pq_config_buf[1],
		pq_config_buf[2],
		pq_config_buf[3],
		pq_config_buf[4]);
	pq_config_set_flag = true;
}

void dolby_vision_update_vsvdb_config(char *vsvdb_buf, u32 tbl_size)
{
	if (tbl_size > sizeof(new_dovi_setting.vsvdb_tbl)) {
		pr_info("update_vsvdb_config tbl size overflow %d\n", tbl_size);
		return;
	}
	memset(&new_dovi_setting.vsvdb_tbl[0],
		0, sizeof(new_dovi_setting.vsvdb_tbl));
	memcpy(&new_dovi_setting.vsvdb_tbl[0],
		vsvdb_buf, tbl_size);
	new_dovi_setting.vsvdb_len = tbl_size;
	new_dovi_setting.vsvdb_changed = 1;
	dolby_vision_set_toggle_flag(1);
	if (tbl_size >= 8)
		pr_info
		("update_vsvdb_config[%d] %x %x %x %x %x %x %x %x\n",
		 tbl_size,
		 vsvdb_buf[0],
		 vsvdb_buf[1],
		 vsvdb_buf[2],
		 vsvdb_buf[3],
		 vsvdb_buf[4],
		 vsvdb_buf[5],
		 vsvdb_buf[6],
		 vsvdb_buf[7]);
	vsvdb_config_set_flag = true;
}

static int prepare_stb_dolby_core1_reg
	(u32 run_mode,
	 u32 *p_core1_dm_regs,
	 u32 *p_core1_comp_regs)
{
	int index = 0;
	int i;

	/* 4 */
	stb_core1_lut[index++] = ((u64)4 << 32) | 4;
	stb_core1_lut[index++] = ((u64)4 << 32) | 4;
	stb_core1_lut[index++] = ((u64)3 << 32) | 1;
	stb_core1_lut[index++] = ((u64)2 << 32) | 1;

	/* 1 + 14 + 10 + 1 */
	stb_core1_lut[index++] = ((u64)1 << 32) | run_mode;
	for (i = 0; i < 14; i++)
		stb_core1_lut[index++] =
			((u64)(6 + i) << 32)
			| p_core1_dm_regs[i];
	for (i = 17; i < 27; i++)
		stb_core1_lut[index++] =
			((u64)(6 + i) << 32)
			| p_core1_dm_regs[i - 3];
	stb_core1_lut[index++] = ((u64)(6 + 27) << 32) | 0;

	/* 173 + 1 */
	for (i = 0; i < 173; i++)
		stb_core1_lut[index++] =
			((u64)(6 + 44 + i) << 32)
			| p_core1_comp_regs[i];
	stb_core1_lut[index++] = ((u64)3 << 32) | 1;

	if (index & 1) {
		pr_dolby_error("stb core1 reg tbl odd size\n");
		stb_core1_lut[index++] = ((u64)3 << 32) | 1;
	}
	return index;
}

static void prepare_stb_dolby_core1_lut(u32 base, u32 *p_core1_lut)
{
	u32 *p_lut;
	int i;

	p_lut = &p_core1_lut[256 * 4]; /* g2l */
	for (i = 0; i < 128; i++) {
		stb_core1_lut[base + i] =
		stb_core1_lut[base + i + 128] =
			((u64)p_lut[1] << 32) |
			((u64)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
	p_lut = &p_core1_lut[0]; /* 4 lut */
	for (i = 256; i < 768; i++) {
		stb_core1_lut[base + i] =
			((u64)p_lut[1] << 32) |
			((u64)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
}

static bool skip_cvm_tbl[2][2][4][4] = {
	{ /* core1: video */
		{ /* video priority */
			{1, 1, 0, 0}, /* dv in */
			{1, 1, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		},
		{ /* graphic priority */
			{0, 0, 0, 0}, /* dv in */
			{0, 0, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		}
	},
	{ /* core2: graphic */
		{ /* video priority */
			{0, 0, 0, 0}, /* dv in */
			{0, 0, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		},
		{ /* graphic priority */
			{0, 0, 0, 0}, /* dv in */
			{0, 0, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		}
	}
};

static bool need_skip_cvm(unsigned int is_graphic)
{
	if (dolby_vision_flags & FLAG_CERTIFICAION)
		return false;
	if (dolby_vision_flags & FLAG_FORCE_CVM)
		return false;

	return skip_cvm_tbl[is_graphic]
		[dolby_vision_graphics_priority]
		[new_dovi_setting.src_format == FORMAT_INVALID ?
			FORMAT_SDR : new_dovi_setting.src_format]
		[new_dovi_setting.dovi_ll_enable ?
			FORMAT_DOVI_LL : new_dovi_setting.dst_format];
}

static int stb_dolby_core1_set
	(u32 dm_count,
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
	 bool dovi_src,
	 int lut_endian,
	 bool reset)
{
	u32 bypass_flag = 0;
	int composer_enable = el_enable;
	u32 run_mode = 0;
	int reg_size = 0;
	bool bypass_core1 = (!hsize || !vsize ||
				!(dolby_vision_mask & 1));

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	WRITE_VPP_DV_REG(DOLBY_TV_CLKGATE_CTRL, 0x2800);
	if (reset) {
		if (!dolby_vision_core1_on) {
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 9);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
			VSYNC_WR_DV_REG(DOLBY_TV_CLKGATE_CTRL, 0x2800);
		} else {
			reset = 0;
		}
	}

	if (!bl_enable)
		VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL5, 0x446);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL0, 0);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL3,
		(hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL4,
		(hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL2,
		(hsize << 16) | vsize);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL6, 0xba000000);
	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;
	VSYNC_WR_DV_REG
		(DOLBY_TV_SWAP_CTRL0,
		 /* el dly 3, bl dly 1 after de*/
		 (el_41_mode ? (0x3 << 4) : (0x1 << 8)) |
		 bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);

	if (el_enable && (dolby_vision_mask & 1))
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			 /* vd2 to core1 */
			 0, 17, 1);
	else
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			 /* vd2 to vpp */
			 1, 17, 1);
	if (dolby_vision_core1_on && !bypass_core1)
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			 /* enable core 1 */
			 0, 16, 1);
	else if (dolby_vision_core1_on && bypass_core1)
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
	/* bypass composer to get 12bit when SDR and HDR source */
#ifdef OLD_VERSION
	if (!dovi_src)
		bypass_flag |= 1 << 14; /* bypass composer */
#endif
	if (dolby_vision_run_mode != 0xff) {
		run_mode = dolby_vision_run_mode;
	} else {
		run_mode = (0x7 << 6) |
			((el_41_mode ? 3 : 1) << 2) |
			bypass_flag;
		if (dolby_vision_on_count < dolby_vision_run_mode_delay) {
			run_mode |= 1;
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
					(0x200 << 10) | 0x200);
		} else if (dolby_vision_on_count ==
			dolby_vision_run_mode_delay) {
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
					(0x200 << 10) | 0x200);
		} else {
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1, 0);
		}
	}
	if (reset)
		VSYNC_WR_DV_REG(DOLBY_TV_REG_START + 1, run_mode);

	/* 962e work around to fix the uv swap issue when bl:el = 1:1 */
	if (el_41_mode)
		VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL5, 0x6);
	else
		VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL5, 0xa);

	/* axi dma for reg table */
	reg_size = prepare_stb_dolby_core1_reg
			(run_mode, p_core1_dm_regs, p_core1_comp_regs);
	/* axi dma for lut table */
	prepare_stb_dolby_core1_lut(reg_size, p_core1_lut);

	if (!dolby_vision_on) {
		/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
		WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL1,
				 0x00000080 | (reg_size << 23));
		WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		/* dma3:23-12 cvm size */
		WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL3,
				 0x80100000 | dma_start_line);
		WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000062);
		WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}
	if (reset) {
		/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
		VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL1,
				0x00000080 | (reg_size << 23));
		VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		/* dma3:23-12 cvm size */
		VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL3,
				0x80100000 | dma_start_line);
		VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000062);
		VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}

	tv_dovi_setting_update_flag = true;
	return 0;
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

static void adjust_vpotch(u32 graphics_w, u32 graphics_h)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	int sync_duration_num = 60;

	if (is_meson_txlx_stbmode()) {
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
	} else if (is_meson_g12()) {
		if (vinfo) {
			if (vinfo->sync_duration_den)
				sync_duration_num = vinfo->sync_duration_num /
						    vinfo->sync_duration_den;
			if (debug_dolby & 2)
				pr_dolby_dbg("vinfo %d %d %d %d %d %d\n",
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
			else
				g_vpotch = 0x20;
			if (vinfo->width > 1920)
				htotal_add = 0xc0;
			else
				htotal_add = 0x140;
		} else {
			g_vpotch = 0x20;
		}

	} else if (is_meson_tm2_stbmode() || is_meson_t7_stbmode() ||
	is_meson_sc2() || is_meson_s4d()) {
		if (vinfo) {
			if (debug_dolby & 2)
				pr_dolby_dbg("vinfo %d %d %d\n",
					     vinfo->width,
					     vinfo->height,
					     vinfo->field_height);
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

			/* for t7 4k fb */
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

static void adjust_vpotch_tv(void)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (is_meson_tm2() || is_meson_t7() ||
	    is_meson_t3() || is_meson_t5w()) {
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

static void dolby_core_reset(enum core_type type)
{
	switch (type) {
	case DOLBY_TVCORE:
		if (is_meson_txlx())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 9);
		else if (is_meson_tm2() || is_meson_t7() ||
			 is_meson_t3() || is_meson_t5w())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 1);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		break;
	case DOLBY_CORE1A:
		if (is_meson_txlx())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 10);
		else if (is_meson_g12())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 2);
		else if (is_meson_tm2() || is_meson_sc2() ||
			 is_meson_s4d() || is_meson_t7() ||
			 is_meson_t3())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 30);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		break;
	case DOLBY_CORE1B:
		if (is_meson_txlx())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 10);
		else if (is_meson_g12())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 3);
		else if (is_meson_tm2() || is_meson_sc2() ||
			 is_meson_s4d() || is_meson_t7() ||
			 is_meson_t3())
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 31);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		break;
	case DOLBY_CORE1C:
		if (is_meson_t7() || is_meson_t3()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 1 << 2);
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 0);
		}
		break;
	case DOLBY_CORE2A:
		if (is_meson_tm2() || is_meson_sc2() ||
		    is_meson_s4d() || is_meson_t7() || is_meson_t3()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 2);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		}
		break;
	case DOLBY_CORE2B:
		if (is_meson_tm2() || is_meson_sc2() ||
		    is_meson_s4d() || is_meson_t7() || is_meson_t3()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 3);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		}
		break;
	case DOLBY_CORE2C:
		if (is_meson_t7() || is_meson_t3()) {
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 1 << 0);
			VSYNC_WR_DV_REG(VIU_SW_RESET0, 0);
		}
		break;
	default:
		pr_debug("error core type %d\n", type);
		break;
	return;
	}
}

static int tv_dolby_core1_set
	(u64 *dma_data,
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
	bool bypass_core1 = (!hsize || !vsize ||
				!(dolby_vision_mask & 1));
	static int start_render;

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (is_meson_t3() || is_meson_t5w()) {
		VSYNC_WR_DV_REG_BITS(VPP_TOP_VTRL, 0, 0, 1); //DOLBY TV select
		//T3 enable tvcore clk
		if (!dolby_vision_on) {/*enable once*/
			vpu_module_clk_enable(0, DV_TVCORE, 1);
			vpu_module_clk_enable(0, DV_TVCORE, 0);
		}
	}

	adjust_vpotch_tv();
	if (is_meson_tm2() || is_meson_t7() ||
	    is_meson_t3() || is_meson_t5w()) {
		/* mempd for ipcore */
		if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
			if (get_dv_vpu_mem_power_status(VPU_DOLBY0) ==
			    VPU_MEM_POWER_ON)
				dv_mem_power_off(VPU_DOLBY0);
			VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL7, 0xf, 4, 4);
		} else {
			if (get_dv_vpu_mem_power_status(VPU_DOLBY0) ==
				VPU_MEM_POWER_DOWN ||
				get_dv_mem_power_flag(VPU_DOLBY0) ==
				VPU_MEM_POWER_DOWN)
				dv_mem_power_on(VPU_DOLBY0);
			VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL7, 0, 4, 9);
			if (is_meson_tm2_revb() || is_meson_t7() ||
			    is_meson_t3() || is_meson_t5w()) {
				/* comp on, mempd on */
				VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL7,
						     0, 14, 4);
			}
		}
	}
	WRITE_VPP_DV_REG(DOLBY_TV_CLKGATE_CTRL, 0x2800);

	if (reset) {
		dolby_core_reset(DOLBY_TVCORE);
		VSYNC_WR_DV_REG(DOLBY_TV_CLKGATE_CTRL, 0x2800);
	}

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;
	VSYNC_WR_DV_REG
		(DOLBY_TV_SWAP_CTRL0,
		 /* el dly 3, bl dly 1 after de*/
		 (el_41_mode ? (0x3 << 4) : (0x1 << 8)) |
		 bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL1,
			((hsize + 0x80) << 16 | (vsize + 0x40)));
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(DOLBY_TV_SWAP_CTRL2, (hsize << 16) | vsize);
	/*0x2c2d0:5-4-1-3-2-0*/
	VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0x2c2d0, 14, 18);
	VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0xa, 0, 4);

	if (hdmi && !hdr10) {
		/*hdmi DV STD and DV LL:  need detunnel*/
		VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL5, 1, 4, 1);
	} else {
		VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0, 4, 1);
	}

	/*set diag reg to 0xb can bypass dither, not need set swap ctrl6 */
	if (!is_meson_tm2() && !is_meson_t7() &&
	    !is_meson_t3() &&
	    !is_meson_t5w()) {
		VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 20, 1);
		/* bypass dither */
		VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 25, 1);
	}
	if (src_chroma_format == 2)
		VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 29, 1);
	else if (src_chroma_format == 1)
		VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL6, 0, 29, 1);
	/* input 12 or 10 bit */
	VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL7, 12, 0, 4);

	if (el_enable && (dolby_vision_mask & 1))
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			 /* vd2 to core1 */
			 0, 17, 1);
	else
		VSYNC_WR_DV_REG_BITS
			(VIU_MISC_CTRL1,
			 /* vd2 to vpp */
			 1, 17, 1);

	if (dolby_vision_core1_on &&
	    !bypass_core1) {
		if (is_meson_tm2_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 1, 8, 2);
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 1, 10, 2);
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 1, 24, 2);
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 0, 16, 1);
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 0, 20, 1);
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 el_enable ? 0 : 2, 0, 2);
		} else if (is_meson_t7_tvmode() ||
			is_meson_t3_tvmode() || is_meson_t5w()) {
			/*enable tv core*/
			if (is_meson_t7_tvmode())
				VSYNC_WR_DV_REG_BITS
					(VPP_VD1_DSC_CTRL,
					 0, 4, 1);
			else
				VSYNC_WR_DV_REG_BITS
					(VIU_VD1_PATH_CTRL,
					 0, 16, 1);

			/*vd1 to tvcore*/
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_SWAP_CTRL1,
				 1, 0, 3);
			/*tvcore bl in sel vd1*/
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_SWAP_CTRL2,
				 0, 6, 2);
			/*tvcore el in sel null*/
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_SWAP_CTRL2,
				 3, 8, 2);
			/*tvcore bl to vd1*/
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_SWAP_CTRL2,
				 0, 20, 2);
			/* vd1 from tvcore*/
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_SWAP_CTRL1,
				 1, 12, 3);
		} else {
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				 /* enable core 1 */
				 0, 16, 1);
		}
	} else if (dolby_vision_core1_on &&
	    bypass_core1) {
		if (is_meson_tm2_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 3, 0, 2);
		} else if (is_meson_t7_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(VPP_VD1_DSC_CTRL,
				 1, 4, 1);
		} else if (is_meson_t3_tvmode()) {
			VSYNC_WR_DV_REG_BITS
				(VIU_VD1_PATH_CTRL,
				 1, 16, 1);
		} else if (is_meson_t5w()) {
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

	if (dolby_vision_run_mode != 0xff) {
		run_mode = dolby_vision_run_mode;
	} else {
		if (debug_dolby & 8)
			pr_dolby_dbg("%s: dolby_vision_on_count %d\n",
				     __func__, dolby_vision_on_count);
		run_mode = tv_run_mode(vsize, hdmi, hdr10, el_41_mode);
		if (dolby_vision_on_count < dolby_vision_run_mode_delay) {
			run_mode = (run_mode & 0xfffffffc) | 1;
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x200 << 10) | 0x200);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1,
					(0x200 << 10) | 0x200);
			start_render = 0;
		} else if (dolby_vision_on_count ==
			   dolby_vision_run_mode_delay) {
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
		VSYNC_WR_DV_REG(DOLBY_TV_REG_START + 1, run_mode);
	if (is_meson_tm2_stbmode() || is_meson_t7_stbmode())
		VSYNC_WR_DV_REG(DOLBY_TV_DIAG_CTRL, 1);

	if (!dolby_vision_on ||
	(!dolby_vision_core1_on &&
	(is_meson_tm2_stbmode() || is_meson_t7_stbmode()))) {
		WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
		if (is_meson_t7() || is_meson_t3() || is_meson_t5w())
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)(dma_paddr >> 4));

		if (is_meson_t3() || is_meson_t5w())
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL3,
				 0x88000000 | dma_start_line);
		else
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL3,
				 0x80000000 | dma_start_line);
		if (is_meson_t7()) {
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000040);
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400040);
		} else {
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000042);
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
		}
	}
	if (reset) {
		VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
		if (is_meson_t7() || is_meson_t3() || is_meson_t5w())
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)(u32)(dma_paddr >> 4));
		if (is_meson_t3() || is_meson_t5w())
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL3,
				0x88000000 | dma_start_line);
		else
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL3,
				0x80000000 | dma_start_line);
		if (is_meson_t7()) {
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000040);
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400040);
		} else {
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000042);
			VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
		}
	}

	tv_dovi_setting_update_flag = true;
	return 0;
}

int dolby_vision_update_setting(void)
{
	u64 *dma_data;
	u32 size = 0;
	int i;
	u64 *p;

	if (!p_funcs_stb && !p_funcs_tv)
		return -1;
	if (!tv_dovi_setting_update_flag)
		return 0;
	if (dolby_vision_flags &
		FLAG_DISABLE_DMA_UPDATE) {
		tv_dovi_setting_update_flag = false;
		setting_update_count++;
		return -1;
	}
	if (!dma_vaddr)
		return -1;
	if (efuse_mode == 1) {
		tv_dovi_setting_update_flag = false;
		setting_update_count++;
		return -1;
	}
	if (is_meson_tm2_tvmode() ||
	    is_meson_t3_tvmode() ||
	    is_meson_t5w() ||
	    (is_meson_tm2_stbmode() && is_meson_stb_hdmimode())) {
		dma_data = tv_dovi_setting->core1_reg_lut;
		size = 8 * TV_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	} else if (is_meson_t7_tvmode() ||
		(is_meson_t7_stbmode() && is_meson_stb_hdmimode())) {
		dma_data = tv_dovi_setting->core1_reg_lut;
		size = 8 * TV_DMA_TBL_SIZE * 16;
		p = (u64 *)dma_vaddr;
		/*Write 128bit, DMA address jump 128bit * 16, then write 128bit */
		for (i = 0; i < TV_DMA_TBL_SIZE; i += 2) {
			memcpy((void *)p, dma_data, 16);
			dma_data += 2;
			p += 2 * 16;
		}
	} else if (is_meson_txlx_stbmode()) {
		dma_data = stb_core1_lut;
		size = 8 * STB_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	}
	if (size && (debug_dolby & 0x800)) {
		p = (u64 *)dma_vaddr;
		pr_info("dma_vaddr %p, dma size %d\n", dma_vaddr, TV_DMA_TBL_SIZE);
		for (i = 0; i < size / 8; i += 2)
			pr_info("%016llx, %016llx\n", p[i], p[i + 1]);
	}
	tv_dovi_setting_update_flag = false;
	setting_update_count = frame_count;
	return -1;
}
EXPORT_SYMBOL(dolby_vision_update_setting);

/*update timing to 1080p if size < 1080p*/
void update_dvcore2_timing(u32 *hsize, u32 *vsize)
{
	if (hsize && vsize &&
	    !(dolby_vision_flags & FLAG_CERTIFICAION) &&
	    !(dolby_vision_flags & FLAG_DEBUG_CORE2_TIMING) &&
	    *hsize < 1920 && *vsize < 1080) {
		*hsize = 1920;
		*vsize = 1080;
	}
}
EXPORT_SYMBOL(update_dvcore2_timing);

static int dolby_core1_set
	(u32 dm_count,
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
	 bool dovi_src,
	 int lut_endian,
	 bool reset)
{
	u32 count;
	u32 bypass_flag = 0;
	int composer_enable =
		bl_enable && el_enable && (dolby_vision_mask & 1);
	int i;
	bool set_lut = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg1;
	u32 *last_comp = (u32 *)&dovi_setting.comp_reg;
	bool bypass_core1 = (!hsize || !vsize ||
			     !(dolby_vision_mask & 1));
	int copy_core1a_to_core1b = ((copy_core1a & 1) &&
				(is_meson_tm2_stbmode() || is_meson_t7_stbmode()));
	int copy_core1a_to_core1c = ((copy_core1a & 2) && is_meson_t7_stbmode());

	/* G12A: make sure the BL is enable for the very 1st frame*/
	/* Register: dolby_path_ctrl[0] = 0 to enable BL*/
	/*           dolby_path_ctrl[1] = 0 to enable EL*/
	/*           dolby_path_ctrl[2] = 0 to enable OSD*/
	if ((is_meson_g12() || is_meson_tm2_stbmode() ||
	    is_meson_sc2() || is_meson_s4d()) &&
	    frame_count == 1 && dolby_vision_core1_on == 0) {
		pr_dolby_dbg("((%s %d, register DOLBY_PATH_CTRL: %x))\n",
			     __func__, __LINE__,
			     VSYNC_RD_DV_REG(DOLBY_PATH_CTRL));
		if ((VSYNC_RD_DV_REG(DOLBY_PATH_CTRL) & 0x1) != 0) {
			pr_dolby_dbg("BL is disable for 1st frame.Re-enable BL\n");
			VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 0, 1);
			pr_dolby_dbg("((%s %d, enable_bl, DOLBY_PATH_CTRL: %x))\n",
				     __func__, __LINE__,
				     VSYNC_RD_DV_REG(DOLBY_PATH_CTRL));
		}
		if (el_enable) {
			if ((VSYNC_RD_DV_REG(DOLBY_PATH_CTRL) & 0x10) != 0) {
				pr_dolby_dbg("((%s %d enable el))\n",
					     __func__, __LINE__);
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL,
						     0, 1, 1);
				pr_dolby_dbg("((%s %d, enable_el, DOLBY_PATH_CTRL: %x))\n",
					     __func__, __LINE__,
					     VSYNC_RD_DV_REG(DOLBY_PATH_CTRL));
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

	if (dolby_vision_on_count
		== dolby_vision_run_mode_delay)
		reset = true;

	if ((!dolby_vision_on || reset) && bl_enable) {
		dolby_core_reset(DOLBY_CORE1A);
		if (copy_core1a_to_core1b)
			dolby_core_reset(DOLBY_CORE1B);
		if (copy_core1a_to_core1c)
			dolby_core_reset(DOLBY_CORE1C);
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (dolby_vision_core1_on &&
	    dolby_vision_core1_on_cnt < DV_CORE1_RECONFIG_CNT) {
		reset = true;
		dolby_vision_core1_on_cnt++;
	}

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC)
		set_lut = true;

	if ((bl_enable && el_enable && (dolby_vision_mask & 1)) ||
	    (copy_core1a_to_core1b || copy_core1a_to_core1c)) {
		if (is_meson_g12() || is_meson_tm2_stbmode()) {
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 /* vd2 to core1a */
				 0, 1, 1);
		} else if (is_meson_t7_stbmode()) {
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
		if (is_meson_g12() || is_meson_sc2() ||
			is_meson_tm2_stbmode() || is_meson_s4d()) {
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				/* vd2 to vpp */
				1, 1, 1);
		} else if (is_meson_t7_stbmode()) {
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

	if ((is_meson_tm2_stbmode() ||
	    is_meson_g12() ||
	    is_meson_sc2() ||
	    is_meson_t7_stbmode() ||
	    is_meson_s4d()) && bl_enable) {
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

	VSYNC_WR_DV_REG(DOLBY_CORE1A_CLKGATE_CTRL, 0);
	/* VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL0, 0); */
	VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL1,
		((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL2, (hsize << 16) | vsize);
	VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL5, 0xa);

	VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(DOLBY_CORE1A_REG_START + 4, 4);
	VSYNC_WR_DV_REG(DOLBY_CORE1A_REG_START + 2, 1);

	if (copy_core1a_to_core1b) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY1B) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1B) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1B);
		VSYNC_WR_DV_REG(DOLBY_CORE1B_CLKGATE_CTRL, 0);
		/* VSYNC_WR_DV_REG(DOLBY_CORE1B_SWAP_CTRL0, 0); */
		VSYNC_WR_DV_REG(DOLBY_CORE1B_SWAP_CTRL1,
				((hsize + 0x80) << 16) | (vsize + 0x40));
		VSYNC_WR_DV_REG(DOLBY_CORE1B_SWAP_CTRL3,
				(hwidth << 16) | vwidth);
		VSYNC_WR_DV_REG(DOLBY_CORE1B_SWAP_CTRL4,
				(hpotch << 16) | vpotch);
		VSYNC_WR_DV_REG(DOLBY_CORE1B_SWAP_CTRL2,
				(hsize << 16) | vsize);
		VSYNC_WR_DV_REG(DOLBY_CORE1B_SWAP_CTRL5, 0xa);

		VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_CTRL, 0x0);
		VSYNC_WR_DV_REG(DOLBY_CORE1B_REG_START + 4, 4);
		VSYNC_WR_DV_REG(DOLBY_CORE1B_REG_START + 2, 1);
	}

	if (copy_core1a_to_core1c) {
		VSYNC_WR_DV_REG(DOLBY_CORE1C_CLKGATE_CTRL, 0);
		/* VSYNC_WR_DV_REG(DOLBY_CORE1C_SWAP_CTRL0, 0); */
		VSYNC_WR_DV_REG(DOLBY_CORE1C_SWAP_CTRL1,
				((hsize + 0x80) << 16) | (vsize + 0x40));
		VSYNC_WR_DV_REG(DOLBY_CORE1C_SWAP_CTRL3,
				(hwidth << 16) | vwidth);
		VSYNC_WR_DV_REG(DOLBY_CORE1C_SWAP_CTRL4,
				(hpotch << 16) | vpotch);
		VSYNC_WR_DV_REG(DOLBY_CORE1C_SWAP_CTRL2,
				(hsize << 16) | vsize);
		VSYNC_WR_DV_REG(DOLBY_CORE1C_SWAP_CTRL5, 0xa);

		VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_CTRL, 0x0);
		VSYNC_WR_DV_REG(DOLBY_CORE1C_REG_START + 4, 4);
		VSYNC_WR_DV_REG(DOLBY_CORE1C_REG_START + 2, 1);
	}

	/* bypass composer to get 12bit when SDR and HDR source */
#ifdef OLD_VERSION
	if (!dovi_src)
		bypass_flag |= 1 << 0;
#endif
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (need_skip_cvm(0))
		bypass_flag |= 1 << 2;
	if (el_41_mode)
		bypass_flag |= 1 << 3;

	VSYNC_WR_DV_REG(DOLBY_CORE1A_REG_START + 1,
			0x70 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_DV_REG(DOLBY_CORE1A_REG_START + 1,
			0x70 | bypass_flag); /* for delay */
	if (copy_core1a_to_core1b) {
		VSYNC_WR_DV_REG(DOLBY_CORE1B_REG_START + 1,
				0x70 | bypass_flag); /* bypass CVM and/or CSC */
		VSYNC_WR_DV_REG(DOLBY_CORE1B_REG_START + 1,
				0x70 | bypass_flag); /* for delay */
	}
	if (copy_core1a_to_core1c) {
		VSYNC_WR_DV_REG(DOLBY_CORE1C_REG_START + 1,
				0x70 | bypass_flag); /* bypass CVM and/or CSC */
		VSYNC_WR_DV_REG(DOLBY_CORE1C_REG_START + 1,
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
				(DOLBY_CORE1A_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG
				(DOLBY_CORE1B_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG
				(DOLBY_CORE1C_REG_START + 6 + i,
				 p_core1_dm_regs[i]);
		}

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		if (reset || p_core1_comp_regs[i] != last_comp[i]) {
			VSYNC_WR_DV_REG
				(DOLBY_CORE1A_REG_START + 6 + 44 + i,
				 p_core1_comp_regs[i]);
			if (copy_core1a_to_core1b)
				VSYNC_WR_DV_REG
					(DOLBY_CORE1B_REG_START + 6 + 44 + i,
					 p_core1_comp_regs[i]);
			if (copy_core1a_to_core1c)
				VSYNC_WR_DV_REG
					(DOLBY_CORE1C_REG_START + 6 + 44 + i,
					 p_core1_comp_regs[i]);
		}
	/* metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE1A_REG_START + 3, 1);
	if (copy_core1a_to_core1b)
		VSYNC_WR_DV_REG(DOLBY_CORE1B_REG_START + 3, 1);
	if (copy_core1a_to_core1c)
		VSYNC_WR_DV_REG(DOLBY_CORE1C_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset)) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(DOLBY_CORE1A_CLKGATE_CTRL,
					     2, 2, 2);
#endif

		VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_CTRL, 0x1401);
		if (copy_core1a_to_core1b)
			VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_CTRL, 0x1401);
		if (copy_core1a_to_core1c)
			VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_CTRL, 0x1401);
		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_PORT,
						p_core1_lut[i + 3]);
				VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_PORT,
						p_core1_lut[i + 2]);
				VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_PORT,
						p_core1_lut[i + 1]);
				VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_PORT,
						p_core1_lut[i]);
				if (copy_core1a_to_core1b) {
					VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_PORT,
							p_core1_lut[i + 3]);
					VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_PORT,
							p_core1_lut[i + 2]);
					VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_PORT,
							p_core1_lut[i + 1]);
					VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_PORT,
							p_core1_lut[i]);
				}
				if (copy_core1a_to_core1c) {
					VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_PORT,
							p_core1_lut[i + 3]);
					VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_PORT,
							p_core1_lut[i + 2]);
					VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_PORT,
							p_core1_lut[i + 1]);
					VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_PORT,
							p_core1_lut[i]);
				}
			}
		} else {
			for (i = 0; i < count; i++)
				VSYNC_WR_DV_REG(DOLBY_CORE1A_DMA_PORT,
						p_core1_lut[i]);
			if (copy_core1a_to_core1b) {
				for (i = 0; i < count; i++)
					VSYNC_WR_DV_REG(DOLBY_CORE1B_DMA_PORT,
							p_core1_lut[i]);
			}
			if (copy_core1a_to_core1c) {
				for (i = 0; i < count; i++)
					VSYNC_WR_DV_REG(DOLBY_CORE1C_DMA_PORT,
							p_core1_lut[i]);
			}
		}
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(DOLBY_CORE1A_CLKGATE_CTRL,
					     0, 2, 2);
#endif
	}

	if (dolby_vision_on_count
		< dolby_vision_run_mode_delay) {
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC0,
			 (0x200 << 10) | 0x200);
		VSYNC_WR_DV_REG
			(VPP_VD1_CLIP_MISC1,
			 (0x200 << 10) | 0x200);
		if (is_meson_g12())
			VSYNC_WR_DV_REG_BITS
				(DOLBY_PATH_CTRL,
				 1,
				 0, 1);
		else
			VSYNC_WR_DV_REG_BITS
				(VIU_MISC_CTRL1,
				 1, 16, 1);
	} else {
		if (dolby_vision_on_count >
			dolby_vision_run_mode_delay) {
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC0,
				 (0x3ff << 20) |
				 (0x3ff << 10) |
				 0x3ff);
			VSYNC_WR_DV_REG
				(VPP_VD1_CLIP_MISC1,
				 0);
		}
		if (dolby_vision_core1_on &&
		    !bypass_core1) {
			if (is_meson_g12()) {
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 0, 1);
			} else if (is_meson_tm2_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 8, 2);
				if (copy_core1a_to_core1b)
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 3, 10, 2);
				else
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 0, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 24, 2);
				if (copy_core1a_to_core1b) {
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 1, 19, 1);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 1, 23, 1);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 3, 26, 2);
				}
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					0, 0, 1); /* core1 */
			} else if (is_meson_t7_stbmode()) {
				/* vd1 to core1a*/
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_SWAP_CTRL1,
					 0, 0, 3);
				/*core1a bl in sel vd1*/
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_SWAP_CTRL2,
					 0, 2, 2);
				/*core1a el in sel null*/
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_SWAP_CTRL2,
					 3, 4, 2);
				/*core1a out to vd1*/
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_SWAP_CTRL2,
					 0, 18, 2);
				/* vd1 from core1a*/
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_SWAP_CTRL1,
					 0, 12, 3);
				/*enable core1a*/
				VSYNC_WR_DV_REG_BITS
					(VPP_VD1_DSC_CTRL,
					 0, 4, 1);
				if (copy_core1a_to_core1b) {
					/*vd2 to core1b*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 3, 4, 3);
					/*core1b in sel vd2*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 1, 10, 2);
					/*core1b out to vd2*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 1, 22, 2);
					/* vd2 from core1b*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 3, 16, 3);
					/*enable core1b*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD2_DSC_CTRL,
						 0, 4, 1);
				}

				if (copy_core1a_to_core1c) {
					/*core1c to vd3*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 4, 8, 3);
					/*core1c in sel vd3*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 2, 12, 2);
					/*core1c out to vd3*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 2, 24, 2);
					/*vd3 from core1c*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 4, 20, 3);

					/*enable core1c*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD3_DSC_CTRL,
						 0, 4, 1);
				}
			} else if (is_meson_sc2() || is_meson_s4d()) {
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 8, 2);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 10, 2);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 17, 1);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 21, 1);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 24, 2);
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 0, 0, 1); /* core1 */
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* enable core 1 */
					 0, 16, 1);
			}
		} else if (dolby_vision_core1_on &&
			bypass_core1) {
			if (is_meson_g12())
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 1, 0, 1);
			else if (is_meson_tm2_stbmode() || is_meson_t7_stbmode())
				VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL,
					 3, 0, 2); /* core1 */
			else
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* bypass core 1 */
					 1, 16, 1);
		}
	}

	if (is_meson_g12() || is_meson_tm2_stbmode() ||
	    is_meson_t7_stbmode() || is_meson_sc2() ||
	    is_meson_s4d()) {
		VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL0,
			(el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
			bl_enable | composer_enable << 1 | el_41_mode << 2);
		if (copy_core1a_to_core1b) {
			VSYNC_WR_DV_REG
				(DOLBY_CORE1B_SWAP_CTRL0,
				 (el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
				 bl_enable | composer_enable << 1 |
				 el_41_mode << 2);
		}
		if (copy_core1a_to_core1c) {
			VSYNC_WR_DV_REG
				(DOLBY_CORE1C_SWAP_CTRL0,
				 (el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
				 bl_enable | composer_enable << 1 |
				 el_41_mode << 2);
		}
	} else {
	/* enable core1 */
		VSYNC_WR_DV_REG(DOLBY_CORE1A_SWAP_CTRL0,
				bl_enable << 0 |
				composer_enable << 1 |
				el_41_mode << 2);
	}
	tv_dovi_setting_update_flag = true;
	return 0;
}

static int dolby_core2c_set
	(u32 dm_count,
	 u32 lut_count,
	 u32 *p_core2_dm_regs,
	 u32 *p_core2_lut,
	 int hsize,
	 int vsize,
	 int dolby_enable,
	 int lut_endian)
{
	u32 count;
	int i;
	bool set_lut = false;
	bool reset = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg2;
	u32 bypass_flag = 0;

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on || force_reset_core2) {
		dolby_core_reset(DOLBY_CORE2C);
		force_reset_core2 = false;
		reset = true;
	}

	if (dolby_vision_on &&
	    dolby_vision_core2_on_cnt < DV_CORE2_RECONFIG_CNT) {
		reset = true;
		dolby_vision_core2_on_cnt++;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (is_meson_gxm()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if ((first_reseted & 0x1) == 0) {
			first_reseted = (first_reseted | 0x1);
			reset = true;
		}
#endif
	} else {
		if (dolby_vision_on_count == 0)
			reset = true;
	}

	if (force_update_reg & 2)
		set_lut = true;

	if (force_update_reg & 4)
		reset = true;

	if (is_meson_tm2_stbmode() ||
	    is_meson_g12() ||
	    is_meson_sc2() ||
	    is_meson_t7_stbmode() ||
	    is_meson_s4d()) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY2) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY2) == VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY2);
	}

	VSYNC_WR_DV_REG(DOLBY_CORE2C_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE2C_SWAP_CTRL0, 0);
	if (is_meson_t7_stbmode() || reset) {
		VSYNC_WR_DV_REG
			(DOLBY_CORE2C_SWAP_CTRL1,
			 ((hsize + g_htotal_add) << 16) | (vsize
			 + ((g_vtiming & 0xff000000) ?
			 ((g_vtiming >> 24) & 0xff) : g_vtotal_add)
			 + ((g_vtiming & 0xff0000) ?
			 ((g_vtiming >> 16) & 0xff) : g_vsize_add)));
		VSYNC_WR_DV_REG
			(DOLBY_CORE2C_SWAP_CTRL2,
			 (hsize << 16) | (vsize
			 + ((g_vtiming & 0xff0000) ?
			 ((g_vtiming >> 16) & 0xff) : g_vsize_add)));
	}
	VSYNC_WR_DV_REG(DOLBY_CORE2C_SWAP_CTRL3,
			(g_hwidth << 16) | ((g_vtiming & 0xff00) ?
			((g_vtiming >> 8) & 0xff) : g_vwidth));
	VSYNC_WR_DV_REG(DOLBY_CORE2C_SWAP_CTRL4,
			(g_hpotch << 16) | ((g_vtiming & 0xff) ?
			(g_vtiming & 0xff) : g_vpotch));

	VSYNC_WR_DV_REG(DOLBY_CORE2C_SWAP_CTRL5,  0xa8000000);

	VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(DOLBY_CORE2C_REG_START + 2, 1);
	if (need_skip_cvm(1))
		bypass_flag |= 1 << 0;
	VSYNC_WR_DV_REG(DOLBY_CORE2C_REG_START + 2, 1);
	VSYNC_WR_DV_REG(DOLBY_CORE2C_REG_START + 1,
			2 | bypass_flag);
	VSYNC_WR_DV_REG(DOLBY_CORE2C_REG_START + 1,
			2 | bypass_flag);
	VSYNC_WR_DV_REG(DOLBY_CORE2C_CTRL, 0);

	VSYNC_WR_DV_REG(DOLBY_CORE2C_CTRL, 0);

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		if (reset ||
		    p_core2_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(DOLBY_CORE2C_REG_START + 6 + i,
				 p_core2_dm_regs[i]);
			set_lut = true;
		}

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2)
		set_lut = true;
	else if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
		set_lut = false;

	if (debug_dolby & 2)
		pr_dolby_dbg("core2a g_potch %x %x, reset %d, set_lut %d, flag %x\n",
			     g_hpotch, g_vpotch, reset, set_lut,
			     stb_core_setting_update_flag);

	/* core2 metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE2C_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset || force_set_lut)) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(DOLBY_CORE2C_CLKGATE_CTRL,
				2, 2, 2);
#endif
		VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_CTRL, 0x1401);
		if (lut_endian)
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_PORT,
					p_core2_lut[i + 3]);
				VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_PORT,
					p_core2_lut[i + 2]);
				VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_PORT,
					p_core2_lut[i + 1]);
				VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_PORT,
					p_core2_lut[i]);
			}
		else
			for (i = 0; i < count; i++)
				VSYNC_WR_DV_REG(DOLBY_CORE2C_DMA_PORT,
					p_core2_lut[i]);
		/* core2 lookup table program done */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS
				(DOLBY_CORE2C_CLKGATE_CTRL, 0, 2, 2);
#endif
	}
	force_set_lut = false;

	/* enable core2c */
	VSYNC_WR_DV_REG(DOLBY_CORE2C_SWAP_CTRL0, dolby_enable << 0);
	return 0;
}

static int dolby_core2a_set
	(u32 dm_count,
	 u32 lut_count,
	 u32 *p_core2_dm_regs,
	 u32 *p_core2_lut,
	 int hsize,
	 int vsize,
	 int dolby_enable,
	 int lut_endian)
{
	u32 count;
	int i;
	bool set_lut = false;
	bool reset = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg2;
	u32 bypass_flag = 0;
	u32 tmp_h = hsize;
	u32 tmp_v = vsize;

	if (dolby_vision_on &&
	    (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on || force_reset_core2) {
		dolby_core_reset(DOLBY_CORE2A);
		force_reset_core2 = false;
		reset = true;
	}
	if (dolby_vision_on &&
	    dolby_vision_core2_on_cnt < DV_CORE2_RECONFIG_CNT) {
		reset = true;
		dolby_vision_core2_on_cnt++;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (is_meson_gxm()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if ((first_reseted & 0x1) == 0) {
			first_reseted = (first_reseted | 0x1);
			reset = true;
		}
#endif
	} else {
		if (dolby_vision_on_count == 0)
			reset = true;
	}

	if (force_update_reg & 0x10)
		set_lut = true;

	if (force_update_reg & 2)
		reset = true;

	if (is_meson_tm2_stbmode() ||
	    is_meson_g12() ||
	    is_meson_sc2() ||
	    is_meson_t7_stbmode() ||
	    is_meson_s4d()) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY2) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY2) == VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY2);
	}

	VSYNC_WR_DV_REG(DOLBY_CORE2A_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL0, 0);

	if (is_meson_box() || is_meson_tm2_stbmode() ||
	    is_meson_t7_stbmode() || reset) {
		/*update timing to 1080p if size < 1080p, otherwise display color dot*/
		update_dvcore2_timing(&tmp_h, &tmp_v);
		VSYNC_WR_DV_REG
			(DOLBY_CORE2A_SWAP_CTRL1,
			((tmp_h + g_htotal_add) << 16) | (tmp_v
			 + ((g_vtiming & 0xff000000) ?
			 ((g_vtiming >> 24) & 0xff) : g_vtotal_add)
			 + ((g_vtiming & 0xff0000) ?
			 ((g_vtiming >> 16) & 0xff) : g_vsize_add)));
		VSYNC_WR_DV_REG
			(DOLBY_CORE2A_SWAP_CTRL2,
			 (tmp_h << 16) | (tmp_v
			 + ((g_vtiming & 0xff0000) ?
			 ((g_vtiming >> 16) & 0xff) : g_vsize_add)));
	}
	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL3,
			(g_hwidth << 16) | ((g_vtiming & 0xff00) ?
			((g_vtiming >> 8) & 0xff) : g_vwidth));
	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL4,
			(g_hpotch << 16) | ((g_vtiming & 0xff) ?
			(g_vtiming & 0xff) : g_vpotch));
	if (is_meson_txlx_stbmode())
		VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL5, 0xf8000000);
	else if (is_meson_g12() || is_meson_tm2_stbmode() ||
		 is_meson_t7_stbmode() || is_meson_sc2() ||
		 is_meson_s4d())
		VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL5,  0xa8000000);
	else
		VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL5, 0x0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 2, 1);
	if (need_skip_cvm(1))
		bypass_flag |= 1 << 0;
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 2, 1);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 1,
			2 | bypass_flag);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 1,
			2 | bypass_flag);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_CTRL, 0);

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++) {
		if (reset ||
		    p_core2_dm_regs[i] !=
		    last_dm[i]) {
			VSYNC_WR_DV_REG
				(DOLBY_CORE2A_REG_START + 6 + i,
				 p_core2_dm_regs[i]);
			set_lut = true;
		}
	}

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2)
		set_lut = true;
	else if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
		set_lut = false;

	if (debug_dolby & 2)
		pr_dolby_dbg("core2 potch %x %x, reset %d, %d, flag %x, size %d %d\n",
			     g_hpotch, g_vpotch, reset, set_lut,
			     stb_core_setting_update_flag, hsize, vsize);

	/* core2 metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;
	if (count && (set_lut || reset || force_set_lut)) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS(DOLBY_CORE2A_CLKGATE_CTRL,
				2, 2, 2);
#endif
		VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_CTRL, 0x1401);

		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i + 3]);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i + 2]);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i + 1]);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i]);
			}
		} else {
			for (i = 0; i < count; i++) {
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i]);
			}
		}
		/* core2 lookup table program done */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm() &&
		(dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT))
			VSYNC_WR_DV_REG_BITS
				(DOLBY_CORE2A_CLKGATE_CTRL, 0, 2, 2);
#endif
	}
	force_set_lut = false;

	/* enable core2 */
	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL0, dolby_enable << 0);
	return 0;
}

bool is_core3_mute_reg(int index)
{
	return (index == 12) || /* ipt_scale for ipt*/
	(index >= 16 && index <= 17) || /* rgb2yuv scale for yuv */
	(index >= 5 && index <= 9); /* lms2rgb coeff for rgb */
}

#ifdef V2_4_3
static unsigned char hdmi_metadata[211 * 8];
static unsigned int hdmi_metadata_size;
static void convert_hdmi_metadata(u32 *md)
{
	int i = 0;
	u32 *p = md;
	int shift = 0;
	u32 size = md[0] >> 8;

	if (size > 211 * 8)
		size = 211 * 8;

	hdmi_metadata[0] = *p++ & 0xff;
	shift = 0;
	for (i = 1; i < size; i++) {
		hdmi_metadata[i] = (*p >> shift) & 0xff;
		shift += 8;
		if (shift == 32) {
			p++;
			shift = 0;
		}
	}
	hdmi_metadata_size = size;
}

bool need_send_emp_meta(const struct vinfo_s *vinfo)
{
	if (!vinfo || !vinfo->vout_device || !vinfo->vout_device->dv_info)
		return false;
	return is_meson_tm2_stbmode() &&
	(vinfo->vout_device->dv_info->dv_emp_cap || force_support_emp) &&
	((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) ||
	dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT) &&
	(dolby_vision_ll_policy == DOLBY_VISION_LL_DISABLE) &&
	(dolby_vision_std_policy == DOLBY_VISION_STD_YUV422);
}
#endif
static int dolby_core3_set
	(u32 dm_count,
	 u32 md_count,
	 u32 *p_core3_dm_regs,
	 u32 *p_core3_md_regs,
	 int hsize,
	 int vsize,
	 int dolby_enable,
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
	if (new_dovi_setting.diagnostic_enable ||
	    new_dovi_setting.dovi_ll_enable)
		diag_enable = 1;

	if (dolby_vision_on &&
	    (dolby_vision_flags &
	    FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on ||
	    (dolby_vision_flags & FLAG_CERTIFICAION))
		reset = true;

	if (force_update_reg & 4)
		reset = true;

	if (is_meson_gxm()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if ((first_reseted & 0x2) == 0) {
			first_reseted = (first_reseted | 0x2);
			reset = true;
		}
#endif
	} else {
		if (dolby_vision_on_count == 0)
			reset = true;
	}
	if ((cur_dv_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
	     cur_dv_mode == DOLBY_VISION_OUTPUT_MODE_IPT) && diag_enable) {
		cur_dv_mode = dv_ll_output_mode & 0xff;

		if (is_meson_g12() ||
		    is_meson_tm2_stbmode() ||
		    is_meson_t7_stbmode() ||
		    is_meson_sc2() ||
		    is_meson_s4d()) {
			if (dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422)
				diag_mode = 0x20;
			else
				diag_mode = 3;
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
	    new_dovi_setting.mode_changed ||
	    new_dovi_setting.vsvdb_changed ||
	    pps_state)) {
		last_dolby_vision_ll_policy =
			dolby_vision_ll_policy;
		new_dovi_setting.vsvdb_changed = 0;
		new_dovi_setting.mode_changed = 0;
		/* TODO: verify 962e case */
		if (is_meson_box() ||
		    is_meson_tm2_stbmode() ||
		    is_meson_t7_stbmode() ||
		    is_meson_sc2() ||
		    is_meson_s4d()) {
			if (new_dovi_setting.dovi_ll_enable &&
			    new_dovi_setting.diagnostic_enable == 0) {
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL,
					 3, 6, 2); /* post matrix */
				VSYNC_WR_DV_REG_BITS
					(VPP_MATRIX_CTRL,
					 1, 0, 1); /* post matrix */
			} else {
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL,
					 0, 6, 2); /* post matrix */
				VSYNC_WR_DV_REG_BITS
					(VPP_MATRIX_CTRL,
					 0, 0, 1); /* post matrix */
			}
		} else if (is_meson_txlx_stbmode()) {
			if (pps_state == 2) {
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL,
					 1, 0, 1); /* skip pps/dither/cm */
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA0, 0x08000800);
			} else if (pps_state == 1) {
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL,
					 0, 0, 1); /* enable pps/dither/cm */
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA0, 0x20002000);
			}
			if (new_dovi_setting.dovi_ll_enable &&
				new_dovi_setting.diagnostic_enable == 0) {
				/*bypass gainoff to vks */
				/*enable wn tp vks*/
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL, 0, 2, 1);
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL, 1, 1, 1);
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA1, 0x8000800);
				VSYNC_WR_DV_REG_BITS
					(VPP_MATRIX_CTRL,
					 1, 0, 1); /* post matrix */
			} else {
				/* bypass wm tp vks*/
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL, 1, 2, 1);
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL, 0, 1, 1);
				VSYNC_WR_DV_REG
					(VPP_DAT_CONV_PARA1, 0x20002000);
				if (is_meson_tvmode())
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
	if (new_dovi_setting.dovi_ll_enable &&
	    new_dovi_setting.diagnostic_enable == 0 &&
	    dolby_vision_on &&
	    (reset_post_table || reset ||
	    memcmp(&p_core3_dm_regs[18],
		   &last_dm[18], 32)))
		enable_rgb_to_yuv_matrix_for_dvll
			(1, &p_core3_dm_regs[18], 12);
	if (is_meson_tm2_stbmode() || is_meson_t7_stbmode() || is_meson_g12()) {
		if (get_dv_vpu_mem_power_status(VPU_DOLBY_CORE3) ==
			VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY_CORE3) ==
			VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY_CORE3);
	}

	VSYNC_WR_DV_REG(DOLBY_CORE3_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL1,
			((hsize + htotal_add) << 16)
			| (vsize + vtotal_add + vsize_add + vsize_hold * 2));
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL2,
			(hsize << 16) | (vsize + vsize_add));
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL3,
			(0x80 << 16) | vsize_hold);
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL4,
			(0x04 << 16) | vsize_hold);
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL5, 0x0000);
	if (cur_dv_mode !=
		DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
		VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL6, 0);
	else
		VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL6,
				0x10000000);  /* swap UV */
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 5, 7);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 4, 4);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 4, 2);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 2, 1);
	/* Control Register, address 0x04 2:0 RW */
	/* Output_operating mode*/
	/*   00- IPT 12 bit 444 bypass Dolby Vision output*/
	/*   01- IPT 12 bit tunnelled over RGB 8 bit 444, dolby vision output*/
	/*   02- HDR10 output, RGB 10 bit 444 PQ*/
	/*   03- Deep color SDR, RGB 10 bit 444 Gamma*/
	/*   04- SDR, RGB 8 bit 444 Gamma*/
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 1, cur_dv_mode);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 1, cur_dv_mode);
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
					(DOLBY_CORE3_REG_START + 0x6 + i,
					 0);
			else
				VSYNC_WR_DV_REG
					(DOLBY_CORE3_REG_START + 0x6 + i,
					 p_core3_dm_regs[i]);
		}
	}
	/* from addr 0x18 */

	if (scramble_en) {
		if (md_count > 204) {
			pr_dolby_error("core3 metadata size %d > 204 !\n",
				       md_count);
		} else {
			count = md_count;
			for (i = 0; i < count; i++) {
#ifdef FORCE_HDMI_META
				if (i == 20 &&
				    p_core3_md_regs[i] == 0x5140a3e)
					VSYNC_WR_DV_REG
						(DOLBY_CORE3_REG_START +
						 0x24 + i,
						 (p_core3_md_regs[i] &
						 0xffffff00) | 0x80);
				else
#endif
					VSYNC_WR_DV_REG
						(DOLBY_CORE3_REG_START +
						 0x24 + i, p_core3_md_regs[i]);
			}
			for (; i < (MAX_CORE3_MD_SIZE + 1); i++)
				VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START +
					0x24 + i, 0);
		}
	}

	/* from addr 0x90 */
	/* core3 metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 3, 1);

	VSYNC_WR_DV_REG(DOLBY_CORE3_DIAG_CTRL, diag_mode);

	if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
	    !(dolby_vision_flags & FLAG_DISABLE_CRC))
		VSYNC_WR_DV_REG(DOLBY_CORE3_CRC_CTRL, 1);
	/* enable core3 */
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL0, (dolby_enable << 0));
	return 0;
}

void update_graphic_width_height(unsigned int width,
				 unsigned int height)
{
	new_osd_graphic_width = width;
	new_osd_graphic_height = height;
}
EXPORT_SYMBOL(update_graphic_width_height);

void update_graphic_status(void)
{
	osd_update = true;
	pr_dolby_dbg("osd update, need toggle\n");
}

static int is_graphic_changed(void)
{
	int ret = 0;

	if (is_graphics_output_off()) {
		if (!is_osd_off) {
			pr_dolby_dbg("osd off\n");
			is_osd_off = true;
			ret |= 1;
		}
	} else if (is_osd_off) {
		/* force reset core2 when osd off->on */
		force_reset_core2 = true;
		pr_dolby_dbg("osd on\n");
		is_osd_off = false;
		ret |= 2;
	}

	if (osd_graphic_width != new_osd_graphic_width ||
	    osd_graphic_height != new_osd_graphic_height) {
		if (debug_dolby & 0x2)
			pr_dolby_dbg("osd changed %d %d-%d %d\n",
				     osd_graphic_width,
				     osd_graphic_height,
				     new_osd_graphic_width,
				     new_osd_graphic_height);

		/* TODO: g12/tm2/sc2/t7 osd pps is after dolby core2, but */
		/* sometimes osd do crop,should monitor osd size change*/
		if (!is_osd_off /*&& !is_meson_tm2() && !is_meson_sc2() && !is_meson_t7()*/) {
			osd_graphic_width = new_osd_graphic_width;
			osd_graphic_height = new_osd_graphic_height;
			ret |= 2;
		}
	}
	if (old_dolby_vision_graphic_max !=
	    dolby_vision_graphic_max) {
		if (debug_dolby & 0x2)
			pr_dolby_dbg("graphic max changed %d-%d\n",
				     old_dolby_vision_graphic_max,
				     dolby_vision_graphic_max);
		if (!is_osd_off) {
			old_dolby_vision_graphic_max =
				dolby_vision_graphic_max;
			ret |= 2;
			force_set_lut = true;
		}
	}
	return ret;
}

static bool is_hdr10_src_primary_changed(void)
{
	if (hdr10_param.r_x != last_hdr10_param.r_x ||
	    hdr10_param.r_y != last_hdr10_param.r_y ||
	    hdr10_param.g_x != last_hdr10_param.g_x ||
	    hdr10_param.g_y != last_hdr10_param.g_y ||
	    hdr10_param.b_x != last_hdr10_param.b_x ||
	    hdr10_param.b_y != last_hdr10_param.b_y) {
		last_hdr10_param.r_x = hdr10_param.r_x;
		last_hdr10_param.r_y = hdr10_param.r_y;
		last_hdr10_param.g_x = hdr10_param.g_x;
		last_hdr10_param.g_y = hdr10_param.g_y;
		last_hdr10_param.b_x = hdr10_param.b_x;
		last_hdr10_param.b_y = hdr10_param.b_y;
		return 1;
	}
	return 0;
}

static int cur_mute_type;
static char mute_type_str[4][4] = {
	"NON",
	"YUV",
	"RGB",
	"IPT"
};

int get_mute_type(void)
{
	if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444)
		return MUTE_TYPE_RGB;
	else if ((dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422) ||
		 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_SDR8) ||
		 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_SDR10) ||
		 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_HDR10))
		return MUTE_TYPE_YUV;
	else if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT) ||
		 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL))
		return MUTE_TYPE_IPT;
	else
		return MUTE_TYPE_NONE;
}

static void apply_stb_core_settings
	(int enable, unsigned int mask,
	 bool reset, u32 frame_size, u8 pps_state)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	u32 h_size = (frame_size >> 16) & 0xffff;
	u32 v_size = frame_size & 0xffff;
	u32 core1_dm_count = 27;
	u32 graphics_w = osd_graphic_width;
	u32 graphics_h = osd_graphic_height;
	u32 update_bk = stb_core_setting_update_flag;
	static u32 update_flag_more;
	int mute_type;

	if (h_size == 0xffff)
		h_size = 0;
	if (v_size == 0xffff)
		v_size = 0;

	if (stb_core_setting_update_flag != update_flag_more &&
	    (debug_dolby & 2))
		pr_dolby_dbg
			("apply_stb_core_settings update setting again %x->%x\n",
			 stb_core_setting_update_flag, update_flag_more);

	stb_core_setting_update_flag |= update_flag_more;

	if (is_dolby_vision_stb_mode() &&
		(dolby_vision_flags & FLAG_CERTIFICAION)) {
		graphics_w = dv_cert_graphic_width;
		graphics_h = dv_cert_graphic_height;
	}
	adjust_vpotch(graphics_w, graphics_h);
	if (mask & 1) {
		if (is_meson_txlx_stbmode()) {
			stb_dolby_core1_set
				(27, 173, 256 * 5,
				 (u32 *)&new_dovi_setting.dm_reg1,
				 (u32 *)&new_dovi_setting.comp_reg,
				 (u32 *)&new_dovi_setting.dm_lut1,
				 h_size,
				 v_size,
				 /* BL enable */
				 enable,
				 /* EL enable */
				 enable && new_dovi_setting.el_flag,
				 new_dovi_setting.el_halfsize_flag,
				 dolby_vision_mode ==
				 DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
				 new_dovi_setting.src_format == FORMAT_DOVI,
				 1,
				 reset);
		} else {
			dolby_core1_set
				(core1_dm_count, 173, 256 * 5,
				 (u32 *)&new_dovi_setting.dm_reg1,
				 (u32 *)&new_dovi_setting.comp_reg,
				 (u32 *)&new_dovi_setting.dm_lut1,
				 h_size,
				 v_size,
				 /* BL enable */
				 enable,
				 /* EL enable */
				 enable && new_dovi_setting.el_flag,
				 new_dovi_setting.el_halfsize_flag,
				 dolby_vision_mode ==
				 DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
				 new_dovi_setting.src_format == FORMAT_DOVI,
				 1,
				 reset);
		}
	}

	if (mask & 2) {
		if (stb_core_setting_update_flag != CP_FLAG_CHANGE_ALL) {
			/* when CP_FLAG_CONST_TC2 is set, */
			/* set the stb_core_setting_update_flag */
			/* until only meeting the CP_FLAG_CONST_TC2 */
			if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
				stb_core2_const_flag = true;
			else if (stb_core_setting_update_flag &
				CP_FLAG_CHANGE_TC2)
				stb_core2_const_flag = false;
		}
		/* revert the core2 lut as last corret one when const case */
		if (stb_core2_const_flag)
			memcpy(&new_dovi_setting.dm_lut2,
			       &dovi_setting.dm_lut2,
			       sizeof(struct dm_lut_ipcore));

		if (core2_sel & 1)
			dolby_core2a_set
				(24, 256 * 5,
				 (u32 *)&new_dovi_setting.dm_reg2,
				 (u32 *)&new_dovi_setting.dm_lut2,
				 graphics_w, graphics_h, 1, 1);

		if (core2_sel & 2)
			dolby_core2c_set
				(24, 256 * 5,
				 (u32 *)&new_dovi_setting.dm_reg2,
				 (u32 *)&new_dovi_setting.dm_lut2,
				 graphics_w, graphics_h, 1, 1);
	}

	if (mask & 4) {
		v_size = vinfo->height;
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
			v_size = v_size / 2;
		mute_type = get_mute_type();
		if ((get_video_mute() == VIDEO_MUTE_ON_DV) &&
		    (!(dolby_vision_flags & FLAG_MUTE) ||
		    cur_mute_type != mute_type)) {
			pr_dolby_dbg("mute %s\n", mute_type_str[mute_type]);
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
			pr_dolby_dbg("unmute %s\n",
				     mute_type_str[cur_mute_type]);
			cur_mute_type = MUTE_TYPE_NONE;
			dolby_vision_flags &= ~FLAG_MUTE;
		}
		dolby_core3_set
			(26, new_dovi_setting.md_reg3.size,
			 (u32 *)&new_dovi_setting.dm_reg3,
			 new_dovi_setting.md_reg3.raw_metadata,
			 vinfo->width, v_size,
			 1,
			 dolby_vision_mode ==
			 DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
			 pps_state);
#ifdef V2_4_3
		if (need_send_emp_meta(vinfo)) {
			convert_hdmi_metadata
				(new_dovi_setting.md_reg3.raw_metadata);
			send_emp(EOTF_T_DOLBYVISION,
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL
				? RGB_8BIT : YUV422_BIT12,
				NULL,
				hdmi_metadata,
				hdmi_metadata_size,
				false);
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
/*static u32 vpp_vadj2_backup;*/
static u32 vpp_gainoff_backup;
static u32 vpp_ve_enable_ctrl_backup;
static u32 xvycc_vd1_rgb_ctrst_backup;
#endif
static bool is_video_effect_bypass;

static void video_effect_bypass(int bypass)
{
#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	return;
#endif

	if (is_meson_tvmode()) {
		/*only bypass vpp pq for IDK cert or debug mode*/
		if (!debug_bypass_vpp_pq &&
		    !(dolby_vision_flags & FLAG_CERTIFICAION))
			return;
	}
	if (debug_bypass_vpp_pq == 1) {
		if ((dolby_vision_flags & FLAG_CERTIFICAION) ||
		    bypass_all_vpp_pq || is_meson_tvmode())
			dv_pq_ctl(DV_PQ_CERT);
		else
			dv_pq_ctl(DV_PQ_BYPASS);
		return;
	} else if (debug_bypass_vpp_pq == 2) {
		dv_pq_ctl(DV_PQ_REC);
		return;
	} else if (debug_bypass_vpp_pq == 3) {
		return;
	}
	if (bypass) {
		if (!is_video_effect_bypass) {
			if (is_meson_txlx()) {
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
			is_video_effect_bypass = true;
		}
		if (is_meson_txlx()) {
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
			    bypass_all_vpp_pq || is_meson_tvmode())
				dv_pq_ctl(DV_PQ_CERT);
			else
				dv_pq_ctl(DV_PQ_BYPASS);
		}
	} else if (is_video_effect_bypass) {
		if (is_meson_txlx()) {
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

/*flag bit0: bypass from preblend to VADJ1, skip sr/pps/cm2*/
/*flag bit1: skip OE/EO*/
/*flag bit2: bypass from dolby3 to vkeystone, skip vajd2/post/mtx/gainoff*/
static void bypass_pps_sr_gamma_gainoff(int flag)
{
	pr_dolby_dbg("%s: %d\n", __func__, flag);

	if (flag & 1) {
		if (is_meson_t3() || is_meson_t5w()) {
			/*from t3, 1d93 bit0 change to 1d26 bit8*/
			VSYNC_WR_DV_REG_BITS(VPP_MISC, 1, 8, 1);
			force_bypass_from_prebld_to_vadj1 = true;
		} else {
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 1, 0, 1);
		}
	}
	if (flag & 2)
		VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 1, 1, 1);
	if (flag & 4)
		VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 1, 2, 1);
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

static u32 dolby_ctrl_backup = 0x22000;
static u32 viu_misc_ctrl_backup;
static u32 vpp_matrix_backup;
static u32 vpp_dummy1_backup;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static u32 vpp_data_conv_para0_backup;
static u32 vpp_data_conv_para1_backup;
#endif
void enable_dolby_vision(int enable)
{
	u32 size = 0;
	u64 *dma_data = tv_dovi_setting->core1_reg_lut;
	u32 core_flag = 0;
	int gd_en = 0;
	if (enable) {
		if (!dolby_vision_on) {
			dolby_vision_wait_on = true;
			dolby_ctrl_backup =
				VSYNC_RD_DV_REG(VPP_DOLBY_CTRL);
			viu_misc_ctrl_backup =
				VSYNC_RD_DV_REG(VIU_MISC_CTRL1);
			vpp_matrix_backup =
				VSYNC_RD_DV_REG(VPP_MATRIX_CTRL);
			vpp_dummy1_backup =
				VSYNC_RD_DV_REG(VPP_DUMMY_DATA1);
			if (is_meson_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				vpp_data_conv_para0_backup =
					VSYNC_RD_DV_REG(VPP_DAT_CONV_PARA0);
				vpp_data_conv_para1_backup =
					VSYNC_RD_DV_REG(VPP_DAT_CONV_PARA1);
				setting_update_count = 0;
#endif
			}
			if (is_meson_tvmode()) {
				if (efuse_mode == 1) {
					size = 8 * TV_DMA_TBL_SIZE;
					if (is_meson_t7())
						size = 8 * TV_DMA_TBL_SIZE * 16;
					memset(dma_vaddr, 0x0, size);
					memcpy((u64 *)dma_vaddr + 1, dma_data + 1, 8);
				}
				if (!dolby_vision_core1_on)
					frame_count = 0;
				if (is_meson_txlx_tvmode()) {
					if ((dolby_vision_mask & 1) &&
					    dovi_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(VIU_MISC_CTRL1,
							 0,
							 16, 1); /* core1 */
						dolby_vision_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(VIU_MISC_CTRL1,
							 1,
							 16, 1); /* core1 */
						dolby_vision_core1_on = false;
					}
				} else if (is_meson_tm2_tvmode()) {
					/* common flow should */
					/* stop hdr core before */
					/* start dv core */
					if (dolby_vision_flags &
					FLAG_CERTIFICAION)
						hdr_vd1_off(VPP_TOP0);
					if ((dolby_vision_mask & 1) &&
					    dovi_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 1, 8, 2);
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 1, 10, 2);
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 1, 24, 2);
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 0, 16, 1);
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 0, 20, 1);
						VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 tv_dovi_setting->el_flag ?
						 0 : 2, 0, 2);
						dolby_vision_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 3, 0, 2);
						VSYNC_WR_DV_REG
							(DOLBY_TV_CLKGATE_CTRL,
							0x55555555);
						dv_mem_power_off(VPU_DOLBY0);
						dolby_vision_core1_on = false;
					}
				} else if (is_meson_t7_tvmode()) {
					/* common flow should */
					/* stop hdr core before */
					/* start dv core */
					if (dolby_vision_flags &
						FLAG_CERTIFICAION)
						hdr_vd1_off(VPP_TOP0);
					if ((dolby_vision_mask & 1) &&
						dovi_setting_video_flag) {
						/*enable tv core*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							0, 4, 1);
						/*vd1 to tvcore*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL1,
							 1, 0, 3);
						/*tvcore bl in sel vd1*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL2,
							 0, 6, 2);
						/*tvcore el in sel null*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL2,
							 3, 8, 2);
						/*tvcore bl to vd1*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL2,
							 0, 20, 2);
						/* vd1 from tvcore*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL1,
							 1, 12, 3);
						dolby_vision_core1_on = true;
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
							(DOLBY_TV_CLKGATE_CTRL,
							0x55555555);
						dv_mem_power_off(VPU_DOLBY0);
						dolby_vision_core1_on = false;
					}
				} else  if (is_meson_t3_tvmode() ||
					    is_meson_t5w()) {
					/* common flow should */
					/* stop hdr core before */
					/* start dv core */
					if (dolby_vision_flags &
						FLAG_CERTIFICAION)
						hdr_vd1_off(VPP_TOP0);
					if ((dolby_vision_mask & 1) &&
						dovi_setting_video_flag) {
						/*enable tv core*/
						/* T3 is not the same as T7 */
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							0, 16, 1);
						/*vd1 to tvcore*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL1,
							 1, 0, 3);
						/*tvcore bl in sel vd1*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL2,
							 0, 6, 2);
						/*tvcore el in sel null*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL2,
							 3, 8, 2);
						/*tvcore bl to vd1*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL2,
							 0, 20, 2);
						/* vd1 from tvcore*/
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_SWAP_CTRL1,
							 1, 12, 3);
						dolby_vision_core1_on = true;
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
							(DOLBY_TV_CLKGATE_CTRL,
							0x55555555);
						dv_mem_power_off(VPU_DOLBY0);
						dolby_vision_core1_on = false;
					}
				}
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM, EO/OE */
					bypass_pps_sr_gamma_gainoff(3);
					/* bypass all video effect */
					video_effect_bypass(1);
					if (is_meson_txlx_tvmode()) {
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
					WRITE_VPP_DV_REG(DOLBY_TV_DIAG_CTRL,
							 0xb);
				} else {
					/* bypass all video effect */
					if (dolby_vision_flags &
					    FLAG_BYPASS_VPP)
						video_effect_bypass(1);
					if (is_meson_txlx_tvmode()) {
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
				pr_info("Dolby Vision TV core turn on\n");
			} else if (is_meson_txlx_stbmode()) {
				size = 8 * STB_DMA_TBL_SIZE;
				if (efuse_mode == 1)
					memset(dma_vaddr, 0x0, size);
				osd_bypass(1);
				if (dolby_vision_mask & 4)
					VSYNC_WR_DV_REG_BITS
						(VPP_DOLBY_CTRL,
						 1, 3, 1);   /* core3 enable */
				if ((dolby_vision_mask & 1) &&
				    dovi_setting_video_flag) {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 0,
						 16, 1); /* core1 */
					dolby_vision_core1_on = true;
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 1,
						 16, 1); /* core1 */
					dolby_vision_core1_on = false;
				}
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 (((dolby_vision_mask & 1) &&
					 dovi_setting_video_flag)
					 ? 0 : 1),
					 16, 1); /* core1 */
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 ((dolby_vision_mask & 2) ? 0 : 1),
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
				if (is_meson_tvmode())
					VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 1);
				else
					VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);

				if ((dolby_vision_mode ==
				     DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
				     dolby_vision_mode ==
				     DOLBY_VISION_OUTPUT_MODE_IPT) &&
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
						(VPP_DOLBY_CTRL, 1, 1, 2);
					enable_rgb_to_yuv_matrix_for_dvll
						(1, &reg[18], 12);
				} else {
					enable_rgb_to_yuv_matrix_for_dvll
						(0, NULL, 12);
				}
				last_dolby_vision_ll_policy =
					dolby_vision_ll_policy;
				pr_info("Dolby Vision STB cores turn on\n");
			} else if (is_meson_g12() ||
				   is_meson_tm2_stbmode() ||
				   is_meson_t7_stbmode() ||
				   is_meson_sc2() ||
				   is_meson_s4d()) {
				hdr_osd_off(VPP_TOP0);
				hdr_vd1_off(VPP_TOP0);
				set_hdr_module_status(VD1_PATH,
					HDR_MODULE_BYPASS);
				if (!dolby_vision_core1_on)
					frame_count = 0;
				if (is_meson_t7_stbmode()) {
					if (dolby_vision_mask & 4)
						VSYNC_WR_DV_REG_BITS
						(VPP_DOLBY_CTRL,
						 1, 3, 1);   /* core3 enable */
					else
						VSYNC_WR_DV_REG_BITS
						(VPP_DOLBY_CTRL,
						 0, 3, 1);   /* bypass core3  */

					if ((dolby_vision_mask & 2) && (core2_sel & 1)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD_TOP_CTRL,
							 0,
							 14, 1);/*core2a enable*/
					}
					if ((dolby_vision_mask & 2) && (core2_sel & 2)) {
						VSYNC_WR_DV_REG_BITS
							(MALI_AFBCD1_TOP_CTRL,
							 0,
							 19, 1);/*core2c enable*/
					}
					if (!(dolby_vision_mask & 2)) {
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
					if (dolby_vision_mask & 4)
						VSYNC_WR_DV_REG_BITS
						(VPP_DOLBY_CTRL,
						 1, 3, 1);   /* core3 enable */
					else
						VSYNC_WR_DV_REG_BITS
						(VPP_DOLBY_CTRL,
						 0, 3, 1);   /* bypass core3  */

					if (dolby_vision_mask & 2)
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 0,
							 2, 1);/*core2 enable*/
					else
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 1,
							 2, 1);/*core2 bypass*/
				}
				if (is_meson_g12() || is_meson_sc2() || is_meson_s4d()) {
					if ((dolby_vision_mask & 1) &&
					    dovi_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 0,
							 0, 1); /* core1 on*/
						dolby_vision_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 1,
							 0, 1); /* core1 off*/
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(DOLBY_CORE1A_CLKGATE_CTRL,
							0x55555455);
						dolby_vision_core1_on = false;
					}
				} else if (is_meson_tm2_stbmode()) {
					if (is_meson_stb_hdmimode())
						core_flag = 1;
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						core_flag, 8, 2);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						core_flag, 10, 2);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						core_flag, 24, 2);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						0, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						0, 17, 1);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						0, 20, 1);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						0, 21, 1);
					if ((dolby_vision_mask & 1) &&
						dovi_setting_video_flag) {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							0,
							0, 2); /* core1 on */
						dolby_vision_core1_on = true;
					} else {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							 3,
							 0, 2); /* core1 off*/
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(DOLBY_CORE1A_CLKGATE_CTRL,
							0x55555455);
						dolby_vision_core1_on = false;
					}
					if (core_flag &&
					dolby_vision_core1_on) {
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							3,
							0, 2); /* core1 off */
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(DOLBY_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					}
				} else if (is_meson_t7_stbmode()) {
					if (is_meson_stb_hdmimode())
						core_flag = 1;
					/* DOLBY_PATH_SWAP_CTRL1 todo*/
					if ((dolby_vision_mask & 1) &&
					    dovi_setting_video_flag) {
						/*vd1 core1 on*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 0, 4, 1);
						dolby_vision_core1_on = true;
					} else {
						/*vd1 core1 off*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG(DOLBY_CORE1A_CLKGATE_CTRL,
								0x55555455);
						dolby_vision_core1_on = false;
					}
					if (core_flag && dolby_vision_core1_on) {
						/*vd1 core1 off*/
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG(DOLBY_CORE1A_CLKGATE_CTRL,
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
				    DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
				    dolby_vision_mode ==
				    DOLBY_VISION_OUTPUT_MODE_IPT) &&
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
						(VPP_DOLBY_CTRL, 1, 1, 2);
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
				pr_dolby_dbg
					("Dolby Vision G12a turn on%s\n",
					dolby_vision_core1_on ?
					", core1 on" : "");
				if (!dolby_vision_core1_on)
					frame_count = 0;
			} else {
				VSYNC_WR_DV_REG
					(VPP_DOLBY_CTRL,
					 /* cm_datx4_mode */
					 (0x0 << 21) |
					 /* reg_front_cti_bit_mode */
					 (0x0 << 20) |
					 /* vpp_clip_ext_mode 19:17 */
					 (0x0 << 17) |
					 /* vpp_dolby2_en core3 */
					 (((dolby_vision_mask & 4) ?
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
					 ((dolby_vision_mask & 2) ?
					 0 : 1),
					 18, 6);
				if ((dolby_vision_mask & 1) &&
				    dovi_setting_video_flag) {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 0,
						 16, 1); /* core1 */
					dolby_vision_core1_on = true;
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 1,
						 16, 1); /* core1 */
					dolby_vision_core1_on = false;
				}
				/* bypass all video effect */
				if ((dolby_vision_flags & FLAG_BYPASS_VPP) ||
				    (dolby_vision_flags & FLAG_CERTIFICAION))
					video_effect_bypass(1);
				VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_DV_REG(VPP_DUMMY_DATA1, 0x20000000);
				if ((dolby_vision_mode ==
				    DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
				    dolby_vision_mode ==
				    DOLBY_VISION_OUTPUT_MODE_IPT) &&
				    dovi_setting.diagnostic_enable == 0 &&
				    dovi_setting.dovi_ll_enable) {
					u32 *reg =
						(u32 *)&dovi_setting.dm_reg3;
					VSYNC_WR_DV_REG_BITS
						(VPP_DOLBY_CTRL,
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
				pr_dolby_dbg("Dolby Vision turn on%s\n",
					     dolby_vision_core1_on ?
					     ", core1 on" : "");
			}
			dolby_vision_core1_on_cnt = 0;
		} else {
			if (!dolby_vision_core1_on &&
				(dolby_vision_mask & 1) &&
				dovi_setting_video_flag) {
				if (is_meson_g12() ||
				    is_meson_tm2_stbmode() ||
				    is_meson_sc2() ||
				    is_meson_s4d()) {
					/* enable core1 with el */
					if (dovi_setting.el_flag)
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							0, 0, 2);
					else /* enable core1 without el */
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							0, 0, 1);
					if (is_meson_stb_hdmimode()) {
						/* core1 off */
						VSYNC_WR_DV_REG_BITS
							(DOLBY_PATH_CTRL,
							3, 0, 2);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(DOLBY_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					} else {
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_meson_tm2_tvmode()) {
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 1, 8, 2);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 1, 10, 2);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 1, 24, 2);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 0, 16, 1);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 0, 20, 1);
					/* enable core1 */
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_CTRL,
						 /* enable core1 */
						 tv_dovi_setting->el_flag ?
						 0 : 2, 0, 2);
				} else if (is_meson_t7_stbmode()) {
					/* vd1 to core1a*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 0, 0, 3);
					/*core1a bl in sel vd1*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 0, 2, 2);
					/*core1a el in sel null*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 3, 4, 2);
					/*core1a out to vd1*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 0, 18, 2);
					/* vd1 from core1a*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 0, 12, 3);
					/*enable core1a*/
					VSYNC_WR_DV_REG_BITS
						(VPP_VD1_DSC_CTRL,
						 0, 4, 1);
					if (is_meson_stb_hdmimode()) {
						/* core1 off */
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 1, 4, 1);
						dv_mem_power_off(VPU_DOLBY1A);
						dv_mem_power_off
							(VPU_PRIME_DOLBY_RAM);
						VSYNC_WR_DV_REG
							(DOLBY_CORE1A_CLKGATE_CTRL,
							0x55555455);
						/* hdr core on */
						hdr_vd1_iptmap(VPP_TOP0);
					} else {
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_meson_t7_tvmode() ||
				is_meson_t3_tvmode() || is_meson_t5w()) {
					/* enable core1 */
					if (is_meson_t7_tvmode())
						VSYNC_WR_DV_REG_BITS
							(VPP_VD1_DSC_CTRL,
							 0, 4, 1);
					else
						VSYNC_WR_DV_REG_BITS
							(VIU_VD1_PATH_CTRL,
							 0, 16, 1);
					/*vd1 to tvcore*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 1, 0, 3);
					/*tvcore bl in sel vd1*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 0, 6, 2);
					/*tvcore el in sel null*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 3, 8, 2);
					/*tvcore bl to vd1*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL2,
						 0, 20, 2);
					/* vd1 from tvcore*/
					VSYNC_WR_DV_REG_BITS
						(DOLBY_PATH_SWAP_CTRL1,
						 1, 12, 3);
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 0,
						 16, 1); /* core1 */
				}
				dolby_vision_core1_on = true;
				dolby_vision_core1_on_cnt = 0;
				pr_dolby_dbg("Dolby Vision core1 turn on\n");
			} else if (dolby_vision_core1_on &&
					   (!(dolby_vision_mask & 1) ||
					    !dovi_setting_video_flag)) {
				if (is_meson_g12() ||
				    is_meson_tm2_stbmode() ||
				    is_meson_t7_stbmode() ||
				    is_meson_sc2() ||
				    is_meson_s4d()) {
					if (is_meson_t7_stbmode()) {
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
							(DOLBY_PATH_CTRL,
							3, 0, 2);
					}
					dv_mem_power_off(VPU_DOLBY1A);
					dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
					VSYNC_WR_DV_REG
						(DOLBY_CORE1A_CLKGATE_CTRL,
						0x55555455);
					if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
						/* core1b */
						dv_mem_power_off(VPU_DOLBY1B);
						VSYNC_WR_DV_REG
							(DOLBY_CORE1B_CLKGATE_CTRL,
							0x55555455);
						/* coretv */
						VSYNC_WR_DV_REG_BITS
							(DOLBY_TV_SWAP_CTRL7,
							0xf, 4, 4);
						VSYNC_WR_DV_REG
							(DOLBY_TV_CLKGATE_CTRL,
							0x55555455);
						dv_mem_power_off(VPU_DOLBY0);
						/* hdr core */
						hdr_vd1_off(VPP_TOP0);
					}
				} else if (is_meson_tm2_tvmode()) {
					/* disable coretv */
					VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL,
							     3, 0, 2);
					VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
							0x01000042);
					VSYNC_WR_DV_REG_BITS(DOLBY_TV_SWAP_CTRL7,
							     0xf, 4, 4);
					VSYNC_WR_DV_REG(DOLBY_TV_CLKGATE_CTRL,
							0x55555455);
					dv_mem_power_off(VPU_DOLBY0);
				} else if (is_meson_t7_tvmode() ||
					is_meson_t3_tvmode() ||
					is_meson_t5w()) {
					/* disable coretv */
					if (is_meson_t7_tvmode())
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
					VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
						0x01000042);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(DOLBY_TV_CLKGATE_CTRL,
						0x55555455);
					dv_mem_power_off(VPU_DOLBY0);
					if (is_meson_t3() || is_meson_t5w())
						vpu_module_clk_disable
							(0, DV_TVCORE, 0);
				} else {
					VSYNC_WR_DV_REG_BITS
						(VIU_MISC_CTRL1,
						 1,
						 16, 1); /* core1 */
				}
				dolby_vision_core1_on = false;
				dolby_vision_core1_on_cnt = 0;
				frame_count = 0;
				last_vf_valid_crc = 0;
				pr_dolby_dbg("Dolby Vision core1 turn off\n");
			}
		}
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_wait_count = 0;
		vsync_count = 0;
	} else {
		if (dolby_vision_on) {
			if (is_meson_tvmode()) {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					 /* vd2 connect to vpp */
					 (1 << 1) |
					 /* 16 core1 bl bypass */
					 (1 << 0),
					 16, 2);
				if (is_meson_tm2_tvmode()) {
					VSYNC_WR_DV_REG_BITS
					(DOLBY_PATH_CTRL, 3, 0, 2);
				} else if (is_meson_t7_tvmode() ||
					   is_meson_t3_tvmode() ||
					   is_meson_t5w()) {
					if (is_meson_t7_tvmode())
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
				if (is_meson_tm2_tvmode() || is_meson_t7_tvmode() ||
				    is_meson_t3_tvmode() ||
				    is_meson_t5w()) {
					VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
						0x01000042);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(DOLBY_TV_CLKGATE_CTRL,
						0x55555555);
					dv_mem_power_off(VPU_DOLBY0);
					if (is_meson_t3() || is_meson_t5w())
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
				pr_dolby_dbg("Dolby Vision TV core turn off\n");
				if (tv_dovi_setting)
					tv_dovi_setting->src_format =
					FORMAT_SDR;
			} else if (is_meson_txlx_stbmode()) {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_DV_REG_BITS
					(VPP_DOLBY_CTRL,
					 0, 3, 1);/* core3 disable */
				osd_bypass(0);
				if (p_funcs_stb) /* destroy ctx */
					p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 comp_buf[current_id], 0,
						 md_buf[current_id], 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 &hdr10_param,
						 &new_dovi_setting);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
				stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
				stb_core2_const_flag = false;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				dovi_setting.src_format = FORMAT_SDR;
				pr_dolby_dbg("Dolby Vision STB cores turn off\n");
			} else if (is_meson_g12() ||
				   is_meson_tm2_stbmode() ||
				   is_meson_t7_stbmode() ||
				   is_meson_sc2() ||
				   is_meson_s4d()) {
				if (is_meson_t7_stbmode()) {
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
						(DOLBY_PATH_CTRL,
						(1 << 2) |	/* core2 bypass */
						(1 << 1) |	/* vd2 connect to vpp */
						(1 << 0),	/* core1 bl bypass */
						0, 3);
				}
				VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL,
						     0, 3, 1);   /* core3 disable */

				/* core1a */
				VSYNC_WR_DV_REG
					(DOLBY_CORE1A_CLKGATE_CTRL,
					0x55555455);
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				/* core2 */
				VSYNC_WR_DV_REG
					(DOLBY_CORE2A_CLKGATE_CTRL,
					0x55555555);
				dv_mem_power_off(VPU_DOLBY2);
				/* core3 */
				VSYNC_WR_DV_REG
					(DOLBY_CORE3_CLKGATE_CTRL,
					0x55555555);
				dv_mem_power_off(VPU_DOLBY_CORE3);
				if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
					/* core1b */
					VSYNC_WR_DV_REG
						(DOLBY_CORE1B_CLKGATE_CTRL,
						0x55555555);
					dv_mem_power_off(VPU_DOLBY1B);
					/* tv core */
					VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
						0x01000042);
					VSYNC_WR_DV_REG_BITS
						(DOLBY_TV_SWAP_CTRL7,
						0xf, 4, 4);
					VSYNC_WR_DV_REG
						(DOLBY_TV_CLKGATE_CTRL,
						0x55555555);
					/* hdr core */
					hdr_vd1_off(VPP_TOP0);
					dv_mem_power_off(VPU_DOLBY0);
				}
				if (p_funcs_stb) /* destroy ctx */
					p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 comp_buf[current_id], 0,
						 md_buf[current_id], 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 &hdr10_param,
						 &new_dovi_setting);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
				stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
				stb_core2_const_flag = false;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				dovi_setting.src_format = FORMAT_SDR;
				pr_dolby_dbg("Dolby Vision G12a turn off\n");
			} else {
				VSYNC_WR_DV_REG_BITS
					(VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL,
						     0, 16, 1);/*core3 disable*/
				/* enable osd effect and*/
				/*	use default shadow mode */
				osd_path_enable(1);
				if (p_funcs_stb) /* destroy ctx */
					p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 comp_buf[current_id], 0,
						 md_buf[current_id], 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 &hdr10_param,
						 &new_dovi_setting);
				last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
				stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
				stb_core2_const_flag = false;
				memset(&dovi_setting, 0, sizeof(dovi_setting));
				dovi_setting.src_format = FORMAT_SDR;
				pr_dolby_dbg("Dolby Vision turn off\n");
			}
			VSYNC_WR_DV_REG(VIU_SW_RESET, 3 << 9);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
			if (is_meson_txlx()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0,
						vpp_data_conv_para0_backup);
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA1,
						vpp_data_conv_para1_backup);
				VSYNC_WR_DV_REG(DOLBY_TV_CLKGATE_CTRL,
						0x2414);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_CLKGATE_CTRL,
						0x4);
				VSYNC_WR_DV_REG(DOLBY_CORE3_CLKGATE_CTRL,
						0x414);
				VSYNC_WR_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
						0x01000042);
#endif
			}
			if (is_meson_box() || is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
				VSYNC_WR_DV_REG(DOLBY_CORE1A_CLKGATE_CTRL,
						0x55555555);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_CLKGATE_CTRL,
						0x55555555);
				VSYNC_WR_DV_REG(DOLBY_CORE3_CLKGATE_CTRL,
						0x55555555);
			}
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0,
					(0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1, 0);
			video_effect_bypass(0);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_meson_gxm())
				VSYNC_WR_DV_REG(VPP_DOLBY_CTRL,
						dolby_ctrl_backup);
#endif
			/* always vd2 to vpp and bypass core 1 */
			viu_misc_ctrl_backup |=
				(VSYNC_RD_DV_REG(VIU_MISC_CTRL1) & 2);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_meson_gxm()) {
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
		frame_count = 0;
		last_vf_valid_crc = 0;
		core1_disp_hsize = 0;
		core1_disp_vsize = 0;
		dolby_vision_on = false;
		force_reset_core2 = true;
		dolby_vision_core2_on_cnt = 0;
		dolby_vision_on_in_uboot = false;
		dolby_vision_core1_on = false;
		dolby_vision_core1_on_cnt = 0;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_wait_count = 0;
		dolby_vision_status = BYPASS_PROCESS;
		dolby_vision_target_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
		dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
		dolby_vision_src_format = 0;
		dolby_vision_on_count = 0;
		cur_csc_type[VD1_PATH] = VPP_MATRIX_NULL;
		/* clean mute flag for next time dv on */
		dolby_vision_flags &= ~FLAG_MUTE;
		force_bypass_from_prebld_to_vadj1 = false;
		hdmi_frame_count = 0;
		if (!is_meson_gxm() && !is_meson_txlx()) {
			hdr_osd_off(VPP_TOP0);
			hdr_vd1_off(VPP_TOP0);
		}
		/*dv release control of pwm*/
		if (is_meson_tvmode()) {
			gd_en = 0;
#ifdef CONFIG_AMLOGIC_LCD
			aml_lcd_atomic_notifier_call_chain
			(LCD_EVENT_BACKLIGHT_GD_SEL, &gd_en);
			dv_control_backlight_status = false;
#endif
		}
	}
}
EXPORT_SYMBOL(enable_dolby_vision);

/*  dolby vision enhanced layer receiver*/

#define DVEL_RECV_NAME "dvel"

static inline void dvel_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(DVEL_RECV_NAME);
	int event = 0;

	if (vfp) {
		vf_put(vf, DVEL_RECV_NAME);
		event |= VFRAME_EVENT_RECEIVER_PUT;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
}

static inline struct vframe_s *dvel_vf_peek(void)
{
	return vf_peek(DVEL_RECV_NAME);
}

static inline struct vframe_s *dvel_vf_get(void)
{
	int event = 0;
	struct vframe_s *vf = vf_get(DVEL_RECV_NAME);

	if (vf) {
		event |= VFRAME_EVENT_RECEIVER_GET;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
	return vf;
}

static struct vframe_s *dv_vf[16][2];
static void *metadata_parser;
static bool metadata_parser_reset_flag;
static char meta_buf[1024];

void dv_vf_light_unreg_provider(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dovi_lock, flags);
	if (vfm_path_on) {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0]) {
				if (dv_vf[i][1])
					dvel_vf_put(dv_vf[i][1]);
				dv_vf[i][1] = NULL;
			}
			dv_vf[i][0] = NULL;
		}
		/* if (metadata_parser && p_funcs) {*/
		/*	p_funcs->metadata_parser_release();*/
		/*	metadata_parser = NULL;*/
		/*} */

		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		memset(&last_hdr10_param, 0, sizeof(last_hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		dolby_vision_el_disable = 0;
	}
	vfm_path_on = false;
	spin_unlock_irqrestore(&dovi_lock, flags);
}
EXPORT_SYMBOL(dv_vf_light_unreg_provider);

void dv_vf_light_reg_provider(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dovi_lock, flags);
	if (!vfm_path_on) {
		for (i = 0; i < 16; i++) {
			dv_vf[i][0] = NULL;
			dv_vf[i][1] = NULL;
		}
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		memset(&last_hdr10_param, 0, sizeof(last_hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		dolby_vision_el_disable = 0;
	}
	vfm_path_on = true;
	spin_unlock_irqrestore(&dovi_lock, flags);
}
EXPORT_SYMBOL(dv_vf_light_reg_provider);

static int dvel_receiver_event_fun(int type, void *data, void *arg)
{
	char *provider_name = (char *)data;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("%s, provider %s unregistered\n",
			__func__, provider_name);
		dv_vf_light_unreg_provider();
		return -1;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		pr_info("%s, provider %s registered\n",
			__func__, provider_name);
		dv_vf_light_reg_provider();
	}
	return 0;
}

static const struct vframe_receiver_op_s dvel_vf_receiver = {
	.event_cb = dvel_receiver_event_fun
};

static struct vframe_receiver_s dvel_vf_recv;

void dolby_vision_init_receiver(void *pdev)
{
	ulong alloc_size;
	int i;

	pr_info("%s(%s)\n", __func__, DVEL_RECV_NAME);
	vf_receiver_init(&dvel_vf_recv, DVEL_RECV_NAME,
			 &dvel_vf_receiver, &dvel_vf_recv);
	vf_reg_receiver(&dvel_vf_recv);
	pr_info("%s: %s\n", __func__, dvel_vf_recv.name);
	dovi_pdev = (struct platform_device *)pdev;

	alloc_size = dma_size;
	if (is_meson_t7())
		alloc_size = dma_size * 16; /*t7 need dma addr align to 128bit*/

	alloc_size = (alloc_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dma_vaddr = dma_alloc_coherent(&dovi_pdev->dev,
				       alloc_size, &dma_paddr, GFP_KERNEL);
	pr_info("get dma_vaddr %p\n", dma_vaddr);
	for (i = 0; i < 2; i++) {
		md_buf[i] = vmalloc(MD_BUF_SIZE);
		if (md_buf[i])
			memset(md_buf[i], 0, MD_BUF_SIZE);
		comp_buf[i] = vmalloc(COMP_BUF_SIZE);
		if (comp_buf[i])
			memset(comp_buf[i], 0, COMP_BUF_SIZE);
		drop_md_buf[i] = vmalloc(MD_BUF_SIZE);
		if (drop_md_buf[i])
			memset(drop_md_buf[i], 0, MD_BUF_SIZE);
		drop_comp_buf[i] = vmalloc(COMP_BUF_SIZE);
		if (drop_comp_buf[i])
			memset(drop_comp_buf[i], 0, COMP_BUF_SIZE);
	}
	vsem_if_buf = vmalloc(VSEM_IF_BUF_SIZE);
	if (vsem_if_buf)
		memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
	vsem_md_buf = vmalloc(VSEM_IF_BUF_SIZE);
	if (vsem_md_buf)
		memset(vsem_md_buf, 0, VSEM_IF_BUF_SIZE);
}

void dolby_vision_clear_buf(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dovi_lock, flags);
	for (i = 0; i < 16; i++) {
		dv_vf[i][0] = NULL;
		dv_vf[i][1] = NULL;
	}
	spin_unlock_irqrestore(&dovi_lock, flags);
}
EXPORT_SYMBOL(dolby_vision_clear_buf);

#define MAX_FILENAME_LENGTH 64
static const char comp_file[] = "%s_comp.%04d.reg";
static const char dm_reg_core1_file[] = "%s_dm_core1.%04d.reg";
static const char dm_reg_core2_file[] = "%s_dm_core2.%04d.reg";
static const char dm_reg_core3_file[] = "%s_dm_core3.%04d.reg";
static const char dm_lut_core1_file[] = "%s_dm_core1.%04d.lut";
static const char dm_lut_core2_file[] = "%s_dm_core2.%04d.lut";

static void dump_struct(void *structure,
			int struct_length,
			const char file_string[],
			int frame_nr)
{
	char fn[MAX_FILENAME_LENGTH];
	struct file *fp;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	if (frame_nr == 0)
		return;
	set_fs(KERNEL_DS);
	snprintf(fn, MAX_FILENAME_LENGTH, file_string,
		 "/data/tmp/tmp", frame_nr - 1);
	fp = filp_open(fn, O_RDWR | O_CREAT, 0666);
	if (!fp) {
		pr_info("Error open file for writing NULL\n");
	} else {
		vfs_write(fp, structure, struct_length, &pos);
		vfs_fsync(fp, 0);
		filp_close(fp, NULL);
	}
	set_fs(old_fs);
}

void dolby_vision_dump_struct(void)
{
	dump_struct(&dovi_setting.dm_reg1,
		    sizeof(dovi_setting.dm_reg1),
		    dm_reg_core1_file, frame_count);
	if (dovi_setting.el_flag)
		dump_struct(&dovi_setting.comp_reg,
			    sizeof(dovi_setting.comp_reg),
			    comp_file, frame_count);

	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_reg2,
			    sizeof(dovi_setting.dm_reg2),
			    dm_reg_core2_file, frame_count);

	dump_struct(&dovi_setting.dm_reg3,
		    sizeof(dovi_setting.dm_reg3),
		    dm_reg_core3_file, frame_count);

	dump_struct(&dovi_setting.dm_lut1,
		    sizeof(dovi_setting.dm_lut1),
		    dm_lut_core1_file, frame_count);
	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_lut2,
			    sizeof(dovi_setting.dm_lut2),
			    dm_lut_core2_file, frame_count);
	pr_dolby_dbg("setting for frame %d dumped\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_struct);

static void dump_setting
	(struct dovi_setting_s *setting,
	 int frame_cnt, int debug_flag)
{
	int i;
	u32 *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("path control reg\n");
		if (is_meson_txlx_stbmode()) {
			pr_info("txlx dv core1/2 reg: 0x1a07 val = 0x%x\n",
				READ_VPP_DV_REG(VIU_MISC_CTRL1));
		} else if (is_meson_g12() || is_meson_sc2() || is_meson_s4d()) {
			pr_info("g12/sc2/s4d  stb reg: 0x1a0c val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_CTRL));
		} else if (is_meson_tm2_stbmode()) {
			pr_info("tm2_stb reg: 0x1a0c val = 0x%x\n",
				READ_VPP_DV_REG(DOLBY_PATH_CTRL));
		} else if (is_meson_t7_stbmode()) {
			pr_info("t7_stb reg: vd1 core1 on 0x1a83 (bit4)val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD1_DSC_CTRL));
			pr_info("t7_stb reg: vd2 dv 0x1a84 val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD2_DSC_CTRL));
			pr_info("t7_stb reg: vd1 dv 0x1a85 val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD3_DSC_CTRL));
			pr_info("t7_stb reg: core2a (osd1_dolby_bypass_en bit14) 0x1a0f val = 0x%x\n",
				READ_VPP_DV_REG(MALI_AFBCD_TOP_CTRL));
			pr_info("t7_stb reg: core2c 0x1a55 (osd3_dolby_bypass_en bit19) val = 0x%x\n",
				READ_VPP_DV_REG(MALI_AFBCD1_TOP_CTRL));
		}

		pr_info("core1\n");
		p = (u32 *)&setting->dm_reg1;
		for (i = 0; i < 27; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\ncomposer\n");
		p = (u32 *)&setting->comp_reg;
		for (i = 0; i < 173; i++)
			pr_info("%08x\n", p[i]);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxm()) {
			pr_info("core1 swap\n");
			for (i = DOLBY_CORE1A_CLKGATE_CTRL;
				i <= DOLBY_CORE1A_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core1 real reg\n");
			for (i = DOLBY_CORE1A_REG_START;
				i <= DOLBY_CORE1A_REG_START + 5;
				i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core1 composer real reg\n");
			for (i = 0; i < 173 ; i++)
				pr_info("%08x\n",
					READ_VPP_DV_REG
					(DOLBY_CORE1A_REG_START
					 + 50 + i));
		} else if (is_meson_txlx_stbmode()) {
			pr_info("core1 swap\n");
			for (i = DOLBY_CORE1A_CLKGATE_CTRL;
				i <= DOLBY_CORE1A_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core1 real reg\n");
			for (i = DOLBY_CORE1A_REG_START;
				i <= DOLBY_CORE1A_REG_START + 5;
				i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		}
#endif
	}

	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ncore1lut\n");
		p = (u32 *)&setting->dm_lut1.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable && !is_graphics_output_off()) {
		pr_info("core2\n");
		p = (u32 *)&setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core2 swap\n");
		for (i = DOLBY_CORE2A_CLKGATE_CTRL;
			i <= DOLBY_CORE2A_DMA_PORT; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		pr_info("core2 real reg\n");
		for (i = DOLBY_CORE2A_REG_START;
			i <= DOLBY_CORE2A_REG_START + 5; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
	}

	if ((debug_flag & 0x20) && dump_enable && !is_graphics_output_off()) {
		pr_info("\ncore2lut\n");
		p = (u32 *)&setting->dm_lut2.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core3\n");
		p = (u32 *)&setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core3 swap\n");
		for (i = DOLBY_CORE3_CLKGATE_CTRL;
			i <= DOLBY_CORE3_OUTPUT_CSC_CRC; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		pr_info("core3 real reg\n");
		for (i = DOLBY_CORE3_REG_START;
			i <= DOLBY_CORE3_REG_START + 67; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
	}

	if ((debug_flag & 0x40) && dump_enable &&
	    dolby_vision_mode <= DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
		pr_info("\ncore3_meta %d\n", setting->md_reg3.size);
		p = setting->md_reg3.raw_metadata;
		for (i = 0; i < setting->md_reg3.size; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\n");
	}
}

void dolby_vision_dump_setting(int debug_flag)
{
	pr_dolby_dbg("\n====== setting for frame %d ======\n", frame_count);
	if (is_meson_tvmode())
		dump_tv_setting(tv_dovi_setting,
			frame_count, debug_flag);
	else
		dump_setting(&new_dovi_setting, frame_count, debug_flag);
	pr_dolby_dbg("=== setting for frame %d dumped ===\n\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_setting);

static int sink_support_dolby_vision(const struct vinfo_s *vinfo)
{
	if (dolby_vision_flags & FLAG_DISABLE_DOVI_OUT)
		return 0;
	return (sink_hdr_support(vinfo) & DV_SUPPORT) >> DV_SUPPORT_SHF;
}

static int sink_support_hdr(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDR_SUPPORT;
}

static int sink_support_hdr10_plus(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDRP_SUPPORT;
}

static int current_hdr_cap = -1; /* should set when probe */
static int current_sink_available;

static int is_policy_changed(void)
{
	int ret = 0;

	if (last_dolby_vision_policy != dolby_vision_policy) {
		/* handle policy change */
		pr_dolby_dbg("policy changed %d->%d\n",
			last_dolby_vision_policy,
			dolby_vision_policy);
		last_dolby_vision_policy = dolby_vision_policy;
		ret |= 1;
	}
	if (last_dolby_vision_ll_policy != dolby_vision_ll_policy) {
		/* handle ll policy change when dolby on */
		if (dolby_vision_on) {
			pr_dolby_dbg("ll policy changed %d->%d\n",
				     last_dolby_vision_ll_policy,
				     dolby_vision_ll_policy);
			last_dolby_vision_ll_policy = dolby_vision_ll_policy;
			ret |= 2;
		}
	}
	if (last_dolby_vision_hdr10_policy != dolby_vision_hdr10_policy) {
		/* handle policy change */
		pr_dolby_dbg("hdr10 policy changed %d->%d\n",
			last_dolby_vision_hdr10_policy,
			dolby_vision_hdr10_policy);
		last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
		ret |= 4;
	}
	return ret;
}

static bool vf_is_hdr10_plus(struct vframe_s *vf);
static bool vf_is_hdr10(struct vframe_s *vf);
static bool vf_is_hlg(struct vframe_s *vf);
static bool is_mvc_frame(struct vframe_s *vf);
static bool is_primesl_frame(struct vframe_s *vf);

static const char *input_str[8] = {
	"NONE",
	"HDR",
	"HDR+",
	"DOVI",
	"PRIME",
	"HLG",
	"SDR",
	"MVC"
};

/*update pwm control when src changed or pic mode changed*/
/*control pwm only in case : src=dovi and gdEnable=1*/
static void update_pwm_control(void)
{
	int gd_en = 0;

	if (is_meson_tvmode()) {
		if (pq_config_fake &&
			((struct pq_config *)
			pq_config_fake)->tdc.gd_config.gd_enable &&
			dolby_vision_src_format == 3)
			gd_en = 1;
		else
			gd_en = 0;

		pr_dolby_dbg("%s: %s, src %d, gd_en %d, bl %d\n",
			     __func__, cfg_info[cur_pic_mode].pic_mode_name,
			     dolby_vision_src_format, gd_en, tv_backlight);

#ifdef CONFIG_AMLOGIC_LCD
		if (!force_disable_dv_backlight) {
			aml_lcd_atomic_notifier_call_chain
			(LCD_EVENT_BACKLIGHT_GD_SEL, &gd_en);
			dv_control_backlight_status = gd_en > 0 ? true : false;
			if (gd_en)
				tv_backlight_force_update = true;
		}
#endif

	}
}

static void update_src_format
	(enum signal_format_enum src_format, struct vframe_s *vf)
{
	enum signal_format_enum cur_format = dolby_vision_src_format;

	if (src_format == FORMAT_DOVI ||
	    src_format == FORMAT_DOVI_LL) {
		dolby_vision_src_format = 3;
	} else {
		if (vf) {
			/* need check prime_sl before hdr and sdr */
			if (is_primesl_frame(vf))
				dolby_vision_src_format = 4;
			else if (vf_is_hdr10_plus(vf))
				dolby_vision_src_format = 2;
			else if (vf_is_hdr10(vf))
				dolby_vision_src_format = 1;
			else if (vf_is_hlg(vf))
				dolby_vision_src_format = 5;
			else if (is_mvc_frame(vf))
				dolby_vision_src_format = 7;
			else
				dolby_vision_src_format = 6;
		}
	}
	if (cur_format != dolby_vision_src_format) {
		update_pwm_control();
		pr_dolby_dbg
			("update src fmt: %s => %s, signal_type 0x%x, src fmt %d\n",
			input_str[cur_format],
			input_str[dolby_vision_src_format],
			vf ? vf->signal_type : 0,
			src_format);
		cur_format = dolby_vision_src_format;
	}
}

int get_dolby_vision_src_format(void)
{
	return dolby_vision_src_format;
}
EXPORT_SYMBOL(get_dolby_vision_src_format);

static enum signal_format_enum get_cur_src_format(void)
{
	int cur_format = dolby_vision_src_format;
	enum signal_format_enum ret = FORMAT_SDR;

	switch (cur_format) {
	case 1: /* HDR10 */
		ret = FORMAT_HDR10;
		break;
	case 2: /* HDR10+ */
		ret = FORMAT_HDR10PLUS;
		break;
	case 3: /* DOVI */
		ret = FORMAT_DOVI;
		break;
	case 4: /* PRIMESL */
		ret = FORMAT_PRIMESL;
		break;
	case 5: /* HLG */
		ret = FORMAT_HLG;
		break;
	case 6: /* SDR */
		ret = FORMAT_SDR;
		break;
	case 7: /* MVC */
		ret = FORMAT_MVC;
		break;
	default:
		break;
	}
	return ret;
}

static int dolby_vision_policy_process
	(int *mode, enum signal_format_enum src_format)
{
	const struct vinfo_s *vinfo;
	int mode_change = 0;

	if (!dolby_vision_enable || (!p_funcs_stb && !p_funcs_tv))
		return mode_change;

	if (is_meson_tvmode()) {
		if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
			if (*mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output mode change %d -> %d\n",
						     dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else if (*mode == DOLBY_VISION_OUTPUT_MODE_SDR8) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg
					("dovi tv output mode change %d->%d\n",
					 dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else {
				pr_dolby_error
					("not support dovi output mode %d\n",
					 *mode);
				return mode_change;
			}
		} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
			/* bypass dv_mode with efuse */
			if (efuse_mode == 1 && !dolby_vision_efuse_bypass) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else {
					mode_change = 0;
				}
				return mode_change;
			}

			if (cur_csc_type[VD1_PATH] != 0xffff &&
			    (get_hdr_module_status(VD1_PATH, VPP_TOP0)
			     == HDR_MODULE_ON)) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("src=%d, hdr module=ON, dovi tv output -> BYPASS\n",
						src_format);
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else {
					if (debug_dolby & 1)
						pr_dolby_dbg("src=%d, but hdr ON, dv keep BYPASS\n",
							     src_format);
				}
			} else if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL) ||
				((src_format == FORMAT_HDR10) &&
				(dolby_vision_hdr10_policy &
				 HDR_BY_DV_F_SINK)) ||
				((src_format == FORMAT_HLG) &&
				(dolby_vision_hdr10_policy &
				 HLG_BY_DV_F_SINK))) {
				if (dolby_vision_mode !=
				    DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("src=%d, dovi tv output -> SDR8\n",
						src_format);
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				    DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("src=%d, dovi tv output -> BYPASS\n",
						src_format);
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
			/* bypass dv_mode with efuse */
			if (efuse_mode == 1 && !dolby_vision_efuse_bypass) {
				if (dolby_vision_mode !=
				    DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else {
					mode_change = 0;
				}
				return mode_change;
			}
			if (cur_csc_type[VD1_PATH] != 0xffff &&
			    (get_hdr_module_status(VD1_PATH, VPP_TOP0)
			     == HDR_MODULE_ON)) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("src=%d, hdr module=ON, dovi tv output -> BYPASS\n",
						src_format);
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL) ||
				((src_format == FORMAT_HDR10) &&
				(dolby_vision_hdr10_policy &
				 HDR_BY_DV_F_SRC)) ||
				((src_format == FORMAT_HLG) &&
				(dolby_vision_hdr10_policy &
				 HLG_BY_DV_F_SRC))) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("src=%d, dovi tv output -> SDR8\n",
						src_format);
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("src=%d, dovi tv output -> BYPASS\n",
						src_format);
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		}
		if (src_format == FORMAT_SDR &&
		    (mode_change || *mode == dolby_vision_mode)) {
			if (*mode == DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_hdr10_policy &= ~SDR_BY_DV;
			else
				dolby_vision_hdr10_policy |= SDR_BY_DV;
		}
		return mode_change;
	}

	vinfo = get_current_vinfo();
	if (src_format == FORMAT_MVC) {
		if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dolby_dbg
					("mvc, dovi output -> BYPASS\n");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (src_format == FORMAT_PRIMESL) {
		if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dolby_dbg
					("prime_sl, dovi output -> BYPASS\n");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !dolby_vision_efuse_bypass)  {
			if (dolby_vision_mode !=
			    DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if (src_format == FORMAT_HLG ||
		    (src_format == FORMAT_HDR10PLUS &&
		    !(dolby_vision_hdr10_policy & HDRP_BY_DV))) {
			if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dolby_dbg("hlg/hdr+, dovi output -> BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		} else if (cur_csc_type[VD1_PATH] != 0xffff &&
			(get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_ON)) {
			/*if vpp is playing hlg/hdr10+*/
			/*dolby need bypass at this time*/
			if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dolby_dbg
				("src=%d, hdr module on, dovi output->BYPASS\n",
					 src_format);
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
		} else if (is_meson_stb_hdmimode() &&
			(src_format == FORMAT_DOVI) &&
			sink_support_dolby_vision(vinfo)) {
			/* HDMI DV sink-led in and TV support dv */
			if (dolby_vision_ll_policy !=
			DOLBY_VISION_LL_DISABLE &&
			!dolby_vision_core1_on)
				pr_dolby_error
					("hdmi in sink-led but output is source-led!\n");
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dolby_dbg("hdmi dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
				*mode =	DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_dolby_vision(vinfo)) {
			/* TV support DOVI, All -> DOVI */
			if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dolby_dbg("src=%d, dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n",
					src_format);
				*mode = DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_hdr(vinfo)) {
			/* TV support HDR, All -> HDR */
			if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_HDR10) {
				pr_dolby_dbg("src=%d, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n",
					src_format);
				*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
				mode_change = 1;
			}
		} else {
			/* TV not support DOVI and HDR */
			if (src_format == FORMAT_DOVI ||
			    src_format == FORMAT_DOVI_LL) {
				/* DOVI to SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (src_format == FORMAT_HDR10) {
				if (dolby_vision_hdr10_policy
					& HDR_BY_DV_F_SINK) {
					if (dolby_vision_mode !=
					DOLBY_VISION_OUTPUT_MODE_SDR8) {
						/* HDR10 to SDR */
						pr_dolby_dbg("hdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
						*mode =
						DOLBY_VISION_OUTPUT_MODE_SDR8;
						mode_change = 1;
					}
				} else if (dolby_vision_mode !=
					DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					/* HDR bypass */
					pr_dolby_dbg("hdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if (is_meson_g12b_cpu() || is_meson_g12a_cpu()) {
				/* dv cores keep on if in sdr mode */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					/* SDR to SDR */
					pr_dolby_dbg("sdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				/* HDR/SDR bypass */
				pr_dolby_dbg("sdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !dolby_vision_efuse_bypass) {
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if (cur_csc_type[VD1_PATH] != 0xffff &&
		    (get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_ON) &&
		    (!((src_format == FORMAT_DOVI) ||
		    (src_format == FORMAT_DOVI_LL)))) {
			/* bypass dolby incase VPP is not in sdr mode */
			if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				pr_dolby_dbg("hdr module on, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
		} else if (is_meson_stb_hdmimode() &&
			(src_format == FORMAT_DOVI)) {
			/* HDMI DV sink-led in and TV support */
			if (sink_support_dolby_vision(vinfo)) {
				/* support dv sink-led or source-led*/
				if (dolby_vision_ll_policy !=
				DOLBY_VISION_LL_DISABLE &&
				!dolby_vision_core1_on)
					pr_dolby_error
						("hdmi in sink-led but output is source-led!\n");
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dolby_dbg("hdmi dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_HDR10) {
					pr_dolby_dbg("hdmi dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("hdmi dovi,, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_DOVI) ||
			(src_format == FORMAT_DOVI_LL)) {
			/* DOVI source */
			if (vinfo && sink_support_dolby_vision(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_HDR10) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_HDR10) &&
			(dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC)) {
			if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, HDR -> HDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_HDR10) {
					pr_dolby_dbg("hdr10, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					/* HDR10 to SDR */
					pr_dolby_dbg("hdr10, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			/* HDR/SDR bypass */
			pr_dolby_dbg("sdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
		if (dolby_vision_mode != *mode) {
			pr_dolby_dbg("src=%d, dovi output mode change %d -> %d\n",
				src_format, dolby_vision_mode, *mode);
			mode_change = 1;
		}
	}

	if (src_format == FORMAT_SDR &&
	    (mode_change || *mode == dolby_vision_mode)) {
		if (*mode == DOLBY_VISION_OUTPUT_MODE_BYPASS)
			dolby_vision_hdr10_policy &= ~SDR_BY_DV;
		else
			dolby_vision_hdr10_policy |= SDR_BY_DV;
	}
	return mode_change;
}

static char dv_provider[32] = "dvbldec";

void dolby_vision_set_provider(char *prov_name)
{
	if (prov_name && strlen(prov_name) < 32) {
		if (strcmp(dv_provider, prov_name)) {
			strcpy(dv_provider, prov_name);
		pr_dolby_dbg("provider changed to %s\n", dv_provider);
		}
	}
}
EXPORT_SYMBOL(dolby_vision_set_provider);

/* 0: no dv, 1: dv std, 2: dv ll */
int is_dovi_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	enum vframe_signal_fmt_e fmt;

	if (!vf)
		return 0;

	fmt = get_vframe_src_fmt(vf);
	if (fmt == VFRAME_SIGNAL_FMT_DOVI)
		return true;

	if (fmt != VFRAME_SIGNAL_FMT_INVALID)
		return false;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	req.low_latency = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
	    (is_meson_tvmode() ||
	    ((is_meson_tm2_stbmode() || is_meson_t7_stbmode()) &&
	    hdmi_to_stb_policy))) {
		vf_notify_provider_by_name
			("dv_vdin",
			 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			 (void *)&req);
		if (debug_dolby & 2)
			pr_dolby_dbg("is_dovi: vf %p, emp.size %d, aux_size %d\n",
				     vf, vf->emp.size,
				     req.aux_size);
		if ((req.aux_buf && req.aux_size) ||
			(dolby_vision_flags & FLAG_FORCE_DOVI_LL))
			return 1;
		if (req.low_latency)
			return 2;
		p = vf->emp.addr;
		if (p && vf->emp.size > 0 &&
		    p[0] == 0x7f &&
		    p[10] == 0x46 &&
		    p[11] == 0xd0) {
			if (p[13] == 0)
				return 1;
			else
				return 2;
		}
		return 0;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (!strcmp(dv_provider, "dvbldec"))
			vf_notify_provider_by_name
				(dv_provider,
				 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				 (void *)&req);
		if (req.dv_enhance_exist)
			return 1;
		if (!req.aux_buf || !req.aux_size)
			return 0;
		p = req.aux_buf;
		while (p < req.aux_buf + req.aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == DV_SEI ||
			    (type & 0xffff0000) == DV_AV1_SEI)
				return 1;
			p += size;
		}
	}
	return 0;
}
EXPORT_SYMBOL(is_dovi_frame);

bool is_dovi_dual_layer_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	enum vframe_signal_fmt_e fmt;

	if (!vf)
		return false;

	if (!enable_fel)
		return false;

	fmt = get_vframe_src_fmt(vf);
	/* valid src_fmt = DOVI or invalid src_fmt will check dual layer */
	/* otherwise, it certainly is a non-dv vframe */
	if (fmt != VFRAME_SIGNAL_FMT_DOVI &&
	    fmt != VFRAME_SIGNAL_FMT_INVALID)
		return false;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (!strcmp(dv_provider, "dvbldec"))
			vf_notify_provider_by_name(dv_provider,
			 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			 (void *)&req);
		if (req.dv_enhance_exist)
			return true;
	}
	return false;
}
EXPORT_SYMBOL(is_dovi_dual_layer_frame);

#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)

static bool vf_is_hlg(struct vframe_s *vf)
{
	if ((signal_transfer_characteristic == 14 ||
	     signal_transfer_characteristic == 18) &&
	     signal_color_primaries == 9)
		return true;
	return false;
}

static bool is_hlg_frame(struct vframe_s *vf)
{
	if (!vf)
		return false;
	if ((is_meson_tm2_tvmode() || is_meson_t7_tvmode() ||
	    is_meson_t3_tvmode() ||
	    is_meson_t5w() ||
	    (get_dolby_vision_hdr_policy() & 2) == 0) &&
	    (signal_transfer_characteristic == 14 ||
	    signal_transfer_characteristic == 18) &&
	    signal_color_primaries == 9)
		return true;

	return false;
}

static bool vf_is_hdr10_plus(struct vframe_s *vf)
{
	if (signal_transfer_characteristic == 0x30 &&
	    (signal_color_primaries == 9 ||
	    signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_hdr10plus_frame(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return false;
	if (!(dolby_vision_hdr10_policy & HDRP_BY_DV)) {
		/* report hdr10 for the content hdr10+ and
		 * sink is hdr10+ case
		 */
		if (signal_transfer_characteristic == 0x30 &&
		    (is_meson_tvmode() || sink_support_hdr10_plus(vinfo)) &&
		    (signal_color_primaries == 9 ||
		    signal_color_primaries == 2))
			return true;
	}
	return false;
}

static bool vf_is_hdr10(struct vframe_s *vf)
{
	if (signal_transfer_characteristic == 16 &&
	    (signal_color_primaries == 9 ||
	    signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_hdr10_frame(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return false;
	if ((signal_transfer_characteristic == 16 ||
		/* report as hdr10 for the content hdr10+ and
		 * sink not support hdr10+ or use DV to handle
		 * hdr10+ as hdr10
		 */
		(signal_transfer_characteristic == 0x30 &&
		((!sink_support_hdr10_plus(vinfo) && !is_meson_tvmode()) ||
		(dolby_vision_hdr10_policy & HDRP_BY_DV)))) &&
		(signal_color_primaries == 9 ||
		 signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_mvc_frame(struct vframe_s *vf)
{
	if (!vf)
		return false;
	if (vf->type & VIDTYPE_MVC)
		return true;
	return false;
}

static bool is_primesl_frame(struct vframe_s *vf)
{
	enum vframe_signal_fmt_e fmt;

	if (!vf)
		return false;

	fmt = get_vframe_src_fmt(vf);
	if (fmt == VFRAME_SIGNAL_FMT_HDR10PRIME)
		return true;

	return false;
}

int dolby_vision_check_mvc(struct vframe_s *vf)
{
	int mode;

	if (is_mvc_frame(vf) && dolby_vision_on) {
		/* mvc source, but dovi enabled, need bypass dv */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_MVC)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_mvc);

int dolby_vision_check_hlg(struct vframe_s *vf)
{
	int mode;

	if (!is_dovi_frame(vf) && is_hlg_frame(vf) && !dolby_vision_on) {
		/* hlg source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_HLG)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hlg);

int dolby_vision_check_hdr10plus(struct vframe_s *vf)
{
	int mode;

	if (!is_dovi_frame(vf) && is_hdr10plus_frame(vf) && !dolby_vision_on) {
		/* hdr10+ source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_HDR10PLUS)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10plus);

int dolby_vision_check_hdr10(struct vframe_s *vf)
{
	int mode;

	if (!is_dovi_frame(vf) && is_hdr10_frame(vf) && !dolby_vision_on) {
		/* hdr10 source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_HDR10)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10);

int dolby_vision_check_primesl(struct vframe_s *vf)
{
	int mode;

	if (is_primesl_frame(vf) && dolby_vision_on) {
		/* primesl source, but dovi enabled, need bypass dv */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_PRIMESL)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_primesl);

void dolby_vision_vf_put(struct vframe_s *vf)
{
	int i;

	if (vf)
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]) {
					if (debug_dolby & 2)
						pr_dolby_dbg
					("put bl(%p-%lld) with el(%p-%lld)\n",
					 vf, vf->pts_us64,
					 dv_vf[i][1],
					 dv_vf[i][1]->pts_us64);
					dvel_vf_put(dv_vf[i][1]);
				} else if (debug_dolby & 2) {
					pr_dolby_dbg("--- put bl(%p-%lld) ---\n",
						vf, vf->pts_us64);
				}
				dv_vf[i][0] = NULL;
				dv_vf[i][1] = NULL;
			}
		}
}
EXPORT_SYMBOL(dolby_vision_vf_put);

struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf)
{
	int i;

	if (dolby_vision_flags && (p_funcs_stb || p_funcs_tv)) {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1] &&
				    dolby_vision_status == BYPASS_PROCESS &&
				    !is_dolby_vision_on())
					dv_vf[i][1]->type |= VIDTYPE_VD2;
				return dv_vf[i][1];
			}
		}
	}
	return NULL;
}
EXPORT_SYMBOL(dolby_vision_vf_peek_el);

static void dolby_vision_vf_add(struct vframe_s *vf, struct vframe_s *el_vf)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (!dv_vf[i][0]) {
			dv_vf[i][0] = vf;
			dv_vf[i][1] = el_vf;
			break;
		}
	}
}

static int dolby_vision_vf_check(struct vframe_s *vf)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (dv_vf[i][0] == vf) {
			if (debug_dolby & 2) {
				if (dv_vf[i][1])
					pr_dolby_dbg("=== bl(%p-%lld) with el(%p-%lld) toggled ===\n",
						     vf,
						     vf->pts_us64,
						     dv_vf[i][1],
						     dv_vf[i][1]->pts_us64);
				else
					pr_dolby_dbg("=== bl(%p-%lld) toggled ===\n",
						     vf,
						     vf->pts_us64);
			}
			return 0;
		}
	}
	return 1;
}

static bool check_atsc_dvb(char *p)
{
	u32 country_code;
	u32 provider_code;
	u32 user_id;
	u32 user_type_code;

	if (!p)
		return false;

	country_code = *(p + 4);
	provider_code = (*(p + 5) << 8) |
			*(p + 6);
	user_id = (*(p + 7) << 24) |
		(*(p + 8) << 16) |
		(*(p + 9) << 8) |
		(*(p + 10));
	user_type_code = *(p + 11);
	if (country_code == 0xB5 &&
	    ((provider_code ==
	    ATSC_T35_PROV_CODE &&
	    user_id == ATSC_USER_ID_CODE) ||
	    (provider_code ==
	    DVB_T35_PROV_CODE &&
	    user_id == DVB_USER_ID_CODE)) &&
	    user_type_code ==
	    DM_MD_USER_TYPE_CODE)
		return true;
	else
		return false;
}

/*return 0: parse ok; 1,2,3: parse err */
int parse_sei_and_meta_ext(struct vframe_s *vf,
	 char *aux_buf,
	 int aux_size,
	 int *total_comp_size,
	 int *total_md_size,
	 void *fmt,
	 int *ret_flags,
	 char *md_buf,
	 char *comp_buf)
{
	int i;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	int md_size = 0;
	int comp_size = 0;
	int parser_ready = 0;
	int ret = 2;
	unsigned long flags;
	bool parser_overflow = false;
	int rpu_ret = 0;
	int reset_flag = 0;
	unsigned int rpu_size = 0;
	enum signal_format_enum *src_format = (enum signal_format_enum *)fmt;
	static int parse_process_count;
	int dump_size = 100;
	static u32 last_play_id;

	if (!aux_buf || aux_size == 0 || !fmt || !md_buf || !comp_buf ||
	    !total_comp_size || !total_md_size || !ret_flags)
		return 1;

	parse_process_count++;
	if (parse_process_count > 1) {
		pr_err("parser not support multi instance\n");
		ret = 1;
		goto parse_err;
	}

	/* release metadata_parser when new playing */
	if (vf && vf->src_fmt.play_id != last_play_id) {
		if (metadata_parser) {
			if (p_funcs_stb)
				p_funcs_stb->metadata_parser_release();
			if (p_funcs_tv)
				p_funcs_tv->metadata_parser_release();
			metadata_parser = NULL;
			pr_dolby_dbg("new play, release parser\n");
			dolby_vision_clear_buf();
		}
		last_play_id = vf->src_fmt.play_id;
		if (debug_dolby & 2)
			pr_dolby_dbg("update play id=%d:\n", last_play_id);
	}

	p = aux_buf;
	while (p < aux_buf + aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		if (debug_dolby & 4)
			pr_dolby_dbg("metadata type=%08x, size=%d:\n",
				     type, size);
		if (size == 0 || size > aux_size) {
			pr_dolby_dbg("invalid aux size %d\n", size);
			ret = 1;
			goto parse_err;
		}
		if (type == DV_SEI || /* hevc t35 sei */
			(type & 0xffff0000) == DV_AV1_SEI) { /* av1 t35 obu */
			*total_comp_size = 0;
			*total_md_size = 0;

			if ((type & 0xffff0000) == DV_AV1_SEI &&
			    p[0] == 0xb5 &&
			    p[1] == 0x00 &&
			    p[2] == 0x3b &&
			    p[3] == 0x00 &&
			    p[4] == 0x00 &&
			    p[5] == 0x08 &&
			    p[6] == 0x00 &&
			    p[7] == 0x37 &&
			    p[8] == 0xcd &&
			    p[9] == 0x08) {
				/* AV1 dv meta in obu */
				*src_format = FORMAT_DOVI;
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				meta_buf[3] = 0x01;
				meta_buf[4] = 0x19;
				if (p[11] & 0x10) {
					rpu_size = 0x100;
					rpu_size |= (p[11] & 0x0f) << 4;
					rpu_size |= (p[12] >> 4) & 0x0f;
					if (p[12] & 0x08) {
						pr_dolby_error
							("rpu in obu exceed 512 bytes\n");
						break;
					}
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[12 + i] & 0x07) << 5;
						meta_buf[5 + i] |=
							(p[13 + i] >> 3) & 0x1f;
					}
					rpu_size += 5;
				} else {
					rpu_size = (p[10] & 0x1f) << 3;
					rpu_size |= (p[11] >> 5) & 0x07;
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[11 + i] & 0x0f) << 4;
						meta_buf[5 + i] |=
							(p[12 + i] >> 4) & 0x0f;
					}
					rpu_size += 5;
				}
			} else {
				/* HEVC dv meta in sei */
				*src_format = FORMAT_DOVI;
				if (size > (sizeof(meta_buf) - 3))
					size = (sizeof(meta_buf) - 3);
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				memcpy(&meta_buf[3], p + 1, size - 1);
				rpu_size = size + 2;
			}
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", rpu_size);
				for (i = 0; i < size; i += 16)
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7],
						meta_buf[i + 8],
						meta_buf[i + 9],
						meta_buf[i + 10],
						meta_buf[i + 11],
						meta_buf[i + 12],
						meta_buf[i + 13],
						meta_buf[i + 14],
						meta_buf[i + 15]);
			}
			if (tv_mode) {
				if (!p_funcs_tv) {
					ret = 1;
					goto parse_err;
				}
			} else {
				if (!p_funcs_stb) {
					ret = 1;
					goto parse_err;
				}
			}
			/* prepare metadata parser */
			spin_lock_irqsave(&dovi_lock, flags);
			parser_ready = 0;
			if (!metadata_parser) {
				if (is_meson_tvmode()) {
					if (p_funcs_tv) {
						metadata_parser =
						p_funcs_tv->metadata_parser_init
						(dolby_vision_flags
						 & FLAG_CHANGE_SEQ_HEAD
						 ? 1 : 0);
						p_funcs_tv->metadata_parser_reset(1);
					} else {
						pr_dolby_dbg("p_funcs_tv is null\n");
					}
				} else {
					if (p_funcs_stb) {
						metadata_parser =
						p_funcs_stb->metadata_parser_init
						(dolby_vision_flags
						 & FLAG_CHANGE_SEQ_HEAD
						 ? 1 : 0);
						p_funcs_stb->metadata_parser_reset(1);
					} else {
						pr_dolby_dbg("p_funcs_stb is null\n");
					}
				}
				if (metadata_parser) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dolby_dbg("metadata parser init OK\n");
				}
			} else {
				if (is_meson_tvmode()) {
					if (p_funcs_tv->metadata_parser_reset
					    (metadata_parser_reset_flag) == 0)
						metadata_parser_reset_flag = 0;
				} else {
					if (p_funcs_stb->metadata_parser_reset
					    (metadata_parser_reset_flag) == 0)
						metadata_parser_reset_flag = 0;
				}
				parser_ready = 1;
			}
			if (!parser_ready) {
				spin_unlock_irqrestore(&dovi_lock, flags);
				pr_dolby_error
				("meta(%d), pts(%lld) -> parser init fail\n",
					size, vf->pts_us64);
				*total_comp_size = backup_comp_size;
				*total_md_size = backup_md_size;
				ret = 2;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (is_meson_tvmode())
				rpu_ret =
				p_funcs_tv->metadata_parser_process
				(meta_buf, rpu_size,
				 comp_buf + *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true);
			else
				rpu_ret =
				p_funcs_stb->metadata_parser_process
				(meta_buf, rpu_size,
				 comp_buf + *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true);

			if (rpu_ret < 0) {
				if (vf)
					pr_dolby_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					rpu_size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			spin_unlock_irqrestore(&dovi_lock, flags);
			if (parser_overflow) {
				ret = 2;
				break;
			}
			/*dolby type just appears once in metadata
			 *after parsing dolby type,breaking the
			 *circulation directly
			 */
			break;
		}
		p += size;
	}
	/*continue to check atsc/dvb dv*/
	if (atsc_sei && *src_format != FORMAT_DOVI) {
		struct dv_vui_parameters vui_param;
		static u32 len_2086_sei;
		u32 len_2094_sei = 0;
		static u8 payload_2086_sei[MAX_LEN_2086_SEI];
		u8 payload_2094_sei[MAX_LEN_2094_SEI];
		unsigned char nal_type;
		unsigned char sei_payload_type = 0;
		unsigned char sei_payload_size = 0;

		if (vf) {
			vui_param.video_fmt_i = (vf->signal_type >> 26) & 7;
			vui_param.video_fullrange_b = (vf->signal_type >> 25) & 1;
			vui_param.color_description_b = (vf->signal_type >> 24) & 1;
			vui_param.color_primaries_i = (vf->signal_type >> 16) & 0xff;
			vui_param.trans_characteristic_i =
						(vf->signal_type >> 8) & 0xff;
			vui_param.matrix_coeff_i = (vf->signal_type) & 0xff;
			if (debug_dolby & 2)
				pr_dolby_dbg("vui_param %d, %d, %d, %d, %d, %d\n",
					vui_param.video_fmt_i,
					vui_param.video_fullrange_b,
					vui_param.color_description_b,
					vui_param.color_primaries_i,
					vui_param.trans_characteristic_i,
					vui_param.matrix_coeff_i);
		}

		p = aux_buf;

		if ((debug_dolby & 0x200) && dump_enable) {
			pr_dolby_dbg("aux_buf(%d):\n", aux_size);
			for (i = 0; i < aux_size; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					p[i],
					p[i + 1],
					p[i + 2],
					p[i + 3],
					p[i + 4],
					p[i + 5],
					p[i + 6],
					p[i + 7]);
		}
		while (p < aux_buf + aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (debug_dolby & 2)
				pr_dolby_dbg("type: 0x%x\n", type);

			/*4 byte size + 4 byte type*/
			/*1 byte nal_type + 1 byte (layer_id+temporal_id)*/
			/*1 byte payload type + 1 byte size + payload data*/
			if (type == 0x02000000) {
				nal_type = ((*p) & 0x7E) >> 1; /*nal unit type*/
				if (debug_dolby & 2)
					pr_dolby_dbg("nal_type: %d\n",
						     nal_type);

				if (nal_type == PREFIX_SEI_NUT_NAL ||
					nal_type == SUFFIX_SEI_NUT_NAL) {
					sei_payload_type = *(p + 2);
					sei_payload_size = *(p + 3);
					if (debug_dolby & 2)
						pr_dolby_dbg("type %d, size %d\n",
							     sei_payload_type,
							     sei_payload_size);
					if (sei_payload_type ==
					SEI_TYPE_MASTERING_DISP_COLOUR_VOLUME) {
						len_2086_sei =
							sei_payload_size;
						memcpy(payload_2086_sei, p + 4,
						       len_2086_sei);
					} else if (sei_payload_type == SEI_ITU_T_T35 &&
						sei_payload_size >= 8 && check_atsc_dvb(p)) {
						len_2094_sei = sei_payload_size;
						memcpy(payload_2094_sei, p + 4,
						       len_2094_sei);
					}
					if (len_2086_sei > 0 &&
					    len_2094_sei > 0)
						break;
				}
			}
			p += size;
		}
		if (len_2094_sei > 0) {
			/* source is VS10 */
			*total_comp_size = 0;
			*total_md_size = 0;
			*src_format = FORMAT_DOVI;
			if (vf)
				p_atsc_md.vui_param = vui_param;
			p_atsc_md.len_2086_sei = len_2086_sei;
			memcpy(p_atsc_md.sei_2086, payload_2086_sei,
			       len_2086_sei);
			p_atsc_md.len_2094_sei = len_2094_sei;
			memcpy(p_atsc_md.sei_2094, payload_2094_sei,
			       len_2094_sei);
			size = sizeof(struct dv_atsc);
			if (size > sizeof(meta_buf))
				size = sizeof(meta_buf);
			memcpy(meta_buf, (unsigned char *)(&p_atsc_md), size);
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", size);
				for (i = 0; i < size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7]);
			}
			if (tv_mode) {
				if (!p_funcs_tv) {
					ret = 1;
					goto parse_err;
				}
			} else {
				if (!p_funcs_stb) {
					ret = 1;
					goto parse_err;
				}
			}
			/* prepare metadata parser */
			spin_lock_irqsave(&dovi_lock, flags);
			parser_ready = 0;
			reset_flag = 2; /*flag: bit0 flag, bit1 0->dv, 1->atsc*/

			if (!metadata_parser) {
				if (is_meson_tvmode()) {
					metadata_parser =
					p_funcs_tv->metadata_parser_init
						(dolby_vision_flags
						& FLAG_CHANGE_SEQ_HEAD
						? 1 : 0);
					p_funcs_tv->metadata_parser_reset
						(reset_flag | 1);
				} else {
					metadata_parser =
					p_funcs_stb->metadata_parser_init
						(dolby_vision_flags
						& FLAG_CHANGE_SEQ_HEAD
						? 1 : 0);
					p_funcs_stb->metadata_parser_reset
						(reset_flag | 1);
				}
				if (metadata_parser) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dolby_dbg("metadata parser init OK\n");
				}
			} else {
				if (is_meson_tvmode()) {
					if (p_funcs_tv->metadata_parser_reset
						(reset_flag |
						metadata_parser_reset_flag
							) == 0)
						metadata_parser_reset_flag = 0;
				} else {
					if (p_funcs_stb->metadata_parser_reset
						(reset_flag |
						metadata_parser_reset_flag
							) == 0)
						metadata_parser_reset_flag = 0;
				}
				parser_ready = 1;
			}
			if (!parser_ready) {
				spin_unlock_irqrestore(&dovi_lock, flags);
				if (vf)
					pr_dolby_error
					("meta(%d), pts(%lld) -> metadata parser init fail\n",
					size, vf->pts_us64);
				*total_comp_size = backup_comp_size;
				*total_md_size = backup_md_size;
				ret = 2;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (is_meson_tvmode())
				rpu_ret =
				p_funcs_tv->metadata_parser_process
				(meta_buf, size,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true);
			else
				rpu_ret =
				p_funcs_stb->metadata_parser_process
				(meta_buf, size,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true);

			if (rpu_ret < 0) {
				if (vf)
					pr_dolby_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			spin_unlock_irqrestore(&dovi_lock, flags);
			if (parser_overflow)
				ret = 2;
		} else {
			len_2086_sei = 0;
		}
	}

	if (*total_md_size) {
		if ((debug_dolby & 1) && vf)
			pr_dolby_dbg
			("meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			 size, vf->pts_us64,
			 *total_md_size, *total_comp_size);
		if ((debug_dolby & 4) && dump_enable)  {
			pr_dolby_dbg("parsed md(%d):\n", *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i + 1],
					md_buf[i + 2],
					md_buf[i + 3],
					md_buf[i + 4],
					md_buf[i + 5],
					md_buf[i + 6],
					md_buf[i + 7]);
			}
			pr_dolby_dbg("parsed comp(%d):\n", *total_comp_size);
			if (*total_comp_size < dump_size)
				dump_size = *total_comp_size;
			for (i = 0; i < dump_size + 7; i += 8)
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					comp_buf[i],
					comp_buf[i + 1],
					comp_buf[i + 2],
					comp_buf[i + 3],
					comp_buf[i + 4],
					comp_buf[i + 5],
					comp_buf[i + 6],
					comp_buf[i + 7]);
		}
	}
parse_err:
	parse_process_count--;
	return ret;
}
EXPORT_SYMBOL(parse_sei_and_meta_ext);

static int parse_sei_and_meta
	(struct vframe_s *vf,
	 struct provider_aux_req_s *req,
	 int *total_comp_size,
	 int *total_md_size,
	 enum signal_format_enum *src_format,
	 int *ret_flags, bool drop_flag)
{
	int ret = 2;
	char *p_md_buf;
	char *p_comp_buf;
	int next_id = current_id ^ 1;

	if (!req->aux_buf || req->aux_size == 0)
		return 1;

	if (drop_flag) {
		p_md_buf = drop_md_buf[next_id];
		p_comp_buf = drop_comp_buf[next_id];
	} else {
		p_md_buf = md_buf[next_id];
		p_comp_buf = comp_buf[next_id];
	}
	ret = parse_sei_and_meta_ext(vf,
				     req->aux_buf,
				     req->aux_size,
				     total_comp_size,
				     total_md_size,
				     src_format,
				     ret_flags,
				     p_md_buf,
				     p_comp_buf);

	if (ret == 0) {
		current_id = next_id;
		backup_comp_size = *total_comp_size;
		backup_md_size = *total_md_size;
	}
	if (ret == 3) {
		*total_comp_size = backup_comp_size;
		*total_md_size = backup_md_size;
	}
	return ret;
}

#define INORM	50000
static u32 bt2020_primaries[3][2] = {
	{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},	/* G */
	{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},	/* B */
	{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},	/* R */
};

static u32 p3_primaries[3][2] = {
	{0.265 * INORM + 0.5, 0.69 * INORM + 0.5},	/* G */
	{0.15 * INORM + 0.5, 0.06 * INORM + 0.5},	/* B */
	{0.68 * INORM + 0.5, 0.32 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static u32 p3_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

void prepare_hdr10_param(struct vframe_master_display_colour_s *p_mdc,
			 struct hdr10_parameter *p_hdr10_param)
{
	struct vframe_content_light_level_s *p_cll =
		&p_mdc->content_light_level;
	u8 flag = 0;
	u32 max_lum = 1000 * 10000;
	u32 min_lum = 50;
	int primaries_type = 0;

	if (get_primary_policy() == PRIMARIES_NATIVE ||
		primary_debug == 1 ||
		(dolby_vision_flags & FLAG_CERTIFICAION) ||
		!strcasecmp(cfg_info[cur_pic_mode].pic_mode_name,
		"hdr10_dark")) {
		p_hdr10_param->min_display_mastering_lum = min_lum;
		p_hdr10_param->max_display_mastering_lum = max_lum;
		p_hdr10_param->r_x = bt2020_primaries[2][0];
		p_hdr10_param->r_y = bt2020_primaries[2][1];
		p_hdr10_param->g_x = bt2020_primaries[0][0];
		p_hdr10_param->g_y = bt2020_primaries[0][1];
		p_hdr10_param->b_x = bt2020_primaries[1][0];
		p_hdr10_param->b_y = bt2020_primaries[1][1];
		p_hdr10_param->w_x = bt2020_white_point[0];
		p_hdr10_param->w_y = bt2020_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_frame_avg_light_level = 0;
		return;
	} else if (get_primary_policy() == PRIMARIES_AUTO ||
		primary_debug == 2) {
		p_hdr10_param->min_display_mastering_lum = min_lum;
		p_hdr10_param->max_display_mastering_lum = max_lum;
		p_hdr10_param->r_x = p3_primaries[2][0];
		p_hdr10_param->r_y = p3_primaries[2][1];
		p_hdr10_param->g_x = p3_primaries[0][0];
		p_hdr10_param->g_y = p3_primaries[0][1];
		p_hdr10_param->b_x = p3_primaries[1][0];
		p_hdr10_param->b_y = p3_primaries[1][1];
		p_hdr10_param->w_x = p3_white_point[0];
		p_hdr10_param->w_y = p3_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_frame_avg_light_level = 0;
		return;
	}

	primaries_type = get_primaries_type(p_mdc);
	if (primaries_type == 2) {
		/* GBR -> RGB as dolby will swap back to GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->max_display_mastering_lum !=
		    p_mdc->luminance[0] ||
		    p_hdr10_param->min_display_mastering_lum !=
		    p_mdc->luminance[1] ||
		    p_hdr10_param->r_x != p_mdc->primaries[2][0] ||
		    p_hdr10_param->r_y != p_mdc->primaries[2][1] ||
		    p_hdr10_param->g_x != p_mdc->primaries[0][0] ||
		    p_hdr10_param->g_y != p_mdc->primaries[0][1] ||
		    p_hdr10_param->b_x != p_mdc->primaries[1][0] ||
		    p_hdr10_param->b_y != p_mdc->primaries[1][1] ||
		    p_hdr10_param->w_x != p_mdc->white_point[0] ||
		    p_hdr10_param->w_y != p_mdc->white_point[1]) {
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
				p_mdc->luminance[0];
			p_hdr10_param->min_display_mastering_lum =
				p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[2][0];
			p_hdr10_param->r_y = p_mdc->primaries[2][1];
			p_hdr10_param->g_x = p_mdc->primaries[0][0];
			p_hdr10_param->g_y = p_mdc->primaries[0][1];
			p_hdr10_param->b_x = p_mdc->primaries[1][0];
			p_hdr10_param->b_y = p_mdc->primaries[1][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
		}
	} else if (primaries_type == 1) {
		/* RGB -> RGB and dolby will swap to send as GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->max_display_mastering_lum !=
		    p_mdc->luminance[0] ||
		    p_hdr10_param->min_display_mastering_lum !=
		    p_mdc->luminance[1] ||
		    p_hdr10_param->r_x != p_mdc->primaries[0][0] ||
		    p_hdr10_param->r_y != p_mdc->primaries[0][1] ||
		    p_hdr10_param->g_x != p_mdc->primaries[1][0] ||
		    p_hdr10_param->g_y != p_mdc->primaries[1][1] ||
		    p_hdr10_param->b_x != p_mdc->primaries[2][0] ||
		    p_hdr10_param->b_y != p_mdc->primaries[2][1] ||
		    p_hdr10_param->w_x != p_mdc->white_point[0] ||
		    p_hdr10_param->w_y != p_mdc->white_point[1]) {
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
				p_mdc->luminance[0];
			p_hdr10_param->min_display_mastering_lum =
				p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[0][0];
			p_hdr10_param->r_y = p_mdc->primaries[0][1];
			p_hdr10_param->g_x = p_mdc->primaries[1][0];
			p_hdr10_param->g_y = p_mdc->primaries[1][1];
			p_hdr10_param->b_x = p_mdc->primaries[2][0];
			p_hdr10_param->b_y = p_mdc->primaries[2][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
		}
	} else {
		/* GBR -> RGB as dolby will swap back to GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->min_display_mastering_lum !=
		    min_lum ||
		    p_hdr10_param->max_display_mastering_lum !=
		    max_lum ||
		    p_hdr10_param->r_x != bt2020_primaries[2][0] ||
		    p_hdr10_param->r_y != bt2020_primaries[2][1] ||
		    p_hdr10_param->g_x != bt2020_primaries[0][0] ||
		    p_hdr10_param->g_y != bt2020_primaries[0][1] ||
		    p_hdr10_param->b_x != bt2020_primaries[1][0] ||
		    p_hdr10_param->b_y != bt2020_primaries[1][1] ||
		    p_hdr10_param->w_x != bt2020_white_point[0] ||
		    p_hdr10_param->w_y != bt2020_white_point[1]) {
			flag |= 2;
			p_hdr10_param->min_display_mastering_lum =
				min_lum;
			p_hdr10_param->max_display_mastering_lum =
				max_lum;
			p_hdr10_param->r_x = bt2020_primaries[2][0];
			p_hdr10_param->r_y = bt2020_primaries[2][1];
			p_hdr10_param->g_x = bt2020_primaries[0][0];
			p_hdr10_param->g_y = bt2020_primaries[0][1];
			p_hdr10_param->b_x = bt2020_primaries[1][0];
			p_hdr10_param->b_y = bt2020_primaries[1][1];
			p_hdr10_param->w_x = bt2020_white_point[0];
			p_hdr10_param->w_y = bt2020_white_point[1];
		}
	}

	if (p_cll->present_flag) {
		if (p_hdr10_param->max_content_light_level
		    != p_cll->max_content ||
		    p_hdr10_param->max_frame_avg_light_level
			!= p_cll->max_pic_average)
			flag |= 4;
		if (flag & 4) {
			p_hdr10_param->max_content_light_level =
				p_cll->max_content;
			p_hdr10_param->max_frame_avg_light_level =
				p_cll->max_pic_average;
		}
	} else {
		if (p_hdr10_param->max_content_light_level != 0 ||
		    p_hdr10_param->max_frame_avg_light_level != 0) {
			p_hdr10_param->max_content_light_level = 0;
			p_hdr10_param->max_frame_avg_light_level = 0;
			flag |= 8;
		}
	}

	if (flag && (debug_dolby & 1)) {
		pr_dolby_dbg
			("HDR10: primaries %d, maxcontent %d, flag %d, type %d\n",
			 p_mdc->present_flag, p_cll->present_flag,
			 flag, primaries_type);
		pr_dolby_dbg
			("\tR = %04x, %04x\n",
			 p_hdr10_param->r_x, p_hdr10_param->r_y);
		pr_dolby_dbg
			("\tG = %04x, %04x\n",
			 p_hdr10_param->g_x, p_hdr10_param->g_y);
		pr_dolby_dbg
			("\tB = %04x, %04x\n",
			 p_hdr10_param->b_x, p_hdr10_param->b_y);
		pr_dolby_dbg
			("\tW = %04x, %04x\n",
			 p_hdr10_param->w_x, p_hdr10_param->w_y);
		pr_dolby_dbg
			("\tMax = %d\n",
			 p_hdr10_param->max_display_mastering_lum);
		pr_dolby_dbg
			("\tMin = %d\n",
			 p_hdr10_param->min_display_mastering_lum);
		pr_dolby_dbg
			("\tMCLL = %d\n",
			 p_hdr10_param->max_content_light_level);
		pr_dolby_dbg
			("\tMPALL = %d\n\n",
			 p_hdr10_param->max_frame_avg_light_level);
	}
}

static int prepare_vsif_pkt
	(struct dv_vsif_para *vsif,
	struct dovi_setting_s *setting,
	const struct vinfo_s *vinfo,
	enum signal_format_enum src_format)
{
	if (!vsif || !vinfo || !setting ||
	    !vinfo->vout_device || !vinfo->vout_device->dv_info)
		return -1;
	vsif->vers.ver2.low_latency =
		setting->dovi_ll_enable;
	if (src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL)
		vsif->vers.ver2.dobly_vision_signal = 1;/*0b0001*/
	else if (src_format == FORMAT_HDR10)
		vsif->vers.ver2.dobly_vision_signal = 3;/*0b0011*/
	else if (src_format == FORMAT_HLG)
		vsif->vers.ver2.dobly_vision_signal = 7;/*0b0111*/
	else if (src_format == FORMAT_SDR || src_format == FORMAT_SDR_2020)
		vsif->vers.ver2.dobly_vision_signal = 5;/*0b0101*/
	if ((debug_dolby & 2))
		pr_dolby_dbg("src %d, dobly_vision_signal %d\n",
			     src_format, vsif->vers.ver2.dobly_vision_signal);

	if (vinfo->vout_device->dv_info &&
	    vinfo->vout_device->dv_info->sup_backlight_control &&
	    (setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
		vsif->vers.ver2.backlt_ctrl_MD_present = 1;
		vsif->vers.ver2.eff_tmax_PQ_hi =
			setting->ext_md.level_2.target_max_pq_h & 0xf;
		vsif->vers.ver2.eff_tmax_PQ_low =
			setting->ext_md.level_2.target_max_pq_l;
	} else {
		vsif->vers.ver2.backlt_ctrl_MD_present = 0;
		vsif->vers.ver2.eff_tmax_PQ_hi = 0;
		vsif->vers.ver2.eff_tmax_PQ_low = 0;
	}

	if (setting->dovi_ll_enable &&
	    (setting->ext_md.avail_level_mask
		& EXT_MD_LEVEL_255)) {
		vsif->vers.ver2.auxiliary_MD_present = 1;
		vsif->vers.ver2.auxiliary_runmode =
			setting->ext_md.level_255.run_mode;
		vsif->vers.ver2.auxiliary_runversion =
			setting->ext_md.level_255.run_version;
		vsif->vers.ver2.auxiliary_debug0 =
			setting->ext_md.level_255.dm_debug_0;
	} else {
		vsif->vers.ver2.auxiliary_MD_present = 0;
		vsif->vers.ver2.auxiliary_runmode = 0;
		vsif->vers.ver2.auxiliary_runversion = 0;
		vsif->vers.ver2.auxiliary_debug0 = 0;
	}
	return 0;
}

unsigned char vsif_emp[32];
static int prepare_emp_vsif_pkt
	(unsigned char *vsif_PB,
	struct dovi_setting_s *setting,
	const struct vinfo_s *vinfo)
{
	if (!vinfo || !setting ||
		!vinfo->vout_device || !vinfo->vout_device->dv_info)
		return -1;
	memset(vsif_PB, 0, 32);
	/* low_latency */
	if (setting->dovi_ll_enable)
		vsif_PB[0] = 1 << 0;
	/* dovi_signal_type */
	vsif_PB[0] |= 1 << 1;
	/* source_dm_version */
	vsif_PB[0] |= 3 << 5;

	if (vinfo->vout_device->dv_info->sup_backlight_control &&
		(setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
		vsif_PB[1] |= 1 << 7;
		vsif_PB[1] |=
			setting->ext_md.level_2.target_max_pq_h & 0xf;
		vsif_PB[2] =
			setting->ext_md.level_2.target_max_pq_l;
	}

	if (setting->dovi_ll_enable && (setting->ext_md.avail_level_mask
		& EXT_MD_LEVEL_255)) {
		vsif_PB[1] |= 1 << 6;
		vsif_PB[3] = setting->ext_md.level_255.run_mode;
		vsif_PB[4] = setting->ext_md.level_255.run_version;
		vsif_PB[5] = setting->ext_md.level_255.dm_debug_0;
	}
	return 0;
}
static int notify_vd_signal_to_amvideo(struct vd_signal_info_s *vd_signal)
{
	static int pre_signal = -1;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	if (pre_signal != vd_signal->signal_type) {
		vd_signal->vd1_signal_type =
			SIGNAL_DOVI;
		vd_signal->vd2_signal_type =
			SIGNAL_DOVI;
		amvideo_notifier_call_chain
			(AMVIDEO_UPDATE_SIGNAL_MODE,
			(void *)vd_signal);
	}
#endif
	pre_signal = vd_signal->signal_type;
	return 0;
}

/* #define HDMI_SEND_ALL_PKT */
static u32 last_dst_format = FORMAT_SDR;
static bool send_hdmi_pkt
	(enum signal_format_enum src_format,
	 enum signal_format_enum dst_format,
	 const struct vinfo_s *vinfo, struct vframe_s *vf)
{
	struct hdr10_infoframe *p_hdr;
	int i;
	bool flag = false;
	static int sdr_transition_delay;
	struct vd_signal_info_s vd_signal;

	if ((debug_dolby & 2))
		pr_dolby_dbg("[%s]src_format %d, dst %d, last %d:\n",
			     __func__, src_format, dst_format, last_dst_format);

	if (dst_format == FORMAT_HDR10) {
		sdr_transition_delay = 0;
		p_hdr = &dovi_setting.hdr_info;
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (9 << 16)	/* bt2020 */
			| (0x10 << 8)	/* bt2020-10 */
			| (10 << 0);/* bt2020c */
		if (src_format != FORMAT_HDR10PLUS) {
			/* keep as r,g,b when src not HDR+ */
			if (hdr10_data.primaries[0][0] !=
				((p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb))
				flag = true;
			hdr10_data.primaries[0][0] =
				(p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb;

			if (hdr10_data.primaries[0][1] !=
				((p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb))
				flag = true;
			hdr10_data.primaries[0][1] =
				(p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb;

			if (hdr10_data.primaries[1][0] !=
				((p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb))
				flag = true;
			hdr10_data.primaries[1][0] =
				(p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb;

			if (hdr10_data.primaries[1][1] !=
				((p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb))
				flag = true;
			hdr10_data.primaries[1][1] =
				(p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb;
			if (hdr10_data.primaries[2][0] !=
				((p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb))
				flag = true;
			hdr10_data.primaries[2][0] =
				(p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb;

			if (hdr10_data.primaries[2][1] !=
				((p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb))
				flag = true;
			hdr10_data.primaries[2][1] =
				(p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb;
		} else {
			/* need invert to g,b,r */
			if (hdr10_data.primaries[0][0] !=
				((p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb))
				flag = true;
			hdr10_data.primaries[0][0] =
				(p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb;

			if (hdr10_data.primaries[0][1] !=
				((p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb))
				flag = true;
			hdr10_data.primaries[0][1] =
				(p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb;

			if (hdr10_data.primaries[1][0] !=
				((p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb))
				flag = true;
			hdr10_data.primaries[1][0] =
				(p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb;

			if (hdr10_data.primaries[1][1] !=
				((p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb))
				flag = true;
			hdr10_data.primaries[1][1] =
				(p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb;
			if (hdr10_data.primaries[2][0] !=
				((p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb))
				flag = true;
			hdr10_data.primaries[2][0] =
				(p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb;

			if (hdr10_data.primaries[2][1] !=
				((p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb))
				flag = true;
			hdr10_data.primaries[2][1] =
				(p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb;
		}

		if (hdr10_data.white_point[0] !=
			((p_hdr->white_point_x_msb << 8)
			| p_hdr->white_point_x_lsb))
			flag = true;
		hdr10_data.white_point[0] =
			(p_hdr->white_point_x_msb << 8)
			| p_hdr->white_point_x_lsb;

		if (hdr10_data.white_point[1] !=
			((p_hdr->white_point_y_msb << 8)
			| p_hdr->white_point_y_lsb))
			flag = true;
		hdr10_data.white_point[1] =
			(p_hdr->white_point_y_msb << 8)
			| p_hdr->white_point_y_lsb;

		if (hdr10_data.luminance[0] !=
			((p_hdr->max_display_mastering_lum_msb << 8)
			| p_hdr->max_display_mastering_lum_lsb))
			flag = true;
		hdr10_data.luminance[0] =
			(p_hdr->max_display_mastering_lum_msb << 8)
			| p_hdr->max_display_mastering_lum_lsb;

		if (hdr10_data.luminance[1] !=
			((p_hdr->min_display_mastering_lum_msb << 8)
			| p_hdr->min_display_mastering_lum_lsb))
			flag = true;
		hdr10_data.luminance[1] =
			(p_hdr->min_display_mastering_lum_msb << 8)
			| p_hdr->min_display_mastering_lum_lsb;

		if (hdr10_data.max_content !=
			((p_hdr->max_content_light_level_msb << 8)
			| p_hdr->max_content_light_level_lsb))
			flag = true;
		hdr10_data.max_content =
			(p_hdr->max_content_light_level_msb << 8)
			| p_hdr->max_content_light_level_lsb;

		if (hdr10_data.max_frame_average !=
			((p_hdr->max_frame_avg_light_level_msb << 8)
			| p_hdr->max_frame_avg_light_level_lsb))
			flag = true;
		hdr10_data.max_frame_average =
			(p_hdr->max_frame_avg_light_level_msb << 8)
			| p_hdr->max_frame_avg_light_level_lsb;

		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_hdr_pkt)
			vinfo->vout_device->fresh_tx_hdr_pkt(&hdr10_data);
#ifdef HDMI_SEND_ALL_PKT
		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {
#ifdef V2_4_3
			send_emp(EOTF_T_NULL,
				YUV422_BIT12,
				(struct dv_vsif_para *)NULL,
				(unsigned char *)NULL,
				0,
				true);
#else
			vinfo->vout_device->fresh_tx_vsif_pkt
				(0, 0, NULL, true);
#endif
		}
#endif
		if (last_dst_format != FORMAT_HDR10 ||
		    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT))
			pr_dolby_dbg("send hdmi pkt: HDR10\n");

		last_dst_format = dst_format;
		vd_signal.signal_type = SIGNAL_HDR10;
		notify_vd_signal_to_amvideo(&vd_signal);
		if (debug_dolby & 8) {
			pr_dolby_dbg("Info frame for hdr10 changed:\n");
			for (i = 0; i < 3; i++)
				pr_dolby_dbg("\tprimaries[%1d] = %04x, %04x\n",
					     i,
					     hdr10_data.primaries[i][0],
					     hdr10_data.primaries[i][1]);
			pr_dolby_dbg("\twhite_point = %04x, %04x\n",
				hdr10_data.white_point[0],
				hdr10_data.white_point[1]);
			pr_dolby_dbg("\tMax = %d\n",
				hdr10_data.luminance[0]);
			pr_dolby_dbg("\tMin = %d\n",
				hdr10_data.luminance[1]);
			pr_dolby_dbg("\tMCLL = %d\n",
				hdr10_data.max_content);
			pr_dolby_dbg("\tMPALL = %d\n\n",
				hdr10_data.max_frame_average);
		}
	} else if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;

		sdr_transition_delay = 0;
		memset(&vsif, 0, sizeof(vsif));
		if (vinfo) {
			prepare_vsif_pkt
				(&vsif, &dovi_setting, vinfo, src_format);
			prepare_emp_vsif_pkt
				(vsif_emp, &dovi_setting, vinfo);
		}
#ifdef HDMI_SEND_ALL_PKT
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		for (i = 0; i < 3; i++) {
			hdr10_data.primaries[i][0] = 0;
			hdr10_data.primaries[i][1] = 0;
		}
		hdr10_data.white_point[0] = 0;
		hdr10_data.white_point[1] = 0;
		hdr10_data.luminance[0] = 0;
		hdr10_data.luminance[1] = 0;
		hdr10_data.max_content = 0;
		hdr10_data.max_frame_average = 0;
		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_hdr_pkt)
			vinfo->vout_device->fresh_tx_hdr_pkt(&hdr10_data);
#endif
		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_vsif_pkt) {
			if (dovi_setting.dovi_ll_enable) {
#ifdef V2_4_3
				send_emp(EOTF_T_LL_MODE,
					dovi_setting.diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif, vsif_emp, 15, false);
#else
				vinfo->vout_device->fresh_tx_vsif_pkt
					(EOTF_T_LL_MODE,
					dovi_setting.diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif, false);
#endif
			} else {
#ifdef V2_4_3
				send_emp(EOTF_T_DOLBYVISION,
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL
				? RGB_8BIT : YUV422_BIT12,
				NULL,
				hdmi_metadata,
				hdmi_metadata_size,
				false);
#else
				vinfo->vout_device->fresh_tx_vsif_pkt
				(EOTF_T_DOLBYVISION,
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL
				? RGB_8BIT : YUV422_BIT12, &vsif,
				false);
#endif
			}
		}
		if (last_dst_format != FORMAT_DOVI ||
		    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT))
			pr_dolby_dbg("send hdmi pkt: %s\n",
				     dovi_setting.dovi_ll_enable ? "LL" : "DV");
		last_dst_format = dst_format;
		vd_signal.signal_type = SIGNAL_DOVI;
		notify_vd_signal_to_amvideo(&vd_signal);
	} else if (last_dst_format != dst_format) {
		if (last_dst_format == FORMAT_HDR10) {
			sdr_transition_delay = 0;
			if (!(vf && (is_hlg_frame(vf) ||
				is_hdr10plus_frame(vf) ||
				is_primesl_frame(vf)))) {
				/* TODO: double check if need add prime sl case */
				hdr10_data.features =
					  (1 << 29)	/* video available */
					| (5 << 26)	/* unspecified */
					| (0 << 25)	/* limit */
					| (1 << 24)	/* color available */
					| (1 << 16)	/* bt709 */
					| (1 << 8)	/* bt709 */
					| (1 << 0);	/* bt709 */
				for (i = 0; i < 3; i++) {
					hdr10_data.primaries[i][0] = 0;
					hdr10_data.primaries[i][1] = 0;
				}
				hdr10_data.white_point[0] = 0;
				hdr10_data.white_point[1] = 0;
				hdr10_data.luminance[0] = 0;
				hdr10_data.luminance[1] = 0;
				hdr10_data.max_content = 0;
				hdr10_data.max_frame_average = 0;
				if (vinfo && vinfo->vout_device &&
				    vinfo->vout_device->fresh_tx_hdr_pkt) {
					vinfo->vout_device->fresh_tx_hdr_pkt
					(&hdr10_data);
					last_dst_format = dst_format;
				}
			}
		} else if (last_dst_format == FORMAT_DOVI) {
			if (vinfo && vinfo->vout_device &&
			    vinfo->vout_device->fresh_tx_vsif_pkt) {
				if (vf && (is_hlg_frame(vf) ||
					   is_hdr10plus_frame(vf) ||
					   is_primesl_frame(vf))) {
					/* TODO: double check if need add prime sl case */
					/* HLG/HDR10+/PRIMESL case: first switch to SDR
					 * immediately.
					 */
					pr_dolby_dbg
				("send pkt: HDR10+/HLG/PRIMESL: signal SDR first\n");
#ifdef V2_4_3
					send_emp(EOTF_T_NULL,
					YUV422_BIT12,
					(struct dv_vsif_para *)NULL,
					(unsigned char *)NULL,
					0,
					true);
#else
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, true);
#endif
					last_dst_format = dst_format;
					sdr_transition_delay = 0;
				} else if (sdr_transition_delay >=
					   MAX_TRANSITION_DELAY) {
					pr_dolby_dbg
				("send pkt: VSIF disabled, signal SDR\n");
#ifdef V2_4_3
					send_emp(EOTF_T_NULL,
					YUV422_BIT12,
					(struct dv_vsif_para *)NULL,
					(unsigned char *)NULL,
					0,
					true);
#else
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, true);
#endif
					last_dst_format = dst_format;
					sdr_transition_delay = 0;
				} else {
					if (sdr_transition_delay == 0) {
						pr_dolby_dbg("send pkt: disable Dovi/H14b VSIF\n");
#ifdef V2_4_3
					send_emp(EOTF_T_NULL,
					YUV422_BIT12,
					(struct dv_vsif_para *)NULL,
					(unsigned char *)NULL,
					0,
					false);
#else
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, false);
#endif
					}
					sdr_transition_delay++;
				}
			}
		}
		vd_signal.signal_type = SIGNAL_SDR;
		notify_vd_signal_to_amvideo(&vd_signal);
	}
	if (dolby_vision_flags & FLAG_FORCE_HDMI_PKT)
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;
	return flag;
}

static void send_hdmi_pkt_ahead
	(enum signal_format_enum dst_format,
	const struct vinfo_s *vinfo)
{
	bool dovi_ll_enable = false;
	bool diagnostic_enable = false;

	if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
	    dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422 ||
	    dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444) {
		dovi_ll_enable = true;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444)
			diagnostic_enable = true;
	}

	if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;

		memset(&vsif, 0, sizeof(vsif));
		vsif.vers.ver2.low_latency = dovi_ll_enable;
		vsif.vers.ver2.dobly_vision_signal = 1;
		vsif.vers.ver2.backlt_ctrl_MD_present = 0;
		vsif.vers.ver2.eff_tmax_PQ_hi = 0;
		vsif.vers.ver2.eff_tmax_PQ_low = 0;
		vsif.vers.ver2.auxiliary_MD_present = 0;
		vsif.vers.ver2.auxiliary_runmode = 0;
		vsif.vers.ver2.auxiliary_runversion = 0;
		vsif.vers.ver2.auxiliary_debug0 = 0;

		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {
			if (dovi_ll_enable)
				vinfo->vout_device->fresh_tx_vsif_pkt
					(EOTF_T_DV_AHEAD,
					diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif, false);
			else
				vinfo->vout_device->fresh_tx_vsif_pkt
					(EOTF_T_DV_AHEAD,
					dolby_vision_target_mode ==
					dovi_ll_enable
					? YUV422_BIT12 : RGB_8BIT, &vsif,
					false);
		}
		pr_dolby_dbg("send_hdmi_pkt ahead: %s\n",
			     dovi_ll_enable ? "LL" : "DV");
	}
}

static u32 null_vf_cnt;
static bool video_off_handled;
static int is_video_output_off(struct vframe_s *vf)
{
	if ((READ_VPP_DV_REG(VPP_MISC) & (1 << 10)) == 0) {
		/*Not reset frame0/1 clipping*/
		/*when core off to avoid green garbage*/
		if (is_meson_tvmode() && !vf &&
		    dolby_vision_on_count <= dolby_vision_run_mode_delay)
			return 0;
		if (!vf)
			null_vf_cnt++;
		else
			null_vf_cnt = 0;
		if (null_vf_cnt > dolby_vision_wait_delay + 1) {
			null_vf_cnt = 0;
			return 1;
		}
		if (video_off_handled)
			return 1;
	} else {
		video_off_handled = 0;
	}
	return 0;
}

static void calculate_panel_max_pq
	(enum signal_format_enum src_format,
	 const struct vinfo_s *vinfo,
	 struct target_config *config)
{
	u32 max_lin = tv_max_lin;
	u16 max_pq = tv_max_pq;
	u32 panel_max = tv_max_lin;

	if (use_target_lum_from_cfg &&
	    !(src_format == FORMAT_HDR10 && force_hdr_tonemapping))
		return;
	if (dolby_vision_flags & FLAG_CERTIFICAION)
		return;
	if (panel_max_lumin)
		panel_max = panel_max_lumin;
	else if (vinfo->mode == VMODE_LCD)
		panel_max = vinfo->hdr_info.lumi_max;
	if (panel_max < 100)
		panel_max = 100;
	else if (panel_max > 4000)
		panel_max = 4000;
	if (src_format == FORMAT_HDR10 && force_hdr_tonemapping) {
		if (force_hdr_tonemapping >= hdr10_param.max_display_mastering_lum)
			max_lin = hdr10_param.max_display_mastering_lum;
		else
			max_lin = force_hdr_tonemapping;
	} else {
		max_lin = panel_max;
	}
	if (debug_dolby & 1)
		pr_dolby_dbg("max_lin %d\n", max_lin);

	if (max_lin != tv_max_lin) {
		if (max_lin < 500) {
			max_lin = max_lin - 100 + 10;
			max_lin = (max_lin / 20) * 20 + 100;
			max_pq = L2PQ_100_500[(max_lin - 100) / 20];
		} else {
			max_lin = max_lin - 500 + 50;
			max_lin = (max_lin / 100) * 100 + 500;
			max_pq = L2PQ_500_4000[(max_lin - 500) / 100];
		}
		if (tv_max_lin != max_lin || tv_max_pq != max_pq)
			pr_dolby_dbg("panel max lumin changed from %d(%d) to %d(%d)\n",
				     tv_max_lin, tv_max_pq, max_lin, max_pq);
		tv_max_lin = max_lin;
		tv_max_pq = max_pq;
		config->max_lin = tv_max_lin << 18;
		config->max_lin_dm3 = config->max_lin;
		config->max_pq = tv_max_pq;
		config->max_pq_dm3 = config->max_pq;
	}
}

bool is_dv_standard_es(int dvel, int mflag, int width)
{
	if (dolby_vision_profile == 4 &&
	    dvel == 1 && mflag == 0 &&
	    width >= 3840)
		return false;
	else
		return true;
}

static int prepare_dv_meta
	(struct md_reg_ipcore3 *out,
	unsigned char *p_md, int size)
{
	int i, shift;
	u32 value;
	unsigned char *p;
	u32 *p_out;

	/* calculate md size in double word */
	out->size = 1 + (size - 1 + 3) / 4;

	/* write metadata into register structure*/
	p = p_md;
	p_out = out->raw_metadata;
	*p_out++ = (size << 8) | p[0];
	shift = 0; value = 0;
	for (i = 1; i < size; i++) {
		value = value | (p[i] << shift);
		shift += 8;
		if (shift == 32) {
			*p_out++ = value;
			shift = 0; value = 0;
		}
	}
	if (shift != 0)
		*p_out++ = value;

	return out->size;
}

#define VSEM_BUF_SIZE 0x1000
#define VSIF_PAYLOAD_LEN   24
#define VSEM_PKT_BODY_SIZE   28
#define VSEM_FIRST_PKT_EDR_DATA_SIZE   15

struct vsem_data_pkt {
	unsigned char pkt_type;
	unsigned char hb1;
	unsigned char seq_index;
	unsigned char pb[VSEM_PKT_BODY_SIZE];
};

#define HEADER_MASK_FIRST        0x80
#define HEADER_MASK_LAST         0x40

unsigned short get_data_len(struct vsem_data_pkt *pkt)
{
	return (pkt->pb[5] << 8) + pkt->pb[6];
}

unsigned char get_vsem_byte(u8 md_byte, u8 mask)
{
	u8 field_lsb_off = 0;
	u8 field_mask = mask;

	while (!(field_mask & 1)) {
		field_mask >>= 1;
		field_lsb_off++;
	}

	return ((md_byte & mask) >> field_lsb_off);
}

static u32 crc32_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static u32 get_crc32(const void *data, size_t data_size)
{
	const u8 *p = (u8 *)data;
	u32 crc = 0xFFFFFFFF;

	while (data_size) {
		crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *p) & 0xff];
		p++;
		data_size--;
	}

	return crc;
}

int vsem_check(unsigned char *control_data, unsigned char *vsem_payload)
{
	struct vsem_data_pkt *cur_pkt;
	unsigned char *p_buf = NULL;
	u16 cur_len = 0;
	int rv = 0;
	u32 crc;
	u32 data_length = 0;
	bool last = false;

	cur_pkt = (struct vsem_data_pkt *)(control_data);
	p_buf = vsem_payload;

	if (!get_vsem_byte(cur_pkt->hb1, HEADER_MASK_FIRST)) {
		rv = -1;
		return rv;
	}
	data_length = get_data_len(cur_pkt);
	memcpy(p_buf, &cur_pkt->pb[0], VSEM_PKT_BODY_SIZE);
	cur_len = VSEM_PKT_BODY_SIZE - 7;
	p_buf += VSEM_PKT_BODY_SIZE;
	while (!last) {
		cur_pkt += 1;
		if (get_vsem_byte(cur_pkt->hb1, HEADER_MASK_LAST)) {
			memcpy(p_buf, &cur_pkt->pb[0],
				data_length - cur_len);
			last = true;
		} else {
			memcpy(p_buf, &cur_pkt->pb[0],
				VSEM_PKT_BODY_SIZE);
			cur_len += VSEM_PKT_BODY_SIZE;
			p_buf += VSEM_PKT_BODY_SIZE;
		}
		if (cur_len >= data_length) {
			pr_info("didn't find last pkt! %d >= %d\n",
				cur_len, data_length);
			break;
		}
		if (cur_len >= VSEM_BUF_SIZE - VSEM_PKT_BODY_SIZE) {
			pr_info("vsem payload buffer is too small!\n");
			break;
		}
	}

	/* crc check */
	crc = get_crc32(vsem_payload + 13, data_length - 6);
	if (crc != 0) {
		pr_info("Error: crc error while reading control data\n");
		rv = -1;
		return rv;
	}

	return rv;
}

/*for hdmi cert: not run cp when vf not change*/
/*sometimes  even if frame is same but  frame crc changed, maybe ucd or rx not stable,*/
/*so need to check  few more frames*/
bool check_vf_changed(struct vframe_s *vf)
{
#define MAX_VF_CRC_CHECK_CUONT 4

	static u32 new_vf_crc;
	static u32 vf_crc_repeat_cnt;
	bool changed = false;

	if (!vf)
		return true;

	if (debug_dolby & 1)
		pr_dolby_dbg("vf %p, crc %x, last valid crc %x, rpt %d\n",
				vf, vf->crc, last_vf_valid_crc,
				vf_crc_repeat_cnt);

	if (vf->crc == 0) {
		/*invalid crc, maybe vdin dropped last frame*/
		return changed;
	}
	if (last_vf_valid_crc == 0) {
		last_vf_valid_crc = vf->crc;
		changed = true;
		hdmi_frame_count = 0;
		pr_dolby_dbg("first frame\n");
	} else if (vf->crc != last_vf_valid_crc) {
		if (vf->crc != new_vf_crc)
			vf_crc_repeat_cnt = 0;
		else
			++vf_crc_repeat_cnt;

		if (vf_crc_repeat_cnt >= MAX_VF_CRC_CHECK_CUONT) {
			vf_crc_repeat_cnt = 0;
			last_vf_valid_crc = vf->crc;
			changed = true;
			++hdmi_frame_count;
			pr_dolby_dbg("new frame\n");
		}
	} else {
		vf_crc_repeat_cnt = 0;
	}
	new_vf_crc = vf->crc;
	return changed;
}

static u32 last_total_md_size;
static u32 last_total_comp_size;
/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
int dolby_vision_parse_metadata(struct vframe_s *vf,
				u8 toggle_mode,
				bool bypass_release,
				bool drop_flag)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct vframe_s *el_vf;
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	int flag;
	enum signal_format_enum src_format = FORMAT_SDR;
	enum signal_format_enum check_format;
	enum signal_format_enum dst_format;
	enum signal_format_enum cur_src_format;
	enum signal_format_enum cur_dst_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	bool el_flag = 0;
	bool el_halfsize_flag = 1;
	u32 w = 0xffff;
	u32 h = 0xffff;
	int meta_flag_bl = 1;
	int meta_flag_el = 1;
	int src_chroma_format = 0;
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	u32 target_lumin_max = 0;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	enum priority_mode_enum pri_mode = V_PRIORITY;
	u32 graphic_min = 50; /* 0.0001 */
	u32 graphic_max = 100; /* 1 */
	int ret_flags = 0;
	static int bypass_frame = -1;
	static int last_current_format;
	int ret = -1;
	bool mel_flag = false;
	int vsem_size = 0;
	int vsem_if_size = 0;
	bool dump_emp = false;
	bool dv_vsem = false;
	bool hdr10_src_primary_changed = false;
	unsigned long time_use = 0;
	struct timeval start;
	struct timeval end;
	int dump_size = 100;
	bool run_control_path = true;
	bool vf_changed = true;
	char *dvel_provider = NULL;
	struct ambient_cfg_s *p_ambient = NULL;

	memset(&req, 0, (sizeof(struct provider_aux_req_s)));
	memset(&el_req, 0, (sizeof(struct provider_aux_req_s)));

	if (!dolby_vision_enable || !module_installed)
		return -1;

	if (vf) {
		video_frame = true;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	if (is_meson_tvmode() && vf &&
	    vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;
		vf_notify_provider_by_name("dv_vdin",
					   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					   (void *)&req);
		input_mode = IN_MODE_HDMI;

		if ((dolby_vision_flags & FLAG_CERTIFICAION) && enable_vf_check)
			vf_changed = check_vf_changed(vf);

		/* meta */
		if ((dolby_vision_flags & FLAG_RX_EMP_VSEM) &&
			vf->emp.size > 0) {
			vsem_size = vf->emp.size * VSEM_PKT_SIZE;
			memcpy(vsem_if_buf, vf->emp.addr, vsem_size);
			if (vsem_if_buf[0] == 0x7f &&
			    vsem_if_buf[10] == 0x46 &&
			    vsem_if_buf[11] == 0xd0) {
				dv_vsem = true;
				if (!vsem_check(vsem_if_buf, vsem_md_buf)) {
					vsem_if_size = vsem_size;
					if (!vsem_md_buf[10]) {
						req.aux_buf =
							&vsem_md_buf[13];
						req.aux_size =
							(vsem_md_buf[5] << 8)
							+ vsem_md_buf[6]
							- 6 - 4;
						/* cancel vsem, use md */
						/* vsem_if_size = 0; */
					} else {
						req.low_latency = 1;
					}
				} else {
					/* emp error, use previous md */
					pr_dolby_dbg("EMP packet error %d\n",
						vf->emp.size);
					dump_emp = true;
					vsem_if_size = 0;
					req.aux_buf = NULL;
					req.aux_size =
						last_total_md_size;
				}
			} else if (debug_dolby & 4) {
				pr_dolby_dbg("EMP packet not DV vsem %d\n",
					vf->emp.size);
				dump_emp = true;
			}
			if ((debug_dolby & 4) || dump_emp) {
				pr_info("vsem pkt count = %d\n", vf->emp.size);
				for (i = 0; i < vsem_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}

		/* w/t vsif and no dv_vsem */
		if (vf->vsif.size && !dv_vsem) {
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			memcpy(vsem_if_buf, vf->vsif.addr, vf->vsif.size);
			vsem_if_size = vf->vsif.size;
			if (debug_dolby & 4) {
				pr_info("vsif size = %d\n", vf->vsif.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}

		if (debug_dolby & 1) {
			if (dv_vsem || vsem_if_size)
				pr_dolby_dbg("vdin get %s:%d, md:%p %d,ll:%d,bit %x\n",
					dv_vsem ? "vsem" : "vsif",
					dv_vsem ? vsem_size : vsem_if_size,
					req.aux_buf, req.aux_size,
					req.low_latency,
					vf->bitdepth);
			else
				pr_dolby_dbg("vdin get md %p %d,ll:%d,bit %x\n",
					req.aux_buf, req.aux_size,
					req.low_latency, vf->bitdepth);
		}

		/*check vsem_if_buf */
		if (vsem_if_size &&
			vsem_if_buf[0] != 0x81) {
			/*not vsif, continue to check vsem*/
			if (!(vsem_if_buf[0] == 0x7F &&
				vsem_if_buf[1] == 0x80 &&
				vsem_if_buf[10] == 0x46 &&
				vsem_if_buf[11] == 0xd0 &&
				vsem_if_buf[12] == 0x00)) {
				vsem_if_size = 0;
				pr_dolby_dbg("vsem_if_buf is invalid!\n");
				pr_dolby_dbg("%x %x %x %x %x %x %x %x %x %x %x %x\n",
					     vsem_if_buf[0],
					     vsem_if_buf[1],
					     vsem_if_buf[2],
					     vsem_if_buf[3],
					     vsem_if_buf[4],
					     vsem_if_buf[5],
					     vsem_if_buf[6],
					     vsem_if_buf[7],
					     vsem_if_buf[8],
					     vsem_if_buf[9],
					     vsem_if_buf[10],
					     vsem_if_buf[11]);
			}
		}
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
		    req.low_latency == 1) {
			src_format = FORMAT_DOVI_LL;
			src_chroma_format = 0;
			for (i = 0; i < 2; i++) {
				if (md_buf[i])
					memset(md_buf[i], 0, MD_BUF_SIZE);
				if (comp_buf[i])
					memset(comp_buf[i], 0, COMP_BUF_SIZE);
			}
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_size) {
			if (req.aux_buf) {
				current_id = current_id ^ 1;
				memcpy(md_buf[current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
		} else {
			if (toggle_mode == 2)
				src_format =  tv_dovi_setting->src_format;
			if (vf->type & VIDTYPE_VIU_422)
				src_chroma_format = 1;
			p_mdc =	&vf->prop.master_display_colour;
			if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
				src_format = FORMAT_HDR10;
				/* prepare parameter from hdmi for hdr10 */
				p_mdc->luminance[0] *= 10000;
				prepare_hdr10_param
					(p_mdc, &hdr10_param);
				req.dv_enhance_exist = 0;
				src_bdp = 12;
			}
			if (is_meson_tm2_tvmode() || is_meson_t7_tvmode() ||
			    is_meson_t3_tvmode() ||
			    is_meson_t5w()) {
				if (src_format != FORMAT_DOVI &&
					(is_hlg_frame(vf) || force_hdmin_fmt == 2)) {
					src_format = FORMAT_HLG;
					src_bdp = 12;
				}
				if (src_format == FORMAT_SDR &&
					!req.dv_enhance_exist)
					src_bdp = 12;
			}
		}
		if ((debug_dolby & 4) && req.aux_size) {
			pr_dolby_dbg("metadata(%d):\n", req.aux_size);
			for (i = 0; i < req.aux_size + 8; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[current_id][i],
					md_buf[current_id][i + 1],
					md_buf[current_id][i + 2],
					md_buf[current_id][i + 3],
					md_buf[current_id][i + 4],
					md_buf[current_id][i + 5],
					md_buf[current_id][i + 6],
					md_buf[current_id][i + 7]);
		}

		total_md_size = req.aux_size;
		total_comp_size = 0;
		meta_flag_bl = 0;
		if (req.aux_buf && req.aux_size) {
			last_total_md_size = total_md_size;
			last_total_comp_size = total_comp_size;
		} else if (toggle_mode == 2) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;
		}
		if (debug_dolby & 1)
			pr_dolby_dbg("frame %d, %p, pts %lld, format: %s\n",
			frame_count, vf, vf->pts_us64,
			(src_format == FORMAT_HDR10) ? "HDR10" :
			((src_format == FORMAT_DOVI) ? "DOVI" :
			((src_format == FORMAT_DOVI_LL) ? "DOVI_LL" :
			((src_format == FORMAT_HLG) ? "HLG" : "SDR"))));

		if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dolby_dbg
					("+++ get bl(%p-%lld) +++\n",
					 vf, vf->pts_us64);
			dolby_vision_vf_add(vf, NULL);
		}
	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		enum vframe_signal_fmt_e fmt;

		input_mode = IN_MODE_OTT;
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;

		/* check source format */
		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
		    fmt == VFRAME_SIGNAL_FMT_INVALID) &&
		    !vf->discard_dv_data) {
			vf_notify_provider_by_name
				(dv_provider,
				 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				  (void *)&req);
		}
		/* use callback aux date first, if invalid, use sei_ptr */
		if ((!req.aux_buf || !req.aux_size) &&
		    fmt == VFRAME_SIGNAL_FMT_DOVI) {
			u32 sei_size = 0;
			char *sei;

			if (debug_dolby & 1)
				pr_dolby_dbg("no aux %p %x, el %d from %s, use sei_ptr\n",
					     req.aux_buf,
					     req.aux_size,
					     req.dv_enhance_exist,
					     dv_provider);

			sei = (char *)get_sei_from_src_fmt
				(vf, &sei_size);
			if (sei && sei_size) {
				req.aux_buf = sei;
				req.aux_size = sei_size;
				req.dv_enhance_exist =
					vf->src_fmt.dual_layer;
			}

		}
		if (debug_dolby & 1)
			pr_dolby_dbg("%s get vf %p(%d,index %d),fmt %d,aux %p %x,el %d\n",
				     dv_provider, vf, vf->discard_dv_data, vf->omx_index, fmt,
				     req.aux_buf,
				     req.aux_size,
				     req.dv_enhance_exist);
		/* parse meta in base layer */
		if (toggle_mode != 2) {
			ret = get_md_from_src_fmt(vf);
			if (ret == 1) { /*parse succeeded*/
				meta_flag_bl = 0;
				src_format = FORMAT_DOVI;
				memcpy(md_buf[current_id],
				       vf->src_fmt.md_buf,
				       vf->src_fmt.md_size);
				memcpy(comp_buf[current_id],
				       vf->src_fmt.comp_buf,
				       vf->src_fmt.comp_size);
				total_md_size =  vf->src_fmt.md_size;
				total_comp_size =  vf->src_fmt.comp_size;
				ret_flags = vf->src_fmt.parse_ret_flags;
				if ((debug_dolby & 4) && dump_enable) {
					pr_dolby_dbg("get md_buf %p, size(%d):\n",
						vf->src_fmt.md_buf, vf->src_fmt.md_size);
					for (i = 0; i < total_md_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
							md_buf[current_id][i],
							md_buf[current_id][i + 1],
							md_buf[current_id][i + 2],
							md_buf[current_id][i + 3],
							md_buf[current_id][i + 4],
							md_buf[current_id][i + 5],
							md_buf[current_id][i + 6],
							md_buf[current_id][i + 7]);
					pr_dolby_dbg("comp(%d):\n", vf->src_fmt.comp_size);
					if (vf->src_fmt.comp_size < dump_size)
						dump_size = vf->src_fmt.comp_size;
					for (i = 0; i < dump_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
							comp_buf[current_id][i],
							comp_buf[current_id][i + 1],
							comp_buf[current_id][i + 2],
							comp_buf[current_id][i + 3],
							comp_buf[current_id][i + 4],
							comp_buf[current_id][i + 5],
							comp_buf[current_id][i + 6],
							comp_buf[current_id][i + 7]);
				}
			} else {  /*no parse or parse failed*/
				if (get_vframe_src_fmt(vf) ==
				    VFRAME_SIGNAL_FMT_HDR10PRIME)
					src_format = FORMAT_PRIMESL;
				else
					meta_flag_bl =
					parse_sei_and_meta
						(vf, &req,
						&total_comp_size,
						&total_md_size,
						&src_format,
						&ret_flags, drop_flag);
			}
			if (force_mel)
				ret_flags = 1;

			if (ret_flags && req.dv_enhance_exist) {
				if (!strcmp(dv_provider, "dvbldec"))
					vf_notify_provider_by_name
					(dv_provider,
					 VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL,
					 (void *)&req);
				pr_dolby_dbg("bypass mel\n");
			}
			if (ret_flags == 1)
				mel_flag = true;
			if (!is_dv_standard_es(req.dv_enhance_exist,
					       ret_flags, w)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
			}
			if ((is_meson_tm2_stbmode()  || is_meson_sc2() || is_meson_t7_stbmode() ||
				is_meson_s4d()) &&
			    (req.dv_enhance_exist && !mel_flag &&
			    ((dolby_vision_flags & FLAG_CERTIFICAION) == 0)) &&
			    !enable_fel) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
			}
			if (is_meson_tvmode() &&
				(req.dv_enhance_exist && !mel_flag)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
				if (debug_dolby & 1)
					pr_dolby_dbg("tv: bypass fel\n");
			}
		} else if (is_dolby_vision_stb_mode()) {
			src_format = dovi_setting.src_format;
		} else if (is_meson_tvmode()) {
			src_format = tv_dovi_setting->src_format;
		}

		if (src_format != FORMAT_DOVI && is_primesl_frame(vf)) {
			src_format = FORMAT_PRIMESL;
			src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &hdr10_param);
			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			req.dv_enhance_exist = 0;
		}

		if (src_format != FORMAT_DOVI && is_hlg_frame(vf)) {
			src_format = FORMAT_HLG;
			if (is_meson_tvmode())
				src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_hdr10plus_frame(vf))
			src_format = FORMAT_HDR10PLUS;

		if (src_format != FORMAT_DOVI && is_mvc_frame(vf))
			src_format = FORMAT_MVC;

		/* TODO: need 962e ? */
		if (src_format == FORMAT_SDR &&
		    is_dolby_vision_stb_mode() &&
		    !req.dv_enhance_exist)
			src_bdp = 10;

		if (src_format == FORMAT_SDR &&
			is_meson_tvmode() &&
			!req.dv_enhance_exist)
			src_bdp = 10;

		if (((debug_dolby & 1) || frame_count == 0) && toggle_mode == 1)
			pr_info
			("DV:[%d,%lld,%d,%s,%d,%d]\n",
			 frame_count, vf->pts_us64, src_bdp,
			 (src_format == FORMAT_HDR10) ? "HDR10" :
			 (src_format == FORMAT_DOVI ? "DOVI" :
			 (src_format == FORMAT_HLG ? "HLG" :
			 (src_format == FORMAT_HDR10PLUS ? "HDR10+" :
		     (src_format == FORMAT_PRIMESL ? "PRIMESL" :
			 (req.dv_enhance_exist ? "DOVI (el meta)" : "SDR"))))),
			 req.aux_size, req.dv_enhance_exist);
		if (src_format != FORMAT_DOVI && !req.dv_enhance_exist)
			memset(&req, 0, sizeof(req));

		/* check dvel decoder is active, if active, should */
		/* get/put el data, otherwise, dvbl is stuck */
		dvel_provider = vf_get_provider_name(DVEL_RECV_NAME);
		if (req.dv_enhance_exist && toggle_mode == 1 &&
		    dvel_provider && !strcmp(dvel_provider, "dveldec")) {
			el_vf = dvel_vf_get();
			if (el_vf && (el_vf->pts_us64 == vf->pts_us64 ||
				      !(dolby_vision_flags &
				      FLAG_CHECK_ES_PTS))) {
				if (debug_dolby & 2)
					pr_dolby_dbg("+++ get bl(%p-%lld) with el(%p-%lld) +++\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				if (meta_flag_bl) {
					int el_md_size = 0;
					int el_comp_size = 0;

					el_req.vf = el_vf;
					el_req.bot_flag = 0;
					el_req.aux_buf = NULL;
					el_req.aux_size = 0;
					if (!strcmp(dv_provider, "dvbldec"))
						vf_notify_provider_by_name
						("dveldec",
						 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						 (void *)&el_req);
					if (el_req.aux_buf &&
					    el_req.aux_size) {
						meta_flag_el =
							parse_sei_and_meta
							(el_vf, &el_req,
							 &el_comp_size,
							 &el_md_size,
							 &src_format,
							 &ret_flags, drop_flag);
					}
					if (!meta_flag_el) {
						total_comp_size =
							el_comp_size;
						total_md_size =
							el_md_size;
						src_bdp = 12;
					}
					/* force set format as DOVI*/
					/*	when meta data error */
					if (meta_flag_el &&
					    el_req.aux_buf &&
					    el_req.aux_size)
						src_format = FORMAT_DOVI;
					if (debug_dolby & 2)
						pr_dolby_dbg
					("el mode:src_fmt:%d,meta_flag_el:%d\n",
						 src_format,
						 meta_flag_el);
					if (meta_flag_el && frame_count == 0)
						pr_info
					("el mode:parser err,aux %p,size:%d\n",
						 el_req.aux_buf,
						 el_req.aux_size);
				}
				dolby_vision_vf_add(vf, el_vf);
				el_flag = 1;
				if (vf->width == el_vf->width)
					el_halfsize_flag = 0;
			} else {
				if (!el_vf)
					pr_dolby_error
					("bl(%p-%lld) not found el\n",
					 vf, vf->pts_us64);
				else
					pr_dolby_error
					("bl(%p-%lld) not found el(%p-%lld)\n",
					 vf, vf->pts_us64,
					 el_vf, el_vf->pts_us64);
			}
		} else if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dolby_dbg("+++ get bl(%p-%lld) +++\n",
					     vf, vf->pts_us64);
			dolby_vision_vf_add(vf, NULL);
		}

		if (req.dv_enhance_exist)
			el_flag = 1;

		if (toggle_mode != 2) {
			if (!drop_flag) {
				last_total_md_size = total_md_size;
				last_total_comp_size = total_comp_size;
			}
		} else if (meta_flag_bl && meta_flag_el) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;
			if (is_dolby_vision_stb_mode())
				el_flag = dovi_setting.el_flag;
			else
				el_flag = tv_dovi_setting->el_flag;
			mel_flag = mel_mode;
			if (debug_dolby & 2)
				pr_dolby_dbg("update el_flag %d, melFlag %d\n",
					     el_flag, mel_flag);
			meta_flag_bl = 0;
		}

		if (el_flag && !mel_flag &&
		    ((dolby_vision_flags & FLAG_CERTIFICAION) == 0) &&
		    !enable_fel) {
			el_flag = 0;
			dolby_vision_el_disable = true;
		}
		if (el_flag && !enable_mel)
			el_flag = 0;
		if (src_format != FORMAT_DOVI) {
			el_flag = 0;
			mel_flag = 0;
		}
		if (src_format == FORMAT_DOVI &&
		    meta_flag_bl && meta_flag_el) {
			/* dovi frame no meta or meta error */
			/* use old setting for this frame   */
			pr_dolby_dbg("no meta or meta err!\n");
			return -1;
		}
	} else if (vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		(is_meson_tm2_stbmode() || is_meson_t7_stbmode()) && hdmi_to_stb_policy) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;
		if (!strcmp(dv_provider, "dv_vdin"))
			vf_notify_provider_by_name(dv_provider,
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (req.low_latency == 1) {
			src_format = FORMAT_HDR10;
			if (!vf_is_hdr10(vf)) {
				vf->signal_type &= 0xff0000ff;
				vf->signal_type |= 0x00091000;
			}
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &hdr10_param);
			src_bdp = 10;
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_size) {
			if (req.aux_buf) {
				current_id = current_id ^ 1;
				memcpy(md_buf[current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
			input_mode = IN_MODE_HDMI;
			src_bdp = 12;
			meta_flag_bl = 0;
			el_flag = 0;
			mel_flag = 0;
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", req.aux_size);
				for (i = 0; i < req.aux_size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						md_buf[current_id][i],
						md_buf[current_id][i + 1],
						md_buf[current_id][i + 2],
						md_buf[current_id][i + 3],
						md_buf[current_id][i + 4],
						md_buf[current_id][i + 5],
						md_buf[current_id][i + 6],
						md_buf[current_id][i + 7]);
			}
		} else {
			if (toggle_mode == 2)
				src_format = dovi_setting.src_format;

			if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
				src_format = FORMAT_HDR10;
				p_mdc =	&vf->prop.master_display_colour;
				if (p_mdc->present_flag)
					p_mdc->luminance[0] *= 10000;
				prepare_hdr10_param(p_mdc, &hdr10_param);
			}

			if (is_hlg_frame(vf) || force_hdmin_fmt == 2)
				src_format = FORMAT_HLG;

			if (is_hdr10plus_frame(vf))
				src_format = FORMAT_HDR10PLUS;

			src_bdp = 10;
		}
		total_md_size = req.aux_size;
		total_comp_size = 0;
	}

	if (src_format == FORMAT_DOVI && meta_flag_bl && meta_flag_el) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		pr_dolby_dbg("no meta or meta err!\n");
		return -1;
	}

	/* if not DOVI, release metadata_parser */
	if (vf && src_format != FORMAT_DOVI &&
	    metadata_parser &&
	    !bypass_release) {
		if (p_funcs_stb)
			p_funcs_stb->metadata_parser_release();
		if (p_funcs_tv)
			p_funcs_tv->metadata_parser_release();
		metadata_parser = NULL;
		pr_dolby_dbg("parser release\n");
	}

	if (drop_flag) {
		pr_dolby_dbg("drop frame_count %d\n", frame_count);
		return 1;
	}

	check_format = src_format;
	if (vf) {
		update_src_format(check_format, vf);
		last_current_format = check_format;
	}

	if (dolby_vision_request_mode != 0xff) {
		dolby_vision_mode = dolby_vision_request_mode;
		dolby_vision_request_mode = 0xff;
	}
	current_mode = dolby_vision_mode;
	if (dolby_vision_policy_process
		(&current_mode, check_format)) {
		if (!dolby_vision_wait_init)
			dolby_vision_set_toggle_flag(1);
		pr_info("[%s]output change from %d to %d\n",
			__func__, dolby_vision_mode, current_mode);
		dolby_vision_target_mode = current_mode;
		dolby_vision_mode = current_mode;
		if (is_dolby_vision_stb_mode())
			new_dovi_setting.mode_changed = 1;
		pr_dolby_dbg("[%s]output change from %d to %d(%d, %p, %d)\n",
			     __func__, dolby_vision_mode, current_mode,
			     toggle_mode, vf, src_format);
	} else {
		dolby_vision_target_mode = dolby_vision_mode;
	}
	if (vf && (debug_dolby & 8))
		pr_dolby_dbg("parse_metadata: vf %p(index %d), mode %d\n",
			      vf, vf->omx_index, dolby_vision_mode);

	if (get_hdr_module_status(VD1_PATH, VPP_TOP0) != HDR_MODULE_ON &&
	    check_format == FORMAT_SDR) {
		/* insert 2 SDR frames before send DOVI */
		if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
		     dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT) &&
		    (last_current_format == FORMAT_HLG ||
		     last_current_format == FORMAT_PRIMESL ||
		     last_current_format == FORMAT_HDR10PLUS)) {
			/* TODO: if need add primesl */
			bypass_frame = 0;
			pr_dolby_dbg
			("[%s] source transition from %d to %d\n",
			 __func__, last_current_format, check_format);
		}
		last_current_format = check_format;
	}

	if (bypass_frame >= 0 && bypass_frame < MIN_TRANSITION_DELAY) {
		dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
		bypass_frame++;
	} else {
		bypass_frame = -1;
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		new_dovi_setting.mode_changed = 0;
		dolby_vision_wait_on = false;
		if (get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_BYPASS)
			return 1;
		return -1;
	}

	if (tv_mode) {
		if (!p_funcs_tv)
			return -1;
	} else {
		if (!p_funcs_stb)
			return -1;
	}

	/* TV core */
	if (is_meson_tvmode()) {
		if (src_format != tv_dovi_setting->src_format ||
			(dolby_vision_flags & FLAG_CERTIFICAION)) {
			pq_config_set_flag = false;
			best_pq_config_set_flag = false;
		}
		if (!pq_config_set_flag) {
			if (debug_dolby & 2)
				pr_dolby_dbg("update def_tgt_display_cfg\n");
			if (!load_bin_config) {
				memcpy(&(((struct pq_config *)pq_config_fake)->tdc),
				       &def_tgt_display_cfg_ll,
				       sizeof(def_tgt_display_cfg_ll));
			}
			pq_config_set_flag = true;
		}
		if (force_best_pq && !best_pq_config_set_flag) {
			pr_dolby_dbg("update best def_tgt_display_cfg\n");
			memcpy(&(((struct pq_config *)
				pq_config_fake)->tdc),
				&def_tgt_display_cfg_bestpq,
				sizeof(def_tgt_display_cfg_bestpq));
			best_pq_config_set_flag = true;

			if (p_funcs_tv && p_funcs_tv->tv_control_path)
				p_funcs_tv->tv_control_path
					(FORMAT_INVALID, 0,
					NULL, 0,
					NULL, 0,
					0, 0,
					SIGNAL_RANGE_SMPTE,
					NULL, NULL,
					0,
					NULL,
					NULL,
					NULL, 0,
					NULL,
					NULL);
		}
		calculate_panel_max_pq(src_format, vinfo,
				       &(((struct pq_config *)
				       pq_config_fake)->tdc));

		((struct pq_config *)
			pq_config_fake)->tdc.tuning_mode =
			dolby_vision_tuning_mode;
		if (dolby_vision_flags & FLAG_DISABLE_COMPOSER) {
			((struct pq_config *)pq_config_fake)
				->tdc.tuning_mode |=
				TUNING_MODE_EL_FORCE_DISABLE;
			el_halfsize_flag = 0;
		} else {
			((struct pq_config *)pq_config_fake)
				->tdc.tuning_mode &=
				(~TUNING_MODE_EL_FORCE_DISABLE);
		}
		if ((dolby_vision_flags & FLAG_CERTIFICAION) && sdr_ref_mode) {
			((struct pq_config *)
			pq_config_fake)->tdc.ambient_config.ambient =
			0;
			((struct pq_config *)pq_config_fake)
				->tdc.ref_mode_dark_id = 0;
			((struct pq_config *)pq_config_fake)
				->tdc.dm31_avail = 1;
		}
		if (is_hdr10_src_primary_changed()) {
			hdr10_src_primary_changed = true;
			pr_dolby_dbg("hdr10 src primary changed!\n");
		}
		if (src_format != tv_dovi_setting->src_format ||
			tv_dovi_setting->video_width != w ||
			tv_dovi_setting->video_height != h ||
			hdr10_src_primary_changed) {
			pr_dolby_dbg("reset control_path fmt %d->%d, w %d->%d, h %d->%d\n",
				tv_dovi_setting->src_format, src_format,
				tv_dovi_setting->video_width, w,
				tv_dovi_setting->video_height, h);
			/*for hdmi in cert*/
			if (dolby_vision_flags & FLAG_CERTIFICAION)
				vf_changed = true;
			if (p_funcs_tv && p_funcs_tv->tv_control_path)
				p_funcs_tv->tv_control_path
					(FORMAT_INVALID, 0,
					NULL, 0,
					NULL, 0,
					0, 0,
					SIGNAL_RANGE_SMPTE,
					NULL, NULL,
					0,
					NULL,
					NULL,
					NULL, 0,
					NULL,
					NULL);
		}
		tv_dovi_setting->video_width = w << 16;
		tv_dovi_setting->video_height = h << 16;
		if (debug_cp_res > 0) {
			tv_dovi_setting->video_width = debug_cp_res & 0xffff0000;
			tv_dovi_setting->video_height = (debug_cp_res & 0xffff) << 16;
		}
		if (!p_funcs_tv || !p_funcs_tv->tv_control_path)
			return -1;
		if (!(dolby_vision_flags & FLAG_CERTIFICAION) &&
		    (strstr(cfg_info[cur_pic_mode].pic_mode_name, "dark") ||
		    strstr(cfg_info[cur_pic_mode].pic_mode_name, "Dark") ||
		    strstr(cfg_info[cur_pic_mode].pic_mode_name, "DARK"))) {
			memcpy(tv_input_info,
			       brightness_off,
			       sizeof(brightness_off));
		} else {
			memset(tv_input_info, 0, sizeof(brightness_off));
		}

		/*config source fps and gd_rf_adjust, dmcfg_id*/
		tv_input_info->content_fps = content_fps * (1 << 16);
		tv_input_info->gd_rf_adjust = gd_rf_adjust;
		tv_input_info->tid = cur_pic_mode;

		if (debug_dolby & 0x400)
			do_gettimeofday(&start);

		/*for hdmi in cert, only run control_path for different frame*/
		if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
		    !vf_changed && input_mode == IN_MODE_HDMI) {
			run_control_path = false;
		}

		if (run_control_path) {
			if (ambient_update) {
				/*only if cfg enables darkdetail we allow the API to set values*/
				if (((struct pq_config *)pq_config_fake)->
					tdc.ambient_config.dark_detail) {
					ambient_config_new.dark_detail =
					cfg_info[cur_pic_mode].dark_detail;
				}
				p_ambient = &ambient_config_new;
			} else {
				if (ambient_test_mode == 1 && toggle_mode == 1 &&
				    frame_count < AMBIENT_CFG_FRAMES) {
					p_ambient = &ambient_test_cfg[frame_count];
				} else if (ambient_test_mode == 2 && toggle_mode == 1 &&
					   frame_count < AMBIENT_CFG_FRAMES) {
					p_ambient = &ambient_test_cfg_2[frame_count];
				} else if (ambient_test_mode == 3 && toggle_mode == 1 &&
					   hdmi_frame_count < AMBIENT_CFG_FRAMES) {
					p_ambient = &ambient_test_cfg_3[hdmi_frame_count];
				} else if (((struct pq_config *)pq_config_fake)->
					tdc.ambient_config.dark_detail) {
					/*only if cfg enables darkdetail we allow the API to set*/
					ambient_darkdetail.dark_detail =
						cfg_info[cur_pic_mode].dark_detail;
					p_ambient = &ambient_darkdetail;
				}
			}
			if (debug_dolby & 0x200)
				pr_dolby_dbg("[count %d %d]dark_detail from cfg:%d,from api:%d\n",
					     hdmi_frame_count, frame_count,
					     ((struct pq_config *)pq_config_fake)->
					     tdc.ambient_config.dark_detail,
					     cfg_info[cur_pic_mode].dark_detail);
			flag = p_funcs_tv->tv_control_path
				(src_format, input_mode,
				comp_buf[current_id],
				(src_format == FORMAT_DOVI) ? total_comp_size : 0,
				md_buf[current_id],
				(src_format == FORMAT_DOVI) ? total_md_size : 0,
				src_bdp,
				src_chroma_format,
				SIGNAL_RANGE_SMPTE, /* bit/chroma/range */
				(struct pq_config *)pq_config_fake, &menu_param,
				(!el_flag) ||
				(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
				&hdr10_param,
				tv_dovi_setting,
				vsem_if_buf, vsem_if_size,
				p_ambient,
				tv_input_info);

			if (debug_dolby & 0x400) {
				do_gettimeofday(&end);
				time_use = (end.tv_sec - start.tv_sec) * 1000000 +
					(end.tv_usec - start.tv_usec);

				pr_info("controlpath time: %5ld us\n", time_use);
			}
			if (flag >= 0) {
				if (input_mode == IN_MODE_HDMI) {
					if (h > 1080)
						tv_dovi_setting->core1_reg_lut[1] =
							0x0000000100000043;
					else
						tv_dovi_setting->core1_reg_lut[1] =
							0x0000000100000042;
					if (get_video_reverse() &&
					(src_format == FORMAT_DOVI ||
					src_format == FORMAT_DOVI_LL)) {
						u32 coeff[3][3];
						u64 *p =
							&tv_dovi_setting->core1_reg_lut[9];

						coeff[0][1] = (p[0] >> 16) & 0xffff;
						coeff[0][2] = p[1] & 0xffff;
						coeff[1][1] = p[2] & 0xffff;
						coeff[1][2] = (p[2] >> 16) & 0xffff;
						coeff[2][1] = (p[3] >> 16) & 0xffff;
						coeff[2][2] = p[4] & 0xffff;

						p[0] = (p[0] & 0xffffffff0000ffff) |
							(coeff[0][2] << 16);
						p[1] = (p[1] & 0xffffffffffff0000) |
							coeff[0][1];
						p[2] = (p[2] & 0xffffffff00000000) |
							(coeff[1][1] << 16) |
							coeff[1][2];
						p[3] = (p[3] & 0xffffffff0000ffff) |
							(coeff[2][2] << 16);
						p[4] = (p[4] & 0xffffffffffff0000) |
							coeff[2][1];
					}
				} else {
					if (src_format == FORMAT_HDR10)
						tv_dovi_setting->core1_reg_lut[1] =
							0x000000010000404c;
					else if (el_halfsize_flag)
						tv_dovi_setting->core1_reg_lut[1] =
							0x000000010000004c;
					else
						tv_dovi_setting->core1_reg_lut[1] =
							0x0000000100000044;
				}
				/* enable CRC */
				if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
					!(dolby_vision_flags & FLAG_DISABLE_CRC))
					tv_dovi_setting->core1_reg_lut[3] =
						0x000000ea00000001;
				tv_dovi_setting->src_format = src_format;
				tv_dovi_setting->el_flag = el_flag;
				tv_dovi_setting
					->el_halfsize_flag = el_halfsize_flag;
				tv_dovi_setting->video_width = w;
				tv_dovi_setting->video_height = h;
				tv_dovi_setting
					->input_mode = input_mode;
				tv_dovi_setting_change_flag = true;
				dovi_setting_video_flag = video_frame;

				if (debug_dolby & 1) {
					if (el_flag)
						pr_dolby_dbg
					("tv setting %s-%d:flag=%x,md=%d,comp=%d\n",
						 input_mode == IN_MODE_HDMI ?
						 "hdmi" : "ott",
						 src_format,
						 flag,
						 total_md_size,
						 total_comp_size);
					else
						pr_dolby_dbg
						("tv setting %s-%d:flag=%x,md=%d\n",
						 input_mode == IN_MODE_HDMI ?
						 "hdmi" : "ott",
						 src_format,
						 flag,
						 total_md_size);
				}
				dump_tv_setting(tv_dovi_setting,
					frame_count, debug_dolby);
				el_mode = el_flag;
				mel_mode = mel_flag;
				ret = 0; /* setting updated */
			} else {
				tv_dovi_setting->video_width = 0;
				tv_dovi_setting->video_height = 0;
				pr_dolby_error("tv_control_path() failed\n");
			}
		} else { /*for cert: vf no change, not run cp*/
			if (h > 1080)
				tv_dovi_setting->core1_reg_lut[1] =
					0x0000000100000043;
			else
				tv_dovi_setting->core1_reg_lut[1] =
					0x0000000100000042;

			/* enable CRC */
			if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
				!(dolby_vision_flags & FLAG_DISABLE_CRC))
				tv_dovi_setting->core1_reg_lut[3] =
					0x000000ea00000001;
			tv_dovi_setting->src_format = src_format;
			tv_dovi_setting->el_flag = el_flag;
			tv_dovi_setting
				->el_halfsize_flag = el_halfsize_flag;
			tv_dovi_setting->video_width = w;
			tv_dovi_setting->video_height = h;
			tv_dovi_setting
				->input_mode = input_mode;
			tv_dovi_setting_change_flag = true;
			dovi_setting_video_flag = video_frame;
			ret = 0;
		}
		return ret;
	}

	/* update input mode for HDMI in STB core */
	if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
		tv_dovi_setting->input_mode = input_mode;
		if (is_meson_stb_hdmimode()) {
			tv_dovi_setting->src_format = src_format;
			tv_dovi_setting->video_width = w;
			tv_dovi_setting->video_height = h;
			tv_dovi_setting->el_flag = false;
			tv_dovi_setting->el_halfsize_flag = false;
			dolby_vision_run_mode_delay = RUN_MODE_DELAY;
		} else {
			dolby_vision_run_mode_delay = 0;
		}
	}
	/* check dst format */
	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
	    dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT)
		dst_format = FORMAT_DOVI;
	else if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_HDR10)
		dst_format = FORMAT_HDR10;
	else
		dst_format = FORMAT_SDR;

	/* STB core */
	/* check target luminance */
	graphic_min = dolby_vision_graphic_min;
	if (dolby_vision_graphic_max != 0) {
		graphic_max = dolby_vision_graphic_max;
	} else {
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
		    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422) {
			graphic_max =
				dv_target_graphics_LL_max
					[src_format][dst_format];
		} else {
			graphic_max =
				dv_target_graphics_max
					[src_format][dst_format];
		}
		if (dv_graphic_blend_test && dst_format == FORMAT_HDR10)
			graphic_max = dv_HDR10_graphics_max;
	}

	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->vout_device->dv_info->ieeeoui == 0x00d046) {
			if (vinfo->vout_device->dv_info->ver == 0) {
				/* need lookup PQ table ... */
			} else if (vinfo->vout_device->dv_info->ver == 1) {
				if (vinfo->vout_device->dv_info->tmaxLUM) {
					/* Target max luminance = 100+50*CV */
					graphic_max =
					target_lumin_max =
						(vinfo->vout_device
						->dv_info->tmaxLUM
						* 50 + 100);
					/* Target min luminance = (CV/127)^2 */
					graphic_min =
					dolby_vision_target_min =
					(vinfo->vout_device->dv_info->tminLUM
					^ 2) * 10000 / (127 * 127);
				}
			}
		} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
			if (vinfo->hdr_info.lumi_max) {
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				graphic_max =
				target_lumin_max = 50 *
					(2 ^ (vinfo->hdr_info.lumi_max >> 5));
				/* Desired Content Min Luminance =*/
				/*	Desired Content Max Luminance*/
				/*	* (CV/255) * (CV/255) / 100	*/
				graphic_min =
				dolby_vision_target_min =
					target_lumin_max * 10000
					* vinfo->hdr_info.lumi_min
					* vinfo->hdr_info.lumi_min
					/ (255 * 255 * 100);
			}
		}
		if (target_lumin_max) {
			dolby_vision_target_max[0][0] =
			dolby_vision_target_max[0][1] =
			dolby_vision_target_max[1][0] =
			dolby_vision_target_max[1][1] =
			dolby_vision_target_max[2][0] =
			dolby_vision_target_max[2][1] =
				target_lumin_max;
		} else {
			memcpy(dolby_vision_target_max,
			       dolby_vision_default_max,
			       sizeof(dolby_vision_target_max));
		}
	}

	if (is_osd_off) {
		graphic_min = 0;
		graphic_max = 0;
	}

	if (new_dovi_setting.video_width && new_dovi_setting.video_height) {
	/* Toggle multiple frames in one vsync case: */
	/* new_dovi_setting.video_width will be available, but not be applied */
	/* So use new_dovi_setting as reference instead of dovi_setting. */
	/* To avoid unnecessary reset control_path. */
		cur_src_format = new_dovi_setting.src_format;
		cur_dst_format = new_dovi_setting.dst_format;
	} else {
		cur_src_format = dovi_setting.src_format;
		cur_dst_format = dovi_setting.dst_format;
	}

	if (src_format != cur_src_format ||
	    dst_format != cur_dst_format) {
		pr_dolby_dbg
		("reset: src:%d-%d, dst:%d-%d, frame_count:%d, flags:0x%x\n",
		 cur_src_format, src_format,
		 cur_dst_format, dst_format,
		 frame_count, dolby_vision_flags);
		p_funcs_stb->control_path
			(FORMAT_INVALID, 0,
			 comp_buf[current_id], 0,
			 md_buf[current_id], 0,
			 0, 0, 0, SIGNAL_RANGE_SMPTE,
			 0, 0, 0, 0,
			 0,
			 &hdr10_param,
			 &new_dovi_setting);
	}
	if (!vsvdb_config_set_flag) {
		memset(&new_dovi_setting.vsvdb_tbl[0],
		       0, sizeof(new_dovi_setting.vsvdb_tbl));
		new_dovi_setting.vsvdb_len = 0;
		new_dovi_setting.vsvdb_changed = 1;
		vsvdb_config_set_flag = true;
	}
	if ((dolby_vision_flags & FLAG_DISABLE_LOAD_VSVDB) == 0) {
		/* check if vsvdb is changed */
		if (vinfo &&  vinfo->vout_device &&
		    vinfo->vout_device->dv_info &&
		    vinfo->vout_device->dv_info->ieeeoui == 0x00d046 &&
		    vinfo->vout_device->dv_info->block_flag == CORRECT) {
			if (new_dovi_setting.vsvdb_len
				!= vinfo->vout_device->dv_info->length + 1)
				new_dovi_setting.vsvdb_changed = 1;
			else if (memcmp
				(&new_dovi_setting.vsvdb_tbl[0],
				 &vinfo->vout_device->dv_info->rawdata[0],
				 vinfo->vout_device->dv_info->length + 1))
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
			       0, sizeof(new_dovi_setting.vsvdb_tbl));
			memcpy(&new_dovi_setting.vsvdb_tbl[0],
			       &vinfo->vout_device->dv_info->rawdata[0],
			       vinfo->vout_device->dv_info->length + 1);
			new_dovi_setting.vsvdb_len =
				vinfo->vout_device->dv_info->length + 1;
			if (new_dovi_setting.vsvdb_changed &&
			    new_dovi_setting.vsvdb_len) {
				int k = 0;

				pr_dolby_dbg("new vsvdb[%d]:\n",
					     new_dovi_setting.vsvdb_len);
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
			}
		} else {
			if (new_dovi_setting.vsvdb_len)
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
				0, sizeof(new_dovi_setting.vsvdb_tbl));
			new_dovi_setting.vsvdb_len = 0;
		}
	}

	/* check video/graphics priority on the fly */
	/* cert: some graphic test also need video pri 5223,5243,5253,5263 */
	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		dolby_vision_graphics_priority = 0;
	} else {
		if (get_video_enabled() && is_graphics_output_off())
			dolby_vision_graphics_priority = 0;
		else
			dolby_vision_graphics_priority = 1;
	}
	if (dolby_vision_graphics_priority ||
	    (dolby_vision_flags & FLAG_PRIORITY_GRAPHIC))
		pri_mode = G_PRIORITY;
	else
		pri_mode = V_PRIORITY;

	if (dst_format == FORMAT_DOVI) {
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
		    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422)
			new_dovi_setting.use_ll_flag = 1;
		else
			new_dovi_setting.use_ll_flag = 0;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444 ||
		    (dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT))
			new_dovi_setting.ll_rgb_desired = 1;
		else
			new_dovi_setting.ll_rgb_desired = 0;
	} else {
		new_dovi_setting.use_ll_flag = 0;
		new_dovi_setting.ll_rgb_desired = 0;
	}
	if (dst_format == FORMAT_HDR10 &&
	    (dolby_vision_flags & FLAG_DOVI2HDR10_NOMAPPING))
		new_dovi_setting.dovi2hdr10_nomapping = 1;
	else
		new_dovi_setting.dovi2hdr10_nomapping = 0;

	/* always use rgb setting */
	new_dovi_setting.g_bitdepth = 8;
	new_dovi_setting.g_format = G_SDR_RGB;
	new_dovi_setting.diagnostic_enable = 0;
	new_dovi_setting.diagnostic_mux_select = 0;
	new_dovi_setting.dovi_ll_enable = 0;
	if (vinfo) {
		new_dovi_setting.vout_width = vinfo->width;
		new_dovi_setting.vout_height = vinfo->height;
	} else {
		new_dovi_setting.vout_width = 0;
		new_dovi_setting.vout_height = 0;
	}
	memset(&new_dovi_setting.ext_md, 0, sizeof(struct ext_md_s));
	new_dovi_setting.video_width = w << 16;
	new_dovi_setting.video_height = h << 16;
	if (debug_dolby & 0x400)
		do_gettimeofday(&start);
	if (is_meson_stb_hdmimode()) {
		/* generate core2/core3 setting */
		flag = p_funcs_stb->control_path
			(src_format, dst_format,
			comp_buf[current_id],
			(src_format == FORMAT_DOVI) ? total_comp_size : 0,
			md_buf[current_id],
			(src_format == FORMAT_DOVI) ? total_md_size : 0,
			V_PRIORITY,
			src_bdp, 0, SIGNAL_RANGE_SMPTE,
			graphic_min,
			graphic_max * 10000,
			dolby_vision_target_min,
			dolby_vision_target_max
			[src_format][dst_format] * 10000,
			false,
			&hdr10_param,
			&new_dovi_setting);
		/* overwrite core3 meta by hdmi input */
		if (dst_format == FORMAT_DOVI &&
		dolby_vision_ll_policy == DOLBY_VISION_LL_DISABLE)
			prepare_dv_meta
				(&new_dovi_setting.md_reg3,
				md_buf[current_id], total_md_size);
	} else {
		flag = p_funcs_stb->control_path
		(src_format, dst_format,
		comp_buf[current_id],
		(src_format == FORMAT_DOVI) ? total_comp_size : 0,
		md_buf[current_id],
		(src_format == FORMAT_DOVI) ? total_md_size : 0,
		pri_mode,
		src_bdp, 0, SIGNAL_RANGE_SMPTE, /* bit/chroma/range */
		graphic_min,
		graphic_max * 10000,
		dolby_vision_target_min,
		dolby_vision_target_max[src_format][dst_format] * 10000,
		(!el_flag && !mel_flag) ||
		(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
		&hdr10_param,
		&new_dovi_setting);
	}
	if (debug_dolby & 0x400) {
		do_gettimeofday(&end);
		time_use = (end.tv_sec - start.tv_sec) * 1000000 +
				(end.tv_usec - start.tv_usec);

		pr_info("controlpath time: %5ld us\n", time_use);
	}
	if (flag >= 0) {
		stb_core_setting_update_flag |= flag;
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) &&
		    dst_format == FORMAT_DOVI)
			new_dovi_setting.dovi_ll_enable = 1;
		if ((dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT) &&
		    dst_format == FORMAT_DOVI) {
			new_dovi_setting.dovi_ll_enable = 1;
			new_dovi_setting.diagnostic_enable = 1;
			new_dovi_setting.diagnostic_mux_select = 1;
		}
		if (debug_dolby & 2)
			pr_dolby_dbg("ll_enable=%d,diagnostic=%d,ll_policy=%d\n",
				     new_dovi_setting.dovi_ll_enable,
				     new_dovi_setting.diagnostic_enable,
				     dolby_vision_ll_policy);
		new_dovi_setting.src_format = src_format;
		new_dovi_setting.dst_format = dst_format;
		new_dovi_setting.el_flag = el_flag;
		new_dovi_setting.el_halfsize_flag = el_halfsize_flag;
		new_dovi_setting.video_width = w;
		new_dovi_setting.video_height = h;
		dovi_setting_video_flag = video_frame;
		if (debug_dolby & 1) {
			if (is_video_output_off(vf))
				pr_dolby_dbg
				("setting %d->%d(T:%d-%d), osd:%dx%d\n",
				 src_format, dst_format,
				 dolby_vision_target_min,
				 dolby_vision_target_max
				 [src_format][dst_format],
				 osd_graphic_width,
				 osd_graphic_height);
			if (el_flag) {
				pr_dolby_dbg
					("v %d: %dx%d %d->%d(T:%d-%d), g %d: %dx%d %d->%d, %s\n",
					dovi_setting_video_flag,
					w == 0xffff ? 0 : w,
					h == 0xffff ? 0 : h,
					src_format, dst_format,
					dolby_vision_target_min,
					dolby_vision_target_max
					[src_format][dst_format],
					!is_graphics_output_off(),
					osd_graphic_width,
					osd_graphic_height,
					graphic_min,
					graphic_max * 10000,
					pri_mode == V_PRIORITY ?
					"vpr" : "gpr");
				pr_dolby_dbg
					("flag=%x, md=%d, comp=%d, frame:%d\n",
					flag, total_md_size, total_comp_size,
					frame_count);
			} else {
				pr_dolby_dbg("v %d: %dx%d %d->%d(T:%d-%d), g %d: %dx%d %d->%d, %s, flag=%x, md=%d, frame:%d\n",
					dovi_setting_video_flag,
					w == 0xffff ? 0 : w,
					h == 0xffff ? 0 : h,
					src_format, dst_format,
					dolby_vision_target_min,
					dolby_vision_target_max
					[src_format][dst_format],
					!is_graphics_output_off(),
					osd_graphic_width,
					osd_graphic_height,
					graphic_min,
					graphic_max * 10000,
					pri_mode == V_PRIORITY ?
					"vpr" : "gpr",
					flag,
					total_md_size, frame_count);
			}
		}
		dump_setting(&new_dovi_setting, frame_count, debug_dolby);
		el_mode = el_flag;
		mel_mode = mel_flag;
		return 0; /* setting updated */
	}
	if (flag < 0) {
		pr_dolby_dbg("video %d:%dx%d setting %d->%d(T:%d-%d): pri_mode=%d, no_el=%d, md=%d, frame:%d\n",
			dovi_setting_video_flag,
			w == 0xffff ? 0 : w,
			h == 0xffff ? 0 : h,
			src_format, dst_format,
			dolby_vision_target_min,
			dolby_vision_target_max
			[src_format][dst_format],
			pri_mode,
			(!el_flag && !mel_flag),
			total_md_size, frame_count);
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		pr_dolby_error("control_path(%d, %d) failed %d\n",
			src_format, dst_format, flag);
		if ((debug_dolby & 0x2000) && dump_enable && total_md_size > 0) {
			pr_dolby_dbg("control_path failed, md(%d):\n",
				total_md_size);
			for (i = 0; i < total_md_size + 7; i += 8)
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[current_id][i],
					md_buf[current_id][i + 1],
					md_buf[current_id][i + 2],
					md_buf[current_id][i + 3],
					md_buf[current_id][i + 4],
					md_buf[current_id][i + 5],
					md_buf[current_id][i + 6],
					md_buf[current_id][i + 7]);
		}
	}
	return -1; /* do nothing for this frame */
}
EXPORT_SYMBOL(dolby_vision_parse_metadata);

/*dual_layer && parse_ret_flags   =1 =>mel*/
/*dual_layer && parse_ret_flags !=1 =>fel*/
bool vf_is_fel(struct vframe_s *vf)
{
	enum vframe_signal_fmt_e fmt;
	bool fel = false;

	if (!vf)
		return false;

	fmt = get_vframe_src_fmt(vf);

	if (fmt == VFRAME_SIGNAL_FMT_DOVI) {
		if (debug_dolby & 0x1)
			pr_dolby_dbg("dual layer %d, parse ret flags %d\n",
				     vf->src_fmt.dual_layer,
				     vf->src_fmt.parse_ret_flags);
		if (vf->src_fmt.dual_layer && vf->src_fmt.parse_ret_flags != 1)
			fel = true;
	}
	return fel;
}

/* 0: no el; >0: with el */
/* 1: need wait el vf    */
/* 2: no match el found  */
/* 3: found match el     */
int dolby_vision_wait_metadata(struct vframe_s *vf)
{
	struct vframe_s *el_vf;
	int ret = 0;
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool vd1_on = false;

	if (!dolby_vision_enable || !module_installed)
		return 0;

	if (single_step_enable) {
		if (dolby_vision_flags & FLAG_SINGLE_STEP)
			/* wait fake el for "step" */
			return 1;

		dolby_vision_flags |= FLAG_SINGLE_STEP;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		bool ott_mode = true;

		if (is_meson_tvmode())
			ott_mode = tv_dovi_setting->input_mode !=
				IN_MODE_HDMI;
		if (debug_dolby & 0x1000)
			pr_dolby_dbg("setting_update_count %d, crc_count %d, flag %x\n",
				setting_update_count, crc_count, dolby_vision_flags);
		if (setting_update_count > crc_count &&
			!(dolby_vision_flags & FLAG_DISABLE_CRC)) {
			if (ott_mode)
				return 1;
		}
	}

	if (is_dovi_dual_layer_frame(vf)) {
		el_vf = dvel_vf_peek();
		while (el_vf) {
			if (debug_dolby & 2)
				pr_dolby_dbg("=== peek bl(%p-%lld) with el(%p-%lld) ===\n",
					     vf, vf->pts_us64,
					     el_vf, el_vf->pts_us64);
			if (el_vf->pts_us64 == vf->pts_us64 ||
			    !(dolby_vision_flags & FLAG_CHECK_ES_PTS)) {
				/* found el */
				ret = 3;
				break;
			} else if (el_vf->pts_us64 < vf->pts_us64) {
				if (debug_dolby & 2)
					pr_dolby_dbg("bl(%p-%lld) => skip el pts(%p-%lld)\n",
						     vf, vf->pts_us64,
						     el_vf, el_vf->pts_us64);
				el_vf = dvel_vf_get();
				dvel_vf_put(el_vf);
				vf_notify_provider
					(DVEL_RECV_NAME,
					 VFRAME_EVENT_RECEIVER_PUT, NULL);
				if (debug_dolby & 2)
					pr_dolby_dbg("=== get & put el(%p-%lld) ===\n",
						     el_vf, el_vf->pts_us64);

				/* skip old el and peek new */
				el_vf = dvel_vf_peek();
			} else {
				/* no el found */
				ret = 2;
				break;
			}
		}
		/* need wait el */
		if (!el_vf) {
			if (debug_dolby & 2)
				pr_dolby_dbg("=== bl wait el(%p-%lld) ===\n",
					     vf, vf->pts_us64);
			ret = 1;
		}
	}
	if (ret == 1)
		return ret;

	if (!dolby_vision_wait_init && !dolby_vision_core1_on) {
		ret = is_dovi_frame(vf);
		if (ret) {
			/* STB hdmi LL input as HDR */
			if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
			    ret == 2 && hdmi_to_stb_policy &&
			    (is_meson_tm2_stbmode() || is_meson_t7_stbmode()))
				check_format = FORMAT_HDR10;
			else
				check_format = FORMAT_DOVI;
			ret = 0;
		} else if (is_primesl_frame(vf)) {
			check_format = FORMAT_PRIMESL;
		} else if (is_hdr10_frame(vf)) {
			check_format = FORMAT_HDR10;
		} else if (is_hlg_frame(vf)) {
			check_format = FORMAT_HLG;
		} else if (is_hdr10plus_frame(vf)) {
			check_format = FORMAT_HDR10PLUS;
		} else if (is_mvc_frame(vf)) {
			check_format = FORMAT_MVC;
		} else {
			check_format = FORMAT_SDR;
		}

		if (vf)
			update_src_format(check_format, vf);

		if (dolby_vision_policy_process(&mode, check_format)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				dolby_vision_wait_init = true;
				dolby_vision_target_mode = mode;
				dolby_vision_wait_on = true;

				/*dv off->on, delay vfream*/
				if (dolby_vision_policy ==
				    DOLBY_VISION_FOLLOW_SOURCE &&
				    dolby_vision_mode ==
				    DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				    mode ==
				    DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL &&
				    dolby_vision_wait_delay > 0 &&
				    !vf_is_fel(vf)) {
					dolby_vision_wait_count =
					dolby_vision_wait_delay;
				} else {
					dolby_vision_wait_count = 0;
				}

				pr_dolby_dbg("dolby_vision_need_wait src=%d mode=%d\n",
					check_format, mode);
			}
		}
		/*chip after g12 not used bit VPP_MISC 9:11*/
		if (is_meson_gxm() || is_meson_txlx() || is_meson_g12()) {
			if (READ_VPP_DV_REG(VPP_MISC) & (1 << 10))
				vd1_on = true;
		} else if (is_meson_tm2() || is_meson_sc2() || is_meson_t7() ||
			   is_meson_t3() || is_meson_s4d() || is_meson_t5w()) {
			if (READ_VPP_DV_REG(VD1_BLEND_SRC_CTRL) & (1 << 0))
				vd1_on = true;
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (/*dolby_vision_wait_init && */vd1_on && is_meson_tvmode() && !force_runmode)
			dolby_vision_on_count =
				dolby_vision_run_mode_delay + 1;
		if (debug_dolby & 8)
			pr_dolby_dbg("dolby_vision_on_count %d, vd1_on %d\n",
				      dolby_vision_on_count, vd1_on);
	}
	if (dolby_vision_wait_init && dolby_vision_wait_count > 0) {
		if (debug_dolby & 8)
			pr_dolby_dbg("delay wait %d\n",
				dolby_vision_wait_count);

		if (!get_disable_video_flag(VD1_PATH)) {
			/*update only after app enable video display,*/
			/* to distinguish play start and netflix exit*/
			send_hdmi_pkt_ahead(FORMAT_DOVI, vinfo);
			dolby_vision_wait_count--;
		} else {
			/*exit netflix, still process vf after video disable,*/
			/*wait init will be on, need reset wait init */
			dolby_vision_wait_init = false;
			dolby_vision_wait_count = 0;
			if (debug_dolby & 8)
				pr_dolby_dbg("clear dolby_vision_wait_on\n");
		}
		ret = 1;
	} else if (dolby_vision_core1_on &&
		(dolby_vision_on_count <=
		dolby_vision_run_mode_delay))
		ret = 1;

	if (vf && (debug_dolby & 8))
		pr_dolby_dbg("wait return %d, vf %p(index %d), core1_on %d\n",
			      ret, vf, vf->omx_index, dolby_vision_core1_on);

	return ret;
}

int dolby_vision_update_metadata(struct vframe_s *vf, bool drop_flag)
{
	int ret = -1;

	if (!dolby_vision_enable)
		return -1;

	/*clear dv_vf when render first frame */
	if (vf && vf->omx_index == 0)
		dolby_vision_clear_buf();

	if (vf && dolby_vision_vf_check(vf)) {
		ret = dolby_vision_parse_metadata(vf, 1, false, drop_flag);
		frame_count++;
	}

	return ret;
}
EXPORT_SYMBOL(dolby_vision_update_metadata);

/* to re-init the src format after video off -> on case */
int dolby_vision_update_src_format(struct vframe_s *vf, u8 toggle_mode)
{
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	int ret = 0;

	if (!dolby_vision_enable || !vf)
		return -1;

	/* src_format is valid, need not re-init */
	if (dolby_vision_src_format != 0)
		return 0;

	/* vf is not in the dv queue, new frame case */
	if (dolby_vision_vf_check(vf))
		return 0;

	ret = is_dovi_frame(vf);
	if (ret) {
		/* STB hdmi LL input as HDR */
		if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		    (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) &&
		    hdmi_to_stb_policy && ret == 2)
			check_format = FORMAT_HDR10;
		else
			check_format = FORMAT_DOVI;
	} else if (is_primesl_frame(vf)) {
		check_format = FORMAT_PRIMESL;
	} else if (is_hdr10_frame(vf)) {
		check_format = FORMAT_HDR10;
	} else if (is_hlg_frame(vf)) {
		check_format = FORMAT_HLG;
	} else if (is_hdr10plus_frame(vf)) {
		check_format = FORMAT_HDR10PLUS;
	} else if (is_mvc_frame(vf)) {
		check_format = FORMAT_MVC;
	} else {
		check_format = FORMAT_SDR;
	}
	if (vf)
		update_src_format(check_format, vf);

	if (!dolby_vision_wait_init &&
	    !dolby_vision_core1_on &&
	    dolby_vision_src_format != 0) {
		if (dolby_vision_policy_process
			(&mode, check_format)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				dolby_vision_wait_init = true;
				dolby_vision_target_mode = mode;
				dolby_vision_wait_on = true;
				pr_dolby_dbg
					("dolby_vision_need_wait src=%d mode=%d\n",
					 check_format, mode);
			}
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (dolby_vision_wait_init &&
		    (READ_VPP_DV_REG(VPP_MISC) & (1 << 10)))
			dolby_vision_on_count =
				dolby_vision_run_mode_delay + 1;
	}
	pr_dolby_dbg
		("%s done vf:%p, src=%d, toggle mode:%d\n",
		__func__, vf, dolby_vision_src_format, toggle_mode);
	return 1;
}
EXPORT_SYMBOL(dolby_vision_update_src_format);

static void update_dolby_vision_status(enum signal_format_enum src_format)
{
	if ((src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL) &&
	    dolby_vision_status != DV_PROCESS) {
		pr_dolby_dbg("Dolby Vision mode changed to DV_PROCESS %d\n",
			     src_format);
		dolby_vision_status = DV_PROCESS;
	} else if (src_format == FORMAT_HDR10 &&
		   dolby_vision_status != HDR_PROCESS) {
		pr_dolby_dbg("Dolby Vision mode changed to HDR_PROCESS %d\n",
			     src_format);
		dolby_vision_status = HDR_PROCESS;
	} else if (src_format == FORMAT_HLG &&
		   dolby_vision_status != HLG_PROCESS &&
		   (is_meson_tm2_tvmode() || is_meson_t7_tvmode() ||
		   is_meson_t3_tvmode() || is_meson_t5w())) {
		pr_dolby_dbg
			("Dolby Vision mode changed to HLG_PROCESS %d\n",
			src_format);
		dolby_vision_status = HLG_PROCESS;
	} else if (src_format == FORMAT_SDR &&
		   dolby_vision_status != SDR_PROCESS) {
		pr_dolby_dbg("Dolby Vision mode changed to SDR_PROCESS %d\n",
			     src_format);
		dolby_vision_status = SDR_PROCESS;
	}
}

static u8 last_pps_state;
static void bypass_pps_path(u8 pps_state)
{
	if (is_meson_txlx_package_962E()) {
		if (pps_state == 2) {
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 1, 0, 1);
			VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0, 0x08000800);
		} else if (pps_state == 1) {
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 0, 0, 1);
			VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0, 0x20002000);
		}
	}
	if (pps_state && last_pps_state != pps_state) {
		pr_dolby_dbg("pps_state %d => %d\n",
			     last_pps_state, pps_state);
		last_pps_state = pps_state;
	}
}

/*In some cases, from full screen to small window, the L5 metadata of the stream does*/
/*not change, and the AOI area does not change, lead to the display to be incomplete*/
/*if vpp disp size smaller than stream source size,  ignore AOI info*/
static void update_aoi_flag(struct vframe_s *vf, u32 display_size)
{
	int tmp_h;
	int tmp_v;

	tmp_h = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compWidth : vf->width;
	tmp_v = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compHeight : vf->height;
	if (tmp_h != ((display_size >> 16) & 0xffff) ||
		tmp_v != (display_size & 0xffff)) {
		disable_aoi = true;
		if (debug_dolby & 1)
			pr_dolby_dbg
			("disp size != src size %d %d->%d %d\n",
			 tmp_h, tmp_v,
			 (display_size >> 16) & 0xffff,
			 display_size & 0xffff);
	} else {
		disable_aoi = false;
	}
}

static void update_dovi_core2_reg(void)
{
	u32 reg_val, reg_val_set, reg1_val, reg1_val_set, reg_changed = 0;

	if (is_meson_t7() && !tv_mode && update_mali_top_ctrl) {
		reg_val = VSYNC_RD_DV_REG(MALI_AFBCD_TOP_CTRL);
		reg_val_set = reg_val & (~mali_afbcd_top_ctrl_mask);
		if (is_dolby_vision_graphic_on())
			reg_val_set &= ~(1 << 14);
		else
			reg_val_set |= 1 << 14;
		reg_val_set |=
			(mali_afbcd_top_ctrl & mali_afbcd_top_ctrl_mask);
		if (reg_val != reg_val_set) {
			VSYNC_WR_DV_REG(MALI_AFBCD_TOP_CTRL, reg_val_set);
			reg_changed++;
		}

		reg1_val = VSYNC_RD_DV_REG(MALI_AFBCD1_TOP_CTRL);
		reg1_val_set = reg1_val & (~mali_afbcd1_top_ctrl_mask);
		if (is_dolby_vision_on() &&
		    (dolby_vision_mask & 2) &&
		    (core2_sel & 2))
			reg1_val_set &= ~(1 << 19);
		else
			reg1_val_set |= 1 << 19;
		reg1_val_set |=
			(mali_afbcd1_top_ctrl & mali_afbcd1_top_ctrl_mask);
		if (reg1_val != reg1_val_set) {
			VSYNC_WR_DV_REG(MALI_AFBCD1_TOP_CTRL, reg1_val_set);
			reg_changed++;
		}
		if (reg_changed)
			pr_dolby_dbg
				("%s reg changed from: (%04x):%08x->%08x, (%04x):%08x->%08x",
				__func__,
				MALI_AFBCD_TOP_CTRL,
				reg_val, reg_val_set,
				MALI_AFBCD1_TOP_CTRL,
				reg1_val, reg1_val_set);
		update_mali_top_ctrl = false;
	}
}

/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
/* pps_state 0: no change, 1: pps enable, 2: pps disable */
int dolby_vision_process(struct vframe_s *vf,
			 u32 display_size,
			 u8 toggle_mode, u8 pps_state)
{
	int src_chroma_format = 0;
	u32 h_size = (display_size >> 16) & 0xffff;
	u32 v_size = display_size & 0xffff;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool reset_flag = false;
	bool force_set = false;
	static int sdr_delay;
	unsigned int mode = dolby_vision_mode;
	static bool video_turn_off = true;
	static bool video_on[VD_PATH_MAX];
	int video_status = 0;
	int graphic_status = 0;
	int policy_changed = 0;
	int sink_changed = 0;
	int format_changed = 0;
	bool src_is_42210bit = false;
	u8 core_mask = 0x7;
	static bool reverse_status;
	bool reverse = false;
	bool reverse_changed = false;
	static u8 last_toggle_mode;
	struct vout_device_s *p_vout = NULL;

	if (!is_meson_box() && !is_meson_txlx() && !is_meson_tm2() && !is_meson_t7() &&
		!is_meson_t3() && !is_meson_t5w())
		return -1;

	if (dolby_vision_enable == 1 && tv_mode == 1)
		amdolby_vision_wakeup_queue();

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		if (vf) {
			h_size = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compWidth : vf->width;
			v_size = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compHeight : vf->height;
		} else {
			h_size = 0;
			v_size = 0;
		}
		dolby_vision_on_count = 1 +
			dolby_vision_run_mode_delay;
	} else {
		if (vf)
			update_aoi_flag(vf, display_size);
	}
	if (vf && (debug_dolby & 0x8))
		pr_dolby_dbg("%s: vf %p(index %d), mode %d, core1_on %d\n",
			     __func__, vf, vf->omx_index,
			     dolby_vision_mode, dolby_vision_core1_on);

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME)	{
		h_size = (display_size >> 16) & 0xffff;
		v_size = display_size & 0xffff;
		if (!is_meson_tvmode()) {
			/* stb control path case */
			if (new_dovi_setting.video_width & 0xffff &&
			    new_dovi_setting.video_height & 0xffff) {
				if (new_dovi_setting.video_width != h_size ||
				    new_dovi_setting.video_height != v_size) {
					if (debug_dolby & 8)
						pr_dolby_dbg
					("stb update disp size %d %d->%d %d\n",
					 new_dovi_setting.video_width,
					 new_dovi_setting.video_height,
					 h_size, v_size);
				}
				if (h_size && v_size) {
					new_dovi_setting.video_width =
						h_size;
					new_dovi_setting.video_height =
						v_size;
				} else {
					new_dovi_setting.video_width =
						0xffff;
					new_dovi_setting.video_height =
						0xffff;
				}

				/* tvcore need force config for res change */
				if (is_meson_stb_hdmimode() &&
				    (core1_disp_hsize != h_size ||
				     core1_disp_vsize != v_size))
					force_set = true;
			} else if (core1_disp_hsize != h_size ||
				core1_disp_vsize != v_size) {
				if (debug_dolby & 8)
					pr_dolby_dbg
					("stb update disp size %d %d->%d %d\n",
					 core1_disp_hsize, core1_disp_vsize,
					 h_size, v_size);
				if (h_size && v_size) {
					new_dovi_setting.video_width = h_size;
					new_dovi_setting.video_height = v_size;
				} else {
					new_dovi_setting.video_width = 0xffff;
					new_dovi_setting.video_height = 0xffff;
				}
			}
		} else {
			/* tv control path case */
			if (core1_disp_hsize != h_size ||
				core1_disp_vsize != v_size) {
				/* tvcore need force config for res change */
				force_set = true;
				if (debug_dolby & 8)
					pr_dolby_dbg
					("tv update disp size %d %d->%d %d\n",
					 core1_disp_hsize, core1_disp_vsize,
					 h_size, v_size);
			}
		}
		if ((!vf || toggle_mode != 1) && !sdr_delay) {
			/* log to monitor if has dv toggles not needed */
			/* !sdr_delay: except in transition from DV to SDR */
			pr_dolby_dbg("NULL/RPT frame %p, hdr module %s, video %s\n",
				     vf,
				     get_hdr_module_status(VD1_PATH, VPP_TOP0)
				     == HDR_MODULE_ON ? "on" : "off",
				     get_video_enabled() ? "on" : "off");
		}
	}

	last_toggle_mode = toggle_mode;
	if (debug_dolby & 0x1000)
		pr_dolby_dbg("setting_update_count %d, crc_count %d\n",
			setting_update_count, crc_count);
	if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
		!(dolby_vision_flags & FLAG_DISABLE_CRC) &&
	    setting_update_count > crc_count &&
	    is_dolby_vision_on()) {
		s32 delay_count =
			(dolby_vision_flags >>
			FLAG_FRAME_DELAY_SHIFT)
			& FLAG_FRAME_DELAY_MASK;
		bool ott_mode = true;

		if (is_meson_tvmode())
			ott_mode =
				(tv_dovi_setting->input_mode !=
				IN_MODE_HDMI);

		if ((is_meson_txlx_stbmode() ||
		     is_meson_tm2_stbmode() ||
		     is_meson_t7_stbmode() ||
		     is_meson_box()) &&
			setting_update_count == 1 &&
			crc_read_delay == 1) {
			/* work around to enable crc for frame 0 */
			VSYNC_WR_DV_REG(DOLBY_CORE3_CRC_CTRL, 1);
			crc_read_delay++;
		} else {
			crc_read_delay++;
			if (debug_dolby & 0x1000)
				pr_dolby_dbg("crc_read_delay %d, delay_count %d\n",
					crc_read_delay, delay_count);
			if (crc_read_delay > delay_count) {
				if (ott_mode) {
					tv_dolby_vision_insert_crc
						((crc_count == 0) ? true : false);
					crc_read_delay = 0;
				} else if (is_meson_tm2_tvmode() ||
					   is_meson_t7_tvmode() ||
					   is_meson_t3_tvmode() ||
					   is_meson_t5w()) {
					/* hdmi mode*/
					if (READ_VPP_DV_REG(DOLBY_TV_DIAG_CTRL) == 0xb) {
						u32 crc =
						READ_VPP_DV_REG
						(DOLBY_TV_OUTPUT_DM_CRC);
						/*pr_dolby_dbg("crc = 0x%08x\n", crc);*/
						snprintf(cur_crc, sizeof(cur_crc), "0x%08x", crc);
					}
					crc_count++;
					crc_read_delay = 0;
				}
			}
		}
	}

	video_status = is_video_turn_on(video_on, VD1_PATH);
	if (video_status == -1) {
		video_turn_off = true;
		pr_dolby_dbg("VD1 video off, video_status -1\n");
	} else if (video_status == 1) {
		pr_dolby_dbg("VD1 video on, video_status 1\n");
		video_turn_off = false;
	}

	if (dolby_vision_mode != dolby_vision_target_mode)
		format_changed = 1;

	graphic_status = is_graphic_changed();

	/* monitor policy changes */
	policy_changed = is_policy_changed();
	if (policy_changed || format_changed ||
	    (graphic_status & 2) || osd_update) {
		dolby_vision_set_toggle_flag(1);
		if (osd_update)
			osd_update = false;
	}

	if (!is_dolby_vision_on())
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;

	sink_changed = (is_sink_cap_changed(vinfo,
		&current_hdr_cap, &current_sink_available, VPP_TOP0) & 2) ? 1 : 0;
	if (is_meson_tvmode()) {
		reverse = get_video_reverse();
		if (reverse != reverse_status) {
			reverse_status = reverse;
			reverse_changed = true;
		}
		sink_changed = false;
		graphic_status = 0;
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;
		if (policy_changed || format_changed ||
			video_status == 1 || reverse_changed)
			pr_dolby_dbg("tv %s %s %s %s\n",
				policy_changed ? "policy changed" : "",
				video_status ? "video_status changed" : "",
				format_changed ? "format_changed" : "",
				reverse_changed ? "reverse_changed" : "");
	}
	if (sink_changed || policy_changed || format_changed ||
	    (video_status == 1 && !(dolby_vision_flags & FLAG_CERTIFICAION)) ||
	    (graphic_status & 2) ||
	    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT) ||
	    need_update_cfg || reverse_changed) {
		if (debug_dolby & 1)
			pr_dolby_dbg("sink %s,cap 0x%x,video %s,osd %s,vf %p,toggle %d\n",
				     current_sink_available ? "on" : "off",
				     current_hdr_cap,
				     video_turn_off ? "off" : "on",
				     is_graphics_output_off() ? "off" : "on",
				     vf, toggle_mode);
		/* do not toggle a new el vf */
		if (toggle_mode == 1)
			toggle_mode = 0;
		if (vf &&
		    !dolby_vision_parse_metadata
			(vf, toggle_mode, false, false)) {
			h_size = (display_size >> 16) & 0xffff;
			v_size = display_size & 0xffff;
			if (!is_meson_tvmode()) {
				new_dovi_setting.video_width = h_size;
				new_dovi_setting.video_height = v_size;
			}
			dolby_vision_set_toggle_flag(1);
		}
		need_update_cfg = false;
	}

	if (debug_dolby & 8)
		pr_dolby_dbg("vf %p, turn_off %d, video_status %d, toggle %d, flag %x\n",
			vf, video_turn_off, video_status,
			toggle_mode, dolby_vision_flags);

	if ((!vf && video_turn_off) ||
	    (video_status == -1)) {
		if (dolby_vision_policy_process(&mode, FORMAT_SDR)) {
			pr_dolby_dbg("Fake SDR, mode->%d\n", mode);
			if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE &&
			    mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				dolby_vision_target_mode =
					DOLBY_VISION_OUTPUT_MODE_BYPASS;
				dolby_vision_mode =
					DOLBY_VISION_OUTPUT_MODE_BYPASS;
				dolby_vision_set_toggle_flag(0);
				dolby_vision_wait_on = false;
				dolby_vision_wait_init = false;
			} else {
				dolby_vision_set_toggle_flag(1);
			}
		}
		if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
		((video_status == -1) && dolby_vision_core1_on)) {
			pr_dolby_dbg("update when video off\n");
			dolby_vision_parse_metadata(NULL, 1, false, false);
			dolby_vision_set_toggle_flag(1);
		}
		if (!vf && video_turn_off &&
			!dolby_vision_core1_on &&
			dolby_vision_src_format != 0) {
			pr_dolby_dbg("update src_fmt when video off\n");
			dolby_vision_src_format = 0;
		}
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		if (!is_meson_tvmode()) {
			if (vinfo && vinfo->vout_device &&
			    !vinfo->vout_device->dv_info &&
			    vsync_count < FLAG_VSYNC_CNT) {
				vsync_count++;
				update_dovi_core2_reg();
				return 0;
			}
		}
		if (dolby_vision_status != BYPASS_PROCESS) {
			if (vinfo && !is_meson_tvmode()) {
				if (vf && is_primesl_frame(vf)) {
					/* disable dolby immediately */
					pr_dolby_dbg("Dolby bypass: PRIMESL: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_PRIMESL,
						      FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else if (vf && is_hdr10plus_frame(vf)) {
					/* disable dolby immediately */
					pr_dolby_dbg("Dolby bypass: HDR10+: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HDR10PLUS,
						      FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else if (vf && is_hlg_frame(vf)) {
					/* disable dolby immediately */
					pr_dolby_dbg("Dolby bypass: HLG: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HLG,
						      FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else if (last_dst_format != FORMAT_DOVI) {
					/* disable dolby immediately:
					 * non-dovi alwyas hdr to adaptive
					 */
					pr_dolby_dbg("Dolby bypass: Switched %d to SDR\n",
						     last_dst_format);
					send_hdmi_pkt(dolby_vision_src_format,
						      FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else {
					/* disable dolby after sdr delay:
					 * dovi alwyas hdr to adaptive or dovi
					 * playback exit in adaptive mode on
					 * a dovi tv
					 */
					if (sdr_delay == 0) {
						pr_dolby_dbg("Dolby bypass: Start - Switched to SDR\n");
						dolby_vision_set_toggle_flag(1);
					}
					if ((get_video_mute() ==
					VIDEO_MUTE_ON_DV &&
					!(dolby_vision_flags & FLAG_MUTE)) ||
					(get_video_mute() == VIDEO_MUTE_OFF &&
					dolby_vision_flags & FLAG_MUTE))
						/* core 3 only */
						apply_stb_core_settings
						(dovi_setting_video_flag,
						 dolby_vision_mask & 0x4,
						 0,
						 (dovi_setting.video_width
						 << 16)
						 | dovi_setting.video_height,
						 pps_state);
					send_hdmi_pkt(dolby_vision_src_format,
						      FORMAT_SDR, vinfo, vf);
					if (sdr_delay >= MAX_TRANSITION_DELAY) {
						pr_dolby_dbg("Dolby bypass: Done - Switched to SDR\n");
						enable_dolby_vision(0);
						sdr_delay = 0;
					} else {
						sdr_delay++;
					}
				}
				/* if tv ko loaded for tm2 stb*/
				/* switch to tv vore */
				if ((is_meson_tm2_stbmode() ||
				     is_meson_t7_stbmode()) &&
					p_funcs_tv && !p_funcs_stb &&
					!is_dolby_vision_on()) {
					pr_dolby_dbg("Switch from STB to TV core Done\n");
					tv_mode = 1;
					support_info |= 1 << 3;
					dolby_vision_run_mode_delay =
						RUN_MODE_DELAY;
				}
			} else {
				enable_dolby_vision(0);
			}
		}
		if (sdr_delay == 0)
			dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		update_dovi_core2_reg();
		return 0;
	} else if (sdr_delay != 0) {
		/* in case mode change to a mode requiring dolby block */
		sdr_delay = 0;
	}

	if ((dolby_vision_flags & FLAG_CERTIFICAION) ||
	    (dolby_vision_flags & FLAG_BYPASS_VPP))
		video_effect_bypass(1);

	if (!p_funcs_stb && !p_funcs_tv) {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		tv_dovi_setting_change_flag = false;
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		update_dovi_core2_reg();
		return 0;
	}
	if ((debug_dolby & 2) && force_set &&
	    !(dolby_vision_flags & FLAG_CERTIFICAION))
		pr_dolby_dbg
			("core1 size changed--old: %d x %d, new: %d x %d\n",
			 core1_disp_hsize, core1_disp_vsize,
			 h_size, v_size);
	if (dolby_vision_core1_on &&
	    dolby_vision_core1_on_cnt < DV_CORE1_RECONFIG_CNT &&
	    !(dolby_vision_flags & FLAG_TOGGLE_FRAME) &&
	    !is_meson_tvmode()) {
		dolby_vision_set_toggle_flag(1);
		if (!(dolby_vision_flags & FLAG_CERTIFICAION))
			pr_dolby_dbg
				("Need update core1 setting first %d times, force toggle frame\n",
				dolby_vision_core1_on_cnt);
	}
	if (dolby_vision_on && !dolby_vision_core1_on &&
	    dolby_vision_core2_on_cnt &&
	    dolby_vision_core2_on_cnt < DV_CORE2_RECONFIG_CNT &&
	    !(dolby_vision_flags & FLAG_TOGGLE_FRAME) &&
	    !is_meson_tvmode() &&
	    !(dolby_vision_flags & FLAG_CERTIFICAION)) {
		force_set_lut = true;
		dolby_vision_set_toggle_flag(1);
		if (debug_dolby & 2)
			pr_dolby_dbg("Need update core2 first %d times\n",
				dolby_vision_core2_on_cnt);
	}
	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (!(dolby_vision_flags & FLAG_CERTIFICAION))
			reset_flag =
				(dolby_vision_reset & 1) &&
				(!dolby_vision_core1_on) &&
				(dolby_vision_on_count == 0);
		if (is_meson_tvmode()) {
			if (tv_dovi_setting_change_flag || force_set) {
				if (vf && (vf->type & VIDTYPE_VIU_422))
					src_chroma_format = 2;
				else if (vf)
					src_chroma_format = 1;
				/* mark by brian.zhu 2021.4.27 */
#ifdef NEED_REMOVE
				if (force_set &&
				    !(dolby_vision_flags & FLAG_CERTIFICAION))
					reset_flag = true;
#endif
				if ((dolby_vision_flags & FLAG_CERTIFICAION)) {
					if (tv_dovi_setting->src_format ==
						FORMAT_HDR10 ||
						tv_dovi_setting->src_format ==
						FORMAT_HLG ||
						tv_dovi_setting->src_format ==
						FORMAT_SDR)
						src_is_42210bit = true;
				} else {
					if (tv_dovi_setting->src_format ==
					    FORMAT_HDR10 ||
					    tv_dovi_setting->src_format ==
					    FORMAT_HLG)
						src_is_42210bit = true;
				}
				tv_dolby_core1_set
					(tv_dovi_setting->core1_reg_lut,
					 h_size,
					 v_size,
					 dovi_setting_video_flag,/*BL enable*/
					 dovi_setting_video_flag &&
					 tv_dovi_setting->el_flag && !mel_mode,
					 tv_dovi_setting->el_halfsize_flag,
					 src_chroma_format,
					 tv_dovi_setting->input_mode ==
					 IN_MODE_HDMI,
					 src_is_42210bit,
					 reset_flag
				);
				if (!h_size || !v_size)
					dovi_setting_video_flag = false;
				if (dovi_setting_video_flag &&
				    dolby_vision_on_count == 0)
					pr_dolby_dbg("first frame reset %d\n",
						     reset_flag);
				enable_dolby_vision(1);
				if (tv_dovi_setting->backlight !=
				    tv_backlight ||
				    (dovi_setting_video_flag &&
				    dolby_vision_on_count == 0) ||
				    tv_backlight_force_update) {
					pr_dolby_dbg("backlight %d -> %d\n",
						tv_backlight,
						tv_dovi_setting->backlight);
					tv_backlight =
						tv_dovi_setting->backlight;
					tv_backlight_changed = true;
					bl_delay_cnt = 0;
					tv_backlight_force_update = false;
				}
				tv_dovi_setting_change_flag = false;
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				update_dolby_vision_status
					(tv_dovi_setting->src_format);
			}
		} else {
			if (((new_dovi_setting.video_width & 0xffff) &&
			    (new_dovi_setting.video_height & 0xffff)) ||
			    force_set_lut) {
				if (new_dovi_setting.video_width == 0xffff)
					new_dovi_setting.video_width = 0;
				if (new_dovi_setting.video_height == 0xffff)
					new_dovi_setting.video_height = 0;
				if (vf && (vf->type & VIDTYPE_VIU_422))
					src_chroma_format = 2;
				else if (vf)
					src_chroma_format = 1;

				if (is_meson_stb_hdmimode()) {
					core_mask = 0x6;
					tv_dolby_core1_set
						(tv_dovi_setting->core1_reg_lut,
						new_dovi_setting.video_width,
						new_dovi_setting.video_height,
						dovi_setting_video_flag,
						false,
						false,
						2,
						true,
						false,
						reset_flag
					);
				}

				if (force_set &&
					!(dolby_vision_flags
					& FLAG_CERTIFICAION))
					reset_flag = true;
				apply_stb_core_settings
					(dovi_setting_video_flag,
					 dolby_vision_mask & core_mask,
					 reset_flag,
					 (new_dovi_setting.video_width << 16)
					 | new_dovi_setting.video_height,
					 pps_state);
				memcpy(&dovi_setting, &new_dovi_setting,
				       sizeof(dovi_setting));
				if (core1_disp_hsize !=
					dovi_setting.video_width ||
					core1_disp_vsize !=
					dovi_setting.video_height)
					if (core1_disp_hsize &&
					    core1_disp_vsize)
						pr_dolby_dbg
						("frame size %d %d->%d %d\n",
						 core1_disp_hsize,
						 core1_disp_vsize,
						 dovi_setting.video_width,
						 dovi_setting.video_height);
				new_dovi_setting.video_width =
				new_dovi_setting.video_height = 0;
				if (!dovi_setting.video_width ||
				    !dovi_setting.video_height)
					dovi_setting_video_flag = false;
				if (dovi_setting_video_flag &&
				    dolby_vision_on_count == 0)
					pr_dolby_dbg("first frame reset %d\n",
						     reset_flag);
				/* clr hdr+ pkt when enable dv */
				if (!dolby_vision_on &&
				    vinfo && vinfo->vout_device) {
					p_vout = vinfo->vout_device;
					if (p_vout->fresh_tx_hdr10plus_pkt)
						p_vout->fresh_tx_hdr10plus_pkt
							(0, NULL);
				}
				enable_dolby_vision(1);
				bypass_pps_path(pps_state);
				core1_disp_hsize =
					dovi_setting.video_width;
				core1_disp_vsize =
					dovi_setting.video_height;
				/* send HDMI packet according to dst_format */
				if (vinfo)
					send_hdmi_pkt
						(dovi_setting.src_format,
						 dovi_setting.dst_format,
						 vinfo, vf);
				update_dolby_vision_status
					(dovi_setting.src_format);
			} else {
				if ((get_video_mute() == VIDEO_MUTE_ON_DV &&
				     !(dolby_vision_flags & FLAG_MUTE)) ||
				    (get_video_mute() == VIDEO_MUTE_OFF &&
				     dolby_vision_flags & FLAG_MUTE) ||
				     last_dolby_vision_ll_policy !=
				     dolby_vision_ll_policy)
					/* core 3 only */
					apply_stb_core_settings
					(dovi_setting_video_flag,
					 dolby_vision_mask & 0x4,
					 reset_flag,
					 (dovi_setting.video_width << 16)
					 | dovi_setting.video_height,
					 pps_state);
				/* force send hdmi pkt */
				if (dolby_vision_flags & FLAG_FORCE_HDMI_PKT) {
					if (vinfo)
						send_hdmi_pkt
						(dovi_setting.src_format,
						 dovi_setting.dst_format,
						 vinfo, vf);
				}
			}
		}
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	} else if (dolby_vision_core1_on &&
		!(dolby_vision_flags & FLAG_CERTIFICAION)) {
		bool reset_flag =
			(dolby_vision_reset & 2) &&
			(dolby_vision_on_count <=
			(dolby_vision_reset_delay >> 8)) &&
			(dolby_vision_on_count >=
			(dolby_vision_reset_delay & 0xff));
		if (is_meson_txlx_stbmode()) {
			if (dolby_vision_on_count <=
				dolby_vision_run_mode_delay || force_set) {
				if (force_set)
					reset_flag = true;
				apply_stb_core_settings
					(dovi_setting_video_flag,
					 /* core 1 only */
					 dolby_vision_mask & 0x1,
					 reset_flag,
					 (h_size << 16) | v_size,
					 pps_state);
				bypass_pps_path(pps_state);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (dolby_vision_on_count <=
					dolby_vision_run_mode_delay)
					pr_dolby_dbg("fake frame %d reset %d\n",
						     dolby_vision_on_count,
						     reset_flag);
			}
		} else if (is_meson_tvmode()) {
			if (dolby_vision_on_count <=
				dolby_vision_run_mode_delay + 1 || force_set) {
				if (force_set)
					reset_flag = true;
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					if (tv_dovi_setting->src_format ==
					FORMAT_HDR10 ||
					tv_dovi_setting->src_format ==
					FORMAT_HLG ||
					tv_dovi_setting->src_format ==
					FORMAT_SDR)
						src_is_42210bit = true;
				} else {
					if (tv_dovi_setting->src_format ==
					    FORMAT_HDR10 ||
					    tv_dovi_setting->src_format ==
					    FORMAT_HLG)
						src_is_42210bit = true;
				}
				tv_dolby_core1_set
					(tv_dovi_setting->core1_reg_lut,
					 h_size,
					 v_size,
					 dovi_setting_video_flag,/* BL enable */
					 dovi_setting_video_flag &&
					 tv_dovi_setting->el_flag && !mel_mode,/*ELenable*/
					 tv_dovi_setting->el_halfsize_flag,
					 src_chroma_format,
					 tv_dovi_setting->input_mode ==
					 IN_MODE_HDMI,
					 src_is_42210bit,
					 reset_flag);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (dolby_vision_on_count <=
					dolby_vision_run_mode_delay + 1)
					pr_dolby_dbg("fake frame %d reset %d\n",
						     dolby_vision_on_count,
						     reset_flag);
			}
		} else if (is_meson_box() || is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
			if (dolby_vision_on_count <=
				dolby_vision_run_mode_delay || force_set) {
				if (force_set)
					reset_flag = true;
				if (is_meson_stb_hdmimode())
					tv_dolby_core1_set
						(tv_dovi_setting->core1_reg_lut,
						core1_disp_hsize,
						core1_disp_vsize,
						dovi_setting_video_flag,
						false,
						false,
						2,
						true,
						false,
						reset_flag
					);
				else
					apply_stb_core_settings
						(true, /* always enable */
						 /* core 1 only */
						 dolby_vision_mask & 0x1,
						 reset_flag,
						 (core1_disp_hsize << 16)
						 | core1_disp_vsize,
						 pps_state);
				if (dolby_vision_on_count <
					dolby_vision_run_mode_delay)
					pr_dolby_dbg("fake frame (%d %d) %d reset %d\n",
						     core1_disp_hsize,
						     core1_disp_vsize,
						     dolby_vision_on_count,
						     reset_flag);
			}
		}
	}
	if (dolby_vision_core1_on) {
		if (dolby_vision_on_count <=
			dolby_vision_run_mode_delay + 1)
			dolby_vision_on_count++;
		if (debug_dolby & 8)
			pr_dolby_dbg("%s: dolby_vision_on_count %d\n",
				     __func__, dolby_vision_on_count);
	} else {
		dolby_vision_on_count = 0;
	}
	update_dovi_core2_reg();
	return 0;
}
EXPORT_SYMBOL(dolby_vision_process);

/* when dolby on in uboot, other module cannot get dolby status
 * in time through dolby_vision_on due to dolby_vision_on
 * is set in register_dv_functions
 * Add dolby_vision_on_in_uboot condition for this case.
 */
bool is_dolby_vision_on(void)
{
	return dolby_vision_on ||
		dolby_vision_wait_on ||
		dolby_vision_on_in_uboot;
}
EXPORT_SYMBOL(is_dolby_vision_on);

bool is_dolby_vision_video_on(void)
{
	return dolby_vision_core1_on;
}
EXPORT_SYMBOL(is_dolby_vision_video_on);

bool is_dolby_vision_graphic_on(void)
{
	/* TODO: check (core2_sel & 2) for core2c */
	return is_dolby_vision_on() && !tv_mode &&
		(dolby_vision_mask & 2) && (core2_sel & 1);
}
EXPORT_SYMBOL(is_dolby_vision_graphic_on);

bool for_dolby_vision_certification(void)
{
	return is_dolby_vision_on() &&
		dolby_vision_flags & FLAG_CERTIFICAION;
}
EXPORT_SYMBOL(for_dolby_vision_certification);

bool for_dolby_vision_video_effect(void)
{
	return is_dolby_vision_on() &&
		dolby_vision_flags & FLAG_BYPASS_VPP;
}
EXPORT_SYMBOL(for_dolby_vision_video_effect);

void dolby_vision_set_toggle_flag(int flag)
{
	if (flag) {
		dolby_vision_flags |= FLAG_TOGGLE_FRAME;
		if (flag & 2)
			dolby_vision_flags |= FLAG_FORCE_HDMI_PKT;
	} else {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	}
}
EXPORT_SYMBOL(dolby_vision_set_toggle_flag);

bool is_dv_control_backlight(void)
{
	return dv_control_backlight_status;
}
EXPORT_SYMBOL(is_dv_control_backlight);

void set_dolby_vision_mode(int mode)
{
	if ((is_meson_box() || is_meson_txlx() || is_meson_tm2() || is_meson_t7() ||
	    is_meson_t3() || is_meson_t5w()) &&
	    dolby_vision_enable && dolby_vision_request_mode == 0xff) {
		if (dolby_vision_policy_process(&mode, get_cur_src_format())) {
			dolby_vision_set_toggle_flag(1);
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			pr_info("DOVI output change from %d to %d\n",
				dolby_vision_mode, mode);
			dolby_vision_target_mode = mode;
			dolby_vision_mode = mode;
		}
	}
}
EXPORT_SYMBOL(set_dolby_vision_mode);

int get_dolby_vision_mode(void)
{
	return dolby_vision_mode;
}
EXPORT_SYMBOL(get_dolby_vision_mode);

int get_dolby_vision_target_mode(void)
{
	return dolby_vision_target_mode;
}
EXPORT_SYMBOL(get_dolby_vision_target_mode);

bool is_dolby_vision_enable(void)
{
	return dolby_vision_enable;
}
EXPORT_SYMBOL(is_dolby_vision_enable);

bool is_dolby_vision_stb_mode(void)
{
	return is_meson_txlx_stbmode() ||
		is_meson_tm2_stbmode() ||
		is_meson_t7_stbmode() ||
		is_meson_box();
}
EXPORT_SYMBOL(is_dolby_vision_stb_mode);

bool is_dolby_vision_el_disable(void)
{
	return dolby_vision_el_disable;
}
EXPORT_SYMBOL(is_dolby_vision_el_disable);

void set_dolby_vision_ll_policy(int policy)
{
	dolby_vision_ll_policy = policy;
}
EXPORT_SYMBOL(set_dolby_vision_ll_policy);

void set_dolby_vision_policy(int policy)
{
	dolby_vision_policy = policy;
}
EXPORT_SYMBOL(set_dolby_vision_policy);

int get_dolby_vision_policy(void)
{
	return dolby_vision_policy;
}
EXPORT_SYMBOL(get_dolby_vision_policy);

void set_dolby_vision_enable(bool enable)
{
	dolby_vision_enable = enable;
}
EXPORT_SYMBOL(set_dolby_vision_enable);

/* bit 0 for HDR10: 1=by dv, 0-by vpp */
/* bit 1 for HLG: 1=by dv, 0-by vpp */
int get_dolby_vision_hdr_policy(void)
{
	int ret = 0;

	if (!is_dolby_vision_enable())
		return 0;
	if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
		/* policy == FOLLOW_SRC, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC) ? 2 : 0;
	} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
		/* policy == FOLLOW_SINK, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK) ? 2 : 0;
	} else {
		/* policy == FORCE, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK) ? 2 : 0;
	}
	return ret;
}
EXPORT_SYMBOL(get_dolby_vision_hdr_policy);

void dolby_vision_update_backlight(void)
{
#ifdef CONFIG_AMLOGIC_LCD
	u32 new_bl = 0;

	if (is_meson_tvmode()) {
		if (!force_disable_dv_backlight) {
			bl_delay_cnt++;
			if (tv_backlight_changed &&
			    set_backlight_delay_vsync == bl_delay_cnt) {
				new_bl = use_12b_bl ? tv_backlight << 4 :
					tv_backlight;
				pr_dolby_dbg("dv set backlight %d\n", new_bl);
				aml_lcd_atomic_notifier_call_chain
					(LCD_EVENT_BACKLIGHT_GD_DIM,
					 &new_bl);
				tv_backlight_changed = false;
			}
		}
	}
#endif
}
EXPORT_SYMBOL(dolby_vision_update_backlight);

void dolby_vision_disable_backlight(void)
{
#ifdef CONFIG_AMLOGIC_LCD
	int gd_en = 0;

	pr_dolby_dbg("disable dv backlight\n");

	if (is_meson_tvmode()) {
		aml_lcd_atomic_notifier_call_chain
		(LCD_EVENT_BACKLIGHT_GD_SEL, &gd_en);
		dv_control_backlight_status = false;
	}
#endif
}

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	pr_info("arm_smccc_smc 0x%lx, get res.a0 0x%lx\n", function_id, res.a0);
	return res.a0;
}

static void parse_param_amdolby_vision(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
		if (n >= MAX_PARAM)
			break;
	}
}

static inline s16 clamps(s16 value, s16 v_min, s16 v_max)
{
	if (value > v_max)
		return v_max;
	if (value < v_min)
		return v_min;
	return value;
}

/*to do*/
static void update_vsvdb_to_rx(void)
{
	u8 *p = cfg_info[cur_pic_mode].vsvdb;

	if (memcmp(&current_vsvdb[0], p, sizeof(current_vsvdb))) {
		memcpy(&current_vsvdb[0], p, sizeof(current_vsvdb));
		pr_info("%s: %d %d %d %d %d %d %d\n",
			__func__, p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
	}
}

static void update_cp_cfg(void)
{
	unsigned long lockflags;
	struct target_config *tdc;

	if (cur_pic_mode >= num_picture_mode || num_picture_mode == 0 ||
	    !bin_to_cfg) {
		pr_info("%s, invalid para %d/%d, bin_to_cfg %p",
			__func__, cur_pic_mode, num_picture_mode, bin_to_cfg);
		return;
	}

	memcpy(pq_config_fake,
	       &bin_to_cfg[cur_pic_mode],
	       sizeof(struct pq_config));
	tdc = &(((struct pq_config *)pq_config_fake)->tdc);
	tdc->d_brightness = cfg_info[cur_pic_mode].brightness;
	tdc->d_contrast = cfg_info[cur_pic_mode].contrast;
	tdc->d_color_shift = cfg_info[cur_pic_mode].colorshift;
	tdc->d_saturation = cfg_info[cur_pic_mode].saturation;

	if (debug_tprimary) {
		tdc->t_primaries[0] = cur_debug_tprimary[0][0]; /*rx*/
		tdc->t_primaries[1] = cur_debug_tprimary[0][1]; /*ry*/
		tdc->t_primaries[2] = cur_debug_tprimary[1][0]; /*gx*/
		tdc->t_primaries[3] = cur_debug_tprimary[1][1]; /*gy*/
		tdc->t_primaries[4] = cur_debug_tprimary[2][0]; /*bx*/
		tdc->t_primaries[5] = cur_debug_tprimary[2][1]; /*by*/
	}

	spin_lock_irqsave(&cfg_lock, lockflags);
	need_update_cfg = true;
	spin_unlock_irqrestore(&cfg_lock, lockflags);
}

/*0: reset picture mode and reset pq for all picture mode*/
/*1: reset pq for all picture mode*/
/*2: reset pq for current picture mode */
static void restore_dv_pq_setting(enum pq_reset_e pq_reset)
{
	int mode = 0;

	if (pq_reset == RESET_ALL)
		cur_pic_mode = default_pic_mode;

	for (mode = 0; mode < num_picture_mode; mode++) {
		if (pq_reset == RESET_PQ_FOR_CUR && mode != cur_pic_mode)
			continue;
		cfg_info[mode].brightness =
			bin_to_cfg[mode].tdc.d_brightness;
		cfg_info[mode].contrast =
			bin_to_cfg[mode].tdc.d_contrast;
		cfg_info[mode].colorshift =
			bin_to_cfg[mode].tdc.d_color_shift;
		cfg_info[mode].saturation =
			bin_to_cfg[mode].tdc.d_saturation;
		cfg_info[mode].dark_detail =
			bin_to_cfg[mode].tdc.ambient_config.dark_detail;
		memcpy(cfg_info[mode].vsvdb,
		       bin_to_cfg[mode].tdc.vsvdb,
		       sizeof(cfg_info[mode].vsvdb));
	}
	cur_debug_tprimary[0][0] =
		bin_to_cfg[0].tdc.t_primaries[0];
	cur_debug_tprimary[0][1] =
		bin_to_cfg[0].tdc.t_primaries[1];
	cur_debug_tprimary[1][0] =
		bin_to_cfg[0].tdc.t_primaries[2];
	cur_debug_tprimary[1][1] =
		bin_to_cfg[0].tdc.t_primaries[3];
	cur_debug_tprimary[2][0] =
		bin_to_cfg[0].tdc.t_primaries[4];
	cur_debug_tprimary[2][1] =
		bin_to_cfg[0].tdc.t_primaries[5];

	update_cp_cfg();
	if (debug_dolby & 0x200)
		pr_info("reset pq %d\n", pq_reset);
}

static void set_dv_pq_center(void)
{
	int mode = 0;
#if USE_CENTER_0
	for (mode = 0; mode < num_picture_mode; mode++) {
		pq_center[mode][0] = 0;
		pq_center[mode][1] = 0;
		pq_center[mode][2] = 0;
		pq_center[mode][3] = 0;
	}
#else
	for (mode = 0; mode < num_picture_mode; mode++) {
		pq_center[mode][0] =
			bin_to_cfg[mode].tdc.d_brightness;
		pq_center[mode][1] =
			bin_to_cfg[mode].tdc.d_contrast;
		pq_center[mode][2] =
			bin_to_cfg[mode].tdc.d_color_shift;
		pq_center[mode][3] =
			bin_to_cfg[mode].tdc.d_saturation;
	}
#endif
}

static void set_dv_pq_range(void)
{
	pq_range[PQ_BRIGHTNESS].left = LEFT_RANGE_BRIGHTNESS;
	pq_range[PQ_BRIGHTNESS].right = RIGHT_RANGE_BRIGHTNESS;
	pq_range[PQ_CONTRAST].left = LEFT_RANGE_CONTRAST;
	pq_range[PQ_CONTRAST].right = RIGHT_RANGE_CONTRAST;
	pq_range[PQ_COLORSHIFT].left = LEFT_RANGE_COLORSHIFT;
	pq_range[PQ_COLORSHIFT].right = RIGHT_RANGE_COLORSHIFT;
	pq_range[PQ_SATURATION].left = LEFT_RANGE_SATURATION;
	pq_range[PQ_SATURATION].right = RIGHT_RANGE_SATURATION;
}

static void remove_comments(char *p_buf)
{
	if (!p_buf)
		return;
	while (*p_buf && *p_buf != '#' && *p_buf != '%')
		p_buf++;

	*p_buf = 0;
}

#define MAX_READ_SIZE 256
static char cur_line[MAX_READ_SIZE];

/*read one line from cfg, return eof flag*/
static bool get_one_line(char **cfg_buf, char *line_buf, bool ignore_comments)
{
	char *line_end;
	size_t line_len = 0;
	bool eof_flag = false;

	if (!cfg_buf || !(*cfg_buf) || !line_buf)
		return true;

	line_buf[0] = '\0';
	while (*line_buf == '\0') {
		if (debug_dolby & 0x2)
			pr_info("*cfg_buf: %s\n", *cfg_buf);
		line_end = strnchr(*cfg_buf, MAX_READ_SIZE, '\n');
		if (debug_dolby & 0x2)
			pr_info("line_end: %s\n", line_end);
		if (!line_end) {
			line_len = strlen(*cfg_buf);
			eof_flag = true;
		} else {
			line_len = (size_t)(line_end - *cfg_buf);
		}
		memcpy(line_buf, *cfg_buf, line_len);
		line_buf[line_len] = '\0';
		if (line_len > 0 && line_buf[line_len - 1] == '\r')
			line_buf[line_len - 1] = '\0';

		*cfg_buf = *cfg_buf + line_len + 1;
		if (ignore_comments)
			remove_comments(line_buf);
		while (isspace(*line_buf))
			line_buf++;

		if (**cfg_buf == '\0')
			eof_flag = true;

		if (eof_flag)
			break;
	}
	if (debug_dolby & 0x200)
		pr_info("get line: %s\n", line_buf);
	return eof_flag;
}

static void get_picture_mode_info(char *cfg_buf)
{
	char *ptr_line;
	char name[32];
	int picmode_count = 0;
	int ret = 0;
	bool eof_flag = false;
	char *p_cfg_buf = cfg_buf;
	s32 bri_off_1 = 0;
	s32 bri_off_2 = 0;

	while (!eof_flag) {
		eof_flag = get_one_line(&cfg_buf, (char *)&cur_line, true);
		ptr_line = cur_line;
		if (eof_flag && (strlen(cur_line) == 0))
			break;
		if ((strncmp(ptr_line, "PictureModeName",
			strlen("PictureModeName")) == 0)) {
			ret = sscanf(ptr_line, "PictureModeName = %s", name);
			if (ret == 1) {
				cfg_info[picmode_count].id =
					picmode_count;
				strcpy(cfg_info[picmode_count].pic_mode_name,
					name);
				if (debug_dolby & 0x200)
					pr_info("update cfg_info[%d]: %s\n",
					picmode_count, name);
				picmode_count++;
				if (picmode_count >= num_picture_mode)
					break;
			}
		}
	}

	eof_flag = false;
	while (!eof_flag) {
		eof_flag = get_one_line(&p_cfg_buf, (char *)&cur_line, false);
		ptr_line = cur_line;
		if (eof_flag && (strlen(cur_line) == 0))
			break;
		if ((strncmp(ptr_line, "#Amlogic_inside DV OTT",
			strlen("#Amlogic_inside DV OTT")) == 0)) {
			ret = sscanf(ptr_line, "#Amlogic_inside DV OTT{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[0][0] = bri_off_1;
				brightness_off[0][1] = bri_off_2;
				pr_info("update DV OTT bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside Sink-led",
			strlen("#Amlogic_inside Sink-led")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside Sink-led{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[1][0] = bri_off_1;
				brightness_off[1][1] = bri_off_2;
				pr_info("update Sink-led bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside Source-led",
			strlen("#Amlogic_inside Source-led")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside Source-led{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[2][0] = bri_off_1;
				brightness_off[2][1] = bri_off_2;
				pr_info("update Source-led bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside HDR OTT",
			strlen("#Amlogic_inside HDR OTT")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside HDR OTT{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[3][0] = bri_off_1;
				brightness_off[3][1] = bri_off_2;
				pr_info("update HDR OTT bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside HLG OTT",
			strlen("#Amlogic_inside HLG OTT")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside HLG OTT{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[4][0] = bri_off_1;
				brightness_off[4][1] = bri_off_2;
				pr_info("update HLG OTT bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		}
	}
}

static bool load_dv_pq_config_data(char *bin_path, char *txt_path)
{
	struct file *filp = NULL;
	struct file *filp_txt = NULL;
	bool ret = true;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	struct kstat stat;
	unsigned int length = 0;
	unsigned int cfg_len = sizeof(struct pq_config) * MAX_DV_PICTUREMODES;
	char *txt_buf = NULL;
	int i = 0;
	int error;

	if (!module_installed)
		return false;

	if (!bin_to_cfg)
		bin_to_cfg = vmalloc(cfg_len);
	if (!bin_to_cfg)
		return false;

	set_fs(KERNEL_DS);
	filp = filp_open(bin_path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		ret = false;
		pr_info("[%s] failed to open file: |%s|\n", __func__, bin_path);
		goto LOAD_END;
	}

	error = vfs_stat(bin_path, &stat);
	if (error < 0) {
		ret = false;
		filp_close(filp, NULL);
		goto LOAD_END;
	}
	length = (stat.size > cfg_len) ? cfg_len : stat.size;
	num_picture_mode = length / sizeof(struct pq_config);

	if (num_picture_mode >
	    sizeof(cfg_info) / sizeof(struct dv_cfg_info_s)) {
		pr_info("dv_config.bin size(%d) bigger than cfg_info(%d)\n",
			num_picture_mode,
			(int)(sizeof(cfg_info) / sizeof(struct dv_cfg_info_s)));
		num_picture_mode =
			sizeof(cfg_info) / sizeof(struct dv_cfg_info_s);
	}
	if (num_picture_mode == 1)
		default_pic_mode = 0;
	vfs_read(filp, (char *)bin_to_cfg, length, &pos);

	for (i = 0; i < num_picture_mode; i++) {
		cfg_info[i].id = i;
		strcpy(cfg_info[i].pic_mode_name, "uninitialized");
	}

	/***read cfg txt to get picture mode name****/
	filp_txt = filp_open(txt_path, O_RDONLY, 0444);
	pos = 0;
	if (IS_ERR(filp_txt)) {
		ret = false;
		pr_info("[%s] failed to open file: |%s|\n", __func__, txt_path);
		filp_close(filp, NULL);
		goto LOAD_END;
	} else {
		error = vfs_stat(txt_path, &stat);
		if (error < 0) {
			ret = false;
			filp_close(filp, NULL);
			filp_close(filp_txt, NULL);
			goto LOAD_END;
		}

		length = stat.size;
		txt_buf = vmalloc(length + 2);
		memset(txt_buf, 0, length + 2);
		if (txt_buf) {
			vfs_read(filp_txt, txt_buf, length, &pos);
			get_picture_mode_info(txt_buf);
		}
	}
	/*************read cfg txt done***********/

	set_dv_pq_center();
	set_dv_pq_range();
	restore_dv_pq_setting(RESET_ALL);
	update_vsvdb_to_rx();

	filp_close(filp, NULL);
	filp_close(filp_txt, NULL);

	use_target_lum_from_cfg = true;
	pr_info("DV config: load %d picture mode\n", num_picture_mode);

LOAD_END:
	if (txt_buf)
		vfree(txt_buf);
	set_fs(old_fs);
	return ret;
}

/*internal range -> external range*/
static s16 map_pq_inter_to_exter
	(enum pq_item_e pq_item,
	 s16 inter_value)
{
	s16 inter_pq_min;
	s16 inter_pq_max;
	s16 exter_pq_min;
	s16 exter_pq_max;
	s16 exter_value;
	s16 inter_range;
	s16 exter_range;
	s16 tmp;
	s16 left_range = pq_range[pq_item].left;
	s16 right_range = pq_range[pq_item].right;
	s16 center = pq_center[cur_pic_mode][pq_item];

	if (inter_value >= center) {
		exter_pq_min = 0;
		exter_pq_max = EXTER_MAX_PQ;
		inter_pq_min = center;
		inter_pq_max = right_range;
	} else {
		exter_pq_min = EXTER_MIN_PQ;
		exter_pq_max = 0;
		inter_pq_min = left_range;
		inter_pq_max = center;
	}
	inter_pq_min = clamps(inter_pq_min, INTER_MIN_PQ, INTER_MAX_PQ);
	inter_pq_max = clamps(inter_pq_max, INTER_MIN_PQ, INTER_MAX_PQ);
	tmp = clamps(inter_value, inter_pq_min, inter_pq_max);

	inter_range = inter_pq_max - inter_pq_min;
	exter_range = exter_pq_max - exter_pq_min;

	if (inter_range == 0) {
		inter_range = INTER_MAX_PQ;
		pr_info("invalid inter_range, set to INTER_MAX_PQ\n");
	}

	exter_value = exter_pq_min +
		(tmp - inter_pq_min) * exter_range / inter_range;

	if (debug_dolby & 0x200)
		pr_info("%s: %s [%d]->[%d]\n",
			__func__, pq_item_str[pq_item],
			inter_value, exter_value);
	return exter_value;
}

/*external range -> internal range*/
static s16 map_pq_exter_to_inter
	(enum pq_item_e pq_item,
	 s16 exter_value)
{
	s16 inter_pq_min;
	s16 inter_pq_max;
	s16 exter_pq_min;
	s16 exter_pq_max;
	s16 inter_value;
	s16 inter_range;
	s16 exter_range;
	s16 tmp;
	s16 left_range = pq_range[pq_item].left;
	s16 right_range = pq_range[pq_item].right;
	s16 center = pq_center[cur_pic_mode][pq_item];

	if (exter_value >= 0) {
		exter_pq_min = 0;
		exter_pq_max = EXTER_MAX_PQ;
		inter_pq_min = center;
		inter_pq_max = right_range;
	} else {
		exter_pq_min = EXTER_MIN_PQ;
		exter_pq_max = 0;
		inter_pq_min = left_range;
		inter_pq_max = center;
	}

	inter_pq_min = clamps(inter_pq_min, INTER_MIN_PQ, INTER_MAX_PQ);
	inter_pq_max = clamps(inter_pq_max, INTER_MIN_PQ, INTER_MAX_PQ);
	tmp = clamps(exter_value, exter_pq_min, exter_pq_max);

	inter_range = inter_pq_max - inter_pq_min;
	exter_range = exter_pq_max - exter_pq_min;

	inter_value = inter_pq_min +
		(tmp - exter_pq_min) * inter_range / exter_range;

	if (debug_dolby & 0x200)
		pr_info("%s: %s [%d]->[%d]\n",
			__func__, pq_item_str[pq_item],
			exter_value, inter_value);
	return inter_value;
}

static bool is_valid_pic_mode(int mode)
{
	if (mode < num_picture_mode && mode >= 0)
		return true;
	else
		return false;
}

static bool is_valid_pq_exter_value(s16 exter_value)
{
	if (exter_value <= EXTER_MAX_PQ && exter_value >= EXTER_MIN_PQ)
		return true;

	pr_info("pq %d is out of range[%d, %d]\n",
		exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
		return false;
}

static bool is_valid_pq_inter_value(s16 exter_value)
{
	if (exter_value <= INTER_MAX_PQ && exter_value >= INTER_MIN_PQ)
		return true;

	pr_info("pq %d is out of range[%d, %d]\n",
		exter_value, INTER_MIN_PQ, INTER_MAX_PQ);
	return false;
}

static void set_pic_mode(int mode)
{
	if (cur_pic_mode != mode) {
		cur_pic_mode = mode;
		update_cp_cfg();
		update_vsvdb_to_rx();
		update_pwm_control();
	}
}

static int get_pic_mode_num(void)
{
	return num_picture_mode;
}

static int get_pic_mode(void)
{
	return cur_pic_mode;
}

static char *get_pic_mode_name(int mode)
{
	if (!is_valid_pic_mode(mode))
		return "errmode";

	return cfg_info[mode].pic_mode_name;
}

static s16 get_single_pq_value(int mode, enum pq_item_e item)
{
	s16 exter_value = 0;
	s16 inter_value = 0;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return exter_value;
	}
	switch (item) {
	case PQ_BRIGHTNESS:
		inter_value = cfg_info[mode].brightness;
		break;
	case PQ_CONTRAST:
		inter_value = cfg_info[mode].contrast;
		break;
	case PQ_COLORSHIFT:
		inter_value = cfg_info[mode].colorshift;
		break;
	case PQ_SATURATION:
		inter_value = cfg_info[mode].saturation;
		break;
	default:
		pr_err("err pq_item %d\n", item);
		break;
	}

	exter_value = map_pq_inter_to_exter(item, inter_value);

	if (debug_dolby & 0x200)
		pr_info("%s: mode:%d, item:%s, [inter: %d, exter: %d]\n",
			__func__, mode, pq_item_str[item],
			inter_value, exter_value);
	return exter_value;
}

static struct dv_full_pq_info_s get_full_pq_value(int mode)
{
	s16 exter_value = 0;
	struct dv_full_pq_info_s full_pq_info = {mode, 0, 0, 0, 0};

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return full_pq_info;
	}

	exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS,
					    cfg_info[mode].brightness);
	full_pq_info.brightness = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_CONTRAST,
					    cfg_info[mode].contrast);
	full_pq_info.contrast = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT,
					    cfg_info[mode].colorshift);
	full_pq_info.colorshift = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_SATURATION,
					    cfg_info[mode].saturation);
	full_pq_info.saturation = exter_value;

	if (debug_dolby & 0x200) {
		pr_info("----------%s: mode:%d-----------\n", __func__, mode);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_BRIGHTNESS],
			cfg_info[mode].brightness,
			full_pq_info.brightness);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_CONTRAST],
			cfg_info[mode].contrast,
			full_pq_info.contrast);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_COLORSHIFT],
			cfg_info[mode].colorshift,
			full_pq_info.colorshift);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_SATURATION],
			cfg_info[mode].saturation,
			full_pq_info.saturation);
	}

	return full_pq_info;
}

static void set_single_pq_value(int mode, enum pq_item_e item, s16 value)
{
	s16 inter_value = 0;
	s16 exter_value = value;
	bool pq_changed = false;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return;
	}
	if (!set_inter_pq) {
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps %s to %d\n",
				pq_item_str[item], exter_value);
		}
		inter_value = map_pq_exter_to_inter(item, exter_value);
		if (debug_dolby & 0x200) {
			pr_info("%s: mode:%d, item:%s, [inter:%d, exter:%d]\n",
				__func__, mode, pq_item_str[item],
				inter_value, exter_value);
		}
	} else {
		inter_value = value;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps %s to %d\n",
				pq_item_str[item], inter_value);
		}
		if (debug_dolby & 0x200) {
			exter_value = map_pq_inter_to_exter(item, inter_value);
			pr_info("%s: mode:%d, item:%s, [inter:%d, exter:%d]\n",
				__func__, mode, pq_item_str[item],
				inter_value, exter_value);
		}
	}
	switch (item) {
	case PQ_BRIGHTNESS:
		if (cfg_info[cur_pic_mode].brightness != inter_value) {
			cfg_info[cur_pic_mode].brightness = inter_value;
			pq_changed = true;
		}
		break;
	case PQ_CONTRAST:
		if (cfg_info[cur_pic_mode].contrast != inter_value) {
			cfg_info[cur_pic_mode].contrast = inter_value;
			pq_changed = true;
		}
		break;
	case PQ_COLORSHIFT:
		if (cfg_info[cur_pic_mode].colorshift != inter_value) {
			cfg_info[cur_pic_mode].colorshift = inter_value;
			pq_changed = true;
		}
		break;
	case PQ_SATURATION:
		if (cfg_info[cur_pic_mode].saturation != inter_value) {
			cfg_info[cur_pic_mode].saturation = inter_value;
			pq_changed = true;
		}
		break;
	default:
		pr_err("err pq_item %d\n", item);
		break;
	}
	if (pq_changed)
		update_cp_cfg();
}

static void set_full_pq_value(struct dv_full_pq_info_s full_pq_info)
{
	s16 inter_value = 0;
	s16 exter_value = 0;
	int mode = full_pq_info.pic_mode_id;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return;
	}
	/*------------set brightness----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.brightness;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps brightness from %d to %d\n",
				full_pq_info.brightness, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_BRIGHTNESS, exter_value);
	} else {
		inter_value = full_pq_info.brightness;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps brightness from %d to %d\n",
				full_pq_info.brightness, inter_value);
		}
	}
	cfg_info[cur_pic_mode].brightness = inter_value;

	/*-------------set contrast-----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.contrast;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps contrast from %d to %d\n",
				full_pq_info.contrast, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_CONTRAST, exter_value);
	} else {
		inter_value = full_pq_info.contrast;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps contrast from %d to %d\n",
				full_pq_info.contrast, inter_value);
		}
	}
	cfg_info[cur_pic_mode].contrast = inter_value;

	/*-------------set colorshift-----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.colorshift;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps colorshift from %d to %d\n",
				full_pq_info.colorshift, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_COLORSHIFT, exter_value);
	} else {
		inter_value = full_pq_info.colorshift;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps colorshift from %d to %d\n",
				full_pq_info.colorshift, inter_value);
		}
	}
	cfg_info[cur_pic_mode].colorshift = inter_value;

	/*-------------set colorshift-----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.saturation;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps saturation from %d to %d\n",
				full_pq_info.saturation, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_SATURATION, exter_value);
	} else {
		inter_value = full_pq_info.saturation;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps saturation from %d to %d\n",
				full_pq_info.saturation, inter_value);
		}
	}
	cfg_info[cur_pic_mode].saturation = inter_value;

	update_cp_cfg();

	if (debug_dolby & 0x200) {
		pr_info("----------%s: mode:%d----------\n", __func__, mode);
		exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS,
						    cfg_info[mode].brightness);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_BRIGHTNESS],
			cfg_info[mode].brightness,
			exter_value);

		exter_value = map_pq_inter_to_exter(PQ_CONTRAST,
						    cfg_info[mode].contrast);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_CONTRAST],
			cfg_info[mode].contrast,
			exter_value);

		exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT,
						    cfg_info[mode].colorshift);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_COLORSHIFT],
			cfg_info[mode].colorshift,
			exter_value);

		exter_value = map_pq_inter_to_exter(PQ_SATURATION,
						    cfg_info[mode].saturation);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_SATURATION],
			cfg_info[mode].saturation,
			exter_value);
	}
}

static ssize_t amdolby_vision_set_inter_pq_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	return snprintf(buf, 40, "%d\n",
		set_inter_pq);
}

static ssize_t amdolby_vision_set_inter_pq_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)

{
	size_t r;

	pr_info("cmd: %s\n", buf);
	r = kstrtoint(buf, 0, &set_inter_pq);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t amdolby_vision_load_cfg_status_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	return snprintf(buf, 40, "%d\n",
		load_bin_config);
}

static ssize_t amdolby_vision_load_cfg_status_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("%s: cmd: %s\n", __func__, buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;
	load_bin_config = value;
	if (load_bin_config)
		update_cp_cfg();
	else
		pq_config_set_flag = false;
	return count;
}

static ssize_t  amdolby_vision_pq_info_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	int pos = 0;
	int exter_value = 0;
	int inter_value = 0;
	int i = 0;
	const char *pq_usage_str = {
	"\n"
	"------------------------- set pq usage ------------------------\n"
	"echo picture_mode value > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo brightness value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo contrast value     > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo colorshift value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo saturation value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo darkdetail value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo all v1 v2 v3 v4 v5 > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo reset value        > /sys/class/amdolby_vision/dv_pq_info;\n"
	"\n"
	"picture_mode range: [0, num_picture_mode-1]\n"
	"brightness   range: [-256, 256]\n"
	"contrast     range: [-256, 256]\n"
	"colorshift   range: [-256, 256]\n"
	"saturation   range: [-256, 256]\n"
	"darkdetail   range: [0, 1]\n"
	"reset            0: reset pict mode/all pq for all pict mode]\n"
	"                 1: reset pq for all picture mode]\n"
	"                 2: reset pq for current picture mode]\n"
	"---------------------------------------------------------------\n"
	};

	if (!load_bin_config) {
		pr_info("no load_bin_config\n");
		return 0;
	}

	pos += sprintf(buf + pos,
		       "\n==== show current picture mode info ====\n");

	pos += sprintf(buf + pos,
		       "num_picture_mode:          [%d]\n", num_picture_mode);
	for (i = 0; i < num_picture_mode; i++) {
		pos += sprintf(buf + pos,
			       "picture_mode   %d:          [%s]\n",
			       cfg_info[i].id, cfg_info[i].pic_mode_name);
	}
	pos += sprintf(buf + pos, "\n\n");
	pos += sprintf(buf + pos,
		       "current picture mode:      [%d]\n", cur_pic_mode);

	pos += sprintf(buf + pos,
		       "current picture mode name: [%s]\n",
		       cfg_info[cur_pic_mode].pic_mode_name);

	inter_value = cfg_info[cur_pic_mode].brightness;
	exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS, inter_value);
	pos += sprintf(buf + pos,
		       "current brightness:        [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].contrast;
	exter_value = map_pq_inter_to_exter(PQ_CONTRAST, inter_value);
	pos += sprintf(buf + pos,
		       "current contrast:          [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].colorshift;
	exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT, inter_value);
	pos += sprintf(buf + pos,
		       "current colorshift:        [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].saturation;
	exter_value = map_pq_inter_to_exter(PQ_SATURATION, inter_value);
	pos += sprintf(buf + pos,
		       "current saturation:        [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	pos += sprintf(buf + pos,
		       "current darkdetail:        [%d]\n",
		       cfg_info[cur_pic_mode].dark_detail);
	pos += sprintf(buf + pos, "========================================\n");

	pos += sprintf(buf + pos, "%s\n", pq_usage_str);

	return pos;
}

static ssize_t amdolby_vision_pq_info_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig;
	char *parm[MAX_PARAM] = {NULL};
	struct dv_full_pq_info_s full_pq_info;
	enum pq_reset_e pq_reset;
	int val = 0;
	int val1 = 0;
	int val2 = 0;
	int val3 = 0;
	int val4 = 0;
	int val5 = 0;
	int item = -1;

	if (!load_bin_config)
		return count;
	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);

	pr_info("%s: cmd: %s\n", __func__, buf_orig);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!parm[0] || !parm[1])
		goto ERR;

	if (!strcmp(parm[0], "picture_mode")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		if (is_valid_pic_mode(val))
			set_pic_mode(val);
		else
			pr_info("err picture_mode\n");
	} else if (!strcmp(parm[0], "brightness")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;

		item = PQ_BRIGHTNESS;
	} else if (!strcmp(parm[0], "contrast")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;

		item = PQ_CONTRAST;
	} else if (!strcmp(parm[0], "colorshift")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;

		item = PQ_COLORSHIFT;
	} else if (!strcmp(parm[0], "saturation")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		item = PQ_SATURATION;
	} else if (!strcmp(parm[0], "all")) {
		if (kstrtoint(parm[1], 10, &val1) != 0 ||
		    kstrtoint(parm[2], 10, &val2) != 0 ||
		    kstrtoint(parm[3], 10, &val3) != 0 ||
		    kstrtoint(parm[4], 10, &val4) != 0 ||
		    kstrtoint(parm[5], 10, &val5) != 0)
			goto ERR;
		full_pq_info.pic_mode_id = val1;
		full_pq_info.brightness = val2;
		full_pq_info.contrast = val3;
		full_pq_info.colorshift = val4;
		full_pq_info.saturation = val5;
		set_full_pq_value(full_pq_info);
	} else if (!strcmp(parm[0], "reset")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		pq_reset = val;
		restore_dv_pq_setting(pq_reset);
	} else if (!strcmp(parm[0], "darkdetail")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		if (debug_dolby & 0x200)
			pr_info("[DV]: set mode %d darkdetail %d\n",
				cur_pic_mode, val);
		val = val > 0 ? 1 : 0;
		if (val != cfg_info[cur_pic_mode].dark_detail) {
			need_update_cfg = true;
			cfg_info[cur_pic_mode].dark_detail = val;
		}
	} else {
		pr_info("unsupport cmd\n");
	}
	if (item != -1)
		set_single_pq_value(cur_pic_mode, item, val);
ERR:
	kfree(buf_orig);
	return count;
}

static ssize_t amdolby_vision_bin_config_show
	(struct class *cla,
	 struct class_attribute *attr,
	 char *buf)
{
	int mode = 0;
	struct pq_config *pq_config = NULL;
	struct target_config *config = NULL;

	if (!load_bin_config) {
		pr_info("no load_bin_config\n");
		return 0;
	}

	pr_info("There are %d picture modes\n", num_picture_mode);
	for (mode = 0; mode < num_picture_mode; mode++) {
		pr_info("================ Picture Mode: %s =================\n",
			cfg_info[mode].pic_mode_name);
		pq_config = &bin_to_cfg[mode];
		config = &pq_config->tdc;
		pr_info("gamma:                %d\n",
			config->gamma);
		pr_info("eotf:                 %d-%s\n",
			config->eotf,
			eotf_str[config->eotf]);
		pr_info("range_spec:           %d\n",
			config->range_spec);
		pr_info("max_pq:               %d\n",
			config->max_pq);
		pr_info("min_pq:               %d\n",
			config->min_pq);
		pr_info("max_pq_dm3:           %d\n",
			config->max_pq_dm3);
		pr_info("min_lin:              %d\n",
			config->min_lin);
		pr_info("max_lin:              %d\n",
			config->max_lin);
		pr_info("max_lin_dm3:          %d\n",
			config->max_lin_dm3);
		pr_info("tPrimaries:           %d,%d,%d,%d,%d,%d,%d,%d\n",
			config->t_primaries[0],
			config->t_primaries[1],
			config->t_primaries[2],
			config->t_primaries[3],
			config->t_primaries[4],
			config->t_primaries[5],
			config->t_primaries[6],
			config->t_primaries[7]);
		pr_info("m_sweight:             %d\n",
			config->m_sweight);
		pr_info("trim_slope_bias:       %d\n",
			config->trim_slope_bias);
		pr_info("trim_offset_bias:      %d\n",
			config->trim_offset_bias);
		pr_info("trim_power_bias:       %d\n",
			config->trim_power_bias);
		pr_info("ms_weight_bias:        %d\n",
			config->ms_weight_bias);
		pr_info("chroma_weight_bias:    %d\n",
			config->chroma_weight_bias);
		pr_info("saturation_gain_bias:  %d\n",
			config->saturation_gain_bias);
		pr_info("d_brightness:          %d\n",
			config->d_brightness);
		pr_info("d_contrast:            %d\n",
			config->d_contrast);
		pr_info("d_color_shift:         %d\n",
			config->d_color_shift);
		pr_info("d_saturation:          %d\n",
			config->d_saturation);
		pr_info("d_backlight:           %d\n",
			config->d_backlight);
		pr_info("dbg_exec_paramsprint:  %d\n",
			config->dbg_exec_params_print_period);
		pr_info("dbg_dm_md_print_period:%d\n",
			config->dbg_dm_md_print_period);
		pr_info("dbg_dmcfg_print_period:%d\n",
			config->dbg_dm_cfg_print_period);
		pr_info("\n");
		pr_info("------ Global Dimming configuration ------\n");
		pr_info("gd_enable:            %d\n",
			config->gd_config.gd_enable);
		pr_info("gd_wmin:              %d\n",
			config->gd_config.gd_wmin);
		pr_info("gd_wmax:              %d\n",
			config->gd_config.gd_wmax);
		pr_info("gd_wmm:               %d\n",
			config->gd_config.gd_wmm);
		pr_info("gd_wdyn_rng_sqrt:     %d\n",
			config->gd_config.gd_wdyn_rng_sqrt);
		pr_info("gd_weight_mean:       %d\n",
			config->gd_config.gd_weight_mean);
		pr_info("gd_weight_std:        %d\n",
			config->gd_config.gd_weight_std);
		pr_info("gd_delay_ms_hdmi:     %d\n",
			config->gd_config.gd_delay_msec_hdmi);
		pr_info("gd_rgb2yuv_ext:       %d\n",
			config->gd_config.gd_rgb2yuv_ext);
		pr_info("gd_wdyn_rng_sqrt:     %d\n",
			config->gd_config.gd_wdyn_rng_sqrt);
		pr_info("gd_m33_rgb2yuv:       %d,%d,%d\n",
			config->gd_config.gd_m33_rgb2yuv[0][0],
			config->gd_config.gd_m33_rgb2yuv[0][1],
			config->gd_config.gd_m33_rgb2yuv[0][2]);
		pr_info("                      %d,%d,%d\n",
			config->gd_config.gd_m33_rgb2yuv[1][0],
			config->gd_config.gd_m33_rgb2yuv[1][1],
			config->gd_config.gd_m33_rgb2yuv[1][2]);
		pr_info("                      %d,%d,%d\n",
			config->gd_config.gd_m33_rgb2yuv[2][0],
			config->gd_config.gd_m33_rgb2yuv[2][1],
			config->gd_config.gd_m33_rgb2yuv[2][2]);
		pr_info("gd_m33_rgb2yuv_scale2p:%d\n",
			config->gd_config.gd_m33_rgb2yuv_scale2p);
		pr_info("gd_rgb2yuv_off_ext:    %d\n",
			config->gd_config.gd_rgb2yuv_off_ext);
		pr_info("gd_rgb2yuv_off:        %d,%d,%d\n",
			config->gd_config.gd_rgb2yuv_off[0],
			config->gd_config.gd_rgb2yuv_off[1],
			config->gd_config.gd_rgb2yuv_off[2]);
		pr_info("gd_up_bound:           %d\n",
			config->gd_config.gd_up_bound);
		pr_info("gd_low_bound:          %d\n",
			config->gd_config.gd_low_bound);
		pr_info("last_max_pq:           %d\n",
			config->gd_config.last_max_pq);
		pr_info("gd_wmin_pq:            %d\n",
			config->gd_config.gd_wmin_pq);
		pr_info("gd_wmax_pq:            %d\n",
			config->gd_config.gd_wmax_pq);
		pr_info("gd_wm_pq:              %d\n",
			config->gd_config.gd_wm_pq);
		pr_info("gd_trigger_period:     %d\n",
			config->gd_config.gd_trigger_period);
		pr_info("gd_trigger_lin_thresh: %d\n",
			config->gd_config.gd_trigger_lin_thresh);
		pr_info("gdDelayMilliSec_ott:   %d\n",
			config->gd_config.gd_delay_msec_ott);
		pr_info("gd_rise_weight:        %d\n",
			config->gd_config.gd_rise_weight);
		pr_info("gd_fall_weight:        %d\n",
			config->gd_config.gd_fall_weight);
		pr_info("gd_delay_msec_ll:      %d\n",
			config->gd_config.gd_delay_msec_ll);
		pr_info("gd_contrast:           %d\n",
			config->gd_config.gd_contrast);
		pr_info("\n");

		pr_info("------ Adaptive boost configuration ------\n");
		pr_info("ab_enable:             %d\n",
			config->ab_config.ab_enable);
		pr_info("ab_highest_tmax:       %d\n",
			config->ab_config.ab_highest_tmax);
		pr_info("ab_lowest_tmax:        %d\n",
			config->ab_config.ab_lowest_tmax);
		pr_info("ab_rise_weight:        %d\n",
			config->ab_config.ab_rise_weight);
		pr_info("ab_fall_weight:        %d\n",
			config->ab_config.ab_fall_weight);
		pr_info("ab_delay_msec_hdmi:    %d\n",
			config->ab_config.ab_delay_msec_hdmi);
		pr_info("ab_delay_msec_ott:     %d\n",
			config->ab_config.ab_delay_msec_ott);
		pr_info("ab_delay_msec_ll:      %d\n",
			config->ab_config.ab_delay_msec_ll);
		pr_info("\n");

		pr_info("------- Ambient light configuration ------\n");
		pr_info("dark_detail:        %d\n",
			config->ambient_config.dark_detail);
		pr_info("dark_detail_complum:%d\n",
			config->ambient_config.dark_detail_complum);
		pr_info("ambient:            %d\n",
			config->ambient_config.ambient);
		pr_info("t_front_lux:        %d\n",
			config->ambient_config.t_front_lux);
		pr_info("t_front_lux_scale:  %d\n",
			config->ambient_config.t_front_lux_scale);
		pr_info("t_rear_lum:         %d\n",
			config->ambient_config.t_rear_lum);
		pr_info("t_rear_lum_scale:   %d\n",
			config->ambient_config.t_rear_lum_scale);
		pr_info("t_whitexy:          %d, %d\n",
			config->ambient_config.t_whitexy[0],
			config->ambient_config.t_whitexy[1]);
		pr_info("t_surround_reflecti:%d\n",
			config->ambient_config.t_surround_reflection);
		pr_info("t_screen_reflection:%d\n",
			config->ambient_config.t_screen_reflection);
		pr_info("al_delay:           %d\n",
			config->ambient_config.al_delay);
		pr_info("al_rise:            %d\n",
			config->ambient_config.al_rise);
		pr_info("al_fall:            %d\n",
			config->ambient_config.al_fall);

		pr_info("vsvdb:              %x,%x,%x,%x,%x,%x,%x\n",
			config->vsvdb[0],
			config->vsvdb[1],
			config->vsvdb[2],
			config->vsvdb[3],
			config->vsvdb[4],
			config->vsvdb[5],
			config->vsvdb[6]);
		pr_info("\n");
		pr_info("---------------- ocsc_config --------------\n");
		pr_info("lms2rgb_mat:        %d,%d,%d\n",
			config->ocsc_config.lms2rgb_mat[0][0],
			config->ocsc_config.lms2rgb_mat[0][1],
			config->ocsc_config.lms2rgb_mat[0][2]);
		pr_info("                    %d,%d,%d\n",
			config->ocsc_config.lms2rgb_mat[1][0],
			config->ocsc_config.lms2rgb_mat[1][1],
			config->ocsc_config.lms2rgb_mat[1][2]);
		pr_info("                    %d,%d,%d\n",
			config->ocsc_config.lms2rgb_mat[2][0],
			config->ocsc_config.lms2rgb_mat[2][1],
			config->ocsc_config.lms2rgb_mat[2][2]);
		pr_info("lms2rgb_mat_scale:  %d\n",
			config->ocsc_config.lms2rgb_mat_scale);
		pr_info("\n");

		pr_info("dm31_avail:         %d\n",
			config->dm31_avail);
		pr_info("bright_preservation:%d\n",
			config->bright_preservation);
		pr_info("total_viewing_modes:%d\n",
			config->total_viewing_modes_num);
		pr_info("viewing_mode_valid: %d\n",
			config->viewing_mode_valid);
		pr_info("\n");
	}
	return 0;
}

static int get_chip_name(void)
{
	char *check = "chip_name = ";
	int check_len = 0;
	int name_len = 0;
	int total_name_len;

	if (is_meson_g12())
		snprintf(chip_name, sizeof("g12"), "%s", "g12");
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else if (is_meson_txlx())
		snprintf(chip_name, sizeof("txlx"), "%s", "txlx");
	else if (is_meson_gxm())
		snprintf(chip_name, sizeof("gxm"), "%s", "gxm");
#endif
	else if (is_meson_tm2())
		snprintf(chip_name, sizeof("tm2"), "%s", "tm2");
	else if (is_meson_sc2())
		snprintf(chip_name, sizeof("sc2"), "%s", "sc2");
	else if (is_meson_t7())
		snprintf(chip_name, sizeof("t7"), "%s", "t7");
	else if (is_meson_t3())
		snprintf(chip_name, sizeof("t3"), "%s", "t3");
	else if (is_meson_s4d())
		snprintf(chip_name, sizeof("s4d"), "%s", "s4d");
	else if (is_meson_t5w())
		snprintf(chip_name, sizeof("t5w"), "%s", "t5w");
	else
		snprintf(chip_name, sizeof("null"), "%s", "null");

	check_len = strlen(check);
	name_len = strlen(chip_name);

	total_name_len =
		check_len + name_len + 1;

	if (total_name_len < sizeof(total_chip_name))
		snprintf(total_chip_name,
			total_name_len,
			"%s%s",
			check,
			chip_name);
	return total_name_len;
}

bool chip_support_dv(void)
{
	if (is_meson_txlx() || is_meson_gxm() ||
	    is_meson_g12() || is_meson_tm2() ||
	    is_meson_sc2() || is_meson_t7() ||
	    is_meson_t3() || is_meson_s4d() ||
	    is_meson_t5w())
		return true;
	else
		return false;
}

int register_dv_functions(const struct dolby_vision_func_s *func)
{
	int ret = -1;
	unsigned int reg_clk;
	unsigned int reg_value;
	struct pq_config *pq_cfg;
	const struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int ko_info_len = 0;
	int total_name_len = 0;
	char *get_ko = NULL;
	u32 graphics_w = osd_graphic_width;
	u32 graphics_h = osd_graphic_height;

	if (dolby_vision_probe_ok == 0) {
		pr_info("error:(%s) dv probe fail cannot register\n", __func__);
		return -ENOMEM;
	}
	/*when dv ko load into kernel, this flag will be disabled
	 *otherwise it will effect hdr module
	 */
	if (dolby_vision_on_in_uboot) {
		if (is_vinfo_available(vinfo))
			is_sink_cap_changed(vinfo,
					    &current_hdr_cap,
					    &current_sink_available,
					    VPP_TOP0);
		else
			pr_info("sink not available\n");
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_on_in_uboot = 0;
	}

	if (!chip_support_dv()) {
		pr_info("chip not support dv\n");
		return ret;
	}
	if (is_meson_t7() || is_meson_t3() || is_meson_s4d() ||
	    is_meson_t5w()) {
		total_name_len = get_chip_name();
		get_ko = strstr(func->version_info, total_chip_name);

		if (!get_ko) {
			pr_info("error: dolby vision get fail ko, version: %s", func->version_info);
			return ret;
		}
	}

	if ((!p_funcs_stb || !p_funcs_tv) && func) {
		if (func->control_path && !p_funcs_stb) {
			pr_info("*** register_dv_stb_functions.***\n");
			if (!ko_info) {
				ko_info_len = strlen(func->version_info);
				ko_info = vmalloc(ko_info_len + 1);
				if (ko_info) {
					strncpy(ko_info, func->version_info, ko_info_len);
					ko_info[ko_info_len] = '\0';
				}
			}
			p_funcs_stb = func;
			if (is_meson_tm2() || is_meson_t7() || is_meson_t3()) {
				tv_dovi_setting =
				vmalloc(sizeof(struct tv_dovi_setting_s));
				if (!tv_dovi_setting)
					return -ENOMEM;
			}
		} else if (func->tv_control_path && !p_funcs_tv) {
			pr_info("*** register_dv_tv_functions\n");
			if (!ko_info) {
				ko_info_len = strlen(func->version_info);
				ko_info = vmalloc(ko_info_len + 1);
				if (ko_info) {
					strncpy(ko_info, func->version_info, ko_info_len);
					ko_info[ko_info_len] = '\0';
				}
			}
			p_funcs_tv = func;
			if (is_meson_txlx() || is_meson_tm2() ||
			    is_meson_t7() ||
			    is_meson_t3() || is_meson_t5w()) {
				pq_cfg =
					vmalloc(sizeof(struct pq_config));
				if (!pq_cfg)
					return -ENOMEM;
				pq_config_fake =
					(struct pq_config *)pq_cfg;
				tv_dovi_setting =
				vmalloc(sizeof(struct tv_dovi_setting_s));
				if (!tv_dovi_setting) {
					vfree(pq_config_fake);
					pq_config_fake = NULL;
					p_funcs_tv = NULL;
					return -ENOMEM;
				}
				tv_dovi_setting->src_format = FORMAT_SDR;

				tv_input_info =
				vmalloc(sizeof(struct tv_input_info_s));
				if (!tv_input_info) {
					vfree(pq_config_fake);
					pq_config_fake = NULL;
					p_funcs_tv = NULL;
					vfree(tv_dovi_setting);
					tv_dovi_setting = NULL;
					return -ENOMEM;
				}
				memset(tv_input_info, 0,
				       sizeof(struct tv_input_info_s));
			}
		} else {
			return ret;
		}
		ret = 0;
		/* get efuse flag*/

		if (is_meson_txlx() || is_meson_tm2() || is_meson_t7() ||
		    is_meson_t3() || is_meson_t5w()) {
			reg_clk = READ_VPP_DV_REG(DOLBY_TV_CLKGATE_CTRL);
			WRITE_VPP_DV_REG(DOLBY_TV_CLKGATE_CTRL, 0x2800);
			reg_value = READ_VPP_DV_REG(DOLBY_TV_REG_START + 1);
			if ((reg_value & 0x400) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
			WRITE_VPP_DV_REG(DOLBY_TV_CLKGATE_CTRL, reg_clk);
		} else {
			reg_value = READ_VPP_DV_REG(DOLBY_CORE1A_REG_START + 1);
			if ((reg_value & 0x100) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
		}
		pr_info("efuse_mode=%d reg_value = 0x%x\n",
			efuse_mode, reg_value);

		support_info = efuse_mode ? 0 : 1;/*bit0=1 => no efuse*/
		support_info = support_info | (1 << 1); /*bit1=1 => ko loaded*/
		support_info = support_info | (1 << 2); /*bit2=1 => updated*/
		if (tv_mode)
			support_info |= 1 << 3; /*bit3=1 => tv*/

		pr_info("dv capability %d\n", support_info);

		dovi_setting.src_format = FORMAT_SDR;
		new_dovi_setting.src_format = FORMAT_SDR;

		/*stb core doesn't need run mode*/
		/*TV core need run mode and the value is 2*/
		if (is_meson_g12() || is_meson_txlx_stbmode() ||
		    is_meson_tm2_stbmode() || is_meson_t7_stbmode() ||
		    is_meson_sc2() || is_meson_s4d())
			dolby_vision_run_mode_delay = 0;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		else if (is_meson_gxm())
			dolby_vision_run_mode_delay = RUN_MODE_DELAY_GXM;
#endif
		else
			dolby_vision_run_mode_delay = RUN_MODE_DELAY;

		adjust_vpotch(graphics_w, graphics_h);
		adjust_vpotch_tv();

		if ((is_meson_tm2_stbmode() || is_meson_t7_stbmode()) &&
			p_funcs_tv && !p_funcs_stb) {
			/* if tv ko loaded and no stb ko */
			/* turn off stb core and force SDR output */
			set_force_output(BT709);
			if (is_dolby_vision_on()) {
				pr_dolby_dbg("Switch from STB to TV core ...\n");
				dolby_vision_mode =
					DOLBY_VISION_OUTPUT_MODE_BYPASS;
			}
		}
	}
	module_installed = true;
	return ret;
}
EXPORT_SYMBOL(register_dv_functions);

int unregister_dv_functions(void)
{
	int ret = -1;

	module_installed = false;

	if (p_funcs_stb || p_funcs_tv) {
		pr_info("*** %s ***\n", __func__);
		if (pq_config_fake) {
			vfree(pq_config_fake);
			pq_config_fake = NULL;
		}
		if (tv_dovi_setting) {
			vfree(tv_dovi_setting);
			tv_dovi_setting = NULL;
		}
		if (tv_input_info) {
			vfree(tv_input_info);
			tv_input_info = NULL;
		}
		if (bin_to_cfg) {
			vfree(bin_to_cfg);
			bin_to_cfg = NULL;
		}
		if (ko_info) {
			vfree(ko_info);
			ko_info = NULL;
		}
		p_funcs_stb = NULL;
		p_funcs_tv = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_dv_functions);

void tv_dolby_vision_crc_clear(int flag)
{
	crc_output_buff_off = 0;
	crc_count = 0;
	crc_bypass_count = 0;
	setting_update_count = 0;
	if (!crc_output_buf)
		crc_output_buf = vmalloc(CRC_BUFF_SIZE);
	pr_info("clear crc_output_buf\n");
	if (crc_output_buf)
		memset(crc_output_buf, 0, CRC_BUFF_SIZE);
	strcpy(cur_crc, "invalid");
}

char *tv_dolby_vision_get_crc(u32 *len)
{
	if (!crc_output_buf || !len || crc_output_buff_off == 0)
		return NULL;
	*len = crc_output_buff_off;
	return crc_output_buf;
}

void tv_dolby_vision_insert_crc(bool print)
{
	char str[64];
	int len;
	bool crc_enable;
	u32 crc;

	if (dolby_vision_flags & FLAG_DISABLE_CRC) {
		crc_bypass_count++;
		crc_count++;
		return;
	}
	if (is_meson_tvmode()) {
		crc_enable = (READ_VPP_DV_REG(DOLBY_TV_DIAG_CTRL) == 0xb);
		crc = READ_VPP_DV_REG(DOLBY_TV_OUTPUT_DM_CRC);
	} else {
		crc_enable = true; /* (READ_VPP_DV_REG(0x36fb) & 1); */
		crc = READ_VPP_DV_REG(0x36fd);
	}
	if (debug_dolby & 0x1000)
		pr_dolby_dbg("crc_enable %d, crc %x\n", crc_enable, crc);
	if (crc == 0 || !crc_enable || !crc_output_buf) {
		crc_bypass_count++;
		crc_count++;
		return;
	}
	if (crc_count < crc_bypass_count)
		crc_bypass_count = crc_count;
	memset(str, 0, sizeof(str));
	snprintf(str, 64, "crc(%d) = 0x%08x",
		crc_count - crc_bypass_count, crc);
	len = strlen(str);
	str[len] = 0xa;
	len++;
	memcpy(&crc_output_buf[crc_output_buff_off],
	       &str[0], len);
	crc_output_buff_off += len;
	//if (print || (debug_dolby & 2))
		pr_info("%s\n", str);
	crc_count++;
}

void tv_dolby_vision_dma_table_modify(u32 tbl_id, u64 value)
{
	u64 *tbl = NULL;

	if (!dma_vaddr || tbl_id >= 3754) {
		pr_info("No dma table %p to write or id %d overflow\n",
			dma_vaddr, tbl_id);
		return;
	}
	tbl = (u64 *)dma_vaddr;
	pr_info("dma_vaddr:%p, modify table[%d]=0x%llx -> 0x%llx\n",
		dma_vaddr, tbl_id, tbl[tbl_id], value);
	tbl[tbl_id] = value;
}

void tv_dolby_vision_efuse_info(void)
{
	if (p_funcs_tv) {
		pr_info("\n dv efuse info:\n");
		pr_info("efuse_mode:%d, version: %s\n",
			efuse_mode, p_funcs_tv->version_info);
	} else {
		pr_info("\n p_funcs is NULL\n");
		pr_info("efuse_mode:%d\n", efuse_mode);
	}
}

void tv_dolby_vision_el_info(void)
{
	pr_info("el_mode:%d\n", el_mode);
}

static int amdolby_vision_open(struct inode *inode, struct file *file)
{
	struct amdolby_vision_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amdolby_vision_dev_s, cdev);
	file->private_data = devp;
	return 0;

}

static char *pq_config_buf;
static u32 pq_config_level;
static ssize_t amdolby_vision_write
	(struct file *file,
	const char __user *buf,
	size_t len,
	loff_t *off)
{
	int max_len, w_len;

	if (!pq_config_buf) {
		pq_config_buf = vmalloc(108 * 1024);
		pq_config_level = 0;
		if (!pq_config_buf)
			return -ENOSPC;
	}
	max_len = sizeof(struct pq_config) - pq_config_level;
	w_len = len < max_len ? len : max_len;

	pr_info("write len %d, w_len %d, level %d\n",
		(int)len, w_len, pq_config_level);
	if (copy_from_user(pq_config_buf + pq_config_level, buf, w_len))
		return -EFAULT;

	pq_config_level += w_len;
	if (pq_config_level == sizeof(struct pq_config)) {
		dolby_vision_update_pq_config(pq_config_buf);
		pq_config_level = 0;
	}

	if (len <= 0x1f) {
		dolby_vision_update_vsvdb_config(pq_config_buf, len);
		pq_config_level = 0;
	}
	return len;
}

static ssize_t amdolby_vision_read
	(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	char *out;
	u32 data_size = 0, res, ret_val = -1;

	if (!is_dolby_vision_enable())
		return ret_val;
	out = tv_dolby_vision_get_crc(&data_size);
	if (data_size > CRC_BUFF_SIZE) {
		pr_err("crc_output_buff_off is out of bound\n");
		tv_dolby_vision_crc_clear(0);
		return ret_val;
	}

	if (out && data_size > 0) {
		res = copy_to_user((void *)buf,
			(void *)out,
			data_size);
		ret_val = data_size - res;
		pr_info
			("%s crc size %d, res: %d, ret: %d\n",
			__func__, data_size, res, ret_val);
		tv_dolby_vision_crc_clear(0);
	}
	return ret_val;
}

static int amdolby_vision_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long amdolby_vision_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
#define MAX_BYTES (128)
	int ret = 0;
	int mode_num = 0;
	int mode_id = 0;
	s16 pq_value = 0;
	enum pq_item_e pq_item;
	enum pq_reset_e pq_reset;

	struct pic_mode_info_s pic_info;
	struct dv_pq_info_s pq_info;
	struct dv_full_pq_info_s pq_full_info;
	struct dv_config_file_s config_file;
	void __user *argp = (void __user *)arg;
	unsigned char bin_name[MAX_BYTES] = "";
	unsigned char cfg_name[MAX_BYTES] = "";
	int dark_detail = 0;

	if (debug_dolby & 0x200)
		pr_info("[DV]: %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (!module_installed) {
		pr_info("[DV] module not install\n");
		return ret;
	}

	if (!load_bin_config && cmd != DV_IOC_SET_DV_CONFIG_FILE) {
		pr_info("[DV] no config file, pq ioctl disable!\n");
		return ret;
	}
	switch (cmd) {
	case DV_IOC_GET_DV_PIC_MODE_NUM:
		mode_num = get_pic_mode_num();
		put_user(mode_num, (u32 __user *)argp);
		break;
	case DV_IOC_GET_DV_PIC_MODE_NAME:
		if (copy_from_user(&pic_info, argp,
				   sizeof(struct pic_mode_info_s)) == 0) {
			mode_id = pic_info.pic_mode_id;
			strcpy(pic_info.name, get_pic_mode_name(mode_id));
			if (debug_dolby & 0x200)
				pr_info("[DV]: get mode %d, name %s\n",
					pic_info.pic_mode_id, pic_info.name);
			if (copy_to_user(argp,
					 &pic_info,
					 sizeof(struct pic_mode_info_s)))
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_GET_DV_PIC_MODE_ID:
		mode_id = get_pic_mode();
		put_user(mode_id, (u32 __user *)argp);
		break;
	case DV_IOC_SET_DV_PIC_MODE_ID:
		if (copy_from_user(&mode_id, argp, sizeof(s32)) == 0) {
			if (debug_dolby & 0x200)
				pr_info("[DV]: set mode %d\n", mode_id);
			set_pic_mode(mode_id);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_GET_DV_SINGLE_PQ_VALUE:
		if (copy_from_user(&pq_info, argp,
				   sizeof(struct dv_pq_info_s)) == 0) {
			mode_id = pq_info.pic_mode_id;
			pq_item = pq_info.item;
			pq_info.value = get_single_pq_value(mode_id, pq_item);

			if (debug_dolby & 0x200)
				pr_info("[DV]: get mode %d, pq %s, value %d\n",
					mode_id,
					pq_item_str[pq_item],
					pq_info.value);

			if (copy_to_user(argp,
					 &pq_info,
					 sizeof(struct dv_pq_info_s)))
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_GET_DV_FULL_PQ_VALUE:
		if (copy_from_user(&pq_full_info, argp,
				   sizeof(struct dv_full_pq_info_s)) == 0) {
			mode_id = pq_full_info.pic_mode_id;
			pq_full_info = get_full_pq_value(mode_id);

			if (copy_to_user(argp,
					 &pq_full_info,
					 sizeof(struct dv_full_pq_info_s)))
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_SINGLE_PQ_VALUE:
		if (copy_from_user(&pq_info, argp,
				   sizeof(struct dv_pq_info_s)) == 0) {
			mode_id = pq_info.pic_mode_id;
			pq_item = pq_info.item;
			pq_value = pq_info.value;
			set_single_pq_value(mode_id, pq_item, pq_value);

			if (debug_dolby & 0x200)
				pr_info("[DV]: set mode %d, pq %s, value %d\n",
					mode_id,
					pq_item_str[pq_item],
					pq_value);

		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_FULL_PQ_VALUE:
		if (copy_from_user(&pq_full_info, argp,
				   sizeof(struct dv_full_pq_info_s)) == 0) {
			set_full_pq_value(pq_full_info);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_PQ_RESET:
		if (copy_from_user(&pq_reset, argp,
				   sizeof(enum pq_reset_e)) == 0) {
			restore_dv_pq_setting(pq_reset);
			if (debug_dolby & 0x200)
				pr_info("[DV]: reset mode %d\n", pq_reset);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_CONFIG_FILE:
		if (copy_from_user(&config_file, argp,
				   sizeof(struct dv_config_file_s)) == 0) {
			if (is_meson_tm2_tvmode() ||
			    is_meson_t7_tvmode() ||
			    is_meson_t3_tvmode() ||
			    is_meson_t5w() ||
			    p_funcs_tv) {
				memcpy(bin_name, config_file.bin_name, MAX_BYTES - 1);
				memcpy(cfg_name, config_file.cfg_name, MAX_BYTES - 1);
				bin_name[MAX_BYTES - 1] = '\0';
				cfg_name[MAX_BYTES - 1] = '\0';
				if (load_dv_pq_config_data(bin_name, cfg_name))
					load_bin_config = true;
			}
			if (debug_dolby & 0x200)
				pr_info("[DV]: config_file %s, %s\n",
					config_file.bin_name,
					config_file.cfg_name);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_CONFIG_DV_BL:
		if (copy_from_user(&force_disable_dv_backlight, argp, sizeof(s32)) == 0) {
			if (debug_dolby & 0x200)
				pr_info("[DV]: disable dv bl %d\n", force_disable_dv_backlight);
			if (force_disable_dv_backlight)
				dolby_vision_disable_backlight();
			else
				update_pwm_control();
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_AMBIENT:
		if (copy_from_user(&ambient_config_new, argp,
			sizeof(struct ambient_cfg_s)) == 0) {
			ambient_update = true;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_DARK_DETAIL:
		if (copy_from_user(&dark_detail, argp,
			sizeof(s32) == 0)) {
			if (debug_dolby & 0x200)
				pr_info("[DV]: set mode %d darkdetail %d\n",
					cur_pic_mode, dark_detail);
			dark_detail = dark_detail > 0 ? 1 : 0;
			if (dark_detail != cfg_info[cur_pic_mode].dark_detail) {
				need_update_cfg = true;
				cfg_info[cur_pic_mode].dark_detail = dark_detail;
			}
		} else {
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long amdolby_vision_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amdolby_vision_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations amdolby_vision_fops = {
	.owner   = THIS_MODULE,
	.open    = amdolby_vision_open,
	.write   = amdolby_vision_write,
	.read = amdolby_vision_read,
	.release = amdolby_vision_release,
	.unlocked_ioctl   = amdolby_vision_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amdolby_vision_compat_ioctl,
#endif
	.poll = amdolby_vision_poll,
};

static const char *amdolby_vision_debug_usage_str = {
	"Usage:\n"
	"echo dolby_crc 0/1 > /sys/class/amdolby_vision/debug; dolby_crc insert or clr\n"
	"echo dolby_dma index(D) value(H) > /sys/class/amdolby_vision/debug; dolby dma table modify\n"
	"echo dv_efuse > /sys/class/amdolby_vision/debug; get dv efuse info\n"
	"echo dv_el > /sys/class/amdolby_vision/debug; get dv enhanced layer info\n"
	"echo force_support_emp 1/0 > /sys/class/amdolby_vision/debug; send emp\n"
	"echo set_backlight_delay 0 > /sys/class/amdolby_vision/debug; set backlight no delay\n"
	"echo set_backlight_delay 1 > /sys/class/amdolby_vision/debug; set backlight delay one vysnc\n"
	"echo enable_vpu_probe 1 > /sys/class/amdolby_vision/debug; enable vpu probe\n"
	"echo debug_bypass_vpp_pq 0 > /sys/class/amdolby_vision/debug; not debug mode\n"
	"echo debug_bypass_vpp_pq 1 > /sys/class/amdolby_vision/debug; force disable vpp pq\n"
	"echo debug_bypass_vpp_pq 2 > /sys/class/amdolby_vision/debug; force enable vpp pq\n"
	"echo bypass_all_vpp_pq 1 > /sys/class/amdolby_vision/debug; force bypass vpp pq in cert mode\n"
	"echo enable_vpu_probe 1 > /sys/class/amdolby_vision/debug; enable vpu probe\n"
	"echo ko_info > /sys/class/amdolby_vision/debug; query ko info\n"
	"echo enable_vf_check 1 > /sys/class/amdolby_vision/debug;\n"
	"echo enable_vf_check 0 > /sys/class/amdolby_vision/debug;\n"
	"echo force_hdmin_fmt value > /sys/class/amdolby_vision/debug; 1:HDR10 2:HLG 3:DV LL\n"
	"echo debug_cp_res value > /sys/class/amdolby_vision/debug; bit0~bit15 h, bit16~bit31 w\n"
	"echo debug_disable_aoi value > /sys/class/amdolby_vision/debug; 1:disable aoi;>1 not disable\n"
	"echo debug_dma_start_line value > /sys/class/amdolby_vision/debug;\n"
};

static ssize_t  amdolby_vision_debug_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  amdolby_vision_debug_usage_str);
}

static ssize_t amdolby_vision_debug_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};
	long val = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "dolby_crc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 1)
			tv_dolby_vision_crc_clear(val);
		else
			tv_dolby_vision_insert_crc(true);
	} else if (!strcmp(parm[0], "dolby_dma")) {
		long tbl_id;
		long value;

		if (kstrtoul(parm[1], 10, &tbl_id) < 0)
			return -EINVAL;
		if (kstrtoul(parm[2], 16, &value) < 0)
			return -EINVAL;
		tv_dolby_vision_dma_table_modify((u32)tbl_id, (u64)value);
	} else if (!strcmp(parm[0], "dv_efuse")) {
		tv_dolby_vision_efuse_info();
	} else if (!strcmp(parm[0], "dv_el")) {
		tv_dolby_vision_el_info();
#ifdef V2_4_3
	} else if (!strcmp(parm[0], "force_support_emp")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			force_support_emp = 0;
		else
			force_support_emp = 1;
		pr_info("force_support_emp %d\n", force_support_emp);
#endif
	} else if (!strcmp(parm[0], "enable_vpu_probe")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0) {
			enable_vpu_probe = 0;
		} else {
			enable_vpu_probe = 1;
			if ((is_meson_tm2() || is_meson_sm1_cpu()))
				__invoke_psci_fn_smc(0x82000080, 0, 0, 0);
		}
		pr_info("set enable_vpu_probe %d\n",
			enable_vpu_probe);
	} else if (!strcmp(parm[0], "debug_bypass_vpp_pq")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		debug_bypass_vpp_pq = val;
		pr_info("set debug_bypass_vpp_pq %d\n",
			debug_bypass_vpp_pq);
	} else if (!strcmp(parm[0], "force_cert_bypass_vpp_pq")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			bypass_all_vpp_pq = 0;
		else
			bypass_all_vpp_pq = 1;
		pr_info("set bypass_all_vpp_pq %d\n",
			bypass_all_vpp_pq);
	} else if (!strcmp(parm[0], "enable_fel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_fel = val;
		pr_info("enable_fel %d\n", enable_fel);
	} else if (!strcmp(parm[0], "enable_mel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_mel = val;
		pr_info("enable_mel %d\n", enable_mel);
	} else if (!strcmp(parm[0], "enable_tunnel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_tunnel = val;
		pr_info("enable_tunnel %d\n", enable_tunnel);
		if (is_meson_sc2() || is_meson_t7() || is_meson_t3() ||
		    is_meson_s4d() || is_meson_t5w()) {
			/*for vdin1 loop back, 444,12bit->422,12bit->444,8bit*/
			if (enable_tunnel) {
				if (vpp_data_422T0444_backup == 0) {
					vpp_data_422T0444_backup =
					VSYNC_RD_DV_REG(VPU_422T0444_CTRL1);
					pr_dolby_dbg("vpp_data_422T0444_backup %x\n",
						     vpp_data_422T0444_backup);
				}
				/*go_field_en and go_line_en bit 24 25 =1*/
				VSYNC_WR_DV_REG(VPU_422T0444_CTRL1, 0x07c0ba14);
			} else {
				if (vpp_data_422T0444_backup != 0)
					VSYNC_WR_DV_REG(VPU_422T0444_CTRL1,
						vpp_data_422T0444_backup);
			}
		}
	} else if (!strcmp(parm[0], "ko_info")) {
		if (ko_info)
			pr_info("ko info: %s\n", ko_info);
	} else if (!strcmp(parm[0], "enable_vf_check")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			enable_vf_check = 0;
		else
			enable_vf_check = 1;
		pr_info("set enable_vf_check %d\n",
			enable_vf_check);
	} else if (!strcmp(parm[0], "force_hdmin_fmt")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		force_hdmin_fmt = val;
		pr_info("set force_hdmin_fmt %d\n", force_hdmin_fmt);
	} else if (!strcmp(parm[0], "debug_disable_aoi")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		debug_disable_aoi = val;
		pr_info("set debug_disable_aoi %d\n", debug_disable_aoi);
	} else if (!strcmp(parm[0], "debug_cp_res")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		debug_cp_res = val;
		pr_info("set debug_cp_res %d\n", debug_cp_res);
	} else if (!strcmp(parm[0], "debug_dma_start_line")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		debug_dma_start_line = val;
		pr_info("set debug_dma_start_line %d\n", debug_dma_start_line);
	} else if (!strcmp(parm[0], "force_runmode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			force_runmode = 0;
		else
			force_runmode = 1;
		pr_info("set force_runmode %d\n",
			force_runmode);
	} else {
		pr_info("unsupport cmd\n");
	}

	kfree(buf_orig);
	return count;
}

static ssize_t	amdolby_vision_primary_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 100, "debug %d, cur_debug_tprimary %d %d %d %d %d %d\n",
			debug_tprimary,
			cur_debug_tprimary[0][0], cur_debug_tprimary[0][1],
			cur_debug_tprimary[1][0], cur_debug_tprimary[1][1],
			cur_debug_tprimary[2][0], cur_debug_tprimary[2][1]);
}

static ssize_t amdolby_vision_primary_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "primary_r")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[0][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[0][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "primary_g")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[1][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[1][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "primary_b")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[2][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[2][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "debug_tprimary")) {
		if (kstrtoint(parm[1], 10, &debug_tprimary) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	}
	kfree(buf_orig);
	update_cp_cfg();
	return count;
}

static ssize_t	amdolby_vision_config_file_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	const char *str =
		"echo bin cfg > /sys/class/amdolby_vision/config_file";
	return sprintf(buf, "%s\n", str);
}

static ssize_t amdolby_vision_config_file_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!parm[0] || !parm[1]) {
		pr_info("missing parameter... param1:bin param2:cfg\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	pr_info("parm[0]: %s, parm[1]: %s\n", parm[0], parm[1]);
	if (is_meson_tm2_tvmode() || p_funcs_tv) {
		if (load_dv_pq_config_data(parm[0], parm[1]))
			load_bin_config = true;
	}
	kfree(buf_orig);
	return count;
}

static ssize_t	amdolby_vision_dv_provider_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", dv_provider);
}

static ssize_t	amdolby_vision_content_fps_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "content_fps: %d\n", content_fps);
}

static ssize_t amdolby_vision_content_fps_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &content_fps);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t	amdolby_vision_gd_rf_adjust_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "gd_rf_adjust: %d\n", gd_rf_adjust);
}

static ssize_t amdolby_vision_gd_rf_adjust_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &gd_rf_adjust);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t	amdolby_vision_force_disable_bl_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "force disable dv bl: %d\n", force_disable_dv_backlight);
}

static ssize_t amdolby_vision_force_disable_bl_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &force_disable_dv_backlight);
	if (r != 0)
		return -EINVAL;

	pr_info("update force_disable_dv_backlight to %d\n", force_disable_dv_backlight);
	if (force_disable_dv_backlight)
		dolby_vision_disable_backlight();
	else
		update_pwm_control();

	return count;
}

static ssize_t	amdolby_vision_use_cfg_target_lum_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "use_target_lum_from_cfg: %d\n",
		       use_target_lum_from_cfg);
}

static ssize_t amdolby_vision_use_cfg_target_lum_store
	(struct class *cla,
	 struct class_attribute *attr,
	 const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &use_target_lum_from_cfg);
	if (r != 0)
		return -EINVAL;

	pr_info("update use_target_lum_from_cfg to %d\n",
		use_target_lum_from_cfg);

	return count;
}

static ssize_t	amdolby_vision_brightness_off_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 100, "brightness_off %d %d,%d %d,%d %d,%d %d\n",
			brightness_off[0][0], brightness_off[0][1],
			brightness_off[1][0], brightness_off[1][1],
			brightness_off[2][0], brightness_off[2][1],
			brightness_off[3][0], brightness_off[3][1]);
}

/* supported mode: IPT_TUNNEL/HDR10/SDR10 */
static const int dv_mode_table[6] = {
	5, /*DOLBY_VISION_OUTPUT_MODE_BYPASS*/
	0, /*DOLBY_VISION_OUTPUT_MODE_IPT*/
	1, /*DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL*/
	2, /*DOLBY_VISION_OUTPUT_MODE_HDR10*/
	3, /*DOLBY_VISION_OUTPUT_MODE_SDR10*/
	4, /*DOLBY_VISION_OUTPUT_MODE_SDR8*/
};

static const char dv_mode_str[6][12] = {
	"IPT",
	"IPT_TUNNEL",
	"HDR10",
	"SDR10",
	"SDR8",
	"BYPASS"
};

unsigned int dolby_vision_check_enable(void)
{
	int uboot_dv_mode = 0;
	int uboot_dv_source_led_yuv = 0;
	int uboot_dv_source_led_rgb = 0;
	int uboot_dv_sink_led = 0;
	const struct vinfo_s *vinfo = get_current_vinfo();

	/*first step: check tv mode, dv is disabled by default */
	if (is_meson_tm2_tvmode() || is_meson_t7_tvmode() ||
	    is_meson_t3() || is_meson_t5w()) {
		if (!dolby_vision_on_in_uboot) {
			/* tm2/t7 also has stb core, should power off stb core*/
			if (is_meson_tm2_tvmode() || is_meson_t7_tvmode()) {
				/* core1a */
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				WRITE_VPP_DV_REG
					(DOLBY_CORE1A_CLKGATE_CTRL,
					0x55555455);
				/* core1b */
				dv_mem_power_off(VPU_DOLBY1B);
				WRITE_VPP_DV_REG
					(DOLBY_CORE1B_CLKGATE_CTRL,
					0x55555455);
				/* core2 */
				dv_mem_power_off(VPU_DOLBY2);
				WRITE_VPP_DV_REG
					(DOLBY_CORE2A_CLKGATE_CTRL,
					0x55555555);
				/* core3 */
				dv_mem_power_off(VPU_DOLBY_CORE3);
				WRITE_VPP_DV_REG
					(DOLBY_CORE3_CLKGATE_CTRL,
					0x55555555);
			}
			/* tv core */
			WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
				0x01000042);
			WRITE_VPP_DV_REG_BITS
				(DOLBY_TV_SWAP_CTRL7,
				0xf, 4, 4);
			dv_mem_power_off(VPU_DOLBY0);
			WRITE_VPP_DV_REG
				(DOLBY_TV_CLKGATE_CTRL,
				0x55555555);
			if (is_meson_t3() || is_meson_t5w())
				vpu_module_clk_disable(0, DV_TVCORE, 1);
			pr_info("dovi disable in uboot\n");
		}
	}

	/*second step: check ott mode*/
	if (is_meson_g12() || is_meson_sc2() || is_meson_tm2_stbmode() ||
		is_meson_t7_stbmode() || is_meson_s4d()) {
		if (dolby_vision_on_in_uboot) {
			if ((READ_VPP_DV_REG(DOLBY_CORE3_DIAG_CTRL)
				& 0xff) == 0x20) {
				/*LL YUV422 mode*/
				uboot_dv_mode = dv_mode_table[2];
				uboot_dv_source_led_yuv = 1;
			} else if ((READ_VPP_DV_REG
				(DOLBY_CORE3_DIAG_CTRL)
				& 0xff) == 0x3) {
				/*LL RGB444 mode*/
				uboot_dv_mode = dv_mode_table[2];
				uboot_dv_source_led_rgb = 1;
			} else {
				if (READ_VPP_DV_REG
					(DOLBY_CORE3_REG_START + 1)
					== 2) {
					/*HDR10 mode*/
					uboot_dv_mode = dv_mode_table[3];
				} else if (READ_VPP_DV_REG
					(DOLBY_CORE3_REG_START + 1)
					== 4) {
					/*SDR mode*/
					uboot_dv_mode = dv_mode_table[5];
				} else {
					/*STANDARD RGB444 mode*/
					uboot_dv_mode = dv_mode_table[2];
					uboot_dv_sink_led = 1;
				}
			}
			if (recovery_mode) {/*recovery mode*/
				pr_info("recovery_mode\n");
				dolby_vision_on = true;
				if (uboot_dv_source_led_yuv ||
					uboot_dv_sink_led) {
					#ifdef CONFIG_AMLOGIC_HDMITX
					if (uboot_dv_source_led_yuv)
						setup_attr("422,12bit");
					else
						setup_attr("444,8bit");
					#endif
				}
				if (vinfo && vinfo->vout_device &&
				    vinfo->vout_device->fresh_tx_vsif_pkt) {
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, true);
				}
				enable_dolby_vision(0);
				dolby_vision_on_in_uboot = 0;
			} else {
				dolby_vision_enable = 1;
				if (uboot_dv_mode == dv_mode_table[2] &&
					uboot_dv_source_led_yuv == 1) {
					/*LL YUV422 mode*/
					/*set_dolby_vision_mode(dv_mode);*/
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = DV_PROCESS;
					dolby_vision_ll_policy =
						DOLBY_VISION_LL_YUV422;
					last_dst_format = FORMAT_DOVI;
					pr_info("dovi enable in uboot and mode is LL 422\n");
				} else if ((uboot_dv_mode ==
						dv_mode_table[2]) &&
						(uboot_dv_source_led_rgb ==
						1)) {
					/*LL RGB444 mode*/
					/*set_dolby_vision_mode(dv_mode);*/
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = DV_PROCESS;
					dolby_vision_ll_policy =
						DOLBY_VISION_LL_RGB444;
					last_dst_format = FORMAT_DOVI;
					pr_info("dovi enable in uboot and mode is LL RGB\n");
				} else {
					if (uboot_dv_mode == dv_mode_table[3]) {
						/*HDR10 mode*/
						dolby_vision_hdr10_policy |=
							HDR_BY_DV_F_SINK;
						dolby_vision_mode =
							uboot_dv_mode;
						dolby_vision_status =
							HDR_PROCESS;
						pr_info("dovi enable in uboot and mode is HDR10\n");
						last_dst_format = FORMAT_HDR10;
					} else if (uboot_dv_mode ==
						dv_mode_table[5]) {
						/*SDR mode*/
						dolby_vision_mode =
							uboot_dv_mode;
						dolby_vision_status =
							SDR_PROCESS;
						pr_info("dovi enable in uboot and mode is SDR\n");
						last_dst_format = FORMAT_SDR;
					} else {
						/*STANDARD RGB444 mode*/
						dolby_vision_mode =
							uboot_dv_mode;
						dolby_vision_status =
							DV_PROCESS;
						dolby_vision_ll_policy =
							DOLBY_VISION_LL_DISABLE;
						last_dst_format = FORMAT_DOVI;
						pr_info("dovi enable in uboot and mode is DV ST\n");
					}
				}
				dolby_vision_target_mode = dolby_vision_mode;
			}
		} else {
			/* core1a */
			dv_mem_power_off(VPU_DOLBY1A);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
			WRITE_VPP_DV_REG
				(DOLBY_CORE1A_CLKGATE_CTRL,
				0x55555455);
			/* core1b */
			dv_mem_power_off(VPU_DOLBY1B);
			WRITE_VPP_DV_REG
				(DOLBY_CORE1B_CLKGATE_CTRL,
				0x55555455);
			/* core2 */
			dv_mem_power_off(VPU_DOLBY2);
			WRITE_VPP_DV_REG
				(DOLBY_CORE2A_CLKGATE_CTRL,
				0x55555555);
			/* core3 */
			dv_mem_power_off(VPU_DOLBY_CORE3);
			WRITE_VPP_DV_REG
				(DOLBY_CORE3_CLKGATE_CTRL,
				0x55555555);
			/*tm2/t7 has tvcore, should also power off tvcore*/
			if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
				/* tv core */
				WRITE_VPP_DV_REG(DOLBY_TV_AXI2DMA_CTRL0,
					0x01000042);
				WRITE_VPP_DV_REG_BITS
					(DOLBY_TV_SWAP_CTRL7,
					0xf, 4, 4);
				dv_mem_power_off(VPU_DOLBY0);
				WRITE_VPP_DV_REG
					(DOLBY_TV_CLKGATE_CTRL,
					0x55555555);
			}
			pr_info("dovi disable in uboot\n");
		}
	}
	return 0;
}

static ssize_t amdolby_vision_dv_mode_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("usage: echo mode > /sys/class/amdolby_vision/dv_mode\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_BYPASS		0\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_IPT			1\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL	2\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_HDR10		3\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_SDR10		4\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_SDR8		5\n");
	if (is_dolby_vision_enable())
		pr_info("current dv_mode = %s\n",
			dv_mode_str[get_dolby_vision_mode()]);
	else
		pr_info("current dv_mode = off\n");
	return 0;
}

static ssize_t amdolby_vision_dv_mode_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r != 0)
		return -EINVAL;

	if (val >= 0 && val < 6)
		set_dolby_vision_mode(dv_mode_table[val]);
	else if (val & 0x200)
		dolby_vision_dump_struct();
	else if (val & 0x70)
		dolby_vision_dump_setting(val);
	return count;
}

static void parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t amdolby_vision_reg_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int reg_addr, reg_val;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = READ_VPP_DV_REG(reg_addr);
		pr_info("reg[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_val = val;
		WRITE_VPP_DV_REG(reg_addr, reg_val);
	}
	kfree(buf_orig);
	buf_orig =	NULL;
	return count;

}

static ssize_t amdolby_vision_core1_switch_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n",
		core1_switch);
}

static ssize_t amdolby_vision_core1_switch_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	u32 reg = 0, mask = 0xfaa1f00, set = 0;

	r = kstrtoint(buf, 0, &core1_switch);
	if (r != 0)
		return -EINVAL;
	if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
		reg = VSYNC_RD_DV_REG(DOLBY_PATH_CTRL);
		switch (core1_switch) {
		case NO_SWITCH:
			reg &= ~mask;
			set = reg | 0x0c880c00;
			VSYNC_WR_DV_REG(DOLBY_PATH_CTRL, set);
			break;
		case SWITCH_BEFORE_DVCORE_1:
			reg &= ~mask;
			set = reg | 0x0c881c00;
			VSYNC_WR_DV_REG(DOLBY_PATH_CTRL, set);
			break;
		case SWITCH_BEFORE_DVCORE_2:
			reg &= ~mask;
			set = reg | 0x0c820300;
			VSYNC_WR_DV_REG(DOLBY_PATH_CTRL, set);
			break;
		case SWITCH_AFTER_DVCORE:
			reg &= ~mask;
			set = reg | 0x03280c00;
			VSYNC_WR_DV_REG(DOLBY_PATH_CTRL, set);
			break;
		}
	}
	return count;
}

static ssize_t amdolby_vision_core3_switch_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n",
		core3_switch);
}

static ssize_t amdolby_vision_dv_support_info_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dolby_dbg("show dv capability %d\n", support_info);
	return snprintf(buf, 40, "%d\n",
		support_info);
}

static ssize_t amdolby_vision_core3_switch_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &core3_switch);
	if (r != 0)
		return -EINVAL;
	if (is_meson_tm2_stbmode() || is_meson_t7_stbmode()) {
		switch (core3_switch) {
		case CORE3_AFTER_WM:
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL,
					     0, 24, 2);
			break;
		case CORE3_AFTER_OSD1_HDR:
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL,
					     1, 24, 2);
			break;
		case CORE3_AFTER_VD2_HDR:
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL,
					     2, 24, 2);
			break;
		}
	}
	return count;
}

static ssize_t amdolby_vision_copy_core1a_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dolby_dbg("copy_core1a %d\n", copy_core1a);
	return snprintf(buf, 40, "%d\n",
		copy_core1a);
}

static ssize_t amdolby_vision_copy_core1a_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &copy_core1a);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t amdolby_vision_core2_sel_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dolby_dbg("core2_sel %d\n", core2_sel);
	return snprintf(buf, 40, "%d\n",
		core2_sel);
}

static ssize_t amdolby_vision_core2_sel_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	if (!is_meson_t7())
		return -EINVAL;
	r = kstrtoint(buf, 0, &core2_sel);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t  amdolby_vision_crc_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", cur_crc);
}

static int amdv_notify_callback(struct notifier_block *block,
	unsigned long cmd,
	void *para)
{
	u32 *p, val;

	switch (cmd) {
	case AMDV_UPDATE_OSD_MODE:
		p = (u32 *)para;
		if (!update_mali_top_ctrl) {
			mali_afbcd_top_ctrl_mask = p[1];
			mali_afbcd1_top_ctrl_mask = p[3];
		}
		val = mali_afbcd_top_ctrl
			& (~mali_afbcd_top_ctrl_mask);
		val |= (p[0] & mali_afbcd_top_ctrl_mask);
		mali_afbcd_top_ctrl = val;

		val = mali_afbcd1_top_ctrl
			& (~mali_afbcd1_top_ctrl_mask);
		val |= (p[2] & mali_afbcd1_top_ctrl_mask);
		mali_afbcd1_top_ctrl = val;

		if (!update_mali_top_ctrl)
			update_mali_top_ctrl = true;
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block amdv_notifier = {
	.notifier_call = amdv_notify_callback,
};

static RAW_NOTIFIER_HEAD(amdv_notifier_list);
int amdv_register_client(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&amdv_notifier_list, nb);
}
EXPORT_SYMBOL(amdv_register_client);

int amdv_unregister_client(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&amdv_notifier_list, nb);
}
EXPORT_SYMBOL(amdv_unregister_client);

int amdv_notifier_call_chain(unsigned long val, void *v)
{
	return raw_notifier_call_chain(&amdv_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(amdv_notifier_call_chain);

static struct class_attribute amdolby_vision_class_attrs[] = {
	__ATTR(debug, 0644,
	amdolby_vision_debug_show, amdolby_vision_debug_store),
	__ATTR(dv_mode, 0644,
	amdolby_vision_dv_mode_show, amdolby_vision_dv_mode_store),
	__ATTR(dv_reg, 0220,
	NULL, amdolby_vision_reg_store),
	__ATTR(core1_switch, 0644,
	amdolby_vision_core1_switch_show, amdolby_vision_core1_switch_store),
	__ATTR(core3_switch, 0644,
	amdolby_vision_core3_switch_show, amdolby_vision_core3_switch_store),
	__ATTR(dv_bin_config, 0644,
	       amdolby_vision_bin_config_show, NULL),
	__ATTR(dv_pq_info, 0644,
	       amdolby_vision_pq_info_show,
	       amdolby_vision_pq_info_store),
	__ATTR(dv_set_inter_pq, 0644,
	       amdolby_vision_set_inter_pq_show,
	       amdolby_vision_set_inter_pq_store),
	__ATTR(dv_load_cfg_status, 0644,
	       amdolby_vision_load_cfg_status_show,
	       amdolby_vision_load_cfg_status_store),
	__ATTR(support_info, 0444,
	       amdolby_vision_dv_support_info_show, NULL),
	__ATTR(tprimary, 0644,
	       amdolby_vision_primary_show,
	       amdolby_vision_primary_store),
	__ATTR(config_file, 0644,
	       amdolby_vision_config_file_show,
	       amdolby_vision_config_file_store),
	__ATTR(copy_core1a, 0644,
	       amdolby_vision_copy_core1a_show,
	       amdolby_vision_copy_core1a_store),
	__ATTR(core2_sel, 0644,
	       amdolby_vision_core2_sel_show,
	       amdolby_vision_core2_sel_store),
	__ATTR(dv_provider, 0644,
	       amdolby_vision_dv_provider_show,
	       NULL),
	__ATTR(content_fps, 0644,
	       amdolby_vision_content_fps_show,
	       amdolby_vision_content_fps_store),
	__ATTR(gd_rf_adjust, 0644,
	       amdolby_vision_gd_rf_adjust_show,
	       amdolby_vision_gd_rf_adjust_store),
	__ATTR(cur_crc, 0644,
	       amdolby_vision_crc_show,
	       NULL),
	__ATTR(force_disable_dv_backlight, 0644,
	       amdolby_vision_force_disable_bl_show,
	       amdolby_vision_force_disable_bl_store),
	__ATTR(use_target_lum_from_cfg, 0644,
	       amdolby_vision_use_cfg_target_lum_show,
	       amdolby_vision_use_cfg_target_lum_store),
	__ATTR(brightness_off, 0644,
	       amdolby_vision_brightness_off_show,
	       NULL),
	__ATTR_NULL
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct dv_device_data_s dolby_vision_gxm = {
	.cpu_id = _CPU_MAJOR_ID_GXM,
};

static struct dv_device_data_s dolby_vision_txlx = {
	.cpu_id = _CPU_MAJOR_ID_TXLX,
};
#endif

static struct dv_device_data_s dolby_vision_g12 = {
	.cpu_id = _CPU_MAJOR_ID_G12,
};

static struct dv_device_data_s dolby_vision_tm2 = {
	.cpu_id = _CPU_MAJOR_ID_TM2,
};

static struct dv_device_data_s dolby_vision_tm2_revb = {
	.cpu_id = _CPU_MAJOR_ID_TM2_REVB,
};

static struct dv_device_data_s dolby_vision_sc2 = {
	.cpu_id = _CPU_MAJOR_ID_SC2,
};

static struct dv_device_data_s dolby_vision_t7 = {
	.cpu_id = _CPU_MAJOR_ID_T7,
};

static struct dv_device_data_s dolby_vision_t3 = {
	.cpu_id = _CPU_MAJOR_ID_T3,
};

static struct dv_device_data_s dolby_vision_s4d = {
	.cpu_id = _CPU_MAJOR_ID_S4D,
};

static struct dv_device_data_s dolby_vision_t5w = {
	.cpu_id = _CPU_MAJOR_ID_T5W,
};

static const struct of_device_id amlogic_dolby_vision_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, dolby_vision_gxm",
		.data = &dolby_vision_gxm,
	},
	{
		.compatible = "amlogic, dolby_vision_txlx",
		.data = &dolby_vision_txlx,
	},
#endif
	{
		.compatible = "amlogic, dolby_vision_g12a",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_g12b",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_sm1",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_tm2",
		.data = &dolby_vision_tm2,
	},
	{
		.compatible = "amlogic, dolby_vision_tm2_revb",
		.data = &dolby_vision_tm2_revb,
	},
	{
		.compatible = "amlogic, dolby_vision_sc2",
		.data = &dolby_vision_sc2,
	},
	{
		.compatible = "amlogic, dolby_vision_t7",
		.data = &dolby_vision_t7,
	},
	{
		.compatible = "amlogic, dolby_vision_t3",
		.data = &dolby_vision_t3,
	},
	{
		.compatible = "amlogic, dolby_vision_s4d",
		.data = &dolby_vision_s4d,
	},
	{
		.compatible = "amlogic, dolby_vision_t5w",
		.data = &dolby_vision_t5w,
	},
	{},
};

static int amdolby_vision_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;
	unsigned int val;

	pr_info("\n amdolby_vision probe start & ver: %s\n", DRIVER_VER);
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct dv_device_data_s *dv_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(amlogic_dolby_vision_match, of_node);
		if (match) {
			dv_meson = (struct dv_device_data_s *)match->data;
			if (dv_meson) {
				memcpy(&dv_meson_dev, dv_meson,
				       sizeof(struct dv_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				dolby_vision_probe_ok = 0;
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			dolby_vision_probe_ok = 0;
			return -ENODEV;
		}
		ret = of_property_read_u32(of_node, "tv_mode", &val);
		if (ret)
			pr_info("Can't find  tv_mode.\n");
		else
			tv_mode = val;
		if (tv_mode)
			support_info |= 1 << 3;
	}
	pr_info("\n cpu_id=%d tvmode=%d\n", dv_meson_dev.cpu_id, tv_mode);
	memset(devp, 0, (sizeof(struct amdolby_vision_dev_s)));
	if (is_meson_tm2_tvmode() || is_meson_t7_tvmode() ||
		is_meson_t3_tvmode() || is_meson_t5w()) {
		dolby_vision_hdr10_policy |= HLG_BY_DV_F_SINK;
		pr_info("enable DV HLG when follow sink.\n");
		dolby_vision_flags |= FLAG_RX_EMP_VSEM;
		pr_info("enable DV VSEM.\n");
	}
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMDOLBY_VISION_NAME);
	if (ret < 0)
		goto fail_alloc_region;
	devp->clsp = class_create(THIS_MODULE,
		AMDOLBY_VISION_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0;  amdolby_vision_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp,
			&amdolby_vision_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amdolby_vision_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMDOLBY_VISION_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}
	dolby_vision_addr();
	dolby_vision_init_receiver(pdev);
	init_waitqueue_head(&devp->dv_queue);
	pr_info("%s: ok\n", __func__);
	dolby_vision_check_enable();
	dolby_vision_probe_ok = 1;
	return 0;

fail_create_device:
	pr_info("[amdolby_vision.] : amdolby_vision device create error.\n");
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_info("[amdolby_vision.] : amdolby_vision add device error.\n");
fail_class_create_file:
	pr_info("[amdolby_vision.] : amdolby_vision class create file error.\n");
	for (i = 0; amdolby_vision_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
		&amdolby_vision_class_attrs[i]);
	}
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amdolby_vision.] : amdolby_vision class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amdolby_vision.] : amdolby_vision alloc error.\n");
	pr_info("[amdolby_vision.] : amdolby_vision_init.\n");
	dolby_vision_probe_ok = 0;
	return ret;

}

static int __exit amdolby_vision_remove(struct platform_device *pdev)
{
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;
	int i;

	if (pq_config_buf) {
		vfree(pq_config_buf);
		pq_config_buf = NULL;
	}
	for (i = 0; i < 2; i++) {
		if (md_buf[i]) {
			vfree(md_buf[i]);
			md_buf[i] = NULL;
		}
		if (comp_buf[i]) {
			vfree(comp_buf[i]);
			comp_buf[i] = NULL;
		}
		if (drop_md_buf[i]) {
			vfree(drop_md_buf[i]);
			drop_md_buf[i] = NULL;
		}
		if (drop_comp_buf[i]) {
			vfree(drop_comp_buf[i]);
			drop_comp_buf[i] = NULL;
		}
	}
	if (vsem_if_buf) {
		vfree(vsem_if_buf);
		vsem_if_buf = NULL;
	}
	if (vsem_md_buf) {
		vfree(vsem_md_buf);
		vsem_md_buf = NULL;
	}
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	pr_info("[ amdolby_vision.] :  amdolby_vision_exit.\n");
	return 0;
}

static struct platform_driver aml_amdolby_vision_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "aml_amdolby_vision_driver",
		.of_match_table = amlogic_dolby_vision_match,
	},
	.probe = amdolby_vision_probe,
	.remove = __exit_p(amdolby_vision_remove),
};

static int get_dolby_uboot_status(char *str)
{
	char uboot_dolby_status[DV_NAME_LEN_MAX] = {0};

	snprintf(uboot_dolby_status, DV_NAME_LEN_MAX, "%s", str);
	pr_info("get_dolby_on: %s\n", uboot_dolby_status);

	if (!strcmp(uboot_dolby_status, "1")) {
		dolby_vision_on_in_uboot = 1;
		dolby_vision_enable = 1;
	}
	return 0;
}
__setup("dolby_vision_on=", get_dolby_uboot_status);

static int recovery_mode_check(char *str)
{
	char recovery_status[DV_NAME_LEN_MAX] = {0};

	snprintf(recovery_status, DV_NAME_LEN_MAX, "%s", str);
	pr_info("recovery_status: %s\n", recovery_status);

	if (strlen(recovery_status) > 0)
		recovery_mode = true;
	return 0;
}
__setup("recovery_part=", recovery_mode_check);

int __init amdolby_vision_init(void)
{
	pr_info("%s:module init\n", __func__);

	if (platform_driver_register(&aml_amdolby_vision_driver)) {
		pr_err("failed to register amdolby_vision module\n");
		return -ENODEV;
	}
	amdv_register_client(&amdv_notifier);
	return 0;
}

void __exit amdolby_vision_exit(void)
{
	pr_info("%s:module exit\n", __func__);
	amdv_unregister_client(&amdv_notifier);
	platform_driver_unregister(&aml_amdolby_vision_driver);
}

//MODULE_DESCRIPTION("AMLOGIC amdolby_vision driver");
//MODULE_LICENSE("GPL");

