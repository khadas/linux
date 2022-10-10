/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 *
 */
#include "../../lcd_reg.h"

#define LDIM_BL_ADDR_PORT			0x144e
#define LDIM_BL_DATA_PORT			0x144f
#define ASSIST_SPARE8_REG1			0x1f58

/*gxtvbb new add*/
#define LDIM_STTS_GCLK_CTRL0			0x1ac0
#define LDIM_STTS_CTRL0				0x1ac1
#define LDIM_STTS_WIDTHM1_HEIGHTM1		0x1ac2
#define LDIM_STTS_MATRIX_COEF00_01		0x1ac3
#define LDIM_STTS_MATRIX_COEF02_10		0x1ac4
#define LDIM_STTS_MATRIX_COEF11_12		0x1ac5
#define LDIM_STTS_MATRIX_COEF20_21		0x1ac6
#define LDIM_STTS_MATRIX_COEF22			0x1ac7
#define LDIM_STTS_MATRIX_OFFSET0_1		0x1ac8
#define LDIM_STTS_MATRIX_OFFSET2		0x1ac9
#define LDIM_STTS_MATRIX_PRE_OFFSET0_1		0x1aca
#define LDIM_STTS_MATRIX_PRE_OFFSET2		0x1acb
#define LDIM_STTS_MATRIX_HL_COLOR		0x1acc
#define LDIM_STTS_MATRIX_PROBE_POS		0x1acd
#define LDIM_STTS_MATRIX_PROBE_COLOR		0x1ace
#define LDIM_STTS_HIST_REGION_IDX		0x1ad0
#define LDIM_STTS_HIST_SET_REGION		0x1ad1
#define LDIM_STTS_HIST_READ_REGION		0x1ad2
#define LDIM_STTS_HIST_START_RD_REGION		0x1ad3

#define VDIN0_HIST_CTRL				0x1230

/*** GXTVBB & TXLX common use register*/
/* each base has 16 address space */
#define REG_LD_CFG_BASE           0x00
#define REG_LD_RGB_IDX_BASE       0x10
#define REG_LD_RGB_LUT_BASE       0x2000
#define REG_LD_MATRIX_BASE        0x3000
/* LD_CFG_BASE */
#define REG_LD_FRM_SIZE           0x0
#define REG_LD_RGB_MOD            0x1
#define REG_LD_BLK_HVNUM          0x2
#define REG_LD_HVGAIN             0x3
#define REG_LD_BKLIT_VLD          0x4
#define REG_LD_BKLIT_PARAM        0x5
#define REG_LD_LIT_GAIN_COMP      0x7
#define REG_LD_FRM_RST_POS        0x8
#define REG_LD_FRM_BL_START_POS   0x9
#define REG_LD_MISC_CTRL0         0xa
#define REG_LD_FRM_HBLAN_VHOLS    0xb
#define REG_LD_XLUT_DEMO_ROI_XPOS 0xc
#define REG_LD_XLUT_DEMO_ROI_YPOS 0xd
#define REG_LD_XLUT_DEMO_ROI_CTRL 0xe

/* each base has 16 address space */
/******  GXTVBB ******/
#define REG_LD_G_IDX_BASE          0x20
#define REG_LD_B_IDX_BASE          0x30
#define REG_LD_RGB_NRMW_BASE       0x40
#define REG_LD_LUT_HDG_BASE        0x50
#define REG_LD_LUT_VHK_BASE        0x60
#define REG_LD_LUT_VDG_BASE        0x70
#define REG_LD_BLK_HIDX_BASE       0x80
#define REG_LD_BLK_VIDX_BASE       0xa0
#define REG_LD_LUT_VHK_NEGPOS_BASE 0xc0
#define REG_LD_LUT_VHO_NEGPOS_BASE 0xd0
#define REG_LD_LUT_HHK_BASE        0xe0
#define REG_LD_REFLECT_DGR_BASE    0xf0
/* LD_CFG_BASE */
#define REG_LD_LUT_XDG_LEXT        0x6

/* each base has 16 address space */
/******  TXLX  ******/
#define REG_LD_RGB_NRMW_BASE_TXLX       0x20
#define REG_LD_BLK_HIDX_BASE_TXLX       0x30
#define REG_LD_BLK_VIDX_BASE_TXLX       0x50
#define REG_LD_LUT_VHK_NEGPOS_BASE_TXLX 0x60
#define REG_LD_LUT_VHO_NEGPOS_BASE_TXLX 0x70
#define REG_LD_LUT_HHK_BASE_TXLX        0x80
#define REG_LD_REFLECT_DGR_BASE_TXLX    0x90
#define REG_LD_LUT_LEXT_BASE_TXLX       0xa0
#define REG_LD_LUT_HDG_BASE_TXLX        0x100
#define REG_LD_LUT_VDG_BASE_TXLX        0x180
#define REG_LD_LUT_VHK_BASE_TXLX        0x200
#define REG_LD_LUT_ID_BASE_TXLX         0x300
/* LD_CFG_BASE */
#define REG_LD_BLMAT_RAM_MISC           0xf

/* each base has 16 address space */
/******  TM2  ******/
#define REG_LD_RGB_NRMW_BASE_TM2       0x20
#define REG_LD_BLK_HIDX_BASE_TM2       0x30
#define REG_LD_BLK_VIDX_BASE_TM2       0x50
#define REG_LD_LUT_VHK_NEGPOS_BASE_TM2 0x70
#define REG_LD_LUT_VHO_NEGPOS_BASE_TM2 0x80
#define REG_LD_LUT_HHK_BASE_TM2        0x90
#define REG_LD_REFLECT_DGR_BASE_TM2    0xa0
#define REG_LD_LUT_LEXT_BASE_TM2       0xb0
#define REG_LD_LUT_HDG_BASE_TM2        0x100
#define REG_LD_LUT_VDG_BASE_TM2        0x200
#define REG_LD_LUT_VHK_BASE_TM2        0x300
#define REG_LD_LUT_ID_BASE_TM2         0x400

/* #define LDIM_STTS_HIST_REGION_IDX      0x1aa0 */
#define LOCAL_DIM_STATISTIC_EN_BIT          31
#define LOCAL_DIM_STATISTIC_EN_WID           1
#define EOL_EN_BIT                          28
#define EOL_EN_WID                           1

/* 0: 17 pix, 1: 9 pix, 2: 5 pix, 3: 3 pix, 4: 0 pix */
#define HOVLP_NUM_SEL_BIT                   21
#define HOVLP_NUM_SEL_WID                    2
#define LPF_BEFORE_STATISTIC_EN_BIT         20
#define LPF_BEFORE_STATISTIC_EN_WID          1
#define BLK_HV_POS_IDX_BIT                 16
#define BLK_HV_POS_IDX_WID                  4
#define RD_INDEX_INC_MODE_BIT               14
#define RD_INDEX_INC_MODE_WID                2
#define REGION_RD_SUB_INDEX_BIT              8
#define REGION_RD_SUB_INDEX_WID              4
#define REGION_RD_INDEX_BIT                  0
#define REGION_RD_INDEX_WID                  7

// configure DMA
#define VPU_DMA_RDMIF_CTRL3				0x27cc
#define VPU_DMA_RDMIF_BADDR0				0x27cd
#define VPU_DMA_WRMIF_CTRL1				0x27d1
#define VPU_DMA_WRMIF_CTRL2				0x27d2
#define VPU_DMA_WRMIF_CTRL3				0x27d3
#define VPU_DMA_WRMIF_BADDR0				0x27d4
#define VPU_DMA_RDMIF_CTRL				0x27d8
#define VPU_DMA_RDMIF_BADDR1				0x27d9
#define VPU_DMA_RDMIF_BADDR2				0x27da
#define VPU_DMA_RDMIF_BADDR3				0x27db
#define VPU_DMA_WRMIF_CTRL				0x27dc
#define VPU_DMA_WRMIF_BADDR1				0x27dd
#define VPU_DMA_WRMIF_BADDR2				0x27de
#define VPU_DMA_WRMIF_BADDR3				0x27df

#define VPU_DMA_RDMIF0_CTRL                        0x2750
#define VPU_DMA_RDMIF0_BADR0                       0x2758
//Bit 31:0  lut0_reg_baddr0
#define VPU_DMA_RDMIF0_BADR1                       0x2759
//Bit 31:0  lut0_reg_baddr1
#define VPU_DMA_RDMIF0_BADR2                       0x275a
//Bit 31:0  lut0_reg_baddr2
#define VPU_DMA_RDMIF0_BADR3                       0x275b
//Bit 31:0  lut0_reg_baddr3

/* *************************************************************************
 * T7 LDC register
 */
#define LDC_REG_BLOCK_NUM                          0x1400
#define LDC_REG_SEG_Y_BOUNDARY_0_1                 0x1401
#define LDC_REG_SEG_X_BOUNDARY_0_1                 0x1411
#define LDC_REG_PANEL_SIZE                         0x1429
#define LDC_REG_DOWNSAMPLE                         0x142a
#define LDC_REG_HIST_OVERLAP                       0x142b
#define LDC_REG_BLEND_DIFF_TH                      0x142c
#define LDC_REG_CURVE_COEF                         0x142d
#define LDC_REG_INIT_BL                            0x142e
#define LDC_REG_SF_MODE                            0x142f
#define LDC_REG_SF_GAIN                            0x1430
#define LDC_REG_BS_MODE                            0x1431
#define LDC_REG_APL                                0x1432
#define LDC_REG_GLB_BOOST                          0x1433
#define LDC_REG_LOCAL_BOOST                        0x1434
#define LDC_REG_TF                                 0x1435
#define LDC_REG_TF_SC                              0x1436
#define LDC_REG_PROFILE_MODE                       0x1437
#define LDC_REG_BLK_FILTER                         0x1438
#define LDC_REG_BLK_FILTER_COEF                    0x1439
#define LDC_REG_BL_MEMORY                          0x143a
#define LDC_REG_FACTOR_DIV_0                       0x143b
#define LDC_REG_FACTOR_DIV_1                       0x143c
#define LDC_REG_BITS_DIV                           0x143d
#define LDC_REG_GLB_GAIN                           0x143e
#define LDC_REG_DITHER                             0x143f
#define LDC_REG_MIN_GAIN_LUT_0                     0x1440
#define LDC_REG_MIN_GAIN_LUT_1                     0x1441
#define LDC_REG_MIN_GAIN_LUT_2                     0x1442
#define LDC_REG_MIN_GAIN_LUT_3                     0x1443
#define LDC_REG_MIN_GAIN_LUT_4                     0x1444
#define LDC_REG_MIN_GAIN_LUT_5                     0x1445
#define LDC_REG_MIN_GAIN_LUT_6                     0x1446
#define LDC_REG_MIN_GAIN_LUT_7                     0x1447
#define LDC_REG_MIN_GAIN_LUT_8                     0x1448
#define LDC_REG_MIN_GAIN_LUT_9                     0x1449
#define LDC_REG_MIN_GAIN_LUT_10                    0x144a
#define LDC_REG_MIN_GAIN_LUT_11                    0x144b
#define LDC_REG_MIN_GAIN_LUT_12                    0x144c
#define LDC_REG_MIN_GAIN_LUT_13                    0x144d
#define LDC_REG_MIN_GAIN_LUT_14                    0x144e
#define LDC_REG_MIN_GAIN_LUT_15                    0x144f
#define LDC_REG_DITHER_LUT_1_0                     0x1450
#define LDC_REG_LDC_DITHER_LUT_1_1                 0x1451
#define LDC_REG_LDC_DITHER_LUT_1_2                 0x1452
#define LDC_REG_LDC_DITHER_LUT_1_3                 0x1453
#define LDC_REG_LDC_DITHER_LUT_1_4                 0x1454
#define LDC_REG_LDC_DITHER_LUT_1_5                 0x1455
#define LDC_REG_LDC_DITHER_LUT_1_6                 0x1456
#define LDC_REG_LDC_DITHER_LUT_1_7                 0x1457
#define LDC_REG_LDC_DITHER_LUT_1_8                 0x1458
#define LDC_REG_LDC_DITHER_LUT_1_9                 0x1459
#define LDC_REG_LDC_DITHER_LUT_1_10                0x145a
#define LDC_REG_LDC_DITHER_LUT_1_11                0x145b
#define LDC_REG_LDC_DITHER_LUT_1_12                0x145c
#define LDC_REG_LDC_DITHER_LUT_1_13                0x145d
#define LDC_REG_LDC_DITHER_LUT_1_14                0x145e
#define LDC_REG_LDC_DITHER_LUT_1_15                0x145f
#define LDC_REG_LDC_DITHER_LUT_1_16                0x1460
#define LDC_REG_LDC_DITHER_LUT_1_17                0x1461
#define LDC_REG_LDC_DITHER_LUT_1_18                0x1462
#define LDC_REG_LDC_DITHER_LUT_1_19                0x1463
#define LDC_REG_LDC_DITHER_LUT_1_20                0x1464
#define LDC_REG_LDC_DITHER_LUT_1_21                0x1465
#define LDC_REG_LDC_DITHER_LUT_1_22                0x1466
#define LDC_REG_LDC_DITHER_LUT_1_23                0x1467
#define LDC_REG_LDC_DITHER_LUT_1_24                0x1468
#define LDC_REG_LDC_DITHER_LUT_1_25                0x1469
#define LDC_REG_LDC_DITHER_LUT_1_26                0x146a
#define LDC_REG_LDC_DITHER_LUT_1_27                0x146b
#define LDC_REG_LDC_DITHER_LUT_1_28                0x146c
#define LDC_REG_LDC_DITHER_LUT_1_29                0x146d
#define LDC_REG_LDC_DITHER_LUT_1_30                0x146e
#define LDC_REG_LDC_DITHER_LUT_1_31                0x146f
#define LDC_SEG_INFO_SEL                           0x1470
#define LDC_DDR_ADDR_BASE                          0x1471
#define LDC_GAIN_LUT_DATA                          0x1472
#define LDC_ADJ_VS_CTRL                            0x1476
#define LDC_CTRL_MISC0                             0x1478
#define LDC_CTRL_MISC1                             0x1479
#define LDC_GAIN_LUT_ADDR                          0x1473
#define LDC_GAIN_LUT_CTRL0                         0x1474
#define LDC_GAIN_LUT_CTRL1                         0x1475
#define LDC_DGB_CTRL                               0x147a
#define LDC_RO_BL_MEMORY_IDX                       0x147b
#define LDC_RO_GLB_HIST_SUM                        0x147c
#define LDC_RO_GLB_HIST_CNT                        0x147d
#define LDC_RO_GAIN_SMP_DATA                       0x147f
#define LDC_RO_PREF_CHK_ERROR                      0x1480
#define VPU_RDARB_MODE_L2C1                        0x279d

/* *************************************************************************/

#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
#define LDIM_VSYNC_RDMA      0
#else
#define LDIM_VSYNC_RDMA      0
#endif

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);

void ldim_wr_vcbus(unsigned int addr, unsigned int val);
unsigned int ldim_rd_vcbus(unsigned int addr);
void ldim_wr_vcbus_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len);
unsigned int ldim_rd_vcbus_bits(unsigned int addr,
				unsigned int start, unsigned int len);
void ldim_wr_reg_bits(unsigned int addr, unsigned int val,
				    unsigned int start, unsigned int len);
void ldim_wr_reg(unsigned int addr, unsigned int val);
unsigned int ldim_rd_reg(unsigned int addr);
