/*
 * drivers/amlogic/atv_demod/atv_demod_isr.h
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
