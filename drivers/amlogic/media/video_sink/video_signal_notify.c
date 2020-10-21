// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/video_sink/video_signal_notify.h>

static RAW_NOTIFIER_HEAD(vd_signal_notifier_list);

int vd_signal_register_client(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&vd_signal_notifier_list, nb);
}
EXPORT_SYMBOL(vd_signal_register_client);

int vd_signal_unregister_client(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&vd_signal_notifier_list, nb);
}
EXPORT_SYMBOL(vd_signal_unregister_client);

int vd_signal_notifier_call_chain(unsigned long val, void *v)
{
	return raw_notifier_call_chain(&vd_signal_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(vd_signal_notifier_call_chain);
