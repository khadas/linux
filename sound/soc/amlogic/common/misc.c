// SPDX-License-Identifier: GPL-2.0
/*
 * misc function interface
 *
 * Copyright (C) 2019 Amlogic,inc
 *
 */

#include <sound/asoundef.h>

#if (defined CONFIG_AMLOGIC_ATV_DEMOD ||\
		defined CONFIG_AMLOGIC_ATV_DEMOD_MODULE)
#include <linux/amlogic/aml_atvdemod.h>
#endif

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif

#include "misc.h"

static const char *const audio_is_stable[] = {
	"false",
	"true"
};

#if (defined CONFIG_AMLOGIC_ATV_DEMOD ||\
		defined CONFIG_AMLOGIC_ATV_DEMOD_MODULE)

const struct soc_enum atv_audio_status_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_is_stable),
			audio_is_stable);

int aml_get_atv_audio_stable(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int state = 0;

	aml_fe_get_atvaudio_state(&state);
	ucontrol->value.integer.value[0] = state;
	return 0;
}
#endif /* CONFIG_AMLOGIC_ATV_DEMOD */

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT_MODULE)

const struct soc_enum av_audio_status_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_is_stable),
			audio_is_stable);

int aml_get_av_audio_stable(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tvin_get_av_status();
	return 0;
}
#endif /* CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT */

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
int hdmiin_fifo_disable_count;

int get_hdmi_sample_rate_index(void)
{
	struct rx_audio_stat_s aud_sts;
	int val = 0;

	rx_get_audio_status(&aud_sts);
	switch (aud_sts.aud_sr) {
	case 0:
		val = 0;
		break;
	case 32000:
		val = 1;
		break;
	case 44100:
		val = 2;
		break;
	case 48000:
		val = 3;
		break;
	case 88200:
		val = 4;
		break;
	case 96000:
		val = 5;
		break;
	case 176400:
		val = 6;
		break;
	case 192000:
		val = 7;
		break;
	default:
		pr_err("HDMIRX samplerate not support: %d\n",
			aud_sts.aud_sr);
		break;
	}
	return val;
}

int update_spdifin_audio_type(int audio_type)
{
	struct rx_audio_stat_s aud_sts;

	rx_get_audio_status(&aud_sts);
	if (aud_sts.afifo_thres_pass)
		hdmiin_fifo_disable_count = 0;
	else
		hdmiin_fifo_disable_count++;
	if (hdmiin_fifo_disable_count > 200)
		audio_type = 6/*PAUSE*/;

	return audio_type;
}

static const char *const hdmi_in_samplerate[] = {
	"N/A",
	"32000",
	"44100",
	"48000",
	"88200",
	"96000",
	"176400",
	"192000"
};

static const char *const hdmi_in_channels[] = {
	"NONE",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8"
};

enum HDMIIN_format {
	REFER_TO_HEADER = 0,
	LPCM = 1,
	AC3,
	MPEG1,
	MP3,
	MPEG2,
	AAC,
	DTS,
	ATRAC,
	ONE_BIT_AUDIO,
	DDP,
	DTS_HD,
	MAT,
	DST,
	WMA_PRO
};

static const char * const hdmi_in_format[] = {
	"REFER_TO_HEADER",
	"LPCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"ONE_BIT_AUDIO",
	"DDP",
	"DTS_HD",
	"MAT",
	"DST",
	"WMA_PRO"
};

static const char * const hdmi_in_audio_packet[] = {
	"None",
	"AUDS",
	"OBA",
	"DST",
	"HBR",
	"OBM",
	"MAS"
};

static const char * const hdmi_in_bitwidth[] = {
	"REFER_TO_HEADER",
	"16bit",
	"20bit",
	"24bit"
};

static const char * const hdmi_in_nonaudio[] = {
	"PCM",
	"NONAUDIO"
};

const struct soc_enum hdmi_in_status_enum[] = {
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_is_stable),
			audio_is_stable),
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hdmi_in_samplerate),
			hdmi_in_samplerate),
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hdmi_in_channels),
			hdmi_in_channels),
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hdmi_in_format),
			hdmi_in_format),
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hdmi_in_audio_packet),
			hdmi_in_audio_packet),
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hdmi_in_bitwidth),
			hdmi_in_bitwidth),
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hdmi_in_nonaudio),
			hdmi_in_nonaudio)
};

int get_hdmiin_audio_stable(void)
{
	struct rx_audio_stat_s aud_sts;
	bool audio_packet = 0;
	bool rx_sample_rate = 0;

	rx_get_audio_status(&aud_sts);
	rx_sample_rate = (aud_sts.aud_sr > 0) ? 1 : 0;
	audio_packet = (aud_sts.aud_rcv_packet == 0) ? 0 : 1;

	return (aud_sts.aud_stb_flag && audio_packet && rx_sample_rate);
}

int aml_get_hdmiin_audio_stable(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_hdmiin_audio_stable();

	return 0;
}

int aml_get_hdmiin_audio_samplerate(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int val = get_hdmi_sample_rate_index();

	ucontrol->value.integer.value[0] = val;

	return 0;
}

int aml_get_hdmiin_audio_channels(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct rx_audio_stat_s aud_sts;

	rx_get_audio_status(&aud_sts);
	if (aud_sts.aud_channel_cnt <= 7)
		ucontrol->value.integer.value[0] = aud_sts.aud_channel_cnt;

	return 0;
}

int aml_get_hdmiin_audio_allocation(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct rx_audio_stat_s aud_sts;

	rx_get_audio_status(&aud_sts);
	ucontrol->value.integer.value[0] = aud_sts.aud_alloc;

	return 0;
}

int aml_get_hdmiin_audio_format(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct rx_audio_stat_s aud_sts;

	rx_get_audio_status(&aud_sts);
	if (aud_sts.aud_type <= 14)
		ucontrol->value.integer.value[0] = aud_sts.aud_type;

	return 0;
}

/* call HDMI CEC API to enable arc audio */
int aml_set_atmos_audio_edid(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	bool enable = ucontrol->value.integer.value[0];

	rx_set_atmos_flag(enable);

	return 0;
}

int aml_get_atmos_audio_edid(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	bool flag = rx_get_atmos_flag();

	ucontrol->value.integer.value[0] = flag;

	return 0;
}

int aml_get_hdmiin_audio_packet(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct rx_audio_stat_s aud_sts;
	int val;

	rx_get_audio_status(&aud_sts);

	switch (aud_sts.aud_rcv_packet) {
	case 0:
		val = 0;
		break;
	case 1:
		val = 1;
		break;
	case 2:
		val = 2;
		break;
	case 4:
		val = 3;
		break;
	case 8:
		val = 4;
		break;
	case 16:
		val = 5;
		break;
	case 32:
		val = 6;
		break;
	default:
		val = 0;
		break;
	}
	ucontrol->value.integer.value[0] = val;

	return 0;
}

int aml_get_hdmiin_nonaudio(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct rx_audio_stat_s aud_sts;
	int nonaudio = 0;
	(void)kcontrol;

	rx_get_audio_status(&aud_sts);
	if (aud_sts.ch_sts[0] & IEC958_AES0_NONAUDIO)
		nonaudio = 1;

	ucontrol->value.integer.value[0] = nonaudio;

	return 0;
}
#endif
