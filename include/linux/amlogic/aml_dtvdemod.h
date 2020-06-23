/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DTVDEMOD_H__
#define __AML_DTVDEMOD_H__

#include <linux/amlogic/aml_demod_common.h>
#include <dvb_frontend.h>

/* For demod ctrl */
struct demod_priv {
	bool inited;
	bool suspended;
	bool serial_mode;

	struct mutex mutex;

	struct dvb_frontend fe;
	struct demod_config cfg;
	enum fe_delivery_system delivery_system;

	void *private;

	int index;
};

/* For demod */
struct demod_frontend {
	struct platform_device *pdev;
	struct class class;
	struct mutex mutex;

	struct list_head demod_list;
	int demod_count;
};

#if (defined CONFIG_AMLOGIC_DTV_DEMOD ||\
		defined CONFIG_AMLOGIC_DTV_DEMOD_MODULE)
struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *cfg);
#else
static inline __maybe_unused struct dvb_frontend *aml_dtvdm_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
#endif

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
enum dtv_demod_type aml_get_dtvdemod_type(const char *name);

struct dvb_frontend *aml_attach_detach_dtvdemod(
		enum dtv_demod_type type,
		struct demod_config *cfg,
		int attch);
#else
static inline __maybe_unused enum dtv_demod_type aml_get_dtvdemod_type(
		const char *name)
{
	return AM_DTV_DEMOD_NONE;
}

static inline __maybe_unused struct dvb_frontend *aml_attach_detach_dtvdemod(
		enum dtv_demod_type type,
		struct demod_config *cfg,
		int attch)
{
	return NULL;
}
#endif

static inline __maybe_unused struct dvb_frontend *aml_attach_dtvdemod(
		enum dtv_demod_type type,
		struct demod_config *cfg)
{
	return aml_attach_detach_dtvdemod(type, cfg, 1);
}

static inline __maybe_unused int aml_detach_dtvdemod(
		const enum dtv_demod_type type)
{
	aml_attach_detach_dtvdemod(type, NULL, 0);

	return 0;
}

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
int aml_get_dts_demod_config(struct device_node *node,
		struct demod_config *cfg, int index);
void aml_show_demod_config(const char *title, struct demod_config *cfg);
#else
static inline __maybe_unused int aml_get_dts_demod_config(
		struct device_node *node, struct demod_config *cfg, int index)
{
	return 0;
}

static inline __maybe_unused void aml_show_demod_config(const char *title,
		struct demod_config *cfg)
{
}
#endif

/* For attach demod driver end*/
#endif /* __AML_DTVDEMOD_H__ */
