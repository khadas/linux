/*
 * Meson Power Management Routines
 *
 * Copyright (C) 2010 Amlogic, Inc. http://www.amlogic.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/fs.h>

#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/mach/map.h>
#include <linux/amlogic/iomap.h>

#include <linux/init.h>
#include <linux/of.h>

#include <asm/compiler.h>
#include <linux/errno.h>
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#include <asm/psci.h>
#include <linux/amlogic/pm.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif
/* appf functions */
#define APPF_INITIALIZE             0
#define APPF_POWER_DOWN_CPU         1
#define APPF_POWER_UP_CPUS          2
/* appf flags */
#define APPF_SAVE_PMU          (1<<0)
#define APPF_SAVE_TIMERS       (1<<1)
#define APPF_SAVE_VFP          (1<<2)
#define APPF_SAVE_DEBUG        (1<<3)
#define APPF_SAVE_L2           (1<<4)
struct meson_pm_config {
	void __iomem *pctl_reg_base;
	void __iomem *mmc_reg_base;
	void __iomem *hiu_reg_base;
	unsigned power_key;
	unsigned ddr_clk;
	void __iomem *ddr_reg_backup;
	unsigned core_voltage_adjust;
	int sleepcount;
	void (*set_vccx2)(int power_on);
	void (*set_exgpio_early_suspend)(int power_on);
};

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
};
static int debug_mask = DEBUG_USER_STATE | DEBUG_SUSPEND;

#define ON  1
#define OFF 0

static struct meson_pm_config *pdata;

#define CLK(addr)  \
{ \
	.clk_name = #addr, \
	.clk_addr = addr, \
	.clk_flag = 0, \
}

struct clk_desc {
	char *clk_name;
	unsigned clk_addr;
	unsigned clk_flag;
};
static DEFINE_MUTEX(late_suspend_lock);
static LIST_HEAD(late_suspend_handlers);

struct late_suspend {
	struct list_head link;
	int level;
	void (*suspend)(struct late_suspend *h);
	void (*resume)(struct late_suspend *h);
	void *param;
};

void register_late_suspend(struct late_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&late_suspend_lock);
	list_for_each(pos, &late_suspend_handlers) {
		struct late_suspend *e;
		e = list_entry(pos, struct late_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&late_suspend_lock);
}
EXPORT_SYMBOL(register_late_suspend);

void unregister_late_suspend(struct late_suspend *handler)
{
	mutex_lock(&late_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&late_suspend_lock);
}
EXPORT_SYMBOL(unregister_late_suspend);
static void uart_change_buad(unsigned reg, unsigned clk_rate)
{
	return;
	aml_aobus_update_bits(reg, 0x7FFFFF, 0);
	aml_aobus_update_bits(reg, 0x7FFFFF,
		(((clk_rate / (115200 * 4)) - 1) & 0x7fffff)|(1<<23));
}
#define AO_UART_REG5 ((0x01 << 10) | (0x35 << 2))

void clk_switch(int flag)
{

	if (flag) {
		aml_cbus_update_bits(0x105d, BIT(7), BIT(7));
		udelay(10);
		aml_cbus_update_bits(0x105d, BIT(8), BIT(8));
		udelay(10);
		uart_change_buad(AO_UART_REG5, 141666667);
	} else {
		aml_cbus_update_bits(0x105d, BIT(8), 0);
		udelay(10);
		aml_cbus_update_bits(0x105d, BIT(7), 0);
		udelay(10);
		uart_change_buad(AO_UART_REG5, 24000000);
	}
}
EXPORT_SYMBOL(clk_switch);

static void late_suspend(void)
{
	struct late_suspend *pos;

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_suspend: call handlers\n");
	list_for_each_entry(pos, &late_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			pr_info("%pf\n", pos->suspend);
			pos->suspend(pos);
		}
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_suspend: sync\n");

}

static void early_resume(void)
{
	struct late_suspend *pos;

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: call handlers\n");
	list_for_each_entry_reverse(pos, &late_suspend_handlers, link)
	    if (pos->resume != NULL) {
		pr_info("%pf\n", pos->resume);
		pos->resume(pos);
	}
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: done\n");
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void meson_system_early_suspend(struct early_suspend *h)
{
	if (!early_suspend_flag) {
		pr_info(KERN_INFO "%s\n", __func__);
		if (pdata->set_exgpio_early_suspend)
			pdata->set_exgpio_early_suspend(OFF);
		/* early_clk_switch(OFF); */
		/* early_power_gate_switch(OFF); */
		early_suspend_flag = 1;
	}
}

static void meson_system_late_resume(struct early_suspend *h)
{
	if (early_suspend_flag) {
		/* early_power_gate_switch(ON); */
		/* early_clk_switch(ON); */
		early_suspend_flag = 0;
		pr_info(KERN_INFO "%s\n", __func__);
	}
}
#endif
#if 0
void __iomem *pl_310_base;
void __iomem *suspend_entry;
int meson_power_suspend(void)
{
	static int test_flag;

	void __iomem *p_addr;
	void (*pwrtest_entry)(unsigned, unsigned, unsigned, unsigned);

	flush_cache_all();

	p_addr = suspend_entry + 0x4400;
	pwrtest_entry =
	    (void (*)(unsigned, unsigned, unsigned, unsigned))p_addr;

	if (test_flag != 1234) {
		test_flag = 1234;
		pr_info("initial appf\n");
		pwrtest_entry(APPF_INITIALIZE, 0, 0, (u32) pl_310_base);
	}

	pr_info("power down cpu --\n");
	pwrtest_entry(APPF_POWER_DOWN_CPU, 0, 0,
		      APPF_SAVE_PMU | APPF_SAVE_VFP | APPF_SAVE_L2 | (u32)
		      pl_310_base);
	return 0;
}

unsigned int kk[] = {
	0xe51afb04,
};

#define AO_MF_IR_DEC_STATUS ((0x01 << 10) | (0x66 << 2))
#define AO_MF_IR_DEC_FRAME ((0x01 << 10) | (0x65 << 2))
int remote_detect_key(void)
{
	unsigned power_key;
	if (((aml_read_aobus(AO_MF_IR_DEC_STATUS)) >> 3) & 0x1) {
		power_key = aml_read_aobus(AO_MF_IR_DEC_FRAME);
		if ((power_key) == kk[0])
			return 1;
	}
	return 0;
}
#endif

#define AO_RTC_ADDR1 ((0x01 << 10) | (0xd1 << 2))
#define AO_RTI_STATUS_REG2 ((0x00 << 10) | (0x02 << 2))
#define AO_RTC_ADDR3 ((0x01 << 10) | (0xd3 << 2))

static noinline int __invoke_pm_fn_smc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2)
{

	register long r0 asm("r0") = function_id;
	register long r1 asm("r1") = arg0;
	register long r2 asm("r2") = arg1;
	register long r3 asm("r3") = arg2;
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			__SMC(0)
		: "+r" (r0)
		: "r" (r1), "r" (r2), "r" (r3));

	return function_id;
}


static void meson_pm_suspend(void)
{

	pr_info(KERN_INFO "enter meson_pm_suspend!\n");

	if (pdata->set_vccx2)
		pdata->set_vccx2(OFF);
	late_suspend();
	clk_switch(0);

	aml_cbus_update_bits(0x1067, 1 << 7, 0);
	aml_cbus_update_bits(0x10c0, 1 << 30, 0);
	pr_info(KERN_INFO "sleep ...\n");

	__invoke_pm_fn_smc((u32)0x82000041, (u32)0x300, (u32)0x4, (u32)0);

	aml_cbus_update_bits(0x10c0, (1 << 30), 1<<30);
	pr_info(KERN_INFO "... wake up\n");

	if (aml_read_aobus(AO_RTC_ADDR1) & (1<<12)) {
		/* Woke from alarm, not power button.
		* Set flag to inform key_input driver. */
		aml_write_aobus(AO_RTI_STATUS_REG2, 0x12345678);
	}
	/* clear RTC interrupt */
	aml_write_aobus((AO_RTC_ADDR1), aml_read_aobus(AO_RTC_ADDR1)|(0xf000));
	if (aml_read_aobus(AO_RTC_ADDR3)|(1<<29)) {
		aml_write_aobus((AO_RTC_ADDR3),
				aml_read_aobus(AO_RTC_ADDR3)&(~(1<<29)));
		udelay(1000);
	}

	early_resume();
	if (pdata->set_vccx2)
		pdata->set_vccx2(ON);
	aml_cbus_update_bits(0x1067 , (1 << 7), 1<<7);
	clk_switch(1);
}

static int meson_pm_prepare(void)
{
	pr_info(KERN_INFO "enter meson_pm_prepare!\n");
	return 0;
}

static int meson_pm_enter(suspend_state_t state)
{
	int ret = 0;
	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		meson_pm_suspend();
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static void meson_pm_finish(void)
{
	pr_info(KERN_INFO "enter meson_pm_finish!\n");
}

static const struct platform_suspend_ops meson_pm_ops = {
	.enter = meson_pm_enter,
	.prepare = meson_pm_prepare,
	.finish = meson_pm_finish,
	.valid = suspend_valid_only_mem,
};

static void m6ref_set_vccx2(int power_on)
{
	return;
}

static struct meson_pm_config aml_pm_pdata = {
	.set_vccx2 = m6ref_set_vccx2,
};

static int __init meson_pm_probe(struct platform_device *pdev)
{
	pr_info(KERN_INFO "enter meson_pm_probe!\n");
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	early_suspend.suspend = meson_system_early_suspend;
	early_suspend.resume = meson_system_late_resume;
	register_early_suspend(&early_suspend);
#endif
	pdev->dev.platform_data = &aml_pm_pdata;
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "cannot get platform data\n");
		return -ENOENT;
	}
	suspend_set_ops(&meson_pm_ops);
#if 0
	pl_310_base = ioremap(0xc4200000, 0x1000);
	suspend_entry =
	    __arm_ioremap_exec(0x04f00000, 0x100000, MT_MEMORY_RWX_NONCACHED);
#endif
	pr_info(KERN_INFO "meson_pm_probe done\n");
	return 0;
}

static int __exit meson_pm_remove(struct platform_device *pdev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_pm_dt_match[] = {
	{.compatible = "amlogic, pm",
	 },
};
#else
#define amlogic_nand_dt_match NULL
#endif

static struct platform_driver meson_pm_driver = {
	.driver = {
		   .name = "pm-meson",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_pm_dt_match,
		   },
	.remove = __exit_p(meson_pm_remove),
};

static int __init meson_pm_init(void)
{
	return platform_driver_probe(&meson_pm_driver, meson_pm_probe);
}

late_initcall(meson_pm_init);
