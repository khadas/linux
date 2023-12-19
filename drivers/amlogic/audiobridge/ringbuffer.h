/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _USER_RING_BUFFER_H
#define _USER_RING_BUFFER_H

struct ring_buffer {
	void *buffer;
	u32 size;
	u32 in;
	u32 out;
	u8 go_empty;
};

struct ring_buffer *ring_buffer_init(u32 size);
int ring_buffer_release(struct ring_buffer *ring_buf);
u32 ring_buffer_len(const struct ring_buffer *ring_buf);
u32 ring_buffer_used_len(const struct ring_buffer *ring_buf);
u32 ring_buffer_free_len(const struct ring_buffer *ring_buf);
u32 no_thread_safe_ring_buffer_get(struct ring_buffer *ring_buf, void *buffer, u32 size);
u32 no_thread_safe_ring_buffer_put(struct ring_buffer *ring_buf, void *buffer, u32 size);
u32 ring_buffer_go_empty(struct ring_buffer *ring_buf);

#endif
