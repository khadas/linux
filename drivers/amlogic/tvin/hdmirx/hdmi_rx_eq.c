/*
 * hdmi rx eq adjust module for g9tv
 *
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/spinlock_types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/amlogic/tvin/tvin.h>
/* Local include */
#include "hdmi_rx_eq.h"
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"

int eq_setting[EQ_CH_NUM];

static bool finish_flag[EQ_CH_NUM];

static int nretry;
/* Auxiliary variable to perform early-late counters averaging */
int accumulator[EQ_CH_NUM];

/* Variables to store early-late counters averaging */
int early_cnt[EQ_CH_NUM];

/* Variables to store early-late counters slope */
int slope_accumulator[EQ_CH_NUM];

/* Auxiliary variable to detect early-late counters trend (up/down) */
int last_early_cnt[EQ_CH_NUM];

/* EQ Best Long cable setting and valid flag */
int best_long_setting[EQ_CH_NUM];
int valid_long_setting[EQ_CH_NUM];

/* EQ Best Short cable setting and valid flag */
int best_short_setting[EQ_CH_NUM];
int valid_short_setting[EQ_CH_NUM];

/* EQ Final Setting should be programed to the PHY */
int best_setting[EQ_CH_NUM];

/*Auxiliary variables to detect stable acquisitions this
will save 2mS * EQ_MAX_SETTING when good cables are used*/
static int upperBound_acqCH0, upperBound_acqCH1, upperBound_acqCH2;
static int lowerBound_acqCH0, lowerBound_acqCH1, lowerBound_acqCH2;
static int outBound_acqCH0, outBound_acqCH1, outBound_acqCH2;

/* Auxiliary variables to control algorithm status */
static bool error_cable_flag[EQ_CH_NUM];

/*Auxiliary variable to control how many times Algorithm
will retry if any error is detected*/
static bool minmax_err_flag;
static int minmax_check_cnt;
static int pll_rate_value = 0xff;

/* TMDSVALID FLAG */
static bool tmds_valid_flag;

static int min_max_diff = 4;
int eq_setting_back = 0;
static int tmds_valid_cnt;
int fat_bit_status = 0;

static struct hdmirx_phy_data_t *phy_private_data;
static struct phy_eq_algorithm_data_t *phy_eq_algo_data;

int hdmirx_log_flag = 1;
MODULE_PARM_DESC(hdmirx_log_flag, "\n hdmirx_log_flag\n");
module_param(hdmirx_log_flag, int, 0664);

static int tmds_valid_cnt_max = 2;
MODULE_PARM_DESC(tmds_valid_cnt_max, "\n tmds_valid_cnt_max\n");
module_param(tmds_valid_cnt_max, int, 0664);

static int force_clk_rate;
MODULE_PARM_DESC(force_clk_rate, "\n force_clk_rate\n");
module_param(force_clk_rate, int, 0664);

static bool fast_switching = true;
MODULE_PARM_DESC(fast_switching, "\n fast_switching\n");
module_param(fast_switching, bool, 0664);

static int mpll_ctl_setting = 0x200;
MODULE_PARM_DESC(mpll_ctl_setting, "\n mpll_ctl_setting\n");
module_param(mpll_ctl_setting, int, 0664);

static int eq_clk_rate_wait = 2;
MODULE_PARM_DESC(eq_clk_rate_wait, "\n eq_clk_rate_wait\n");
module_param(eq_clk_rate_wait, int, 0664);

static int mpll_param4 = 0x24dc;
MODULE_PARM_DESC(mpll_param4, "\n mpll_param4\n");
module_param(mpll_param4, int, 0664);

bool phy_maxvsmin(int ch0Setting, int ch1Setting, int ch2Setting)
{
	int min = ch0Setting;
	int max = ch0Setting;

	if (ch1Setting > max)
		max = ch1Setting;
	if (ch2Setting > max)
		max = ch2Setting;
	if (ch1Setting < min)
		min = ch1Setting;
	if (ch2Setting < min)
		min = ch2Setting;
	if ((max - min) > min_max_diff) {
		rx_print("MINMAX ERROR\n");
		return 0;
	}
	return 1;
}

void phy_init_param(void)
{
	best_long_setting[EQ_CH0] = 6;
	best_long_setting[EQ_CH1] = 6;
	best_long_setting[EQ_CH2] = 6;
	best_short_setting[EQ_CH0] = EQ_DEFAULT_SETTING;
	best_short_setting[EQ_CH1] = EQ_DEFAULT_SETTING;
	best_short_setting[EQ_CH2] = EQ_DEFAULT_SETTING;
	best_setting[EQ_CH0] = EQ_DEFAULT_SETTING;
	best_setting[EQ_CH1] = EQ_DEFAULT_SETTING;
	best_setting[EQ_CH2] = EQ_DEFAULT_SETTING;
	eq_setting[EQ_CH0] = EQ_DEFAULT_SETTING;
	eq_setting[EQ_CH1] = EQ_DEFAULT_SETTING;
	eq_setting[EQ_CH2] = EQ_DEFAULT_SETTING;
	/*remove error*/
	phy_eq_algo_data = NULL;
}

void phy_eq_set_state(enum phy_eq_states_e state, bool force)
{
	if (force)
		phy_private_data->phy_eq_state = state;
	else
		if ((phy_private_data->phy_eq_state != EQ_IDLE) &&
			(state <= EQ_FAILED))
			phy_private_data->phy_eq_state = state;
}

void phy_conf_eq_setting(int ch0_lockVector,
				int ch1_lockVector, int ch2_lockVector)
{
	/* ConfEqualSetting */
	hdmirx_wr_phy(PHY_EQCTRL4_CH0, 1<<ch0_lockVector);
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x0024);
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x0026);

	hdmirx_wr_phy(PHY_EQCTRL4_CH1, 1<<ch1_lockVector);
	hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0024);
	hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0026);

	hdmirx_wr_phy(PHY_EQCTRL4_CH2, 1<<ch2_lockVector);
	hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0024);
	hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0026);
}

void set_cmd_state(bool enable)
{
	spin_lock(&phy_private_data->slock);
	phy_private_data->new_cmd = false;
	spin_unlock(&phy_private_data->slock);
}

int phy_wait_clk_stable(void)
{
	int i;
	int eq_mainfsm_status;
	int stable_start = 0;

	for (i = 0; i < EQ_CLK_WAIT_MAX_COUNT; i++) {
		eq_mainfsm_status = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);
		/*Make sure that SW is not overriding the equalization not
		mandatory code (this is the default status)*/
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);

		/*clock_stable = false => Main FSM should wait for clock stable
		Is mandatory to have a stable clock before starting to do
		equalization*/
		if (hdmirx_log_flag & VIDEO_LOG_ENABLE)
			rx_print("phy state---[%x]\n", eq_mainfsm_status);
		if ((eq_mainfsm_status >> 8 & 0x1) == 0)
			stable_start = 0;
		else {
			stable_start++;
			if (stable_start >= eq_clk_rate_wait)
				break;
		}

		if (((EQ_CLK_WAIT_MAX_COUNT - 1) == i) ||
			phy_private_data->new_cmd) {
			return -1;
		}
		block_delay_ms(EQ_CLK_WAIT_DELAY);
	}

	return 0;
}

void phy_eq_task_continue(void)
{
	if (phy_private_data != NULL)
		complete(&phy_private_data->phy_task_lock);
}

void phy_EQ_workaround(void)
{
	int stepSlope[EQ_CH_NUM];
	int eq_mainfsm_status = 0;
	int nacq = 5;
	int eq_counter_th = EQ_COUNTERTHRESHOLD;
	enum phy_eq_cmd_e new_cmd;

	memset(stepSlope, 0, sizeof(stepSlope));
	/*check new command*/
	if (phy_private_data->new_cmd) {
		spin_lock(&phy_private_data->slock);
		phy_private_data->new_cmd = false;
		new_cmd = phy_private_data->cmd;
		spin_unlock(&phy_private_data->slock);
		if (new_cmd == EQ_START) {
			rx_print("start eq calc\n");
			phy_eq_set_state(EQ_DATA_START, true);
		} else if (new_cmd == EQ_STOP) {
			rx_print("exit eq calc\n");
			phy_eq_set_state(EQ_IDLE, true);
		}
	}

	switch (phy_private_data->phy_eq_state) {
	case EQ_DATA_START:
		hdmirx_phy_EQ_workaround_init();
		block_delay_ms(1);
		/********************   IMPORTANT   *************************/
		/********************* PLEASE README ************************/
		/*The following piece of code was added here to make sure that
		before running the algorithm all needed condition are available
		 if not	The conditions are, PHY is receiving a stable clock
		[reg (MAINFSM_STATUS1) 0x09[8]=1'b1] and PLL_RATE if greater
		that 94.5MHz reg (MAINFSM_STATUS1) 0x09[10:9]=2'b0x]
		If stable clock is not asserted MAIN FSM should wait for such
		 signal	and call again the algorithm.*/

		/*if it is forced to exit delay,set state to EQ_IDLE
		otherwise clk stable failed*/
		if (phy_wait_clk_stable() != 0) {
			if (phy_private_data->new_cmd) {
				set_cmd_state(false);
				phy_eq_set_state(EQ_IDLE, false);
			} else
				phy_eq_set_state(EQ_FAILED, false);
			return;
		}
		/*block_delay_ms(eq_clk_rate_wait);*/
		/* GET PHY STATUS (MAINFSM_STATUS1) */
		eq_mainfsm_status = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);
		pll_rate_value = (eq_mainfsm_status >> 9 & 0x3);
		rx_print("eq_mainfsm_status: %#x,tmds_clk:%d\n",
			eq_mainfsm_status, hdmirx_get_tmds_clock());
		if (!hdmirx_tmds_34g()) {
			if (((eq_mainfsm_status >> 10) & 0x1) != 0) {
				/* pll_rate smaller than 94.5MHz, algorithm
				not needed Please make sure that PHY get the
				 default status */
				rx_print("low pll rate-[%x]\n",
						eq_mainfsm_status);
				/* default status for register 0x33 */
				hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x0020);
				/* default status for register 0x53 */
				hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0020);
				/* default status for register 0x73 */
				hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0020);

				eq_setting[EQ_CH0] = 0;
				eq_setting[EQ_CH1] = 0;
				eq_setting[EQ_CH2] = 0;
				hdmirx_phy_conf_eq_setting(rx.port,
							 eq_setting[EQ_CH0],
				eq_setting[EQ_CH1], eq_setting[EQ_CH2]);

				/* hdmirx_phy_init(rx.port, 0); */
				phy_eq_set_state(EQ_SUCCESS_END, false);
				return;
			}
		}

		/************* END  NOTICE **************************/
		fat_bit_status = EQ_FATBIT_MASK;
		min_max_diff = MINDIFF;
		eq_counter_th = EQ_COUNTERTHRESHOLD;
		if ((((eq_mainfsm_status >> 9) & 0x3) == 0))
			fat_bit_status = EQ_FATBIT_MASK_4k;
		if (hdmirx_tmds_34g()) {
			fat_bit_status = EQ_FATBIT_MASK_HDMI20;
			eq_counter_th = EQ_COUNTERTHRESHOLD_HDMI20;
			min_max_diff = MINDIFF_HDMI20;
		}

		hdmirx_wr_phy(0x43, fat_bit_status);
		hdmirx_wr_phy(0x63, fat_bit_status);
		hdmirx_wr_phy(0x83, fat_bit_status);

		finish_flag[EQ_CH0] = 0;
		finish_flag[EQ_CH1] = 0;
		finish_flag[EQ_CH2] = 0;

		error_cable_flag[EQ_CH0] = 0;
		error_cable_flag[EQ_CH1] = 0;
		error_cable_flag[EQ_CH2] = 0;
		valid_long_setting[EQ_CH0] = 0;
		valid_long_setting[EQ_CH1] = 0;
		valid_long_setting[EQ_CH2] = 0;
		valid_short_setting[EQ_CH0] = 0;
		valid_short_setting[EQ_CH1] = 0;
		valid_short_setting[EQ_CH2] = 0;
		accumulator[EQ_CH0] = 0;
		accumulator[EQ_CH1] = 0;
		accumulator[EQ_CH2] = 0;
		slope_accumulator[EQ_CH0] = 0;
		slope_accumulator[EQ_CH1] = 0;
		slope_accumulator[EQ_CH2] = 0;
		last_early_cnt[EQ_CH0] = 0;
		last_early_cnt[EQ_CH1] = 0;
		last_early_cnt[EQ_CH2] = 0;
		eq_setting_back = 0;
		phy_eq_set_state(EQ_SET_LOCK_VECTOR, false);
		phy_eq_task_continue();
	break;
	case EQ_SET_LOCK_VECTOR:
		/* tmds_valid_flag = 1; */
		early_cnt[EQ_CH0] = 0;
		early_cnt[EQ_CH1] = 0;
		early_cnt[EQ_CH2] = 0;
		outBound_acqCH0 = 0;
		outBound_acqCH1 = 0;
		outBound_acqCH2 = 0;
		nretry = 0;
		phy_conf_eq_setting(eq_setting_back,
			eq_setting_back, eq_setting_back);
		phy_eq_set_state(EQ_SET_FORCE_FMS_STATE, false);
		phy_eq_task_continue();
	break;
	case EQ_SET_FORCE_FMS_STATE:
		hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
		hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1819);
		hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
		/*please make sure that there's at least 10 mS between
		that state and  TMDSVALID detection/test*/
		phy_eq_set_state(EQ_CHECK_TMDS_VALID, false);
		if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
			rx_print("set for FMS state->check TMDS valid\n");
		phy_eq_task_continue();
	break;
	case EQ_CHECK_TMDS_VALID:
		if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
			rx_print("check TMDS valid\n");
		/* Please check the following */
		/* between the following rx_print instructions */
		/* you have at lest 10ms */
		/*if it is forced to exit delay,set state to EQ_DATA_START
		otherwise clk stable failed*/
		/*if (phy_wait_clk_stable() != 0) {
			if (phy_private_data->exit_task_delay)
				phy_eq_set_state(EQ_DATA_START, false);
			else
				phy_eq_set_state(EQ_FAILED, false);
			return;
		}*/

		/*Is this the tmds valid detection? ALG shouldn't wait for
		 a long time (much more than 10mS) for TMDSVALID assertion*/
		if (!hdmirx_phy_check_tmds_valid()) {
			/*how long are you waiting for TMDSVALID? should
			 be ~~10mS did you see any case where TMDSVALID
			 doesn't get asserted after	10mS but get asserted
			 after a long time?System shouldn't wait for TMDS
			 VALID assertion more than 10mS	because if TMDSVALID
			 is not asserted means that setting is not good
			 enough and must proceed for the next one please
			 send to us some log file (early counters slope
			acumulator and setting ) where TMDS VALID just
			gets asserted after long time*/
			if (tmds_valid_cnt++ > tmds_valid_cnt_max) {
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print(
					"valid_cnt=%d 0x09[%x]-0x30[%x]-0x50[%x]-0x70[%x]\n",
					tmds_valid_cnt, hdmirx_rd_phy(0x09),
					hdmirx_rd_phy(0x30),
					hdmirx_rd_phy(0x50),
					hdmirx_rd_phy(0x70));
					tmds_valid_flag = 0;
					tmds_valid_cnt = 0;
					phy_eq_set_state(EQ_GET_CABLE_TYPE,
								 false);
			} else {
				block_delay_ms(EQ_TMDS_VALID_WAIT_DELAY);
			}
		} else {
			tmds_valid_cnt = 0;
			tmds_valid_flag = 1;
			early_cnt[EQ_CH0] = hdmirx_rd_phy(PHY_EQSTAT3_CH0);
			early_cnt[EQ_CH1] = hdmirx_rd_phy(PHY_EQSTAT3_CH1);
			early_cnt[EQ_CH2] = hdmirx_rd_phy(PHY_EQSTAT3_CH2);

			accumulator[EQ_CH0] = early_cnt[EQ_CH0];
			accumulator[EQ_CH1] = early_cnt[EQ_CH1];
			accumulator[EQ_CH2] = early_cnt[EQ_CH2];
			upperBound_acqCH0 =
				accumulator[EQ_CH0] + EQ_BOUNDARYSPREAD;
			lowerBound_acqCH0 =
				accumulator[EQ_CH0] - EQ_BOUNDARYSPREAD;
			upperBound_acqCH1 =
				accumulator[EQ_CH1] + EQ_BOUNDARYSPREAD;
			lowerBound_acqCH1 =
				accumulator[EQ_CH1] - EQ_BOUNDARYSPREAD;
			upperBound_acqCH2 =
				accumulator[EQ_CH2] + EQ_BOUNDARYSPREAD;
			lowerBound_acqCH2 =
				accumulator[EQ_CH1] - EQ_BOUNDARYSPREAD;
			phy_eq_set_state(EQ_AQUIRE_EARLY_COUNTER, false);
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("tmds valid->aquire early counter\n");
		}
		phy_eq_task_continue();
	break;
	case EQ_AQUIRE_EARLY_COUNTER:
		/* ************************   IMPORTANT   *******************/
		/* ************************ PLEASE README *******************/
		/* the execution of the following loop before jumping to the
		MAIN FSM will save 500mS, taking into account that MAIN FSM
		 takes 10mS to finish.Important loop to save 500mS of
		execution time. Loop takes a maximum time of ~5mS */
		while (1) {
			nretry++;
			/* hdmi_rx_phy_ConfEqualAutoCalib */
			hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
			hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1819);
			hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
			/* wait EQ_WAITTIME to have a stable
						read from Early edge counter */
			mdelay(EQ_WAITTIME);

			/* Update boundaries to detect a stable acquisitions */
			if (accumulator[EQ_CH0] > upperBound_acqCH0 ||
				accumulator[EQ_CH0] < lowerBound_acqCH0)
				outBound_acqCH0++;
			if (accumulator[EQ_CH1] > upperBound_acqCH1 ||
				accumulator[EQ_CH1] < lowerBound_acqCH1)
				outBound_acqCH1++;
			if (accumulator[EQ_CH2] > upperBound_acqCH2 ||
				accumulator[EQ_CH2] < lowerBound_acqCH2)
				outBound_acqCH2++;
			/*Finish averaging because Stable acquisitions were
			detected,minimum of three readings between boundaries
			 to finish the averaging*/
			if (nretry == EQ_MIN_ACQ_STABLE_DETECTION) {
				if (outBound_acqCH0 == 0 &&
					outBound_acqCH1 == 0 &&
					outBound_acqCH2 == 0) {
					nacq = EQ_MIN_ACQ_STABLE_DETECTION;
					early_cnt[EQ_CH0] =
						accumulator[EQ_CH0] / nacq;
					if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
						rx_print(
						"3early_cnt_ch0=[%d]\n",
							 early_cnt[EQ_CH0]);
					early_cnt[EQ_CH1] =
						accumulator[EQ_CH1] / nacq;
					if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
						rx_print(
						"3early_cnt_ch1=[%d]\n",
						early_cnt[EQ_CH1]);
					early_cnt[EQ_CH2] =
						accumulator[EQ_CH2] / nacq;
					if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
						rx_print(
						"3early_cnt_ch2=[%d]\n",
						 early_cnt[EQ_CH2]);
					nretry = 0;
					phy_eq_set_state(EQ_GET_CABLE_TYPE,
								false);
					phy_eq_task_continue();
					return;
				}
			} else if (nretry == nacq) {
				/* Finish averaging,  maximum number of
							readings achieved */
				nacq = nretry;
				early_cnt[EQ_CH0] = accumulator[EQ_CH0] / nacq;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("5early_cnt_ch0=[%d]\n",
							early_cnt[EQ_CH0]);
				early_cnt[EQ_CH1] = accumulator[EQ_CH1] / nacq;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("5early_cnt_ch1=[%d]\n",
							 early_cnt[EQ_CH1]);
				early_cnt[EQ_CH2] = accumulator[EQ_CH2] / nacq;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("5early_cnt_ch2=[%d]\n",
							 early_cnt[EQ_CH2]);
				nretry = 0;
				phy_eq_set_state(EQ_GET_CABLE_TYPE, false);
				phy_eq_task_continue();
				return;
			}
			/* Read next set of Early edges counter */
			early_cnt[EQ_CH0] = hdmirx_rd_phy(PHY_EQSTAT3_CH0);
			early_cnt[EQ_CH1] = hdmirx_rd_phy(PHY_EQSTAT3_CH1);
			early_cnt[EQ_CH2] = hdmirx_rd_phy(PHY_EQSTAT3_CH2);

			/* update the averaging accumulator */
			accumulator[EQ_CH0] += early_cnt[EQ_CH0];
			accumulator[EQ_CH1] += early_cnt[EQ_CH1];
			accumulator[EQ_CH2] += early_cnt[EQ_CH2];
		}
		/* ********************** PLEASE README *********************/
		/* if the following break gets removed, algorithm becomes
		10mS*EQ_MAX_SETTING faster */
	break;
	case EQ_GET_CABLE_TYPE:
		/* CABLE SETTING FOR THE BEST EQUALIZATION */
		/* LONG CABLE EQUALIZATION */
		if (tmds_valid_flag == 1) {
			/* rx_print("enter cable type detection\n"); */
			if ((early_cnt[EQ_CH0] < last_early_cnt[EQ_CH0]) &&
					(finish_flag[EQ_CH0] == 0)) {
				slope_accumulator[EQ_CH0] +=
				(last_early_cnt[EQ_CH0] - early_cnt[EQ_CH0]);
				if ((0 == valid_long_setting[EQ_CH0]) &&
					(early_cnt[EQ_CH0] < eq_counter_th) &&
					(slope_accumulator[EQ_CH0] >
						EQ_SLOPEACM_MINTHRESHOLD)) {
					best_long_setting[EQ_CH0] =
							 eq_setting_back;
					valid_long_setting[EQ_CH0] = 1;
				}
				stepSlope[EQ_CH0] =
				 last_early_cnt[EQ_CH0]-early_cnt[EQ_CH0];
			}
			if ((early_cnt[EQ_CH1] < last_early_cnt[EQ_CH1]) &&
					   (finish_flag[EQ_CH1] == 0)) {
				slope_accumulator[EQ_CH1] +=
				(last_early_cnt[EQ_CH1] - early_cnt[EQ_CH1]);
				if ((0 == valid_long_setting[EQ_CH1]) &&
					(early_cnt[EQ_CH1] < eq_counter_th) &&
					(slope_accumulator[EQ_CH1] >
						EQ_SLOPEACM_MINTHRESHOLD)) {
					best_long_setting[EQ_CH1] =
							eq_setting_back;
					valid_long_setting[EQ_CH1] = 1;
				}
				stepSlope[EQ_CH1] =
				last_early_cnt[EQ_CH1]-early_cnt[EQ_CH1];
			}
			if ((early_cnt[EQ_CH2] < last_early_cnt[EQ_CH2]) &&
					   (finish_flag[EQ_CH2] == 0)) {
				slope_accumulator[EQ_CH2] +=
				(last_early_cnt[EQ_CH2] - early_cnt[EQ_CH2]);
				if ((0 == valid_long_setting[EQ_CH2]) &&
					(early_cnt[EQ_CH2] < eq_counter_th) &&
					(slope_accumulator[EQ_CH2] >
						EQ_SLOPEACM_MINTHRESHOLD)) {
					best_long_setting[EQ_CH2] =
							eq_setting_back;
					valid_long_setting[EQ_CH2] = 1;
				}
				stepSlope[EQ_CH2] =
				 last_early_cnt[EQ_CH2]-early_cnt[EQ_CH2];
			}
		}
		/* SHORT CABLE EQUALIZATION */
		if (tmds_valid_flag == 1 &&
			eq_setting_back <= EQ_SHORT_CABLE_BEST_SETTING) {
			/* Short setting better than default */
			if (early_cnt[EQ_CH0] < eq_counter_th &&
				valid_short_setting[EQ_CH0] == 0) {
				best_short_setting[EQ_CH0] = eq_setting_back;
				valid_short_setting[EQ_CH0] = 1;
			}
			if (eq_setting_back == EQ_SHORT_CABLE_BEST_SETTING &&
				valid_short_setting[EQ_CH0] == 0) {
				/* default Short setting is valid */
				best_short_setting[EQ_CH0] =
					EQ_SHORT_CABLE_BEST_SETTING;
				valid_short_setting[EQ_CH0] = 1;
			}
			/* Short setting better than default */
			if (early_cnt[EQ_CH1] < eq_counter_th &&
				valid_short_setting[EQ_CH1] == 0) {
				best_short_setting[EQ_CH1] = eq_setting_back;
				valid_short_setting[EQ_CH1] = 1;
			}
			if (eq_setting_back == EQ_SHORT_CABLE_BEST_SETTING &&
				valid_short_setting[EQ_CH1] == 0) {
				/* Short setting is valid */
				best_short_setting[EQ_CH1] =
					 EQ_SHORT_CABLE_BEST_SETTING;
				valid_short_setting[EQ_CH1] = 1;
			}

			if (early_cnt[EQ_CH2] < eq_counter_th &&
				valid_short_setting[EQ_CH2] == 0) {
				/* Short setting better than default */
				best_short_setting[EQ_CH2] = eq_setting_back;
				valid_short_setting[EQ_CH2] = 1;
			}
			if (eq_setting_back == EQ_SHORT_CABLE_BEST_SETTING &&
				valid_short_setting[EQ_CH2] == 0) {
				/* Short setting is valid */
				best_short_setting[EQ_CH2] =
					EQ_SHORT_CABLE_BEST_SETTING;
				valid_short_setting[EQ_CH2] = 1;
			}
		}

		/* CABLE DETECTION */
		/* long cable */
		if ((1 == valid_long_setting[EQ_CH0]) &&
			(slope_accumulator[EQ_CH0] >
			EQ_SLOPEACM_LONG_CABLE_THRESHOLD)
			&& (finish_flag[EQ_CH0] == 0)) {
			best_setting[EQ_CH0] = best_long_setting[EQ_CH0];
			finish_flag[EQ_CH0] = 1;
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("ch0:long cable\n");
		}
		if ((1 == valid_long_setting[EQ_CH1]) &&
			(slope_accumulator[EQ_CH1] >
			EQ_SLOPEACM_LONG_CABLE_THRESHOLD) &&
			(finish_flag[EQ_CH1] == 0)) {
			best_setting[EQ_CH1] = best_long_setting[EQ_CH1];
			finish_flag[EQ_CH1] = 1;
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("ch1:long cable\n");
		}
		if ((1 == valid_long_setting[EQ_CH2]) &&
			 (slope_accumulator[EQ_CH2] >
			EQ_SLOPEACM_LONG_CABLE_THRESHOLD) &&
			 (finish_flag[EQ_CH2] == 0)) {
			best_setting[EQ_CH2] = best_long_setting[EQ_CH2];
			finish_flag[EQ_CH2] = 1;
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("ch2:long cable\n");
		}

		/* Maximum setting achieved without long cable detection,
		so decision must be taken */
		if (eq_setting_back >= EQ_MAX_SETTING) {
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("eq reach the max settin---%d\n",
				 eq_setting_back);
			/* short cable */
			if ((slope_accumulator[EQ_CH0] <
				EQ_SLOPEACM_LONG_CABLE_THRESHOLD)
				&& (finish_flag[EQ_CH0] == 0)) {
				best_setting[EQ_CH0] =
						best_short_setting[EQ_CH0];
				finish_flag[EQ_CH0] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch0:short cable\n");
			}
			if ((slope_accumulator[EQ_CH1] <
				EQ_SLOPEACM_LONG_CABLE_THRESHOLD)
				&& (finish_flag[EQ_CH1] == 0)) {
				best_setting[EQ_CH1] =
					best_short_setting[EQ_CH1];
				finish_flag[EQ_CH1] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch1:short cable\n");
			}
			if ((slope_accumulator[EQ_CH2] <
				EQ_SLOPEACM_LONG_CABLE_THRESHOLD)
				&& (finish_flag[EQ_CH2] == 0)) {
				best_setting[EQ_CH2] =
					best_short_setting[EQ_CH2];
				finish_flag[EQ_CH2] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch2:short cable\n");
			}
			/* very long cable */
			if ((slope_accumulator[EQ_CH0] >
				EQ_SLOPEACM_LONG_CABLE_THRESHOLD) &&
				(stepSlope[EQ_CH0] > EQ_MINSLOPE_VERYLONGCABLE)
				&& tmds_valid_flag &&
				(finish_flag[EQ_CH0] == 0)) {
				best_setting[EQ_CH0] = EQ_MAX_SETTING;
				finish_flag[EQ_CH0] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch0:very long cable\n");
			}
			if ((slope_accumulator[EQ_CH1] >
				 EQ_SLOPEACM_LONG_CABLE_THRESHOLD) &&
				(stepSlope[EQ_CH1] > EQ_MINSLOPE_VERYLONGCABLE)
				&& tmds_valid_flag
				&& (finish_flag[EQ_CH1] == 0)) {
				best_setting[EQ_CH1] = EQ_MAX_SETTING;
				finish_flag[EQ_CH1] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch1:very long cable\n");
			}
			if ((slope_accumulator[EQ_CH2] >
				EQ_SLOPEACM_LONG_CABLE_THRESHOLD)
				&& (stepSlope[EQ_CH2] >
						EQ_MINSLOPE_VERYLONGCABLE)
				&& tmds_valid_flag &&
				(finish_flag[EQ_CH2] == 0)) {
				best_setting[EQ_CH2] = EQ_MAX_SETTING;
				finish_flag[EQ_CH2] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch2:very long cable\n");
			}
			/* error cable */
			if (finish_flag[EQ_CH0] == 0) {
				best_setting[EQ_CH0] =
					 EQ_ERROR_CABLE_BEST_SETTING;
				finish_flag[EQ_CH0] = 1;
				error_cable_flag[EQ_CH0] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch0:error cable\n");
			}
			if (finish_flag[EQ_CH1] == 0) {
				best_setting[EQ_CH1] =
					 EQ_ERROR_CABLE_BEST_SETTING;
				finish_flag[EQ_CH1] = 1;
				error_cable_flag[EQ_CH1] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch1:error cable\n");
			}
			if (finish_flag[EQ_CH2] == 0) {
				best_setting[EQ_CH2] =
					 EQ_ERROR_CABLE_BEST_SETTING;
				finish_flag[EQ_CH2] = 1;
				error_cable_flag[EQ_CH2] = 1;
				if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
					rx_print("ch2:error cable\n");
			}
		}
		/* Algorithm already take the decision for all Channels */
		if (finish_flag[EQ_CH0] && finish_flag[EQ_CH1] &&
						 finish_flag[EQ_CH2]) {
			phy_eq_set_state(EQ_CONF_BEST_SETTING, false);
		} else {
			/* Updating latest acquisition memory and jump
			to the next setting */
			eq_setting_back++;
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("eq=[%d]\n", eq_setting_back);
			last_early_cnt[EQ_CH0] = early_cnt[EQ_CH0];
			last_early_cnt[EQ_CH1] = early_cnt[EQ_CH1];
			last_early_cnt[EQ_CH2] = early_cnt[EQ_CH2];
			phy_eq_set_state(EQ_SET_LOCK_VECTOR, false);
		}
		phy_eq_task_continue();
	break;
	case EQ_CONF_BEST_SETTING:
		minmax_err_flag = 0;
		if ((phy_maxvsmin(best_setting[EQ_CH0], best_setting[EQ_CH1],
			best_setting[EQ_CH2]) == 0) ||
			error_cable_flag[EQ_CH0] == 1
			|| error_cable_flag[EQ_CH1] == 1 ||
			error_cable_flag[EQ_CH2] == 1) {
			rx_print("ch0-%d-%d,ch1-%d-%d,ch2-%d-%d\n",
				best_setting[EQ_CH0], error_cable_flag[EQ_CH0],
				best_setting[EQ_CH1], error_cable_flag[EQ_CH1],
				best_setting[EQ_CH2], error_cable_flag[EQ_CH2]);
			best_setting[EQ_CH0] = EQ_ERROR_CABLE_BEST_SETTING;
			best_setting[EQ_CH1] = EQ_ERROR_CABLE_BEST_SETTING;
			best_setting[EQ_CH2] = EQ_ERROR_CABLE_BEST_SETTING;
			minmax_err_flag = 1;
			minmax_check_cnt++;
		}
		if ((minmax_err_flag == 0) || (minmax_check_cnt >= 2)) {
			tmds_valid_cnt = 0; /* What this means?	Are you
			waiting for tmdsvalid? */
			minmax_check_cnt = 0;
			if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
				rx_print("eq cal->timing chage\n");
		} else{ /* error detected and will retry */
			phy_eq_set_state(EQ_DATA_START, false);
			phy_eq_task_continue();
			return;
		}
		/* please check readme for this particular function */
		hdmirx_phy_conf_eq_setting(rx.port, best_setting[EQ_CH0],
		best_setting[EQ_CH1], best_setting[EQ_CH2]);
		rx_print("best EQ ch0=%d, ch1=%d,ch2=%d\n",
		best_setting[EQ_CH0], best_setting[EQ_CH1],
					best_setting[EQ_CH2]);
		eq_setting[EQ_CH0] = best_setting[EQ_CH0];
		eq_setting[EQ_CH1] = best_setting[EQ_CH1];
		eq_setting[EQ_CH2] = best_setting[EQ_CH2];
		/* Algorithm finish */
		phy_eq_set_state(EQ_SUCCESS_END, false);
	break;
	default:
	break;
	}
}

void phy_set_cmd(enum phy_eq_cmd_e cmd)
{
	spin_lock(&phy_private_data->slock);
	phy_private_data->cmd = cmd;
	phy_private_data->new_cmd = true;
	spin_unlock(&phy_private_data->slock);
}

int phy_eq_task(void *data)
{
	while (phy_private_data->task_running) {
		wait_for_completion_interruptible(
			&((struct hdmirx_phy_data_t *)data)->phy_task_lock);
		/*while (
		down_trylock(&((struct hdmirx_phy_data_t *)data)->phy_task_lock)
			== 0);*/
		phy_EQ_workaround();
	}
	return 0;
}

void hdmirx_phy_conf_eq_setting(int rx_port_sel, int ch0Setting,
				int ch1Setting, int ch2Setting)
{
	unsigned int data32;
	if (hdmirx_log_flag&VIDEO_LOG_ENABLE)
		rx_print("hdmirx_phy_conf_eq_setting\n");
	/* PDDQ = 1'b1; PHY_RESET = 1'b0; */
	data32  = 0;
	data32 |= 1             << 6;   /* [6]      physvsretmodez */
	data32 |= 1             << 4;   /* [5:4]    cfgclkfreq */
	data32 |= rx_port_sel   << 2;   /* [3:2]    portselect */
	data32 |= 1             << 1;   /* [1]      phypddq */
	data32 |= 0             << 0;   /* [0]      phyreset */
	/*DEFAULT: {27'd0, 3'd0, 2'd1} */
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
	hdmirx_wr_phy(PHY_CH0_EQ_CTRL3, ch0Setting);
	hdmirx_wr_phy(PHY_CH1_EQ_CTRL3, ch1Setting);
	hdmirx_wr_phy(PHY_CH2_EQ_CTRL3, ch2Setting);
	hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x40);

	/* PDDQ = 1'b0; PHY_RESET = 1'b0; */
	data32  = 0;
	data32 |= 1             << 6;   /* [6]      physvsretmodez */
	data32 |= 1             << 4;   /* [5:4]    cfgclkfreq */
	data32 |= rx_port_sel   << 2;   /* [3:2]    portselect */
	data32 |= 0             << 1;   /* [1]      phypddq */
	data32 |= 0             << 0;   /* [0]      phyreset */
	/* DEFAULT: {27'd0, 3'd0, 2'd1} */
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
}

bool hdmirx_phy_clk_rate_monitor(void)
{
	unsigned int clk_rate;
	bool changed = false;
	int i;

	if (force_clk_rate & 0x10)
		clk_rate = force_clk_rate & 1;
	else
		clk_rate = (hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1;

	if (clk_rate != hdmirx_tmds_34g()) {
		changed = true;
		for (i = 0; i < 3; i++) {
			if (1 == clk_rate) {
				hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
					hdmirx_rd_phy(PHY_CDR_CTRL_CNT)|(1<<8));
			} else {
				hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
					hdmirx_rd_phy(
						PHY_CDR_CTRL_CNT)&(~(1<<8)));
			}

			if ((hdmirx_rd_phy(PHY_CDR_CTRL_CNT) & 0x100) ==
				(clk_rate << 8))
				break;
		}
		rx_print("clk_rate:%d, save: %d\n",
			clk_rate, phy_private_data->last_clk_rate);
	}
	return changed;
}

int hdmirx_phy_probe(void)
{
	phy_init_param();
	phy_private_data = kmalloc(sizeof(struct hdmirx_phy_data_t),
							GFP_KERNEL);
	if (phy_private_data == NULL)
		return -1;
	/*phy_eq_algo_data= kmalloc(sizeof(struct phy_eq_algorithm_data_t),
							GFP_KERNEL);
	if (phy_eq_algo_data == NULL)
		return -1;*/
	spin_lock_init(&phy_private_data->slock);
	mutex_init(&phy_private_data->state_lock);
	init_completion(&phy_private_data->phy_task_lock);
	phy_private_data->task_running = true;
	phy_private_data->cmd = 0;
	phy_private_data->new_cmd = false;
	phy_private_data->task = kthread_create(phy_eq_task,
				phy_private_data, "eq");
	if (IS_ERR(phy_private_data->task)) {
		rx_print("thread creating error.\n");
		return -1;
	}
	wake_up_process(phy_private_data->task);
	return 0;
}

void hdmirx_phy_exit(void)
{
	if (phy_private_data->task) {
		phy_private_data->cmd = EQ_STOP;
		phy_private_data->task_running = false;
		send_sig(SIGTERM, phy_private_data->task, 1);
		complete(&phy_private_data->phy_task_lock);
		kthread_stop(phy_private_data->task);
		phy_private_data->task = NULL;
	}

	kfree(phy_private_data);
	/*kfree(phy_eq_algo_data);*/
}

int hdmirx_phy_start_eq(void)
{
	if (phy_private_data != NULL) {
		phy_set_cmd(EQ_START);
		complete(&phy_private_data->phy_task_lock);
		rx_print("%s\n", __func__);
	} else {
		return -1;
	}

	return 0;
}

int hdmirx_phy_stop_eq(void)
{
	if (phy_private_data != NULL) {
		if ((hdmirx_phy_get_eq_state() != EQ_IDLE) &&
			(hdmirx_phy_get_eq_state() < EQ_SUCCESS_END))
			phy_set_cmd(EQ_STOP);
		rx_print("%s\n", __func__);
	} else {
		return -1;
	}


	return 0;
}

int hdmirx_phy_suspend_eq(void)
{
	hdmirx_phy_stop_eq();

	while ((phy_private_data->phy_eq_state > EQ_IDLE)
		&& (phy_private_data->phy_eq_state <
		EQ_SUCCESS_END)) {
		block_delay_ms(1);
	}

	return 0;
}

void hdmirx_phy_reset(int rx_port_sel, int dcm)
{
	unsigned int data32;
	data32	= 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx_port_sel << 2;
	data32 |= 1 << 1;
	data32 |= 0 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);

	hdmirx_wr_phy(0x43, fat_bit_status);
	hdmirx_wr_phy(0x63, fat_bit_status);
	hdmirx_wr_phy(0x83, fat_bit_status);

	hdmirx_wr_phy(PHY_CH0_EQ_CTRL3, eq_setting[EQ_CH0]);
	hdmirx_wr_phy(PHY_CH1_EQ_CTRL3, eq_setting[EQ_CH1]);
	hdmirx_wr_phy(PHY_CH2_EQ_CTRL3, eq_setting[EQ_CH2]);
	if ((0 == eq_setting[EQ_CH0]) &&
		(0 == eq_setting[EQ_CH1]) &&
		(0 == eq_setting[EQ_CH2]))
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);
	else
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x40);

	/*hdmirx_phy_clk_rate_monitor();*/

	data32 = 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx_port_sel << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);

	rx_print("%s  %d Done!\n", __func__, rx.port);
}

void hdmirx_phy_init(int rx_port_sel, int dcm)
{
	unsigned int data32;
	data32 = 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx_port_sel << 2;
	data32 |= 1 << 1;
	data32 |= 1 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
	mdelay(1);

	data32	= 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx_port_sel << 2;
	data32 |= 1 << 1;
	data32 |= 0 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);

	hdmirx_wr_phy(MPLL_PARAMETERS2,    0x1c94);
	hdmirx_wr_phy(MPLL_PARAMETERS3,    0x3713);
	hdmirx_wr_phy(MPLL_PARAMETERS4,    mpll_param4);
	hdmirx_wr_phy(MPLL_PARAMETERS5,    0x5492);
	hdmirx_wr_phy(MPLL_PARAMETERS6,    0x4b0d);
	hdmirx_wr_phy(MPLL_PARAMETERS7,    0x4760);
	hdmirx_wr_phy(MPLL_PARAMETERS8,    0x008c);
	hdmirx_wr_phy(MPLL_PARAMETERS9,    0x0010);
	hdmirx_wr_phy(MPLL_PARAMETERS10,   0x2d20);
	hdmirx_wr_phy(MPLL_PARAMETERS11, 0x2e31);
	hdmirx_wr_phy(MPLL_PARAMETERS12, 0x4b64);
	hdmirx_wr_phy(MPLL_PARAMETERS13, 0x2493);
	hdmirx_wr_phy(MPLL_PARAMETERS14, 0x676d);
	hdmirx_wr_phy(MPLL_PARAMETERS15, 0x23e0);
	hdmirx_wr_phy(MPLL_PARAMETERS16, 0x001b);
	hdmirx_wr_phy(MPLL_PARAMETERS17, 0x2218);
	hdmirx_wr_phy(MPLL_PARAMETERS18, 0x1b25);
	hdmirx_wr_phy(MPLL_PARAMETERS19, 0x2492);
	hdmirx_wr_phy(MPLL_PARAMETERS20, 0x48ea);
	hdmirx_wr_phy(MPLL_PARAMETERS21, 0x0011);
	hdmirx_wr_phy(MPLL_PARAMETERS22, 0x04d2);
	hdmirx_wr_phy(MPLL_PARAMETERS23, 0x0414);

	hdmirx_wr_phy(0x43, fat_bit_status);
	hdmirx_wr_phy(0x63, fat_bit_status);
	hdmirx_wr_phy(0x83, fat_bit_status);

	/* Configuring I2C to work in fastmode */
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_MODE,	 0x1);
	/* disable overload protect for Philips DVD */
	/* NOTE!!!!! don't remove below setting */
	hdmirx_wr_phy(OVL_PROT_CTRL, 0xa);

	data32 = 0;
	data32 |= 0	<< 15;
	data32 |= 0	<< 13;
	data32 |= 0	<< 12;
	data32 |= fast_switching << 11;
	data32 |= 0	<< 10;
	data32 |= rx.phy.fsm_enhancement << 9;
	data32 |= 0	<< 8;
	data32 |= 0	<< 7;
	data32 |= dcm << 5;
	data32 |= 0	<< 3;
	data32 |= rx.phy.port_select_ovr_en << 2;
	data32 |= rx_port_sel << 0;

	hdmirx_wr_phy(PHY_SYSTEM_CONFIG,
		(rx.phy.phy_system_config_force_val != 0) ?
		rx.phy.phy_system_config_force_val : data32);

	hdmirx_wr_phy(PHY_CMU_CONFIG,
		(rx.phy.phy_cmu_config_force_val != 0) ?
		rx.phy.phy_cmu_config_force_val :
		((rx.phy.lock_thres << 10) | (1 << 9) |
			(((1 << 9) - 1) & ((rx.phy.cfg_clk * 4) / 1000))));

	hdmirx_wr_phy(PHY_CH0_EQ_CTRL3, eq_setting[EQ_CH0]);
	hdmirx_wr_phy(PHY_CH1_EQ_CTRL3, eq_setting[EQ_CH1]);
	hdmirx_wr_phy(PHY_CH2_EQ_CTRL3, eq_setting[EQ_CH2]);
	if ((0 == eq_setting[EQ_CH0]) &&
		(0 == eq_setting[EQ_CH1]) &&
		(0 == eq_setting[EQ_CH2]))
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);
	else
		hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x40);

	/*hdmirx_phy_clk_rate_monitor();*/

	data32 = 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx_port_sel << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);

	rx_print("%s  %d Done!\n", __func__, rx.port);
}

bool hdmirx_phy_check_tmds_valid(void)
{
	if ((((hdmirx_rd_phy(0x30) & 0x80) == 0x80) | finish_flag[EQ_CH0]) &&
	(((hdmirx_rd_phy(0x50) & 0x80) == 0x80) | finish_flag[EQ_CH1]) &&
	(((hdmirx_rd_phy(0x70) & 0x80) == 0x80) | finish_flag[EQ_CH2]))
		return true;
	else
		return false;
}

enum phy_eq_states_e hdmirx_phy_get_eq_state(void)
{
	return phy_private_data->phy_eq_state;
}

void hdmirx_phy_EQ_workaround_init(void)
{
	/* ConfEqualSingle */
	hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);
	rx_print("hdmirx_phy_EQ_workaround_init\n");
	hdmirx_wr_phy(PHY_EQCTRL1_CH0, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL1_CH1, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL1_CH2, 0x0211);
}

