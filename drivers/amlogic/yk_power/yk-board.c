/*
 * Base driver for YEKER
 *
 * Copyright (C) 2013 yeker, Ltd.
 *  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
//#include <mach/irqs.h>
#include <linux/power_supply.h>
#include <linux/apm_bios.h>
#include <linux/apm-emulation.h>
#include <linux/module.h>

#include "yk-cfg.h"
#include "yk-mfd.h"


static struct power_supply_info battery_data ={
		.name ="PTI PL336078",
		.technology = POWER_SUPPLY_TECHNOLOGY_LION,
		.voltage_max_design = CHGVOL,
		.voltage_min_design = SHUTDOWNVOL,
		.energy_full_design = BATCAP,
		.use_for_apm = 1,
};


static struct yk_supply_init_data yk_sply_init_data = {
	.battery_info = &battery_data,
	.chgcur = STACHGCUR,
	.chgvol = CHGVOL,
	.chgend = ENDCHGRATE,
	.chgen = CHGEN,
	.sample_time = ADCFREQ,
	.chgpretime = CHGPRETIME,
	.chgcsttime = CHGCSTTIME,
};

static struct yk_funcdev_info yk_splydev[]={
   	{
   		.name = "yk618-supplyer",
		.id = 0,
		.platform_data = &yk_sply_init_data,
    },
};


struct yk_platform_data yk_pdata = {
	.num_sply_devs = ARRAY_SIZE(yk_splydev),
	.sply_devs = yk_splydev,
};


