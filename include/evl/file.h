/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_FILE_H
#define _EVL_FILE_H

#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/irq_work.h>

struct file;
struct files_struct;
struct evl_element;
struct evl_poll_node;

#ifdef CONFIG_EVL

struct evl_file {
	struct file *filp;
	atomic_t oob_refs;
	struct completion oob_done;
	struct irq_work oob_work;
};

struct evl_fd {
	unsigned int fd;
	struct evl_file *efilp;
	struct files_struct *files;
	struct rb_node rb;
	struct list_head poll_nodes; /* poll_item->node */
};

struct evl_file_binding {
	struct evl_file efile;
	struct evl_element *element;
};

int evl_open_file(struct evl_file *efilp,
		struct file *filp);

void evl_release_file(struct evl_file *efilp);

static inline
void evl_get_fileref(struct evl_file *efilp)
{
	atomic_inc(&efilp->oob_refs);
}

struct evl_file *evl_get_file(unsigned int fd);

void __evl_put_file(struct evl_file *efilp);

static inline
void evl_put_file(struct evl_file *efilp) /* OOB */
{
	if (atomic_dec_return(&efilp->oob_refs) == 0)
		__evl_put_file(efilp);
}

#else

struct evl_file { };

#define evl_open_file(__efilp, __filp)	({ 0; })

#define evl_release_file(__efilp)	do { } while (0)

#endif	/* !CONFIG_EVL */

struct evl_file *evl_watch_fd(unsigned int fd,
			struct evl_poll_node *node);

void evl_ignore_fd(struct evl_poll_node *node);

int evl_init_files(void);

void evl_cleanup_files(void);

#endif /* !_EVL_FILE_H */
