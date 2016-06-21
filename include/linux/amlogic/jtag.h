/*
 * include/linux/amlogic/jtag.h
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __INCLUDE_AMLOGIC_JTAG_H
#define __INCLUDE_AMLOGIC_JTAG_H

#include <linux/types.h>

#define JTAG_STATE_ON		0
#define JTAG_STATE_OFF		1

#define JTAG_DISABLE		(-1)
#define JTAG_A53_AO		2
#define JTAG_A53_EE		3

#define JTAG_ON			0x82000040
#define JTAG_OFF		0x82000041


#ifdef CONFIG_AML_JTAG_SETUP
extern bool is_jtag_disable(void);
extern bool is_jtag_apao(void);
extern bool is_jtag_apee(void);
#else
static inline bool is_jtag_disable(void) { return true; }
static inline bool is_jtag_apao(void) { return false; }
static inline bool is_jtag_apee(void) { return false; }
#endif


#endif
