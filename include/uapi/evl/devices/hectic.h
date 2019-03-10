/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt's switchtest driver, https://xenomai.org/
 * Copyright (C) 2010 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_DEVICES_HECTIC_H
#define _EVL_UAPI_DEVICES_HECTIC_H

struct hectic_task_index {
	unsigned int index;
	unsigned int flags;
};

struct hectic_switch_req {
	unsigned int from;
	unsigned int to;
};

struct hectic_error {
	struct hectic_switch_req last_switch;
	unsigned int fp_val;
};

#define EVL_HECTIC_IOCBASE	'H'

#define EVL_HECIOC_SET_TASKS_COUNT	_IOW(EVL_HECTIC_IOCBASE, 0, __u32)
#define EVL_HECIOC_SET_CPU		_IOW(EVL_HECTIC_IOCBASE, 1, __u32)
#define EVL_HECIOC_REGISTER_UTASK 	_IOW(EVL_HECTIC_IOCBASE, 2, struct hectic_task_index)
#define EVL_HECIOC_CREATE_KTASK 	_IOWR(EVL_HECTIC_IOCBASE, 3, struct hectic_task_index)
#define EVL_HECIOC_PEND 		_IOR(EVL_HECTIC_IOCBASE, 4, struct hectic_task_index)
#define EVL_HECIOC_SWITCH_TO 		_IOR(EVL_HECTIC_IOCBASE, 5, struct hectic_switch_req)
#define EVL_HECIOC_GET_SWITCHES_COUNT 	_IOR(EVL_HECTIC_IOCBASE, 6, __u32)
#define EVL_HECIOC_GET_LAST_ERROR 	_IOR(EVL_HECTIC_IOCBASE, 7, struct hectic_error)
#define EVL_HECIOC_SET_PAUSE 		_IOW(EVL_HECTIC_IOCBASE, 8, __u32)

#endif /* !_EVL_UAPI_DEVICES_HECTIC_H */
