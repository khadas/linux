/* SPDX-License-Identifier: GPL-2.0
 *
 * ASoC simple sound card support
 *
 * Copyright (C) 2012 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 */

#ifndef __SIMPLE_CARD_H
#define __SIMPLE_CARD_H

#include <sound/soc.h>
#include "card_utils.h"
#include "../common/iec_info.h"
#include "soft_locker.h"

enum hdmitx_src {
	HDMITX_SRC_SPDIF,
	HDMITX_SRC_SPDIF_B,
	HDMITX_SRC_TDM_A,
	HDMITX_SRC_TDM_B,
	HDMITX_SRC_TDM_C,
	HDMITX_SRC_NUM
};

struct aml_card_info {
	const char *name;
	const char *card;
	const char *codec;
	const char *platform;

	unsigned int daifmt;
	struct aml_dai cpu_dai;
	struct aml_dai codec_dai;
};

struct soft_locker *aml_get_card_locker(struct snd_soc_card *card);
int get_aml_audio_binv(struct snd_soc_card *card);
int get_aml_audio_binv_index(struct snd_soc_card *card);
int set_aml_audio_binv(struct snd_soc_card *card, int audio_binv);
int set_aml_audio_binv_index(struct snd_soc_card *card, int binv_tdm_index);
int get_aml_audio_inskew(struct snd_soc_card *card);
int get_aml_audio_inskew_index(struct snd_soc_card *card);
int set_aml_audio_inskew(struct snd_soc_card *card, int audio_inskew);
int set_aml_audio_inskew_index(struct snd_soc_card *card, int inskew_tdm_index);
enum hdmitx_src get_hdmitx_audio_src(struct snd_soc_card *card);
enum aud_codec_types get_i2s2hdmitx_audio_format(struct snd_soc_card *card);
int get_hdmitx_i2s_mask(struct snd_soc_card *card);

#endif /* __SIMPLE_CARD_H */
