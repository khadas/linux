// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * cm2_adj for pq module
 *
 * Copyright (c) 2017 powerqin <yong.qin@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/module.h>

/* module headers */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "arch/vpp_regs.h"
#include "cm2_adj.h"
#include "reg_helper.h"
#include "amve_v2.h"

#define NUM_MATRIX_PARAM 7
#define NUM_COLOR_MAX ecm2colormode_max
#define NUM_SMTH_PARAM 11

#define NUM_CM_14_COLOR_MAX cm_14_ecm2colormode_max

static uint lpf_coef_matrix_param = NUM_MATRIX_PARAM;
static uint lpf_coef[NUM_MATRIX_PARAM] = {
	0, 16, 32, 32, 32, 16, 0
};

static uint color_key_pts_matrix_param = NUM_COLOR_MAX;
static uint color_key_pts[NUM_COLOR_MAX] = {
	5, 9, 12, 15, 17, 19, 23, 25, 29
};

static uint color_start_param = NUM_COLOR_MAX;
static uint color_start[NUM_COLOR_MAX] = {
	1, 7, 11, 14, 16, 18, 23, 25, 29
};

static uint color_end_param = NUM_COLOR_MAX;
static uint color_end[NUM_COLOR_MAX] = {
	6, 10, 13, 15, 17, 22, 24, 28, 32
};

static uint cm_14_color_key_pts_matrix_param = NUM_CM_14_COLOR_MAX;
static uint cm_14_color_key_pts[NUM_CM_14_COLOR_MAX] = {
	2, 4, 7, 9, 11, 13, 14, 16, 18, 20, 22, 25, 28, 30
};

static uint cm_14_color_start_param = NUM_CM_14_COLOR_MAX;
static uint cm_14_color_start[NUM_CM_14_COLOR_MAX] = {
	1, 3, 6, 8, 10, 12, 14, 15, 17, 20, 22, 25, 28, 30
};

static uint cm_14_color_end_param = NUM_CM_14_COLOR_MAX;
static uint cm_14_color_end[NUM_CM_14_COLOR_MAX] = {
	2, 5, 7, 9, 11, 13, 14, 16, 19, 21, 24, 27, 29, 32
};

static uint smth_coef_hue_matrix_param = NUM_SMTH_PARAM;
static uint smth_coef_hue[NUM_SMTH_PARAM] = {
	0, 20, 40, 80, 110, 128, 110, 80, 40, 20, 0
};

static uint smth_coef_luma_matrix_param = NUM_SMTH_PARAM;
static uint smth_coef_luma[NUM_SMTH_PARAM] = {
	40, 100, 105, 110, 115, 120, 115, 110, 85, 60, 40
};

static uint smth_coef_sat_matrix_param = NUM_SMTH_PARAM;
static uint smth_coef_sat[NUM_SMTH_PARAM] = {
	40, 60, 85, 105, 115, 120, 115, 105, 85, 60, 30
};

static char adj_hue_via_s[NUM_CM_14_COLOR_MAX][5][32];
static char adj_hue_via_hue[NUM_CM_14_COLOR_MAX][32];
static char adj_sat_via_hs[NUM_CM_14_COLOR_MAX][3][32];
static char adj_luma_via_hue[NUM_CM_14_COLOR_MAX][32];

//static char def_sat_via_hs[3][32];
static char def_sat_via_hs[3][32];
static char def_hue_via_s[5][32];
static char def_hue_via_hue[32];
static char def_luma_via_hue[32];

//static char def_14_color_sat_via_hs[3][32];

module_param_array(lpf_coef, uint,
		   &lpf_coef_matrix_param, 0664);
MODULE_PARM_DESC(lpf_coef, "\n lpf_coef\n");

module_param_array(color_key_pts, uint,
		   &color_key_pts_matrix_param, 0664);
MODULE_PARM_DESC(color_key_pts, "\n color_key_pts\n");

module_param_array(color_start, uint,
		   &color_start_param, 0664);
MODULE_PARM_DESC(color_start, "\n color_start\n");

module_param_array(color_end, uint,
		   &color_end_param, 0664);
MODULE_PARM_DESC(color_end, "\n color_end\n");

module_param_array(cm_14_color_key_pts, uint,
		   &cm_14_color_key_pts_matrix_param, 0664);
MODULE_PARM_DESC(cm_14_color_key_pts, "\n cm_14_color_key_pts\n");

module_param_array(cm_14_color_start, uint,
		   &cm_14_color_start_param, 0664);
MODULE_PARM_DESC(cm_14_color_start, "\n 14 color_start\n");

module_param_array(cm_14_color_end, uint,
		   &cm_14_color_end_param, 0664);
MODULE_PARM_DESC(cm_14_color_end, "\n 14 color_end\n");

module_param_array(smth_coef_hue, uint,
		   &smth_coef_hue_matrix_param, 0664);
MODULE_PARM_DESC(smth_coef_hue_matrix_param, "\n smth_coef_hue\n");

module_param_array(smth_coef_luma, uint,
		   &smth_coef_luma_matrix_param, 0664);
MODULE_PARM_DESC(smth_coef_luma_matrix_param, "\n smth_coef_luma\n");

module_param_array(smth_coef_sat, uint,
		   &smth_coef_sat_matrix_param, 0664);
MODULE_PARM_DESC(smth_coef_sat_matrix_param, "\n smth_coef_sat\n");

/*static int lpf_coef[] = {
 *	0, 0, 32, 64, 32, 0, 0
 *};
 *static int color_key_pts[] = {
 *	4, 9, 12, 15, 19, 25, 31
 *};
 */

/*smth_coef= round([0.2 0.6 0.9 1 0.9 0.6 0.2]*128)*/
/*static int smth_coef[] = {26, 77, 115, 128, 115, 77, 26};*/

/*original value x100 */
static int huegain_via_sat5[NUM_CM_14_COLOR_MAX][5] = {
	{100, 100, 100, 100, 100},
	{30, 60, 80, 100, 100},
	{100, 100, 80, 50, 30},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
};

/*original value x100 */
static int satgain_via_sat3[NUM_CM_14_COLOR_MAX][3] = {
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
};

static int rsround(int val)
{
	if (val > 0)
		val = val + 50;
	else if (val < 0)
		val = val - 50;

	return val;
}

/**
 * [color_adj calculate lut table]
 * @param  inp_color     [0~6]
 * @param  inp_val       [-100~100]
 * @param  lpf_en        [1~0]
 * @param  lpf_coef      [tab]
 * @param  color_key_pts [tab]
 * @param  smth_coef     [tab]
 * @param  out_lut       [output lut]
 * @return               [fail, true]
 */
static void color_adj(int inp_color, int inp_val, int lpf_en,
		      int *lpf_coef, int *color_key_pts,
				int *smth_coef, int *out_lut)
{
	/*int smth_win = 7;*/
	int inp_val2, temp;
	int smth_val[11];
	int x, k;
	int kpt = 0;
	int varargin_1;

	inp_val2 = max(-128, min(127, inp_val));

	for (x = 0; x < NUM_SMTH_PARAM; x++) {
		if (inp_val2 > 0) {
			temp =
			((smth_coef[x] * inp_val2 * 100 / 128) + 50) / 100;
			smth_val[x] = temp;
		} else if (inp_val2 < 0) {
			temp =
			((smth_coef[x] * inp_val2 * 100 / 128) - 50) / 100;
			smth_val[x] = temp;
		} else {
			smth_val[x] =
			((smth_coef[x] * inp_val2 * 100 / 128)) / 100;
		}
	}

	if (inp_color >= 0 && (inp_color <= cm_14_ecm2colormode_max ||
		inp_color <= ecm2colormode_max))
		kpt = color_key_pts[inp_color];

	for (x = 0; x < NUM_SMTH_PARAM; x++) {
		inp_val2 = kpt + x - (NUM_SMTH_PARAM / 2);
		if (inp_val2 < 0)
			inp_val2 = 32 - abs(inp_val2);

		if (inp_val2 > 31)
			inp_val2 -= 32;

		out_lut[(int)inp_val2] = 0;

		/*  reset to 0 before changing?? */
		out_lut[(int)inp_val2] += smth_val[x];
	}

	if (lpf_en) {
		for (x = 0; x < 32; x++) {
			inp_val2 = 0;
			for (k = 0; k < NUM_MATRIX_PARAM; k++) {
				varargin_1 = (x + k) - (NUM_MATRIX_PARAM / 2);
				if (varargin_1 < 0)
					varargin_1 = 32 - abs(varargin_1);

				if (varargin_1 > 31)
					varargin_1 -= 32;

				inp_val2 += lpf_coef[k] *
					out_lut[varargin_1];
			}
			out_lut[x] = inp_val2 / 128;
		}
	}
}

/**
 * [cm2_curve_update_hue description]
 *	0:purple
 *	1:red
 *	2: skin
 *	3: yellow
 *	4: yellow green
 *	5: green
 *	6: green blue
 *	7: cyan
 *	8: blue
 * @param colormode [description]
 * @param Adj_Hue_via_S[][32]  [description]
 */
void cm2_curve_update_hue_by_hs(struct cm_color_md cm_color_md_hue_by_hs)
{
	unsigned int i, j, k, m, start = 0, end = 0;
	unsigned int val1[5] = {0}, val2[5] = {0};
	int temp, reg_node1, reg_node2;
	int colormode;
	int addr_port;
	int data_port;
	struct cm_port_s cm_port;

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		addr_port = cm_port.cm_addr_port[0];
		data_port = cm_port.cm_data_port[0];
	} else {
		addr_port = VPP_CHROMA_ADDR_PORT;
		data_port = VPP_CHROMA_DATA_PORT;
	}

	if (cm_color_md_hue_by_hs.color_type == cm_9_color) {
		colormode = cm_color_md_hue_by_hs.cm_9_color_md;
		start	= color_start[colormode];
		end		= color_end[colormode];
	} else {
		colormode = cm_color_md_hue_by_hs.cm_14_color_md;
		start	= cm_14_color_start[colormode];
		end		= cm_14_color_end[colormode];
	}

	if (cm_color_md_hue_by_hs.color_type == cm_9_color &&
		cm_color_md_hue_by_hs.cm_9_color_md == ecm2colormode_max) {
		pr_info("color_type:9 clr, cm_9_color_md=9 error, return!!!\n");
		return;
	} else if (cm_color_md_hue_by_hs.color_type == cm_14_color &&
		cm_color_md_hue_by_hs.cm_14_color_md == cm_14_ecm2colormode_max) {
		pr_info("color_type:14 clr, cm_14_color_md=14 error, return!!!\n");
		return;
	}

	if (cm2_debug & CM_HUE_BY_HIS_DEBUG_FLAG)
		pr_info("%s:color_type:%d,colormode:%d start:%d end:%d\n",
				__func__,
				cm_color_md_hue_by_hs.color_type,
				colormode, start, end);

	reg_node1 = (CM2_ENH_COEF2_H00 - 0x100) % 8;
	reg_node2 = (CM2_ENH_COEF3_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		k = i;
		if (k > 31)
			k = k  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(addr_port,
				      0x100 + k * 8 + j);
			val1[j] = READ_VPP_REG(data_port);

			if (j == reg_node1) {
				/*curve 0,1*/
				val1[j] &= 0x0000ffff;
				temp = (s8)adj_hue_via_s[colormode][0][k] +
					(s8)def_hue_via_s[0][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp << 16) & 0x00ff0000;

				temp = (s8)adj_hue_via_s[colormode][1][k] +
					(s8)def_hue_via_s[1][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp << 24) & 0xff000000;
				continue;
			}
		}

		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(addr_port,
				      0x100 + k * 8 + j);
			WRITE_VPP_REG(data_port, val1[j]);
		}

		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(addr_port,
				      0x100 + k * 8 + j);
			val2[j] = READ_VPP_REG(data_port);
			if (j == reg_node2) {
				/*curve 2,3,4*/
				val2[j] &= 0xff000000;
				temp = (s8)adj_hue_via_s[colormode][2][k] +
					(s8)def_hue_via_s[2][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val2[j] |= temp & 0x000000ff;

				temp = (s8)adj_hue_via_s[colormode][3][k] +
					(s8)def_hue_via_s[3][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val2[j] |= (temp << 8) & 0x0000ff00;

				temp = (s8)adj_hue_via_s[colormode][4][k] +
					(s8)def_hue_via_s[4][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val2[j] |= (temp << 16) & 0x00ff0000;
				continue;
			}
			/*pr_info("0x%x, 0x%x\n", val1, val2);*/
		}

		if (chip_type_id == chip_s5) {
			for (m = SLICE0; m < SLICE_MAX; m++) {
				addr_port = cm_port.cm_addr_port[m];
				data_port = cm_port.cm_data_port[m];
				for (j = 0; j < 5; j++) {
					if (cm2_debug & CM_HUE_BY_HIS_DEBUG_FLAG)
						pr_info("%s:val1[%d]:%d\n",
							__func__, j, val1[j]);
					WRITE_VPP_REG(addr_port,
						      0x100 + k * 8 + j);
					WRITE_VPP_REG(data_port, val2[j]);
				}
			}
		} else {
			for (j = 0; j < 5; j++) {
				if (cm2_debug & CM_HUE_BY_HIS_DEBUG_FLAG)
					pr_info("%s:val1[%d]:%d\n",
						__func__, j, val1[j]);
				WRITE_VPP_REG(addr_port,
					      0x100 + k * 8 + j);
				WRITE_VPP_REG(data_port, val2[j]);
			}
		}
	}
}

void cm2_curve_update_hue(struct cm_color_md cm_color_md_hue)
{
	unsigned int i, j, k, m, start = 0, end = 0;
	unsigned int val1[5] = {0};
	int temp = 0, reg_node;
	int colormode;
	int addr_port;
	int data_port;
	struct cm_port_s cm_port;

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		addr_port = cm_port.cm_addr_port[0];
		data_port = cm_port.cm_data_port[0];
	} else {
		addr_port = VPP_CHROMA_ADDR_PORT;
		data_port = VPP_CHROMA_DATA_PORT;
	}

	if (cm_color_md_hue.color_type == cm_9_color) {
		colormode = cm_color_md_hue.cm_9_color_md;
		start	= color_start[colormode];
		end		= color_end[colormode];
	} else {
		colormode = cm_color_md_hue.cm_14_color_md;
		start	= cm_14_color_start[colormode];
		end		= cm_14_color_end[colormode];
	}

	if (cm_color_md_hue.color_type == cm_9_color &&
		cm_color_md_hue.cm_9_color_md == ecm2colormode_max) {
		pr_info("color_type:9 clr, cm_9_color_md=9 error, return!!!\n");
		return;
	} else if (cm_color_md_hue.color_type == cm_14_color &&
		cm_color_md_hue.cm_14_color_md == cm_14_ecm2colormode_max) {
		pr_info("color_type:14 clr, cm_14_color_md=14 error, return!!!\n");
		return;
	}

	if (cm2_debug & CM_HUE_DEBUG_FLAG)
		pr_info("%s:color_type:%d, colormode:%d start:%d end:%d\n",
			__func__,
			cm_color_md_hue.color_type,
			colormode, start, end);

	reg_node = (CM2_ENH_COEF1_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		k = i;
		if (k > 31)
			k = k  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(addr_port,
				      0x100 + k * 8 + j);
			val1[j] = READ_VPP_REG(data_port);
			if (j == reg_node) {
				/*curve 0*/
				val1[j] &= 0xffffff00;
				temp = (s8)adj_hue_via_hue[colormode][k] +
					(s8)def_hue_via_hue[k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp) & 0x000000ff;
				continue;
			}
		}
		if (chip_type_id == chip_s5) {
			for (m = SLICE0; m < SLICE_MAX; m++) {
				addr_port = cm_port.cm_addr_port[m];
				data_port = cm_port.cm_data_port[m];
				for (j = 0; j < 5; j++) {
					if (cm2_debug & CM_HUE_DEBUG_FLAG)
						pr_info("%s:val1[%d]:%d\n",
							__func__, j, val1[j]);
					WRITE_VPP_REG(addr_port,
						      0x100 + k * 8 + j);
					WRITE_VPP_REG(data_port, val1[j]);
				}
			}
		} else {
			for (j = 0; j < 5; j++) {
				if (cm2_debug & CM_HUE_DEBUG_FLAG)
					pr_info("%s:val1[%d]:%d\n",
						__func__, j, val1[j]);
				WRITE_VPP_REG(addr_port,
					      0x100 + k * 8 + j);
				WRITE_VPP_REG(data_port, val1[j]);
			}
		}
	}
}

/**
 * [cm2_curve_update_luma description]
 *	0:purple
 *	1:red
 *	2: skin
 *	3: yellow
 *	4: yellow green
 *	5: green
 *	6: green blue
 *	7: cyan
 *	8: blue
 * @param colormode [description]
 * @param luma_lut  [description]
 */
void cm2_curve_update_luma(struct cm_color_md cm_color_md_luma)
{
	unsigned int i, j, k, m, start = 0, end = 0;
	unsigned int val1[5] = {0};
	int temp = 0, reg_node;
	int colormode;
	int addr_port;
	int data_port;
	struct cm_port_s cm_port;

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		addr_port = cm_port.cm_addr_port[0];
		data_port = cm_port.cm_data_port[0];
	} else {
		addr_port = VPP_CHROMA_ADDR_PORT;
		data_port = VPP_CHROMA_DATA_PORT;
	}

	if (cm_color_md_luma.color_type == cm_9_color) {
		colormode = cm_color_md_luma.cm_9_color_md;
		start	= color_start[colormode];
		end		= color_end[colormode];
	} else {
		colormode = cm_color_md_luma.cm_14_color_md;
		start	= cm_14_color_start[colormode];
		end		= cm_14_color_end[colormode];
	}

	if (cm_color_md_luma.color_type == cm_9_color &&
		cm_color_md_luma.cm_9_color_md == ecm2colormode_max) {
		pr_info("color_type:9 clr, cm_9_color_md=9 error, return!!!\n");
		return;
	} else if (cm_color_md_luma.color_type == cm_14_color &&
		cm_color_md_luma.cm_14_color_md == cm_14_ecm2colormode_max) {
		pr_info("color_type:14 clr, cm_14_color_md=14 error, return!!!\n");
		return;
	}

	if (cm2_debug & CM_LUMA_DEBUG_FLAG)
		pr_info("%s:color_type:%d colormode:%d start:%d end:%d\n",
			__func__,
			cm_color_md_luma.color_type,
			colormode, start, end);

	reg_node = (CM2_ENH_COEF0_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		k = i;
		if (k > 31)
			k = k  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(addr_port,
				      CM2_ENH_COEF0_H00 + k * 8 + j);
			val1[j] = READ_VPP_REG(data_port);
			if (j == reg_node) {
				/*curve 0*/
				val1[j] &= 0xffffff00;
				temp = (s8)adj_luma_via_hue[colormode][k] +
					(s8)def_luma_via_hue[k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp) & 0x000000ff;
				continue;
			}
		}
		if (chip_type_id == chip_s5) {
			for (m = SLICE0; m < SLICE_MAX; m++) {
				addr_port = cm_port.cm_addr_port[m];
				data_port = cm_port.cm_data_port[m];
				for (j = 0; j < 5; j++) {
					if (cm2_debug & CM_LUMA_DEBUG_FLAG)
						pr_info("%s:val1[%d]:%d\n",
							__func__, j, val1[j]);
					WRITE_VPP_REG(addr_port,
						      CM2_ENH_COEF0_H00 + k * 8 + j);
					WRITE_VPP_REG(data_port, val1[j]);
				}
			}
		} else {
			for (j = 0; j < 5; j++) {
				if (cm2_debug & CM_LUMA_DEBUG_FLAG)
					pr_info("%s:val1[%d]:%d\n",
						__func__, j, val1[j]);
				WRITE_VPP_REG(addr_port,
					      CM2_ENH_COEF0_H00 + k * 8 + j);
				WRITE_VPP_REG(data_port, val1[j]);
			}
		}
	}
}

/**
 * [cm2_curve_update_sat description]
 *	0:purple
 *	1:red
 *	2: skin
 *	3: yellow
 *	4: yellow green
 *	5: green
 *	6: green blue
 *	7: cyan
 *	8: blue
 * @param colormode [description]
 * @param Adj_Sat_via_HS[3][32]  [description]
 */
void cm2_curve_update_sat(struct cm_color_md cm_color_md_sat)
{
	unsigned int i, j, k, m, start = 0, end = 0;
	unsigned int val1[5] = {0};
	int temp = 0, reg_node;
	int colormode;
	int addr_port;
	int data_port;
	struct cm_port_s cm_port;

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		addr_port = cm_port.cm_addr_port[0];
		data_port = cm_port.cm_data_port[0];
	} else {
		addr_port = VPP_CHROMA_ADDR_PORT;
		data_port = VPP_CHROMA_DATA_PORT;
	}

	if (cm_color_md_sat.color_type == cm_9_color) {
		colormode = cm_color_md_sat.cm_9_color_md;
		start	= color_start[colormode];
		end		= color_end[colormode];
	} else {
		colormode = cm_color_md_sat.cm_14_color_md;
		start	= cm_14_color_start[colormode];
		end		= cm_14_color_end[colormode];
	}

	if (cm_color_md_sat.color_type == cm_9_color &&
		cm_color_md_sat.cm_9_color_md == ecm2colormode_max) {
		pr_info("color_type:9 clr, cm_9_color_md=9 error, return!!!\n");
		return;
	} else if (cm_color_md_sat.color_type == cm_14_color &&
		cm_color_md_sat.cm_14_color_md == cm_14_ecm2colormode_max) {
		pr_info("color_type:14 clr, cm_14_color_md=14 error, return!!!\n");
		return;
	}

	if (cm2_debug & CM_SAT_DEBUG_FLAG)
		pr_info("%s:color_type:%d colormode:%d, start:%d, end:%d\n",
			__func__,
			cm_color_md_sat.color_type, colormode, start, end);

	reg_node = (CM2_ENH_COEF0_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		k = i;
		if (k > 31)
			k = k  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(addr_port,
				      CM2_ENH_COEF0_H00 + k * 8 + j);
			val1[j] = READ_VPP_REG(data_port);
			if (j == reg_node) {
				val1[j] &= 0x000000ff;
				/*curve 0*/
				temp = (s8)adj_sat_via_hs[colormode][0][k] +
					(s8)def_sat_via_hs[0][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp << 8) & 0x0000ff00;

				/*curve 1*/
				temp = (s8)adj_sat_via_hs[colormode][1][k] +
					(s8)def_sat_via_hs[1][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp << 16) & 0x00ff0000;

				/*curve 2*/
				temp = (s8)adj_sat_via_hs[colormode][2][k] +
					(s8)def_sat_via_hs[2][k];
				if (temp > 127)
					temp = 127;
				else if (temp < -128)
					temp = -128;
				val1[j] |= (temp << 24) & 0xff000000;
				continue;
			}
		}
		if (chip_type_id == chip_s5) {
			for (m = SLICE0; m < SLICE_MAX; m++) {
				addr_port = cm_port.cm_addr_port[m];
				data_port = cm_port.cm_data_port[m];
				for (j = 0; j < 5; j++) {
					if (cm2_debug & CM_SAT_DEBUG_FLAG)
						pr_info("%s:val1[%d]:%d\n",
							__func__, j, val1[j]);

					WRITE_VPP_REG(addr_port,
						      CM2_ENH_COEF0_H00 + k * 8 + j);
					WRITE_VPP_REG(data_port, val1[j]);
				}
			}
		} else {
			for (j = 0; j < 5; j++) {
				if (cm2_debug & CM_SAT_DEBUG_FLAG)
					pr_info("%s:val1[%d]:%d\n",
						__func__, j, val1[j]);

				WRITE_VPP_REG(addr_port,
					      CM2_ENH_COEF0_H00 + k * 8 + j);
				WRITE_VPP_REG(data_port, val1[j]);
			}
		}
	}
}

/**
 * get saturation default value
 *	0:purple
 *	1:red
 *	2: skin
 *	3: yellow
 *	4: yellow green
 *	5: green
 *	6: green blue
 *	7: cyan
 *	8: blue
 * @param colormode [description]
 * @param def_sat_via_hs[3][32]  [description]
 */
void default_sat_param(unsigned int reg, unsigned int value)
{
	unsigned int i;

	if (reg < CM2_ENH_COEF0_H00 || reg >= 0x200)
		return;

	if ((reg - CM2_ENH_COEF0_H00) % 8 == 0) {
		i = (reg - CM2_ENH_COEF0_H00) / 8;
		def_luma_via_hue[i] = value & 0xff;
		def_sat_via_hs[0][i] = (value >> 8) & 0xff;
		def_sat_via_hs[1][i] = (value >> 16) & 0xff;
		def_sat_via_hs[2][i] = (value >> 24) & 0xff;
	}

	if ((reg - CM2_ENH_COEF1_H00) % 8 == 0) {
		i = (reg - CM2_ENH_COEF1_H00) / 8;
		def_hue_via_hue[i] = value & 0xff;
	}

	if ((reg - CM2_ENH_COEF2_H00) % 8 == 0) {
		i = (reg - CM2_ENH_COEF2_H00) / 8;
		def_hue_via_s[0][i] = (value >> 16) & 0xff;
		def_hue_via_s[1][i] = (value >> 24) & 0xff;
	}

	if ((reg - CM2_ENH_COEF3_H00) % 8 == 0) {
		i = (reg - CM2_ENH_COEF3_H00) / 8;
		def_hue_via_s[2][i] = value & 0xff;
		def_hue_via_s[3][i] = (value >> 8) & 0xff;
		def_hue_via_s[4][i] = (value >> 16) & 0xff;
	}
}

/**
 * [cm2_luma adj cm2 Hue offset for each four pieces Saturation region]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */
void cm2_hue_by_hs(struct cm_color_md cm_color_mode, int hue_val, int lpf_en)
{
	int inp_color;
	/*[-100, 100], color_adj will mapping to value [-128, 127]*/
	int inp_val = hue_val;
	int temp;
	int out_lut[32];
	/*int lpf_en = 0;*/
	int k, i;

	memset(out_lut, 0, sizeof(int) * 32);
	/*pr_info("color mode:%d, input val =%d\n", colormode, hue_val);*/

	if (cm_color_mode.color_type == cm_9_color) {
		inp_color = cm_color_mode.cm_9_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef,
			  color_key_pts, smth_coef_hue, out_lut);
	} else {
		inp_color = cm_color_mode.cm_14_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef,
			  cm_14_color_key_pts, smth_coef_hue, out_lut);
	}

	memset(adj_hue_via_s[inp_color], 0, sizeof(char) * 32 * 5);

	if (cm2_debug & CM_HUE_BY_HIS_DEBUG_FLAG)
		pr_info("%s: color mode:%d, inp_val:%d lpf_en:%d\n",
			__func__,
			inp_color, hue_val, lpf_en);

	for (k = 0; k < 5; k++) {
		/*pr_info("\n Adj_Hue via %d\n", k);*/
		for (i = 0; i < 32; i++) {
			temp = out_lut[i] * huegain_via_sat5[inp_color][k];
			adj_hue_via_s[inp_color][k][i] =
						(char)(rsround(temp) / 100);
			if (cm2_debug & CM_HUE_BY_HIS_DEBUG_FLAG)
				pr_info("adj_hue_via_s[%d][%d][%d] =%d\n",
				inp_color, k, i,
				adj_hue_via_s[inp_color][k][i]);
			/*pr_info("%d  ", reg_CM2_Adj_Hue_via_S[k][i]);*/
		}
	}
	/*pr_info("\n ---end\n");*/
}

/**
 * [cm2_luma adj cm2 Hue offset for each four pieces Saturation region]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */

void cm2_hue(struct cm_color_md cm_color_mode, int hue_val, int lpf_en)
{
	int inp_color;
	/*[-100, 100], color_adj will mapping to value [-128, 127]*/
	int inp_val = hue_val;
	int i;
	int out_lut[32];
	/*int lpf_en = 0;*/

	memset(out_lut, 0, sizeof(int) * 32);

	if (cm_color_mode.color_type == cm_9_color) {
		inp_color = cm_color_mode.cm_9_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef,
			  color_key_pts, smth_coef_hue, out_lut);
	} else {
		inp_color = cm_color_mode.cm_14_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef,
			  cm_14_color_key_pts, smth_coef_hue, out_lut);
	}

	memset(adj_hue_via_hue[inp_color], 0, sizeof(char) * 32);

	if (cm2_debug & CM_HUE_DEBUG_FLAG)
		pr_info("%s: color mode:%d, input val =%d lpf_en:%d\n",
			__func__,
			inp_color, hue_val, lpf_en);

	for (i = 0; i < 32; i++) {
		adj_hue_via_hue[inp_color][i] = (char)out_lut[i];
		if (cm2_debug & CM_HUE_DEBUG_FLAG)
			pr_info("adj_hue_via_hue[%d][%d] =%d\n",
				inp_color, i,
				adj_hue_via_hue[inp_color][i]);
	}
}

/**
 * [cm2_luma adj cm2 Luma offsets for Hue section]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */
void cm2_luma(struct cm_color_md cm_color_mode, int luma_val, int lpf_en)
{
	int out_luma_lut[32];
	int i;
	int inp_color;
	int inp_val = luma_val;

	memset(out_luma_lut, 0, sizeof(int) * 32);

	if (cm_color_mode.color_type == cm_9_color) {
		inp_color = cm_color_mode.cm_9_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef, color_key_pts,
			  smth_coef_luma, out_luma_lut);
	} else {
		inp_color = cm_color_mode.cm_14_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef,
			cm_14_color_key_pts, smth_coef_luma, out_luma_lut);
	}

	memset(adj_luma_via_hue[inp_color], 0, sizeof(char) * 32);

	if (cm2_debug & CM_LUMA_DEBUG_FLAG)
		pr_info("%s: color mode:%d, inp_val =%d lpf_en:%d\n",
			__func__,
			inp_color, inp_val, lpf_en);

	for (i = 0; i < 32; i++) {
		adj_luma_via_hue[inp_color][i] = (char)out_luma_lut[i];
		if (cm2_debug & CM_LUMA_DEBUG_FLAG)
			pr_info("adj_luma_via_hue[%d][%d] =%d\n", inp_color, i,
				adj_luma_via_hue[inp_color][i]);
		/*pr_info("%d,", out_luma_lut[i]);*/
	}

	/*pr_info("\n---end\n");*/
}

/**
 * [cm2_sat adj cm2 saturation gain offset]
 * @param colormode [struct cm_color_md]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */
void cm2_sat(struct cm_color_md cm_color_mode, int sat_val, int lpf_en)
{
	int inp_color;
	int inp_val = sat_val;

	int out_sat_lut[32];
	int k, i;
	int temp;

	memset(out_sat_lut, 0, sizeof(int) * 32);

	if (cm_color_mode.color_type == cm_9_color) {
		inp_color = cm_color_mode.cm_9_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef, color_key_pts,
			  smth_coef_sat, out_sat_lut);
	} else {
		inp_color = cm_color_mode.cm_14_color_md;
		color_adj(inp_color, inp_val, lpf_en, lpf_coef,
			cm_14_color_key_pts, smth_coef_sat, out_sat_lut);
	}

	memset(adj_sat_via_hs[inp_color], 0, sizeof(char) * 32 * 3);

	if (cm2_debug & CM_SAT_DEBUG_FLAG)
		pr_info("%s: color mode:%d, input val =%d lpf_en:%d\n",
			__func__,
			inp_color, inp_val, lpf_en);

	for (k = 0; k < 3; k++) {
		for (i = 0; i < 32; i++) {
			temp = out_sat_lut[i] * satgain_via_sat3[inp_color][k];
			adj_sat_via_hs[inp_color][k][i] =
				(char)(rsround(temp) / 100);
			if (cm2_debug & CM_SAT_DEBUG_FLAG)
				pr_info("adj_sat_via_hs[%d][%d][%d] =%d\n",
					inp_color, k, i,
					adj_sat_via_hs[inp_color][k][i]);
		}
	}
}

