/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __GPIO_LNBC_H__
#define __GPIO_LNBC_H__

#include "lnb_controller.h"

int gpio_lnbc_create(struct lnbc *lnbc, struct gpio_desc *gpio_lnb_en,
		struct gpio_desc *gpio_lnb_sel);

#endif /* __GPIO_LNBC_H__ */
