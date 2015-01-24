/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt) "psci: " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <asm/compiler.h>
#include <asm/errno.h>
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#include <asm/psci.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <asm/smp_scu.h>
#include <linux/io.h>
struct psci_operations psci_ops;
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
aml_set_reg32_bits(void __iomem *_reg, uint32_t _value,
uint32_t _start, uint32_t _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
					 ~(((1L << (_len))-1) << (_start)))
					| ((unsigned)((_value)&((1L<<(_len))-1))
					   << (_start))), _reg);
}

static int (*invoke_psci_fn)(u32, u32, u32, u32);
enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_MAX,
};

static u32 psci_function_id[PSCI_FN_MAX];

#define PSCI_RET_SUCCESS		0
#define PSCI_RET_EOPNOTSUPP		-1
#define PSCI_RET_EINVAL			-2
#define PSCI_RET_EPERM			-3
#define PSCI_RET_EALREADYON		-4
static DEFINE_SPINLOCK(clockfw_lock);

static int psci_to_linux_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case PSCI_RET_EINVAL:
		return -EINVAL;
	case PSCI_RET_EPERM:
		return -EPERM;
	case PSCI_RET_EALREADYON:
		return -EAGAIN;
	};

	return -EINVAL;
}

#define PSCI_POWER_STATE_ID_MASK	0xffff
#define PSCI_POWER_STATE_ID_SHIFT	0
#define PSCI_POWER_STATE_TYPE_MASK	0x1
#define PSCI_POWER_STATE_TYPE_SHIFT	16
#define PSCI_POWER_STATE_AFFL_MASK	0x3
#define PSCI_POWER_STATE_AFFL_SHIFT	24

static u32 psci_power_state_pack(struct psci_power_state state)
{
	return	((state.id & PSCI_POWER_STATE_ID_MASK)
			<< PSCI_POWER_STATE_ID_SHIFT)	|
		((state.type & PSCI_POWER_STATE_TYPE_MASK)
			<< PSCI_POWER_STATE_TYPE_SHIFT)	|
		((state.affinity_level & PSCI_POWER_STATE_AFFL_MASK)
			<< PSCI_POWER_STATE_AFFL_SHIFT);
}

/*
 * The following two functions are invoked via the invoke_psci_fn pointer
 * and will not be inlined, allowing us to piggyback on the AAPCS.
 */
static noinline int __invoke_psci_fn_hvc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2)
{
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			__HVC(0)
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

static noinline int __invoke_psci_fn_smc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2)
{
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			__SMC(0)
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

#define PSCI_VERSION			0x84000000
#define PSCI_CPU_SUSPEND_AARCH32	0x84000001
#define PSCI_CPU_OFF			0x84000002
#define PSCI_CPU_ON_AARCH32		0x84000003
#define PSCI_AFFINITY_INFO_AARCH32	0x84000004
#define PSCI_MIG_AARCH32		0x84000005
#define PSCI_MIG_INFO_TYPE		0x84000006
#define PSCI_MIG_INFO_UP_CPU_AARCH32	0x84000007
#define PSCI_SYSTEM_OFF			0x84000008
#define PSCI_SYSTEM_RESET		0x84000009
void __iomem *meson_cpu_power_ctrl;
void __iomem *sys_cpu_clk_ctrl;
void __iomem *rti_pwr_a9_ctrl1;
void __iomem *rti_pwr_a9_ctrl0;
void __iomem *meson_cpu_ctrl_reg;
void __iomem *meson_cpu1_ctrl_addr_reg;
void __iomem *io_peri_base;
static void meson_set_cpu_power_ctrl(uint32_t cpu, int is_power_on)
{
	BUG_ON(cpu == 0);

	if (is_power_on) {
		/* SCU Power on CPU */
		aml_set_reg32_bits(meson_cpu_power_ctrl + 0x8,
						   0x0 , (cpu << 3), 2);

#ifndef CONFIG_MESON_CPU_EMULATOR
		/* Reset enable*/
		aml_set_reg32_bits(sys_cpu_clk_ctrl, 1 , (cpu + 24), 1);
		/* Power on*/
		aml_set_reg32_bits(rti_pwr_a9_ctrl1, 0x0, ((cpu + 1) << 1), 2);
		udelay(10);
		/* Isolation disable */
		aml_set_reg32_bits(rti_pwr_a9_ctrl0, 0x0, cpu, 1);
		/* Reset disable */
		aml_set_reg32_bits(sys_cpu_clk_ctrl, 0 , (cpu + 24), 1);
#endif
	} else{
		aml_set_reg32_bits(meson_cpu_power_ctrl + 0x8,
						   0x3, (cpu << 3), 2);

#ifndef CONFIG_MESON_CPU_EMULATOR
		/* Isolation enable */
		aml_set_reg32_bits(rti_pwr_a9_ctrl0, 0x1, cpu, 1);
		udelay(10);
		/* Power off */
		aml_set_reg32_bits(rti_pwr_a9_ctrl1, 0x3, ((cpu + 1) << 1), 2);
#endif
	}
	dsb();
	/* add comment*/
	dmb();

	/* pr_debug("----CPU %d\n",cpu); */
	/* pr_debug("----MESON_CPU_POWER_CTRL_REG(%08x) = %08x\n", \
	 MESON_CPU_POWER_CTRL_REG,aml_read_reg32(MESON_CPU_POWER_CTRL_REG)); */
	/* pr_debug("----P_AO_RTI_PWR_A9_CNTL0(%08x)    = %08x\n", \
	 P_AO_RTI_PWR_A9_CNTL0,aml_read_reg32(P_AO_RTI_PWR_A9_CNTL0)); */
	/* pr_debug("----P_AO_RTI_PWR_A9_CNTL1(%08x)    = %08x\n", \
	 P_AO_RTI_PWR_A9_CNTL1,aml_read_reg32(P_AO_RTI_PWR_A9_CNTL1)); */

}

static void meson_set_cpu_ctrl_reg(int cpu, int is_on)
{
	spin_lock(&clockfw_lock);
	aml_set_reg32_bits(meson_cpu_ctrl_reg + 0x1ff80,
					   is_on, cpu, 1);
	aml_set_reg32_bits(meson_cpu_ctrl_reg + 0x1ff80, 1, 0, 1);

	spin_unlock(&clockfw_lock);
}

static void meson_set_cpu_ctrl_addr(uint32_t cpu, const uint32_t addr)
{
	spin_lock(&clockfw_lock);

	aml_write_reg32(((meson_cpu_ctrl_reg + 0x1ff84) + ((cpu-1) << 2)),
					addr);

	spin_unlock(&clockfw_lock);
}
static int meson_get_cpu_ctrl_addr(int cpu)
{
	return readl_relaxed((meson_cpu_ctrl_reg + 0x1ff84) + ((cpu-1) << 2));

}

static DEFINE_SPINLOCK(boot_lock);
static int aml_psci_cpu_on(u32 cpu, unsigned long entry_point, u32 arg2)
{
		unsigned long timeout;
		/*
		* Set synchronisation state between this boot processor
		* and the secondary one
		*/
		scu_enable((void __iomem *) io_peri_base);
		spin_lock(&boot_lock);

	/* check_and_rewrite_cpu_entry(); */
		meson_set_cpu_ctrl_addr(cpu, (const uint32_t)entry_point);
		meson_set_cpu_power_ctrl(cpu, 1);
		timeout = jiffies + (10 * HZ);
		while (meson_get_cpu_ctrl_addr(cpu)) {
			if (!time_before(jiffies, timeout))
				return -EPERM;
		}

		meson_set_cpu_ctrl_addr(cpu, (const uint32_t)entry_point);
		meson_set_cpu_ctrl_reg(cpu, 1);
		smp_wmb();
		mb();

		dsb_sev();

		/*
		 * now the secondary core is starting up let it run its
		 * calibrations, then wait for it to finish
		 */
		spin_unlock(&boot_lock);
		return 0;

}

static noinline int
__invoke_psci_fn_firmware(u32 function_id, u32 arg0, u32 arg1, u32 arg2)
{
#if 0
	static int test_flag;
	unsigned p_addr;
	unsigned addr;
	unsigned long flag;
	void (*pwrtest_entry)(unsigned, unsigned, unsigned, unsigned);
	 addr = 0x04F04400;
	spin_lock_irqsave(&boot_lock, flag);
	p_addr = (unsigned)__phys_to_virt(addr);
	pwrtest_entry =
		(void (*)(unsigned, unsigned, unsigned, unsigned))p_addr;
	if (test_flag != 1234) {
		test_flag = 1234;
		pwrtest_entry(0, 0, 0, IO_PL310_BASE & 0xffff0000);
	}
	pwrtest_entry(function_id, arg0, arg1, IO_PL310_BASE & 0xffff0000);
	spin_unlock_irqrestore(&boot_lock, flag);
#else
	int ret =  -1;
	pr_info("using fake psci\n");
	switch (function_id) {
	case PSCI_CPU_ON_AARCH32:
		ret = aml_psci_cpu_on(arg0,  arg1, arg2);
		break;
	case PSCI_CPU_OFF:
		break;
	}
	 return ret;
#endif
	return 0;

}

static int psci_cpu_suspend(struct psci_power_state state,
			    unsigned long entry_point)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_SUSPEND];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_off(struct psci_power_state state)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_OFF];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, 0, 0);
	return psci_to_linux_errno(err);
}
static struct device_node *NP;
static int flag;
static int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int err;
	u32 fn;
	if (!flag) {
		meson_cpu_power_ctrl = of_iomap(NP, 0);
		sys_cpu_clk_ctrl = of_iomap(NP, 1);
		rti_pwr_a9_ctrl1 = of_iomap(NP, 2);
		rti_pwr_a9_ctrl0 = of_iomap(NP, 3);
		meson_cpu_ctrl_reg = of_iomap(NP, 4);
		io_peri_base = of_iomap(NP, 5);
		flag = 1;
	}
	fn = psci_function_id[PSCI_FN_CPU_ON];
	err = invoke_psci_fn(fn, cpuid, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_migrate(unsigned long cpuid)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_MIGRATE];
	err = invoke_psci_fn(fn, cpuid, 0, 0);
	return psci_to_linux_errno(err);
}

static const struct of_device_id psci_of_match[] = {
	{ .compatible = "arm,psci",	},
	{},
};

void __init psci_init(void)
{
	struct device_node *np;
	const char *method;
	u32 id;

	np = of_find_matching_node(NULL, psci_of_match);
	if (!np)
		return;
	NP = np;
	pr_info("probing function IDs from device-tree\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		goto out_put_node;
	}

	if (!strcmp("hvc", method)) {
		invoke_psci_fn = __invoke_psci_fn_hvc;
	} else if (!strcmp("smc", method)) {
		invoke_psci_fn = __invoke_psci_fn_smc;
	} else if (!strcmp("firmware", method)) {
		invoke_psci_fn = __invoke_psci_fn_firmware;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		goto out_put_node;
	}

	if (!of_property_read_u32(np, "cpu_suspend", &id)) {
		psci_function_id[PSCI_FN_CPU_SUSPEND] = id;
		psci_ops.cpu_suspend = psci_cpu_suspend;
	}

	if (!of_property_read_u32(np, "cpu_off", &id)) {
		psci_function_id[PSCI_FN_CPU_OFF] = id;
		psci_ops.cpu_off = psci_cpu_off;
	}

	if (!of_property_read_u32(np, "cpu_on", &id)) {
		psci_function_id[PSCI_FN_CPU_ON] = id;
		psci_ops.cpu_on = psci_cpu_on;
	}

	if (!of_property_read_u32(np, "migrate", &id)) {
		psci_function_id[PSCI_FN_MIGRATE] = id;
		psci_ops.migrate = psci_migrate;
	}

out_put_node:
	of_node_put(np);
	return;
}

int psci_probe(void)
{
	struct device_node *np;
	int ret = -ENODEV;

	np = of_find_matching_node(NULL, psci_of_match);
	if (np)
		ret = 0;

	of_node_put(np);
	return ret;
}
