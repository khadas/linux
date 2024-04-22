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
#include "j83b_frontend.h"
#include <linux/amlogic/aml_dtvdemod.h>

MODULE_PARM_DESC(auto_search_std, "");
static unsigned int auto_search_std;
module_param(auto_search_std, int, 0644);

static int dvb_j83b_count = 5;
module_param(dvb_j83b_count, int, 0644);
MODULE_PARM_DESC(dvb_atsc_count, "");
/*come from j83b_speedup_func*/

/*copy from dtvdemod_atsc_init*/
int dtvdemod_j83b_init(struct aml_dtvdemod *demod)
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

	PR_DBG("%s modulation: %d\n", __func__, c->modulation);
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

/*copy from gxtv_demod_atsc_set_frontend*/
int gxtv_demod_j83b_set_frontend(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk = &devp->data->dig_clk;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param_j83b;
	int nco_rate;
	/*[0]: spectrum inverse(1),normal(0); [1]:if_frequency*/

	PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modul:%d, invert:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	memset(&param_j83b, 0, sizeof(param_j83b));
	if (!devp->demod_thread)
		return 0;

	demod->freq = c->frequency / 1000;
	demod->last_lock = -1;
	demod->atsc_mode = c->modulation;
	demod->last_qam_mode = QAM_MODE_NUM;

	tuner_set_params(fe);

	if (c->modulation <= QAM_AUTO && c->modulation != QPSK) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			/* add for j83b 256 qam, livetv and timeshif */
			demod->demod_status.clk_freq = DEMOD_CLK_167M;
			nco_rate = (demod->demod_status.adc_freq * 256) /
					demod->demod_status.clk_freq + 2;
			PR_ATSC("demod_clk:%d, ADC_CLK:%d, nco_rate:%d\n",
				demod->demod_status.clk_freq,
				demod->demod_status.adc_freq, nco_rate);
			if (devp->data->hw_ver == DTVDEMOD_HW_S4D) {
				front_write_bits(AFIFO_ADC_S4D, nco_rate,
					AFIFO_NCO_RATE_BIT, AFIFO_NCO_RATE_WID);
			} else {
				front_write_bits(AFIFO_ADC, nco_rate,
					AFIFO_NCO_RATE_BIT, AFIFO_NCO_RATE_WID);
			}
			front_write_reg(SFIFO_OUT_LENS, 0x5);
			/* sys clk = 167M */
			if (devp->data->hw_ver == DTVDEMOD_HW_S4D)
				dd_hiu_reg_write(DEMOD_CLK_CTL_S4D, 0x502);
			else
				dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x502);
		} else {
			/* txlx j.83b set sys clk to 222M */
			dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x502);
		}

		demod_set_mode_ts(demod, SYS_DVBC_ANNEX_A);
		if (devp->data->hw_ver == DTVDEMOD_HW_S4D) {
			demod_top_write_reg(DEMOD_TOP_REG0, 0x00);
			demod_top_write_reg(DEMOD_TOP_REGC, 0xcc0011);
		}
		param_j83b.ch_freq = c->frequency / 1000;
		param_j83b.mode = amdemod_qam(c->modulation);
		if (param_j83b.mode == QAM_MODE_AUTO ||
			param_j83b.mode == QAM_MODE_256) {
			param_j83b.symb_rate = 5361;
			param_j83b.mode = QAM_MODE_256;
			demod->auto_qam_mode = QAM_MODE_256;
		} else {
			param_j83b.symb_rate = 5057;
			demod->auto_qam_mode = param_j83b.mode;
		}

		j83b_set_ch(demod, &param_j83b, fe);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			qam_write_reg(demod, 0x7, 0x10f33);
			set_j83b_filter_reg_v4(demod);
			qam_write_reg(demod, 0x12, 0x50e1000);
			qam_write_reg(demod, 0x30, 0x41f2f69);
		}

		demod_dvbc_store_qam_cfg(demod);
		demod_dvbc_set_qam(demod, param_j83b.mode, false);

		PR_ATSC("j83b mode: %d\n", param_j83b.mode);
	}

	return 0;
}

int gxtv_demod_j83b_get_frontend(struct dvb_frontend *fe)
{
	/*these content will be written into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	PR_ATSC("frequency %d\n", c->frequency);
	return 0;
}

int gxtv_demod_j83b_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	if (c->modulation == QAM_256 || c->modulation == QAM_64)
		*ber = dvbc_get_status(demod);

	return 0;
}

//WARNING: clone from gxtv_demod_dvbc_read_signal_strength
int gxtv_demod_j83b_read_signal_strength(struct dvb_frontend *fe,
		s16 *strength)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*strength = (s16)tuner_get_ch_power(fe);
	if (tuner_find_by_name(fe, "r842") ||
		tuner_find_by_name(fe, "r836") ||
		tuner_find_by_name(fe, "r850"))
		*strength += 6;
	else if (tuner_find_by_name(fe, "mxl661"))
		*strength += 3;

	PR_DVBC("[id %d] strength %d dBm\n", demod->id, *strength);

	return 0;
}

int gxtv_demod_j83b_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_ATSC("[id %d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

int gxtv_demod_j83b_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;

	return 0;
}

int gxtv_demod_j83b_read_status(struct dvb_frontend *fe,
		enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_sts demod_sts;
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

	/* j83b */
	if (c->modulation <= QAM_AUTO && c->modulation != QPSK) {
		s = amdemod_stat_j83b_islock(demod, SYS_DVBC_ANNEX_A);
		dvbc_status(demod, &demod_sts, NULL);
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
		PR_INFO("%s [id %d]: %s.\n", __func__, demod->id,
			ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		demod->last_lock = ilock;
	}

	if (aml_demod_debug & DBG_ATSC) {
		if (demod->atsc_dbg_lst_status != s || demod->last_lock != ilock) {
			/* check tuner */
			strength = tuner_get_ch_power(fe);

			PR_ATSC("s=%d(1 is lock),lock=%d\n", s, ilock);
			PR_ATSC("[rsj_test]freq[%d] strength[%d]\n",
					demod->freq, strength);

			/*update */
			demod->atsc_dbg_lst_status = s;
			demod->last_lock = ilock;
		}
	}

	return 0;
}

void gxtv_demod_j83b_release(struct dvb_frontend *fe)
{
}

void j83b_switch_qam(struct dvb_frontend *fe, enum qam_md_e qam)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	PR_ATSC("switch to %s\n", get_qam_name(qam));

	if (qam == QAM_MODE_64) {
		qam_write_bits(demod, 0xd, 5057, 0, 16);
		qam_write_bits(demod, 0x11, 5057, 8, 16);
	} else {
		qam_write_bits(demod, 0xd, 5361, 0, 16);
		qam_write_bits(demod, 0x11, 5361, 8, 16);
	}
	demod_dvbc_set_qam(demod, qam, false);
	demod_dvbc_fsm_reset(demod);
}

#define J83B_CHECK_SNR_THRESHOLD 230
int j83b_read_status(struct dvb_frontend *fe, enum fe_status *status, bool re_tune)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	s16 strength = 0;
	unsigned int s;
	unsigned int curtime, time_passed_qam;
	static int check_first;
	static unsigned int time_start_qam;
	static unsigned int timeout;
	static bool is_signal;

	if (re_tune) {
		timeout = auto_search_std == 0 ? TIMEOUT_ATSC : TIMEOUT_ATSC_STD;
		demod->last_lock = 0;
		is_signal = false;
		demod->time_start = jiffies_to_msecs(jiffies);
		time_start_qam = 0;
		if (c->modulation == QAM_AUTO)
			check_first = 1;
		else
			check_first = 0;

		*status = 0;
		demod->last_status = 0;

		return 0;
	}

	gxtv_demod_j83b_read_signal_strength(fe, &strength);
	if (strength < THRD_TUNER_STRENGTH_J83) {
		PR_ATSC("strength [%d] no signal(%d)\n",
				strength, THRD_TUNER_STRENGTH_J83);
		*status = FE_TIMEDOUT;
		demod->last_status = *status;
		real_para_clear(&demod->real_para);
		time_start_qam = 0;

		return 0;
	}

	curtime = jiffies_to_msecs(jiffies);
	demod->time_passed = curtime - demod->time_start;
	s = qam_read_reg(demod, 0x31) & 0xf;

	if (s != 5)
		real_para_clear(&demod->real_para);

	demod->real_para.snr = dvbc_get_snr(demod);
	PR_ATSC("fsm %d, snr %d dBx10, time_passed %u\n", s,
		demod->real_para.snr, demod->time_passed);

	if (s == 5) {
		is_signal = true;
		timeout = TIMEOUT_ATSC;

		*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
		demod->real_para.modulation = amdemod_qam_fe(demod->auto_qam_mode);

		PR_ATSC("locked at Qam:%s\n", get_qam_name(demod->auto_qam_mode));
	} else if (s < 3) {
		if (time_start_qam == 0)
			time_start_qam = curtime;
		time_passed_qam = curtime - time_start_qam;

		if (time_passed_qam < 125) {
			*status = 0;
		} else {
			is_signal = false;
			timeout = auto_search_std == 0 ? TIMEOUT_ATSC : TIMEOUT_ATSC_STD;

			if (demod->last_lock == 0 && check_first > 0) {
				*status = 0;
				check_first--;
			} else {
				*status = FE_TIMEDOUT;
			}

			if (c->modulation == QAM_AUTO) {
				demod->auto_qam_mode = demod->auto_qam_mode == QAM_MODE_64 ?
						QAM_MODE_256 : QAM_MODE_64;
				j83b_switch_qam(fe, demod->auto_qam_mode);
				time_start_qam = 0;
			}
		}
	} else if (s == 4 || s == 7) {
		*status = 0;
	} else {
		if (demod->real_para.snr > J83B_CHECK_SNR_THRESHOLD) {
			is_signal = true;
			timeout = TIMEOUT_ATSC;
		}

		if ((demod->last_lock == 0 || is_signal) && demod->time_passed < timeout) {
			*status = 0;
		} else if (demod->last_status == 0x1F) {
			*status = 0;
			PR_ATSC("retry fsm 0x%x, snr %d dBx10\n",
				qam_read_reg(demod, 0x31), demod->real_para.snr);
		} else {
			*status = FE_TIMEDOUT;
		}

		if (demod->time_passed > TIMEOUT_ATSC_STD &&
			(demod->time_passed % TIMEOUT_ATSC_STD) < 100) {
			PR_ATSC("has signal but need reset\n");
			demod_dvbc_fsm_reset(demod);
			msleep(50);
			time_start_qam = 0;
		}
	}

	demod->last_status = *status;

	if (*status == 0) {
		PR_ATSC("!! >> wait << !!\n");
	} else if (*status == FE_TIMEDOUT) {
		if (demod->last_lock == -1)
			PR_ATSC("!! >> lost again << !!\n");
		else
			PR_INFO("!! >> UNLOCK << !!, freq=%d, qam %d, time_passed:%u\n",
				c->frequency, demod->auto_qam_mode, demod->time_passed);
		demod->last_lock = -1;
	} else {
		if (demod->last_lock == 1)
			PR_ATSC("!! >> lock continue << !!\n");
		else
			PR_INFO("!! >> LOCK << !!, freq=%d, time_passed:%u\n",
				c->frequency, demod->time_passed);
		demod->last_lock = 1;
	}

	return 0;
}

int j83b_set_frontend_mode(struct dvb_frontend *fe, int mode)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param_j83b;
	int temp_freq = 0;

	PR_INFO("%s [id %d]: freq:%d, mode:%d\n", __func__, demod->id, c->frequency, mode);

	memset(&param_j83b, 0, sizeof(param_j83b));
	if (mode == 2 && (c->frequency == 79000000 || c->frequency == 85000000))//IRC
		temp_freq = c->frequency + 2000000;
	else if (mode == 1)//HRC
		temp_freq = c->frequency - 1250000;
	else
		return -1;

	c->frequency = temp_freq;
	tuner_set_params(fe);
	demod_set_mode_ts(demod, SYS_DVBC_ANNEX_A);
	param_j83b.ch_freq = temp_freq / 1000;
	param_j83b.mode = amdemod_qam(c->modulation);
	if (param_j83b.mode == QAM_MODE_AUTO ||
		param_j83b.mode == QAM_MODE_256) {
		param_j83b.symb_rate = 5361;
		param_j83b.mode = QAM_MODE_256;
		demod->auto_qam_mode = QAM_MODE_256;
	} else {
		param_j83b.symb_rate = 5057;
		demod->auto_qam_mode = param_j83b.mode;
	}

	j83b_set_ch(demod, &param_j83b, fe);

	demod_dvbc_store_qam_cfg(demod);
	demod_dvbc_set_qam(demod, param_j83b.mode, false);

	return 0;
}

static int j83b_polling(struct dvb_frontend *fe, enum fe_status *s)
{
	int j83b_status, i;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int strength;

	j83b_status = 0;
	strength = tuner_get_ch_power(fe);
	if (strength < THRD_TUNER_STRENGTH_J83) {
		*s = FE_TIMEDOUT;
		PR_ATSC("strength [%d] no signal(%d)\n",
				strength, THRD_TUNER_STRENGTH_J83);
		return 0;
	}

	gxtv_demod_j83b_read_status(fe, s);

	if (*s != 0x1f) {
		/*msleep(200);*/
		for (i = 0; i < dvb_j83b_count; i++) {
			msleep(25);
			gxtv_demod_j83b_read_ber(fe, &j83b_status);

			/*J.83 status >=0x38,has signal*/
			if (j83b_status >= 0x3)
				break;
		}
		PR_DBG("j.83b_status %x,modulation %d\n",
				j83b_status,
				c->modulation);

		if (j83b_status < 0x3)
			*s = FE_TIMEDOUT;
	}

	return 0;
}

int gxtv_demod_j83b_tune(struct dvb_frontend *fe, bool re_tune,
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
	static unsigned int s_j83b_mode;//2:HRC,1:IRC
	int lastlock;
	int ret;

	*delay = HZ / 20;

	if (re_tune) {
		s_j83b_mode = auto_search_std == 0 ? 0 : 2;
		demod->en_detect = 1; /*fist set*/
		gxtv_demod_j83b_set_frontend(fe);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			timer_begain(demod, D_TIMER_DETECT);

		if (c->modulation == QPSK) {
			PR_ATSC("[id %d] modulation QPSK do nothing!", demod->id);
		} else if (c->modulation <= QAM_AUTO) {
			PR_ATSC("[id %d] detect modulation j83 first\n", demod->id);
			j83b_read_status(fe, status, re_tune);
		} else {
			PR_ATSC("[id %d] modulation is %d unsupported\n",
					demod->id, c->modulation);
		}

		return 0;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] j83b not enable\n", demod->id);
		return 0;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (c->modulation <= QAM_AUTO && c->modulation !=  QPSK) {
			lastlock = demod->last_lock;
			j83b_read_status(fe, status, re_tune);
			if (s_j83b_mode > 0 && *status == FE_TIMEDOUT && lastlock == 0) {
				ret = j83b_set_frontend_mode(fe, s_j83b_mode--);
				if (ret < 0 && s_j83b_mode > 0)
					ret = j83b_set_frontend_mode(fe, s_j83b_mode--);
				if (ret == 0)
					j83b_read_status(fe, status, true);
			}
		}
	} else {
		if (c->modulation <= QAM_AUTO && c->modulation != QPSK)
			j83b_polling(fe, status);
	}

	return 0;
}

int amdemod_stat_j83b_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys)
{
	struct aml_demod_sts demod_sts;
	unsigned int ret = 0;

	demod_sts.ch_sts = dvbc_get_ch_sts(demod);
	ret = demod_sts.ch_sts & 0x1;

	return ret;
}

int j83b_set_ch(struct aml_dtvdemod *demod, struct aml_demod_dvbc *demod_dvbc,
		struct dvb_frontend *fe)
{
	int ret = 0;
	u16 symb_rate;
	u8 mode;
	u32 ch_freq;

	PR_DVBC("[id %d] f=%d, s=%d, q=%d\n", demod->id, demod_dvbc->ch_freq,
			demod_dvbc->symb_rate, demod_dvbc->mode);
	mode = demod_dvbc->mode;
	symb_rate = demod_dvbc->symb_rate;
	ch_freq = demod_dvbc->ch_freq;

	if (mode == QAM_MODE_AUTO) {
		/* auto QAM mode, force to QAM256 */
		mode = QAM_MODE_256;
		PR_DVBC("[id %d] auto QAM, set mode %d.\n", demod->id, mode);
	}

	demod->demod_status.ch_mode = mode;
	/* 0:16, 1:32, 2:64, 3:128, 4:256 */
	demod->demod_status.agc_mode = 1;
	/* 0:NULL, 1:IF, 2:RF, 3:both */
	demod->demod_status.ch_freq = ch_freq;
	demod->demod_status.ch_bw = 8000;
	if (demod->demod_status.ch_if == 0)
		demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.symb_rate = symb_rate;

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		demod->demod_status.adc_freq = demod_dvbc->dat0;

	demod_dvbc_qam_reset(demod);

	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		dvbc_reg_initial_old(demod);
	else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX) && !is_meson_txhd_cpu())
		dvbc_reg_initial(demod, fe);

	return ret;
}
