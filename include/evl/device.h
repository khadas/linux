/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_DEVICE_H
#define _EVL_DEVICE_H

#ifdef CONFIG_EVL

#include <evl/file.h>
#include <evl/lock.h>
#include <evl/mutex.h>
#include <evl/wait.h>
#include <evl/poll.h>

#else  /* !CONFIG_EVL */

struct evl_file { };

#define evl_open_file(__efilp, __filp)	({ 0; })
#define evl_release_file(__efilp)	do { } while (0)

#endif	/* !CONFIG_EVL */

static inline bool evl_enabled(void)
{
	return IS_ENABLED(CONFIG_EVL);
}

#endif /* !_EVL_DEVICE_H */
