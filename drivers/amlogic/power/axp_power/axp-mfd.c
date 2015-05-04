/*
 * drivers/amlogic/power/axp_power/axp-mfd.c
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
#include <linux/regulator/machine.h>
#include <linux/amlogic/usbtype.h>
#include <linux/of.h>
#include <asm/setup.h>

#include "axp20-mfd.h"

#include <linux/amlogic/battery_parameter.h>
#ifdef CONFIG_OF

#ifdef CONFIG_AMLOGIC_USB
static struct notifier_block axp20_otg_nb;
static struct notifier_block axp20_usb_nb;
#endif

/*
 * Move these configs from bsp to this file.
 * Most of these configs are fixed and no need to change.
 * So this can save memory size for dtb
 */
/* Reverse engineered partly from Platformx drivers */
enum axp_regls {
	vcc_ldo1 = 0,
	vcc_ldo2,
	vcc_ldo3,
	vcc_ldo4,
	vcc_ldo5,

	vcc_buck2,
	vcc_buck3,
	vcc_ldoio0,
};

/* The values of the various regulator constraints are obviously dependent
 * on exactly what is wired to each ldo.  Unfortunately this information is
 * not generally available.  More information has been requested from Xbow
 * but as of yet they haven't been forthcoming.
 *
 * Some of these are clearly Stargate 2 related (no way of plugging
 * in an lcd on the IM2 for example!).
 */
static struct regulator_consumer_supply ldo1_data[] = {
	{
		.supply = "VDD_RTC",
	},
};


static struct regulator_consumer_supply ldo2_data[] = {
	{
		.supply = "VDDIO_AO",
	},
};

static struct regulator_consumer_supply ldo3_data[] = {
	{
		.supply = "AVDD2.5V",
	},
};

static struct regulator_consumer_supply ldo4_data[] = {
	{
		.supply = "AVDD3.0V",
	},
};

static struct regulator_consumer_supply ldoio0_data[] = {
	{
		.supply = "POWER_MISC",
	},
};


static struct regulator_consumer_supply buck2_data[] = {
	{
		.supply = "DDR3_1.5V",
	},
};

static struct regulator_consumer_supply buck3_data[] = {
	{
		.supply = "VDD_AO",
	},
};

static struct regulator_init_data axp_regl_init_data[] = {
	[vcc_ldo1] = {
		.constraints = { /* board default 1.25V */
			.name = "axp20_ldo1",
			.min_uV =  1300 * 1000,
			.max_uV =  1300 * 1000,
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo1_data),
		.consumer_supplies = ldo1_data,
	},
	[vcc_ldo2] = {
		.constraints = { /* board default 3.0V */
			.name = "axp20_ldo2",
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				/* .uV = ldo2_vol * 1000, */
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo2_data),
		.consumer_supplies = ldo2_data,
	},
	[vcc_ldo3] = {
		.constraints = {/* default is 1.8V */
			.name = "axp20_ldo3",
			.min_uV =  700 * 1000,
			.max_uV =  3500 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				/* .uV = ldo3_vol * 1000, */
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo3_data),
		.consumer_supplies = ldo3_data,
	},
	[vcc_ldo4] = {
		.constraints = {
			/* board default is 3.3V */
			.name = "axp20_ldo4",
			.min_uV = 1250000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				/* .uV = ldo4_vol * 1000, */
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo4_data),
		.consumer_supplies = ldo4_data,
	},
	[vcc_buck2] = {
		.constraints = { /* default 1.5V */
			.name = "axp20_buck2",
			.min_uV = 700 * 1000,
			.max_uV = 2275 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = 1500 * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(buck2_data),
		.consumer_supplies = buck2_data,
	},
	[vcc_buck3] = {
		.constraints = { /* default 1.2V */
			.name = "axp20_buck3",
			.min_uV = 700 * 1000,
			.max_uV = 3500 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = 1200 * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(buck3_data),
		.consumer_supplies = buck3_data,
	},
	[vcc_ldoio0] = {
		.constraints = { /* default 2.5V */
			.name = "axp20_ldoio0",
			.min_uV = 1800 * 1000,
			.max_uV = 3300 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ldoio0_data),
		.consumer_supplies = ldoio0_data,
	},
};

static struct axp_funcdev_info axp_regldevs[] = {
	{
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO1,
		.platform_data = &axp_regl_init_data[vcc_ldo1],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO2,
		.platform_data = &axp_regl_init_data[vcc_ldo2],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO3,
		.platform_data = &axp_regl_init_data[vcc_ldo3],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO4,
		.platform_data = &axp_regl_init_data[vcc_ldo4],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_BUCK2,
		.platform_data = &axp_regl_init_data[vcc_buck2],
	}, {
		.name = "axp20-regulator",
			.id = AXP20_ID_BUCK3,
			.platform_data = &axp_regl_init_data[vcc_buck3],
	}, {
		.name = "axp20-regulator",
			.id = AXP20_ID_LDOIO0,
			.platform_data = &axp_regl_init_data[vcc_ldoio0],
	},
};

static struct axp_supply_init_data axp_sply_init_data = {
	.soft_limit_to99 =  0,
	.para		 =  NULL,  /* para will pass to g24_pmu_call_back */
};

static struct axp_funcdev_info axp_splydev[] = {
	{
		.name = "axp20-supplyer",
		.id = AXP20_ID_SUPPLY,
		.platform_data = &axp_sply_init_data,
	},
};

static struct axp_funcdev_info axp_gpiodev[] = {
	{
		.name = "axp20-gpio",
		.id   = AXP20_ID_GPIO,
		.platform_data = NULL,
	},
};

static struct axp_platform_data axp_pdata = {
	.num_regl_devs = ARRAY_SIZE(axp_regldevs),
	.num_sply_devs = ARRAY_SIZE(axp_splydev),
	.num_gpio_devs = ARRAY_SIZE(axp_gpiodev),
	.regl_devs = axp_regldevs,
	.sply_devs = axp_splydev,
	.gpio_devs = axp_gpiodev,
	.gpio_base = 0,
};
#endif			  /* CONFIG_OF */

static inline int is_ac_online(void)
{
	uint8_t val;
	axp_read(&axp->dev, 0x00, &val);
	if (val & ((1<<7) | (1<<5)))
		return 1;
	else
		return 0;
}

static void axp_mfd_irq_work(struct work_struct *work)
{
	struct axp_mfd_chip *chip;
	uint64_t irqs = 0;

	chip = container_of(work, struct axp_mfd_chip, irq_work);
	pr_info("[AXP]========================in irq=====================\n");

	while (1) {
		if (chip->ops->read_irqs(chip, &irqs))
			break;
		pr_info("%s->%d:irqs = 0x%x\n",
			 __func__, __LINE__, (int) irqs);
		irqs &= chip->irqs_enabled;
		pr_info("%s->%d: chip->irqs_enabled = 0x%x\n",
			 __func__, __LINE__, (int) chip->irqs_enabled);

		if (irqs == 0)
			break;
		pr_info("%s->%d:irqs = 0x%x\n",
			 __func__, __LINE__, (int) irqs);

		blocking_notifier_call_chain(&chip->notifier_list, irqs, NULL);
	}
	enable_irq(chip->client->irq);
}

#if 0
static irqreturn_t axp_mfd_irq_handler(int irq, void *data)
{
	struct axp_mfd_chip *chip = data;
	disable_irq_nosync(irq);
	(void)schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}
#endif

static struct axp_mfd_chip_ops axp_mfd_ops[] = {
	[0] = {
	},
	[1] = {
	},
	[2] = {
		.init_chip    = axp20_init_chip,
		.enable_irqs  = axp20_enable_irqs,
		.disable_irqs = axp20_disable_irqs,
		.read_irqs    = axp20_read_irqs,
	},
};

static const struct i2c_device_id axp_mfd_id_table[] = {
	{ "axp18_mfd", 0 },
	{ "axp19_mfd", 1 },
	{ "axp20_mfd", 2 },
	{},
};
MODULE_DEVICE_TABLE(i2c, axp_mfd_id_table);

int axp_mfd_create_attrs(struct axp_mfd_chip *chip)
{
	int j, r;
	if (chip->type ==  AXP20) {
		for (j = 0; j < ARRAY_SIZE(axp20_mfd_attrs); j++) {
			r = device_create_file(chip->dev, &axp20_mfd_attrs[j]);
			if (r)
				goto sysfs_failed3;
		}
	} else
		r = 0;
	goto succeed;

sysfs_failed3:
	while (j--)
		device_remove_file(chip->dev, &axp20_mfd_attrs[j]);
succeed:
	return r;
}

static int __remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int axp_mfd_remove_subdevs(struct axp_mfd_chip *chip)
{
	return device_for_each_child(chip->dev, NULL, __remove_subdev);
}

static int  axp_mfd_add_subdevs(struct axp_mfd_chip *chip,
		struct axp_platform_data *pdata)
{
	struct axp_funcdev_info *regl_dev;
	struct axp_funcdev_info *sply_dev;
	struct axp_funcdev_info *gpio_dev;
	struct platform_device *pdev;
	int i, ret = 0;

	/* register for regultors */
	for (i = 0; i < pdata->num_regl_devs; i++) {
		regl_dev = &pdata->regl_devs[i];
		pdev = platform_device_alloc(regl_dev->name, regl_dev->id);
		pdev->dev.parent = chip->dev;
		pdev->dev.platform_data = regl_dev->platform_data;
		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}

	/* register for power supply */
	for (i = 0; i < pdata->num_sply_devs; i++) {
		sply_dev = &pdata->sply_devs[i];
		pdev = platform_device_alloc(sply_dev->name, sply_dev->id);
		pdev->dev.parent = chip->dev;
		pdev->dev.platform_data = sply_dev->platform_data;
		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}

	/* register for gpio */
	for (i = 0; i < pdata->num_gpio_devs; i++) {
		gpio_dev = &pdata->gpio_devs[i];
		pdev = platform_device_alloc(gpio_dev->name, gpio_dev->id);
		pdev->dev.parent = chip->dev;
		pdev->dev.platform_data = gpio_dev->platform_data;
		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}

	return 0;

failed:
	axp_mfd_remove_subdevs(chip);
	return ret;
}

void axp_power_off(void)
{
	pr_info("[axp] send power-off command!\n");
	mdelay(20);
	axp_set_bits(&axp->dev, POWER20_OFF_CTL, 0x80);
	mdelay(20);
	while (1) {
		pr_info("[axp] warning!!! %s, maybe some error happend!\n",
			 "axp can't power-off");
	}
}
EXPORT_SYMBOL_GPL(axp_power_off);

#ifdef CONFIG_OF
#define DEBUG_TREE	0
#define DEBUG_PARSE	0
#define DBG(format, args...) pr_info("[AXP]%s, "format, __func__, ##args)

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
		DBG("get property:%25s, value:0x%08x, dec:%8d\n",\
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
void scan_node_tree(struct device_node *top_node, int off)
{
	struct device_node *child;
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

int setup_supply_data(struct device_node *node, struct axp_supply_init_data *s)
{
	struct device_node *b_node;
	struct battery_parameter *battery;
	phandle fhandle;

	P_U32_PROP(node, "soft_limit_to99", s->soft_limit_to99, f);
	P_U32_PROP(node, "board_battery",   fhandle, f);
	b_node = of_find_node_by_phandle(fhandle);
	if (!b_node)
		DBG("find battery node failed, current:%s\n", node->name);
	else {
		ALLOC_DEVICES(battery, sizeof(*battery), GFP_KERNEL);
		if (parse_battery_parameters(b_node, battery)) {
			DBG("failed to parse battery parameter, node:%s\n",
			    b_node->name);
			kfree(battery);
		} else
			s->board_battery = battery;
	}
	return 0;

f:
	return -EINVAL;
}

int setup_platform_pmu_init_data(struct device_node *node,
				 struct axp_platform_data *pdata)
{
	struct axp_supply_init_data *s;
	struct regulator_init_data  *buck2 = NULL, *buck3 = NULL;
	uint32_t tmp, i;
	void *temp;

	s = (struct axp_supply_init_data *)pdata->sply_devs[0].platform_data;
	for (i = 0; i < pdata->num_regl_devs; i++) {
		if (pdata->regl_devs[i].id == AXP20_ID_BUCK2) {
			temp = pdata->regl_devs[i].platform_data;
			buck2 = (struct regulator_init_data *)temp;
		}
		if (pdata->regl_devs[i].id == AXP20_ID_BUCK3) {
			temp = pdata->regl_devs[i].platform_data;
			buck3 = (struct regulator_init_data *)temp;
		}
	}
	if (setup_supply_data(node, s))
		return  -EINVAL;
	/*
	 * if there are not assigned propertys of dc2/dc3 voltage,
	 * just leave them to default value.
	 */
	if (buck2) {
		P_U32_PROP(node, "ddr_voltage", tmp, setup1);
		buck2->constraints.state_standby.uV = tmp;
	}
setup1:
	if (buck3) {
		P_U32_PROP(node, "vddao_voltage", tmp, setup2);
		buck3->constraints.state_standby.uV = tmp;
	}
setup2:

	return 0;
}

static struct i2c_device_id *find_id_by_name(const struct i2c_device_id *l,
					     char *name)
{
	while (l->name && l->name[0]) {
		if (!strcmp(l->name, name))
			return (struct i2c_device_id *)l;
		l++;
	}
	return NULL;
}
#endif

static int  axp_mfd_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
#ifdef CONFIG_OF
	struct i2c_device_id *type;
	char   *sub_type = NULL;
#else
	struct axp_platform_data *pdata = client->dev.platform_data;
#endif
	struct axp_mfd_chip *chip;
	int ret = 0;

	chip = kzalloc(sizeof(struct axp_mfd_chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;
#ifdef CONFIG_OF
#if DEBUG_TREE
	scan_node_tree(client->dev.of_node, 0);
#endif
	setup_platform_pmu_init_data(client->dev.of_node, &axp_pdata);
	P_STR_PROP(client->dev.of_node, "sub_type", sub_type, out_free_chip);
	type = find_id_by_name(axp_mfd_id_table, sub_type);
	if (!type) { /* sub type is not supported */
		DBG("sub_type of '%s' is not match, abort\n", sub_type);
		goto out_free_chip;
	}
#endif
#ifdef CONFIG_AW_AXP20
	if (type->driver_data == 2) {
#ifdef CONFIG_AMLOGIC_USB
		axp20_otg_nb.notifier_call = axp202_otg_change;
		axp20_usb_nb.notifier_call = axp202_usb_charger;
		dwc_otg_power_register_notifier(&axp20_otg_nb);
		dwc_otg_charger_detect_register_notifier(&axp20_usb_nb);
#endif
	}
#endif
	axp = client;

	chip->client = client;
	chip->dev = &client->dev;
#ifdef CONFIG_OF
	chip->ops = &axp_mfd_ops[type->driver_data];
#else
	chip->ops = &axp_mfd_ops[id->driver_data];
#endif

	mutex_init(&chip->lock);
	INIT_WORK(&chip->irq_work, axp_mfd_irq_work);
	BLOCKING_INIT_NOTIFIER_HEAD(&chip->notifier_list);

	i2c_set_clientdata(client, chip);

	ret = chip->ops->init_chip(chip);
	if (ret)
		goto out_free_chip;

#ifdef CONFIG_OF
	ret = axp_mfd_add_subdevs(chip, &axp_pdata);
#else
	ret = axp_mfd_add_subdevs(chip, pdata);
#endif
	if (ret)
		goto out_free_irq;

	/* PM hookup */
	if (!pm_power_off)
		pm_power_off = axp_power_off;

	ret = axp_mfd_create_attrs(chip);
	if (ret)
		return ret;

	return 0;

out_free_irq:
	free_irq(client->irq, chip);

out_free_chip:
	i2c_set_clientdata(client, NULL);
	kfree(chip);

	return ret;
}

static int  axp_mfd_remove(struct i2c_client *client)
{
	struct axp_mfd_chip *chip = i2c_get_clientdata(client);

	pm_power_off = NULL;
	axp = NULL;

	axp_mfd_remove_subdevs(chip);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_axp_match_id = {
	.compatible = "amlogic, axp_mfd",
};
#endif

static struct i2c_driver axp_mfd_driver = {
	.driver	= {
		.name	= "axp_mfd",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = &aml_axp_match_id,
#endif
	},
	.probe	  = axp_mfd_probe,
	.remove	  = axp_mfd_remove,
	.id_table = axp_mfd_id_table,
};

static int __init axp_mfd_init(void)
{
	return i2c_add_driver(&axp_mfd_driver);
}
arch_initcall(axp_mfd_init);

static void __exit axp_mfd_exit(void)
{
	i2c_del_driver(&axp_mfd_driver);
}
module_exit(axp_mfd_exit);

MODULE_DESCRIPTION("PMIC MFD Driver for AXP");
MODULE_AUTHOR("Donglu Zhang Krosspower");
MODULE_LICENSE("GPL");
