// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_reg.h"

static unsigned int ldc_gain_lut_array[16 * 64] = {
3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668, 2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1056, 968, 880, 792, 704, 616, 528, 440, 352, 264, 176, 88, 0,

3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668, 2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 992, 868, 744, 620, 496, 372, 248, 124, 0,

3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668, 2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 880, 660, 440, 220, 0,

3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668, 2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 0,

3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668, 2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

2720, 2720, 2720, 2720, 2720, 2720, 2720, 2668, 2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

2336, 2336, 2336, 2336, 2336, 2336, 2336, 2336, 2336, 2336, 2299, 2208, 2128, 2056, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 1991, 1933,
1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808, 1808,
1808, 1808, 1785, 1744, 1705, 1669, 1635, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632, 1632,
1632, 1632, 1632, 1632, 1632, 1632, 1632, 1604, 1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488,
1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1488, 1470, 1447, 1426, 1405,
1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360,
1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360, 1360,
1360, 1360, 1349, 1331, 1315, 1299, 1284, 1269, 1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248,
1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248,
1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1248, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168,
1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168,
1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1168, 1167,
1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088,
1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088,
1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088,
1088, 1088, 1088, 1088, 1088, 1088, 1088, 1088, 1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024,

1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
};

static void ldc_gain_lut_set_t7(void)
{
	unsigned int data, ram_base = 0;
	int i, j;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 64; j = j + 2) {
			lcd_vcbus_write(LDC_GAIN_LUT_ADDR, ram_base);
			data = ((ldc_gain_lut_array[i * 64 + j + 1] << 12) +
				ldc_gain_lut_array[i * 64 + j]);
			lcd_vcbus_write(LDC_GAIN_LUT_DATA, data);
			lcd_vcbus_write(LDC_GAIN_LUT_CTRL0, 0x1);
			lcd_vcbus_write(LDC_GAIN_LUT_CTRL1, 0x0);
			lcd_vcbus_write(LDC_GAIN_LUT_CTRL1, 0x1);

			ram_base = ram_base + 2;
			lcd_vcbus_read(LDC_REG_PANEL_SIZE);
			lcd_vcbus_read(LDC_REG_PANEL_SIZE);
			lcd_vcbus_read(LDC_REG_PANEL_SIZE);
		}
	}
}

static void ldc_gain_lut_set_t3(void)
{
	unsigned int data_wr, data_rd;
	int i, j;

	lcd_vcbus_write(LDC_GAIN_LUT_CTRL0, 2); //switch to cbus clock for gain lut ram
	lcd_vcbus_write(LDC_GAIN_LUT_ADDR, 0);

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 64; j = j + 2) {
			data_wr = ((ldc_gain_lut_array[i * 64 + j + 1] << 12) +
				   ldc_gain_lut_array[i * 64 + j]);
			lcd_vcbus_write(LDC_GAIN_LUT_DATA, data_wr);
		}
	}
	lcd_vcbus_write(LDC_GAIN_LUT_ADDR, 0);
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 64; j = j + 2) {
			data_wr = ((ldc_gain_lut_array[i * 64 + j + 1] << 12) +
				   ldc_gain_lut_array[i * 64 + j]);
			data_rd = lcd_vcbus_read(LDC_RO_GAIN_SMP_DATA);
			if (data_wr != data_rd) {
				if (ldim_debug_print) {
					LDIMERR("%s: %d: data_wr=0x%x, data_rd=0x%x\n",
						__func__, i * 64 + j, data_wr, data_rd);
				}
			}
		}
	}
	lcd_vcbus_write(LDC_GAIN_LUT_CTRL0, 0); //switch to ip clock for gain lut ram

	lcd_vcbus_read(LDC_REG_PANEL_SIZE);
	lcd_vcbus_read(LDC_REG_PANEL_SIZE);
}

static void ldim_profile_load(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_profile_s *profile = ldim_drv->dev_drv->bl_profile;

	if (!profile)
		return;

	if (profile->mode != 2)
		return;
	if (strcmp(profile->file_path, "null") == 0) {
		LDIMERR("%s: file_path is null\n", __func__);
		return;
	}
	if (ldim_drv->rmem->flag == 0) {
		LDIMERR("%s: ldc resv mem invalid\n", __func__);
		return;
	}

	ldc_mem_write(profile->file_path,
		      ldim_drv->rmem->profile_mem_paddr,
		      ldim_drv->rmem->profile_mem_size);
}

static void ldc_factor_init(unsigned int width, unsigned int height,
			    unsigned int col_num, unsigned int row_num)
{
	unsigned int factor_div_00, factor_div_01, factor_div_10, factor_div_11;
	unsigned int bits_div_00, bits_div_01, bits_div_10, bits_div_11;
	unsigned int dis_x0, dis_y0;
	unsigned int dis_x1, dis_y1;
	unsigned int block_xnum, block_ynum;
	unsigned int dis[4], multifactor[4], bits_for_div[4];
	int i;

	block_xnum = (1 << 7); //reg_ldc_blk_xnum
	block_ynum = (1 << 6); //reg_ldc_blk_ynum

	dis_x0 = width / block_xnum;
	dis_y0 = height / block_ynum;

	if (width % block_xnum)
		dis_x1 = dis_x0 + 1;
	else
		dis_x1 = dis_x0;

	if (height % block_ynum)
		dis_y1 = dis_y0 + 1;
	else
		dis_y1 = dis_y0;

	dis[0] = dis_x0 * dis_y0;
	dis[1] = dis_x1 * dis_y0;
	dis[2] = dis_x0 * dis_y1;
	dis[3] = dis_x1 * dis_y1;

	memset(multifactor, 0, 4 * sizeof(unsigned int));
	memset(bits_for_div, 0, 4 * sizeof(unsigned int));
	for (i = 0; i < 4; i++) {
		while (multifactor[i] < (1 << 16)) {
			bits_for_div[i] = bits_for_div[i] + 1;
			multifactor[i] = (1 << bits_for_div[i]) / dis[i];
		}
		bits_for_div[i] = bits_for_div[i] - 1;
		multifactor[i] = (1 << bits_for_div[i]) / dis[i];
	}

	factor_div_00 = multifactor[0];
	factor_div_01 = multifactor[1];
	factor_div_10 = multifactor[2];
	factor_div_11 = multifactor[3];
	bits_div_00 = bits_for_div[0];
	bits_div_01 = bits_for_div[1];
	bits_div_10 = bits_for_div[2];
	bits_div_11 = bits_for_div[3];

	LDIMPR("ldc_factor_for_div_00/01/10/11:%d, %d, %d, %d\n",
	       factor_div_00, factor_div_01,
	       factor_div_10, factor_div_11);
	LDIMPR("ldc_bits_for_div_00/01/10/11:%d, %d, %d, %d\n",
	       bits_div_00, bits_div_01,
	       bits_div_10, bits_div_11);

	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_0, factor_div_00, 16, 16);
	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_0, factor_div_01, 0, 16);
	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_1, factor_div_10, 16, 16);
	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_1, factor_div_11, 0, 16);

	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_00, 24, 8);
	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_01, 16, 8);
	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_10, 8, 8);
	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_11, 0, 8);
	LDIMPR("%s: LDC_REG_BITS_DIV 0x%x=0x%x\n", __func__,
	       LDC_REG_BITS_DIV, lcd_vcbus_read(LDC_REG_BITS_DIV));
}

#define MAX_SEG_COL_NUM 48
#define MAX_SEG_ROW_NUM 32
static void ldc_set_t7(unsigned int width, unsigned int height,
		       unsigned int col_num, unsigned int row_num)
{
	unsigned int seg_base;
	unsigned short seg_x_bdy[MAX_SEG_COL_NUM]; //col_num
	unsigned short seg_y_bdy[MAX_SEG_ROW_NUM]; //row_num
	unsigned int temp[2], data;
	int i;

	LDIMPR("width:%d, height:%d, col_num:%d, row_num:%d\n",
	       width, height, col_num, row_num);

	lcd_vcbus_setb(LDC_REG_PANEL_SIZE, width, 16, 16);
	lcd_vcbus_setb(LDC_REG_PANEL_SIZE, height, 0, 16);
	LDIMPR("%s: LDC_REG_PANEL_SIZE 0x%x=0x%x\n", __func__,
	       LDC_REG_PANEL_SIZE, lcd_vcbus_read(LDC_REG_PANEL_SIZE));

	ldc_factor_init(width, height, col_num, row_num);

	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, row_num, 8, 6);
	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, col_num, 14, 6);
	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 7, 4, 4); //reg_ldc_blk_xnum
	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 6, 0, 4); //reg_ldc_blk_ynum

	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 1, 23, 1); //reg_ldc_ds_filter_mode
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 0x40, 15, 8); //reg_ldc_y_gain
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 3, 12, 3); //reg_ldc_hist_mode
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 1, 11, 1); //reg_ldc_hist_blend_mode
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 0x60, 4, 7); //reg_ldc_hist_blend_alpha
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 13, 0, 4); //reg_ldc_hist_adap_blend_max_gain

	lcd_vcbus_setb(LDC_REG_HIST_OVERLAP, 0, 18, 10); //reg_ldc_seg_x_overlap
	lcd_vcbus_setb(LDC_REG_HIST_OVERLAP, 0, 8, 10); //reg_ldc_seg_y_overlap
	lcd_vcbus_setb(LDC_REG_HIST_OVERLAP, 10, 0, 8); //reg_ldc_max95_ratio

	lcd_vcbus_setb(LDC_REG_BLEND_DIFF_TH, 256, 12, 12); //reg_ldc_hist_adap_blend_diff_th1
	lcd_vcbus_setb(LDC_REG_BLEND_DIFF_TH, 640, 0, 12); //reg_ldc_hist_adap_blend_diff_th2

	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 0x70, 18, 8); //reg_ldc_hist_adap_blend_gain_0
	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 0x40, 10, 8); //reg_ldc_hist_adap_blend_gain_1
	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 2, 4, 6); //reg_ldc_hist_adap_blend_th0
	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 4, 0, 4); //reg_ldc_hist_adap_blend_thn

	lcd_vcbus_setb(LDC_REG_INIT_BL, 0, 12, 12); //reg_ldc_init_bl_min
	lcd_vcbus_setb(LDC_REG_INIT_BL, 0xfff, 0, 12); //reg_ldc_init_bl_max

	lcd_vcbus_setb(LDC_REG_SF_MODE, 2, 24, 2); //reg_ldc_sf_mode
	lcd_vcbus_setb(LDC_REG_SF_MODE, 0x600, 12, 12); //reg_ldc_sf_tsf_3x3
	lcd_vcbus_setb(LDC_REG_SF_MODE, 0xc00, 0, 12); //reg_ldc_sf_tsf_5x5

	lcd_vcbus_setb(LDC_REG_SF_GAIN, 0x20, 8, 8); //reg_ldc_sf_gain_up
	lcd_vcbus_setb(LDC_REG_SF_GAIN, 0x00, 0, 8); //reg_ldc_sf_gain_dn

	lcd_vcbus_setb(LDC_REG_BS_MODE, 0, 12, 3); //reg_ldc_bs_bl_mode
	lcd_vcbus_setb(LDC_REG_BS_MODE, 0, 0, 12); //reg_ldc_glb_apl

	lcd_vcbus_setb(LDC_REG_APL, 0x20, 0, 8); //reg_ldc_bs_glb_apl_gain

	lcd_vcbus_setb(LDC_REG_GLB_BOOST, 0x200, 16, 12); //reg_ldc_bs_dark_scene_bl_th
	lcd_vcbus_setb(LDC_REG_GLB_BOOST, 0x20, 8, 8); //reg_ldc_bs_gain
	lcd_vcbus_setb(LDC_REG_GLB_BOOST, 0x60, 0, 8); //reg_ldc_bs_limit_gain

	lcd_vcbus_setb(LDC_REG_LOCAL_BOOST, 0x20, 20, 8); //reg_ldc_bs_loc_apl_gain
	lcd_vcbus_setb(LDC_REG_LOCAL_BOOST, 0x20, 12, 8); //reg_ldc_bs_loc_max_min_gain
	lcd_vcbus_setb(LDC_REG_LOCAL_BOOST, 0x600, 0, 12); //reg_ldc_bs_loc_dark_scene_bl_th

	lcd_vcbus_setb(LDC_REG_TF, 0x20, 24, 8); //reg_ldc_tf_low_alpha
	lcd_vcbus_setb(LDC_REG_TF, 0x20, 16, 8); //reg_ldc_tf_high_alpha
	lcd_vcbus_setb(LDC_REG_TF, 0x40, 8, 8); //reg_ldc_tf_low_alpha_sc
	lcd_vcbus_setb(LDC_REG_TF, 0x40, 0, 8); //reg_ldc_tf_high_alpha_sc

	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 9, 1); //reg_ldc_calc_tmp_flt_en
	lcd_vcbus_setb(LDC_REG_TF_SC, 0, 8, 1); //reg_ldc_tf_sc_flag
	lcd_vcbus_setb(LDC_REG_TF_SC, 7, 4, 4); //reg_ldc_cmp_mask_x
	lcd_vcbus_setb(LDC_REG_TF_SC, 7, 0, 4); //reg_ldc_cmp_mask_y

	lcd_vcbus_setb(LDC_REG_PROFILE_MODE, 0x240, 8, 16); //reg_ldc_profile_k
	lcd_vcbus_setb(LDC_REG_PROFILE_MODE, 0x18, 0, 8); //reg_ldc_profile_bits

	lcd_vcbus_setb(LDC_REG_BLK_FILTER, 56, 8, 8); //reg_ldc_block_filter_a
	lcd_vcbus_setb(LDC_REG_BLK_FILTER, 37, 0, 8); //reg_ldc_block_filter_b

	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 20, 24, 8); //reg_ldc_block_filter_c
	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 10, 16, 8); //reg_ldc_block_filter_d
	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 5, 8, 8); //reg_ldc_block_filter_e
	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 2, 0, 8); //reg_ldc_block_filter_f

	//ldc_bl_adp_frm_en=0
	//ldc_bl_input_fid=0
	//ro_ldc_bl_input_fid=0
	//ro_ldc_bl_output_fid=0
	//ldc_bl_buf_diff=0
	lcd_vcbus_setb(LDC_REG_BL_MEMORY, 0, 0, 3); //reg_ldc_bl_buf_diff
	//ldc_bl_buf_num=4

	lcd_vcbus_setb(LDC_REG_GLB_GAIN, 1024, 0, 12); //reg_ldc_glb_gain

	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 10, 1); //reg_ldc_comp_blk_intsty_en
	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 13, 1); //reg_ldc_comp_pxl_cmp_en
	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 14, 1); //reg_ldc_comp_en
	lcd_vcbus_setb(LDC_REG_DITHER, 0, 1, 1); //reg_ldc_dth_en
	lcd_vcbus_setb(LDC_REG_DITHER, 0, 0, 1); //reg_ldc_dth_bw

	memset(seg_x_bdy, 0, MAX_SEG_COL_NUM * sizeof(unsigned short));
	memset(seg_y_bdy, 0, MAX_SEG_ROW_NUM * sizeof(unsigned short));
	data = width / col_num;
	for (i = 0; i < col_num; i++) {
		seg_x_bdy[i] = data * (i + 1);
		LDIMPR("seg_x_bdy[%d]: %d\n", i, seg_x_bdy[i]);
	}
	data = height / row_num;
	for (i = 0; i < row_num; i++) {
		seg_y_bdy[i] = data * (i + 1);
		LDIMPR("seg_y_bdy[%d]: %d\n", i, seg_y_bdy[i]);
	}

	seg_base = LDC_REG_SEG_X_BOUNDARY_0_1;
	for (i = 0; i < MAX_SEG_COL_NUM; i = i + 2) {
		if (i < col_num) {
			lcd_vcbus_setb(seg_base, seg_x_bdy[i], 14, 14);
			temp[0] = seg_x_bdy[i];
		} else {
			lcd_vcbus_setb(seg_base, 0, 14, 14);
			temp[0] = 0;
		}

		if ((i + 1) < col_num) {
			lcd_vcbus_setb(seg_base, seg_x_bdy[i + 1], 0, 14);
			temp[1] = seg_x_bdy[i + 1];
		} else {
			lcd_vcbus_setb(seg_base, 0, 0, 14);
			temp[1] = 0;
		}

		LDIMPR("SEG_X addr: 0x%x, %d,%d, readback: 0x%x\n",
		       seg_base, temp[0], temp[1],
		       lcd_vcbus_read(seg_base));
		seg_base = seg_base + 1;
	}

	seg_base = LDC_REG_SEG_Y_BOUNDARY_0_1;
	for (i = 0; i < MAX_SEG_ROW_NUM; i = i + 2) {
		if (i < row_num) {
			lcd_vcbus_setb(seg_base, seg_y_bdy[i], 13, 13);
			temp[0] = seg_y_bdy[i];
		} else {
			lcd_vcbus_setb(seg_base, 0, 13, 13);
			temp[0] = 0;
		}

		if ((i + 1) < row_num) {
			lcd_vcbus_setb(seg_base, seg_y_bdy[i + 1], 0, 13);
			temp[1] = seg_y_bdy[i + 1];
		} else {
			lcd_vcbus_setb(seg_base, 0, 0, 13);
			temp[1] = 0;
		}

		LDIMPR("SEG_Y addr: 0x%x, %d,%d, readback: 0x%x\n",
		       seg_base, temp[0], temp[1],
		       lcd_vcbus_read(seg_base));
		seg_base = seg_base + 1;
	}

	lcd_vcbus_setb(LDC_CTRL_MISC0, 0, 28, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC0, 1, 17, 1); //reg_ldc_vs_edge_sel

	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 2, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 1, 2, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 2, 1);

	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 7, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 6, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 5, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 4, 1);

	lcd_vcbus_setb(LDC_CTRL_MISC1, 2000, 8, 14);

	lcd_vcbus_setb(LDC_ADJ_VS_CTRL, 10, 0, 16);

	LDIMPR("%s ok\n", __func__);
}

static void ldc_rmem_seg_hist_get(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned char *buf, *p;
	unsigned int row, col, n, index, temp = 0;
	int i, j, k;

	if (!ldim_drv->seg_hist) {
		LDIMERR("%s: seg_hist buf is null\n", __func__);
		return;
	}

	buf = ldim_drv->rmem->seg_hist_vaddr;
	if (!buf)
		return;

	row = ldim_drv->conf->hist_row;
	col = ldim_drv->conf->hist_col;

	n = 6; /* 6bytes per seg */
	for (i = 0; i < row; i++) {
		p = buf + (i * 0x120);
		for (j = 0; j < col; j++) {
			temp = 0;
			for (k = 0; k < 3; k++)
				temp |= (p[j * 3 + k] << (k * 8));
			index = i * col + j;
			ldim_drv->seg_hist[j].min_index = temp & 0xfff;
			ldim_drv->seg_hist[j].max_index = (temp >> 12) & 0xfff;
			p = buf + 3;
			temp = 0;
			for (k = 0; k < 3; k++)
				temp |= (p[k] << (k * 8));
			ldim_drv->seg_hist[j].weight_avg_95 = temp & 0xfff;
			ldim_drv->seg_hist[j].weight_avg = (temp >> 12) & 0xfff;
		}
	}
}

static void ldc_rmem_duty_get(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned char *buf, *p;
	unsigned int row, col, n, zone_num, index;
	unsigned int temp = 0, m, max;
	int i, j, k;

	if (!ldim_drv->duty) {
		LDIMERR("%s: duty buf is null\n", __func__);
		return;
	}

	buf = ldim_drv->rmem->duty_vaddr;
	if (!buf)
		return;

	row = ldim_drv->conf->hist_row;
	col = ldim_drv->conf->hist_col;
	zone_num = row * col;

	n = 3; /* 3bytes 2 seg */
	for (i = 0; i < row; i++) {
		if (ldim_drv->hist_en == 0)
			pr_info("%s: row[%d]:\n", __func__, i);
		p = buf + (i * 0x50);
		for (j = 0; j < ((col + 1) / 2); j++) {
			max = (i + 1) * col;
			if (max > zone_num)
				break;
			temp = 0;
			for (k = 0; k < n; k++) {
				temp |= (p[j * n + k] << (k * 8));
				m = i * 0x50 + j * n + k;
				if (ldim_drv->hist_en == 0) {
					pr_info("    buf[0x%x]=0x%x, p[0x%x]=0x%x\n",
						m, buf[m], j * n + k, p[j * n + k]);
				}
			}
			index = i * col + j * 2;
			if (ldim_drv->hist_en == 0) {
				pr_info("  index=%d %d, zone_num=%d\n",
					index, index + 1, zone_num);
			}
			if (index >= max)
				break;
			ldim_drv->duty[index] = temp & 0xfff;
			if ((index + 1) >= max)
				break;
			ldim_drv->duty[index + 1] = (temp >> 12) & 0xfff;
		}
	}
}

void ldc_rmem_data_parse(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int size, i;

	if (ldim_drv->hist_en)
		return;

	ldc_rmem_seg_hist_get(ldim_drv);
	ldc_rmem_duty_get(ldim_drv);

	size = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	for (i = 0; i < size; i++)
		ldim_drv->local_bl_matrix[i] = (unsigned short)ldim_drv->duty[i];
}

void ldim_vs_arithmetic_t7(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int size, i;

	if (ldim_drv->hist_en == 0)
		return;

	ldc_rmem_seg_hist_get(ldim_drv);
	ldc_rmem_duty_get(ldim_drv);

	size = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	for (i = 0; i < size; i++)
		ldim_drv->local_bl_matrix[i] = (unsigned short)ldim_drv->duty[i];
}

void ldim_func_ctrl_t7(struct aml_ldim_driver_s *ldim_drv, int flag)
{
	if (flag) {
		ldim_drv->top_en = 1;
		ldim_drv->hist_en = 1;
		lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 1, 20, 1);
	} else {
		lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 0, 20, 1);
		ldim_drv->top_en = 0;
		ldim_drv->hist_en = 0;
	}

	LDIMPR("%s: %d\n", __func__, flag);
}

void ldim_func_ctrl_t3(struct aml_ldim_driver_s *ldim_drv, int flag)
{
	if (flag) {
		ldim_drv->top_en = 1;
		ldim_drv->hist_en = 1;
		lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 1, 20, 1);
	} else {
		lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 0, 20, 1);
		ldim_drv->top_en = 0;
		ldim_drv->hist_en = 0;

		/* refresh system brightness */
		ldim_drv->level_update = 1;
	}

	LDIMPR("%s: %d\n", __func__, flag);
}

void ldim_drv_init_t7(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int width, height, col_num, row_num;

	if (!ldim_drv)
		return;

	/*init */
	if (ldim_drv->dev_drv)
		ldim_profile_load(ldim_drv);

	width = ldim_drv->conf->hsize;
	height = ldim_drv->conf->vsize;
	col_num = ldim_drv->conf->hist_col;
	row_num = ldim_drv->conf->hist_row;

	lcd_vcbus_write(VPU_RDARB_MODE_L2C1, 0); //change ldc to vpu read0

	lcd_vcbus_write(LDC_REG_BLOCK_NUM, 0);
	lcd_vcbus_write(LDC_DDR_ADDR_BASE, (ldim_drv->rmem->profile_mem_paddr >> 2));
	ldc_set_t7(width, height, col_num, row_num);

	ldc_gain_lut_set_t7();

	LDIMPR("drv_init: col: %d, row: %d, axi paddr: 0x%lx\n",
		col_num, row_num, (unsigned long)ldim_drv->rmem->rsv_mem_paddr);
}

void ldim_drv_init_t3(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int width, height, col_num, row_num;

	if (!ldim_drv)
		return;

	/*init */
	if (ldim_drv->dev_drv)
		ldim_profile_load(ldim_drv);

	width = ldim_drv->conf->hsize;
	height = ldim_drv->conf->vsize;
	col_num = ldim_drv->conf->hist_col;
	row_num = ldim_drv->conf->hist_row;

	lcd_vcbus_write(LDC_REG_BLOCK_NUM, 0);
	lcd_vcbus_write(LDC_DDR_ADDR_BASE, (ldim_drv->rmem->profile_mem_paddr >> 2));
	ldc_set_t7(width, height, col_num, row_num);

	ldc_gain_lut_set_t3();

	LDIMPR("drv_init: col: %d, row: %d, axi paddr: 0x%lx\n",
		col_num, row_num, (unsigned long)ldim_drv->rmem->rsv_mem_paddr);
}
