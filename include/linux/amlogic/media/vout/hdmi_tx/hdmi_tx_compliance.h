/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_COMPLIANCE_H
#define __HDMI_TX_COMPLIANCE_H

#include "hdmi_info_global.h"
#include "hdmi_tx_module.h"

void hdmitx_special_handler_video(struct hdmitx_dev *hdmitx_device);
void hdmitx_special_handler_audio(struct hdmitx_dev *hdmitx_device);

#endif

