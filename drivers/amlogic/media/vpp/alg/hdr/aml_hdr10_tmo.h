/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_HDR10_TMO_H
#define _AML_HDR10_TMO_H

extern unsigned int panell;

int hdr10_tmo_dynamic_proc(int *data_oo_y_lut,
	int *data_oo_y_lut_def,
	u32 *percentile,
	int *latest_hdr_hist,
	int latest_hist_diff,
	int latest_hist_sum,
	u32 *luminance);

#endif

