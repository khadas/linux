/*
 * drivers/amlogic/jtag/jtag_setup.c
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

#define pr_fmt(fmt)	"jtag: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/string.h>

#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/mmc_notify.h>
#include <linux/amlogic/jtag.h>
#include "jtag.h"

#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#else
#include <asm/psci.h>
#endif


#define AML_JTAG_NAME		"jtag"

static int jtag_select = JTAG_DISABLE;
static int jtag_cluster = CLUSTER_DISABLE;

bool is_jtag_disable(void)
{
	if (jtag_select == JTAG_DISABLE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_disable);

bool is_jtag_apao(void)
{
	if (jtag_select == JTAG_A53_AO)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_apao);

bool is_jtag_apee(void)
{
	if (jtag_select == JTAG_A53_EE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_apee);

static void aml_jtag_option_parse(struct aml_jtag_dev *jdev, const char *s)
{
	char *cluster;
	unsigned long value;
	int ret;

	if (!strncmp(s, "disable", 7)) {
		jdev->select = JTAG_DISABLE;
		jtag_select = jdev->select;
	} else if (!strncmp(s, "apao", 4)) {
		jdev->select = JTAG_A53_AO;
		jtag_select = jdev->select;
	} else if (!strncmp(s, "apee", 4)) {
		jdev->select = JTAG_A53_EE;
		jtag_select = jdev->select;
	} else {
		jdev->select = JTAG_DISABLE;
		jtag_select = jdev->select;
	}

	pr_info("jtag select %d\n", jdev->select);

	cluster = strchr(s, ',');
	if (cluster != NULL) {
		cluster++;
		ret = kstrtoul(cluster, 10, &value);
		if (ret) {
			pr_err("invalid cluster index %d\n", jdev->cluster);
			jdev->cluster = CLUSTER_DISABLE;
		} else {
			jdev->cluster = value;
		}
		jtag_cluster = jdev->cluster;

		pr_info("cluster index %d\n", jdev->cluster);
	} else {
		jdev->cluster = CLUSTER_DISABLE;
		jtag_cluster = jdev->cluster;
	}
}

static int __init setup_jtag(char *p)
{
	char *cluster;
	unsigned long value;
	int ret;

	if (!strncmp(p, "disable", 7))
		jtag_select = JTAG_DISABLE;
	else if (!strncmp(p, "apao", 4))
		jtag_select = JTAG_A53_AO;
	else if (!strncmp(p, "apee", 4))
		jtag_select = JTAG_A53_EE;
	else
		jtag_select = JTAG_DISABLE;

	pr_info("jtag select %d\n", jtag_select);

	cluster = strchr(p, ',');
	if (cluster != NULL) {
		cluster++;
		ret = kstrtoul(cluster, 10, &value);
		if (ret) {
			pr_err("invalid cluster index %d\n", jtag_cluster);
			jtag_cluster = CLUSTER_DISABLE;
		} else {
			jtag_cluster = value;
		}
		pr_info("cluster index %d\n", jtag_cluster);
	}

	return 0;
}

/*
 * jtag=[apao|apee]
 * jtag=[apao|apee]{,[0|1]}
 *
 * [apao|apee]: jtag domain
 * [0|1]: cluster index
 */
__setup("jtag=", setup_jtag);


/*
 * clean other pinmux except jtag register.
 * jtag register will be setup by aml_set_jtag_state().
 *
 *@return: 0 success, other failed
 *
 */
static int aml_jtag_pinmux_apao(struct aml_jtag_dev *jdev)
{
	struct pinctrl *p;

	p = devm_pinctrl_get_select(&jdev->pdev->dev, "jtag_apao_pins");
	if (IS_ERR(p)) {
		pr_err("failed to get select pins\n");
		return -1;
	}
	return 0;
}

static int aml_jtag_pinmux_apee(struct aml_jtag_dev *jdev)
{
	/* @todo jtag ee pinmux are setup in sd module,
	 *       need to do here.
	 */
	return 0;
}

static void aml_jtag_pinctrl(struct aml_jtag_dev *jdev)
{
	switch (jdev->select) {
	case JTAG_A53_AO:
		aml_jtag_pinmux_apao(jdev);
		pr_info("pinctrl setup apao\n");
		break;
	case JTAG_A53_EE:
		aml_jtag_pinmux_apee(jdev);
		pr_info("pinctrl setup apee\n");
		break;
	default:
		break;
	}
}


/*
 * setup jtag on/off, and setup ao/ee jtag
 *
 * @state: must be JTAG_STATE_ON/JTAG_STATE_OFF
 * @select: mest be JTAG_DISABLE/JTAG_A53_AO/JTAG_A53_EE
 */
#ifdef CONFIG_ARM64
void aml_set_jtag_state(unsigned state, unsigned select)
{
	uint64_t command;
	if (state == JTAG_STATE_ON)
		command = JTAG_ON;
	else
		command = JTAG_OFF;
	asm __volatile__("" : : : "memory");

	__invoke_psci_fn_smc(command, select, 0, 0);
}
#else
void aml_set_jtag_state(unsigned state, unsigned select)
{
	unsigned command;
	register unsigned r0 asm("r0");
	register unsigned r1 asm("r1");

	if (state == JTAG_STATE_ON)
		command = JTAG_ON;
	else
		command = JTAG_OFF;

	asm __volatile__("" : : : "memory");

	r0 = select;
	r1 = command;

	asm volatile(
		__asmeq("%0", "r1")
		__asmeq("%1", "r0")
		__SMC(0)
		: : "r" (r1), "r"(r0));
}
#endif



void aml_jtag_select(struct aml_jtag_dev *jdev)
{
	unsigned int sel = jdev->select;

	if (jdev->cluster != CLUSTER_DISABLE)
		sel |= jdev->cluster << CLUSTER_BIT;

	pr_info("set state %u\n", sel);

	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(JTAG_STATE_ON, sel);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}


static void aml_jtag_setup(struct aml_jtag_dev *jdev)
{
	aml_jtag_pinctrl(jdev);
	aml_jtag_select(jdev);
}

static ssize_t jtag_select_show(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	unsigned int len = 0;
	len += sprintf(buf + len, "usage:\n");
	len += sprintf(buf + len, "    echo [apao|apee] > select\n");
	len += sprintf(buf + len, "    echo [apao|apee]{,[0|1]} > select\n");
	return len;
}


static ssize_t jtag_select_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{
	struct aml_jtag_dev *jdev;

	jdev = container_of(cls, struct aml_jtag_dev, cls);
	aml_jtag_option_parse(jdev, buffer);
	aml_jtag_setup(jdev);

	return count;
}

static struct class_attribute aml_jtag_attrs[] = {
	__ATTR(select,  0644, jtag_select_show, jtag_select_store),
	__ATTR_NULL,
};


static int jtag_notify_callback(struct notifier_block *block,
			unsigned long event, void *data)
{
	struct aml_jtag_dev *jdev;

	jdev = container_of(block, struct aml_jtag_dev, notifier);


	/* @todo need mmc driver to implement notify calling */
	pr_info("%s %lu\n", __func__, event);
	switch (event) {
	case MMC_EVENT_JTAG_IN:
		jdev->select = JTAG_A53_EE;
		jtag_select = jdev->select;
		break;

	case MMC_EVENT_JTAG_OUT:
		jdev->select = JTAG_A53_AO;
		jtag_select = jdev->select;
		break;

	default:
		return NOTIFY_DONE;
	}

	aml_jtag_setup(jdev);

	return NOTIFY_OK;
}


static int aml_jtag_probe(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev;
	int ret;

	/* kzalloc device */
	jdev = kzalloc(sizeof(struct aml_jtag_dev), GFP_KERNEL);
	if (!jdev) {
		pr_err("kzalloc error\n");
		return -ENOMEM;
	}

	/* create class attributes */
	jdev->cls.name = AML_JTAG_NAME;
	jdev->cls.owner = THIS_MODULE;
	jdev->cls.class_attrs = aml_jtag_attrs;
	class_register(&jdev->cls);

	/* set driver data */
	jdev->pdev = pdev;
	platform_set_drvdata(pdev, jdev);

	/* register mmc notify */
	jdev->notifier.notifier_call = jtag_notify_callback;
	ret = mmc_register_client(&jdev->notifier);

	/* setup jtag */
	jdev->select = jtag_select;
	jdev->cluster = jtag_cluster;
	aml_jtag_setup(jdev);

	pr_info("module probed ok\n");
	return 0;
}


static int __exit aml_jtag_remove(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);

	class_unregister(&jdev->cls);
	platform_set_drvdata(pdev, NULL);
	kfree(jdev);

	pr_info("module removed ok\n");
	return 0;
}


static const struct of_device_id aml_jtag_dt_match[] = {
	{
		.compatible = "amlogic, jtag",
	},
	{},
};


static struct platform_driver aml_jtag_driver = {
	.driver = {
		.name = AML_JTAG_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_jtag_dt_match,
	},
	.probe = aml_jtag_probe,
	.remove = __exit_p(aml_jtag_remove),
};

static int __init aml_jtag_init(void)
{
	pr_info("module init\n");
	if (platform_driver_register(&aml_jtag_driver)) {
		pr_err("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}


static void __exit aml_jtag_exit(void)
{
	pr_info("module exit\n");
	platform_driver_unregister(&aml_jtag_driver);
}


module_init(aml_jtag_init);
module_exit(aml_jtag_exit);

MODULE_DESCRIPTION("Meson JTAG Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

