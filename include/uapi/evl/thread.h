/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_THREAD_H
#define _EVL_UAPI_THREAD_H

#include <linux/types.h>
#include <uapi/evl/sched.h>

#define EVL_THREAD_DEV		"thread"

/* State flags (shared) */

#define T_SUSP    0x00000001 /*< Suspended */
#define T_PEND    0x00000002 /*< Blocked on a wait_queue/mutex */
#define T_DELAY   0x00000004 /*< Delayed/timed */
#define T_WAIT    0x00000008 /*< Periodic wait */
#define T_READY   0x00000010 /*< Ready to run (in rq) */
#define T_DORMANT 0x00000020 /*< Not started yet */
#define T_ZOMBIE  0x00000040 /*< Dead, waiting for disposal */
#define T_INBAND  0x00000080 /*< Running in-band */
#define T_HALT    0x00000100 /*< Halted */
#define T_BOOST   0x00000200 /*< PI/PP boost undergoing */
#define T_SSTEP   0x00000400 /*< Single-stepped by debugger */
#define T_RRB     0x00000800 /*< Undergoes round-robin scheduling */
#define T_WARN    0x00001000 /*< Wants SIGDEBUG on error detection */
#define T_ROOT    0x00002000 /*< Root thread (in-band kernel placeholder) */
#define T_WEAK    0x00004000 /*< Weak scheduling (non real-time) */
#define T_USER    0x00008000 /*< Userland thread */
#define T_DEBUG   0x00010000 /*< User-level debugging enabled */

/* Information flags (shared) */

#define T_TIMEO   0x00000001 /*< Woken up due to a timeout condition */
#define T_RMID    0x00000002 /*< Pending on a removed resource */
#define T_BREAK   0x00000004 /*< Forcibly awaken from a wait state */
#define T_KICKED  0x00000008 /*< Forced out of OOB context */
#define T_WAKEN   0x00000010 /*< Thread waken up upon resource availability */
#define T_ROBBED  0x00000020 /*< Robbed from resource ownership */
#define T_CANCELD 0x00000040 /*< Cancellation request is pending */
#define T_PIALERT 0x00000080 /*< Priority inversion alert (SIGDEBUG sent) */
#define T_SCHEDP  0x00000100 /*< schedparam propagation is pending */
#define T_BCAST   0x00000200 /*< Woken up upon resource broadcast */
#define T_SIGNAL  0x00000400 /*< Event monitor signaled */

/* Local information flags (private to current thread) */

#define T_MOVED   0x00000001 /*< CPU migration request issued from OOB context */
#define T_SYSRST  0x00000002 /*< Thread awaiting syscall restart after signal */
#define T_HICCUP  0x00000004 /*< Just left from ptracing - timings wrecked */
#define T_INFAULT 0x00000008 /*< In fault handling */

/*
 * Must follow strictly the declaration order of the state flags
 * defined above. Status symbols are defined as follows:
 *
 * 'S' -> Forcibly suspended
 * 'w'/'W' -> Blocked with/without timeout
 * 'D' -> Delayed
 * 'p' -> Periodic timeline
 * 'R' -> Ready to run
 * 'U' -> Dormant
 * 'Z' -> Zombie
 * 'X' -> Running in-band
 * 'H' -> Held in emergency
 * 'b' -> Priority boost undergoing
 * 'T' -> Ptraced and stopped
 * 'r' -> Undergoes round-robin
 * 't' -> SIGDEBUG notifications enabled
 */
#define EVL_THREAD_STATE_LABELS  "SWDpRUZXHbTrt...."

struct evl_user_window {
	__u32 state;
	__u32 info;
	__u32 pp_pending;
};

struct evl_thread_state {
	struct evl_sched_attrs eattrs;
	int cpu;
};

#define EVL_THREAD_IOCBASE	'T'

#define EVL_THRIOC_SIGNAL		_IOW(EVL_THREAD_IOCBASE, 0, __u32)
#define EVL_THRIOC_SET_SCHEDPARAM	_IOW(EVL_THREAD_IOCBASE, 1, struct evl_sched_attrs)
#define EVL_THRIOC_GET_SCHEDPARAM	_IOR(EVL_THREAD_IOCBASE, 2, struct evl_sched_attrs)
#define EVL_THRIOC_JOIN			_IO(EVL_THREAD_IOCBASE, 3)
#define EVL_THRIOC_GET_STATE		_IOR(EVL_THREAD_IOCBASE, 4, struct evl_thread_state)
#define EVL_THRIOC_SWITCH_OOB		_IO(EVL_THREAD_IOCBASE, 5)
#define EVL_THRIOC_SWITCH_INBAND	_IO(EVL_THREAD_IOCBASE, 6)
#define EVL_THRIOC_DETACH_SELF		_IO(EVL_THREAD_IOCBASE, 7)
#define EVL_THRIOC_SET_MODE		_IOW(EVL_THREAD_IOCBASE, 8, __u32)
#define EVL_THRIOC_CLEAR_MODE		_IOW(EVL_THREAD_IOCBASE, 9, __u32)

#endif /* !_EVL_UAPI_THREAD_H */
