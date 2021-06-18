// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 #define pr_fmt(fmt) "cpu_version: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define AO_SEC_SD_CFG8		0xe0
#define AO_SEC_SOCINFO_OFFSET	AO_SEC_SD_CFG8
static unsigned char cpu_version[MESON_CPU_VERSION_LVL_MAX + 1];
static int init_done;
static int cpu_id_from_media;

int get_cpu_type_from_media(void)
{
	return cpu_id_from_media;
}
EXPORT_SYMBOL(get_cpu_type_from_media);

void set_cpu_type_from_media(int cpu_id)
{
	if (cpu_id <= 0) {
		pr_err("wrong cpu id from media driver 0x%x.\n", cpu_id);
		return;
	}
	cpu_id_from_media = cpu_id;
}
EXPORT_SYMBOL(set_cpu_type_from_media);

unsigned char get_meson_cpu_version(int level)
{
	if (!init_done) {
		pr_err("too early call get_meson_cpu_version()\n");
		dump_stack();
		return -1;
	}

	if (level >= 0 && level <= MESON_CPU_VERSION_LVL_MAX)
		return cpu_version[level];
	return 0;
}
EXPORT_SYMBOL(get_meson_cpu_version);

int __init meson_cpu_version_init(void)
{
	struct device_node *np;
	unsigned int socinfo;
	struct regmap *regmap;
	int ret;

	/* look up for chipid node */
	np = of_find_compatible_node(NULL, NULL, "amlogic,meson-gx-ao-secure");
	if (!np)
		return -ENODEV;

	/* node should be a syscon */
	regmap = syscon_node_to_regmap(np);
	of_node_put(np);
	if (IS_ERR(regmap)) {
		pr_err("%s: failed to get regmap\n", __func__);
		return -ENODEV;
	}

	ret = regmap_read(regmap, AO_SEC_SOCINFO_OFFSET, &socinfo);
	if (ret < 0)
		return ret;

	if (!socinfo) {
		pr_err("%s: invalid chipid value\n", __func__);
		return -EINVAL;
	}

	cpu_version[MESON_CPU_VERSION_LVL_MAJOR] = (socinfo >> 24) & 0xff;
	cpu_version[MESON_CPU_VERSION_LVL_MINOR] =  (socinfo >> 8) & 0xff;
	cpu_version[MESON_CPU_VERSION_LVL_PACK] =  (socinfo >> 16) & 0xff;
	cpu_version[MESON_CPU_VERSION_LVL_MISC] = socinfo & 0xff;
	pr_info("chip version = %X:%X - %X:%X\n",
		cpu_version[MESON_CPU_VERSION_LVL_MAJOR],
		cpu_version[MESON_CPU_VERSION_LVL_MINOR],
		cpu_version[MESON_CPU_VERSION_LVL_PACK],
		cpu_version[MESON_CPU_VERSION_LVL_MISC]
	);

	init_done = 1;
	return 0;
}
