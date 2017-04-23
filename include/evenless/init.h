/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_INIT_H
#define _EVENLESS_INIT_H

#include <linux/dovetail.h>

struct evl_machine_cpudata {
};

DECLARE_PER_CPU(struct evl_machine_cpudata, evl_machine_cpudata);

#ifdef CONFIG_SMP
extern struct cpumask evl_oob_cpus;
#endif

#ifdef CONFIG_EVENLESS_DEBUG
void evl_warn_init(const char *fn, int level, int status);
#else
static inline void evl_warn_init(const char *fn, int level, int status)
{ }
#endif

#define EVL_INIT_CALL(__level, __call)				\
	({							\
		int __ret = __call;				\
		if (__ret)					\
			evl_warn_init(#__call, __level, __ret);	\
		__ret;						\
	})

#endif /* !_EVENLESS_INIT_H_ */
