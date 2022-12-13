/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 */

#ifndef __VIDEO_TUNNEL_H
#define __VIDEO_TUNNEL_H

struct vt_session;

/**
 * vt_session_create() - allocate a vt_session and return it
 * @name:	used for debugging
 */
struct vt_session *vt_session_create(const char *name);

/**
 * vt_session_destroy() - free's a vt_session and it's all vt_buffers
 * @session:	the vt_session
 *
 * Free the provided session and all it's resources.
 */
void vt_session_destroy(struct vt_session *session);

/**
 * vt_alloc_id() - allocate an unique videotunnel id
 * @session:	the vt_session
 * @tunnel_id:	tunnel id to return
 *
 * Return of a value other than 0 means an error has occurred.
 */
int vt_alloc_id(struct vt_session *session, int *tunnel_id);

/**
 * vt_free_id() - free tunnel id allocated previous
 * @session:	the vt_session
 * @tunnel_id:	tunnel id to free
 *
 * Return of a value other than 0 means an error has occurred.
 */
int vt_free_id(struct vt_session *session, int tunnel_id);

/**
 * vt_producer_connect() - producer connect to a specific videotunnel
 * @session:	the vt_session
 * @tunnel_id:	tunnel id on which to connect
 *
 * Before queue/dequeue bufer, producer need connect to videotunnel.
 * Return of a value other than 0 means an error has occurred.
 */
int vt_producer_connect(struct vt_session *session, int tunnel_id);

/**
 * vt_producer_disconnect - producer disconnect to a specific videotunnel
 * @session:	the vt_session
 * @tunnel_id:	tunnel id on which to disconnect
 *
 * All the videotunnel buffer on the disconnected side will be freed.
 * Return of a value other than 0 means an error has occurred.
 */
int vt_producer_disconnect(struct vt_session *session, int tunnel_id);

/**
 * vt_queue_buffer - transfer ownership of the buffer to consumer
 * @session:	the vt_session
 * @tunnel_id:	tunnel id on which to queuebuffer
 * @buffer_fd:	buffer fd to transfer
 * @fence_fd:	acquire fence, not used currently
 *
 * Return of a value other than 0 means an error has occurred.
 */
int vt_queue_buffer(struct vt_session *session, int tunnel_id,
			   struct file *buffer_file,
			   int fence_fd, int64_t time_stamp);

/**
 * vt_dequeue_buffer -requests a buffer from videotunnel to use.
 * @session:	the vt_session
 * @tunnel_id:	tunnel id on which to dequeuebuffer
 * @buffer_fd:	buffer fd to get from videotunnel
 * @fence_fd:	release fence not used currently, waited in vt driver
 *
 * Ownership of the buffer is transferred to the producer. The returned
 * buffer is previous queued use vt_queue_buffer. If no buffer is pending,
 * it returns -EAGAIN with a default 4ms timeout.
 */
int vt_dequeue_buffer(struct vt_session *session, int tunnel_id,
			     struct file **buffer_file, struct file **fence_file);

/**
 * vt_send_cmd - send videotunnel cmd to server
 * @session:	the vt_session
 * @tunnel_id:	tunnel id on which to send cmd
 * @cmd:	vt cmd
 * @cmd_data:	dmc data
 */
int vt_send_cmd(struct vt_session *session, int tunnel_id,
		       enum vt_video_cmd_e cmd, int cmd_data);

#endif /* __VIDEO_TUNNEL_H */
