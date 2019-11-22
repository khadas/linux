/*
 * drivers/amlogic/amlkaraoke/aml_usb_capture.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

/*
 *   A virtual path to get usb audio capture data.
 *
 */
#include <linux/slab.h>
#include <linux/bug.h>
#include <sound/core.h>

#include <linux/amlogic/media/sound/usb_karaoke.h>

#include "aml_usb_capture.h"
#include "aml_reverb.h"

/*void *snd_usb_pcm_capture_buffer = NULL;*/
struct usb_audio_buffer *snd_usb_pcm_capture_buffer;
EXPORT_SYMBOL(snd_usb_pcm_capture_buffer);

/* delay time for no audio effect to avoid noise. */
int usb_ignore_effect_time;

struct usb_input *s_usb_capture;
struct reverb_t aml_reverb_l;
struct reverb_t aml_reverb_r;

/* i2s info */
static unsigned int aml_i2s_playback_channels;
static unsigned int aml_i2s_playback_sample;

void aml_i2s_set_ch_r_info(unsigned int channels, unsigned int samplerate)
{
	aml_i2s_playback_sample = samplerate;
	aml_i2s_playback_channels = channels;

	pr_info("[%s]:[%d] samplerate:%d, channels:%d\n",
		__func__, __LINE__, samplerate, channels);

	/* limit resample usb in to 2 channels */
	if (aml_i2s_playback_channels > 2)
		aml_i2s_playback_channels = 2;
}

static inline short clip(int x)
{
	if (x < -32768)
		x = -32768;
	else if (x > 32767)
		x =  32767;
	return x & 0xFFFF;
}

struct usb_input *usb_audio_get_capture(void)
{
	return s_usb_capture;
}

static void usb_audio_data_tuning_mic_gain(unsigned char *audiobuf,
					   int frames)
{
	s16 *tuningbuf = (s16 *)audiobuf;
	int i;

	WARN_ON(!audiobuf);
	for (i = 0; i < frames; i++) {
		*tuningbuf = ((*tuningbuf) * (usb_mic_digital_gain)) >> 8;
		*tuningbuf = clip(*tuningbuf);
		tuningbuf += 1;
	}
}

static int usb_audio_mono2stereo(unsigned char *dst,
				 unsigned char *src, int frames)
{
	int j;
	s16 *transfer_src = (s16 *)src;
	s16 *transfer_dst = (s16 *)dst;

	for (j = 0; j < frames; j++) {
		transfer_dst[2 * j] = transfer_src[j];
		transfer_dst[2 * j + 1] = transfer_src[j];
	}

	return 0;
}

static int usb_resample_and_mono2stereo_malloc_buffer(
	struct usb_input *usbinput,
	unsigned int out_rate)
{
	if (!usbinput->out_buffer &&
	    !usbinput->usb_buf &&
	    !usbinput->usb_buf->rate) {
		pr_info("usb resample buffer alloc failed\n");
		return -ENOMEM;
	}
	return 0;
}

static int usb_resample_and_mono2stereo_free_buffer(
	struct usb_input *usbinput)
{
	if (usbinput &&
	    (usbinput->out_buffer || usbinput->mono2stereo)) {
		kfree(usbinput->out_buffer);
		kfree(usbinput->mono2stereo);
	}

	return 0;
}

static int usb_audio_capture_malloc_buffer(
	struct usb_audio_buffer *usb_audio, int size)
{
	usb_audio->addr = (unsigned char *)
		kzalloc(size, GFP_KERNEL);
	if (!usb_audio->addr) {
		pr_info("USB audio capture buffer alloc failed\n");
		return -ENOMEM;
	}
	usb_audio->size = size;
	return 0;
}

static int usb_audio_capture_free_buffer(
	struct usb_audio_buffer *usb_audio)
{
	if (usb_audio && usb_audio->addr) {
		kfree(usb_audio->addr);
		usb_audio->addr = NULL;
	}
	return 0;
}

static int usb_check_resample_and_mono2stereo_buffer(
	struct usb_input *usbinput, unsigned int rate)
{
	int ret = -1;

	if (rate &&
	    ((!usbinput->out_buffer) || (!usbinput->mono2stereo))) {
		usb_resample_and_mono2stereo_malloc_buffer(
			usbinput,
			rate);

		ret = 0;
	}

	return ret;
}

static int usb_check_and_do_mono2stereo(struct usb_input *usbinput,
					unsigned int channels,
					unsigned char *cp,
					unsigned int frames)

{
	int ret = -1;
	struct usb_audio_buffer *usb_buf = usbinput->usb_buf;

	if (!usb_buf) {
		pr_info("check and do mono2stereo, invalid usb buffer\n");
		return ret;
	}

	if (channels && channels != usb_buf->channels &&
	    usbinput->mono2stereo) {
		usb_audio_mono2stereo(
			usbinput->mono2stereo,
			cp,
			frames);
		ret = 0;
	}
	return ret;
}

static void usb_check_resample_init(struct usb_input *usbinput,
				    unsigned int rate,
				    unsigned int channels)
{
	struct usb_audio_buffer *usb_buf = usbinput->usb_buf;

	if (!usb_buf) {
		pr_info("check mono2stereo init, invalid usb buffer\n");
		return;
	}

	if (!usbinput->resample_request &&
	    usb_buf->rate && rate && channels &&
	    rate != usb_buf->rate) {
		usbinput->resample_request = true;

		usbinput->resampler.input_sr = usb_buf->rate;
		usbinput->resampler.output_sr = rate;
		usbinput->resampler.channels = channels;
		resampler_init(&usbinput->resampler);
		pr_info("check mono2stereo init, resample from %d ch, %d to %d ch, %d",
			usb_buf->channels,
			usb_buf->rate,
			channels,
			rate);
	}
}

static int usb_check_and_do_resample(struct usb_input *usbinput,
				     unsigned int channels,
				     unsigned char *cp,
				     unsigned int in_frames)
{
	struct usb_audio_buffer *usb_buf = usbinput->usb_buf;
	void *in_buffer;
	unsigned int out_frames;

	if (!usb_buf) {
		pr_info("check and do resample, invalid usb buffer\n");
		return -1;
	}

	if ((usb_buf->channels == 1) &&
	    (channels != usb_buf->channels) &&
	    usbinput->mono2stereo) {
		in_buffer = usbinput->mono2stereo;
		/* in_frames <<= 1; */
	} else {
		/* in_frames = bytes >> subs->
		 * usb_input->resampler.channels;
		 */
		in_buffer = cp;
	}

	out_frames = resample_process(
			&usbinput->resampler,
			in_frames,
			(short *)(in_buffer),
			(short *)usbinput->out_buffer);

	return out_frames;
}

static void usb_check_and_do_reverb(struct reverb_t *reverb_L,
				    struct reverb_t *reverb_R,
				    unsigned int channels,
				    unsigned char *cp,
				    unsigned int frames)
{
	if (channels == 1) {
		reverb_process(reverb_L, (s16 *)cp, (s16 *)cp,
			       frames, channels, 0);
	} else {
		reverb_process(reverb_L, (s16 *)cp, (s16 *)cp,
			       frames, channels, 0);
		reverb_process(reverb_R, (s16 *)cp + 1,
			       (s16 *)cp + 1, frames,
			       channels, 1);
	}
}

static void usb_audio_copy_ringbuffer(struct usb_audio_buffer *usb_buf,
				      unsigned char *cp_src,
				      unsigned int cp_bytes)
{
	uint avail;
	unsigned long usbirqflags;

	spin_lock_irqsave(&usb_buf->lock, usbirqflags);

	if (usb_buf->wr >= usb_buf->rd)
		avail = usb_buf->rd + usb_buf->size - usb_buf->wr;
	else
		avail = usb_buf->rd - usb_buf->wr;

	if (avail >= cp_bytes) {
		if (usb_buf->wr + cp_bytes > usb_buf->size) {
			memcpy(usb_buf->addr + usb_buf->wr,
			       cp_src, usb_buf->size - usb_buf->wr);
			memcpy(usb_buf->addr,
			       cp_src + usb_buf->size - usb_buf->wr,
			       cp_bytes + usb_buf->wr - usb_buf->size);
		} else {
			memcpy(usb_buf->addr + usb_buf->wr,
			       cp_src, cp_bytes);
		}

		usb_buf->wr = (usb_buf->wr + cp_bytes) %
			usb_buf->size;
		usb_buf->level = (usb_buf->size + usb_buf->wr
			- usb_buf->rd) % usb_buf->size;
	} else {
		/*reset buffer ptr*/
		usb_buf->wr = (usb_buf->rd + usb_buf->size / 2) % usb_buf->size;
	}
	spin_unlock_irqrestore(&usb_buf->lock, usbirqflags);
}

int usb_set_capture_status(bool isrunning)
{
	struct usb_input *usbinput = usb_audio_get_capture();

	if (!usbinput)
		return -1;
	if (!usbinput->usb_buf)
		return -2;

	usbinput->usb_buf->running = isrunning;

	return 0;
}
EXPORT_SYMBOL(usb_set_capture_status);

int usb_audio_capture_init(void)
{
	struct usb_audio_buffer *usb_buffer = NULL;
	struct usb_input *s_usb_input;
	int ret = 0;
	unsigned int buffer_size = 0;

	s_usb_input = kzalloc(sizeof(*s_usb_input), GFP_KERNEL);
	if (!s_usb_input) {
		ret = -ENOMEM;
		goto err;
	}
	usb_buffer = kzalloc(sizeof(*usb_buffer), GFP_KERNEL);
	if (!usb_buffer) {
		ret = -ENOMEM;
		goto err;
	}
	s_usb_input->usb_buf = usb_buffer;

	snd_usb_pcm_capture_buffer = usb_buffer;

	if (usb_audio_capture_malloc_buffer(
		usb_buffer,
		USB_AUDIO_CAPTURE_BUFFER_SIZE)) {
		ret = -ENOMEM;
		goto err;
	}
	buffer_size = (USB_AUDIO_CAPTURE_PACKAGE_SIZE  * (48000
				/ 8000) + 1) * 4;

	if (!s_usb_input->out_buffer) {
		s_usb_input->out_buffer = (unsigned char *)
			kzalloc(buffer_size, GFP_KERNEL);
		if (!s_usb_input->out_buffer) {
			ret = -ENOMEM;
			goto err;
		}
	}

	s_usb_input->mono2stereo = (unsigned char *)
		kzalloc(buffer_size * 2, GFP_KERNEL);
	if (!s_usb_input->mono2stereo) {
		ret = -ENOMEM;
		goto err;
	}

	/*snd_usb_pcm_capture_buffer->addr = usb_buffer->addr;*/
	spin_lock_init(&usb_buffer->lock);
	s_usb_capture = s_usb_input;

	reverb_start(&aml_reverb_l);
	reverb_start(&aml_reverb_r);

	/* ignore some usb data */
	usb_ignore_effect_time = 3;

err:
	if (!s_usb_input) {
		if (!s_usb_input->out_buffer)
			kfree(s_usb_input->out_buffer);
		kfree(s_usb_input);
	}
	if (!usb_buffer) {
		usb_audio_capture_free_buffer(usb_buffer);
		kfree(usb_buffer);
	}
	return ret;
}
EXPORT_SYMBOL(usb_audio_capture_init);

int usb_audio_capture_deinit(void)
{
	struct usb_input *usbinput = usb_audio_get_capture();

	if (!usbinput)
		return 0;

	if (snd_usb_pcm_capture_buffer) {
		usb_audio_capture_free_buffer(usbinput->usb_buf);
		kfree(usbinput->usb_buf);
		usbinput->usb_buf = NULL;
		snd_usb_pcm_capture_buffer = NULL;
	}

	usb_resample_and_mono2stereo_free_buffer(usbinput);
	kfree(usbinput);

	reverb_stop(&aml_reverb_l);
	reverb_stop(&aml_reverb_r);

	return 0;
}
EXPORT_SYMBOL(usb_audio_capture_deinit);

int retire_capture_usb(struct snd_pcm_runtime *runtime,
		       unsigned char *cp, unsigned int bytes,
		       unsigned int oldptr, unsigned int stride)
{
	struct usb_input *usbinput = usb_audio_get_capture();
	struct usb_audio_buffer *usb_buf = NULL;
	unsigned char *cp_src = NULL;
	unsigned char *effect_cp = NULL;
	unsigned int effect_bytes = 0;
	unsigned int frame_count = 0;
	unsigned int cp_bytes = 0;
	unsigned int in_frames = 0, out_frames = 0;
	int ret = 0;

	if ((builtin_mixer && usb_ignore_effect_time) || (!builtin_mixer)) {
		/* At the beginning when usb capture start,
		 * noise is captured in the first audio data,
		 * so deley 3ms for no audio effect.
		 */
		if (usb_ignore_effect_time)
			usb_ignore_effect_time--;

		/* copy a data chunk */
		if (oldptr + bytes > runtime->buffer_size * stride) {
			unsigned int bytes1 =
			runtime->buffer_size * stride - oldptr;
			memcpy(runtime->dma_area + oldptr, cp, bytes1);
			memcpy(runtime->dma_area,
			       cp + bytes1, bytes - bytes1);
		} else {
			memcpy(runtime->dma_area + oldptr, cp, bytes);
		}

		goto exit;
	}

	effect_cp = cp;
	effect_bytes = bytes;
	frame_count =  bytes_to_frames(runtime, effect_bytes);

	/* check data is valid */
	if (NULL == effect_cp || 0 == effect_bytes) {
		/* pr_info("retire usb, invalid data,cp:%p, bytes:%d\n",
		 * cp, bytes);
		 */
		ret = -1;
		goto exit;
	}

	/* Audio effect reverb */
	if (reverb_enable) {
		usb_check_and_do_reverb(
			&aml_reverb_l, &aml_reverb_r,
			runtime->channels,
			effect_cp, frame_count);
	}

	/* Tuning usb mic gain */
	usb_audio_data_tuning_mic_gain(effect_cp,
				       frame_count);

	/* copy a data chunk */
	if (oldptr + effect_bytes >
		runtime->buffer_size * stride) {
		unsigned int bytes1 =
			runtime->buffer_size * stride - oldptr;
		memcpy(runtime->dma_area + oldptr,
		       effect_cp, bytes1);
		memcpy(runtime->dma_area,
		       effect_cp + bytes1, effect_bytes - bytes1);
	} else {
		memcpy(runtime->dma_area + oldptr,
		       effect_cp, effect_bytes);
	}

	/* Audio reverb effect is added to the source,
	 * countine to do resample/mono2stereo, then copy
	 * audio data to ring buffer.
	 */
	usb_buf = usbinput->usb_buf;
	usb_buf->channels = runtime->channels;
	usb_buf->rate = runtime->rate;

	/*usb resample and mono2stereo buffer prepared*/
	usb_check_resample_and_mono2stereo_buffer(
		usbinput, aml_i2s_playback_sample);

	/*mono to stereo, default that i2s channel is stereo.*/
	in_frames = frame_count;
	usb_check_and_do_mono2stereo(
		usbinput, aml_i2s_playback_channels,
		cp, in_frames);

	/* Resample Init */
	usb_check_resample_init(usbinput, aml_i2s_playback_sample,
				aml_i2s_playback_channels);

	if ((bytes) && (usbinput->resample_request) &&
	    (usbinput->out_buffer)) {
		/* bytes to frame, bytes / (channel * bytes_per_frame),
		 * default:16bit
		 */
		out_frames = usb_check_and_do_resample(
			usbinput,
			aml_i2s_playback_channels,
			cp, in_frames);
		if (out_frames < 0) {
			pr_info("do reample failed\n");
			goto exit;
		}
		cp_src = usbinput->out_buffer;
		cp_bytes = out_frames * 4;
	} else {
		/*snd_printdd(KERN_ERR "usb record not resample\n");*/
		if (usb_buf->channels == 1 && usbinput->mono2stereo &&
		    aml_i2s_playback_channels &&
		    aml_i2s_playback_channels != usb_buf->channels) {
			cp_src = usbinput->mono2stereo;
			cp_bytes = in_frames * 4;
		} else {
			cp_src = cp;
			cp_bytes = bytes;
		}
	}

	/* copy audio audio to ring buffer.
	 * Default, ring buffer, stereo channel, 16bit
	 */
	usb_audio_copy_ringbuffer(usb_buf, cp_src, cp_bytes);

exit:
	return ret;
}
EXPORT_SYMBOL(retire_capture_usb);
