/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_COOLDEV_H__
#define __MESON_COOLDEV_H__

struct cool_dev {
	u32 ddr_reg;
	u32 ddr_status;
	u32 ddr_bits[2];
	u32 ddr_data[16];
	char *device_type;
	struct device_node *np;
	struct thermal_cooling_device *cooling_dev;
};

#ifndef mc_capable
#define mc_capable()		0
#endif
#ifdef CONFIG_AMLOGIC_COOLDEV
struct thermal_cooling_device;
int meson_gcooldev_min_update(struct thermal_cooling_device *cdev);
#else
struct thermal_cooling_device;
int meson_gcooldev_min_update(struct thermal_cooling_device *cdev)
{
	return 0;
}
#endif
#endif

