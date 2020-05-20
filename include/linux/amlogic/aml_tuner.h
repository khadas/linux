/*
 * include/linux/amlogic/aml_tuner.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __AML_TUNER_H__
#define __AML_TUNER_H__

#include <linux/amlogic/aml_demod_common.h>

static inline struct dvb_frontend *si2176_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2196_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *fq1216_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *htm_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *ctc703_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2177_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *r840_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2157_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2151_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl661_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl608_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2159_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *r842_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *atbm2040_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *atbm253_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2124_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *av2011_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *av2012_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *av2018_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl603_attach(struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return NULL;
}

static __maybe_unused enum tuner_type aml_get_tuner_type(const char *name)
{
	enum tuner_type type = AM_TUNER_NONE;

	if (name == NULL)
		return type;

	if (!strncasecmp(name, "SI2176", strlen("SI2176")))
		type = AM_TUNER_SI2176;
	else if (!strncasecmp(name, "SI2196", strlen("SI2196")))
		type = AM_TUNER_SI2196;
	else if (!strncasecmp(name, "FQ1216", strlen("FQ1216")))
		type = AM_TUNER_FQ1216;
	else if (!strncasecmp(name, "HTM", strlen("HTM")))
		type = AM_TUNER_HTM;
	else if (!strncasecmp(name, "CTC703", strlen("CTC703")))
		type = AM_TUNER_CTC703;
	else if (!strncasecmp(name, "SI2177", strlen("SI2177")))
		type = AM_TUNER_SI2177;
	else if (!strncasecmp(name, "R840", strlen("R840")))
		type = AM_TUNER_R840;
	else if (!strncasecmp(name, "SI2157", strlen("SI2157")))
		type = AM_TUNER_SI2157;
	else if (!strncasecmp(name, "SI2151", strlen("SI2151")))
		type = AM_TUNER_SI2151;
	else if (!strncasecmp(name, "MXL661", strlen("MXL661")))
		type = AM_TUNER_MXL661;
	else if (!strncasecmp(name, "MXL608", strlen("MXL608")))
		type = AM_TUNER_MXL608;
	else if (!strncasecmp(name, "SI2159", strlen("SI2159")))
		type = AM_TUNER_SI2159;
	else if (!strncasecmp(name, "R842", strlen("R842")))
		type = AM_TUNER_R842;
	else if (!strncasecmp(name, "ATBM2040", strlen("ATBM2040")))
		type = AM_TUNER_ATBM2040;
	else if (!strncasecmp(name, "ATBM253", strlen("ATBM253")))
		type = AM_TUNER_ATBM253;
	else if (!strncasecmp(name, "SI2124", strlen("SI2124")))
		type = AM_TUNER_SI2124;
	else if (!strncasecmp(name, "AV2011", strlen("AV2011")))
		type = AM_TUNER_AV2011;
	else if (!strncasecmp(name, "AV2012", strlen("AV2012")))
		type = AM_TUNER_AV2012;
	else if (!strncasecmp(name, "AV2018", strlen("AV2018")))
		type = AM_TUNER_AV2018;
	else if (!strncasecmp(name, "MXL603", strlen("MXL603")))
		type = AM_TUNER_MXL603;
	else
		type = AM_TUNER_NONE;

	return type;
}

static __maybe_unused struct dvb_frontend *aml_attach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	struct dvb_frontend *p = NULL;

	if (type == AM_TUNER_SI2176)
		p = aml_dvb_attach(si2176_attach, fe, cfg);
	else if (type == AM_TUNER_SI2196)
		p = aml_dvb_attach(si2196_attach, fe, cfg);
	else if (type == AM_TUNER_FQ1216)
		p = aml_dvb_attach(fq1216_attach, fe, cfg);
	else if (type == AM_TUNER_HTM)
		p = aml_dvb_attach(htm_attach, fe, cfg);
	else if (type == AM_TUNER_CTC703)
		p = aml_dvb_attach(ctc703_attach, fe, cfg);
	else if (type == AM_TUNER_SI2177)
		p = aml_dvb_attach(si2177_attach, fe, cfg);
	else if (type == AM_TUNER_R840)
		p = aml_dvb_attach(r840_attach, fe, cfg);
	else if (type == AM_TUNER_SI2157)
		p = aml_dvb_attach(si2157_attach, fe, cfg);
	else if (type == AM_TUNER_SI2151)
		p = aml_dvb_attach(si2151_attach, fe, cfg);
	else if (type == AM_TUNER_MXL661)
		p = aml_dvb_attach(mxl661_attach, fe, cfg);
	else if (type == AM_TUNER_MXL608)
		p = aml_dvb_attach(mxl608_attach, fe, cfg);
	else if (type == AM_TUNER_SI2159)
		p = aml_dvb_attach(si2159_attach, fe, cfg);
	else if (type == AM_TUNER_R842)
		p = aml_dvb_attach(r842_attach, fe, cfg);
	else if (type == AM_TUNER_ATBM2040)
		p = aml_dvb_attach(atbm2040_attach, fe, cfg);
	else if (type == AM_TUNER_ATBM253)
		p = aml_dvb_attach(atbm253_attach, fe, cfg);
	else if (type == AM_TUNER_SI2124)
		p = aml_dvb_attach(si2124_attach, fe, cfg);
	else if (type == AM_TUNER_AV2011)
		p = aml_dvb_attach(av2011_attach, fe, cfg);
	else if (type == AM_TUNER_AV2012)
		p = aml_dvb_attach(av2012_attach, fe, cfg);
	else if (type == AM_TUNER_AV2018)
		p = aml_dvb_attach(av2018_attach, fe, cfg);
	else if (type == AM_TUNER_MXL603)
		p = aml_dvb_attach(mxl603_attach, fe, cfg);
	else
		p = NULL;

	return p;
}

static __maybe_unused int aml_detach_tuner(const enum tuner_type type)
{
	int ret = 0;

	if (type == AM_TUNER_SI2176)
		aml_dvb_detach(si2176_attach);
	else if (type == AM_TUNER_SI2196)
		aml_dvb_detach(si2196_attach);
	else if (type == AM_TUNER_FQ1216)
		aml_dvb_detach(fq1216_attach);
	else if (type == AM_TUNER_HTM)
		aml_dvb_detach(htm_attach);
	else if (type == AM_TUNER_CTC703)
		aml_dvb_detach(ctc703_attach);
	else if (type == AM_TUNER_SI2177)
		aml_dvb_detach(si2177_attach);
	else if (type == AM_TUNER_R840)
		aml_dvb_detach(r840_attach);
	else if (type == AM_TUNER_SI2157)
		aml_dvb_detach(si2151_attach);
	else if (type == AM_TUNER_SI2151)
		aml_dvb_detach(mxl661_attach);
	else if (type == AM_TUNER_MXL661)
		aml_dvb_detach(mxl608_attach);
	else if (type == AM_TUNER_MXL608)
		aml_dvb_detach(si2159_attach);
	else if (type == AM_TUNER_SI2159)
		aml_dvb_detach(r842_attach);
	else if (type == AM_TUNER_R842)
		aml_dvb_detach(atbm2040_attach);
	else if (type == AM_TUNER_ATBM2040)
		aml_dvb_detach(atbm2040_attach);
	else if (type == AM_TUNER_ATBM253)
		aml_dvb_detach(atbm253_attach);
	else if (type == AM_TUNER_SI2124)
		aml_dvb_detach(si2124_attach);
	else if (type == AM_TUNER_AV2011)
		aml_dvb_detach(av2011_attach);
	else if (type == AM_TUNER_AV2012)
		aml_dvb_detach(av2012_attach);
	else if (type == AM_TUNER_AV2018)
		aml_dvb_detach(av2018_attach);
	else if (type == AM_TUNER_MXL603)
		aml_dvb_detach(mxl603_attach);
	else
		ret = -EINVAL;

	return ret;
}

static __maybe_unused int aml_get_dts_tuner_config(struct device_node *node,
		struct tuner_config *cfg, int index)
{
	int ret = 0;
	u32 value = 0;
	struct device_node *node_i2c = NULL;
	char buf[32] = { 0 };
	const char *str = NULL;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_name_%d", index);
	ret = of_property_read_string(node, buf, &str);
	if (!ret) {
		cfg->id = aml_get_tuner_type(str);
		if (cfg->id == AM_TUNER_NONE)
			pr_err("Tuner: can't support tuner type: %s.\n", str);
	} else {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->id = AM_TUNER_NONE;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_i2c_adap_%d", index);
	node_i2c = of_parse_phandle(node, buf, 0);
	if (node_i2c != NULL) {
		cfg->i2c_adap = of_find_i2c_adapter_by_node(node_i2c);
		of_node_put(node_i2c);
		if (cfg->i2c_adap == NULL) {
			pr_err("Tuner: can't get i2c_get_adapter ret[NULL].\n");

			return -1;
		}
	} else {
		pr_err("Tuner: can't get %s ret[NULL].\n", buf);

		cfg->i2c_adap = NULL;

		return -1;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_i2c_addr_%d", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret)
		cfg->i2c_addr = value;
	else {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->i2c_addr = 0;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_code_%d", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret)
		cfg->code = value;
	else {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->code = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_xtal_%d", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->xtal = 0;
		ret = 0;
	} else
		cfg->xtal = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_xtal_mode_%d", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->xtal_mode = 0;
		ret = 0;
	} else
		cfg->xtal_mode = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_xtal_cap_%d", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->xtal_cap = 0;
		ret = 0;
	} else
		cfg->xtal_cap = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner_lt_out_%d", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->lt_out = 0;
		ret = 0;
	} else
		cfg->lt_out = value;

	return ret;
}

#endif /* __AML_TUNER_H__ */
