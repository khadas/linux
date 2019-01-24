/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_XBUF_H
#define _EVENLESS_XBUF_H

#include <linux/types.h>

struct evl_file;
struct evl_xbuf;

struct evl_xbuf *evl_get_xbuf(int efd,
			      struct evl_file **sfilpp);

void evl_put_xbuf(struct evl_file *sfilp);

ssize_t evl_read_xbuf(struct evl_xbuf *xbuf,
		      void *buf, size_t count,
		      int f_flags);

ssize_t evl_write_xbuf(struct evl_xbuf *xbuf,
		       const void *buf, size_t count,
		       int f_flags);

#endif /* !_EVENLESS_XBUF_H */
