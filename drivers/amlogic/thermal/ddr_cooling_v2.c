// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/amlogic/ddr_cooling.h>
#include <linux/amlogic/meson_cooldev.h>
#include <linux/io.h>
#include "../../thermal/thermal_core.h"

static int ddrfreqcd_id;
static DEFINE_MUTEX(cooling_ddr_lock);
static DEFINE_MUTEX(cooling_list_lock);
static LIST_HEAD(ddr_dev_list);

static void ddr_coolingdevice_id_get(int *id)
{
	mutex_lock(&cooling_ddr_lock);
	*id = ddrfreqcd_id++;
	mutex_unlock(&cooling_ddr_lock);
}

static void ddr_coolingdevice_id_put(void)
{
	mutex_lock(&cooling_ddr_lock);
	ddrfreqcd_id--;
	mutex_unlock(&cooling_ddr_lock);
}

static int ddr_get_max_state(struct thermal_cooling_device *cdev,
			     unsigned long *state)
{
	struct ddr_cooling_device_v2 *ddr_device = cdev->devdata;

	*state = ddr_device->ddr_status - 1;
	return 0;
}

static int ddr_get_cur_state(struct thermal_cooling_device *cdev,
			     unsigned long *state)
{
	struct ddr_cooling_device_v2 *ddr_device = cdev->devdata;
	u32 val, mask;
	u32 start_bit, end_bit;

	start_bit = ddr_device->ddr_bits[0][0];
	end_bit = ddr_device->ddr_bits[0][1];
	mask = GENMASK(end_bit, start_bit);
	val = readl_relaxed(ddr_device->vddr_reg[0]);
	val = (val & mask) >> start_bit;

	*state = val;
	return 0;
}

static int ddr_set_cur_state(struct thermal_cooling_device *cdev,
			     unsigned long state)
{
	struct ddr_cooling_device_v2 *ddr_device = cdev->devdata;
	u32 val, mask;
	u32 start_bit, end_bit;
	int i;

	if (!(state < ddr_device->ddr_status)) {
		dev_warn(&ddr_device->cool_dev->device,
				"Not matching any states!\n");
		return -EINVAL;
	}

	for (i = 0; i < ddr_device->ddr_reg_cnt; i++) {
		start_bit = ddr_device->ddr_bits[i][0];
		end_bit = ddr_device->ddr_bits[i][1];
		mask = GENMASK(end_bit, start_bit);
		val = readl_relaxed(ddr_device->vddr_reg[i]);
		val = (val & (~mask)) | (ddr_device->ddr_data[i][state] << start_bit);

		writel_relaxed(val, ddr_device->vddr_reg[i]);
	}

	return 0;
}

static unsigned long cdev_calc_next_state_by_temp(struct thermal_instance *instance,
	int temperature)
{
	struct thermal_instance *ins = instance;
	struct thermal_zone_device *tz;
	struct thermal_cooling_device *cdev;
	struct ddr_cooling_device_v2 *ddr_device;
	int i, hyst = 0, trip_temp, max;

	if (!ins)
		return -EINVAL;

	tz = ins->tz;
	cdev = ins->cdev;

	if (!tz || !cdev)
		return -EINVAL;

	ddr_device = cdev->devdata;
	max = ddr_device->ddr_status - 1;

	tz->ops->get_trip_hyst(tz, instance->trip, &hyst);
	tz->ops->get_trip_temp(tz, instance->trip, &trip_temp);

	for (i = 0; i < ddr_device->ddr_status; i++) {
		if (temperature < (trip_temp + (i + 1) * hyst))
			break;
	}

	return i > max ? max : i;
}

static int ddr_get_requested_power(struct thermal_cooling_device *cdev,
				   struct thermal_zone_device *tz, u32 *power)
{
	struct thermal_instance *instance;

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		if (cdev->ops && cdev->ops->set_cur_state)
			*power = (u32)cdev_calc_next_state_by_temp(instance, tz->temperature);
	}
	mutex_unlock(&cdev->lock);

	return 0;
}

static int ddr_state2power(struct thermal_cooling_device *cdev,
			   struct thermal_zone_device *tz,
			   unsigned long state, u32 *power)
{
	*power = 0;
	return 0;
}

static int ddr_power2state(struct thermal_cooling_device *cdev,
			   struct thermal_zone_device *tz, u32 power,
			   unsigned long *state)
{
	return 0;
}

static int ddr_notify_state_v2(void *thermal_instance,
			    int trip, enum thermal_trip_type type)
{
	struct thermal_instance *ins = thermal_instance;
	struct thermal_zone_device *tz;
	struct thermal_cooling_device *cdev;
	struct ddr_cooling_device_v2 *ddrdev;
	u32 state_set;
	unsigned long state_get;

	if (!ins)
		return -EINVAL;

	tz = ins->tz;
	cdev = ins->cdev;

	if (!tz || !cdev)
		return -EINVAL;
	ddrdev = cdev->devdata;

	switch (type) {
	case THERMAL_TRIP_HOT:
		if (cdev->ops && cdev->ops->set_cur_state) {
			cdev->ops->get_requested_power(cdev, tz, &state_set);
			if (state_set != ddrdev->last_state) {
				cdev->ops->set_cur_state(cdev, (unsigned long)state_set);
				ddrdev->last_state = state_set;
				cdev->ops->get_cur_state(cdev, &state_get);
				pr_info("[%s]temp:%d, set state:0x%x, get reg0 val:0x%lx\n",
					cdev->type, tz->temperature, state_set, state_get);
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

static struct thermal_cooling_device_ops const ddr_cooling_ops = {
	.get_max_state = ddr_get_max_state,
	.get_cur_state = ddr_get_cur_state,
	.set_cur_state = ddr_set_cur_state,
	.state2power   = ddr_state2power,
	.power2state   = ddr_power2state,
	.notify_state  = ddr_notify_state_v2,
	.get_requested_power = ddr_get_requested_power,
};

struct thermal_cooling_device *
ddr_cooling_register_v2(struct device_node *np)
{
	struct thermal_cooling_device *cool_dev;
	struct ddr_cooling_device_v2 *ddrdev = NULL;
	struct thermal_instance *pos = NULL;
	char dev_name[20];
	const char *ddrdata_name[2] = {"ddr_data", "gpu_data"}, *ddrnp_name;
	struct device_node *ddrnp;
	int i, ddrreg_cnt;

	ddrdev = kmalloc(sizeof(*ddrdev), GFP_KERNEL);
	if (!ddrdev)
		return ERR_PTR(-EINVAL);
	ddr_coolingdevice_id_get(&ddrdev->id);
	ddrreg_cnt = of_property_count_u32_elems(np, "ddr_reg");
	if (ddrreg_cnt < 1)
		goto out;

	ddrdev->ddr_reg_cnt = ddrreg_cnt;
	ddrdev->ddr_reg = kmalloc_array(ddrreg_cnt, sizeof(u32), GFP_KERNEL);
	ddrdev->vddr_reg = kmalloc_array(ddrreg_cnt, sizeof(void *), GFP_KERNEL);
	ddrdev->ddr_bits = kmalloc_array(ddrreg_cnt, sizeof(u32) * 2, GFP_KERNEL);
	ddrdev->ddr_data = kmalloc_array(ddrreg_cnt, sizeof(u32) * 20, GFP_KERNEL);
	ddrdev->ddr_bits_keep = kmalloc_array(ddrreg_cnt, sizeof(u32), GFP_KERNEL);
	if (!ddrdev->ddr_reg || !ddrdev->vddr_reg || !ddrdev->ddr_bits_keep ||
		!ddrdev->ddr_bits || !ddrdev->ddr_data)
		goto out;

	if (of_property_read_u32_array(np, "ddr_reg", ddrdev->ddr_reg, ddrreg_cnt))
		goto out;
	if (of_property_read_u32(np, "ddr_status", &ddrdev->ddr_status)) {
		pr_err("thermal: read ddr reg_status failed\n");
		goto out;
	}
	if (of_property_read_u32_array(np, "ddr_bits",
				       ddrdev->ddr_bits[0], 2 * ddrreg_cnt)) {
		pr_err("thermal: read ddr_bits failed\n");
		goto out;
	}
	for (i = 0; i < ddrreg_cnt; i++) {
		if (of_property_read_u32_array(np, ddrdata_name[i],
					       ddrdev->ddr_data[i], ddrdev->ddr_status)) {
			pr_err("thermal: read ddr_data failed\n");
			goto out;
		}
	}
	for (i = 0; i < ddrreg_cnt; i++) {
		ddrdev->vddr_reg[i] = ioremap(ddrdev->ddr_reg[i], 4);
		if (!ddrdev->vddr_reg[i]) {
			pr_err("thermal ddr cdev: ddr reg ioremap fail.\n");
			goto out;
		}
		ddrdev->ddr_bits_keep[i] = ~(0xffffffff << (ddrdev->ddr_bits[i][1] + 1));
		ddrdev->ddr_bits_keep[i] = ~((ddrdev->ddr_bits_keep[i] >> ddrdev->ddr_bits[i][0])
				   << ddrdev->ddr_bits[i][0]);
	}

	if (of_property_read_string(np, "node_name", &ddrnp_name)) {
		pr_err("thermal: read node_name failed\n");
		goto out;
	}
	ddrnp = of_find_node_by_name(NULL, ddrnp_name);
	if (!ddrnp) {
		pr_err("thermal: can't find node\n");
		goto out;
	}
	snprintf(dev_name, sizeof(dev_name), "thermal-ddr2-%d", ddrdev->id);
	cool_dev = thermal_of_cooling_device_register(ddrnp, dev_name, ddrdev,
						      &ddr_cooling_ops);

	if (!cool_dev)
		goto out;

	ddrdev->cool_dev = cool_dev;

	list_for_each_entry(pos, &cool_dev->thermal_instances, cdev_node) {
		if (!strncmp(pos->cdev->type, dev_name, sizeof(dev_name))) {
			pr_err("Notice!!! The notify interface has been removed.\n");
			break;
		}
	}

	mutex_lock(&cooling_list_lock);
	list_add(&ddrdev->node, &ddr_dev_list);
	mutex_unlock(&cooling_list_lock);

	return cool_dev;
out:
	ddr_coolingdevice_id_put();
	kfree(ddrdev->ddr_reg);
	kfree(ddrdev->vddr_reg);
	kfree(ddrdev->ddr_bits);
	kfree(ddrdev->ddr_data);
	kfree(ddrdev->ddr_bits_keep);
	kfree(ddrdev);
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL_GPL(ddr_cooling_register_v2);

/**
 * cpucore_cooling_unregister - function to remove cpucore cooling device.
 * @cdev: thermal cooling device pointer.
 *
 * This interface function unregisters the "thermal-cpucore-%x" cooling device.
 */
void ddr_cooling_unregister_v2(struct thermal_cooling_device *cdev)
{
	struct ddr_cooling_device *ddrdev;

	if (!cdev)
		return;

	ddrdev = cdev->devdata;
	iounmap(ddrdev->vddr_reg);

	mutex_lock(&cooling_list_lock);
	list_del(&ddrdev->node);
	mutex_unlock(&cooling_list_lock);

	thermal_cooling_device_unregister(ddrdev->cool_dev);
	ddr_coolingdevice_id_put();
	kfree(ddrdev);
}
EXPORT_SYMBOL_GPL(ddr_cooling_unregister_v2);
