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

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "hw/common.h"

int hdmitx21_set_audio(struct hdmitx_dev *hdev,
		     struct hdmitx_audpara *audio_param)
{
	int i, ret = -1;
	/* struct hdmi_audio_infoframe *info = &hdev->infoframes.aud.audio; */
	u8 CHAN_STAT_BUF[24 * 2];

	/* hdmi_audio_infoframe_init(info); */
	for (i = 0; i < (24 * 2); i++)
		CHAN_STAT_BUF[i] = 0;
	if (hdev->hwop.setaudmode(hdev, audio_param) >= 0) {
		/* hdmi_audio_infoframe_set(info); */
		ret = 0;
	}
	return ret;
}
