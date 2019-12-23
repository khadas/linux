/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RPTX_H_
#define _HDMI_RPTX_H_

enum rptx_hdcp14_cmd {
	RPTX_HDCP14_OFF,
	RPTX_HDCP14_ON,
	RPTX_HDCP14_GET_AUTHST,
};

void direct_hdcptx14_opr(enum rptx_hdcp14_cmd cmd, void *args);

#endif
