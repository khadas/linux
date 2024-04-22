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
#include "atsc_frontend.h"
#include <linux/amlogic/aml_dtvdemod.h>

#define ATSC_TIME_CHECK_SIGNAL 600
#define ATSC_TIME_START_CCI 1500

//atsc-c

MODULE_PARM_DESC(std_lock_timeout, "");
static unsigned int std_lock_timeout = 1000;
module_param(std_lock_timeout, int, 0644);

//atsc-t
MODULE_PARM_DESC(atsc_agc_target, "");
static unsigned char atsc_agc_target;
module_param(atsc_agc_target, byte, 0644);

MODULE_PARM_DESC(atsc_t_lock_continuous_cnt, "");
static unsigned int atsc_t_lock_continuous_cnt = 1;
module_param(atsc_t_lock_continuous_cnt, int, 0644);

MODULE_PARM_DESC(atsc_check_signal_time, "");
static unsigned int atsc_check_signal_time = ATSC_TIME_CHECK_SIGNAL;
module_param(atsc_check_signal_time, int, 0644);

MODULE_PARM_DESC(atsc_t_lost_continuous_cnt, "");
static unsigned int atsc_t_lost_continuous_cnt = 15;
module_param(atsc_t_lost_continuous_cnt, int, 0644);

void gxtv_demod_atsc_release(struct dvb_frontend *fe)
{
}

int gxtv_demod_atsc_read_status(struct dvb_frontend *fe,
		enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	//struct aml_demod_sts demod_sts;
	int ilock;
	unsigned char s = 0;
	int strength = 0;

	if (!devp->demod_thread) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
		return 0;
	}

	if (c->modulation > QAM_AUTO) {/* atsc */
		/*atsc_thread();*/
		s = amdemod_stat_atsc_islock(demod, SYS_ATSC);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			atsc_check_fsm_status(demod);

			if (!s) {
				PR_ATSC("ber dalta:%d\n",
					atsc_read_reg_v4(ATSC_FEC_BER) - devp->ber_base);
			}
		} else {
			if (s == 0 && demod->last_lock == 1 && atsc_read_reg(0x0980) >= 0x76) {
				s = 1;
				PR_ATSC("unlock fsm >= 0x76\n");
			}
		}
	}

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (timer_not_enough(demod, D_TIMER_DETECT)) {
				*status = 0;
				PR_INFO("WAIT!\n");
			} else {
				*status = FE_TIMEDOUT;
				timer_disable(demod, D_TIMER_DETECT);
			}
		} else {
			*status = FE_TIMEDOUT;
		}
	}

	if (demod->last_lock != ilock) {
		PR_INFO("%s [id %d]: %s\n", __func__, demod->id,
			ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		demod->last_lock = ilock;

		if (c->modulation > QAM_AUTO && ilock) {
			devp->ber_base = atsc_read_reg_v4(ATSC_FEC_BER);
			PR_ATSC("ber base:%d\n", devp->ber_base);
		}
	}

	if (aml_demod_debug & DBG_ATSC) {
		if (demod->atsc_dbg_lst_status != s || demod->last_lock != ilock) {
			/* check tuner */
			strength = tuner_get_ch_power(fe);

			PR_ATSC("s=%d(1 is lock),lock=%d\n", s, ilock);
			PR_ATSC("freq[%d] strength[%d]\n",
					demod->freq, strength);

			/*update */
			demod->atsc_dbg_lst_status = s;
			demod->last_lock = ilock;
		}
	}

	return 0;
}

int gxtv_demod_atsc_read_signal_strength(struct dvb_frontend *fe,
		s16 *strength)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	unsigned int agc_gain = 0;

	*strength = (s16)tuner_get_ch_power(fe);
	if (tuner_find_by_name(fe, "r842") ||
		tuner_find_by_name(fe, "r836") ||
		tuner_find_by_name(fe, "r850")) {
		if (fe->dtv_property_cache.modulation <= QAM_AUTO &&
			fe->dtv_property_cache.modulation != QPSK)
			*strength += 18;
		else
			*strength += 15;

		if (*strength <= -80) {
			agc_gain = atsc_read_reg_v4(0x44) & 0xfff;
			*strength = (s16)atsc_get_power_strength(agc_gain, *strength);
		}

		*strength += 8;
	} else if (tuner_find_by_name(fe, "mxl661")) {
		*strength += 3;
	}

	PR_ATSC("[id %d] strength %d dBm\n", demod->id, *strength);

	return 0;
}

int gxtv_demod_atsc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_ATSC("[id %d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

int gxtv_demod_atsc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (c->modulation > QAM_AUTO)
		atsc_thread();

	*ucblocks = 0;

	return 0;
}

int gxtv_demod_atsc_set_frontend(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk = &devp->data->dig_clk;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_atsc param_atsc;
	union ATSC_DEMOD_REG_0X6A_BITS val_0x6a;
	union atsc_cntl_reg_0x20 val;
	/*[0]: spectrum inverse(1),normal(0); [1]:if_frequency*/
	unsigned int tuner_freq[2] = {0};
	enum fe_delivery_system delsys = demod->last_delsys;

	PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modul:%d, invert:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	memset(&param_atsc, 0, sizeof(param_atsc));
	if (!devp->demod_thread)
		return 0;

	demod->freq = c->frequency / 1000;
	demod->last_lock = -1;
	demod->atsc_mode = c->modulation;

	if (c->modulation > QAM_AUTO) {
		if (fe->ops.tuner_ops.get_if_frequency)
			fe->ops.tuner_ops.get_if_frequency(fe, tuner_freq);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			/* bit0~3: AGC bandwidth select */
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X58, 0x528220d);
			/*bit 2: invert spectrum, for r842 tuner AGC control*/
			if (tuner_freq[0] == 1)
				atsc_write_reg_v4(ATSC_DEMOD_REG_0X56, 0x4);
			else
				atsc_write_reg_v4(ATSC_DEMOD_REG_0X56, 0x0);

			if (tuner_find_by_name(fe, "r842")) {
				/* adjust IF AGC bandwidth, default 0x40208007. */
				/* for atsc agc speed test >= 85Hz. */
				atsc_write_reg_v4(ATSC_AGC_REG_0X42, 0x40208003);

				/* agc target */
				atsc_write_reg_bits_v4(ATSC_AGC_REG_0X40,
					atsc_agc_target, 0, 8);
			}
		}
	}

	tuner_set_params(fe);

	if (c->modulation > QAM_AUTO && tuner_find_by_name(fe, "r842"))
		msleep(200);

	if (c->modulation > QAM_AUTO) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			val_0x6a.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X6A);
			val_0x6a.b.peak_thd = 0x6;//Let CCFO Quality over 6
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X6A, val_0x6a.bits);
			atsc_write_reg_v4(ATSC_EQ_REG_0XA5, 0x8c);
			/* bit30: enable CCI */
			atsc_write_reg_v4(ATSC_EQ_REG_0X92, 0x40000240);
			//static echo
			atsc_write_reg_v4(ATSC_EQ_REG_0X93, 0x90f01A0);
			/* clk recover confidence control */
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X5D, 0x14400202);
			/* CW bin frequency */
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X61, 0x2ee);

			if (demod->demod_status.adc_freq == ADC_CLK_24M) {
				if (tuner_find_by_name(fe, "r842") &&
					tuner_freq[1] == DEMOD_4_57M_IF * 1000) {
					demod->demod_status.ch_if = DEMOD_4_57M_IF;
					//4.57M IF, 2^23 * IF / Fs.
					atsc_write_reg_v4(ATSC_DEMOD_REG_0X54,
							0x185F92);
				} else {
					//5M IF, 2^23 * IF / Fs.
					atsc_write_reg_v4(ATSC_DEMOD_REG_0X54,
							0x1aaaaa);
				}

				atsc_write_reg_v4(ATSC_DEMOD_REG_0X55,
					0x3ae28d);

				atsc_write_reg_v4(ATSC_DEMOD_REG_0X6E,
					0x16e3600);
			}

			/*for timeshift mosaic issue*/
			atsc_write_reg_v4(0x12, 0x18);
			val.bits = atsc_read_reg_v4(ATSC_CNTR_REG_0X20);
			val.b.cpu_rst = 1;
			atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
			usleep_range(1000, 1001);
			val.b.cpu_rst = 0;
			atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
			usleep_range(5000, 5001);
			demod->last_status = 0;
		} else {
			/*demod_set_demod_reg(0x507, TXLX_ADC_REG6);*/
			dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x507);
			demod_set_mode_ts(demod, delsys);
			param_atsc.ch_freq = c->frequency / 1000;
			param_atsc.mode = c->modulation;
			atsc_set_ch(demod, &param_atsc);

			/* bit 2: invert spectrum, 0:normal, 1:invert */
			if (tuner_freq[0] == 1)
				atsc_write_reg(0x716, atsc_read_reg(0x716) | 0x4);
			else
				atsc_write_reg(0x716, atsc_read_reg(0x716) & ~0x4);
		}
	}

	PR_DBG("atsc_mode %d\n", demod->atsc_mode);

	return 0;
}

int gxtv_demod_atsc_get_frontend(struct dvb_frontend *fe)
{
	/*these content will be written into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	PR_ATSC("frequency %d\n", c->frequency);
	return 0;
}

void atsc_polling(struct dvb_frontend *fe, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (!devp->demod_thread)
		return;

	if (c->modulation == QPSK) {
		PR_DBG("qpsk, return\n");
		/*return;*/
	} else if (c->modulation <= QAM_AUTO) {
		//atsc_j83b_polling(fe, status);
		PR_DBG("qam, return\n");
	} else {
		atsc_thread();
		gxtv_demod_atsc_read_status(fe, status);
	}
}

void atsc_optimize_cn(bool reset)
{
	unsigned int r_c3, ave_c3, r_a9, r_9e, r_d8, ave_d8;
	static unsigned int arr_c3[10] = { 0 };
	static unsigned int arr_d8[10] = { 0 };
	static unsigned int times;

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		return;

	if (reset) {
		times = 0;
		memset(arr_c3, 0, sizeof(int) * 10);
		memset(arr_d8, 0, sizeof(int) * 10);
		return;
	}

	times++;
	if (times == 10000)
		times = 20;
	r_c3 = atsc_read_reg_v4(ATSC_EQ_REG_0XC3);
	r_d8 = atsc_read_reg_v4(ATSC_EQ_REG_0XD8);
	arr_c3[times % 10] = r_c3;
	arr_d8[times % 10] = r_d8;
	if (times < 10) {
		ave_c3 = 0;
		ave_d8 = 0;
	} else {
		ave_c3 = (arr_c3[0] + arr_c3[1] + arr_c3[2] + arr_c3[3] + arr_c3[4] +
			arr_c3[5] + arr_c3[6] + arr_c3[7] + arr_c3[8] + arr_c3[9]) / 10;
		ave_d8 = (arr_d8[0] + arr_d8[1] + arr_d8[2] + arr_d8[3] + arr_d8[4] +
			arr_d8[5] + arr_d8[6] + arr_d8[7] + arr_d8[8] + arr_d8[9]) / 10;
	}

	r_a9 = atsc_read_reg_v4(ATSC_EQ_REG_0XA9);
	r_9e = atsc_read_reg_v4(ATSC_EQ_REG_0X9E);
	PR_ATSC("r_a9=0x%x,r_9e=0x%x,ave_c3=0x%x,ave_d8=0x%x\n", r_a9, r_9e, ave_c3, ave_d8);
	if ((r_a9 != 0x77744 || r_9e != 0xd0d0d09) &&
		ave_d8 < 0x1000 && ave_c3 > 0x240) {
		PR_ATSC("set cn to 15dB\n");
		atsc_write_reg_v4(ATSC_EQ_REG_0XA9, 0x77744);
		atsc_write_reg_v4(ATSC_EQ_REG_0X9E, 0xd0d0d09);
	} else if ((r_a9 != 0x44444 || r_9e != 0xa080809) &&
		(ave_d8 > 0x2000 || (ave_c3 < 0x170 && ave_c3 != 0))) {
		PR_ATSC("set cn to 15.8dB\n");
		atsc_write_reg_v4(ATSC_EQ_REG_0XA9, 0x44444);
		atsc_write_reg_v4(ATSC_EQ_REG_0X9E, 0xa080809);
	}
}

int gxtv_demod_atsc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (c->modulation > QAM_AUTO) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			*ber = atsc_read_reg_v4(ATSC_FEC_BER);
		else
			*ber = atsc_read_reg(0x980) & 0xffff;
	}

	return 0;
}

void atsc_read_status(struct dvb_frontend *fe, enum fe_status *status, unsigned int re_tune)
{
	int fsm_status;//0:none;1:lock;-1:lost
	s16 strength = 0;
	u16 rf_strength = 0;
	unsigned int sys_sts;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	static int lock_status;
	static int peak;
	//Threshold value of times of continuous lock and lost
	int lock_continuous_cnt = atsc_t_lock_continuous_cnt > 1 ? atsc_t_lock_continuous_cnt : 1;
	int lost_continuous_cnt = atsc_t_lost_continuous_cnt > 1 ? atsc_t_lost_continuous_cnt : 1;
	int check_signal_time = atsc_check_signal_time > 1 ? atsc_check_signal_time :
		ATSC_TIME_CHECK_SIGNAL;
	int tuner_strength_threshold = THRD_TUNER_STRENGTH_ATSC;

	if (re_tune) {
		lock_status = 0;
		demod->last_status = 0;
		peak = 0;
		demod->time_start = jiffies_to_msecs(jiffies);
		*status = 0;
		atsc_optimize_cn(true);
		return;
	}

	demod->time_passed = jiffies_to_msecs(jiffies) - demod->time_start;

	if (tuner_find_by_name(fe, "r842") ||
		tuner_find_by_name(fe, "r836") ||
		tuner_find_by_name(fe, "r850")) {
		tuner_strength_threshold = -89;
		check_signal_time += 100;
	}

	gxtv_demod_atsc_read_signal_strength(fe, &strength);
	if (strength < tuner_strength_threshold) {
		*status = FE_TIMEDOUT;
		PR_ATSC("strength [%d] no signal(%d)\n",
				strength, tuner_strength_threshold);

		goto finish;
	}

	atsc_check_fsm_status(demod);

	sys_sts = atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) & 0xff;
	PR_ATSC("fsm=0x%x, time_passed=%d\n", sys_sts, demod->time_passed);
	if (sys_sts >= ATSC_LOCK) {
		atsc_optimize_cn(false);
		fsm_status = 1;
		peak = 1;//atsc signal
	} else {
		if (sys_sts >= (CR_PEAK_LOCK & 0xf0))
			peak = 1;//atsc signal

		if (sys_sts >= ATSC_SYNC_LOCK ||
			demod->time_passed <= check_signal_time ||
			(demod->time_passed <= TIMEOUT_ATSC && peak)) {
			fsm_status = 0;
		} else {
			fsm_status = -1;

			//If the fsm value read within check time cannot reach 0x60 or above,
			//it means that the signal is not an ATSC signal.
			if (peak == 0) {//not atsc signal
				*status = FE_TIMEDOUT;
				PR_ATSC("not atsc signal\n");

				goto finish;
			}
		}

		if (demod->time_passed >= ATSC_TIME_START_CCI &&
			(sys_sts & 0xf0) == (CR_PEAK_LOCK & 0xf0))
			atsc_check_cci(demod);
	}

	//The status is updated only when the status continuously reaches the threshold of times
	if (fsm_status == -1) {
		if (lock_status >= 0) {
			lock_status = -1;
			PR_ATSC("lost signal first\n");
		} else if (lock_status <= -lost_continuous_cnt) {
			lock_status = -lost_continuous_cnt;
			PR_ATSC("lost signal continue\n");
		} else {
			lock_status--;
			PR_ATSC("lost signal times%d\n", lock_status);
		}

		if (lock_status <= -lost_continuous_cnt)
			*status = FE_TIMEDOUT;
		else
			*status = 0;
	} else if (fsm_status == 1) {
		if (lock_status <= 0) {
			lock_status = 1;
			PR_ATSC("lock signal first\n");
		} else if (lock_status >= lock_continuous_cnt) {
			lock_status = lock_continuous_cnt;
			PR_ATSC("lock signal continue\n");
		} else {
			lock_status++;
			PR_ATSC("lock signal times:%d\n", lock_status);
		}

		if (lock_status >= lock_continuous_cnt) {
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL |
				FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;

			/* for call r842 atsc monitor */
			if (tuner_find_by_name(fe, "r842") &&
					fe->ops.tuner_ops.get_rf_strength)
				fe->ops.tuner_ops.get_rf_strength(fe, &rf_strength);
		} else {
			*status = 0;
		}
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

int gxtv_demod_atsc_tune(struct dvb_frontend *fe, bool re_tune,
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
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	*delay = HZ / 20;

	if (re_tune) {
		demod->en_detect = 1; /*fist set*/
		gxtv_demod_atsc_set_frontend(fe);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			timer_begain(demod, D_TIMER_DETECT);

		if (c->modulation == QPSK) {
			PR_ATSC("[id %d] modulation QPSK do nothing", demod->id);
		} else if (c->modulation > QAM_AUTO) {
			PR_ATSC("[id %d] modulation 8VSB.\n", demod->id);
			atsc_read_status(fe, status, re_tune);
		} else {
			PR_ATSC("[id %d] modulation %d unsupported\n",
					demod->id, c->modulation);
		}

		return 0;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] atsc not enable\n", demod->id);
		return 0;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (c->modulation > QAM_AUTO)
			atsc_read_status(fe, status, re_tune);
	} else {
		atsc_polling(fe, status);
	}

	return 0;
}

int dtvdemod_atsc_init(struct aml_dtvdemod *demod)
{
	int ret = 0;
	struct aml_demod_sys sys;
	struct dtv_frontend_properties *c = &demod->frontend.dtv_property_cache;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk = &devp->data->dig_clk;

	memset(&sys, 0, sizeof(sys));
	memset(&demod->demod_status, 0, sizeof(demod->demod_status));
	demod->demod_status.delsys = SYS_ATSC;
	sys.adc_clk = ADC_CLK_24M;

	PR_DBG("atsc modulation: %d\n", c->modulation);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		sys.demod_clk = DEMOD_CLK_250M;
	else
		sys.demod_clk = DEMOD_CLK_225M;

	demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.tmp = ADC_MODE;
	demod->demod_status.adc_freq = sys.adc_clk;
	demod->demod_status.clk_freq = sys.demod_clk;
	demod->last_status = 0;

	if (devp->data->hw_ver == DTVDEMOD_HW_S4D) {
		demod->demod_status.adc_freq = sys.adc_clk;
		dd_hiu_reg_write(DEMOD_CLK_CTL_S4D, 0x501);
	} else {
		if (devp->data->hw_ver >= DTVDEMOD_HW_TL1)
			dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x501);
	}

	ret = demod_set_sys(demod, &sys);

	return ret;
}

int amdemod_stat_atsc_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys)
{
	int atsc_fsm;
	unsigned int ret = 0;
	unsigned int val;

	if (demod->atsc_mode == VSB_8) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			//ret = amdemod_check_8vsb_rst(demod);
			val = atsc_read_reg_v4(ATSC_CNTR_REG_0X2E);
			if (val >= ATSC_LOCK)
				ret = 1;
			else if (val >= CR_PEAK_LOCK)
				ret = atsc_check_cci(demod);
			else //if (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) <= 0x50)
				ret = 0;
		} else {
			atsc_fsm = atsc_read_reg(0x0980);
			PR_DBGL("atsc status [%x]\n", atsc_fsm);

			if (atsc_read_reg(0x0980) >= 0x79)
				ret = 1;
			else
				ret = 0;
		}
	} else {
		atsc_fsm = atsc_read_reg(0x0980);
		PR_DBGL("atsc status [%x]\n", atsc_fsm);
		if (atsc_read_reg(0x0980) >= 0x79)
			ret = 1;
		else
			ret = 0;
	}

	return ret;
}

unsigned int dtvdemod_get_atsc_lock_sts(struct aml_dtvdemod *demod)
{
	return amdemod_stat_atsc_islock(demod, SYS_ATSC);
}
