// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/stat.h>
#include <asm/memory.h>
#include <linux/sched_clock.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/arm-smccc.h>
#ifdef CONFIG_AMLOGIC_CPUIDLE
#include <linux/amlogic/aml_cpuidle.h>
#endif
#include <linux/amlogic/cpu_version.h>

#undef pr_fmt
#define pr_fmt(fmt)   KBUILD_MODNAME ": " fmt

#define TIMER_E_RESOLUTION_BIT         8
#define TIMER_E_ENABLE_BIT        20
#define TIMER_E_RESOLUTION_MASK       (7UL << TIMER_E_RESOLUTION_BIT)

#define TIMER_RESOLUTION_1us            0
#define TIMER_RESOLUTION_10us           1
#define TIMER_RESOLUTION_100us          2
#define TIMER_RESOLUTION_1ms            3

/********** Clock Event Device, Timer-ABCD/FGHI *********/
struct meson_clock {
	const char *name;	/*A,B,C,D,F,G,H,I*/
	int	bit_enable;
	int	bit_mode;
	int	bit_resolution;
	void __iomem *reg;
	unsigned int init_flag;
};

static struct meson_clock bc_clock;
static DEFINE_SPINLOCK(time_lock);

static inline void aml_set_reg32_mask(void __iomem *_reg, const u32 _mask)
{
	u32 _val;

	_val = readl_relaxed(_reg) | _mask;

	writel_relaxed(_val, _reg);
}

static inline void
aml_set_reg32_bits(void __iomem *_reg, const u32 _value,
		   const u32 _start, const u32 _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
			 ~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))), _reg);
}

static int meson_set_next_event(unsigned long evt,
				struct clock_event_device *dev)
{
	struct meson_clock *clk =  &bc_clock;

	if (is_meson_a1_cpu() || is_meson_c1_cpu()) {
		if ((evt & 0xffff) <= 80) {
			/*pr_info("timer counter is too small!\n");*/
			return -1;
		}
	}
	/* use a big number to clear previous trigger cleanly */
	aml_set_reg32_mask(clk->reg + 4, evt & 0xffff);
	/* then set next event */
	aml_set_reg32_bits(clk->reg + 4, evt, 0, 16);
	return 0;
}

static int meson_clkevt_shutdown(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;
	unsigned long flags;

	spin_lock_irqsave(&time_lock, flags);
	/* pr_info("Disable timer %p %s\n",dev,dev->name); */
	aml_set_reg32_bits(clk->reg, 0, clk->bit_enable, 1);
	spin_unlock_irqrestore(&time_lock, flags);
	return 0;
}

static int meson_clkevt_set_periodic(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;
	unsigned long flags;

	spin_lock_irqsave(&time_lock, flags);
	aml_set_reg32_bits(clk->reg, 1, clk->bit_mode, 1);
	aml_set_reg32_bits(clk->reg, 1, clk->bit_enable, 1);
	/* pr_info("Periodic timer %s!,reg=%x\n",*/
	/* dev->name,readl_relaxed(clk->reg)); */
	spin_unlock_irqrestore(&time_lock, flags);
	return 0;
}

static int meson_clkevt_set_oneshot(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;
	unsigned long flags;

	spin_lock_irqsave(&time_lock, flags);
	aml_set_reg32_bits(clk->reg, 0, clk->bit_mode, 1);
	aml_set_reg32_bits(clk->reg, 1, clk->bit_enable, 1);
	/* pr_info("One shot timer %s!reg=%x\n",*/
	/* dev->name,readl_relaxed(clk->reg)); */
	spin_unlock_irqrestore(&time_lock, flags);
	return 0;
}

static int meson_clkevt_resume(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;
	unsigned long flags;

	spin_lock_irqsave(&time_lock, flags);
	/* pr_info("Resume timer%s\n", dev->name); */
	aml_set_reg32_bits(clk->reg, 1, clk->bit_enable, 1);
	spin_unlock_irqrestore(&time_lock, flags);
	return 0;
}

/* Clock event timer interrupt handler */
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	struct arm_smccc_res res __attribute__((unused));

	if (!evt || !evt->event_handler) {
		WARN_ONCE(!evt || !evt->event_handler,
			  "%p %s %p %d",
			  evt, evt ? evt->name : NULL,
			  evt ? evt->event_handler : NULL, irq);
		return IRQ_HANDLED;
	}
	evt->event_handler(evt);
#ifdef CONFIG_AMLOGIC_CPUIDLE
	do_aml_resume_power();
#endif
	return IRQ_HANDLED;
}

static struct clock_event_device meson_clockevent = {
	.cpumask = cpu_possible_mask,
	.set_next_event = meson_set_next_event,
	.set_state_shutdown = meson_clkevt_shutdown,
	.set_state_periodic = meson_clkevt_set_periodic,
	.set_state_oneshot = meson_clkevt_set_oneshot,
	.tick_resume = meson_clkevt_resume,
};

static int __init meson_bc_timer_init(struct device_node *np)
{
	struct meson_clock *mclk = &bc_clock;
	int irq;
	int ret;
	unsigned int resolution_1us = TIMER_RESOLUTION_1us;
	unsigned int min_delta_ns = 1;

	if (of_property_read_string(np, "timer_name",
				    &meson_clockevent.name)) {
		pr_err("fail to read timer_name\n");
		return -1;
	}

	if (of_property_read_u32(np, "clockevent-rating",
				 &meson_clockevent.rating)) {
		pr_err("failed to read clockevent-rating\n");
		return -1;
	}

	if (of_property_read_u32(np, "clockevent-shift",
				 &meson_clockevent.shift)) {
		pr_err("failed to read clockevent-shift\n");
		return -1;
	}

	if (of_property_read_u32(np, "clockevent-features",
				 &meson_clockevent.features)) {
		pr_err("failed to read clockevent-features\n");
		return -1;
	}

	if (of_property_read_u32(np, "bit_enable", &mclk->bit_enable)) {
		pr_err("failed to read bit_enable\n");
		return -1;
	}

	if (of_property_read_u32(np, "bit_mode", &mclk->bit_mode)) {
		pr_err("failed to read bit_mode\n");
		return -1;
	}

	if (of_property_read_u32(np, "bit_resolution",
				 &mclk->bit_resolution)) {
		pr_err("failed to read bit_resolution\n");
		return -1;
	}
	/* A1 1us resolution value is changed from 0 to 1*/
	of_property_read_u32(np, "resolution_1us", &resolution_1us);

	/* A1/C1 min delta change to 10, default is 1*/
	of_property_read_u32(np, "min_delta_ns", &min_delta_ns);

	mclk->reg = of_iomap(np, 0);
	if (!mclk->reg) {
		pr_err("failed to iomap mux reg\n");
		return -1;
	}

	irq = irq_of_parse_and_map(np, 0);
	if (!irq) {
		pr_err("failed to map irq\n");
		iounmap(mclk->reg);
		return -1;
	}

	aml_set_reg32_mask(mclk->reg,
			   ((1 << mclk->bit_mode)
			    | (resolution_1us << mclk->bit_resolution)));

	meson_clockevent.mult = div_sc(1000000, NSEC_PER_SEC, 20);
	meson_clockevent.max_delta_ns =
		clockevent_delta2ns(0xfffe, &meson_clockevent);
	meson_clockevent.min_delta_ns =
		clockevent_delta2ns(min_delta_ns, &meson_clockevent);
	meson_clockevent.irq = irq;
	clockevents_register_device(&meson_clockevent);

	ret = request_irq(irq,
			  meson_timer_interrupt,
			  IRQF_TIMER | IRQF_IRQPOLL | IRQF_TRIGGER_RISING,
			  meson_clockevent.name,
			  &meson_clockevent);
	if (ret) {
		pr_err("request irq:%d failed=%d\n", irq, ret);
		iounmap(mclk->reg);
		return -1;
	}

	return 0;
}

TIMER_OF_DECLARE(meson_bc_timer, "amlogic,bc-timer", meson_bc_timer_init);
