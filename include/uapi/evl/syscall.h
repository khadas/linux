/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_SYSCALL_H
#define _EVL_UAPI_SYSCALL_H

#define __NR_EVL_SYSCALLS	3

#define sys_evl_read	0	/* oob_read() */
#define sys_evl_write	1	/* oob_write() */
#define sys_evl_ioctl	2	/* oob_ioctl() */

#endif /* !_EVL_UAPI_SYSCALL_H */
