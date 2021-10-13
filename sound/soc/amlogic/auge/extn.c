// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

/*#define DEBUG*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/asoundef.h>

#include "ddr_mngr.h"
#include "audio_utils.h"
#include "frhdmirx_hw.h"
#include "resample.h"
#include "earc.h"

#include "../common/misc.h"
#include "../common/debug.h"

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif

#define DRV_NAME "EXTN"
#define MAX_INT    0x7ffffff

#define MAX_AUDIO_EDID_LENGTH 30

struct extn_chipinfo {
	int arc_version;
	bool PAO_channel_sync;
	int frhdmirx_version;
};

struct extn {
	struct aml_audio_controller *actrl;
	struct device *dev;

	struct toddr *tddr;
	struct frddr *fddr;

	int sysclk_freq;

	int irq_frhdmirx;
	/* protection of request/free not in pair */
	bool irq_on;

	/*
	 * 0: select spdif lane;
	 * 1: select PAO mode;
	 */
	int hdmirx_mode;

	/*
	 * arc source sel:
	 * 0: hi_hdmirx_arc_cntl[5]
	 * 1: audio_spdif_a
	 * 2: audio_spdif_b
	 * 4: hdmir_aud_spdif
	 */
	int arc_src;
	int arc_en;

	/* check whether irq generating
	 * if not, reset
	 * 'cuase no irq from nonpcm2pcm, do it by sw.
	 */
	unsigned int frhdmirx_cnt;      /* irq counter */
	unsigned int frhdmirx_last_cnt;
	unsigned int frhdmirx_same_cnt;
	bool nonpcm_flag;

	struct extn_chipinfo *chipinfo;
	int audio_type;
	struct snd_aes_iec958 iec;
	int frhdmirx_version;

	char default_edid[MAX_AUDIO_EDID_LENGTH];
	int default_edid_size;
	char user_setting_edid[MAX_AUDIO_EDID_LENGTH];
	int user_setting_edid_size;
	int hdmiin_audio_output_mode;
};

#define EXTN_BUFFER_BYTES (512 * 1024)
#define EXTN_RATES      (SNDRV_PCM_RATE_8000_192000)
#define EXTN_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static struct extn *s_extn;
static const struct snd_pcm_hardware extn_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,

	.formats = EXTN_FORMATS,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = EXTN_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

static snd_pcm_uframes_t extn_pointer(struct snd_pcm_substream *substream);

int get_hdmirx_mode(void)
{
	if (s_extn)
		return s_extn->hdmirx_mode;
	return 0;
}

int get_arc_version(void)
{
	if (s_extn)
		return s_extn->chipinfo->arc_version;
	return 0;
}

static void frhdmirx_nonpcm2pcm_clr_reset(struct extn *p_extn)
{
	p_extn->frhdmirx_cnt = 0;
	p_extn->frhdmirx_last_cnt = 0;
	p_extn->frhdmirx_same_cnt = 0;
}

static irqreturn_t extn_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct extn *p_extn = (struct extn *)dev_get_drvdata(dev);
	unsigned int period_dur_ms = 0;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

#ifdef DEBUG_IRQ
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && p_extn->tddr) {
		unsigned int addr = extn_pointer(substream);
		struct snd_pcm_runtime *runtime = substream->runtime;

		get_time_stamp(TODDR, p_extn->tddr->fifo_id, addr,
			(runtime->control->appl_ptr) % (runtime->buffer_size),
			runtime->buffer_size);
	}
#endif

	if (p_extn->frhdmirx_version == T7_FRHDMIRX) {
		frhdmirx_clr_SPDIF_irq_bits_for_t7_version();
		return IRQ_HANDLED;
	}

	period_dur_ms = substream->runtime->period_size * 1000 / substream->runtime->rate;

	/* check pcm or nonpcm for PAO*/
	if (p_extn->hdmirx_mode == HDMIRX_MODE_PAO) {
		/* by default, we check about 100 ms duration for the
		 * hdmirx data pipeline, if no PaPb there after 100ms, clear the PaPb flag
		 */
		unsigned int timeout_thres = 100 / period_dur_ms;

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
		int sample_rate_index = get_hdmi_sample_rate_index();

		/*192K & 176K audio*/
		if (sample_rate_index == 7 ||  sample_rate_index == 6)
			timeout_thres = timeout_thres * 2;
#endif
		if (p_extn->frhdmirx_last_cnt == p_extn->frhdmirx_cnt) {
			p_extn->frhdmirx_same_cnt++;

			if (p_extn->frhdmirx_same_cnt > timeout_thres)
				frhdmirx_nonpcm2pcm_clr_reset(p_extn);

			if (p_extn->frhdmirx_cnt == 0)
				p_extn->nonpcm_flag = false;
		} else {
			p_extn->frhdmirx_last_cnt = p_extn->frhdmirx_cnt;
			p_extn->frhdmirx_same_cnt = 0;
			p_extn->nonpcm_flag = true;
			frhdmirx_clr_PAO_irq_bits();
		}
	} else {
		frhdmirx_clr_SPDIF_irq_bits();
	}

	return IRQ_HANDLED;
}

static irqreturn_t frhdmirx_isr(int irq, void *devid)
{
	struct extn *p_extn = (struct extn *)devid;

	p_extn->frhdmirx_cnt++;
	if (p_extn->frhdmirx_cnt > MAX_INT - 2)
		frhdmirx_nonpcm2pcm_clr_reset(p_extn);

	return IRQ_HANDLED;
}

static int extn_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct extn *p_extn;
	int ret = 0;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	p_extn = (struct extn *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &extn_hardware);
	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, EXTN_BUFFER_BYTES / 2, EXTN_BUFFER_BYTES);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_extn->fddr = aml_audio_register_frddr(dev,
			p_extn->actrl,
			extn_ddr_isr, substream, false);
		if (!p_extn->fddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim from ddr\n");
			goto err_ddr;
		}
	} else {
		p_extn->tddr = aml_audio_register_toddr(dev,
			p_extn->actrl,
			extn_ddr_isr, substream);
		if (!p_extn->tddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim to ddr\n");
			goto err_ddr;
		}

		if (toddr_src_get() == FRHDMIRX && !p_extn->irq_on) {
			ret = request_irq(p_extn->irq_frhdmirx,
					frhdmirx_isr, IRQF_SHARED,
					"irq_frhdmirx", p_extn);
			if (ret) {
				ret = -ENXIO;
				dev_err(p_extn->dev, "failed to claim irq_frhdmirx %u\n",
							p_extn->irq_frhdmirx);
				goto err_irq;
			}
			p_extn->irq_on = true;
		}
	}

	runtime->private_data = p_extn;

	return 0;

err_irq:
	aml_audio_unregister_toddr(p_extn->dev, substream);
err_ddr:
	snd_pcm_lib_preallocate_free(substream);
	return ret;
}

static int extn_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = runtime->private_data;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aml_audio_unregister_frddr(p_extn->dev, substream);
	} else {
		aml_audio_unregister_toddr(p_extn->dev, substream);

		if (toddr_src_get() == FRHDMIRX && p_extn->irq_on) {
			frhdmirx_nonpcm2pcm_clr_reset(p_extn);
			frhdmirx_clr_all_irq_bits(p_extn->frhdmirx_version);
			free_irq(p_extn->irq_frhdmirx, p_extn);
			p_extn->irq_on = false;
		}
	}
	runtime->private_data = NULL;
	snd_pcm_lib_preallocate_free(substream);

	return 0;
}

static int extn_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int extn_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int extn_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static int extn_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_extn->fddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, fr->chipinfo->fifo_depth);
		threshold /= 2;
		/* Use all the fifo */
		aml_frddr_set_fifos(fr, fr->chipinfo->fifo_depth, threshold);

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_extn->tddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, to->chipinfo->fifo_depth);
		threshold /= 2;
		aml_toddr_set_fifos(to, threshold);

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t extn_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_extn->fddr);
	else
		addr = aml_toddr_get_position(p_extn->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

static int extn_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops extn_ops = {
	.open      = extn_open,
	.close     = extn_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = extn_hw_params,
	.hw_free   = extn_hw_free,
	.prepare   = extn_prepare,
	.trigger   = extn_trigger,
	.pointer   = extn_pointer,
	.mmap      = extn_mmap,
};

static int arc_get_enable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct extn *p_extn = snd_kcontrol_chip(kcontrol);

	if (!p_extn)
		return 0;

	ucontrol->value.integer.value[0] = p_extn->arc_en;

	return 0;
}

static int arc_set_enable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct extn *p_extn = snd_kcontrol_chip(kcontrol);

	if (!p_extn)
		return 0;

	p_extn->arc_en = ucontrol->value.integer.value[0];

	if (p_extn->chipinfo && p_extn->chipinfo->arc_version == TL1_ARC)
		arc_source_enable(p_extn->arc_src, p_extn->arc_en);
	else if (p_extn->chipinfo && p_extn->chipinfo->arc_version >= TM2_ARC)
		arc_enable(p_extn->arc_en, p_extn->chipinfo->arc_version);

	return 0;
}

static const struct snd_kcontrol_new extn_arc_controls[] = {
	SOC_SINGLE_BOOL_EXT("HDMI ARC Switch",
			    0,
			    arc_get_enable,
			    arc_set_enable),
};

static int extn_create_controls(struct snd_card *card,
				struct extn *p_extn)
{
	unsigned int idx;
	int err;

	for (idx = 0; idx < ARRAY_SIZE(extn_arc_controls); idx++) {
		err = snd_ctl_add(card,
				  snd_ctl_new1(&extn_arc_controls[idx], p_extn));
		if (err < 0)
			return err;
	}

	return 0;
}

static int extn_dai_probe(struct snd_soc_dai *cpu_dai)
{
	struct snd_card *card = cpu_dai->component->card->snd_card;
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	extn_create_controls(card, p_extn);

	if (p_extn->chipinfo && p_extn->chipinfo->arc_version == TL1_ARC)
		arc_source_enable(p_extn->arc_src, false);

	if (p_extn->chipinfo && p_extn->chipinfo->arc_version >= TM2_ARC) {
		/* set arc_earc_source_select by earc device exist or not */
		if (is_earc_spdif())
			arc_earc_source_select(EARCTX_SPDIF_TO_HDMIRX);
		else
			arc_earc_source_select(SPDIFA_TO_HDMIRX);
	}

	if (get_audioresample(RESAMPLE_A))
		resample_set(RESAMPLE_A, RATE_48K);

	return 0;
}

static int extn_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_extn->fddr;
		enum frddr_dest dst = frddr_src_get();

		pr_info("%s Expected frddr dst:%s\n",
			__func__,
			frddr_src_get_str(dst));

		aml_frddr_select_dst(fr, dst);
	} else {
		struct toddr *to = p_extn->tddr;
		unsigned int msb = 0, lsb = 0, toddr_type = 0;
		enum toddr_src src = toddr_src_get();
		struct toddr_fmt fmt;

		if (bit_depth == 24)
			toddr_type = 4;
		else
			toddr_type = 0;

		if (src == FRATV) {
			/* Now tv supports 48k, 16bits */
			if (bit_depth != 16 || runtime->rate != 48000) {
				pr_err("not support sample rate:%d, bits:%d\n",
					runtime->rate, bit_depth);
				return -EINVAL;
			}

			msb = 15;
			lsb = 0;

			/* commented it, selected by atv demod,
			 * select 0 for non standard signal.
			 */
			/* fratv_src_select(1); */
		} else if (src == FRHDMIRX) {
			if (bit_depth == 32)
				toddr_type = 3;
			else if (bit_depth == 24)
				toddr_type = 4;
			else
				toddr_type = 0;

			if (p_extn->hdmirx_mode == HDMIRX_MODE_PAO) { /* PAO */
				msb = 28 - 1 - 4;
				lsb = (bit_depth == 16) ? 24 - bit_depth : 4;
			} else { /* SPDIFIN */
				msb = 28 - 1;
				lsb = (bit_depth <= 24) ? 28 - bit_depth : 4;
			}

			if (get_resample_version() >= T5_RESAMPLE &&
			    get_resample_source(RESAMPLE_A) == FRHDMIRX) {
				msb = 31;
				lsb = 32 - bit_depth;
			}

			frhdmirx_ctrl(runtime->channels,
				      p_extn->hdmirx_mode,
				      p_extn->frhdmirx_version);
			if (p_extn->frhdmirx_version != T7_FRHDMIRX)
				frhdmirx_src_select(p_extn->hdmirx_mode);
		} else {
			pr_info("Not support toddr src:%s\n",
				toddr_src_get_str(src));
			return -EINVAL;
		}

		pr_debug("%s Expected toddr src:%s, m:%d, n:%d, toddr type:%d\n",
			__func__, toddr_src_get_str(src),
			msb, lsb, toddr_type);

		fmt.type      = toddr_type;
		fmt.msb       = msb;
		fmt.lsb       = lsb;
		fmt.endian    = 0;
		fmt.bit_depth = bit_depth;
		fmt.ch_num    = runtime->channels;
		fmt.rate      = runtime->rate;

		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, &fmt);
	}

	return 0;
}

static int extn_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);
	enum toddr_src src = toddr_src_get();

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "External Playback enable\n");

			aml_frddr_enable(p_extn->fddr, true);
		} else {
			dev_dbg(substream->pcm->card->dev,
				"External Capture enable\n");

			if (src == FRATV)
				fratv_enable(true);
			else if (src == FRHDMIRX)
				frhdmirx_enable(true, p_extn->frhdmirx_version);

			aml_toddr_enable(p_extn->tddr, true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "External Playback disable\n");

			aml_frddr_enable(p_extn->fddr, false);
		} else {
			bool toddr_stopped = false;

			if (src == FRATV)
				fratv_enable(false);
			else if (src == FRHDMIRX)
				frhdmirx_enable(false, p_extn->frhdmirx_version);

			dev_dbg(substream->pcm->card->dev,
				"External Capture disable\n");

			toddr_stopped = aml_toddr_burst_finished(p_extn->tddr);
			if (toddr_stopped)
				aml_toddr_enable(p_extn->tddr, false);
			else
				pr_err("%s(), toddr may be stuck\n", __func__);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int extn_dai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate = params_rate(params);
	int ret = 0;

	pr_info("%s:rate:%d, sysclk:%d\n",
		__func__,
		rate,
		p_extn->sysclk_freq);

	return ret;
}

static int extn_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);

	p_extn->sysclk_freq = freq;
	pr_info("extn_dai_set_sysclk, %d, %d, %d\n",
			clk_id, freq, dir);

	return 0;
}

static struct snd_soc_dai_ops extn_dai_ops = {
	.prepare = extn_dai_prepare,
	.trigger = extn_dai_trigger,
	.hw_params = extn_dai_hw_params,
	.set_sysclk = extn_dai_set_sysclk,
};

static struct snd_soc_dai_driver extn_dai[] = {
	{
		.name = "EXTN",
		.id = 0,
		.probe = extn_dai_probe,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates = EXTN_RATES,
		      .formats = EXTN_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates = EXTN_RATES,
		     .formats = EXTN_FORMATS,
		},
		.ops = &extn_dai_ops,
	},
};

static int frhdmirx_get_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);

	if (!p_extn)
		return 0;

	ucontrol->value.integer.value[0] = p_extn->hdmirx_mode;

	return 0;
}

static int frhdmirx_set_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);

	if (!p_extn)
		return 0;
	if (p_extn->frhdmirx_version == T7_FRHDMIRX) {
		pr_err("T7_FRHDMIRX, can't switch mode\n");
		return 0;
	}

	p_extn->hdmirx_mode = ucontrol->value.integer.value[0];

	return 0;
}

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
/* spdif in audio format detect: LPCM or NONE-LPCM */
struct spdif_audio_info {
	unsigned char aud_type;
	/*IEC61937 package presamble Pc value*/
	short pc;
	char *aud_type_str;
};

static const char *const spdif_audio_type_texts[] = {
	"LPCM",
	"AC3",
	"EAC3",
	"DTS",
	"DTS-HD",
	"TRUEHD",
	"PAUSE"
};

static const struct spdif_audio_info type_texts[] = {
	{0, 0, "LPCM"},
	{1, 0x1, "AC3"},
	{2, 0x15, "EAC3"},
	{3, 0xb, "DTS-I"},
	{3, 0x0c, "DTS-II"},
	{3, 0x0d, "DTS-III"},
	{4, 0x11, "DTS-IV"},
	{5, 0x16, "TRUEHD"},
	{6, 0x103, "PAUSE"},
	{6, 0x003, "PAUSE"},
	{6, 0x100, "PAUSE"},
};

static const struct soc_enum hdmirx_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdif_audio_type_texts),
			spdif_audio_type_texts);

static int hdmiin_check_audio_type(struct extn *p_extn)
{
	int total_num = sizeof(type_texts) / sizeof(struct spdif_audio_info);
	int pc = frhdmirx_get_chan_status_pc(p_extn->hdmirx_mode, p_extn->frhdmirx_version);
	int audio_type = 0;
	int i;

	if (!p_extn->nonpcm_flag && p_extn->hdmirx_mode)
		return audio_type;

	for (i = 0; i < total_num; i++) {
		if (pc == type_texts[i].pc) {
			audio_type = type_texts[i].aud_type;
			/* only for TL1 */
			if (audio_type != p_extn->audio_type &&
			    p_extn->chipinfo &&
			    !p_extn->chipinfo->PAO_channel_sync) {
				frhdmirx_afifo_reset();
				p_extn->audio_type = audio_type;
			}
			break;
		}
	}

	pr_debug("%s audio type:%d\n", __func__, audio_type);

	return audio_type;
}

static int hdmirx_audio_type_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);

	if (!p_extn)
		return 0;

	ucontrol->value.enumerated.item[0] =
		hdmiin_check_audio_type(p_extn);

	return 0;
}

int aml_get_audio_edid(struct snd_kcontrol *kcontrol,
		       unsigned int __user *bytes,
		       unsigned int size)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	int res;

	if (p_extn->default_edid_size == 0 &&
			p_extn->user_setting_edid_size == 0) {
		p_extn->default_edid_size =
			(int)rx_edid_get_aud_sad(p_extn->default_edid);
		memcpy(p_extn->user_setting_edid, p_extn->default_edid,
		       p_extn->default_edid_size);
	}

	res = copy_to_user(val, p_extn->user_setting_edid,
			   MAX_AUDIO_EDID_LENGTH);

	return 0;
}

int aml_set_audio_edid(struct snd_kcontrol *kcontrol,
		       const unsigned int __user *bytes,
		       unsigned int size)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	int res, edid_size = size - sizeof(*tlv);

	pr_info("%s edid_size = %d\n", __func__, edid_size);
	if ((edid_size % 3) != 0 || edid_size > (MAX_AUDIO_EDID_LENGTH - 3))
		return -EFAULT;

	if (edid_size == 0) {
		/* restore the default edid */
		p_extn->default_edid_size =
			(int)rx_edid_get_aud_sad(p_extn->default_edid);
		memset(p_extn->user_setting_edid, 0, MAX_AUDIO_EDID_LENGTH);
		memcpy(p_extn->user_setting_edid, p_extn->default_edid,
		       p_extn->default_edid_size);
		rx_edid_set_aud_sad(NULL, 0);
		pr_info("%s default_edid_size = %d\n", __func__,
			p_extn->default_edid_size);
	} else {
		/* update user setting edid */
		/* first 3 bytes for pcm, don't update. */
		memset(p_extn->user_setting_edid + 3, 0,
		       MAX_AUDIO_EDID_LENGTH - 3);
		res = copy_from_user(p_extn->user_setting_edid + 3,
				     val, edid_size);
		if (res)
			return -EFAULT;

		rx_edid_set_aud_sad(p_extn->user_setting_edid, edid_size + 3);
	}
	p_extn->user_setting_edid_size = edid_size;

	return 0;
}

int aml_get_hdmiin_audio_bitwidth(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);
	struct snd_aes_iec958 *iec;
	u32 status = 0, wl_status = 0;
	u32 wl;

	if (!p_extn)
		return 0;

	iec = &p_extn->iec;
	status = frhdmirx_get_ch_status(1, p_extn->frhdmirx_version);
	iec->status[4] = status & 0xff;
	wl_status = iec->status[4] & IEC958_AES4_CON_WORDLEN;
	if (iec->status[4] & IEC958_AES4_CON_MAX_WORDLEN_24) {
		switch (wl_status) {
		case IEC958_AES4_CON_WORDLEN_NOTID:
		default:
			wl = 0;
			break;
		case IEC958_AES4_CON_WORDLEN_20_16:
			wl = 2;
			break;
		case IEC958_AES4_CON_WORDLEN_24_20:
			wl = 3;
			break;
		}
	} else {
		switch (wl_status) {
		case IEC958_AES4_CON_WORDLEN_NOTID:
		default:
			wl = 0;
			break;
		case IEC958_AES4_CON_WORDLEN_20_16:
			wl = 1;
			break;
		case IEC958_AES4_CON_WORDLEN_24_20:
			wl = 2;
			break;
		}
	}

	ucontrol->value.integer.value[0] = wl;

	return 0;
}

static const char * const hdmiin_audio_output_mode_names[] = {
	/*  0 */ "SPDIF",
	/*  1 */ "I2S",
	/*  2 */ "TDM",
};

const struct soc_enum hdmiin_audio_output_mode_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(hdmiin_audio_output_mode_names),
			hdmiin_audio_output_mode_names);

int aml_get_hdmiin_audio_output_mode(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);

	ucontrol->value.integer.value[0] = p_extn->hdmiin_audio_output_mode;
	return 0;
}

int aml_set_hdmiin_audio_output_mode(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct extn *p_extn = dev_get_drvdata(component->dev);

	rx_set_audio_param(ucontrol->value.integer.value[0]);
	p_extn->hdmiin_audio_output_mode = ucontrol->value.integer.value[0];
	return 0;
}

#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
static int get_tvin_video_delay_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = get_tvin_delay();

	return 0;
}

static int set_tvin_video_delay_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value = 0;
	unsigned int max_delay = get_tvin_delay_max_ms();
	unsigned int min_delay = get_tvin_delay_min_ms();

	value = ucontrol->value.integer.value[0];
	if (!max_delay || !min_delay)
		return 0;

	if (value > max_delay)
		value = max_delay;
	else if (value < min_delay)
		value = min_delay;

	set_tvin_delay_duration(value);
	set_tvin_delay_start(1);

	pr_info("%s, set tvin video delay [%dms], range: (%d~%dms)\n",
			__func__, value, min_delay, max_delay);

	return 0;
}

static int get_tvin_video_max_delay_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = get_tvin_delay_max_ms();

	return 0;
}

static int get_tvin_video_min_delay_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = get_tvin_delay_min_ms();

	return 0;
}
#endif

static const struct snd_kcontrol_new extn_controls[] = {
	/* In */
	SOC_SINGLE_BOOL_EXT("SPDIFIN PAO",
		0,
		frhdmirx_get_mode,
		frhdmirx_set_mode),

#if (defined CONFIG_AMLOGIC_ATV_DEMOD ||\
		defined CONFIG_AMLOGIC_ATV_DEMOD_MODULE)
	SOC_ENUM_EXT("ATV audio stable",
		atv_audio_status_enum,
		aml_get_atv_audio_stable,
		NULL),
#endif

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT_MODULE)
	SOC_ENUM_EXT("AV audio stable",
		av_audio_status_enum,
		aml_get_av_audio_stable,
		NULL),
#endif

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
	SOC_ENUM_EXT("HDMIIN audio stable",
		hdmi_in_status_enum[0],
		aml_get_hdmiin_audio_stable,
		NULL),

	SOC_ENUM_EXT("HDMIIN audio samplerate",
		hdmi_in_status_enum[1],
		aml_get_hdmiin_audio_samplerate,
		NULL),

	SOC_ENUM_EXT("HDMIIN audio channels",
		hdmi_in_status_enum[2],
		aml_get_hdmiin_audio_channels,
		NULL),

	SOC_ENUM_EXT("HDMIIN audio format",
		hdmi_in_status_enum[3],
		aml_get_hdmiin_audio_format,
		NULL),

	SOC_ENUM_EXT("HDMIIN Audio Packet",
		hdmi_in_status_enum[4],
		aml_get_hdmiin_audio_packet,
		NULL),

	SOC_ENUM_EXT("HDMIIN Audio bit width",
		hdmi_in_status_enum[5],
		aml_get_hdmiin_audio_bitwidth,
		NULL),

	/* normally use "HDMIIN AUDIO EDID" to update audio edid*/
	SOC_SINGLE_BOOL_EXT("HDMI ATMOS EDID Switch",
		0,
		aml_get_atmos_audio_edid,
		aml_set_atmos_audio_edid),

	SOC_ENUM_EXT("HDMIIN Audio Type",
		hdmirx_audio_type_enum,
		hdmirx_audio_type_get_enum,
		NULL),

	SND_SOC_BYTES_TLV("HDMIIN AUDIO EDID",
		MAX_AUDIO_EDID_LENGTH,
		aml_get_audio_edid,
		aml_set_audio_edid),
	SOC_ENUM_EXT("HDMIIN Audio output mode",
		hdmiin_audio_output_mode_enum,
		aml_get_hdmiin_audio_output_mode,
		aml_set_hdmiin_audio_output_mode),
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	SOC_SINGLE_EXT("TVIN VIDEO DELAY",
		0, 0, 400, 0,
		get_tvin_video_delay_enum,
		set_tvin_video_delay_enum),
	SOC_SINGLE_EXT("TVIN VIDEO MIN DELAY",
		0, 0, 0, 0,
		get_tvin_video_min_delay_enum,
		NULL),
	SOC_SINGLE_EXT("TVIN VIDEO MAX DELAY",
		0, 0, 0, 0,
		get_tvin_video_max_delay_enum,
		NULL),
#endif
};

static const struct snd_soc_component_driver extn_component = {
	.name = DRV_NAME,
	.controls       = extn_controls,
	.ops = &extn_ops,
	.num_controls   = ARRAY_SIZE(extn_controls),
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
struct extn_chipinfo tl1_extn_chipinfo = {
	.arc_version	= TL1_ARC,
	.PAO_channel_sync = false,
	.frhdmirx_version = TL1_FRHDMIRX,
};
#endif

struct extn_chipinfo tm2_extn_chipinfo = {
	.arc_version	= TM2_ARC,
	.PAO_channel_sync = true,
	.frhdmirx_version = TL1_FRHDMIRX,
};

struct extn_chipinfo t7_extn_chipinfo = {
	.arc_version	= T7_ARC,
	.PAO_channel_sync = false,
	.frhdmirx_version = T7_FRHDMIRX,
};

static const struct of_device_id extn_device_id[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, snd-extn",
	},
	{
		.compatible = "amlogic, tl1-snd-extn",
		.data       = &tl1_extn_chipinfo,
	},
#endif
	{
		.compatible = "amlogic, tm2-snd-extn",
		.data       = &tm2_extn_chipinfo,
	},
	{
		.compatible = "amlogic, t7-snd-extn",
		.data       = &t7_extn_chipinfo,
	},
	{}
};

MODULE_DEVICE_TABLE(of, extn_device_id);

static int extn_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct extn *p_extn = NULL;
	struct extn_chipinfo *p_chipinfo;
	int ret = 0;

	p_extn = devm_kzalloc(dev, sizeof(struct extn), GFP_KERNEL);
	if (!p_extn)
		return -ENOMEM;

	p_extn->dev = dev;
	dev_set_drvdata(dev, p_extn);

	/* match data */
	p_chipinfo = (struct extn_chipinfo *)of_device_get_match_data(dev);
	p_extn->frhdmirx_version = 0;
	if (!p_chipinfo) {
		dev_warn_once(dev, "check whether to update chipinfo, exit\n");
		return -EINVAL;
	} else {
		p_extn->chipinfo = p_chipinfo;
		p_extn->frhdmirx_version = p_chipinfo->frhdmirx_version;
	}

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	p_extn->actrl = actrl;

	/* irqs */
	p_extn->irq_frhdmirx = platform_get_irq_byname(pdev, "irq_frhdmirx");
	if (p_extn->irq_frhdmirx < 0) {
		dev_err(dev, "Failed to get irq_frhdmirx:%d\n",
			p_extn->irq_frhdmirx);
		return -ENXIO;
	}
	p_extn->irq_on = false;

	/* Default ARC SRC */
	p_extn->arc_src = SPDIFA_TO_HDMIRX;

	if (p_extn->chipinfo) {
		if (p_extn->chipinfo->PAO_channel_sync) {
			p_extn->hdmirx_mode = HDMIRX_MODE_PAO;
#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
			rx_set_audio_param(HDMIRX_MODE_PAO);
#endif
		} else {
			p_extn->hdmirx_mode = HDMIRX_MODE_SPDIFIN;
#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
			rx_set_audio_param(HDMIRX_MODE_SPDIFIN);
#endif
		}
	}

	s_extn = p_extn;
	ret = devm_snd_soc_register_component(&pdev->dev,
					      &extn_component,
					      extn_dai,
					      ARRAY_SIZE(extn_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, register soc platform\n", __func__);
	return 0;
}

struct platform_driver extn_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = extn_device_id,
	},
	.probe = extn_platform_probe,
};

int __init extn_init(void)
{
	return platform_driver_register(&(extn_driver));
}

void __exit extn_exit(void)
{
	platform_driver_unregister(&extn_driver);
}

#ifndef MODULE
module_init(extn_init);
module_exit(extn_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic External Input/Output ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, extn_device_id);
#endif
