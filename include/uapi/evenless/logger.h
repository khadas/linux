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

#define EVL_LOGGER_IOCBASE	'l'

#define EVL_LOGIOC_CONFIG	_IOW(EVL_LOGGER_IOCBASE, 0, struct evl_logger_attrs)

#endif /* !_EVENLESS_UAPI_LOGGER_H */
