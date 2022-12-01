/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DMC_DEV_ACCESS_H__
#define __DMC_DEV_ACCESS_H__

#define MAX_NAME	15

struct dmc_dev_access_data {
	unsigned long addr;
	unsigned long size;
};

struct dmc_notifier_list {
	char port_name[MAX_NAME];
	char port_id;
	struct list_head list;
};

void dmc_dev_access(unsigned char id, unsigned long addr, unsigned long size);
int show_dmc_notifier_list(char *buf);
int register_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb);
int unregister_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb);

#endif
