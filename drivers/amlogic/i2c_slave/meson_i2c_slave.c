// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include "meson_i2c_slave.h"

/*uboot i2c_auto_test: open/disable*/
static char i2c_auto_test_mode[10] = "disable";
static char i2c_auto_test_flag;

struct aml_i2c_slave *g_i2c;

static void i2c_slave_init(struct aml_i2c_slave *slave)
{
	slave->slave_regs->s_reg_ctrl = 0x0;
	slave->slave_regs->s_reg_ctrl |= (1 << 7);/*machine state en*/
	slave->slave_regs->s_reg_ctrl |= (1 << 25);/*IRQ_EN*/
	slave->slave_regs->s_reg_ctrl |= (1 << 24);/*ack en*/
	slave->slave_regs->s_reg_ctrl |= (1 << 28);/*send en*/
	slave->slave_regs->s_reg_ctrl |= (1 << 27);/*recv en*/
	slave->slave_regs->s_reg_ctrl |= (0x40 << 16);   /*slave addr*/
	slave->slave_regs->s_reg_ctrl |= (0x0 << 8); /*hold time*/
	slave->slave_regs->s_reg_ctrl |= (0x0 << 0); /*sampling rate*/
}

static ssize_t show_i2c_slave(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;

	struct aml_i2c_slave *i2c;

	i2c = g_i2c;

	if  (!strcmp(attr->attr.name, "addr")) {
		ret = sprintf(buf, "addr=0x%x\n",
			(i2c->slave_regs->s_reg_ctrl >> 16 & (0xff)) >> 1);
	} else if  (!strcmp(attr->attr.name, "control_reg")) {
		ret = sprintf(buf, "control_reg=0x%x\n",
			i2c->slave_regs->s_reg_ctrl & (0xffffffff));
	} else if  (!strcmp(attr->attr.name, "data")) {
		ret = sprintf(buf, "data=0x%x\n",
			i2c->slave_regs->s_rev_reg & (0xffffffff));
	} else  {
		pr_info("error attribute access\n");
		ret = 0;
	}

	return ret;
}

static ssize_t store_i2c_slave(struct class *class,
struct class_attribute *attr, const char *buf, size_t count)
{
	struct aml_i2c_slave *i2c = g_i2c;
	int ret;
	unsigned long val = 0;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	if (!strcmp(attr->attr.name, "addr")) {
		mutex_lock(i2c->lock);
		i2c->slave_regs->s_reg_ctrl &= ~(0xff << 16);
		i2c->slave_regs->s_reg_ctrl |= (val & 0xff) << 16;
		mutex_unlock(i2c->lock);
	} else if (!strcmp(attr->attr.name, "control_reg")) {
		mutex_lock(i2c->lock);
		i2c->slave_regs->s_reg_ctrl = (val & 0xffffffff);
		mutex_unlock(i2c->lock);
	} else if (!strcmp(attr->attr.name, "data")) {
		mutex_lock(i2c->lock);
		i2c->slave_regs->s_send_reg = val & 0xffffffff;
		mutex_unlock(i2c->lock);
	}
	return count;
}

static struct class_attribute slave_attrs[] = {
	__ATTR(addr, 0644,  show_i2c_slave, store_i2c_slave),
	__ATTR(control_reg, 0644,  show_i2c_slave,
						store_i2c_slave),
	__ATTR(data, 0644,  show_i2c_slave, store_i2c_slave),
	__ATTR_NULL
};

static struct attribute *slave_class_attrs[] = {
	&slave_attrs[0].attr,
	&slave_attrs[1].attr,
	&slave_attrs[2].attr,
	NULL
};

ATTRIBUTE_GROUPS(slave_class);

static struct class slave_class = {
	.name = "i2c_slave_class",
	.owner = THIS_MODULE,
	.class_groups = slave_class_groups,
};

int  i2c_slave_read_data(struct aml_i2c_slave *slave,
struct aml_i2c_slave_reg_ctrl *slave_ctrl)
{
	unsigned int rev_data;

	rev_data = (slave->slave_regs->s_rev_reg);

	pr_info("slave receive data:0x%8x\n", rev_data);

	if (i2c_auto_test_flag) {
		if (rev_data == 0x998855aa)
			slave->slave_regs->s_send_reg = rev_data & 0xffffffff;
	}
	slave->slave_regs->s_reg_ctrl |= (1 << 27);

	return  0;
}

#ifdef CONFIG_I2C_SLAVE_TIMER
static void i2c_slave_check_status(unsigned long lparam)
{
	struct aml_i2c_slave *slave = (struct aml_i2c_slave *)lparam;

	struct aml_i2c_slave_reg_ctrl *slave_ctrl;

	slave_ctrl = (struct aml_i2c_slave_reg_ctrl *)&
		(slave->slave_regs->s_reg_ctrl);

	if (((slave->slave_regs->s_reg_ctrl >> 27) & (0x1)) == 0x1)
		mod_timer(&slave->timer,  jiffies +
		msecs_to_jiffies(slave->time_out));
	else if (!i2c_slave_read_data(slave, slave_ctrl))
		return;
}
#endif

static irqreturn_t i2c_slave_xfer_isr(int irq, void *dev_id)
{
	struct aml_i2c_slave *slave;

	struct aml_i2c_slave_reg_ctrl *slave_ctrl;

	slave = (struct aml_i2c_slave *)dev_id;
	slave_ctrl = (struct aml_i2c_slave_reg_ctrl *)&
		(slave->slave_regs->s_reg_ctrl);
	if (((slave->slave_regs->s_reg_ctrl >> 27) & (0x1)) == 0x0)
		if (!i2c_slave_read_data(slave, slave_ctrl))
			return IRQ_HANDLED;

	if  (((slave->slave_regs->s_reg_ctrl >> 28) & (0x1)) == 0x0)
		pr_info("\n");
#ifdef CONFIG_I2C_SLAVE_TIMER
	slave->timer.data = (unsigned long)slave;
	mod_timer(&slave->timer,  jiffies +
				msecs_to_jiffies(slave->time_out));
#endif
	return IRQ_HANDLED;
}

static int i2c_slave_probe(struct platform_device *pdev)
{
	struct resource *res;
	resource_size_t *res_start;
	int ret;
	struct aml_i2c_slave *slave;

	i2c_auto_test_flag = 0;
	if (!strcmp(i2c_auto_test_mode, "open"))
		i2c_auto_test_flag = 1;

	slave =	devm_kzalloc(&pdev->dev, sizeof(struct aml_i2c_slave),
		GFP_KERNEL);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	res_start = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(res_start))
		return PTR_ERR(res_start);

	slave->slave_regs = (struct aml_i2c_reg_slave __iomem *)(res_start);

	slave->irq = platform_get_irq(pdev, 0);

	slave->lock = kzalloc(sizeof(*slave->lock),  GFP_KERNEL);

	mutex_init(slave->lock);

	i2c_slave_init(slave);

	ret = request_irq(slave->irq,  i2c_slave_xfer_isr,
	IRQF_SHARED,  "aml_slave",  (void *)slave);
	if (ret < 0)
		dev_info(&pdev->dev, "slave request irq(%d) failed!\n",
		slave->irq);
	else
		dev_info(&pdev->dev, "slave work in interrupt (irq=%d)\n",
		slave->irq);

	slave->cls = &slave_class;

	ret = class_register(&slave_class);

#ifdef CONFIG_I2C_SLAVE_TIMER
	slave->time_out = 20;

	setup_timer(&slave->timer,
		i2c_slave_check_status, (unsigned long)slave);
#endif
	platform_set_drvdata(pdev, slave);

	g_i2c = slave;

	return 0;
}

static int __init i2c_auto_test_setup(char *s)
{
	if (!s)
		return -EINVAL;

	sprintf(i2c_auto_test_mode, "%s", s);

	return 0;
}
__setup("i2c_auto_test=", i2c_auto_test_setup);

static int i2c_slave_remove(struct platform_device *pdev)
{
	struct aml_i2c_slave *slave;

	slave = platform_get_drvdata(pdev);
	class_destroy(slave->cls);
	mutex_destroy(slave->lock);
	free_irq(slave->irq, slave);
	return 0;
}

static int i2c_slave_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_i2c_slave *slave;

	slave = platform_get_drvdata(pdev);
	mutex_lock(slave->lock);
	disable_irq(slave->irq);
	mutex_unlock(slave->lock);
	pr_info("%s: disable #%d irq\n", __func__, slave->irq);
	return 0;
}

static int i2c_slave_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_i2c_slave *slave;

	slave = platform_get_drvdata(pdev);
	pr_info("%s\n", __func__);
	mutex_lock(slave->lock);
	i2c_slave_init(slave);
	enable_irq(slave->irq);
	mutex_unlock(slave->lock);
	pr_info("%s: enable #%d irq\n", __func__, slave->irq);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2c_slave_matches[] = {
	{ .compatible = "amlogic, meson-i2c-slave", },
	{},
};
#endif

static const struct dev_pm_ops i2c_slave_pm_ops = {
	.suspend_noirq = i2c_slave_suspend,
	.resume_noirq  = i2c_slave_resume,
};

static struct platform_driver i2c_slave_driver = {
	.driver     = {
		.name   = "meson_i2c_slave",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(i2c_slave_matches),
		.pm = &i2c_slave_pm_ops,
	},
	.probe      = i2c_slave_probe,
	.remove     = i2c_slave_remove,
};
module_platform_driver(i2c_slave_driver);
