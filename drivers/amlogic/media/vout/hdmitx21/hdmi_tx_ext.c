// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include "hdmi_tx_ext.h"

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
/* for notify to cec */
int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
	if (get_hdmitx20_init() == 1)
		return hdmitx20_event_notifier_regist(nb);
	else if (get_hdmitx21_init() == 1)
		return hdmitx21_event_notifier_regist(nb);

	return 1;
}
EXPORT_SYMBOL(hdmitx_event_notifier_regist);

int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
	if (get_hdmitx20_init() == 1)
		return hdmitx20_event_notifier_unregist(nb);
	else if (get_hdmitx21_init() == 1)
		return hdmitx21_event_notifier_unregist(nb);

	return 1;
}
EXPORT_SYMBOL(hdmitx_event_notifier_unregist);

int get_hpd_state(void)
{
	int ret = 0;

	if (get_hdmitx20_init() == 1)
		ret = get20_hpd_state();
	else if (get_hdmitx21_init() == 1)
		ret = get21_hpd_state();

	return ret;
}
EXPORT_SYMBOL(get_hpd_state);

struct vsdb_phyaddr *get_hdmitx_phy_addr(void)
{
	if (get_hdmitx20_init() == 1)
		return get_hdmitx20_phy_addr();
	else if (get_hdmitx21_init() == 1)
		return get_hdmitx21_phy_addr();

	return NULL;
}
EXPORT_SYMBOL(get_hdmitx_phy_addr);

void get_attr(char attr[16])
{
	if (get_hdmitx20_init() == 1)
		get20_attr(attr);
	else if (get_hdmitx21_init() == 1)
		get21_attr(attr);
}
EXPORT_SYMBOL(get_attr);

void setup_attr(const char *buf)
{
	if (get_hdmitx20_init() == 1)
		setup20_attr(buf);
	else if (get_hdmitx21_init() == 1)
		setup21_attr(buf);
}
EXPORT_SYMBOL(setup_attr);

/*
 * hdmitx_audio_mute_op() is used by external driver call
 * flag: 0: audio off   1: audio_on
 *       2: for EDID auto mode
 */
void hdmitx_audio_mute_op(unsigned int flag)
{
	if (get_hdmitx20_init() == 1)
		hdmitx20_audio_mute_op(flag);
	else if (get_hdmitx21_init() == 1)
		hdmitx21_audio_mute_op(flag);
}
EXPORT_SYMBOL(hdmitx_audio_mute_op);

void hdmitx_video_mute_op(u32 flag)
{
	if (get_hdmitx20_init() == 1)
		hdmitx20_video_mute_op(flag);
	else if (get_hdmitx21_init() == 1)
		hdmitx21_video_mute_op(flag);
}
EXPORT_SYMBOL(hdmitx_video_mute_op);

void hdmitx_ext_set_audio_output(int enable)
{
	if (get_hdmitx20_init() == 1)
		hdmitx20_ext_set_audio_output(enable);
	else if (get_hdmitx21_init() == 1)
		hdmitx21_ext_set_audio_output(enable);
}
EXPORT_SYMBOL(hdmitx_ext_set_audio_output);

int hdmitx_ext_get_audio_status(void)
{
	if (get_hdmitx20_init() == 1)
		return hdmitx20_ext_get_audio_status();
	else if (get_hdmitx21_init() == 1)
		return hdmitx21_ext_get_audio_status();

	return 0;
}
EXPORT_SYMBOL(hdmitx_ext_get_audio_status);

void hdmitx_ext_set_i2s_mask(char ch_num, char ch_msk)
{
	if (get_hdmitx20_init() == 1)
		return hdmitx20_ext_set_i2s_mask(ch_num, ch_msk);
	else if (get_hdmitx21_init() == 1)
		return hdmitx21_ext_set_i2s_mask(ch_num, ch_msk);
}
EXPORT_SYMBOL(hdmitx_ext_set_i2s_mask);

char hdmitx_ext_get_i2s_mask(void)
{
	if (get_hdmitx20_init() == 1)
		return hdmitx20_ext_get_i2s_mask();
	else if (get_hdmitx21_init() == 1)
		return hdmitx21_ext_get_i2s_mask();

	return 0;
}
EXPORT_SYMBOL(hdmitx_ext_get_i2s_mask);

int register_earcrx_callback(pf_callback callback)
{
	pf_callback *hdmitx;

	if (get_hdmitx20_init() == 1) {
		hdmitx = hdmitx_earc_hpdst();
		*hdmitx = callback;
	} else if (get_hdmitx21_init() == 1) {
		hdmitx = hdmitx21_earc_hpdst();
		*hdmitx = callback;
	}
	return 0;
}
EXPORT_SYMBOL(register_earcrx_callback);

void unregister_earcrx_callback(void)
{
	pf_callback *hdmitx;

	if (get_hdmitx20_init() == 1) {
		hdmitx = hdmitx_earc_hpdst();
		*hdmitx = NULL;
	} else if (get_hdmitx21_init() == 1) {
		hdmitx = hdmitx21_earc_hpdst();
		*hdmitx = NULL;
	}
}
EXPORT_SYMBOL(unregister_earcrx_callback);
#endif

