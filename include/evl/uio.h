/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UIO_H
#define _EVL_UIO_H

#include <linux/types.h>

struct iovec;
struct kvec;

ssize_t evl_copy_to_uio(const struct iovec *iov, size_t iovlen,
			const void *data, size_t len);

ssize_t evl_copy_from_uio(const struct iovec *iov, size_t iovlen,
			void *data, size_t len, size_t *remainder);

struct iovec *evl_load_uio(const struct iovec __user *u_iov,
			size_t iovlen, struct iovec *fast_iov);

ssize_t evl_copy_from_uio_to_kvec(const struct iovec *iov,
				size_t iovlen, size_t count,
				struct kvec *kvec);

static inline
size_t evl_iov_flat_length(const struct iovec *iov, int iovlen)
{
	size_t count;
	int n;

	for (n = 0, count = 0; n < iovlen; n++)
		count += iov[n].iov_len;

	return count;
}

#endif /* !_EVL_UIO_H */
