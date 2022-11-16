// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "deinterlace.h"
#ifdef DIM_HAVE_HDR
#include <linux/amlogic/media/amvecm/hdr2_ext.h>
#endif /* DIM_HAVE_HDR */
#include "di_data_l.h"
#include <linux/amlogic/media/di/di.h>
#include "register.h"
#include "di_prc.h"

#ifdef DIM_HAVE_HDR

enum EREG_HDR_IDX {
	EMATRIXI_COEF00_01,
	EMATRIXI_COEF02_10,
	EMATRIXI_COEF11_12,
	EMATRIXI_COEF20_21,
	EMATRIXI_COEF22,
	EMATRIXI_COEF30_31,
	EMATRIXI_COEF32_40,
	EMATRIXI_COEF41_42,
	EMATRIXI_OFFSET0_1,
	EMATRIXI_OFFSET2,
	EMATRIXI_PRE_OFFSET0_1,
	EMATRIXI_PRE_OFFSET2,
	EMATRIXI_CLIP,
	EMATRIXI_EN_CTRL,
	EMATRIXO_COEF00_01,
	EMATRIXO_COEF02_10,
	EMATRIXO_COEF11_12,
	EMATRIXO_COEF20_21,
	EMATRIXO_COEF22,
	EMATRIXO_COEF30_31,
	EMATRIXO_COEF32_40,
	EMATRIXO_COEF41_42,
	EMATRIXO_OFFSET0_1,
	EMATRIXO_OFFSET2,
	EMATRIXO_PRE_OFFSET0_1,
	EMATRIXO_PRE_OFFSET2,
	EMATRIXO_CLIP,
	EMATRIXO_EN_CTRL,
	ECGAIN_OFFT,
	ECGAIN_COEF0,
	ECGAIN_COEF1,
	EADPS_CTRL,
	EADPS_ALPHA0,
	EADPS_ALPHA1,
	EADPS_BETA0,
	EADPS_BETA1,
	EADPS_BETA2,
	EADPS_COEF0,
	EADPS_COEF1,
	EGMUT_CTRL,
	EGMUT_COEF0,
	EGMUT_COEF1,
	EGMUT_COEF2,
	EGMUT_COEF3,
	EGMUT_COEF4,
	EHDR_CTRL,
};

static const unsigned int reg_hdr_idx_tab[] = {
	[EMATRIXI_COEF00_01] = DI_HDR2_MATRIXI_COEF00_01,
	[EMATRIXI_COEF02_10] = DI_HDR2_MATRIXI_COEF02_10,
	[EMATRIXI_COEF11_12] = DI_HDR2_MATRIXI_COEF11_12,
	[EMATRIXI_COEF20_21] = DI_HDR2_MATRIXI_COEF20_21,
	[EMATRIXI_COEF22] = DI_HDR2_MATRIXI_COEF22,
	[EMATRIXI_COEF30_31] = DI_HDR2_MATRIXI_COEF30_31,
	[EMATRIXI_COEF32_40] = DI_HDR2_MATRIXI_COEF32_40,
	[EMATRIXI_COEF41_42] = DI_HDR2_MATRIXI_COEF41_42,
	[EMATRIXI_OFFSET0_1] = DI_HDR2_MATRIXI_OFFSET0_1,
	[EMATRIXI_OFFSET2] = DI_HDR2_MATRIXI_OFFSET2,
	[EMATRIXI_PRE_OFFSET0_1] = DI_HDR2_MATRIXI_PRE_OFFSET0_1,
	[EMATRIXI_PRE_OFFSET2] = DI_HDR2_MATRIXI_PRE_OFFSET2,
	[EMATRIXI_CLIP] = DI_HDR2_MATRIXI_CLIP,
	[EMATRIXI_EN_CTRL] = DI_HDR2_MATRIXI_EN_CTRL,
	[EMATRIXO_COEF00_01] = DI_HDR2_MATRIXO_COEF00_01,
	[EMATRIXO_COEF02_10] = DI_HDR2_MATRIXO_COEF02_10,
	[EMATRIXO_COEF11_12] = DI_HDR2_MATRIXO_COEF11_12,
	[EMATRIXO_COEF20_21] = DI_HDR2_MATRIXO_COEF20_21,
	[EMATRIXO_COEF22] = DI_HDR2_MATRIXO_COEF22,
	[EMATRIXO_COEF30_31] = DI_HDR2_MATRIXO_COEF30_31,
	[EMATRIXO_COEF32_40] = DI_HDR2_MATRIXO_COEF32_40,
	[EMATRIXO_COEF41_42] = DI_HDR2_MATRIXO_COEF41_42,
	[EMATRIXO_OFFSET0_1] = DI_HDR2_MATRIXO_OFFSET0_1,
	[EMATRIXO_OFFSET2] = DI_HDR2_MATRIXO_OFFSET2,
	[EMATRIXO_PRE_OFFSET0_1] = DI_HDR2_MATRIXO_PRE_OFFSET0_1,
	[EMATRIXO_PRE_OFFSET2] = DI_HDR2_MATRIXO_PRE_OFFSET2,
	[EMATRIXO_CLIP] = DI_HDR2_MATRIXO_CLIP,
	[EMATRIXO_EN_CTRL] = DI_HDR2_MATRIXO_EN_CTRL,
	[ECGAIN_OFFT] = DI_HDR2_CGAIN_OFFT,
	[ECGAIN_COEF0] = DI_HDR2_CGAIN_COEF0,
	[ECGAIN_COEF1] = DI_HDR2_CGAIN_COEF1,
	[EADPS_CTRL] = DI_HDR2_ADPS_CTRL,
	[EADPS_ALPHA0] = DI_HDR2_ADPS_ALPHA0,
	[EADPS_ALPHA1] = DI_HDR2_ADPS_ALPHA1,
	[EADPS_BETA0] = DI_HDR2_ADPS_BETA0,
	[EADPS_BETA1] = DI_HDR2_ADPS_BETA1,
	[EADPS_BETA2] = DI_HDR2_ADPS_BETA2,
	[EADPS_COEF0] = DI_HDR2_ADPS_COEF0,
	[EADPS_COEF1] = DI_HDR2_ADPS_COEF1,
	[EGMUT_CTRL] = DI_HDR2_GMUT_CTRL,
	[EGMUT_COEF0] = DI_HDR2_GMUT_COEF0,
	[EGMUT_COEF1] = DI_HDR2_GMUT_COEF1,
	[EGMUT_COEF2] = DI_HDR2_GMUT_COEF2,
	[EGMUT_COEF3] = DI_HDR2_GMUT_COEF3,
	[EGMUT_COEF4] = DI_HDR2_GMUT_COEF4,
	[EHDR_CTRL] = DI_HDR2_CTRL,
};

struct di_hdr_sub_s {
	/* */
	unsigned char en; /* 1: en; 2: to disable; 0: disable */
	bool para_done;
	bool n_update;
	bool pre_post; //
	bool have_set; //for disable
	unsigned int w;
	unsigned int h;
	unsigned int last_sgn_type; //
	enum hdr_process_sel last_prc_sel;
	/* */
	enum hdr_module_sel module_sel; //set by di
	enum hdr_process_sel hdr_process_select; //
	struct hdr_proc_setting_param_s hdr_para;
};

struct di_hdr_s {
	/* set by di*/
	unsigned int code;
	unsigned int size;
	const struct reg_acc *op;
	const struct di_hdr_ops_s *ops; /**/

	struct di_hdr_sub_s c;
};

static unsigned int hdr_get_data_size(void)
{
	return sizeof(struct di_hdr_s);
}

static bool hdr_data_check(void *pdata)
{
	struct di_hdr_s *pd;

	pd = (struct di_hdr_s *)pdata;
	if (!pdata || pd->code != DIM_C_HDR_DATA_CODE) {
		PR_ERR("dim:%s:code\n", __func__);
		return false;
	}
	if (pd->size < hdr_get_data_size()) {
		PR_ERR("dim:%s:size:%d->%d\n",
		       __func__,
		       pd->size, hdr_get_data_size());
		return false;
	}
	//dbg_hdr("%s:\n", __func__);
	return true;
}

/************************************************
 * di_mp_uit_get(edi_mp_hdr_en): en
 * di_mp_uit_get(edi_mp_hdr_mode):
 *	ref to enum hdr_process_sel
 * di_mp_uit_get(edi_mp_hdr_ctrl):
 * bit 0: for update setting every frame
 * bit 8: for print level
 ***********************************************/

static bool is_ctr_update_oneframe(void)
{
	return (di_mp_uit_get(edi_mp_hdr_ctrl) & DI_BIT0) ? true : false;
}

static unsigned char ctr_get_dbg_level(void)
{
	unsigned int dbg_level;

	dbg_level = (di_mp_uit_get(edi_mp_hdr_ctrl) & 0xff00) >> 8;

	//dbg_hdr("%s:%d\n", __func__, dbg_level);
	return (unsigned char)dbg_level;
}

static bool hdr_init(void)
{
	struct di_hdr_s *pd;

	if (!hdr_data_check(get_datal()->hw_hdr))
		return false;
	pd = (struct di_hdr_s *)get_datal()->hw_hdr;

	pd->c.module_sel	= DI_HDR;
	pd->c.para_done		= false;
	pd->c.en		= di_mp_uit_get(edi_mp_hdr_en);
	return true;
}

static bool hdr_get_setting(struct vframe_s *vfm)
{
	struct di_hdr_s *pd;

	if (!hdr_data_check(get_datal()->hw_hdr))
		return false;
	pd = (struct di_hdr_s *)get_datal()->hw_hdr;
	if (!pd->c.en)
		return false;

	pd->c.module_sel = DI_HDR;
	/* pre or post */
	if (vfm) {
		if (IS_I_SRC(vfm->type))
			pd->c.pre_post = 0; /* post */
		else
			pd->c.pre_post = 1; /* pre */
	} else {
		pd->c.pre_post = 3;
	}
	get_hdr_setting(pd->c.module_sel,
			pd->c.hdr_process_select,
			&pd->c.hdr_para,
			HDR_FULL_SETTING);
	/* finish */
	pd->c.para_done = true;
	pd->c.n_update = true;
	dbg_hdr("%s:\n", __func__);
	return true;
}

#define HDR_IVALID	3
static unsigned char hdr_get_pre_post(void)
{
	struct di_hdr_s *pd;

	if (!hdr_data_check(get_datal()->hw_hdr))
		return HDR_IVALID;
	pd = (struct di_hdr_s *)get_datal()->hw_hdr;
	if (!pd->c.en)
		return HDR_IVALID;

	return (unsigned char)pd->c.pre_post;
}

/* from set_hdr_matrix */
static void dim_hdr_set_matrix(struct di_hdr_s *pd,
			       enum hdr_matrix_sel mtx_sel)
{
	enum hdr_module_sel module_sel;
	struct hdr_proc_mtx_param_s *pmtx;//hdr_mtx_param;
	struct hdr_proc_adpscl_param_s *padpscl;
	const struct reg_acc *op;
	const unsigned int *regt;
	int c_gain_lim_coef[3]; //ary copy and << 2?
	int gmut_coef[3][3]; //copy and reset
	int gmut_shift;
	int i = 0;

	regt = &reg_hdr_idx_tab[0];

	module_sel	= pd->c.module_sel;
	pmtx	= &pd->c.hdr_para.hdr_mtx_param;//&pd->hdr_mtx_param;
	padpscl	= &pd->c.hdr_para.hdr_adpscl_param;
	op		= pd->op;

	if (ctr_get_dbg_level() > 1) {
		dbg_hdr("%s:mtx_sel=%d,module_sel=0x%x,mtx_only[0x%x]\n",
			__func__,
			mtx_sel,
			module_sel,
			pmtx->mtx_only);
		dbg_hdr("\tpscl:%d\n", padpscl->adpscl_alpha[0]);
		dbg_hdr("op:0x%px\n", op);
	}

	if (!pmtx)
		return;

	op->bwr(regt[EHDR_CTRL], pmtx->mtx_on, 13, 1);

	/*****************************
	 * need change clock gate as freerun when mtx on directly,
	 * not rdma op
	 *****************************/
	/* Now only operate osd1/vd1/vd2 hdr core */

	op->bwr(regt[EHDR_CTRL], pmtx->mtx_on, 13, 1);

	/* recover the clock gate as auto gate by rdma op when mtx off */
	/* Now only operate osd1/vd1/vd2 hdr core */

	if (mtx_sel == HDR_IN_MTX) {
		if (pmtx->mtx_only == MTX_ONLY &&
		    !pmtx->mtx_on)
			op->wr(regt[EMATRIXI_EN_CTRL], 1);
		else
			op->wr(regt[EMATRIXI_EN_CTRL], pmtx->mtx_on);
		op->bwr(regt[EHDR_CTRL], pmtx->mtx_on, 4, 1);
		op->bwr(regt[EHDR_CTRL], pmtx->mtx_only,
			16, 1);
		op->bwr(regt[EHDR_CTRL], 1, 14, 1);

		op->wr(regt[EMATRIXI_COEF00_01],
		       (pmtx->mtx_in[0 * 3 + 0] << 16) |
		       (pmtx->mtx_in[0 * 3 + 1] & 0x1FFF));
		op->wr(regt[EMATRIXI_COEF02_10],
		       (pmtx->mtx_in[0 * 3 + 2] << 16) |
		       (pmtx->mtx_in[1 * 3 + 0] & 0x1FFF));
		op->wr(regt[EMATRIXI_COEF11_12],
		       (pmtx->mtx_in[1 * 3 + 1] << 16) |
		       (pmtx->mtx_in[1 * 3 + 2] & 0x1FFF));
		op->wr(regt[EMATRIXI_COEF20_21],
		       (pmtx->mtx_in[2 * 3 + 0] << 16) |
		       (pmtx->mtx_in[2 * 3 + 1] & 0x1FFF));
		op->wr(regt[EMATRIXI_COEF22],
		       pmtx->mtx_in[2 * 3 + 2]);
		op->wr(regt[EMATRIXI_OFFSET0_1],
		       (pmtx->mtxi_pos_offset[0] << 16) |
		       (pmtx->mtxi_pos_offset[1] & 0xFFF));
		op->wr(regt[EMATRIXI_OFFSET2],
		       pmtx->mtxi_pos_offset[2]);
		op->wr(regt[EMATRIXI_PRE_OFFSET0_1],
		       (pmtx->mtxi_pre_offset[0] << 16) |
		       (pmtx->mtxi_pre_offset[1] & 0xFFF));
		op->wr(regt[EMATRIXI_PRE_OFFSET2],
		       pmtx->mtxi_pre_offset[2]);
	} else if (mtx_sel == HDR_GAMUT_MTX) {
		for (i = 0; i < 9; i++)
			gmut_coef[i / 3][i % 3] =
				pmtx->mtx_gamut[i];

		gmut_shift = pmtx->gmut_shift;

		for (i = 0; i < 3; i++)
			c_gain_lim_coef[i] = pmtx->mtx_cgain[i] << 2;

		/*gamut mode: 1->gamut before ootf*/
					/*2->gamut after ootf*/
					/*other->disable gamut*/
		op->bwr(regt[EHDR_CTRL],
			pmtx->mtx_gamut_mode, 6, 2);
		op->wr(regt[EGMUT_CTRL], gmut_shift);
		op->wr(regt[EGMUT_COEF0],
		       (gmut_coef[0][1] & 0xffff) << 16 |
		       (gmut_coef[0][0] & 0xffff));
		op->wr(regt[EGMUT_COEF1],
		       (gmut_coef[1][0] & 0xffff) << 16 |
		       (gmut_coef[0][2] & 0xffff));
		op->wr(regt[EGMUT_COEF2],
		       (gmut_coef[1][2] & 0xffff) << 16 |
		       (gmut_coef[1][1] & 0xffff));
		op->wr(regt[EGMUT_COEF3],
		       (gmut_coef[2][1] & 0xffff) << 16 |
		       (gmut_coef[2][0] & 0xffff));
		op->wr(regt[EGMUT_COEF4],
		       gmut_coef[2][2] & 0xffff);

		op->wr(regt[ECGAIN_COEF0],
		       c_gain_lim_coef[1] << 16 |
		       c_gain_lim_coef[0]);
		op->bwr(regt[ECGAIN_COEF1],
		       c_gain_lim_coef[2],	0, 12);

		op->wr(regt[EADPS_CTRL],
		       padpscl->adpscl_bypass[2] << 6 |
		       padpscl->adpscl_bypass[1] << 5 |
		       padpscl->adpscl_bypass[0] << 4 |
		       padpscl->adpscl_mode);
		op->wr(regt[EADPS_ALPHA0],
		       padpscl->adpscl_alpha[1] << 16 |
		       padpscl->adpscl_alpha[0]);
		op->wr(regt[EADPS_ALPHA1],
		       padpscl->adpscl_shift[0] << 24 |
		       padpscl->adpscl_shift[1] << 20 |
		       padpscl->adpscl_shift[2] << 16 |
		       padpscl->adpscl_alpha[2]);
		op->wr(regt[EADPS_BETA0],
		       padpscl->adpscl_beta_s[0] << 20 |
		       padpscl->adpscl_beta[0]);
		op->wr(regt[EADPS_BETA1],
		       padpscl->adpscl_beta_s[1] << 20 |
		       padpscl->adpscl_beta[1]);
		op->wr(regt[EADPS_BETA2],
		       padpscl->adpscl_beta_s[2] << 20 |
		       padpscl->adpscl_beta[2]);
		op->wr(regt[EADPS_COEF0],
		       padpscl->adpscl_ys_coef[1] << 16 |
		       padpscl->adpscl_ys_coef[0]);
		op->wr(regt[EADPS_COEF1],
		       padpscl->adpscl_ys_coef[2]);
	} else if (mtx_sel == HDR_OUT_MTX) {
		op->wr(regt[ECGAIN_OFFT],
		       (pmtx->mtx_cgain_offset[2] << 16) |
		       pmtx->mtx_cgain_offset[1]);
		op->wr(regt[EMATRIXO_EN_CTRL],
				    pmtx->mtx_on);
		op->bwr(regt[EHDR_CTRL], 0, 17, 1);
		op->bwr(regt[EHDR_CTRL], 1, 15, 1);

		op->wr(regt[EMATRIXO_COEF00_01],
		       (pmtx->mtx_out[0 * 3 + 0] << 16) |
		       (pmtx->mtx_out[0 * 3 + 1] & 0x1FFF));
		op->wr(regt[EMATRIXO_COEF02_10],
		       (pmtx->mtx_out[0 * 3 + 2] << 16) |
		       (pmtx->mtx_out[1 * 3 + 0] & 0x1FFF));
		op->wr(regt[EMATRIXO_COEF11_12],
		       (pmtx->mtx_out[1 * 3 + 1] << 16) |
		       (pmtx->mtx_out[1 * 3 + 2] & 0x1FFF));
		op->wr(regt[EMATRIXO_COEF20_21],
		       (pmtx->mtx_out[2 * 3 + 0] << 16) |
		       (pmtx->mtx_out[2 * 3 + 1] & 0x1FFF));
		op->wr(regt[EMATRIXO_COEF22],
		       pmtx->mtx_out[2 * 3 + 2]);

		op->wr(regt[EMATRIXO_OFFSET0_1],
		       (pmtx->mtxo_pos_offset[0] << 16) |
		       (pmtx->mtxo_pos_offset[1] & 0xFFF));
		op->wr(regt[EMATRIXO_OFFSET2],
		       pmtx->mtxo_pos_offset[2]);
		op->wr(regt[EMATRIXO_PRE_OFFSET0_1],
		       (pmtx->mtxo_pre_offset[0] << 16) |
		       (pmtx->mtxo_pre_offset[1] & 0xFFF));
		op->wr(regt[EMATRIXO_PRE_OFFSET2],
		       pmtx->mtxo_pre_offset[2]);
	}

	/* below is dbg */
	if (ctr_get_dbg_level() <= 1)
		return;

	if (mtx_sel == HDR_IN_MTX) {
		PR_INF("hdr:in_mtx %d;%d; = 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,%x\n",
		       pmtx->mtx_on,
		       pmtx->mtx_only,
		       (pmtx->mtxi_pre_offset[0] << 16) |
		       (pmtx->mtxi_pre_offset[1] & 0xFFF),
		       (pmtx->mtx_in[0 * 3 + 0] << 16) |
		       (pmtx->mtx_in[0 * 3 + 1] & 0x1FFF),
		       (pmtx->mtx_in[0 * 3 + 2] << 16) |
		       (pmtx->mtx_in[1 * 3 + 0] & 0x1FFF),
		       (pmtx->mtx_in[1 * 3 + 1] << 16) |
		       (pmtx->mtx_in[1 * 3 + 2] & 0x1FFF),
		       (pmtx->mtx_in[2 * 3 + 0] << 16) |
		       (pmtx->mtx_in[2 * 3 + 1] & 0x1FFF),
		       pmtx->mtx_in[2 * 3 + 2],
		       (pmtx->mtxi_pos_offset[0] << 16) |
		       (pmtx->mtxi_pos_offset[1] & 0xFFF));
	} else if (mtx_sel == HDR_GAMUT_MTX) {
		PR_INF("hdr: gamut_mtx %d mode %d shift %d = %x %x %x %x %x\n",
			pmtx->mtx_on,
			pmtx->mtx_gamut_mode,
			gmut_shift,
			(gmut_coef[0][1] & 0xffff) << 16 |
			(gmut_coef[0][0] & 0xffff),
			(gmut_coef[1][0] & 0xffff) << 16 |
			(gmut_coef[0][2] & 0xffff),
			(gmut_coef[1][2] & 0xffff) << 16 |
			(gmut_coef[1][1] & 0xffff),
			(gmut_coef[2][1] & 0xffff) << 16 |
			(gmut_coef[2][0] & 0xffff),
			gmut_coef[2][2] & 0xffff);
		PR_INF("hdr: adpscl bypass %d\n",
			padpscl->adpscl_bypass[0]);
		PR_INF("hdr: adpscl bypass %d, x_shift %d, y_shift %d\n",
			padpscl->adpscl_bypass[0],
			padpscl->adpscl_shift[0],
			padpscl->adpscl_shift[1]);

	}  else if (mtx_sel == HDR_OUT_MTX) {
		PR_INF("hdr: out_mtx %d %d = %x,%x %x %x %x %x,%x\n",
		       pmtx->mtx_on,
		       pmtx->mtx_only,
		       (pmtx->mtxo_pre_offset[0] << 16) |
		       (pmtx->mtxo_pre_offset[1] & 0xFFF),
		       (pmtx->mtx_out[0 * 3 + 0] << 16) |
		       (pmtx->mtx_out[0 * 3 + 1] & 0x1FFF),
		       (pmtx->mtx_out[0 * 3 + 2] << 16) |
		       (pmtx->mtx_out[1 * 3 + 0] & 0x1FFF),
		       (pmtx->mtx_out[1 * 3 + 1] << 16) |
		       (pmtx->mtx_out[1 * 3 + 2] & 0x1FFF),
		       (pmtx->mtx_out[2 * 3 + 0] << 16) |
		       (pmtx->mtx_out[2 * 3 + 1] & 0x1FFF),
		       pmtx->mtx_out[2 * 3 + 2],
		       (pmtx->mtxo_pos_offset[0] << 16) |
		       (pmtx->mtxo_pos_offset[1] & 0xFFF));
	}
}

/************************************************
 * from set_eotf_lut
 * plut -> hdr_lut_param
 ************************************************/
static void dim_hdr_set_eotf_lut(struct di_hdr_s *pd,
				 enum hdr_module_sel module_sel,
				 struct hdr_proc_lut_param_s *plut)
{
	unsigned int eotf_lut_addr_port = 0;
	unsigned int eotf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;
	const struct reg_acc *op = pd->op;

	if (module_sel == DI_HDR || module_sel == DI_M2M_HDR) {
		eotf_lut_addr_port = DI_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = DI_EOTF_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else {
		PR_ERR("dim:%s:not di hdr\n", __func__);
		return;
	}

	op->bwr(hdr_ctrl, plut->lut_on, 3, 1);

	if (!plut->lut_on)
		return;

	op->wr(eotf_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++)
		op->wr(eotf_lut_data_port, (unsigned int)plut->eotf_lut[i]);
}

/************************************************
 * from set_ootf_lut
 * plut -> hdr_lut_param
 ************************************************/
static void dim_hdr_set_ootf_lut(struct di_hdr_s *pd,
				 enum hdr_module_sel module_sel,
				 struct hdr_proc_lut_param_s *plut)
{
	unsigned int ootf_lut_addr_port = 0;
	unsigned int ootf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;
	const struct reg_acc *op = pd->op;

	if (module_sel == DI_HDR ||
	    module_sel == DI_M2M_HDR) {
		ootf_lut_addr_port = DI_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = DI_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else {
		PR_ERR("dim:%s:not di hdr\n", __func__);
		return;
	}

	op->bwr(hdr_ctrl, plut->lut_on, 1, 1);

	if (!plut->lut_on)
		return;

	op->wr(ootf_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_OOTF_LUT_SIZE / 2; i++)
		op->wr(ootf_lut_data_port,
		       ((unsigned int)plut->ogain_lut[i * 2 + 1] << 16) +
			(unsigned int)plut->ogain_lut[i * 2]);
	op->wr(ootf_lut_data_port, (unsigned int)plut->ogain_lut[148]);
}

/************************************************
 * from set_oetf_lut
 * plut-> hdr_lut_param
 ***********************************************/
static void dim_hdr_set_oetf_lut(struct di_hdr_s *pd,
				 enum hdr_module_sel module_sel,
				 struct hdr_proc_lut_param_s *plut)
{
	unsigned int oetf_lut_addr_port = 0;
	unsigned int oetf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;
	unsigned int tmp1, tmp2;
	const struct reg_acc *op = pd->op;

	if (module_sel == DI_HDR ||
		module_sel == DI_M2M_HDR) {
		oetf_lut_addr_port = DI_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = DI_OETF_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else {
		PR_ERR("dim:%s:not di hdr\n", __func__);
		return;
	}

	op->bwr(hdr_ctrl, plut->lut_on, 2, 1);

	if (!plut->lut_on)
		return;

	op->wr(oetf_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_OETF_LUT_SIZE / 2; i++) {
		tmp1 = (unsigned int)plut->oetf_lut[i * 2 + 1];
		tmp2 = (unsigned int)plut->oetf_lut[i * 2];
		if (plut->bitdepth == 10) {
			tmp1 = tmp1 >> 2;
			tmp2 = tmp2 >> 2;
		}
		op->wr(oetf_lut_data_port, (tmp1 << 16) + tmp2);
	}
	tmp1 = (unsigned int)plut->oetf_lut[148];
	if (plut->bitdepth == 10)
		op->wr(oetf_lut_data_port, tmp1 >> 2);
	else
		op->wr(oetf_lut_data_port, tmp1);
}

/************************************************
 * from set_c_gain
 * plut->hdr_lut_param
 ************************************************/
static void dim_hdr_set_c_gain(struct di_hdr_s *pd,
			       enum hdr_module_sel module_sel,
			       struct hdr_proc_lut_param_s *plut)
{
	unsigned int cgain_lut_addr_port = 0;
	unsigned int cgain_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int cgain_coef1 = 0;
	unsigned int i = 0;
	const struct reg_acc *op = pd->op;

	if (module_sel == DI_HDR ||
		module_sel == DI_M2M_HDR) {
		cgain_lut_addr_port = DI_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = DI_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
		cgain_coef1 = DI_HDR2_CGAIN_COEF1;
	} else {
		PR_ERR("dim:%s:not di hdr\n", __func__);
		return;
	}

	/*cgain mode: 0->y domin*/
	/*cgain mode: 1->rgb domin, use r/g/b max*/
	op->bwr(hdr_ctrl, 0, 12, 1);
	op->bwr(hdr_ctrl, plut->cgain_en, 0, 1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_SM1)) {
		if (plut->bitdepth == 10)
			op->bwr(cgain_coef1, 0x400, 16, 13);
		else if (plut->bitdepth == 12)
			op->bwr(cgain_coef1, 0x1000, 16, 13);
	}

	if (!plut->cgain_en)
		return;

	op->wr(cgain_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_CGAIN_LUT_SIZE / 2; i++)
		op->wr(cgain_lut_data_port,
		       ((unsigned int)plut->cgain_lut[i * 2 + 1] << 16) +
		       (unsigned int)plut->cgain_lut[i * 2]);
	op->wr(cgain_lut_data_port, (unsigned int)plut->cgain_lut[64]);
}

/************************************************
 * hdr_hist_config
 * plut -> hdr_lut_param
 ************************************************/
static void dim_hdr_hist_config(struct di_hdr_s *pd,
				enum hdr_module_sel module_sel,
				struct hdr_proc_lut_param_s *plut)
{
	unsigned int hist_ctrl;
	unsigned int hist_hs_he;
	unsigned int hist_vs_ve;
	const struct reg_acc *op = pd->op;

	if (module_sel == DI_HDR ||
		module_sel == DI_M2M_HDR) {
		hist_ctrl = DI_HDR2_HIST_CTRL;
		hist_hs_he = DI_HDR2_HIST_H_START_END;
		hist_vs_ve = DI_HDR2_HIST_V_START_END;
	} else {
		PR_ERR("dim:%s:not di hdr\n", __func__);
		return;
	}

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TM2)
		return;

	if (plut->hist_en) {
		op->wr(hist_ctrl, 0);
		op->wr(hist_hs_he, 0xeff);
		op->wr(hist_vs_ve, 0x86f);
	} else {
		op->wr(hist_ctrl, 0x5510);
		op->wr(hist_hs_he, 0x10000);
		op->wr(hist_vs_ve, 0x0);
	}
}

static void dim_hdr_set(unsigned char pre_post)
{
	enum hdr_module_sel module_sel;
	struct di_hdr_s *pd;

	if (!hdr_data_check(get_datal()->hw_hdr))
		return;
	pd = (struct di_hdr_s *)get_datal()->hw_hdr;
	if (!pd->c.en ||
	    !pd->c.para_done ||
	    !pd->c.n_update) {
		if (ctr_get_dbg_level() > 0) {
			dbg_hdr("no need update\n");
			dbg_hdr("%s:en[%d],para_done[%d], update[%d]\n",
				__func__,
				pd->c.en,
				pd->c.para_done,
				pd->c.n_update);
		}
		return;
	}
	pd->c.have_set = true;
	module_sel = pd->c.module_sel;
	/* same register with pps */
	if (pre_post < 2)
		pd->op->bwr(DI_SC_TOP_CTRL, (pre_post ? 3 : 0), 29, 2);
	pd->op->bwr(DI_HDR_IN_HSIZE, pd->c.w, 0, 13);
	pd->op->bwr(DI_HDR_IN_VSIZE, pd->c.h, 0, 13);
	/*************************/
	dim_hdr_set_matrix(pd, HDR_IN_MTX);//, &pd->hdr_mtx_param, NULL

	dim_hdr_set_eotf_lut(pd, module_sel, &pd->c.hdr_para.hdr_lut_param);

	dim_hdr_set_matrix(pd, HDR_GAMUT_MTX);//, &pd->hdr_mtx_param, NULL

	dim_hdr_set_ootf_lut(pd, module_sel, &pd->c.hdr_para.hdr_lut_param);

	dim_hdr_set_oetf_lut(pd, module_sel, &pd->c.hdr_para.hdr_lut_param);

	dim_hdr_set_matrix(pd, HDR_OUT_MTX); //, &pd->hdr_mtx_param, NULL

	dim_hdr_set_c_gain(pd, module_sel, &pd->c.hdr_para.hdr_lut_param);

	dim_hdr_hist_config(pd, module_sel, &pd->c.hdr_para.hdr_lut_param);
	pd->c.n_update = false;
	dbg_hdr("%s:pre_post[%d]\n", __func__, pre_post);

#ifdef HIS_CODE //ary: discuss with mingliang, this is not need for di
	if (clip_func == 0xff) {
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3)
			dim_hdr_clip_func_after_ootf(xxx);
	}
#endif
}

static bool hdr_disable(void)
{
	struct di_hdr_s *pd;

	if (!hdr_data_check(get_datal()->hw_hdr))
		return false;
	pd = (struct di_hdr_s *)get_datal()->hw_hdr;

	if (pd->c.en &&
	    pd->c.have_set &&
	    pd->c.hdr_process_select != HDR_BYPASS) {
		pd->c.hdr_process_select = HDR_BYPASS;
		hdr_get_setting(NULL);
		dim_hdr_set(3);
		PR_INF("%s:\n", __func__);
	}
	memset(&pd->c, 0, sizeof(pd->c));
	return true;
}

static bool hdr_get_change(struct vframe_s *vfm, unsigned int force_change)
{
	struct di_hdr_s *hdr;
	unsigned int x, y;

	if (!hdr_data_check(get_datal()->hw_hdr))
		return false;

	hdr = (struct di_hdr_s *)get_datal()->hw_hdr;
	if (!di_mp_uit_get(edi_mp_hdr_en)	&&
	    (!hdr->c.en || hdr->c.en == 2)) {
		hdr->c.en = 0;
		return false;
	}

	if (!di_mp_uit_get(edi_mp_hdr_en) && hdr->c.en == 1)
		goto HDR_UP_INF;

	if (force_change)
		goto HDR_UP_INF;

	if (!vfm)
		return false;

	if (hdr->c.last_sgn_type != vfm->signal_type)
		goto HDR_UP_INF;

	if (di_mp_uit_get(edi_mp_hdr_mode) &&
	    hdr->c.last_prc_sel != di_mp_uit_get(edi_mp_hdr_mode))
		goto HDR_UP_INF;

	if (is_ctr_update_oneframe() && hdr->c.en)
		hdr->c.n_update = true;

	return false;
HDR_UP_INF:
	if (!di_mp_uit_get(edi_mp_hdr_en)		&&
	    hdr->c.en == 1	&&
	    hdr->c.hdr_process_select != HDR_BYPASS) {
		/*en -> disable*/
		hdr->c.en = 2;
		hdr->c.hdr_process_select = HDR_BYPASS;
	} else {
		if (di_mp_uit_get(edi_mp_hdr_en))
			hdr->c.en = 1;
		if (di_mp_uit_get(edi_mp_hdr_mode))
			hdr->c.hdr_process_select =
				di_mp_uit_get(edi_mp_hdr_mode);

		/* else: to do need get hdr sel from hdr module*/
	}
	if (vfm) {
		PR_INF("%s:0x%x->0x%x\n",
			"signal_type",
			hdr->c.last_sgn_type, vfm->signal_type);
		hdr->c.last_sgn_type = vfm->signal_type;
	}
	PR_INF("%s:0x%x->0x%x\n",
		"hdr_process_select",
		hdr->c.last_prc_sel, hdr->c.hdr_process_select);
	hdr->c.last_prc_sel = hdr->c.hdr_process_select;
	dim_vf_x_y(vfm, &x, &y);
	hdr->c.w = x;
	hdr->c.h = y;

	hdr->c.n_update = true;
	return true;
}

const struct di_hdr_ops_s di_hdr_op_data = {
	.get_data_size	= hdr_get_data_size,
	.init		= hdr_init,	/* reg */
	.unreg_setting	= hdr_disable,	/* unreg */
	.get_change	= hdr_get_change,
	.get_setting	= hdr_get_setting,
	.set		= dim_hdr_set,
	.get_pre_post	= hdr_get_pre_post,

};
#endif /* DIM_HAVE_HDR */

void dim_hdr_prob(void)
{
#ifdef DIM_HAVE_HDR
	struct di_hdr_s *hdr;
	unsigned int sizev;

	if (!IS_IC_SUPPORT(HDR) || !cfgg(HDR_EN))
		return;
	hdr = (struct di_hdr_s *)get_datal()->hw_hdr;
	if (!hdr) {
		vfree(hdr);
		hdr = NULL;
		PR_INF("%s:hdr is not null, clear\n", __func__);
	}
	sizev = hdr_get_data_size();
	hdr = vmalloc(sizeof(*hdr));
	if (!hdr) {
		PR_WARN("%s:vmalloc\n", __func__);
		return;
	}
	memset(hdr, 0, sizeof(*hdr));
	hdr->code = DIM_C_HDR_DATA_CODE;
	hdr->size = sizev;
	hdr->op = &di_pre_regset;
	hdr->ops = &di_hdr_op_data;
	get_datal()->hw_hdr = hdr;

	/*check*/
	hdr = (struct di_hdr_s *)get_datal()->hw_hdr;
	PR_INF("%s:0x%x\n", __func__, hdr->code);
#endif	/* DIM_HAVE_HDR */
}

void dim_hdr_remove(void)
{
#ifdef DIM_HAVE_HDR
	if (!get_datal()->hw_hdr)
		return;
	vfree(get_datal()->hw_hdr);
	get_datal()->hw_hdr = NULL;

	PR_INF("%s:end\n", __func__);
#endif /* DIM_HAVE_HDR */
}

const struct di_hdr_ops_s *dim_hdr_ops(void)
{
#ifdef DIM_HAVE_HDR
	struct di_hdr_s *hdr;

	if (!get_datal()->hw_hdr)
		return NULL;
	hdr = (struct di_hdr_s *)get_datal()->hw_hdr;
	return hdr->ops;
#endif /* DIM_HAVE_HDR */
	return NULL;
}

int dim_dbg_hdr_reg1(struct seq_file *s, void *v, unsigned int index)
{
#ifdef DIM_HAVE_HDR
	int i;
	unsigned int rbase = 0;
	unsigned int nub1, off2, nub2, off3, nub3;

	nub1 = 0x1d;
	off2 = 0x24;
	nub2 = 2;
	off3 = 0x28;
	nub3  = 0x18;

	if (index == VD1_HDR) {
		seq_puts(s, "VD1_HDR\n");
		rbase = 0x3800;//VD1_HDR2_CTRL;
	} else if (index == DI_HDR || index == DI_M2M_HDR) {
		seq_puts(s, "DI_HDR\n");
		rbase = DI_HDR2_CTRL;
	}

	if (!rbase) {
		seq_puts(s, "base is 0, do nothing\n");
		return 0;
	}

	for (i = 0; i < nub1; i++) {
		seq_printf(s, "%d:[0x%x]=0x%x\n",
			   i,
			   rbase + i,
			   DIM_RDMA_RD(rbase + i));
	}
	for (i = 0; i < nub2; i++) {
		seq_printf(s, "%d:[0x%x]=0x%x\n",
			   i,
			   rbase + off2 + i,
			   DIM_RDMA_RD(rbase + off2 + i));
	}
	for (i = 0; i < nub3; i++) {
		seq_printf(s, "%d:[0x%x]=0x%x\n",
			   i,
			   rbase + off3 + i,
			   DIM_RDMA_RD(rbase + off3 + i));
	}
#endif /* DIM_HAVE_HDR */
	return 0;
}

int dim_dbg_hdr_para_show(struct seq_file *s, void *v)
{
#ifdef DIM_HAVE_HDR
	int i;
	struct di_hdr_s *pd;
	char *splt = "---------------------------";

	pd = (struct di_hdr_s *)get_datal()->hw_hdr;
	if (!pd) {
		seq_puts(s, "no hdr, donothing\n");
		return 0;
	}
	seq_printf(s, "%s\n", splt);
	seq_printf(s, "code[0x%x],size[%d], op:0x%px,ops:0x%px,dbg_level[%d]\n",
		   pd->code,
		   pd->size,
		   pd->op,
		   pd->ops,
		   ctr_get_dbg_level());
	seq_printf(s, "%s\n", splt);
	seq_printf(s, "en[%d], para_done[%d], n_update:%d, have_set:%d\n",
		   pd->c.en,
		   pd->c.para_done,
		   pd->c.n_update,
		   pd->c.have_set);
	seq_printf(s, "l_sgn_type[0x%x], l_prc_sel[0x%x], pre_post:%d\n",
		   pd->c.last_sgn_type,
		   pd->c.last_prc_sel,
		   pd->c.pre_post);
	seq_printf(s, "module_sel[0x%x], hdr_process_select[0x%x]\n",
		   pd->c.module_sel,
		   pd->c.hdr_process_select);
	seq_printf(s, "%s:%s\n", "hdr_mtx_param", splt);
	seq_printf(s, "mtx_on[0x%x], mtx_only[0x%x], p_sel[0x%x]\n",
		   pd->c.hdr_para.hdr_mtx_param.mtx_on,
		   pd->c.hdr_para.hdr_mtx_param.mtx_only,
		   pd->c.hdr_para.hdr_mtx_param.p_sel);

	seq_printf(s, "%s:%s\n", "hdr_lut_param", splt);
	seq_printf(s, "lut_on[%d], bitdepth[%d], cgain_en[%d], hist_en[%d]\n",
		   pd->c.hdr_para.hdr_lut_param.lut_on,
		   pd->c.hdr_para.hdr_lut_param.bitdepth,
		   pd->c.hdr_para.hdr_lut_param.cgain_en,
		   pd->c.hdr_para.hdr_lut_param.hist_en);
	seq_printf(s, "%s:%s\n", "hdr_lut_param", splt);
	seq_printf(s, "clip_en[%d], clip_max[0x%x]\n",
		   pd->c.hdr_para.hdr_clip_param.clip_en,
		   pd->c.hdr_para.hdr_clip_param.clip_max);

	seq_printf(s, "%s:%s\n", "mtx_in", splt);
	for (i = 0; i < MTX_NUM_PARAM; i++)
		seq_printf(s, "%d:%d\n",
			   i, pd->c.hdr_para.hdr_mtx_param.mtx_in[i]);
#endif /* DIM_HAVE_HDR */
	return 0;
}

