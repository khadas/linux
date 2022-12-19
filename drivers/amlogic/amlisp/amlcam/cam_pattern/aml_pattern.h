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

#ifndef __AML_PATTERN_H__
#define __AML_PATTERN_H__

enum {
	AML_PATTERN_PAD_SRC,
	AML_PATTERN_PAD_MAX,
};

struct pattern_dev_t {
	u32 index;
	u32 fmt_cnt;
	struct device *dev;
	struct platform_device *pdev;
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev sd;
	struct media_pad pads[AML_PATTERN_PAD_MAX];
	struct v4l2_mbus_framefmt pfmt[AML_PATTERN_PAD_MAX];
	const struct aml_format *formats;
	struct aml_subdev subdev;
};

int aml_pattern_subdev_register(struct pattern_dev_t *pattern);
void aml_pattern_subdev_unregister(struct pattern_dev_t *pattern);

int aml_pattern_subdev_init(void *c_dev);
void aml_pattern_subdev_deinit(void *c_dev);

#endif  /*__AML_P1_PATTERN_H__ */
