/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef THREAD_RW_H
#define THREAD_RW_H
#include "../../stream_input/amports/streambuf_reg.h"
#include "../../stream_input/amports/streambuf.h"
#include "../../stream_input/amports/amports_priv.h"

ssize_t threadrw_write(struct file *file,
		struct stream_buf_s *stbuf,
		const char __user *buf,
		size_t count);

void *threadrw_alloc(int num,
		int block_size,
			ssize_t (*write)(struct file *,
				struct stream_buf_s *,
				const char __user *,
				size_t, int),
				int flags);/*flags &1: manual mode*/

void threadrw_release(struct stream_buf_s *stbuf);

int threadrw_buffer_level(struct stream_buf_s *stbuf);
int threadrw_buffer_size(struct stream_buf_s *stbuf);
int threadrw_datafifo_len(struct stream_buf_s *stbuf);
int threadrw_freefifo_len(struct stream_buf_s *stbuf);
int threadrw_passed_len(struct stream_buf_s *stbuf);
int threadrw_flush_buffers(struct stream_buf_s *stbuf);
int threadrw_dataoffset(struct stream_buf_s *stbuf);
int threadrw_alloc_more_buffer_size(
	struct stream_buf_s *stbuf,
	int size);
int threadrw_support_more_buffers(struct stream_buf_s *stbuf);
void threadrw_update_buffer_level(struct stream_buf_s *stbuf,
	int parsed_size);
#endif
