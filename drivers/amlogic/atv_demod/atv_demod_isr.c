/*
 * drivers/amlogic/atv_demod/atv_demod_isr.c
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

#include "atv_demod_isr.h"
#include "atvdemod_func.h"
#include "atv_demod_access.h"
#include "atv_demod_debug.h"

static DEFINE_MUTEX(isr_mutex);

bool atvdemod_isr_en;

static void atv_demod_reset_irq(void)
{
	atv_dmd_wr_long(APB_BLOCK_ADDR_INTERPT_MGT, 0x10, 0x3);
	atv_dmd_wr_long(APB_BLOCK_ADDR_INTERPT_MGT, 0x10, 0x0);
}

static void atv_demod_set_irq(bool enable)
{
	if (enable)
		atv_dmd_wr_long(APB_BLOCK_ADDR_INTERPT_MGT, 0x14, 0x0);
	else
		atv_dmd_wr_long(APB_BLOCK_ADDR_INTERPT_MGT, 0x14, 0xf);
}

static int atv_demod_get_irq_status(void)
{
	return atv_dmd_rd_long(APB_BLOCK_ADDR_INTERPT_MGT, 0x18);
}

static irqreturn_t atv_demod_isr_handler(int irq, void *dev)
{
	int status = atv_demod_get_irq_status();

	pr_isr("irq status: 0x%x.\n", status);
	pr_isr("line_unlock:        %s.\n",
			(status & 0x80) ? "unlocked" : "locked");
	pr_isr("line_strong_unlock: %s.\n",
			(status & 0x40) ? "unlocked" : "locked");
	pr_isr("field_unlock:       %s.\n",
			(status & 0x20) ? "unlocked" : "locked");
	pr_isr("pll_unlock:         %s.\n",
			(status & 0x10) ? "unlocked" : "locked");

	pr_isr("line_lock:          %s.\n",
			(status & 0x08) ? "locked" : "unlocked");
	pr_isr("line_strong_lock:   %s.\n",
			(status & 0x04) ? "locked" : "unlocked");
	pr_isr("field_lock:         %s.\n",
			(status & 0x02) ? "locked" : "unlocked");
	pr_isr("pll_lock:           %s.\n\n",
			(status & 0x01) ? "locked" : "unlocked");

	atv_demod_reset_irq();

	return IRQ_HANDLED;
}

static void atv_demod_isr_disable(struct atv_demod_isr *isr)
{
	mutex_lock(&isr->mtx);

	if (atvdemod_isr_en && isr->init && isr->state == true) {
		atv_demod_set_irq(false);
		disable_irq(isr->irq);
		isr->state = false;
	}

	mutex_unlock(&isr->mtx);

	pr_isr("%s: state: %d.\n", __func__, isr->state);
}

static void atv_demod_isr_enable(struct atv_demod_isr *isr)
{
	mutex_lock(&isr->mtx);

	if (atvdemod_isr_en && isr->init && isr->state == false) {
		atv_demod_reset_irq();
		atv_demod_set_irq(true);
		enable_irq(isr->irq);
		isr->state = true;
	}

	mutex_unlock(&isr->mtx);

	pr_isr("%s: state: %d.\n", __func__, isr->state);
}

void atv_demod_isr_init(struct atv_demod_isr *isr)
{
	int ret = 0;

	mutex_lock(&isr_mutex);

	mutex_init(&isr->mtx);

	isr->state = false;
	isr->disable = atv_demod_isr_disable;
	isr->enable = atv_demod_isr_enable;
	isr->handler = atv_demod_isr_handler;

	ret = request_irq(isr->irq, isr->handler, IRQF_SHARED,
			"atv_demod_irq", (void *) isr);
	if (ret != 0) {
		isr->init = false;
		pr_err("atv_demod_isr request irq error: %d.\n", ret);
	} else {
		isr->init = true;
		disable_irq_nosync(isr->irq);
	}

	mutex_unlock(&isr_mutex);
}

void atv_demod_isr_uninit(struct atv_demod_isr *isr)
{
	mutex_lock(&isr_mutex);

	if (isr->init) {
		free_irq(isr->irq, (void *) isr);
		isr->init = false;
		isr->state = false;
	}

	mutex_unlock(&isr_mutex);
}
