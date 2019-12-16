/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_CPUIDLE_H_
#define _AML_CPUIDLE_H_

#include <linux/cpumask.h>

void aml_suspend_power_handler(void);
void do_aml_resume_power(void);
bool is_aml_cpuidle_enabled(void);
void arch_suspend_notifier(const struct cpumask *mask);

#endif
