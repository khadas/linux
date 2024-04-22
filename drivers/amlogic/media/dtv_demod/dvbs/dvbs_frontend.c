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

#include "dvbs_frontend.h"
#include "dvbs.h"
#include "dvbs_diseqc.h"
#include "dvbs_singlecable.h"
#include <linux/amlogic/aml_dtvdemod.h>

static bool blind_scan_new = true;
module_param(blind_scan_new, bool, 0644);
MODULE_PARM_DESC(blind_scan_new, "");

int dtvdemod_dvbs_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	/* x e10 */
	*ber = demod->real_para.ber;

	PR_DVBS("ber %d E-10\n", *ber);

	return 0;
}

int dtvdemod_dvbs_read_signal_strength(struct dvb_frontend *fe, s16 *strength)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	unsigned int agc_level = 0;
	unsigned int i = 0, if_agc_value = 0;
	u16 rf_strength = 0;

	if (demod->last_status != 0x1F) {
		*strength = -100;
	} else if (tuner_find_by_name(fe, "av2018") ||
		tuner_find_by_name(fe, "rda5815m")) {
		agc_level = dvbs_rd_byte(DVBS_AGC_LEVEL_ADDR);
		if (agc_level > 183) {
			*strength = 0;
			if (agc_level == 184)
				*strength = -3;
			if (agc_level == 185)
				*strength = -1;
		} else if (agc_level > 170 && agc_level <= 183) {
			*strength = (s16)(agc_level * 120 / 13 - 1769) / 10;
		} else if (agc_level >= 130 && agc_level <= 170) {
			*strength = (s16)agc_level / 2 - 105;
		} else if (agc_level >= 13 && agc_level < 130) {
			*strength = (s16)(agc_level * 110 / 25 - 965) / 10;
			if (*strength < -90)
				*strength = -90;
		} else {
			*strength = -90;
			if (agc_level == 0)
				*strength = -95;
		}
	} else if (tuner_find_by_name(fe, "rt710") ||
			tuner_find_by_name(fe, "rt720")) {
		for (i = 0; i < 10; i++)
			//IF AGC1 integrator value
			if_agc_value +=
				(unsigned int)MAKEWORD(dvbs_rd_byte(0x91a), dvbs_rd_byte(0x91b));

		if_agc_value /= 10;
		rf_strength = (u16)if_agc_value;
		if (fe->ops.tuner_ops.get_rf_strength)
			fe->ops.tuner_ops.get_rf_strength(fe, &rf_strength);
		PR_DBGL("if_agc_code %d\n", if_agc_value);

		*strength = (s16)tuner_get_ch_power(fe);
	} else {
		*strength = (s16)tuner_get_ch_power(fe);
		if (*strength <= -57)
			*strength += (s16)dvbs_get_signal_strength_off();
	}

	PR_DVBS("[id %d] strength %d dBm\n", demod->id, *strength);

	return 0;
}

int dvbs_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_DVBS("[id %d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

/*
 * in Unicable mode, when try signal or try lock TP, the actual IF freq
 * should be moved to the specified user band freq first,
 * then reset fe->dtv_property_cache.frequency with the target userband freq.
 */
int dtvdemod_dvbs_unicable_change_channel(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	int ret = 0;
	struct dvb_diseqc_master_cmd cmd;
	u32 singlecable_freq_mhz = 0;

	PR_DVBS("%s: bank %d committed %d umcommitted %d IF_freq %d\n", __func__,
			devp->singlecable_param.bank, devp->singlecable_param.committed,
			devp->singlecable_param.uncommitted, c->frequency);

	if (devp->singlecable_param.version == SINGLECABLE_VERSION_1_EN50494) {
		//set singlecable
		ret = aml_singlecable_command_ODU_channel_change(&cmd,
				(unsigned char)SINGLECABLE_ADDRESS_ALL_LNB_SMATV_SWITCHER,
				devp->singlecable_param.userband, devp->singlecable_param.frequency,
				devp->singlecable_param.bank, c->frequency);
		if (ret != 0) {
			PR_ERR("singlecable ODU_channel_change fail %d\n", ret);
			return ret;
		}
	} else if (devp->singlecable_param.version == SINGLECABLE_VERSION_2_EN50607) {
		singlecable_freq_mhz = (c->frequency + 500) / 1000;
		if (singlecable_freq_mhz > 2147)
			singlecable_freq_mhz = 2147;

		ret = aml_singlecable2_command_ODU_channel_change(&cmd,
				devp->singlecable_param.userband, singlecable_freq_mhz,
				devp->singlecable_param.uncommitted,
				devp->singlecable_param.committed);
		if (ret != 0) {
			PR_ERR("singlecable2 ODU_channel_change fail %d\n", ret);
			return ret;
		}
	} else {
		PR_ERR("not support singlecable ver %d\n",
				devp->singlecable_param.version);
		return -EINVAL;
	}

	ret = aml_diseqc_send_master_cmd(fe, &cmd);
	if (ret < 0) {
		PR_ERR("send cmd fail %d\n", ret);
		return ret;
	}
	c->frequency = devp->singlecable_param.frequency;

	return ret;
}

int dtvdemod_dvbs_blind_check_signal(struct dvb_frontend *fe,
		unsigned int freq_khz, int *next_step_khz, int *signal_state)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	int ret = 0, next_step_khz1 = 0, asperity = 0;

#ifdef DVBS_BLIND_SCAN_DEBUG
	unsigned int fld_value[2] = { 0 };
	int i = 0;
	int agc1_iq_amp = 0, agc1_iq_power = 0;
#endif
	/* set tuner */
	c->frequency = freq_khz; // KHz
	if (tuner_find_by_name(fe, "av2018")) {
		c->bandwidth_hz = 40000000;
		c->symbol_rate = 40000000;
	} else {
		c->bandwidth_hz = 45000000;
		c->symbol_rate = 45000000;
	}

	//in Unicable blind scan mode, when try signal, the actual IF freq
	//should be moved to the specified user band freq first,
	//then tuner_set_params with the specified user band freq.
	if (!devp->blind_scan_stop && demod->demod_status.is_blind_scan &&
			demod->demod_status.is_singlecable) {
		ret = dtvdemod_dvbs_unicable_change_channel(fe);
		if (ret != 0)
			return ret;
	}

	tuner_set_params(fe);

	usleep_range(2 * 1000, 3 * 1000);//msleep(2);

#ifdef DVBS_BLIND_SCAN_DEBUG
	fld_value[0] = dvbs_rd_byte(0x91a);
	fld_value[1] = dvbs_rd_byte(0x91b);

	agc1_iq_amp = (int)((fld_value[0] << 8) | fld_value[1]);
	if (!agc1_iq_amp) {
		for (i = 0; i < 5; i++) {
			fld_value[0] = dvbs_rd_byte(0x916);
			fld_value[1] = dvbs_rd_byte(0x917);
			agc1_iq_power = agc1_iq_power + fld_value[0] + fld_value[1];
		}

		agc1_iq_power /= 5;
	}

	PR_DVBS("agc1_iq_amp: %d, agc1_iq_power: %d\n", agc1_iq_amp, agc1_iq_power);
#endif

	if (blind_scan_new) {
		if (*signal_state == 0 || *signal_state == 1) {
			asperity = dvbs_blind_check_AGC2_bandwidth_new(next_step_khz,
					&next_step_khz1, signal_state);
			if (*signal_state != 0)
				*next_step_khz = next_step_khz1;

			return asperity;
		} else if (*signal_state == 2) {
			return 2;
		} else {
			return 0;
		}
	} else {
		return dvbs_blind_check_AGC2_bandwidth_old(next_step_khz);
	}
}

int dtvdemod_dvbs_blind_set_frontend(struct dvb_frontend *fe,
	struct fft_in_bw_result *in_bw_result, unsigned int fft_frc_range_min,
	unsigned int fft_frc_range_max, unsigned int range_ini)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct fft_threadcontrols spectr_ana_data;
	struct fft_search_result search_result;
	unsigned int fft_reg_val[60] = {0};
	unsigned int reg_value[3];
	int frq = 0;
	int ret = 0;

	reg_value[0] = dvbs_rd_byte(0x910);//AGC1CFG
	reg_value[1] = dvbs_rd_byte(0x912);//AGC1CN
	reg_value[2] = dvbs_rd_byte(0x913);//AGC1REF

	dvbs_fft_reg_init(fft_reg_val);

	spectr_ana_data.flow = fft_frc_range_min;
	spectr_ana_data.fup = fft_frc_range_max;
	spectr_ana_data.range = range_ini;
	spectr_ana_data.mode = 0;
	spectr_ana_data.acc = 1;

	//get center frc of the frc range
	spectr_ana_data.in_bw_center_frc = (spectr_ana_data.flow +  spectr_ana_data.fup) >> 1;

	fe->dtv_property_cache.frequency = spectr_ana_data.in_bw_center_frc * 1000;
	if (tuner_find_by_name(fe, "av2018")) {
		fe->dtv_property_cache.bandwidth_hz = 40000000;
		fe->dtv_property_cache.symbol_rate = 40000000;
	} else {
		fe->dtv_property_cache.bandwidth_hz = 45000000;
		fe->dtv_property_cache.symbol_rate = 45000000;
	}

	if (!devp->blind_scan_stop && demod->demod_status.is_blind_scan &&
			demod->demod_status.is_singlecable)
		fe->dtv_property_cache.frequency = devp->singlecable_param.frequency;

	tuner_set_params(fe);
	msleep(20);

	for (frq = -22; frq <= 22;) {
		if (devp->blind_scan_stop)
			break;
		/* modify range and central frequency to adjust*/
		/*the last acquisition to the wanted range */
		search_result.found_ok = 0;
		search_result.result_bw = 0;
		search_result.result_frc = 0;

		dvbs_blind_fft_work(&spectr_ana_data, frq, &search_result);
		if (search_result.found_ok == 1) {
			in_bw_result->freq[in_bw_result->tp_num] =
				search_result.result_frc;
			in_bw_result->bw[in_bw_result->tp_num] =
				search_result.result_bw;
			in_bw_result->tp_num++;
			PR_DVBS("IN range:%d: find frc:%d,sr:%d\n",
				spectr_ana_data.range, search_result.result_frc,
				search_result.result_bw);
		}

		frq = frq + (int)spectr_ana_data.range * 2 / 5;
	}

	//fft reg reset
	dvbs_fft_reg_term(fft_reg_val);

	dvbs_wr_byte(0x910, reg_value[0]);
	dvbs_wr_byte(0x912, reg_value[1]);
	dvbs_wr_byte(0x913, reg_value[2]);

	return ret;
}

static struct fft_total_result total_result;
static struct fft_in_bw_result in_bw_result;
void dvbs_blind_scan_new_work(struct work_struct *work)
{
	struct amldtvdemod_device_s *devp = container_of(work,
			struct amldtvdemod_device_s, blind_scan_work);
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;
	struct dvb_frontend *fe = NULL;
	struct dtv_frontend_properties *c = NULL;
	enum fe_status status = FE_NONE;
	unsigned int last_locked_freq = 0, last_locked_sr = 0;
	unsigned int cur_freq = 0, cur_sr = 0, cur_iq = dvbs_get_iq_swap();
	unsigned int freq_min = devp->blind_min_fre;
	unsigned int freq_max = devp->blind_max_fre;
	unsigned int freq_step = devp->blind_fre_step;
	unsigned int freq = 0, srate = 45000000;
	unsigned int range[4] = {22, 11, 6, 3};
	unsigned int fft_frc_range_min = 0;
	unsigned int fft_frc_range_max = 0;
	unsigned int freq_one_percent = 0;
	unsigned int freq_offset = 0;
	unsigned int range_ini = 0;
	unsigned int polarity = 0;
	unsigned int iq_swap_default = dvbs_get_iq_swap();
	int i = 0, j = 0, k = 0;
	int asperity = 0, next_step_khz = 0, signal_state = 0, freq_add = 0, freq_add_dly = 0;
	int scan_time = 1;
	bool found_tp = false;
	unsigned int last_step_num = 50, cur_step_num = 0, percent_base = 0;

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			break;
		}
	}

	if (!demod) {
		PR_INFO("demod NULL\n");
		return;
	}

	fe = &demod->frontend;

	if (unlikely(!fe)) {
		PR_ERR("fe NULL\n");
		return;
	}

	timer_set_max(demod, D_TIMER_DETECT, 600);
	fe->ops.info.type = FE_QPSK;
	c = &fe->dtv_property_cache;

	if (fe->ops.tuner_ops.set_config)
		fe->ops.tuner_ops.set_config(fe, NULL);

	demod->blind_result_frequency = 0;
	demod->blind_result_symbol_rate = 0;

	c->symbol_rate = srate;
	demod->demod_status.symb_rate = srate / 1000;
	demod->demod_status.is_blind_scan = !devp->blind_scan_stop;

	if (devp->singlecable_param.version) {
		demod->demod_status.is_singlecable = 1;
		scan_time = 2;
		PR_ERR("blind scan_time %d\n", scan_time);
		//when blind scan started, reset the voltage for H/V polarization.
		if (devp->singlecable_param.version != SINGLECABLE_VERSION_UNKNOWN)
			aml_diseqc_set_voltage(fe, SEC_VOLTAGE_13);
		else
			PR_ERR("invalid unicable ver %d\n",
					devp->singlecable_param.version);
	}

	demod->last_lock = -1;
	//dvbs all reg init
	dtvdemod_dvbs_set_ch(&demod->demod_status);

	if (devp->agc_direction) {
		PR_DVBS("AGC direction: %d\n", devp->agc_direction);
		dvbs_wr_byte(0x118, 0x04);
	}

	total_result.tp_num = 0;
	if (blind_scan_new && freq_max == 2150000)
		freq_max = freq_max + freq_step;

	//map blind scan fft process to 0% - 50%
	freq_one_percent =
		scan_time == 2 ? (freq_max - freq_min) / 25 : (freq_max - freq_min) / 50;
	PR_DVBS("freq_one_percent: %d\n", freq_one_percent);

	/* 950MHz ~ 2150MHz. */
	do {
		for (freq = freq_min; freq <= freq_max;) {
			if (devp->blind_scan_stop)
				break;
			PR_INFO("Search From: [%d KHz to %d KHz]\n",
					freq - 20000, freq + freq_step - 20000);

			asperity = dtvdemod_dvbs_blind_check_signal(fe, freq,
				&next_step_khz, &signal_state);

			PR_INFO("get asperity: %d, next_step_khz %d, signal_state %d\n",
				asperity, next_step_khz, signal_state);

			fft_frc_range_min = (freq - 20000) / 1000;
			fft_frc_range_max = ((freq - 20000) / 1000) + (freq_step / 1000);
			for (i = 0; i < 4 && asperity; i++) {
				if (devp->blind_scan_stop)
					break;
				range_ini = range[i];
				in_bw_result.tp_num = 0;
				dtvdemod_dvbs_blind_set_frontend(fe, &in_bw_result,
					fft_frc_range_min, fft_frc_range_max, range_ini);

				if (in_bw_result.tp_num != 0)
					PR_INFO("In Bw(range_ini %d) Find Tp Num:%d\n",
							range_ini, in_bw_result.tp_num);

				for (j = 0; j < in_bw_result.tp_num; j++) {
					total_result.freq[total_result.tp_num] =
						in_bw_result.freq[j];
					total_result.bw[total_result.tp_num] =
						in_bw_result.bw[j];
					total_result.iq_swap[total_result.tp_num] =
						cur_iq;
					total_result.tp_num++;
				}
			}

			cur_step_num = (freq - freq_min) / freq_one_percent + percent_base;
			PR_DVBS("last %d cur_step_num %d\n",
					demod->blind_result_frequency, cur_step_num);
			if (freq <= freq_max &&
					cur_step_num > demod->blind_result_frequency &&
					cur_step_num <= (scan_time == 1 ? 50 : 25)) {
				demod->blind_result_frequency = cur_step_num;
				demod->blind_result_symbol_rate = 0;

				status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
				PR_DVBS("FFT search: blind scan process: [%d%%]\n",
						demod->blind_result_frequency);
				dvb_frontend_add_event(fe, status);
			}

			if (blind_scan_new) {
				if (asperity == 2 && signal_state == 2) {
					signal_state = 0;
					next_step_khz = freq_add;
				}

				if (signal_state == 1) {
					freq = freq + freq_step;
					freq_add_dly = next_step_khz;
				} else if (signal_state == 2) {
					freq_add = 40000 + 3000 +
						(freq_add_dly + next_step_khz) / 2;
					freq = freq - 20000 + next_step_khz / 2 - freq_add_dly / 2;
				} else {
					freq = freq + next_step_khz;
				}
			} else {
				freq = freq + (freq_step - next_step_khz);
			}

			PR_INFO("[signal_state %d], freq %d, next_step_khz %d",
					signal_state, freq, next_step_khz);
			PR_INFO("freq_add %d, freq_add_dly %d\n", freq_add, freq_add_dly);
		}
		PR_ERR("cur blind scan_time %d\n", scan_time);
		if (scan_time == 2) {
			dvbs_set_iq_swap(iq_swap_default ? 0 : 1);
			cur_iq = dvbs_get_iq_swap();
			PR_INFO("set dvbs_iq_swap %d\n", cur_iq);
			percent_base = 25;
		}
		scan_time--;
	} while (scan_time && !devp->blind_scan_stop);
	dvbs_set_iq_swap(iq_swap_default);

	PR_INFO("TOTAL FIND TP NUM: %d\n", total_result.tp_num);

	if (total_result.tp_num > 0 && !devp->blind_scan_stop) {
		// map blind scan try lock process to 51% ~ 99%
		freq_one_percent = total_result.tp_num * 50 / 50;

		dvbs_blind_fft_result_handle(&total_result);

		PR_INFO("Start Try To Lock Test\n");
		last_locked_freq = 0;
		last_locked_sr = 0;
		for (k = 0; k < total_result.tp_num; ++k) {
			if (devp->blind_scan_stop)
				break;

			cur_freq = total_result.freq[k] * 1000;
			cur_sr = total_result.bw[k] * 1000;
			cur_iq = total_result.iq_swap[k];

			PR_INFO("Try TP: [%d KHz, %d bps]\n",
					cur_freq, cur_sr);

			if ((abs(last_locked_freq - cur_freq) < 5000) ||
				(abs(last_locked_freq - cur_freq) < 10000 &&
				last_locked_sr >= 20000000) ||
				((abs(last_locked_sr - cur_sr) <= 5000) &&
				(abs(last_locked_freq - cur_freq) <= 8000))) {
				status = FE_TIMEDOUT;
				PR_INFO("Skip tune: last[%d KHz, %d bps], cur[%d KHz, %d bps]\n",
						last_locked_freq, last_locked_sr,
						cur_freq, cur_sr);
			} else {
				c->frequency = cur_freq;
				c->bandwidth_hz = cur_sr;
				c->symbol_rate = cur_sr;

				if (demod->demod_status.is_singlecable)
					dvbs_set_iq_swap(cur_iq);
				//in Unicable blind scan mode, when try lock TP, the actual IF freq
				//should be moved to the specified user band freq first
				if (!devp->blind_scan_stop && demod->demod_status.is_singlecable)
					dtvdemod_dvbs_unicable_change_channel(fe);

				dtvdemod_dvbs_set_frontend(fe);
				timer_begain(demod, D_TIMER_DETECT);

				usleep_range(500000, 510000);
				dtvdemod_dvbs_read_status(fe, &status, cur_freq, false);
			}

			if (status == FE_TIMEDOUT || status == 0) {
				found_tp = false;
			} else {
				freq_offset = dvbs_get_freq_offset(&polarity) / 1000;

				//if (demod->demod_status.is_singlecable)
				//	polarity = polarity ? 0 : 1;

				if (polarity)
					cur_freq = cur_freq + freq_offset * 1000;
				else
					cur_freq = cur_freq - freq_offset * 1000;

				// cur_sr = dvbs_get_symbol_rate() * 1000;

				if ((abs(last_locked_freq - cur_freq) < 5000) ||
					(abs(last_locked_freq - cur_freq) < 10000 &&
					last_locked_sr >= 20000000)) {
					status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
				PR_INFO("Skip report: last[%d KHz, %d bps], cur[%d KHz, %d bps]\n",
						last_locked_freq, last_locked_sr,
						cur_freq, cur_sr);
					found_tp = false;
				} else {
					found_tp = true;
				}
			}

			if (found_tp) {
				found_tp = false;
				c->symbol_rate = cur_sr;
				c->delivery_system = demod->last_delsys;
				c->bandwidth_hz = cur_sr;
				c->frequency = cur_freq;

				if (demod->demod_status.is_singlecable &&
					!devp->blind_scan_stop) {
					dtvdemod_dvbs_unicable_change_channel(fe);
					dtvdemod_dvbs_set_frontend(fe);
					timer_begain(demod, D_TIMER_DETECT);
					usleep_range(500000, 510000);
					dtvdemod_dvbs_read_status(fe, &status,
						cur_freq, false);

					if (status == FE_TIMEDOUT || status == 0) {
						status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
						PR_INFO("Skip unlock TP: [%d KHz, %d bps]\n",
							cur_freq, cur_sr);
						continue;
					}

					if (last_locked_sr == cur_sr &&
						(abs(last_locked_freq - cur_freq) <= 15000)) {
						PR_INFO("Skip repo TP: l[%d, %d], c[%d, %d]\n",
							last_locked_freq, last_locked_sr,
							cur_freq, cur_sr);
						status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
						continue;
					}
				}
				last_locked_sr = cur_sr;
				last_locked_freq = cur_freq;

				demod->blind_result_frequency = cur_freq;
				demod->blind_result_symbol_rate = cur_sr;

				status = BLINDSCAN_UPDATERESULTFREQ |
					FE_HAS_LOCK;

				PR_INFO("Get actual TP: [%d KHz, %d bps]\n",
						cur_freq, cur_sr);

				dvb_frontend_add_event(fe, status);
			}

			//map trying lock process to 51% - 99%
			cur_step_num = (k + 1) * 50 / freq_one_percent + 50;
			PR_DVBS("last_step_num %d cur_step_num %d\n",
					last_step_num, cur_step_num);
			if (total_result.tp_num > 1 && cur_step_num < 100 &&
					cur_step_num > last_step_num) {
				last_step_num = cur_step_num;
				demod->blind_result_frequency = cur_step_num;
				demod->blind_result_symbol_rate = 0;

				status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
				PR_DVBS("Try lock: blind scan process: [%d%%]\n",
						demod->blind_result_frequency);
				dvb_frontend_add_event(fe, status);
			}
		}
	}
	dvbs_set_iq_swap(iq_swap_default);
	if (!devp->blind_scan_stop) {
		c->frequency = 100;
		demod->blind_result_frequency = 100;
		demod->blind_result_symbol_rate = 0;
		status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
		PR_INFO("force 100%% to upper layer\n");
		dvb_frontend_add_event(fe, status);
		devp->blind_scan_stop = 1;
	}
}

int dtvdemod_dvbs_set_ch(struct aml_demod_sta *demod_sta)
{
	int ret = 0;

	/* 0:16, 1:32, 2:64, 3:128, 4:256 */
	demod_sta->agc_mode = 1;
	demod_sta->ch_bw = 8000;
	if (demod_sta->ch_if == 0)
		demod_sta->ch_if = DEMOD_5M_IF;

	dvbs2_reg_initial(demod_sta->symb_rate, demod_sta->is_blind_scan);

	return ret;
}

unsigned int dvbs_get_bitrate(int sr)
{
	unsigned int s2, cr_t = 1, cr_b, modu, modu_ratio = 1;

	s2 = dvbs_rd_byte(0x932) & 0x60;
	modu = dvbs_rd_byte(0x930) >> 2;
	if (s2 == 0x40) {//bit5.6 S2
		switch (modu) {
		case 0x1://0x1~0xB:QPSK
			modu_ratio = 2;
			cr_t = 1;
			cr_b = 4;
			break;

		case 0x2:
			modu_ratio = 2;
			cr_t = 1;
			cr_b = 3;
			break;

		case 0x3:
			modu_ratio = 2;
			cr_t = 2;
			cr_b = 5;
			break;

		case 0x4:
			modu_ratio = 2;
			cr_t = 1;
			cr_b = 2;
			break;

		case 0x5:
			modu_ratio = 2;
			cr_t = 3;
			cr_b = 5;
			break;

		case 0x6:
			modu_ratio = 2;
			cr_t = 2;
			cr_b = 3;
			break;

		case 0x7:
			modu_ratio = 2;
			cr_t = 3;
			cr_b = 4;
			break;

		case 0x8:
			modu_ratio = 2;
			cr_t = 4;
			cr_b = 5;
			break;

		case 0x9:
			modu_ratio = 2;
			cr_t = 5;
			cr_b = 6;
			break;

		case 0xA:
			modu_ratio = 2;
			cr_t = 8;
			cr_b = 9;
			break;

		case 0xB:
			modu_ratio = 2;
			cr_t = 9;
			cr_b = 10;
			break;

		case 0xC://0xC~0x11:8PSK
			modu_ratio = 3;
			cr_t = 3;
			cr_b = 5;
			break;

		case 0xD:
			modu_ratio = 3;
			cr_t = 2;
			cr_b = 3;
			break;

		case 0xE:
			modu_ratio = 3;
			cr_t = 3;
			cr_b = 4;
			break;

		case 0xF:
			modu_ratio = 3;
			cr_t = 5;
			cr_b = 6;
			break;

		case 0x10:
			modu_ratio = 3;
			cr_t = 8;
			cr_b = 9;
			break;

		case 0x11:
			modu_ratio = 3;
			cr_t = 9;
			cr_b = 10;
			break;

		case 0x12://0x12~0x17:APSK_16
			modu_ratio = 4;
			cr_t = 2;
			cr_b = 3;
			break;

		case 0x13:
			modu_ratio = 4;
			cr_t = 3;
			cr_b = 4;
			break;

		case 0x14:
			modu_ratio = 4;
			cr_t = 4;
			cr_b = 5;
			break;

		case 0x15:
			modu_ratio = 4;
			cr_t = 5;
			cr_b = 6;
			break;

		case 0x16:
			modu_ratio = 4;
			cr_t = 8;
			cr_b = 9;
			break;

		case 0x17:
			modu_ratio = 4;
			cr_t = 9;
			cr_b = 10;
			break;

		case 0x18://0x18~0x1C:APSK_32
			modu_ratio = 5;
			cr_t = 3;
			cr_b = 4;
			break;

		case 0x19:
			modu_ratio = 5;
			cr_t = 4;
			cr_b = 5;
			break;

		case 0x1A:
			modu_ratio = 5;
			cr_t = 5;
			cr_b = 6;
			break;

		case 0x1B:
			modu_ratio = 5;
			cr_t = 8;
			cr_b = 9;
			break;

		case 0x1C:
			modu_ratio = 5;
			cr_t = 9;
			cr_b = 10;
			break;

		default:
			return 20000000;//20M
		};
	} else {
		modu_ratio = 2;
		switch (modu) {
		case 0x20:
			cr_t = 1;
			cr_b = 2;
			break;

		case 0x21:
			cr_t = 2;
			cr_b = 3;
			break;

		case 0x22:
			cr_t = 3;
			cr_b = 4;
			break;

		case 0x23:
			cr_t = 5;
			cr_b = 6;
			break;

		case 0x24:
			cr_t = 6;
			cr_b = 7;
			break;

		case 0x25:
			cr_t = 7;
			cr_b = 8;
			break;

		default:
			return 20000000;//20M
		}
	}

	PR_DVBS("DVBS%d modu=%d, modu=PSK_%d, cr=%d/%d\n",
		s2 == 0x40 ? 2 : 1, modu, 1 << modu_ratio, cr_t, cr_b);

	return sr * cr_b / cr_t / modu_ratio;
}

static int calc_ave(int *val_arr, int cnt, unsigned int mode, int par)
{
	int i, j, k, tot, ave, *val, calc_cnt, *val_tmp, calc_cnt_tmp, need_order;

	need_order = (mode >> 31) & 1 ? 0 : 1;
	mode = mode & 0x7FFFFFFF;

	if (!val_arr || cnt <= 0 || mode < 0 || mode > 2)
		return 0;

	if (mode != 0 && par <= 0)
		return 0;

	if (mode == 1 && (par * 2) >= cnt)
		return 0;

	tot = 0;
	calc_cnt = cnt;
	if (need_order) {
		val = kmalloc_array(calc_cnt, sizeof(int), GFP_KERNEL);
		if (!val)
			return 0;

		memset(val, 0, sizeof(int) * calc_cnt);
		for (i = 0; i < calc_cnt; i++) {
			tot += val_arr[i];
			for (j = 0; j < i; j++) {
				if (val_arr[i] < val[j]) {
					for (k = i; k > j; k--)
						val[k] = val[k - 1];
					break;
				}
			}
			val[j] = val_arr[i];
		}
	} else {
		for (i = 0; i < calc_cnt; i++)
			tot += val_arr[i];
		val = val_arr;
	}

	if (mode == 1) {
		for (i = 0; i < par; i++) {
			tot -= val[i];
			tot -= val[calc_cnt - 1 - i];
		}
		calc_cnt -= par * 2;
	}

	ave = tot / calc_cnt;
	ave += (tot % calc_cnt) >= (calc_cnt / 2) ? 1 : 0;

	if (mode == 2) {
		val_tmp = val;
		calc_cnt_tmp = calc_cnt;
		for (i = 0; i < calc_cnt; i++) {
			if (val[i] < (ave - par)) {
				val_tmp = &val[i + 1];
				calc_cnt_tmp--;
			} else if (val[i] > (ave + par)) {
				calc_cnt_tmp -= calc_cnt - i;
				break;
			}
		}
		if (calc_cnt_tmp < calc_cnt)
			ave = calc_ave(val_tmp, calc_cnt_tmp, mode | 0x80000000, par);
	}

	if (need_order)
		kfree(val);

	return ave;
}

int dtvdemod_dvbs_read_status(struct dvb_frontend *fe, enum fe_status *status,
		unsigned int if_freq_khz, bool re_tune)
{
	int ilock = 0;
	unsigned char s = 0;
	s16 strength = 0;
	int offset = 0, polarity = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)demod->priv;
	char buf[32] = { 0 };
	char *polarity_name = NULL;
	char *band = NULL;
	unsigned int pkt_err_cnt, br;
	static unsigned int pkt_err_cnt_last;
	static unsigned int times, snr[8];

	if (re_tune) {
		pkt_err_cnt_last = 0;
		real_para_clear(&demod->real_para);
		*status = 0;
		demod->last_status = 0;
		times = 0;
		memset(snr, 0, sizeof(unsigned int) * 8);

		return 0;
	}

	dtvdemod_dvbs_read_signal_strength(fe, &strength);
//	if (strength < THRD_TUNER_STRENGTH_DVBS) {
//		*status = FE_TIMEDOUT;
//		demod->last_lock = -1;
//		PR_DVBS("[id %d] strength [%d] no signal(%d)\n",
//			demod->id, strength, THRD_TUNER_STRENGTH_DVBS);
//		return 0;
//	}

	br = dvbs_get_bitrate(c->symbol_rate);
	pkt_err_cnt = ((dvbs_rd_byte(0xe61) & 0x7f) << 16) +
		((dvbs_rd_byte(0xe62) & 0xff) << 8) + (dvbs_rd_byte(0xe63) & 0xff);
	if (pkt_err_cnt < pkt_err_cnt_last)
		pkt_err_cnt_last |= 0xff800000;
	//250ms:errcnt * 10000000000 / (br / 4) >> errcnt * 10 * (4000000000 / br)
	demod->real_para.ber = (pkt_err_cnt - pkt_err_cnt_last) * 10 * (400000000 / (br / 10));
	PR_DVBS("sr=%d, br=%d, ber=%d.%d E-8\n", c->symbol_rate, br,
		demod->real_para.ber / 100, demod->real_para.ber % 100);
	pkt_err_cnt_last = pkt_err_cnt;
	//snr[times % 8] = dvbs_get_quality();
	//demod->real_para.snr = (snr[0] + snr[1] + snr[2] + snr[3] +
	//	snr[4] + snr[5] + snr[6] + snr[7]) / 8;
	//PR_DVBS("==testSNR== snr=%d, ave8=%d, fun:ave8=%d, ave8-1-1=%d, ave8-1-2=%d,"
	//	" ave8-2-1=%d, ave8-2-2=%d\n", snr[times % 8], demod->real_para.snr,
	//	calc_ave(snr, 8, 0, 0), calc_ave(snr, 8, 1, 1), calc_ave(snr, 8, 1, 2),
	//	calc_ave(snr, 8, 2, 1), calc_ave(snr, 8, 2, 2));
	//times++;
	snr[times++ % 8] = dvbs_get_quality();
	//PR_DVBS("calc_ave cnt:%d ==>%d, %d, %d, %d, %d, %d, %d, %d<==\n",
	//	cnt, snr[0], snr[1], snr[2], snr[3], snr[4], snr[5], snr[6], snr[7]);
	demod->real_para.snr = calc_ave(snr, 8, 1, 1);
	PR_DVBS("calc snr %d dBx10\n", demod->real_para.snr);

	demod->time_passed = jiffies_to_msecs(jiffies) - demod->time_start;
	if (demod->time_passed >= 500) {
		if (!dvbs_rd_byte(0x160)) {
//			*status = FE_TIMEDOUT;
//			demod->last_lock = -1;
//			PR_DVBS("[id %d] not dvbs signal\n", demod->id);
//			return 0;
		}
	}

	s = amdemod_stat_dvbs_islock(demod, SYS_DVBS);
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;

		if (devp->blind_scan_stop) {
			/* limiting frequency offset is too large. */
			offset = dvbs_get_freq_offset(&polarity);
			if ((offset > 6500 && c->symbol_rate < SR_LOW_THRD) ||
				(offset > 11500 && c->symbol_rate >= SR_LOW_THRD)) {
				ilock = 0;
				*status = FE_TIMEDOUT;
				PR_INFO("[id %d]: offset %d KHz too large, sr %d\n",
						demod->id,
						offset, c->symbol_rate);
			}
		}
	} else {
		if (timer_not_enough(demod, D_TIMER_DETECT)) {
			ilock = 0;
			*status = 0;
		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
			timer_disable(demod, D_TIMER_DETECT);
		}
	}

	demod->last_status = *status;

	if (demod->last_lock != ilock) {
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "%d", c->frequency);

		if (!devp->blind_scan_stop && !demod->demod_status.is_blind_scan &&
				demod->demod_status.is_singlecable) {
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "[%d, %d]", if_freq_khz, c->frequency);

			if (devp->singlecable_param.version == SINGLECABLE_VERSION_1_EN50494) {
				band = (devp->singlecable_param.bank & 0x1) ? "H" : "L";
				polarity_name = (devp->singlecable_param.bank & 0x2) ? "H" : "V";
			} else if (devp->singlecable_param.version ==
					SINGLECABLE_VERSION_2_EN50607) {
				band = (devp->singlecable_param.committed & 0x1) ? "H" : "L";
				polarity_name =
					(devp->singlecable_param.committed & 0x2) ? "H" : "V";
			}
		}

		PR_INFO("%s [id %d]: %s.freq: %s, sr: %d %s%s\n", __func__, demod->id,
				ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!",
				buf, c->symbol_rate,
				polarity_name ? polarity_name : "",
				band ? band : "");
		demod->last_lock = ilock;
		/*add for DVBS2 QPSK 1/4 C/N worse*/
		if (ilock == 1) {
			if (((dvbs_rd_byte(0x932) & 0x60) == 0x40) &&
					((dvbs_rd_byte(0x930) >> 2) == 0x1)) {
				dvbs_wr_byte(0x991, 0x24);
				PR_INFO("DVBS2 QPSK 1/4 set 0x991 to 0x24\n");
			}
		} else {
			dvbs_wr_byte(0x991, 0x40);
		}
	}

	return 0;
}

int dtvdemod_dvbs_set_frontend(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	int ret = 0;
	unsigned int tmp_sr = 0;

	if (devp->blind_same_frec == 0)
		PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz);

	demod->demod_status.symb_rate = c->symbol_rate / 1000;
	demod->demod_status.is_blind_scan = 0; /* The actual frequency lock normal mode */
	demod->last_lock = -1;

	/* for wider frequency offset when low SR. */
	tmp_sr = c->symbol_rate;
	if (devp->blind_scan_stop && tmp_sr < SR_LOW_THRD)
		c->symbol_rate = 2 * c->symbol_rate;

	tuner_set_params(fe);

	if (devp->blind_scan_stop && tmp_sr < SR_LOW_THRD)
		c->symbol_rate = tmp_sr;

	dtvdemod_dvbs_set_ch(&demod->demod_status);
	demod->time_start = jiffies_to_msecs(jiffies);

	if (devp->agc_direction) {
		PR_DVBS("AGC direction: %d\n", devp->agc_direction);
		dvbs_wr_byte(0x118, 0x04);
	}

	return ret;
}

int dtvdemod_dvbs_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	int ret = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp =
		(struct amldtvdemod_device_s *)demod->priv;

	*delay = HZ / 8;

	if (!devp->blind_scan_stop)
		return ret;

	if (re_tune) {
		/*first*/
		demod->en_detect = 1;
		dtvdemod_dvbs_set_frontend(fe);
		timer_begain(demod, D_TIMER_DETECT);
		dtvdemod_dvbs_read_status(fe, status, fe->dtv_property_cache.frequency, re_tune);

		return ret;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] dvbs not enable\n", demod->id);
		return ret;
	}

	dtvdemod_dvbs_read_status(fe, status, fe->dtv_property_cache.frequency, re_tune);
	dvbs_check_status(NULL);

	return ret;
}

int dtvdemod_dvbs2_init(struct aml_dtvdemod *demod)
{
	int ret = 0;
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk = &devp->data->dig_clk;

	memset(&sys, 0, sizeof(sys));
	memset(&demod->demod_status, 0, sizeof(demod->demod_status));
	memset(&devp->singlecable_param, 0, sizeof(devp->singlecable_param));

	demod->demod_status.delsys = SYS_DVBS2;
	sys.adc_clk = ADC_CLK_135M;
	sys.demod_clk = DEMOD_CLK_270M;
	demod->demod_status.tmp = CRY_MODE;
	demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.adc_freq = sys.adc_clk;
	demod->demod_status.clk_freq = sys.demod_clk;
	PR_DBG("[%s]adc_clk is %d,demod_clk is %d\n", __func__, sys.adc_clk,
			sys.demod_clk);
	demod->auto_flags_trig = 0;
	demod->last_status = 0;

	if (devp->data->hw_ver == DTVDEMOD_HW_S4D) {
		PR_DBG("[%s]S4D SET DEMOD CLK\n", __func__);
		dd_hiu_reg_write(0x81, 0x702);
		dd_hiu_reg_write(0x80, 0x501);
	} else {
		dd_hiu_reg_write(dig_clk->demod_clk_ctl_1, 0x702);
		dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x501);
	}

	ret = demod_set_sys(demod, &sys);

	return ret;
}

int amdemod_stat_dvbs_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys)
{
	unsigned int ret = 0;

	ret = (dvbs_rd_byte(0x160) >> 3) & 0x1;
	return ret;
}
