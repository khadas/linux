/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _CODEC_STATE_H
#define _CODEC_STATE_H

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>

#define __CODEC_STATE_RW(_name, _show, _store)		\
	static struct codec_state_ops _name##_cs_ops = {\
		.name	= __stringify(_name),		\
		.show	= _show,			\
		.store	= _store,			\
	}

#define CODEC_STATE_RO(_name) \
	__CODEC_STATE_RW(_name, _name##_cs_show, NULL)

#define CODEC_STATE_WO(_name) \
	__CODEC_STATE_RW(_name, NULL, _name##_cs_store)

#define CODEC_STATE_RW(_name) \
	__CODEC_STATE_RW(_name, _name##_cs_show, _name##_cs_store)

#define cs_printk printk

#define cs_printf(m, fmt, ...)				\
({							\
	typeof(m) _m = (m);				\
	typeof(fmt) _f = (fmt);				\
	if (_m)						\
		seq_printf(_m, _f, ##__VA_ARGS__);	\
	else						\
		cs_printk(_f, ##__VA_ARGS__);		\
})

struct codec_state_node;

struct codec_state_ops {
	const char *name;
	int (*store)(int argc, const char *argv[]);
	int (*show)(struct seq_file *m, struct codec_state_node *cs);
};

struct codec_state_node {
	struct list_head	list;
	struct codec_state_ops	*ops;
	bool			on;
};

int codec_state_register(struct codec_state_node *cs, struct codec_state_ops *ops);

void codec_state_unregister(struct codec_state_node *cs);

int codec_state_debugfs_init(void);

void codec_state_debugfs_release(void);

#endif //_CODEC_STATE_H

