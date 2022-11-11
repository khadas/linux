// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include "hdmi_tx_ext.h"

/* for notify to cec */
int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		return hdmitx20_event_notifier_regist(nb);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		return hdmitx21_event_notifier_regist(nb);
#endif
	return 1;
}
EXPORT_SYMBOL(hdmitx_event_notifier_regist);

int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		return hdmitx20_event_notifier_unregist(nb);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		return hdmitx21_event_notifier_unregist(nb);
#endif
	return 1;
}
EXPORT_SYMBOL(hdmitx_event_notifier_unregist);

int get_hpd_state(void)
{
	int ret = 0;

#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		ret = get20_hpd_state();
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		ret = get21_hpd_state();
#endif
	return ret;
}
EXPORT_SYMBOL(get_hpd_state);

struct vsdb_phyaddr *get_hdmitx_phy_addr(void)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		return get_hdmitx20_phy_addr();
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		return get_hdmitx21_phy_addr();
#endif
	return NULL;
}
EXPORT_SYMBOL(get_hdmitx_phy_addr);

void get_attr(char attr[16])
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		get20_attr(attr);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		get21_attr(attr);
#endif
}
EXPORT_SYMBOL(get_attr);

void setup_attr(const char *buf)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		setup20_attr(buf);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		setup21_attr(buf);
#endif
}
EXPORT_SYMBOL(setup_attr);

/*
 * hdmitx_audio_mute_op() is used by external driver call
 * flag: 0: audio off   1: audio_on
 *       2: for EDID auto mode
 */
void hdmitx_audio_mute_op(unsigned int flag)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		hdmitx20_audio_mute_op(flag);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	/* 0x8000000: AUDIO_MUTE_PATH_1 */
	if (get_hdmitx21_init() == 1)
		hdmitx21_audio_mute_op(flag, 0x8000000);
#endif
}
EXPORT_SYMBOL(hdmitx_audio_mute_op);

void hdmitx_video_mute_op(u32 flag)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		hdmitx20_video_mute_op(flag);
#endif
/* just for debug, path = 0 */
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		hdmitx21_video_mute_op(flag, 0);
#endif
}
EXPORT_SYMBOL(hdmitx_video_mute_op);

void hdmitx_ext_set_audio_output(int enable)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		hdmitx20_ext_set_audio_output(enable);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		hdmitx21_ext_set_audio_output(enable);
#endif
}
EXPORT_SYMBOL(hdmitx_ext_set_audio_output);

int hdmitx_ext_get_audio_status(void)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		return hdmitx20_ext_get_audio_status();
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		return hdmitx21_ext_get_audio_status();
#endif
	return 0;
}
EXPORT_SYMBOL(hdmitx_ext_get_audio_status);

void hdmitx_ext_set_i2s_mask(char ch_num, char ch_msk)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		return hdmitx20_ext_set_i2s_mask(ch_num, ch_msk);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		return hdmitx21_ext_set_i2s_mask(ch_num, ch_msk);
#endif
}
EXPORT_SYMBOL(hdmitx_ext_set_i2s_mask);

char hdmitx_ext_get_i2s_mask(void)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		return hdmitx20_ext_get_i2s_mask();
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		return hdmitx21_ext_get_i2s_mask();
#endif

	return 0;
}
EXPORT_SYMBOL(hdmitx_ext_get_i2s_mask);

int register_earcrx_callback(pf_callback callback)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		hdmitx_earc_hpdst(callback);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	/* ARC IN audio capture not working due to init
	 * sequence issue of eARC driver and HDMI Tx driver.
	 * when eARC driver try to register_earcrx_callback,
	 * HDMI Tx driver probe/init is not finish, that lead
	 * register_earcrx_callback fail and eARC driver
	 * doesn't know if HDMI Tx cable plug in/out.
	 * so don't check hdmitx21 init or not
	 */
	/*if (get_hdmitx21_init() == 1)*/
		hdmitx21_earc_hpdst(callback);
#endif
	return 0;
}
EXPORT_SYMBOL(register_earcrx_callback);

void unregister_earcrx_callback(void)
{
#if defined(CONFIG_AMLOGIC_HDMITX)
	if (get_hdmitx20_init() == 1)
		hdmitx_earc_hpdst(NULL);
#endif
#if defined(CONFIG_AMLOGIC_HDMITX21)
	if (get_hdmitx21_init() == 1)
		hdmitx21_earc_hpdst(NULL);
#endif
}
EXPORT_SYMBOL(unregister_earcrx_callback);

