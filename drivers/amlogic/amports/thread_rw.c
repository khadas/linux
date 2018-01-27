/*
 * drivers/amlogic/amports/thread_rw.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

/* #include <mach/am_regs.h> */
#include <linux/delay.h>

#include "streambuf.h"
#include "amports_priv.h"
#include "thread_rw.h"

#define BUF_NAME "fetchbuf"

#define DEFAULT_BLOCK_SIZE (64*1024)

struct threadrw_buf {
	void *vbuffer;
	dma_addr_t dma_handle;
	int write_off;
	int data_size;
	int buffer_size;
	int from_cma;
};

#define MAX_MM_BUFFER_NUM 16
struct threadrw_write_task {
	struct file *file;
	struct delayed_work write_work;
	DECLARE_KFIFO_PTR(datafifo, void *);
	DECLARE_KFIFO_PTR(freefifo, void *);
	int bufs_num;
	int max_bufs;
	int errors;
	spinlock_t lock;
	struct mutex mutex;
	struct stream_buf_s *sbuf;
	int buffered_data_size;
	int passed_data_len;
	int buffer_size;
	int def_block_size;
	int data_offset;
	int writework_on;
	unsigned long codec_mm_buffer[MAX_MM_BUFFER_NUM];
	int manual_write;
	int failed_onmore;
	wait_queue_head_t wq;
	ssize_t (*write)(struct file *,
		struct stream_buf_s *,
		const char __user *,
		size_t, int);
	struct threadrw_buf buf[1];
	/*don't add any after buf[] define */
};

static int free_task_buffers(struct threadrw_write_task *task);

static struct workqueue_struct *threadrw_wq_get(void)
{
	static struct workqueue_struct *threadrw_wq;
	if (!threadrw_wq)
		threadrw_wq = create_singlethread_workqueue("threadrw");
	return threadrw_wq;
}

static int threadrw_schedule_delayed_work(
		struct threadrw_write_task *task,
		unsigned long delay)
{
	bool ret;
	if (threadrw_wq_get()) {
		ret = queue_delayed_work(threadrw_wq_get(),
			&task->write_work, delay);
	} else
		ret = schedule_delayed_work(&task->write_work, delay);
	if (!ret) {
		cancel_delayed_work(&task->write_work);
		if (threadrw_wq_get())
			ret = queue_delayed_work(threadrw_wq_get(),
					&task->write_work, 0);
		else
			ret = schedule_delayed_work(&task->write_work, 0);
	}
	return 0;
}

static ssize_t threadrw_write_onece(
	struct threadrw_write_task *task,
	struct file *file,
	struct stream_buf_s *stbuf,
	const char __user *buf, size_t count)
{
	struct threadrw_buf *rwbuf = NULL;
	int ret = 0;
	int to_write;

	if (!kfifo_get(&task->freefifo, (void *)&rwbuf)) {
		if (task->errors)
			return task->errors;
		return -EAGAIN;
	}

	to_write = min_t(u32, rwbuf->buffer_size, count);
	if (copy_from_user(rwbuf->vbuffer, buf, to_write)) {
		kfifo_put(&task->freefifo, (const void *)buf);
		ret = -EFAULT;
		goto err;
	}
	rwbuf->data_size = to_write;
	rwbuf->write_off = 0;
	kfifo_put(&task->datafifo, (const void *)rwbuf);
	threadrw_schedule_delayed_work(task, 0);
	return to_write;
err:
	return ret;
}

static ssize_t threadrw_write_in(
	struct threadrw_write_task *task,
	struct stream_buf_s *stbuf,
	const char __user *buf, size_t count)
{
	int ret = 0;
	int off = 0;
	int left = count;
	int wait_num = 0;
	unsigned long flags;

	while (left > 0) {
		ret = threadrw_write_onece(task,
				task->file,
				stbuf, buf + off, left);
		if (ret >= left) {
			off = count;
			left = 0;
		} else if (ret > 0) {
			off += ret;
			left -= ret;

		} else if (ret < 0) {
			if (off > 0) {
				break;	/*have write ok some data. */
			} else if (ret == -EAGAIN) {
				if (!(task->file->f_flags & O_NONBLOCK) &&
					(++wait_num < 10)) {
					wait_event_interruptible_timeout(
						task->wq,
						!kfifo_is_empty(
							&task->freefifo),
						HZ / 100);
					continue;	/* write again. */
				}
				ret = -EAGAIN;
				break;
			}
			break;	/*to end */
		}
	}

	/*end: */
	spin_lock_irqsave(&task->lock, flags);
	if (off > 0) {
		task->buffered_data_size += off;
		task->data_offset += off;
	}
	spin_unlock_irqrestore(&task->lock, flags);
	if (off > 0)
		return off;
	else
		return ret;
}

static int do_write_work_in(struct threadrw_write_task *task)
{
	struct threadrw_buf *rwbuf = NULL;
	int ret;
	int need_re_write = 0;
	int write_len = 0;
	unsigned long flags;

	if (kfifo_is_empty(&task->datafifo))
		return 0;
	if (!kfifo_peek(&task->datafifo, (void *)&rwbuf))
		return 0;
	if (!task->manual_write &&
			rwbuf->from_cma &&
			!rwbuf->write_off)
		codec_mm_dma_flush(rwbuf->vbuffer,
						rwbuf->buffer_size,
						DMA_TO_DEVICE);
	if (task->manual_write) {
		ret = task->write(task->file, task->sbuf,
			(const char __user *)rwbuf->vbuffer + rwbuf->write_off,
			rwbuf->data_size,
			2);	/* noblock,virtual addr */
	} else {
		ret = task->write(task->file, task->sbuf,
		(const char __user *)rwbuf->dma_handle + rwbuf->write_off,
		rwbuf->data_size,
		3);	/* noblock,phy addr */
	}
	if (ret == -EAGAIN) {
		need_re_write = 0;
		/*do later retry. */
	} else if (ret >= rwbuf->data_size) {
		write_len += rwbuf->data_size;
		if (kfifo_get(&task->datafifo, (void *)&rwbuf)) {
			rwbuf->data_size = 0;
			kfifo_put(&task->freefifo, (const void *)rwbuf);
			/*wakeup write thread. */
			wake_up_interruptible(&task->wq);
		} else
			pr_err("write ok,but kfifo_get data failed.!!!\n");
		need_re_write = 1;
	} else if (ret > 0) {
		rwbuf->data_size -= ret;	/* half data write */
		rwbuf->write_off += ret;
		write_len += ret;
		need_re_write = 1;
	} else {		/*ret <=0 */
		pr_err("get errors ret=%d size=%d\n", ret,
			rwbuf->data_size);
		task->errors = ret;
	}
	if (write_len > 0) {
		spin_lock_irqsave(&task->lock, flags);
		task->passed_data_len += write_len;
		task->buffered_data_size -= write_len;
		spin_unlock_irqrestore(&task->lock, flags);
	}
	return need_re_write;

}

static void do_write_work(struct work_struct *work)
{
	struct threadrw_write_task *task = container_of(work,
					struct threadrw_write_task,
					write_work.work);
	int need_retry = 1;
	task->writework_on = 1;
	while (need_retry) {
		mutex_lock(&task->mutex);
		need_retry = do_write_work_in(task);
		mutex_unlock(&task->mutex);
	}
	threadrw_schedule_delayed_work(task, HZ / 10);
	task->writework_on = 0;
	return;
}

static int alloc_task_buffers_inlock(struct threadrw_write_task *task,
		int new_bubffers,
		int block_size)
{
	struct threadrw_buf *rwbuf;
	int i;
	int used_codec_mm = task->manual_write ? 0 : 1;
	int new_num = new_bubffers;
	int mm_slot = -1;
	int start_idx = task->bufs_num;
	int total_mm = 0;
	unsigned long addr;

	if (codec_mm_get_total_size() < 80 ||
		codec_mm_get_free_size() < 40)
		used_codec_mm = 0;
	if (task->bufs_num + new_num > task->max_bufs)
		new_num = task->max_bufs - task->bufs_num;
	for (i = 0; i < MAX_MM_BUFFER_NUM; i++) {
		if (task->codec_mm_buffer[i] == 0) {
			mm_slot = i;
			break;
		}
	}
	if (mm_slot < 0)
		used_codec_mm = 0;
	if (block_size <= 0)
		block_size = DEFAULT_BLOCK_SIZE;

	if (used_codec_mm && (block_size * new_num) >= 128 * 1024) {
		total_mm = ALIGN(block_size * new_num, (1 << 17));
		addr =
				codec_mm_alloc_for_dma(BUF_NAME,
					total_mm / PAGE_SIZE, 0,
					CODEC_MM_FLAGS_DMA_CPU);
		if (addr != 0) {
			task->codec_mm_buffer[mm_slot] = addr;
			task->buffer_size += total_mm;
		} else {
			used_codec_mm = 0;
		}
	}
	for (i = 0; i < new_num; i++) {
		int bufidx = start_idx + i;
		rwbuf = &task->buf[bufidx];
		rwbuf->buffer_size = block_size;
		if (used_codec_mm) {
			unsigned long start_addr =
					task->codec_mm_buffer[mm_slot];
			if (i == new_num - 1)
				rwbuf->buffer_size = total_mm -
						block_size * i;
			rwbuf->dma_handle = (dma_addr_t) start_addr +
						block_size * i;
			rwbuf->vbuffer = codec_mm_phys_to_virt(
						rwbuf->dma_handle);
			rwbuf->from_cma = 1;

		} else {
			rwbuf->vbuffer = dma_alloc_coherent(
					amports_get_dma_device(),
					rwbuf->buffer_size,
					&rwbuf->dma_handle, GFP_KERNEL);
			if (!rwbuf->vbuffer) {
				rwbuf->buffer_size = 0;
				rwbuf->dma_handle = 0;
				task->bufs_num = bufidx;
				break;
			}
			rwbuf->from_cma = 0;
			task->buffer_size += rwbuf->buffer_size;
		}

		kfifo_put(&task->freefifo, (const void *)rwbuf);
		task->bufs_num = bufidx + 1;
	}
	if (start_idx > 0 ||/*have buffers before*/
		task->bufs_num >= 3 ||
		task->bufs_num == new_num) {
		if (!task->def_block_size)
			task->def_block_size = task->buf[0].buffer_size;
		return 0;	/*must >=3 for swap buffers. */
	}
	if (task->bufs_num > 0)
		free_task_buffers(task);
	return -1;
}

static int free_task_buffers(struct threadrw_write_task *task)
{
	int i;
	for (i = 0; i < MAX_MM_BUFFER_NUM; i++) {
		if (task->codec_mm_buffer[i])
			codec_mm_free_for_dma(BUF_NAME,
				task->codec_mm_buffer[i]);
	}
	for (i = 0; i < task->bufs_num; i++) {
		if (task->buf[i].vbuffer && task->buf[i].from_cma == 0)
			dma_free_coherent(amports_get_dma_device(),
				task->buf[i].buffer_size,
				task->buf[i].vbuffer,
				task->buf[i].dma_handle);
	}
	return 0;
}

static struct threadrw_write_task *threadrw_alloc_in(int num,
		int block_size,
		ssize_t (*write)(struct file *,
			struct stream_buf_s *,
			const char __user *, size_t, int),
			int flags)
{
	int max_bufs = num;
	int task_buffer_size;
	struct threadrw_write_task *task;
	int ret;

	if (!(flags & 1)) /*not audio*/
		max_bufs = 300; /*can great for video bufs.*/
	task_buffer_size = sizeof(struct threadrw_write_task) +
				sizeof(struct threadrw_buf) * max_bufs;
	task = vmalloc(task_buffer_size);

	if (!task)
		return NULL;
	memset(task, 0, task_buffer_size);

	spin_lock_init(&task->lock);
	mutex_init(&task->mutex);
	INIT_DELAYED_WORK(&task->write_work, do_write_work);
	init_waitqueue_head(&task->wq);
	ret = kfifo_alloc(&task->datafifo, max_bufs, GFP_KERNEL);
	if (ret)
		goto err1;
	ret = kfifo_alloc(&task->freefifo, max_bufs, GFP_KERNEL);
	if (ret)
		goto err2;
	task->write = write;
	task->file = NULL;
	task->buffer_size = 0;
	task->manual_write = flags & 1;
	task->max_bufs = max_bufs;
	mutex_lock(&task->mutex);
	ret = alloc_task_buffers_inlock(task, num, block_size);
	mutex_unlock(&task->mutex);
	if (ret < 0)
		goto err3;
	threadrw_wq_get();	/*start thread. */
	return task;

err3:
	kfifo_free(&task->freefifo);
err2:
	kfifo_free(&task->datafifo);
err1:
	vfree(task);
	pr_err("alloc threadrw failed num:%d,block:%d\n", num, block_size);
	return NULL;
}

/*
fifo data size;
*/

int threadrw_buffer_level(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (task)
		return task->buffered_data_size;
	return 0;
}

int threadrw_buffer_size(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (task)
		return task->buffer_size;
	return 0;
}

int threadrw_datafifo_len(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (task)
		return kfifo_len(&task->datafifo);
	return 0;
}

int threadrw_freefifo_len(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (task)
		return kfifo_len(&task->freefifo);
	return 0;
}
int threadrw_support_more_buffers(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (!task)
		return 0;
	if (task->failed_onmore)
		return 0;
	return task->max_bufs - task->bufs_num;
}

/*
data len out fifo;
*/
int threadrw_passed_len(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (task)
		return task->passed_data_len;
	return 0;

}
/*
all data writed.;
*/
int threadrw_dataoffset(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	int offset = 0;
	if (task)
		return task->data_offset;
	return offset;

}

ssize_t threadrw_write(struct file *file, struct stream_buf_s *stbuf,
					   const char __user *buf, size_t count)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	ssize_t size;
	if (!task->file) {
		task->file = file;
		task->sbuf = stbuf;
	}
	mutex_lock(&task->mutex);
	size = threadrw_write_in(task, stbuf, buf, count);
	mutex_unlock(&task->mutex);
	return size;
}

int threadrw_flush_buffers(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	int max_retry = 20;
	if (!task)
		return 0;
	while (!kfifo_is_empty(&task->datafifo) && max_retry-- > 0) {
		threadrw_schedule_delayed_work(task, 0);
		msleep(20);
	}
	if (!kfifo_is_empty(&task->datafifo))
		return -1;/*data not flushed*/
	return 0;
}
int threadrw_alloc_more_buffer_size(
	struct stream_buf_s *stbuf,
	int size)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	int block_size;
	int new_num;
	int ret = -1;
	int old_num;

	if (!task)
		return -1;
	mutex_lock(&task->mutex);
	block_size = task->def_block_size;
	if (block_size == 0)
		block_size = 32 * 1024;
	new_num = size / block_size;
	old_num = task->bufs_num;
	if (new_num == 0)
		new_num = 1;
	else if (new_num > task->max_bufs - task->bufs_num)
		new_num = task->max_bufs - task->bufs_num;
	if (new_num != 0)
		ret = alloc_task_buffers_inlock(task, new_num,
			block_size);
	mutex_unlock(&task->mutex);
	pr_info("threadrw add more buffer from %d -> %d for size %d\n",
		old_num, task->bufs_num,
		size);
	if (ret < 0 || old_num == task->bufs_num)
		task->failed_onmore = 1;
	return ret;
}

void *threadrw_alloc(int num,
		int block_size,
			ssize_t (*write)(struct file *,
				struct stream_buf_s *,
				const char __user *,
				size_t, int),
				int flags)
{
	return threadrw_alloc_in(num, block_size, write, flags);
}

void threadrw_release(struct stream_buf_s *stbuf)
{
	struct threadrw_write_task *task = stbuf->write_thread;
	if (task) {
		wake_up_interruptible(&task->wq);
		cancel_delayed_work_sync(&task->write_work);
		mutex_lock(&task->mutex);
		free_task_buffers(task);
		mutex_unlock(&task->mutex);
		kfifo_free(&task->freefifo);
		kfifo_free(&task->datafifo);
		vfree(task);
	}
	stbuf->write_thread = NULL;
	return;
}
