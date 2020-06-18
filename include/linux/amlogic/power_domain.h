/*
 * include/linux/amlogic/power_domain.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define PWR_ON	0
#define PWR_OFF	1

#ifdef CONFIG_AMLOGIC_POWER
bool is_support_power_domain(void);
void power_domain_switch(int pwr_domain, bool pwr_switch);
#else
static inline void power_domain_switch(int pwr_domain, bool pwr_switch)
{
}
#endif
