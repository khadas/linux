/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __MISC_H__
#define __MISC_H__

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/control.h>

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
extern const struct soc_enum atv_audio_status_enum;

int aml_get_atv_audio_stable(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
int tvin_get_av_status(void);
extern const struct soc_enum av_audio_status_enum;

int aml_get_av_audio_stable(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
int update_spdifin_audio_type(int audio_type);

extern const struct soc_enum hdmi_in_status_enum[];

int get_hdmi_sample_rate_index(void);

int aml_get_hdmiin_audio_stable(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

int aml_get_hdmiin_audio_samplerate(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol);

int aml_get_hdmiin_audio_channels(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

int aml_get_hdmiin_audio_format(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

int aml_get_hdmiin_audio_bitwidth(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

int aml_set_atmos_audio_edid(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

int aml_get_atmos_audio_edid(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

int aml_get_hdmiin_audio_packet(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

int get_hdmiin_audio_stable(void);
int get_hdmi_sample_rate_index(void);

#endif

#endif
