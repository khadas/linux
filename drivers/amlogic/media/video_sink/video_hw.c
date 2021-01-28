// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
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
#include <linux/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_reg.h"

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
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "../common/vfm/vfm.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "video_receiver.h"
#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
#include <linux/amlogic/media/lut_dma/lut_dma.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
struct video_layer_s vd_layer[MAX_VD_LAYER];
struct disp_info_s glayer_info[MAX_VD_LAYER];

struct video_dev_s video_dev;
struct video_dev_s *cur_dev = &video_dev;
bool legacy_vpp = true;

static bool bypass_cm;
bool hscaler_8tap_enable[MAX_VD_LAYER];
int pre_hscaler_ntap_enable[MAX_VD_LAYER];
int pre_hscaler_ntap_set[MAX_VD_LAYER];
int pre_hscaler_ntap[MAX_VD_LAYER];
int pre_vscaler_ntap_enable[MAX_VD_LAYER];
int pre_vscaler_ntap_set[MAX_VD_LAYER];
int pre_vscaler_ntap[MAX_VD_LAYER];

static DEFINE_SPINLOCK(video_onoff_lock);
static DEFINE_SPINLOCK(video2_onoff_lock);
static DEFINE_SPINLOCK(video3_onoff_lock);
/* VPU delay work */
#define VPU_DELAYWORK_VPU_VD1_CLK		BIT(0)
#define VPU_DELAYWORK_VPU_VD2_CLK		BIT(1)
#define VPU_DELAYWORK_VPU_VD3_CLK		BIT(2)

#define VPU_DELAYWORK_MEM_POWER_OFF_VD1		BIT(3)
#define VPU_DELAYWORK_MEM_POWER_OFF_VD2		BIT(4)
#define VPU_DELAYWORK_MEM_POWER_OFF_VD3		BIT(5)

#define VPU_VIDEO_LAYER1_CHANGED		BIT(6)
#define VPU_VIDEO_LAYER2_CHANGED		BIT(7)
#define VPU_VIDEO_LAYER3_CHANGED		BIT(8)

#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0		BIT(9)
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A		BIT(10)
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B		BIT(11)
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2		BIT(12)
#define VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3		BIT(13)
#define VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY		BIT(14)

#define VPU_MEM_POWEROFF_DELAY	100
#define DV_MEM_POWEROFF_DELAY	2

static struct work_struct vpu_delay_work;
static int vpu_vd1_clk_level;
static int vpu_vd2_clk_level;
static int vpu_vd3_clk_level;
static DEFINE_SPINLOCK(delay_work_lock);
static int vpu_delay_work_flag;
static int vpu_mem_power_off_count;
static int dv_mem_power_off_count;
static struct vpu_dev_s *vd1_vpu_dev;
static struct vpu_dev_s *afbc_vpu_dev;
static struct vpu_dev_s *di_post_vpu_dev;
static struct vpu_dev_s *vd1_scale_vpu_dev;
static struct vpu_dev_s *vpu_fgrain0;
static struct vpu_dev_s *vd2_vpu_dev;
static struct vpu_dev_s *afbc2_vpu_dev;
static struct vpu_dev_s *vd2_scale_vpu_dev;

static struct vpu_dev_s *vpu_fgrain1;

static struct vpu_dev_s *vd3_vpu_dev;
static struct vpu_dev_s *afbc3_vpu_dev;
static struct vpu_dev_s *vd3_scale_vpu_dev;
static struct vpu_dev_s *vpu_fgrain2;

static struct vpu_dev_s *vpu_dolby0;
static struct vpu_dev_s *vpu_dolby1a;
static struct vpu_dev_s *vpu_dolby1b;
static struct vpu_dev_s *vpu_dolby2;
static struct vpu_dev_s *vpu_dolby_core3;
static struct vpu_dev_s *vpu_prime_dolby_ram;

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

#define VPU_VD3_CLK_SWITCH(level) \
	do { \
		unsigned long flags; \
		int next_level = (level); \
		if (vpu_vd3_clk_level != next_level) { \
			vpu_vd3_clk_level = next_level; \
			spin_lock_irqsave(&delay_work_lock, flags); \
			vpu_delay_work_flag |= \
				VPU_DELAYWORK_VPU_VD3_CLK; \
			spin_unlock_irqrestore(&delay_work_lock, flags); \
		} \
	} while (0)

#ifdef DI_POST_PWR
#define VD1_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD1; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		vpu_dev_mem_power_on(vd1_vpu_dev); \
		vpu_dev_mem_power_on(afbc_vpu_dev); \
		vpu_dev_mem_power_on(di_post_vpu_dev); \
		if (!legacy_vpp) \
			vpu_dev_mem_power_on(vd1_scale_vpu_dev); \
		if (glayer_info[0].fgrain_support) \
			vpu_dev_mem_power_on(vpu_fgrain0); \

	} while (0)
#else
#define VD1_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD1; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		vpu_dev_mem_power_on(vd1_vpu_dev); \
		vpu_dev_mem_power_on(afbc_vpu_dev); \
		if (!legacy_vpp) \
			vpu_dev_mem_power_on(vd1_scale_vpu_dev); \
		if (glayer_info[0].fgrain_support) \
			vpu_dev_mem_power_on(vpu_fgrain0); \
	} while (0)
#endif
#define VD2_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD2; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		vpu_dev_mem_power_on(vd2_vpu_dev); \
		vpu_dev_mem_power_on(afbc2_vpu_dev); \
		if (!legacy_vpp) \
			vpu_dev_mem_power_on(vd2_scale_vpu_dev); \
		if (glayer_info[1].fgrain_support) \
			vpu_dev_mem_power_on(vpu_fgrain1); \
	} while (0)
#define VD3_MEM_POWER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag &= ~VPU_DELAYWORK_MEM_POWER_OFF_VD3; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
		vpu_dev_mem_power_on(vd3_vpu_dev); \
		vpu_dev_mem_power_on(afbc3_vpu_dev); \
		if (!legacy_vpp) \
			vpu_dev_mem_power_on(vd3_scale_vpu_dev); \
		if (glayer_info[2].fgrain_support) \
			vpu_dev_mem_power_on(vpu_fgrain2); \
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
#define VD3_MEM_POWER_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&delay_work_lock, flags); \
		vpu_delay_work_flag |= VPU_DELAYWORK_MEM_POWER_OFF_VD3; \
		vpu_mem_power_off_count = VPU_MEM_POWEROFF_DELAY; \
		spin_unlock_irqrestore(&delay_work_lock, flags); \
	} while (0)

#define VIDEO_LAYER_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video_onoff_lock, flags); \
		if (vd_layer[0].onoff_state != VIDEO_ENABLE_STATE_ON_PENDING) \
			vd_layer[0].onoff_state = \
			VIDEO_ENABLE_STATE_ON_PENDING; \
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
			vd_layer[1].onoff_state = \
			VIDEO_ENABLE_STATE_ON_PENDING; \
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

#define VIDEO_LAYER3_ON() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video3_onoff_lock, flags); \
		if (vd_layer[2].onoff_state != VIDEO_ENABLE_STATE_ON_PENDING) \
			vd_layer[2].onoff_state = \
			VIDEO_ENABLE_STATE_ON_PENDING; \
		vd_layer[2].enabled = 1; \
		vd_layer[2].enabled_status_saved = 1; \
		spin_unlock_irqrestore(&video3_onoff_lock, flags); \
	} while (0)

#define VIDEO_LAYER3_OFF() \
	do { \
		unsigned long flags; \
		spin_lock_irqsave(&video3_onoff_lock, flags); \
		vd_layer[2].onoff_state = VIDEO_ENABLE_STATE_OFF_REQ; \
		vd_layer[2].enabled = 0; \
		vd_layer[2].enabled_status_saved = 0; \
		spin_unlock_irqrestore(&video3_onoff_lock, flags); \
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
#define enable_video_layer3()  \
	do { \
		VD3_MEM_POWER_ON(); \
		VIDEO_LAYER3_ON(); \
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
				vd_layer[0].vd_afbc_reg.afbc_enable, 0); \
			WRITE_VCBUS_REG( \
				vd_layer[0].vd_mif_reg.vd_if0_gen_reg, 0); \
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
				vd_layer[1].vd_afbc_reg.afbc_enable, 0); \
			WRITE_VCBUS_REG( \
				vd_layer[1].vd_mif_reg.vd_if0_gen_reg, 0); \
		} \
		VIDEO_LAYER2_OFF(); \
		VD2_MEM_POWER_OFF(); \
		if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("VIDEO: DisableVideoLayer2()\n"); \
		} \
	} while (0)

#define disable_video_layer3(async) \
	do { \
		if (!(async)) { \
			if (!legacy_vpp) \
				WRITE_VCBUS_REG( \
					VD3_BLEND_SRC_CTRL + \
					vd_layer[2].misc_reg_offt, 0); \
			WRITE_VCBUS_REG( \
				vd_layer[2].vd_afbc_reg.afbc_enable, 0); \
			WRITE_VCBUS_REG( \
				vd_layer[2].vd_mif_reg.vd_if0_gen_reg, 0); \
		} \
		VIDEO_LAYER3_OFF(); \
		VD3_MEM_POWER_OFF(); \
		if (vd_layer[2].global_debug & DEBUG_FLAG_BLACKOUT) {  \
			pr_info("VIDEO: DisableVideoLayer3()\n"); \
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
			vd_layer[0].vd_afbc_reg.afbc_enable, 0); \
		WRITE_VCBUS_REG( \
			vd_layer[0].vd_mif_reg.vd_if0_gen_reg, \
			0); \
		if (!legacy_vpp) \
			WRITE_VCBUS_REG( \
				VD2_BLEND_SRC_CTRL + \
				vd_layer[1].misc_reg_offt, 0); \
		WRITE_VCBUS_REG( \
			vd_layer[1].vd_afbc_reg.afbc_enable, 0); \
		WRITE_VCBUS_REG( \
			vd_layer[1].vd_mif_reg.vd_if0_gen_reg, 0); \
		if (cur_dev->max_vd_layers == 3) { \
			if (!legacy_vpp) \
				WRITE_VCBUS_REG( \
					VD3_BLEND_SRC_CTRL + \
					vd_layer[2].misc_reg_offt, 0); \
			WRITE_VCBUS_REG( \
				vd_layer[2].vd_afbc_reg.afbc_enable, 0); \
			WRITE_VCBUS_REG( \
				vd_layer[2].vd_mif_reg.vd_if0_gen_reg, 0); \
		} \
		pr_info("VIDEO: disable_video_all_layer_nodelay()\n"); \
	} while (0)

void dv_mem_power_off(enum vpu_mod_e mode)
{
#ifdef DV_PWR
	unsigned long flags;
#endif
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
	/*currently power function not ok*/
#ifdef DV_PWR
	spin_lock_irqsave(&delay_work_lock, flags);
	vpu_delay_work_flag |= dv_flag;
	dv_mem_power_off_count = DV_MEM_POWEROFF_DELAY;
	spin_unlock_irqrestore(&delay_work_lock, flags);
#endif
}
EXPORT_SYMBOL(dv_mem_power_off);

void dv_mem_power_on(enum vpu_mod_e mode)
{
	unsigned long flags;
	unsigned int dv_flag;
	static struct vpu_dev_s *p_dev;

	if (mode == VPU_DOLBY0) {
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0;
		p_dev = vpu_dolby0;
	} else if (mode == VPU_DOLBY1A) {
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A;
		p_dev = vpu_dolby1a;
	} else if (mode == VPU_DOLBY1B) {
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B;
		p_dev = vpu_dolby1b;
	} else if (mode == VPU_DOLBY2) {
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2;
		p_dev = vpu_dolby2;
	} else if (mode == VPU_DOLBY_CORE3) {
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3;
		p_dev = vpu_dolby_core3;
	} else if (mode == VPU_PRIME_DOLBY_RAM) {
		dv_flag = VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY;
		p_dev = vpu_prime_dolby_ram;
	} else {
		return;
	}
	spin_lock_irqsave(&delay_work_lock, flags);
	vpu_delay_work_flag &= ~dv_flag;
	spin_unlock_irqrestore(&delay_work_lock, flags);
	vpu_dev_mem_power_on(p_dev);
}
EXPORT_SYMBOL(dv_mem_power_on);

/*get mem power status on the way*/
int get_dv_mem_power_flag(enum vpu_mod_e mode)
{
#ifdef DV_PWR
	unsigned long flags;
#endif
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
	/*currently power function not ok*/
#ifndef DV_PWR
	return ret;
#else
	spin_lock_irqsave(&delay_work_lock, flags);
	if (vpu_delay_work_flag & dv_flag)
		ret = VPU_MEM_POWER_DOWN;
	spin_unlock_irqrestore(&delay_work_lock, flags);
	return ret;
#endif
}
EXPORT_SYMBOL(get_dv_mem_power_flag);

/*get vpu mem power status*/
int get_dv_vpu_mem_power_status(enum vpu_mod_e mode)
{
	int ret = VPU_MEM_POWER_ON;
	static struct vpu_dev_s *p_dev;

	if (mode == VPU_DOLBY0)
		p_dev = vpu_dolby0;
	else if (mode == VPU_DOLBY1A)
		p_dev = vpu_dolby1a;
	else if (mode == VPU_DOLBY1B)
		p_dev = vpu_dolby1b;
	else if (mode == VPU_DOLBY2)
		p_dev = vpu_dolby2;
	else if (mode == VPU_DOLBY_CORE3)
		p_dev = vpu_dolby_core3;
	else if (mode == VPU_PRIME_DOLBY_RAM)
		p_dev = vpu_prime_dolby_ram;
	else
		return ret;
	/*currently power function not ok*/
#ifndef DV_PWR
	return ret;
#else
	ret = vpu_dev_mem_pd_get(p_dev);
	return ret;
#endif
}
EXPORT_SYMBOL(get_dv_vpu_mem_power_status);

/*********************************************************/
static struct vframe_pic_mode_s gpic_info[MAX_VD_LAYERS];
static u32 reference_zorder = 128;
static u32 vpp_hold_line = 8;
static unsigned int cur_vf_flag;
static u32 vpp_ofifo_size = 0x1000;
static u32 conv_lbuf_len = 0x100;

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

void dv_vf_light_reg_provider(void)
{
}

void dv_vf_light_unreg_provider(void)
{
}

void dolby_vision_update_backlight(void)
{
}

bool is_dovi_frame(struct vframe_s *vf)
{
	return false;
}

void dolby_vision_set_toggle_flag(int flag)
{
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

u32 get_videopip2_enabled(void)
{
	return vd_layer[2].enabled && vd_layer[2].global_output;
}
EXPORT_SYMBOL(get_videopip2_enabled);

void set_video_enabled(u32 value, u32 index)
{
	u32 disable_video = value ? 0 : 1;

	if (index >= MAX_VD_LAYER)
		return;
	vd_layer[index].global_output = value;
	if (index == 0)
		_video_set_disable(disable_video);
	else
		_videopip_set_disable(index, disable_video);
}
EXPORT_SYMBOL(set_video_enabled);

bool is_di_on(void)
{
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (DI_POST_REG_RD(DI_IF1_GEN_REG) & 0x1)
		ret = true;
#endif
	return ret;
}

bool is_di_post_on(void)
{
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (DI_POST_REG_RD(DI_POST_CTRL) & 0x100)
		ret = true;
#endif
	return ret;
}

bool is_di_post_link_on(void)
{
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (DI_POST_REG_RD(DI_POST_CTRL) & 0x1000)
		ret = true;
#endif
	return ret;
}

bool is_di_post_mode(struct vframe_s *vf)
{
	bool ret = false;

	if (vf && (vf->type & VIDTYPE_PRE_INTERLACE) &&
	    !(vf->type & VIDTYPE_DI_PW))
		ret = true;
	return ret;
}

bool is_afbc_enabled(u8 layer_id)
{
	struct video_layer_s *layer = NULL;
	u32 value;

	if (layer_id >= MAX_VD_LAYER)
		return -1;

	layer = &vd_layer[layer_id];
	value = READ_VCBUS_REG(layer->vd_afbc_reg.afbc_enable);
	return (value & 0x100) ? true : false;
}

bool is_local_vf(struct vframe_s *vf)
{
	if (vf &&
	    (vf == &vf_local ||
	     vf == &vf_local2 ||
	     vf == &local_pip ||
	     vf == &local_pip2))
		return true;

	/* FIXEME: remove gvideo_recv */
	if (vf && gvideo_recv[0] &&
	    vf == &gvideo_recv[0]->local_buf)
		return true;
	if (vf && gvideo_recv[1] &&
	    vf == &gvideo_recv[1]->local_buf)
		return true;
	if (vf && gvideo_recv[2] &&
	    vf == &gvideo_recv[2]->local_buf)
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

	if (layer_id == 2 && cur_dev->max_vd_layers == 3) {
		if (on)
			enable_video_layer3();
		else
			disable_video_layer3(async);
	}

	if (layer_id == 0xff) {
		if (on) {
			enable_video_layer();
			enable_video_layer2();
			if (cur_dev->max_vd_layers == 3)
				enable_video_layer3();
		} else  if (async) {
			disable_video_layer(async);
			disable_video_layer2(async);
			if (cur_dev->max_vd_layers == 3)
				disable_video_layer3(async);
		} else {
			disable_video_all_layer_nodelay();
		}
	}
}

static void vd1_path_select(struct video_layer_s *layer,
			    bool afbc, bool di_afbc,
			    bool di_post)
{
	u32 misc_off = layer->misc_reg_offt;
	u8 vpp_index;

	vpp_index = layer->vpp_index;
	if (cur_dev->t7_display)
		return;

	if (!legacy_vpp) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VD1_AFBCD0_MISC_CTRL,
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
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VD1_AFBCD0_MISC_CTRL,
				/* Vd1_afbc0_mem_sel */
				(afbc ? 1 : 0),
				22, 1);
		if (di_post) {
			/* check di_vpp_out_en bit */
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VD1_AFBCD0_MISC_CTRL,
				/* vd1 mif to di */
				1,
				8, 2);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VD1_AFBCD0_MISC_CTRL,
				/* go field select di post */
				1,
				20, 2);
		}
	} else {
		if (!di_post)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VIU_MISC_CTRL0 + misc_off,
				0, 16, 3);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VIU_MISC_CTRL0 + misc_off,
			(afbc ? 1 : 0), 20, 1);
	}
}

static void vdx_path_select(struct video_layer_s *layer,
			    bool afbc, bool di_afbc)
{
	u32 misc_off = layer->misc_reg_offt;
	u8 vpp_index;

	vpp_index = layer->vpp_index;
	if (cur_dev->t7_display)
		return;

	if (!legacy_vpp) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VD2_AFBCD1_MISC_CTRL,
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
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VD2_AFBCD1_MISC_CTRL,
				/* Vd2_afbc0_mem_sel */
				(afbc ? 1 : 0),
				22, 1);
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VIU_MISC_CTRL1 + misc_off,
			(afbc ? 2 : 0), 0, 2);
	}
}

static u32 viu_line_stride(u32 buffr_width)
{
	u32 line_stride;

	/* input: buffer width not hsize */
	/* 1 stride = 16 byte */
	line_stride = (buffr_width + 15) / 16;
	return line_stride;
}

static void set_vd_mif_linear_cs(struct video_layer_s *layer,
				   struct canvas_s *cs0,
				   struct canvas_s *cs1,
				   struct canvas_s *cs2,
				   struct vframe_s *vf)
{
	u32 y_line_stride;
	u32 c_line_stride;
	int y_buffer_width, c_buffer_width;
	u64 baddr_y, baddr_cb, baddr_cr;
	struct hw_vd_linear_reg_s *vd_mif_linear_reg = &layer->vd_mif_linear_reg;
	u8 vpp_index;

	vpp_index = layer->vpp_index;
	if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444) ||
		    (vf->type & VIDTYPE_VIU_422)) {
		baddr_y = cs0->addr;
		y_buffer_width = cs0->width;
		y_line_stride = viu_line_stride(y_buffer_width);
		baddr_cb = 0;
		baddr_cr = 0;
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_0,
			y_line_stride | 0 << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_1,
			1 << 16 | 0);
	} else {
		baddr_y = cs0->addr;
		y_buffer_width = cs0->width;
		baddr_cb = cs1->addr;
		c_buffer_width = cs1->width;
		baddr_cr = cs2->addr;

		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_1,
			1 << 16 | c_line_stride);
	}
}

static void set_vd_mif_linear(struct video_layer_s *layer,
				   struct canvas_config_s *config,
				   u32 planes,
				   struct vframe_s *vf)
{
	u32 y_line_stride;
	u32 c_line_stride;
	int y_buffer_width, c_buffer_width;
	u64 baddr_y, baddr_cb, baddr_cr;
	struct hw_vd_linear_reg_s *vd_mif_linear_reg = &layer->vd_mif_linear_reg;
	struct canvas_config_s *cfg = config;
	u8 vpp_index;

	vpp_index = layer->vpp_index;
	switch (planes) {
	case 1:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		y_line_stride = viu_line_stride(y_buffer_width);
		baddr_cb = 0;
		baddr_cr = 0;
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_0,
			y_line_stride | 0 << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_1,
			1 << 16 | 0);
		break;
	case 2:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		cfg++;
		baddr_cb = cfg->phy_addr;
		c_buffer_width = cfg->width;
		baddr_cr = 0;
		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_1,
			1 << 16 | c_line_stride);
		break;
	case 3:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		cfg++;
		baddr_cb = cfg->phy_addr;
		c_buffer_width = cfg->width;
		cfg++;
		baddr_cr = cfg->phy_addr;
		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_linear_reg->vd_if0_stride_1,
			1 << 16 | c_line_stride);
		break;
	default:
		pr_err("error planes=%d\n", planes);
		break;
	}
}

static void vd_set_blk_mode(struct video_layer_s *layer, u8 block_mode)
{
	struct hw_vd_reg_s *vd_mif_reg = &layer->vd_mif_reg;
	u32 pic_32byte_aligned = 0;
	u8 vpp_index;

	vpp_index = layer->vpp_index;
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		block_mode, 12, 2);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		block_mode, 14, 2);
	if (block_mode)
		pic_32byte_aligned = 7;
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		(pic_32byte_aligned << 6) |
		(block_mode << 4) |
		(block_mode << 2) |
		(block_mode << 0),
		18,
		9);
}

static void vd1_set_dcu(struct video_layer_s *layer,
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
	struct hw_vd_reg_s *vd_mif_reg = &layer->vd_mif_reg;
	struct hw_vd_reg_s *vd2_mif_reg = &vd_layer[1].vd_mif_reg;
	struct hw_afbc_reg_s *vd_afbc_reg = &layer->vd_afbc_reg;
	bool di_post = false;
	u8 vpp_index = layer->vpp_index;

	if (!vf) {
		pr_info("%s vf NULL, return\n", __func__);
		return;
	}

	type = vf->type;
	if (type & VIDTYPE_MVC)
		is_mvc = true;

	pr_debug("%s for vd%d %p, type:0x%x\n",
		 __func__, layer->layer_id, vf, type);
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (is_di_post_mode(vf) && is_di_post_on())
		di_post = true;
#endif

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_afbc_reg->afbc_top_ctrl,
				3, 13, 2);
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
		else if (glayer_info[0].mirror == H_MIRROR)
			r |= (1 << 26) | (0 << 27);
		else if (glayer_info[0].mirror == V_MIRROR)
			r |= (0 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_mode, r);

		r = 0x1700;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (vf &&
			    (vf->source_type != VFRAME_SOURCE_TYPE_HDMI &&
			    !(vf->type & VIDTYPE_DI_PW)))
				r |= (1 << 19); /* dos_uncomp */
			if (type & VIDTYPE_COMB_MODE)
				r |= (1 << 20);
		}
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_enable, r);

		r = conv_lbuf_len;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if ((type & VIDTYPE_VIU_444) ||
			    (type & VIDTYPE_RGB_444))
				r |= 0;
			else if (type & VIDTYPE_VIU_422)
				r |= (1 << 12);
			else
				r |= (2 << 12);
		}
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_conv_ctrl, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_dec_def_color,
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
			if ((type & VIDTYPE_VIU_444) ||
			    (type & VIDTYPE_RGB_444)) {
				r &= ~HFORMATTER_EN;
				r &= ~VFORMATTER_EN;
				r &= ~HFORMATTER_YC_RATIO_2_1;
			} else if (type & VIDTYPE_VIU_422) {
				r &= ~VFORMATTER_EN;
			}
		}
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_vd_cfmt_ctrl, r);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_COMPRESS_LOSS)
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_afbc_reg->afbcdec_iquant_enable,
					((1 << 11) |
					(1 << 10) |
					(1 << 4) |
					(1 << 0)));
			else
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_afbc_reg->afbcdec_iquant_enable, 0);
		}
		vd1_path_select(layer, true, false, di_post);
		if (is_mvc)
			vdx_path_select(layer, true, false);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_gen_reg, 0);
		return;
	}
	if (cur_dev->t7_display)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_afbc_reg->afbc_top_ctrl,
			0, 13, 2);

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width
			(vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;
	if (layer->mif_setting.block_mode)
		burst_len = layer->mif_setting.block_mode;
	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !frame_par->nocomp) {
		if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444)) {
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
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			(bit_mode & 0x3), 8, 2);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			(burst_len & 0x3), 1, 2);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				0, 0, 1);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				1, 0, 1);
		vd_set_blk_mode(layer, layer->mif_setting.block_mode);
		if (is_mvc) {
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg3,
				(bit_mode & 0x3), 8, 2);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg3,
				(burst_len & 0x3), 1, 2);
			if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd2_mif_reg->vd_if0_gen_reg3,
					0, 0, 1);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd2_mif_reg->vd_if0_gen_reg3,
					1, 0, 1);
			vd_set_blk_mode(&vd_layer[1], layer->mif_setting.block_mode);
		}
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			(bit_mode & 0x3), 8, 2);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				0, 0, 1);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				1, 0, 1);

		if (is_mvc) {
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg3,
				(bit_mode & 0x3), 8, 2);
			if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd2_mif_reg->vd_if0_gen_reg3,
					0, 0, 1);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd2_mif_reg->vd_if0_gen_reg3,
					1, 0, 1);
		}
	}
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (is_di_post_mode(vf)) {
		DI_POST_WR_REG_BITS
		(DI_IF1_GEN_REG3,
		(bit_mode & 0x3), 8, 2);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
			DI_POST_WR_REG_BITS
				(DI_IF2_GEN_REG3,
				(bit_mode & 0x3), 8, 2);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			DI_POST_WR_REG_BITS
				(DI_IF0_GEN_REG3,
				(bit_mode & 0x3), 8, 2);
	}
#endif
	if (glayer_info[0].need_no_compress ||
	    (vf->type & VIDTYPE_PRE_DI_AFBC)) {
		vd1_path_select(layer, false, true, di_post);
	} else {
		vd1_path_select(layer, false, false, di_post);
		if (is_mvc)
			vdx_path_select(layer, false, false);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_enable, 0);
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
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		r |= (1 << 4);

	/*enable go field reset default according to vlsi*/
	r |= VDIF_RESET_ON_GO_FIELD;
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_gen_reg, r);
	if (is_mvc)
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_gen_reg, r);

	if (type & VIDTYPE_VIU_NV21)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 2, 0, 2);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0, 0, 2);

	/* FIXME: don't use glayer_info[0].reverse */
	if (glayer_info[0].reverse) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0xf, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0xf, 2, 4);
	} else if (glayer_info[0].mirror == H_MIRROR) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0x5, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0x5, 2, 4);
	} else if (glayer_info[0].mirror == V_MIRROR) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0xa, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0xa, 2, 4);
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0, 2, 4);
	}

	/* chroma formatter */
	if ((type & VIDTYPE_VIU_444) ||
	    (type & VIDTYPE_RGB_444)) {
		r = HFORMATTER_YC_RATIO_1_1;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl, r);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->viu_vd_fmt_ctrl, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			    (type & VIDTYPE_VIU_422))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if (frame_par->hscale_skip_count &&
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

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl,
			hformatter | vformatter);

		if (is_mvc) {
			vini_phase = (0xa << VFORMATTER_INIPHASE_BIT);
			vformatter = (vphase | vini_phase
				| vrepeat | VFORMATTER_EN);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->viu_vd_fmt_ctrl,
				hformatter | vformatter);
		}
	}

	if (is_meson_txlx_cpu()	||
	    cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->viu_vd_fmt_ctrl,
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

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_rpt_loop,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));
	if (is_mvc)
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_rpt_loop,
			(loop << VDIF_CHROMA_LOOP1_BIT) |
			(loop << VDIF_LUMA_LOOP1_BIT) |
			(loop << VDIF_CHROMA_LOOP0_BIT) |
			(loop << VDIF_LUMA_LOOP0_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_luma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_chroma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_luma1_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_chroma1_rpt_pat, pat);

	if (is_mvc) {
		if (framepacking_support)
			pat = 0;
		else
			pat = 0x88;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_luma0_rpt_pat, pat);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_chroma0_rpt_pat, pat);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_luma1_rpt_pat, pat);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_chroma1_rpt_pat, pat);
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
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, r);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, r);
	} else if (process_3d_type & MODE_3D_OUT_FA_MASK) {
		/*FA LR/TB output , do nothing*/
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, 0);
		if (is_mvc) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_luma_psel, 0);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_chroma_psel, 0);
		}
	}
}

static void vdx_set_dcu(struct video_layer_s *layer,
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
	struct hw_vd_reg_s *vd_mif_reg = &layer->vd_mif_reg;
	struct hw_afbc_reg_s *vd_afbc_reg = &layer->vd_afbc_reg;
	int layer_id = layer->layer_id;
	u8 vpp_index = layer->vpp_index;

	if (!vf) {
		pr_err("%s vf is NULL\n", __func__);
		return;
	}
	if (!glayer_info[layer_id].layer_support)
		return;

	type = vf->type;
	if (type & VIDTYPE_MVC)
		is_mvc = true;
	pr_debug("%s for vd%d %p, type:0x%x\n",
		 __func__, layer->layer_id, vf, type);

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_afbc_reg->afbc_top_ctrl,
				3, 13, 2);
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
		if (glayer_info[layer_id].reverse)
			r |= (1 << 26) | (1 << 27);
		else if (glayer_info[layer_id].mirror == H_MIRROR)
			r |= (1 << 26) | (0 << 27);
		else if (glayer_info[layer_id].mirror == V_MIRROR)
			r |= (0 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_mode, r);

		r = 0x1700;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (vf &&
			    (vf->source_type != VFRAME_SOURCE_TYPE_HDMI &&
			    !(vf->type & VIDTYPE_DI_PW)))
				r |= (1 << 19); /* dos_uncomp */
		}
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_enable, r);

		r = conv_lbuf_len;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if ((type & VIDTYPE_VIU_444) ||
			    (type & VIDTYPE_RGB_444))
				r |= 0;
			else if (type & VIDTYPE_VIU_422)
				r |= (1 << 12);
			else
				r |= (2 << 12);
		}
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_conv_ctrl, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_dec_def_color,
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
			if ((type & VIDTYPE_VIU_444) ||
			    (type & VIDTYPE_RGB_444)) {
				r &= ~HFORMATTER_EN;
				r &= ~VFORMATTER_EN;
				r &= ~HFORMATTER_YC_RATIO_2_1;
			} else if (type & VIDTYPE_VIU_422) {
				r &= ~VFORMATTER_EN;
			}
		}
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_vd_cfmt_ctrl, r);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (type & VIDTYPE_COMPRESS_LOSS)
				cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbcdec_iquant_enable,
				((1 << 11) |
				(1 << 10) |
				(1 << 4) |
				(1 << 0)));
			else
				cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbcdec_iquant_enable, 0);
		}

		vdx_path_select(layer, true, false);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_gen_reg, 0);
		return;
	}
	if (cur_dev->t7_display)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_afbc_reg->afbc_top_ctrl,
			0, 13, 2);

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width
			(vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;
	if (layer->mif_setting.block_mode)
		burst_len = layer->mif_setting.block_mode;

	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !frame_par->nocomp) {
		if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444)) {
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

	vdx_path_select(layer, false, false);
	if (!legacy_vpp) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			(bit_mode & 0x3), 8, 2);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			(burst_len & 0x3), 1, 2);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				0, 0, 1);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				1, 0, 1);
		vd_set_blk_mode(layer, layer->mif_setting.block_mode);
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			(bit_mode & 0x3), 8, 2);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				0, 0, 1);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_mif_reg->vd_if0_gen_reg3,
				1, 0, 1);
	}
	if (!(cur_dev->rdma_func[vpp_index].rdma_rd(VIU_MISC_CTRL1) & 0x1))
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_enable, 0);

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

	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		r |= (1 << 4);

	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_gen_reg, r);

	if (type & VIDTYPE_VIU_NV21)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			2, 0, 2);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0, 0, 2);

	/* FIXME: don't use glayer_info[1].reverse */
	if (glayer_info[layer_id].reverse)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0xf, 2, 4);
	else if (glayer_info[layer_id].mirror == H_MIRROR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0x5, 2, 4);
	else if (glayer_info[layer_id].mirror == V_MIRROR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0xa, 2, 4);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0, 2, 4);

	/* chroma formatter */
	if ((type & VIDTYPE_VIU_444) ||
	    (type & VIDTYPE_RGB_444)) {
		r = HFORMATTER_YC_RATIO_1_1;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			    (type & VIDTYPE_VIU_422))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if (frame_par->hscale_skip_count &&
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

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl,
			hformatter | vformatter);
	}
	if (is_meson_txlx_cpu()	||
	    cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->viu_vd_fmt_ctrl,
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

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_rpt_loop,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));

	if (is_mvc)
		pat = 0x88;

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma1_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma1_rpt_pat, pat);

	/* picture 0/1 control */
	if ((((type & VIDTYPE_INTERLACE) == 0) &&
	     ((type & VIDTYPE_VIU_FIELD) == 0) &&
	     ((type & VIDTYPE_MVC) == 0)) ||
	    (frame_par->vpp_2pic_mode & 0x3)) {
		/* progressive frame in two pictures */
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, 0);
	}
}

static void vd_afbc_setting_tl1(struct video_layer_s *layer, struct mif_pos_s *setting)
{
	int crop_left, crop_top;
	int vsize_in, hsize_in;
	int mif_blk_bgn_h, mif_blk_end_h;
	int mif_blk_bgn_v, mif_blk_end_v;
	int pix_bgn_h, pix_end_h;
	int pix_bgn_v, pix_end_v;
	struct hw_afbc_reg_s *vd_afbc_reg;
	u8 vpp_index, layer_id;

	if (!setting)
		return;
	layer_id = layer->layer_id;
	vpp_index = layer->vpp_index;

	vd_afbc_reg = setting->p_vd_afbc_reg;
	/* afbc horizontal setting */
	crop_left = setting->start_x_lines;
	hsize_in = round_up
		((setting->src_w - 1) + 1, 32);
	mif_blk_bgn_h = crop_left / 32;
	mif_blk_end_h = (crop_left + setting->end_x_lines -
		setting->start_x_lines) / 32;
	pix_bgn_h = crop_left - mif_blk_bgn_h * 32;
	pix_end_h = pix_bgn_h + setting->end_x_lines -
		setting->start_x_lines;

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_mif_hor_scope,
		(mif_blk_bgn_h << 16) |
		mif_blk_end_h);

	if (((process_3d_type & MODE_3D_FA) ||
	     (process_3d_type & MODE_FORCE_3D_FA_LR)) &&
	    setting->vpp_3d_mode == 1) {
			/* do nothing*/
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_pixel_hor_scope,
			((pix_bgn_h << 16) |
			pix_end_h));
	}

	/* afbc vertical setting */
	crop_top = setting->start_y_lines;
	vsize_in = round_up((setting->src_h - 1) + 1, 4);
	mif_blk_bgn_v = crop_top / 4;
	mif_blk_end_v = (crop_top + setting->end_y_lines -
		setting->start_y_lines) / 4;
	pix_bgn_v = crop_top - mif_blk_bgn_v * 4;
	pix_end_v = pix_bgn_v + setting->end_y_lines -
		setting->start_y_lines;

	if (layer_id != 2 &&
	   ((process_3d_type & MODE_3D_FA) ||
	   (process_3d_type & MODE_FORCE_3D_FA_TB)) &&
	    setting->vpp_3d_mode == 2) {
		int block_h;

		block_h = vsize_in;
		block_h = block_h / 8;
		if (toggle_3d_fa_frame == OUT_FA_B_FRAME) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_mif_ver_scope,
				(block_h << 16) |
				(vsize_in / 4));
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_mif_ver_scope,
				(0 << 16) |
				block_h);
		}
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_mif_ver_scope,
			(mif_blk_bgn_v << 16) |
			mif_blk_end_v);
	}
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_pixel_ver_scope,
		(pix_bgn_v << 16) |
		pix_end_v);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_size_in,
		(hsize_in << 16) |
		(vsize_in & 0xffff));
	return;
}

void vd_mif_setting(struct video_layer_s *layer,
			struct mif_pos_s *setting)
{
	u32 ls = 0, le = 0;
	u32 h_skip, v_skip, hc_skip, vc_skip;
	int l_aligned, r_aligned;
	int t_aligned, b_aligned, ori_t_aligned, ori_b_aligned;
	int content_w, content_l, content_r;
	struct hw_vd_reg_s *vd_mif_reg;
	struct hw_afbc_reg_s *vd_afbc_reg;
	u8 vpp_index, layer_id;

	if (!setting)
		return;
	layer_id = layer->layer_id;
	vpp_index = layer->vpp_index;
	vd_mif_reg = setting->p_vd_mif_reg;
	vd_afbc_reg = setting->p_vd_afbc_reg;
	h_skip = setting->h_skip + 1;
	v_skip = setting->v_skip + 1;
	hc_skip = setting->hc_skip;
	vc_skip = setting->vc_skip;

	/* vd horizontal setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_x0,
		(setting->l_hs_luma << VDIF_PIC_START_BIT) |
		(setting->l_he_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_x0,
		(setting->l_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_he_chrm << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_x1,
		(setting->r_hs_luma  << VDIF_PIC_START_BIT) |
		(setting->r_he_luma  << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_x1,
		(setting->r_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_he_chrm << VDIF_PIC_END_BIT));

	ls = setting->start_x_lines;
	le = setting->end_x_lines;
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->viu_vd_fmt_w,
		(((le - ls + 1) / h_skip)
		<< VD1_FMT_LUMA_WIDTH_BIT) |
		(((le / 2 - ls / 2 + 1) / h_skip)
		<< VD1_FMT_CHROMA_WIDTH_BIT));

	/* vd vertical setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_y0,
		(setting->l_vs_luma << VDIF_PIC_START_BIT) |
		(setting->l_ve_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_y0,
		(setting->l_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_ve_chrm << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_y1,
		(setting->r_vs_luma << VDIF_PIC_START_BIT) |
		(setting->r_ve_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_y1,
		(setting->r_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_ve_chrm << VDIF_PIC_END_BIT));

	if (setting->skip_afbc)
		goto SKIP_VD1_AFBC;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TL1)
		return vd_afbc_setting_tl1(layer, setting);

	/* afbc horizontal setting */
	if (setting->start_x_lines > 0 ||
	    (setting->end_x_lines < (setting->src_w - 1))) {
		/* also 0 line as start */
		l_aligned = round_down(0, 32);
		r_aligned = round_up
			((setting->src_w - 1) + 1, 32);
	} else {
		l_aligned = round_down
			(setting->start_x_lines, 32);
		r_aligned = round_up
			(setting->end_x_lines + 1, 32);
	}

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_vd_cfmt_w,
		(((r_aligned - l_aligned) / h_skip) << 16) |
		((r_aligned / 2 - l_aligned / hc_skip) / h_skip));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_mif_hor_scope,
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
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_pixel_hor_scope,
			(((content_l << 16)) | content_r) / h_skip);
	} else {
		if (((process_3d_type & MODE_3D_FA) ||
		     (process_3d_type & MODE_FORCE_3D_FA_LR)) &&
		    setting->vpp_3d_mode == 1) {
			/* do nothing*/
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_pixel_hor_scope,
				(((setting->start_x_lines - l_aligned) << 16) |
				(setting->end_x_lines - l_aligned)) / h_skip);
		}
	}

	/* afbc vertical setting */
	t_aligned = round_down(setting->start_y_lines, 4);
	b_aligned = round_up(setting->end_y_lines + 1, 4);

	ori_t_aligned = round_down(0, 4);
	ori_b_aligned = round_up((setting->src_h - 1) + 1, 4);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_vd_cfmt_h,
		(b_aligned - t_aligned) / vc_skip / v_skip);

	if (layer_id == 0 &&
	     ((process_3d_type & MODE_3D_FA) ||
	     (process_3d_type & MODE_FORCE_3D_FA_TB)) &&
	    setting->vpp_3d_mode == 2) {
		int block_h;

		block_h = ori_b_aligned - ori_t_aligned;
		block_h = block_h / 8;
		if (toggle_3d_fa_frame == OUT_FA_B_FRAME) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_mif_ver_scope,
				(((ori_t_aligned / 4) +  block_h) << 16) |
				((ori_b_aligned / 4)  - 1));
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_mif_ver_scope,
				((ori_t_aligned / 4) << 16) |
				((ori_t_aligned / 4)  + block_h - 1));
		}
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_mif_ver_scope,
			((t_aligned / 4) << 16) |
			((b_aligned / 4) - 1));
	}

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_mif_ver_scope,
		((t_aligned / 4) << 16) |
		((b_aligned / 4) - 1));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_pixel_ver_scope,
		((setting->start_y_lines - t_aligned) << 16) |
		(setting->end_y_lines - t_aligned));

	/* TODO: if need remove this limit */
	/* afbc pixel vertical output region must be */
	/* [0, end_y_lines - start_y_lines] */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_pixel_ver_scope,
		(setting->end_y_lines -
		setting->start_y_lines));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_size_in,
		((r_aligned - l_aligned) << 16) |
		((ori_b_aligned - ori_t_aligned) & 0xffff));
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_size_out,
			(((r_aligned - l_aligned) / h_skip) << 16) |
			(((b_aligned - t_aligned) / v_skip) & 0xffff));
	}
SKIP_VD1_AFBC:
	return;
}

static void get_pre_hscaler_para(u8 layer_id, int *ds_ratio, int *flt_num)
{
	switch (pre_hscaler_ntap[layer_id]) {
	case PRE_HSCALER_2TAP:
		*ds_ratio = 0;
		*flt_num = 2;
		break;
	case PRE_HSCALER_4TAP:
		*ds_ratio = 1;
		*flt_num = 4;
		break;
	case PRE_HSCALER_6TAP:
		*ds_ratio = 2;
		*flt_num = 6;
		break;
	case PRE_HSCALER_8TAP:
		*ds_ratio = 3;
		*flt_num = 8;
		break;
	}
}

static void get_pre_hscaler_coef(u8 layer_id, int *pre_hscaler_table)
{
	switch (pre_hscaler_ntap[layer_id]) {
	case PRE_HSCALER_2TAP:
		pre_hscaler_table[0] = 0x100;
		pre_hscaler_table[1] = 0x0;
		pre_hscaler_table[2] = 0x0;
		pre_hscaler_table[3] = 0x0;
		break;
	case PRE_HSCALER_4TAP:
		pre_hscaler_table[0] = 0xc0;
		pre_hscaler_table[1] = 0x40;
		pre_hscaler_table[2] = 0x0;
		pre_hscaler_table[3] = 0x0;
		break;
	case PRE_HSCALER_6TAP:
		pre_hscaler_table[0] = 0x9c;
		pre_hscaler_table[1] = 0x44;
		pre_hscaler_table[2] = 0x20;
		pre_hscaler_table[3] = 0x0;
		break;
	case PRE_HSCALER_8TAP:
		pre_hscaler_table[0] = 0x90;
		pre_hscaler_table[1] = 0x40;
		pre_hscaler_table[2] = 0x20;
		pre_hscaler_table[3] = 0x10;
		break;
	}
}

static void vd1_scaler_setting(struct video_layer_s *layer, struct scaler_setting_s *setting)
{
	u32 misc_off, i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;
	u32 hsc_rpt_p0_num0 = 1;
	u32 hsc_init_rev_num0 = 4;
	struct hw_pps_reg_s *vd_pps_reg;
	u32 bit9_mode = 0, s11_mode = 0;
	u8 vpp_index;

	if (!setting || !setting->frame_par)
		return;

	vd_pps_reg = &layer->pps_reg;
	frame_par = setting->frame_par;
	misc_off = setting->misc_reg_offt;
	vpp_index = layer->vpp_index;
	/* vpp super scaler */
	vpp_set_super_scaler_regs
		(frame_par->supscl_path,
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
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_SRSHARP0_CTRL, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_SRSHARP1_CTRL, 0);
	}

	vpp_filter = &frame_par->vpp_filter;

	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		if (setting->sc_h_enable) {
			if (has_pre_hscaler_ntap(0)) {
				/* for sc2/t5 support hscaler 8 tap */
				if (pre_hscaler_ntap_enable[0]) {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
				} else {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT_OLD)
					| VPP_SC_HORZ_EN);
				}
			} else {
				sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
			}
			if (hscaler_8tap_enable[0]) {
				sc_misc_val |=
				((vpp_filter->vpp_horz_coeff[0] & 0xf)
					<< VPP_SC_HBANK_LENGTH_BIT);

			} else {
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 7)
					<< VPP_SC_HBANK_LENGTH_BIT);
			}
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

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_sc_misc,
			sc_misc_val);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_sc_misc,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
	}

	/* horitontal filter settings */
	if (setting->sc_h_enable) {
		bit9_mode = vpp_filter->vpp_horz_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_horz_coeff[1] & 0x4000;
		if (s11_mode && cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (hscaler_8tap_enable[0]) {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA |
						  VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 3]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			}
		} else {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i <
					(vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
			}
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;
		r2 = frame_par->VPP_hsc_linear_endp
			- frame_par->VPP_hsc_startp;
		r3 = frame_par->VPP_hsc_endp
			- frame_par->VPP_hsc_startp;

		if (frame_par->supscl_path ==
		     CORE0_PPS_CORE1 ||
		    frame_par->supscl_path ==
		     CORE1_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE1_CM)
			r3 >>= frame_par->supsc1_hori_ratio;
		if (frame_par->supscl_path ==
		     CORE0_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1)
			r3 >>= frame_par->supsc0_hori_ratio;

		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_hsc_phase_ctrl,
			frame_par->VPP_hf_ini_phase_,
			VPP_HSC_TOP_INI_PHASE_BIT,
			VPP_HSC_TOP_INI_PHASE_WID);
		if (has_pre_hscaler_ntap(0)) {
			int ds_ratio = 1;
			int flt_num = 4;
			int pre_hscaler_table[4] = {
				0x0, 0x0, 0xf8, 0x48};
			get_pre_hscaler_para(0, &ds_ratio, &flt_num);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->VPP_hf_ini_phase_,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			if (hscaler_8tap_enable[0]) {
				hsc_rpt_p0_num0 = 3;
				hsc_init_rev_num0 = 8;
			} else {
				hsc_init_rev_num0 = 4;
				hsc_rpt_p0_num0 = 1;
			}
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			if (has_pre_hscaler_8tap(0)) {
				/* 8 tap */
				get_pre_hscaler_coef(0, pre_hscaler_table);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_8TAP_COEF0_BIT,
					VPP_PREHSC_8TAP_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_8TAP_COEF1_BIT,
					VPP_PREHSC_8TAP_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[2],
					VPP_PREHSC_8TAP_COEF2_BIT,
					VPP_PREHSC_8TAP_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[3],
					VPP_PREHSC_8TAP_COEF3_BIT,
					VPP_PREHSC_8TAP_COEF3_WID);
			} else {
				/* 2,4 tap */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_COEF0_BIT,
					VPP_PREHSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_COEF1_BIT,
					VPP_PREHSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[2],
					VPP_PREHSC_COEF2_BIT,
					VPP_PREHSC_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[3],
					VPP_PREHSC_COEF3_BIT,
					VPP_PREHSC_COEF3_WID);
			}
			if (has_pre_vscaler_ntap(0)) {
				/* T5, T7 */
				if (has_pre_hscaler_8tap(0)) {
					/* T7 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T7,
						VPP_PREHSC_FLT_NUM_WID_T7);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T7,
						VPP_PREHSC_DS_RATIO_WID_T7);
				} else {
					/* T5 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T5,
						VPP_PREHSC_FLT_NUM_WID_T5);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T5,
						VPP_PREHSC_DS_RATIO_WID_T5);
				}
			} else {
				/* SC2 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREHSC_FLT_NUM_BIT,
					VPP_PREHSC_FLT_NUM_WID);
			}
		}
		if (has_pre_vscaler_ntap(0)) {
			int ds_ratio = 1;
			int flt_num = 4;

			if (has_pre_hscaler_8tap(0)) {
				int pre_vscaler_table[2] = {0xc0, 0x40};

				if (!pre_vscaler_ntap_enable[(0)]) {
					pre_vscaler_table[0] = 0x100;
					pre_vscaler_table[1] = 0x0;
					flt_num = 2;
				}
				/* T7 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT_T7,
					VPP_PREVSC_COEF0_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT_T7,
					VPP_PREVSC_COEF1_WID_T7);

				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T7,
					VPP_PREVSC_FLT_NUM_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T7,
					VPP_PREVSC_DS_RATIO_WID_T7);
			} else {
				int pre_vscaler_table[2] = {0xf8, 0x48};

				if (!pre_vscaler_ntap_enable[0]) {
					pre_vscaler_table[0] = 0;
					pre_vscaler_table[1] = 0x40;
					flt_num = 2;
				}
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT,
					VPP_PREVSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT,
					VPP_PREVSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T5,
					VPP_PREVSC_FLT_NUM_WID_T5);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T5,
					VPP_PREVSC_DS_RATIO_WID_T5);
			}
		}

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region12_startp,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION2_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region34_startp,
			((r2 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region4_endp, r3);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_start_phase_step,
			vpp_filter->vpp_hf_start_phase_step);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region1_phase_slope,
			vpp_filter->vpp_hf_start_phase_slope);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region3_phase_slope,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		bit9_mode = vpp_filter->vpp_vert_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_vert_coeff[1] & 0x4000;
		if (s11_mode && cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (bit9_mode || s11_mode) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT |
				VPP_COEF_9BIT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
			for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
			}
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
		}
		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			bit9_mode = pcoeff[1] & 0x8000;
			s11_mode = pcoeff[1] & 0x4000;
			if (s11_mode && cur_dev->t7_display)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x199, 12, 9);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x77, 12, 9);
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
			}
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region12_startp, 0);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region34_startp,
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));

		if (frame_par->supscl_path ==
		     CORE0_PPS_CORE1 ||
		    frame_par->supscl_path ==
		     CORE1_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE1_CM)
			r1 >>= frame_par->supsc1_vert_ratio;
		if (frame_par->supscl_path ==
		     CORE0_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1 ||
		     frame_par->supscl_path ==
		     PPS_POSTBLEND_CORE1 ||
		     frame_par->supscl_path ==
		     PPS_CORE1_CM)
			r1 >>= frame_par->supsc0_vert_ratio;

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region4_endp, r1);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_start_phase_step,
			vpp_filter->vpp_vsc_start_phase_step);
	}

	/*vpp input size setting*/
	cur_dev->rdma_func[vpp_index].rdma_wr
		(VPP_IN_H_V_SIZE + misc_off,
		((frame_par->video_input_w & 0x1fff) << 16)
		| (frame_par->video_input_h & 0x1fff));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(VPP_PIC_IN_HEIGHT + misc_off,
		frame_par->VPP_pic_in_height_);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(VPP_LINE_IN_LENGTH + misc_off,
		frame_par->VPP_line_in_length_);
	if (cur_dev->t7_display)
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VD1_HDR_IN_SIZE + misc_off,
			(frame_par->VPP_pic_in_height_ << 16)
			| frame_par->VPP_line_in_length_);
	/* FIXME: if enable it */
#ifdef CHECK_LATER
	if (layer->enabled == 1) {
		struct vppfilter_mode_s *vpp_filter =
		    &cur_frame_par->vpp_filter;
		u32 h_phase_step, v_phase_step;

		h_phase_step = READ_VCBUS_REG
			(vd_pps_reg->vd_hsc_start_phase_step);
		v_phase_step = READ_VCBUS_REG
			(vd_pps_reg->vd_vsc_start_phase_step);
		if (vpp_filter->vpp_hf_start_phase_step != h_phase_step ||
		    vpp_filter->vpp_vsc_start_phase_step != v_phase_step)
			layer->property_changed = true;
	}
#endif
}

static void vdx_scaler_setting(struct video_layer_s *layer, struct scaler_setting_s *setting)
{
	u32 misc_off, i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;
	u32 hsc_rpt_p0_num0 = 1;
	u32 hsc_init_rev_num0 = 4;
	struct hw_pps_reg_s *vd_pps_reg;
	u32 bit9_mode = 0, s11_mode = 0;
	u8 vpp_index, layer_id;

	if (!setting || !setting->frame_par)
		return;

	layer_id = layer->layer_id;
	frame_par = setting->frame_par;
	misc_off = setting->misc_reg_offt;
	vpp_filter = &frame_par->vpp_filter;
	vd_pps_reg = &layer->pps_reg;
	vpp_index = layer->vpp_index;

	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		if (setting->sc_h_enable) {
			if (has_pre_hscaler_ntap(layer_id)) {
				if (pre_hscaler_ntap_enable[layer_id]) {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
				} else {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT_OLD)
					| VPP_SC_HORZ_EN);
				}
			} else {
				sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
			}
			if (hscaler_8tap_enable[layer_id])
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 0xf)
					<< VPP_SC_HBANK_LENGTH_BIT);
			else
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 7)
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

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_sc_misc,
			sc_misc_val);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_sc_misc,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
	}

	/* horitontal filter settings */
	if (setting->sc_h_enable) {
		bit9_mode = vpp_filter->vpp_horz_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_horz_coeff[1] & 0x4000;
		if (s11_mode && cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (hscaler_8tap_enable[layer_id]) {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA |
						  VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 3]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			}
		} else {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
			}
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;
		r2 = frame_par->VPP_hsc_linear_endp
			- frame_par->VPP_hsc_startp;
		r3 = frame_par->VPP_hsc_endp
			- frame_par->VPP_hsc_startp;

		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_hsc_phase_ctrl,
			frame_par->VPP_hf_ini_phase_,
			VPP_HSC_TOP_INI_PHASE_BIT,
			VPP_HSC_TOP_INI_PHASE_WID);
		if (has_pre_hscaler_ntap(layer_id)) {
			int ds_ratio = 1;
			int flt_num = 4;
			int pre_hscaler_table[4] = {
				0x0, 0x0, 0xf8, 0x48};

			get_pre_hscaler_para(layer_id, &ds_ratio, &flt_num);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->VPP_hf_ini_phase_,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			if (hscaler_8tap_enable[layer_id]) {
				hsc_rpt_p0_num0 = 3;
				hsc_init_rev_num0 = 8;
			} else {
				hsc_init_rev_num0 = 4;
				hsc_rpt_p0_num0 = 1;
			}
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			if (has_pre_hscaler_8tap(layer_id)) {
				/* 8 tap */
				get_pre_hscaler_coef(layer_id, pre_hscaler_table);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_8TAP_COEF0_BIT,
					VPP_PREHSC_8TAP_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_8TAP_COEF1_BIT,
					VPP_PREHSC_8TAP_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[2],
					VPP_PREHSC_8TAP_COEF2_BIT,
					VPP_PREHSC_8TAP_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[3],
					VPP_PREHSC_8TAP_COEF3_BIT,
					VPP_PREHSC_8TAP_COEF3_WID);
			} else {
				/* 2,4 tap */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_COEF0_BIT,
					VPP_PREHSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_COEF1_BIT,
					VPP_PREHSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[2],
					VPP_PREHSC_COEF2_BIT,
					VPP_PREHSC_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[3],
					VPP_PREHSC_COEF3_BIT,
					VPP_PREHSC_COEF3_WID);
			}
			if (has_pre_vscaler_ntap(layer_id)) {
				/* T5, T7 */
				if (has_pre_hscaler_8tap(layer_id)) {
					/* T7 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T7,
						VPP_PREHSC_FLT_NUM_WID_T7);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T7,
						VPP_PREHSC_DS_RATIO_WID_T7);
				} else {
					/* T5 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T5,
						VPP_PREHSC_FLT_NUM_WID_T5);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T5,
						VPP_PREHSC_DS_RATIO_WID_T5);
				}
			} else {
				/* SC2 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREHSC_FLT_NUM_BIT,
					VPP_PREHSC_FLT_NUM_WID);
			}
		}
		if (has_pre_vscaler_ntap(layer_id)) {
			int ds_ratio = 1;
			int flt_num = 4;

			if (has_pre_hscaler_8tap(layer_id)) {
				int pre_vscaler_table[2] = {0xc0, 0x40};

				if (!pre_vscaler_ntap_enable[(layer_id)]) {
					pre_vscaler_table[0] = 0x100;
					pre_vscaler_table[1] = 0x0;
					flt_num = 2;
				}
				/* T7 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT_T7,
					VPP_PREVSC_COEF0_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT_T7,
					VPP_PREVSC_COEF1_WID_T7);

				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T7,
					VPP_PREVSC_FLT_NUM_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T7,
					VPP_PREVSC_DS_RATIO_WID_T7);

			} else {
				int pre_vscaler_table[2] = {0xf8, 0x48};

				if (!pre_vscaler_ntap_enable[(layer_id)]) {
					pre_vscaler_table[0] = 0;
					pre_vscaler_table[1] = 0x40;
					flt_num = 2;
				}
				/* T5 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT,
					VPP_PREVSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT,
					VPP_PREVSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T5,
					VPP_PREVSC_FLT_NUM_WID_T5);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T5,
					VPP_PREVSC_DS_RATIO_WID_T5);
			}
		}
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region12_startp,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION2_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region34_startp,
			((r2 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region4_endp, r3);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_start_phase_step,
			vpp_filter->vpp_hf_start_phase_step);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region1_phase_slope,
			vpp_filter->vpp_hf_start_phase_slope);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region3_phase_slope,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		bit9_mode = vpp_filter->vpp_vert_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_vert_coeff[1] & 0x4000;
		if (s11_mode && cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (bit9_mode || s11_mode) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT |
				VPP_COEF_9BIT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
			for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
			}
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
		}

		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			bit9_mode = pcoeff[1] & 0x8000;
			s11_mode = pcoeff[1] & 0x4000;
			if (s11_mode && cur_dev->t7_display)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x199, 12, 9);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x77, 12, 9);
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
			}
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region12_startp, 0);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region34_startp,
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region4_endp, r1);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_start_phase_step,
			vpp_filter->vpp_vsc_start_phase_step);
	}
	if (layer_id == 1) {
		if (cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VD2_HDR_IN_SIZE + misc_off,
				(frame_par->VPP_pic_in_height_ << 16)
				| frame_par->VPP_line_in_length_);

		else
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_VD2_HDR_IN_SIZE + misc_off,
				(frame_par->VPP_pic_in_height_ << 16)
				| frame_par->VPP_line_in_length_);
	} else if (layer_id == 2) {
		if (cur_dev->t7_display)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VD3_HDR_IN_SIZE + misc_off,
				(frame_par->VPP_pic_in_height_ << 16)
				| frame_par->VPP_line_in_length_);
	}
}

static void proc_vd1_vsc_phase_per_vsync(struct video_layer_s *layer,
					 struct vpp_frame_par_s *frame_par,
					 struct vframe_s *vf)
{
	struct f2v_vphase_s *vphase;
	u32 misc_off, vin_type;
	struct vppfilter_mode_s *vpp_filter;
	struct hw_pps_reg_s *vd_pps_reg;
	u8 vpp_index;

	if (!layer || !frame_par || !vf)
		return;

	vpp_filter = &frame_par->vpp_filter;
	misc_off = layer->misc_reg_offt;
	vin_type = vf->type & VIDTYPE_TYPEMASK;
	vd_pps_reg = &layer->pps_reg;
	vpp_index = layer->vpp_index;

	/* vertical phase */
	vphase = &frame_par->VPP_vf_ini_phase_
		[vpp_phase_table[vin_type]
		[layer->vout_type]];
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_pps_reg->vd_vsc_init_phase,
		((u32)(vphase->phase) << 8));

	if (vphase->repeat_skip >= 0) {
		/* skip lines */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			skip_tab[vphase->repeat_skip],
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID +
			VPP_PHASECTL_INIRPTNUM_WID);
	} else {
		/* repeat first line */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			4,
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			1 - vphase->repeat_skip,
			VPP_PHASECTL_INIRPTNUMT_BIT,
			VPP_PHASECTL_INIRPTNUM_WID);
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_pps_reg->vd_vsc_phase_ctrl,
		(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
		VPP_PHASECTL_DOUBLELINE_BIT,
		VPP_PHASECTL_DOUBLELINE_WID);
}

static void proc_vd2_vsc_phase_per_vsync(struct video_layer_s *layer,
					 struct vpp_frame_par_s *frame_par,
					 struct vframe_s *vf)
{
	struct f2v_vphase_s *vphase;
	u32 misc_off, vin_type;
	struct vppfilter_mode_s *vpp_filter;
	struct hw_pps_reg_s *vd_pps_reg;
	u8 vpp_index;

	if (!layer || !frame_par || !vf)
		return;

	vpp_filter = &frame_par->vpp_filter;
	misc_off = layer->misc_reg_offt;
	vin_type = vf->type & VIDTYPE_TYPEMASK;
	vd_pps_reg = &layer->pps_reg;
	vpp_index = layer->vpp_index;

	/* vertical phase */
	vphase = &frame_par->VPP_vf_ini_phase_
		[vpp_phase_table[vin_type]
		[layer->vout_type]];
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_pps_reg->vd_vsc_init_phase,
		((u32)(vphase->phase) << 8));

	if (vphase->repeat_skip >= 0) {
		/* skip lines */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			skip_tab[vphase->repeat_skip],
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID +
			VPP_PHASECTL_INIRPTNUM_WID);
	} else {
		/* repeat first line */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			4,
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			1 - vphase->repeat_skip,
			VPP_PHASECTL_INIRPTNUMT_BIT,
			VPP_PHASECTL_INIRPTNUM_WID);
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_pps_reg->vd_vsc_phase_ctrl,
		(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
		VPP_PHASECTL_DOUBLELINE_BIT,
		VPP_PHASECTL_DOUBLELINE_WID);
}

static void proc_vd3_vsc_phase_per_vsync(struct video_layer_s *layer,
					 struct vpp_frame_par_s *frame_par,
					 struct vframe_s *vf)
{
	struct f2v_vphase_s *vphase;
	u32 misc_off, vin_type;
	struct vppfilter_mode_s *vpp_filter;
	struct hw_pps_reg_s *vd_pps_reg;
	u8 vpp_index;

	if (!layer || !frame_par || !vf)
		return;

	vpp_filter = &frame_par->vpp_filter;
	misc_off = layer->misc_reg_offt;
	vin_type = vf->type & VIDTYPE_TYPEMASK;
	vd_pps_reg = &layer->pps_reg;
	vpp_index = layer->vpp_index;

	/* vertical phase */
	vphase = &frame_par->VPP_vf_ini_phase_
		[vpp_phase_table[vin_type]
		[layer->vout_type]];
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_pps_reg->vd_vsc_init_phase,
		((u32)(vphase->phase) << 8));

	if (vphase->repeat_skip >= 0) {
		/* skip lines */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			skip_tab[vphase->repeat_skip],
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID +
			VPP_PHASECTL_INIRPTNUM_WID);
	} else {
		/* repeat first line */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			4,
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			1 - vphase->repeat_skip,
			VPP_PHASECTL_INIRPTNUMT_BIT,
			VPP_PHASECTL_INIRPTNUM_WID);
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_pps_reg->vd_vsc_phase_ctrl,
		(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
		VPP_PHASECTL_DOUBLELINE_BIT,
		VPP_PHASECTL_DOUBLELINE_WID);
}

static void disable_vd1_blend(struct video_layer_s *layer)
{
	u32 misc_off;
	u8 vpp_index;

	if (!layer)
		return;
	vpp_index = layer->vpp_index;

	misc_off = layer->misc_reg_offt;
	if (layer->global_debug & DEBUG_FLAG_BLACKOUT)
		pr_info("VIDEO: VD1 AFBC off now. dispbuf:%p, *dispbuf_mapping:%p, local: %p, %p, %p, %p\n",
			layer->dispbuf,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL);
	cur_dev->rdma_func[vpp_index].rdma_wr(layer->vd_afbc_reg.afbc_enable, 0);
	cur_dev->rdma_func[vpp_index].rdma_wr(layer->vd_mif_reg.vd_if0_gen_reg, 0);

	if (is_dolby_vision_enable()) {
		if (is_meson_txlx_cpu() ||
		    is_meson_gxm_cpu())
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VIU_MISC_CTRL1 + misc_off,
				3, 16, 2); /* bypass core1 */
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(DOLBY_PATH_CTRL + misc_off,
				3, 0, 2);
	}

	/*auto disable sr when video off*/
	if (!is_meson_txl_cpu() &&
	    !is_meson_txlx_cpu()) {
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_SRSHARP0_CTRL, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_SRSHARP1_CTRL, 0);
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
	u8 vpp_index;
	if (!layer)
		return;
	vpp_index = layer->vpp_index;

	if (layer->global_debug & DEBUG_FLAG_BLACKOUT)
		pr_info("VIDEO: VD2 AFBC off now. dispbuf:%p, *dispbuf_mapping:%p, local: %p, %p, %p, %p\n",
			layer->dispbuf,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL);
	cur_dev->rdma_func[vpp_index].rdma_wr(layer->vd_afbc_reg.afbc_enable, 0);
	cur_dev->rdma_func[vpp_index].rdma_wr(layer->vd_mif_reg.vd_if0_gen_reg, 0);

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

static void disable_vd3_blend(struct video_layer_s *layer)
{
	u8 vpp_index;
	if (!layer)
		return;
	vpp_index = layer->vpp_index;

	if (layer->global_debug & DEBUG_FLAG_BLACKOUT)
		pr_info("VIDEO: VD3 AFBC off now. dispbuf:%p, *dispbuf_mapping:%p, local: %p, %p, %p, %p, %p\n",
			layer->dispbuf,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip2,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);
	cur_dev->rdma_func[vpp_index].rdma_wr(layer->vd_afbc_reg.afbc_enable, 0);
	cur_dev->rdma_func[vpp_index].rdma_wr(layer->vd_mif_reg.vd_if0_gen_reg, 0);

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
	need_disable_vd3 = false;
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
		    (par->supscl_path == CORE0_PPS_CORE1 ||
		     par->supscl_path == CORE0_BEFORE_PPS))
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

void correct_vd1_mif_size_for_DV(struct vpp_frame_par_s *par,
				 struct vframe_s *el_vf)
{
	u32 aligned_mask = 0xfffffffe;
	u32 old_len;

	if ((is_dolby_vision_on() == true) &&
	    par->VPP_line_in_length_ > 0 &&
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
			par->VPP_line_in_length_ &=
				0xfffffffe;
			par->VPP_hd_end_lines_ &=
				0xfffffffe;
			par->VPP_hd_start_lines_ =
				par->VPP_hd_end_lines_ + 1
				- (par->VPP_line_in_length_ <<
				par->hscale_skip_count);
		} else {
#endif
		{
		par->VPP_line_in_length_ &=
			aligned_mask;
		par->VPP_hd_start_lines_ &=
			aligned_mask;
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
#ifdef CHECK_LATER
		}
#endif
	}
}

void config_dvel_position(struct video_layer_s *layer,
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
	setting->p_vd_mif_reg = &vd_layer[1].vd_mif_reg;
	setting->p_vd_afbc_reg = &vd_layer[1].vd_afbc_reg;
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
	    cur_frame_par->VPP_line_in_length_ > 0 &&
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
		if ((layer->dispbuf->type
		    & VIDTYPE_VIU_444) ||
		    (layer->dispbuf->type
		    & VIDTYPE_RGB_444)) {
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

s32 config_dvel_pps(struct video_layer_s *layer,
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
	setting->misc_reg_offt = vd_layer[1].misc_reg_offt;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B))
		setting->last_line_fix = true;
	else
		setting->last_line_fix = false;

	setting->sc_top_enable = false;

	setting->vinfo_width = info->width;
	setting->vinfo_height = info->height;
	return 0;
}

s32 config_dvel_blend(struct video_layer_s *layer,
		      struct blend_setting_s *setting,
		      struct vframe_s *dvel_vf)
{
	if (!layer || !layer->cur_frame_par || !setting)
		return -1;

	setting->frame_par = layer->cur_frame_par;
	setting->id = 1;
	setting->misc_reg_offt = vd_layer[1].misc_reg_offt;
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
static void get_3d_horz_pos(struct video_layer_s *layer,
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
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
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
#endif
	}
}

static void get_3d_vert_pos(struct video_layer_s *layer,
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
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
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
#endif
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

void config_3d_vd2_position(struct video_layer_s *layer,
			    struct mif_pos_s *setting)
{
	if (!layer || !layer->cur_frame_par || !layer->dispbuf || !setting)
		return;

	memcpy(setting, &layer->mif_setting, sizeof(struct mif_pos_s));
	setting->id = 1;
	setting->p_vd_mif_reg = &vd_layer[1].vd_mif_reg;
	setting->p_vd_afbc_reg = &vd_layer[1].vd_afbc_reg;

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

s32 config_3d_vd2_pps(struct video_layer_s *layer,
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

s32 config_3d_vd2_blend(struct video_layer_s *layer,
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

void switch_3d_view_per_vsync(struct video_layer_s *layer)
{
	struct vpp_frame_par_s *cur_frame_par;
	u32 misc_off;
	u32 start_aligned, end_aligned, block_len;
	u32 FA_enable = process_3d_type & MODE_3D_OUT_FA_MASK;
	u32 src_start_x_lines, src_end_x_lines;
	u32 src_start_y_lines, src_end_y_lines;
	struct hw_vd_reg_s *vd_mif_reg;
	struct hw_vd_reg_s *vd2_mif_reg;
	struct hw_afbc_reg_s *vd_afbc_reg;
	u8 vpp_index = VPP0;

	if (!layer || !layer->cur_frame_par || !layer->dispbuf)
		return;

	vd_mif_reg = &layer->vd_mif_reg;
	vd2_mif_reg = &vd_layer[1].vd_mif_reg;
	vd_afbc_reg = &layer->vd_afbc_reg;
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
	cur_frame_par = layer->cur_frame_par;
	if (FA_enable && toggle_3d_fa_frame == OUT_FA_A_FRAME) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VPP_MISC + misc_off, 1, 14, 1);
			/* VPP_VD1_PREBLEND disable */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(VPP_MISC +
			VPP_MISC + misc_off, 1, 10, 1);
			/* VPP_VD1_POSTBLEND disable */
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel,
			0x4000000);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel,
			0x4000000);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_luma_psel,
			0x4000000);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_chroma_psel,
			0x4000000);
		if (layer->dispbuf->type & VIDTYPE_COMPRESS) {
			if ((process_3d_type & MODE_FORCE_3D_FA_LR) &&
			    cur_frame_par->vpp_3d_mode == 1) {
				start_aligned = src_start_x_lines;
				end_aligned = src_end_x_lines + 1;
				block_len =
					(end_aligned - start_aligned) / 2;
				block_len = block_len /
					(cur_frame_par->hscale_skip_count + 1);
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_afbc_reg->afbc_pixel_hor_scope,
					(start_aligned << 16) |
					(start_aligned + block_len - 1));
			}
			if ((process_3d_type & MODE_FORCE_3D_FA_TB) &&
			    cur_frame_par->vpp_3d_mode == 2) {
				start_aligned =
					round_down(src_start_y_lines, 4);
				end_aligned =
					round_up(src_end_y_lines + 1, 4);
				block_len = end_aligned - start_aligned;
				block_len = block_len / 8;
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_afbc_reg->afbc_mif_ver_scope,
					((start_aligned / 4) << 16) |
					((start_aligned / 4)  + block_len - 1));
			}
		}
	} else if (FA_enable && (toggle_3d_fa_frame == OUT_FA_B_FRAME)) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VPP_MISC + misc_off, 1, 14, 1);
		/* VPP_VD1_PREBLEND disable */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VPP_MISC + misc_off, 1, 10, 1);
		/* VPP_VD1_POSTBLEND disable */
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_chroma_psel, 0);
		if (layer->dispbuf->type & VIDTYPE_COMPRESS) {
			if ((process_3d_type & MODE_FORCE_3D_FA_LR) &&
			    cur_frame_par->vpp_3d_mode == 1) {
				start_aligned = src_end_x_lines;
				end_aligned = src_end_x_lines + 1;
				block_len =
					(end_aligned - start_aligned) / 2;
				block_len = block_len /
					(cur_frame_par->hscale_skip_count + 1);
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_afbc_reg->afbc_pixel_hor_scope,
					((start_aligned + block_len) << 16) |
					(end_aligned - 1));
			}
			if ((process_3d_type & MODE_FORCE_3D_FA_TB) &&
			    cur_frame_par->vpp_3d_mode == 2) {
				start_aligned =
					round_down(src_start_y_lines, 4);
				end_aligned =
					round_up(src_end_y_lines + 1, 4);
				block_len = end_aligned - start_aligned;
				block_len = block_len / 8;
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_afbc_reg->afbc_mif_ver_scope,
				(((start_aligned / 4) + block_len) << 16) |
				((end_aligned / 4)  - 1));
			}
		}
	} else if (FA_enable && (toggle_3d_fa_frame == OUT_FA_BANK_FRAME)) {
		/* output a banking frame */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VPP_MISC + misc_off, 0, 14, 1);
		/* VPP_VD1_PREBLEND disable */
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VPP_MISC + misc_off, 0, 10, 1);
		/* VPP_VD1_POSTBLEND disable */
	}

	if ((process_3d_type & MODE_3D_OUT_TB) ||
	    (process_3d_type & MODE_3D_OUT_LR)) {
		if (cur_frame_par->vpp_2pic_mode & VPP_PIC1_FIRST) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_mif_reg->vd_if0_luma_psel,
				0x4000000);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_mif_reg->vd_if0_chroma_psel,
				0x4000000);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_luma_psel, 0);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_chroma_psel, 0);
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_mif_reg->vd_if0_luma_psel, 0);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_mif_reg->vd_if0_chroma_psel, 0);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_luma_psel,
				0x4000000);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_chroma_psel,
				0x4000000);
		}
	}

	if (force_3d_scaler <= 3 &&
	    cur_frame_par->vpp_3d_scale) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(VPP_VSC_PHASE_CTRL + misc_off,
			force_3d_scaler,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
	}
}
#endif

s32 config_vd_position(struct video_layer_s *layer,
		       struct mif_pos_s *setting)
{
	if (!layer || !layer->cur_frame_par || !layer->dispbuf || !setting)
		return -1;

	setting->id = layer->layer_id;
	setting->p_vd_mif_reg = &layer->vd_mif_reg;
	setting->p_vd_afbc_reg = &layer->vd_afbc_reg;

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
		if ((layer->dispbuf->type
			& VIDTYPE_VIU_444) ||
			(layer->dispbuf->type
			& VIDTYPE_RGB_444)) {
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
	if (setting->id == 0 &&
	    (process_3d_type & MODE_3D_ENABLE)) {
		get_3d_horz_pos
			(layer, layer->dispbuf,
			layer->cur_frame_par->vpp_3d_mode,
			&setting->l_hs_luma, &setting->l_he_luma,
			&setting->r_hs_luma, &setting->r_he_luma);
		get_3d_vert_pos
			(layer, layer->dispbuf,
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
	if (setting->id == 0 &&
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

s32 config_vd_pps(struct video_layer_s *layer,
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

	if (vpp_filter->vpp_hsc_start_phase_step == 0x1000000 &&
	    vpp_filter->vpp_vsc_start_phase_step == 0x1000000 &&
	    vpp_filter->vpp_hsc_start_phase_step ==
	     vpp_filter->vpp_hf_start_phase_step &&
	    !vpp_filter->vpp_pre_vsc_en &&
	    !vpp_filter->vpp_pre_hsc_en &&
	    layer->bypass_pps)
		setting->sc_top_enable = false;

	setting->vinfo_width = info->width;
	setting->vinfo_height = info->height;
	return 0;
}

s32 config_vd_blend(struct video_layer_s *layer,
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
	if (layer->dispbuf &&
	    (layer->dispbuf->type & VIDTYPE_MVC)) {
		setting->preblend_v_start =
			cur_frame_par->VPP_vd_start_lines_;
		setting->preblend_v_end = y_lines;
		setting->preblend_h_start =
			cur_frame_par->VPP_hd_start_lines_;
		setting->preblend_h_end =
			cur_frame_par->VPP_hd_end_lines_;
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
		if (v_phase > 0x1000000 &&
		    glayer_info[setting->id].layer_top < 0) {
			if (cur_frame_par->VPP_vsc_endp > 0x6 &&
			    (cur_frame_par->VPP_vsc_endp < 0x250 ||
			     (cur_frame_par->VPP_vsc_endp <
			      cur_frame_par->VPP_post_blend_vd_v_end_ / 2))) {
				setting->postblend_v_end -= 6;
			}
		}
	}
#endif
	return 0;
}

void vd_blend_setting(struct video_layer_s *layer, struct blend_setting_s *setting)
{
	u32 misc_off;
	u32 vd_size_mask = VPP_VD_SIZE_MASK;
	struct hw_vpp_blend_reg_s *vpp_blend_reg =
		&layer->vpp_blend_reg;
	u8 vpp_index = VPP0;

	if (!setting)
		return;

	/* g12a change to 13 bits */
	if (!legacy_vpp)
		vd_size_mask = 0x1fff;

	misc_off = setting->misc_reg_offt;
	/* preblend setting */
	if (layer->layer_id == 0) {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vpp_blend_reg->preblend_h_start_end,
			((setting->preblend_h_start & vd_size_mask)
			<< VPP_VD1_START_BIT) |
			((setting->preblend_h_end & vd_size_mask)
			<< VPP_VD1_END_BIT));

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vpp_blend_reg->preblend_v_start_end,
			((setting->preblend_v_start & vd_size_mask)
			<< VPP_VD1_START_BIT) |
			((setting->preblend_v_end & vd_size_mask)
			<< VPP_VD1_END_BIT));

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vpp_blend_reg->preblend_h_size,
			setting->preblend_h_size);

		if (!legacy_vpp)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_MISC1 + misc_off,
				setting->layer_alpha);
	}
	/* postblend setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vpp_blend_reg->postblend_h_start_end,
		((setting->postblend_h_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_h_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vpp_blend_reg->postblend_v_start_end,
		((setting->postblend_v_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_v_end & vd_size_mask)
		<< VPP_VD1_END_BIT));
}

void vd_set_dcu(u8 layer_id,
		struct video_layer_s *layer,
		struct vpp_frame_par_s *frame_par,
		struct vframe_s *vf)
{
	if (layer_id == 0)
		vd1_set_dcu(layer, frame_par, vf);
	else
		vdx_set_dcu(layer, frame_par, vf);
}

void vd_scaler_setting(struct video_layer_s *layer,
		       struct scaler_setting_s *setting)
{
	u8 layer_id = 0;

	if (setting && !setting->support)
		return;
	layer_id = layer->layer_id;
	if (layer_id == 0)
		vd1_scaler_setting(layer, setting);
	else
		vdx_scaler_setting(layer, setting);
}

void proc_vd_vsc_phase_per_vsync(struct video_layer_s *layer,
				 struct vpp_frame_par_s *frame_par,
				 struct vframe_s *vf)
{
	u8 layer_id = layer->layer_id;

	if (!glayer_info[layer_id].pps_support)
		return;
	if (layer_id == 0)
		proc_vd1_vsc_phase_per_vsync
			(layer, frame_par, vf);
	else if (layer_id == 1)
		proc_vd2_vsc_phase_per_vsync
			(layer, frame_par, vf);
	else
		proc_vd3_vsc_phase_per_vsync
			(layer, frame_par, vf);
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
	u32 black_val;
	u8 vpp_index = VPP0;

	if (is_meson_tl1_cpu())
		black_val = (0x0 << 20) | (0x0 << 10) | 0; /* RGB */
	else
		black_val =
			(0x0 << 20) | (0x200 << 10) | 0x200; /* YUV */

	if (video_mute_on) {
		if (is_dolby_vision_on()) {
			/* core 3 black */
			if (video_mute_status != VIDEO_MUTE_ON_DV) {
				dolby_vision_set_toggle_flag(1);
				pr_info("DOLBY: %s: VIDEO_MUTE_ON_DV\n",
					__func__);
			}
			video_mute_status = VIDEO_MUTE_ON_DV;
		} else {
			if (video_mute_status != VIDEO_MUTE_ON_VPP) {
				/* vpp black */
				cur_dev->rdma_func[vpp_index].rdma_wr
					(VPP_CLIP_MISC0,
					black_val);
				cur_dev->rdma_func[vpp_index].rdma_wr
					(VPP_CLIP_MISC1,
					black_val);
				WRITE_VCBUS_REG
					(VPP_CLIP_MISC0,
					black_val);
				WRITE_VCBUS_REG
					(VPP_CLIP_MISC1,
					black_val);
				pr_info("DOLBY: %s: VIDEO_MUTE_ON_VPP\n",
					__func__);
			}
			video_mute_status = VIDEO_MUTE_ON_VPP;
		}
	} else {
		if (is_dolby_vision_on()) {
			if (video_mute_status != VIDEO_MUTE_OFF) {
				dolby_vision_set_toggle_flag(2);
				pr_info("DOLBY: %s: VIDEO_MUTE_OFF dv on\n",
					__func__);
			}
			video_mute_status = VIDEO_MUTE_OFF;
		} else {
			if (video_mute_status != VIDEO_MUTE_OFF) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(VPP_CLIP_MISC0,
					(0x3ff << 20) |
					(0x3ff << 10) |
					0x3ff);
				cur_dev->rdma_func[vpp_index].rdma_wr
					(VPP_CLIP_MISC1,
					(0x0 << 20) |
					(0x0 << 10) | 0x0);
				pr_info("DOLBY: %s: VIDEO_MUTE_OFF dv off\n",
					__func__);
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
		if (glayer_info[0].zorder >= reference_zorder &&
		    glayer_info[1].zorder < reference_zorder)
			layer0_sel++;
	} else {
		layer0_sel = 0;
		layer1_sel = 1;
		if (glayer_info[1].zorder >= reference_zorder &&
		    glayer_info[0].zorder < reference_zorder)
			layer1_sel++;
	}
	if (glayer_info[0].zorder >= reference_zorder &&
	    glayer_info[1].zorder >= reference_zorder) {
		if (vd_layer[0].enabled && vd_layer[1].enabled) {
			if (glayer_info[1].zorder < glayer_info[0].zorder) {
				layer0_sel = 1;
				layer1_sel = 0;
			} else {
				layer0_sel = 0;
				layer1_sel = 1;
			}
		} else if (vd_layer[0].enabled) {
			layer0_sel = 2;
			layer1_sel = 0;
		} else if (vd_layer[1].enabled) {
			layer0_sel = 0;
			layer1_sel = 2;
		}
	}

	glayer_info[0].cur_sel_port = layer0_sel;
	glayer_info[1].cur_sel_port = layer1_sel;

	if (glayer_info[0].cur_sel_port != glayer_info[0].last_sel_port ||
	    glayer_info[1].cur_sel_port != glayer_info[1].last_sel_port) {
		force_flush = 1;
		glayer_info[0].last_sel_port = glayer_info[0].cur_sel_port;
		glayer_info[1].last_sel_port = glayer_info[1].cur_sel_port;
	}
	return force_flush;
}

static int vpp_zorder_check_t7(void)
{
	int force_flush = 0;
	u8 layer_sel[3] = {0, 1, 2};
	u8 overlay_cnt = 0;
	u8 max_port_id = 2;

	if (legacy_vpp)
		return 0;

	if (glayer_info[2].zorder < glayer_info[1].zorder) {
		/* layer2 = 1, layer1 = 2, layer0 = 0 */
		layer_sel[2]--;
		layer_sel[1]++;
		max_port_id = 1;
		if (glayer_info[2].zorder < glayer_info[0].zorder) {
			/* layer2 = 0, layer1 = 2, layer0 = 1 */
			layer_sel[0]++;
			layer_sel[2]--;
			if (glayer_info[1].zorder < glayer_info[0].zorder) {
				/* layer2 = 0, layer1 = 1, layer0 = 2 */
				layer_sel[0]++;
				layer_sel[1]--;
				max_port_id = 0;
			}
		}
	} else {
		/* layer2 = 2, layer1 = 1, layer0 = 0 */
		if (glayer_info[1].zorder < glayer_info[0].zorder) {
			/* layer2 = 2, layer1 = 0, layer0 = 1 */
			layer_sel[0]++;
			layer_sel[1]--;
			if (glayer_info[2].zorder < glayer_info[0].zorder) {
				/* layer2 = 1, layer1 = 0, layer0 = 2 */
				layer_sel[0]++;
				layer_sel[2]--;
				max_port_id = 0;
			}
		}
	}

	if (glayer_info[0].zorder >= reference_zorder)
		overlay_cnt++;
	if (glayer_info[1].zorder >= reference_zorder)
		overlay_cnt++;
	if (glayer_info[2].zorder >= reference_zorder)
		overlay_cnt++;

	if (overlay_cnt == 1)
		layer_sel[max_port_id]++;
	/* (overlay_cnt > 1 || overlay_cnt == 0) */

	/* TODO */
#ifdef vd3_zorder
	if (glayer_info[0].zorder >= reference_zorder &&
	    glayer_info[1].zorder >= reference_zorder) {
		if (vd_layer[0].enabled && vd_layer[1].enabled) {
			if (glayer_info[1].zorder < glayer_info[0].zorder) {
				layer0_sel = 1;
				layer1_sel = 0;
			} else {
				layer0_sel = 0;
				layer1_sel = 1;
			}
		} else if (vd_layer[0].enabled) {
			layer0_sel = 2;
			layer1_sel = 0;
		} else if (vd_layer[1].enabled) {
			layer0_sel = 0;
			layer1_sel = 2;
		}
	}
#endif
	glayer_info[0].cur_sel_port = layer_sel[0];
	glayer_info[1].cur_sel_port = layer_sel[1];
	glayer_info[2].cur_sel_port = layer_sel[2];

	if (glayer_info[0].cur_sel_port != glayer_info[0].last_sel_port ||
	    glayer_info[1].cur_sel_port != glayer_info[1].last_sel_port ||
	    glayer_info[2].cur_sel_port != glayer_info[2].last_sel_port) {
		force_flush = 1;
		glayer_info[0].last_sel_port = glayer_info[0].cur_sel_port;
		glayer_info[1].last_sel_port = glayer_info[1].cur_sel_port;
		glayer_info[2].last_sel_port = glayer_info[2].cur_sel_port;
	}
	return force_flush;
}

void vpp_blend_update(const struct vinfo_s *vinfo)
{
	static u32 vd1_enabled;
	static u32 vpp_misc_set_save;
	u32 vpp_misc_save, vpp_misc_set, mode = 0;
	u32 vpp_off = cur_dev->vpp_off;
	int video1_off_req = 0;
	int video2_off_req = 0;
	unsigned long flags;
	struct vpp_frame_par_s *vd1_frame_par =
		vd_layer[0].cur_frame_par;
	bool force_flush = false;
	u8 vpp_index = VPP0;

	if (cur_dev->t7_display) {
		vpp_blend_update_t7(vinfo);
		return;
	}
	check_video_mute();

	if (vd_layer[0].enable_3d_mode == mode_3d_mvc_enable)
		mode |= COMPOSE_MODE_3D;
	else if (is_dolby_vision_on() && last_el_status)
		mode |= COMPOSE_MODE_DV;
	if (bypass_cm)
		mode |= COMPOSE_MODE_BYPASS_CM;

	/* check the vpp post size first */
	if (!legacy_vpp && vinfo) {
		u32 read_value = cur_dev->rdma_func[vpp_index].rdma_rd
			(VPP_POSTBLEND_H_SIZE + vpp_off);
		if (((vinfo->field_height << 16) | vinfo->width)
			!= read_value)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_POSTBLEND_H_SIZE + vpp_off,
				((vinfo->field_height << 16) | vinfo->width));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_OUT_H_V_SIZE + vpp_off,
			vinfo->width << 16 |
			vinfo->field_height);
	} else if (vinfo) {
		if (cur_dev->rdma_func[vpp_index].rdma_rd
			(VPP_POSTBLEND_H_SIZE + vpp_off)
			!= vinfo->width)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_POSTBLEND_H_SIZE + vpp_off,
				vinfo->width);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_OUT_H_V_SIZE + vpp_off,
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

	if (vd_layer[0].global_output == 0 ||
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

	if (vd1_enabled != vd_layer[0].enabled) {
		if (vd_layer[0].enabled) {
			video_prop_status &= ~VIDEO_PROP_CHANGE_DISABLE;
			video_prop_status |= VIDEO_PROP_CHANGE_ENABLE;
		} else {
			video_prop_status &= ~VIDEO_PROP_CHANGE_ENABLE;
			video_prop_status |= VIDEO_PROP_CHANGE_DISABLE;
		}
		if (vd_layer[0].global_debug & DEBUG_FLAG_TRACE_EVENT)
			pr_info("VD1 enable/disable status changed: %s->%s.\n",
				vd1_enabled ? "enable" : "disable",
				vd_layer[0].enabled ? "enable" : "disable");
		vd1_enabled = vd_layer[0].enabled;
	}

	if (!(mode & COMPOSE_MODE_3D) &&
	    (vd_layer[1].global_output == 0 ||
	     black_threshold_check(1))) {
		vd_layer[1].enabled = 0;
		vpp_misc_set &= ~(VPP_VD2_PREBLEND |
			VPP_VD2_POSTBLEND);
	} else {
		vd_layer[1].enabled = vd_layer[1].enabled_status_saved;
	}

	if (!vd_layer[0].enabled && is_local_vf(vd_layer[0].dispbuf)) {
		safe_switch_videolayer(0, false, true);
		if (vd_layer[0].keep_frame_id == 1)
			video_pip_keeper_new_frame_notify();
		else if (vd_layer[0].keep_frame_id == 0)
			video_keeper_new_frame_notify();
	}

	if (!vd_layer[1].enabled && is_local_vf(vd_layer[1].dispbuf)) {
		safe_switch_videolayer(1, false, true);
		if (vd_layer[1].keep_frame_id == 1)
			video_pip_keeper_new_frame_notify();
		else if (vd_layer[1].keep_frame_id == 0)
			video_keeper_new_frame_notify();
	}

	if (!vd_layer[1].enabled &&
	    ((vpp_misc_set & VPP_VD2_POSTBLEND) ||
	     (vpp_misc_set & VPP_VD2_PREBLEND)))
		vpp_misc_set &= ~(VPP_VD2_PREBLEND |
			VPP_VD2_POSTBLEND);

	force_flush |= vpp_zorder_check();

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
		if (vpp_misc_set != vpp_misc_set_save || force_flush) {
			u32 port_val[3] = {0, 0, 0};
			u32 vd1_port, vd2_port, icnt;
			u32 post_blend_reg[3] = {
				VD1_BLEND_SRC_CTRL,
				VD2_BLEND_SRC_CTRL,
				OSD2_BLEND_SRC_CTRL
			};

			/* just reset the select port */
			if (glayer_info[0].cur_sel_port > 2 ||
			    glayer_info[1].cur_sel_port > 2) {
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
				cur_dev->rdma_func[vpp_index].rdma_wr
					(post_blend_reg[icnt]
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
			set_value |= VPP_PREBLEND_EN;
			if (osd_preblend_en) {
				set_value |= VPP_VD1_POSTBLEND;
				set_value |= VPP_VD1_PREBLEND;
			}
			/* t5d bit 9:11 used by wm ctrl, chip after g12 not used bit 9:11, mask it*/
			set_value &= 0xfffff1ff;
			set_value |= (vpp_misc_save & 0xe00);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_MISC + vpp_off,
				set_value);
		}
	} else if (vpp_misc_save != vpp_misc_set) {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_MISC + vpp_off,
			vpp_misc_set);
	}

	if ((vd_layer[1].dispbuf && video2_off_req) ||
	    (!vd_layer[1].dispbuf &&
	     (video1_off_req || video2_off_req)))
		disable_vd2_blend(&vd_layer[1]);

	if (video1_off_req)
		disable_vd1_blend(&vd_layer[0]);
}

void vpp_blend_update_t7(const struct vinfo_s *vinfo)
{
	static u32 t7_vd1_enabled, vpp_misc_set_save;
	u32 vpp_misc_save, vpp_misc_set, mode = 0;
	u32 vpp_off = cur_dev->vpp_off;
	int video1_off_req = 0;
	int video2_off_req = 0;
	int video3_off_req = 0;
	unsigned long flags;
	struct vpp_frame_par_s *vd1_frame_par =
		vd_layer[0].cur_frame_par;
	bool force_flush = false;
	int i;
	u8 vpp_index = VPP0;

	check_video_mute();

	if (vd_layer[0].enable_3d_mode == mode_3d_mvc_enable)
		mode |= COMPOSE_MODE_3D;
	else if (is_dolby_vision_on() && last_el_status)
		mode |= COMPOSE_MODE_DV;
	if (bypass_cm)
		mode |= COMPOSE_MODE_BYPASS_CM;

	/* check the vpp post size first */
	if (!legacy_vpp && vinfo) {
		u32 read_value = cur_dev->rdma_func[vpp_index].rdma_rd
			(VPP_POSTBLEND_H_SIZE + vpp_off);
		if (((vinfo->field_height << 16) | vinfo->width)
			!= read_value)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_POSTBLEND_H_SIZE + vpp_off,
				((vinfo->field_height << 16) | vinfo->width));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_OUT_H_V_SIZE + vpp_off,
			vinfo->width << 16 |
			vinfo->field_height);
	} else if (vinfo) {
		if (cur_dev->rdma_func[vpp_index].rdma_rd
			(VPP_POSTBLEND_H_SIZE + vpp_off)
			!= vinfo->width)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_POSTBLEND_H_SIZE + vpp_off,
				vinfo->width);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_OUT_H_V_SIZE + vpp_off,
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

	for (i = 0; i < MAX_VD_LAYERS; i++) {
		vd_layer[i].pre_blend_en = 0;
		vd_layer[i].post_blend_en = 0;
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
			vd_layer[0].pre_blend_en = 1;
			vd_layer[0].post_blend_en = 1;
			vd_layer[0].onoff_state = VIDEO_ENABLE_STATE_IDLE;
			vd_layer[0].onoff_time = jiffies_to_msecs(jiffies);
			if (vd_layer[0].global_debug & DEBUG_FLAG_BLACKOUT)
				pr_info("VIDEO: VsyncEnableVideoLayer\n");
			vpu_delay_work_flag |=
				VPU_VIDEO_LAYER1_CHANGED;
			force_flush = true;
		} else if (vd_layer[0].onoff_state ==
			VIDEO_ENABLE_STATE_OFF_REQ) {
			vd_layer[0].pre_blend_en = 0;
			vd_layer[0].post_blend_en = 0;
			if (mode & COMPOSE_MODE_3D)
				vd_layer[1].pre_blend_en = 0;
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
		vd_layer[0].pre_blend_en = 1;
		vd_layer[0].post_blend_en = 1;
	}

	if (vd_layer[1].vpp_index == VPP0) {
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
					vd_layer[1].pre_blend_en = 1;
					vd_layer[1].post_blend_en = 0;
					vpp_misc_set &= ~VPP_VD2_POSTBLEND;
				} else if (vd_layer[0].enabled &&
					(mode & COMPOSE_MODE_DV)) {
					/* DV el case */
					vd_layer[1].pre_blend_en = 0;
					vd_layer[1].post_blend_en = 0;
				} else if (vd_layer[1].dispbuf) {
					vd_layer[1].pre_blend_en = 0;
					vd_layer[1].post_blend_en = 1;
				} else {
					vd_layer[1].pre_blend_en = 1;
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
				vd_layer[1].pre_blend_en = 0;
				vd_layer[1].post_blend_en = 0;
				vpp_misc_set &= ~(0x1ff << VPP_VD2_ALPHA_BIT);
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
				vd_layer[1].pre_blend_en = 1;
				vd_layer[1].post_blend_en = 0;
			} else if (vd_layer[0].enabled &&
				(mode & COMPOSE_MODE_DV)) {
				/* DV el case */
				vd_layer[1].pre_blend_en = 0;
				vd_layer[1].post_blend_en = 0;
			} else if (vd_layer[1].dispbuf) {
				vd_layer[1].pre_blend_en = 0;
				vd_layer[1].post_blend_en = 1;
			} else {
				vd_layer[1].pre_blend_en = 1;
			}
			vpp_misc_set |= (vd_layer[1].layer_alpha
				<< VPP_VD2_ALPHA_BIT);
		}
	}
	if (vd_layer[2].vpp_index == VPP0) {
		if (likely(vd_layer[2].onoff_state != VIDEO_ENABLE_STATE_IDLE)) {
			/* state change for video layer3 enable/disable */
			spin_lock_irqsave(&video3_onoff_lock, flags);
			if (vd_layer[2].onoff_state == VIDEO_ENABLE_STATE_ON_REQ) {
				/*
				 * the video layer 3
				 * is enabled one vsync later, assumming
				 * all registers are ready from RDMA.
				 */
				vd_layer[2].onoff_state = VIDEO_ENABLE_STATE_ON_PENDING;
			} else if (vd_layer[2].onoff_state ==
				VIDEO_ENABLE_STATE_ON_PENDING) {
				if (vd_layer[2].dispbuf) {
					vd_layer[2].pre_blend_en = 0;
					vd_layer[2].post_blend_en = 1;
				} else {
					vd_layer[2].pre_blend_en = 1;
				}
				//vpp_misc_set |= (vd_layer[2].layer_alpha);
				vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_IDLE;
				vd_layer[1].onoff_time = jiffies_to_msecs(jiffies);
				vpu_delay_work_flag |=
					VPU_VIDEO_LAYER3_CHANGED;
				if (vd_layer[2].global_debug & DEBUG_FLAG_BLACKOUT)
					pr_info("VIDEO: VsyncEnableVideoLayer3\n");
				force_flush = true;
			} else if (vd_layer[2].onoff_state ==
				VIDEO_ENABLE_STATE_OFF_REQ) {
				vd_layer[2].pre_blend_en = 0;
				vd_layer[2].post_blend_en = 0;
				//vpp_misc_set &= 0x1ff << VPP_VD2_ALPHA_BIT;
				vd_layer[2].onoff_state = VIDEO_ENABLE_STATE_IDLE;
				vd_layer[2].onoff_time = jiffies_to_msecs(jiffies);
				vpu_delay_work_flag |=
					VPU_VIDEO_LAYER3_CHANGED;
				if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT)
					pr_info("VIDEO: VsyncDisableVideoLayer2\n");
				video3_off_req = 1;
				force_flush = true;
			}
			spin_unlock_irqrestore(&video3_onoff_lock, flags);
		} else if (vd_layer[2].enabled) {
			if (vd_layer[2].dispbuf) {
				vd_layer[2].pre_blend_en = 0;
				vd_layer[2].post_blend_en = 1;
			} else {
				vd_layer[2].pre_blend_en = 1;
			}
		}
	}
	if (vd_layer[0].global_output == 0 ||
	    black_threshold_check(0)) {
		vd_layer[0].enabled = 0;
		/* preblend need disable together */
		vd_layer[0].pre_blend_en = 0;
		vd_layer[0].post_blend_en = 0;

	} else {
		vd_layer[0].enabled = vd_layer[0].enabled_status_saved;
	}

	if (t7_vd1_enabled != vd_layer[0].enabled) {
		if (vd_layer[0].enabled) {
			video_prop_status &= ~VIDEO_PROP_CHANGE_DISABLE;
			video_prop_status |= VIDEO_PROP_CHANGE_ENABLE;
		} else {
			video_prop_status &= ~VIDEO_PROP_CHANGE_ENABLE;
			video_prop_status |= VIDEO_PROP_CHANGE_DISABLE;
		}
		if (vd_layer[0].global_debug & DEBUG_FLAG_TRACE_EVENT)
			pr_info("VD1 enable/disable status changed: %s->%s.\n",
				t7_vd1_enabled ? "enable" : "disable",
				vd_layer[0].enabled ? "enable" : "disable");
		t7_vd1_enabled = vd_layer[0].enabled;
	}

	if (!vd_layer[0].enabled) {
		vd_layer[0].pre_blend_en = 0;
		vd_layer[0].post_blend_en = 0;
	}

	if (vd_layer[1].vpp_index == VPP0) {
		if (!(mode & COMPOSE_MODE_3D) &&
		    (vd_layer[1].global_output == 0 ||
		     black_threshold_check(1))) {
			vd_layer[1].enabled = 0;
			vd_layer[1].pre_blend_en = 0;
			vd_layer[1].post_blend_en = 0;
		} else {
			vd_layer[1].enabled = vd_layer[1].enabled_status_saved;
		}

		if (!vd_layer[1].enabled) {
			vd_layer[1].pre_blend_en = 0;
			vd_layer[1].post_blend_en = 0;
		}
	} else {
		vd_layer[1].pre_blend_en = 0;
		vd_layer[1].post_blend_en = 0;
	}
	if (vd_layer[2].vpp_index == VPP0) {
		if ((vd_layer[2].global_output == 0 ||
		     black_threshold_check(2))) {
			vd_layer[2].enabled = 0;
			vd_layer[2].pre_blend_en = 0;
			vd_layer[2].post_blend_en = 0;
		} else {
			vd_layer[2].enabled = vd_layer[2].enabled_status_saved;
		}

		if (!vd_layer[2].enabled) {
			vd_layer[2].pre_blend_en = 0;
			vd_layer[2].post_blend_en = 0;
		}
	} else {
		vd_layer[2].pre_blend_en = 0;
		vd_layer[2].post_blend_en = 0;
	}
	force_flush |= vpp_zorder_check_t7();

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
			VPP_PREBLEND_EN |
			VPP_POSTBLEND_EN |
			0xf);

		/* if vd2 is bottom layer, need remove alpha for vd2 */
		/* 3d case vd2 preblend, need remove alpha for vd2 */
		if ((!vd_layer[0].post_blend_en &&
		    vd_layer[1].post_blend_en) ||
		    vd_layer[1].pre_blend_en) {
			vpp_misc_set &= ~(0x1ff << VPP_VD2_ALPHA_BIT);
			vpp_misc_set |= (vd_layer[1].layer_alpha
				<< VPP_VD2_ALPHA_BIT);
		}

		vpp_misc_save &=
			((1 << 29) | VPP_CM_ENABLE |
			(0x1ff << VPP_VD2_ALPHA_BIT) |
			VPP_PREBLEND_EN |
			VPP_POSTBLEND_EN |
			0xf);
		if (vpp_misc_set != vpp_misc_set_save || force_flush) {
			u32 port_val[4] = {0, 0, 0, 0};
			u32 vd1_port, vd2_port, vd3_port;
			u32 post_blend_reg[4] = {
				VD1_BLEND_SRC_CTRL,
				VD2_BLEND_SRC_CTRL,
				VD3_BLEND_SRC_CTRL,
				OSD2_BLEND_SRC_CTRL
			};

			/* just reset the select port */
			if (glayer_info[0].cur_sel_port > 3 ||
				glayer_info[1].cur_sel_port > 3 ||
			    glayer_info[2].cur_sel_port > 3) {
				glayer_info[0].cur_sel_port = 0;
				glayer_info[1].cur_sel_port = 1;
				glayer_info[2].cur_sel_port = 2;
			}

			vd1_port = glayer_info[0].cur_sel_port;
			vd2_port = glayer_info[1].cur_sel_port;
			vd3_port = glayer_info[2].cur_sel_port;

			/* post bld premult*/
			port_val[0] |= (1 << 16);

			/* vd2 path sel */
			if (vd_layer[1].post_blend_en)
				port_val[1] |= (1 << 20);
			else
				port_val[1] &= ~(1 << 20);

			/* vd3 path sel */
			if (vd_layer[2].post_blend_en)
				port_val[2] |= (1 << 20);
			else
				port_val[2] &= ~(1 << 20);

			/* osd2 path sel */
			port_val[3] |= (1 << 20);

			if (vd_layer[0].post_blend_en) {
				 /* post src */
				port_val[vd1_port] |= (1 << 8);
				port_val[0] |=
					((1 << 4) | /* pre bld premult*/
					(1 << 0)); /* pre bld src 1 */
			} else {
				port_val[0] &= ~0xff;
			}

			if (vd_layer[1].post_blend_en)
				 /* post src */
				port_val[vd2_port] |= (2 << 8);
			else if (vd_layer[1].pre_blend_en)
				port_val[1] |=
					((1 << 4) | /* pre bld premult*/
					(2 << 0)); /* pre bld src 1 */

			if (vd_layer[2].post_blend_en)
				 /* post src */
				port_val[vd3_port] |= (3 << 8);
			else if (vd_layer[2].pre_blend_en)
				port_val[2] |=
					((1 << 4) | /* pre bld premult*/
					(3 << 0)); /* pre bld src 1 */

			for (i = 0; i < 4; i++)
				cur_dev->rdma_func[vpp_index].rdma_wr
					(post_blend_reg[i]
					+ vpp_off,
					port_val[i]);

			set_value = vpp_misc_set;
			set_value &=
				((1 << 29) | VPP_CM_ENABLE |
				(0x1ff << VPP_VD2_ALPHA_BIT) |
				VPP_POSTBLEND_EN |
				VPP_PREBLEND_EN |
				0xf);
			if (vd_layer[0].pre_blend_en ||
			    vd_layer[1].pre_blend_en ||
			    vd_layer[2].pre_blend_en)
				set_value |= VPP_PREBLEND_EN;
			if (vd_layer[0].post_blend_en ||
			    vd_layer[1].post_blend_en ||
			    vd_layer[2].post_blend_en)
				set_value |= VPP_POSTBLEND_EN;
			if (mode & COMPOSE_MODE_BYPASS_CM)
				set_value &= ~VPP_CM_ENABLE;
			if (osd_preblend_en) {
				set_value |= VPP_POSTBLEND_EN;
				set_value |= VPP_PREBLEND_EN;
			}
			/* t5d bit 9:11 used by wm ctrl, chip after g12 not used bit 9:11, mask it*/
			set_value &= 0xfffff1ff;
			set_value |= (vpp_misc_save & 0xe00);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VPP_MISC + vpp_off,
				set_value);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(VPP_MISC2 + vpp_off,
				vd_layer[2].layer_alpha, 0, 9);

		}
	}
	vpp_misc_set_save = vpp_misc_set;

	if (vd_layer[2].vpp_index == VPP0 &&
	    vd_layer[2].dispbuf && video3_off_req)
		disable_vd3_blend(&vd_layer[2]);

	if (vd_layer[1].vpp_index == VPP0 &&
	    ((vd_layer[1].dispbuf && video2_off_req) ||
	    (!vd_layer[1].dispbuf &&
	     (video1_off_req || video2_off_req))))
		disable_vd2_blend(&vd_layer[1]);

	if (video1_off_req)
		disable_vd1_blend(&vd_layer[0]);
}

void vppx_vd_blend_setting(struct video_layer_s *layer, struct blend_setting_s *setting)
{
	u32 vpp_index;
	u32 vd_size_mask = 0x1fff;
	struct hw_vppx_blend_reg_s *vppx_blend_reg;

	if (!setting || layer->layer_id == 0)
		return;
	vpp_index = layer->vpp_index;
	if (vpp_index >= VPP2)
		return;
	vppx_blend_reg = &vppx_blend_reg_array[vpp_index - 1];
	/* vppx blend setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vppx_blend_reg->vpp_bld_din0_hscope,
		((setting->postblend_h_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_h_end & vd_size_mask)
		<< VPP_VD1_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vppx_blend_reg->vpp_bld_din0_vscope,
		((setting->postblend_v_start & vd_size_mask)
		<< VPP_VD1_START_BIT) |
		((setting->postblend_v_end & vd_size_mask)
		<< VPP_VD1_END_BIT));
}

void vppx_blend_update(const struct vinfo_s *vinfo, u32 vpp_index)
{
	unsigned long flags;
	int video1_off_req = 0;
	int video2_off_req = 0;
	int video3_off_req = 0;
	bool force_flush = false;
	u32 vd2_path_start_sel, vd3_path_start_sel;
	u32 vd_path_msic_ctrl;
	u32 vpp1_blend_ctrl = 0, vpp2_blend_ctrl = 0;
	u32 blend_en, vd1_premult = 1, osd1_premult = 0;
	u32 bld_src1_sel, bld_src2_sel;

	if (vinfo) {
		u32 read_value = cur_dev->rdma_func[vpp_index].rdma_rd
			(vppx_blend_reg_array[vpp_index].vpp_bld_out_size);
		if (((vinfo->field_height << 16) | vinfo->width)
			!= read_value)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vppx_blend_reg_array[vpp_index].vpp_bld_out_size,
				((vinfo->field_height << 16) | vinfo->width));
	}
	if (vd_layer[1].vpp_index != VPP0) {
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
				if (vd_layer[1].dispbuf)
					vd_layer[1].vppx_blend_en = 1;
				vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_IDLE;
				vd_layer[1].onoff_time = jiffies_to_msecs(jiffies);
				vpu_delay_work_flag |=
					VPU_VIDEO_LAYER2_CHANGED;
				if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT)
					pr_info("VIDEO: VsyncEnableVideoLayer2\n");
				force_flush = true;
			} else if (vd_layer[1].onoff_state ==
				VIDEO_ENABLE_STATE_OFF_REQ) {
				vd_layer[1].vppx_blend_en = 0;
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
			if (vd_layer[1].dispbuf)
				vd_layer[1].vppx_blend_en = 1;
		}
	}
	if (vd_layer[2].vpp_index != VPP0) {
		if (likely(vd_layer[2].onoff_state != VIDEO_ENABLE_STATE_IDLE)) {
			/* state change for video layer3 enable/disable */
			spin_lock_irqsave(&video3_onoff_lock, flags);
			if (vd_layer[2].onoff_state == VIDEO_ENABLE_STATE_ON_REQ) {
				/*
				 * the video layer 3
				 * is enabled one vsync later, assumming
				 * all registers are ready from RDMA.
				 */
				vd_layer[2].onoff_state = VIDEO_ENABLE_STATE_ON_PENDING;
			} else if (vd_layer[2].onoff_state ==
				VIDEO_ENABLE_STATE_ON_PENDING) {
				if (vd_layer[2].dispbuf)
					vd_layer[2].vppx_blend_en = 1;
				vd_layer[1].onoff_state = VIDEO_ENABLE_STATE_IDLE;
				vd_layer[1].onoff_time = jiffies_to_msecs(jiffies);
				vpu_delay_work_flag |=
					VPU_VIDEO_LAYER3_CHANGED;
				if (vd_layer[2].global_debug & DEBUG_FLAG_BLACKOUT)
					pr_info("VIDEO: VsyncEnableVideoLayer3\n");
				force_flush = true;
			} else if (vd_layer[2].onoff_state ==
				VIDEO_ENABLE_STATE_OFF_REQ) {
				vd_layer[2].vppx_blend_en = 0;
				vd_layer[2].onoff_state = VIDEO_ENABLE_STATE_IDLE;
				vd_layer[2].onoff_time = jiffies_to_msecs(jiffies);
				vpu_delay_work_flag |=
					VPU_VIDEO_LAYER3_CHANGED;
				if (vd_layer[1].global_debug & DEBUG_FLAG_BLACKOUT)
					pr_info("VIDEO: VsyncDisableVideoLayer2\n");
				video3_off_req = 1;
				force_flush = true;
			}
			spin_unlock_irqrestore(&video3_onoff_lock, flags);
		} else if (vd_layer[2].enabled) {
			if (vd_layer[2].dispbuf)
				vd_layer[2].vppx_blend_en = 1;
		}
	}

	if (vd_layer[1].vpp_index != VPP0) {
		if (vd_layer[1].global_output == 0 ||
		     black_threshold_check(1)) {
			vd_layer[1].enabled = 0;
			vd_layer[1].vppx_blend_en = 0;
		} else {
			vd_layer[1].enabled = vd_layer[1].enabled_status_saved;
		}

		if (!vd_layer[1].enabled)
			vd_layer[1].vppx_blend_en = 0;
	}
	if (vd_layer[2].vpp_index != VPP0) {
		if ((vd_layer[2].global_output == 0 ||
		     black_threshold_check(2))) {
			vd_layer[2].enabled = 0;
			vd_layer[2].vppx_blend_en = 0;
		} else {
			vd_layer[2].enabled = vd_layer[2].enabled_status_saved;
		}

		if (!vd_layer[2].enabled)
			vd_layer[2].vppx_blend_en = 0;
	}

	if (vd_layer[1].vpp_index != VPP0 ||
	    vd_layer[2].vpp_index != VPP0) {
		bld_src1_sel = 1;
		bld_src2_sel = 0;
		/* 1:vd1  2:osd1 else :close */
		/* osd1 is top, vd1 is bottom */

		vd_path_msic_ctrl =
			cur_dev->rdma_func[vpp_index].rdma_rd(viu_misc_reg.vd_path_misc_ctrl);
		if (vd_layer[1].vppx_blend_en) {
			vd_path_msic_ctrl &= 0xffffff0f;
			vd_path_msic_ctrl |= 2 << 12;
			blend_en = 1;

		} else {
			blend_en = 0;
		}
		#ifdef blendctrl
		vpp1_blend_ctrl =
			cur_dev->rdma_func[vpp_index].rdma_rd(vppx_blend_reg_array[0].vpp_bld_ctrl);
		vpp2_blend_ctrl =
			cur_dev->rdma_func[vpp_index].rdma_rd(vppx_blend_reg_array[1].vpp_bld_ctrl);
		#endif
		vpp1_blend_ctrl = bld_src1_sel;
		vpp1_blend_ctrl |=
			bld_src2_sel << 4 |
			vd1_premult << 16 |
			osd1_premult << 17 |
			(vd_layer[1].layer_alpha & 0x1ff) << 20 |
			blend_en << 31;
		if (vd_layer[2].vppx_blend_en) {
			vd_path_msic_ctrl &= 0xfffff0ff;
			vd_path_msic_ctrl |= 3 << 16;
			blend_en = 1;
		} else {
			blend_en = 0;
		}
		vpp2_blend_ctrl |= bld_src1_sel;
		vpp2_blend_ctrl |=
			bld_src2_sel << 4 |
			vd1_premult << 16 |
			osd1_premult << 17 |
			(vd_layer[2].layer_alpha & 0x1ff) << 20 |
			blend_en << 31;
		cur_dev->rdma_func[vpp_index].rdma_wr(viu_misc_reg.vd_path_misc_ctrl,
			vd_path_msic_ctrl);
		vd2_path_start_sel = vd_layer[1].vpp_index;
		vd3_path_start_sel = vd_layer[2].vpp_index;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(viu_misc_reg.path_start_sel,
			vd2_path_start_sel, 4, 4);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(viu_misc_reg.path_start_sel,
			vd3_path_start_sel, 8, 4);

		cur_dev->rdma_func[vpp_index].rdma_wr(vppx_blend_reg_array[0].vpp_bld_ctrl,
			vpp1_blend_ctrl);
		cur_dev->rdma_func[vpp_index].rdma_wr(vppx_blend_reg_array[1].vpp_bld_ctrl,
			vpp2_blend_ctrl);
	}
	if (vd_layer[2].vpp_index != VPP0 &&
	    vd_layer[2].dispbuf && video3_off_req)
		disable_vd3_blend(&vd_layer[2]);

	if (vd_layer[1].vpp_index != VPP0 &&
	   ((vd_layer[1].dispbuf && video2_off_req) ||
	    (!vd_layer[1].dispbuf &&
	     (video1_off_req || video2_off_req))))
		disable_vd2_blend(&vd_layer[1]);
}

/*********************************************************
 * frame canvas APIs
 *********************************************************/
static void vframe_canvas_set
	(struct canvas_config_s *config,
	u32 planes,
	u32 *index)
{
	int i;
	u32 *canvas_index = index;

	struct canvas_config_s *cfg = config;

	for (i = 0; i < planes; i++, canvas_index++, cfg++)
		canvas_config_config(*canvas_index, cfg);
}

static bool is_vframe_changed
	(u8 layer_id,
	struct vframe_s *cur_vf,
	struct vframe_s *new_vf)
{
	if (cur_vf && new_vf &&
	    (cur_vf->ratio_control & DISP_RATIO_ADAPTED_PICMODE) &&
	    cur_vf->ratio_control == new_vf->ratio_control &&
	    memcmp(&cur_vf->pic_mode, &new_vf->pic_mode,
		   sizeof(struct vframe_pic_mode_s)))
		return true;
	else if (cur_vf &&
		 memcmp(&cur_vf->pic_mode, &gpic_info[layer_id],
			sizeof(struct vframe_pic_mode_s)))
		return true;

	if (glayer_info[layer_id].reverse != reverse ||
	    glayer_info[layer_id].proc_3d_type != process_3d_type)
		return true;
	if (cur_vf && new_vf &&
	    (cur_vf->bufWidth != new_vf->bufWidth ||
	     cur_vf->width != new_vf->width ||
	     cur_vf->height != new_vf->height ||
	     (cur_vf->sar_width != new_vf->sar_width) ||
	     (cur_vf->sar_height != new_vf->sar_height) ||
	     cur_vf->bitdepth != new_vf->bitdepth ||
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	     cur_vf->trans_fmt != new_vf->trans_fmt ||
#endif
	     cur_vf->ratio_control != new_vf->ratio_control ||
	     ((cur_vf->type_backup & VIDTYPE_INTERLACE) !=
	      (new_vf->type_backup & VIDTYPE_INTERLACE)) ||
	     cur_vf->type != new_vf->type))
		return true;
	return false;
}

int get_layer_display_canvas(u8 layer_id)
{
	int ret = -1;
	struct video_layer_s *layer = NULL;

	if (layer_id >= MAX_VD_LAYER)
		return -1;

	layer = &vd_layer[layer_id];
	ret = READ_VCBUS_REG(layer->vd_mif_reg.vd_if0_canvas0);
	return ret;
}

int set_layer_display_canvas(u8 layer_id,
			     struct vframe_s *vf,
			     struct vpp_frame_par_s *cur_frame_par,
			     struct disp_info_s *disp_info)
{
	u32 *cur_canvas_tbl;
	u8 cur_canvas_id;
	struct video_layer_s *layer = NULL;
	bool is_mvc = false;
	bool update_mif = true;
	u8 vpp_index;

	if (layer_id >= MAX_VD_LAYER)
		return -1;

	if ((vf->type & VIDTYPE_MVC) && layer_id == 0)
		is_mvc = true;

	layer = &vd_layer[layer_id];
	vpp_index = vd_layer[layer_id].vpp_index;

	cur_canvas_id = layer->cur_canvas_id;
	cur_canvas_tbl =
		&layer->canvas_tbl[cur_canvas_id][0];

	/* switch buffer */
	if (!glayer_info[layer_id].need_no_compress &&
	    (vf->type & VIDTYPE_COMPRESS)) {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(layer->vd_afbc_reg.afbc_head_baddr,
			vf->compHeadAddr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(layer->vd_afbc_reg.afbc_body_baddr,
			vf->compBodyAddr >> 4);
	}

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (layer_id == 0 &&
	    is_di_post_mode(vf) &&
	    is_di_post_link_on())
		update_mif = false;
#endif
	if (vf->canvas0Addr == 0)
		update_mif = false;

	if (update_mif) {
		if (vf->canvas0Addr != (u32)-1) {
			struct canvas_s cs0, cs1, cs2;

			canvas_copy(vf->canvas0Addr & 0xff,
				    cur_canvas_tbl[0]);
			canvas_copy((vf->canvas0Addr >> 8) & 0xff,
				    cur_canvas_tbl[1]);
			canvas_copy((vf->canvas0Addr >> 16) & 0xff,
				    cur_canvas_tbl[2]);
			if (cur_dev->mif_linear) {
				canvas_read(cur_canvas_tbl[0], &cs0);
				canvas_read(cur_canvas_tbl[1], &cs1);
				canvas_read(cur_canvas_tbl[2], &cs2);
				set_vd_mif_linear_cs(layer,
					&cs0, &cs1, &cs2,
					vf);
			}
		} else {
			vframe_canvas_set
				(&vf->canvas0_config[0],
				vf->plane_num,
				&cur_canvas_tbl[0]);
			if (cur_dev->mif_linear) {
				set_vd_mif_linear(layer,
					&vf->canvas0_config[0],
					vf->plane_num,
					vf);
				vd_layer[layer_id].mif_setting.block_mode =
					vf->canvas0_config[0].block_mode;
			}
		}
		if (layer_id == 0 &&
		    (is_mvc || process_3d_type)) {
			if (vf->canvas1Addr != (u32)-1) {
				struct canvas_s cs0, cs1, cs2;
				canvas_copy
					(vf->canvas1Addr & 0xff,
					cur_canvas_tbl[3]);
				canvas_copy
					((vf->canvas1Addr >> 8) & 0xff,
					cur_canvas_tbl[4]);
				canvas_copy
					((vf->canvas1Addr >> 16) & 0xff,
					cur_canvas_tbl[5]);
				if (cur_dev->mif_linear) {
					canvas_read(cur_canvas_tbl[3], &cs0);
					canvas_read(cur_canvas_tbl[4], &cs1);
					canvas_read(cur_canvas_tbl[5], &cs2);
					set_vd_mif_linear_cs(layer,
						&cs0, &cs1, &cs2,
						vf);
				}
			} else {
				vframe_canvas_set
					(&vf->canvas1_config[0],
					vf->plane_num,
					&cur_canvas_tbl[3]);
				if (cur_dev->mif_linear) {
					set_vd_mif_linear(layer,
						&vf->canvas1_config[0],
						vf->plane_num,
						vf);
				vd_layer[1].mif_setting.block_mode =
					vf->canvas1_config[0].block_mode;
				}
			}
		}
		cur_dev->rdma_func[vpp_index].rdma_wr
			(layer->vd_mif_reg.vd_if0_canvas0,
			layer->disp_canvas[cur_canvas_id][0]);

		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_layer[1].vd_mif_reg.vd_if0_canvas0,
				layer->disp_canvas[cur_canvas_id][1]);
		if (cur_frame_par &&
		    cur_frame_par->vpp_2pic_mode == 1) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(layer->vd_mif_reg.vd_if0_canvas1,
				layer->disp_canvas[cur_canvas_id][0]);
			if (is_mvc)
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_layer[1].vd_mif_reg.vd_if0_canvas1,
					layer->disp_canvas[cur_canvas_id][0]);
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(layer->vd_mif_reg.vd_if0_canvas1,
				layer->disp_canvas[cur_canvas_id][1]);
			if (is_mvc)
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_layer[1].vd_mif_reg.vd_if0_canvas1,
					layer->disp_canvas[cur_canvas_id][0]);
		}
		if (cur_frame_par && disp_info &&
		    (disp_info->proc_3d_type & MODE_3D_ENABLE) &&
		    (disp_info->proc_3d_type & MODE_3D_TO_2D_R) &&
		    cur_frame_par->vpp_2pic_mode == VPP_SELECT_PIC1) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(layer->vd_mif_reg.vd_if0_canvas0,
				layer->disp_canvas[cur_canvas_id][1]);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(layer->vd_mif_reg.vd_if0_canvas1,
				layer->disp_canvas[cur_canvas_id][1]);
			if (is_mvc) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_layer[1].vd_mif_reg.vd_if0_canvas0,
					layer->disp_canvas[cur_canvas_id][1]);
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_layer[1].vd_mif_reg.vd_if0_canvas1,
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

s32 layer_swap_frame(struct vframe_s *vf, u8 layer_id,
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

	if ((vf->type & VIDTYPE_MVC) && layer_id == 0)
		is_mvc = true;

	if (layer->dispbuf != vf) {
		layer->new_vframe_count++;
		layer->new_frame = true;
		if (!layer->dispbuf ||
		    layer->new_vframe_count == 1 ||
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

	set_layer_display_canvas
		(layer->layer_id,
		vf, layer->cur_frame_par, layer_info);

	frame_changed = is_vframe_changed
		(layer->layer_id,
		layer->dispbuf, vf);

	/* enable new config on the new frames */
	if (first_picture || force_toggle || frame_changed) {
		layer->next_frame_par =
			(&layer->frame_parms[0] == layer->next_frame_par) ?
			&layer->frame_parms[1] : &layer->frame_parms[0];
		/* FIXME: remove the global variables */
		glayer_info[layer->layer_id].reverse = reverse;
		glayer_info[layer->layer_id].mirror = mirror;
		glayer_info[layer->layer_id].proc_3d_type =
			process_3d_type;

		ret = vpp_set_filters
			(&glayer_info[layer->layer_id], vf,
			layer->next_frame_par, vinfo,
			(is_dolby_vision_on() &&
			is_dolby_vision_stb_mode() &&
			for_dolby_vision_certification()),
			1);

		memcpy(&gpic_info[layer->layer_id], &vf->pic_mode,
		       sizeof(struct vframe_pic_mode_s));

		if (ret == vppfilter_success_and_changed ||
		    ret == vppfilter_changed_but_hold)
			layer->property_changed = true;

		if (ret != vppfilter_changed_but_hold)
			layer->new_vpp_setting = true;

		if (layer->layer_id == 0) {
			if ((vf->width > 1920 && vf->height > 1088) ||
			    ((vf->type & VIDTYPE_COMPRESS) &&
			     vf->compWidth > 1920 &&
			     vf->compHeight > 1080)) {
				VPU_VD1_CLK_SWITCH(1);
			} else {
				VPU_VD1_CLK_SWITCH(0);
			}
		} else if (layer_id == 1) {
			if ((vf->width > 1920 && vf->height > 1088) ||
			    ((vf->type & VIDTYPE_COMPRESS) &&
			     vf->compWidth > 1920 &&
			     vf->compHeight > 1080)) {
				VPU_VD2_CLK_SWITCH(1);
			} else {
				VPU_VD2_CLK_SWITCH(0);
			}
		} else if (layer_id == 2) {
			if ((vf->width > 1920 && vf->height > 1088) ||
			    ((vf->type & VIDTYPE_COMPRESS) &&
			     vf->compWidth > 1920 &&
			     vf->compHeight > 1080)) {
				VPU_VD3_CLK_SWITCH(1);
			} else {
				VPU_VD3_CLK_SWITCH(0);
			}
		}
	}

	if (((vf->type & VIDTYPE_NO_VIDEO_ENABLE) == 0) &&
	    (!layer->force_config_cnt || vf != layer->dispbuf)) {
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
			if (layer->layer_id == 1 || is_mvc)
				enable_video_layer2();
			if (layer->layer_id == 2)
				enable_video_layer3();
		}
	}

	if (first_picture)
		layer->new_vpp_setting = true;
	layer->dispbuf = vf;
	return ret;
}

/* pip apha APIs */
static void vd_set_alpha(struct video_layer_s *layer,
			     u32 win_en, struct pip_alpha_scpxn_s *alpha_win)
{
	int i;
	u32 alph_gen_mode = 1;
	/* 0:original, 1:  0.5 alpha 2: 0.25/0.5/0.75 */
	u32 alph_gen_byps = 0;
	u8 vpp_index;
	struct hw_vpp_blend_reg_s *vpp_blend_reg =
		&layer->vpp_blend_reg;

	vpp_index = layer->vpp_index;
	if (!win_en)
		alph_gen_byps = 1;
	cur_dev->rdma_func[vpp_index].rdma_wr(vpp_blend_reg->vd_pip_alph_ctrl,
			  ((0 & 0x1) << 28) |
			  ((win_en & 0xffff) << 12) |
			  ((0 & 0x1ff) << 3) |
			  ((alph_gen_mode & 0x3) << 1) |
			  ((alph_gen_byps & 0x1) << 0));
	for (i = 0; i < MAX_PIP_WINDOW; i++) {
		cur_dev->rdma_func[vpp_index].rdma_wr(vpp_blend_reg->vd_pip_alph_scp_h + i,
				  (alpha_win->scpxn_end_h[i] & 0x1fff) << 16 |
				  (alpha_win->scpxn_bgn_h[i] & 0x1fff));

		cur_dev->rdma_func[vpp_index].rdma_wr(vpp_blend_reg->vd_pip_alph_scp_v + i,
				  (alpha_win->scpxn_end_v[i] & 0x1fff) << 16 |
				  (alpha_win->scpxn_bgn_v[i] & 0x1fff));
	}
}

void set_alpha(struct video_layer_s *layer,
	       u32 win_en,
	       struct pip_alpha_scpxn_s *alpha_win)
{
	u8 layer_id = layer->layer_id;

	if (glayer_info[layer_id].alpha_support)
		vd_set_alpha(layer, win_en, alpha_win);
}

/*********************************************************
 * vout APIs
 *********************************************************/
int detect_vout_type(const struct vinfo_s *vinfo)
{
	int vout_type = VOUT_TYPE_PROG;

	if (vinfo && vinfo->field_height != vinfo->height) {
		if (vinfo->height == 576 || vinfo->height == 480)
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

	if (vpu_delay_work_flag & VPU_VIDEO_LAYER3_CHANGED)
		vpu_delay_work_flag &= ~VPU_VIDEO_LAYER3_CHANGED;

	spin_lock_irqsave(&delay_work_lock, flags);

	if (vpu_delay_work_flag & VPU_DELAYWORK_VPU_VD1_CLK) {
		vpu_delay_work_flag &= ~VPU_DELAYWORK_VPU_VD1_CLK;

		spin_unlock_irqrestore(&delay_work_lock, flags);
		if (vpu_vd1_clk_level > 0)
			vpu_dev_clk_request(vd1_vpu_dev, 360000000);
		else
			vpu_dev_clk_release(vd1_vpu_dev);

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

	if (vpu_delay_work_flag & VPU_DELAYWORK_VPU_VD3_CLK) {
		vpu_delay_work_flag &= ~VPU_DELAYWORK_VPU_VD3_CLK;

		/* FIXME: check the right vd3 clock */
#ifdef CHECK_LATER
		spin_unlock_irqrestore(&delay_work_lock, flags);
		if (vpu_vd3_clk_level > 0)
			request_vpu_clk_vmod(360000000, VPU_VIU_VD3);
		else
			release_vpu_clk_vmod(VPU_VIU_VD3);
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
				vpu_dev_mem_power_down(vd1_vpu_dev);
				vpu_dev_mem_power_down(afbc_vpu_dev);
				vpu_dev_mem_power_down(di_post_vpu_dev);
				if (!legacy_vpp)
					vpu_dev_mem_power_down
					(vd1_scale_vpu_dev);
				if (glayer_info[0].fgrain_support)
					vpu_dev_mem_power_down
						(vpu_fgrain0);
			}

			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_VD2) &&
			    ((r & VPP_VD2_PREBLEND) == 0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_VD2;
				vpu_dev_mem_power_down(vd2_vpu_dev);
				vpu_dev_mem_power_down(afbc2_vpu_dev);
				if (!legacy_vpp)
					vpu_dev_mem_power_down
					(vd2_scale_vpu_dev);
				if (glayer_info[1].fgrain_support)
					vpu_dev_mem_power_down
						(vpu_fgrain1);
			}
			#ifdef vpu_clk
			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_VD3)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_VD3;
				vpu_dev_mem_power_down(vd3_vpu_dev);
				vpu_dev_mem_power_down(afbc3_vpu_dev);
				if (!legacy_vpp)
					vpu_dev_mem_power_down
					(vd3_scale_vpu_dev);
				if (glayer_info[2].fgrain_support)
					vpu_dev_mem_power_down
						(vpu_fgrain2);
			}
			#endif
		}
	}

	if (dv_mem_power_off_count > 0) {
		dv_mem_power_off_count--;
		if (dv_mem_power_off_count == 0) {
			if ((vpu_delay_work_flag &
			     VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0)) {
				vpu_delay_work_flag &=
				    ~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY0;
				vpu_dev_mem_power_down(vpu_dolby0);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A)) {
				vpu_delay_work_flag &=
					 ~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1A;
				vpu_dev_mem_power_down(vpu_dolby1a);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B)) {
				vpu_delay_work_flag &=
					~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY1B;
				vpu_dev_mem_power_down(vpu_dolby1b);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2)) {
				vpu_delay_work_flag &=
				~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY2;
				vpu_dev_mem_power_down(vpu_dolby2);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3)) {
				vpu_delay_work_flag &=
				~VPU_DELAYWORK_MEM_POWER_OFF_DOLBY_CORE3;
				vpu_dev_mem_power_down(vpu_dolby_core3);
			}
			if ((vpu_delay_work_flag &
				VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY)) {
				vpu_delay_work_flag &=
				~VPU_DELAYWORK_MEM_POWER_OFF_PRIME_DOLBY;
				vpu_dev_mem_power_down(vpu_prime_dolby_ram);
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

int vpp_crc_check(u32 vpp_crc_en, u8 vpp_index)
{
	u32 val = 0;
	int vpp_crc_result = 0;
	static u32 val_pre, crc_cnt;

	if (vpp_crc_en && cpu_after_eq(MESON_CPU_MAJOR_ID_SM1)) {
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_CRC_CHK, 1);
		if (crc_cnt >= 1) {
			val = cur_dev->rdma_func[vpp_index].rdma_rd(VPP_RO_CRCSUM);
			if (val_pre && val != val_pre)
				vpp_crc_result = -1;
			else
				vpp_crc_result = val;
		}
		val_pre = val;
		crc_cnt++;
	} else {
		crc_cnt  = 0;
	}
	return vpp_crc_result;
}

int vpp_crc_viu2_check(u32 vpp_crc_en)
{
	int vpp_crc_result = 0;

	if (vpp_crc_en)
		vpp_crc_result = READ_VCBUS_REG(VPP2_RO_CRCSUM);

	return vpp_crc_result;
}

void enable_vpp_crc_viu2(u32 vpp_crc_en)
{
	if (vpp_crc_en)
		WRITE_VCBUS_REG_BITS(VPP2_CRC_CHK, 1, 0, 1);
}

int get_osd_reverse(void)
{
	u8 vpp_index = VPP0;

	return (cur_dev->rdma_func[vpp_index].rdma_rd
		(VIU_OSD1_BLK0_CFG_W0) >> 28) & 3;
}
EXPORT_SYMBOL(get_osd_reverse);

void dump_pps_coefs_info(u8 layer_id, u8 bit9_mode, u8 coef_type)
{
	u32 pps_coef_idx_save;
	int i;
	struct hw_pps_reg_s *vd_pps_reg;

	/* bit9_mode : 0 8bit, 1: 9bit*/
	/* coef_type : 0 horz, 1: vert*/
	vd_pps_reg = &vd_layer[layer_id].pps_reg;
	pps_coef_idx_save = READ_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx);
	if (hscaler_8tap_enable[layer_id]) {
		if (bit9_mode) {
			if (coef_type == 0) {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004100 |
					VPP_COEF_9BIT);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 8TAP 9bit HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 8TAP 9bit HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));

				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004180 |
					VPP_COEF_9BIT);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 8TAP 9bit HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 8TAP 9bit HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			} else {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004000 |
					VPP_COEF_9BIT);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 9bit VERT coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 9bit VERT coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			}
		} else {
			if (coef_type == 0) {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004100);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 8TAP HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004180);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 8 TAP HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			} else {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004000);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 VERT coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			}
		}
	} else {
		if (bit9_mode) {
			if (coef_type == 0) {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004100 |
					VPP_COEF_9BIT);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 9bit HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 9bit HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			} else {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004000 |
					VPP_COEF_9BIT);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 9bit VERT coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 9bit VERT coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			}
		} else {
			if (coef_type == 0) {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004100);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 HORZ coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			} else {
				WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
					0x00004000);
				READ_VCBUS_REG(vd_pps_reg->vd_scale_coef);
				for (i = 0;
					i < VPP_FILER_COEFS_NUM; i++)
					pr_info("VD1 VERT coef[%d]=%x\n",
					i,
					READ_VCBUS_REG(vd_pps_reg->vd_scale_coef));
			}
		}
	}
	WRITE_VCBUS_REG(vd_pps_reg->vd_scale_coef_idx,
			pps_coef_idx_save);
}

/*********************************************************
 * Film Grain APIs
 *********************************************************/
#define FGRAIN_TBL_SIZE  (498 * 16)

#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
static void fgrain_set_config(struct video_layer_s *layer,
				  struct fgrain_setting_s *setting, u8 vpp_index)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 1 << 1;
	u32 reg_block_mode = 1 << 2;
	u32 reg_rev_mode = 0 << 4;
	u32 reg_comp_bits = 0 << 6;
	/* unsigned , RW, default = 0:8bits; 1:10bits, else 12 bits */
	u32 reg_fmt_mode = 2 << 8;
	/* unsigned , RW, default =  0:444; 1:422; 2:420; 3:reserved */
	u32 reg_last_in_mode = 0;
	/* for none-afbc, it need set to 1,  default it is 0 */
	u32 reg_fgrain_ext_imode = 1;
	/*  unsigned , RW, default = 0 to indicate the
	 *input data is *4 in 8bit mode
	 */
	u32 layer_id = 0;
	struct hw_fg_reg_s *fg_reg;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (!setting)
		return;
	layer_id = setting->id;
	fg_reg = &layer->fg_reg;

	reg_block_mode = setting->afbc << 2;
	reg_rev_mode = setting->reverse << 4;
	reg_comp_bits = setting->bitdepth << 6;
	reg_fmt_mode = setting->fmt_mode << 8;
	reg_last_in_mode = setting->last_in_mode;

	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_fgrain_glb_en |
			       reg_fgrain_loc_en |
			       reg_block_mode |
			       reg_rev_mode |
			       reg_comp_bits |
			       reg_fmt_mode,
			       0, 10);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_last_in_mode, 14, 1);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_fgrain_ext_imode, 16, 1);
}

static void fgrain_start(struct video_layer_s *layer, u8 vpp_index)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 1 << 1;
	struct hw_fg_reg_s *fg_reg;
	u8 layer_id = layer->layer_id;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (glayer_info[layer_id].fgrain_start)
		return;
	fg_reg = &layer->fg_reg;

	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_fgrain_glb_en |
			       reg_fgrain_loc_en,
			       0, 2);

	glayer_info[layer_id].fgrain_start = true;
	glayer_info[layer_id].fgrain_force_update = true;
}

static void fgrain_stop(struct video_layer_s *layer, u8 vpp_index)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 0 << 1;
	struct hw_fg_reg_s *fg_reg;
	u8 layer_id = layer->layer_id;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (!glayer_info[layer_id].fgrain_start)
		return;
	fg_reg = &layer->fg_reg;

	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_fgrain_glb_en |
			       reg_fgrain_loc_en,
			       0, 2);
	glayer_info[layer_id].fgrain_start = false;
}

static void fgrain_set_window(struct video_layer_s *layer,
			      struct fgrain_setting_s *setting,
			      u8 vpp_index)
{
	struct hw_fg_reg_s *fg_reg;

	fg_reg = &layer->fg_reg;
	cur_dev->rdma_func[vpp_index].rdma_wr(fg_reg->fgrain_win_h,
			  (setting->start_x << 0) |
			  (setting->end_x << 16));
	cur_dev->rdma_func[vpp_index].rdma_wr(fg_reg->fgrain_win_v,
			  (setting->start_y << 0) |
			  (setting->end_y << 16));
}

static int fgrain_init(u8 layer_id, u32 table_size)
{
	int ret;
	u32 channel = FILM_GRAIN0_CHAN;
	struct lut_dma_set_t lut_dma_set;

	if (!glayer_info[layer_id].fgrain_support)
		return -1;
	if (layer_id == 0)
		channel = FILM_GRAIN0_CHAN;
	else if (layer_id == 1)
		channel = FILM_GRAIN1_CHAN;
	else if (layer_id == 2)
		channel = FILM_GRAIN2_CHAN;
	lut_dma_set.channel = channel;
	lut_dma_set.dma_dir = LUT_DMA_WR;
	lut_dma_set.irq_source = ENCP_GO_FEILD;
	lut_dma_set.mode = LUT_DMA_MANUAL;
	lut_dma_set.table_size = table_size;
	ret = lut_dma_register(&lut_dma_set);
	if (ret >= 0) {
		glayer_info[layer_id].lut_dma_support = 1;

	} else {
		pr_info("%s failed, fg not support\n", __func__);
		glayer_info[layer_id].lut_dma_support = 0;
	}
	return ret;
}

static void fgrain_uninit(u8 layer_id)
{
	u32 channel = FILM_GRAIN0_CHAN;

	if (!glayer_info[layer_id].fgrain_support)
		return;

	if (layer_id == 0)
		channel = FILM_GRAIN0_CHAN;
	else if (layer_id == 1)
		channel = FILM_GRAIN1_CHAN;
	else if (layer_id == 2)
		channel = FILM_GRAIN2_CHAN;

	lut_dma_unregister(LUT_DMA_WR, channel);
}

static int fgrain_write(u32 layer_id, ulong fgs_table_addr)
{
	int table_size = FGRAIN_TBL_SIZE;
	u32 channel = 0;

	if (layer_id == 0)
		channel = FILM_GRAIN0_CHAN;
	else if (layer_id == 1)
		channel = FILM_GRAIN1_CHAN;
	else if (layer_id == 2)
		channel = FILM_GRAIN2_CHAN;
	lut_dma_write_phy_addr(channel,
			       fgs_table_addr,
			       table_size);
	return 0;
}

static int get_viu_irq_source(u8 vpp_index)
{
	u32 venc_mux = 3, viu = 0;
	u32 venc_addr = VPU_VENC_CTRL;
	u32 irq_source = ENCP_GO_FEILD;

	venc_mux = READ_VCBUS_REG(VPU_VIU_VENC_MUX_CTRL) & 0x3f;
	venc_mux >>= (vpp_index * 2);
	venc_mux &= 0x3;

	switch (venc_mux) {
	case 0:
		venc_addr = VPU_VENC_CTRL;
		break;
	case 1:
		venc_addr = VPU1_VENC_CTRL;
		break;
	case 2:
		venc_addr = VPU2_VENC_CTRL;
		break;
	}
	viu = READ_VCBUS_REG(venc_addr) & 0x3;
	switch (viu) {
	case 0:
		irq_source = ENCI_GO_FEILD;
		break;
	case 1:
		irq_source = ENCP_GO_FEILD;
		break;
	case 2:
		irq_source = ENCL_GO_FEILD;
		break;
	}
	return irq_source;
}

static void fgrain_update_irq_source(u8 layer_id, u8 vpp_index)
{
	u32 irq_source = ENCP_GO_FEILD;
	u32 viu, channel = 0;

	if (cur_dev->t7_display) {
		/* get vpp0 irq source */
		irq_source = get_viu_irq_source(vpp_index);
	} else {
		viu = READ_VCBUS_REG(VPU_VIU_VENC_MUX_CTRL) & 0x3;

		switch (viu) {
		case 0:
			irq_source = ENCL_GO_FEILD;
			break;
		case 1:
			irq_source = ENCI_GO_FEILD;
			break;
		case 2:
			irq_source = ENCP_GO_FEILD;
			break;
		}
	}
	if (layer_id == 0)
		channel = FILM_GRAIN0_CHAN;
	else if (layer_id == 1)
		channel = FILM_GRAIN1_CHAN;
	else if (layer_id == 2)
		channel = FILM_GRAIN2_CHAN;
	lut_dma_update_irq_source(channel, irq_source);
}

void fgrain_config(struct video_layer_s *layer,
		   struct vpp_frame_par_s *frame_par,
		   struct mif_pos_s *mif_setting,
		   struct fgrain_setting_s *setting,
		   struct vframe_s *vf)
{
	u32 type;
	u8 layer_id;

	if (!vf || !mif_setting || !setting || !frame_par)
		return;
	layer_id = layer->layer_id;
	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (!glayer_info[layer_id].lut_dma_support)
		return;
	setting->id = layer_id;
	type = vf->type;
	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		/* 1:afbc mode or 0: non-afbc mode  */
		setting->afbc = 1;
		/* bit[2]=0, non-afbc mode */
		setting->last_in_mode = 0;
		/* afbc copress is always 420 */
		setting->fmt_mode = 2;
		setting->used = 1;
		if (vf->bitdepth & BITDEPTH_Y10)
			setting->bitdepth = 1;
		else
			setting->bitdepth = 0;
	} else {
		setting->afbc = 0;
		setting->last_in_mode = 1;
		if (type & VIDTYPE_VIU_NV21) {
			setting->fmt_mode = 2;
			setting->used = 1;
		} else {
			/* only support 420 */
			setting->used = 0;
		}
		/* fg after mif always 10 bits */
		setting->bitdepth = 1;
	}

	if (glayer_info[layer_id].reverse)
		setting->reverse = 3;
	else
		setting->reverse = 0;

	setting->start_x = mif_setting->start_x_lines;
	setting->end_x = mif_setting->end_x_lines;
	setting->start_y = mif_setting->start_y_lines;
	setting->end_y = mif_setting->end_y_lines;
	if (setting->afbc) {
		setting->start_x = setting->start_x / 32 * 32;
		setting->end_x = setting->end_x / 32 * 32;
		setting->start_y = setting->start_y / 4 * 4;
		setting->end_y = setting->end_y / 4 * 4;
	} else {
		setting->end_x = (setting->end_x >> 1) << 1;
		setting->end_y = (setting->end_y >> 1) << 1;

		setting->start_x = (setting->start_x >> 1) << 1;
		setting->start_y = (setting->start_y >> 1) << 1;
	}
}

void fgrain_setting(struct video_layer_s *layer,
		    struct fgrain_setting_s *setting,
		    struct vframe_s *vf)
{
	u8 vpp_index, layer_id;

	if (!vf || !setting)
		return;
	layer_id = layer->layer_id;
	if (!glayer_info[layer_id].lut_dma_support)
		return;
	vpp_index = layer->vpp_index;
	if (!setting->used || !vf->fgs_valid ||
	    !glayer_info[layer_id].fgrain_enable)
		fgrain_stop(layer, vpp_index);
	if (glayer_info[layer_id].fgrain_enable) {
		if (setting->used && vf->fgs_valid &&
		    vf->fgs_table_adr) {
			fgrain_set_config(layer, setting, vpp_index);
			fgrain_set_window(layer, setting, vpp_index);
		}
	}
}

void fgrain_update_table(struct video_layer_s *layer,
			 struct vframe_s *vf)
{
	u8 vpp_index, layer_id;

	if (!vf)
		return;
	layer_id = layer->layer_id;
	if (!glayer_info[layer_id].lut_dma_support)
		return;
	vpp_index = layer->vpp_index;
	if (!vf->fgs_valid || !glayer_info[layer_id].fgrain_enable)
		fgrain_stop(layer, vpp_index);

	if (glayer_info[layer_id].fgrain_enable) {
		if (vf->fgs_valid && vf->fgs_table_adr) {
			fgrain_start(layer, vpp_index);
			fgrain_update_irq_source(layer_id, vpp_index);
			fgrain_write(layer_id, vf->fgs_table_adr);
		}
	}
}

#else
static int fgrain_init(u8 layer_id, u32 table_size)
{
	pr_info("warning: film grain not support!\n");
	return 0;
}

static void fgrain_uninit(u8 layer_id)
{
}

void fgrain_config(struct video_layer_s *layer,
		   struct vpp_frame_par_s *frame_par,
		   struct mif_pos_s *mif_setting,
		   struct fgrain_setting_s *setting,
		   struct vframe_s *vf)
{
}

void fgrain_setting(struct video_layer_s *layer,
		    struct fgrain_setting_s *setting,
		    struct vframe_s *vf)
{
}

void fgrain_update_table(struct video_layer_s *layer,
			 struct vframe_s *vf)

{
}
#endif

/*********************************************************
 * Init APIs
 *********************************************************/
static void init_layer_canvas(struct video_layer_s *layer,
			      u32 start_canvas)
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

static noinline int __invoke_psci_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

void vpp_probe_en_set(u32 enable)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (enable)
			__invoke_psci_fn_smc(0x82000080, 1, 0, 0);
		else
			__invoke_psci_fn_smc(0x82000080, 0, 0, 0);
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
void vpp_secure_cb(u32 arg)
{
	pr_debug("%s: arg=%x, reg:%x\n",
		 __func__,
		 arg,
		 READ_VCBUS_REG(VIU_DATA_SEC));
}
#endif

void video_secure_set(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	int i;
	u32 secure_src = 0;
	u32 secure_enable = 0;
	struct video_layer_s *layer = NULL;

	for (i = 0; i < MAX_VD_LAYERS; i++) {
		layer = &vd_layer[i];
		if (layer->dispbuf &&
		    (layer->dispbuf->flag & VFRAME_FLAG_VIDEO_SECURE))
			secure_enable = 1;
		else
			secure_enable = 0;
		if (layer->dispbuf)
			cur_vf_flag = layer->dispbuf->flag;
		if (layer->dispbuf &&
		    secure_enable) {
			if (layer->layer_id == 0)
				secure_src |= VD1_INPUT_SECURE;
			else if (layer->layer_id == 1)
				secure_src |= VD2_INPUT_SECURE;
		}
	}
	secure_config(VIDEO_MODULE, secure_src);
#endif
}

int video_hw_init(void)
{
	u32 cur_hold_line, ofifo_size;
	struct vpu_dev_s *arb_vpu_dev;
	int i;

	if (!legacy_vpp) {
		if (vpp_ofifo_size == 0xff)
			ofifo_size = 0x1000;
		else
			ofifo_size = vpp_ofifo_size;
		WRITE_VCBUS_REG_BITS
			(VPP_OFIFO_SIZE, ofifo_size,
			VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
		WRITE_VCBUS_REG_BITS
			(VPP_MATRIX_CTRL, 0, 10, 5);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		WRITE_VCBUS_REG_BITS
			(VPP_OFIFO_SIZE, 0xfff,
			VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);

	WRITE_VCBUS_REG(VPP_PREBLEND_VD1_H_START_END, 4096);
	WRITE_VCBUS_REG(VPP_BLEND_VD2_H_START_END, 4096);

	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		/* fifo max size on txl :128*3=384[0x180]  */
		WRITE_VCBUS_REG
			(vd_layer[0].vd_mif_reg.vd_if0_luma_fifo_size,
			0x180);
		WRITE_VCBUS_REG
			(vd_layer[1].vd_mif_reg.vd_if0_luma_fifo_size,
			0x180);
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
		if (cur_dev->t7_display) {
			WRITE_VCBUS_REG(T7_VD2_PPS_DUMMY_DATA, 0x4080200);
			WRITE_VCBUS_REG(VD3_PPS_DUMMY_DATA, 0x4080200);
		}
	}
	/* temp: enable VPU arb mem */
	vd1_vpu_dev = vpu_dev_register(VPU_VIU_VD1, "VD1");
	afbc_vpu_dev = vpu_dev_register(VPU_AFBC_DEC, "VD1_AFBC");
	di_post_vpu_dev = vpu_dev_register(VPU_DI_POST, "DI_POST");
	vd1_scale_vpu_dev = vpu_dev_register(VPU_VD1_SCALE, "VD1_SCALE");
	if (glayer_info[0].fgrain_support)
		vpu_fgrain0 = vpu_dev_register(VPU_FGRAIN0, "VPU_FGRAIN0");
	vd2_vpu_dev = vpu_dev_register(VPU_VIU_VD2, "VD2");
	afbc2_vpu_dev = vpu_dev_register(VPU_AFBC_DEC1, "VD2_AFBC");
	vd2_scale_vpu_dev = vpu_dev_register(VPU_VD2_SCALE, "VD2_SCALE");
	if (glayer_info[1].fgrain_support)
		vpu_fgrain1 = vpu_dev_register(VPU_FGRAIN1, "VPU_FGRAIN1");
#ifdef DEPEND_VPU
	vd3_vpu_dev = vpu_dev_register(VPU_VIU_VD3, "VD3");
	afbc3_vpu_dev = vpu_dev_register(VPU_AFBC_DEC2, "VD3_AFBC");
	vd3_scale_vpu_dev = vpu_dev_register(VPU_VD3_SCALE, "VD3_SCALE");
	if (glayer_info[2].fgrain_support)
		vpu_fgrain2 = vpu_dev_register(VPU_FGRAIN2, "VPU_FGRAIN2");

#endif
	vpu_dolby0 = vpu_dev_register(VPU_DOLBY0, "DOLBY0");
	vpu_dolby1a = vpu_dev_register(VPU_DOLBY1A, "DOLBY1A");
	vpu_dolby1b = vpu_dev_register(VPU_DOLBY1B, "DOLBY1B");
	vpu_dolby2 = vpu_dev_register(VPU_DOLBY2, "DOLBY2");
	vpu_dolby_core3 = vpu_dev_register(VPU_DOLBY_CORE3, "DOLBY_CORE3");
	vpu_prime_dolby_ram = vpu_dev_register(VPU_PRIME_DOLBY_RAM, "PRIME_DOLBY_RAM");

	arb_vpu_dev = vpu_dev_register(VPU_VPU_ARB, "ARB");
	vpu_dev_mem_power_on(arb_vpu_dev);

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
		if (!cur_dev->t7_display)
			WRITE_DMCREG
				(DMC_AM0_CHAN_CTRL,
				0x8ff403cf);
		/* for vd1 & vd2 dummy alpha*/
		WRITE_VCBUS_REG
			(VPP_POST_BLEND_DUMMY_ALPHA,
			0x7fffffff);
		WRITE_VCBUS_REG_BITS
			(VPP_MISC1, 0x100, 0, 9);
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		/* disable latch for sr core0/1 scaler */
		WRITE_VCBUS_REG_BITS
			(sr_info.sr0_sharp_sync_ctrl,
			1, 0, 1);
		WRITE_VCBUS_REG_BITS
			(sr_info.sr0_sharp_sync_ctrl,
			1, 8, 1);
		WRITE_VCBUS_REG_BITS
			(sr_info.sr1_sharp_sync_ctrl,
			1, 8, 1);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		WRITE_VCBUS_REG_BITS
			(sr_info.sr0_sharp_sync_ctrl,
			1, 0, 1);
		WRITE_VCBUS_REG_BITS
			(sr_info.sr0_sharp_sync_ctrl,
			1, 8, 1);
	}
	/* force bypass dolby for TL1/T5, no dolby function */
	if (!glayer_info[0].dv_support)
		WRITE_VCBUS_REG_BITS(DOLBY_PATH_CTRL, 0xf, 0, 6);
	for (i = 0; i < MAX_VD_LAYER; i++) {
		if (glayer_info[i].fgrain_support)
			fgrain_init(i, FGRAIN_TBL_SIZE);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(VIDEO_MODULE, 0,
		VSYNC_WR_MPEG_REG, vpp_secure_cb);
#endif
	return 0;
}

static int get_sr_core_support(struct amvideo_device_data_s *p_amvideo)
{
	u32 core_support = 0;

	if (p_amvideo->sr0_support == 0xff &&
	    p_amvideo->sr1_support == 0xff) {
		/* chip before tm2 revb, used old logic */
		if (is_meson_tl1_cpu() ||
		    is_meson_tm2_cpu())
			core_support = NEW_CORE0_CORE1;
		else if (is_meson_txhd_cpu() ||
			 is_meson_g12a_cpu() ||
			 is_meson_g12b_cpu() ||
			 is_meson_sm1_cpu())
			core_support = ONLY_CORE0;
		else if (is_meson_gxlx_cpu())
			core_support = ONLY_CORE1;
		else if (is_meson_txlx_cpu() ||
			 is_meson_txl_cpu() ||
			 is_meson_gxtvbb_cpu())
			core_support = OLD_CORE0_CORE1;
		return core_support;
	} else if ((p_amvideo->sr0_support == 1) &&
		(p_amvideo->sr1_support == 1)) {
		core_support = NEW_CORE0_CORE1;
	} else if (p_amvideo->sr0_support == 1) {
		core_support = ONLY_CORE0;
	} else if (p_amvideo->sr1_support == 1) {
		core_support = ONLY_CORE1;
	}
	return core_support;
}

static void vpp_sr_init(struct amvideo_device_data_s *p_amvideo)
{
	struct sr_info_s *sr;

	sr = &sr_info;
	/* sr_info */
	if (is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu() ||
	    is_meson_sm1_cpu()) {
		sr->sr_reg_offt = 0xc00;
		sr->sr_reg_offt2 = 0x00;
	} else if (is_meson_tl1_cpu() ||
		   is_meson_tm2_cpu()) {
		sr->sr_reg_offt = 0xc00;
		sr->sr_reg_offt2 = 0xc80;
	} else {
		sr->sr_reg_offt = 0;
		sr->sr_reg_offt2 = 0x00;
	}

	if (p_amvideo->sr_reg_offt != 0xff) {
		sr->sr_reg_offt = p_amvideo->sr_reg_offt;
		sr->sr0_sharp_sync_ctrl =
			TM2REVB_SRSHARP0_SHARP_SYNC_CTRL;
	} else {
		sr->sr0_sharp_sync_ctrl =
			SRSHARP0_SHARP_SYNC_CTRL;
	}
	if (p_amvideo->sr_reg_offt2 != 0xff) {
		sr->sr_reg_offt2 = p_amvideo->sr_reg_offt2;
		sr->sr1_sharp_sync_ctrl =
			TM2REVB_SRSHARP1_SHARP_SYNC_CTRL;
	} else {
		sr->sr1_sharp_sync_ctrl =
			SRSHARP1_SHARP_SYNC_CTRL;
	}

	if (p_amvideo->sr0_support == 0xff &&
	    p_amvideo->sr1_support == 0xff) {
		if (is_meson_gxlx_cpu()) {
			sr->sr_support &= ~SUPER_CORE0_SUPPORT;
			sr->sr_support |= SUPER_CORE1_SUPPORT;
			sr->core1_v_disable_width_max = 4096;
			sr->core1_v_enable_width_max = 2048;
		} else if (is_meson_txhd_cpu()) {
			/* 2k pannal */
			sr->sr_support |= SUPER_CORE0_SUPPORT;
			sr->sr_support &= ~SUPER_CORE1_SUPPORT;
			sr->core0_v_disable_width_max = 2048;
			sr->core0_v_enable_width_max = 1024;
		} else if (is_meson_g12a_cpu() ||
			is_meson_g12b_cpu() ||
			is_meson_sm1_cpu()) {
			sr->sr_support |= SUPER_CORE0_SUPPORT;
			sr->sr_support &= ~SUPER_CORE1_SUPPORT;
			sr->core0_v_disable_width_max = 4096;
			sr->core0_v_enable_width_max = 2048;
		} else if (is_meson_gxtvbb_cpu() ||
			   is_meson_txl_cpu() ||
			   is_meson_txlx_cpu() ||
			   is_meson_tl1_cpu() ||
			   is_meson_tm2_cpu()) {
			sr->sr_support |= SUPER_CORE0_SUPPORT;
			sr->sr_support |= SUPER_CORE1_SUPPORT;
			sr->core0_v_disable_width_max = 2048;
			sr->core0_v_enable_width_max = 1024;
			sr->core1_v_disable_width_max = 4096;
			sr->core1_v_enable_width_max = 2048;
		} else {
			sr->sr_support &= ~SUPER_CORE0_SUPPORT;
			sr->sr_support &= ~SUPER_CORE1_SUPPORT;
		}
	} else {
		if (p_amvideo->sr0_support == 1) {
			sr->sr_support |= SUPER_CORE0_SUPPORT;
			sr->core0_v_disable_width_max =
				p_amvideo->core_v_disable_width_max[0];
			sr->core0_v_enable_width_max =
				p_amvideo->core_v_enable_width_max[0];
		}
		if (p_amvideo->sr1_support == 1) {
			sr->sr_support |= SUPER_CORE1_SUPPORT;
			sr->core1_v_disable_width_max =
				p_amvideo->core_v_disable_width_max[1];
			sr->core1_v_enable_width_max =
				p_amvideo->core_v_enable_width_max[1];
		}
	}
	sr->supscl_path = p_amvideo->supscl_path;
	sr->core_support = get_sr_core_support(p_amvideo);
}

int video_early_init(struct amvideo_device_data_s *p_amvideo)
{
	int r = 0, i;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		legacy_vpp = false;

	/* check super scaler support status */
	vpp_sr_init(p_amvideo);
	/* adaptive config bypass ratio */
	vpp_bypass_ratio_config();

	memset(vd_layer, 0, sizeof(vd_layer));
	memset(vd_layer_vpp, 0, sizeof(vd_layer_vpp));
	for (i = 0; i < MAX_VD_LAYER; i++) {
		vd_layer[i].layer_id = i;
		vd_layer[i].cur_canvas_id = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		vd_layer[i].next_canvas_id = 1;
#endif
		/* vd_layer[i].global_output = 1; */
		vd_layer[i].keep_frame_id = 0xff;
		vd_layer[i].disable_video = VIDEO_DISABLE_FORNEXT;
		vd_layer[i].vpp_index = VPP0;
		vpp_disp_info_init(&glayer_info[i], i);
		memset(&gpic_info[i], 0, sizeof(struct vframe_pic_mode_s));
		glayer_info[i].wide_mode = 1;
		glayer_info[i].zorder = reference_zorder - MAX_VD_LAYER + i;
		glayer_info[i].cur_sel_port = i;
		glayer_info[i].last_sel_port = i;
		glayer_info[i].display_path_id = VFM_PATH_DEF;
		glayer_info[i].need_no_compress = false;
		glayer_info[i].afbc_support = 0;
		if (p_amvideo->layer_support[i] == 0) {
			glayer_info[i].afbc_support = false;
			glayer_info[i].pps_support = false;
			glayer_info[i].dv_support = false;
			glayer_info[i].fgrain_support = false;
			glayer_info[i].fgrain_enable = false;
			glayer_info[i].alpha_support = false;
			hscaler_8tap_enable[i] = false;
			pre_hscaler_ntap_enable[i] = false;
			pre_vscaler_ntap_enable[i] = false;
			pre_hscaler_ntap_set[i] = 0xff;
			pre_vscaler_ntap_set[i] = 0xff;
			continue;
		}
		if (legacy_vpp) {
			glayer_info[i].afbc_support = true;
			glayer_info[i].pps_support =
				(i == 0) ? true : false;
		} else if (is_meson_tl1_cpu()) {
			glayer_info[i].afbc_support =
				(i == 0) ? true : false;
			glayer_info[i].pps_support =
				(i == 0) ? true : false;
			/* force bypass dolby for TL1/T5, no dolby function */
			p_amvideo->dv_support = 0;
		} else if (is_meson_tm2_cpu()) {
			glayer_info[i].afbc_support =
				(i == 0) ? true : false;
			glayer_info[i].pps_support = true;
		} else if (is_meson_g12a_cpu() ||
			is_meson_g12b_cpu() ||
			is_meson_sm1_cpu()) {
			glayer_info[i].afbc_support = true;
			glayer_info[i].pps_support = true;
		} else {
			/* after tm2 revb */
			glayer_info[i].afbc_support =
				p_amvideo->afbc_support[i];
			glayer_info[i].pps_support =
				p_amvideo->pps_support[i];
		}
		if (p_amvideo->dv_support)
			glayer_info[i].dv_support = true;
		else
			glayer_info[i].dv_support = false;
		if (p_amvideo->fgrain_support[i]) {
			glayer_info[i].fgrain_support = true;
			glayer_info[i].fgrain_enable = true;
		} else {
			glayer_info[i].fgrain_support = false;
			glayer_info[i].fgrain_enable = false;
		}
		vd_layer[i].layer_support = p_amvideo->layer_support[i];
		glayer_info[i].layer_support = p_amvideo->layer_support[i];
		glayer_info[i].alpha_support = p_amvideo->alpha_support[i];
		hscaler_8tap_enable[i] = has_hscaler_8tap(i);
		pre_hscaler_ntap_enable[i] = has_pre_hscaler_ntap(i);
		pre_vscaler_ntap_enable[i] = has_pre_vscaler_ntap(i);
		pre_hscaler_ntap_set[i] = 0xff;
		pre_vscaler_ntap_set[i] = 0xff;
		pre_hscaler_ntap[i] = PRE_HSCALER_4TAP;
		if (p_amvideo->src_width_max[i] != 0xff)
			glayer_info[i].src_width_max =
				p_amvideo->src_width_max[i];
		else
			glayer_info[i].src_width_max = 4096;
		if (p_amvideo->src_height_max[i] != 0xff)
			glayer_info[i].src_height_max =
				p_amvideo->src_height_max[i];
		else
			glayer_info[i].src_height_max = 2160;
	}
	/* only enable vd1 as default */
	vd_layer[0].global_output = 1;
	vd_layer[0].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[1].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[2].misc_reg_offt = 0 + cur_dev->vpp_off;
	cur_dev->mif_linear = p_amvideo->mif_linear;
	cur_dev->t7_display = p_amvideo->t7_display;
	cur_dev->max_vd_layers = p_amvideo->max_vd_layers;
	if (video_is_meson_t7_cpu()) {
		for (i = 0; i < cur_dev->max_vd_layers; i++) {
			memcpy(&vd_layer[i].vd_afbc_reg,
			       &vd_afbc_reg_t7_array[i],
			       sizeof(struct hw_afbc_reg_s));
			memcpy(&vd_layer[i].vd_mif_reg,
			       &vd_mif_reg_t7_array[i],
			       sizeof(struct hw_vd_reg_s));
			memcpy(&vd_layer[i].vd_mif_linear_reg,
			      &vd_mif_linear_reg_t7_array[i],
			      sizeof(struct hw_vd_linear_reg_s));
			memcpy(&vd_layer[i].fg_reg,
			       &fg_reg_t7_array[i],
			       sizeof(struct hw_fg_reg_s));
			memcpy(&vd_layer[i].pps_reg,
			       &pps_reg_t7_array[i],
			       sizeof(struct hw_pps_reg_s));
			memcpy(&vd_layer[i].vpp_blend_reg,
			       &vpp_blend_reg_t7_array[i],
			       sizeof(struct hw_vpp_blend_reg_s));
		}
	} else if (video_is_meson_sc2_cpu() ||
			video_is_meson_s4_cpu()) {
		for (i = 0; i < cur_dev->max_vd_layers; i++) {
			memcpy(&vd_layer[i].vd_afbc_reg,
			       &vd_afbc_reg_sc2_array[i],
			       sizeof(struct hw_afbc_reg_s));
			memcpy(&vd_layer[i].vd_mif_reg,
			       &vd_mif_reg_sc2_array[i],
			       sizeof(struct hw_vd_reg_s));
			memcpy(&vd_layer[i].fg_reg,
			       &fg_reg_sc2_array[i],
			       sizeof(struct hw_fg_reg_s));
			if (video_is_meson_s4_cpu())
				memcpy(&vd_layer[i].pps_reg,
				       &pps_reg_array_t5d[i],
				       sizeof(struct hw_pps_reg_s));
			else
				memcpy(&vd_layer[i].pps_reg,
				       &pps_reg_array[i],
				       sizeof(struct hw_pps_reg_s));
			memcpy(&vd_layer[i].vpp_blend_reg,
			       &vpp_blend_reg_array[i],
			       sizeof(struct hw_vpp_blend_reg_s));
		}
	} else {
		for (i = 0; i < cur_dev->max_vd_layers; i++) {
			memcpy(&vd_layer[i].vd_afbc_reg,
			       &vd_afbc_reg_array[i],
			       sizeof(struct hw_afbc_reg_s));
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
				memcpy(&vd_layer[i].vd_mif_reg,
				       &vd_mif_reg_g12_array[i],
				       sizeof(struct hw_vd_reg_s));
				if (is_meson_tm2_revb()) {
					memcpy(&vd_layer[i].fg_reg,
					       &fg_reg_g12_array[i],
					       sizeof(struct hw_fg_reg_s));
				}
			} else {
				memcpy(&vd_layer[i].vd_mif_reg,
				       &vd_mif_reg_legacy_array[i],
				       sizeof(struct hw_vd_reg_s));
			}
			if (video_is_meson_t5d_cpu())
				memcpy(&vd_layer[i].pps_reg,
				       &pps_reg_array_t5d[i],
				       sizeof(struct hw_pps_reg_s));
			else
				memcpy(&vd_layer[i].pps_reg,
				       &pps_reg_array[i],
				       sizeof(struct hw_pps_reg_s));
			memcpy(&vd_layer[i].vpp_blend_reg,
			       &vpp_blend_reg_array[i],
			       sizeof(struct hw_vpp_blend_reg_s));
		}
	}
	vd_layer[0].layer_alpha = 0x100;

	/* g12a has no alpha overflow check in hardware */
	vd_layer[1].layer_alpha = legacy_vpp ? 0x1ff : 0x100;
	vd_layer[2].layer_alpha = legacy_vpp ? 0x1ff : 0x100;
	vpp_ofifo_size = p_amvideo->ofifo_size;
	conv_lbuf_len = p_amvideo->afbc_conv_lbuf_len;

	INIT_WORK(&vpu_delay_work, do_vpu_delay_work);

	init_layer_canvas(&vd_layer[0], LAYER1_CANVAS_BASE_INDEX);
	init_layer_canvas(&vd_layer[1], LAYER2_CANVAS_BASE_INDEX);
	init_layer_canvas(&vd_layer[2], LAYER3_CANVAS_BASE_INDEX);
	/* vd_layer_vpp is for multiple vpp */
	memcpy(&vd_layer_vpp[0], &vd_layer[1], sizeof(struct video_layer_s));
	memcpy(&vd_layer_vpp[1], &vd_layer[2], sizeof(struct video_layer_s));
	return r;
}

int video_late_uninit(void)
{
	int i;

	for (i = 0; i < MAX_VD_LAYER; i++)
		fgrain_uninit(i);
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

MODULE_PARM_DESC(cur_vf_flag, "cur_vf_flag");
module_param(cur_vf_flag, uint, 0444);

