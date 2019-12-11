// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 #define pr_fmt(fmt) "Codec io: " fmt

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
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/utils/log.h>
#include <linux/amlogic/cpu_version.h>

static u32 regs_cmd_debug;

static const struct of_device_id codec_io_dt_match[] = {
	{
		.compatible = "amlogic, meson-g12a, codec-io",
	},
	{
		.compatible = "amlogic, meson-g12b, codec-io",
	},
	{
		.compatible = "amlogic, meson-tl1, codec-io",
	},
	{
		.compatible = "amlogic, meson-c1, codec-io",
	},
	{},
};

enum {
	CODECIO_CBUS_BASE = 0,
	CODECIO_DOSBUS_BASE,
	CODECIO_HIUBUS_BASE,
	CODECIO_AOBUS_BASE,
	CODECIO_VCBUS_BASE,
	CODECIO_DMCBUS_BASE,
	CODECIO_EFUSE_BASE,
	CODECIO_BUS_MAX,
};

static void __iomem *codecio_reg_map[CODECIO_BUS_MAX];

static inline void __aml_reg_read(u32 bus, unsigned int reg, unsigned int *val)
{
	WARN_ON((bus >= CODECIO_BUS_MAX) || !codecio_reg_map[bus]);
	*val = readl((codecio_reg_map[bus] + reg));
}

static inline void __aml_reg_write(u32 bus, unsigned int reg, unsigned int val)
{
	WARN_ON((bus >= CODECIO_BUS_MAX) || !codecio_reg_map[bus]);
	writel(val, (codecio_reg_map[bus] + reg));
}

int aml_reg_read(u32 bus, unsigned int reg, unsigned int *val)
{
	__aml_reg_read(bus, reg, val);
	return 0;
}
EXPORT_SYMBOL(aml_reg_read);

int aml_reg_write(u32 bus, unsigned int reg, unsigned int val)
{
	__aml_reg_write(bus, reg, val);
	return 0;
}
EXPORT_SYMBOL(aml_reg_write);

int aml_regmap_update_bits(u32 bus,
			   unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	unsigned int tmp, orig;

	WARN_ON((bus >= CODECIO_BUS_MAX) || !codecio_reg_map[bus]);

	orig = readl((codecio_reg_map[bus] + reg));
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, (codecio_reg_map[bus] + reg));
	return 0;
}
EXPORT_SYMBOL(aml_regmap_update_bits);

int aml_read_dosbus(unsigned int reg)
{
	int val;

	__aml_reg_read(CODECIO_DOSBUS_BASE, reg << 2, &val);
	if (regs_cmd_debug)
		pr_info("DOS READ , reg: 0x%x, val: 0x%x\n", reg, val);
	return val;
}

void aml_write_dosbus(unsigned int reg, unsigned int val)
{
	__aml_reg_write(CODECIO_DOSBUS_BASE, reg << 2, val);
	if (regs_cmd_debug)
		pr_info("DOS WRITE, reg: 0x%x, val: 0x%x\n", reg, val);
}

int aml_read_dmcbus(unsigned int reg)
{
	int val;

	__aml_reg_read(CODECIO_DMCBUS_BASE, reg << 2, &val);
	return val;
}

void aml_write_dmcbus(unsigned int reg, unsigned int val)
{
	__aml_reg_write(CODECIO_DMCBUS_BASE, reg << 2, val);
}

int aml_read_efusebus(unsigned int reg)
{
	int val;

	__aml_reg_read(CODECIO_EFUSE_BASE, reg << 2, &val);
	return val;
}

void aml_write_efusebus(unsigned int reg, unsigned int val)
{
	__aml_reg_write(CODECIO_EFUSE_BASE, reg << 2, val);
}

int aml_read_cbus(unsigned int reg)
{
	int val;

	__aml_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	return val;
}
EXPORT_SYMBOL(aml_read_cbus);

void aml_write_cbus(unsigned int reg, unsigned int val)
{
	__aml_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
}
EXPORT_SYMBOL(aml_write_cbus);

void aml_cbus_update_bits(unsigned int reg,
			  unsigned int mask,
			  unsigned int val)
{
	aml_regmap_update_bits(CODECIO_CBUS_BASE, reg << 2, mask, val);
}
EXPORT_SYMBOL(aml_cbus_update_bits);

int aml_read_aobus(unsigned int reg)
{
	int val;

	__aml_reg_read(CODECIO_AOBUS_BASE, reg, &val);
	return val;
}
EXPORT_SYMBOL(aml_read_aobus);

void aml_write_aobus(unsigned int reg, unsigned int val)
{
	__aml_reg_write(CODECIO_AOBUS_BASE, reg, val);
}
EXPORT_SYMBOL(aml_write_aobus);

void aml_aobus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	aml_regmap_update_bits(CODECIO_AOBUS_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_aobus_update_bits);

int aml_read_vcbus(unsigned int reg)
{
	int val;

	WARN_ON(reg >= 0x1900 && reg < 0x1a00);
	__aml_reg_read(CODECIO_VCBUS_BASE, reg << 2, &val);
	return val;
}
EXPORT_SYMBOL(aml_read_vcbus);

void aml_write_vcbus(unsigned int reg, unsigned int val)
{
	WARN_ON(reg >= 0x1900 && reg < 0x1a00);
	__aml_reg_write(CODECIO_VCBUS_BASE, reg << 2, val);
}

void aml_vcbus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_VCBUS_BASE,
				     reg << 2, mask, val);
	if (ret)
		pr_err("write vcbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_vcbus_update_bits);

int aml_read_hiubus(unsigned int reg)
{
	int val;

	__aml_reg_read(CODECIO_HIUBUS_BASE, reg << 2, &val);
	return val;
}
EXPORT_SYMBOL(aml_read_hiubus);

void aml_write_hiubus(unsigned int reg, unsigned int val)
{
	__aml_reg_write(CODECIO_HIUBUS_BASE, reg << 2, val);
}
EXPORT_SYMBOL(aml_write_hiubus);

void aml_hiubus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val)
{
	aml_regmap_update_bits(CODECIO_HIUBUS_BASE,
			       reg << 2, mask, val);
}
EXPORT_SYMBOL(aml_hiubus_update_bits);

static int __init codec_io_probe(struct platform_device *pdev)
{
	int i = 0;
	struct resource res;
	struct device_node	*of_node = pdev->dev.of_node;
	void __iomem *base;

	for (i = CODECIO_CBUS_BASE; i < CODECIO_BUS_MAX; i++) {
		if (of_address_to_resource(of_node, i, &res)) {
			pr_err("Fail to get regbase index %d\n", i);
			return (-ENOENT);
		}
		base = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
		if (IS_ERR(base))
			return -ENOMEM;

		codecio_reg_map[i] = base;
		pr_debug("codec map io source 0x%lx,size=%d to 0x%lx\n",
			 (unsigned long)res.start, (int)resource_size(&res),
			(unsigned long)codecio_reg_map[i]);
	}
	pr_info("%s success. %d mapped\n", __func__, i);

	return 0;
}

static struct platform_driver codec_io_platform_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "codec_io",
		.of_match_table = codec_io_dt_match,
	},
};

static int __init codec_io_init(void)
{
	return platform_driver_probe(&codec_io_platform_driver,
				    codec_io_probe);
}

module_param(regs_cmd_debug, uint, 0664);
MODULE_PARM_DESC(regs_cmd_debug, "\n register commands sequence debug.\n");

arch_initcall_sync(codec_io_init);
