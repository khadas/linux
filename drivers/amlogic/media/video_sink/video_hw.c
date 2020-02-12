/*
 * drivers/amlogic/media/video_sink/video_hw.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"

#include <linux/amlogic/media/video_sink/vpp.h>
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "../common/vfm/vfm.h"
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>

struct video_layer_s vd_layer[MAX_VD_LAYER];
struct disp_info_s glayer_info[MAX_VD_LAYERS];

struct video_dev_s video_dev = {0x1d00 - 0x1d00, 0x1a50 - 0x1a50};
struct video_dev_s *cur_dev = &video_dev;
bool legacy_vpp = true;

static bool bypass_cm;

static DEFINE_SPINLOCK(video_onoff_lock);
static DEFINE_SPINLOCK(video2_onoff_lock);

/* VPU delay work */
#define VPU_DELAYWORK_VPU_VD1_CLK			1
#define VPU_DELAYWORK_VPU_VD2_CLK			2
#define VPU_DELAYWORK_MEM_POWER_OFF_VD1			4
#define VPU_DELAYWORK_MEM_POWER_OFF_VD2			8
#define VPU_VIDEO_LAYER1_CHANGED			0x10
#define VPU_VIDEO_LAYER2_CHANGED			0x20
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0		0x40
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A		0x80
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B		0x100
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2		0x200
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3		0x400
#define VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY		0x800

#define VPU_MEM_POWEROFF_DELAY	100
#define DV_MEM_POWEROFF_DELAY	2

static struct work_struct vpu_delay_work;
static int vpu_vd1_clk_level;
static int vpu_vd2_clk_level;
static DEFINE_SPINLOCK(delay_work_lock);
static int vpu_delay_work_flag;
static int vpu_mem_power_off_count;
static int dv_mem_power_off_count;

#define VPU_VD1_CLK_SWITCH(level) \
	do { \
		unsigned long flags; \
		int next_level = (level); \
		if (vpu_vd1_clk_level != next_level) { \
			vpu_vd1_clk_level = next_level; \
			spin_lock_irqsave(&delay_work_lock, flags); \
			vpu_delay_work_flag |= \
				VPU_DELAYWORK_VPU_VD1_CLK; \
			spin_unlock_irqrestore(&delay_work_lock, flags); \
		} \
	} while (0)

#define VPU_VD2_CLK_SWITCH(level) \
	do { \
		unsigned long flags; \
		int next_level = (level); \
		if (vpu_vd2_clk_level != next_level) { \
			vpu_vd2_clk_level = next_level; \
			spin_lock_irqsave(&delay_work_lock, flags); \
			vpu_delay_work_flag |= \
				VPU_DELAYWORK_VPU_VD2_CLK; \
			spin_unlock_irqrestore(&delay_work_lock, flags); \
		} \
	} while (0)

#define VD1_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD1; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		switch_vpu_mem_pd_vmod(VPU_VIU_VD1, VPU_MEM_POWER_ON); \
		switch_vpu_mem_pd_vmod(VPU_AFBC_DEC, VPU_MEM_POWER_ON); \
		switch_vpu_mem_pd_vmod(VPU_DI_POST, VPU_MEM_POWER_ON); \
		if (!legacy_vpp) \
			switch_vpu_mem_pd_vmod( \
				VPU_VD1_SCALE, VPU_MEM_POWER_ON); \
	} while (0)
#define VD2_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD2; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		switch_vpu_mem_pd_vmod(VPU_VIU_VD2, VPU_MEM_POWER_ON); \
		switch_vpu_mem_pd_vmod(VPU_AFBC_DEC1, VPU_MEM_POWER_ON); \
		if (!legacy_vpp) \
			switch_vpu_mem_pd_vmod( \
				VPU_VD2_SCALE, VPU_MEM_POWER_ON); \
	} while (0)
#define VD1_MEM_POWER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag |= VPU_DELAYWORK_MEM_POWER_OFF_VD1; \
		vpu_mem_power_off_count = VPU_MEM_POWEROFF_DELAY; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
	} while (0)
#define VD2_MEM_POWER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag |= VPU_DELAYWORK_MEM_POWER_OFF_VD2; \
		vpu_mem_power_off_count = VPU_MEM_POWEROFF_DELAY; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
	} while (0)

#define VIDEO_LAYER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video_onoff_lock, flags); \
		if (vd_layer[0].onoff_state != VIDEO_ENABLE_STATE_ON_PENDING) \
			vd_layer[0].onoff_state = VIDEO_ENABLE_STATE_ON_REQ; \
		vd_layer[0].enabled = 1; \
		vd_layer[0].enabled_status_saved = 1; \
		spin_unlock_irqrestore(&video_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video_onoff_lock, flags); \
		vd_layer[0].onoff_state = VIDEO_ENABLE_STATE_OFF_REQ; \
		vd_layer[0].enabled = 0; \
		vd_layer[0].enabled_status_saved = 0; \
		spin_unlock_irqrestore(&video_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER2_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video2_onoff_lock, flags); \
		if (vd_layer[1].onoff_state != VIDEO_ENABLE_STATE_ON_PENDING) \
			vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_ON_REQ; \
		vd_layer[1].enabled = 1; \
		vd_layer[1].enabled_status_saved = 1; \
		spin_unlock_irqrestore(&video2_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER2_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video2_onoff_lock, flags); \
		vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_OFF_REQ; \
		vd_layer[1].enabled = 0; \
		vd_layer[1].enabled_status_saved = 0; \
		spin_unlock_irqrestore(&video2_onoff_lock, flags); \
	} while (0)

#define enable_video_layer()  \
	do { \
		VD1_MEM_POWER_ON(); \
		VIDEO_LAYER_ON(); \
	} while (0)

#define enable_video_layer2()  \
	do { \
		VD2_MEM_POWER_ON(); \
		VIDEO_LAYER2_ON(); \
	} while (0)

#define disable_video_layer(async) \
	do { \
		if (!(async)) { \
			CLEAR_VCBUS_REG_MASK( \
				VPP_MISC + cur_dev->vpp_off, \
				VPP_VD1_PREBLEND | \
				VPP_VD1_POSTBLEND); \
			if (!legacy_vpp) \
				WRITE_VCBUS_REG( \
					VD1_BLEND_SRC_CTRL + \
					vd_layer[0].misc_reg_offt, 0); \
			WRITE_VCBUS_REG( \
				AFBC_ENABLE + \
				vd_layer[0].afbc_reg_offt, 0); \
			WRITE_VCBUS_REG( \
				VD1_IF0_GEN_REG + \
				vd_layer[0].vd_reg_offt, 0); \
		} \
		VIDEO_LAYER_OFF(); \
		VD1_MEM_POWER_OFF(); \
		if (vd_layer[0].global_debug & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("VIDEO: DisableVideoLayer()\n"); \
		} \
	} while (0)

#define disable_video_layer2(async) \
	do { \
		if (!(async)) { \
			CLEAR_VCBUS_REG_MASK( \
				VPP_MISC + cur_dev->vpp_off, \
				VPP_VD2_POSTBLEND | \
				VPP_VD2_PREBLEND); \
			if (!legacy_vpp) \
				WRITE_VCBUS_REG( \
					VD2_BLEND_SRC_CTRL + \
					vd_layer[1].misc_reg_offt, 0); \
			WRITE_VCBUS_REG( \
				VD2_AFBC_ENABLE + \
				vd_layer[1].afbc_reg_offt, 0); \
			WRITE_VCBUS_REG( \
				VD2_IF0_GEN_REG + \
				vd_layer[1].vd_reg_offt, 0); \
		} \
		VIDEO_LAYER2_OFF(); \
		VD2_MEM_POWER_OFF(); \
		if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("VIDEO: DisableVideoLayer2()\n"); \
		} \
	} while (0)

#define disable_video_all_layer_nodelay() \
	do { \
		CLEAR_VCBUS_REG_MASK( \
			VPP_MISC + cur_dev->vpp_off, \
			VPP_VD1_PREBLEND | VPP_VD2_PREBLEND |\
			VPP_VD2_POSTBLEND | VPP_VD1_POSTBLEND); \
		if (!legacy_vpp) \
			WRITE_VCBUS_REG( \
				VD1_BLEND_SRC_CTRL + \
				vd_layer[0].misc_reg_offt, 0); \
		WRITE_VCBUS_REG( \
			AFBC_ENABLE + \
			vd_layer[0].afbc_reg_offt, 0); \
		WRITE_VCBUS_REG( \
			VD1_IF0_GEN_REG + \
			vd_layer[0].vd_reg_offt, 0); \
		if (!legacy_vpp) \
			WRITE_VCBUS_REG( \
				VD2_BLEND_SRC_CTRL + \
				vd_layer[1].misc_reg_offt, 0); \
		WRITE_VCBUS_REG( \
			VD2_AFBC_ENABLE + \
			vd_layer[1].afbc_reg_offt, 0); \
		WRITE_VCBUS_REG( \
			VD2_IF0_GEN_REG + \
			vd_layer[1].vd_reg_offt, 0); \
		pr_info("VIDEO: disable_video_all_layer_nodelay()\n"); \
	} while (0)

void dv_mem_power_off(enum vpu_mod_e mode)
{
	unsigned long flags;
	unsigned int dv_flag;

	if (mode == VPU_DOLBY0)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0;
	else if (mode == VPU_DOLBY1A)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A;
	else if (mode == VPU_DOLBY1B)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B;
	else if (mode == VPU_DOLBY2)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2;
	else if (mode == VPU_DOLBY_CORE3)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3;
	else if (mode == VPU_PRIME_DOLBY_RAM)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY;
	else
		return;

	spin_lock_irqsave(&delay_work_lock, flags);
	vpu_delay_work_flag |= dv_flag;
	dv_mem_power_off_count = DV_MEM_POWEROFF_DELAY;
	spin_unlock_irqrestore(&delay_work_lock, flags);
}
EXPORT_SYMBOL(dv_mem_power_off);

void dv_mem_power_on(enum vpu_mod_e mode)
{
	unsigned long flags;
	unsigned int dv_flag;

	if (mode == VPU_DOLBY0)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0;
	else if (mode == VPU_DOLBY1A)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A;
	else if (mode == VPU_DOLBY1B)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B;
	else if (mode == VPU_DOLBY2)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2;
	else if (mode == VPU_DOLBY_CORE3)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3;
	else if (mode == VPU_PRIME_DOLBY_RAM)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY;
	else
		return;

	spin_lock_irqsave(&delay_work_lock, flags);
	vpu_delay_work_flag &= ~dv_flag;
	spin_unlock_irqrestore(&delay_work_lock, flags);
	switch_vpu_mem_pd_vmod(mode, VPU_MEM_POWER_ON);
}
EXPORT_SYMBOL(dv_mem_power_on);

int get_dv_mem_power_flag(enum vpu_mod_e mode)
{
	unsigned long flags;
	unsigned int dv_flag;
	int ret = VPU_MEM_POWER_ON;

	if (mode == VPU_DOLBY0)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0;
	else if (mode == VPU_DOLBY1A)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A;
	else if (mode == VPU_DOLBY1B)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B;
	else if (mode == VPU_DOLBY2)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2;
	else if (mode == VPU_DOLBY_CORE3)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3;
	else if (mode == VPU_PRIME_DOLBY_RAM)
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY;
	else
		return -1;

	spin_lock_irqsave(&delay_work_lock, flags);
	if (vpu_delay_work_flag & dv_flag)
		ret = VPU_MEM_POWER_DOWN;
	spin_unlock_irqrestore(&delay_work_lock, flags);
	return ret;
}
EXPORT_SYMBOL(get_dv_mem_power_flag);

/*********************************************************/
static struct vframe_pic_mode_s gpic_info[MAX_VD_LAYERS];
static u32 reference_zorder = 128;
static u32 vpp_hold_line = 8;

static const enum f2v_vphase_type_e vpp_phase_table[4][3] = {
	{F2V_P2IT, F2V_P2IB, F2V_P2P},	/* VIDTYPE_PROGRESSIVE */
	{F2V_IT2IT, F2V_IT2IB, F2V_IT2P},	/* VIDTYPE_INTERLACE_TOP */
	{F2V_P2IT, F2V_P2IB, F2V_P2P},
	{F2V_IB2IT, F2V_IB2IB, F2V_IB2P}	/* VIDTYPE_INTERLACE_BOTTOM */
};

static const u8 skip_tab[6] = { 0x24, 0x04, 0x68, 0x48, 0x28, 0x08 };

static bool video_mute_on;
/* 0: off, 1: vpp mute 2:dv mute */
static int video_mute_status;

/*********************************************************
 * Utils APIs
 *********************************************************/
#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
bool is_dolby_vision_enable(void)
{
	return false;
}

bool is_dolby_vision_on(void)
{
	return false;
}

bool is_dolby_vision_stb_mode(void)
{
	return false;
}

bool for_dolby_vision_certification(void)
{
	return false;
}
#endif

bool is_dovi_tv_on(void)
{
#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	return false;
#else
	return ((is_meson_txlx_package_962X() || is_meson_tm2_cpu()) &&
		!is_dolby_vision_stb_mode() && is_dolby_vision_on());
#endif
}

struct video_dev_s *get_video_cur_dev(void)
{
	return cur_dev;
}

u32 get_video_enabled(void)
{
	return vd_layer[0].enabled && vd_layer[0].global_output;
}
EXPORT_SYMBOL(get_video_enabled);

u32 get_videopip_enabled(void)
{
	return vd_layer[1].enabled && vd_layer[1].global_output;
}
EXPORT_SYMBOL(get_videopip_enabled);

bool is_di_on(void)
{
	bool ret = false;

	if (DI_POST_REG_RD(DI_IF1_GEN_REG) & 0x1)
		ret = true;
	return ret;
}

bool is_di_post_on(void)
{
	bool ret = false;

	if (DI_POST_REG_RD(DI_POST_CTRL) & 0x100)
		ret = true;
	return ret;
}

bool is_di_post_link_on(void)
{
	bool ret = false;

	if (DI_POST_REG_RD(DI_POST_CTRL) & 0x1000)
		ret = true;
	return ret;
}

bool is_afbc_enabled(u8 layer_id)
{
	struct video_layer_s *layer = NULL;
	u32 afbc_off, value;
	u16 AFBC_ENABLE_REG =
		(layer_id == 0) ?
		AFBC_ENABLE :
		VD2_AFBC_ENABLE;

	if (layer_id >= MAX_VD_LAYER)
		return -1;

	layer = &vd_layer[layer_id];
	afbc_off = layer->afbc_reg_offt;
	value = READ_VCBUS_REG(AFBC_ENABLE_REG + afbc_off);
	return (value & 0x100) ? true : false;
}

bool is_local_vf(struct vframe_s *vf)
{
	if (vf &&
	    ((vf == &vf_local) ||
	     (vf == &vf_local2) ||
	     (vf == &local_pip)))
		return true;
	return false;
}

static u32 is_crop_left_odd(struct vpp_frame_par_s *frame_par)
{
	int crop_left_odd;

	/* odd, not even aligned*/
	if (frame_par->VPP_hd_start_lines_ & 0x01)
		crop_left_odd = 1;
	else
		crop_left_odd = 0;
	return crop_left_odd;
}

/*********************************************************
 * vd HW APIs
 *********************************************************/
void safe_switch_videolayer(u8 layer_id, bool on, bool async)
{
	if (layer_id == 0xff)
		pr_debug("VID: VD all %s with async %s\n",
			 on ? "on" : "off",
			 async ? "true" : "false");
	else
		pr_debug("VID: VD%d %s with async %s\n",
			 layer_id,
			 on ? "on" : "off",
			 async ? "true" : "false");

	if (layer_id == 0) {
		if (on)
			enable_video_layer();
		else
			disable_video_layer(async);
	}

	if (layer_id == 1) {
		if (on)
			enable_video_layer2();
		else
			disable_video_layer2(async);
	}

	if (layer_id == 0xff) {
		if (on) {
			enable_video_layer();
			enable_video_layer2();
		} else  if (async) {
			disable_video_layer(async);
			disable_video_layer2(async);
		} else {
			disable_video_all_layer_nodelay();
		}
	}
}

static void vd1_path_select(
	struct video_layer_s *layer,
	bool afbc, bool di_afbc)
{
	u32 misc_off = layer->misc_reg_offt;

	if (!legacy_vpp) {
		VSYNC_WR_MPEG_REG_BITS(
			VD1_AFBCD0_MISC_CTRL,
			/* go field sel */
			(0 << 20) |
			/* linebuffer en */
			(0 << 16) |
			/* vd1 -> dolby -> vpp top */
			(0 << 14) |
			/* axi sel: vd1 mif or afbc */
			(((afbc || di_afbc) ? 1 : 0) << 12) |
			/* data sel: vd1 & afbc0 (not osd4) */
			(0 << 11) |
			/* data sel: afbc0 or vd1 */
			((afbc ? 1 : 0) << 10) |
			/* afbc0 to vd1 (not di) */
			((di_afbc ? 1 : 0) << 9) |
			/* vd1 mif to vpp (not di) */
			(0 << 8) |
			/* afbc0 gclk ctrl */
			(0 << 0),
			0, 22);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			VSYNC_WR_MPEG_REG_BITS(
				VD1_AFBCD0_MISC_CTRL,
				/* Vd1_afbc0_mem_sel */
				(afbc ? 1 : 0),
				22, 1);
		if (((glayer_info[0].display_path_id
		      == VFM_PATH_AMVIDEO) ||
		     (glayer_info[0].display_path_id
		      == VFM_PATH_DEF)) &&
		    is_di_post_on()) {
			/* check di_vpp_out_en bit */
			VSYNC_WR_MPEG_REG_BITS(
				VD1_AFBCD0_MISC_CTRL,
				/* vd1 mif to di */
				1,
				8, 2);
			VSYNC_WR_MPEG_REG_BITS(
				VD1_AFBCD0_MISC_CTRL,
				/* go field select di post */
				1,
				20, 2);
		}
	} else {
		if (!is_di_post_on())
			VSYNC_WR_MPEG_REG_BITS(
				VIU_MISC_CTRL0 + misc_off,
				0, 16, 3);
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL0 + misc_off,
			(afbc ? 1 : 0), 20, 1);
	}
}

static void vd2_path_select(
	struct video_layer_s *layer,
	bool afbc, bool di_afbc)
{
	u32 misc_off = layer->misc_reg_offt;

	if (!legacy_vpp) {
		VSYNC_WR_MPEG_REG_BITS(
			VD2_AFBCD1_MISC_CTRL,
			/* go field sel */
			(0 << 20) |
			/* linebuffer en */
			(0 << 16) |
			/* TODO: vd2 -> dolby -> vpp top ?? */
			(0 << 14) |
			/* axi sel: vd2 mif */
			(((afbc || di_afbc) ? 1 : 0) << 12) |
			/* data sel: vd2 & afbc1 (not osd4) */
			(0 << 11) |
			/* data sel: afbc1 */
			((afbc ? 1 : 0) << 10) |
			/* afbc1 to vd2 (not di) */
			((di_afbc ? 1 : 0)  << 9) |
			/* vd2 mif to vpp (not di) */
			(0 << 8) |
			/* afbc1 gclk ctrl */
			(0 << 0),
			0, 22);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			VSYNC_WR_MPEG_REG_BITS(
				VD2_AFBCD1_MISC_CTRL,
				/* Vd2_afbc0_mem_sel */
				(afbc ? 1 : 0),
				22, 1);
	} else {
		VSYNC_WR_MPEG_REG_BITS(
			VIU_MISC_CTRL1 + misc_off,
			(afbc ? 2 : 0), 0, 2);
	}
}

static void vd1_set_dcu(
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf)
{
	u32 r;
	u32 vphase, vini_phase, vformatter, vrepeat, hphase = 0;
	u32 hformatter;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type, bit_mode = 0, canvas_w;
	bool is_mvc = false;
	u8 burst_len = 1;
	u32 vd_off = layer->vd_reg_offt;
	u32 afbc_off = layer->afbc_reg_offt;

	if (!vf) {
		pr_info("vd1_set_dcu vf NULL, return\n");
		return;
	}

	type = vf->type;
	if (type & VIDTYPE_MVC)
		is_mvc = true;

	pr_debug("%s for vd%d %p, type:0x%x\n",
		 __func__, layer->layer_id, vf, type);

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (!legacy_vpp || is_meson_txlx_cpu())
			burst_len = 2;
		r = (3 << 24) |
			(vpp_hold_line << 16) |
			(burst_len << 14) | /* burst1 */
			(vf->bitdepth & BITDEPTH_MASK);

		if (frame_par->hscale_skip_count)
			r |= 0x33;
		if (frame_par->vscale_skip_count)
			r |= 0xcc;

		/* FIXME: don't use glayer_info[0].reverse */
		if (glayer_info[0].reverse)
			r |= (1 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		VSYNC_WR_MPEG_REG(AFBC_MODE + afbc_off, r);

		r = 0x1700;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (vf && (vf->source_type
				!= VFRAME_SOURCE_TYPE_HDMI))
				r |= (1 << 19); /* dos_uncomp */

			if (type & VIDTYPE_COMB_MODE)
				r |= (1 << 20);
		}
		VSYNC_WR_MPEG_REG(AFBC_ENABLE + afbc_off, r);

		r = 0x100;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_VIU_444)
				r |= 0;
			else if (type & VIDTYPE_VIU_422)
				r |= (1 << 12);
			else
				r |= (2 << 12);
		}
		VSYNC_WR_MPEG_REG(AFBC_CONV_CTRL + afbc_off, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		VSYNC_WR_MPEG_REG(
			AFBC_DEC_DEF_COLOR + afbc_off,
			0x3FF00000 | /*Y,bit20+*/
			0x80 << (u + 10) |
			0x80 << v);
		/* chroma formatter */
		r = HFORMATTER_REPEAT |
			HFORMATTER_YC_RATIO_2_1 |
			HFORMATTER_EN |
			(0x8 << VFORMATTER_PHASE_BIT) |
			VFORMATTER_EN;
		if (is_dovi_tv_on())
			r |= VFORMATTER_ALWAYS_RPT |
				(0x0 << VFORMATTER_INIPHASE_BIT);
		else
			r |= VFORMATTER_RPTLINE0_EN |
				(0xc << VFORMATTER_INIPHASE_BIT);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_VIU_444) {
				r &= ~HFORMATTER_EN;
				r &= ~VFORMATTER_EN;
				r &= ~HFORMATTER_YC_RATIO_2_1;
			} else if (type & VIDTYPE_VIU_422) {
				r &= ~VFORMATTER_EN;
			}
		}
		VSYNC_WR_MPEG_REG(AFBC_VD_CFMT_CTRL + afbc_off, r);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_COMPRESS_LOSS)
				VSYNC_WR_MPEG_REG(
					AFBCDEC_IQUANT_ENABLE + afbc_off,
					((1 << 11) |
					(1 << 10) |
					(1 << 4) |
					(1 << 0)));
			else
				VSYNC_WR_MPEG_REG(
					AFBCDEC_IQUANT_ENABLE + afbc_off, 0);
		}
		vd1_path_select(layer, true, false);
		if (is_mvc)
			vd2_path_select(layer, true, false);
		VSYNC_WR_MPEG_REG(
			VD1_IF0_GEN_REG + vd_off, 0);
		return;
	}

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width(
			vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;

	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !frame_par->nocomp) {
		if (vf->type & VIDTYPE_VIU_444) {
			bit_mode = 2;
		} else {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				bit_mode = 3;
			else
				bit_mode = 1;
		}
	} else {
		bit_mode = 0;
	}
	if (!legacy_vpp) {
		VSYNC_WR_MPEG_REG_BITS(
			G12_VD1_IF0_GEN_REG3,
			(bit_mode & 0x3), 8, 2);
		VSYNC_WR_MPEG_REG_BITS(
			G12_VD1_IF0_GEN_REG3,
			(burst_len & 0x3), 1, 2);
		if (is_mvc) {
			VSYNC_WR_MPEG_REG_BITS(
				G12_VD2_IF0_GEN_REG3,
				(bit_mode & 0x3), 8, 2);
			VSYNC_WR_MPEG_REG_BITS(
				G12_VD2_IF0_GEN_REG3,
				(burst_len & 0x3), 1, 2);
		}
	} else {
		VSYNC_WR_MPEG_REG_BITS(
			VD1_IF0_GEN_REG3 +
			vd_off,
			(bit_mode & 0x3), 8, 2);
		if (is_mvc)
			VSYNC_WR_MPEG_REG_BITS(
				VD2_IF0_GEN_REG3 +
				vd_off,
				(bit_mode & 0x3), 8, 2);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	DI_POST_WR_REG_BITS(
		DI_IF1_GEN_REG3,
		(bit_mode & 0x3), 8, 2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		DI_POST_WR_REG_BITS(
			DI_IF2_GEN_REG3,
			(bit_mode & 0x3), 8, 2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		DI_POST_WR_REG_BITS(
			DI_IF0_GEN_REG3,
			(bit_mode & 0x3), 8, 2);
#endif
	if (glayer_info[0].need_no_compress ||
	    (vf->type & VIDTYPE_PRE_DI_AFBC)) {
		vd1_path_select(layer, false, true);
	} else {
		vd1_path_select(layer, false, false);
		if (is_mvc)
			vd2_path_select(layer, false, false);
		VSYNC_WR_MPEG_REG(
			AFBC_ENABLE + afbc_off, 0);
	}

	r = (3 << VDIF_URGENT_BIT) |
		(vpp_hold_line << VDIF_HOLD_LINES_BIT) |
		VDIF_FORMAT_SPLIT |
		VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (layer->global_debug & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1 << 7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0) {
		r |= VDIF_SEPARATE_EN;
	} else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
	}

	if (frame_par->hscale_skip_count) {
		if ((type & VIDTYPE_VIU_444) || (type & VIDTYPE_RGB_444))
			r |= VDIF_LUMA_HZ_AVG;
		else
			r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;
	}

	/*enable go field reset default according to vlsi*/
	r |= VDIF_RESET_ON_GO_FIELD;
	VSYNC_WR_MPEG_REG(VD1_IF0_GEN_REG + vd_off, r);
	if (is_mvc)
		VSYNC_WR_MPEG_REG(
			VD2_IF0_GEN_REG + vd_off, r);

	if (type & VIDTYPE_VIU_NV21)
		VSYNC_WR_MPEG_REG_BITS(
			VD1_IF0_GEN_REG2 +
			vd_off, 1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		VSYNC_WR_MPEG_REG_BITS(
			VD1_IF0_GEN_REG2 +
			vd_off, 2, 0, 2);
	else
		VSYNC_WR_MPEG_REG_BITS(
			VD1_IF0_GEN_REG2 +
			vd_off, 0, 0, 2);

	/* FIXME: don't use glayer_info[0].reverse */
	if (glayer_info[0].reverse) {
		VSYNC_WR_MPEG_REG_BITS(
			VD1_IF0_GEN_REG2 + vd_off, 0xf, 2, 4);
		if (is_mvc)
			VSYNC_WR_MPEG_REG_BITS(
				VD2_IF0_GEN_REG2 + vd_off, 0xf, 2, 4);
	} else {
		VSYNC_WR_MPEG_REG_BITS(
			VD1_IF0_GEN_REG2 + vd_off, 0, 2, 4);
		if (is_mvc)
			VSYNC_WR_MPEG_REG_BITS(
				VD2_IF0_GEN_REG2 + vd_off, 0, 2, 4);
	}

	/* chroma formatter */
	if (type & VIDTYPE_VIU_444) {
		r = HFORMATTER_YC_RATIO_1_1;
		VSYNC_WR_MPEG_REG(
			VIU_VD1_FMT_CTRL + vd_off, r);
		if (is_mvc)
			VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + vd_off, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if (!(type & VIDTYPE_PRE_INTERLACE) &&
			    ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			     (type & VIDTYPE_VIU_422)))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if ((frame_par->hscale_skip_count) &&
		    (type & VIDTYPE_VIU_422))
			hformatter =
			(HFORMATTER_YC_RATIO_2_1 |
			hphase);
		else
			hformatter =
				(HFORMATTER_YC_RATIO_2_1 |
				hphase |
				HFORMATTER_EN);
		if (is_dolby_vision_on())
			hformatter |= HFORMATTER_REPEAT;
		vrepeat = VFORMATTER_RPTLINE0_EN;
		vini_phase = (0xc << VFORMATTER_INIPHASE_BIT);
		if (type & VIDTYPE_VIU_422) {
			vformatter = 0;
			vphase = (0x10 << VFORMATTER_PHASE_BIT);
		} else {
			/*vlsi suggest only for yuv420 vformatter should be 1*/
			vformatter = VFORMATTER_EN;
			vphase = (0x08 << VFORMATTER_PHASE_BIT);
		}
		if (is_dovi_tv_on()) {
			/* dolby vision tv mode */
			vini_phase = (0 << VFORMATTER_INIPHASE_BIT);
			/* TODO: check the vrepeat */
			if (type & VIDTYPE_VIU_422)
				vrepeat = VFORMATTER_RPTLINE0_EN;
			else
				vrepeat = VFORMATTER_ALWAYS_RPT;
		} else if (is_mvc) {
			/* mvc source */
			vini_phase = (0xe << VFORMATTER_INIPHASE_BIT);
		} else if (type & VIDTYPE_TYPEMASK) {
			/* interlace source */
			if ((type & VIDTYPE_TYPEMASK) ==
			     VIDTYPE_INTERLACE_TOP)
				vini_phase =
					(0xe << VFORMATTER_INIPHASE_BIT);
			else
				vini_phase =
					(0xa << VFORMATTER_INIPHASE_BIT);
		}
		vformatter |= (vphase | vini_phase | vrepeat);

		VSYNC_WR_MPEG_REG(
			VIU_VD1_FMT_CTRL + vd_off,
			hformatter | vformatter);

		if (is_mvc) {
			vini_phase = (0xa << VFORMATTER_INIPHASE_BIT);
			vformatter = (vphase | vini_phase
				| vrepeat | VFORMATTER_EN);
			VSYNC_WR_MPEG_REG(
				VIU_VD2_FMT_CTRL + vd_off,
				hformatter | vformatter);
		}
	}

	if (is_meson_txlx_cpu()	||
	    cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_VD1_FMT_CTRL + vd_off,
			1, 29, 1);

	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if (is_mvc) {
		loop = 0x11;
		if (framepacking_support)
			pat = 0;
		else
			pat = 0x80;
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else {
		loop = 0;
	}

	VSYNC_WR_MPEG_REG(
		VD1_IF0_RPT_LOOP + vd_off,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));
	if (is_mvc)
		VSYNC_WR_MPEG_REG(
			VD2_IF0_RPT_LOOP + vd_off,
			(loop << VDIF_CHROMA_LOOP1_BIT) |
			(loop << VDIF_LUMA_LOOP1_BIT) |
			(loop << VDIF_CHROMA_LOOP0_BIT) |
			(loop << VDIF_LUMA_LOOP0_BIT));

	VSYNC_WR_MPEG_REG(VD1_IF0_LUMA0_RPT_PAT + vd_off, pat);
	VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA0_RPT_PAT + vd_off, pat);
	VSYNC_WR_MPEG_REG(VD1_IF0_LUMA1_RPT_PAT + vd_off, pat);
	VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA1_RPT_PAT + vd_off, pat);

	if (is_mvc) {
		if (framepacking_support)
			pat = 0;
		else
			pat = 0x88;
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA0_RPT_PAT + vd_off, pat);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA0_RPT_PAT + vd_off, pat);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA1_RPT_PAT + vd_off, pat);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA1_RPT_PAT + vd_off, pat);
	}

	/* picture 0/1 control */
	if ((((type & VIDTYPE_INTERLACE) == 0) &&
	     ((type & VIDTYPE_VIU_FIELD) == 0) &&
	     ((type & VIDTYPE_MVC) == 0)) ||
	    (frame_par->vpp_2pic_mode & 0x3)) {
		/* progressive frame in two pictures */
		r = ((2 << 26) | /* two pic mode */
			(2 << 24) | /* use own last line */
			0x01); /* loop pattern */
		if (frame_par->vpp_2pic_mode & VPP_PIC1_FIRST)
			r |= (1 << 8); /* use pic1 first*/
		else
			r |= (2 << 8);	 /* use pic0 first */
		VSYNC_WR_MPEG_REG(
			VD1_IF0_LUMA_PSEL + vd_off, r);
		VSYNC_WR_MPEG_REG(
			VD1_IF0_CHROMA_PSEL + vd_off, r);
	} else if (process_3d_type & MODE_3D_OUT_FA_MASK) {
		/*FA LR/TB output , do nothing*/
	} else {
		VSYNC_WR_MPEG_REG(
			VD1_IF0_LUMA_PSEL + vd_off, 0);
		VSYNC_WR_MPEG_REG(
			VD1_IF0_CHROMA_PSEL + vd_off, 0);
		if (is_mvc) {
			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_PSEL + vd_off, 0);
			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_PSEL + vd_off, 0);
		}
	}
}

static void vd2_set_dcu(
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf)
{
	u32 r;
	u32 vphase, vini_phase, vformatter, vrepeat, hphase = 0;
	u32 hformatter;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type, bit_mode = 0, canvas_w;
	bool is_mvc = false;
	u8 burst_len = 1;
	u32 vd_off = layer->vd_reg_offt;
	u32 afbc_off = layer->afbc_reg_offt;

	if (!vf) {
		pr_err("vd2_set_dcu vf is NULL\n");
		return;
	}

	type = vf->type;
	if (type & VIDTYPE_MVC)
		is_mvc = true;
	pr_debug("%s for vd%d %p, type:0x%x\n",
		 __func__, layer->layer_id, vf, type);

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (!legacy_vpp || is_meson_txlx_cpu())
			burst_len = 2;
		r = (3 << 24) |
		    (vpp_hold_line << 16) |
		    (burst_len << 14) | /* burst1 */
		    (vf->bitdepth & BITDEPTH_MASK);

		if (frame_par->hscale_skip_count)
			r |= 0x33;
		if (frame_par->vscale_skip_count)
			r |= 0xcc;

		/* FIXME: don't use glayer_info[1].reverse */
		if (glayer_info[1].reverse)
			r |= (1 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		VSYNC_WR_MPEG_REG(VD2_AFBC_MODE + afbc_off, r);

		r = 0x1700;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (vf && (vf->source_type
			    != VFRAME_SOURCE_TYPE_HDMI))
				r |= (1 << 19); /* dos_uncomp */
		}
		VSYNC_WR_MPEG_REG(VD2_AFBC_ENABLE + afbc_off, r);

		r = 0x100;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_VIU_444)
				r |= 0;
			else if (type & VIDTYPE_VIU_422)
				r |= (1 << 12);
			else
				r |= (2 << 12);
		}
		VSYNC_WR_MPEG_REG(VD2_AFBC_CONV_CTRL + afbc_off, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		VSYNC_WR_MPEG_REG(
			VD2_AFBC_DEC_DEF_COLOR,
			0x3FF00000 | /*Y,bit20+*/
			0x80 << (u + 10) |
			0x80 << v);

		/* chroma formatter */
		r = HFORMATTER_YC_RATIO_2_1 |
			HFORMATTER_EN |
			(0x8 << VFORMATTER_PHASE_BIT) |
			VFORMATTER_EN;
		if (is_dovi_tv_on())
			r |= HFORMATTER_REPEAT |
				VFORMATTER_ALWAYS_RPT |
				(0x0 << VFORMATTER_INIPHASE_BIT);
		else if (is_dolby_vision_on()) /* stb case */
			r |= HFORMATTER_REPEAT |
				VFORMATTER_RPTLINE0_EN |
				(0xc << VFORMATTER_INIPHASE_BIT);
		else
			r |= HFORMATTER_RRT_PIXEL0 |
				VFORMATTER_RPTLINE0_EN |
				(0 << VFORMATTER_INIPHASE_BIT);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_VIU_444) {
				r &= ~HFORMATTER_EN;
				r &= ~VFORMATTER_EN;
				r &= ~HFORMATTER_YC_RATIO_2_1;
			} else if (type & VIDTYPE_VIU_422) {
				r &= ~VFORMATTER_EN;
			}
		}
		VSYNC_WR_MPEG_REG(
			VD2_AFBC_VD_CFMT_CTRL + afbc_off, r);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_COMPRESS_LOSS)
				VSYNC_WR_MPEG_REG(
				VD2_AFBCDEC_IQUANT_ENABLE + afbc_off,
				((1 << 11) |
				(1 << 10) |
				(1 << 4) |
				(1 << 0)));
			else
				VSYNC_WR_MPEG_REG(
					VD2_AFBCDEC_IQUANT_ENABLE
					+ afbc_off, 0);
		}

		vd2_path_select(layer, true, false);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_GEN_REG + vd_off, 0);
		return;
	}

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width(
			vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;

	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !frame_par->nocomp) {
		if (vf->type & VIDTYPE_VIU_444) {
			bit_mode = 2;
		} else {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				bit_mode = 3;
			else
				bit_mode = 1;
		}
	} else {
		bit_mode = 0;
	}

	vd2_path_select(layer, false, false);
	if (!legacy_vpp) {
		VSYNC_WR_MPEG_REG_BITS(
			G12_VD2_IF0_GEN_REG3,
			(bit_mode & 0x3), 8, 2);
		VSYNC_WR_MPEG_REG_BITS(
			G12_VD2_IF0_GEN_REG3,
			(burst_len & 0x3), 1, 2);
	} else {
		VSYNC_WR_MPEG_REG_BITS(
			VD2_IF0_GEN_REG3 + vd_off,
			(bit_mode & 0x3), 8, 2);
	}
	if (!(VSYNC_RD_MPEG_REG(VIU_MISC_CTRL1) & 0x1))
		VSYNC_WR_MPEG_REG(
			VD2_AFBC_ENABLE + afbc_off, 0);

	r = (3 << VDIF_URGENT_BIT) |
		(vpp_hold_line << VDIF_HOLD_LINES_BIT) |
		VDIF_FORMAT_SPLIT |
		VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (layer->global_debug & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1 << 7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0) {
		r |= VDIF_SEPARATE_EN;
	} else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
	}

	if (frame_par->hscale_skip_count) {
		if ((type & VIDTYPE_VIU_444) || (type & VIDTYPE_RGB_444))
			r |= VDIF_LUMA_HZ_AVG;
		else
			r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;
	}

	VSYNC_WR_MPEG_REG(VD2_IF0_GEN_REG + vd_off, r);

	if (type & VIDTYPE_VIU_NV21)
		VSYNC_WR_MPEG_REG_BITS(
			VD2_IF0_GEN_REG2 +
			vd_off, 1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		VSYNC_WR_MPEG_REG_BITS(
			VD2_IF0_GEN_REG2 +
			vd_off, 2, 0, 2);
	else
		VSYNC_WR_MPEG_REG_BITS(
			VD2_IF0_GEN_REG2 +
			vd_off, 0, 0, 2);

	/* FIXME: don't use glayer_info[1].reverse */
	if (glayer_info[1].reverse)
		VSYNC_WR_MPEG_REG_BITS(
			VD2_IF0_GEN_REG2 +
			vd_off, 0xf, 2, 4);
	else
		VSYNC_WR_MPEG_REG_BITS(
			VD2_IF0_GEN_REG2 +
			vd_off, 0, 2, 4);

	/* chroma formatter */
	if (type & VIDTYPE_VIU_444) {
		r = HFORMATTER_YC_RATIO_1_1;
		VSYNC_WR_MPEG_REG(
			VIU_VD2_FMT_CTRL + vd_off, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if (!(type & VIDTYPE_PRE_INTERLACE) &&
			    ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			     (type & VIDTYPE_VIU_422)))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if ((frame_par->hscale_skip_count) &&
		    (type & VIDTYPE_VIU_422))
			hformatter =
			(HFORMATTER_YC_RATIO_2_1 |
			hphase);
		else
			hformatter =
				(HFORMATTER_YC_RATIO_2_1 |
				hphase |
				HFORMATTER_EN);
		if (is_dolby_vision_on())
			hformatter |= HFORMATTER_REPEAT;
		vrepeat = VFORMATTER_RPTLINE0_EN;
		vini_phase = (0xc << VFORMATTER_INIPHASE_BIT);
		if (type & VIDTYPE_VIU_422) {
			vformatter = 0;
			vphase = (0x10 << VFORMATTER_PHASE_BIT);
		} else {
			/*vlsi suggest only for yuv420 vformatter should be 1*/
			vformatter = VFORMATTER_EN;
			vphase = (0x08 << VFORMATTER_PHASE_BIT);
		}
		if (is_dovi_tv_on()) {
			/* dolby vision tv mode */
			vini_phase = (0 << VFORMATTER_INIPHASE_BIT);
			/* TODO: check the vrepeat */
			if (type & VIDTYPE_VIU_422)
				vrepeat = VFORMATTER_RPTLINE0_EN;
			else
				vrepeat = VFORMATTER_ALWAYS_RPT;
		} else if (is_mvc) {
			/* mvc source */
			/* vini_phase = (0xe << VFORMATTER_INIPHASE_BIT); */
			vini_phase = (0xa << VFORMATTER_INIPHASE_BIT);
		} else if (type & VIDTYPE_TYPEMASK) {
			/* interlace source */
			if ((type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP)
				vini_phase =
					(0xe << VFORMATTER_INIPHASE_BIT);
			else
				vini_phase =
					(0xa << VFORMATTER_INIPHASE_BIT);
		}
		vformatter |= (vphase | vini_phase | vrepeat);

		VSYNC_WR_MPEG_REG(
			VIU_VD2_FMT_CTRL + vd_off,
			hformatter | vformatter);
	}
	if (is_meson_txlx_cpu()	||
	    cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		VSYNC_WR_MPEG_REG_BITS(
			VIU_VD2_FMT_CTRL + vd_off,
			1, 29, 1);
	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if (is_mvc) {
		loop = 0x11;
		pat = 0x80;
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else {
		loop = 0;
	}

	VSYNC_WR_MPEG_REG(
		VD2_IF0_RPT_LOOP + vd_off,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));

	if (is_mvc)
		pat = 0x88;

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA0_RPT_PAT + vd_off, pat);
	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA0_RPT_PAT + vd_off, pat);
	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA1_RPT_PAT + vd_off, pat);
	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA1_RPT_PAT + vd_off, pat);

	/* picture 0/1 control */
	if ((((type & VIDTYPE_INTERLACE) == 0) &&
	     ((type & VIDTYPE_VIU_FIELD) == 0) &&
	     ((type & VIDTYPE_MVC) == 0)) ||
	    (frame_par->vpp_2pic_mode & 0x3)) {
		/* progressive frame in two pictures */
	} else {
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA_PSEL + vd_off, 0);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA_PSEL + vd_off, 0);
	}
}

static s32 vd1_mif_setting(struct mif_pos_s *setting)
{
	u32 vd_off, afbc_off;
	u32 ls = 0, le = 0;
	u32 h_skip, v_skip, hc_skip, vc_skip;
	int l_aligned, r_aligned;
	int t_aligned, b_aligned, ori_t_aligned, ori_b_aligned;
	int content_w, content_l, content_r;

	if (!setting)
		return -1;

	vd_off = setting->vd_reg_offt;
	afbc_off = setting->afbc_reg_offt;
	h_skip = setting->h_skip + 1;
	v_skip = setting->v_skip + 1;
	hc_skip = setting->hc_skip;
	vc_skip = setting->vc_skip;

	/* vd horizontal setting */
	VSYNC_WR_MPEG_REG(
		VD1_IF0_LUMA_X0 + vd_off,
		(setting->l_hs_luma << VDIF_PIC_START_BIT) |
		(setting->l_he_luma << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD1_IF0_CHROMA_X0 + vd_off,
		(setting->l_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_he_chrm << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD1_IF0_LUMA_X1 + vd_off,
		(setting->r_hs_luma  << VDIF_PIC_START_BIT) |
		(setting->r_he_luma  << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD1_IF0_CHROMA_X1 + vd_off,
		(setting->r_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_he_chrm << VDIF_PIC_END_BIT));

	ls = setting->start_x_lines;
	le = setting->end_x_lines;
	VSYNC_WR_MPEG_REG(
		VIU_VD1_FMT_W + vd_off,
		(((le - ls + 1) / h_skip)
		<< VD1_FMT_LUMA_WIDTH_BIT) |
		(((le / 2 - ls / 2 + 1) / h_skip)
		<< VD1_FMT_CHROMA_WIDTH_BIT));

	/* vd vertical setting */
	VSYNC_WR_MPEG_REG(
		VD1_IF0_LUMA_Y0 + vd_off,
		(setting->l_vs_luma << VDIF_PIC_START_BIT) |
		(setting->l_ve_luma << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD1_IF0_CHROMA_Y0 + vd_off,
		(setting->l_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_ve_chrm << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD1_IF0_LUMA_Y1 + vd_off,
		(setting->r_vs_luma << VDIF_PIC_START_BIT) |
		(setting->r_ve_luma << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD1_IF0_CHROMA_Y1 + vd_off,
		(setting->r_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_ve_chrm << VDIF_PIC_END_BIT));

	if (setting->skip_afbc)
		goto SKIP_VD1_AFBC;

	/* afbc horizontal setting */
	if ((setting->start_x_lines > 0) ||
	    (setting->end_x_lines < (setting->src_w - 1))) {
		/* also 0 line as start */
		l_aligned = round_down(0, 32);
		r_aligned = round_up(
			(setting->src_w - 1) + 1, 32);
	} else {
		l_aligned = round_down(
			setting->start_x_lines, 32);
		r_aligned = round_up(
			setting->end_x_lines + 1, 32);
	}
	VSYNC_WR_MPEG_REG(
		AFBC_VD_CFMT_W + afbc_off,
		(((r_aligned - l_aligned) / h_skip) << 16) |
		((r_aligned / 2 - l_aligned / hc_skip) / h_skip));

	VSYNC_WR_MPEG_REG(
		AFBC_MIF_HOR_SCOPE + afbc_off,
		((l_aligned / 32) << 16) |
		((r_aligned / 32) - 1));

	if (setting->reverse) {
		content_w = setting->end_x_lines
			- setting->start_x_lines + 1;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			content_l = 0;
		else
			content_l = r_aligned - setting->end_x_lines - 1;
		content_r = content_l + content_w - 1;
		VSYNC_WR_MPEG_REG(
			AFBC_PIXEL_HOR_SCOPE + afbc_off,
			(((content_l << 16)) | content_r) / h_skip);
	} else {
		if (((process_3d_type & MODE_3D_FA) ||
		     (process_3d_type & MODE_FORCE_3D_FA_LR)) &&
		    (setting->vpp_3d_mode == 1)) {
			/* do nothing*/
		} else {
			VSYNC_WR_MPEG_REG(
				AFBC_PIXEL_HOR_SCOPE + afbc_off,
				(((setting->start_x_lines - l_aligned) << 16) |
				(setting->end_x_lines - l_aligned)) / h_skip);
		}
	}

	/* afbc vertical setting */
	t_aligned = round_down(setting->start_y_lines, 4);
	b_aligned = round_up(setting->end_y_lines + 1, 4);

	ori_t_aligned = round_down(0, 4);
	ori_b_aligned = round_up((setting->src_h - 1) + 1, 4);

	VSYNC_WR_MPEG_REG(
		AFBC_VD_CFMT_H + afbc_off,
		(b_aligned - t_aligned) / vc_skip / v_skip);

	if (((process_3d_type & MODE_3D_FA) ||
	     (process_3d_type & MODE_FORCE_3D_FA_TB)) &&
	    (setting->vpp_3d_mode == 2)) {
		int block_h;

		block_h = ori_b_aligned - ori_t_aligned;
		block_h = block_h / 8;
		if (toggle_3d_fa_frame == OUT_FA_B_FRAME) {
			VSYNC_WR_MPEG_REG(
				AFBC_MIF_VER_SCOPE,
				(((ori_t_aligned / 4) +  block_h) << 16) |
				((ori_b_aligned / 4)  - 1));
		} else {
			VSYNC_WR_MPEG_REG(
				AFBC_MIF_VER_SCOPE,
				((ori_t_aligned / 4) << 16) |
				((ori_t_aligned / 4)  + block_h - 1));
		}
	} else {
		VSYNC_WR_MPEG_REG(
			AFBC_MIF_VER_SCOPE,
			((t_aligned / 4) << 16) |
			((b_aligned / 4) - 1));
	}

	VSYNC_WR_MPEG_REG(
		AFBC_MIF_VER_SCOPE + afbc_off,
		((t_aligned / 4) << 16) |
		((b_aligned / 4) - 1));

	VSYNC_WR_MPEG_REG(
		AFBC_PIXEL_VER_SCOPE + afbc_off,
		((setting->start_y_lines - t_aligned) << 16) |
		(setting->end_y_lines - t_aligned));

	/* TODO: if need remove this limit */
	/* afbc pixel vertical output region must be */
	/* [0, end_y_lines - start_y_lines] */
	VSYNC_WR_MPEG_REG(
		AFBC_PIXEL_VER_SCOPE + afbc_off,
		(setting->end_y_lines -
		setting->start_y_lines));

	VSYNC_WR_MPEG_REG(
		AFBC_SIZE_IN + afbc_off,
		((r_aligned - l_aligned) << 16) |
		((ori_b_aligned - ori_t_aligned) & 0xffff));
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		VSYNC_WR_MPEG_REG(
			AFBC_SIZE_OUT + afbc_off,
			(((r_aligned - l_aligned) / h_skip) << 16) |
			(((b_aligned - t_aligned) / v_skip) & 0xffff));
	}
SKIP_VD1_AFBC:
	return 0;
}

static s32 vd2_mif_setting(struct mif_pos_s *setting)
{
	u32 vd_off, afbc_off;
	u32 ls = 0, le = 0;
	u32 h_skip, v_skip, hc_skip, vc_skip;
	int l_aligned, r_aligned;
	int t_aligned, b_aligned, ori_t_aligned, ori_b_aligned;
	int content_w, content_l, content_r;

	if (!setting)
		return -1;

	vd_off = setting->vd_reg_offt;
	afbc_off = setting->afbc_reg_offt;
	h_skip = setting->h_skip + 1;
	v_skip = setting->v_skip + 1;
	hc_skip = setting->hc_skip;
	vc_skip = setting->vc_skip;

	/* vd horizontal setting */
	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_X0 + vd_off,
		(setting->l_hs_luma << VDIF_PIC_START_BIT) |
		(setting->l_he_luma << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_X0 + vd_off,
		(setting->l_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_he_chrm << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_X1 + vd_off,
		(setting->r_hs_luma  << VDIF_PIC_START_BIT) |
		(setting->r_he_luma  << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_X1 + vd_off,
		(setting->r_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_he_chrm << VDIF_PIC_END_BIT));

	ls = setting->start_x_lines;
	le = setting->end_x_lines;
	VSYNC_WR_MPEG_REG(
		VIU_VD2_FMT_W + vd_off,
		(((le - ls + 1) / h_skip)
		<< VD1_FMT_LUMA_WIDTH_BIT) |
		(((le / 2 - ls / 2 + 1) / h_skip)
		<< VD1_FMT_CHROMA_WIDTH_BIT));

	/* vd vertical setting */
	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_Y0 + vd_off,
		(setting->l_vs_luma << VDIF_PIC_START_BIT) |
		(setting->l_ve_luma << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_Y0 + vd_off,
		(setting->l_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_ve_chrm << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_LUMA_Y1 + vd_off,
		(setting->r_vs_luma << VDIF_PIC_START_BIT) |
		(setting->r_ve_luma << VDIF_PIC_END_BIT));

	VSYNC_WR_MPEG_REG(
		VD2_IF0_CHROMA_Y1 + vd_off,
		(setting->r_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_ve_chrm << VDIF_PIC_END_BIT));

	if (setting->skip_afbc)
		goto SKIP_VD2_AFBC;

	/* afbc horizontal setting */
	if ((setting->start_x_lines > 0) ||
	    (setting->end_x_lines < (setting->src_w - 1))) {
		/* also 0 line as start */
		l_aligned = round_down(0, 32);
		r_aligned = round_up(
			(setting->src_w - 1) + 1, 32);
	} else {
		l_aligned = round_down(
			setting->start_x_lines, 32);
		r_aligned = round_up(
			setting->end_x_lines + 1, 32);
	}
	VSYNC_WR_MPEG_REG(
		VD2_AFBC_VD_CFMT_W + afbc_off,
		(((r_aligned - l_aligned) / h_skip) << 16) |
		((r_aligned / 2 - l_aligned / hc_skip) / h_skip));

	VSYNC_WR_MPEG_REG(
		VD2_AFBC_MIF_HOR_SCOPE + afbc_off,
		((l_aligned / 32) << 16) |
		((r_aligned / 32) - 1));

	if (setting->reverse) {
		content_w = setting->end_x_lines
			- setting->start_x_lines + 1;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			content_l = 0;
		else
			content_l = (r_aligned - setting->end_x_lines - 1)
				+ (setting->start_x_lines - l_aligned);
		/* TODO: content_l = r_aligned - setting->end_x_lines - 1; */
		content_r = content_l + content_w - 1;
		VSYNC_WR_MPEG_REG(
			VD2_AFBC_PIXEL_HOR_SCOPE + afbc_off,
			(((content_l << 16)) | content_r) / h_skip);
	} else {
		VSYNC_WR_MPEG_REG(
			VD2_AFBC_PIXEL_HOR_SCOPE + afbc_off,
			(((setting->start_x_lines - l_aligned) << 16) |
			(setting->end_x_lines - l_aligned)) / h_skip);
	}

	/* afbc vertical setting */
	t_aligned = round_down(setting->start_y_lines, 4);
	b_aligned = round_up(setting->end_y_lines + 1, 4);

	ori_t_aligned = round_down(0, 4);
	ori_b_aligned = round_up((setting->src_h - 1) + 1, 4);

	VSYNC_WR_MPEG_REG(
		VD2_AFBC_VD_CFMT_H + afbc_off,
		(b_aligned - t_aligned) / vc_skip / v_skip);

	VSYNC_WR_MPEG_REG(
		VD2_AFBC_MIF_VER_SCOPE + afbc_off,
		((t_aligned / 4) << 16) |
		((b_aligned / 4) - 1));

	VSYNC_WR_MPEG_REG(
		VD2_AFBC_PIXEL_VER_SCOPE + afbc_off,
		((setting->start_y_lines - t_aligned) << 16) |
		(setting->end_y_lines - t_aligned));

	/* TODO: if need remove this limit */
	/* afbc pixel vertical output region must be */
	/* [0, end_y_lines - start_y_lines] */
	VSYNC_WR_MPEG_REG(
		VD2_AFBC_PIXEL_VER_SCOPE + afbc_off,
		(setting->end_y_lines - setting->start_y_lines));

	VSYNC_WR_MPEG_REG(
		VD2_AFBC_SIZE_IN + afbc_off,
		((r_aligned - l_aligned) << 16) |
		((ori_b_aligned - ori_t_aligned) & 0xffff));
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		VSYNC_WR_MPEG_REG(
			VD2_AFBC_SIZE_OUT + afbc_off,
			(((r_aligned - l_aligned) / h_skip) << 16) |
			(((b_aligned - t_aligned) / v_skip) & 0xffff));
	}
SKIP_VD2_AFBC:
	return 0;
}

static void vd1_scaler_setting(
	struct scaler_setting_s *setting)
{
	u32 misc_off, i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;

	if (!setting || !setting->frame_par)
		return;

	frame_par = setting->frame_par;
	misc_off = setting->misc_reg_offt;
	/* vpp super scaler */
	vpp_set_super_scaler_regs(
		frame_par->supscl_path,
		frame_par->supsc0_enable,
		frame_par->spsc0_w_in,
		frame_par->spsc0_h_in,
		frame_par->supsc0_hori_ratio,
		frame_par->supsc0_vert_ratio,
		frame_par->supsc1_enable,
		frame_par->spsc1_w_in,
		frame_par->spsc1_h_in,
		frame_par->supsc1_hori_ratio,
		frame_par->supsc1_vert_ratio,
		setting->vinfo_width,
		setting->vinfo_height);

	if (is_dolby_vision_on() &&
	    is_dolby_vision_stb_mode() &&
	    !frame_par->supsc0_enable &&
	    !frame_par->supsc1_enable) {
		VSYNC_WR_MPEG_REG(VPP_SRSHARP0_CTRL, 0);
		VSYNC_WR_MPEG_REG(VPP_SRSHARP1_CTRL, 0);
	}

	vpp_filter = &frame_par->vpp_filter;

	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		if (setting->sc_h_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_hsc_en & 1)
				<< VPP_SC_PREHORZ_EN_BIT)
				| VPP_SC_HORZ_EN);
			sc_misc_val |= ((vpp_filter->vpp_horz_coeff[0] & 7)
				<< VPP_SC_HBANK_LENGTH_BIT);
		}

		if (setting->sc_v_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_SC_PREVERT_EN_BIT)
				| VPP_SC_VERT_EN);
			sc_misc_val |= ((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_LINE_BUFFER_EN_BIT);
			sc_misc_val |= ((vpp_filter->vpp_vert_coeff[0] & 7)
				<< VPP_SC_VBANK_LENGTH_BIT);
		}

		if (setting->last_line_fix)
			sc_misc_val |= VPP_PPS_LAST_LINE_FIX;

		VSYNC_WR_MPEG_REG(
			VPP_SC_MISC + misc_off,
			sc_misc_val);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		VSYNC_WR_MPEG_REG_BITS(
			VPP_SC_MISC + misc_off,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
	}

	/* horitontal filter settings */
	if (setting->sc_h_enable) {
		if (vpp_filter->vpp_horz_coeff[1] & 0x8000) {
			VSYNC_WR_MPEG_REG(
				VPP_SCALE_COEF_IDX + misc_off,
				VPP_COEF_HORZ | VPP_COEF_9BIT);
		} else {
			VSYNC_WR_MPEG_REG(
				VPP_SCALE_COEF_IDX + misc_off,
				VPP_COEF_HORZ);
		}
		for (i = 0; i < (vpp_filter->vpp_horz_coeff[1] & 0xff); i++) {
			VSYNC_WR_MPEG_REG(
				VPP_SCALE_COEF + misc_off,
				vpp_filter->vpp_horz_coeff[i + 2]);
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;
		r2 = frame_par->VPP_hsc_linear_endp
			- frame_par->VPP_hsc_startp;
		r3 = frame_par->VPP_hsc_endp
			- frame_par->VPP_hsc_startp;

		if ((frame_par->supscl_path ==
		     CORE0_PPS_CORE1) ||
		    (frame_par->supscl_path ==
		     CORE1_AFTER_PPS) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_CORE1) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1))
			r3 >>= frame_par->supsc1_hori_ratio;
		if ((frame_par->supscl_path ==
		     CORE0_AFTER_PPS) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_CORE1) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1))
			r3 >>= frame_par->supsc0_hori_ratio;

		VSYNC_WR_MPEG_REG_BITS(
			VPP_HSC_PHASE_CTRL + misc_off,
			frame_par->VPP_hf_ini_phase_,
			VPP_HSC_TOP_INI_PHASE_BIT,
			VPP_HSC_TOP_INI_PHASE_WID);

		VSYNC_WR_MPEG_REG(
			VPP_HSC_REGION12_STARTP + misc_off,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION2_BIT));
		VSYNC_WR_MPEG_REG(
			VPP_HSC_REGION34_STARTP + misc_off,
			((r2 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));
		VSYNC_WR_MPEG_REG(
			VPP_HSC_REGION4_ENDP + misc_off, r3);

		VSYNC_WR_MPEG_REG(
			VPP_HSC_START_PHASE_STEP + misc_off,
			vpp_filter->vpp_hf_start_phase_step);

		VSYNC_WR_MPEG_REG(
			VPP_HSC_REGION1_PHASE_SLOPE + misc_off,
			vpp_filter->vpp_hf_start_phase_slope);

		VSYNC_WR_MPEG_REG(
			VPP_HSC_REGION3_PHASE_SLOPE + misc_off,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		VSYNC_WR_MPEG_REG_BITS(
			VPP_VSC_PHASE_CTRL + misc_off,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		VSYNC_WR_MPEG_REG(
			VPP_SCALE_COEF_IDX + misc_off,
			VPP_COEF_VERT);
		for (i = 0; i < vpp_filter->vpp_vert_coeff[1]; i++) {
			VSYNC_WR_MPEG_REG(
				VPP_SCALE_COEF + misc_off,
				vpp_filter->vpp_vert_coeff[i + 2]);
		}

		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			VSYNC_WR_MPEG_REG(
				VPP_SCALE_COEF_IDX + misc_off,
				VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
			for (i = 0; i < pcoeff[1]; i++)
				VSYNC_WR_MPEG_REG(
					VPP_SCALE_COEF + misc_off,
					pcoeff[i + 2]);
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		VSYNC_WR_MPEG_REG(
			VPP_VSC_REGION12_STARTP + misc_off, 0);

		VSYNC_WR_MPEG_REG(
			VPP_VSC_REGION34_STARTP + misc_off,
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));

		if ((frame_par->supscl_path ==
		     CORE0_PPS_CORE1) ||
		    (frame_par->supscl_path ==
		     CORE1_AFTER_PPS) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1))
			r1 >>= frame_par->supsc1_vert_ratio;
		if ((frame_par->supscl_path ==
		     CORE0_AFTER_PPS) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_CORE1) ||
		    (frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1))
			r1 >>= frame_par->supsc0_vert_ratio;

		VSYNC_WR_MPEG_REG(
			VPP_VSC_REGION4_ENDP + misc_off, r1);

		VSYNC_WR_MPEG_REG(
			VPP_VSC_START_PHASE_STEP + misc_off,
			vpp_filter->vpp_vsc_start_phase_step);
	}

	/*vpp input size setting*/
	VSYNC_WR_MPEG_REG(
		VPP_IN_H_V_SIZE + misc_off,
		((frame_par->video_input_w & 0x1fff) << 16)
		| (frame_par->video_input_h & 0x1fff));

	VSYNC_WR_MPEG_REG(
		VPP_PIC_IN_HEIGHT + misc_off,
		frame_par->VPP_pic_in_height_);

	VSYNC_WR_MPEG_REG(
		VPP_LINE_IN_LENGTH + misc_off,
		frame_par->VPP_line_in_length_);

	/* FIXME: if enable it */
#ifdef CHECK_LATER
	if (vd_layer[0].enabled == 1) {
		struct vppfilter_mode_s *vpp_filter =
		    &cur_frame_par->vpp_filter;
		u32 h_phase_step, v_phase_step;

		h_phase_step = READ_VCBUS_REG(
			VPP_HSC_START_PHASE_STEP + misc_off);
		v_phase_step = READ_VCBUS_REG(
			VPP_VSC_START_PHASE_STEP + misc_off);
		if ((vpp_filter->vpp_hf_start_phase_step != h_phase_step) ||
		    (vpp_filter->vpp_vsc_start_phase_step != v_phase_step))
			vd_layer[0].property_changed = true;
	}
#endif
}

static void vd2_scaler_setting(
	struct scaler_setting_s *setting)
{
	u32 misc_off, i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;

	if (!setting || !setting->frame_par)
		return;

	frame_par = setting->frame_par;
	misc_off = setting->misc_reg_offt;
	vpp_filter = &frame_par->vpp_filter;

	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		if (setting->sc_h_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_hsc_en & 1)
				<< VPP_SC_PREHORZ_EN_BIT)
				| VPP_SC_HORZ_EN);
			sc_misc_val |= ((vpp_filter->vpp_horz_coeff[0] & 7)
				<< VPP_SC_HBANK_LENGTH_BIT);
		}

		if (setting->sc_v_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_SC_PREVERT_EN_BIT)
				| VPP_SC_VERT_EN);
			sc_misc_val |= ((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_LINE_BUFFER_EN_BIT);
			sc_misc_val |= ((vpp_filter->vpp_vert_coeff[0] & 7)
				<< VPP_SC_VBANK_LENGTH_BIT);
		}

		if (setting->last_line_fix)
			sc_misc_val |= VPP_PPS_LAST_LINE_FIX;

		VSYNC_WR_MPEG_REG(
			VD2_SC_MISC + misc_off,
			sc_misc_val);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		VSYNC_WR_MPEG_REG_BITS(
			VD2_SC_MISC + misc_off,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
	}

	/* horitontal filter settings */
	if (setting->sc_h_enable) {
		if (vpp_filter->vpp_horz_coeff[1] & 0x8000) {
			VSYNC_WR_MPEG_REG(
				VD2_SCALE_COEF_IDX + misc_off,
				VPP_COEF_HORZ | VPP_COEF_9BIT);
		} else {
			VSYNC_WR_MPEG_REG(
				VD2_SCALE_COEF_IDX + misc_off,
				VPP_COEF_HORZ);
		}
		for (i = 0; i < (vpp_filter->vpp_horz_coeff[1] & 0xff); i++) {
			VSYNC_WR_MPEG_REG(
				VD2_SCALE_COEF + misc_off,
				vpp_filter->vpp_horz_coeff[i + 2]);
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;
		r2 = frame_par->VPP_hsc_linear_endp
			- frame_par->VPP_hsc_startp;
		r3 = frame_par->VPP_hsc_endp
			- frame_par->VPP_hsc_startp;

		VSYNC_WR_MPEG_REG_BITS(
			VD2_HSC_PHASE_CTRL + misc_off,
			frame_par->VPP_hf_ini_phase_,
			VPP_HSC_TOP_INI_PHASE_BIT,
			VPP_HSC_TOP_INI_PHASE_WID);

		VSYNC_WR_MPEG_REG(
			VD2_HSC_REGION12_STARTP + misc_off,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION2_BIT));
		VSYNC_WR_MPEG_REG(
			VD2_HSC_REGION34_STARTP + misc_off,
			((r2 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));
		VSYNC_WR_MPEG_REG(
			VD2_HSC_REGION4_ENDP + misc_off, r3);

		VSYNC_WR_MPEG_REG(
			VD2_HSC_START_PHASE_STEP + misc_off,
			vpp_filter->vpp_hf_start_phase_step);

		VSYNC_WR_MPEG_REG(
			VD2_HSC_REGION1_PHASE_SLOPE + misc_off,
			vpp_filter->vpp_hf_start_phase_slope);

		VSYNC_WR_MPEG_REG(
			VD2_HSC_REGION3_PHASE_SLOPE + misc_off,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		VSYNC_WR_MPEG_REG_BITS(
			VD2_VSC_PHASE_CTRL + misc_off,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		VSYNC_WR_MPEG_REG(
			VD2_SCALE_COEF_IDX + misc_off,
			VPP_COEF_VERT);
		for (i = 0; i < vpp_filter->vpp_vert_coeff[1]; i++) {
			VSYNC_WR_MPEG_REG(
				VD2_SCALE_COEF + misc_off,
				vpp_filter->vpp_vert_coeff[i + 2]);
		}

		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			VSYNC_WR_MPEG_REG(
				VD2_SCALE_COEF_IDX + misc_off,
				VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
			for (i = 0; i < pcoeff[1]; i++)
				VSYNC_WR_MPEG_REG(
					VD2_SCALE_COEF + misc_off,
					pcoeff[i + 2]);
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		VSYNC_WR_MPEG_REG(
			VD2_VSC_REGION12_STARTP + misc_off, 0);

		VSYNC_WR_MPEG_REG(
			VD2_VSC_REGION34_STARTP + misc_off,
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));

		VSYNC_WR_MPEG_REG(
			VD2_VSC_REGION4_ENDP + misc_off, r1);

		VSYNC_WR_MPEG_REG(
			VD2_VSC_START_PHASE_STEP + misc_off,
			vpp_filter->vpp_vsc_start_phase_step);
	}

	VSYNC_WR_MPEG_REG(
		VPP_VD2_HDR_IN_SIZE + misc_off,
		(frame_par->VPP_pic_in_height_ << 16)
		| frame_par->VPP_line_in_length_);
}

static void vd1_blend_setting(
	struct blend_setting_s *setting)
{
	u32 misc_off;
	u32 vd_size_mask = VPP_VD_SIZE_MASK;

	if (!setting)
		return;

	/* g12a change to 13 bits */
	if (!legacy_vpp)
		vd_size_mask = 0x1fff;

	misc_off = setting->misc_reg_offt;

	/* preblend setting */
	VSYNC_WR_MPEG_REG(
		VPP_PREBLEND_VD1_H_START_END + misc_off,
		((setting->preblend_h_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->preblend_h_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	VSYNC_WR_MPEG_REG(
		VPP_PREBLEND_VD1_V_START_END + misc_off,
		((setting->preblend_v_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->preblend_v_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	VSYNC_WR_MPEG_REG(
		VPP_PREBLEND_H_SIZE + misc_off,
		setting->preblend_h_size);

	/* postblend setting */
	VSYNC_WR_MPEG_REG(
		VPP_POSTBLEND_VD1_H_START_END + misc_off,
		((setting->postblend_h_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_h_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	VSYNC_WR_MPEG_REG(
		VPP_POSTBLEND_VD1_V_START_END + misc_off,
		((setting->postblend_v_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_v_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	if (!legacy_vpp)
		VSYNC_WR_MPEG_REG(
			VPP_MISC1 + misc_off,
			setting->layer_alpha);
}

static void vd2_blend_setting(
	struct blend_setting_s *setting)
{
	u32 misc_off;
	u32 vd_size_mask = VPP_VD_SIZE_MASK;

	if (!setting)
		return;

	/* g12a change to 13 bits */
	if (!legacy_vpp)
		vd_size_mask = 0x1fff;

	misc_off = 0;

	/* vd2 preblend size should be same postblend size */
	/* preblend setting */
	VSYNC_WR_MPEG_REG(
		VPP_BLEND_VD2_H_START_END + misc_off,
		((setting->postblend_h_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_h_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	VSYNC_WR_MPEG_REG(
		VPP_BLEND_VD2_V_START_END + misc_off,
		((setting->postblend_v_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_v_end & vd_size_mask)
		<< VPP_VD1_END_BIT));
}

static void proc_vd1_vsc_phase_per_vsync(
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf)
{
	struct f2v_vphase_s *vphase;
	u32 misc_off, vin_type;
	struct vppfilter_mode_s *vpp_filter;

	if (!layer || !frame_par || !vf)
		return;

	vpp_filter = &frame_par->vpp_filter;
	misc_off = layer->misc_reg_offt;
	vin_type = vf->type & VIDTYPE_TYPEMASK;
	/* vertical phase */
	vphase = &frame_par->VPP_vf_ini_phase_
		[vpp_phase_table[vin_type]
		[layer->vout_type]];
	VSYNC_WR_MPEG_REG(
		VPP_VSC_INI_PHASE + misc_off,
		((u32)(vphase->phase) << 8));

	if (vphase->repeat_skip >= 0) {
		/* skip lines */
		VSYNC_WR_MPEG_REG_BITS(
			VPP_VSC_PHASE_CTRL + misc_off,
			skip_tab[vphase->repeat_skip],
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID +
			VPP_PHASECTL_INIRPTNUM_WID);
	} else {
		/* repeat first line */
		VSYNC_WR_MPEG_REG_BITS(
			VPP_VSC_PHASE_CTRL + misc_off,
			4,
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID);
		VSYNC_WR_MPEG_REG_BITS(
			VPP_VSC_PHASE_CTRL + misc_off,
			1 - vphase->repeat_skip,
			VPP_PHASECTL_INIRPTNUMT_BIT,
			VPP_PHASECTL_INIRPTNUM_WID);
	}

	VSYNC_WR_MPEG_REG_BITS(
		VPP_VSC_PHASE_CTRL + misc_off,
		(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
		VPP_PHASECTL_DOUBLELINE_BIT,
		VPP_PHASECTL_DOUBLELINE_WID);
}

static void proc_vd2_vsc_phase_per_vsync(
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf)
{
	struct f2v_vphase_s *vphase;
	u32 misc_off, vin_type;
	struct vppfilter_mode_s *vpp_filter;

	if (!layer || !frame_par || !vf)
		return;

	vpp_filter = &frame_par->vpp_filter;
	misc_off = layer->misc_reg_offt;
	vin_type = vf->type & VIDTYPE_TYPEMASK;
	/* vertical phase */
	vphase = &frame_par->VPP_vf_ini_phase_
		[vpp_phase_table[vin_type]
		[layer->vout_type]];
	VSYNC_WR_MPEG_REG(
		VD2_VSC_INI_PHASE + misc_off,
		((u32)(vphase->phase) << 8));

	if (vphase->repeat_skip >= 0) {
		/* skip lines */
		VSYNC_WR_MPEG_REG_BITS(
			VD2_VSC_PHASE_CTRL + misc_off,
			skip_tab[vphase->repeat_skip],
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID +
			VPP_PHASECTL_INIRPTNUM_WID);
	} else {
		/* repeat first line */
		VSYNC_WR_MPEG_REG_BITS(
			VD2_VSC_PHASE_CTRL + misc_off,
			4,
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID);
		VSYNC_WR_MPEG_REG_BITS(
			VD2_VSC_PHASE_CTRL + misc_off,
			1 - vphase->repeat_skip,
			VPP_PHASECTL_INIRPTNUMT_BIT,
			VPP_PHASECTL_INIRPTNUM_WID);
	}

	VSYNC_WR_MPEG_REG_BITS(
		VD2_VSC_PHASE_CTRL + misc_off,
		(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
		VPP_PHASECTL_DOUBLELINE_BIT,
		VPP_PHASECTL_DOUBLELINE_WID);
}

static void disable_vd1_blend(struct video_layer_s *layer)
{
	u32 misc_off, vd_off, afbc_off;

	if (!layer)
		return;

	misc_off = layer->misc_reg_offt;
	vd_off = layer->vd_reg_offt;
	afbc_off = layer->afbc_reg_offt;
	if (layer->global_debug & DEBUG_FLAG_BLACKOUT)
		pr_info("VIDEO: VD1 AFBC off now. dispbuf:%p, *dispbuf_mapping:%p, local: %p-%p\n",
			layer->dispbuf,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip);
	VSYNC_WR_MPEG_REG(AFBC_ENABLE + afbc_off, 0);
	VSYNC_WR_MPEG_REG(VD1_IF0_GEN_REG + vd_off, 0);
	if (!legacy_vpp)
		VSYNC_WR_MPEG_REG(
			VD1_BLEND_SRC_CTRL + misc_off, 0);

	if (is_dolby_vision_enable()) {
		if (is_meson_txlx_cpu() ||
		    is_meson_gxm_cpu())
			VSYNC_WR_MPEG_REG_BITS(
				VIU_MISC_CTRL1 + misc_off,
				3, 16, 2); /* bypass core1 */
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			VSYNC_WR_MPEG_REG_BITS(
				DOLBY_PATH_CTRL + misc_off,
				3, 0, 2);
	}

	/*auto disable sr when video off*/
	if (!is_meson_txl_cpu() &&
	    !is_meson_txlx_cpu()) {
		VSYNC_WR_MPEG_REG(VPP_SRSHARP0_CTRL, 0);
		VSYNC_WR_MPEG_REG(VPP_SRSHARP1_CTRL, 0);
	}

	if (layer->dispbuf &&
	    is_local_vf(layer->dispbuf))
		layer->dispbuf = NULL;
	if (layer->dispbuf_mapping) {
		if (*layer->dispbuf_mapping &&
		    is_local_vf(*layer->dispbuf_mapping))
			*layer->dispbuf_mapping = NULL;
		layer->dispbuf_mapping = NULL;
		layer->dispbuf = NULL;
	}
	layer->new_vframe_count = 0;
}

static void disable_vd2_blend(struct video_layer_s *layer)
{
	u32 misc_off, vd_off, afbc_off;

	if (!layer)
		return;

	misc_off = layer->misc_reg_offt;
	vd_off = layer->vd_reg_offt;
	afbc_off = layer->afbc_reg_offt;
	if (layer->global_debug & DEBUG_FLAG_BLACKOUT)
		pr_info("VIDEO: VD2 AFBC off now. dispbuf:%p, *dispbuf_mapping:%p, local: %p-%p\n",
			layer->dispbuf,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip);
	VSYNC_WR_MPEG_REG(VD2_AFBC_ENABLE + afbc_off, 0);
	VSYNC_WR_MPEG_REG(VD2_IF0_GEN_REG + vd_off, 0);
	if (!legacy_vpp)
		VSYNC_WR_MPEG_REG(
			VD2_BLEND_SRC_CTRL + misc_off, 0);

	if (layer->dispbuf &&
	    is_local_vf(layer->dispbuf))
		layer->dispbuf = NULL;
	if (layer->dispbuf_mapping) {
		if (*layer->dispbuf_mapping &&
		    is_local_vf(*layer->dispbuf_mapping))
			*layer->dispbuf_mapping = NULL;
		layer->dispbuf_mapping = NULL;
		layer->dispbuf = NULL;
	}
	/* FIXME: remove global variables */
	last_el_status = 0;
	need_disable_vd2 = false;
	layer->new_vframe_count = 0;
}

/*********************************************************
 * DV EL APIs
 *********************************************************/
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static bool is_sc_enable_before_pps(struct vpp_frame_par_s *par)
{
	bool ret = false;

	if (par) {
		if (par->supsc0_enable &&
		    ((par->supscl_path == CORE0_PPS_CORE1) ||
		     (par->supscl_path == CORE0_BEFORE_PPS)))
			ret = true;
		else if (par->supsc1_enable &&
			 (par->supscl_path == CORE1_BEFORE_PPS))
			ret = true;
		else if ((par->supsc0_enable || par->supsc1_enable) &&
			 (par->supscl_path == CORE0_CORE1_PPS))
			ret = true;
	}
	return ret;
}

void correct_vd1_mif_size_for_DV(
	struct vpp_frame_par_s *par, struct vframe_s *el_vf)
{
	u32 aligned_mask = 0xfffffffe;
	u32 old_len;

	if ((is_dolby_vision_on() == true) &&
	    (par->VPP_line_in_length_ > 0) &&
	    !is_sc_enable_before_pps(par)) {
		/* work around to skip the size check when sc enable */
		if (el_vf) {
			/*
			 *if (cur_dispbuf2->type
			 *	& VIDTYPE_COMPRESS)
			 *	aligned_mask = 0xffffffc0;
			 *else
			 */
			aligned_mask = 0xfffffffc;
		}
#ifdef CHECK_LATER /* def TV_REVERSE */
		if (reverse) {
			par->VPP_line_in_length_
				&= 0xfffffffe;
			par->VPP_hd_end_lines_
				&= 0xfffffffe;
			par->VPP_hd_start_lines_ =
				par->VPP_hd_end_lines_ + 1
				- (par->VPP_line_in_length_ <<
				par->hscale_skip_count);
		} else
#endif
		{
		par->VPP_line_in_length_
			&= aligned_mask;
		par->VPP_hd_start_lines_
			&= aligned_mask;
		par->VPP_hd_end_lines_ =
			par->VPP_hd_start_lines_ +
			(par->VPP_line_in_length_ <<
			par->hscale_skip_count) - 1;
		/* if have el layer, need 2 pixel align by height */
		if (el_vf) {
			old_len =
				par->VPP_vd_end_lines_ -
				par->VPP_vd_start_lines_ + 1;
			if (old_len & 1)
				par->VPP_vd_end_lines_--;
			if (par->VPP_vd_start_lines_ & 1) {
				par->VPP_vd_start_lines_--;
				par->VPP_vd_end_lines_--;
			}
			old_len =
				par->VPP_vd_end_lines_ -
				par->VPP_vd_start_lines_ + 1;
			old_len = old_len >> par->vscale_skip_count;
			if (par->VPP_pic_in_height_ < old_len)
				par->VPP_pic_in_height_ = old_len;
		}
		}
	}
}

void config_dvel_position(
	struct video_layer_s *layer,
	struct mif_pos_s *setting,
	struct vframe_s *el_vf)
{
	int width_bl, width_el;
	int height_bl, height_el;
	int shift = 0, line_in_length;
	struct vpp_frame_par_s *cur_frame_par;
	struct vframe_s *bl_vf;

	if (!layer || !layer->cur_frame_par ||
	    !layer->dispbuf || !setting || !el_vf)
		return;

	bl_vf = layer->dispbuf;
	cur_frame_par = layer->cur_frame_par;

	setting->id = 1;
	setting->vd_reg_offt = layer->vd_reg_offt;
	setting->afbc_reg_offt = layer->afbc_reg_offt;
	setting->reverse = glayer_info[0].reverse;

	width_el = (el_vf->type & VIDTYPE_COMPRESS) ?
		el_vf->compWidth : el_vf->width;
	if (!(el_vf->type & VIDTYPE_VD2)) {
		width_bl =
			(bl_vf->type & VIDTYPE_COMPRESS) ?
			bl_vf->compWidth : bl_vf->width;
		if (width_el >= width_bl)
			width_el = width_bl;
		else if (width_el != width_bl / 2)
			width_el = width_bl / 2;
		if (width_el >= width_bl)
			shift = 0;
		else
			shift = 1;
	}

	height_el = (el_vf->type & VIDTYPE_COMPRESS) ?
		el_vf->compHeight : el_vf->height;
	if (!(el_vf->type & VIDTYPE_VD2)) {
		height_bl =
			(bl_vf->type & VIDTYPE_COMPRESS) ?
			bl_vf->compHeight : bl_vf->height;
		if (height_el >= height_bl)
			height_el = height_bl;
		else if (height_el != height_bl / 2)
			height_el = height_bl / 2;
	}

	setting->src_w = width_el;
	setting->src_h = height_el;

	setting->start_x_lines = 0;
	setting->end_x_lines = width_el - 1;
	setting->start_y_lines = 0;
	setting->end_y_lines = height_el - 1;

	if ((is_dolby_vision_on() == true) &&
	    (cur_frame_par->VPP_line_in_length_ > 0) &&
	    !is_sc_enable_before_pps(cur_frame_par)) {
		setting->start_x_lines =
			cur_frame_par->VPP_hd_start_lines_ >> shift;
		line_in_length =
			cur_frame_par->VPP_line_in_length_ >> shift;
		line_in_length &= 0xfffffffe;
		if (line_in_length > 1)
			setting->end_x_lines =
				setting->start_x_lines +
				line_in_length - 1;
		else
			setting->end_x_lines = setting->start_x_lines;

		setting->start_y_lines =
			cur_frame_par->VPP_vd_start_lines_ >> shift;
		if (setting->start_y_lines >= setting->end_y_lines)
			setting->end_y_lines = setting->start_y_lines;
		/* FIXME: if el len is 0, need disable bl */
	}

	setting->h_skip = layer->cur_frame_par->hscale_skip_count;
	setting->v_skip = layer->cur_frame_par->vscale_skip_count;
	/* afbc is nv12 as default */
	setting->hc_skip = 2;
	setting->vc_skip = 2;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (layer->dispbuf->type
		    & VIDTYPE_VIU_444) {
			setting->hc_skip = 1;
			setting->vc_skip = 1;
		} else if (layer->dispbuf->type
			   & VIDTYPE_VIU_422) {
			setting->vc_skip = 1;
		}
	}

	if ((layer->dispbuf->type & VIDTYPE_COMPRESS) &&
	    !layer->cur_frame_par->nocomp)
		setting->skip_afbc = false;
	else
		setting->skip_afbc = true;

	setting->l_hs_luma = setting->start_x_lines;
	setting->l_he_luma = setting->end_x_lines;
	setting->l_vs_luma = setting->start_y_lines;
	setting->l_ve_luma = setting->end_y_lines;
	setting->r_hs_luma = setting->start_x_lines;
	setting->r_he_luma = setting->end_x_lines;
	setting->r_vs_luma = setting->start_y_lines;
	setting->r_ve_luma = setting->end_y_lines;

	setting->l_hs_chrm = setting->l_hs_luma >> 1;
	setting->l_he_chrm = setting->l_he_luma >> 1;
	setting->r_hs_chrm = setting->r_hs_luma >> 1;
	setting->r_he_chrm = setting->r_he_luma >> 1;
	setting->l_vs_chrm = setting->l_vs_luma >> 1;
	setting->l_ve_chrm = setting->l_ve_luma >> 1;
	setting->r_vs_chrm = setting->r_vs_luma >> 1;
	setting->r_ve_chrm = setting->r_ve_luma >> 1;
	setting->vpp_3d_mode = 0;
}

s32 config_dvel_pps(
	struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	const struct vinfo_s *info)
{
	if (!layer || !layer->cur_frame_par ||
	    !setting || !info)
		return -1;

	setting->support =
		glayer_info[1].pps_support;
	setting->frame_par = layer->cur_frame_par;
	setting->id = 1;
	setting->misc_reg_offt = layer->misc_reg_offt;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B))
		setting->last_line_fix = true;
	else
		setting->last_line_fix = false;

	setting->sc_top_enable = false;

	setting->vinfo_width = info->width;
	setting->vinfo_height = info->height;
	return 0;
}

s32 config_dvel_blend(
	struct video_layer_s *layer,
	struct blend_setting_s *setting,
	struct vframe_s *dvel_vf)
{
	if (!layer || !layer->cur_frame_par || !setting)
		return -1;

	setting->frame_par = layer->cur_frame_par;
	setting->id = 1;
	setting->misc_reg_offt = layer->misc_reg_offt;
	setting->layer_alpha = layer->layer_alpha;

	setting->postblend_h_start = 0;
	setting->postblend_h_end =
		((dvel_vf->type & VIDTYPE_COMPRESS) ?
		dvel_vf->compWidth :
		dvel_vf->width) - 1;
	setting->postblend_v_start = 0;
	setting->postblend_v_end =
		((dvel_vf->type & VIDTYPE_COMPRESS) ?
		dvel_vf->compHeight :
		dvel_vf->height) - 1;
	return 0;
}
#endif

/*********************************************************
 * 3D APIs
 *********************************************************/
#ifdef TV_3D_FUNCTION_OPEN
static void get_3d_horz_pos(
	struct video_layer_s *layer,
	struct vframe_s *vf, u32 vpp_3d_mode,
	u32 *ls, u32 *le, u32 *rs, u32 *re)
{
	u32 crop_sx, crop_ex;
	int frame_width;
	struct disp_info_s *info = &glayer_info[0];

	if (!vf)
		return;

	crop_sx = info->crop_left;
	crop_ex = info->crop_right;

	if (vf->type & VIDTYPE_COMPRESS)
		frame_width = vf->compWidth;
	else
		frame_width = vf->width;

	if (vpp_3d_mode == VPP_3D_MODE_LR) {
		/*half width, double height */
		*ls = layer->start_x_lines;
		if (process_3d_type & MODE_3D_OUT_LR)
			*le = layer->end_x_lines >> 1;
		else
			*le = layer->end_x_lines;
		*rs = *ls + (frame_width >> 1);
		*re = *le + (frame_width >> 1);
	} else {
		/* VPP_3D_MODE_TB: */
		/* VPP_3D_MODE_LA: */
		/* VPP_3D_MODE_FA: */
		/* and others. */
		if (vf->trans_fmt == TVIN_TFMT_3D_FP) {
			*ls = vf->left_eye.start_x + crop_sx;
			*le = vf->left_eye.start_x
				+ vf->left_eye.width
				- crop_ex - 1;
			*rs = vf->right_eye.start_x + crop_sx;
			*re = vf->right_eye.start_x
				+ vf->right_eye.width
				- crop_ex - 1;
		} else if (process_3d_type
			   & MODE_3D_OUT_LR) {
			*ls = layer->start_x_lines;
			*le = layer->end_x_lines >> 1;
			*rs = *ls;
			*re = *le;
		} else {
			*ls = *rs = layer->start_x_lines;
			*le = *re = layer->end_x_lines;
		}
	}
}

static void get_3d_vert_pos(
	struct video_layer_s *layer,
	struct vframe_s *vf, u32 vpp_3d_mode,
	u32 *ls, u32 *le, u32 *rs, u32 *re)
{
	u32 crop_sy, crop_ey, height;
	int frame_height;
	struct disp_info_s *info = &glayer_info[0];

	if (!vf)
		return;

	crop_sy = info->crop_top;
	crop_ey = info->crop_bottom;

	if (vf->type & VIDTYPE_COMPRESS)
		frame_height = vf->compHeight;
	else
		frame_height = vf->height;

	if (vf->type & VIDTYPE_INTERLACE)
		height = frame_height >> 1;
	else
		height = frame_height;

	if (vpp_3d_mode == VPP_3D_MODE_TB) {
		if (vf->trans_fmt == TVIN_TFMT_3D_FP) {
			*ls = vf->left_eye.start_y + (crop_sy >> 1);
			*le = vf->left_eye.start_y
				+ vf->left_eye.height
				- (crop_ey >> 1);
			*rs = vf->right_eye.start_y + (crop_sy >> 1);
			*re = vf->right_eye.start_y
				+ vf->left_eye.height
				- (crop_ey >> 1);
			if (vf->type & VIDTYPE_INTERLACE) {
				/*if input is interlace vertical*/
				/*crop will be reduce by half */
				*ls = *ls >> 1;
				*le = *le >> 1;
				*rs = *rs >> 1;
				*re = *re >> 1;
			}
			*le = *le - 1;
			*re = *re - 1;
		} else {
			*ls = layer->start_y_lines >> 1;
			*le = layer->end_y_lines >> 1;
			if ((vf->type & VIDTYPE_VIU_FIELD) &&
			    (vf->type & VIDTYPE_INTERLACE)) {
				/* vdin interlace case */
				*rs = *ls + (height >> 1);
				*re = *le + (height >> 1);
			} else if (vf->type & VIDTYPE_INTERLACE) {
				/* decode interlace case */
				*rs = *ls + height;
				*re = *le + height;
			} else {
				/* mvc case: same width, same height */
				*rs = *ls + (height >> 1);
				*re = *le + (height >> 1);
			}
			if ((process_3d_type
			     & MODE_3D_TO_2D_MASK) ||
			    (process_3d_type
			     & MODE_3D_OUT_LR)) {
				/* same width, half height */
				*ls = layer->start_y_lines;
				*le = layer->end_y_lines;
				*rs = *ls + (height >> 1);
				*re = *le + (height >> 1);
			}
		}
	} else if (vpp_3d_mode == VPP_3D_MODE_LR) {
		if ((process_3d_type & MODE_3D_TO_2D_MASK) ||
		    (process_3d_type & MODE_3D_OUT_LR)) {
			/*half width ,same height */
			*ls = layer->start_y_lines;
			*le = layer->end_y_lines;
			*rs = layer->start_y_lines;
			*re = layer->end_y_lines;
		} else {
			/* half width,double height */
			*ls = layer->start_y_lines >> 1;
			*le = layer->end_y_lines >> 1;
			*rs = layer->start_y_lines >> 1;
			*re = layer->end_y_lines >> 1;
		}
	} else if (vpp_3d_mode == VPP_3D_MODE_FA) {
		/*same width same heiht */
		if ((process_3d_type & MODE_3D_TO_2D_MASK) ||
		    (process_3d_type & MODE_3D_OUT_LR)) {
			*ls = layer->start_y_lines;
			*le = layer->end_y_lines;
			*rs = layer->start_y_lines;
			*re = layer->end_y_lines;
		} else {
			*ls = (layer->start_y_lines + crop_sy) >> 1;
			*le = (layer->end_y_lines + crop_ey) >> 1;
			*rs = (layer->start_y_lines + crop_sy) >> 1;
			*re = (layer->end_y_lines + crop_ey) >> 1;
		}
	} else if (vpp_3d_mode == VPP_3D_MODE_LA) {
		if ((process_3d_type & MODE_3D_OUT_FA_MASK) ||
		    (process_3d_type & MODE_3D_OUT_TB) ||
		    (process_3d_type & MODE_3D_OUT_LR)) {
			*rs = layer->start_y_lines + 1;
			*ls = layer->start_y_lines;
		} else if ((process_3d_type & MODE_3D_LR_SWITCH) ||
			   (process_3d_type & MODE_3D_TO_2D_R)) {
			*ls = layer->start_y_lines + 1;
			*rs = layer->start_y_lines + 1;
		} else {
			*ls = layer->start_y_lines;
			*rs = layer->start_y_lines;
		}
		*le = layer->end_y_lines;
		*re = layer->end_y_lines;
	} else {
		*ls = layer->start_y_lines;
		*le = layer->end_y_lines;
		*rs = layer->start_y_lines;
		*re = layer->end_y_lines;
	}
}

void config_3d_vd2_position(
	struct video_layer_s *layer,
	struct mif_pos_s *setting)
{
	if (!layer || !layer->cur_frame_par || !layer->dispbuf || !setting)
		return;

	memcpy(setting, &layer->mif_setting, sizeof(struct mif_pos_s));
	setting->id = 1;

	/* need not change the horz position */
	/* not framepacking mode, need process vert position more */
	/* framepacking mode, need use right eye position */
	/* to set vd2 Y0 not vd1 Y1*/
	if ((layer->dispbuf->type & VIDTYPE_MVC) && framepacking_support) {
		setting->l_vs_chrm =
			setting->r_vs_chrm;
		setting->l_ve_chrm =
			setting->r_ve_chrm;
		setting->l_vs_luma =
			setting->r_vs_luma;
		setting->l_ve_luma =
			setting->r_ve_luma;
		setting->r_vs_luma = 0;
		setting->r_ve_luma = 0;
		setting->r_vs_chrm = 0;
		setting->r_ve_chrm = 0;
	}
	setting->vpp_3d_mode = 0;
}

s32 config_3d_vd2_pps(
	struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	const struct vinfo_s *info)
{
	if (!layer || !layer->cur_frame_par || !setting || !info)
		return -1;

	memcpy(setting, &layer->sc_setting, sizeof(struct scaler_setting_s));
	setting->support = glayer_info[1].pps_support;
	setting->sc_top_enable = false;
	setting->id = 1;
	return 0;
}

s32 config_3d_vd2_blend(
	struct video_layer_s *layer,
	struct blend_setting_s *setting)
{
	struct vpp_frame_par_s *cur_frame_par;
	u32 x_lines, y_lines;

	if (!layer || !layer->cur_frame_par || !setting)
		return -1;

	memcpy(setting, &layer->bld_setting, sizeof(struct blend_setting_s));
	setting->id = 1;

	cur_frame_par = layer->cur_frame_par;
	x_lines = layer->end_x_lines /
		(cur_frame_par->hscale_skip_count + 1);
	y_lines = layer->end_y_lines /
		(cur_frame_par->vscale_skip_count + 1);

	if (process_3d_type & MODE_3D_OUT_TB) {
		setting->postblend_h_start =
			layer->start_x_lines;
		setting->postblend_h_end =
			layer->end_x_lines;
		setting->postblend_v_start =
			(y_lines + 1) >> 1;
		setting->postblend_v_end = y_lines;
	} else if (process_3d_type & MODE_3D_OUT_LR) {
		/* vd1 and vd2 do pre blend */
		setting->postblend_h_start =
			(x_lines + 1) >> 1;
		setting->postblend_h_end = x_lines;
		setting->postblend_v_start =
			layer->start_y_lines;
		setting->postblend_v_end =
			layer->end_y_lines;
	} else if (layer->dispbuf &&
		   (layer->dispbuf->type & VIDTYPE_MVC)) {
		u32 blank;

		if (framepacking_support)
			blank = framepacking_blank;
		else
			blank = 0;
		setting->postblend_v_start =
			(cur_frame_par->VPP_vd_end_lines_
			- blank + 1) >> 1;
		setting->postblend_v_start += blank;
		setting->postblend_v_end =
			cur_frame_par->VPP_vd_end_lines_;
		setting->postblend_h_start =
			cur_frame_par->VPP_hd_start_lines_;
		setting->postblend_h_end =
			cur_frame_par->VPP_hd_end_lines_;
	}
	return 0;
}

void switch_3d_view_per_vsync(
	struct video_layer_s *layer)
{
	struct vpp_frame_par_s *cur_frame_par;
	u32 misc_off, vd_off, afbc_off;
	u32 start_aligned, end_aligned, block_len;
	u32 FA_enable = process_3d_type & MODE_3D_OUT_FA_MASK;
	u32 src_start_x_lines, src_end_x_lines;
	u32 src_start_y_lines, src_end_y_lines;

	if (!layer || !layer->cur_frame_par || !layer->dispbuf)
		return;

	src_start_x_lines = 0;
	src_end_x_lines =
		((layer->dispbuf->type &
		VIDTYPE_COMPRESS) ?
		layer->dispbuf->compWidth :
		layer->dispbuf->width) - 1;
	src_start_y_lines = 0;
	src_end_y_lines =
		((layer->dispbuf->type &
		VIDTYPE_COMPRESS) ?
		layer->dispbuf->compHeight :
		layer->dispbuf->height) - 1;

	misc_off = layer->misc_reg_offt;
	vd_off = layer->vd_reg_offt;
	afbc_off = layer->afbc_reg_offt;
	cur_frame_par = layer->cur_frame_par;
	if (FA_enable && (toggle_3d_fa_frame == OUT_FA_A_FRAME)) {
		VSYNC_WR_MPEG_REG_BITS(
			VPP_MISC + misc_off, 1, 14, 1);
			/* VPP_VD1_PREBLEND disable */
		VSYNC_WR_MPEG_REG_BITS(VPP_MISC +
			VPP_MISC + misc_off, 1, 10, 1);
			/* VPP_VD1_POSTBLEND disable */
		VSYNC_WR_MPEG_REG(
			VD1_IF0_LUMA_PSEL + vd_off,
			0x4000000);
		VSYNC_WR_MPEG_REG(
			VD1_IF0_CHROMA_PSEL + vd_off,
			0x4000000);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA_PSEL + vd_off,
			0x4000000);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA_PSEL + vd_off,
			0x4000000);
		if (layer->dispbuf->type & VIDTYPE_COMPRESS) {
			if ((process_3d_type & MODE_FORCE_3D_FA_LR) &&
			    (cur_frame_par->vpp_3d_mode == 1)) {
				start_aligned = src_start_x_lines;
				end_aligned = src_end_x_lines + 1;
				block_len =
					(end_aligned - start_aligned) / 2;
				block_len = block_len /
					(cur_frame_par->hscale_skip_count + 1);
				VSYNC_WR_MPEG_REG(
					AFBC_PIXEL_HOR_SCOPE + afbc_off,
					(start_aligned << 16) |
					(start_aligned + block_len - 1));
			}
			if ((process_3d_type & MODE_FORCE_3D_FA_TB) &&
			    (cur_frame_par->vpp_3d_mode == 2)) {
				start_aligned =
					round_down(src_start_y_lines, 4);
				end_aligned =
					round_up(src_end_y_lines + 1, 4);
				block_len = end_aligned - start_aligned;
				block_len = block_len / 8;
				VSYNC_WR_MPEG_REG(
					AFBC_MIF_VER_SCOPE + afbc_off,
					((start_aligned / 4) << 16) |
					((start_aligned / 4)  + block_len - 1));
			}
		}
	} else if (FA_enable && (toggle_3d_fa_frame == OUT_FA_B_FRAME)) {
		VSYNC_WR_MPEG_REG_BITS(
			VPP_MISC + misc_off, 1, 14, 1);
		/* VPP_VD1_PREBLEND disable */
		VSYNC_WR_MPEG_REG_BITS(
			VPP_MISC + misc_off, 1, 10, 1);
		/* VPP_VD1_POSTBLEND disable */
		VSYNC_WR_MPEG_REG(
			VD1_IF0_LUMA_PSEL + vd_off, 0);
		VSYNC_WR_MPEG_REG(
			VD1_IF0_CHROMA_PSEL + vd_off, 0);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_LUMA_PSEL + vd_off, 0);
		VSYNC_WR_MPEG_REG(
			VD2_IF0_CHROMA_PSEL + vd_off, 0);
		if (layer->dispbuf->type & VIDTYPE_COMPRESS) {
			if ((process_3d_type & MODE_FORCE_3D_FA_LR) &&
			    (cur_frame_par->vpp_3d_mode == 1)) {
				start_aligned = src_end_x_lines;
				end_aligned = src_end_x_lines + 1;
				block_len =
					(end_aligned - start_aligned) / 2;
				block_len = block_len /
					(cur_frame_par->hscale_skip_count + 1);
				VSYNC_WR_MPEG_REG(
					AFBC_PIXEL_HOR_SCOPE + afbc_off,
					((start_aligned + block_len) << 16) |
					(end_aligned - 1));
			}
			if ((process_3d_type & MODE_FORCE_3D_FA_TB) &&
			    (cur_frame_par->vpp_3d_mode == 2)) {
				start_aligned =
					round_down(src_start_y_lines, 4);
				end_aligned =
					round_up(src_end_y_lines + 1, 4);
				block_len = end_aligned - start_aligned;
				block_len = block_len / 8;
				VSYNC_WR_MPEG_REG(
					AFBC_MIF_VER_SCOPE + afbc_off,
				(((start_aligned / 4) + block_len) << 16) |
				((end_aligned / 4)  - 1));
			}
		}
	} else if (FA_enable && (toggle_3d_fa_frame == OUT_FA_BANK_FRAME)) {
		/* output a banking frame */
		VSYNC_WR_MPEG_REG_BITS(
			VPP_MISC + misc_off, 0, 14, 1);
		/* VPP_VD1_PREBLEND disable */
		VSYNC_WR_MPEG_REG_BITS(
			VPP_MISC + misc_off, 0, 10, 1);
		/* VPP_VD1_POSTBLEND disable */
	}

	if ((process_3d_type & MODE_3D_OUT_TB) ||
	    (process_3d_type & MODE_3D_OUT_LR)) {
		if (cur_frame_par->vpp_2pic_mode & VPP_PIC1_FIRST) {
			VSYNC_WR_MPEG_REG(
				VD1_IF0_LUMA_PSEL + vd_off,
				0x4000000);
			VSYNC_WR_MPEG_REG(
				VD1_IF0_CHROMA_PSEL + vd_off,
				0x4000000);
			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_PSEL + vd_off, 0);
			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_PSEL + vd_off, 0);
		} else {
			VSYNC_WR_MPEG_REG(
				VD1_IF0_LUMA_PSEL + vd_off, 0);
			VSYNC_WR_MPEG_REG(
				VD1_IF0_CHROMA_PSEL + vd_off, 0);
			VSYNC_WR_MPEG_REG(
				VD2_IF0_LUMA_PSEL + vd_off,
				0x4000000);
			VSYNC_WR_MPEG_REG(
				VD2_IF0_CHROMA_PSEL + vd_off,
				0x4000000);
		}
	}

	if (force_3d_scaler <= 3 &&
	    cur_frame_par->vpp_3d_scale) {
		VSYNC_WR_MPEG_REG_BITS(
			VPP_VSC_PHASE_CTRL + misc_off,
			force_3d_scaler,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
	}
}
#endif

s32 config_vd_position(
	struct video_layer_s *layer,
	struct mif_pos_s *setting)
{
	if (!layer || !layer->cur_frame_par || !layer->dispbuf || !setting)
		return -1;

	setting->id = layer->layer_id;
	setting->vd_reg_offt = layer->vd_reg_offt;
	setting->afbc_reg_offt = layer->afbc_reg_offt;

	setting->reverse = glayer_info[setting->id].reverse;
	setting->src_w =
		(layer->dispbuf->type & VIDTYPE_COMPRESS) ?
		layer->dispbuf->compWidth : layer->dispbuf->width;
	setting->src_h =
		(layer->dispbuf->type & VIDTYPE_COMPRESS) ?
		layer->dispbuf->compHeight : layer->dispbuf->height;

	setting->start_x_lines = layer->start_x_lines;
	setting->end_x_lines = layer->end_x_lines;
	setting->start_y_lines = layer->start_y_lines;
	setting->end_y_lines = layer->end_y_lines;

	setting->h_skip = layer->cur_frame_par->hscale_skip_count;
	setting->v_skip = layer->cur_frame_par->vscale_skip_count;
	/* afbc is nv12 as default */
	setting->hc_skip = 2;
	setting->vc_skip = 2;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (layer->dispbuf->type
			& VIDTYPE_VIU_444) {
			setting->hc_skip = 1;
			setting->vc_skip = 1;
		} else if (layer->dispbuf->type
			& VIDTYPE_VIU_422) {
			setting->vc_skip = 1;
		}
	}

	if ((layer->dispbuf->type & VIDTYPE_COMPRESS) &&
	    !layer->cur_frame_par->nocomp)
		setting->skip_afbc = false;
	else
		setting->skip_afbc = true;

	setting->l_hs_luma = layer->start_x_lines;
	setting->l_he_luma = layer->end_x_lines;
	setting->l_vs_luma = layer->start_y_lines;
	setting->l_ve_luma = layer->end_y_lines;
	setting->r_hs_luma = layer->start_x_lines;
	setting->r_he_luma = layer->end_x_lines;
	setting->r_vs_luma = layer->start_y_lines;
	setting->r_ve_luma = layer->end_y_lines;

#ifdef TV_3D_FUNCTION_OPEN
	if ((setting->id == 0) &&
	    (process_3d_type & MODE_3D_ENABLE)) {
		get_3d_horz_pos(
			layer, layer->dispbuf,
			layer->cur_frame_par->vpp_3d_mode,
			&setting->l_hs_luma, &setting->l_he_luma,
			&setting->r_hs_luma, &setting->r_he_luma);
		get_3d_vert_pos(
			layer, layer->dispbuf,
			layer->cur_frame_par->vpp_3d_mode,
			&setting->l_vs_luma, &setting->l_ve_luma,
			&setting->r_vs_luma, &setting->r_ve_luma);
	}
#endif
	setting->l_hs_chrm = setting->l_hs_luma >> 1;
	setting->l_he_chrm = setting->l_he_luma >> 1;
	setting->r_hs_chrm = setting->r_hs_luma >> 1;
	setting->r_he_chrm = setting->r_he_luma >> 1;
	setting->l_vs_chrm = setting->l_vs_luma >> 1;
	setting->l_ve_chrm = setting->l_ve_luma >> 1;
	setting->r_vs_chrm = setting->r_vs_luma >> 1;
	setting->r_ve_chrm = setting->r_ve_luma >> 1;

#ifdef TV_3D_FUNCTION_OPEN
	/* need not change the horz position */
	/* not framepacking mode, need process vert position more */
	/* framepacking mode, need use right eye position */
	/* to set vd2 Y0 not vd1 Y1*/
	if ((setting->id == 0) &&
	    (layer->dispbuf->type & VIDTYPE_MVC) &&
	    !framepacking_support) {
		setting->l_vs_chrm =
			setting->l_vs_luma;
		setting->l_ve_chrm =
			setting->l_ve_luma - 1;
		setting->l_vs_luma *= 2;
		setting->l_ve_luma =
			setting->l_ve_luma * 2 - 1;
		setting->r_vs_luma = 0;
		setting->r_ve_luma = 0;
		setting->r_vs_chrm = 0;
		setting->r_ve_chrm = 0;
	}
#endif
	setting->vpp_3d_mode =
		layer->cur_frame_par->vpp_3d_mode;
	return 0;
}

s32 config_vd_pps(
	struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	const struct vinfo_s *info)
{
	struct vppfilter_mode_s *vpp_filter;
	struct vpp_frame_par_s *cur_frame_par;

	if (!layer || !layer->cur_frame_par || !setting || !info)
		return -1;

	setting->support = glayer_info[layer->layer_id].pps_support;
	cur_frame_par = layer->cur_frame_par;
	vpp_filter = &cur_frame_par->vpp_filter;
	setting->frame_par = cur_frame_par;
	setting->id = layer->layer_id;
	setting->misc_reg_offt = layer->misc_reg_offt;
	/* enable pps as default */
	setting->sc_top_enable = true;
	setting->sc_h_enable = true;
	setting->sc_v_enable = true;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B))
		setting->last_line_fix = true;
	else
		setting->last_line_fix = false;

	if ((vpp_filter->vpp_hsc_start_phase_step == 0x1000000) &&
	    (vpp_filter->vpp_vsc_start_phase_step == 0x1000000) &&
	    (vpp_filter->vpp_hsc_start_phase_step ==
	     vpp_filter->vpp_hf_start_phase_step) &&
	    !vpp_filter->vpp_pre_vsc_en &&
	    !vpp_filter->vpp_pre_hsc_en &&
	    layer->bypass_pps)
		setting->sc_top_enable = false;

	setting->vinfo_width = info->width;
	setting->vinfo_height = info->height;
	return 0;
}

s32 config_vd_blend(
	struct video_layer_s *layer,
	struct blend_setting_s *setting)
{
	struct vpp_frame_par_s *cur_frame_par;
	u32 x_lines, y_lines;
#ifdef CHECK_LATER
	u32 type = 0;
#endif

	if (!layer || !layer->cur_frame_par || !setting)
		return -1;

	cur_frame_par = layer->cur_frame_par;
	setting->frame_par = cur_frame_par;
	setting->id = layer->layer_id;
	setting->misc_reg_offt = layer->misc_reg_offt;
	setting->layer_alpha = layer->layer_alpha;
	x_lines = layer->end_x_lines /
		(cur_frame_par->hscale_skip_count + 1);
	y_lines = layer->end_y_lines /
		(cur_frame_par->vscale_skip_count + 1);

	if (legacy_vpp) {
		setting->preblend_h_start = 0;
		setting->preblend_h_end = 4096;
	} else {
		setting->preblend_h_start = 0;
		setting->preblend_h_end =
			cur_frame_par->video_input_w - 1;
	}
	if ((cur_frame_par->VPP_post_blend_vd_v_end_ -
	     cur_frame_par->VPP_post_blend_vd_v_start_ + 1)
	    > VPP_PREBLEND_VD_V_END_LIMIT) {
		setting->preblend_v_start =
			cur_frame_par->VPP_post_blend_vd_v_start_;
		setting->preblend_v_end =
			cur_frame_par->VPP_post_blend_vd_v_end_;
	} else {
		setting->preblend_v_start = 0;
		setting->preblend_v_end =
			VPP_PREBLEND_VD_V_END_LIMIT - 1;
	}

	setting->preblend_h_size =
		cur_frame_par->video_input_w;
	setting->postblend_h_size =
		cur_frame_par->VPP_post_blend_h_size_;

	/* no scaler, so only can use mif setting */
	if (!glayer_info[layer->layer_id].pps_support) {
		setting->postblend_h_start =
			cur_frame_par->VPP_hd_start_lines_;
		setting->postblend_h_end =
			cur_frame_par->VPP_hd_start_lines_ +
			cur_frame_par->video_input_w - 1;
		setting->postblend_v_start =
			cur_frame_par->VPP_vd_start_lines_;
		setting->postblend_v_end =
			cur_frame_par->VPP_vd_start_lines_ +
			cur_frame_par->video_input_h - 1;
	} else {
		/* FIXME: use sr output */
		setting->postblend_h_start =
			cur_frame_par->VPP_hsc_startp;
		setting->postblend_h_end =
			cur_frame_par->VPP_hsc_endp;
		setting->postblend_v_start =
			cur_frame_par->VPP_vsc_startp;
		setting->postblend_v_end =
			cur_frame_par->VPP_vsc_endp;
	}

	if (!legacy_vpp) {
		u32 temp_h = cur_frame_par->video_input_h;

		temp_h <<= 16;
		setting->preblend_h_size |= temp_h;

		temp_h =
			cur_frame_par->VPP_post_blend_vd_v_end_ + 1;
		temp_h <<= 16;
		setting->postblend_h_size |= temp_h;
	}
#ifdef TV_3D_FUNCTION_OPEN
	if (setting->id == 0) {
		if (process_3d_type & MODE_3D_OUT_TB) {
			setting->preblend_h_start =
				cur_frame_par->VPP_hd_start_lines_;
			setting->preblend_h_end =
				cur_frame_par->VPP_hd_end_lines_;
			setting->preblend_v_start =
				cur_frame_par->VPP_vd_start_lines_;
			setting->preblend_v_end = y_lines >> 1;
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			/* vd1 and vd2 do pre blend */
			setting->preblend_h_start =
				cur_frame_par->VPP_hd_start_lines_;
			setting->preblend_h_end = x_lines >> 1;
			setting->preblend_v_start =
				cur_frame_par->VPP_vd_start_lines_;
			setting->preblend_v_end =
				cur_frame_par->VPP_vd_end_lines_;
		}
	}
#endif
	/* FIXME: need remove this work around check for afbc */
#ifdef CHECK_LATER
	if (layer->dispbuf)
		type = layer->dispbuf->type;
	if (cur_frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		u32 v_phase;
		struct vppfilter_mode_s *vpp_filter =
			&cur_frame_par->vpp_filter;

		v_phase = vpp_filter->vpp_vsc_start_phase_step;
		v_phase *= (cur_frame_par->vscale_skip_count + 1);
		if ((v_phase > 0x1000000) &&
		    (glayer_info[setting->id].layer_top < 0)) {
			if ((cur_frame_par->VPP_vsc_endp > 0x6) &&
			    ((cur_frame_par->VPP_vsc_endp < 0x250) ||
			     (cur_frame_par->VPP_vsc_endp <
			      cur_frame_par->VPP_post_blend_vd_v_end_ / 2))) {
				setting->postblend_v_end -= 6;
			}
		}
	}
#endif
	return 0;
}

void vd_set_dcu(
	u8 layer_id,
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf)
{
	if (layer_id == 0)
		vd1_set_dcu(layer, frame_par, vf);
	else
		vd2_set_dcu(layer, frame_par, vf);
}

void vd_mif_setting(
	u8 layer_id,
	struct mif_pos_s *setting)
{
	if (layer_id == 0)
		vd1_mif_setting(setting);
	else
		vd2_mif_setting(setting);
}

void vd_scaler_setting(
	u8 layer_id,
	struct scaler_setting_s *setting)
{
	if (setting && !setting->support)
		return;

	if (layer_id == 0)
		vd1_scaler_setting(setting);
	else
		vd2_scaler_setting(setting);
}

void vd_blend_setting(
	u8 layer_id,
	struct blend_setting_s *setting)
{
	if (layer_id == 0)
		vd1_blend_setting(setting);
	else
		vd2_blend_setting(setting);
}

void proc_vd_vsc_phase_per_vsync(
	u8 layer_id,
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf)
{
	if (!glayer_info[layer_id].pps_support)
		return;
	if (layer_id == 0)
		proc_vd1_vsc_phase_per_vsync(
			layer, frame_par, vf);
	else
		proc_vd2_vsc_phase_per_vsync(
			layer, frame_par, vf);
}

/*********************************************************
 * Vpp APIs
 *********************************************************/
void set_video_mute(bool on)
{
	video_mute_on = on;
}
EXPORT_SYMBOL(set_video_mute);

int get_video_mute(void)
{
	return video_mute_status;
}
EXPORT_SYMBOL(get_video_mute);

static void check_video_mute(void)
{
	if (video_mute_on) {
		if (is_dolby_vision_on()) {
			/* core 3 black */
			if (video_mute_status != VIDEO_MUTE_ON_DV) {
				dolby_vision_set_toggle_flag(1);
				pr_info("DOLBY: check_video_mute: VIDEO_MUTE_ON_DV\n");
			}
			video_mute_status = VIDEO_MUTE_ON_DV;
		} else {
			if (video_mute_status != VIDEO_MUTE_ON_VPP) {
				/* vpp black */
				VSYNC_WR_MPEG_REG(VPP_CLIP_MISC0,
						  (0x0 << 20) |
						  (0x200 << 10) |
						  0x200);
				VSYNC_WR_MPEG_REG(VPP_CLIP_MISC1,
						  (0x0 << 20) |
						  (0x200 << 10) |
						  0x200);
				pr_info("DOLBY: check_video_mute: VIDEO_MUTE_ON_VPP\n");
			}
			video_mute_status = VIDEO_MUTE_ON_VPP;
		}
	} else {
		if (is_dolby_vision_on()) {
			if (video_mute_status != VIDEO_MUTE_OFF) {
				dolby_vision_set_toggle_flag(2);
				pr_info("DOLBY: check_video_mute: VIDEO_MUTE_OFF dv on\n");
			}
			video_mute_status = VIDEO_MUTE_OFF;
		} else {
			if (video_mute_status != VIDEO_MUTE_OFF) {
				VSYNC_WR_MPEG_REG(VPP_CLIP_MISC0,
						  (0x3ff << 20) |
						  (0x3ff << 10) |
						  0x3ff);
				VSYNC_WR_MPEG_REG(VPP_CLIP_MISC1,
						  (0x0 << 20) |
						  (0x0 << 10) | 0x0);
				pr_info("DOLBY: check_video_mute: VIDEO_MUTE_OFF dv off\n");
			}
			video_mute_status = VIDEO_MUTE_OFF;
		}
	}
}

static int vpp_zorder_check(void)
{
	int force_flush = 0;
	u32 layer0_sel = 0; /* vd1 */
	u32 layer1_sel = 1; /* vd2 */

	if (legacy_vpp)
		return 0;

	if (glayer_info[1].zorder < glayer_info[0].zorder) {
		layer0_sel = 1;
		layer1_sel = 0;
		if ((glayer_info[0].zorder >= reference_zorder) &&
		    (glayer_info[1].zorder < reference_zorder))
			layer0_sel++;
	} else {
		layer0_sel = 0;
		layer1_sel = 1;
		if ((glayer_info[1].zorder >= reference_zorder) &&
		    (glayer_info[0].zorder < reference_zorder))
			layer1_sel++;
	}

	glayer_info[0].cur_sel_port = layer0_sel;
	glayer_info[1].cur_sel_port = layer1_sel;

	if ((glayer_info[0].cur_sel_port != glayer_info[0].last_sel_port) ||
	    (glayer_info[1].cur_sel_port != glayer_info[1].last_sel_port)) {
		force_flush = 1;
		glayer_info[0].last_sel_port = glayer_info[0].cur_sel_port;
		glayer_info[1].last_sel_port = glayer_info[1].cur_sel_port;
	}
	return force_flush;
}

void vpp_blend_update(
	const struct vinfo_s *vinfo)
{
	u32 vpp_misc_save, vpp_misc_set, mode = 0;
	u32 vpp_off = cur_dev->vpp_off;
	int video1_off_req = 0;
	int video2_off_req = 0;
	unsigned long flags;
	struct vpp_frame_par_s *vd1_frame_par =
		vd_layer[0].cur_frame_par;
	bool force_flush = vpp_zorder_check();

	check_video_mute();

	if (vd_layer[0].enable_3d_mode == mode_3d_mvc_enable)
		mode |= COMPOSE_MODE_3D;
	else if (is_dolby_vision_on() && last_el_status)
		mode |= COMPOSE_MODE_DV;
	if (bypass_cm)
		mode |= COMPOSE_MODE_BYPASS_CM;

	/* check the vpp post size first */
	if (!legacy_vpp && vinfo) {
		u32 read_value = VSYNC_RD_MPEG_REG(
			VPP_POSTBLEND_H_SIZE + vpp_off);
		if (((vinfo->field_height << 16) | vinfo->width)
			!= read_value)
			VSYNC_WR_MPEG_REG(
				VPP_POSTBLEND_H_SIZE + vpp_off,
				((vinfo->field_height << 16) | vinfo->width));
		VSYNC_WR_MPEG_REG(
			VPP_OUT_H_V_SIZE + vpp_off,
			vinfo->width << 16 |
			vinfo->field_height);
	} else if (vinfo) {
		if (VSYNC_RD_MPEG_REG(
			VPP_POSTBLEND_H_SIZE + vpp_off)
			!= vinfo->width)
			VSYNC_WR_MPEG_REG(
				VPP_POSTBLEND_H_SIZE + vpp_off,
				vinfo->width);
		VSYNC_WR_MPEG_REG(
			VPP_OUT_H_V_SIZE + vpp_off,
			vinfo->width << 16 |
			vinfo->field_height);
	}

	vpp_misc_save = READ_VCBUS_REG(VPP_MISC + vpp_off);

	vpp_misc_set = vpp_misc_save;
	if (is_dolby_vision_on())
		vpp_misc_set &= ~VPP_CM_ENABLE;
	else if (mode & COMPOSE_MODE_BYPASS_CM)
		vpp_misc_set &= ~VPP_CM_ENABLE;
	else
		vpp_misc_set |= VPP_CM_ENABLE;

	if (update_osd_vpp_misc && legacy_vpp) {
		vpp_misc_set &= ~osd_vpp_misc_mask;
		vpp_misc_set |=
			(osd_vpp_misc & osd_vpp_misc_mask);
		if (vpp_misc_set &
		    (VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND))
			vpp_misc_set |= VPP_POSTBLEND_EN;
	}

	if (likely(vd_layer[0].onoff_state != VIDEO_ENABLE_STATE_IDLE)) {
		/* state change for video layer enable/disable */
		spin_lock_irqsave(&video_onoff_lock, flags);
		if (vd_layer[0].onoff_state == VIDEO_ENABLE_STATE_ON_REQ) {
			/*
			 * the video layer is enabled one vsync later,assumming
			 * all registers are ready from RDMA.
			 */
			vd_layer[0].onoff_state = VIDEO_ENABLE_STATE_ON_PENDING;
		} else if (vd_layer[0].onoff_state ==
			VIDEO_ENABLE_STATE_ON_PENDING) {
			vpp_misc_set |=
				VPP_VD1_PREBLEND |
				VPP_VD1_POSTBLEND |
				VPP_POSTBLEND_EN;
			vd_layer[0].onoff_state = VIDEO_ENABLE_STATE_IDLE;
			vd_layer[0].onoff_time = jiffies_to_msecs(jiffies);
			if (vd_layer[0].global_debug & DEBUG_FLAG_BLACKOUT)
				pr_info("VIDEO: VsyncEnableVideoLayer\n");
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER1_CHANGED;
			force_flush = true;
		} else if (vd_layer[0].onoff_state ==
			VIDEO_ENABLE_STATE_OFF_REQ) {
			vpp_misc_set &= ~(VPP_VD1_PREBLEND |
				  VPP_VD1_POSTBLEND);
			if (mode & COMPOSE_MODE_3D)
				vpp_misc_set &= ~(VPP_VD2_PREBLEND |
					VPP_PREBLEND_EN);
			vd_layer[0].onoff_state = VIDEO_ENABLE_STATE_IDLE;
			vd_layer[0].onoff_time = jiffies_to_msecs(jiffies);
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER1_CHANGED;
			if (vd_layer[0].global_debug & DEBUG_FLAG_BLACKOUT)
				pr_info("VIDEO: VsyncDisableVideoLayer\n");
			video1_off_req = 1;
			force_flush = true;
		}
		spin_unlock_irqrestore(&video_onoff_lock, flags);
	} else if (vd_layer[0].enabled) {
		vpp_misc_set |=
			VPP_VD1_PREBLEND |
			VPP_VD1_POSTBLEND |
			VPP_POSTBLEND_EN;
	}

	if (likely(vd_layer[1].onoff_state != VIDEO_ENABLE_STATE_IDLE)) {
		/* state change for video layer2 enable/disable */
		spin_lock_irqsave(&video2_onoff_lock, flags);
		if (vd_layer[1].onoff_state == VIDEO_ENABLE_STATE_ON_REQ) {
			/*
			 * the video layer 2
			 * is enabled one vsync later, assumming
			 * all registers are ready from RDMA.
			 */
			vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_ON_PENDING;
		} else if (vd_layer[1].onoff_state ==
			VIDEO_ENABLE_STATE_ON_PENDING) {
			if (mode & COMPOSE_MODE_3D) {
				vpp_misc_set |= VPP_VD2_PREBLEND |
					VPP_PREBLEND_EN;
				vpp_misc_set &= ~VPP_VD2_POSTBLEND;
			} else if (vd_layer[0].enabled &&
				(mode & COMPOSE_MODE_DV)) {
				/* DV el case */
				vpp_misc_set &= ~(VPP_VD2_PREBLEND |
					VPP_VD2_POSTBLEND | VPP_PREBLEND_EN);
			} else if (vd_layer[1].dispbuf) {
				vpp_misc_set &=
					~(VPP_PREBLEND_EN | VPP_VD2_PREBLEND);
				vpp_misc_set |= VPP_VD2_POSTBLEND |
					VPP_POSTBLEND_EN;
			} else {
				vpp_misc_set |= VPP_VD2_PREBLEND |
					VPP_PREBLEND_EN;
			}
			vpp_misc_set |= (vd_layer[1].layer_alpha
				<< VPP_VD2_ALPHA_BIT);
			vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_IDLE;
			vd_layer[1].onoff_time = jiffies_to_msecs(jiffies);
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER2_CHANGED;
			if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT)
				pr_info("VIDEO: VsyncEnableVideoLayer2\n");
			force_flush = true;
		} else if (vd_layer[1].onoff_state ==
			VIDEO_ENABLE_STATE_OFF_REQ) {
			vpp_misc_set &= ~(VPP_VD2_PREBLEND |
				VPP_VD2_POSTBLEND | VPP_PREBLEND_EN
				| (0x1ff << VPP_VD2_ALPHA_BIT));
			vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_IDLE;
			vd_layer[1].onoff_time = jiffies_to_msecs(jiffies);
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER2_CHANGED;
			if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT)
				pr_info("VIDEO: VsyncDisableVideoLayer2\n");
			video2_off_req = 1;
			force_flush = true;
		}
		spin_unlock_irqrestore(&video2_onoff_lock, flags);
	} else if (vd_layer[1].enabled) {
		if (mode & COMPOSE_MODE_3D) {
			vpp_misc_set |= VPP_VD2_PREBLEND |
				VPP_PREBLEND_EN;
			vpp_misc_set &= ~VPP_VD2_POSTBLEND;
		} else if (vd_layer[0].enabled &&
			(mode & COMPOSE_MODE_DV)) {
			/* DV el case */
			vpp_misc_set &= ~(VPP_VD2_PREBLEND |
				VPP_VD2_POSTBLEND | VPP_PREBLEND_EN);
		} else if (vd_layer[1].dispbuf) {
			vpp_misc_set &=
				~(VPP_PREBLEND_EN | VPP_VD2_PREBLEND);
			vpp_misc_set |= VPP_VD2_POSTBLEND |
				VPP_POSTBLEND_EN;
		} else {
			vpp_misc_set |= VPP_VD2_PREBLEND |
				VPP_PREBLEND_EN;
		}
		vpp_misc_set |= (vd_layer[1].layer_alpha
			<< VPP_VD2_ALPHA_BIT);
	}

	if ((vd_layer[0].global_output == 0) ||
	    black_threshold_check(0)) {
		vd_layer[0].enabled = 0;
		/* preblend need disable together */
		vpp_misc_set &= ~(VPP_VD1_PREBLEND |
			VPP_VD2_PREBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN);
	} else {
		vd_layer[0].enabled = vd_layer[0].enabled_status_saved;
	}

	if (!vd_layer[0].enabled &&
	    ((vpp_misc_set & VPP_VD1_POSTBLEND) ||
	     (vpp_misc_set & VPP_VD1_PREBLEND)))
		vpp_misc_set &= ~(VPP_VD1_PREBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN);

	if (!(mode & COMPOSE_MODE_3D) &&
	    ((vd_layer[1].global_output == 0) ||
	     black_threshold_check(1))) {
		vd_layer[1].enabled = 0;
		vpp_misc_set &= ~(VPP_VD2_PREBLEND |
			VPP_VD2_POSTBLEND);
	} else {
		vd_layer[1].enabled = vd_layer[1].enabled_status_saved;
	}

	if (!vd_layer[1].enabled &&
	    ((vpp_misc_set & VPP_VD2_POSTBLEND) ||
	     (vpp_misc_set & VPP_VD2_PREBLEND)))
		vpp_misc_set &= ~(VPP_VD2_PREBLEND |
			VPP_VD2_POSTBLEND);

	if (!legacy_vpp) {
		u32 set_value = 0;

		/* for sr core0, put it between prebld & pps as default */
		if (vd1_frame_par &&
		    (vd1_frame_par->sr_core_support & SUPER_CORE0_SUPPORT))
			if (vd1_frame_par->sr0_position)
				vpp_misc_set |=
					PREBLD_SR0_VD1_SCALER;
			else /* SR0_AFTER_DNLP */
				vpp_misc_set &=
					~PREBLD_SR0_VD1_SCALER;
		else
			vpp_misc_set |=
				PREBLD_SR0_VD1_SCALER;
		/* for sr core1, put it before post blend as default */
		if (vd1_frame_par &&
		    (vd1_frame_par->sr_core_support & SUPER_CORE1_SUPPORT))
			if (vd1_frame_par->sr1_position)
				vpp_misc_set |=
					DNLP_SR1_CM;
			else /* SR1_AFTER_POSTBLEN */
				vpp_misc_set &=
					~DNLP_SR1_CM;
		else
			vpp_misc_set |=
				DNLP_SR1_CM;

		vpp_misc_set &=
			((1 << 29) | VPP_CM_ENABLE |
			(0x1ff << VPP_VD2_ALPHA_BIT) |
			VPP_VD2_PREBLEND |
			VPP_VD1_PREBLEND |
			VPP_VD2_POSTBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN |
			VPP_POSTBLEND_EN |
			0xf);

		/* if vd2 is bottom layer, need remove alpha for vd2 */
		/* 3d case vd2 preblend, need remove alpha for vd2 */
		if ((((vpp_misc_set & VPP_VD1_POSTBLEND) == 0) &&
		     (vpp_misc_set & VPP_VD2_POSTBLEND)) ||
		    (vpp_misc_set & VPP_VD2_PREBLEND)) {
			vpp_misc_set &= ~(0x1ff << VPP_VD2_ALPHA_BIT);
			vpp_misc_set |= (vd_layer[1].layer_alpha
				<< VPP_VD2_ALPHA_BIT);
		}

		vpp_misc_save &=
			((1 << 29) | VPP_CM_ENABLE |
			(0x1ff << VPP_VD2_ALPHA_BIT) |
			VPP_VD2_PREBLEND |
			VPP_VD1_PREBLEND |
			VPP_VD2_POSTBLEND |
			VPP_VD1_POSTBLEND |
			VPP_PREBLEND_EN |
			VPP_POSTBLEND_EN |
			0xf);
		if ((vpp_misc_set != vpp_misc_save) || force_flush) {
			u32 port_val[3] = {0, 0, 0};
			u32 vd1_port, vd2_port, icnt;
			u32 post_blend_reg[3] = {
				VD1_BLEND_SRC_CTRL,
				VD2_BLEND_SRC_CTRL,
				OSD2_BLEND_SRC_CTRL
			};

			/* just reset the select port */
			if ((glayer_info[0].cur_sel_port > 2) ||
			    (glayer_info[1].cur_sel_port > 2)) {
				glayer_info[0].cur_sel_port = 0;
				glayer_info[1].cur_sel_port = 1;
			}

			vd1_port = glayer_info[0].cur_sel_port;
			vd2_port = glayer_info[1].cur_sel_port;

			/* post bld premult*/
			port_val[0] |= (1 << 16);

			/* vd2 path sel */
			if (vpp_misc_set & VPP_VD2_POSTBLEND)
				port_val[1] |= (1 << 20);
			else
				port_val[1] &= ~(1 << 20);

			/* osd2 path sel */
			port_val[2] |= (1 << 20);

			if (vpp_misc_set & VPP_VD1_POSTBLEND) {
				 /* post src */
				port_val[vd1_port] |= (1 << 8);
				port_val[0] |=
					((1 << 4) | /* pre bld premult*/
					(1 << 0)); /* pre bld src 1 */
			} else {
				port_val[0] &= ~0xff;
			}

			if (vpp_misc_set & VPP_VD2_POSTBLEND)
				 /* post src */
				port_val[vd2_port] |= (2 << 8);
			else if (vpp_misc_set & VPP_VD2_PREBLEND)
				port_val[1] |=
					((1 << 4) | /* pre bld premult*/
					(2 << 0)); /* pre bld src 1 */

			for (icnt = 0; icnt < 3; icnt++)
				VSYNC_WR_MPEG_REG(
					post_blend_reg[icnt]
					+ vpp_off,
					port_val[icnt]);

			set_value = vpp_misc_set;
			set_value &=
				((1 << 29) | VPP_CM_ENABLE |
				(0x1ff << VPP_VD2_ALPHA_BIT) |
				VPP_VD2_PREBLEND |
				VPP_VD1_PREBLEND |
				VPP_VD2_POSTBLEND |
				VPP_VD1_POSTBLEND |
				0xf);
			if ((vpp_misc_set & VPP_VD2_PREBLEND) &&
			    (vpp_misc_set & VPP_VD1_PREBLEND))
				set_value |= VPP_PREBLEND_EN;
			if (mode & COMPOSE_MODE_BYPASS_CM)
				set_value &= ~VPP_CM_ENABLE;
			set_value |= VPP_POSTBLEND_EN;
			VSYNC_WR_MPEG_REG(
				VPP_MISC + vpp_off,
				set_value);
		}
	} else if (vpp_misc_save != vpp_misc_set) {
		VSYNC_WR_MPEG_REG(
			VPP_MISC + vpp_off,
			vpp_misc_set);
	}

	if ((vd_layer[1].dispbuf && video2_off_req) ||
	    (!vd_layer[1].dispbuf &&
	     (video1_off_req || video2_off_req)))
		disable_vd2_blend(&vd_layer[1]);

	if (video1_off_req)
		disable_vd1_blend(&vd_layer[0]);
}

/*********************************************************
 * frame canvas APIs
 *********************************************************/
static void vframe_canvas_set(
	struct canvas_config_s *config,
	u32 planes,
	u32 *index)
{
	int i;
	u32 *canvas_index = index;

	struct canvas_config_s *cfg = config;

	for (i = 0; i < planes; i++, canvas_index++, cfg++)
		canvas_config_config(*canvas_index, cfg);
}

static bool is_vframe_changed(
	u8 layer_id,
	struct vframe_s *cur_vf,
	struct vframe_s *new_vf)
{
	if (cur_vf && new_vf &&
	    (cur_vf->ratio_control & DISP_RATIO_ADAPTED_PICMODE) &&
	    (cur_vf->ratio_control == new_vf->ratio_control) &&
	    memcmp(&cur_vf->pic_mode, &new_vf->pic_mode,
		   sizeof(struct vframe_pic_mode_s)))
		return true;
	else if (cur_vf &&
		 memcmp(&cur_vf->pic_mode, &gpic_info[layer_id],
			sizeof(struct vframe_pic_mode_s)))
		return true;

	if ((glayer_info[layer_id].reverse != reverse) ||
	    (glayer_info[layer_id].proc_3d_type != process_3d_type))
		return true;

	if (cur_vf && new_vf &&
	    ((cur_vf->bufWidth != new_vf->bufWidth) ||
	     (cur_vf->width != new_vf->width) ||
	     (cur_vf->height != new_vf->height) ||
	     (cur_vf->sar_width != new_vf->sar_width) ||
	     (cur_vf->sar_height != new_vf->sar_height) ||
	     (cur_vf->bitdepth != new_vf->bitdepth) ||
	     (cur_vf->trans_fmt != new_vf->trans_fmt) ||
	     (cur_vf->ratio_control != new_vf->ratio_control) ||
	     ((cur_vf->type_backup & VIDTYPE_INTERLACE) !=
	      (new_vf->type_backup & VIDTYPE_INTERLACE)) ||
	     (cur_vf->type != new_vf->type)))
		return true;
	return false;
}

int get_layer_display_canvas(u8 layer_id)
{
	int ret = -1;
	u32 vd_off;
	u16 VD_IF0_CANVAS0 =
		(layer_id == 0) ?
		VD1_IF0_CANVAS0 :
		VD2_IF0_CANVAS0;
	struct video_layer_s *layer = NULL;

	if (layer_id >= MAX_VD_LAYER)
		return -1;

	layer = &vd_layer[layer_id];
	vd_off = layer->vd_reg_offt;
	ret = READ_VCBUS_REG(VD_IF0_CANVAS0 + vd_off);
	return ret;
}

int set_layer_display_canvas(
	u8 layer_id, struct vframe_s *vf,
	struct vpp_frame_par_s *cur_frame_par,
	struct disp_info_s *disp_info)
{
	u32 *cur_canvas_tbl;
	u8 cur_canvas_id;
	u32 vd_off, afbc_off;
	u16 VD_IF0_CANVAS0, VD_IF0_CANVAS1;
	u16 AFBC_HEADR =
		(layer_id == 0) ?
		AFBC_HEAD_BADDR :
		VD2_AFBC_HEAD_BADDR;
	u16 AFBC_BODY =
		(layer_id == 0) ?
		AFBC_BODY_BADDR :
		VD2_AFBC_BODY_BADDR;
	struct video_layer_s *layer = NULL;
	bool is_mvc = false;
	bool update_mif = true;

	if (layer_id >= MAX_VD_LAYER)
		return -1;

	if ((vf->type & VIDTYPE_MVC) && (layer_id == 0))
		is_mvc = true;

	layer = &vd_layer[layer_id];
	vd_off = layer->vd_reg_offt;
	afbc_off = layer->afbc_reg_offt;
	VD_IF0_CANVAS0 =
		(layer_id == 0) ?
		VD1_IF0_CANVAS0 :
		VD2_IF0_CANVAS0;
	VD_IF0_CANVAS1 =
		(layer_id == 0) ?
		VD1_IF0_CANVAS1 :
		VD2_IF0_CANVAS1;
	AFBC_HEADR =
		(layer_id == 0) ?
		AFBC_HEAD_BADDR :
		VD2_AFBC_HEAD_BADDR;
	AFBC_BODY =
		(layer_id == 0) ?
		AFBC_BODY_BADDR :
		VD2_AFBC_BODY_BADDR;

	cur_canvas_id = layer->cur_canvas_id;
	cur_canvas_tbl =
		&layer->canvas_tbl[cur_canvas_id][0];

	/* switch buffer */
	if (!glayer_info[layer_id].need_no_compress &&
	    (vf->type & VIDTYPE_COMPRESS)) {
		VSYNC_WR_MPEG_REG(
			AFBC_HEADR + afbc_off,
			vf->compHeadAddr >> 4);
		VSYNC_WR_MPEG_REG(
			AFBC_BODY + afbc_off,
			vf->compBodyAddr >> 4);
	}

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if ((layer_id == 0) &&
	    is_di_post_link_on())
		update_mif = false;
#endif
	if (vf->canvas0Addr == 0)
		update_mif = false;

	if (update_mif) {
		if (vf->canvas0Addr != (u32)-1) {
			canvas_copy(
				vf->canvas0Addr & 0xff,
				cur_canvas_tbl[0]);
			canvas_copy(
				(vf->canvas0Addr >> 8) & 0xff,
				cur_canvas_tbl[1]);
			canvas_copy(
				(vf->canvas0Addr >> 16) & 0xff,
				cur_canvas_tbl[2]);
		} else {
			vframe_canvas_set(
				&vf->canvas0_config[0],
				vf->plane_num,
				&cur_canvas_tbl[0]);
		}
		if ((layer_id == 0) &&
		    (is_mvc || process_3d_type)) {
			if (vf->canvas1Addr != (u32)-1) {
				canvas_copy(
					vf->canvas1Addr & 0xff,
					cur_canvas_tbl[3]);
				canvas_copy(
					(vf->canvas1Addr >> 8) & 0xff,
					cur_canvas_tbl[4]);
				canvas_copy(
					(vf->canvas1Addr >> 16) & 0xff,
					cur_canvas_tbl[5]);
			} else {
				vframe_canvas_set(
					&vf->canvas1_config[0],
					vf->plane_num,
					&cur_canvas_tbl[3]);
			}
		}
		VSYNC_WR_MPEG_REG(
			VD_IF0_CANVAS0 + vd_off,
			layer->disp_canvas[cur_canvas_id][0]);

		if (is_mvc)
			VSYNC_WR_MPEG_REG(
				VD2_IF0_CANVAS0 + vd_off,
				layer->disp_canvas[cur_canvas_id][1]);
		if (cur_frame_par &&
		    (cur_frame_par->vpp_2pic_mode == 1)) {
			VSYNC_WR_MPEG_REG(
				VD_IF0_CANVAS1 + vd_off,
				layer->disp_canvas[cur_canvas_id][0]);
			if (is_mvc)
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS1 + vd_off,
					layer->disp_canvas[cur_canvas_id][0]);
		} else {
			VSYNC_WR_MPEG_REG(
				VD_IF0_CANVAS1 + vd_off,
				layer->disp_canvas[cur_canvas_id][1]);
			if (is_mvc)
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS1 + vd_off,
					layer->disp_canvas[cur_canvas_id][0]);
		}
		if (cur_frame_par && disp_info &&
		    (disp_info->proc_3d_type & MODE_3D_ENABLE) &&
		    (disp_info->proc_3d_type & MODE_3D_TO_2D_R) &&
		    (cur_frame_par->vpp_2pic_mode == VPP_SELECT_PIC1)) {
			VSYNC_WR_MPEG_REG(
				VD_IF0_CANVAS0 + vd_off,
				layer->disp_canvas[cur_canvas_id][1]);
			VSYNC_WR_MPEG_REG(
				VD_IF0_CANVAS1 + vd_off,
				layer->disp_canvas[cur_canvas_id][1]);
			if (is_mvc) {
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS0 + vd_off,
					layer->disp_canvas[cur_canvas_id][1]);
				VSYNC_WR_MPEG_REG(
					VD2_IF0_CANVAS1 + vd_off,
					layer->disp_canvas[cur_canvas_id][1]);
			}
		}
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		layer->next_canvas_id = layer->cur_canvas_id ? 0 : 1;
#endif
	}
	return 0;
}

u32 *get_canvase_tbl(u8 layer_id)
{
	struct video_layer_s *layer = NULL;

	if (layer_id >= MAX_VD_LAYER)
		return NULL;

	layer = &vd_layer[layer_id];
	return (u32 *)&layer->canvas_tbl[0][0];
}

s32 layer_swap_frame(
	struct vframe_s *vf, u8 layer_id,
	bool force_toggle,
	const struct vinfo_s *vinfo)
{
	bool first_picture = false;
	bool enable_layer = false;
	bool frame_changed;
	struct video_layer_s *layer = NULL;
	struct disp_info_s *layer_info = NULL;
	int ret = vppfilter_success;
	bool is_mvc = false;

	if (!vf)
		return vppfilter_fail;

	layer = &vd_layer[layer_id];
	layer_info = &glayer_info[layer_id];

	if ((vf->type & VIDTYPE_MVC) && (layer_id == 0))
		is_mvc = true;

	if (layer->dispbuf != vf) {
		layer->new_vframe_count++;
		layer->new_frame = true;
		if (!layer->dispbuf ||
		    (layer->new_vframe_count == 1) ||
		    (is_local_vf(layer->dispbuf)))
			first_picture = true;
	}

	if (layer->property_changed) {
		layer->force_config_cnt = 2;
		layer->property_changed = false;
		force_toggle = true;
	}
	if (layer->force_config_cnt > 0) {
		layer->force_config_cnt--;
		force_toggle = true;
	}

	if ((layer->global_debug & DEBUG_FLAG_BLACKOUT) && first_picture)
		pr_info("first swap picture {%d,%d} pts:%x,\n",
			vf->width, vf->height, vf->pts);

	set_layer_display_canvas(
		layer->layer_id,
		vf, layer->cur_frame_par, layer_info);

	frame_changed = is_vframe_changed(
		layer->layer_id,
		layer->dispbuf, vf);

	/* enable new config on the new frames */
	if (first_picture || force_toggle || frame_changed) {
		layer->next_frame_par =
			(&layer->frame_parms[0] == layer->next_frame_par) ?
			&layer->frame_parms[1] : &layer->frame_parms[0];
		/* FIXME: remove the global variables */
		glayer_info[layer->layer_id].reverse = reverse;
		glayer_info[layer->layer_id].proc_3d_type =
			process_3d_type;

		ret = vpp_set_filters(
			&glayer_info[layer->layer_id], vf,
			layer->next_frame_par, vinfo,
			(is_dolby_vision_on() &&
			is_dolby_vision_stb_mode() &&
			for_dolby_vision_certification()),
			1);

		memcpy(&gpic_info[layer->layer_id], &vf->pic_mode,
		       sizeof(struct vframe_pic_mode_s));

		if ((ret == vppfilter_success_and_changed) ||
		    (ret == vppfilter_changed_but_hold))
			layer->property_changed = true;

		if (ret != vppfilter_changed_but_hold)
			layer->new_vpp_setting = true;

		if (layer->layer_id == 0) {
			if (((vf->width > 1920) && (vf->height > 1088)) ||
			    ((vf->type & VIDTYPE_COMPRESS) &&
			     (vf->compWidth > 1920) &&
			     (vf->compHeight > 1080))) {
				VPU_VD1_CLK_SWITCH(1);
			} else {
				VPU_VD1_CLK_SWITCH(0);
			}
		} else {
			if (((vf->width > 1920) && (vf->height > 1088)) ||
			    ((vf->type & VIDTYPE_COMPRESS) &&
			     (vf->compWidth > 1920) &&
			     (vf->compHeight > 1080))) {
				VPU_VD2_CLK_SWITCH(1);
			} else {
				VPU_VD2_CLK_SWITCH(0);
			}
		}
	}

	if (((vf->type & VIDTYPE_NO_VIDEO_ENABLE) == 0) &&
	    (!layer->force_config_cnt || (vf != layer->dispbuf))) {
		if (layer->disable_video == VIDEO_DISABLE_FORNEXT) {
			enable_layer = true;
			layer->disable_video = VIDEO_DISABLE_NONE;
		}
		if (first_picture &&
		    (layer->disable_video !=
		     VIDEO_DISABLE_NORMAL))
			enable_layer = true;
		if (enable_layer) {
			if (layer->layer_id == 0)
				enable_video_layer();
			if ((layer->layer_id == 1) || is_mvc)
				enable_video_layer2();
		}
	}

	if (first_picture)
		layer->new_vpp_setting = true;
	layer->dispbuf = vf;
	return ret;
}

/*********************************************************
 * vout APIs
 *********************************************************/
int detect_vout_type(const struct vinfo_s *vinfo)
{
	int vout_type = VOUT_TYPE_PROG;

	if (vinfo && (vinfo->field_height != vinfo->height)) {
		if ((vinfo->height == 576) || (vinfo->height == 480))
			vout_type = (READ_VCBUS_REG(ENCI_INFO_READ) &
				(1 << 29)) ?
				VOUT_TYPE_BOT_FIELD : VOUT_TYPE_TOP_FIELD;
		else if (vinfo->height == 1080)
			vout_type = (((READ_VCBUS_REG(ENCP_INFO_READ) >> 16) &
				0x1fff) < 562) ?
				VOUT_TYPE_TOP_FIELD : VOUT_TYPE_BOT_FIELD;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (is_vsync_rdma_enable()) {
			if (vout_type == VOUT_TYPE_TOP_FIELD)
				vout_type = VOUT_TYPE_BOT_FIELD;
			else if (vout_type == VOUT_TYPE_BOT_FIELD)
				vout_type = VOUT_TYPE_TOP_FIELD;
		}
#endif
	}
	return vout_type;
}

int calc_hold_line(void)
{
	if ((READ_VCBUS_REG(ENCI_VIDEO_EN) & 1) == 0)
		return READ_VCBUS_REG(ENCP_VIDEO_VAVON_BLINE) >> 1;
	else
		return READ_VCBUS_REG(ENCP_VFIFO2VD_LINE_TOP_START) >> 1;
}

u32 get_cur_enc_line(void)
{
	u32 ret = 0;

	switch (READ_VCBUS_REG(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		ret = (READ_VCBUS_REG(ENCL_INFO_READ) >> 16) & 0x1fff;
		break;
	case 1:
		ret = (READ_VCBUS_REG(ENCI_INFO_READ) >> 16) & 0x1fff;
		break;
	case 2:
		ret = (READ_VCBUS_REG(ENCP_INFO_READ) >> 16) & 0x1fff;
		break;
	case 3:
		ret = (READ_VCBUS_REG(ENCT_INFO_READ) >> 16) & 0x1fff;
		break;
	}
	return ret;
}

static void do_vpu_delay_work(struct work_struct *work)
{
	unsigned long flags;
	unsigned int r;

	if (vpu_delay_work_flag & VPU_VIDEO_LAYER1_CHANGED)
		vpu_delay_work_flag &= ~VPU_VIDEO_LAYER1_CHANGED;

	if (vpu_delay_work_flag & VPU_VIDEO_LAYER2_CHANGED)
		vpu_delay_work_flag &= ~VPU_VIDEO_LAYER2_CHANGED;

	spin_lock_irqsave(&delay_work_lock, flags);

	if (vpu_delay_work_flag & VPU_DELAYWORK_VPU_VD1_CLK) {
		vpu_delay_work_flag &= ~VPU_DELAYWORK_VPU_VD1_CLK;

		spin_unlock_irqrestore(&delay_work_lock, flags);

		if (vpu_vd1_clk_level > 0)
			request_vpu_clk_vmod(360000000, VPU_VIU_VD1);
		else
			release_vpu_clk_vmod(VPU_VIU_VD1);

		spin_lock_irqsave(&delay_work_lock, flags);
	}

	if (vpu_delay_work_flag & VPU_DELAYWORK_VPU_VD2_CLK) {
		vpu_delay_work_flag &= ~VPU_DELAYWORK_VPU_VD2_CLK;

		/* FIXME: check the right vd2 clock */
#ifdef CHECK_LATER
		spin_unlock_irqrestore(&delay_work_lock, flags);

		if (vpu_vd2_clk_level > 0)
			request_vpu_clk_vmod(360000000, VPU_VIU_VD2);
		else
			release_vpu_clk_vmod(VPU_VIU_VD2);

		spin_lock_irqsave(&delay_work_lock, flags);
#endif
	}

	r = READ_VCBUS_REG(VPP_MISC + cur_dev->vpp_off);

	if (vpu_mem_power_off_count > 0) {
		vpu_mem_power_off_count--;

		if (vpu_mem_power_off_count == 0) {
			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_VD1) &&
			    ((r & VPP_VD1_PREBLEND) == 0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_VD1;

				switch_vpu_mem_pd_vmod(
					VPU_VIU_VD1,
					VPU_MEM_POWER_DOWN);
				switch_vpu_mem_pd_vmod(
					VPU_AFBC_DEC,
					VPU_MEM_POWER_DOWN);
				switch_vpu_mem_pd_vmod(
					VPU_DI_POST,
					VPU_MEM_POWER_DOWN);
				if (!legacy_vpp)
					switch_vpu_mem_pd_vmod(
						VPU_VD1_SCALE,
						VPU_MEM_POWER_DOWN);
			}

			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_VD2) &&
			    ((r & VPP_VD2_PREBLEND) == 0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_VD2;

				switch_vpu_mem_pd_vmod(
					VPU_VIU_VD2,
					VPU_MEM_POWER_DOWN);
				switch_vpu_mem_pd_vmod(
					VPU_AFBC_DEC1,
					VPU_MEM_POWER_DOWN);
				if (!legacy_vpp)
					switch_vpu_mem_pd_vmod(
						VPU_VD2_SCALE,
						VPU_MEM_POWER_DOWN);
			}
		}
	}
	if (dv_mem_power_off_count > 0) {
		dv_mem_power_off_count--;
		if (dv_mem_power_off_count == 0) {
			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0;
				switch_vpu_mem_pd_vmod(
					VPU_DOLBY0,
					VPU_MEM_POWER_DOWN);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A)) {
				vpu_delay_work_flag &=
					 ~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A;
				switch_vpu_mem_pd_vmod(
					VPU_DOLBY1A,
					VPU_MEM_POWER_DOWN);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B)) {
				vpu_delay_work_flag &=
					~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B;
				switch_vpu_mem_pd_vmod(
					VPU_DOLBY1B,
					VPU_MEM_POWER_DOWN);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2)) {
				vpu_delay_work_flag &=
				~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2;
				switch_vpu_mem_pd_vmod(
					VPU_DOLBY2,
					VPU_MEM_POWER_DOWN);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3)) {
				vpu_delay_work_flag &=
				~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3;
				switch_vpu_mem_pd_vmod(
					VPU_DOLBY_CORE3,
					VPU_MEM_POWER_DOWN);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY)) {
				vpu_delay_work_flag &=
				~VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY;
				switch_vpu_mem_pd_vmod(
					VPU_PRIME_DOLBY_RAM,
					VPU_MEM_POWER_DOWN);
			}
		}
	}
	spin_unlock_irqrestore(&delay_work_lock, flags);
}

void vpu_work_process(void)
{
	if (vpu_delay_work_flag)
		schedule_work(&vpu_delay_work);
}

/*********************************************************
 * Init APIs
 *********************************************************/
static void init_layer_canvas(
	struct video_layer_s *layer, u32 start_canvas)
{
	u32 i, j;

	if (!layer)
		return;

	for (i = 0; i < CANVAS_TABLE_CNT; i++) {
		for (j = 0; j < 6; j++)
			layer->canvas_tbl[i][j] =
				start_canvas++;
		layer->disp_canvas[i][0] =
			(layer->canvas_tbl[i][2] << 16) |
			(layer->canvas_tbl[i][1] << 8) |
			layer->canvas_tbl[i][0];
		layer->disp_canvas[i][1] =
			(layer->canvas_tbl[i][5] << 16) |
			(layer->canvas_tbl[i][4] << 8) |
			layer->canvas_tbl[i][3];
	}
}

int video_hw_init(void)
{
	u32 cur_hold_line;

	if (!legacy_vpp) {
		WRITE_VCBUS_REG_BITS(
			VPP_OFIFO_SIZE, 0x1000,
			VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
		WRITE_VCBUS_REG_BITS(
			VPP_MATRIX_CTRL, 0, 10, 5);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		WRITE_VCBUS_REG_BITS(
			VPP_OFIFO_SIZE, 0xfff,
			VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);

	WRITE_VCBUS_REG(VPP_PREBLEND_VD1_H_START_END, 4096);
	WRITE_VCBUS_REG(VPP_BLEND_VD2_H_START_END, 4096);

	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		/* fifo max size on txl :128*3=384[0x180]  */
		WRITE_VCBUS_REG(
			VD1_IF0_LUMA_FIFO_SIZE +
			vd_layer[0].vd_reg_offt, 0x180);
		WRITE_VCBUS_REG(
			VD2_IF0_LUMA_FIFO_SIZE +
			vd_layer[1].vd_reg_offt, 0x180);
	}

	/* default 10bit setting for gxm */
	if (is_meson_gxm_cpu()) {
		WRITE_VCBUS_REG_BITS(VIU_MISC_CTRL1, 0xff, 16, 8);
		WRITE_VCBUS_REG(VPP_DOLBY_CTRL, 0x22000);
		/*
		 *default setting is black for dummy data1& dumy data0,
		 *for dummy data1 the y/cb/cr data width is 10bit on gxm,
		 *for dummy data the y/cb/cr data width is 8bit but
		 *vpp_dummy_data will be left shift 2bit auto on gxm!!!
		 */
		WRITE_VCBUS_REG(VPP_DUMMY_DATA1, 0x1020080);
		WRITE_VCBUS_REG(VPP_DUMMY_DATA, 0x42020);
	} else if (is_meson_txlx_cpu() ||
		cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/*black 10bit*/
		WRITE_VCBUS_REG(VPP_DUMMY_DATA, 0x4080200);
	}
	/* temp: enable VPU arb mem */
	switch_vpu_mem_pd_vmod(VPU_VPU_ARB, VPU_MEM_POWER_ON);
	/*disable sr default when power up*/
	WRITE_VCBUS_REG(VPP_SRSHARP0_CTRL, 0);
	WRITE_VCBUS_REG(VPP_SRSHARP1_CTRL, 0);

	cur_hold_line = READ_VCBUS_REG(VPP_HOLD_LINES + cur_dev->vpp_off);
	cur_hold_line = cur_hold_line & 0xff;

	if (cur_hold_line > 0x1f)
		vpp_hold_line = 0x1f;
	else
		vpp_hold_line = cur_hold_line;

	/* Temp force set dmc */
	if (!legacy_vpp) {
		WRITE_DMCREG(
			DMC_AM0_CHAN_CTRL,
			0x8ff403cf);
		/* for vd1 & vd2 dummy alpha*/
		WRITE_VCBUS_REG(
			VPP_POST_BLEND_DUMMY_ALPHA,
			0x7fffffff);
		WRITE_VCBUS_REG_BITS(
			VPP_MISC1, 0x100, 0, 9);
	}
	if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
		/* force bypass dolby for TL1, no dolby function */
		if (is_meson_tl1_cpu())
			WRITE_VCBUS_REG_BITS(
				DOLBY_PATH_CTRL, 0xf, 0, 6);
		/* disable latch for sr core0/1 scaler */
		WRITE_VCBUS_REG_BITS(
			SRSHARP0_SHARP_SYNC_CTRL, 1, 0, 1);
		WRITE_VCBUS_REG_BITS(
			SRSHARP0_SHARP_SYNC_CTRL, 1, 8, 1);
		WRITE_VCBUS_REG_BITS(
			SRSHARP1_SHARP_SYNC_CTRL, 1, 8, 1);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		WRITE_VCBUS_REG_BITS(
			SRSHARP0_SHARP_SYNC_CTRL, 1, 0, 1);
		WRITE_VCBUS_REG_BITS(
			SRSHARP0_SHARP_SYNC_CTRL, 1, 8, 1);
	}
	return 0;
}

int video_early_init(void)
{
	int r = 0, i;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		cur_dev->viu_off = 0x3200 - 0x1a50;
		legacy_vpp = false;
	}

	/* check super scaler support status */
	vpp_super_scaler_support();
	/* adaptive config bypass ratio */
	vpp_bypass_ratio_config();

	memset(vd_layer, 0, sizeof(vd_layer));
	for (i = 0; i < MAX_VD_LAYER; i++) {
		vd_layer[i].layer_id = i;
		vd_layer[i].cur_canvas_id = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		vd_layer[i].next_canvas_id = 1;
#endif
		/* vd_layer[i].global_output = 1; */
		vd_layer[i].keep_frame_id = i;
		vd_layer[i].disable_video = VIDEO_DISABLE_FORNEXT;
		vpp_disp_info_init(&glayer_info[i], i);
		memset(&gpic_info[i], 0, sizeof(struct vframe_pic_mode_s));
		glayer_info[i].wide_mode = 1;
		glayer_info[i].zorder = reference_zorder - 2 + i;
		glayer_info[i].cur_sel_port = i;
		glayer_info[i].last_sel_port = i;
		glayer_info[i].display_path_id = VFM_PATH_DEF;
		glayer_info[i].need_no_compress = false;
		if (legacy_vpp) {
			glayer_info[i].afbc_support = true;
			glayer_info[i].pps_support =
				(i == 0) ? true : false;
		} else if (is_meson_tl1_cpu()) {
			glayer_info[i].afbc_support =
				(i == 0) ? true : false;
			glayer_info[i].pps_support =
				(i == 0) ? true : false;
		} else if (is_meson_tm2_cpu()) {
			glayer_info[i].afbc_support =
				(i == 0) ? true : false;
			glayer_info[i].pps_support = true;
		} else {
			glayer_info[i].afbc_support = true;
			glayer_info[i].pps_support = true;
		}
	}
	/* only enable vd1 as default */
	vd_layer[0].global_output = 1;
	vd_layer[0].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[0].afbc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[0].vd_reg_offt = 0 + cur_dev->viu_off;
	vd_layer[0].layer_alpha = 0x100;
	vd_layer[1].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[1].afbc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[1].vd_reg_offt = 0 + cur_dev->viu_off;
	/* g12a has no alpha overflow check in hardware */
	vd_layer[1].layer_alpha = legacy_vpp ? 0x1ff : 0x100;

	INIT_WORK(&vpu_delay_work, do_vpu_delay_work);

	init_layer_canvas(&vd_layer[0], LAYER1_CANVAS_BASE_INDEX);
	init_layer_canvas(&vd_layer[1], LAYER2_CANVAS_BASE_INDEX);
	return r;
}

int video_late_uninit(void)
{
	return 0;
}

MODULE_PARM_DESC(vpp_hold_line, "\n vpp_hold_line\n");
module_param(vpp_hold_line, uint, 0664);

MODULE_PARM_DESC(bypass_cm, "\n bypass_cm\n");
module_param(bypass_cm, bool, 0664);

MODULE_PARM_DESC(reference_zorder, "\n reference_zorder\n");
module_param(reference_zorder, uint, 0664);

MODULE_PARM_DESC(video_mute_on, "\n video_mute_on\n");
module_param(video_mute_on, bool, 0664);
