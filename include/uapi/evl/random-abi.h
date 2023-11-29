/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_RANDOM_ABI_H
#define _EVL_UAPI_RANDOM_ABI_H

#include <linux/types.h>

#define EVL_RANDOM_DEV	"random"

#define EVL_RANDOM_IOCBASE  'r'

#define EVL_RNGIOC_U8		_IOR(EVL_RANDOM_IOCBASE, 0, __u8)
#define EVL_RNGIOC_U16		_IOR(EVL_RANDOM_IOCBASE, 1, __u16)
#define EVL_RNGIOC_U32		_IOR(EVL_RANDOM_IOCBASE, 2, __u32)

#endif /* !_EVL_UAPI_RANDOM_ABI_H */
