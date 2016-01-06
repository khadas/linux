/*
 * hdmi_rx_eq.h for HDMI device driver, and declare IO function,
 * structure, enum, used in TVIN AFE sub-module processing
 *
 * Copyright (C) 2014 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef _HDMI_RX_EQ_H
#define _HDMI_RX_EQ_H

/*-------------------macro define---------------------------*/
#define MIN_SLOPE		50
#define ACC_MIN_LIMIT	0
#define ACC_LIMIT		370
/* #define EQ_MAX_SETTING 12//for very long cable */
#define MINDIFF		4/* max min diff between data chs on hdmi1.4 */
#define MINDIFF_HDMI20	2/* max min diff between data chs on hdmi2.0 */
#define EQ_CLK_WAIT_MAX_COUNT 1000
#define EQ_CLK_WAIT_STABLE_COUNT (EQ_CLK_WAIT_MAX_COUNT - 900)
#define EQ_CLK_WAIT_DELAY 1
#define EQ_TMDS_VALID_WAIT_DELAY 2
/*#define EQ_CLK_RATE_WAIT 15*/
#define block_delay_ms(x) msleep_interruptible((x))

/* Default best setting */
#define EQ_DEFAULT_SETTING 4
/* wait time between early/late counter acquisitions */
#define EQ_WAITTIME						1
/* Slope acumulator, minimum limit  to
consider setting suitable for long cable */
#define EQ_SLOPEACM_MINTHRESHOLD			0
/* Slope acumulator threshold to considere as long cable */
#define EQ_SLOPEACM_LONG_CABLE_THRESHOLD	360
/* minimum slope at maximum setting to consider it as a long cable */
#define EQ_MINSLOPE_VERYLONGCABLE			50
/* threshold for equalized system */
#define EQ_COUNTERTHRESHOLD				512
/* threshold for equalized system */
#define EQ_COUNTERTHRESHOLD_HDMI20			512
/* Maximum allowable setting */
#define EQ_MAX_SETTING						7/* 13 */
/* Default best setting for short cables (electrical short length) */
#define EQ_SHORT_CABLE_BEST_SETTING		4
/* Default setting when not good equalization is achieved,
same as short cable */
#define EQ_ERROR_CABLE_BEST_SETTING		4
/* Stop averaging (Stable measures), if early/late counter
acquisitions are within the following range 1oread +-20 */
#define EQ_BOUNDARYSPREAD					20
/* Minimum number of early/late counter acquisitions
to considere a stable acquisition */
#define EQ_MIN_ACQ_STABLE_DETECTION			3
/* wait time between early/late counter acquisitions */
#define EQ_WAITTIME							1
/* Slope acumulator, minimum limit  to consider setting
suitable for long cable */
#define EQ_SLOPEACM_MINTHRESHOLD			0
/* Slope acumulator threshold to considere as long cable */
#define	EQ_SLOPEACM_LONG_CABLE_THRESHOLD	360
/* minimum slope at maximum setting to consider it as a long cable */
#define EQ_MINSLOPE_VERYLONGCABLE			50
/* hdmi 1.4 */
#define EQ_FATBIT_MASK						0
/* hdmi 1.4 & pll rate = 00 */
#define EQ_FATBIT_MASK_4k					0xc03
/* for hdmi2.0 */
#define EQ_FATBIT_MASK_HDMI20				0xe03

#define FSM_LOG_ENABLE		0x01
#define VIDEO_LOG_ENABLE	0x02
#define AUDIO_LOG_ENABLE	0x04
#define PACKET_LOG_ENABLE   0x08
#define CEC_LOG_ENABLE		0x10
#define REG_LOG_ENABLE		0x20

/*macro define end*/

/*--------------------------enum define---------------------*/
enum phy_eq_states_e {
	EQ_IDLE,
	EQ_DATA_START,
	EQ_SET_LOCK_VECTOR,
	EQ_SET_FORCE_FMS_STATE,
	EQ_CHECK_TMDS_VALID,
	EQ_AQUIRE_EARLY_COUNTER,
	EQ_GET_CABLE_TYPE,
	EQ_CONF_BEST_SETTING,
	EQ_SUCCESS_END,
	EQ_FAILED,
};

enum phy_eq_channel_e {
	EQ_CH0,
	EQ_CH1,
	EQ_CH2,
	EQ_CH_NUM,
};

/*enum define end*/

/*--------------------------struct define---------------------*/
struct phy_eq_algorithm_data_t {
	int nretry;
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
	int upperBound_acqCH0, upperBound_acqCH1, upperBound_acqCH2;
	int lowerBound_acqCH0, lowerBound_acqCH1, lowerBound_acqCH2;
	int outBound_acqCH0, outBound_acqCH1, outBound_acqCH2;

	/* Auxiliary variables to control algorithm status */
	bool ch0_error_cable_flag;
	bool ch1_error_cable_flag;
	bool ch2_error_cable_flag;
	bool tmds_valid_flag;

	/*Auxiliary variable to control how many times Algorithm
	will retry if any error is detected*/
	bool minmax_err_flag;
	int minmax_check_cnt;
	int pll_rate_value;
};

struct hdmirx_phy_data_t {
	struct completion phy_task_lock;
	enum phy_eq_states_e phy_eq_state;
	struct task_struct *task;
	int phy_clk_wait_count;
	bool task_running;
	bool exit_task_delay;/* exit clk stable delay */
	struct mutex state_lock;
	bool last_clk_rate;
};

/*struct define end*/

/*--------------------------function declare------------------*/
bool hdmirx_phy_clk_rate_monitor(void);
void hdmirx_phy_init(int rx_port_sel, int dcm);
void hdmirx_phy_EQ_workaround_init(void);
int hdmirx_phy_probe(void);
void hdmirx_phy_exit(void);
int hdmirx_phy_start_eq(void);
enum phy_eq_states_e hdmirx_phy_get_eq_state(void);
int hdmirx_phy_stop_eq(void);
void hdmirx_phy_reset(int rx_port_sel, int dcm);
int hdmirx_phy_suspend_eq(void);
bool hdmirx_phy_check_tmds_valid(void);

/*function declare end*/

#endif /*_HDMI_RX_EQ_H*/

