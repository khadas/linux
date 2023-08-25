// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 Endless Mobile, Inc.
 * Author: Carlo Caione <carlo@endlessm.com>
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <linux/of.h>
#include <linux/of_address.h>
#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/amlogic/glb_timer.h>
#endif

#define NUM_CHANNEL 8
#define MAX_INPUT_MUX 256

#define REG_EDGE_POL	0x00
#define REG_PIN_03_SEL	0x04
#define REG_PIN_47_SEL	0x08
#define REG_FILTER_SEL	0x0c

/* use for A1 like chips */
#define REG_PIN_A1_SEL	0x04

/*
 * Note: The S905X3 datasheet reports that BOTH_EDGE is controlled by
 * bits 24 to 31. Tests on the actual HW show that these bits are
 * stuck at 0. Bits 8 to 15 are responsive and have the expected
 * effect.
 */
#define REG_EDGE_POL_EDGE(params, x)	BIT((params)->edge_single_offset + (x))
#define REG_EDGE_POL_LOW(params, x)	BIT((params)->pol_low_offset + (x))
#define REG_BOTH_EDGE(params, x)	BIT((params)->edge_both_offset + (x))
#define REG_EDGE_POL_MASK(params, x)    ({		\
		typeof(params) __params = (params);	\
		typeof(x) __x = (x);			\
		REG_EDGE_POL_EDGE(__params, __x) |	\
		REG_EDGE_POL_LOW(__params, __x)  |	\
		REG_BOTH_EDGE(__params, __x); })
#define REG_PIN_SEL_SHIFT(x)	(((x) % 4) * 8)
#define REG_FILTER_SEL_SHIFT(x)	((x) * 4)

#ifdef CONFIG_AMLOGIC_MODIFY
#define REG_PIN_SC2_SEL					0x04
#define REG_EDGE_POL_EXTR				0x1c
#define REG_EDGE_POL_MASK_SC2(x)			\
	({typeof(x) _x = (x); BIT(_x) | BIT(12 + (_x)); })
#endif

struct meson_gpio_irq_controller;
static void meson8_gpio_irq_sel_pin(struct meson_gpio_irq_controller *ctl,
				    unsigned int channel, unsigned long hwirq);
static void meson_gpio_irq_init_dummy(struct meson_gpio_irq_controller *ctl);
static void meson_a1_gpio_irq_sel_pin(struct meson_gpio_irq_controller *ctl,
				      unsigned int channel,
				      unsigned long hwirq);

#ifdef CONFIG_AMLOGIC_MODIFY
static unsigned int
meson_sc2_gpio_irq_sel_type(struct meson_gpio_irq_controller *ctl,
			    unsigned int idx, u32 val);
static unsigned int
meson_p1_gpio_irq_sel_type(struct meson_gpio_irq_controller *ctl,
			    unsigned int idx, u32 val);
#endif

static void meson_a1_gpio_irq_init(struct meson_gpio_irq_controller *ctl);

struct irq_ctl_ops {
	void (*gpio_irq_sel_pin)(struct meson_gpio_irq_controller *ctl,
				 unsigned int channel, unsigned long hwirq);
	void (*gpio_irq_init)(struct meson_gpio_irq_controller *ctl);

#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned int (*gpio_irq_sel_type)(struct meson_gpio_irq_controller *ctl,
					  unsigned int idx, u32 val);
#endif
};

struct meson_gpio_irq_params {
	unsigned int nr_hwirq;
	bool support_edge_both;
	unsigned int edge_both_offset;
	unsigned int edge_single_offset;
	unsigned int pol_low_offset;
	unsigned int pin_sel_mask;
	struct irq_ctl_ops ops;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned int pin_sel_reg_base;
	unsigned int edge_sel_reg;
	unsigned int pol_low_sel_reg;
	unsigned int both_sel_reg;
	u8 channel_num;
#endif
};

#ifndef CONFIG_AMLOGIC_MODIFY
#define INIT_MESON_COMMON(irqs, init, sel)			\
	.nr_hwirq = irqs,					\
	.ops = {						\
		.gpio_irq_init = init,				\
		.gpio_irq_sel_pin = sel,			\
	},

#else
#define INIT_MESON_COMMON(irqs, init, sel)			\
	.nr_hwirq = irqs,					\
	.channel_num = 8,					\
	.ops = {						\
		.gpio_irq_init = init,				\
		.gpio_irq_sel_pin = sel,			\
	},

#define INIT_MESON_SC2_COMMON(irqs, init, sel, type)		\
	.nr_hwirq = irqs,					\
	.ops = {						\
		.gpio_irq_init = init,				\
		.gpio_irq_sel_pin = sel,			\
		.gpio_irq_sel_type = type,			\
	},
#endif

#define INIT_MESON8_COMMON_DATA(irqs)				\
	INIT_MESON_COMMON(irqs, meson_gpio_irq_init_dummy,	\
			  meson8_gpio_irq_sel_pin)		\
	.edge_single_offset = 0,				\
	.pol_low_offset = 16,					\
	.pin_sel_mask = 0xff,					\

#define INIT_MESON_A1_COMMON_DATA(irqs)				\
	INIT_MESON_COMMON(irqs, meson_a1_gpio_irq_init,		\
			  meson_a1_gpio_irq_sel_pin)		\
	.support_edge_both = true,				\
	.edge_both_offset = 16,					\
	.edge_single_offset = 8,				\
	.pol_low_offset = 0,					\
	.pin_sel_mask = 0x7f,					\

#ifdef CONFIG_AMLOGIC_MODIFY
/* For sc2/t7 like platform */
#define INIT_MESON_SC2_COMMON_DATA(irqs)			\
	INIT_MESON_SC2_COMMON(irqs, meson_a1_gpio_irq_init,	\
			      meson_a1_gpio_irq_sel_pin,	\
			      meson_sc2_gpio_irq_sel_type)	\
	.support_edge_both = true,				\
	.edge_both_offset = 0,					\
	.edge_single_offset = 12,				\
	.pol_low_offset = 0,					\
	.pin_sel_mask = 0xff,					\
	.pin_sel_reg_base = 0x04,				\
	.channel_num = 12,

#define INIT_MESON_P1_COMMON_DATA(irqs)			\
	INIT_MESON_SC2_COMMON(irqs, meson_a1_gpio_irq_init,	\
			      meson_a1_gpio_irq_sel_pin,	\
			      meson_p1_gpio_irq_sel_type)	\
	.support_edge_both = true,				\
	.edge_both_offset = 0,					\
	.edge_single_offset = 0,				\
	.pol_low_offset = 0,					\
	.pin_sel_mask = 0xff,					\
	.pin_sel_reg_base = 0x10,				\
	.edge_sel_reg = 0x04,					\
	.pol_low_sel_reg = 0x08,				\
	.both_sel_reg =  0x0c,					\
	.channel_num = 32,

#define INIT_MESON_A4_AO_COMMON_DATA(irqs)			\
	INIT_MESON_SC2_COMMON(irqs, meson_a1_gpio_irq_init,	\
			      meson_a1_gpio_irq_sel_pin,	\
			      meson_p1_gpio_irq_sel_type)	\
	.support_edge_both = true,				\
	.edge_both_offset = 0,					\
	.edge_single_offset = 12,				\
	.pol_low_offset = 0,					\
	.pin_sel_mask = 0xff,					\
	.pin_sel_reg_base = 0x04,				\
	.edge_sel_reg = 0x00,					\
	.pol_low_sel_reg = 0x00,				\
	.both_sel_reg =  0x08,					\
	.channel_num = 2,

#endif

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct meson_gpio_irq_params meson8_params = {
	INIT_MESON8_COMMON_DATA(134)
};

static const struct meson_gpio_irq_params meson8b_params = {
	INIT_MESON8_COMMON_DATA(119)
};

static const struct meson_gpio_irq_params gxbb_params = {
	INIT_MESON8_COMMON_DATA(133)
};

static const struct meson_gpio_irq_params gxl_params = {
	INIT_MESON8_COMMON_DATA(110)
};

static const struct meson_gpio_irq_params a1_params = {
	INIT_MESON_A1_COMMON_DATA(62)
};
#endif

static const struct meson_gpio_irq_params axg_params = {
	INIT_MESON8_COMMON_DATA(100)
};

static const struct meson_gpio_irq_params sm1_params = {
	INIT_MESON8_COMMON_DATA(100)
	.support_edge_both = true,
	.edge_both_offset = 8,
};

#ifdef CONFIG_AMLOGIC_MODIFY
static const struct meson_gpio_irq_params tm2_params = {
	INIT_MESON8_COMMON_DATA(104)
	.support_edge_both = true,
	.edge_both_offset = 8,
};

static const struct meson_gpio_irq_params sc2_params = {
	INIT_MESON_SC2_COMMON_DATA(87)
};

static const struct meson_gpio_irq_params t5d_params = {
	INIT_MESON8_COMMON_DATA(113)
	.support_edge_both = true,
	.edge_both_offset = 8,
};

static const struct meson_gpio_irq_params t7_params = {
	INIT_MESON_SC2_COMMON_DATA(157)
};

static const struct meson_gpio_irq_params s4_params = {
	INIT_MESON_SC2_COMMON_DATA(82)
};

static const struct meson_gpio_irq_params t3_params = {
	INIT_MESON_SC2_COMMON_DATA(139)
};

static const struct meson_gpio_irq_params p1_params = {
	INIT_MESON_P1_COMMON_DATA(230)
};

static const struct meson_gpio_irq_params t5w_params = {
	INIT_MESON8_COMMON_DATA(130)
	.support_edge_both = true,
	.edge_both_offset = 8,
};

static const struct meson_gpio_irq_params a5_params = {
	INIT_MESON_SC2_COMMON_DATA(99)
};

static const struct meson_gpio_irq_params s5_params = {
	INIT_MESON_P1_COMMON_DATA(118)
};

static const struct meson_gpio_irq_params a4_params = {
	INIT_MESON_SC2_COMMON_DATA(81)
};

static const struct meson_gpio_irq_params a4_ao_params = {
	INIT_MESON_A4_AO_COMMON_DATA(8)
};

static const struct meson_gpio_irq_params tl1_params = {
	INIT_MESON8_COMMON_DATA(102)
	.support_edge_both = true,
	.edge_both_offset = 8,
};

#endif

static const struct of_device_id meson_irq_gpio_matches[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{ .compatible = "amlogic,meson8-gpio-intc", .data = &meson8_params },
	{ .compatible = "amlogic,meson8b-gpio-intc", .data = &meson8b_params },
	{ .compatible = "amlogic,meson-gxbb-gpio-intc", .data = &gxbb_params },
	{ .compatible = "amlogic,meson-gxl-gpio-intc", .data = &gxl_params },
	{ .compatible = "amlogic,meson-axg-gpio-intc", .data = &axg_params },
	{ .compatible = "amlogic,meson-a1-gpio-intc", .data = &a1_params },
#endif
	{ .compatible = "amlogic,meson-g12a-gpio-intc", .data = &axg_params },
	{ .compatible = "amlogic,meson-sm1-gpio-intc", .data = &sm1_params },
#ifdef CONFIG_AMLOGIC_MODIFY
	{ .compatible = "amlogic,meson-tm2-gpio-intc", .data = &tm2_params },
	{ .compatible = "amlogic,meson-sc2-gpio-intc", .data = &sc2_params },
	{ .compatible = "amlogic,meson-t5d-gpio-intc", .data = &t5d_params },
	{ .compatible = "amlogic,meson-t7-gpio-intc", .data = &t7_params },
	{ .compatible = "amlogic,meson-s4-gpio-intc", .data = &s4_params },
	{ .compatible = "amlogic,meson-t3-gpio-intc", .data = &t3_params },
	{ .compatible = "amlogic,meson-p1-gpio-intc", .data = &p1_params },
	{ .compatible = "amlogic,meson-t5w-gpio-intc", .data = &t5w_params },
	{ .compatible = "amlogic,meson-a5-gpio-intc", .data = &a5_params },
	{ .compatible = "amlogic,meson-s5-gpio-intc", .data = &s5_params },
	{ .compatible = "amlogic,meson-a4-gpio-intc", .data = &a4_params },
	{ .compatible = "amlogic,meson-a4-gpio-ao-intc", .data = &a4_ao_params },
	{ .compatible = "amlogic,meson-tl1-gpio-intc", .data = &tl1_params },
#endif
	{ }
};

#ifdef CONFIG_AMLOGIC_MODIFY
struct meson_gpio_irq_controller *gclt;
#endif

struct meson_gpio_irq_controller {
	const struct meson_gpio_irq_params *params;
	void __iomem *base;
#ifndef CONFIG_AMLOGIC_MODIFY
	u32 channel_irqs[NUM_CHANNEL];
	DECLARE_BITMAP(channel_map, NUM_CHANNEL);
#else
	u32 *channel_irqs;
	unsigned long *channel_map;
	u8 channel_num;
#endif
	spinlock_t lock;
};

static void meson_gpio_irq_update_bits(struct meson_gpio_irq_controller *ctl,
				       unsigned int reg, u32 mask, u32 val)
{
	u32 tmp;

	tmp = readl_relaxed(ctl->base + reg);
	tmp &= ~mask;
	tmp |= val;
	writel_relaxed(tmp, ctl->base + reg);
}

static void meson_gpio_irq_init_dummy(struct meson_gpio_irq_controller *ctl)
{
}

static void meson8_gpio_irq_sel_pin(struct meson_gpio_irq_controller *ctl,
				    unsigned int channel, unsigned long hwirq)
{
	unsigned int reg_offset;
	unsigned int bit_offset;

	reg_offset = (channel < 4) ? REG_PIN_03_SEL : REG_PIN_47_SEL;
	bit_offset = REG_PIN_SEL_SHIFT(channel);

	meson_gpio_irq_update_bits(ctl, reg_offset,
				   ctl->params->pin_sel_mask << bit_offset,
				   hwirq << bit_offset);
}

static void meson_a1_gpio_irq_sel_pin(struct meson_gpio_irq_controller *ctl,
				      unsigned int channel,
				      unsigned long hwirq)
{
	unsigned int reg_offset;
	unsigned int bit_offset;

	bit_offset = ((channel % 2) == 0) ? 0 : 16;
#ifndef CONFIG_AMLOGIC_MODIFY
	reg_offset = REG_PIN_A1_SEL + ((channel / 2) << 2);
#else
	reg_offset = ctl->params->pin_sel_reg_base + ((channel / 2) << 2);
#endif

	meson_gpio_irq_update_bits(ctl, reg_offset,
				   ctl->params->pin_sel_mask << bit_offset,
				   hwirq << bit_offset);
}

/* For a1 or later chips like a1 there is a switch to enable/disable irq */
static void meson_a1_gpio_irq_init(struct meson_gpio_irq_controller *ctl)
{
	meson_gpio_irq_update_bits(ctl, REG_EDGE_POL, BIT(31), BIT(31));
}

#ifdef CONFIG_AMLOGIC_MODIFY
static unsigned int
meson_sc2_gpio_irq_sel_type(struct meson_gpio_irq_controller *ctl,
			    unsigned int idx, unsigned int type)
{
	unsigned int val = 0;
	unsigned long flags;

	meson_gpio_irq_update_bits(ctl, REG_EDGE_POL_EXTR, BIT(0 + (idx)), 0);

	if (type == IRQ_TYPE_EDGE_BOTH) {
		val |= BIT(ctl->params->edge_both_offset + (idx));
		spin_lock_irqsave(&ctl->lock, flags);
		meson_gpio_irq_update_bits(ctl, REG_EDGE_POL_EXTR,
					   BIT(ctl->params->edge_both_offset + (idx)), val);
		spin_unlock_irqrestore(&ctl->lock, flags);
		return 0;
	}

	spin_lock_irqsave(&ctl->lock, flags);

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
		val |= BIT(ctl->params->pol_low_offset + (idx));

	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
		val |= BIT(ctl->params->edge_single_offset  + (idx));

	meson_gpio_irq_update_bits(ctl, REG_EDGE_POL,
				   REG_EDGE_POL_MASK_SC2(idx), val);

	spin_unlock_irqrestore(&ctl->lock, flags);

	return 0;
};

static unsigned int
meson_p1_gpio_irq_sel_type(struct meson_gpio_irq_controller *ctl,
			    unsigned int idx, unsigned int type)
{
	unsigned int val = 0;
	unsigned long flags;
	const struct meson_gpio_irq_params *params;

	params = ctl->params;

	meson_gpio_irq_update_bits(ctl, params->both_sel_reg, BIT(0 + (idx)), 0);

	if (type == IRQ_TYPE_EDGE_BOTH) {
		val |= BIT(params->edge_both_offset + (idx));
		spin_lock_irqsave(&ctl->lock, flags);
		meson_gpio_irq_update_bits(ctl, params->both_sel_reg,
					   BIT(params->edge_both_offset + (idx)), val);
		spin_unlock_irqrestore(&ctl->lock, flags);
		return 0;
	}

	spin_lock_irqsave(&ctl->lock, flags);

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING)) {
		val = 0;
		val |= BIT(params->pol_low_offset + (idx));
		meson_gpio_irq_update_bits(ctl, params->pol_low_sel_reg, BIT(idx), val);
	}

	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)) {
		val = 0;
		val |= BIT(params->edge_single_offset  + (idx));
		meson_gpio_irq_update_bits(ctl, params->edge_sel_reg, BIT(idx), val);
	}

	spin_unlock_irqrestore(&ctl->lock, flags);

	return 0;
};
#endif

static int
meson_gpio_irq_request_channel(struct meson_gpio_irq_controller *ctl,
			       unsigned long  hwirq,
			       u32 **channel_hwirq)
{
	unsigned int idx;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned long flags;
#endif

#ifndef CONFIG_AMLOGIC_MODIFY
	spin_lock(&ctl->lock);
#else
	spin_lock_irqsave(&ctl->lock, flags);
#endif
	/* Find a free channel */
#ifndef CONFIG_AMLOGIC_MODIFY
	idx = find_first_zero_bit(ctl->channel_map, NUM_CHANNEL);
	if (idx >= NUM_CHANNEL) {
		spin_unlock(&ctl->lock);
#else
	idx = find_first_zero_bit(ctl->channel_map, ctl->channel_num);
	if (idx >= ctl->channel_num) {
		spin_unlock_irqrestore(&ctl->lock, flags);
#endif
		pr_err("No channel available\n");
		return -ENOSPC;
	}

	/* Mark the channel as used */
	set_bit(idx, ctl->channel_map);

	/*
	 * Setup the mux of the channel to route the signal of the pad
	 * to the appropriate input of the GIC
	 */
	ctl->params->ops.gpio_irq_sel_pin(ctl, idx, hwirq);

	/*
	 * Get the hwirq number assigned to this channel through
	 * a pointer the channel_irq table. The added benifit of this
	 * method is that we can also retrieve the channel index with
	 * it, using the table base.
	 */
	*channel_hwirq = &(ctl->channel_irqs[idx]);

#ifndef CONFIG_AMLOGIC_MODIFY
	spin_unlock(&ctl->lock);
#else
	spin_unlock_irqrestore(&ctl->lock, flags);
#endif

	pr_debug("hwirq %lu assigned to channel %d - irq %u\n",
		 hwirq, idx, **channel_hwirq);

	return 0;
}

static unsigned int
meson_gpio_irq_get_channel_idx(struct meson_gpio_irq_controller *ctl,
			       u32 *channel_hwirq)
{
	return channel_hwirq - ctl->channel_irqs;
}

static void
meson_gpio_irq_release_channel(struct meson_gpio_irq_controller *ctl,
			       u32 *channel_hwirq)
{
	unsigned int idx;

	idx = meson_gpio_irq_get_channel_idx(ctl, channel_hwirq);
	clear_bit(idx, ctl->channel_map);
}

#ifdef CONFIG_AMLOGIC_MODIFY
unsigned int gpio_irq_get_channel_idx(int irq)
{
	struct irq_desc *desc;
	u32 *channel_hwirq;

	desc = irq_to_desc(irq);
	if (!desc)
		return -EINVAL;

	channel_hwirq = irq_data_get_irq_chip_data(irq_desc_get_irq_data(desc));

	return meson_gpio_irq_get_channel_idx(gclt, channel_hwirq);
}
EXPORT_SYMBOL(gpio_irq_get_channel_idx);
#endif

static int meson_gpio_irq_type_setup(struct meson_gpio_irq_controller *ctl,
				     unsigned int type,
				     u32 *channel_hwirq)
{
	u32 val = 0;
	unsigned int idx;
	const struct meson_gpio_irq_params *params;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned long flags;
#endif

	params = ctl->params;
	idx = meson_gpio_irq_get_channel_idx(ctl, channel_hwirq);

	/*
	 * The controller has a filter block to operate in either LEVEL or
	 * EDGE mode, then signal is sent to the GIC. To enable LEVEL_LOW and
	 * EDGE_FALLING support (which the GIC does not support), the filter
	 * block is also able to invert the input signal it gets before
	 * providing it to the GIC.
	 */
	type &= IRQ_TYPE_SENSE_MASK;

	/*
	 * New controller support EDGE_BOTH trigger. This setting takes
	 * precedence over the other edge/polarity settings
	 */
#ifdef CONFIG_AMLOGIC_MODIFY
	if (params->ops.gpio_irq_sel_type)
		return params->ops.gpio_irq_sel_type(ctl, idx, type);
#endif
	if (type == IRQ_TYPE_EDGE_BOTH) {
		if (!params->support_edge_both)
			return -EINVAL;

		val |= REG_BOTH_EDGE(params, idx);
	} else {
		if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
			val |= REG_EDGE_POL_EDGE(params, idx);

		if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
			val |= REG_EDGE_POL_LOW(params, idx);
	}

#ifndef CONFIG_AMLOGIC_MODIFY
	spin_lock(&ctl->lock);
#else
	spin_lock_irqsave(&ctl->lock, flags);
#endif

	meson_gpio_irq_update_bits(ctl, REG_EDGE_POL,
				   REG_EDGE_POL_MASK(params, idx), val);

#ifndef CONFIG_AMLOGIC_MODIFY
	spin_unlock(&ctl->lock);
#else
	spin_unlock_irqrestore(&ctl->lock, flags);
#endif

	return 0;
}

static unsigned int meson_gpio_irq_type_output(unsigned int type)
{
	unsigned int sense = type & IRQ_TYPE_SENSE_MASK;

	type &= ~IRQ_TYPE_SENSE_MASK;

	/*
	 * The polarity of the signal provided to the GIC should always
	 * be high.
	 */
	if (sense & (IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW))
		type |= IRQ_TYPE_LEVEL_HIGH;
	else
		type |= IRQ_TYPE_EDGE_RISING;

	return type;
}

static int meson_gpio_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct meson_gpio_irq_controller *ctl = data->domain->host_data;
	u32 *channel_hwirq = irq_data_get_irq_chip_data(data);
	int ret;

	ret = meson_gpio_irq_type_setup(ctl, type, channel_hwirq);
	if (ret)
		return ret;

	return irq_chip_set_type_parent(data,
					meson_gpio_irq_type_output(type));
}

static struct irq_chip meson_gpio_irq_chip = {
	.name			= "meson-gpio-irqchip",
	.irq_mask		= irq_chip_mask_parent,
	.irq_unmask		= irq_chip_unmask_parent,
	.irq_eoi		= irq_chip_eoi_parent,
	.irq_set_type		= meson_gpio_irq_set_type,
	.irq_retrigger		= irq_chip_retrigger_hierarchy,
#ifdef CONFIG_SMP
	.irq_set_affinity	= irq_chip_set_affinity_parent,
#endif
	.flags			= IRQCHIP_SET_TYPE_MASKED | IRQCHIP_SKIP_SET_WAKE,
};

static int meson_gpio_irq_domain_translate(struct irq_domain *domain,
					   struct irq_fwspec *fwspec,
					   unsigned long *hwirq,
					   unsigned int *type)
{
	if (is_of_node(fwspec->fwnode) && fwspec->param_count == 2) {
		*hwirq	= fwspec->param[0];
		*type	= fwspec->param[1];
		return 0;
	}

	return -EINVAL;
}

static int meson_gpio_irq_allocate_gic_irq(struct irq_domain *domain,
					   unsigned int virq,
					   u32 hwirq,
					   unsigned int type)
{
	struct irq_fwspec fwspec;

	fwspec.fwnode = domain->parent->fwnode;
	fwspec.param_count = 3;
	fwspec.param[0] = 0;	/* SPI */
	fwspec.param[1] = hwirq;
	fwspec.param[2] = meson_gpio_irq_type_output(type);

	return irq_domain_alloc_irqs_parent(domain, virq, 1, &fwspec);
}

static int meson_gpio_irq_domain_alloc(struct irq_domain *domain,
				       unsigned int virq,
				       unsigned int nr_irqs,
				       void *data)
{
	struct irq_fwspec *fwspec = data;
	struct meson_gpio_irq_controller *ctl = domain->host_data;
	unsigned long hwirq;
	u32 *channel_hwirq;
	unsigned int type;
	int ret;

	if (WARN_ON(nr_irqs != 1))
		return -EINVAL;

	ret = meson_gpio_irq_domain_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	ret = meson_gpio_irq_request_channel(ctl, hwirq, &channel_hwirq);
	if (ret)
		return ret;

	ret = meson_gpio_irq_allocate_gic_irq(domain, virq,
					      *channel_hwirq, type);
	if (ret < 0) {
		pr_err("failed to allocate gic irq %u\n", *channel_hwirq);
		meson_gpio_irq_release_channel(ctl, channel_hwirq);
		return ret;
	}

	irq_domain_set_hwirq_and_chip(domain, virq, hwirq,
				      &meson_gpio_irq_chip, channel_hwirq);

	return 0;
}

static void meson_gpio_irq_domain_free(struct irq_domain *domain,
				       unsigned int virq,
				       unsigned int nr_irqs)
{
	struct meson_gpio_irq_controller *ctl = domain->host_data;
	struct irq_data *irq_data;
	u32 *channel_hwirq;

	if (WARN_ON(nr_irqs != 1))
		return;

	irq_domain_free_irqs_parent(domain, virq, 1);

	irq_data = irq_domain_get_irq_data(domain, virq);
	channel_hwirq = irq_data_get_irq_chip_data(irq_data);

	meson_gpio_irq_release_channel(ctl, channel_hwirq);
}

static const struct irq_domain_ops meson_gpio_irq_domain_ops = {
	.alloc		= meson_gpio_irq_domain_alloc,
	.free		= meson_gpio_irq_domain_free,
	.translate	= meson_gpio_irq_domain_translate,
};

static int __init meson_gpio_irq_parse_dt(struct device_node *node,
					  struct meson_gpio_irq_controller *ctl)
{
	const struct of_device_id *match;
	int ret;

	match = of_match_node(meson_irq_gpio_matches, node);
	if (!match)
		return -ENODEV;

	ctl->params = match->data;

#ifndef CONFIG_AMLOGIC_MODIFY
	ret = of_property_read_variable_u32_array(node,
						  "amlogic,channel-interrupts",
						  ctl->channel_irqs,
						  NUM_CHANNEL,
						  NUM_CHANNEL);
	if (ret < 0) {
		pr_err("can't get %d channel interrupts\n", NUM_CHANNEL);
		return ret;
	}
#else
	ctl->channel_num = ctl->params->channel_num;
	ctl->channel_irqs = kcalloc(ctl->channel_num,
				    sizeof(*ctl->channel_irqs), GFP_KERNEL);
	ctl->channel_map = bitmap_zalloc(ctl->params->channel_num, GFP_KERNEL);

	if (!ctl->channel_irqs || !ctl->channel_map) {
		pr_err("allocate  irqs and channel maps mem fail\n");
		return -ENOMEM;
	}

	ret = of_property_read_variable_u32_array(node,
						  "amlogic,channel-interrupts",
						  ctl->channel_irqs,
						  ctl->channel_num,
						  ctl->channel_num);
	if (ret < 0) {
		pr_err("can't get %d channel interrupts\n", ctl->channel_num);
		kfree(ctl->channel_irqs);
		ctl->channel_irqs = NULL;
		bitmap_free(ctl->channel_map);
		ctl->channel_map = NULL;
		return ret;
	}
#endif

	ctl->params->ops.gpio_irq_init(ctl);

	return 0;
}

static int __init meson_gpio_irq_of_init(struct device_node *node,
					 struct device_node *parent)
{
	struct irq_domain *domain, *parent_domain;
	struct meson_gpio_irq_controller *ctl;
	int ret;

	if (!parent) {
		pr_err("missing parent interrupt node\n");
		return -ENODEV;
	}

	parent_domain = irq_find_host(parent);
	if (!parent_domain) {
		pr_err("unable to obtain parent domain\n");
		return -ENXIO;
	}

	ctl = kzalloc(sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return -ENOMEM;

	spin_lock_init(&ctl->lock);

	ctl->base = of_iomap(node, 0);
	if (!ctl->base) {
		ret = -ENOMEM;
		goto free_ctl;
	}

	ret = meson_gpio_irq_parse_dt(node, ctl);
	if (ret)
		goto free_channel_irqs;

	domain = irq_domain_create_hierarchy(parent_domain, 0,
					     ctl->params->nr_hwirq,
					     of_node_to_fwnode(node),
					     &meson_gpio_irq_domain_ops,
					     ctl);
	if (!domain) {
		pr_err("failed to add domain\n");
		ret = -ENODEV;
		goto free_channel_irqs;
	}
#ifndef CONFIG_AMLOGIC_MODIFY
	pr_info("%d to %d gpio interrupt mux initialized\n",
		ctl->params->nr_hwirq, NUM_CHANNEL);
#else
	pr_info("%d to %d gpio interrupt mux initialized\n",
		ctl->params->nr_hwirq, ctl->channel_num);
	gclt = ctl;
#endif

	return 0;

free_channel_irqs:
	iounmap(ctl->base);
free_ctl:
	kfree(ctl);

	return ret;
}

IRQCHIP_DECLARE(meson_gpio_ao_intc, "amlogic,meson-gpio-ao-intc",
		meson_gpio_irq_of_init);
IRQCHIP_DECLARE(meson_gpio_intc, "amlogic,meson-gpio-intc",
		meson_gpio_irq_of_init);
