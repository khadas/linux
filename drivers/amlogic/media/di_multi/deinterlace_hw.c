// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/string.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/atomic.h>

#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/video_sink/video.h>
#include "deinterlace.h"
#include "di_data_l.h"

#include "deinterlace_hw.h"
#include "register.h"
#include "register_nr4.h"
#ifdef DET3D
#include "detect3d.h"
#endif
#include "di_api.h"
#include "di_reg_tab.h"
#include "di_prc.h"

#include <linux/seq_file.h>

static unsigned int ctrl_regs[SKIP_CTRE_NUM];

static void set_di_inp_fmt_more(unsigned int repeat_l0_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt		/* 1bit */
				);

static void set_di_inp_mif(struct DI_MIF_s  *mif, int urgent, int hold_line);

static void set_di_mem_fmt_more(int hfmt_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
	);

static void set_di_mem_mif(struct DI_MIF_s *mif, int urgent, int hold_line);

static void set_di_if0_fmt_more(int hfmt_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt		/* 1bit */
				);

static void set_di_if1_fmt_more(int hfmt_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt		/* 1bit */
				);

static void set_di_if1_mif(struct DI_MIF_s *mif, int urgent,
			   int hold_line, int vskip_cnt);

static void set_di_chan2_mif(struct DI_MIF_s *mif, int urgent, int hold_line);

static void set_di_if0_mif(struct DI_MIF_s *mif, int urgent,
			   int hold_line, int vskip_cnt, int wr_en);

static void set_di_if0_mif_g12(struct DI_MIF_s *mif, int urgent,
			       int hold_line, int vskip_cnt, int wr_en);

static void ma_di_init(void)
{
	/* 420->422 chrome difference is large motion is large,flick */
	DIM_DI_WR(DI_MTN_1_CTRL4, 0x01800880);
	DIM_DI_WR(DI_MTN_1_CTRL7, 0x0a800480);
	/* mtn setting */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		DIM_DI_WR_REG_BITS(DI_MTN_CTRL, 1, 0, 1);
		DIM_DI_WR(DI_MTN_1_CTRL1, 0x202015);
	} else {
		DIM_DI_WR(DI_MTN_1_CTRL1, 0xa0202015);
	}
	/* invert chan2 field num */
	DIM_DI_WR(DI_MTN_CTRL1, (1 << 17) | 2);
		/* no invert chan2 field num from gxlx*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		DIM_DI_WR(DI_MTN_CTRL1, 2);
}

static void ei_hw_init(void)
{
	/* ei setting */
	DIM_DI_WR(DI_EI_CTRL0, 0x00ff0100);
	DIM_DI_WR(DI_EI_CTRL1, 0x5a0a0f2d);
	DIM_DI_WR(DI_EI_CTRL2, 0x050a0a5d);
	DIM_DI_WR(DI_EI_CTRL3, 0x80000013);
	if (is_meson_txlx_cpu()) {
		DIM_DI_WR_REG_BITS(DI_EI_DRT_CTRL, 1, 30, 1);
		DIM_DI_WR_REG_BITS(DI_EI_DRT_CTRL, 1, 31, 1);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
		DIM_DI_WR_REG_BITS(DI_EI_DRT_CTRL_GXLX, 1, 30, 1);
		DIM_DI_WR_REG_BITS(DI_EI_DRT_CTRL_GXLX, 1, 31, 1);
	}
}

static void mc_di_param_init(void)
{
	DIM_DI_WR(MCDI_CHK_EDGE_GAIN_OFFST, 0x4f6124);
	DIM_DI_WR(MCDI_LMV_RT, 0x7455);
	DIM_DI_WR(MCDI_LMV_GAINTHD, 0x6014d409);
	DIM_DI_WR(MCDI_REL_DET_LPF_MSK_22_30, 0x0a010001);
	DIM_DI_WR(MCDI_REL_DET_LPF_MSK_31_34, 0x01010101);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		DIM_DI_WR_REG_BITS(MCDI_REF_MV_NUM, 2, 0, 2);
}

void dimh_init_field_mode(unsigned short height)
{
	DIM_DI_WR(DIPD_COMB_CTRL0, 0x02400210);
	DIM_DI_WR(DIPD_COMB_CTRL1, 0x88080808);
	DIM_DI_WR(DIPD_COMB_CTRL2, 0x41041008);
	DIM_DI_WR(DIPD_COMB_CTRL3, 0x00008053);
	DIM_DI_WR(DIPD_COMB_CTRL4, 0x20070002);
	if (height > 288)
		DIM_DI_WR(DIPD_COMB_CTRL5, 0x04041020);
	else
		DIM_DI_WR(DIPD_COMB_CTRL5, 0x04040804);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		DIM_DI_WR(DIPD_COMB_CTRL6, 0x00107064);
	DIM_DI_WR_REG_BITS(DI_MC_32LVL0, 16, 0, 8);
	DIM_DI_WR_REG_BITS(DI_MC_22LVL0, 256, 0, 16);
}

static void mc_pd22_check_irq(void)
{
	int cls_2_stl_thd = 1, cls_2_stl = 0;
	int is_zmv = 0, no_gmv = 0;
	int i, last_gmv, last_22_flg;
	int cur_gmv, cur_22_flg;
	unsigned int reg_val = 0;

	if (!dimp_get(edi_mp_pd22_flg_calc_en))
		return;

	is_zmv  = DIM_RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 1, 1);
	last_gmv = DIM_RDMA_RD_BITS(MCDI_FIELD_MV, 0, 6);
	last_gmv = last_gmv > 32 ? (32 - last_gmv) : last_gmv;
	cur_gmv = DIM_RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 2, 6);
	cur_gmv = cur_gmv > 32 ? (32 - cur_gmv) : cur_gmv;

	cls_2_stl = abs(cur_gmv) <= cls_2_stl_thd;
	no_gmv  = (abs(cur_gmv) == 32 && (abs(last_gmv) <= cls_2_stl_thd));
	for (i = 0; i < 3; i++) {
		last_22_flg =
			DIM_RDMA_RD_BITS(MCDI_PD_22_CHK_FLG_CNT, (24 + i), 1);
		cur_22_flg = DIM_RDMA_RD_BITS(MCDI_RO_PD_22_FLG, (24 + i), 1);
		if ((is_zmv == 1 || cls_2_stl == 1 || no_gmv == 1) &&
		    last_22_flg == 1 && cur_22_flg == 0) {
			DIM_RDMA_WR_BITS(MCDI_PD_22_CHK_FLG_CNT,
					 last_22_flg, (24 + i), 1);
			reg_val = DIM_RDMA_RD_BITS(MCDI_PD22_CHK_THD_RT,
						   0, 5) - 1;
			DIM_RDMA_WR_BITS(MCDI_PD_22_CHK_FLG_CNT,
					 reg_val, i * 8, 8);
		} else {
			DIM_RDMA_WR_BITS(MCDI_PD_22_CHK_FLG_CNT,
					 cur_22_flg, (24 + i), 1);
			reg_val =
				DIM_RDMA_RD_BITS(MCDI_RO_PD_22_FLG, i * 8, 8);
			DIM_RDMA_WR_BITS(MCDI_PD_22_CHK_FLG_CNT,
					 reg_val, i * 8, 8);
		}
	}
}

void dimh_mc_pre_mv_irq(void)
{
	unsigned int val1;

	if (dimp_get(edi_mp_pd22_flg_calc_en) && is_meson_gxlx_cpu()) {
		mc_pd22_check_irq();
	} else {
		val1 = DIM_RDMA_RD(MCDI_RO_PD_22_FLG);
		DIM_RDMA_WR(MCDI_PD_22_CHK_FLG_CNT, val1);
	}

	val1 = DIM_RDMA_RD_BITS(MCDI_RO_HIGH_VERT_FRQ_FLG, 0, 1);
	DIM_RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 0, 1);
	val1 = DIM_RDMA_RD_BITS(MCDI_RO_HIGH_VERT_FRQ_FLG, 1, 2);
	DIM_RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 2, 2);
	val1 = DIM_RDMA_RD_BITS(MCDI_RO_HIGH_VERT_FRQ_FLG, 8, 8);
	DIM_RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 8, 8);
	val1 = DIM_RDMA_RD_BITS(MCDI_RO_MOTION_PARADOX_FLG, 0, 16);
	DIM_RDMA_WR_BITS(MCDI_FIELD_HVF_PRDX_CNT, val1, 16, 16);

	val1 = DIM_RDMA_RD_BITS(MCDI_RO_RPT_MV, 0, 6);
	if (val1 == 32) {
		val1 = 0;
		DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 15, 1);
	} else {
		DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 15, 1);
	}

	DIM_RDMA_WR_BITS(MCDI_FIELD_MV, val1, 8, 6);

	val1 = DIM_RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 0, 1);
	DIM_RDMA_WR_BITS(MCDI_FIELD_MV, val1, 14, 1);

	val1 = DIM_RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 8, 8);
	DIM_RDMA_WR_BITS(MCDI_FIELD_MV, val1, 16, 8);

	val1 = DIM_RDMA_RD_BITS(MCDI_RO_GMV_LOCK_FLG, 2, 6);
	if (val1 == 32) {
		val1 = 0;
		DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 14, 1);
	} else {
		DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 14, 1);
	}
	DIM_RDMA_WR_BITS(MCDI_FIELD_MV, val1, 0, 6);

	val1 = DIM_RDMA_RD(MCDI_FIELD_LUMA_AVG_SUM_1);
	DIM_RDMA_WR(MCDI_FIELD_LUMA_AVG_SUM_0, val1);

	val1 = DIM_RDMA_RD(MCDI_RO_FLD_LUMA_AVG_SUM);
	DIM_RDMA_WR(MCDI_FIELD_LUMA_AVG_SUM_1, val1);
}

static void lmvs_init(struct mcinfo_lmv_s *lmvs, unsigned short lmv)
{
	lmvs->lock_flag = (lmv >> 14) & 3;
	lmvs->lmv = (lmv >> 8) & 63;
	lmvs->lmv = lmvs->lmv > 32 ? (32 - lmvs->lmv) : lmvs->lmv;
	lmvs->lock_cnt = (lmv & 255);
}

void dimh_calc_lmv_init(void)
{
	if (dimp_get(edi_mp_lmv_lock_win_en)) {
		DIM_RDMA_WR_BITS(MCDI_REL_DET_LMV_DIF_CHK, 3, 12, 2);
		DIM_RDMA_WR_BITS(MCDI_LMVLCKSTEXT_1, 3, 30, 2);
	} else {
		DIM_RDMA_WR_BITS(MCDI_REL_DET_LMV_DIF_CHK, 0, 12, 2);
		DIM_RDMA_WR_BITS(MCDI_LMVLCKSTEXT_1, 0, 30, 2);
	}
}

static struct mcinfo_lmv_s lines_mv[540];

void dimh_calc_lmv_base_mcinfo(unsigned int vf_height,
			       unsigned short *mcinfo_adr_v,
			       unsigned int mcinfo_size)
{
	unsigned short i, top_str, bot_str, top_end, bot_end, j = 0;
	unsigned short *mcinfo_vadr = NULL, lck_num;
	unsigned short flg_m1 = 0, flg_i = 0, nlmvlckst = 0;
	unsigned short lmv_lckstext[3] = {0, 0, 0}, nlmvlcked;
	unsigned short lmv_lckedext[3] = {0, 0, 0}, nlmvlcknum;

	/*mcinfo_vadr = (unsigned short *)phys_to_virt(mcinfo_adr);*/

	if (!dimp_get(edi_mp_lmv_lock_win_en))
		return;

	mcinfo_vadr = mcinfo_adr_v;

	for (i = 0; i < (vf_height >> 1); i++) {
		lmvs_init(&lines_mv[i], *(mcinfo_vadr + i));
		j = i + (vf_height >> 1);
		lmvs_init(&lines_mv[j], *(mcinfo_vadr + i + 272));
		if (dimp_get(edi_mp_pr_mcinfo_cnt) && j < (vf_height - 10) &&
		    j > (vf_height - dimp_get(edi_mp_offset_lmv))) {
			pr_info("MCINFO[%u]=0x%x\t", j,
				*(mcinfo_vadr + i + 272));
			if (i % 16 == 0)
				pr_info("\n");
		}
	}

	/*pr_mcinfo_cnt ? pr_mcinfo_cnt-- : (pr_mcinfo_cnt = 0);*/
	dimp_get(edi_mp_pr_mcinfo_cnt) ?
		dimp_dec(edi_mp_pr_mcinfo_cnt) :
		dimp_set(edi_mp_pr_mcinfo_cnt, 0);

	top_str = 0;
	top_end = dimp_get(edi_mp_offset_lmv);
	i = top_str;
	j = 0;
	lck_num = rd_reg_bits(MCDI_LMVLCKSTEXT_1, 16, 12);

	while (i < top_end) {
		flg_m1 = (i == top_str) ? 0 :
			(lines_mv[i - 1].lock_flag > 0);
		flg_i  = (i == top_end - 1) ? 0 :
			lines_mv[i].lock_flag > 0;
		if (!flg_m1 && flg_i) {
			#ifdef MARK_HIS
			nlmvlckst = (j == 0) ? i : ((i < (lmv_lckedext[j - 1] +
				dimp_get(edi_mp_lmv_dist))) ?
				lmv_lckstext[j - 1] : i);
			#else
			if (j == 0) {
				nlmvlckst = i;
			} else {
				if (i < (lmv_lckedext[j - 1] +
					 dimp_get(edi_mp_lmv_dist)))
					nlmvlckst = lmv_lckstext[j - 1];
				else
					nlmvlckst = i;
			}
			#endif
			j = (nlmvlckst != i) ? (j - 1) : j;
		} else if (flg_m1 && !flg_i) {
			nlmvlcked = i;
			nlmvlcknum = (nlmvlcked - nlmvlckst + 1);
			if (nlmvlcknum >= lck_num) {
				lmv_lckstext[j] = nlmvlckst;
				lmv_lckedext[j] = nlmvlcked;
				j++;
			}
		}
		i++;
		if (j > 2)
			break;
	}

	bot_str = vf_height - dimp_get(edi_mp_offset_lmv) - 1;
	bot_end = vf_height;
	i = bot_str;
	while (i < bot_end && j < 3) {
		flg_m1 = (i == bot_str) ? 0 :
			(lines_mv[i - 1].lock_flag > 0);
		flg_i  = (i == bot_end - 1) ? 0 :
			lines_mv[i].lock_flag > 0;
		if (!flg_m1 && flg_i) {
			nlmvlckst = (j == 0) ? i : ((i < (lmv_lckedext[j - 1] +
				dimp_get(edi_mp_lmv_dist))) ?
				lmv_lckstext[j - 1] : i);
			j = (nlmvlckst != i) ? (j - 1) : j;
		} else if (flg_m1 && !flg_i) {
			nlmvlcked = i;
			nlmvlcknum = (nlmvlcked - nlmvlckst + 1);
			if (nlmvlcknum >= lck_num) {
				lmv_lckstext[j] = nlmvlckst;
				lmv_lckedext[j] = nlmvlcked;
				j++;
			}
		}
		i++;
		if (j > 2)
			break;
	}

	WR(MCDI_LMVLCKSTEXT_0, lmv_lckstext[1] << 16 | lmv_lckstext[0]);
	wr_reg_bits(MCDI_LMVLCKSTEXT_1, lmv_lckstext[2], 0, 12);
	WR(MCDI_LMVLCKEDEXT_0, lmv_lckedext[1] << 16 | lmv_lckedext[0]);
	WR(MCDI_LMVLCKEDEXT_1, lmv_lckedext[2]);
}

/*
 * config pre hold ratio & mif request block len
 * pass_ratio = (pass_cnt + 1)/(pass_cnt + 1 + hold_cnt + 1)
 */
static void pre_hold_block_mode_config(void)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		DIM_DI_WR(DI_PRE_HOLD, 0);
		/* go field after 2 lines */
	#ifdef OLD_PRE_GL
		DIM_DI_WR(DI_PRE_GL_CTRL,
			  (0x80000000 | dimp_get(edi_mp_line_num_pre_frst)));
	#else
		hpre_gl_sw(false);
	#endif
	} else if (is_meson_txlx_cpu()) {
		/* setup pre process ratio to 66.6%*/
		DIM_DI_WR(DI_PRE_HOLD, (1 << 31) | (1 << 16) | 3);
		/* block len, after block insert null req to balance reqs */
		DIM_DI_WR_REG_BITS(DI_INP_GEN_REG3, 0, 4, 3);
		DIM_DI_WR_REG_BITS(DI_MEM_GEN_REG3, 0, 4, 3);
		DIM_DI_WR_REG_BITS(DI_CHAN2_GEN_REG3, 0, 4, 3);
		DIM_DI_WR_REG_BITS(DI_IF1_GEN_REG3, 0, 4, 3);
		DIM_DI_WR_REG_BITS(DI_IF2_GEN_REG3, 0, 4, 3);
		DIM_DI_WR_REG_BITS(VD1_IF0_GEN_REG3, 0, 4, 3);
	} else {
		DIM_DI_WR(DI_PRE_HOLD, (1 << 31) | (31 << 16) | 31);
	}
}

/*
 * ctrl or size related regs configured
 * in software base on real size and condition
 */
static void set_skip_ctrl_size_regs(void)
{
	ctrl_regs[0]	= DI_CLKG_CTRL;
	ctrl_regs[1]	= DI_MTN_1_CTRL1;
	ctrl_regs[2]	= MCDI_MOTINEN;
	ctrl_regs[3]	= MCDI_CTRL_MODE;
	ctrl_regs[4]	= MCDI_MC_CRTL;
	ctrl_regs[5]	= MCDI_PD_22_CHK_WND0_X;
	ctrl_regs[6]	= MCDI_PD_22_CHK_WND0_Y;
	ctrl_regs[7]	= MCDI_PD_22_CHK_WND1_X;
	ctrl_regs[8]	= MCDI_PD_22_CHK_WND1_Y;
	ctrl_regs[9]	= NR4_MCNR_LUMA_STAT_LIMTX;
	ctrl_regs[10]	= NR4_MCNR_LUMA_STAT_LIMTY;
	ctrl_regs[11]	= NR4_NM_X_CFG;
	ctrl_regs[12]	= NR4_NM_Y_CFG;
}

void dim_hw_init_reg(void)
{
	unsigned short fifo_size_post = 0x120;/*feijun 08-02*/

	if (is_meson_g12a_cpu()	||
	    is_meson_g12b_cpu()	||
	    is_meson_sm1_cpu()) {
		DIM_DI_WR(DI_IF1_LUMA_FIFO_SIZE, fifo_size_post);
		/* 17f2 is  DI_IF1_luma_fifo_size */
		DIM_DI_WR(DI_IF2_LUMA_FIFO_SIZE, fifo_size_post);
		DIM_DI_WR(DI_IF0_LUMA_FIFO_SIZE, fifo_size_post);
	}

	PR_INF("%s, 0x%x\n", __func__, DIM_RDMA_RD(DI_IF0_LUMA_FIFO_SIZE));
}

void dimh_hw_init(bool pd_enable, bool mc_enable)
{
	unsigned short fifo_size_vpp = 0xc0;
	unsigned short fifo_size_di = 0xc0;

	diext_clk_b_sw(true);
	if (is_meson_txlx_cpu() ||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_sm1_cpu()	||
	    is_meson_tm2_cpu())
		dim_top_gate_control(true, true);
	else if (is_meson_gxl_cpu()	||
		 is_meson_gxm_cpu()	||
		 is_meson_gxlx_cpu())
		DIM_DI_WR(DI_CLKG_CTRL, 0xffff0001);
	else
		DIM_DI_WR(DI_CLKG_CTRL, 0x1); /* di no clock gate */

	if (is_meson_txl_cpu()	||
	    is_meson_txlx_cpu()	||
	    is_meson_gxlx_cpu() ||
	    is_meson_txhd_cpu() ||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu() ||
	    is_meson_sm1_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()) {
		/* vpp fifo max size on txl :128*3=384[0x180] */
		/* di fifo max size on txl :96*3=288[0x120] */
		fifo_size_vpp = 0x180;
		fifo_size_di = 0x120;
	}

	/*enable lock win, suggestion from vlsi zheng.bao*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		dimp_set(edi_mp_lmv_lock_win_en, 0);/*lmv_lock_win_en = 0;*/

	DIM_DI_WR(VD1_IF0_LUMA_FIFO_SIZE, fifo_size_vpp);
	DIM_DI_WR(VD2_IF0_LUMA_FIFO_SIZE, fifo_size_vpp);
	/* 1a83 is vd2_if0_luma_fifo_size */
	DIM_DI_WR(DI_INP_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 17d8 is DI_INP_luma_fifo_size */
	DIM_DI_WR(DI_MEM_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 17e5 is DI_MEM_luma_fifo_size */
	DIM_DI_WR(DI_IF1_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 17f2 is  DI_IF1_luma_fifo_size */
	DIM_DI_WR(DI_IF2_LUMA_FIFO_SIZE,	fifo_size_di);
	/* 201a is if2 fifo size */
	DIM_DI_WR(DI_CHAN2_LUMA_FIFO_SIZE, fifo_size_di);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		DIM_DI_WR(DI_IF0_LUMA_FIFO_SIZE, fifo_size_di);
		DIM_DI_WR(DI_ARB_CTRL, 0);
	} else {
		/* enable di all arb */
		DIM_DI_WR_REG_BITS(DI_ARB_CTRL, 0xf0f, 0, 16);
	}
	/* 17b3 is DI_chan2_luma_fifo_size */
	if (is_meson_txlx_cpu()	||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu()	||
	    is_meson_g12b_cpu()	||
	    is_meson_sm1_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()) {
		dim_pre_gate_control(true, true);
		dim_post_gate_control(true);
	}

	pre_hold_block_mode_config();
	set_skip_ctrl_size_regs();
	ma_di_init();
	ei_hw_init();
	get_ops_nr()->nr_hw_init();
	if (pd_enable)
		dimh_init_field_mode(288);

	if (mc_enable)
		mc_di_param_init();
	if (is_meson_txlx_cpu()	||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu()	||
	    is_meson_sm1_cpu()	||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()) {
		dim_pre_gate_control(false, true);
		dim_post_gate_control(false);
		dim_top_gate_control(false, false);
	} else if (is_meson_txl_cpu() || is_meson_gxlx_cpu()) {
		/* di clock div enable for pq load */
		DIM_DI_WR(DI_CLKG_CTRL, 0x80000000);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0x2); /* di clock gate all */
	}

	diext_clk_b_sw(false);

	/*move from prob*/
	DIM_DI_WR_REG_BITS(MCDI_MC_CRTL, 0, 0, 1);
}

void dimh_hw_uninit(void)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		get_ops_nr()->nr_gate_control(false);
}

/*
 * mtn wr mif, contprd mif, contp2rd mif,
 * contwr mif config
 */
static void set_ma_pre_mif(struct DI_SIM_MIF_s *mtnwr_mif,
			   struct DI_SIM_MIF_s *contprd_mif,
			   struct DI_SIM_MIF_s *contp2rd_mif,
			   struct DI_SIM_MIF_s *contwr_mif,
			   unsigned short urgent)
{
	/* current field mtn canvas index. */
	DIM_RDMA_WR(DI_MTNWR_X,
		    (mtnwr_mif->start_x << 16)	|
		    (mtnwr_mif->end_x));
	DIM_RDMA_WR(DI_MTNWR_Y,
		    (mtnwr_mif->start_y << 16)	|
		    (mtnwr_mif->end_y));
	DIM_RDMA_WR(DI_MTNWR_CTRL,
		    mtnwr_mif->canvas_num	|
		    (urgent << 8));	/* urgent. */

	DIM_RDMA_WR(DI_CONTPRD_X,
		    (contprd_mif->start_x << 16)	|
		    (contprd_mif->end_x));
	DIM_RDMA_WR(DI_CONTPRD_Y,
		    (contprd_mif->start_y << 16)	|
		    (contprd_mif->end_y));
	DIM_RDMA_WR(DI_CONTP2RD_X,
		    (contp2rd_mif->start_x << 16)	|
		    (contp2rd_mif->end_x));
	DIM_RDMA_WR(DI_CONTP2RD_Y,
		    (contp2rd_mif->start_y << 16)	|
		    (contp2rd_mif->end_y));
	DIM_RDMA_WR(DI_CONTRD_CTRL,
		    (contprd_mif->canvas_num << 8)	|
		    (urgent << 16)			| /* urgent */
		    contp2rd_mif->canvas_num);

	DIM_RDMA_WR(DI_CONTWR_X,
		    (contwr_mif->start_x << 16)	|
		    (contwr_mif->end_x));
	DIM_RDMA_WR(DI_CONTWR_Y,
		    (contwr_mif->start_y << 16)	|
		    (contwr_mif->end_y));
	DIM_RDMA_WR(DI_CONTWR_CTRL,
		    contwr_mif->canvas_num	|
		    (urgent << 8));/* urgent. */
}

static void set_ma_pre_mif_g12(struct DI_SIM_MIF_s *mtnwr_mif,
			       struct DI_SIM_MIF_s *contprd_mif,
			       struct DI_SIM_MIF_s *contp2rd_mif,
			       struct DI_SIM_MIF_s *contwr_mif,
			       unsigned short urgent)
{
	DIM_RDMA_WR_BITS(CONTRD_SCOPE_X, contprd_mif->start_x, 0, 13);
	DIM_RDMA_WR_BITS(CONTRD_SCOPE_X, contprd_mif->end_x, 16, 13);
	DIM_RDMA_WR_BITS(CONTRD_SCOPE_Y, contprd_mif->start_y, 0, 13);
	DIM_RDMA_WR_BITS(CONTRD_SCOPE_Y, contprd_mif->end_y, 16, 13);
	DIM_RDMA_WR_BITS(CONTRD_CTRL1, contprd_mif->canvas_num, 16, 8);
	DIM_RDMA_WR_BITS(CONTRD_CTRL1, 2, 8, 2);
	DIM_RDMA_WR_BITS(CONTRD_CTRL1, 0, 0, 3);

	DIM_RDMA_WR_BITS(CONT2RD_SCOPE_X, contp2rd_mif->start_x, 0, 13);
	DIM_RDMA_WR_BITS(CONT2RD_SCOPE_X, contp2rd_mif->end_x, 16, 13);
	DIM_RDMA_WR_BITS(CONT2RD_SCOPE_Y, contp2rd_mif->start_y, 0, 13);
	DIM_RDMA_WR_BITS(CONT2RD_SCOPE_Y, contp2rd_mif->end_y, 16, 13);
	DIM_RDMA_WR_BITS(CONT2RD_CTRL1, contp2rd_mif->canvas_num, 16, 8);
	DIM_RDMA_WR_BITS(CONT2RD_CTRL1, 2, 8, 2);
	DIM_RDMA_WR_BITS(CONT2RD_CTRL1, 0, 0, 3);

	/* current field mtn canvas index. */
	DIM_RDMA_WR_BITS(MTNWR_X, mtnwr_mif->start_x, 16, 13);
	DIM_RDMA_WR_BITS(MTNWR_X, mtnwr_mif->end_x, 0, 13);
	DIM_RDMA_WR_BITS(MTNWR_X, 2, 30, 2);
	DIM_RDMA_WR_BITS(MTNWR_Y, mtnwr_mif->start_y, 16, 13);
	DIM_RDMA_WR_BITS(MTNWR_Y, mtnwr_mif->end_y, 0, 13);
	DIM_RDMA_WR_BITS(MTNWR_CTRL, mtnwr_mif->canvas_num, 0, 8);
	DIM_RDMA_WR_BITS(MTNWR_CAN_SIZE,
			 (mtnwr_mif->end_y - mtnwr_mif->start_y), 0, 13);
	DIM_RDMA_WR_BITS(MTNWR_CAN_SIZE,
			 (mtnwr_mif->end_x - mtnwr_mif->start_x), 16, 13);

	DIM_RDMA_WR_BITS(CONTWR_X, contwr_mif->start_x, 16, 13);
	DIM_RDMA_WR_BITS(CONTWR_X, contwr_mif->end_x, 0, 13);
	DIM_RDMA_WR_BITS(CONTWR_X, 2, 30, 2);
	DIM_RDMA_WR_BITS(CONTWR_Y, contwr_mif->start_y, 16, 13);
	DIM_RDMA_WR_BITS(CONTWR_Y, contwr_mif->end_y, 0, 13);
	DIM_RDMA_WR_BITS(CONTWR_CTRL, contwr_mif->canvas_num, 0, 8);
	DIM_RDMA_WR_BITS(CONTWR_CAN_SIZE,
			 (contwr_mif->end_y - contwr_mif->start_y), 0, 13);
	DIM_RDMA_WR_BITS(CONTWR_CAN_SIZE,
			 (contwr_mif->end_x - contwr_mif->start_x), 16, 13);
}

static void set_di_nrwr_mif(struct DI_SIM_MIF_s *nrwr_mif,
			    unsigned short urgent)
{
	DIM_RDMA_WR_BITS(DI_NRWR_X, nrwr_mif->end_x, 0, 14);
	DIM_RDMA_WR_BITS(DI_NRWR_X, nrwr_mif->start_x, 16, 14);
	DIM_RDMA_WR_BITS(DI_NRWR_Y, nrwr_mif->start_y, 16, 13);
	DIM_RDMA_WR_BITS(DI_NRWR_Y, nrwr_mif->end_y, 0, 13);
	/* wr ext en from gxtvbb */
	DIM_RDMA_WR_BITS(DI_NRWR_Y, 1, 15, 1);
	DIM_RDMA_WR_BITS(DI_NRWR_Y, 3, 30, 2);

	DIM_RDMA_WR_BITS(DI_NRWR_Y, nrwr_mif->bit_mode & 0x1, 14, 1);

	/*fix 1080i crash when di work on low speed*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL) &&
	    ((nrwr_mif->bit_mode & 0x3) == 0x3)) {
		DIM_RDMA_WR(DI_NRWR_CTRL,
			    nrwr_mif->canvas_num |
			    (urgent << 16)	 |
			    3 << 22		 |
			    1 << 24		 |
			    2 << 26		 | /*burst_lim 1->2 2->4*/
			    1 << 30); /* urgent bit 16 */
	} else {
		DIM_RDMA_WR(DI_NRWR_CTRL,
			    nrwr_mif->canvas_num |
			    (urgent << 16)	 |
			    1 << 24		 |
			    2 << 26		 | /*burst_lim 1->2 2->4*/
			    1 << 30);		   /* urgent bit 16 */
	}
}

void dimh_interrupt_ctrl(unsigned char ma_en,
			 unsigned char det3d_en, unsigned char nrds_en,
			 unsigned char post_wr, unsigned char mc_en)
{
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, ma_en ? 0 : 1, 17, 1);
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, ma_en ? 0 : 1, 20, 1);
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, mc_en ? 0 : 3, 22, 2);
	/* enable nr wr int */
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, 0, 16, 1);
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, post_wr ? 0 : 1, 18, 1);
	/* mask me interrupt hit abnormal */
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, 1, 21, 1);
	/* mask hist interrupt */
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, 1, 19, 1);
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, det3d_en ? 0 : 1, 24, 1);
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, nrds_en ? 0 : 1, 25, 1);
	/* clean all pending interrupt bits */
	DIM_RDMA_WR_BITS(DI_INTR_CTRL, 0xffff, 0, 16);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, 3, 30, 2);
	else
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, 0, 30, 2);
}

void dimh_int_ctr(unsigned int set_mod, unsigned char ma_en,
		  unsigned char det3d_en, unsigned char nrds_en,
		  unsigned char post_wr, unsigned char mc_en)
{
	static unsigned char lst_ma, lst_det3d, lst_nrds, lst_pw, lst_mc;

	if (set_mod == 0) {
		/*int:*/
		lst_ma = 1;
		lst_det3d = 0;
		lst_nrds = 1;
		lst_pw = 1;
		lst_mc = 1;
		dimh_interrupt_ctrl(lst_ma,
				    lst_det3d,
				    lst_nrds,
				    lst_pw,
				    lst_mc);
		return;
	}

	if (ma_en != lst_ma) {
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, ma_en ? 0 : 1, 17, 1);
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, ma_en ? 0 : 1, 20, 1);
		lst_ma = ma_en;
	}
	if (mc_en != lst_mc) {
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, mc_en ? 0 : 3, 22, 2);
		lst_mc = mc_en;
	}

	if (post_wr != lst_pw) {
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, post_wr ? 0 : 1, 18, 1);
		lst_pw = post_wr;
	}

	if (det3d_en != lst_det3d) {
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, det3d_en ? 0 : 1, 24, 1);
		lst_det3d = det3d_en;
	}

	if (nrds_en != lst_nrds) {
		DIM_RDMA_WR_BITS(DI_INTR_CTRL, nrds_en ? 0 : 1, 25, 1);
		lst_nrds = nrds_en;
	}
}

void dimh_enable_di_pre_aml(struct DI_MIF_s *di_inp_mif,
			    struct DI_MIF_s *di_mem_mif,
			    struct DI_MIF_s *di_chan2_mif,
			    struct DI_SIM_MIF_s *di_nrwr_mif,
			    struct DI_SIM_MIF_s *di_mtnwr_mif,
			    struct DI_SIM_MIF_s *di_contp2rd_mif,
			    struct DI_SIM_MIF_s *di_contprd_mif,
			    struct DI_SIM_MIF_s *di_contwr_mif,
			    unsigned char madi_en,
			    unsigned char pre_field_num,
			    unsigned char pre_vdin_link)
{
	bool mem_bypass = false, chan2_disable = false;
	unsigned short nrwr_hsize = 0, nrwr_vsize = 0;
	unsigned short chan2_hsize = 0, chan2_vsize = 0;
	unsigned short mem_hsize = 0, mem_vsize = 0;

	set_di_inp_mif(di_inp_mif,
		       dimp_get(edi_mp_pre_urgent),
		       dimp_get(edi_mp_pre_hold_line));
	set_di_nrwr_mif(di_nrwr_mif,
			dimp_get(edi_mp_pre_urgent));
	set_di_mem_mif(di_mem_mif,
		       dimp_get(edi_mp_pre_urgent),
		       dimp_get(edi_mp_pre_hold_line));
	set_di_chan2_mif(di_chan2_mif,
			 dimp_get(edi_mp_pre_urgent),
			 dimp_get(edi_mp_pre_hold_line));

	nrwr_hsize = di_nrwr_mif->end_x -
		di_nrwr_mif->start_x + 1;
	nrwr_vsize = di_nrwr_mif->end_y -
		di_nrwr_mif->start_y + 1;
	chan2_hsize = di_chan2_mif->luma_x_end0 -
		di_chan2_mif->luma_x_start0 + 1;
	chan2_vsize = di_chan2_mif->luma_y_end0 -
		di_chan2_mif->luma_y_start0 + 1;
	mem_hsize = di_mem_mif->luma_x_end0 -
		di_mem_mif->luma_x_start0 + 1;
	mem_vsize = di_mem_mif->luma_y_end0 -
		di_mem_mif->luma_y_start0 + 1;
	if (chan2_hsize != nrwr_hsize || chan2_vsize != nrwr_vsize)
		chan2_disable = true;
	if (mem_hsize != nrwr_hsize || mem_vsize != nrwr_vsize)
		mem_bypass = true;
	/*
	 * enable&disable contwr txt
	 */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B))
		DIM_RDMA_WR_BITS(DI_MTN_CTRL, madi_en ? 5 : 0, 29, 3);
	else
		DIM_RDMA_WR_BITS(DI_MTN_1_CTRL1, madi_en ? 5 : 0, 29, 3);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (madi_en) {
			set_ma_pre_mif_g12(di_mtnwr_mif,
					   di_contprd_mif,
					   di_contp2rd_mif,
					   di_contwr_mif,
					   dimp_get(edi_mp_pre_urgent));
		} else {
			chan2_disable = true;
		}
		DIM_RDMA_WR_BITS(DI_PRE_GL_THD,
				 dimp_get(edi_mp_pre_hold_line), 16, 6);
		if (dimp_get(edi_mp_pre_ctrl))
			DIM_RDMA_WR_BITS(DI_PRE_CTRL,
					 dimp_get(edi_mp_pre_ctrl), 0, 29);
		else
			DIM_RDMA_WR(DI_PRE_CTRL,
				    1			| /* nr wr en */
				    (madi_en << 1)	| /* mtn en */
				    (madi_en << 2)	| /* check3:2pulldown*/
				    (madi_en << 3)	| /* check2:2pulldown*/
				    (1 << 4)		|
				    (madi_en << 5)	| /*hist check enable*/
				/* hist check  use chan2. */
				    (madi_en << 6)	|
				/*hist check use data before noise reduction.*/
				    ((chan2_disable ? 0 : 1) << 8) |
				/* chan 2 enable for 2:2 pull down check.*/
				/* line buffer 2 enable */
				    ((chan2_disable ? 0 : 1) << 9) |
				    (0 << 10)		| /* pre drop first. */
				    (1 << 11)		| /* nrds mif enable */
				    (0 << 12)		| /* pre viu link */
				    (pre_vdin_link << 13)	   |
				/* pre go line link */
				    (pre_vdin_link << 14)	   |
				    (1 << 21)		| /*invertNRfield num*/
				    (1 << 22)		| /* MTN after NR. */
				    (0 << 25)		| /* contrd en */
				    ((mem_bypass ? 1 : 0) << 28)   |
				    pre_field_num << 29);
	} else {
		if (madi_en) {
			set_ma_pre_mif(di_mtnwr_mif,
				       di_contprd_mif,
				       di_contp2rd_mif,
				       di_contwr_mif,
				       dimp_get(edi_mp_pre_urgent));
		}
		DIM_RDMA_WR(DI_PRE_CTRL,
			    1			/* nr enable */
			    | (madi_en << 1)	/* mtn_en */
			    | (madi_en << 2)	/* check 3:2 pulldown */
			    | (madi_en << 3)	/* check 2:2 pulldown */
			    | (1 << 4)
			    | (madi_en << 5)	/* hist check enable */
			    | (1 << 6)		/* hist check  use chan2. */
			    | (0 << 7)
			    /* hist check use data before noise reduction. */
			    | (madi_en << 8)
			    /* chan 2 enable for 2:2 pull down check.*/
			    | (madi_en << 9)	/* line buffer 2 enable */
			    | (0 << 10)		/* pre drop first. */
			    | (0 << 11)		/* di pre repeat */
			    | (0 << 12)		/* pre viu link */
			    | (pre_vdin_link << 13)
			    | (pre_vdin_link << 14)	/* pre go line link */
			    | (dimp_get(edi_mp_pre_hold_line) << 16)
			    /* pre hold line number */
			    | (1 << 22)		/* MTN after NR. */
			    | (madi_en << 25)	/* contrd en */
			    | (pre_field_num << 29)	/* pre field number.*/
			    );
	}
}

/*
 * after g12a, framereset will not reset simple
 * wr mif of pre such as mtn&cont&mv&mcinfo wr
 */
static const unsigned int reg_AFBC[AFBC_DEC_NUB][AFBC_REG_INDEX_NUB] = {
	{
		AFBC_ENABLE,
		AFBC_MODE,
		AFBC_SIZE_IN,
		AFBC_DEC_DEF_COLOR,
		AFBC_CONV_CTRL,
		AFBC_LBUF_DEPTH,
		AFBC_HEAD_BADDR,
		AFBC_BODY_BADDR,
		AFBC_SIZE_OUT,
		AFBC_OUT_YSCOPE,
		AFBC_STAT,
		AFBC_VD_CFMT_CTRL,
		AFBC_VD_CFMT_W,
		AFBC_MIF_HOR_SCOPE,
		AFBC_MIF_VER_SCOPE,
		AFBC_PIXEL_HOR_SCOPE,
		AFBC_PIXEL_VER_SCOPE,
		AFBC_VD_CFMT_H,
	},
	{
		VD2_AFBC_ENABLE,
		VD2_AFBC_MODE,
		VD2_AFBC_SIZE_IN,
		VD2_AFBC_DEC_DEF_COLOR,
		VD2_AFBC_CONV_CTRL,
		VD2_AFBC_LBUF_DEPTH,
		VD2_AFBC_HEAD_BADDR,
		VD2_AFBC_BODY_BADDR,
		VD2_AFBC_OUT_XSCOPE,
		VD2_AFBC_OUT_YSCOPE,
		VD2_AFBC_STAT,
		VD2_AFBC_VD_CFMT_CTRL,
		VD2_AFBC_VD_CFMT_W,
		VD2_AFBC_MIF_HOR_SCOPE,
		VD2_AFBC_MIF_VER_SCOPE,
		VD2_AFBC_PIXEL_HOR_SCOPE,
		VD2_AFBC_PIXEL_VER_SCOPE,
		VD2_AFBC_VD_CFMT_H,

	},

};

#define AFBC_DEC_SEL	(EAFBC_DEC1)

static enum EAFBC_DEC afbc_get_decnub(void)
{
	enum EAFBC_DEC sel_dec = EAFBC_DEC0;

	if (is_meson_gxl_cpu())
		sel_dec = EAFBC_DEC0;
	else if (is_meson_txlx_cpu())
		sel_dec = EAFBC_DEC1;
	else if (is_meson_g12a_cpu())
		sel_dec = AFBC_DEC_SEL;
	/* TL1 only have AFBC0 */
	else if (is_meson_tl1_cpu())
		sel_dec = EAFBC_DEC0;
	return sel_dec;
}

static const unsigned int *afbc_get_regbase(void)
{
	return &reg_AFBC[afbc_get_decnub()][0];
}

bool dimh_afbc_is_supported(void)
{
	bool ret = false;

	/*currently support txlx and g12a*/
	if (is_meson_txlx_cpu()	||
	    is_meson_g12a_cpu()
		/*|| is_meson_tl1_cpu()*/)
		ret = false;
	return ret;
}

static void afbc_sw_trig(bool  on);

void dimh_enable_afbc_input(struct vframe_s *vf)
{
	unsigned int r, u, v, w_aligned, h_aligned;
	unsigned int out_height = 0;
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 0;
	const unsigned int *reg = afbc_get_regbase();

	if (!dimh_afbc_is_supported())
		return;

	if ((vf->type & VIDTYPE_COMPRESS)) {
		/* only reg for the first time*/
		dimh_afbc_reg_sw(true);
		afbc_sw_trig(true);
	} else {
		afbc_sw_trig(false);
		return;
	}
	w_aligned = round_up((vf->width - 1), 32);
	h_aligned = round_up((vf->height - 1), 4);
	r = (3 << 24)	|
	    (10 << 16)	|
	    (1 << 14)	| /*burst1 1*/
	    (vf->bitdepth & BITDEPTH_MASK);
	if (vf->bitdepth & BITDEPTH_SAVING_MODE)
		r |= (1 << 28); /* mem_saving_mode */
	if (vf->type & VIDTYPE_SCATTER)
		r |= (1 << 29);
	out_height = h_aligned;
	if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		r |= 0x40;
		vt_ini_phase = 0xc;
		out_height = h_aligned >> 1;
	} else if ((vf->type & VIDTYPE_TYPEMASK) ==
			VIDTYPE_INTERLACE_BOTTOM) {
		r |= 0x80;
		vt_ini_phase = 0x4;
		vfmt_rpt_first = 0;
		out_height = h_aligned >> 1;
	}
	DIM_RDMA_WR(reg[EAFBC_MODE], r);
	r = 0x100;
	/* TL1 add bit[13:12]: fmt_mode; 0:yuv444; 1:yuv422; 2:yuv420
	 * di does not support yuv444, so for fmt yuv444 di will bypass+
	 */
	if (is_meson_tl1_cpu()) {
		if (vf->type & VIDTYPE_VIU_444)
			r |= (0 << 12);
		else if (vf->type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
	}
	DIM_RDMA_WR(reg[EAFBC_CONV_CTRL], r);
	u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
	v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
	DIM_RDMA_WR(reg[EAFBC_DEC_DEF_COLOR],
		    0x3FF00000		| /*Y,bit20+*/
		    0x80 << (u + 10)	|
		    0x80 << v);
	/* chroma formatter */
	DIM_RDMA_WR(reg[EAFBC_VD_CFMT_CTRL],
		    (1 << 21)			| /* HFORMATTER_YC_RATIO_2_1 */
		    (1 << 20)			| /* HFORMATTER_EN */
		    (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
		    (vt_ini_phase << 8)		|
		    (16 << 1)			| /* VFORMATTER_PHASE_BIT */
		    0);				  /* different with inp */

	DIM_RDMA_WR(reg[EAFBC_VD_CFMT_W],
		    (w_aligned << 16) | (w_aligned / 2));
	DIM_RDMA_WR(reg[EAFBC_MIF_HOR_SCOPE],
		    (0 << 16) | ((w_aligned >> 5) - 1));
	DIM_RDMA_WR(reg[EAFBC_MIF_VER_SCOPE],
		    (0 << 16) | ((h_aligned >> 2) - 1));

	DIM_RDMA_WR(reg[EAFBC_PIXEL_HOR_SCOPE],
		    (0 << 16) | (vf->width - 1));
	DIM_RDMA_WR(reg[EAFBC_VD_CFMT_H], out_height);

	DIM_RDMA_WR(reg[EAFBC_PIXEL_VER_SCOPE],
		    0 << 16 | (vf->height - 1));
	DIM_RDMA_WR(reg[EAFBC_SIZE_IN], h_aligned | w_aligned << 16);
	DIM_RDMA_WR(reg[EAFBC_SIZE_OUT], out_height | w_aligned << 16);
	DIM_RDMA_WR(reg[EAFBC_HEAD_BADDR], vf->compHeadAddr >> 4);
	DIM_RDMA_WR(reg[EAFBC_BODY_BADDR], vf->compBodyAddr >> 4);
}

static void afbcx_power_sw(enum EAFBC_DEC decsel, bool on)	/*g12a*/
{
	unsigned int reg_ctrl;

	if (decsel == EAFBC_DEC0)
		reg_ctrl = VD1_AFBCD0_MISC_CTRL;
	else
		reg_ctrl = VD2_AFBCD1_MISC_CTRL;
	if (on)
		DIM_RDMA_WR_BITS(reg_ctrl, 0, 0, 8);
	else
		DIM_RDMA_WR_BITS(reg_ctrl, 0x55, 0, 8);
}

static void afbcx_sw(bool on)	/*g12a*/
{
	unsigned int tmp;
	unsigned int mask;
	unsigned int reg_ctrl, reg_en;
	enum EAFBC_DEC dec_sel;

	dec_sel = afbc_get_decnub();

	if (dec_sel == EAFBC_DEC0) {
		reg_ctrl = VD1_AFBCD0_MISC_CTRL;
		reg_en = AFBC_ENABLE;
	} else {
		reg_ctrl = VD2_AFBCD1_MISC_CTRL;
		reg_en = VD2_AFBC_ENABLE;
	}

	mask = (3 << 20)  | (1 << 12) | (1 << 9);
	/*clear*/
	tmp = DIM_RDMA_RD(reg_ctrl) & (~mask);

	if (on) {
		tmp = tmp		|
			(2 << 20)	|
			(1 << 12)	|
			(1 << 9);
		DIM_RDMA_WR(reg_ctrl, tmp);
		DIM_RDMA_WR_BITS(VD2_AFBCD1_MISC_CTRL,
				 (reg_ctrl == VD1_AFBCD0_MISC_CTRL) ? 0 : 1,
				 8, 1);
		DIM_RDMA_WR(reg_en, 0x1600);
		DIM_RDMA_WR_BITS(VIUB_MISC_CTRL0, 1, 16, 1);
		/*TL1 add mem control bit */
		if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
			DIM_RDMA_WR_BITS(VD1_AFBCD0_MISC_CTRL, 1, 22, 1);
	} else {
		DIM_RDMA_WR(reg_ctrl, tmp);
		DIM_RDMA_WR(reg_en, 0x1600);
		DIM_RDMA_WR_BITS(VIUB_MISC_CTRL0, 0, 16, 1);
		if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
			DIM_RDMA_WR_BITS(VD1_AFBCD0_MISC_CTRL, 0, 22, 1);
	}
#ifdef MARK_HIS
	PR_INF("%s,on[%d],CTRL[0x%x],en[0x%x]\n", __func__, on,
	       DIM_RDMA_RD(VD1_AFBCD0_MISC_CTRL),
	       DIM_RDMA_RD(VD1_AFBCD0_MISC_CTRL));
#endif
}

static void afbc_sw_old(bool on)/*txlx*/
{
	enum EAFBC_DEC dec_sel;
	unsigned int reg_en;

	dec_sel = afbc_get_decnub();

	if (dec_sel == EAFBC_DEC0) {
		/*reg_ctrl = VD1_AFBCD0_MISC_CTRL;*/
		reg_en = AFBC_ENABLE;
	} else {
		/*reg_ctrl = VD2_AFBCD1_MISC_CTRL;*/
		reg_en = VD2_AFBC_ENABLE;
	}

	if (on) {
		/* DI inp(current data) switch to AFBC */
		if (DIM_RDMA_RD_BITS(VIU_MISC_CTRL0, 29, 1) != 1)
			DIM_RDMA_WR_BITS(VIU_MISC_CTRL0, 1, 29, 1);
		if (DIM_RDMA_RD_BITS(VIUB_MISC_CTRL0, 16, 1) != 1)
			DIM_RDMA_WR_BITS(VIUB_MISC_CTRL0, 1, 16, 1);
		if (DIM_RDMA_RD_BITS(VIU_MISC_CTRL1, 0, 1) != 1)
			DIM_RDMA_WR_BITS(VIU_MISC_CTRL1, 1, 0, 1);
		if (dec_sel == EAFBC_DEC0) {
			/*gxl only?*/
			if (DIM_RDMA_RD_BITS(VIU_MISC_CTRL0, 19, 1) != 1)
				DIM_RDMA_WR_BITS(VIU_MISC_CTRL0, 1, 19, 1);
		}
		if (DIM_RDMA_RD(reg_en) != 0x1600)
			DIM_RDMA_WR(reg_en, 0x1600);

	} else {
		DIM_RDMA_WR(reg_en, 0);
		/* afbc to vpp(replace vd1) enable */

		if (DIM_RDMA_RD_BITS(VIU_MISC_CTRL1, 0, 1) != 0	||
		    DIM_RDMA_RD_BITS(VIUB_MISC_CTRL0, 16, 1) != 0) {
			DIM_RDMA_WR_BITS(VIU_MISC_CTRL1, 0, 0, 1);
			DIM_RDMA_WR_BITS(VIUB_MISC_CTRL0, 0, 16, 1);
		}
	}
}

static bool afbc_is_used(void)
{
	bool ret = false;

	if (DIM_RDMA_RD_BITS(VIUB_MISC_CTRL0, 16, 1) == 1)
		ret = true;

	/*dim_print("%s:%d\n",__func__,ret);*/

	return ret;
}

static void afbc_power_sw(bool on)
{
	/*afbc*/
	enum EAFBC_DEC dec_sel;
	unsigned int vpu_sel;

	dec_sel = afbc_get_decnub();
	if (dec_sel == EAFBC_DEC0)
		vpu_sel = VPU_AFBC_DEC;
	else
		vpu_sel = VPU_AFBC_DEC1;

	ext_ops.dim_vpu_mem_pd_vmod(vpu_sel, on);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		afbcx_power_sw(dec_sel, on);
}

static int afbc_reg_unreg_flag;
void dimh_afbc_reg_sw(bool on)
{
	if (!dimh_afbc_is_supported())
		return;

	if (on && !afbc_reg_unreg_flag) {
		afbc_power_sw(true);
		afbc_reg_unreg_flag = 1;
	}
	if (!on && afbc_reg_unreg_flag) {
		afbc_sw_trig(false);
		afbc_power_sw(false);
		afbc_reg_unreg_flag = 0;
	}
}

static void afbc_sw(bool on)
{
	if (is_meson_gxl_cpu() || is_meson_txlx_cpu())
		afbc_sw_old(on);
	else
		afbcx_sw(on);
}

static void afbc_sw_trig(bool  on)
{
	afbc_sw(on);
}

static void afbc_input_sw(bool on)
{
	const unsigned int *reg = afbc_get_regbase();
	unsigned int reg_AFBC_ENABLE;

	if (!dimh_afbc_is_supported())
		return;

	reg_AFBC_ENABLE = reg[EAFBC_ENABLE];

	/*dim_print("%s:0x%x\n", __func__,reg_AFBC_ENABLE);*/
	if (on)
		DIM_RDMA_WR_BITS(reg_AFBC_ENABLE, 1, 8, 1);
	else
		DIM_RDMA_WR_BITS(reg_AFBC_ENABLE, 0, 8, 1);
}

void dimh_enable_mc_di_pre_g12(struct DI_MC_MIF_s *mcinford_mif,
			       struct DI_MC_MIF_s *mcinfowr_mif,
			       struct DI_MC_MIF_s *mcvecwr_mif,
			       unsigned char mcdi_en)
{
	DIM_RDMA_WR_BITS(MCDI_MOTINEN, (mcdi_en ? 3 : 0), 0, 2);
	if (is_meson_g12a_cpu()	||
	    is_meson_g12b_cpu()	||
	    is_meson_sm1_cpu())
		DIM_RDMA_WR(MCDI_CTRL_MODE, (mcdi_en ? 0x1bfef7ff : 0));
	else
		DIM_RDMA_WR(MCDI_CTRL_MODE, (mcdi_en ? 0x1bfff7ff : 0));

	DIM_RDMA_WR_BITS(DI_PRE_CTRL, (mcdi_en ? 3 : 0), 16, 2);

	DIM_RDMA_WR_BITS(MCINFRD_SCOPE_X, mcinford_mif->size_x, 16, 13);
	DIM_RDMA_WR_BITS(MCINFRD_SCOPE_Y, mcinford_mif->size_y, 16, 13);
	DIM_RDMA_WR_BITS(MCINFRD_CTRL1, mcinford_mif->canvas_num, 16, 8);
	DIM_RDMA_WR_BITS(MCINFRD_CTRL1, 2, 0, 3);

	DIM_RDMA_WR_BITS(MCVECWR_X, mcvecwr_mif->size_x, 0, 13);
	DIM_RDMA_WR_BITS(MCVECWR_Y, mcvecwr_mif->size_y, 0, 13);
	DIM_RDMA_WR_BITS(MCVECWR_CTRL, mcvecwr_mif->canvas_num, 0, 8);
	DIM_RDMA_WR_BITS(MCVECWR_CAN_SIZE, mcvecwr_mif->size_y, 0, 13);
	DIM_RDMA_WR_BITS(MCVECWR_CAN_SIZE, mcvecwr_mif->size_x, 16, 13);

	DIM_RDMA_WR_BITS(MCINFWR_X, mcinfowr_mif->size_x, 0, 13);
	DIM_RDMA_WR_BITS(MCINFWR_Y, mcinfowr_mif->size_y, 0, 13);
	DIM_RDMA_WR_BITS(MCINFWR_CTRL, mcinfowr_mif->canvas_num, 0, 8);
	DIM_RDMA_WR_BITS(MCINFWR_CAN_SIZE, mcinfowr_mif->size_y, 0, 13);
	DIM_RDMA_WR_BITS(MCINFWR_CAN_SIZE, mcinfowr_mif->size_x, 16, 13);
}

void dimh_enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
			   struct DI_MC_MIF_s *di_mcinfowr_mif,
			   struct DI_MC_MIF_s *di_mcvecwr_mif,
			   unsigned char mcdi_en)
{
	bool me_auto_en = true;
	unsigned int ctrl_mode = 0;

	DIM_RDMA_WR_BITS(DI_MTN_CTRL1, (mcdi_en ? 3 : 0), 12, 2);
	if (is_meson_gxlx_cpu() || is_meson_txhd_cpu())
		me_auto_en = false;

	ctrl_mode = (me_auto_en ? 0x1bfff7ff : 0x1bfe37ff);
	DIM_RDMA_WR(MCDI_CTRL_MODE, (mcdi_en ? ctrl_mode : 0));
	DIM_RDMA_WR_BITS(MCDI_MOTINEN, (mcdi_en ? 3 : 0), 0, 2);

	DIM_RDMA_WR(MCDI_MCVECWR_X, di_mcvecwr_mif->size_x);
	DIM_RDMA_WR(MCDI_MCVECWR_Y, di_mcvecwr_mif->size_y);
	DIM_RDMA_WR(MCDI_MCINFOWR_X, di_mcinfowr_mif->size_x);
	DIM_RDMA_WR(MCDI_MCINFOWR_Y, di_mcinfowr_mif->size_y);

	DIM_RDMA_WR(MCDI_MCINFORD_X, di_mcinford_mif->size_x);
	DIM_RDMA_WR(MCDI_MCINFORD_Y, di_mcinford_mif->size_y);
	DIM_RDMA_WR(MCDI_MCVECWR_CANVAS_SIZE,
		    (di_mcvecwr_mif->size_x << 16) + di_mcvecwr_mif->size_y);
	DIM_RDMA_WR(MCDI_MCINFOWR_CANVAS_SIZE,
		    (di_mcinfowr_mif->size_x << 16) + di_mcinfowr_mif->size_y);
	DIM_RDMA_WR(MCDI_MCINFORD_CANVAS_SIZE,
		    (di_mcinford_mif->size_x << 16) + di_mcinford_mif->size_y);

	DIM_RDMA_WR(MCDI_MCVECWR_CTRL,
		    di_mcvecwr_mif->canvas_num	|
		    (0 << 14)			| /* sync latch en */
		    (dimp_get(edi_mp_pre_urgent) << 8) | /* urgent */
		    (1 << 12)			| /*enable reset by frame rst*/
		    (0x4031 << 16));
	DIM_RDMA_WR(MCDI_MCINFOWR_CTRL,
		    di_mcinfowr_mif->canvas_num	|
		    (0 << 14)			| /* sync latch en */
		    (dimp_get(edi_mp_pre_urgent) << 8) | /* urgent */
		    (1 << 12)			| /*enable reset by frame rst*/
		    (0x4042 << 16));
	DIM_RDMA_WR(MCDI_MCINFORD_CTRL,
		    di_mcinford_mif->canvas_num |
		    (0 << 10)			|  /* sync latch en */
		    (dimp_get(edi_mp_pre_urgent) << 8) | /* urgent */
		    (1 << 9)			| /*enable reset by frame rst*/
		    (0x42 << 16));
}

void dimh_en_mc_di_post_g12(struct DI_MC_MIF_s *mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv)
{
	unsigned int end_x;

	DIM_VSYNC_WR_MPEG_REG(MCVECRD_CTRL1,
			      mcvecrd_mif->canvas_num << 16	|
			      2 << 8				|
			      (reverse ? 3 : 0) << 4		|
			      2);
	end_x = mcvecrd_mif->size_x + mcvecrd_mif->start_x;
	DIM_VSYNC_WR_MPEG_REG(MCVECRD_SCOPE_X,
			      mcvecrd_mif->start_x	|
			      end_x << 16);
	DIM_VSYNC_WR_MPEG_REG(MCVECRD_SCOPE_Y,
			      (reverse ? 1 : 0) << 30	|
			      mcvecrd_mif->start_y	|
			      mcvecrd_mif->end_y << 16);
	DIM_VSC_WR_MPG_BT(MCVECRD_CTRL2, urgent, 16, 1);
	DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, mcvecrd_mif->vecrd_offset, 12, 3);
	if (mcvecrd_mif->blend_en) {
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
				  dimp_get(edi_mp_mcen_mode), 0, 2);
		if (!di_cfg_top_get(EDI_CFG_REF_2)) {
			/*(!dimp_get(edi_mp_post_wr_en)) {*/
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 1, 11, 1);
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 3, 18, 2);
		} else {/*OTT-3210*/
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 11, 1);
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 2, 18, 2);
		}
	} else {
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 11, 1);
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 2, 18, 2);
	}
	DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, dimp_get(edi_mp_mcuv_en), 10, 1);
	DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, invert_mv, 17, 1);
	DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, dimp_get(edi_mp_mcdebug_mode), 2, 3);
}

void dimh_enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv)
{
	di_mcvecrd_mif->size_y =
			(di_mcvecrd_mif->end_y - di_mcvecrd_mif->start_y + 1);
	DIM_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_X,
			      (reverse ? 1 : 0) << 30 |
			      di_mcvecrd_mif->start_x << 16 |
			      (di_mcvecrd_mif->size_x +
			      di_mcvecrd_mif->start_x));
	DIM_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_Y,
			      (reverse ? 1 : 0) << 30		|
			      di_mcvecrd_mif->start_y << 16	|
			      di_mcvecrd_mif->end_y);
	DIM_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_CANVAS_SIZE,
			      (di_mcvecrd_mif->size_x << 16) |
			      di_mcvecrd_mif->size_y);
	DIM_VSYNC_WR_MPEG_REG(MCDI_MCVECRD_CTRL,
			      di_mcvecrd_mif->canvas_num	|
			      (urgent << 8)		| /* urgent */
			      (1 << 9)			| /* canvas enable */
			      (0 << 10)			|
			      (0x31 << 16));
	DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, di_mcvecrd_mif->vecrd_offset, 12, 3);
	if (di_mcvecrd_mif->blend_en)
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
				  dimp_get(edi_mp_mcen_mode), 0, 2);
	else
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
				  dimp_get(edi_mp_mcuv_en), 10, 1);
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 1, 11, 1);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, invert_mv, 17, 1);
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 3, 18, 2);
		}
	} else {
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
				  dimp_get(edi_mp_mcuv_en), 9, 1);
	}
	DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, dimp_get(edi_mp_mcdebug_mode), 2, 3);
}

static void set_di_inp_fmt_more(unsigned int repeat_l0_en,
				int hz_yc_ratio,		/* 2bit */
				int hz_ini_phase,		/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,		/* 2bit */
				int vt_ini_phase,		/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
		)
{
	int hfmt_en = 1, nrpt_phase0_en = 0;
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_RDMA_WR(DI_INP_FMT_CTRL,
		    (hz_rpt << 28)		| /* hz rpt pixel */
		    (hz_ini_phase << 24)	| /* hz ini phase */
		    (0 << 23)			| /* repeat p0 enable */
		    (hz_yc_ratio << 21)		| /* hz yc ratio */
		    (hfmt_en << 20)		| /* hz enable */
		    (nrpt_phase0_en << 17)	| /* nrpt_phase0 enable */
		    (repeat_l0_en << 16)	| /* repeat l0 enable */
		    (0 << 12)			| /* skip line num */
		    (vt_ini_phase << 8)		| /* vt ini phase */
		    (vt_phase_step << 1)	| /* vt phase step (3.4) */
		    (vfmt_en << 0)		  /* vt enable */
		    );

	DIM_RDMA_WR(DI_INP_FMT_W,
		    (y_length << 16) |	/* hz format width */
		    (c_length << 0)	/* vt format width */
		    );
}

static void set_di_inp_mif(struct DI_MIF_s *mif, int urgent, int hold_line)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl, vfmt_rpt_first = 0;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int vt_ini_phase = 0;
	unsigned int reset_on_gofield;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = mif->src_prog ? 0 : 1;
		chroma0_rpt_loop_end = mif->src_prog ? 0 : 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = mif->src_prog ? 0 : 0x80;

		vfmt_rpt_first = 1;
		if (mif->output_field_num == 0)
			vt_ini_phase = 0xe;
		else
			vt_ini_phase = 0xa;

		if (mif->src_prog) {
			if (mif->output_field_num == 0) {
				vt_ini_phase = 0xc;
			} else {
				vt_ini_phase = 0x4;
				vfmt_rpt_first = 0;
			}
		}

	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}

	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;

	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = 1;/* default enable according to vlsi */
	DIM_RDMA_WR(DI_INP_GEN_REG,
		    (reset_on_gofield << 29)	|
		    (urgent << 28)		| /* chroma urgent bit */
		    (urgent << 27)		| /* luma urgent bit. */
		    (1 << 25)			| /* no dummy data. */
		    (hold_line << 19)		| /* hold lines */
		    (1 << 18)			| /* push dummy pixel */
		    (demux_mode << 16)		| /* demux_mode */
		    (bytes_per_pixel << 14)	|
		    (1 << 12)			| /*burst_size_cr*/
		    (1 << 10)			| /*burst_size_cb*/
		    (3 << 8)			| /*burst_size_y*/
		    (chro_rpt_lastl_ctrl << 6)	|
		    ((mif->set_separate_en != 0) << 1) |
		    (0 << 0)/* cntl_enable */
		    );
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		DIM_RDMA_WR_BITS(DI_INP_GEN_REG2, 1, 0, 1);
	} else {
		DIM_RDMA_WR_BITS(DI_INP_GEN_REG2, 0, 0, 1);
	}

	DIM_RDMA_WR_BITS(DI_INP_GEN_REG3, mif->bit_mode & 0x3, 8, 2);
	DIM_RDMA_WR(DI_INP_CANVAS0,
		    (mif->canvas0_addr2 << 16)	| /* cntl_canvas0_addr2 */
		    (mif->canvas0_addr1 << 8)	| /* cntl_canvas0_addr1 */
		    (mif->canvas0_addr0 << 0)	  /* cntl_canvas0_addr0 */
	);

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_RDMA_WR(DI_INP_LUMA_X0, (mif->luma_x_end0 << 16) |
			/* cntl_luma_x_end0 */
			(mif->luma_x_start0 << 0)/* cntl_luma_x_start0 */
		);
	DIM_RDMA_WR(DI_INP_LUMA_Y0, (mif->luma_y_end0 << 16) |
			/* cntl_luma_y_end0 */
			(mif->luma_y_start0 << 0) /* cntl_luma_y_start0 */
		);
	DIM_RDMA_WR(DI_INP_CHROMA_X0, (mif->chroma_x_end0 << 16) |
			(mif->chroma_x_start0 << 0));
	DIM_RDMA_WR(DI_INP_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		   (mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_RDMA_WR(DI_INP_RPT_LOOP,
		    (0 << 28) |
		    (0 << 24) |
		    (0 << 20) |
		    (0 << 16) |
		    (chroma0_rpt_loop_start << 12)	|
		    (chroma0_rpt_loop_end << 8)		|
		    (luma0_rpt_loop_start << 4)		|
		    (luma0_rpt_loop_end << 0)
		    );

	DIM_RDMA_WR(DI_INP_LUMA0_RPT_PAT, luma0_rpt_loop_pat);
	DIM_RDMA_WR(DI_INP_CHROMA0_RPT_PAT, chroma0_rpt_loop_pat);

	/* Dummy pixel value */
	DIM_RDMA_WR(DI_INP_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) {/* 4:2:0 block mode.*/
		set_di_inp_fmt_more(vfmt_rpt_first,/* hfmt_en */
				    1,/* hz_yc_ratio */
				    0,/* hz_ini_phase */
				    1,/* vfmt_en */
				    mif->src_prog ? 0 : 1,/* vt_yc_ratio */
				    vt_ini_phase,/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    /* y_length */
				mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
				    /* c length */
				    0);	/* hz repeat. */
	} else {
		set_di_inp_fmt_more(vfmt_rpt_first,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,	/* hz_ini_phase */
				    0,	/* vfmt_en */
				    0,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    ((mif->luma_x_end0 >> 1) -
					(mif->luma_x_start0 >> 1) + 1),
				    0); /* hz repeat. */
	}
}

static void set_di_mem_fmt_more(int hfmt_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
		)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_RDMA_WR(DI_MEM_FMT_CTRL,
		    (hz_rpt << 28)		| /* hz rpt pixel */
		    (hz_ini_phase << 24)	| /* hz ini phase */
		    (0 << 23)			| /* repeat p0 enable */
		    (hz_yc_ratio << 21)		| /* hz yc ratio */
		    (hfmt_en << 20)		| /* hz enable */
		    (1 << 17)			| /* nrpt_phase0 enable */
		    (0 << 16)			| /* repeat l0 enable */
		    (0 << 12)			| /* skip line num */
		    (vt_ini_phase << 8)		| /* vt ini phase */
		    (vt_phase_step << 1)	| /* vt phase step (3.4) */
		    (vfmt_en << 0)		/* vt enable */
		    );

	DIM_RDMA_WR(DI_MEM_FMT_W,
		    (y_length << 16) |	/* hz format width */
		    (c_length << 0)	/* vt format width */
		    );
}

static void set_di_chan2_fmt_more(int hfmt_en,
				  int hz_yc_ratio,/* 2bit */
				  int hz_ini_phase,/* 4bit */
				  int vfmt_en,
				  int vt_yc_ratio,/* 2bit */
				  int vt_ini_phase,/* 4bit */
				  int y_length,
				  int c_length,
				  int hz_rpt	/* 1bit */
				  )
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_RDMA_WR(DI_CHAN2_FMT_CTRL,
		    (hz_rpt << 28)		| /* hz rpt pixel */
		    (hz_ini_phase << 24)	| /* hz ini phase */
		    (0 << 23)			| /* repeat p0 enable */
		    (hz_yc_ratio << 21)		| /* hz yc ratio */
		    (hfmt_en << 20)		| /* hz enable */
		    (1 << 17)			| /* nrpt_phase0 enable */
		    (0 << 16)			| /* repeat l0 enable */
		    (0 << 12)			| /* skip line num */
		    (vt_ini_phase << 8)		| /* vt ini phase */
		    (vt_phase_step << 1)	| /* vt phase step (3.4) */
		    (vfmt_en << 0)		/* vt enable */
		    );

	DIM_RDMA_WR(DI_CHAN2_FMT_W, (y_length << 16) | /* hz format width */
					(c_length << 0)	/* vt format width */
				);
}

static void set_di_mem_mif(struct DI_MIF_s *mif, int urgent, int hold_line)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int reset_on_gofield;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}

	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;

	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = 1;/* default enable according to vlsi */
	DIM_RDMA_WR(DI_MEM_GEN_REG,
		    (reset_on_gofield << 29)	| /* reset on go field */
		    (urgent << 28)		| /* urgent bit. */
		    (urgent << 27)		| /* urgent bit. */
		    (1 << 25)			| /* no dummy data. */
		    (hold_line << 19)		| /* hold lines */
		    (1 << 18)			| /* push dummy pixel */
		    (demux_mode << 16)		| /* demux_mode */
		    (bytes_per_pixel << 14)	|
		    (1 << 12)			| /*burst_size_cr*/
		    (1 << 10)			| /*burst_size_cb*/
		    (3 << 8)			| /*burst_size_y*/
		    (chro_rpt_lastl_ctrl << 6)	|
		    ((mif->set_separate_en != 0) << 1)	|
		    (0 << 0)			/* cntl_enable */
		    );
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		DIM_RDMA_WR_BITS(DI_MEM_GEN_REG2, 1, 0, 1);
	} else {
		DIM_RDMA_WR_BITS(DI_MEM_GEN_REG2, 0, 0, 1);
	}
	DIM_RDMA_WR_BITS(DI_MEM_GEN_REG3, mif->bit_mode & 0x3, 8, 2);
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DIM_RDMA_WR(DI_MEM_CANVAS0,
		    (mif->canvas0_addr2 << 16)	|
		    /* cntl_canvas0_addr2 */
		    (mif->canvas0_addr1 << 8)	|
		    (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_RDMA_WR(DI_MEM_LUMA_X0,
		    (mif->luma_x_end0 << 16) |
		    (mif->luma_x_start0 << 0) /* cntl_luma_x_start0 */
		);
	DIM_RDMA_WR(DI_MEM_LUMA_Y0,
		    (mif->luma_y_end0 << 16) |
		    (mif->luma_y_start0 << 0) /* cntl_luma_y_start0 */
		);
	DIM_RDMA_WR(DI_MEM_CHROMA_X0,
		    (mif->chroma_x_end0 << 16) |
		    (mif->chroma_x_start0 << 0)
		);
	DIM_RDMA_WR(DI_MEM_CHROMA_Y0,
		    (mif->chroma_y_end0 << 16) |
		    (mif->chroma_y_start0 << 0)
		);

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_RDMA_WR(DI_MEM_RPT_LOOP, (0 << 28) |
					(0	<< 24) |
					(0	<< 20) |
					(0	  << 16) |
					(chroma0_rpt_loop_start << 12) |
					(chroma0_rpt_loop_end << 8) |
					(luma0_rpt_loop_start << 4) |
					(luma0_rpt_loop_end << 0)
		);

	DIM_RDMA_WR(DI_MEM_LUMA0_RPT_PAT, luma0_rpt_loop_pat);
	DIM_RDMA_WR(DI_MEM_CHROMA0_RPT_PAT, chroma0_rpt_loop_pat);

	/* Dummy pixel value */
	DIM_RDMA_WR(DI_MEM_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) {/* 4:2:0 block mode.*/
		set_di_mem_fmt_more(1,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,  /* hz_ini_phase */
				    1,	/* vfmt_en */
				    1,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
			mif->luma_x_end0 - mif->luma_x_start0 + 1,
						/* y_length */
			mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
						/* c length */
						0);	/* hz repeat. */
	} else {
		set_di_mem_fmt_more(1,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,  /* hz_ini_phase */
				    0,	/* vfmt_en */
				    0,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    ((mif->luma_x_end0 >> 1)
					- (mif->luma_x_start0 >> 1) + 1),
				    0);	/* hz repeat. */
	}
}

static void set_di_if0_fmt_more(int hfmt_en,
				int hz_yc_ratio,		/* 2bit */
				int hz_ini_phase,		/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,		/* 2bit */
				int vt_ini_phase,		/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt		        /* 1bit */
				)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_VSYNC_WR_MPEG_REG(VIU_VD1_FMT_CTRL,
			      (hz_rpt << 28)		| /* hz rpt pixel */
			      (hz_ini_phase << 24)	| /* hz ini phase */
			      (0 << 23)			| /* repeat p0 enable*/
			      (hz_yc_ratio << 21)	| /* hz yc ratio */
			      (hfmt_en << 20)		| /* hz enable */
			      (1 << 17)			| /* nrpt_phase0 en*/
			      (0 << 16)			| /* repeat l0 en*/
			      (0 << 12)			| /* skip line num */
			      (vt_ini_phase << 8)	| /* vt ini phase */
			      (vt_phase_step << 1)	| /*vt phase step(3.4*/
			      (vfmt_en << 0)		/* vt enable */
			      );

	DIM_VSYNC_WR_MPEG_REG(VIU_VD1_FMT_W,
			      (y_length << 16) |	/* hz format width */
			      (c_length << 0)		/* vt format width */
					);
}

static void set_di_if1_fmt_more(int hfmt_en,
				int hz_yc_ratio,/* 2bit */
				int hz_ini_phase,/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,/* 2bit */
				int vt_ini_phase,/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
				)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_VSYNC_WR_MPEG_REG(DI_IF1_FMT_CTRL,
			      (hz_rpt << 28)		/* hz rpt pixel */
			      | (hz_ini_phase << 24)	/* hz ini phase */
			      | (0 << 23)		/* repeat p0 enable */
			      | (hz_yc_ratio << 21)	/* hz yc ratio */
			      | (hfmt_en << 20)		/* hz enable */
			      | (1 << 17)		/* nrpt_phase0 enable*/
			      | (0 << 16)		/* repeat l0 enable */
			      | (0 << 12)		/* skip line num */
			      | (vt_ini_phase << 8)	/* vt ini phase */
			      | (vt_phase_step << 1)	/* vt phase step_3.4*/
			      | (vfmt_en << 0)		/* vt enable */
			      );

	DIM_VSYNC_WR_MPEG_REG(DI_IF1_FMT_W,
			      (y_length << 16) | (c_length << 0));
}

static void set_di_if2_fmt_more(int hfmt_en,
				int hz_yc_ratio,/* 2bit */
				int hz_ini_phase,/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,/* 2bit */
				int vt_ini_phase,/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
				)
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_VSYNC_WR_MPEG_REG(DI_IF2_FMT_CTRL,
			      (hz_rpt << 28)		/* hz rpt pixel */
			      | (hz_ini_phase << 24)	/* hz ini phase */
			      | (0 << 23)		/* repeat p0 enable */
			      | (hz_yc_ratio << 21)	/* hz yc ratio */
			      | (hfmt_en << 20)		/* hz enable */
			      | (1 << 17)		/* nrpt_phase0 enable*/
			      | (0 << 16)		/* repeat l0 enable */
			      | (0 << 12)		/* skip line num */
			      | (vt_ini_phase << 8)	/* vt ini phase */
			      | (vt_phase_step << 1)	/* vt phase step(3.4)*/
			      | (vfmt_en << 0)		/* vt enable */
			      );

	DIM_VSYNC_WR_MPEG_REG(DI_IF2_FMT_W,
			      (y_length << 16) | (c_length << 0));
}

static const u32 vpat[] = {0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};

static void set_di_if2_mif(struct DI_MIF_s *mif, int urgent,
			   int hold_line, int vskip_cnt)
{
	unsigned int bytes_per_pixel, demux_mode;
	unsigned int pat, loop = 0, chro_rpt_lastl_ctrl = 0;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt << 1) + 1];
		/*top*/
		if (mif->src_field_mode == 0) {
			chro_rpt_lastl_ctrl = 1;
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
		pat = vpat[vskip_cnt];
	}
	#ifdef MARK_HIS
	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	#else
	if (mif->set_separate_en) {
		bytes_per_pixel = 0;
	} else {
		if (mif->video_mode)
			bytes_per_pixel = 2;
		else
			bytes_per_pixel = 1;
	}
	#endif
	demux_mode = mif->video_mode;

	/* ---------------------- */
	/* General register */
	/* ---------------------- */

	DIM_VSYNC_WR_MPEG_REG(DI_IF2_GEN_REG,
			      (1 << 29)		| /* reset on go field */
			      (urgent << 28)	| /* urgent */
			      (urgent << 27)	| /* luma urgent */
			      (1 << 25)		| /* no dummy data. */
			      (hold_line << 19)	| /* hold lines */
			      (1 << 18)		| /* push dummy pixel */
			      (demux_mode << 16)	| /* demux_mode */
			      (bytes_per_pixel << 14)	|
			      (1 << 12)		| /*burst_size_cr*/
			      (1 << 10)		| /*burst_size_cb*/
			      (3 << 8)		| /*burst_size_y*/
			      (chro_rpt_lastl_ctrl << 6)		|
			      ((mif->set_separate_en != 0) << 1)	|
			      (1 << 0)/* cntl_enable */
			      );
	/* post bit mode config, if0 config in video.c
	 * DIM_VSC_WR_MPG_BT(DI_IF2_GEN_REG3, mif->bit_mode, 8, 2);
	 */
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_CANVAS0, (mif->canvas0_addr2 << 16) |
		(mif->canvas0_addr1 << 8) | (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_LUMA_X0, (mif->luma_x_end0 << 16) |
		(mif->luma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_LUMA_Y0, (mif->luma_y_end0 << 16) |
		(mif->luma_y_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_CHROMA_X0, (mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_RPT_LOOP,
			      (loop << 24)	|
			      (loop << 16)	|
			      (loop << 8)	|
			      (loop << 0)
			      );

	DIM_VSYNC_WR_MPEG_REG(DI_IF2_LUMA0_RPT_PAT, pat);
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_CHROMA0_RPT_PAT, pat);

	/* Dummy pixel value */
	DIM_VSYNC_WR_MPEG_REG(DI_IF2_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) { /* 4:2:0 block mode. */
		set_di_if2_fmt_more(1,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,	/* hz_ini_phase */
				    1,	/* vfmt_en */
				    1,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    mif->chroma_x_end0 -
				    mif->chroma_x_start0 + 1,
				    0);	/* hz repeat. */
	} else {
		set_di_if2_fmt_more(1,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,	/* hz_ini_phase */
				    0,	/* vfmt_en */
				    0,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    ((mif->luma_x_end0 >> 1) -
				    (mif->luma_x_start0 >> 1) + 1),
				    0); /* hz repeat */
	}
}

static void set_di_if1_mif(struct DI_MIF_s *mif, int urgent,
			   int hold_line, int vskip_cnt)
{
	unsigned int bytes_per_pixel, demux_mode;
	unsigned int pat, loop = 0, chro_rpt_lastl_ctrl = 0;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt << 1) + 1];
		/*top*/
		if (mif->src_field_mode == 0) {
			chro_rpt_lastl_ctrl = 1;
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
		pat = vpat[vskip_cnt];
	}
	#ifdef MARK_HIS
	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	#else
	if (mif->set_separate_en) {
		bytes_per_pixel = 0;
	} else {
		if (mif->video_mode)
			bytes_per_pixel = 2;
		else
			bytes_per_pixel = 1;
	}
	#endif
	demux_mode = mif->video_mode;

	/* ---------------------- */
	/* General register */
	/* ---------------------- */

	DIM_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG,
			      (1 << 29)		| /* reset on go field */
			      (urgent << 28)	| /* urgent */
			      (urgent << 27)	| /* luma urgent */
			      (1 << 25)		| /* no dummy data. */
			      (hold_line << 19)	| /* hold lines */
			      (1 << 18)		| /* push dummy pixel */
			      (demux_mode << 16)	| /* demux_mode */
			      (bytes_per_pixel << 14)	|
			      (1 << 12)		| /*burst_size_cr*/
			      (1 << 10)		| /*burst_size_cb*/
			      (3 << 8)		| /*burst_size_y*/
			      (chro_rpt_lastl_ctrl << 6)		|
			      ((mif->set_separate_en != 0) << 1)	|
			      (1 << 0)		/* cntl_enable */
			      );
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_CANVAS0,
			      (mif->canvas0_addr2 << 16)	|
			      (mif->canvas0_addr1 << 8)		|
			      (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_LUMA_X0,
			      (mif->luma_x_end0 << 16) |
			      (mif->luma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_LUMA_Y0,
			      (mif->luma_y_end0 << 16)		|
			      (mif->luma_y_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_CHROMA_X0,
			      (mif->chroma_x_end0 << 16)	|
			      (mif->chroma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_CHROMA_Y0,
			      (mif->chroma_y_end0 << 16)	|
			      (mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_RPT_LOOP,
			      (loop << 24)	|
			      (loop << 16)	|
			      (loop << 8)	|
			      (loop << 0)
			      );

	DIM_VSYNC_WR_MPEG_REG(DI_IF1_LUMA0_RPT_PAT, pat);
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_CHROMA0_RPT_PAT, pat);

	/* Dummy pixel value */
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) { /* 4:2:0 block mode. */
		set_di_if1_fmt_more(1,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,	/* hz_ini_phase */
				    1,	/* vfmt_en */
				    1,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    mif->chroma_x_end0 -
				    mif->chroma_x_start0 + 1,
				    0); /* hz repeat. */
	} else {
		set_di_if1_fmt_more(1,	/* hfmt_en */
				    1,	/* hz_yc_ratio */
				    0,	/* hz_ini_phase */
				    0,	/* vfmt_en */
				    0,	/* vt_yc_ratio */
				    0,	/* vt_ini_phase */
				    mif->luma_x_end0 - mif->luma_x_start0 + 1,
				    ((mif->luma_x_end0 >> 1) -
				    (mif->luma_x_start0 >> 1) + 1),
				    0); /* hz repeat */
	}
}

static void set_di_chan2_mif(struct DI_MIF_s *mif, int urgent, int hold_line)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int reset_on_gofield;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}
	#ifdef MARK_HIS
	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	#else
	if (mif->set_separate_en) {
		bytes_per_pixel = 0;
	} else {
		if (mif->video_mode)
			bytes_per_pixel = 2;
		else
			bytes_per_pixel = 1;
	}
	#endif
	demux_mode = mif->video_mode;

	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = 1;/* default enable according to vlsi */
	DIM_RDMA_WR(DI_CHAN2_GEN_REG,
		    (reset_on_gofield << 29)	|
		    (urgent << 28)		|	/* urgent */
		    (urgent << 27)		|	/* luma urgent */
		    (1 << 25)			|	/* no dummy data. */
		    (hold_line << 19)		|	/* hold lines */
		    (1 << 18)			|	/* push dummy pixel */
		    (demux_mode << 16)		|
		    (bytes_per_pixel << 14)	|
		    (1 << 12)			|	/*burst_size_cr*/
		    (1 << 10)			|	/*burst_size_cb*/
		    (3 << 8)			|	/*burst_size_y*/
		    (chro_rpt_lastl_ctrl << 6)	|
		    ((mif->set_separate_en != 0) << 1)	|
		    (0 << 0)			/* cntl_enable */
	  );
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		DIM_RDMA_WR_BITS(DI_CHAN2_GEN_REG2, 1, 0, 1);
	} else {
		DIM_RDMA_WR_BITS(DI_CHAN2_GEN_REG2, 0, 0, 1);
	}
	DIM_RDMA_WR_BITS(DI_CHAN2_GEN_REG3, mif->bit_mode & 0x3, 8, 2);
	DIM_RDMA_WR(DI_CHAN2_CANVAS0, (mif->canvas0_addr2 << 16) |
				(mif->canvas0_addr1 << 8) |
				(mif->canvas0_addr0 << 0));
	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_RDMA_WR(DI_CHAN2_LUMA_X0, (mif->luma_x_end0 << 16) |
		/* cntl_luma_x_end0 */
		(mif->luma_x_start0 << 0));
	DIM_RDMA_WR(DI_CHAN2_LUMA_Y0, (mif->luma_y_end0 << 16) |
				(mif->luma_y_start0 << 0));
	DIM_RDMA_WR(DI_CHAN2_CHROMA_X0, (mif->chroma_x_end0 << 16) |
				(mif->chroma_x_start0 << 0));
	DIM_RDMA_WR(DI_CHAN2_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
				(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_RDMA_WR(DI_CHAN2_RPT_LOOP,
		    (0 << 28)			|
		    (0	 << 24)			|
		    (0	 << 20)			|
		    (0	 << 16)			|
		    (0	 << 12)			|
		    (0	 << 8)			|
		    (luma0_rpt_loop_start << 4)	|
		    (luma0_rpt_loop_end << 0)
	);

	DIM_RDMA_WR(DI_CHAN2_LUMA0_RPT_PAT, luma0_rpt_loop_pat);

	/* Dummy pixel value */
	DIM_RDMA_WR(DI_CHAN2_DUMMY_PIXEL, 0x00808000);

	if (mif->set_separate_en != 0) {	/* 4:2:0 block mode. */
		set_di_chan2_fmt_more(1,	/* hfmt_en */
				      1,	/* hz_yc_ratio */
				      0,	/* hz_ini_phase */
				      1,	/* vfmt_en */
				      1,	/* vt_yc_ratio */
				      0,	/* vt_ini_phase */
				      mif->luma_x_end0 -
				      mif->luma_x_start0 + 1,	/* y_length */
				      mif->chroma_x_end0 -
				      mif->chroma_x_start0 + 1,/* c length */
				      0);	/* hz repeat. */
	} else {
		set_di_chan2_fmt_more(1,	/* hfmt_en */
				      1,	/* hz_yc_ratio */
				      0,	/* hz_ini_phase */
				      0,	/* vfmt_en */
				      0,	/* vt_yc_ratio */
				      0,	/* vt_ini_phase */
				      mif->luma_x_end0 -
				      mif->luma_x_start0 + 1,	/* y_length */
				      ((mif->luma_x_end0 >> 1) -
				      (mif->luma_x_start0 >> 1) + 1),
				      0);	/* hz repeat. */
	}
}

static void set_di_if0_mif(struct DI_MIF_s *mif, int urgent, int hold_line,
			   int vskip_cnt, int post_write_en)
{
	unsigned int pat, loop = 0;
	unsigned int bytes_per_pixel, demux_mode;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt << 1) + 1];
		if (mif->src_field_mode == 0) {/* top */
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
	pat = vpat[vskip_cnt];

	if (post_write_en) {
		bytes_per_pixel =
			mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
		demux_mode = mif->video_mode;
		DIM_VSYNC_WR_MPEG_REG(VD1_IF0_GEN_REG,
				      (1 << 29)	| /*reset on go field*/
				      (urgent << 28)	| /* urgent */
				      (urgent << 27)	| /* luma urgent */
				      (1 << 25)	 | /* no dummy data. */
				      (hold_line << 19)	| /* hold lines */
				      (1 << 18)	 | /* push dummy pixel */
				      (demux_mode << 16) | /* demux_mode */
				      (bytes_per_pixel << 14)	|
				      (1 << 12)		|
				      (1 << 10)		|
				      (3 << 8)		|
				      (0 << 6)		|
				      ((mif->set_separate_en != 0) << 1) |
				      (1 << 0)); /* cntl_enable */
	}
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_CANVAS0,
			      (mif->canvas0_addr2 << 16)	|
			      (mif->canvas0_addr1 << 8)		|
			      (mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_X0, (mif->luma_x_end0 << 16) |
		(mif->luma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_LUMA_Y0, (mif->luma_y_end0 << 16) |
		(mif->luma_y_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_X0, (mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0));
	}

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_RPT_LOOP,
			      (loop << 24)	|
			      (loop << 16)	|
			      (loop << 8)	|
			      (loop << 0));
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_LUMA0_RPT_PAT, pat);
	DIM_VSYNC_WR_MPEG_REG(VD1_IF0_CHROMA0_RPT_PAT, pat);

	if (post_write_en) {
		/* 4:2:0 block mode. */
		if (mif->set_separate_en != 0) {
			set_di_if0_fmt_more(1,	/* hfmt_en */
					    1,	/* hz_yc_ratio */
					    0,	/* hz_ini_phase */
					    1,	/* vfmt_en */
					    1,	/* vt_yc_ratio */
					    0,	/* vt_ini_phase */
					    /* y_length */
					    mif->luma_x_end0 -
					    mif->luma_x_start0 + 1,
					    /* c length */
					    mif->chroma_x_end0 -
					    mif->chroma_x_start0 + 1,
					    0);	/* hz repeat. */
		} else {
			set_di_if0_fmt_more(1,	/* hfmt_en */
					    1,	/* hz_yc_ratio */
					    0,	/* hz_ini_phase */
					    0,	/* vfmt_en */
					    0,	/* vt_yc_ratio */
					    0,	/* vt_ini_phase */
						/* y_length */
					    mif->luma_x_end0 -
					    mif->luma_x_start0 + 1,
						/* c length */
					    ((mif->luma_x_end0 >> 1) -
					    (mif->luma_x_start0 >> 1) + 1),
					    0);	/* hz repeat */
		}
	}
}

static void set_di_if0_fmt_more_g12(int hfmt_en,
				    int hz_yc_ratio,		/* 2bit */
				    int hz_ini_phase,		/* 4bit */
				    int vfmt_en,
				    int vt_yc_ratio,		/* 2bit */
				    int vt_ini_phase,		/* 4bit */
				    int y_length,
				    int c_length,
				    int hz_rpt		        /* 1bit */
				    )
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	DIM_VSYNC_WR_MPEG_REG(DI_IF0_FMT_CTRL,
			      (hz_rpt << 28) | /* hz rpt pixel */
			      (hz_ini_phase << 24) | /* hz ini phase */
			      (0 << 23)	| /* repeat p0 enable */
			      (hz_yc_ratio << 21) | /* hz yc ratio */
			      (hfmt_en << 20) |	/* hz enable */
			      (1 << 17) | /* nrpt_phase0 enable */
			      (0 << 16)	| /* repeat l0 enable */
			      (0 << 12)	| /* skip line num */
			      (vt_ini_phase << 8) | /* vt ini phase */
			      (vt_phase_step << 1) | /* vt phase step (3.4) */
			      (vfmt_en << 0)); /* vt enable */

	DIM_VSYNC_WR_MPEG_REG(DI_IF0_FMT_W,
			      (y_length << 16)	|	/* hz format width */
			      (c_length << 0)		/* vt format width */
					);
}

static void set_di_if0_mif_g12(struct DI_MIF_s *mif, int urgent, int hold_line,
			       int vskip_cnt, int post_write_en)
{
	unsigned int pat, loop = 0;
	unsigned int bytes_per_pixel, demux_mode;

	if (mif->set_separate_en == 1) {
		pat = vpat[(vskip_cnt << 1) + 1];
		if (mif->src_field_mode == 0) {/* top */
			loop = 0x11;
			pat <<= 4;
		}
	} else {
		loop = 0;
		pat = vpat[vskip_cnt];

		bytes_per_pixel =
			mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
		demux_mode = mif->video_mode;
		DIM_VSYNC_WR_MPEG_REG(DI_IF0_GEN_REG,
				      (1 << 29) | /* reset on go field */
				      (urgent << 28) | /* urgent */
				      (urgent << 27) | /* luma urgent */
				      (1 << 25) | /* no dummy data. */
				      (hold_line << 19)	| /* hold lines */
				      (1 << 18) | /* push dummy pixel */
				      (demux_mode << 16) | /* demux_mode */
				      (bytes_per_pixel << 14) |
				      (1 << 12) |
				      (1 << 10) |
				      (3 << 8) |
				      (0 << 6) |
				      ((mif->set_separate_en != 0) << 1) |
				      (1 << 0)); /* cntl_enable */
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_CANVAS0,
			      (mif->canvas0_addr2 << 16)	|
			      (mif->canvas0_addr1 << 8)		|
			      (mif->canvas0_addr0 << 0));
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		DIM_RDMA_WR_BITS(DI_IF0_GEN_REG2, 1, 0, 1);
	} else {
		DIM_RDMA_WR_BITS(DI_IF0_GEN_REG2, 0, 0, 1);
	}

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_LUMA_X0,
			      (mif->luma_x_end0 << 16)	|
			      (mif->luma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_LUMA_Y0,
			      (mif->luma_y_end0 << 16)	|
			      (mif->luma_y_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_CHROMA_X0,
			      (mif->chroma_x_end0 << 16)	|
			      (mif->chroma_x_start0 << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_CHROMA_Y0,
			      (mif->chroma_y_end0 << 16)	|
			      (mif->chroma_y_start0 << 0));
		}
	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_REPEAT_LOOP,
			      (loop << 24)	|
			      (loop << 16)	|
			      (loop << 8)	|
			      (loop << 0));
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_LUMA0_RPT_PAT,   pat);
	DIM_VSYNC_WR_MPEG_REG(DI_IF0_CHROMA0_RPT_PAT, pat);

	/* 4:2:0 block mode. */
	if (mif->set_separate_en != 0) {
		set_di_if0_fmt_more_g12(1, /* hfmt_en */
					1,	/* hz_yc_ratio */
					0,	/* hz_ini_phase */
					1,	/* vfmt_en */
					1, /* vt_yc_ratio */
					0, /* vt_ini_phase */
					/* y_length */
					mif->luma_x_end0 -
					mif->luma_x_start0 + 1,
					/* c length */
					mif->chroma_x_end0
					- mif->chroma_x_start0 + 1,
					0); /* hz repeat. */
	} else {
		set_di_if0_fmt_more_g12(1,	/* hfmt_en */
					1,	/* hz_yc_ratio */
					0,	/* hz_ini_phase */
					0,	/* vfmt_en */
					0,	/* vt_yc_ratio */
					0,	/* vt_ini_phase */
					/* y_length */
					mif->luma_x_end0 -
					mif->luma_x_start0 + 1,
					/* c length */
					((mif->luma_x_end0 >> 1) -
					(mif->luma_x_start0 >> 1) + 1),
					0); /* hz repeat */
	}
}

static unsigned int di_mc_update;
void dimh_patch_post_update_mc(void)
{
	if (di_mc_update == DI_MC_SW_ON_MASK)
		DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_CTRL, 1, 9, 1);
}

void dimh_patch_post_update_mc_sw(unsigned int cmd, bool on)
{
	unsigned int l_flg = di_mc_update;

	switch (cmd) {
	case DI_MC_SW_IC:
		if (is_meson_gxtvbb_cpu()	||
		    is_meson_txl_cpu()		||
		    is_meson_txlx_cpu()		||
		    is_meson_txhd_cpu()) {
			di_mc_update |= DI_MC_SW_IC;
		}
		break;
	case DI_MC_SW_REG:
		if (on) {
			di_mc_update |= cmd;
			di_mc_update &= ~DI_MC_SW_OTHER;
		} else {
			di_mc_update &= ~(cmd | DI_MC_SW_OTHER);
		}
		break;
	case DI_MC_SW_OTHER:

/*	case DI_MC_SW_POST:*/
		if (on)
			di_mc_update |= cmd;
		else
			di_mc_update &= ~cmd;

		break;
	}

	if (l_flg !=  di_mc_update)
		pr_debug("%s:0x%x->0x%x\n", __func__, l_flg, di_mc_update);
}

void dimh_post_ctrl(enum DI_HW_POST_CTRL contr, unsigned int post_write_en)
{
	unsigned int reg_val;

	if (!post_write_en || !cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;

	switch (contr) {
	case DI_HW_POST_CTRL_INIT:
		DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL,
				      0x80000000 |
				      dimp_get(edi_mp_line_num_post_frst));
		break;
	case DI_HW_POST_CTRL_RESET:	/*replace post_frame_reset_g12a*/
		reg_val = (0xc3200000 | dimp_get(edi_mp_line_num_post_frst));
		DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, reg_val);
		reg_val = (0x83200000 | dimp_get(edi_mp_line_num_post_frst));
		DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, reg_val);

		break;
	}
}

void dimh_initial_di_post_2(int hsize_post, int vsize_post,
			    int hold_line, bool post_write_en)
{
	di_post_set_flow(post_write_en, EDI_POST_FLOW_STEP1_STOP);/*dbg a*/
	DIM_VSYNC_WR_MPEG_REG(DI_POST_SIZE,
			      (hsize_post - 1) | ((vsize_post - 1) << 16));

	/* if post size < MIN_POST_WIDTH, force old ei */
	if (hsize_post < MIN_POST_WIDTH)
		DIM_VSC_WR_MPG_BT(DI_EI_CTRL3, 0, 31, 1);
	else
		DIM_VSC_WR_MPG_BT(DI_EI_CTRL3, 1, 31, 1);

	/* DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_Y, (vsize_post >> 2) - 1); */
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_Y, (vsize_post - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG1_Y,
			      ((vsize_post >> 2) << 16) |
			      (2 * (vsize_post >> 2) - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG2_Y,
			      ((2 * (vsize_post >> 2)) << 16) |
			      (3 * (vsize_post >> 2) - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG3_Y,
			      ((3 * (vsize_post >> 2)) << 16) |
			      (vsize_post - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_X, (hsize_post - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG1_X, (hsize_post - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG2_X, (hsize_post - 1));
	DIM_VSYNC_WR_MPEG_REG(DI_BLEND_REG3_X, (hsize_post - 1));
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (post_write_en) {
	#ifdef MARK_HIS
			dim_print("%s:VD1_AFBCD0_MISC_CTRL\n", __func__);
	#endif
			/*di if0 mif to di post*/
			DIM_VSC_WR_MPG_BT(VIUB_MISC_CTRL0, 0, 4, 1);
			/*di_mif0_en:select mif to di*/
			#ifdef MARK_HIS
			DI_VSC_WR_MPG_BT(VD1_AFBCD0_MISC_CTRL,
					 1, 8, 1);
			#endif
		#ifdef DIM_HIS
			DIM_VSC_WR_MPG_BT(VD1_AFBCD0_MISC_CTRL, 0, 8, 1);
		#endif
		} else {
			DIM_VSC_WR_MPG_BT(VD1_AFBCD0_MISC_CTRL, 1, 8, 1);
			DIM_VSC_WR_MPG_BT(VIUB_MISC_CTRL0, 0, 4, 1);
		}

	} else {
		/* enable ma,disable if0 to vpp */
		if ((VSYNC_RD_MPEG_REG(VIU_MISC_CTRL0) & 0x50000) != 0x50000) {
			DIM_VSC_WR_MPG_BT(VIU_MISC_CTRL0, 5, 16, 3);
			if (post_write_en)
				DIM_VSC_WR_MPG_BT(VIU_MISC_CTRL0, 1, 28, 1);
		}
	}
	DIM_VSYNC_WR_MPEG_REG(DI_POST_CTRL,
			      (0 << 0)				|
			      (0 << 1)				|
			      (0 << 2)				|
			      (0 << 3)				|
			      (0 << 4)				|
			      (0 << 5)				|
			      (0 << 6)				|
			      ((post_write_en ? 1 : 0) << 7)	|
			      ((post_write_en ? 0 : 1) << 8)	|
			      (0 << 9)				|
			      (0 << 10)				|
			      (0 << 11)				|
			      (0 << 12)				|
			      (hold_line << 16)			|
			      (0 << 29)				|
			      (0x3 << 30)
		);
}

static void post_bit_mode_config(unsigned char if0,
				 unsigned char if1,
				 unsigned char if2,
				 unsigned char post_wr)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		return;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		DIM_DI_WR_REG_BITS(DI_IF0_GEN_REG3, if0 & 0x3, 8, 2);
	else
		DIM_DI_WR_REG_BITS(VD1_IF0_GEN_REG3, if0 & 0x3, 8, 2);
	DIM_DI_WR_REG_BITS(DI_IF1_GEN_REG3, if1 & 0x3, 8, 2);
	DIM_DI_WR_REG_BITS(DI_IF2_GEN_REG3, if2 & 0x3, 8, 2);
	DIM_DI_WR_REG_BITS(DI_DIWR_Y, post_wr & 0x1, 14, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL) && ((post_wr & 0x3) == 0x3))
		DIM_DI_WR_REG_BITS(DI_DIWR_CTRL, 0x3, 22, 2);
}

void dimh_post_switch_buffer(struct DI_MIF_s *di_buf0_mif,
			     struct DI_MIF_s *di_buf1_mif,
			     struct DI_MIF_s *di_buf2_mif,
			     struct DI_SIM_MIF_s *di_diwr_mif,
			     struct DI_SIM_MIF_s *di_mtnprd_mif,
			     struct DI_MC_MIF_s *di_mcvecrd_mif,
			     int ei_en, int blend_en,
			     int blend_mtn_en, int blend_mode,
			     int di_vpp_en, int di_ddr_en,
			     int post_field_num, int hold_line,
			     int urgent,
			     int invert_mv, bool pd_enable,
			     bool mc_enable, int vskip_cnt)
{
	int ei_only, buf1_en;

	ei_only = ei_en && !blend_en && (di_vpp_en || di_ddr_en);
	buf1_en =  (!ei_only && (di_ddr_en || di_vpp_en));

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		DIM_VSYNC_WR_MPEG_REG(DI_IF0_CANVAS0,
				      (di_buf0_mif->canvas0_addr2 << 16) |
				      (di_buf0_mif->canvas0_addr1 << 8)	|
				      (di_buf0_mif->canvas0_addr0 << 0));
			if (!di_ddr_en)
				DIM_VSC_WR_MPG_BT(VD1_IF0_GEN_REG, 0, 0, 1);
		if (mc_enable) {
			DIM_VSC_WR_MPG_BT(MCVECRD_CTRL1,
					  di_mcvecrd_mif->canvas_num, 16, 8);
		}
		/*motion for current display field.*/
		if (blend_mtn_en) {
			DIM_VSC_WR_MPG_BT(MTNRD_CTRL1,
					  di_mtnprd_mif->canvas_num, 16, 8);
			/* current field mtn canvas index.*/
		}
	} else {
		if ((VSYNC_RD_MPEG_REG(VIU_MISC_CTRL0) & 0x50000) != 0x50000)
			DIM_VSC_WR_MPG_BT(VIU_MISC_CTRL0, 5, 16, 3);
		if (di_ddr_en)
			DIM_VSC_WR_MPG_BT(VIU_MISC_CTRL0, 1, 28, 1);
		if (ei_en || di_vpp_en || di_ddr_en)
			set_di_if0_mif(di_buf0_mif, urgent,
				       hold_line, vskip_cnt, di_ddr_en);
		if (mc_enable) {
			DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_CTRL,
					  /* canvas enable */
					  (1 << 9)		|
					  (urgent << 8)	|
					  di_mcvecrd_mif->canvas_num,
					  0, 10);
		}
		/*motion for current display field.*/
		if (blend_mtn_en) {
			DIM_VSYNC_WR_MPEG_REG(DI_MTNRD_CTRL,
					      (di_mtnprd_mif->canvas_num << 8) |
					      (urgent << 16));
			/*current field mtn canvas index.*/
		}
	}

	if (!ei_only && (di_ddr_en || di_vpp_en)) {
		DIM_VSYNC_WR_MPEG_REG(DI_IF1_CANVAS0,
				      (di_buf1_mif->canvas0_addr2 << 16) |
				      (di_buf1_mif->canvas0_addr1 << 8)  |
				      (di_buf1_mif->canvas0_addr0 << 0));
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			DIM_VSYNC_WR_MPEG_REG(DI_IF2_CANVAS0,
					      (di_buf2_mif->canvas0_addr2 <<
					      16)	|
					      (di_buf2_mif->canvas0_addr1 <<
					      8)	|
					      (di_buf2_mif->canvas0_addr0 <<
					      0));
	}

	if (di_ddr_en) {
		DIM_VSYNC_WR_MPEG_REG(DI_DIWR_CTRL,
				      di_diwr_mif->canvas_num	|
				      (urgent << 16)		|
				      (2 << 26)			|
				      (di_ddr_en << 30));
		post_bit_mode_config(di_buf0_mif->bit_mode,
				     di_buf1_mif->bit_mode,
				     di_buf2_mif->bit_mode,
				     di_diwr_mif->bit_mode);
	}

	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, blend_en, 31, 1);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, blend_mode, 20, 2);
	if ((dimp_get(edi_mp_pldn_ctrl_rflsh) == 1) && pd_enable)
		DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, 7, 22, 3);

	if (mc_enable) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, invert_mv, 17, 1);
			/* invert mv */
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
				  di_mcvecrd_mif->vecrd_offset, 12, 3);
		if (di_mcvecrd_mif->blend_en) {
			if (blend_mode == 1) {
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
						  dimp_get(edi_mp_mcen_mode),
						  0, 2);
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 11, 1);
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 2, 18, 2);
			} else {
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
						  dimp_get(edi_mp_mcen_mode),
						  0, 2);
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 1, 11, 1);
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 3, 18, 2);
			}
		} else {
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 11, 1);
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 2, 18, 2);
		}
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		DIM_VSC_WR_MPG_BT(DI_POST_GL_THD, hold_line, 16, 5);
		hold_line = 0;
	}

	if (!is_meson_txlx_cpu())
		invert_mv = 0;
	if (dimp_get(edi_mp_post_ctrl) != 0) {
		DIM_VSYNC_WR_MPEG_REG(DI_POST_CTRL,
				      dimp_get(edi_mp_post_ctrl) |
				      (0x3 << 30));
	} else {
		DIM_VSYNC_WR_MPEG_REG(DI_POST_CTRL,
				      ((ei_en | blend_en) << 0) |
				      /* line buf 0 en */
				      ((blend_mode == 1 ? 1 : 0) << 1)	|
				      (ei_en << 2)		|
				      /* ei  enable */
				      (blend_mtn_en << 3)	|
				      /* mtn line buffer enable */
				      (blend_mtn_en << 4)	|
				      /* mtnp read mif enable */
				      (blend_en << 5)		|
				      (1 << 6)		|
				      /* di mux output enable */
				      (di_ddr_en << 7)	|
				      /* di wr to SDRAM enable.*/
				      (di_vpp_en << 8)	|
				      /* di to VPP enable. */
				      (0 << 9)		|
				      /* mif0 to VPP enable. */
				      (0 << 10)		| /* post drop first. */
				      (0 << 11)		|
				      (di_vpp_en << 12)	| /* post viu link */
				      (invert_mv << 14)	|
				      (hold_line << 16)	|
				      /* post hold line number */
				      (post_field_num << 29)	|
				      /* post field num */
				      (0x3 << 30));
				      /* post soft rst  post frame rst. */
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A) && di_ddr_en) {
#ifdef MARK_HIS
		post_frame_reset_g12a();
#endif
	} else if (di_ddr_en && mc_enable) {
		DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_CTRL, 1, 9, 1);
	}
}

static void set_post_mtnrd_mif(struct DI_SIM_MIF_s *mtnprd_mif,
			       unsigned char urgent)
{
	DIM_VSYNC_WR_MPEG_REG(DI_MTNPRD_X,
			      (mtnprd_mif->start_x << 16) |
			      (mtnprd_mif->end_x));
	DIM_VSYNC_WR_MPEG_REG(DI_MTNPRD_Y,
			      (mtnprd_mif->start_y << 16) |
			      (mtnprd_mif->end_y));
	DIM_VSYNC_WR_MPEG_REG(DI_MTNRD_CTRL,
			      (mtnprd_mif->canvas_num << 8) |
			      (urgent << 16)
	 );
}

static void set_post_mtnrd_mif_g12(struct DI_SIM_MIF_s *mtnprd_mif)
{
	DIM_VSYNC_WR_MPEG_REG(MTNRD_SCOPE_X,
			      (mtnprd_mif->end_x << 16) |
			      (mtnprd_mif->start_x));
	DIM_VSYNC_WR_MPEG_REG(MTNRD_SCOPE_Y,
			      (mtnprd_mif->end_y << 16) |
			      (mtnprd_mif->start_y));
	DIM_VSC_WR_MPG_BT(MTNRD_CTRL1, mtnprd_mif->canvas_num, 16, 8);
	DIM_VSC_WR_MPG_BT(MTNRD_CTRL1, 0, 0, 3);
}

void dimh_enable_di_post_2(struct DI_MIF_s		   *di_buf0_mif,
			   struct DI_MIF_s		   *di_buf1_mif,
			   struct DI_MIF_s		   *di_buf2_mif,
			   struct DI_SIM_MIF_s    *di_diwr_mif,
			   struct DI_SIM_MIF_s    *di_mtnprd_mif,
			   int ei_en, int blend_en, int blend_mtn_en,
			   int blend_mode,
			   int di_vpp_en, int di_ddr_en, int post_field_num,
			   int hold_line, int urgent, int invert_mv,
			   int vskip_cnt)
{
	int ei_only;
	int buf1_en;

	ei_only = ei_en && !blend_en && (di_vpp_en || di_ddr_en);
	buf1_en =  (!ei_only && (di_ddr_en || di_vpp_en));

	if (ei_en || di_vpp_en || di_ddr_en) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
			set_di_if0_mif_g12(di_buf0_mif, di_vpp_en,
					   hold_line, vskip_cnt, di_ddr_en);
			/* if di post vpp link disable vd1 for new if0 */
			if (!di_ddr_en)
				DIM_VSC_WR_MPG_BT(VD1_IF0_GEN_REG, 0, 0, 1);
		} else {
			set_di_if0_mif(di_buf0_mif, di_vpp_en,
				       hold_line, vskip_cnt, di_ddr_en);
		}
	}

	set_di_if1_mif(di_buf1_mif, di_vpp_en, hold_line, vskip_cnt);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		set_di_if2_mif(di_buf2_mif,
			       di_vpp_en, hold_line, vskip_cnt);
	/* motion for current display field. */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		set_post_mtnrd_mif_g12(di_mtnprd_mif);
	else
		set_post_mtnrd_mif(di_mtnprd_mif, urgent);
	if (di_ddr_en) {
		DIM_VSYNC_WR_MPEG_REG(DI_DIWR_X,
				      (di_diwr_mif->start_x << 16)	|
				      (di_diwr_mif->end_x));
		DIM_VSYNC_WR_MPEG_REG(DI_DIWR_Y,
				      (3 << 30)				|
				      (di_diwr_mif->start_y << 16)	|
					/* wr ext en from gxtvbb */
				      (1 << 15)				|
				      (di_diwr_mif->end_y));
		DIM_VSYNC_WR_MPEG_REG(DI_DIWR_CTRL,
				      di_diwr_mif->canvas_num	|
				      (urgent << 16)		|
				      (2 << 26)			|
				      (di_ddr_en << 30));
		post_bit_mode_config(di_buf0_mif->bit_mode,
				     di_buf1_mif->bit_mode,
				     di_buf2_mif->bit_mode,
				     di_diwr_mif->bit_mode);
	}
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, 7, 22, 3);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, blend_en & 0x1, 31, 1);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, blend_mode & 0x3, 20, 2);
	if (!is_meson_txlx_cpu())
		invert_mv = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		DIM_VSC_WR_MPG_BT(DI_POST_GL_THD, hold_line, 16, 5);
		hold_line = 0;
	}
	DIM_VSYNC_WR_MPEG_REG(DI_POST_CTRL,
			      ((ei_en | blend_en) << 0)	|
			      /* line buffer 0 enable */
			      ((blend_mode == 1 ? 1 : 0) << 1)	|
			      (ei_en << 2)			|
			      /* ei  enable */
			      (blend_mtn_en << 3)		|
			      /*mtn line buf en */
			      (blend_mtn_en  << 4)		|
			      /*mtnp read mif en */
			      (blend_en << 5)			|
			      (1 << 6)			|
			      /* di mux output enable */
			      /* di write to SDRAM enable. */
			      (di_ddr_en << 7)		|
			      (di_vpp_en << 8)		|
			      /* di to VPP enable. */
			      (0 << 9)			|
			      /* mif0 to VPP enable. */
			      (0 << 10)			|
			      /* post drop first. */
			      (0 << 11)			|
			      (di_vpp_en << 12)		|
			      /* post viu link */
			      (invert_mv << 14)		|
			      /* invert mv */
			      (hold_line << 16)		|
			      /* post hold line number */
			      (post_field_num << 29)		|
			      /* post field num */
			      (0x3 << 30));
			      /* post soft rst  post frame rst */
}

void dimh_pst_trig_resize(void)
{
	DIM_VSYNC_WR_MPEG_REG(DI_POST_SIZE, (32 - 1) | ((128 - 1) << 16));
}

void dimh_disable_post_deinterlace_2(void)
{
	DIM_VSYNC_WR_MPEG_REG(DI_POST_CTRL, 0x3 << 30);
	DIM_VSYNC_WR_MPEG_REG(DI_POST_SIZE, (32 - 1) | ((128 - 1) << 16));
	DIM_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, 0x3 << 30);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		DIM_VSYNC_WR_MPEG_REG(DI_IF2_GEN_REG, 0x3 << 30);
	/* disable ma,enable if0 to vpp,enable afbc to vpp */
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if ((VSYNC_RD_MPEG_REG(VIU_MISC_CTRL0) & 0x50000) != 0)
			DIM_VSC_WR_MPG_BT(VIU_MISC_CTRL0, 0, 16, 4);
		/* DI inp(current data) switch to memory */
		DIM_VSC_WR_MPG_BT(VIUB_MISC_CTRL0, 0, 16, 1);
	}
	/* DIM_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG,
	 * RD(DI_IF1_GEN_REG) & 0xfffffffe);
	 */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/*dbg a DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, 0);*/
		di_post_set_flow(1, EDI_POST_FLOW_STEP1_STOP);
		#ifdef DIM_HIS
		dim_print("%s:VD1_AFBCD0_MISC_CTRL 0", __func__);
		DIM_VSC_WR_MPG_BT(VD1_AFBCD0_MISC_CTRL, 0, 8, 2);
		DIM_VSC_WR_MPG_BT(VD1_AFBCD0_MISC_CTRL, 0, 20, 2);
		#endif
	}
}

void dimh_enable_di_post_mif(enum gate_mode_e mode)
{
	unsigned char gate = 0;

	switch (mode) {
	case GATE_OFF:
		gate = 1;
		break;
	case GATE_ON:
		gate = 2;
		break;
	case GATE_AUTO:
		gate = 2;
		break;
	default:
		gate = 0;
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable if0 external gate freerun hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 2, 2);
		/* enable if1 external gate freerun hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 4, 2);
		/* enable if1 external gate freerun hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 6, 2);
		/* enable di wr external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 8, 2);
		/* enable mtn rd external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 10, 2);
		/* enable mv rd external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 12, 2);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/* enable if1 external gate freerun hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1,
				   ((gate == 0) ? 2 : gate), 2, 2);
		/* enable if2 external gate freerun hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1,
				   ((gate == 0) ? 2 : gate), 4, 2);
		/* enable di wr external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 6, 2);
		/* enable mtn rd external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 8, 2);
		/* enable mv rd external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, gate, 10, 2);
	}
}

void dim_hw_disable(bool mc_enable)
{
	dimh_enable_di_pre_mif(false, mc_enable);
	DIM_DI_WR(DI_POST_SIZE, (32 - 1) | ((128 - 1) << 16));
	DIM_DI_WR_REG_BITS(DI_IF1_GEN_REG, 0, 0, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		DIM_DI_WR_REG_BITS(DI_IF2_GEN_REG, 0, 0, 1);
	/* disable ma,enable if0 to vpp,enable afbc to vpp */
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (rd_reg_bits(VIU_MISC_CTRL0, 16, 4) != 0)
			DIM_DI_WR_REG_BITS(VIU_MISC_CTRL0, 0, 16, 4);
		/* DI inp(current data) switch to memory */
		DIM_DI_WR_REG_BITS(VIUB_MISC_CTRL0, 0, 16, 1);
	}
	DIM_DI_WR(DI_POST_CTRL, 0);
}

/*
 * old pulldown windows share below ctrl
 * registers
 * with new pulldown windows
 */
void dim_film_mode_win_config(unsigned int width, unsigned int height)
{
	unsigned int win0_start_x, win0_end_x, win0_start_y, win0_end_y;
	unsigned int win1_start_x, win1_end_x, win1_start_y, win1_end_y;
	unsigned int win2_start_x, win2_end_x, win2_start_y, win2_end_y;
	unsigned int win3_start_x, win3_end_x, win3_start_y, win3_end_y;
	unsigned int win4_start_x, win4_end_x, win4_start_y, win4_end_y;

	win0_start_x = 0;
	win1_start_x = 0;
	win2_start_x = 0;
	win3_start_x = 0;
	win4_start_x = 0;
	win0_end_x = width - 1;
	win1_end_x = width - 1;
	win2_end_x = width - 1;
	win3_end_x = width - 1;
	win4_end_x = width - 1;
	win0_start_y = 0;
	win1_start_y = (height >> 3); /* 1/8 */
	win0_end_y = win1_start_y - 1;
	win2_start_y = win1_start_y + (height >> 2); /* 1/4 */
	win1_end_y = win2_start_y - 1;
	win3_start_y = win2_start_y + (height >> 2); /* 1/4 */
	win2_end_y = win3_start_y - 1;
	win4_start_y = win3_start_y + (height >> 2); /* 1/4 */
	win3_end_y = win4_start_y - 1;
	win4_end_y = win4_start_y + (height >> 3) - 1; /* 1/8 */

	DIM_RDMA_WR(DI_MC_REG0_X, (win0_start_x << 16) | win0_end_x);
	DIM_RDMA_WR(DI_MC_REG0_Y, (win0_start_y << 16) | win0_end_y);
	DIM_RDMA_WR(DI_MC_REG1_X, (win1_start_x << 16) | win1_end_x);
	DIM_RDMA_WR(DI_MC_REG1_Y, (win1_start_y << 16) | win1_end_y);
	DIM_RDMA_WR(DI_MC_REG2_X, (win2_start_x << 16) | win2_end_x);
	DIM_RDMA_WR(DI_MC_REG2_Y, (win2_start_y << 16) | win2_end_y);
	DIM_RDMA_WR(DI_MC_REG3_X, (win3_start_x << 16) | win3_end_x);
	DIM_RDMA_WR(DI_MC_REG3_Y, (win3_start_y << 16) | win3_end_y);
	DIM_RDMA_WR(DI_MC_REG4_X, (win4_start_x << 16) | win4_end_x);
	DIM_RDMA_WR(DI_MC_REG4_Y, (win4_start_y << 16) | win4_end_y);
}

/*
 * old pulldown detction module, global field diff/num & frame
 * diff/numm and 5 window included
 */
void dim_read_pulldown_info(unsigned int *glb_frm_mot_num,
			    unsigned int *glb_fid_mot_num)
{
	/*
	 * addr will increase by 1 automatically
	 */
	DIM_DI_WR(DI_INFO_ADDR, 1);
	*glb_frm_mot_num = (RD(DI_INFO_DATA) & 0xffffff);
	DIM_DI_WR(DI_INFO_ADDR, 4);
	*glb_fid_mot_num = (RD(DI_INFO_DATA) & 0xffffff);
}

/*
 * DIPD_RO_COMB_0~DIPD_RO_COMB11 and DI_INFO_DATA
 * will be reset, so call this function after all
 * data have be fetched
 */
void dim_pulldown_info_clear_g12a(void)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 1, 30, 1);
}

/*
 * manual reset contrd, cont2rd, mcinford mif
 * which fix after g12a
 */
static void reset_pre_simple_rd_mif_g12(unsigned char madi_en,
					unsigned char mcdi_en)
{
	unsigned int reg_val = 0;

	if (madi_en || mcdi_en) {
		DIM_RDMA_WR_BITS(CONTRD_CTRL2, 1, 31, 1);
		DIM_RDMA_WR_BITS(CONT2RD_CTRL2, 1, 31, 1);
		DIM_RDMA_WR_BITS(MCINFRD_CTRL2, 1, 31, 1);
		reg_val = DIM_RDMA_RD(DI_PRE_CTRL);
		if (madi_en)
			reg_val |= (1 << 25);
		if (mcdi_en)
			reg_val |= (1 << 10);
		/* enable cont rd&mcinfo rd, manual start */
		DIM_RDMA_WR(DI_PRE_CTRL, reg_val);
		DIM_RDMA_WR_BITS(CONTRD_CTRL2, 0, 31, 1);
		DIM_RDMA_WR_BITS(CONT2RD_CTRL2, 0, 31, 1);
		DIM_RDMA_WR_BITS(MCINFRD_CTRL2, 0, 31, 1);
	}
}

/*
 * frame reset for pre which have nothing with encoder
 * go field
 */
void dim_pre_frame_reset_g12(unsigned char madi_en,
			     unsigned char mcdi_en)
{
	unsigned int reg_val = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		reset_pre_simple_rd_mif_g12(madi_en, mcdi_en);
	} else {
		reg_val = DIM_RDMA_RD(DI_PRE_CTRL);
		if (madi_en)
			reg_val |= (1 << 25);
		if (mcdi_en)
			reg_val |= (1 << 10);
		DIM_RDMA_WR(DI_PRE_CTRL, reg_val);
	}
	/* reset simple mif which framereset not cover */
	DIM_RDMA_WR_BITS(CONTWR_CAN_SIZE, 1, 14, 1);
	DIM_RDMA_WR_BITS(MTNWR_CAN_SIZE, 1, 14, 1);
	DIM_RDMA_WR_BITS(MCVECWR_CAN_SIZE, 1, 14, 1);
	DIM_RDMA_WR_BITS(MCINFWR_CAN_SIZE, 1, 14, 1);

	DIM_RDMA_WR_BITS(CONTWR_CAN_SIZE, 0, 14, 1);
	DIM_RDMA_WR_BITS(MTNWR_CAN_SIZE, 0, 14, 1);
	DIM_RDMA_WR_BITS(MCVECWR_CAN_SIZE, 0, 14, 1);
	DIM_RDMA_WR_BITS(MCINFWR_CAN_SIZE, 0, 14, 1);

	#ifdef OLD_PRE_GL
	reg_val = 0xc3200000 | dimp_get(edi_mp_line_num_pre_frst);
	DIM_RDMA_WR(DI_PRE_GL_CTRL, reg_val);
	reg_val = 0x83200000 | dimp_get(edi_mp_line_num_pre_frst);
	DIM_RDMA_WR(DI_PRE_GL_CTRL, reg_val);
	#else
	hpre_gl_sw(false);
	hpre_gl_sw(true);
	#endif
}

/*2019-12-25 by feijun*/
void hpre_gl_sw(bool on)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (on)
		DIM_RDMA_WR(DI_PRE_GL_CTRL,
			    0x80200000 | dimp_get(edi_mp_line_num_pre_frst));
	else
		DIM_RDMA_WR(DI_PRE_GL_CTRL, 0xc0000000);
}

/*
 * frame + soft reset for the pre modules
 */

void dim_pre_frame_reset(void)
{
	DIM_RDMA_WR_BITS(DI_PRE_CTRL, 3, 30, 2);
}

/*
 * frame reset for post which have nothing with encoder
 * go field
 */
void post_frame_reset_g12a(void)
{
	unsigned int reg_val = 0;

	reg_val = (0xc0200000 | dimp_get(edi_mp_line_num_post_frst));
	DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, reg_val);
	reg_val = (0x80200000 | dimp_get(edi_mp_line_num_post_frst));
	DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, reg_val);
}

void dim_post_read_reverse_irq(bool reverse, unsigned char mc_pre_flag,
			       bool mc_enable)
{
	unsigned short flag_val = 1;

	mc_pre_flag = dimp_get(edi_mp_if2_disable) ? 1 : mc_pre_flag;
	if (reverse) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
			DIM_VSC_WR_MPG_BT(DI_IF0_GEN_REG2, 3, 2, 2);
			DIM_VSC_WR_MPG_BT(MTNRD_CTRL1, 3, 4, 2);
		} else {
			DIM_VSC_WR_MPG_BT(VD1_IF0_GEN_REG2, 0xf, 2, 4);
			DIM_VSC_WR_MPG_BT(DI_MTNRD_CTRL, 0xf, 17, 4);
		}
		DIM_VSC_WR_MPG_BT(DI_IF1_GEN_REG2, 3, 2, 2);
		DIM_VSC_WR_MPG_BT(VD2_IF0_GEN_REG2, 0xf, 2, 4);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			DIM_VSC_WR_MPG_BT(DI_IF2_GEN_REG2,  3, 2, 2);
		if (mc_enable) {
			/* motion vector read reverse*/
			DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_X, 1, 30, 1);
			DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_Y, 1, 30, 1);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
				if (is_meson_txlx_cpu()) {
					DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
							  dimp_get(edi_mp_pre),
							  8, 2);
					flag_val = (dimp_get(edi_mp_pre) != 2) ?
						    0 : 1;
				} else {
					DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
							  mc_pre_flag, 8, 2);
					flag_val = (mc_pre_flag != 2) ? 0 : 1;
				}
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
						  flag_val, 11, 1);
				/* disable if2 for wave if1 case,
				 *disable mc for pq issue
				 */
				if (dimp_get(edi_mp_if2_disable)) {
					DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
							  0, 11, 1);
					DIM_VSC_WR_MPG_BT(DI_IF2_GEN_REG,
							  0, 0, 1);
					if (cpu_after_eq
						(MESON_CPU_MAJOR_ID_GXLX))
						DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
								  0, 18, 1);
				}
			} else {
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
						  mc_pre_flag, 8, 1);
			}
		}
	} else {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
			DIM_VSC_WR_MPG_BT(DI_IF0_GEN_REG2, 0, 2, 2);
			DIM_VSC_WR_MPG_BT(MTNRD_CTRL1, 0, 4, 2);
		} else {
			DIM_VSC_WR_MPG_BT(VD1_IF0_GEN_REG2, 0, 2, 4);
			DIM_VSC_WR_MPG_BT(DI_MTNRD_CTRL, 0, 17, 4);
		}
		DIM_VSC_WR_MPG_BT(DI_IF1_GEN_REG2,  0, 2, 2);
		DIM_VSC_WR_MPG_BT(VD2_IF0_GEN_REG2, 0, 2, 4);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			DIM_VSC_WR_MPG_BT(DI_IF2_GEN_REG2, 0, 2, 2);
		if (mc_enable) {
			DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_X, 0, 30, 1);
			DIM_VSC_WR_MPG_BT(MCDI_MCVECRD_Y, 0, 30, 1);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
				if (is_meson_txlx_cpu()) {
					DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
							  dimp_get(edi_mp_pre),
							  8, 2);
					flag_val =
					(dimp_get(edi_mp_pre) != 2) ? 0 : 1;
				} else {
					DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
							  mc_pre_flag, 8, 2);
					flag_val = (mc_pre_flag != 2) ? 0 : 1;
				}
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
						  flag_val, 11, 1);
				/* disable if2 for wave if1 case */
				if (dimp_get(edi_mp_if2_disable)) {
					DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
							  0, 11, 1);
					DIM_VSC_WR_MPG_BT(DI_IF2_GEN_REG,
							  0, 0, 1);
					if (cpu_after_eq
						(MESON_CPU_MAJOR_ID_GXLX))
						DIM_VSC_WR_MPG_BT
							(MCDI_MC_CRTL,
							 0, 18, 1);
				}
			} else {
				DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL,
						  mc_pre_flag, 8, 1);
			}
		}
	}
}

void dim_set_power_control(unsigned char enable)
{
	ext_ops.dim_vpu_mem_pd_vmod(VPU_VIU_VD1, enable ? true : false);
	ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, enable ? true : false);
}

void dim_top_gate_control(bool top_en, bool mc_en)
{
	if (top_en) {
		/* enable clkb input */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 1, 0, 1);
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 1, 15, 1);
		/* enable slow clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, mc_en ? 1 : 0, 10, 1);
		/* enable di arb */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 2, 0, 2);
	} else {
		/* disable clkb input */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 0, 1);
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 15, 1);
		/* disable slow clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 10, 1);
		/* disable di arb */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 1, 0, 2);
	}
}

void dim_pre_gate_control(bool gate, bool mc_enable)
{
	if (gate) {
		/* enable ma pre clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 1, 8, 1);
		/* enable mc clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 1, 11, 1);
		/* enable pd clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 2, 2, 2);
		/* enable motion clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 2, 4, 2);
		/* enable deband clk gate freerun for hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 2, 6, 2);
		/* enable input mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 2, 16, 2);
		/* enable mem mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 2, 18, 2);
		/* enable chan2 mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 2, 20, 2);
		/* enable nr wr mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 2, 22, 2);
		/* enable mtn wr mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 2, 24, 2);
		if (mc_enable) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 0, 12, 2);
			else
				/* enable me clk always run vlsi issue */
				DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 3, 12, 2);
			/*
			 * enable mc pre mv(wr) mcinfo w/r
			 * mif external gate
			 */
			DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1,
					   2, 26, 2);
		}
		/* cowork with auto gate to config reg */
		DIM_DI_WR_REG_BITS(DI_PRE_CTRL, 3, 2, 2);
	} else {
		/* disable ma pre clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 8, 1);
		/* disable mc clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 11, 1);
		/* disable pd clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 1, 2, 2);
		/* disable motion clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 1, 4, 2);
		/* disable deband clk gate freerun for hw issue */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 1, 6, 2);
		/* disable input mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 1, 16, 2);
		/* disable mem mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 1, 18, 2);
		/* disable chan2 mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 1, 20, 2);
		/* disable nr wr mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 1, 22, 2);
		/* disable mtn wr mif external gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1, 1, 24, 2);
		if (mc_enable) {
			/* disable mc pre mv(wr) mcinfo
			 * w/r mif external gate
			 */
			DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL1,
					   1, 26, 2);
			/* disable me clk gate */
			DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL2, 1, 12, 2);
		}
	}
}

void dim_post_gate_control(bool gate)
{
	if (gate) {
		/* enable clk post div */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 1, 12, 1);
		/* enable post line buf/fifo/mux clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 1, 9, 1);
		/* enable blend1 clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL3, 0, 0, 2);
		/* enable ei clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL3, 0, 2, 2);
		/* enable ei_0 clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL3, 0, 4, 2);
	} else {
		/* disable clk post div */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 12, 1);
		/* disable post line buf/fifo/mux clk */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL0, 0, 9, 1);
		/* disable blend1 clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL3, 1, 0, 2);
		/* disable ei clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL3, 1, 2, 2);
		/* disable ei_0 clk gate */
		DIM_DI_WR_REG_BITS(VIUB_GCLK_CTRL3, 1, 4, 2);
	}
}

static void di_async_reset(void)/*2019-01-17 add for debug*/
{
	/*wrmif async reset*/
	DIM_RDMA_WR_BITS(VIUB_SW_RESET, 1, 14, 1);
	DIM_RDMA_WR_BITS(VIUB_SW_RESET, 0, 14, 1);
}

static void di_pre_rst_frame(void)
{
	DIM_RDMA_WR(DI_PRE_CTRL, RD(DI_PRE_CTRL) | (1 << 31));
}

static void di_pre_nr_enable(bool on)
{
	if (on)
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 1, 0, 1);
	else
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 0, 0, 1);
}

void dim_pre_nr_wr_done_sel(bool on)
{
	if (on)	/*wait till response finish*/
		DIM_RDMA_WR_BITS(DI_CANVAS_URGENT0, 1, 8, 1);
	else
		DIM_RDMA_WR_BITS(DI_CANVAS_URGENT0, 0, 0, 1);
}

void dim_rst_protect(bool on)
{
	if (on)
		DIM_RDMA_WR_BITS(DI_NRWR_Y, 1, 15, 1);
	else
		DIM_RDMA_WR_BITS(DI_NRWR_Y, 0, 15, 1);
}

/*bit 10,12,16,18 [3:1]*/
/*#define PRE_ID_MASK	(0x5140e) */
#define PRE_ID_MASK	(0x51400)

/*bit 8,10,14,16*/
#define PRE_ID_MASK_TL1	(0x14500)

static bool di_pre_idle(void)
{
	bool ret = false;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if ((DIM_RDMA_RD(DI_ARB_DBG_STAT_L1C1) &
			PRE_ID_MASK_TL1) == PRE_ID_MASK_TL1)
			ret = true;
	} else {
		if ((DIM_RDMA_RD(DI_ARB_DBG_STAT_L1C1_OLD) &
			PRE_ID_MASK) == PRE_ID_MASK)
			ret = true;
	}

	return ret;
}

void dim_arb_sw(bool on)
{
	int i;
	u32 REG_VPU_WRARB_REQEN_SLV_L1C1;
	u32 REG_VPU_RDARB_REQEN_SLV_L1C1;
	u32 REG_VPU_ARB_DBG_STAT_L1C1;
	u32 WRARB_onval;
	u32 WRARB_offval;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		REG_VPU_WRARB_REQEN_SLV_L1C1 = DI_WRARB_REQEN_SLV_L1C1;
		REG_VPU_RDARB_REQEN_SLV_L1C1 = DI_RDARB_REQEN_SLV_L1C1;
		REG_VPU_ARB_DBG_STAT_L1C1 = DI_ARB_DBG_STAT_L1C1;
		if (on)
			WRARB_onval = 0x3f;
		else
			WRARB_offval = 0x3e;
	} else {
		REG_VPU_WRARB_REQEN_SLV_L1C1 = DI_WRARB_REQEN_SLV_L1C1_OLD;
		REG_VPU_RDARB_REQEN_SLV_L1C1 = DI_RDARB_REQEN_SLV_L1C1_OLD;
		REG_VPU_ARB_DBG_STAT_L1C1 = DI_ARB_DBG_STAT_L1C1_OLD;
		if (on)
			WRARB_onval = 0x3f;
		else
			WRARB_offval = 0x2b;
	}

	if (on) {
		DIM_RDMA_WR(REG_VPU_WRARB_REQEN_SLV_L1C1, WRARB_onval);
		DIM_RDMA_WR(REG_VPU_RDARB_REQEN_SLV_L1C1, 0xffff);
	} else {
		/*close arb:*/
		DIM_RDMA_WR(REG_VPU_WRARB_REQEN_SLV_L1C1, WRARB_offval);
		DIM_RDMA_WR(REG_VPU_RDARB_REQEN_SLV_L1C1, 0xf1f1);

		di_pre_nr_enable(false); /*by Feijun*/
		/*check status*/
		if (!di_pre_idle()) {
			PR_ERR("%s:1:0x[%x]\n", __func__,
			       DIM_RDMA_RD(REG_VPU_ARB_DBG_STAT_L1C1));
			for (i = 0; i < 9; i++) {
				if (di_pre_idle())
					break;
			}

			if (!di_pre_idle()) {
				di_pre_rst_frame();

				for (i = 0; i < 9; i++) {
					if (di_pre_idle())
						break;
				}
				if (!di_pre_idle())
					PR_ERR("%s:2\n", __func__);
			}
		}
		if (di_pre_idle())
			di_async_reset();
	}
}

/* keep 0x1700' bit8 bit9 bit11 bit28 : 1*/

void dbg_set_DI_PRE_CTRL(void)
{
	unsigned int val;
	unsigned int mask, tmp;

	mask = 0x10000b00;

	val = RD(DI_PRE_CTRL);
	tmp = (~mask) & val;
	tmp = tmp | 0x10000000;

	DIM_DI_WR(DI_PRE_CTRL, tmp);
}

/*************************************
 *VIUB_SW_RESET's bits:
 *	[1][2]
 *	[10][11]
 *	[12][13][14]
 *	[16][17]
 *	[27]
 *	[28][31]
 *	all bits set 1 and then set 0
 *************************************/
void di_async_reset2(void)/*2019-04-05 add for debug*/
{
	unsigned int mask, val1, val2, val3;

	mask = 0x98037c06;
	val1 = RD(VIUB_SW_RESET);
	val2 = val1 | mask;

	DIM_DI_WR(VIUB_SW_RESET, val2);
	val3 = val2 & (~mask);
	DIM_DI_WR(VIUB_SW_RESET, val3);
	pr_info("%s:0x%x,0x%x,0x%x\n", __func__, val1, val2, val3);
}

#define DI_NOP_REG1	(0x2fcb)
#define DI_NOP_REG2	(0x2fcd)

void h_dbg_reg_set(unsigned int val)
{
	struct di_hpst_s  *pst = get_hw_pst();
	enum EDI_PST_ST pst_st = pst->state;
	unsigned int valb;

	DIM_DI_WR(DI_NOP_REG1, val);

	valb = pst_st;
	if (pst->curr_ch)
		valb = pst_st | 0x80000000;

	DIM_DI_WR(DI_NOP_REG2, valb);
}

/*below for post */
void post_mif_sw(bool on)
{
	if (on) {
		DIM_RDMA_WR_BITS(DI_IF0_GEN_REG, 1, 0, 1);
		/*by feijun 2018-11-19*/
		DIM_RDMA_WR_BITS(DI_IF1_GEN_REG, 1, 0, 1);
		/*by feijun 2018-11-19*/
		DIM_RDMA_WR_BITS(DI_IF2_GEN_REG, 1, 0, 1);

		DIM_RDMA_WR_BITS(DI_POST_CTRL, 1, 7, 1);
	} else {
		DIM_RDMA_WR_BITS(DI_IF0_GEN_REG, 0, 0, 1);
		/*by feijun 2018-11-19*/
		DIM_RDMA_WR_BITS(DI_IF1_GEN_REG, 0, 0, 1);
		/*by feijun 2018-11-19*/
		DIM_RDMA_WR_BITS(DI_IF2_GEN_REG, 0, 0, 1);
		DIM_RDMA_WR_BITS(DI_POST_CTRL, 0, 7, 1);
	}
	dim_print("%s:%d\n", __func__, on);
}

void post_close_new(void)
{
	unsigned int data32;

	post_mif_sw(false);
	data32 = RD(DI_INTR_CTRL);
	/*intr_mode*/
	DIM_DI_WR(DI_INTR_CTRL, (data32 & 0xffff0004) | (3 << 30));
	di_post_set_flow(1, EDI_POST_FLOW_STEP1_STOP);	/*dbg a*/
}

/*asynchronous rest ,2018-11-19 from feijun: after set reset ,*/
/*mif setting must set again*/
void di_post_reset(void)
{
#ifdef MARK_HIS
	0x2001 = 0x48300;
	0x2001 = 0x0;
	0x2002 = 0x30c1;
	0x2002 = 0x0;

	0x2001 = 0x483c0;
	0x2001 = 0x0;
	0x2002 = 0x3001;
	0x2002 = 0x0;
#endif
	WR(VIUB_SW_RESET, 0x483c0);
	WR(VIUB_SW_RESET, 0x0);

	WR(VIUB_SW_RESET0, 0x3001);
	WR(VIUB_SW_RESET0, 0x0);
	pr_info("%s\n", __func__);
}

void post_dbg_contr(void)
{
	DIM_RDMA_WR_BITS(DI_IF0_GEN_REG3, 1, 11, 1);
	DIM_RDMA_WR_BITS(DI_IF1_GEN_REG3, 1, 11, 1);
	DIM_RDMA_WR_BITS(DI_IF2_GEN_REG3, 1, 11, 1);
}

void di_post_set_flow(unsigned int post_wr_en, enum EDI_POST_FLOW step)
{
	unsigned int val;

	if (!post_wr_en)
		return;

	switch (step) {
	case EDI_POST_FLOW_STEP1_STOP:
		/*val = (0xc0200000 | line_num_post_frst);*/
		val = (0xc0000000 | 1);
		DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, val);
		break;
	case EDI_POST_FLOW_STEP2_START:
		/*val = (0x80200000 | line_num_post_frst);*/
		val = (0x80200000 | 1);
		DIM_VSYNC_WR_MPEG_REG(DI_POST_GL_CTRL, val);
		break;
	}
}

/*add 2019-04-25 for post crash debug*/
void hpst_power_ctr(bool on)
{
	if (on) {
		ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, true);
		dim_post_gate_control(true);
		dimh_enable_di_post_mif(GATE_AUTO);
	} else {
		dimh_enable_di_post_mif(GATE_OFF);
		dim_post_gate_control(false);
		ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, false);
	}
	/*pr_info("%s:%d\n", __func__, on);*/
}

void hpst_dbg_power_ctr_trig(unsigned int cmd)
{
	if (cmd)
		hpst_power_ctr(true);
	else
		hpst_power_ctr(false);
}

void hpst_dbg_mem_pd_trig(unsigned int cmd)
{
	ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, false);
	ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, true);
/*	pr_info("%s\n", __func__);*/
}

void hpst_mem_pd_sw(unsigned int on)
{
	if (on)
		ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, true);
	else
		ext_ops.dim_vpu_mem_pd_vmod(VPU_DI_POST, false);
}

void hpst_vd1_sw(unsigned int on)
{
	if (on)
		ext_ops.dim_vpu_mem_pd_vmod(VPU_VIU_VD1, true);
	else
		ext_ops.dim_vpu_mem_pd_vmod(VPU_VIU_VD1, false);
}

void hpst_dbg_trig_gate(unsigned int cmd)
{
	dim_post_gate_control(false);
	dim_post_gate_control(true);
	pr_info("%s\n", __func__);
}

void hpst_dbg_trig_mif(unsigned int cmd)
{
	dimh_enable_di_post_mif(GATE_OFF);
	dimh_enable_di_post_mif(GATE_AUTO);
	pr_info("%s\n", __func__);
}

/**/
/*
 * enable/disable mc pre mif mcinfo&mv
 */
static void mc_pre_mif_ctrl_g12(bool enable)
{
	unsigned char mif_ctrl = 0;

	mif_ctrl = enable ? 1 : 0;
	/* enable mcinfo rd mif */
	DIM_RDMA_WR_BITS(DI_PRE_CTRL, mif_ctrl, 10, 1);
	/* enable mv wr mif */
	DIM_RDMA_WR_BITS(MCVECWR_CTRL, mif_ctrl, 12, 1);
	/* enable mcinfo wr mif */
	DIM_RDMA_WR_BITS(MCINFWR_CTRL, mif_ctrl, 12, 1);
}

static void mc_pre_mif_ctrl(bool enable)
{
	if (enable) {
		/* gate clk */
		DIM_RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 0, 9, 1);
		/* gate clk */
		DIM_RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 0, 9, 1);
		/* mcinfo rd req en =1 */
		DIM_RDMA_WR_BITS(MCDI_MCINFORD_CTRL, 1, 9, 1);
		/* mv wr req en =1 */
		DIM_RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 1, 12, 1);
		/* mcinfo wr req en =1 */
		DIM_RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 1, 12, 1);
	} else {
		/* no gate clk */
		DIM_RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 1, 9, 1);
		/* no gate clk */
		DIM_RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 1, 9, 1);
		/* mcvec wr req en =0 */
		DIM_RDMA_WR_BITS(MCDI_MCVECWR_CTRL, 0, 12, 1);
		/* mcinfo wr req en =0 */
		DIM_RDMA_WR_BITS(MCDI_MCINFOWR_CTRL, 0, 12, 1);
		/* mcinfo rd req en = 0 */
		DIM_RDMA_WR_BITS(MCDI_MCINFORD_CTRL, 0, 9, 1);
	}
}

/*
 * enable/disable madi pre mif, mtn&cont
 */
static void ma_pre_mif_ctrl_g12(bool enable)
{
	if (enable) {
		/* enable cont wr mif */
		DIM_RDMA_WR_BITS(CONTWR_CTRL, 1, 12, 1);
		/* enable mtn wr mif */
		DIM_RDMA_WR_BITS(MTNWR_CTRL, 1, 12, 1);
		/* enable cont rd mif */
	} else {
		DIM_RDMA_WR_BITS(MTNWR_CTRL, 0, 12, 1);
		DIM_RDMA_WR_BITS(CONTWR_CTRL, 0, 12, 1);
		/* disable cont rd */
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 0, 25, 1);
	}
}

/*
 * use logic enable/disable replace mif
 */
static void ma_pre_mif_ctrl(bool enable)
{
	if (!enable) {
		/* mtn wr req en =0 */
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 0, 1, 1);
		/* cont wr req en =0 */
		DIM_RDMA_WR_BITS(DI_MTN_1_CTRL1, 0, 31, 1);
		/* disable cont rd */
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 0, 25, 1);
	}
}

/*
 * enable/disable inp&chan2&mem&nrwr mif
 */
static void di_pre_data_mif_ctrl(bool enable)
{
	if (enable) {
		/* enable input mif*/
		DIM_DI_WR(DI_CHAN2_GEN_REG, RD(DI_CHAN2_GEN_REG) | 0x1);
		DIM_DI_WR(DI_MEM_GEN_REG, RD(DI_MEM_GEN_REG) | 0x1);
		#ifdef MARK_HIS
		if (rd_reg_bits(VIU_MISC_CTRL1, 0, 1) == 1) {
			DI_WR(DI_INP_GEN_REG, RD(DI_INP_GEN_REG) & ~0x1);
			DIM_RDMA_WR_BITS(VD2_AFBC_ENABLE, 1, 8, 1);
		} else {
			DI_WR(DI_INP_GEN_REG, RD(DI_INP_GEN_REG) | 0x1);
			DIM_RDMA_WR_BITS(VD2_AFBC_ENABLE, 0, 8, 1);
		}
		#else
		if (afbc_is_used()) {
			DIM_DI_WR(DI_INP_GEN_REG, RD(DI_INP_GEN_REG) & ~0x1);
			afbc_input_sw(true);
		} else {
			DIM_DI_WR(DI_INP_GEN_REG, RD(DI_INP_GEN_REG) | 0x1);
			afbc_input_sw(false);
		}
		#endif
		/* nrwr no clk gate en=0 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 0, 24, 1);*/
	} else {
		/* nrwr no clk gate en=1 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 1, 24, 1);*/
		/* nr wr req en =0 */
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 0, 0, 1);
		/* disable input mif*/
		DIM_DI_WR(DI_CHAN2_GEN_REG, RD(DI_CHAN2_GEN_REG) & ~0x1);
		DIM_DI_WR(DI_MEM_GEN_REG, RD(DI_MEM_GEN_REG) & ~0x1);
		DIM_DI_WR(DI_INP_GEN_REG, RD(DI_INP_GEN_REG) & ~0x1);
		#ifdef MARK_HIS
		/* disable AFBC input */
		if (rd_reg_bits(VIU_MISC_CTRL1, 0, 1) == 1)
			DIM_RDMA_WR_BITS(VD2_AFBC_ENABLE, 0, 8, 1);
		#else
		/* disable AFBC input */
		if (afbc_is_used())
			afbc_input_sw(false);

		#endif
	}
}

static atomic_t mif_flag;
void dimh_enable_di_pre_mif(bool en, bool mc_enable)
{
	if (atomic_read(&mif_flag))
		return;

	if (dimp_get(edi_mp_pre_mif_gate) && !en)
		return;
	atomic_set(&mif_flag, 1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (mc_enable)
			mc_pre_mif_ctrl_g12(en);
		ma_pre_mif_ctrl_g12(en);
	} else {
		if (mc_enable)
			mc_pre_mif_ctrl(en);
		ma_pre_mif_ctrl(en);
	}
	di_pre_data_mif_ctrl(en);
	atomic_set(&mif_flag, 0);
}

void dimh_combing_pd22_window_config(unsigned int width, unsigned int height)
{
	unsigned short y1 = 39, y2 = height - 41;

	if (height >= 540) {
		y1 = 79;
		y2 = height - 81;
	}
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
		DIM_DI_WR_REG_BITS(DECOMB_WIND00, 0, 16, 13);/* dcomb x0 */
		/* dcomb x1 */
		DIM_DI_WR_REG_BITS(DECOMB_WIND00, (width - 1), 0, 13);
		DIM_DI_WR_REG_BITS(DECOMB_WIND01, 0, 16, 13);/* dcomb y0 */
		DIM_DI_WR_REG_BITS(DECOMB_WIND01, y1, 0, 13);/* dcomb y1 */
		DIM_DI_WR_REG_BITS(DECOMB_WIND10, 0, 16, 13);/* dcomb x0 */
		/* dcomb x1 */
		DIM_DI_WR_REG_BITS(DECOMB_WIND10, (width - 1), 0, 13);
		/* dcomb y0 */
		DIM_DI_WR_REG_BITS(DECOMB_WIND11, (y1 + 1), 16, 13);
		DIM_DI_WR_REG_BITS(DECOMB_WIND11, y2, 0, 13);/* dcomb y1 */
	}
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND0_X, 0, 0, 13);/* pd22 x0 */
	/* pd22 x1 */
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND0_X, (width - 1), 16, 13);
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND0_Y, 0, 0, 13);/* pd22 y0 */
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND0_Y, y1, 16, 13);/* pd y1 */
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND1_X, 0, 0, 13);/* pd x0 */
	/* pd x1 */
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND1_X, (width - 1), 16, 13);
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND1_Y, (y1 + 1), 0, 13);/* pd y0 */
	DIM_DI_WR_REG_BITS(MCDI_PD_22_CHK_WND1_Y, y2, 16, 13);/* pd y2 */
}

void dimh_pulldown_vof_win_config(struct pulldown_detected_s *wins)
{
	DIM_VSC_WR_MPG_BT(DI_BLEND_REG0_Y, wins->regs[0].win_vs, 17, 12);
	DIM_VSC_WR_MPG_BT(DI_BLEND_REG0_Y, wins->regs[0].win_ve, 1, 12);

	DIM_VSC_WR_MPG_BT(DI_BLEND_REG1_Y, wins->regs[1].win_vs, 17, 12);
	DIM_VSC_WR_MPG_BT(DI_BLEND_REG1_Y, wins->regs[1].win_ve, 1, 12);

	DIM_VSC_WR_MPG_BT(DI_BLEND_REG2_Y, wins->regs[2].win_vs, 17, 12);
	DIM_VSC_WR_MPG_BT(DI_BLEND_REG2_Y, wins->regs[2].win_ve, 1, 12);

	DIM_VSC_WR_MPG_BT(DI_BLEND_REG3_Y, wins->regs[3].win_vs, 17, 12);
	DIM_VSC_WR_MPG_BT(DI_BLEND_REG3_Y, wins->regs[3].win_ve, 1, 12);

	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL,
			  (wins->regs[0].win_ve > wins->regs[0].win_vs)
			  ? 1 : 0, 16, 1);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, wins->regs[0].blend_mode, 8, 2);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL,
			  (wins->regs[1].win_ve > wins->regs[1].win_vs)
			  ? 1 : 0, 17, 1);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, wins->regs[1].blend_mode, 10, 2);

	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL,
			  (wins->regs[2].win_ve > wins->regs[2].win_vs)
			  ? 1 : 0, 18, 1);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL, wins->regs[2].blend_mode, 12, 2);

	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL,
			  (wins->regs[3].win_ve > wins->regs[3].win_vs)
			  ? 1 : 0, 19, 1);
	DIM_VSC_WR_MPG_BT(DI_BLEND_CTRL,
			  wins->regs[3].blend_mode, 14, 2);
}

void dimh_load_regs(struct di_pq_parm_s *di_pq_ptr)
{
	unsigned int i = 0, j = 0, addr = 0, value = 0, mask = 0, len;
	unsigned int table_name = 0, nr_table = 0;
	bool ctrl_reg_flag = false;
	struct am_reg_s *regs_p = NULL;

	if (dimp_get(edi_mp_pq_load_dbg) == 1)
		return;
	if (dimp_get(edi_mp_pq_load_dbg) == 2)
		pr_info("[DI]%s hw load 0x%x pq table len %u.\n",
			__func__, di_pq_ptr->pq_parm.table_name,
			di_pq_ptr->pq_parm.table_len);
	if (PTR_RET(di_pq_ptr->regs)) {
		PR_ERR("[DI] table ptr error.\n");
		return;
	}
	pr_info("[DI]%s hw load 0x%x pq table len %u.\n",
		__func__, di_pq_ptr->pq_parm.table_name,
		di_pq_ptr->pq_parm.table_len);
	nr_table = TABLE_NAME_NR | TABLE_NAME_DEBLOCK | TABLE_NAME_DEMOSQUITO;
	regs_p = (struct am_reg_s *)di_pq_ptr->regs;
	len = di_pq_ptr->pq_parm.table_len;
	table_name = di_pq_ptr->pq_parm.table_name;
	for (i = 0; i < len; i++) {
		ctrl_reg_flag = false;
		addr = regs_p->addr;
		value = regs_p->val;
		mask = regs_p->mask;
		if (dimp_get(edi_mp_pq_load_dbg) == 2)
			pr_info("[%u][0x%x] = [0x%x]&[0x%x]\n",
				i, addr, value, mask);

		for (j = 0; j < SKIP_CTRE_NUM; j++) {
			if (addr == ctrl_regs[j])
				break;
		}

		if (regs_p->mask != 0xffffffff) {
			value = ((RD(addr) & (~(mask))) |
				(value & mask));
		}
		regs_p++;
		if (j < SKIP_CTRE_NUM) {
			if (dimp_get(edi_mp_pq_load_dbg) == 3)
				pr_info("%s skip [0x%x]=[0x%x].\n",
					__func__, addr, value);
			continue;
		}
		if (table_name & nr_table)
			ctrl_reg_flag =
			get_ops_nr()->set_nr_ctrl_reg_table(addr, value);

		if (!ctrl_reg_flag)
			DIM_DI_WR(addr, value);
		if (dimp_get(edi_mp_pq_load_dbg) == 2)
			pr_info("[%u][0x%x] = [0x%x] %s\n", i, addr,
				value, RD(addr) != value ? "fail" : "success");
	}
}

/*note:*/
/*	function: patch for txl for progressive source	*/
/*		480p/576p/720p from hdmi will timeout	*/
/*	prog_flg: in:	1:progressive;			*/
/*	cnt:	in:	di_pre_stru.field_count_for_cont*/
/*	mc_en:	in:	mcpre_en*/
void dimh_txl_patch_prog(int prog_flg, unsigned int cnt, bool mc_en)
{
	unsigned int di_mtn_1_ctrl1 = 0; /*ary add tmp*/

	if (!prog_flg || !is_meson_txl_cpu())
		return;

	/*printk("prog patch\n");*/
	if (cnt >= 3) {
		di_mtn_1_ctrl1 |= 1 << 29;/* enable txt */

		if (mc_en) {
			DIM_RDMA_WR(DI_MTN_CTRL1,
				    (0xffffcfff & DIM_RDMA_RD(DI_MTN_CTRL1)));

			/* enable me(mc di) */
			if (cnt == 4) {
				di_mtn_1_ctrl1 &= (~(1 << 30));
				/* enable contp2rd and contprd */
				DIM_RDMA_WR(MCDI_MOTINEN, 1 << 1 | 1);
			}
			if (cnt == 5)
				DIM_RDMA_WR(MCDI_CTRL_MODE, 0x1bfff7ff);
		}
	} else {
		if (mc_en) {
			/* txtdet_en mode */
			DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 1, 1);
			DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 9, 1);
			DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 16, 1);
			DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 28, 1);
			DIM_RDMA_WR(MCDI_MOTINEN, 0);
			DIM_RDMA_WR(DI_MTN_CTRL1,
				    (0xffffcfff & DIM_RDMA_RD(DI_MTN_CTRL1)));
			/* disable me(mc di) */
		}
		DIM_RDMA_WR(DNR_CTRL, 0);
	}
	DIM_RDMA_WR(DI_MTN_1_CTRL1, di_mtn_1_ctrl1);
}

void dim_init_setting_once(void)
{
	if (di_get_flg_hw_int())
		return;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		dim_wr_cue_int();

	di_set_flg_hw_int(true);
}

int dim_seq_file_module_para_hw(struct seq_file *seq)
{
	seq_puts(seq, "hw---------------\n");
#ifdef MARK_HIS
	seq_printf(seq, "%-15s:%d\n", "pq_load_dbg", pq_load_dbg);
	seq_printf(seq, "%-15s:%d\n", "lmv_lock_win_en", lmv_lock_win_en);
	seq_printf(seq, "%-15s:%d\n", "lmv_dist", lmv_dist);
	seq_printf(seq, "%-15s:%d\n", "pr_mcinfo_cnt", pr_mcinfo_cnt);
	seq_printf(seq, "%-15s:%d\n", "offset_lmv", offset_lmv);
	seq_printf(seq, "%-15s:%d\n", "post_ctrl", post_ctrl);
	seq_printf(seq, "%-15s:%d\n", "if2_disable", if2_disable);
	seq_printf(seq, "%-15s:%d\n", "pre_flag", pre_flag);

	seq_printf(seq, "%-15s:%d\n", "pre_mif_gate", pre_mif_gate);
	seq_printf(seq, "%-15s:%d\n", "pre_urgent", pre_urgent);
	seq_printf(seq, "%-15s:%d\n", "pre_hold_line", pre_hold_line);
	seq_printf(seq, "%-15s:%d\n", "pre_ctrl, uint", pre_ctrl);
	seq_printf(seq, "%-15s:%d\n", "line_num_post_frst",
		   line_num_post_frst);
	seq_printf(seq, "%-15s:%d\n", "line_num_pre_frst", line_num_pre_frst);
	seq_printf(seq, "%-15s:%d\n", "pd22_flg_calc_en", pd22_flg_calc_en);

	/***********************/
	seq_printf(seq, "%-15s:%d\n", "mcen_mode", mcen_mode);
	seq_printf(seq, "%-15s:%d\n", "mcuv_en", mcuv_en);
	seq_printf(seq, "%-15s:%d\n", "mcdebug_mode", mcdebug_mode);
	seq_printf(seq, "%-15s:%d\n", "pldn_ctrl_rflsh", pldn_ctrl_rflsh);
#endif
	return 0;
}

