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

#define MUA_IMM_ALLOC	BIT(UVM_IMM_ALLOC)
#define MUA_DELAY_ALLOC	BIT(UVM_DELAY_ALLOC)

struct mua_device;
struct mua_buffer;

struct mua_buffer {
	struct uvm_buf_obj base;
	struct mua_device *dev;
	size_t size;
	struct ion_buffer *ibuffer;
	struct sg_table *sg_table;

	int byte_stride;
	u32 width;
	u32 height;
	phys_addr_t paddr;
	int commit_display;
	struct vframe_s *vf;
	u32 index;
};

struct mua_device {
	struct miscdevice dev;
	struct rb_root root;

	struct mutex buffer_lock;
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
};

struct uvm_pid_data {
	int pid;
};

struct uvm_fd_data {
	int fd;
	int commit_display;
};

union uvm_ioctl_arg {
	struct uvm_alloc_data alloc_data;
	struct uvm_pid_data pid_data;
	struct uvm_fd_data fd_data;
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

#endif

