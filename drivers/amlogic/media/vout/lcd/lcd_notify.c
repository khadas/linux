// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

static BLOCKING_NOTIFIER_HEAD(lcd_block_notifier_list);
static ATOMIC_NOTIFIER_HEAD(lcd_atomic_notifier_list);

/**
 * aml_lcd_notifier_register - register a client notifier
 * @nb: notifier block to callback on events
 */
int aml_lcd_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lcd_block_notifier_list, nb);
}
EXPORT_SYMBOL(aml_lcd_notifier_register);

/**
 * aml_lcd_notifier_unregister - unregister a client notifier
 * @nb: notifier block to callback on events
 */
int aml_lcd_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lcd_block_notifier_list, nb);
}
EXPORT_SYMBOL(aml_lcd_notifier_unregister);

/**
 * aml_lcd_notifier_call_chain - notify clients of lcd events
 *
 */
int aml_lcd_notifier_call_chain(unsigned long event, void *v)
{
	return blocking_notifier_call_chain(&lcd_block_notifier_list, event, v);
}
EXPORT_SYMBOL_GPL(aml_lcd_notifier_call_chain);

/**
 * aml_lcd_notifier_register - register a client notifier
 * @nb: notifier block to callback on events
 */
int aml_lcd_atomic_notifier_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&lcd_atomic_notifier_list, nb);
}
EXPORT_SYMBOL(aml_lcd_atomic_notifier_register);

/**
 * aml_lcd_notifier_unregister - unregister a client notifier
 * @nb: notifier block to callback on events
 */
int aml_lcd_atomic_notifier_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&lcd_atomic_notifier_list, nb);
}
EXPORT_SYMBOL(aml_lcd_atomic_notifier_unregister);

/**
 * aml_lcd_notifier_call_chain - notify clients of lcd events
 *
 */
int aml_lcd_atomic_notifier_call_chain(unsigned long event, void *v)
{
	return atomic_notifier_call_chain(&lcd_atomic_notifier_list, event, v);
}
EXPORT_SYMBOL_GPL(aml_lcd_atomic_notifier_call_chain);
