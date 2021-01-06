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
#include <linux/pm_domain.h>

#define PWR_ON	1
#define PWR_OFF	0

#define DOMAIN_INIT_ON    0
#define DOMAIN_INIT_OFF   1
#define POWER_CTRL_IRQ_SET     0x82000094
#define FLAG_ALWAYS_ON		2

int get_max_id(void);
unsigned long pwr_ctrl_psci_smc(int power_domain, bool power_control);
unsigned long pwr_ctrl_status_psci_smc(int power_domain);
unsigned long vpu_mempd_psci_smc(int mempd_id, bool power_control);

/*
 *irq:irq number for wakeup pwrctrl
 *irq_mask:irq mask,1:enable,0:mask
 *irq_invert:1:invert irq level,0:do not
 */
unsigned long pwr_ctrl_irq_set(u64 irq, u64 irq_mask, u64 irq_invert);

void pd_dev_create_file(struct device *dev, int cnt_start, int cnt_end,
			struct generic_pm_domain **domains);
void pd_dev_remove_file(struct device *dev);

#ifdef CONFIG_AMLOGIC_POWER
bool is_support_power_domain(void);
void power_domain_switch(int pwr_domain, bool pwr_switch);
#else
static inline void power_domain_switch(int pwr_domain, bool pwr_switch)
{
}
#endif
