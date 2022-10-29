// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/amlogic/media/vout/dsc/dsc.h>

static ATOMIC_NOTIFIER_HEAD(dsc_atomic_notifier_list);

/**
 * aml_dsc_atomic_notifier_register - register a client notifier
 * @nb: notifier block to callback on events
 */
int aml_dsc_atomic_notifier_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&dsc_atomic_notifier_list, nb);
}
EXPORT_SYMBOL(aml_dsc_atomic_notifier_register);

/**
 * aml_dsc_atomic_notifier_unregister - unregister a client notifier
 * @nb: notifier block to callback on events
 */
int aml_dsc_atomic_notifier_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&dsc_atomic_notifier_list, nb);
}
EXPORT_SYMBOL(aml_dsc_atomic_notifier_unregister);

/**
 * aml_dsc_atomic_notifier_call_chain - notify clients of lcd events
 *
 */
int aml_dsc_atomic_notifier_call_chain(unsigned long event, void *v)
{
	return atomic_notifier_call_chain(&dsc_atomic_notifier_list, event, v);
}
EXPORT_SYMBOL_GPL(aml_dsc_atomic_notifier_call_chain);

