/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
/*
 * AMLOGIC DVB frontend driver.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/dma-contiguous.h>
#include <linux/of_gpio.h>
/*add for gpio api by hualing.chen*/
#include <gpiolib.h>

#include "aml_fe.h"

#ifdef pr_dbg
#undef pr_dbg
#endif

#define pr_dbg(fmt, args ...) \
	pr_info("FE: " fmt, ## args)
#define pr_error(fmt, args ...) pr_err("FE: " fmt, ## args)
#define pr_inf(fmt, args ...) pr_info("FE: " fmt, ## args)

static DEFINE_SPINLOCK(lock);
static struct aml_fe_drv *tuner_drv_list;
static struct aml_fe_drv *atv_demod_drv_list;
static struct aml_fe_drv *dtv_demod_drv_list;
static struct aml_fe_man fe_man;
static long aml_fe_suspended;

static int aml_fe_set_sys(struct dvb_frontend *dev,
			enum fe_delivery_system sys);

static struct aml_fe_drv **aml_get_fe_drv_list(enum aml_fe_dev_type_t type)
{
	switch (type) {
	case AM_DEV_TUNER:
		return &tuner_drv_list;
	case AM_DEV_ATV_DEMOD:
		return &atv_demod_drv_list;
	case AM_DEV_DTV_DEMOD:
		return &dtv_demod_drv_list;
	default:
		return NULL;
	}
}



int amlogic_gpio_direction_output(unsigned int pin, int value,
				  const char *owner)
{
	gpio_direction_output(pin, value);
	return 0;
}
EXPORT_SYMBOL(amlogic_gpio_direction_output);
int amlogic_gpio_request(unsigned int pin, const char *label)
{
	return 0;
}
EXPORT_SYMBOL(amlogic_gpio_request);
int aml_register_fe_drv(enum aml_fe_dev_type_t type, struct aml_fe_drv *drv)
{
	if (drv) {
		struct aml_fe_drv **list = aml_get_fe_drv_list(type);
		unsigned long flags;

		spin_lock_irqsave(&lock, flags);

		drv->next = *list;
		*list = drv;

		drv->ref = 0;

		spin_unlock_irqrestore(&lock, flags);
	}

	return 0;
}
EXPORT_SYMBOL(aml_register_fe_drv);

int aml_unregister_fe_drv(enum aml_fe_dev_type_t type, struct aml_fe_drv *drv)
{
	int ret = 0;

	if (drv) {
		struct aml_fe_drv *pdrv, *pprev;
		struct aml_fe_drv **list = aml_get_fe_drv_list(type);
		unsigned long flags;

		spin_lock_irqsave(&lock, flags);

		if (!drv->ref) {
			for (pprev = NULL, pdrv = *list;
			     pdrv; pprev = pdrv, pdrv = pdrv->next) {
				if (pdrv == drv) {
					if (pprev)
						pprev->next = pdrv->next;
					else
						*list = pdrv->next;
					break;
				}
			}
		} else {
			pr_error("fe driver %d is inused\n", drv->id);
			ret = -1;
		}

		spin_unlock_irqrestore(&lock, flags);
	}

	return ret;
}
EXPORT_SYMBOL(aml_unregister_fe_drv);

static int aml_fe_support_sys(struct aml_fe *fe,
			enum fe_delivery_system sys)
{
	if (fe->dtv_demod
			&& fe->dtv_demod->drv
			&& fe->dtv_demod->drv->support) {
		if (!fe->dtv_demod->drv->support(fe->dtv_demod, sys))
			return 0;
	}

	if (fe->atv_demod
			&& fe->atv_demod->drv
			&& fe->atv_demod->drv->support) {
		if (!fe->atv_demod->drv->support(fe->atv_demod, sys))
			return 0;
	}

	if (fe->tuner
			&& fe->tuner->drv
			&& fe->tuner->drv->support) {
		if (!fe->tuner->drv->support(fe->tuner, sys))
			return 0;
	}

	return 1;
}
/*
 *#define DTV_START_BLIND_SCAN            71
 *#define DTV_CANCEL_BLIND_SCAN           72
 *#define DTV_BLIND_SCAN_MIN_FRE          73
 *#define DTV_BLIND_SCAN_MAX_FRE          74
 *#define DTV_BLIND_SCAN_MIN_SRATE        75
 *#define DTV_BLIND_SCAN_MAX_SRATE        76
 *#define DTV_BLIND_SCAN_FRE_RANGE        77
 *#define DTV_BLIND_SCAN_FRE_STEP         78
 *#define DTV_BLIND_SCAN_TIMEOUT          79
 */
static int aml_fe_blind_cmd(struct dvb_frontend *dev,
			struct dtv_property *tvp)
{
	struct aml_fe *fe;
	int ret = 0;

	fe = dev->demodulator_priv;
	pr_error("fe blind cmd into cmd:[%d]\n", tvp->cmd);
	switch (tvp->cmd) {
	case DTV_START_BLIND_SCAN:
		if (fe->dtv_demod
			&& fe->dtv_demod->drv
			&& fe->dtv_demod->drv->start_blind_scan) {
			ret = fe->dtv_demod->drv->start_blind_scan(
					fe->dtv_demod);
		} else {
			pr_error("fe dtv_demod not surport blind start\n");
		}
		break;
	case DTV_CANCEL_BLIND_SCAN:
		if (fe->dtv_demod
			&& fe->dtv_demod->drv
			&& fe->dtv_demod->drv->stop_blind_scan) {
			ret = fe->dtv_demod->drv->stop_blind_scan(
				fe->dtv_demod);
		} else {
			pr_error("fe dtv_demod not surport blind stop\n");
		}
		break;
	case DTV_BLIND_SCAN_MIN_FRE:
		fe->blind_scan_para.minfrequency = tvp->u.data;
		break;
	case DTV_BLIND_SCAN_MAX_FRE:
		fe->blind_scan_para.maxfrequency = tvp->u.data;
		break;
	case DTV_BLIND_SCAN_MIN_SRATE:
		fe->blind_scan_para.minSymbolRate = tvp->u.data;
		break;
	case DTV_BLIND_SCAN_MAX_SRATE:
		fe->blind_scan_para.maxSymbolRate = tvp->u.data;
		break;
	case DTV_BLIND_SCAN_FRE_RANGE:
		fe->blind_scan_para.frequencyRange = tvp->u.data;
		break;
	case DTV_BLIND_SCAN_FRE_STEP:
		fe->blind_scan_para.frequencyStep = tvp->u.data;
		break;
	case DTV_BLIND_SCAN_TIMEOUT:
		fe->blind_scan_para.timeout = tvp->u.data;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int aml_fe_set_property(struct dvb_frontend *dev,
			struct dtv_property *tvp)
{
	struct aml_fe *fe;
	int r = 0;

	fe = dev->demodulator_priv;

	if (tvp->cmd == DTV_DELIVERY_SYSTEM) {
		enum fe_delivery_system sys = tvp->u.data;

		pr_error("fe aml_fe_set_property %d\n", sys);
		r = aml_fe_set_sys(dev, sys);
		if (r < 0)
			return r;
	}

	if (tvp->cmd == DTV_DELIVERY_SUB_SYSTEM) {
		int sub_sys = tvp->u.data;

		pr_error("fe aml_fe_set_property sub_sys: %d\n", sub_sys);
		fe->sub_sys = sub_sys;
		r = 0;
	}
	pr_error("fe aml_fe_set_property -tvp->cmd[%d]\n", tvp->cmd);
	switch (tvp->cmd) {
	case DTV_START_BLIND_SCAN:
	case DTV_CANCEL_BLIND_SCAN:
	case DTV_BLIND_SCAN_MIN_FRE:
	case DTV_BLIND_SCAN_MAX_FRE:
	case DTV_BLIND_SCAN_MIN_SRATE:
	case DTV_BLIND_SCAN_MAX_SRATE:
	case DTV_BLIND_SCAN_FRE_RANGE:
	case DTV_BLIND_SCAN_FRE_STEP:
	case DTV_BLIND_SCAN_TIMEOUT:
		r = aml_fe_blind_cmd(dev, tvp);
		if (r < 0)
			return r;
	default:
		break;
	}

	if (fe->set_property) {
		pr_error("fe fe->set_property -0\n");
		return fe->set_property(dev, tvp);
	}
	pr_error("fe aml_fe_set_property -2\n");
	return r;
}

static int aml_fe_get_property(struct dvb_frontend *dev,
			struct dtv_property *tvp)
{
	struct aml_fe *fe;
	int r = 0;

	fe = dev->demodulator_priv;

	if (tvp->cmd == DTV_TS_INPUT)
		tvp->u.data = fe->ts;
	if (tvp->cmd == DTV_DELIVERY_SUB_SYSTEM) {
		tvp->u.data = fe->sub_sys;
		pr_error("fe aml_fe_get_property sub_sys: %d\n", fe->sub_sys);
		r = 0;
	}
	if (fe->get_property)
		return fe->get_property(dev, tvp);

	return r;
}

static int aml_fe_set_sys(struct dvb_frontend *dev,
			enum fe_delivery_system sys)
{
	struct aml_fe *fe = dev->demodulator_priv;
	unsigned long flags;
	int ret = -1;

	if (fe->sys == sys) {
		pr_dbg("[%s]:the mode is not change!!!!\n", __func__);
		return 0;
	}
	/*set dvb-t or dvb-t2
	 * if dvb-t or t2 is set
	 * we only set sys value, not init sys
	 */
	if (fe->sys != SYS_UNDEFINED) {
		pr_dbg("release system %d\n", fe->sys);

		if (fe->dtv_demod
				&& fe->dtv_demod->drv
				&& fe->dtv_demod->drv->release_sys)
			fe->dtv_demod->drv->release_sys(fe->dtv_demod, fe->sys);
		if (fe->atv_demod
				&& fe->atv_demod->drv
				&& fe->atv_demod->drv->release_sys)
			fe->atv_demod->drv->release_sys(fe->atv_demod, fe->sys);
		if (fe->tuner
				&& fe->tuner->drv
				&& fe->tuner->drv->release_sys)
			fe->tuner->drv->release_sys(fe->tuner, fe->sys);

		fe->set_property = NULL;
		fe->get_property = NULL;

		fe->sys = SYS_UNDEFINED;
	}

	if (sys == SYS_UNDEFINED)
		return 0;

	if (!aml_fe_support_sys(fe, sys)) {
		int i;

		spin_lock_irqsave(&lock, flags);
		for (i = 0; i < FE_DEV_COUNT; i++) {
			if (aml_fe_support_sys(&fe_man.fe[i], sys)
					&& (fe_man.fe[i].dev_id == fe->dev_id))
				break;
		}
		spin_unlock_irqrestore(&lock, flags);

		if (i >= FE_DEV_COUNT) {
			pr_error("do not support delivery system %d\n", sys);
			return -1;
		}

		fe = &fe_man.fe[i];
		dev->demodulator_priv = fe;
	}

	spin_lock_irqsave(&fe->slock, flags);

	memset(&fe->fe->ops.tuner_ops, 0, sizeof(fe->fe->ops.tuner_ops));
	memset(&fe->fe->ops.analog_ops, 0, sizeof(fe->fe->ops.analog_ops));
	memset(&fe->fe->ops.info, 0, sizeof(fe->fe->ops.info));
	fe->fe->ops.release = NULL;
	fe->fe->ops.release_sec = NULL;
	fe->fe->ops.init = NULL;
	fe->fe->ops.sleep = NULL;
	fe->fe->ops.write = NULL;
	fe->fe->ops.tune = NULL;
	fe->fe->ops.get_frontend_algo = NULL;
	fe->fe->ops.set_frontend = NULL;
	fe->fe->ops.get_tune_settings = NULL;
	fe->fe->ops.get_frontend = NULL;
	fe->fe->ops.read_status = NULL;
	fe->fe->ops.read_ber = NULL;
	fe->fe->ops.read_signal_strength = NULL;
	fe->fe->ops.read_snr = NULL;
	fe->fe->ops.read_ucblocks = NULL;
	fe->fe->ops.diseqc_reset_overload = NULL;
	fe->fe->ops.diseqc_send_master_cmd = NULL;
	fe->fe->ops.diseqc_recv_slave_reply = NULL;
	fe->fe->ops.diseqc_send_burst = NULL;
	fe->fe->ops.set_tone = NULL;
	fe->fe->ops.set_voltage = NULL;
	fe->fe->ops.enable_high_lnb_voltage = NULL;
	fe->fe->ops.dishnetwork_send_legacy_command = NULL;
	fe->fe->ops.i2c_gate_ctrl = NULL;
	fe->fe->ops.ts_bus_ctrl = NULL;
	fe->fe->ops.search = NULL;
	fe->fe->ops.set_property = NULL;
	fe->fe->ops.get_property = NULL;

	if (fe->tuner
			&& fe->tuner->drv
			&& fe->tuner->drv->get_ops)
		fe->tuner->drv->get_ops(fe->tuner, sys, &fe->fe->ops);

	if (fe->atv_demod
			&& fe->atv_demod->drv
			&& fe->atv_demod->drv->get_ops)
		fe->atv_demod->drv->get_ops(fe->atv_demod, sys, &fe->fe->ops);

	if (fe->dtv_demod
			&& fe->dtv_demod->drv
			&& fe->dtv_demod->drv->get_ops)
		fe->dtv_demod->drv->get_ops(fe->dtv_demod, sys, &fe->fe->ops);

	spin_unlock_irqrestore(&fe->slock, flags);

	pr_dbg("init system %d\n", sys);

	if (fe->dtv_demod
			&& fe->dtv_demod->drv
			&& fe->dtv_demod->drv->init_sys)
		ret = fe->dtv_demod->drv->init_sys(fe->dtv_demod, sys);
	if (fe->atv_demod
			&& fe->atv_demod->drv
			&& fe->atv_demod->drv->init_sys)
		ret = fe->atv_demod->drv->init_sys(fe->atv_demod, sys);
	if (fe->tuner
			&& fe->tuner->drv
			&& fe->tuner->drv->init_sys)
		ret = fe->tuner->drv->init_sys(fe->tuner, sys);

	if (ret != 0) {
		pr_error("init system, %d fail, ret %d\n", sys, ret);
		goto end;
	}

	fe->set_property = fe->fe->ops.set_property;
	fe->get_property = fe->fe->ops.get_property;

	strcpy(fe->fe->ops.info.name, "amlogic dvb frontend");
	fe->sys = sys;
	pr_dbg("set mode ok\n");

end:
	fe->fe->ops.set_property = aml_fe_set_property;
	fe->fe->ops.get_property = aml_fe_get_property;

	return 0;
}

static const char *aml_fe_dev_type_str(struct aml_fe_dev *dev)
{
	switch (dev->type) {
	case AM_DEV_TUNER:
		return "tuner";
	case AM_DEV_ATV_DEMOD:
		return "atv_demod";
	case AM_DEV_DTV_DEMOD:
		return "dtv_demod";
	}

	return "";
}

static void aml_fe_property_name(struct aml_fe_dev *dev, const char *name,
			char *buf)
{
	const char *tstr;

	tstr = aml_fe_dev_type_str(dev);

	if (name)
		sprintf(buf, "%s%d_%s", tstr, dev->dev_id, name);
	else
		sprintf(buf, "%s%d", tstr, dev->dev_id);
}

int aml_fe_of_property_string(struct aml_fe_dev *dev,
			const char *name, const char **str)
{
	struct platform_device *pdev = dev->man->pdev;
	//char   buf[128];
	int    r;

	//aml_fe_property_name(dev, name, buf);
	pr_error("start find resource: \"%s\" --\n", name);
	r = of_property_read_string(pdev->dev.of_node, name, str);
	if (r)
		pr_error("cannot find resource: \"%s\"\n", name);

	return r;
}
EXPORT_SYMBOL(aml_fe_of_property_string);

int aml_fe_of_property_u32(struct aml_fe_dev *dev,
			const char *name, u32 *v)
{
	struct platform_device *pdev = dev->man->pdev;
	//char   buf[128];
	int    r;

	//aml_fe_property_name(dev, name, buf);
	r = of_property_read_u32(pdev->dev.of_node, name, v);
	if (r)
		pr_error("cannot find resource \"%s\"\n", name);

	return r;
}
EXPORT_SYMBOL(aml_fe_of_property_u32);

static int aml_fe_dev_init(struct aml_fe_man *man,
			   enum aml_fe_dev_type_t type,
			   struct aml_fe_dev *dev,
			   int id)
{
	struct aml_fe_drv **list = aml_get_fe_drv_list(type);
	struct aml_fe_drv *drv;
	unsigned long flags;
	char  buf[128];
	const char *str;
	char *name = NULL;
	u32 value;
	int ret;
	struct device_node *node;

	dev->man    = man;
	dev->dev_id = id;
	dev->type   = type;
	dev->drv    = NULL;
	dev->fe     = NULL;
	dev->priv_data = NULL;

	memset(buf, 0, 128);
	name = NULL;
	aml_fe_property_name(dev, name, buf);
	pr_dbg("get string: %s\n", buf);
	ret = aml_fe_of_property_string(dev, buf, &str);
	if (ret) {
		pr_dbg("get string: %s error\n", buf);
		return 0;
	}


	spin_lock_irqsave(&lock, flags);

	for (drv = *list; drv; drv = drv->next)
		if (!strcmp(drv->name, str))
			break;

	if (dev->drv != drv) {
		if (dev->drv) {
			dev->drv->ref--;
			if (dev->drv->owner)
				module_put(dev->drv->owner);
		}
		if (drv) {
			drv->ref++;
			if (drv->owner)
				try_module_get(drv->owner);
		}
		dev->drv = drv;
	}

	spin_unlock_irqrestore(&lock, flags);

	if (drv) {
		pr_inf("found driver: %s\n", str);
	} else {
		pr_err("cannot find driver: %s\n", str);
		return -1;
	}
	/*get i2c adap and i2c addr*/
	memset(buf, 0, 128);
	name = "i2c_adap";
	aml_fe_property_name(dev, name, buf);
	pr_dbg("get u32: %s\n", buf);
	//ret = aml_fe_of_property_u32(dev, buf, &value);
	node = of_parse_phandle(dev->man->pdev->dev.of_node, buf, 0);
	if (node) {
		dev->i2c_adap = of_find_i2c_adapter_by_node(node);
		pr_inf("%s:[%p]\n", buf, dev->i2c_adap);
		of_node_put(node);
	} else {
		dev->i2c_adap_id = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	memset(buf, 0, 128);
	name = "i2c_addr";
	aml_fe_property_name(dev, name, buf);
	pr_dbg("get u32: %s\n", buf);
	ret = aml_fe_of_property_u32(dev, buf, &value);
	if (!ret) {
		dev->i2c_addr = value;
		pr_inf("%s: %d\n", buf, dev->i2c_addr);
	} else {
		dev->i2c_addr = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	/*get i2c reset and reset value*/
	memset(buf, 0, 128);
	name = "reset_gpio";
	aml_fe_property_name(dev, name, buf);
	pr_dbg("get string: %s\n", buf);
	ret = aml_fe_of_property_string(dev, buf, &str);
	if (!ret) {
		dev->reset_gpio =
		     of_get_named_gpio_flags(dev->man->pdev->dev.of_node,
		     buf, 0, NULL);
		pr_inf("%s: %s\n", buf, str);
	} else {
		dev->reset_gpio = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	memset(buf, 0, 128);
	name = "reset_value";
	aml_fe_property_name(dev, name, buf);
	pr_dbg("get u32: %s\n", buf);
	ret = aml_fe_of_property_u32(dev, buf, &value);
	if (!ret) {
		dev->reset_value = value;
		pr_inf("%s: %d\n", buf, dev->reset_value);
	} else {
		dev->reset_value = -1;
	}

	if (dev->drv && dev->drv->init) {
		int ret;

		ret = dev->drv->init(dev);
		if (ret != 0) {
			dev->drv = NULL;
			pr_error("[aml_fe..]%s error.\n", __func__);
			return ret;
		}
	}

	return 0;
}

static int aml_fe_dev_release(struct aml_fe_dev *dev)
{
	if (dev->drv) {
		if (dev->drv->owner)
			module_put(dev->drv->owner);
		dev->drv->ref--;
		if (dev->drv->release)
			dev->drv->release(dev);
	}

	dev->drv = NULL;
	return 0;
}

static void aml_fe_man_run(struct aml_fe *fe)
{
	if (fe->init)
		return;

	if (fe->tuner && fe->tuner->drv)
		fe->init = 1;

	if (fe->atv_demod && fe->atv_demod->drv)
		fe->init = 1;

	if (fe->dtv_demod && fe->dtv_demod->drv)
		fe->init = 1;

	if (fe->init) {
		struct aml_dvb *dvb = fe->man->dvb;
		int reg = 1;
		int ret;
		int id;

		spin_lock_init(&fe->slock);

		fe->sys = SYS_UNDEFINED;

		pr_dbg("fe: %p\n", fe);

		for (id = 0; id < FE_DEV_COUNT; id++) {
			struct aml_fe *prev_fe = &fe_man.fe[id];

			if (prev_fe == fe)
				continue;
			if (prev_fe->init && (prev_fe->dev_id == fe->dev_id)) {
				reg = 0;
				break;
			}
		}

		fe->fe = &fe_man.dev[fe->dev_id];
		if (reg) {
			fe->fe->demodulator_priv = fe;
			fe->fe->ops.set_property = aml_fe_set_property;
			fe->fe->ops.get_property = aml_fe_set_property;
		}

		if (fe->tuner)
			fe->tuner->fe = fe;
		if (fe->atv_demod)
			fe->atv_demod->fe = fe;
		if (fe->dtv_demod)
			fe->dtv_demod->fe = fe;

		ret = dvb_register_frontend(&dvb->dvb_adapter, fe->fe);
		if (ret) {
			pr_error("register fe%d failed\n", fe->dev_id);
			return;
		}
	}
}

static void fe_property_name(struct aml_fe *fe, const char *name,
			char *buf)
{
	if (name)
		sprintf(buf, "fe%d_%s", fe->dev_id, name);
	else
		sprintf(buf, "fe%d", fe->dev_id);
}

static int fe_of_property_u32(struct aml_fe *fe,
			const char *name, u32 *v)
{
	struct platform_device *pdev = fe->man->pdev;
	char   buf[128];
	int    r;

	fe_property_name(fe, name, buf);
	r = of_property_read_u32(pdev->dev.of_node, buf, v);
	if (r)
		pr_error("cannot find resource \"%s\"\n", buf);

	return r;
}

static int aml_fe_man_init(struct aml_fe_man *man, struct aml_fe *fe, int id)
{
	u32 value;
	int ret;

	fe->sys    = SYS_UNDEFINED;
	fe->man    = man;
	fe->dev_id = id;
	fe->init   = 0;
	fe->ts     = AM_TS_SRC_TS0;
	fe->work_running = 0;
	fe->work_q       = NULL;
	fe->tuner        = NULL;
	fe->atv_demod    = NULL;
	fe->dtv_demod    = NULL;
	fe->do_work      = NULL;
	fe->get_property = NULL;
	fe->set_property = NULL;

	init_waitqueue_head(&fe->wait_q);
	spin_lock_init(&fe->slock);

	ret = fe_of_property_u32(fe, "tuner", &value);
	if (!ret) {
		id = value;

		if ((id < 0) || (id >= FE_DEV_COUNT) || !fe_man.tuner[id].drv) {
			pr_error("invalid tuner device id %d\n", id);
			return -1;
		}

		fe->tuner = &fe_man.tuner[id];
		fe_man.tuner[id].fe = fe;
	}

	ret = fe_of_property_u32(fe, "atv_demod", &value);
	if (!ret) {
		id = value;

		if ((id < 0) ||
			(id >= FE_DEV_COUNT) ||
			!fe_man.atv_demod[id].drv) {
			pr_error("invalid ATV demod device id %d\n", id);
			return -1;
		}

		fe->atv_demod = &fe_man.atv_demod[id];
		fe_man.atv_demod[id].fe = fe;
	}

	ret = fe_of_property_u32(fe, "dtv_demod", &value);
	if (!ret) {
		id = value;

		if ((id < 0) ||
			(id >= FE_DEV_COUNT) ||
			!fe_man.dtv_demod[id].drv) {
			pr_error("invalid DTV demod device id %d\n", id);
			return -1;
		}

		fe->dtv_demod = &fe_man.dtv_demod[id];
		fe_man.dtv_demod[id].fe = fe;
	}

	ret = fe_of_property_u32(fe, "ts", &value);
	if (!ret) {
		enum aml_ts_source_t ts = AM_TS_SRC_TS0;

		switch (value) {
		case 0:
			ts = AM_TS_SRC_TS0;
			break;
		case 1:
			ts = AM_TS_SRC_TS1;
			break;
		case 2:
			ts = AM_TS_SRC_TS2;
			break;
		default:
			break;
		}

		fe->ts = ts;
	}

	ret = fe_of_property_u32(fe, "dev", &value);
	if (!ret) {
		id = value;

		if ((id >= 0) && (id < FE_DEV_COUNT))
			fe->dev_id = id;
		else
			fe->dev_id = 0;
	}

	aml_fe_man_run(fe);

	return 0;
}

static int aml_fe_man_release(struct aml_fe *fe)
{
	if (fe->init) {
		aml_fe_cancel_work(fe);

		if (fe->work_q)
			destroy_workqueue(fe->work_q);

		aml_fe_set_sys(fe->fe, SYS_UNDEFINED);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);

		fe->tuner = NULL;
		fe->atv_demod = NULL;
		fe->dtv_demod = NULL;
		fe->init = 0;
	}

	return 0;
}

void aml_fe_set_pdata(struct aml_fe_dev *dev, void *pdata)
{
	dev->priv_data = pdata;
}
EXPORT_SYMBOL(aml_fe_set_pdata);

void *aml_fe_get_pdata(struct aml_fe_dev *dev)
{
	return dev->priv_data;
}
EXPORT_SYMBOL(aml_fe_get_pdata);

static void aml_fe_do_work(struct work_struct *work)
{
	struct aml_fe *fe;

	fe = container_of(work, struct aml_fe, work);

	if (fe->do_work)
		fe->do_work(fe);

	fe->work_running = 0;
}

void aml_fe_schedule_work(struct aml_fe *fe, void(*func)(struct aml_fe *fe))
{
	if (fe->work_running)
		cancel_work_sync(&fe->work);

	fe->work_running = 1;
	fe->do_work      = func;

	if (!fe->work_q) {
		fe->work_q = create_singlethread_workqueue("amlfe");
		INIT_WORK(&fe->work, aml_fe_do_work);
	}

	queue_work(fe->work_q, &fe->work);
}
EXPORT_SYMBOL(aml_fe_schedule_work);

void aml_fe_cancel_work(struct aml_fe *fe)
{
	if (fe->work_running) {
		fe->work_running = 0;
		cancel_work_sync(&fe->work);
	}

	fe->do_work = NULL;
}
EXPORT_SYMBOL(aml_fe_cancel_work);


int aml_fe_work_cancelled(struct aml_fe *fe)
{
	return fe->work_running ? 0 : 1;
}
EXPORT_SYMBOL(aml_fe_work_cancelled);

int aml_fe_work_sleep(struct aml_fe *fe, unsigned long delay)
{
	wait_event_interruptible_timeout(fe->wait_q, !fe->work_running, delay);
	return aml_fe_work_cancelled(fe);
}
EXPORT_SYMBOL(aml_fe_work_sleep);

static ssize_t tuner_name_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	size_t len = 0;
	struct aml_fe_drv *drv;
	unsigned long flags;

	struct aml_fe_drv **list = aml_get_fe_drv_list(AM_DEV_TUNER);

	spin_lock_irqsave(&lock, flags);
	for (drv = *list; drv; drv = drv->next)
		len += sprintf(buf + len, "%s\n", drv->name);
	spin_unlock_irqrestore(&lock, flags);
	return len;
}

static ssize_t atv_demod_name_show(struct class *cls,
				   struct class_attribute *attr, char *buf)
{
	size_t len = 0;
	struct aml_fe_drv *drv;
	unsigned long flags;

	struct aml_fe_drv **list = aml_get_fe_drv_list(AM_DEV_ATV_DEMOD);

	spin_lock_irqsave(&lock, flags);
	for (drv = *list; drv; drv = drv->next)
		len += sprintf(buf + len, "%s\n", drv->name);
	spin_unlock_irqrestore(&lock, flags);
	return len;
}

static ssize_t dtv_demod_name_show(struct class *cls,
				   struct class_attribute *attr, char *buf)
{
	size_t len = 0;
	struct aml_fe_drv *drv;
	unsigned long flags;

	struct aml_fe_drv **list = aml_get_fe_drv_list(AM_DEV_DTV_DEMOD);

	spin_lock_irqsave(&lock, flags);
	for (drv = *list; drv; drv = drv->next)
		len += sprintf(buf + len, "%s\n", drv->name);
	spin_unlock_irqrestore(&lock, flags);
	return len;
}

static ssize_t setting_show(struct class *cls, struct class_attribute *attr,
			    char *buf)
{
	int r, total = 0;
	int i;
	struct aml_fe_man *fm = &fe_man;

	r = sprintf(buf, "tuner:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe_dev *dev = &fm->tuner[i];

		if (dev->drv) {
			r = sprintf(buf, "\t%d: %s\n", i, dev->drv->name);
			buf += r;
			total += r;
		}
	}

	r = sprintf(buf, "atv_demod:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe_dev *dev = &fm->atv_demod[i];

		if (dev->drv) {
			r = sprintf(buf, "\t%d: %s\n", i, dev->drv->name);
			buf += r;
			total += r;
		}
	}

	r = sprintf(buf, "dtv_demod:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe_dev *dev = &fm->dtv_demod[i];

		if (dev->drv) {
			r = sprintf(buf, "\t%d: %s\n", i, dev->drv->name);
			buf += r;
			total += r;
		}
	}

	r = sprintf(buf, "frontend:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe *fe = &fm->fe[i];

		r = sprintf(buf,
			    "\t%d: %s device: %d ts: %d tuner: %s atv_demod: %s dtv_demod: %s\n",
			    i, fe->init ? "enabled" : "disabled", fe->dev_id,
			    fe->ts, fe->tuner ? fe->tuner->drv->name : "none",
			    fe->atv_demod ? fe->atv_demod->drv->name : "none",
			    fe->dtv_demod ? fe->dtv_demod->drv->name : "none");
		buf += r;
		total += r;
	}

	return total;
}

static void reset_drv(int id, enum aml_fe_dev_type_t type, const char *name)
{
	struct aml_fe_man *fm = &fe_man;
	struct aml_fe_drv **list;
	struct aml_fe_drv **pdrv;
	struct aml_fe_drv *drv;
	struct aml_fe_drv *old;

	if ((id < 0) || (id >= FE_DEV_COUNT))
		return;

	if (fm->fe[id].init) {
		pr_error("cannot reset driver when the device is inused\n");
		return;
	}

	list = aml_get_fe_drv_list(type);
	for (drv = *list; drv; drv = drv->next)
		if (!strcmp(drv->name, name))
			break;

	switch (type) {
	case AM_DEV_TUNER:
		pdrv = &fm->tuner[id].drv;
		break;
	case AM_DEV_ATV_DEMOD:
		pdrv = &fm->atv_demod[id].drv;
		break;
	case AM_DEV_DTV_DEMOD:
		pdrv = &fm->dtv_demod[id].drv;
		break;
	default:
		return;
	}

	old = *pdrv;
	if (old == drv)
		return;

	if (old) {
		old->ref--;
		if (old->owner)
			module_put(old->owner);
	}

	if (drv) {
		drv->ref++;
		if (drv->owner)
			try_module_get(drv->owner);
	}

	*pdrv = drv;
}

static ssize_t setting_store(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t size)
{
	struct aml_fe_man *fm = &fe_man;
	int id, val;
	char dev_name[32];
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	if (sscanf(buf, "tuner %i driver %s", &id, dev_name) == 2) {
		reset_drv(id, AM_DEV_TUNER, dev_name);
	} else if (sscanf(buf, "atv_demod %i driver %s", &id, dev_name) == 2) {
		reset_drv(id, AM_DEV_ATV_DEMOD, dev_name);
	} else if (sscanf(buf, "dtv_demod %i driver %s", &id, dev_name) == 2) {
		reset_drv(id, AM_DEV_DTV_DEMOD, dev_name);
	} else if (sscanf(buf, "frontend %i device %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->fe[id].dev_id = val;
	} else if (sscanf(buf, "frontend %i ts %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->fe[id].ts = val;
	} else if (sscanf(buf, "frontend %i tuner %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT) && (val >= 0)
		    && (val < FE_DEV_COUNT) && fm->tuner[val].drv)
			fm->fe[id].tuner = &fm->tuner[val];
	} else if (sscanf(buf, "frontend %i atv_demod %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT) && (val >= 0)
		    && (val < FE_DEV_COUNT) && fm->atv_demod[val].drv)
			fm->fe[id].atv_demod = &fm->atv_demod[val];
	} else if (sscanf(buf, "frontend %i dtv_demod %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT) && (val >= 0)
		    && (val < FE_DEV_COUNT) && fm->dtv_demod[val].drv)
			fm->fe[id].dtv_demod = &fm->dtv_demod[val];
	}

	spin_unlock_irqrestore(&lock, flags);

	if (sscanf(buf, "enable %i", &id) == 1) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			aml_fe_man_run(&fm->fe[id]);
	} else if (sscanf(buf, "disable %i", &id) == 1) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			aml_fe_man_release(&fm->fe[id]);
	} else if (strstr(buf, "autoload")) {
		for (id = 0; id < FE_DEV_COUNT; id++) {
			aml_fe_dev_init(fm, AM_DEV_TUNER,
					&fm->tuner[id], id);
			aml_fe_dev_init(fm, AM_DEV_ATV_DEMOD,
					&fm->atv_demod[id], id);
			aml_fe_dev_init(fm, AM_DEV_DTV_DEMOD,
					&fm->dtv_demod[id], id);
		}

		for (id = 0; id < FE_DEV_COUNT; id++)
			aml_fe_man_init(fm, &fm->fe[id], id);
	} else if (strstr(buf, "disableall")) {
		for (id = 0; id < FE_DEV_COUNT; id++)
			aml_fe_man_release(&fm->fe[id]);

		for (id = 0; id < FE_DEV_COUNT; id++) {
			aml_fe_dev_release(&fm->dtv_demod[id]);
			aml_fe_dev_release(&fm->atv_demod[id]);
			aml_fe_dev_release(&fm->tuner[id]);
		}
	}

	return size;
}

static ssize_t aml_fe_show_suspended_flag(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "%ld\n", aml_fe_suspended);

	return ret;
}

static ssize_t aml_fe_store_suspended_flag(struct class *class,
					   struct class_attribute *attr,
					   const char *buf, size_t size)
{
	/*aml_fe_suspended = simple_strtol(buf, 0, 0); */
	int ret = kstrtol(buf, 0, &aml_fe_suspended);

	if (ret)
		return ret;
	return size;
}

static struct class_attribute aml_fe_cls_attrs[] = {
	__ATTR(tuner_name,
	       0644,
	       tuner_name_show, NULL),
	__ATTR(atv_demod_name,
	       0644,
	       atv_demod_name_show, NULL),
	__ATTR(dtv_demod_name,
	       0644,
	       dtv_demod_name_show, NULL),
	__ATTR(setting,
	       0644,
	       setting_show, setting_store),
	__ATTR(aml_fe_suspended_flag,
	       0644,
	       aml_fe_show_suspended_flag,
	       aml_fe_store_suspended_flag),
	__ATTR_NULL
};

static struct class aml_fe_class = {
	.name = "amlfe",
	.class_attrs = aml_fe_cls_attrs,
};

static int aml_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	int i;

	fe_man.dvb  = dvb;
	fe_man.pdev = pdev;

	platform_set_drvdata(pdev, &fe_man);

	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (aml_fe_dev_init(&fe_man,
					AM_DEV_TUNER,
					&fe_man.tuner[i], i) < 0)
			goto probe_end;

		if (aml_fe_dev_init(&fe_man,
					AM_DEV_ATV_DEMOD,
					&fe_man.atv_demod[i], i) < 0)
			goto probe_end;

		if (aml_fe_dev_init(&fe_man,
			AM_DEV_DTV_DEMOD,
			&fe_man.dtv_demod[i], i) < 0)
			goto probe_end;
	}

	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (aml_fe_man_init(&fe_man, &fe_man.fe[i], i) < 0)
			goto probe_end;
	}

 probe_end:

	fe_man.pinctrl = devm_pinctrl_get_select_default(&pdev->dev);

	if (class_register(&aml_fe_class) < 0)
		pr_error("[aml_fe..] register class error\n");

	pr_dbg("[aml_fe..] probe ok.\n");

	return 0;
}

static int aml_fe_remove(struct platform_device *pdev)
{
	struct aml_fe_man *fe_man = platform_get_drvdata(pdev);
	int i;

	if (fe_man) {
		platform_set_drvdata(pdev, NULL);

		for (i = 0; i < FE_DEV_COUNT; i++)
			aml_fe_man_release(&fe_man->fe[i]);
		for (i = 0; i < FE_DEV_COUNT; i++) {
			aml_fe_dev_release(&fe_man->dtv_demod[i]);
			aml_fe_dev_release(&fe_man->atv_demod[i]);
			aml_fe_dev_release(&fe_man->tuner[i]);
		}

		if (fe_man->pinctrl)
			devm_pinctrl_put(fe_man->pinctrl);
	}

	class_unregister(&aml_fe_class);

	return 0;
}

static int aml_fe_suspend(struct platform_device *dev, pm_message_t state)
{
	int i;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe *fe = &fe_man.fe[i];

		if (fe->tuner && fe->tuner->drv->suspend)
			fe->tuner->drv->suspend(fe->tuner);

		if (fe->atv_demod && fe->atv_demod->drv->suspend)
			fe->atv_demod->drv->suspend(fe->atv_demod);

		if (fe->dtv_demod && fe->dtv_demod->drv->suspend)
			fe->dtv_demod->drv->suspend(fe->dtv_demod);
	}

	aml_fe_suspended = 1;

	return 0;
}

static int aml_fe_resume(struct platform_device *dev)
{
	int i;

	aml_fe_suspended = 0;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe *fe = &fe_man.fe[i];

		if (fe->tuner && fe->tuner->drv->resume)
			fe->tuner->drv->resume(fe->tuner);

		if (fe->atv_demod && fe->atv_demod->drv->resume)
			fe->atv_demod->drv->resume(fe->atv_demod);

		if (fe->dtv_demod && fe->dtv_demod->drv->resume)
			fe->dtv_demod->drv->resume(fe->dtv_demod);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_fe_dt_match[] = {
	{
	 .compatible = "amlogic, dvbfe",
	 },
	{},
};
#endif				/*CONFIG_OF */

static struct platform_driver aml_fe_driver = {
	.probe   = aml_fe_probe,
	.remove  = aml_fe_remove,
	.suspend = aml_fe_suspend,
	.resume  = aml_fe_resume,
	.driver  = {
		.name = "amlogic-dvb-fe",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_fe_dt_match,
#endif
	}
};

static int __init aml_fe_init(void)
{
	return platform_driver_register(&aml_fe_driver);
}

static void __exit aml_fe_exit(void)
{
	platform_driver_unregister(&aml_fe_driver);
}

module_init(aml_fe_init);
module_exit(aml_fe_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("amlogic frontend driver");
MODULE_AUTHOR("L+#= +0=1");

