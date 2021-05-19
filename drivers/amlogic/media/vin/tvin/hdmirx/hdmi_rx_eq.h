/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_EQ_H
#define _HDMI_RX_EQ_H
/* time mS */
#define WAITTIMESTARTCONDITIONS	3
/* WAIT FOR, CDR LOCK and TMDSVALID */
#define SLEEP_TIME_CDR	10
/* Maximum slope  accumulator to consider the cable as a short cable */
#define ACCLIMIT	360
/* Minimum slope accumulator to consider the following setting */
#define ACCMINLIMIT	0
/* suitable for a long cable */
/* Maximum allowable setting, HW as the maximum = 15, should */
#define MAXSETTING	14
/* only be need for ultra long cables at high rates, */
/* condition never detected on LAB */
/* Default setting for short cables, */
/* if system cannot find any one better than this. */
#define SHORTCABLESETTING	4
/* Default setting when system cannot detect the cable type */
#define ERRORCABLESETTING	4
/* Minimum current slope needed to consider the cable */
/* as "very long" and therefore */
#define MINSLOPE	50
/* max setting is suitable for equalization */
/* Maximum number of early-counter measures to average each setting */
#define AVGACQ	4
/* Threshold Value for the statistics counter */
/* 3'd0: Selects counter threshold 1K */
/* 3'd1: Selects counter threshold 2K */
/* 3'd2: Selects counter threshold 4K */
/* 3d3: Selects counter threshold 8K */
/* 3d4: Selects counter threshold 16K */
/* Number of retries in case of algorithm ending with errors */
#define NTRYS	1
/* theoretical threshold for an equalized system */
#define EQUALIZEDCOUNTERVALUE	512
/* theoretical threshold for an equalized system */
#define EQUALIZEDCOUNTERVALUE_HDMI20	512
/* Maximum  difference between pairs */
#define MINMAX_MAXDIFF	4
/* Maximum  difference between pairs  under HDMI2.0 MODE */
#define MINMAX_MAXDIFF_HDMI20	2
/* FATBIT MASK FOR hdmi-1.4 xxxx_001 or xxxx_110 */
#define EQ_FATBIT_MASK	0x0000
/* hdmi 1.4 ( pll rate = 00) xx00_001 or xx11_110 */
#define EQ_FATBIT_MASK_4k	0x0c03
/* for hdmi2.0 x000_001 or x111_110 */
#define EQ_FATBIT_MASK_HDMI20	0x0e03

#define block_delay_ms(x) msleep_interruptible((x))

/*macro define end*/

/*--------------------------enum define---------------------*/
enum eq_states_e {
	EQ_ENABLE,
	EQ_MANUAL,
	EQ_USE_PRE,
	EQ_DISABLE,
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

enum eq_sts_e {
	E_EQ_START,
	E_EQ_FINISH,
	E_EQ_PASS,
	E_EQ_SAME,
	E_EQ_FAIL
};

enum e_eq_freq {
	E_EQ_3G,
	E_EQ_HD,
	E_EQ_SD,
	E_EQ_6G = 4,
	E_EQ_FREQ
};

struct eq_cfg_coef_s {
	u16 fat_bit_sts;
	u8 min_max_diff;
};

enum eq_cable_type_e {
	E_CABLE_NOT_FOUND,
	E_LONG_CABLE,
	E_SHORT_CABLE,
	E_LONG_CABLE2,
	E_ERR_CABLE = 255,
};

struct st_eq_data {
	/* Best long cable setting */
	u16 bestlongsetting;
	/* long cable setting detected and valid */
	u8 validlongsetting;
	/* best short cable setting */
	u16 bestshortsetting;
	/* best short cable setting detected and valid */
	u8 validshortsetting;
	/* TMDS Valid for channel */
	u8 tmdsvalid;
	/* best setting to be programed */
	u16 bestsetting;
	/* Accumulator register */
	u16 acc;
	/* Acquisition register */
	u16 acq;
	u16 acq_n[15];
	u16 lastacq;
	u8 eq_ref[3];
};

/*struct define end*/
extern struct st_eq_data eq_ch0;
extern struct st_eq_data eq_ch1;
extern struct st_eq_data eq_ch2;
extern int delay_ms_cnt;
extern int eq_max_setting;
extern u32 phy_pddq_en;
extern int eq_dbg_ch0;
extern int eq_dbg_ch1;
extern int eq_dbg_ch2;
extern int long_cable_best_setting;
extern enum eq_sts_e eq_sts;
/*--------------------------function declare------------------*/
int rx_eq_algorithm(void);
int hdmirx_phy_start_eq(void);
u8 settingfinder(void);
bool eq_maxvsmin(int ch0setting, int ch1setting, int ch2setting);
/* int hdmirx_phy_suspend_eq(void); */
bool hdmirx_phy_check_tmds_valid(void);
void hdmirx_phy_conf_eq_setting(int rx_port_sel,
				int ch0setting,
				int ch1setting,
				int ch2setting);
void eq_cfg(void);
void eq_run(void);
void rx_set_eq_run_state(enum eq_sts_e state);
enum eq_sts_e rx_get_eq_run_state(void);
void dump_eq_data(void);
void eq_dwork_handler(struct work_struct *work);

/*function declare end*/

#endif /*_HDMI_RX_EQ_H*/

