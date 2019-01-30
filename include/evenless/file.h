/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_FILE_H
#define _EVENLESS_FILE_H

#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/rbtree.h>
#include <linux/completion.h>
#include <linux/irq_work.h>

struct file;
struct files_struct;
struct evl_element;

struct evl_file {
	struct file *filp;
	atomic_t oob_refs;
	struct completion oob_done;
	struct irq_work oob_work;
};

struct evl_fd {
	unsigned int fd;
	struct evl_file *sfilp;
	struct files_struct *files;
	struct rb_node rb;
};

struct evl_file_binding {
	struct evl_file efile;
	struct evl_element *element;
};

int evl_open_file(struct evl_file *sfilp,
		struct file *filp);

void evl_release_file(struct evl_file *sfilp);

static inline
void evl_get_fileref(struct evl_file *sfilp)
{
	atomic_inc(&sfilp->oob_refs);
}

struct evl_file *evl_get_file(unsigned int fd);

void __evl_put_file(struct evl_file *sfilp);

static inline
void evl_put_file(struct evl_file *sfilp) /* OOB */
{
	if (atomic_dec_return(&sfilp->oob_refs) == 0)
		__evl_put_file(sfilp);
}

int evl_init_files(void);

void evl_cleanup_files(void);

#endif /* !_EVENLESS_FILE_H */
