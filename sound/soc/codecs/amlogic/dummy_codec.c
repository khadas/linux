// SPDX-License-Identifier: GPL-2.0
/*
 * ALSA SoC dummy codec driver
 *
 * Copyright (C) 2019 Amlogic,inc
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/of.h>

#define DRV_NAME "dummy"

struct dummy_codec_private {
	struct snd_soc_component component;
};

#define DUMMY_CODEC_RATES      (SNDRV_PCM_RATE_8000_192000)
#define DUMMY_CODEC_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dapm_widget dummy_codec_dapm_widgets[] = {
	/* Output Side */
	/* DACs */
	SND_SOC_DAPM_DAC("Left DAC", "HIFI Playback", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HIFI Playback", SND_SOC_NOPM, 7, 0),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),

};

static const struct snd_soc_dapm_route dummy_codec_dapm_routes[] = {
	{"LOUTL", NULL, "Left DAC"},
	{"LOUTR", NULL, "Right DAC"},
};

struct snd_soc_dai_driver dummy_codec_dai = {
	.name     = DRV_NAME,
	.id       = 0,
	.playback = {
		.stream_name  = "HIFI Playback",
		.channels_min = 1,
		.channels_max = 32,
		.rates        = DUMMY_CODEC_RATES,
		.formats      = DUMMY_CODEC_FORMATS,
	},
	.capture  = {
		.stream_name  = "HIFI Capture",
		.channels_min = 1,
		.channels_max = 32,
		.rates        = DUMMY_CODEC_RATES,
		.formats      = DUMMY_CODEC_FORMATS,
	},
};

static const struct snd_soc_component_driver soc_codec_dev_dummy_codec = {
	.name             = DRV_NAME,

	.dapm_widgets     = dummy_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dummy_codec_dapm_widgets),
	.dapm_routes      = dummy_codec_dapm_routes,
	.num_dapm_routes  = ARRAY_SIZE(dummy_codec_dapm_routes),
};

static const struct of_device_id amlogic_codec_dt_match[] = {
	{.compatible = "amlogic, aml_dummy_codec",
	 },
	{},
};

static int dummy_codec_platform_probe(struct platform_device *pdev)
{
	struct dummy_codec_private *dummy_codec;
	int ret;

	dummy_codec = kzalloc(sizeof(*dummy_codec), GFP_KERNEL);
	if (!dummy_codec)
		return -ENOMEM;

	platform_set_drvdata(pdev, dummy_codec);
	ret = devm_snd_soc_register_component(&pdev->dev,
					      &soc_codec_dev_dummy_codec,
					      &dummy_codec_dai, 1);

	if (ret < 0)
		kfree(dummy_codec);

	return ret;
}

static int dummy_codec_platform_remove(struct platform_device *pdev)
{
	kfree(platform_get_drvdata(pdev));

	return 0;
}

static struct platform_driver dummy_codec_platform_driver = {
	.driver = {
		   .name           = DRV_NAME,
		   .owner          = THIS_MODULE,
		   .of_match_table = of_match_ptr(amlogic_codec_dt_match),
		   },
	.probe  = dummy_codec_platform_probe,
	.remove = dummy_codec_platform_remove,
};

module_platform_driver(dummy_codec_platform_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("ASoC dummy_codec driver");
MODULE_LICENSE("GPL");

