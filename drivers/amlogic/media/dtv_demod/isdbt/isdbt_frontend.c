// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#define __DVB_CORE__	/*ary 2018-1-31*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/firmware.h>
#include <linux/err.h>	/*IS_ERR*/
#include <linux/clk.h>	/*clk tree*/
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/crc32.h>

#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>

#include "aml_demod.h"
#include "demod_func.h"
#include "demod_dbg.h"
#include "amlfrontend.h"
#include "isdbt_frontend.h"
#include <linux/amlogic/aml_dtvdemod.h>

#define ISDBT_TIME_CHECK_SIGNAL 400
#define ISDBT_RESET_IN_UNLOCK_TIMES 40
#define ISDBT_FSM_CHECK_SIGNAL 7

//isdb-t
MODULE_PARM_DESC(isdbt_check_signal_time, "");
static unsigned int isdbt_check_signal_time = ISDBT_TIME_CHECK_SIGNAL;
module_param(isdbt_check_signal_time, int, 0644);

MODULE_PARM_DESC(isdbt_reset_in_unlock_times, "");
static unsigned int isdbt_reset_in_unlock_times = ISDBT_RESET_IN_UNLOCK_TIMES;
module_param(isdbt_reset_in_unlock_times, int, 0644);

MODULE_PARM_DESC(isdbt_lock_continuous_cnt, "");
static unsigned int isdbt_lock_continuous_cnt = 1;
module_param(isdbt_lock_continuous_cnt, int, 0644);

MODULE_PARM_DESC(isdbt_lost_continuous_cnt, "");
static unsigned int isdbt_lost_continuous_cnt = 10;
module_param(isdbt_lost_continuous_cnt, int, 0644);

int dvbt_isdbt_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dvbt *demod_dvbt)
{
	int ret = 0;
	u8_t demod_mode = 1;
	u8_t bw, sr, ifreq, agc_mode;
	u32_t ch_freq;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D)
		bw = BANDWIDTH_AUTO;
	else
		bw = demod_dvbt->bw;

	sr = demod_dvbt->sr;
	ifreq = demod_dvbt->ifreq;
	agc_mode = demod_dvbt->agc_mode;
	ch_freq = demod_dvbt->ch_freq;
	demod_mode = demod_dvbt->dat0;
	PR_DVBT("bw:%d, sr:%d, ifreq:%d, agc_mode:%d, ch_freq:%d, demod_mode:%d\n",
		bw, sr, ifreq, agc_mode, ch_freq, demod_mode);
	if (ch_freq < 1000 || ch_freq > 900000000) {
		/* PR_DVBT("Error: Invalid Channel Freq option %d\n",*/
		/* ch_freq); */
		ch_freq = 474000;
		ret = -1;
	}

	/*if (demod_mode < 0 || demod_mode > 4) {*/
	if (demod_mode > 4) {
		/* PR_DVBT("Error: Invalid demod mode option %d\n",*/
		/* demod_mode); */
		demod_mode = 1;
		ret = -1;
	}

	demod->demod_status.ch_mode = 0;	/* TODO */
	demod->demod_status.agc_mode = agc_mode;
	demod->demod_status.ch_freq = ch_freq;
	/*   if (demod_i2c->tuner == 1) */
	/*     demod_sta->ch_if = 36130;*/
	/* else if (demod_i2c->tuner == 2)*/
	/*     demod_sta->ch_if = 4570;*/
	/* else if (demod_i2c->tuner == 3)*/
	/*     demod_sta->ch_if = 4000;// It is nouse.(alan)*/
	/* else if (demod_i2c->tuner == 7)*/
	/*     demod_sta->ch_if = 5000;//silab 5000kHz IF*/

	demod->demod_status.ch_bw = (8 - bw) * 1000;
	demod->demod_status.symb_rate = 0;	/* TODO */

	/* bw=0; */
	demod_mode = 1;
	/* for si2176 IF:5M   sr 28.57 */
	sr = 4;
	ifreq = 4;
	PR_INFO("%s:1:bw=%d, demod_mode=%d\n", __func__, bw, demod_mode);

	/*bw = BANDWIDTH_AUTO;*/
	if (bw == BANDWIDTH_AUTO)
		demod_mode = 2;

	ofdm_initial(bw,
			/* 00:8M 01:7M 10:6M 11:5M */
		     sr,
		     /* 00:45M 01:20.8333M 10:20.7M 11:28.57  100:24m */
		     ifreq,
		     /* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		     demod_mode - 1,
		     /* 00:DVBT,01:ISDBT */
		     1
		     /* 0: Unsigned, 1:TC */
	    );
	PR_DVBT("DVBT/ISDBT mode\n");

	return ret;
}

int dvbt_isdbt_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = dvbt_isdbt_rd_reg((0xbf << 2)) & 0xffff;

	return 0;
}

int gxtv_demod_dvbt_isdbt_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_ISDBT("[id %d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

void isdbt_reset_demod(void)
{
	dvbt_isdbt_wr_reg_new(0x02, 0x00800000);
	dvbt_isdbt_wr_reg_new(0x00, 0x00000000);
	dvbt_isdbt_wr_reg_new(0x0e, 0xffff);
	dvbt_isdbt_wr_reg_new(0x02, 0x11001a);
	msleep(20);
	dvbt_isdbt_wr_reg_new(0x02, dvbt_isdbt_rd_reg_new(0x02) | (1 << 0));
	dvbt_isdbt_wr_reg_new(0x02, dvbt_isdbt_rd_reg_new(0x02) | (1 << 24));
}

int dvbt_isdbt_read_status(struct dvb_frontend *fe, enum fe_status *status, bool re_tune)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	unsigned char s = 0;
	unsigned int fsm = 0;
	s16 strength = 0;
	int lock = 0, snr10 = 0;
	static int has_signal;
	static int no_signal_cnt, unlock_cnt;
	int lock_continuous_cnt = isdbt_lock_continuous_cnt > 1 ? isdbt_lock_continuous_cnt : 1;
	int lost_continuous_cnt = isdbt_lost_continuous_cnt > 1 ? isdbt_lost_continuous_cnt : 1;
	int check_signal_time = isdbt_check_signal_time > 1 ? isdbt_check_signal_time :
		ISDBT_TIME_CHECK_SIGNAL;
	int reset_in_unlock_times = isdbt_reset_in_unlock_times > 0 ? isdbt_reset_in_unlock_times :
		ISDBT_RESET_IN_UNLOCK_TIMES;

	if (re_tune) {
		demod->time_start = jiffies_to_msecs(jiffies);
		*status = 0;
		demod->last_status = 0;
		has_signal = 0;
		no_signal_cnt = 0;
		unlock_cnt = 0;
		demod->last_lock = 0;

		return 0;
	}

	gxtv_demod_isdbt_read_signal_strength(fe, &strength);
	if (strength < THRD_TUNER_STRENGTH_ISDBT) {
		*status = FE_TIMEDOUT;

		PR_ISDBT("strength [%d] no signal(%d)\n",
				strength, THRD_TUNER_STRENGTH_ISDBT);

		if (!(no_signal_cnt++ % 20))
			isdbt_reset_demod();

		unlock_cnt = 0;

		goto finish;
	}

	if (no_signal_cnt) {
		no_signal_cnt = 0;
		isdbt_reset_demod();
	}

	demod->time_passed = jiffies_to_msecs(jiffies) - demod->time_start;
	fsm = dvbt_isdbt_rd_reg(0x2a << 2);
	if ((fsm & 0xF) >= ISDBT_FSM_CHECK_SIGNAL)
		has_signal = 1;
	snr10 = (((dvbt_isdbt_rd_reg((0x0a << 2))) >> 20) & 0x3ff) * 10 / 8;
	demod->real_para.snr = snr10;
	PR_ISDBT("fsm=0x%x, strength=%ddBm snr=%d dBx10, time_passed=%dms\n",
		fsm, strength, snr10, demod->time_passed);

	s = dvbt_isdbt_rd_reg(0x0) >> 12 & 1;
	if (s == 1) {
		lock = 1;
	} else if (demod->time_passed < check_signal_time ||
		(demod->time_passed < TIMEOUT_ISDBT && has_signal && demod->last_lock == 0)) {
		lock = 0;
	} else {
		lock = -1;
		if (!has_signal && demod->last_lock == 0) {
			demod->last_lock = -lost_continuous_cnt;
			*status = FE_TIMEDOUT;
			PR_ISDBT("not isdb-t signal\n");

			goto finish;
		}
	}

	//The status is updated only when the status continuously reaches the threshold of times
	if (lock < 0) {
		if (!(++unlock_cnt % reset_in_unlock_times))
			isdbt_reset_demod();

		if (demod->last_lock >= 0) {
			demod->last_lock = -1;
			PR_ISDBT("lost signal first\n");
		} else if (demod->last_lock <= -lost_continuous_cnt) {
			demod->last_lock = -lost_continuous_cnt;
			PR_ISDBT("lost signal continue\n");
		} else {
			demod->last_lock--;
			PR_ISDBT("lost signal times:%d\n", demod->last_lock);
		}

		if (demod->last_lock <= -lost_continuous_cnt)
			*status = FE_TIMEDOUT;
		else
			*status = 0;
	} else if (lock > 0) {
		unlock_cnt = 0;

		if (demod->last_lock <= 0) {
			demod->last_lock = 1;
			PR_ISDBT("lock signal first\n");
		} else if (demod->last_lock >= lock_continuous_cnt) {
			demod->last_lock = lock_continuous_cnt;
			PR_ISDBT("lock signal continue\n");
		} else {
			demod->last_lock++;
			PR_ISDBT("lock signal times:%d\n", demod->last_lock);
		}

		if (demod->last_lock >= lock_continuous_cnt)
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL |
				FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;
		else
			*status = 0;
	} else {
		*status = 0;
		PR_ISDBT("==> wait\n");
	}

finish:
	if (demod->last_status != *status && *status != 0)
		PR_INFO("!! >> %s << !!, freq=%d, time_passed=%dms\n", *status == FE_TIMEDOUT ?
			"UNLOCK" : "LOCK", fe->dtv_property_cache.frequency, demod->time_passed);
	demod->last_status = *status;

	return 0;
}

int dvbt_isdbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*
	 * It is safe to discard "params" here, as the DVB core will sync
	 * fe->dtv_property_cache with fepriv->parameters_in, where the
	 * DVBv3 params are stored. The only practical usage for it indicate
	 * that re-tuning is needed, e. g. (fepriv->state & FESTATE_RETUNE) is
	 * true.
	 */
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*delay = HZ / 20;

	if (re_tune) {
		PR_INFO("%s [id %d]: re_tune\n", __func__, demod->id);
		demod->en_detect = 1; /*fist set*/
		dvbt_isdbt_set_frontend(fe);
		dvbt_isdbt_read_status(fe, status, re_tune);
		return 0;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] isdbt not enable\n", demod->id);
		return 0;
	}

	/*polling*/
	dvbt_isdbt_read_status(fe, status, re_tune);

	return 0;
}

int gxtv_demod_isdbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

int gxtv_demod_isdbt_read_signal_strength(struct dvb_frontend *fe,
		s16 *strength)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*strength = (s16)tuner_get_ch_power(fe);

	if (tuner_find_by_name(fe, "r842") ||
		tuner_find_by_name(fe, "r836") ||
		tuner_find_by_name(fe, "r850"))
		*strength += 7;
	else if (tuner_find_by_name(fe, "mxl661"))
		*strength += 3;

	PR_ISDBT("[id %d] strength %d dbm\n", demod->id, *strength);

	return 0;
}

int dvbt_isdbt_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*struct aml_demod_sts demod_sts;*/
	struct aml_demod_dvbt param;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modul:%d, invert:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	/* bw == 0 : 8M*/
	/*       1 : 7M*/
	/*       2 : 6M*/
	/*       3 : 5M*/
	/* agc_mode == 0: single AGC*/
	/*             1: dual AGC*/
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	param.bw = dtvdemod_convert_bandwidth(c->bandwidth_hz);
	param.agc_mode = 1;
	/*ISDBT or DVBT : 0 is QAM, 1 is DVBT, 2 is ISDBT,*/
	/* 3 is DTMB, 4 is ATSC */
	param.dat0 = 1;
	demod->last_lock = -1;
	demod->last_status = 0;

	if (is_meson_t5w_cpu() || is_meson_t3_cpu() ||
		demod_is_t5d_cpu(devp)) {
		dvbt_isdbt_wr_reg((0x2 << 2), 0x111021b);
		dvbt_isdbt_wr_reg((0x2 << 2), 0x011021b);
		dvbt_isdbt_wr_reg((0x2 << 2), 0x011001b);

		if (is_meson_t3_cpu() && is_meson_rev_b())
			t3_revb_set_ambus_state(false, false);

		if (is_meson_t5w_cpu())
			t5w_write_ambus_reg(0xe138, 0x1, 23, 1);
	}

	tuner_set_params(fe);
	msleep(20);
	dvbt_isdbt_set_ch(demod, &param);

	if (is_meson_t5w_cpu())
		t5w_write_ambus_reg(0x3c4e, 0x0, 23, 1);

	return 0;
}

int gxtv_demod_isdbt_get_frontend(struct dvb_frontend *fe)
{
	return 0;
}

int dvbt_isdbt_init(struct aml_dtvdemod *demod)
{
	int ret = 0;
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk = &devp->data->dig_clk;

	PR_DBG("DVB-T/isdbt init\n");

	memset(&sys, 0, sizeof(sys));
	memset(&demod->demod_status, 0, sizeof(demod->demod_status));
	demod->last_status = 0;

	demod->demod_status.delsys = SYS_ISDBT;
	sys.adc_clk = ADC_CLK_24M;
	sys.demod_clk = DEMOD_CLK_60M;
	demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.adc_freq = sys.adc_clk;
	demod->demod_status.clk_freq = sys.demod_clk;

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D)
		dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x507);
	else
		dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x501);

	ret = demod_set_sys(demod, &sys);

	return ret;
}
