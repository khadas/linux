// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
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
/*#include <linux/firmware.h>*/
#include <linux/err.h>
/*#include <linux/clk.h>*/
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
/*#include <linux/gpio/consumer.h>*/
#include <linux/i2c.h>
#include <linux/gpio.h>

#include "demod_func.h"
#include "dvbs.h"
#include "dvbs_diseqc.h"

struct aml_def_diseqc_lnb aml_lnb;
static int diseqc_debug;
static unsigned int diseqc_attached;

#define AML_DISEQC_VER		"20201225\n"
#define DISEQC_EN_ON_DELAY	60

#define dprintk(level, fmt, arg...)				\
	do {							\
		if (diseqc_debug >= (level))			\
			pr_info("diseqc: " fmt, ## arg);	\
	} while (0)

#define aml_lnb_sgm41286

u32 sendburst_on;
u32 diseqc_cmd_bypass;

void aml_diseqc_dbg_en(unsigned int val)
{
	diseqc_debug = val;
}

void aml_def_set_lnb_en(int high_low)
{
	if (IS_ERR_OR_NULL(aml_lnb.gpio_lnb_en)) {
		dprintk(0, "err lnb en gpio dest\n");
		return;
	}

	if (aml_lnb.lnb_en == high_low)
		return;

	aml_lnb.lnb_en = high_low;
	if (high_low)
		gpiod_set_value(aml_lnb.gpio_lnb_en, 1);
	else
		gpiod_set_value(aml_lnb.gpio_lnb_en, 0);
	dprintk(1, "lnb en:%d\n", high_low);
}

void aml_def_set_lnb_sel(int high_low)
{
	if (IS_ERR_OR_NULL(aml_lnb.gpio_lnb_sel)) {
		dprintk(0, "err lnb sel gpio dest\n");
		return;
	}

	if (aml_lnb.lnb_sel == high_low)
		return;

	aml_lnb.lnb_sel = high_low;
	if (high_low)
		gpiod_set_value(aml_lnb.gpio_lnb_sel, 1);
	else
		gpiod_set_value(aml_lnb.gpio_lnb_sel, 0);
	dprintk(1, "lnb sel:%d\n", high_low);
}

void aml_diseqc_toneburst_sa(void)
{
	/* tone burst a is 12.5ms continus tone*/
	dvbs2_diseqc_continuous_tone(1);
	mdelay(13);
	dvbs2_diseqc_continuous_tone(0);
}

void aml_diseqc_toneburst_sb(void)
{
	struct dvb_diseqc_master_cmd cmd;

	cmd.msg_len = 1;
	cmd.msg[0] = 0xff;
	cmd.msg[1] = 0xff;
	dvbs2_diseqc_send_msg(cmd.msg_len, &cmd.msg[0]);
}

void aml_diseqc_tone_on(u32 onoff)
{
	if (onoff)
		dvbs2_diseqc_continuous_tone(1);
	else
		dvbs2_diseqc_continuous_tone(0);

	dprintk(0, "%s %d\n", __func__, onoff);
}

void aml_diseqc_flag_tone_on(u32 onoff)
{
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	lnb->tone_on = onoff;
}

u32 aml_diseqc_send_cmd(struct dvb_diseqc_master_cmd *cmd)
{
	unsigned int timeout;
	unsigned long  ret = 0;
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	reinit_completion(&lnb->tx_msg_ok);

	/* disable continuous tone */
	aml_diseqc_tone_on(0);
	if (lnb->tone_on)
		mdelay(12);

	mutex_lock(&lnb->demod->mutex_tx_msg);
	/*PR_INFO("diseqc_send_master_cmd empty func call\n");*/
	timeout = msecs_to_jiffies(2000);
	if (cmd->msg_len)
		dvbs2_diseqc_send_msg(cmd->msg_len, cmd->msg);
	else
		goto cmd_exit;

	ret = wait_for_completion_timeout(&lnb->tx_msg_ok, timeout);
	if (ret <= 0)	/* time out */
		dprintk(0, "send cmd time out\n");

	/* Send burst SA or SB */
	if (sendburst_on && cmd->msg_len == 4 && cmd->msg[2] == 0x38 &&
	    cmd->msg[3] >= 0xf0) {
		mdelay(16);
		if ((cmd->msg[3] >= 0xf0 && cmd->msg[3] <= 0xf3) &&
		    (cmd->msg[3] >= 0xf8 && cmd->msg[3] <= 0xfb))
			aml_diseqc_toneburst_sa();
		else
			aml_diseqc_toneburst_sb();
		dprintk(1, "burst\n");
	}

	/* Is tone on, need set tone on */
	if (lnb->tone_on)
		mdelay(16);
	aml_diseqc_tone_on(lnb->tone_on);


cmd_exit:
	dprintk(0, "%s burst_on:%d\n", __func__, sendburst_on);
	mutex_unlock(&lnb->demod->mutex_tx_msg);

	return ret;
}

int aml_diseqc_send_burst(struct dvb_frontend *fe,
				 enum fe_sec_mini_cmd minicmd)

{
	dprintk(0, "%s burst-%d\n", __func__, minicmd);

	if (minicmd == SEC_MINI_A)
		aml_diseqc_toneburst_sa();
	else
		aml_diseqc_toneburst_sb();

	return 0;
}

#ifdef aml_lnb_sgm41286

/*
 * This callback function to implement the
 *	FE_SET_TONE ioctl (only Satellite).
 *
 *
 */
int aml_diseqc_set_tone(struct dvb_frontend *fe, enum fe_sec_tone_mode tone)
{
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	if (diseqc_cmd_bypass)
		return 1;

	dprintk(1, "%s: %d\n", __func__, tone ? 0 : 1);

	switch (tone) {
	case SEC_TONE_OFF:
		aml_diseqc_flag_tone_on(0);
		aml_diseqc_tone_on(0);

		if (lnb->voltage != SEC_VOLTAGE_OFF)
			mdelay(23);
		break;
	case SEC_TONE_ON:
		aml_diseqc_flag_tone_on(1);
		aml_diseqc_tone_on(1);

		if (lnb->voltage != SEC_VOLTAGE_OFF)
			mdelay(DISEQC_EN_ON_DELAY);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * This callback function to implement the
 *	FE_SET_VOLTAGE ioctl (only Satellite).
 *
 *
 */
int aml_diseqc_set_voltage(struct dvb_frontend *fe,
			   enum fe_sec_voltage voltage)
{
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	dprintk(1, "%s: %d\n", __func__, voltage);
	lnb->voltage = voltage;
	switch (voltage) {
	case SEC_VOLTAGE_OFF:
		aml_def_set_lnb_en(0);
		aml_def_set_lnb_sel(0);
		mdelay(20);
		break;
	case SEC_VOLTAGE_13:
		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(0);
		mdelay(DISEQC_EN_ON_DELAY);
		break;
	case SEC_VOLTAGE_18:
		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(1);
		mdelay(DISEQC_EN_ON_DELAY);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * This callback function to implement the
 *	FE_DISEQC_SEND_MASTER_CMD ioctl (only Satellite).
 *
 *
 */
int aml_diseqc_send_master_cmd(struct dvb_frontend *fe,
			       struct dvb_diseqc_master_cmd *cmd)
{
	int ret;
	u32 i, size;
	unsigned char log_buf[128] = {0};

	if (diseqc_cmd_bypass)
		return 1;

	if (cmd->msg_len <= 0)
		return -EINVAL;
	dprintk(1, "%s\n", __func__);

	size = sprintf(log_buf, "tx len %d:", cmd->msg_len);
	for (i = 0; i < cmd->msg_len; i++)
		size += sprintf(log_buf + size, " %02x", cmd->msg[i]);

	size += sprintf(log_buf + size, "\n");
	log_buf[size] = '\0';
	dprintk(1, "%s", log_buf);

	ret = aml_diseqc_send_cmd(cmd);
	return 0;
}

/*
 * callback function to implement the
 *	FE_ENABLE_HIGH_LNB_VOLTAGE ioctl (only Satellite).
 *
 */
int
aml_diseqc_en_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{
	/*struct aml_def_diseqc_lnb *lnb = &aml_lnb;*/

	dprintk(1, "%s %d\n", __func__, (u32)arg);
	if (arg) {
		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(1);
		mdelay(DISEQC_EN_ON_DELAY);
	} else {
		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(0);
	}

	return 0;
}

/*
 * callback function requesting that the Satelite Equipment
 *	Control (SEC) driver to release and free any memory
 *
 */
void aml_diseqc_release_sec(struct dvb_frontend *fe)
{
	aml_diseqc_set_voltage(fe, SEC_VOLTAGE_OFF);
	dprintk(1, "diseqc release\n");
	/* if necessary free memory*/
}

#endif

void aml_diseqc_attach(struct amldtvdemod_device_s *devp,
		       struct dvb_frontend *fe)
{
	struct aml_def_diseqc_lnb *diseqc = &aml_lnb;

	if (diseqc_attached) {
		dprintk(0, "diseqc had attached\n");
		return;
	}

	memset(&aml_lnb, 0, sizeof(struct aml_def_diseqc_lnb));
	diseqc->demod = devp;

	init_completion(&diseqc->tx_msg_ok);
	init_completion(&diseqc->rx_msg_ok);
	diseqc->gpio_lnb_en = NULL;
	diseqc->gpio_lnb_sel = NULL;
	if (!strcmp(devp->diseqc_name, "sgm_41286")) {
		diseqc = &aml_lnb;
		/*
		 * Diseqc 1.x compatible
		 */
		diseqc->gpio_lnb_en = devm_gpiod_get(devp->dev, "lnb_en",
						GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(diseqc->gpio_lnb_en))
			dprintk(0, "get lnb en fail\n");
		diseqc->gpio_lnb_sel = devm_gpiod_get(devp->dev, "lnb_sel",
						 GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(diseqc->gpio_lnb_sel))
			dprintk(0, "get lnb sel fail\n");

		fe->ops.release_sec = aml_diseqc_release_sec;
		fe->ops.set_tone = aml_diseqc_set_tone;
		fe->ops.set_voltage = aml_diseqc_set_voltage;
		fe->ops.diseqc_send_master_cmd = aml_diseqc_send_master_cmd;
		fe->ops.enable_high_lnb_voltage = aml_diseqc_en_high_lnb_voltage;
		fe->ops.diseqc_send_burst = aml_diseqc_send_burst;
		diseqc->lnb_en = 1;
		diseqc->lnb_sel = 1;
		aml_def_set_lnb_en(0);
		aml_def_set_lnb_sel(0);
		/*aml_diseqc_tone_on(0);*/
	}

	diseqc_attached = 1;
	dprintk(0, "%s flag:%d\n", __func__, diseqc_attached);
}

void aml_diseqc_isr(void)
{
	unsigned int diseq_irq_sts = 0;
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	diseq_irq_sts = dvbs2_diseqc_irq_check();
	if (diseq_irq_sts) {
		if (diseq_irq_sts & IRQ_STS_GAPBURST)
			dprintk(1, "isr IRQGAPBURST\n");

		if (diseq_irq_sts & IRQ_STS_TXFIFO64B)
			dprintk(1, "isr IRQTXFIFO64B\n");

		if (diseq_irq_sts & IRQ_STS_TXEND) {
			complete(&lnb->tx_msg_ok);
			dprintk(1, "isr IRQTXEND\n");
		}

		if (diseq_irq_sts & IRQ_STS_TIMEOUT)
			dprintk(1, "isr IRQTIMEOUT\n");

		if (diseq_irq_sts & IRQ_STS_TRFINISH)
			dprintk(1, "isr IRQTRFINISH\n");

		if (diseq_irq_sts & IRQ_STS_RXFIFO8B)
			dprintk(1, "isr IRQRXFIFO8B\n");

		if (diseq_irq_sts & IRQ_STS_RXEND)
			dprintk(1, "isr IRQRXEND\n");
	}
}

void aml_diseqc_status(void)
{
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	dprintk(0, "%s\n", AML_DISEQC_VER);
	dprintk(0, "lnb_en=%d\n", lnb->lnb_en);
	dprintk(0, "lnb_sel=%d\n", lnb->lnb_sel);
	dprintk(0, "tone_on=%d\n", lnb->tone_on);
	dprintk(0, "voltage=%d\n", lnb->voltage);
}

