/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_DEMOD_ISR_H__
#define __ATV_DEMOD_ISR_H__

#include <linux/mutex.h>
#include <linux/interrupt.h>


struct atv_demod_isr {
	struct mutex mtx;

	unsigned int irq;

	bool init;
	bool state;

	void (*disable)(struct atv_demod_isr *isr);
	void (*enable)(struct atv_demod_isr *isr);

	irqreturn_t (*handler)(int irq, void *dev);
};

extern void atv_demod_isr_init(struct atv_demod_isr *isr);
extern void atv_demod_isr_uninit(struct atv_demod_isr *isr);

#endif /* __ATV_DEMOD_ISR_H__ */
