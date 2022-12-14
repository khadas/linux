/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_sm.h
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

#ifndef __TVIN_STATE_MACHINE_H
#define __TVIN_STATE_MACHINE_H

#include "vdin_drv.h"

enum tvin_sm_status_e {
/* processing status from init to the finding of the 1st confirmed status */
	TVIN_SM_STATUS_NULL = 0,
	/* no signal - physically no signal */
	TVIN_SM_STATUS_NOSIG,
	/* unstable - physically bad signal */
	TVIN_SM_STATUS_UNSTABLE,
	 /* not supported - physically good signal & not supported */
	TVIN_SM_STATUS_NOTSUP,

	TVIN_SM_STATUS_PRESTABLE,
	 /* stable - physically good signal & supported */
	TVIN_SM_STATUS_STABLE,
};

enum tvin_sg_chg_flg {
	TVIN_SIG_CHG_NONE = 0,
	TVIN_SIG_CHG_SDR2HDR	= 0x01,
	TVIN_SIG_CHG_HDR2SDR	= 0x02,
	TVIN_SIG_CHG_DV2NO	= 0x04,
	TVIN_SIG_CHG_NO2DV	= 0x08,
	TVIN_SIG_CHG_COLOR_FMT	= 0x10,
	TVIN_SIG_CHG_RANGE	= 0x20,	/*color range:full or limit*/
	TVIN_SIG_CHG_BIT	= 0x40,	/*color bit depth: 8,10,12 ...*/
	TVIN_SIG_CHG_VS_FRQ	= 0x80,
	TVIN_SIG_CHG_DV_ALLM	= 0x100,
	TVIN_SIG_CHG_AFD	= 0x200,/*aspect ratio*/
	TVIN_SIG_CHG_VRR        = 0x1000, /*vrr*/
	TVIN_SIG_CHG_CLOSE_FE	= 0x40000000,	/*closed frontend*/
	TVIN_SIG_CHG_STS	= 0x80000000,	/*sm state change*/
};

#define TVIN_SIG_DV_CHG		(TVIN_SIG_CHG_DV2NO | TVIN_SIG_CHG_NO2DV)
#define TVIN_SIG_HDR_CHG	(TVIN_SIG_CHG_SDR2HDR | TVIN_SIG_CHG_HDR2SDR)

#define RE_CONFIG_DV_EN		0x01
#define RE_CONFIG_HDR_EN	0x02
#define RE_CONFIG_ALLM_EN	0x04

#define VDIN_STABLED_CNT		500
#define VDIN_SEND_EVENT_INTERVAL	50 /* Experience value can be adjusted */

enum vdin_sm_log_level {
	VDIN_SM_LOG_L_1 = 0x01,
	VDIN_SM_LOG_L_2 = 0x02,
	VDIN_SM_LOG_L_3 = 0x04,
	VDIN_SM_LOG_L_4 = 0x08,
	VDIN_SM_LOG_L_5 = 0x10,
	VDIN_SM_LOG_L_6 = 0x20,
	VDIN_SM_LOG_L_7 = 0x40, /* vtem and spd */
};

struct tvin_sm_s {
	enum tvin_sig_status_e sig_status;
	enum tvin_sm_status_e state;
	unsigned int state_cnt; /* STATE_NOSIG, STATE_UNSTABLE */
	unsigned int exit_nosig_cnt; /* STATE_NOSIG */
	unsigned int back_nosig_cnt; /* STATE_UNSTABLE */
	unsigned int back_stable_cnt; /* STATE_UNSTABLE */
	unsigned int exit_prestable_cnt; /* STATE_PRESTABLE */
	/* thresholds of state switched */
	int back_nosig_max_cnt;
	int atv_unstable_in_cnt;
	int atv_unstable_out_cnt;
	int atv_stable_out_cnt;
	int hdmi_unstable_out_cnt;
};

extern bool manual_flag;
extern unsigned int vdin_get_prop_in_sm_en;

void tvin_smr(struct vdin_dev_s *pdev);
void tvin_smr_init(int index);
void reset_tvin_smr(unsigned int index);

enum tvin_sm_status_e tvin_get_sm_status(int index);
void vdin_dump_vs_info(struct vdin_dev_s *devp);
void vdin_send_event(struct vdin_dev_s *devp, enum tvin_sg_chg_flg sts);
void vdin_update_prop(struct vdin_dev_s *devp);

#endif

