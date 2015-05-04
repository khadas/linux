/*
 * drivers/amlogic/power/aml_dvfs/aml_dvfs.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/cpufreq.h>
#include <linux/amlogic/aml_dvfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/sched.h>

#define DVFS_DBG(format, args...) \
	({if (1) pr_info("[DVFS]"format, ##args); })

#define DVFS_WARN(format, args...) \
	({if (1) pr_info("[DVFS]"format, ##args); })

#define DEBUG_DVFS		0

DEFINE_MUTEX(driver_mutex);
/*
 * @id: id of dvfs source
 * @table_count: count of dvfs table
 * @driver: voltage scale driver for this source
 * @table: dvfs table
 */
struct aml_dvfs_master {
	unsigned int			id;
	unsigned int			table_count;

	struct aml_dvfs_driver		*driver;
	struct aml_dvfs			*table;
	struct cpufreq_frequency_table  *freq_table;
	struct mutex			mutex;
	struct list_head		list;
};

LIST_HEAD(__aml_dvfs_list);

int aml_dvfs_register_driver(struct aml_dvfs_driver *driver)
{
	struct list_head *element;
	struct aml_dvfs_master *master = NULL, *target = NULL;

	if (driver == NULL) {
		DVFS_DBG("%s, NULL input of driver\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&driver_mutex);
	list_for_each(element, &__aml_dvfs_list) {
		master = list_entry(element, struct aml_dvfs_master, list);
		if (!master)
			continue;

		if (driver->id_mask & master->id) {
			/* driver support for this dvfs source */
			target = master;
			break;
		}
	}
	if (!target)
		return -ENODEV;

	if (target->driver) {
		DVFS_DBG("%s, source id %x has driver %s, reject %s\n",
			 __func__, target->id, target->driver->name,
			 driver->name);
		return -EINVAL;
	}
	target->driver = driver;
	DVFS_DBG("%s, %s regist success, mask:%x, source id:%x\n",
		 __func__, driver->name,
		 driver->id_mask, target->id);
	mutex_unlock(&driver_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvfs_register_driver);

int aml_dvfs_unregister_driver(struct aml_dvfs_driver *driver)
{
	struct list_head *element;
	struct aml_dvfs_master *master;
	int ok = 0;

	if (driver == NULL)
		return -EINVAL;

	mutex_lock(&driver_mutex);
	list_for_each(element, &__aml_dvfs_list) {
		master = list_entry(element, struct aml_dvfs_master, list);
		if (master && master->driver == driver) {
			DVFS_DBG("%s, driver %s unregist success\n",
				 __func__, master->driver->name);
			master->driver = NULL;
			ok = 1;
		}
	}
	mutex_unlock(&driver_mutex);
	if (!ok)
		DVFS_DBG("%s, driver %s not found\n", __func__, driver->name);

	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvfs_unregister_driver);

int aml_dvfs_find_voltage(struct aml_dvfs *table, unsigned int freq,
			  unsigned int *min_uV,
			  unsigned int *max_uV,
			  int count)
{
	int i = 0;

	if (unlikely(freq <= table[0].freq)) {
		*min_uV = table[0].min_uV;
		*max_uV = table[0].max_uV;
		return 0;
	}
	for (i = 0; i < count - 1; i++) {
		if (table[i].freq	  <  freq &&
			table[i + 1].freq >= freq) {
			*min_uV = table[i + 1].min_uV;
			*max_uV = table[i + 1].max_uV;
			return 0;
		}
	}
	return -EINVAL;
}

int aml_dvfs_do_voltage_change(struct aml_dvfs_master *master,
			       uint32_t new_freq,
			       uint32_t old_freq,
			       uint32_t flags)
{
	uint32_t id = master->id;
	uint32_t min_uV = 0, max_uV = 0, curr_voltage = 0;
	int ret = 0;

	if (master->table == NULL) {
		DVFS_WARN("%s, no dvfs table\n", __func__);
		goto error;
	}
	if (master->driver == NULL) {
		DVFS_WARN("%s, no dvfs driver\n", __func__);
		goto error;
	}
	/*
	 * update voltage
	 */
	if ((flags == AML_DVFS_FREQ_PRECHANGE  && new_freq >= old_freq) ||
	    (flags == AML_DVFS_FREQ_POSTCHANGE && new_freq <= old_freq)) {
		if (aml_dvfs_find_voltage(master->table, new_freq, &min_uV,
					  &max_uV,
					  master->table_count) < 0) {
			DVFS_DBG("%s, voltage not found for freq:%d\n",
				 __func__, new_freq);
			goto error;
		}
		if (master->driver->get_voltage) {
			master->driver->get_voltage(id, &curr_voltage);
			/* in range, do not change */
			if (curr_voltage >= min_uV && curr_voltage <= max_uV) {
			#if DEBUG_DVFS
				DVFS_WARN("%s, voltage %d is in [%d, %d]\n",
					  __func__, curr_voltage,
					  min_uV, max_uV);
			#endif
				goto ok;
			}
		}
		if (master->driver->set_voltage) {
		#if DEBUG_DVFS
			DVFS_WARN("%s, freq [%u -> %u], voltage [%u -> %u]\n",
				  __func__, old_freq, new_freq,
				  curr_voltage, min_uV);
		#endif
			ret = master->driver->set_voltage(id, min_uV, max_uV);
		#if DEBUG_DVFS
			DVFS_WARN("%s, set voltage finished\n", __func__);
		#endif
		}
	}
ok:
	return ret;
error:
	return -EINVAL;
}

int aml_dvfs_freq_change(unsigned int id,
			 unsigned int new_freq,
			 unsigned int old_freq,
			 unsigned int flag)
{
	struct aml_dvfs_master  *m = NULL;
	struct list_head	*element;
	int ret = 0;

	list_for_each(element, &__aml_dvfs_list) {
		m = list_entry(element, struct aml_dvfs_master, list);
		if (m->id != id)
			break;
	}
	mutex_lock(&m->mutex);
	ret = aml_dvfs_do_voltage_change(m, new_freq, old_freq, flag);
	mutex_unlock(&m->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(aml_dvfs_freq_change);

static int aml_dummy_set_voltage(uint32_t id, uint32_t min_uV, uint32_t max_uV)
{
	return 0;
}

struct aml_dvfs_driver aml_dummy_dvfs_driver = {
	.name		 = "aml-dumy-dvfs",
	.id_mask	 = 0,
	.set_voltage = aml_dummy_set_voltage,
	.get_voltage = NULL,
};

#define DVFS_HELP_MSG		\
	"HELP:\n" \
	"   echo r [name] > dvfs         -- get voltage of [name]\n" \
	"   echo w [name] [value] > dvfs -- set voltage [name] to [value]\n" \
	"\n" \
	"EXAMPLE:\n" \
	"   echo r vcck > dvfs           -- read current voltage of vcck\n" \
	"   echo w vcck 1100000 > dvfs   -- set voltage of vcck to 1.1v\n" \
	"\n" \
	"Supported names:\n" \
	"   vcck    -- voltage of ARM core\n" \
	"   vddee   -- voltage of VDDEE(everything else)\n" \
	"   ddr     -- voltage of DDR\n"

static ssize_t dvfs_help(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%s", DVFS_HELP_MSG);
}

static int get_dvfs_id_by_name(char *str)
{
	if (!strncmp(str, "vcck", 4)) {
		str[4] = '\0';
		return AML_DVFS_ID_VCCK;
	} else if (!strncmp(str, "vddee", 5)) {
		str[5] = '\0';
		return AML_DVFS_ID_VDDEE;
	} else if (!strncmp(str, "ddr", 3)) {
		str[3] = '\0';
		return AML_DVFS_ID_DDR;
	}
	return -1;
}

static ssize_t dvfs_class_write(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
	int  id, i;
	unsigned int uV;
	char *arg[3] = {}, *para, *buf_work, *p;
	struct aml_dvfs_master  *m, *t = NULL;
	struct list_head	*element;

	buf_work = kstrdup(buf, GFP_KERNEL);
	p = buf_work;
	for (i = 0; i < 3; i++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;

		arg[i] = para;
	}
	if (i < 2 || i > 3) {
		ret = 1;
		goto error;
	}

	switch (arg[0][0]) {
	case 'r':
		id = get_dvfs_id_by_name(arg[1]);
		if (id < 0)
			goto error;

		list_for_each(element, &__aml_dvfs_list) {
			m = list_entry(element, struct aml_dvfs_master, list);
			if (m->id == id) {
				t = m;
				break;
			}
		}
		if (!t) {
			pr_info("get %s failed\n", arg[1]);
			goto error;
		}
		mutex_lock(&t->mutex);
		if (t->driver->get_voltage)
			ret = t->driver->get_voltage(id, &uV);
		mutex_unlock(&t->mutex);
		if (ret < 0)
			pr_info("get voltage of %s failed\n", arg[1]);
		else
			pr_info("voltage of %s is %d\n", arg[1], uV);
		break;

	case 'w':
		if (i != 3)
			goto error;

		id = get_dvfs_id_by_name(arg[1]);
		if (id < 0)
			goto error;

		if (kstrtouint(arg[2], 10, &uV)) {
				pr_info("wrong input\n");
				goto error;
		}
		list_for_each(element, &__aml_dvfs_list) {
			m = list_entry(element, struct aml_dvfs_master, list);
			if (m->id == id) {
				t = m;
				break;
			}
		}
		if (!t) {
			pr_info("get %s failed\n", arg[1]);
			goto error;
		}
		mutex_lock(&t->mutex);
		if (t->driver->set_voltage)
			ret = t->driver->set_voltage(id, uV, uV);
		mutex_unlock(&t->mutex);
		if (ret < 0)
			pr_info("set %s to %d uV failed\n", arg[1], uV);
		else
			pr_info("set %s to %d uV success\n", arg[1], uV);
		break;
	default:
		break;
	}
error:
	kfree(buf_work);
	if (ret)
		pr_info(" error\n");
	return count;
}

static CLASS_ATTR(dvfs, S_IWUSR | S_IRUGO, dvfs_help, dvfs_class_write);
struct class *aml_dvfs_class;


static int aml_dvfs_probe(struct platform_device *pdev)
{
	struct device_node		*dvfs_node = pdev->dev.of_node;
	struct device_node		*child;
	struct aml_dvfs_master	*master;
	struct aml_dvfs			*table;
	int   err;
	int   id = 0;
	int   table_cnt;

	for_each_child_of_node(dvfs_node, child) {
		DVFS_DBG("%s, child name:%s\n", __func__, child->name);
		/* read dvfs id */
		err = of_property_read_u32(child, "dvfs_id", &id);
		if (err) {
			DVFS_DBG("%s, get 'dvfs_id' failed\n", __func__);
			continue;
		}
		master = kzalloc(sizeof(*master), GFP_KERNEL);
		if (master == NULL) {
			DVFS_DBG("%s, allocate memory failed\n", __func__);
			return -ENOMEM;
		}
		master->id = id;
		/* get table count */
		err = of_property_read_u32(child, "table_count", &table_cnt);
		if (err) {
			DVFS_DBG("%s, get 'table_count' failed\n", __func__);
			continue;
		}
	#if DEBUG_DVFS
		DVFS_DBG("%s, get table_count = %d\n", __func__, table_cnt);
	#endif
		master->table_count = table_cnt;
		table = kzalloc(sizeof(*table) * table_cnt, GFP_KERNEL);
		if (table == NULL) {
			DVFS_DBG("%s, allocate memory failed\n", __func__);
			return -ENOMEM;
		}
		err = of_property_read_u32_array(child, "dvfs_table",
						 (uint32_t *)table,
						 (sizeof(*table) * table_cnt) /
						  sizeof(unsigned int));
		DVFS_DBG("dvfs table of %s is:\n", child->name);
		DVFS_DBG("%9s, %9s, %9s\n", "freq", "min_uV", "max_uV");
		for (id = 0; id < table_cnt; id++) {
			DVFS_DBG("%9d, %9d, %9d\n",
				 table[id].freq, table[id].min_uV,
				 table[id].max_uV);
		}
		if (err) {
			DVFS_DBG("%s, get 'dvfs_table' failed\n", __func__);
			continue;
		}
		master->table = table;
		list_add_tail(&master->list, &__aml_dvfs_list);
		err = of_property_read_bool(child, "change-frequent-only");
		if (err) {
			aml_dummy_dvfs_driver.id_mask = master->id;
			aml_dvfs_register_driver(&aml_dummy_dvfs_driver);
		}
	}

	aml_dvfs_class = class_create(THIS_MODULE, "dvfs");
	return class_create_file(aml_dvfs_class, &class_attr_dvfs);
}

static int aml_dvfs_remove(struct platform_device *pdev)
{
	struct list_head *element;
	struct aml_dvfs_master *master;

	class_destroy(aml_dvfs_class);
	list_for_each(element, &__aml_dvfs_list) {
		master = list_entry(element, struct aml_dvfs_master, list);
		kfree(master->freq_table);
		kfree(master->table);
		kfree(master);
	}
	return 0;
}

static const struct of_device_id aml_dvfs_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-dvfs",
	},
	{}
};

static  struct platform_driver aml_dvfs_prober = {
	.probe   = aml_dvfs_probe,
	.remove  = aml_dvfs_remove,
	.driver  = {
		.name			= "amlogic-dvfs",
		.owner			= THIS_MODULE,
		.of_match_table = aml_dvfs_dt_match,
	},
};

static int __init aml_dvfs_init(void)
{
	int ret;

	pr_info("call %s in\n", __func__);
	ret = platform_driver_register(&aml_dvfs_prober);
	return ret;
}

static void __exit aml_dvfs_exit(void)
{
	platform_driver_unregister(&aml_dvfs_prober);
}

arch_initcall(aml_dvfs_init);
module_exit(aml_dvfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic DVFS interface driver");
