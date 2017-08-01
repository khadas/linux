/*
 * sound/soc/aml/m8/aml_i2s2_dai.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/soundcard.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <linux/major.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/sound/aout_notify.h>

#include "aml_i2s2_dai.h"
#include "aml_i2s.h"
#include "aml_audio_hw.h"

static int aml_dai_i2s2_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd =
		(struct aml_runtime_data *)runtime->private_data;
	struct audio_stream *s;

	if (prtd == NULL) {
		prtd =
			(struct aml_runtime_data *)
			kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			dev_err(substream->pcm->card->dev,
						"alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}
	s = &prtd->s;
	if (substream->stream
			== SNDRV_PCM_STREAM_PLAYBACK) {
		/*dev_info(substream->pcm->card->dev,
				"aml_dai_i2s2_startup:i2s2 playback\n");*/
		if (is_meson_txlx_cpu())
			s->device_type = AML_AUDIO_I2SIN;
	} else {
		dev_info(substream->pcm->card->dev,
						"aml_dai_i2s2_startup:i2s2 capture\n");
	}
	return 0;
 out:
	return ret;
}

static void aml_dai_i2s2_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	return;
}

static int aml_dai_i2s2_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/*dev_info(substream->pcm->card->dev,
					"aml_dai_i2s2_prepare[capture]\n");*/
		if (is_meson_txlx_cpu()) {
			if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
				audio_in_i2s_set_buf(runtime->dma_addr,
						runtime->dma_bytes * 2,
						0, 1, 1,
						runtime->channels);
				memset((void *)runtime->dma_area, 0,
						runtime->dma_bytes * 2);
			} else {
				audio_in_i2s_set_buf(runtime->dma_addr,
						runtime->dma_bytes,
						0, 1, 1,
						runtime->channels);
				memset((void *)runtime->dma_area, 0,
						runtime->dma_bytes);
			}
			s->device_type = AML_AUDIO_I2SIN;
		}
	} else {
		dev_info(substream->pcm->card->dev,
					"aml_dai_i2s2_prepare[playback]\n");
	}
	return 0;
}

static int aml_dai_i2s2_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* TODO */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev,
					"aml_dai_i2s2_trigger:i2s2 playback enable\n");
		} else {
			/*dev_info(substream->pcm->card->dev,
				"aml_dai_i2s2_trigger:i2s2 capture enable\n");*/
			if (is_meson_txlx_cpu())
				audio_in_i2s_enable(1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev,
					"aml_dai_i2s2_trigger:i2s2 playback disable\n");
		} else {
			/*dev_info(substream->pcm->card->dev,
			"aml_dai_i2s2_trigger:i2s2 capture disable\n");*/
			if (is_meson_txlx_cpu())
				audio_in_i2s_enable(0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_dai_i2s2_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	return 0;
}

static int aml_dai_set_i2s2_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	/*pr_info("%s: cpu_dai = %s, fmt = %u\n", __func__, dai->name, fmt);*/
	return 0;
}

static int aml_dai_set_i2s2_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_dai_i2s2_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int aml_dai_i2s2_resume(struct snd_soc_dai *dai)
{
	return 0;
}

#define AML_DAI_i2s2_RATES		(SNDRV_PCM_RATE_48000\
		| SNDRV_PCM_RATE_44100)
#define AML_DAI_i2s2_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE\
		| SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops aml_dai_i2s2_ops = {
	.startup = aml_dai_i2s2_startup,
	.shutdown = aml_dai_i2s2_shutdown,
	.prepare = aml_dai_i2s2_prepare,
	.trigger = aml_dai_i2s2_trigger,
	.hw_params = aml_dai_i2s2_hw_params,
	.set_fmt = aml_dai_set_i2s2_fmt,
	.set_sysclk = aml_dai_set_i2s2_sysclk,
};

struct snd_soc_dai_driver aml_i2s2_dai[] = {
	{
	 .id = 0,
	 .suspend = aml_dai_i2s2_suspend,
	 .resume = aml_dai_i2s2_resume,
	 .playback = {
			  .channels_min = 1,
			  .channels_max = 8,
			  .rates = AML_DAI_i2s2_RATES,
			  .formats = AML_DAI_i2s2_FORMATS,},
	 .capture = {
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = AML_DAI_i2s2_RATES,
			 .formats = AML_DAI_i2s2_FORMATS,},
	 .ops = &aml_dai_i2s2_ops,
	 },
};
EXPORT_SYMBOL_GPL(aml_i2s2_dai);

static const struct snd_soc_component_driver aml_component = {
	.name = "aml-i2s2-dai",
};

static int aml_i2s2_dai_probe(struct platform_device *pdev)
{
	struct aml_i2s2 *i2s2 = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "aml i2s2 dai probe!\n");
	i2s2 = devm_kzalloc(&pdev->dev, sizeof(struct aml_i2s2), GFP_KERNEL);
	if (!i2s2) {
		dev_err(&pdev->dev, "Can't allocate aml_i2s2!\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, i2s2);

	ret = snd_soc_register_component(&pdev->dev, &aml_component,
				aml_i2s2_dai, ARRAY_SIZE(aml_i2s2_dai));
	if (ret) {
		dev_err(&pdev->dev, "Can't register i2s2 dai: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

static int aml_i2s2_dai_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "aml i2s2 dai remove!\n");

	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static void aml_i2s2_dai_shutdown(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "aml i2s2 dai shutdown!\n");

	return;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_dai_dt_match[] = {
	{.compatible = "amlogic, aml-i2s2-dai",},
	{},
};
#else
#define amlogic_dai_dt_match NULL
#endif

static struct platform_driver aml_i2s2_dai_driver = {
	.driver = {
		   .name = "aml-i2s2-dai",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_dai_dt_match,
		   },

	.probe = aml_i2s2_dai_probe,
	.remove = aml_i2s2_dai_remove,
	.shutdown = aml_i2s2_dai_shutdown,
};

static int __init aml_i2s2_dai_modinit(void)
{
	return platform_driver_register(&aml_i2s2_dai_driver);
}

module_init(aml_i2s2_dai_modinit);

static void __exit aml_i2s2_dai_modexit(void)
{
	platform_driver_unregister(&aml_i2s2_dai_driver);
}

module_exit(aml_i2s2_dai_modexit);

/* Module information */
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML DAI driver for ALSA");
MODULE_LICENSE("GPL");
