/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Based on the Xenomai Cobalt core:
 * Copyright (C) 2001, 2013, 2018 Philippe Gerum <rpm@xenomai.org>
 * Copyright (C) 2008, 2009 Jan Kiszka <jan.kiszka@siemens.com>.
 */

#ifndef _EVENLESS_UAPI_SYNCH_H
#define _EVENLESS_UAPI_SYNCH_H

#include <uapi/evenless/types.h>

/* Type flags */
#define EVL_SYN_FIFO    0x0
#define EVL_SYN_PRIO    0x1
#define EVL_SYN_PI      0x2
#define EVL_SYN_OWNER   0x4
#define EVL_SYN_PP      0x8

static inline int evl_fast_syn_is_claimed(fundle_t handle)
{
	return (handle & EVL_SYN_FLCLAIM) != 0;
}

static inline fundle_t evl_syn_fast_claim(fundle_t handle)
{
	return handle | EVL_SYN_FLCLAIM;
}

static inline fundle_t evl_syn_fast_ceil(fundle_t handle)
{
	return handle | EVL_SYN_FLCEIL;
}

static inline int
evl_is_syn_owner(atomic_t *fastlock, fundle_t ownerh)
{
	return evl_get_index(atomic_read(fastlock)) == ownerh;
}

static inline
int evl_fast_acquire_syn(atomic_t *fastlock, fundle_t new_ownerh)
{
	fundle_t h;

	h = atomic_cmpxchg(fastlock, EVL_NO_HANDLE, new_ownerh);
	if (h != EVL_NO_HANDLE) {
		if (evl_get_index(h) == new_ownerh)
			return -EBUSY;

		return -EAGAIN;
	}

	return 0;
}

static inline
int evl_fast_release_syn(atomic_t *fastlock, fundle_t cur_ownerh)
{
	return atomic_cmpxchg(fastlock, cur_ownerh, EVL_NO_HANDLE)
		== cur_ownerh;
}

#endif /* !_EVENLESS_UAPI_SYNCH_H */
