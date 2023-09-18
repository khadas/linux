// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_compliance.h>
#include "hw/common.h"

int hdmitx_set_audio(struct hdmitx_dev *hdmitx_device,
	struct hdmitx_audpara *audio_param)
{
	int i, ret = -1;
	unsigned char AUD_DB[32];
	unsigned char CHAN_STAT_BUF[24 * 2];

	for (i = 0; i < 32; i++)
		AUD_DB[i] = 0;
	for (i = 0; i < (24 * 2); i++)
		CHAN_STAT_BUF[i] = 0;
	if (hdmitx_device->hwop.setaudmode(hdmitx_device,
					   audio_param) >= 0) {
		hdmitx_device->hwop.setaudioinfoframe(AUD_DB, CHAN_STAT_BUF);
		ret = 0;
	}
	return ret;
}
