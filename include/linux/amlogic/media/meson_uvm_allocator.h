/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_UVM_ALLOCATOR_H
#define __MESON_UVM_ALLOCATOR_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/miscdevice.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>

#define MUA_IMM_ALLOC        BIT(UVM_IMM_ALLOC)
#define MUA_DELAY_ALLOC      BIT(UVM_DELAY_ALLOC)
#define MUA_FAKE_ALLOC       BIT(UVM_FAKE_ALLOC)
#define MUA_USAGE_PROTECTED  BIT(UVM_SECURE_ALLOC)
#define MUA_SKIP_REALLOC     BIT(UVM_SKIP_REALLOC)
#define MUA_BUFFER_CACHED    BIT(UVM_USAGE_CACHED)
#define MUA_DETACH           BIT(UVM_DETACH_FLAG)
#define ION_FLAG_PROTECTED   BIT(31)
#define META_DATA_SIZE       (512)

struct mua_device;
struct mua_buffer;

enum mua_debug_mask {
	MUA_DEBUG_LEVEL_ERROR,
	MUA_DEBUG_LEVEL_INFO,
	MUA_DEBUG_LEVEL_DBG
};

#define MUA_ERROR       BIT(MUA_DEBUG_LEVEL_ERROR)
#define MUA_INFO        BIT(MUA_DEBUG_LEVEL_INFO)
#define MUA_DBG         BIT(MUA_DEBUG_LEVEL_DBG)

struct mua_buffer {
	struct uvm_buf_obj base;
	struct mua_device *dev;
	size_t size;
	struct ion_buffer *ibuffer[2];
	struct dma_buf *idmabuf[2];
	struct sg_table *sg_table;

	int byte_stride;
	u32 width;
	u32 height;
	phys_addr_t paddr;
	int commit_display;
	u32 index;
	u32 ion_flags;
	u32 align;
};

struct mua_device {
	struct miscdevice dev;
	struct rb_root root;

	struct mutex buffer_lock; /* dev mutex */
	int pid;
};

struct uvm_alloc_data {
	int size;
	int align;
	unsigned int flags;
	int v4l2_fd;
	int fd;
	int byte_stride;
	u32 width;
	u32 height;
	int scalar;
	int scaled_buf_size;
};

struct uvm_pid_data {
	int pid;
};

/*get video info from uvm vframe */
struct uvm_fd_info {
	int fd;
	int type;
	u64 timestamp;
};

struct uvm_fd_data {
	int fd;
	int data;
};

struct uvm_usage_data {
	int fd;
	int uvm_data_usage;
};

struct uvm_meta_data {
	int fd;
	int type;
	int size;
	u8 data[META_DATA_SIZE];
};

struct uvm_hook_data {
	int mode_type;
	int shared_fd;
	char data_buf[META_DATA_SIZE + 1];
};

union uvm_ioctl_arg {
	struct uvm_alloc_data alloc_data;
	struct uvm_pid_data pid_data;
	struct uvm_fd_data fd_data;
	struct uvm_fd_info fd_info;
	struct uvm_usage_data usage_data;
	struct uvm_meta_data meta_data;
	struct uvm_hook_data hook_data;
};

#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_ALLOC _IOWR(UVM_IOC_MAGIC, 0, \
				struct uvm_alloc_data)
#define UVM_IOC_FREE _IOWR(UVM_IOC_MAGIC, 1, \
				struct uvm_alloc_data)
#define UVM_IOC_SET_PID _IOWR(UVM_IOC_MAGIC, 2, \
				struct uvm_pid_data)
#define UVM_IOC_SET_FD _IOWR(UVM_IOC_MAGIC, 3, \
				struct uvm_fd_data)
#define UVM_IOC_GET_METADATA _IOWR(UVM_IOC_MAGIC, 4, \
				struct uvm_meta_data)
#define UVM_IOC_ATTATCH _IOWR(UVM_IOC_MAGIC, 5, \
				struct uvm_hook_data)
#define UVM_IOC_GET_INFO _IOWR(UVM_IOC_MAGIC, 6, \
				struct uvm_hook_data)
#define UVM_IOC_SET_INFO _IOWR(UVM_IOC_MAGIC, 7, \
				struct uvm_hook_data)
#define UVM_IOC_DETATCH _IOWR(UVM_IOC_MAGIC, 8, \
				struct uvm_hook_data)
#define UVM_IOC_SET_USAGE _IOWR(UVM_IOC_MAGIC, 9, \
				struct uvm_usage_data)
#define UVM_IOC_GET_USAGE _IOWR(UVM_IOC_MAGIC, 10, \
				struct uvm_usage_data)
#define UVM_IOC_GET_VIDEO_INFO _IOWR(UVM_IOC_MAGIC, 11, \
				struct uvm_fd_info)

#endif

