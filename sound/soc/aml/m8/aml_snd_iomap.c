/*
 * sound/soc/aml/m8/aml_snd_iomap.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/sound/aml_snd_iomap.h>

/*
 * compatible for chipset except txlx
 * 'cause of sound reg is managed by cbus operation befor, so reg is fixed
 * in audin_reg.h and aiu_reg.h, for compability, adjust the base in dts
 */
static bool aml_snd_iomap_is_default = true;

static void __iomem *aml_snd_reg_map[IO_BASE_MAX];


static int aml_snd_read(u32 base_type, unsigned int reg, unsigned int *val)
{
	if (aml_snd_iomap_is_default) {

		*val = aml_read_cbus(reg);
		if (*val < 0) {
			pr_err("read cbus reg %x error\n", reg);
			return -1;
		}
		return 0;
	}

	if ((base_type >= IO_AUDIN_BASE) && (base_type < IO_BASE_MAX)) {
		*val = readl((aml_snd_reg_map[base_type] + (reg << 2)));

		return 0;
	}

	return -1;
}

static void aml_snd_write(u32 base_type, unsigned int reg, unsigned int val)
{
	if (aml_snd_iomap_is_default) {
		aml_write_cbus(reg, val);

		return;
	}

	if ((base_type >= IO_AUDIN_BASE) && (base_type < IO_BASE_MAX)) {
		writel(val, (aml_snd_reg_map[base_type] + (reg << 2)));

		return;
	}

	pr_err("write snd reg %x error\n", reg);
}

static void aml_snd_update_bits(u32 base_type,
			unsigned int reg, unsigned int mask,
			unsigned int val)
{
	if (aml_snd_iomap_is_default) {
		aml_cbus_update_bits(reg, mask, val);
		return;
	}

	if ((base_type >= IO_AUDIN_BASE) && (base_type < IO_BASE_MAX)) {
		unsigned int tmp, orig;

		aml_snd_read(base_type, reg, &orig);
		tmp = orig & ~mask;
		tmp |= val & mask;
		aml_snd_write(base_type, reg, tmp);

		return;
	}
	pr_err("write snd reg %x error\n", reg);

}

int aml_audin_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_AUDIN_BASE, reg, &val);

	if (ret) {
		pr_err("read audin reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_audin_read);

void aml_audin_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_AUDIN_BASE, reg, val);
}
EXPORT_SYMBOL(aml_audin_write);

void aml_audin_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_AUDIN_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_audin_update_bits);

int aml_aiu_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_AIU_BASE, reg, &val);

	if (ret) {
		pr_err("read aiu reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_aiu_read);

void aml_aiu_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_AIU_BASE, reg, val);
}
EXPORT_SYMBOL(aml_aiu_write);

void aml_aiu_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_AIU_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_aiu_update_bits);

int aml_eqdrc_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_EQDRC_BASE, reg, &val);

	if (ret) {
		pr_err("read eqdrc reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_eqdrc_read);

void aml_eqdrc_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_EQDRC_BASE, reg, val);
}
EXPORT_SYMBOL(aml_eqdrc_write);

void aml_eqdrc_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_EQDRC_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_eqdrc_update_bits);

int aml_hiu_reset_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_HIU_RESET_BASE, reg, &val);

	if (ret) {
		pr_err("read hiu reset reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_hiu_reset_read);

void aml_hiu_reset_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_HIU_RESET_BASE, reg, val);
}
EXPORT_SYMBOL(aml_hiu_reset_write);

void aml_hiu_reset_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_HIU_RESET_BASE, reg, mask, val);

	return;
}
EXPORT_SYMBOL(aml_hiu_reset_update_bits);

int aml_isa_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_ISA_BASE, reg, &val);

	if (ret) {
		pr_err("read ISA reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_isa_read);

void aml_isa_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_ISA_BASE, reg, val);
}
EXPORT_SYMBOL(aml_isa_write);

void aml_isa_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_ISA_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_isa_update_bits);

static int snd_iomap_probe(struct platform_device *pdev)
{
	struct resource res;
	struct device_node *np, *child;
	int i = 0;
	int ret = 0;

	np = pdev->dev.of_node;
	for_each_child_of_node(np, child) {
			if (of_address_to_resource(child, 0, &res)) {
				ret = -1;
				pr_err("%s could not get resource, use in default",
					__func__);
				break;
			}
			aml_snd_reg_map[i] =
				ioremap(res.start, resource_size(&res));
			pr_info("aml_snd_reg_map[%d], reg:%x, size:%x\n",
				i, (u32)res.start, (u32)resource_size(&res));

			if (aml_snd_iomap_is_default)
				aml_snd_iomap_is_default = false;

			i++;
	}
	pr_info("aml snd iomap probe done, aml_snd_iomap_is_default:%d\n",
		aml_snd_iomap_is_default);
	return ret;
}

static const struct of_device_id snd_iomap_dt_match[] = {
	{ .compatible = "amlogic, snd_iomap" },
	{},
};

static  struct platform_driver snd_iomap_platform_driver = {
	.probe		= snd_iomap_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "snd_iomap",
		.of_match_table	= snd_iomap_dt_match,
	},
};

int __init meson_snd_iomap_init(void)
{
	int ret;

	ret = platform_driver_register(&snd_iomap_platform_driver);

	return ret;
}
core_initcall(meson_snd_iomap_init);
