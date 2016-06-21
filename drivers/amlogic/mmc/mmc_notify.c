/*
 * drivers/amlogic/mmc/mmc_notify.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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


#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/export.h>

#include <linux/amlogic/mmc_notify.h>

static BLOCKING_NOTIFIER_HEAD(mmc_notifier_list);

/**
 *	mmc_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int mmc_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mmc_notifier_list, nb);
}
EXPORT_SYMBOL(mmc_register_client);

/**
 *	mmc_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int mmc_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mmc_notifier_list, nb);
}
EXPORT_SYMBOL(mmc_unregister_client);

/**
 * mmc_notifier_call_chain - notify clients of mmc events
 *
 */
int mmc_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mmc_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(mmc_notifier_call_chain);

