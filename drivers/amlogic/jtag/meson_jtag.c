// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"jtag: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/arm-smccc.h>

#include <linux/amlogic/jtag.h>
#include <linux/arm-smccc.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#ifdef CONFIG_MACH_MESON8B
#include <linux/amlogic/meson-secure.h>
#endif

#include "meson_jtag.h"

#define AML_JTAG_NAME		"jtag"
#define JTAG_IRQ_CMD		0xFFF

/* store the jtag select globaly */
static int global_select = AMLOGIC_JTAG_DISABLE;

/* whether the jtag select is setup by the boot param
 * jtag select is setup by the boot prior to device tree.
 */
static bool jtag_select_setup;

/* store the params that are setup by the boot param */
static int jtag_select = AMLOGIC_JTAG_DISABLE;

/* cluster default is 0 */
static int jtag_cluster;

static struct jtag_id_desc jtag_id_data[] = {
	{JTAG_SELECT_ID(AP,   JTAG_A,  0), "ap", "jtag_a", "apao"},
	{JTAG_SELECT_ID(AP,   JTAG_B,  0), "ap", "jtag_b", "apee"},
	{JTAG_SELECT_ID(AP,   SWD_A,   0), "ap",  "swd_a", "swd_apao"},
};

bool is_jtag_disable(void)
{
	if (global_select == AMLOGIC_JTAG_DISABLE)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_disable);

bool is_jtag_a(void)
{
	if (global_select == JTAG_SELECT_ID(AP, JTAG_A, 0))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_a);

bool is_jtag_b(void)
{
	if (global_select == JTAG_SELECT_ID(AP, JTAG_B, 0))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_jtag_b);

static int jtag_id_find(const char *core_type, const char *jtag_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(jtag_id_data); i++) {
		if (!jtag_id_data[i].core_type ||
		    !jtag_id_data[i].jtag_type)
			continue;
		if ((strcmp(core_type, jtag_id_data[i].core_type) == 0) &&
		    (strcmp(jtag_type, jtag_id_data[i].jtag_type) == 0)) {
			return jtag_id_data[i].id;
		}
	}

	return AMLOGIC_JTAG_DISABLE;
}

static int jtag_id_find_by_alias(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(jtag_id_data); i++) {
		if (!jtag_id_data[i].alias)
			continue;
		if (strcmp(name, jtag_id_data[i].alias) == 0)
			return jtag_id_data[i].id;
	}

	return AMLOGIC_JTAG_DISABLE;
}

static struct jtag_id_desc *select_to_name(int select)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(jtag_id_data); i++) {
		if (select == jtag_id_data[i].id)
			return &jtag_id_data[i];
	}

	return NULL;
}

static void aml_jtag_option_parse(const char *s,
				  int *jtag_select, int *jtag_cluster)
{
	char *argv;
	int argc = 0;
	void *parameter[MAX_PARAM_NUM];
	int ret;
	unsigned long cluster = 0;
	int jtag_id = AMLOGIC_JTAG_DISABLE;

	if (strcmp(s, "disable") == 0) {
		*jtag_select = jtag_id;
		*jtag_cluster = (int)cluster;
		return;
	}

	do {
		argv = strsep((char **)&s, ",");
		if (!argv)
			break;
		pr_debug("param[%d] = %s\n", argc, argv);
		parameter[argc] = argv;
		argc++;
	} while (argc < MAX_PARAM_NUM);

	switch (argc) {
	case 1:
		jtag_id = jtag_id_find_by_alias(parameter[0]);
		break;
	case 2:
		if (isdigit(*(char *)parameter[1])) {
			jtag_id = jtag_id_find_by_alias(parameter[0]);
			ret = kstrtoul(parameter[1], 10,
				       (unsigned long *)&cluster);
			if (ret)
				cluster = 0;
		} else {
			jtag_id = jtag_id_find(parameter[0], parameter[1]);
		}
		break;
	case 3:
		jtag_id = jtag_id_find(parameter[0], parameter[1]);
		ret = kstrtoul(parameter[2], 10, (unsigned long *)&cluster);
		if (ret)
			cluster = 0;
		break;
	default:
		pr_info("invalid argument count!\n");
		*jtag_select = jtag_id;
		*jtag_cluster = (int)cluster;
		return;
	}
	if (jtag_id == AMLOGIC_JTAG_DISABLE)
		pr_info("parameter not support, disabled jtag.\n");
	*jtag_select = jtag_id;
	*jtag_cluster = (int)cluster;
}

static int setup_jtag(char *p)
{
	if (!p)
		return -EINVAL;

	jtag_select_setup = true;
	aml_jtag_option_parse(p, &jtag_select, &jtag_cluster);

	return 0;
}

/*
 * jtag=[ap,jtag_a|ap,jtag_b|ap,swd_a]
 * jtag=[ap,jtag_a|ap,jtag_b|ap,swd_a]{,[0|1]}
 * jtag=[apao|apee|swd_apao]
 * jtag=[apao|apee|swd_apao]{,[0|1]}
 *
 * [jtag_a|jtag_b]: jtag_type
 * [0|1]: cluster index
 */
static char *jtag_mode = "";

static int set_jtag_mode(const char *val, const struct kernel_param *kp)
{
	param_set_charp(val, kp);

	return setup_jtag(jtag_mode);
}

static const struct kernel_param_ops setup_jtag_ops = {
	.set = set_jtag_mode,
	.get = param_get_charp,
};

module_param_cb(jtag, &setup_jtag_ops, &jtag_mode, 0644);
MODULE_PARM_DESC(jtag, "jtag mode");


#ifdef CONFIG_MACH_MESON8B

static int aml_jtag_select_tee(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	u32 select = jdev->select;
	struct jtag_id_desc *tmp = NULL;

	pr_info("set state %u\n", select);
	set_cpus_allowed_ptr(current, cpumask_of(0));
	tmp = select_to_name(jdev->select);
	if (tmp)
		pr_info("meson8b select %s,%s, alias:%s\n",
			tmp->core_type, tmp->jtag_type, tmp->alias);
	else
		pr_info("meson8b select disable\n");
	switch (select) {
	case AMLOGIC_JTAG_DISABLE:
		meson_secure_jtag_disable();
		break;
	case JTAG_SELECT_ID(AP, JTAG_A, 0):
		meson_secure_jtag_apao();
		break;
	case JTAG_SELECT_ID(AP, JTAG_B, 0):
		meson_secure_jtag_apao();
		break;

	default:
		writel_relaxed(0x0, jdev->base);
		break;
	}
	set_cpus_allowed_ptr(current, cpu_all_mask);

	return 0;
}

static int aml_jtag_select_ree(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	unsigned int sel = jdev->select;
	u32 val;
	struct jtag_id_desc *tmp = NULL;

	jdev->base = of_iomap(np, 0);
	if (!jdev->base) {
		pr_err("failed to iomap regs");
		return -ENODEV;
	}
	tmp = select_to_name(jdev->select);
	if (tmp)
		pr_info("meson8b select %s,%s, alias:%s\n",
			tmp->core_type, tmp->jtag_type, tmp->alias);
	else
		pr_info("meson8b select disable\n");
	switch (sel) {
	case AMLOGIC_JTAG_DISABLE:
		writel_relaxed(0x0, jdev->base);
		break;
	case JTAG_SELECT_ID(AP, JTAG_A, 0):
		val = readl_relaxed(jdev->base);
		val &= ~0x3FF;
		val |= (2 << 0) | (1 << 8);
		writel_relaxed(val, jdev->base);
		break;
	case JTAG_SELECT_ID(AP, JTAG_B, 0):
		val = readl_relaxed(jdev->base);
		val &= ~0x3FF;
		val |= (2 << 4) | (2 << 8);
		writel_relaxed(val, jdev->base);
		break;
	default:
		writel_relaxed(0x0, jdev->base);
		break;
	}

	return 0;
}

static int aml_jtag_select(struct platform_device *pdev)
{
	if (meson_secure_enabled())
		aml_jtag_select_tee(pdev);
	else
		aml_jtag_select_ree(pdev);

	return 0;
}

#else

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
					  unsigned long arg0,
					  unsigned long arg1,
					  unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

/*
 * setup jtag on/off, and setup jtag-a/jtag-b
 *
 * @state: must be JTAG_STATE_ON/JTAG_STATE_OFF
 * @select: mest be JTAG_DISABLE/JTAG_A/JTAG_B
 */
void aml_set_jtag_state(unsigned int state, unsigned int select)
{
	u64 command;

	if (state == AMLOGIC_JTAG_STATE_ON)
		command = AMLOGIC_JTAG_ON;
	else
		command = AMLOGIC_JTAG_OFF;
	asm __volatile__("" : : : "memory");

	__invoke_psci_fn_smc(command, select, 0, 0);
}

static int aml_jtag_select(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	unsigned int select = jdev->select;
	unsigned int state = AMLOGIC_JTAG_STATE_OFF;
	struct jtag_id_desc *tmp = NULL;

	if (select != AMLOGIC_JTAG_DISABLE)
		state = AMLOGIC_JTAG_STATE_ON;

	if (jdev->cluster != 0)
		jdev->cluster = CLUSTER_TYPE_UPDATE(jdev->select,
						    jdev->cluster);
	tmp = select_to_name(jdev->select);
	if (tmp)
		pr_info("select %s,%s, alias:%s\n",
			tmp->core_type, tmp->jtag_type, tmp->alias);
	else
		pr_info("select disable\n");

	set_cpus_allowed_ptr(current, cpumask_of(0));
	aml_set_jtag_state(state, select);
	set_cpus_allowed_ptr(current, cpu_all_mask);

	return 0;
}

static void jtag_send_irq_to_bl31(void)
{
	asm __volatile__("" : : : "memory");
	__invoke_psci_fn_smc(AMLOGIC_JTAG_ON, JTAG_IRQ_CMD, 0, 0);
}

static irqreturn_t jtag_irq_handler(int irq, void *data)
{
	jtag_send_irq_to_bl31();
	return IRQ_HANDLED;
}

#endif

static int aml_jtag_setup(struct aml_jtag_dev *jdev)
{
	unsigned int old_select = jdev->old_select;
	unsigned int select = jdev->select;
	struct pinctrl_state *s;
	int ret;

	if (old_select == select)
		return 0;
	if (!jdev->jtag_pinctrl) {
		jdev->jtag_pinctrl = devm_pinctrl_get(&jdev->pdev->dev);
		if (IS_ERR_OR_NULL(jdev->jtag_pinctrl)) {
			dev_err(&jdev->pdev->dev, "could not get pinctrl handle\n");
			return -EINVAL;
		}
	}
	/* set pinmux */
	switch (select) {
	case JTAG_SELECT_ID(AP, JTAG_A, 0):
		s = pinctrl_lookup_state(jdev->jtag_pinctrl, "jtag_a_pins");
		if (IS_ERR_OR_NULL(s)) {
			dev_err(&jdev->pdev->dev,
				"could not get jtag_a_pins state\n");
			return -EINVAL;
		}
		ret = pinctrl_select_state(jdev->jtag_pinctrl, s);
		if (ret) {
			dev_err(&jdev->pdev->dev, "failed to set pinctrl\n");
			return -EINVAL;
		}
		break;
	case JTAG_SELECT_ID(AP, JTAG_B, 0):
		s = pinctrl_lookup_state(jdev->jtag_pinctrl, "jtag_b_pins");
		if (IS_ERR_OR_NULL(s)) {
			dev_err(&jdev->pdev->dev,
				"could not get jtag_b_pins state\n");
			return -EINVAL;
		}
		ret = pinctrl_select_state(jdev->jtag_pinctrl, s);
		if (ret) {
			dev_err(&jdev->pdev->dev, "failed to set pinctrl\n");
			return -EINVAL;
		}
		break;
	case JTAG_SELECT_ID(AP, SWD_A, 0):
		s = pinctrl_lookup_state(jdev->jtag_pinctrl,
					 "swd_a_pins");
		if (IS_ERR_OR_NULL(s)) {
			dev_err(&jdev->pdev->dev,
				"could not get swd_a_pins state\n");
			return -EINVAL;
		}
		ret = pinctrl_select_state(jdev->jtag_pinctrl, s);
		if (ret) {
			dev_err(&jdev->pdev->dev, "failed to set pinctrl\n");
			return -EINVAL;
		}
		break;
	default:
		if (old_select != AMLOGIC_JTAG_DISABLE) {
			devm_pinctrl_put(jdev->jtag_pinctrl);
			jdev->jtag_pinctrl = NULL;
		}
		break;
	}

	/* save to global */
	global_select = jdev->select;
	aml_jtag_select(jdev->pdev);
	jdev->old_select = select;

	return 0;
}

static ssize_t select_show(struct class *cls,
			   struct class_attribute *attr, char *buf)
{
	unsigned int len = 0;
	struct jtag_id_desc *tmp = NULL;

	tmp = select_to_name(global_select);
	if (tmp)
		len += sprintf(buf + len,
			"current select: %s,%s, alias:%s\n\n",
			tmp->core_type, tmp->jtag_type, tmp->alias);
	else
		len += sprintf(buf + len, "current select: disable\n\n");
	len += sprintf(buf + len, "usage:\n");
	len += sprintf(buf + len,
				   " echo <core_type>,<jtag_type>{,[0|1]} > select\n");
	len += sprintf(buf + len, " core_type:ap\n");
	len += sprintf(buf + len, " jtag_type:jtag_a|jtag_b|swd_a\n");

	return len;
}

static ssize_t select_store(struct class *cls,
			    struct class_attribute *attr,
			    const char *buffer, size_t count)
{
	struct aml_jtag_dev *jdev;
	int ret;
	char tmp[MAX_PARAM_LENGTH] = {0};

	jdev = container_of(cls, struct aml_jtag_dev, cls);
	count = min_t(size_t, MAX_PARAM_LENGTH, count);
	strncpy(tmp, buffer, count - 1);
	aml_jtag_option_parse(tmp, &jtag_select, &jtag_cluster);
	jdev->select = jtag_select;
	jdev->cluster = jtag_cluster;
	ret = aml_jtag_setup(jdev);
	if (ret < 0)
		return ret;

	return count;
}

static CLASS_ATTR_RW(select);

static struct attribute *aml_jtag_attrs[] = {
	&class_attr_select.attr,
	NULL
};

ATTRIBUTE_GROUPS(aml_jtag);

static int aml_jtag_dt_parse(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);
	const char *tmp;
	int ret;

	/* otherwise set select with dt */
	ret = of_property_read_string(np, "select", &tmp);
	if (ret < 0) {
		pr_err("select not configured\n");
		return -EINVAL;
	}
	aml_jtag_option_parse(tmp, &jdev->select, &jdev->cluster);

	return 0;
}

static int aml_jtag_probe(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev;
	int ret;
	int irq;

	/* kzalloc device */
	jdev = devm_kzalloc(&pdev->dev, sizeof(*jdev), GFP_KERNEL);
	if (!jdev)
		return -ENOMEM;

	/* set driver data */
	jdev->pdev = pdev;
	platform_set_drvdata(pdev, jdev);

	ret = aml_jtag_dt_parse(pdev);
	if (ret)
		return -EINVAL;
#ifndef CONFIG_MACH_MESON8B
	if (of_property_read_bool(pdev->dev.of_node, "interrupts")) {
		irq = platform_get_irq(pdev, 0);
		ret = devm_request_irq(&pdev->dev,
				       irq, jtag_irq_handler,
				       IRQ_TYPE_EDGE_RISING, "jtag", NULL);
		if (ret) {
			dev_err(&pdev->dev, "Requesting jtag irq failed!\n");
			return -EINVAL;
		}
	}
#endif
	jdev->old_select = AMLOGIC_JTAG_DISABLE;
	/* if jtag= param is setup, use select with jtag= param */
	if (jtag_select_setup) {
		jdev->select = jtag_select;
		jdev->cluster = jtag_cluster;
	}

	/* create class attributes */
	jdev->cls.name = AML_JTAG_NAME;
	jdev->cls.owner = THIS_MODULE;
	jdev->cls.class_groups = aml_jtag_groups;
	ret = class_register(&jdev->cls);
	if (ret) {
		pr_err("couldn't register sysfs class\n");
		return ret;
	}

	/* setup jtag */
	ret = aml_jtag_setup(jdev);
	if (ret < 0) {
		class_unregister(&jdev->cls);
		return ret;
	}

	return 0;
}

static int __exit aml_jtag_remove(struct platform_device *pdev)
{
	struct aml_jtag_dev *jdev = platform_get_drvdata(pdev);

	class_unregister(&jdev->cls);
	platform_set_drvdata(pdev, NULL);

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
	return platform_driver_register(&aml_jtag_driver);
}
/* Jtag will be setuped before device_initcall that most driver used.
 * But jtag should be after pinmux.
 * That means we must use some initcall between arch_initcall
 * and device_initcall.
 */
fs_initcall(aml_jtag_init);

static void __exit aml_jtag_exit(void)
{
	platform_driver_unregister(&aml_jtag_driver);
}
module_exit(aml_jtag_exit);

MODULE_DESCRIPTION("Meson JTAG Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
