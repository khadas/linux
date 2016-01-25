/*
 * drivers/amlogic/iomap/iomap.c
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
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include <asm/compiler.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#else
#include <asm/psci.h>
#endif
#undef pr_fmt
#define pr_fmt(fmt) "aml_iomap: " fmt

#if 0
void meson_regmap_lock(void *p)
{
	return;
}

void meson_regmap_unlock(void *p)
{
	return;
}

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.lock = meson_regmap_lock,
	.unlock = meson_regmap_unlock,
};
#endif

static const struct of_device_id iomap_dt_match[] = {
	{ .compatible = "amlogic, iomap" },
	{ /* sentinel */ },
};
void __iomem *meson_reg_map[IO_BUS_MAX];

int aml_reg_read(u32 bus_type, unsigned int reg, unsigned int *val)
{
#if 0
	if ((bus_type >= IO_CBUS_BASE) && (bus_type < IO_BUS_MAX))
		return regmap_read(meson_reg_map[bus_type], reg, val);
	else
		return -1;
#endif
	if ((bus_type >= IO_CBUS_BASE) && (bus_type < IO_BUS_MAX)) {
		*val = readl((meson_reg_map[bus_type]+reg));
		return 0;
	} else
		return -1;
}
EXPORT_SYMBOL(aml_reg_read);

int aml_reg_write(u32 bus_type, unsigned int reg, unsigned int val)
{
#if 0
	if ((bus_type >= IO_CBUS_BASE) && (bus_type < IO_BUS_MAX))
		return regmap_write(meson_reg_map[bus_type], reg, val);
	else
		return -1;
#endif
	if ((bus_type >= IO_CBUS_BASE) && (bus_type < IO_BUS_MAX)) {
		writel(val, (meson_reg_map[bus_type]+reg));
		return 0;
	} else
		return -1;
}
EXPORT_SYMBOL(aml_reg_write);

int aml_regmap_update_bits(u32 bus_type,
					unsigned int reg, unsigned int mask,
					unsigned int val)
{
#if 0
	if ((bus_type >= IO_CBUS_BASE) && (bus_type < IO_BUS_MAX))
		return regmap_update_bits(meson_reg_map[bus_type],
				reg, mask, val);
	else
		return -1;
#endif
	if ((bus_type >= IO_CBUS_BASE) && (bus_type < IO_BUS_MAX)) {
		unsigned int tmp, orig;
		aml_reg_read(bus_type, reg, &orig);
		tmp = orig & ~mask;
		tmp |= val & mask;
		aml_reg_write(bus_type, reg, tmp);
		return 0;
	} else
		return -1;
}
EXPORT_SYMBOL(aml_regmap_update_bits);

/*
** CBUS REG Read Write and Update some bits
*/
int aml_read_cbus(unsigned int reg)
{
	int ret, val;
	ret = aml_reg_read(IO_CBUS_BASE, reg<<2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else
		return val;
}
EXPORT_SYMBOL(aml_read_cbus);

void aml_write_cbus(unsigned int reg, unsigned int val)
{
	int ret;
	ret = aml_reg_write(IO_CBUS_BASE, reg<<2, val);
	if (ret) {
		pr_err("write cbus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_write_cbus);

void aml_cbus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	int ret;
	ret = aml_regmap_update_bits(IO_CBUS_BASE, reg<<2, mask, val);
	if (ret) {
		pr_err("write cbus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_cbus_update_bits);

/*
** AO REG Read Write and Update some bits
*/
int aml_read_aobus(unsigned int reg)
{
	int ret, val;
	ret = aml_reg_read(IO_AOBUS_BASE, reg, &val);
	if (ret) {
		pr_err("read ao bus reg %x error %d\n", reg, ret);
		return -1;
	} else
		return val;
}
EXPORT_SYMBOL(aml_read_aobus);

void aml_write_aobus(unsigned int reg, unsigned int val)
{
	int ret;
	ret = aml_reg_write(IO_AOBUS_BASE, reg, val);
	if (ret) {
		pr_err("write ao bus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_write_aobus);

void aml_aobus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	int ret;
	ret = aml_regmap_update_bits(IO_AOBUS_BASE, reg, mask, val);
	if (ret) {
		pr_err("write aobus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_aobus_update_bits);


/*
** VCBUS Bus REG Read Write and Update some bits
*/
int aml_read_vcbus(unsigned int reg)
{
	int ret, val;
	ret = aml_reg_read(IO_APB_BUS_BASE, (0x100000+(reg<<2)), &val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", reg, ret);
		return -1;
	} else
		return val;
}
EXPORT_SYMBOL(aml_read_vcbus);


void aml_write_vcbus(unsigned int reg, unsigned int val)
{
	int ret;
	ret = aml_reg_write(IO_APB_BUS_BASE, (0x100000+(reg<<2)), val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_write_vcbus);

void aml_vcbus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	int ret;
	ret = aml_regmap_update_bits(IO_APB_BUS_BASE,
						(0x100000+(reg<<2)), mask, val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_vcbus_update_bits);


/*
** DOS BUS Bus REG Read Write and Update some bits
*/
int aml_read_dosbus(unsigned int reg)
{
	int ret, val;
	ret = aml_reg_read(IO_APB_BUS_BASE, (0x50000+(reg<<2)), &val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", reg, ret);
		return -1;
	} else
		return val;
}
EXPORT_SYMBOL(aml_read_dosbus);

void aml_write_dosbus(unsigned int reg, unsigned int val)
{
	int ret;
	ret = aml_reg_write(IO_APB_BUS_BASE, (0x50000+(reg<<2)), val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_write_dosbus);

void aml_dosbus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	int ret;
	ret = aml_regmap_update_bits(IO_APB_BUS_BASE,
						(0x50000+(reg<<2)), mask, val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", reg, ret);
		return;
	} else
		return;
}
EXPORT_SYMBOL(aml_dosbus_update_bits);

#ifndef CONFIG_ARM64
static noinline int __invoke_sec_fn_smc(u32 function_id, u32 arg0, u32 arg1,
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
#else
static noinline int __invoke_sec_fn_smc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2)
{
	return __invoke_psci_fn_smc(function_id, arg0, arg1, arg2);
}
#endif

int  aml_read_sec_reg(unsigned int reg)
{
	return __invoke_sec_fn_smc(0x82000035, reg, 0, 0);
}

void  aml_write_sec_reg(unsigned int reg, unsigned int val)
{
	 __invoke_sec_fn_smc(0x82000036, reg, val, 0);
}



static int iomap_probe(struct platform_device *pdev)
{
	int i = 0;
/* void __iomem *base; */
	struct resource res;
	struct device_node *np, *child;
	np = pdev->dev.of_node;

	for_each_child_of_node(np, child) {
			if (of_address_to_resource(child, 0, &res))
				return -1;
#if 0
			base = ioremap(res.start, resource_size(&res));
			meson_regmap_config.max_register =
				resource_size(&res) - 4;
			meson_regmap_config.name = child->name;
			meson_reg_map[i] =
				devm_regmap_init_mmio(&pdev->dev,
					base, &meson_regmap_config);

			if (IS_ERR(meson_reg_map[i])) {
				pr_err("iomap index %d registers not found\n",
					   i);
				return PTR_ERR(meson_reg_map[i]);
			}
#endif
			meson_reg_map[i] =
				ioremap(res.start, resource_size(&res));
			i++;
	}
	pr_info("amlogic iomap probe done\n");
	return 0;
}



static  struct platform_driver iomap_platform_driver = {
	.probe		= iomap_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "iomap_version",
		.of_match_table	= iomap_dt_match,
	},
};

int __init meson_iomap_version_init(void)
{

	int ret;

	ret = platform_driver_register(&iomap_platform_driver);

	return ret;
}
core_initcall(meson_iomap_version_init);
