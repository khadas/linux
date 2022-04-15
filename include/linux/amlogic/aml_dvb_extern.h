/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DVB_EXTERN_H__
#define __AML_DVB_EXTERN_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/amlogic/aml_demod_common.h>
#include <linux/amlogic/aml_tuner.h>
#include <dvb_frontend.h>

struct tuner_ops {
	bool dts_cfg; /*to check if tuner_config is from userspace or dts*/
	bool attached;
	bool valid; /* There is hardware exist. */
	bool pre_inited;

	int refcount;
	int index;
	int delivery_system; /* The tuner's current delivery system. */
	int type; /* The tuner's current FE type. */

	const struct tuner_module *module;
	struct dvb_frontend fe; /* Used to attach tuner. */
	struct dvb_frontend *user; /* The current tuner user. */
	struct tuner_config cfg;
	struct list_head list;
};

struct dvb_tuner {
	struct tuner_ops *used;
	struct list_head list;
	struct mutex mutex; /* tuner ops mutex */
	int refcount;

	int (*attach)(struct dvb_tuner *tuner, bool attach);
	struct tuner_ops *(*match)(struct dvb_tuner *tuner, int std);
	int (*detect)(struct dvb_tuner *tuner);
	int (*pre_init)(struct dvb_tuner *tuner);
};

struct demod_ops {
	bool attached;
	bool registered;
	bool valid; /* There is hardware. */
	bool pre_inited;
	bool external; /* external demod. */

	int refcount;
	int index;
	void *ops;
	int delivery_system; /* The demod's current delivery system. */
	int type; /* The demod's current FE type. */

	const struct demod_module *module;
	struct dvb_frontend *fe; /* The return value of the attach to init. */
	struct demod_config cfg;
	struct list_head list;
};

struct dvb_demod {
	struct dvb_adapter *dvb_adapter;
	struct demod_ops *used;
	struct list_head list;
	struct mutex mutex;  /* demod ops mutex */
	int refcount;

	int (*attach)(struct dvb_demod *demod, bool attach);
	struct demod_ops *(*match)(struct dvb_demod *demod, int std);
	int (*detect)(struct dvb_demod *demod);
	int (*register_frontend)(struct dvb_demod *demod, bool regist);
	int (*pre_init)(struct dvb_demod *demod);
};

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
struct dvb_frontend *dvb_tuner_attach(struct dvb_frontend *fe);
int dvb_tuner_detach(void);

struct tuner_ops *dvb_tuner_ops_create(void);
void dvb_tuner_ops_destroy(struct tuner_ops *ops);
void dvb_tuner_ops_destroy_all(void);
int dvb_tuner_ops_add(struct tuner_ops *ops);
void dvb_tuner_ops_remove(struct tuner_ops *ops);
struct tuner_ops *dvb_tuner_ops_get_byindex(int index);
int dvb_tuner_ops_get_index(void);

struct dvb_tuner *get_dvb_tuners(void);

int dvb_extern_register_frontend(struct dvb_adapter *adapter);
int dvb_extern_unregister_frontend(void);

struct demod_ops *dvb_demod_ops_create(void);
void dvb_demod_ops_destroy(struct demod_ops *ops);
void dvb_demod_ops_destroy_all(void);
int dvb_demod_ops_add(struct demod_ops *ops);
void dvb_demod_ops_remove(struct demod_ops *ops);
struct demod_ops *dvb_demod_ops_get_byindex(int index);

struct dvb_demod *get_dvb_demods(void);
#else
static inline __maybe_unused struct dvb_frontend *dvb_tuner_attach(
		struct dvb_frontend *fe)
{
	return NULL;
}

static inline __maybe_unused int dvb_tuner_detach(void)
{
	return -ENODEV;
}

static inline __maybe_unused int dvb_extern_register_frontend(
		struct dvb_adapter *adapter)
{
	return -ENODEV;
}

static inline __maybe_unused int dvb_extern_unregister_frontend(void)
{
	return -ENODEV;
}

static inline __maybe_unused struct tuner_ops *dvb_tuner_ops_create(void)
{
	return NULL;
}

static inline __maybe_unused void dvb_tuner_ops_destroy(struct tuner_ops *ops)
{
}

static inline __maybe_unused void dvb_tuner_ops_destroy_all(void)
{
}

static inline __maybe_unused int dvb_tuner_ops_add(struct tuner_ops *ops)
{
	return -ENODEV;
}

static inline __maybe_unused void dvb_tuner_ops_remove(struct tuner_ops *ops)
{
}

static inline __maybe_unused struct tuner_ops *dvb_tuner_ops_get_byindex(
		int index)
{
	return NULL;
}

static inline __maybe_unused int dvb_tuner_ops_get_index(void)
{
	return -ENODEV;
}

static inline __maybe_unused struct demod_ops *dvb_demod_ops_create(void)
{
	return NULL;
}

static inline __maybe_unused void dvb_demod_ops_destroy(struct demod_ops *ops)
{
}

static inline __maybe_unused void dvb_demod_ops_destroy_all(void)
{
}

static inline __maybe_unused int dvb_demod_ops_add(struct demod_ops *ops)
{
	return -ENODEV;
}

static inline __maybe_unused void dvb_demod_ops_remove(struct demod_ops *ops)
{
}

static inline __maybe_unused struct demod_ops *dvb_demod_ops_get_byindex(
		int index)
{
	return NULL;
}
#endif /* CONFIG_AMLOGIC_DVB_EXTERN */

#endif /* __AML_DVB_EXTERN_H__ */
