/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_HDCP_H__
#define __HDMI_HDCP_H__
#include <linux/wait.h>
#include "../hdmi_tx_ext.h"

struct hdcp_obs_val {
	unsigned char obs0;
	unsigned char obs1;
	unsigned char obs2;
	unsigned char obs3;
	unsigned char intstat;
};

#define HDCP_LOG_SIZE 4096 /* 4KB */
struct hdcplog_buf {
	unsigned int wr_pos;
	unsigned int rd_pos;
	wait_queue_head_t wait;
	unsigned char buf[HDCP_LOG_SIZE];
};

#endif
