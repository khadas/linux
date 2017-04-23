/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_LOGGER_H
#define _EVENLESS_UAPI_LOGGER_H

struct evl_logger_attrs {
	__u32 fd;
	__u32 logsz;
};

#endif /* !_EVENLESS_UAPI_LOGGER_H */
