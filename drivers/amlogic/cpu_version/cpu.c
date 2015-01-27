/*
 * drivers/amlogic/cpu_version/cpu.c
 *
 * Copyright (C) 2013 Amlogic, Inc.
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
#include <asm/system_info.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/amlogic/soc_io.h>

static int meson_cpu_version[MESON_CPU_VERSION_LVL_MAX+1];
void __iomem  *assist_hw_rev;
void __iomem  *bootrom_base;
void __iomem  *metal_version;

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


int get_meson_cpu_version(int level)
{
	if (level >= 0 && level <= MESON_CPU_VERSION_LVL_MAX)
		return meson_cpu_version[level];
	return 0;
}
EXPORT_SYMBOL(get_meson_cpu_version);

static const struct of_device_id cpu_dt_match[] = {
	{ .compatible = "amlogic,cpu_version" },
	{ /* sentinel */ },
};
struct regmap *meson_reg_map[IO_BUS_MAX];

int aml_reg_read(u32 bus_type, unsigned int reg, unsigned int *val)
{
	if ((bus_type >= IO_A9_PERIPH_BASE) && (bus_type < IO_BUS_MAX))
		return regmap_read(meson_reg_map[bus_type], reg, val);
	else
		return -1;
}
EXPORT_SYMBOL(aml_reg_read);

int aml_reg_write(u32 bus_type, unsigned int reg, unsigned int val)
{
	if ((bus_type >= IO_A9_PERIPH_BASE) && (bus_type < IO_BUS_MAX))
		return regmap_write(meson_reg_map[bus_type], reg, val);
	else
		return -1;
}
EXPORT_SYMBOL(aml_reg_write);

int aml_regmap_update_bits(u32 bus_type,
					unsigned int reg, unsigned int mask,
					unsigned int val)
{
	if ((bus_type >= IO_A9_PERIPH_BASE) && (bus_type < IO_BUS_MAX))
		return regmap_update_bits(meson_reg_map[bus_type],
				reg, mask, val);
	else
		return -1;
}
EXPORT_SYMBOL(aml_regmap_update_bits);


static void cpu_version_init(void)
{
		unsigned int version, ver;
		int rev, val;

		aml_reg_read(IO_CBUS_BASE, 0x1f53*4, &val);
		meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR] = val;
		aml_reg_read(IO_CBUS_BASE, 0x206a*4, &version);
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
		rev = get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR);
		rev <<= 24;
		system_serial_high = rev;
		rev = get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR);
		system_rev = rev;
}

static int cpu_probe(struct platform_device *pdev)
{
	int i = 0;
	void __iomem *base;
	struct resource res;
	struct device_node *np, *child;
	np = pdev->dev.of_node;

	for_each_child_of_node(np, child) {
			if (of_address_to_resource(child, 0, &res))
				return -1;
			base = ioremap(res.start, resource_size(&res));
			meson_regmap_config.max_register =
				resource_size(&res) - 4;
			meson_regmap_config.name = child->name;
			meson_reg_map[i] =
				devm_regmap_init_mmio(&pdev->dev,
					base, &meson_regmap_config);

			if (IS_ERR(meson_reg_map)) {
				pr_err("mux registers not found\n");
				return PTR_ERR(meson_reg_map);
			}
			i++;
	}
	cpu_version_init();
	return 0;
}



static  struct platform_driver cpu_platform_driver = {
	.probe		= cpu_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "cpu_version",
		.of_match_table	= cpu_dt_match,
	},
};

int __init meson_cpu_version_init(void)
{

	int ret;

	ret = platform_driver_register(&cpu_platform_driver);

	return ret;
}
pure_initcall(meson_cpu_version_init);
