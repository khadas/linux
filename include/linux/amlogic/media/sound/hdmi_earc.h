/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __HDMI_EARC_H__
#define __HDMI_EARC_H__

void earc_hdmirx_hpdst(int port, bool st);
int register_earcrx_callback(void (*callbakck)(bool st));
void unregister_earcrx_callback(void);

#endif
