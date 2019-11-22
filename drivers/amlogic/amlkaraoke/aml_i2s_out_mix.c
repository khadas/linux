/*
 * drivers/amlogic/amlkaraoke/aml_i2s_out_mix.c
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
 *   A virtual path to get usb audio capture data to i2s mixed out.
 */
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/usb_karaoke.h>

#include "aml_i2s_out_mix.h"

#define BASE_IRQ           (32)
#define AM_IRQ(reg)        ((reg) + BASE_IRQ)
#define INT_I2S_DDR        AM_IRQ(48)
#define IRQ_OUT            INT_I2S_DDR
#define I2S_INT_NUM        (16) /* min 2, max 32 */
#define I2S_BLOCK_SIZE     (64) /* block_size=32byte*channel_num, normal is 2*/
#define I2S_INT_BLOCK      ((I2S_INT_NUM) * (I2S_BLOCK_SIZE))
#define MIN_LATENCY        (64 * 32)

static int i2s_block_size;
static int speaker_channel_mask = 1;

struct i2s_info {
	/* hw info */
	unsigned int channels;
	unsigned int format;

	/* for mixer */
	unsigned int int_num;
	unsigned int i2s_block;
	unsigned int int_block;
	unsigned int latency;
};

/*Ultimate output from i2s */
struct i2s_output {
	/* audio info */
	i2s_audio_buffer i2s_buf;

	/* work queue */
	struct work_struct work;

	/* is irq registered ? */
	bool isInit;

	/*i2s info for mixer*/
	struct i2s_info i2sinfo;
};

typedef int (*mixer_f)(char *dst, char *src, unsigned int count,
	unsigned int channels, unsigned int format);

static struct i2s_output s_i2s_output;

static inline s16 clip16(int x)
{
	if (x < -32768)
		return -32768;
	else if (x > 32767)
		return 32767;

	return (s16)x;
}

static inline s32 clip32(long x)
{
	if (x < -2147483647)
		return -2147483647;	//default:-2147483648
	else if (x > 2147483647)
		return 2147483647;

	return (s32)x;
}

/* Average weight to mix */
static inline s16 aweight_mix16(s16 s1, s16 s2)
{
	return clip16(((s32)s1 + (s32)s2) >> 1);
}

static inline s32 aweight_mix32(s32 s1, s32 s2)
{
	return clip32(((int64_t)s1 + (int64_t)s2) >> 1);
}

/* Can expand to support more mixer method */
static inline s16 mix16(s16 s1, s16 s2)
{
	return aweight_mix16(s1, s2);
}

static inline s32 mix32(s32 s1, s32 s2)
{
	return aweight_mix32(s1, s2);
}

/* Get usb audio info. */
struct usb_audio_buffer *usb_get_audio_info(void)
{
	return (struct usb_audio_buffer *)snd_usb_pcm_capture_buffer;
}

/* Get i2s audio info. */
i2s_audio_buffer *i2s_get_audio_info(void)
{
	return (i2s_audio_buffer *)&s_i2s_output.i2s_buf;
}

/* Get i2s output. */
static struct i2s_output *i2s_get_output(void)
{
	return (struct i2s_output *)&s_i2s_output;
}

/* i2s get memory size */
static unsigned int i2s_get_out_size(void)
{
	return aml_read_cbus(AIU_MEM_I2S_END_PTR) -
		aml_read_cbus(AIU_MEM_I2S_START_PTR) + i2s_block_size;
}

/* i2s get read pointer */
static unsigned int i2s_get_out_read_ptr(void)
{
	return aml_read_cbus(AIU_MEM_I2S_RD_PTR) -
		aml_read_cbus(AIU_MEM_I2S_START_PTR);
}

static int mixer_transfer(
		char *dst, char *src,
		unsigned int count,
		unsigned int i2s_channels,
		unsigned int i2s_format)
{
	int i;

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	int i2s_frame, i2s_frames_count;

	/* bytes in one frame */
	i2s_frame = i2s_channels * (i2s_format / 8);
	/* frames in on block */
	i2s_frames_count = count / i2s_frame;

	if (i2s_channels == 2) {
		if (i2s_format == 16) {
			s16 *to = (s16 *)dst;
			s16 *tfrom = (s16 *)src;

			for (i = 0; i < i2s_frames_count * i2s_channels;
				i++, to++)
				*to = mix16(*to, *tfrom++);
		} else if (i2s_format == 32 ||
				i2s_format == 24) {
			s32 *to = (s32 *)dst;
			s16 *tfrom = (s16 *)src;

			for (i = 0; i < i2s_frames_count * i2s_channels;
				i++, to++)
				*to = mix32(*to, (s32)(*tfrom++) << 16);
		} else {
			pr_err("Error: Unsupport format:%d\n", i2s_format);
		}
	} else if (i2s_channels == 8) {
		if (i2s_format == 16) {
			s16 *to = (s16 *)dst;
			s16 *tfrom = (s16 *)src;
			s16 *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

			/* yet only cpu txhd support split 8ch, 16bit */
			lf  = to + 0;
			cf  = to + 1;
			rf  = to + 2;
			ls  = to + 3;
			rs  = to + 4;
			lef = to + 5;
			sbl = to + 6;
			sbr = to + 7;

			for (i = 0; i < i2s_frames_count; i++) {
				if (speaker_channel_mask == 0) {
					(*lf) = mix16(*lf, (*tfrom++));
					(*cf) = mix16(*cf, (*tfrom++));
				}
				if (speaker_channel_mask == 1) {
					(*rf) = mix16(*rf, (*tfrom++));
					(*ls) = mix16(*ls, (*tfrom++));
				}
				if (speaker_channel_mask == 2) {
					(*rs) = mix16(*rs, (*tfrom++));
					(*lef) = mix16(*lef, (*tfrom++));
				}
				if (speaker_channel_mask == 3) {
					(*sbl) = mix16(*sbl, (*tfrom++));
					(*sbr) = mix16(*sbr, (*tfrom++));
				}
				lf  += 8;
				cf  += 8;
				rf  += 8;
				ls  += 8;
				rs  += 8;
				lef += 8;
				sbl += 8;
				sbr += 8;
			}
		} else if (i2s_format == 32 ||
				i2s_format == 24) {
			s32 *to = (s32 *)dst;
			s16 *tfrom = (s16 *)src;
			s32 *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

			lf  = to + 0;
			cf  = to + 1;
			rf  = to + 2;
			ls  = to + 3;
			rs  = to + 4;
			lef = to + 5;
			sbl = to + 6;
			sbr = to + 7;

			for (i = 0; i < i2s_frames_count; i++) {
				if (speaker_channel_mask == 0) {
					(*lf) = mix32(*lf, (*tfrom++) << 16);
					(*cf) = mix32(*cf, (*tfrom++) << 16);
				}
				if (speaker_channel_mask == 1) {
					(*rf) = mix32(*rf, (*tfrom++) << 16);
					(*ls) = mix32(*ls, (*tfrom++) << 16);
				}
				if (speaker_channel_mask == 2) {
					(*rs) = mix32(*rs, (*tfrom++) << 16);
					(*lef) = mix32(*lef, (*tfrom++) << 16);
				}

				if (speaker_channel_mask == 3) {
					(*sbl) = mix32(*sbl, (*tfrom++) << 16);
					(*sbr) = mix32(*sbr, (*tfrom++) << 16);
				}
				lf  += 8;
				cf  += 8;
				rf  += 8;
				ls  += 8;
				rs  += 8;
				lef += 8;
				sbl += 8;
				sbr += 8;
			}
		} else {
			pr_err("Error: Unsupport format:%d\n", i2s_format);
		}
	} else {
		pr_err("Error: Unsupport channels:%d\n", i2s_channels);
	}
#else
	int j;

	if (i2s_channels == 2) {
		if (i2s_format == 16) {
			s16 *to = (s16 *)(dst);
			s16 *tfrom = (s16 *)(src);
			s16 *lf, *rf;

			lf = to;
			rf = lf + 16;
			for (i = 0; i < count; i += 64) {
				for (j = 0; j < 16; j++) {
					(*lf++) = mix16(*lf, *tfrom++);
					(*rf++) = mix16(*rf, *tfrom++);
				}
				lf += 16;
				rf += 16;
			}
		} else if (i2s_format == 32 ||
				i2s_format == 24) {
			s32 *to = (s32 *)dst;
			s16 *tfrom = (s16 *)src;
			s32 *lf, *rf;
			s32 sample;

			lf = to;
			rf = to + 8;
			for (i = 0; i < count; i += 64) {
				for (j = 0; j < 8; j++) {
					(*lf++) = mix32(
						*lf, ((s32)(*tfrom++)) << 8);
					(*rf++) = mix32(
						*rf, ((s32)(*tfrom++)) << 8);
				}
				lf += 8;
				rf += 8;
			}
		} else {
			pr_err("Error: Unsupport format:%d\n", i2s_format);
		}
	} else if (i2s_channels == 8) {
		if (i2s_format == 16) {
			s16 *to = (s16 *)(dst);
			s16 *tfrom = (s16 *)(src);
			s16 *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

			lf  = to + 0 * 16;
			cf  = to + 1 * 16;
			rf  = to + 2 * 16;
			ls  = to + 3 * 16;
			rs  = to + 4 * 16;
			lef = to + 5 * 16;
			sbl = to + 6 * 16;
			sbr = to + 7 * 16;
			for (j = 0; j < count; j += 256) {
				for (i = 0; i < 16; i++) {
					if (speaker_channel_mask == 0) {
						(*lf++) = mix16(
							*lf, (*tfrom++));
						(*cf++) = mix16(
							*cf, (*tfrom++));
					} else {
						lf++;
						cf++;
					}
					if (speaker_channel_mask == 1) {
						(*rf++) = mix16(
							*rf, (*tfrom++));
						(*ls++) = mix16(
							*ls, (*tfrom++));
					} else {
						rf++;
						ls++;
					}
					if (speaker_channel_mask == 2) {
						(*rs++) = mix16(
							*rs, (*tfrom++));
						(*lef++) = mix16(
							*lef, (*tfrom++));
					} else {
						rs++;
						lef++;
					}
					if (speaker_channel_mask == 3) {
						(*sbl++) = mix16(
							*sbl, (*tfrom++));
						(*sbr++) = mix16(
							*sbr, (*tfrom++));
					} else {
						sbl++;
						sbr++;
					}
				}
				lf  += 7 * 16;
				cf  += 7 * 16;
				rf  += 7 * 16;
				ls  += 7 * 16;
				rs  += 7 * 16;
				lef += 7 * 16;
				sbl += 7 * 16;
				sbr += 7 * 16;
			}

		} else if (i2s_format == 32 ||
				i2s_format == 24) {
			s32 *to = (s32 *)(dst);
			s16 *tfrom = (s16 *)(src);
			s32 *lf, *cf, *rf, *ls, *rs, *lef, *sbl, *sbr;

			lf  = to + 0 * 8;
			cf  = to + 1 * 8;
			rf  = to + 2 * 8;
			ls  = to + 3 * 8;
			rs  = to + 4 * 8;
			lef = to + 5 * 8;
			sbl = to + 6 * 8;
			sbr = to + 7 * 8;
			for (j = 0; j < count; j += 256) {
				for (i = 0; i < 8; i++) {
					if (speaker_channel_mask == 0) {
						(*lf++) = mix32(
							*lf,
							((s32)(*tfrom++)) << 8);
						(*cf++) = mix32(
							*cf,
							((s32)(*tfrom++)) << 8);
					} else {
						lf++;
						cf++;
					}
					if (speaker_channel_mask == 1) {
						(*rf++) = mix32(
							*rf,
							((s32)(*tfrom++)) << 8);
						(*ls++) = mix32(
							*ls,
							((s32)(*tfrom++)) << 8);
					} else {
						rf++;
						ls++;
					}
					if (speaker_channel_mask == 2) {
						(*rs++) = mix32(
							*rs,
							((s32)(*tfrom++)) << 8);
						(*lef++) = mix32(
							*lef,
							((s32)(*tfrom++)) << 8);
					} else {
						rs++;
						lef++;
					}
					if (speaker_channel_mask == 3) {
						(*sbl++) = mix32(
							*sbl,
							((s32)(*tfrom++)) << 8);
						(*sbr++) = mix32(
							*sbr,
							((s32)(*tfrom++)) << 8);
					} else {
						sbl++;
						sbr++;
					}
				}
				lf  += 7 * 8;
				cf  += 7 * 8;
				rf  += 7 * 8;
				ls  += 7 * 8;
				rs  += 7 * 8;
				lef += 7 * 8;
				sbl += 7 * 8;
				sbr += 7 * 8;
			}
		} else {
			pr_err("Error: Unsupport format:%d\n", i2s_format);
		}
	} else {
		pr_err("Error: Unsupport channels:%d\n", i2s_channels);
	}
#endif

	return 0;
}

/*mix i2s memory audio data with usb audio record in,output stereo to i2s*/
static void i2s_out_mix(
	i2s_audio_buffer *i2s_audio,
	struct usb_audio_buffer *usb_audio,
	struct i2s_info *p_i2sinfo,
	mixer_f mixer)
{
	i2s_audio_buffer *i2sbuf = i2s_audio;
	struct usb_audio_buffer *usbbuf = usb_audio;
	unsigned int i2s_out_ptr = i2s_get_out_read_ptr();
	unsigned int alsa_delay = (aml_i2s_alsa_write_addr +
		i2sbuf->size - i2s_out_ptr) % i2sbuf->size;
	unsigned int i2s_mix_delay = (i2sbuf->wr +
		i2sbuf->size - i2s_out_ptr) % i2sbuf->size;
	unsigned long i2sirqflags, usbirqflags;
	unsigned int mix_count = 0, mix_usb_count;
	int avail = 0;
	int i2s_frame, usb_frame, i2s_frames_count, usb_frames_count;

	i2s_frame = p_i2sinfo->channels * (p_i2sinfo->format / 8);
	usb_frame = 4; /* usb in: 2ch, 16bit */

	spin_lock_irqsave(&i2sbuf->lock, i2sirqflags);

	i2sbuf->rd = i2s_out_ptr;
	/*(i2sbuf->size + i2sbuf->wr - i2s_out_ptr) % i2sbuf->size;*/
	i2sbuf->level = i2s_mix_delay;

	mix_count = p_i2sinfo->int_block;
	i2s_frames_count = mix_count / i2s_frame;

	/*update*/
	usb_frames_count = i2s_frames_count;
	mix_usb_count = usb_frames_count * usb_frame;

	if (i2sbuf->level <= p_i2sinfo->int_block ||
	    (alsa_delay - i2s_mix_delay) < p_i2sinfo->int_block) {
		i2sbuf->wr = (i2sbuf->rd + p_i2sinfo->latency) % i2sbuf->size;
		i2sbuf->wr /= p_i2sinfo->int_block;
		i2sbuf->wr *= p_i2sinfo->int_block;
		i2sbuf->level = p_i2sinfo->latency;
		goto EXIT;
	}

	if (i2sbuf->wr % p_i2sinfo->int_block) {
		i2sbuf->wr /= p_i2sinfo->int_block;
		i2sbuf->wr *= p_i2sinfo->int_block;
	}

	if (usbbuf->wr >= usbbuf->rd)
		avail = usbbuf->wr - usbbuf->rd;
	else
		avail = usbbuf->wr + usbbuf->size - usbbuf->rd;

	if (avail < mix_usb_count) {
		/*
		 * pr_info("i2sOUT buffer underrun\n");
		 * goto EXIT;
		 */

		/*fill zero data*/
		memset(usbbuf->addr + (usbbuf->rd + avail) % usbbuf->size,
		       0,
		       mix_usb_count - avail);
	}

	spin_lock_irqsave(&usbbuf->lock, usbirqflags);

	mixer(i2sbuf->addr + i2sbuf->wr,
	      usbbuf->addr + usbbuf->rd,
	      mix_count,
	      p_i2sinfo->channels,
	      p_i2sinfo->format);

	i2sbuf->wr = (i2sbuf->wr + mix_count) % i2sbuf->size;
	i2sbuf->level = (i2sbuf->size + i2sbuf->wr - i2sbuf->rd) % i2sbuf->size;

	usbbuf->rd = (usbbuf->rd + mix_usb_count) % usbbuf->size;

	spin_unlock_irqrestore(&usbbuf->lock, usbirqflags);

EXIT:
	spin_unlock_irqrestore(&i2sbuf->lock, i2sirqflags);
}

/* IRQ handler */
static irqreturn_t i2s_out_mix_callback(int irq, void *data)
{
	struct i2s_output *p_i2s_out = (struct i2s_output *)data;
	i2s_audio_buffer *i2s_buf = (i2s_audio_buffer *)&p_i2s_out->i2s_buf;
	unsigned int i2s_size = i2s_get_out_size();
	struct usb_audio_buffer *usbbuf =
		(struct usb_audio_buffer *)usb_get_audio_info();
	//struct i2s_info *p_i2sinfo = &p_i2s_out->i2sinfo;

	/*check whether usb audio record start*/
	if (!usbbuf || !usbbuf->addr || !usbbuf->running)
		return IRQ_HANDLED;

	/*update i2s buffer informaiton if needed.*/
	if (i2s_size != i2s_buf->size) {
		i2s_buf->size = i2s_size;
		i2s_buf->addr = (unsigned char *)aml_i2s_playback_start_addr;
		i2s_buf->paddr = aml_i2s_playback_phy_start_addr;
		i2s_buf->rd = i2s_get_out_read_ptr();
	}

	schedule_work(&p_i2s_out->work);
	//i2s_out_mix(i2s_buf, usbbuf, p_i2sinfo, mixer_transfer);

	return IRQ_HANDLED;
}

/* Work Queue handler */
static void i2s_out_mix_work_handler(struct work_struct *data)
{
	struct i2s_output *p_i2s_out = i2s_get_output();
	i2s_audio_buffer *i2sbuf =
		(i2s_audio_buffer *)i2s_get_audio_info();
	struct usb_audio_buffer *usbbuf =
		(struct usb_audio_buffer *)usb_get_audio_info();
	struct i2s_info *p_i2sinfo = &p_i2s_out->i2sinfo;

	if (!i2sbuf || !usbbuf || !p_i2sinfo->channels)
		return;

	/* mixer: usb info, 2 channels, 16 bits;
	 *    i2s info, 2/8 channels, 16/32 bits
	 * case 1: 2 channel mixer with 2 channel i2s
	 * case 2: 2 channel mixer with 8 channel i2s,
	 *    to special channels according to speaker_channel_mask
	 */
	i2s_out_mix(i2sbuf, usbbuf, p_i2sinfo, mixer_transfer);
}

int i2s_out_mix_init(void)
{
	int ret = 0;

	if (!builtin_mixer) {
		pr_info("Not to mix usb in and i2s out\n");
		return 0;
	}

	memset((void *)&s_i2s_output, 0, sizeof(struct i2s_output));
	/* init i2s audio buffer */
	spin_lock_init(&s_i2s_output.i2s_buf.lock);
	s_i2s_output.i2s_buf.addr =
		(unsigned char *)aml_i2s_playback_start_addr;
	s_i2s_output.i2s_buf.paddr =
		aml_i2s_playback_phy_start_addr;
	s_i2s_output.i2s_buf.size =
	i2s_get_out_size();
	s_i2s_output.i2s_buf.rd =
		i2s_get_out_read_ptr();

	/*defalut for 2ch, 16bit,*/
	i2s_block_size = I2S_BLOCK_SIZE;
	s_i2s_output.i2sinfo.int_num   = I2S_INT_NUM;
	s_i2s_output.i2sinfo.i2s_block = i2s_block_size;
	s_i2s_output.i2sinfo.int_block = I2S_INT_BLOCK;
	s_i2s_output.i2sinfo.latency   = MIN_LATENCY * 2;
	s_i2s_output.i2sinfo.channels  = aml_i2s_playback_channel;
	s_i2s_output.i2sinfo.format    = aml_i2s_playback_format;
	if (s_i2s_output.i2sinfo.channels == 8) {
		i2s_block_size *= 4;
		s_i2s_output.i2sinfo.int_num   *= 2;
		s_i2s_output.i2sinfo.i2s_block = i2s_block_size;
		s_i2s_output.i2sinfo.int_block = s_i2s_output.i2sinfo.int_num
			* s_i2s_output.i2sinfo.i2s_block;
		s_i2s_output.i2sinfo.latency   *= 8;
	}

	pr_info("%s:%d, channels:%d, format:%d, i2s_block_size:%d\n",
		__func__, __LINE__,
		s_i2s_output.i2sinfo.channels,
		s_i2s_output.i2sinfo.format,
		i2s_block_size);

	if (!s_i2s_output.isInit) {
		s_i2s_output.isInit = true;

		/*register irq*/
		if (request_irq(
				irq_karaoke, i2s_out_mix_callback,
				IRQF_SHARED, "i2s_out_mix",
				&s_i2s_output)) {
			ret = -EINVAL;
		}
		pr_info("register irq\n");
	}
	/*irq block*/
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
	/* TODO: split mode aiu_mem_i2s_mask[15:0] must set 8'hffff_ffff. */
	aml_cbus_update_bits(AIU_MEM_I2S_MASKS, 0xffff << 16, 4 << 16);
#else
	aml_cbus_update_bits(
			AIU_MEM_I2S_MASKS,
			0xffff << 16,
			s_i2s_output.i2sinfo.int_num << 16);
#endif

	/*work queue*/
	INIT_WORK(&s_i2s_output.work, i2s_out_mix_work_handler);

	return ret;
}
EXPORT_SYMBOL(i2s_out_mix_init);

/* Deinit */
int i2s_out_mix_deinit(void)
{
	if (!s_i2s_output.isInit)
		return -1;
	free_irq(IRQ_OUT, &s_i2s_output);
	memset((void *)&s_i2s_output, 0, sizeof(struct i2s_output));
	return 0;
}
EXPORT_SYMBOL(i2s_out_mix_deinit);
