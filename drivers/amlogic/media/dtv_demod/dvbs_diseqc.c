// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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

#include "demod_func.h"
#include "dvbs.h"
#include "dvbs_diseqc.h"
#include "gpio_lnbc.h"
#include "wt20_1811.h"
//#include "meson_saradc.h"

static int diseqc_debug;

#define AML_DISEQC_VER		"20221111"
#define DISEQC_EN_ON_DELAY	60

#define dprintk(level, fmt, arg...)				\
	do {							\
		if (diseqc_debug >= (level))			\
			pr_info("diseqc: " fmt, ## arg);	\
	} while (0)

u32 sendburst_on;
u32 diseqc_cmd_bypass;

int lnb_high_voltage;
module_param(lnb_high_voltage, int, 0644);
MODULE_PARM_DESC(lnb_high_voltage, "\nlnb_high_voltage\n");

void aml_diseqc_dbg_en(unsigned int val)
{
	diseqc_debug = val;
}

void aml_diseqc_toneburst_sa(void)
{
	/* tone burst a is 12.5ms continus tone*/
	dvbs2_diseqc_continuous_tone(true);
	mdelay(13);
	dvbs2_diseqc_continuous_tone(false);
}

void aml_diseqc_toneburst_sb(void)
{
	struct dvb_diseqc_master_cmd cmd;

	cmd.msg_len = 1;
	cmd.msg[0] = 0xff;
	cmd.msg[1] = 0xff;
	dvbs2_diseqc_send_msg(cmd.msg_len, &cmd.msg[0]);
}

void aml_diseqc_tone_on(struct aml_diseqc *diseqc, bool onoff)
{
	dvbs2_diseqc_continuous_tone(onoff);

	diseqc->tone_on = onoff;
}

static int aml_diseqc_set_lnb_voltage(struct aml_diseqc *diseqc,
		enum fe_sec_voltage voltage)
{
	int ret = 0;

	if (diseqc->voltage == voltage)
		return 0;

	switch (voltage) {
	case SEC_VOLTAGE_OFF:
		if (diseqc->lnbc.set_voltage)
			ret = diseqc->lnbc.set_voltage(&diseqc->lnbc, LNBC_VOLTAGE_OFF);
		break;
	case SEC_VOLTAGE_13:
		if (diseqc->lnbc.set_voltage)
			ret = diseqc->lnbc.set_voltage(&diseqc->lnbc, LNBC_VOLTAGE_LOW);
		break;
	case SEC_VOLTAGE_18:
		if (diseqc->lnbc.set_voltage)
			ret = diseqc->lnbc.set_voltage(&diseqc->lnbc, LNBC_VOLTAGE_HIGH);
		break;
	default:
		return -EINVAL;
	}

	diseqc->voltage = voltage;

	return ret;
}

u32 aml_diseqc_send_cmd(struct aml_diseqc *diseqc,
		struct dvb_diseqc_master_cmd *cmd)
{
	unsigned int timeout;
	unsigned long ret = 0;
	bool tone = diseqc->tone_on;
	bool unicable_cmd = false;

	mutex_lock(&diseqc->mutex_tx_msg);

	reinit_completion(&diseqc->tx_msg_ok);

	/* disable continuous tone */
	if (tone) {
		aml_diseqc_tone_on(diseqc, false);
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
		if (diseqc->voltage != SEC_VOLTAGE_18)
			aml_diseqc_set_lnb_voltage(diseqc, SEC_VOLTAGE_18);

		mdelay(10);
	}

	/* diseqc2.0 with reply. */
	if (cmd->msg[0] == 0xE2 || cmd->msg[0] == 0xE3 ||
		(0x7A <= cmd->msg[0] && 0x7E >= cmd->msg[0])) {
		if (!diseqc->sar_adc_enable) {
			//meson_sar_adc_diseqc_in_mode_enable();
			//diseqc->sar_adc_enable = true;
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

	ret = wait_for_completion_timeout(&diseqc->tx_msg_ok, timeout);
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

		aml_diseqc_set_lnb_voltage(diseqc, SEC_VOLTAGE_13);
	}

	/* Is tone on, need set tone on */
	if (tone) {
		mdelay(16);
		aml_diseqc_tone_on(diseqc, true);
	}

	dvbs2_diseqc_send_irq_en(false);

	if (diseqc->sar_adc_enable) {
		dvbs2_diseqc_recv_en(true);
		dvbs2_diseqc_recv_irq_en(true);
	}

	dprintk(0, "%s unicable_cmd:%d, burst_on:%d done.\n",
			__func__, unicable_cmd, sendburst_on);

	mutex_unlock(&diseqc->mutex_tx_msg);

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
	struct aml_diseqc *diseqc = &devp->diseqc;

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

	switch (tone) {
	case SEC_TONE_OFF:
		aml_diseqc_tone_on(diseqc, false);

		if (diseqc->voltage != SEC_VOLTAGE_OFF)
			mdelay(23);
		break;

	case SEC_TONE_ON:
		aml_diseqc_tone_on(diseqc, true);

		if (diseqc->voltage != SEC_VOLTAGE_OFF)
			mdelay(DISEQC_EN_ON_DELAY);
		break;

	default:
		ret = -EINVAL;
	}

	dprintk(1, "%s: tone %d, ret %d.\n", __func__, tone, ret);

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
	struct aml_diseqc *diseqc = &devp->diseqc;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	ret = aml_diseqc_set_lnb_voltage(diseqc, voltage);

	c->voltage = voltage;

	dprintk(1, "%s: voltage %d, ret %d.\n", __func__, voltage, ret);

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
	struct aml_diseqc *diseqc = &devp->diseqc;

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

	ret = aml_diseqc_send_cmd(diseqc, cmd);

	fe->dtv_property_cache.voltage = diseqc->voltage;

	mutex_unlock(&devp->lock);

	return ret;
}

u32 aml_diseqc_recv_cmd(struct aml_diseqc *diseqc, unsigned char *buf, int len)
{
	unsigned long ret = 0;
	unsigned int size = 0, timeout = 0, rx_ready = 0;

	mutex_lock(&diseqc->mutex_rx_msg);

	reinit_completion(&diseqc->rx_msg_ok);

	timeout = msecs_to_jiffies(2000);

	rx_ready = dvbs2_diseqc_rx_check();
	if (rx_ready) {
		ret = wait_for_completion_timeout(&diseqc->rx_msg_ok, timeout);
		if (ret <= 0) /* time out */
			dprintk(0, "recv cmd time out, ret %ld.\n", ret);
		size = dvbs2_diseqc_read_msg(len, buf);
	} else {
		ret = wait_for_completion_timeout(&diseqc->rx_msg_ok, timeout);
		if (ret <= 0) /* time out */
			dprintk(0, "recv cmd time out, ret %ld.\n", ret);

		rx_ready = dvbs2_diseqc_rx_check();
		if (rx_ready)
			size = dvbs2_diseqc_read_msg(len, buf);
		else
			dprintk(1, "%s: dvbs2_diseqc_rx_check fail.\n",
					__func__);
	}

	if (diseqc->sar_adc_enable) {
		//meson_sar_adc_diseqc_in_mode_disable();
		diseqc->sar_adc_enable = false;
	}

	mutex_unlock(&diseqc->mutex_rx_msg);

	return size;
}

int aml_diseqc_recv_slave_reply(struct dvb_frontend *fe,
		struct dvb_diseqc_slave_reply *reply)
{
	int i = 0, size = 0;
	unsigned char buf[4] = { 0 };
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct aml_diseqc *diseqc = &devp->diseqc;

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

	size = aml_diseqc_recv_cmd(diseqc, buf, sizeof(buf));
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
int aml_diseqc_en_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "%s: demod uninited.\n", __func__);

		mutex_unlock(&devp->lock);

		return 0;
	}

	dprintk(1, "%s %d\n", __func__, (u32)arg);

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

void aml_diseqc_attach(struct device *dev, struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct gpio_desc *gpio_en = NULL, *gpio_sel = NULL;
	struct device_node *node = NULL;
	struct i2c_adapter *i2c_adap = NULL;
	unsigned char i2c_addr = 0;
	u32 value = 0;
	int ret = 0;
	struct aml_diseqc *diseqc = &devp->diseqc;

	if (diseqc->attached) {
		dprintk(0, "diseqc had attached\n");
		return;
	}

	init_completion(&diseqc->tx_msg_ok);
	init_completion(&diseqc->rx_msg_ok);

	fe->ops.release_sec = aml_diseqc_release_sec;
	fe->ops.set_tone = aml_diseqc_set_tone;
	fe->ops.set_voltage = aml_diseqc_set_voltage;
	fe->ops.diseqc_send_master_cmd = aml_diseqc_send_master_cmd;
	fe->ops.diseqc_recv_slave_reply = aml_diseqc_recv_slave_reply;
	fe->ops.enable_high_lnb_voltage = aml_diseqc_en_high_lnb_voltage;
	fe->ops.diseqc_send_burst = aml_diseqc_send_burst;

	fe->dtv_property_cache.voltage = SEC_VOLTAGE_OFF;
	fe->dtv_property_cache.sectone = SEC_TONE_OFF;
	diseqc->voltage = SEC_VOLTAGE_OFF;
	diseqc->tone_on = 0;

	if (!strcmp(diseqc->name, "wt20_1811")) {
		node = of_parse_phandle(dev->of_node, "lnbc_i2c_adap", 0);
		if (!IS_ERR_OR_NULL(node)) {
			i2c_adap = of_find_i2c_adapter_by_node(node);
			of_node_put(node);
			if (IS_ERR_OR_NULL(i2c_adap)) {
				i2c_adap = NULL;
				dprintk(0, "get lnbc i2c adapter fail.\n");
			}
		}

		if (of_property_read_u32(dev->of_node, "lnbc_i2c_addr", &value)) {
			i2c_addr = 0;
			dprintk(0, "get lnbc i2c addr fail.\n");
		} else {
			i2c_addr = value;
		}

		ret = wt20_1811_create(&diseqc->lnbc, i2c_adap, i2c_addr);

		if (diseqc->lnbc.init)
			diseqc->lnbc.init(&diseqc->lnbc);
	} else {
		gpio_en = devm_gpiod_get(dev, "lnb_en", GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(gpio_en)) {
			gpio_en = NULL;
			dprintk(0, "get lnbc en gpio fail.\n");
		}

		gpio_sel = devm_gpiod_get(dev, "lnb_sel", GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(gpio_sel)) {
			gpio_sel = NULL;
			dprintk(0, "get lnbc sel gpio fail.\n");
		}

		ret = gpio_lnbc_create(&diseqc->lnbc, gpio_en, gpio_sel);

		if (diseqc->lnbc.init)
			diseqc->lnbc.init(&diseqc->lnbc);
	}

	dvbs2_diseqc_init();

	diseqc->attached = true;

	dprintk(0, "%s: flag %d, ret %d.\n", __func__, diseqc->attached, ret);
}

void aml_diseqc_isr(struct aml_diseqc *diseqc)
{
	unsigned int diseq_irq_sts = 0;

	diseq_irq_sts = dvbs2_diseqc_irq_check();
	if (diseq_irq_sts) {
		if (diseq_irq_sts & IRQ_STS_GAPBURST)
			dprintk(2, "isr IRQGAPBURST\n");

		if (diseq_irq_sts & IRQ_STS_TXFIFO64B)
			dprintk(2, "isr IRQTXFIFO64B\n");

		if (diseq_irq_sts & IRQ_STS_TXEND) {
			complete(&diseqc->tx_msg_ok);
			dprintk(2, "isr IRQTXEND\n");
		}

		if (diseq_irq_sts & IRQ_STS_TIMEOUT)
			dprintk(2, "isr IRQTIMEOUT\n");

		if (diseq_irq_sts & IRQ_STS_TRFINISH)
			dprintk(2, "isr IRQTRFINISH\n");

		if (diseq_irq_sts & IRQ_STS_RXFIFO8B)
			dprintk(2, "isr IRQRXFIFO8B\n");

		if (diseq_irq_sts & IRQ_STS_RXEND) {
			if (dvbs2_diseqc_rx_check() && diseqc->sar_adc_enable)
				complete(&diseqc->rx_msg_ok);
			dprintk(2, "isr IRQRXEND\n");
		}
	}
}

void aml_diseqc_status(struct aml_diseqc *diseqc)
{
	dprintk(0, "AML_DISEQC_VER: %s.\n", AML_DISEQC_VER);
	dprintk(0, "diseqc name: %s.\n", diseqc->name);
	dprintk(0, "irq_num: %d.\n", diseqc->irq_num);
	dprintk(0, "irq_en: %d.\n", diseqc->irq_en);
	dprintk(0, "attached: %d.\n", diseqc->attached);
	dprintk(0, "tone_on: %d.\n", diseqc->tone_on);
	dprintk(0, "voltage: %d.\n", diseqc->voltage);
}

irqreturn_t aml_diseqc_isr_handler(int irq, void *data)
{
	struct amldtvdemod_device_s *devp = data;
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			break;
		}
	}

	if (!demod) {
		dprintk(0, "%s: demod == NULL.\n", __func__);
		return IRQ_NONE;
	}

	if (demod->last_delsys == SYS_DVBS || demod->last_delsys == SYS_DVBS2)
		aml_diseqc_isr(&devp->diseqc);

	return IRQ_HANDLED;
}

void aml_diseqc_isr_en(struct aml_diseqc *diseqc, bool en)
{
	if (en) {
		if (!diseqc->irq_en && diseqc->irq_num > 0) {
			enable_irq(diseqc->irq_num);
			diseqc->irq_en = true;
			dprintk(0, "enable diseqc_irq.\n");
		}
	} else {
		if (diseqc->irq_en && diseqc->irq_num > 0) {
			disable_irq(diseqc->irq_num);
			diseqc->irq_en = false;
			dprintk(0, "disable diseqc_irq.\n");
		}
	}
}
