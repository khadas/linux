/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_FCNTL_H
#define _EVL_UAPI_FCNTL_H

/*
 * EVL-specific open mode. Must match kernel UAPI in
 * include/uapi/asm-generic/fcntl.h.
 */
#define O_OOB	010000000000	/* Request out-of-band capabilities */

#endif /* !_EVL_UAPI_FCNTL_H */
