/*
 * drivers/amlogic/clk/rstc.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


#include <linux/io.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "clk.h"

/*
 * Modules and submodules within the chip can be reset by disabling the
 * clock and enabling it again.
 * The modules in the AO (Always-On) domain are controlled by a different
 * register mapped in a different memory region accessed by the ao_base.
 *
 */
#define  BITS_PER_REG	32

struct meson_rstc {
	struct reset_controller_dev	rcdev;
	void __iomem			*ao_base;
	void __iomem			*ot_base;
	unsigned int			num_regs;
	unsigned int			ao_off_id;
	u8				flags;
	spinlock_t			lock;
	s8				*id_ref;
};

static int meson_rstc_assert(struct reset_controller_dev *rcdev,
			     unsigned long id)
{
	struct meson_rstc *rstc = container_of(rcdev,
					       struct meson_rstc,
					       rcdev);
	int bank = id / BITS_PER_REG;
	int offset;
	void __iomem *rstc_mem;
	unsigned long flags;
	u32 reg;

	/*
	 * The higher IDs are used for the AO domain register
	 */
	if (id >= rstc->ao_off_id) {
		offset = id - rstc->ao_off_id;
		rstc_mem = rstc->ao_base;
	} else {
		offset = id % BITS_PER_REG;
		rstc_mem = rstc->ot_base + (bank << 2);
	}

	spin_lock_irqsave(&rstc->lock, flags);
	if (--rstc->id_ref[id] <= 0) {
		reg = readl(rstc_mem);
		writel(reg & ~BIT(offset), rstc_mem);
		rstc->id_ref[id] = 0;
	}
	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;
}

static int meson_rstc_deassert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct meson_rstc *rstc = container_of(rcdev,
					       struct meson_rstc,
					       rcdev);
	int bank = id / BITS_PER_REG;
	int offset;
	void __iomem *rstc_mem;
	unsigned long flags;
	u32 reg;

	if (id >= rstc->ao_off_id) {
		offset = id - rstc->ao_off_id;
		rstc_mem = rstc->ao_base;
	} else {
		offset = id % BITS_PER_REG;
		rstc_mem = rstc->ot_base + (bank << 2);
	}

	spin_lock_irqsave(&rstc->lock, flags);
	if (rstc->id_ref[id]++ == 0) {
		reg = readl(rstc_mem);
		writel(reg | BIT(offset), rstc_mem);
	}
	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;

}

static int meson_rstc_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	int err;

	err = meson_rstc_assert(rcdev, id);
	if (err)
		return err;

	return meson_rstc_deassert(rcdev, id);
}

static struct reset_control_ops meson_rstc_ops = {
	.assert		= meson_rstc_assert,
	.deassert	= meson_rstc_deassert,
	.reset		= meson_rstc_reset,
};

static __init void meson_init_id_ref(struct meson_rstc *rstc)
{
	unsigned long flags;
	int i, bank, offset;
	void __iomem *rstc_mem;
	u32 reg;
	spin_lock_irqsave(&rstc->lock, flags);
	for (i = 0; i < rstc->ao_off_id; i++) {
		bank = i / BITS_PER_REG;
		offset = i % BITS_PER_REG;
		rstc_mem = rstc->ot_base + (bank << 2);
		reg = readl(rstc_mem);
		rstc->id_ref[i] = (reg & (1 << offset)) ? 1 : 0;
		/* pr_info("id_ref[%d]: %d\n", i, rstc->id_ref[i]); */
	}
	spin_unlock_irqrestore(&rstc->lock, flags);
}

void __init meson_register_rstc(struct device_node *np, unsigned int num_regs,
				void __iomem *ao_base, void __iomem *ot_base,
				unsigned int ao_off_id, u8 flags)
{
	struct meson_rstc *rstc;
	int ret;

	rstc = kzalloc(sizeof(*rstc), GFP_KERNEL);
	if (!rstc)
		return;
	rstc->id_ref = kzalloc(ao_off_id + 1, GFP_KERNEL);
	if (!rstc->id_ref) {
		pr_err("rstc->id_ref is not allocated\n");
		kfree(rstc);
		return;
	}

	spin_lock_init(&rstc->lock);

	rstc->ao_base = ao_base;
	rstc->ot_base = ot_base;
	rstc->num_regs = num_regs;
	rstc->flags = flags;

	rstc->rcdev.owner = THIS_MODULE;
	rstc->rcdev.nr_resets = num_regs * BITS_PER_REG;
	rstc->rcdev.of_node = np;
	rstc->rcdev.ops = &meson_rstc_ops;
	rstc->ao_off_id = ao_off_id;
	meson_init_id_ref(rstc);

	ret = reset_controller_register(&rstc->rcdev);
	if (ret) {
		pr_err("%s: could not register reset controller: %d\n",
		       __func__, ret);
		kfree(rstc->id_ref);
		kfree(rstc);
	}
}
