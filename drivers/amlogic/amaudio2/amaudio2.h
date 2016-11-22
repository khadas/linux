/*
 * drivers/amlogic/amaudio2/amaudio2.h
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
#ifndef _AMAUDIO2_H_
#define _AMAUDIO2_H_

#define AMAUDIO2_MODULE_NAME "amaudio2"
#define AMAUDIO2_DRIVER_NAME "amaudio2"
#define AMAUDIO2_DEVICE_NAME "amaudio2-dev"
#define AMAUDIO2_CLASS_NAME "amaudio2"

struct amaudio_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
	void *runtime;
};

struct BUF {
	dma_addr_t paddr;
	unsigned char *addr;
	unsigned size;
	unsigned wr;
	unsigned rd;
	unsigned level;
	spinlock_t lock;
};

struct amaudio_t {
	struct device *dev;
	struct BUF hw;
	struct BUF sw;
	int type;
};

static int amaudio_open(struct inode *inode, struct file *file);

static int amaudio_release(struct inode *inode, struct file *file);

static int amaudio_mmap(struct file *file, struct vm_area_struct *vms);

static long amaudio_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);

static long amaudio_utils_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);

#ifdef CONFIG_COMPAT
static long amaudio_compat_ioctl(struct file *file, unsigned int cmd,
				ulong arg);
static long amaudio_compat_utils_ioctl(struct file *file, unsigned int cmd,
				ulong arg);
#endif

static irqreturn_t i2s_out_callback(int irq, void *data);
static unsigned get_i2s_out_size(void);
static unsigned get_i2s_out_ptr(void);
void cover_memcpy(struct BUF *des, int a, struct BUF *src, int b,
				unsigned count);
void direct_mix_memcpy(struct BUF *des, int a, struct BUF *src, int b,
				unsigned count);
void inter_mix_memcpy(struct BUF *des, int a, struct BUF *src, int b,
				unsigned count);
void cover_memcpy_8_channel(struct BUF *des, int a, struct BUF *src, int b,
				unsigned count);
void direct_mix_memcpy_8_channel(struct BUF *des, int a, struct BUF *src, int b,
				unsigned count);
void inter_mix_memcpy_8_channel(struct BUF *des, int a, struct BUF *src, int b,
				unsigned count);
extern void set_hw_resample_source(int source);

extern unsigned long aml_i2s_playback_start_addr;
extern unsigned long aml_i2s_playback_phy_start_addr;
extern unsigned long aml_i2s_alsa_write_addr;
extern unsigned int aml_i2s_playback_channel;
extern unsigned int aml_i2s_playback_format;

#define AMAUDIO_IOC_MAGIC  'A'

#define AMAUDIO_IOC_GET_SIZE	_IOW(AMAUDIO_IOC_MAGIC, 0x00, int)
#define AMAUDIO_IOC_GET_PTR		_IOW(AMAUDIO_IOC_MAGIC, 0x01, int)
#define AMAUDIO_IOC_RESET		_IOW(AMAUDIO_IOC_MAGIC, 0x02, int)
#define AMAUDIO_IOC_UPDATE_APP_PTR	_IOW(AMAUDIO_IOC_MAGIC, 0x03, int)
#define AMAUDIO_IOC_AUDIO_OUT_MODE	_IOW(AMAUDIO_IOC_MAGIC, 0x04, int)
#define AMAUDIO_IOC_MIC_LEFT_GAIN	_IOW(AMAUDIO_IOC_MAGIC, 0x05, int)
#define AMAUDIO_IOC_MIC_RIGHT_GAIN	_IOW(AMAUDIO_IOC_MAGIC, 0x06, int)
#define AMAUDIO_IOC_MUSIC_GAIN		_IOW(AMAUDIO_IOC_MAGIC, 0x07, int)

#endif
