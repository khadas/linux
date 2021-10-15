// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ext.h>
#include "hdmi_tx_ext.h"

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
/* for notify to cec */
int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
	return _hdmitx_event_notifier_regist(nb);
}
EXPORT_SYMBOL(hdmitx_event_notifier_regist);

int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
	return _hdmitx_event_notifier_unregist(nb);
}
EXPORT_SYMBOL(hdmitx_event_notifier_unregist);
#endif

int register_earcrx_callback(pf_callback callback)
{
#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	pf_callback *hdmitx;
#endif
#ifdef CONFIG_AMLOGIC_HDMITX
	hdmitx = hdmitx_earc_hpdst();
	*hdmitx = callback;
#endif

#ifdef CONFIG_AMLOGIC_HDMITX21
	hdmitx = hdmitx21_earc_hpdst();
	*hdmitx = callback;
#endif

	return 0;
}
EXPORT_SYMBOL(register_earcrx_callback);

void unregister_earcrx_callback(void)
{
#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	pf_callback *hdmitx;
#endif
#ifdef CONFIG_AMLOGIC_HDMITX
	hdmitx = hdmitx_earc_hpdst();
	*hdmitx = NULL;
#endif

#ifdef CONFIG_AMLOGIC_HDMITX21
	hdmitx = hdmitx21_earc_hpdst();
	*hdmitx = NULL;
#endif
}
EXPORT_SYMBOL(unregister_earcrx_callback);
