/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __PLAT_MESON_BT_DEVICE_H
#define __PLAT_MESON_BT_DEVICE_H

struct bt_dev_data {
	int gpio_reset;
	int gpio_en;
	int gpio_hostwake;
	int gpio_btwakeup;
	int power_low_level;
	int power_on_pin_OD;
	int power_off_flag;
	int power_down_disable;
	int irqno_wakeup;
	struct work_struct btwakeup_work;
	struct input_dev *input_dev;
	struct hrtimer timer;
};

int sdio_get_vendor(void);

#endif
