// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_sm.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/extcon.h>
#include <linux/workqueue.h>

/* Amlogic Headers */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/* Local Headers */
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"
#include "vdin_sm.h"
#include "vdin_ctl.h"
#include "vdin_drv.h"

/* Stay in TVIN_SIG_STATE_NOSIG for some
 * cycles => be sure TVIN_SIG_STATE_NOSIG
 */
#define NOSIG_MAX_CNT 8
/* Stay in TVIN_SIG_STATE_UNSTABLE for some
 * cycles => be sure TVIN_SIG_STATE_UNSTABLE
 */
#define UNSTABLE_MAX_CNT 2/* 4 */
/* Have signal for some cycles  => exit TVIN_SIG_STATE_NOSIG */
#define EXIT_NOSIG_MAX_CNT 2/* 1 */
/* No signal for some cycles  => back to TVAFE_STATE_NOSIG */
#define BACK_NOSIG_MAX_CNT 24 /* 8 */
/* Signal unstable for some cycles => exit TVAFE_STATE_STABLE */
#define EXIT_STABLE_MAX_CNT 1
/* Signal stable for some cycles  => back to TVAFE_STATE_STABLE */
/* must >=500ms,for new api function */
#define BACK_STABLE_MAX_CNT 50
#define EXIT_PRESTABLE_MAX_CNT 50
static struct tvin_sm_s sm_dev[VDIN_MAX_DEVS];

static int sm_print_nosig;
static int sm_print_notsup;
static int sm_print_unstable;
static int sm_print_fmt_nosig;
static int sm_print_fmt_chg;
static int sm_atv_prestable_fmt;
static int sm_print_prestable;

/*bit0:general debug bit;bit1:hdmirx color change*/
static unsigned int sm_debug_enable = VDIN_SM_LOG_L_1;
module_param(sm_debug_enable, uint, 0664);
MODULE_PARM_DESC(sm_debug_enable,
		 "enable/disable state machine debug message");

static int back_nosig_max_cnt = BACK_NOSIG_MAX_CNT;
static int atv_unstable_in_cnt = 45;
static int atv_unstable_out_cnt = 50;
static int hdmi_unstable_out_cnt = 1;
static int hdmi_stable_out_cnt = 1;/* 25; */
/* new add in gxtvbb@20160523,reason:
 *gxtvbb add atv snow config,the config will affect signal detect.
 *if atv_stable_out_cnt < 100,the signal state will change
 *after swich source to atv or after atv search
 */
static int atv_stable_out_cnt = 100;
/* new add in gxtvbb@20160613,reason:
 *gxtvbb add atv snow config,the config will affect signal detect.
 *ensure after fmt change,the new fmt can be detect in time!
 */
static int atv_stable_fmt_check_cnt = 10;
/* new add in gxtvbb@20160613,reason:
 * ensure vdin fmt can update when fmt is changed in menu
 */
static int atv_stable_fmt_check_enable;
/* new add in gxtvbb@20160523,reason:
 *gxtvbb add atv snow config,the config will affect signal detect.
 *ensure after prestable into stable,the state is really stable!
 */
static int atv_prestable_out_cnt = 50;
static int other_stable_out_cnt = EXIT_STABLE_MAX_CNT;
static int other_unstable_out_cnt = BACK_STABLE_MAX_CNT;
static int manual_unstable_out_cnt = 30;
static int other_unstable_in_cnt = UNSTABLE_MAX_CNT;
static int nosig_in_cnt = NOSIG_MAX_CNT;
static int nosig2_unstable_cnt = EXIT_NOSIG_MAX_CNT;
bool manual_flag;

u32 vdin_re_config = (RE_CONFIG_DV_EN | RE_CONFIG_HDR_EN);
module_param(vdin_re_config, int, 0664);
MODULE_PARM_DESC(vdin_re_config, "vdin_re_config");

u32 vdin_re_cfg_drop_cnt = 8;
module_param(vdin_re_cfg_drop_cnt, int, 0664);
MODULE_PARM_DESC(vdin_re_cfg_drop_cnt, "vdin_re_cfg_drop_cnt");

/*#define DEBUG_SUPPORT*/
#ifdef DEBUG_SUPPORT
module_param(back_nosig_max_cnt, int, 0664);
MODULE_PARM_DESC(back_nosig_max_cnt,
		 "unstable enter nosignal state max count");

module_param(atv_unstable_in_cnt, int, 0664);
MODULE_PARM_DESC(atv_unstable_in_cnt, "atv_unstable_in_cnt");

module_param(atv_unstable_out_cnt, int, 0664);
MODULE_PARM_DESC(atv_unstable_out_cnt, "atv_unstable_out_cnt");

module_param(hdmi_unstable_out_cnt, int, 0664);
MODULE_PARM_DESC(hdmi_unstable_out_cnt, "hdmi_unstable_out_cnt");

module_param(hdmi_stable_out_cnt, int, 0664);
MODULE_PARM_DESC(hdmi_stable_out_cnt, "hdmi_stable_out_cnt");

module_param(atv_stable_out_cnt, int, 0664);
MODULE_PARM_DESC(atv_stable_out_cnt, "atv_stable_out_cnt");

module_param(atv_stable_fmt_check_cnt, int, 0664);
MODULE_PARM_DESC(atv_stable_fmt_check_cnt, "atv_stable_fmt_check_cnt");

module_param(atv_prestable_out_cnt, int, 0664);
MODULE_PARM_DESC(atv_prestable_out_cnt, "atv_prestable_out_cnt");

module_param(other_stable_out_cnt, int, 0664);
MODULE_PARM_DESC(other_stable_out_cnt, "other_stable_out_cnt");

module_param(other_unstable_out_cnt, int, 0664);
MODULE_PARM_DESC(other_unstable_out_cnt, "other_unstable_out_cnt");

module_param(other_unstable_in_cnt, int, 0664);
MODULE_PARM_DESC(other_unstable_in_cnt, "other_unstable_in_cnt");

module_param(nosig_in_cnt, int, 0664);
MODULE_PARM_DESC(nosig_in_cnt, "nosig_in_cnt");
#endif

module_param(nosig2_unstable_cnt, int, 0664);
MODULE_PARM_DESC(nosig2_unstable_cnt, "nosig2_unstable_cnt");

static int signal_status = TVIN_SIG_STATUS_NULL;
module_param(signal_status, int, 0664);
MODULE_PARM_DESC(signal_status, "signal_status");

static unsigned int vdin_dv_chg_cnt = 1;
module_param(vdin_dv_chg_cnt, uint, 0664);
MODULE_PARM_DESC(vdin_dv_chg_cnt, "vdin_dv_chg_cnt");

static unsigned int vdin_hdr_chg_cnt = 1;
module_param(vdin_hdr_chg_cnt, uint, 0664);
MODULE_PARM_DESC(vdin_hdr_chg_cnt, "vdin_hdr_chg_cnt");

static unsigned int vdin_vrr_chg_cnt = 1;
module_param(vdin_vrr_chg_cnt, uint, 0664);
MODULE_PARM_DESC(vdin_vrr_chg_cnt, "vdin_vrr_chg_cnt");

enum tvin_color_fmt_range_e
	tvin_get_force_fmt_range(enum tvin_color_fmt_e color_fmt)
{
	u32 fmt_range = TVIN_YUV_FULL;

	if (color_fmt == TVIN_YUV444 ||
	    color_fmt == TVIN_YUV422) {
		if (color_range_force == COLOR_RANGE_FULL)
			fmt_range = TVIN_YUV_FULL;
		else if (color_range_force == COLOR_RANGE_LIMIT)
			fmt_range = TVIN_YUV_LIMIT;
		else
			fmt_range = TVIN_YUV_FULL;
	} else if (color_fmt == TVIN_RGB444) {
		if (color_range_force == COLOR_RANGE_FULL)
			fmt_range = TVIN_RGB_FULL;
		else if (color_range_force == COLOR_RANGE_LIMIT)
			fmt_range = TVIN_RGB_LIMIT;
		else
			fmt_range = TVIN_RGB_FULL;
	}
	return fmt_range;
}

void vdin_update_prop(struct vdin_dev_s *devp)
{
	/*devp->pre_prop.fps = devp->prop.fps;*/
	devp->dv.dv_flag = devp->prop.dolby_vision;
	/*devp->pre_prop.latency.allm_mode = devp->prop.latency.allm_mode;*/
	/*devp->pre_prop.aspect_ratio = devp->prop.aspect_ratio;*/
	devp->parm.info.aspect_ratio = devp->prop.aspect_ratio;
	memcpy(&devp->pre_prop, &devp->prop,
	       sizeof(struct tvin_sig_property_s));
}

/*
 * check hdmirx color format
 */
static enum tvin_sg_chg_flg vdin_hdmirx_fmt_chg_detect(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	enum tvin_port_e port = TVIN_PORT_NULL;
	enum tvin_color_fmt_e cur_color_fmt, pre_color_fmt;
	/*enum tvin_color_fmt_e cur_dest_color_fmt, pre_dest_color_fmt;*/
	struct tvin_sig_property_s *prop, *pre_prop;
	unsigned int vdin_hdr_flag, pre_vdin_hdr_flag;
	unsigned int vdin_fmt_range, pre_vdin_fmt_range;
	unsigned int cur_dv_flag, pre_dv_flag;
	unsigned int temp;
	enum tvin_sg_chg_flg signal_chg = TVIN_SIG_CHG_NONE;

	if (!devp) {
		return signal_chg;
	}
	if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return signal_chg;
	}

	prop = &devp->prop;
	pre_prop = &devp->pre_prop;
	sm_ops = devp->frontend->sm_ops;
	port = devp->parm.port;

	if (port < TVIN_PORT_HDMI0 || port > TVIN_PORT_HDMI7)
		return signal_chg;

	if (sm_ops->get_sig_property) {
		/*if (!(devp->flags & VDIN_FLAG_ISR_EN))*/
		/*	sm_ops->get_sig_property(devp->frontend, prop);*/

		cur_color_fmt = prop->color_format;
		pre_color_fmt = pre_prop->color_format;
		/*cur_dest_color_fmt = prop->dest_cfmt;*/
		/*pre_dest_color_fmt = pre_prop->dest_cfmt;*/

		vdin_hdr_flag = prop->vdin_hdr_flag;
		pre_vdin_hdr_flag = pre_prop->vdin_hdr_flag;
		if (vdin_hdr_flag != pre_vdin_hdr_flag) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				prop->hdr_info.hdr_check_cnt++;
			if (prop->hdr_info.hdr_check_cnt >=
			    vdin_hdr_chg_cnt) {
				prop->hdr_info.hdr_check_cnt = 0;
				signal_chg |= vdin_hdr_flag ?
					TVIN_SIG_CHG_SDR2HDR :
					TVIN_SIG_CHG_HDR2SDR;
				if (signal_chg &&
				    (sm_debug_enable & VDIN_SM_LOG_L_2))
					pr_info("%s hdr chg 0x%x:(0x%x->0x%x)\n",
						__func__,
						signal_chg, pre_vdin_hdr_flag,
						vdin_hdr_flag);
				pre_prop->vdin_hdr_flag = prop->vdin_hdr_flag;
			}
		}

		cur_dv_flag = prop->dolby_vision;
		pre_dv_flag = devp->dv.dv_flag;
		if (cur_dv_flag != pre_dv_flag) {
			if (devp->dv.chg_cnt > vdin_dv_chg_cnt) {
				devp->dv.chg_cnt = 0;
				signal_chg |= cur_dv_flag ? TVIN_SIG_CHG_NO2DV :
						TVIN_SIG_CHG_DV2NO;
				if (signal_chg &&
				    (sm_debug_enable & VDIN_SM_LOG_L_2))
					pr_info("%s dv chg0x%x:(0x%x->0x%x)\n",
						__func__,
						signal_chg, pre_dv_flag,
						cur_dv_flag);
				pre_prop->dolby_vision = prop->dolby_vision;
				devp->dv.dv_flag = prop->dolby_vision;
			}
		}

		if (devp->pre_prop.latency.allm_mode !=
		    devp->prop.latency.allm_mode) {
			if (devp->dv.allm_chg_cnt > vdin_dv_chg_cnt) {
				devp->dv.allm_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				temp = devp->pre_prop.latency.allm_mode;
				if (signal_chg)
					pr_info("%s allm chg:(0x%x->0x%x)\n",
						__func__,
						temp,
						devp->prop.latency.allm_mode);
				devp->pre_prop.latency.allm_mode =
					devp->prop.latency.allm_mode;
			}
		}

		if (devp->pre_prop.latency.it_content !=
		    devp->prop.latency.it_content) {
			if (devp->dv.allm_chg_cnt > vdin_dv_chg_cnt) {
				devp->dv.allm_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				temp = devp->pre_prop.latency.it_content;
				if (signal_chg)
					pr_info("%s it_content chg:(0x%x->0x%x)\n",
						__func__,
						temp,
						devp->prop.latency.it_content);
				devp->pre_prop.latency.it_content =
					devp->prop.latency.it_content;
			}
		}

		if (devp->pre_prop.latency.cn_type !=
		    devp->prop.latency.cn_type) {
			if (devp->dv.allm_chg_cnt > vdin_dv_chg_cnt) {
				devp->dv.allm_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				temp = devp->pre_prop.latency.cn_type;
				if (signal_chg)
					pr_info("%s cn_type chg:(0x%x->0x%x)\n",
						__func__,
						temp,
						devp->prop.latency.cn_type);
				devp->pre_prop.latency.cn_type =
					devp->prop.latency.cn_type;
			}
		}

		if (devp->pre_prop.fps != devp->prop.fps) {
			signal_chg |= TVIN_SIG_CHG_VS_FRQ;
			pr_info("%s fps chg:(0x%x->0x%x)\n", __func__,
				devp->pre_prop.fps,
				devp->prop.fps);
			devp->pre_prop.fps = devp->prop.fps;
			devp->parm.info.fps = devp->prop.fps;
		}

		if (devp->pre_prop.aspect_ratio !=
		     devp->prop.aspect_ratio &&
		    IS_HDMI_SRC(devp->parm.port)) {
			if (devp->sg_chg_afd_cnt > 1) {
				signal_chg |= TVIN_SIG_CHG_AFD;
				if (signal_chg)
					pr_info("%s afd chg:(0x%x->0x%x)\n",
						__func__,
						devp->pre_prop.aspect_ratio,
						devp->prop.aspect_ratio);
				devp->pre_prop.aspect_ratio =
						    devp->prop.aspect_ratio;
				devp->parm.info.aspect_ratio =
					prop->aspect_ratio;
			}
		}

		if (devp->vrr_data.vdin_vrr_en_flag != devp->prop.vtem_data.vrr_en ||
			vdin_check_spd_data_chg(devp)) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				devp->vrr_data.vrr_chg_cnt++;
			if (devp->vrr_data.vrr_chg_cnt > vdin_vrr_chg_cnt) {
				devp->vrr_data.vrr_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_VRR;
				if (signal_chg && (sm_debug_enable & VDIN_SM_LOG_L_1))
					pr_info("%s vrr_chg:(%d->%d) spd:([5]%d->%d) [0](%d->%d)\n",
						__func__,
						devp->vrr_data.vdin_vrr_en_flag,
						devp->prop.vtem_data.vrr_en,
						devp->pre_prop.spd_data.data[5],
						devp->prop.spd_data.data[5],
						devp->pre_prop.spd_data.data[0],
						devp->prop.spd_data.data[0]);
				devp->pre_prop.spd_data.data[5] = devp->prop.spd_data.data[5];
			}
		}

		if (color_range_force)
			prop->color_fmt_range =
			tvin_get_force_fmt_range(pre_prop->color_format);
		vdin_fmt_range = prop->color_fmt_range;
		pre_vdin_fmt_range = pre_prop->color_fmt_range;

		if (devp->flags & VDIN_FLAG_DEC_STARTED &&
		    (cur_color_fmt != pre_color_fmt ||
		     vdin_fmt_range != pre_vdin_fmt_range)) {
			if (sm_debug_enable & VDIN_SM_LOG_L_1)
				pr_info("[smr.%d] fmt(%d->%d), fmt_range(%d->%d), csc_cfg:0x%x\n",
					devp->index,
					pre_color_fmt, cur_color_fmt,
					pre_vdin_fmt_range, vdin_fmt_range,
					devp->csc_cfg);
			vdin_get_format_convert(devp);
			devp->csc_cfg = 1;
		}
	}

	return signal_chg;
}

/* check auto de to adjust vdin cutwindow */
void vdin_auto_de_handler(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_sig_property_s *prop;
	unsigned int cur_vs, cur_ve, pre_vs, pre_ve;
	unsigned int cur_hs, cur_he, pre_hs, pre_he;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}
	if (devp->auto_cutwindow_en == 0)
		return;
	prop = &devp->prop;
	sm_ops = devp->frontend->sm_ops;
	if ((devp->flags & VDIN_FLAG_DEC_STARTED) &&
	    sm_ops->get_sig_property) {
		sm_ops->get_sig_property(devp->frontend, prop);
		cur_vs = prop->vs;
		cur_ve = prop->ve;
		cur_hs = prop->hs;
		cur_he = prop->he;
		pre_vs = prop->pre_vs;
		pre_ve = prop->pre_ve;
		pre_hs = prop->pre_hs;
		pre_he = prop->pre_he;
		if (pre_vs != cur_vs || pre_ve != cur_ve ||
		    pre_hs != cur_hs || pre_he != cur_he) {
			if (sm_debug_enable & VDIN_SM_LOG_L_4)
				pr_info("[smr.%d] pre_vs(%d->%d),pre_ve(%d->%d),pre_hs(%d->%d),pre_he(%d->%d),cutwindow_cfg:0x%x\n",
					devp->index, pre_vs, cur_vs, pre_ve, cur_ve,
					pre_hs, cur_hs, pre_he, cur_he,
					devp->cutwindow_cfg);
			devp->cutwindow_cfg = 1;
		}
	}
}

void tvin_smr_init_counter(int index)
{
	sm_dev[index].state_cnt          = 0;
	sm_dev[index].exit_nosig_cnt     = 0;
	sm_dev[index].back_nosig_cnt     = 0;
	sm_dev[index].back_stable_cnt    = 0;
	sm_dev[index].exit_prestable_cnt = 0;
}

u32 tvin_hdmirx_signal_type_check(struct vdin_dev_s *devp)
{
	unsigned int signal_type = devp->parm.info.signal_type;
	enum tvin_sg_chg_flg signal_chg = TVIN_SIG_CHG_NONE;
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_sig_property_s *prop;

	/* need always polling the signal property, if isr enable,
	 * it be called in isr
	 */
	prop = &devp->prop;
	if (!(devp->flags & VDIN_FLAG_ISR_EN) && devp->frontend) {
		sm_ops = devp->frontend->sm_ops;
		if (sm_ops && sm_ops->get_sig_property) {
			if (IS_TVAFE_SRC(devp->parm.port) ||
			    vdin_get_prop_in_sm_en)
				sm_ops->get_sig_property(devp->frontend,
					&devp->prop);
			/*devp->dv.dv_flag = devp->prop.dolby_vision;*/
		}
	}

	if (prop->low_latency != devp->dv.low_latency) {
		devp->dv.low_latency = prop->low_latency;
		if ((devp->dtdata->hw_ver == VDIN_HW_TM2 ||
		     devp->dtdata->hw_ver == VDIN_HW_TM2_B) &&
		    prop->dolby_vision) {
			signal_chg |= TVIN_SIG_DV_CHG;
			if (sm_debug_enable & VDIN_SM_LOG_L_2)
				pr_info("[sm.%d]dv.ll:%d prop.ll:%d\n",
					devp->index, devp->dv.low_latency,
					prop->low_latency);
		}
	}
	memcpy(&devp->dv.dv_vsif,
	       &prop->dv_vsif, sizeof(struct tvin_dv_vsif_s));

	if (sm_debug_enable & VDIN_SM_LOG_L_4)
		pr_info("[sm.%d]dv:%d, hdr state:%d eotf:%d flag:0x%x, vrr state:%d\n",
			devp->index,
			devp->prop.dolby_vision,
			devp->prop.hdr_info.hdr_state,
			devp->prop.hdr_info.hdr_data.eotf,
			devp->prop.vdin_hdr_flag,
			devp->prop.vdin_vrr_flag);

	if (devp->prop.dolby_vision)
		signal_type |= (1 << 30);
	else
		signal_type &= ~(1 << 30);
	/* check dv end */

	/* check HDR/HLG begin */
	if (prop->hdr_info.hdr_state == HDR_STATE_GET) {
		if (vdin_hdr_sei_error_check(devp) == 1) {
			/*devp->prop.vdin_hdr_flag = false;*/
			signal_type &= ~(1 << 29);
			signal_type &= ~(1 << 25);
			/* default is bt709,if change need sync */
			signal_type = ((1 << 16) |
				       (signal_type & (~0xFF0000)));
			signal_type = ((1 << 8) | (signal_type & (~0xFF00)));
		} else {
			devp->prop.vdin_hdr_flag = true;
			if (prop->hdr_info.hdr_data.eotf ==
			    EOTF_SMPTE_ST_2048 ||
			    prop->hdr_info.hdr_data.eotf == EOTF_HDR) {
				signal_type |= (1 << 29);
				signal_type |= (0 << 25);/* 0:limit */
				signal_type = ((9 << 16) |
					(signal_type & (~0xFF0000)));
				signal_type = ((16 << 8) |
					(signal_type & (~0xFF00)));
				signal_type = ((9 << 0) |
					(signal_type & (~0xFF)));
			} else if (devp->prop.hdr_info.hdr_data.eotf ==
				   EOTF_HLG) {
				signal_type |= (1 << 29);
				signal_type |= (0 << 25);/* 0:limit */
				signal_type = ((9 << 16) |
					(signal_type & (~0xFF0000)));
				signal_type = ((14 << 8) |
					(signal_type & (~0xFF00)));
				signal_type = ((9 << 0) |
					(signal_type & (~0xFF)));
			} else {
				if (devp->prop.hdr_info.hdr_data.eotf ==
				    EOTF_SDR)
					devp->prop.vdin_hdr_flag = false;

				signal_type &= ~(1 << 29);
				signal_type &= ~(1 << 25);
				/* default is bt709,if change need sync */
				signal_type = ((1 << 16) |
					(signal_type & (~0xFF0000)));
				signal_type = ((1 << 8) |
					(signal_type & (~0xFF00)));
			}
		}
		devp->prop.hdr_info.hdr_state = HDR_STATE_SET;
	} else if (prop->hdr_info.hdr_state == HDR_STATE_NULL) {
		devp->prop.vdin_hdr_flag = false;
		signal_type &= ~(1 << 29);
		signal_type &= ~(1 << 25);
		signal_type |= (0 << 25);/* 0:limit */
		/* default is bt709,if change need sync */
		signal_type = ((1 << 16) | (signal_type & (~0xFF0000)));
		signal_type = ((1 << 8) | (signal_type & (~0xFF00)));
	}
	/* check HDR/HLG end */

	/* check HDR 10+ begin */
	if (prop->hdr10p_info.hdr10p_on) {
		devp->prop.vdin_hdr_flag = true;

		signal_type |= (1 << 29);/* present_flag */
		signal_type |= (0 << 25);/* 0:limited */

		/* color_primaries */
		signal_type = ((9 << 16) | (signal_type & (~0xFF0000)));

		/*transfer_characteristic*/
		signal_type = ((0x30 << 8) | (signal_type & (~0xFF00)));

		/* matrix_coefficient */
		signal_type = ((9 << 0) | (signal_type & (~0xFF)));
	}

	if (sm_debug_enable & VDIN_SM_LOG_L_4)
		pr_info("[sm.%d] hdr flag:0x%x signal_type:0x%x\n",
			devp->index,
			devp->prop.vdin_hdr_flag, signal_type);

	if (devp->prop.vdin_hdr_flag &&
	    devp->parm.info.signal_type != signal_type) {
		signal_chg |= TVIN_SIG_CHG_SDR2HDR;
	}
	/* check HDR 10+ end */

	/* check vrr begin */
	if (devp->prop.vdin_vrr_flag)
		signal_type |= (1 << 31);
	else
		signal_type &= ~(1 << 31);
	/* check vrr end */

	devp->parm.info.signal_type = signal_type;

	return signal_chg;
}

void reset_tvin_smr(unsigned int index)
{
	sm_dev[index].sig_status = TVIN_SIG_STATUS_NULL;
}

void tvin_sigchg_event_process(struct vdin_dev_s *devp, u32 chg)
{
	/*struct tvin_sm_s *sm_p;*/
	bool re_cfg = 0;

	/*avoid when doing start dec, hdr or dv change re-config coming*/
	if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
		/* record starting need re_cfg status */
		if (chg) {
			devp->starting_chg = chg;
			pr_info("starting_chg:0X%x\n", devp->starting_chg);
		}
		return;
	}

	if (devp->starting_chg) {
		pr_info("starting_chg send event:0X%x\n", devp->starting_chg);
		chg = devp->starting_chg;
		devp->starting_chg = 0;
	}

	if (chg & TVIN_SIG_CHG_STS) {
		devp->event_info.event_sts = TVIN_SIG_CHG_STS;
	} else {
		if (chg & TVIN_SIG_DV_CHG) {
			devp->event_info.event_sts = (chg & TVIN_SIG_DV_CHG);
			if (vdin_re_config & RE_CONFIG_DV_EN)
				re_cfg = true;
		} else if (chg & TVIN_SIG_HDR_CHG) {
			devp->event_info.event_sts = (chg & TVIN_SIG_HDR_CHG);
			if (vdin_re_config & RE_CONFIG_HDR_EN)
				re_cfg = true;
		} else if (chg & TVIN_SIG_CHG_COLOR_FMT) {
			devp->event_info.event_sts = TVIN_SIG_CHG_COLOR_FMT;
		} else if (chg & TVIN_SIG_CHG_RANGE) {
			devp->event_info.event_sts = TVIN_SIG_CHG_RANGE;
		} else if (chg & TVIN_SIG_CHG_BIT) {
			devp->event_info.event_sts = TVIN_SIG_CHG_BIT;
		} else if (chg & TVIN_SIG_CHG_VS_FRQ) {
			devp->event_info.event_sts = TVIN_SIG_CHG_VS_FRQ;
		} else if (chg & TVIN_SIG_CHG_DV_ALLM) {
			devp->event_info.event_sts = TVIN_SIG_CHG_DV_ALLM;
			if (vdin_re_config & RE_CONFIG_ALLM_EN)
				re_cfg = true;
		} else if (chg & TVIN_SIG_CHG_AFD) {
			devp->event_info.event_sts = TVIN_SIG_CHG_AFD;
		} else if (chg & TVIN_SIG_CHG_VRR) {
			devp->event_info.event_sts = TVIN_SIG_CHG_VRR;
			vdin_frame_lock_check(devp, 1);

			pr_info("%s vrr chg:(%d->%d) spd:(%d->%d)\n", __func__,
				devp->vrr_data.vdin_vrr_en_flag,
				devp->prop.vtem_data.vrr_en,
				devp->pre_prop.spd_data.data[5],
				devp->prop.spd_data.data[5]);
			devp->pre_prop.vtem_data.vrr_en =
				devp->prop.vtem_data.vrr_en;
			devp->vrr_data.vdin_vrr_en_flag =
				devp->prop.vtem_data.vrr_en;
		} else {
			return;
		}
	}

	if (re_cfg) {
		if (sm_debug_enable & VDIN_SM_LOG_L_1)
			pr_info("vdin reconfig, set unstable\n");
		devp->parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
		devp->frame_drop_num = vdin_re_cfg_drop_cnt;
	}
	devp->pre_event_info.event_sts = devp->event_info.event_sts;
	vdin_send_event(devp, devp->event_info.event_sts);
	wake_up(&devp->queue);
}

/*
 * tvin state machine routine
 *
 */
void tvin_smr(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_info_s *info;
	enum tvin_port_e port = TVIN_PORT_NULL;
	unsigned int unstb_in;
	struct tvin_sm_s *sm_p;
	struct tvin_frontend_s *fe;
	struct tvin_sig_property_s *prop, *pre_prop;
	enum tvin_sg_chg_flg signal_chg = TVIN_SIG_CHG_NONE;
	unsigned int cnt;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}

	if (devp->flags & VDIN_FLAG_SM_DISABLE ||
	    devp->flags & VDIN_FLAG_SUSPEND)
		return;

	if (!(devp->flags & VDIN_FLAG_DEC_OPENED))
		return;

	sm_p = &sm_dev[devp->index];
	fe = devp->frontend;
	sm_ops = devp->frontend->sm_ops;
	info = &devp->parm.info;
	port = devp->parm.port;
	prop = &devp->prop;
	pre_prop = &devp->pre_prop;

	signal_chg = tvin_hdmirx_signal_type_check(devp);

	switch (sm_p->state) {
	case TVIN_SM_STATUS_NOSIG:
		++sm_p->state_cnt;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
		if (IS_TVAFE_ATV_SRC(port) &&
		    (devp->flags & VDIN_FLAG_SNOW_FLAG))
			tvafe_snow_config_clamp(1);
#endif
		if (sm_ops->nosig(devp->frontend)) {
			sm_p->exit_nosig_cnt = 0;
			if (sm_p->state_cnt >= nosig_in_cnt) {
				sm_p->state_cnt = nosig_in_cnt;
				info->status = TVIN_SIG_STATUS_NOSIG;
				if (!(IS_TVAFE_ATV_SRC(port) &&
				      (devp->flags & VDIN_FLAG_SNOW_FLAG)))
					info->fmt = TVIN_SIG_FMT_NULL;
				if (sm_debug_enable && !sm_print_nosig) {
					pr_info("[smr.%d] no signal\n",
						devp->index);
					sm_print_nosig = 1;
				}
				sm_print_unstable = 0;
			}
		} else {
			if (IS_TVAFE_SRC(port))
				nosig2_unstable_cnt = 2;
			++sm_p->exit_nosig_cnt;
			if (sm_p->exit_nosig_cnt >= nosig2_unstable_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (sm_debug_enable) {
					pr_info("[smr.%d] no signal --> unstable\n",
						devp->index);
					pr_info("unstable_cnt:0x%x\n",
						nosig2_unstable_cnt);
				}
				if (IS_HDMI_SRC(port))
					nosig2_unstable_cnt = 5;

				sm_print_nosig  = 0;
				sm_print_unstable = 0;
			}
		}
		break;

	case TVIN_SM_STATUS_UNSTABLE:
		++sm_p->state_cnt;
		if (sm_ops->nosig(devp->frontend)) {
			sm_p->back_stable_cnt = 0;
			++sm_p->back_nosig_cnt;
			if (sm_p->back_nosig_cnt >= sm_p->back_nosig_max_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_NOSIG;
				info->status = TVIN_SIG_STATUS_NOSIG;
				if (!(IS_TVAFE_ATV_SRC(port) &&
				      (devp->flags & VDIN_FLAG_SNOW_FLAG)))
					info->fmt = TVIN_SIG_FMT_NULL;
				if (sm_debug_enable)
					pr_info("[smr.%d] unstable --> no signal\n",
						devp->index);
				sm_print_nosig  = 0;
				sm_print_unstable = 0;
			}
		} else {
			sm_p->back_nosig_cnt = 0;
			if (sm_ops->fmt_changed(devp->frontend)) {
				sm_p->back_stable_cnt = 0;
				if (IS_TVAFE_ATV_SRC(port) &&
				    devp->unstable_flag &&
				    (devp->flags & VDIN_FLAG_SNOW_FLAG))
					/* UNSTABLE_ATV_MAX_CNT; */
					unstb_in = sm_p->atv_unstable_in_cnt;
				else
					unstb_in = other_unstable_in_cnt;
				if (sm_p->state_cnt >= unstb_in) {
					sm_p->state_cnt  = unstb_in;
					info->status = TVIN_SIG_STATUS_UNSTABLE;
					/*info->fmt = TVIN_SIG_FMT_NULL;*/

					if (sm_debug_enable &&
					    !sm_print_unstable) {
						pr_info("[smr.%d] unstable\n",
							devp->index);
						sm_print_unstable = 1;
					}
					sm_print_nosig  = 0;
				}
			} else {
				++sm_p->back_stable_cnt;
				if (IS_TVAFE_ATV_SRC(port) &&
				    (devp->flags & VDIN_FLAG_SNOW_FLAG))
					unstb_in = sm_p->atv_unstable_out_cnt;
				else if (IS_TVAFE_ATV_SRC(port) && manual_flag)
					unstb_in = manual_unstable_out_cnt;
				else if (port >= TVIN_PORT_HDMI0 &&
					 port <= TVIN_PORT_HDMI7)
					unstb_in = sm_p->hdmi_unstable_out_cnt;
				else
					unstb_in = other_unstable_out_cnt;

				cnt = sizeof(struct tvin_sig_property_s);
				 /* must wait enough time for cvd signal lock */
				if (sm_p->back_stable_cnt >= unstb_in) {
					sm_p->back_stable_cnt = 0;
					sm_p->state_cnt = 0;
					if (sm_ops->get_fmt &&
					    sm_ops->get_sig_property) {
						info->fmt =
							sm_ops->get_fmt(fe);
						/*sm_ops->get_sig_property(fe,*/
						/*prop);*/
						info->cfmt = prop->color_format;
						memcpy(pre_prop, prop, cnt);
						devp->parm.info.trans_fmt =
							prop->trans_fmt;
						devp->parm.info.is_dvi =
							prop->dvi_info;
						devp->parm.info.fps =
							prop->fps;
					}

					if (sm_ops->fmt_config)
						sm_ops->fmt_config(fe);

					tvin_smr_init_counter(devp->index);
					sm_p->state = TVIN_SM_STATUS_PRESTABLE;
					sm_atv_prestable_fmt = info->fmt;

					if (sm_debug_enable) {
						pr_info("[smr.%d]unstable-->prestable",
							devp->index);
						pr_info("and format is %d(%s)\n",
							info->fmt,
						tvin_sig_fmt_str(info->fmt));
					}

					sm_print_nosig  = 0;
					sm_print_unstable = 0;
					sm_print_fmt_nosig = 0;
					sm_print_fmt_chg = 0;
					sm_print_prestable = 0;

				} else {
					info->status = TVIN_SIG_STATUS_UNSTABLE;

					if (sm_debug_enable &&
					    !sm_print_notsup) {
						pr_info("[smr.%d] unstable --> not support\n",
							devp->index);
						sm_print_notsup = 1;
					}
				}
			}
		}
		break;
	case TVIN_SM_STATUS_PRESTABLE: {
		bool nosig = false, fmt_changed = false;
		unsigned int prestable_out_cnt = 0;

		devp->unstable_flag = true;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
		if (IS_TVAFE_ATV_SRC(port) &&
		    (devp->flags & VDIN_FLAG_SNOW_FLAG))
			tvafe_snow_config_clamp(0);
#endif
		if (sm_ops->nosig(devp->frontend)) {
			nosig = true;
			if (sm_debug_enable && !(sm_print_prestable & 0x1)) {
				pr_info("[smr.%d] warning: no signal\n",
					devp->index);
				sm_print_prestable |= 1;
			}
		}

		if (sm_ops->fmt_changed(devp->frontend)) {
			fmt_changed = true;
			if (sm_debug_enable && !(sm_print_prestable & 0x2)) {
				pr_info("[smr.%d] warning: format changed\n",
					devp->index);
				sm_print_prestable |= (1 << 1);
			}
		}

		if (nosig || fmt_changed) {
			++sm_p->state_cnt;
			if (IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG))
				prestable_out_cnt = atv_prestable_out_cnt;
			else
				prestable_out_cnt = other_stable_out_cnt;
			if (sm_p->state_cnt >= prestable_out_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (sm_debug_enable)
					pr_info("[smr.%d] prestable --> unstable\n",
						devp->index);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_unstable = 0;
				sm_print_prestable = 0;
				break;
			}
		} else {
			sm_p->state_cnt = 0;

			if (IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG)) {
				++sm_p->exit_prestable_cnt;
				if (sm_p->exit_prestable_cnt <
				    atv_prestable_out_cnt)
					break;
				else
					sm_p->exit_prestable_cnt = 0;
			}

			sm_p->state = TVIN_SM_STATUS_STABLE;
			info->status = TVIN_SIG_STATUS_STABLE;
			vdin_update_prop(devp);
			/* tcl vrr case */
			devp->vrr_data.vdin_vrr_en_flag =
				devp->pre_prop.vtem_data.vrr_en;
			devp->csc_cfg = 0;
			devp->starting_chg = 0;
			if (sm_debug_enable)
				pr_info("[smr.%d] %ums prestable --> stable\n",
					devp->index, jiffies_to_msecs(jiffies));
			sm_print_nosig  = 0;
			sm_print_notsup = 0;
			sm_print_prestable = 0;
		}
		break;
	}
	case TVIN_SM_STATUS_STABLE: {
		bool nosig = false, fmt_changed = false;
		unsigned int stable_out_cnt = 0;
		unsigned int stable_fmt = 0;

		devp->unstable_flag = true;
		if (sm_ops->nosig(devp->frontend)) {
			nosig = true;
			if (sm_debug_enable && !sm_print_fmt_nosig) {
				pr_info("[smr.%d] warning: no signal\n",
					devp->index);
				sm_print_fmt_nosig = 1;
			}
		}

		if (sm_ops->fmt_changed(devp->frontend)) {
			fmt_changed = true;
			if (sm_debug_enable && !sm_print_fmt_chg) {
				pr_info("[smr.%d] warning: format changed\n",
					devp->index);
				sm_print_fmt_chg = 1;
			}
		}
		/* dynamic adjust cutwindow for atv test */
		if (IS_TVAFE_SRC(port))
			vdin_auto_de_handler(devp);

		if (IS_TVAFE_SRC(port) && sm_ops->get_sig_property) {
			sm_ops->get_sig_property(devp->frontend, prop);
			devp->parm.info.fps = prop->fps;
		}

		if (nosig || fmt_changed /* || !pll_lock */) {
			++sm_p->state_cnt;
			if (IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG))
				stable_out_cnt = sm_p->atv_stable_out_cnt;
			else if ((port >= TVIN_PORT_HDMI0) &&
				 (port <= TVIN_PORT_HDMI7))
				stable_out_cnt = hdmi_stable_out_cnt;
			else
				stable_out_cnt = other_stable_out_cnt;
			/*add for atv snow*/
			if (sm_p->state_cnt >= atv_stable_fmt_check_cnt &&
			    IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG))
				atv_stable_fmt_check_enable = 1;
			if (sm_p->state_cnt >= stable_out_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (sm_debug_enable)
					pr_info("[smr.%d] stable --> unstable\n",
						devp->index);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_unstable = 0;
				sm_print_fmt_nosig = 0;
				sm_print_fmt_chg = 0;
				sm_print_prestable = 0;
				atv_stable_fmt_check_enable = 0;
			}
		} else {
			/*add for atv snow*/
			if (IS_TVAFE_ATV_SRC(port) &&
			    atv_stable_fmt_check_enable &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG) &&
				(sm_ops->get_fmt && sm_ops->get_sig_property)) {
				sm_p->state_cnt = 0;
				stable_fmt = sm_ops->get_fmt(fe);
				if (sm_atv_prestable_fmt != stable_fmt &&
				    stable_fmt != TVIN_SIG_FMT_NULL) {
					/*cvbs in*/
					sm_ops->get_sig_property(fe, prop);
					memcpy(pre_prop, prop,
					       sizeof(struct
						      tvin_sig_property_s));
					devp->parm.info.trans_fmt =
						prop->trans_fmt;
					devp->parm.info.is_dvi =
						prop->dvi_info;
					devp->parm.info.fps =
						prop->fps;
					info->fmt = stable_fmt;
					atv_stable_fmt_check_enable = 0;
					if (sm_debug_enable)
						pr_info("[smr.%d] stable fmt changed:0x%x-->0x%x\n",
							devp->index,
							sm_atv_prestable_fmt,
							stable_fmt);
					sm_atv_prestable_fmt = stable_fmt;
				}
			}
			sm_p->state_cnt = 0;
			signal_chg |= vdin_hdmirx_fmt_chg_detect(devp);
			if (signal_chg || devp->starting_chg)
				tvin_sigchg_event_process(devp, signal_chg);
		}
		/* check unreliable vsync interrupt */
		if (devp->unreliable_vs_cnt != devp->unreliable_vs_cnt_pre) {
			devp->unreliable_vs_cnt_pre = devp->unreliable_vs_cnt;
			if (sm_debug_enable & 0x2)
				vdin_dump_vs_info(devp);
		}
		break;
	}
	case TVIN_SM_STATUS_NULL:
	default:
		sm_p->state = TVIN_SM_STATUS_NOSIG;
		break;
	}

	if (devp->flags & VDIN_FLAG_DEC_OPENED) {
		if (sm_p->sig_status != info->status) {
			sm_p->sig_status = info->status;
			devp->event_info.event_sts = TVIN_SIG_CHG_STS;
			devp->pre_event_info.event_sts =
				devp->event_info.event_sts;
			vdin_send_event(devp, devp->event_info.event_sts);
			wake_up(&devp->queue);
		}
	}

	signal_status = sm_p->sig_status;
}

/*
 * tvin state machine routine init
 *
 */

void tvin_smr_init(int index)
{
	sm_dev[index].sig_status = TVIN_SIG_STATUS_NULL;
	sm_dev[index].state = TVIN_SM_STATUS_NULL;
	sm_dev[index].atv_stable_out_cnt = atv_stable_out_cnt;
	sm_dev[index].atv_unstable_in_cnt = atv_unstable_in_cnt;
	sm_dev[index].back_nosig_max_cnt = back_nosig_max_cnt;
	sm_dev[index].atv_unstable_out_cnt = atv_unstable_out_cnt;
	sm_dev[index].hdmi_unstable_out_cnt = hdmi_unstable_out_cnt;
	tvin_smr_init_counter(index);
}

enum tvin_sm_status_e tvin_get_sm_status(int index)
{
	return sm_dev[index].state;
}
EXPORT_SYMBOL(tvin_get_sm_status);

int tvin_get_av_status(void)
{
	if (tvin_get_sm_status(0) == TVIN_SM_STATUS_STABLE)
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(tvin_get_av_status);

void vdin_send_event(struct vdin_dev_s *devp, enum tvin_sg_chg_flg sts)
{
	/*pr_info("%s :0x%x\n", __func__, sts);*/
	/*devp->extcon_event->state = sts;*/
	if (devp->dbg_v4l_no_vdin_event == 0)
		schedule_delayed_work(&devp->event_dwork, 0);
}

