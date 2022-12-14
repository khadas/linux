// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "aml_demod.h"
#include "demod_func.h"
#include <linux/kthread.h>

/*#include "dvbc_func.h"*/

MODULE_PARM_DESC(debug_amldvbc, "\n\t\t Enable frontend demod debug information");
static int debug_amldvbc;
module_param(debug_amldvbc, int, 0644);

/*#define dprintk(a ...) do { if (debug_amldvbc) printk(a); } while (0)*/

/*move to dvbc_old.c*/
/* static struct task_struct *cci_task; */
/*int cciflag = 0;*/
struct timer_list mytimer;


static void dvbc_cci_timer(struct timer_list *timer)
{
#if 0
	int count;
	int maxCCI_p, re, im, j, i, times, maxCCI, sum, sum1, reg_0xf0, tmp1,
	    tmp, tmp2, reg_0xa8, reg_0xac;
	int reg_0xa8_t, reg_0xac_t;

	count = 100;
	if ((((dvbc_read_reg(QAM_BASE + 0x18)) & 0x1) == 1)) {
		PR_DVBC("[cci]lock ");
		if (cciflag == 0) {
			dvbc_write_reg(QAM_BASE + 0xa8, 0);

			cciflag = 0;
		}
		PR_DVBC("\n");
		mdelay(500);
		mod_timer(&mytimer, jiffies + 2 * HZ);
		return;
	}
	if (cciflag == 1) {
		PR_DVBC("[cci]cciflag is 1,wait 20\n");
		mdelay(20000);
	}
	times = 300;
	tmp = 0x2be2be3;
	/*0x2ae4772;  IF = 6M, fs = 35M, dec2hex(round(8*IF/fs*2^25)) */
	tmp2 = 0x2000;
	tmp1 = 8;
	reg_0xa8 = 0xc0000000;	/* bypass CCI */
	reg_0xac = 0xc0000000;	/* bypass CCI */

	maxCCI = 0;
	maxCCI_p = 0;
	for (i = 0; i < times; i++) {
		/*reg_0xa8 = app_apb_read_reg(0xa8); */
		reg_0xa8_t = reg_0xa8 + tmp + i * tmp2;
		dvbc_write_reg(QAM_BASE + 0xa8, reg_0xa8_t);
		reg_0xac_t = reg_0xac + tmp - i * tmp2;
		dvbc_write_reg(QAM_BASE + 0xac, reg_0xac_t);
		sum = 0;
		sum1 = 0;
		for (j = 0; j < tmp1; j++) {
			/* msleep(20); */
			/* mdelay(20); */
			reg_0xf0 = dvbc_read_reg(QAM_BASE + 0xf0);
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
		if (sum > maxCCI) {
			maxCCI = sum;
			if (maxCCI > 24)
				maxCCI_p = reg_0xa8_t & 0x7fffffff;
		}
		if ((sum < 24) && (maxCCI_p > 0))
			break;	/* stop CCI detect. */
	}

	if (maxCCI_p > 0) {
		dvbc_write_reg(QAM_BASE + 0xa8, maxCCI_p & 0x7fffffff);
		/* enable CCI */
		dvbc_write_reg(QAM_BASE + 0xac, maxCCI_p & 0x7fffffff);
		/* enable CCI */
		/*     if(dvbc.mode == 4) // 256QAM */
		dvbc_write_reg(QAM_BASE + 0x54, 0xa25705fa);
		/**/ cciflag = 1;
		mdelay(1000);
	} else {
		PR_DVBC
		    ("[cci] ------------  find NO CCI -------------------\n");
		cciflag = 0;
	}

	PR_DVBC("[cci][%s]--------------------------\n", __func__);
	mod_timer(&mytimer, jiffies + 2 * HZ);
	return;
/*      }*/
#endif
}

int dvbc_timer_init(void)
{
	PR_DVBC("%s\n", __func__);
	timer_setup(&mytimer, dvbc_cci_timer, 0);
	mytimer.expires = jiffies + 2 * HZ;
	add_timer(&mytimer);
	return 0;
}

void dvbc_timer_exit(void)
{
	PR_DVBC("%s\n", __func__);
	del_timer(&mytimer);
}
#if 0	/*ary*/
int dvbc_cci_task(void *data)
{
	int count;
	int maxCCI_p, re, im, j, i, times, maxCCI, sum, sum1, reg_0xf0, tmp1,
	    tmp, tmp2, reg_0xa8, reg_0xac;
	int reg_0xa8_t, reg_0xac_t;

	count = 100;
	while (1) {
		msleep(200);
		if ((((dvbc_read_reg(QAM_BASE + 0x18)) & 0x1) == 1)) {
			PR_DVBC("[cci]lock ");
			if (cciflag == 0) {
				dvbc_write_reg(QAM_BASE + 0xa8, 0);
				dvbc_write_reg(QAM_BASE + 0xac, 0);
				PR_DVBC("no cci ");
				cciflag = 0;
			}
			PR_DVBC("\n");
			msleep(500);
			continue;
		}

		if (cciflag == 1) {
			PR_DVBC("[cci]cciflag is 1,wait 20\n");
			msleep(20000);
		}
		times = 300;
		tmp = 0x2be2be3;
		/*0x2ae4772; IF = 6M,fs = 35M, dec2hex(round(8*IF/fs*2^25)) */
		tmp2 = 0x2000;
		tmp1 = 8;
		reg_0xa8 = 0xc0000000;	/* bypass CCI */
		reg_0xac = 0xc0000000;	/* bypass CCI */

		maxCCI = 0;
		maxCCI_p = 0;
		for (i = 0; i < times; i++) {
			/*reg_0xa8 = app_apb_read_reg(0xa8); */
			reg_0xa8_t = reg_0xa8 + tmp + i * tmp2;
			dvbc_write_reg(QAM_BASE + 0xa8, reg_0xa8_t);
			reg_0xac_t = reg_0xac + tmp - i * tmp2;
			dvbc_write_reg(QAM_BASE + 0xac, reg_0xac_t);
			sum = 0;
			sum1 = 0;
			for (j = 0; j < tmp1; j++) {
				/*         msleep(1); */
				reg_0xf0 = dvbc_read_reg(QAM_BASE + 0xf0);
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
			if (sum > maxCCI) {
				maxCCI = sum;
				if (maxCCI > 24)
					maxCCI_p = reg_0xa8_t & 0x7fffffff;
			}

			if ((sum < 24) && (maxCCI_p > 0))
				break;	/* stop CCI detect. */
		}

		if (maxCCI_p > 0) {
			dvbc_write_reg(QAM_BASE + 0xa8, maxCCI_p & 0x7fffffff);
			/* enable CCI */
			dvbc_write_reg(QAM_BASE + 0xac, maxCCI_p & 0x7fffffff);
			/* enable CCI */
			/*     if(dvbc.mode == 4) // 256QAM */
			dvbc_write_reg(QAM_BASE + 0x54, 0xa25705fa);
			/**/ cciflag = 1;
			msleep(1000);
		} else {
			cciflag = 0;
		}

		PR_DVBC("[cci][%s]--------------------------\n", __func__);
	}
	return 0;
}

int dvbc_get_cci_task(void)
{
	if (cci_task)
		return 0;
	else
		return 1;
}

void dvbc_create_cci_task(void)
{
	int ret;

	/*dvbc_write_reg(QAM_BASE+0xa8, 0x42b2ebe3); // enable CCI */
	/* dvbc_write_reg(QAM_BASE+0xac, 0x42b2ebe3); // enable CCI */
/*     if(dvbc.mode == 4) // 256QAM*/
	/* dvbc_write_reg(QAM_BASE+0x54, 0xa25705fa); // */
	ret = 0;
	cci_task = kthread_create(dvbc_cci_task, NULL, "cci_task");
	if (ret != 0) {
		PR_DVBC("[%s]Create cci kthread error!\n", __func__);
		cci_task = NULL;
		return;
	}
	wake_up_process(cci_task);
	PR_DVBC("[%s]Create cci kthread and wake up!\n", __func__);
}

void dvbc_kill_cci_task(void)
{
	if (cci_task) {
		kthread_stop(cci_task);
		cci_task = NULL;
		PR_DVBC("[%s]kill cci kthread !\n", __func__);
	}
}
#endif


int dvbc_set_ch(struct aml_dtvdemod *demod, struct aml_demod_dvbc *demod_dvbc,
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

	if (ch_freq < 1000 || ch_freq > 900000) {
		PR_DVBC("[id %d] Error: Invalid Channel Freq option %d\n", demod->id, ch_freq);
		ch_freq = 474000;
		ret = -1;
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

	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		dvbc_reg_initial_old(demod);
	else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX) && !is_meson_txhd_cpu())
		dvbc_reg_initial(demod, fe);

	return ret;
}
