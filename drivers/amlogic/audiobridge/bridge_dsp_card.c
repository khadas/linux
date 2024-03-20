// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "bridge_dsp_card.h"

/*
 * define pcm harward buffer capability not real size
 * support max period bytes size = 8channels * 4bytes * 2048(period size)
 * max buffer size = max period size * period count, real size much smaller than this value
 **/
#define PRD_SIZE_MAX     (PAGE_SIZE * 64)
#define BUFF_SIZE_MAX    (PRD_SIZE_MAX * 4)
#define SOUND_CARD_ID    (2)

struct aml_aprocess_card *aprocess_card;
unsigned int dsp_pcm_num;
static struct snd_pcm_hardware aml_aprocess_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER
		 | SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID
		 | SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.rates = SNDRV_PCM_RATE_CONTINUOUS,
	.buffer_bytes_max = BUFF_SIZE_MAX,
	.period_bytes_max = PRD_SIZE_MAX,
	.periods_min      = BUFF_SIZE_MAX / PRD_SIZE_MAX,
	.periods_max      = BUFF_SIZE_MAX / PRD_SIZE_MAX,
};

void aml_aprocess_set_hw(struct aml_aprocess *p_aprocess, int channels,
					int format, int rate, int period)
{
	if (!p_aprocess)
		return;
	p_aprocess->g_hw.rate_min = rate;
	p_aprocess->g_hw.rate_max = rate;
	p_aprocess->g_hw.channels_min = channels;
	p_aprocess->g_hw.channels_max = channels;
	p_aprocess->g_hw.formats = format;
	if (format == SNDRV_PCM_FMTBIT_S8)
		p_aprocess->g_hw.period_bytes_min = channels * period;
	else if (format == SNDRV_PCM_FMTBIT_S16)
		p_aprocess->g_hw.period_bytes_min = 2 * channels * period;
	else if (format == SNDRV_PCM_FMTBIT_S32)
		p_aprocess->g_hw.period_bytes_min = 4 * channels * period;
	else
		p_aprocess->g_hw.period_bytes_min = 2 * channels * period;
}

int aml_aprocess_complete(struct aml_aprocess *p_aprocess, char *out, int size)
{
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	unsigned int pending;
	unsigned long flags, flags2;
	unsigned int hw_ptr;

	if (!p_aprocess)
		return 0;

	spin_lock_irqsave(&p_aprocess->lock, flags);
	if (!p_aprocess->status)
		goto fail;

	substream = p_aprocess->g_substream;
	if (!substream)
		goto fail;

	snd_pcm_stream_lock_irqsave(substream, flags2);

	runtime = substream->runtime;
	if (!runtime || !snd_pcm_running(substream)) {
		snd_pcm_stream_unlock_irqrestore(substream, flags2);
		goto fail;
	}
	hw_ptr = p_aprocess->g_hwptr;

	/* Pack DSP load in ALSA ring buffer */
	pending = runtime->dma_bytes - hw_ptr;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (unlikely(pending < size)) {
			memcpy(out, runtime->dma_area + hw_ptr, pending);
			memcpy(out + pending, runtime->dma_area,
			       size - pending);
		} else {
			memcpy(out, runtime->dma_area + hw_ptr,
			       size);
		}
	} else {
		if (unlikely(pending < size)) {
			memcpy(runtime->dma_area + hw_ptr, out, pending);
			memcpy(runtime->dma_area, out + pending,
			       size - pending);
		} else {
			memcpy(runtime->dma_area + hw_ptr, out,
			       size);
		}
	}
	/* update hw_ptr after data is copied to memory */
	p_aprocess->g_hwptr = (hw_ptr + size) % runtime->dma_bytes;
	hw_ptr = p_aprocess->g_hwptr;
	snd_pcm_stream_unlock_irqrestore(substream, flags2);
	spin_unlock_irqrestore(&p_aprocess->lock, flags);
	if ((hw_ptr % snd_pcm_lib_period_bytes(substream)) < size)
		snd_pcm_period_elapsed(substream);

	return size;
fail:
	spin_unlock_irqrestore(&p_aprocess->lock, flags);
	return 0;
}

static int aml_aprocess_open(struct snd_pcm_substream *substream)
{
	struct aml_aprocess *p_aprocess;
	struct aml_aprocess_card *apro_card = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		p_aprocess = (struct aml_aprocess *)apro_card->p_prm;
	else
		p_aprocess = (struct aml_aprocess *)apro_card->c_prm;

	runtime->hw = p_aprocess->g_hw;
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	return 0;
}

static int aml_aprocess_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int aml_aprocess_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_aprocess_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int aml_aprocess_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int aml_aprocess_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct aml_aprocess *p_aprocess;
	struct aml_aprocess_card *apro_card = snd_pcm_substream_chip(substream);
	unsigned long flags;
	int err = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		p_aprocess = (struct aml_aprocess *)apro_card->p_prm;
	else
		p_aprocess = (struct aml_aprocess *)apro_card->c_prm;

	spin_lock_irqsave(&p_aprocess->lock, flags);
	p_aprocess->g_hwptr = 0;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		p_aprocess->status = 1;
		p_aprocess->g_substream = substream;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		p_aprocess->status = 0;
		p_aprocess->g_substream = NULL;
		break;
	default:
		err = -EINVAL;
	}
	spin_unlock_irqrestore(&p_aprocess->lock, flags);
	return err;
}

static snd_pcm_uframes_t aml_aprocess_pointer(struct snd_pcm_substream *substream)
{
	struct aml_aprocess *p_aprocess;
	struct aml_aprocess_card *apro_card = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		p_aprocess = (struct aml_aprocess *)apro_card->p_prm;
	else
		p_aprocess = (struct aml_aprocess *)apro_card->c_prm;

	return bytes_to_frames(runtime, p_aprocess->g_hwptr);
}

static struct snd_pcm_ops aml_aprocess_ops = {
	.open = aml_aprocess_open,
	.close = aml_aprocess_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_aprocess_hw_params,
	.hw_free = aml_aprocess_hw_free,
	.prepare = aml_aprocess_prepare,
	.trigger = aml_aprocess_trigger,
	.pointer = aml_aprocess_pointer,
};

struct aml_aprocess *aml_aprocess_init(struct device *dev, const char *pcm_name,
					const char *card_name, enum PCM_MODE modeid)
{
	struct aml_aprocess *p_aprocess;
	struct snd_card *card;
	struct snd_pcm *pcm;
	int i, err;

	p_aprocess = kzalloc(sizeof(*p_aprocess), GFP_KERNEL);
	if (!p_aprocess)
		return NULL;
	dsp_pcm_num++;
	if (!aprocess_card) {
		aprocess_card = kzalloc(sizeof(*aprocess_card), GFP_KERNEL);
		if (!aprocess_card)
			return NULL;

		aprocess_card->dev = dev;
		/* Choose any slot, with no id */
		for (i = 0; i < 3; i++) {
			err = snd_card_new(dev,
					SOUND_CARD_ID + i, NULL, THIS_MODULE, 0, &card);
			if (!err)
				break;
		}
		if (err < 0)
			goto fail;

		aprocess_card->card = card;
		/*
		 * Create first PCM device
		 * Create a substream only for non-zero channel streams
		 */
		err = snd_pcm_new(card, pcm_name, 0, 1, 1, &pcm);
		if (err < 0)
			goto snd_fail;

		strlcpy(pcm->name, pcm_name, sizeof(pcm->name));
		pcm->private_data = aprocess_card;
		aprocess_card->pcm = pcm;

		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &aml_aprocess_ops);
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &aml_aprocess_ops);

		strlcpy(card->driver, card_name, sizeof(card->driver));
		strlcpy(card->shortname, card_name, sizeof(card->shortname));
		sprintf(card->longname, "%s %i", card_name, card->dev->id);

		snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
			snd_dma_continuous_data(GFP_KERNEL), 0, BUFF_SIZE_MAX);

		err = snd_card_register(card);
		if (err < 0)
			goto snd_fail;
	}
	if (modeid == PCM_CAPTURE)
		aprocess_card->c_prm = p_aprocess;
	else
		aprocess_card->p_prm = p_aprocess;

	p_aprocess->a_card = aprocess_card;
	p_aprocess->status = 0;
	spin_lock_init(&p_aprocess->lock);
	memcpy(&p_aprocess->g_hw, &aml_aprocess_hardware, sizeof(p_aprocess->g_hw));
	return p_aprocess;
snd_fail:
	dsp_pcm_num--;
	if (!dsp_pcm_num && aprocess_card) {
		snd_card_free(card);
		kfree(aprocess_card);
		aprocess_card = NULL;
	}
fail:
	kfree(p_aprocess);
	return NULL;
}

void aml_aprocess_destroy(struct aml_aprocess *p_aprocess)
{
	if (!p_aprocess)
		return;
	dsp_pcm_num--;
	if (!dsp_pcm_num && aprocess_card) {
		snd_card_free(aprocess_card->card);
		kfree(aprocess_card);
		aprocess_card = NULL;
	}
	kfree(p_aprocess);
}
