// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/amlogic/aml_tuner.h>
#include <linux/amlogic/aml_dtvdemod.h>
#include <linux/amlogic/aml_dvb_extern.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "aml_dvb_extern_driver.h"

#define AML_DVB_EXTERN_DEVICE_NAME    "aml_dvb_extern"
#define AML_DVB_EXTERN_DRIVER_NAME    "aml_dvb_extern"
#define AML_DVB_EXTERN_MODULE_NAME    "aml_dvb_extern"
#define AML_DVB_EXTERN_CLASS_NAME     "aml_dvb_extern"

#define AML_DVB_EXTERN_VERSION    "V1.09"

static struct dvb_extern_device *dvb_extern_dev;

static char *fe_type_name[] = {
	"FE_QPSK",
	"FE_QAM",
	"FE_OFDM",
	"FE_ATSC",
	"FE_ANALOG",
	"FE_DTMB",
	"FE_ISDBT"
};

static char *fe_delsys_name[] = {
	"SYS_UNDEFINED",
	"SYS_DVBC_ANNEX_A",
	"SYS_DVBC_ANNEX_B",
	"SYS_DVBT",
	"SYS_DSS",
	"SYS_DVBS",
	"SYS_DVBS2",
	"SYS_DVBH",
	"SYS_ISDBT",
	"SYS_ISDBS",
	"SYS_ISDBC",
	"SYS_ATSC",
	"SYS_ATSCMH",
	"SYS_DTMB",
	"SYS_CMMB",
	"SYS_DAB",
	"SYS_DVBT2",
	"SYS_TURBO",
	"SYS_DVBC_ANNEX_C",
	"SYS_ANALOG"
};

static void aml_dvb_extern_set_power(struct gpio_config *pin_cfg, int on)
{
	if (!aml_gpio_is_valid(pin_cfg->pin)) {
		pr_err("%s: dvb power gpio is invalid.", __func__);

		return;
	}

	/* OD pin[No output capacity], set direction output as low output. */
	if (pin_cfg->dir == GPIOF_DIR_OUT) {
		if (on)
			aml_gpio_set_value(pin_cfg->pin, pin_cfg->value);
		else
			aml_gpio_set_value(pin_cfg->pin, !(pin_cfg->value));
	} else {
		if (on) {
			aml_gpio_direction_input(pin_cfg->pin);
			aml_gpio_set_value(pin_cfg->pin, pin_cfg->value);
		} else {
			aml_gpio_direction_output(pin_cfg->pin, !(pin_cfg->value));
		}
	}
}

void aml_dvb_extern_work(struct work_struct *work)
{
	struct tuner_ops *tops = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();

	list_for_each_entry(tops, &tuner->list, list) {
		if (tops->fe.ops.tuner_ops.resume)
			tops->fe.ops.tuner_ops.resume(&tops->fe);
		else if (tops->fe.ops.tuner_ops.init)
			tops->fe.ops.tuner_ops.init(&tops->fe);
	}
}

static ssize_t tuner_debug_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0, tuner_id = 0, fe_type = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL, *name = NULL;
	char *parm[10] = { NULL };
	unsigned long val = 0, freq = 0, symbol_rate = 0, bw = 0, modul = 0;
	unsigned long rolloff = 0;
	struct tuner_ops *ops = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);
	struct dvb_frontend *fe = dev->tuner_fe;
	struct analog_parameters *p = &dev->para;
	struct dtv_frontend_properties *c = NULL;
	u32 status = FE_TIMEDOUT, frequency = 0, bandwidth = 0;
	u32 if_frequency[2] = { 0 };
	s16 strength = 0;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (!token)
			break;

		if (*token == '\0')
			continue;

		if (n >= 10)
			break;

		parm[n++] = token;
	}

	if (!parm[0])
		goto EXIT;

	if (!fe && strncmp(parm[0], "attach", 6) && strncmp(parm[0], "status", 6)) {
		pr_err("please attach first:\n");
		pr_err("echo attach [tuner_id] > /sys/class/aml_dvb_extern/tuner_debug\n");
		goto EXIT;
	}

	if (!strncmp(parm[0], "init", 4)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			fe->ops.info.type = val;
		else
			fe->ops.info.type = FE_ANALOG;

		if (val > 6) {
			pr_err("init tuner mode value [%ld] error.\n", val);
			goto EXIT;
		}

		pr_err("init tuner mode [%d][%s].\n",
				fe->ops.info.type,
				fe_type_name[fe->ops.info.type]);

		if (fe->ops.tuner_ops.init)
			fe->ops.tuner_ops.init(fe);

		if (fe->ops.tuner_ops.set_config)
			fe->ops.tuner_ops.set_config(fe, NULL);
	} else if (!strncmp(parm[0], "tune", 4)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &freq)) {
			p->frequency = freq;
		} else {
			pr_err("please input frequency.\n");
			goto EXIT;
		}

		if (fe->ops.info.type == FE_ANALOG) {
			p->frequency = freq;
			if (fe->ops.tuner_ops.set_analog_params)
				fe->ops.tuner_ops.set_analog_params(fe, p);

			pr_err("tune mode [%d][%s] freq[%d] std[0x%llx], audmode[0x%x].\n",
					fe->ops.info.type,
					fe_type_name[fe->ops.info.type],
					p->frequency,
					p->std, p->audmode);
		} else {
			c = &fe->dtv_property_cache;

			c->frequency = freq;

			if (fe->ops.info.type == FE_QPSK)
				c->frequency = c->frequency / 1000;

			if (parm[2] && !kstrtoul(parm[2], 0, &symbol_rate))
				c->symbol_rate = symbol_rate;
			else
				c->symbol_rate = 0;

			if (parm[3] && !kstrtoul(parm[3], 0, &bw))
				c->bandwidth_hz = bw;
			else
				c->bandwidth_hz = 8000000;

			if (parm[4] && !kstrtoul(parm[4], 0, &modul)) {
				c->modulation = modul;
			} else {
				switch (fe->ops.info.type) {
				case FE_QPSK:
					c->modulation = QPSK;
					break;
				case FE_ATSC:
					c->modulation = VSB_8;
					break;
				default:
					c->modulation = QAM_AUTO;
					break;
				}
			}

			if (parm[5] && !kstrtoul(parm[5], 0, &rolloff))
				c->rolloff = rolloff;
			else
				c->rolloff = ROLLOFF_AUTO;

			if (fe->ops.tuner_ops.set_params)
				fe->ops.tuner_ops.set_params(fe);

			pr_err("tune mode [%d][%s], delivery_system %d, frequency %d, symbol_rate %d, bandwidth_hz %d, modulation %d, rolloff %d, inversion %d.\n",
					fe->ops.info.type,
					fe_type_name[fe->ops.info.type],
					c->delivery_system,
					c->frequency, c->symbol_rate,
					c->bandwidth_hz, c->modulation,
					c->rolloff, c->inversion);
		}
	} else if (!strncmp(parm[0], "std", 3)) {
		if (!strncmp(parm[1], "pal", 3))
			p->std = V4L2_COLOR_STD_PAL;
		else if (!strncmp(parm[1], "ntsc", 4))
			p->std = V4L2_COLOR_STD_NTSC;
		else if (!strncmp(parm[1], "secam", 5))
			p->std = V4L2_COLOR_STD_SECAM;
		else
			pr_err("invalid video std: %s.\n", parm[1]);

		if (!strncmp(parm[2], "dk", 2))
			p->std |= V4L2_STD_PAL_DK;
		else if (!strncmp(parm[2], "bg", 2))
			p->std |= V4L2_STD_BG;
		else if (!strncmp(parm[2], "i", 1))
			p->std |= V4L2_STD_PAL_I;
		else if (!strncmp(parm[2], "nm", 2))
			p->std |= V4L2_STD_NTSC;
		else if (!strncmp(parm[2], "pm", 2))
			p->std |= V4L2_STD_PAL_M;
		else if (!strncmp(parm[2], "lc", 2))
			p->std |= V4L2_STD_SECAM_LC;
		else if (!strncmp(parm[2], "l", 1))
			p->std |= V4L2_STD_SECAM_L;
		else if (!strncmp(parm[2], "n", 1))
			p->std |= V4L2_STD_PAL_N;
		else
			pr_err("invalid audio std: %s.\n", parm[2]);
	} else if (!strncmp(parm[0], "status", 6)) {
		pr_err("tuner numbers: %d.\n", dev->tuner_num);
		pr_err("all tuners:\n");
		list_for_each_entry(ops, &tuner->list, list) {
			name = ops->fe.ops.tuner_ops.info.name;
			pr_err("tuner%d, id %d (%s) addr 0x%x, adap 0x%p %s, %sdetected.\n",
					ops->index, ops->cfg.id,
					name ? name : "",
					ops->cfg.i2c_addr, ops->cfg.i2c_adap,
					ops->attached ? "attached" : "detached",
					ops->cfg.detect ? (ops->valid ? "" : "un") : "no ");
		}

		if (tuner->used) {
			name = tuner->used->fe.ops.tuner_ops.info.name;
			pr_err("current use tuner%d, id %d (%s), fe type %d.\n",
					tuner->used->index,
					tuner->used->cfg.id,
					name ? name : "",
					tuner->used->type);

			if (tuner->used->fe.ops.tuner_ops.get_status) {
				tuner->used->fe.ops.tuner_ops.get_status(
						&tuner->used->fe, &status);
				pr_err("signal status: %s.\n",
						status & FE_HAS_LOCK ? "Locked" : "Unlocked");
			}

			if (tuner->used->fe.ops.tuner_ops.get_strength) {
				tuner->used->fe.ops.tuner_ops.get_strength(
						&tuner->used->fe, &strength);
				pr_err("strength: %d dBm.\n", strength);
			}

			if (tuner->used->fe.ops.tuner_ops.get_frequency) {
				tuner->used->fe.ops.tuner_ops.get_frequency(
						&tuner->used->fe, &frequency);
				pr_err("frequency: %d Hz.\n", frequency);
			}

			if (tuner->used->fe.ops.tuner_ops.get_bandwidth) {
				tuner->used->fe.ops.tuner_ops.get_bandwidth(
						&tuner->used->fe, &bandwidth);
				pr_err("bandwidth: %d Hz.\n", bandwidth);
			}

			if (tuner->used->fe.ops.tuner_ops.get_if_frequency) {
				tuner->used->fe.ops.tuner_ops.get_if_frequency(
						&tuner->used->fe, if_frequency);
				pr_err("if frequency: %d Hz (IF spectrum %s).\n",
						if_frequency[1],
						if_frequency[0] ? "inverted" : "normal");
			}
		}
	} else if (!strncmp(parm[0], "uninit", 6)) {
		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
	} else if (!strncmp(parm[0], "attach", 6)) {
		tuner->attach(tuner, true);
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			tuner_id = val;
		else
			tuner_id = 0;

		if (tuner_id >= dev->tuner_num) {
			pr_err("input tuner index %d error, the max is %d, use default %d.\n",
					tuner_id, dev->tuner_num - 1,
					dev->tuner_cur);

			goto EXIT;
		}

		dev->tuner_cur = tuner_id;
		ops = dvb_tuner_ops_get_byindex(dev->tuner_cur);
		if (ops) {
			dev->tuner_fe = &ops->fe;
			//memcpy(&fe->ops.tuner_ops, &ops->fe.ops.tuner_ops,
			//		sizeof(struct dvb_tuner_ops));
			//fe->tuner_priv = ops->fe.tuner_priv;

			pr_err("tuner %d attach done.\n", dev->tuner_cur);
		}
	} else if (!strncmp(parm[0], "detach", 6)) {
		tuner->attach(tuner, false);
	} else if (!strncmp(parm[0], "match", 5)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			fe_type = val;
		else
			fe_type = FE_ANALOG;

		if (fe_type > FE_ISDBT) {
			pr_err("input tuner match fe type %d error.\n",
					fe_type);

			goto EXIT;
		}

		if (tuner->match(tuner, fe_type))
			pr_err("tuner match fe type %d (%s) done.\n",
					fe_type, fe_type_name[fe_type]);
		else
			pr_err("tuner match fe type %d (%s) fail.\n",
					fe_type, fe_type_name[fe_type]);

	} else if (!strncmp(parm[0], "detect", 6)) {
		tuner->detect(tuner);

		dev->tuner_fe = NULL;
	} else {
		pr_err("invalid command: %s.\n", parm[0]);
	}

EXIT:
	kfree(buf_orig);

	return count;
}

static ssize_t tuner_debug_show(struct class *class,
		struct class_attribute *attr, char *buff)
{
	int n = 0;
	char *name = NULL;
	const char *path = "/sys/class/aml_dvb_extern/tuner_debug";
	struct tuner_ops *ops = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);
	u32 status = FE_TIMEDOUT, frequency = 0, bandwidth = 0;
	u32 if_frequency[2] = { 0 };
	s16 strength = 0;

	n += sprintf(buff + n, "\nTuner Debug Usage (%s):\n", AML_DVB_EXTERN_VERSION);
	n += sprintf(buff + n, "[status]\necho status > %s\n", path);
	n += sprintf(buff + n, "[attach]\necho attach [tuner_id] > %s\n", path);
	n += sprintf(buff + n, "[detach]\necho detach > %s\n", path);
	n += sprintf(buff + n, "[match]\necho match [fe_type] > %s\n", path);
	n += sprintf(buff + n, "\tfe_type:\n");
	n += sprintf(buff + n, "\tDVB-S/S2 [FE_QPSK]  : 0\n");
	n += sprintf(buff + n, "\tDVB-C    [FE_QAM]   : 1\n");
	n += sprintf(buff + n, "\tDVB-T/T2 [FE_OFDM]  : 2\n");
	n += sprintf(buff + n, "\tATSC     [FE_ATSC]  : 3\n");
	n += sprintf(buff + n, "\tANALOG   [FE_ANALOG]: 4\n");
	n += sprintf(buff + n, "\tDTMB     [FE_DTMB]  : 5\n");
	n += sprintf(buff + n, "\tISDBT    [FE_ISDBT] : 6\n");
	n += sprintf(buff + n, "[detect]\necho detect > %s\n", path);
	n += sprintf(buff + n, "[init]\n");
	n += sprintf(buff + n, "echo init [fe_type] > %s\n", path);
	n += sprintf(buff + n, "\tfe_type:\n");
	n += sprintf(buff + n, "\tDVB-S/S2 [FE_QPSK]  : 0\n");
	n += sprintf(buff + n, "\tDVB-C    [FE_QAM]   : 1\n");
	n += sprintf(buff + n, "\tDVB-T/T2 [FE_OFDM]  : 2\n");
	n += sprintf(buff + n, "\tATSC     [FE_ATSC]  : 3\n");
	n += sprintf(buff + n, "\tANALOG   [FE_ANALOG]: 4\n");
	n += sprintf(buff + n, "\tDTMB     [FE_DTMB]  : 5\n");
	n += sprintf(buff + n, "\tISDBT    [FE_ISDBT] : 6\n");
	n += sprintf(buff + n, "[set atv std]\n");
	n += sprintf(buff + n, "echo std [vstd] [astd] > %s\n", path);
	n += sprintf(buff + n, "\tvstd: pal, ntsc, secam\n");
	n += sprintf(buff + n, "\tastd: dk, bg, i, nm, pm, l, lc, n\n");
	n += sprintf(buff + n, "[tune]\n");
	n += sprintf(buff + n, "echo tune [frequency_hz] [symbol_rate_bps] ");
	n += sprintf(buff + n, "[bw_hz] [modulation] [rolloff]");
	n += sprintf(buff + n, " > %s\n", path);
	n += sprintf(buff + n, "\tmodulation:\n");
	n += sprintf(buff + n, "\tQPSK     : 0\n");
	n += sprintf(buff + n, "\tQAM_16   : 1\n");
	n += sprintf(buff + n, "\tQAM_32   : 2\n");
	n += sprintf(buff + n, "\tQAM_64   : 3\n");
	n += sprintf(buff + n, "\tQAM_128  : 4\n");
	n += sprintf(buff + n, "\tQAM_256  : 5\n");
	n += sprintf(buff + n, "\tQAM_AUTO : 6\n");
	n += sprintf(buff + n, "\tVSB_8    : 7\n");
	n += sprintf(buff + n, "\tVSB_16   : 8\n");
	n += sprintf(buff + n, "\tPSK_8    : 9\n");
	n += sprintf(buff + n, "\tAPSK_16  : 10\n");
	n += sprintf(buff + n, "\tAPSK_32  : 11\n");
	n += sprintf(buff + n, "\tDQPSK    : 12\n");
	n += sprintf(buff + n, "\tQAM_4_NR : 13\n");
	n += sprintf(buff + n, "\trolloff:\n");
	n += sprintf(buff + n, "\tROLLOFF_35   : 0\n");
	n += sprintf(buff + n, "\tROLLOFF_20   : 1\n");
	n += sprintf(buff + n, "\tROLLOFF_25   : 2\n");
	n += sprintf(buff + n, "\tROLLOFF_AUTO : 3\n");
	n += sprintf(buff + n, "[uninit]\necho uninit > %s\n", path);

	n += sprintf(buff + n, "\n------------------------------------------------------------\n");
	n += sprintf(buff + n, "\nTuner Status:\n");
	n += sprintf(buff + n, "tuner numbers: %d.\n", dev->tuner_num);
	n += sprintf(buff + n, "all tuners:\n");
	list_for_each_entry(ops, &tuner->list, list) {
		name = ops->fe.ops.tuner_ops.info.name;
		n += sprintf(buff + n, "tuner%d, id %d (%s) addr 0x%x, adap 0x%p %s, %sdetected.\n",
				ops->index, ops->cfg.id,
				name ? name : "",
				ops->cfg.i2c_addr, ops->cfg.i2c_adap,
				ops->attached ? "attached" : "detached",
				ops->cfg.detect ? (ops->valid ? "" : "un") : "no ");
	}

	if (tuner->used) {
		name = tuner->used->fe.ops.tuner_ops.info.name;
		n += sprintf(buff + n, "current use tuner%d, id %d (%s), fe type %d.\n",
				tuner->used->index,
				tuner->used->cfg.id,
				name ? name : "",
				tuner->used->type);

		if (tuner->used->fe.ops.tuner_ops.get_status) {
			tuner->used->fe.ops.tuner_ops.get_status(
					&tuner->used->fe, &status);
			n += sprintf(buff + n, "signal status: %s.\n",
					status & FE_HAS_LOCK ? "Locked" : "Unlocked");
		}

		if (tuner->used->fe.ops.tuner_ops.get_strength) {
			tuner->used->fe.ops.tuner_ops.get_strength(
					&tuner->used->fe, &strength);
			n += sprintf(buff + n, "strength: %d dBm.\n", strength);
		}

		if (tuner->used->fe.ops.tuner_ops.get_frequency) {
			tuner->used->fe.ops.tuner_ops.get_frequency(
					&tuner->used->fe, &frequency);
			n += sprintf(buff + n, "frequency: %d Hz.\n", frequency);
		}

		if (tuner->used->fe.ops.tuner_ops.get_bandwidth) {
			tuner->used->fe.ops.tuner_ops.get_bandwidth(
					&tuner->used->fe, &bandwidth);
			n += sprintf(buff + n, "bandwidth: %d Hz.\n", bandwidth);
		}

		if (tuner->used->fe.ops.tuner_ops.get_if_frequency) {
			tuner->used->fe.ops.tuner_ops.get_if_frequency(
					&tuner->used->fe, if_frequency);
			n += sprintf(buff + n, "if frequency: %d Hz (IF spectrum %s).\n",
					if_frequency[1],
					if_frequency[0] ? "inverted" : "normal");
		}
	}

	n += sprintf(buff + n, "\n");

	return n;
}

static ssize_t demod_debug_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0, demod_id = 0, fe_type = 0;
	unsigned int ret = 0, delay = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL, *name = NULL;
	char *parm[10] = { NULL };
	unsigned long val = 0, freq = 0, symbol_rate = 0, bw = 0, modul = 0;
	unsigned long rolloff = 0;
	struct demod_ops *ops = NULL;
	struct dvb_demod *demod = get_dvb_demods();
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);
	struct dvb_frontend *fe = dev->demod_fe;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct dtv_property tvp;
	enum fe_status s = FE_NONE;
	enum fe_delivery_system delsys = 0;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (!token)
			break;

		if (*token == '\0')
			continue;

		if (n >= 10)
			break;

		parm[n++] = token;
	}

	if (!parm[0])
		goto EXIT;

	if (!fe && strncmp(parm[0], "attach", 6) && strncmp(parm[0], "status", 6)) {
		pr_err("please attach first:\n");
		pr_err("echo attach [demod_id] > /sys/class/aml_dvb_extern/demod_debug\n");
		goto EXIT;
	}

	if (!strncmp(parm[0], "init", 4)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			delsys = val;
		else
			delsys = SYS_DTMB;

		if (delsys > SYS_ANALOG) {
			pr_err("init demod delsys value [%d] error.\n",
					delsys);
			goto EXIT;
		}

		memset(&tvp, 0, sizeof(tvp));
		c->delivery_system = delsys;
		tvp.cmd = DTV_DELIVERY_SYSTEM;
		tvp.u.data = c->delivery_system;

		pr_err("init demod delsys [%d][%s].\n",
				c->delivery_system,
				fe_delsys_name[c->delivery_system]);

		if (fe->ops.init)
			fe->ops.init(fe);

		if (fe->ops.set_property)
			fe->ops.set_property(fe, &tvp);
	} else if (!strncmp(parm[0], "tune", 4)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			freq = val;
		else
			freq = 88000000;

		if (parm[2] && !kstrtoul(parm[2], 0, &val))
			symbol_rate = val;
		else
			symbol_rate = 0;

		if (parm[3] && !kstrtoul(parm[3], 0, &val))
			bw = val;
		else
			bw = 8000000;

		if (parm[4] && !kstrtoul(parm[4], 0, &val)) {
			modul = val;
		} else {
			switch (c->delivery_system) {
			case SYS_DVBS:
			case SYS_DVBS2:
			case SYS_TURBO:
				modul = QPSK;
				break;
			case SYS_ATSC:
				modul = VSB_8;
				break;
			default:
				modul = QAM_AUTO;
				break;
			}
		}

		if (parm[5] && !kstrtoul(parm[5], 0, &val))
			rolloff = val;
		else
			rolloff = ROLLOFF_AUTO;

		if (c->delivery_system == SYS_DVBS || c->delivery_system == SYS_DVBS2)
			c->frequency = freq / 1000;
		else
			c->frequency = freq;

		c->symbol_rate = symbol_rate;
		c->bandwidth_hz = bw;
		c->modulation = modul;
		c->rolloff = rolloff;

		pr_err("tune delivery_system %d, frequency %d, symbol_rate %d, bandwidth_hz %d, modulation %d, rolloff %d, inversion %d.\n",
				c->delivery_system, c->frequency, c->symbol_rate,
				c->bandwidth_hz, c->modulation,
				c->rolloff, c->inversion);

		if (fe->ops.tune) {
			fe->ops.tune(fe, true, 0, &delay, &s);

			pr_err("tune status: [0x%x] %s.\n", s,
					s & FE_HAS_LOCK ?
					"Locked" : "Unlocked");
		} else if (fe->ops.set_frontend) {
			ret = fe->ops.set_frontend(fe);

			pr_err("tune status: [0x%x] %s.\n", ret,
					!ret ? "Locked" : "Unlocked");
		}

	} else if (!strncmp(parm[0], "status", 6)) {
		pr_err("demod numbers: %d.\n", dev->demod_num);
		pr_err("all demods:\n");
		list_for_each_entry(ops, &demod->list, list) {
			name = ops->fe ? ops->fe->ops.info.name : "";
			pr_err("demod%d, id %d (%s) %s, %sregistered, %sdetected, use tuner0 %s, tuner1 %s.\n",
					ops->index, ops->cfg.id,
					name ? name : "", ops->attached ?
					"attached" : "detached",
					ops->registered ? "" : "un",
					ops->cfg.detect ? (ops->valid ? "" : "un") : "no ",
					ops->cfg.tuner0.name ? ops->cfg.tuner0.name : "NONE",
					ops->cfg.tuner1.name ? ops->cfg.tuner1.name : "NONE");
		}

		if (demod->used && demod->used->fe) {
			name = ops->fe->ops.info.name;
			pr_err("current use demod%d, id %d (%s), fe type %d, delivery system %d.\n",
					demod->used->index,
					demod->used->cfg.id,
					name ? name : "",
					demod->used->type,
					demod->used->fe->dtv_property_cache.delivery_system);
		}
	} else if (!strncmp(parm[0], "uninit", 6)) {
		memset(&tvp, 0, sizeof(tvp));
		c->delivery_system = SYS_ANALOG;
		tvp.cmd = DTV_DELIVERY_SYSTEM;
		tvp.u.data = c->delivery_system;

		if (fe->ops.set_property)
			fe->ops.set_property(fe, &tvp);

		if (fe->ops.sleep)
			fe->ops.sleep(fe);
	} else if (!strncmp(parm[0], "attach", 6)) {
		demod->attach(demod, true);
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			demod_id = val;
		else
			demod_id = 0;

		if (demod_id >= dev->demod_num) {
			pr_err("demod index %d error, the max is %d, use default %d.\n",
					demod_id, dev->demod_num - 1,
					dev->demod_cur);

			goto EXIT;
		}

		dev->demod_cur = demod_id;
		ops = dvb_demod_ops_get_byindex(dev->demod_cur);
		if (ops && ops->fe) {
			dev->demod_fe = ops->fe;
			//memcpy(&fe->ops, &ops->fe->ops,
			//		sizeof(struct dvb_frontend_ops));
			//memcpy(&fe->dtv_property_cache,
			//		&ops->fe->dtv_property_cache,
			//		sizeof(struct dtv_frontend_properties));

			//fe->demodulator_priv = ops->fe->demodulator_priv;
			//fe->frontend_priv = ops->fe->frontend_priv;

			demod->used = ops;

			pr_err("demod %d attach done.\n", dev->demod_cur);
		}
	} else if (!strncmp(parm[0], "detach", 6)) {
		demod->attach(demod, false);

		dev->demod_fe = NULL;
	} else if (!strncmp(parm[0], "match", 5)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &val))
			fe_type = val;
		else
			fe_type = FE_ANALOG;

		if (fe_type > FE_ISDBT) {
			pr_err("input demod match fe type %d error.\n",
					fe_type);

			goto EXIT;
		}

		if (demod->match(demod, fe_type))
			pr_err("demod match fe type %d (%s) done.\n",
					fe_type, fe_type_name[fe_type]);
		else
			pr_err("demod match fe type %d (%s) fail.\n",
					fe_type, fe_type_name[fe_type]);

	} else if (!strncmp(parm[0], "detect", 6)) {
		demod->detect(demod);
	} else if (!strncmp(parm[0], "register", 8)) {
		demod->register_frontend(demod, true);
	} else if (!strncmp(parm[0], "unregister", 10)) {
		demod->register_frontend(demod, false);
	} else {
		pr_err("invalid command: %s.\n", parm[0]);
	}

EXIT:
	kfree(buf_orig);

	return count;
}

static ssize_t demod_debug_show(struct class *class,
		struct class_attribute *attr, char *buff)
{
	int n = 0;
	char *name = NULL;
	const char *path = "/sys/class/aml_dvb_extern/demod_debug";
	struct demod_ops *ops = NULL;
	struct dvb_demod *demod = get_dvb_demods();
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);

	n += sprintf(buff + n, "\nDemod Debug Usage (%s):\n", AML_DVB_EXTERN_VERSION);
	n += sprintf(buff + n, "[status]\necho status > %s\n", path);
	n += sprintf(buff + n, "[attach]\necho attach [demod_id] > %s\n", path);
	n += sprintf(buff + n, "[detach]\necho detach > %s\n", path);
	n += sprintf(buff + n, "[match]\necho match [fe_type] > %s\n", path);
	n += sprintf(buff + n, "\tfe_type:\n");
	n += sprintf(buff + n, "\tDVB-S/S2 [FE_QPSK]  : 0\n");
	n += sprintf(buff + n, "\tDVB-C    [FE_QAM]   : 1\n");
	n += sprintf(buff + n, "\tDVB-T/T2 [FE_OFDM]  : 2\n");
	n += sprintf(buff + n, "\tATSC     [FE_ATSC]  : 3\n");
	n += sprintf(buff + n, "\tANALOG   [FE_ANALOG]: 4\n");
	n += sprintf(buff + n, "\tDTMB     [FE_DTMB]  : 5\n");
	n += sprintf(buff + n, "\tISDBT    [FE_ISDBT] : 6\n");
	n += sprintf(buff + n, "[detect]\necho detect > %s\n", path);
	n += sprintf(buff + n, "[register]\necho register > %s\n", path);
	n += sprintf(buff + n, "[unregister]\necho unregister > %s\n", path);
	n += sprintf(buff + n, "[init]\n");
	n += sprintf(buff + n, "echo init [deliver_system] > %s\n", path);
	n += sprintf(buff + n, "\tdeliver_system:\n");
	n += sprintf(buff + n, "\tUNDEFINED    : 0\n");
	n += sprintf(buff + n, "\tDVBC_ANNEX_A : 1\n");
	n += sprintf(buff + n, "\tDVBC_ANNEX_B : 2\n");
	n += sprintf(buff + n, "\tDVBT         : 3\n");
	n += sprintf(buff + n, "\tDSS          : 4\n");
	n += sprintf(buff + n, "\tDVBS         : 5\n");
	n += sprintf(buff + n, "\tDVBS2        : 6\n");
	n += sprintf(buff + n, "\tDVBH         : 7\n");
	n += sprintf(buff + n, "\tISDBT        : 8\n");
	n += sprintf(buff + n, "\tISDBS        : 9\n");
	n += sprintf(buff + n, "\tISDBC        : 10\n");
	n += sprintf(buff + n, "\tATSC         : 11\n");
	n += sprintf(buff + n, "\tATSCMH       : 12\n");
	n += sprintf(buff + n, "\tDTMB         : 13\n");
	n += sprintf(buff + n, "\tCMMB         : 14\n");
	n += sprintf(buff + n, "\tDAB          : 15\n");
	n += sprintf(buff + n, "\tDVBT2        : 16\n");
	n += sprintf(buff + n, "\tTURBO        : 17\n");
	n += sprintf(buff + n, "\tDVBC_ANNEX_C : 18\n");
	n += sprintf(buff + n, "\tANALOG       : 19\n");
	n += sprintf(buff + n, "[tune]\n");
	n += sprintf(buff + n, "echo tune [frequency_hz] [symbol_rate_bps] ");
	n += sprintf(buff + n, "[bw_hz] [modulation] [rolloff]");
	n += sprintf(buff + n, " > %s\n", path);
	n += sprintf(buff + n, "\tmodulation:\n");
	n += sprintf(buff + n, "\tQPSK     : 0\n");
	n += sprintf(buff + n, "\tQAM_16   : 1\n");
	n += sprintf(buff + n, "\tQAM_32   : 2\n");
	n += sprintf(buff + n, "\tQAM_64   : 3\n");
	n += sprintf(buff + n, "\tQAM_128  : 4\n");
	n += sprintf(buff + n, "\tQAM_256  : 5\n");
	n += sprintf(buff + n, "\tQAM_AUTO : 6\n");
	n += sprintf(buff + n, "\tVSB_8    : 7\n");
	n += sprintf(buff + n, "\tVSB_16   : 8\n");
	n += sprintf(buff + n, "\tPSK_8    : 9\n");
	n += sprintf(buff + n, "\tAPSK_16  : 10\n");
	n += sprintf(buff + n, "\tAPSK_32  : 11\n");
	n += sprintf(buff + n, "\tDQPSK    : 12\n");
	n += sprintf(buff + n, "\tQAM_4_NR : 13\n");
	n += sprintf(buff + n, "\trolloff:\n");
	n += sprintf(buff + n, "\tROLLOFF_35   : 0\n");
	n += sprintf(buff + n, "\tROLLOFF_20   : 1\n");
	n += sprintf(buff + n, "\tROLLOFF_25   : 2\n");
	n += sprintf(buff + n, "\tROLLOFF_AUTO : 3\n");
	n += sprintf(buff + n, "[uninit]\necho uninit > %s\n", path);

	n += sprintf(buff + n, "\n------------------------------------------------------------\n");
	n += sprintf(buff + n, "\nDemod Status:\n");
	n += sprintf(buff + n, "demod numbers: %d.\n", dev->demod_num);
	n += sprintf(buff + n, "all demods:\n");
	list_for_each_entry(ops, &demod->list, list) {
		name = ops->fe ? ops->fe->ops.info.name : "";
		n += sprintf(buff + n, "demod%d, id %d (%s) %s, %sregistered, %sdetected, use tuner0 %s, tuner1 %s.\n",
				ops->index, ops->cfg.id,
				name ? name : "", ops->attached ?
				"attached" : "detached",
				ops->registered ? "" : "un",
				ops->cfg.detect ? (ops->valid ? "" : "un") : "no ",
				ops->cfg.tuner0.name ? ops->cfg.tuner0.name : "NONE",
				ops->cfg.tuner1.name ? ops->cfg.tuner1.name : "NONE");
	}

	if (demod->used && demod->used->fe) {
		name = demod->used->fe->ops.info.name;
		n += sprintf(buff + n,
				"current use demod%d, id %d (%s), fe type %d, delivery system %d.\n",
				demod->used->index,
				demod->used->cfg.id,
				name ? name : "",
				demod->used->type,
				demod->used->fe->dtv_property_cache.delivery_system);
	}

	n += sprintf(buff + n, "\n");

	return n;
}

static ssize_t dvb_debug_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	unsigned long val = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL;
	char *parm[10] = { NULL };
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (!token)
			break;

		if (*token == '\0')
			continue;

		if (n >= 10)
			break;

		parm[n++] = token;
	}

	if (!parm[0])
		goto EXIT;

	if (!strncmp(parm[0], "dvb_power", 9)) {
		if (parm[1] && !kstrtoul(parm[1], 0, &val)) {
			aml_dvb_extern_set_power(&dev->dvb_power, val);

			pr_err("set dvb power: %s.\n", val ? "ON" : "OFF");
		} else {
			pr_err("please input power status of DVB to be set: 1[ON] or 0[OFF].\n");
		}
	} else {
		pr_err("invalid command: %s.\n", parm[0]);
	}

EXIT:
	kfree(buf_orig);

	return count;
}

static ssize_t dvb_debug_show(struct class *class,
		struct class_attribute *attr, char *buff)
{
	int n = 0;
	const char *path = "/sys/class/aml_dvb_extern/dvb_debug";
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);

	n += sprintf(buff + n, "\nDVB Extern Version: %s.\n",
			AML_DVB_EXTERN_VERSION);
	n += sprintf(buff + n, "\ndvb power gpio: %d, dir: %d, value: %d.\n",
			dev->dvb_power.pin, dev->dvb_power.dir, dev->dvb_power.value);
	n += sprintf(buff + n, "[set dvb power]\necho dvb_power [value] > %s\n",
			path);
	n += sprintf(buff + n, "\tvalue: 1 - [ON], 0 - [OFF].\n");

	return n;
}

static int tuner_debug_seq_show(struct seq_file *m, void *v)
{
	char *name = NULL;
	struct tuner_ops *ops = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();
	u32 status = FE_TIMEDOUT, frequency = 0, bandwidth = 0;
	u32 if_frequency[2] = { 0 };
	s16 strength = 0;
	struct dvb_extern_device *dev = m->private;

	seq_puts(m, "\n------------------------------------------------------------\n");
	seq_puts(m, "Tuner Status:\n");
	seq_printf(m, "tuner numbers: %d.\n", dev->tuner_num);
	seq_puts(m, "all tuners:\n");
	list_for_each_entry(ops, &tuner->list, list) {
		name = ops->fe.ops.tuner_ops.info.name;
		seq_printf(m, "tuner %d, id %d (%s) addr 0x%x, adap 0x%p %s, %sdetected.\n",
				ops->index, ops->cfg.id,
				name ? name : "",
				ops->cfg.i2c_addr, ops->cfg.i2c_adap,
				ops->attached ? "attached" : "detached",
				ops->cfg.detect ? (ops->valid ? "" : "un") : "no ");
	}

	if (tuner->used) {
		name = tuner->used->fe.ops.tuner_ops.info.name;
		seq_printf(m, "current use tuner%d, id %d (%s), fe type %d.\n",
				tuner->used->index,
				tuner->used->cfg.id,
				name ? name : "",
				tuner->used->type);

		if (tuner->used->fe.ops.tuner_ops.get_status) {
			tuner->used->fe.ops.tuner_ops.get_status(
					&tuner->used->fe, &status);
			seq_printf(m, "signal status: %s.\n",
					status & FE_HAS_LOCK ? "Locked" : "Unlocked");
		}

		if (tuner->used->fe.ops.tuner_ops.get_strength) {
			tuner->used->fe.ops.tuner_ops.get_strength(
					&tuner->used->fe, &strength);
			seq_printf(m, "strength: %d dBm.\n", strength);
		}

		if (tuner->used->fe.ops.tuner_ops.get_frequency) {
			tuner->used->fe.ops.tuner_ops.get_frequency(
					&tuner->used->fe, &frequency);
			seq_printf(m, "frequency: %d Hz.\n", frequency);
		}

		if (tuner->used->fe.ops.tuner_ops.get_bandwidth) {
			tuner->used->fe.ops.tuner_ops.get_bandwidth(
					&tuner->used->fe, &bandwidth);
			seq_printf(m, "bandwidth: %d Hz.\n", bandwidth);
		}

		if (tuner->used->fe.ops.tuner_ops.get_if_frequency) {
			tuner->used->fe.ops.tuner_ops.get_if_frequency(
					&tuner->used->fe, if_frequency);
			seq_printf(m, "if frequency: %d Hz (IF spectrum %s).\n",
					if_frequency[1],
					if_frequency[0] ? "inverted" : "normal");
		}
	}

	seq_puts(m, "------------------------------------------------------------\n\n");

	return 0;
}

static int tuner_debug_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, tuner_debug_seq_show, PDE_DATA(inode));
}

static const struct file_operations tuner_debug_fops = {
	.owner = THIS_MODULE,
	.open = tuner_debug_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int demod_debug_seq_show(struct seq_file *m, void *v)
{
	char *name = NULL;
	struct demod_ops *ops = NULL;
	struct dvb_demod *demod = get_dvb_demods();
	struct dvb_extern_device *dev = m->private;

	seq_puts(m, "\nDemod Status:\n");
	seq_printf(m, "demod numbers: %d.\n", dev->demod_num);
	seq_puts(m, "all demods:\n");
	list_for_each_entry(ops, &demod->list, list) {
		name = ops->fe ? ops->fe->ops.info.name : "";
		seq_printf(m, "demod%d, id %d (%s) %s, %sregistered, %sdetected, use tuner0 %s, tuner1 %s.\n",
				ops->index, ops->cfg.id,
				name ? name : "", ops->attached ?
				"attached" : "detached",
				ops->registered ? "" : "un",
				ops->cfg.detect ? (ops->valid ? "" : "un") : "no ",
				ops->cfg.tuner0.name ? ops->cfg.tuner0.name : "NONE",
				ops->cfg.tuner1.name ? ops->cfg.tuner1.name : "NONE");
	}

	if (demod->used && demod->used->fe) {
		name = demod->used->fe->ops.info.name;
		seq_printf(m, "current use demod%d, id %d (%s), fe type %d, delivery system %d.\n",
				demod->used->index,
				demod->used->cfg.id,
				name ? name : "",
				demod->used->type,
				demod->used->fe->dtv_property_cache.delivery_system);
	}

	seq_puts(m, "\n");

	return 0;
}

static int demod_debug_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, demod_debug_seq_show, PDE_DATA(inode));
}

static const struct file_operations demod_debug_fops = {
	.owner = THIS_MODULE,
	.open = demod_debug_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int cfgdev_open(struct inode *inode, struct file *filp)
{
	int num = -1;

	num = MINOR(inode->i_rdev);
	if (num >= DVB_CFGDEV_COUNT)
		return -ENODEV;

	return 0;
}

static int dvb_cfgdev_set_tuner(void *pcfg)
{
	struct tuner_ops *tops = NULL;
	int ret = -1;

	if (!pcfg)
		return ret;

	tops = dvb_tuner_ops_create();
	if (!tops) {
		pr_err("create dvb tuner ops fail.\n");
		return ret;
	}

	memcpy(&tops->cfg, (struct tuner_config *)pcfg, sizeof(struct tuner_config));

	/*get i2c_adapter*/
	tops->cfg.i2c_adap = i2c_get_adapter(tops->cfg.i2c_id);
	tops->dts_cfg = false;
	tops->index = dvb_tuner_ops_get_index();

	pr_err("add tuner%d, id %d, i2c_addr 0x%x.\n",
		tops->index, tops->cfg.id, tops->cfg.i2c_addr);

	/* add to tuner */
	ret = dvb_tuner_ops_add(tops);
	if (ret)
		pr_err("add tuner ops fail, ret = %d.\n", ret);

	return ret;
}

static long cfgdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tuner_config cfg_arg;
	struct compact_tuner_config cpt_arg;

	/*check cmd's validity*/
	if (_IOC_TYPE(cmd) != DVB_CFGDEV_IOC_MAGIC) {
		pr_err("check ioc_type error!");
		return -EINVAL;
	}
	if (_IOC_NR(cmd) > DVB_CFGDEV_IOC_MAXNR) {
		pr_err("check ioc_nr error!");
		return -EINVAL;
	}

	if (cmd == DVB_CFGDEV_IOC_SET_PARAM) {
		pr_err("set param !!");
		if (copy_from_user(&cfg_arg, (struct tuner_config *)arg,
			sizeof(struct tuner_config))) {
			return -EINVAL;
		}
		dvb_cfgdev_set_tuner(&cfg_arg);
	} else if (cmd == COMPAT_DVB_CFGDEV_IOC_SET_PARAM) {
		pr_err("compat set param !!");
		if (copy_from_user(&cpt_arg, (struct compact_tuner_config *)arg,
			sizeof(struct compact_tuner_config))) {
			return -EINVAL;
		}
		cfg_arg.name       = NULL;
		cfg_arg.code       = cpt_arg.code;
		cfg_arg.id         = cpt_arg.id;
		cfg_arg.i2c_addr   = cpt_arg.i2c_addr;
		cfg_arg.i2c_id     = cpt_arg.i2c_id;
		cfg_arg.i2c_adap   = NULL;
		cfg_arg.xtal       = cpt_arg.xtal;
		cfg_arg.xtal_cap   = cpt_arg.xtal_cap;
		cfg_arg.xtal_mode  = cpt_arg.xtal_mode;
		cfg_arg.lt_out     = cpt_arg.lt_out;
		cfg_arg.dual_power = cpt_arg.dual_power;
		cfg_arg.if_agc     = cpt_arg.if_agc;
		cfg_arg.if_hz      = cpt_arg.if_hz;
		cfg_arg.if_invert  = cpt_arg.if_invert;
		cfg_arg.if_amp     = cpt_arg.if_amp;
		cfg_arg.detect     = cpt_arg.detect;
		cfg_arg.reset      = cpt_arg.reset;
		cfg_arg.power      = cpt_arg.power;
		cfg_arg.reserved0  = cpt_arg.reserved0;
		cfg_arg.reserved1  = cpt_arg.reserved1;
		dvb_cfgdev_set_tuner(&cfg_arg);
	} else if (cmd == DVB_CFGDEV_IOC_GET_PARAM) {
		pr_err("get param !!");
	} else {
		pr_err("default.\n");
		return -EINVAL;
	}
	pr_err("%s --\n", __func__);
	return 0;
}

#ifdef CONFIG_COMPAT
static long cfgdev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	cfgdev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));

	return ret;
}
#endif

static const struct file_operations cfg_dev_fops = {
	.owner = THIS_MODULE,
	.open = cfgdev_open,
	.unlocked_ioctl = cfgdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cfgdev_compat_ioctl,
#endif
};

static CLASS_ATTR_RW(tuner_debug);
static CLASS_ATTR_RW(demod_debug);
static CLASS_ATTR_RW(dvb_debug);

static struct attribute *dvb_extern_class_attrs[] = {
	&class_attr_tuner_debug.attr,
	&class_attr_demod_debug.attr,
	&class_attr_dvb_debug.attr,
	NULL
};

ATTRIBUTE_GROUPS(dvb_extern_class);

static const struct of_device_id aml_dvb_extern_dt_match[] = {
	{
		.compatible = "amlogic, dvb-extern"
	},
	{
	}
};

static int aml_dvb_extern_probe(struct platform_device *pdev)
{
	int ret = -1, i = 0;
	unsigned int val = 0;
	char buf[32] = { 0 };
	const char *str = NULL;
	struct tuner_ops *tops = NULL;
	struct demod_ops *dops = NULL;
	struct dvb_extern_device *dvbdev = NULL;
	struct device *pcfgdev = NULL;
	static int cdevno_maj;
	static int cdevno_min;

	if (!IS_ERR_OR_NULL(dvb_extern_dev))
		goto PROPERTY_DONE;

	dvbdev = kzalloc(sizeof(*dvbdev), GFP_KERNEL);
	if (!dvbdev) {
		/* pr_err("allocate for dvb extern device fail.\n"); */
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dvbdev);

	dvbdev->name = AML_DVB_EXTERN_DEVICE_NAME;
	dvbdev->dev = &pdev->dev;
	dvbdev->class.name = AML_DVB_EXTERN_DEVICE_NAME;
	dvbdev->class.owner = THIS_MODULE;
	dvbdev->class.class_groups = dvb_extern_class_groups;

	dvbdev->debug_proc_dir = proc_mkdir(AML_DVB_EXTERN_DEVICE_NAME, NULL);
	if (!dvbdev->debug_proc_dir)
		goto fail_proc_dir;

	if (!proc_create_data("tuner_debug", 0644, dvbdev->debug_proc_dir,
			&tuner_debug_fops, dvbdev)) {
		pr_err("proc create tuner_debug fail.\n");
		goto fail_proc_create;
	}

	if (!proc_create_data("demod_debug", 0644, dvbdev->debug_proc_dir,
			&demod_debug_fops, dvbdev)) {
		pr_err("proc create tuner_debug fail.\n");
		goto fail_proc_create;
	}

	if (class_register(&dvbdev->class)) {
		pr_err("class register fail.\n");
		goto fail_class_register;
	}

	cdevno_maj = register_chrdev(cdevno_maj, AML_DVB_EXTERN_DEVICE_NAME, &cfg_dev_fops);
	dvbdev->cfgdevno = MKDEV(cdevno_maj, cdevno_min);
	pr_err("aml_dvb_cfgdev: MAJOR Number %d, MINOR Numbe %d\n",
		MAJOR(dvbdev->cfgdevno), MINOR(dvbdev->cfgdevno));

	pcfgdev = device_create(&dvbdev->class, NULL, dvbdev->cfgdevno,
		NULL, AML_DVB_EXTERN_DEVICE_NAME);
	if (!pcfgdev) {
		pr_err("%s device create fail.\n", AML_DVB_EXTERN_DEVICE_NAME);
		unregister_chrdev(MAJOR(dvbdev->cfgdevno), AML_DVB_EXTERN_DEVICE_NAME);
	} else {
		dvbdev->cdev = *pcfgdev;
	}

	/* DVB Power pin. */
	str = NULL;
	ret = of_property_read_string(pdev->dev.of_node,
			"dvb_power_gpio", &str);
	if (ret) {
		dvbdev->dvb_power.pin = -1;
	} else {
		dvbdev->dvb_power.pin = of_get_named_gpio_flags(pdev->dev.of_node,
				"dvb_power_gpio", 0, NULL);
		pr_err("get dvb_power_gpio: %d.\n", dvbdev->dvb_power.pin);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "dvb_power_dir", &val);
	if (ret) {
		dvbdev->dvb_power.dir = 0;
	} else {
		dvbdev->dvb_power.dir = val;
		pr_err("get dvb_power_dir: %d (%s).\n",
				dvbdev->dvb_power.pin, val ? "IN" : "OUT");
	}

	ret = of_property_read_u32(pdev->dev.of_node, "dvb_power_value", &val);
	if (ret) {
		dvbdev->dvb_power.value = 0;
	} else {
		dvbdev->dvb_power.value = val;
		pr_err("get dvb_power_value: %d.\n", dvbdev->dvb_power.value);
	}

	/* PROPERTY_TUNER */
	ret = of_property_read_u32(pdev->dev.of_node, "tuner_num", &val);
	if (ret == 0) {
		dvbdev->tuner_num = val;
		pr_debug("find dvb tuner numbers %d.\n", dvbdev->tuner_num);
	} else {
		pr_err("can't find tuner_num.\n");
		goto PROPERTY_DEMOD;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "tuner_cur", &val);
	if (ret)
		dvbdev->tuner_cur = 0;
	else
		dvbdev->tuner_cur = val;

	for (i = 0; i < dvbdev->tuner_num; ++i) {
		tops = dvb_tuner_ops_create();
		if (!tops) {
			pr_err("create dvb tuner ops fail.\n");
			goto fail_tuner_create;
		}

		ret = aml_get_dts_tuner_config(pdev->dev.of_node,
				&tops->cfg, i);
		if (ret) {
			dvb_tuner_ops_destroy(tops);

			pr_err("can't find tuner%d.\n", i);
		} else {
			tops->index = i;

			pr_debug("find tuner%d, id %d, i2c_addr 0x%x.\n",
				i, tops->cfg.id, tops->cfg.i2c_addr);

			ret = dvb_tuner_ops_add(tops);
			if (ret)
				pr_err("add tuner ops fail, ret = %d.\n", ret);
		}
	}

PROPERTY_DEMOD:
	ret = of_property_read_u32(pdev->dev.of_node, "fe_num", &val);
	if (ret == 0) {
		dvbdev->demod_num = val;
		pr_debug("find dvb fe numbers %d.\n", dvbdev->demod_num);
	} else {
		pr_err("can't find fe_num.\n");
		goto PROPERTY_DONE;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "fe_cur", &val);
	if (ret)
		dvbdev->demod_cur = 0;
	else
		dvbdev->demod_cur = val;

	for (i = 0; i < dvbdev->demod_num; ++i) {
		dops = dvb_demod_ops_create();
		if (!dops) {
			pr_err("create dvb demod ops fail.\n");
			goto fail_demod_create;
		}

		ret = aml_get_dts_demod_config(pdev->dev.of_node,
				&dops->cfg, i);
		if (ret) {
			dvb_demod_ops_destroy(dops);

			pr_err("can't find demod%d.\n", i);

			continue;
		}

		dops->index = i;

		pr_debug("find demod%d, id %d, i2c_addr 0x%x.\n",
				i, dops->cfg.id, dops->cfg.i2c_addr);

		ret = dvb_demod_ops_add(dops);
		if (ret) {
			pr_err("add demod ops fail, ret = %d.\n", ret);

			continue;
		}

		/* Tuner configuration for the external demod.
		 * demod includes the tuner driver.
		 */
		/* T/C tuner */
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "fe%d_tuner0", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &val);
		if (!ret) {
			tops = dvb_tuner_ops_get_byindex(val);
			if (tops)
				memcpy(&dops->cfg.tuner0, &tops->cfg,
						sizeof(struct tuner_config));

			pr_debug("get %s %d.\n", buf, val);
		}

		/* S/S2 tuner */
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "fe%d_tuner1", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &val);
		if (!ret) {
			tops = dvb_tuner_ops_get_byindex(val);
			if (tops)
				memcpy(&dops->cfg.tuner1, &tops->cfg,
						sizeof(struct tuner_config));

			pr_debug("get %s %d.\n", buf, val);
		}
	}

	aml_demod_gpio_config(&dvbdev->dvb_power, "dvb_power");
	aml_dvb_extern_set_power(&dvbdev->dvb_power, 1);

	INIT_WORK(&dvbdev->work, aml_dvb_extern_work);

PROPERTY_DONE:
	dvb_extern_dev = dvbdev;

	pr_debug("%s: OK.\n", __func__);

	return 0;

fail_tuner_create:
fail_demod_create:
	class_unregister(&dvbdev->class);

fail_class_register:
fail_proc_create:
	proc_remove(dvbdev->debug_proc_dir);

fail_proc_dir:
	kfree(dvbdev);
	dvbdev = NULL;

	pr_info("%s: fail, ret = %d.\n", __func__, ret);

	return ret;
}

static int aml_dvb_extern_remove(struct platform_device *pdev)
{
	struct dvb_extern_device *dvbdev = platform_get_drvdata(pdev);

	if (IS_ERR_OR_NULL(dvbdev))
		return -EFAULT;

	dvb_tuner_ops_destroy_all();
	dvb_demod_ops_destroy_all();

	unregister_chrdev(MAJOR(dvbdev->cfgdevno), AML_DVB_EXTERN_DEVICE_NAME);
	device_destroy(&dvbdev->class, dvbdev->cfgdevno);

	class_unregister(&dvbdev->class);
	if (dvbdev->debug_proc_dir)
		proc_remove(dvbdev->debug_proc_dir);

	aml_dvb_extern_set_power(&dvbdev->dvb_power, 0);

	kfree(dvbdev);
	dvbdev = NULL;
	dvb_extern_dev = NULL;

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void aml_dvb_extern_shutdown(struct platform_device *pdev)
{
	struct dvb_extern_device *dvbdev = platform_get_drvdata(pdev);

	if (IS_ERR_OR_NULL(dvbdev))
		return;

	aml_dvb_extern_set_power(&dvbdev->dvb_power, 0);
}

static int aml_dvb_extern_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct dvb_extern_device *dvbdev = platform_get_drvdata(pdev);
	struct tuner_ops *tops = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();

	if (IS_ERR_OR_NULL(dvbdev))
		return -EFAULT;

	list_for_each_entry(tops, &tuner->list, list) {
		if (tops->fe.ops.tuner_ops.suspend)
			tops->fe.ops.tuner_ops.suspend(&tops->fe);
		else if (tops->fe.ops.tuner_ops.sleep)
			tops->fe.ops.tuner_ops.sleep(&tops->fe);
	}

	aml_dvb_extern_set_power(&dvbdev->dvb_power, 0);

	return 0;
}

static int aml_dvb_extern_resume(struct platform_device *pdev)
{
	struct dvb_extern_device *dvbdev = platform_get_drvdata(pdev);

	if (IS_ERR_OR_NULL(dvbdev))
		return -EFAULT;

	aml_dvb_extern_set_power(&dvbdev->dvb_power, 1);

	schedule_work(&dvbdev->work);

	return 0;
}

static struct platform_driver aml_dvb_extern_driver = {
	.driver = {
		.name = AML_DVB_EXTERN_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_dvb_extern_dt_match
	},
	.probe    = aml_dvb_extern_probe,
	.remove   = aml_dvb_extern_remove,
	.shutdown = aml_dvb_extern_shutdown,
	.suspend  = aml_dvb_extern_suspend,
	.resume   = aml_dvb_extern_resume
};

static int __init aml_dvb_extern_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_dvb_extern_driver);
	if (ret) {
		pr_err("%s: failed to register driver, ret %d.\n",
				__func__, ret);
		return ret;
	}

	pr_info("%s: OK, version: %s.\n", __func__, AML_DVB_EXTERN_VERSION);

	return 0;
}

static void __exit aml_dvb_extern_exit(void)
{
	platform_driver_unregister(&aml_dvb_extern_driver);

	pr_info("%s: OK.\n", __func__);
}

module_init(aml_dvb_extern_init);
module_exit(aml_dvb_extern_exit);

MODULE_AUTHOR("nengwen.chen <nengwen.chen@amlogic.com>");
MODULE_DESCRIPTION("aml dvb extern device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(AML_DVB_EXTERN_VERSION);
