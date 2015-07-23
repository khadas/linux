/*
 * drivers/amlogic/rtc/aml_vrtc.c
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
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/pm_wakeup.h>

static u32 total_sleep_time;
static u32 alarm_time;
static u32 alarm_reg_addr;

static int aml_vrtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u32 time_t = (u32)(jiffies_to_msecs(jiffies) / 1000) + total_sleep_time;

	rtc_time_to_tm(time_t, tm);

	return 0;
}

static int set_wakeup_time(unsigned long time)
{
	void __iomem *vaddr;
	if (alarm_reg_addr)
		vaddr = ioremap(alarm_reg_addr, 0x4);
	else
		return -1;
	writel(time, vaddr);
	pr_info("set_wakeup_time: %lu\n", time);
	return 0;
}

static int aml_vrtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	unsigned long alarm_secs, cur_secs;
	int ret;
	struct	rtc_device *vrtc;

	struct rtc_time cur_rtc_time;

	vrtc = dev_get_drvdata(dev);

	if (alarm->enabled) {
		ret = rtc_tm_to_time(&alarm->time, &alarm_secs);
		if (ret)
			return ret;
		aml_vrtc_read_time(dev, &cur_rtc_time);
		ret = rtc_tm_to_time(&cur_rtc_time, &cur_secs);
		if (alarm_secs >= cur_secs) {
			alarm_secs = alarm_secs - cur_secs;
			ret = set_wakeup_time(alarm_secs);
			if (ret < 0)
				return ret;
			alarm_time = alarm_secs;
			pr_info("system will wakeup %lus later\n", alarm_secs);
		}
	}
	return 0;
}

static const struct rtc_class_ops aml_vrtc_ops = {
	.read_time = aml_vrtc_read_time,
	.set_alarm = aml_vrtc_set_alarm,
};

static int aml_vrtc_probe(struct platform_device *pdev)
{
	struct	rtc_device *vrtc;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node,
			"alarm_reg_addr", &alarm_reg_addr);
	if (ret)
		return -1;
	pr_info("alarm_reg_addr: 0x%x\n", alarm_reg_addr);

	device_init_wakeup(&pdev->dev, 1);
	vrtc = rtc_device_register("aml_vrtc", &pdev->dev,
		&aml_vrtc_ops, THIS_MODULE);
	if (!vrtc)
		return -1;
	platform_set_drvdata(pdev, vrtc);

	return 0;
}

static int aml_vrtc_remove(struct platform_device *dev)
{
	struct rtc_device *vrtc = platform_get_drvdata(dev);
	rtc_device_unregister(vrtc);

	return 0;
}

int aml_vrtc_resume(struct platform_device *pdev)
{
	total_sleep_time += alarm_time;
	set_wakeup_time(0);
	return 0;
}

static const struct of_device_id aml_vrtc_dt_match[] = {
	{ .compatible = "amlogic, aml_vrtc"},
	{},
};

struct platform_driver aml_vrtc_driver = {
	.driver = {
		.name = "aml_vrtc",
		.owner = THIS_MODULE,
		.of_match_table = aml_vrtc_dt_match,
	},
	.probe = aml_vrtc_probe,
	.remove = aml_vrtc_remove,
	.resume = aml_vrtc_resume,
};

static int  __init aml_vrtc_init(void)
{
	return platform_driver_register(&aml_vrtc_driver);
}

static void __init aml_vrtc_exit(void)
{
	return platform_driver_unregister(&aml_vrtc_driver);
}

module_init(aml_vrtc_init);
module_exit(aml_vrtc_exit);

MODULE_DESCRIPTION("Amlogic internal vrtc driver");
MODULE_LICENSE("GPL");
