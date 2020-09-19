/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_ARM_ASM_CALIBRATION_H
#define _EVL_ARM_ASM_CALIBRATION_H

#include <linux/kconfig.h>

static inline unsigned int evl_get_default_clock_gravity(void)
{
	/* Reasonable default for many armv7-based systems. */
	return IS_ENABLED(CONFIG_SMP) ? 6000 : 3000;
}

#endif /* !_EVL_ARM_ASM_CALIBRATION_H */
