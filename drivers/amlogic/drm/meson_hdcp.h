/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDCP_H__
#define __MESON_HDCP_H__

#define HDCP_MODE14    1
#define HDCP_MODE22    2
#define HDCP_NULL      0

enum {
	HDCP_TX22_DISCONNECT,
	HDCP_TX22_START,
	HDCP_TX22_STOP
};

void am_hdcp_disable(struct am_hdmi_tx *am_hdmi);
void am_hdcp_enable(struct am_hdmi_tx *am_hdmi);
void am_hdcp_disconnect(struct am_hdmi_tx *am_hdmi);

void hdcp_comm_init(struct am_hdmi_tx *am_hdmi);
void hdcp_comm_exit(struct am_hdmi_tx *am_hdmi);

#endif
