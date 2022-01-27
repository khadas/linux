/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/amlogic/pm/vad_power.h
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

struct pm_ops {
	int (*vad_pm_init)(struct platform_device *pdev);
	int (*vad_pm_suspend)(struct platform_device *pdev);
	int (*vad_pm_resume)(struct platform_device *pdev);
};

#define SMCID_POWER_MANAGER	0x82000079
	#define SMCSUBID_PM_DDR_ASR	0x1000
	#define SMCSUBID_PM_DUMP_INFO	0x1001

struct pm_data {
	struct device *dev;
	bool vad_wakeup_disable;
	int pm_wakeup_irq;
	/* Define platform private data */
	const char *name;
	void *data;
	struct pm_ops *ops;
};

extern struct pm_ops t3_pm_ops;

int vad_wakeup_power_init(struct platform_device *pdev, struct pm_data *p_data);
int vad_wakeup_power_suspend(struct device *dev);
int vad_wakeup_power_resume(struct device *dev);
