/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_SYSCALL_H
#define _EVENLESS_UAPI_SYSCALL_H

#define __NR_EVENLESS_SYSCALLS	3

#define sys_evenless_read	0	/* oob_read() */
#define sys_evenless_write	1	/* oob_write() */
#define sys_evenless_ioctl	2	/* oob_ioctl() */

#endif /* !_EVENLESS_UAPI_SYSCALL_H */
