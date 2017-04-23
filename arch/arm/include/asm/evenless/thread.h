/*
 * Copyright (C) 2005 Stelian Pop
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _EVENLESS_ARM_ASM_THREAD_H
#define _EVENLESS_ARM_ASM_THREAD_H

#define xnarch_fault_pf_p(__trapnr)	((__trapnr) == ARM_TRAP_ACCESS)
#define xnarch_fault_bp_p(__trapnr)	((current->ptrace & PT_PTRACED) && \
					 (__trapnr == ARM_TRAP_BREAK || \
					  (__trapnr) == ARM_TRAP_UNDEFINSTR))

#define xnarch_fault_notify(__trapnr) (!xnarch_fault_bp_p(__trapnr))

#endif /* !_EVENLESS_ARM_ASM_THREAD_H */
