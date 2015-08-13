/*
 * drivers/amlogic/clocksource/meson_timer.c
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <asm/memory.h>
#include <linux/sched_clock.h>
#include <linux/of.h>
#include <asm/smp_plat.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/cpu.h>

#undef pr_fmt
#define pr_fmt(fmt) "meson_timer: " fmt

/***********************************************************************
 * System timer
 **********************************************************************/
#define TIMER_E_RESOLUTION_BIT         8
#define TIMER_E_ENABLE_BIT        20
#define TIMER_E_RESOLUTION_MASK       (7UL << TIMER_E_RESOLUTION_BIT)
#define TIMER_E_RESOLUTION_SYS           0
#define TIMER_E_RESOLUTION_1us           1
#define TIMER_E_RESOLUTION_10us          2
#define TIMER_E_RESOLUTION_100us         3
#define TIMER_E_RESOLUTION_1ms           4

#define TIMER_DI_RESOLUTION_BIT         6
#define TIMER_CH_RESOLUTION_BIT         4
#define TIMER_BG_RESOLUTION_BIT         2
#define TIMER_AF_RESOLUTION_BIT         0

#define TIMER_DI_ENABLE_BIT         19
#define TIMER_CH_ENABLE_BIT         18
#define TIMER_BG_ENABLE_BIT         17
#define TIMER_AF_ENABLE_BIT         16

#define TIMER_DI_MODE_BIT         15
#define TIMER_CH_MODE_BIT         14
#define TIMER_BG_MODE_BIT         13
#define TIMER_AF_MODE_BIT         12

#define TIMER_RESOLUTION_1us            0
#define TIMER_RESOLUTION_10us           1
#define TIMER_RESOLUTION_100us          2
#define TIMER_RESOLUTION_1ms            3


static DEFINE_PER_CPU(struct clock_event_device, percpu_clockevent);
static DEFINE_PER_CPU(struct meson_clock, percpu_mesonclock);
void __iomem *timer_ctrl_base;
void __iomem *clocksrc_base;

static inline void aml_set_reg32_mask(void __iomem *_reg, const uint32_t _mask)
{
	uint32_t _val;

	_val = readl_relaxed(_reg) | _mask;

	writel_relaxed(_val , _reg);
}

static inline void aml_write_reg32(void __iomem *_reg, const uint32_t _value)
{
	writel_relaxed(_value, _reg);
}

static inline void aml_clr_reg32_mask(void __iomem *_reg, const uint32_t _mask)
{
	writel_relaxed((readl_relaxed(_reg) & (~(_mask))), _reg);
}

static inline void
aml_set_reg32_bits(void __iomem *_reg, const uint32_t _value,
		const uint32_t _start, const uint32_t _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
					~(((1L << (_len))-1) << (_start))) |
				(((_value)&((1L<<(_len))-1)) << (_start))),
				_reg);
}


/********** Clock Source Device, Timer-E *********/
static cycle_t cycle_read_timerE(struct clocksource *cs)
{
	return (cycle_t) readl_relaxed(clocksrc_base);
}
static unsigned long cycle_read_timerE1(void)
{
	return (unsigned long) readl_relaxed(clocksrc_base);
}
static struct clocksource clocksource_timer_e = {
	.name   = "Timer-E",
	.rating = 300,
	.read   = cycle_read_timerE,
	.mask   = CLOCKSOURCE_MASK(32),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct delay_timer aml_delay_timer;

static u32 notrace meson8_read_sched_clock(void)
{
	return (u32)readl_relaxed(clocksrc_base);
}

static void __init meson_clocksource_init(void)
{
	aml_clr_reg32_mask(timer_ctrl_base, TIMER_E_RESOLUTION_MASK);
	aml_set_reg32_mask(timer_ctrl_base,
			TIMER_E_RESOLUTION_1us << TIMER_E_RESOLUTION_BIT);

	clocksource_timer_e.shift = 22;
	clocksource_timer_e.mult = 4194304000u;
	clocksource_register_khz(&clocksource_timer_e, 1000);
	setup_sched_clock(meson8_read_sched_clock, 32, USEC_PER_SEC);
}

/********** Clock Event Device, Timer-ABCD/FGHI *********/

struct meson_clock {
	struct irqaction	irq;
	const char *name;	/*A,B,C,D,F,G,H,I*/
	int	bit_enable;
	int	bit_mode;
	int	bit_resolution;
	void __iomem *mux_reg;
	void __iomem *reg;
	unsigned int init_flag;
};

static irqreturn_t meson_timer_interrupt(int irq, void *dev_id);
static int meson_set_next_event(unsigned long evt,
				struct clock_event_device *dev);
static void meson_clkevt_set_mode(enum clock_event_mode mode,
				  struct clock_event_device *dev);


#if 0
static struct meson_clock *clockevent_to_clock(struct clock_event_device *evt)
{
	/* return container_of(evt, struct meson_clock, clockevent); */
	return (struct meson_clock *)evt->private;
}
#endif
static DEFINE_SPINLOCK(time_lock);

static void meson_clkevt_set_mode(enum clock_event_mode mode,
				  struct clock_event_device *dev)
{
	int cpuidx = smp_processor_id();
	struct meson_clock *clk =  &per_cpu(percpu_mesonclock, cpuidx);

	spin_lock(&time_lock);
	switch (mode) {
	case CLOCK_EVT_MODE_RESUME:
		/* printk(KERN_INFO"Resume timer%s\n", dev->name); */
		aml_set_reg32_bits(clk->mux_reg, 1,
				clk->bit_enable, 1);
	break;

	case CLOCK_EVT_MODE_PERIODIC:
		aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_mode, 1);
		aml_set_reg32_bits(clk->mux_reg, 1,
				clk->bit_enable, 1);
		/* printk("Periodic timer %s!,mux_reg=%x\n", \
		 dev->name,readl_relaxed(clk->mux_reg)); */
	break;

	case CLOCK_EVT_MODE_ONESHOT:
		aml_set_reg32_bits(clk->mux_reg, 0, clk->bit_mode, 1);
		aml_set_reg32_bits(clk->mux_reg, 1,
				clk->bit_enable, 1);
		/* pr_info("One shot timer %s!mux_reg=%x\n", \
		  dev->name,readl_relaxed(clk->mux_reg)); */
	break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		/* pr_info("Disable timer %p %s\n",dev,dev->name); */
		aml_set_reg32_bits(clk->mux_reg, 0,
				clk->bit_enable, 1);
	break;
	}
	spin_unlock(&time_lock);
}
static int meson_set_next_event(unsigned long evt,
				struct clock_event_device *dev)
{
	int cpuidx = smp_processor_id();
	struct meson_clock *clk =  &per_cpu(percpu_mesonclock, cpuidx);
	/* use a big number to clear previous trigger cleanly */
	aml_set_reg32_mask(clk->reg, evt & 0xffff);
	/* then set next event */
	aml_set_reg32_bits(clk->reg, evt, 0, 16);
	return 0;
}


/* Clock event timer interrupt handler */
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	if (evt == NULL || evt->event_handler == NULL) {
		WARN_ONCE(evt == NULL || evt->event_handler == NULL,
			"%p %s %p %d",
			evt, evt?evt->name:NULL,
			evt?evt->event_handler:NULL, irq);
		return IRQ_HANDLED;
	}
	evt->event_handler(evt);
	return IRQ_HANDLED;

}
static void  meson_timer_init_device(struct clock_event_device *evt)
{
	evt->mult = div_sc(1000000, NSEC_PER_SEC, 20);
	evt->max_delta_ns =
		clockevent_delta2ns(0xfffe, evt);
	evt->min_delta_ns = clockevent_delta2ns(1, evt);
	evt->cpumask = cpumask_of(smp_processor_id());
}
void local_timer_setup_data(int cpuidx)
{
		phandle phandle =  -1;
		u32 hwid = 0;
		struct device_node *cpu, *cpus, *timer;
		struct meson_clock *mclk = &per_cpu(percpu_mesonclock, cpuidx);
		struct clock_event_device *clock_evt =
			&per_cpu(percpu_clockevent, cpuidx);
		cpus = of_find_node_by_path("/cpus");
		if (!cpus)
			return;
		for_each_child_of_node(cpus, cpu) {
			if (hwid == cpuidx)
				break;
			hwid++;
		}
		if (hwid != cpuidx)
			cpu = of_get_next_child(cpus, NULL);

		if (of_property_read_u32(cpu, "timer", &phandle)) {
			pr_info(" * missing timer property\n");
				return;
		}
		timer = of_find_node_by_phandle(phandle);
		if (!timer) {
			pr_info(" * %s missing timer phandle\n",
						 cpu->full_name);
			return;
		}
		if (of_property_read_string(timer, "timer_name", &clock_evt->name))
			return;

		if (of_property_read_u32(timer, "clockevent-rating",
					&clock_evt->rating))
			return;

		if (of_property_read_u32(timer, "clockevent-shift",
					&clock_evt->shift))
			return;

		if (of_property_read_u32(timer, "clockevent-features",
					&clock_evt->features))
			return;

		if (of_property_read_u32(timer, "bit_enable", &mclk->bit_enable))
			return;

		if (of_property_read_u32(timer, "bit_mode", &mclk->bit_mode))
			return;

		if (of_property_read_u32(timer, "bit_resolution",
					&mclk->bit_resolution))
			return;


		mclk->mux_reg = timer_ctrl_base;
		mclk->reg = of_iomap(timer, 0);
		pr_info("%s mclk->mux_reg =%p,mclk->reg =%p\n",
				clock_evt->name, mclk->mux_reg, mclk->reg);
		mclk->irq.irq = irq_of_parse_and_map(timer, 0);
		setup_irq(mclk->irq.irq, &mclk->irq);

}
void  clockevent_local_timer(void)
	{
		int cpuidx = smp_processor_id();

		struct meson_clock *mclk = &per_cpu(percpu_mesonclock, cpuidx);
		struct clock_event_device *clock_evt =
			&per_cpu(percpu_clockevent, cpuidx);

		aml_set_reg32_mask(mclk->mux_reg,
			((1 << mclk->bit_enable)
			|(1 << mclk->bit_mode)
			|(TIMER_RESOLUTION_1us << mclk->bit_resolution)));
		aml_write_reg32(mclk->reg, 9999);

		meson_timer_init_device(clock_evt);

		clock_evt->set_next_event = meson_set_next_event;
		clock_evt->set_mode = meson_clkevt_set_mode;
		clock_evt->cpumask = cpumask_of(cpuidx);
		clock_evt->irq = mclk->irq.irq;
		mclk->irq.dev_id = clock_evt;
		mclk->irq.handler = meson_timer_interrupt;
		mclk->irq.name = clock_evt->name;
		mclk->irq.flags =
			IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL;
		/* Set up the IRQ handler */
		clockevents_register_device(clock_evt);

		if (cpuidx)
			irq_force_affinity(mclk->irq.irq, cpumask_of(cpuidx));

		enable_percpu_irq(mclk->irq.irq, 0);
		return;
	}

/*
 * This sets up the system timers, clock source and clock event.
 */
void  clockevent_init_and_register(void)
{
	phandle phandle =  -1;
	int cpuidx = smp_processor_id();
	u32 hwid = 0;
	struct device_node *cpu, *cpus, *timer;
	struct meson_clock *mclk = &per_cpu(percpu_mesonclock, cpuidx);
	struct clock_event_device *clock_evt =
		&per_cpu(percpu_clockevent, cpuidx);
	cpus = of_find_node_by_path("/cpus");
	if (!cpus)
		return;
	for_each_child_of_node(cpus, cpu) {
		if (hwid == cpuidx)
			break;
		hwid++;
	}
	if (hwid != cpuidx)
		cpu = of_get_next_child(cpus, NULL);

	if (of_property_read_u32(cpu, "timer", &phandle)) {
		pr_info(" * missing timer property\n");
			return;
	}
	timer = of_find_node_by_phandle(phandle);
	if (!timer) {
		pr_info(" * %s missing timer phandle\n",
				     cpu->full_name);
		return;
	}
	if (of_property_read_string(timer, "timer_name", &clock_evt->name))
		return;

	if (of_property_read_u32(timer, "clockevent-rating",
				&clock_evt->rating))
		return;

	if (of_property_read_u32(timer, "clockevent-shift",
				&clock_evt->shift))
		return;

	if (of_property_read_u32(timer, "clockevent-features",
				&clock_evt->features))
		return;

	if (of_property_read_u32(timer, "bit_enable", &mclk->bit_enable))
		return;

	if (of_property_read_u32(timer, "bit_mode", &mclk->bit_mode))
		return;

	if (of_property_read_u32(timer, "bit_resolution",
				&mclk->bit_resolution))
		return;


	mclk->mux_reg = timer_ctrl_base;
	mclk->reg = of_iomap(timer, 0);
	pr_info("mclk->mux_reg =%p,mclk->reg =%p\n", mclk->mux_reg, mclk->reg);
	mclk->irq.irq = irq_of_parse_and_map(timer, 0);

	aml_set_reg32_mask(mclk->mux_reg,
		((1 << mclk->bit_enable)
		|(1 << mclk->bit_mode)
		|(TIMER_RESOLUTION_1us << mclk->bit_resolution)));
	aml_write_reg32(mclk->reg, 9999);

	meson_timer_init_device(clock_evt);

	clock_evt->set_next_event = meson_set_next_event;
	clock_evt->set_mode = meson_clkevt_set_mode;

	mclk->irq.dev_id = clock_evt;
	mclk->irq.handler = meson_timer_interrupt;
	mclk->irq.name = clock_evt->name;
	mclk->irq.flags =
		IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL|IRQF_TRIGGER_RISING;
	/* Set up the IRQ handler */
	clockevents_register_device(clock_evt);
	setup_irq(mclk->irq.irq, &mclk->irq);
	enable_percpu_irq(mclk->irq.irq, 0);
	return;
}
void   meson_local_timer_stop(void *hcpu)
{
	struct meson_clock *clk;
	int cpuidx = (long)hcpu;
	struct clock_event_device *clock_evt =
		&per_cpu(percpu_clockevent, cpuidx);
	if (!clock_evt) {
		pr_err("meson_local_timer_stop: null evt!\n");
		return;/* return -EINVAL; */
	}
	if (0 == cpuidx)
		BUG();
	clk = &per_cpu(percpu_mesonclock, cpuidx);
	clock_evt->set_mode(CLOCK_EVT_MODE_UNUSED, clock_evt);

	aml_clr_reg32_mask(clk->mux_reg, (1 << clk->bit_enable));

	disable_percpu_irq(clk->irq.irq);
	return;
}
static int  arch_timer_cpu_notify(struct notifier_block *self,
					   unsigned long action, void *hcpu)
{
	/*
	 * Grab cpu pointer in each case to avoid spurious
	 * preemptible warnings
	 */
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		clockevent_local_timer();
		break;
	case CPU_DYING:
		meson_local_timer_stop(hcpu);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block arch_timer_cpu_nb __cpuinitdata = {
	.notifier_call = arch_timer_cpu_notify,
};

void __init meson_timer_init(struct device_node *np)
{
	int err;
	timer_ctrl_base = of_iomap(np, 0);
	clocksrc_base = of_iomap(np, 1);
	meson_clocksource_init();
	clockevent_init_and_register();
	{
		int i;
		for (i = 1; i < nr_cpu_ids; i++)
			local_timer_setup_data(i);
	}
	err = register_cpu_notifier(&arch_timer_cpu_nb);
	if (err)
		return;

	aml_delay_timer.read_current_timer = &cycle_read_timerE1;
	aml_delay_timer.freq = 1000*1000;/* 1us resolution */
	register_current_timer_delay(&aml_delay_timer);
}
CLOCKSOURCE_OF_DECLARE(meson_timer, "arm, meson-timer", meson_timer_init);
