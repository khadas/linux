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
#include "dvbt_frontend.h"
#include "dvbt_func.h"
#include <linux/amlogic/aml_dtvdemod.h>

//dvb-t
MODULE_PARM_DESC(dvbt_reset_per_times, "");
static unsigned int dvbt_reset_per_times = 40;
module_param(dvbt_reset_per_times, int, 0644);

//dvb-tx
static unsigned char dvbtx_auto_check_times = 3;
MODULE_PARM_DESC(dvbtx_auto_check_times, "");
module_param(dvbtx_auto_check_times, byte, 0644);

static int dvbt_read_status(struct dvb_frontend *fe, enum fe_status *status, int *is_signal)
{
	int ilock = 0;
	unsigned char s = 0;
	s16 strength = 0;
	u16 rf_strength = 0;
	int strength_limit = THRD_TUNER_STRENGTH_DVBT;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	unsigned int tps_coderate, ts_fifo_cnt = 0, ts_cnt = 0, fec_rate = 0;
	static int no_signal_cnt, unlock_cnt, reset_time;
	unsigned int cur_time, reset_per_times = dvbt_reset_per_times;

	cur_time = jiffies_to_msecs(jiffies);
	demod->time_passed = cur_time - demod->time_start;

	if (devp->tuner_strength_limit)
		strength_limit = devp->tuner_strength_limit;

	gxtv_demod_dvbt_read_signal_strength(fe, &strength);
	if (strength < strength_limit) {
		if (!(no_signal_cnt++ % 20))
			dvbt_rst_demod(demod, fe);
		unlock_cnt = 0;
		*is_signal = 0;
		reset_time = cur_time;
		*status = FE_TIMEDOUT;
		demod->last_lock = -1;
		demod->last_status = *status;
		real_para_clear(&demod->real_para);
		PR_DVBT("strength [%d] no signal(%d)\n",
				strength, strength_limit);

		return 0;
	}
	no_signal_cnt = 0;

	if ((dvbt_t2_rdb(0x2901) & 0xf) >= 4) {
		*is_signal = 1;
	} else if (demod->time_passed <= 200 || (cur_time - reset_time) <= 200) {
		*is_signal = 0;
	} else {
		if (reset_per_times && ++unlock_cnt >= reset_per_times) {
			unlock_cnt = 0;
			dvbt_rst_demod(demod, fe);
			reset_time = cur_time;
		}
		*status = FE_TIMEDOUT;
		demod->last_lock = -1;
		demod->last_status = *status;
		real_para_clear(&demod->real_para);
		*is_signal = -1;
		PR_INFO("%s: [id %d] not dvbt signal, unlock\n",
				__func__, demod->id);

		return 0;
	}

	s = amdemod_stat_dvbt_islock(demod, SYS_DVBT);
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;

		//dBx10.
		demod->real_para.snr =
			(((dvbt_t2_rdb(CHC_CIR_SNR1) & 0x7) << 8)
			| dvbt_t2_rdb(CHC_CIR_SNR0)) * 30 / 64;
		demod->real_para.modulation = dvbt_t2_rdb(0x2912) & 0x3;
		demod->real_para.coderate = dvbt_t2_rdb(0x2913) & 0x7;
		demod->real_para.tps_cell_id =
			(dvbt_t2_rdb(0x2916) & 0xff) |
			((dvbt_t2_rdb(0x2915) & 0xff) << 8);
	} else {
		if (demod->time_passed < TIMEOUT_DVBT) {
			ilock = 0;
			*status = 0;
		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
		}
		real_para_clear(&demod->real_para);
	}

	/* porting from ST driver FE_368dvbt_LockFec() */
	if (ilock) {
		dvbt_t2_rdb(0x2913);
		if ((dvbt_t2_rdb(0x3760) >> 5) & 1)
			tps_coderate = (dvbt_t2_rdb(0x2913) >> 4) & 0x7;
		else
			tps_coderate = dvbt_t2_rdb(0x2913) & 0x7;

		if (demod->bw == BANDWIDTH_6_MHZ) {
			if ((dvbt_t2_rdb(0x2744) & 0xf) == 0x3 &&
				(dvbt_t2_rdb(0x2912) & 0x3) == 0x2 && tps_coderate != 0) {
				if (tps_coderate == 1 && dvbt_t2_rdb(0x5d0) != 0x2)
					dvbt_t2_wrb(0x5d0, 0x2);
				else if (tps_coderate != 1 && dvbt_t2_rdb(0x5d0) != 0x80)
					dvbt_t2_wrb(0x5d0, 0x80);
			} else if (dvbt_t2_rdb(0x5d0) != 0) {
				dvbt_t2_wrb(0x5d0, 0);
			}
		}

		do {
			dvbt_t2_wr_byte_bits(0x53d, 0, 6, 1);
			dvbt_t2_wr_byte_bits(0x572, 0, 0, 1);

			switch (tps_coderate) {
			case 0: /*  CR=1/2*/
				dvbt_t2_wrb(0x5d1, 0x78);
				break;
			default: /* other CR */
				dvbt_t2_wrb(0x5d1, 0x60);
				break;
			}

			if (amdemod_stat_dvbt_islock(demod, SYS_DVBT))
				ts_fifo_cnt++;
			else
				ts_fifo_cnt = 0;

			ts_cnt++;
			usleep_range(10000, 10001);
		} while (ts_fifo_cnt < 4 && ts_cnt <= 10);

		dvbt_t2_wrb(R368TER_FFT_FACTOR_2K_S2, 0x02);
		dvbt_t2_wrb(R368TER_CAS_CCDCCI, 0x7f);
		dvbt_t2_wrb(R368TER_CAS_CCDNOCCI, 0x1a);
		dvbt_t2_wrb(0x2906, (dvbt_t2_rdb(0x2906) & 0x87) | (1 << 4));

		if ((dvbt_t2_rdb(0x3760) & 0x20) == 0x20)
			fec_rate = (dvbt_t2_rdb(R368TER_TPS_RCVD3) & 0x70) >> 4;
		else
			fec_rate = dvbt_t2_rdb(R368TER_TPS_RCVD3) & 0x07;

//		if (((dvbt_t2_rdb(0x2914) & 0x0f) == 0) &&
//				((dvbt_t2_rdb(0x2912) & 0x03) == 2) &&
//					(fec_rate == 3 || fec_rate == 4))
//			dvbt_t2_wrb(R368TER_INC_CONF3, 0x0d);
//		else
//			dvbt_t2_wrb(R368TER_INC_CONF3, 0x0a);

		/* for call r842 dvbt agc slow */
		if (tuner_find_by_name(fe, "r842") && fe->ops.tuner_ops.get_rf_strength)
			fe->ops.tuner_ops.get_rf_strength(fe, &rf_strength);
	} else {
		if (((dvbt_t2_rdb(0x2901) & 0x0f) == 0x09) &&
			((dvbt_t2_rdb(0x2901) & 0x40) == 0x40)) {
			if (dvbt_t2_rdb(0x805) == 0x01)
				dvbt_t2_wrb(0x805, 0x02);
			if ((dvbt_t2_rdb(0x581) & 0x80) != 0x80) {
				if (dvbt_t2_rdb(0x805) == 2) {
					if ((dvbt_t2_rdb(0x3760) & 0x20) == 0x20)
						tps_coderate = (dvbt_t2_rdb(0x2913) >> 4) & 0x07;
					else
						tps_coderate = dvbt_t2_rdb(0x2913) & 0x07;

					dvbt_t2_wrb(0x53c, 0x6f);

					if (tps_coderate == 0)
						dvbt_t2_wrb(0x5d1, 0x78);
					else
						dvbt_t2_wrb(0x5d1, 0x60);

					dvbt_t2_wrb(0x805, 0x03);
				}
			}
		} else {
			if ((dvbt_t2_rdb(0x2901) & 0x20) == 0x20) {
				dvbt_t2_wrb(0x15d6, 0x50);
				dvbt_t2_wrb(0x805, 0x01);
			}
		}
	}

	if (ilock && ts_fifo_cnt < 4)
		*status = 0;

	if (dvbt_t2_rdb(0x2901) >> 5 & 0x1)
		/* reduce agc target to default, otherwise will influence on snr */
		dvbt_t2_wrb(0x15d6, 0x50);
	else
		/* increase agc target to make signal strong enough for locking */
		dvbt_t2_wrb(0x15d6, 0xa0);

	if (*status ==
	    (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC)) {
		dvbt_t2_wr_byte_bits(0x2906, 2, 3, 4);
		dvbt_t2_wrb(0x2815, 0x02);
	} else {
		dvbt_t2_wr_byte_bits(0x2906, 0, 3, 4);
		dvbt_t2_wrb(0x2815, 0x03);
	}

	if (reset_per_times) {
		if (*status == FE_TIMEDOUT)
			unlock_cnt++;
		else
			unlock_cnt = 0;
		if (unlock_cnt >= reset_per_times) {
			unlock_cnt = 0;
			dvbt_rst_demod(demod, fe);
			reset_time = cur_time;
		}
	}

	if (demod->last_lock != ilock) {
		if (*status == (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
		    FE_HAS_VITERBI | FE_HAS_SYNC)) {
			PR_INFO("%s [id %d]: !!  >> LOCKT << !!, freq:%d\n",
					__func__, demod->id, fe->dtv_property_cache.frequency);
			demod->last_lock = ilock;
		} else if (*status == FE_TIMEDOUT) {
			PR_INFO("%s [id %d]: !!  >> UNLOCKT << !!, freq:%d\n",
				__func__, demod->id, fe->dtv_property_cache.frequency);
			demod->last_lock = ilock;
		} else {
			PR_INFO("%s [id %d]: !!  >> WAITT << !!\n", __func__, demod->id);
		}
	}

	demod->last_status = *status;

	return 0;
}

static u64_t get_common_plp(void)
{
	int i, id, common_cnt;
	u64_t plp_common = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T3)) {
		common_cnt = dvbt_t2_rdb(0xe0);
		for (i = 0; i < common_cnt; i++) {
			id = dvbt_t2_rdb(0xa0 + i);
			if (id < 64)
				plp_common |= 1ULL << id;
		}
	}

	PR_DVBT("get common plp: 0x%llx\n", plp_common);

	return plp_common;
}

#define DVBT2_DEBUG_INFO
#define TIMEOUT_SIGNAL_T2 800
#define CONTINUE_TIMES_LOCK 3
#define CONTINUE_TIMES_UNLOCK 2
#define RESET_IN_UNLOCK_TIMES 24
//24:3Seconds
static int dvbt2_read_status(struct dvb_frontend *fe, enum fe_status *status, int *is_signal)
{
	unsigned char s = 0;
	s16 strength = 0;
	u16 rf_strength = 0;
	int strength_limit = THRD_TUNER_STRENGTH_DVBT;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	unsigned int p1_peak, val, cur_time;
	static int no_signal_cnt, unlock_cnt, reset_time;
	int snr, modu, cr, l1post, ldpc;
	unsigned int plp_num, fef_info = 0;
	u64_t plp_common;

	cur_time = jiffies_to_msecs(jiffies);
	demod->time_passed = cur_time - demod->time_start;

	if (devp->tuner_strength_limit)
		strength_limit = devp->tuner_strength_limit;

	gxtv_demod_dvbt_read_signal_strength(fe, &strength);
	if (!tuner_find_by_name(fe, "mxl661") || demod->last_status != 0x1F) {
		if (strength < strength_limit) {
			if (!(no_signal_cnt++ % 20))
				dvbt2_reset(demod, fe);
			unlock_cnt = 0;
			*is_signal = 0;
			reset_time = cur_time;
			*status = FE_TIMEDOUT;
			demod->last_status = *status;
			real_para_clear(&demod->real_para);
			PR_DVBT("strength [%d] no signal(%d)\n",
					strength, strength_limit);

			return 0;
		}
	}

	no_signal_cnt = 0;

	if (demod_is_t5d_cpu(devp)) {
		val = front_read_reg(0x3e);
		s = val & 0x01;
		p1_peak = ((val >> 1) & 0x01) == 1 ? 0 : 1;//test bus val[1];
	} else {
		val = dvbt_t2_rdb(TS_STATUS);
		s = (val >> 7) & 0x01;
		val = (dvbt_t2_rdb(0x2838) +
			(dvbt_t2_rdb(0x2839) << 8) +
			(dvbt_t2_rdb(0x283A) << 16));
		p1_peak = val <= 0xe00 ? 0 : 1;
	}

	PR_DVBT("s=%d, p1=%d, demod->p1=%d, demod->last_lock=%d, val=0x%08x\n",
		s, p1_peak, demod->p1_peak, demod->last_lock, val);

	if (demod_is_t5d_cpu(devp)) {
		cr = (val >> 2) & 0x7;
		modu = (val >> 5) & 0x3;
		fef_info = (val >> 7) & 0x1;
		ldpc = (val >> 7) & 0x3E;
		snr = val >> 13;
		l1post = (val >> 30) & 0x1;
		plp_num = (val >> 24) & 0x3f;
		plp_common = 0;
	} else {
		snr = (dvbt_t2_rdb(0x2a09) << 8) | dvbt_t2_rdb(0x2a08);
		cr = (dvbt_t2_rdb(0x8c3) >> 1) & 0x7;
		modu = (dvbt_t2_rdb(0x8c3) >> 4) & 0x7;
		ldpc = dvbt_t2_rdb(0xa50);
		l1post = (dvbt_t2_rdb(0x839) >> 3) & 0x1;
		plp_num = dvbt_t2_rdb(0x805);
		plp_common = get_common_plp();
	}
	snr &= 0x7ff;
	snr = snr * 30 / 64; //dBx10.

#ifdef DVBT2_DEBUG_INFO
	PR_DVBT("code_rate=%d, modu=%d, ldpc=%d, snr=%d dBx10, l1post=%d\n",
		cr, modu, ldpc, snr, l1post);
	if (modu < 4)
		PR_DVBT("minimum_snr_x10=%d\n", minimum_snr_x10[modu][cr]);
	else
		PR_DVBT("modu is overflow\n");
#endif

	if (p1_peak || s == 1 || l1post) {
		demod->p1_peak = cur_time ? cur_time : 1;
		*is_signal = 1;
	} else if (demod->time_passed < TIMEOUT_SIGNAL_T2 ||
		(cur_time - demod->p1_peak) < TIMEOUT_SIGNAL_T2 ||
		(cur_time - reset_time) < TIMEOUT_SIGNAL_T2) {
		*is_signal = 0;
	} else {
		*is_signal = -1;
	}

	if (s == 1) {
		if (demod->last_lock >= 0 &&
			demod->last_lock < CONTINUE_TIMES_LOCK &&
			demod->time_passed < TIMEOUT_DVBT2) {
			PR_DVBT("!!>> maybe lock %d <<!!\n", demod->last_lock);
			demod->last_lock += 1;
		} else {
			PR_DVBT("!!>> continue lock <<!!\n");
			demod->last_lock = CONTINUE_TIMES_LOCK;
		}
	} else if (demod->last_lock == 0) {
		if ((demod->p1_peak == 0 && demod->time_passed > TIMEOUT_SIGNAL_T2) ||
			demod->time_passed > TIMEOUT_DVBT2){
			*status = FE_TIMEDOUT;
			demod->last_lock = -CONTINUE_TIMES_UNLOCK;
		}
	} else if (demod->last_lock > 0) {
		if (demod->time_passed < TIMEOUT_DVBT2) {
			PR_DVBT("!!>> lost reset <<!!\n");
			demod->last_lock = 0;
		} else {
			PR_DVBT("!!>> maybe lost <<!!\n");
			demod->last_lock = -1;
		}
	} else {
		if (demod->last_lock > -CONTINUE_TIMES_UNLOCK) {
			PR_DVBT("!!>> lost +1 <<!!\n");
			demod->last_lock -= 1;
		}
	}

	if (demod->last_lock == CONTINUE_TIMES_LOCK) {
		*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
				FE_HAS_VITERBI | FE_HAS_SYNC;
		demod->real_para.snr = snr;
		demod->real_para.modulation = modu;
		demod->real_para.coderate = cr;
		demod->real_para.plp_num = plp_num;
		demod->real_para.plp_common = plp_common;
		demod->real_para.fef_info = fef_info;

		/* for call r842 dvbt agc slow */
		if (tuner_find_by_name(fe, "r842") && fe->ops.tuner_ops.get_rf_strength)
			fe->ops.tuner_ops.get_rf_strength(fe, &rf_strength);
	} else if (demod->last_lock == -CONTINUE_TIMES_UNLOCK) {
		*status = FE_TIMEDOUT;
		real_para_clear(&demod->real_para);
	} else {
		*status = 0;
	}

	if (*status == FE_TIMEDOUT)
		unlock_cnt++;
	else
		unlock_cnt = 0;
	if (unlock_cnt >= RESET_IN_UNLOCK_TIMES) {
		unlock_cnt = 0;
		dvbt2_reset(demod, fe);
		reset_time = cur_time;
	}

	if (*status == 0)
		PR_INFO("!! >> WAITT2 << !!\n");
	else if (demod->last_status != *status)
		PR_INFO("!! >> %sT2 << !!, freq:%d\n", *status == FE_TIMEDOUT ? "UNLOCK" : "LOCK",
			fe->dtv_property_cache.frequency);

	demod->last_status = *status;

	return 0;
}

int dvbt_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_DVBT("[id %d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

int dvbt2_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	*snr = demod->real_para.snr;

	PR_DVBT("[id %d] snr %d dBx10\n", demod->id, *snr);

	return 0;
}

int dvbt_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = 0;

	return 0;
}

int gxtv_demod_dvbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

int gxtv_demod_dvbt_read_signal_strength(struct dvb_frontend *fe,
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

	PR_DVBT("[id %d] strength %d dBm\n", demod->id, *strength);

	return 0;
}

int dvbt_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*struct aml_demod_sts demod_sts;*/
	struct aml_demod_dvbt param;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modul:%d, invert:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	demod->freq = c->frequency / 1000;
	param.bw = dtvdemod_convert_bandwidth(c->bandwidth_hz);
	param.agc_mode = 1;
	param.dat0 = 1;
	demod->last_lock = -1;
	demod->last_status = 0;
	real_para_clear(&demod->real_para);

	tuner_set_params(fe);

	dvbt_set_ch(demod, &param, fe);
	demod->time_start = jiffies_to_msecs(jiffies);
	/* wait tuner stable */
	msleep(30);

	return 0;
}

int dvbt2_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	PR_INFO("%s [id %d]: delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modul:%d, invert:%d\n",
			__func__, demod->id, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	if (!demod_is_t5d_cpu(devp) && !dvbt2_mplp_retune && demod->freq == c->frequency / 1000) {
		PR_INFO("same freq and mplp_retune %d not retune\n",
				dvbt2_mplp_retune);

		demod->time_start = jiffies_to_msecs(jiffies);

		return 0;
	}

	demod->bw = dtvdemod_convert_bandwidth(c->bandwidth_hz);
	demod->last_lock = 0;
	demod->last_status = 0;
	demod->p1_peak = 0;
	real_para_clear(&demod->real_para);

	if (is_meson_t5w_cpu() || is_meson_t3_cpu() ||
		demod_is_t5d_cpu(devp)) {
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt_t2_wr_byte_bits(0x09, 1, 4, 1);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
		riscv_ctl_write_reg(0x30, 4);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt_t2_wr_byte_bits(0x07, 1, 7, 1);
		dvbt_t2_wr_byte_bits(0x3613, 0, 4, 3);
		dvbt_t2_wr_byte_bits(0x3617, 0, 0, 3);

		if (is_meson_t3_cpu() && is_meson_rev_b())
			t3_revb_set_ambus_state(false, true);

		if (is_meson_t5w_cpu())
			t5w_write_ambus_reg(0x3c4e, 0x1, 23, 1);
	}

	tuner_set_params(fe);

	/* wait tuner stable */
	msleep(30);

	dvbt2_set_ch(demod, fe);

	demod->time_start = jiffies_to_msecs(jiffies);

	if (is_meson_t5w_cpu())
		t5w_write_ambus_reg(0x3c4e, 0x0, 23, 1);

	return 0;
}

int dvbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int is_signal;

	*delay = HZ / 20;

	if (re_tune) {
		PR_INFO("%s [id %d]: re_tune\n", __func__, demod->id);
		demod->en_detect = 1; /*fist set*/

		dvbt_set_frontend(fe);
		dvbt_read_status(fe, status, &is_signal);
		return 0;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] dvbt not enable\n", demod->id);
		return 0;
	}

	/*polling*/
	dvbt_read_status(fe, status, &is_signal);

	dvbt_info(demod, NULL);

	return 0;
}

int dvbt2_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int is_signal;

	*delay = HZ / 8;

	if (re_tune) {
		PR_INFO("%s [id %d]: re_tune\n", __func__, demod->id);
		demod->en_detect = 1; /*fist set*/

		dvbt2_set_frontend(fe);
		*status = 0;
		return 0;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] dvbt2 not enable\n", demod->id);
		return 0;
	}

	/*polling*/
	dvbt2_read_status(fe, status, &is_signal);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T3))
		dvbt2_info(demod, NULL);

	return 0;
}

typedef int (*dvbtx_set_frontend)(struct dvb_frontend *fe);
typedef int (*dvbtx_read_status)(struct dvb_frontend *fe, enum fe_status *status, int *is_signal);
typedef void (*dvbtx_info)(struct aml_dtvdemod *demod, struct seq_file *seq);
int dvbtx_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	static unsigned int cur_system, chk_times;
	dvbtx_set_frontend set_frontend = NULL;
	dvbtx_read_status read_status = NULL;
	dvbtx_info show_info = NULL;
	int is_signal = 0; //0:Wait; 1:Yes; -1:No
	int check_times = dvbtx_auto_check_times > 0 ? dvbtx_auto_check_times : 3;

	PR_DVBT("%s: [id %d] setSys=%d, curSys=%d, re_tune=%d\n", __func__,
		demod->id, c->delivery_system, cur_system, (int)re_tune);

	*delay = HZ / 10;

	if (re_tune) {
		demod->en_detect = 1; /*fist set*/
		chk_times = check_times;
		cur_system = c->delivery_system;
		set_frontend = cur_system == SYS_DVBT ? dvbt_set_frontend : dvbt2_set_frontend;
		set_frontend(fe);
		*status = 0;

		return 0;
	}

	if (!demod->en_detect) {
		PR_DBGL("[id %d] dvbtx not enable\n", demod->id);

		return 0;
	}

	read_status = cur_system == SYS_DVBT ? dvbt_read_status : dvbt2_read_status;
	read_status(fe, status, &is_signal);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T3)) {
		show_info = cur_system == SYS_DVBT ? dvbt_info : dvbt2_info;
		show_info(demod, NULL);
	}

	if (*status == FE_TIMEDOUT && is_signal == -1) {
		if (chk_times > 0) {
			chk_times--;
		} else {
			if (cur_system == SYS_DVBT) {
				cur_system = SYS_DVBT2;
				set_frontend = dvbt2_set_frontend;
			} else {//SYS_DVBT2
				cur_system = SYS_DVBT;
				set_frontend = dvbt_set_frontend;
			}
			c->delivery_system = cur_system;
			delsys_set(fe, cur_system);
			demod->en_detect = 1;
			msleep(50);
			set_frontend(fe);
			chk_times = check_times;
		}
	} else {
		chk_times = check_times;
	}

	return 0;
}

int gxtv_demod_dvbt_get_frontend(struct dvb_frontend *fe)
{
	return 0;
}

unsigned int dvbt_init(struct aml_dtvdemod *demod)
{
	int ret = 0;
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk;

	PR_DBG("DVB-T init\n");

	dig_clk = &devp->data->dig_clk;
	memset(&sys, 0, sizeof(sys));

	memset(&demod->demod_status, 0, sizeof(demod->demod_status));

	demod->demod_status.delsys = SYS_DVBT;
	sys.adc_clk = ADC_CLK_54M;
	sys.demod_clk = DEMOD_CLK_216M;
	demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.adc_freq = sys.adc_clk;
	demod->demod_status.clk_freq = sys.demod_clk;
	demod->last_status = 0;

	dd_hiu_reg_write(dig_clk->demod_clk_ctl_1, 0x704);
	dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x501);

	ret = demod_set_sys(demod, &sys);

	return ret;
}

unsigned int dtvdemod_dvbt2_init(struct aml_dtvdemod *demod)
{
	int ret = 0;
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct ddemod_dig_clk_addr *dig_clk = &devp->data->dig_clk;

	PR_DBG("DVB-T2 init\n");

	memset(&sys, 0, sizeof(sys));

	memset(&demod->demod_status, 0, sizeof(demod->demod_status));

	demod->demod_status.delsys = SYS_DVBT2;
	sys.adc_clk = ADC_CLK_54M;
	sys.demod_clk = DEMOD_CLK_216M;
	demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.adc_freq = sys.adc_clk;
	demod->demod_status.clk_freq = sys.demod_clk;
	demod->last_status = 0;

	dd_hiu_reg_write(dig_clk->demod_clk_ctl_1, 0x704);
	dd_hiu_reg_write(dig_clk->demod_clk_ctl, 0x501);

	ret = demod_set_sys(demod, &sys);

	return ret;
}

int amdemod_stat_dvbt_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys)
{
	unsigned int ret = 0;

	if (dvbt_t2_rdb(TS_STATUS) & 0x80)
		ret = 1;
	else
		ret = 0;

	return ret;
}
