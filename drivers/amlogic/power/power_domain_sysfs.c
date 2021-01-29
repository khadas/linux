/*
 * drivers/amlogic/power/power_domain_sysfs.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>
#include <linux/amlogic/power_domain.h>

static unsigned int power_domain;
struct generic_pm_domain **power_domains;
static unsigned int pdid_start, pdid_max;

int get_max_id(void)
{
	return pdid_max;
}

static ssize_t power_on_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &power_domain);
	if (ret)
		return -EINVAL;

	pwr_ctrl_psci_smc(power_domain, PWR_ON);

	return count;
}
static DEVICE_ATTR_WO(power_on);

static ssize_t power_off_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &power_domain);
	if (ret)
		return -EINVAL;

	pwr_ctrl_psci_smc(power_domain, PWR_OFF);

	return count;
}
static DEVICE_ATTR_WO(power_off);

static ssize_t power_status_show(struct device *_dev,
				 struct device_attribute *attr, char *buf)
{
	int power_status, i;
	unsigned int len = 0, cnt = 20;

	for (i = pdid_start; i < pdid_max; i++) {
		if (!*(power_domains + i))
			continue;
		power_status = pwr_ctrl_status_psci_smc(i);
		snprintf(buf, cnt, "%s[%d]		:%d\n",
			 (*(power_domains + i))->name, i, power_status);
		buf = buf + cnt;
		len = len + cnt;
	}

	return len;
}

static ssize_t power_status_store(struct device *_dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &power_domain);
	if (ret)
		return -EINVAL;

	return count;
}
static DEVICE_ATTR_RW(power_status);

void pd_dev_create_file(struct device *dev, int cnt_start, int cnt_end,
			struct generic_pm_domain **domains)
{
	power_domains = domains;
	pdid_start = cnt_start;
	pdid_max = cnt_end;

	device_create_file(dev, &dev_attr_power_status);
	device_create_file(dev, &dev_attr_power_on);
	device_create_file(dev, &dev_attr_power_off);
}

void pd_dev_remove_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_power_status);
	device_remove_file(dev, &dev_attr_power_on);
	device_remove_file(dev, &dev_attr_power_off);
}
