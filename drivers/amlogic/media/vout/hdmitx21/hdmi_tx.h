/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_H__
#define __HDMI_TX_H__

int hdcpksv_valid(u8 *dat);
int hdmitx21_hdcp_init(void);
int hdmitx21_uboot_audio_en(void);

int hdmitx21_init_reg_map(struct platform_device *pdev);
void hdmitx21_set_audioclk(bool en);
void hdmitx21_disable_clk(struct hdmitx_dev *hdev);
u32 hdcp21_rd_hdcp22_ver(void);
struct hdmi_timing *hdmitx21_gettiming(enum hdmi_vic vic);

#endif
