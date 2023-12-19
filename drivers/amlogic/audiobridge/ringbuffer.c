// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/log2.h>
#include "ringbuffer.h"

static u32 roundup_power_of_2(unsigned int a)
{
	u32 i;
	u32 position = 0;

	if (a == 0)
		return 0;

	for (i = a; i != 0; i >>= 1)
		position++;

	return (u32)(1 << position);
}

struct ring_buffer *ring_buffer_init(u32 size)
{
	char *buf = NULL;
	struct ring_buffer *ring_buf = NULL;

	if (!is_power_of_2(size))
		size = roundup_power_of_2(size);

	buf = vmalloc(size);
	if (!buf)
		return ring_buf;

	ring_buf = vmalloc(sizeof(*ring_buf));
	if (!ring_buf) {
		vfree(buf);
		buf = NULL;
		pr_err("Failed to malloc rb struct!\n");
		return ring_buf;
	}

	memset(buf, 0, size);
	memset(ring_buf, 0, sizeof(struct ring_buffer));
	ring_buf->buffer = buf;
	ring_buf->size = size;
	ring_buf->in = 0;
	ring_buf->out = 0;
	ring_buf->go_empty = false;
	return ring_buf;
}

int ring_buffer_release(struct ring_buffer *ring_buf)
{
	if (ring_buf) {
		vfree(ring_buf->buffer);
		ring_buf->buffer = NULL;
		vfree(ring_buf);
		ring_buf = NULL;
	}
	return 0;
}

static u32 __ring_buffer_len(const struct ring_buffer *ring_buf)
{
	return(ring_buf->in - ring_buf->out);
}

static u32 __ring_buffer_get(struct ring_buffer *ring_buf, void *buffer, u32 size)
{
	u32 len = 0;

	if (!ring_buf || !buffer)
		return 0;

	size  = min(size, ring_buf->in - ring_buf->out);
	/* first get the data from fifo->out until the end of the buffer */
	len = min(size, ring_buf->size - (ring_buf->out & (ring_buf->size - 1)));
	memcpy(buffer, ring_buf->buffer + (ring_buf->out & (ring_buf->size - 1)), len);
	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(buffer + len, ring_buf->buffer, size - len);
	ring_buf->out += size;
	return size;
}

static u32 __ring_buffer_put(struct ring_buffer *ring_buf, void *buffer, u32 size)
{
	u32 len = 0;

	if (!ring_buf || !buffer)
		return 0;

	size = min(size, ring_buf->size - ring_buf->in + ring_buf->out);
	/* first put the data starting from fifo->in to buffer end */
	len  = min(size, ring_buf->size - (ring_buf->in & (ring_buf->size - 1)));
	memcpy(ring_buf->buffer + (ring_buf->in & (ring_buf->size - 1)), buffer, len);
	/* then put the rest (if any) at the beginning of the buffer */
	memcpy(ring_buf->buffer, buffer + len, size - len);
	ring_buf->in += size;
	return size;
}

u32 ring_buffer_len(const struct ring_buffer *ring_buf)
{
	return ring_buf->size;
}

u32 ring_buffer_used_len(const struct ring_buffer *ring_buf)
{
	return __ring_buffer_len(ring_buf);
}

u32 ring_buffer_free_len(const struct ring_buffer *ring_buf)
{
	return ring_buf->size - __ring_buffer_len(ring_buf);
}

u32 no_thread_safe_ring_buffer_get(struct ring_buffer *ring_buf, void *buffer, u32 size)
{
	u32 ret;

	if (ring_buf->go_empty) {
		ring_buf->go_empty = false;
		ring_buf->out = ring_buf->in;
	}
	if (ring_buffer_used_len(ring_buf) < size)
		return 0;
	ret = __ring_buffer_get(ring_buf, buffer, size);
	return ret;
}

u32 no_thread_safe_ring_buffer_put(struct ring_buffer *ring_buf, void *buffer, u32 size)
{
	u32 ret;

	if (ring_buf->go_empty) {
		ring_buf->go_empty = false;
		ring_buf->in = ring_buf->out;
	}
	ret = __ring_buffer_put(ring_buf, buffer, size);
	return ret;
}

u32 ring_buffer_go_empty(struct ring_buffer *ring_buf)
{
	ring_buf->go_empty = true;
	return 0;
}

