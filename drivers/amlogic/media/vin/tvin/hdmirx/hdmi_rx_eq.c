// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_eq.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"

static int fat_bit_status;
static int min_max_diff = 4;
struct st_eq_data eq_ch0;
struct st_eq_data eq_ch1;
struct st_eq_data eq_ch2;
enum eq_sts_e eq_sts = E_EQ_START;
/* variable define*/
int long_cable_best_setting = 6;
int delay_ms_cnt = 5; /* 5 */
MODULE_PARM_DESC(delay_ms_cnt, "\n delay_ms_cnt\n");
module_param(delay_ms_cnt, int, 0664);

int eq_max_setting = 7;
int eq_dbg_ch0;
int eq_dbg_ch1;
int eq_dbg_ch2;
/*
 * select the mode to config eq setting
 * 0: no pddq mode	1: use pddq down and up mode
 */
u32 phy_pddq_en;
/*------------------------variable define end----------------------*/

bool eq_maxvsmin(int ch0setting, int ch1setting, int ch2setting)
{
	int min = ch0setting;
	int max = ch0setting;

	if (ch1setting > max)
		max = ch1setting;
	if (ch2setting > max)
		max = ch2setting;
	if (ch1setting < min)
		min = ch1setting;
	if (ch2setting < min)
		min = ch2setting;
	if ((max - min) > min_max_diff) {
		rx_pr("MINMAX ERROR\n");
		return 0;
	}
	return 1;
}

void initvars(struct st_eq_data *ch_data)
{
	/* Slope accumulator */
	ch_data->acc = 0;
	/* Early Counter dataAcquisition data */
	ch_data->acq = 0;
	ch_data->lastacq = 0;
	ch_data->validlongsetting = 0;
	ch_data->validshortsetting = 0;
	/* BEST Setting = short */
	ch_data->bestsetting = SHORTCABLESETTING;
	/* TMDS VALID not valid */
	ch_data->tmdsvalid = 0;
	memset(ch_data->acq_n, 0, sizeof(u16) * 15);
}

void hdmi_rx_phy_confequalsingle(void)
{
	hdmirx_wr_phy(PHY_EQCTRL1_CH0, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL1_CH1, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL1_CH2, 0x0211);
	hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x0024 | (AVGACQ << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x0024 | (AVGACQ << 11));
}

void hdmi_rx_phy_confequalsetting(u16 stepslope)
{
	hdmirx_wr_phy(PHY_EQCTRL4_CH0, stepslope);
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4024 | (AVGACQ << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4026 | (AVGACQ << 11));
}

void hdmi_rx_phy_confequalautocalib(void)
{
	hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
	hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1819);
	hdmirx_wr_phy(PHY_MAINFSM_CTL, 0x1809);
}

u16 rx_phy_rd_earlycnt_ch0(void)
{
	return hdmirx_rd_phy(PHY_EQSTAT3_CH0);
}

u16 rx_phy_rd_earlycnt_ch1(void)
{
	return hdmirx_rd_phy(PHY_EQSTAT3_CH1);
}

u16 rx_phy_rd_earlycnt_ch2(void)
{
	return hdmirx_rd_phy(PHY_EQSTAT3_CH2);
}

u16 hdmi_rx_phy_corestatusch0(void)
{
	return hdmirx_rd_phy(PHY_CORESTATUS_CH0);
}

u16 hdmi_rx_phy_corestatusch1(void)
{
	return hdmirx_rd_phy(PHY_CORESTATUS_CH1);
}

u16 hdmi_rx_phy_corestatusch2(void)
{
	return hdmirx_rd_phy(PHY_CORESTATUS_CH2);
}

void rx_set_eq_run_state(enum eq_sts_e state)
{
	if (state <= E_EQ_FAIL) {
		eq_sts = state;
		/*rx_pr("run_eq_flag: %d\n", eq_sts);*/
	}
}

enum eq_sts_e rx_get_eq_run_state(void)
{
	return eq_sts;
}

void eq_dwork_handler(struct work_struct *work)
{
	unsigned int i;

	cancel_delayed_work(&eq_dwork);

	/* for tl1 no SW eq */
	if (rx.chip_id >= CHIP_ID_TL1)
		return;

	for (i = 0; i < NTRYS; i++) {
		if (settingfinder() == 1) {
			rx_pr("EQ-%d-%d-%d-",
			      eq_ch0.bestsetting,
			      eq_ch1.bestsetting,
			      eq_ch2.bestsetting);

			if (eq_maxvsmin(eq_ch0.bestsetting,
					eq_ch1.bestsetting,
					eq_ch2.bestsetting) == 1) {
				eq_sts = E_EQ_PASS;
				if (log_level & EQ_LOG)
					rx_pr("pass\n");
				break;
			}
			if (log_level & EQ_LOG)
				rx_pr("fail\n");
		}
	}
	if (i >= NTRYS) {
		eq_ch0.bestsetting = ERRORCABLESETTING;
		eq_ch1.bestsetting = ERRORCABLESETTING;
		eq_ch2.bestsetting = ERRORCABLESETTING;
		if (log_level & EQ_LOG)
			rx_pr("EQ fail-retry\n");
		eq_sts = E_EQ_FAIL;
	}
	eq_cfg();
	/*rx_set_eq_run_state(E_EQ_FINISH);*/
	/*return;*/
}

void eq_run(void)
{
	queue_delayed_work(eq_wq,
			   &eq_dwork, msecs_to_jiffies(1));
}

void eq_cfg(void)
{
	hdmirx_wr_phy(PHY_EQCTRL4_CH0, 1 << eq_ch0.bestsetting);
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4024 | (AVGACQ << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH0, 0x4026 | (AVGACQ << 11));

	hdmirx_wr_phy(PHY_EQCTRL4_CH1, 1 << eq_ch1.bestsetting);
	hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x4024 | (AVGACQ << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH1, 0x4026 | (AVGACQ << 11));

	hdmirx_wr_phy(PHY_EQCTRL4_CH2, 1 << eq_ch2.bestsetting);
	hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x4024 | (AVGACQ << 11));
	hdmirx_wr_phy(PHY_EQCTRL2_CH2, 0x4026 | (AVGACQ << 11));

	/* As for the issue of "Toggling PDDQ after SW EQ sometime effect
	 * HDCP Authentication", SNPS suggestion is to: Skip the PDDQ toggle
	 * after SW EQ, and use below settings instead to update the PHY.
	 * The settings below can move the PHY FSM machine to equalization
	 * again.
	 */
	if (phy_pddq_en) {
		hdmirx_phy_pddq(1);
		hdmirx_phy_pddq(0);
	} else {
		hdmi_rx_phy_confequalautocalib();
	}
}

u8 testtype(u16 setting, struct st_eq_data *ch_data)
{
	u16 stepslope = 0;
	/* LONG CABLE EQUALIZATION */
	if (ch_data->acq < ch_data->lastacq &&
	    ch_data->tmdsvalid == 1) {
		ch_data->acc += (ch_data->lastacq - ch_data->acq);
		if (ch_data->validlongsetting == 0 &&
		    ch_data->acq < EQUALIZEDCOUNTERVALUE &&
		    ch_data->acc > ACCMINLIMIT) {
			ch_data->bestlongsetting = setting;
			ch_data->validlongsetting = 1;
		}
		stepslope = ch_data->lastacq - ch_data->acq;
	}
	/* SHORT CABLE EQUALIZATION */
	if (ch_data->tmdsvalid == 1 &&
	    ch_data->validshortsetting == 0) {
		/* Short setting better than default, system over-equalized */
		if (setting < SHORTCABLESETTING &&
		    ch_data->acq < EQUALIZEDCOUNTERVALUE)  {
			ch_data->validshortsetting = 1;
			ch_data->bestshortsetting = setting;
		}
		/* default Short setting is valid */
		if (setting == SHORTCABLESETTING) {
			ch_data->validshortsetting = 1;
			ch_data->bestshortsetting = SHORTCABLESETTING;
		}
	}
	/* Exit type Long cable
	 * (early-late count curve well behaved
	 * and 50% threshold achived)
	 */
	if (ch_data->validlongsetting  == 1 &&
	    ch_data->acc > ACCLIMIT) {
		ch_data->bestsetting = ch_data->bestlongsetting;
		if (ch_data->bestsetting > long_cable_best_setting)
			ch_data->bestsetting = long_cable_best_setting;
		if (log_level & EQ_LOG)
			rx_pr("longcable1");
		return E_LONG_CABLE;
	}
	/* Exit type short cable
	 * (early-late count curve  behaved as a short cable)
	 */
	if (setting == eq_max_setting &&
	    ch_data->acc < ACCLIMIT &&
	    ch_data->validshortsetting == 1) {
		ch_data->bestsetting = ch_data->bestshortsetting;
		if (log_level & EQ_LOG)
			rx_pr("shortcable");
		return E_SHORT_CABLE;
	}
	/* Exit type long cable
	 * (early-late count curve well behaved
	 * nevertheless 50% threshold not achieved
	 */
	if (setting == eq_max_setting &&
	    ch_data->tmdsvalid == 1 &&
	    ch_data->acc > ACCLIMIT &&
	    stepslope > MINSLOPE) {
		ch_data->bestsetting = long_cable_best_setting;
		if (log_level & EQ_LOG)
			rx_pr("longcable2");
		return E_LONG_CABLE2;
	}
	/* error cable */
	if (setting == eq_max_setting) {
		if (log_level & EQ_LOG)
			rx_pr("errcable");
		ch_data->bestsetting = ERRORCABLESETTING;
		return E_ERR_CABLE;
	}
	/* Cable not detected,
	 * continue to next setting
	 */
	return E_CABLE_NOT_FOUND;
}

u8 aquireearlycnt(u16 setting)
{
	u16 stepslope = 0x0001;
	int timeout_cnt = 10;

	stepslope = stepslope << setting;
	hdmi_rx_phy_confequalsetting(stepslope);
	hdmi_rx_phy_confequalautocalib();
	/* mdelay(delay_ms_cnt); */
	while (timeout_cnt--) {
		mdelay(delay_ms_cnt);
		eq_ch0.tmdsvalid =
			(hdmi_rx_phy_corestatusch0() & 0x0080) > 0 ? 1 : 0;
		eq_ch1.tmdsvalid =
			(hdmi_rx_phy_corestatusch1() & 0x0080) > 0 ? 1 : 0;
		eq_ch2.tmdsvalid =
			(hdmi_rx_phy_corestatusch2() & 0x0080) > 0 ? 1 : 0;
		if ((eq_ch0.tmdsvalid |
		    eq_ch1.tmdsvalid |
		    eq_ch2.tmdsvalid) != 0)
			break;
	}
	if ((eq_ch0.tmdsvalid |
	    eq_ch1.tmdsvalid |
	    eq_ch2.tmdsvalid) == 0) {
		if (log_level & EQ_LOG)
			rx_pr("invalid-earlycnt\n");
		return 0;
	}

	/* End the acquisitions if no TMDS valid */
	/* hdmi_rx_phy_confequalsetting(stepslope); */
	/* phy_conf_eq_setting(setting, setting, setting); */
	/* sleep_time_CDR should be enough */
	/*	to have TMDS valid asserted. */
	/* TMDS VALID can be obtained either */
	/*	by per channel basis or global pin */
	/* TMDS VALID BY channel basis (Option #1) */
	/* Get early counters */
	eq_ch0.acq = rx_phy_rd_earlycnt_ch0() >> AVGACQ;
	eq_ch0.acq_n[setting] = eq_ch0.acq;
	if (log_level & ERR_LOG)
		rx_pr("eq_ch0_acq #%d = %d\n", setting, eq_ch0.acq);
	eq_ch1.acq = rx_phy_rd_earlycnt_ch1() >> AVGACQ;
	eq_ch1.acq_n[setting] = eq_ch1.acq;
	if (log_level & ERR_LOG)
		rx_pr("eq_ch1_acq #%d = %d\n", setting, eq_ch1.acq);
	eq_ch2.acq = rx_phy_rd_earlycnt_ch2() >> AVGACQ;
	eq_ch2.acq_n[setting] = eq_ch2.acq;
	if (log_level & ERR_LOG)
		rx_pr("eq_ch2_acq #%d = %d\n", setting, eq_ch2.acq);

	return 1;
}

u8 settingfinder(void)
{
	u16 actsetting = 0;
	u16 retcodech0 = 0;
	u16 retcodech1 = 0;
	u16 retcodech2 = 0;
	u8 tmds_valid = 0;

	initvars(&eq_ch0);
	initvars(&eq_ch1);
	initvars(&eq_ch2);

	/* Get statistics of early-late counters for setting 0 */
	tmds_valid = aquireearlycnt(actsetting);

	while (retcodech0 == 0 || retcodech1 == 0 || retcodech2 == 0) {
		actsetting++;
		/* Update last acquisition value, */
		/* for threshold crossing detection */
		eq_ch0.lastacq = eq_ch0.acq;
		eq_ch1.lastacq = eq_ch1.acq;
		eq_ch2.lastacq = eq_ch2.acq;
		/* Get statistics of early-late */
		/* counters for next setting */
		tmds_valid = aquireearlycnt(actsetting);
		/* check for cable type, stop after detection */
		if (retcodech0 == 0) {
			retcodech0 = testtype(actsetting, &eq_ch0);
			if ((log_level & EQ_LOG) && retcodech0)
				rx_pr("-CH0\n");
		}
		if (retcodech1 == 0) {
			retcodech1 = testtype(actsetting, &eq_ch1);
			if ((log_level & EQ_LOG) && retcodech1)
				rx_pr("-CH1\n");
		}
		if (retcodech2 == 0) {
			retcodech2 = testtype(actsetting, &eq_ch2);
			if ((log_level & EQ_LOG) && retcodech2)
				rx_pr("-CH2\n");
		}
	}
	if (retcodech0 == 255 || retcodech1 == 255 || retcodech2 == 255)
		return 0;

	return 1;
}

void rx_eq_cfg(u8 ch0, u8 ch1, u8 ch2)
{
	hdmirx_wr_bits_dwc(DWC_SNPS_PHYG3_CTRL, PDDQ_DW, 1);

	hdmirx_wr_phy(PHY_CH0_EQ_CTRL3, ch0);
	hdmirx_wr_phy(PHY_CH1_EQ_CTRL3, ch1);
	hdmirx_wr_phy(PHY_CH2_EQ_CTRL3, ch2);
	hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x40);

	hdmirx_wr_bits_dwc(DWC_SNPS_PHYG3_CTRL, PDDQ_DW, 0);
}

bool is_6g_mode(void)
{
	return (rx_get_scdc_clkrate_sts() == 1) ? true : false;
}

struct eq_cfg_coef_s eq_cfg_coef_tbl[] = {
	{0x0c03,	4}, /* 3G */
	{0,			4}, /* HD */
	{0,			4}, /* 74M */
	{0,			4}, /* 27M */
	{0x0e03,	2}  /* 6G */
};

/*
 *rx_eq_algorithm
 * 6G 0, clk rate
 * 3G 0,
 * 1.5G 1
 * 74M 2
 * 27M 3
 */
int rx_eq_algorithm(void)
{
	static u8 pre_eq_freq = 0xff;
	u8 pll_rate = hdmirx_rd_phy(PHY_MAINFSM_STATUS1) >> 9 & 3;

	if (is_6g_mode())
		pll_rate = E_EQ_6G;

	rx_pr("pll rate pre:%d, cur:%d\n", pre_eq_freq, pll_rate);
	rx_pr("eq_sts = %d\n", eq_sts);

	if ((eq_dbg_ch0) || (eq_dbg_ch1) || (eq_dbg_ch2))  {
		rx_eq_cfg(eq_dbg_ch0, eq_dbg_ch1, eq_dbg_ch2);
		return 0;
	}
	if (pre_eq_freq == pll_rate) {
		if (eq_sts == E_EQ_PASS ||
		    eq_sts == E_EQ_SAME) {
			eq_sts = E_EQ_SAME;
			rx_pr("same pll rate\n");
			return 0;
		}
	}
	if ((pll_rate & 0x2) == E_EQ_SD) {
		eq_sts = E_EQ_FINISH;
		pre_eq_freq = pll_rate;
		rx_pr("low pll rate\n");
		return 0;
	}

	pre_eq_freq = pll_rate;
	fat_bit_status = eq_cfg_coef_tbl[pll_rate].fat_bit_sts;
	min_max_diff = eq_cfg_coef_tbl[pll_rate].min_max_diff;

	hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE2, 0x0);
	hdmi_rx_phy_confequalsingle();
	hdmirx_wr_phy(PHY_EQCTRL6_CH0, fat_bit_status);
	hdmirx_wr_phy(PHY_EQCTRL6_CH1, fat_bit_status);
	hdmirx_wr_phy(PHY_EQCTRL6_CH2, fat_bit_status);
	eq_run();

	eq_sts = E_EQ_START;

	return 1;
}

void dump_eq_data(void)
{
	int i = 0;

	rx_pr("/n*****************\n");
	for (i = 0; i < 15; i++)
		rx_pr("CH0-acq #%d: %d\n", i, eq_ch0.acq_n[i]);
	for (i = 0; i < 15; i++)
		rx_pr("CH1-acq #%d: %d\n", i, eq_ch1.acq_n[i]);
	for (i = 0; i < 15; i++)
		rx_pr("CH2-acq #%d: %d\n", i, eq_ch2.acq_n[i]);
}

