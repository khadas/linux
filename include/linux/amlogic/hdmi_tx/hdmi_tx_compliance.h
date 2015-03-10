/*
 * Amlogic Meson HDMI Transmitter Driver
 * hdmitx driver-----------HDMI_TX
 * Copyright (C) 2013 Amlogic, Inc.
 * Author: zongdong.jiao@amlogic.com
 *
 * In order to get better HDMI TX compliance,
 * you can add special code here, such as clock configure.
 *
 * Function hdmitx_special_operation() is called by
 * hdmitx_m3_set_dispmode() at the end
 *
 */

#ifndef __HDMI_TX_COMPLIANCE_H
#define __HDMI_TX_COMPLIANCE_H

#include "hdmi_info_global.h"
#include "hdmi_tx_module.h"

void hdmitx_special_handler_video(struct Hdmitx_Dev *hdmitx_device);
void hdmitx_special_handler_audio(struct Hdmitx_Dev *hdmitx_device);

#endif

