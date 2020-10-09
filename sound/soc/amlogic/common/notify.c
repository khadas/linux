// SPDX-License-Identifier: GPL-2.0
/*
 * Notify driver
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#undef pr_fmt
#define pr_fmt(fmt) "snd_notify: " fmt

#include <linux/module.h>

static BLOCKING_NOTIFIER_HEAD(aout_notifier_list);
/*
 *	aout_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int aout_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&aout_notifier_list, nb);
}
EXPORT_SYMBOL(aout_register_client);

/*
 *	aout_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int aout_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&aout_notifier_list, nb);
}
EXPORT_SYMBOL(aout_unregister_client);

/*
 * aout_notifier_call_chain - notify clients of fb_events
 *
 */
int aout_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&aout_notifier_list, val, v);
}
EXPORT_SYMBOL(aout_notifier_call_chain);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Sound notify interface");
MODULE_LICENSE("GPL v2");
