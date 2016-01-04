/*
* Amlogic Ldim Driver for Meson Chip
*
* Author:
*
* Copyright (C) 2015 Amlogic Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/device.h>
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
/* #include <mach/am_regs.h> */
#include <linux/amlogic/amports/vframe.h>
#include <linux/spinlock.h>
#include <linux/amlogic/iomap.h>
#include "ldim_drv.h"
#include "ldim_func.h"
#include "ldim_reg.h"
#include "ldim_extern.h"

static int LD_STA1max_Hidx[25] = {
	/*  U12* 25	*/
	0, 480, 960, 1440, 1920, 2400, 2880,
	3360, 3840, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095
};

static int LD_STA1max_Vidx[17] = {
	/* u12x 17	*/
	0, 2160, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095
};

static int LD_STA2max_Hidx[25] = {
	/* U12* 25	*/
	0, 480, 960, 1440, 1920, 2400, 2880,
	3360, 3840, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095
};

static int LD_STA2max_Vidx[17] = {
	/* u12x 17	*/
	0, 2160, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095
};

static int LD_STAhist_Hidx[25] = {
	/* U12* 25	*/
	0, 480, 960, 1440, 1920, 2400, 2880,
	3360, 3840, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095
};

static int LD_STAhist_Vidx[17] = {
	/*  u12x 17	*/
	0, 2160, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095, 4095,
	4095, 4095, 4095, 4095, 4095
};

static int LD_BLK_Hidx[33] = {
	/* S14* 33	*/
	-1920, -1440, -960, -480, 0, 480,
	960, 1440, 1920, 2400, 2880, 3360,
	3840, 4320, 4800, 5280, 5760, 6240,
	6720, 7200, 7680, 8160, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191
};

static int LD_BLK_Vidx[25] = {
	/*  S14* 25 */
	-8192, -8192, -8192, -4320, 0, 4320,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191, 8191
};

static int LD_LUT_Hdg[32] = {
	 /*	u10	*/
	503, 501, 494, 481, 465, 447, 430, 409, 388, 369, 354,
	343, 334, 326, 318, 311, 305, 299, 293, 286, 279, 272,
	266, 261, 257, 252, 245, 235, 226, 218, 214, 213
};

static int LD_LUT_Vdg[32] = {
	 /*	u10	*/
	373, 371, 367, 364, 359, 353, 346, 337, 328, 318, 308,
	297, 286, 274, 261, 247, 232, 218, 204, 191, 180, 169,
	158, 148, 138, 130, 122, 115, 108, 104, 100, 97
};

static int LD_LUT_VHk[32] = {
	 /*	u10	*/
	492, 492, 492, 492, 427, 356, 328, 298, 272, 251, 229,
	206, 191, 175, 162, 151, 144, 139, 131, 127, 119, 110,
	105, 101, 99, 98, 94, 85, 83, 77, 74, 73
};

static int LD_LUT_Hdg1[32] = {
	 /*	u10	*/
	503, 501, 494, 481, 465, 447, 430, 409, 388, 369, 354,
	343, 334, 326, 318, 311, 305, 299, 293, 286, 279, 272,
	266, 261, 257, 252, 245, 235, 226, 218, 214, 213
};

static int LD_LUT_Vdg1[32] = {
	/*	u10	*/
	373, 371, 367, 364, 359, 353, 346, 337, 328, 318, 308,
	297, 286, 274, 261, 247, 232, 218, 204, 191, 180, 169,
	158, 148, 138, 130, 122, 115, 108, 104, 100, 97
};

static int LD_LUT_VHk1[32] = {
	 /*	u10	*/
	492, 492, 492, 492, 427, 356, 328, 298, 272, 251, 229,
	206, 191, 175, 162, 151, 144, 139, 131, 127, 119, 110,
	105, 101, 99, 98, 94, 85, 83, 77, 74, 73
};

static int LD_LUT_VHo_pos[] = {
	104, 75, 50, 20, -15, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25
};

static int LD_LUT_VHo_neg[] = {
	104, 75, 50, 20, -15, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25
};

/*	public function	*/
int ldim_round(int ix, int ib)
{
	int ld_rst = 0;

	if (ix == 0)
		ld_rst = 0;
	else if (ix > 0)
		ld_rst = (ix+ib/2)/ib;
	else
		ld_rst = (ix-ib/2)/ib;

	return ld_rst;
}

/************************************************************************
local dimming stts functions begin
*************************************************************************/
/*hist mode: 0: comp0 hist only, 1: Max(comp0,1,2) for hist,
2: the hist of all comp0,1,2 are calculated*/
/*lpf_en   1: 1,2,1 filter on before finding max& hist*/
/*rd_idx_auto_inc_mode 0: no self increase, 1: read index increase after
read a 25/48 block, 2: increases every read and lock sub-idx*/
/*one_ram_en 1: one ram mode;  0:double ram mode*/
void ldim_stts_en(unsigned int resolution, unsigned int pix_drop_mode,
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

/* resolution:
0: auto calc by height/width/row_start/col_start
1: 720p
2: 1080p
*/
void ldim_set_region(unsigned int resolution, unsigned int blk_height,
	unsigned int blk_width, unsigned int row_start, unsigned int col_start,
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
		Wr(LDIM_STTS_HIST_SET_REGION, ((((row_start & 0x1fff) << 16)
			& 0xffff0000) | (col_start & 0x1fff)));
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

void ldim_read_region(unsigned int nrow, unsigned int ncol)
{
	unsigned int i, j, k;
	unsigned int data32;
	if (invalid_val_cnt > 0xfffffff)
		invalid_val_cnt = 0;

	Wr(LDIM_STTS_HIST_REGION_IDX, Rd(LDIM_STTS_HIST_REGION_IDX)
		& 0xffffc000);
	data32 = Rd(LDIM_STTS_HIST_START_RD_REGION);

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 24; j++) {
			data32 = Rd(LDIM_STTS_HIST_START_RD_REGION);
			for (k = 0; k < 17; k++) {
				if (k == 16) {
					data32 = Rd(LDIM_STTS_HIST_READ_REGION);
					if ((i < nrow) && (j < ncol))
						max_rgb[i*ncol+j] = data32;
				} else {
					data32 = Rd(LDIM_STTS_HIST_READ_REGION);
					if ((i < nrow) && (j < ncol))
						hist_matrix[i*ncol*16+j*16+k]
							= data32;
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
	0x00000000, 0x00000000, 0x00000000,},*/
void ldim_set_matrix_ycbcr2rgb(void)
{
	Wr_reg_bits(LDIM_STTS_CTRL0, 1, 2, 1);

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
		Wr(LDIM_STTS_MATRIX_COEF00_01, (0x107 << 16) |
		0x204);
		Wr(LDIM_STTS_MATRIX_COEF02_10, (0x64 << 16) |
		0x1f68);
		Wr(LDIM_STTS_MATRIX_COEF11_12, (0x1ed6 << 16) |
		0x1c2);
		Wr(LDIM_STTS_MATRIX_COEF20_21, (0x1c2 << 16) |
		0x1e87);
		Wr(LDIM_STTS_MATRIX_COEF22, 0x1fb7);

		Wr(LDIM_STTS_MATRIX_OFFSET2, 0x0200);
	} else if (mode == 1) {/*ycbcr full range, 601 conversion*/
	}
}

/*
local dimming stts functions end
local dimming backlight control functions end
*/
int Round(int iX, int iB)
{
	int Rst = 0;

	if (iX == 0)
		Rst = 0;
	else if (iX > 0)
		Rst = (iX + iB/2)/iB;
	else
		Rst = (iX - iB/2)/iB;
	return Rst;
}

void LD_IntialData(int pData[], int size, int val)
{
	int i;
	for (i = 0; i < size; i++)
		pData[i] = val;
}

int LD_GetBLMtxAvg(int pMtx[], int size, int mode)
{
	int i;
	int da;
	if (mode == 0)
		da = 0;
	else if (mode == 1) {
		da = 0;
		for (i = 0; i < size; i++)
			da += pMtx[i];
			da = da/size;
	} else if (mode == 2) {
		da = pMtx[0];
		for (i = 1; i < size; i++)
			if (da > pMtx[i])
				da = pMtx[i];
	} else {
		da = pMtx[0];
		for (i = 1; i < size; i++)
			if (da < pMtx[i])
				da = pMtx[i];
	}
	return da;
}

void LD_MtxInv(int *oDat, int *iDat, int nRow, int nCol)
{
	int nT1 = 0;
	int nT2 = 0;
	int nY = 0;
	int nX = 0;

	for (nY = 0; nY < nRow; nY++) {
		for (nX = 0; nX < nCol; nX++) {
			nT1 = nY*nCol + nX;
			nT2 = nX*nRow + nY;
			oDat[nT2] = iDat[nT1];
		}
	}
}

#if 0
int Tmp[][32] = {
	{ 443,  889, 1336, 1782, 2229, 2675, 3122, 3221, 3315, 3404,
		3488, 3567, 3641, 3710, 3774, 3833, 3887, 3936, 3980, 4019,
		4053, 4082, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
		4095, 4095},
	{ 324,  651,  977, 1304, 1630, 1957, 2283, 2610, 2936, 3263,
		3356, 3445, 3528, 3605, 3678, 3745, 3806, 3863, 3914, 3960,
		4000, 4035, 4065, 4090, 4095, 4095, 4095, 4095, 4095, 4095,
		4095, 4095},
	{ 270,  542,  814, 1086, 1358, 1630, 1902, 2174, 2446, 2718,
		2990, 3262, 3534, 3600, 3663, 3721, 3775, 3825, 3871, 3913,
		3951, 3985, 4014, 4040, 4061, 4078, 4091, 4095, 4095, 4095,
		4095, 4095},
	{ 237,  476,  715,  954, 1193, 1432, 1671, 1910, 2149, 2387,
		2626, 2865, 3104, 3343, 3582, 3648, 3710, 3766, 3819, 3867,
		3911, 3950, 3984, 4015, 4040, 4061, 4078, 4091, 4095, 4095,
		4095, 4095},
	{ 214,  430,  647,  863, 1079, 1295, 1511, 1727, 1943, 2159,
		2375, 2591, 2807, 3024, 3240, 3456, 3548, 3634, 3712, 3784,
		3849, 3906, 3957, 4000, 4036, 4066, 4088, 4095, 4095, 4095,
		4095, 4095},
	{ 198,  397,  596,  795,  994, 1193, 1392, 1591, 1790, 1989,
		2188, 2387, 2586, 2785, 2984, 3183, 3382, 3581, 3661, 3734,
		3800, 3860, 3913, 3960, 4000, 4033, 4060, 4080, 4094, 4095,
		4095, 4095},
	{ 184,  370,  556,  741,  927, 1113, 1299, 1484, 1670, 1856,
		2041, 2227, 2413, 2598, 2784, 2970, 3156, 3341, 3527, 3713,
		3774, 3830, 3880, 3926, 3965, 4000, 4029, 4053, 4072, 4085,
		4093, 4095},
	{ 174,  348,  523,  698,  873, 1048, 1223, 1398, 1572, 1747,
		1922, 2097, 2272, 2447, 2622, 2797, 2971, 3146, 3321, 3496,
		3671, 3748, 3818, 3879, 3933, 3980, 4018, 4049, 4072, 4088,
		4095, 4095},
	{ 165,  330,  496,  662,  828,  994, 1160, 1325, 1491, 1657,
		1823, 1989, 2155, 2320, 2486, 2652, 2818, 2984, 3150, 3316,
		3481, 3647, 3738, 3819, 3890, 3950, 4000, 4040, 4069, 4088,
		4095, 4095},
	{ 157,  315,  473,  631,  790,  948, 1106, 1264, 1422, 1580,
		1739, 1897, 2055, 2213, 2371, 2529, 2687, 2846, 3004, 3162,
		3320, 3478, 3636, 3740, 3831, 3909, 3973, 4024, 4062, 4086,
		4095, 4095},
	{ 150,  302,  453,  605,  756,  908, 1059, 1211, 1362, 1514,
		1665, 1817, 1969, 2120, 2272, 2423, 2575, 2726, 2878, 3029,
		3181, 3332, 3484, 3635, 3787, 3860, 3923, 3977, 4021, 4055,
		4080, 4095},
	{ 145,  290,  436,  582,  727,  873, 1019, 1164, 1310, 1456,
		1602, 1747, 1893, 2039, 2184, 2330, 2476, 2621, 2767, 2913,
		3058, 3204, 3350, 3496, 3641, 3787, 3871, 3942, 4000, 4045,
		4076, 4095},
	{ 139,  280,  421,  561,  702,  842,  983, 1123, 1264, 1404,
		1545, 1685, 1826, 1966, 2107, 2248, 2388, 2529, 2669, 2810,
		2950, 3091, 3231, 3372, 3512, 3653, 3793, 3889, 3968, 4028,
		4070, 4095},
	{ 135,  271,  407,  543,  679,  815,  950, 1086, 1222, 1358,
		1494, 1630, 1766, 1902, 2038, 2174, 2310, 2446, 2582, 2718,
		2853, 2989, 3125, 3261, 3397, 3533, 3669, 3805, 3915, 4000,
		4060, 4095},
	{ 131,  263,  394,  526,  658,  790,  921, 1053, 1185, 1317,
		1448, 1580, 1712, 1844, 1976, 2107, 2239, 2371, 2503, 2634,
		2766, 2898, 3030, 3162, 3293, 3425, 3557, 3689, 3820, 3950,
		4041, 4095},
	{ 127,  255,  383,  511,  639,  767,  895, 1023, 1151, 1279,
		1407, 1535, 1663, 1791, 1919, 2047, 2175, 2303, 2431, 2559,
		2687, 2815, 2943, 3071, 3199, 3327, 3455, 3583, 3711, 3925,
		4053, 4095}
	};
#endif

void LD_LUTInit(struct LDReg *Reg)
{
	int k = 0;
	int t = 0;
	int tmp = 0;

	/* Emulate the FW to set the LUTs */
	for (k = 0; k < 16; k++) {
		/*set the LUT to be inverse of the Lit_value,*/
		/* lit_idx distribute equal space, set by FW */
		Reg->X_idx[0][k] = 4095 - 256*k;
		Reg->X_nrm[0][k] = 8;
		for (t = 0; t < 32; t++) {
			/* May be different from Matlab when (16-k)
			is an odd integer */
			tmp = Round(64*(t + 1)*32, (16 - k));
			if (tmp > 4095)
				tmp = 4095;
			Reg->X_lut[0][k][t] = tmp;
			Reg->X_lut[1][k][t] = tmp;
			Reg->X_lut[2][k][t] = tmp;
		}
	}
}

#if 1
void LD_ConLDReg(struct LDReg *Reg)
{
	unsigned int T = 0;
	unsigned int Vnum = 0;
	unsigned int Hnum = 0;
	unsigned int BSIZE = 0;

	/* General registers; */
	Reg->reg_LD_pic_RowMax = 2160;/* setting default */
	Reg->reg_LD_pic_ColMax = 3840;
	LD_IntialData(Reg->reg_LD_pic_YUVsum, 3, 0);
	/* only output u16*3, (internal ACC will be u32x3)*/
	LD_IntialData(Reg->reg_LD_pic_RGBsum, 3, 0);

	/* set same region division for statistics */
	Reg->reg_LD_STA_Vnum  = 1;
	Reg->reg_LD_STA_Hnum  = 8;

	/*Image Statistic options */
	Reg->reg_LD_BLK_Vnum = 1;/*u5: Maximum to BLKVMAX */
	Reg->reg_LD_BLK_Hnum  = 8;/*u5: Maximum to BLKHMAX */

	Reg->reg_LD_STA1max_LPF = 1;
	/*u1: STA1max statistics on [1 2 1]/4 filtered results */
	Reg->reg_LD_STA2max_LPF = 1;
	/*u1: STA2max statistics on [1 2 1]/4 filtered results*/
	Reg->reg_LD_STAhist_LPF  = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results*/
	Reg->reg_LD_STA1max_Hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	Reg->reg_LD_STA1max_Vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	Reg->reg_LD_STA2max_Hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	Reg->reg_LD_STA2max_Vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	Reg->reg_LD_STAhist_mode = 3;
	/*u3: histogram statistics on XX separately 20bits*16bins:
	0:R-only,1:G-only 2:B-only 3:Y-only; 4:MAX(R,G,B),5/6/7:R&G&B*/
	Reg->reg_LD_STAhist_pix_drop_mode = 0;/*u2 */
	for (T = 0; T < LD_STA_LEN_H; T++) {
		Reg->reg_LD_STA1max_Hidx[T] = LD_STA1max_Hidx[T];/*U12* 25*/
		Reg->reg_LD_STA2max_Hidx[T] = LD_STA2max_Hidx[T];/*U12* 25*/
		Reg->reg_LD_STAhist_Hidx[T] = LD_STAhist_Hidx[T];/*U12* 25*/
	}
	for (T = 0; T < LD_STA_LEN_V; T++) {
		Reg->reg_LD_STA1max_Vidx[T] = LD_STA1max_Vidx[T];/*u12x 17*/
		Reg->reg_LD_STA2max_Vidx[T] = LD_STA2max_Vidx[T];/*u12x 17*/
		Reg->reg_LD_STAhist_Vidx[T] = LD_STAhist_Vidx[T];/*u12x 17*/
	}
	/*---------------------Setting BL_matrix initial value
			(will be updated frame to frame in FW)*/
	Vnum = Reg->reg_LD_BLK_Vnum;
	Hnum = Reg->reg_LD_BLK_Hnum;
	BSIZE = Vnum*Hnum;
	/*Initialization */
	LD_IntialData(Reg->BL_matrix, BSIZE, 4095);

	/* BackLight Modeling control register setting*/
	Reg->reg_LD_BackLit_Xtlk = 1;
	/* u1: 0 no block to block Xtalk model needed;	 1: Xtalk model needed*/
	Reg->reg_LD_BackLit_mode = 1;
	/*u2: 0- LEFT/RIGHT Edge Lit; 1- Top/Bot Edge Lit; 2 - DirectLit modeled
		H/V independant; 3- DirectLit modeled HV Circle distribution */
	Reg->reg_LD_Reflect_Hnum = 3;
	/*u3: numbers of band reflection considered in Horizontal
			direction; 0~4*/
	Reg->reg_LD_Reflect_Vnum = 0;
	/*u3: numbers of band reflection considered in Horizontal
			direction; 0~4*/
	Reg->reg_LD_BkLit_curmod = 0;
	/*u1: 0: H/V separately, 1 Circle distribution*/
	Reg->reg_LD_BkLUT_Intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	Reg->reg_LD_BkLit_Intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	Reg->reg_LD_BkLit_LPFmod = 7;
	/*u3: 0: no LPF, 1:[1 14 1]/16;2:[1 6 1]/8; 3: [1 2 1]/4;
			4:[9 14 9]/32  5/6/7: [5 6 5]/16;*/
	Reg->reg_LD_BkLit_Celnum = 121;
	/*u8:0:1920~61####((Reg->reg_LD_pic_ColMax+1)/32)+1;*/
	Reg->reg_BL_matrix_AVG = 4095;
	/*u12: DC of whole picture BL to be substract from BL_matrix
		during modeling (Set by FW daynamically)*/
	Reg->reg_BL_matrix_Compensate = 4095;
	/*u12: DC of whole picture BL to be compensated back to
		Litfull after the model (Set by FW dynamically);*/
	LD_IntialData(Reg->reg_LD_Reflect_Hdgr, 20, 32);
	/*20*u6: cells 1~20 for H Gains of different dist of Left/Right;*/
	LD_IntialData(Reg->reg_LD_Reflect_Vdgr, 20, 32);
	/*20*u6: cells 1~20 for V Gains of different dist of Top/Bot; */
	LD_IntialData(Reg->reg_LD_Reflect_Xdgr, 4, 32);/*  4*u6: */

	Reg->reg_LD_Vgain	= 256;/* u12 */
	Reg->reg_LD_Hgain	= 242;/* u12 */
	Reg->reg_LD_Litgain = 256;/* u12 */
	Reg->reg_LD_Litshft = 3;
	/* u3	right shif of bits for the all Lit's sum */
	LD_IntialData(Reg->reg_LD_BkLit_valid, 32, 1);
	/*u1x32: valid bits for the 32 cell Bklit to contribut to current
		position (refer to the backlit padding pattern)
	 region division index  1  2	3	4 5(0) 6(1) 7(2) 8(3) 9(4)
		10(5)11(6)12(7)13(8) 14(9)15(10) 16   17   18	19 */
	for (T = 0; T < LD_BLK_LEN_H; T++)
		Reg->reg_LD_BLK_Hidx[T] = LD_BLK_Hidx[T];/* S14* BLK_LEN_H */
	for (T = 0; T < LD_BLK_LEN_V; T++)
		Reg->reg_LD_BLK_Vidx[T] = LD_BLK_Vidx[T];/* S14x BLK_LEN_V */
	for (T = 0; T < LD_LUT_LEN; T++) {
		Reg->reg_LD_LUT_Hdg[T] = LD_LUT_Hdg[T];
		Reg->reg_LD_LUT_Vdg[T] = LD_LUT_Vdg[T];
		Reg->reg_LD_LUT_VHk[T] = LD_LUT_VHk[T];
	}
	/* set the VHk_pos and VHk_neg value ,normalized to
		128 as "1" 20150428 */
	for (T = 0; T < 32; T++) {
		Reg->reg_LD_LUT_VHk_pos[T] = 128;/* vdist>=0 */
		Reg->reg_LD_LUT_VHk_neg[T] = 128;/* vdist<0 */
		Reg->reg_LD_LUT_HHk[T] = (T == 4) ? 128 : 128;/* hdist gain */
		Reg->reg_LD_LUT_VHo_pos[T] = 0;/* vdist>=0 */
		Reg->reg_LD_LUT_VHo_neg[T] = 0;/*  vdist<0 */
	}
	Reg->reg_LD_LUT_VHo_LS = 0;
	Reg->reg_LD_LUT_Hdg_LEXT = 505;
	/* 2*(nPRM->reg_LD_LUT_Hdg[0]) - (nPRM->reg_LD_LUT_Hdg[1]); */
	Reg->reg_LD_LUT_Vdg_LEXT = 372;
	/* 2*(nPRM->reg_LD_LUT_Vdg[0]) - (nPRM->reg_LD_LUT_Vdg[1]); */
	Reg->reg_LD_LUT_VHk_LEXT = 492;
	/* 2*(nPRM->reg_LD_LUT_VHk[0]) - (nPRM->reg_LD_LUT_VHk[1]); */
	/* set the demo window */
	Reg->reg_LD_xlut_demo_roi_xstart = (Reg->reg_LD_pic_ColMax/4);
	     /* u14 start col index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_xend = (Reg->reg_LD_pic_ColMax*3/4);
	  /* u14 end col index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_ystart = (Reg->reg_LD_pic_RowMax/4);
	     /* u14 start row index of the region of interest */
	Reg->reg_LD_xlut_demo_roi_yend = (Reg->reg_LD_pic_RowMax*3/4);
	   /*  u14 end row index of the region of interest */
	Reg->reg_LD_xlut_iroi_enable = 1;
	     /*  u1: enable rgb LUT remapping inside regon of interest:
	      0: no rgb remapping; 1: enable rgb remapping */
	Reg->reg_LD_xlut_oroi_enable = 1;
	    /* u1: enable rgb LUT remapping outside regon of interest:
	      0: no rgb remapping; 1: enable rgb remapping */

	/*  Registers used in LD_RGB_LUT for RGB remaping */
	Reg->reg_LD_RGBmapping_demo = 0;
	/* u2: 0 no demo mode 1: display BL_fulpel on RGB */
	Reg->reg_LD_X_LUT_interp_mode[0] = 1;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  1: use the nearest LUT results */
	Reg->reg_LD_X_LUT_interp_mode[1] = 1;
	 /*  U1 0: using linear interpolation between to neighbour LUT;
	  1: use the nearest LUT results */
	Reg->reg_LD_X_LUT_interp_mode[2] = 1;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  1: use the nearest LUT results */
	LD_LUTInit(Reg);
	/* only do the Lit modleing on the AC part */
	Reg->fw_LD_BLEst_ACmode = 0;
	/* u2: 0: est on BLmatrix; 1: est on (BL-DC);
		2: est on (BL-MIN); 3: est on (BL-MAX) */
}

void ld_fw_cfg_once(struct LDReg *nPRM)
{
	int k, dlt;
	int hofst = 4;
	int vofst = 4;
	int oneside = 1;/* 0: left/top side,
		1: right/bot side, others: non-one-side */
	int Hnrm = 256;/* Hgain norm (256: for Hlen==2048) */
	int Vnrm = 256;/* Vgain norm (256: for Vlen==2048),related to VHk */

	int drt_LD_LUT_dg[] = {254, 248, 239, 226, 211, 194, 176, 156, 137,
				119, 101, 85, 70, 57, 45, 36, 28, 21, 16,
				12, 9, 6, 4, 3, 2, 1, 1, 1, 0, 0, 0, 0};

	/* demo mode to show the Lit map */
	nPRM->reg_LD_RGBmapping_demo = 0;

	/*  set Reg->reg_LD_BkLit_Celnum */
	nPRM->reg_LD_BkLit_Celnum = (nPRM->reg_LD_pic_ColMax + 63)/32;

	/* set same region division for statistics */
	nPRM->reg_LD_STA_Vnum  = nPRM->reg_LD_BLK_Vnum;
	nPRM->reg_LD_STA_Hnum  = nPRM->reg_LD_BLK_Hnum;

	/* STA1max_Hidx */
	nPRM->reg_LD_STA1max_Hidx[0] = 0;
	for (k = 1; k < LD_STA_LEN_H; k++) {
		nPRM->reg_LD_STA1max_Hidx[k] = ((nPRM->reg_LD_pic_ColMax +
			(nPRM->reg_LD_BLK_Hnum) - 1)/(nPRM->reg_LD_BLK_Hnum))*k;
		if (nPRM->reg_LD_STA1max_Hidx[k] > 4095)
			nPRM->reg_LD_STA1max_Hidx[k] = 4095;/* clip U12 */
	}
	/* STA1max_Vidx */
	nPRM->reg_LD_STA1max_Vidx[0] = 0;
	for (k = 1; k < LD_STA_LEN_V; k++) {
		nPRM->reg_LD_STA1max_Vidx[k] = ((nPRM->reg_LD_pic_RowMax +
			(nPRM->reg_LD_BLK_Vnum) - 1)/(nPRM->reg_LD_BLK_Vnum))*k;
		if (nPRM->reg_LD_STA1max_Vidx[k] > 4095)
			nPRM->reg_LD_STA1max_Vidx[k] = 4095;/* clip to U12 */
	}
	/* config LD_STA2max_H/Vidx/LD_STAhist_H/Vidx */
	for (k = 0; k < LD_STA_LEN_H; k++) {
		nPRM->reg_LD_STA2max_Hidx[k] = nPRM->reg_LD_STA1max_Hidx[k];
		nPRM->reg_LD_STAhist_Hidx[k] = nPRM->reg_LD_STA1max_Hidx[k];
	}
	for (k = 0; k < LD_STA_LEN_V; k++) {
		nPRM->reg_LD_STA2max_Vidx[k] = nPRM->reg_LD_STA1max_Vidx[k];
		nPRM->reg_LD_STAhist_Vidx[k] = nPRM->reg_LD_STA1max_Vidx[k];
	}
	if (nPRM->reg_LD_BackLit_mode == 0) {/* Left/right EdgeLit */
		/*  set reflect num */
		nPRM->reg_LD_Reflect_Hnum = 0;
		/* u3: numbers of band reflection considered in Horizontal
			direction; 0~4 */
		nPRM->reg_LD_Reflect_Vnum = 3;
		/* u3: numbers of band reflection considered in Horizontal
			direction; 0~4 */
		/* config reg_LD_BLK_Hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nPRM->reg_LD_pic_ColMax/(nPRM->reg_LD_BLK_Hnum)*2;
			if (oneside == 1) /* bot/right one side */
				nPRM->reg_LD_BLK_Hidx[k] = 0 + (k-hofst)*dlt;
			else
				nPRM->reg_LD_BLK_Hidx[k] = (-1)*(dlt/2)  +
					(k-hofst)*dlt;
			nPRM->reg_LD_BLK_Hidx[k] = (nPRM->reg_LD_BLK_Hidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Hidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Hidx[k]));/* Clip to S14 */
		}
		/*  config reg_LD_BLK_Vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = (nPRM->reg_LD_pic_RowMax)/(nPRM->reg_LD_BLK_Vnum);
			nPRM->reg_LD_BLK_Vidx[k] = 0 + (k-vofst)*dlt;
			nPRM->reg_LD_BLK_Vidx[k] = (nPRM->reg_LD_BLK_Vidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Vidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Vidx[k]));/*Clip to S14*/
		}
		/*  configure  Hgain/Vgain */
		nPRM->reg_LD_Hgain = (Hnrm*2048/(nPRM->reg_LD_pic_ColMax));
		nPRM->reg_LD_Vgain = (Vnrm*2048/(nPRM->reg_LD_pic_RowMax));
		nPRM->reg_LD_Hgain = (nPRM->reg_LD_Hgain >
			4095)  ? 4095 : (nPRM->reg_LD_Hgain);
		nPRM->reg_LD_Vgain = (nPRM->reg_LD_Vgain >
			4095)  ? 4095 : (nPRM->reg_LD_Vgain);

		/* if one side led, set the Hdg/Vdg/VHk differently */
		if (nPRM->reg_LD_BLK_Hnum == 1) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nPRM->reg_LD_LUT_Hdg[k] = LD_LUT_Hdg1[k];
				nPRM->reg_LD_LUT_Vdg[k] = LD_LUT_Vdg1[k];
				nPRM->reg_LD_LUT_VHk[k] = LD_LUT_VHk1[k];
				nPRM->reg_LD_LUT_VHo_neg[k] = LD_LUT_VHo_neg[k];
				nPRM->reg_LD_LUT_VHo_pos[k] = LD_LUT_VHo_pos[k];
			}
			nPRM->reg_LD_LUT_Hdg_LEXT = 2*(nPRM->reg_LD_LUT_Hdg[0])
			- (nPRM->reg_LD_LUT_Hdg[1]);
			nPRM->reg_LD_LUT_Vdg_LEXT = 2*(nPRM->reg_LD_LUT_Vdg[0])
			- (nPRM->reg_LD_LUT_Vdg[1]);
			nPRM->reg_LD_LUT_VHk_LEXT = 2*(nPRM->reg_LD_LUT_VHk[0])
			- (nPRM->reg_LD_LUT_VHk[1]);
			nPRM->reg_LD_Litgain = 230;
			/* u12 will be adjust according to pannel */
		}
	} else if (nPRM->reg_LD_BackLit_mode == 1) {/* Top/Bot EdgeLit */
		/* set reflect num */
		nPRM->reg_LD_Reflect_Hnum = 3;
		/* u3: numbers of band reflection considered in Horizontal
			direction; 0~4 */
		nPRM->reg_LD_Reflect_Vnum = 0;
		/* u3: numbers of band reflection considered in Horizontal
			direction; 0~4 */
		/* config reg_LD_BLK_Hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = (nPRM->reg_LD_pic_ColMax)/(nPRM->reg_LD_BLK_Hnum);
			nPRM->reg_LD_BLK_Hidx[k] = 0 + (k - hofst)*dlt;
			nPRM->reg_LD_BLK_Hidx[k] = (nPRM->reg_LD_BLK_Hidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Hidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Hidx[k]));/*Clip to S14*/
		}
		/* config reg_LD_BLK_Vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = nPRM->reg_LD_pic_RowMax/(nPRM->reg_LD_BLK_Vnum)*2;
			if (oneside == 1) /*  bot/right one side */
				nPRM->reg_LD_BLK_Vidx[k] = 0 + (k-vofst)*dlt;
			else
				nPRM->reg_LD_BLK_Vidx[k] = (-1)*(dlt/2) +
					(k-vofst)*dlt;
			nPRM->reg_LD_BLK_Vidx[k] = (nPRM->reg_LD_BLK_Vidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Vidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Vidx[k])); /* Clip to S14*/
		}
		/*  configure  Hgain/Vgain */
		nPRM->reg_LD_Hgain = (Hnrm*2048/(nPRM->reg_LD_pic_RowMax));
		nPRM->reg_LD_Vgain = (Vnrm*2048/(nPRM->reg_LD_pic_ColMax));
		nPRM->reg_LD_Hgain = (nPRM->reg_LD_Hgain >
			4095) ? 4095 : (nPRM->reg_LD_Hgain);
		nPRM->reg_LD_Vgain = (nPRM->reg_LD_Vgain >
			4095) ? 4095 : (nPRM->reg_LD_Vgain);

		/* if one side led, set the Hdg/Vdg/VHk differently */
		if (nPRM->reg_LD_BLK_Vnum == 1) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nPRM->reg_LD_LUT_Hdg[k] = LD_LUT_Hdg1[k];
				nPRM->reg_LD_LUT_Vdg[k] = LD_LUT_Vdg1[k];
				nPRM->reg_LD_LUT_VHk[k] = LD_LUT_VHk1[k];
				nPRM->reg_LD_LUT_VHo_neg[k] = LD_LUT_VHo_neg[k];
				nPRM->reg_LD_LUT_VHo_pos[k] = LD_LUT_VHo_pos[k];
			}
			nPRM->reg_LD_LUT_Hdg_LEXT = 2*(nPRM->reg_LD_LUT_Hdg[0])
				- (nPRM->reg_LD_LUT_Hdg[1]);
			nPRM->reg_LD_LUT_Vdg_LEXT = 2*(nPRM->reg_LD_LUT_Vdg[0])
				- (nPRM->reg_LD_LUT_Vdg[1]);
			nPRM->reg_LD_LUT_VHk_LEXT = 2*(nPRM->reg_LD_LUT_VHk[0])
				- (nPRM->reg_LD_LUT_VHk[1]);
			nPRM->reg_LD_Litgain = 256;
		}
	} else {/* DirectLit */
		/*  set reflect num */
		nPRM->reg_LD_Reflect_Hnum = 2;
		/* u3: numbers of band reflection considered in Horizontal
			direction; 0~4 */
		nPRM->reg_LD_Reflect_Vnum = 2;
		/* u3: numbers of band reflection considered in Horizontal
			direction; 0~4 */
		/* config reg_LD_BLK_Hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = (nPRM->reg_LD_pic_ColMax)/(nPRM->reg_LD_BLK_Hnum);
			nPRM->reg_LD_BLK_Hidx[k] = 0 + (k-hofst)*dlt;
			nPRM->reg_LD_BLK_Hidx[k] = (nPRM->reg_LD_BLK_Hidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Hidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Hidx[k]));/*Clip to S14*/
		}
		/* config reg_LD_BLK_Vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = (nPRM->reg_LD_pic_RowMax)/(nPRM->reg_LD_BLK_Vnum);
			nPRM->reg_LD_BLK_Vidx[k] = 0 + (k-vofst)*dlt;
			nPRM->reg_LD_BLK_Vidx[k] = (nPRM->reg_LD_BLK_Vidx[k] >
				8191) ? 8191 : ((nPRM->reg_LD_BLK_Vidx[k] <
				(-8192)) ? (-8192) :
				(nPRM->reg_LD_BLK_Vidx[k]));/*Clip to S14*/
		}
		/* configure  Hgain/Vgain */
		nPRM->reg_LD_Hgain = ((nPRM->reg_LD_BLK_Hnum)*73*2048/
			(nPRM->reg_LD_pic_ColMax));
		nPRM->reg_LD_Vgain = ((nPRM->reg_LD_BLK_Vnum)*73*2048/
			(nPRM->reg_LD_pic_RowMax));
		nPRM->reg_LD_Hgain = (nPRM->reg_LD_Hgain >
			4095) ? 4095 : (nPRM->reg_LD_Hgain);
		nPRM->reg_LD_Vgain = (nPRM->reg_LD_Vgain >
			4095) ? 4095 : (nPRM->reg_LD_Vgain);

		/*  configure */
		for (k = 0; k < LD_LUT_LEN; k++) {
			nPRM->reg_LD_LUT_Hdg[k] = drt_LD_LUT_dg[k];
			nPRM->reg_LD_LUT_Vdg[k] = drt_LD_LUT_dg[k];
			nPRM->reg_LD_LUT_VHk[k] = 128;
		}
		nPRM->reg_LD_LUT_Hdg_LEXT = 2*(nPRM->reg_LD_LUT_Hdg[0])
			- (nPRM->reg_LD_LUT_Hdg[1]);
		nPRM->reg_LD_LUT_Vdg_LEXT = 2*(nPRM->reg_LD_LUT_Vdg[0])
			- (nPRM->reg_LD_LUT_Vdg[1]);
		nPRM->reg_LD_LUT_VHk_LEXT = 2*(nPRM->reg_LD_LUT_VHk[0])
			- (nPRM->reg_LD_LUT_VHk[1]);
		nPRM->reg_LD_Litgain = 256;
		nPRM->reg_LD_BkLit_curmod = 0;
	}
	/* set one time nPRM here */
	nPRM->reg_LD_STA1max_LPF = 1;
	/* u1: STA1max statistics on [1 2 1]/4 filtered results */
	nPRM->reg_LD_STA2max_LPF = 1;
	/* u1: STA2max statistics on [1 2 1]/4 filtered results */
	nPRM->reg_LD_STAhist_LPF = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results */
	nPRM->reg_LD_X_LUT_interp_mode[0] = 0;
	nPRM->reg_LD_X_LUT_interp_mode[1] = 0;
	nPRM->reg_LD_X_LUT_interp_mode[2] = 0;
	/* set the demo window*/
	nPRM->reg_LD_xlut_demo_roi_xstart = (nPRM->reg_LD_pic_ColMax/4);
	/* u14 start col index of the region of interest */
	nPRM->reg_LD_xlut_demo_roi_xend   = (nPRM->reg_LD_pic_ColMax*3/4);
	/* u14 end col index of the region of interest */
	nPRM->reg_LD_xlut_demo_roi_ystart = (nPRM->reg_LD_pic_RowMax/4);
	/* u14 start row index of the region of interest */
	nPRM->reg_LD_xlut_demo_roi_yend  = (nPRM->reg_LD_pic_RowMax*3/4);
	/* u14 end row index of the region of interest */
	nPRM->reg_LD_xlut_iroi_enable = 1;
	/* u1: enable rgb LUT remapping inside regon of interest:
		 0: no rgb remapping; 1: enable rgb remapping */
	nPRM->reg_LD_xlut_oroi_enable = 1;
	/* u1: enable rgb LUT remapping outside regon of interest:
		 0: no rgb remapping; 1: enable rgb remapping */
}
#endif

