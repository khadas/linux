// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 /* Enables DVBv3 compatibility bits at the headers */
#define __DVB_CORE__	/*ary 2018-1-31*/

#include "demod_func.h"
#include "aml_demod.h"
#include <linux/string.h>
#include <linux/kernel.h>
#include "acf_filter_coefficient.h"
#include <linux/mutex.h>
#include "dvbt_func.h"

void dvbt_write_regb(unsigned long addr, int index, unsigned long data)
{
	/*to achieve write func*/
}

int dvbt_isdbt_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_dvbt *demod_dvbt)
{
	int ret = 0;
	u8_t demod_mode = 1;
	u8_t bw, sr, ifreq, agc_mode;
	u32_t ch_freq;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_INFO("err: %s devp is null\n", __func__);
		return -1;
	}

	if (devp->data->hw_ver == DTVDEMOD_HW_T5D)
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

	demod_sta->ch_mode = 0;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	/*   if (demod_i2c->tuner == 1) */
	/*     demod_sta->ch_if = 36130;*/
	/* else if (demod_i2c->tuner == 2)*/
	/*     demod_sta->ch_if = 4570;*/
	/* else if (demod_i2c->tuner == 3)*/
	/*     demod_sta->ch_if = 4000;// It is nouse.(alan)*/
	/* else if (demod_i2c->tuner == 7)*/
	/*     demod_sta->ch_if = 5000;//silab 5000kHz IF*/

	demod_sta->ch_bw = (8 - bw) * 1000;
	demod_sta->symb_rate = 0;	/* TODO */

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

static void setsrcgain(unsigned int val)
{
	dvbt_t2_wrb(0x15b2, val & 0xff);
	dvbt_t2_wrb(0x15b3, (dvbt_t2_rdb(0x15b3) & 0xf0) | ((val & 0xf00) >> 8));
}

static unsigned int calcul_src_gain_m(int bw, int s_r)
{
	unsigned int src_gain = 0;

	src_gain = (4096 * 4 * 64 * bw) / (7 * 8 * s_r * 2);

	return src_gain;
}

static void settrl(unsigned int val)
{
	dvbt_t2_wrb(0x2890, val & 0xff);
	dvbt_t2_wrb(0x2891, (val & 0xff00) >> 8);
	dvbt_t2_wrb(0x2892, (val & 0xff0000) >> 16);
	dvbt_t2_wrb(0x2893, (val & 0x3000000) >> 24);
}

unsigned int calcul_tr_inominal_rate_m(int bw, int s_r)
{
	unsigned int nominal_rate = 0;
	/* attention enum tordu! */
	const unsigned short table65a[] = {7, 71, 7, 7, 7, 7, 7, 1, 7, 7, 7};
	const unsigned short table65b[] = {64, 131, 64, 64, 64, 40, 48, 8, 64, 80, 80};

	nominal_rate = (134217728 / (s_r * table65a[bw]) * table65b[bw]) + 1;
	return nominal_rate;
}

void ini_icfo_pn_index(int mode)
{				/* 00:DVBT,01:ISDBT */
	if (mode == 0) {
		dvbt_isdbt_wr_reg(0x3f8, 0x00000031);
		dvbt_isdbt_wr_reg(0x3fc, 0x00030000);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000032);
		dvbt_isdbt_wr_reg(0x3fc, 0x00057036);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000033);
		dvbt_isdbt_wr_reg(0x3fc, 0x0009c08d);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000034);
		dvbt_isdbt_wr_reg(0x3fc, 0x000c90c0);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000035);
		dvbt_isdbt_wr_reg(0x3fc, 0x001170ff);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000036);
		dvbt_isdbt_wr_reg(0x3fc, 0x0014d11a);
	} else if (mode == 1) {
		dvbt_isdbt_wr_reg(0x3f8, 0x00000031);
		dvbt_isdbt_wr_reg(0x3fc, 0x00085046);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000032);
		dvbt_isdbt_wr_reg(0x3fc, 0x0019a0e9);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000033);
		dvbt_isdbt_wr_reg(0x3fc, 0x0024b1dc);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000034);
		dvbt_isdbt_wr_reg(0x3fc, 0x003b3313);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000035);
		dvbt_isdbt_wr_reg(0x3fc, 0x0048d409);
		dvbt_isdbt_wr_reg(0x3f8, 0x00000036);
		dvbt_isdbt_wr_reg(0x3fc, 0x00527509);
	}
}

static int coef[] = {
	0xf900, 0xfe00, 0x0000, 0x0000, 0x0100, 0x0100, 0x0000, 0x0000,
	0xfd00, 0xf700, 0x0000, 0x0000, 0x4c00, 0x0000, 0x0000, 0x0000,
	0x2200, 0x0c00, 0x0000, 0x0000, 0xf700, 0xf700, 0x0000, 0x0000,
	0x0300, 0x0900, 0x0000, 0x0000, 0x0600, 0x0600, 0x0000, 0x0000,
	0xfc00, 0xf300, 0x0000, 0x0000, 0x2e00, 0x0000, 0x0000, 0x0000,
	0x3900, 0x1300, 0x0000, 0x0000, 0xfa00, 0xfa00, 0x0000, 0x0000,
	0x0100, 0x0200, 0x0000, 0x0000, 0xf600, 0x0000, 0x0000, 0x0000,
	0x0700, 0x0700, 0x0000, 0x0000, 0xfe00, 0xfb00, 0x0000, 0x0000,
	0x0900, 0x0000, 0x0000, 0x0000, 0x3200, 0x1100, 0x0000, 0x0000,
	0x0400, 0x0400, 0x0000, 0x0000, 0xfe00, 0xfb00, 0x0000, 0x0000,
	0x0e00, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfb00, 0x0000, 0x0000,
	0x0100, 0x0200, 0x0000, 0x0000, 0xf400, 0x0000, 0x0000, 0x0000,
	0x3900, 0x1300, 0x0000, 0x0000, 0x1700, 0x1700, 0x0000, 0x0000,
	0xfc00, 0xf300, 0x0000, 0x0000, 0x0c00, 0x0000, 0x0000, 0x0000,
	0x0300, 0x0900, 0x0000, 0x0000, 0xee00, 0x0000, 0x0000, 0x0000,
	0x2200, 0x0c00, 0x0000, 0x0000, 0x2600, 0x2600, 0x0000, 0x0000,
	0xfd00, 0xf700, 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000,
	0xf900, 0xfe00, 0x0000, 0x0000, 0x0400, 0x0b00, 0x0000, 0x0000,
	0xf900, 0x0000, 0x0000, 0x0000, 0x0700, 0x0200, 0x0000, 0x0000,
	0x2100, 0x2100, 0x0000, 0x0000, 0x0200, 0x0700, 0x0000, 0x0000,
	0xf900, 0x0000, 0x0000, 0x0000, 0x0b00, 0x0400, 0x0000, 0x0000,
	0xfe00, 0xf900, 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000,
	0xf700, 0xfd00, 0x0000, 0x0000, 0x2600, 0x2600, 0x0000, 0x0000,
	0x0c00, 0x2200, 0x0000, 0x0000, 0xee00, 0x0000, 0x0000, 0x0000,
	0x0900, 0x0300, 0x0000, 0x0000, 0x0c00, 0x0000, 0x0000, 0x0000,
	0xf300, 0xfc00, 0x0000, 0x0000, 0x1700, 0x1700, 0x0000, 0x0000,
	0x1300, 0x3900, 0x0000, 0x0000, 0xf400, 0x0000, 0x0000, 0x0000,
	0x0200, 0x0100, 0x0000, 0x0000, 0xfb00, 0xfb00, 0x0000, 0x0000,
	0x0e00, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfe00, 0x0000, 0x0000,
	0x0400, 0x0400, 0x0000, 0x0000, 0x1100, 0x3200, 0x0000, 0x0000,
	0x0900, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfe00, 0x0000, 0x0000,
	0x0700, 0x0700, 0x0000, 0x0000, 0xf600, 0x0000, 0x0000, 0x0000,
	0x0200, 0x0100, 0x0000, 0x0000, 0xfa00, 0xfa00, 0x0000, 0x0000,
	0x1300, 0x3900, 0x0000, 0x0000, 0x2e00, 0x0000, 0x0000, 0x0000,
	0xf300, 0xfc00, 0x0000, 0x0000, 0x0600, 0x0600, 0x0000, 0x0000,
	0x0900, 0x0300, 0x0000, 0x0000, 0xf700, 0xf700, 0x0000, 0x0000,
	0x0c00, 0x2200, 0x0000, 0x0000, 0x4c00, 0x0000, 0x0000, 0x0000,
	0xf700, 0xfd00, 0x0000, 0x0000, 0x0100, 0x0100, 0x0000, 0x0000,
	0xfe00, 0xf900, 0x0000, 0x0000, 0x0b00, 0x0400, 0x0000, 0x0000,
	0xfc00, 0xfc00, 0x0000, 0x0000, 0x0200, 0x0700, 0x0000, 0x0000,
	0x4200, 0x0000, 0x0000, 0x0000, 0x0700, 0x0200, 0x0000, 0x0000,
	0xfc00, 0xfc00, 0x0000, 0x0000, 0x0400, 0x0b00, 0x0000, 0x0000
};

void tfd_filter_coff_ini(void)
{
	int i = 0;

	for (i = 0; i < 336; i++) {
		dvbt_isdbt_wr_reg(0x99 * 4, (i << 16) | coef[i]);
		dvbt_isdbt_wr_reg(0x03 * 4, (1 << 12));
	}
}

void ofdm_initial(int bandwidth,
		/* 00:8M 01:7M 10:6M 11:5M */
		int samplerate,
		/* 00:45M 01:20.8333M 10:20.7M 11:28.57 100: 24.00 */
		int IF,
		/* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		int mode,
		/* 00:DVBT,01:ISDBT */
		int tc_mode
		/* 0: Unsigned, 1:TC */
		)
{
	int tmp;
	int ch_if;
	int adc_freq;
	/*int memstart;*/
	PR_DVBT
	    ("[ofdm_initial]bandwidth is %d,samplerate is %d",
	     bandwidth, samplerate);
	PR_DVBT
	    ("IF is %d, mode is %d,tc_mode is %d\n",
	    IF, mode, tc_mode);
	switch (IF) {
	case 0:
		ch_if = 36130;
		break;
	case 1:
		ch_if = -5500;
		break;
	case 2:
		ch_if = 4570;
		break;
	case 3:
		ch_if = 4000;
		break;
	case 4:
		ch_if = 5000;
		break;
	default:
		ch_if = 4000;
		break;
	}
	switch (samplerate) {
	case 0:
		adc_freq = 45000;
		break;
	case 1:
		adc_freq = 20833;
		break;
	case 2:
		adc_freq = 20700;
		break;
	case 3:
		adc_freq = 28571;
		break;
	case 4:
		adc_freq = 24000;
		break;
	case 5:
		adc_freq = 25000;
		break;
	default:
		adc_freq = 28571;
		break;
	}

	dvbt_isdbt_wr_reg((0x02 << 2), 0x00800000);
	/* SW reset bit[23] ; write anything to zero */
	dvbt_isdbt_wr_reg((0x00 << 2), 0x00000000);

	dvbt_isdbt_wr_reg((0xe << 2), 0xffff);
	/* enable interrupt */

	if (mode == 0) {	/* DVBT */
		switch (samplerate) {
		case 0:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00005a00);
			break;	/* 45MHz */
		case 1:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x000029aa);
			break;	/* 20.833 */
		case 2:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00002966);
			break;	/* 20.7   SAMPLERATE*512 */
		case 3:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00003924);
			break;	/* 28.571 */
		case 4:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00003000);
			break;	/* 24 */
		case 5:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00003200);
			break;	/* 25 */
		default:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00003924);
			break;	/* 28.571 */
		}
	} else {		/* ISDBT */
		switch (samplerate) {
		case 0:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x0000580d);
			break;	/* 45MHz */
		case 1:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x0000290d);
			break;	/* 20.833 = 56/7 * 20.8333 / (512/63)*512 */
		case 2:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x000028da);
			break;	/* 20.7 */
		case 3:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x0000383F);
			break;	/* 28.571  3863 */
		case 4:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00002F40);
			break;	/* 24 */
		default:
			dvbt_isdbt_wr_reg((0x08 << 2), 0x00003863);
			break;	/* 28.571 */
		}
	}
	/* memstart = 0x35400000;*/
	/* PR_DVBT("memstart is %x\n", memstart);*/
	/* dvbt_write_reg((0x10 << 2), memstart);*/
	/* 0x8f300000 */

	dvbt_isdbt_wr_reg((0x14 << 2), 0xe81c4ff6);
	/* AGC_TARGET 0xf0121385 */

	switch (samplerate) {
	case 0:
		dvbt_isdbt_wr_reg((0x15 << 2), 0x018c2df2);
		break;
	case 1:
		dvbt_isdbt_wr_reg((0x15 << 2), 0x0185bdf2);
		break;
	case 2:
		dvbt_isdbt_wr_reg((0x15 << 2), 0x0185bdf2);
		break;
	case 3:
		dvbt_isdbt_wr_reg((0x15 << 2), 0x0187bdf2);
		break;
	case 4:
		dvbt_isdbt_wr_reg((0x15 << 2), 0x0187bdf2);
		break;
	default:
		dvbt_isdbt_wr_reg((0x15 << 2), 0x0187bdf2);
		break;
	}
	if (tc_mode == 1)
		dvbt_write_regb((0x15 << 2), 11, 0);
	/* For TC mode. Notice, For ADC input is Unsigned,*/
	/* For Capture Data, It is TC. */
	dvbt_write_regb((0x15 << 2), 26, 1);
	/* [19:0] = [I , Q], I is high, Q is low. This bit is swap I/Q. */

	dvbt_isdbt_wr_reg((0x16 << 2), 0x00047f80);
	/* AGC_IFGAIN_CTRL */
	dvbt_isdbt_wr_reg((0x17 << 2), 0x00027f80);
	/* AGC_RFGAIN_CTRL */
	dvbt_isdbt_wr_reg((0x18 << 2), 0x00000190);
	/* AGC_IFGAIN_ACCUM */
	dvbt_isdbt_wr_reg((0x19 << 2), 0x00000190);
	/* AGC_RFGAIN_ACCUM */
	if (ch_if < 0)
		ch_if += adc_freq;
	if (ch_if > adc_freq)
		ch_if -= adc_freq;

	tmp = ch_if * (1 << 15) / adc_freq;
	dvbt_isdbt_wr_reg((0x20 << 2), tmp);

	dvbt_isdbt_wr_reg((0x21 << 2), 0x001ff000);
	/* DDC CS_FCFO_ADJ_CTRL */
	dvbt_isdbt_wr_reg((0x22 << 2), 0x00000000);
	/* DDC ICFO_ADJ_CTRL */
	dvbt_isdbt_wr_reg((0x23 << 2), 0x00004000);
	/* DDC TRACK_FCFO_ADJ_CTRL */

	dvbt_isdbt_wr_reg((0x27 << 2), (1 << 23)
	| (3 << 19) | (3 << 15) |  (1000 << 4) | 9);
	/* {8'd0,1'd1,4'd3,4'd3,11'd50,4'd9});//FSM_1 */
	dvbt_isdbt_wr_reg((0x28 << 2), (100 << 13) | 1000);
	/* {8'd0,11'd40,13'd50});//FSM_2 */
	dvbt_isdbt_wr_reg((0x29 << 2), (31 << 20) | (1 << 16) |
	(24 << 9) | (3 << 6) | 20);
	/* {5'd0,7'd127,1'd0,3'd0,7'd24,3'd5,6'd20}); */
	/*8K cannot sync*/
	dvbt_isdbt_wr_reg((0x29 << 2),
			dvbt_isdbt_rd_reg((0x29 << 2)) |
			0x7f << 9 | 0x7f << 20);

	if (mode == 0) {	/* DVBT */
		if (bandwidth == 0) {	/* 8M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_8m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x004ebf2e);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_8m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00247551);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_8m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00243999);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_8m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0031ffcd);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_8m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x002A0000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_8m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0031ffcd);
				break;	/* 28.57M */
			}
		} else if (bandwidth == 1) {	/* 7M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_7m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0059ff10);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_7m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0029aaa6);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_7m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00296665);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_7m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00392491);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_7m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00300000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_7m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00392491);
				break;	/* 28.57M */
			}
		} else if (bandwidth == 2) {	/* 6M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_6m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00690000);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_6m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00309c3e);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_6m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x002eaaaa);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_6m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0042AA69);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_6m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00380000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_6m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0042AA69);
				break;	/* 28.57M */
			}
		} else {	/* 5M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_5m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x007dfbe0);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_5m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x003a554f);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_5m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x0039f5c0);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_5m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x004FFFFE);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_5m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x00433333);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_5m();
				dvbt_isdbt_wr_reg((0x44 << 2),
					      0x004FFFFE);
				break;	/* 28.57M */
			}
		}
	} else {		/* ISDBT */
		switch (samplerate) {
		case 0:
			ini_acf_iireq_src_45m_6m();
			dvbt_isdbt_wr_reg((0x44 << 2), 0x00589800);
			break;
			/* 45M */
			/*SampleRate/(symbolRate)*2^20, */
			/*symbolRate = 512/63 for isdbt */
		case 1:
			ini_acf_iireq_src_207m_6m();
			dvbt_isdbt_wr_reg((0x44 << 2), 0x002903d4);
			break;	/* 20.833M */
		case 2:
			ini_acf_iireq_src_207m_6m();
			dvbt_isdbt_wr_reg((0x44 << 2), 0x00280ccc);
			break;	/* 20.7M */
		case 3:
			ini_acf_iireq_src_2857m_6m();
			dvbt_isdbt_wr_reg((0x44 << 2), 0x00383fc8);
			break;	/* 28.57M */
		case 4:
			ini_acf_iireq_src_24m_6m();
			dvbt_isdbt_wr_reg((0x44 << 2), 0x002F4000);
			break;	/* 24M */
		default:
			ini_acf_iireq_src_2857m_6m();
			dvbt_isdbt_wr_reg((0x44 << 2), 0x00383fc8);
			break;	/* 28.57M */
		}
	}

	if (mode == 0)		/* DVBT */
		dvbt_isdbt_wr_reg((0x02 << 2),
			      (bandwidth << 20) | 0x10002);
	else			/* ISDBT */
		dvbt_isdbt_wr_reg((0x02 << 2), (1 << 20) | 0x1001a);
	/* {0x000,2'h1,20'h1_001a});    //For ISDBT , bandwidth should be 1,*/

	dvbt_isdbt_wr_reg((0x45 << 2), 0x00000000);
	/* SRC SFO_ADJ_CTRL */
	dvbt_isdbt_wr_reg((0x46 << 2), 0x02004000);
	/* SRC SFO_ADJ_CTRL */
	dvbt_isdbt_wr_reg((0x48 << 2), 0x000c0287);
	/* DAGC_CTRL1 */
	dvbt_isdbt_wr_reg((0x49 << 2), 0x00000005);
	/* DAGC_CTRL2 */
	dvbt_isdbt_wr_reg((0x4c << 2), 0x00000bbf);
	/* CCI_RP */
	dvbt_isdbt_wr_reg((0x4d << 2), 0x00000376);
	/* CCI_RPSQ */
	dvbt_isdbt_wr_reg((0x4e << 2), 0x0f0f1d09);
	/* CCI_CTRL */
	dvbt_isdbt_wr_reg((0x4f << 2), 0x00000000);
	/* CCI DET_INDX1 */
	dvbt_isdbt_wr_reg((0x50 << 2), 0x00000000);
	/* CCI DET_INDX2 */
	dvbt_isdbt_wr_reg((0x51 << 2), 0x00000000);
	/* CCI_NOTCH1_A1 */
	dvbt_isdbt_wr_reg((0x52 << 2), 0x00000000);
	/* CCI_NOTCH1_A2 */
	dvbt_isdbt_wr_reg((0x53 << 2), 0x00000000);
	/* CCI_NOTCH1_B1 */
	dvbt_isdbt_wr_reg((0x54 << 2), 0x00000000);
	/* CCI_NOTCH2_A1 */
	dvbt_isdbt_wr_reg((0x55 << 2), 0x00000000);
	/* CCI_NOTCH2_A2 */
	dvbt_isdbt_wr_reg((0x56 << 2), 0x00000000);
	/* CCI_NOTCH2_B1 */
	dvbt_isdbt_wr_reg((0x58 << 2), 0x00000885);
	/* MODE_DETECT_CTRL // 582 */
	if (mode == 0)		/* DVBT */
		dvbt_isdbt_wr_reg((0x5c << 2), 0x00001011);	/*  */
	else
		dvbt_isdbt_wr_reg((0x5c << 2), 0x00000753);
	/* ICFO_EST_CTRL ISDBT ICFO thres = 2 */

	dvbt_isdbt_wr_reg((0x5f << 2), 0x0ffffe10);
	/* TPS_FCFO_CTRL */
	dvbt_isdbt_wr_reg((0x61 << 2), 0x0000006c);
	/* FWDT ctrl */
	dvbt_isdbt_wr_reg((0x68 << 2), 0x128c3929);
	dvbt_isdbt_wr_reg((0x69 << 2), 0x91017f2d);
	/* 0x1a8 */
	dvbt_isdbt_wr_reg((0x6b << 2), 0x00442211);
	/* 0x1a8 */
	dvbt_isdbt_wr_reg((0x6c << 2), 0x01fc400a);
	/* 0x */
	dvbt_isdbt_wr_reg((0x6d << 2), 0x0030303f);
	/* 0x */
	dvbt_isdbt_wr_reg((0x73 << 2), 0xffffffff);
	/* CCI0_PILOT_UPDATE_CTRL */
	dvbt_isdbt_wr_reg((0x74 << 2), 0xffffffff);
	/* CCI0_DATA_UPDATE_CTRL */
	dvbt_isdbt_wr_reg((0x75 << 2), 0xffffffff);
	/* CCI1_PILOT_UPDATE_CTRL */
	dvbt_isdbt_wr_reg((0x76 << 2), 0xffffffff);
	/* CCI1_DATA_UPDATE_CTRL */

	tmp = mode == 0 ? 0x000001a2 : 0x00000da2;
	dvbt_isdbt_wr_reg((0x78 << 2), tmp);	/* FEC_CTR */

	dvbt_isdbt_wr_reg((0x7d << 2), 0x0000009d);
	dvbt_isdbt_wr_reg((0x7e << 2), 0x00004000);
	dvbt_isdbt_wr_reg((0x7f << 2), 0x00008000);

	dvbt_isdbt_wr_reg(((0x8b + 0) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 1) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 2) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 3) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 4) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 5) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 6) << 2), 0x20002000);
	dvbt_isdbt_wr_reg(((0x8b + 7) << 2), 0x20002000);

	dvbt_isdbt_wr_reg((0x93 << 2), 0x31);
	dvbt_isdbt_wr_reg((0x94 << 2), 0x00);
	dvbt_isdbt_wr_reg((0x95 << 2), 0x7f1);
	dvbt_isdbt_wr_reg((0x96 << 2), 0x20);

	dvbt_isdbt_wr_reg((0x98 << 2), 0x03f9115a);
	dvbt_isdbt_wr_reg((0x9b << 2), 0x000005df);

	dvbt_isdbt_wr_reg((0x9c << 2), 0x00100000);
	/* TestBus write valid, 0 is system clk valid */
	dvbt_isdbt_wr_reg((0x9d << 2), 0x01000000);
	/* DDR Start address */
	dvbt_isdbt_wr_reg((0x9e << 2), 0x02000000);
	/* DDR End   address */

	dvbt_write_regb((0x9b << 2), 7, 0);
	/* Enable Testbus dump to DDR */
	dvbt_write_regb((0x9b << 2), 8, 0);
	/* Run Testbus dump to DDR */

	dvbt_isdbt_wr_reg((0xd6 << 2), 0x00000003);
	dvbt_isdbt_wr_reg((0xd8 << 2), 0x00000120);
	dvbt_isdbt_wr_reg((0xd9 << 2), 0x01010101);

	ini_icfo_pn_index(mode);
	tfd_filter_coff_ini();

	calculate_cordic_para();
	msleep(20);
	/* delay_us(1); */

	dvbt_isdbt_wr_reg((0x02 << 2),
		      dvbt_isdbt_rd_reg((0x02 << 2)) | (1 << 0));
	dvbt_isdbt_wr_reg((0x02 << 2),
		      dvbt_isdbt_rd_reg((0x02 << 2)) | (1 << 24));
/* dvbt_check_status(); */
}

void calculate_cordic_para(void)
{
	dvbt_isdbt_wr_reg(0x0c, 0x00000040);
}

static int dvbt_get_status(void)
{
	return dvbt_isdbt_rd_reg(0x0) >> 12 & 1;
}

static int dvbt_get_ber(void)
{
	return dvbt_isdbt_rd_reg((0xbf << 2));
}

static int dvbt_get_snr(struct aml_demod_sta *demod_sta)
{
	return ((dvbt_isdbt_rd_reg((0x0a << 2))) >> 20) & 0x3ff;
	/*dBm: bit0~bit2=decimal */
}

static int dvbt_get_strength(struct aml_demod_sta *demod_sta)
{
/* int dbm = dvbt_get_ch_power(demod_sta, demod_i2c); */
/* return dbm; */
	return 0;
}

static int dvbt_get_ucblocks(struct aml_demod_sta *demod_sta)
{
	return 0;
/* return dvbt_get_per(); */
}

struct demod_status_ops *dvbt_get_status_ops(void)
{
	static struct demod_status_ops ops = {
		.get_status = dvbt_get_status,
		.get_ber = dvbt_get_ber,
		.get_snr = dvbt_get_snr,
		.get_strength = dvbt_get_strength,
		.get_ucblocks = dvbt_get_ucblocks,
	};

	return &ops;
}

struct st_chip_register_t {
	unsigned short addr;/* Address */
	char value;/* Current value */
};

struct st_chip_register_t reset368_dvbt2_val[] = {
	{R368TER_I2CRPT, 0x34},
	{R368TER_TOPCTRL, 0x00},
	{R368TER_STDBY0, 0x02}, /* PJ 10/2013 was 0x0b with both DVBSX and DVBC set */
	{R368TER_STDBY1, 0x05},
	{R368TER_RESET0, 0x00},
	{R368TER_RESET1, 0xd1},
	{R368TER_RESET1, 0xd0},
	{R368TER_LOWPOW0, 0x04},
	{R368TER_LOWPOW1, 0xf8},
	{R368TER_LOWPOW2, 0xff},
	{R368TER_PAD_CFG0, 0x04},
	{R368TER_AUX_CLK, 0x02},
	{R368TER_DVBX_CHOICE, 0x33},
	{R368TER_CLK_XP70_CFG, 0x00},
	{R368TER_TEST_CFG0, 0x70},/* PT 01/2013 TS conf change was 0x7B */
	{R368TER_TEST_CFG1, 0x00},/* PT 01/2013 package change was 0x08 */
	/* XP70_CONF1 PJ 11/2013 write XP70_CONF1 in init() array BZ 40019 */
	{R368TER_XP70_CONF1, 0x80},
	{R368TER_SERIAL_XP70_DBG0, 0x30},
	{R368TER_SERIAL_XP70_DBG1, 0x03},
	{R368TER_ANA_CTRL, 0x81},/* PJ/TA/MS 10/2012 was 0x00 */
	{R368TER_ANADIG_CTRL, 0x1F},
	{R368TER_PLLODF, 0x0A},
	/* test bus addr3, change from 0x12 to 0xe for bus hang issue */
	{R368TER_PLLNDIV, 0xe},
	{R368TER_PLLIDF, 0x29},
	{R368TER_DUAL_AD12, 0x07},
	{R368TER_PAD_COMP_CTRL, 0x00},
	{R368TER_SIGD_FREQ0, 0x77},
	{R368TER_SIGD_FREQ1, 0x36},
	{R368TER_SIGD_FREQ2, 0x00},
	{R368TER_SIGD_FREQ3, 0x00},
	{R368TER_SIGD0, 0x94},
	{R368TER_SIGD1, 0x95},
	{R368TER_SIGD2, 0x91},
	{R368TER_SIGD3, 0x8E},
	/* PJ 10/2013 reset mailbox registers before getting F/W version after init */
	{R368TER_MB4, 0x00},
	{R368TER_MB5, 0x00},
	{R368TER_MB6, 0x00},
	{R368TER_MB7, 0x00},
	{R368TER_TEST_CONF1, 0x00},
	{R368TER_P1_TSCFGH, 0xA0},/* serial TS + TEI bit update */
	{R368TER_AGC_DBW0, 0x00},
	{R368TER_AGC_DBW1, 0x05},
	{R368TER_TOP_AGC_CONF1, 0xF0},
	{R368TER_AGC_FREEZE_CONF0, 0x43},
	{R368TER_TOP_AGC_CONF5, 0x13},
	{R368TER_TSTTS4, 0x01},
	{R368TER_IQFE_AGC_CONF0, 0x70},
	{R368TER_IQFE_AGC_CONF1, 0x8C},
	{R368TER_IQFE_AGC_CONF2, 0x30},
	{R368TER_IQFE_AGC_CONF3, 0x3e},
	{R368TER_IQFE_AGC_CONF4, 0x29},
	{R368TER_AGC_TARGETI0, 0x6e},
	{R368TER_AGC_TARGETI1,	0x00},
	{R368TER_AGC_TARGETQ0,	0x6e},
	{R368TER_AGC_TARGETQ1,	0x00},
	{R368TER_AGC_CONF5, 0xc6},
	{R368TER_AGC_CONF6, 0x90},
	{R368TER_LOCK_N, 0x20},
	{R368TER_SRC_CONF1, 0x00},
	{R368TER_P1_TSMINSPEED, 0xB0},/* PJ/PT 11/2013 */
	{R368TER_P1_TSMAXSPEED, 0x20},
	{R368TER_P1_CTRL, 0x40},
	{R368TER_P1_DIV, 0x80},
	{R368TER_P1_CFG, 0x08},
	{R368TER_CHC_SNR10, 0xC9},
	/* PJ/PG 10/2013 force BB to T/T2 mode instead of auto (was 00) */
	{R368TER_P1_SYMBCFG2, 0x55}
};

void dvbt2_init(void)
{
	/* bandwidth: 1=8M,2=7M,3=6M,4=5M,5=1.7M */
	int i;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("%s devp is NULL\n", __func__);
		return;
	}

	dvbt_t2_wrb(0x0004, 0xff);
	dvbt_t2_wrb(0x0005, 0xff);
	dvbt_t2_wrb(0x0006, 0xff);
	dvbt_t2_wrb(0x0007, 0xff);
	dvbt_t2_wrb(0x0008, 0xff);
	dvbt_t2_wrb(0x0009, 0xff);
	dvbt_t2_wrb(0x000a, 0xff);
	dvbt_t2_wrb(0x4, 0x00);
	dvbt_t2_wrb(0x5, 0x00);
	dvbt_t2_wrb(0x6, 0x00);
	dvbt_t2_wrb(0x7, 0x00);
	dvbt_t2_wrb(0x8, 0x00);
	dvbt_t2_wrb(0x9, 0x40);

	for (i = 1; i <= (sizeof(reset368_dvbt2_val) / sizeof(struct st_chip_register_t)); i++)
		dvbt_t2_wrb(reset368_dvbt2_val[i - 1].addr, reset368_dvbt2_val[i - 1].value);

	/* time deinterleave output speed, 8 cycle per data */
	dvbt_t2_wrb(0x3613, 0xd2);
	/* wr num < half-full threshood */
	dvbt_t2_wrb(0x3614, 0x80);
	dvbt_t2_wrb(0x3615, 0x0);

	dvbt_t2_wrb(0x1c, 0x8);
	dvbt_t2_wrb(0x19, 0x03);
	dvbt_t2_wrb(0x0598, 0x31);
	dvbt_t2_wrb(0x05d8, 0x31);
	dvbt_t2_wrb(0x0825, 0x70);
	dvbt_t2_wrb(0x08f0, 0x0c);
	dvbt_t2_wrb(0x1590, 0x80);
	dvbt_t2_wrb(0x1593, 0x80);
	dvbt_t2_wrb(0x15e0, 0x09);
	dvbt_t2_wrb(0x2754, 0x40);
	dvbt_t2_wrb(0x80c, 0xff);
	dvbt_t2_wrb(0x80d, 0xff);
	dvbt_t2_wrb(0x80e, 0xff);
	dvbt_t2_wrb(0x80f, 0xff);
	dvbt_t2_wrb(0x01a, 0x14);
	dvbt_t2_wrb(0x1560, 0x50);
	dvbt_t2_wrb(0x1561, 0x29);
	dvbt_t2_wrb(0x1563, 0x56);
	dvbt_t2_wrb(0x1564, 0x33);
	dvbt_t2_wrb(0x156c, 0x68);
	dvbt_t2_wrb(0x1594, 0x01);
	dvbt_t2_wrb(0x2835, 0x0e);
	dvbt_t2_wrb(0x804, 0x0f);
	dvbt_t2_wrb(0x2830, 0x40);

	dvbt_t2_wrb(0x281b, 0x02);
	dvbt_t2_wrb(0x2810, 0x02);
	dvbt_t2_wrb(0x2813, 0x02);
	dvbt_t2_wrb(0x2817, 0x02);
	dvbt_t2_wrb(0x281f, 0x02);
	dvbt_t2_wrb(0x2824, 0x02);
	dvbt_t2_wrb(0x281b, 0x02);

	/* T5D revA:0,revB:1, select workaround in fw to init sram */
	dvbt_t2_wrb(0x807, 0);

	/* DDR addr */
	dvbt_t2_wrb(0x360c, devp->mem_start & 0xff);
	dvbt_t2_wrb(0x360d, (devp->mem_start >> 8) & 0xff);
	dvbt_t2_wrb(0x360e, (devp->mem_start >> 16) & 0xff);
	dvbt_t2_wrb(0x360f, (devp->mem_start >> 24) & 0xff);

	dtvdemod_set_plpid(devp->plp_id);

	/* test bus addr4 */
	dvbt_t2_wr_byte_bits(0xe5, 0x1, 0, 6);
	dvbt_t2_wrb(0xf0, 0x78);
	dvbt_t2_wrb(0xf1, 0x56);
	dvbt_t2_wrb(0xf2, 0x34);
	dvbt_t2_wrb(0xf3, 0x12);
}

const unsigned short index_bw_1[] = {0, 0, 1, 1, 1, 1, 2, 3, 4};
const unsigned int iir_tab_coef1[28][6] = {
	/* BW=1.7  BW=5   BW=6   BW=7   BW=8 */
	{0x10D0, 0x110B, 0x11F8, 0x12CA, 0x136A},
	{0xDEA2, 0xDF81, 0xDE62, 0xDDAF, 0xDD6C},
	{0x10D0, 0x110B, 0x11F8, 0x12CA, 0x136A},
	{0xC3DD, 0xC967, 0xCB21, 0xCCEB, 0xCE7B},
	{0x7BDF, 0x74F0, 0x7274, 0x6FB1, 0x6D1C},
	{0x1C9D, 0x2147, 0x224E, 0x2329, 0x2414},
	{0xC744, 0xC0B0, 0xC000, 0xC000, 0xC000},
	{0x1C9D, 0x2147, 0x224E, 0x2329, 0x2414},
	{0xC20C, 0xC524, 0xC5E9, 0xC6B0, 0xC759},
	{0x7D67, 0x775D, 0x7512, 0x725F, 0x6FB6},
	{0x1A00, 0x21E1, 0x22A0, 0x2394, 0x2498},
	{0xCCA2, 0xC000, 0xC000, 0xC000, 0xC000},
	{0x1A00, 0x21E1, 0x22A0, 0x2394, 0x2498},
	{0xC0D9, 0xC245, 0xC287, 0xC2C7, 0xC2FC},
	{0x7E6C, 0x7901, 0x76C6, 0x7410, 0x7151},
	{0x05B5, 0x226D, 0x23C7, 0x251C, 0x2689},
	{0xF4F7, 0xC092, 0xC000, 0xC000, 0xC000},
	{0x05B5, 0x226D, 0x23C7, 0x251C, 0x2689},
	{0xC039, 0xC0E0, 0xC0F2, 0xC103, 0xC111},
	{0x7EF9, 0x79D2, 0x7798, 0x74D9, 0x720D},
	{0x0000, 0x063F, 0x070C, 0x0804, 0x088A},
	{0x0394, 0xF60E, 0xF59E, 0xF546, 0xF5A7},
	{0x0394, 0x063F, 0x070C, 0x0804, 0x088A},
	{0x0000, 0xC039, 0xC03D, 0xC040, 0xC043},
	{0x3D7B, 0x7A42, 0x7807, 0x7544, 0x7271},
	{0x0000, 0x0957, 0x0B3C, 0x0CD9, 0x0E75},
	{0x4000, 0x0957, 0x0B3C, 0x0CD9, 0x0E75},
	{0x0000, 0x36c6, 0x3875, 0x370A, 0x35C2}
};

struct st_chip_register_t reset368dvbt_val[] =	 /*init minimum setting STV368+TDA18212 RF=474MHz*/
{
	{R368TER_P1AGC_TARG, 0x64 },/* P1AGC_TARG */
	{R368TER_CAS_CONF1, 0x94 },/* CAS_CONF1 */
	{R368TER_CAS_CCSMU, 0x33 },/* CAS_CCSMU */
	{R368TER_CAS_CCDCCI, 0x7f },/* CAS_CCDCCI */
	{R368TER_CAS_CCDNOCCI, 0x1a },/* CAS_CCDNOCCI */
	/* BZ49942:This is to improve the performances of 2kFFT mode in presence of Rayleigh echo */
	{R368TER_FFT_FACTOR_2K_S2, 0x02 },
	{R368TER_FFT_FACTOR_8K_S2, 0x02 },
	{R368TER_P1_CTRL, 0x20 },/* P1_CTRL */
	{R368TER_P1_DIV, 0xc0 },/* P1_DIV */
	{R368TER_DVBT_CTRL, 0x20 },/* DVBT_CTRL */
	{R368TER_SCENARII_CFG, 0x0a },/* SCENARII_CFG */
	{R368TER_SYMBOL_TEMPO, 0x08 },/* SYMBOL_TEMPO */
	{R368TER_CHC_TI0, 0xb0 },/* CHC_TI0 */
	{R368TER_CHC_TRIG0, 0x58 },/* CHC_TRIG0 */
	{R368TER_CHC_TRIG1, 0x44 },/* CHC_TRIG1 */
	{R368TER_CHC_TRIG2, 0x70 },/* CHC_TRIG2 */
	{R368TER_CHC_TRIG3, 0x88 },/* CHC_TRIG2 */
	{R368TER_CHC_TRIG4, 0x40 },/* CHC_TRIG4 */
	{R368TER_CHC_TRIG5, 0x8b },/* CHC_TRIG5 */
	{R368TER_CHC_TRIG6, 0x89 },/* CHC_TRIG6 */
	{R368TER_CHC_SNR10, 0xc9 },/* CHC_SNR10 */
	{R368TER_CHC_SNR11, 0x0c },  /* CHC_SNR11 */
	{R368TER_NSCALE_DVBT_0, 0x64 },/* NSCALE_DVBT_0 */
	{R368TER_NSCALE_DVBT_1, 0x8c },/* NSCALE_DVBT_1 */
	{R368TER_NSCALE_DVBT_2, 0xc8 },/* NSCALE_DVBT_2 */
	{R368TER_NSCALE_DVBT_3, 0x78 },/* NSCALE_DVBT_3 */
	{R368TER_NSCALE_DVBT_4, 0x75 },/* NSCALE_DVBT_4 */
	{R368TER_NSCALE_DVBT_5, 0xaa },/* NSCALE_DVBT_5 */
	{R368TER_NSCALE_DVBT_6, 0xaa },/* NSCALE_DVBT_6 */
	{R368TER_IDM_RD_DVBT_1, 0x55 },/* IDM_RD_DVBT_1 */
	{R368TER_IDM_RD_DVBT_2, 0x15 } /* IDM_RD_DVBT_2 */
};

void write_riscv_ram(void)
{
	unsigned int ck0;
	unsigned int addr = 0;
	int value;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0xa0);

	for (ck0 = 0; ck0 < (10240 + 1024) * 4; ck0 += 4) {
		value = (devp->fw_buf[ck0 + 3] << 24) | (devp->fw_buf[ck0 + 2] << 16) |
			 (devp->fw_buf[ck0 + 1] << 8) | devp->fw_buf[ck0];
		dvbt_t2_write_w(addr, value);
		addr += 4;
	}

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0xb0);
	addr = 0;

	for (ck0 = 0; ck0 < 5120 * 4; ck0 += 4)	{
		value = (devp->fw_buf[ck0 + 3] << 24) | (devp->fw_buf[ck0 + 2] << 16) |
			 (devp->fw_buf[ck0 + 1] << 8) | devp->fw_buf[ck0];
		dvbt_t2_write_w(addr, value);
		addr += 4;
	}

	PR_DBGL("write_riscv_ram:\n");
	for (ck0 = 0; ck0 < 100 * 4; ck0 += 4) {
		value = (devp->fw_buf[ck0 + 3] << 24) | (devp->fw_buf[ck0 + 2] << 16) |
			 (devp->fw_buf[ck0 + 1] << 8) | devp->fw_buf[ck0];
		PR_DBGL("[%d] = 0x%x\n", ck0, value);
	}

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
}

static void dvbt2_riscv_init(enum fe_bandwidth bw)
{
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
	dvbt2_init();

	switch (bw) {
	case BANDWIDTH_8_MHZ:
		dvbt_t2_wrb(0x1c, 0x8);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;

	case BANDWIDTH_7_MHZ:
		dvbt_t2_wrb(0x1c, 0x7);
		dvbt_t2_wrb(0x2835, 0x07);
		break;

	default:
		dvbt_t2_wrb(0x1c, 0x8);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;
	}
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
	riscv_ctl_write_reg(0x30, 0);
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
}

void dtvdemod_reset_fw(void)
{
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
	riscv_ctl_write_reg(0x30, 4);
	/* spare write for delay */
	riscv_ctl_write_reg(0x30, 4);
	riscv_ctl_write_reg(0x30, 4);
	riscv_ctl_write_reg(0x30, 0);
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
}

void dvbt_reg_initial(unsigned int bw)
{
	/* bandwidth: 1=8M,2=7M,3=6M,4=5M,5=1.7M */
	int bw_cov, i;
	enum channel_bw_e bandwidth;
	unsigned int s_r, if_tuner_freq, tmp;

	/* 24M */
	s_r = 54;
	if_tuner_freq = 49;

	switch (bw) {
	case BANDWIDTH_8_MHZ:
		bandwidth = CHAN_BW_8M;
		break;

	case BANDWIDTH_7_MHZ:
		bandwidth = CHAN_BW_7M;
		break;

	case BANDWIDTH_6_MHZ:
		bandwidth = CHAN_BW_6M;
		break;

	case BANDWIDTH_5_MHZ:
		bandwidth = CHAN_BW_5M;
		break;

	case BANDWIDTH_1_712_MHZ:
		bandwidth = CHAN_BW_1M7;
		break;

	case BANDWIDTH_AUTO:
	case BANDWIDTH_10_MHZ:
	default:
		bandwidth = CHAN_BW_8M;
		break;
	}
	PR_DVBT("dvbt_reg_initial, bandwidth : %d\n", bandwidth);

	dvbt_t2_wrb(0x0004, 0x00);
	dvbt_t2_wrb(0x0005, 0x00);
	dvbt_t2_wrb(0x0006, 0x00);
	dvbt_t2_wrb(0x0007, 0x00);
	dvbt_t2_wrb(0x0008, 0x00);
	dvbt_t2_wrb(0x0009, 0x00);
	dvbt_t2_wrb(0x000a, 0x00);
	dvbt_t2_wrb(0x19, 0x32);
	dvbt_t2_wrb(0x824, 0xa0);
	dvbt_t2_wrb(0x825, 0x70);
	dvbt_t2_wrb(0x827, 0x00);
	dvbt_t2_wrb(0x841, 0x08);
	dvbt_t2_wrb(0x1590, 0x80);
	dvbt_t2_wrb(0x1593, 0x80);
	dvbt_t2_wrb(0x1594, 0x00);
	dvbt_t2_wrb(0x15b0, 0x55);
	dvbt_t2_wrb(0x15b1, 0x35);
	dvbt_t2_wrb(0x15b2, 0x30);
	dvbt_t2_wrb(0x15b3, 0x0c);
	dvbt_t2_wrb(0x2830, 0x20);
	dvbt_t2_wrb(0x2890, 0xc3);
	dvbt_t2_wrb(0x2891, 0x30);
	dvbt_t2_wrb(0x2892, 0x0c);
	dvbt_t2_wrb(0x2893, 0x03);
	dvbt_t2_wrb(0x2900, 0x00);
	dvbt_t2_wrb(0x2900, 0x20);

	if (bandwidth == CHAN_BW_1M7) {
		dvbt_t2_wrb(0x15a0, (dvbt_t2_rdb(0x15a0) & 0xef) | (1 << 4));
		dvbt_t2_wrb(0x15b3, (dvbt_t2_rdb(0x15b3) & 0xcf) | (1 << 4));
	} else {
		dvbt_t2_wrb(0x15a0, (dvbt_t2_rdb(0x15a0) & 0xef));
		dvbt_t2_wrb(0x15b3, (dvbt_t2_rdb(0x15b3) & 0xcf));
	}

	setsrcgain(calcul_src_gain_m(bandwidth, s_r));
	settrl(calcul_tr_inominal_rate_m(bandwidth, s_r));

	tmp = (((s_r - if_tuner_freq) * 65536) * 10) / s_r;
	tmp /= 10;

	/* if ((tmp % 10) > 4) {
	 *	tmp /= 10;
	 *	tmp += 1;
	 * } else {
	 *	tmp /= 10;
	 * }
	 */

	/* 15b0 17b4 */
	dvbt_t2_wrb(0x15b0, (tmp) & 0xff);
	dvbt_t2_wrb(0x15b1, (tmp >> 8) & 0xff);
	bw_cov = index_bw_1[bandwidth];

	dvbt_t2_wrb(0x1504, iir_tab_coef1[0][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1505, (iir_tab_coef1[0][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1506, iir_tab_coef1[1][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1507, (iir_tab_coef1[1][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1508, iir_tab_coef1[2][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1509, (iir_tab_coef1[2][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x150a, iir_tab_coef1[3][bw_cov] & 0xff);
	dvbt_t2_wrb(0x150b, (iir_tab_coef1[3][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x150c, iir_tab_coef1[4][bw_cov] & 0xff);
	dvbt_t2_wrb(0x150d, (iir_tab_coef1[4][bw_cov] & 0xff00) >> 8);

	dvbt_t2_wrb(0x1510, iir_tab_coef1[5][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1511, (iir_tab_coef1[5][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1512, iir_tab_coef1[6][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1513, (iir_tab_coef1[6][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1514, iir_tab_coef1[7][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1515, (iir_tab_coef1[7][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1516, iir_tab_coef1[8][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1517, (iir_tab_coef1[8][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1518, iir_tab_coef1[9][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1519, (iir_tab_coef1[9][bw_cov] & 0xff00) >> 8);

	dvbt_t2_wrb(0x1520, iir_tab_coef1[10][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1521, (iir_tab_coef1[10][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1522, iir_tab_coef1[11][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1523, (iir_tab_coef1[11][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1524, iir_tab_coef1[12][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1525, (iir_tab_coef1[12][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1526, iir_tab_coef1[13][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1527, (iir_tab_coef1[13][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1528, iir_tab_coef1[14][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1529, (iir_tab_coef1[14][bw_cov] & 0xff00) >> 8);

	dvbt_t2_wrb(0x1530, iir_tab_coef1[15][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1531, (iir_tab_coef1[15][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1532, iir_tab_coef1[16][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1533, (iir_tab_coef1[16][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1534, iir_tab_coef1[17][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1535, (iir_tab_coef1[17][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1536, iir_tab_coef1[18][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1537, (iir_tab_coef1[18][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1538, iir_tab_coef1[19][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1539, (iir_tab_coef1[19][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1540, iir_tab_coef1[20][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1541, (iir_tab_coef1[20][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1542, iir_tab_coef1[21][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1543, (iir_tab_coef1[21][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1544, iir_tab_coef1[22][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1545, (iir_tab_coef1[22][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1546, iir_tab_coef1[23][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1547, (iir_tab_coef1[23][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1548, iir_tab_coef1[24][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1549, (iir_tab_coef1[24][bw_cov] & 0xff00) >> 8);

	dvbt_t2_wrb(0x1550, iir_tab_coef1[25][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1551, (iir_tab_coef1[25][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1552, iir_tab_coef1[26][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1553, (iir_tab_coef1[26][bw_cov] & 0xff00) >> 8);
	dvbt_t2_wrb(0x1554, iir_tab_coef1[27][bw_cov] & 0xff);
	dvbt_t2_wrb(0x1555, (iir_tab_coef1[27][bw_cov] & 0xff00) >> 8);

	dvbt_t2_wrb(0x1500, 0x20);
	dvbt_t2_wrb(0x1500, 0x00);
	dvbt_t2_wrb(0x598, 0x91);
	dvbt_t2_wrb(0x5a0, 0x0);
	dvbt_t2_wrb(0x5a1, 0x0);
	dvbt_t2_wrb(0x5a2, 0x0);
	dvbt_t2_wrb(0x5a3, 0x0);
	dvbt_t2_wrb(0x5a4, 0x0);
	dvbt_t2_wrb(0x5a5, 0x0);
	dvbt_t2_wrb(0x5a6, 0x0);

	for (i = 0; i < (sizeof(reset368dvbt_val) / sizeof(struct st_chip_register_t)); i++)
		dvbt_t2_wrb(reset368dvbt_val[i].addr, reset368dvbt_val[i].value);

	PR_DVBT("DVB-T init ok.\n");
}

unsigned int dvbt_set_ch(struct aml_demod_sta *demod_sta, struct aml_demod_dvbt *demod_dvbt)
{
	int ret = 0;
	u8_t demod_mode = 1;
	u8_t bw, sr, ifreq, agc_mode;
	u32_t ch_freq;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp))
		return -1;

	bw = demod_dvbt->bw;
	sr = demod_dvbt->sr;
	ifreq = demod_dvbt->ifreq;
	agc_mode = demod_dvbt->agc_mode;
	ch_freq = demod_dvbt->ch_freq;
	demod_mode = demod_dvbt->dat0;
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

	demod_sta->ch_mode = 0;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->ch_bw = (8 - bw) * 1000;
	demod_sta->symb_rate = 0;

	demod_mode = 1;
	/* for si2176 IF:5M   sr 28.57 */
	sr = 4;
	ifreq = 4;
	PR_INFO("%s:1:bw=%d, demod_mode=%d\n", __func__, bw, demod_mode);

	/*bw = BANDWIDTH_AUTO;*/
	if (bw == BANDWIDTH_AUTO)
		demod_mode = 2;

	devp->bw = bw;
	dvbt_reg_initial(bw);
	PR_DVBT("DVBT mode\n");

	return ret;
}

int dvbt2_set_ch(struct amldtvdemod_device_s *devp)
{
	int ret = 0;

	dvbt2_riscv_init(devp->bw);

	return ret;
}

unsigned int pow2[32] = {
	(1) << 0, (1) << 1, (1) << 2, (1) << 3,
	(1) << 4, (1) << 5, (1) << 7, (1) << 8,
	(1) << 8, (1) << 9, (1) << 10, (1) << 11,
	(1) << 12, (1) << 13, (1) << 14, (1) << 15,
	(1) << 16, (1) << 17, (1) << 18, (1) << 19,
	(1) << 20, (1) << 21, (1) << 22, (1) << 23,
	(1) << 24, (1) << 25, (1) << 26, (1) << 27,
	(1) << 28, (1) << 29, (1) << 30, (1) << 31
};

unsigned int dtvdemod_calcul_get_field(unsigned int memory_base, unsigned int nb_bits_shift,
					unsigned int var_size)
{
	unsigned int nb_bits;
	unsigned int nb_W32;
	unsigned int result;
	unsigned int right_bits;
	unsigned int temp_addr;
	unsigned int temp_val;
	unsigned int reg_rd_o;
	unsigned int var_size_use;

	/* nombre de mots de 32 bits a avancer */
	nb_W32 = (nb_bits_shift >> 5);
	/* just in case*/
	nb_W32 = (nb_W32 & 0x7ffffff);
	/* nombre de bits a avancer */
	nb_bits = (nb_bits_shift & 0x1F);
	temp_addr = memory_base;
	temp_addr += (nb_W32 * 4);
	var_size_use = (var_size >= 32) ? 0xffffffff : ((int)pow2[var_size] - 1);
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x184);

	if ((nb_bits + var_size) <= 32)	{
		//read_register(temp_addr, &reg_rd_o);
		reg_rd_o = dvbt_t2_read_w(temp_addr);
		result = (reg_rd_o << nb_bits);
		result = (result >> (32 - var_size)); /* remove left bits */
		result = (result & var_size_use);
	} else {
		//read_register(temp_addr, &reg_rd_o);
		reg_rd_o = dvbt_t2_read_w(temp_addr);
		temp_val = reg_rd_o;
		temp_addr += 4;
		//read_register(temp_addr, &reg_rd_o);
		reg_rd_o = dvbt_t2_read_w(temp_addr);
		right_bits = (64 - nb_bits - var_size);
		result = (reg_rd_o >> right_bits);  /* si right_bits == 32 pas de shift */
		result = (right_bits <= 0) ? result : (result & ((int)pow2[32 - right_bits] - 1));
		/* remove left bits */
		temp_val = (temp_val << nb_bits);
		temp_val = (temp_val >> nb_bits);

		/* on laisser la place aux bits de l'autre mot */
		if ((right_bits > 0) && (right_bits < 32))
			temp_val = (temp_val << (32 - right_bits));
		else
			PR_ERR("shift err, %d\n", right_bits);

		result = (result | temp_val);
		result = (result & var_size_use);
	}

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
	return result;
}

void dtvdemod_get_plp(struct dtv_property *tvp)
{
	/* unsigned int miso_mode; */
	unsigned int plp_num;
	/* unsigned int pos; */
	unsigned int i;
	char plp_ids[MAX_PLP_NUM];
	/* char plp_type[MAX_PLP_NUM]; */
	unsigned int val;

	/* val[30-24]:0x805, plp num */
	val = front_read_reg(0x3e);
	plp_num = (val >> 24) & 0x7f;

	/* miso_mode = (dvbt_t2_rdb(0x871) >> 4) & 0x7; */
	/* plp_num = dvbt_t2_rdb(0x805); */
	/* PR_INFO("plp num: %d, miso mode: 0x%x\n", plp_num, miso_mode); */
	PR_INFO("plp num: %d\n", plp_num);

	/* if (miso_mode)
	 *	pos = 70 + 4 + 22 + 8;
	 * else
	 *	pos = 70;
	 */

	tvp->u.buffer.reserved1[0] = plp_num;

	for (i = 0; i < plp_num; i++) {
		/* plp_ids[i] = dtvdemod_calcul_get_field(0, pos, 8); */
		plp_ids[i] = i;
		/* plp_type[i] = (enum plp_type_e)dtvdemod_calcul_get_field(0, pos + 8, 3);
		 * pos += 89;
		 * PR_INFO("plp id: %d, type : %d\n", plp_ids[i], plp_type[i]);
		 */
	}

	if (copy_to_user(tvp->u.buffer.reserved2, plp_ids, plp_num * sizeof(char)))
		pr_err("copy plp ids to user err\n");
}

void dtvdemod_get_plp_dbg(void)
{
	unsigned int miso_mode;
	unsigned int plp_num;
	unsigned int pos;
	unsigned int i;
	char plp_ids[MAX_PLP_NUM];
	char plp_type[MAX_PLP_NUM];

	miso_mode = (dvbt_t2_rdb(0x871) >> 4) & 0x7;
	plp_num = dvbt_t2_rdb(0x805);
	PR_INFO("plp num: %d, miso mode: 0x%x\n", plp_num, miso_mode);

	if (1 == (miso_mode & 1))
		pos = 70 + 4 + 22 + 8;
	else
		pos = 70;

	for (i = 0; i < plp_num; i++) {
		plp_ids[i] = dtvdemod_calcul_get_field(0, pos, 8);
		plp_type[i] = (enum plp_type_e)dtvdemod_calcul_get_field(0, pos + 8, 3);
		pos += 89;
		PR_INFO("plp id: %d, type : %d\n", plp_ids[i], plp_type[i]);
	}
}

void dtvdemod_set_plpid(char id)
{
	dvbt_t2_wrb(0x806, id);
	PR_INFO("dtvdemod_set_plpid : %d\n", id);
}

static int m_calcul_carrier_offset(int crl_in, enum channel_bw_e bw)
{
	int crl = 0;
	unsigned int freq = 0;

	switch (bw) {
	case 1:
		freq = (131 * 1000000) / 71;
		break;

	case 5:
		freq = (40 * 1000000) / 7;
		break;

	case 6:
		freq = (48 * 1000000) / 7;
		break;

	case 7:
		freq = 8 * 1000000;
		break;

	case 8:
	default:
		freq = (64 * 1000000) / 7;
		break;
	}

	crl = ((int)crl_in * freq) / 262144;

	return crl;
}

static int m_get_carrier_offset(void)
{
	int crl_freq_status;
	int carrier_offset;
	unsigned int crl_freq_stat;
	int bw_value = dvbt_t2_rdb(0x1c) & 0xf;

	crl_freq_stat = dvbt_t2_rdb(0x28cc) + (dvbt_t2_rdb(0x28cd) << 8);

	if (crl_freq_stat & 0x8000)
		crl_freq_status = -(crl_freq_stat ^ 0xFFFF) - 1;
	else
		crl_freq_status = crl_freq_stat;

	carrier_offset = m_calcul_carrier_offset(crl_freq_status, bw_value);

	return carrier_offset;
}

void dvbt2_info(void)
{
	/* SNR */
	unsigned int c_snr = dvbt_t2_rdb(0x2a08) + ((dvbt_t2_rdb(0x2a09)) << 8);
	unsigned int f_snr = dvbt_t2_rdb(0x2a5c) + ((dvbt_t2_rdb(0x2a5d)) << 8);
	unsigned int d_snr = dvbt_t2_rdb(0xabc) + ((dvbt_t2_rdb(0xabd)) << 8);

	/* ts type */
	unsigned int ts_type = dvbt_t2_rdb(0x870) & 0x3;

	/* GI Detection 0x872 */
	unsigned int gi_st0 = dvbt_t2_rdb(0x2745);
	unsigned int gi = dvbt_t2_rdb(0x83a) & 0x7;
	unsigned int fft = gi_st0 & 0x7;
	unsigned int miso_mode = (dvbt_t2_rdb(0x871) >> 4) & 0x7;

	/* L1 Post */
	unsigned int l1_cstl = (dvbt_t2_rdb(0x873) >> 2) & 0xf;

	/* Pilot Mode */
	unsigned int pp_mode = (dvbt_t2_rdb(0x876) >> 2) & 0xf;

	/* CFO */
	unsigned int cfo = m_get_carrier_offset();

	/* ldpc status */
	unsigned int ldpc_it = dvbt_t2_rdb(0xa50);
	unsigned int bch = dvbt_t2_rdb(0xab8) + (dvbt_t2_rdb(0xab9) << 8);
	unsigned int data_err = dvbt_t2_rdb(0xab0) + (dvbt_t2_rdb(0xab1) << 8);
	unsigned int data_ttl = dvbt_t2_rdb(0xab2) + (dvbt_t2_rdb(0xab3) << 8);
	unsigned int cmmn_err = dvbt_t2_rdb(0xab4) + (dvbt_t2_rdb(0xab5) << 8);
	unsigned int cmmn_ttl = dvbt_t2_rdb(0xab6) + (dvbt_t2_rdb(0xab7) << 8);
	unsigned int constel = (dvbt_t2_rdb(0x8c3) >> 4) & 7;
	char *str_ts_type, *str_fft, *str_constel, *str_miso_mode, *str_gi, *str_l1_cstl;
	char *str_pp_mode;

	/* SFO */
	unsigned int sfo = dvbt_t2_rdb(0x28f8) | ((dvbt_t2_rdb(0x28f9) & 7) << 8);

	sfo = ((sfo > 16383) ? (sfo - 32768) : sfo) / 16;
	c_snr = c_snr * 3 / 64;
	f_snr = f_snr * 3 / 64;
	d_snr = d_snr * 3 / 64;

	switch (ts_type) {
	case 0:
		str_ts_type = "T ";
		break;

	case 1:
		str_ts_type = "G ";
		break;

	case 2:
		str_ts_type = "T&G ";
		break;

	default:
		str_ts_type = "UNK ";
		break;
	}

	if (fft == 0)
		str_fft = "2K	 ";
	else if (fft == 1)
		str_fft = "8K	 ";
	else if (fft == 2)
		str_fft = "4K	 ";
	else if (fft == 3)
		str_fft = "1K	 ";
	else if (fft == 4)
		str_fft = "16K  ";
	else if (fft == 5)
		str_fft = "32K  ";
	else if (fft == 6)
		str_fft = "8KE  ";
	else
		str_fft = "32KE ";

	if (constel == 0)
		str_constel = "QPSK  ";
	else if (constel == 1)
		str_constel = "16QAM ";
	else if (constel == 2)
		str_constel = "64QAM ";
	else if (constel == 3)
		str_constel = "256QAM ";
	else
		str_constel = "Err   ";

	switch (miso_mode) {
	case 0:
		str_miso_mode = "SISO ";
		break;

	case 1:
		str_miso_mode = "MISO ";
		break;

	case 2:
		str_miso_mode = "NoT2 ";
		break;

	default:
		str_miso_mode = "undef ";
		break;
	}

	/* GID Print */
	if (gi == 0)
		str_gi = "1/32   ";
	else if (gi == 1)
		str_gi = "1/16   ";
	else if (gi == 2)
		str_gi = "1/8    ";
	else if (gi == 3)
		str_gi = "1/4    ";
	else if (gi == 4)
		str_gi = "1/128  ";
	else if (gi == 5)
		str_gi = "19/128 ";
	else if (gi == 6)
		str_gi = "19/256 ";
	else
		str_gi = "NF	   ";

	/* L1 Constellation */
	if (l1_cstl == 0)
		str_l1_cstl = "BPSK  ";
	else if (l1_cstl == 1)
		str_l1_cstl = "QPSK  ";
	else if (l1_cstl == 2)
		str_l1_cstl = "16QAM ";
	else if (l1_cstl == 3)
		str_l1_cstl = "64QAM ";
	else
		str_l1_cstl = "Ukn   ";

	/* Pilot Mode */
	switch (pp_mode) {
	case 0:
		str_pp_mode = "PP1 ";
		break;
	case 1:
		str_pp_mode = "PP2 ";
		break;
	case 2:
		str_pp_mode = "PP3 ";
		break;
	case 3:
		str_pp_mode = "PP4 ";
		break;
	case 4:
		str_pp_mode = "PP5 ";
		break;
	case 5:
		str_pp_mode = "PP6 ";
		break;
	case 6:
		str_pp_mode = "PP7 ";
		break;
	case 7:
		str_pp_mode = "PP8 ";
		break;
	default:
		str_pp_mode = "Ukn ";
		break;
	}

	PR_DVBT("SNR c%ddB, F%d,D%d %s FFT %s %s %s, GI %s, L1 %s, PP_md %s, CFO %dHz, SFO %dppm\n",
		c_snr, f_snr, d_snr, str_ts_type, str_fft,
		str_constel, str_miso_mode, str_gi, str_l1_cstl, str_pp_mode, cfo, sfo);
	PR_DVBT("LDPC %2d,bch 0x%x, D 0x%x/0x%x C 0x%x/0x%x\n",
		ldpc_it, bch, data_err, data_ttl, cmmn_err, cmmn_ttl);
	PR_DVBT("COM:%x,AUT:%x,GI:%x,PRE:%x,POST:%x,P1G:%2x,CASG:%2x, 0x361b:0x%x\n\n",
		(dvbt_t2_rdb(0x1579) >> 6) & 0x01, (dvbt_t2_rdb(0x1579) >> 5) & 0x01,
		(dvbt_t2_rdb(0x2745) >> 5) & 0x01, (dvbt_t2_rdb(0x839) >> 4) & 0x01,
		(dvbt_t2_rdb(0x839) >> 3) & 0x01, dvbt_t2_rdb(0x15ba), dvbt_t2_rdb(0x15d5),
		dvbt_t2_rdb(0x361b));
}

