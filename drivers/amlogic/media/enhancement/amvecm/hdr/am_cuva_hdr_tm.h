/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * drivers/amlogic/media/enhancement/amvecm/hdr/am_cuva_hdr_tm.h
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AM_CUVA_HDR_H
#define AM_CUVA_HDR_H
#include <linux/amlogic/media/amvecm/cuva_alg.h>

#define DEBUG_PROC_INFO 0x1

struct aml_gain_reg *get_gain_lut(void);
int cuva_hdr_dbg(void);
void cuva_tm_func(enum cuva_func_e tm_func,
	struct vframe_master_display_colour_s *p);
#endif
