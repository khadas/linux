/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_HDCP_H
#define __AM_MESON_HDCP_H

#define HDCP_SLAVE     0x3a
#define HDCP2_VERSION  0x50
#define HDCP_MODE14    1
#define HDCP_MODE22    2

#define HDCP_QUIT      0
#define HDCP14_ENABLE  1
#define HDCP14_AUTH    2
#define HDCP14_SUCCESS 3
#define HDCP14_FAIL    4
#define HDCP22_ENABLE  5
#define HDCP22_AUTH    6
#define HDCP22_SUCCESS 7
#define HDCP22_FAIL    8
#define HDCP_READY     9

int am_hdcp_init(struct am_hdmi_tx *am_hdmi);
int is_hdcp_hdmitx_supported(struct am_hdmi_tx *am_hdmi);
int am_hdcp_work(void *data);
void am_hdcp_disable(struct am_hdmi_tx *am_hdmi);

#endif
