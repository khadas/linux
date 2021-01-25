/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __HDMI_EARC_H__
#define __HDMI_EARC_H__

int register_earcrx_callback(void (*callback)(bool st));
void unregister_earcrx_callback(void);
int register_earctx_callback(void (*callback)(int earc_port, bool st));
void unregister_earctx_callback(void);

#endif
