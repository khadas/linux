/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_FACTORY_H
#define _EVENLESS_FACTORY_H

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/bits.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/rcupdate.h>
#include <linux/workqueue.h>
#include <linux/irq_work.h>
#include <linux/mutex.h>
#include <linux/hashtable.h>
#include <evenless/file.h>
#include <uapi/evenless/types.h>

#define element_of(__filp, __type)					\
	({								\
		struct evl_file_binding *__fbind = (__filp)->private_data; \
		container_of(__fbind->element, __type, element);	\
	})								\

#define fundle_of(__obj)	((__obj)->element.fundle)

struct evl_element;

#define EVL_FACTORY_CLONE	BIT(0)
#define EVL_FACTORY_SINGLE	BIT(1)

#define EVL_DEVHASH_BITS	8

struct evl_factory {
	const char *name;
	const struct file_operations *fops;
	unsigned int nrdev;
	struct evl_element *(*build)(struct evl_factory *fac,
				const char *name,
				void __user *u_attrs,
				u32 *state_offp);
	void (*dispose)(struct evl_element *e);
	const struct attribute_group **attrs;
	int flags;
	struct {
		struct class *class;
		struct cdev cdev;
		dev_t rdev;
		dev_t sub_rdev;
		unsigned long *minor_map;
		struct evl_index {
			struct rb_root root;
			hard_spinlock_t lock;
			fundle_t generator;
		} index;
		DECLARE_HASHTABLE(name_hash, EVL_DEVHASH_BITS);
		struct mutex hash_lock;
	}; /* Internal. */
};

struct evl_element {
	struct rcu_head rcu;
	struct evl_factory *factory;
	struct cdev cdev;
	struct filename *devname;
	unsigned int minor;
	int refs;
	bool zombie;
	hard_spinlock_t ref_lock;
	fundle_t fundle;
	struct rb_node index_node;
	struct irq_work irq_work;
	struct work_struct work;
	struct hlist_node hash;
};

static inline const char *
evl_element_name(struct evl_element *e)
{
	if (e->devname)
		return e->devname->name;

	return NULL;
}

int evl_init_element(struct evl_element *e,
		struct evl_factory *fac);

void evl_destroy_element(struct evl_element *e);

void evl_get_element(struct evl_element *e);

struct evl_element *
__evl_get_element_by_fundle(struct evl_factory *fac,
			fundle_t fundle);

#define evl_get_element_by_fundle(__fac, __fundle, __type)		\
	({								\
		struct evl_element *__e;				\
		__e = __evl_get_element_by_fundle(__fac, __fundle);	\
		__e ? container_of(__e, __type, element) : NULL;	\
	})

/*
 * An element can be disposed of only after the device backing it is
 * removed. If @dev is valid, so is @e at the time of the call.
 */
#define evl_get_element_by_dev(__dev, __type)				\
	({								\
		struct evl_element *__e = dev_get_drvdata(__dev);	\
		evl_get_element(__e);					\
		container_of(__e, __type, element);			\
	})

void evl_put_element(struct evl_element *e);

int evl_open_element(struct inode *inode,
		struct file *filp);

int evl_release_element(struct inode *inode,
			struct file *filp);

int evl_create_element_device(struct evl_element *e,
			struct evl_factory *fac,
			const char *name);

void evl_remove_element_device(struct evl_element *e);

void evl_index_element(struct evl_element *e);

int evl_index_element_at(struct evl_element *e,
			fundle_t fundle);

void evl_unindex_element(struct evl_element *e);

int evl_early_init_factories(void);

void evl_early_cleanup_factories(void);

int evl_late_init_factories(void);

void evl_cleanup_factories(void);

extern struct evl_factory evl_clock_factory;
extern struct evl_factory evl_control_factory;
extern struct evl_factory evl_logger_factory;
extern struct evl_factory evl_monitor_factory;
extern struct evl_factory evl_poll_factory;
extern struct evl_factory evl_sem_factory;
extern struct evl_factory evl_thread_factory;
extern struct evl_factory evl_timerfd_factory;
extern struct evl_factory evl_trace_factory;
extern struct evl_factory evl_xbuf_factory;
extern struct evl_factory evl_mapper_factory;

#endif /* !_EVENLESS_FACTORY_H */
