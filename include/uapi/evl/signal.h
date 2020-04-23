/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2013, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_SIGNAL_H
#define _EVL_UAPI_SIGNAL_H

/*
 * EVL_HMDIAG_xxx codes are possible values of sigdebug_cause().
 */
#define SIGDEBUG			SIGXCPU
#define sigdebug_code(si)		((si)->si_value.sival_int)
#define sigdebug_cause(si)		(sigdebug_code(si) & 0xff)
#define sigdebug_marker			0xfccf0000
#define sigdebug_marked(si)		\
	((sigdebug_code(si) & 0xffff0000) == sigdebug_marker)

#endif /* !_EVL_UAPI_SIGNAL_H */
