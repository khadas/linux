/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_UVM_H
#define __MESON_UVM_H

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

#include <linux/amlogic/media/vfm/vframe.h>

struct uvm_device;
struct uvm_buffer;

struct uvm_buffer {
	struct rb_node node;
	struct uvm_device *dev;

	size_t size;
	unsigned long flags;
	unsigned long align;
	int byte_stride;
	u32 width;
	u32 height;

	struct dma_buf *import;
	struct sg_table *sgt;

	void *vaddr;
	phys_addr_t paddr;
	struct page **pages;
	struct dma_buf *dmabuf;

	struct file_private_data *file_private_data;
	struct vframe_s *vf;
	u32 index;

	int commit_display;
};

struct uvm_device {
	struct miscdevice dev;
	struct rb_root root;

	/* protect root tree */
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

