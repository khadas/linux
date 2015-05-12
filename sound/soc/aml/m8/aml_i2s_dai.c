/*
 * sound/soc/aml/m8/aml_i2s_dai.c
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

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "aml_i2s_dai.h"
#include "aml_pcm.h"
#include "aml_i2s.h"
#include "aml_audio_hw.h"
#include <linux/amlogic/sound/aout_notify.h>
#include "aml_spdif_dai.h"

struct aml_dai_info dai_info[3] = { {0} };

static int i2s_pos_sync;
/* #define AML_DAI_DEBUG */
#define ALSA_PRINT(fmt, args...)	pr_info("[aml-i2s-dai]" fmt, ##args)
#ifdef AML_DAI_DEBUG
#define ALSA_DEBUG(fmt, args...)	pr_info("[aml-i2s-dai]" fmt, ##args)
#define ALSA_TRACE()	pr_info("[aml-i2s-dai] enter func %s\n", __func__)
#else
#define ALSA_DEBUG(fmt, args...)
#define ALSA_TRACE()
#endif
/* extern int amaudio2_enable; */
/* extern int kernel_android_50; */

/* extern int set_i2s_iec958_samesource(int enable); */

static int i2sbuf[32 + 16];
static void aml_i2s_play(void)
{
	audio_util_set_dac_i2s_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);
	audio_set_i2s_mode(AIU_I2S_MODE_PCM16);
	memset(i2sbuf, 0, sizeof(i2sbuf));
	audio_set_aiubuf((virt_to_phys(i2sbuf) + 63) & (~63), 128, 2);
	audio_out_i2s_enable(1);

}

/*
the I2S hw  and IEC958 PCM output initation,958 initation here,
for the case that only use our ALSA driver for PCM s/pdif output.
*/
static void aml_hw_i2s_init(struct snd_pcm_runtime *runtime)
{
	unsigned i2s_mode = AIU_I2S_MODE_PCM16;
	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		i2s_mode = AIU_I2S_MODE_PCM32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		i2s_mode = AIU_I2S_MODE_PCM24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		i2s_mode = AIU_I2S_MODE_PCM16;
		break;
	}
	audio_set_i2s_mode(i2s_mode);
	audio_set_aiubuf(runtime->dma_addr, runtime->dma_bytes,
			 runtime->channels);
	ALSA_PRINT("i2s dma %p,phy addr %ld,mode %d,ch %d\n",
		   runtime->dma_area, (long)runtime->dma_addr,
		   i2s_mode, runtime->channels);
}

static int aml_dai_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd =
	    (struct aml_runtime_data *)runtime->private_data;
	struct audio_stream *s;
	ALSA_TRACE();
	if (prtd == NULL) {
		prtd =
		    (struct aml_runtime_data *)
		    kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			pr_info("alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}
	s = &prtd->s;
	if (substream->stream
			== SNDRV_PCM_STREAM_PLAYBACK) {
		s->device_type = AML_AUDIO_I2SOUT;
	} else {
		s->device_type = AML_AUDIO_I2SIN;
	}
	return 0;
 out:
	return ret;
}

static void aml_dai_i2s_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	return;
}

#define AOUT_EVENT_IEC_60958_PCM 0x1

static int aml_dai_i2s_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct aml_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	int audio_clk_config = AUDIO_CLK_FREQ_48;
	struct audio_stream *s = &prtd->s;
	ALSA_TRACE();
	switch (runtime->rate) {
	case 192000:
		audio_clk_config = AUDIO_CLK_FREQ_192;
		break;
	case 176400:
		audio_clk_config = AUDIO_CLK_FREQ_1764;
		break;
	case 96000:
		audio_clk_config = AUDIO_CLK_FREQ_96;
		break;
	case 88200:
		audio_clk_config = AUDIO_CLK_FREQ_882;
		break;
	case 48000:
		audio_clk_config = AUDIO_CLK_FREQ_48;
		break;
	case 44100:
		audio_clk_config = AUDIO_CLK_FREQ_441;
		break;
	case 32000:
		audio_clk_config = AUDIO_CLK_FREQ_32;
		break;
	case 8000:
		audio_clk_config = AUDIO_CLK_FREQ_8;
		break;
	case 11025:
		audio_clk_config = AUDIO_CLK_FREQ_11;
		break;
	case 16000:
		audio_clk_config = AUDIO_CLK_FREQ_16;
		break;
	case 22050:
		audio_clk_config = AUDIO_CLK_FREQ_22;
		break;
	case 12000:
		audio_clk_config = AUDIO_CLK_FREQ_12;
		break;
	case 24000:
		audio_clk_config = AUDIO_CLK_FREQ_22;
		break;
	default:
		audio_clk_config = AUDIO_CLK_FREQ_441;
		break;
	};
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		audio_out_i2s_enable(0);
	if (i2s->old_samplerate != runtime->rate) {
		ALSA_PRINT("enterd %s,old_samplerate:%d,sample_rate=%d\n",
			   __func__, i2s->old_samplerate, runtime->rate);
		i2s->old_samplerate = runtime->rate;
		audio_set_i2s_clk(audio_clk_config,
			AUDIO_CLK_256FS, i2s->mpll);
	}
	audio_util_set_dac_i2s_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		s->i2s_mode = dai_info[dai->id].i2s_mode;
		audio_in_i2s_set_buf(runtime->dma_addr, runtime->dma_bytes * 2,
				     0, i2s_pos_sync);
		memset((void *)runtime->dma_area, 0, runtime->dma_bytes * 2);
		{
			int *ppp =
			    (int *)(runtime->dma_area + runtime->dma_bytes * 2 -
				    8);
			ppp[0] = 0x78787878;
			ppp[1] = 0x78787878;
		}
		s->device_type = AML_AUDIO_I2SIN;
	} else {
		s->device_type = AML_AUDIO_I2SOUT;
		aml_hw_i2s_init(runtime);
		/* i2s/958 share the same audio hw buffer when PCM mode */
		if (IEC958_mode_codec == 0) {
			aml_hw_iec958_init(substream);
			/* use the hw same sync for i2s/958 */
			audio_i2s_958_same_source(1);
		}
	}
	if (runtime->channels == 8) {
		pr_info("[%s,%d]8ch PCM output->notify HDMI\n", __func__,
		       __LINE__);
		aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM, substream);
	}
	return 0;
}

static int aml_dai_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	int *ppp = NULL;
	ALSA_TRACE();
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* TODO */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pr_info("aiu i2s playback enable\n");
			audio_out_i2s_enable(1);
			if (IEC958_mode_codec == 0) {
				pr_info("audio_hw_958_enable 1\n");
				audio_hw_958_enable(1);
			}
		} else {
			audio_in_i2s_enable(1);
			ppp = (int *)(rtd->dma_area + rtd->dma_bytes * 2 - 8);
			ppp[0] = 0x78787878;
			ppp[1] = 0x78787878;
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pr_info("aiu i2s playback disable\n");
			audio_out_i2s_enable(0);
			if (IEC958_mode_codec == 0) {
				pr_info("audio_hw_958_enable 0\n");
				audio_hw_958_enable(0);
			}
		} else {
			audio_in_i2s_enable(0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_dai_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	ALSA_TRACE();
	return 0;
}

static int aml_dai_set_i2s_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	ALSA_TRACE();
	if (fmt & SND_SOC_DAIFMT_CBS_CFS)	/* slave mode */
		dai_info[dai->id].i2s_mode = I2S_SLAVE_MODE;

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		i2s_pos_sync = 0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		i2s_pos_sync = 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int aml_dai_set_i2s_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	ALSA_TRACE();
	return 0;
}

#ifdef CONFIG_PM
static int aml_dai_i2s_suspend(struct snd_soc_dai *dai)
{
	ALSA_TRACE();
	return 0;
}

static int aml_dai_i2s_resume(struct snd_soc_dai *dai)
{
	ALSA_TRACE();
	return 0;
}

#else				/* CONFIG_PM */
#define aml_dai_i2s_suspend	NULL
#define aml_dai_i2s_resume	NULL
#endif				/* CONFIG_PM */

#define AML_DAI_I2S_RATES		(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_I2S_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops aml_dai_i2s_ops = {
	.startup = aml_dai_i2s_startup,
	.shutdown = aml_dai_i2s_shutdown,
	.prepare = aml_dai_i2s_prepare,
	.trigger = aml_dai_i2s_trigger,
	.hw_params = aml_dai_i2s_hw_params,
	.set_fmt = aml_dai_set_i2s_fmt,
	.set_sysclk = aml_dai_set_i2s_sysclk,
};

struct snd_soc_dai_driver aml_i2s_dai[] = {
	{
	 .id = 0,
	 .suspend = aml_dai_i2s_suspend,
	 .resume = aml_dai_i2s_resume,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = AML_DAI_I2S_RATES,
		      .formats = AML_DAI_I2S_FORMATS,},
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = AML_DAI_I2S_RATES,
		     .formats = AML_DAI_I2S_FORMATS,},
	 .ops = &aml_dai_i2s_ops,
	 },
};
EXPORT_SYMBOL_GPL(aml_i2s_dai);

static const struct snd_soc_component_driver aml_component = {
	.name = "aml-i2s-dai",
};

static const char *const gate_names[] = {
	"top_glue", "aud_buf", "i2s_out", "amclk_measure",
	"aififo2", "aud_mixer", "mixer_reg", "adc",
	"top_level", "aoclk", "aud_in"
};

static int aml_i2s_dai_probe(struct platform_device *pdev)
{
	struct aml_i2s *i2s = NULL;
	struct reset_control *audio_reset;
	int ret = 0, i;

	i2s = kzalloc(sizeof(struct aml_i2s), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "Can't allocate aml_i2s\n");
		ret = -ENOMEM;
		goto exit;
	}
	dev_set_drvdata(&pdev->dev, i2s);
	if (of_property_read_u32(pdev->dev.of_node,
			"clk_src_mpll", &i2s->mpll)) {
		pr_info("i2s get no clk src setting, use the default mpll 0\n");
		i2s->mpll = 0;
	}
	/* enable i2s power gate first */
	for (i = 0; i < ARRAY_SIZE(gate_names); i++) {
		audio_reset = devm_reset_control_get(&pdev->dev, gate_names[i]);
		if (IS_ERR(audio_reset)) {
			dev_err(&pdev->dev, "Can't get aml audio gate\n");
			return PTR_ERR(audio_reset);
		}
		reset_control_deassert(audio_reset);
	}

	/* enable the mclk because m8 codec need it to setup */
	if (i2s->old_samplerate != 48000) {
		ALSA_PRINT("enterd %s,old_samplerate:%d,sample_rate=%d\n",
			   __func__, i2s->old_samplerate, 48000);
		i2s->old_samplerate = 48000;
		audio_set_i2s_clk(AUDIO_CLK_FREQ_48,
			AUDIO_CLK_256FS, i2s->mpll);
	}
	aml_i2s_play();
	return snd_soc_register_component(&pdev->dev, &aml_component,
					  aml_i2s_dai, ARRAY_SIZE(aml_i2s_dai));

 exit:
	return ret;
}

static int aml_i2s_dai_remove(struct platform_device *pdev)
{
	struct aml_i2s *i2s = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	kfree(i2s);
	i2s = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_dai_dt_match[] = {
	{.compatible = "amlogic, aml-i2s-dai",},
	{},
};
#else
#define amlogic_dai_dt_match NULL
#endif

static struct platform_driver aml_i2s_dai_driver = {
	.driver = {
		   .name = "aml-i2s-dai",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_dai_dt_match,
		   },

	.probe = aml_i2s_dai_probe,
	.remove = aml_i2s_dai_remove,
};

static int __init aml_i2s_dai_modinit(void)
{
	return platform_driver_register(&aml_i2s_dai_driver);
}

module_init(aml_i2s_dai_modinit);

static void __exit aml_i2s_dai_modexit(void)
{
	platform_driver_unregister(&aml_i2s_dai_driver);
}

module_exit(aml_i2s_dai_modexit);

/* Module information */
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML DAI driver for ALSA");
MODULE_LICENSE("GPL");
