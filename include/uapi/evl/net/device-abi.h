/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2024 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_NET_DEVICE_ABI_H
#define _EVL_UAPI_NET_DEVICE_ABI_H

#include <linux/types.h>

#define EVL_NETDEV_IOCBASE  0xef

#define EVL_NDEVIOC_SETRXEBPF	_IOW(EVL_NETDEV_IOCBASE, 0, __s32 /* fd */)

#endif /* !_EVL_UAPI_NET_DEVICE_ABI_H */
