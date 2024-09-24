/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020-2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/uio.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <evl/uaccess.h>
#include <evl/uio.h>
#include <evl/memory.h>

static int load_iov_native(struct iovec *iov,
			const struct iovec __user *u_iov,
			size_t iovlen)
{
	return raw_copy_from_user(iov, u_iov, iovlen * sizeof(*u_iov)) ?
		-EFAULT : 0;
}

#ifdef CONFIG_COMPAT

static int do_load_iov(struct iovec *iov,
		const struct iovec __user *u_iov,
		size_t iovlen)
{
	struct compat_iovec c_iov[UIO_FASTIOV], __user *uc_iov;
	size_t nvec = 0;
	int ret, n, i;

	if (likely(!is_compat_oob_call()))
		return load_iov_native(iov, u_iov, iovlen);

	uc_iov = (struct compat_iovec *)u_iov;

	/*
	 * Slurp compat_iovector in by chunks of UIO_FASTIOV
	 * cells. This is faster in the most likely case compared to
	 * allocating yet another in-kernel vector dynamically for
	 * such purpose.
	 */
	while (nvec < iovlen) {
		n = iovlen - nvec;
		if (n > UIO_FASTIOV)
			n = UIO_FASTIOV;
		ret = raw_copy_from_user(c_iov, uc_iov, sizeof(*uc_iov) * n);
		if (ret)
			return -EFAULT;
		for (i = 0; i < n; i++, iov++) {
			iov->iov_base = compat_ptr(c_iov[i].iov_base);
			iov->iov_len = c_iov[i].iov_len;
		}
		uc_iov += n;
		nvec += n;
	}

	return 0;
}

#else

static inline int do_load_iov(struct iovec *iov,
			const struct iovec __user *u_iov,
			size_t iovlen)
{
	return load_iov_native(iov, u_iov, iovlen);
}

#endif

struct iovec *evl_load_uio(const struct iovec __user *u_iov,
			size_t iovlen, struct iovec *fast_iov)
{
	struct iovec *slow_iov;
	int ret;

	if (iovlen > UIO_MAXIOV)
		return ERR_PTR(-EINVAL);

	if (likely(iovlen <= UIO_FASTIOV)) {
		ret = do_load_iov(fast_iov, u_iov, iovlen);
		return ret ? ERR_PTR(ret) : fast_iov;
	}

	slow_iov = evl_alloc(iovlen * sizeof(*u_iov));
	if (slow_iov == NULL)
		return ERR_PTR(-ENOMEM);

	ret = do_load_iov(slow_iov, u_iov, iovlen);
	if (ret) {
		evl_free(slow_iov);
		return ERR_PTR(ret);
	}

	return slow_iov;
}
EXPORT_SYMBOL_GPL(evl_load_uio);

ssize_t evl_copy_to_uio(const struct iovec *iov, size_t iovlen,
			const void *data, size_t len)
{
	ssize_t written = 0;
	size_t nbytes;
	int n, ret;

	for (n = 0; len > 0 && n < iovlen; n++, iov++) {
		if (iov->iov_len == 0)
			continue;

		nbytes = iov->iov_len;
		if (nbytes > len)
			nbytes = len;

		ret = raw_copy_to_user(iov->iov_base, data, nbytes);
		if (ret)
			return -EFAULT;

		len -= nbytes;
		data += nbytes;
		written += nbytes;
		if (written < 0)
			return -EINVAL;
	}

	return written;
}
EXPORT_SYMBOL_GPL(evl_copy_to_uio);

ssize_t evl_copy_from_uio(const struct iovec *iov, size_t iovlen,
			void *data, size_t len, size_t *remainder)
{
	size_t nbytes, avail = 0;
	ssize_t read = 0;
	int n, ret;

	for (n = 0; len > 0 && n < iovlen; n++, iov++) {
		if (iov->iov_len == 0)
			continue;

		nbytes = iov->iov_len;
		avail += nbytes;
		if (nbytes > len)
			nbytes = len;

		ret = raw_copy_from_user(data, iov->iov_base, nbytes);
		if (ret)
			return -EFAULT;

		len -= nbytes;
		data += nbytes;
		read += nbytes;
		if (read < 0)
			return -EINVAL;
	}

	if (remainder) {
		for (; n < iovlen; n++, iov++)
			avail += iov->iov_len;
		*remainder = avail - read;
	}

	return read;
}
EXPORT_SYMBOL_GPL(evl_copy_from_uio);

/*
 * Copy the content referred to by a user I/O vector to a linear area,
 * returning a kernel I/O vector composed of a single cell covering
 * the loaded data.
 */
ssize_t evl_copy_from_uio_to_kvec(const struct iovec *iov, size_t iovlen,
				size_t count, struct kvec *kvec)
{
	ssize_t ret;
	void *data;

	data = evl_alloc(count);
	if (!data)
		return -ENOMEM;

	ret = evl_copy_from_uio(iov, iovlen, data, count, NULL);
	if (ret <= 0) {
		evl_free(data);
	} else {
		kvec->iov_base = data;
		kvec->iov_len = ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(evl_copy_from_uio_to_kvec);
