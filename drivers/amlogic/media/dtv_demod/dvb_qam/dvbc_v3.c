// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "aml_demod.h"
#include "demod_func.h"
#include "dvbc_func.h"

MODULE_PARM_DESC(dvbc_agc_target, "");
static unsigned char dvbc_agc_target = 0xc;
module_param(dvbc_agc_target, byte, 0644);

MODULE_PARM_DESC(j83b_agc_target, "");
static unsigned char j83b_agc_target = 0xe;
module_param(j83b_agc_target, byte, 0644);

static unsigned char dvbc_qam_reg[15] = { 0 };
static unsigned int dvbc_qam_value[15] = { 0 };

u32 dvbc_get_status(struct aml_dtvdemod *demod)
{
	/*PR_DVBC("c4 is %x\n",dvbc_read_reg(QAM_BASE+0xc4));*/
	return qam_read_reg(demod, 0x31) & 0xf;
}

static u32 dvbc_get_ch_power(struct aml_dtvdemod *demod)
{
	u32 tmp;
	u32 ad_power;
	u32 agc_gain;
	u32 ch_power;

	tmp = qam_read_reg(demod, 0x27);

	ad_power = (tmp >> 22) & 0x1ff;
	agc_gain = (tmp >> 0) & 0x7ff;

	ad_power = ad_power >> 4;
	/* ch_power = lookuptable(agc_gain) + ad_power; TODO */
	ch_power = (ad_power & 0xffff) + ((agc_gain & 0xffff) << 16);

	return ch_power;
}

u32 dvbc_get_snr(struct aml_dtvdemod *demod)
{
	u32 tmp = 0, snr = 0;

	tmp = qam_read_reg(demod, 0x5) & 0xfff;
	snr = tmp * 10 / 32; /* dBx10 */

	return snr;
}

static u32 dvbc_get_ber(struct aml_dtvdemod *demod)
{
	u32 rs_ber;
	u32 rs_packet_len;

	rs_packet_len = qam_read_reg(demod, 0x4) & 0xffff;
	rs_ber = qam_read_reg(demod, 0x5) >> 12 & 0xfffff;

	/* rs_ber = rs_ber / 204.0 / 8.0 / rs_packet_len; */
	if (rs_packet_len == 0)
		rs_ber = 1000000;
	else
		rs_ber = rs_ber * 613 / rs_packet_len;	/* 1e-6 */

	return rs_ber;
}

u32 dvbc_get_per(struct aml_dtvdemod *demod)
{
	u32 rs_per;
	u32 rs_packet_len;
	u32 acc_rs_per_times;

	rs_packet_len = qam_read_reg(demod, 0x4) & 0xffff;
	rs_per = qam_read_reg(demod, 0x6) >> 16 & 0xffff;

	acc_rs_per_times = qam_read_reg(demod, 0x33) & 0xffff;
	/*rs_per = rs_per / rs_packet_len; */

	if (rs_packet_len == 0)
		rs_per = 10000;
	else
		rs_per = 10000 * rs_per / rs_packet_len;	/* 1e-4 */

	/*return rs_per; */
	return acc_rs_per_times;
}

u32 dvbc_get_symb_rate(struct aml_dtvdemod *demod)
{
	u32 tmp;
	u32 adc_freq;
	u32 symb_rate;

	adc_freq = qam_read_reg(demod, 0xd) >> 16 & 0xffff;
	tmp = qam_read_reg(demod, 0x2e);

	if ((tmp >> 15) == 0)
		symb_rate = 0;
	else
		symb_rate = 10 * (adc_freq << 12) / (tmp >> 15);

	return symb_rate / 10;
}

static int dvbc_get_freq_off(struct aml_dtvdemod *demod)
{
	int tmp;
	int symb_rate;
	int freq_off;

	symb_rate = dvbc_get_symb_rate(demod);
	tmp = qam_read_reg(demod, 0x38) & 0x3fffffff;
	if (tmp >> 29 & 1)
		tmp -= (1 << 30);

	freq_off = ((tmp >> 16) * 25 * (symb_rate >> 10)) >> 3;

	return freq_off;
}

void qam_auto_scan(struct aml_dtvdemod *demod, int auto_qam_enable)
{
	if (auto_qam_enable) {
		if (demod->auto_sr)
			dvbc_cfg_sr_scan_speed(demod, SYM_SPEED_MIDDLE);
		else
			dvbc_cfg_sr_scan_speed(demod, SYM_SPEED_NORMAL);

		/* j83b */
		if (demod->atsc_mode == QAM_64 || demod->atsc_mode == QAM_256 ||
			demod->atsc_mode == QAM_AUTO ||
			!cpu_after_eq(MESON_CPU_MAJOR_ID_T5D)) {
			dvbc_cfg_tim_sweep_range(demod, SYM_SPEED_NORMAL);
		} else {
			if (demod->auto_sr)
				dvbc_cfg_tim_sweep_range(demod, SYM_SPEED_MIDDLE);
			else
				dvbc_cfg_tim_sweep_range(demod, SYM_SPEED_NORMAL);
		}

		qam_write_reg(demod, 0x4e, 0x12010012);
	} else {
		qam_write_reg(demod, 0x4e, 0x12000012);
	}
}

static unsigned int get_adc_freq(void)
{
	return 24000;
}

void demod_dvbc_qam_reset(struct aml_dtvdemod *demod)
{
	/* reset qam register */
	qam_write_reg(demod, 0x1, qam_read_reg(demod, 0x1) | (1 << 0));
	qam_write_reg(demod, 0x1, qam_read_reg(demod, 0x1) & ~(1 << 0));

	PR_DVBC("dvbc reset qam\n");
}

void demod_dvbc_fsm_reset(struct aml_dtvdemod *demod)
{
	/* reset fsm */
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) & ~(1 << 4));
	qam_write_reg(demod, 0x3a, 0x0);
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) | (1 << 4));
	qam_write_reg(demod, 0x3a, 0x4);

	PR_DVBC("dvbc reset fsm\n");
}

void demod_dvbc_store_qam_cfg(struct aml_dtvdemod *demod)
{
	dvbc_qam_reg[0] = 0x71;
	dvbc_qam_reg[1] = 0x72;
	dvbc_qam_reg[2] = 0x73;
	dvbc_qam_reg[3] = 0x75;
	dvbc_qam_reg[4] = 0x76;
	dvbc_qam_reg[5] = 0x7a;
	dvbc_qam_reg[6] = 0x93;
	dvbc_qam_reg[7] = 0x94;
	dvbc_qam_reg[8] = 0x77;
	dvbc_qam_reg[9] = 0x7c;
	dvbc_qam_reg[10] = 0x7d;
	dvbc_qam_reg[11] = 0x7e;
	dvbc_qam_reg[12] = 0x9c;
	dvbc_qam_reg[13] = 0x78;
	dvbc_qam_reg[14] = 0x57;

	dvbc_qam_value[0] = qam_read_reg(demod, 0x71);
	dvbc_qam_value[1] = qam_read_reg(demod, 0x72);
	dvbc_qam_value[2] = qam_read_reg(demod, 0x73);
	dvbc_qam_value[3] = qam_read_reg(demod, 0x75);
	dvbc_qam_value[4] = qam_read_reg(demod, 0x76);
	dvbc_qam_value[5] = qam_read_reg(demod, 0x7a);
	dvbc_qam_value[6] = qam_read_reg(demod, 0x93);
	dvbc_qam_value[7] = qam_read_reg(demod, 0x94);
	dvbc_qam_value[8] = qam_read_reg(demod, 0x77);
	dvbc_qam_value[9] = qam_read_reg(demod, 0x7c);
	dvbc_qam_value[10] = qam_read_reg(demod, 0x7d);
	dvbc_qam_value[11] = qam_read_reg(demod, 0x7e);
	dvbc_qam_value[12] = qam_read_reg(demod, 0x9c);
	dvbc_qam_value[13] = qam_read_reg(demod, 0x78);
	dvbc_qam_value[14] = qam_read_reg(demod, 0x57);
}

void demod_dvbc_restore_qam_cfg(struct aml_dtvdemod *demod)
{
	qam_write_reg(demod, dvbc_qam_reg[0], dvbc_qam_value[0]);
	qam_write_reg(demod, dvbc_qam_reg[1], dvbc_qam_value[1]);
	qam_write_reg(demod, dvbc_qam_reg[2], dvbc_qam_value[2]);
	qam_write_reg(demod, dvbc_qam_reg[3], dvbc_qam_value[3]);
	qam_write_reg(demod, dvbc_qam_reg[4], dvbc_qam_value[4]);
	qam_write_reg(demod, dvbc_qam_reg[5], dvbc_qam_value[5]);
	qam_write_reg(demod, dvbc_qam_reg[6], dvbc_qam_value[6]);
	qam_write_reg(demod, dvbc_qam_reg[7], dvbc_qam_value[7]);
	qam_write_reg(demod, dvbc_qam_reg[8], dvbc_qam_value[8]);
	qam_write_reg(demod, dvbc_qam_reg[9], dvbc_qam_value[9]);
	qam_write_reg(demod, dvbc_qam_reg[10], dvbc_qam_value[10]);
	qam_write_reg(demod, dvbc_qam_reg[11], dvbc_qam_value[11]);
	qam_write_reg(demod, dvbc_qam_reg[12], dvbc_qam_value[12]);
	qam_write_reg(demod, dvbc_qam_reg[13], dvbc_qam_value[13]);
	qam_write_reg(demod, dvbc_qam_reg[14], dvbc_qam_value[14]);
}

void demod_dvbc_set_qam(struct aml_dtvdemod *demod, enum qam_md_e qam, bool auto_sr)
{
	if (demod->last_qam_mode != qam) {
		PR_DVBC("last qam %d, switch to %d\n",
				demod->last_qam_mode, qam);
	} else {
		PR_DVBC("last qam == qam %d\n", qam);

		return;
	}

	demod_dvbc_restore_qam_cfg(demod);

	/* 0x2 bit[0-3]: qam_mode_cfg. */
	if (auto_sr)
		qam_write_reg(demod, QAM_MODE_CFG,
			(qam_read_reg(demod, QAM_MODE_CFG) & ~7)
				| (qam & 7) | (1 << 17));
	else
		qam_write_reg(demod, QAM_MODE_CFG,
			(qam_read_reg(demod, QAM_MODE_CFG) & ~7) | (qam & 7));

	demod->last_qam_mode = qam;

	switch (qam) {
	case QAM_MODE_16:
		qam_write_reg(demod, 0x71, 0x000a2200);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			qam_write_reg(demod, 0x72, 0xc2b0c49);
		else
			qam_write_reg(demod, 0x72, 0x0c2b04a9);

		qam_write_reg(demod, 0x73, 0x02020000);
		qam_write_reg(demod, 0x75, 0x000e9178);
		qam_write_reg(demod, 0x76, 0x0001c100);
		qam_write_reg(demod, 0x7a, 0x002ab7ff);
		qam_write_reg(demod, 0x93, 0x641a180c);
		qam_write_reg(demod, 0x94, 0x0c141400);
		break;

	case QAM_MODE_32:
		qam_write_reg(demod, 0x71, 0x00061200);
		qam_write_reg(demod, 0x72, 0x099301ae);
		qam_write_reg(demod, 0x73, 0x08080000);
		qam_write_reg(demod, 0x75, 0x000bf10c);
		qam_write_reg(demod, 0x76, 0x0000a05c);
		qam_write_reg(demod, 0x77, 0x001000d6);
		qam_write_reg(demod, 0x7a, 0x0019a7ff);
		qam_write_reg(demod, 0x7c, 0x00111222);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			qam_write_reg(demod, 0x7d, 0x2020305);
		else
			qam_write_reg(demod, 0x7d, 0x05050505);

		qam_write_reg(demod, 0x7e, 0x03000d0d);
		qam_write_reg(demod, 0x93, 0x641f1d0c);
		qam_write_reg(demod, 0x94, 0x0c1a1a00);
		break;

	case QAM_MODE_64:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			qam_write_reg(demod, 0x9c, 0x2a132100);
			qam_write_reg(demod, 0x57, 0x606060d);
		}
		break;

	case QAM_MODE_128:
		qam_write_reg(demod, 0x71, 0x0002c200);
		qam_write_reg(demod, 0x72, 0x0a6e0059);
		qam_write_reg(demod, 0x73, 0x08080000);
		qam_write_reg(demod, 0x75, 0x000a70e9);
		qam_write_reg(demod, 0x76, 0x00002013);
		qam_write_reg(demod, 0x77, 0x00035068);
		qam_write_reg(demod, 0x78, 0x000ab100);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			qam_write_reg(demod, 0x7a, 0xba7ff);
		else
			qam_write_reg(demod, 0x7a, 0x002ba7ff);

		qam_write_reg(demod, 0x7c, 0x00111222);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			qam_write_reg(demod, 0x7d, 0x2020305);
		else
			qam_write_reg(demod, 0x7d, 0x05050505);

		qam_write_reg(demod, 0x7e, 0x03000d0d);
		qam_write_reg(demod, 0x93, 0x642a240c);
		qam_write_reg(demod, 0x94, 0x0c262600);
		break;

	case QAM_MODE_256:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			qam_write_reg(demod, 0x9c, 0x2a232100);
			qam_write_reg(demod, 0x57, 0x606040d);
		}
		break;

	default:
		break;
	}
}

void dvbc_reg_initial(struct aml_dtvdemod *demod, struct dvb_frontend *fe)
{
	u32 clk_freq;
	u32 adc_freq;
	/*ary no use u8 tuner;*/
	u8 ch_mode;
	u8 agc_mode;
	u32 ch_freq;
	u16 ch_if;
	u16 ch_bw;
	u16 symb_rate;
	u32 phs_cfg;
	int afifo_ctr;
	int max_frq_off, tmp, adc_format;

	clk_freq = demod->demod_status.clk_freq;	/* kHz */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		adc_freq = demod->demod_status.adc_freq;
	else
		adc_freq  = get_adc_freq();/*24000*/;
	adc_format = 1;
	ch_mode = demod->demod_status.ch_mode;
	agc_mode = demod->demod_status.agc_mode;
	ch_freq = demod->demod_status.ch_freq;	/* kHz */
	ch_if = demod->demod_status.ch_if;	/* kHz */
	ch_bw = demod->demod_status.ch_bw;	/* kHz */
	symb_rate = demod->demod_status.symb_rate;	/* k/sec */
	PR_DVBC("ch_if %d, %d, %d, %d, %d %d\n",
		ch_if, ch_mode, ch_freq, ch_bw, symb_rate, adc_freq);
	/* disable irq */
	qam_write_reg(demod, 0x34, 0);

	/* reset */
	/*dvbc_reset(); */
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) & ~(1 << 4));
	/* disable fsm_en */
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) & ~(1 << 0));
	/* Sw disable demod */
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) | (1 << 0));

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (agc_mode == 1) {
			qam_write_reg(demod, 0x25,
				qam_read_reg(demod, 0x25) & ~(0x1 << 10));
			qam_write_reg(demod, 0x24,
				qam_read_reg(demod, 0x24) | (0x1 << 17));
		}

		if (tuner_find_by_name(fe, "r842") ||
			tuner_find_by_name(fe, "r836") ||
			tuner_find_by_name(fe, "r850")) {
			if (demod->atsc_mode == QAM_64 || demod->atsc_mode == QAM_256 ||
				demod->atsc_mode == QAM_AUTO)
				qam_write_reg(demod, 0x25,
					(qam_read_reg(demod, 0x25) & 0xFFFFFFF0) | j83b_agc_target);
			else
				qam_write_reg(demod, 0x25,
					(qam_read_reg(demod, 0x25) & 0xFFFFFFF0) | dvbc_agc_target);
		}
	}

	/* Sw enable demod */
	qam_write_reg(demod, 0x0, 0x0);
	/* QAM_STATUS */
	qam_write_reg(demod, 0x7, 0x00000f00);
	//demod_dvbc_set_qam(demod, ch_mode, demod->auto_sr);
	/*dvbc_write_reg(QAM_BASE+0x00c, 0xfffffffe);*/
	/* // adc_cnt, symb_cnt*/

	if (demod->auto_sr)
		dvbc_cfg_sr_cnt(demod, SYM_SPEED_MIDDLE);
	else
		dvbc_cfg_sr_cnt(demod, SYM_SPEED_NORMAL);

	/* adc_cnt, symb_cnt    by raymond 20121213 */
	if (clk_freq == 0)
		afifo_ctr = 0;
	else
		afifo_ctr = (adc_freq * 256 / clk_freq) + 2;
	if (afifo_ctr > 255)
		afifo_ctr = 255;
	qam_write_reg(demod, 0x4, (afifo_ctr << 16) | 8000);
	/* afifo, rs_cnt_cfg */

	/*dvbc_write_reg(QAM_BASE+0x020, 0x21353e54);*/
	 /* // PHS_reset & TIM_CTRO_ACCURATE  sw_tim_select=0*/
	/*dvbc_write_reg(QAM_BASE+0x020, 0x21b53e54);*/
	 /* //modified by qiancheng*/
	qam_write_reg(demod, SR_OFFSET_ACC, 0x61b53e54);
	/*modified by qiancheng by raymond 20121208  0x63b53e54 for cci */
	/*  dvbc_write_reg(QAM_BASE+0x020, 0x6192bfe2);*/
	/* //modified by ligg 20130613 auto symb_rate scan*/
	if (adc_freq == 0)
		phs_cfg = 0;
	else
		phs_cfg = (1 << 31) / adc_freq * ch_if / (1 << 8);
	/*  8*fo/fs*2^20 fo=36.125, fs = 28.57114, = 21d775 */
	/* PR_DVBC("phs_cfg = %x\n", phs_cfg); */
	qam_write_reg(demod, 0x9, 0x4c000000 | (phs_cfg & 0x7fffff));
	/* PHS_OFFSET, IF offset, */

	if (adc_freq == 0) {
		max_frq_off = 0;
	} else {
		max_frq_off = (1 << 29) / symb_rate;
		/* max_frq_off = (400KHz * 2^29) /  */
		/*   (AD=28571 * symbol_rate=6875) */
		tmp = 40000000 / adc_freq;
		max_frq_off = tmp * max_frq_off;
	}
	PR_DVBC("max_frq_off %x,\n", max_frq_off);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		/* j83b */
		if (demod->atsc_mode == QAM_64 || demod->atsc_mode == QAM_256 ||
			demod->atsc_mode == QAM_AUTO ||
			!cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
			qam_write_reg(demod, SR_SCAN_SPEED, 0x245cf450);
		else
			qam_write_reg(demod, SR_SCAN_SPEED, 0x235cf459);
	} else {
		qam_write_reg(demod, 0xb, max_frq_off & 0x3fffffff);
	}
	/* max frequency offset, by raymond 20121208 */

	/* modified by ligg 20130613 --auto symb_rate scan */
	qam_write_reg(demod, 0xd, ((adc_freq & 0xffff) << 16) | (symb_rate & 0xffff));

	/************* hw state machine config **********/
	/*dvbc_write_reg(QAM_BASE + (0x10 << 2), 0x003c);*/
	/* configure symbol rate step 0*/

	/* modified 0x44 0x48 */
	qam_write_reg(demod, 0x11, (symb_rate & 0xffff) * 256);
	/* support CI+ card */
	//ts_clk and ts_data direction 0x98 0x90
	if (demod->ci_mode == 1)
		qam_write_bits(demod, 0x11, 0x00, 24, 8);
	else
		qam_write_bits(demod, 0x11, 0x90, 24, 8);
	/* blind search, configure max symbol_rate      for 7218  fb=3.6M */
	/*dvbc_write_reg(QAM_BASE+0x048, 3600*256);*/
	/* // configure min symbol_rate fb = 6.95M*/
	if (demod->auto_sr)
		qam_write_reg(demod, 0x12,
			(qam_read_reg(demod, 0x12) & ~(0xffff << 8)) | 3400 * 256);
	else
		qam_write_reg(demod, 0x12,
			(qam_read_reg(demod, 0x12) & ~(0xffff << 8)) | symb_rate * 256);

	/************* hw state machine config **********/
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if ((agc_mode & 1) == 0)
			/* freeze if agc */
			qam_write_reg(demod, 0x25,
			qam_read_reg(demod, 0x25) | (0x1 << 10));
		if ((agc_mode & 2) == 0) {
			/* IF control */
			/*freeze rf agc */
			qam_write_reg(demod, 0x25,
			qam_read_reg(demod, 0x25) | (0x1 << 13));
		}
	}

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		qam_write_reg(demod, 0x28,
		qam_read_reg(demod, 0x28) | (adc_format << 27));

	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) | 0x33);
	/* IMQ, QAM Enable */

	/* start hardware machine */
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) | (1 << 4));
	qam_write_reg(demod, 0x3a, qam_read_reg(demod, 0x3a) | (1 << 2));

	/* clear irq status */
	qam_read_reg(demod, 0x35);

	/* enable irq */
	qam_write_reg(demod, 0x34, 0x7fff << 3);

	if (is_meson_txlx_cpu() || cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		/*my_tool setting j83b mode*/
		qam_write_reg(demod, 0x7, 0x10f33);

		switch (demod->demod_status.delsys) {
		case SYS_ATSC:
		case SYS_ATSCMH:
		case SYS_DVBC_ANNEX_B:
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_meson_txlx_cpu()) {
				/*j83b filter para*/
				qam_write_reg(demod, 0x40, 0x3f010201);
				qam_write_reg(demod, 0x41, 0x0a003a3b);
				qam_write_reg(demod, 0x42, 0xe1ee030e);
				qam_write_reg(demod, 0x43, 0x002601f2);
				qam_write_reg(demod, 0x44, 0x009b006b);
				qam_write_reg(demod, 0x45, 0xb3a1905);
				qam_write_reg(demod, 0x46, 0x1c396e07);
				qam_write_reg(demod, 0x47, 0x3801cc08);
				qam_write_reg(demod, 0x48, 0x10800a2);
				qam_write_reg(demod, 0x12, 0x50e1000);
				qam_write_reg(demod, 0x30, 0x41f2f69);
				/*j83b_symbolrate(please see register doc)*/
				qam_write_reg(demod, 0x4d, 0x23d125f7);
				/*for phase noise case 256qam*/
				qam_write_reg(demod, 0x9c, 0x2a232100);
				qam_write_reg(demod, 0x57, 0x606040d);
				/*for phase noise case 64qam*/
				qam_write_reg(demod, 0x54, 0x606050d);
				qam_write_reg(demod, 0x52, 0x346dc);
			}
#endif
			break;
		default:
			break;
		}

		qam_auto_scan(demod, 1);
	}

	qam_write_reg(demod, 0x65, 0x700c); // offset
	qam_write_reg(demod, 0xb4, 0x32030);
	qam_write_reg(demod, 0xb7, 0x3084);

	// agc gain
	qam_write_reg(demod, 0x24, (qam_read_reg(demod, 0x24) | (1 << 17)));
	qam_write_reg(demod, 0x60, 0x10466000);
	qam_write_reg(demod, 0xac, (qam_read_reg(demod, 0xac) & (~0xff00))
		| 0x800);
	qam_write_reg(demod, 0xae, (qam_read_reg(demod, 0xae)
		& (~0xff000000)) | 0x8000000);

	qam_write_reg(demod, 0x7, 0x10f23);
	qam_write_reg(demod, 0x3a, 0x0);
	qam_write_reg(demod, 0x7, 0x10f33);
	/*enable fsm, sm start work, need wait some time(2ms) for AGC stable*/
	qam_write_reg(demod, 0x3a, 0x4);
	/*auto track*/
	/*dvbc_set_auto_symtrack(demod); */
}

#ifdef AML_DEMOD_SUPPORT_DVBC
u32 dvbc_set_auto_symtrack(struct aml_dtvdemod *demod)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		return 0;

	qam_write_reg(demod, SR_SCAN_SPEED, 0x245bf45c);	/*open track */
	qam_write_reg(demod, SR_OFFSET_ACC, 0x61b2bf5c);
	qam_write_reg(demod, 0x11, (7000 & 0xffff) * 256);
	qam_write_reg(demod, TIM_SWEEP_RANGE_CFG, 0x00220000);
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) & ~(1 << 0));
	/* Sw disable demod */
	qam_write_reg(demod, 0x7, qam_read_reg(demod, 0x7) | (1 << 0));
	/* Sw enable demod */
	return 0;
}
#endif

int dvbc_status(struct aml_dtvdemod *demod, struct aml_demod_sts *demod_sts, struct seq_file *seq)
{
	demod_sts->ch_sts = dvbc_get_ch_sts(demod);
	demod_sts->ch_pow = dvbc_get_ch_power(demod);
	demod_sts->ch_snr = dvbc_get_snr(demod);
	demod_sts->ch_ber = dvbc_get_ber(demod);
	demod_sts->ch_per = dvbc_get_per(demod);
	demod_sts->symb_rate = dvbc_get_symb_rate(demod);
	demod_sts->freq_off = dvbc_get_freq_off(demod);
	demod_sts->dat1 = tuner_get_ch_power(&demod->frontend);

	if (seq) {
		seq_printf(seq, "ch_sts:0x%x,snr:%d dBx10,ber:%d,per:%d,srate:%d,freqoff:%dkHz\n",
			demod_sts->ch_sts, demod_sts->ch_snr, demod_sts->ch_ber,
			demod_sts->ch_per, demod_sts->symb_rate, demod_sts->freq_off);
		seq_printf(seq, "strength:%ddb,0xe0 status:%u,b4 status:%u,dagc_gain:%u\n",
			demod_sts->dat1, qam_read_reg(demod, 0x38) & 0xffff,
			qam_read_reg(demod, 0x2d) & 0xffff, qam_read_reg(demod, 0x29) & 0x7f);
		seq_printf(seq, "power:%ddb,0x31=0x%x\n", demod_sts->ch_pow & 0xffff,
			   qam_read_reg(demod, 0x31));
	} else {
		PR_DVBC("ch_sts is 0x%x, snr %d dBx10, ber %d, per %d, srate %d, freqoff %dkHz\n",
			demod_sts->ch_sts, demod_sts->ch_snr, demod_sts->ch_ber,
			demod_sts->ch_per, demod_sts->symb_rate, demod_sts->freq_off);
		PR_DVBC("strength %ddb,0xe0 status %u,b4 status %u, dagc_gain %u, power %ddb\n",
			demod_sts->dat1, qam_read_reg(demod, 0x38) & 0xffff,
			qam_read_reg(demod, 0x2d) & 0xffff, qam_read_reg(demod, 0x29) & 0x7f,
			demod_sts->ch_pow & 0xffff);
		PR_DVBC("0xba=0x%x, 0x07=0x%x, 0x3a=0x%x, 0x31=0x%x\n",
				qam_read_reg(demod, 0xba),
				qam_read_reg(demod, 0x07),
				qam_read_reg(demod, 0x3a),
				qam_read_reg(demod, 0x31));
	}

	return 0;
}

void dvbc_enable_irq(struct aml_dtvdemod *demod, int dvbc_irq)
{
	u32 mask;

	/* clear status */
	qam_read_reg(demod, 0x35);
	/* enable irq */
	mask = qam_read_reg(demod, 0x34);
	mask |= (1 << dvbc_irq);
	qam_write_reg(demod, 0x34, mask);
}

void dvbc_init_reg_ext(struct aml_dtvdemod *demod)
{
	qam_write_reg(demod, 0x7, 0xf33);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		/* set min sr to 3000 for cover 3500 ~ 7000 */
		if (demod->auto_sr)
			qam_write_reg(demod, 0x12, 0x50bb800);//0x50e1000 -->0x50bb800
		else
			qam_write_reg(demod, 0x12, 0x5000000 |
					(((demod->sr_val_hw - 100) & 0xffff) << 8));

		qam_write_reg(demod, 0x30, 0x41f2f69);
	}
}

u32 dvbc_get_ch_sts(struct aml_dtvdemod *demod)
{
	return qam_read_reg(demod, 0x6);
}

u32 dvbc_get_qam_mode(struct aml_dtvdemod *demod)
{
	return qam_read_reg(demod, 0x2) & 7;
}

void dvbc_cfg_sr_cnt(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd)
{
	switch (spd) {
	case SYM_SPEED_HIGH:
		qam_write_reg(demod, SYMB_CNT_CFG, 0xffff03ff);
		break;
	case SYM_SPEED_MIDDLE:
	case SYM_SPEED_NORMAL:
	default:
		qam_write_reg(demod, SYMB_CNT_CFG, 0xffff8ffe);
		break;
	}
}

void dvbc_cfg_sr_scan_speed(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd)
{
	switch (spd) {
	case SYM_SPEED_MIDDLE:
		qam_write_reg(demod, SR_SCAN_SPEED, 0x245bf45c);
		break;
	case SYM_SPEED_HIGH:
		qam_write_reg(demod, SR_SCAN_SPEED, 0x234cf523);
		break;
	case SYM_SPEED_NORMAL:
	default:
		qam_write_reg(demod, SR_SCAN_SPEED, 0x235cf459);
		break;
	}
}

void dvbc_cfg_tim_sweep_range(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd)
{
	switch (spd) {
	case SYM_SPEED_MIDDLE:
		qam_write_reg(demod, TIM_SWEEP_RANGE_CFG, 0x220000);
		break;
	case SYM_SPEED_HIGH:
		qam_write_reg(demod, TIM_SWEEP_RANGE_CFG, 0x400000);
		break;
	case SYM_SPEED_NORMAL:
	default:
		qam_write_reg(demod, TIM_SWEEP_RANGE_CFG, 0x400);
		break;
	}
}

void dvbc_cfg_sw_hw_sr_max(struct aml_dtvdemod *demod, unsigned int max_sr)
{
	/* bit[0-15]: sw_symbol_rate. */
	qam_write_bits(demod, 0xd, max_sr & 0xffff, 0, 16);

	/* bit[8-23]: hw_symbol_rate_max. */
	qam_write_bits(demod, 0x11, max_sr & 0xffff, 8, 16);

	/* bit[8-23]: hw_symbol_rate_min. */
	qam_write_bits(demod, 0x12, (max_sr - 100) & 0xffff, 8, 16);
}

static void sort_with_index(int size, int data[], int index[])
{
	int i = 0, j = 0;

	struct index_data {
		int value;
		int index;
	};

	struct index_data idx_data[5], temp;

	if (size > 5) {
		PR_ERR("error[size > 5]\n");

		return;
	}

	for (i = 0; i < size; i++) {
		idx_data[i].value = data[i];
		idx_data[i].index = i;
	}

	for (i = 0; i < size - 1; i++) {
		for (j = 0; j < size - i - 1; j++) {
			if (idx_data[j].value > idx_data[j + 1].value) {
				temp = idx_data[j];
				idx_data[j] = idx_data[j + 1];
				idx_data[j + 1] = temp;
			}
		}
	}

	for (i = 0; i < size; i++) {
		data[i] = idx_data[i].value;
		index[i] = idx_data[i].index;
	}
}

int dvbc_auto_qam_process(struct aml_dtvdemod *demod, unsigned int *qam_mode)
{
#define MSIZE 8
	unsigned int idx_1_acc = 0, idx_2_acc = 0, idx_4_acc = 0;
	unsigned int idx_8_acc = 0, idx_16_acc = 0, idx_32_acc = 0;
	unsigned int idx_64_acc = 0, idx_128_acc = 0, idx_256_acc = 0;
	unsigned int idx[64] = { 0 }, idx_77 = 0, idx_all = 0, temp = 0;
	unsigned int be = 0, bf = 0, c0 = 0;
	unsigned int d3 = 0, d4 = 0, d5 = 0;
	unsigned int eq_state = 0;
	unsigned int total_64_128_256_acc = 0;
	unsigned int total_32_64_128_256_acc = 0;
	unsigned int total_all_acc = 0;
	int i = 0, ret = 0;
	int dis_idx[5] = { 0 }, dis_val[5] = { 0 };
	unsigned int len = ARRAY_SIZE(dis_val);

	qam_mode[0] = 0xf;
	qam_mode[1] = 0xf;
	qam_mode[2] = 0xf;
	qam_mode[3] = 0xf;
	qam_mode[4] = 0xf;

	// 1. wait eq stable.
	for (i = 0; i < 5; i++) {
		eq_state = qam_read_reg(demod, 0x5d) & 0xf;
		PR_DVBC("eq_state: 0x%x\n", eq_state);
		if (eq_state > 0x01)
			break;

		msleep(50);
	}

	if (eq_state <= 0x01) {
		ret = -1;

		goto AUTO_QAM_DONE;
	}

	// 2. get QAM constellation map accumulated value.
	be = qam_read_reg(demod, 0xbe);
	bf = qam_read_reg(demod, 0xbf);
	c0 = qam_read_reg(demod, 0xc0);
	d4 = qam_read_reg(demod, 0xd4);
	d5 = qam_read_reg(demod, 0xd5);
	d3 = qam_read_reg(demod, 0xd3);

	PR_DVBC("0xbe[0x%x], 0xbf[0x%x], 0xc0[0x%x].\n", be, bf, c0);
	PR_DVBC("0xd3[0x%x], 0xd4[0x%x], 0xd5[0x%x].\n", d3, d4, d5);

	// 3. get QAM probability distribution.
	idx[0] = (be & 0x1);
	idx[0] += ((be >> 2) & 0x1) * 2;
	idx[0] += ((be >> 4) & 0x1) * 4;
	idx[0] += (bf & 0x1) * 8;
	idx[0] += ((bf >> 2) & 0x1) * 16;
	idx[0] += ((bf >> 4) & 0x1) * 32;
	idx[0] += (c0 & 0x1) * 64;
	idx[0] += ((c0 >> 2) & 0x1) * 128;
	idx[0] += ((c0 >> 4) & 0x1) * 256;

	idx_2_acc = d4 & 0x7f;
	idx_4_acc = (d4 & 0x7f00) >> 8;
	idx_8_acc = (d4 & 0x7f0000) >> 16;
	idx_16_acc = (d4 & 0x7f000000) >> 24;
	idx_32_acc = d5 & 0x7f;
	idx_64_acc = (d5 & 0x7f00) >> 8;
	idx_128_acc = (d5 & 0x7f0000) >> 16;
	idx_256_acc = (d5 & 0x7f000000) >> 24;

	idx_1_acc = (d3 & 0x3f0000) >> 16;
	idx_77 = (d3 & 0x7ff0) >> 4;
	idx_77 = (idx_77 * 10000) >> 2;

	idx_all = idx_256_acc * 256;
	idx_all += idx_128_acc * 128;
	idx_all += idx_64_acc * 64;
	idx_all += idx_32_acc * 32;
	idx_all += idx_16_acc * 16;
	idx_all += idx_8_acc * 8;
	idx_all += idx_4_acc * 4;
	idx_all += idx_2_acc * 2;
	idx_all += idx_1_acc;
	idx_all += idx[0];
	idx_all = idx_all >> 6;

	if (idx_all > 0)
		idx_77 = idx_77 / idx_all;

	total_64_128_256_acc = idx_64_acc + idx_128_acc + idx_256_acc;
	total_32_64_128_256_acc = total_64_128_256_acc + idx_32_acc;
	total_all_acc = total_32_64_128_256_acc + idx_16_acc;
	total_all_acc += idx_8_acc + idx_4_acc + idx_2_acc + idx_1_acc;

	PR_DVBC("idx_1_acc: 0x%x, idx_2_acc: 0x%x, idx_4_acc: 0x%x\n",
			idx_1_acc, idx_2_acc, idx_4_acc);
	PR_DVBC("idx_8_acc: 0x%x, idx_16_acc: 0x%x, index_32_acc: 0x%x\n",
			idx_8_acc, idx_16_acc, idx_32_acc);
	PR_DVBC("idx_64_acc: 0x%x, idx_128_acc: 0x%x, idx_256_acc: 0x%x\n",
			idx_64_acc, idx_128_acc, idx_256_acc);
	PR_DVBC("total_64_128_256_acc: 0x%x.\n", total_64_128_256_acc);
	PR_DVBC("total_32_64_128_256_acc: 0x%x.\n", total_32_64_128_256_acc);
	PR_DVBC("total_all_acc: 0x%x.\n", total_all_acc);
	PR_DVBC("idx_all: 0x%x(%d), idx_77: 0x%x(%d), idx[0]: %d\n",
			idx_all, idx_all, idx_77, idx_77, idx[0]);

	if (total_all_acc < 60 || total_64_128_256_acc <= 4) {
		ret = -2;

		goto AUTO_QAM_DONE;
	}

	// total_all_acc >= 60 && total_64_128_256_acc > 4
	dis_val[4] = abs(idx_256_acc - 64) + 1;
	dis_val[3] = abs(idx_256_acc - 56) + 1;
	dis_val[2] = abs(idx_256_acc - 32) + 1;
	dis_val[1] = abs(idx_256_acc - 16) + 1;
	dis_val[0] = abs(idx_256_acc - 8) + 1;

	PR_DVBC("orig dis_val: %d, %d, %d, %d, %d\n",
			dis_val[0], dis_val[1],
			dis_val[2], dis_val[3], dis_val[4]);

	sort_with_index(len, dis_val, dis_idx);

	PR_DVBC("sort dis_val(idx): %d(%d), %d(%d), %d(%d), %d(%d), %d(%d)\n",
			dis_val[0], dis_idx[0], dis_val[1], dis_idx[1],
			dis_val[2], dis_idx[2], dis_val[3], dis_idx[3],
			dis_val[4], dis_idx[4]);

	if (idx_77 <= 2000) {
		if (dis_idx[0] > 2 && dis_idx[1] > 2) {
			if (dis_idx[0] == 3) //128
				dis_val[0] = -1;
			else
				dis_val[1] = -1;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in idx_77 <= 2000  128 256\n");
		} else if (dis_idx[0] > 1 && dis_idx[1] > 1 &&
			dis_idx[0] < 4 && dis_idx[1] < 4) {
			if (dis_idx[0] == 3) //128
				dis_val[0] = -1;
			else
				dis_val[1] = -1;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in idx_77 <= 2000  128 64\n");
		} else if (dis_idx[0] > 0 && dis_idx[1] > 0 &&
			dis_idx[0] < 3 && dis_idx[1] < 3) {
			if (dis_idx[0] == 1) //32
				dis_val[0] = -1;
			else
				dis_val[1] = -1;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in idx_77 <= 2000  32 64\n\n");
		}
	} else {
		// > 2000
		if (dis_idx[0] > 2 && dis_idx[1] > 2 && dis_idx[2] == 2) {
			if (dis_idx[0] == 4) //256
				dis_val[0] = -1;
			else
				dis_val[1] = -1;

			if (dis_val[0] > dis_val[1]) { // sort
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			temp = dis_val[1];
			dis_val[1] = dis_val[2];
			dis_val[2] = temp;

			temp = dis_idx[1];
			dis_idx[1] = dis_idx[2];
			dis_idx[2] = temp;
		}

		if (dis_idx[0] < 2 && dis_idx[1] < 2 && dis_idx[2] == 2) {
			dis_val[2] = 0;
			temp = dis_val[1];
			dis_val[1] = dis_val[0];
			dis_val[0] = dis_val[2];
			dis_val[2] = temp;

			temp = dis_idx[1];
			dis_idx[1] = dis_idx[0];
			dis_idx[0] = dis_idx[2];
			dis_idx[2] = temp;

			if (dis_idx[0] < 2 && dis_idx[1] < 2 && dis_idx[3] == 2) {
				dis_val[3] = 0;
				temp = dis_val[1];
				dis_val[1] = dis_val[0];
				dis_val[0] = dis_val[3];
				dis_val[3] = dis_val[2];
				dis_val[2] = temp;

				temp = dis_idx[1];
				dis_idx[1] = dis_idx[0];
				dis_idx[0] = dis_idx[3];
				dis_idx[3] = dis_idx[2];
				dis_idx[2] = temp;
			}
		}

		PR_DVBC("in idx_77 > 2000  128 256\n\n");
	}

	if (idx[0] < 25) {
		if (dis_idx[0] > 0 && dis_idx[1] > 0 &&
			dis_idx[0] < 3 && dis_idx[1] < 3) {
			if (dis_idx[0] == 1) //32
				dis_val[0] = -2;
			else
				dis_val[1] = -2;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in index_00 < 25  32 64\n\n");
		}
	}

	if (idx[0] < 150) {
		if (dis_idx[0] > 1 && dis_idx[1] > 1 &&
			dis_idx[0] < 4 && dis_idx[1] < 4) {
			if (dis_idx[0] == 2) //64
				dis_val[0] = -3;
			else
				dis_val[1] = -3;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in index_00 < 150  128 64\n\n");
		}
	}

	if (dis_idx[0] < 2 && dis_idx[1] < 2) {
		if (total_32_64_128_256_acc == 8) {
			if (dis_idx[0] == 0) //16
				dis_val[0] = -4;
			else
				dis_val[1] = -4;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in aa == 8  16\n\n");
		} else {
			if (dis_idx[0] == 1) //32
				dis_val[0] = -4;
			else
				dis_val[1] = -4;

			if (dis_val[0] > dis_val[1]) {
				temp = dis_val[0];
				dis_val[0] = dis_val[1];
				dis_val[1] = temp;

				temp = dis_idx[0];
				dis_idx[0] = dis_idx[1];
				dis_idx[1] = temp;
			}

			PR_DVBC("in aa > or < 8  32\n");
		}
	}

	qam_mode[0] = dis_idx[0];
	qam_mode[1] = dis_idx[1];
	qam_mode[2] = dis_idx[2];
	qam_mode[3] = dis_idx[3];
	qam_mode[4] = dis_idx[4];

AUTO_QAM_DONE:
	if (!ret)
		PR_INFO("find qam: %d, %d, %d, %d, %d\n",
				qam_mode[0], qam_mode[1],
				qam_mode[2], qam_mode[3], qam_mode[4]);
	else
		PR_INFO("can not find qam(ret %d)\n", ret);

	return ret;
}
