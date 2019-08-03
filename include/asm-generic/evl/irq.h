/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_IRQ_H
#define _ASM_GENERIC_EVL_IRQ_H

#include <evl/irq.h>

static inline void irq_enter_pipeline(void)
{
#ifdef CONFIG_EVL
	evl_enter_irq();
#endif
}

static inline void irq_exit_pipeline(void)
{
#ifdef CONFIG_EVL
	evl_exit_irq();
#endif
}

#endif /* !_ASM_GENERIC_EVL_IRQ_H */
