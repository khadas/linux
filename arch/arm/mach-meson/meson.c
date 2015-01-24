/*
 * arch/arm/mach-meson/meson.c
 *
 * Copyright (C) 2011-2013 Amlogic, Inc.
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

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>

static __init void meson_init_machine_devicetree(void)
{
	struct device *parent;
	parent = get_device(&platform_bus);

	of_platform_populate(NULL, of_default_bus_match_table, NULL, parent);

}

static const char * const m8_common_board_compat[] __initconst = {
	"AMLOGIC,mesonX",
	NULL,
};

DT_MACHINE_START(AMLOGIC, "Amlogic MesonX")
	.init_machine	= meson_init_machine_devicetree,
	.dt_compat	= m8_common_board_compat,
MACHINE_END
