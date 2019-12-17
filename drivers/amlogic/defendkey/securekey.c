// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include "defendkey.h"

int aml_is_secure_set(void __iomem *reg_base)
{
	int ret;

	/*AO_SEC_SD_CFG10: bit4 secure boot enable*/
	ret = (readl(reg_base + 0x00) >> 4) & 0x1;

	return ret;
}

unsigned long aml_sec_boot_check(unsigned long type,
				 unsigned long buffer,
				 unsigned long length,
				 unsigned long option)
{
	struct arm_smccc_res res;

	asm __volatile__("" : : : "memory");

	do {
		arm_smccc_smc((unsigned long)AML_DATA_PROCESS,
			      (unsigned long)type,
			      (unsigned long)buffer,
			      (unsigned long)length,
			      (unsigned long)option,
			      0, 0, 0, &res);
	} while (0);

	return res.a0;
}
