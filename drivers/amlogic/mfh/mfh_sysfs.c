// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "mfh_module.h"

static ssize_t pwron_cpuid_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	u32 cpu_id = 0;

	ret = kstrtouint(buf, 0, &cpu_id);
	if (ret)
		return -EINVAL;
	if (cpu_id > 1) {
		pr_err("0 -> M4A, 1 -> M4B, other invalid");
		return count;
	}
	mfh_poweron(dev, cpu_id, 1);

	return count;
}
static DEVICE_ATTR_WO(pwron_cpuid);

static ssize_t pwroff_cpuid_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;
	int cpu_id = 0;

	ret = kstrtouint(buf, 0, &cpu_id);
	if (ret)
		return -EINVAL;

	if (cpu_id > 1) {
		pr_err("0 -> M4A, 1 -> M4B, other invalid");
		return count;
	}
	mfh_poweron(dev, cpu_id, 0);

	return count;
}
static DEVICE_ATTR_WO(pwroff_cpuid);

void mfh_dev_create_file(struct device *dev)
{
	WARN_ON(device_create_file(dev, &dev_attr_pwron_cpuid));
	WARN_ON(device_create_file(dev, &dev_attr_pwroff_cpuid));
}

void mfh_dev_remove_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_pwron_cpuid);
	device_remove_file(dev, &dev_attr_pwroff_cpuid);
}
