/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_FACTORY_H
#define _EVL_FACTORY_H

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
#include <linux/refcount.h>
#include <evl/assert.h>
#include <evl/file.h>
#include <uapi/evl/types.h>
#include <uapi/evl/factory-abi.h>

#define element_of(__filp, __type)					\
	({								\
		struct evl_file_binding *__fbind = (__filp)->private_data; \
		container_of(__fbind->element, __type, element);	\
	})

#define fundle_of(__obj)	((__obj)->element.fundle)

struct evl_element;

#define EVL_FACTORY_CLONE	BIT(0)
#define EVL_FACTORY_SINGLE	BIT(1)

#define EVL_DEVHASH_BITS	8

struct evl_index {
	struct rb_root root;
	hard_spinlock_t lock;
	fundle_t generator;
};

struct evl_factory {
	const char *name;
	const struct file_operations *fops;
	unsigned int nrdev;
	struct evl_element *(*build)(struct evl_factory *fac,
				const char __user *u_name,
				void __user *u_attrs,
				int clone_flags,
				u32 *state_offp);
	void (*dispose)(struct evl_element *e);
	const struct attribute_group **attrs;
	int flags;
	struct {
		struct device_type type;
		struct class *class;
		struct cdev cdev;
		struct device *dev;
		dev_t sub_rdev;
		kuid_t kuid;
		kgid_t kgid;
		unsigned long *minor_map;
		struct evl_index index;
		DECLARE_HASHTABLE(name_hash, EVL_DEVHASH_BITS);
		struct mutex hash_lock;
	}; /* Internal. */
};

struct evl_element {
	struct rcu_head rcu;
	struct evl_factory *factory;
	struct cdev cdev;
	struct device *dev;
	struct filename *devname;
	unsigned int minor;
	refcount_t refs;
	bool zombie;
	fundle_t fundle;
	int clone_flags;
	struct rb_node index_node;
	struct irq_work irq_work;
	struct work_struct work;
	struct hlist_node hash;
	struct {
		struct file *filp;
		int efd;
	} fpriv;
};

static inline const char *
evl_element_name(struct evl_element *e)
{
	if (e->devname)
		return e->devname->name;

	return NULL;
}

int evl_init_element(struct evl_element *e,
		struct evl_factory *fac,
		int clone_flags);

int evl_init_user_element(struct evl_element *e,
			struct evl_factory *fac,
			const char __user *u_name,
			int clone_flags);

void evl_destroy_element(struct evl_element *e);

static inline bool __evl_get_element(struct evl_element *e)
{
	return refcount_inc_not_zero(&e->refs);
}

static inline void evl_get_element(struct evl_element *e)
{
	bool ret = !__evl_get_element(e);
	EVL_WARN_ON(CORE, ret);
}

struct evl_element *
__evl_get_element_by_fundle(struct evl_index *map,
			fundle_t fundle);

#define evl_get_element_by_fundle(__map, __fundle, __type)		\
	({								\
		struct evl_element *__e;				\
		__e = __evl_get_element_by_fundle(__map, __fundle);	\
		__e ? container_of(__e, __type, element) : NULL;	\
	})

#define evl_get_factory_element_by_fundle(__fac, __fundle, __type)	\
	({								\
		struct evl_index *__map = &(__fac)->index;		\
		evl_get_element_by_fundle(__map, __fundle, __type);	\
	})

/*
 * An element can be disposed of only after the device backing it is
 * removed. If @dev is valid, so is @e at the time of the call.
 */
#define evl_get_element_by_dev(__dev, __type)				\
	({								\
		struct evl_element *__e = dev_get_drvdata(__dev);	\
		__e ? ({ evl_get_element(__e);				\
			container_of(__e, __type, element); }) : NULL;	\
	})

/* Hide the element from sysfs operations. */
static inline void evl_hide_element(struct evl_element *e)
{
	struct device *dev = e->dev;
	if (dev)
		dev_set_drvdata(dev, NULL);
}

static inline bool evl_element_is_public(struct evl_element *e)
{
	return !!(e->clone_flags & EVL_CLONE_PUBLIC);
}

static inline bool evl_element_has_coredev(struct evl_element *e)
{
	return !!(e->clone_flags & EVL_CLONE_COREDEV);
}

static inline bool evl_element_is_observable(struct evl_element *e)
{
	return !!(e->clone_flags & EVL_CLONE_OBSERVABLE);
}

void __evl_put_element(struct evl_element *e);

static inline void evl_put_element(struct evl_element *e) /* in-band or OOB */
{
	if (refcount_dec_and_test(&e->refs))
		__evl_put_element(e);
}

int evl_open_element(struct inode *inode,
		struct file *filp);

int evl_release_element(struct inode *inode,
			struct file *filp);

int evl_create_core_element_device(struct evl_element *e,
				struct evl_factory *fac,
				const char *name);

void evl_remove_element_device(struct evl_element *e);

void evl_index_element(struct evl_index *map,
		struct evl_element *e);

static inline void evl_index_factory_element(struct evl_element *e)
{
	evl_index_element(&e->factory->index, e);
}

void evl_unindex_element(struct evl_index *map,
			struct evl_element *e);

static inline void evl_unindex_factory_element(struct evl_element *e)
{
	evl_unindex_element(&e->factory->index, e);
}

int evl_create_factory(struct evl_factory *fac, dev_t rdev);

void evl_delete_factory(struct evl_factory *fac);

bool evl_may_access_factory(struct evl_factory *fac);

int evl_early_init_factories(void);

void evl_early_cleanup_factories(void);

int evl_late_init_factories(void);

void evl_late_cleanup_factories(void);

extern struct evl_factory evl_clock_factory;
extern struct evl_factory evl_control_factory;
extern struct evl_factory evl_monitor_factory;
extern struct evl_factory evl_poll_factory;
extern struct evl_factory evl_thread_factory;
extern struct evl_factory evl_trace_factory;
extern struct evl_factory evl_xbuf_factory;
extern struct evl_factory evl_proxy_factory;
extern struct evl_factory evl_observable_factory;

#endif /* !_EVL_FACTORY_H */
