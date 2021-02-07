// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_demod_common.h>
#include <linux/amlogic/aml_dtvdemod.h>
#include <linux/amlogic/aml_dvb_extern.h>

static int demod_attach(struct dvb_demod *demod, bool attach)
{
	void *fe = NULL;
	struct demod_ops *ops = NULL;

	if (IS_ERR_OR_NULL(demod))
		return -EFAULT;

	mutex_lock(&demod->mutex);

	list_for_each_entry(ops, &demod->list, list) {
		if ((ops->attached && attach) ||
			(!ops->attached && !attach)) {
			pr_err("Demod: demod%d [id %d] had %s.\n",
					ops->index, ops->cfg.id,
					attach ? "attached" : "detached");

			continue;
		}

		if (attach) {
			fe = ops->module->attach(ops->module, &ops->cfg);
			if (!IS_ERR_OR_NULL(fe)) {
				ops->attached = true;
				ops->fe = fe;

				if (!ops->external)
					dvb_tuner_attach(ops->fe);

				pr_err("Demod: attach demod%d [id %d] done.\n",
						ops->index, ops->cfg.id);
			} else {
				ops->attached = false;

				pr_err("Demod: attach demod%d [id %d] fail.\n",
						ops->index, ops->cfg.id);
			}
		} else {
			if (demod->used == ops)
				demod->used = NULL;

			ops->module->detach(ops->module);

			ops->attached = false;
			ops->delivery_system = SYS_UNDEFINED;
			ops->type = AML_FE_UNDEFINED;
			ops->fe = NULL;

			pr_err("Demod: detach demod%d [id %d] done.\n",
					ops->index, ops->cfg.id);
		}
	}

	mutex_unlock(&demod->mutex);

	return 0;
}

static struct demod_ops *demod_match(struct dvb_demod *demod, const int std)
{
	int ret = 0;
	struct demod_ops *ops = NULL;

	if (IS_ERR_OR_NULL(demod))
		return NULL;

	mutex_lock(&demod->mutex);

	if (demod->used) {
		if (demod->used->type == std) {
			mutex_unlock(&demod->mutex);

			return demod->used;
		}

		ret = demod->used->module->match(demod->used->module, std);
		if (!ret) {
			demod->used->type = std;

			mutex_unlock(&demod->mutex);

			return demod->used;
		}
	}

	demod->used = NULL;
	list_for_each_entry(ops, &demod->list, list) {
		if (!ops->attached || (ops->cfg.detect && !ops->valid))
			continue;

		ret = ops->module->match(ops->module, std);
		if (!ret) {
			ops->type = std;
			demod->used = ops;

			break;
		}
	}

	mutex_unlock(&demod->mutex);

	return demod->used;
}

static int demod_detect(struct dvb_demod *demod)
{
	int ret = 0;
	struct demod_ops *ops = NULL;

	if (IS_ERR_OR_NULL(demod))
		return -EFAULT;

	mutex_lock(&demod->mutex);

	list_for_each_entry(ops, &demod->list, list) {
		if (!ops->attached || !ops->cfg.detect || ops->valid)
			continue;

		if (ops->fe->ops.init) {
			ret = ops->fe->ops.init(ops->fe);
		} else {
			pr_err("Demod: demod%d [id %d] init() is NULL.\n",
					ops->index, ops->cfg.id);

			continue;
		}

		if (!ret) {
			ret = ops->module->detect(&ops->cfg);
			if (!ret)
				ops->valid = true;
			else
				ops->valid = false;

			pr_err("Demod: detect demod%d [id %d] %s.\n",
					ops->index, ops->cfg.id,
					ops->valid ? "done" : "fail");

			if (ops->fe->ops.release)
				ops->fe->ops.release(ops->fe);
		} else {
			pr_err("Demod: demod%d [id %d] init() error, ret %d.\n",
					ops->index, ops->cfg.id, ret);
		}
	}

	mutex_unlock(&demod->mutex);

	return 0;
}

static int demod_register_frontend(struct dvb_demod *demod, bool regist)
{
	int ret = 0;
	struct demod_ops *ops = NULL;

	if (IS_ERR_OR_NULL(demod) || IS_ERR_OR_NULL(demod->dvb_adapter))
		return -EFAULT;

	mutex_lock(&demod->mutex);

	list_for_each_entry(ops, &demod->list, list) {
		if (!ops->attached) {
			pr_err("Demod: demod%d [id %d] had not attached.\n",
					ops->index, ops->cfg.id);
			continue;
		}

		if (!ops->valid && ops->cfg.detect) {
			pr_err("Demod: demod%d [id %d] had not detected.\n",
					ops->index, ops->cfg.id);
			continue;
		}

		if ((ops->registered && regist) ||
			(!ops->registered && !regist)) {
			pr_err("Demod: demod%d [id %d] had %sregistered.\n",
					ops->index, ops->cfg.id,
					regist ? "" : "un");

			continue;
		}

		if (regist) {
			ret = ops->module->register_frontend(
					demod->dvb_adapter, ops->fe);
			if (ret)
				pr_err("Demod: demod%d [id %d] register frontend fail, ret %d.\n",
						ops->index, ops->cfg.id, ret);
			else
				ops->registered = true;
		} else {
			ret = ops->module->unregister_frontend(ops->fe);
			if (ret)
				pr_err("Demod: demod%d [Id %d] unregister frontend fail, ret %d.\n",
						ops->index, ops->cfg.id, ret);
			else
				ops->registered = false;
		}
	}

	mutex_unlock(&demod->mutex);

	return 0;
}

static int demod_pre_init(struct dvb_demod *demod)
{
	struct demod_ops *ops = NULL;

	if (IS_ERR_OR_NULL(demod))
		return -EFAULT;

	mutex_lock(&demod->mutex);

	list_for_each_entry(ops, &demod->list, list) {
		if (!ops->attached || ops->pre_inited)
			continue;

		/* In some cases, pre-init is required. */
		ops->pre_inited = true;
	}

	mutex_unlock(&demod->mutex);

	return 0;
}

static DEFINE_MUTEX(dvb_demods_mutex);

static struct dvb_demod demods = {
	.used = NULL,
	.list = LIST_HEAD_INIT(demods.list),
	.mutex = __MUTEX_INITIALIZER(demods.mutex),
	.refcount = 0,
	.attach = demod_attach,
	.match = demod_match,
	.detect = demod_detect,
	.register_frontend = demod_register_frontend,
	.pre_init = demod_pre_init
};

int dvb_extern_register_frontend(struct dvb_adapter *adapter)
{
	struct dvb_demod *demod = NULL;

	if (IS_ERR_OR_NULL(adapter)) {
		pr_err("Demod: %s: NULL or error pointer of adapter.\n",
				__func__);

		return -EFAULT;
	}

	mutex_lock(&dvb_demods_mutex);

	demod = get_dvb_demods();

	demod->dvb_adapter = adapter;

	demod->attach(demod, true);

	demod->detect(demod);

	demod->register_frontend(demod, true);

	demod->pre_init(demod);

	demod->refcount++;

	mutex_unlock(&dvb_demods_mutex);

	return 0;
}
EXPORT_SYMBOL(dvb_extern_register_frontend);

int dvb_extern_unregister_frontend(void)
{
	struct dvb_demod *demod = NULL;

	mutex_lock(&dvb_demods_mutex);

	demod = get_dvb_demods();

	demod->attach(demod, false);

	demod->register_frontend(demod, false);

	demod->refcount--;

	demod->dvb_adapter = NULL;

	mutex_unlock(&dvb_demods_mutex);

	return 0;
}
EXPORT_SYMBOL(dvb_extern_unregister_frontend);

struct demod_ops *dvb_demod_ops_create(void)
{
	struct demod_ops *ops = NULL;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (ops) {
		ops->attached = false;
		ops->registered = false;
		ops->index = -1;
		ops->pre_inited = false;
		ops->delivery_system = SYS_UNDEFINED;
		ops->type = AML_FE_UNDEFINED;
		ops->module = NULL;
		INIT_LIST_HEAD(&ops->list);
	}

	return ops;
}
EXPORT_SYMBOL(dvb_demod_ops_create);

void dvb_demod_ops_destroy(struct demod_ops *ops)
{
	if (IS_ERR_OR_NULL(ops))
		return;

	dvb_demod_ops_remove(ops);

	kfree(ops);
}
EXPORT_SYMBOL(dvb_demod_ops_destroy);

void dvb_demod_ops_destroy_all(void)
{
	struct dvb_demod *demod = NULL;
	struct demod_ops *ops = NULL, *temp = NULL;

	mutex_lock(&dvb_demods_mutex);

	demod = get_dvb_demods();

	list_for_each_entry_safe(ops, temp, &demod->list, list) {
		dvb_demod_ops_destroy(ops);
	}

	list_del_init(&demod->list);

	mutex_unlock(&dvb_demods_mutex);
}
EXPORT_SYMBOL(dvb_demod_ops_destroy_all);

int dvb_demod_ops_add(struct demod_ops *ops)
{
	struct demod_ops *p = NULL;
	struct dvb_demod *demod = NULL;

	if (IS_ERR_OR_NULL(ops))
		return -EFAULT;

	mutex_lock(&dvb_demods_mutex);

	demod = get_dvb_demods();

	list_for_each_entry(p, &demod->list, list) {
		if (p == ops) {
			mutex_unlock(&dvb_demods_mutex);

			pr_err("Demod: demod%d [id %d] ops [0x%p] exist.\n",
					ops->index, ops->cfg.id, ops);

			return -EEXIST;
		}
	}

	ops->module = aml_get_demod_module(ops->cfg.id);
	if (IS_ERR_OR_NULL(ops->module)) {
		mutex_unlock(&dvb_demods_mutex);

		return -ENODEV;
	}

	ops->external = (ops->cfg.id != AM_DTV_DEMOD_AMLDTV);

	list_add_tail(&ops->list, &demod->list);

	mutex_unlock(&dvb_demods_mutex);

	return 0;
}
EXPORT_SYMBOL(dvb_demod_ops_add);

void dvb_demod_ops_remove(struct demod_ops *ops)
{
	if (IS_ERR_OR_NULL(ops))
		return;

	if (ops->attached) {
		if (ops->module) {
			if (ops->module->detach)
				ops->module->detach(ops->module);

			if (ops->module->register_frontend)
				ops->module->unregister_frontend(ops->fe);
		}

		ops->attached = false;
		ops->registered = false;
		ops->valid = false;
		ops->pre_inited = false;
		ops->external = false;
		ops->index = -1;
		ops->delivery_system = SYS_UNDEFINED;
		ops->type = AML_FE_UNDEFINED;
		ops->module = NULL;
	}

	list_del_init(&ops->list);
}
EXPORT_SYMBOL(dvb_demod_ops_remove);

struct demod_ops *dvb_demod_ops_get_byindex(int index)
{
	struct dvb_demod *demod = NULL;
	struct demod_ops *ops = NULL;

	mutex_lock(&dvb_demods_mutex);

	demod = get_dvb_demods();

	list_for_each_entry(ops, &demod->list, list) {
		if (ops->index == index) {
			mutex_unlock(&dvb_demods_mutex);

			return ops;
		}
	}

	mutex_unlock(&dvb_demods_mutex);

	return NULL;
}
EXPORT_SYMBOL(dvb_demod_ops_get_byindex);

struct dvb_demod *get_dvb_demods(void)
{
	return &demods;
}
