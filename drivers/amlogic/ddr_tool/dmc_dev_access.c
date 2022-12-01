// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "ddr_port.h"
#include "dmc_monitor.h"
#include <linux/amlogic/dmc_dev_access.h>

static BLOCKING_NOTIFIER_HEAD(dmc_dev_access_nofitier_chain);
static LIST_HEAD(dmc_dev_access_list);

static int dmc_name_get_id(char *port_name)
{
	int i, len;

	if (!dmc_mon) {
		pr_err("dmc monitor not register\n");
		return -1;
	}

	for (i = 0; i < dmc_mon->port_num; i++) {
		len = strlen(dmc_mon->port[i].port_name);
		if (!strncmp(dmc_mon->port[i].port_name, port_name, len))
			break;
	}
	if (i >= dmc_mon->port_num) {
		pr_err("%s not find in dmc\n", port_name);
		return -1;
	}

	return dmc_mon->port[i].port_id;
}

int register_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb)
{
	int err;
	struct dmc_notifier_list *dmc_notifier = NULL;

	dmc_notifier = kzalloc(sizeof(*dmc_notifier), GFP_KERNEL);
	if (!dmc_notifier)
		return -1;

	dmc_notifier->port_id = dmc_name_get_id(dev_name);
	snprintf(dmc_notifier->port_name, MAX_NAME, "%s", dev_name);
	list_add(&dmc_notifier->list, &dmc_dev_access_list);

	err = blocking_notifier_chain_register(&dmc_dev_access_nofitier_chain, nb);
	return dmc_notifier->port_id;
}
EXPORT_SYMBOL(register_dmc_dev_access_notifier);

int unregister_dmc_dev_access_notifier(char *dev_name, struct notifier_block *nb)
{
	struct dmc_notifier_list *dmc_notifier = NULL;

	list_for_each_entry(dmc_notifier, &dmc_dev_access_list, list) {
		if (!strncmp(dmc_notifier->port_name, dev_name, strlen(dev_name)))
			break;
	}
	if (&dmc_notifier->list != &dmc_dev_access_list) {
		list_del(&dmc_notifier->list);
		kfree(dmc_notifier);
	}
	return blocking_notifier_chain_unregister(&dmc_dev_access_nofitier_chain, nb);
}
EXPORT_SYMBOL(unregister_dmc_dev_access_notifier);

static int call_dmc_dev_access_notifiers(unsigned char id, struct dmc_dev_access_data *data)
{
	return blocking_notifier_call_chain(&dmc_dev_access_nofitier_chain, id, data);
}

void dmc_dev_access(unsigned char id, unsigned long addr, unsigned long size)
{
	struct dmc_dev_access_data data;

	data.addr = addr;
	data.size = size;
	call_dmc_dev_access_notifiers(id, &data);
}

int show_dmc_notifier_list(char *buf)
{
	struct dmc_notifier_list *dmc_notifier = NULL;
	int len = 0;

	len += sprintf(buf + len, "%s\n", "dmc dev register callback list:");
	list_for_each_entry(dmc_notifier, &dmc_dev_access_list, list) {
		if (dmc_notifier->port_id == (char)-1)
			len += sprintf(buf + len, "%3d : %s [need check name]\n",
					dmc_notifier->port_id,
					dmc_notifier->port_name);
		else
			len += sprintf(buf + len, "%3d : %s\n",
					dmc_notifier->port_id,
					dmc_notifier->port_name);
	}
	return len;
}
