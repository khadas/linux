// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_gamma.h"

#define VPP_GAMMA_TABLE_LEN     (257)
#define VPP_PRE_GAMMA_TABLE_LEN (65)

#define GAMMA_ADR_RDY (0x20)
#define GAMMA_WR_RDY  (0x10)
#define GAMMA_RD_RDY  (0x8)

enum _gamma_mode_e {
	EN_MODE_GAMMA_PRE = 0,
	EN_MODE_GAMMA_LCD,
};

enum _gamma_data_type_e {
	EN_TYPE_GAMMA_R = 0,
	EN_TYPE_GAMMA_G,
	EN_TYPE_GAMMA_B
};

struct _gamma_bit_cfg_s {
	struct _bit_s bit_gamma_ctrl_en;
};

struct _gamma_reg_cfg_s {
	unsigned char page;
	unsigned char reg_gamma_ctrl;
	unsigned char reg_gamma_addr;
	unsigned char reg_gamma_data;
};

static int viu_sel;
static unsigned int add_offset;
static int combine_mode;
static int auto_inc;

static unsigned int pre_gamma_tbl_len;
static unsigned int lcd_gamma_tbl_len;

static bool pregm_tp_flag;
static bool lcdgm_tp_flag;
static bool pregm_update_flag;
static bool lcdgm_update_flag;

static unsigned int cur_pregm_tbl[EN_MODE_RGB_MAX][VPP_PRE_GAMMA_TABLE_LEN];
static unsigned int cur_lcdgm_tbl[EN_MODE_RGB_MAX][VPP_GAMMA_TABLE_LEN];
static unsigned int tmp_tbl[EN_MODE_RGB_MAX][VPP_GAMMA_TABLE_LEN];

/*Default table from T3*/
static struct _gamma_reg_cfg_s pre_gamma_reg_cfg = {
	0x39,
	0xd4,
	0xd5,
	0xd6
};

static struct _gamma_reg_cfg_s lcd_gamma_reg_cfg = {
	0x14,
	0x00,
	0x02,
	0x01
};

static struct _gamma_bit_cfg_s gamma_bit_cfg = {
	{0, 1}
};

/*Internal functions*/
static int _set_gamma_ctrl(enum _gamma_mode_e mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (mode) {
	case EN_MODE_GAMMA_PRE:
		addr = ADDR_PARAM(pre_gamma_reg_cfg.page, pre_gamma_reg_cfg.reg_gamma_ctrl);
		break;
	case EN_MODE_GAMMA_LCD:
		addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_ctrl);
		addr += add_offset;
		break;
	default:
		break;
	}

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static void _set_pre_gamma_data(enum io_mode_e io_mode,
	unsigned int addr, int *pdata)
{
	int i = 0;
	unsigned int val = 0;

	if (!pdata)
		return;

	for (i = 0; i < (pre_gamma_tbl_len >> 1); i++) {
		val = (((pdata[i * 2 + 1] << 2) & 0xffff) << 16) | ((pdata[i * 2] << 2) & 0xffff);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
	}

	val = (pdata[pre_gamma_tbl_len - 1] << 2) & 0xffff;
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _operate_wait_timer(int val)
{
	int cnt = 0;
	unsigned int addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_ctrl);

	while (!(READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr) & val)) {
		/*udelay(10);*/
		if (cnt++ > 1000)
			break;
	}
}

static void _set_lcd_gamma_data_single(enum io_mode_e io_mode,
	enum _gamma_data_type_e rgb_type, int *pdata)
{
	int i = 0;
	unsigned int val = 0;
	unsigned int addr = 0;
	unsigned int addr_data = 0;
	unsigned int rgb_mask = 0;

	switch (rgb_type) {
	case EN_TYPE_GAMMA_R:
	default:
		rgb_mask = 0x400;
		break;
	case EN_TYPE_GAMMA_G:
		rgb_mask = 0x200;
		break;
	case EN_TYPE_GAMMA_B:
		rgb_mask = 0x100;
		break;
	}

	addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_addr);
	addr_data = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_data);

	_operate_wait_timer(GAMMA_ADR_RDY);

	val = rgb_mask | (0x800);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	for (i = 0; i < lcd_gamma_tbl_len; i++) {
		_operate_wait_timer(GAMMA_WR_RDY);

		WRITE_VPP_REG_BY_MODE(io_mode, addr_data, pdata[i]);
	}

	_operate_wait_timer(GAMMA_ADR_RDY);

	val = rgb_mask | (0x800) | (0x23);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _get_lcd_gamma_data_single(enum _gamma_data_type_e rgb_type, int *pdata)
{
	int i = 0;
	unsigned int val = 0;
	unsigned int addr = 0;
	unsigned int addr_data = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;
	unsigned int rgb_mask = 0;

	switch (rgb_type) {
	case EN_TYPE_GAMMA_R:
	default:
		rgb_mask = 0x400;
		break;
	case EN_TYPE_GAMMA_G:
		rgb_mask = 0x200;
		break;
	case EN_TYPE_GAMMA_B:
		rgb_mask = 0x100;
		break;
	}

	addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_addr);
	addr_data = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_data);

	_operate_wait_timer(GAMMA_ADR_RDY);

	for (i = 0; i < lcd_gamma_tbl_len; i++) {
		_operate_wait_timer(GAMMA_ADR_RDY);

		val = i | rgb_mask | (0x1000);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

		_operate_wait_timer(GAMMA_ADR_RDY);

		pdata[i] = READ_VPP_REG_BY_MODE(io_mode, addr_data);
	}

	val = rgb_mask | (0x800) | (0x23);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
}

static void _set_lcd_gamma_data_combine(enum io_mode_e io_mode,
	int *pr_data, int *pg_data, int *pb_data)
{
	int i = 0;
	unsigned int val = 0;
	unsigned int addr = 0;

	addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_addr);
	addr += add_offset;

	WRITE_VPP_REG_BY_MODE(io_mode, addr, auto_inc);

	addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_data);
	addr += add_offset;

	for (i = 0; i < lcd_gamma_tbl_len; i++) {
		val = ((pr_data[i] & 0x3ff) << 20)
			| ((pg_data[i] & 0x3ff) << 10)
			| (pb_data[i] & 0x3ff);
		WRITE_VPP_REG_BY_MODE(io_mode, addr, val);
	}
}

static void _get_lcd_gamma_data_combine(int *pr_data, int *pg_data, int *pb_data)
{
	int i = 0;
	unsigned int val = 0;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_addr);
	addr += add_offset;

	WRITE_VPP_REG_BY_MODE(io_mode, addr, auto_inc);

	addr = ADDR_PARAM(lcd_gamma_reg_cfg.page, lcd_gamma_reg_cfg.reg_gamma_data);
	addr += add_offset;

	for (i = 0; i < lcd_gamma_tbl_len; i++) {
		val = READ_VPP_REG_BY_MODE(io_mode, addr);
		pr_data[i] = (val >> 20) & 0x3ff;
		pg_data[i] = (val >> 10) & 0x3ff;
		pb_data[i] = val & 0x3ff;
	}
}

static void _dump_gamma_reg_info(void)
{
	PR_DRV("pre_gamma_reg_cfg info:\n");
	PR_DRV("page = %x\n", pre_gamma_reg_cfg.page);
	PR_DRV("reg_gamma_ctrl = %x\n", pre_gamma_reg_cfg.reg_gamma_ctrl);
	PR_DRV("reg_gamma_addr = %x\n", pre_gamma_reg_cfg.reg_gamma_addr);
	PR_DRV("reg_gamma_data = %x\n", pre_gamma_reg_cfg.reg_gamma_data);

	PR_DRV("lcd_gamma_reg_cfg info:\n");
	PR_DRV("page = %x\n", lcd_gamma_reg_cfg.page);
	PR_DRV("reg_gamma_ctrl = %x\n", lcd_gamma_reg_cfg.reg_gamma_ctrl);
	PR_DRV("reg_gamma_addr = %x\n", lcd_gamma_reg_cfg.reg_gamma_addr);
	PR_DRV("reg_gamma_data = %x\n", lcd_gamma_reg_cfg.reg_gamma_data);
}

static void _dump_gamma_data_info(void)
{
	int i = 0;

	PR_DRV("viu_sel = %d\n", viu_sel);
	PR_DRV("add_offset = %x\n", add_offset);
	PR_DRV("combine_mode = %d\n", combine_mode);
	PR_DRV("auto_inc = %d\n", auto_inc);
	PR_DRV("pre_gamma_tbl_len = %d\n", pre_gamma_tbl_len);
	PR_DRV("lcd_gamma_tbl_len = %d\n", lcd_gamma_tbl_len);
	PR_DRV("pregm_tp_flag = %d\n", pregm_tp_flag);
	PR_DRV("lcdgm_tp_flag = %d\n", lcdgm_tp_flag);

	PR_DRV("cur_pregm_tbl data info:\n");
	for (i = 0; i < pre_gamma_tbl_len; i++)
		PR_DRV("%d\t%d\t%d\n", cur_pregm_tbl[0][i],
			cur_pregm_tbl[1][i], cur_pregm_tbl[2][i]);

	PR_DRV("cur_lcdgm_tbl data info:\n");
	for (i = 0; i < lcd_gamma_tbl_len; i++)
		PR_DRV("%d\t%d\t%d\n", cur_lcdgm_tbl[0][i],
			cur_lcdgm_tbl[1][i], cur_lcdgm_tbl[2][i]);
}

/*External functions*/
int vpp_module_gamma_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;

	if (chip_id >= CHIP_T7) {
		combine_mode = 1;
		lcd_gamma_reg_cfg.page = 0x14;
		lcd_gamma_reg_cfg.reg_gamma_ctrl = 0xb4;
		lcd_gamma_reg_cfg.reg_gamma_addr = 0xb6;
		lcd_gamma_reg_cfg.reg_gamma_data = 0xb5;
	}

	pregm_tp_flag = false;
	lcdgm_tp_flag = false;
	pregm_update_flag = false;
	lcdgm_update_flag = false;

	auto_inc = 1 << 8;
	lcd_gamma_tbl_len = VPP_GAMMA_TABLE_LEN - 1;
	pre_gamma_tbl_len = VPP_PRE_GAMMA_TABLE_LEN;

	if (chip_id == CHIP_T5M) {
		auto_inc = 1 << 9;
		lcd_gamma_tbl_len = VPP_GAMMA_TABLE_LEN;
	}

	return 0;
}

void vpp_module_gamma_set_viu_sel(int val)
{
	pr_vpp(PR_DEBUG_GAMMA, "[%s] viu_sel = %d\n", __func__, val);

	viu_sel = val;

	if (viu_sel == 1) {         /*venc1*/
		add_offset = 0x100;
	} else if (viu_sel == 2) {  /*venc2*/
		add_offset = 0x200;
	} else {                    /*venc0*/
		add_offset = 0x0;
	}
}

int vpp_module_pre_gamma_en(bool enable)
{
	pr_vpp(PR_DEBUG_GAMMA, "[%s] enable = %d\n", __func__, enable);

	return _set_gamma_ctrl(EN_MODE_GAMMA_PRE, enable,
		gamma_bit_cfg.bit_gamma_ctrl_en.start, gamma_bit_cfg.bit_gamma_ctrl_en.len);
}

int vpp_module_pre_gamma_write_single(enum vpp_rgb_mode_e mode,
	unsigned int *pdata)
{
	int i;

	if (!pdata || mode == EN_MODE_RGB_MAX)
		return 0;

	pr_vpp(PR_DEBUG_GAMMA, "[%s] set data, mode = %d\n",
		__func__, mode);

	for (i = 0; i < pre_gamma_tbl_len; i++)
		cur_pregm_tbl[mode][i] = pdata[i];

	pregm_update_flag = true;

	return 0;
}

int vpp_module_pre_gamma_write(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data)
{
	int i;

	if (!pr_data || !pg_data || !pb_data)
		return 0;

	pr_vpp(PR_DEBUG_GAMMA, "[%s] set data\n", __func__);

	for (i = 0; i < pre_gamma_tbl_len; i++) {
		cur_pregm_tbl[EN_MODE_R][i] = pr_data[i];
		cur_pregm_tbl[EN_MODE_G][i] = pg_data[i];
		cur_pregm_tbl[EN_MODE_B][i] = pb_data[i];
	}

	pregm_update_flag = true;

	return 0;
}

int vpp_module_pre_gamma_read(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data)
{
	int i;

	if (!pr_data || !pg_data || !pb_data)
		return 0;

	for (i = 0; i < pre_gamma_tbl_len; i++) {
		pr_data[i] = cur_pregm_tbl[EN_MODE_R][i];
		pg_data[i] = cur_pregm_tbl[EN_MODE_G][i];
		pb_data[i] = cur_pregm_tbl[EN_MODE_B][i];
	}

	return 0;
}

int vpp_module_pre_gamma_pattern(bool enable,
	unsigned int r_val, unsigned int g_val, unsigned int b_val)
{
	int i;
	int *pr_data;
	int *pg_data;
	int *pb_data;
	unsigned int addr;
	enum io_mode_e io_mode = EN_MODE_DIR;

	pr_vpp(PR_DEBUG_GAMMA,
		"[%s] enable = %d, r/g/b = %d\t%d\t%d\n",
		__func__, enable, r_val, g_val, b_val);

	pregm_tp_flag = enable;

	if (enable) {
		for (i = 0; i < pre_gamma_tbl_len; i++) {
			tmp_tbl[EN_MODE_R][i] = r_val;
			tmp_tbl[EN_MODE_G][i] = g_val;
			tmp_tbl[EN_MODE_B][i] = b_val;
		}

		pr_data = &tmp_tbl[EN_MODE_R][0];
		pg_data = &tmp_tbl[EN_MODE_G][0];
		pb_data = &tmp_tbl[EN_MODE_B][0];
	} else {
		pr_data = &cur_pregm_tbl[EN_MODE_R][0];
		pg_data = &cur_pregm_tbl[EN_MODE_G][0];
		pb_data = &cur_pregm_tbl[EN_MODE_B][0];
	}

	addr = ADDR_PARAM(pre_gamma_reg_cfg.page, pre_gamma_reg_cfg.reg_gamma_addr);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	addr = ADDR_PARAM(pre_gamma_reg_cfg.page, pre_gamma_reg_cfg.reg_gamma_data);
	_set_pre_gamma_data(io_mode, addr, pr_data);
	_set_pre_gamma_data(io_mode, addr, pg_data);
	_set_pre_gamma_data(io_mode, addr, pb_data);

	return 0;
}

unsigned int vpp_module_pre_gamma_get_table_len(void)
{
	return pre_gamma_tbl_len;
}

void vpp_module_pre_gamma_on_vs(void)
{
	unsigned int addr;
	enum io_mode_e io_mode = EN_MODE_RDMA;

	if (pregm_tp_flag || !pregm_update_flag)
		return;

	addr = ADDR_PARAM(pre_gamma_reg_cfg.page, pre_gamma_reg_cfg.reg_gamma_addr);
	WRITE_VPP_REG_BY_MODE(io_mode, addr, 0);

	addr = ADDR_PARAM(pre_gamma_reg_cfg.page, pre_gamma_reg_cfg.reg_gamma_data);
	_set_pre_gamma_data(io_mode, addr, &cur_pregm_tbl[EN_MODE_R][0]);
	_set_pre_gamma_data(io_mode, addr, &cur_pregm_tbl[EN_MODE_G][0]);
	_set_pre_gamma_data(io_mode, addr, &cur_pregm_tbl[EN_MODE_B][0]);

	pregm_update_flag = false;
}

int vpp_module_lcd_gamma_en(bool enable)
{
	pr_vpp(PR_DEBUG_GAMMA, "[%s] enable = %d\n", __func__, enable);

	return _set_gamma_ctrl(EN_MODE_GAMMA_LCD, enable,
		gamma_bit_cfg.bit_gamma_ctrl_en.start, gamma_bit_cfg.bit_gamma_ctrl_en.len);
}

int vpp_module_lcd_gamma_write_single(enum vpp_rgb_mode_e mode,
	unsigned int *pdata)
{
	int i;
	enum io_mode_e io_mode = EN_MODE_DIR;
	enum _gamma_data_type_e rgb_type;

	if (!pdata || mode == EN_MODE_RGB_MAX)
		return 0;

	pr_vpp(PR_DEBUG_GAMMA, "[%s] set data, mode = %d\n",
		__func__, mode);

	for (i = 0; i < lcd_gamma_tbl_len; i++)
		cur_lcdgm_tbl[mode][i] = pdata[i];

	if (!combine_mode) {
		if (lcdgm_tp_flag)
			return 0;

		rgb_type = (enum _gamma_data_type_e)mode;
		_set_lcd_gamma_data_single(io_mode, rgb_type, pdata);
	} else {
		lcdgm_update_flag = true;
	}

	return 0;
}

int vpp_module_lcd_gamma_write(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data)
{
	int i;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!pr_data || !pg_data || !pb_data)
		return 0;

	pr_vpp(PR_DEBUG_GAMMA, "[%s] set data\n", __func__);

	for (i = 0; i < lcd_gamma_tbl_len; i++) {
		cur_lcdgm_tbl[EN_MODE_R][i] = pr_data[i];
		cur_lcdgm_tbl[EN_MODE_G][i] = pg_data[i];
		cur_lcdgm_tbl[EN_MODE_B][i] = pb_data[i];
	}

	if (!combine_mode) {
		if (lcdgm_tp_flag)
			return 0;

		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_R, pr_data);
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_G, pg_data);
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_B, pb_data);
	} else {
		lcdgm_update_flag = true;
	}

	return 0;
}

int vpp_module_lcd_gamma_read(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data)
{
	if (!pr_data || !pg_data || !pb_data)
		return 0;

	if (combine_mode) {
		_get_lcd_gamma_data_combine(pr_data, pg_data, pb_data);
	} else {
		_get_lcd_gamma_data_single(EN_TYPE_GAMMA_R, pr_data);
		_get_lcd_gamma_data_single(EN_TYPE_GAMMA_G, pg_data);
		_get_lcd_gamma_data_single(EN_TYPE_GAMMA_B, pb_data);
	}

	return 0;
}

int vpp_module_lcd_gamma_pattern(bool enable,
	unsigned int r_val, unsigned int g_val, unsigned int b_val)
{
	int i;
	int *pr_data;
	int *pg_data;
	int *pb_data;
	enum io_mode_e io_mode = EN_MODE_DIR;

	pr_vpp(PR_DEBUG_GAMMA,
		"[%s] enable = %d, r/g/b = %d\t%d\t%d\n",
		__func__, enable, r_val, g_val, b_val);

	lcdgm_tp_flag = enable;

	if (enable) {
		for (i = 0; i < lcd_gamma_tbl_len; i++) {
			tmp_tbl[EN_MODE_R][i] = r_val;
			tmp_tbl[EN_MODE_G][i] = g_val;
			tmp_tbl[EN_MODE_B][i] = b_val;
		}

		pr_data = &tmp_tbl[EN_MODE_R][0];
		pg_data = &tmp_tbl[EN_MODE_G][0];
		pb_data = &tmp_tbl[EN_MODE_B][0];
	} else {
		pr_data = &cur_lcdgm_tbl[EN_MODE_R][0];
		pg_data = &cur_lcdgm_tbl[EN_MODE_G][0];
		pb_data = &cur_lcdgm_tbl[EN_MODE_B][0];
	}

	if (combine_mode) {
		_set_lcd_gamma_data_combine(io_mode, pr_data, pg_data, pb_data);
	} else {
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_R, pr_data);
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_G, pg_data);
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_B, pb_data);
	}

	return 0;
}

unsigned int vpp_module_lcd_gamma_get_table_len(void)
{
	return lcd_gamma_tbl_len;
}

void vpp_module_lcd_gamma_notify(void)
{
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (!combine_mode) {
		if (lcdgm_tp_flag)
			return;

		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_R,
			&cur_lcdgm_tbl[EN_MODE_R][0]);
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_G,
			&cur_lcdgm_tbl[EN_MODE_G][0]);
		_set_lcd_gamma_data_single(io_mode, EN_TYPE_GAMMA_B,
			&cur_lcdgm_tbl[EN_MODE_B][0]);
	} else {
		lcdgm_update_flag = true;
	}
}

void vpp_module_lcd_gamma_on_vs(void)
{
	if (lcdgm_tp_flag || !lcdgm_update_flag)
		return;

	if (combine_mode) {
		_set_lcd_gamma_data_combine(EN_MODE_RDMA,
			&cur_lcdgm_tbl[EN_MODE_R][0],
			&cur_lcdgm_tbl[EN_MODE_G][0],
			&cur_lcdgm_tbl[EN_MODE_B][0]);

		lcdgm_update_flag = false;
	}
}

void vpp_module_gamma_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_gamma_reg_info();
	else
		_dump_gamma_data_info();
}

