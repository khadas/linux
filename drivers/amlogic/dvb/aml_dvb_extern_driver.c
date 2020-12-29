// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_tuner.h>
#include <linux/amlogic/aml_dtvdemod.h>
#include <linux/amlogic/aml_dvb_extern.h>

#include "aml_dvb_extern_driver.h"

#define AML_DVB_EXTERN_DEVICE_NAME    "aml_dvb_extern"
#define AML_DVB_EXTERN_DRIVER_NAME    "aml_dvb_extern"
#define AML_DVB_EXTERN_MODULE_NAME    "aml_dvb_extern"
#define AML_DVB_EXTERN_CLASS_NAME     "aml_dvb_extern"

#define AML_DVB_EXTERN_VERSION    "V1.02"

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

static ssize_t tuner_debug_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	unsigned int ret = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL, *name = NULL;
	char *parm[10] = { NULL };
	unsigned long val = 0, symbol_rate = 0, rolloff = 0;
	struct tuner_ops *ops = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);
	struct dvb_frontend *fe = &dev->fe;
	struct analog_parameters *p = &dev->para;

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

	if (!strncmp(parm[0], "init", 4)) {
		if (parm[1] && kstrtoul(parm[1], 0, &val) == 0)
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
		if (fe->ops.tuner_ops.set_config)
			ret = fe->ops.tuner_ops.set_config(fe, NULL);
	} else if (!strncmp(parm[0], "tune", 4)) {
		if (parm[1] && kstrtoul(parm[1], 0, &val) == 0) {
			if (fe->ops.info.type == FE_ANALOG) {
				p->frequency = val;
				if (fe->ops.tuner_ops.set_analog_params)
					ret = fe->ops.tuner_ops.set_analog_params(fe, p);
			} else {
				struct dtv_frontend_properties *c = &fe->dtv_property_cache;

				c->frequency = val;

				if (fe->ops.info.type == FE_QPSK) {
					c->frequency = c->frequency / 1000;
					if (parm[2] && kstrtoul(parm[2], 0, &symbol_rate) == 0)
						c->symbol_rate = symbol_rate;
					else
						pr_err("tune mode DVB-S, need to set symbol rate.\n");

					if (parm[3] && kstrtoul(parm[3], 0, &rolloff) == 0)
						c->rolloff = rolloff;
					else
						pr_err("tune mode DVB-S, need to set Roll off factor.\n");
				}

				if (fe->ops.tuner_ops.set_params)
					ret = fe->ops.tuner_ops.set_params(fe);
			}

			if (fe->ops.info.type == FE_QPSK) {
				pr_err("tune mode [%d][%s] freq[%ld] symbol_rate[%ld] rolloff[%ld].\n",
						fe->ops.info.type,
						fe_type_name[fe->ops.info.type],
						val, symbol_rate, rolloff);
			} else {
				pr_err("tune mode [%d][%s] freq[%ld].\n",
						fe->ops.info.type,
						fe_type_name[fe->ops.info.type], val);
			}
		} else {
			pr_err("please input frequency.\n");
		}
	} else if (!strncmp(parm[0], "std", 3)) {
		if (!strncmp(parm[1], "pal", 3))
			p->std = V4L2_COLOR_STD_PAL;
		else if (!strncmp(parm[1], "ntsc", 4))
			p->std = V4L2_COLOR_STD_NTSC;
		else if (!strncmp(parm[1], "secam", 5))
			p->std = V4L2_COLOR_STD_SECAM;
		else
			pr_err("invaild video std.\n");

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
			pr_err("invaild audio std.\n");
	} else if (!strncmp(parm[0], "status", 6)) {
		pr_err("tuner numbers %d.\n", dev->tuner_num);
		pr_err("all tuners:\n");
		list_for_each_entry(ops, &tuner->list, list) {
			name = ops->fe.ops.tuner_ops.info.name;
			pr_err("tuner %d, id %d (%s) %s.\n",
					ops->index, ops->cfg.id,
					name ? name : "", ops->attached ?
					"attached" : "detached");
		}

		if (tuner->used) {
			name = tuner->used->fe.ops.tuner_ops.info.name;
			pr_err("current use tuner %d, id %d (%s), fe type %d.\n",
					tuner->used->index,
					tuner->used->cfg.id,
					name ? name : "",
					tuner->used->type);
		}
	} else if (!strncmp(parm[0], "uninit", 6)) {
		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
	} else if (!strncmp(parm[0], "attach", 6)) {
		tuner->attach(tuner, true);
		if (parm[1])
			ret = kstrtoul(parm[1], 0, &val);
		else
			val = 0;

		if (val >= dev->tuner_num) {
			pr_err("tuner index %ld error, the max is %d, use default %d.\n",
					val, dev->tuner_num - 1, dev->tuner_cur);

			goto EXIT;
		}

		dev->tuner_cur = val;
		ops = dvb_tuner_ops_get_byindex(dev->tuner_cur);
		if (ops) {
			memcpy(&fe->ops.tuner_ops, &ops->fe.ops.tuner_ops,
					sizeof(struct dvb_tuner_ops));
			fe->tuner_priv = ops->fe.tuner_priv;

			pr_err("tuner %d attach done.\n", dev->tuner_cur);
		}
	} else if (!strncmp(parm[0], "detach", 6)) {
		tuner->attach(tuner, false);
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

	n += sprintf(buff + n, "\nTuner Debug Usage:\n");
	n += sprintf(buff + n, "[status]\necho status > %s\n", path);
	n += sprintf(buff + n, "[attach]\necho attach > %s\n", path);
	n += sprintf(buff + n, "[detach]\necho detach > %s\n", path);
	n += sprintf(buff + n, "[init]\n");
	n += sprintf(buff + n, "echo init [fe_type] > %s\n", path);
	n += sprintf(buff + n, "\tfe_type:\n");
	n += sprintf(buff + n, "\tDVB-S  [FE_QPSK]  : 0\n");
	n += sprintf(buff + n, "\tDVB-C  [FE_QAM]   : 1\n");
	n += sprintf(buff + n, "\tDVB-T  [FE_OFDM]  : 2\n");
	n += sprintf(buff + n, "\tATSC   [FE_ATSC]  : 3\n");
	n += sprintf(buff + n, "\tANALOG [FE_ANALOG]: 4\n");
	n += sprintf(buff + n, "\tDTMB   [FE_DTMB]  : 5\n");
	n += sprintf(buff + n, "\tISDBT  [FE_ISDBT] : 6\n");
	n += sprintf(buff + n, "[set atv std]\n");
	n += sprintf(buff + n, "echo std [vstd] [astd] > %s\n", path);
	n += sprintf(buff + n, "\tvstd: pal, ntsc, secam\n");
	n += sprintf(buff + n, "\tastd: dk, bg, i, nm, pm, l, lc, n\n");
	n += sprintf(buff + n, "[tune]\n");
	n += sprintf(buff + n, "echo tune [frequency_hz] [symbol_rate_bps] [rolloff] > %s\n", path);
	n += sprintf(buff + n, "\trolloff (rt710 tuner):\n");
	n += sprintf(buff + n, "\t0: roll-off = 0.35\n");
	n += sprintf(buff + n, "\t1: roll-off = 0.20\n");
	n += sprintf(buff + n, "\t2: roll-off = 0.25\n");
	n += sprintf(buff + n, "\t3: roll-off = 0.40\n");
	n += sprintf(buff + n, "[uninit]\necho uninit > %s\n", path);

	n += sprintf(buff + n, "\ntuner status:\n");
	n += sprintf(buff + n, "tuner numbers %d.\n", dev->tuner_num);
	n += sprintf(buff + n, "all tuners:\n");
	list_for_each_entry(ops, &tuner->list, list) {
		name = ops->fe.ops.tuner_ops.info.name;
		n += sprintf(buff + n, "tuner %d, id %d (%s) %s.\n",
				ops->index, ops->cfg.id,
				name ? name : "", ops->attached ?
				"attached" : "detached");
	}

	if (tuner->used) {
		name = tuner->used->fe.ops.tuner_ops.info.name;
		n += sprintf(buff + n, "current use tuner %d, id %d (%s), fe type %d.\n",
				tuner->used->index,
				tuner->used->cfg.id,
				name ? name : "",
				tuner->used->type);
	}

	n += sprintf(buff + n, "\n");

	return n;
}

static ssize_t demod_debug_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	unsigned int ret = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL, *name = NULL;
	char *parm[10] = { NULL };
	unsigned long val = 0, freq = 0, symbol = 0, bw = 0, modul = 0;
	struct demod_ops *ops = NULL;
	struct dvb_demod *demod = get_dvb_demods();
	struct dvb_extern_device *dev =
			container_of(class, struct dvb_extern_device, class);
	struct dvb_frontend *fe = &dev->fe;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct dtv_property tvp;

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

	if (!strncmp(parm[0], "init", 4)) {
		if (parm[1])
			ret = kstrtoul(parm[1], 0, &val);
		else
			val = SYS_DTMB;

		if (val > 19) {
			pr_err("init demod delsys value [%ld] error.\n", val);
			goto EXIT;
		}

		memset(&tvp, 0, sizeof(tvp));
		c->delivery_system = val;
		tvp.cmd = DTV_DELIVERY_SYSTEM;
		tvp.u.data = c->delivery_system;

		pr_err("init demod delsys [%d][%s].\n",
				c->delivery_system,
				fe_delsys_name[c->delivery_system]);
		if (fe->ops.set_property)
			ret = fe->ops.set_property(fe, &tvp);
	} else if (!strncmp(parm[0], "tune", 4)) {
		if (parm[1])
			ret = kstrtoul(parm[1], 0, &freq);
		else
			freq = 88000000;

		if (parm[2])
			ret = kstrtoul(parm[2], 0, &symbol);
		else
			symbol = 0;

		if (parm[3])
			ret = kstrtoul(parm[3], 0, &bw);
		else
			bw = 8000000;

		if (parm[4])
			ret = kstrtoul(parm[4], 0, &modul);
		else
			modul = 0;

		if (c->delivery_system == SYS_DVBS || c->delivery_system == SYS_DVBS2)
			c->frequency = freq / 1000;
		else
			c->frequency = freq;

		c->symbol_rate = symbol;
		c->bandwidth_hz = bw;
		c->modulation = modul;

		pr_err("[mxl258c] [tune] delivery_system %d, frequency %d, symbol_rate %d, bandwidth_hz %d, modulation %d, inversion %d.\n",
				c->delivery_system, c->frequency, c->symbol_rate,
				c->bandwidth_hz, c->modulation, c->inversion);

		if (fe->ops.set_frontend)
			fe->ops.set_frontend(fe);

	} else if (!strncmp(parm[0], "status", 6)) {
		pr_err("demod numbers %d.\n", dev->demod_num);
		pr_err("all demods:\n");
		list_for_each_entry(ops, &demod->list, list) {
			name = demod->used->fe ? ops->fe->ops.info.name : "";
			pr_err("demod %d, id %d (%s) %s, %sregistered.\n",
					ops->index, ops->cfg.id,
					name ? name : "", ops->attached ?
					"attached" : "detached",
					ops->registered ? "" : "un");
		}

		if (demod->used && demod->used->fe) {
			name = ops->fe->ops.info.name;
			pr_err("current use demod %d, id %d (%s), fe type %d, delivery system %d.\n",
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
			ret = fe->ops.set_property(fe, &tvp);
	} else if (!strncmp(parm[0], "attach", 6)) {
		demod->attach(demod, true);
		if (parm[1])
			ret = kstrtoul(parm[1], 0, &val);
		else
			val = 0;

		if (val >= dev->demod_num) {
			pr_err("demod index %ld error, the max is %d, use default %d.\n",
					val, dev->demod_num - 1, dev->demod_cur);

			goto EXIT;
		}

		dev->demod_cur = val;
		ops = dvb_demod_ops_get_byindex(dev->demod_cur);
		if (ops) {
			memcpy(&fe->ops, &ops->fe->ops,
					sizeof(struct dvb_frontend_ops));
			memcpy(&fe->dtv_property_cache,
					&ops->fe->dtv_property_cache,
					sizeof(struct dtv_frontend_properties));

			fe->demodulator_priv = ops->fe->demodulator_priv;
			fe->frontend_priv = ops->fe->frontend_priv;

			demod->used = ops;

			pr_err("demod %d attach done.\n", dev->demod_cur);
		}
	} else if (!strncmp(parm[0], "detach", 6)) {
		demod->attach(demod, false);
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

	n += sprintf(buff + n, "\nDemod Debug Usage:\n");
	n += sprintf(buff + n, "[status]\necho status > %s\n", path);
	n += sprintf(buff + n, "[attach]\necho attach > %s\n", path);
	n += sprintf(buff + n, "[detach]\necho detach > %s\n", path);
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
	n += sprintf(buff + n, "echo tune [frequency_hz] [symbol_rate_bps] [bw_hz] [modulation]");
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
	n += sprintf(buff + n, "[uninit]\necho uninit > %s\n", path);

	n += sprintf(buff + n, "\ndemod status:\n");
	n += sprintf(buff + n, "demod numbers %d.\n", dev->demod_num);
	n += sprintf(buff + n, "all demods:\n");
	list_for_each_entry(ops, &demod->list, list) {
		name = ops->fe ? ops->fe->ops.info.name : "";
		n += sprintf(buff + n, "demod %d, id %d (%s) %s, %sregistered.\n",
				ops->index, ops->cfg.id,
				name ? name : "", ops->attached ?
				"attached" : "detached",
				ops->registered ? "" : "un");
	}

	if (demod->used && demod->used->fe) {
		name = demod->used->fe->ops.info.name;
		n += sprintf(buff + n,
				"current use demod %d, id %d (%s), fe type %d, delivery system %d.\n",
				demod->used->index,
				demod->used->cfg.id,
				name ? name : "",
				demod->used->type,
				demod->used->fe->dtv_property_cache.delivery_system);
	}

	n += sprintf(buff + n, "\n");

	return n;
}

static CLASS_ATTR_RW(tuner_debug);
static CLASS_ATTR_RW(demod_debug);

static struct attribute *dvb_extern_class_attrs[] = {
	&class_attr_tuner_debug.attr,
	&class_attr_demod_debug.attr,
	NULL,
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
	struct tuner_ops *tops = NULL;
	struct demod_ops *dops = NULL;
	struct dvb_extern_device *dvbdev = NULL;

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

	if (class_register(&dvbdev->class)) {
		pr_err("class register fail.\n");
		goto fail_class_register;
	}

	/* PROPERTY_TUNER */
	ret = of_property_read_u32(pdev->dev.of_node, "tuner_num", &val);
	if (ret == 0) {
		dvbdev->tuner_num = val;
		pr_err("find dvb tuner numbers %d.\n", dvbdev->tuner_num);
	} else {
		pr_err("can't find tuner_num.\n");
		goto PROPERTY_DEMOD;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "tuner_cur", &val);
	if (ret) {
		pr_err("can't find tuner_cur, use default 0.\n");
		dvbdev->tuner_cur = 0;
	} else {
		dvbdev->tuner_cur = val;
	}

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

			pr_err("can't find tuner %d.\n", i);
		} else {
			tops->index = i;

			pr_err("find tuner %d, id %d, i2c_addr 0x%x.\n",
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
		pr_err("find dvb fe numbers %d.\n", dvbdev->demod_num);
	} else {
		pr_err("can't find fe_num.\n");
		goto PROPERTY_DONE;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "fe_cur", &val);
	if (ret) {
		pr_err("can't find fe current use, use default 0.\n");
		dvbdev->demod_cur = 0;
	} else {
		dvbdev->demod_cur = val;
	}

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

			pr_err("can't find demod %d.\n", i);

			continue;
		}

		dops->index = i;

		pr_err("find demod %d, id %d, i2c_addr 0x%x.\n",
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
		ret = of_property_read_u32(pdev->dev.of_node,
				"fe0_tuner0", &val);
		if (!ret) {
			tops = dvb_tuner_ops_get_byindex(val);
			if (tops)
				memcpy(&dops->cfg.tuner0, &tops->cfg,
						sizeof(struct tuner_config));

			pr_err("get fe0_tuner0 %d.\n", val);
		}

		/* S/S2 tuner */
		ret = of_property_read_u32(pdev->dev.of_node,
				"fe0_tuner1", &val);
		if (!ret) {
			tops = dvb_tuner_ops_get_byindex(val);
			if (tops)
				memcpy(&dops->cfg.tuner1, &tops->cfg,
						sizeof(struct tuner_config));

			pr_err("get fe0_tuner1 %d.\n", val);
		}
	}

PROPERTY_DONE:
	dvb_extern_dev = dvbdev;

	pr_info("%s: OK.\n", __func__);

	return 0;

fail_tuner_create:
fail_demod_create:
	class_unregister(&dvbdev->class);

fail_class_register:
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

	class_unregister(&dvbdev->class);

	kfree(dvbdev);
	dvbdev = NULL;
	dvb_extern_dev = NULL;

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void aml_dvb_extern_shutdown(struct platform_device *pdev)
{
}

static int aml_dvb_extern_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int aml_dvb_extern_resume(struct platform_device *pdev)
{
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
