/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_PROXY_H
#define _EVL_UAPI_PROXY_H

struct evl_proxy_attrs {
	__u32 fd;
	__u32 bufsz;
};

#endif /* !_EVL_UAPI_PROXY_H */
