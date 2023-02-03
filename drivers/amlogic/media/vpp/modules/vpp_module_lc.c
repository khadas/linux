// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_lc.h"

#define LC_N (4)
#define LC_MTRX_SIZE (9)
#define LC_CURVE_NODES (6)
#define LC_HIST_BIN_COUNT (16)
#define LC_BLK_NUM_MAX_H (12)
#define LC_BLK_NUM_MAX_V (8)
/*(12*8*17)*/
#define LC_HIST_SIZE (1632)
/*(12*8*6+4)*/
#define LC_CURVE_SIZE (580)
/*(96=vnum*hnum)*/
#define LC_ALPHA_SIZE (96)

enum lc_lmt_type_e {
	EN_TYPE_LC_LMT_12 = 0,
	EN_TYPE_LC_LMT_16,
};

enum lc_csc_type_e {
	EN_LC_CSC_LINEAR = 0,
	EN_LC_CSC_RGB_YUV601L,
	EN_LC_CSC_YUV601L_RGB,
	EN_LC_CSC_RGB_YUV709L,
	EN_LC_CSC_YUV709L_RGB,
	EN_LC_CSC_RGB_YUV709,  /*5*/
	EN_LC_CSC_YUV709_RGB,
	EN_LC_CSC_MAX,
};

struct _lc_csc_mtrx_idx_s {
	int csc_coef_idx;
	int stts_coef_idx;
	int csc_ofst_idx;
	int stts_ofst_idx;
};

struct _lc_csc_mtrx_coef_s {
	int data[5];
};

struct _lc_csc_mtrx_ofst_s {
	int data[4];
};

struct _lc_curve_lmt_lut_s {
	int data[16];
};

struct _sr1_lc_sat_lut_s {
	int data[63];
};

struct _lc_bit_cfg_s {
	struct _bit_s bit_curve_en;
	struct _bit_s bit_curve_interrupt_en;
	struct _bit_s bit_stts_mtrx_en;
	struct _bit_s bit_stts_in_data_sel;
	struct _bit_s bit_stts_probe_en;
	struct _bit_s bit_stts_hist_region_rd_idx;  /*5*/
	struct _bit_s bit_stts_lpf_en;
	struct _bit_s bit_stts_hist_mode;
	struct _bit_s bit_stts_eol_en;
	struct _bit_s bit_stts_pix_drop_mode;
	struct _bit_s bit_stts_lc_max_static_en;    /*10*/
	struct _bit_s bit_stts_black_data_thrd;
	struct _bit_s bit_stts_black_cnt;
};

struct _lc_reg_cfg_s {
	unsigned char page;
	unsigned char reg_curve_ctrl;
	unsigned char reg_curve_hv_num;
	unsigned char reg_curve_lmt_rat;
	unsigned char reg_curve_contrast_lh;
	unsigned char reg_curve_contrast_lmt_lh;  /*5*/
	unsigned char reg_curve_contrast_scl_lh;
	unsigned char reg_curve_misc0;
	unsigned char reg_curve_ypkbv_rat;
	unsigned char reg_curve_ypkbv_slp_lmt;
	unsigned char reg_curve_ymin_lmt_0_1;     /*10*/
	unsigned char reg_curve_ypkbv_ymax_lmt_0_1;
	unsigned char reg_curve_ymax_lmt_0_1;
	unsigned char reg_curve_histvld_thrd;
	unsigned char reg_curve_bb_mute_thrd;
	unsigned char reg_curve_ram_ctrl;         /*15*/
	unsigned char reg_curve_ram_addr;
	unsigned char reg_curve_ram_data;
	unsigned char reg_curve_ymin_lmt_12_13;
	unsigned char reg_curve_ymax_lmt_12_13;
	unsigned char reg_curve_ypkbv_lmt_0_1;    /*20*/
	unsigned char reg_stts_gclk_ctrl0;
	unsigned char reg_stts_ctrl0;
	unsigned char reg_stts_widthm1_heightm1;
	unsigned char reg_stts_mtrx_coef00_01;
	unsigned char reg_stts_mtrx_coef02_10;    /*25*/
	unsigned char reg_stts_mtrx_coef11_12;
	unsigned char reg_stts_mtrx_coef20_21;
	unsigned char reg_stts_mtrx_coef22;
	unsigned char reg_stts_mtrx_offset0_1;
	unsigned char reg_stts_mtrx_offset2;      /*30*/
	unsigned char reg_stts_mtrx_preoffset0_1;
	unsigned char reg_stts_mtrx_preoffset2;
	unsigned char reg_stts_hist_region_idx;
	unsigned char reg_stts_hist_set_region;
	unsigned char reg_stts_hist_read_region;  /*35*/
	unsigned char reg_stts_hist_start_rd_region;
	unsigned char reg_stts_black_info;
};

struct _sr1_bit_cfg_s {
	struct _bit_s bit_lc_in_csel;
	struct _bit_s bit_lc_in_ysel;
	struct _bit_s bit_lc_blkbld_mode;
	struct _bit_s bit_lc_en;
	struct _bit_s bit_lc_hblank;
	struct _bit_s bit_lc_sync_ctrl;  /*5*/
	struct _bit_s bit_lc_vnum;
	struct _bit_s bit_lc_hnum;
};

struct _sr1_reg_cfg_s {
	unsigned char page;
	unsigned char reg_lc_input_mux;
	unsigned char reg_lc_top_ctrl;
	unsigned char reg_lc_hv_num;
	unsigned char reg_lc_sat_lut_0_1;
	unsigned char reg_lc_sat_lut_62;     /*5*/
	unsigned char reg_lc_curve_blk_hidx_0_1;
	unsigned char reg_lc_curve_blk_vidx_0_1;
	unsigned char reg_lc_y2r_mtrx_0_1;
	unsigned char reg_lc_r2y_mtrx_0_1;
	unsigned char reg_lc_y2r_mtrx_ofst;  /*10*/
	unsigned char reg_lc_y2r_mtrx_clip;
	unsigned char reg_lc_r2y_mtrx_ofst;
	unsigned char reg_lc_r2y_mtrx_clip;
	unsigned char reg_lc_map_ram_ctrl;
	unsigned char reg_lc_map_ram_addr;   /*15*/
	unsigned char reg_lc_map_ram_data;
};

/*Default table from T3*/
static struct _lc_reg_cfg_s lc_reg_cfg = {
	0x40,
	0x00,
	0x01,
	0x02,
	0x03,
	0x04,  /*5*/
	0x05,
	0x07,
	0x08,
	0x09,
	0x0a,  /*10*/
	0x10,
	0x10,
	0x16,
	0x17,
	0x20,  /*15*/
	0x21,
	0x22,
	0x40,
	0x42,
	0x44,  /*20*/
	0x28,
	0x29,
	0x2a,
	0x2b,
	0x2c,  /*25*/
	0x2d,
	0x2e,
	0x2f,
	0x30,
	0x31,  /*30*/
	0x32,
	0x33,
	0x37,
	0x38,
	0x39,  /*35*/
	0x3a,
	0x3c
};

static struct _lc_bit_cfg_s lc_bit_cfg = {
	{0, 1},
	{31, 1},
	{2, 1},
	{3, 3},
	{10, 1},
	{14, 1},  /*5*/
	{21, 1},
	{22, 2},
	{28, 1},
	{29, 2},
	{31, 1},  /*10*/
	{0, 8},
	{8, 24}
};

static struct _sr1_reg_cfg_s sr1_reg_cfg = {
	0x52,
	0xb1,
	0xc0,
	0xc1,
	0xc2,
	0xe1,  /*5*/
	0xe2,
	0xe9,
	0xee,
	0xf3,
	0xf8,  /*10*/
	0xf9,
	0xfa,
	0xfb,
	0xfc,
	0xfd,  /*15*/
	0xfe
};

static struct _sr1_bit_cfg_s sr1_bit_cfg = {
	{0, 3},
	{4, 3},
	{0, 1},
	{4, 1},
	{8, 8},
	{16, 1},  /*5*/
	{0, 5},
	{8, 5}
};

static struct lc_vs_param_s lc_vs_param;

static int lc_top_cfg[EN_LC_CFG_PARAM_MAX] = {
	5,
	5,
	1,
	8,
	LC_BLK_NUM_MAX_V,
	LC_BLK_NUM_MAX_H,
};

static int lc_alg_cfg[EN_LC_ALG_PARAM_MAX] = {
	10,
	2,
	1200,
	3200,
	331,
	2,
	1,
	6,
	1,
	2,
	5,
	6,
	50,
	512,
	512,
	12,
};

/*lc curve data and hist data*/
static int lc_curve_isr_defined;
static int lc_curve_isr_used;
static int lc_demo_mode;/*0/1/2=off/left_side/right_side*/
static bool lc_support;
static bool lc_bypass;
static bool lc_curve_fresh;
static bool lc_malloc_ok;
static int *lc_hist_data;
static int *lc_wrcurve;/*10bit*/
static int *curve_nodes_cur;
static int *curve_nodes_pre;
static s64 *curve_nodes_pre_raw;
static int *refresh_alpha;
static enum lc_lmt_type_e lc_lmt_type;
static enum lc_curve_tune_mode_e lc_tune_mode;
static int lc_tune_curve[EN_LC_CURVE_TUNE_PARAM_MAX] = {0};

/*just temporarily define here to avoid grammar error*/
static int video_scene_change_flag_en;
static int video_scene_change_flag;

/*CSC matrix tables*/
static struct _lc_csc_mtrx_idx_s mtrx_idx_tbl[EN_LC_CSC_MAX];
static struct _lc_csc_mtrx_coef_s mtrx_coef_tbl[] = {
	/*LINEAR*/
	{{0x04000000, 0x0, 0x04000000, 0x0, 0x0400}},
	/*RGB_YUV601L*/
	{{0x01070204, 0x00640f68, 0x0ed601c2, 0x01c20e87, 0x0fb7}},
	/*YUV601L_RGB*/
	{{0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812, 0x0}},
	{{0x012a0000, 0x0198012a, 0x0f9c0f30, 0x012a0204, 0x0}},
	/*RGB_YUV709L*/
	{{0x00bb0275, 0x003f1f99, 0x1ea601c2, 0x01c21e67, 0x1fd7}},
	/*YUV709L_RGB*/
	{{0x04a80000, 0x072c04a8, 0x1f261ddd, 0x04a80876, 0x0}},
	{{0x012a0000, 0x01cb012a, 0x1fc90f77, 0x012a021d, 0x0}},
	/*RGB_YUV709*/
	{{0x00da02dc, 0x004a1f8a, 0x1e760200, 0x02001e2f, 0x1fd1}},
	/*YUV709_RGB*/
	{{0x04000000, 0x064d0400, 0x1f411e21, 0x0400076d, 0x0}},
	{{0x01000000, 0x01930100, 0x1fd01f88, 0x010001db, 0x0}},
};

static struct _lc_csc_mtrx_ofst_s mtrx_ofst_tbl[] = {
	{{0, 0, 0, 0}},
	{{0x00400200, 0x03ff, 0, 0}},
	{{0x01000800, 0x0fff, 0, 0}},
	{{0x00000200, 0x03ff, 0, 0}},
	{{0x00000800, 0x0fff, 0, 0}},
	{{0x00400200, 0x0200, 0, 0}},
	{{0, 0, 0x07c00600, 0x0600}},
	{{0, 0, 0x00000600, 0x0600}},
};

static struct _lc_curve_lmt_lut_s lmt_lut_tbl[] = {
	{{0x0030, 0x005d, 0x0083, 0x0091, 0x00a0, 0x00c4, 0x00e0, 0x0100,
	0x0120, 0x0140, 0x0160, 0x0190, 0x01b0, 0x01d0, 0x01f0, 0x0210}},
	{{0x0044, 0x00b4, 0x00fb, 0x0123, 0x0159, 0x01a2, 0x01d9, 0x0208,
	0x0240, 0x0280, 0x02d7, 0x0310, 0x0340, 0x0380, 0x03c0, 0x03ff}},
};

static struct _sr1_lc_sat_lut_s sat_lut_tbl[] = {
	{{64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960, 1024,
	1088, 1152, 1216, 1280, 1344, 1408, 1472, 1536,
	1600, 1664, 1728, 1792, 1856, 1920, 1984, 2048,
	2112, 2176, 2240, 2304, 2368, 2432, 2496, 2560,
	2624, 2688, 2752, 2816, 2880, 2944, 3008, 3072,
	3136, 3200, 3264, 3328, 3392, 3456, 3520, 3584,
	3648, 3712, 3776, 3840, 3904, 3968, 4032}},
};

/*Internal functions*/
static int _set_lc_curve_ctrl(int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(lc_reg_cfg.page, lc_reg_cfg.reg_curve_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static void _set_lc_curve_ymin_lmt(int *pdata)
{
	unsigned int i, j, tmp;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pdata)
		return;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ymin_lmt_0_1);

	for (i = 0; i < 6; i++) {
		tmp = ((pdata[2 * i] & 0x3ff) << 16) |
			(pdata[2 * i + 1] & 0x3ff);
		WRITE_VPP_REG_BY_MODE(io_mode, addr + i, tmp);
	}

	if (lc_lmt_type == EN_TYPE_LC_LMT_16) {
		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ymin_lmt_12_13);

		for (j = 0; j < 2; j++) {
			tmp = ((pdata[2 * i] & 0x3ff) << 16) |
				(pdata[2 * i + 1] & 0x3ff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr + j, tmp);
			i++;
		}
	}
}

static void _set_lc_curve_ypkbv_ymax_lmt(int *pdata)
{
	unsigned int i, tmp;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (lc_lmt_type == EN_TYPE_LC_LMT_12) {
		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ypkbv_ymax_lmt_0_1);

		for (i = 0; i < 6; i++) {
			tmp = ((pdata[2 * i] & 0x3ff) << 16) |
				(pdata[2 * i + 1] & 0x3ff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr + i, tmp);
		}
	}
}

static void _set_lc_curve_ymax_lmt(int *pdata)
{
	unsigned int i, j, tmp;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (lc_lmt_type == EN_TYPE_LC_LMT_16) {
		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ymax_lmt_0_1);

		for (i = 0; i < 6; i++) {
			tmp = ((pdata[2 * i] & 0x3ff) << 16) |
				(pdata[2 * i + 1] & 0x3ff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr + i, tmp);
		}

		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ymax_lmt_12_13);

		for (j = 0; j < 2; j++) {
			tmp = ((pdata[2 * i] & 0x3ff) << 16) |
				(pdata[2 * i + 1] & 0x3ff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr + j, tmp);
			i++;
		}
	}
}

static void _set_lc_curve_ypkbv_lmt(int *pdata)
{
	unsigned int i, tmp;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (lc_lmt_type == EN_TYPE_LC_LMT_16) {
		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ypkbv_lmt_0_1);

		for (i = 0; i < 8; i++) {
			tmp = ((pdata[2 * i] & 0x3ff) << 16) |
				(pdata[2 * i + 1] & 0x3ff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr + i, tmp);
		}
	}
}

static void _set_lc_curve_ypkbv_rat(int *pdata)
{
	unsigned int tmp;
	unsigned int addr = 0;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ypkbv_rat);

	tmp = (pdata[0] & 0xff) << 24 | (pdata[1] & 0xff) << 16 |
		(pdata[2] & 0xff) << 8 | (pdata[3] & 0xff);

	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, tmp);
}

static void _set_lc_curve_ypkbv_slp_lmt(int *pdata)
{
	unsigned int tmp;
	unsigned int addr = 0;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ypkbv_slp_lmt);

	tmp = (pdata[0] & 0xff) << 8 | (pdata[1] & 0xff);
	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, tmp);
}

static void _set_lc_curve_cntst_lmt_lh(int *pdata)
{
	unsigned int tmp;
	unsigned int addr = 0;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_contrast_lmt_lh);

	tmp = (pdata[0] & 0xff) << 24 | (pdata[1] & 0xff) << 16 |
		(pdata[2] & 0xff) << 8 | (pdata[3] & 0xff);

	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, tmp);
}

static void _set_lc_curve_config(int h_num, int v_num,
	unsigned int height, unsigned int width)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned int lc_histvld_thrd;
	unsigned int lc_blackbar_mute_thrd;
	unsigned int lmtrat_minmax;
	unsigned int tmp;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_lmt_rat);

	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	lmtrat_minmax = (tmp >> 8) & 0xff;

	tmp = (height * width) / (h_num * v_num);
	lc_histvld_thrd = (tmp * lmtrat_minmax) >> 10;
	lc_blackbar_mute_thrd = tmp >> 3;

	tmp = (h_num << 8) | v_num;
	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_hv_num);
	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, tmp);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_histvld_thrd);
	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, lc_histvld_thrd);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_bb_mute_thrd);
	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, lc_blackbar_mute_thrd);
}

static int _set_lc_stts_ctrl(int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(lc_reg_cfg.page, lc_reg_cfg.reg_stts_ctrl0);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_lc_stts_hist_region_idx(int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_hist_region_idx);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static void _set_lc_stts_hist_region(unsigned int height,
	unsigned int width)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	int i, tmp;
	int blk_height, blk_width;
	int row_start = 0;
	int col_start = 0;
	int h_num = LC_BLK_NUM_MAX_H;
	int v_num = LC_BLK_NUM_MAX_V;
	int hend[LC_BLK_NUM_MAX_H] = {0};
	int vend[LC_BLK_NUM_MAX_V] = {0};

	blk_height = height / v_num;
	blk_width = width / h_num;

	hend[0] = col_start + blk_width - 1;
	hend[11] = width - 1;
	for (i = 1; i < LC_BLK_NUM_MAX_H - 1; i++)
		hend[i] = hend[i - 1] + blk_width;

	vend[0] = row_start + blk_height - 1;
	vend[7] = height - 1;
	for (i = 1; i < LC_BLK_NUM_MAX_V - 1; i++)
		vend[i] = vend[i - 1] + blk_height;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_hist_region_idx);

	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0xffe0ffff & tmp);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_hist_set_region);

	tmp = (((row_start & 0x1fff) << 16) & 0xffff0000) |
		(col_start & 0x1fff);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);

	for (i = 0; i < LC_BLK_NUM_MAX_H >> 1; i++) {
		if (i < 4) {
			tmp = ((hend[2 * i + 1] & 0x1fff) << 16) |
				(hend[2 * i] & 0x1fff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);

			tmp = ((vend[2 * i + 1] & 0x1fff) << 16) |
				(vend[2 * i] & 0x1fff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);
		} else {
			tmp = ((hend[2 * i + 1] & 0x1fff) << 16) |
				(hend[2 * i] & 0x1fff);
			WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);
		}
	}

	WRITE_VPP_REG_BY_MODE(io_mode, addr, h_num);
}

static void _set_lc_stts_mtrx(int *pcoef_data,
	int *pofst_data)
{
	int i, idx;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pcoef_data || !pofst_data)
		return;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_mtrx_coef00_01);

	idx = (LC_MTRX_SIZE + 1) >> 1;
	for (i = 0; i < idx; i++)
		WRITE_VPP_REG_BY_MODE(io_mode, addr + i, pcoef_data[i]);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_mtrx_offset0_1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[0]);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_mtrx_offset2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[1]);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_mtrx_preoffset0_1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[2]);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_mtrx_preoffset2);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[3]);
}

static void _set_sr1_lc_y2r_mtrx(int *pcoef_data,
	int *pofst_data)
{
	int i, idx;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pcoef_data || !pofst_data)
		return;

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_y2r_mtrx_0_1);

	idx = (LC_MTRX_SIZE + 1) >> 1;
	for (i = 0; i < idx; i++)
		WRITE_VPP_REG_BY_MODE(io_mode, addr + i, pcoef_data[i]);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_y2r_mtrx_ofst);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[0]);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_y2r_mtrx_clip);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[1]);
}

static void _set_sr1_lc_r2y_mtrx(int *pcoef_data,
	int *pofst_data)
{
	int i, idx;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pcoef_data || !pofst_data)
		return;

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_r2y_mtrx_0_1);

	idx = (LC_MTRX_SIZE + 1) >> 1;
	for (i = 0; i < idx; i++)
		WRITE_VPP_REG_BY_MODE(io_mode, addr + i, pcoef_data[i]);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_r2y_mtrx_ofst);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[0]);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_r2y_mtrx_clip);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, pofst_data[1]);
}

static void _set_sr1_lc_input_mux(int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(sr1_reg_cfg.page, sr1_reg_cfg.reg_lc_input_mux);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_sr1_lc_top_ctrl(int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(sr1_reg_cfg.page, sr1_reg_cfg.reg_lc_top_ctrl);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_sr1_lc_hv_num(int h_num, int v_num)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(sr1_reg_cfg.page, sr1_reg_cfg.reg_lc_hv_num);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, v_num,
		sr1_bit_cfg.bit_lc_vnum.start,
		sr1_bit_cfg.bit_lc_vnum.len);
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, h_num,
		sr1_bit_cfg.bit_lc_hnum.start,
		sr1_bit_cfg.bit_lc_hnum.len);
}

static void _set_sr1_lc_blk_bdry(unsigned int height,
	unsigned int width)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned int value, hsize, vsize;

	hsize = width / LC_BLK_NUM_MAX_H;
	vsize = height / LC_BLK_NUM_MAX_V;

	/*lc curve mapping block IDX default 4k panel*/
	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_curve_blk_hidx_0_1);

	value = vpp_mask_int(hsize, 0, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, value);
	value = vpp_mask_int(hsize * 3, 0, 14);
	value |= vpp_mask_int((hsize * 2) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 1, value);
	value = vpp_mask_int(hsize * 5, 0, 14);
	value |= vpp_mask_int((hsize * 4) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 2, value);
	value = vpp_mask_int(hsize * 7, 0, 14);
	value |= vpp_mask_int((hsize * 6) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 3, value);
	value = vpp_mask_int(hsize * 9, 0, 14);
	value |= vpp_mask_int((hsize * 8) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 4, value);
	value = vpp_mask_int(hsize * 11, 0, 14);
	value |= vpp_mask_int((hsize * 10) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 5, value);
	value = vpp_mask_int(hsize, 0, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 6, value);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_curve_blk_vidx_0_1);

	value = vpp_mask_int(vsize, 0, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, value);
	value = vpp_mask_int(vsize * 3, 0, 14);
	value |= vpp_mask_int((vsize * 2) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 1, value);
	value = vpp_mask_int(vsize * 5, 0, 14);
	value |= vpp_mask_int((vsize * 4) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 2, value);
	value = vpp_mask_int(vsize * 7, 0, 14);
	value |= vpp_mask_int((vsize * 6) << 16, 16, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 3, value);
	value = vpp_mask_int(vsize, 0, 14);
	WRITE_VPP_REG_BY_MODE(io_mode, addr + 4, value);
}

static void _set_sr1_lc_sat_lut(int *pdata)
{
	unsigned int i, tmp, tmp1, tmp2;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_sat_lut_0_1);

	for (i = 0; i < 31; i++) {
		tmp1 = *(pdata + 2 * i);
		tmp2 = *(pdata + 2 * i + 1);
		tmp = ((tmp1 & 0xfff) << 16) | (tmp2 & 0xfff);
		WRITE_VPP_REG_BY_MODE(io_mode, addr + i, tmp);
	}

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_sat_lut_62);

	tmp = (*(pdata + 62)) & 0xfff;
	WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);
}

static void _set_sr1_lc_curve(enum io_mode_e io_mode,
	int init, int *pdata)
{
	int h_num, v_num;
	unsigned int i, j, k;
	unsigned int tmp, tmp1;
	unsigned int lnr_data, lnr_data1;
	unsigned int addr = 0;

	if (!init && !pdata)
		return;

	/*initial linear data*/
	lnr_data = 0 | (0 << 10) | (512 << 20);
	lnr_data1 = 1023 | (1023 << 10) | (512 << 20);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_hv_num);
	tmp = READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr);

	h_num = (tmp >> 8) & 0x1f;
	v_num = tmp & 0x1f;

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_map_ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 1);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_map_ram_addr);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_map_ram_data);
	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num; j++) {
			if (!init) {
				switch (lc_demo_mode) {
				case 0:/*off*/
				default:
					k = 6 * (i * h_num + j);
					tmp = pdata[k + 0] |
						(pdata[k + 1] << 10) |
						(pdata[k + 2] << 20);
					tmp1 = pdata[k + 3] |
						(pdata[k + 4] << 10) |
						(pdata[k + 5] << 20);
					break;
				case 1:/*left_side*/
					if (j < h_num / 2) {
						k = 6 * (i * h_num + j);
						tmp = pdata[k + 0] |
							(pdata[k + 1] << 10) |
							(pdata[k + 2] << 20);
						tmp1 = pdata[k + 3] |
							(pdata[k + 4] << 10) |
							(pdata[k + 5] << 20);
					} else {
						tmp = lnr_data;
						tmp1 = lnr_data1;
					}
					break;
				case 2:/*right_side*/
					if (j < h_num / 2) {
						tmp = lnr_data;
						tmp1 = lnr_data1;
					} else {
						k = 6 * (i * h_num + j);
						tmp = pdata[k + 0] |
							(pdata[k + 1] << 10) |
							(pdata[k + 2] << 20);
						tmp1 = pdata[k + 3] |
							(pdata[k + 4] << 10) |
							(pdata[k + 5] << 20);
					}
					break;
				}

				WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);
				WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp1);
			} else {
				tmp = lnr_data;
				tmp1 = lnr_data1;

				WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, tmp);
				WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, tmp1);
			}
		}
	}

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_map_ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);
}

static int _get_lc_curve_ymin_lmt(int *pdata, int len)
{
	int ret = 0;
	unsigned int i, j, tmp, tmp1, tmp2;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pdata || len < 12)
		return ret;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ymin_lmt_0_1);

	for (i = 0; i < 6; i++) {
		tmp = READ_VPP_REG_BY_MODE(io_mode, addr + i);
		tmp1 = (tmp >> 16) & 0x3ff;
		tmp2 = tmp & 0x3ff;
		pdata[2 * i] = tmp1;
		pdata[2 * i + 1] = tmp2;
	}

	if (lc_lmt_type == EN_TYPE_LC_LMT_16) {
		if (len < 16)
			return ret;

		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ymin_lmt_12_13);

		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG_BY_MODE(io_mode, addr + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pdata[2 * i] = tmp1;
			pdata[2 * i + 1] = tmp2;
			i++;
		}

		ret = 16;
	} else {
		ret = 12;
	}

	return ret;
}

static int _get_lc_curve_ypkbv_ymax_lmt(int *pdata, int len)
{
	int ret = 0;
	unsigned int i, tmp, tmp1, tmp2;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pdata)
		return ret;

	if (lc_lmt_type == EN_TYPE_LC_LMT_12) {
		if (len < 12)
			return ret;

		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ypkbv_ymax_lmt_0_1);

		for (i = 0; i < 6; i++) {
			tmp = READ_VPP_REG_BY_MODE(io_mode, addr + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pdata[2 * i] = tmp1;
			pdata[2 * i + 1] = tmp2;
		}

		ret = 12;
	}

	return ret;
}

static int _get_lc_curve_ymax_lmt(int *pdata, int len)
{
	int ret = 0;
	unsigned int i, j, tmp, tmp1, tmp2;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pdata)
		return ret;

	if (lc_lmt_type == EN_TYPE_LC_LMT_16) {
		if (len < 16)
			return ret;

		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ymax_lmt_0_1);

		for (i = 0; i < 6; i++) {
			tmp = READ_VPP_REG_BY_MODE(io_mode, addr + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pdata[2 * i] = tmp1;
			pdata[2 * i + 1] = tmp2;
		}

		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ymax_lmt_12_13);

		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG_BY_MODE(io_mode, addr + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pdata[2 * i] = tmp1;
			pdata[2 * i + 1] = tmp2;
			i++;
		}

		ret = 16;
	}

	return ret;
}

static int _get_lc_curve_ypkbv_lmt(int *pdata, int len)
{
	int ret = 0;
	unsigned int i, tmp, tmp1, tmp2;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pdata)
		return ret;

	if (lc_lmt_type == EN_TYPE_LC_LMT_16) {
		if (len < 16)
			return ret;

		addr = ADDR_PARAM(lc_reg_cfg.page,
			lc_reg_cfg.reg_curve_ypkbv_lmt_0_1);

		for (i = 0; i < 8; i++) {
			tmp = READ_VPP_REG_BY_MODE(io_mode, addr + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pdata[2 * i] = tmp1;
			pdata[2 * i + 1] = tmp2;
		}

		ret = 16;
	}

	return ret;
}

static int _get_lc_curve_ypkbv_rat(int *pdata, int len)
{
	int ret = 0;
	unsigned int tmp;
	unsigned int addr = 0;

	if (!pdata || len < 4)
		return ret;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ypkbv_rat);

	tmp = READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr);
	pdata[0] = (tmp >> 24) & 0xff;
	pdata[1] = (tmp >> 16) & 0xff;
	pdata[2] = (tmp >> 8) & 0xff;
	pdata[3] = tmp & 0xff;

	ret = 4;

	return ret;
}

static int _get_lc_curve_ypkbv_slp_lmt(int *pdata, int len)
{
	int ret = 0;
	unsigned int tmp;
	unsigned int addr = 0;

	if (!pdata || len < 2)
		return ret;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ypkbv_slp_lmt);

	tmp = READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr);
	pdata[0] = (tmp >> 8) & 0xff;
	pdata[1] = tmp & 0xff;

	ret = 2;

	return ret;
}

static int _get_lc_curve_cntst_lmt_lh(int *pdata, int len)
{
	int ret = 0;
	unsigned int tmp;
	unsigned int addr = 0;

	if (!pdata || len < 4)
		return ret;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_contrast_lmt_lh);

	tmp = READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr);
	pdata[0] = (tmp >> 24) & 0xff;
	pdata[1] = (tmp >> 16) & 0xff;
	pdata[2] = (tmp >> 8) & 0xff;
	pdata[3] = tmp & 0xff;

	ret = 4;

	return ret;
}

static int _get_sr1_lc_sat_lut(int *pdata, int len)
{
	int ret = 0;
	unsigned int i, tmp, tmp1, tmp2;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pdata || len < 63)
		return ret;

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_sat_lut_0_1);

	for (i = 0; i < 31; i++) {
		tmp = READ_VPP_REG_BY_MODE(io_mode, addr + i);
		tmp1 = (tmp >> 16) & 0xfff;
		tmp2 = tmp & 0xfff;
		pdata[2 * i] = tmp1;
		pdata[2 * i + 1] = tmp2;
	}

	addr = ADDR_PARAM(sr1_reg_cfg.page,
		sr1_reg_cfg.reg_lc_sat_lut_62);

	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	pdata[62] = tmp & 0xfff;
	ret = 63;

	return ret;
}

static void _dump_lc_reg_info(void)
{
	PR_DRV("lc_reg_cfg info:\n");
	PR_DRV("page = %x\n", lc_reg_cfg.page);
	PR_DRV("reg_curve_ctrl = %x\n", lc_reg_cfg.reg_curve_ctrl);
	PR_DRV("reg_curve_hv_num = %x\n", lc_reg_cfg.reg_curve_hv_num);
	PR_DRV("reg_curve_lmt_rat = %x\n", lc_reg_cfg.reg_curve_lmt_rat);
	PR_DRV("reg_curve_contrast_lh = %x\n",
		lc_reg_cfg.reg_curve_contrast_lh);
	PR_DRV("reg_gamma_ctrl = %x\n",
		lc_reg_cfg.reg_curve_contrast_lmt_lh);

	PR_DRV("sr1_reg_cfg info:\n");
	PR_DRV("page = %x\n", sr1_reg_cfg.page);
	PR_DRV("reg_lc_input_mux = %x\n", sr1_reg_cfg.reg_lc_input_mux);
	PR_DRV("reg_lc_top_ctrl = %x\n", sr1_reg_cfg.reg_lc_top_ctrl);
	PR_DRV("reg_lc_hv_num = %x\n", sr1_reg_cfg.reg_lc_hv_num);
	PR_DRV("reg_lc_sat_lut_0_1 = %x\n", sr1_reg_cfg.reg_lc_sat_lut_0_1);
	PR_DRV("reg_lc_sat_lut_62 = %x\n", sr1_reg_cfg.reg_lc_sat_lut_62);
	PR_DRV("reg_lc_curve_blk_hidx_0_1 = %x\n",
		sr1_reg_cfg.reg_lc_curve_blk_hidx_0_1);
	PR_DRV("reg_lc_curve_blk_vidx_0_1 = %x\n",
		sr1_reg_cfg.reg_lc_curve_blk_vidx_0_1);
}

static void _dump_lc_data_info(void)
{
	int i = 0;

	PR_DRV("lc_curve_isr_defined = %d\n", lc_curve_isr_defined);
	PR_DRV("lc_curve_isr_used = %x\n", lc_curve_isr_used);
	PR_DRV("lc_demo_mode = %d\n", lc_demo_mode);
	PR_DRV("lc_support = %d\n", lc_support);
	PR_DRV("lc_bypass = %d\n", lc_bypass);
	PR_DRV("lc_malloc_ok = %d\n", lc_malloc_ok);
	PR_DRV("lc_lmt_type = %d\n", lc_lmt_type);
	PR_DRV("lc_tune_mode = %d\n", lc_tune_mode);
	PR_DRV("video_scene_change_flag_en = %d\n",
		video_scene_change_flag_en);
	PR_DRV("video_scene_change_flag = %d\n",
		video_scene_change_flag);

	if (refresh_alpha) {
		PR_DRV("refresh_alpha data info:\n");
		for (i = 0; i < LC_ALPHA_SIZE; i++) {
			PR_DRV("%d\t", refresh_alpha[i]);
			if (i % 8 == 0)
				PR_DRV("\n");
		}
	}
}

static void _lc_tune_correct(int *omap, int *ihistogram, int i, int j)
{
	int yminv_org, minbv_org, pkbv_org;
	int maxbv_org, ymaxv_org, ypkbv_org;
	int yminv, ymaxv, ypkbv;
	int k, alpha, bin_pk, idx_pk, bin_pk2nd;
	int pksum, pksum_2nd, dist;
	int amount;
	int thrd_sglbin;/*u24 maybe 80% or 90%*/
	int thrd_sglbin_thd_max;/*u24 maybe 80% or 90%*/
	int thrd_sglbin_thd_black;
	int alpha_sgl;
	int tmp_peak_val;/*pksum; bin_pk*/
	int idx_pk2nd;

	yminv_org = omap[0];
	minbv_org = omap[1];
	pkbv_org = omap[2];
	maxbv_org = omap[3];
	ymaxv_org = omap[4];
	ypkbv_org = omap[5];

	idx_pk = (pkbv_org + 32) >> 6;
	bin_pk2nd = 0;
	amount = 0;
	bin_pk = 0;
	idx_pk2nd = 0;

	for (k = 0; k < LC_HIST_BIN_COUNT; k++) {
		amount += ihistogram[k];
		if (ihistogram[k] > bin_pk) {
			bin_pk = ihistogram[k];
			idx_pk = k;
		}
	}

	pksum = (idx_pk <= 1 || idx_pk >= 14) ?
		ihistogram[idx_pk] :
		(ihistogram[idx_pk] +
		ihistogram[max(min(idx_pk - 1, 14), 1)] +
		ihistogram[max(min(idx_pk + 1, 14), 1)]);

	for (k = 0; k < LC_HIST_BIN_COUNT; k++) {
		if (k < (idx_pk - 1) || k > (idx_pk + 1)) {
			if (ihistogram[k] > bin_pk2nd) {
				bin_pk2nd = ihistogram[k];
				idx_pk2nd = k;
			}
		}
	}

	pksum_2nd = (idx_pk2nd <= 1 || idx_pk2nd >= 14) ?
		ihistogram[idx_pk2nd] :
		(ihistogram[idx_pk2nd] +
		ihistogram[max(min(idx_pk2nd - 1, 14), 1)] +
		ihistogram[max(min(idx_pk2nd + 1, 14), 1)]);

	dist = abs(idx_pk - idx_pk2nd);

	if (bin_pk2nd == 0) {
		alpha = 0;
	} else {
		if ((pksum_2nd >= (pksum * 4 / 16)) && dist > 2)
			alpha = min(dist * 8, 64);
		else
			alpha = 0;
	}

	yminv = yminv_org + (minbv_org - yminv_org) * alpha / 64;
	ymaxv = ymaxv_org + (maxbv_org - ymaxv_org) * alpha / 64;

	/*2. tune ypkBV if single peak*/
	/*int reg_lmtrat_sigbin = 922; 95% 1024*0.95 = 973 / 90% 1024*0.9 = 922*/
	/*85% 1024*0.85 = 870 / 80% 1024*0.8 = 819*/

	/*u24 maybe 80% or 90%*/
	thrd_sglbin =
		(lc_tune_curve[EN_LC_LMTRAT_SIGBIN] * amount) >> 10;
	thrd_sglbin_thd_max =
		(lc_tune_curve[EN_LC_LMTRAT_THD_MAX] * amount) >> 10;
	alpha_sgl = 0;
	tmp_peak_val = bin_pk;/*pksum; bin_pk;*/

	if (tmp_peak_val > thrd_sglbin) {
		alpha_sgl = (tmp_peak_val - thrd_sglbin) * 1024 /
			(thrd_sglbin_thd_max - thrd_sglbin);
		if (alpha_sgl > 1024)
			alpha_sgl = 1024;
	}

	ypkbv = ypkbv_org + (pkbv_org - ypkbv_org) * alpha_sgl / 1024;
	yminv = yminv_org + (minbv_org - yminv_org) * alpha_sgl / 1024;

	/*3. check black info, global checking*/
	thrd_sglbin_thd_black =
		(lc_tune_curve[EN_LC_LMTRAT_THD_BLACK] * amount) >> 10;

	if (lc_tune_curve[EN_LC_BLACK_COUNT] >= thrd_sglbin_thd_black &&
		minbv_org < lc_tune_curve[EN_LC_YMINV_BLACK_THD])
		yminv = minbv_org;

	if (lc_tune_curve[EN_LC_BLACK_COUNT] >= thrd_sglbin_thd_black &&
		pkbv_org < lc_tune_curve[EN_LC_YPKBV_BLACK_THD]) {
		ypkbv = pkbv_org;
	}

	omap[0] = yminv;
	omap[4] = ymaxv;
	omap[5] = ypkbv;
}

static void _get_lc_region_data(int blk_vnum, int blk_hnum,
	int *pcurve_data, int *phist_data)
{
	unsigned int addr[3] = {0};
	unsigned char start = 0;
	unsigned char len = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	int i, j, k, idx_curve, idx_hist;
	int data32;
	unsigned int temp1, temp2, cur_block;

	if (!pcurve_data || !phist_data)
		return;

	/*read stts black info*/
	addr[0] = ADDR_PARAM(lc_reg_cfg.page, lc_reg_cfg.reg_stts_black_info);
	start = lc_bit_cfg.bit_stts_black_cnt.start;
	len = lc_bit_cfg.bit_stts_black_cnt.len;
	temp1 = READ_VPP_REG_BY_MODE(io_mode, addr[0]);
	lc_tune_curve[EN_LC_BLACK_COUNT] = vpp_mask_int(temp1, start, len) / 96;

	/*set stts region index*/
	addr[0] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_hist_region_idx);
	start = lc_bit_cfg.bit_stts_hist_region_rd_idx.start;
	len = lc_bit_cfg.bit_stts_hist_region_rd_idx.len;
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr[0], 1, start, len);

	addr[1] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_hist_start_rd_region);
	data32 = READ_VPP_REG_BY_MODE(io_mode, addr[1]);

	/*set curve ram*/
	addr[0] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr[0], 1);

	addr[0] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ram_addr);
	WRITE_VPP_REG_BY_MODE(io_mode, addr[0], 0);

	/*read data*/
	addr[0] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ram_data);
	addr[2] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_hist_read_region);

	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			cur_block = i * blk_hnum + j;
			idx_curve = cur_block * 6;
			idx_hist = cur_block * 17;

			/*part1: get lc curve node*/
			temp1 = READ_VPP_REG_BY_MODE(io_mode, addr[0]);
			temp2 = READ_VPP_REG_BY_MODE(io_mode, addr[0]);
			pcurve_data[idx_curve + 0] = temp1 & 0x3ff;/*bit0:9*/
			pcurve_data[idx_curve + 1] = (temp1 >> 10) & 0x3ff;/*bit10:19*/
			pcurve_data[idx_curve + 2] = (temp1 >> 20) & 0x3ff;/*bit20:29*/
			pcurve_data[idx_curve + 3] = temp2 & 0x3ff;/*bit0:9*/
			pcurve_data[idx_curve + 4] = (temp2 >> 10) & 0x3ff;/*bit10:19*/
			pcurve_data[idx_curve + 5] = (temp2 >> 20) & 0x3ff;/*bit20:29*/

			/*part2: get lc hist*/
			data32 = READ_VPP_REG_BY_MODE(io_mode, addr[1]);
			for (k = 0; k < 17; k++) {
				data32 = READ_VPP_REG_BY_MODE(io_mode, addr[2]);
				phist_data[idx_hist + k] = data32;
			}

			/*part3: add tune curve node patch--by vlsi-guopan*/
			if (lc_tune_mode == EN_LC_TUNE_CORRECT) {
				_lc_tune_correct(&pcurve_data[idx_curve],
						 &phist_data[idx_hist], i, j);
			} else if (lc_tune_mode == EN_LC_TUNE_LINEAR) {
				pcurve_data[idx_curve + 0] = 0;
				pcurve_data[idx_curve + 1] = 0;
				pcurve_data[idx_curve + 2] = 512;
				pcurve_data[idx_curve + 3] = 1023;
				pcurve_data[idx_curve + 4] = 1023;
				pcurve_data[idx_curve + 5] = 512;
			}
		}
	}

	/*curve ram off*/
	addr[0] = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr[0], 0);
}

/*
 * function: detect super white and super black currently
 * return: signal range type, 0=limit, 1=full
 */
static int _lc_signal_range_detect(unsigned short *phist)
{
	int bin_blk, bin_wht;
	int range = 0;

	if (!phist)
		return range;

	bin_blk = phist[0] + phist[1];
	bin_wht = phist[62] + phist[63];

	if (bin_blk > lc_alg_cfg[EN_LC_DET_RANGE_TH_BLK] ||
		bin_wht > lc_alg_cfg[EN_LC_DET_RANGE_TH_WHT])
		range = 1;

	return range;
}

static void _lc_csc_config(enum lc_csc_type_e csc_type_in,
	enum lc_csc_type_e csc_type_out)
{
	int idx_coef, idx_ofst;

	if (csc_type_in == EN_LC_CSC_MAX ||
		csc_type_out == EN_LC_CSC_MAX)
		return;

	idx_coef = mtrx_idx_tbl[csc_type_in].csc_coef_idx;
	idx_ofst = mtrx_idx_tbl[csc_type_in].csc_ofst_idx;

	_set_sr1_lc_y2r_mtrx(&mtrx_coef_tbl[idx_coef].data[0],
		&mtrx_ofst_tbl[idx_ofst].data[0]);

	idx_coef = mtrx_idx_tbl[csc_type_out].csc_coef_idx;
	idx_ofst = mtrx_idx_tbl[csc_type_out].csc_ofst_idx;

	_set_sr1_lc_r2y_mtrx(&mtrx_coef_tbl[idx_coef].data[0],
		&mtrx_ofst_tbl[idx_ofst].data[0]);

	idx_coef = mtrx_idx_tbl[csc_type_in].stts_coef_idx;
	idx_ofst = mtrx_idx_tbl[csc_type_in].stts_ofst_idx;

	_set_lc_stts_mtrx(&mtrx_coef_tbl[idx_coef].data[0],
		&mtrx_ofst_tbl[idx_ofst].data[0]);
}

static void _lc_top_config(int h_num, int v_num,
	unsigned int height, unsigned int width)
{
	/*lcinput_ysel and _csel*/
	_set_sr1_lc_input_mux(lc_top_cfg[EN_LC_INPUT_YSEL],
			sr1_bit_cfg.bit_lc_in_ysel.start,
			sr1_bit_cfg.bit_lc_in_ysel.len);

	_set_sr1_lc_input_mux(lc_top_cfg[EN_LC_INPUT_CSEL],
		sr1_bit_cfg.bit_lc_in_csel.start,
		sr1_bit_cfg.bit_lc_in_csel.len);

	/*lc top ctrl*/
	_set_sr1_lc_top_ctrl(lc_top_cfg[EN_LC_BLKBLEND_MODE],
		sr1_bit_cfg.bit_lc_blkbld_mode.start,
		sr1_bit_cfg.bit_lc_blkbld_mode.len);

	_set_sr1_lc_top_ctrl(lc_top_cfg[EN_LC_HBLANK],
		sr1_bit_cfg.bit_lc_hblank.start,
		sr1_bit_cfg.bit_lc_hblank.len);

	_set_sr1_lc_top_ctrl(0,
		sr1_bit_cfg.bit_lc_sync_ctrl.start,
		sr1_bit_cfg.bit_lc_sync_ctrl.len);

	/*lc ram write h/v num*/
	_set_sr1_lc_hv_num(h_num, v_num);

	/*lc curve mapping config*/
	_set_sr1_lc_blk_bdry(height, width);
}

static void _lc_stts_config(int bitdepth,
	unsigned int height, unsigned int width)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned char start = 0;
	unsigned char len = 0;
	unsigned int tmp;

	_set_lc_stts_hist_region(height, width);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_gclk_ctrl0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	tmp = ((width - 1) << 16) | (height - 1);
	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_widthm1_heightm1);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, tmp);

	start = lc_bit_cfg.bit_stts_lpf_en.start;
	len = lc_bit_cfg.bit_stts_lpf_en.len;
	_set_lc_stts_hist_region_idx(1, start, len);

	start = lc_bit_cfg.bit_stts_hist_mode.start;
	len = lc_bit_cfg.bit_stts_hist_mode.len;
	_set_lc_stts_hist_region_idx(1, start, len);

	start = lc_bit_cfg.bit_stts_eol_en.start;
	len = lc_bit_cfg.bit_stts_eol_en.len;
	_set_lc_stts_hist_region_idx(0, start, len);

	start = lc_bit_cfg.bit_stts_pix_drop_mode.start;
	len = lc_bit_cfg.bit_stts_pix_drop_mode.len;
	_set_lc_stts_hist_region_idx(0, start, len);

	start = lc_bit_cfg.bit_stts_in_data_sel.start;
	len = lc_bit_cfg.bit_stts_in_data_sel.len;
	_set_lc_stts_ctrl(4, start, len);

	start = lc_bit_cfg.bit_stts_probe_en.start;
	len = lc_bit_cfg.bit_stts_probe_en.len;
	_set_lc_stts_ctrl(1, start, len);

	start = lc_bit_cfg.bit_stts_lc_max_static_en.start;
	len = lc_bit_cfg.bit_stts_lc_max_static_en.len;
	_set_lc_stts_hist_region_idx(1, start, len);

	tmp = lc_tune_curve[EN_LC_THD_BLACK];
	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_stts_black_info);
	start = lc_bit_cfg.bit_stts_black_cnt.start;
	len = lc_bit_cfg.bit_stts_black_cnt.len;
	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, tmp, start, len);
}

static void _lc_config(int bitdepth, unsigned short *phist,
	struct lc_vs_param_s *pvs_param)
{
	static unsigned int flag_full_pre;
	static unsigned int vf_height_pre, vf_width_pre;
	static unsigned int sps_w_in_pre, sps_h_in_pre;
	int h_num = lc_top_cfg[EN_LC_BLK_HNUM];
	int v_num = lc_top_cfg[EN_LC_BLK_VNUM];
	unsigned int height, width;
	unsigned int flag_csc, flag_full;
	enum lc_csc_type_e csc_type_in = EN_LC_CSC_LINEAR;
	enum lc_csc_type_e csc_type_out = EN_LC_CSC_LINEAR;

	if (!phist || !pvs_param) {
		vf_height_pre = 0;
		vf_width_pre = 0;
		return;
	}

	/*calculate csc type*/
	flag_full = 0;
	if (lc_alg_cfg[EN_LC_DET_RANGE_MODE] == 2) {
		flag_full = _lc_signal_range_detect(phist);
		if (pvs_param->vf_type & 0x20000000)
			flag_full = 1; /*VIDTYPE_RGB_444*/
	} else {
		flag_full = lc_alg_cfg[EN_LC_DET_RANGE_MODE];
	}

	if (vf_height_pre == pvs_param->vf_height &&
		vf_width_pre == pvs_param->vf_width &&
		sps_w_in_pre == pvs_param->sps_w_in &&
		sps_h_in_pre == pvs_param->sps_h_in &&
		flag_full_pre == flag_full)
		return;

	vf_height_pre = pvs_param->vf_height;
	vf_width_pre = pvs_param->vf_width;
	sps_w_in_pre = pvs_param->sps_w_in;
	sps_h_in_pre = pvs_param->sps_h_in;
	flag_full_pre = flag_full;

	/*bit 29: present_flag*/
	/*bit 23-16: color_primaries*/
	/*"unknown", "bt709", "undef", "bt601",*/
	/*"bt470m", "bt470bg", "smpte170m", "smpte240m",*/
	/*"film", "bt2020"*/
	if (pvs_param->vf_signal_type & (1 << 29))
		flag_csc = (pvs_param->vf_signal_type >> 16) & 0xff;
	else
		/*signal_type is not present*/
		/*use default value bt709*/
		flag_csc = 0x1;

	if (flag_csc == 0x3) {
		csc_type_in = EN_LC_CSC_YUV601L_RGB;
		csc_type_out = EN_LC_CSC_RGB_YUV601L;
	} else {
		/*for special signal, keep full range to avoid clipping*/
		if (flag_full == 1) {
			csc_type_in = EN_LC_CSC_YUV709_RGB;
			csc_type_out = EN_LC_CSC_RGB_YUV709;
		} else {
			csc_type_in = EN_LC_CSC_YUV709L_RGB;
			csc_type_out = EN_LC_CSC_RGB_YUV709L;
		}
	}

	_lc_csc_config(csc_type_in, csc_type_out);

	height = pvs_param->sps_h_in << pvs_param->sps_h_en;
	width = pvs_param->sps_w_in << pvs_param->sps_v_en;

	_lc_top_config(h_num, v_num, height, width);

	height = pvs_param->sps_h_in;
	width = pvs_param->sps_w_in;

	_set_lc_curve_config(h_num, v_num, height, width);
	_lc_stts_config(bitdepth, height, width);
}

/*
 * function: detect osd and provide osd signal status
 * output:
 * osd_flag_cnt: signal indicate osd exist (process)
 * frm_cnt: signal that record the sudden
 * moment osd appear or disappear
 */
static void _lc_osd_det_func(int *posd_flag_cnt,
	int *pfrm_cnt, int blk_hist_sum, int *plc_hist,
	int blk_hnum, int osd_vnum_strt, int osd_vnum_end)
{
	static int osd_hist_cnt;
	int i, j, tmp;
	int min_bin_value, min_bin_percent;
	int osd_flag;
	int valid_blk;
	int percent_norm = (1 << 10) - 1;

	if (!posd_flag_cnt ||
		!pfrm_cnt ||
		!plc_hist)
		return;

	osd_flag = 0;
	min_bin_value = 0;
	osd_hist_cnt = 0;

	/*two line*/
	for (i = osd_vnum_strt; i <= osd_vnum_end; i++) {
		for (j = 0; j < blk_hnum; j++) {
			/*first bin*/
			tmp = (i * blk_hnum + j) * (LC_HIST_BIN_COUNT + 1);
			min_bin_value = plc_hist[tmp];

			/*black osd means first bin value very large*/
			tmp = min_bin_value * percent_norm;
			min_bin_percent = tmp / (blk_hist_sum + 1);

			/*black bin count*/
			if (min_bin_percent > lc_alg_cfg[EN_LC_MIN_BV_PERCENT_TH])
				osd_hist_cnt++;
		}
	}

	/*how many block osd occupy*/
	tmp = (osd_vnum_end - osd_vnum_strt + 1) * blk_hnum;
	valid_blk = tmp - lc_alg_cfg[EN_LC_INVALID_BLK];

	/*we suppose when osd appear,
	 * 1)it came in certain area,
	 * 2)and almost all those area are black
	 */
	if (osd_hist_cnt > valid_blk)
		posd_flag_cnt[1] = 1;
	else
		posd_flag_cnt[1] = 0;

	/*detect the moment osd appear and disappear:*/
	osd_flag = XOR(posd_flag_cnt[0], posd_flag_cnt[1]);

	/*set osd appear and disappear heavy iir time*/
	if (osd_flag && lc_alg_cfg[EN_LC_OSD_IIR_EN])
		*pfrm_cnt = 60 * lc_alg_cfg[EN_LC_TS];
}

/*
 * note 1: osd appear in 5,6(0,1,2,...7) line in Vnum = 8 situation
 * if Vnum changed, debug osd location,
 * find vnum_start_below, vnum_end_below
 * (osd below situation,above situation is same)
 * note 2: here just consider 2 situation,
 * osd appear below or appear above
 * function: 2 situation osd detect and provide
 * osd status signal(osd_flag_cnt & frm_cnt)
 */
static void _lc_osd_detect(int *posd_flag_cnt_above,
	int *posd_flag_cnt_below, int *pfrm_cnt_above,
	int *pfrm_cnt_below, int *plc_hist, int blk_hnum)
{
	int i, tmp;
	unsigned long blk_hist_sum = 0;

	if (!posd_flag_cnt_above ||
		!posd_flag_cnt_below ||
		!pfrm_cnt_above ||
		!pfrm_cnt_below ||
		!plc_hist)
		return;

	/*use blk[0,0], 16bin*/
	for (i = 0; i < LC_HIST_BIN_COUNT; i++) {
		tmp = (0 * blk_hnum + 0) * (LC_HIST_BIN_COUNT + 1) + i;
		blk_hist_sum += plc_hist[tmp];
	}

	/*above situation*/
	_lc_osd_det_func(posd_flag_cnt_above, pfrm_cnt_above,
		blk_hist_sum, plc_hist, blk_hnum,
		lc_alg_cfg[EN_LC_VNUM_START_ABOVE],
		lc_alg_cfg[EN_LC_VNUM_END_ABOVE]);

	/*below situation*/
	_lc_osd_det_func(posd_flag_cnt_below, pfrm_cnt_below,
		blk_hist_sum, plc_hist, blk_hnum,
		lc_alg_cfg[EN_LC_VNUM_START_BELOW],
		lc_alg_cfg[EN_LC_VNUM_END_BELOW]);
}

/*
 * function: detect global scene change signal
 * output: scene_change_flag
 */
static int _lc_global_scene_change(int *pcurve_nodes_cur,
	int *pcurve_nodes_pre, int blk_vnum, int blk_hnum,
	int *posd_flag_cnt_above, int *posd_flag_cnt_below)
{
	static int scene_dif[LC_N];
	static int frm_dif[LC_N];
	/*store frame valid block for frm diff calc*/
	static int valid_blk_num[LC_N];
	int scene_change_flag;
	int frm_dif_osd, vnum_osd;
	int addr_curv1;
	int apl_cur, apl_pre;
	int i, j, tmp;

	if (!pcurve_nodes_cur ||
		!pcurve_nodes_pre ||
		!posd_flag_cnt_above ||
		!posd_flag_cnt_below)
		return 0;

	/*history message delay*/
	for (i = 0; i < (LC_N - 1); i++) {
		scene_dif[i] = scene_dif[i + 1];
		frm_dif[i] = frm_dif[i + 1];
		valid_blk_num[i] = valid_blk_num[i + 1];
	}

	if (video_scene_change_flag_en) {
		/*2.1 flag from front module*/
		scene_change_flag = video_scene_change_flag;
	} else {
		/* 2.2.1 use block APL to calculate frame dif:*/
		/* omap[5]: (yminV), minBV, pkBV, maxBV, (ymaxV),ypkBV*/
		frm_dif[LC_N - 1] = 0;
		scene_dif[LC_N - 1] = 0;
		valid_blk_num[LC_N - 1] = 0;
		frm_dif_osd = 0;
		vnum_osd = 0;

		for (i = 0; i < blk_vnum; i++) {
			for (j = 0; j < blk_hnum; j++) {
				addr_curv1 = i * blk_hnum + j;
				/*apl value*/
				tmp = addr_curv1 * LC_CURVE_NODES + 2;
				apl_cur = pcurve_nodes_cur[tmp];
				apl_pre = pcurve_nodes_pre[tmp];
				/*frame motion*/
				frm_dif[LC_N - 1] += abs(apl_cur - apl_pre);

				/*when have osd, remove them to calc frame motion*/
				if (posd_flag_cnt_below[1] &&
					i >= lc_alg_cfg[EN_LC_VNUM_START_BELOW] &&
					i <= lc_alg_cfg[EN_LC_VNUM_END_BELOW]) {
					frm_dif_osd += abs(apl_cur - apl_pre);
					vnum_osd =
						lc_alg_cfg[EN_LC_VNUM_END_BELOW] -
						lc_alg_cfg[EN_LC_VNUM_START_BELOW] + 1;
				}

				if (posd_flag_cnt_above[1] &&
					i >= lc_alg_cfg[EN_LC_VNUM_START_ABOVE] &&
					i <= lc_alg_cfg[EN_LC_VNUM_END_ABOVE]) {
					frm_dif_osd += abs(apl_cur - apl_pre);
					vnum_osd =
						lc_alg_cfg[EN_LC_VNUM_END_ABOVE] -
						lc_alg_cfg[EN_LC_VNUM_START_ABOVE] + 1;
				}
			}
		}

		/*remove osd to calc frame motion */
		frm_dif[LC_N - 1] = frm_dif[LC_N - 1] - frm_dif_osd;
		valid_blk_num[LC_N - 1] = (blk_vnum - vnum_osd) * blk_hnum;

		/*2.2.2 motion dif.if motion dif too large,*/
		/*we think scene changed*/
		scene_dif[LC_N - 1] =
			abs((frm_dif[LC_N - 1] / (valid_blk_num[LC_N - 1] + 1)) -
			(frm_dif[LC_N - 2] / (valid_blk_num[LC_N - 2] + 1)));

		if (scene_dif[LC_N - 1] > lc_alg_cfg[EN_LC_SCENE_CHANGE_TH])
			scene_change_flag = 1;
		else
			scene_change_flag = 0;
	}

	return scene_change_flag;
}

/*
 * function: set tiir alpha based on different situation
 * input: scene_change_flag, frm_cnt(frm_cnt_above,frm_cnt_below)
 * output: refresh_alpha[96]
 */
static int _lc_cal_iir_alpha(int *prefresh_alpha,
	int blk_vnum, int blk_hnum, int refresh,
	int scene_change_flag,
	int frm_cnt_above, int frm_cnt_below)
{
	int i, j, val;
	int addr_curv1;
	int osd_local_p, osd_local_m;
	int alpha1 = lc_alg_cfg[EN_LC_ALPHA1];
	int alpha2 = lc_alg_cfg[EN_LC_ALPHA2];

	if (!prefresh_alpha)
		return 0;

	if (frm_cnt_above > 0) {
		/*3.2 osd situation-2, osd above*/
		osd_local_p = max((lc_alg_cfg[EN_LC_VNUM_START_ABOVE] - 1), 0);
		osd_local_m = min((lc_alg_cfg[EN_LC_VNUM_END_ABOVE] + 1), blk_vnum);
		/*osd around use heavy iir*/
		val = alpha2;
	} else if (frm_cnt_below > 0) {
		/*3.2 osd situation-1, osd below*/
		osd_local_p = max((lc_alg_cfg[EN_LC_VNUM_START_BELOW] - 1), 0);
		osd_local_m = min((lc_alg_cfg[EN_LC_VNUM_END_BELOW] + 1), blk_vnum);
		/*osd around use heavy iir*/
		val = alpha2;
	} else {
		/*3.1 global scene change, highest priority*/
		osd_local_p = 0;
		osd_local_m = blk_vnum;
		if (scene_change_flag)
			val = refresh;/*only use current curve*/
		else
			val = alpha1;/*normal iir*/
	}

	for (i = osd_local_p; i <= osd_local_m; i++) {
		for (j = 0; j < blk_hnum; j++) {
			addr_curv1 = i * blk_hnum + j;
			prefresh_alpha[addr_curv1] = val;
		}
	}

	return 0;
}

/*
 * function: curve iir process
 * output: curve_nodes_cur(after iir)
 */
static int _lc_cal_curv_iir(int *pcurve_nodes_cur,
	int *pcurve_nodes_pre, int *prefresh_alpha,
	int blk_vnum, int blk_hnum, int refresh)
{
	int i, j, k;
	int node_cur;
	int addr_curv1, addr_curv2;
	s64 tmap[LC_CURVE_NODES], node_pre_raw;
	int refresh_bit = lc_alg_cfg[EN_LC_REFRESH_BIT];

	if (!pcurve_nodes_cur ||
		!pcurve_nodes_pre ||
		!prefresh_alpha)
		return 0;

	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			addr_curv1 = i * blk_hnum + j;
			for (k = 0; k < LC_CURVE_NODES; k++) {
				addr_curv2 = addr_curv1 * LC_CURVE_NODES + k;
				/*u12 calc*/
				node_cur = pcurve_nodes_cur[addr_curv2] << 2;
				node_pre_raw = curve_nodes_pre_raw[addr_curv2];
				tmap[k] =
					(node_cur * prefresh_alpha[addr_curv1])
					+ ((node_pre_raw *
					(refresh - prefresh_alpha[addr_curv1]))
					>> refresh_bit);
				curve_nodes_pre_raw[addr_curv2] = tmap[k];
				tmap[k] = (tmap[k] + (1 << (1 + refresh_bit)))
					>> (2 + refresh_bit);
				/*output the iir result, back to u10*/
				pcurve_nodes_cur[addr_curv2] = (int)(tmap[k]);
				/*delay for next iir*/
				pcurve_nodes_pre[addr_curv2] = (int)(tmap[k]);
			}
		}
	}

	return 0;
}

/*
 * function: iir algorithm top level path
 * attention !
 * note 1:
 * video_scene_change_flag_en
 * video_scene_change_flag
 * should from front module, indicate scene changed.
 *
 * note 2: OSD appear location
 * vnum_start_below
 * vnum_end_below (case1)
 * vnum_start_above
 * vnum_end_above (case2)
 */
static void _lc_curve_iir_process(int *plc_hist,
	int *plc_szcurve, int blk_vnum, int blk_hnum)
{
	/*osd detect*/
	static int frm_cnt_below, frm_cnt_above;
	static int osd_flag_cnt_below[2];
	static int osd_flag_cnt_above[2];

	int refresh = 1 << lc_alg_cfg[EN_LC_REFRESH_BIT];
	int scene_change_flag;
	int i;

	if (!plc_szcurve)
		return;

	if (!lc_curve_fresh)
		goto curve_fresh;

	/*pre: get curve nodes from szCurveInfo*/
	/*and save to curve_nodes_cur*/
	for (i = 0; i < LC_CURVE_SIZE; i++)
		curve_nodes_cur[i] = plc_szcurve[i];

	/*pre: osd flag delay*/
	osd_flag_cnt_below[0] = osd_flag_cnt_below[1];
	osd_flag_cnt_above[0] = osd_flag_cnt_above[1];

	/*step 1: osd detect*/
	_lc_osd_detect(osd_flag_cnt_above,/*out*/
		osd_flag_cnt_below,/*out*/
		&frm_cnt_above,/*out*/
		&frm_cnt_below,/*out: osd case heavy iir time*/
		plc_hist, blk_hnum);

	/*step 2: scene change signal get: two method*/
	scene_change_flag = _lc_global_scene_change(curve_nodes_cur,
		curve_nodes_pre, blk_vnum, blk_hnum,
		osd_flag_cnt_above, osd_flag_cnt_below);
	if (scene_change_flag) {
		memset(osd_flag_cnt_below, 0, sizeof(int) * 2);
		memset(osd_flag_cnt_above, 0, sizeof(int) * 2);
		frm_cnt_below = 0;
		frm_cnt_above = 0;
	}

	/*step 3: set tiir alpha based on different situation*/
	memset(refresh_alpha, 0, LC_ALPHA_SIZE * sizeof(int));
	_lc_cal_iir_alpha(refresh_alpha,/*out*/
		blk_vnum, blk_hnum, refresh,
		scene_change_flag, frm_cnt_above, frm_cnt_below);

	/*step 4: iir filter*/
	_lc_cal_curv_iir(curve_nodes_cur,/*out: iir-ed curve*/
		curve_nodes_pre,/*out: store previous frame curve*/
		refresh_alpha, blk_vnum, blk_hnum, refresh);

	frm_cnt_below--;
	if (frm_cnt_below < 0)
		frm_cnt_below = 0;

	frm_cnt_above--;
	if (frm_cnt_above < 0)
		frm_cnt_above = 0;

curve_fresh:
	for (i = 0; i < LC_CURVE_SIZE; i++)
		plc_szcurve[i] = curve_nodes_cur[i];
}

static void _lc_buff_init(void)
{
	lc_malloc_ok = false;

	lc_hist_data = kcalloc(LC_HIST_SIZE, sizeof(int), GFP_KERNEL);
	if (!lc_hist_data)
		return;

	lc_wrcurve = kcalloc(LC_CURVE_SIZE, sizeof(int), GFP_KERNEL);
	if (!lc_wrcurve) {
		kfree(lc_hist_data);
		return;
	}

	curve_nodes_cur = kcalloc(LC_CURVE_SIZE, sizeof(int), GFP_KERNEL);
	if (!curve_nodes_cur) {
		kfree(lc_hist_data);
		kfree(lc_wrcurve);
		return;
	}

	curve_nodes_pre = kcalloc(LC_CURVE_SIZE, sizeof(int), GFP_KERNEL);
	if (!curve_nodes_pre) {
		kfree(lc_hist_data);
		kfree(lc_wrcurve);
		kfree(curve_nodes_cur);
		return;
	}

	curve_nodes_pre_raw = kcalloc(LC_CURVE_SIZE,
		sizeof(int64_t), GFP_KERNEL);
	if (!curve_nodes_pre_raw) {
		kfree(lc_hist_data);
		kfree(lc_wrcurve);
		kfree(curve_nodes_cur);
		kfree(curve_nodes_pre);
		return;
	}

	refresh_alpha = kcalloc(LC_ALPHA_SIZE, sizeof(int), GFP_KERNEL);
	if (!refresh_alpha) {
		kfree(lc_hist_data);
		kfree(lc_wrcurve);
		kfree(curve_nodes_cur);
		kfree(curve_nodes_pre);
		kfree(curve_nodes_pre_raw);
		return;
	}

	lc_malloc_ok = true;

	memset(lc_hist_data, 0, LC_HIST_SIZE * sizeof(int));
	memset(lc_wrcurve, 0, LC_CURVE_SIZE * sizeof(int));
	memset(curve_nodes_cur, 0, LC_CURVE_SIZE * sizeof(int));
	memset(curve_nodes_pre, 0, LC_CURVE_SIZE * sizeof(int));
	memset(curve_nodes_pre_raw, 0, LC_CURVE_SIZE * sizeof(int64_t));
	memset(refresh_alpha, 0, LC_ALPHA_SIZE * sizeof(int));
}

static void _lc_buff_free(void)
{
	kfree(lc_hist_data);
	kfree(lc_wrcurve);
	kfree(curve_nodes_cur);
	kfree(curve_nodes_pre);
	kfree(curve_nodes_pre_raw);
	kfree(refresh_alpha);

	lc_malloc_ok = false;
}

static void _lc_mtrx_idx_init(enum vpp_chip_type_e chip_id)
{
	mtrx_idx_tbl[EN_LC_CSC_LINEAR].csc_coef_idx = 0;
	mtrx_idx_tbl[EN_LC_CSC_LINEAR].csc_ofst_idx = 0;
	mtrx_idx_tbl[EN_LC_CSC_LINEAR].stts_coef_idx = 0;
	mtrx_idx_tbl[EN_LC_CSC_LINEAR].stts_ofst_idx = 0;

	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV601L].csc_coef_idx = 1;
	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV601L].stts_coef_idx = 4;
	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV601L].stts_ofst_idx = 5;

	mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].stts_coef_idx = 5;
	mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].stts_ofst_idx = 6;

	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709L].csc_coef_idx = 4;
	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709L].stts_coef_idx = 4;
	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709L].stts_ofst_idx = 5;

	mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].stts_coef_idx = 5;
	mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].stts_ofst_idx = 6;

	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709].csc_coef_idx = 7;
	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709].stts_coef_idx = 4;
	mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709].stts_ofst_idx = 5;

	mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].stts_coef_idx = 8;
	mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].stts_ofst_idx = 7;

	if (lc_alg_cfg[EN_LC_BITDEPTH] == 10) {
		if (chip_id == CHIP_T5 ||
			chip_id == CHIP_T5D ||
			chip_id == CHIP_T3) {
			mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].csc_coef_idx = 2;
			mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].csc_coef_idx = 5;
			mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].csc_coef_idx = 8;
		} else {
			mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].csc_coef_idx = 3;
			mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].csc_coef_idx = 6;
			mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].csc_coef_idx = 9;
		}

		mtrx_idx_tbl[EN_LC_CSC_RGB_YUV601L].csc_ofst_idx = 1;
		mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].csc_ofst_idx = 1;
		mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709L].csc_ofst_idx = 1;
		mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].csc_ofst_idx = 1;
		mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709].csc_ofst_idx = 3;
		mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].csc_ofst_idx = 3;
	} else {
		if (chip_id == CHIP_TM2) {
			mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].csc_coef_idx = 3;
			mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].csc_coef_idx = 6;
			mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].csc_coef_idx = 9;
		} else {
			mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].csc_coef_idx = 2;
			mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].csc_coef_idx = 5;
			mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].csc_coef_idx = 8;
		}

		mtrx_idx_tbl[EN_LC_CSC_RGB_YUV601L].csc_ofst_idx = 2;
		mtrx_idx_tbl[EN_LC_CSC_YUV601L_RGB].csc_ofst_idx = 2;
		mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709L].csc_ofst_idx = 2;
		mtrx_idx_tbl[EN_LC_CSC_YUV709L_RGB].csc_ofst_idx = 2;
		mtrx_idx_tbl[EN_LC_CSC_RGB_YUV709].csc_ofst_idx = 4;
		mtrx_idx_tbl[EN_LC_CSC_YUV709_RGB].csc_ofst_idx = 4;
	}
}

static void _lc_config_init(void)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ram_ctrl);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	/*default lc low parameters*/
	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_contrast_lh);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x000b000b);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_contrast_scl_lh);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x00000b0b);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_misc0);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x00023028);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ypkbv_rat);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x8cc0c060);

	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ypkbv_slp_lmt);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0x00000b3a);

	_set_lc_curve_ymin_lmt(&lmt_lut_tbl[0].data[0]);
	_set_lc_curve_ymax_lmt(&lmt_lut_tbl[1].data[0]);
	_set_lc_curve_ypkbv_lmt(&lmt_lut_tbl[1].data[0]);
	_set_lc_curve_ypkbv_ymax_lmt(&lmt_lut_tbl[1].data[0]);

	/*sr1 lc saturation gain, off parameters*/
	_set_sr1_lc_sat_lut(&sat_lut_tbl[0].data[0]);
	/*sr1 lc curve linear parameters*/
	_set_sr1_lc_curve(EN_MODE_DIR, 1, NULL);
}

static void _lc_tune_curve_init(void)
{
	lc_tune_curve[EN_LC_LMTRAT_SIGBIN] = 1024;
	lc_tune_curve[EN_LC_LMTRAT_THD_MAX] = 950;
	lc_tune_curve[EN_LC_LMTRAT_THD_BLACK] = 200;
	lc_tune_curve[EN_LC_THD_BLACK] = 0x20;
	lc_tune_curve[EN_LC_YMINV_BLACK_THD] = 500;
	lc_tune_curve[EN_LC_YPKBV_BLACK_THD] = 256;
	lc_tune_curve[EN_LC_BLACK_COUNT] = 0;
}

/*External functions*/
int vpp_module_lc_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;

	video_scene_change_flag_en = 0;
	video_scene_change_flag = 0;

	lc_curve_isr_defined = 0;
	lc_curve_isr_used = 1;

	lc_demo_mode = 0;
	lc_bypass = false;
	lc_curve_fresh = true;

	chip_id = pdev->pm_data->chip_id;

	if (chip_id < CHIP_TM2)
		lc_lmt_type = EN_TYPE_LC_LMT_12;
	else
		lc_lmt_type = EN_TYPE_LC_LMT_16;

	if (chip_id < CHIP_TL1)
		lc_support = false;
	else
		lc_support = true;

	_lc_mtrx_idx_init(chip_id);
	_lc_buff_init();
	_lc_tune_curve_init();
	_lc_config_init();

	return 0;
}

void vpp_module_lc_deinit(void)
{
	_lc_buff_free();
}

int vpp_module_lc_en(bool enable)
{
	unsigned int addr = 0;

	pr_vpp(PR_DEBUG_LC, "[%s] enable = %d\n", __func__, enable);

	_set_sr1_lc_top_ctrl(enable,
		sr1_bit_cfg.bit_lc_en.start,
		sr1_bit_cfg.bit_lc_en.len);

	_set_lc_curve_ctrl(enable,
		lc_bit_cfg.bit_curve_en.start,
		lc_bit_cfg.bit_curve_en.len);

	_set_lc_stts_ctrl(enable,
		lc_bit_cfg.bit_stts_mtrx_en.start,
		lc_bit_cfg.bit_stts_mtrx_en.len);

	/*set curve ram*/
	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_ram_ctrl);

	WRITE_VPP_REG_BY_MODE(EN_MODE_DIR, addr, 0);

	/*set local contrast max statistic enable*/
	_set_lc_stts_hist_region_idx(enable,
		lc_bit_cfg.bit_stts_lc_max_static_en.start,
		lc_bit_cfg.bit_stts_lc_max_static_en.len);

	if (enable) {
		lc_bypass = false;

		_set_lc_curve_ctrl(1,
			lc_bit_cfg.bit_curve_interrupt_en.start,
			lc_bit_cfg.bit_curve_interrupt_en.len);
	} else {
		lc_bypass = true;

		if (lc_malloc_ok) {
			memset(lc_hist_data, 0, LC_HIST_SIZE * sizeof(int));
			memset(lc_wrcurve, 0, LC_CURVE_SIZE * sizeof(int));
			memset(curve_nodes_cur, 0, LC_CURVE_SIZE * sizeof(int));
			memset(curve_nodes_pre, 0, LC_CURVE_SIZE * sizeof(int));
			memset(curve_nodes_pre_raw, 0, LC_CURVE_SIZE * sizeof(int64_t));
			memset(refresh_alpha, 0, LC_ALPHA_SIZE * sizeof(int));
		}
	}

	return 0;
}

void vpp_module_lc_write_lut(enum lc_lut_type_e type, int *pdata)
{
	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_LC, "[%s] type = %d\n", __func__, type);

	switch (type) {
	case EN_LC_SAT:
		_set_sr1_lc_sat_lut(pdata);
		break;
	case EN_LC_YMIN_LMT:
		_set_lc_curve_ymin_lmt(pdata);
		break;
	case EN_LC_YPKBV_YMAX_LMT:
		_set_lc_curve_ypkbv_ymax_lmt(pdata);
		break;
	case EN_LC_YMAX_LMT:
		_set_lc_curve_ymax_lmt(pdata);
		break;
	case EN_LC_YPKBV_LMT:
		_set_lc_curve_ypkbv_lmt(pdata);
		break;
	case EN_LC_YPKBV_RAT:
		_set_lc_curve_ypkbv_rat(pdata);
		break;
	case EN_LC_YPKBV_SLP_LMT:
		_set_lc_curve_ypkbv_slp_lmt(pdata);
		break;
	case EN_LC_CNTST_LMT:
		_set_lc_curve_cntst_lmt_lh(pdata);
		break;
	default:
		break;
	}
}

void vpp_module_lc_set_demo_mode(bool enable, bool left_side)
{
	pr_vpp(PR_DEBUG_LC, "[%s] enable = %d, left_side = %d\n",
		__func__, enable, left_side);

	if (enable)
		if (left_side)
			lc_demo_mode = 1;
		else
			lc_demo_mode = 2;
	else
		lc_demo_mode = 0;
}

void vpp_module_lc_set_tune_mode(enum lc_curve_tune_mode_e mode)
{
	lc_tune_mode = mode;
}

void vpp_module_lc_set_tune_param(enum lc_curve_tune_param_e index,
	int val)
{
	if (index == EN_LC_CURVE_TUNE_PARAM_MAX)
		return;

	pr_vpp(PR_DEBUG_LC, "[%s] index = %d, val = %d\n",
		__func__, index, val);

	lc_tune_curve[index] = val;
}

void vpp_module_lc_set_cfg_param(enum lc_config_param_e index,
	int val)
{
	if (index == EN_LC_CFG_PARAM_MAX)
		return;

	pr_vpp(PR_DEBUG_LC, "[%s] index = %d, val = %d\n",
		__func__, index, val);

	lc_top_cfg[index] = val;
}

void vpp_module_lc_set_alg_param(enum lc_algorithm_param_e index,
	int val)
{
	if (index == EN_LC_ALG_PARAM_MAX)
		return;

	pr_vpp(PR_DEBUG_LC, "[%s] index = %d, val = %d\n",
		__func__, index, val);

	lc_alg_cfg[index] = val;
}

void vpp_module_lc_set_isr_def(int val)
{
	lc_curve_isr_defined = val;
}

void vpp_module_lc_set_isr_use(int val)
{
	lc_curve_isr_used = val;
}

void vpp_module_lc_set_isr(void)
{
	if (!lc_support || !lc_malloc_ok ||
		!lc_curve_isr_used || !lc_curve_isr_defined)
		return;

	_get_lc_region_data(LC_BLK_NUM_MAX_V, LC_BLK_NUM_MAX_H,
		lc_wrcurve, lc_hist_data);
}

bool vpp_module_lc_get_support(void)
{
	return lc_support;
}

int vpp_module_lc_read_lut(enum lc_lut_type_e type, int *pdata, int len)
{
	int ret = 0;

	if (!pdata)
		return ret;

	switch (type) {
	case EN_LC_SAT:
		ret = _get_sr1_lc_sat_lut(pdata, len);
		break;
	case EN_LC_YMIN_LMT:
		ret = _get_lc_curve_ymin_lmt(pdata, len);
		break;
	case EN_LC_YPKBV_YMAX_LMT:
		ret = _get_lc_curve_ypkbv_ymax_lmt(pdata, len);
		break;
	case EN_LC_YMAX_LMT:
		ret = _get_lc_curve_ymax_lmt(pdata, len);
		break;
	case EN_LC_YPKBV_LMT:
		ret = _get_lc_curve_ypkbv_lmt(pdata, len);
		break;
	case EN_LC_YPKBV_RAT:
		ret = _get_lc_curve_ypkbv_rat(pdata, len);
		break;
	case EN_LC_YPKBV_SLP_LMT:
		ret = _get_lc_curve_ypkbv_slp_lmt(pdata, len);
		break;
	case EN_LC_CNTST_LMT:
		ret = _get_lc_curve_cntst_lmt_lh(pdata, len);
		break;
	default:
		break;
	}

	return ret;
}

int vpp_module_lc_get_tune_param(enum lc_curve_tune_param_e index)
{
	if (index == EN_LC_CURVE_TUNE_PARAM_MAX)
		return 0;

	return lc_tune_curve[index];
}

int vpp_module_lc_get_cfg_param(enum lc_config_param_e index)
{
	if (index == EN_LC_CFG_PARAM_MAX)
		return 0;

	return lc_top_cfg[index];
}

int vpp_module_lc_get_alg_param(enum lc_algorithm_param_e index)
{
	if (index == EN_LC_ALG_PARAM_MAX)
		return 0;

	return lc_alg_cfg[index];
}

void vpp_module_lc_on_vs(unsigned short *pdata_hist,
	struct lc_vs_param_s *pvs_param)
{
	int blk_hnum, blk_vnum, tmp;
	unsigned int addr = 0;

	if (!lc_support || lc_bypass || !lc_malloc_ok ||
		!pdata_hist || !pvs_param)
		return;

	memcpy(&lc_vs_param, pvs_param, sizeof(struct lc_vs_param_s));

	_lc_config(lc_alg_cfg[EN_LC_BITDEPTH], pdata_hist, pvs_param);

	/*get hist & curve node*/
	addr = ADDR_PARAM(lc_reg_cfg.page,
		lc_reg_cfg.reg_curve_hv_num);
	tmp = READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr);
	blk_hnum = (tmp >> 8) & 0x1f;
	blk_vnum = tmp & 0x1f;

	if (!lc_curve_isr_used || !lc_curve_isr_defined)
		_get_lc_region_data(blk_vnum, blk_hnum, lc_wrcurve, lc_hist_data);

	/*do time domain iir*/
	_lc_curve_iir_process(lc_hist_data, lc_wrcurve, blk_vnum, blk_hnum);

	/*set curve data*/
	_set_sr1_lc_curve(EN_MODE_RDMA, 0, lc_wrcurve);
}

void vpp_module_lc_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_lc_reg_info();
	else
		_dump_lc_data_info();
}

