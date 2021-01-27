/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_EXT_H__
#define __HDMI_TX_EXT_H__

void direct_hdcptx14_start(void);
void direct_hdcptx14_stop(void);

/*
 * HDMI TX output enable, such as ACRPacket/AudInfo/AudSample
 * enable: 1, normal output; 0: disable output
 */
void hdmitx_ext_set_audio_output(int enable);

/*
 * return Audio output status
 * 1: normal output status; 0: output disabled
 */
int hdmitx_ext_get_audio_status(void);

/*
 * For I2S interface, there are four input ports
 * I2S_0/I2S_1/I2S_2/I2S_3
 * ch_num: must be 2/4/6/8
 * ch_msk: Mask for channel_num
 * 2ch via I2S_0, set ch_num = 2 and ch_msk = 1
 * 4ch via I2S_1/I2S_2, set ch_num = 4 and ch_msk = 6
 */
void hdmitx21_ext_set_i2s_mask(char ch_num, char ch_msk);

/*
 * get I2S mask
 */
char hdmitx21_ext_get_i2s_mask(void);

#endif
