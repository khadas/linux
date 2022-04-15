// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_tuner.h>
#include <linux/amlogic/aml_dvb_extern.h>

static int tuner_attach(struct dvb_tuner *tuner, bool attach)
{
	void *fe = NULL;
	struct tuner_ops *ops = NULL;
	bool user_cfg = false;

	if (IS_ERR_OR_NULL(tuner))
		return -EFAULT;

	mutex_lock(&tuner->mutex);

	list_for_each_entry(ops, &tuner->list, list) {
		if (ops->dts_cfg)
			continue;
		user_cfg = true;
		break;
	}

	ops = NULL;
	list_for_each_entry(ops, &tuner->list, list) {
		if ((ops->attached && attach) ||
			(!ops->attached && !attach)) {
			pr_err("Tuner: tuner%d [id %d] had %s.\n",
					ops->index, ops->cfg.id,
					attach ? "attached" : "detached");
			continue;
		}

		if (attach) {
			if (!user_cfg || (user_cfg && !ops->dts_cfg)) {
				fe = ops->module->attach(ops->module,
						&ops->fe, &ops->cfg);
				if (!IS_ERR_OR_NULL(fe))
					ops->attached = true;
				else
					ops->attached = false;

				pr_err("Tuner: attach tuner%d [id %d] %s.\n",
						ops->index, ops->cfg.id,
						ops->attached ? "done" : "fail");
			}
		} else {
			if (tuner->used == ops)
				tuner->used = NULL;

			ops->module->detach(ops->module);

			ops->attached = false;
			ops->delivery_system = SYS_UNDEFINED;
			ops->type = AML_FE_UNDEFINED;

			memset(&ops->fe, 0, sizeof(struct dvb_frontend));

			pr_err("Tuner: detach tuner%d [id %d] done.\n",
					ops->index, ops->cfg.id);
		}
	}
	mutex_unlock(&tuner->mutex);

	return 0;
}

static struct tuner_ops *tuner_match(struct dvb_tuner *tuner, int std)
{
	int ret = 0;
	struct tuner_ops *ops = NULL;

	if (IS_ERR_OR_NULL(tuner))
		return NULL;

	mutex_lock(&tuner->mutex);

	if (tuner->used) {
		if (tuner->used->type == std) {
			mutex_unlock(&tuner->mutex);

			return tuner->used;
		}

		ret = tuner->used->module->match(tuner->used->module, std);
		if (!ret) {
			tuner->used->type = std;

			mutex_unlock(&tuner->mutex);

			return tuner->used;
		}
	}

	tuner->used = NULL;
	list_for_each_entry(ops, &tuner->list, list) {
		if (!ops->attached || (ops->cfg.detect && !ops->valid))
			continue;

		ret = ops->module->match(ops->module, std);
		if (!ret) {
			ops->type = std;
			tuner->used = ops;

			break;
		}
	}

	mutex_unlock(&tuner->mutex);

	return tuner->used;
}

static int tuner_detect(struct dvb_tuner *tuner)
{
	int ret = 0;
	struct tuner_ops *ops = NULL;

	if (IS_ERR_OR_NULL(tuner))
		return -EFAULT;

	mutex_lock(&tuner->mutex);

	list_for_each_entry(ops, &tuner->list, list) {
		ret = 0;

		if (!ops->attached || !ops->cfg.detect || ops->valid)
			continue;

		if (ops->fe.ops.tuner_ops.set_config) {
			ret = ops->fe.ops.tuner_ops.set_config(&ops->fe, NULL);
		} else {
			pr_err("Tuner: tuner%d [id %d] set_config() is NULL.\n",
					ops->index, ops->cfg.id);

			continue;
		}

		if (!ret) {
			ret = ops->module->detect(&ops->cfg);
			if (!ret)
				ops->valid = true;
			else
				ops->valid = false;

			pr_err("Tuner: detect tuner%d [id %d] %s.\n",
					ops->index, ops->cfg.id,
					ops->valid ? "done" : "fail");

			if (ops->fe.ops.tuner_ops.release)
				ops->fe.ops.tuner_ops.release(&ops->fe);
		} else {
			pr_err("Tuner: tuner%d [id %d] set_config() error, ret %d.\n",
					ops->index, ops->cfg.id, ret);
		}
	}

	mutex_unlock(&tuner->mutex);

	return 0;
}

static int tuner_pre_init(struct dvb_tuner *tuner)
{
	int ret = 0;
	struct tuner_ops *ops = NULL;

	if (IS_ERR_OR_NULL(tuner))
		return -EFAULT;

	mutex_lock(&tuner->mutex);

	list_for_each_entry(ops, &tuner->list, list) {
		if (!ops->attached || ops->pre_inited)
			continue;

		/* In some cases, pre-init is required. */
		/* 1. Loop thorugh is enabled. */
		if (ops->cfg.lt_out && ops->fe.ops.tuner_ops.set_config) {
			ret = ops->fe.ops.tuner_ops.set_config(&ops->fe, NULL);

			ops->pre_inited = ret ? false : true;

			pr_err("Tuner: pre_init tuner%d [id %d] %s.\n",
					ops->index, ops->cfg.id,
					ret ? "fail" : "done");

			if (ops->pre_inited && ops->fe.ops.tuner_ops.release)
				ops->fe.ops.tuner_ops.release(&ops->fe);
		} else {
			ops->pre_inited = true;
		}
	}

	mutex_unlock(&tuner->mutex);

	return 0;
}

static DEFINE_MUTEX(dvb_tuners_mutex);

static struct dvb_tuner tuners = {
	.used = NULL,
	.list = LIST_HEAD_INIT(tuners.list),
	.mutex = __MUTEX_INITIALIZER(tuners.mutex),
	.refcount = 0,
	.attach = tuner_attach,
	.match = tuner_match,
	.detect = tuner_detect,
	.pre_init = tuner_pre_init
};

static DEFINE_MUTEX(tuner_fe_type_match_mutex);

static bool tuner_is_same_cfg(struct tuner_config *cfg1,
		struct tuner_config *cfg2)
{
	return (cfg1->id == cfg2->id &&
			cfg1->i2c_addr == cfg2->i2c_addr &&
			cfg1->i2c_adap == cfg2->i2c_adap);
}

static bool tuner_is_valid_cfg(struct tuner_config *cfg)
{
	return !cfg ? false : cfg->id != AM_TUNER_NONE;
}

static bool tuner_is_valid_ops(struct tuner_ops *ops)
{
	return !ops ? false :
			!(!ops->attached || (ops->cfg.detect && !ops->valid));
}

static struct tuner_ops *tuner_fe_type_match(struct dvb_frontend *fe)
{
	char *name = NULL;
	struct tuner_ops *match = NULL, *tops = NULL;
	struct demod_ops *dops = NULL, *find = NULL;
	struct dvb_tuner *tuner = get_dvb_tuners();
	struct dvb_demod *demod = get_dvb_demods();

	mutex_lock(&tuner_fe_type_match_mutex);

	/* First, lookup in the previous match. */
	if (tuner->used && tuner->used->user == fe &&
		tuner->used->type == fe->ops.info.type) {
		/* name = tuner->used->fe.ops.tuner_ops.info.name;
		 * pr_err("Tuner: return current match fe type [%d] tuner%d (%s).\n",
		 * fe->ops.info.type, ops->index, name ? name : "");
		 */

		mutex_unlock(&tuner_fe_type_match_mutex);

		return tuner->used;
	}

	/* Then, check if the specified tuner configuration is present. */
	list_for_each_entry(dops, &demod->list, list) {
		if (dops->fe == fe) {
			/* External demod doesn't need to match tuner. */
			if (dops->external) {
				mutex_unlock(&tuner_fe_type_match_mutex);

				return NULL;
			}

			find = dops;

			break;
		}
	}

	if (find) {
		if (tuner_is_valid_cfg(&find->cfg.tuner0) ||
			tuner_is_valid_cfg(&find->cfg.tuner1)) {
			list_for_each_entry(tops, &tuner->list, list) {
				if (tuner_is_same_cfg(&tops->cfg,
						&find->cfg.tuner0) ||
					tuner_is_same_cfg(&tops->cfg,
						&find->cfg.tuner1)) {
					if (!tuner_is_valid_ops(tops))
						continue;

					if (!tops->module->match(tops->module,
							fe->ops.info.type)) {
						match = tops;

						break;
					}
				}
			}
		}
	}

	/* Finally, lookup all the configurations, not include specified. */
	if (!match)
		match = tuner->match(tuner, fe->ops.info.type);

	if (!match) {
		pr_err("Tuner: can't get match fe type [%d] tuner.\n",
				fe->ops.info.type);

	} else {
		/* Get the actual tuner information and private data. */
		memcpy(&fe->ops.tuner_ops.info, &match->fe.ops.tuner_ops.info,
				sizeof(struct dvb_tuner_info));
		fe->tuner_priv = match->fe.tuner_priv;

		match->type = fe->ops.info.type;
		match->delivery_system = fe->dtv_property_cache.delivery_system;

		match->user = fe;
		tuner->used = match;

		name = match->fe.ops.tuner_ops.info.name;
		pr_err("Tuner: get match fe type [%d] tuner%d (%s).\n",
				fe->ops.info.type, match->index,
				name ? name : "");
	}

	mutex_unlock(&tuner_fe_type_match_mutex);

	return match;
}

/* In general, this interface must be called when it is no longer used. */
static void tuner_release(struct dvb_frontend *fe)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.release)
		match->fe.ops.tuner_ops.release(fe);
}

static int tuner_init(struct dvb_frontend *fe)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.init)
		return match->fe.ops.tuner_ops.init(fe);

	return 0;
}

static int tuner_sleep(struct dvb_frontend *fe)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.sleep)
		return match->fe.ops.tuner_ops.sleep(fe);

	return 0;
}

static int tuner_suspend(struct dvb_frontend *fe)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.suspend)
		return match->fe.ops.tuner_ops.suspend(fe);

	return 0;
}

static int tuner_resume(struct dvb_frontend *fe)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.resume)
		return match->fe.ops.tuner_ops.resume(fe);

	return 0;
}

/* This is the recomended way to set the tuner */
static int tuner_set_params(struct dvb_frontend *fe)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.set_params)
		return match->fe.ops.tuner_ops.set_params(fe);

	return 0;
}

static int tuner_set_analog_params(struct dvb_frontend *fe,
		struct analog_parameters *p)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.set_analog_params)
		return match->fe.ops.tuner_ops.set_analog_params(fe, p);

	return 0;
}

/* In general, this interface must be called first,
 * and the value of 'fe->ops.info.type' is valid and clear.
 */
static int tuner_set_config(struct dvb_frontend *fe, void *priv_cfg)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.set_config)
		return match->fe.ops.tuner_ops.set_config(fe, priv_cfg);

	return 0;
}

static int tuner_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_frequency)
		return match->fe.ops.tuner_ops.get_frequency(fe, frequency);

	return 0;
}

static int tuner_get_bandwidth(struct dvb_frontend *fe, u32 *frequency)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_bandwidth)
		return match->fe.ops.tuner_ops.get_bandwidth(fe, frequency);

	return 0;
}

static int tuner_get_if_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_if_frequency)
		return match->fe.ops.tuner_ops.get_if_frequency(fe, frequency);

	return 0;
}

static int tuner_get_status(struct dvb_frontend *fe, u32 *status)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_status)
		return match->fe.ops.tuner_ops.get_status(fe, status);

	return 0;
}

static int tuner_get_rf_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_rf_strength)
		return match->fe.ops.tuner_ops.get_rf_strength(fe, strength);

	return 0;
}

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
static int tuner_get_strength(struct dvb_frontend *fe, s16 *strength)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_strength)
		return match->fe.ops.tuner_ops.get_strength(fe, strength);

	return 0;
}
#endif

static int tuner_get_afc(struct dvb_frontend *fe, s32 *afc)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.get_afc)
		return match->fe.ops.tuner_ops.get_afc(fe, afc);

	return 0;
}

/*
 * This is support for demods like the mt352 - fills out the supplied
 * buffer with what to write.
 *
 * Don't use on newer drivers.
 */
static int tuner_calc_regs(struct dvb_frontend *fe, u8 *buf, int buf_len)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.calc_regs)
		return match->fe.ops.tuner_ops.calc_regs(fe, buf, buf_len);

	return 0;
}

/*
 * These are provided separately from set_params in order to
 * facilitate silicon tuners which require sophisticated tuning loops,
 * controlling each parameter separately.
 *
 * Don't use on newer drivers.
 */
static int tuner_set_frequency(struct dvb_frontend *fe, u32 frequency)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.set_frequency)
		return match->fe.ops.tuner_ops.set_frequency(fe, frequency);

	return 0;
}

static int tuner_set_bandwidth(struct dvb_frontend *fe, u32 bandwidth)
{
	struct tuner_ops *match = tuner_fe_type_match(fe);

	if (match && match->fe.ops.tuner_ops.set_bandwidth)
		return match->fe.ops.tuner_ops.set_bandwidth(fe, bandwidth);

	return 0;
}

/* Generic interface, which needs to be attached to the actual tuner.
 * The actual tuner information and private data must be obtained.
 */
static struct dvb_tuner_ops tuner_ops = {
	.info = {
		.name              = "tuner-useless",
		.frequency_min_hz  = 0,
		.frequency_max_hz  = 0,
		.frequency_step_hz = 0,
		.bandwidth_min     = 0,
		.bandwidth_max     = 0,
		.bandwidth_step    = 0,
	},
	.release = tuner_release,
	.init = tuner_init,
	.sleep = tuner_sleep,
	.suspend = tuner_suspend,
	.resume = tuner_resume,
	.set_params = tuner_set_params,
	.set_analog_params = tuner_set_analog_params,
	.calc_regs = tuner_calc_regs,
	.set_config = tuner_set_config,
	.get_frequency = tuner_get_frequency,
	.get_bandwidth = tuner_get_bandwidth,
	.get_if_frequency = tuner_get_if_frequency,
	.get_status = tuner_get_status,
	.get_rf_strength = tuner_get_rf_strength,
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	.get_strength = tuner_get_strength,
#endif
	.get_afc = tuner_get_afc,
	.set_frequency = tuner_set_frequency,
	.set_bandwidth = tuner_set_bandwidth
};

struct dvb_frontend *dvb_tuner_attach(struct dvb_frontend *fe)
{
	struct dvb_tuner *tuner = get_dvb_tuners();

	if (IS_ERR_OR_NULL(fe)) {
		pr_err("Tuner: %s: NULL or error pointer of fe.\n", __func__);

		return NULL;
	}

	mutex_lock(&dvb_tuners_mutex);

	/* try attach */
	tuner->attach(tuner, true);

	/* try detect */
	tuner->detect(tuner);

	tuner->pre_init(tuner);

	memcpy(&fe->ops.tuner_ops, &tuner_ops, sizeof(struct dvb_tuner_ops));

	/* try match */
	if (fe->ops.info.type != AML_FE_UNDEFINED)
		tuner_fe_type_match(fe);

	tuner->refcount++;

	mutex_unlock(&dvb_tuners_mutex);

	return fe;
}
EXPORT_SYMBOL(dvb_tuner_attach);

int dvb_tuner_detach(void)
{
	return 0;
}
EXPORT_SYMBOL(dvb_tuner_detach);

struct tuner_ops *dvb_tuner_ops_create(void)
{
	struct tuner_ops *ops = NULL;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (ops) {
		ops->attached = false;
		ops->dts_cfg = true;
		ops->index = -1;
		ops->pre_inited = false;
		ops->delivery_system = SYS_UNDEFINED;
		ops->type = AML_FE_UNDEFINED;
		ops->module = NULL;
		INIT_LIST_HEAD(&ops->list);
	}

	return ops;
}
EXPORT_SYMBOL(dvb_tuner_ops_create);

void dvb_tuner_ops_destroy(struct tuner_ops *ops)
{
	if (IS_ERR_OR_NULL(ops))
		return;

	dvb_tuner_ops_remove(ops);

	kfree(ops);
}
EXPORT_SYMBOL(dvb_tuner_ops_destroy);

void dvb_tuner_ops_destroy_all(void)
{
	struct dvb_tuner *tuner = NULL;
	struct tuner_ops *ops = NULL, *temp = NULL;

	mutex_lock(&dvb_tuners_mutex);

	tuner = get_dvb_tuners();

	list_for_each_entry_safe(ops, temp, &tuner->list, list) {
		dvb_tuner_ops_destroy(ops);
	}

	list_del_init(&tuner->list);

	mutex_unlock(&dvb_tuners_mutex);
}
EXPORT_SYMBOL(dvb_tuner_ops_destroy_all);

int dvb_tuner_ops_add(struct tuner_ops *ops)
{
	struct tuner_ops *p = NULL;
	struct dvb_tuner *tuner = NULL;

	if (IS_ERR_OR_NULL(ops))
		return -EFAULT;

	mutex_lock(&dvb_tuners_mutex);

	tuner = get_dvb_tuners();

	list_for_each_entry(p, &tuner->list, list) {
		if (p == ops) {
			mutex_unlock(&dvb_tuners_mutex);

			pr_err("Tuner: tuner%d ops [0x%p] exist.\n",
					ops->index, ops);

			return -EEXIST;
		}
	}

	ops->module = aml_get_tuner_module(ops->cfg.id);
	if (IS_ERR_OR_NULL(ops->module)) {
		mutex_unlock(&dvb_tuners_mutex);

		return -ENODEV;
	}

	ops->cfg.name = ops->module->name;

	list_add_tail(&ops->list, &tuner->list);

	mutex_unlock(&dvb_tuners_mutex);

	return 0;
}
EXPORT_SYMBOL(dvb_tuner_ops_add);

void dvb_tuner_ops_remove(struct tuner_ops *ops)
{
	if (IS_ERR_OR_NULL(ops))
		return;

	if (ops->attached) {
		if (ops->module && ops->module->detach)
			ops->module->detach(ops->module);

		ops->attached = false;
		ops->valid = false;
		ops->pre_inited = false;
		ops->index = -1;
		ops->delivery_system = SYS_UNDEFINED;
		ops->type = AML_FE_UNDEFINED;
		ops->module = NULL;
	}

	list_del_init(&ops->list);
}
EXPORT_SYMBOL(dvb_tuner_ops_remove);

struct tuner_ops *dvb_tuner_ops_get_byindex(int index)
{
	struct dvb_tuner *tuner = NULL;
	struct tuner_ops *ops = NULL;

	mutex_lock(&dvb_tuners_mutex);

	tuner = get_dvb_tuners();

	list_for_each_entry(ops, &tuner->list, list) {
		if (ops->index == index) {
			mutex_unlock(&dvb_tuners_mutex);

			return ops;
		}
	}

	mutex_unlock(&dvb_tuners_mutex);

	return NULL;
}
EXPORT_SYMBOL(dvb_tuner_ops_get_byindex);

int dvb_tuner_ops_get_index(void)
{
	struct dvb_tuner *tuner = NULL;
	struct tuner_ops *ops = NULL;
	int index = 0;

	mutex_lock(&dvb_tuners_mutex);

	tuner = get_dvb_tuners();

	list_for_each_entry(ops, &tuner->list, list) {
		index = ops->index;
	}
	mutex_unlock(&dvb_tuners_mutex);
	if (index != 0)
		index++;
	return index;
}
EXPORT_SYMBOL(dvb_tuner_ops_get_index);

struct dvb_tuner *get_dvb_tuners(void)
{
	return &tuners;
}
