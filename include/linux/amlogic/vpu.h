/*
 * include/linux/amlogic/vpu.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __VPU_H__
#define __VPU_H__

#define VPU_MOD_START	100
enum vpu_mod_t {
	VPU_VIU_OSD1 = VPU_MOD_START,         /* reg0[1:0] */
	VPU_VIU_OSD2,         /* reg0[3:2] */
	VPU_VIU_VD1,          /* reg0[5:4] */
	VPU_VIU_VD2,          /* reg0[7:6] */
	VPU_VIU_CHROMA,       /* reg0[9:8] */
	VPU_VIU_OFIFO,        /* reg0[11:10] */
	VPU_VIU_SCALE,        /* reg0[13:12] */
	VPU_VIU_OSD_SCALE,    /* reg0[15:14] */
	VPU_VIU_VDIN0,        /* reg0[17:16] */
	VPU_VIU_VDIN1,        /* reg0[19:18] */
	VPU_PIC_ROT1,         /* reg0[21:20] */
	VPU_PIC_ROT2,         /* reg0[23:22] */
	VPU_PIC_ROT3,         /* reg0[25:24] */
	VPU_DI_PRE,           /* reg0[27:26] */
	VPU_DI_POST,          /* reg0[29:28] */
	VPU_SHARP,            /* reg0[31:30]  //G9TV & G9BB only */
	VPU_VIU2_OSD1,        /* reg1[1:0] */
	VPU_VIU2_OSD2,        /* reg1[3:2] */
	VPU_VIU2_VD1,         /* reg1[5:4] */
	VPU_VIU2_CHROMA,      /* reg1[7:6] */
	VPU_VIU2_OFIFO,       /* reg1[9:8] */
	VPU_VIU2_SCALE,       /* reg1[11:10] */
	VPU_VIU2_OSD_SCALE,   /* reg1[13:12] */
	VPU_VDIN_AM_ASYNC,    /* reg1[15:14]  //G9TV only */
	VPU_VDISP_AM_ASYNC,   /* reg1[17:16]  //G9TV only */
	VPU_VPUARB2_AM_ASYNC, /* reg1[19:18]  //G9TV only */
	VPU_VENCP,            /* reg1[21:20] */
	VPU_VENCL,            /* reg1[23:22] */
	VPU_VENCI,            /* reg1[25:24] */
	VPU_ISP,              /* reg1[27:26] */
	VPU_CVD2,             /* reg1[29:28]  //G9TV & G9BB only */
	VPU_ATV_DMD,          /* reg1[31:30]  //G9TV & G9BB only */
	VPU_VIU_SRSCL,        /* reg0[21:20]  //G9TV only */
	VPU_VIU_OSDSR,        /* reg0[23:22]  //G9TV only */
	VPU_REV,              /* reg0[25:24]  //G9TV only */
	VPU_D2D3,             /* reg1[3:0]    //G9TV only */
	VPU_MAX,
};

/* VPU memory power down */
#define VPU_MEM_POWER_ON		0
#define VPU_MEM_POWER_DOWN		1

extern unsigned int get_vpu_clk(void);
extern unsigned int get_vpu_clk_vmod(unsigned int vmod);
extern int request_vpu_clk_vmod(unsigned int vclk, unsigned int vmod);
extern int release_vpu_clk_vmod(unsigned int vmod);

extern void switch_vpu_mem_pd_vmod(unsigned int vmod, int flag);
extern int get_vpu_mem_pd_vmod(unsigned int vmod);
#endif
