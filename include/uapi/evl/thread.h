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

#define T_SUSP    0x00000001 /* Suspended */
#define T_PEND    0x00000002 /* Blocked on a wait_queue/mutex */
#define T_DELAY   0x00000004 /* Delayed/timed */
#define T_WAIT    0x00000008 /* Periodic wait */
#define T_READY   0x00000010 /* Ready to run (in rq) */
#define T_DORMANT 0x00000020 /* Not started yet */
#define T_ZOMBIE  0x00000040 /* Dead, waiting for disposal */
#define T_INBAND  0x00000080 /* Running in-band */
#define T_HALT    0x00000100 /* Halted */
#define T_BOOST   0x00000200 /* PI/PP boost undergoing */
#define T_PTSYNC  0x00000400 /* Synchronizing on ptrace event */
#define T_RRB     0x00000800 /* Undergoes round-robin scheduling */
#define T_ROOT    0x00001000 /* Root thread (in-band kernel placeholder) */
#define T_WEAK    0x00002000 /* Weak scheduling (in-band) */
#define T_USER    0x00004000 /* Userland thread */
#define T_WOSS    0x00008000 /* Warn on stage switch (HM) */
#define T_WOLI    0x00010000 /* Warn on locking inconsistency (HM) */
#define T_WOSX    0x00020000 /* Warn on stage exclusion (HM) */
#define T_PTRACE  0x00040000 /* Stopped on ptrace event */
#define T_OBSERV  0x00080000 /* Observable (only for export to userland) */
#define T_HMSIG   0x00100000 /* Notify HM events via SIGDEBUG */
#define T_HMOBS   0x00200000 /* Notify HM events via observable */

/* Information flags (shared) */

#define T_TIMEO   0x00000001 /* Woken up due to a timeout condition */
#define T_RMID    0x00000002 /* Pending on a removed resource */
#define T_BREAK   0x00000004 /* Forcibly awaken from a wait state */
#define T_KICKED  0x00000008 /* Forced out of OOB context */
#define T_WAKEN   0x00000010 /* Thread waken up upon resource availability */
#define T_ROBBED  0x00000020 /* Robbed from resource ownership */
#define T_CANCELD 0x00000040 /* Cancellation request is pending */
#define T_PIALERT 0x00000080 /* Priority inversion alert (HM notified) */
#define T_SCHEDP  0x00000100 /* Schedparam propagation is pending */
#define T_BCAST   0x00000200 /* Woken up upon resource broadcast */
#define T_SIGNAL  0x00000400 /* Event monitor signaled */
#define T_SXALERT 0x00000800 /* Stage exclusion alert (HM notified) */
#define T_PTSIG   0x00001000 /* Ptrace signal is pending */
#define T_PTSTOP  0x00002000 /* Ptrace stop is ongoing */
#define T_PTJOIN  0x00004000 /* Ptracee should join ptsync barrier */

/* Local information flags (private to current thread) */

#define T_SYSRST  0x00000001 /* Thread awaiting syscall restart after signal */
#define T_IGNOVR  0x00000002 /* Overrun detection temporarily disabled */
#define T_INFAULT 0x00000004 /* In fault handling */

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
 * 'H' -> Halted
 * 'b' -> Priority boost undergoing
 * '#' -> Ptrace sync ongoing
 * 'r' -> Undergoes round-robin
 * 't' -> Warned on stage switch (T_WOSS)
 * 'T' -> Stopped on ptrace event
 * 'o' -> Observable
 */
#define EVL_THREAD_STATE_LABELS  "SWDpRUZXHb#r...t..To.."

/* Health monitoring diag codes (via observable or SIGDEBUG). */
#define EVL_HMDIAG_SIGDEMOTE	1
#define EVL_HMDIAG_SYSDEMOTE	2
#define EVL_HMDIAG_EXDEMOTE	3
#define EVL_HMDIAG_WATCHDOG	4
#define EVL_HMDIAG_LKDEPEND	5
#define EVL_HMDIAG_LKIMBALANCE	6
#define EVL_HMDIAG_LKSLEEP	7
#define EVL_HMDIAG_STAGEX	8

struct evl_user_window {
	__u32 state;
	__u32 info;
	__u32 pp_pending;
};

struct evl_thread_state {
	struct evl_sched_attrs eattrs;
	__u32 cpu;
	__u32 state;
	__u32 isw;
	__u32 csw;
	__u32 sc;
	__u32 rwa;
	__u64 xtime;
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
#define EVL_THRIOC_SET_MODE		_IOWR(EVL_THREAD_IOCBASE, 8, __u32)
#define EVL_THRIOC_CLEAR_MODE		_IOWR(EVL_THREAD_IOCBASE, 9, __u32)
#define EVL_THRIOC_UNBLOCK		_IO(EVL_THREAD_IOCBASE, 10)
#define EVL_THRIOC_DEMOTE		_IO(EVL_THREAD_IOCBASE, 11)
#define EVL_THRIOC_YIELD		_IO(EVL_THREAD_IOCBASE, 12)

#endif /* !_EVL_UAPI_THREAD_H */
