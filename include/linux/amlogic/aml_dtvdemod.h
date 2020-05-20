/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DTVDEMOD_H__
#define __AML_DTVDEMOD_H__

#include <linux/amlogic/aml_demod_common.h>

#if (defined CONFIG_AMLOGIC_DTV_DEMOD)
extern struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *cfg);
#else
static inline struct dvb_frontend *aml_dtvdm_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
#endif

static inline struct dvb_frontend *m1_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl101_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *avl6211_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *si2168_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *ite9133_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *ite9173_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *dib8096_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *atbm8869_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl241_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *avl68xx_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl683_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *atbm8881_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *atbm7821_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *avl6762_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *cxd2856_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
static inline struct dvb_frontend *mxl248_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static __maybe_unused enum dtv_demod_type aml_get_dtvdemod_type(
		const char *name)
{
	enum dtv_demod_type type = AM_DTV_DEMOD_NONE;

	if (name == NULL)
		return type;

	if (!strncasecmp(name, "internal", strlen("internal")))
		type = AM_DTV_DEMOD_AMLDTV;
	else if (!strncasecmp(name, "AMLDTV", strlen("AMLDTV")))
		type = AM_DTV_DEMOD_AMLDTV;
	else if (!strncasecmp(name, "M1", strlen("M1")))
		type = AM_DTV_DEMOD_M1;
	else if (!strncasecmp(name, "MXL101", strlen("MXL101")))
		type = AM_DTV_DEMOD_MXL101;
	else if (!strncasecmp(name, "AVL6211", strlen("AVL6211")))
		type = AM_DTV_DEMOD_AVL6211;
	else if (!strncasecmp(name, "SI2168", strlen("SI2168")))
		type = AM_DTV_DEMOD_SI2168;
	else if (!strncasecmp(name, "ITE9133", strlen("ITE9133")))
		type = AM_DTV_DEMOD_ITE9133;
	else if (!strncasecmp(name, "ITE9173", strlen("ITE9173")))
		type = AM_DTV_DEMOD_ITE9173;
	else if (!strncasecmp(name, "DIB8096", strlen("DIB8096")))
		type = AM_DTV_DEMOD_DIB8096;
	else if (!strncasecmp(name, "ATBM8869", strlen("ATBM8869")))
		type = AM_DTV_DEMOD_ATBM8869;
	else if (!strncasecmp(name, "MXL241", strlen("MXL241")))
		type = AM_DTV_DEMOD_MXL241;
	else if (!strncasecmp(name, "AVL68xx", strlen("AVL68xx")))
		type = AM_DTV_DEMOD_AVL68xx;
	else if (!strncasecmp(name, "MXL683", strlen("MXL683")))
		type = AM_DTV_DEMOD_MXL683;
	else if (!strncasecmp(name, "ATBM8881", strlen("ATBM8881")))
		type = AM_DTV_DEMOD_ATBM8881;
	else if (!strncasecmp(name, "ATBM7821", strlen("ATBM7821")))
		type = AM_DTV_DEMOD_ATBM7821;
	else if (!strncasecmp(name, "AVL6762", strlen("AVL6762")))
		type = AM_DTV_DEMOD_AVL6762;
	else if (!strncasecmp(name, "CXD2856", strlen("CXD2856")))
		type = AM_DTV_DEMOD_CXD2856;
	else if (!strncasecmp(name, "MXL248", strlen("MXL248")))
		type = AM_DTV_DEMOD_MXL248;
	else
		type = AM_DTV_DEMOD_NONE;

	return type;
}

static __maybe_unused struct dvb_frontend *aml_attach_dtvdemod(
		enum dtv_demod_type type,
		struct demod_config *cfg)
{
	struct dvb_frontend *p = 0;

	if (type == AM_DTV_DEMOD_AMLDTV)
		p = aml_dvb_attach(aml_dtvdm_attach, cfg);
	else if (type == AM_DTV_DEMOD_M1)
		p = aml_dvb_attach(m1_attach, cfg);
	else if (type == AM_DTV_DEMOD_MXL101)
		p = aml_dvb_attach(mxl101_attach, cfg);
	else if (type == AM_DTV_DEMOD_AVL6211)
		p = aml_dvb_attach(avl6211_attach, cfg);
	else if (type == AM_DTV_DEMOD_SI2168)
		p = aml_dvb_attach(si2168_attach, cfg);
	else if (type == AM_DTV_DEMOD_ITE9133)
		p = aml_dvb_attach(ite9133_attach, cfg);
	else if (type == AM_DTV_DEMOD_ITE9173)
		p = aml_dvb_attach(ite9173_attach, cfg);
	else if (type == AM_DTV_DEMOD_DIB8096)
		p = aml_dvb_attach(dib8096_attach, cfg);
	else if (type == AM_DTV_DEMOD_ATBM8869)
		p = aml_dvb_attach(atbm8869_attach, cfg);
	else if (type == AM_DTV_DEMOD_MXL241)
		p = aml_dvb_attach(mxl241_attach, cfg);
	else if (type == AM_DTV_DEMOD_AVL68xx)
		p = aml_dvb_attach(avl68xx_attach, cfg);
	else if (type == AM_DTV_DEMOD_MXL683)
		p = aml_dvb_attach(mxl683_attach, cfg);
	else if (type == AM_DTV_DEMOD_ATBM8881)
		p = aml_dvb_attach(atbm8881_attach, cfg);
	else if (type == AM_DTV_DEMOD_ATBM7821)
		p = aml_dvb_attach(atbm7821_attach, cfg);
	else if (type == AM_DTV_DEMOD_AVL6762)
		p = aml_dvb_attach(avl6762_attach, cfg);
	else if (type == AM_DTV_DEMOD_CXD2856)
		p = aml_dvb_attach(cxd2856_attach, cfg);
	else if (type == AM_DTV_DEMOD_MXL248)
		p = aml_dvb_attach(mxl248_attach, cfg);
	else
		p = NULL;

	return p;
}

static __maybe_unused int aml_detach_dtvdemod(const enum dtv_demod_type type)
{
	int ret = 0;

	if (type == AM_DTV_DEMOD_AMLDTV)
		aml_dvb_detach(aml_dtvdm_attach);
	else if (type == AM_DTV_DEMOD_M1)
		aml_dvb_detach(m1_attach);
	else if (type == AM_DTV_DEMOD_MXL101)
		aml_dvb_detach(mxl101_attach);
	else if (type == AM_DTV_DEMOD_AVL6211)
		aml_dvb_detach(avl6211_attach);
	else if (type == AM_DTV_DEMOD_SI2168)
		aml_dvb_detach(si2168_attach);
	else if (type == AM_DTV_DEMOD_ITE9133)
		aml_dvb_detach(ite9133_attach);
	else if (type == AM_DTV_DEMOD_ITE9173)
		aml_dvb_detach(ite9173_attach);
	else if (type == AM_DTV_DEMOD_DIB8096)
		aml_dvb_detach(dib8096_attach);
	else if (type == AM_DTV_DEMOD_ATBM8869)
		aml_dvb_detach(atbm8869_attach);
	else if (type == AM_DTV_DEMOD_MXL241)
		aml_dvb_detach(mxl241_attach);
	else if (type == AM_DTV_DEMOD_AVL68xx)
		aml_dvb_detach(avl68xx_attach);
	else if (type == AM_DTV_DEMOD_MXL683)
		aml_dvb_detach(mxl683_attach);
	else if (type == AM_DTV_DEMOD_ATBM8881)
		aml_dvb_detach(atbm8881_attach);
	else if (type == AM_DTV_DEMOD_ATBM7821)
		aml_dvb_detach(atbm7821_attach);
	else if (type == AM_DTV_DEMOD_AVL6762)
		aml_dvb_detach(avl6762_attach);
	else if (type == AM_DTV_DEMOD_CXD2856)
		aml_dvb_detach(cxd2856_attach);
	else if (type == AM_DTV_DEMOD_MXL248)
		aml_dvb_detach(mxl248_attach);
	else
		ret = -EINVAL;

	return ret;
}

static __maybe_unused int aml_get_dts_demod_config(struct device_node *node,
		struct demod_config *cfg, int index)
{
	int ret = 0;
	u32 value = 0;
	struct device_node *node_i2c = NULL;
	char buf[32] = { 0 };
	const char *str = NULL;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod", index);
	ret = of_property_read_string(node, buf, &str);
	if (!ret) {
		cfg->id = aml_get_dtvdemod_type(str);
		if (cfg->id == AM_DTV_DEMOD_NONE)
			pr_err("Demod: can't support demod type: %s.\n", str);
	} else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->id = AM_DTV_DEMOD_NONE;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_i2c_adap_id", index);
	node_i2c = of_parse_phandle(node, buf, 0);
	if (node_i2c != NULL) {
		cfg->i2c_adap = of_find_i2c_adapter_by_node(node_i2c);
		of_node_put(node_i2c);
		if (cfg->i2c_adap == NULL) {
			pr_err("Demod: can't get i2c_adapter ret[NULL].\n");

			return -1;
		}
	} else {
		pr_err("Demod: can't get %s ret[NULL].\n", buf);

		cfg->i2c_adap = NULL;

		return -1;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod_i2c_addr", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret)
		cfg->i2c_addr = value;
	else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->i2c_addr = 0;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret)
		cfg->ts = value;
	else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts = 0;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod_code", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret)
		cfg->code = value;
	else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->code = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "fe%d_reset_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reset.pin = -1;
		ret = 0;
	} else {
		cfg->reset.pin = of_get_named_gpio_flags(node, buf, 0, NULL);
		pr_err("Demod: get %s: %d.\n", buf, cfg->reset.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_reset_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reset.dir = 0;
		ret = 0;
	} else
		cfg->reset.dir = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_reset_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reset.value = 0;
		ret = 0;
	} else
		cfg->reset.value = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ant_power_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ant_power.pin = -1;
		ret = 0;
	} else {
		cfg->ant_power.pin = of_get_named_gpio_flags(
				node, buf, 0, NULL);
		pr_err("Demod: get %s: %d\n", buf, cfg->ant_power.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ant_power_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ant_power.dir = 0;
		ret = 0;
	} else
		cfg->ant_power.dir = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ant_poweron_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ant_power.value = 0;
		ret = 0;
	} else
		cfg->ant_power.value = value;

	return 0;
}

/* For attach demod driver end*/
#endif /* __AML_DTVDEMOD_H__ */
