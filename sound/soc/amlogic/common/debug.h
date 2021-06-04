/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_AUDIO_DEBUG_H_
#define __AML_AUDIO_DEBUG_H_

#include "../auge/ddr_mngr.h"

/* debug audio irq both from ddr and to ddr, get hw or hal pointer runtime */

/* #define DEBUG_IRQ */
#ifdef DEBUG_IRQ
enum ddr_type {
	TODDR,
	FRDDR,
};

void get_time_stamp(bool ddr_type, unsigned int ddr_id, unsigned int hwptr,
		unsigned int swptr, unsigned int buffer_size);
void set_ddr_debug(bool debug_enable);
bool get_ddr_debug(void);
#endif

#endif
