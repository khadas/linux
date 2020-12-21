/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/deinterlace/sc2/di_afbc_v3.h
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

#ifndef __DI_AFBC_H__
#define __DI_AFBC_H__

#define AFBC_REG_INDEX_NUB	(18)
/*also see enum eAFBC_DEC*/
#define AFBC_DEC_NUB		(4)
#define AFBC_ENC_NUB		(1)

#define AFBC_DEC_NUB_V5		(8)
#define AFBC_ENC_NUB_V5		(2)

enum EAFBC_REG {
	EAFBC_ENABLE,
	EAFBC_MODE,
	EAFBC_SIZE_IN,
	EAFBC_DEC_DEF_COLOR,
	EAFBC_CONV_CTRL,
	EAFBC_LBUF_DEPTH,
	EAFBC_HEAD_BADDR,
	EAFBC_BODY_BADDR,
	EAFBC_SIZE_OUT,
	EAFBC_OUT_YSCOPE,
	EAFBC_STAT,
	EAFBC_VD_CFMT_CTRL,
	EAFBC_VD_CFMT_W,
	EAFBC_MIF_HOR_SCOPE,
	EAFBC_MIF_VER_SCOPE,
	EAFBC_PIXEL_HOR_SCOPE,
	EAFBC_PIXEL_VER_SCOPE,
	EAFBC_VD_CFMT_H,
};

#define DIM_AFBCE_NUB	(44)//for sc2 add 5 register (39)
#define DIM_AFBCE_UP_NUB	(21)

/* afbce bytes index */
enum EAFBCE_INDEX {
	EAFBCE_ENABLE,	/* 0 */
	EAFBCE_MODE,	/* 1 */
	EAFBCE_SIZE_IN,	/* 2 */
	EAFBCE_BLK_SIZE_IN,	/* 3 */
	EAFBCE_HEAD_BADDR,	/* 4 */
	EAFBCE_MIF_SIZE,	/* 5 */
	EAFBCE_PIXEL_IN_HOR_SCOPE,	/* 6 */
	EAFBCE_PIXEL_IN_VER_SCOPE,	/* 7 */
	EAFBCE_CONV_CTRL,	/* 8 */
	EAFBCE_MIF_HOR_SCOPE,	/* 9 */
	EAFBCE_MIF_VER_SCOPE,	/* 10 */

	EAFBCE_FORMAT,		/* 11 */
	EAFBCE_DEFCOLOR_1,	/* 12 */
	EAFBCE_DEFCOLOR_2,	/* 13 */
	EAFBCE_QUANT_ENABLE,	/* 14 */

	EAFBCE_MMU_RMIF_CTRL1,	/* 15 */
	EAFBCE_MMU_RMIF_CTRL2,	/* 16 */
	EAFBCE_MMU_RMIF_CTRL3,	/* 17 */
	EAFBCE_MMU_RMIF_CTRL4,	/* 18 */
	EAFBCE_MMU_RMIF_SCOPE_X,	/* 29 */
	EAFBCE_MMU_RMIF_SCOPE_Y,	/* 20 */

	/**********************/
	EAFBCE_MODE_EN,
	EAFBCE_DWSCALAR,
	EAFBCE_IQUANT_LUT_1,
	EAFBCE_IQUANT_LUT_2,
	EAFBCE_IQUANT_LUT_3,
	EAFBCE_IQUANT_LUT_4,
	EAFBCE_RQUANT_LUT_1,
	EAFBCE_RQUANT_LUT_2,
	EAFBCE_RQUANT_LUT_3,
	EAFBCE_RQUANT_LUT_4,
	EAFBCE_YUV_FORMAT_CONV_MODE,
	EAFBCE_DUMMY_DATA,
	EAFBCE_CLR_FLAG,
	EAFBCE_STA_FLAGT,
	EAFBCE_MMU_NUM,
	EAFBCE_STAT1,		/*read only*/
	EAFBCE_STAT2,
	EAFBCE_MMU_RMIF_RO_STAT,
	/* from sc2 */
	EAFBCE_PIP_CTRL,
	EAFBCE_ROT_CTRL,
	EAFBCE_DIMM_CTRL,
	EAFBCE_BND_DEC_MISC,
	EAFBCE_RD_ARB_MISC,
};

enum AFBC_WK_MODE {
	AFBC_WK_NONE,
	AFBC_WK_IN,
	AFBC_WK_P,
	AFBC_WK_6D,
	AFBC_WK_6D_ALL,
	AFBC_WK_6D_NV21,
};

#define AFBC_VTYPE_MASK_CHG	(VIDTYPE_INTERLACE	|	\
				 VIDTYPE_VIU_422	|	\
				 VIDTYPE_VIU_444	|	\
				 VIDTYPE_VIU_NV21	|	\
				 VIDTYPE_VIU_NV12	|	\
				 VIDTYPE_COMPRESS	|	\
				 VIDTYPE_SCATTER	|	\
				 VIDTYPE_COMB_MODE)

#define AFBC_VTYPE_MASK_OTHER	(VIDTYPE_COMPRESS	|	\
				 VIDTYPE_SCATTER	|	\
				 VIDTYPE_COMB_MODE)

#define AFBC_VTYPE_MASK_SAV	(VIDTYPE_TYPEMASK	|	\
				 VIDTYPE_VIU_422	|	\
				 VIDTYPE_VIU_444	|	\
				 VIDTYPE_VIU_NV21	|	\
				 VIDTYPE_VIU_NV12	|	\
				 VIDTYPE_COMPRESS	|	\
				 VIDTYPE_SCATTER	|	\
				 VIDTYPE_COMB_MODE)

#define AFBCD_NONE	(0)
#define AFBCD_V1	(1) /*gxl and txlx*/
#define AFBCD_V2	(2) /*g12a*/
#define AFBCD_V3	(3) /*tl1*/
#define AFBCD_V4	(4) /*tm2 vb*/
#define AFBCD_V5	(5)	/*sc2*/

//void afbc_prob(void);

//unsigned int afbc_count_info_size(unsigned int w, unsigned int h);

//extern struct afd_ops_s di_afd_ops;

//const unsigned int *afbc_get_addrp(enum eAFBC_DEC eidx);
//const unsigned int *afbce_get_addrp(enum EAFBC_ENC eidx);
/*define in vpp*/
s32 di_request_afbc_hw(u8 id, bool on);
unsigned int afbce_read_used(enum EAFBC_ENC enc);
//bool cfg_pmode(void);
//bool cfg_test4k(void);
//bool dbg_di_prelink(void);
//void dbg_afd_reg(struct seq_file *s, enum EAFBC_DEC eidx);
#endif	/*__DI_AFBC_H__*/
