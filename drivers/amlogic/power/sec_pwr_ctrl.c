// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/arm-smccc.h>
#include <linux/amlogic/power_domain.h>

unsigned long pwr_ctrl_psci_smc(int power_domain, bool power_control)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000093, power_domain, power_control, 0,
		      0, 0, 0, 0, &res);
	return res.a0;
}
EXPORT_SYMBOL(pwr_ctrl_psci_smc);

unsigned long pwr_ctrl_status_psci_smc(int power_domain)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000095, power_domain, 0, 0,
		      0, 0, 0, 0, &res);
	return res.a0;
}
EXPORT_SYMBOL(pwr_ctrl_status_psci_smc);

unsigned long vpu_mempd_psci_smc(int mempd_id, bool power_control)
{
	int switch_id, pd_max_id;
	unsigned long ret;

	pd_max_id = get_max_id();
	switch_id = mempd_id + pd_max_id;
	ret = pwr_ctrl_psci_smc(switch_id, power_control);

	return ret;
}
EXPORT_SYMBOL(vpu_mempd_psci_smc);

unsigned long pwr_ctrl_irq_set(u64 irq, u64 irq_mask, u64 irq_invert)
{
	struct arm_smccc_res res;

	arm_smccc_smc(POWER_CTRL_IRQ_SET, irq, irq_mask, irq_invert,
		      0, 0, 0, 0, &res);
	return res.a0;
}
EXPORT_SYMBOL(pwr_ctrl_irq_set);

