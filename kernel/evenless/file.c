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
#include <evenless/file.h>
#include <evenless/assert.h>
#include <evenless/sched.h>

static struct kmem_cache *fd_cache;

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

static inline int index_sfd(struct evl_fd *sfd, struct file *filp)
{
	struct rb_node **rbp, *parent = NULL;
	struct evl_fd *tmp;

	rbp = &fd_tree.rb_node;
	while (*rbp) {
		tmp = rb_entry(*rbp, struct evl_fd, rb);
		parent = *rbp;
		if (lean_left(sfd, tmp))
			rbp = &(*rbp)->rb_left;
		else if (lean_right(sfd, tmp))
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&sfd->rb, parent, rbp);
	rb_insert_color(&sfd->rb, &fd_tree);

	return 0;
}

static inline
struct evl_fd *lookup_sfd(unsigned int fd,
			struct files_struct *files)
{
	struct evl_fd *sfd, tmp;
	struct rb_node *rb;

	tmp.fd = fd;
	tmp.files = files;
	rb = fd_tree.rb_node;
	while (rb) {
		sfd = rb_entry(rb, struct evl_fd, rb);
		if (lean_left(&tmp, sfd))
			rb = rb->rb_left;
		else if (lean_right(&tmp, sfd))
			rb = rb->rb_right;
		else
			return sfd;
	}

	return NULL;
}

static inline
struct evl_fd *unindex_sfd(unsigned int fd,
			struct files_struct *files)
{
	struct evl_fd *sfd = lookup_sfd(fd, files);

	if (sfd)
		rb_erase(&sfd->rb, &fd_tree);

	return sfd;
}

void install_inband_fd(unsigned int fd, struct file *filp,
		struct files_struct *files) /* in-band */
{
	struct evl_fd *sfd;
	unsigned long flags;
	int ret = -ENOMEM;

	if (filp->oob_data == NULL)
		return;

	sfd = kmem_cache_alloc(fd_cache, GFP_KERNEL);
	if (sfd) {
		sfd->fd = fd;
		sfd->files = files;
		sfd->sfilp = filp->oob_data;
		raw_spin_lock_irqsave(&fdt_lock, flags);
		ret = index_sfd(sfd, filp);
		raw_spin_unlock_irqrestore(&fdt_lock, flags);
	}

	EVL_WARN_ON(CORE, ret);
}

void uninstall_inband_fd(unsigned int fd, struct file *filp,
			struct files_struct *files) /* in-band */
{
	struct evl_fd *sfd;
	unsigned long flags;

	if (filp->oob_data == NULL)
		return;

	raw_spin_lock_irqsave(&fdt_lock, flags);
	sfd = unindex_sfd(fd, files);
	raw_spin_unlock_irqrestore(&fdt_lock, flags);

	if (sfd)
		kmem_cache_free(fd_cache, sfd);
}

void replace_inband_fd(unsigned int fd, struct file *filp,
		struct files_struct *files) /* in-band */
{
	struct evl_fd *sfd;
	unsigned long flags;

	if (filp->oob_data == NULL)
		return;

	raw_spin_lock_irqsave(&fdt_lock, flags);

	sfd = lookup_sfd(fd, files);
	if (sfd) {
		sfd->sfilp = filp->oob_data;
		raw_spin_unlock_irqrestore(&fdt_lock, flags);
		return;
	}

	raw_spin_unlock_irqrestore(&fdt_lock, flags);

	install_inband_fd(fd, filp, files);
}

struct evl_file *evl_get_file(unsigned int fd) /* OOB */
{
	struct evl_file *sfilp = NULL;
	struct evl_fd *sfd;
	unsigned long flags;

	raw_spin_lock_irqsave(&fdt_lock, flags);
	sfd = lookup_sfd(fd, current->files);
	if (sfd) {
		sfilp = sfd->sfilp;
		evl_get_fileref(sfilp);
	}
	raw_spin_unlock_irqrestore(&fdt_lock, flags);

	return sfilp;
}

static void release_oob_ref(struct irq_work *work)
{
	struct evl_file *sfilp;

	sfilp = container_of(work, struct evl_file, oob_work);
	complete(&sfilp->oob_done);
}

void __evl_put_file(struct evl_file *sfilp)
{
	init_irq_work(&sfilp->oob_work, release_oob_ref);
	irq_work_queue(&sfilp->oob_work);
}

/**
 * evl_open_file - Open new file with OOB capabilities
 *
 * Called by chrdev with OOB capabilities when a new @sfilp is
 * opened. @sfilp is paired with the in-band file struct at @filp.
 */
int evl_open_file(struct evl_file *sfilp, struct file *filp)
{
	sfilp->filp = filp;
	filp->oob_data = sfilp;	/* mark filp as OOB-capable. */
	atomic_set(&sfilp->oob_refs, 1);
	init_completion(&sfilp->oob_done);

	return 0;
}

/**
 * evl_release_file - Drop an OOB-capable file
 *
 * Called by chrdev with OOB capabilities when @sfilp is about to be
 * released. Must be called from a fops->release() handler, and paired
 * with a previous call to evl_open_file() from the fops->open()
 * handler.
 */
void evl_release_file(struct evl_file *sfilp)
{
	/*
	 * Release the original reference on @sfilp. If OOB references
	 * are still pending (e.g. some thread is still blocked in
	 * fops->oob_read()), we must wait for them to be dropped
	 * before allowing the in-band code to dismantle @sfilp->filp.
	 *
	 * NOTE: In-band and OOB fds are working together in lockstep
	 * mode via dovetail_install/uninstall_fd() calls.  Therefore,
	 * we can't livelock with evl_get_file() as @sfilp was
	 * removed from the fd tree before fops->release() called us.
	 */
	if (atomic_dec_return(&sfilp->oob_refs) > 0)
		wait_for_completion(&sfilp->oob_done);
}

void evl_cleanup_files(void)
{
	kmem_cache_destroy(fd_cache);
}

int __init evl_init_files(void)
{
	fd_cache = kmem_cache_create("evl_fdcache",
				sizeof(struct evl_fd),
				0, 0, NULL);
	if (fd_cache == NULL)
		return -ENOMEM;

	return 0;
}
