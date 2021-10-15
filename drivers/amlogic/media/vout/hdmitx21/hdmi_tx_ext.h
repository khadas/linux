/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_EXT_H
#define __HDMI_EXT_H

int _hdmitx_event_notifier_regist(struct notifier_block *nb);
int _hdmitx_event_notifier_unregist(struct notifier_block *nb);

#endif

