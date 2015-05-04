/*
 * drivers/amlogic/power/aml_pmu_of_common.c
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

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include <linux/amlogic/battery_parameter.h>
#include <linux/amlogic/aml_pmu_common.h>

#define AML_I2C_BUS_AO		0
#define AML_I2C_BUS_A		1
#define AML_I2C_BUS_B		2

#define DEBUG_TREE		0
#define DEBUG_PARSE		0
#define DBG(format, args...) pr_info("%s, "format, __func__, ##args)

/*
 * must make sure value is 32 bit when use this macro
 * otherwise you should use another variable to get result value
 */
#define P_U32_PROP(node, prop_name, value, exception) \
do { \
	if (of_property_read_u32(node, prop_name, (u32 *)(&value))) { \
		DBG("failed to get property: %s\n", prop_name); \
		goto exception; \
	} \
	if (DEBUG_PARSE) { \
		DBG("get property:%25s, value:0x%08x, dec:%8d\n", \
		    prop_name, value, value); \
	} \
} while (0)

#define P_STR_PROP(node, prop_name, value, exception) \
do { \
	if (of_property_read_string(node, prop_name, (const char **)&value)) {\
		DBG("failed to get property: %s\n", prop_name); \
		goto exception; \
	} \
	if (DEBUG_PARSE) { \
		DBG("get property:%25s, value:%s\n", \
		    prop_name, value); \
	} \
} while (0)

/*
 * common API for callback and drivers
 */
static void	*pmu_callback_mutex;
static struct aml_pmu_driver *g_driver;
static struct aml_pmu_api *g_aml_pmu_api;

struct pmu_callback_group {
	char			  name[20];
	void			  *private;
	pmu_callback		  callback;
	struct pmu_callback_group *next;
};

static struct pmu_callback_group *g_callbacks;

int aml_pmu_register_callback(pmu_callback callback, void *pdata, char *name)
{
	struct pmu_callback_group *cg = NULL, *cn;

	if (!pmu_callback_mutex) {
		pmu_callback_mutex = pmu_alloc_mutex();
		if (!pmu_callback_mutex) {
			pr_info("%s, allocate mutex failed\n", __func__);
			return -ENOMEM;
		}
	}
	pmu_mutex_lock(pmu_callback_mutex);
	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg) {
		pr_info("%s, allocate callback failed\n", __func__);
		pmu_mutex_unlock(pmu_callback_mutex);
		return -ENOMEM;
	}
	cg->callback = callback;
	cg->private  = pdata;
	strcpy(cg->name, name);
	if (!g_callbacks) {			 /* add first callback */
		g_callbacks = cg;
		pr_info("callback %s registed, cg:%p\n", cg->name, cg);
		pmu_mutex_unlock(pmu_callback_mutex);
		return 0;
	}
	for (cn = g_callbacks; cn->next; cn = cn->next) {
		if (name && !strcmp(cn->name, name)) {
			pr_info("callback %s is already exist\n", name);
			pmu_mutex_unlock(pmu_callback_mutex);
			return -EINVAL;
		}
	}
	cn->next = cg;
	pr_info("callback %s registed, cg:%p\n", cg->name, cg);
	pmu_mutex_unlock(pmu_callback_mutex);
	return 0;
}
EXPORT_SYMBOL(aml_pmu_register_callback);

int aml_pmu_unregister_callback(char *name)
{
	struct pmu_callback_group *cn, *tmp = g_callbacks;
	int find = 0;
	pmu_mutex_lock(pmu_callback_mutex);
	if (name && !strcmp(tmp->name, name)) {	/* first node is target */
		g_callbacks = g_callbacks->next;
		kfree(tmp);
		pr_info("%s, callback %s unregisted\n", __func__, name);
		find = 1;
	}
	if (g_callbacks) {
		for (cn = g_callbacks->next; cn; cn = cn->next) {
			if (name && !strcmp(cn->name, name)) {
				tmp->next = cn->next;
				kfree(cn);
				pr_info("callback %s unregisted\n", name);
				find = 1;
				break;
			}
			tmp = tmp->next;
		}
	}
	pmu_mutex_unlock(pmu_callback_mutex);
	if (!find) {
		pr_info("%s, callback %s not find\n", __func__, name);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(aml_pmu_unregister_callback);

void aml_pmu_do_callbacks(struct aml_charger *charger)
{
	struct pmu_callback_group *cn;

	if (g_callbacks) {
		pmu_mutex_lock(pmu_callback_mutex);
		for (cn = g_callbacks; cn; cn = cn->next) {
			if (cn->callback)
				cn->callback(charger, cn->private);
		}
		pmu_mutex_unlock(pmu_callback_mutex);
	}
}
EXPORT_SYMBOL(aml_pmu_do_callbacks);

long aml_pmu_get_ts(void)
{
	struct timespec ts;
	ktime_get_ts(&ts);
	return ts.tv_sec;
}
EXPORT_SYMBOL(aml_pmu_get_ts);

int aml_pmu_register_api(struct aml_pmu_api *api)
{
	if (!api || g_aml_pmu_api) {
		pr_info("invalid input, api:%p\n", g_aml_pmu_api);
		return -EINVAL;
	}
	g_aml_pmu_api = api;
	return 0;
}
EXPORT_SYMBOL(aml_pmu_register_api);

void aml_pmu_clear_api(void)
{
	g_aml_pmu_api = NULL;
}
EXPORT_SYMBOL(aml_pmu_clear_api);

struct aml_pmu_api *aml_pmu_get_api(void)
{
	return g_aml_pmu_api;
}
EXPORT_SYMBOL(aml_pmu_get_api);

/*
 * register PMU operater for this lib
 * @driver: amlogic PMU driver to be registed
 */
int aml_pmu_register_driver(struct aml_pmu_driver *driver)
{
	if (driver == NULL) {
		pr_info("%s, ERROR:NULL driver\n", __func__);
		return -1;
	}
	if (g_driver != NULL) {
		pr_info("ERROR:driver %s has alread registed\n", driver->name);
		return -1;
	}
	if (!driver->pmu_get_coulomb || !driver->pmu_update_status) {
		pr_info("%s, lost important functions\n", __func__);
		return -1;
	}
	g_driver = driver;

	return 0;
}
EXPORT_SYMBOL(aml_pmu_register_driver);

/*
 * clear pmu drivers which already registered;
 */
void aml_pmu_clear_driver(void)
{
	g_driver = NULL;
}
EXPORT_SYMBOL(aml_pmu_clear_driver);

/*
 * return registed pmu driver
 */
struct aml_pmu_driver *aml_pmu_get_driver(void)
{
	return g_driver;
}
EXPORT_SYMBOL(aml_pmu_get_driver);


/*
 * common API of parse battery parameters for each PMU
 */
int parse_battery_parameters(struct device_node *n,
			     struct battery_parameter *b)
{
	unsigned int *curve;
	char *bat_name = NULL;
	int len;

	P_U32_PROP(n, "pmu_twi_id", b->pmu_twi_id, err);
	P_U32_PROP(n, "pmu_irq_id", b->pmu_irq_id, err);
	P_U32_PROP(n, "pmu_twi_addr", b->pmu_twi_addr, err);
	P_U32_PROP(n, "pmu_battery_rdc", b->pmu_battery_rdc, err);
	P_U32_PROP(n, "pmu_battery_cap", b->pmu_battery_cap, err);
	P_U32_PROP(n, "pmu_battery_technology", b->pmu_battery_technology, err);
	P_STR_PROP(n, "pmu_battery_name", bat_name, err);
	/*
	 * of_property_read_string only change output pointer address,
	 * so need copy string from pointed address to array.
	 */
	if (bat_name)
		strcpy(b->pmu_battery_name, bat_name);
	P_U32_PROP(n, "pmu_init_chgvol", b->pmu_init_chgvol, err);
	P_U32_PROP(n, "pmu_init_chgend_rate", b->pmu_init_chgend_rate, err);
	P_U32_PROP(n, "pmu_init_chg_enabled", b->pmu_init_chg_enabled, err);
	P_U32_PROP(n, "pmu_init_adc_freq", b->pmu_init_adc_freq, err);
	P_U32_PROP(n, "pmu_init_adc_freqc", b->pmu_init_adc_freqc, err);
	P_U32_PROP(n, "pmu_init_chg_pretime", b->pmu_init_chg_pretime, err);
	P_U32_PROP(n, "pmu_init_chg_csttime", b->pmu_init_chg_csttime, err);
	P_U32_PROP(n, "pmu_init_chgcur", b->pmu_init_chgcur, err);
	P_U32_PROP(n, "pmu_suspend_chgcur", b->pmu_suspend_chgcur, err);
	P_U32_PROP(n, "pmu_resume_chgcur", b->pmu_resume_chgcur, err);
	P_U32_PROP(n, "pmu_shutdown_chgcur", b->pmu_shutdown_chgcur, err);
	P_U32_PROP(n, "pmu_usbcur_limit", b->pmu_usbcur_limit, err);
	P_U32_PROP(n, "pmu_usbcur", b->pmu_usbcur, err);
	P_U32_PROP(n, "pmu_usbvol_limit", b->pmu_usbvol_limit, err);
	P_U32_PROP(n, "pmu_usbvol", b->pmu_usbvol, err);
	P_U32_PROP(n, "pmu_pwroff_vol", b->pmu_pwroff_vol, err);
	P_U32_PROP(n, "pmu_pwron_vol", b->pmu_pwron_vol, err);
	P_U32_PROP(n, "pmu_pekoff_time", b->pmu_pekoff_time, err);
	P_U32_PROP(n, "pmu_pekoff_en", b->pmu_pekoff_en, err);
	P_U32_PROP(n, "pmu_peklong_time", b->pmu_peklong_time, err);
	P_U32_PROP(n, "pmu_pwrok_time", b->pmu_pwrok_time, err);
	P_U32_PROP(n, "pmu_pwrnoe_time", b->pmu_pwrnoe_time, err);
	P_U32_PROP(n, "pmu_intotp_en", b->pmu_intotp_en, err);
	P_U32_PROP(n, "pmu_pekon_time", b->pmu_pekon_time, err);
	P_U32_PROP(n, "pmu_charge_efficiency",  b->pmu_charge_efficiency, err);
	/*
	 * These 4 members are not "MUST HAVE", so if parse erred,
	 * just keep going
	 */
	P_U32_PROP(n, "pmu_ntc_enable", b->pmu_ntc_enable, next1);
next1:
	P_U32_PROP(n, "pmu_ntc_ts_current", b->pmu_ntc_ts_current, next2);
next2:
	P_U32_PROP(n, "pmu_ntc_lowtempvol", b->pmu_ntc_lowtempvol, next3);
next3:
	P_U32_PROP(n, "pmu_ntc_hightempvol", b->pmu_ntc_hightempvol, next4);
next4:
	curve = (unsigned int *)b->pmu_bat_curve;
	len = sizeof(b->pmu_bat_curve) / sizeof(int);
	if (of_property_read_u32_array(n, "pmu_bat_curve", curve, len)) {
		DBG("failed to read battery curve\n");
		goto err;
	}
	return 0;

err:
	return -EINVAL;
}
EXPORT_SYMBOL(parse_battery_parameters);

static int aml_pmus_probe(struct platform_device *pdev)
{
	struct device_node	*pmu_node = pdev->dev.of_node;
	struct device_node	*child;
	struct i2c_board_info   board_info;
	struct i2c_adapter	*adapter;
	struct i2c_client	*client;
	int	err;
	int	addr;
	int	bus_type = -1;
	const  char *str;

	for_each_child_of_node(pmu_node, child) {
		/* register exist pmu */
		pr_info("%s, child name:%s\n", __func__, child->name);
		err = of_property_read_string(child, "i2c_bus", &str);
		if (err) {
			pr_info("get 'i2c_bus' failed, ret:%d\n", err);
			continue;
		}
		pr_info("%s, i2c_bus:%s\n", __func__, str);
		if (!strncmp(str, "i2c_bus_ao", 10))
			bus_type = AML_I2C_BUS_AO;
		else if (!strncmp(str, "i2c_bus_b", 9))
			bus_type = AML_I2C_BUS_B;
		else if (!strncmp(str, "i2c_bus_a", 9))
			bus_type = AML_I2C_BUS_A;
		else
			bus_type = AML_I2C_BUS_AO;
		err = of_property_read_string(child, "status", &str);
		if (err) {
			pr_info("get 'status' failed, ret:%d\n", err);
			continue;
		}
		if (strcmp(str, "okay") && strcmp(str, "ok")) {
			/* status is not OK, do not probe it */
			pr_info("device %s status is %s, stop probe it\n",
				 child->name, str);
			continue;
		}
		err = of_property_read_u32(child, "reg", &addr);
		if (err) {
			pr_info("get 'reg' failed, ret:%d\n", err);
			continue;
		}
		memset(&board_info, 0, sizeof(board_info));
		adapter = i2c_get_adapter(bus_type);
		if (!adapter)
			pr_info("wrong i2c adapter:%d\n", bus_type);
		err = of_property_read_string(child, "compatible", &str);
		if (err) {
			pr_info("get 'compatible' failed, ret:%d\n", err);
			continue;
		}
		strncpy(board_info.type, str, I2C_NAME_SIZE);
		board_info.addr = addr;
		board_info.of_node = child;	 /* for device driver */
		board_info.irq = irq_of_parse_and_map(child, 0);
		client = i2c_new_device(adapter, &board_info);
		if (!client) {
			pr_info("%s, allocate i2c_client failed\n", __func__);
			continue;
		}
		pr_info("%s: adapter:%d, addr:0x%x, node name:%s, type:%s\n",
			 "Allocate new i2c device",
			 bus_type, addr, child->name, str);
	}
	return 0;
}

static int aml_pmus_remove(struct platform_device *pdev)
{
	/* nothing to do */
	return 0;
}

static const struct of_device_id aml_pmu_dt_match[] = {
	{
		.compatible = "amlogic, aml_pmu_prober",
	},
	{}
};

static  struct platform_driver aml_pmu_prober = {
	.probe	 = aml_pmus_probe,
	.remove	 = aml_pmus_remove,
	.driver	 = {
		.name   = "aml_pmu_prober",
		.owner  = THIS_MODULE,
		.of_match_table = aml_pmu_dt_match,
	},
};

static int __init aml_pmu_probe_init(void)
{
	int ret;
	pr_info("call %s in\n", __func__);
	ret = platform_driver_register(&aml_pmu_prober);
	return ret;
}

static void __exit aml_pmu_probe_exit(void)
{
	platform_driver_unregister(&aml_pmu_prober);
}

subsys_initcall(aml_pmu_probe_init);
module_exit(aml_pmu_probe_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic pmu common driver");
