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
#ifdef DISEQC_SAR_ADC_RX
#include "meson_saradc.h"
#endif

static int diseqc_debug;

#define AML_DISEQC_VER		"20230418"
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
MODULE_PARM_DESC(lnb_high_voltage, "");

void aml_diseqc_dbg_en(unsigned int val)
{
	diseqc_debug = val;
}

static void aml_diseqc_enable_rx(struct aml_diseqc *diseqc, bool enable)
{
	if (IS_ERR_OR_NULL(diseqc->lnbc_enable_rx)) {
		dprintk(0, "rx gpio error\n");
		return;
	}

	if (enable)
		gpiod_set_value(diseqc->lnbc_enable_rx, 0);
	else
		gpiod_set_value(diseqc->lnbc_enable_rx, 1);

	dprintk(1, "%s: %d\n", __func__, enable);
}

void aml_diseqc_toneburst_sa(void)
{
	/* tone burst a is 12.5ms continuous tone*/
	dvbs2_diseqc_continuous_tone(true);
	usleep_range(15000, 16000);
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

int aml_diseqc_set_lnb_voltage(struct aml_diseqc *diseqc,
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

static int aml_diseqc_send_cmd(struct aml_diseqc *diseqc,
		struct dvb_diseqc_master_cmd *cmd)
{
	int i = 0, ret = 0;
	unsigned long timeout = 0, time_left = 0;

	mutex_lock(&diseqc->mutex_tx_msg);

	reinit_completion(&diseqc->tx_msg_ok);

	timeout = msecs_to_jiffies(2000);
	if (cmd->msg_len) {
		dvbs2_diseqc_send_msg(cmd->msg_len, cmd->msg);
	} else {
		mutex_unlock(&diseqc->mutex_tx_msg);

		return -ENODATA;
	}

	time_left = wait_for_completion_timeout(&diseqc->tx_msg_ok, timeout);
	if (time_left <= 0) /* time out */
		dprintk(0, "send cmd time out, time_left %ld\n", time_left);

	dvbs2_diseqc_send_irq_en(false);

	/* backup for filtering rx data */
	diseqc->send_cmd.msg_len = cmd->msg_len;
	for (i = 0; i < cmd->msg_len && i < sizeof(diseqc->send_cmd.msg); ++i)
		diseqc->send_cmd.msg[i] = cmd->msg[i];

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
		dprintk(1, "uninited\n");

		mutex_unlock(&devp->lock);

		return 0;
	}

	dprintk(0, "burst-%d\n", minicmd);

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
		dprintk(1, "uninited\n");

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
			msleep(23);
		break;

	case SEC_TONE_ON:
		aml_diseqc_tone_on(diseqc, true);

		if (diseqc->voltage != SEC_VOLTAGE_OFF)
			msleep(DISEQC_EN_ON_DELAY);
		break;

	default:
		ret = -EINVAL;
	}

	dprintk(1, "%s: %d ret %d\n", __func__, tone, ret);

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
		dprintk(1, "uninited\n");

		mutex_unlock(&devp->lock);

		return 0;
	}

	ret = aml_diseqc_set_lnb_voltage(diseqc, voltage);

	c->voltage = voltage;

	dprintk(1, "%s: %d ret %d\n", __func__, voltage, ret);

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_diseqc_get_reply_msg(struct aml_diseqc *diseqc)
{
	int i = 0, ret = 0;
	unsigned long time_left = 0, timeout = 0;
	unsigned int len = 0;

	mutex_lock(&diseqc->mutex_rx_msg);

	if (!diseqc->rx_enable) {
		diseqc->rx_enable = true;
		aml_diseqc_enable_rx(diseqc, true);
		if (is_meson_s4d_cpu()) {
#ifdef DISEQC_SAR_ADC_RX
			meson_sar_adc_diseqc_in_mode_enable();
#endif
		}
	}

	dvbs2_diseqc_recv_en(true);
	dvbs2_diseqc_recv_irq_en(true);

	reinit_completion(&diseqc->rx_msg_ok);
	timeout = msecs_to_jiffies(1000);

	diseqc->reply_len = 0;

	len = dvbs2_diseqc_rx_check();
	if (len) {
		time_left = wait_for_completion_timeout(&diseqc->rx_msg_ok, timeout);
		if (time_left <= 0) /* time out */
			dprintk(0, "recv cmd time out, time_left %ld\n", time_left);

		diseqc->reply_len = dvbs2_diseqc_read_msg(sizeof(diseqc->reply_msg),
				diseqc->reply_msg);
	} else {
		time_left = wait_for_completion_timeout(&diseqc->rx_msg_ok, timeout);
		if (time_left <= 0) /* time out */
			dprintk(0, "recv cmd time out, time_left %ld\n", time_left);

		len = dvbs2_diseqc_rx_check();
		if (len)
			diseqc->reply_len = dvbs2_diseqc_read_msg(sizeof(diseqc->reply_msg),
					diseqc->reply_msg);
		else
			dprintk(1, "rx_check fail\n");
	}

	usleep_range(5000, 6000);

	dvbs2_diseqc_recv_irq_en(false);
	dvbs2_diseqc_recv_en(false);
	dvbs2_diseqc_reset();

	if (diseqc->rx_enable) {
		diseqc->rx_enable = false;
		if (is_meson_s4d_cpu()) {
#ifdef DISEQC_SAR_ADC_RX
			meson_sar_adc_diseqc_in_mode_disable();
#endif
		}
		aml_diseqc_enable_rx(diseqc, false);
	}

	for (i = 0; i < diseqc->reply_len; ++i)
		dprintk(1, "reply_msg[%d]: 0x%02X\n",
				i, diseqc->reply_msg[i]);

	mutex_unlock(&diseqc->mutex_rx_msg);

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
	int ret = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct aml_diseqc *diseqc = &devp->diseqc;
	bool tone = diseqc->tone_on;
	bool unicable_cmd = false;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "uninited\n");

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

	/* disable continuous tone */
	if (tone) {
		aml_diseqc_tone_on(diseqc, false);
		usleep_range(15000, 16000);
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

		usleep_range(4000, 5000);
	}

	dprintk(1, "send len %d, msg: [0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]\n",
		cmd->msg_len, cmd->msg[0], cmd->msg[1],
		cmd->msg[2], cmd->msg[3], cmd->msg[4], cmd->msg[5]);

	ret = aml_diseqc_send_cmd(diseqc, cmd);

	if (unicable_cmd) {
		usleep_range(2000, 3000);

		aml_diseqc_set_lnb_voltage(diseqc, SEC_VOLTAGE_13);
	}

	/* diseqc2.0 with reply. */
	if (!ret && (cmd->msg[0] == 0xE2 || cmd->msg[0] == 0xE3 ||
		(0x7A <= cmd->msg[0] && 0x7E >= cmd->msg[0]))) {
		if (0x7A <= cmd->msg[0] && 0x7E >= cmd->msg[0])
			usleep_range(15000, 16000);
		else
			usleep_range(5000, 6000);
		ret = aml_diseqc_get_reply_msg(diseqc);
	}

	/* Send burst SA or SB */
	if (sendburst_on && cmd->msg_len == 4 && cmd->msg[2] == 0x38 &&
		cmd->msg[3] >= 0xf0) {
		usleep_range(15000, 16000);
		if ((cmd->msg[3] >= 0xf0 && cmd->msg[3] <= 0xf3) &&
			(cmd->msg[3] >= 0xf8 && cmd->msg[3] <= 0xfb))
			aml_diseqc_toneburst_sa();
		else
			aml_diseqc_toneburst_sb();

		dprintk(1, "burst\n");
	}

	/* Is tone on, need set tone on */
	if (tone) {
		usleep_range(15000, 16000);
		aml_diseqc_tone_on(diseqc, true);
	}

	fe->dtv_property_cache.voltage = diseqc->voltage;

	dprintk(0, "unicable:%d, burst:%d, tone: %d, voltage:%d\n",
		unicable_cmd, sendburst_on, tone, diseqc->voltage);

	mutex_unlock(&devp->lock);

	return ret;
}

int aml_diseqc_recv_slave_reply(struct dvb_frontend *fe,
		struct dvb_diseqc_slave_reply *reply)
{
	int i = 0, j = 0;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct aml_diseqc *diseqc = &devp->diseqc;

	mutex_lock(&devp->lock);

	if (!demod->inited) {
		dprintk(1, "uninited\n");

		mutex_unlock(&devp->lock);

		return 0;
	}

	if (diseqc_cmd_bypass) {
		mutex_unlock(&devp->lock);

		return 1;
	}

	if (diseqc->reply_len > diseqc->send_cmd.msg_len) {
		for (i = 0; i < diseqc->send_cmd.msg_len; ++i) {
			if (diseqc->send_cmd.msg[i] != diseqc->reply_msg[i])
				break;
		}

		if (i == diseqc->send_cmd.msg_len)
			j = diseqc->send_cmd.msg_len;
		else
			j = 0;
	}

	reply->msg_len = 0;
	for (i = 0; i < diseqc->reply_len; ++i) {
		if (i < (sizeof(reply->msg) + j) && i >= j) {
			reply->msg[reply->msg_len] = diseqc->reply_msg[i];
			reply->msg_len++;
		} else {
			break;
		}
	}

	dprintk(0, "recv len %d, msg: [0x%02X 0x%02X 0x%02X 0x%02X]\n",
			diseqc->reply_len, reply->msg[0], reply->msg[1],
			reply->msg[2], reply->msg[3]);

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
		dprintk(1, "uninited\n");

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
		dprintk(1, "uninited\n");

		return;
	}

	aml_diseqc_set_voltage(fe, SEC_VOLTAGE_OFF);
	dprintk(1, "ok\n");
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

	fe->ops.release_sec = aml_diseqc_release_sec;
	fe->ops.set_tone = aml_diseqc_set_tone;
	fe->ops.set_voltage = aml_diseqc_set_voltage;
	fe->ops.diseqc_send_master_cmd = aml_diseqc_send_master_cmd;
	fe->ops.diseqc_recv_slave_reply = aml_diseqc_recv_slave_reply;
	fe->ops.enable_high_lnb_voltage = aml_diseqc_en_high_lnb_voltage;
	fe->ops.diseqc_send_burst = aml_diseqc_send_burst;

	fe->dtv_property_cache.voltage = SEC_VOLTAGE_OFF;
	fe->dtv_property_cache.sectone = SEC_TONE_OFF;

	if (diseqc->attached) {
		dprintk(0, "had attached\n");
		return;
	}

	init_completion(&diseqc->tx_msg_ok);
	init_completion(&diseqc->rx_msg_ok);

	diseqc->voltage = SEC_VOLTAGE_OFF;
	diseqc->tone_on = 0;
	diseqc->rx_enable = false;

	diseqc->lnbc_enable_rx = devm_gpiod_get(dev, "diseqc_rx", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(diseqc->lnbc_enable_rx)) {
		diseqc->lnbc_enable_rx = NULL;
		dprintk(0, "get diseqc_rx gpio fail\n");
	}

	if (!strcmp(diseqc->name, "wt20_1811")) {
		node = of_parse_phandle(dev->of_node, "lnbc_i2c_adap", 0);
		if (!IS_ERR_OR_NULL(node)) {
			i2c_adap = of_find_i2c_adapter_by_node(node);
			of_node_put(node);
			if (IS_ERR_OR_NULL(i2c_adap)) {
				i2c_adap = NULL;
				dprintk(0, "get lnbc i2c adapter fail\n");
			}
		}

		if (of_property_read_u32(dev->of_node, "lnbc_i2c_addr", &value)) {
			i2c_addr = 0;
			dprintk(0, "get lnbc i2c addr fail\n");
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
			dprintk(0, "get lnbc en gpio fail\n");
		}

		gpio_sel = devm_gpiod_get(dev, "lnb_sel", GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(gpio_sel)) {
			gpio_sel = NULL;
			dprintk(0, "get lnbc sel gpio fail\n");
		}

		ret = gpio_lnbc_create(&diseqc->lnbc, gpio_en, gpio_sel);

		if (diseqc->lnbc.init)
			diseqc->lnbc.init(&diseqc->lnbc);
	}

	dvbs2_diseqc_init();

	diseqc->attached = true;

	dprintk(0, "%s: flag %d, ret %d\n", __func__, diseqc->attached, ret);
}

void aml_diseqc_isr(struct aml_diseqc *diseqc)
{
	unsigned int diseq_irq_sts = 0;

	diseq_irq_sts = dvbs2_diseqc_irq_check();
	if (diseq_irq_sts) {
		if (diseq_irq_sts & IRQ_STS_GAPBURST)
			dprintk(2, "IRQGAPBURST\n");

		if (diseq_irq_sts & IRQ_STS_TXFIFO64B)
			dprintk(2, "IRQTXFIFO64B\n");

		if (diseq_irq_sts & IRQ_STS_TXEND) {
			complete(&diseqc->tx_msg_ok);
			dprintk(2, "IRQTXEND\n");
		}

		if (diseq_irq_sts & IRQ_STS_TIMEOUT)
			dprintk(2, "IRQTIMEOUT\n");

		if (diseq_irq_sts & IRQ_STS_TRFINISH)
			dprintk(2, "IRQTRFINISH\n");

		if (diseq_irq_sts & IRQ_STS_RXFIFO8B)
			dprintk(2, "IRQRXFIFO8B\n");

		if (diseq_irq_sts & IRQ_STS_RXEND) {
			if (dvbs2_diseqc_rx_check() && diseqc->rx_enable)
				complete(&diseqc->rx_msg_ok);
			dprintk(2, "IRQRXEND\n");
		}
	}
}

void aml_diseqc_status(struct aml_diseqc *diseqc)
{
	dprintk(0, "AML_DISEQC_VER: %s\n", AML_DISEQC_VER);
	dprintk(0, "diseqc name: %s\n", diseqc->name);
	dprintk(0, "irq_num: %d\n", diseqc->irq_num);
	dprintk(0, "irq_en: %d\n", diseqc->irq_en);
	dprintk(0, "attached: %d\n", diseqc->attached);
	dprintk(0, "tone_on: %d\n", diseqc->tone_on);
	dprintk(0, "voltage: %d\n", diseqc->voltage);
	dprintk(0, "gpio_lnb_en: %p\n", diseqc->lnbc.gpio_lnb_en);
	dprintk(0, "gpio_lnb_sel: %p\n", diseqc->lnbc.gpio_lnb_sel);
	dprintk(0, "lnbc_enable_rx: %p\n", diseqc->lnbc_enable_rx);
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
		dprintk(0, "demod NULL\n");
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
			dprintk(0, "on diseqc_irq\n");
		}
	} else {
		if (diseqc->irq_en && diseqc->irq_num > 0) {
			disable_irq(diseqc->irq_num);
			diseqc->irq_en = false;
			dprintk(0, "off diseqc_irq\n");
		}
	}
}
