/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_EXT_H__
#define __HDMI_TX_EXT_H__

#include "hdmi_common.h"

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
struct hdmitx_dev *get_hdmitx_device(void);
int get_hpd_state(void);
bool is_tv_changed(void);
int hdmitx_event_notifier_regist(struct notifier_block *nb);
int hdmitx_event_notifier_unregist(struct notifier_block *nb);
#else
static inline struct hdmitx_dev *get_hdmitx_device(void)
{
	return NULL;
}

static inline int get_hpd_state(void)
{
	return 0;
}

static inline int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
	return -EINVAL;
}

static inline int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
	return -EINVAL;
}
#endif

typedef void (*pf_callback)(bool st);

#ifdef CONFIG_AMLOGIC_HDMITX
	pf_callback *hdmitx_earc_hpdst(void);
#else
	#define hdmitx_earc_hpdst NULL
#endif

#ifdef CONFIG_AMLOGIC_HDMITX21
	pf_callback *hdmitx21_earc_hpdst(void);
#else
	#define hdmitx21_earc_hpdst NULL
#endif

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

struct aud_para {
	enum hdmi_audio_type type;
	enum hdmi_audio_fs rate;
	enum hdmi_audio_sampsize size;
	enum hdmi_audio_chnnum chs;
	bool fifo_rst;
};

#endif
