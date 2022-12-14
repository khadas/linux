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

struct aml_card_info {
	const char *name;
	const char *card;
	const char *codec;
	const char *platform;

	unsigned int daifmt;
	struct aml_dai cpu_dai;
	struct aml_dai codec_dai;
};

int get_aml_audio_inskew(struct snd_soc_card *card);
int get_aml_audio_inskew_index(struct snd_soc_card *card);
int set_aml_audio_inskew(struct snd_soc_card *card, int audio_inskew);
int set_aml_audio_inskew_index(struct snd_soc_card *card, int inskew_tdm_index);

#endif /* __SIMPLE_CARD_H */
