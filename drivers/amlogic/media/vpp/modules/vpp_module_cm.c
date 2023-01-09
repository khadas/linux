// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_cm.h"

#define CM2_DATA_WR_COUNT (5)

enum _cm2_curve_type_e {
	EN_CM2_CURVE_LUMA = 0,
	EN_CM2_CURVE_SAT,
	EN_CM2_CURVE_SAT_Y,
	EN_CM2_CURVE_HUE,
	EN_CM2_CURVE_HUE_S,
	EN_CM2_CURVE_HUE_Y,
};

struct _chroma_reg_cfg_s {
	unsigned char page;
	unsigned char reg_misc;
	unsigned char reg_addr_port;
	unsigned char reg_data_port;
};

struct _cm_addr_cfg_s {
	unsigned int addr_sat_byyb_node0;
	unsigned int addr_sat_byyb_node1;
	unsigned int addr_sat_byyb_node2;
	unsigned int addr_sat_src_node;
	unsigned int addr_cm_enh_sft_mode;
	unsigned int addr_frm_size;
	unsigned int addr_filter_cfg;
	unsigned int addr_cm_global_gain;
	unsigned int addr_cm_enh_ctl;
	unsigned int addr_roi_x_scope;
	unsigned int addr_roi_y_scope;
	unsigned int addr_ifo_mode;
	unsigned int addr_luma_adj_lmt;
	unsigned int addr_sat_adj_lmt;
	unsigned int addr_hue_adj_lmt;
	unsigned int addr_hue_cfg;
	unsigned int addr_luma_adj1;
	unsigned int addr_sta_cfg;
	unsigned int addr_sta_sat_hist0;
	unsigned int addr_sta_sat_hist1;
	unsigned int addr_cm2_enh_coef0_h00;
	unsigned int addr_cm_final_gain_luma;
	unsigned int addr_cm_final_gain_sat;
	unsigned int addr_cm_final_gain_hue;
};

struct _cm_bit_cfg_s {
	struct _bit_s bit_misc_cm_en;
	struct _bit_s bit_frm_width;
	struct _bit_s bit_frm_height;
	struct _bit_s bit_cm_glb_gain_hue;
	struct _bit_s bit_cm_glb_gain_sat;
	struct _bit_s bit_cm1_en;
	struct _bit_s bit_cm2_filter_en;
	struct _bit_s bit_cm2_h_adj_en;
	struct _bit_s bit_cm2_s_adj_en;
	struct _bit_s bit_cm2_l_adj_en;
	struct _bit_s bit_cm_bypass;
	struct _bit_s bit_roi_x_end;
	struct _bit_s bit_roi_y_end;
	struct _bit_s bit_sta_sat_hist_mode;
	struct _bit_s bit_sta_hue_hist_sat_th;
	struct _bit_s bit_sta_en;
	struct _bit_s bit_cm_ro_frame;
};

/*Default table from T3*/
static struct _chroma_reg_cfg_s chroma_reg_cfg = {
	0x1d,
	0x26,
	0x70,
	0x71
};

static struct _cm_addr_cfg_s cm_addr_cfg = {
	0x200,
	0x201,
	0x202,
	0x203,
	0x204,
	0x205,
	0x206,
	0x207,
	0x208,
	0x209,
	0x20a,
	0x20f,
	0x215,
	0x216,
	0x217,
	0x219,
	0x220,
	0x223,
	0x224,
	0x225,
	0x100,
	0x268,
	0x269,
	0x26a
};

static struct _cm_bit_cfg_s cm_bit_cfg = {
	{28, 1},
	{0, 13},
	{16, 13},
	{0, 12},
	{16, 12},
	{0, 1},
	{1, 1},
	{2, 1},
	{3, 1},
	{4, 1},
	{5, 1},
	{16, 13},
	{16, 13},
	{16, 13},
	{16, 8},
	{30, 2},
	{24, 1}
};

/*For single color adjustment*/
static int cur_cm2_luma[CM2_CURVE_SIZE] = {0};
static int cur_cm2_sat[CM2_CURVE_SIZE * 3] = {0};
static int cur_cm2_sat_l[9] = {0};
static int cur_cm2_sat_hl[CM2_CURVE_SIZE * 5] = {0};
static int cur_cm2_hue[CM2_CURVE_SIZE] = {0};
static int cur_cm2_hue_hs[CM2_CURVE_SIZE * 5] = {0};
static int cur_cm2_hue_hl[CM2_CURVE_SIZE * 5] = {0};
static char cur_cm2_offset_luma[CM2_CURVE_SIZE] = {0};
static char cur_cm2_offset_sat[CM2_CURVE_SIZE * 3] = {0};
static char cur_cm2_offset_hue[CM2_CURVE_SIZE] = {0};
static char cur_cm2_offset_hue_hs[CM2_CURVE_SIZE * 5] = {0};
static bool support_cm_final_gain;

/*For ai pq*/
static bool cm_ai_pq_update;
static struct cm_ai_pq_param_s cm_ai_pq_offset;
static struct cm_ai_pq_param_s cm_ai_pq_base;

/*Internal functions*/
static void _set_cm_reg_by_addr(unsigned int addr, int val)
{
	unsigned int reg_addr[2] = {0};
	enum io_mode_e io_mode = EN_MODE_DIR;

	reg_addr[0] = ADDR_PARAM(chroma_reg_cfg.page,
		chroma_reg_cfg.reg_addr_port);
	reg_addr[1] = ADDR_PARAM(chroma_reg_cfg.page,
		chroma_reg_cfg.reg_data_port);

	WRITE_VPP_REG_BY_MODE(io_mode, reg_addr[0], addr);
	WRITE_VPP_REG_BY_MODE(io_mode, reg_addr[1], val);
}

static void _set_cm_bit_by_addr(unsigned int addr, int val,
	unsigned char start, unsigned char len)
{
	unsigned int reg_addr[2] = {0};
	enum io_mode_e io_mode = EN_MODE_DIR;
	int tmp = 0;

	reg_addr[0] = ADDR_PARAM(chroma_reg_cfg.page,
		chroma_reg_cfg.reg_addr_port);
	reg_addr[1] = ADDR_PARAM(chroma_reg_cfg.page,
		chroma_reg_cfg.reg_data_port);

	WRITE_VPP_REG_BY_MODE(io_mode, reg_addr[0], addr);
	tmp = READ_VPP_REG_BY_MODE(io_mode, reg_addr[1]);
	tmp = vpp_insert_int(tmp, val, start, len);

	WRITE_VPP_REG_BY_MODE(io_mode, reg_addr[0], addr);
	WRITE_VPP_REG_BY_MODE(io_mode, reg_addr[1], tmp);
}

static int _get_cm_reg_by_addr(unsigned int addr)
{
	unsigned int reg_addr[2] = {0};
	enum io_mode_e io_mode = EN_MODE_DIR;
	int tmp = 0;

	reg_addr[0] = ADDR_PARAM(chroma_reg_cfg.page,
		chroma_reg_cfg.reg_addr_port);
	reg_addr[1] = ADDR_PARAM(chroma_reg_cfg.page,
		chroma_reg_cfg.reg_data_port);

	WRITE_VPP_REG_BY_MODE(io_mode, reg_addr[0], addr);
	tmp = READ_VPP_REG_BY_MODE(io_mode, reg_addr[1]);

	return tmp;
}

static void _set_cm2_luma_by_index(int *pdata,
	int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		/*curve 0*/
		val[0] &= 0xffffff00;
		val[0] |= pdata[i] & 0x000000ff;

		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			_set_cm_reg_by_addr(addr, val[j]);
		}
	}
}

static void _set_cm2_sat_by_index(int *pdata,
	int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j, tmp;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		val[0] &= 0x000000ff;
		/*curve 0*/
		tmp = pdata[i + CM2_CURVE_SIZE * 0];
		val[0] |= (tmp << 8) & 0x0000ff00;
		/*curve 1*/
		tmp = pdata[i + CM2_CURVE_SIZE * 1];
		val[0] |= (tmp << 16) & 0x00ff0000;
		/*curve 2*/
		tmp = pdata[i + CM2_CURVE_SIZE * 2];
		val[0] |= (tmp << 24) & 0xff000000;

		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			_set_cm_reg_by_addr(addr, val[j]);
		}
	}
}

static void _set_cm2_sat_via_y_by_index(int *pdata,
	int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j, tmp;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		/*curve 0*/
		val[3] &= 0x00ffffff;
		tmp = pdata[i + CM2_CURVE_SIZE * 0];
		val[3] |= (tmp << 24) & 0xff000000;

		/*curve 1,2,3,4*/
		tmp = pdata[i + CM2_CURVE_SIZE * 1];
		val[4] |= tmp & 0x000000ff;
		tmp = pdata[i + CM2_CURVE_SIZE * 2];
		val[4] |= (tmp << 8) & 0x0000ff00;
		tmp = pdata[i + CM2_CURVE_SIZE * 3];
		val[4] |= (tmp << 16) & 0x00ff0000;
		tmp = pdata[i + CM2_CURVE_SIZE * 4];
		val[4] |= (tmp << 24) & 0xff000000;

		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			_set_cm_reg_by_addr(addr, val[j]);
		}
	}
}

static void _set_cm2_hue_by_index(int *pdata,
	int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		/*curve 0*/
		val[1] &= 0xffffff00;
		val[1] |= pdata[i] & 0x000000ff;

		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			_set_cm_reg_by_addr(addr, val[j]);
		}
	}
}

static void _set_cm2_hue_via_s_by_index(int *pdata,
	int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j, tmp;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		/*curve 0,1*/
		val[2] &= 0x0000ffff;
		tmp = pdata[i + CM2_CURVE_SIZE * 0];
		val[2] |= (tmp << 16) & 0x00ff0000;
		tmp = pdata[i + CM2_CURVE_SIZE * 1];
		val[2] |= (tmp << 24) & 0xff000000;

		/*curve 2,3,4*/
		val[3] &= 0xff000000;
		tmp = pdata[i + CM2_CURVE_SIZE * 2];
		val[3] |= tmp & 0x000000ff;
		tmp = pdata[i + CM2_CURVE_SIZE * 3];
		val[3] |= (tmp << 8) & 0x0000ff00;
		tmp = pdata[i + CM2_CURVE_SIZE * 4];
		val[3] |= (tmp << 16) & 0x00ff0000;

		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			_set_cm_reg_by_addr(addr, val[j]);
		}
	}
}

static void _set_cm2_hue_via_y_by_index(int *pdata,
	int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j, tmp;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		/*curve 0,1,2*/
		val[1] &= 0x000000ff;
		tmp = pdata[i + CM2_CURVE_SIZE * 0];
		val[1] |= (tmp << 8) & 0x0000ff00;
		tmp = pdata[i + CM2_CURVE_SIZE * 1];
		val[1] |= (tmp << 16) & 0x00ff0000;
		tmp = pdata[i + CM2_CURVE_SIZE * 2];
		val[1] |= (tmp << 24) & 0xff000000;

		/*curve 3,4*/
		val[2] &= 0xffff0000;
		tmp = pdata[i + CM2_CURVE_SIZE * 3];
		val[2] |= tmp & 0x000000ff;
		tmp = pdata[i + CM2_CURVE_SIZE * 4];
		val[2] |= (tmp << 8) & 0x0000ff00;

		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			_set_cm_reg_by_addr(addr, val[j]);
		}
	}
}

static int _get_cm2_curve_by_index(enum _cm2_curve_type_e type,
	int *pdata, int start, int end)
{
	unsigned int addr = 0;
	int val[CM2_DATA_WR_COUNT] = {0};
	int i, j;
	int data_len = 0;

	if (!pdata || end < start ||
		start > (CM2_CURVE_SIZE - 1) ||
		end > (CM2_CURVE_SIZE - 1))
		return data_len;

	for (i = start; i <= end; i++) {
		for (j = 0; j < CM2_DATA_WR_COUNT; j++) {
			addr = cm_addr_cfg.addr_cm2_enh_coef0_h00 + i * 8 + j;
			val[j] = _get_cm_reg_by_addr(addr);
		}

		switch (type) {
		case EN_CM2_CURVE_LUMA:
			/*curve 0*/
			pdata[i] = val[0] & 0x000000ff;
			data_len += 1;
			break;
		case EN_CM2_CURVE_SAT:
			/*curve 0*/
			pdata[i + CM2_CURVE_SIZE * 0] = (val[0] & 0x0000ff00) >> 8;
			/*curve 1*/
			pdata[i + CM2_CURVE_SIZE * 1] = (val[0] & 0x00ff0000) >> 16;
			/*curve 2*/
			pdata[i + CM2_CURVE_SIZE * 2] = (val[0] & 0xff000000) >> 24;
			data_len += 3;
			break;
		case EN_CM2_CURVE_SAT_Y:
			/*curve 0*/
			pdata[i + CM2_CURVE_SIZE * 0] = (val[3] & 0xff000000) >> 24;
			/*curve 1,2,3,4*/
			pdata[i + CM2_CURVE_SIZE * 1] = val[4] & 0x000000ff;
			pdata[i + CM2_CURVE_SIZE * 2] = (val[4] & 0x0000ff00) >> 8;
			pdata[i + CM2_CURVE_SIZE * 3] = (val[4] & 0x00ff0000) >> 16;
			pdata[i + CM2_CURVE_SIZE * 4] = (val[4] & 0xff000000) >> 24;
			data_len += 5;
			break;
		case EN_CM2_CURVE_HUE:
			/*curve 0*/
			pdata[i] = val[1] & 0x000000ff;
			data_len += 1;
			break;
		case EN_CM2_CURVE_HUE_S:
			/*curve 0,1*/
			pdata[i + CM2_CURVE_SIZE * 0] = (val[2] & 0x00ff0000) >> 16;
			pdata[i + CM2_CURVE_SIZE * 1] = (val[2] & 0xff000000) >> 24;
			/*curve 2,3,4*/
			pdata[i + CM2_CURVE_SIZE * 2] = val[3] & 0x000000ff;
			pdata[i + CM2_CURVE_SIZE * 3] = (val[3] & 0x0000ff00) >> 8;
			pdata[i + CM2_CURVE_SIZE * 4] = (val[3] & 0x00ff0000) >> 16;
			data_len += 5;
			break;
		case EN_CM2_CURVE_HUE_Y:
			/*curve 0,1,2*/
			pdata[i + CM2_CURVE_SIZE * 0] = (val[1] & 0x0000ff00) >> 8;
			pdata[i + CM2_CURVE_SIZE * 1] = (val[1] & 0x00ff0000) >> 16;
			pdata[i + CM2_CURVE_SIZE * 2] = (val[1] & 0xff000000) >> 24;
			/*curve 3,4*/
			pdata[i + CM2_CURVE_SIZE * 3] = val[2] & 0x000000ff;
			pdata[i + CM2_CURVE_SIZE * 4] = (val[2] & 0x0000ff00) >> 8;
			data_len += 5;
			break;
		}
	}

	return data_len;
}

static void _dump_cm_reg_info(void)
{
	PR_DRV("chroma_reg_cfg info:\n");
	PR_DRV("page = %x\n", chroma_reg_cfg.page);
	PR_DRV("reg_misc = %x\n", chroma_reg_cfg.reg_misc);
	PR_DRV("reg_addr_port = %x\n", chroma_reg_cfg.reg_addr_port);
	PR_DRV("reg_data_port = %x\n", chroma_reg_cfg.reg_data_port);

	PR_DRV("cm_addr_cfg info:\n");
	PR_DRV("addr_sat_byyb_node0 = %x\n",
		cm_addr_cfg.addr_sat_byyb_node0);
	PR_DRV("addr_sat_byyb_node1 = %x\n",
		cm_addr_cfg.addr_sat_byyb_node1);
	PR_DRV("addr_sat_byyb_node2 = %x\n",
		cm_addr_cfg.addr_sat_byyb_node2);
	PR_DRV("addr_sat_src_node = %x\n",
		cm_addr_cfg.addr_sat_src_node);
	PR_DRV("addr_cm_enh_sft_mode = %x\n",
		cm_addr_cfg.addr_cm_enh_sft_mode);
	PR_DRV("addr_frm_size = %x\n",
		cm_addr_cfg.addr_frm_size);
	PR_DRV("addr_filter_cfg = %x\n",
		cm_addr_cfg.addr_filter_cfg);
	PR_DRV("addr_cm_global_gain = %x\n",
		cm_addr_cfg.addr_cm_global_gain);
	PR_DRV("addr_cm_enh_ctl = %x\n",
		cm_addr_cfg.addr_cm_enh_ctl);
	PR_DRV("addr_roi_x_scope = %x\n",
		cm_addr_cfg.addr_roi_x_scope);
	PR_DRV("addr_roi_y_scope = %x\n",
		cm_addr_cfg.addr_roi_y_scope);
	PR_DRV("addr_ifo_mode = %x\n",
		cm_addr_cfg.addr_ifo_mode);
	PR_DRV("addr_luma_adj_lmt = %x\n",
		cm_addr_cfg.addr_luma_adj_lmt);
	PR_DRV("addr_sat_adj_lmt = %x\n",
		cm_addr_cfg.addr_sat_adj_lmt);
	PR_DRV("addr_hue_adj_lmt = %x\n",
		cm_addr_cfg.addr_hue_adj_lmt);
	PR_DRV("addr_hue_cfg = %x\n",
		cm_addr_cfg.addr_hue_cfg);
	PR_DRV("addr_luma_adj1 = %x\n",
		cm_addr_cfg.addr_luma_adj1);
	PR_DRV("addr_sta_cfg = %x\n",
		cm_addr_cfg.addr_sta_cfg);
	PR_DRV("addr_sta_sat_hist0 = %x\n",
		cm_addr_cfg.addr_sta_sat_hist0);
	PR_DRV("addr_sta_sat_hist1 = %x\n",
		cm_addr_cfg.addr_sta_sat_hist1);
	PR_DRV("addr_cm2_enh_coef0_h00 = %x\n",
		cm_addr_cfg.addr_cm2_enh_coef0_h00);

	if (support_cm_final_gain) {
		PR_DRV("addr_cm_final_gain_luma = %x\n",
			cm_addr_cfg.addr_cm_final_gain_luma);
		PR_DRV("addr_cm_final_gain_sat = %x\n",
			cm_addr_cfg.addr_cm_final_gain_sat);
		PR_DRV("addr_cm_final_gain_hue = %x\n",
			cm_addr_cfg.addr_cm_final_gain_hue);
	}
}

static void _dump_cm_data_info(void)
{
	int i = 0;

	PR_DRV("cur_cm2_luma data info:\n");
	for (i = 0; i < CM2_CURVE_SIZE; i++) {
		PR_DRV("%d\t", cur_cm2_luma[i]);
		if (i % 8 == 0)
			PR_DRV("\n");
	}

	PR_DRV("support_cm_final_gain = %d\n", support_cm_final_gain);
}

/*External functions*/
int vpp_module_cm_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;
	if (chip_id < CHIP_G12A) {
		cm_bit_cfg.bit_cm1_en.start = 1;
		_set_cm_reg_by_addr(cm_addr_cfg.addr_cm_enh_ctl, 0x76);
	} else {
		_set_cm_reg_by_addr(cm_addr_cfg.addr_cm_enh_ctl, 0x1d);
	}

	if (chip_id == CHIP_T5M)
		support_cm_final_gain = true;
	else
		support_cm_final_gain = false;

	_set_cm_reg_by_addr(cm_addr_cfg.addr_sat_byyb_node0, 0x0);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_sat_byyb_node1, 0x0);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_sat_byyb_node2, 0x0);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_sat_src_node, 0x08000400);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_cm_enh_sft_mode, 0x0);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_frm_size, 0x04380780);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_filter_cfg, 0x0);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_cm_global_gain, 0x02000000);
	_set_cm_reg_by_addr(cm_addr_cfg.addr_ifo_mode, 0x0);

	cm_ai_pq_update = false;
	memset(&cm_ai_pq_offset, 0, sizeof(cm_ai_pq_offset));
	memset(&cm_ai_pq_base, 0, sizeof(cm_ai_pq_base));

	return 0;
}

int vpp_module_cm_en(bool enable)
{
	unsigned int addr = 0;
	unsigned char start = 0;
	unsigned char len = 0;
	int val = 0;

	pr_vpp(PR_DEBUG_CM, "[%s] enable = %d\n", __func__, enable);

	if (enable) {
		addr = ADDR_PARAM(chroma_reg_cfg.page,
			chroma_reg_cfg.reg_misc);
		start = cm_bit_cfg.bit_misc_cm_en.start;
		len = cm_bit_cfg.bit_misc_cm_en.len;
		val = 1;
		WRITE_VPP_REG_BITS_BY_MODE(EN_MODE_DIR, addr,
			val, start, len);

		addr = cm_addr_cfg.addr_sta_cfg;
		start = cm_bit_cfg.bit_sta_hue_hist_sat_th.start;
		len = cm_bit_cfg.bit_sta_hue_hist_sat_th.len;
		val = 24;
		_set_cm_bit_by_addr(addr, val, start, len);

		addr = cm_addr_cfg.addr_luma_adj1;
		start = cm_bit_cfg.bit_sta_sat_hist_mode.start;
		len = cm_bit_cfg.bit_sta_sat_hist_mode.len;
		val = 0;
		_set_cm_bit_by_addr(addr, val, start, len);

		addr = cm_addr_cfg.addr_sta_sat_hist0;
		val = 1 << cm_bit_cfg.bit_cm_ro_frame.start;
		_set_cm_reg_by_addr(addr, val);

		addr = cm_addr_cfg.addr_sta_sat_hist1;
		val = 0;
		_set_cm_reg_by_addr(addr, val);
	}

	addr = cm_addr_cfg.addr_cm_enh_ctl;
	start = cm_bit_cfg.bit_cm1_en.start;
	len = cm_bit_cfg.bit_cm1_en.len;
	val = enable;
	_set_cm_bit_by_addr(addr, val, start, len);

	addr = cm_addr_cfg.addr_sta_cfg;
	start = cm_bit_cfg.bit_sta_en.start;
	len = cm_bit_cfg.bit_sta_en.len;
	val = enable;
	_set_cm_bit_by_addr(addr, val, start, len);

	return 0;
}

/*input array size is 32 for pdata*/
void vpp_module_cm_set_cm2_luma(int *pdata)
{
	int i = 0;
	int tmp = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE; i++) {
		tmp = pdata[i] + cur_cm2_offset_luma[i];
		cur_cm2_luma[i] = vpp_check_range(tmp, (-128), 127);
	}

	_set_cm2_luma_by_index(&cur_cm2_luma[0], 0, 31);
}

/*input array size is 32*3 as curve 0/1/2 for pdata*/
void vpp_module_cm_set_cm2_sat(int *pdata)
{
	int i = 0;
	int tmp = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 3; i++) {
		tmp = pdata[i] + cur_cm2_offset_sat[i] + cm_ai_pq_offset.sat[i];
		cur_cm2_sat[i] = vpp_check_range(tmp, (-128), 127);
		cm_ai_pq_base.sat[i] = pdata[i];
	}

	_set_cm2_sat_by_index(&cur_cm2_sat[0], 0, 31);
}

/*input array size is 9 for pdata*/
void vpp_module_cm_set_cm2_sat_by_l(int *pdata)
{
	unsigned int addr = 0;
	int val = 0;
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < 9; i++)
		cur_cm2_sat_l[i] = pdata[i] & 0x000000ff;

	addr = cm_addr_cfg.addr_sat_byyb_node0;
	val = cur_cm2_sat_l[0];
	val |= cur_cm2_sat_l[1] << 8;
	val |= cur_cm2_sat_l[2] << 16;
	val |= cur_cm2_sat_l[3] << 24;
	_set_cm_reg_by_addr(addr, val);

	addr = cm_addr_cfg.addr_sat_byyb_node1;
	val = cur_cm2_sat_l[4];
	val |= cur_cm2_sat_l[5] << 8;
	val |= cur_cm2_sat_l[6] << 16;
	val |= cur_cm2_sat_l[7] << 24;
	_set_cm_reg_by_addr(addr, val);

	addr = cm_addr_cfg.addr_sat_byyb_node2;
	val = cur_cm2_sat_l[8];
	_set_cm_reg_by_addr(addr, val);
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
void vpp_module_cm_set_cm2_sat_by_hl(int *pdata)
{
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 5; i++)
		cur_cm2_sat_hl[i] = pdata[i];

	_set_cm2_sat_via_y_by_index(&cur_cm2_sat_hl[0], 0, 31);
}

/*input array size is 32 for pdata*/
void vpp_module_cm_set_cm2_hue(int *pdata)
{
	int i = 0;
	int tmp = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE; i++) {
		tmp = pdata[i] + cur_cm2_offset_hue[i];
		cur_cm2_hue[i] = vpp_check_range(tmp, (-128), 127);
	}

	_set_cm2_hue_by_index(pdata, 0, 31);
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
void vpp_module_cm_set_cm2_hue_by_hs(int *pdata)
{
	int i = 0;
	int tmp = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 5; i++) {
		tmp = pdata[i] + cur_cm2_offset_hue_hs[i];
		cur_cm2_hue_hs[i] = vpp_check_range(tmp, (-128), 127);
	}

	_set_cm2_hue_via_s_by_index(&cur_cm2_hue_hs[0], 0, 31);
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
void vpp_module_cm_set_cm2_hue_by_hl(int *pdata)
{
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 5; i++)
		cur_cm2_hue_hl[i] = pdata[i];

	_set_cm2_hue_via_y_by_index(&cur_cm2_hue_hl[0], 0, 31);
}

void vpp_module_cm_set_demo_mode(bool enable, bool left_side)
{
	int val = 0;
	int tmp = 0;
	int height = 0;
	int width = 0;

	pr_vpp(PR_DEBUG_CM, "[%s] enable = %d, left_side = %d\n",
		__func__, enable, left_side);

	if (enable) {
		tmp = _get_cm_reg_by_addr(cm_addr_cfg.addr_frm_size);
		width = vpp_mask_int(tmp,
			cm_bit_cfg.bit_frm_width.start,
			cm_bit_cfg.bit_frm_width.len);
		height = vpp_mask_int(tmp,
			cm_bit_cfg.bit_frm_height.start,
			cm_bit_cfg.bit_frm_height.len);

		_set_cm_bit_by_addr(cm_addr_cfg.addr_roi_x_scope,
			width / 2,
			cm_bit_cfg.bit_roi_x_end.start,
			cm_bit_cfg.bit_roi_x_end.len);

		_set_cm_bit_by_addr(cm_addr_cfg.addr_roi_y_scope,
			height,
			cm_bit_cfg.bit_roi_y_end.start,
			cm_bit_cfg.bit_roi_y_end.len);

		if (left_side)
			val = 0x1;
		else
			val = 0x4;
	} else {
		val = 0x0;
	}

	pr_vpp(PR_DEBUG_CM, "[%s] addr_ifo_mode = %x, val = %x\n",
		__func__, cm_addr_cfg.addr_ifo_mode, val);

	_set_cm_reg_by_addr(cm_addr_cfg.addr_ifo_mode, val);
}

void vpp_module_cm_set_cfg_param(struct cm_cfg_param_s *pparam)
{
	int val = 0;

	if (!pparam)
		return;

	val = (pparam->frm_width & 0x00001fff) |
		((pparam->frm_height & 0x00001fff) << 16);

	pr_vpp(PR_DEBUG_CM, "[%s] addr_frm_size = %x, val = %x\n",
		__func__, cm_addr_cfg.addr_frm_size, val);

	_set_cm_reg_by_addr(cm_addr_cfg.addr_frm_size, val);
}

void vpp_module_cm_set_tuning_param(enum cm_tuning_param_e type,
	int *pdata)
{
	unsigned int addr = 0;
	unsigned char start = 0;
	unsigned char len = 0;
	int val = 0;

	switch (type) {
	case EN_PARAM_GLB_HUE:
		addr = cm_addr_cfg.addr_cm_global_gain;
		start = cm_bit_cfg.bit_cm_glb_gain_hue.start;
		len = cm_bit_cfg.bit_cm_glb_gain_hue.len;
		val = *pdata;

		pr_vpp(PR_DEBUG_CM, "[%s] start = %d, len = %d\n",
			__func__, start, len);
		pr_vpp(PR_DEBUG_CM, "[%s] addr_cm_global_gain = %x, val = %x\n",
			__func__, addr, val);

		_set_cm_bit_by_addr(addr, val, start, len);
		break;
	case EN_PARAM_GLB_SAT:
		addr = cm_addr_cfg.addr_cm_global_gain;
		start = cm_bit_cfg.bit_cm_glb_gain_sat.start;
		len = cm_bit_cfg.bit_cm_glb_gain_sat.len;
		val = *pdata;

		pr_vpp(PR_DEBUG_CM, "[%s] start = %d, len = %d\n",
			__func__, start, len);
		pr_vpp(PR_DEBUG_CM, "[%s] addr_cm_global_gain = %x, val = %x\n",
			__func__, addr, val);

		_set_cm_bit_by_addr(addr, val, start, len);
		break;
	case EN_PARAM_FINAL_GAIN_LUMA:
		if (support_cm_final_gain) {
			addr = cm_addr_cfg.addr_cm_final_gain_luma;
			val = *pdata;

			pr_vpp(PR_DEBUG_CM, "[%s] addr_cm_final_gain_luma = %x, val = %x\n",
				__func__, addr, val);

			_set_cm_reg_by_addr(addr, val);
		} else {
			pr_vpp(PR_DEBUG_CM, "[%s] No support cm final gain\n", __func__);
		}
		break;
	case EN_PARAM_FINAL_GAIN_SAT:
		if (support_cm_final_gain) {
			addr = cm_addr_cfg.addr_cm_final_gain_sat;
			val = *pdata;

			pr_vpp(PR_DEBUG_CM, "[%s] addr_cm_final_gain_sat = %x, val = %x\n",
				__func__, addr, val);

			_set_cm_reg_by_addr(addr, val);
		} else {
			pr_vpp(PR_DEBUG_CM, "[%s] No support cm final gain\n", __func__);
		}
		break;
	case EN_PARAM_FINAL_GAIN_HUE:
		if (support_cm_final_gain) {
			addr = cm_addr_cfg.addr_cm_final_gain_hue;
			val = *pdata;

			pr_vpp(PR_DEBUG_CM, "[%s] addr_cm_final_gain_hue = %x, val = %x\n",
				__func__, addr, val);

			_set_cm_reg_by_addr(addr, val);
		} else {
			pr_vpp(PR_DEBUG_CM, "[%s] No support cm final gain\n", __func__);
		}
		break;
	default:
		break;
	}
}

void vpp_module_cm_sub_module_en(enum cm_sub_module_e sub_module,
	bool enable)
{
	unsigned int addr = 0;
	unsigned char start = 0;
	unsigned char len = 0;

	pr_vpp(PR_DEBUG_CM, "[%s] sub_module = %d, enable = %d\n",
		__func__, sub_module, enable);

	switch (sub_module) {
	case EN_SUB_MD_LUMA_ADJ:
		addr = cm_addr_cfg.addr_cm_enh_ctl;
		start = cm_bit_cfg.bit_cm2_l_adj_en.start;
		len = cm_bit_cfg.bit_cm2_l_adj_en.len;
		_set_cm_bit_by_addr(addr, enable, start, len);
		break;
	case EN_SUB_MD_SAT_ADJ:
		addr = cm_addr_cfg.addr_cm_enh_ctl;
		start = cm_bit_cfg.bit_cm2_s_adj_en.start;
		len = cm_bit_cfg.bit_cm2_s_adj_en.len;
		_set_cm_bit_by_addr(addr, enable, start, len);
		break;
	case EN_SUB_MD_HUE_ADJ:
		addr = cm_addr_cfg.addr_cm_enh_ctl;
		start = cm_bit_cfg.bit_cm2_h_adj_en.start;
		len = cm_bit_cfg.bit_cm2_h_adj_en.len;
		_set_cm_bit_by_addr(addr, enable, start, len);
		break;
	default:
		break;
	}
}

/*input array size is 32 for pdata*/
void vpp_module_cm_set_cm2_offset_luma(char *pdata)
{
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE; i++)
		cur_cm2_offset_luma[i] = pdata[i];
}

/*input array size is 32*3 as curve 0/1/2 for pdata*/
void vpp_module_cm_set_cm2_offset_sat(char *pdata)
{
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 3; i++)
		cur_cm2_offset_sat[i] = pdata[i];
}

/*input array size is 32 for pdata*/
void vpp_module_cm_set_cm2_offset_hue(char *pdata)
{
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE; i++)
		cur_cm2_offset_hue[i] = pdata[i];
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
void vpp_module_cm_set_cm2_offset_hue_by_hs(char *pdata)
{
	int i = 0;

	if (!pdata)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 5; i++)
		cur_cm2_offset_hue_hs[i] = pdata[i];
}

/*input array size is 32 for pdata*/
int vpp_module_cm_get_cm2_luma(int *pdata)
{
	if (!pdata)
		return 0;

	return _get_cm2_curve_by_index(EN_CM2_CURVE_LUMA, pdata, 0, 31);
}

/*input array size is 32*3 as curve 0/1/2 for pdata*/
int vpp_module_cm_get_cm2_sat(int *pdata)
{
	if (!pdata)
		return 0;

	return _get_cm2_curve_by_index(EN_CM2_CURVE_SAT, pdata, 0, 31);
}

/*input array size is 9 for pdata*/
int vpp_module_cm_get_cm2_sat_by_l(int *pdata)
{
	unsigned int addr = 0;
	int val = 0;

	if (!pdata)
		return 0;

	addr = cm_addr_cfg.addr_sat_byyb_node0;
	val = _get_cm_reg_by_addr(addr);
	pdata[0] = val & 0x000000ff;
	pdata[1] = (val & 0x0000ff00) >> 8;
	pdata[2] = (val & 0x00ff0000) >> 16;
	pdata[3] = (val & 0xff000000) >> 24;

	addr = cm_addr_cfg.addr_sat_byyb_node1;
	val = _get_cm_reg_by_addr(addr);
	pdata[4] = val & 0x000000ff;
	pdata[5] = (val & 0x0000ff00) >> 8;
	pdata[6] = (val & 0x00ff0000) >> 16;
	pdata[7] = (val & 0xff000000) >> 24;

	addr = cm_addr_cfg.addr_sat_byyb_node2;
	val = _get_cm_reg_by_addr(addr);
	pdata[8] = val & 0x000000ff;

	return 9;
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
int vpp_module_cm_get_cm2_sat_by_hl(int *pdata)
{
	if (!pdata)
		return 0;

	return _get_cm2_curve_by_index(EN_CM2_CURVE_SAT_Y, pdata, 0, 31);
}

/*input array size is 32 for pdata*/
int vpp_module_cm_get_cm2_hue(int *pdata)
{
	if (!pdata)
		return 0;

	return _get_cm2_curve_by_index(EN_CM2_CURVE_HUE, pdata, 0, 31);
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
int vpp_module_cm_get_cm2_hue_by_hs(int *pdata)
{
	if (!pdata)
		return 0;

	return _get_cm2_curve_by_index(EN_CM2_CURVE_HUE_S, pdata, 0, 31);
}

/*input array size is 32*5 as curve 0/1/2/3/4 for pdata*/
int vpp_module_cm_get_cm2_hue_by_hl(int *pdata)
{
	if (!pdata)
		return 0;

	return _get_cm2_curve_by_index(EN_CM2_CURVE_HUE_Y, pdata, 0, 31);
}

bool vpp_module_cm_get_final_gain_support(void)
{
	return support_cm_final_gain;
}

void vpp_module_cm_on_vs(void)
{
	if (cm_ai_pq_update) {
		vpp_module_cm_set_cm2_sat(&cm_ai_pq_base.sat[0]);
		cm_ai_pq_update = false;
	}
}

void vpp_module_cm_set_reg(unsigned int addr, int val)
{
	_set_cm_reg_by_addr(addr, val);
}

int vpp_module_cm_get_reg(unsigned int addr)
{
	return _get_cm_reg_by_addr(addr);
}

/*For ai pq*/
void vpp_module_cm_get_ai_pq_base(struct cm_ai_pq_param_s *pparam)
{
	int i = 0;

	if (!pparam)
		return;

	for (i = 0; i < CM2_CURVE_SIZE * 3; i++)
		pparam->sat[i] = cm_ai_pq_base.sat[i];
}

void vpp_module_cm_set_ai_pq_offset(struct cm_ai_pq_param_s *pparam)
{
	int i = 0;

	if (!pparam)
		return;

	pr_vpp(PR_DEBUG_CM, "[%s] set data\n", __func__);

	for (i = 0; i < CM2_CURVE_SIZE * 3; i++)
		if (cm_ai_pq_offset.sat[i] != pparam->sat[i]) {
			cm_ai_pq_offset.sat[i] = pparam->sat[i];
			cm_ai_pq_update = true;
		}
}

void vpp_module_cm_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_cm_reg_info();
	else
		_dump_cm_data_info();
}

