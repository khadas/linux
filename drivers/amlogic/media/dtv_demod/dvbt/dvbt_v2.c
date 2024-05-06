// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

 /* Enables DVBv3 compatibility bits at the headers */
#define __DVB_CORE__	/*ary 2018-1-31*/

#include "demod_func.h"
#include "aml_demod.h"
#include <linux/string.h>
#include <linux/kernel.h>
//#include "acf_filter_coefficient.h"
#include <linux/mutex.h>
#include "dvbt_func.h"

MODULE_PARM_DESC(dvbt2_agc_target1, "");
static unsigned char dvbt2_agc_target1 = 0x60;
module_param(dvbt2_agc_target1, byte, 0644);

MODULE_PARM_DESC(dvbt2_agc_target2, "");
static unsigned char dvbt2_agc_target2 = 0x11;
module_param(dvbt2_agc_target2, byte, 0644);

/* copy from dvbt_isdbt_set_ch*/
int dvbt_dvbt_set_ch(struct aml_dtvdemod *demod,
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

static void setsrcgain(unsigned int val)
{
	dvbt_t2_wrb(0x15b2, val & 0xff);
	dvbt_t2_wrb(0x15b3, (dvbt_t2_rdb(0x15b3) & 0xf0) | ((val & 0xf00) >> 8));
}

static unsigned int calcul_src_gain_m(int bw, int s_r)
{
	unsigned int src_gain = 0;

//	src_gain = (4096 * 4 * 64 * bw) / (7 * 8 * s_r * 2);
	src_gain = ((1 << 20) * bw) / (56 * s_r);

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

const unsigned int minimum_snr_x10[4][6] = {
	{4, 16, 25, 35, 41, 46},	/* QPSK */
	{56, 70, 84, 95, 103, 108},	/* QAM16 */
	{100, 118, 131, 146, 157, 163},	/* QAM64 */
	{139, 163, 177, 197, 212, 219}	/* QAM256 */
};

void calculate_cordic_para(void)
{
	dvbt_isdbt_wr_reg(0x0c, 0x00000040);
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

void dvbt2_init(struct aml_dtvdemod *demod, struct dvb_frontend *fe)
{
	/* bandwidth: 1=8M,2=7M,3=6M,4=5M,5=1.7M */
	int i;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

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
	dvbt_t2_wrb(0x9, 0x60);

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
	/* fepath_gain 0x20 */
	dvbt_t2_wrb(0x15e0, 0x09);
	dvbt_t2_wrb(0x2754, 0x40);
	dvbt_t2_wrb(0x80c, 0xff);
	dvbt_t2_wrb(0x80d, 0xff);
	dvbt_t2_wrb(0x80e, 0xff);
	dvbt_t2_wrb(0x80f, 0xff);
	dvbt_t2_wrb(0x01a, 0x14);

	if (tuner_find_by_name(fe, "r842")) {
		PR_INFO("set r842 dvbt2 config\n");
		dvbt_t2_wrb(0x821, 0x70);
		dvbt_t2_wrb(0x824, dvbt2_agc_target1);
		dvbt_t2_wrb(0x825, dvbt2_agc_target2);
		dvbt_t2_wrb(0x827, 0x50);
	} else {
		PR_INFO("set default dvbt2 config\n");
	}

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
	dvbt_t2_wrb(0x807, 1);

	/* T2-MI ts unlock issue
	 * immediate output of every packet or every frame as soon as they are ready
	 */
	dvbt_t2_wr_byte_bits(0x570, 1, 0, 1);

	/* DDR addr */
	dvbt_t2_wrb(0x360c, devp->mem_start & 0xff);
	dvbt_t2_wrb(0x360d, (devp->mem_start >> 8) & 0xff);
	dvbt_t2_wrb(0x360e, (devp->mem_start >> 16) & 0xff);
	dvbt_t2_wrb(0x360f, (devp->mem_start >> 24) & 0xff);

	dtvdemod_set_plpid(demod->plp_id);

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
//	{R368TER_P1AGC_TARG, 0x64 },/* P1AGC_TARG */
//	{R368TER_CAS_CONF1, 0x94 },/* CAS_CONF1 */
//	{R368TER_CAS_CCSMU, 0x33 },/* CAS_CCSMU */
//	{R368TER_CAS_CCDCCI, 0x7f },/* CAS_CCDCCI */
//	{R368TER_CAS_CCDNOCCI, 0x1a },/* CAS_CCDNOCCI */
//	/* BZ49942:This is to improve the performances of 2kFFT mode in presence of Rayleigh echo */
//	{R368TER_FFT_FACTOR_2K_S2, 0x02 },
//	{R368TER_FFT_FACTOR_8K_S2, 0x02 },
//	{R368TER_P1_CTRL, 0x20 },/* P1_CTRL */
//	{R368TER_P1_DIV, 0xc0 },/* P1_DIV */
//	{R368TER_DVBT_CTRL, 0x20 },/* DVBT_CTRL */
//	{R368TER_SCENARII_CFG, 0x0a },/* SCENARII_CFG */
//	{R368TER_SYMBOL_TEMPO, 0x08 },/* SYMBOL_TEMPO */
//	{R368TER_CHC_TI0, 0xb0 },/* CHC_TI0 */
//	{R368TER_CHC_TRIG0, 0x58 },/* CHC_TRIG0 */
//	{R368TER_CHC_TRIG1, 0x44 },/* CHC_TRIG1 */
//	{R368TER_CHC_TRIG2, 0x70 },/* CHC_TRIG2 */
//	{R368TER_CHC_TRIG3, 0x88 },/* CHC_TRIG2 */
//	{R368TER_CHC_TRIG4, 0x40 },/* CHC_TRIG4 */
//	{R368TER_CHC_TRIG5, 0x8b },/* CHC_TRIG5 */
//	{R368TER_CHC_TRIG6, 0x89 },/* CHC_TRIG6 */
//	{R368TER_CHC_SNR10, 0xc9 },/* CHC_SNR10 */
//	{R368TER_CHC_SNR11, 0x0c },  /* CHC_SNR11 */
//	{R368TER_NSCALE_DVBT_0, 0x64 },/* NSCALE_DVBT_0 */
//	{R368TER_NSCALE_DVBT_1, 0x8c },/* NSCALE_DVBT_1 */
//	{R368TER_NSCALE_DVBT_2, 0xc8 },/* NSCALE_DVBT_2 */
//	{R368TER_NSCALE_DVBT_3, 0x78 },/* NSCALE_DVBT_3 */
//	{R368TER_NSCALE_DVBT_4, 0x75 },/* NSCALE_DVBT_4 */
//	{R368TER_NSCALE_DVBT_5, 0xaa },/* NSCALE_DVBT_5 */
//	{R368TER_NSCALE_DVBT_6, 0xaa },/* NSCALE_DVBT_6 */
//	{R368TER_IDM_RD_DVBT_1, 0x55 },/* IDM_RD_DVBT_1 */
//	{R368TER_IDM_RD_DVBT_2, 0x15 } /* IDM_RD_DVBT_2 */

	{R368TER_IQFE_AGC_CONF0, 0x48 },/* IQFE_AGC_CONF0 */
	{R368TER_IQFE_AGC_CONF1, 0x8C },/* IQFE_AGC_CONF0 */
	{R368TER_IQFE_AGC_CONF2, 0x60 },/* IQFE_AGC_CONF2 */
	{R368TER_IQFE_AGC_CONF3, 0x76 },/* IQFE_AGC_CONF3 */
	{R368TER_IQFE_AGC_CONF4, 0x38 },/* IQFE_AGC_CONF4 */
	{R368TER_AGC_TARGETI0, 0xbe },/* AGC_TARGETI0 */
	{R368TER_AGC_TARGETI1, 0x00 },/* AGC_TARGETI0 */
	{R368TER_AGC_TARGETQ0, 0xbe },/* AGC_TARGETQ0 */
	{R368TER_AGC_TARGETQ1, 0x00 },/* AGC_TARGETQ0 */
	{R368TER_AGC_CONF5, 0x66 },/* AGC_CONF5 */
	{R368TER_AGC_CONF6, 0x70 },/* AGC_CONF6 */
	{R368TER_LOCK_DET1_0, 0x20 },/* LOCK_DET1_0 */
	{R368TER_LOCK_DET1_1, 0x00 },/* LOCK_DET1_1 */
	{R368TER_LOCK_DET2_0, 0x20 },/* LOCK_DET2_0 */
	{R368TER_LOCK_DET2_1, 0x00 },/* LOCK_DET2_1 */
	{R368TER_LOCK_DET3_0, 0x20 },/* LOCK_DET3_0 */
	{R368TER_LOCK_DET3_1, 0x00 },/* LOCK_DET3_1 */
	{R368TER_LOCK_DET4_0, 0x20 },/* LOCK_DET4_0 */
	{R368TER_LOCK_DET4_1, 0x00 },/* LOCK_DET4_1 */
	{R368TER_LOCK_N, 0x50 },/* LOCK_N */
//	{R368TER_INC_CONF0, 0x05 },/* INC_is 'ON' */
//	{R368TER_INC_CONF1, 0xff },/* INC_is 'ON' */
//	{R368TER_INC_CONF2, 0x23 },/* INC_is 'ON' */
//	{R368TER_INC_CONF3, 0x0a },/* INC_is 'ON' */
//	{R368TER_INC_CONF4, 0x3f },/* INC_is 'ON' */
//	{R368TER_INC_BRSTCNT0, 0x6c },/* INC_BRSTCNT0 */
//	{R368TER_INC_BRSTCNT1, 0x0c },/* INC_BRSTCNT1 */
	{R368TER_DCCOMP, 0x03 },/* DCCOMP */
//	{R368TER_SRC_CONF1, 0x1a },/* SRC_CONF1 */
	{R368TER_P1AGC_TARG, 0x64 },/* P1AGC_TARG */
	{R368TER_CAS_CONF1, 0x94 },/* CAS_CONF1 */
	{R368TER_CAS_CCSMU, 0x33 },/* CAS_CCSMU */
	{R368TER_CAS_CCDCCI, 0x7f },/* CAS_CCDCCI */
	{R368TER_CAS_CCDNOCCI, 0x1a },/* CAS_CCDNOCCI */
	{R368TER_FFT_FACTOR_2K_S2, 0x02 },/* FFT_FACTOR_2K_S2 */
	{R368TER_FFT_FACTOR_8K_S2, 0x02 },
	{R368TER_P1_CTRL, 0x20 },/* P1_CTRL */
	{R368TER_P1_DIV, 0xc0 },/* P1_DIV */
	{R368TER_TFO_GAIN, 0x20 },/* TFO_GAIN */
	{R368TER_TFO_GAIN_CONV, 0x80 },/* TFO_GAIN_CONV */
	{R368TER_CFO_GAIN, 0x20 },/* CFO_GAIN */
	{R368TER_CFO_GAIN_CONV, 0x0a },/* CFO_GAIN_CONV */
	{R368TER_TFO_COEFF0, 0x7f },/* TFO_COEFF0 */
	{R368TER_TFO_COEFF1, 0x00 },/* TFO_COEFF1 */
	{R368TER_TFO_COEFF2, 0x00 },/* TFO_COEFF2 */
	{R368TER_CFO_COEFF0, 0x7f },/* CFO_COEFF0 */
	{R368TER_CFO_COEFF1, 0x00 },/* CFO_COEFF1 */
	{R368TER_CFO_COEFF2, 0x00 },/* CFO_COEFF2 */
	{R368TER_DVBT_CTRL, 0x20 },/* DVBT_CTRL */
	{R368TER_CORREL_CTL, 0x11 },/* CORREL_CTL */
	{R368TER_SCENARII_CFG, 0x0a },/* SCENARII_CFG */
	{R368TER_SYMBOL_TEMPO, 0x08 },/* SYMBOL_TEMPO */
	{R368TER_CHC_TI0, 0xb0 },/* CHC_TI0 */
	{R368TER_CHC_TRIG0, 0x58 },/* CHC_TRIG0 */
	{R368TER_CHC_TRIG1, 0x44 },/* CHC_TRIG1 */
	{R368TER_CHC_TRIG2, 0x00 },/* CHC_TRIG2 */
	{R368TER_CHC_TRIG3, 0x88 },/* CHC_TRIG2 */
	{R368TER_CHC_TRIG4, 0x40 },/* CHC_TRIG4 */
	{R368TER_CHC_TRIG5, 0x8b },/* CHC_TRIG5 */
	{R368TER_CHC_TRIG6, 0x89 },/* CHC_TRIG6 */
	{R368TER_CHC_TRIG7, 0x00 },/* CHC_TRIG7 */
	{R368TER_CHC_TRIG8, 0x00 },/* CHC_TRIG8 */
	{R368TER_CHC_TRIG9, 0x00 },/* CHC_TRIG9 */
	{R368TER_CHC_SNR10, 0xc9 },/* CHC_SNR10 */
	{R368TER_CHC_SNR11, 0x0c }, /* CHC_SNR11 */
	/*PJ/DB/TA 02/2014 added for proper DVB-T operation*/
	{R368TER_DVBT_CONF, 0x0c },/* DVBT_CONF */
	{R368TER_NSCALE_DVBT_0, 0x64 },/* NSCALE_DVBT_0 */
	{R368TER_NSCALE_DVBT_1, 0x8c },/* NSCALE_DVBT_1 */
	{R368TER_NSCALE_DVBT_2, 0xc8 },/* NSCALE_DVBT_2 */
	{R368TER_NSCALE_DVBT_3, 0x78 },/* NSCALE_DVBT_3 */
	{R368TER_NSCALE_DVBT_4, 0x97 },/* NSCALE_DVBT_4 */
	{R368TER_NSCALE_DVBT_5, 0xaa },/* NSCALE_DVBT_5 */
	{R368TER_NSCALE_DVBT_6, 0xaa },/* NSCALE_DVBT_6 */
	{R368TER_IDM_RD_DVBT_1, 0x55 },/* IDM_RD_DVBT_1 */
	{R368TER_IDM_RD_DVBT_2, 0x15 } /* IDM_RD_DVBT_2 */
};

unsigned int write_riscv_ram(void)
{
	unsigned int ck0;
	unsigned int addr = 0;
	int value;
	unsigned int ret = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0xa0);

	for (ck0 = 0; ck0 < (10240 + 1024) * 4; ck0 += 4) {
		value = (devp->fw_buf[ck0 + 3] << 24) | (devp->fw_buf[ck0 + 2] << 16) |
			 (devp->fw_buf[ck0 + 1] << 8) | devp->fw_buf[ck0];
		dvbt_t2_write_w(addr, value);
		if (dvbt_t2_read_w(addr) != value) {
			PR_ERR("write fw err, addr: 0x%x, val: 0x%x, value: 0x%x\n", addr,
			       dvbt_t2_read_w(addr), value);
			return 1;
		}
		addr += 4;
	}

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0xb0);
	addr = 0;

	for (ck0 = 0; ck0 < 5120 * 4; ck0 += 4)	{
		value = (devp->fw_buf[ck0 + 3] << 24) | (devp->fw_buf[ck0 + 2] << 16) |
			 (devp->fw_buf[ck0 + 1] << 8) | devp->fw_buf[ck0];
		dvbt_t2_write_w(addr, value);
		if (dvbt_t2_read_w(addr) != value) {
			PR_ERR("write fw err, addr: 0x%x, val: 0x%x, value: 0x%x\n", addr,
			       dvbt_t2_read_w(addr), value);
			return 1;
		}
		addr += 4;
	}

	return ret;
}

static void download_fw_to_sram(struct amldtvdemod_device_s *devp)
{
	unsigned int i;

	for (i = 0; i < 10; i++) {
		if (write_riscv_ram()) {
			PR_ERR("write fw err %d\n", i);
		} else {
			devp->fw_wr_done = 1;
			PR_INFO("download fw to sram done\n");
			break;
		}
	}
}

void dvbt2_riscv_init(struct aml_dtvdemod *demod, struct dvb_frontend *fe)
{
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
	usleep_range(1000, 1001);
	demod_top_write_reg(DEMOD_TOP_REGC, 0x000010);

	demod_top_write_reg(DEMOD_TOP_REGC, 0x000011);
	usleep_range(1000, 1001);
	demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	front_write_bits(AFIFO_ADC, 0x80, AFIFO_NCO_RATE_BIT,
			 AFIFO_NCO_RATE_WID);
	front_write_reg(0x22, 0x7200a06);
	front_write_reg(0x2f, 0x0);

	/* config the address in advance to prevent DDR abnormal access */
	front_write_reg(TEST_BUS_DC_ADDR, devp->mem_start);
	front_write_reg(TEST_BUS_VLD, 0x80000000);

	if (is_meson_t3_cpu() && is_meson_rev_b())
		front_write_reg(TEST_BUS, 0x40002000);
	else
		front_write_reg(TEST_BUS, 0xc0002000);

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
	riscv_ctl_write_reg(0x30, 5);
	riscv_ctl_write_reg(0x30, 4);
	/* t2 top test bus addr */
	dvbt_t2_wr_word_bits(0x38, 0, 16, 4);

	if (!devp->fw_wr_done)
		download_fw_to_sram(devp);

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
	dvbt2_init(demod, fe);

	switch (demod->bw) {
	case BANDWIDTH_8_MHZ:
		dvbt_t2_wrb(0x1c, 0x8);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;

	case BANDWIDTH_7_MHZ:
		dvbt_t2_wrb(0x1c, 0x7);
		dvbt_t2_wrb(0x2835, 0x07);
		break;

	case BANDWIDTH_6_MHZ:
		dvbt_t2_wrb(0x1c, 0x6);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;

	case BANDWIDTH_5_MHZ:
		dvbt_t2_wrb(0x1c, 0x5);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;

	case BANDWIDTH_1_712_MHZ:
		dvbt_t2_wrb(0x1c, 0x1);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;

	default:
		/* default 8M */
		dvbt_t2_wrb(0x1c, 0x8);
		dvbt_t2_wrb(0x2835, 0x0e);
		break;
	}

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
	riscv_ctl_write_reg(0x30, 0);

	/* close ddr clock and delay 8ms them open to fix t2 unlock issue. */
	switch (devp->data->hw_ver) {
	case DTVDEMOD_HW_T5D:
	case DTVDEMOD_HW_T5D_B:
		dtvdemod_ddr_reg_write(0x148, dtvdemod_ddr_reg_read(0x148) & 0xefffffff);
		usleep_range(8000, 9000);
		dtvdemod_ddr_reg_write(0x148, dtvdemod_ddr_reg_read(0x148) | 0x10000000);
		break;

	case DTVDEMOD_HW_T3:
		dtvdemod_ddr_reg_write(0x44, dtvdemod_ddr_reg_read(0x44) & 0xffffffdf);
		dtvdemod_ddr_reg_write(0x54, dtvdemod_ddr_reg_read(0x54) & 0xffffffdf);
		usleep_range(8000, 9000);
		dtvdemod_ddr_reg_write(0x44, dtvdemod_ddr_reg_read(0x44) | 0x00000020);
		dtvdemod_ddr_reg_write(0x54, dtvdemod_ddr_reg_read(0x54) | 0x00000020);
		break;

	default:
		break;
	}

	if (demod_is_t5d_cpu(devp))
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	else
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
}

void dvbt2_reset(struct aml_dtvdemod *demod, struct dvb_frontend *fe)
{
	dvbt2_riscv_init(demod, fe);
}

void dvbt_reg_initial(unsigned int bw, struct dvb_frontend *fe)
{
	/* bandwidth: 1=8M,2=7M,3=6M,4=5M,5=1.7M */
	int bw_cov, i;
	enum channel_bw_e bandwidth;
	unsigned int s_r, if_tuner_freq, tmp;
	unsigned int temp_bw = 0;
	int temp_bw1 = 0;
	int temp_bw2 = 0;

	/* 24M */
	s_r = 54;
	if_tuner_freq = 49;
	demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
	demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
	demod_top_write_reg(DEMOD_TOP_REGC, 0x110010);
	usleep_range(1000, 1001);
	demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	front_write_bits(AFIFO_ADC, 0x80, AFIFO_NCO_RATE_BIT,
			 AFIFO_NCO_RATE_WID);
	front_write_reg(0x22, 0x7200a06);
	front_write_reg(0x2f, 0x0);
	front_write_reg(TEST_BUS, 0x40001000);
	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);

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
	PR_DVBT("%s, bandwidth: %d\n", __func__, bandwidth);

	dvbt_t2_wrb(0x0004, 0x00);
	dvbt_t2_wrb(0x0005, 0x00);
	dvbt_t2_wrb(0x0006, 0x00);
	dvbt_t2_wrb(0x0007, 0x00);
	dvbt_t2_wrb(0x0008, 0x00);
	dvbt_t2_wrb(0x0009, 0x00);
	dvbt_t2_wrb(0x000a, 0x00);
	dvbt_t2_wrb(0x19, 0x32);

	if (tuner_find_by_name(fe, "r842")) {
		PR_INFO("set r842 dvbt config\n");
		dvbt_t2_wrb(0x821, 0x70);
		dvbt_t2_wrb(0x824, 0x5e);
		dvbt_t2_wrb(0x825, 0x10);
		dvbt_t2_wrb(0x827, 0x50);
	} else {
		PR_INFO("set default dvbt config\n");
		dvbt_t2_wrb(0x824, 0xa0);
		dvbt_t2_wrb(0x825, 0x70);
		dvbt_t2_wrb(0x827, 0x00);
	}

	dvbt_t2_wrb(0x841, 0x08);
	dvbt_t2_wrb(0x1590, 0x80);
	dvbt_t2_wrb(0x1593, 0x80);
	dvbt_t2_wrb(0x1594, 0x0);
	//dvbt_t2_wrb(0x15b0, 0x55);
	//dvbt_t2_wrb(0x15b1, 0x35);
	//dvbt_t2_wrb(0x15b2, 0x30);
	//dvbt_t2_wrb(0x15b3, 0x0c);
	//dvbt_t2_wrb(0x2830, 0x20);
	//dvbt_t2_wrb(0x2890, 0xc3);
	//dvbt_t2_wrb(0x2891, 0x30);
	//dvbt_t2_wrb(0x2892, 0x0c);
	//dvbt_t2_wrb(0x2893, 0x03);
	/* increase agc target to make signal strong enough for locking */
	dvbt_t2_wrb(0x15d6, 0xa0);
	//dvbt_t2_wrb(0x2751, 0xf0);
	//dvbt_t2_wrb(0x2752, 0x3c);
	//dvbt_t2_wrb(0x2815, 0x03);
	//dvbt_t2_wr_byte_bits(0x2906, 0, 3, 4);

	//dvbt_t2_wrb(0x2900, 0x00);
	//dvbt_t2_wrb(0x2900, 0x20);

	if (bandwidth == CHAN_BW_1M7) {
		dvbt_t2_wrb(0x15a0, (dvbt_t2_rdb(0x15a0) & 0xef) | (1 << 4));
		dvbt_t2_wrb(0x15b3, (dvbt_t2_rdb(0x15b3) & 0xcf) | (1 << 4));
	} else {
		dvbt_t2_wrb(0x15a0, (dvbt_t2_rdb(0x15a0) & 0xef));
		dvbt_t2_wrb(0x15b3, (dvbt_t2_rdb(0x15b3) & 0xcf));
	}

	/* T3 chip close dccomp*/
	if (is_meson_t3_cpu())
		dvbt_t2_wrb(0x15a8, 0x02);

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

//	dvbt_t2_wrb(0x2815, 0x03);
	dvbt_t2_wrb(0x2751, 0xf0);
	dvbt_t2_wrb(0x2752, 0x3c);
	dvbt_t2_wrb(0x53c, 0x6f);
	dvbt_t2_wrb(0x2906, (dvbt_t2_rdb(0x2906) & 0xfe) | (1));
	dvbt_t2_wrb(0x28fd, 0x00);
	dvbt_t2_wrb(0x1500, 0x00);
//	dvbt_t2_wrb(R368TER_FEPATH_CONF0, (dvbt_t2_rdb(R368TER_FEPATH_CONF0) & 0xfe) | (1));

	temp_bw1 = (((1 << 11) * 100) / (914 * bandwidth));
	temp_bw2 = 125 - (100 * bandwidth / 2);

	switch (bandwidth) {
	case 6:
		temp_bw = 2;
		break;

	case 7:
		temp_bw = 3;
		break;

	case 8:
	default:
		temp_bw = 4;
		break;
	}

	dvbt_t2_wrb(0x15d0, (dvbt_t2_rdb(0x15d0) & 0xfd) | (1 << 1));
	dvbt_t2_wrb(0x15d0, (dvbt_t2_rdb(0x15d0) & 0xef));
	dvbt_t2_wrb(0x15d1, (dvbt_t2_rdb(0x15d1) & 0x7f) | (1 << 7));
	dvbt_t2_wrb(0x15d2, (unsigned char)((temp_bw1 * temp_bw2) / 100));
	dvbt_t2_wrb(0x15d4, (unsigned char)((temp_bw1 * temp_bw2) / 100));
	dvbt_t2_wrb(0x15d1, (dvbt_t2_rdb(0x15d1) & 0xf8) | (temp_bw));
	dvbt_t2_wrb(0x15d8, 0x7f);
	dvbt_t2_wrb(0x15d9, 0x1a);
	dvbt_t2_wrb(0x2a2a, dvbt_t2_rdb(0x2a2a) & 0x0f);
//	dvbt_t2_wrb(0x15a3, 0x0a);
	dvbt_t2_wrb(0x15d6, 0xa0);
	dvbt_t2_wrb(0x2906, dvbt_t2_rdb(0x2906) & 0x87);

	dvbt_t2_wrb(0x2830, 0x20);
	dvbt_t2_wrb(R368TER_DVBT_CTRL, 0x00);
	dvbt_t2_wrb(R368TER_DVBT_CTRL, 0x20);
	PR_DVBT("DVB-T init ok\n");
}

void dvbt_rst_demod(struct aml_dtvdemod *demod, struct dvb_frontend *fe)
{
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
		demod_top_write_reg(DEMOD_TOP_REGC, 0x110010);
		usleep_range(1000, 1001);
		demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		front_write_bits(AFIFO_ADC, 0x80, AFIFO_NCO_RATE_BIT,
				 AFIFO_NCO_RATE_WID);
		front_write_reg(0x22, 0x7200a06);
		front_write_reg(0x2f, 0x0);
		front_write_reg(0x39, 0x40001000);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
	}

	dvbt_reg_initial(demod->bw, fe);
}

unsigned int dvbt_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dvbt *demod_dvbt, struct dvb_frontend *fe)
{
	int ret = 0;
	u8_t demod_mode = 1;
	u8_t bw, sr, ifreq, agc_mode;
	u32_t ch_freq;

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

	demod->demod_status.ch_mode = 0;	/* TODO */
	demod->demod_status.agc_mode = agc_mode;
	demod->demod_status.ch_freq = ch_freq;
	demod->demod_status.ch_bw = (8 - bw) * 1000;
	demod->demod_status.symb_rate = 0;

	demod_mode = 1;
	/* for si2176 IF:5M   sr 28.57 */
	sr = 4;
	ifreq = 4;
	PR_INFO("%s:1:bw=%d, demod_mode=%d\n", __func__, bw, demod_mode);

	/*bw = BANDWIDTH_AUTO;*/
	if (bw == BANDWIDTH_AUTO)
		demod_mode = 2;

	demod->bw = bw;
	dvbt_reg_initial(bw, fe);
	PR_DVBT("DVBT mode\n");

	return ret;
}

int dvbt2_set_ch(struct aml_dtvdemod *demod, struct dvb_frontend *fe)
{
	int ret = 0;

	dvbt2_riscv_init(demod, fe);

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

		/* on lasser place aux bits de l'autre mot */
		if (right_bits > 0 && right_bits < 32)
			temp_val = (temp_val << (32 - right_bits));
		else
			PR_ERR("shift err, %d\n", right_bits);

		result = (result | temp_val);
		result = (result & var_size_use);
	}

	demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
	return result;
}

void dtvdemod_get_plp(struct amldtvdemod_device_s *devp, struct dtv_property *tvp)
{
	/* unsigned int miso_mode; */
	unsigned int plp_num;
	/* unsigned int pos; */
	unsigned int i;
	char plp_ids[MAX_PLP_NUM];
	/* char plp_type[MAX_PLP_NUM]; */
	unsigned int val;

	/* val[29-24]:0x805, plp num */
	if (demod_is_t5d_cpu(devp)) {
		val = front_read_reg(0x3e);
		plp_num = (val >> 24) & 0x3f;
	} else {
		plp_num = dvbt_t2_rdb(0x805);
		PR_INFO("read 0x805: %d\n", plp_num);
	}

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
	PR_INFO("%s: %d\n", __func__, id);
}

static int calcul_carrier_offset(struct aml_dtvdemod *demod)
{
	int cfo = dvbt_t2_rdb(0x28cc) + (dvbt_t2_rdb(0x28cd) << 8);

	if (cfo & 0x8000)
		cfo = -(cfo ^ 0xFFFF) - 1;

	switch (demod->bw) {
	case BANDWIDTH_1_712_MHZ:
		cfo = cfo * 171 / 100;
		break;
	case BANDWIDTH_5_MHZ:
		cfo *= 5;
		break;
	case BANDWIDTH_6_MHZ:
		cfo *= 6;
		break;
	case BANDWIDTH_7_MHZ:
		cfo *= 7;
		break;
	case BANDWIDTH_8_MHZ:
	default:
		cfo *= 8;
		break;
	}
	cfo = cfo * 125 / 28672;

	return cfo;
}

static int get_per_val(void)
{
	int timeout1;
	int oldvalue;
	int errors;
	int per;
	int cpt = 0;
	int i;
	unsigned int r_599, r_59a, r_59b;

	timeout1 = 1500;

	do {
		r_599 =	(dvbt_t2_rdb(0x599) & 0x7) << 16;
		r_59a =	dvbt_t2_rdb(0x59a) << 8;
		r_59b =	dvbt_t2_rdb(0x59b);
		oldvalue = r_599 & 0x80;

		if (!oldvalue) {
			errors = r_599 + r_59a + r_59b;

			if (errors == 0) {
				per = 0;
			} else {
				for (i = 0; i < 3; i++)
					errors = ((errors * 100) / (1 << 6));
				per = ((errors * 10) / (1 << 3));
			}
		}

		cpt += 5;
	} while ((oldvalue == 1) && (cpt < timeout1));

	return per;
}

void dvbt_info(struct aml_dtvdemod *demod, struct seq_file *seq)
{
	unsigned int sm = dvbt_t2_rdb(0x2901);
	unsigned int sm_st = sm >> 4;
	unsigned int sm_cst = sm & 0xf;
	unsigned int gi_st0 = dvbt_t2_rdb(0x2745);
	unsigned int gi_st1 = dvbt_t2_rdb(0x2744);
	unsigned int gi_echo = (gi_st0 >> 4) & 0x1;
	unsigned int giq = gi_st1 >> 4;
	unsigned int gi = gi_st1 & 0xf;
	int cfo = calcul_carrier_offset(demod);
	unsigned int snr = dvbt_t2_rdb(0x2a08) + ((dvbt_t2_rdb(0x2a09)) << 8);
	unsigned int csnr = snr * 30 / 64; //dBx10.
	unsigned int ber = dvbt_t2_rdb(0x53b);
	unsigned int per = get_per_val();
	unsigned int p1_dlok = dvbt_t2_rdb(0x53e);
	unsigned int punc = dvbt_t2_rdb(0x53a) & 0x1f;
	char *str_sm_st, *str_sm_cst, *str_gi, *str_giq, *str_punc;

	switch (sm_st) {
	case 0:
		str_sm_st = "No_st";
		break;
	case 1:
		str_sm_st = "AGC_L";
		break;
	case 3:
		str_sm_st = "GID_L";
		break;
	case 7:
		str_sm_st = "TPS_L";
		break;
	case 15:
		str_sm_st = "Tra_F";
		break;
	default:
		str_sm_st = "Error";
		break;
	}

	switch (sm_cst) {
	case 0:
		str_sm_cst = "Idle";
		break;
	case 1:
		str_sm_cst = "res_stat";
		break;
	case 2:
		str_sm_cst = "W_AGC_lock";
		break;
	case 3:
		str_sm_cst = "W_GID_lock";
		break;
	case 4:
		str_sm_cst = "W_CP_Corr";
		break;
	case 5:
		str_sm_cst = "W_2CP_Corr";
		break;
	case 6:
		str_sm_cst = "W_SP_Corr";
		break;
	case 7:
		str_sm_cst = "G_SP_Corr";
		break;
	case 8:
		str_sm_cst = "W_TPS_lock";
		break;
	case 9:
		str_sm_cst = "TPS_Monitor";
		break;
	case 12:
		str_sm_cst = "G_AGC_lock";
		break;
	case 13:
		str_sm_cst = "G_GID_lock";
		break;
	case 14:
		str_sm_cst = "W_CP_Corr";
		break;
	default:
		str_sm_cst = "Error";
		break;
	}

	switch (gi) {
	case 0:
		str_gi = "1/32";
		break;
	case 1:
		str_gi = "1/16";
		break;
	case 2:
		str_gi = "1/8";
		break;
	case 3:
		str_gi = "1/4";
		break;
	case 4:
		str_gi = "1/128";
		break;
	case 5:
		str_gi = "19/128";
		break;
	case 6:
		str_gi = "19/256";
		break;
	default:
		str_gi = "NF";
		break;
	}

	str_giq = "NULL";
	if (gi_echo) {
		if (giq > 2)
			str_giq = "G";
		else
			str_giq = "B";
	} else {
		if (giq > 7)
			str_giq = "G";
		else if (giq < 3)
			str_giq = "B";
	}

	switch (punc) {
	case 0xd:
		str_punc = "1/2";
		break;
	case 0x12:
		str_punc = "2/3";
		break;
	case 0x15:
		str_punc = "3/4";
		break;
	case 0x18:
		str_punc = "5/6";
		break;
	case 0x19:
		str_punc = "6/7";
		break;
	case 0x1a:
		str_punc = "7/8";
		break;
	default:
		str_punc = "Err";
		break;
	}

	if (seq) {
		seq_printf(seq, "FSM 0x%x, ST %s, CST %s, GI %s, GI_Q %s, CFO %dKHz, SNR %d dBx10\n",
			sm, str_sm_st, str_sm_cst, str_gi, str_giq, cfo, csnr);
		seq_printf(seq, "pwr_meter 0x%x,ber %d, per %d, TS 0x%x, VIT lock %d, Punc %s\n\n",
			(dvbt_t2_rdb(0x82f) << 8) + dvbt_t2_rdb(0x82e), ber, per,
			dvbt_t2_rdb(0x581) & 0x8 >> 3, p1_dlok, str_punc);
	} else {
		PR_DVBT("FSM 0x%x, ST %s, CST %s, GI %s, GI_Q %s, CFO %dKHz, SNR %d dBx10\n",
			sm, str_sm_st, str_sm_cst, str_gi, str_giq, cfo, csnr);
		PR_DVBT("pwr_meter 0x%x,ber %d, per %d, TS 0x%x, VIT lock %d, Punc %s\n\n",
			(dvbt_t2_rdb(0x82f) << 8) + dvbt_t2_rdb(0x82e), ber, per,
			dvbt_t2_rdb(0x581) & 0x8 >> 3, p1_dlok, str_punc);
	}
}

void dvbt2_info(struct aml_dtvdemod *demod, struct seq_file *seq)
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
	int cfo = calcul_carrier_offset(demod);

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

	if (seq) {
		seq_printf(seq, "SNR c%ddB, F%d,D%d %s FFT %s %s %s, GI %s, L1 %s, PP_md %s\n",
			c_snr, f_snr, d_snr, str_ts_type, str_fft,
			str_constel, str_miso_mode, str_gi, str_l1_cstl, str_pp_mode);
		seq_printf(seq, "LDPC %2d,bch 0x%x, D 0x%x/0x%x C 0x%x/0x%x, CFO %dKHz, SFO %dppm\n",
			ldpc_it, bch, data_err, data_ttl, cmmn_err, cmmn_ttl, cfo, sfo);
		seq_printf(seq, "COM:%x,AUT:%x,GI:%x,PRE:%x,POST:%x,P1G:%x,CASG:%x,0x361b:%x\n",
			(dvbt_t2_rdb(0x1579) >> 6) & 0x01, (dvbt_t2_rdb(0x1579) >> 5) & 0x01,
			(dvbt_t2_rdb(0x2745) >> 5) & 0x01, (dvbt_t2_rdb(0x839) >> 4) & 0x01,
			(dvbt_t2_rdb(0x839) >> 3) & 0x01, dvbt_t2_rdb(0x15ba), dvbt_t2_rdb(0x15d5),
			dvbt_t2_rdb(0x361b));
		seq_printf(seq, "pwr_meter 0x%x\n\n",
			   (dvbt_t2_rdb(0x82f) << 8) + dvbt_t2_rdb(0x82e));
	} else {
		PR_DVBT("SNR c%ddB, F%d,D%d %s FFT %s %s %s, GI %s, L1 %s, PP_md %s\n",
			c_snr, f_snr, d_snr, str_ts_type, str_fft,
			str_constel, str_miso_mode, str_gi, str_l1_cstl, str_pp_mode);
		PR_DVBT("LDPC %2d,bch 0x%x, D 0x%x/0x%x C 0x%x/0x%x, CFO %dKHz, SFO %dppm\n",
			ldpc_it, bch, data_err, data_ttl, cmmn_err, cmmn_ttl, cfo, sfo);
		PR_DVBT("COM:%x,AUT:%x,GI:%x,PRE:%x,POST:%x,P1G:%2x,CASG:%2x, 0x361b:0x%x\n",
			(dvbt_t2_rdb(0x1579) >> 6) & 0x01, (dvbt_t2_rdb(0x1579) >> 5) & 0x01,
			(dvbt_t2_rdb(0x2745) >> 5) & 0x01, (dvbt_t2_rdb(0x839) >> 4) & 0x01,
			(dvbt_t2_rdb(0x839) >> 3) & 0x01, dvbt_t2_rdb(0x15ba), dvbt_t2_rdb(0x15d5),
			dvbt_t2_rdb(0x361b));
		PR_DVBT("pwr_meter 0x%x\n\n", (dvbt_t2_rdb(0x82f) << 8) + dvbt_t2_rdb(0x82e));
	}
}

