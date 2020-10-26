/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_MHU_H__
#define __MESON_MHU_H__
#include <linux/amlogic/meson_mhu_common.h>

#define CONTROLLER_NAME		"mhu_ctlr"

#define CHANNEL_MAX		2
#define CHANNEL_LOW_PRIORITY	"cpu_to_scp_low"
#define CHANNEL_HIGH_PRIORITY	"cpu_to_scp_high"

extern u32 num_scp_chans;
extern u32 send_listen_chans;

int __init aml_mhu_init(void);
void aml_mhu_exit(void);
#endif
