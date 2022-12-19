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

#define pr_fmt(fmt)  "aml-pattern:%s:%d: " fmt, __func__, __LINE__

#include <linux/io.h>

#include "aml_cam.h"
#include "aml_common.h"

#define AML_PATTERN_NAME "isp-test-pattern-gen"

static const struct aml_format pattern_support_formats[] = {
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB14_1X14, 0, 1, 14},
};

static int pattern_subdev_set_format(void *priv, void *s_fmt, void *m_fmt)
{
	return 0;
}

static void pattern_subdev_log_status(void *priv)
{
	pr_info("Log status done\n");
}

static int pattern_subdev_stream_on(void *priv)
{
	return 0;
}

static void pattern_subdev_stream_off(void *priv)
{
	return;
}

static const struct aml_sub_ops pattern_subdev_ops = {
	.set_format = pattern_subdev_set_format,
	.stream_on = pattern_subdev_stream_on,
	.stream_off = pattern_subdev_stream_off,
	.log_status = pattern_subdev_log_status,
};

int aml_pattern_subdev_register(struct pattern_dev_t *pattern)
{
	int rtn = -1;
	struct media_pad *pads = pattern->pads;
	struct aml_subdev *subdev = &pattern->subdev;

	pattern->formats = pattern_support_formats;
	pattern->fmt_cnt = ARRAY_SIZE(pattern_support_formats);

	pads[AML_PATTERN_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;

	subdev->name = AML_PATTERN_NAME;
	subdev->dev = pattern->dev;
	subdev->sd = &pattern->sd;
	subdev->function = MEDIA_ENT_F_VID_MUX;
	subdev->v4l2_dev = pattern->v4l2_dev;
	subdev->pads = pads;
	subdev->pad_max = AML_PATTERN_PAD_MAX;
	subdev->formats = pattern->formats;
	subdev->fmt_cnt = pattern->fmt_cnt;
	subdev->pfmt = pattern->pfmt;
	subdev->ops = &pattern_subdev_ops;
	subdev->priv = pattern;

	rtn = aml_subdev_register(subdev);
	if (rtn)
		goto error_rtn;

	pr_info("PATTTERN%u: register subdev\n", pattern->index);

error_rtn:
	return rtn;
}

void aml_pattern_subdev_unregister(struct pattern_dev_t *pattern)
{
	aml_subdev_unregister(&pattern->subdev);
}

int aml_pattern_subdev_init(void *c_dev)
{
	struct pattern_dev_t *pattern;
	struct cam_device *cam_dev = c_dev;

	pattern = &cam_dev->pattern;

	pattern->dev = NULL;
	pattern->index = cam_dev->index;
	pattern->v4l2_dev = &cam_dev->v4l2_dev;
	pattern->index = cam_dev->index;

	pr_info("PATTERN%u: subdev init\n", pattern->index);

	return 0;
}

void aml_pattern_subdev_deinit(void *c_dev)
{
	struct pattern_dev_t *pattern;
	struct cam_device *cam_dev = c_dev;

	cam_dev = c_dev;
	pattern = &cam_dev->pattern;

	pr_info("PATTERN%u: subdev deinit\n", pattern->index);
}
