/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * include/linux/amlogic/media/amvecm/bls.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef BLUE_STRETCH_ALG_H
#define BLUE_STRETCH_ALG_H

struct bls_par {
	char *alg_ver;
	int blue_stretch_en;
	int blue_stretch_cr_inc;
	int blue_stretch_cb_inc;
	int blue_stretch_gain;
	int blue_stretch_gain_cb4cr;
	int blue_stretch_error_crp;
	int blue_stretch_error_crp_inv;
	int blue_stretch_error_crn;
	int blue_stretch_error_crn_inv;
	int blue_stretch_error_cbp;
	int blue_stretch_error_cbp_inv;
	int blue_stretch_error_cbn;
	int blue_stretch_error_cbn_inv;
	int blue_stretch_luma_high;

	void (*bls_proc)(int *yuv_o, int *yuv_i, struct bls_par *bls_param);
};

struct bls_par *get_bls_par(void);
#endif
