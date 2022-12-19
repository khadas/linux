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

#ifndef __AML_CAM_H__
#define __AML_CAM_H__

#include "cam_isp/aml_isp.h"
#include "cam_csiphy/aml_csiphy.h"
#include "cam_adapter/aml_adapter.h"
#include "cam_pattern/aml_pattern.h"

#define AML_CAM_DRIVER_NAME	"aml-cam"
#define AML_CAM_BUS_INFO	"platform:" AML_CAM_DRIVER_NAME
#define AML_CAM_COUNT_MAX      8

enum {
	AML_CAM_0 = 0,
	AML_CAM_1,
	AML_CAM_2,
	AML_CAM_3,
	AML_CAM_4,
};

struct cam_device {
	u32 index;
	char *bus_info;
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;

	struct csiphy_dev_t csiphy_dev;
	struct adapter_dev_t adap_dev;
	struct isp_dev_t isp_dev;
	struct pattern_dev_t pattern;
};

#endif /* __AML_CAM_H__ */
