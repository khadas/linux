/*
 * drivers/amlogic/irblaster/core.c
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

#include <linux/module.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/amlogic/irblaster.h>

void irblaster_chip_data_clear(struct irblaster_chip *chip)
{
	chip->buffer = NULL;
	chip->buffer_len = 0;
	chip->sum_time = 0;
}

/**
 * irblaster_send() - send raw level data
 * @chip: irblaster controller
 * @data: raw level data (us)
 * @len: raw len
 */
int irblaster_send(struct irblaster_chip *chip, unsigned int *data,
		   unsigned int len)
{
	unsigned int sum_time = 0;
	int err, i;

	if (!chip || (len % 2 == 1) || len == 0 || len > MAX_PLUSE) {
		pr_err("%s(): parameter error\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < len; i++)
		sum_time += data[i];

	chip->buffer = data;
	chip->buffer_len = len;
	chip->sum_time = sum_time;

	if (chip->ops->send) {
		err = chip->ops->send(chip, data, len);
		if (err)
			return err;
	} else {
		pr_err("%s(): irblaster func %s not found\n",
		       __func__, __func__);
		return -EINVAL;
	}

	irblaster_chip_data_clear(chip);

	return 0;
}

/**
 * irblaster_set_freq() - set irblaster freq
 * @chip: irblaster controller
 * @freq: irblaster freq (HZ)
 */
int irblaster_set_freq(struct irblaster_chip *chip, unsigned int freq)
{
	int ret;

	if (!chip || freq <= 0)
		return -EINVAL;

	if (chip->ops->set_freq) {
		ret = chip->ops->set_freq(chip, freq);
		if (ret)
			return -EINVAL;
	}

	chip->state.freq = freq;

	return 0;
}

/**
 * irblaster_get_freq() - get irblaster freq
 * @chip: irblaster controller
 */
unsigned int irblaster_get_freq(struct irblaster_chip *chip)
{
	unsigned int freq;

	if (!chip)
		return -EINVAL;

	if (chip->ops->get_freq) {
		freq = chip->ops->get_freq(chip);
		if (freq == 0)
			return -EINVAL;
	} else {
		freq = chip->state.freq;
	}

	return freq;
}

/**
 * irblaster_set_duty() - set irblaster duty
 * @chip: irblaster controller
 * @duty: irblaster duty
 */
int irblaster_set_duty(struct irblaster_chip *chip, unsigned int duty)
{
	int ret;

	if (!chip || duty <= 0 || duty > 100)
		return -EINVAL;

	if (chip->ops->set_duty) {
		ret = chip->ops->set_duty(chip, duty);
		if (ret)
			return -EINVAL;
	}

	chip->state.duty = duty;

	return 0;
}

/**
 * irblaster_set_duty() - set irblaster duty
 * @chip: irblaster controller
 * @duty: irblaster duty
 */
unsigned int irblaster_get_duty(struct irblaster_chip *chip)
{
	unsigned int duty;

	if (chip->ops->get_duty) {
		duty = chip->ops->get_duty(chip);
		if (duty == 0)
			return -EINVAL;
	} else {
		duty = chip->state.duty;
	}

	return duty;
}

/**
 * irblasterchip_remove() - remove a irblaster Controller
 * @chip: the irblaster chip to remove
 * @Returns: 0 on success or a negative error code on failure.
 */
int irblasterchip_remove(struct irblaster_chip *chip)
{
	mutex_lock(&irblaster_lock);
	list_del_init(&chip->list);

	if (chip->dev)
		of_node_put(chip->dev->of_node);

	irblasterchip_sysfs_unexport(chip);
	mutex_unlock(&irblaster_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(irblasterchip_remove);

static bool irblaster_ops_check(struct irblaster_ops *ops)
{
	/* These one interfaces are the most basic of the irblaster */
	if (ops->send)
		return true;

	return false;
}

/**
 * irblasterchip_add() - register a new irblaster Controller
 * @chip: the irblaster chip to add
 * @Returns: 0 on success or a negative error code on failure.
 */
int irblasterchip_add(struct irblaster_chip *chip)
{
	if (!chip || !chip->dev || !chip->ops)
		return -EINVAL;

	if (!irblaster_ops_check(chip->ops))
		return -EINVAL;

	mutex_lock(&irblaster_lock);
	atomic_set(&chip->request, IRBLASTER_EXPORTED);
	INIT_LIST_HEAD(&chip->list);
	list_add(&chip->list, &irblaster_chips);
	irblasterchip_sysfs_export(chip);
	mutex_unlock(&irblaster_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(irblasterchip_add);

/**
 * irblaster_put() - release a irblaster controller
 * @chip: irblaster controller
 */
void irblaster_put(struct irblaster_chip *chip)
{
	if (!chip)
		return;

	mutex_lock(&irblaster_lock);
	atomic_set(&chip->request, IRBLASTER_EXPORTED);
	irblaster_chip_data_clear(chip);
	irblasterchip_sysfs_export(chip);
	mutex_unlock(&irblaster_lock);
}
EXPORT_SYMBOL_GPL(irblaster_put);

static struct irblaster_chip *of_node_to_irblasterchip(struct device_node *np)
{
	struct irblaster_chip *chip;

	mutex_lock(&irblaster_lock);
	list_for_each_entry(chip, &irblaster_chips, list)
		if (chip->dev && chip->dev->of_node == np) {
			mutex_unlock(&irblaster_lock);
			if (atomic_read(&chip->request) == IRBLASTER_REQUESTED)
				return ERR_PTR(-EPROBE_DEFER);
			return chip;
		}

	mutex_unlock(&irblaster_lock);
	return ERR_PTR(-EPROBE_DEFER);
}

static int irblaster_set_default_state(struct irblaster_chip *pc,
				       const struct of_phandle_args *args)
{
	int ret;

	if (pc->of_irblaster_n_cells < 2 ||
	    args->args[0] <= 0 || args->args[1] > 100)
		return -EINVAL;

	pc->state.freq = args->args[0];
	pc->state.duty = args->args[1];

	ret = irblaster_set_freq(pc, pc->state.freq);
	if (ret)
		return -EINVAL;

	ret = irblaster_set_duty(pc, pc->state.duty);
	if (ret)
		return -EINVAL;

	return 0;
}

/**
 * of_irblaster_get() - request a irblaster via the irblaster framework
 * @np: device node to get the irblaster from
 * @con_id: consumer name
 *
 * Returns the irblaster controller parsed from the phandle and index
 * specified in the "irblaster-config" property of a device tree node
 * or a negative error-code on failure. Values parsed from the device
 * tree are stored in the returned irblaster device object.
 *
 * Returns: A pointer to the requested irblaster controller or an ERR_PTR()
 * -encoded error code on failure.
 */
struct irblaster_chip *of_irblaster_get(struct device_node *np,
					const char *con_id)
{
	struct of_phandle_args args;
	struct irblaster_chip *pc;
	int err, index = 0;

	err = of_parse_phandle_with_args(np, "irblaster-config",
					 "#irblaster-cells", index,
					 &args);
	if (err) {
		pr_err("%s(): can't parse \"irblaster-config\" property\n",
		       __func__);
		return ERR_PTR(err);
	}

	pc = of_node_to_irblasterchip(args.np);
	if (IS_ERR(pc)) {
		pr_err("%s(): irblaster chip not found\n", __func__);
		pc = ERR_PTR(-EINVAL);
		goto put;
	}

	if (args.args_count != pc->of_irblaster_n_cells) {
		pr_err("%s: wrong #irblaster-cells for %s\n", np->full_name,
		       args.np->full_name);
		pc = ERR_PTR(-EINVAL);
		goto put;
	}

	err = irblaster_set_default_state(pc, &args);
	if (err < 0) {
		pr_err("%s(): irblaster get state fail\n", __func__);
		pc = ERR_PTR(-EINVAL);
		goto put;
	}

	atomic_set(&pc->request, IRBLASTER_REQUESTED);
	irblasterchip_sysfs_unexport(pc);
put:
	of_node_put(args.np);

	return pc;
}
EXPORT_SYMBOL_GPL(of_irblaster_get);

static void devm_irblaster_release(struct device *dev, void *res)
{
	irblaster_put(*(struct irblaster_chip **)res);
}

/**
 * devm_of_irblaster_get() - resource managed of_irblaster_get()
 * @dev: device for irblaster consumer
 * @np: device node to get the irblaster from
 * @con_id: consumer name
 *
 * This function performs like of_irblaster_get() but the acquired irblaster
 * device will automatically be released on driver detach.
 *
 * Returns: A pointer to the requested irblaster device or an ERR_PTR()-encoded
 * error code on failure.
 */
struct irblaster_chip *devm_of_irblaster_get(struct device *dev,
					     struct device_node *np,
					     const char *con_id)
{
	struct irblaster_chip **dr, *chip;

	dr = devres_alloc(devm_irblaster_release, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return ERR_PTR(-ENOMEM);

	chip = of_irblaster_get(np, con_id);
	if (!IS_ERR(chip)) {
		*dr = chip;
		devres_add(dev, dr);
	} else {
		devres_free(dr);
	}

	return chip;
}
EXPORT_SYMBOL_GPL(devm_of_irblaster_get);
