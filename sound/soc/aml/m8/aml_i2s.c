/*
 * sound/soc/aml/m8/aml_i2s.c
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
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/major.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

/* Amlogic headers */
#include <linux/amlogic/iomap.h>
#include "aml_i2s.h"
#include "aml_spdif_dai.h"
#include "aml_audio_hw.h"
#include <linux/amlogic/sound/aiu_regs.h>

#define USE_HW_TIMER
#ifdef USE_HW_TIMER
#define XRUN_NUM 100 /*1ms*100=100ms timeout*/
#else
#define XRUN_NUM 10 /*10ms*10=100ms timeout*/
#endif

unsigned long aml_i2s_playback_start_addr = 0;
EXPORT_SYMBOL(aml_i2s_playback_start_addr);

unsigned long aml_i2s_playback_phy_start_addr = 0;
EXPORT_SYMBOL(aml_i2s_playback_phy_start_addr);

unsigned long aml_i2s_alsa_write_addr = 0;
EXPORT_SYMBOL(aml_i2s_alsa_write_addr);

unsigned int aml_i2s_playback_channel = 2;
EXPORT_SYMBOL(aml_i2s_playback_channel);

static int trigger_underrun;
void aml_audio_hw_trigger(void)
{
	trigger_underrun = 1;
}
EXPORT_SYMBOL(aml_audio_hw_trigger);

static void aml_i2s_timer_callback(unsigned long data);

/*--------------------------------------------------------------------------*\
 * Hardware definition
\*--------------------------------------------------------------------------*/
/* TODO: These values were taken from the AML platform driver, check
 *	 them against real values for AML
 */
static const struct snd_pcm_hardware aml_i2s_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,

	.formats =
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min = 64,
	.period_bytes_max = 32 * 1024 * 2,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 128 * 1024 * 2 * 2,

	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 8,
	.fifo_size = 0,
};

static const struct snd_pcm_hardware aml_i2s_capture = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER |
	    SNDRV_PCM_INFO_MMAP |
	    SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_PAUSE,

	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min = 64,
	.period_bytes_max = 32 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 64 * 1024,

	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 8,
	.fifo_size = 0,
};

static unsigned int period_sizes[] = {
	64, 128, 256, 512, 1024, 2048, 4096, 8192,
	16384, 32768, 65536, 65536 * 2, 65536 * 4
};

static struct snd_pcm_hw_constraint_list hw_constraints_period_sizes = {
	.count = ARRAY_SIZE(period_sizes),
	.list = period_sizes,
	.mask = 0
};

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*\
 * Helper functions
\*--------------------------------------------------------------------------*/
static int aml_i2s_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{

	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = NULL;
	size_t size = 0;

	tmp_buf = kzalloc(sizeof(struct aml_audio_buffer), GFP_KERNEL);
	if (tmp_buf == NULL) {
		dev_err(pcm->card->dev, "allocate tmp buffer error\n");
		return -ENOMEM;
	}
	buf->private_data = tmp_buf;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* malloc DMA buffer */
		size = aml_i2s_hardware.buffer_bytes_max;
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = pcm->card->dev;
		/* one size for i2s output, another for 958,
		*and 128 for alignment
		*/
		buf->area = dma_alloc_coherent(pcm->card->dev, size + 4096,
					       &buf->addr, GFP_KERNEL);
		if (!buf->area) {
			dev_err(pcm->card->dev, "alloc playback DMA buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		buf->bytes = size;
		/* malloc tmp buffer */
		size = aml_i2s_hardware.buffer_bytes_max;
		tmp_buf->buffer_start = kzalloc(size, GFP_KERNEL);
		if (tmp_buf->buffer_start == NULL) {
			dev_err(pcm->card->dev, "alloc playback tmp buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		tmp_buf->buffer_size = size;

	} else {
		/* malloc DMA buffer */
		size = aml_i2s_capture.buffer_bytes_max;
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = pcm->card->dev;
		buf->area = dma_alloc_coherent(pcm->card->dev, size * 2,
					       &buf->addr, GFP_KERNEL);

		if (!buf->area) {
			dev_err(pcm->card->dev, "alloc capture DMA buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		buf->bytes = size;
		/* malloc tmp buffer */
		size = aml_i2s_capture.period_bytes_max;
		tmp_buf->buffer_start = kzalloc(size, GFP_KERNEL);
		if (tmp_buf->buffer_start == NULL) {
			dev_err(pcm->card->dev, "alloc capture tmp buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		tmp_buf->buffer_size = size;
	}

	return 0;

}

/*--------------------------------------------------------------------------*\
 * ISR
\*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*\
 * i2s operations
\*--------------------------------------------------------------------------*/
static int aml_i2s_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;

	/*
	* this may get called several times by oss emulation
	* with different params
	*/
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);
	s->I2S_addr = runtime->dma_addr;

	/*
	* Both capture and playback need to reset the last ptr
	* to the start address, playback and capture use
	* different address calculate, so we reset the different
	* start address to the last ptr
	*/
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* s->last_ptr must initialized as dma buffer's start addr */
		s->last_ptr = runtime->dma_addr;
	} else {

		s->last_ptr = 0;
	}

	return 0;
}

static int aml_i2s_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int aml_i2s_prepare(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buf->private_data;

	if (s && s->device_type == AML_AUDIO_I2SOUT && trigger_underrun) {
		dev_info(substream->pcm->card->dev, "clear i2s out trigger underrun\n");
		trigger_underrun = 0;
	}
	if (s && s->device_type == AML_AUDIO_I2SOUT)
		aml_i2s_playback_channel = runtime->channels;
	tmp_buf->cached_len = 0;
	return 0;
}

#ifdef USE_HW_TIMER
int hw_timer_init = 0;
static irqreturn_t audio_isr_handler(int irq, void *data)
{
	struct aml_runtime_data *prtd = data;
	struct snd_pcm_substream *substream = prtd->substream;
	aml_i2s_timer_callback((unsigned long)substream);
	return IRQ_HANDLED;
}

static int snd_free_hw_timer_irq(void *data)
{
	free_irq(INT_TIMER_D, data);
	return 0;
}

static int snd_request_hw_timer(void *data)
{
	int ret = 0;
	if (hw_timer_init == 0) {
		aml_write_cbus(ISA_TIMERD, TIMER_COUNT);
		aml_cbus_update_bits(ISA_TIMER_MUX, 3 << 6,
					TIMERD_RESOLUTION << 6);
		aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 15, TIMERD_MODE << 15);
		aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 19, 1 << 19);
		hw_timer_init = 1;
	}
	ret = request_irq(INT_TIMER_D, audio_isr_handler,
				IRQF_SHARED, "timerd_irq", data);
		if (ret < 0) {
			pr_err("audio hw interrupt register fail\n");
			return -1;
		}
	return 0;
}

#endif

static void start_timer(struct aml_runtime_data *prtd)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (!prtd->active) {
#ifndef USE_HW_TIMER
		prtd->timer.expires = jiffies + 1;
		add_timer(&prtd->timer);
#endif
		prtd->active = 1;
		prtd->xrun_num = 0;
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);

}

static void stop_timer(struct aml_runtime_data *prtd)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (prtd->active) {
#ifndef USE_HW_TIMER
		del_timer(&prtd->timer);
#endif
		prtd->active = 0;
		prtd->xrun_num = 0;
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);
}


static int aml_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct aml_runtime_data *prtd = rtd->private_data;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		start_timer(prtd);
		break;		/* SNDRV_PCM_TRIGGER_START */
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		stop_timer(prtd);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t aml_i2s_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;

	unsigned int addr, ptr;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (s->device_type == AML_AUDIO_I2SOUT)
			ptr = read_i2s_rd_ptr();
		else
			ptr = read_iec958_rd_ptr();
		addr = ptr - s->I2S_addr;
		return bytes_to_frames(runtime, addr);
	} else {
		if (s->device_type == AML_AUDIO_I2SIN)
			ptr = audio_in_i2s_wr_ptr();
		else
			ptr = audio_in_spdif_wr_ptr();
		addr = ptr - s->I2S_addr;
		return bytes_to_frames(runtime, addr) / 2;
	}

	return 0;
}

static void aml_i2s_timer_callback(unsigned long data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = NULL;
	struct audio_stream *s = NULL;
	int elapsed = 0;
	unsigned int last_ptr, size = 0;
	unsigned long flags = 0;

	if (runtime == NULL || runtime->private_data == NULL)
		return;

	prtd = runtime->private_data;
	s = &prtd->s;

	if (prtd->active == 0)
		return;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (s->device_type == AML_AUDIO_I2SOUT)
			last_ptr = read_i2s_rd_ptr();
		else
			last_ptr = read_iec958_rd_ptr();
		if (last_ptr < s->last_ptr) {
			size =
				runtime->dma_bytes + last_ptr -
				(s->last_ptr);
		} else {
			size = last_ptr - (s->last_ptr);
		}
		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(substream->runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			elapsed = 1;
		}
	} else {
		if (s->device_type == AML_AUDIO_I2SIN)
			last_ptr = audio_in_i2s_wr_ptr();
		else
			last_ptr = audio_in_spdif_wr_ptr();
		if (last_ptr < s->last_ptr) {
			size =
				runtime->dma_bytes + (last_ptr -
						  (s->last_ptr)) / 2;
			prtd->xrun_num = 0;
		} else if (last_ptr == s->last_ptr) {
			if (prtd->xrun_num++ > XRUN_NUM) {
				/*dev_info(substream->pcm->card->dev,
					"alsa capture long time no data, quit xrun!\n");
				*/
				prtd->xrun_num = 0;
				s->size = runtime->period_size;
			}
		} else {
			size = (last_ptr - (s->last_ptr)) / 2;
			prtd->xrun_num = 0;
		}
		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(substream->runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			elapsed = 1;
		}
	}

#ifndef USE_HW_TIMER
	mod_timer(&prtd->timer, jiffies + 1);
#endif

	spin_unlock_irqrestore(&prtd->timer_lock, flags);
	if (elapsed)
		snd_pcm_period_elapsed(substream);
}


static int aml_i2s_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct audio_stream *s = &prtd->s;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_set_runtime_hwparams(substream, &aml_i2s_hardware);
		if (s->device_type == AML_AUDIO_I2SOUT) {
			aml_i2s_playback_start_addr = (unsigned long)buf->area;
			aml_i2s_playback_phy_start_addr = buf->addr;
		}
	} else {
		snd_soc_set_runtime_hwparams(substream, &aml_i2s_capture);
	}

	/* ensure that peroid size is a multiple of 32bytes */
	ret =
	    snd_pcm_hw_constraint_list(runtime, 0,
				       SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				       &hw_constraints_period_sizes);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev,
			"set period bytes constraint error\n");
		goto out;
	}

	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev, "set period error\n");
		goto out;
	}
	if (!prtd) {
		prtd = kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			dev_err(substream->pcm->card->dev, "alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}

	spin_lock_init(&prtd->timer_lock);

#ifndef USE_HW_TIMER
	init_timer(&prtd->timer);
	prtd->timer.function = &aml_i2s_timer_callback;
	prtd->timer.data = (unsigned long)substream;
#else
	ret = snd_request_hw_timer(prtd);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev, "request audio hw timer failed\n");
		goto out;
	}
#endif

 out:
	return ret;
}

static int aml_i2s_close(struct snd_pcm_substream *substream)
{
	struct aml_runtime_data *prtd = substream->runtime->private_data;

#ifdef USE_HW_TIMER
	snd_free_hw_timer_irq(prtd);
#endif
	kfree(prtd);
	prtd = NULL;

	return 0;
}

static int aml_i2s_copy_playback(struct snd_pcm_runtime *runtime, int channel,
				 snd_pcm_uframes_t pos,
				 void __user *buf, snd_pcm_uframes_t count,
				 struct snd_pcm_substream *substream)
{
	int res = 0;
	int n;
	int i = 0, j = 0;
	int align = runtime->channels * 32;
	unsigned long offset = frames_to_bytes(runtime, pos);
	char *hwbuf = runtime->dma_area + offset;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buffer->private_data;
	void *ubuf = tmp_buf->buffer_start;
	struct audio_stream *s = &prtd->s;
	struct device *dev = substream->pcm->card->dev;
	int cached_len = tmp_buf->cached_len;
	char *cache_buffer_bytes = tmp_buf->cache_buffer_bytes;

	n = frames_to_bytes(runtime, count);
	if (n > tmp_buf->buffer_size) {
		dev_err(dev, "FATAL_ERR:UserData/%d > buffer_size/%d\n",
				n, tmp_buf->buffer_size);
		return -EFAULT;
	}
	res = copy_from_user(ubuf, buf, n);
	if (res)
		return -EFAULT;

	/*mask align byte(64 or 256)*/
	if ((cached_len != 0 || (n % align) != 0)) {
		int byte_size = n;
		int total_len;
		int ouput_len;
		int next_cached_len;
		int cache_buffer_bytes_tmp[256];

		offset -= cached_len;
		hwbuf = runtime->dma_area + offset;

		total_len = byte_size + cached_len;
		ouput_len = total_len & (~(align - 1));
		next_cached_len = total_len - ouput_len;

		if (next_cached_len)
			memcpy((void *)cache_buffer_bytes_tmp,
				(void *)((char *)ubuf +
				byte_size - next_cached_len),
				next_cached_len);
		memmove((void *)((char *)ubuf + cached_len),
				(void *)ubuf, ouput_len - cached_len);
		if (cached_len)
			memcpy((void *)ubuf,
				(void *)cache_buffer_bytes, cached_len);
		if (next_cached_len)
			memcpy((void *)cache_buffer_bytes,
				(void *)cache_buffer_bytes_tmp,
				next_cached_len);

		tmp_buf->cached_len = next_cached_len;
		n = ouput_len;
	}
	/*end of mask*/

	if (s->device_type == AML_AUDIO_I2SOUT)
		aml_i2s_alsa_write_addr = offset;

	if (access_ok(VERIFY_READ, buf, frames_to_bytes(runtime, count))) {
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {

			int16_t *tfrom, *to, *left, *right;
			tfrom = (int16_t *) ubuf;
			to = (int16_t *) hwbuf;

			left = to;
			right = to + 16;

			for (j = 0; j < n; j += 64) {
				for (i = 0; i < 16; i++) {
					*left++ = (*tfrom++);
					*right++ = (*tfrom++);
				}
				left += 16;
				right += 16;
			}
		} else if (runtime->format == SNDRV_PCM_FORMAT_S24_LE
			   && I2S_MODE == AIU_I2S_MODE_PCM24) {
			int32_t *tfrom, *to, *left, *right;
			tfrom = (int32_t *) ubuf;
			to = (int32_t *) hwbuf;

			left = to;
			right = to + 8;

			for (j = 0; j < n; j += 64) {
				for (i = 0; i < 8; i++) {
					*left++ = (*tfrom++);
					*right++ = (*tfrom++);
				}
				left += 8;
				right += 8;
			}

		} else if (runtime->format == SNDRV_PCM_FORMAT_S32_LE) {
			int32_t *tfrom, *to, *left, *right;
			tfrom = (int32_t *) ubuf;
			to = (int32_t *) hwbuf;

			left = to;
			right = to + 8;

			if (runtime->channels == 8) {
				int32_t *lf, *cf, *rf, *ls, *rs, *lef, *sbl,
				    *sbr;
				lf = to;
				cf = to + 8 * 1;
				rf = to + 8 * 2;
				ls = to + 8 * 3;
				rs = to + 8 * 4;
				lef = to + 8 * 5;
				sbl = to + 8 * 6;
				sbr = to + 8 * 7;
				for (j = 0; j < n; j += 256) {
					for (i = 0; i < 8; i++) {
						*lf++ = (*tfrom++) >> 8;
						*cf++ = (*tfrom++) >> 8;
						*rf++ = (*tfrom++) >> 8;
						*ls++ = (*tfrom++) >> 8;
						*rs++ = (*tfrom++) >> 8;
						*lef++ = (*tfrom++) >> 8;
						*sbl++ = (*tfrom++) >> 8;
						*sbr++ = (*tfrom++) >> 8;
					}
					lf += 7 * 8;
					cf += 7 * 8;
					rf += 7 * 8;
					ls += 7 * 8;
					rs += 7 * 8;
					lef += 7 * 8;
					sbl += 7 * 8;
					sbr += 7 * 8;
				}
			} else {
				for (j = 0; j < n; j += 64) {
					for (i = 0; i < 8; i++) {
						*left++ = (*tfrom++) >> 8;
						*right++ = (*tfrom++) >> 8;
					}
					left += 8;
					right += 8;
				}
			}
		}

	} else {
		res = -EFAULT;
	}
	return res;
}

static int aml_i2s_copy_capture(struct snd_pcm_runtime *runtime, int channel,
				snd_pcm_uframes_t pos,
				void __user *buf, snd_pcm_uframes_t count,
				struct snd_pcm_substream *substream)
{
	unsigned int *tfrom, *left, *right;
	unsigned short *to;
	int res = 0, n = 0, i = 0, j = 0;
	unsigned int t1, t2;
	unsigned char r_shift = 8;
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos) * 2;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buffer->private_data;
	void *ubuf = tmp_buf->buffer_start;
	struct device *dev = substream->pcm->card->dev;
	to = (unsigned short *)ubuf;	/* tmp buf; */
	tfrom = (unsigned int *)hwbuf;	/* 32bit buffer */
	n = frames_to_bytes(runtime, count);
	if (access_ok(VERIFY_WRITE, buf, frames_to_bytes(runtime, count))) {
		if (runtime->channels == 2) {
			left = tfrom;
			right = tfrom + 8;
			if (pos % 8)
				dev_err(dev, "audio data unligned\n");

			if ((n * 2) % 64)
				dev_err(dev, "audio data unaligned 64 bytes\n");

			for (j = 0; j < n * 2; j += 64) {
				for (i = 0; i < 8; i++) {
					t1 = (*left++);
					t2 = (*right++);
					*to++ =
					    (unsigned short)((t1 >> r_shift) &
							     0xffff);
					*to++ =
					    (unsigned short)((t2 >> r_shift) &
							     0xffff);
				}
				left += 8;
				right += 8;
			}
			/* clean hw buffer */
			memset(hwbuf, 0, n * 2);
		}
	}
	res = copy_to_user(buf, ubuf, n);
	return res;
}

static int aml_i2s_copy(struct snd_pcm_substream *substream, int channel,
			snd_pcm_uframes_t pos,
			void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret =
		    aml_i2s_copy_playback(runtime, channel, pos, buf, count,
					  substream);
	} else {
		ret =
		    aml_i2s_copy_capture(runtime, channel, pos, buf, count,
					 substream);
	}
	return ret;
}

int aml_i2s_silence(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	char *ppos;
	int n;
	struct snd_pcm_runtime *runtime = substream->runtime;

	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);
	return 0;
}

static struct snd_pcm_ops aml_i2s_ops = {
	.open = aml_i2s_open,
	.close = aml_i2s_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_i2s_hw_params,
	.hw_free = aml_i2s_hw_free,
	.prepare = aml_i2s_prepare,
	.trigger = aml_i2s_trigger,
	.pointer = aml_i2s_pointer,
	.copy = aml_i2s_copy,
	.silence = aml_i2s_silence,
};

/*--------------------------------------------------------------------------*\
 * ASoC platform driver
\*--------------------------------------------------------------------------*/
static u64 aml_i2s_dmamask = 0xffffffff;

static int aml_i2s_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_card *card = rtd->card;
	struct snd_pcm *pcm = rtd->pcm;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &aml_i2s_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = aml_i2s_preallocate_dma_buffer(pcm,
						     SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = aml_i2s_preallocate_dma_buffer(pcm,
						     SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

 out:
	return ret;
}

static void aml_i2s_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	struct aml_audio_buffer *tmp_buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(pcm->card->dev, buf->bytes,
				  buf->area, buf->addr);
		buf->area = NULL;

		tmp_buf = buf->private_data;
		if (tmp_buf->buffer_start != NULL && tmp_buf != NULL)
			kfree(tmp_buf->buffer_start);
		if (tmp_buf != NULL)
			kfree(tmp_buf);
		buf->private_data = NULL;
	}
}

#ifdef CONFIG_PM
static int aml_i2s_suspend(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	struct aml_runtime_data *prtd;
	struct aml_i2s_dma_params *params;
	if (!runtime)
		return 0;

	prtd = runtime->private_data;
	params = prtd->params;

	/* disable the PDC and save the PDC registers */
	/* TODO */
	return 0;
}

static int aml_i2s_resume(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	struct aml_runtime_data *prtd;
	struct aml_i2s_dma_params *params;
	if (!runtime)
		return 0;

	prtd = runtime->private_data;
	params = prtd->params;

	/* restore the PDC registers and enable the PDC */
	/* TODO */
	return 0;
}
#else
#define aml_i2s_suspend	NULL
#define aml_i2s_resume	NULL
#endif

bool aml_audio_i2s_mute_flag = 0;
static int aml_audio_set_i2s_mute(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	aml_audio_i2s_mute_flag = ucontrol->value.integer.value[0];
	pr_info("aml_audio_i2s_mute_flag: flag=%d\n", aml_audio_i2s_mute_flag);
	if (aml_audio_i2s_mute_flag)
		aml_audio_i2s_mute();
	else
		aml_audio_i2s_unmute();
	return 0;
}

static int aml_audio_get_i2s_mute(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aml_audio_i2s_mute_flag;
	return 0;
}

static const struct snd_kcontrol_new aml_i2s_controls[] = {
	SOC_SINGLE_BOOL_EXT("Audio i2s mute",
				0, aml_audio_get_i2s_mute,
				aml_audio_set_i2s_mute),
};
static int aml_i2s_probe(struct snd_soc_platform *platform)
{
	return snd_soc_add_platform_controls(platform,
			aml_i2s_controls, ARRAY_SIZE(aml_i2s_controls));
}

struct snd_soc_platform_driver aml_soc_platform = {
	.probe = aml_i2s_probe,
	.ops = &aml_i2s_ops,
	.pcm_new = aml_i2s_new,
	.pcm_free = aml_i2s_free_dma_buffers,
	.suspend = aml_i2s_suspend,
	.resume = aml_i2s_resume,
};

static int aml_soc_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &aml_soc_platform);
}

static int aml_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_audio_dt_match[] = {
	{.compatible = "amlogic, aml-i2s",
	 },
	{},
};
#else
#define amlogic_audio_dt_match NULL
#endif

#ifdef CONFIG_HIBERNATION
static unsigned long isa_timerd_saved;
static unsigned long isa_timerd_mux_saved;
static int aml_i2s_freeze(struct device *dev)
{
	isa_timerd_saved = aml_read_cbus(ISA_TIMERD);
	isa_timerd_mux_saved = aml_read_cbus(ISA_TIMER_MUX);
	return 0;
}

static int aml_i2s_thaw(struct device *dev)
{
	return 0;
}

static int aml_i2s_restore(struct device *dev)
{
	aml_write_cbus(ISA_TIMERD, isa_timerd_saved);
	aml_write_cbus(ISA_TIMER_MUX, isa_timerd_mux_saved);
	return 0;
}

const struct dev_pm_ops aml_i2s_pm = {
	.freeze		= aml_i2s_freeze,
	.thaw		= aml_i2s_thaw,
	.restore	= aml_i2s_restore,
};
#endif

static struct platform_driver aml_i2s_driver = {
	.driver = {
		.name = "aml-i2s",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_audio_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm     = &aml_i2s_pm,
#endif
		},

	.probe = aml_soc_platform_probe,
	.remove = aml_soc_platform_remove,
};

static int __init aml_alsa_audio_init(void)
{
	return platform_driver_register(&aml_i2s_driver);
}

static void __exit aml_alsa_audio_exit(void)
{
	platform_driver_unregister(&aml_i2s_driver);
}

module_init(aml_alsa_audio_init);
module_exit(aml_alsa_audio_exit);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AML audio driver for ALSA");
