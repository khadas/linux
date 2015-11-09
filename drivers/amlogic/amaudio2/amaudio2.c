/*
 * drivers/amlogic/amaudio2/amaudio2.c
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
#define pr_fmt(fmt) "amaudio2: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/* Amlogic headers */
#include <linux/amlogic/major.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/sound/aiu_regs.h>
#include <linux/amlogic/sound/audin_regs.h>
#include "amaudio2.h"

#define BASE_IRQ                        (32)
#define AM_IRQ(reg)                     (reg + BASE_IRQ)
#define INT_I2S_DDR                     AM_IRQ(48)
#define INT_AUDIO_IN                    AM_IRQ(7)

#define IRQ_OUT                         INT_I2S_DDR

#define AMAUDIO2_DEVICE_COUNT    ARRAY_SIZE(amaudio_ports)
#define BUFFER_THRESHOLD_DEFAULT 10240
#define LATENCY_DEFAULT 2048

static struct class *amaudio_classp;
static struct device *amaudio_dev;

static const struct file_operations amaudio_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_out_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.mmap		= amaudio_mmap,
	.read		= amaudio_read,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_in_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.mmap		= amaudio_mmap,
	.read		= amaudio_read,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_ctl_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.unlocked_ioctl = amaudio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_ioctl,
#endif
};

static const struct file_operations amaudio_utils_fops = {
	.owner		= THIS_MODULE,
	.open		= amaudio_open,
	.release	= amaudio_release,
	.unlocked_ioctl = amaudio_utils_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amaudio_compat_utils_ioctl,
#endif
};

static struct amaudio_port_t amaudio_ports[] = {
	{
		.name = "amaudio2_out",
		.fops = &amaudio_out_fops,
	},
	{
		.name = "amaudio2_in",
		.fops = &amaudio_in_fops,
	},
	{
		.name = "amaudio2_ctl",
		.fops = &amaudio_ctl_fops,
	},
	{
		.name = "amaudio2_utils",
		.fops = &amaudio_utils_fops,
	},
};

int direct_left_gain = 256;
int direct_right_gain = 256;
int music_gain = 0;
int audio_out_mode = 0;
int audio_out_read_enable = 0;
int amaudio2_enable = 0;
int android_left_gain = 256;
int android_right_gain = 256;
int soft_buffer_threshold = BUFFER_THRESHOLD_DEFAULT;

#define SOFT_BUFFER_SIZE (PAGE_SIZE * 8)
#define MAX_LATENCY (64 * 32 * 3)
#define MIN_LATENCY (64 * 32)
static unsigned latency = MIN_LATENCY * 2;      /* 20ms */

static u64 amaudio_pcm_dmamask = DMA_BIT_MASK(32);
#define HRTIMER_PERIOD (1000000000UL / 1000)

#define INT_NUM         (16)    /* min 2, max 32 */
/* block_size = 32byte*channel_num, normal is 2 channel */
#define I2S_BLOCK       (64)
#define INT_BLOCK ((INT_NUM)*(I2S_BLOCK))

static int malloc_soft_readback_buffer(struct amaudio_t *amaudio, int size)
{
	amaudio->sw_read.addr = kzalloc(size, GFP_KERNEL);
	if (!amaudio->sw_read.addr) {
		pr_info(KERN_ERR
			"amaudio2 out read soft buffer alloc failed\n");
		return -ENOMEM;
	}
	amaudio->sw_read.size = size;
	return 0;
}

static int amaudio_open(struct inode *inode, struct file *file)
{
	struct amaudio_port_t
	*this = &amaudio_ports[iminor(inode)];
	struct amaudio_t *amaudio =
		kzalloc(sizeof(struct amaudio_t), GFP_KERNEL);
	int res = 0;

	if (iminor(inode) == 0) {
		pr_info(KERN_DEBUG "amaudio2_out opened\n");
		if (!this->dev->dma_mask)
			this->dev->dma_mask = &amaudio_pcm_dmamask;
		if (!this->dev->coherent_dma_mask)
			this->dev->coherent_dma_mask = 0xffffffff;

		amaudio->sw.addr = (char *)dma_alloc_coherent(this->dev,
							      SOFT_BUFFER_SIZE,
							      &amaudio->sw.
							      paddr,
							      GFP_KERNEL);
		amaudio->sw.size = SOFT_BUFFER_SIZE;
		if (!amaudio->sw.addr) {
			res = -ENOMEM;
			pr_info(KERN_ERR
				"amaudio2 out soft DMA buffer alloc failed\n");
			goto error;
		}

		amaudio->hw.addr = (unsigned char *)aml_i2s_playback_start_addr;
		amaudio->hw.paddr = aml_i2s_playback_phy_start_addr;
		amaudio->hw.size = get_i2s_out_size();
		amaudio->hw.rd = get_i2s_out_ptr();

		spin_lock_init(&amaudio->sw.lock);
		spin_lock_init(&amaudio->hw.lock);
		spin_lock_init(&amaudio->sw_read.lock);

		if (request_irq
			    (IRQ_OUT, i2s_out_callback, IRQF_SHARED, "i2s_out",
			    amaudio)) {
			res = -EINVAL;
			goto error;
		}
		aml_cbus_update_bits(AIU_MEM_I2S_MASKS, 0xffff << 16,
				     INT_NUM << 16);

	} else if (iminor(inode) == 1) {
		pr_info("amaudio2_in opened\n");
		if (!this->dev->dma_mask)
			this->dev->dma_mask = &amaudio_pcm_dmamask;
		if (!this->dev->coherent_dma_mask)
			this->dev->coherent_dma_mask = 0xffffffff;
	} else if (iminor(inode) == 2) {
		pr_info("amaudio2_ctl opened\n");
	} else if (iminor(inode) == 3) {
		pr_info("amaudio2_utils opened\n");
	} else {
		pr_err("BUG:%s,%d, please check\n", __FILE__, __LINE__);
		res = -EINVAL;
		goto error;
	}

	amaudio->type = iminor(inode);
	amaudio->dev = this->dev;
	file->private_data = amaudio;
	file->f_op = this->fops;
	this->runtime = amaudio;
	return res;
error: kfree(amaudio);
	return res;
}

static int amaudio_release(struct inode *inode, struct file *file)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;

	if (iminor(inode) == 0) {
		free_irq(IRQ_OUT, amaudio);
		if (amaudio->sw.addr) {
			dma_free_coherent(amaudio->dev, amaudio->sw.size,
					  (void *)amaudio->sw.addr,
					  amaudio->sw.paddr);
			amaudio->sw.addr = 0;
		}
		kfree(amaudio->sw_read.addr);
		amaudio->sw_read.addr = 0;

	}

	kfree(amaudio);
	return 0;
}

static int amaudio_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;

	if (amaudio->type == 0) {
		int mmap_flag = dma_mmap_coherent(amaudio->dev, vma,
						  (void *)amaudio->sw.addr,
						  amaudio->sw.paddr,
						  amaudio->sw.size);
		return mmap_flag;
	} else if (amaudio->type == 1) {
		pr_info("audio in mmap!\n");
	} else {
		return -ENODEV;
	}

	return 0;
}

static ssize_t amaudio_read(struct file *file,
			    char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;
	struct BUF *sw_read = &amaudio->sw_read;

	if (amaudio->type == 0) {
		ret = copy_to_user((void *)buf,
				   (void *)(sw_read->addr + sw_read->rd),
				   count);
		return count - ret;
	} else if (amaudio->type == 1) {
		pr_info("audio in read!\n");
	} else {
		return -ENODEV;
	}

	return 0;
}

static unsigned get_i2s_out_size(void)
{
	return aml_read_cbus(AIU_MEM_I2S_END_PTR)
	       - aml_read_cbus(AIU_MEM_I2S_START_PTR) + 64;
}

static unsigned get_i2s_out_ptr(void)
{
	return aml_read_cbus(AIU_MEM_I2S_RD_PTR)
	       - aml_read_cbus(AIU_MEM_I2S_START_PTR);
}

void cover_memcpy(struct BUF *des, int a, struct BUF *src, int b,
		  unsigned count)
{
	int i, j;
	int samp;

	short *des_left = (short *)(des->addr + a);
	short *des_right = des_left + 16;
	short *src_buf = (short *)(src->addr + b);

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			samp = ((*src_buf++) * direct_left_gain) >> 8;
			*des_left++ = (short)samp;
			samp = ((*src_buf++) * direct_right_gain) >> 8;
			*des_right++ = (short)samp;
		}
		des_left += 16;
		des_right += 16;
	}
}

void direct_mix_memcpy(struct BUF *des, int a, struct BUF *src, int b,
		       unsigned count)
{
	int i, j;
	int samp;

	short *des_left = (short *)(des->addr + a);
	short *des_right = des_left + 16;
	short *src_buf = (short *)(src->addr + b);

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			samp =
				((*des_left) * music_gain +
				 (*src_buf++) * direct_left_gain)
				>> 8;
			if (samp > 32767)
				samp = 32767;
			if (samp < -32768)
				samp = -32768;
			*des_left++ = (short)(samp & 0xffff);

			samp =
				((*des_right) * music_gain
				 + (*src_buf++) * direct_right_gain) >> 8;
			if (samp > 32767)
				samp = 32767;
			if (samp < -32768)
				samp = -32768;
			*des_right++ = (short)(samp & 0xffff);
		}
		des_left += 16;
		des_right += 16;
	}
}

void inter_mix_memcpy(struct BUF *des, int a, struct BUF *src, int b,
		      unsigned count)
{
	int i, j;
	short sampL, sampR;
	int samp, sampLR;

	short *des_left = (short *)(des->addr + a);
	short *des_right = des_left + 16;
	short *src_buf = (short *)(src->addr + b);

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			sampL = *src_buf++;
			sampR = *src_buf++;
			/* Here has risk to distortion.
			 * Linein signals are always weak,
			 * so add them direct. */
			sampLR =
				(sampL * direct_left_gain +
				 sampR * direct_right_gain)
				>> 1;
			samp = ((*des_left) * music_gain + sampLR) >> 8;
			if (samp > 32767)
				samp = 32767;
			if (samp < -32768)
				samp = -32768;
			*des_left++ = (short)(samp & 0xffff);

			samp = ((*des_right) * music_gain + sampLR) >> 8;
			if (samp > 32767)
				samp = 32767;
			if (samp < -32768)
				samp = -32768;
			*des_right++ = (short)(samp & 0xffff);
		}
		des_left += 16;
		des_right += 16;
	}
}

void interleave_memcpy(struct BUF *des, int a, struct BUF *src, int b,
		       unsigned count)
{
	int i, j;
	short *out = (short *)(des->addr + a);
	short *in_left = (short *)(src->addr + b);
	short *in_right = in_left + 16;

	for (i = 0; i < count; i += 64) {
		for (j = 0; j < 16; j++) {
			*out++ = *in_left++;
			*out++ = *in_right++;
		}
		in_left += 16;
		in_right += 16;
	}
}

static void i2s_copy(struct amaudio_t *amaudio)
{
	struct BUF *hw = &amaudio->hw;
	struct BUF *sw = &amaudio->sw;
	struct BUF *sw_read = &amaudio->sw_read;
	unsigned long swirqflags, hwirqflags, sw_readirqflags;
	unsigned i2s_out_ptr = get_i2s_out_ptr();
	unsigned alsa_delay = (aml_i2s_alsa_write_addr + hw->size - i2s_out_ptr)
			      % hw->size;
	unsigned amaudio_delay = (hw->wr + hw->size - i2s_out_ptr) % hw->size;

	spin_lock_irqsave(&hw->lock, hwirqflags);

	hw->rd = i2s_out_ptr;
	hw->level = amaudio_delay;
	if (hw->level <= INT_BLOCK ||
	    (alsa_delay - amaudio_delay) < INT_BLOCK) {
		hw->wr = (hw->rd + latency) % hw->size;
		hw->wr /= INT_BLOCK;
		hw->wr *= INT_BLOCK;
		hw->level = latency;

		goto EXIT;
	}

	if (sw->level > BUFFER_THRESHOLD_DEFAULT) {
		/*pr_info(KERN_DEBUG
		*	"Reset sw: hw->wr = %x,hw->rd = %x,
		*	hw->level = %x,"
		*	"alsa_delay:%x,sw->wr = %x,
		*	sw->rd = %x,sw->level = %x\n",
		*	hw->wr, hw->rd, hw->level,
		*	alsa_delay, sw->wr, sw->rd,
		*	sw->level);
		*/
		sw->rd = (sw->wr + 1024) % sw->size;
		sw->level = sw->size - 1024;
		goto EXIT;
	}

	if (sw->level < INT_BLOCK)
		goto EXIT;

	BUG_ON((hw->wr + INT_BLOCK > hw->size)
	       || (sw->rd + INT_BLOCK > sw->size));
	BUG_ON((hw->wr < 0) || (sw->rd < 0));

	if (audio_out_mode == 0)
		cover_memcpy(hw, hw->wr, sw, sw->rd, INT_BLOCK);
	else if (audio_out_mode == 1)
		inter_mix_memcpy(hw, hw->wr, sw, sw->rd, INT_BLOCK);
	else if (audio_out_mode == 2)
		direct_mix_memcpy(hw, hw->wr, sw, sw->rd, INT_BLOCK);

	if (audio_out_read_enable == 1) {
		spin_lock_irqsave(&sw_read->lock, sw_readirqflags);
		if (sw_read->level > INT_BLOCK) {
			interleave_memcpy(sw_read, sw_read->wr, hw, hw->wr,
					  INT_BLOCK);
			sw_read->wr = (sw_read->wr + INT_BLOCK) % sw_read->size;
			sw_read->level = sw_read->size
					 - (sw_read->size + sw_read->wr -
					    sw_read->rd)
					 % sw_read->size;
			spin_unlock_irqrestore(&sw_read->lock, sw_readirqflags);
		}
	}

	hw->wr = (hw->wr + INT_BLOCK) % hw->size;
	hw->level = (hw->wr + hw->size - i2s_out_ptr) % hw->size;

	spin_lock_irqsave(&sw->lock, swirqflags);
	sw->rd = (sw->rd + INT_BLOCK) % sw->size;
	sw->level = (sw->size + sw->wr - sw->rd) % sw->size;
	spin_unlock_irqrestore(&sw->lock, swirqflags);

EXIT:  spin_unlock_irqrestore(&hw->lock, hwirqflags);
	return;
}

static irqreturn_t i2s_out_callback(int irq, void *data)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)data;

	i2s_copy(amaudio);
	return IRQ_HANDLED;
}

/* -----------------control interface---------------- */

#ifdef CONFIG_COMPAT
static long amaudio_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	return amaudio_ioctl(file, cmd, (ulong)compat_ptr(arg));
}

static long amaudio_compat_utils_ioctl(struct file *file,
				       unsigned int cmd, ulong arg)
{
	return amaudio_utils_ioctl(file, cmd, (ulong)compat_ptr(arg));
}
#endif

static long amaudio_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct amaudio_t *amaudio = (struct amaudio_t *)file->private_data;
	s32 r = 0;
	unsigned long swirqflags, hwirqflags, sw_readirqflags;

	switch (cmd) {
	case AMAUDIO_IOC_GET_SIZE:
		/* total size of internal buffer */
		r = amaudio->sw.size;
		break;
	case AMAUDIO_IOC_GET_PTR:
		/* the read pointer of internal buffer */
		spin_lock_irqsave(&amaudio->sw.lock, swirqflags);
		r = amaudio->sw.rd;
		spin_unlock_irqrestore(&amaudio->sw.lock, swirqflags);
		break;
	case AMAUDIO_IOC_UPDATE_APP_PTR:
		/*
		 * the user space write pointer
		 * of the internal buffer
		 */
	{
		spin_lock_irqsave(&amaudio->sw.lock, swirqflags);
		amaudio->sw.wr = arg;
		amaudio->sw.level = (amaudio->sw.size + amaudio->sw.wr
				     -
				     amaudio->sw.rd) % amaudio->sw.size;
		spin_unlock_irqrestore(&amaudio->sw.lock, swirqflags);
		if (amaudio->sw.wr % 64)
			pr_info("wr:%x, not 64 Bytes align\n",
				amaudio->sw.wr);
	}
	break;
	case AMAUDIO_IOC_RESET:
		/*
		 * reset the internal write buffer
		 * pointer to get a given latency
		 * this api should be called before fill datas
		 */
		spin_lock_irqsave(&amaudio->hw.lock, hwirqflags);

		amaudio->hw.rd = get_i2s_out_ptr();
		amaudio->hw.wr = (amaudio->hw.rd + latency) % amaudio->hw.size;
		amaudio->hw.wr /= INT_BLOCK;
		amaudio->hw.wr *= INT_BLOCK;
		amaudio->hw.level = latency;
		spin_unlock_irqrestore(&amaudio->hw.lock, hwirqflags);
		/* empty the buffer */
		spin_lock_irqsave(&amaudio->sw.lock, swirqflags);
		amaudio->sw.wr = 0;
		amaudio->sw.rd = 0;
		amaudio->sw.level = 0;
		spin_unlock_irqrestore(&amaudio->sw.lock, swirqflags);
		/* empty the out read buffer */
		spin_lock_irqsave(&amaudio->sw_read.lock, sw_readirqflags);
		amaudio->sw_read.wr = 0;
		amaudio->sw_read.rd = 0;
		amaudio->sw_read.level = amaudio->sw_read.size;
		spin_unlock_irqrestore(&amaudio->sw_read.lock, sw_readirqflags);

		pr_info("Reset amaudio2: latency=%d bytes\n", latency);
		break;
	case AMAUDIO_IOC_AUDIO_OUT_MODE:
		/*
		 * audio_out_mode = 0, covered alsa audio mode;
		 * audio_out_mode = 1, karaOK mode, Linein left and
		 * right channel inter mixed with android alsa audio;
		 * audio_out_mode = 2,
		 * TV in direct mix with android audio;
		 */
		if (arg < 0 || arg > 2)
			return -EINVAL;

		audio_out_mode = arg;
		break;
	case AMAUDIO_IOC_MIC_LEFT_GAIN:
		/* in karaOK mode, mic volume can be set from 0-256 */
		if (arg < 0 || arg > 256)
			return -EINVAL;
		direct_left_gain = arg;
		break;
	case AMAUDIO_IOC_MIC_RIGHT_GAIN:
		if (arg < 0 || arg > 256)
			return -EINVAL;

		direct_right_gain = arg;
		break;
	case AMAUDIO_IOC_MUSIC_GAIN:
		/* music volume can be set from 0-256 */
		if (arg < 0 || arg > 256)
			return -EINVAL;

		music_gain = arg;
		break;
	case AMAUDIO_IOC_GET_PTR_READ:
		/* the write pointer of internal read buffer */
		if (amaudio->sw_read.addr == NULL)
			break;

		spin_lock_irqsave(&amaudio->sw_read.lock, sw_readirqflags);
		r = amaudio->sw_read.wr;
		spin_unlock_irqrestore(&amaudio->sw_read.lock, sw_readirqflags);
		break;
	case AMAUDIO_IOC_UPDATE_APP_PTR_READ:
		/* the user space read pointer of the read buffer */
	{
		if (amaudio->sw_read.addr == NULL)
			break;

		spin_lock_irqsave(&amaudio->sw_read.lock,
				  sw_readirqflags);
		amaudio->sw_read.rd = arg;
		amaudio->sw_read.level = amaudio->sw_read.size
					 - (amaudio->sw_read.size +
					    amaudio->sw_read.wr
					    - amaudio->sw_read.rd) %
					 amaudio->sw_read.size;
		spin_unlock_irqrestore(&amaudio->sw_read.lock,
				       sw_readirqflags);
		if (amaudio->sw_read.rd % 64)
			pr_info("rd:%x, not 64 Bytes align\n",
				amaudio->sw_read.rd);
	}
	break;
	case AMAUDIO_IOC_OUT_READ_ENABLE:
		/* enable amaudio output read from hw buffer */
		spin_lock_irqsave(&amaudio->sw_read.lock, sw_readirqflags);
		if (arg == 1) {
			if (!malloc_soft_readback_buffer
				    (amaudio, SOFT_BUFFER_SIZE))
				audio_out_read_enable = 1;
		} else if (arg == 0) {
			kfree(amaudio->sw_read.addr);
			amaudio->sw_read.addr = NULL;
			audio_out_read_enable = 0;
		}
		spin_unlock_irqrestore(&amaudio->sw_read.lock, sw_readirqflags);
		break;
	default:
		break;
	}
	;

	return r;
}

static long amaudio_utils_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	return 0;
}

/* -----------------class interface---------------- */
static ssize_t show_audio_out_mode(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", audio_out_mode);
}

static ssize_t store_audio_out_mode(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	if (buf[0] == '0') {
		pr_info
		(
			"Audio_in data covered the android local data as output!\n");
		audio_out_mode = 0;
	} else if (buf[0] == '1') {
		pr_info
		(
			"Audio_in left/right channels and the android local data inter mixed as output!\n");
		audio_out_mode = 1;
	} else if (buf[0] == '2') {
		pr_info
		(
			"Audio_in data direct mixed with the android local data as output!\n");
		audio_out_mode = 2;
	}
	return count;
}

static ssize_t show_direct_left_gain(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", direct_left_gain);
}

static ssize_t store_direct_left_gain(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	direct_left_gain = val;
	pr_info("direct_left_gain set to %d\n", direct_left_gain);
	return count;
}

static ssize_t show_direct_right_gain(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", direct_right_gain);
}

static ssize_t store_direct_right_gain(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	direct_right_gain = val;
	pr_info("direct_right_gain set to %d\n", direct_right_gain);
	return count;
}

static ssize_t show_music_gain(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", music_gain);
}

static ssize_t store_music_gain(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	music_gain = val;
	pr_info("music_gain set to %d\n", music_gain);
	return count;
}

static ssize_t show_android_left_gain(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", android_left_gain);
}

static ssize_t store_android_left_gain(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	android_left_gain = val;
	pr_info("android_left_gain set to %d\n", android_left_gain);
	return count;
}

static ssize_t show_android_right_gain(struct class *class,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", android_right_gain);
}

static ssize_t store_android_right_gain(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 256)
		val = 256;

	android_right_gain = val;
	pr_info("android_right_gain set to %d\n", android_right_gain);
	return count;
}

static ssize_t show_audio_read_enable(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", audio_out_read_enable);
}

static ssize_t store_audio_read_enable(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	if (buf[0] == '0') {
		pr_info("Read audio data is disable!\n");
		audio_out_read_enable = 0;
	} else if (buf[0] == '1') {
		pr_info("Read audio data is enable!\n");
		audio_out_read_enable = 1;
	} else {
		pr_info("Invalid argument!\n");
	}
	return count;
}

int set_i2s_iec958_samesource(int enable)
{
	if (enable == 0)
		aml_cbus_update_bits(AIU_I2S_MISC, 1 << 3, 0);
	else if (enable == 1)
		aml_cbus_update_bits(AIU_I2S_MISC, 1 << 3, 1 << 3);

	return 0;
}

static ssize_t show_aml_amaudio2_enable(struct class *class,
					struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", amaudio2_enable);
}

static ssize_t store_aml_amaudio2_enable(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	if (buf[0] == '0') {
		pr_info("amaudio2 is disable!\n");
		amaudio2_enable = 0;
		set_i2s_iec958_samesource(0);
	} else if (buf[0] == '1') {
		pr_info("amaudio2 is enable!\n");
		amaudio2_enable = 1;
		set_i2s_iec958_samesource(1);
	} else {
		pr_info("Invalid argument!\n");
	}

	return count;
}

static struct class_attribute amaudio_attrs[] = {
	__ATTR(aml_audio_out_mode,
	       S_IRUGO | S_IWUSR,
	       show_audio_out_mode,
	       store_audio_out_mode),
	__ATTR(aml_direct_left_gain,
	       S_IRUGO | S_IWUSR,
	       show_direct_left_gain,
	       store_direct_left_gain),
	__ATTR(aml_direct_right_gain,
	       S_IRUGO | S_IWUSR,
	       show_direct_right_gain,
	       store_direct_right_gain),
	__ATTR(aml_music_gain,
	       S_IRUGO | S_IWUSR,	   show_music_gain,
	       store_music_gain),
	__ATTR(aml_android_left_gain,
	       S_IRUGO | S_IWUSR,
	       show_android_left_gain,
	       store_android_left_gain),
	__ATTR(aml_android_right_gain,
	       S_IRUGO | S_IWUSR,
	       show_android_right_gain,
	       store_android_right_gain),
	__ATTR(aml_audio_read_enable,
	       S_IRUGO | S_IWUSR,
	       show_audio_read_enable,
	       store_audio_read_enable),
	__ATTR(aml_amaudio2_enable,
	       S_IRUGO | S_IWUSR | S_IWGRP,
	       show_aml_amaudio2_enable,
	       store_aml_amaudio2_enable),
	__ATTR_NULL
};

static struct class amaudio_class = {
	.name		= AMAUDIO2_CLASS_NAME,
	.class_attrs	= amaudio_attrs,
};

static int amaudio2_init(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amaudio_port_t *ap;

	/*pr_info("amaudio2: driver %s init!\n", AMAUDIO2_DRIVER_NAME);*/

	ret =
		register_chrdev(AMAUDIO2_MAJOR, AMAUDIO2_DRIVER_NAME,
				&amaudio_fops);
	if (ret < 0) {
		pr_err("Can't register %s divece ", AMAUDIO2_DRIVER_NAME);
		ret = -ENODEV;
		goto err;
	}

	amaudio_classp = class_create(THIS_MODULE, AMAUDIO2_DEVICE_NAME);
	if (IS_ERR(amaudio_classp)) {
		ret = PTR_ERR(amaudio_classp);
		goto err1;
	}

	ap = &amaudio_ports[0];
	for (i = 0; i < AMAUDIO2_DEVICE_COUNT; ap++, i++) {
		ap->dev = device_create(amaudio_classp, NULL,
					MKDEV(AMAUDIO2_MAJOR, i), NULL,
					amaudio_ports[i].name);
		if (IS_ERR(ap->dev)) {
			pr_err
			(
				"amaudio2: failed to create amaudio device node\n");
			goto err2;
		}
	}

	ret = class_register(&amaudio_class);
	if (ret) {
		pr_err("amaudio2 class create fail.\n");
		goto err3;
	}

	amaudio_dev =
		device_create(&amaudio_class, NULL, MKDEV(AMAUDIO2_MAJOR,
							  15), NULL,
			      AMAUDIO2_CLASS_NAME);
	if (IS_ERR(amaudio_dev)) {
		pr_err("amaudio2 creat device fail.\n");
		goto err4;
	}

	pr_info("amaudio2: driver %s succuess!\n", AMAUDIO2_DRIVER_NAME);
	return 0;

err4:
	device_destroy(&amaudio_class, MKDEV(AMAUDIO2_MAJOR, 10));
err3:
	class_unregister(&amaudio_class);
err2:
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO2_DEVICE_COUNT;
	     ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO2_MAJOR, i));
err1:
	class_destroy(amaudio_classp);
err:
	unregister_chrdev(AMAUDIO2_MAJOR, AMAUDIO2_DRIVER_NAME);
	return ret;
}

static int amaudio2_exit(struct platform_device *pdev)
{
	int i = 0;
	struct amaudio_port_t *ap;

	device_destroy(&amaudio_class, MKDEV(AMAUDIO2_MAJOR, 10));
	class_unregister(&amaudio_class);
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO2_DEVICE_COUNT;
	     ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO2_MAJOR, i));
	class_destroy(amaudio_classp);
	unregister_chrdev(AMAUDIO2_MAJOR, AMAUDIO2_DRIVER_NAME);

	return 0;
}

static const struct of_device_id amlogic_match[] = {
	{.compatible = "amlogic, aml_amaudio2",},
	{},
};

static struct platform_driver aml_amaudio2_driver = {
	.driver = {
		   .name = "aml_amaudio2_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_match,
		   },

	.probe = amaudio2_init,
	.remove = amaudio2_exit,
};

static int __init aml_amaudio2_modinit(void)
{
	return platform_driver_register(&aml_amaudio2_driver);
}

static void __exit aml_amaudio2_modexit(void)
{
	platform_driver_unregister(&aml_amaudio2_driver);
}

module_init(aml_amaudio2_modinit);
module_exit(aml_amaudio2_modexit);

MODULE_DESCRIPTION("AMLOGIC TV Audio        driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_VERSION("2.0.0");
