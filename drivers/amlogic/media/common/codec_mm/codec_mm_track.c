// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <linux/hash.h>
#include <linux/hashtable.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/fdtable.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/dma-buf.h>
#include <linux/genalloc.h>
#include <linux/mm.h>
#include <linux/dma-heap.h>
#include <linux/amlogic/meson_uvm_core.h>

#include "codec_mm_track_priv.h"

#define DBUF_TRACE_HASH_BITS	(10)

struct codec_mm_track_s {
	struct list_head leader_head;
	struct list_head module_head;

	DECLARE_HASHTABLE(dbuf_trace_table, DBUF_TRACE_HASH_BITS);
	spinlock_t trk_slock; /* Used for trk context locked. */
};

static inline u64 get_time_us(void)
{
	return div64_u64(local_clock(), 1000);
}

static ulong get_dbuf_addr(struct dma_buf *dbuf)
{
	struct dma_heap *heap = NULL;
	struct dma_buf_attachment *dba;
	struct sg_table *sgt;
	ulong addr;

	heap = dma_heap_find("system");
	if (!heap) {
		pr_err("%s: heap is NULL\n", __func__);
		return 0;
	}

	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, dma_heap_get_dev(heap));
	if (IS_ERR(dba)) {
		dma_heap_put(heap);
		pr_err("failed to attach dmabuf\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	sgt = dma_buf_map_attachment(dba, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		dma_heap_put(heap);
		dma_buf_detach(dbuf, dba);
		pr_err("Error getting dmabuf scatterlist\n");
		return 0;
	}

	addr = sg_dma_address(sgt->sgl);

	/* unmap attachment and detach dbuf */
	dma_buf_unmap_attachment(dba, sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, dba);
	dma_heap_put(heap);

	return addr;
}

struct file *find_next_fd_rcu(struct task_struct *task, u32 *ret_fd)
{
	/* Must be called with rcu_read_lock held */
	struct files_struct *files;
	u32 fd = *ret_fd;
	struct file *file = NULL;

	task_lock(task);

	files = task->files;
	if (files) {
		for (; fd < files_fdtable(files)->max_fds; fd++) {
			file = fcheck_files(files, fd);
			if (file)
				break;
		}
	}

	task_unlock(task);

	*ret_fd = fd;

	return file;
}

static bool find_match_file(struct task_struct *tsk, struct file *file)
{
	struct file *f;
	u32 fd = 0;
	bool found = false;

	if (!tsk)
		return -ENOENT;

	rcu_read_lock();

	for (;; fd++) {
		f = find_next_fd_rcu(tsk, &fd);
		if (!f)
			break;

		if (f == file) {
			pr_info("|   |-- FD:%4u, PID:%4d, comm: %s\n",
				fd, tsk->pid, tsk->comm);
			found = true;
		}
	}

	rcu_read_unlock();

	return found;
}

static void find_ref_process(const struct dma_buf *dbuf)
{
	struct task_struct *tsk = NULL;
	bool have_leaf = false;

	read_lock(&tasklist_lock);

	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		if (find_match_file(tsk, dbuf->file))
			have_leaf = true;
	}

	if (have_leaf)
		pr_info("|   |__ leaf end\n");

	read_unlock(&tasklist_lock);
}

static int walk_dbuf_callback(const struct dma_buf *dbuf, void *private)
{
	struct file *f = dbuf->file;

#ifdef CONFIG_AMLOGIC_UVM_CORE
	if (!dmabuf_is_uvm((struct dma_buf *)((ulong)dbuf)))
		return 0;
#endif
	pr_info("|-- exp-name:%s, addr:0x%lx, size:%zu, ino:%lu, Ref:%lu\n",
		dbuf->exp_name,
		get_dbuf_addr((struct dma_buf *)((ulong)dbuf)),
		dbuf->size,
		file_inode(f)->i_ino,
		file_count(f));

	find_ref_process(dbuf);

	return 0;
}

int codec_mm_walk_dbuf(void)
{
	int ret;

	pr_info("Dbuf walk.\n");

	ret = get_each_dmabuf(walk_dbuf_callback, NULL);

	pr_info("|__ walk end\n");

	return ret;
}

int codec_mm_track_open(void **trk_out)
{
	struct codec_mm_track_s *trk;

	trk = vzalloc(sizeof(*trk));
	if (!trk)
		return -ENOMEM;

	spin_lock_init(&trk->trk_slock);
	hash_init(trk->dbuf_trace_table);
	INIT_LIST_HEAD(&trk->leader_head);
	INIT_LIST_HEAD(&trk->module_head);

	*trk_out = trk;

	return 0;
}

void codec_mm_track_close(void *trk_h)
{
	struct codec_mm_track_s *trk = trk_h;

	vfree(trk);
}

MODULE_IMPORT_NS(MINIDUMP);

