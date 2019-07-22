/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_IRQ_H
#define _ASM_GENERIC_EVL_IRQ_H

#include <evl/irq.h>

static inline void irq_enter_pipeline(void)
{
	evl_enter_irq();
}

static inline void irq_exit_pipeline(void)
{
	evl_exit_irq();
}

#endif /* !_ASM_GENERIC_EVL_IRQ_H */
