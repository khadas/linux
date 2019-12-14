// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/arm-smccc.h>
#include <linux/amlogic/pwr_ctrl.h>

unsigned long pwr_ctrl_psci_smc(enum pm_e power_domain, bool power_control)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000093, power_domain, power_control, 0,
		      0, 0, 0, 0, &res);
	return res.a0;
}
EXPORT_SYMBOL(pwr_ctrl_psci_smc);

unsigned long pwr_ctrl_status_psci_smc(enum pm_e power_domain)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000095, power_domain, 0, 0,
		      0, 0, 0, 0, &res);
	return res.a0;
}
EXPORT_SYMBOL(pwr_ctrl_status_psci_smc);

unsigned long pwr_ctrl_irq_set(u64 irq, u64 irq_mask, u64 irq_invert)
{
	struct arm_smccc_res res;

	arm_smccc_smc(POWER_CTRL_IRQ_SET, irq, irq_mask, irq_invert,
		      0, 0, 0, 0, &res);
	return res.a0;
}

