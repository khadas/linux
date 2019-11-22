/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_hw.c
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

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/amlogic/iomap.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include "ldim_drv.h"
#include "ldim_reg.h"

#define Wr(reg, val)    Wr_reg(reg, val)
#define Rd(reg)         Rd_reg(reg)

static int LDIM_Update_Matrix(int NewBlMatrix[], int BlMatrixNum)
{
	int data;

	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	if (data & (1 << 12)) {  /*bl_ram_mode=1;*/
		if (LDIM_RD_32Bits(REG_LD_BLMAT_RAM_MISC) & 0x10000)
			/*Previous Matrix is not used*/
			goto Previous_Matrix;
		else {
			LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
				NewBlMatrix, BlMatrixNum);
			LDIM_WR_32Bits(REG_LD_BLMAT_RAM_MISC,
				(BlMatrixNum & 0x1ff) | (1 << 16));
			/*set Matrix update ready*/

			return 0;
		}
	} else {  /*bl_ram_mode=0*/
		/*set ram_clk_sel=0, ram_bus_sel = 0*/
		data = data & (~(3 << 9));
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			NewBlMatrix, BlMatrixNum);
		data = data | (3 << 9); /*set ram_clk_sel=1, ram_bus_sel = 1*/
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

		return 0;
	}

Previous_Matrix:
	return 1;
}

void ldim_initial_txlx(struct LDReg_s *nPRM,
		unsigned int ldim_bl_en, unsigned int ldim_hvcnt_bypass)
{
	unsigned int i;
	unsigned int data;
	unsigned int *arrayTmp;

	arrayTmp = kcalloc(1536, sizeof(unsigned int), GFP_KERNEL);
	if (arrayTmp == NULL) {
		LDIMERR("%s malloc error\n", __func__);
		return;
	}

	/*  LD_FRM_SIZE  */
	data = ((nPRM->reg_LD_pic_RowMax & 0xfff)<<16) |
		(nPRM->reg_LD_pic_ColMax & 0xfff);
	LDIM_WR_32Bits(REG_LD_FRM_SIZE, data);

	/* LD_RGB_MOD */
	data = ((0 & 0xfff)                               << 20) |
		((nPRM->reg_LD_RGBmapping_demo & 0x1)      << 19) |
		((nPRM->reg_LD_X_LUT_interp_mode[2] & 0x1) << 18) |
		((nPRM->reg_LD_X_LUT_interp_mode[1] & 0x1) << 17) |
		((nPRM->reg_LD_X_LUT_interp_mode[0] & 0x1) << 16) |
		((0 & 0x1) << 15) |
		((nPRM->reg_LD_BkLit_LPFmod & 0x7) << 12) |
		((nPRM->reg_LD_Litshft & 0x7)      << 8)  |
		((nPRM->reg_LD_BackLit_Xtlk & 0x1) << 7)  |
		((nPRM->reg_LD_BkLit_Intmod & 0x1) << 6)  |
		((nPRM->reg_LD_BkLUT_Intmod & 0x1) << 5)  |
		((nPRM->reg_LD_BkLit_curmod & 0x1) << 4)  |
		((nPRM->reg_LD_BackLit_mode & 0x3));
	LDIM_WR_32Bits(REG_LD_RGB_MOD, data);

	/* LD_BLK_HVNUM  */
	data = ((nPRM->reg_LD_Reflect_Vnum & 0x7)  << 20) |
		((nPRM->reg_LD_Reflect_Hnum & 0x7) << 16) |
		((nPRM->reg_LD_BLK_Vnum & 0x3f)    <<  8) |
		((nPRM->reg_LD_BLK_Hnum & 0x3f));
	LDIM_WR_32Bits(REG_LD_BLK_HVNUM, data);

	/* LD_HVGAIN */
	data = ((nPRM->reg_LD_Vgain & 0xfff) << 16) |
		(nPRM->reg_LD_Hgain & 0xfff);
	LDIM_WR_32Bits(REG_LD_HVGAIN, data);

	/*  LD_BKLIT_VLD  */
	data = 0;
	for (i = 0; i < 32; i++)
		if (nPRM->reg_LD_BkLit_valid[i])
			data = data | (1 << i);
	LDIM_WR_32Bits(REG_LD_BKLIT_VLD, data);

	/* LD_BKLIT_PARAM */
	data = ((nPRM->reg_LD_BkLit_Celnum & 0xff) << 16) |
		(nPRM->reg_BL_matrix_AVG & 0xfff);
	LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);

	/*  LD_LIT_GAIN_COMP */
	data = ((nPRM->reg_LD_Litgain & 0xfff) << 16) |
		(nPRM->reg_BL_matrix_Compensate & 0xfff);
	LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);

	/* LD_FRM_RST_POS */
	data = (1 << 16) | (5); /* h=1,v=5 :ldim_param_frm_rst_pos */
	LDIM_WR_32Bits(REG_LD_FRM_RST_POS, data);

	/* LD_FRM_BL_START_POS */
	data = (1 << 16) | (6); /* ldim_param_frm_bl_start_pos; */
	LDIM_WR_32Bits(REG_LD_FRM_BL_START_POS, data);

	/* REG_LD_FRM_HBLAN_VHOLS  */
	data = ((nPRM->reg_LD_LUT_VHo_LS & 0x7) << 16) |
		((6 & 0x1fff)) ;  /*frm_hblank_num */
	LDIM_WR_32Bits(REG_LD_FRM_HBLAN_VHOLS, data);

	/* REG_LD_XLUT_DEMO_ROI_XPOS */
	data = ((nPRM->reg_LD_xlut_demo_roi_xend & 0x1fff) << 16) |
		(nPRM->reg_LD_xlut_demo_roi_xstart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_XPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_YPOS */
	data = ((nPRM->reg_LD_xlut_demo_roi_yend & 0x1fff) << 16) |
		(nPRM->reg_LD_xlut_demo_roi_ystart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_YPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_CTRL */
	data = ((nPRM->reg_LD_xlut_oroi_enable & 0x1) << 1) |
		(nPRM->reg_LD_xlut_iroi_enable & 0x1);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_CTRL, data);

	/*LD_BLMAT_RAM_MISC*/
	LDIM_WR_32Bits(REG_LD_BLMAT_RAM_MISC, 384 & 0x1ff);

	/*  X_idx: 12*16  */
	LDIM_WR_BASE_LUT(REG_LD_RGB_IDX_BASE, nPRM->X_idx[0], 16, 16);

	/* X_nrm[16]: 4 * 16 */
	LDIM_WR_BASE_LUT(REG_LD_RGB_NRMW_BASE_TXLX, nPRM->X_nrm[0], 4, 16);

	/*reg_LD_BLK_Hidx[33]: 14*33 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_HIDX_BASE_TXLX,
			nPRM->reg_LD_BLK_Hidx, 16, 33);

	/* reg_LD_BLK_Vidx[25]: 14*25 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_VIDX_BASE_TXLX,
			nPRM->reg_LD_BLK_Vidx, 16, 25);

	/* reg_LD_LUT_VHk_pos[32]/reg_LD_LUT_VHk_neg[32]: u8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM->reg_LD_LUT_VHk_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM->reg_LD_LUT_VHk_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_NEGPOS_BASE_TXLX, arrayTmp, 8, 64);

	/* reg_LD_LUT_VHo_pos[32]/reg_LD_LUT_VHo_neg[32]: s8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM->reg_LD_LUT_VHo_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM->reg_LD_LUT_VHo_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHO_NEGPOS_BASE_TXLX, arrayTmp, 8, 64);

	/* reg_LD_LUT_HHk[32]:u8 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_HHK_BASE_TXLX, nPRM->reg_LD_LUT_HHk, 8, 32);

	/*reg_LD_Reflect_Hdgr[20],reg_LD_Reflect_Vdgr[20],
	 *	reg_LD_Reflect_Xdgr[4]
	 */
	for (i = 0; i < 20; i++)
		arrayTmp[i] = nPRM->reg_LD_Reflect_Hdgr[i];
	for (i = 0; i < 20; i++)
		arrayTmp[20+i] = nPRM->reg_LD_Reflect_Vdgr[i];
	for (i = 0; i < 4; i++)
		arrayTmp[40+i] = nPRM->reg_LD_Reflect_Xdgr[i];
	LDIM_WR_BASE_LUT(REG_LD_REFLECT_DGR_BASE_TXLX, arrayTmp, 8, 44);

	/*reg_LD_LUT_Hdg_LEXT[8]/reg_LD_LUT_Vdg_LEXT[8]/reg_LD_LUT_VHk_LEXT[8]*/
	for (i = 0; i < 8; i++)
		arrayTmp[i] = (nPRM->reg_LD_LUT_Hdg_LEXT_TXLX[i] & 0x3ff) |
			((nPRM->reg_LD_LUT_VHk_LEXT_TXLX[i] & 0x3ff) << 10) |
			((nPRM->reg_LD_LUT_Vdg_LEXT_TXLX[i] & 0x3ff) << 20);
	LDIM_WR_BASE_LUT_DRT(REG_LD_LUT_LEXT_BASE_TXLX, arrayTmp, 8);

	/*reg_LD_LUT_Hdg[8][32]: u10*8*32*/
	LDIM_WR_BASE_LUT(REG_LD_LUT_HDG_BASE_TXLX,
		nPRM->reg_LD_LUT_Hdg_TXLX[0], 16, 8*32);

	/*reg_LD_LUT_Vdg[8][32]: u10*8*32*/
	LDIM_WR_BASE_LUT(REG_LD_LUT_VDG_BASE_TXLX,
		nPRM->reg_LD_LUT_Vdg_TXLX[0], 16, 8*32);

	/*reg_LD_LUT_VHk[8][32]: u10*8*32*/
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_BASE_TXLX,
		nPRM->reg_LD_LUT_VHk_TXLX[0], 16, 8*32);

	/*reg_LD_LUT_Id[16][24]: u3*16*24=u3*384 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_ID_BASE_TXLX, nPRM->reg_LD_LUT_Id, 4, 384);

	/*enable the CBUS configure the RAM*/
	/*LD_MISC_CTRL0  {reg_blmat_ram_mode,
	 *1'h0,ram_bus_sel,ram_clk_sel,ram_clk_gate_en,
	 *2'h0,reg_hvcnt_bypass,reg_demo_synmode,reg_ldbl_synmode,
	 *reg_ldim_bl_en,soft_bl_start,reg_soft_rst)
	 */
	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = (data & (~(3 << 9))) | (1 << 8);
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

	/*X_lut[3][16][16]*/
	LDIM_WR_BASE_LUT_DRT(REG_LD_RGB_LUT_BASE, nPRM->X_lut2[0][0], 3*16*16);

	data = 0 | (0 << 1) | ((ldim_bl_en & 0x1) << 2) |
		(ldim_hvcnt_bypass << 5) | (1 << 8) |
		(3 << 9) | (1 << 12);
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

	LDIM_Update_Matrix(nPRM->BL_matrix, 16 * 24);

	kfree(arrayTmp);
}

static unsigned int ldim_hw_reg_dump_table[] = {
	LDIM_STTS_GCLK_CTRL0,
	LDIM_STTS_WIDTHM1_HEIGHTM1,
	LDIM_STTS_CTRL0,
	LDIM_STTS_HIST_REGION_IDX,
};

int ldim_hw_reg_dump(char *buf)
{
	unsigned int reg, data32;
	int i, size, len = 0;

	size = sizeof(ldim_hw_reg_dump_table) / sizeof(unsigned int);
	for (i = 0; i < size; i++) {
		reg = ldim_hw_reg_dump_table[i];
		data32 = Rd(reg);
		len += sprintf(buf+len, "0x%x = 0x%08x\n", reg, data32);
	}

	data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
	Wr(LDIM_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
	for (i = 0; i < 23; i++) {
		reg = LDIM_STTS_HIST_SET_REGION;
		data32 = Rd(reg);
		len += sprintf(buf+len, "0x%x = 0x%08x\n", reg, data32);
	}

	return len;
}

/***** local dimming stts functions begin *****/
/*hist mode: 0: comp0 hist only, 1: Max(comp0,1,2) for hist,
 *2: the hist of all comp0,1,2 are calculated
 */
/*lpf_en   1: 1,2,1 filter on before finding max& hist*/
/*rd_idx_auto_inc_mode 0: no self increase, 1: read index increase after
 *read a 25/48 block, 2: increases every read and lock sub-idx
 */
/*one_ram_en 1: one ram mode;  0:double ram mode*/
static void ldim_stts_en(unsigned int resolution, unsigned int pix_drop_mode,
	unsigned int eol_en, unsigned int hist_mode, unsigned int lpf_en,
	unsigned int rd_idx_auto_inc_mode, unsigned int one_ram_en)
{
	int data32;

	Wr(LDIM_STTS_GCLK_CTRL0, 0x0);
	Wr(LDIM_STTS_WIDTHM1_HEIGHTM1, resolution);

	data32 = 0x80000000 | ((pix_drop_mode & 0x3) << 29);
	data32 = data32 | ((eol_en & 0x1) << 28);
	data32 = data32 | ((one_ram_en & 0x1) << 27);
	data32 = data32 | ((hist_mode & 0x3) << 22);
	data32 = data32 | ((lpf_en & 0x1) << 21);
	data32 = data32 | ((rd_idx_auto_inc_mode & 0xff) << 14);
	Wr(LDIM_STTS_HIST_REGION_IDX, data32);
}

static void ldim_stts_set_region_txlx(unsigned int resolution,
		unsigned int blk_height, unsigned int blk_width,
		unsigned int row_start, unsigned int col_start,
		unsigned int blk_hnum)
{
	unsigned int hend0, hend1, hend2, hend3, hend4, hend5,
			hend6, hend7, hend8, hend9, hend10, hend11, hend12,
			hend13, hend14, hend15, hend16, hend17, hend18,
			hend19, hend20, hend21, hend22, hend23;
	unsigned int vend0, vend1, vend2, vend3, vend4, vend5,
			vend6, vend7, vend8, vend9, vend10, vend11,
			vend12, vend13, vend14, vend15;
	unsigned int data32, k, h_index[24], v_index[16];

	if (resolution == 0) {
		h_index[0] = col_start + blk_width - 1;
		for (k = 1; k < 24; k++) {
			h_index[k] = h_index[k-1] + blk_width;
			if (h_index[k] > 4095)
				h_index[k] = 4095; /* clip U12 */
		}
		v_index[0] = row_start + blk_height - 1;
		for (k = 1; k < 16; k++) {
			v_index[k] = v_index[k-1] + blk_height;
			if (v_index[k] > 4095)
				v_index[k] = 4095; /* clip U12 */
		}
		hend0 = h_index[0];/*col_start + blk_width - 1;*/
		hend1 = h_index[1];/*hend0 + blk_width;*/
		hend2 = h_index[2];/*hend1 + blk_width;*/
		hend3 = h_index[3];/*hend2 + blk_width;*/
		hend4 = h_index[4];/*hend3 + blk_width;*/
		hend5 = h_index[5];/*hend4 + blk_width;*/
		hend6 = h_index[6];/*hend5 + blk_width;*/
		hend7 = h_index[7];/*hend6 + blk_width;*/
		hend8 = h_index[8];/*hend7 + blk_width;*/
		hend9 = h_index[9];/*hend8 + blk_width;*/
		hend10 = h_index[10];/*hend9 + blk_width ;*/
		hend11 = h_index[11];/*hend10 + blk_width;*/
		hend12 = h_index[12];/*hend11 + blk_width;*/
		hend13 = h_index[13];/*hend12 + blk_width;*/
		hend14 = h_index[14];/*hend13 + blk_width;*/
		hend15 = h_index[15];/*hend14 + blk_width;*/
		hend16 = h_index[16];/*hend15 + blk_width;*/
		hend17 = h_index[17];/*hend16 + blk_width;*/
		hend18 = h_index[18];/*hend17 + blk_width;*/
		hend19 = h_index[19];/*hend18 + blk_width;*/
		hend20 = h_index[20];/*hend19 + blk_width ;*/
		hend21 = h_index[21];/*hend20 + blk_width;*/
		hend22 = h_index[22];/*hend21 + blk_width;*/
		hend23 = h_index[23];/*hend22 + blk_width;*/
		vend0 = v_index[0];/*row_start + blk_height - 1;*/
		vend1 = v_index[1];/*vend0 + blk_height;*/
		vend2 = v_index[2];/*vend1 + blk_height;*/
		vend3 = v_index[3];/*vend2 + blk_height;*/
		vend4 = v_index[4];/*vend3 + blk_height;*/
		vend5 = v_index[5];/*vend4 + blk_height;*/
		vend6 = v_index[6];/*vend5 + blk_height;*/
		vend7 = v_index[7];/*vend6 + blk_height;*/
		vend8 = v_index[8];/*vend7 + blk_height;*/
		vend9 = v_index[9];/*vend8 + blk_height;*/
		vend10 = v_index[10];/*vend9 +  blk_height;*/
		vend11 = v_index[11];/*vend10 + blk_height;*/
		vend12 = v_index[12];/*vend11 + blk_height;*/
		vend13 = v_index[13];/*vend12 + blk_height;*/
		vend14 = v_index[14];/*vend13 + blk_height;*/
		vend15 = v_index[15];/*vend14 + blk_height;*/

		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
		Wr(LDIM_STTS_HIST_SET_REGION,
			((((row_start & 0x1fff) << 16) & 0xffff0000) |
			(col_start & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend1 & 0x1fff) << 16)
			| (hend0 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend1 & 0x1fff) << 16)
			| (vend0 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend3 & 0x1fff) << 16)
			| (hend2 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend3 & 0x1fff) << 16)
			| (vend2 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend5 & 0x1fff) << 16)
			| (hend4 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend5 & 0x1fff) << 16)
			| (vend4 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend7 & 0x1fff) << 16)
			| (hend6 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend7 & 0x1fff) << 16)
			| (vend6 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend9 & 0x1fff) << 16)
			| (hend8 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend9 & 0x1fff) << 16)
			| (vend8 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend11 & 0x1fff) << 16)
			| (hend10 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend11 & 0x1fff) << 16)
			| (vend10 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend13 & 0x1fff) << 16)
			| (hend12 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend13 & 0x1fff) << 16)
			| (vend12 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend15 & 0x1fff) << 16)
			| (hend14 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend15 & 0x1fff) << 16)
			| (vend14 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend17 & 0x1fff) << 16)
			| (hend16 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend19 & 0x1fff) << 16)
			| (hend18 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend21 & 0x1fff) << 16)
			| (hend20 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend23 & 0x1fff) << 16)
			| (hend22 & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, blk_hnum); /*h region number*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0); /*line_n_int_num*/
	} else if (resolution == 1) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010010);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1000080);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0800040);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2000180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x10000c0);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x3000280);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1800140);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000380);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x20001c0);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4ff0480);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x2cf0260);*/
	} else if (resolution == 2) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);/*hv00*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x17f00bf);/*h01*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0d7006b);/*v01*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2ff023f);/*h23*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1af0143);/*v23*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x47f03bf);/*h45*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x287021b);/*v45*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x5ff053f);/*h67*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0x35f02f3);/*v67*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h89*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v89*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1011*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v1011*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1213*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v1213*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1415*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*v1415*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1617*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h1819*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h2021*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/*h2223*/
	} else if (resolution == 3) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1df00ef);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x10d0086);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x3bf02cf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x21b0194);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x59f04af);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x32902a2);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f068f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x43703b0);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 4) { /* 5x6 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0040001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x27f0136);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1af00d7);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4ff03bf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x35f0287);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f063f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 5) { /* 8x2 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x31f02bb);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x233012b);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x30b0243);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x42d03d3);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 6) { /* 2x1 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x78002bb);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 7) { /* 2x2 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f03bf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x437021b);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 8) { /* 3x5 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2ff017f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2cf0167);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x5ff047f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x780077f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 9) { /* 4x3 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4560333);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800666);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	} else if (resolution == 10) { /* 6x8 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2430167);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000350);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x6000510);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4370410);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f0700);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);*/
		/*Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);*/
	}
}

static void ldim_stts_set_region_tl1(
		unsigned int resolution,
			/* 0: auto calc by height/width/row_start/col_start
			 * 1: 720p
			 * 2: 1080p
			 */
		unsigned int height, unsigned int width,
		unsigned int blk_height, unsigned int blk_width,
		unsigned int row_start, unsigned int col_start,
		unsigned int blk_vnum, unsigned int blk_hnum)
{
	unsigned int data32, h_region_num;
	unsigned int heightm1, widthm1;
	unsigned int lights_mode = 0;
	unsigned int hend[24], vend[16], i;

	memset(hend, 0, (sizeof(unsigned int) * 24));
	memset(vend, 0, (sizeof(unsigned int) * 16));

	/* 0:normal   1:>24 h region */
	if (blk_hnum > 24)
		lights_mode = 1;
	h_region_num = blk_hnum;
	if (ldim_debug_print) {
		pr_info("%s: lights_mode=%d, h_region_num=%d\n",
			__func__, lights_mode, h_region_num);
	}

	heightm1 = height - 1;
	widthm1  = width - 1;

	if (resolution == 0) {
		hend[0] = col_start + blk_width - 1;
		for (i = 1; i < 24; i++) {
			hend[i] = (hend[i-1] + blk_width >= widthm1) ?
				hend[i-1] : (hend[i-1] + blk_width);
		}
		vend[0] = row_start + blk_height - 1;
		for (i = 1; i < 16; i++) {
			vend[i] = (vend[i-1] + blk_height >= heightm1) ?
				vend[i-1] : (vend[i-1] + blk_height);
		}

		if (lights_mode == 1) {  /* 31 h region */
			vend[8] = (hend[23] + blk_width >= widthm1) ?
				hend[23] : (hend[23] + blk_width);
			for (i = 9; i < 16; i++) {
				vend[i] = (vend[i-1] + blk_width >= widthm1) ?
					vend[i-1] : (vend[i-1] + blk_width);
			}
		}

		Wr_reg_bits(LDIM_STTS_CTRL0, lights_mode, 22, 2);
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
		Wr(LDIM_STTS_HIST_SET_REGION,
			((((row_start & 0x1fff) << 16) & 0xffff0000) |
			(col_start & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[1] & 0x1fff) << 16) |
			(hend[0] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[1] & 0x1fff) << 16) |
			(vend[0] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[3] & 0x1fff) << 16) |
			(hend[2] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[3] & 0x1fff) << 16) |
			(vend[2] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[5] & 0x1fff) << 16) |
			(hend[4] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[5] & 0x1fff) << 16) |
			(vend[4] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[7] & 0x1fff) << 16) |
			(hend[6] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[7] & 0x1fff) << 16) |
			(vend[6] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[9] & 0x1fff) << 16) |
			(hend[8] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[9] & 0x1fff) << 16) |
			(vend[8] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[11] & 0x1fff) << 16) |
			(hend[10] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[11] & 0x1fff) << 16) |
			(vend[10] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[13] & 0x1fff) << 16) |
			(hend[12] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[13] & 0x1fff) << 16) |
			(vend[12] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[15] & 0x1fff) << 16) |
			(hend[14] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((vend[15] & 0x1fff) << 16) |
			(vend[14] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[17] & 0x1fff) << 16) |
			(hend[16] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[19] & 0x1fff) << 16) |
			(hend[18] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[21] & 0x1fff) << 16) |
			(hend[20] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, (((hend[23] & 0x1fff) << 16) |
			(hend[22] & 0x1fff)));
		Wr(LDIM_STTS_HIST_SET_REGION, h_region_num); /*h region number*/
		Wr(LDIM_STTS_HIST_SET_REGION, 0); /* line_n_int_num */
	} else if (resolution == 1) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010010);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1000080);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0800040);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2000180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x10000c0);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x3000280);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1800140);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000380);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x20001c0);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4ff0480); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x2cf0260); */
	} else if (resolution == 2) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);/* hv00 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x17f00bf);/* h01 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0d7006b);/* v01 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2ff023f);/* h23 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1af0143);/* v23 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x47f03bf);/* h45 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x287021b);/* v45 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x5ff053f);/* h67 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0x35f02f3);/* v67 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h89 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v89 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1011 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v1011 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1213 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v1213 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1415 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v1415 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1617 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1819 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h2021 */
		Wr(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h2223 */
	} else if (resolution == 3) {
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1df00ef);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x10d0086);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x3bf02cf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x21b0194);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x59f04af);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x32902a2);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f068f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x43703b0);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 4) { /* 5x6 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0040001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x27f0136);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x1af00d7);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4ff03bf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x35f0287);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f063f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 5) { /* 8x2 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x31f02bb);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x233012b);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x30b0243);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x42d03d3);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 6) { /* 2x1 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x78002bb);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 7) { /* 2x2 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f03bf);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x437021b);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 8) { /* 3x5 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2ff017f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2cf0167);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x5ff047f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x780077f);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 9) { /* 4x3 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4560333);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800666);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 10) { /* 6x8 */
		data32 = Rd(LDIM_STTS_HIST_REGION_IDX);
		Wr(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		Wr(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2430167);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000350);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x6000510);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4370410);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x77f0700);
		Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    Wr(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	}
}

void ldim_stts_initial_txlx(unsigned int pic_h, unsigned int pic_v,
		unsigned int blk_vnum, unsigned int blk_hnum)
{
	unsigned int resolution, blk_height, blk_width;
	unsigned int row_start, col_start;

	blk_vnum = (blk_vnum == 0) ? 1 : blk_vnum;
	blk_hnum = (blk_hnum == 0) ? 1 : blk_hnum;

	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*Wr_reg(VDIN0_HIST_CTRL, 0x10d);*/

	blk_height = (pic_v - 8) / blk_vnum;
	blk_width = (pic_h - 8) / blk_hnum;
	row_start = (pic_v - (blk_height * blk_vnum)) >> 1;
	col_start = (pic_h - (blk_width * blk_hnum)) >> 1;

	Wr_reg(LDIM_STTS_CTRL0, 7 << 2);
	ldim_set_matrix_ycbcr2rgb();
	ldim_stts_en(resolution, 0, 0, 1, 1, 1, 0);

	ldim_stts_set_region_txlx(0, blk_height, blk_width,
		row_start, col_start, blk_hnum);
}

void ldim_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
		unsigned int blk_vnum, unsigned int blk_hnum)
{
	unsigned int resolution, blk_height, blk_width;
	unsigned int row_start, col_start;

	blk_vnum = (blk_vnum == 0) ? 1 : blk_vnum;
	blk_hnum = (blk_hnum == 0) ? 1 : blk_hnum;

	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*Wr_reg(VDIN0_HIST_CTRL, 0x10d);*/

	blk_height = (pic_v - 8) / blk_vnum;
	blk_width = (pic_h - 8) / blk_hnum;
	row_start = (pic_v - (blk_height * blk_vnum)) >> 1;
	col_start = (pic_h - (blk_width * blk_hnum)) >> 1;

	ldim_stts_en(resolution, 0, 0, 1, 1, 1, 0);
	ldim_set_matrix_ycbcr2rgb();
	/*0:di  1:vdin  2:null  3:postblend  4:vpp out  5:vd1  6:vd2  7:osd1*/
	Wr_reg_bits(LDIM_STTS_CTRL0, 3, 3, 3);

	ldim_stts_set_region_tl1(0, pic_v, pic_h, blk_height, blk_width,
		row_start, col_start, blk_vnum, blk_hnum);
}

static unsigned int invalid_val_cnt;
void ldim_stts_read_region(unsigned int nrow, unsigned int ncol)
{
	unsigned int i, j, k;
	unsigned int data32;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (invalid_val_cnt > 0xfffffff)
		invalid_val_cnt = 0;

	Wr(LDIM_STTS_HIST_REGION_IDX, Rd(LDIM_STTS_HIST_REGION_IDX)
		& 0xffffc000);
	data32 = Rd(LDIM_STTS_HIST_START_RD_REGION);

	for (i = 0; i < nrow; i++) {
		for (j = 0; j < ncol; j++) {
			data32 = Rd(LDIM_STTS_HIST_START_RD_REGION);
			for (k = 0; k < 17; k++) {
				if (k == 16) {
					data32 = Rd(LDIM_STTS_HIST_READ_REGION);
					ldim_drv->max_rgb[i * ncol + j]
						= data32;
				} else {
					data32 = Rd(LDIM_STTS_HIST_READ_REGION);
					ldim_drv->hist_matrix[i * ncol * 16 +
						j * 16 + k] = data32;
				}
				if (!(data32 & 0x40000000))
					invalid_val_cnt++;
			}
		}
	}
}

/* VDIN_MATRIX_YUV601_RGB */
/* -16	   1.164  0	 1.596	    0 */
/* -128     1.164 -0.391 -0.813      0 */
/* -128     1.164  2.018  0	     0 */
/*{0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
 *	0x00000000, 0x00000000, 0x00000000,},
 */
void ldim_set_matrix_ycbcr2rgb(void)
{
	Wr_reg_bits(LDIM_STTS_CTRL0, 1, 2, 1);  /* enable matrix */

	Wr(LDIM_STTS_MATRIX_PRE_OFFSET0_1, 0x07c00600);
	Wr(LDIM_STTS_MATRIX_PRE_OFFSET2, 0x00000600);
	Wr(LDIM_STTS_MATRIX_COEF00_01, 0x04a80000);
	Wr(LDIM_STTS_MATRIX_COEF02_10, 0x066204a8);
	Wr(LDIM_STTS_MATRIX_COEF11_12, 0x1e701cbf);
	Wr(LDIM_STTS_MATRIX_COEF20_21, 0x04a80812);
	Wr(LDIM_STTS_MATRIX_COEF22, 0x00000000);
	Wr(LDIM_STTS_MATRIX_OFFSET0_1, 0x00000000);
	Wr(LDIM_STTS_MATRIX_OFFSET2, 0x00000000);
}

void ldim_set_matrix_rgb2ycbcr(int mode)
{
	Wr_reg_bits(LDIM_STTS_CTRL0, 1, 2, 1);
	if (mode == 0) {/*ycbcr not full range, 601 conversion*/
		Wr(LDIM_STTS_MATRIX_PRE_OFFSET0_1, 0x0);
		Wr(LDIM_STTS_MATRIX_PRE_OFFSET2, 0x0);
		/*	0.257     0.504   0.098	*/
		/*	-0.148    -0.291  0.439	*/
		/*	0.439     -0.368 -0.071	*/
		Wr(LDIM_STTS_MATRIX_COEF00_01, (0x107 << 16) | 0x204);
		Wr(LDIM_STTS_MATRIX_COEF02_10, (0x64 << 16) | 0x1f68);
		Wr(LDIM_STTS_MATRIX_COEF11_12, (0x1ed6 << 16) | 0x1c2);
		Wr(LDIM_STTS_MATRIX_COEF20_21, (0x1c2 << 16) | 0x1e87);
		Wr(LDIM_STTS_MATRIX_COEF22, 0x1fb7);

		Wr(LDIM_STTS_MATRIX_OFFSET2, 0x0200);
	} else if (mode == 1) {/*ycbcr full range, 601 conversion*/
		/* todo */
	}
}

void ldim_remap_update_txlx(struct LDReg_s *nPRM,
		unsigned int avg_update_en, unsigned int matrix_update_en)
{
	unsigned int data;

	if (avg_update_en) {
		/* LD_BKLIT_PARAM */
		data = LDIM_RD_32Bits(REG_LD_BKLIT_PARAM);
		data = (data & (~0xfff)) | (nPRM->reg_BL_matrix_AVG & 0xfff);
		LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);

		/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = (data & (~0xfff)) |
			(nPRM->reg_BL_matrix_Compensate & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	}

	if (matrix_update_en) {
		data = nPRM->reg_LD_BLK_Vnum * nPRM->reg_LD_BLK_Hnum;
		LDIM_Update_Matrix(nPRM->BL_matrix, data);
	}
}
