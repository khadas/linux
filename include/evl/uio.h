/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UIO_H
#define _EVL_UIO_H

#include <linux/types.h>

struct iovec;

ssize_t evl_copy_to_user_iov(const struct iovec *iov, size_t iovlen,
			const void *data, size_t len);

ssize_t evl_copy_from_user_iov(const struct iovec *iov, size_t iovlen,
		void *data, size_t len, size_t *remainder);

struct iovec *evl_load_user_iov(const struct iovec __user *u_iov,
				size_t iovlen, struct iovec *fast_iov);

#endif /* !_EVL_UIO_H */
