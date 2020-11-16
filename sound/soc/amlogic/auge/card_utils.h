/* SPDX-License-Identifier: GPL-2.0
 *
 * simple_card_utils.h
 *
 * Copyright (c) 2016 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 */

#ifndef __SIMPLE_CARD_UTILS_H
#define __SIMPLE_CARD_UTILS_H

#include <sound/soc.h>
#include "audio_utils.h"

struct aml_dai {
	const char *name;
	unsigned int sysclk;
	int slots;
	int slot_width;
	unsigned int tx_slot_mask;
	unsigned int rx_slot_mask;
	struct clk *clk;
};

int aml_card_parse_daifmt(struct device *dev,
				struct device_node *node,
				struct device_node *codec,
				char *prefix,
				unsigned int *retfmt);
__printf(3, 4)
int aml_card_set_dailink_name(struct device *dev,
				struct snd_soc_dai_link *dai_link,
				const char *fmt, ...);
int aml_card_parse_card_name(struct snd_soc_card *card,
				char *prefix);

#define aml_card_parse_clk_cpu(node, dai_link, aml_dai)		\
	aml_card_parse_clk(node, (dai_link)->cpus->of_node, aml_dai)
#define aml_card_parse_clk_codec(node, dai_link, aml_dai)		\
	aml_card_parse_clk(node, (dai_link)->codecs->of_node, aml_dai)
int aml_card_parse_clk(struct device_node *node,
				struct device_node *dai_of_node,
				struct aml_dai *aml_dai);

#define aml_card_parse_cpu(node, dai_link,				\
			list_name, cells_name, is_single_link)		\
	({typeof(dai_link) dai_link_ = (dai_link);			\
		aml_card_parse_dai(node, &dai_link_->cpus->of_node,	\
		&dai_link_->cpus->dai_name, list_name, cells_name, is_single_link); })
#define aml_card_parse_codec(node, dai_link, list_name, cells_name)	\
	({typeof(dai_link) dai_link_ = (dai_link);			\
		aml_card_parse_dai(node, &dai_link_->codecs->of_node,		\
		&dai_link_->codecs->dai_name, list_name, cells_name, NULL); })
#define aml_card_parse_platform(node, dai_link, list_name, cells_name)	\
	aml_card_parse_dai(node, &(dai_link)->platforms->of_node,		\
		NULL, list_name, cells_name, NULL)
int aml_card_parse_dai(struct device_node *node,
				struct device_node **endpoint_np,
				const char **dai_name,
				const char *list_name,
				const char *cells_name,
				int *is_single_links);
int aml_card_parse_codec_confs(struct device_node *codec_np,
				struct snd_soc_card *card);

int aml_card_init_dai(struct snd_soc_dai *dai,
				struct aml_dai *aml_dai, bool cont_clk);

int aml_card_canonicalize_dailink(struct snd_soc_dai_link *dai_link);
void aml_card_canonicalize_cpu(struct snd_soc_dai_link *dai_link,
				int is_single_links);

int aml_card_clean_reference(struct snd_soc_card *card);

int aml_card_add_controls(struct snd_soc_card *card);

#endif /* __SIMPLE_CARD_UTILS_H */
