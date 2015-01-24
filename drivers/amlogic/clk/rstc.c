/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
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

struct meson_rstc {
	struct reset_controller_dev	rcdev;
	void __iomem			*ao_base;
	void __iomem			*ot_base;
	unsigned int			num_regs;
	unsigned int			ao_off_id;
	u8				flags;
	spinlock_t			lock;
};

static int meson_rstc_assert(struct reset_controller_dev *rcdev,
			     unsigned long id)
{
	struct meson_rstc *rstc = container_of(rcdev,
					       struct meson_rstc,
					       rcdev);
	int bank = id / BITS_PER_LONG;
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
		offset = id % BITS_PER_LONG;
		rstc_mem = rstc->ot_base + (bank << 2);
	}

	spin_lock_irqsave(&rstc->lock, flags);

	reg = readl(rstc_mem);
	writel(reg & ~BIT(offset), rstc_mem);

	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;
}

static int meson_rstc_deassert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct meson_rstc *rstc = container_of(rcdev,
					       struct meson_rstc,
					       rcdev);
	int bank = id / BITS_PER_LONG;
	int offset;
	void __iomem *rstc_mem;
	unsigned long flags;
	u32 reg;

	if (id >= rstc->ao_off_id) {
		offset = id - rstc->ao_off_id;
		rstc_mem = rstc->ao_base;
	} else {
		offset = id % BITS_PER_LONG;
		rstc_mem = rstc->ot_base + (bank << 2);
	}

	spin_lock_irqsave(&rstc->lock, flags);

	reg = readl(rstc_mem);
	writel(reg | BIT(offset), rstc_mem);

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

void __init meson_register_rstc(struct device_node *np, unsigned int num_regs,
				void __iomem *ao_base, void __iomem *ot_base,
				unsigned int ao_off_id, u8 flags)
{
	struct meson_rstc *rstc;
	int ret;

	rstc = kzalloc(sizeof(*rstc), GFP_KERNEL);
	if (!rstc)
		return;

	spin_lock_init(&rstc->lock);

	rstc->ao_base = ao_base;
	rstc->ot_base = ot_base;
	rstc->num_regs = num_regs;
	rstc->flags = flags;

	rstc->rcdev.owner = THIS_MODULE;
	rstc->rcdev.nr_resets = num_regs * BITS_PER_LONG;
	rstc->rcdev.of_node = np;
	rstc->rcdev.ops = &meson_rstc_ops;
	rstc->ao_off_id = ao_off_id;

	ret = reset_controller_register(&rstc->rcdev);
	if (ret) {
		pr_err("%s: could not register reset controller: %d\n",
		       __func__, ret);
		kfree(rstc);
	}
}
