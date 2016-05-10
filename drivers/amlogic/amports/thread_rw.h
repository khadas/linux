#ifndef THREAD_RW_H
#define THREAD_RW_H
#include "streambuf_reg.h"
#include "streambuf.h"
#include "esparser.h"
#include "amports_priv.h"

ssize_t threadrw_write(struct file *file,
		struct stream_buf_s *stbuf,
		const char __user *buf,
		size_t count);

void *threadrw_alloc(int num,
		int block_size,
			ssize_t (*write)(struct file *,
					struct stream_buf_s *,
					const char __user *,
					size_t, int));
void threadrw_release(struct stream_buf_s *stbuf);

int threadrw_buffer_level(struct stream_buf_s *stbuf);
int threadrw_buffer_size(struct stream_buf_s *stbuf);
int threadrw_datafifo_len(struct stream_buf_s *stbuf);
int threadrw_freefifo_len(struct stream_buf_s *stbuf);
int threadrw_passed_len(struct stream_buf_s *stbuf);
int threadrw_flush_buffers(struct stream_buf_s *stbuf);
int threadrw_dataoffset(struct stream_buf_s *stbuf);

#endif
