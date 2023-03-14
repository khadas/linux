/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __AML_CSIPHY_H__
#define __AML_CSIPHY_H__

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>

#include <media/v4l2-async.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>

#include "aml_common.h"

enum {
	AML_CSIPHY_PAD_SINK = 0,
	AML_CSIPHY_PAD_SRC,
	AML_CSIPHY_PAD_MAX,
};

struct csiphy_async_subdev {
	struct v4l2_async_subdev asd;
	int phy_id;
	int data_lanes;
};

struct csiphy_dev_ops {
	void (*hw_reset)(void *c_dev, int idx);
	u32 (*hw_version)(void *c_dev, int idx);
	int (*hw_start)(void *c_dev, int idx, int lanes, s64 link_freq);
	void (*hw_stop)(void *c_dev, int idx);
};

struct csiphy_dev_t {
	u32 index;
	u32 version;
	struct device *dev;
	struct platform_device *pdev;

	void __iomem *csi_dphy;
	void __iomem *csi_aphy;
	u32 clock_mode;
	struct clk *csiphy_clk;
	struct clk *csiphy_clk1;

	struct v4l2_subdev sd;
	struct media_pad pads[AML_CSIPHY_PAD_MAX];
	struct v4l2_mbus_framefmt pfmt[AML_CSIPHY_PAD_MAX];
	u32 fmt_cnt;
	const struct aml_format *formats;
	struct v4l2_device *v4l2_dev;
	struct aml_subdev subdev;

	struct v4l2_async_notifier *notifier;

	const struct csiphy_dev_ops *ops;
};


void csiphy_subdev_suspend(struct csiphy_dev_t *csiphy_dev);
int csiphy_subdev_resume(struct csiphy_dev_t *csiphy_dev);

int aml_csiphy_subdev_init(void *c_dev);
void aml_csiphy_subdev_deinit(void *c_dev);
int aml_csiphy_subdev_register(struct csiphy_dev_t *csiphy_dev);
void aml_csiphy_subdev_unregister(struct csiphy_dev_t *csiphy_dev);


extern const struct csiphy_dev_ops csiphy_dev_hw_ops;

#endif /* __AML_CSIPHY_H__ */
