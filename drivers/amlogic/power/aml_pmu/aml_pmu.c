/*
 * drivers/amlogic/power/aml_pmu/aml_pmu.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/amlogic/aml_pmu.h>
#include <linux/amlogic/battery_parameter.h>
#ifdef CONFIG_AML_DVFS
#include <linux/amlogic/aml_dvfs.h>
#endif

#ifdef CONFIG_AMLOGIC_USB
#ifdef CONFIG_AML1212
static struct notifier_block aml1212_otg_nb;
static struct notifier_block aml1212_usb_nb;
#endif
#ifdef CONFIG_AML1216
static struct notifier_block aml1216_otg_nb;
static struct notifier_block aml1216_usb_nb;
#endif
#ifdef CONFIG_AML1218
static struct notifier_block aml1218_otg_nb;
static struct notifier_block aml1218_usb_nb;
#endif
#endif

#ifdef CONFIG_AML1216
struct i2c_client *g_aml1216_client = NULL;
#endif
#ifdef CONFIG_AML1218
struct i2c_client *g_aml1218_client = NULL;
#endif
#ifdef CONFIG_AML1212
struct i2c_client *g_aml1212_client = NULL;
#endif
static const struct i2c_device_id aml_pmu_id_table[] = {
#ifdef CONFIG_AML1212
	{ "aml1212", 0},
#endif
#ifdef CONFIG_AML1216
	{ AML1216_DRIVER_NAME, 1},
#endif
#ifdef CONFIG_AML1218
	{ AML1218_DRIVER_NAME, 2},
#endif
	{},
};
MODULE_DEVICE_TABLE(i2c, aml_pmu_id_table);

#ifdef CONFIG_OF
#define DEBUG_TREE	0
#define DEBUG_PARSE	0
#define DBG(format, args...) pr_info("[AML_PMU]%s, "format, __func__, ##args)

/*
 * must make sure value is 32 bit when use this macro
 * otherwise you should use another variable to get result value
 */
#define P_U32_PROP(node, prop_name, value, exception, lable) \
do { \
	if (of_property_read_u32(node, prop_name, (u32 *)(&value))) { \
		DBG("failed to get property: %s\n", prop_name); \
		if (exception) { \
			goto lable; \
		} \
	} \
	if (DEBUG_PARSE) { \
		DBG("get property:%25s, value:0x%08x, dec:%8d\n", \
		    prop_name, value, value); \
	} \
} while (0)

#define P_STR_PROP(node, prop_name, value, exception, lable) \
do { \
	if (of_property_read_string(node, prop_name, (const char **)&value)) {\
		DBG("failed to get property: %s\n", prop_name); \
		if (exception) { \
			goto lable; \
		} \
	} \
	if (DEBUG_PARSE) { \
		DBG("get property:%25s, value:%s\n", \
		    prop_name, value); \
	} \
} while (0)

#define ALLOC_DEVICES(return_pointer, size, flag) \
do { \
	return_pointer = kzalloc(size, flag); \
	if (!return_pointer) { \
		DBG("%d, allocate "#return_pointer" failed\n", __LINE__); \
		return -ENOMEM; \
	} \
} while (0)

#if DEBUG_TREE
char msg_buf[100];
static void scan_node_tree(struct device_node *top_node, int off)
{
	if (!top_node)
		return;
	if (!off)
		pr_info("device tree is :\n");
	while (top_node) {
		memset(msg_buf, ' ', sizeof(msg_buf));
		sprintf(msg_buf + off, "|--%s\n", top_node->name);
		pr_info(msg_buf);
		scan_node_tree(top_node->child, off + 4);
		top_node = top_node->sibling;
	}
}
#endif

static int setup_supply_data(struct device_node *n,
			     struct amlogic_pmu_init *s_data)
{
	struct device_node *b_node;
	struct battery_parameter *battery;
	phandle fhandle;

	P_U32_PROP(n, "soft_limit_to99", s_data->soft_limit_to99, 0, f);
	P_U32_PROP(n, "board_battery", fhandle, 0, f);
	P_U32_PROP(n, "vbus_dcin_short_connect",
		   s_data->vbus_dcin_short_connect, 0, f);
	b_node = of_find_node_by_phandle(fhandle);
	if (!b_node)
		DBG("find battery node failed, current:%s\n", n->name);
	else {
		ALLOC_DEVICES(battery, sizeof(*battery), GFP_KERNEL);
		if (parse_battery_parameters(b_node, battery)) {
			DBG("failed to parse battery parameter, node:%s\n",
			    b_node->name);
			kfree(battery);
		} else
			s_data->board_battery = battery;
	}
	return 0;

f:
	return -EINVAL;
}

static int setup_platform_pmu_init_data(struct device_node *node,
					struct amlogic_pmu_init *pdata)
{
	if (setup_supply_data(node, pdata))
		return  -EINVAL;

	return 0;
}

static struct i2c_device_id *find_id_by_name(const struct i2c_device_id *l,
					     char *name)
{
	while (l->name && l->name[0]) {
		pr_info("table name:%s, name:%s\n", l->name, name);
		if (!strcmp(l->name, name))
			return (struct i2c_device_id *)l;
		l++;
	}
	return NULL;
}
static struct amlogic_pmu_init *init_data;
#endif /* CONFIG_OF */

#if defined(CONFIG_AML_DVFS) && defined(CONFIG_AML1216)
static int aml_1216_convert_id_to_dcdc(uint32_t id)
{
	int dcdc = 0;
	switch (id) {
	case AML_DVFS_ID_VCCK:
		dcdc = 1;
		break;

	case AML_DVFS_ID_VDDEE:
		dcdc = 2;
		break;

	case AML_DVFS_ID_DDR:
		dcdc = 3;
		break;

	default:
		break;
	}
	return dcdc;
}

static int aml1216_set_voltage(uint32_t id, uint32_t min_uV, uint32_t max_uV)
{
	int dcdc = aml_1216_convert_id_to_dcdc(id);
	uint32_t vol = 0;

	if (min_uV > max_uV)
		return -1;
	vol = (min_uV + max_uV) / 2;
	if (dcdc >= 1 && dcdc <= 3)
		return aml1216_set_dcdc_voltage(dcdc, vol);
	return -EINVAL;
}

static int aml1216_get_voltage(uint32_t id, uint32_t *uV)
{
	int dcdc = aml_1216_convert_id_to_dcdc(id);

	if (dcdc >= 1 && dcdc <= 3)
		return aml1216_get_dcdc_voltage(dcdc, uV);
	return -EINVAL;
}

struct aml_dvfs_driver aml1216_dvfs_driver = {
	.name	     = "aml1216-dvfs",
	.id_mask     = (AML_DVFS_ID_VCCK | AML_DVFS_ID_VDDEE | AML_DVFS_ID_DDR),
	.set_voltage = aml1216_set_voltage,
	.get_voltage = aml1216_get_voltage,
};

#endif

#if defined(CONFIG_AML_DVFS) && defined(CONFIG_AML1218)
static int aml_1218_convert_id_to_dcdc(uint32_t id)
{
	int dcdc = 0;
	switch (id) {
	case AML_DVFS_ID_VCCK:
		dcdc = 4;
		break;

	case AML_DVFS_ID_VDDEE:
		dcdc = 2;
		break;

	case AML_DVFS_ID_DDR:
		dcdc = 3;
		break;

	default:
		break;
	}
	return dcdc;
}

static int aml1218_set_voltage(uint32_t id, uint32_t min_uV, uint32_t max_uV)
{
	int dcdc = aml_1218_convert_id_to_dcdc(id);
	uint32_t vol = 0;

	if (min_uV > max_uV)
		return -1;
	vol = (min_uV + max_uV) / 2;
	if (dcdc >= 1 && dcdc <= 4)
		return aml1218_set_dcdc_voltage(dcdc, vol);
	return -EINVAL;
}

static int aml1218_get_voltage(uint32_t id, uint32_t *uV)
{
	int dcdc = aml_1218_convert_id_to_dcdc(id);

	if (dcdc >= 1 && dcdc <= 4)
		return aml1218_get_dcdc_voltage(dcdc, uV);
	return -EINVAL;
}

struct aml_dvfs_driver aml1218_dvfs_driver = {
	.name	     = "aml1218-dvfs",
	.id_mask     = (AML_DVFS_ID_VCCK | AML_DVFS_ID_VDDEE | AML_DVFS_ID_DDR),
	.set_voltage = aml1218_set_voltage,
	.get_voltage = aml1218_get_voltage,
};
#endif

static int aml_pmu_check_device(struct i2c_client *client)
{
	int ret;
	uint8_t buf[2] = {};
	struct i2c_msg msg[] = {
		{
			.addr  = client->addr & 0xff,
			.flags = 0,
			.len   = sizeof(buf),
			.buf   = buf,
		},
		{
			.addr  = client->addr & 0xff,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = &buf[1],
		}
	};

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		DBG("%s: i2c transfer for %x failed, ret:%d\n",
		    __func__, client->addr, ret);
		return ret;
	}
	return 0;
}

static int aml_pmu_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
#ifdef CONFIG_OF
	char   *sub_type = NULL;
	struct  platform_device *pdev;
	struct  i2c_device_id *type = NULL;
	int	ret;

	if (aml_pmu_check_device(client))
		return -ENODEV;
#if DEBUG_TREE
	scan_node_tree(client->dev.of_node, 0);
#endif
	init_data = kzalloc(sizeof(*init_data), GFP_KERNEL);
	if (!init_data) {
		DBG("%s, allocate initialize data failed\n", __func__);
		return -ENOMEM;
	}
	setup_platform_pmu_init_data(client->dev.of_node, init_data);
	P_STR_PROP(client->dev.of_node, "sub_type", sub_type, 0, out_free_chip);
	type = find_id_by_name(aml_pmu_id_table, sub_type);
	if (!type) { /* sub type is not supported */
		DBG("sub_type of '%s' is not match, abort\n", sub_type);
		goto out_free_chip;
	}

#ifdef CONFIG_AML1212
	if (type->driver_data == 0) {
		g_aml1212_client = client;
		aml_pmu_register_driver(&aml1212_driver);
#ifdef CONFIG_AMLOGIC_USB
		aml1212_otg_nb.notifier_call = aml1212_otg_change;
		aml1212_usb_nb.notifier_call = aml1212_usb_charger;
		dwc_otg_power_register_notifier(&aml1212_otg_nb);
		dwc_otg_charger_detect_register_notifier(&aml1212_usb_nb);
#endif
	}
#endif
#ifdef CONFIG_AML1216
	if (type->driver_data == 1) {
		g_aml1216_client = client;
#if defined(CONFIG_AML_DVFS) && defined(CONFIG_AML1216)
		aml_dvfs_register_driver(&aml1216_dvfs_driver);
#endif
#ifdef CONFIG_AMLOGIC_USB
		aml1216_otg_nb.notifier_call = aml1216_otg_change;
		aml1216_usb_nb.notifier_call = aml1216_usb_charger;
		dwc_otg_power_register_notifier(&aml1216_otg_nb);
		dwc_otg_charger_detect_register_notifier(&aml1216_usb_nb);
#endif
		aml_pmu_register_driver(&aml1216_pmu_driver);
	}
#endif
#ifdef CONFIG_AML1218
	if (type->driver_data == 2) {
		g_aml1218_client = client;
#if defined(CONFIG_AML_DVFS) && defined(CONFIG_AML1218)
		aml_dvfs_register_driver(&aml1218_dvfs_driver);
#endif
#ifdef CONFIG_AMLOGIC_USB
		aml1218_otg_nb.notifier_call = aml1218_otg_change;
		aml1218_usb_nb.notifier_call = aml1218_usb_charger;
		dwc_otg_power_register_notifier(&aml1218_otg_nb);
		dwc_otg_charger_detect_register_notifier(&aml1218_usb_nb);
#endif
		aml_pmu_register_driver(&aml1218_pmu_driver);
	}
#endif
	pdev = platform_device_alloc(sub_type, 0);
	if (pdev == NULL) {
		pr_info(">> %s, allocate platform device failed\n", __func__);
		return -ENOMEM;
	}
	init_data->board_battery->pmu_irq_id = client->irq;
	pdev->dev.parent	= &client->dev;
	pdev->dev.platform_data =  init_data;
	pdev->dev.of_node	=  client->dev.of_node;
	ret = platform_device_add(pdev);
	if (ret) {
		pr_info(">> %s, add platform device failed\n", __func__);
		platform_device_del(pdev);
		return -EINVAL;
	}
	i2c_set_clientdata(client, pdev);

out_free_chip:
#endif  /* CONFIG_OF */
	return 0;
}

static int aml_pmu_remove(struct i2c_client *client)
{
	struct platform_device *pdev = i2c_get_clientdata(client);

#ifdef CONFIG_AML1212
	g_aml1212_client = NULL;
	aml_pmu_clear_driver();
#ifdef CONFIG_AMLOGIC_USB
	dwc_otg_power_unregister_notifier(&aml1212_otg_nb);
	dwc_otg_charger_detect_unregister_notifier(&aml1212_usb_nb);
#endif
#endif
#ifdef CONFIG_AML1216
	g_aml1216_client = NULL;
#if defined(CONFIG_AML_DVFS) && defined(CONFIG_AML1216)
	aml_dvfs_unregister_driver(&aml1216_dvfs_driver);
#endif
	aml_pmu_clear_driver();
#ifdef CONFIG_AMLOGIC_USB
	dwc_otg_power_unregister_notifier(&aml1216_otg_nb);
	dwc_otg_charger_detect_unregister_notifier(&aml1216_usb_nb);
#endif
#endif
#ifdef CONFIG_AML1218
	g_aml1218_client = NULL;
#if defined(CONFIG_AML_DVFS) && defined(CONFIG_AML1218)
	aml_dvfs_unregister_driver(&aml1218_dvfs_driver);
#endif
	aml_pmu_clear_driver();
#ifdef CONFIG_AMLOGIC_USB
	dwc_otg_power_unregister_notifier(&aml1218_otg_nb);
	dwc_otg_charger_detect_unregister_notifier(&aml1218_usb_nb);
#endif
#endif
	platform_device_del(pdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_pmu_match_id = {
	.compatible = "amlogic, amlogic_pmu",
};
#endif

static struct i2c_driver aml_pmu_driver = {
	.driver	= {
		.name	= "amlogic_pmu",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = &amlogic_pmu_match_id,
#endif
	},
	.probe		= aml_pmu_probe,
	.remove		= aml_pmu_remove,
	.id_table	= aml_pmu_id_table,
};

static int __init aml_pmu_init(void)
{
	pr_info("%s, %d\n", __func__, __LINE__);
	return i2c_add_driver(&aml_pmu_driver);
}
arch_initcall(aml_pmu_init);

static void __exit aml_pmu_exit(void)
{
	i2c_del_driver(&aml_pmu_driver);
}
module_exit(aml_pmu_exit);

MODULE_DESCRIPTION("Amlogic PMU device driver");
MODULE_AUTHOR("tao.zeng@amlogic.com");
MODULE_LICENSE("GPL");
