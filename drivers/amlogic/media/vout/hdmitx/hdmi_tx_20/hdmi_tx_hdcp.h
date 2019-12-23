/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_HDCP_H
#define __HDMI_TX_HDCP_H
/*
 * hdmi_tx_hdcp.c
 * version 1.0
 */

/* Notic: the HDCP key setting has been moved to uboot
 * On MBX project, it is too late for HDCP get from
 * other devices
 */
int hdcp_ksv_valid(unsigned char *dat);
int hdmitx_hdcp_init(void);

#endif

