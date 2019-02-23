/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stdarg.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/irq_work.h>
#include <linux/spinlock.h>
#include <evenless/file.h>
#include <evenless/memory.h>
#include <evenless/assert.h>
#include <evenless/sched.h>
#include <evenless/poll.h>

static struct rb_root fd_tree = RB_ROOT;

static DEFINE_HARD_SPINLOCK(fdt_lock);

/*
 * We could have a per-files_struct table of OOB fds, but this looks
 * overkill at the moment. So we only have a single rb-tree for now,
 * indexing our file descriptors on a composite key which pairs the
 * the in-band fd and the originating files struct pointer.
 */

static inline bool lean_left(struct evl_fd *lh, struct evl_fd *rh)
{
	if (lh->files == rh->files)
		return lh->fd < rh->fd;

	return lh->files < rh->files;
}

static inline bool lean_right(struct evl_fd *lh, struct evl_fd *rh)
{
	if (lh->files == rh->files)
		return lh->fd > rh->fd;

	return lh->files > rh->files;
}

static inline int index_efd(struct evl_fd *efd, struct file *filp)
{
	struct rb_node **rbp, *parent = NULL;
	struct evl_fd *tmp;

	rbp = &fd_tree.rb_node;
	while (*rbp) {
		tmp = rb_entry(*rbp, struct evl_fd, rb);
		parent = *rbp;
		if (lean_left(efd, tmp))
			rbp = &(*rbp)->rb_left;
		else if (lean_right(efd, tmp))
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&efd->rb, parent, rbp);
	rb_insert_color(&efd->rb, &fd_tree);

	return 0;
}

static inline
struct evl_fd *lookup_efd(unsigned int fd,
			struct files_struct *files)
{
	struct evl_fd *efd, tmp;
	struct rb_node *rb;

	tmp.fd = fd;
	tmp.files = files;
	rb = fd_tree.rb_node;
	while (rb) {
		efd = rb_entry(rb, struct evl_fd, rb);
		if (lean_left(&tmp, efd))
			rb = rb->rb_left;
		else if (lean_right(&tmp, efd))
			rb = rb->rb_right;
		else
			return efd;
	}

	return NULL;
}

static inline
struct evl_fd *unindex_efd(unsigned int fd,
			struct files_struct *files)
{
	struct evl_fd *efd = lookup_efd(fd, files);

	if (efd)
		rb_erase(&efd->rb, &fd_tree);

	return efd;
}

/* in-band, caller may hold files->file_lock */
void install_inband_fd(unsigned int fd, struct file *filp,
		struct files_struct *files)
{
	unsigned long flags;
	struct evl_fd *efd;
	int ret = -ENOMEM;

	if (filp->oob_data == NULL)
		return;

	efd = evl_alloc(sizeof(struct evl_fd));
	if (efd) {
		efd->fd = fd;
		efd->files = files;
		efd->efilp = filp->oob_data;
		INIT_LIST_HEAD(&efd->poll_nodes);
		raw_spin_lock_irqsave(&fdt_lock, flags);
		ret = index_efd(efd, filp);
		raw_spin_unlock_irqrestore(&fdt_lock, flags);
	}

	EVL_WARN_ON(CORE, ret);
}

/* fdt_lock held, irqs off. CAUTION: resched required on exit. */
static void drop_watchpoints(struct evl_fd *efd)
{
	if (!list_empty(&efd->poll_nodes))
		evl_drop_watchpoints(&efd->poll_nodes);
}

/* in-band, caller holds files->file_lock */
void uninstall_inband_fd(unsigned int fd, struct file *filp,
			struct files_struct *files)
{
	unsigned long flags;
	struct evl_fd *efd;

	if (filp->oob_data == NULL)
		return;

	raw_spin_lock_irqsave(&fdt_lock, flags);
	efd = unindex_efd(fd, files);
	if (efd)
		drop_watchpoints(efd);
	raw_spin_unlock_irqrestore(&fdt_lock, flags);
	evl_schedule();

	if (efd)
		evl_free(efd);
}

/* in-band, caller holds files->file_lock */
void replace_inband_fd(unsigned int fd, struct file *filp,
		struct files_struct *files)
{
	unsigned long flags;
	struct evl_fd *efd;

	if (filp->oob_data == NULL)
		return;

	raw_spin_lock_irqsave(&fdt_lock, flags);

	efd = lookup_efd(fd, files);
	if (efd) {
		drop_watchpoints(efd);
		efd->efilp = filp->oob_data;
		raw_spin_unlock_irqrestore(&fdt_lock, flags);
		evl_schedule();
		return;
	}

	raw_spin_unlock_irqrestore(&fdt_lock, flags);

	install_inband_fd(fd, filp, files);
}

struct evl_file *evl_get_file(unsigned int fd)
{
	struct evl_file *efilp = NULL;
	unsigned long flags;
	struct evl_fd *efd;

	raw_spin_lock_irqsave(&fdt_lock, flags);
	efd = lookup_efd(fd, current->files);
	if (efd) {
		efilp = efd->efilp;
		evl_get_fileref(efilp);
	}
	raw_spin_unlock_irqrestore(&fdt_lock, flags);

	return efilp;
}

static void release_oob_ref(struct irq_work *work)
{
	struct evl_file *efilp;

	efilp = container_of(work, struct evl_file, oob_work);
	complete(&efilp->oob_done);
}

void __evl_put_file(struct evl_file *efilp)
{
	init_irq_work(&efilp->oob_work, release_oob_ref);
	irq_work_queue(&efilp->oob_work);
}

struct evl_file *evl_watch_fd(unsigned int fd,
			struct evl_poll_node *node)
{
	struct evl_file *efilp = NULL;
	unsigned long flags;
	struct evl_fd *efd;

	raw_spin_lock_irqsave(&fdt_lock, flags);
	efd = lookup_efd(fd, current->files);
	if (efd) {
		efilp = efd->efilp;
		evl_get_fileref(efilp);
		list_add(&node->next, &efd->poll_nodes);
	}
	raw_spin_unlock_irqrestore(&fdt_lock, flags);

	return efilp;
}

void evl_ignore_fd(struct evl_poll_node *node)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&fdt_lock, flags);
	list_del(&node->next);
	raw_spin_unlock_irqrestore(&fdt_lock, flags);
}

/**
 * evl_open_file - Open new file with OOB capabilities
 *
 * Called by chrdev with OOB capabilities when a new @efilp is
 * opened. @efilp is paired with the in-band file struct at @filp.
 */
int evl_open_file(struct evl_file *efilp, struct file *filp)
{
	efilp->filp = filp;
	filp->oob_data = efilp;	/* mark filp as OOB-capable. */
	atomic_set(&efilp->oob_refs, 1);
	init_completion(&efilp->oob_done);

	return 0;
}

/**
 * evl_release_file - Drop an OOB-capable file
 *
 * Called by chrdev with OOB capabilities when @efilp is about to be
 * released. Must be called from a fops->release() handler, and paired
 * with a previous call to evl_open_file() from the fops->open()
 * handler.
 */
void evl_release_file(struct evl_file *efilp)
{
	/*
	 * Release the original reference on @efilp. If OOB references
	 * are still pending (e.g. some thread is still blocked in
	 * fops->oob_read()), we must wait for them to be dropped
	 * before allowing the in-band code to dismantle @efilp->filp.
	 *
	 * NOTE: In-band and OOB fds are working together in lockstep
	 * mode via dovetail_install/uninstall_fd() calls.  Therefore,
	 * we can't livelock with evl_get_file() as @efilp was
	 * removed from the fd tree before fops->release() called us.
	 */
	if (atomic_dec_return(&efilp->oob_refs) > 0)
		wait_for_completion(&efilp->oob_done);
}
