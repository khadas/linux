/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _POWER_CTRL_H_
#define _POWER_CTRL_H_
#include <linux/types.h>

bool is_support_power_ctrl(void);
int power_ctrl_sleep(bool power_on, unsigned int shift);
int power_ctrl_sleep_mask(bool power_on,
			  unsigned int mask_val, unsigned int shift);
int power_ctrl_iso(bool power_on, unsigned int shift);
int power_ctrl_iso_mask(bool power_on,
			unsigned int mask_val, unsigned int shift);
int power_ctrl_mempd0(bool power_on, unsigned int mask_val, unsigned int shift);

#endif /*_POWER_CTRL_H_*/
