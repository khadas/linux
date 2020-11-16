/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __SPDIF_INFO_H__
#define __SPDIF_INFO_H__

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/control.h>

struct iec958_chsts {
	unsigned short chstat0_l;
	unsigned short chstat1_l;
	unsigned short chstat0_r;
	unsigned short chstat1_r;
};

bool spdifout_is_raw(void);
bool spdif_is_4x_clk(void);
void spdif_get_channel_status_info(struct iec958_chsts *chsts,
				   unsigned int rate);
void spdif_notify_to_hdmitx(struct snd_pcm_substream *substream);
extern const struct soc_enum spdif_format_enum;
int spdif_format_get_enum(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
int spdif_format_set_enum(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
#ifdef CONFIG_AMLOGIC_HDMITX
int aml_get_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int aml_set_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
#endif
#endif
