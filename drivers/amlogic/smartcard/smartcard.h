/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_SMC_H_
#define _AML_SMC_H_
int amlogic_gpio_name_map_num(const char *name);
int amlogic_gpio_direction_output(unsigned int pin, int value,
				  const char *owner);
int amlogic_gpio_request(unsigned int pin, const char *label);
unsigned long get_mpeg_clk(void);
long smc_get_reg_base(void);
#endif
