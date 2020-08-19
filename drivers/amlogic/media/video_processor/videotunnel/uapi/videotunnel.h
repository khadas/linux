/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/videotunnel/uapi/videotunnel.h
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

#ifndef __UAPI_VIDEO_TUNNEL_H
#define __UAPI_VIDEO_TUNNEL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MAX_VIDEO_BUFFER_NUM 8
#define MAX_VIDEO_TUNNEL 8

enum vt_role_e {
	VT_ROLE_PRODUCER,
	VT_ROLE_CONSUMER,
	VT_ROLE_INVALID,
};

enum vt_ctrl_cmd_e {
	VT_CTRL_CONNECT,
	VT_CTRL_DISCONNECT,
	VT_CTRL_SEND_CMD,
	VT_CTRL_RECV_CMD,
	VT_CTRL_REPLY_CMD,
};

enum vt_video_cmd_e {
	VT_VIDEO_SET_STATUS,
	VT_VIDEO_GET_STATUS,
};

enum vt_buffer_status {
	VT_BUFFER_QUEUE,
	VT_BUFFER_DEQUEUE,
	VT_BUFFER_ACQUIRE,
	VT_BUFFER_RELEASE,
	VT_BUFFER_FREE,
	VT_BUFFER_INVALID,
};

struct vt_alloc_id_data {
	int tunnel_id;
};

struct vt_ctrl_data {
	int tunnel_id;
	enum vt_role_e role;
	enum vt_ctrl_cmd_e ctrl_cmd;
	enum vt_video_cmd_e video_cmd;
	int video_cmd_data;
	int client_id;
};

/**
 * struct vt_buffer_item - vframe buffer metadata
 */
struct vt_buffer_item {
	int tunnel_id;
	int buffer_fd;
	int fence_fd;
	int buffer_status;
};

/**
 * struct vt_buffer_data - buffer data transfer between producer and consumer
 */
struct vt_buffer_data {
	int buffer_size;
	int tunnel_id;
	struct vt_buffer_item buffers[MAX_VIDEO_BUFFER_NUM];
};

#define VT_IOC_MAGIC 'V'

/**
 * VT_IOC_ALLOC_ID - allocate tunnel id
 *
 * takes an vt_alloc_id_data struct and returns it with tunnel_id
 * field populated
 */
#define VT_IOC_ALLOC_ID _IOWR(VT_IOC_MAGIC, 0, \
				       struct vt_alloc_id_data)

/**
 * VT_IOC_FREE_ID - free tunnel id
 *
 * takes an vt_alloc_id_data struct and no return
 * free the videotunnel_instance
 */
#define VT_IOC_FREE_ID _IOWR(VT_IOC_MAGIC, 1, \
				      struct vt_alloc_id_data)

/**
 * VT_IOC_CTRL - control tunnel on and off
 * takes an vt_ctrl_data struct and no return
 * control the tunnel switch
 */
#define VT_IOC_CTRL	_IOWR(VT_IOC_MAGIC, 2, \
				      struct vt_ctrl_data)

/**
 * VT_IOC_QUEUE_BUFFER - producer queue the vframe info
 * takes an vt_buffer_item and no return
 */
#define VT_IOC_QUEUE_BUFFER	_IOWR(VT_IOC_MAGIC, 3, \
				      struct vt_buffer_data)

/**
 * VT_IOC_DEQUEUE_BUFFER - producer dequeue the vframe info
 * takes an vt_buffer_item and returns it with index field
 * populated
 */
#define VT_IOC_DEQUEUE_BUFFER	_IOWR(VT_IOC_MAGIC, 4, \
				      struct vt_buffer_data)

/**
 * VT_IOC_RELEASE_BUFFER - consumer release the vframe info
 * takes an vt_buffer_data and no return
 * the vt_buffer_item should contain the fence
 */
#define VT_IOC_RELEASE_BUFFER	_IOWR(VT_IOC_MAGIC, 5, \
				      struct vt_buffer_data)

/**
 * VT_IOC_ACQUIRE_BUFFER - consumer dequeue the vframe info
 * takes an vt_buffer_data and returns it with buffer_tiem field
 * populated
 */
#define VT_IOC_ACQUIRE_BUFFER	_IOWR(VT_IOC_MAGIC, 6, \
				      struct vt_buffer_data)
#endif
