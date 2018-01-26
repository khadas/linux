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
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include "aml_i2s_dai.h"
#include "aml_pcm.h"
#include "aml_i2s.h"
#include "aml_audio_hw.h"
#include <linux/amlogic/sound/aout_notify.h>
#include "aml_spdif_dai.h"

struct aml_dai_info dai_info[3] = { {0} };

static int i2s_pos_sync;

struct channel_speaker_allocation {
        int channels;
        int speakers[8];
};

#define NA	SNDRV_CHMAP_NA
#define FL	SNDRV_CHMAP_FL
#define FR	SNDRV_CHMAP_FR
#define RL	SNDRV_CHMAP_RL
#define RR	SNDRV_CHMAP_RR
#define LFE	SNDRV_CHMAP_LFE
#define FC	SNDRV_CHMAP_FC
#define RLC	SNDRV_CHMAP_RLC
#define RRC	SNDRV_CHMAP_RRC
#define RC	SNDRV_CHMAP_RC
#define FLC	SNDRV_CHMAP_FLC
#define FRC	SNDRV_CHMAP_FRC
#define FLH	SNDRV_CHMAP_TFL
#define FRH	SNDRV_CHMAP_TFR
#define FLW	SNDRV_CHMAP_FLW
#define FRW	SNDRV_CHMAP_FRW
#define TC	SNDRV_CHMAP_TC
#define FCH	SNDRV_CHMAP_TFC

static struct channel_speaker_allocation channel_allocations[] = {
/*      	       channel:   7     6    5    4    3     2    1    0  */
{ .channels = 2,  .speakers = {  NA,   NA,  NA,  NA,  NA,   NA,  FR,  FL } },
                                 /* 2.1 */
{ .channels = 3,  .speakers = {  NA,   NA,  NA,  NA,  NA,  LFE,  FR,  FL } },
                                 /* surround40 */
{ .channels = 4,  .speakers = {  NA,   NA,  RR,  RL,  NA,   NA,  FR,  FL } },
                                 /* surround41 */
{ .channels = 5,  .speakers = {  NA,   NA,  RR,  RL,  NA,  LFE,  FR,  FL } },
                                 /* surround50 */
{ .channels = 5,  .speakers = {  NA,   NA,  RR,  RL,  FC,   NA,  FR,  FL } },
                                 /* surround51 */
{ .channels = 6,  .speakers = {  NA,   NA,  RR,  RL,  FC,  LFE,  FR,  FL } },
                                 /* 6.1 */
{ .channels = 7,  .speakers = {  NA,   RC,  RR,  RL,  FC,  LFE,  FR,  FL } },
                                 /* surround71 */
{ .channels = 8,  .speakers = { RRC,  RLC,  RR,  RL,  FC,  LFE,  FR,  FL } },
};


/* extern int set_i2s_iec958_samesource(int enable); */
#define DEFAULT_SAMPLERATE 48000
#define DEFAULT_MCLK_RATIO_SR 256

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
#ifdef CONFIG_SND_AML_SPLIT_MODE
	audio_set_i2s_mode(i2s_mode, runtime->channels);
#else
	audio_set_i2s_mode(i2s_mode);
#endif
	audio_set_aiubuf(runtime->dma_addr, runtime->dma_bytes,
			 runtime->channels);
}

static int aml_dai_i2s_chmap_ctl_tlv(struct snd_kcontrol *kcontrol, int op_flag,
                                     unsigned int size, unsigned int __user *tlv)
{
    unsigned int __user *dst;
    int count = 0;
    int i;

    if (size < 8)
        return -ENOMEM;

    if (put_user(SNDRV_CTL_TLVT_CONTAINER, tlv))
        return -EFAULT;

    size -= 8;
    dst = tlv + 2;

    for (i = 0; i < ARRAY_SIZE(channel_allocations); i++)
    {
        struct channel_speaker_allocation *ch = &channel_allocations[i];
        int num_chs = 0;
        int chs_bytes;
        int c;

        for (c = 0; c < 8; c++)
        {
            if (ch->speakers[c])
                num_chs++;
        }

        chs_bytes = num_chs * 4;
        if (size < 8)
            return -ENOMEM;

        if (put_user(SNDRV_CTL_TLVT_CHMAP_FIXED, dst) ||
            put_user(chs_bytes, dst + 1))
            return -EFAULT;

        dst += 2;
        size -= 8;
        count += 8;

        if (size < chs_bytes)
            return -ENOMEM;

        size -= chs_bytes;
        count += chs_bytes;

        for (c = 0; c < 8; c++)
        {
            int sp = ch->speakers[7 - c];
            if (sp)
            {
                if (put_user(sp, dst))
                    return -EFAULT;
                dst++;
            }
        }
    }

    if (put_user(count, tlv + 1))
        return -EFAULT;

    return 0;
}

static int aml_dai_i2s_chmap_ctl_get(struct snd_kcontrol *kcontrol,
                                     struct snd_ctl_elem_value *ucontrol)
{

    struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
    unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
    struct snd_pcm_substream *substream = snd_pcm_chmap_substream(info, idx);
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct aml_runtime_data *prtd = (struct aml_runtime_data *)runtime->private_data;
    int res = 0, channel;

    if (mutex_lock_interruptible(&prtd->chmap_lock))
        return -EINTR;

    // we need 8 channels
    if (runtime->channels != 8)
    {
        pr_err("channel count should be 8, we got %d aborting\n", runtime->channels);
        res = -EINVAL;
        goto unlock;
    }

    for (channel=0; channel<8; channel++)
    {
        ucontrol->value.integer.value[7 - channel] = channel_allocations[prtd->chmap_layout].speakers[channel];
    }

unlock:
    mutex_unlock(&prtd->chmap_lock);
    return res;
}

static int aml_dai_i2s_chmap_ctl_put(struct snd_kcontrol *kcontrol,
                                     struct snd_ctl_elem_value *ucontrol)
{

    struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
    unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
    struct snd_pcm_substream *substream = snd_pcm_chmap_substream(info, idx);
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct aml_runtime_data *prtd = (struct aml_runtime_data *)runtime->private_data;
    int res = 0, channel, layout, matches, matched_layout;

    if (mutex_lock_interruptible(&prtd->chmap_lock))
        return -EINTR;

    // we need 8 channels
    if (runtime->channels != 8)
    {
        pr_err("channel count should be 8, we got %d aborting\n", runtime->channels);
        res = -EINVAL;
        goto unlock;
    }

    // now check if the channel setup matches one of our layouts
    for (layout = 0; layout < ARRAY_SIZE(channel_allocations); layout++)
    {
        matches = 1;

        for (channel = 0; channel < substream->runtime->channels; channel++)
        {
            int sp = ucontrol->value.integer.value[channel];
            int chan = channel_allocations[layout].speakers[7 - channel];

            if (sp != chan)
            {
                matches = 0;
                break;
            }
        }

        if (matches)
        {
            matched_layout = layout;
            break;
        }
    }


    // default to first layout if we didnt find any
    if (!matches)
        matched_layout = 0;

    pr_info("Setting a %d channel layout matching layout #%d\n", runtime->channels, matched_layout);

    prtd->chmap_layout = matched_layout;

unlock:
    mutex_unlock(&prtd->chmap_lock);
    return res;
}

static struct snd_kcontrol *aml_dai_i2s_chmap_kctrl_get(struct snd_pcm_substream *substream)
{
    int str;

    if ((substream) && (substream->pcm))
    {
        for (str=0; str<2; str++)
        {
            if (substream->pcm->streams[str].chmap_kctl)
            {
                return substream->pcm->streams[str].chmap_kctl;
            }
        }
    }

    return 0;
}

static int aml_dai_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
        int ret = 0, i;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd =
	    (struct aml_runtime_data *)runtime->private_data;
	struct audio_stream *s;
	struct snd_pcm_chmap *chmap;
	struct snd_kcontrol *kctl;

	if (prtd == NULL) {
		prtd =
		    (struct aml_runtime_data *)
		    kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			dev_err(substream->pcm->card->dev, "alloc aml_runtime_data error\n");
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


	// Alsa Channel Mapping API handling
	if (!aml_dai_i2s_chmap_kctrl_get(substream))
	{
	    ret = snd_pcm_add_chmap_ctls(substream->pcm, SNDRV_PCM_STREAM_PLAYBACK, NULL, 8, 0, &chmap);

	    if (ret < 0)
	    {
	      pr_err("aml_dai_i2s_startup error %d\n", ret);
	      goto out;
	    }

	    kctl = chmap->kctl;
	    for (i = 0; i < kctl->count; i++)
	      kctl->vd[i].access |= SNDRV_CTL_ELEM_ACCESS_WRITE;

	    kctl->get = aml_dai_i2s_chmap_ctl_get;
	    kctl->put = aml_dai_i2s_chmap_ctl_put;
	    kctl->tlv.c = aml_dai_i2s_chmap_ctl_tlv;
	}

	mutex_init(&prtd->chmap_lock);
	return 0;
 out:
	return ret;
}

static void aml_dai_i2s_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	if (IEC958_mode_codec == 0)
		aml_spdif_play(1);
	return;
}

#define AOUT_EVENT_IEC_60958_PCM 0x1
static int aml_i2s_set_amclk(struct aml_i2s *i2s, unsigned long rate)
{
	int ret = 0;

	ret = clk_set_rate(i2s->clk_mpll, rate * 10);
	if (ret)
		return ret;

	ret = clk_set_parent(i2s->clk_mclk, i2s->clk_mpll);
	if (ret)
		return ret;

	ret = clk_set_rate(i2s->clk_mclk, rate);
	if (ret)
		return ret;

	audio_set_i2s_clk_div();

	return 0;
}

static int aml_dai_i2s_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
	struct aml_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		s->i2s_mode = dai_info[dai->id].i2s_mode;
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
			audio_in_i2s_set_buf(runtime->dma_addr,
					runtime->dma_bytes * 2,
					0, i2s_pos_sync, i2s->audin_fifo_src);
			memset((void *)runtime->dma_area, 0,
					runtime->dma_bytes * 2);
		} else {
			audio_in_i2s_set_buf(runtime->dma_addr,
					runtime->dma_bytes,
					0, i2s_pos_sync, i2s->audin_fifo_src);
			memset((void *)runtime->dma_area, 0,
					runtime->dma_bytes);
		}
		s->device_type = AML_AUDIO_I2SIN;
	} else {
		s->device_type = AML_AUDIO_I2SOUT;
		audio_out_i2s_enable(0);
		audio_util_set_dac_i2s_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);
		IEC958_mode_codec = 0;
		aml_hw_i2s_init(runtime);
		/* i2s/958 share the same audio hw buffer when PCM mode */
		if (IEC958_mode_codec == 0) {
			aml_hw_iec958_init(substream, 1);
			/* use the hw same sync for i2s/958 */
			dev_info(substream->pcm->card->dev, "i2s/958 same source\n");
			audio_i2s_958_same_source(1);
		}
		if (runtime->channels == 8) {
			dev_info(substream->pcm->card->dev,
				"8ch PCM output->notify HDMI\n");
			aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM,
				substream);
		}
	}
	return 0;
}

static int aml_dai_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* TODO */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "I2S playback enable\n");
			audio_out_i2s_enable(1);
			if (IEC958_mode_codec == 0) {
				dev_info(substream->pcm->card->dev, "IEC958 playback enable\n");
				audio_hw_958_enable(1);
			}
		} else {
			audio_in_i2s_enable(1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "I2S playback disable\n");
			audio_out_i2s_enable(0);
			if (IEC958_mode_codec == 0) {
				dev_info(substream->pcm->card->dev, "IEC958 playback disable\n");
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
	struct aml_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	int srate, mclk_rate;

	srate = params_rate(params);
	if (i2s->old_samplerate != srate) {
		i2s->old_samplerate = srate;
		mclk_rate = srate * DEFAULT_MCLK_RATIO_SR;
		aml_i2s_set_amclk(i2s, mclk_rate);
	}

	return 0;
}

static int aml_dai_set_i2s_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
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
	return 0;
}

static int aml_dai_i2s_suspend(struct snd_soc_dai *dai)
{
	struct aml_i2s *i2s = dev_get_drvdata(dai->dev);

	if (i2s && i2s->clk_mclk && !i2s->disable_clk_suspend)
		clk_disable_unprepare(i2s->clk_mclk);

	return 0;
}

static int aml_dai_i2s_resume(struct snd_soc_dai *dai)
{
	struct aml_i2s *i2s = dev_get_drvdata(dai->dev);

	if (i2s && i2s->clk_mclk && !i2s->disable_clk_suspend)
		clk_prepare_enable(i2s->clk_mclk);

	return 0;
}

#define AML_DAI_I2S_RATES		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000
#define AML_DAI_I2S_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE

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
	struct device_node *pnode = pdev->dev.of_node;
	int ret = 0, i;

	/* enable AIU module power gate first */
	for (i = 0; i < ARRAY_SIZE(gate_names); i++) {
		audio_reset = devm_reset_control_get(&pdev->dev, gate_names[i]);
		if (IS_ERR(audio_reset)) {
			dev_err(&pdev->dev, "Can't get aml audio gate\n");
			return PTR_ERR(audio_reset);
		}
		reset_control_deassert(audio_reset);
	}

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct aml_i2s), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "Can't allocate aml_i2s\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, i2s);

	i2s->disable_clk_suspend =
		of_property_read_bool(pnode, "disable_clk_suspend");

	i2s->clk_mpll = devm_clk_get(&pdev->dev, "mpll2");
	if (IS_ERR(i2s->clk_mpll)) {
		dev_err(&pdev->dev, "Can't retrieve mpll2 clock\n");
		ret = PTR_ERR(i2s->clk_mpll);
		goto err;
	}

	i2s->clk_mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(i2s->clk_mclk)) {
		dev_err(&pdev->dev, "Can't retrieve clk_mclk clock\n");
		ret = PTR_ERR(i2s->clk_mclk);
		goto err;
	}

	/* now only 256fs is supported */
	ret = aml_i2s_set_amclk(i2s,
			DEFAULT_SAMPLERATE * DEFAULT_MCLK_RATIO_SR);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't set aml_i2s :%d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(i2s->clk_mclk);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable I2S mclk clock: %d\n", ret);
		goto err;
	}

	if (of_property_read_bool(pdev->dev.of_node, "DMIC")) {
		i2s->audin_fifo_src = 3;
		dev_info(&pdev->dev, "DMIC is in platform!\n");
	} else {
		i2s->audin_fifo_src = 1;
		dev_info(&pdev->dev, "I2S Mic is in platform!\n");
	}

	ret = snd_soc_register_component(&pdev->dev, &aml_component,
					  aml_i2s_dai, ARRAY_SIZE(aml_i2s_dai));
	if (ret) {
		dev_err(&pdev->dev, "Can't register i2s dai: %d\n", ret);
		goto err_clk_dis;
	}
	return 0;

err_clk_dis:
	clk_disable_unprepare(i2s->clk_mclk);
err:
	return ret;
}

static int aml_i2s_dai_remove(struct platform_device *pdev)
{
	struct aml_i2s *i2s = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	clk_disable_unprepare(i2s->clk_mclk);

	return 0;
}

static void aml_i2s_dai_shutdown(struct platform_device *pdev)
{
	struct aml_i2s *i2s = dev_get_drvdata(&pdev->dev);

	if (i2s && i2s->clk_mclk)
		clk_disable_unprepare(i2s->clk_mclk);

	return;
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
	.shutdown = aml_i2s_dai_shutdown,
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
