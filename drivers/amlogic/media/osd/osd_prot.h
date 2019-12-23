/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_PROT_H_
#define _OSD_PROT_H_

#include "osd_hw.h"

#define CUGT           0
#define CID_VALUE      161
#define CID_MODE       6
#define REQ_ONOFF_EN    0
#define REQ_ON_MAX     0
#define REQ_OFF_MIN    0
#define PAT_VAL        0x00000000
#define PAT_START_PTR  1
#define PAT_END_PTR    1
#define Y_STEP         0

int osd_set_prot(unsigned char   x_rev,
		 unsigned char   y_rev,
		 unsigned char   bytes_per_pixel,
		 unsigned char   conv_422to444,
		 unsigned char   little_endian,
		 unsigned int    hold_lines,
		 unsigned int    x_start,
		 unsigned int    x_end,
		 unsigned int    y_start,
		 unsigned int    y_end,
		 unsigned int    y_len_m1,
		 unsigned char   y_step,
		 unsigned char   pat_start_ptr,
		 unsigned char   pat_end_ptr,
		 unsigned long   pat_val,
		 unsigned int    canv_addr,
		 unsigned int    cid_val,
		 unsigned char   cid_mode,
		 unsigned char   cugt,
		 unsigned char   req_onoff_en,
		 unsigned int    req_on_max,
		 unsigned int    req_off_min,
		 unsigned char	osd_index,
		 unsigned char	on_off);
#endif
