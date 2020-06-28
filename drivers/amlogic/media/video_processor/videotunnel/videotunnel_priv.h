/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/videotunnel/videotunnel_priv.h
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

#ifndef __VIDEO_TUNNEL_H
#define __VIDEO_TUNNEL_H

#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/idr.h>
#include <linux/kref.h>
#include <linux/rtmutex.h>
#include <linux/dma-fence.h>

#include "uapi/videotunnel.h"

#define  VIDEO_TUNNEL_POOL_SIZE 32
#define  VIDEO_TUNNEL_MAX_WAIT_MS 4

union videotunnel_ioctl_arg {
	struct vt_alloc_id_data alloc_data;
	struct vt_ctrl_data ctrl_data;
	struct vt_buffer_data buffer_data;
};

/*
 * struct videotunnel_dev - the metadata of the videotunnel device node
 * @mdev:			the actual misc device
 * @instance_lock:	mutex procting the instances
 * @instance_idr:	an idr space for allocating instance ids
 * @instances:		an rb tree for all the existing video instance
 * @session_lock:	an semaphore protect sessions
 * @sessions:		an rb tree for all the existing video session
 * @dev_name:		the device name
 */
struct videotunnel_dev {
	struct miscdevice mdev;
	struct mutex instance_lock; /* protect the instances */
	struct idr instance_idr;
	struct rb_root instances;

	struct rw_semaphore session_lock;
	struct rb_root sessions;

	char *dev_name;
	struct kfifo cmd_queue;
	wait_queue_head_t cmd_wait;
	struct dentry *debug_root;
};

/*
 * struct videotunbel_session - a process block lock address space data
 * @node:		node in the tree of all session
 * @dev:		backpointer to videotunnel device
 * @instances_head:	an list of all instance allocated in this session
 * @lock:		lock protecting vt buffers
 * @buffers:	an rb tree for all the buffers in this session
 * @role:		producer or consumer
 * @name:		used for debugging
 * @display_name:	used for debugging (unique version of @name)
 * @display_serial:	used for debugging (to make display_name unique)
 * @task:		used for debugging
 */
struct videotunnel_session {
	struct rb_node node;
	struct videotunnel_dev *dev;
	struct mutex lock; /* protect vt buffers */
	struct list_head instances_head;
	struct rb_root buffers;

	enum vt_role_e role;
	pid_t pid;
	char *name;
	char *display_name;
	int display_serial;
	struct task_struct *task;
	long cid;
	struct dentry *debug_root;
};

/*
 * struct videotunnel_buffer - videtunnel buffer
 * @node:			node in the sessions buffer rb tree
 * @file_buffer:	file get from buffer fd
 * @buffer_fd_pro:	fd in the producer side
 * @buffer_fd_con:	fd in the consumer side
 * @file_fence:		fence file get from release fence
 * @item:			buffer item
 */
struct videotunnel_buffer {
	struct rb_node node;
	struct file *file_buffer;
	int buffer_fd_pro;
	int buffer_fd_con;

	struct dma_fence *file_fence;
	struct videotunnel_session *session_pro;
	long cid_pro;
	struct vt_buffer_item item;
};

/*
 * struct videotunnel_instance - an instance that holds the buffer
 * @id:			unique id allocated by device->idr
 * @node:		node in the videotunel device rbtree
 * @lock:		proctect fifo
 * @dev:		backpointer to device
 * @entry:		list node in session list
 * @consumer:		consumer session on this instance
 * @wait_consumer:	cosnumer wait queue for buffer
 * @producer:		producer session on this instance
 * @wait_producer:	producer wait queue for buffer
 * @fifo_to_consumer:	fifo that queued the buffer transfer to consumer
 * @fifo_to_producer:	fifo that queued the buffer transfer to producer
 */

struct videotunnel_instance {
	struct kref ref;
	int id;
	struct videotunnel_dev *dev;
	struct rb_node node;
	struct mutex lock; /* proctect fifo */
	struct list_head entry;
	struct videotunnel_session *consumer;
	wait_queue_head_t wait_consumer;
	struct videotunnel_session *producer;
	wait_queue_head_t wait_producer;
	struct dentry *debug_root;

	DECLARE_KFIFO_PTR(fifo_to_consumer, struct videotunnel_buffer*);
	DECLARE_KFIFO_PTR(fifo_to_producer, struct videotunnel_buffer*);
/*	struct vt_frame_item items[VIDEO_TUNNEL_ITEM_SIZE]; */
};

#endif
