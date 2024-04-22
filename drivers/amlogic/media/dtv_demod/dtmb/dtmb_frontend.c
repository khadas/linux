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
#include "dtmb_frontend.h"
#include "amlfrontend.h"
#include "addr_dtmb_top.h"
#include <linux/amlogic/aml_dtvdemod.h>

#define DTMB_TIME_CHECK_SIGNAL 150

static unsigned int dtmb_check_signal_time = DTMB_TIME_CHECK_SIGNAL;
MODULE_PARM_DESC(dtmb_check_signal_time, "");
module_param(dtmb_check_signal_time, int, 0644);

static unsigned int dtmb_lock_continuous_cnt = 1;
MODULE_PARM_DESC(dtmb_lock_continuous_cnt, "");
module_param(dtmb_lock_continuous_cnt, int, 0644);

static unsigned int dtmb_lost_continuous_cnt = 15;
MODULE_PARM_DESC(dtmb_lost_continuous_cnt, "");
module_param(dtmb_lost_continuous_cnt, int, 0644);

void gxtv_demod_dtmb_release(struct dvb_frontend *fe)
{
}

int gxtv_demod_dtmb_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct poll_machie_s *pollm = &demod->poll_machie;

	*status = pollm->last_s;

	return 0;
}

void dtmb_save_status(struct aml_dtvdemod *demod, unsigned int s)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	if (s) {
		pollm->last_s =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;

	} else {
		pollm->last_s = FE_TIMEDOUT;
	}
}

void dtmb_poll_start(struct aml_dtvdemod *demod)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	pollm->last_s = 0;
	pollm->flg_restart = 1;
	PR_DTMB("dtmb_poll_start2\n");
}

void dtmb_poll_stop(struct aml_dtvdemod *demod)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	pollm->flg_stop = 1;
}

void dtmb_set_delay(struct aml_dtvdemod *demod, unsigned int delay)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	pollm->delayms = delay;
	pollm->flg_updelay = 1;
}

unsigned int dtmb_is_update_delay(struct aml_dtvdemod *demod)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	return pollm->flg_updelay;
}

unsigned int dtmb_get_delay_clear(struct aml_dtvdemod *demod)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	pollm->flg_updelay = 0;

	return pollm->delayms;
}

void dtmb_poll_clear(struct aml_dtvdemod *demod)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	memset(pollm, 0, sizeof(struct poll_machie_s));
}

/*dtmb_poll_v3 is same as dtmb_check_status_txl*/
void dtmb_poll_v3(struct aml_dtvdemod *demod)
{
	struct poll_machie_s *pollm = &demod->poll_machie;
	unsigned int bch_tmp;
	unsigned int s;

	if (!pollm->state) {
		/* idle */
		/* idle -> start check */
		if (!pollm->flg_restart) {
			PR_DBG("x");
			return;
		}
	} else {
		if (pollm->flg_stop) {
			PR_DBG("dtmb poll stop!\n");
			dtmb_poll_clear(demod);
			dtmb_set_delay(demod, 3 * HZ);

			return;
		}
	}

	/* restart: clear */
	if (pollm->flg_restart) {
		PR_DBG("dtmb poll restart!\n");
		dtmb_poll_clear(demod);
	}
	PR_DBG("-");
	s = check_dtmb_fec_lock();

	/* bch exceed the threshold: wait lock*/
	if (pollm->state & DTMBM_BCH_OVER_CHEK) {
		if (pollm->crrcnt < DTMBM_POLL_CNT_WAIT_LOCK) {
			pollm->crrcnt++;

			if (s) {
				PR_DBG("after reset get lock again!cnt=%d\n",
					pollm->crrcnt);
				dtmb_constell_check();
				pollm->state = DTMBM_HV_SIGNEL_CHECK;
				pollm->crrcnt = 0;
				pollm->bch = dtmb_reg_r_bch();

				dtmb_save_status(demod, s);
			}
		} else {
			PR_DBG("can't lock after reset\n");
			pollm->state = DTMBM_NO_SIGNEL_CHECK;
			pollm->crrcnt = 0;
			/* to no signal*/
			dtmb_save_status(demod, s);
		}
		return;
	}

	if (s) {
		/*have signal*/
		if (!pollm->state) {
			pollm->state = DTMBM_CHEK_NO;
			PR_DBG("from idle to have signal wait 1\n");
			return;
		}
		if (pollm->state & DTMBM_CHEK_NO) {
			/*no to have*/
			PR_DBG("poll machie: from no signal to have signal\n");
			pollm->bch = dtmb_reg_r_bch();
			pollm->state = DTMBM_HV_SIGNEL_CHECK;

			dtmb_set_delay(demod, DTMBM_POLL_DELAY_HAVE_SIGNAL);
			return;
		}

		bch_tmp = dtmb_reg_r_bch();
		if (bch_tmp > (pollm->bch + 50)) {
			pollm->state = DTMBM_BCH_OVER_CHEK;

			PR_DBG("bch add,need reset,wait not to reset\n");
			dtmb_reset();

			pollm->crrcnt = 0;
			dtmb_set_delay(demod, DTMBM_POLL_DELAY_HAVE_SIGNAL);
		} else {
			pollm->bch = bch_tmp;
			pollm->state = DTMBM_HV_SIGNEL_CHECK;

			dtmb_save_status(demod, s);
			/*have signale to have signal*/
			dtmb_set_delay(demod, 300);
		}
		return;
	}

	/*no signal */
	if (!pollm->state) {
		/* idle -> no signal */
		PR_DBG("poll machie: from idle to no signal\n");
		pollm->crrcnt = 0;

		pollm->state = DTMBM_NO_SIGNEL_CHECK;
	} else if (pollm->state & DTMBM_CHEK_HV) {
		/*have signal -> no signal*/
		PR_DBG("poll machie: from have signal to no signal\n");
		pollm->crrcnt = 0;
		pollm->state = DTMBM_NO_SIGNEL_CHECK;
		dtmb_save_status(demod, s);
	}

	/*no signal check process */
	if (pollm->crrcnt < DTMBM_POLL_CNT_NO_SIGNAL) {
		dtmb_no_signal_check_v3(demod);
		pollm->crrcnt++;

		dtmb_set_delay(demod, DTMBM_POLL_DELAY_NO_SIGNAL);
	} else {
		dtmb_no_signal_check_finishi_v3(demod);
		pollm->crrcnt = 0;

		dtmb_save_status(demod, s);
		/*no signal to no signal*/
		dtmb_set_delay(demod, 300);
	}
}

void dtmb_poll_start_tune(struct aml_dtvdemod *demod, unsigned int state)
{
	struct poll_machie_s *pollm = &demod->poll_machie;

	dtmb_poll_clear(demod);

	pollm->state = state;
	if (state & DTMBM_NO_SIGNEL_CHECK)
		dtmb_save_status(demod, 0);
	else
		dtmb_save_status(demod, 1);

	PR_DTMB("dtmb_poll_start tune to %d\n", state);
}

/*come from gxtv_demod_dtmb_read_status, have ms_delay*/
int dtmb_poll_v2(struct dvb_frontend *fe, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int ilock;
	unsigned char s = 0;

	s = dtmb_check_status_gxtv(fe);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}

	if (demod->last_lock != ilock) {
		PR_INFO("%s [id %d]: %s.\n", __func__, demod->id,
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		demod->last_lock = ilock;
	}

	return 0;
}

/*this is ori gxtv_demod_dtmb_read_status*/
int gxtv_demod_dtmb_read_status_old(struct dvb_frontend *fe,
		enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int ilock;
	unsigned char s = 0;

	if (is_meson_gxtvbb_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		dtmb_check_status_gxtv(fe);
#endif
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		dtmb_bch_check(fe);
		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			dtmb_check_status_txl(fe);
	} else {
		return -1;
	}

	demod->real_para.snr = convert_snr(dtmb_reg_r_che_snr()) * 10;
	PR_DTMB("snr %d dBx10.\n", demod->real_para.snr);

	s = amdemod_stat_dtmb_islock(demod, SYS_DTMB);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (demod->last_lock != ilock) {
		PR_INFO("%s [%d]: %s.\n", __func__, demod->id,
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		demod->last_lock = ilock;
	}

	return 0;
}

int gxtv_demod_dtmb_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	unsigned int value = 0;

	value = dtmb_read_reg(DTMB_TOP_FEC_LDPC_IT_AVG);
	value = (value >> 16) & 0x1fff;
	*ber = 1000 * value / (8 * 188 * 8); // x 1e6.

	PR_DTMB("%s: value %d, ber %d.\n", __func__, value, *ber);

	return 0;
}

int gxtv_demod_dtmb_read_signal_strength(struct dvb_frontend *fe,
		s16 *strength)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*strength = (s16)tuner_get_ch_power(fe);
	if (tuner_find_by_name(fe, "r842") ||
		tuner_find_by_name(fe, "r836") ||
		tuner_find_by_name(fe, "r850"))
		*strength += 7;

	PR_DTMB("demod [id %d] signal strength %d dBm\n", demod->id, *strength);

	return 0;
}

int gxtv_demod_dtmb_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_DTMB("demod[%d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

int gxtv_demod_dtmb_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

int gxtv_demod_dtmb_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct aml_demod_dtmb param;
	int times;
	/*[0]: spectrum inverse(1),normal(0); [1]:if_frequency*/
	unsigned int tuner_freq[2] = {0};

	PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modul:%d, invert:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	if (!devp->demod_thread)
		return 0;

	times = 2;
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;

	demod->last_lock = -1;

	if (is_meson_t3_cpu()) {
		PR_DTMB("dtmb set ddr\n");
		dtmb_write_reg(0x7, 0x6ffffd);
		//dtmb_write_reg(0x47, 0xed33221);
		dtmb_write_reg_bits(0x47, 0x1, 22, 1);
		dtmb_write_reg_bits(0x47, 0x1, 23, 1);

		if (is_meson_t3_cpu() && is_meson_rev_b())
			t3_revb_set_ambus_state(false, false);
	}

	tuner_set_params(fe);
	if (!dtmb_new_driver)
		msleep(100);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (fe->ops.tuner_ops.get_if_frequency)
			fe->ops.tuner_ops.get_if_frequency(fe, tuner_freq);
		if (tuner_freq[0] == 0)
			demod->demod_status.spectrum = 0;
		else if (tuner_freq[0] == 1)
			demod->demod_status.spectrum = 1;
		else
			pr_err("wrong spectrum\n");
	}

	demod->demod_status.ch_bw = c->bandwidth_hz / 1000;

	dtmb_set_ch(demod, &param);

	return 0;
}

int gxtv_demod_dtmb_get_frontend(struct dvb_frontend *fe)
{
	/* these content will be written into eeprom. */

	return 0;
}

int gxtv_demod_dtmb_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	int ret = 0;
	unsigned int firstdetect = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	if (re_tune) {
		/*first*/
		demod->en_detect = 1;

		*delay = HZ / 4;
		gxtv_demod_dtmb_set_frontend(fe);
		firstdetect = dtmb_detect_first();

		if (firstdetect == 1) {
			*status = FE_TIMEDOUT;
			/*polling mode*/
			dtmb_poll_start_tune(demod, DTMBM_NO_SIGNEL_CHECK);

		} else if (firstdetect == 2) {  /*need check*/
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
			dtmb_poll_start_tune(demod, DTMBM_HV_SIGNEL_CHECK);

		} else if (firstdetect == 0) {
			gxtv_demod_dtmb_read_status_old(fe, status);
			if (*status == (0x1f))
				dtmb_poll_start_tune(demod, DTMBM_HV_SIGNEL_CHECK);
			else
				dtmb_poll_start_tune(demod, DTMBM_NO_SIGNEL_CHECK);
		}

		return ret;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] dtmb not enable\n", demod->id);
		return ret;
	}

	*delay = HZ / 4;
	gxtv_demod_dtmb_read_status_old(fe, status);

	if (*status == (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
		FE_HAS_VITERBI | FE_HAS_SYNC))
		dtmb_poll_start_tune(demod, DTMBM_HV_SIGNEL_CHECK);
	else
		dtmb_poll_start_tune(demod, DTMBM_NO_SIGNEL_CHECK);

	return ret;
}

static void dtmb_read_status(struct dvb_frontend *fe, enum fe_status *status, unsigned int re_tune,
		unsigned int *delay)
{
	int i, lock;//0:none;1:lock;-1:lost
	s16 strength;
	unsigned int fsm_state, val, cur_time;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	static int lock_status;
	static int has_signal;
	//Threshold value of times of continuous lock and lost
	int lock_continuous_cnt = dtmb_lock_continuous_cnt > 1 ? dtmb_lock_continuous_cnt : 1;
	int lost_continuous_cnt = dtmb_lost_continuous_cnt > 1 ? dtmb_lost_continuous_cnt : 1;
	int check_signal_time = dtmb_check_signal_time > 1 ? dtmb_check_signal_time :
		DTMB_TIME_CHECK_SIGNAL;
	int tuner_strength_threshold = THRD_TUNER_STRENGTH_DTMB;

	cur_time = jiffies_to_msecs(jiffies);

	if (re_tune) {
		lock_status = 0;
		has_signal = 0;
		demod->time_start = cur_time;
		*status = 0;
		demod->last_status = 0;
		dtmb_bch_check_new(fe, true);
		return;
	}

	demod->time_passed = jiffies_to_msecs(jiffies) - demod->time_start;

	gxtv_demod_dtmb_read_signal_strength(fe, &strength);
	if (strength < tuner_strength_threshold && demod->time_passed < check_signal_time) {
		*status = FE_TIMEDOUT;
		PR_DTMB("strength [%d] no signal(%d)\n",
				strength, tuner_strength_threshold);

		goto finish;
	}

	fsm_state = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	for (i = 0; i < 8; i++)
		if (((fsm_state >> (i * 4)) & 0xf) > 4)
			has_signal = 0x1;

	val = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);
	if (is_meson_gxtvbb_cpu())
		demod->real_para.snr = convert_snr(val & 0xfff) * 10;
	else
		demod->real_para.snr = convert_snr(val & 0x3fff) * 10;
	PR_DTMB("fsm=0x%x, r_snr=0x%x, snr=%d dB*10, time_passed=%d\n",
		fsm_state, val, demod->real_para.snr, demod->time_passed);
	if (val & 0x4000) {
		lock = 1;
		has_signal = 1;
		dtmb_bch_check_new(fe, false);
		*delay = 100;
	} else {
		if (demod->time_passed <= check_signal_time ||
			(demod->time_passed <= TIMEOUT_DTMB && has_signal)) {
			lock = 0;
		} else {
			lock = -1;

			if (lock_status == 0) {//not dtmb signal
				*status = FE_TIMEDOUT;
				PR_ATSC("not dtmb signal\n");

				goto finish;
			}
		}
	}

	//The status is updated only when the status continuously reaches the threshold of times
	if (lock == -1) {
		if (lock_status >= 0) {
			lock_status = -1;
			PR_ATSC("==> lost signal first\n");
		} else if (lock_status <= -lost_continuous_cnt) {
			lock_status = -lost_continuous_cnt;
			PR_ATSC("==> lost signal continue\n");
		} else {
			lock_status--;
			PR_ATSC("==> lost signal times%d\n", lock_status);
		}

		if (lock_status <= -lost_continuous_cnt)
			*status = FE_TIMEDOUT;
		else
			*status = 0;
	} else if (lock == 1) {
		if (lock_status <= 0) {
			lock_status = 1;
			PR_ATSC("==> lock signal first\n");
		} else if (lock_status >= lock_continuous_cnt) {
			lock_status = lock_continuous_cnt;
			PR_ATSC("==> lock signal continue\n");
		} else {
			lock_status++;
			PR_ATSC("==> lock signal times:%d\n", lock_status);
		}

		if (lock_status >= lock_continuous_cnt)
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL |
				FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;
		else
			*status = 0;
	} else {
		*status = 0;
	}

finish:
	if (demod->last_status != *status && *status != 0) {
		PR_INFO("!!  >> %s << !!, freq=%d, time_passed=%d\n", *status == FE_TIMEDOUT ?
			"UNLOCK" : "LOCK", fe->dtv_property_cache.frequency, demod->time_passed);
		demod->last_status = *status;
	}
}

int dtmb_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*delay = HZ / 20;

	if (re_tune) {
		PR_INFO("%s [id %d]: re_tune.\n", __func__, demod->id);
		demod->en_detect = 1;

		gxtv_demod_dtmb_set_frontend(fe);
		dtmb_read_status(fe, status, re_tune, delay);

		*status = 0;

		return 0;
	}

	dtmb_read_status(fe, status, re_tune, delay);

	return 0;
}

int Gxtv_Demod_Dtmb_Init(struct aml_dtvdemod *demod)
{
	int ret = 0;
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk;

	PR_DBG("AML Demod DTMB init\r\n");
	dig_clk = &devp->data->dig_clk;
	memset(&sys, 0, sizeof(sys));
	memset(&demod->demod_status, 0, sizeof(demod->demod_status));
	demod->demod_status.delsys = SYS_DTMB;

	if (is_meson_gxtvbb_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		sys.adc_clk = ADC_CLK_25M;
		sys.demod_clk = DEMOD_CLK_200M;
#endif
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		if (is_meson_txl_cpu()) {
			sys.adc_clk = ADC_CLK_25M;
			sys.demod_clk = DEMOD_CLK_225M;
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			sys.adc_clk = ADC_CLK_24M;
			sys.demod_clk = DEMOD_CLK_250M;
		} else {
			sys.adc_clk = ADC_CLK_24M;
			sys.demod_clk = DEMOD_CLK_225M;
		}
	} else {
		return -1;
	}

	demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.tmp = ADC_MODE;
	demod->demod_status.adc_freq = sys.adc_clk;
	demod->demod_status.clk_freq = sys.demod_clk;
	demod->last_status = 0;

	if (devp->data->hw_ver >= DTVDEMOD_HW_TL1)
		dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x501);

	ret = demod_set_sys(demod, &sys);

	return ret;
}

int amdemod_stat_dtmb_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys)
{
	unsigned int ret = 0;

	ret = dtmb_reg_r_fec_lock();
	return ret;
}
