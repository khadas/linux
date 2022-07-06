/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDCP_H__
#define __MESON_HDCP_H__
#include <drm/amlogic/meson_connector_dev.h>

void meson_hdcp_init(void);
void meson_hdcp_exit(void);

void meson_hdcp_enable(int hdcp_type);
void meson_hdcp_disable(void);
void meson_hdcp_disconnect(void);

void meson_hdcp_reg_result_notify(struct connector_hdcp_cb *cb);

bool hdcp_tx22_daemon_ready(void);
#endif
