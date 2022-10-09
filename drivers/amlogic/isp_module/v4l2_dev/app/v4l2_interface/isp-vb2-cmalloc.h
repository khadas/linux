/*
 * videobuf2-vmalloc.h - vmalloc memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef _MEDIA_VIDEOBUF2_CMAALLOC_H
#define _MEDIA_VIDEOBUF2_CMAALLOC_H

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>

extern const struct vb2_mem_ops vb2_cmalloc_memops;

struct vb2_cmalloc_buf {
	void				*vaddr;
	struct frame_vector		*vec;
	enum dma_data_direction		dma_dir;
	unsigned long			size;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	refcount_t		    refcount;
#else
    atomic_t			refcount;
#endif
	struct vb2_vmarea_handler	handler;
	struct dma_buf			*dbuf;
};


#endif
