/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_TUNER_H__
#define __AML_TUNER_H__

#include <linux/amlogic/aml_demod_common.h>
#include <dvb_frontend.h>

struct tuner_frontend {
	struct platform_device *pdev;
	struct class class;
	struct mutex mutex;

	struct tuner_config cfg;
	struct analog_parameters param;

	enum fe_type fe_type;

	void *private;
};

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
enum tuner_type aml_get_tuner_type(const char *name);

struct dvb_frontend *aml_attach_detach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg,
		int attach);
#else
static inline __maybe_unused enum tuner_type aml_get_tuner_type(
		const char *name)
{
	return AM_TUNER_NONE;
}

static inline __maybe_unused struct dvb_frontend *aml_attach_detach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg,
		int attach)
{
	return NULL;
}
#endif

static __maybe_unused struct dvb_frontend *aml_attach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return aml_attach_detach_tuner(type, fe, cfg, 1);
}

static __maybe_unused int aml_detach_tuner(const enum tuner_type type)
{
	aml_attach_detach_tuner(type, NULL, NULL, 0);

	return 0;
}

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
int aml_get_dts_tuner_config(struct device_node *node,
		struct tuner_config *cfg, int index);
#else
static inline __maybe_unused int aml_get_dts_tuner_config(
		struct device_node *node, struct tuner_config *cfg, int index)
{
	return 0;
}
#endif

#endif /* __AML_TUNER_H__ */
