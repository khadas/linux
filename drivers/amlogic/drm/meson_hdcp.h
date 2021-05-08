/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDCP_H__
#define __MESON_HDCP_H__


enum {
	HDCP_NULL = 0,
	HDCP_MODE14 = 1 << 0,
	HDCP_MODE22 = 1 << 1,
};

enum {
	HDCP_AUTH_FAIL = 0,
	HDCP_AUTH_OK = 1,
	HDCP_AUTH_UNKNOWN = 0xff,
};

typedef void (*hdcp_notify)(int type, int auth_result);

void meson_hdcp_init(void);
void meson_hdcp_exit(void);

void meson_hdcp_enable(int hdcp_type);
void meson_hdcp_disable(void);
void meson_hdcp_disconnect(void);

unsigned int meson_hdcp_get_key_version(void);
int meson_hdcp_get_valid_type(int request_type_mask);
void meson_hdcp_reg_result_notify(hdcp_notify cb);

#endif
