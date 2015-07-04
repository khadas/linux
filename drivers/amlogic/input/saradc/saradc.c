/*
 * drivers/amlogic/input/saradc/saradc.c
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/amlogic/saradc.h>
#include "saradc_reg.h"

/* #define ENABLE_DYNAMIC_POWER */

#define saradc_info(x...) dev_info(adc->dev, x)
#define saradc_dbg(x...) /* dev_info(adc->dev, x) */

#define SARADC_STATE_IDLE 0
#define SARADC_STATE_BUSY 1
#define SARADC_STATE_SUSPEND 2

#define FLAG_INITIALIZED (1<<28)
#define FLAG_BUSY (1<<29)

struct saradc {
	struct device *dev;
	struct saradc_regs __iomem *regs;
	unsigned int __iomem *clk_reg;
	struct clk *clk;
	spinlock_t lock;
	int ref_val;
	int ref_nominal;
	int coef;
	int state;
};

static struct saradc *gp_saradc;

static inline void saradc_power_control(struct saradc *adc, int on)
{
	struct saradc_reg3 *reg3 = (struct saradc_reg3 *)&adc->regs->reg3;

	if (on) {
		adc->regs->reg11 |= 1<<13; /* enable bandgap */
		reg3->adc_en = 1;
		udelay(5);
		if (adc->clk_reg)
			*adc->clk_reg |= (1<<8);
		else
			reg3->clk_en = 1;
	}	else {
		if (adc->clk_reg)
			*adc->clk_reg &= ~(1<<8);
		else
			reg3->clk_en = 0;
		reg3->adc_en = 0;
		adc->regs->reg11 &= ~(1<<13); /* disable bandgap */
	}
}

static void saradc_reset(struct saradc *adc)
{
	int clk_src, clk_div;
	if (adc->regs->reg3 & FLAG_INITIALIZED) {
		saradc_info("initialized by BL30\n");
		return;
	}
	saradc_info("initialized by gxbb\n");
	adc->regs->reg0 = 0x84004040;
	adc->regs->ch_list = 0;
	/* REG2: all chanel set to 8-samples & median averaging mode */
	adc->regs->avg_cntl = 0x0;

	clk_prepare_enable(adc->clk);
	clk_div = clk_get_rate(adc->clk) / 1200000;
	clk_src = 0; /* should get from adc->clk in g9tv */
	clk_div = 20;
	saradc_info("clk_src=%d, clk_div=%d\n", clk_src, clk_div);
	adc->regs->reg3 = 0x9388000a | (clk_div << 10);
	if (adc->clk_reg)
		*adc->clk_reg = (clk_src<<9) | (clk_div << 0);

	adc->regs->delay = 0x10a000a; /* if low clk(32K), set to 0x190a380a */
	adc->regs->aux_sw = 0x3eb1a0c;
	adc->regs->ch10_sw = 0x8c000c;
	adc->regs->detect_idle_sw = 0xc000c;
#ifndef ENABLE_DYNAMIC_POWER
	saradc_power_control(adc, 1);
#endif
}

static int  saradc_internal_cal(struct saradc *adc)
{
	struct saradc_reg3 *reg3 = (struct saradc_reg3 *)&adc->regs->reg3;
	int val[5], nominal[5] = {0, 256, 512, 768, 1023};
	int i;

	saradc_info("calibration start:\n");
	adc->coef = 0;
	for (i = 0; i < 5; i++) {
		reg3->cal_cntl = i;
		udelay(10);
		val[i] = get_adc_sample(0, CHAN_7);
		if (val[i] < 0)
			return -1;
		saradc_info("nominal=%d, value=%d\n", nominal[i], val[i]);
	}
	adc->ref_val = val[2];
	adc->ref_nominal = nominal[2];
	if (val[3] > val[1]) {
		adc->coef = (nominal[3] - nominal[1]) << 12;
		adc->coef /= val[3] - val[1];
	}
	saradc_info("calibration end: coef=%d\n", adc->coef);
	reg3->cal_cntl = 7;
	return 0;
}

static int saradc_get_cal_value(struct saradc *adc, int val)
{
	int nominal;
/*
	((nominal - ref_nominal) << 10) / (val - ref_val) = coef
	==> nominal = ((val - ref_val) * coef >> 10) + ref_nominal
*/
	nominal = val;
	if ((adc->coef > 0) && (val > 0)) {
		nominal = (val - adc->ref_val) * adc->coef;
		nominal >>= 12;
		nominal += adc->ref_nominal;
	}
	if (nominal < 0)
		nominal = 0;
	if (nominal > 1023)
		nominal = 1023;
	return nominal;
}

int get_adc_sample(int dev_id, int ch)
{
	struct saradc *adc;
	struct saradc_reg0 *reg0;
	struct saradc_reg3 *reg3;
	int value = 0, count = 0;
	unsigned long flags;

	adc = gp_saradc;
	if (!adc)
		return -1;

	spin_lock_irqsave(&adc->lock, flags);
	if (adc->state == SARADC_STATE_SUSPEND) {
		spin_unlock_irqrestore(&adc->lock, flags);
		return -1;
	}
	adc->state = SARADC_STATE_BUSY;
	reg0 = (struct saradc_reg0 *)&adc->regs->reg0;
	reg3 = (struct saradc_reg3 *)&adc->regs->reg3;
	while (adc->regs->reg3 & FLAG_BUSY) {
		udelay(100);
		if (++count > 100) {
			saradc_info("bl30 busy error\n");
			value = -1;
			goto end1;
		}
	}
#ifdef ENABLE_DYNAMIC_POWER
	saradc_power_control(adc, 1);
#endif
	adc->regs->ch_list = ch;
	adc->regs->detect_idle_sw = 0xc000c | (ch<<23) | (ch<<7);
	reg0->sample_engine_en = 1;
	reg0->start_sample = 1;
	do {
		udelay(10);
		if (!(adc->regs->reg0 & 0x70000000))
			break;
		else if (++count > 10000) {
			saradc_info("busy error=%x\n", adc->regs->reg0);
			value = -1;
			goto end;
		}
	} while (1);

	value = adc->regs->fifo_rd & 0x3ff;
	saradc_dbg("before cal: %d, count=%d\n", value, count);
	if (adc->coef) {
		value = saradc_get_cal_value(adc, value);
		saradc_dbg("after cal: %d\n", value);
	}
end:
	reg0->stop_sample = 1;
	reg0->sample_engine_en = 0;
#ifdef ENABLE_DYNAMIC_POWER
	saradc_power_control(0);
#endif
end1:
	adc->state = SARADC_STATE_IDLE;
	spin_unlock_irqrestore(&adc->lock, flags);
	return value;
}

static unsigned int __iomem *
saradc_get_reg_addr(struct platform_device *pdev, int index)
{
	struct resource *res;
	unsigned int __iomem *reg_addr;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (!res) {
		dev_err(&pdev->dev, "reg: cannot obtain I/O memory region");
		return 0;
	}
	if (!devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Memory region busy\n");
		return 0;
	}
	reg_addr = devm_ioremap_nocache(&pdev->dev, res->start,
			resource_size(res));
	return reg_addr;
}

static ssize_t ch0_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 0));
}
static ssize_t ch1_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 1));
}
static ssize_t ch2_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 2));
}
static ssize_t ch3_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 3));
}
static ssize_t ch4_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 4));
}
static ssize_t ch5_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 5));
}
static ssize_t ch6_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 6));
}
static ssize_t ch7_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_adc_sample(0, 7));
}

static struct class_attribute saradc_class_attrs[] = {
		__ATTR_RO(ch0),
		__ATTR_RO(ch1),
		__ATTR_RO(ch2),
		__ATTR_RO(ch3),
		__ATTR_RO(ch4),
		__ATTR_RO(ch5),
		__ATTR_RO(ch6),
		__ATTR_RO(ch7),
		__ATTR_NULL
};

static struct class saradc_class = {
		.name = "saradc",
		.class_attrs = saradc_class_attrs,
};

static int saradc_probe(struct platform_device *pdev)
{
	int err;
	struct saradc *adc;

	adc = kzalloc(sizeof(struct saradc), GFP_KERNEL);
	if (!adc) {
		err = -ENOMEM;
		goto end_err;
	}
	adc->dev = &pdev->dev;

	if (!pdev->dev.of_node) {
		err = -EINVAL;
		goto end_err;
	}
	adc->regs = (struct saradc_regs __iomem *)saradc_get_reg_addr(pdev, 0);
	if (!adc->regs) {
		err = -ENODEV;
		goto end_err;
	}
	adc->clk_reg = saradc_get_reg_addr(pdev, 1);
	adc->clk = devm_clk_get(&pdev->dev, "saradc_clk");
	if (IS_ERR(adc->clk)) {
		err = -ENOENT;
		goto end_err;
	}

	saradc_reset(adc);
	gp_saradc = adc;
	dev_set_drvdata(&pdev->dev, adc);
	spin_lock_init(&adc->lock);
	adc->state = SARADC_STATE_IDLE;
	saradc_internal_cal(adc);
	return 0;

end_err:
	dev_err(&pdev->dev, "error=%d\n", err);
	kfree(adc);
	return err;
}

static int saradc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 0);
	adc->state = SARADC_STATE_SUSPEND;
	spin_unlock_irqrestore(&adc->lock, flags);
	return 0;
}

static int saradc_resume(struct platform_device *pdev)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 1);
	adc->state = SARADC_STATE_IDLE;
	spin_unlock_irqrestore(&adc->lock, flags);
	return 0;
}

static int saradc_remove(struct platform_device *pdev)
{
	struct saradc *adc = (struct saradc *)dev_get_drvdata(&pdev->dev);
	unsigned long flags;

	spin_lock_irqsave(&adc->lock, flags);
	saradc_power_control(adc, 0);
	spin_unlock_irqrestore(&adc->lock, flags);
	kfree(adc);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id saradc_dt_match[] = {
	{ .compatible = "amlogic, saradc"},
	{},
};
#else
#define saradc_dt_match NULL
#endif

static struct platform_driver saradc_driver = {
	.probe      = saradc_probe,
	.remove     = saradc_remove,
	.suspend    = saradc_suspend,
	.resume     = saradc_resume,
	.driver     = {
		.name   = "saradc",
		.of_match_table = saradc_dt_match,
	},
};

static int __init saradc_init(void)
{
	/* printk(KERN_INFO "SARADC Driver init.\n"); */
	class_register(&saradc_class);
	return platform_driver_register(&saradc_driver);
}

static void __exit saradc_exit(void)
{
	/* printk(KERN_INFO "SARADC Driver exit.\n"); */
	platform_driver_unregister(&saradc_driver);
	class_unregister(&saradc_class);
}

module_init(saradc_init);
module_exit(saradc_exit);

MODULE_AUTHOR("aml");
MODULE_DESCRIPTION("SARADC Driver");
MODULE_LICENSE("GPL");
