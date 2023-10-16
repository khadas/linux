// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
// drivers/amlogic/media/enhancement/amvecm/am_lut3d.c

/* #include <mach/am_regs.h> */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/uaccess.h>
#include "arch/vpp_regs.h"
#include "arch/cm_regs.h"
#include "arch/ve_regs.h"
#include <linux/amlogic/media/amvecm/color_tune.h>
#include "blue_stretch/blue_str.h"
#include "amve.h"

void bs_ct_tbl(void)
{
	int i, j;

	if (!plut3d || !plut)
		return;

	if (bs_3dlut_en) {
		//memcpy(&plut[0][0], plut3d, 14739 * sizeof(int));
		for (i = 0; i < 4913; i++)
			for (j = 0; j < 3; j++)
				plut[i][j] = plut3d[i * 3 + j];
	}
}

/* color tune and blue stretch set */
void lut3d_set_api(void)
{
	int st;
	int mid;
	int mid2;
	int end;

	struct ct_func_s *ct_f = get_ct_func();

	st = _get_cur_enc_line();

	if (vecm_latch_flag2 & LUT3D_UPDATE &&
		!(vecm_latch_flag2 & (BS_UPDATE | CT_UPDATE))) {
		vecm_latch_flag2 &= ~LUT3D_UPDATE;
		if (!ct_f->cl_par->en || !ct_f->ct) {
			pr_info("%s: ct_en = %d, ct = %p\n", __func__, ct_f->cl_par->en, ct_f->ct);
			lut3d_update(0);
		} else {
			pr_info("%s: ct_en = %d, ct = %p\n", __func__, ct_f->cl_par->en, ct_f->ct);
			lut3d_update(plut_out);
		}
	}

	mid = _get_cur_enc_line();
	if (vecm_latch_flag2 & CT_UPDATE &&
		!(vecm_latch_flag2 & BS_UPDATE)) {
		vecm_latch_flag2 &= ~CT_UPDATE;
		vecm_latch_flag2 |= LUT3D_UPDATE;
		bs_ct_tbl();
		ct_process();
	}

	mid2 = _get_cur_enc_line();
	if (vecm_latch_flag2 & BS_UPDATE) {
		vecm_latch_flag2 &= ~BS_UPDATE;
		vecm_latch_flag2 |= CT_UPDATE;
		bls_set();
	}
	end = _get_cur_enc_line();

	if (rd_vencl) {
		pr_info("%s: st = %d, mid = %d, mid2 = %d, end = %d, diff = %d\n",
			__func__, st, mid, mid2, end, end - st);
		rd_vencl--;
	}
}

