// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "aml_demod.h"
#include "demod_func.h"

void enable_qam_int(int idx)
{
	unsigned long mask;

	mask = dvbc_read_reg(0xd0);
	mask |= (1 << idx);
	dvbc_write_reg(0xd0, mask);
}

void disable_qam_int(int idx)
{
	unsigned long mask;

	mask = dvbc_read_reg(0xd0);
	mask &= ~(1 << idx);
	dvbc_write_reg(0xd0, mask);
}

#ifdef AML_DEMOD_SUPPORT_DVBC
int dvbc_cci_task(void *data)
{
	int count;
	int max_CCI_p, re, im, j, i, times, max_CCI, sum, sum1, reg_0xf0, tmp1,
	    tmp, tmp2, reg_0xa8, reg_0xac;
	int reg_0xa8_t, reg_0xac_t;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)data;

	count = 100;
	while (1) {
		msleep(200);
		if ((((dvbc_read_reg(0x18)) & 0x1) == 1)) {
			PR_DVBC("[id %d] [cci]lock\n", demod->id);
			if (demod->cciflag == 0) {
				dvbc_write_reg(0xa8, 0);
				dvbc_write_reg(0xac, 0);
				PR_DVBC("[id %d] no cci\n", demod->id);
				demod->cciflag = 0;
			}

			msleep(500);
			continue;
		}

		if (demod->cciflag == 1) {
			PR_DVBC("[id %d] [cci] cciflag is 1, wait 20\n",
					demod->id);
			msleep(20000);
		}
		times = 300;
		tmp = 0x2be2be3;
		/*0x2ae4772; IF = 6M,fs = 35M, dec2hex(round(8*IF/fs*2^25)) */
		tmp2 = 0x2000;
		tmp1 = 8;
		reg_0xa8 = 0xc0000000;	/* bypass CCI */
		reg_0xac = 0xc0000000;	/* bypass CCI */

		max_CCI = 0;
		max_CCI_p = 0;
		for (i = 0; i < times; i++) {
			/*reg_0xa8 = app_apb_read_reg(0xa8); */
			reg_0xa8_t = reg_0xa8 + tmp + i * tmp2;
			dvbc_write_reg(0xa8, reg_0xa8_t);
			reg_0xac_t = reg_0xac + tmp - i * tmp2;
			dvbc_write_reg(0xac, reg_0xac_t);
			sum = 0;
			sum1 = 0;
			for (j = 0; j < tmp1; j++) {
				/*         msleep(1); */
				reg_0xf0 = dvbc_read_reg(0xf0);
				re = (reg_0xf0 >> 24) & 0xff;
				im = (reg_0xf0 >> 16) & 0xff;
				if (re > 127)
					/*re = re - 256; */
					re = 256 - re;
				if (im > 127)
					/*im = im - 256; */
					im = 256 - im;

				sum += re + im;

				re = (reg_0xf0 >> 8) & 0xff;
				im = (reg_0xf0 >> 0) & 0xff;
				if (re > 127)
					/*re = re - 256; */
					re = 256 - re;
				if (im > 127)
					/*im = im - 256; */
					im = 256 - im;

				sum1 += re + im;
			}
			sum = sum / tmp1;
			sum1 = sum1 / tmp1;
			if (sum1 > sum) {
				sum = sum1;
				reg_0xa8_t = reg_0xac_t;
			}
			if (sum > max_CCI) {
				max_CCI = sum;
				if (max_CCI > 24)
					max_CCI_p = reg_0xa8_t & 0x7fffffff;
			}

			if (sum < 24 && max_CCI_p > 0)
				break; /* stop CCI detect. */
		}

		if (max_CCI_p > 0) {
			dvbc_write_reg(0xa8, max_CCI_p & 0x7fffffff);
			/* enable CCI */
			dvbc_write_reg(0xac, max_CCI_p & 0x7fffffff);
			/* enable CCI */
			/*     if(dvbc.mode == 4) // 256QAM */
			dvbc_write_reg(0x54, 0xa25705fa);
			/**/ demod->cciflag = 1;
			msleep(1000);
		} else {
			demod->cciflag = 0;
		}

		PR_DVBC("[id %d] [cci] cciflag %d\n",
				demod->id, demod->cciflag);
	}
	return 0;
}

int dvbc_get_cci_task(struct aml_dtvdemod *demod)
{
	if (demod->cci_task)
		return 0;
	else
		return 1;
}

void dvbc_create_cci_task(struct aml_dtvdemod *demod)
{
	int ret;

	/*dvbc_write_reg(QAM_BASE+0xa8, 0x42b2ebe3); // enable CCI */
	/*dvbc_write_reg(QAM_BASE+0xac, 0x42b2ebe3); // enable CCI */
	/*if(dvbc.mode == 4) // 256QAM*/
	/*dvbc_write_reg(QAM_BASE+0x54, 0xa25705fa); // */
	ret = 0;
	demod->cci_task = kthread_create(dvbc_cci_task, (void *)demod,
			"cci_task%d", demod->id);
	if (ret != 0) {
		PR_DVBC("[id %d] Create cci kthread error\n", demod->id);
		demod->cci_task = NULL;
		return;
	}

	wake_up_process(demod->cci_task);

	PR_DVBC("[id %d] Create cci kthread and wake up\n", demod->id);
}

void dvbc_kill_cci_task(struct aml_dtvdemod *demod)
{
	if (demod->cci_task) {
		kthread_stop(demod->cci_task);
		demod->cci_task = NULL;
		PR_DVBC("[id %d] kill cci kthread\n", demod->id);
	}
}

u32 dvbc_set_qam_mode(unsigned char mode)
{
	PR_DVBC("set qam mode %d\n", mode);
	dvbc_write_reg(0x008, (mode & 7));
	/* qam mode */
	switch (mode) {
	case 0:		/* 16 QAM */
		dvbc_write_reg(0x054, 0x23460224);
		/* EQ_FIR_CTL, */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		dvbc_write_reg(0x074, 0x50001a0);
		/* EQ_TH_LMS  40db      13db */
		dvbc_write_reg(0x07c, 0x003001e9);
		/* EQ_NORM and EQ_TH_MMA */
		/*dvbc_write_reg(QAM_BASE+0x080, 0x000be1ff);*/
		/* // EQ_TH_SMMA0*/
		dvbc_write_reg(0x080, 0x000e01fe);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x00000000);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2b);*/
		/* // AGC_CTRL  ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292b);*/
		/* // Pilips Tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292d);*/
		/* // Pilips Tuner*/
		dvbc_write_reg(0x094, 0x7f80092d);
		/* Pilips Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f66);
		/* by raymond 20121213 */
		break;

	case 1:		/* 32 QAM */
		dvbc_write_reg(0x054, 0x24560506);
		/* EQ_FIR_CTL, */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074, 0x5000260);*/
		/* // EQ_TH_LMS      40db  19db*/
		dvbc_write_reg(0x074, 0x50001f0);
		/* EQ_TH_LMS  40db      17.5db */
		dvbc_write_reg(0x07c, 0x00500102);
		/* EQ_TH_MMA  0x000001cc */
		dvbc_write_reg(0x080, 0x00077140);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x001fb000);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2b);*/
		/* // AGC_CTRL  ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292b);*/
		/* // Pilips Tuner*/
		dvbc_write_reg(0x094, 0x7f80092b);
		/* Pilips Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f66);
		/* by raymond 20121213 */
		break;

	case 2:		/* 64 QAM */
		/*dvbc_write_reg(QAM_BASE+0x054, 0x2256033a);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0x2336043a);
		/* EQ_FIR_CTL, by raymond */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074, 0x5000260);*/
		/* // EQ_TH_LMS  40db  19db*/
		dvbc_write_reg(0x074, 0x5000230);
		/* EQ_TH_LMS  40db      17.5db */
		dvbc_write_reg(0x07c, 0x007001bd);
		/* EQ_TH_MMA */
		dvbc_write_reg(0x080, 0x000580ed);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x001771fb);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL  ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips & maxlinear Tuner*/
		dvbc_write_reg(0x094, 0x7f802b3d);
		/* Pilips Tuner & maxlinear Tuner */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f802b3a);*/
		/* // Pilips Tuner & maxlinear Tuner*/
		dvbc_write_reg(0x0c0, 0x061f2f66);
		/* by raymond 20121213 */
		break;

	case 3:		/* 128 QAM */
		/*dvbc_write_reg(QAM_BASE+0x054, 0x2557046a);*/
		 /* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0x2437067a);
		/* EQ_FIR_CTL, by raymond 20121213 */
		dvbc_write_reg(0x068, 0x00c000d0);
		/* EQ_CRTH_SNR */
		/* dvbc_write_reg(QAM_BASE+0x074, 0x02440240);*/
		/* // EQ_TH_LMS  18.5db  18db*/
		/* dvbc_write_reg(QAM_BASE+0x074, 0x04000400);*/
		/* // EQ_TH_LMS  22db  22.5db*/
		dvbc_write_reg(0x074, 0x5000260);
		/* EQ_TH_LMS  40db      19db */
		/*dvbc_write_reg(QAM_BASE+0x07c, 0x00b000f2);*/
		/* // EQ_TH_MMA0x000000b2*/
		dvbc_write_reg(0x07c, 0x00b00132);
		/* EQ_TH_MMA0x000000b2 by raymond 20121213 */
		dvbc_write_reg(0x080, 0x0003a09d);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x000f8150);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x001a51f8);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL  ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips Tuner*/
		dvbc_write_reg(0x094, 0x7f80092c);
		/* Pilips Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f66);
		/* by raymond 20121213 */
		break;

	case 4:		/* 256 QAM */
		/*dvbc_write_reg(QAM_BASE+0x054, 0xa2580588);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0xa25905f9);
		/* EQ_FIR_CTL, by raymond 20121213 */
		dvbc_write_reg(0x068, 0x01e00220);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074,  0x50002a0);*/
		/* // EQ_TH_LMS      40db  19db*/
		dvbc_write_reg(0x074, 0x5000270);
		/* EQ_TH_LMS  40db      19db by raymond 201211213 */
		dvbc_write_reg(0x07c, 0x00f001a5);
		/* EQ_TH_MMA */
		dvbc_write_reg(0x080, 0x0002c077);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x000bc0fe);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x0013f17e);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x01bc01f9);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL  ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips Tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292d);*/
		/* // Maxlinear Tuner*/
		dvbc_write_reg(0x094, 0x7f80092d);
		/* Maxlinear Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213, when adc=35M,sys=70M,*/
		/* its better than 0x61f2f66*/
		break;
	default:		/*64qam */
		/*dvbc_write_reg(QAM_BASE+0x054, 0x2256033a);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0x2336043a);
		/* EQ_FIR_CTL, by raymond */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074, 0x5000260);*/
		/* // EQ_TH_LMS  40db  19db*/
		dvbc_write_reg(0x074, 0x5000230);
		/* EQ_TH_LMS  40db      17.5db */
		dvbc_write_reg(0x07c, 0x007001bd);
		/* EQ_TH_MMA */
		dvbc_write_reg(0x080, 0x000580ed);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x001771fb);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL  ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips & maxlinear Tuner*/
		dvbc_write_reg(0x094, 0x7f802b3d);
		/* Pilips Tuner & maxlinear Tuner */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f802b3a);*/
		/* // Pilips Tuner & maxlinear Tuner*/
		dvbc_write_reg(0x0c0, 0x061f2f66);
		/* by raymond 20121213 */
		break;
	}
	return 0;
}
#endif

void dvbc_reg_initial_old(struct aml_dtvdemod *demod)
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
	int max_frq_off, tmp;

	clk_freq = demod->demod_status.clk_freq; /* kHz */
	adc_freq = demod->demod_status.adc_freq; /* kHz */
	/* adc_freq = 25414;*/
	/*ary  no use tuner = demod->demod_status.tuner;*/
	ch_mode = demod->demod_status.ch_mode;
	agc_mode = demod->demod_status.agc_mode;
	ch_freq = demod->demod_status.ch_freq;	/* kHz */
	ch_if = demod->demod_status.ch_if;	/* kHz */
	ch_bw = demod->demod_status.ch_bw;	/* kHz */
	symb_rate = demod->demod_status.symb_rate;	/* k/sec */
	PR_DVBC("ch_if %d, %d, %d, %d, %d\n",
		ch_if, ch_mode, ch_freq, ch_bw, symb_rate);
/*	  ch_mode=4;*/
/*		dvbc_write_reg(DEMOD_CFG_BASE,0x00000007);*/
	/* disable irq */
	dvbc_write_reg(0xd0, 0);

	/* reset */
	/*dvbc_reset(); */
	dvbc_write_reg(0x4,
				dvbc_read_reg(0x4) & ~(1 << 4));
	/* disable fsm_en */
	dvbc_write_reg(0x4,
				dvbc_read_reg(0x4) & ~(1 << 0));
	/* Sw disable demod */
	dvbc_write_reg(0x4,
				dvbc_read_reg(0x4) | (1 << 0));
	/* Sw enable demod */

	dvbc_write_reg(0x000, 0x00000000);
	/* QAM_STATUS */
	dvbc_write_reg(0x004, 0x00000f00);
	/* QAM_GCTL0 */
	dvbc_write_reg(0x008, (ch_mode & 7));
	/* qam mode */

	switch (ch_mode) {
	case 0:/* 16 QAM */
		dvbc_write_reg(0x054, 0x23460224);
		/* EQ_FIR_CTL, */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		dvbc_write_reg(0x074, 0x50001a0);
		/* EQ_TH_LMS  40db	13db */
		dvbc_write_reg(0x07c, 0x003001e9);
		/* EQ_NORM and EQ_TH_MMA */
		/*dvbc_write_reg(QAM_BASE+0x080, 0x000be1ff);*/
		/* // EQ_TH_SMMA0*/
		dvbc_write_reg(0x080, 0x000e01fe);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x00000000);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2b);*/
		/* // AGC_CTRL	ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292b);*/
		/* // Pilips Tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292d);*/
		/* // Pilips Tuner*/
		dvbc_write_reg(0x094, 0x7f80092d);
		/* Pilips Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213 */
		break;

	case 1:/* 32 QAM */
		dvbc_write_reg(0x054, 0x24560506);
		/* EQ_FIR_CTL, */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074, 0x5000260);*/
		/* // EQ_TH_LMS  40db  19db*/
		dvbc_write_reg(0x074, 0x50001f0);
		/* EQ_TH_LMS  40db	17.5db */
		dvbc_write_reg(0x07c, 0x00500102);
		/* EQ_TH_MMA  0x000001cc */
		dvbc_write_reg(0x080, 0x00077140);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x001fb000);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2b);*/
		/* // AGC_CTRL	ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292b);*/
		/* // Pilips Tuner*/
		dvbc_write_reg(0x094, 0x7f80092b);
		/* Pilips Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213 */
		break;

	case 2:/* 64 QAM */
		/*dvbc_write_reg(QAM_BASE+0x054, 0x2256033a);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0x2336043a);
		/* EQ_FIR_CTL, by raymond */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074, 0x5000260);*/
		/* // EQ_TH_LMS  40db  19db*/
		dvbc_write_reg(0x074, 0x5000230);
		/* EQ_TH_LMS  40db	17.5db */
		dvbc_write_reg(0x07c, 0x007001bd);
		/* EQ_TH_MMA */
		dvbc_write_reg(0x080, 0x000580ed);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x001771fb);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL	ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips & maxlinear Tuner*/
		dvbc_write_reg(0x094, 0x7f802b3d);
		/* Pilips Tuner & maxlinear Tuner */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f802b3a);*/
		/* // Pilips Tuner & maxlinear Tuner*/
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213 */
		break;

	case 3:/* 128 QAM */
		/*dvbc_write_reg(QAM_BASE+0x054, 0x2557046a);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0x2437067a);
		/* EQ_FIR_CTL, by raymond 20121213 */
		dvbc_write_reg(0x068, 0x00c000d0);
		/* EQ_CRTH_SNR */
		/* dvbc_write_reg(QAM_BASE+0x074, 0x02440240);*/
		/* // EQ_TH_LMS  18.5db  18db*/
		/* dvbc_write_reg(QAM_BASE+0x074, 0x04000400);*/
		/* // EQ_TH_LMS  22db  22.5db*/
		dvbc_write_reg(0x074, 0x5000260);
		/* EQ_TH_LMS  40db	19db */
		/*dvbc_write_reg(QAM_BASE+0x07c, 0x00b000f2);*/
		/* // EQ_TH_MMA0x000000b2*/
		dvbc_write_reg(0x07c, 0x00b00132);
		/* EQ_TH_MMA0x000000b2 by raymond 20121213 */
		dvbc_write_reg(0x080, 0x0003a09d);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x000f8150);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x001a51f8);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL	ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips Tuner*/
		dvbc_write_reg(0x094, 0x7f80092c);
		/* Pilips Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213 */
		break;

	case 4:/* 256 QAM */
		/*dvbc_write_reg(QAM_BASE+0x054, 0xa2580588);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0xa25905f9);
		/* EQ_FIR_CTL, by raymond 20121213 */
		dvbc_write_reg(0x068, 0x01e00220);
		/* EQ_CRTH_SNR */
		/*dvbc_write_reg(QAM_BASE+0x074,  0x50002a0);*/
		/* // EQ_TH_LMS  40db  19db*/
		dvbc_write_reg(0x074, 0x5000270);
		/* EQ_TH_LMS  40db	19db by raymond 201211213 */
		dvbc_write_reg(0x07c, 0x00f001a5);
		/* EQ_TH_MMA */
		dvbc_write_reg(0x080, 0x0002c077);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x000bc0fe);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x0013f17e);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x01bc01f9);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL	ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips Tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292d);*/
		/* // Maxlinear Tuner*/
		dvbc_write_reg(0x094, 0x7f80092d);
		/* Maxlinear Tuner */
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213, when adc=35M,sys=70M,*/
		/* its better than 0x61f2f66*/
		break;
	default:		/*64qam */
		/*dvbc_write_reg(QAM_BASE+0x054, 0x2256033a);*/
		/* // EQ_FIR_CTL,*/
		dvbc_write_reg(0x054, 0x2336043a);
		/* EQ_FIR_CTL, by raymond */
		dvbc_write_reg(0x068, 0x00c000c0);
		/* EQ_CRTH_SNR */
		/* EQ_TH_LMS  40db	19db */
		dvbc_write_reg(0x074, 0x5000230);
		/* EQ_TH_LMS  40db	17.5db */
		dvbc_write_reg(0x07c, 0x007001bd);
		/* EQ_TH_MMA */
		dvbc_write_reg(0x080, 0x000580ed);
		/* EQ_TH_SMMA0 */
		dvbc_write_reg(0x084, 0x001771fb);
		/* EQ_TH_SMMA1 */
		dvbc_write_reg(0x088, 0x00000000);
		/* EQ_TH_SMMA2 */
		dvbc_write_reg(0x08c, 0x00000000);
		/* EQ_TH_SMMA3 */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);*/
		/* // AGC_CTRL	ALPS tuner*/
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);*/
		/* // Pilips & maxlinear Tuner*/
		dvbc_write_reg(0x094, 0x7f802b3d);
		/* Pilips Tuner & maxlinear Tuner */
		/*dvbc_write_reg(QAM_BASE+0x094, 0x7f802b3a);*/
		/* // Pilips Tuner & maxlinear Tuner*/
		dvbc_write_reg(0x0c0, 0x061f2f67);
		/* by raymond 20121213 */
		break;
	}

	/*dvbc_write_reg(QAM_BASE+0x00c, 0xfffffffe);*/
	/* // adc_cnt, symb_cnt*/
	dvbc_write_reg(0x00c, 0xffff8ffe);
	/* adc_cnt, symb_cnt	by raymond 20121213 */
	if (clk_freq == 0)
		afifo_ctr = 0;
	else
		afifo_ctr = (adc_freq * 256 / clk_freq) + 2;
	if (afifo_ctr > 255)
		afifo_ctr = 255;
	dvbc_write_reg(0x010, (afifo_ctr << 16) | 8000);
	/* afifo, rs_cnt_cfg */

	/*dvbc_write_reg(QAM_BASE+0x020, 0x21353e54);*/
	/* // PHS_reset & TIM_CTRO_ACCURATE  sw_tim_select=0*/
	/*dvbc_write_reg(QAM_BASE+0x020, 0x21b53e54);*/
	/* //modified by qiancheng*/
	dvbc_write_reg(0x020, 0x61b53e54);
	/*modified by qiancheng by raymond 20121208  0x63b53e54 for cci */
	/*	dvbc_write_reg(QAM_BASE+0x020, 0x6192bfe2);*/
	/* //modified by ligg 20130613 auto symb_rate scan*/
	if (adc_freq == 0)
		phs_cfg = 0;
	else
		phs_cfg = (1 << 31) / adc_freq * ch_if / (1 << 8);
	/*	8*fo/fs*2^20 fo=36.125, fs = 28.57114, = 21d775 */
	/* PR_DVBC("phs_cfg = %x\n", phs_cfg); */
	dvbc_write_reg(0x024, 0x4c000000 | (phs_cfg & 0x7fffff));
	/* PHS_OFFSET, IF offset, */

	if (adc_freq == 0) {
		max_frq_off = 0;
	} else {
		max_frq_off = (1 << 29) / symb_rate;
		/* max_frq_off = (400KHz * 2^29) / */
		/*   (AD=28571 * symbol_rate=6875) */
		tmp = 40000000 / adc_freq;
		max_frq_off = tmp * max_frq_off;
	}
	PR_DVBC("max_frq_off %x,\n", max_frq_off);
	dvbc_write_reg(0x02c, max_frq_off & 0x3fffffff);
	/* max frequency offset, by raymond 20121208 */

	/*dvbc_write_reg(QAM_BASE+0x030, 0x011bf400);*/
	/* // TIM_CTL0 start speed is 0,  when know symbol rate*/
	dvbc_write_reg(0x030, 0x245cf451);
	/*MODIFIED BY QIANCHENG */
/*		dvbc_write_reg(QAM_BASE+0x030, 0x245bf451);*/
/* //modified by ligg 20130613 --auto symb_rate scan*/
	dvbc_write_reg(0x034,
			  ((adc_freq & 0xffff) << 16) | (symb_rate & 0xffff));

	dvbc_write_reg(0x038, 0x00400000);
	/* TIM_SWEEP_RANGE 16000 */

/************* hw state machine config **********/
	dvbc_write_reg(0x040, 0x003c);
/* configure symbol rate step 0*/

	/* modified 0x44 0x48 */
	dvbc_write_reg(0x044, (symb_rate & 0xffff) * 256);
	/* blind search, configure max symbol_rate for 7218  fb=3.6M */
	/*dvbc_write_reg(QAM_BASE+0x048, 3600*256);*/
	/* // configure min symbol_rate fb = 6.95M*/
	dvbc_write_reg(0x048, 3400 * 256);
	/* configure min symbol_rate fb = 6.95M */

	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff6f); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xfffffd68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffff2f67);*/
	/* // threshold for skyworth*/
	/* dvbc_write_reg(QAM_BASE+0x0c0, 0x061f2f67); // by raymond 20121208 */
	/* dvbc_write_reg(QAM_BASE+0x0c0, 0x061f2f66);*/
	/* // by raymond 20121213, remove it to every constellation*/
/************* hw state machine config **********/

	dvbc_write_reg(0x04c, 0x00008800);	/* reserved */

	/*dvbc_write_reg(QAM_BASE+0x050, 0x00000002);  // EQ_CTL0 */
	dvbc_write_reg(0x050, 0x01472002);
	/* EQ_CTL0 by raymond 20121208 */

	/*dvbc_write_reg(QAM_BASE+0x058, 0xff550e1e);  // EQ_FIR_INITPOS */
	dvbc_write_reg(0x058, 0xff100e1e);
	/* EQ_FIR_INITPOS for skyworth */

	dvbc_write_reg(0x05c, 0x019a0000);	/* EQ_FIR_INITVAL0 */
	dvbc_write_reg(0x060, 0x019a0000);	/* EQ_FIR_INITVAL1 */

	/*dvbc_write_reg(QAM_BASE+0x064, 0x01101128);  // EQ_CRTH_TIMES */
	dvbc_write_reg(0x064, 0x010a1128);
	/* EQ_CRTH_TIMES for skyworth */
	dvbc_write_reg(0x06c, 0x00041a05);	/* EQ_CRTH_PPM */

	dvbc_write_reg(0x070, 0xffb9aa01);	/* EQ_CRLP */

	/*dvbc_write_reg(QAM_BASE+0x090, 0x00020bd5); // agc control */
	dvbc_write_reg(0x090, 0x00000bd5);	/* agc control */

	/* agc control */
	/* dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);// AGC_CTRL  ALPS tuner */
	/* dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);	  // Pilips Tuner */
	if ((agc_mode & 1) == 0)
		/* freeze if agc */
		dvbc_write_reg(0x094,
				  dvbc_read_reg(0x94) | (0x1 << 10));
	if ((agc_mode & 2) == 0) {
		/* IF control */
		/*freeze rf agc */
		dvbc_write_reg(0x094,
				  dvbc_read_reg(0x94) | (0x1 << 13));
	}
	/*Maxlinear Tuner */
	/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292d); */
	dvbc_write_reg(0x098, 0x9fcc8190);
	/* AGC_IFGAIN_CTRL */
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e028c00);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e03cc00);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e028700);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800 now*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e03cd00);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0603cd11);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800 by raymond,*/
	/* if Adjcent channel test, maybe it need change.20121208 ad invert*/
	dvbc_write_reg(0x0a0, 0x0603cd10);
	/* AGC_RFGAIN_CTRL 0x0e020800 by raymond,*/
	/* if Adjcent channel test, maybe it need change.*/
	/* 20121208 ad invert,20130221, suit for two path channel.*/

	dvbc_write_reg(0x004,
			dvbc_read_reg(0x004) | 0x33);
	/* IMQ, QAM Enable */

	/* start hardware machine */
	/*dvbc_sw_reset(0x004, 4); */
	dvbc_write_reg(0x4,
			dvbc_read_reg(0x4) | (1 << 4));
	dvbc_write_reg(0x0e8,
			(dvbc_read_reg(0x0e8) | (1 << 2)));

	/* clear irq status */
	dvbc_read_reg(0xd4);

	/* enable irq */
	dvbc_write_reg(0xd0, 0x7fff << 3);

	/*auto track*/
	/*dvbc_set_auto_symtrack(demod); */
}

void dvbc_disable_irq(int dvbc_irq)
{
	u32 mask;

	/* disable irq */
	mask = dvbc_read_reg(0xd0);
	mask &= ~(1 << dvbc_irq);
	dvbc_write_reg(0xd0, mask);
	/* clear status */
	dvbc_read_reg(0xd4);
}

char *dvbc_irq_name[] = {
	"      ADC",
	"   Symbol",
	"       RS",
	" In_Sync0",
	" In_Sync1",
	" In_Sync2",
	" In_Sync3",
	" In_Sync4",
	"Out_Sync0",
	"Out_Sync1",
	"Out_Sync2",
	"Out_Sync3",
	"Out_Sync4",
	"In_SyncCo",
	"OutSyncCo",
	"  In_Dagc",
	" Out_Dagc",
	"  Eq_Mode",
	"RS_Uncorr"
};

#ifdef AML_DEMOD_SUPPORT_DVBC
void dvbc_isr(struct aml_demod_sta *demod_sta)
{
	u32 stat, mask;
	int dvbc_irq;

	stat = dvbc_read_reg(0xd4);
	mask = dvbc_read_reg(0xd0);
	stat &= mask;

	for (dvbc_irq = 0; dvbc_irq < ARRAY_SIZE(dvbc_irq_name); dvbc_irq++) {
		if (stat >> dvbc_irq & 1) {
			if (demod_sta->debug)
				PR_DVBC("irq: dvbc %2d %s %8x\n",
					dvbc_irq, dvbc_irq_name[dvbc_irq],
					stat);
			/* dvbc_disable_irq(dvbc_irq); */
		}
	}
}

int dvbc_isr_islock(void)
{
#define IN_SYNC4_MASK (0x80)

	u32 stat, mask;

	stat = dvbc_read_reg(0xd4);
	dvbc_write_reg(0xd4, 0);
	mask = dvbc_read_reg(0xd0);
	stat &= mask;

	return (stat & IN_SYNC4_MASK) == IN_SYNC4_MASK;
}
#endif
