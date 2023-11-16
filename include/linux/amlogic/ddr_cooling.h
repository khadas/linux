/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DDR_COOLING_H__
#define __DDR_COOLING_H__

#include <linux/thermal.h>
#include <linux/amlogic/meson_cooldev.h>

enum ddr_die_type {
	DDR_TYPE1,
	DDR_TYPE2,
	DDR_TYPE_MAX
};

struct ddr_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	u32 ddr_reg;
	void __iomem *vddr_reg;
	u32 ddr_status;
	u32 ddr_bits[2];
	u32 ddr_data[16];
	u32 ddr_bits_keep;	/*for keep ddr reg val excepts change bits*/
	struct list_head node;
};

#ifdef CONFIG_AMLOGIC_DDR_THERMAL

struct thermal_cooling_device *ddr_cooling_register(struct device_node *np,
						    struct cool_dev *cool);

/**
 * ddr_cooling_unregister - function to remove ddr cooling device.
 * @cdev: thermal cooling device pointer.
 */
void ddr_cooling_unregister(struct thermal_cooling_device *cdev);

#else

static inline struct thermal_cooling_device *
ddr_cooling_register(struct device_node *np,
		     struct cool_dev *cool)
{
	return NULL;
}

static inline
void ddr_cooling_unregister(struct thermal_cooling_device *cdev)
{
}
#endif
struct ddr_cooling_device_v2 {
	int id;
	struct thermal_cooling_device *cool_dev;
	int ddr_reg_cnt;
	u32 *ddr_reg;
	void __iomem **vddr_reg;
	u32 ddr_status;
	u32 (*ddr_bits)[2];
	u32 (*ddr_data)[20];
	u32 *ddr_bits_keep;	/*for keep ddr reg val excepts change bits*/
	u32 last_state;
	struct list_head node;
};

#ifdef CONFIG_AMLOGIC_DDR_THERMAL
struct thermal_cooling_device *ddr_cooling_register_v2(struct device_node *np);

/**
 * ddr_cooling_unregister - function to remove ddr cooling device.
 * @cdev: thermal cooling device pointer.
 */
void ddr_cooling_unregister_v2(struct thermal_cooling_device *cdev);

#else

static inline struct thermal_cooling_device *
ddr_cooling_register_v2(struct device_node *np)
{
	return NULL;
}

static inline
void ddr_cooling_unregister_v2(struct thermal_cooling_device *cdev)
{
}
#endif


#endif /* __DDR_COOLING_H__ */
