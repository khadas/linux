/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPU_CTRL_H__
#define __VPU_CTRL_H__
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

/* #define LIMIT_VPU_CLK_LOW */

/* ************************************************ */
/* VPU frequency table, important. DO NOT modify!! */
/* ************************************************ */

#define CLK_FPLL_FREQ          2000 /* MHz */

/* G12A */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_G12A     7
#define CLK_LEVEL_MAX_G12A     8

/* T5D */
/* freq max=400M, default=400M */
#define CLK_LEVEL_DFT_T5D     5
#define CLK_LEVEL_MAX_T5D     6

/* vpu clk setting */
enum vpu_mux_e {
	FCLK_DIV4 = 0,
	FCLK_DIV3,
	FCLK_DIV5,
	FCLK_DIV7,
	MPLL_CLK1,
	VID_PLL_CLK,
	VID2_PLL_CLK,
	GPLL_CLK,
	FCLK_DIV_MAX,
};

static struct fclk_div_s fclk_div_table_g12a[] = {
	/* id,         mux,  div */
	{FCLK_DIV3,    0,    3},
	{FCLK_DIV4,    1,    4},
	{FCLK_DIV5,    2,    5},
	{FCLK_DIV7,    3,    7},
	{FCLK_DIV_MAX, 8,    1},
};

#define VPU_CLK_TOLERANCE    1000000 /* Hz */
static struct vpu_clk_s vpu_clk_table[] = {
	/* frequency   clk_mux       div */
	{100000000,    FCLK_DIV5,    3}, /* 0 */
	{166666667,    FCLK_DIV3,    3}, /* 1 */
	{200000000,    FCLK_DIV5,    1}, /* 2 */
	{250000000,    FCLK_DIV4,    1}, /* 3 */
	{333333333,    FCLK_DIV3,    1}, /* 4 */
	{400000000,    FCLK_DIV5,    0}, /* 5 */
	{500000000,    FCLK_DIV4,    0}, /* 6 */
	{666666667,    FCLK_DIV3,    0}, /* 7 */
	{696000000,    GPLL_CLK,     0}, /* 8 */
	{850000000,    GPLL_CLK,     0}, /* 9 */ /* invalid */
};

/* ************************************************ */

/* ******************************************************* */
/*                 VPU reset table                    */
/* ******************************************************* */
static struct vpu_reset_s vpu_reset_g12a[] = {
	/* reg,             mask */
	{RESET0_LEVEL, ((1 << 5) | (1 << 10) | (1 << 19) | (1 << 13))},
	{RESET1_LEVEL, (1 << 5)},
	{RESET2_LEVEL, (1 << 15)},
	{RESET3_LEVEL, ((1 << 6) | (1 << 7) | (1 << 13) | (1 << 5) |
			(1 << 9) | (1 << 4) | (1 << 12))},
	{RESET7_LEVEL, (1 << 7)},
	{VPU_REG_END, 0},
};

static struct vpu_reset_s vpu_reset_tl1[] = {
	/* reg,             mask */
	{RESET0_LEVEL, ((1 << 5) | (1 << 10) | (1 << 19) | (1 << 13))},
	{RESET1_LEVEL, ((1 << 5) | (1 << 4))},
	{RESET2_LEVEL, (1 << 15)},
	{RESET4_LEVEL, ((1 << 6) | (1 << 7) | (1 << 13) | (1 << 5) |
			(1 << 9) | (1 << 4) | (1 << 12))},
	{RESET7_LEVEL, (1 << 7)},
	{VPU_REG_END, 0},
};

/* ******************************************************* */
/*              VPU_HDMI power: before reset             */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_power_g12a[] = {
	/* 0, reg,                    val, bit, len */
	{0,   AO_RTI_GEN_PWR_SLEEP0,  1,   8,   1},
	{0,   VPU_REG_END,            0,   0,   0},
};

/* ******************************************************* */
/*                 VPU_HDMI ISO                            */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_iso_g12a[] = {
	/* 0, reg,                    val, bit, len */
	{0,   AO_RTI_GEN_PWR_SLEEP0,  1,   9,   1},
	{0,   VPU_REG_END,            0,   0,   0},
};

static struct vpu_ctrl_s vpu_iso_sm1[] = {
	/* 0, reg,                    val, bit, len */
	{0,   AO_RTI_GEN_PWR_ISO0,    1,   8,   1},
	{0,   VPU_REG_END,            0,   0,   0},
};

/* ******************************************************* */
/*                 VPU module init table                 */
/* ******************************************************* */
/*
 *static struct vpu_ctrl_s vpu_module_init_txlx[] = {
 *	// 0, reg,                       val, bit, len
 *	{0,   DOLBY_TV_CLKGATE_CTRL,     1,   10,  2},
 *	{0,   DOLBY_TV_CLKGATE_CTRL,     1,   2,   2},
 *	{0,   DOLBY_TV_CLKGATE_CTRL,     1,   4,   2},
 *	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 1,   10,  2},
 *	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 1,   2,   2},
 *	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 1,   4,   2},
 *	{0,   DOLBY_CORE3_CLKGATE_CTRL,  0,   1,   1},
 *	{0,   DOLBY_CORE3_CLKGATE_CTRL,  1,   2,   2},
 *	{0,   VPU_REG_END,               0,   0,   0},
 *};
 */

/* ******************************************************* */
/*              VPU memory power down table                */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_mem_pd_g12a[] = {
	/* vpu module,      reg,                   val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2_OSD1,       HHI_VPU_MEM_PD_REG1, 0x3,  0,   2},
	{VPU_VIU2_OFIFO,      HHI_VPU_MEM_PD_REG1, 0x3,  2,   2},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY2,          HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
/*	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2}, */
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_g12b[] = {
	/* vpu module,      reg,                   val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2_OSD1,       HHI_VPU_MEM_PD_REG1, 0x3,  0,   2},
	{VPU_VIU2_OFIFO,      HHI_VPU_MEM_PD_REG1, 0x3,  2,   2},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY2,          HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
/*	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2}, */
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_VIU2_OSD_ROT,    HHI_VPU_MEM_PD_REG2, 0x3, 22,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct vpu_ctrl_s vpu_mem_pd_tl1[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2_OSD1,       HHI_VPU_MEM_PD_REG1, 0x3,  0,   2},
	{VPU_VIU2_OFIFO,      HHI_VPU_MEM_PD_REG1, 0x3,  2,   2},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_LC_STTS,         HHI_VPU_MEM_PD_REG1, 0x3, 26,   2},
	{VPU_LDIM_STTS,       HHI_VPU_MEM_PD_REG1, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_DS,              HHI_VPU_MEM_PD_REG2, 0x3, 18,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_VIU2_OSD_ROT,    HHI_VPU_MEM_PD_REG2, 0x3, 22,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG2, 0xf, 24,   4},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3,  0,  16},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3, 16,  16},
	{VPU_AXI_WR1,         HHI_VPU_MEM_PD_REG4, 0x3,  0,   2},
	{VPU_AXI_WR0,         HHI_VPU_MEM_PD_REG4, 0x3,  2,   2},
	{VPU_AFBCE,           HHI_VPU_MEM_PD_REG4, 0x3,  4,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};
#endif

static struct vpu_ctrl_s vpu_mem_pd_sm1[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2,            HHI_VPU_MEM_PD_REG1, 0xf,  0,   4},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY2,          HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
/*	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2}, */
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_LC_STTS,         HHI_VPU_MEM_PD_REG1, 0x3, 26,   2},
	{VPU_LDIM_STTS,       HHI_VPU_MEM_PD_REG1, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_DS,              HHI_VPU_MEM_PD_REG2, 0x3, 18,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_VIU2,            HHI_VPU_MEM_PD_REG2, 0x3, 22,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG2, 0x3, 24,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3_SM1, 0x3,  0,  16},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3_SM1, 0x3, 16,  16},
	{VPU_AXI_WR1,         HHI_VPU_MEM_PD_REG4_SM1, 0x3,  0,   2},
	{VPU_AXI_WR0,         HHI_VPU_MEM_PD_REG4_SM1, 0x3,  2,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_tm2[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2,            HHI_VPU_MEM_PD_REG1, 0xf,  0,   4},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY2,          HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_LC_STTS,         HHI_VPU_MEM_PD_REG1, 0x3, 26,   2},
	{VPU_LDIM_STTS,       HHI_VPU_MEM_PD_REG1, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_DS,              HHI_VPU_MEM_PD_REG2, 0x3, 18,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG2, 0x3, 24,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG2, 0x3, 26,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG2, 0x3, 28,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3,  0,  16},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3, 16,  16},
	{VPU_AXI_WR1,         HHI_VPU_MEM_PD_REG4, 0x3,  0,   2},
	{VPU_AXI_WR0,         HHI_VPU_MEM_PD_REG4, 0x3,  2,   2},
	{VPU_AFBCE,           HHI_VPU_MEM_PD_REG4, 0x3,  4,   2},
	{VPU_VDIN_WR_MIF2,    HHI_VPU_MEM_PD_REG4, 0x3,  6,   2},
	{VPU_DMA,             HHI_VPU_MEM_PD_REG4, 0xf,  8,   4},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_tm2b[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2,            HHI_VPU_MEM_PD_REG1, 0xf,  0,   4},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY2,          HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_LC_STTS,         HHI_VPU_MEM_PD_REG1, 0x3, 26,   2},
	{VPU_LDIM_STTS,       HHI_VPU_MEM_PD_REG1, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_DS,              HHI_VPU_MEM_PD_REG2, 0x3, 18,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG2, 0x3, 24,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG2, 0x3, 26,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG2, 0x3, 28,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3,  0,  16},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3, 16,  16},
	{VPU_AXI_WR1,         HHI_VPU_MEM_PD_REG4, 0x3,  0,   2},
	{VPU_AXI_WR0,         HHI_VPU_MEM_PD_REG4, 0x3,  2,   2},
	{VPU_AFBCE,           HHI_VPU_MEM_PD_REG4, 0x3,  4,   2},
	{VPU_VDIN_WR_MIF2,    HHI_VPU_MEM_PD_REG4, 0x3,  6,   2},
	{VPU_DMA,             HHI_VPU_MEM_PD_REG4, 0xf,  8,   4},
	{VPU_HDMI,            HHI_VPU_MEM_PD_REG4, 0x3, 12,   2},
	{VPU_FGRAIN0,         HHI_VPU_MEM_PD_REG4, 0x3, 14,   2},
	{VPU_FGRAIN1,         HHI_VPU_MEM_PD_REG4, 0x3, 16,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_sc2[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        PWRCTRL_MEM_PD5_SC2, 0x3,  0,   2},
	{VPU_VIU_OSD2,        PWRCTRL_MEM_PD5_SC2, 0x3,  2,   2},
	{VPU_VIU_VD1,         PWRCTRL_MEM_PD5_SC2, 0x3,  4,   2},
	{VPU_VIU_VD2,         PWRCTRL_MEM_PD5_SC2, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      PWRCTRL_MEM_PD5_SC2, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       PWRCTRL_MEM_PD5_SC2, 0x3, 10,   2},
	{VPU_VIU_OSD_SCALE,   PWRCTRL_MEM_PD5_SC2, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       PWRCTRL_MEM_PD5_SC2, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       PWRCTRL_MEM_PD5_SC2, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       PWRCTRL_MEM_PD5_SC2, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       PWRCTRL_MEM_PD5_SC2, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    PWRCTRL_MEM_PD5_SC2, 0x3, 24,   2},
	{VPU_DI_PRE,          PWRCTRL_MEM_PD5_SC2, 0x3, 26,   2},
	{VPU_DI_POST,         PWRCTRL_MEM_PD5_SC2, 0x3, 28,   2},
	{VPU_SHARP,           PWRCTRL_MEM_PD5_SC2, 0x3, 30,   2},
	{VPU_VIU2,            PWRCTRL_MEM_PD6_SC2, 0xf,  0,   4},
	{VPU_VKSTONE,         PWRCTRL_MEM_PD6_SC2, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     PWRCTRL_MEM_PD6_SC2, 0x3,  6,   2},
	{VPU_DOLBY0,          PWRCTRL_MEM_PD6_SC2, 0x3,  8,   2},
	{VPU_DOLBY1A,         PWRCTRL_MEM_PD6_SC2, 0x3, 10,   2},
	{VPU_DOLBY1B,         PWRCTRL_MEM_PD6_SC2, 0x3, 12,   2},
	{VPU_VPU_ARB,         PWRCTRL_MEM_PD6_SC2, 0x3, 14,   2},
	{VPU_AFBC_DEC,        PWRCTRL_MEM_PD6_SC2, 0x3, 16,   2},
	{VPU_VD2_SCALE,       PWRCTRL_MEM_PD6_SC2, 0x3, 18,   2},
	{VPU_VENCP,           PWRCTRL_MEM_PD6_SC2, 0x3, 20,   2},
	{VPU_VENCL,           PWRCTRL_MEM_PD6_SC2, 0x3, 22,   2},
	{VPU_VENCI,           PWRCTRL_MEM_PD6_SC2, 0x3, 24,   2},
	{VPU_LC_STTS,         PWRCTRL_MEM_PD6_SC2, 0x3, 26,   2},
	{VPU_LDIM_STTS,       PWRCTRL_MEM_PD6_SC2, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  PWRCTRL_MEM_PD6_SC2, 0x3, 30,   2},
	{VPU_VIU_WM,          PWRCTRL_MEM_PD7_SC2, 0x3,  0,   2},
	{VPU_TCON,            PWRCTRL_MEM_PD7_SC2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        PWRCTRL_MEM_PD7_SC2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        PWRCTRL_MEM_PD7_SC2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      PWRCTRL_MEM_PD7_SC2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       PWRCTRL_MEM_PD7_SC2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       PWRCTRL_MEM_PD7_SC2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, PWRCTRL_MEM_PD7_SC2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       PWRCTRL_MEM_PD7_SC2, 0x3, 16,   2},
	{VPU_LUT3D,           PWRCTRL_MEM_PD7_SC2, 0x3, 20,   2},
	{VPU_VIU2_OSD_ROT,    PWRCTRL_MEM_PD7_SC2, 0x3, 22,   2},
	{VPU_DI_PRE,          PWRCTRL_MEM_PD7_SC2, 0x3, 24,   2},
	{VPU_DOLBY_S0,        PWRCTRL_MEM_PD7_SC2, 0x3, 26,   2},
	{VPU_DOLBY_S1,        PWRCTRL_MEM_PD7_SC2, 0x3, 28,   2},
	{VPU_TCON,            PWRCTRL_MEM_PD8_SC2, 0xffff,  0,  16},
	{VPU_TCON,            PWRCTRL_MEM_PD8_SC2, 0xffff, 16,  16},
	{VPU_AXI_WR1,         PWRCTRL_MEM_PD9_SC2, 0x3,  0,   2},
	{VPU_AXI_WR0,         PWRCTRL_MEM_PD9_SC2, 0x3,  2,   2},
	{VPU_AFBCE,           PWRCTRL_MEM_PD9_SC2, 0x3,  4,   2},
	{VPU_VDIN_WR_MIF2,    PWRCTRL_MEM_PD9_SC2, 0x3,  6,   2},
	{VPU_DMA,             PWRCTRL_MEM_PD9_SC2, 0xf,  8,   4},
	{VPU_HDMI,            PWRCTRL_MEM_PD9_SC2, 0x3,  12,  2},
	{VPU_FGRAIN0,         PWRCTRL_MEM_PD9_SC2, 0x3,  14,  2},
	{VPU_FGRAIN1,         PWRCTRL_MEM_PD9_SC2, 0x3,  16,  2},
	{VPU_DI_AFBCD,        PWRCTRL_MEM_PD9_SC2, 0x3f, 18,  6},
	{VPU_DI_AFBCE,        PWRCTRL_MEM_PD9_SC2, 0xf,  24,  4},
	{VPU_DI_DOLBY,        PWRCTRL_MEM_PD9_SC2, 0x3,  28,  2},
	{VPU_MOD_MAX,         VPU_REG_END,     0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_t5[] = {
	/* vpu module,        reg,                val,  bit, len */
	{VPU_VIU_OSD1,        PWRCTRL_MEM_PD3_T5, 0x3,  0,   2},
	{VPU_VIU_OSD2,        PWRCTRL_MEM_PD3_T5, 0x3,  2,   2},
	{VPU_VIU_VD1,         PWRCTRL_MEM_PD3_T5, 0x3,  4,   2},
	{VPU_VIU_VD2,         PWRCTRL_MEM_PD3_T5, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      PWRCTRL_MEM_PD3_T5, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       PWRCTRL_MEM_PD3_T5, 0x3, 10,   2},
	{VPU_VIU_OSD_SCALE,   PWRCTRL_MEM_PD3_T5, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       PWRCTRL_MEM_PD3_T5, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       PWRCTRL_MEM_PD3_T5, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       PWRCTRL_MEM_PD3_T5, 0x3, 20,   2},
	{VPU_DI_PRE,          PWRCTRL_MEM_PD3_T5, 0x3, 26,   2},
	{VPU_DI_POST,         PWRCTRL_MEM_PD3_T5, 0x3, 28,   2},
	{VPU_SHARP,           PWRCTRL_MEM_PD3_T5, 0x3, 30,   2},
	{VPU_VIU2,            PWRCTRL_MEM_PD4_T5, 0xf,  0,   4},
	{VPU_VPU_ARB,         PWRCTRL_MEM_PD4_T5, 0x3, 14,   2},
	{VPU_AFBC_DEC,        PWRCTRL_MEM_PD4_T5, 0x3, 16,   2},
	{VPU_VD2_SCALE,       PWRCTRL_MEM_PD4_T5, 0x3, 18,   2},
	{VPU_VENCP,           PWRCTRL_MEM_PD4_T5, 0x3, 20,   2},
	{VPU_VENCL,           PWRCTRL_MEM_PD4_T5, 0x3, 22,   2},
	{VPU_VENCI,           PWRCTRL_MEM_PD4_T5, 0x3, 24,   2},
	{VPU_LC_STTS,         PWRCTRL_MEM_PD4_T5, 0x3, 26,   2},
	{VPU_VD2_OSD2_SCALE,  PWRCTRL_MEM_PD4_T5, 0x3, 30,   2},
	{VPU_VIU_WM,          PWRCTRL_MEM_PD5_T5, 0x3,  0,   2},
	{VPU_TCON,            PWRCTRL_MEM_PD5_T5, 0x3,  2,   2},
	{VPU_MAIL_AFBCD,      PWRCTRL_MEM_PD5_T5, 0x3,  8,   2},
	{VPU_VD1_SCALE,       PWRCTRL_MEM_PD5_T5, 0x3, 10,   2},
	{VPU_VD2_OFIFO,       PWRCTRL_MEM_PD5_T5, 0x3, 16,   2},
	{VPU_LUT3D,           PWRCTRL_MEM_PD5_T5, 0x3, 20,   2},
	{VPU_VIU2_OSD_ROT,    PWRCTRL_MEM_PD5_T5, 0x3, 22,   2},
	{VPU_DI_PRE,          PWRCTRL_MEM_PD5_T5, 0x3, 24,   2},
	{VPU_RDMA,            PWRCTRL_MEM_PD5_T5, 0x3, 30,   2},
	{VPU_TCON,            PWRCTRL_MEM_PD6_T5, 0x3,  0,  16},
	{VPU_TCON,            PWRCTRL_MEM_PD6_T5, 0x3, 16,  16},
	{VPU_AXI_WR1,         PWRCTRL_MEM_PD7_T5, 0x3,  0,   2},
	{VPU_AXI_WR0,         PWRCTRL_MEM_PD7_T5, 0x3,  2,   2},
	{VPU_AFBCE,           PWRCTRL_MEM_PD7_T5, 0x3,  4,   2},
	{VPU_VDIN_WR_MIF2,    PWRCTRL_MEM_PD7_T5, 0x3,  6,   2},
	{VPU_DMA,             PWRCTRL_MEM_PD7_T5, 0xf,  8,   4},
	{VPU_HDMI,            PWRCTRL_MEM_PD7_T5, 0x3, 12,   2},
	{VPU_FGRAIN0,         PWRCTRL_MEM_PD7_T5, 0x3, 14,   2},
	{VPU_FGRAIN1,         PWRCTRL_MEM_PD7_T5, 0x3, 16,   2},
	{VPU_DECONTOUR,       PWRCTRL_MEM_PD7_T5, 0x3, 18,   2},
	{VPU_MOD_MAX,         VPU_REG_END,     0,    0,   0},
};

/* ******************************************************* */
/*                 VPU pwrctrl id table                 */
/* ******************************************************* */
static unsigned int vpu_pwrctrl_id_table[] = {
	PM_VPU_HDMI_SC2,
	VPU_PWR_ID_END
};

static unsigned int vpu_pwrctrl_id_table_t7[] = {
	PM_VPU_HDMI_T7,
	PM_VI_CLK1_T7,
	PM_VI_CLK2_T7,
	VPU_PWR_ID_END
};

/* ******************************************************* */
/*                 VPU clock gate table                    */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_clk_gate_g12a[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1}, /*vpu_sys_clk*/
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  18,   1},
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  17,   1},
	{VPU_CLK_VIB,       VPU_CLK_GATE,         1,  16,   1},
	{VPU_RDMA,          VPU_CLK_GATE,         1,  15,   1}, /*rdma_clk*/
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1}, /*hs,vs intr*/
	{VPU_VENCP,         VPU_CLK_GATE,         1,   3,   1},
	{VPU_VENCP,         VPU_CLK_GATE,         1,   0,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VENCI,         VPU_CLK_GATE,         1,  10,   2},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

#endif
