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
/* #define DEBUG_ALSA_PLATFRORM */

#define ALSA_PRINT(fmt, args...)	pr_info("[aml-platform]" fmt, ##args)
#ifdef DEBUG_ALSA_PLATFRORM
#define ALSA_DEBUG(fmt, args...) pr_info("[aml-platform]" fmt, ##args)
#define ALSA_TRACE()	pr_info("[aml-platform] enter func %s,line %d\n",\
							__func__, __LINE__)
#else
#define ALSA_DEBUG(fmt, args...)
#define ALSA_TRACE()
#endif

unsigned long aml_i2s_playback_start_addr = 0;
EXPORT_SYMBOL(aml_i2s_playback_start_addr);

unsigned long aml_i2s_playback_phy_start_addr = 0;
EXPORT_SYMBOL(aml_i2s_playback_phy_start_addr);

unsigned long aml_i2s_alsa_write_addr = 0;
EXPORT_SYMBOL(aml_i2s_alsa_write_addr);

unsigned long aml_i2s_capture_phy_start_addr = 0;
unsigned long aml_i2s_capture_start_addr = 0;

static int trigger_underrun;
void aml_audio_hw_trigger(void)
{
	trigger_underrun = 1;
}
EXPORT_SYMBOL(aml_audio_hw_trigger);

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
	16384, 32768, 65536, 65536 * 2,	65536 * 4
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
		pr_info("alloc tmp buffer struct error\n");
		return -ENOMEM;
	}
	buf->private_data = tmp_buf;

	ALSA_TRACE();
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
			pr_info("alloc playback DMA buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		buf->bytes = size;
		/* malloc tmp buffer */
		size = aml_i2s_hardware.buffer_bytes_max;
		tmp_buf->buffer_start = kzalloc(size, GFP_KERNEL);
		if (tmp_buf->buffer_start == NULL) {
			pr_info("alloc playback tmp buffer error\n");
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
			pr_info("alloc capture DMA buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		buf->bytes = size;
		/* malloc tmp buffer */
		size = aml_i2s_capture.period_bytes_max;
		tmp_buf->buffer_start = kzalloc(size, GFP_KERNEL);
		if (tmp_buf->buffer_start == NULL) {
			pr_info("alloc capture tmp buffer error\n");
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
	ALSA_DEBUG("runtime dma_bytes %d,stream type %d\n", runtime->dma_bytes,
		   substream->stream);
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

	ALSA_TRACE();
	if (s && s->device_type == AML_AUDIO_I2SOUT && trigger_underrun) {
		pr_info("clear i2s out trigger underrun\n");
		trigger_underrun = 0;
	}
	return 0;
}

static void start_timer(struct aml_runtime_data *prtd)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (!prtd->active) {
		prtd->timer.expires = jiffies + 1;
		add_timer(&prtd->timer);
		prtd->active = 1;
		prtd->xrun_num = 0;
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);
}

static void stop_timer(struct aml_runtime_data *prtd)
{
	unsigned long flags;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (prtd->active) {
		del_timer(&prtd->timer);
		prtd->active = 0;
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);
}

static int aml_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct aml_runtime_data *prtd = rtd->private_data;
	int ret = 0;
	ALSA_TRACE();

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
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
	int elapsed = 0;
	unsigned int last_ptr, size = 0;

	spin_lock(&prtd->timer_lock);
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
		} else if (last_ptr == s->last_ptr) {
			if (prtd->xrun_num++ > 100) {
				pr_info("alsa capture long time no data, quit xrun!\n");
				prtd->xrun_num = 0;
				s->size = runtime->period_size;
			}
		} else {
			size = (last_ptr - (s->last_ptr)) / 2;
		}
		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(substream->runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			elapsed = 1;
		}
	}

	mod_timer(&prtd->timer, jiffies + 1);
	spin_unlock(&prtd->timer_lock);
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
	ALSA_TRACE();
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_set_runtime_hwparams(substream, &aml_i2s_hardware);
		if (s->device_type == AML_AUDIO_I2SOUT) {
			aml_i2s_playback_start_addr = (unsigned long)buf->area;
			aml_i2s_playback_phy_start_addr = buf->addr;
		}
	} else {
		snd_soc_set_runtime_hwparams(substream, &aml_i2s_capture);
		if (s->device_type == AML_AUDIO_I2SIN) {
			aml_i2s_capture_start_addr = (unsigned long)buf->area;
			aml_i2s_capture_phy_start_addr = buf->addr;
		}
	}

	/* ensure that peroid size is a multiple of 32bytes */
	ret =
	    snd_pcm_hw_constraint_list(runtime, 0,
				       SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				       &hw_constraints_period_sizes);
	if (ret < 0) {
		pr_info("set period bytes constraint error\n");
		goto out;
	}

	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_info("set period error\n");
		goto out;
	}
	if (!prtd) {
		prtd = kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			pr_info("alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}

	spin_lock_init(&prtd->timer_lock);
	init_timer(&prtd->timer);
	prtd->timer.function = &aml_i2s_timer_callback;
	prtd->timer.data = (unsigned long)substream;

 out:
	return ret;
}

static int aml_i2s_close(struct snd_pcm_substream *substream)
{
	struct aml_runtime_data *prtd = substream->runtime->private_data;
	ALSA_TRACE();

	kfree(prtd);
	prtd = NULL;
	substream->runtime->private_data = NULL;

	return 0;
}

/* extern void audio_out_i2s_enable(unsigned flag); */
/* extern void aml_hw_iec958_init(struct snd_pcm_substream *substream); */
/* extern void audio_hw_958_enable(unsigned flag); */
/* extern int kernel_android_50; */

static int aml_i2s_copy_playback(struct snd_pcm_runtime *runtime, int channel,
				 snd_pcm_uframes_t pos,
				 void __user *buf, snd_pcm_uframes_t count,
				 struct snd_pcm_substream *substream)
{
	int res = 0;
	int n;
	int i = 0, j = 0;
	int align = runtime->channels * 32 / runtime->byte_align;
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);
	struct aml_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buffer->private_data;
	void *ubuf = tmp_buf->buffer_start;
	struct audio_stream *s = &prtd->s;

	if (s->device_type == AML_AUDIO_I2SOUT)
		aml_i2s_alsa_write_addr = frames_to_bytes(runtime, pos);

	n = frames_to_bytes(runtime, count);
	if (n > tmp_buf->buffer_size) {
		pr_info("[%s]FATAL_ERR:UserData/%d > buffer_size/%d\n",
				__func__ , n, tmp_buf->buffer_size);
		return -EFAULT;
	}
	res = copy_from_user(ubuf, buf, n);
	if (res)
		return -EFAULT;
	if (access_ok(VERIFY_READ, buf, frames_to_bytes(runtime, count))) {
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {

			int16_t *tfrom, *to, *left, *right;
			tfrom = (int16_t *) ubuf;
			to = (int16_t *) hwbuf;

			left = to;
			right = to + 16;
			if (pos % align) {
				pr_info("audio data unligned: pos=%d, n=%d, align=%d\n",
				     (int)pos, n, align);
			}

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

			if (pos % align) {
				pr_info("audio data unaligned: pos=%d, n=%d, align=%d\n",
				     (int)pos, n, align);
			}
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

			if (pos % align) {
				pr_info("audio data unaligned: pos=%d, n=%d, align=%d\n",
				     (int)pos, n, align);
			}

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

static unsigned int aml_get_in_wr_ptr(void)
{
	return audio_in_i2s_wr_ptr() - aml_i2s_capture_phy_start_addr;
}

static int aml_i2s_copy_capture(struct snd_pcm_runtime *runtime, int channel,
				snd_pcm_uframes_t pos,
				void __user *buf, snd_pcm_uframes_t count,
				struct snd_pcm_substream *substream)
{
	unsigned int *tfrom, *left, *right;
	unsigned short *to;
	int res = 0, n = 0, i = 0, j = 0, size = 0;
	unsigned int t1, t2;
	unsigned char r_shift = 8;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos) * 2;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buffer->private_data;
	void *ubuf = tmp_buf->buffer_start;
	if (s->device_type == AML_AUDIO_I2SIN) {
		unsigned int buffersize =
		    (unsigned int)runtime->buffer_size * 8;
		unsigned int hw_ptr = aml_get_in_wr_ptr();
		unsigned int alsa_read_ptr = frames_to_bytes(runtime, pos) * 2;
		size = (buffersize + hw_ptr - alsa_read_ptr) % buffersize;
	}
	if (s->device_type == AML_AUDIO_SPDIFIN) {	/* spdif in */
		r_shift = 12;
	}
	to = (unsigned short *)ubuf;	/* tmp buf; */
	tfrom = (unsigned int *)hwbuf;	/* 32bit buffer */
	n = frames_to_bytes(runtime, count);
	if (size < 2 * n && s->device_type == AML_AUDIO_I2SIN) {
		pr_info("Alsa ptr is too close to HW ptr, Reset ALSA!\n");
		return -EPIPE;
	}
	if (access_ok(VERIFY_WRITE, buf, frames_to_bytes(runtime, count))) {
		if (runtime->channels == 2) {
			left = tfrom;
			right = tfrom + 8;
			if (pos % 8)
				pr_info("audio data unligned\n");

			if ((n * 2) % 64)
				pr_info("audio data unaligned 64 bytes\n");

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
	ALSA_TRACE();

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
	ALSA_TRACE();
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
		pr_debug("aml-i2s:" "Allocating i2s capture DMA buffer\n");
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
	ALSA_TRACE();
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
	pr_info("aml i2s suspend\n");

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
	pr_info("aml i2s resume\n");
	return 0;
}
#else
#define aml_i2s_suspend	NULL
#define aml_i2s_resume	NULL
#endif

struct snd_soc_platform_driver aml_soc_platform = {
	.ops = &aml_i2s_ops,
	.pcm_new = aml_i2s_new,
	.pcm_free = aml_i2s_free_dma_buffers,
	.suspend = aml_i2s_suspend,
	.resume = aml_i2s_resume,
};

static int aml_soc_platform_probe(struct platform_device *pdev)
{
	ALSA_TRACE();
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

static struct platform_driver aml_i2s_driver = {
	.driver = {
		   .name = "aml-i2s",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_audio_dt_match,
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
