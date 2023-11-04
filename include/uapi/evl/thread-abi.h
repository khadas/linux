/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_THREAD_ABI_H
#define _EVL_UAPI_THREAD_ABI_H

#include <evl/sched-abi.h>

#define EVL_THREAD_DEV		"thread"

/* State flags (shared) */

#define EVL_T_SUSP    0x00000001 /* Suspended */
#define EVL_T_PEND    0x00000002 /* Blocked on a wait_queue/mutex */
#define EVL_T_DELAY   0x00000004 /* Delayed/timed */
#define EVL_T_WAIT    0x00000008 /* Periodic wait */
#define EVL_T_READY   0x00000010 /* Ready to run (in rq) */
#define EVL_T_DORMANT 0x00000020 /* Not started yet */
#define EVL_T_ZOMBIE  0x00000040 /* Dead, waiting for disposal */
#define EVL_T_INBAND  0x00000080 /* Running in-band */
#define EVL_T_HALT    0x00000100 /* Halted */
#define EVL_T_BOOST   0x00000200 /* PI/PP boost undergoing */
#define EVL_T_PTSYNC  0x00000400 /* Synchronizing on ptrace event */
#define EVL_T_RRB     0x00000800 /* Undergoes round-robin scheduling */
#define EVL_T_ROOT    0x00001000 /* Root thread (in-band kernel placeholder) */
#define EVL_T_WEAK    0x00002000 /* Weak scheduling (in-band) */
#define EVL_T_USER    0x00004000 /* Userland thread */
#define EVL_T_WOSS    0x00008000 /* Warn on stage switch (HM) */
#define EVL_T_WOLI    0x00010000 /* Warn on locking inconsistency (HM) */
#define EVL_T_WOSX    0x00020000 /* Warn on stage exclusion (HM) */
#define EVL_T_PTRACE  0x00040000 /* Stopped on ptrace event */
#define EVL_T_OBSERV  0x00080000 /* Observable (only for export to userland) */
#define EVL_T_HMSIG   0x00100000 /* Notify HM events via SIGDEBUG */
#define EVL_T_HMOBS   0x00200000 /* Notify HM events via observable */
#define EVL_T_WOSO    0x00400000 /* Schedule overrun */

/* Information flags (shared) */

#define EVL_T_TIMEO   0x00000001 /* Woken up due to a timeout condition */
#define EVL_T_RMID    0x00000002 /* Pending on a removed resource */
#define EVL_T_BREAK   0x00000004 /* Forcibly awaken from a wait state */
#define EVL_T_KICKED  0x00000008 /* Forced out of OOB context */
#define EVL_T_WCHAN   0x00000010 /* Need to requeue in wait channel */
/* free: 0x00000020 */
#define EVL_T_CANCELD 0x00000040 /* Cancellation request is pending */
#define EVL_T_PIALERT 0x00000080 /* Priority inversion alert (HM notified) */
#define EVL_T_SCHEDP  0x00000100 /* Schedparam propagation is pending */
#define EVL_T_BCAST   0x00000200 /* Woken up upon resource broadcast */
#define EVL_T_SIGNAL  0x00000400 /* Event monitor signaled */
#define EVL_T_SXALERT 0x00000800 /* Stage exclusion alert (HM notified) */
#define EVL_T_PTSIG   0x00001000 /* Ptrace signal is pending */
#define EVL_T_PTSTOP  0x00002000 /* Ptrace stop is ongoing */
#define EVL_T_PTJOIN  0x00004000 /* Ptracee should join ptsync barrier */
#define EVL_T_NOMEM   0x00008000 /* No memory to complete the operation */

/* Local information flags (private to current thread) */

#define EVL_T_SYSRST  0x00000001 /* Thread awaiting syscall restart after signal */
#define EVL_T_IGNOVR  0x00000002 /* Overrun detection temporarily disabled */
#define EVL_T_INFAULT 0x00000004 /* In fault handling */
#define EVL_T_NORST   0x00000008 /* Disable syscall restart */

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
 * 't' -> Warned on stage switch (EVL_T_WOSS)
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
#define EVL_HMDIAG_OVERRUN	9

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

#endif /* !_EVL_UAPI_THREAD_ABI_H */
