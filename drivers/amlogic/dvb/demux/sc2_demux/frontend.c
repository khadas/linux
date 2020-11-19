// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include "../aml_dvb.h"
//#include "demod_gt.h"
#include <linux/amlogic/aml_tuner.h>
#include <linux/amlogic/aml_dtvdemod.h>

#include "sc2_control.h"
#include "../dmx_log.h"

#define TSIN_NUM		4

#define dprint_i(fmt, args...)  \
	dprintk(LOG_ERROR, debug_frontend, fmt, ## args)
#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_frontend, "fend:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_frontend, "fend:" fmt, ## args)

MODULE_PARM_DESC(debug_frontend, "\n\t\t Enable debug frontend information");
static int debug_frontend;
module_param(debug_frontend, int, 0644);

static struct dvb_frontend *frontend[FE_DEV_COUNT] = {
	NULL, NULL
};

static enum dtv_demod_type s_demod_type[FE_DEV_COUNT] = {
	AM_DTV_DEMOD_NONE,
	AM_DTV_DEMOD_NONE
};

static enum tuner_type s_tuner_type[FE_DEV_COUNT] = {
	AM_TUNER_NONE,
	AM_TUNER_NONE
};

ssize_t tuner_setting_show(struct class *class,
			   struct class_attribute *attr, char *buf)
{
	int r;

	struct aml_dvb *dvb = aml_get_dvb_device();

	if (dvb->tuner_cur >= 0)
		r = sprintf(buf, "dvb current attatch tuner %d, id: %d\n",
			    dvb->tuner_cur, dvb->tuners[dvb->tuner_cur].cfg.id);
	else
		r = sprintf(buf, "dvb has no attatch tuner.\n");

	return r;
}

ssize_t tuner_setting_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int n = 0, i = 0, val = 0;
	unsigned long tmp = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL;
	char *parm[4] = { NULL };
	struct aml_dvb *dvb = aml_get_dvb_device();
	int tuner_id = 0;
	struct aml_tuner *tuner = NULL;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (parm[0] && kstrtoul(parm[0], 10, &tmp) == 0) {
		val = tmp;

		for (i = 0; i < dvb->tuner_num; ++i) {
			if (dvb->tuners[i].cfg.id == val) {
				tuner_id = dvb->tuners[i].cfg.id;
				break;
			}
		}

		if (tuner_id == 0 || dvb->tuner_cur == i) {
			dprint("%s: set nonsupport or the same tuner %d.\n",
			       __func__, val);
			goto EXIT;
		}

		dvb->tuner_cur = i;

		for (i = 0; i < FE_DEV_COUNT; i++) {
			tuner = &dvb->tuners[dvb->tuner_cur];

			if (!frontend[i])
				continue;

			if (aml_attach_tuner
			    (tuner->cfg.id, frontend[i], &tuner->cfg) == NULL) {
				s_tuner_type[i] = AM_TUNER_NONE;
				dprint("tuner[%d] [type = %d] attach error.\n",
				       dvb->tuner_cur, tuner->cfg.id);
				goto EXIT;
			} else {
				s_tuner_type[i] = tuner->cfg.id;
				dprint
				    ("tuner[%d] [type = %d] attach success.\n",
				     dvb->tuner_cur, tuner->cfg.id);
			}
		}

		dprint("%s: attach tuner %d done.\n",
		       __func__, dvb->tuners[dvb->tuner_cur].cfg.id);
	}

EXIT:
	kfree(buf_orig);
	return count;
}

ssize_t ts_setting_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	int i;
	struct aml_dvb *dvb = aml_get_dvb_device();
	int ctrl;

	//print hardware TS input
	r = sprintf(buf, "tsin:\n");
	buf += r;
	total += r;

	for (i = 0; i < TSIN_NUM; i++) {
		struct aml_ts_input *ts = &dvb->ts[i];

		ctrl = ts->control;

		r = sprintf(buf, "tsin%d %s control: 0x%x\n", i,
			    ts->mode == AM_TS_DISABLE ? "disable" :
			    (ts->mode == AM_TS_SERIAL_3WIRE ?
			     "serial-3wire" :
			     (ts->mode == AM_TS_SERIAL_4WIRE ?
			      "serial-4wire" : "parallel")), ctrl);
		buf += r;
		total += r;
	}

	//stream ts
	r = sprintf(buf, "ts stream:\n");
	buf += r;
	total += r;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_ts_input *ts = &dvb->ts[i];

		r = sprintf(buf, "ts_sid:0x%0x header_len:%d ",
			    ts->ts_sid, ts->header_len);
		buf += r;
		total += r;

		r = sprintf(buf, "header[0]:0x%0x sid_offset:%d\n",
			    ts->header[0], ts->sid_offset);
		buf += r;
		total += r;
	}
	return total;
}

ssize_t ts_setting_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	int id, ctrl, r, mode;
	char mname[32];
	char pname[32];
	unsigned long flags;
	struct aml_ts_input *ts;
	struct aml_dvb *dvb = aml_get_dvb_device();

	r = sscanf(buf, "%d %s %x", &id, mname, &ctrl);
	if (r != 3)
		return -EINVAL;

	if (id < 0 || id >= TSIN_NUM)
		return -EINVAL;

	if ((mname[0] == 's') || (mname[0] == 'S')) {
		sprintf(pname, "s_ts%d", id);
		if (mname[1] == '3')
			mode = AM_TS_SERIAL_3WIRE;
		else
			mode = AM_TS_SERIAL_4WIRE;
	} else if ((mname[0] == 'p') || (mname[0] == 'P')) {
		sprintf(pname, "p_ts%d", id);
		mode = AM_TS_PARALLEL;
	} else {
		mode = AM_TS_DISABLE;
	}
	spin_lock_irqsave(&dvb->slock, flags);

	ts = &dvb->ts[id];

	if (mode != AM_TS_SERIAL_3WIRE && mode != AM_TS_SERIAL_4WIRE) {
		if (ts->pinctrl) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		}

		ts->pinctrl = devm_pinctrl_get_select(&dvb->pdev->dev, pname);
		ts->mode = mode;
		ts->control = ctrl;
	} else if (mode != AM_TS_DISABLE) {
		ts->mode = mode;
		ts->control = ctrl;

		dprint_i("id:%d, control:0x%0x\n", id, ctrl);
		demod_config_tsin_invert(id, ctrl);
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return count;
}

static void set_dvb_ts(struct platform_device *pdev,
		       struct aml_dvb *advb, int i, const char *str)
{
	char buf[32];
	u8 control = 0;

	if (i >= TSIN_NUM) {
		dprint("set tsin %d invalid\n", i);
		return;
	}
	if (!strcmp(str, "serial-3wire")) {
		dprint("ts%d:%s\n", i, str);

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "s_ts%d", i);
		advb->ts[i].mode = AM_TS_SERIAL_3WIRE;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		control = advb->ts[i].control;
		demod_config_in(i, DEMOD_3WIRE);
		demod_config_tsin_invert(i, control);
	} else if (!strcmp(str, "serial-4wire")) {
		dprint("ts%d:%s\n", i, str);

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "s_ts%d", i);
		advb->ts[i].mode = AM_TS_SERIAL_4WIRE;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		control = advb->ts[i].control;
		demod_config_in(i, DEMOD_4WIRE);
		demod_config_tsin_invert(i, control);
	} else if (!strcmp(str, "parallel")) {
		dprint("ts%d:%s\n", i, str);

		if (i != 1) {
			dprint("error %s:parallel should be ts1\n", buf);
			return;
		}
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "p_ts%d", i);
		advb->ts[i].mode = AM_TS_PARALLEL;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		demod_config_in(i, DEMOD_PARALLEL);
	} else {
		advb->ts[i].mode = AM_TS_DISABLE;
		advb->ts[i].pinctrl = NULL;
	}
}

static void ts_process(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0, j = 0;
	char buf[32];
	const char *str;
	u32 value;
	u32 data[32] = { 0 };
	struct aml_dvb *advb = aml_get_dvb_device();

	for (i = 0; i < FE_DEV_COUNT; i++) {
		advb->ts[i].mode = AM_TS_DISABLE;
		advb->ts[i].ts_sid = -1;
		advb->ts[i].pinctrl = NULL;
		advb->ts[i].header_len = 0;

		memset(&advb->ts[i].header, 0, sizeof(advb->ts[i].header));

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_sid", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].ts_sid = value;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_header_len", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].header_len = value;

		if (advb->ts[i].header_len) {
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "ts%d_header", i);
			ret = of_property_read_u32_array(pdev->dev.of_node,
							 buf, data,
							 advb->ts[i].header_len);
			if (!ret) {
				for (j = 0; j < advb->ts[i].header_len; ++j)
					advb->ts[i].header[j] = data[j];
			}
		}

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_sid_offset", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret) {
			advb->ts[i].sid_offset = value;
			advb->ts[i].ts_sid = advb->ts[i].header[value];
		}

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_control", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret) {
			dprint("%s: 0x%x\n", buf, value);
			advb->ts[i].control = value;
		} else {
			dprint("read error:%s: 0x%x\n", buf, value);
		}

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d", i);
		ret = of_property_read_string(pdev->dev.of_node, buf, &str);
		if (!ret)
			set_dvb_ts(pdev, advb, i, str);
	}
}

int frontend_probe(struct platform_device *pdev)
{
	struct demod_config config;
	struct tuner_config *tuner_cfg = NULL;
	char buf[32] = { 0 };
	const char *str = NULL;
	struct device_node *node_tuner = NULL;
//	struct tuner_config cfg;
	u32 value = 0;
	int i = 0, j = 0, len = 0;
	int ret = 0;
	struct aml_dvb *advb = aml_get_dvb_device();

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		ts_process(pdev);
#endif

	for (i = 0; i < FE_DEV_COUNT; i++) {
		memset(&config, 0, sizeof(struct demod_config));

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "fe%d_mode", i);
		ret = of_property_read_string(pdev->dev.of_node, buf, &str);
		if (ret)
			continue;

		if (!strcmp(str, "internal")) {
			config.mode = 0;
			config.id = AM_DTV_DEMOD_AMLDTV;
			frontend[i] = aml_attach_dtvdemod(config.id, &config);
			if (!frontend[i]) {
				s_demod_type[i] = AM_DTV_DEMOD_NONE;
				dprint
				    ("internal demod[type=%d] attach error.\n",
				     config.id);
				goto error_fe;
			} else {
				s_demod_type[i] = config.id;
				dprint("internal demod[type=%d] attach ok\n",
				       config.id);
			}

//			memset(&cfg, 0, sizeof(struct tuner_config));
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_tuner", i);
			node_tuner = of_parse_phandle(pdev->dev.of_node,
						      buf, 0);
			if (!node_tuner) {
				pr_err("can't find tuner.\n");
				goto error_fe;
			}
			ret = of_property_read_u32(node_tuner,
						   "tuner_num", &value);
			if (ret) {
				pr_err("can't find tuner_num.\n");
				goto error_fe;
			} else {
				advb->tuner_num = value;
			}

			advb->tuners = kcalloc(advb->tuner_num,
					       sizeof(struct aml_tuner),
					       GFP_KERNEL);
			if (!advb->tuners)
				goto error_fe;

			ret = of_property_read_u32(node_tuner,
						   "tuner_cur", &value);
			if (ret) {
				pr_err
				    ("can't find tuner_cur, use default 0.\n");
				advb->tuner_cur = -1;
			} else {
				advb->tuner_cur = value;
			}

			for (j = 0; j < advb->tuner_num; ++j) {
				ret = aml_get_dts_tuner_config(node_tuner,
							       &advb->tuners[j].cfg, j);
				if (ret) {
					pr_err("can't find tuner.\n");
					goto error_fe;
				}
			}

			of_node_put(node_tuner);

			/* define general-purpose callback pointer */
			frontend[i]->callback = NULL;

			if (advb->tuner_cur >= 0) {
				tuner_cfg = &advb->tuners[advb->tuner_cur].cfg;
				if (aml_attach_tuner
				    (tuner_cfg->id, frontend[i],
				     tuner_cfg) == NULL) {
					s_tuner_type[i] = AM_TUNER_NONE;
					dprint
					    ("tuner [type=%d] attach error.\n",
					     tuner_cfg->id);
					goto error_fe;
				} else {
					s_tuner_type[i] = tuner_cfg->id;
					dprint
					    ("tuner[type=%d]attach success.\n",
					     tuner_cfg->id);
				}
			}

			ret = dvb_register_frontend(&advb->dvb_adapter,
						    frontend[i]);
			if (ret) {
				dprint("register dvb frontend failed\n");
				goto error_fe;
			}
		} else if (!strcmp(str, "external")) {
			config.mode = 1;
			config.id = AM_DTV_DEMOD_NONE;
			ret =
			    aml_get_dts_demod_config(pdev->dev.of_node, &config,
						     i);
			if (ret) {
				pr_err("can't find demod %d.\n", i);
				continue;
			}

			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_tuner", i);
			node_tuner =
			    of_parse_phandle(pdev->dev.of_node, buf, 0);
			if (node_tuner) {
				aml_get_dts_tuner_config(node_tuner,
							 &config.tuner0, 0);
				aml_get_dts_tuner_config(node_tuner,
							 &config.tuner1, 1);
			} else {
				pr_err("can't find %s.\n", buf);
			}
			of_node_put(node_tuner);

			if (advb->ts[config.ts].mode == AM_TS_PARALLEL) {
				config.ts_out_mode = 1;	/* parallel */
			} else {
				config.ts_out_mode = 0;	/* serial */
				if (advb->ts[i].mode == AM_TS_SERIAL_3WIRE) {
					/* Four-wire */
					config.ts_wire_mode = 1;
				} else {
					/* Three-wire */
					config.ts_wire_mode = 0;
				}
			}

			len = advb->ts[i].header_len;
			if (len) {
				config.ts_header_bytes = len;
				for (j = 0; j < len; ++j)
					config.ts_header_data[j] =
					    advb->ts[i].header[j] & 0xFF;
			}

			/* TODO: config.ts_mux_mode, default 0(NO_MUX_4) */
			/* serial ts multiplexer mode,
			 * ts_mux_mode:
			 * 0: NO_MUX_4, 1: MUX_1, 2: MUX_2, 3: MUX_3,
			 * 4: MUX_4, 5: MUX_2_B, 6: NO_MUX_2.
			 * 212: NO_MUX_4.
			 * 213: NO_MUX_4.
			 * 214: NO_MUX_4.
			 * 252: NO_MUX_2, PARALLEL.
			 * 254: MUX_1, MUX_2, NO_MUX_4, PARALLEL.
			 * 256: MUX_2, MUX_2B, MUX_3, MUX_4, NO_MUX_4, PARALLEL.
			 * 258: MUX_4, MUX_2, NO_MUX_4, PARALLEL.
			 */

			frontend[i] = aml_attach_dtvdemod(config.id, &config);
			if (!frontend[i]) {
				s_demod_type[i] = AM_DTV_DEMOD_NONE;
				dprint
				    ("external demod [type=%d] attach error.\n",
				     config.id);
				goto error_fe;
			} else {
				s_demod_type[i] = config.id;
				dprint
				    ("demod [type=%d] attach success\n",
				     config.id);
			}

			if (frontend[i]) {
				ret =
				    dvb_register_frontend(&advb->dvb_adapter,
							  frontend[i]);
				if (ret) {
					dprint
					    ("register dvb frontend failed\n");
					goto error_fe;
				}
			}
		}
	}

	return 0;

error_fe:
	for (i = 0; i < FE_DEV_COUNT; i++) {
		aml_detach_dtvdemod(s_demod_type[i]);
		frontend[i] = NULL;
		s_demod_type[i] = AM_DTV_DEMOD_NONE;

		aml_detach_tuner(s_tuner_type[i]);
		s_tuner_type[i] = AM_TUNER_NONE;
	}

	kfree(advb->tuners);

	return 0;
}

void frontend_config_ts_sid(void)
{
	int i;
	int sid = 0;
	struct aml_dvb *advb = aml_get_dvb_device();

	for (i = 0; i < TSIN_NUM; i++) {
		if (advb->ts[i].mode == AM_TS_DISABLE)
			continue;
		if (advb->ts[i].header_len == 0) {
			if (advb->ts[i].ts_sid != -1) {
				sid = advb->ts[i].ts_sid;
				demod_config_single(i, sid);
			}
		} else {
			demod_config_multi(i, advb->ts[i].header_len / 4,
					   advb->ts[i].header[0],
					   advb->ts[i].sid_offset);
		}
	}
}

int frontend_remove(void)
{
	int i;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		aml_detach_dtvdemod(s_demod_type[i]);

		aml_detach_tuner(s_tuner_type[i]);

		if (frontend[i] && (s_tuner_type[i] == AM_TUNER_SI2151 ||
				    s_tuner_type[i] == AM_TUNER_MXL661 ||
				    s_tuner_type[i] == AM_TUNER_SI2159 ||
				    s_tuner_type[i] == AM_TUNER_R842 ||
				    s_tuner_type[i] == AM_TUNER_R840 ||
				    s_tuner_type[i] == AM_TUNER_ATBM2040)) {
			dvb_unregister_frontend(frontend[i]);
			dvb_frontend_detach(frontend[i]);
		}

		frontend[i] = NULL;
		s_demod_type[i] = AM_DTV_DEMOD_NONE;
		s_tuner_type[i] = AM_TUNER_NONE;
	}
	return 0;
}
