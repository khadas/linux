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
//#include "meson_saradc.h"

struct aml_def_diseqc_lnb aml_lnb;
static int diseqc_debug;
static unsigned int diseqc_attached;

#define AML_DISEQC_VER		"20220722"
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

	dprintk(1, "%s %d\n", __func__, onoff);
}

void aml_diseqc_flag_tone_on(u32 onoff)
{
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	lnb->tone_on = onoff;
}

u32 aml_diseqc_send_cmd(struct dvb_frontend *fe, struct dvb_diseqc_master_cmd *cmd)
{
	unsigned int timeout;
	unsigned long ret = 0;
	bool unicable_cmd = false;
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	mutex_lock(&lnb->demod->mutex_tx_msg);
	dprintk(1, "  -->%s: %#x %#x %#x %#x %#x %#x\n", __func__, cmd->msg[0], cmd->msg[1],
			cmd->msg[2], cmd->msg[3], cmd->msg[4], cmd->msg_len);

	reinit_completion(&lnb->tx_msg_ok);

	/* disable continuous tone */
	if (lnb->tone_on) {
		aml_diseqc_tone_on(0);
		mdelay(12);
	}

	/* Single cable1.0X EN50494
	 * FRAMING: 0xE0.
	 * ADDRESS: 0x00 or 0x10 or 0x11.
	 * COMMAND: 0x5A or 0x5B.
	 * Data1: bit[7:6]: UB, bit[4:2]: Bank, bit[1:0]: T[9:8].
	 * Data2: T[7:0].
	 * T = round((abs(Ft - Fo) + Fub) / S) - 350.
	 * S = 4.
	 */

	/* Single cable2.0X EN50607
	 * FRAMING: 0x7X.
	 * Data1: bit[7:3]: UB, bit[2:0]: T[10:8].
	 * Data2: T[7:0].
	 * Data3: bit[7:4]: uncommitted switches, bit[3:0]: committed switches.
	 * T = round((abs(Ft - Fo) + Fub) / S) - 350.
	 * S = 4.
	 */
	if ((cmd->msg[0] == 0xE0 &&
		(cmd->msg[1] == 0x00 || cmd->msg[1] == 0x10 || cmd->msg[1] == 0x11) &&
		(cmd->msg[2] == 0x5A || cmd->msg[2] == 0x5B)) ||
		cmd->msg[0] == 0x70 || cmd->msg[0] == 0x71 ||
		(0x7A <= cmd->msg[0] && 0x7E >= cmd->msg[0]))
		unicable_cmd = true;

	if (unicable_cmd) {
		if (lnb->voltage != SEC_VOLTAGE_18)
			aml_diseqc_set_voltage(fe, SEC_VOLTAGE_18);

		mdelay(10);
	}

	/* diseqc2.0 with reply. */
	if (cmd->msg[0] == 0xE2 || cmd->msg[0] == 0xE3 ||
		(0x7A <= cmd->msg[0] && 0x7E >= cmd->msg[0])) {
		if (!lnb->sar_adc_enable) {
			//meson_sar_adc_diseqc_in_mode_enable();
			//lnb->sar_adc_enable = true;
		}
	}

	/*PR_INFO("diseqc_send_master_cmd empty func call\n");*/
	timeout = msecs_to_jiffies(2000);
	if (cmd->msg_len) {
		dvbs2_diseqc_send_msg(cmd->msg_len, cmd->msg);
	} else {
		ret = -ENODATA;

		goto cmd_exit;
	}

	ret = wait_for_completion_timeout(&lnb->tx_msg_ok, timeout);
	if (ret <= 0)	/* time out */
		dprintk(0, "send cmd time out, ret %ld.\n", ret);

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

cmd_exit:
	if (unicable_cmd) {
		mdelay(10);

		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(0);
		lnb->voltage = SEC_VOLTAGE_13;
		fe->dtv_property_cache.voltage = SEC_VOLTAGE_13;
	}

	/* Is tone on, need set tone on */
	if (lnb->tone_on) {
		mdelay(16);
		aml_diseqc_tone_on(lnb->tone_on);
	}

	dvbs2_diseqc_send_irq_en(false);

	if (lnb->sar_adc_enable) {
		dvbs2_diseqc_recv_en(true);
		dvbs2_diseqc_recv_irq_en(true);
	}

	dprintk(0, "%s unicable_cmd:%d, burst_on:%d done.\n",
			__func__, unicable_cmd, sendburst_on);

	mutex_unlock(&lnb->demod->mutex_tx_msg);

	return ret;
}

int aml_diseqc_send_burst(struct dvb_frontend *fe,
		enum fe_sec_mini_cmd minicmd)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	dprintk(0, "%s burst-%d\n", __func__, minicmd);

	if (minicmd == SEC_MINI_A)
		aml_diseqc_toneburst_sa();
	else
		aml_diseqc_toneburst_sb();

	mutex_unlock(&devp->lock);

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
	int ret = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	if (diseqc_cmd_bypass) {
		mutex_unlock(&devp->lock);

		return 1;
	}

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
		ret = -EINVAL;
	}

	mutex_unlock(&devp->lock);

	return ret;
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
	int ret = 0;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	dprintk(1, "%s: %d\n", __func__, voltage);
	c->voltage = voltage;
	if (lnb->voltage == voltage) {
		mutex_unlock(&devp->lock);
		return 0;
	}

	lnb->voltage = voltage;
	switch (voltage) {
	case SEC_VOLTAGE_OFF:
		aml_def_set_lnb_en(0);
		aml_def_set_lnb_sel(0);
		break;
	case SEC_VOLTAGE_13:
		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(0);
		break;
	case SEC_VOLTAGE_18:
		aml_def_set_lnb_en(1);
		aml_def_set_lnb_sel(1);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&devp->lock);

	return ret;
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
	int ret = 0, i = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	if (diseqc_cmd_bypass) {
		mutex_unlock(&devp->lock);

		return 1;
	}

	if (cmd->msg_len <= 0) {
		mutex_unlock(&devp->lock);

		return -EINVAL;
	}

	for (i = 0; i < cmd->msg_len; i++)
		dprintk(1, "0x%02X\n", cmd->msg[i]);

	ret = aml_diseqc_send_cmd(fe, cmd);

	mutex_unlock(&devp->lock);

	return ret;
}

u32 aml_diseqc_recv_cmd(unsigned char *buf, int len)
{
	unsigned long ret = 0;
	unsigned int size = 0, timeout = 0, rx_ready = 0;
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	mutex_lock(&lnb->demod->mutex_rx_msg);

	reinit_completion(&lnb->rx_msg_ok);

	timeout = msecs_to_jiffies(2000);

	rx_ready = dvbs2_diseqc_rx_check();
	if (rx_ready) {
		ret = wait_for_completion_timeout(&lnb->rx_msg_ok, timeout);
		if (ret <= 0) /* time out */
			dprintk(0, "recv cmd time out, ret %ld.\n", ret);
		size = dvbs2_diseqc_read_msg(len, buf);
	} else {
		ret = wait_for_completion_timeout(&lnb->rx_msg_ok, timeout);
		if (ret <= 0) /* time out */
			dprintk(0, "recv cmd time out, ret %ld.\n", ret);

		rx_ready = dvbs2_diseqc_rx_check();
		if (rx_ready)
			size = dvbs2_diseqc_read_msg(len, buf);
		else
			dprintk(1, "%s: dvbs2_diseqc_rx_check fail.\n",
					__func__);
	}

	if (lnb->sar_adc_enable) {
		//meson_sar_adc_diseqc_in_mode_disable();
		lnb->sar_adc_enable = false;
	}

	mutex_unlock(&lnb->demod->mutex_rx_msg);

	return size;
}

int aml_diseqc_recv_slave_reply(struct dvb_frontend *fe,
		struct dvb_diseqc_slave_reply *reply)
{
	int i = 0, size = 0;
	unsigned char buf[4] = { 0 };
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	if (diseqc_cmd_bypass) {
		mutex_unlock(&devp->lock);

		return 1;
	}

	size = aml_diseqc_recv_cmd(buf, sizeof(buf));
	reply->msg_len = 0;
	for (i = 0; i < size; ++i) {
		if (i < sizeof(reply->msg)) {
			reply->msg[i] = buf[i];
			reply->msg_len++;
		} else {
			break;
		}
	}

	dprintk(1, "%s: recv size %d, data: [0x%X 0x%X 0x%X 0x%X] done.\n",
			__func__, size, reply->msg[0], reply->msg[1],
			reply->msg[2], reply->msg[3]);

	dvbs2_diseqc_recv_en(false);
	dvbs2_diseqc_recv_irq_en(false);
	dvbs2_diseqc_reset();

	mutex_unlock(&devp->lock);

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
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	dprintk(1, "%s %d\n", __func__, (u32)arg);
	if (arg)
		aml_diseqc_set_voltage(fe, SEC_VOLTAGE_18);
	else
		aml_diseqc_set_voltage(fe, SEC_VOLTAGE_13);

	mutex_unlock(&devp->lock);

	return 0;
}

/*
 * callback function requesting that the Satelite Equipment
 *	Control (SEC) driver to release and free any memory
 *
 */
void aml_diseqc_release_sec(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		return;
	}

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
		fe->ops.diseqc_recv_slave_reply = aml_diseqc_recv_slave_reply;
		fe->ops.enable_high_lnb_voltage = aml_diseqc_en_high_lnb_voltage;
		fe->ops.diseqc_send_burst = aml_diseqc_send_burst;
		diseqc->lnb_en = 1;
		diseqc->lnb_sel = 1;
		diseqc->voltage = SEC_VOLTAGE_OFF;
		fe->dtv_property_cache.voltage = SEC_VOLTAGE_OFF;
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
			dprintk(2, "isr IRQGAPBURST\n");

		if (diseq_irq_sts & IRQ_STS_TXFIFO64B)
			dprintk(2, "isr IRQTXFIFO64B\n");

		if (diseq_irq_sts & IRQ_STS_TXEND) {
			complete(&lnb->tx_msg_ok);
			dprintk(2, "isr IRQTXEND\n");
		}

		if (diseq_irq_sts & IRQ_STS_TIMEOUT)
			dprintk(2, "isr IRQTIMEOUT\n");

		if (diseq_irq_sts & IRQ_STS_TRFINISH)
			dprintk(2, "isr IRQTRFINISH\n");

		if (diseq_irq_sts & IRQ_STS_RXFIFO8B)
			dprintk(2, "isr IRQRXFIFO8B\n");

		if (diseq_irq_sts & IRQ_STS_RXEND) {
			if (dvbs2_diseqc_rx_check() && lnb->sar_adc_enable)
				complete(&lnb->rx_msg_ok);
			dprintk(2, "isr IRQRXEND\n");
		}
	}
}

void aml_diseqc_status(void)
{
	struct aml_def_diseqc_lnb *lnb = &aml_lnb;

	dprintk(0, "AML_DISEQC_VER: %s.\n", AML_DISEQC_VER);
	dprintk(0, "diseqc_attached=%d\n", diseqc_attached);
	dprintk(0, "lnb_en=%d\n", lnb->lnb_en);
	dprintk(0, "lnb_sel=%d\n", lnb->lnb_sel);
	dprintk(0, "tone_on=%d\n", lnb->tone_on);
	dprintk(0, "voltage=%d\n", lnb->voltage);
}
