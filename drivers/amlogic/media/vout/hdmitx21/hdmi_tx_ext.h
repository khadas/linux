/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_EXT_H
#define __HDMI_EXT_H
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>

/* below are extern functions declaration
 * only for hdmitx20/21 module internally
 */
#ifdef CONFIG_AMLOGIC_HDMITX
	void hdmitx_earc_hpdst(pf_callback cb);
#else
	#define hdmitx_earc_hpdst NULL
#endif

#ifdef CONFIG_AMLOGIC_HDMITX21
	void hdmitx21_earc_hpdst(pf_callback cb);
#else
	#define hdmitx21_earc_hpdst NULL
#endif

#ifdef CONFIG_AMLOGIC_HDMITX
/* hdmitx20 external interface */
int hdmitx20_event_notifier_regist(struct notifier_block *nb);
int hdmitx20_event_notifier_unregist(struct notifier_block *nb);
int get_hdmitx20_init(void);
int get20_hpd_state(void);
struct vsdb_phyaddr *get_hdmitx20_phy_addr(void);
void setup20_attr(const char *buf);
void get20_attr(char attr[16]);
void hdmitx20_audio_mute_op(unsigned int flag);
void hdmitx20_video_mute_op(unsigned int flag);
void hdmitx20_ext_set_audio_output(int enable);
int hdmitx20_ext_get_audio_status(void);
void hdmitx20_ext_set_i2s_mask(char ch_num, char ch_msk);
char hdmitx20_ext_get_i2s_mask(void);
#endif

#ifdef CONFIG_AMLOGIC_HDMITX21
/* hdmitx21 external interface */
int hdmitx21_event_notifier_regist(struct notifier_block *nb);
int hdmitx21_event_notifier_unregist(struct notifier_block *nb);
int get_hdmitx21_init(void);
int get21_hpd_state(void);
struct vsdb_phyaddr *get_hdmitx21_phy_addr(void);
void get21_attr(char attr[16]);
void setup21_attr(const char *buf);
void hdmitx21_video_mute_op(u32 flag, unsigned int path);
void hdmitx21_audio_mute_op(u32 flag, unsigned int path);
void hdmitx21_ext_set_audio_output(int enable);
int hdmitx21_ext_get_audio_status(void);
void hdmitx21_ext_set_i2s_mask(char ch_num, char ch_msk);
char hdmitx21_ext_get_i2s_mask(void);
#endif

#endif

