// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_drv.c
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

/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/compat.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <asm/div64.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/cma.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/mm_types.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/sched/clock.h>
/*#include <linux/amlogic/iomap.h>*/
/*#include <linux/amlogic/media/registers/register_map.h>*/
#include <linux/fdtable.h>
#include <linux/extcon.h>
/* Amlogic Headers */
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
#include <linux/amlogic/media/vrr/vrr.h>
/* Local Headers */
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_sm.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_v4l2_dbg.h"
#include "vdin_dv.h"
#include "vdin_v4l2_if.h"
#include "vdin_mem_scatter.h"

#include <linux/amlogic/gki_module.h>

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif

#define VDIN_DRV_NAME		"vdin"
#define VDIN_DEV_NAME		"vdin"
#define VDIN_CLS_NAME		"vdin"
#define PROVIDER_NAME		"vdin"

#define VDIN_DV_NAME		"dv_vdin"

#define VDIN_PUT_INTERVAL (HZ / 100)   /* 10ms, #define HZ 100 */

static dev_t vdin_devno;
static struct class *vdin_class;
static unsigned int vdin_addr_offset[VDIN_MAX_DEVS] = {0, 0x80};
static struct vdin_dev_s *vdin_devp[VDIN_MAX_DEVS];
static unsigned long mem_start, mem_end;
static unsigned int use_reserved_mem;
static unsigned int pr_times;

struct vdin_set_canvas_addr_s vdin_set_canvas_addr[VDIN_CANVAS_MAX_CNT];
static DECLARE_WAIT_QUEUE_HEAD(vframe_waitq);
/*
 * canvas_configure_mode
 * 0: canvas_config in driver probe
 * 1: start cofigure
 * 2: auto configure
 */
static int canvas_config_mode = 2;
static bool work_mode_simple;
static int phase_lock_flag;
/*game_mode_switch_frames:min num is 5 by 1080p60hz input test*/
static int game_mode_switch_frames = 10;
static int game_mode_phlock_switch_frames = 60;
static unsigned int dv_work_dolby;

unsigned int max_ignore_frame_cnt = 2;
unsigned int skip_frame_debug;

/* viu isr select:
 * enable viu_hw_irq for the bandwidth is enough on gxbb/gxtvbb and later ic(s)
 */
static bool viu_hw_irq = 1;
static bool de_fmt_flag;

u32 vdin_cfg_444_to_422_wmif_en;

#ifdef DEBUG_SUPPORT
module_param(canvas_config_mode, int, 0664);
MODULE_PARM_DESC(canvas_config_mode, "canvas configure mode");

module_param(work_mode_simple, bool, 0664);
MODULE_PARM_DESC(work_mode_simple, "enable/disable simple work mode");

module_param(viu_hw_irq, bool, 0664);
MODULE_PARM_DESC(viu_hw_irq, "viu_hw_irq");

module_param(dv_work_dolby, uint, 0664);
MODULE_PARM_DESC(dv_work_dolby, "dv_work_dolby");

module_param(game_mode_switch_frames, int, 0664);
MODULE_PARM_DESC(game_mode_switch_frames, "game mode switch <n> frames");
#endif

module_param(game_mode_phlock_switch_frames, int, 0664);
MODULE_PARM_DESC(game_mode_phlock_switch_frames,
		 "game mode switch <n> frames for phase_lock");

/* module_param(vrr_input_switch_frames, int, 0664);
 *MODULE_PARM_DESC(vrr_input_switch_frames,
 *		 "vrr input M_CONST switch <n> frames");
 */

int vdin_dbg_en;
module_param(vdin_dbg_en, int, 0664);
MODULE_PARM_DESC(vdin_dbg_en, "enable/disable vdin debug information");

int vdin_delay_num;
module_param(vdin_delay_num, int, 0664);
MODULE_PARM_DESC(vdin_delay_num, "vdin_delay_num vdin debug information");

static bool vdin_time_en;
module_param(vdin_time_en, bool, 0664);
MODULE_PARM_DESC(vdin_time_en, "enable/disable vdin debug information");
/*
 * the check flag in vdin_isr
 * bit0:bypass stop check,bit1:bypass cyc check
 * bit2:bypass vsync check,bit3:bypass vga check
 */
static unsigned int vdin_isr_flag;
module_param(vdin_isr_flag, uint, 0664);
MODULE_PARM_DESC(vdin_isr_flag, "flag which affect the skip field");

unsigned int vdin_drop_cnt;
module_param(vdin_drop_cnt, uint, 0664);
MODULE_PARM_DESC(vdin_drop_cnt, "vdin_drop_cnt");

int vdin_isr_drop;
module_param(vdin_isr_drop, int, 0664);
MODULE_PARM_DESC(vdin_isr_drop, "vdin_isr_drop vdin debug");

unsigned int vdin_isr_drop_num;
module_param(vdin_isr_drop_num, uint, 0664);
MODULE_PARM_DESC(vdin_isr_drop_num, "vdin_isr_drop_num");

enum vdin_vf_put_md vdin_frame_work_mode = VDIN_VF_PUT;
module_param(vdin_frame_work_mode, uint, 0664);
MODULE_PARM_DESC(vdin_frame_work_mode, "vdin_frame_work_mode");

unsigned int vdin_vrr_switch_cnt = 3;
module_param(vdin_vrr_switch_cnt, uint, 0664);
MODULE_PARM_DESC(vdin_vrr_switch_cnt, "vdin vrr switch cnt");

static unsigned int panel_reverse;
struct vdin_hist_s vdin1_hist;

int irq_max_count;

enum tvin_force_color_range_e color_range_force = COLOR_RANGE_AUTO;

int vpu_dev_register_flag;
struct vpu_dev_s *vpu_dev_clk_gate;
struct vpu_dev_s *vpu_dev_mem_pd_vdin0;
struct vpu_dev_s *vpu_dev_mem_pd_vdin1;
struct vpu_dev_s *vpu_dev_mem_pd_afbce;

static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp);

char *vf_get_receiver_name(const char *provider_name);

static int vdin_get_video_reverse(char *str)
{
	unsigned char *ptr = str;

	pr_info("%s: bootargs is %s.\n", __func__, str);
	if (strstr(ptr, "1"))
		panel_reverse = 1;
	return 0;
}
__setup("video_reverse=", vdin_get_video_reverse);

static void vdin_timer_func(struct timer_list *t)
{
	/*struct vdin_dev_s *devp = (struct vdin_dev_s *)arg;*/
	struct vdin_dev_s *devp = from_timer(devp, t, timer);

	/* state machine routine */
	tvin_smr(devp);
	/* add timer */
	devp->timer.expires = jiffies + VDIN_PUT_INTERVAL;
	add_timer(&devp->timer);
}

static const struct vframe_operations_s vdin_vf_ops = {
	.peek = vdin_vf_peek,
	.get  = vdin_vf_get,
	.put  = vdin_vf_put,
	.event_cb = vdin_event_cb,
	.vf_states = vdin_vf_states,
};

/*
 * not game mode vdin has one frame
 * delay this interface get vdin
 * whether has two frame delay
 * return
 *	0: vdin is one delay
 *	1: vdin is two delay
 */
int get_vdin_add_delay_num(void)
{
	if (vdin_delay_num)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(get_vdin_add_delay_num);

unsigned int get_vdin_buffer_num(void)
{
	struct vdin_dev_s *vdin0_devp = vdin_devp[0];

	if (vdin0_devp)
		return vdin0_devp->frame_buff_num;
	else
		return 0;
}
EXPORT_SYMBOL(get_vdin_buffer_num);

void tvin_update_vdin_prop(void)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct vframe_s *update_wr_vf = NULL;
	struct vdin_dev_s *vdin0_devp = vdin_devp[0];

	if (!vdin0_devp || !vdin0_devp->frontend ||
	    !(vdin0_devp->flags & VDIN_FLAG_ISR_EN))
		return;

	sm_ops = vdin0_devp->frontend->sm_ops;
	if (!vdin0_devp->last_wr_vfe &&
	    vdin0_devp->game_mode & VDIN_GAME_MODE_1 &&
	    vdin0_devp->vdin_function_sel & VDIN_PROP_RX_UPDATE)
		update_wr_vf = &vdin0_devp->last_wr_vfe->vf;
	else if (!vdin0_devp->curr_wr_vfe &&
		   vdin0_devp->game_mode & VDIN_GAME_MODE_2 &&
		   vdin0_devp->vdin_function_sel & VDIN_PROP_RX_UPDATE)
		update_wr_vf = &vdin0_devp->curr_wr_vfe->vf;
	else
		update_wr_vf = NULL;

	if (update_wr_vf) {
		sm_ops->get_sig_property(vdin0_devp->frontend, &vdin0_devp->prop);
		vdin_set_drm_data(vdin0_devp, update_wr_vf);
		vdin_set_vframe_prop_info(update_wr_vf, vdin0_devp);
		vdin_set_freesync_data(vdin0_devp, update_wr_vf);
	}

	if (vdin_isr_monitor & DBG_RX_UPDATE_VDIN_PROP && update_wr_vf)
		pr_info("tvin update prop index:%d game(%d)\n",
			update_wr_vf->index, vdin0_devp->game_mode);
	else if (vdin_isr_monitor & DBG_RX_UPDATE_VDIN_PROP)
		pr_info("tvin update prop game(%d)\n", vdin0_devp->game_mode);
}
EXPORT_SYMBOL(tvin_update_vdin_prop);

void tvin_notify_vdin_skip_frame(unsigned int drop_num)
{
	struct vdin_dev_s *vdin0_devp = vdin_devp[0];

	if (!vdin0_devp || !(vdin0_devp->flags & VDIN_FLAG_ISR_EN))
		return;

	vdin_vf_skip_all_disp(vdin0_devp->vfp);

	if (vdin0_devp->game_mode || IS_TVAFE_SRC(vdin0_devp->parm.port))
		vdin_pause_hw_write(vdin0_devp, 0);

	vdin0_devp->frame_drop_num = drop_num;
	vdin_drop_frame_info(vdin0_devp, "tvin notify skip frame");
}
EXPORT_SYMBOL(tvin_notify_vdin_skip_frame);

/*
 * 1. find the corresponding frontend according to the port & save it.
 * 2. set default register, including:
 *		a. set default write canvas address.
 *		b. mux null input.
 *		c. set clock auto.
 *		a&b will enable hw work.
 * 3. call the callback function of the frontend to open.
 * 4. register provider.
 * 5. create timer for state machine.
 *
 * port: the port supported by frontend
 * index: the index of frontend
 * 0 success, otherwise failed
 */
int vdin_open_fe(enum tvin_port_e port, int index,  struct vdin_dev_s *devp)
{
	struct tvin_frontend_s *fe = tvin_get_frontend(port, index);
	int ret = 0;

	if (!fe) {
		pr_err("%s(%d): not supported port 0x%x\n",
		       __func__, devp->index, port);
		return -1;
	}

	devp->frontend = fe;
	devp->parm.port = port;

	devp->parm.info.status = TVIN_SIG_STATUS_NULL;
	/* clear color para*/
	memset(&devp->pre_prop, 0, sizeof(devp->pre_prop));
	 /* clear color para*/
	memset(&devp->prop, 0, sizeof(devp->prop));

	/* vdin msr clock gate enable */
	if (devp->msr_clk) {
		devp->vdin_clk_flag = 1;
		clk_prepare_enable(devp->msr_clk);
		}

	if (devp->frontend->dec_ops && devp->frontend->dec_ops->open)
		ret = devp->frontend->dec_ops->open(devp->frontend, port);
	/* check open status */
	if (ret)
		return 1;

	devp->flags |= VDIN_FLAG_DEC_OPENED;
	/* init vdin state machine */
	tvin_smr_init(devp->index);
	/* for 5.4 init_timer(&devp->timer);*/
	/*devp->timer.data = (ulong)devp;*/
	devp->timer.function = vdin_timer_func;
	devp->timer.expires = jiffies + VDIN_PUT_INTERVAL;
	add_timer(&devp->timer);

	pr_info("%s port:0x%x ok\n", __func__, port);
	return 0;
}

/* 1. disable hw work, including:
 *		a. mux null input.
 *		b. set clock off.
 * 2. delete timer for state machine.
 * 3. unregister provider & notify receiver.
 * 4. call the callback function of the frontend to close.
 */
void vdin_close_fe(struct vdin_dev_s *devp)
{
	/* avoid null pointer oops */
	if (!devp) {
		pr_info("%s: null pointer\n", __func__);
		return;
	}
	/* bt656 clock gate disable */
	if (devp->msr_clk) {
		devp->vdin_clk_flag = 0;
		clk_disable_unprepare(devp->msr_clk);
		}

	del_timer_sync(&devp->timer);
	if (devp->frontend && devp->frontend->dec_ops->close) {
		devp->frontend->dec_ops->close(devp->frontend);
		devp->frontend = NULL;
	}
	devp->parm.port = TVIN_PORT_NULL;
	devp->parm.info.fmt  = TVIN_SIG_FMT_NULL;
	devp->parm.info.status = TVIN_SIG_STATUS_NULL;

	devp->flags &= (~VDIN_FLAG_DEC_OPENED);

	devp->event_info.event_sts = TVIN_SIG_CHG_CLOSE_FE;
	vdin_send_event(devp, devp->event_info.event_sts);

	pr_info("%s ok\n", __func__);
}

void vdin_frame_lock_check(struct vdin_dev_s *devp, int state)
{
	struct vrr_notifier_data_s vrr_data;

	if (devp->index != 0 || devp->work_mode == VDIN_WORK_MD_V4L)
		return;

	vrr_data.input_src = VRR_INPUT_TVIN;
	vrr_data.target_vfreq_num = devp->parm.info.fps;
	vrr_data.target_vfreq_den = 1;
	vrr_data.vrr_mode = devp->prop.vtem_data.vrr_en |
			(vdin_check_is_spd_data(devp) &&
			(devp->prop.spd_data.data[5] >> 1 & 0x7));
	/* save vrr_mode status */
	devp->vrr_data.vrr_mode = vrr_data.vrr_mode;

	if (state) {
		if (devp->game_mode) {
			aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_ON,
						   &vrr_data);
			devp->vrr_data.frame_lock_vrr_en = vrr_data.vrr_mode;
			pr_info("%s: state =1 and Game, enable frame lock mode:%x\n", __func__,
				vrr_data.vrr_mode);
		} else {
			aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_OFF,
							   &vrr_data);
			devp->vrr_data.frame_lock_vrr_en = FALSE;
			pr_info("%s: state =1 and no Game, disable v\n", __func__);
		}
	} else {
		aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_OFF,
						   &vrr_data);
		devp->vrr_data.frame_lock_vrr_en = FALSE;
		pr_info("%s: state=0 ,disable frame lock\n", __func__);
	}
}

static inline void vdin_get_in_out_fps(struct vdin_dev_s *devp)
{
	unsigned int vinfo_out_fps = 0;
	const struct vinfo_s *vinfo = NULL;

	if (devp->dtdata->hw_ver >= VDIN_HW_T7 && IS_HDMI_SRC(devp->parm.port)) {
		/* get vin fps */
		if (devp->cycle != 0)
			devp->vdin_std_duration = DIV_ROUND_CLOSEST(devp->msr_clk_val, devp->cycle);
		else
			devp->vdin_std_duration = devp->parm.info.fps;
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		/* get vout fps */
		vinfo_out_fps = vout_frame_rate_measure(1);
#endif
		if (vinfo_out_fps != 0) {
			devp->vinfo_std_duration = vinfo_out_fps / 1000;
		} else {
			vinfo = get_current_vinfo();
			devp->vinfo_std_duration =
				vinfo->sync_duration_num / vinfo->sync_duration_den;
		}
	} else {
		devp->vdin_std_duration = devp->parm.info.fps;
		vinfo = get_current_vinfo();
		devp->vinfo_std_duration =
			vinfo->sync_duration_num / vinfo->sync_duration_den;
	}
}

static bool vdin_need_game_mode(struct vdin_dev_s *devp)
{
	if (devp->fmt_info_p->scan_mode != TVIN_SCAN_MODE_INTERLACED)
		return true;

	// 1080i maybe has game mode demand
	if ((devp->vdin_function_sel & VDIN_INTERLACE_GAME_MODE) &&
	    devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
	    devp->fmt_info_p->v_active == 540)
		return true;
	else
		return false;
}

static void vdin_game_mode_check(struct vdin_dev_s *devp)
{
	devp->game_mode_pre = devp->game_mode;
	devp->vrr_frame_rate_min = vrr_check_frame_rate_min_hz();
	if (game_mode == 1 && vdin_need_game_mode(devp)) {
		vdin_get_in_out_fps(devp);
		if (devp->vinfo_std_duration > (devp->vdin_std_duration + 2)) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0) {
				/* if vout fps greater than vin fps use mode0 */
				devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_SWITCH_EN);
			} else {
				devp->game_mode = VDIN_GAME_MODE_0;
			}
		} else if (devp->parm.info.fps >= devp->vrr_frame_rate_min &&
			devp->parm.info.fps <= 144) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0) {
				devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_1 |
					VDIN_GAME_MODE_SWITCH_EN);
			} else {
				devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_1);
			}
		} else if (devp->parm.info.fps >= 25 &&
			   devp->parm.info.fps < devp->vrr_frame_rate_min) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0) {
				devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_SWITCH_EN);
			} else {
				devp->game_mode = VDIN_GAME_MODE_0;
			}
		} else {
			devp->game_mode = VDIN_GAME_MODE_0;
		}
	} else if (game_mode == 2) {/*for debug force game mode*/
		devp->game_mode = (VDIN_GAME_MODE_0 | VDIN_GAME_MODE_1);
	} else if (game_mode == 3) {/*for debug force game mode*/
		devp->game_mode = (VDIN_GAME_MODE_0 | VDIN_GAME_MODE_2);
	} else if (game_mode == 4) {/*for debug force game mode*/
		devp->game_mode = VDIN_GAME_MODE_0;
	} else if (game_mode == 5) {/*for debug force game mode*/
		devp->game_mode = (VDIN_GAME_MODE_0 | VDIN_GAME_MODE_1 |
			VDIN_GAME_MODE_SWITCH_EN);
	} else {
		devp->game_mode = 0;
	}

	/* dv is auto game not support manual set game */
	if (vdin_dv_not_manual_game(devp))
		devp->game_mode = 0;

	if (vdin_force_game_mode)
		devp->game_mode = vdin_force_game_mode;
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
		pr_info("%s: game_mode_cfg=0x%x;pre:%d,cur:%#x,force mode:0x%x,fps:%d,vout:%d\n",
			__func__, game_mode, devp->game_mode_pre, devp->game_mode,
			vdin_force_game_mode, devp->vdin_std_duration,
			devp->vinfo_std_duration);
}

/* game mode change,check in every isr routine */
static inline void vdin_game_mode_dynamic_chg(struct vdin_dev_s *devp)
{
	if (devp->game_mode_pre & VDIN_GAME_MODE_2) {
		if (devp->game_mode & VDIN_GAME_MODE_2)
			devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
		else if (devp->game_mode & VDIN_GAME_MODE_1)
			devp->game_mode_chg = VDIN_GAME_MODE_2_TO_1;
		else if (devp->game_mode & VDIN_GAME_MODE_0)
			devp->game_mode_chg = VDIN_GAME_MODE_ON_TO_OFF;
		else if (!devp->game_mode)
			devp->game_mode_chg = VDIN_GAME_MODE_ON_TO_OFF;
	} else if (devp->game_mode_pre & VDIN_GAME_MODE_1) {
		if (devp->game_mode & VDIN_GAME_MODE_2)
			devp->game_mode_chg = VDIN_GAME_MODE_1_TO_2;
		else if (devp->game_mode & VDIN_GAME_MODE_1)
			devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
		else if (devp->game_mode & VDIN_GAME_MODE_0)
			devp->game_mode_chg = VDIN_GAME_MODE_ON_TO_OFF;
		else if (!devp->game_mode)
			devp->game_mode_chg = VDIN_GAME_MODE_ON_TO_OFF;
	} else if (devp->game_mode_pre & VDIN_GAME_MODE_0) {
		if (devp->game_mode & VDIN_GAME_MODE_1 ||
			devp->game_mode & VDIN_GAME_MODE_2)
			devp->game_mode_chg = VDIN_GAME_MODE_OFF_TO_ON;
		else /* game mode 0 or no game mode */
			devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
	} else {/* non game mode */
		if (devp->game_mode & VDIN_GAME_MODE_1 ||
			devp->game_mode & VDIN_GAME_MODE_2)
			devp->game_mode_chg = VDIN_GAME_MODE_OFF_TO_ON;
		else/* game mode 0 or no game mode */
			devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
	}

	if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
		pr_info("%s %d,game:pre(%#x)cur(%#x)chg:%d\n",
			__func__, __LINE__, devp->game_mode_pre,
			devp->game_mode, devp->game_mode_chg);
	devp->game_mode_bak = devp->game_mode_pre;
	devp->game_mode_pre = devp->game_mode;
}

static inline void vdin_game_mode_dynamic_check(struct vdin_dev_s *devp)
{
	if (!vdin_need_game_mode(devp))
		return;

	vdin_get_in_out_fps(devp);
	devp->vrr_frame_rate_min = vrr_check_frame_rate_min_hz();
	if (devp->vrr_data.vrr_mode &&
	    devp->vdin_std_duration >= 25 &&
	    devp->vdin_std_duration < devp->vrr_frame_rate_min) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0)
			devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_SWITCH_EN);
		else
			devp->game_mode = VDIN_GAME_MODE_0;
	} else if (devp->vrr_data.vrr_mode &&
		   devp->vdin_std_duration >= devp->vrr_frame_rate_min) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0)
			devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_1 |
					VDIN_GAME_MODE_SWITCH_EN);
		else
			devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_1);
	} else if (devp->vdin_std_duration < 25 ||
		   (devp->vinfo_std_duration >
		    (devp->vdin_std_duration + 1) * 2)) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0) {
			devp->game_mode &= ~VDIN_GAME_MODE_1;
			devp->game_mode &= ~VDIN_GAME_MODE_2;
		} else {
			devp->game_mode &= ~VDIN_GAME_MODE_SWITCH_EN;
			devp->game_mode &= ~VDIN_GAME_MODE_1;
			devp->game_mode &= ~VDIN_GAME_MODE_2;
		}
	} else if ((devp->vdin_std_duration >= 25 &&
		    devp->vdin_std_duration < devp->vrr_frame_rate_min) ||
		   (devp->vinfo_std_duration >
		    (devp->vdin_std_duration + 2))) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0)
			devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_SWITCH_EN);
		else
			devp->game_mode = VDIN_GAME_MODE_0;
	} else {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && panel_reverse == 0)
			devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_1 |
					VDIN_GAME_MODE_SWITCH_EN);
		else
			devp->game_mode = (VDIN_GAME_MODE_0 |
					VDIN_GAME_MODE_1);
	}

	/* dv is auto game not support manual set game */
	if (vdin_dv_not_manual_game(devp))
		devp->game_mode = 0;

	if (vdin_force_game_mode)
		devp->game_mode = vdin_force_game_mode;

	if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
		pr_info("%s vrr_mode:%d,game:pre(%#x)cur(%#x)in fps:%d out fps:%d cycle:%#x\n",
			__func__, devp->vrr_data.vrr_mode, devp->game_mode_pre, devp->game_mode,
			devp->vdin_std_duration, devp->vinfo_std_duration, devp->cycle);
}

static void vdin_game_mode_transfer(struct vdin_dev_s *devp)
{
	unsigned int game_mode_backup;

	if (!devp->game_mode || devp->vdin_function_sel & VDIN_GAME_NOT_TANSFER)
		return;

	game_mode_backup = devp->game_mode;
	devp->vrr_frame_rate_min = vrr_check_frame_rate_min_hz();
	/*switch to game mode 2 from game mode 1,otherwise may appear blink*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (devp->game_mode & VDIN_GAME_MODE_SWITCH_EN) {
			vdin_game_mode_dynamic_check(devp);
			/* phase unlock state, wait ph lock*/
			/* make sure phase lock for next few frames */
			if ((vlock_get_phlock_flag() && vlock_get_vlock_flag()) ||
					frame_lock_vrr_lock_status())
				phase_lock_flag++;
			else
				phase_lock_flag = 0;
			if (phase_lock_flag >= game_mode_phlock_switch_frames) {
				/* vrr mode vinfo_std_duration not correct so separate judgment */
				if (devp->vrr_data.vrr_mode && devp->vdin_std_duration >= 25 &&
				    devp->vdin_std_duration < devp->vrr_frame_rate_min) {
					/*1 to 2 need delay more than one vf*/
					devp->game_mode = (VDIN_GAME_MODE_0 |
						VDIN_GAME_MODE_1);
				} else if (devp->vrr_data.vrr_mode &&
					   devp->vdin_std_duration >= devp->vrr_frame_rate_min) {
					devp->game_mode = (VDIN_GAME_MODE_0 |
						VDIN_GAME_MODE_2);
				} else if (devp->vdin_std_duration < 25 ||
					   (devp->vinfo_std_duration >
					    (devp->vdin_std_duration + 1) * 2)) {
					devp->game_mode &= ~VDIN_GAME_MODE_1;
					devp->game_mode &= ~VDIN_GAME_MODE_2;
				} else if ((devp->vdin_std_duration >= 25 &&
					    devp->vdin_std_duration < devp->vrr_frame_rate_min) ||
					   (devp->vinfo_std_duration >
					    (devp->vdin_std_duration + 2))) {
					devp->game_mode = (VDIN_GAME_MODE_0 |
						VDIN_GAME_MODE_1);
				} else {
					devp->game_mode = (VDIN_GAME_MODE_0 |
						VDIN_GAME_MODE_2);
				}
				phase_lock_flag = 0;
				if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
					pr_info("switch(%d) to game mode 0x%x, frame_cnt=%d in fps:%d out fps:%d\n",
						devp->game_mode_pre, devp->game_mode,
						devp->frame_cnt, devp->vdin_std_duration,
						devp->vinfo_std_duration);
			}
		} else {
			/* if phase lock fail, exit game mode and re-entry
			 * after phase lock
			 */
			if (!vlock_get_phlock_flag() && !frame_lock_vrr_lock_status()) {
				if (phase_lock_flag++ > 1) {
					vdin_game_mode_dynamic_check(devp);
					phase_lock_flag = 0;
					/* vlock need reset automatic,
					 * vlock will re-lock
					 */
				}
			}
			/* fps 60 in and 120 out switch to vrr need switch again */
			if (devp->vrr_data.vrr_mode && !(devp->game_mode & VDIN_GAME_MODE_2))
				vdin_game_mode_dynamic_check(devp);
		}
		if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME &&
		    phase_lock_flag && !(phase_lock_flag % 10))
			pr_info("lock_cnt:%d, mode:%x in fps:%d out fps:%d\n",
				phase_lock_flag, devp->game_mode,
				devp->vdin_std_duration, devp->vinfo_std_duration);
	} else {
		if (devp->frame_cnt >= game_mode_switch_frames &&
		    (devp->game_mode & VDIN_GAME_MODE_SWITCH_EN)) {
			if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME) {
				pr_info("switch to game mode (0x%x-->0x5), frame_cnt=%d\n",
					devp->game_mode, devp->frame_cnt);
			}
			devp->game_mode = (VDIN_GAME_MODE_0 | VDIN_GAME_MODE_2);
		}
	}

	if (game_mode_backup != devp->game_mode)
		devp->game_mode_pre = game_mode_backup;
	/* remember game mode change, handle send vf change in ISR */
	vdin_game_mode_dynamic_chg(devp);
}

/* game mode changed while vdin dec have started
 * game_mode,zero:OFF,non-zero:ON
 */
void vdin_game_mode_chg(struct vdin_dev_s *devp,
	unsigned int old_mode, unsigned int new_mode)
{
	ulong flags = 0;

	if (old_mode == new_mode)
		return;

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		spin_lock_irqsave(&devp->isr_lock, flags);
		/* game_mode change */
		if (!old_mode && new_mode)
			devp->game_mode_chg = VDIN_GAME_MODE_OFF_TO_ON;
		else if (old_mode && !new_mode)
			devp->game_mode_chg = VDIN_GAME_MODE_ON_TO_OFF;
		else
			devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
		if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
			pr_info("%s;game,old:%d,new:%d,chg:%d;game cur:%#x\n",
				__func__, old_mode, new_mode,
				devp->game_mode_chg, devp->game_mode);
		game_mode = new_mode;
		devp->game_mode_bak = devp->game_mode;
		vdin_game_mode_check(devp);
		spin_unlock_irqrestore(&devp->isr_lock, flags);
	}
}

static void vdin_handle_game_mode_chg(struct vdin_dev_s *devp)
{
	int i;
	struct vf_entry *master = NULL;

	if (devp->game_mode_chg != VDIN_GAME_MODE_UN_CHG &&
		devp->game_mode_chg < VDIN_GAME_MODE_NUM) {
		/* recycle all vf in write mode list */
		for (i = 0; i < devp->vfp->size; i++) {
			master = vf_get_master(devp->vfp, i);
			if (!master)
				break;
			if (master->status == VF_STATUS_WM)
				receiver_vf_put(&master->vf, devp->vfp);
		}
		devp->curr_wr_vfe = NULL;
		devp->last_wr_vfe = NULL;
		devp->chg_drop_frame_cnt = 2;
		if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
			pr_info("%s,game_mode_chg:%d,game_mode_bak:%#x\n", __func__,
				devp->game_mode_chg, devp->game_mode_bak);
	}
	devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
}

/*
 *based on the bellow parameters:
 *	a.h_active	(vf->width = devp->h_active)
 *	b.v_active	(vf->height = devp->v_active)
 *function: init vframe
 *1.set source type & mode
 *2.set source signal format
 *3.set pixel aspect ratio
 *4.set display ratio control
 *5.init slave vframe
 *6.set slave vf source type & mode
 */
static void vdin_vf_init(struct vdin_dev_s *devp)
{
	int i = 0;
	unsigned int chroma_id, addr, index;
	struct vf_entry *master, *slave;
	struct vframe_s *vf;
	struct vf_pool *p = devp->vfp;
	enum tvin_scan_mode_e	scan_mode;
	unsigned int chroma_size = 0;
	unsigned int luma_size = 0;
	ulong phy_c_addr;

	index = devp->index;
	/* const struct tvin_format_s *fmt_info = tvin_get_fmt_info(fmt); */
	if (vdin_dbg_en)
		pr_info("vdin.%d vframe initial information table: (%d of %d)\n",
			index, p->size, p->max_size);

	for (i = 0; i < p->size; ++i) {
		master = vf_get_master(p, i);
		master->flag |= VF_FLAG_NORMAL_FRAME;
		vf = &master->vf;
		memset(vf, 0, sizeof(struct vframe_s));
		vf->index = i;
		if (devp->vf_mem_size_small) {
			vf->width = devp->h_shrink_out;
			vf->height = devp->v_shrink_out;
		} else {
			vf->width = devp->h_active;
			vf->height = devp->v_active;
		}
		if (devp->game_mode != 0)
			vf->flag |= VFRAME_FLAG_GAME_MODE;
		if (devp->vdin_pc_mode)
			vf->flag |= VFRAME_FLAG_PC_MODE;
		scan_mode = devp->fmt_info_p->scan_mode;
		if ((scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     (!(devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
		     devp->parm.info.fmt != TVIN_SIG_FMT_NULL)) ||
		     IS_TVAFE_ATV_SRC(devp->parm.port))
			vf->height <<= 1;
#ifndef VDIN_DYNAMIC_DURATION
		vf->duration = devp->fmt_info_p->duration;
#endif
		/* init canvas config */
		/* if output fmt is nv21 or nv12 ,
		 * use the two continuous canvas for one field
		 */
		switch (devp->format_convert) {
		case VDIN_FORMAT_CONVERT_YUV_NV12:
		case VDIN_FORMAT_CONVERT_YUV_NV21:
		case VDIN_FORMAT_CONVERT_RGB_NV12:
		case VDIN_FORMAT_CONVERT_RGB_NV21:
			chroma_size = devp->canvas_w * devp->canvas_h / 2;
			luma_size = devp->canvas_w * devp->canvas_h;
			chroma_id = (vdin_canvas_ids[index][(vf->index << 1) + 1]) << 8;
			addr = vdin_canvas_ids[index][vf->index << 1] | chroma_id;
			vf->plane_num = 2;
			break;
		default:
			addr = vdin_canvas_ids[index][vf->index];
			vf->plane_num = 1;
			break;
		}

		if (devp->dtdata->hw_ver >= VDIN_HW_T7 &&
		    i < devp->canvas_max_num) {
			/* use phy address */
			vf->canvas0Addr = (u32)-1;
			vf->canvas1Addr = (u32)-1;
			/*save phy address to vf*/

			vf->canvas0_config[0].phy_addr = devp->vf_mem_start[i];
			vf->canvas0_config[0].width = devp->canvas_w;
			vf->canvas0_config[0].height = devp->canvas_h;
			vf->canvas0_config[0].block_mode = 0;
			/*if (vdin_dbg_en)*/
			/*	pr_info("vf canvas0 addr:%d paddr:0x%x w:%d v:%d\n",*/
			/*		addr,*/
			/*		vf->canvas0_config[0].phy_addr,*/
			/*		vf->canvas0_config[0].width,*/
			/*		vf->canvas0_config[0].height);*/

			if (vf->plane_num == 2) {
				if (devp->work_mode == VDIN_WORK_MD_V4L &&
					devp->v4l2_fmt.fmt.pix_mp.num_planes == 2)
					phy_c_addr = devp->vf_mem_c_start[i];
				else
					phy_c_addr = (u32)(devp->vf_mem_start[i] + luma_size);
				vf->canvas0_config[1].phy_addr = phy_c_addr;
				vf->canvas0_config[1].width = devp->canvas_w;
				vf->canvas0_config[1].height =
						devp->canvas_h / vf->plane_num;
				vf->canvas0_config[1].block_mode = 0;
				/*if (vdin_dbg_en)*/
				/*	pr_info("vf canvas0 addr:%d paddr:0x%x w:%d v:%d\n",*/
				/*		addr,*/
				/*		vf->canvas0_config[0].phy_addr,*/
				/*		vf->canvas0_config[0].width,*/
				/*		vf->canvas0_config[0].height);*/
			}
			master->phy_y_addr_bak = vf->canvas0_config[0].phy_addr;
			master->phy_c_addr_bak = vf->canvas0_config[1].phy_addr;
		} else {
			vf->canvas0Addr = addr;
			vf->canvas1Addr = addr;
		}
		devp->vf_canvas_id[i] = addr;

		/* init afbce config */
		if (devp->afbce_info) {
			vf->compHeadAddr = devp->afbce_info->fm_head_paddr[i];
			vf->compBodyAddr = devp->afbce_info->fm_body_paddr[i];
			vf->compWidth  = devp->h_active;
			vf->compHeight = devp->v_active;
		}
		/* keeper config */
		vf->mem_handle = devp->vf_codec_mem[i];
		/* set source type & mode */
		vdin_set_source_type(devp, vf);
		vdin_set_source_mode(devp, vf);
		/* set source signal format */
		vf->sig_fmt = devp->parm.info.fmt;
		/* set pixel aspect ratio */
		vdin_set_pixel_aspect_ratio(devp, vf);
		/*set display ratio control */
		if (IS_TVAFE_SRC(devp->parm.port) ||
		    ((devp->vdin_function_sel & VDIN_SET_DISPLAY_RATIO) &&
		     (IS_TVAFE_SRC(devp->parm.port) || IS_HDMI_SRC(devp->parm.port))))
			vdin_set_display_ratio(devp, vf);
		vdin_set_source_bitdepth(devp, vf);
		/* init slave vframe */
		slave	= vf_get_slave(p, i);
		if (!slave)
			continue;
		slave->flag = master->flag;
		memset(&slave->vf, 0, sizeof(struct vframe_s));
		slave->vf.index   = vf->index;
		slave->vf.width   = vf->width;
		slave->vf.height	   = vf->height;
		slave->vf.duration    = vf->duration;
		slave->vf.ratio_control   = vf->ratio_control;
		slave->vf.canvas0Addr = vf->canvas0Addr;
		slave->vf.canvas1Addr = vf->canvas1Addr;
		/* set slave vf source type & mode */
		slave->vf.source_type = vf->source_type;
		slave->vf.source_mode = vf->source_mode;
		slave->vf.bitdepth = vf->bitdepth;
		if (vdin_dbg_en)
			pr_info("\t%2d: 0x%2x %ux%u, duration = %u\n",
				vf->index, vf->canvas0Addr, vf->width,
				vf->height, vf->duration);
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
static void vdin_rdma_irq(void *arg)
{
	struct vdin_dev_s *devp = arg;
	int ret;

	ret = rdma_config(devp->rdma_handle,
			  (devp->rdma_enable & 1) ?
			  devp->rdma_irq : RDMA_TRIGGER_MANUAL);
	if (ret == 0)
		devp->flags_isr |= VDIN_FLAG_RDMA_DONE;
	else
		devp->flags_isr &= ~VDIN_FLAG_RDMA_DONE;

	devp->rdma_irq_cnt++;
	return;
}

static struct rdma_op_s vdin_rdma_op[VDIN_MAX_DEVS];
#endif

static void vdin_double_write_confirm(struct vdin_dev_s *devp)
{
	/* enable double write only afbce is supported */
	if (devp->double_wr_cfg && devp->afbce_valid) {
		devp->double_wr = 1;
	} else {
		devp->double_wr = 0;
		devp->afbce_valid = 0;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	/* CAN NOT use dw due to h shrink lose color when dv tunnel signal in */
	if (vdin_is_dolby_signal_in(devp) &&
		!(devp->vdin_function_sel & VDIN_AFBCE_DOLBY)) {
		/*if (devp->dtdata->hw_ver < VDIN_HW_T7)*/
		devp->double_wr = 0;
		devp->afbce_valid = 0;
		if (vdin_dbg_en)
			pr_info("dv in dw %d\n", devp->double_wr);
	}
#endif
}

static u32 vdin_is_delay_vfe2rd_list(struct vdin_dev_s *devp)
{
	if (devp->index == 0 && !game_mode &&
	    devp->work_mode != VDIN_WORK_MD_V4L) {
		return 1;
	} else {
		return 0;
	}
}

static void vdin_scale_and_cutwin_handle(struct vdin_dev_s *devp)
{
	u32 vdin0_max_w_h;

	if (!devp || !devp->fmt_info_p)
		return;

	vdin0_max_w_h = devp->dtdata->vdin0_max_w_h;

	if (devp->vdin_function_sel & VDIN_SACALE_4096_2_3840 &&
	    devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ) {
		devp->prop.scaling4w = 3840;
		devp->prop.scaling4h = 2160;
	}

	if (devp->parm.dest_width != 0 ||
	    devp->parm.dest_height != 0) {
		devp->prop.scaling4w = devp->parm.dest_width;
		devp->prop.scaling4h = devp->parm.dest_height;
	}

	/* prevent display of resolution beyond specification */
	if (IS_HDMI_SRC(devp->parm.port) && vdin0_max_w_h) {
		if (devp->fmt_info_p->h_active > (vdin0_max_w_h >> 16))
			devp->prop.scaling4w = vdin0_max_w_h >> 16;
		if (devp->fmt_info_p->v_active > (vdin0_max_w_h & 0xffff))
			devp->prop.scaling4h = vdin0_max_w_h & 0xffff;
	}

	if (devp->flags & VDIN_FLAG_MANUAL_CONVERSION) {
		devp->prop.dest_cfmt = devp->debug.dest_cfmt;
		devp->prop.scaling4w = devp->debug.scaling4w;
		devp->prop.scaling4h = devp->debug.scaling4h;
	}

	if (devp->vdin_function_sel & VDIN_CUTWIN_4096_2_3840 &&
	    devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ) {
		devp->prop.hs = 128;
		devp->prop.he = 128;
	}

	if (devp->debug.cutwin.hs ||
	    devp->debug.cutwin.he ||
	    devp->debug.cutwin.vs ||
	    devp->debug.cutwin.ve) {
		devp->prop.hs = devp->debug.cutwin.hs;
		devp->prop.he = devp->debug.cutwin.he;
		devp->prop.vs = devp->debug.cutwin.vs;
		devp->prop.ve = devp->debug.cutwin.ve;
	}
}

/*
 * 1. config canvas base on  canvas_config_mode
 *		0: canvas_config in driver probe
 *		1: start cofigure
 *		2: auto configure
 * 2. recalculate h_active and v_active, including:
 *		a. vdin_set_decimation.
 *		b. vdin_set_cut_win.
 *		c. vdin_set_hv_scale.
 * 3. enable hw work, including:
 *		a. mux null input.
 *		b. set clock auto.
 * 4. set all registers including:
 *		a. mux input.
 * 5. call the callback function of the frontend to start.
 * 6. enable irq .
 */
int vdin_start_dec(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct vf_entry *vfe;
	u32 sts;

	/* avoid null pointer oops */
	if (IS_ERR_OR_NULL(devp->fmt_info_p)) {
		pr_info("[vdin]%s null error.\n", __func__);
		return -1;
	}

	if (devp->index == 0 && devp->dtdata->vdin0_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return -1;
	}

	if (devp->index == 1 && devp->dtdata->vdin1_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return -1;
	}

	/* refuse start dec during suspend period,
	 * otherwise system will not go to suspend due to vdin clk on
	 */
	if (devp->flags & VDIN_FLAG_SUSPEND) {
		pr_info("err:[vdin.%d]%s when suspend.\n",
			devp->index, __func__);
		return -1;
	}

	if (devp->frontend && devp->frontend->sm_ops) {
		sm_ops = devp->frontend->sm_ops;
		sm_ops->get_sig_property(devp->frontend, &devp->prop);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
			vdin_check_hdmi_hdr(devp);

		/* signal maybe change after stabled so not need update here */
		//vdin_update_prop(devp);
		pr_info("%s dv:%d hdr:%d allm:0x%x fps:%d sg_type:0x%x ratio:%d,vrr:%d prop_vrr:%d,%d\n",
			__func__,
			devp->dv.dv_flag, devp->prop.vdin_hdr_flag,
			devp->prop.latency.allm_mode, devp->prop.fps,
			devp->parm.info.signal_type,
			devp->parm.info.aspect_ratio,
			devp->vrr_data.vdin_vrr_en_flag,
			devp->prop.vtem_data.vrr_en,
			devp->prop.spd_data.data[5]);
		if (!(devp->flags & VDIN_FLAG_V4L2_DEBUG))
			devp->parm.info.cfmt = devp->prop.color_format;

		vdin_scale_and_cutwin_handle(devp);
	}
	/*gxbb/gxl/gxm use clkb as vdin clk,
	 *for clkb is low speed,which is enough for 1080p process,
	 *gxtvbb/txl use vpu clk for process 4k
	 *g12a use vpu clk for process 4K input buf can't output 4k
	 */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_gxl_cpu() || is_meson_gxm_cpu() || is_meson_gxbb_cpu() ||
	    is_meson_txhd_cpu())
		vdin_vpu_clk_gate_on_off(devp, 1);
		/*switch_vpu_clk_gate_vmod(VPU_VPU_CLKB, VPU_CLK_GATE_ON);*/
#endif
	vdin_sw_reset(devp);
	/*enable clk*/
	vdin_clk_on_off(devp, true);
	vdin_set_default_regmap(devp);

	if (devp->dts_config.urgent_en && devp->index == 0)
		vdin_urgent_patch_resume(devp->addr_offset);

	devp->vdin_pc_mode = vdin_pc_mode;
	vdin_get_format_convert(devp);
	devp->curr_wr_vfe = NULL;
	devp->frame_drop_num = 0;
	devp->vfp->skip_vf_num = devp->prop.skip_vf_num;
	if (devp->vfp->skip_vf_num >= VDIN_CANVAS_MAX_CNT)
		devp->vfp->skip_vf_num = 0;
	devp->canvas_config_mode = canvas_config_mode;
	/* h_active/v_active will be recalculated by bellow calling */
	vdin_set_decimation(devp);
	/* check if need enable afbce */
	vdin_afbce_mode_init(devp);
	vdin_double_write_confirm(devp);
	vdin_set_double_write_regs(devp);

	if (de_fmt_flag == 1 &&
	    (devp->prop.vs != 0 ||
	    devp->prop.ve != 0 ||
	    devp->prop.hs != 0 ||
	    devp->prop.he != 0)) {
		devp->prop.vs = 0;
		devp->prop.ve = 0;
		devp->prop.hs = 0;
		devp->prop.he = 0;
		devp->prop.pre_vs = 0;
		devp->prop.pre_ve = 0;
		devp->prop.pre_hs = 0;
		devp->prop.pre_he = 0;
		pr_info("ioctl start dec,restore the cutwin param.\n");
	}
	vdin_set_cutwin(devp);
	vdin_set_hv_scale(devp);
	vdin_dv_tunnel_set(devp);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		vdin_set_bitdepth(devp);

	/* txl new add fix for hdmi switch resolution cause cpu holding */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)
		vdin_fix_nonstd_vsync(devp);

	/* continuous or sct memory;should be located before vdin_set_to_vpp_parm */
	vdin_mem_init(devp);

	/* reverse/non-reverse write buffer */
	vdin_wr_reverse(devp->addr_offset, devp->parm.h_reverse, devp->parm.v_reverse);

#ifdef CONFIG_CMA
	vdin_cma_malloc_mode(devp);
	if (vdin_cma_alloc(devp)) {
		pr_err("vdin%d %s fail for cma alloc fail!!!\n",
		       devp->index, __func__);
		return -1;
	}
#endif

	/* h_active/v_active will be used by bellow calling */
	if (canvas_config_mode == 1)
		vdin_canvas_start_config(devp);
	else if (canvas_config_mode == 2)
		vdin_canvas_auto_config(devp);

	if (devp->afbce_info && devp->afbce_valid) {
		vdin_afbce_maptable_init(devp);
		vdin_afbce_config(devp);
	}

	vdin_get_duration_by_fps(devp);
	devp->vfp->size = devp->vf_mem_max_cnt; /* canvas and afbce compatible */
	vf_pool_init(devp->vfp, devp->vfp->size);
	vdin_game_mode_check(devp);
	vdin_frame_lock_check(devp, 1);
	vdin_vf_init(devp);
	/* config dolby mem base */
	vdin_dolby_addr_alloc(devp, devp->vfp->size);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdin_is_dolby_signal_in(devp) &&
	    devp->index == devp->dv.dv_path_idx) {
		/* config dolby vision */
		vdin_dolby_config(devp);
		#ifndef VDIN_BRINGUP_NO_VF
		if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
			vf_reg_provider(&devp->dv.dv_vf_provider);
			pr_info("vdin%d provider: dv reg\n", devp->index);
				vf_notify_receiver(VDIN_DV_NAME,
				VFRAME_EVENT_PROVIDER_START, NULL);
		}
		#endif
	} else {
		/*disable dv mdata write*/
		vdin_dolby_mdata_write_en(devp->addr_offset, 0);
		#ifndef VDIN_BRINGUP_NO_VF
		if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
			vf_reg_provider(&devp->vf_provider);
			pr_info("vdin%d provider: reg\n", devp->index);
			vf_notify_receiver(devp->name,
					   VFRAME_EVENT_PROVIDER_START, NULL);
		}
		#endif
	}
#endif
	sts = vdin_write_done_check(devp->addr_offset, devp);

	devp->dv.chg_cnt = 0;
	devp->prop.hdr_info.hdr_check_cnt = 0;
	devp->vrr_data.vrr_chg_cnt = 0;
	devp->sg_chg_fps_cnt = 0;
	devp->vrr_data.cur_spd_data5 = devp->prop.spd_data.data[5];
	devp->last_wr_vfe = NULL;
	irq_max_count = 0;
	vdin_drop_cnt = 0;
	/* devp->stamp_valid = false; */
	devp->stamp = 0;
	devp->cycle = 0;
	devp->h_cnt64 = 0;

	memset(&devp->parm.histgram[0], 0, sizeof(unsigned short) * 64);

	devp->curr_field_type = vdin_get_curr_field_type(devp);
	devp->curr_dv_flag = devp->dv.dv_flag;
	/* configure regs and enable hw */
	vdin_vpu_clk_mem_pd(devp, 1);
	/*switch_vpu_mem_pd_vmod(devp->addr_offset ? VPU_VIU_VDIN1 :*/
	/*		       VPU_VIU_VDIN0,*/
	/*		       VPU_MEM_POWER_ON);*/

	if (vdin_sct_start(devp)) { /* request one scatter buffer before start */
		pr_err("%s vdin%d mem_type=%d,sct start failed!\n",
			__func__, devp->index, devp->mem_type);
		return -1;
	}

	vdin_set_all_regs(devp);
	vdin_hw_enable(devp);
	vdin_set_dv_tunnel(devp);
	vdin_write_mif_or_afbce_init(devp);
	/* screenshot stress test vdin1 hw crash addr need adjust config */
	vfe = provider_vf_peek(devp->vfp);
	if (vfe)
		vdin_frame_write_ctrl_set(devp, vfe, 0);
	else
		pr_info("vdin%d:peek first vframe fail\n", devp->index);
	//for debug
	vdin_dbg_access_reg(devp, 0);

	sts = vdin_is_delay_vfe2rd_list(devp);
	if (sts) {
		devp->vdin_delay_vfe2rd_list = vdin_delay_num;
	} else {
		devp->vdin_delay_vfe2rd_list = 0;
	}

	if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
	    devp->frontend &&
	    devp->frontend->dec_ops &&
	    devp->frontend->dec_ops->start &&
	    ((!IS_TVAFE_ATV_SRC(devp->parm.port)) ||
	     ((devp->flags & VDIN_FLAG_SNOW_FLAG) == 0)))
		devp->frontend->dec_ops->start(devp->frontend,
				devp->parm.info.fmt);
	else
		pr_info("vdin%d not do frontend start flags=0x%x parm.flag=0x%x\n",
			devp->index, devp->flags, devp->parm.flag);

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	/*it is better put after all reg init*/
	if (devp->rdma_enable && devp->rdma_handle > 0) {
		devp->flags |= VDIN_FLAG_RDMA_ENABLE;
		devp->flags_isr |= VDIN_FLAG_RDMA_DONE;
	} else {
		pr_info("vdin%d rdma enable:%d handle:%d\n",
			devp->index, devp->rdma_enable, devp->rdma_handle);
	}
#endif
	vdin_set_to_vpp_parm(devp);

	if (vdin_dbg_en)
		pr_info("****[%s]ok!****\n", __func__);
#ifdef CONFIG_AM_TIMESYNC
	if (devp->parm.port != TVIN_PORT_VIU1) {
		/*disable audio&video sync used for libplayer*/
		tsync_set_enable(0);
		/* enable system_time */
		timestamp_pcrscr_enable(1);
		pr_info("****[%s]disable tysnc& enable system time!****\n",
			__func__);
	}
#endif
	devp->irq_cnt = 0;
	devp->vpu_crash_cnt = 0;
	devp->rdma_irq_cnt = 0;
	devp->frame_cnt = 0;
	devp->put_frame_cnt = 0;
	phase_lock_flag = 0;
	devp->ignore_frames = max_ignore_frame_cnt;
	if (devp->parm.info.fps)
		devp->vs_time_stamp = (1000 * 50000) / devp->parm.info.fps;
	else
		devp->vs_time_stamp = sched_clock();
	devp->unreliable_vs_cnt = 0;
	devp->unreliable_vs_cnt_pre = 0;
	devp->unreliable_vs_idx = 0;
	devp->drop_hdr_set_sts = 3;
	vdin_isr_drop = vdin_isr_drop_num;
	if (devp->dv.dv_flag)
		color_range_force = COLOR_RANGE_AUTO;
	devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
	devp->chg_drop_frame_cnt = 0;
	devp->vrr_on_add_cnt = 0;
	devp->vrr_off_add_cnt = 0;

	devp->vdin_drop_ctl_cnt = 0;
	devp->af_num = VDIN_CANVAS_MAX_CNT;
	/* write vframe as default */
	devp->vframe_wr_en = 1;
	devp->vframe_wr_en_pre = 1;
	memset(&devp->stats, 0, sizeof(devp->stats));
	if (vdin_time_en)
		pr_info("vdin.%d start time: %ums, run time:%ums.\n",
			devp->index, jiffies_to_msecs(jiffies),
			jiffies_to_msecs(jiffies) - devp->start_time);

	return 0;
}

void vdin_self_start_dec(struct vdin_dev_s *devp)
{
	int ret;

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		pr_err("already flags\n");
		return;
	}

	if (vdin_dbg_en)
		pr_info("%s in\n", __func__);

	ret = vdin_start_dec(devp);
	if (ret) {
		pr_err("%s(%d) error, start dec fail\n",
			__func__, devp->index);
		return;
	}

	devp->flags |= VDIN_FLAG_DEC_STARTED;
	if (devp->parm.port != TVIN_PORT_VIU1 ||
	    viu_hw_irq != 0) {
		/*enable irq */
		enable_irq(devp->irq);
		devp->flags |= VDIN_FLAG_ISR_EN;
		vdin_calculate_isr_interval_value(devp); //init pre cur us value

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) &&
		    devp->index == 0 && devp->vpu_crash_irq > 0)
			enable_irq(devp->vpu_crash_irq);

		if (devp->wr_done_irq > 0)
			enable_irq(devp->wr_done_irq);

		/* for t7 dv meta data */
		if (/*vdin_is_dv_signal_in(devp) &&*/
		    /*devp->index == devp->dv.dv_path_idx &&*/
		    devp->vdin2_meta_wr_done_irq > 0) {
			enable_irq(devp->vdin2_meta_wr_done_irq);
			pr_info("enable meta wt done irq %d\n",
				devp->vdin2_meta_wr_done_irq);
		}
		if (vdin_dbg_en)
			pr_info("%s START_DEC vdin.%d enable_irq\n",
				__func__, devp->index);
	}

	if (vdin_dbg_en)
		pr_info("%s(%d) port %s, decode started ok flags=0x%x\n",
			__func__, devp->index,
			tvin_port_str(devp->parm.port), devp->flags);
}

/*
 * 1. disable irq.
 * 2. disable hw work, including:
 *		a. mux null input.
 *		b. set clock off.
 * 3. reset default canvas
 * 4.call the callback function of the frontend to stop vdin.
 */
void vdin_stop_dec(struct vdin_dev_s *devp)
{
	int afbc_write_down_timeout = 500; /* 50ms to cover a 24Hz vsync */
	int i = 0;

	/* avoid null pointer oops */
	if (!devp) {
		pr_info("vdin err no frontend\n");
		return;
	}

	if (devp->index == 0 && devp->dtdata->vdin0_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return;
	}

	if (devp->index == 1 && devp->dtdata->vdin1_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return;
	}

#ifdef CONFIG_CMA
	if (devp->cma_mem_alloc == 0 && devp->cma_config_en &&
		/* In vdin v4l2 mode,the process should goes down */
		devp->work_mode == VDIN_WORK_MD_NORMAL) {
		pr_info("%s:cma not alloc,don't need do others!\n", __func__);
		return;
	}
#endif
	//vdin_frame_lock_check(devp, 0);

	disable_irq(devp->irq);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) && devp->index == 0 &&
	    devp->vpu_crash_irq > 0)
		disable_irq(devp->vpu_crash_irq);

	if (devp->wr_done_irq > 0)
		disable_irq(devp->wr_done_irq);

	if (devp->vdin2_meta_wr_done_irq > 0)
		disable_irq(devp->vdin2_meta_wr_done_irq);

	devp->flags &= (~VDIN_FLAG_ISR_EN);

	pr_info("%s vdin%d,delay %u us before stop\n",
		__func__, devp->index, devp->dbg_stop_dec_delay);
	if (devp->dbg_stop_dec_delay) {
		usleep_range(devp->dbg_stop_dec_delay, devp->dbg_stop_dec_delay + 1000);
	}

	if (devp->afbce_mode == 1 || devp->double_wr) {
		while (i++ < afbc_write_down_timeout) {
			if (vdin_afbce_read_write_down_flag())
				break;
			usleep_range(100, 105);
		}
		if (i >= afbc_write_down_timeout) {
			pr_info("vdin.%d afbc write done timeout\n",
				devp->index);
		}
	}

	vdin_dump_frames(devp);

	if ((devp->vdin_function_sel & VDIN_SELF_STOP_START) &&
		devp->self_stop_start) {
		pr_info("linux system switch pc/game not need stop\n");
	} else {
		if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
		    devp->frontend &&
		    devp->frontend->dec_ops &&
		    devp->frontend->dec_ops->stop &&
		    ((!IS_TVAFE_ATV_SRC(devp->parm.port)) ||
		     ((devp->flags & VDIN_FLAG_SNOW_FLAG) == 0)))
			devp->frontend->dec_ops->stop(devp->frontend,
						devp->parm.port);
	}
	/* after T7 mif write vdin1 hw crash addr so vdin_hw_disable change to vdin_hw_close */
	//vdin_hw_disable(devp);
	vdin_hw_close(devp);

	vdin_set_default_regmap(devp);
	/*only for vdin0*/
	if (devp->dts_config.urgent_en && devp->index == 0)
		vdin_urgent_patch_resume(devp->addr_offset);

	/* reset default canvas  */
	vdin_set_def_wr_canvas(devp);

	if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
		if (devp->dv.dv_config && devp->index == devp->dv.dv_path_idx) {
			devp->dv.dv_config = 0;
			vf_unreg_provider(&devp->dv.dv_vf_provider);
			pr_info("vdin%d provider: dv unreg\n", devp->index);
		} else {
			vf_unreg_provider(&devp->vf_provider);
			pr_info("vdin%d provider: unreg\n", devp->index);
		}
	}
	vdin_dolby_addr_release(devp, devp->vfp->size);

	/*only when stop vdin0, disable afbc*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && devp->index == 0)
		vdin_afbce_soft_reset();

	vdin_mem_exit(devp);
#ifdef CONFIG_CMA
	vdin_cma_release(devp);
#endif
	/*switch_vpu_mem_pd_vmod(devp->addr_offset ? VPU_VIU_VDIN1 :*/
	/*		       VPU_VIU_VDIN0,*/
	/*		       VPU_MEM_POWER_DOWN);*/
	vdin_vpu_clk_mem_pd(devp, 0);
	//TBD: rdma_clear move to the below of disable_irq(devp->irq);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	rdma_clear(devp->rdma_handle);
#endif
	devp->flags &= (~VDIN_FLAG_RDMA_ENABLE);
	devp->ignore_frames = max_ignore_frame_cnt;
	devp->cycle = 0;
	/*reset csc_cfg in case it is enabled before switch out hdmi*/
	devp->csc_cfg = 0;
	devp->cut_window_cfg = 0;
	devp->frame_drop_num = 0;
	/*unreliable vsync interrupt check*/
	devp->unreliable_vs_cnt = 0;
	devp->unreliable_vs_cnt_pre = 0;
	devp->unreliable_vs_idx = 0;
	devp->prop.hdcp_sts = 0;
	devp->starting_chg = 0;
	devp->vdin_stable_cnt = 0;
	devp->game_mode = 0;
	devp->game_mode_pre = 0;
	devp->game_mode_bak = 0;
	devp->game_mode_chg = VDIN_GAME_MODE_UN_CHG;
	devp->common_divisor = 0;

	if ((devp->vdin_function_sel & VDIN_SELF_STOP_START) &&
		devp->self_stop_start) {
		//bypass not need handling
	} else {
		/* clear color para*/
		memset(&devp->pre_prop, 0, sizeof(devp->pre_prop));
		memset(&devp->prop, 0, sizeof(devp->prop));
	}

	if (vdin_time_en)
		pr_info("vdin.%d stop time %ums,run time:%ums.\n",
			devp->index, jiffies_to_msecs(jiffies),
			jiffies_to_msecs(jiffies) - devp->start_time);

	if (vdin_dbg_en)
		pr_info("%s ok\n", __func__);
}

void vdin_self_stop_dec(struct vdin_dev_s *devp)
{
	if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
		pr_err("%s(%d) decode haven't started flags=0x%x\n",
			__func__, devp->index, devp->flags);
		return;
	}

	if (vdin_dbg_en)
		pr_info("%s in\n", __func__);

	devp->flags |= VDIN_FLAG_DEC_STOP_ISR;

	vdin_stop_dec(devp);

	/* init flag */
	devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
	/* devp->flags &= ~VDIN_FLAG_FORCE_UNSTABLE; */
	/* clear the flag of decode started */
	devp->flags &= (~VDIN_FLAG_DEC_STARTED);
}

/*
 *config the vdin use default regmap
 *call vdin_start_dec to start vdin
 */
int start_tvin_service(int no, struct vdin_parm_s  *para)
{
	struct tvin_frontend_s *fe;
	int ret = 0;
	struct vdin_dev_s *devp = vdin_devp[no];
	struct vdin_dev_s *vdin0_devp = vdin_devp[0];
	enum tvin_sig_fmt_e fmt;

	if (IS_ERR_OR_NULL(devp)) {
		pr_err("[vdin]%s vdin%d hasn't registered\n",
		       __func__, no);
		return -1;
	}

	if (para->frame_rate == 0) {
		pr_err("%s vdin%d frame_rate is zero\n", __func__, no);
		return -1;
	}

	if (!IS_ERR_OR_NULL(vdin0_devp)) {
		vdin0_devp->pre_prop.hdcp_sts = vdin0_devp->prop.hdcp_sts;
		devp->matrix_pattern_mode = 0;
		/* check input content is protected */
		if ((vdin0_devp->flags & VDIN_FLAG_DEC_OPENED) &&
		    (vdin0_devp->flags & VDIN_FLAG_DEC_STARTED)) {
			if (vdin0_devp->prop.hdcp_sts && !devp->mem_protected) {
				pr_err("hdmi hdcp en, non-secure buffer\n");
				devp->matrix_pattern_mode = 4;
			} else {
				pr_err("non-hdcp or hdcp with secure buffer.sts:%d,flg:%d\n",
					vdin0_devp->prop.hdcp_sts, devp->mem_protected);
				devp->matrix_pattern_mode = 0;
			}
		} else {
			vdin0_devp->prop.hdcp_sts = 0;
			devp->matrix_pattern_mode = 0;
		}
		pr_info("vdin0 port:0x%x, flag:0x%x, hdcp sts:%d matrix:%d\n",
		       vdin0_devp->parm.port, vdin0_devp->flags,
		       vdin0_devp->prop.hdcp_sts, devp->matrix_pattern_mode);
	}

	mutex_lock(&devp->fe_lock);
	fmt = devp->parm.info.fmt;
	pr_info("[%s]port:0x%x, cfmt:%d;dfmt:%d;dest_h_active:%d;",
		__func__, para->port, para->cfmt,
		para->dfmt, para->dest_h_active);
	pr_info("dest_vactive:%d;frame_rate:%d;h_active:%d,",
		para->dest_v_active, para->frame_rate,
		para->h_active);
	pr_info("v_active:%d;scan_mode:%d,fmt:%#x**\n",
		para->v_active, para->scan_mode, fmt);

	if (devp->index == 1) {
		devp->parm.reserved |= para->reserved;

		/* for screen cap with panel reverse, vdin output non-reverse data */
		if (devp->hv_reverse_en && panel_reverse) {
			devp->parm.h_reverse = 1;
			devp->parm.v_reverse = 1;
		} else {
			devp->parm.h_reverse = 0;
			devp->parm.v_reverse = 0;
		}

		/*always update buf to avoid older data be captured*/
		if ((devp->parm.reserved & PARAM_STATE_SCREEN_CAP) ||
		    (devp->parm.reserved & PARAM_STATE_HISTGRAM))
			devp->flags |= VDIN_FLAG_FORCE_RECYCLE;
		else
			devp->flags &= ~VDIN_FLAG_FORCE_RECYCLE;

		pr_info("vdin1 add reserved = 0x%lx\n", para->reserved);
		pr_info("vdin1 all reserved = 0x%x\n", devp->parm.reserved);
	}

	devp->start_time = jiffies_to_msecs(jiffies);
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		pr_err("vdin%d %s: port 0x%x, decode started already.flags=0x%x\n",
		       devp->index, __func__, para->port, devp->flags);
		if ((devp->parm.reserved & PARAM_STATE_SCREEN_CAP) &&
			(devp->parm.reserved & PARAM_STATE_HISTGRAM) &&
			devp->index == 1) {
			mutex_unlock(&devp->fe_lock);
			return 0;
		} else {
			mutex_unlock(&devp->fe_lock);
			return -EBUSY;
		}
	}

	devp->parm.port = para->port;
	if (devp->parm.port == TVIN_PORT_VIU1_VIDEO)
		devp->flags |= VDIN_FLAG_V4L2_DEBUG;

	devp->parm.info.fmt = para->fmt;
	fmt = devp->parm.info.fmt;
	/* add for camera random resolution */
	if (para->fmt >= TVIN_SIG_FMT_MAX) {
		devp->fmt_info_p = kmalloc(sizeof(*devp->fmt_info_p),
					   GFP_KERNEL);
		if (!devp->fmt_info_p) {
			pr_err("[vdin]%s kmalloc error.\n", __func__);
			mutex_unlock(&devp->fe_lock);
			return -ENOMEM;
		}
		devp->fmt_info_p->hs_bp     = para->hs_bp;
		devp->fmt_info_p->vs_bp     = para->vs_bp;
		devp->fmt_info_p->hs_pol    = para->hsync_phase;
		devp->fmt_info_p->vs_pol    = para->vsync_phase;
		if ((para->h_active * para->v_active * para->frame_rate)
			> devp->vdin_max_pixel_clk)
			para->h_active >>= 1;
		devp->fmt_info_p->h_active  = para->h_active;
		devp->fmt_info_p->v_active  = para->v_active;
		if (devp->parm.port == TVIN_PORT_VIU1_VIDEO &&
		    (!(devp->flags & VDIN_FLAG_V4L2_DEBUG))) {
			devp->fmt_info_p->v_active =
				((rd(0, VPP_POSTBLEND_VD1_V_START_END) &
				0xfff) - ((rd(0, VPP_POSTBLEND_VD1_V_START_END)
				>> 16) & 0xfff) + 1);
			devp->fmt_info_p->h_active =
				((rd(0, VPP_POSTBLEND_VD1_H_START_END) &
				0xfff) - ((rd(0, VPP_POSTBLEND_VD1_H_START_END)
				>> 16) & 0xfff) + 1);
		}
		devp->fmt_info_p->scan_mode = para->scan_mode;
		devp->fmt_info_p->duration  = 96000 / para->frame_rate;
		devp->fmt_info_p->pixel_clk = para->h_active *
				para->v_active * para->frame_rate;
		devp->fmt_info_p->pixel_clk /= 10000;
	} else {
		devp->fmt_info_p = (struct tvin_format_s *)
				tvin_get_fmt_info(fmt);
		/* devp->fmt_info_p = tvin_get_fmt_info(devp->parm.info.fmt); */
	}
	if (!devp->fmt_info_p) {
		pr_err("%s(%d): error, fmt is null!!!\n", __func__, no);
		mutex_unlock(&devp->fe_lock);
		return -1;
	}

	if ((is_meson_gxbb_cpu()) && para->bt_path == BT_PATH_GPIO_B) {
		devp->bt_path = para->bt_path;
		fe = tvin_get_frontend(devp->parm.port, 1);
	} else {
		fe = tvin_get_frontend(devp->parm.port, 0);
	}

	if (fe) {
		fe->private_data = para;
		fe->port         = devp->parm.port;
		devp->frontend   = fe;

		/* vdin msr clock gate enable */
		if (devp->msr_clk)
			clk_prepare_enable(devp->msr_clk);

		if (fe->dec_ops->open && !(devp->flags & VDIN_FLAG_DEC_OPENED))
			fe->dec_ops->open(fe, fe->port);
	} else {
		pr_err("%s(%d): not supported port 0x%x\n",
		       __func__, no, devp->parm.port);
		mutex_unlock(&devp->fe_lock);
		return -1;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	/* config secure_en by loopback port */
	switch (devp->parm.port) {
	case TVIN_PORT_VIU1_VIDEO:
	case TVIN_PORT_VIU1_WB0_VD1:
	case TVIN_PORT_VIU1_WB1_VD1:
		devp->secure_en = get_secure_state(VD1_OUT);
		break;

	case TVIN_PORT_VIU1_WB0_VD2:
	case TVIN_PORT_VIU1_WB1_VD2:
		devp->secure_en = get_secure_state(VD2_OUT);
		break;

	case TVIN_PORT_VIU1_WB0_OSD1:
	case TVIN_PORT_VIU1_WB1_OSD1:
		devp->secure_en = get_secure_state(OSD1_VPP_OUT);
		break;

	case TVIN_PORT_VIU1_WB0_OSD2:
	case TVIN_PORT_VIU1_WB1_OSD2:
		devp->secure_en = get_secure_state(OSD2_VPP_OUT);
		break;

	case TVIN_PORT_VIU1_WB0_POST_BLEND:
	case TVIN_PORT_VIU1_WB1_POST_BLEND:
	case TVIN_PORT_VIU1_WB0_VPP:
	case TVIN_PORT_VIU1_WB1_VPP:
	case TVIN_PORT_VIU2_ENCL:
	case TVIN_PORT_VIU2_ENCI:
	case TVIN_PORT_VIU2_ENCP:
		devp->secure_en = get_secure_state(POST_BLEND_OUT);
		break;

	default:
		devp->secure_en = 0;
		break;
	}
#endif
	ret = vdin_start_dec(devp);
	if (ret) {
		pr_err("[vdin]%s kmalloc error.\n", __func__);
		mutex_unlock(&devp->fe_lock);
		return -1;
	}

	devp->flags |= VDIN_FLAG_DEC_OPENED;
	devp->flags |= VDIN_FLAG_DEC_STARTED;

	/* vdin bist pattern */
	if (devp->parm.port == TVIN_PORT_VIU1_WB1_VDIN_BIST)
		vdin_set_bist_pattern(devp, 1, 1);
	else
		vdin_set_bist_pattern(devp, 0, 1);

	/* for vdin0 loopback, already request irq in open */
	if (devp->index == 0 && (devp->flags & VDIN_FLAG_ISR_REQ)) {
		free_irq(devp->irq, (void *)devp);
		devp->flags &= ~VDIN_FLAG_ISR_REQ;
	}

	if (devp->parm.port != TVIN_PORT_VIU1 || viu_hw_irq != 0) {
		if (!(devp->flags & VDIN_FLAG_ISR_REQ)) {
			ret = request_irq(devp->irq, vdin_v4l2_isr, IRQF_SHARED,
				devp->irq_name, (void *)devp);
			devp->flags |= VDIN_FLAG_ISR_REQ;

			if (ret != 0) {
				pr_info("vdin_v4l2_isr request irq error.\n");
				mutex_unlock(&devp->fe_lock);
				return -1;
			}
			disable_irq(devp->irq);
			devp->flags &= ~VDIN_FLAG_ISR_EN;
			if (vdin_dbg_en)
				pr_info("%s vdin.%d request_irq\n", __func__,
					devp->index);
		}

		if (!(devp->flags & VDIN_FLAG_ISR_EN)) {
			/*enable irq */
			enable_irq(devp->irq);
			devp->flags |= VDIN_FLAG_ISR_EN;
			vdin_calculate_isr_interval_value(devp); //init pre cur us value
			if (vdin_dbg_en)
				pr_info("vdin.%d enable irq %d\n", devp->index,
					devp->irq);
		}
	}

	pr_info("vdin%d %s flags=0x%x\n", devp->index, __func__, devp->flags);
	mutex_unlock(&devp->fe_lock);
	return 0;
}
EXPORT_SYMBOL(start_tvin_service);

void vdin_set_black_pattern(bool mute)
{
	struct vdin_dev_s *devp = vdin_devp[0];

	if (mute)
		devp->matrix_pattern_mode = 5;
	else
		devp->matrix_pattern_mode = 0;
	vdin_set_matrix_color(devp);
}
EXPORT_SYMBOL(vdin_set_black_pattern);

/*
 *call vdin_stop_dec to stop the frontend
 *close frontend
 *free the memory allocated in start tvin service
 */
int stop_tvin_service(int no)
{
	int ret;
	struct vdin_dev_s *devp;
	unsigned int end_time;

	devp = vdin_devp[no];
	mutex_lock(&devp->fe_lock);

	if ((devp->parm.reserved & PARAM_STATE_HISTGRAM) &&
	    (devp->parm.reserved & PARAM_STATE_SCREEN_CAP) &&
	    devp->index == 1) {
		pr_info("stop vdin v4l2 screencap.\n");
		devp->parm.reserved &= ~PARAM_STATE_SCREEN_CAP;
		mutex_unlock(&devp->fe_lock);
		return 0;
	}

	if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
		pr_err("vdin%d %s:decode hasn't started flags=0x%x\n",
			devp->index, __func__, devp->flags);
		mutex_unlock(&devp->fe_lock);
		return -EBUSY;
	}
	devp->matrix_pattern_mode = 0;
	if (devp->set_canvas_manual && devp->mem_protected)
		devp->mem_protected = 0;
	vdin_stop_dec(devp);
	if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
		/*close fe*/
		if (devp->frontend && devp->frontend->dec_ops &&
			devp->frontend->dec_ops->close)
			devp->frontend->dec_ops->close(devp->frontend);
	}
	/*free the memory allocated in start tvin service*/
	if (devp->parm.info.fmt >= TVIN_SIG_FMT_MAX)
		kfree(devp->fmt_info_p);
/* #ifdef CONFIG_ARCH_MESON6 */
/* switch_mod_gate_by_name("vdin", 0); */
/* #endif */
	if (devp->parm.port == TVIN_PORT_VIU1_VIDEO)
		devp->flags &= (~VDIN_FLAG_V4L2_DEBUG);
	devp->flags &= (~VDIN_FLAG_DEC_OPENED);
	devp->flags &= (~VDIN_FLAG_DEC_STARTED);
	if (devp->parm.port != TVIN_PORT_VIU1 || viu_hw_irq != 0) {
		free_irq(devp->irq, (void *)devp);

		if (vdin_dbg_en)
			pr_info("%s vdin.%d free_irq\n", __func__,
				devp->index);
		devp->flags &= (~VDIN_FLAG_ISR_REQ);
	}
	/* request isr back for non-v4l2/v4l2 mode switch dynamically */
	if (devp->work_mode == VDIN_WORK_MD_V4L &&
		devp->index == 0 && !(devp->flags & VDIN_FLAG_ISR_REQ)) {
		/* request irq */
		if (work_mode_simple)
			ret = request_irq(devp->irq, vdin_isr_simple, IRQF_SHARED,
					  devp->irq_name, (void *)devp);
		else
			ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
					devp->irq_name, (void *)devp);
		if (ret)
			pr_err("err:req vs irq fail\n");
		else
			pr_info("vdin%d req vs irq %d\n", devp->index, devp->irq);
		disable_irq(devp->irq);
		devp->flags |= VDIN_FLAG_ISR_REQ;
		devp->flags &= (~VDIN_FLAG_ISR_EN);
	}

	end_time = jiffies_to_msecs(jiffies);
	if (vdin_time_en)
		pr_info("[vdin]:vdin start time:%ums,stop time:%ums,run time:%u.\n",
			devp->start_time,
			end_time,
			end_time - devp->start_time);
	pr_info("vdin%d %s flags=0x%x\n", devp->index, __func__, devp->flags);
	mutex_unlock(&devp->fe_lock);
	return 0;
}

/*
 * dev_num: 0:vdin0 1:vdin1
 * port 0: capture_osd_plus_video = 0
 * port 1: capture_only_video
 */
int start_tvin_capture_ex(int dev_num, enum port_vpp_e port, struct vdin_parm_s  *para)
{
	unsigned int loop_port;
	unsigned int vdin_dev;

	if (!para) {
		pr_err("%s vdin%d port=0x%x,para == NULL!\n", __func__, dev_num, port);
		return -1;
	}

	if (vdin_dbg_en)
		pr_err("%s vdin%d port=0x%x,0x%x!\n", __func__, dev_num, port, para->port);

	if (port == PORT_VPP0_VIDEO_ONLY)		/* vpp0 */
		loop_port = TVIN_PORT_VIU1_WB0_VD1;
	else if (port == PORT_VPP0_OSD_VIDEO)
		loop_port = TVIN_PORT_VIU1_WB0_VPP;
	else if (port == PORT_VPP0_OSD1_ONLY)
		loop_port = TVIN_PORT_VIU1_WB0_OSD1;
	else if (port == PORT_VPP0_OSD2_ONLY)
		loop_port = TVIN_PORT_VIU1_WB0_OSD2;
	else if (port == PORT_VPP0_VIDEO_ONLY_NO_PQ)
		loop_port = TVIN_PORT_VIU1_VIDEO;
	else if (port == PORT_VPP1_VIDEO_ONLY)		/* vpp1 */
		loop_port = TVIN_PORT_VIU2_VD1;
	else if (port == PORT_VPP1_OSD_VIDEO)
		loop_port = TVIN_PORT_VIU2_VPP;
	else if (port == PORT_VPP1_OSD1_ONLY)
		loop_port = TVIN_PORT_VIU2_OSD1;
	else if (port == PORT_VPP2_VIDEO_ONLY)		/* vpp2 */
		loop_port = TVIN_PORT_VIU3_VD1;
	else if (port == PORT_VPP2_OSD_VIDEO)
		loop_port = TVIN_PORT_VIU3_VPP;
	else if (port == PORT_VPP2_OSD1_ONLY)
		loop_port = TVIN_PORT_VIU3_OSD1;
	else
		loop_port = para->port;

	/* For chips before T7,only vpp0 + VIU2 ENCP/I/L */
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		(para->port > TVIN_PORT_VIU2 && para->port < TVIN_PORT_VIU3_MAX)) {
		loop_port = TVIN_PORT_VIU2_ENCL;
	}

	para->port = loop_port;

	if (dev_num == 0)
		vdin_dev = 0;
	else
		vdin_dev = 1;
	return start_tvin_service(vdin_dev, para);
}

void get_tvin_canvas_info(int *start, int *num)
{
	*start = vdin_canvas_ids[0][0];
	*num = vdin_devp[0]->canvas_max_num;
}
EXPORT_SYMBOL(get_tvin_canvas_info);

static int vdin_ioctl_fe(int no, struct fe_arg_s *parm)
{
	struct vdin_dev_s *devp = vdin_devp[no];
	int ret = 0;

	if (IS_ERR_OR_NULL(devp)) {
		pr_err("[vdin]%s vdin%d hasn't registered or devp is NULL\n",
		       __func__, no);
		return -1;
	}
	if (devp->frontend &&
		devp->frontend->dec_ops &&
		devp->frontend->dec_ops->ioctl)
		ret = devp->frontend->dec_ops->ioctl(devp->frontend, parm->arg);
	else if (!devp->frontend) {
		devp->frontend = tvin_get_frontend(parm->port, parm->index);
		if (devp->frontend &&
			devp->frontend->dec_ops &&
			devp->frontend->dec_ops->ioctl)
			ret = devp->frontend->dec_ops->ioctl(devp->frontend,
					parm->arg);
	}
	return ret;
}

/*
 * if parm.port is TVIN_PORT_VIU1,call vdin_v4l2_isr
 *	vdin_v4l2_isr is used to the sample
 *	v4l2 application such as camera,viu
 */
static void vdin_rdma_isr(struct vdin_dev_s *devp)
{
	if (devp->parm.port == TVIN_PORT_VIU1)
		vdin_v4l2_isr(devp->irq, devp);
}

/*based on parameter
 *parm->cmd :
 *	VDIN_CMD_SET_CSC
 *	VDIN_CMD_SET_CM2
 *	VDIN_CMD_ISR
 *	VDIN_CMD_MPEGIN_START
 *	VDIN_CMD_GET_HISTGRAM
 *	VDIN_CMD_MPEGIN_STOP
 *	VDIN_CMD_FORCE_GO_FIELD
 */
static int vdin_func(int no, struct vdin_arg_s *arg)
{
	struct vdin_dev_s *devp = vdin_devp[no];
	int ret = 0;
	struct vdin_arg_s *parm = NULL;
	struct vframe_s *vf = NULL;

	parm = arg;
	if (IS_ERR_OR_NULL(devp)) {
		if (vdin_dbg_en)
			pr_err("[vdin]%s vdin%d hasn't registered,please register.\n",
			       __func__, no);
		return -1;
	} else if (!(devp->flags & VDIN_FLAG_DEC_STARTED) &&
		   (parm->cmd != VDIN_CMD_MPEGIN_START)) {
		return -1;
	}
	if (vdin_dbg_en)
		pr_info("[vdin_drv]%s vdin%d:parm->cmd : %d\n",
			__func__, no, parm->cmd);
	switch (parm->cmd) {
	/*adjust vdin1 matrix1 & matrix2 for isp to get histogram information*/
	case VDIN_CMD_SET_CSC:
		vdin_select_matrix(devp, parm->matrix_id, parm->color_convert);
		break;
	case VDIN_CMD_SET_CM2:
		vdin_set_cm2(devp->addr_offset, devp->h_active,
			     devp->v_active, parm->cm2);
		break;
	case VDIN_CMD_ISR:
		vdin_rdma_isr(devp);
		break;
	case VDIN_CMD_MPEGIN_START:
		devp->h_active = parm->h_active;
		devp->v_active = parm->v_active;
		/*switch_vpu_mem_pd_vmod(devp->addr_offset ? VPU_VIU_VDIN1 :*/
		/*		       VPU_VIU_VDIN0, VPU_MEM_POWER_ON);*/
		vdin_vpu_clk_mem_pd(devp, 1);
		vdin_set_mpegin(devp);
		pr_info("%s:VDIN_CMD_MPEGIN_START :h_active:%d,v_active:%d\n",
			__func__, devp->h_active, devp->v_active);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			devp->curr_wr_vfe = kmalloc(sizeof(*devp->curr_wr_vfe),
						    GFP_KERNEL);
			devp->flags |= VDIN_FLAG_DEC_STARTED;
		} else {
			pr_info("%s: VDIN_CMD_MPEGIN_START already !!\n",
				__func__);
		}
		break;
	case VDIN_CMD_GET_HISTGRAM:
		if (!devp->curr_wr_vfe) {
			if (vdin_dbg_en)
				pr_info("VDIN_CMD_GET_HISTGRAM:curr_wr_vfe is NULL !!!\n");
			break;
		}
		vdin_set_vframe_prop_info(&devp->curr_wr_vfe->vf, devp);
		vdin_backup_histgram(&devp->curr_wr_vfe->vf, devp);
		vf = (struct vframe_s *)parm->private;
		if (vf)
			memcpy(&vf->prop, &devp->curr_wr_vfe->vf.prop,
			       sizeof(struct vframe_prop_s));
		break;
	case VDIN_CMD_MPEGIN_STOP:
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_info("%s:warning:VDIN_CMD_MPEGIN_STOP\n",
				__func__);
			/*switch_vpu_mem_pd_vmod(devp->addr_offset ?*/
			/*		       VPU_VIU_VDIN1 :*/
			/*		       VPU_VIU_VDIN0,*/
			/*		       VPU_MEM_POWER_DOWN);*/
			vdin_vpu_clk_mem_pd(devp, 0);
			vdin_hw_disable(devp);
			kfree(devp->curr_wr_vfe);
			devp->curr_wr_vfe = NULL;
			devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		} else {
			pr_info("%s:warning:VDIN_CMD_MPEGIN_STOP already\n",
				__func__);
		}
		break;
	case VDIN_CMD_FORCE_GO_FIELD:
		vdin_force_go_filed(devp);
		break;
	default:
		break;
	}
	return ret;
}

static struct vdin_v4l2_ops_s vdin_4v4l2_ops = {
	.start_tvin_service   = start_tvin_service,
	.stop_tvin_service    = stop_tvin_service,
	.get_tvin_canvas_info = get_tvin_canvas_info,
	.set_tvin_canvas_info = NULL,
	.tvin_fe_func         = vdin_ioctl_fe,
	.tvin_vdin_func	      = vdin_func,
	.start_tvin_service_ex = start_tvin_capture_ex,
};

/* call vdin_pause_dec to pause hw
 * type:
 *	1: pause mif
 *	2: pause afbce
 *	other value: pause mif and afbce
 */
void vdin_pause_dec(struct vdin_dev_s *devp, unsigned int type)
{
	if (type == 1)
		devp->debug.pause_mif_dec = true;
	else if (type == 2)
		devp->debug.pause_afbce_dec = true;
	else
		devp->pause_dec = 1;
}

/* call vdin_resume_dec to resume hw
 * type:
 *	1: resume mif
 *	2: resume afbce
 *	other value: resume mif and afbce
 */
void vdin_resume_dec(struct vdin_dev_s *devp, unsigned int type)
{
	if (type == 1)
		devp->debug.pause_mif_dec = false;
	else if (type == 2)
		devp->debug.pause_afbce_dec = false;
	else
		devp->pause_dec = 0;
}

/*register provider & notify receiver */
void vdin_vf_reg(struct vdin_dev_s *devp)
{
	if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
		vf_reg_provider(&devp->vf_provider);
		vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_START, NULL);
	}
}

/*unregister provider*/
void vdin_vf_unreg(struct vdin_dev_s *devp)
{
	vf_unreg_provider(&devp->vf_provider);
}

/*
 * set vframe_view base on parameters:
 * left_eye, right_eye, parm.info.trans_fmt
 * set view:
 * start_x, start_y, width, height
 */
inline void vdin_set_view(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	struct vframe_view_s *left_eye, *right_eye;
	enum tvin_sig_fmt_e fmt = devp->parm.info.fmt;
	const struct tvin_format_s *fmt_info = tvin_get_fmt_info(fmt);

	if (!fmt_info) {
		pr_err("[tvafe..] %s: error,fmt is null!!!\n", __func__);
		return;
	}

	left_eye  = &vf->left_eye;
	right_eye = &vf->right_eye;

	memset(left_eye, 0, sizeof(struct vframe_view_s));
	memset(right_eye, 0, sizeof(struct vframe_view_s));

	switch (devp->parm.info.trans_fmt) {
	case TVIN_TFMT_3D_LRH_OLOR:
	case TVIN_TFMT_3D_LRH_OLER:
		left_eye->width  = devp->h_active >> 1;
		left_eye->height	  = devp->v_active;
		right_eye->start_x   = devp->h_active >> 1;
		right_eye->width	  = devp->h_active >> 1;
		right_eye->height	  = devp->v_active;
		break;
	case TVIN_TFMT_3D_TB:
		left_eye->width  = devp->h_active;
		left_eye->height	  = devp->v_active >> 1;
		right_eye->start_y   = devp->v_active >> 1;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = devp->v_active >> 1;
		break;
	case TVIN_TFMT_3D_FP: {
		unsigned int vactive = 0;
		unsigned int vspace = 0;
		struct vf_entry *slave = NULL;

		vspace  = fmt_info->vs_front + fmt_info->vs_width +
				fmt_info->vs_bp;
		if (devp->parm.info.fmt ==
		    TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING ||
		    devp->parm.info.fmt ==
		    TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING) {
			vactive = (fmt_info->v_active - vspace -
					vspace - vspace + 1) >> 2;
			slave = vf_get_slave(devp->vfp, vf->index);
			if (!slave)
				break;
			slave->vf.left_eye.start_x  = 0;
			slave->vf.left_eye.start_y  = vactive + vspace +
					vactive + vspace - 1;
			slave->vf.left_eye.width	 = devp->h_active;
			slave->vf.left_eye.height	 = vactive;
			slave->vf.right_eye.start_x = 0;
			slave->vf.right_eye.start_y = vactive + vspace +
					vactive + vspace + vactive + vspace - 1;
			slave->vf.right_eye.width	 = devp->h_active;
			slave->vf.right_eye.height  = vactive;
		} else {
			vactive = (fmt_info->v_active - vspace) >> 1;
		}
		left_eye->width  = devp->h_active;
		left_eye->height	  = vactive;
		right_eye->start_y   = vactive + vspace;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = vactive;
		break;
	}
	case TVIN_TFMT_3D_FA: {
		unsigned int vactive = 0;
		unsigned int vspace = 0;

		vspace = fmt_info->vs_front + fmt_info->vs_width +
				fmt_info->vs_bp;
		vactive = (fmt_info->v_active - vspace + 1) >> 1;
		left_eye->width	  = devp->h_active;
		left_eye->height	  = vactive;
		right_eye->start_y   = vactive + vspace;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = vactive;
		break;
	}
	case TVIN_TFMT_3D_LA: {
		left_eye->width	  = devp->h_active;
		left_eye->height	  = (devp->v_active) >> 1;
		right_eye->width	  = devp->h_active;
		right_eye->height	  = (devp->v_active) >> 1;
		break;
	}
	default:
		break;
	}
}

/*function:
 *	get current field type
 *	set canvas addr
 *	disable hw when irq_max_count >= canvas_max_num
 */
irqreturn_t vdin_isr_simple(int irq, void *dev_id)
{
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	struct tvin_decoder_ops_s *dec_ops;
	unsigned int last_field_type;

	if (irq_max_count >= devp->canvas_max_num) {
		vdin_hw_disable(devp);
		return IRQ_HANDLED;
	}

	last_field_type = devp->curr_field_type;
	devp->curr_field_type = vdin_get_curr_field_type(devp);

	if (devp->frontend) {
		dec_ops = devp->frontend->dec_ops;
		if (dec_ops->decode_isr(devp->frontend,
				       vdin_get_meas_h_cnt64(devp->addr_offset)) == -1)
			return IRQ_HANDLED;
	}

	/* set canvas address */
	vdin_set_canvas_id(devp, devp->flags & VDIN_FLAG_RDMA_ENABLE, NULL);
	if (vdin_dbg_en)
		pr_info("%2d: canvas id %d, field type %s\n",
			irq_max_count,
			vdin_canvas_ids[devp->index][irq_max_count],
			((last_field_type & VIDTYPE_TYPEMASK) ==
			  VIDTYPE_INTERLACE_TOP ? "top" : "bottom"));

	irq_max_count++;
	if (irq_max_count >= VDIN_CANVAS_MAX_CNT)
		irq_max_count = 0;
	return IRQ_HANDLED;
}

static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp)
{
	unsigned int i = 0;

	devp->parm.hist_pow = vf->prop.hist.hist_pow;
	devp->parm.luma_sum = vf->prop.hist.luma_sum;
	devp->parm.pixel_sum = vf->prop.hist.pixel_sum;
	for (i = 0; i < 64; i++)
		devp->parm.histgram[i] = vf->prop.hist.gamma[i];
}

static u64 func_div(u64 cur, u32 dividend)
{
	do_div(cur, dividend);
	return cur;
}

static void vdin_hist_tgt(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	int ave_luma;
	int pix_sum;
	ulong flags;
	unsigned int i = 0;

	spin_lock_irqsave(&devp->hist_lock, flags);
	vdin1_hist.sum    = vf->prop.hist.luma_sum;
	pix_sum = vf->prop.hist.pixel_sum;
	vdin1_hist.height = vf->prop.hist.height;
	vdin1_hist.width =  vf->prop.hist.width;

	if (vdin1_hist.height == 0 || vdin1_hist.width == 0) {
		spin_unlock_irqrestore(&devp->hist_lock, flags);
		return;
	}

	vdin1_hist.ave =
		div_u64(vdin1_hist.sum, (vdin1_hist.height * vdin1_hist.width));

	ave_luma = vdin1_hist.ave;
	ave_luma = (ave_luma - 16) < 0 ? 0 : (ave_luma - 16);
	vdin1_hist.ave = ave_luma * 255 / (235 - 16);
	if (vdin1_hist.ave > 255)
		vdin1_hist.ave = 255;

	for (i = 0; i < 64; i++)
		vdin1_hist.hist[i] = vf->prop.hist.gamma[i];

	spin_unlock_irqrestore(&devp->hist_lock, flags);
}

void vdin_drop_frame_info(struct vdin_dev_s *devp, char *info)
{
	if (skip_frame_debug)
		pr_info("vdin%d[%d %d]:irq_flag:%d %s [%d %d],WL:%x WR:%x RL:%x RM:%x\n",
			devp->index, devp->irq_cnt, devp->frame_cnt, devp->vdin_irq_flag, info,
			devp->irq_cnt, devp->frame_cnt,
			devp->vfp->wr_list_size, devp->vfp->wr_mode_size,
			devp->vfp->rd_list_size, devp->vfp->rd_mode_size);
}

/* 0:check pass;others:check fail */
int vdin_vs_duration_check(struct vdin_dev_s *devp)
{
	int ret = 0;
	int cur_time, diff_time;
	int temp;

	if (devp->game_mode || !IS_HDMI_SRC(devp->parm.port))
		return ret;

	cur_time = devp->cycle;
	temp = cur_time - devp->vs_time_stamp;
	diff_time = abs(temp);
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_VS)
		pr_info("isr:diff_time:%d, cycle:%d\n",
			diff_time, devp->cycle);

	/* In a duration 50M clk theory value */
	if (devp->parm.info.fps)
		devp->vs_time_stamp = (1000 * 50000) / devp->parm.info.fps;
	else
		devp->vs_time_stamp = cur_time;

	if (diff_time > 0x186a0) {
		temp = diff_time;
		if (devp->unreliable_vs_idx >= 10)
			devp->unreliable_vs_idx = 0;
		devp->unreliable_vs_time[devp->unreliable_vs_idx] = temp;
		devp->unreliable_vs_idx++;
		ret = -1;
	}

	if (!(is_meson_t7_cpu() || is_meson_t3_cpu() ||
	     is_meson_t5w_cpu()))
		ret = 0;

	return ret;
}

/*
 * put one frame out
 * param: devp
 *	vfe
 * return:
 *	0:put one frame
 *	-1:recycled one frame
 */
int vdin_vframe_put_and_recycle(struct vdin_dev_s *devp, struct vf_entry *vfe,
				  enum vdin_vf_put_md work_md)
{
	int ret = 0;
	int ret_v4l = 0;
	enum vdin_vf_put_md md = work_md;
	struct vf_entry *vfe_tmp;

	/*for debug*/
	if (vdin_frame_work_mode == VDIN_VF_RECYCLE)
		md = VDIN_VF_RECYCLE;

	/*force recycle one frame*/
	if (devp->frame_cnt <= devp->vdin_drop_num) {
		if (vfe)
			receiver_vf_put(&vfe->vf, devp->vfp);
		ret = -1;
	} else if (devp->chg_drop_frame_cnt > 0) {
		if (vfe)
			receiver_vf_put(&vfe->vf, devp->vfp);
		devp->chg_drop_frame_cnt--;
		ret = -1;
	} else if (devp->dbg_fr_ctl > 1 && (devp->frame_cnt % devp->dbg_fr_ctl) != 0) {
		if (vfe)
			receiver_vf_put(&vfe->vf, devp->vfp);
		devp->vdin_drop_ctl_cnt++;
		ret = -1;
	} else if (md == VDIN_VF_RECYCLE) {
		vfe_tmp = receiver_vf_get(devp->vfp);
		if (vfe_tmp)
			receiver_vf_put(&vfe_tmp->vf, devp->vfp);

		if (vfe)
			receiver_vf_put(&vfe->vf, devp->vfp);
		ret = -1;
	} else if (vfe) {
		if (devp->vdin_delay_vfe2rd_list == 0) {
			devp->vfp->last_last_vfe = vfe;
		} else if (devp->vdin_delay_vfe2rd_list == 1 &&
		    !devp->vfp->last_last_vfe) {
			devp->vfp->last_last_vfe = vfe;
			return ret;
		} else if ((devp->vdin_delay_vfe2rd_list == 2) &&
			   (!devp->vfp->last_vfe ||
			   !devp->vfp->last_last_vfe)) {
			devp->vfp->last_last_vfe = devp->vfp->last_vfe;
			devp->vfp->last_vfe = vfe;
			return ret;
		}

		/*put one frame to receiver*/
		if (devp->work_mode == VDIN_WORK_MD_V4L) {
			ret_v4l = vdin_v4l2_if_isr(devp,
					&devp->vfp->last_last_vfe->vf);
			/*v4l put fail, need recycle vframe to write list*/
			if (ret_v4l) {
				receiver_vf_put(&devp->vfp->last_last_vfe->vf,
						devp->vfp);
				return -1;
			}
			devp->put_frame_cnt++;
		} else if (devp->debug.vdin_recycle_num) {
			/* debug for dump more mem or drop frame */
			receiver_vf_put(&devp->vfp->last_last_vfe->vf, devp->vfp);
			devp->put_frame_cnt++;
			devp->debug.vdin_recycle_num--;
		} else {
			provider_vf_put(devp->vfp->last_last_vfe, devp->vfp);
			devp->put_frame_cnt++;
		}

		devp->vfp->last_last_vfe->vf.ready_clock[1] = sched_clock();

		if (vdin_time_en)
			pr_info("vdin.%d put latency %lld us.first %lld us\n",
				devp->index,
			func_div(devp->vfp->last_last_vfe->vf.ready_clock[1],
				 1000),
			func_div(devp->vfp->last_last_vfe->vf.ready_clock[0],
				 1000));

		if (vdin_isr_monitor & VDIN_ISR_MONITOR_VF)
			pr_info("vdin%d cnt:%d vf:%d sg_type:%#x type:%#x flag:%u dur:%u disp:%d\n",
				devp->index, devp->irq_cnt, devp->vfp->last_last_vfe->vf.index,
				devp->vfp->last_last_vfe->vf.signal_type,
				devp->vfp->last_last_vfe->vf.type,
				devp->vfp->last_last_vfe->vf.flag,
				devp->vfp->last_last_vfe->vf.duration,
				devp->vfp->last_last_vfe->vf.index_disp);

		if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (vdin_is_dolby_signal_in(devp) &&
			    devp->dv.dv_config)
				vf_notify_receiver(VDIN_DV_NAME,
						   VFRAME_EVENT_PROVIDER_VFRAME_READY,
						   NULL);
			else
			#endif
				vf_notify_receiver(devp->name,
						   VFRAME_EVENT_PROVIDER_VFRAME_READY,
						   NULL);
		}
		/* debug:afbce compression ratio monitor start */
		if (devp->dbg_afbce_monitor)
			vdin_afbce_compression_ratio_monitor(devp,
				devp->vfp->last_last_vfe);
		/* debug:afbce compression ratio monitor end */
		if (devp->vdin_delay_vfe2rd_list == 1) {
			devp->vfp->last_last_vfe = vfe;
		} else if (devp->vdin_delay_vfe2rd_list == 2) {
			devp->vfp->last_last_vfe = devp->vfp->last_vfe;
			devp->vfp->last_vfe = vfe;
		}
	}
	vdin_sct_read_mmu_num(devp, vfe);

	return ret;
}

irqreturn_t vdin_write_done_isr(int irq, void *dev_id)
{
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	bool sts;

	/*need clear write done flag*/
	sts = vdin_write_done_check(devp->addr_offset, devp);

	/*write done flag VDIN_RO_WRMIF_STATUS bit 0*/
	devp->stats.wr_done_irq_cnt++;
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_WRITE_DONE)
		pr_info("wr_done_irq_cnt:%d\n",
			devp->stats.wr_done_irq_cnt);

	return IRQ_HANDLED;
}

irqreturn_t vpu_crash_isr(int irq, void *dev_id)
{
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;

	devp->vpu_crash_cnt++;
	pr_info("vpu crash happened:%d\n", devp->vpu_crash_cnt);
	return IRQ_HANDLED;
}

void vdin_frame_write_ctrl_set(struct vdin_dev_s *devp,
				struct vf_entry *vfe, bool rdma_en)
{
	if (devp->afbce_mode == 0 || devp->double_wr) {
		/* prepare for chroma canvas*/
		if (vfe->vf.plane_num == 2)
			vdin_set_chroma_canvas_id(devp, rdma_en, vfe);

		if (devp->dtdata->hw_ver >= VDIN_HW_T7)
			vdin_set_frame_mif_write_addr(devp, rdma_en, vfe);
		vdin_set_canvas_id(devp, rdma_en, vfe);
	}

	if (devp->afbce_mode == 1 || devp->double_wr)
		vdin_afbce_set_next_frame(devp, rdma_en, vfe);
}

void vdin_pause_hw_write(struct vdin_dev_s *devp, bool rdma_en)
{
	vdin_pause_mif_write(devp, rdma_en);

	if (devp->afbce_mode == 1 || devp->double_wr)
		vdin_pause_afbce_write(devp, rdma_en);

	devp->chg_drop_frame_cnt = 2; //2 is drop abnormal frame
}

static inline void vdin_dynamic_switch_vrr(struct vdin_dev_s *devp)
{
	bool is_freesync = 0, is_vrr = 0;

	is_vrr = devp->prop.vtem_data.vrr_en;
	is_freesync = (vdin_check_is_spd_data(devp) &&
			(devp->prop.spd_data.data[5] >> 1 & 0x7));

	if (vdin_isr_monitor & VDIN_ISR_MONITOR_GAME)
		pr_info("%s %d,frame_lock_on:%d,vrr:%d,freesync:%d,game:%d\n",
			__func__, __LINE__, devp->vrr_data.frame_lock_vrr_en,
			is_vrr, is_freesync, devp->game_mode);

	/* FRAME_LOCK_EVENT_OFF */
	if (!devp->vrr_data.frame_lock_vrr_en) {
		if ((is_vrr || is_freesync) && devp->game_mode) {
			devp->vrr_on_add_cnt++;
			devp->vrr_off_add_cnt = 0;
		} else {
			devp->vrr_on_add_cnt = 0;
		}
		if (devp->vrr_on_add_cnt > vdin_vrr_switch_cnt) {
			vdin_frame_lock_check(devp, 1);
			devp->vrr_on_add_cnt = 0;
			devp->vrr_off_add_cnt = 0;
		}
	} else {/* FRAME_LOCK_EVENT_ON */
		if (!(is_vrr || is_freesync)) {
			devp->vrr_off_add_cnt++;
			devp->vrr_on_add_cnt = 0;
		} else {
			devp->vrr_off_add_cnt = 0;
		}
		if (devp->vrr_off_add_cnt > vdin_vrr_switch_cnt) {
			vdin_frame_lock_check(devp, 1);
			devp->vrr_on_add_cnt = 0;
			devp->vrr_off_add_cnt = 0;
		}
	}
}

static void vdin_set_vfe_info(struct vdin_dev_s *devp, struct vf_entry *vfe)
{
	vfe->vf.type = vdin_get_curr_field_type(devp);
	vfe->vf.type_original = vfe->vf.type;
	/* debug for video latency */
	vfe->vf.ready_jiffies64 = jiffies_64;
	vfe->vf.ready_clock[0] = sched_clock();

	if (devp->game_mode)
		vfe->vf.flag |= VFRAME_FLAG_GAME_MODE;
	else
		vfe->vf.flag &= ~VFRAME_FLAG_GAME_MODE;

	if (devp->prop.latency.allm_mode)
		vfe->vf.flag |= VFRAME_FLAG_ALLM_MODE;
	else
		vfe->vf.flag &= ~VFRAME_FLAG_ALLM_MODE;

	if (devp->mem_type == VDIN_MEM_TYPE_SCT)
		vfe->vf.type_ext = VIDTYPE_EXT_VDIN_SCATTER;
	else
		vfe->vf.type_ext &= ~VIDTYPE_EXT_VDIN_SCATTER;
}

static void vdin_set_one_buffer_mode(struct vdin_dev_s *devp, struct vf_entry *next_wr_vfe)
{
	struct vf_entry *master = NULL;

	/* vrr lock,and game mode 2, enter one buffer mode */
	if ((devp->game_mode & VDIN_GAME_MODE_2) &&
		devp->dbg_force_one_buffer != 2  &&
		(devp->vrr_data.vrr_mode || devp->dbg_force_one_buffer)) {
		/* all vf will use this phy address of current next_wr_vfe */
		if (devp->af_num >= VDIN_CANVAS_MAX_CNT) {
			devp->af_num = next_wr_vfe->af_num;
			next_wr_vfe->flag |= VF_FLAG_ONE_BUFFER_MODE;
			return;
		}

		if (!(next_wr_vfe->flag & VF_FLAG_ONE_BUFFER_MODE)) {
			next_wr_vfe->flag |= VF_FLAG_ONE_BUFFER_MODE;
			master = vf_get_master(devp->vfp, devp->af_num);
			if (!master) {
				pr_err("ERR:vdin%d,af_num=%d\n",
					devp->index, devp->af_num);
				return;
			}
			/* mif:use address the first time enter one buffer mode */
			next_wr_vfe->vf.canvas0_config[0].phy_addr =
				master->vf.canvas0_config[0].phy_addr;
			next_wr_vfe->vf.canvas0_config[1].phy_addr =
				master->vf.canvas0_config[1].phy_addr;
			/* afbce:update address in vdin_afbce_set_next_frame */
		}
	} else {
		devp->af_num = VDIN_CANVAS_MAX_CNT;
		/* exiting one buffer mode, recovery to the initial phy address */
		if (next_wr_vfe->flag & VF_FLAG_ONE_BUFFER_MODE) {
			next_wr_vfe->flag &= (~VF_FLAG_ONE_BUFFER_MODE);
			/* mif */
			next_wr_vfe->vf.canvas0_config[0].phy_addr = next_wr_vfe->phy_y_addr_bak;
			next_wr_vfe->vf.canvas0_config[1].phy_addr = next_wr_vfe->phy_c_addr_bak;
			/* afbce:update address in vdin_afbce_set_next_frame */
		}
	}
}

/*
 *VDIN_FLAG_RDMA_ENABLE=1
 *	provider_vf_put(devp->last_wr_vfe, devp->vfp);
 *VDIN_FLAG_RDMA_ENABLE=0
 *	provider_vf_put(curr_wr_vfe, devp->vfp);
 */
irqreturn_t vdin_isr(int irq, void *dev_id)
{
	ulong flags = 0;
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	enum tvin_sm_status_e state;
	struct vf_entry *next_wr_vfe = NULL;
	struct vf_entry *curr_wr_vfe = NULL;
	struct vframe_s *curr_wr_vf = NULL;
	unsigned int last_field_type;
	unsigned int last_dv_flag;
	struct tvin_decoder_ops_s *dec_ops;
	unsigned int stamp = 0;
	struct tvin_state_machine_ops_s *sm_ops;
	int vdin2nr = 0;
	unsigned int offset = 0, vf_drop_cnt = 0;
	enum tvin_trans_fmt trans_fmt;
	struct tvin_sig_property_s *prop, *pre_prop;
	/*long long *clk_array;*/
	long long vlock_delay_jiffies, vlock_t1;
	enum vdin_vf_put_md put_md = VDIN_VF_PUT;
	int ret;

	/* avoid null pointer oops */
	if (!devp)
		return IRQ_HANDLED;

	devp->irq_cnt++;

	if (vdin_isr_drop) {
		vdin_isr_drop--;
		vdin_drop_frame_info(devp, "drop debug");
		return IRQ_HANDLED;
	}

	if (!devp->frontend) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_END;
		vdin_drop_frame_info(devp, "no frontend");
		return IRQ_HANDLED;
	}

	if (vdin_check_cycle(devp)) {
		vdin_drop_frame_info(devp, "cycle error");
		vdin_pause_hw_write(devp, 0);
		return IRQ_HANDLED;
	}

	/* debug interrupt interval time
	 *
	 * this code about system time must be outside of spinlock.
	 * because the spinlock may affect the system time.
	 */
	if (vdin_vs_duration_check(devp) < 0) {
		vdin_drop_frame_info(devp, "duration error");
		vdin_pause_hw_write(devp, 0);
		return IRQ_HANDLED;
	}

	if (devp->dbg_force_stop_frame_num &&
		devp->frame_cnt > devp->dbg_force_stop_frame_num) {
		vdin_drop_frame_info(devp, "force stop");
		return IRQ_HANDLED;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_amdv_certification())
		vdin_set_crc_pulse(devp);
#endif

	sm_ops = devp->frontend->sm_ops;

	if (sm_ops && sm_ops->get_sig_property) {
		if (vdin_get_prop_in_vs_en) {
			sm_ops->get_sig_property(devp->frontend, &devp->prop);
			vdin_get_base_fr(devp);
			if (vdin_isr_monitor & VDIN_ISR_MONITOR_VRR_DATA)
				pr_info("vdin vrr_en:%d spd:%d %d\n",
					devp->prop.vtem_data.vrr_en,
					devp->prop.spd_data.data[0],
					devp->prop.spd_data.data[5]);
		}
		vdin_vs_proc_monitor(devp);
		if (devp->prop.vdin_hdr_flag &&
		    devp->prop.hdr_info.hdr_state == HDR_STATE_SET &&
		    devp->drop_hdr_set_sts) {
			devp->drop_hdr_set_sts--;
			vdin_drop_frame_info(devp, "hdr set status");
			return IRQ_HANDLED;
		}
		devp->drop_hdr_set_sts = 0;
	}

	vdin_dynamic_switch_vrr(devp);

	/* ignore fake irq caused by sw reset*/
	if (devp->vdin_reset_flag) {
		devp->vdin_reset_flag = 0;
		devp->vdin_irq_flag = VDIN_IRQ_FLG_FAKE_IRQ;
		vdin_drop_frame_info(devp, "reset_flag");
		return IRQ_HANDLED;
	}

	vf_drop_cnt = vdin_drop_cnt;
	offset = devp->addr_offset;
	vdin_set_mif_on_off(devp, devp->flags & VDIN_FLAG_RDMA_ENABLE);
	isr_log(devp->vfp);

	/* 10 is prevent abnormal frame */
	if (devp->starting_chg && devp->irq_cnt < 10) {
		vdin_drop_frame_info(devp, "starting chg");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&devp->isr_lock, flags);

	if (devp->dv.chg_cnt || devp->dv.allm_chg_cnt) {
		if (devp->game_mode)
			vdin_pause_hw_write(devp, devp->flags & VDIN_FLAG_RDMA_ENABLE);
		vdin_drop_frame_info(devp, "dv or allm chg");
		vdin_vf_skip_all_disp(devp->vfp);
		vdin_drop_cnt++;
		goto irq_handled;
	}

	if (devp->afbce_mode == 1) {
		/* no need reset mif under afbc mode */
		devp->vdin_reset_flag = 0;
	} else {
		/* W_VCBUS_BIT(VDIN_MISC_CTRL, 0, 0, 2); */
		devp->vdin_reset_flag = vdin_vsync_reset_mif(devp->index);
	}
	if ((devp->flags & VDIN_FLAG_DEC_STOP_ISR) &&
	    (!(vdin_isr_flag & VDIN_BYPASS_STOP_CHECK))) {
		vdin_hw_close(devp);
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		devp->vdin_irq_flag = VDIN_IRQ_FLG_IRQ_STOP;
		vdin_drop_frame_info(devp, "stop isr");
		goto irq_handled;
	}

	if (devp->pause_num > 0 && devp->frame_cnt >= devp->pause_num) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_BUFF_SKIP;
		vdin_drop_frame_info(devp, "pause hw");
		vdin_pause_dec(devp, 0);
	}

	if (devp->frame_drop_num) {
		devp->frame_drop_num--;
		devp->vdin_irq_flag = VDIN_IRQ_FLG_DROP_FRAME;
		vdin_drop_frame_info(devp, "drop frame");
		vdin_drop_cnt++;
		goto irq_handled;
	}
	stamp = vdin_get_meas_v_stamp(offset);
	vdin_handle_game_mode_chg(devp);

	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (devp->curr_wr_vfe) {
			devp->curr_wr_vfe->vf.ready_jiffies64 = jiffies_64;
			devp->curr_wr_vfe->vf.ready_clock[0] = sched_clock();
		}

		if (devp->curr_wr_vfe && devp->afbce_mode == 1)
			vdin_afbce_set_next_frame(devp, (devp->flags &
						  VDIN_FLAG_RDMA_ENABLE),
						  devp->curr_wr_vfe);
		/*save the first field stamp*/
		devp->stamp = stamp;
		devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_WR_FE;
		vdin_drop_frame_info(devp, "no wr vfe");
		goto irq_handled;
	}

	/* use RDMA and not game mode */
	if (devp->last_wr_vfe && (devp->flags & VDIN_FLAG_RDMA_ENABLE) &&
	    !(devp->game_mode & VDIN_GAME_MODE_1) &&
	    !(devp->game_mode & VDIN_GAME_MODE_2)) {
		/* get drm and update prop info */
		if (devp->debug.change_get_drm == SEND_LAST_FRAME_GET_PROP) {
			vdin_set_drm_data(devp, &devp->last_wr_vfe->vf);
			vdin_set_vframe_prop_info(&devp->last_wr_vfe->vf, devp);
			vdin_set_freesync_data(devp, &devp->last_wr_vfe->vf);
		}
		/*dolby vision metadata process*/
		if (dv_dbg_mask & DV_UPDATE_DATA_MODE_DOLBY_WORK &&
		    devp->dv.dv_config) {
			/* prepare for dolby vision metadata addr */
			devp->dv.dv_cur_index = devp->last_wr_vfe->vf.index;
			devp->dv.dv_next_index = devp->curr_wr_vfe->vf.index;
			schedule_delayed_work(&devp->dv.dv_dwork,
				dv_work_dolby);
		} else if (vdin_dv_is_visf_data(devp)) {
			vdin_dolby_buffer_update(devp,
						 devp->last_wr_vfe->vf.index);
			vdin_dolby_addr_update(devp,
					       devp->curr_wr_vfe->vf.index);
		} else {
			devp->dv.dv_crc_check = true;
		}

		if (devp->dv.low_latency != devp->vfp->low_latency)
			devp->vfp->low_latency = devp->dv.low_latency;

		memcpy(&devp->vfp->dv_vsif,
			&devp->dv.dv_vsif, sizeof(struct tvin_dv_vsif_s));
		/*dv mode always put vframe, if cec check, vpp set video mute*/
		if (dv_dbg_mask & DV_CRC_FORCE_TRUE)
			devp->last_wr_vfe->vf.dv_crc_sts = true;
		else if (dv_dbg_mask & DV_CRC_FORCE_FALSE)
			devp->last_wr_vfe->vf.dv_crc_sts = false;
		else
			devp->last_wr_vfe->vf.dv_crc_sts =
				devp->dv.dv_crc_check;

		/*skip policy process*/
		if (devp->vfp->skip_vf_num > 0)
			vdin_vf_disp_mode_update(devp->last_wr_vfe, devp->vfp);

		vdin_vframe_put_and_recycle(devp, devp->last_wr_vfe, put_md);

		devp->last_wr_vfe = NULL;

		/* debug: force display to skip frame */
		if (devp->force_disp_skip_num &&
			devp->frame_cnt <= devp->force_disp_skip_num) {
			vdin_drop_cnt++;
			vdin_drop_frame_info(devp, "force disp skip");
		}
		/* debug: end */
	}
	/*check vs is valid base on the time during continuous vs*/
	/* if (vdin_check_cycle(devp) &&
	 *     (!(vdin_isr_flag & VDIN_BYPASS_CYC_CHECK))
	 *	&& (!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
	 *	devp->vdin_irq_flag = VDIN_IRQ_FLG_CYCLE_CHK;
	 *	vdin_drop_frame_info(devp, "cycle chk");
	 *	vdin_drop_cnt++;
	 *	goto irq_handled;
	 *}
	 */

	devp->h_cnt64 = vdin_get_meas_h_cnt64(offset);
	last_field_type = devp->curr_field_type;
	last_dv_flag = devp->curr_dv_flag;
	devp->curr_field_type = vdin_get_curr_field_type(devp);
	devp->curr_dv_flag = devp->dv.dv_flag;

	if (devp->vdin_function_sel & VDIN_ADJUST_VLOCK) {
		if (devp->duration) {
			vlock_delay_jiffies = func_div(96000, devp->duration);
			vlock_t1 = func_div(HZ, vlock_delay_jiffies);
			vlock_delay_jiffies = func_div(vlock_t1, 4);
		} else {
			vlock_delay_jiffies = msecs_to_jiffies(5);
		}
		schedule_delayed_work(&devp->vlock_dwork, vlock_delay_jiffies);
	}

	/* ignore the unstable signal */
	state = tvin_get_sm_status(devp->index);
	if ((devp->parm.info.status != TVIN_SIG_STATUS_STABLE ||
	     state != TVIN_SM_STATUS_STABLE) &&
	    (!(devp->flags & VDIN_FLAG_SNOW_FLAG))) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_SIG_NOT_STABLE;
		vdin_pause_hw_write(devp, devp->flags & VDIN_FLAG_RDMA_ENABLE);
		vdin_drop_frame_info(devp, "sig not stable");
		vdin_drop_cnt++;
		goto irq_handled;
	}

	/* for 3D mode & interlaced format,
	 *  give up bottom field to avoid odd/even phase different
	 */
	trans_fmt = devp->parm.info.trans_fmt;
	if (vdin_is_3d_interlace_signal(devp) &&
	    ((last_field_type & VIDTYPE_INTERLACE_BOTTOM) ==
	     VIDTYPE_INTERLACE_BOTTOM)) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_FMT_TRANS_CHG;
		vdin_drop_frame_info(devp, "tran fmt chg");
		goto irq_handled;
	}
	/* Check whether frame written done */
	if (devp->dts_config.chk_write_done_en && !devp->dbg_no_wr_check) {
		if (!vdin_write_done_check(offset, devp)) {
			devp->vdin_irq_flag = VDIN_IRQ_FLG_SKIP_FRAME;
			vdin_drop_frame_info(devp, "write done check");
			vdin_drop_cnt++;
		}
	}

	curr_wr_vfe = devp->curr_wr_vfe;
	curr_wr_vf  = &curr_wr_vfe->vf;

	next_wr_vfe = provider_vf_peek(devp->vfp);

	/* change afbce mode */
	if (next_wr_vfe && devp->afbce_mode_pre != devp->afbce_mode)
		vdin_afbce_mode_update(devp);

	/* change color matrix */
	if (devp->csc_cfg != 0) {
		prop = &devp->prop;
		pre_prop = &devp->pre_prop;
		if (prop->color_format != pre_prop->color_format ||
		    prop->vdin_hdr_flag != pre_prop->vdin_hdr_flag ||
		    prop->color_fmt_range != pre_prop->color_fmt_range) {
			vdin_set_matrix(devp);
			/*vdin_drop_frame_info(devp, "color fmt chg");*/
		}
		if (prop->color_format != pre_prop->color_format ||
		    prop->vdin_hdr_flag != pre_prop->vdin_hdr_flag) {
			vdin_set_bitdepth(devp);
			vdin_source_bitdepth_reinit(devp);
			vdin_set_wr_ctrl_vsync(devp, devp->addr_offset,
				devp->format_convert,
				devp->full_pack, devp->source_bitdepth,
				devp->flags & VDIN_FLAG_RDMA_ENABLE);
			vdin_set_top(devp, devp->addr_offset, devp->parm.port,
				     devp->prop.color_format, devp->h_active,
				     devp->bt_path);

			if (devp->afbce_valid)
				vdin_afbce_update(devp);

			/*vdin_drop_frame_info(devp, "dest fmt chg");*/
		}
		pre_prop->dest_cfmt = prop->dest_cfmt;
		if (prop->color_format != pre_prop->color_format ||
		    prop->vdin_hdr_flag != pre_prop->vdin_hdr_flag) {
			pre_prop->vdin_hdr_flag = prop->vdin_hdr_flag;
			pre_prop->color_format = prop->color_format;
			pre_prop->color_fmt_range = prop->color_fmt_range;
			devp->csc_cfg = 0;
			devp->ignore_frames = 0;
			devp->vdin_irq_flag = VDIN_IRQ_FLG_CSC_CHG;
			vdin_drop_frame_info(devp, "csc chg");
			vdin_drop_cnt++;
			goto irq_handled;
		} else {
			pre_prop->vdin_hdr_flag = prop->vdin_hdr_flag;
			pre_prop->color_format = prop->color_format;
			pre_prop->color_fmt_range = prop->color_fmt_range;
			devp->csc_cfg = 0;
		}
	}
	/* change cut window */
	if (devp->cut_window_cfg != 0 && devp->auto_cut_window_en == 1) {
		prop = &devp->prop;
		if (prop->pre_vs || prop->pre_ve)
			devp->v_active += (prop->pre_vs + prop->pre_ve);
		if (prop->pre_hs || prop->pre_he)
			devp->h_active += (prop->pre_hs + prop->pre_he);
		vdin_set_cutwin(devp);
		prop->pre_vs = prop->vs;
		prop->pre_ve = prop->ve;
		prop->pre_hs = prop->hs;
		prop->pre_he = prop->he;
		devp->cut_window_cfg = 0;
	}
	if (devp->auto_cut_window_en == 1 && IS_TVAFE_SRC(devp->parm.port))
		curr_wr_vf->width = devp->h_active;

	dec_ops = devp->frontend->dec_ops;
	if (dec_ops->decode_isr(devp->frontend, devp->h_cnt64) == TVIN_BUF_SKIP) {
		if (devp->game_mode)
			vdin_pause_hw_write(devp, devp->flags & VDIN_FLAG_RDMA_ENABLE);
		devp->vdin_irq_flag = VDIN_IRQ_FLG_BUFF_SKIP;
		vdin_drop_frame_info(devp, "buf skip flg");
		vdin_drop_cnt++;
		goto irq_handled;
	}
	if (IS_TVAFE_SRC(devp->parm.port))
		curr_wr_vf->phase = sm_ops->get_secam_phase(devp->frontend) ?
				VFRAME_PHASE_DB : VFRAME_PHASE_DR;

	if (devp->ignore_frames < max_ignore_frame_cnt) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_IGNORE_FRAME;
		vdin_drop_frame_info(devp, "ignore frame flag");
		devp->ignore_frames++;
		vdin_drop_cnt++;

		if (devp->afbce_mode == 1)
			vdin_afbce_set_next_frame(devp, (devp->flags &
						  VDIN_FLAG_RDMA_ENABLE),
						  curr_wr_vfe);

		goto irq_handled;
	}

	if (sm_ops->check_frame_skip &&
	    sm_ops->check_frame_skip(devp->frontend)) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_SKIP_FRAME;
		vdin_drop_frame_info(devp, "skip frame flag");
		vdin_drop_cnt++;
		if (devp->flags & VDIN_FLAG_RDMA_ENABLE)
			devp->ignore_frames = 0;
		goto irq_handled;
	}

	if (!next_wr_vfe) {
		/*add for force vdin buffer recycle*/
		if (devp->flags & VDIN_FLAG_FORCE_RECYCLE) {
			vdin_vframe_put_and_recycle(devp, next_wr_vfe,
						    VDIN_VF_RECYCLE);
		} else {
			devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_NEXT_FE;
			vdin_drop_frame_info(devp, "no next wr vfe-1");
			/* I need drop two frame */
			if ((devp->curr_field_type & VIDTYPE_INTERLACE)
				== VIDTYPE_INTERLACE) {
				devp->frame_drop_num = 1;
				/*devp->interlace_force_drop = 1;*/
				vdin_drop_frame_info(devp, "interlace drop");
			}
			/* vdin_drop_cnt++; no need skip frame,only drop one */
			goto irq_handled;
		}
	}
	if (devp->mem_type == VDIN_MEM_TYPE_SCT && next_wr_vfe) {
		if (next_wr_vfe->sct_stat != VFRAME_SCT_STATE_FULL) {
			devp->msct_top.sct_pause_dec = true;
			devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_NEXT_FE;
			vdin_drop_frame_info(devp, "sct no next wr vfe");
			goto irq_handled;
		} else if (devp->msct_top.sct_pause_dec) {
			devp->msct_top.sct_pause_dec = false;
		}
	}

	/* prepare for next input data */
	next_wr_vfe = provider_vf_get(devp->vfp);
	if (!next_wr_vfe) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_NEXT_FE;
		vdin_drop_frame_info(devp, "no next wr vfe-2");
		/* vdin_drop_cnt++; no need skip frame,only drop one */
		goto irq_handled;
	}

	vdin_set_one_buffer_mode(devp, next_wr_vfe);

	if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (vdin_is_dolby_signal_in(devp) && devp->dv.dv_config)
			vdin2nr =
				vf_notify_receiver(VDIN_DV_NAME,
						   VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR,
						   NULL);
		else
#endif
			vdin2nr =
				vf_notify_receiver(devp->name,
						   VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR,
						   NULL);
	}
	/*if vdin-nr,di must get
	 * vdin current field type which di pre will read
	 */
	if (vdin2nr || (devp->flags & VDIN_FLAG_RDMA_ENABLE)) {
		curr_wr_vf->type = devp->curr_field_type;
		curr_wr_vf->dv_input = devp->curr_dv_flag;
	} else {
		curr_wr_vf->type = last_field_type;
		curr_wr_vf->dv_input = last_dv_flag;
	}

	/* for 2D->3D mode or hdmi 3d mode& interlaced format,
	 *  fill-in as progressive format
	 */
	if (((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) ||
	     curr_wr_vf->trans_fmt) &&
	    (last_field_type & VIDTYPE_INTERLACE)) {
		curr_wr_vf->type &= ~VIDTYPE_INTERLACE_TOP;
		curr_wr_vf->type |=  VIDTYPE_PROGRESSIVE;
		curr_wr_vf->type |=  VIDTYPE_PRE_INTERLACE;
	}
	curr_wr_vf->type_original = curr_wr_vf->type;

	//2 is get prop when send frame
	if (devp->debug.change_get_drm != SEND_LAST_FRAME_GET_PROP ||
	    devp->game_mode & VDIN_GAME_MODE_1 ||
	    devp->game_mode & VDIN_GAME_MODE_2) {
		vdin_set_drm_data(devp, curr_wr_vf);
		vdin_set_vframe_prop_info(curr_wr_vf, devp);
		vdin_set_freesync_data(devp, curr_wr_vf);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_amdv_certification()) {
		vdin_get_crc_val(curr_wr_vf, devp);
		/*pr_info("vdin_isr get vf %p, crc %x\n", curr_wr_vf, curr_wr_vf->crc);*/
	}
#endif
	vdin_backup_histgram(curr_wr_vf, devp);
	#ifdef CONFIG_AML_LOCAL_DIMMING
	/*vdin_backup_histgram_ldim(curr_wr_vf, devp);*/
	#endif
	if (devp->parm.port >= TVIN_PORT_HDMI0 &&
	    devp->parm.port <= TVIN_PORT_HDMI7) {
		curr_wr_vf->trans_fmt = devp->parm.info.trans_fmt;
		vdin_set_view(devp, curr_wr_vf);
	}
	vdin_calculate_duration(devp);
	/*curr_wr_vf->duration = devp->duration;*/

	/* put for receiver
	 * ppmgr had handled master and slave vf by itself,
	 * vdin do not to declare them respectively
	 * ppmgr put the vf that included master vf and slave vf
	 */
	/*config height according to 3d mode dynamically*/
	if ((devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
	     !(devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
	     devp->parm.info.fmt != TVIN_SIG_FMT_NULL) ||
	    IS_TVAFE_ATV_SRC(devp->parm.port)) {
		curr_wr_vf->height = devp->v_active << 1;
	} else {
		if (devp->vf_mem_size_small)
			curr_wr_vf->height = devp->v_shrink_out;
		else
			curr_wr_vf->height = devp->v_active;
	}
	/*new add for atv snow:avoid flick black on bottom when atv search*/
	if (IS_TVAFE_ATV_SRC(devp->parm.port) &&
	    (devp->flags & VDIN_FLAG_SNOW_FLAG))
		curr_wr_vf->height = 480;
	curr_wr_vfe->flag |= VF_FLAG_NORMAL_FRAME;
	if (IS_TVAFE_SRC(devp->parm.port) ||
	    ((devp->vdin_function_sel & VDIN_SET_DISPLAY_RATIO) &&
	     (IS_TVAFE_SRC(devp->parm.port) || IS_HDMI_SRC(devp->parm.port))))
		vdin_set_display_ratio(devp, curr_wr_vf);

	if ((devp->flags & VDIN_FLAG_RDMA_ENABLE) &&
	    !(devp->game_mode & VDIN_GAME_MODE_1)) {
		devp->last_wr_vfe = curr_wr_vfe;
	} else if (!(devp->game_mode & VDIN_GAME_MODE_2)) {
		if (devp->vdin_function_sel & VDIN_PROP_RX_UPDATE)
			devp->last_wr_vfe = curr_wr_vfe;
		/*dolby vision metadata process*/
		if ((dv_dbg_mask & DV_UPDATE_DATA_MODE_DOLBY_WORK) &&
		    devp->dv.dv_config) {
			/* prepare for dolby vision metadata addr */
			devp->dv.dv_cur_index = curr_wr_vfe->vf.index;
			devp->dv.dv_next_index = next_wr_vfe->vf.index;
			schedule_delayed_work(&devp->dv.dv_dwork,
					      dv_work_dolby);
		} else if (vdin_dv_is_visf_data(devp)) {
			vdin_dolby_buffer_update(devp, curr_wr_vfe->vf.index);
			vdin_dolby_addr_update(devp, next_wr_vfe->vf.index);
		} else {
			devp->dv.dv_crc_check = true;
		}

		/*dv mode always put vframe, if cec check, vpp set video mute*/
		if (dv_dbg_mask & DV_CRC_FORCE_TRUE)
			curr_wr_vfe->vf.dv_crc_sts = true;
		else if (dv_dbg_mask & DV_CRC_FORCE_FALSE)
			curr_wr_vfe->vf.dv_crc_sts = false;
		else
			curr_wr_vfe->vf.dv_crc_sts =
				devp->dv.dv_crc_check;

		/*skip policy process*/
		if (devp->vfp->skip_vf_num > 0)
			vdin_vf_disp_mode_update(curr_wr_vfe, devp->vfp);

		vdin_vframe_put_and_recycle(devp, curr_wr_vfe, put_md);
	}

	vdin_frame_write_ctrl_set(devp, next_wr_vfe,
				  devp->flags & VDIN_FLAG_RDMA_ENABLE);

	devp->curr_wr_vfe = next_wr_vfe;
	vdin_set_vfe_info(devp, next_wr_vfe);

	if (!(devp->flags & VDIN_FLAG_RDMA_ENABLE) ||
	    (devp->game_mode & VDIN_GAME_MODE_1)) {
		if (devp->work_mode == VDIN_WORK_MD_NORMAL) {
			/* not RDMA, or game mode 1 */
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (vdin_is_dolby_signal_in(devp) &&
			    devp->dv.dv_config)
				vf_notify_receiver(VDIN_DV_NAME,
						   VFRAME_EVENT_PROVIDER_VFRAME_READY,
						   NULL);
			else
			#endif
				vf_notify_receiver(devp->name,
						   VFRAME_EVENT_PROVIDER_VFRAME_READY,
						   NULL);
		}
	} else if (devp->game_mode & VDIN_GAME_MODE_2) {
		/* game mode 2 */
		vdin_vframe_put_and_recycle(devp, next_wr_vfe, put_md);
	}

	vdin_game_mode_transfer(devp);
	devp->frame_cnt++;

irq_handled:
	//for debug
	vdin_dbg_access_reg(devp, 1);
	/*hdmi skip policy should adapt to all drop front vframe case*/
	if (devp->vfp->skip_vf_num > 0 &&
	    vf_drop_cnt < vdin_drop_cnt)
		vdin_vf_disp_mode_skip(devp->vfp);

	spin_unlock_irqrestore(&devp->isr_lock, flags);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if ((devp->flags & VDIN_FLAG_RDMA_ENABLE) &&
	    (devp->flags_isr & VDIN_FLAG_RDMA_DONE)) {
		ret = rdma_config(devp->rdma_handle,
				  (devp->rdma_enable & 1) ?
				  devp->rdma_irq : RDMA_TRIGGER_MANUAL);
		if (ret == 0)
			devp->flags_isr |= VDIN_FLAG_RDMA_DONE;
		else
			devp->flags_isr &= ~VDIN_FLAG_RDMA_DONE;
	}
#endif
	vdin_sct_queue_work(devp);

	isr_log(devp->vfp);

	if (vdin_isr_monitor & VDIN_ISR_MONITOR_BUFFER)
		pr_info("vdin.%d: frame_cnt:%d wr_list:%d wr_mode:%d rd_list:%d rd_mode:%d\n",
			devp->index, devp->frame_cnt, devp->vfp->wr_list_size,
			devp->vfp->wr_mode_size, devp->vfp->rd_list_size,
			devp->vfp->rd_mode_size);

	return IRQ_HANDLED;
}

/*
 * there are too much logic in vdin_isr which is useless in camera&viu
 * so vdin_v4l2_isr use to the sample v4l2 application such as camera,viu
 */
/*static unsigned short skip_ratio = 1;*/
/*module_param(skip_ratio, ushort, 0664);*/
/*MODULE_PARM_DESC(skip_ratio,*/
/*		 "\n vdin skip frame ratio 1/ratio will reserved.\n");*/

static struct vf_entry *check_vdin_read_list(struct vdin_dev_s *devp)
{
	struct vf_entry *vfe;

	vfe = receiver_vf_peek(devp->vfp);

	return vfe;
}

irqreturn_t vdin_v4l2_isr(int irq, void *dev_id)
{
	ulong flags;
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	struct vdin_dev_s *vdin0_devp = vdin_devp[0];
	enum vdin_vf_put_md put_md = VDIN_VF_PUT;
	struct vf_entry *next_wr_vfe = NULL, *curr_wr_vfe = NULL;
	struct vframe_s *curr_wr_vf = NULL;
	unsigned int last_field_type, stamp;
	struct tvin_decoder_ops_s *dec_ops;
	struct tvin_state_machine_ops_s *sm_ops;
	int ret = 0;
	unsigned int offset;
	unsigned int protect_mode;

	if (!devp)
		return IRQ_HANDLED;
	if (devp->vdin_reset_flag) {
		devp->vdin_reset_flag = 0;
		return IRQ_HANDLED;
	}
	if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
		return IRQ_HANDLED;

	isr_log(devp->vfp);
	devp->irq_cnt++;
	if (vdin_isr_drop) {
		vdin_isr_drop--;
		vdin_drop_frame_info(devp, "drop debug");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&devp->isr_lock, flags);

	if (devp->frame_drop_num) {
		devp->frame_drop_num--;
		devp->vdin_irq_flag = VDIN_IRQ_FLG_DROP_FRAME;
		vdin_drop_frame_info(devp, "drop frame");
		goto irq_handled;
	}

	if (vdin_get_active_h(devp->addr_offset) < VDIN_INPUT_DATA_THRESHOLD ||
	    vdin_get_active_v(devp->addr_offset) < VDIN_INPUT_DATA_THRESHOLD) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_FAKE_IRQ;
		vdin_drop_frame_info(devp, "no data input");
		if (devp->vdin_function_sel & VDIN_NOT_DATA_INPUT_DROP)
			goto irq_handled;
	}

	/* protect mem will fail sometimes due to no res from tee module */
	if (devp->secure_en && !devp->mem_protected && !devp->set_canvas_manual) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_SECURE_MD;
		vdin_drop_frame_info(devp, "secure mode without protect mem");
		goto irq_handled;
	}

	/* set CRC check pulse */
	vdin_set_crc_pulse(devp);
	devp->vdin_reset_flag = vdin_vsync_reset_mif(devp->index);
	offset = devp->addr_offset;
	vdin_set_mif_on_off(devp, devp->flags & VDIN_FLAG_RDMA_ENABLE);

	if (devp)
		/* avoid null pointer oops */
		stamp  = vdin_get_meas_v_stamp(offset);

	if (vdin0_devp) { /* May not probe vdin0 */
		/* check input content is protected */
		protect_mode = vdin0_devp->prop.hdcp_sts ? 4 : 0;
		if (protect_mode != devp->matrix_pattern_mode && !devp->mem_protected &&
			(vdin0_devp->flags & VDIN_FLAG_DEC_OPENED) &&
			(vdin0_devp->flags & VDIN_FLAG_DEC_STARTED)) {
			devp->matrix_pattern_mode = protect_mode;
			pr_info("vdin0:hdcp chg to %d\n", vdin0_devp->prop.hdcp_sts);
			vdin_set_matrix(devp);
		}
	}

	/* if win_size changed for video only */
	if (!(devp->flags & VDIN_FLAG_V4L2_DEBUG))
		vdin_set_wr_mif(devp);
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);

		if (devp->curr_wr_vfe) {
			devp->curr_wr_vfe->vf.ready_jiffies64 = jiffies_64;
			devp->curr_wr_vfe->vf.ready_clock[0] = sched_clock();
		}

		/*save the first field stamp*/
		devp->stamp = stamp;
		devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_WR_FE;
		vdin_drop_frame_info(devp, "no wr vfe");
		goto irq_handled;
	}

	if (devp->parm.port == TVIN_PORT_VIU1 ||
	    devp->parm.port == TVIN_PORT_CAMERA) {
		if (!vdin_write_done_check(offset, devp)) {
			if (vdin_dbg_en)
				pr_info("[vdin.%u] write undone skiped.\n",
						devp->index);
			devp->vdin_irq_flag = VDIN_IRQ_FLG_SKIP_FRAME;
			vdin_drop_frame_info(devp, "write done check");
			goto irq_handled;
		}
	}

	if (devp->set_canvas_manual == 1 && check_vdin_read_list(devp)) {
		devp->keystone_vframe_ready = 1;
		wake_up_interruptible(&vframe_waitq);
	}

	next_wr_vfe = provider_vf_peek(devp->vfp);

	if (devp->last_wr_vfe) {
		/*add for force vdin buffer recycle*/
		if (!next_wr_vfe &&
		    (devp->flags & VDIN_FLAG_FORCE_RECYCLE))
			put_md = VDIN_VF_RECYCLE;
		else
			put_md = VDIN_VF_PUT;

		vdin_vframe_put_and_recycle(devp, devp->last_wr_vfe,
					    put_md);
		devp->last_wr_vfe = NULL;

		if (devp->set_canvas_manual != 1 &&
			devp->work_mode != VDIN_WORK_MD_V4L) {
			vf_notify_receiver(devp->name,
					   VFRAME_EVENT_PROVIDER_VFRAME_READY,
					   NULL);
		}
	}

	/*check vs is valid base on the time during continuous vs*/
	vdin_check_cycle(devp);

	/* ignore invalid vs base on the continuous fields
	 * different cnt to void screen flicker
	 */

	last_field_type = devp->curr_field_type;
	devp->curr_field_type = vdin_get_curr_field_type(devp);

	curr_wr_vfe = devp->curr_wr_vfe;
	curr_wr_vf  = &curr_wr_vfe->vf;

	curr_wr_vf->type = last_field_type;
	curr_wr_vf->type_original = curr_wr_vf->type;

	vdin_set_vframe_prop_info(curr_wr_vf, devp);
	vdin_get_crc_val(curr_wr_vf, devp);
	vdin_backup_histgram(curr_wr_vf, devp);
	vdin_hist_tgt(devp, curr_wr_vf);

	if (devp->frontend && devp->frontend->dec_ops) {
		dec_ops = devp->frontend->dec_ops;
		/*pass the histogram information to viu in frontend*/
		devp->frontend->private_data = &curr_wr_vf->prop;
		ret = dec_ops->decode_isr(devp->frontend, devp->h_cnt64);
		if (ret == TVIN_BUF_SKIP) {
			if (vdin_dbg_en)
				pr_info("%s buffer(%u) skipped.\n",
					__func__, curr_wr_vf->index);
			goto irq_handled;
		/*if the current buffer is reserved,recycle tmp list,
		 * put current buffer to tmp list
		 */
		} else if (ret == TVIN_BUF_TMP) {
			recycle_tmp_vfs(devp->vfp);
			tmp_vf_put(curr_wr_vfe, devp->vfp);
			curr_wr_vfe = NULL;
		/*function of capture end,the reserved is the best*/
		} else if (ret == TVIN_BUF_RECYCLE_TMP) {
			tmp_to_rd(devp->vfp);
			devp->vdin_irq_flag = VDIN_IRQ_FLG_SKIP_FRAME;
			vdin_drop_frame_info(devp, "TVIN_BUF_RECYCLE_TMP");
			goto irq_handled;
		}
	}
	/*check the skip frame*/
	if (devp->frontend && devp->frontend->sm_ops) {
		sm_ops = devp->frontend->sm_ops;
		if (sm_ops->check_frame_skip &&
			sm_ops->check_frame_skip(devp->frontend)) {
			devp->vdin_irq_flag = VDIN_IRQ_FLG_SKIP_FRAME;
			vdin_drop_frame_info(devp, "check frame skip");
			goto irq_handled;
		}
	}

	/* no buffer */
	if (!next_wr_vfe) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_NEXT_FE;
		vdin_drop_frame_info(devp, "no next fe");
		goto irq_handled;
	}

	if (curr_wr_vfe) {
		curr_wr_vfe->flag |= VF_FLAG_NORMAL_FRAME;
		/* provider_vf_put(curr_wr_vfe, devp->vfp); */
		devp->last_wr_vfe = curr_wr_vfe;
	}

	/* prepare for next input data */
	next_wr_vfe = provider_vf_get(devp->vfp);

	if (!next_wr_vfe) {
		devp->vdin_irq_flag = VDIN_IRQ_FLG_NO_NEXT_FE;
		vdin_drop_frame_info(devp, "no next wr vfe");
		goto irq_handled;
	}

	/* debug for video latency */
	next_wr_vfe->vf.ready_jiffies64 = jiffies_64;
	next_wr_vfe->vf.ready_clock[0] = sched_clock();

	vdin_frame_write_ctrl_set(devp, next_wr_vfe,
				  devp->flags & VDIN_FLAG_RDMA_ENABLE);

	devp->curr_wr_vfe = next_wr_vfe;

	if (devp->set_canvas_manual != 1 &&
	    devp->work_mode != VDIN_WORK_MD_V4L)
		vf_notify_receiver(devp->name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

	devp->frame_cnt++;

irq_handled:
	spin_unlock_irqrestore(&devp->isr_lock, flags);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if ((devp->flags & VDIN_FLAG_RDMA_ENABLE) &&
	    (devp->flags_isr & VDIN_FLAG_RDMA_DONE)) {
		ret = rdma_config(devp->rdma_handle,
				  (devp->rdma_enable & 1) ?
				  devp->rdma_irq : RDMA_TRIGGER_MANUAL);
		if (ret == 0)
			devp->flags_isr |= VDIN_FLAG_RDMA_DONE;
		else
			devp->flags_isr &= ~VDIN_FLAG_RDMA_DONE;
	}
#endif
	isr_log(devp->vfp);

	return IRQ_HANDLED;
}

static void vdin_dv_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vdin_dev_s *devp =
		container_of(dwork, struct vdin_dev_s, dv.dv_dwork);

	if (!devp || !devp->frontend) {
		pr_info("%s, dwork error !!!\n", __func__);
		return;
	}
	if (vdin_dv_is_visf_data(devp)) {
		vdin_dolby_buffer_update(devp, devp->dv.dv_cur_index);
		vdin_dolby_addr_update(devp, devp->dv.dv_next_index);
	}

	cancel_delayed_work(&devp->dv.dv_dwork);
}

/*ensure vlock mux switch avoid vlock vsync region*/
static void vdin_vlock_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vdin_dev_s *devp =
		container_of(dwork, struct vdin_dev_s, vlock_dwork);
#ifndef VDIN_BRINGUP_NO_VLOCK
	if (!devp || !devp->frontend || !devp->curr_wr_vfe) {
		pr_info("%s, dwork error !!!\n", __func__);
		return;
	}

	if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
		cancel_delayed_work(&devp->vlock_dwork);
		return;
	}

	//vdin_vlock_input_sel(devp->curr_field_type,
	//		     devp->curr_wr_vfe->vf.source_type);
#endif
	cancel_delayed_work(&devp->vlock_dwork);
}

/*function:open device
 *	1.request irq to open device configure vdinx
 *	2.disable irq until vdin is configured completely
 */
static int vdin_open(struct inode *inode, struct file *file)
{
	struct vdin_dev_s *devp;
	int ret = 0;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct vdin_dev_s, cdev);
	file->private_data = devp;

	if (!devp)
		return -ENXIO;

	if (devp->index == 0 && devp->dtdata->vdin0_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return -ENXIO;
	}

	if (devp->index == 1 && devp->dtdata->vdin1_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return -ENXIO;
	}

	if (devp->set_canvas_manual == 1)
		return 0;

	if (devp->index >= VDIN_MAX_DEVS)
		return -ENXIO;

	if (devp->flags & VDIN_FLAG_FS_OPENED) {
		if (vdin_dbg_en)
			pr_info("%s, device %s opened already\n",
				__func__, dev_name(devp->dev));
		return 0;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		vdin_afbce_vpu_clk_mem_pd(devp, 1);
		/*switch_vpu_mem_pd_vmod(VPU_AFBCE, VPU_MEM_POWER_ON);*/

	devp->flags |= VDIN_FLAG_FS_OPENED;

	/* request irq */
	if (work_mode_simple) {
		ret = request_irq(devp->irq, vdin_isr_simple, IRQF_SHARED,
				  devp->irq_name, (void *)devp);

		disable_irq(devp->irq);
		devp->flags |= VDIN_FLAG_ISR_REQ;
		devp->flags &= (~VDIN_FLAG_ISR_EN);
		if (vdin_dbg_en)
			pr_info("%s vdin.%d simple request_irq\n", __func__,
				devp->index);
	} else {
		if (devp->index == 0)
			ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
					devp->irq_name, (void *)devp);
		else
			ret = request_irq(devp->irq, vdin_v4l2_isr, IRQF_SHARED,
					devp->irq_name, (void *)devp);
		if (ret)
			pr_err("err:req vs irq fail\n");
		else
			pr_info("vdin%d req vs irq %d\n",
				devp->index, devp->irq);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) && devp->index == 0 &&
		    devp->vpu_crash_irq > 0) {
			ret = request_irq(devp->vpu_crash_irq, vpu_crash_isr,
					   IRQF_SHARED,
					   devp->vpu_crash_irq_name,
					   (void *)devp);
			if (ret)
				pr_err("err:req crash_irq fail\n");
			else
				pr_info("vdin%d req crash irq %d\n",
					devp->index, devp->irq);
		}
		if (devp->wr_done_irq > 0) {
			ret = request_irq(devp->wr_done_irq, vdin_write_done_isr,
					  IRQF_SHARED, devp->wr_done_irq_name,
					  (void *)devp);
			if (ret)
				pr_err("err:req wr_done_irq fail\n");
			else
				pr_info("vdin%d req wr_done irq %d\n", devp->index,
					devp->wr_done_irq);
		}

		/* only for t7 meta data */
		if (devp->vdin2_meta_wr_done_irq > 0) {
			ret = request_irq(devp->vdin2_meta_wr_done_irq,
					  vdin_wrmif2_dv_meta_wr_done_isr,
					  IRQF_SHARED,
					  devp->vdin2_meta_wr_done_irq_name,
					  (void *)devp);
			if (ret)
				pr_err("err:req meta_wr_done_irq fail\n");
			else
				pr_info("vdin%d req meta_wr_done_irq\n",
					devp->index);
		}
	}
	devp->flags |= VDIN_FLAG_ISR_REQ;
	devp->flags &= (~VDIN_FLAG_ISR_EN);
	/*disable irq until vdin is configured completely*/
	disable_irq(devp->irq);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) && devp->index == 0 &&
	    devp->vpu_crash_irq > 0)
		disable_irq(devp->vpu_crash_irq);
	if (devp->wr_done_irq > 0)
		disable_irq(devp->wr_done_irq);
	if (devp->vdin2_meta_wr_done_irq > 0)
		disable_irq(devp->vdin2_meta_wr_done_irq);

	if (vdin_dbg_en)
		pr_info("%s vdin.%d disable_irq\n", __func__,
			devp->index);

	/* remove the hardware limit to vertical [0-max]*/
	/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
	if (vdin_dbg_en)
		pr_info("open device %s ok\n", dev_name(devp->dev));
	return ret;
}

/*function:
 *	close device
 *	a.vdin_stop_dec
 *	b.vdin_close_fe
 *	c.free irq
 */
static int vdin_release(struct inode *inode, struct file *file)
{
	struct vdin_dev_s *devp = file->private_data;

	if (devp->index == 0 && devp->dtdata->vdin0_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return 0;
	}

	if (devp->index == 1 && devp->dtdata->vdin1_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return 0;
	}

	if (!(devp->flags & VDIN_FLAG_FS_OPENED)) {
		if (vdin_dbg_en)
			pr_info("%s, device %s not opened\n",
				__func__, dev_name(devp->dev));
		return 0;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		vdin_afbce_vpu_clk_mem_pd(devp, 0);
		/*switch_vpu_mem_pd_vmod(VPU_AFBCE, VPU_MEM_POWER_DOWN);*/

	devp->flags &= (~VDIN_FLAG_FS_OPENED);
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
		vdin_stop_dec(devp);
		/* init flag */
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		/* clear the flag of decode started */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
	}
	if (devp->flags & VDIN_FLAG_DEC_OPENED)
		vdin_close_fe(devp);

	devp->flags &= (~VDIN_FLAG_SNOW_FLAG);

	/* free irq */
	if (devp->flags & VDIN_FLAG_ISR_REQ) {
		devp->flags &= (~VDIN_FLAG_ISR_REQ);
		free_irq(devp->irq, (void *)devp);
		if (vdin_dbg_en)
			pr_info("%s vdin.%d free vs irq:%d\n", __func__,
				devp->index, devp->irq);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) && devp->index == 0 &&
		    devp->vpu_crash_irq > 0) {
			free_irq(devp->vpu_crash_irq, (void *)devp);
			if (vdin_dbg_en)
				pr_info("%s vdin.%d free crash irq\n", __func__,
					devp->index);
		}

		if (devp->wr_done_irq > 0) {
			free_irq(devp->wr_done_irq, (void *)devp);
			if (vdin_dbg_en)
				pr_info("%s vdin.%d free wr done irq\n", __func__,
					devp->index);
		}

		if (devp->vdin2_meta_wr_done_irq > 0) {
			free_irq(devp->vdin2_meta_wr_done_irq, (void *)devp);
			if (vdin_dbg_en)
				pr_info("%s vdin.%d free meta irq\n", __func__,
					devp->index);
		}

		if (vdin_dbg_en)
			pr_info("%s vdin.%d free_irq\n", __func__,
				devp->index);
	}

	file->private_data = NULL;

	/* reset the hardware limit to vertical [0-1079]  */
	/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437); */
	/*if (vdin_dbg_en)*/
	pr_info("vdin%d close device %s ok flags=0x%x\n", devp->index,
		dev_name(devp->dev),
		devp->flags);
	return 0;
}

static int vdin_cmd_check(struct vdin_dev_s *devp, unsigned int cmd)
{
	if (devp->work_mode == VDIN_WORK_MD_V4L &&
	   (devp->vdin_function_sel & VDIN_V4L2_IOCTL_CHK) &&
		!(cmd == TVIN_IOC_G_PARM          ||
		  cmd == TVIN_IOC_G_SIG_INFO      ||
		  cmd == TVIN_IOC_G_FRONTEND_INFO ||
		  cmd == TVIN_IOC_G_BUF_INFO      ||
		  cmd == TVIN_IOC_GET_COLOR_RANGE ||
		  cmd == TVIN_IOC_G_INPUT_TIMING  ||
		  cmd == TVIN_IOC_G_EVENT_INFO))
		return -EBUSY;

	return 0;
}

/* vdin ioctl cmd:
 *	TVIN_IOC_OPEN /TVIN_IOC_CLOSE
 *	TVIN_IOC_START_DEC /TVIN_IOC_STOP_DEC
 *	TVIN_IOC_VF_REG /TVIN_IOC_VF_UNREG
 *	TVIN_IOC_G_SIG_INFO /TVIN_IOC_G_BUF_INFO
 *	TVIN_IOC_G_PARM
 *	TVIN_IOC_START_GET_BUF /TVIN_IOC_GET_BUF
 *	TVIN_IOC_PAUSE_DEC /TVIN_IOC_RESUME_DEC
 *	TVIN_IOC_FREEZE_VF /TVIN_IOC_UNFREEZE_VF
 *	TVIN_IOC_CALLMASTER_SET
 *	TVIN_IOC_SNOW_ON /TVIN_IOC_SNOW_OFF
 */
static long vdin_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int i;
	int callmaster_status = 0;
	struct vdin_dev_s *devp = NULL;
	void __user *argp = (void __user *)arg;
	struct vdin_parm_s param;
	ulong flags;
	struct vdin_hist_s vdin1_hist_temp;
	struct page *page;
	struct vdin_set_canvas_s vdin_set_canvas[VDIN_CANVAS_MAX_CNT];
	unsigned int idx = 0;
	unsigned int recovery_idx = 0;
	struct vdin_vrr_freesync_param_s vdin_vrr_status;
	unsigned int tmp = 0;

	/* Get the per-device structure that contains this cdev */
	devp = file->private_data;
	if (!devp)
		return -EFAULT;

	if (devp->index == 0 && devp->dtdata->vdin0_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return 0;
	}

	if (devp->index == 1 && devp->dtdata->vdin1_en == 0) {
		pr_err("%s vdin%d not enable\n", __func__, devp->index);
		return 0;
	}

	if (devp->dbg_v4l_no_vdin_ioctl) {
		pr_err("%s vdin%d debug no vdin ioctl\n", __func__, devp->index);
		return 0;
	}

	if (vdin_cmd_check(devp, cmd)) {
		pr_warn("vdin%d,do not support cmd:%#x on v4l mode\n",
			devp->index, cmd);
		return -EBUSY;
	}

	if (vdin_dbg_en & VDIN_DBG_CNTL_IOCTL)
		pr_info("vdin%d cmd:0x%x come in\n",
			devp->index, cmd);

	switch (cmd) {
	case TVIN_IOC_OPEN: {
		struct tvin_parm_s parm = {0};

		mutex_lock(&devp->fe_lock);
		if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
			pr_err("TVIN_IOC_OPEN(%d) invalid parameter\n",
			       devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (vdin_time_en)
			pr_info("TVIN_IOC_OPEN %ums.\n",
				jiffies_to_msecs(jiffies));
		if (devp->flags & VDIN_FLAG_DEC_OPENED) {
			pr_err("TVIN_IOC_OPEN(%d) port %s opend already\n",
			       parm.index, tvin_port_str(parm.port));
			ret = -EBUSY;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		devp->parm.index = parm.index;
		devp->parm.port  = parm.port;
		devp->unstable_flag = false;
		ret = vdin_open_fe(devp->parm.port, devp->parm.index, devp);
		if (ret) {
			pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
			       devp->index, parm.port);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		/*devp->flags |= VDIN_FLAG_DEC_OPENED;*/
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_OPEN(%d) port %s opened ok\n\n",
				parm.index,	tvin_port_str(devp->parm.port));
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_START_DEC: {
		struct tvin_parm_s parm = {0};
		enum tvin_sig_fmt_e fmt;

		mutex_lock(&devp->fe_lock);
		if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
			pr_err("TVIN_IOC_START_DEC(%d) invalid parameter\n",
			       devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_err("TVIN_IOC_START_DEC(%d) port 0x%x, started already flags=0x%x\n",
			       parm.index, parm.port, devp->flags);
			ret = -EBUSY;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (vdin_time_en) {
			devp->start_time = jiffies_to_msecs(jiffies);
			pr_info("TVIN_IOC_START_DEC %ums.\n", devp->start_time);
		}
		if ((devp->parm.info.status != TVIN_SIG_STATUS_STABLE ||
		     devp->parm.info.fmt == TVIN_SIG_FMT_NULL) &&
		    !IS_TVAFE_ATV_SRC(devp->parm.port)) {
			pr_err("TVIN_IOC_START_DEC: port %s fmt:%s,start invalid\n",
			       tvin_port_str(devp->parm.port),
			       tvin_sig_fmt_str(parm.info.fmt));
			pr_err("	status: %s, fmt: %s\n",
			       tvin_sig_status_str(devp->parm.info.status),
			       tvin_sig_fmt_str(devp->parm.info.fmt));
			ret = -EPERM;
				mutex_unlock(&devp->fe_lock);
				break;
		}
		if (parm.info.fmt == TVIN_SIG_FMT_NULL) {
			if (IS_TVAFE_ATV_SRC(devp->parm.port)) {
				de_fmt_flag = 1;
				devp->parm.info.fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
				fmt = devp->parm.info.fmt;
			} else {
				pr_err("port:%s,fmt null\n", tvin_port_str(devp->parm.port));
				ret = -EFAULT;
				mutex_unlock(&devp->fe_lock);
				break;
			}
		} else {
			de_fmt_flag = 0;
			devp->parm.info.fmt = parm.info.fmt;
			fmt = parm.info.fmt;
		}
		if (vdin_dbg_en)
			pr_info("port:0x%x, set fmt:0x%x\n", devp->parm.port, fmt);
		devp->fmt_info_p =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
		if (!devp->fmt_info_p) {
			pr_err("TVIN_IOC_START_DEC(%d) error, fmt is null\n",
			       devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		ret = vdin_start_dec(devp);
		if (ret) {
			pr_err("TVIN_IOC_START_DEC(%d) error, start_dec fail\n",
			       devp->index);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		devp->flags |= VDIN_FLAG_DEC_STARTED;
		if (devp->parm.port != TVIN_PORT_VIU1 ||
		    viu_hw_irq != 0) {
			/*enable irq */
			enable_irq(devp->irq);
			devp->flags |= VDIN_FLAG_ISR_EN;
			vdin_calculate_isr_interval_value(devp); //init pre cur us value

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) &&
			    devp->index == 0 && devp->vpu_crash_irq > 0)
				enable_irq(devp->vpu_crash_irq);

			if (devp->wr_done_irq > 0)
				enable_irq(devp->wr_done_irq);

			/* for t7 dv meta data */
			if (/*vdin_is_dolby_signal_in(devp) &&*/
			    /*devp->index == devp->dv.dv_path_idx &&*/
			    devp->vdin2_meta_wr_done_irq > 0) {
				enable_irq(devp->vdin2_meta_wr_done_irq);
				pr_info("enable meta wt done irq %d\n",
					devp->vdin2_meta_wr_done_irq);
			}
			if (vdin_dbg_en)
				pr_info("%s START_DEC vdin.%d enable_irq(%d)\n",
					__func__, devp->index, devp->irq);
		}

		if (vdin_dbg_en)
			pr_info("TVIN_IOC_START_DEC(%d) port %s, decode started ok flags=0x%x\n",
				devp->index,
				tvin_port_str(devp->parm.port), devp->flags);
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_STOP_DEC:	{
		struct tvin_parm_s *parm = &devp->parm;

		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			pr_err("TVIN_IOC_STOP_DEC(%d) decode haven't started flags=0x%x\n",
			       devp->index, devp->flags);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		if (vdin_time_en) {
			devp->start_time = jiffies_to_msecs(jiffies);
			pr_info("TVIN_IOC_STOP_DEC %ums.\n", devp->start_time);
		}

		devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
		vdin_stop_dec(devp);
		/* init flag */
		devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
		/* devp->flags &= ~VDIN_FLAG_FORCE_UNSTABLE; */
		/* clear the flag of decode started */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_STOP_DEC(%d) port %s, decode stop ok flags=0x%x\n",
				parm->index, tvin_port_str(parm->port), devp->flags);

		mutex_unlock(&devp->fe_lock);
		if (IS_HDMI_SRC(parm->port)) {
			reset_tvin_smr(parm->index);
			tvin_smr_init(parm->index);
			devp->parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
		}
		break;
	}
	case TVIN_IOC_VF_REG:
		if ((devp->flags & VDIN_FLAG_DEC_REGISTERED)
				== VDIN_FLAG_DEC_REGISTERED) {
			pr_err("TVIN_IOC_VF_REG(%d) decoder is registered already\n",
			       devp->index);
			ret = -EINVAL;
			break;
		}
		devp->flags |= VDIN_FLAG_DEC_REGISTERED;
		vdin_vf_reg(devp);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_VF_REG(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_VF_UNREG:
		if ((devp->flags & VDIN_FLAG_DEC_REGISTERED)
		    != VDIN_FLAG_DEC_REGISTERED) {
			pr_err("TVIN_IOC_VF_UNREG(%d) decoder isn't registered\n",
			       devp->index);
			ret = -EINVAL;
			break;
		}
		devp->flags &= (~VDIN_FLAG_DEC_REGISTERED);
		vdin_vf_unreg(devp);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_VF_REG(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_CLOSE: {
		struct tvin_parm_s *parm = &devp->parm;
		enum tvin_port_e port = parm->port;

		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			pr_err("TVIN_IOC_CLOSE(%d) you have not opened port\n",
			       devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		if (vdin_time_en)
			pr_info("TVIN_IOC_CLOSE %ums.\n",
				jiffies_to_msecs(jiffies));
		vdin_close_fe(devp);
		/*devp->flags &= (~VDIN_FLAG_DEC_OPENED);*/
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_CLOSE(%d) port %s closed ok\n\n",
				parm->index,
				tvin_port_str(port));

		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_S_PARM: {
		struct tvin_parm_s parm = {0};

		if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
			ret = -EFAULT;
			break;
		}
		devp->parm.flag = parm.flag;
		devp->parm.reserved = parm.reserved;
		break;
	}
	case TVIN_IOC_G_PARM: {
		struct tvin_parm_s parm;

		memcpy(&parm, &devp->parm, sizeof(struct tvin_parm_s));
		/* if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
		 *		parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
		 */
		if (copy_to_user(argp, &parm, sizeof(struct tvin_parm_s)))
			ret = -EFAULT;
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_G_PARM signal_type:%#x status:%#x\n",
				devp->parm.info.signal_type, devp->parm.info.status);
		break;
	}
	case TVIN_IOC_G_SIG_INFO: {
		struct tvin_info_s info;

		memset(&info, 0, sizeof(struct tvin_info_s));
		mutex_lock(&devp->fe_lock);
		/* if port is not opened, ignore this command */
		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			pr_info("vdin get info fail, DEC_OPENED\n");
			break;
		}
		memcpy(&info, &devp->parm.info, sizeof(struct tvin_info_s));
		if (info.status != TVIN_SIG_STATUS_STABLE)
			info.fps = 0;

		if (copy_to_user(argp, &info, sizeof(struct tvin_info_s)))
			ret = -EFAULT;

		if (vdin_dbg_en)
			pr_info("%s signal_type: 0x%x,status:0x%x,fr:%d,fmt:%#x\n",
				__func__, info.signal_type, info.status, info.fps, info.fmt);
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_G_FRONTEND_INFO: {
		struct tvin_frontend_info_s info;

		mutex_lock(&devp->fe_lock);
		if (!devp || !devp->fmt_info_p ||
		    devp->parm.info.status != TVIN_SIG_STATUS_STABLE) {
			pr_err("get frontend failure vdin not stable(%d:%d)\n",
				!devp->fmt_info_p, devp->parm.info.status);
			ret = -EFAULT;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		memset(&info, 0, sizeof(struct tvin_frontend_info_s));
		info.cfmt = devp->parm.info.cfmt;
		info.fps = devp->parm.info.fps;
		info.colordepth = devp->prop.colordepth;
		info.scan_mode = devp->fmt_info_p->scan_mode;
		info.height = devp->fmt_info_p->v_active;
		info.width = devp->fmt_info_p->h_active;
		if (copy_to_user(argp, &info,
				 sizeof(struct tvin_frontend_info_s)))
			ret = -EFAULT;
		mutex_unlock(&devp->fe_lock);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_G_FRONTEND_INFO(%d:%#x)\n",
				devp->index, devp->parm.info.fmt);
		break;
	}
	case TVIN_IOC_G_BUF_INFO: {
		struct tvin_buf_info_s buf_info;

		memset(&buf_info, 0, sizeof(buf_info));
		buf_info.buf_count	= devp->canvas_max_num;
		buf_info.buf_width	= devp->canvas_w;
		buf_info.buf_height = devp->canvas_h;
		buf_info.buf_size	= devp->canvas_max_size;
		buf_info.wr_list_size = devp->vfp->wr_list_size;
		if (copy_to_user(argp, &buf_info,
				 sizeof(struct tvin_buf_info_s)))
			ret = -EFAULT;
		break;
	}
	case TVIN_IOC_START_GET_BUF:
		devp->vfp->wr_next = devp->vfp->wr_list.next;
		break;
	case TVIN_IOC_GET_BUF: {
		struct tvin_video_buf_s tv_buf;
		struct vf_entry *vfe;

		if (!devp->vfp->wr_next) {
			pr_info("not TVIN_IOC_START_GET_BUF wr_next is null\n");
			ret = -EFAULT;
			break;
		}

		memset(&tv_buf, 0, sizeof(tv_buf));
		vfe = list_entry(devp->vfp->wr_next, struct vf_entry, list);
		devp->vfp->wr_next = devp->vfp->wr_next->next;
		if (devp->vfp->wr_next != &devp->vfp->wr_list)
			tv_buf.index = vfe->vf.index;
		else
			tv_buf.index = -1;

		if (copy_to_user(argp, &tv_buf, sizeof(struct tvin_video_buf_s)))
			ret = -EFAULT;
		break;
	}
	case TVIN_IOC_PAUSE_DEC:
		vdin_pause_dec(devp, 0);
		break;
	case TVIN_IOC_RESUME_DEC:
		vdin_resume_dec(devp, 0);
		break;
	case TVIN_IOC_FREEZE_VF: {
		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			pr_err("TVIN_IOC_FREEZE_BUF(%d) decode haven't started\n",
			       devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}

		if (devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE)
			vdin_vf_freeze(devp->vfp, 1);
		else
			vdin_vf_freeze(devp->vfp, 2);
		mutex_unlock(&devp->fe_lock);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_FREEZE_VF(%d) ok\n\n", devp->index);
		break;
	}
	case TVIN_IOC_UNFREEZE_VF: {
		mutex_lock(&devp->fe_lock);
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			pr_err("TVIN_IOC_FREEZE_BUF(%d) decode haven't started\n",
			       devp->index);
			ret = -EPERM;
			mutex_unlock(&devp->fe_lock);
			break;
		}
		vdin_vf_unfreeze(devp->vfp);
		mutex_unlock(&devp->fe_lock);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_UNFREEZE_VF(%d) ok\n\n", devp->index);
		break;
	}
	case TVIN_IOC_CALLMASTER_SET: {
		enum tvin_port_e port_call = TVIN_PORT_NULL;
		struct tvin_frontend_s *fe = NULL;
		/* struct vdin_dev_s *devp = dev_get_drvdata(dev); */
		if (copy_from_user(&port_call,
				   argp,
				   sizeof(enum tvin_port_e))) {
			ret = -EFAULT;
			break;
		}
		fe = tvin_get_frontend(port_call, 0);
		if (fe && fe->dec_ops && fe->dec_ops->callmaster_det) {
			/*call the frontend det function*/
			callmaster_status =
				fe->dec_ops->callmaster_det(port_call, fe);
		}
		if (vdin_dbg_en)
			pr_info("[vdin.%d]:%s callmaster_status=%d,port_call=[%s]\n",
				devp->index, __func__,
				callmaster_status,
				tvin_port_str(port_call));
		break;
	}
	case TVIN_IOC_CALLMASTER_GET: {
		if (copy_to_user(argp, &callmaster_status, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		callmaster_status = 0;
		break;
	}
	case TVIN_IOC_SNOW_ON:
		devp->flags |= VDIN_FLAG_SNOW_FLAG;
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_SNOW_ON(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_SNOW_OFF:
		devp->flags &= (~VDIN_FLAG_SNOW_FLAG);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_SNOW_OFF(%d) ok\n\n", devp->index);
		break;
	case TVIN_IOC_GAME_MODE: {
		if (copy_from_user(&tmp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		if (devp->debug.force_game_mode)
			break;

		mutex_lock(&devp->fe_lock);
		if (game_mode != tmp)
			vdin_game_mode_chg(devp, game_mode, tmp);
		game_mode = tmp;
		mutex_unlock(&devp->fe_lock);
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_GAME_MODE(cfg:%d,cur:%#x) done\n",
				game_mode, devp->game_mode);
		break;
	}
	case TVIN_IOC_VRR_MODE:
		if (copy_from_user(&devp->vrr_mode, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_VRR_MODE(%d) done\n\n", devp->vrr_mode);
		/* if (devp->vrr_mode) */
			vdin_frame_lock_check(devp, 1);
		break;
	case TVIN_IOC_G_VRR_STATUS:
		if (vdin_check_is_spd_data(devp) &&
		   (devp->vrr_data.cur_spd_data5 >> 2 & 0x3)) {
			vdin_vrr_status.cur_vrr_status =
				((devp->vrr_data.cur_spd_data5 >> 2 & 0x3) + 1);
		} else {
			vdin_vrr_status.cur_vrr_status =
				devp->prop.vtem_data.vrr_en;
			devp->vrr_data.cur_spd_data5 = 0;
		}

		vdin_vrr_status.local_dimming_disable = 1;
		vdin_vrr_status.native_color_en = 0;
		vdin_vrr_status.tone_mapping_en = 0;

		if (copy_to_user(argp, &vdin_vrr_status,
					  sizeof(struct vdin_vrr_freesync_param_s))) {
			pr_info("vdin_vrr_status copy fail\n");
			ret = -EFAULT;
		}

		if (vdin_dbg_en)
			pr_info("VrrSta:%d LD_dis:%d NC_en:%d TM_en:%d vrr:%d %d %d %d\n",
				vdin_vrr_status.cur_vrr_status,
				vdin_vrr_status.local_dimming_disable,
				vdin_vrr_status.native_color_en,
				vdin_vrr_status.tone_mapping_en,
				devp->prop.vtem_data.vrr_en,
				devp->prop.spd_data.data[0],
				devp->prop.spd_data.data[5],
				devp->vrr_data.cur_spd_data5);
		break;
	case TVIN_IOC_GET_COLOR_RANGE:
		if (copy_to_user(argp, &color_range_force,
				 sizeof(enum tvin_force_color_range_e))) {
			ret = -EFAULT;
			pr_info("TVIN_IOC_GET_COLOR_RANGE err\n\n");
			break;
		}
		pr_info("get color range-%d\n\n", color_range_force);
		break;
	case TVIN_IOC_SET_COLOR_RANGE:
		if (devp->dv.dv_flag) {
			ret = -EFAULT;
			pr_info("dv not support set color\n");
			break;
		}

		if (copy_from_user(&color_range_force, argp,
				   sizeof(enum tvin_force_color_range_e))) {
			ret = -EFAULT;
			pr_info("TVIN_IOC_SET_COLOR_RANGE err\n\n");
			break;
		}
		pr_info("force color range-%d\n\n", color_range_force);
		break;
	case TVIN_IOC_GET_LATENCY_MODE:
		if (devp->parm.info.status != TVIN_SIG_STATUS_STABLE)
			pr_info("get TVIN_IOC_GET_ALLM_MODE signal not stable\n");

		if (copy_to_user(argp, &devp->prop.latency,
				 sizeof(struct tvin_latency_s))) {
			ret = -EFAULT;
			pr_info("TVIN_IOC_GET_ALLM_MODE err\n\n");
			break;
		}

		if (vdin_dbg_en)
			pr_info("allm mode-%d,IT=%d,CN=%d\n\n",
				devp->prop.latency.allm_mode,
				devp->prop.latency.it_content,
				devp->prop.latency.cn_type);
		break;
	case TVIN_IOC_G_VDIN_HIST:
		if (devp->index == 0) {
			pr_info("TVIN_IOC_G_VDIN_HIST can't be used at vdin0\n");
			break;
		}

		spin_lock_irqsave(&devp->hist_lock, flags);
		vdin1_hist_temp.sum = vdin1_hist.sum;
		vdin1_hist_temp.width = vdin1_hist.width;
		vdin1_hist_temp.height = vdin1_hist.height;
		vdin1_hist_temp.ave = vdin1_hist.ave;
		for (i = 0; i < 64; i++)
			vdin1_hist_temp.hist[i] = vdin1_hist.hist[i];
		spin_unlock_irqrestore(&devp->hist_lock, flags);
		if (vdin_dbg_en) {
			if (pr_times++ > 10) {
				pr_times = 0;
				pr_info("-:h=%d,w=%d,a=%d\n",
					vdin1_hist_temp.height,
					vdin1_hist_temp.width,
					vdin1_hist_temp.ave);
				for (i = 0; i < 64; i++)
					pr_info("-:vdin1 hist[%d]=%d\n",
						i, vdin1_hist_temp.hist[i]);
			}
		}

		if (vdin1_hist.height == 0 || vdin1_hist.width == 0) {
			ret = -EFAULT;
		} else if (copy_to_user(argp, &vdin1_hist_temp,
				      sizeof(struct vdin_hist_s))) {
			pr_info("vdin1_hist copy fail\n");
			ret = -EFAULT;
		}
		break;
	case TVIN_IOC_S_VDIN_V4L2START:{
		struct vdin_v4l2_param_s vdin_v4l2_param;

		memset(&vdin_v4l2_param, 0, sizeof(vdin_v4l2_param));
		if (devp->index == 0) {
			pr_info("TVIN_IOC_S_VDIN_V4L2START can't be used at vdin0\n");
			break;
		}
		//if (devp->flags & VDIN_FLAG_ISR_REQ) {
		//	free_irq(devp->irq, (void *)devp);
		//	if (vdin_dbg_en)
		//		pr_info("%s vdin.%d free_irq\n", __func__,
		//			devp->index);
		//}

		if (copy_from_user(&vdin_v4l2_param, argp,
				   sizeof(struct vdin_v4l2_param_s))) {
			pr_info("vdin_v4l2_param copy fail\n");
			return -EFAULT;
		}
		if (vdin_v4l2_param.fps == 0) {
			ret = -EFAULT;
			pr_err("TVIN_IOC_S_VDIN_V4L2START,fps should not be zero\n");
			break;
		}

		if (vdin_dbg_en) {
			pr_info("width:%d height:%d dst_width:%d dst_height:%d\n",
				vdin_v4l2_param.width, vdin_v4l2_param.height,
				vdin_v4l2_param.dst_width, vdin_v4l2_param.dst_height);
			pr_info("fps:%d dst_fmt:%d bit_order:%d mode:%d bit_dep:%d secure:%d\n",
				vdin_v4l2_param.fps, vdin_v4l2_param.dst_fmt,
				vdin_v4l2_param.bit_order, vdin_v4l2_param.mode,
				vdin_v4l2_param.bit_dep, vdin_v4l2_param.secure_memory_en);
		}

		memset(&param, 0, sizeof(struct vdin_parm_s));
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_SM1))
			param.port = TVIN_PORT_VIU1_WB0_VPP;
		else
			param.port = TVIN_PORT_VIU1;

		param.h_active = vdin_v4l2_param.width;
		param.v_active = vdin_v4l2_param.height;
		param.dest_h_active = vdin_v4l2_param.dst_width;
		param.dest_v_active = vdin_v4l2_param.dst_height;

		if (devp->set_canvas_manual != 1) {
			param.reserved |= PARAM_STATE_HISTGRAM;
			/* use 1280X720 for histgram*/
			if (vdin_v4l2_param.width > 1280 &&
			    vdin_v4l2_param.height > 720) {
				devp->debug.scaling4w = 1280;
				devp->debug.scaling4h = 720;
				devp->debug.dest_cfmt = TVIN_YUV422;
				devp->flags |= VDIN_FLAG_MANUAL_CONVERSION;
			}
		}

		param.frame_rate = vdin_v4l2_param.fps;
		param.cfmt = TVIN_YUV422;
		devp->prop.colordepth = vdin_v4l2_param.bit_dep;

		if (devp->set_canvas_manual == 1)
			param.dfmt = TVIN_RGB444;
		else
			param.dfmt = TVIN_YUV422;

		param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		param.fmt = TVIN_SIG_FMT_MAX;
		//devp->flags |= VDIN_FLAG_V4L2_DEBUG;
		devp->hist_bar_enable = 1;
		if (vdin_v4l2_param.secure_memory_en)
			devp->mem_protected = 1;
		start_tvin_service(devp->index, &param);
		break;
	}
	case TVIN_IOC_S_VDIN_V4L2STOP:
		if (devp->index == 0) {
			pr_info("TVIN_IOC_S_VDIN_V4L2STOP can't be used at vdin0\n");
			break;
		}
		devp->parm.reserved &= ~PARAM_STATE_HISTGRAM;
		devp->flags &= (~VDIN_FLAG_ISR_REQ);
		devp->flags &= (~VDIN_FLAG_FS_OPENED);
		stop_tvin_service(devp->index);

		/*release manual set dma-bufs*/
		if (devp->set_canvas_manual == 1) {
			for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
				if (vdin_set_canvas_addr[i].dma_buffer == 0)
					break;

				dma_buf_unmap_attachment(vdin_set_canvas_addr[i]
							 .dmabuf_attach,
							 vdin_set_canvas_addr[i]
							 .sg_table,
							 DMA_BIDIRECTIONAL);
				dma_buf_detach(vdin_set_canvas_addr[i].dma_buffer,
					       vdin_set_canvas_addr[i]
					       .dmabuf_attach);
				dma_buf_put(vdin_set_canvas_addr[i].dma_buffer);
			}
			memset(vdin_set_canvas_addr, 0,
			       sizeof(struct vdin_set_canvas_addr_s) *
			       VDIN_CANVAS_MAX_CNT);
			memset(devp->keystone_entry, 0,
			       sizeof(struct vf_entry *) * VDIN_CANVAS_MAX_CNT);
		}
		break;

	case TVIN_IOC_S_CANVAS_ADDR:
		if (devp->index == 0 || !devp->set_canvas_manual) {
			pr_info("TVIN_IOC_S_CANVAS_ADDR vdin0 or not manual canvas\n");
			break;
		}

		if (copy_from_user(vdin_set_canvas, argp,
				   sizeof(struct vdin_set_canvas_s) *
					  VDIN_CANVAS_MAX_CNT)) {
			pr_info("TVIN_IOC_S_CANVAS_ADDR copy fail\n");
			return -EFAULT;
		}

		memset(vdin_set_canvas_addr, 0,
		       sizeof(struct vdin_set_canvas_addr_s) *
		       VDIN_CANVAS_MAX_CNT);
		memset(devp->keystone_entry, 0,
		       sizeof(struct vf_entry *) * VDIN_CANVAS_MAX_CNT);

		for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
			/*when fd means, the canvas list reaches end*/
			if (vdin_set_canvas[i].fd < 0)
				break;

			if (vdin_set_canvas[i].index >= VDIN_CANVAS_MAX_CNT) {
				pr_err("vdin buf idx range (0 ~ %d), current idx is too big.\n ",
				       VDIN_CANVAS_MAX_CNT - 1);
				continue;
			}

			idx = vdin_set_canvas[i].index;
			if (idx < VDIN_CANVAS_MAX_CNT) {
				vdin_set_canvas_addr[idx].dma_buffer =
					dma_buf_get(vdin_set_canvas[i].fd);

				vdin_set_canvas_addr[idx].dmabuf_attach =
				dma_buf_attach(vdin_set_canvas_addr[idx]
					       .dma_buffer,
					       devp->dev);
				vdin_set_canvas_addr[idx].sg_table =
				dma_buf_map_attachment(vdin_set_canvas_addr[idx]
						       .dmabuf_attach,
						       DMA_BIDIRECTIONAL);

				page =
				sg_page(vdin_set_canvas_addr[idx].sg_table->sgl);
				vdin_set_canvas_addr[idx].paddr =
					PFN_PHYS(page_to_pfn(page));
				vdin_set_canvas_addr[idx].size =
					vdin_set_canvas_addr[idx].dma_buffer->size;

				pr_info("TVIN_IOC_S_CANVAS_ADDR[%d] addr=0x%lx, len=0x%x.\n",
					i,
					vdin_set_canvas_addr[idx].paddr,
					vdin_set_canvas_addr[idx].size);
			} else {
				pr_info("VDIN err canvas idx:%d\n", idx);
			}
			__close_fd(current->files, vdin_set_canvas[i].fd);
		}
		break;

	case TVIN_IOC_S_CANVAS_RECOVERY:
		if (devp->index == 0) {
			pr_info("TVIN_IOC_S_CANVAS_RECOVERY can't be used at vdin0\n");
			break;
		}

		if (copy_from_user(&recovery_idx, argp, sizeof(unsigned int))) {
			pr_info("TVIN_IOC_S_CANVAS_RECOVERY copy fail\n");
			return -EFAULT;
		}

		if (recovery_idx < VDIN_CANVAS_MAX_CNT &&
		    devp->keystone_entry[recovery_idx]) {
			receiver_vf_put(&devp->keystone_entry[recovery_idx]->vf,
					devp->vfp);
			devp->keystone_entry[recovery_idx] = NULL;
		} else {
			pr_err("[vdin.%d] idx %d RECOVERY error\n",
			       devp->index, recovery_idx);
		}
		break;
	case TVIN_IOC_S_PC_MODE:
		if (copy_from_user(&tmp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		if (devp->debug.force_pc_mode)
			break;

		if (vdin_dbg_en)
			pr_err("Enter,TVIN_IOC_S_PC_MODE:%d->tmp:%d func_sel:0x%x\n",
				vdin_pc_mode, tmp, devp->vdin_function_sel);

		mutex_lock(&devp->fe_lock);
		if (vdin_pc_mode != tmp &&
		    (devp->vdin_function_sel & VDIN_SELF_STOP_START)) {
			if (devp->flags & VDIN_FLAG_DEC_STARTED &&
			    (devp->frame_cnt < devp->vdin_drop_num ||
			     devp->frame_cnt > VDIN_SET_MODE_MAX_FRAME)) {
				_video_set_disable(1);
				/* disable video one vsync effective */
				usleep_range(21000, 22000);
				vdin_pc_mode = tmp;
				devp->self_stop_start = 1;
				vdin_self_stop_dec(devp);
				vdin_self_start_dec(devp);
				devp->self_stop_start = 0;
				/* 1:disable video 0:open video 2:open video when new frame */
				_video_set_disable(2);
			}
		}
		vdin_pc_mode = tmp;
		mutex_unlock(&devp->fe_lock);
		break;
	case TVIN_IOC_S_FRAME_WR_EN:
		if (copy_from_user(&devp->vframe_wr_en, argp,
				   sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		if (vdin_dbg_en)
			pr_info("%s wr vframe en: %d\n", __func__,
				devp->vframe_wr_en);
		break;
	case TVIN_IOC_G_INPUT_TIMING: {
	/* when signal is stable, tvserver can get timing info*/
		struct tvin_format_s info;
		enum tvin_sig_fmt_e fmt = devp->parm.info.fmt;

		mutex_lock(&devp->fe_lock);
		if (tvin_get_sm_status(devp->index) == TVIN_SM_STATUS_STABLE) {
			devp->fmt_info_p =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
			if (!devp->fmt_info_p) {
				pr_err("vdin%d get timing error fmt is null\n",
				       devp->index);
				mutex_unlock(&devp->fe_lock);
				break;
			}
			memcpy(&info, devp->fmt_info_p,
			       sizeof(struct tvin_format_s));
			if (copy_to_user(argp, &info,
					 sizeof(struct tvin_format_s)))
				ret = -EFAULT;
		} else {
			pr_info("vdin%d G_INPUT_TIMING err, not stable\n",
				devp->index);
		}
		mutex_unlock(&devp->fe_lock);
		break;
	}
	case TVIN_IOC_G_EVENT_INFO:
		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			ret = -EPERM;
			pr_info("vdin get event fail, DEC_OPENED\n");
			break;
		}
		if (copy_to_user(argp, &devp->pre_event_info,
				 sizeof(struct vdin_event_info)))
			ret = -EFAULT;

		if (vdin_dbg_en)
			pr_info("vdin%d TVIN_IOC_G_EVENT_INFO 0x%x\n",
				devp->index,
				devp->pre_event_info.event_sts);
		/*clear event info*/
		/*devp->pre_event_info.event_sts = 0;*/
		break;
	case TVIN_IOC_G_VDIN_STATUS:
		mutex_lock(&devp->fe_lock);
		if (copy_to_user(argp, &devp->flags, sizeof(unsigned int))) {
			mutex_unlock(&devp->fe_lock);
			ret = -EFAULT;
			pr_info("TVIN_IOC_G_VDIN_STATUS err\n\n");
			break;
		}
		if (vdin_dbg_en)
			pr_info("TVIN_IOC_G_VDIN_STATUS(%#x)\n", devp->flags);
		mutex_unlock(&devp->fe_lock);
		break;
	default:
		ret = -ENOIOCTLCMD;
	/* pr_info("%s %d is not supported command\n", __func__, cmd); */
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long vdin_compat_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vdin_ioctl(file, cmd, arg);
	return ret;
}
#endif

/*based on parameters:
 *	mem_start, mem_size
 *	vm_pgoff, vm_page_prot
 */
static int vdin_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vdin_dev_s *devp = file->private_data;
	unsigned long start, len, off;
	unsigned long pfn, size;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	start = devp->mem_start & PAGE_MASK;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + devp->mem_size);

	off = vma->vm_pgoff << PAGE_SHIFT;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	size = vma->vm_end - vma->vm_start;
	pfn  = off >> PAGE_SHIFT;

	if (io_remap_pfn_range(vma, vma->vm_start, pfn, size,
			       vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static unsigned int vdin_poll(struct file *file, poll_table *wait)
{
	struct vdin_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	if ((devp->flags & VDIN_FLAG_FS_OPENED) == 0 &&
	    devp->set_canvas_manual != 1)
		return mask;

	if (devp->set_canvas_manual == 1) {
		if (check_vdin_read_list(devp)) {
			mask = (POLLIN | POLLRDNORM);
			return mask;
		}

		poll_wait(file, &vframe_waitq, wait);

		if (devp->keystone_vframe_ready == 1)
			mask = (POLLIN | POLLRDNORM);
	} else {
		poll_wait(file, &devp->queue, wait);
		mask = (POLLIN | POLLRDNORM);
	}

	return mask;
}

static ssize_t vdin_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct vdin_vf_info vf_info;
	long ret;
	struct vf_entry *vfe;
	struct vdin_dev_s *devp = file->private_data;

	/* currently only for keystone get buf use */
	if (devp->set_canvas_manual == 0)
		return 0;

	vfe = receiver_vf_peek(devp->vfp);
	if (!vfe)
		return 0;

	vfe = receiver_vf_get(devp->vfp);
	if (!vfe)
		return 0;

	/*index = report_canvas_index;*/
	vf_info.index = vfe->vf.index;
	vf_info.crc = vfe->vf.crc;

	/* vdin get frame time */
	vf_info.ready_clock[0] = vfe->vf.ready_clock[0];

	/* vdin put frame time */
	vf_info.ready_clock[1] = vfe->vf.ready_clock[1];

	/* receiver get frame time */
	vf_info.ready_clock[2] = sched_clock();
	devp->keystone_entry[vf_info.index] = vfe;
	ret = copy_to_user(buf, (void *)(&vf_info),
			   sizeof(struct vdin_vf_info));
	if (ret) {
		pr_info("vdin_read copy_to_user error\n");
		return -1;
	}

	devp->keystone_vframe_ready = 0;

	return sizeof(struct vdin_vf_info);
}

static const struct file_operations vdin_fops = {
	.owner = THIS_MODULE,
	.open = vdin_open,
	.read = vdin_read,
	.release = vdin_release,
	.unlocked_ioctl = vdin_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vdin_compat_ioctl,
#endif
	.mmap = vdin_mmap,
	.poll = vdin_poll,
};

static int vdin_add_cdev(struct cdev *cdevp,
			 const struct file_operations *fops,
			 int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(vdin_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *vdin_create_device(struct device *parent, int minor)
{
	dev_t devno = MKDEV(MAJOR(vdin_devno), minor);

	return device_create(vdin_class, parent, devno, NULL, "%s%d",
			VDIN_DEV_NAME, minor);
}

static void vdin_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(vdin_devno), minor);

	device_destroy(vdin_class, devno);
}

struct vdin_dev_s *vdin_get_dev(unsigned int index)
{
	if (index)
		return vdin_devp[1];
	else
		return vdin_devp[0];
}

void vdin_vpu_dev_register(struct vdin_dev_s *devp)
{
	if (vpu_dev_register_flag == 0) {
		vpu_dev_clk_gate = vpu_dev_register(VPU_VPU_CLKB, VDIN_DEV_NAME);
		vpu_dev_mem_pd_vdin0 = vpu_dev_register(VPU_VIU_VDIN0, VDIN_DEV_NAME);
		vpu_dev_mem_pd_vdin1 = vpu_dev_register(VPU_VIU_VDIN1, VDIN_DEV_NAME);
		vpu_dev_mem_pd_afbce = vpu_dev_register(VPU_AFBCE, VDIN_DEV_NAME);
		vpu_dev_register_flag = 1;
	}
}

void vdin_vpu_clk_gate_on_off(struct vdin_dev_s *devp, unsigned int on)
{
	if (on)
		vpu_dev_clk_gate_on(vpu_dev_clk_gate);
	else
		vpu_dev_clk_gate_off(vpu_dev_clk_gate);
}

void vdin_vpu_clk_mem_pd(struct vdin_dev_s *devp, unsigned int on)
{
	u32 offset = devp->addr_offset;

	if (on) {
		if (offset)
			vpu_dev_mem_power_on(vpu_dev_mem_pd_vdin1);
		else
			vpu_dev_mem_power_on(vpu_dev_mem_pd_vdin0);
	} else {
		if (offset)
			vpu_dev_mem_power_down(vpu_dev_mem_pd_vdin1);
		else
			vpu_dev_mem_power_down(vpu_dev_mem_pd_vdin0);
	}
}

void vdin_afbce_vpu_clk_mem_pd(struct vdin_dev_s *devp,
			       unsigned int on)
{
	if (on)
		vpu_dev_mem_power_on(vpu_dev_mem_pd_afbce);
	else
		vpu_dev_mem_power_down(vpu_dev_mem_pd_afbce);
}

static ssize_t vdin_param_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	int len = 0;

	len += sprintf(buf + len, "422wmif_en = %d\n",
		       vdin_cfg_444_to_422_wmif_en);
	len += sprintf(buf + len, "vdin_dbg_en = %d\n",
		       vdin_dbg_en);

	return len;
}

static ssize_t vdin_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *bu, size_t count)
{
	const char *delim = " ";
	char *token;
	char *cur = (char *)bu;
	u32 val;
	struct vdin_dev_s *devp;

	devp = dev_get_drvdata(dev);

	token = strsep(&cur, delim);
	if (token && strncmp(token, "422wmif_en", 10) == 0) {
		/*get the next param*/
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		vdin_cfg_444_to_422_wmif_en = val;
	} else if (token && strncmp(token, "vdin_dbg_en", 11) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		vdin_dbg_en = val;
	} else {
		pr_info("----cmd list----\n");
		pr_info("422wmif_en 0/1\n");
		pr_info("dv_de_scramble 0/1\n");
		pr_info("vdin_dbg_en 0/1\n");
	}

	return count;
}
static DEVICE_ATTR_RW(vdin_param);

int vdin_create_dev_class_files(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_vdin_param);

	ret = vdin_create_debug_files(dev);

	vdin_v4l2_create_device_files(dev);

	return ret;
}

void vdin_rm_dev_class_files(struct device *dev)
{
	device_remove_file(dev, &dev_attr_vdin_param);

	vdin_remove_debug_files(dev);

	vdin_v4l2_remove_device_files(dev);
}

static int vdin_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	/*pr_info("%s\n", __func__);*/
	if (IS_HDMI_SRC(port))
		return 0;
	else
		return -1;
}

static int vdin_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	/*pr_info("%s\n", __func__);*/

	return 0;
}

static void vdin_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	/*pr_info("%s\n", __func__);*/
}

static void vdin_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	/*pr_info("%s\n", __func__);*/
}

static void vdin_dec_close(struct tvin_frontend_s *fe)
{
	/*pr_info("%s\n", __func__);*/
}

static int vdin_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	/*pr_info("%s\n", __func__);*/
	return 0;
}

static bool vdin_set_sig_property(struct tvin_frontend_s *fe)
{
	struct vdin_dev_s *devp = container_of(fe, struct vdin_dev_s,
						vdin_frontend);
	struct tvin_state_machine_ops_s *sm_ops;

	if (!(devp->flags & VDIN_FLAG_ISR_EN) && vdin_get_prop_in_vs_en)
		return 1;

	if (!IS_HDMI_SRC(devp->parm.port))
		return 1;

	if (devp->frontend) {
		sm_ops = devp->frontend->sm_ops;
		if (sm_ops && sm_ops->get_sig_property &&
		    vdin_get_prop_in_fe_en) {
			sm_ops->get_sig_property(devp->frontend, &devp->prop);
			devp->dv.dv_flag = devp->prop.dolby_vision;
			devp->dv.low_latency = devp->prop.low_latency;
			devp->pre_prop.latency.allm_mode =
				devp->prop.latency.allm_mode;
			devp->prop.cnt++;
			if (vdin_prop_monitor)
				pr_info("%s dv:%d LL:%d hdr:%d allm:%d signal_type:0x%x\n",
					__func__, devp->dv.dv_flag,
					devp->dv.low_latency,
					devp->prop.vdin_hdr_flag,
					devp->prop.latency.allm_mode,
					devp->parm.info.signal_type);
		}
	}

	return 0;
}

int vdin_get_irq_from_dts(struct platform_device *pdev,
					struct vdin_dev_s *devp)
{
	struct resource *res;
	int ret;

	/* vdin vsync interrupt, first id is vsync int */
	devp->irq = of_irq_get_byname(pdev->dev.of_node, "vsync_int");
	if (devp->irq <= 0) {
		/* get irq from resource */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		if (!res) {
			pr_err("%s: can't get irq resource\n", __func__);
			ret = -ENXIO;
			return ret;
		}
		devp->irq = res->start;
	}

	snprintf(devp->irq_name, sizeof(devp->irq_name),
		 "vdin%d-irq", devp->index);
	pr_info("%s=%d\n", devp->irq_name, devp->irq);

	/* get vpu crash irq number */
	if (devp->index == 0) {
		devp->vpu_crash_irq =
			of_irq_get_byname(pdev->dev.of_node, "vpu_crash_int");
		snprintf(devp->vpu_crash_irq_name,
			 sizeof(devp->vpu_crash_irq_name),
			 "vpu_crash_int");
		pr_info("%s=%d\n", devp->vpu_crash_irq_name,
			devp->vpu_crash_irq);
	}

	/*
	 *if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) && devp->index == 0) {
	 *	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	 *	if (!res) {
	 *		pr_err("%s: can't get crash irq resource\n", __func__);
	 *		devp->vpu_crash_irq = 0;
	 *	} else {
	 *		devp->vpu_crash_irq = res->start;
	 *		snprintf(devp->vpu_crash_irq_name,
	 *			 sizeof(devp->vpu_crash_irq_name),
	 *			 "vpu-crash-irq");
	 *	}
	 *}
	 */

	/*get vdin write down irq number*/
	devp->wr_done_irq = of_irq_get_byname(pdev->dev.of_node,
					       "write_done_int");
	snprintf(devp->wr_done_irq_name,
		 sizeof(devp->wr_done_irq_name),
		 "vdin%d_wr_done", devp->index);
	pr_info("%s %s=%d\n", __func__, devp->wr_done_irq_name, devp->wr_done_irq);

	/* only for t7 dv vdin2 meta write done irq*/
	if (devp->index == 0) {
		devp->vdin2_meta_wr_done_irq =
			of_irq_get_byname(pdev->dev.of_node,
				       "mif2_meta_wr_done_int");
		snprintf(devp->vdin2_meta_wr_done_irq_name,
			 sizeof(devp->vdin2_meta_wr_done_irq_name),
			 "meta_wr_done_int");
		pr_info("%s=%d\n", devp->vdin2_meta_wr_done_irq_name,
			devp->vdin2_meta_wr_done_irq);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "rdma-irq", &devp->rdma_irq);
	if (ret) {
		pr_err("don't find  match rdma irq, disable rdma\n");
		devp->rdma_irq = 0;
	}

	return 0;
}

static struct tvin_decoder_ops_s vdin_dec_ops = {
	.support            = vdin_dec_support,
	.open               = vdin_dec_open,
	.start              = vdin_dec_start,
	.stop               = vdin_dec_stop,
	.close              = vdin_dec_close,
	.decode_isr         = vdin_dec_isr,
};

static struct tvin_state_machine_ops_s vdin_sm_ops = {
	.vdin_set_property = vdin_set_sig_property,
};

static const struct match_data_s vdin_dt_xxx = {
	.name = "vdin",
	.hw_ver = VDIN_HW_ORG,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0,		.ipt444_to_422_12bit = 0,
	.vdin0_line_buff_size = 0xf00,	.vdin1_line_buff_size = 0xf00,
};

const struct match_data_s vdin_dt_tl1 = {
	.name = "vdin",
	.hw_ver = VDIN_HW_ORG,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0,		.ipt444_to_422_12bit = 0,
	.vdin0_line_buff_size = 0xf00,	.vdin1_line_buff_size = 0xf00,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = false,
};

const struct match_data_s vdin_dt_sm1 = {
	.name = "vdin",
	.hw_ver = VDIN_HW_ORG,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0,		.ipt444_to_422_12bit = 0,
	.vdin0_line_buff_size = 0x780,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = 0,		.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_tm2 = {
	.name = "vdin-tm2",
	.hw_ver = VDIN_HW_TM2,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/	.ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0xf00,	.vdin1_line_buff_size = 0xf00,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_tm2_ver_b = {
	.name = "vdin-tm2verb",
	.hw_ver = VDIN_HW_TM2_B,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/	.ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0xf00,	.vdin1_line_buff_size = 0xf00,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_sc2 = {
	.name = "vdin-sc2",
	.hw_ver = VDIN_HW_SC2,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/	.ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0x780,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = 0,		.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_t5 = {
	.name = "vdin-t5",
	.hw_ver = VDIN_HW_T5,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/	.ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0xf00,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = false,
};

static const struct match_data_s vdin_dt_t5d = {
	.name = "vdin-t5d",
	.hw_ver = VDIN_HW_T5D,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/	.ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0x780,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = VDIN_2K_SIZE,	.vdin1_set_hdr = false,
};

static const struct match_data_s vdin_dt_t7 = {
	.name = "vdin-t7",
	.hw_ver = VDIN_HW_T7,
	.vdin0_en = 1,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/	.ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0x780,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_s4 = {
	.name = "vdin-s4",
	.hw_ver = VDIN_HW_S4,
	.vdin0_en = 0,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0,		.ipt444_to_422_12bit = 0,
	.vdin0_line_buff_size = 0x780,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = 0,		.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_t3 = {
	.name = "vdin-t3",
	.hw_ver = VDIN_HW_T3,
	.vdin0_en = 1,                  .vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/  .ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0x1000,  .vdin1_line_buff_size = 0x1000,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = false,
};

static const struct match_data_s vdin_dt_s4d = {
	.name = "vdin-s4d",
	.hw_ver = VDIN_HW_S4D,
	.vdin0_en = 0,			.vdin1_en = 1,
	.de_tunnel_tunnel = 0,		.ipt444_to_422_12bit = 0,
	.vdin0_line_buff_size = 0x780,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = 0,		.vdin1_set_hdr = true,
};

static const struct match_data_s vdin_dt_t5w = {
	.name = "vdin-t5w",
	.hw_ver = VDIN_HW_T5W,
	.vdin0_en = 1,                  .vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/  .ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0x1000,	.vdin1_line_buff_size = 0x780,
	.vdin0_max_w_h = VDIN_4K_SIZE,	.vdin1_set_hdr = false,
};

static const struct match_data_s vdin_dt_s5 = {
	.name = "vdin-s5",
	.hw_ver = VDIN_HW_S5,
	.vdin0_en = 0,                  .vdin1_en = 1,
	.de_tunnel_tunnel = 0, /*0,1*/  .ipt444_to_422_12bit = 0, /*0,1*/
	.vdin0_line_buff_size = 0,      .vdin1_line_buff_size = 0x1000,
	.vdin0_max_w_h = 0,		.vdin1_set_hdr = true,
};

static const struct of_device_id vdin_dt_match[] = {
	{
		.compatible = "amlogic, vdin",
		.data = &vdin_dt_xxx,
	},
	{
		.compatible = "amlogic, vdin-tl1",
		.data = &vdin_dt_tl1,
	},
	{
		.compatible = "amlogic, vdin-sm1",
		.data = &vdin_dt_sm1,
	},
	{
		.compatible = "amlogic, vdin-tm2",
		.data = &vdin_dt_tm2,
	},
	{
		.compatible = "amlogic, vdin-tm2verb",
		.data = &vdin_dt_tm2_ver_b,
	},
	{
		.compatible = "amlogic, vdin-sc2",
		.data = &vdin_dt_sc2,
	},
	{
		.compatible = "amlogic, vdin-t5",
		.data = &vdin_dt_t5,
	},
	{
		.compatible = "amlogic, vdin-t5d",
		.data = &vdin_dt_t5d,
	},
	{
		.compatible = "amlogic, vdin-t7",
		.data = &vdin_dt_t7,
	},
	{
		.compatible = "amlogic, vdin-s4",
		.data = &vdin_dt_s4,
	},
	{
		.compatible = "amlogic, vdin-t3",
		.data = &vdin_dt_t3,
	},
	{
		.compatible = "amlogic, vdin-s4d",
		.data = &vdin_dt_s4d,
	},
	{
		.compatible = "amlogic, vdin-t5w",
		.data = &vdin_dt_t5w,
	},
	{
		.compatible = "amlogic, vdin-s5",
		.data = &vdin_dt_s5,
	},
	/* DO NOT remove to avoid scan error of KASAN */
	{}
};

static void vdin_event_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vdin_dev_s *devp =
		container_of(dwork, struct vdin_dev_s, event_dwork);
	enum tvin_sm_status_e signal_sts;
	unsigned int ret;
	#define VDIN_UEVENT_LEN 64
	char env[VDIN_UEVENT_LEN];
	char *envp[2];

	signal_sts = tvin_get_sm_status(devp->index);

	pr_info("vdin%d event sm_sts:0x%x event:0x%x\n", devp->index,
		signal_sts, devp->event_info.event_sts);
	devp->pre_event_info.event_sts = devp->event_info.event_sts;

	//if (signal_sts > TVIN_SM_STATUS_UNSTABLE) {
	//	extcon_set_state_sync(devp->extcon_event, EXTCON_DISP_VDIN, 0);
	//	extcon_set_state_sync(devp->extcon_event, EXTCON_DISP_VDIN, 1);
	//} else {
	//	extcon_set_state_sync(devp->extcon_event, EXTCON_DISP_VDIN, 1);
	//	extcon_set_state_sync(devp->extcon_event, EXTCON_DISP_VDIN, 0);
	//}

	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, VDIN_UEVENT_LEN, "vdin%d_extcon=0x%x-%d", devp->index,
		 devp->pre_event_info.event_sts, signal_sts);

	if (devp->index == 0)
		devp->dev->kobj.name = "vdin0event";
	else
		devp->dev->kobj.name = "vdin1event";

	ret = kobject_uevent_env(&devp->dev->kobj, KOBJ_CHANGE, envp);
}

/* add for loopback case, vpp notify vdin which HDR format use */
static int vdin_signal_notify_callback(struct notifier_block *block,
					unsigned long cmd, void *para)
{
	struct vd_signal_info_s *vd_signal = NULL;
	/* only for vdin1 loopback */
	struct vdin_dev_s *devp = vdin_get_dev(1);
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	unsigned int i = 0;
	struct vd_secure_info_s *vd_secure = NULL;
#endif

	switch (cmd) {
	case VIDEO_SIGNAL_TYPE_CHANGED:
		vd_signal = para;

		if (!vd_signal)
			break;

		devp->tx_fmt = vd_signal->signal_type;
		devp->vd1_fmt = vd_signal->vd1_signal_type;

		if (vdin_dbg_en)
			pr_info("%s tx fmt:%d, vd1 fmt:%d\n",
				__func__, devp->tx_fmt, devp->vd1_fmt);
		break;
	case VIDEO_SECURE_TYPE_CHANGED:
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
		vd_secure = (struct vd_secure_info_s *)para;
		if (!vd_secure || !(devp->flags & VDIN_FLAG_DEC_STARTED))
			break;

		/* config secure_en by loopback port */
		switch (devp->parm.port) {
		case TVIN_PORT_VIU1_VIDEO:
		case TVIN_PORT_VIU1_WB0_VD1:
		case TVIN_PORT_VIU1_WB1_VD1:
			devp->secure_en = vd_secure[VD1_OUT].secure_enable;
			break;

		case TVIN_PORT_VIU1_WB0_VD2:
		case TVIN_PORT_VIU1_WB1_VD2:
			devp->secure_en = vd_secure[VD2_OUT].secure_enable;
			break;

		case TVIN_PORT_VIU1_WB0_OSD1:
		case TVIN_PORT_VIU1_WB1_OSD1:
			devp->secure_en = vd_secure[OSD1_VPP_OUT].secure_enable;
			break;

		case TVIN_PORT_VIU1_WB0_OSD2:
		case TVIN_PORT_VIU1_WB1_OSD2:
			devp->secure_en = vd_secure[OSD2_VPP_OUT].secure_enable;
			break;

		case TVIN_PORT_VIU1_WB0_POST_BLEND:
		case TVIN_PORT_VIU1_WB1_POST_BLEND:
		case TVIN_PORT_VIU1_WB0_VPP:
		case TVIN_PORT_VIU1_WB1_VPP:
		case TVIN_PORT_VIU2_ENCL:
		case TVIN_PORT_VIU2_ENCI:
		case TVIN_PORT_VIU2_ENCP:
			devp->secure_en =
				vd_secure[POST_BLEND_OUT].secure_enable;
			break;

		default:
			devp->secure_en = 0;
			break;
		}

		/* vdin already started, set secure mode dynamically */
		if (devp->secure_en && !devp->mem_protected && !devp->set_canvas_manual) {
			vdin_set_mem_protect(devp, 1);
		} else if (!devp->secure_en && devp->mem_protected && !devp->set_canvas_manual) {
			/* buf already put out,should be exhausted */
			for (i = 0; i < devp->vf_mem_max_cnt; i++)
				vdin_vframe_put_and_recycle(devp, NULL,
							    VDIN_VF_RECYCLE);

			vdin_set_mem_protect(devp, 0);
		}
#endif
		break;

	default:
		break;
	}

	return 0;
}

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
static int vdin_get_vinfo_notify_callback(struct notifier_block *block,
					unsigned long event, void *para)
{
	if (event != VOUT_EVENT_MODE_CHANGE)
		return 0;

	struct vdin_dev_s *devp = vdin_get_dev(0);
	const struct vinfo_s *vinfo = get_current_vinfo();

	devp->vinfo_std_duration = vinfo->sync_duration_num / vinfo->sync_duration_den;
	if (vdin_dbg_en)
		pr_info("vdin%d,std_dur:%d\n", devp->index, devp->vinfo_std_duration);

	return 0;
}

static struct notifier_block vdin_get_vinfo_notifier = {
	.notifier_call = vdin_get_vinfo_notify_callback,
};
#endif

static struct notifier_block vdin_signal_notifier = {
	.notifier_call = vdin_signal_notify_callback,
};

static void vdin_get_dts_config(struct vdin_dev_s *devp,
	struct platform_device *pdev)
{
	int ret = 0;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "set_canvas_manual",
				   &devp->set_canvas_manual);

	if (ret)
		devp->set_canvas_manual = 0;
	if (devp->set_canvas_manual == 1)
		devp->cma_config_flag = 0x100;

	devp->dts_config.urgent_en = of_property_read_bool(pdev->dev.of_node,
		"urgent_en");

	devp->dts_config.chk_write_done_en =
		of_property_read_bool(pdev->dev.of_node, "chk_wr_done_en");

	devp->dts_config.v4l_en =
		of_property_read_bool(pdev->dev.of_node, "v4l_support_en");

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		devp->double_wr_cfg = of_property_read_bool(pdev->dev.of_node,
							     "double_write_en");
		devp->secure_en = of_property_read_bool(pdev->dev.of_node,
							 "secure_en");
	}

	ret = of_property_read_u32(pdev->dev.of_node, "frame_buff_num",
				   &devp->frame_buff_num);
	if (ret)
		devp->frame_buff_num = 0;
	devp->frame_buff_num_bak = devp->frame_buff_num;

	if (devp->index == 0) {
		ret = of_property_read_u32(pdev->dev.of_node, "vrr_mode_default",
					&devp->vrr_mode);
		if (ret)
			devp->vrr_mode = 1;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "vdin_function_sel",
				   &devp->vdin_function_sel);
	if (ret)
		devp->vdin_function_sel = 0;

	devp->dbg_no_wr_check =
		of_property_read_bool(pdev->dev.of_node, "dbg_no_wr_check");
}

static int vdin_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct vdin_dev_s *devp;
	/*struct resource *res;*/
	unsigned int bit_mode = VDIN_WR_COLOR_DEPTH_8BIT;
	const struct of_device_id *of_id;

	/* const void *name; */
	/* int offset, size; */
	/* struct device_node *of_node = pdev->dev.of_node; */

	/* malloc vdev */
	devp = kzalloc(sizeof(*devp), GFP_KERNEL);
	if (!devp)
		goto fail_kzalloc_vdev;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "vdin_id", &devp->index);
		if (ret) {
			pr_err("don't find  vdin id.\n");
			goto fail_get_resource_irq;
		}
	}
	vdin_devp[devp->index] = devp;

	/* t7 three screen display RDMA not enough remove vdin1 rdma */
	devp->rdma_not_register = of_property_read_bool(pdev->dev.of_node,
					"rdma_not_register");
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	vdin_rdma_op[devp->index].irq_cb = vdin_rdma_irq;
	vdin_rdma_op[devp->index].arg = devp;
	if (!devp->rdma_not_register)
		devp->rdma_handle = rdma_register(&vdin_rdma_op[devp->index],
						NULL, RDMA_TABLE_SIZE);
	else
		pr_info("vdin%d: rdma not register\n", devp->index);
#endif

	/* create cdev and register with sysfs */
	ret = vdin_add_cdev(&devp->cdev, &vdin_fops, devp->index);
	if (ret) {
		pr_err("%s: failed to add cdev. !!!!!!!!!!\n", __func__);
		goto fail_add_cdev;
	}
	devp->dev = vdin_create_device(&pdev->dev, devp->index);
	if (IS_ERR_OR_NULL(devp->dev)) {
		pr_err("%s: failed to create device. or dev=NULL\n", __func__);
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}

	ret = vdin_create_dev_class_files(devp->dev);
	if (ret < 0) {
		pr_err("%s: fail to create vdin attribute files.\n", __func__);
		goto fail_create_dev_file;
	}

	/* vd1, vdin loop back use rev memory
	 * v4l2 use rev memory
	 */
	if (devp->index > 0) {
		ret = of_reserved_mem_device_init(&pdev->dev);
		if (ret)
			pr_info("vdin[%d] memory resource undefined!!\n", devp->index);
	}
	/*got the dt match data*/
	of_id = of_match_device(vdin_dt_match, &pdev->dev);
	if (!of_id) {
		pr_err("%s: of_match_device get fail\n", __func__);
		goto fail_create_dev_file;
	}
	devp->dtdata = of_id->data;
	pr_info("chip:%s hw_ver:%d\n", devp->dtdata->name,
		devp->dtdata->hw_ver);

#ifdef CONFIG_CMA
	if (!use_reserved_mem) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "flag_cma",
					   &devp->cma_config_flag);
		if (ret) {
			pr_err("don't find  match flag_cma\n");
			devp->cma_config_flag = 0;
		}
		if (devp->cma_config_flag & 0x1) {
			ret = of_property_read_u32(pdev->dev.of_node,
						   "cma_size",
						   &devp->cma_mem_size);
			if (ret)
				pr_err("don't find  match cma_size\n");
			else
				devp->cma_mem_size *= SZ_1M;
		} else {
			devp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
		}
		devp->this_pdev = pdev;
		devp->cma_mem_alloc = 0;
		devp->cma_config_en = 1;
	}
#endif
	use_reserved_mem = 0;
	/*use reserved mem*/
	if (devp->cma_config_en != 1) {
		devp->mem_start = mem_start;
		devp->mem_size = mem_end - mem_start + 1;
	}

	pr_info("vdin(%d) dma mask\n", devp->index);
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&pdev->dev, 0xffffffff) < 0)
		pr_info("dev set_coherent_mask fail\n");

	if (dma_set_mask(&pdev->dev, 0xffffffff) < 0)
		pr_info("set dma mask fail\n");

	devp->dev->dma_mask = &devp->dev->coherent_dma_mask;
	if (dma_set_coherent_mask(devp->dev, 0xffffffff) < 0)
		pr_info("dev set_coherent_mask fail\n");

	if (dma_set_mask(devp->dev, 0xffffffff) < 0)
		pr_info("set dma mask fail\n");

	ret = vdin_get_irq_from_dts(pdev, devp);
	if (ret < 0)
		goto fail_get_resource_irq;

	/*set color_depth_mode*/
	ret = of_property_read_u32(pdev->dev.of_node,
				   "tv_bit_mode", &bit_mode);
	if (ret)
		pr_info("no bit mode found, set 8bit as default\n");

	devp->color_depth_support = bit_mode & 0xff;
	devp->color_depth_config = COLOR_DEEPS_AUTO;

	/* bit8:use 8bit  at 4k_50/60hz_10bit*/
	/* bit9:use 10bit at 4k_50/60hz_10bit*/
	ret = (bit_mode >> 8) & 0xff;
	if (ret == 0)
		devp->output_color_depth = 0;
	else if (ret == 1)/*4k 10bit 8bit to video buffer*/
		devp->output_color_depth = VDIN_COLOR_DEEPS_8BIT;
	else if (ret == 2)/*4k 10bit 10bit to video buffer*/
		devp->output_color_depth = VDIN_COLOR_DEEPS_10BIT;

	devp->double_wr_10bit_sup = (bit_mode >> 10) & 0x1;

	if (devp->color_depth_support & VDIN_WR_COLOR_DEPTH_10BIT_FULL_PACK_MODE)
		devp->full_pack = VDIN_422_FULL_PK_EN;
	else
		devp->full_pack = VDIN_422_FULL_PK_DIS;
	if (devp->color_depth_support & VDIN_WR_COLOR_DEPTH_FORCE_MEM_YUV422_TO_YUV444)
		devp->force_malloc_yuv_422_to_444 = 1;
	else
		devp->force_malloc_yuv_422_to_444 = 0;

	/*set afbce config*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && devp->index == 0) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "afbce_bit_mode", &devp->dts_config.afbce_flag_cfg);
		if (ret)
			devp->dts_config.afbce_flag_cfg = 0;

		if (devp->dts_config.afbce_flag_cfg & 0x1) {
			devp->afbce_info =
				devm_kzalloc(devp->dev, sizeof(struct vdin_afbce_s), GFP_KERNEL);
			if (!devp->afbce_info)
				goto fail_kzalloc_vdev;
		}
		devp->afbce_flag = devp->dts_config.afbce_flag_cfg;
	}

	vdin_get_dts_config(devp, pdev);

	/* init vdin parameters */
	devp->flags = VDIN_FLAG_NULL;
	mutex_init(&devp->fe_lock);
	spin_lock_init(&devp->isr_lock);
	spin_lock_init(&devp->hist_lock);
	sema_init(&devp->sem, 0);
	devp->frontend = NULL;
	if (devp->index == 0) {
		/* reg tv in frontend */
		tvin_frontend_init(&devp->vdin_frontend,
				   &vdin_dec_ops,
				   &vdin_sm_ops,
				   VDIN_FRONTEND_IDX + devp->index);
		sprintf(devp->vdin_frontend.name, "%s", VDIN_DEV_NAME);
		if (tvin_reg_frontend(&devp->vdin_frontend) < 0)
			pr_info("vdin_frontend reg error!!!\n");
	}

	/* vdin_addr_offset */
	if (devp->index == 1) {
		if (is_meson_gxbb_cpu())
			vdin_addr_offset[1] = 0x70;
		else if (is_meson_s5_cpu())
			vdin_addr_offset[1] = 0;
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			vdin_addr_offset[1] = 0x100;

		devp->hv_reverse_en = 1;
	}

	devp->addr_offset = vdin_addr_offset[devp->index];

	/*canvas align number*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		devp->canvas_align = 64;
	else
		devp->canvas_align = 32;
	/*mif reset patch for vdin wr ram bug on gxtvbb*/
	if (is_meson_gxtvbb_cpu())
		enable_reset = 1;
	else
		enable_reset = 0;
	/* 1: gxtvbb vdin out full range, */
	/* 0: >=txl vdin out limit range, */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB)
		devp->color_range_mode = 1;
	else
		devp->color_range_mode = 0;
	/* create vf pool */
	devp->vfp = vf_pool_alloc(VDIN_CANVAS_MAX_CNT);
	if (!devp->vfp) {
		pr_err("%s: fail to alloc vf pool.\n", __func__);
		goto fail_alloc_vf_pool;
	}
	/* init vframe provider */
	/* @todo provider name */
	sprintf(devp->name, "%s%d", PROVIDER_NAME, devp->index);
	vf_provider_init(&devp->vf_provider, devp->name, &vdin_vf_ops, devp->vfp);

	vf_provider_init(&devp->dv.dv_vf_provider, VDIN_DV_NAME,
			 &vdin_vf_ops, devp->vfp);
	/* @todo canvas_config_mode */
	if (canvas_config_mode == 0 || canvas_config_mode == 1)
		vdin_canvas_init(devp);

	/* set drvdata */
	dev_set_drvdata(devp->dev, devp);
	platform_set_drvdata(pdev, devp);

	/* set max pixel clk of vdin */
	vdin_set_config(devp);

	/*probe v4l interface*/
	if (devp->dts_config.v4l_en)
		vdin_v4l2_probe(pdev, devp);

	/* vdin measure clock */
	if (is_meson_gxbb_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		struct clk *clk;

		clk = clk_get(&pdev->dev, "xtal");
		devp->msr_clk = clk_get(&pdev->dev, "cts_vid_lock_clk");
		if (IS_ERR(clk) || IS_ERR(devp->msr_clk)) {
			pr_err("%s: vdin cannot get msr clk !!!\n", __func__);
			clk = NULL;
			devp->msr_clk = NULL;
		} else {
			clk_set_parent(devp->msr_clk, clk);
			devp->msr_clk_val = clk_get_rate(devp->msr_clk);
			pr_info("%s: vdin msr clock is %d MHZ\n", __func__,
				devp->msr_clk_val / 1000000);
		}
#endif
	} else {
		struct clk *fclk_div5;
		unsigned int clk_rate;

		fclk_div5 = clk_get(&pdev->dev, "fclk_div5");
		if (IS_ERR(fclk_div5)) {
			pr_err("vdin%d get fclk_div5 err\n", devp->index);
		} else {
			clk_rate = clk_get_rate(fclk_div5);
			pr_info("%s: fclk_div5 is %d MHZ\n", __func__,
				clk_rate / 1000000);
		}
		devp->msr_clk = clk_get(&pdev->dev, "cts_vdin_meas_clk");
		if (IS_ERR(devp->msr_clk)) {
			pr_err("%s: vdin%d cannot get msr clk !!!\n",
				__func__, devp->index);
			fclk_div5 = NULL;
			devp->msr_clk = NULL;
		} else {
			if (!IS_ERR(fclk_div5))
				clk_set_parent(devp->msr_clk, fclk_div5);

			clk_set_rate(devp->msr_clk, 50000000);
			/* vdin clk is not enabled in probe by default */
			//clk_prepare_enable(devp->msr_clk);
			devp->vdin_clk_flag = 0;
			devp->msr_clk_val = clk_get_rate(devp->msr_clk);
			pr_info("%s: vdin[%d] clock is %d MHZ\n",
				__func__, devp->index,
				devp->msr_clk_val / 1000000);
		}
	}
	/* register vpu clk control interface */
	vdin_vpu_dev_register(devp);
	vdin_enable_module(devp, false);

	/*enable auto cut window for atv*/
	if (devp->index == 0) {
		devp->auto_cut_window_en = 1;
		#ifdef CONFIG_CMA
		devp->cma_mem_mode = 1;
		#endif
	}
	devp->rdma_enable = 1;
	/*set vdin_dev_s size*/
	devp->vdin_dev_ssize = sizeof(struct vdin_dev_s);
	devp->canvas_config_mode = canvas_config_mode;
	devp->dv.dv_config = false;
	/* Game mode 2 use one buffer by default */
	devp->dbg_force_one_buffer = 1;
	if (!devp->index) {
		devp->vdin_drop_num = VDIN_DROP_FRAME_NUM_DEF;
		/* avoid abnormal image */
		devp->dbg_stop_dec_delay = 50000;
	} else {
		/* vdin1 drop 1 frame */
		devp->vdin_drop_num = 1;
	}

	INIT_DELAYED_WORK(&devp->dv.dv_dwork, vdin_dv_dwork);
	INIT_DELAYED_WORK(&devp->vlock_dwork, vdin_vlock_dwork);
	/*vdin event*/
	INIT_DELAYED_WORK(&devp->event_dwork, vdin_event_work);
	/*vdin_extcon_register(pdev, devp);*/
	/*init queue*/
	init_waitqueue_head(&devp->queue);

	vdin_mif_config_init(devp); /* 2019-0425 add, ensure mif/afbc bit */
	vdin_debugfs_init(devp);/*2018-07-18 add debugfs*/
	if (devp->index)
		vd_signal_register_client(&vdin_signal_notifier);
	if (!devp->index) {
		#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vout_register_client(&vdin_get_vinfo_notifier);
		#endif
	}

	pr_info("%s: driver initialized ok\n", __func__);
	return 0;

fail_alloc_vf_pool:
fail_get_resource_irq:
fail_create_dev_file:
	vdin_delete_device(devp->index);
fail_create_device:
	cdev_del(&devp->cdev);
fail_add_cdev:
	kfree(devp);
fail_kzalloc_vdev:
	return ret;
}

/*this function is used for removing driver
 *	free the vframe pool
 *	remove device files
 *	delete vdinx device
 *	free drvdata
 */
static int vdin_drv_remove(struct platform_device *pdev)
{
	int ret;
	struct vdin_dev_s *devp;

	devp = platform_get_drvdata(pdev);

	if (!devp->index) {
		#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vout_unregister_client(&vdin_get_vinfo_notifier);
		#endif
	}
	ret = cancel_delayed_work(&devp->vlock_dwork);
	ret = cancel_delayed_work(&devp->event_dwork);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (!devp->rdma_not_register)
		rdma_unregister(devp->rdma_handle);
#endif
	mutex_destroy(&devp->fe_lock);

	vdin_debugfs_exit(devp);/*2018-07-18 add debugfs*/
	vf_pool_free(devp->vfp);
	vdin_rm_dev_class_files(devp->dev);
	vdin_delete_device(devp->index);
	cdev_del(&devp->cdev);
	vdin_devp[devp->index] = NULL;

	/* free drvdata */
	dev_set_drvdata(devp->dev, NULL);
	platform_set_drvdata(pdev, NULL);
	kfree(devp);
	pr_info("%s: driver removed ok\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
struct cpumask vdin_irq_mask;
static int vdin_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct vdin_dev_s *devp;
	//struct irq_desc *desc;
	//const struct cpumask *mask;

	devp = platform_get_drvdata(pdev);

	//if (devp->irq) {
	//	desc = irq_to_desc((long)devp->irq);
	//	mask = desc->irq_data.common->affinity;
#ifdef CONFIG_GENERIC_PENDING_IRQ
	//	if (irqd_is_setaffinity_pending(&desc->irq_data))
	//		mask = desc->pending_mask;
#endif
	//cpumask_copy(&vdin_irq_mask, mask);
	//}

	devp->flags |= VDIN_FLAG_SUSPEND;
	/*no need setting any regs*/
	/*vdin_enable_module(devp, false);*/

	/* disable clock of blackbar, histogram, histogram, line fifo1, matrix,
	 * hscaler, pre hscaler, clock0
	 */
	/* [15:14]  Disable blackbar clock      = 01/(auto, off, on, on) */
	/* [13:12]  Disable histogram clock     = 01/(auto, off, on, on) */
	/* [11:10]  Disable line fifo1 clock    = 01/(auto, off, on, on) */
	/* [ 9: 8]  Disable matrix clock        = 01/(auto, off, on, on) */
	/* [ 7: 6]  Disable hscaler clock       = 01/(auto, off, on, on) */
	/* [ 5: 4]  Disable pre hscaler clock   = 01/(auto, off, on, on) */
	/* [ 3: 2]  Disable clock0              = 01/(auto, off, on, on) */
	/* [    0]  Enable register clock       = 00/(auto, off!!!!!!!!) */
	/*switch_vpu_clk_gate_vmod(devp->addr_offset == 0
	 *? VPU_VIU_VDIN0 : VPU_VIU_VDIN1,
	 *VPU_CLK_GATE_OFF);
	 */
	if (devp->index && devp->set_canvas_manual == 1 &&
	    is_meson_t5d_cpu()) {
		wr_bits(devp->addr_offset, VDIN_COM_CTRL0, 0,
			VDIN_COMMON_INPUT_EN_BIT, VDIN_COMMON_INPUT_EN_WID);
		wr_bits(devp->addr_offset, VDIN_COM_CTRL0, 0,
			VDIN_SEL_BIT, VDIN_SEL_WID);
	}
	/* Restarting vdin1 clock will crash */
	if (!devp->index || devp->dtdata->hw_ver < VDIN_HW_T7)
		vdin_clk_on_off(devp, false);
	/* vdin msr clock gate disable */
	if (!devp->index && devp->msr_clk &&
		devp->vdin_clk_flag)
		clk_disable_unprepare(devp->msr_clk);

	pr_info("%s vdin-id:%d ok.\n", __func__, devp->index);
	return 0;
}

static int vdin_drv_resume(struct platform_device *pdev)
{
	struct vdin_dev_s *devp;

	devp = platform_get_drvdata(pdev);
	/*no need resume anything*/
	/*vdin_enable_module(devp, true);*/

	/* enable clock of blackbar, histogram, histogram, line fifo1, matrix,
	 * hscaler, pre hscaler, clock0
	 */
	/* [15:14]  Enable blackbar clock       = 00/(auto, off, on, on) */
	/* [13:12]  Enable histogram clock      = 00/(auto, off, on, on) */
	/* [11:10]  Enable line fifo1 clock     = 00/(auto, off, on, on) */
	/* [ 9: 8]  Enable matrix clock         = 00/(auto, off, on, on) */
	/* [ 7: 6]  Enable hscaler clock        = 00/(auto, off, on, on) */
	/* [ 5: 4]  Enable pre hscaler clock    = 00/(auto, off, on, on) */
	/* [ 3: 2]  Enable clock0               = 00/(auto, off, on, on) */
	/* [    0]  Enable register clock       = 00/(auto, off!!!!!!!!) */
	/*switch_vpu_clk_gate_vmod(devp->addr_offset == 0
	 *? VPU_VIU_VDIN0 : VPU_VIU_VDIN1,
	 *VPU_CLK_GATE_ON);
	 */
	/* Restarting vdin1 clock will crash */
	if (!devp->index || devp->dtdata->hw_ver < VDIN_HW_T7)
		vdin_clk_on_off(devp, true);

	if (devp->index && devp->set_canvas_manual == 1 &&
	    is_meson_t5d_cpu()) {
		wr_bits(devp->addr_offset, VDIN_COM_CTRL0, 7,
			VDIN_SEL_BIT, VDIN_SEL_WID);
		wr_bits(devp->addr_offset, VDIN_COM_CTRL0, 1,
			VDIN_COMMON_INPUT_EN_BIT, VDIN_COMMON_INPUT_EN_WID);
	}

	//if (devp->irq) {
	//	if (!irq_can_set_affinity(devp->irq))
	//		return -EIO;
	//	if (cpumask_intersects(&vdin_irq_mask, cpu_online_mask))
	//		irq_set_affinity_hint(devp->irq, &vdin_irq_mask);
	//}

	devp->flags &= (~VDIN_FLAG_SUSPEND);
	pr_info("%s id:%d ok.\n", __func__, devp->index);
	return 0;
}
#endif

static void vdin_drv_shutdown(struct platform_device *pdev)
{
	struct vdin_dev_s *devp;

	devp = platform_get_drvdata(pdev);

	mutex_lock(&devp->fe_lock);
	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
		vdin_stop_dec(devp);
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		pr_info("VDIN(%d) decode stop ok at %s, flags=0x%x\n",
			devp->index, __func__, devp->flags);
	} else {
		pr_info("VDIN(%d) decode %s flags=0x%x\n",
			devp->index, __func__, devp->flags);
	}
	mutex_unlock(&devp->fe_lock);

	devp->flags |= VDIN_FLAG_SM_DISABLE;
	vdin_enable_module(devp, false);
	pr_info("%s ok.\n", __func__);
}

static struct platform_driver vdin_driver = {
	.probe		= vdin_drv_probe,
	.remove		= vdin_drv_remove,
#ifdef CONFIG_PM
	.suspend	= vdin_drv_suspend,
	.resume		= vdin_drv_resume,
#endif
	.shutdown   = vdin_drv_shutdown,
	.driver	= {
		.name	        = VDIN_DRV_NAME,
		.of_match_table = vdin_dt_match,
	}
};

/* extern int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops); */
/* extern void vdin_unreg_v4l2(void); */
int __init vdin_drv_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&vdin_devno, 0, VDIN_MAX_DEVS, VDIN_DEV_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	pr_info("%s: major %d ver:%s\n", __func__, MAJOR(vdin_devno), VDIN_VER);

	vdin_class = class_create(THIS_MODULE, VDIN_CLS_NAME);
	if (IS_ERR_OR_NULL(vdin_class)) {
		ret = PTR_ERR(vdin_class);
		pr_err("%s: failed to create class or ret=NULL\n", __func__);
		goto fail_class_create;
	}
#ifdef DEBUG_SUPPORT
	ret = vdin_create_class_files(vdin_class);
	if (ret != 0) {
		pr_err("%s: failed to create class !!\n", __func__);
		goto fail_pdrv_register;
	}
#endif
	ret = platform_driver_register(&vdin_driver);

	if (ret != 0) {
		pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	/*register vdin for v4l2 interface*/
	if (vdin_reg_v4l2(&vdin_4v4l2_ops))
		pr_err("[vdin] %s: register vdin v4l2 error.\n", __func__);
	pr_info("%s: vdin driver init done\n", __func__);
	return 0;

fail_pdrv_register:
	class_destroy(vdin_class);
fail_class_create:
	unregister_chrdev_region(vdin_devno, VDIN_MAX_DEVS);
fail_alloc_cdev_region:
	return ret;
}

void __exit vdin_drv_exit(void)
{
	vdin_unregister_v4l2();
#ifdef DEBUG_SUPPORT
	vdin_remove_class_files(vdin_class);
#endif
	class_destroy(vdin_class);
	unregister_chrdev_region(vdin_devno, VDIN_MAX_DEVS);
	platform_driver_unregister(&vdin_driver);
}

static int vdin_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	s32 ret = 0;

	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	mem_start = rmem->base;
	mem_end = rmem->base + rmem->size - 1;
	if (rmem->size >= 0x1100000)
		use_reserved_mem = 1;
	pr_info("init vdin mem source 0x%lx->0x%lx use_reserved_mem:%d\n",
		mem_start, mem_end, use_reserved_mem);

	return 0;
}

static const struct reserved_mem_ops rmem_vdin_ops = {
	.device_init = vdin_mem_device_init,
};

static int __init vdin_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vdin_ops;
	pr_info("vdin share mem setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(vdin, "amlogic, vdin_memory", vdin_mem_setup);

//MODULE_VERSION(VDIN_VER);

//MODULE_DESCRIPTION("AMLOGIC VDIN Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");
