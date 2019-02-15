/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from the Xenomai Cobalt core:
 * Copyright (C) 2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2013, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_SIGNAL_H
#define _EVENLESS_UAPI_SIGNAL_H

#define SIGEVL				SIGWINCH
#define sigshadow_action(code)		((code) & 0xff)
#define sigshadow_arg(code)		(((code) >> 8) & 0xff)
#define sigshadow_int(action, arg)	((action) | ((arg) << 8))

/* SIGEVL action codes. */
#define SIGEVL_ACTION_HOME		1

#define SIGDEBUG			SIGXCPU
#define sigdebug_code(si)		((si)->si_value.sival_int)
#define sigdebug_cause(si)		(sigdebug_code(si) & 0xff)
#define sigdebug_marker			0xfccf0000
#define sigdebug_marked(si)		\
	((sigdebug_code(si) & 0xffff0000) == sigdebug_marker)

/* Possible values of sigdebug_cause() */
#define SIGDEBUG_UNDEFINED		0
#define SIGDEBUG_MIGRATE_SIGNAL		1
#define SIGDEBUG_MIGRATE_SYSCALL	2
#define SIGDEBUG_MIGRATE_FAULT		3
#define SIGDEBUG_MIGRATE_PRIOINV	4
#define SIGDEBUG_WATCHDOG		5
#define SIGDEBUG_MUTEX_IMBALANCE	6
#define SIGDEBUG_MUTEX_SLEEP		7

#endif /* !_EVENLESS_UAPI_SIGNAL_H */
