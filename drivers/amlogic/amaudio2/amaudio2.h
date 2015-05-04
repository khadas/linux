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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#define AMAUDIO_MODULE_NAME "amaudio2"
#define AMAUDIO_DRIVER_NAME "amaudio2"
#define AMAUDIO_DEVICE_NAME "amaudio2"
#define AMAUDIO_CLASS_NAME  "amaudio2"

struct amaudio_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
	void *runtime;
};

struct BUF {
	dma_addr_t paddr;
	char *addr;
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
	struct BUF sw_read;
	int type;

	/********** for debug ****************/
	int cnt0, cnt1, cnt2, cnt3, cnt4, cnt5, cnt6, cnt7, cnt8;
};

static int amaudio_open(struct inode *inode, struct file *file);

static int amaudio_release(struct inode *inode, struct file *file);

static int amaudio_mmap(struct file *file, struct vm_area_struct *vms);

static long amaudio_ioctl(struct file *file,
			  unsigned int cmd, unsigned long arg);

static long amaudio_utils_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);

static ssize_t amaudio_read(struct file *file,
			    char __user *buf, size_t count, loff_t *pos);

extern unsigned int aml_i2s_playback_start_addr;
extern unsigned int aml_i2s_playback_phy_start_addr;
extern unsigned int aml_i2s_alsa_write_addr;

#define AMAUDIO_IOC_MAGIC  'A'

#define AMAUDIO_IOC_GET_SIZE	_IOW(AMAUDIO_IOC_MAGIC, 0x00, int)
#define AMAUDIO_IOC_GET_PTR		_IOW(AMAUDIO_IOC_MAGIC, 0x01, int)
#define AMAUDIO_IOC_RESET		_IOW(AMAUDIO_IOC_MAGIC, 0x02, int)
#define AMAUDIO_IOC_UPDATE_APP_PTR	_IOW(AMAUDIO_IOC_MAGIC, 0x03, int)
#define AMAUDIO_IOC_AUDIO_OUT_MODE	_IOW(AMAUDIO_IOC_MAGIC, 0x04, int)
#define AMAUDIO_IOC_MIC_LEFT_GAIN	_IOW(AMAUDIO_IOC_MAGIC, 0x05, int)
#define AMAUDIO_IOC_MIC_RIGHT_GAIN	_IOW(AMAUDIO_IOC_MAGIC, 0x06, int)
#define AMAUDIO_IOC_MUSIC_GAIN		_IOW(AMAUDIO_IOC_MAGIC, 0x07, int)
#define AMAUDIO_IOC_GET_PTR_READ	_IOW(AMAUDIO_IOC_MAGIC, 0x08, int)
#define AMAUDIO_IOC_UPDATE_APP_PTR_READ \
			_IOW(AMAUDIO_IOC_MAGIC, 0x09, int)
#define AMAUDIO_IOC_OUT_READ_ENABLE \
			_IOW(AMAUDIO_IOC_MAGIC, 0x0a, int)
#define AMAUDIO_IOC_SET_ANDROID_VOLUME_ENABLE \
			_IOW(AMAUDIO_IOC_MAGIC, 0x0b, int)
#define AMAUDIO_IOC_SET_ANDROID_LEFT_VOLUME \
			_IOW(AMAUDIO_IOC_MAGIC, 0x0c, int)
#define AMAUDIO_IOC_SET_ANDROID_RIGHT_VOLUME \
			_IOW(AMAUDIO_IOC_MAGIC, 0x0d, int)
