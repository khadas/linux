/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __PCPD_MONITOR_TABLE_H__
#define PCPD_MONITOR_A 0
#define PCPD_MONITOR_B 1

struct pcpd_monitor_chipinfo {
	unsigned int id;
};

static struct pcpd_monitor_chipinfo t5w_pcpda_monitor_chipinfo = {
	.id = PCPD_MONITOR_A,
};

static struct pcpd_monitor_chipinfo t5w_pcpdb_monitor_chipinfo = {
	.id = PCPD_MONITOR_B,
};

static const struct of_device_id aml_pcpd_device_id[] = {
	{
		.compatible = "amlogic, pcpda_monitor",
		.data       = &t5w_pcpda_monitor_chipinfo,
	},
	{
		.compatible = "amlogic, pcpdb_monitor",
		.data       = &t5w_pcpdb_monitor_chipinfo,
	},
	{ /*sentinel*/ }
};

#endif
