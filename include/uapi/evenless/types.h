/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Based on the Xenomai Cobalt core:
 * Copyright (C) 2013, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_TYPES_H
#define _EVENLESS_UAPI_TYPES_H

typedef __u32 fundle_t;

#define EVL_NO_HANDLE		((fundle_t)0x00000000)

/* Reserved status bits */
#define EVL_SYN_FLCLAIM		((fundle_t)0x80000000) /* Contended. */
#define EVL_SYN_FLCEIL		((fundle_t)0x40000000) /* Ceiling active. */
#define EVL_HANDLE_INDEX_MASK	(EVL_SYN_FLCLAIM|EVL_SYN_FLCEIL)

/*
 * Strip all reserved bits from the handle, only retaining the fast
 * index value.
 */
static inline fundle_t evl_get_index(fundle_t handle)
{
	return handle & ~EVL_HANDLE_INDEX_MASK;
}

#endif /* !_EVENLESS_UAPI_TYPES_H */
