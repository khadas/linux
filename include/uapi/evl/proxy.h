/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_PROXY_H
#define _EVL_UAPI_PROXY_H

#include <linux/types.h>

#define EVL_PROXY_DEV	"proxy"

struct evl_proxy_attrs {
	__u32 fd;
	__u32 bufsz;
	__u32 granularity;
};

#endif /* !_EVL_UAPI_PROXY_H */
