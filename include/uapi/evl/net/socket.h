/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_NET_SOCKET_H
#define _EVL_UAPI_NET_SOCKET_H

#include <uapi/evl/types.h>
#include <uapi/evl/fcntl.h>

#define AF_OOB		45	/* Out-of-band domain sockets */

#define SOCK_OOB	O_OOB	/* Request out-of-band capabilities */

/* Keep this distinct from SOCK_IOC_TYPE (0x89) */
#define EVL_SOCKET_IOCBASE  0xee

struct user_oob_msghdr {
	__u64 name_ptr;		/* (struct sockaddr __user *name) */
	__u64 iov_ptr;		/* (struct iovec __user *iov) */
	__u64 ctl_ptr;		/* (void __user *control) */
	__u32 namelen;
	__u32 iovlen;
	__u32 ctllen;
	__s32 count;		/* Receive only (actual byte count). */
	__u32 flags;
	struct __evl_timespec timeout;
	struct __evl_timespec timestamp; /* Stats / TSN trigger */
};

struct evl_netdev_activation {
	__u64 poolsz;
	__u64 bufsz;
};

#define EVL_SOCKIOC_ACTIVATE	_IOW(EVL_SOCKET_IOCBASE, 2, struct evl_netdev_activation)
#define EVL_SOCKIOC_DEACTIVATE	_IO(EVL_SOCKET_IOCBASE, 3)
#define EVL_SOCKIOC_SENDMSG	_IOW(EVL_SOCKET_IOCBASE, 4, struct user_oob_msghdr)
#define EVL_SOCKIOC_RECVMSG	_IOWR(EVL_SOCKET_IOCBASE, 5, struct user_oob_msghdr)
#define EVL_SOCKIOC_SETRECVSZ	_IOW(EVL_SOCKET_IOCBASE, 6, int)
#define EVL_SOCKIOC_SETSENDSZ	_IOW(EVL_SOCKET_IOCBASE, 7, int)

#endif /* !_EVL_UAPI_NET_SOCKET_H */
