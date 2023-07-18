// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#undef pr_fmt
#define pr_fmt(fmt) "iec_info: " fmt

#include <sound/asoundef.h>

#include <linux/amlogic/media/sound/aout_notify.h>
#include "iec_info.h"
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>

const struct soc_enum audio_coding_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_coding_type_names),
			audio_coding_type_names);

const struct soc_enum spdifin_sample_rate_enum[] = {
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdifin_samplerate),
			spdifin_samplerate),
};

bool audio_coding_is_lpcm(enum audio_coding_types coding_type)
{
	return ((coding_type >= AUDIO_CODING_TYPE_STEREO_LPCM) &&
		(coding_type <= AUDIO_CODING_TYPE_HBR_LPCM));
}

bool audio_coding_is_non_lpcm(enum audio_coding_types coding_type)
{
	return ((coding_type >= AUDIO_CODING_TYPE_AC3) &&
		(coding_type != AUDIO_CODING_TYPE_PAUSE));
}

int audio_multi_clk(enum audio_coding_types coding_type)
{
	int multi = 1;

	if (coding_type == AUDIO_CODING_TYPE_MULTICH_32CH_LPCM)
		multi = 16;
	else if (coding_type == AUDIO_CODING_TYPE_MULTICH_16CH_LPCM)
		multi = 8;
	else if (coding_type == AUDIO_CODING_TYPE_MULTICH_8CH_LPCM)
		multi = 4;
	else if (coding_type == AUDIO_CODING_TYPE_EAC3 ||
		 coding_type == AUDIO_CODING_TYPE_DTS_HD ||
		 coding_type == AUDIO_CODING_TYPE_MLP ||
		 coding_type == AUDIO_CODING_TYPE_DTS_HD_MA ||
		 coding_type == AUDIO_CODING_TYPE_AC3_LAYOUT_B)
		multi = 4;
	else
		multi = 1;

	return multi;
}

static unsigned int iec_rate_to_csfs(unsigned int rate)
{
	unsigned int csfs = 0;

	switch (rate) {
	case 32000:
		csfs = IEC958_AES3_CON_FS_32000;
		break;
	case 44100:
		csfs = IEC958_AES3_CON_FS_44100;
		break;
	case 48000:
		csfs = IEC958_AES3_CON_FS_48000;
		break;
	case 88200:
		csfs = IEC958_AES3_CON_FS_88200;
		break;
	case 96000:
		csfs = IEC958_AES3_CON_FS_96000;
		break;
	case 176400:
		csfs = IEC958_AES3_CON_FS_176400;
		break;
	case 192000:
		csfs = IEC958_AES3_CON_FS_192000;
		break;
	default:
		csfs = IEC958_AES3_CON_FS_NOTID;
		break;
	}

	return csfs;
}

unsigned int iec_rate_from_csfs(unsigned int csfs)
{
	unsigned int rate = 0;

	switch (csfs) {
	case IEC958_AES3_CON_FS_22050:
		rate = 22050;
		break;
	case IEC958_AES3_CON_FS_24000:
		rate = 24000;
		break;
	case IEC958_AES3_CON_FS_32000:
		rate = 32000;
		break;
	case IEC958_AES3_CON_FS_44100:
		rate = 44100;
		break;
	case IEC958_AES3_CON_FS_48000:
		rate = 48000;
		break;
	case IEC958_AES3_CON_FS_768000:
		rate = 768000;
		break;
	case IEC958_AES3_CON_FS_88200:
		rate = 88200;
		break;
	case IEC958_AES3_CON_FS_96000:
		rate = 96000;
		break;
	case IEC958_AES3_CON_FS_176400:
		rate = 176400;
		break;
	case IEC958_AES3_CON_FS_192000:
		rate = 192000;
		break;
	case IEC958_AES3_CON_FS_NOTID:
		rate = 0;
		break;
	}

	return rate;
}

void iec_get_cnsmr_cs_info(struct iec_cnsmr_cs *cs_info,
			   enum audio_coding_types coding_type,
			   unsigned int channels,
			   unsigned int rate)
{
	cs_info->user           = false;
	cs_info->cp             = false;
	cs_info->emphasis       = 0x0;
	cs_info->mode           = 0x0;
	cs_info->source_num     = 0x0;
	cs_info->channel_num    = channels;
	cs_info->clock_accuracy = 0x0;

	/* defalut cs fs */
	cs_info->sampling_freq = iec_rate_to_csfs(rate);

	if (audio_coding_is_non_lpcm(coding_type)) {
		cs_info->fmt = 1;
		/* default : laser optical product, copy from projects */
		cs_info->category_code = 0x19;

		/* Update cs fs */
		if (coding_type == AUDIO_CODING_TYPE_EAC3 ||
		    coding_type == AUDIO_CODING_TYPE_DTS_HD ||
		    coding_type == AUDIO_CODING_TYPE_AC3_LAYOUT_B) {
			/* DD+ */
			if (rate == 32000) {
				// TODO: need to check, inherited from old codes
				cs_info->sampling_freq =
					IEC958_AES3_CON_FS_32000;
			} else if (rate == 44100)
				cs_info->sampling_freq =
					IEC958_AES3_CON_FS_176400;
			else
				cs_info->sampling_freq =
					IEC958_AES3_CON_FS_192000;

		} else if (coding_type == AUDIO_CODING_TYPE_MLP ||
			   coding_type == AUDIO_CODING_TYPE_DTS_HD_MA) {
			/* True HD, MA */
			cs_info->sampling_freq = IEC958_AES3_CON_FS_768000;
		}
	} else if ((coding_type == AUDIO_CODING_TYPE_MULTICH_32CH_LPCM) ||
		(coding_type == AUDIO_CODING_TYPE_MULTICH_16CH_LPCM)) {
		cs_info->fmt = 0;
		cs_info->sampling_freq = IEC958_AES3_CON_FS_NOTID;
	} else if (coding_type == AUDIO_CODING_TYPE_MULTICH_8CH_LPCM) {
		cs_info->fmt = 0;
		/* fs as 4 * clk */
		if (rate == 44100)
			cs_info->sampling_freq = IEC958_AES3_CON_FS_176400;
		else if (rate == 48000)
			cs_info->sampling_freq = IEC958_AES3_CON_FS_192000;
		else if (rate == 192000)
			cs_info->sampling_freq = IEC958_AES3_CON_FS_768000;
	} else {
		cs_info->fmt = 0;
		/* default : laser optical product*/
		cs_info->category_code = 0x01;
	}
}

enum audio_coding_types iec_61937_pc_to_coding_type(unsigned int pc)
{
	int data_type = pc & 0x1f;
	int subdata_type = (pc >> 5) & 0x3;
	int frames;
	enum audio_coding_types coding_type = AUDIO_CODING_TYPE_UNDEFINED;

	pr_debug("%s pc:%#x, data_type:%#x, subdata_type:%#x\n",
		 __func__, pc, data_type, subdata_type);

	switch (data_type) {
	case 1:
		coding_type = AUDIO_CODING_TYPE_AC3;
		break;
	case 21:
		coding_type = AUDIO_CODING_TYPE_EAC3;
		if (subdata_type == 0)
			frames = 6144;
		break;
	case 22:
		coding_type = AUDIO_CODING_TYPE_MLP;
		break;
	case 11:
	case 12:
	case 13:
		coding_type = AUDIO_CODING_TYPE_DTS;
		break;
	case 17:
		coding_type = AUDIO_CODING_TYPE_DTS_HD;
		break;
	case 3:
		coding_type = AUDIO_CODING_TYPE_PAUSE;
		pr_debug("get iec_61937 pause package, pc:%#x, data_type:%#x, subdata_type:%#x\n",
			pc, data_type, subdata_type);
		break;
	case 0:
		/* invalid data */
		coding_type = AUDIO_CODING_TYPE_PAUSE;
		pr_debug("get iec_61937 pause package, pc:%#x, data_type:%#x, subdata_type:%#x\n",
			pc, data_type, subdata_type);
		break;
	default:
		break;
	}

	return coding_type;
}

const struct soc_enum aud_codec_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(aud_codec_type_names),
			aud_codec_type_names);

bool codec_is_raw(enum aud_codec_types codec_type)
{
	return ((codec_type != AUD_CODEC_TYPE_STEREO_PCM) &&
		(codec_type != AUD_CODEC_TYPE_HSR_STEREO_PCM) &&
		(codec_type != AUD_CODEC_TYPE_MULTI_LPCM));
}

bool raw_is_4x_clk(enum aud_codec_types codec_type)
{
	bool is_4x = false;

	if (codec_type == AUD_CODEC_TYPE_EAC3 ||
	    codec_type == AUD_CODEC_TYPE_DTS_HD ||
	    codec_type == AUD_CODEC_TYPE_TRUEHD ||
	    codec_type == AUD_CODEC_TYPE_DTS_HD_MA ||
	    codec_type == AUD_CODEC_TYPE_AC3_LAYOUT_B) {
		is_4x = true;
	}

	return is_4x;
}

bool raw_is_hbr_audio(enum aud_codec_types codec_type)
{
	if (codec_type == AUD_CODEC_TYPE_TRUEHD ||
	    codec_type == AUD_CODEC_TYPE_DTS_HD_MA)
		return true;

	return false;
}

/**
 * TDM, SPDIF and EARC keep same mpll clk frequency(491.52M)
 * to prevent long time drift. This is to calc multiplier
 * to the desired clk frequency according to codec type.
 * As driver has done multiplier to sysclk by raw_is_4x_clk() before.
 * NOTE: HBR alsa config samplerate is 192000 while DD and DDP config 48000.
 */
unsigned int mpll2sys_clk_ratio_by_type(enum aud_codec_types codec_type)
{
	/* pcm format mpll clk ratio: 491520000/6144000/EARC_DMAC_MUTIPLIER */
	unsigned int ratio = 16;

	if (raw_is_4x_clk(codec_type)) {
		if (raw_is_hbr_audio(codec_type))
			ratio = 1;
		else
			ratio = 4;
	}

	return ratio * EARC_DMAC_MUTIPLIER;
}

unsigned int mpll2dmac_clk_ratio_by_type(enum audio_coding_types coding_type)
{
	/* pcm format mpll clk ratio: 491520000/(6144000*EARC_DMAC_MUTIPLIER)*/
	unsigned int ratio;

	switch (coding_type) {
	case AUDIO_CODING_TYPE_MULTICH_16CH_LPCM:
		ratio = 2;
		break;
	case AUDIO_CODING_TYPE_MULTICH_8CH_LPCM:
		ratio = 4;
		break;
	case AUDIO_CODING_TYPE_EAC3:
	case AUDIO_CODING_TYPE_DTS_HD:
	case AUDIO_CODING_TYPE_AC3_LAYOUT_B:
		ratio = 4;
		break;
	case AUDIO_CODING_TYPE_DTS_HD_MA:
	case AUDIO_CODING_TYPE_MLP:
	case AUDIO_CODING_TYPE_MULTICH_32CH_LPCM:
		ratio = 1;
		break;
	default:
		ratio = 16;
	}

	return ratio;
}

void iec_get_channel_status_info(struct iec958_chsts *chsts,
	enum aud_codec_types codec_type,
	unsigned int rate,
	unsigned int bit_depth,
	unsigned int l_bit)
{
	int rate_bit = snd_pcm_rate_to_rate_bit(rate);

	if (rate_bit == SNDRV_PCM_RATE_KNOT) {
		pr_err("Unsupport sample rate\n");
		return;
	}

	if (codec_is_raw(codec_type)) {
		chsts->chstat0_l = 0x6;
		chsts->chstat0_r = 0x6;
		chsts->chstat2_l = IEC958_AES4_CON_WORDLEN_20_16;
		chsts->chstat2_r = IEC958_AES4_CON_WORDLEN_20_16;
		if (codec_type == AUD_CODEC_TYPE_EAC3 ||
		    codec_type == AUD_CODEC_TYPE_DTS_HD) {
			/* DD+ */
			if (rate_bit == SNDRV_PCM_RATE_32000) {
				chsts->chstat1_l = IEC958_AES3_CON_FS_32000 << 8;
				chsts->chstat1_r = IEC958_AES3_CON_FS_32000 << 8;
				chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_32000;
				chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_32000;
			} else if (rate_bit == SNDRV_PCM_RATE_44100) {
				chsts->chstat1_l = IEC958_AES3_CON_FS_176400 << 8;
				chsts->chstat1_r = IEC958_AES3_CON_FS_176400 << 8;
				chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_44100;
				chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_44100;
			} else {
				chsts->chstat1_l = IEC958_AES3_CON_FS_192000 << 8;
				chsts->chstat1_r = IEC958_AES3_CON_FS_192000 << 8;
				chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_48000;
				chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_48000;
			}
		} else if (codec_type == AUD_CODEC_TYPE_TRUEHD ||
			   codec_type == AUD_CODEC_TYPE_DTS_HD_MA) {
			/* True HD, MA */
			chsts->chstat1_l = IEC958_AES3_CON_FS_768000 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_768000 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_192000;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_192000;
		} else {
			/* DTS,DD */
			if (rate_bit == SNDRV_PCM_RATE_32000) {
				chsts->chstat1_l = IEC958_AES3_CON_FS_32000 << 8;
				chsts->chstat1_r = IEC958_AES3_CON_FS_32000 << 8;
				chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_32000;
				chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_32000;
			} else if (rate_bit == SNDRV_PCM_RATE_44100) {
				chsts->chstat1_l = 0;
				chsts->chstat1_r = 0;
				chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_44100;
				chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_44100;
			} else {
				chsts->chstat1_l = IEC958_AES3_CON_FS_48000 << 8;
				chsts->chstat1_r = IEC958_AES3_CON_FS_48000 << 8;
				chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_48000;
				chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_48000;
			}
		}
	} else {
		chsts->chstat0_l = 0x0100;
		chsts->chstat0_r = 0x0100;

		if (bit_depth >= 24) {
			chsts->chstat2_l = IEC958_AES4_CON_MAX_WORDLEN_24 |
					IEC958_AES4_CON_WORDLEN_24_20;
			chsts->chstat2_r = IEC958_AES4_CON_MAX_WORDLEN_24 |
					IEC958_AES4_CON_WORDLEN_24_20;
		} else {
			chsts->chstat2_l = IEC958_AES4_CON_WORDLEN_20_16;
			chsts->chstat2_r = IEC958_AES4_CON_WORDLEN_20_16;
		}

		if (rate_bit == SNDRV_PCM_RATE_48000) {
			chsts->chstat1_l = IEC958_AES3_CON_FS_48000 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_48000 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_48000;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_48000;
		} else if (rate_bit == SNDRV_PCM_RATE_44100) {
			chsts->chstat1_l = IEC958_AES3_CON_FS_44100 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_44100 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_44100;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_44100;
		} else if (rate_bit == SNDRV_PCM_RATE_88200) {
			chsts->chstat1_l = IEC958_AES3_CON_FS_88200 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_88200 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_88200;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_88200;
		} else if (rate_bit == SNDRV_PCM_RATE_96000) {
			chsts->chstat1_l = IEC958_AES3_CON_FS_96000 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_96000 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_96000;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_96000;
		} else if (rate_bit == SNDRV_PCM_RATE_176400) {
			chsts->chstat1_l = IEC958_AES3_CON_FS_176400 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_176400 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_176400;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_176400;
		} else if (rate_bit == SNDRV_PCM_RATE_192000) {
			chsts->chstat1_l = IEC958_AES3_CON_FS_192000 << 8;
			chsts->chstat1_r = IEC958_AES3_CON_FS_192000 << 8;
			chsts->chstat2_l |= IEC958_AES4_CON_ORIGFS_192000;
			chsts->chstat2_r |= IEC958_AES4_CON_ORIGFS_192000;
		}
	}

	/* IEC958_AES2_CON_CHANNEL */
	chsts->chstat1_l |= 1 << 4;
	chsts->chstat1_r |= 2 << 4;

	if (l_bit) {
		chsts->chstat0_l |= 1 << 15;
		chsts->chstat0_r |= 1 << 15;
	}
	pr_debug("rate: %d, codec_type:0x%x, channel status L:0x%x, R:0x%x\n",
		 rate,
		 codec_type,
		 ((chsts->chstat1_l >> 8) & 0xf) << 24 | chsts->chstat0_l,
		 ((chsts->chstat1_r >> 8) & 0xf) << 24 | chsts->chstat0_r);
}

void spdif_notify_to_hdmitx(struct snd_pcm_substream *substream,
			    enum aud_codec_types codec_type)
{
	struct aud_para aud_param;

	memset(&aud_param, 0, sizeof(aud_param));

	aud_param.rate = substream->runtime->rate;
	aud_param.size = substream->runtime->sample_bits;
	aud_param.chs  = substream->runtime->channels;
	aud_param.aud_src_if = AUD_SRC_IF_SPDIF;

	if (codec_type == AUD_CODEC_TYPE_AC3) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_AC_3,
					 &aud_param);
	} else if (codec_type == AUD_CODEC_TYPE_DTS) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DTS,
					 &aud_param);
	} else if (codec_type == AUD_CODEC_TYPE_EAC3) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS,
					 &aud_param);
	} else if (codec_type == AUD_CODEC_TYPE_DTS_HD) {
		aud_param.fifo_rst = 1;
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DTS_HD,
					 &aud_param);
	} else if (codec_type == AUD_CODEC_TYPE_TRUEHD) {
		aud_param.fifo_rst = 1;
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_MAT_MLP,
					 &aud_param);
	} else if (codec_type == AUD_CODEC_TYPE_DTS_HD_MA) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DTS_HD_MA,
					 &aud_param);
	} else {
		aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM,
					 &aud_param);
	}
}

/* notify hdmitx to prepare for changing audio format or settings */
void notify_hdmitx_to_prepare(void)
{
	struct aud_para aud_param;

	memset(&aud_param, 0, sizeof(aud_param));
	aud_param.prepare = true;
	aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM, &aud_param);
}

#ifdef CONFIG_AMLOGIC_HDMITX
unsigned int aml_audio_hdmiout_mute_flag;
/* call HDMITX API to enable/disable internal audio out */
int aml_get_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = !hdmitx_ext_get_audio_status();

	aml_audio_hdmiout_mute_flag =
			ucontrol->value.integer.value[0];
	return 0;
}

int aml_set_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	bool mute = ucontrol->value.integer.value[0];

	if (aml_audio_hdmiout_mute_flag != mute) {
		hdmitx_ext_set_audio_output(!mute);
		aml_audio_hdmiout_mute_flag = mute;
	}
	return 0;
}
#endif
