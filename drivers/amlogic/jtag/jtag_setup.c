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

#include <linux/amlogic/jtag.h>



#define INFO(format, arg...) pr_info("jtag: " format, ##arg)
#define ERR(format,  arg...) pr_err("jtag: "  format, ##arg)


#define AML_JTAG_NAME		"jtag"


static struct class jtag_cls;
static int jtag_select = JTAG_DISABLE;

bool is_jtag_disable(void)
{
	if (jtag_select == JTAG_DISABLE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_disable);

int get_jtag_select(void)
{
	return jtag_select;
}
EXPORT_SYMBOL(get_jtag_select);


static int __init setup_jtag(char *p)
{
	INFO("%s\n", p);

	if (!strncmp(p, "disable", 7))
		jtag_select = JTAG_DISABLE;
	else if (!strncmp(p, "apao", 4))
		jtag_select = JTAG_A53_AO;
	else if (!strncmp(p, "apee", 4))
		jtag_select = JTAG_A53_EE;
	else
		jtag_select = JTAG_DISABLE;

	return 0;
}
__setup("jtag=", setup_jtag);


/*
 * clean other pinmux except jtag register.
 * jtag register will be setup by aml_set_jtag_state().
 *
 *@return: 0 success, other failed
 *
 */
static int aml_jtag_pinmux_apao(struct platform_device *pdev)
{
	struct pinctrl *p;

	p = devm_pinctrl_get_select(&pdev->dev, "jtag_apao_pins");
	if (IS_ERR(p)) {
		ERR("failed to get select pins\n");
		return -1;
	}
	return 0;
}


/*
 * setup jtag on/off, and setup ao/ee jtag
 *
 * @state: must be JTAG_STATE_ON/JTAG_STATE_OFF
 * @select: mest be JTAG_DISABLE/JTAG_A53_AO/JTAG_A53_EE
 */
void aml_set_jtag_state(unsigned state, unsigned select)
{
	uint64_t command;
	if (state == JTAG_STATE_ON)
		command = JTAG_ON;
	else
		command = JTAG_OFF;
	asm __volatile__("" : : : "memory");

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		"smc    #0\n"
		: : "r" (command), "r"(select));
}

void aml_jtag_disable(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(JTAG_STATE_OFF, JTAG_DISABLE);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}

void aml_jtag_select_apao(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(JTAG_STATE_ON, JTAG_A53_AO);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}

void aml_jtag_select_apee(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(JTAG_STATE_ON, JTAG_A53_EE);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}

static void aml_jtag_select(int select)
{
	switch (select) {
	case JTAG_A53_AO:
		aml_jtag_select_apao();
		INFO("select JTAG_A53_AO\n");
		break;
	case JTAG_A53_EE:
		aml_jtag_select_apee();
		INFO("select JTAG_A53_EE\n");
		break;
	default:
		aml_jtag_disable();
		INFO("select JTAG_DISABLE\n");
		break;
	}
}


static void aml_jtag_setup(struct platform_device *pdev)
{
	switch (jtag_select) {
	case JTAG_A53_AO:
		aml_jtag_pinmux_apao(pdev);
		aml_jtag_select_apao();
		INFO("setup apao\n");
		break;
	case JTAG_A53_EE:
		/*
		 * jtag ee pinmux are setup in sd module
		 */
		aml_jtag_select_apee();
		INFO("setup apee\n");
		break;
	default:
		aml_jtag_disable();
		INFO("disable\n");
		break;
	}
}

static ssize_t jtag_select_show(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	unsigned int len = 0;
	len += sprintf(buf + len, "usage:\n");
	len += sprintf(buf + len, "    echo <n> > select\n");
	len += sprintf(buf + len, "\n");
	len += sprintf(buf + len, "the value of <n> is:\n");
	len += sprintf(buf + len, "    0 - disable\n");
	len += sprintf(buf + len, "    1 - apao(A53 AO)\n");
	len += sprintf(buf + len, "    2 - apee(A53 EE)\n");
	return len;
}


static ssize_t jtag_select_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{
	long sel;

	if (kstrtol(buffer, 10, &sel)) {
		pr_err("unsupported value\n");
		return count;
	}

	pr_info("sel = %ld\n", sel);

	switch (sel) {
	case 1:
		jtag_select = JTAG_A53_AO;
		break;
	case 2:
		jtag_select = JTAG_A53_EE;
		break;
	default:
		jtag_select = JTAG_DISABLE;
		break;

	}

	pr_info("jtag_select = %d\n", jtag_select);

	aml_jtag_select(jtag_select);


	return count;
}

static struct class_attribute aml_jtag_attrs[] = {
	__ATTR(select,  0644, jtag_select_show, jtag_select_store),
	__ATTR_NULL,
};

static int aml_jtag_probe(struct platform_device *pdev)
{

	/* create class attributes */
	jtag_cls.name = AML_JTAG_NAME;
	jtag_cls.owner = THIS_MODULE;
	jtag_cls.class_attrs = aml_jtag_attrs;
	class_register(&jtag_cls);

	aml_jtag_setup(pdev);

	INFO("module probed ok\n");
	return 0;
}


static int __exit aml_jtag_remove(struct platform_device *pdev)
{
	class_unregister(&jtag_cls);
	INFO("module removed ok\n");
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
	INFO("module init\n");
	if (platform_driver_register(&aml_jtag_driver)) {
		ERR("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}


static void __exit aml_jtag_exit(void)
{
	INFO("module exit\n");
	platform_driver_unregister(&aml_jtag_driver);
}


module_init(aml_jtag_init);
module_exit(aml_jtag_exit);

MODULE_DESCRIPTION("Meson JTAG Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

