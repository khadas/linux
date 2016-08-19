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
/* time mS */
#define WaitTimeStartConditions	3
/* WAIT FOR, CDR LOCK and TMDSVALID */
#define sleep_time_CDR	10
/* Maximum slope  accumulator to consider the cable as a short cable */
#define AccLimit	360
/* Minimum slope accumulator to consider the following setting */
#define AccMinLimit	0
/* suitable for a long cable */
/* Maximum allowable setting, HW as the maximum = 15, should */
#define maxsetting	14
/* only be need for ultra long cables at high rates,
condition never detected on LAB */
/* Default setting for short cables,
if system cannot find any one better than this. */
#define shortcableSetting	4
/* Default setting when system cannot detect the cable type */
#define ErrorcableSetting	4
/* Minimum current slope needed to consider the cable
as "very long" and therefore */
#define minSlope	50
/* max setting is suitable for equalization */
/* Maximum number of early-counter measures to average each setting */
#define avgAcq	4
/* Threshold Value for the statistics counter */
/* 3'd0: Selects counter threshold 1K */
/* 3'd1: Selects counter threshold 2K */
/* 3'd2: Selects counter threshold 4K */
/* 3d3: Selects counter threshold 8K */
/* 3d4: Selects counter threshold 16K */
/* Number of retries in case of algorithm ending with errors */
#define MINMAX_nTrys	3
/* theoretical threshold for an equalized system */
#define equalizedCounterValue	512
/* theoretical threshold for an equalized system */
#define equalizedCounterValue_HDMI20	512
/* Maximum  difference between pairs */
#define MINMAX_maxDiff	4
/* Maximum  difference between pairs  under HDMI2.0 MODE */
#define MINMAX_maxDiff_HDMI20	2
/* FATBIT MASK FOR hdmi-1.4 xxxx_001 or xxxx_110 */
#define EQ_FATBIT_MASK	0x0000
/* hdmi 1.4 ( pll rate = 00) xx00_001 or xx11_110 */
#define EQ_FATBIT_MASK_4k	0x0c03
/* for hdmi2.0 x000_001 or x111_110 */
#define EQ_FATBIT_MASK_HDMI20	0x0e03

#define block_delay_ms(x) msleep_interruptible((x))

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
	EQ_INIT,
	EQ_MAINLOOP,
	EQ_END,
};

enum phy_eq_channel_e {
	EQ_CH0,
	EQ_CH1,
	EQ_CH2,
	EQ_CH_NUM,
};

enum phy_eq_cmd_e {
	EQ_START = 0x1,
	EQ_STOP,
};

/*struct define end*/
extern struct st_eq_data eq_ch0;
extern struct st_eq_data eq_ch1;
extern struct st_eq_data eq_ch2;

/*--------------------------function declare------------------*/
bool hdmirx_phy_clk_rate_monitor(void);
/* void hdmirx_phy_init(int rx_port_sel, int dcm); */
bool rx_need_eq_workaround(void);
int hdmirx_phy_probe(void);
void hdmirx_phy_exit(void);
int hdmirx_phy_start_eq(void);
uint8_t SettingFinder(void);
bool eq_maxvsmin(int ch0Setting, int ch1Setting, int ch2Setting);

/* int hdmirx_phy_suspend_eq(void); */
bool hdmirx_phy_check_tmds_valid(void);
void hdmirx_phy_conf_eq_setting(int rx_port_sel,
	int ch0Setting,	int ch1Setting, int ch2Setting);
void phy_conf_eq_setting(int ch0_lockVector,
				int ch1_lockVector, int ch2_lockVector);


/*function declare end*/

#endif /*_HDMI_RX_EQ_H*/

