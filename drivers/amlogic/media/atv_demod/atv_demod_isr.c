// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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

	pr_isr("irq status:0x%x\n", status);
	pr_isr("line: %d\n", status & 0x80);
	pr_isr("line_strong:%d\n", status & 0x40);
	pr_isr("field:%d\n", status & 0x20);
	pr_isr("pll:%d\n", status & 0x10);

	pr_isr("line:%d\n", status & 0x08);
	pr_isr("line_strong:%d\n", status & 0x04);
	pr_isr("field:%d\n", status & 0x02);
	pr_isr("pll:%d\n\n", status & 0x01);

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

	pr_isr("%s:state %d\n", __func__, isr->state);
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

	pr_isr("%s:state %d\n", __func__, isr->state);
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
		pr_err("request irq error %d\n", ret);
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
