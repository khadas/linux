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

#undef PCM_USE_INFOFRAME

static const u8 cs_freq[] = {
	0x0,
	0x3, /*32K*/
	0x0, /*44.1k*/
	0x2, /*48k*/
	0x8, /*88.2k*/
	0xa, /*96k*/
	0xc, /*176.4k*/
	0xe, /*192k*/
};

static const u8 cs_sw_len[] = {
	0x0,
	0x2, /*16 bits*/
	0x3, /*20 bits*/
	0xb  /*24 bits*/
};

static void
hdmi_tx_construct_aud_packet(struct hdmitx_audpara *audio_param,
			     u8 *AUD_DB,
			     u8 *CHAN_STAT_BUF,
			     int hdmi_ch)
{
#ifndef PCM_USE_INFOFRAME
	if (audio_param->type == CT_PCM) {
		pr_info(AUD "Audio Type: PCM\n");
		if (AUD_DB) {
			/*Note: HDMI Spec V1.4 Page 154*/
			if (audio_param->channel_num == CC_2CH ||
			    audio_param->channel_num == CC_REFER_TO_STREAM)
				AUD_DB[0] = 0;
			else
				AUD_DB[0] = 0 << 4 |
				audio_param->channel_num;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[2] = 0x0;
			if (audio_param->channel_num == CC_6CH) {
				AUD_DB[3] = 0xb;
			} else if (audio_param->channel_num == CC_8CH) {
				if (hdmi_ch == CC_6CH)
					AUD_DB[3] = 0x0b;
				else
					AUD_DB[3] = 0x13;
			} else {
				AUD_DB[3] = 0;
			}
			AUD_DB[4] = 0;
		}
		if (CHAN_STAT_BUF) {
			CHAN_STAT_BUF[2] = 0x10 |
			(audio_param->channel_num + 1);
			CHAN_STAT_BUF[24 + 2] = 0x20 |
			(audio_param->channel_num + 1);
			CHAN_STAT_BUF[3] =
				cs_freq[audio_param->sample_rate];
			CHAN_STAT_BUF[24 + 3] =
				cs_freq[audio_param->sample_rate];
			CHAN_STAT_BUF[4] =
			cs_sw_len[audio_param->sample_size] |
			~cs_freq[audio_param->sample_rate] << 4;
			CHAN_STAT_BUF[24 + 4] =
			cs_sw_len[audio_param->sample_size] |
			~cs_freq[audio_param->sample_rate] << 4;
		}
	} else if (audio_param->type == CT_AC_3) {
		pr_info(AUD "Audio Type: AC3\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_AC_3 << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_MPEG1) {
		pr_info(AUD "Audio Type: MPEG1\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_MPEG1 << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_MP3) {
		pr_info(AUD "Audio Type: MP3\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_MP3 << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_MPEG2) {
		pr_info(AUD "Audio Type: MPEG2\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_MPEG2 << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_AAC) {
		pr_info(AUD "Audio Type: AAC\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_AAC << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_DTS) {
		pr_info(AUD "Audio Type: DTS\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_DTS << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_ATRAC) {
		pr_info(AUD "Audio Type: ATRAC\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_ATRAC << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_ONE_BIT_AUDIO) {
		pr_info(AUD "Audio Type: One Bit Audio\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_ONE_BIT_AUDIO << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_DOLBY_D) {
		pr_info(AUD "Audio Type: Dobly Digital +\n");
		if (AUD_DB) {
			AUD_DB[0] = FS_REFER_TO_STREAM << 4 |
			CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
		if (CHAN_STAT_BUF) {
			CHAN_STAT_BUF[0] = 0x2;
			CHAN_STAT_BUF[24 + 0] = 0x2;
			CHAN_STAT_BUF[3] = 0x1e;
			CHAN_STAT_BUF[24 + 3] = 0x1e;
			CHAN_STAT_BUF[4] = 0x1;
			CHAN_STAT_BUF[24 + 4] = 0x1;
		}
	} else if (audio_param->type == CT_DTS_HD) {
		pr_info(AUD "Audio Type: DTS-HD\n");
		if (AUD_DB) {
			AUD_DB[0] = FS_REFER_TO_STREAM << 4 |
			CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_MAT) {
		pr_info(AUD "Audio Type: MAT(MLP)\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_MAT << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_DST) {
		pr_info(AUD "Audio Type: DST\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_DST << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else if (audio_param->type == CT_WMA) {
		pr_info(AUD "Audio Type: WMA Pro\n");
		if (AUD_DB) {
			AUD_DB[0] = CT_WMA << 4 | CC_REFER_TO_STREAM;
			AUD_DB[1] = FS_REFER_TO_STREAM << 2 |
			SS_REFER_TO_STREAM;
			AUD_DB[3] = 0;
			AUD_DB[4] = 0;
		}
	} else {
		;
	}
	if (AUD_DB) {
		AUD_DB[0] = AUD_DB[0] & 0xf;/*bit[7:4] always set to 0 in HDMI*/
		AUD_DB[1] = 0;		/*always set to 0 in HDMI*/
	}
#endif
}

int hdmitx21_set_audio(struct hdmitx_dev *hdmitx_device,
		     struct hdmitx_audpara *audio_param)
{
	int i, ret = -1;
	u8 AUD_DB[32];
	u8 CHAN_STAT_BUF[24 * 2];
	u32 hdmi_ch = hdmitx_device->hdmi_ch;

	for (i = 0; i < 32; i++)
		AUD_DB[i] = 0;
	for (i = 0; i < (24 * 2); i++)
		CHAN_STAT_BUF[i] = 0;
	if (hdmitx_device->hwop.setaudmode(hdmitx_device,
					   audio_param) >= 0) {
		hdmi_tx_construct_aud_packet(audio_param, AUD_DB,
					     CHAN_STAT_BUF, hdmi_ch);

		hdmitx_device->hwop.setinfoframe(HDMI_AUDIO_INFO, AUD_DB, CHAN_STAT_BUF);
		ret = 0;
	}
	return ret;
}
