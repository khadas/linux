/*
 * drivers/amlogic/cpu_version/cpu.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <asm/compiler.h>

#undef pr_fmt
#define pr_fmt(fmt) "cpu_version: " fmt

static int meson_cpu_version[MESON_CPU_VERSION_LVL_MAX+1];
void __iomem  *assist_hw_rev;
void __iomem  *bootrom_base;
void __iomem  *metal_version;
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
static noinline int __invoke_meson_smc(u32 id)
{
		register unsigned r0 asm("r0") = id;
		asm volatile(
		__asmeq("%0", "r0")
		__SMC(0)
		: : "r" (r0));
		return r0;
}
#else
static noinline int __invoke_meson_smc(u32 id)
{
		register unsigned r0 asm("x0") = id;
		asm volatile(
		__asmeq("%0", "x0")
		"smc	#0\n"
		: : "r" (r0));
		return r0;
}
#endif
int get_meson_cpu_version(int level)
{
	if (level >= 0 && level <= MESON_CPU_VERSION_LVL_MAX)
		return meson_cpu_version[level];
	return 0;
}
EXPORT_SYMBOL(get_meson_cpu_version);

int __init meson_cpu_version_init(void)
{
	unsigned int version, ver;
	u32 cmd;
	struct device_node *cpu_version;
	cpu_version = of_find_node_by_path("/cpu_version");
	if (cpu_version) {
		assist_hw_rev = of_iomap(cpu_version, 0);
		metal_version = of_iomap(cpu_version, 1);
	} else{
		return 0;
	}
	meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR] =
		readl_relaxed(assist_hw_rev);

	if (!of_property_read_u32(cpu_version, "smc_cmd", &cmd)) {
		meson_cpu_version[MESON_CPU_VERSION_LVL_MISC] =
			__invoke_meson_smc(cmd);
	}

	version = readl_relaxed(metal_version);
	switch (version) {
	case 0x11111112:
		/*M8 MarkII RevA*/
		meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR] = 0x1D;
	case 0x11111111:
		ver = 0xA;
		break;
	case 0x11111113:
		ver = 0xB;
		break;
	case 0x11111133:
		ver = 0xC;
		break;
	default:/*changed?*/
		ver = 0xD;
		break;
	}
	meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR] = ver;
	pr_info("Meson chip version = Rev%X (%X:%X - %X:%X)\n", ver,
		meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_PACK],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MISC]
		);
	return 0;
}
early_initcall(meson_cpu_version_init);
