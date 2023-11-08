// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
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
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/amlogic/media/codec_mm/codec_mm_state.h>

#include "codec_mm_track_priv.h"
#include "codec_mm_track_kps.h"

#define DBUF_TRACK_TYPE_UVM	(0x1)
#define DBUF_TRACK_TYPE_ES	(0x2)
#define DBUF_TRACK_TYPE_HEAP	(0x4)

#define DBUF_TRACE_HASH_BITS	(10)
#define DBUF_TRACE_ELEMS_NUM	(1024)

static u32 dbuf_track_type_flag = DBUF_TRACK_TYPE_UVM;

struct trace_elem {
	struct list_head	node;
	struct hlist_node	h_node;
	bool			b_head;

	struct task_struct	*task;
	u8			comm[TASK_COMM_LEN];
	pid_t			gid;
	pid_t			pid;
	struct file		*file;
	u32			fd;
	u64			ts;
};

struct trace_pool {
	struct list_head	elem_head;
	u32			elem_cnt;
	spinlock_t		slock; /* Used for elem pool locked. */
};

struct codec_mm_track_s {
	void			*kps_h;
	struct trace_pool	pool;

	u32			sample_cnt;
	DECLARE_HASHTABLE(sample_table, DBUF_TRACE_HASH_BITS);
	spinlock_t trk_slock; /* Used for trk context locked. */

	struct codec_state_node cs;
};

static bool is_need_track(const struct dma_buf *d);

static int trace_pool_init(struct trace_pool *pool);

static void trace_elems_walk(struct codec_mm_track_s *trk);

static bool trace_elem_lookup(struct codec_mm_track_s *trk,
			     struct task_struct *tsk,
			     struct file *file,
			     u32 fd,
			     struct trace_elem *out);

static bool is_fd_alive(u32 fd)
{
	struct files_struct *files = current->files;
	struct fdtable *fdt = files_fdtable(files);

	return test_bit(fd, fdt->open_fds);
}

static bool is_kprobes_enable(struct codec_mm_track_s *trk)
{
	return !!trk->kps_h;
}

static char *ts_to_string(u64 ts, char *buf)
{
	ulong rem_nsec;

	if (!ts)
		ts = local_clock();

	rem_nsec = do_div(ts, 1000000000);

	sprintf(buf, "[%5lu.%06lu]", (ulong)ts, rem_nsec / 1000);

	return buf;
}

static inline void kp_info_show(const char *fname, struct file *file, u32 fd)
{
	pr_debug("Kps show %s -- FD:%u, file:%px, %s-TSK(%d/%s, leader:%d/%s)\n",
		fname,
		fd,
		file,
		user_mode(current_pt_regs()) ? "User" : "Kernel",
		current->pid,
		current->comm,
		current->group_leader->pid,
		current->group_leader->comm);
}

static struct codec_mm_track_s *get_track_ctx(void)
{
	static struct codec_mm_track_s trk;
	static bool inited;
	int ret;

	if (!inited) {
		spin_lock_init(&trk.trk_slock);
		hash_init(trk.sample_table);

		ret = trace_pool_init(&trk.pool);
		if (ret < 0) {
			pr_err("%s trace pool init fail.\n", __func__);
			goto err;
		}

		inited = true;
	}

	return &trk;
err:
	return NULL;
};

static int trace_elems_alloc(struct trace_pool *pool)
{
	struct trace_elem *elems;
	ulong flags;
	int i;

	elems = kmalloc_array(DBUF_TRACE_ELEMS_NUM,
			     sizeof(struct trace_elem), GFP_ATOMIC);
	if (!elems)
		return -ENOMEM;

	spin_lock_irqsave(&pool->slock, flags);

	for (i = 0; i < DBUF_TRACE_ELEMS_NUM; i++) {
		INIT_LIST_HEAD(&elems[i].node);
		INIT_HLIST_NODE(&elems[i].h_node);
		elems[i].b_head	= false;
		elems[i].pid	= -1;
		elems[i].ts	= 0;
		pool->elem_cnt++;

		list_add(&elems[i].node, &pool->elem_head);
	}

	elems[0].b_head = true;

	spin_unlock_irqrestore(&pool->slock, flags);

	return 0;
}

static int trace_pool_init(struct trace_pool *pool)
{
	int ret = -1;

	spin_lock_init(&pool->slock);
	INIT_LIST_HEAD(&pool->elem_head);
	pool->elem_cnt = 0;

	ret = trace_elems_alloc(pool);
	if (ret)
		pr_err("Alloc trace elems fail.\n");

	return ret;
}

static void trace_pool_release(struct trace_pool *pool)
{
	struct trace_elem *elem, *tmp;
	ulong flags;

	spin_lock_irqsave(&pool->slock, flags);

	list_for_each_entry_safe(elem, tmp, &pool->elem_head, node) {
		if (elem->b_head)
			continue;

		list_del(&elem->node);
	}

	list_for_each_entry_safe(elem, tmp, &pool->elem_head, node) {
		list_del(&elem->node);
		kfree(elem);
	}

	pool->elem_cnt = 0;

	spin_unlock_irqrestore(&pool->slock, flags);
}

static struct trace_elem *trace_elem_alloc(struct trace_pool *pool)
{
	struct trace_elem *elem = NULL;
	ulong flags;
	int ret = -1;

retry:
	spin_lock_irqsave(&pool->slock, flags);

	if (list_empty(&pool->elem_head))
		goto out;

	elem = list_first_entry(&pool->elem_head, struct trace_elem, node);
	list_del(&elem->node);
	pool->elem_cnt--;
out:
	spin_unlock_irqrestore(&pool->slock, flags);

	if (!elem) {
		ret = trace_elems_alloc(pool);
		if (ret)
			pr_err("Alloc trace elems fail.\n");
		else
			goto retry;
	}

	return elem;
}

static void trace_elem_free(struct trace_pool *pool, struct trace_elem *elem)
{
	ulong flags;

	spin_lock_irqsave(&pool->slock, flags);

	pool->elem_cnt++;
	list_add(&elem->node, &pool->elem_head);

	spin_unlock_irqrestore(&pool->slock, flags);
}

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

	heap = dma_heap_find("system-uncached");
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

static bool find_match_file(struct task_struct *tsk,
			   struct file *file,
			   struct seq_file *m)
{
	struct codec_mm_track_s *trk = get_track_ctx();
	struct trace_elem elem;
	struct file *f;
	u32 fd = 0;
	bool found = false;
	char sbuf[32] = { 0 };

	if (!tsk || !trk)
		return -ENOENT;

	rcu_read_lock();

	for (;; fd++) {
		f = find_next_fd_rcu(tsk, &fd);
		if (!f)
			break;

		if (f == file) {
			cs_printf(m, "|   |-- FD:%4u, PID:%4d, comm: %s, leader: %s\n",
				fd, tsk->pid, tsk->comm,
				tsk->group_leader->comm);
			found = true;

			if (!is_kprobes_enable(trk))
				continue;

			if (trace_elem_lookup(trk, tsk, file, fd, &elem)) {
				cs_printf(m, "%s TID:%4d, comm: %s, ts: %s, elapse: %lld ms\n",
					"|   |      |____",
					elem.pid, elem.comm,
					ts_to_string(elem.ts, sbuf),
					div64_u64(local_clock() - elem.ts, 1000000));
			} else {
				trace_elems_walk(trk);
			}
		}
	}

	rcu_read_unlock();

	return found;
}

static void find_ref_process(const struct dma_buf *dbuf, struct seq_file *m)
{
	struct task_struct *tsk = NULL;
	bool have_leaf = false;

	read_lock(&tasklist_lock);

	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		if (find_match_file(tsk, dbuf->file, m))
			have_leaf = true;
	}

	if (have_leaf)
		cs_printf(m, "|   |__ leaf end\n");

	read_unlock(&tasklist_lock);
}

static bool is_need_track(const struct dma_buf *d)
{
	struct dma_buf *dbuf = (struct dma_buf *)((ulong)d);

#ifdef CONFIG_AMLOGIC_UVM_CORE
	if ((dbuf_track_type_flag & DBUF_TRACK_TYPE_UVM) &&
		dmabuf_is_uvm(dbuf))
		return true;
#endif

#ifdef CONFIG_AMLOGIC_HEAP_CODEC_MM
	if ((dbuf_track_type_flag & DBUF_TRACK_TYPE_HEAP) &&
		dmabuf_is_codec_mm_heap_buf(dbuf))
		return true;
#endif

	if ((dbuf_track_type_flag & DBUF_TRACK_TYPE_ES) &&
		dmabuf_is_esbuf(dbuf))
		return true;

	//Add more...

	return false;
}

static int walk_dbuf_callback(const struct dma_buf *dbuf, void *private)
{
	struct file *f = dbuf->file;
	struct seq_file *m = private;

	if (!is_need_track(dbuf))
		return 0;

	cs_printf(m, "|-- exp-name:%s, addr:0x%lx, size:%zu, ino:%lu, Ref:%lu\n",
		dbuf->exp_name,
		get_dbuf_addr((struct dma_buf *)((ulong)dbuf)),
		dbuf->size,
		file_inode(f)->i_ino,
		file_count(f));

	find_ref_process(dbuf, m);

	return 0;
}

void codec_mm_dbuf_dump_config(u32 type)
{
	dbuf_track_type_flag = type;

	if (dbuf_track_type_flag & DBUF_TRACK_TYPE_UVM)
		pr_info("UVM dmabuf will be tracked.\n");

	if (dbuf_track_type_flag & DBUF_TRACK_TYPE_ES)
		pr_info("ES dmabuf will be tracked.\n");

	if (dbuf_track_type_flag & DBUF_TRACK_TYPE_HEAP)
		pr_info("Codec mm heap dmabuf will be tracked.\n");

	if (!dbuf_track_type_flag)
		pr_info("Disable dmabuf tracking.\n");
}

int codec_mm_dbuf_walk(struct seq_file *m)
{
	int ret;

	cs_printf(m, "Dbuf walk type:%x.\n", dbuf_track_type_flag);

	ret = get_each_dmabuf(walk_dbuf_callback, m);

	cs_printf(m, "|__ walk end\n");

	return ret;
}

static void trace_elems_walk(struct codec_mm_track_s *trk)
{
	struct trace_elem *elem = NULL;
	ulong bkt_task;
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	hash_for_each(trk->sample_table, bkt_task, elem, h_node) {
		pr_debug("%s, FD:%u, PID:%d, GID:%d, file:%px, comm:%s\n",
			__func__,
			elem->fd,
			elem->pid,
			elem->gid,
			elem->file,
			elem->comm);
	}

	spin_unlock_irqrestore(&trk->trk_slock, flags);
}

static void trace_elem_fill(struct trace_elem *elem,
			   struct file *file,
			   u32 fd)
{
	elem->task	= current;
	elem->pid	= current->pid;
	elem->gid	= current->group_leader->pid;
	elem->file	= file;
	elem->fd	= fd;
	elem->ts	= local_clock();

	get_task_comm(elem->comm, current);
}

static void trace_elem_sampling(struct codec_mm_track_s *trk,
			      struct file *file,
			      u32 fd)
{
	struct trace_elem *elem = NULL;
	struct hlist_node *tmp;
	pid_t pid = current->pid;
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	hash_for_each_possible_safe(trk->sample_table, elem, tmp, h_node, pid) {
		if (pid == elem->pid &&
			fd == elem->fd) {
			trace_elem_fill(elem, file, fd);

			pr_debug("Trace elem reuse, PID:%d, file:%px, FD:%u, ts:%llu, cnt:%u\n",
				elem->pid,
				elem->file,
				elem->fd,
				elem->ts,
				trk->sample_cnt);
			goto out;
		}
	}

	elem = trace_elem_alloc(&trk->pool);
	if (!elem) {
		pr_err("%s, Alloc trace elem fail.\n", __func__);
		goto out;
	}

	trace_elem_fill(elem, file, fd);

	get_task_struct(elem->task);
	trk->sample_cnt++;

	hash_add(trk->sample_table, &elem->h_node, pid);

	pr_debug("Trace elem add, PID:%d, file:%px, FD:%u, ts:%llu, cnt:%u\n",
		elem->pid,
		elem->file,
		elem->fd,
		elem->ts,
		trk->sample_cnt);
out:
	spin_unlock_irqrestore(&trk->trk_slock, flags);
}

static bool find_match_task(struct codec_mm_track_s *trk,
			 struct file *file,
			 u32 fd,
			 pid_t pid,
			 struct trace_elem **elem_out)
{
	struct trace_elem *elem = NULL;

	hash_for_each_possible(trk->sample_table, elem, h_node, pid) {
		if (file == elem->file &&
			fd == elem->fd) {
			*elem_out = elem;

			return true;
		}
	}

	return false;
}

static bool trace_elem_lookup(struct codec_mm_track_s *trk,
			   struct task_struct *task,
			   struct file *file,
			   u32 fd,
			   struct trace_elem *out)
{
	bool found = false;
	struct task_struct *tsk = NULL;
	struct trace_elem *elem = NULL;
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	if (find_match_task(trk, file, fd, task->pid, &elem)) {
		*out = *elem;
		found = true;

		goto out;
	}

	for_each_thread(task->group_leader, tsk) {
		if (find_match_task(trk, file, fd, tsk->pid, &elem)) {
			*out = *elem;
			found = true;

			goto out;
		}
	}
out:
	spin_unlock_irqrestore(&trk->trk_slock, flags);

	return found;
}

static void __trace_sampling_del(struct codec_mm_track_s *trk,
				       struct trace_elem *elem)
{
	pr_debug("Trace elem del, PID:%d, file:%px, FD:%u, ts:%llu, cnt:%u\n",
		elem->pid,
		elem->file,
		elem->fd,
		elem->ts,
		trk->sample_cnt);

	trk->sample_cnt--;
	put_task_struct(elem->task);

	hash_del(&elem->h_node);

	trace_elem_free(&trk->pool, elem);
}

static void __trace_elem_clean(struct codec_mm_track_s *trk,
			      struct file *file,
			      bool all)
{
	struct trace_elem *elem = NULL;
	struct hlist_node *tmp;
	ulong bucket;

	hash_for_each_safe(trk->sample_table, bucket, tmp, elem, h_node) {
		if (all || file == elem->file)
			__trace_sampling_del(trk, elem);
	}
}

static void trace_elem_clean(struct codec_mm_track_s *trk,
			    struct file *file)
{
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	__trace_elem_clean(trk, file, false);

	spin_unlock_irqrestore(&trk->trk_slock, flags);
}

static void trace_elem_clean_all(struct codec_mm_track_s *trk)
{
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	__trace_elem_clean(trk, NULL, true);

	spin_unlock_irqrestore(&trk->trk_slock, flags);
}

static void trace_clean_invalid_fd(struct codec_mm_track_s *trk)
{
	struct trace_elem *elem = NULL;
	struct task_struct *tsk = NULL;
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	rcu_read_lock();

	for_each_thread(current, tsk) {
		hash_for_each_possible(trk->sample_table, elem, h_node, tsk->pid) {
			if (is_fd_alive(elem->fd))
				continue;

			__trace_sampling_del(trk, elem);
		}
	}

	rcu_read_unlock();

	spin_unlock_irqrestore(&trk->trk_slock, flags);
}

static void trace_clean_fd(struct codec_mm_track_s *trk,
			  struct file *file,
			  u32 fd)
{
	struct trace_elem *elem = NULL;
	struct task_struct *tsk = NULL;
	pid_t pid = current->pid;
	ulong flags;

	spin_lock_irqsave(&trk->trk_slock, flags);

	hash_for_each_possible(trk->sample_table, elem, h_node, pid) {
		if (file == elem->file &&
			fd == elem->fd) {
			__trace_sampling_del(trk, elem);
			goto out;
		}
	}

	rcu_read_lock();

	for_each_thread(current, tsk) {
		hash_for_each_possible(trk->sample_table, elem, h_node, tsk->pid) {
			if (file == elem->file &&
				fd == elem->fd) {
				__trace_sampling_del(trk, elem);
				break;
			}
		}
	}

	rcu_read_unlock();
out:
	spin_unlock_irqrestore(&trk->trk_slock, flags);
}

void codec_mm_kps_callback(void *priv,
			  const char *fname,
			  int exe,
			  int fid,
			  void *p0,
			  void *p1,
			  void *p2)
{
	struct codec_mm_track_s *trk = priv;

	switch (fid) {
	case DBUF_TRACE_FUNC_0: //__fd_install
	{
		struct file *file = p0;
		u32 fd = *(u32 *)p1;

		if (!is_need_track(file->private_data))
			return;

		if (trk->sample_cnt > DBUF_TRACE_ELEMS_NUM)
			trace_clean_invalid_fd(trk);

		trace_elem_sampling(trk, file, fd);

		kp_info_show(fname, file, fd);

		break;
	}
	case DBUF_TRACE_FUNC_1: //do_dup2
	{
		struct file *file = p0;
		u32 fd = *(u32 *)p1;

		if (!is_need_track(file->private_data))
			return;

		trace_elem_sampling(trk, file, fd);

		kp_info_show(fname, file, fd);

		break;
	}
	case DBUF_TRACE_FUNC_2: //dma_buf_file_release
	{
		struct file *file = p0;

		if (!is_need_track(file->private_data))
			return;

		trace_elem_clean(trk, file);

		kp_info_show(fname, file, -1);

		break;
	}
	case DBUF_TRACE_FUNC_3: //put_unused_fd
	{
		struct file *file = p0;
		u32 fd = *(u32 *)p1;

		if (!is_need_track(file->private_data))
			return;

		trace_clean_fd(trk, file, fd);

		kp_info_show(fname, file, fd);

		break;
	}
	default:
		break;
	}
}

void codec_mm_dbuf_dump_help(void)
{
	pr_info("Parameter Setting:\n"
		"  echo [h|a|d|0x1 ...] > /sys/class/codec_mm/dbuf_dump\n"
		"  echo dmabuf_track [h|a|d|0x1 ...] > /sys/kernel/debug/codec_state/config\n"
		"  h  help: Dmabuf dump usage help information.\n"
		"  a  all : All supported dmabuf types were enabled.\n"
		"  0x1 ...: Set the dmabuf to be filtered by bit mask.\n"
		"    Bit-0: enable UVM dmabuf status dumping.\n"
		"    Bit-1: enable ES  dmabuf status dumping.\n"
		"    Bit-2: enable Codec mm heap dmabuf status dumping.\n"
		"Dmabuf Infos Dumping:\n"
		"  cat /sys/class/codec_mm/dbuf_dump\n");
}

int codec_mm_sampling_open(void)
{
	struct codec_mm_track_s *trk = get_track_ctx();
	int ret;

	if (!trk)
		return -1;

	if (is_kprobes_enable(trk))
		goto out;

	ret = trace_pool_init(&trk->pool);
	if (ret < 0) {
		pr_err("%s trace pool init fail.\n", __func__);
		goto err0;
	}

	ret = codec_mm_reg_kprobes(&trk->kps_h, trk, codec_mm_kps_callback);
	if (ret < 0) {
		pr_err("%s kprobes reg fail.\n", __func__);
		goto err1;
	}
out:
	pr_info("Sampled %u elements, leaving %u elements.\n",
		trk->sample_cnt,
		trk->pool.elem_cnt);

	return 0;
err1:
	trace_pool_release(&trk->pool);
err0:
	return ret;
}

void codec_mm_sampling_close(void)
{
	struct codec_mm_track_s *trk = get_track_ctx();

	if (!trk || !trk->kps_h)
		return;

	codec_mm_unreg_kprobes(trk->kps_h);

	trace_elem_clean_all(trk);

	trace_pool_release(&trk->pool);

	trk->kps_h = NULL;
}

int dmabuf_track_cs_show(struct seq_file *m, struct codec_state_node *cs)
{
	/*
	 * struct codec_mm_track_s *trk =
	 *	container_of(cs, struct codec_mm_track_s, cs);
	 */
	seq_printf(m, "\n #### Show %s status ####\n", cs->ops->name);

	codec_mm_dbuf_walk(m);

	return 0;
}

int dmabuf_track_cs_store(int argc, const char *argv[])
{
	u32 val = UINT_MAX;
	char *pval = NULL;
	ssize_t ret;

	if (argc < 1)
		return -EINVAL;

	if (argc > 1) {
		ret = kstrtouint(argv[1], 0, &val);
		if (ret) {
			pval = strchr(argv[1], ' ');
			if (pval)
				ret = kstrtouint(strim(pval), 0, &val);
		}
	}

	switch (argv[0][0]) {
	case 'a':
	{
		codec_mm_dbuf_dump_config(val);
		break;
	}
	case 'd':
	{
		if (val)
			codec_mm_sampling_open();
		else
			codec_mm_sampling_close();
		break;
	}
	case 'h':
		codec_mm_dbuf_dump_help();
		break;
	default:
		codec_mm_dbuf_dump_config(val);
	}

	return 0;
}

CODEC_STATE_RW(dmabuf_track);

int codec_mm_track_init(void)
{
	struct codec_mm_track_s *trk = get_track_ctx();

	codec_state_register(&trk->cs, &dmabuf_track_cs_ops);

	return 0;
}

void codec_mm_track_exit(void)
{
	struct codec_mm_track_s *trk = get_track_ctx();

	codec_state_unregister(&trk->cs);
}

MODULE_IMPORT_NS(MINIDUMP);

