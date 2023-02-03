// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_meter.h"

struct _hist_bit_cfg_s {
	struct _bit_s bit_hist_ctrl_en;
	struct _bit_s bit_hist_dnlp_low;
	struct _bit_s bit_hist_in_sel;
	struct _bit_s bit_hist_pic_width;
	struct _bit_s bit_hist_pic_height;
	struct _bit_s bit_hist_min;
	struct _bit_s bit_hist_max;
	struct _bit_s bit_hist_spl_pxl_cnt;
	struct _bit_s bit_hist_dnlp_data0;
	struct _bit_s bit_hist_dnlp_data1;
};

struct _hist_reg_cfg_s {
	unsigned char page;
	unsigned char reg_hist_ctrl;
	unsigned char reg_hist_h_start_end;
	unsigned char reg_hist_v_start_end;
	unsigned char reg_hist_max_min;
	unsigned char reg_hist_spl_val;
	unsigned char reg_hist_spl_pix_cnt;
	unsigned char reg_hist_chroma_sum;
	unsigned char reg_hist_dnlp[DNLP_HIST_CNT];
	unsigned char reg_hist_pic_size;
	unsigned char reg_hist_blk_wht_val;
	unsigned char reg_hist_gclk_ctrl;
	unsigned char reg_hist_ro_low_idx;
	unsigned char reg_hist_ro_low;
};

struct _vpp_bit_cfg_s {
	struct _bit_s bit_vpp_in_width;
	struct _bit_s bit_vpp_in_height;
};

struct _vpp_reg_cfg_s {
	unsigned char page;
	unsigned char reg_vpp_in_size;
	unsigned char reg_vpp_chroma_addr;
	unsigned char reg_vpp_chroma_data;
};

/*Default table from T3*/
static struct _hist_reg_cfg_s hist_reg_cfg = {0};

static struct _hist_bit_cfg_s hist_bit_cfg = {
	{0, 1},
	{5, 3},
	{11, 3},
	{0, 13},
	{16, 13},
	{0, 8},
	{8, 8},
	{0, 22},
	{0, 16},
	{16, 16}
};

static struct _vpp_reg_cfg_s vpp_reg_cfg = {
	0x1d,
	0x20,
	0x70,
	0x71
};

static struct _vpp_bit_cfg_s vpp_bit_cfg = {
	{0, 13},
	{16, 13}
};

static struct vpp_hist_report_s vpp_hist_report;
static unsigned int pre_hist_height;
static unsigned int pre_hist_width;
static unsigned int hue_hist_bin0_addr;
static unsigned int sat_hist_bin0_addr;
static bool support_dark_hist;

/*Internal functions*/
static void _get_vpp_in_size(unsigned int *pheight, unsigned int *pwidth)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	int tmp = 0;

	addr = ADDR_PARAM(vpp_reg_cfg.page, vpp_reg_cfg.reg_vpp_in_size);
	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);

	*pwidth = vpp_mask_int(tmp,
		vpp_bit_cfg.bit_vpp_in_width.start,
		vpp_bit_cfg.bit_vpp_in_width.len);
	*pheight = vpp_mask_int(tmp,
		vpp_bit_cfg.bit_vpp_in_height.start,
		vpp_bit_cfg.bit_vpp_in_height.len);
}

static int _set_hist_ctrl(int val, unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_ctrl);

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static void _set_hist_pic_size(unsigned int height, unsigned int width)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_pic_size);

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, height,
		hist_bit_cfg.bit_hist_pic_height.start,
		hist_bit_cfg.bit_hist_pic_height.len);

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, width,
			hist_bit_cfg.bit_hist_pic_width.start,
			hist_bit_cfg.bit_hist_pic_width.len);
}

static void _dump_meter_reg_info(void)
{
	PR_DRV("hist_reg_cfg info:\n");
	PR_DRV("page = %x\n", hist_reg_cfg.page);
	PR_DRV("reg_hist_ctrl = %x\n", hist_reg_cfg.reg_hist_ctrl);
	PR_DRV("reg_hist_h_start_end = %x\n",
		hist_reg_cfg.reg_hist_h_start_end);
	PR_DRV("reg_hist_v_start_end = %x\n",
		hist_reg_cfg.reg_hist_v_start_end);
	PR_DRV("reg_hist_max_min = %x\n",
		hist_reg_cfg.reg_hist_max_min);
	PR_DRV("reg_hist_spl_val = %x\n",
		hist_reg_cfg.reg_hist_spl_val);
	PR_DRV("reg_hist_spl_pix_cnt = %x\n",
		hist_reg_cfg.reg_hist_spl_pix_cnt);
	PR_DRV("reg_hist_chroma_sum = %x\n",
		hist_reg_cfg.reg_hist_chroma_sum);
	PR_DRV("reg_hist_dnlp_0 = %x\n",
		hist_reg_cfg.reg_hist_dnlp[0]);
	PR_DRV("reg_hist_pic_size = %x\n",
		hist_reg_cfg.reg_hist_pic_size);
	PR_DRV("reg_hist_blk_wht_val = %x\n",
		hist_reg_cfg.reg_hist_blk_wht_val);
	PR_DRV("reg_hist_gclk_ctrl = %x\n",
		hist_reg_cfg.reg_hist_gclk_ctrl);

	if (support_dark_hist) {
		PR_DRV("reg_hist_ro_low_idx = %x\n",
			hist_reg_cfg.reg_hist_ro_low_idx);
		PR_DRV("reg_hist_ro_low = %x\n",
			hist_reg_cfg.reg_hist_ro_low);
	}

	PR_DRV("vpp_reg_cfg info:\n");
	PR_DRV("page = %x\n", vpp_reg_cfg.page);
	PR_DRV("reg_vpp_in_size = %x\n",
		vpp_reg_cfg.reg_vpp_in_size);
	PR_DRV("reg_vpp_chroma_addr = %x\n",
		vpp_reg_cfg.reg_vpp_chroma_addr);
	PR_DRV("reg_vpp_chroma_data = %x\n",
		vpp_reg_cfg.reg_vpp_chroma_data);
}

static void _dump_meter_data_info(void)
{
	PR_DRV("pre_hist_height = %d\n", pre_hist_height);
	PR_DRV("pre_hist_width = %x\n", pre_hist_width);
	PR_DRV("hue_hist_bin0_addr = %d\n", hue_hist_bin0_addr);
	PR_DRV("sat_hist_bin0_addr = %d\n", sat_hist_bin0_addr);
	PR_DRV("support_dark_hist = %d\n", support_dark_hist);
}

/*External functions*/
int vpp_module_meter_init(struct vpp_dev_s *pdev)
{
	int i = 0;
	enum vpp_chip_type_e chip_id;

	hist_reg_cfg.page = 0x2e;
	hist_reg_cfg.reg_hist_ctrl        = 0x00;
	hist_reg_cfg.reg_hist_h_start_end = 0x01;
	hist_reg_cfg.reg_hist_v_start_end = 0x02;
	hist_reg_cfg.reg_hist_max_min     = 0x03;
	hist_reg_cfg.reg_hist_spl_val     = 0x04;
	hist_reg_cfg.reg_hist_spl_pix_cnt = 0x05;
	hist_reg_cfg.reg_hist_chroma_sum  = 0x06;

	for (i = 0; i < DNLP_HIST_CNT; i++)
		hist_reg_cfg.reg_hist_dnlp[i] = 0x07 + i;

	hist_reg_cfg.reg_hist_pic_size    = 0x28;
	hist_reg_cfg.reg_hist_blk_wht_val = 0x29;
	hist_reg_cfg.reg_hist_gclk_ctrl   = 0x2a;

	chip_id = pdev->pm_data->chip_id;

	if (chip_id < CHIP_G12A)
		vpp_reg_cfg.reg_vpp_in_size = 0xa6;

	hue_hist_bin0_addr = 0x226;
	sat_hist_bin0_addr = 0x246;

	if (chip_id == CHIP_T5M)
		support_dark_hist = true;
	else
		support_dark_hist = false;

	hist_reg_cfg.reg_hist_ro_low_idx = 0x2b;
	hist_reg_cfg.reg_hist_ro_low     = 0x2c;

	return 0;
}

int vpp_module_meter_hist_en(bool enable)
{
	pr_vpp(PR_DEBUG_METER, "[%s] enable = %d\n", __func__, enable);

	return _set_hist_ctrl(enable,
		hist_bit_cfg.bit_hist_ctrl_en.start,
		hist_bit_cfg.bit_hist_ctrl_en.len);
}

void vpp_module_meter_fetch_hist_report(void)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	int i = 0;
	int tmp = 0;
	unsigned int hist_height;
	unsigned int hist_width;

	_get_vpp_in_size(&hist_height, &hist_width);

	if (hist_height != pre_hist_height ||
		hist_width != pre_hist_width) {
		_set_hist_pic_size(hist_height, hist_width);
		pre_hist_height = hist_height;
		pre_hist_width = hist_width;
	}

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_ctrl);
	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	vpp_hist_report.hist_pow = vpp_mask_int(tmp,
		hist_bit_cfg.bit_hist_dnlp_low.start,
		hist_bit_cfg.bit_hist_dnlp_low.len);

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_spl_val);
	vpp_hist_report.luma_sum = READ_VPP_REG_BY_MODE(io_mode, addr);

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_chroma_sum);
	vpp_hist_report.chroma_sum = READ_VPP_REG_BY_MODE(io_mode, addr);

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_spl_pix_cnt);
	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	vpp_hist_report.pixel_sum = vpp_mask_int(tmp,
		hist_bit_cfg.bit_hist_spl_pxl_cnt.start,
		hist_bit_cfg.bit_hist_spl_pxl_cnt.len);

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_pic_size);
	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	vpp_hist_report.height = vpp_mask_int(tmp,
		hist_bit_cfg.bit_hist_pic_height.start,
		hist_bit_cfg.bit_hist_pic_height.len);
	vpp_hist_report.width = vpp_mask_int(tmp,
		hist_bit_cfg.bit_hist_pic_width.start,
		hist_bit_cfg.bit_hist_pic_width.len);

	addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_max_min);
	tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
	vpp_hist_report.luma_max = vpp_mask_int(tmp,
		hist_bit_cfg.bit_hist_max.start,
		hist_bit_cfg.bit_hist_max.len);
	vpp_hist_report.luma_min = vpp_mask_int(tmp,
		hist_bit_cfg.bit_hist_min.start,
		hist_bit_cfg.bit_hist_min.len);

	for (i = 0; i < DNLP_HIST_CNT; i++) {
		addr = ADDR_PARAM(hist_reg_cfg.page, hist_reg_cfg.reg_hist_dnlp[i]);
		tmp = READ_VPP_REG_BY_MODE(io_mode, addr);
		vpp_hist_report.gamma[i * 2] = (unsigned short)vpp_mask_int(tmp,
			hist_bit_cfg.bit_hist_dnlp_data0.start,
			hist_bit_cfg.bit_hist_dnlp_data0.len);
		vpp_hist_report.gamma[i * 2 + 1] = (unsigned short)vpp_mask_int(tmp,
			hist_bit_cfg.bit_hist_dnlp_data1.start,
			hist_bit_cfg.bit_hist_dnlp_data1.len);
	}

	for (i = 0; i < HIST_HUE_GM_BIN_CNT; i++) {
		addr = ADDR_PARAM(vpp_reg_cfg.page, vpp_reg_cfg.reg_vpp_chroma_addr);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, hue_hist_bin0_addr + i);
		addr = ADDR_PARAM(vpp_reg_cfg.page, vpp_reg_cfg.reg_vpp_chroma_data);
		vpp_hist_report.hue_gamma[i] = READ_VPP_REG_BY_MODE(io_mode, addr);
	}

	for (i = 0; i < HIST_SAT_GM_BIN_CNT; i++) {
		addr = ADDR_PARAM(vpp_reg_cfg.page, vpp_reg_cfg.reg_vpp_chroma_addr);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, sat_hist_bin0_addr + i);
		addr = ADDR_PARAM(vpp_reg_cfg.page, vpp_reg_cfg.reg_vpp_chroma_data);
		vpp_hist_report.sat_gamma[i] = READ_VPP_REG_BY_MODE(io_mode, addr);
	}

	if (support_dark_hist) {
		addr = ADDR_PARAM(hist_reg_cfg.page,
			hist_reg_cfg.reg_hist_ro_low_idx);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, 1 << 6);

		addr = ADDR_PARAM(hist_reg_cfg.page,
			hist_reg_cfg.reg_hist_ro_low);
		for (i = 0; i < HIST_GM_BIN_CNT; i++)
			vpp_hist_report.dark_gamma[i] =
				READ_VPP_REG_BY_MODE(io_mode, addr);
	}
}

struct vpp_hist_report_s *vpp_module_meter_get_hist_report(void)
{
	return &vpp_hist_report;
}

bool vpp_module_meter_get_dark_hist_support(void)
{
	return support_dark_hist;
}

void vpp_module_meter_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_meter_reg_info();
	else
		_dump_meter_data_info();
}

