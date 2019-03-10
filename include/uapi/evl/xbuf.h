/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_XBUF_H
#define _EVL_UAPI_XBUF_H

struct evl_xbuf_attrs {
	__u32 i_bufsz;
	__u32 o_bufsz;
};

#define EVL_XBUF_IOCBASE	'x'

#endif /* !_EVL_UAPI_XBUF_H */
