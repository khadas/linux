/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _FREERTOS_H_
#define _FREERTOS_H_
#include <linux/amlogic/rtosinfo.h>

unsigned int freertos_is_run(void);
int freertos_finish(void);
int freertos_is_finished(void);
int freertos_is_irq_rsved(unsigned int irq);
u32 freertos_get_irqregval(u32 val, u32 oldval,
			   unsigned int irqbase,
			   unsigned int n);
struct xrtosinfo_t *freertos_get_info(void);

void arch_send_ipi_rtos(int cpu);

#endif
