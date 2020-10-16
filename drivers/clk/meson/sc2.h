/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 *
 * Copyright (c) 2018 Amlogic, inc.
 *
 */

#ifndef __SC2_H
#define __SC2_H

/*
 * Clock controller register offsets
 * REG_BASE:  REGISTER_BASE_ADDR = 0xfe000000
 */
#define CLKCTRL_OSCIN_CTRL                         ((0x0001 << 2))
#define CLKCTRL_RTC_BY_OSCIN_CTRL0                 ((0x0002 << 2))
#define CLKCTRL_RTC_BY_OSCIN_CTRL1                 ((0x0003 << 2))
#define CLKCTRL_RTC_CTRL                           ((0x0004 << 2))
#define CLKCTRL_CHECK_CLK_RESULT                   ((0x0005 << 2))
#define CLKCTRL_MBIST_ATSPEED_CTRL                 ((0x0006 << 2))
#define CLKCTRL_LOCK_BIT_REG0                      ((0x0008 << 2))
#define CLKCTRL_LOCK_BIT_REG1                      ((0x0009 << 2))
#define CLKCTRL_LOCK_BIT_REG2                      ((0x000a << 2))
#define CLKCTRL_LOCK_BIT_REG3                      ((0x000b << 2))
#define CLKCTRL_PROT_BIT_REG0                      ((0x000c << 2))
#define CLKCTRL_PROT_BIT_REG1                      ((0x000d << 2))
#define CLKCTRL_PROT_BIT_REG2                      ((0x000e << 2))
#define CLKCTRL_PROT_BIT_REG3                      ((0x000f << 2))
#define CLKCTRL_SYS_CLK_CTRL0                      ((0x0010 << 2))
#define CLKCTRL_SYS_CLK_EN0_REG0                   ((0x0011 << 2))
#define CLKCTRL_SYS_CLK_EN0_REG1                   ((0x0012 << 2))
#define CLKCTRL_SYS_CLK_EN0_REG2                   ((0x0013 << 2))
#define CLKCTRL_SYS_CLK_EN0_REG3                   ((0x0014 << 2))
#define CLKCTRL_SYS_CLK_EN1_REG0                   ((0x0015 << 2))
#define CLKCTRL_SYS_CLK_EN1_REG1                   ((0x0016 << 2))
#define CLKCTRL_SYS_CLK_EN1_REG2                   ((0x0017 << 2))
#define CLKCTRL_SYS_CLK_EN1_REG3                   ((0x0018 << 2))
#define CLKCTRL_SYS_CLK_VPU_EN0                    ((0x0019 << 2))
#define CLKCTRL_SYS_CLK_VPU_EN1                    ((0x001a << 2))
#define CLKCTRL_AXI_CLK_CTRL0                      ((0x001b << 2))
#define CLKCTRL_TST_CTRL0                          ((0x0020 << 2))
#define CLKCTRL_TST_CTRL1                          ((0x0021 << 2))
#define CLKCTRL_CECA_CTRL0                         ((0x0022 << 2))
#define CLKCTRL_CECA_CTRL1                         ((0x0023 << 2))
#define CLKCTRL_CECB_CTRL0                         ((0x0024 << 2))
#define CLKCTRL_CECB_CTRL1                         ((0x0025 << 2))
#define CLKCTRL_SC_CLK_CTRL                        ((0x0026 << 2))
#define CLKCTRL_DSPA_CLK_CTRL0                     ((0x0027 << 2))
#define CLKCTRL_DSPB_CLK_CTRL0                     ((0x0028 << 2))
#define CLKCTRL_RAMA_CLK_CTRL0                     ((0x0029 << 2))
#define CLKCTRL_CLK12_24_CTRL                      ((0x002a << 2))
#define CLKCTRL_VID_CLK_CTRL                       ((0x0030 << 2))
#define CLKCTRL_VID_CLK_CTRL2                      ((0x0031 << 2))
#define CLKCTRL_VID_CLK_DIV                        ((0x0032 << 2))
#define CLKCTRL_VIID_CLK_DIV                       ((0x0033 << 2))
#define CLKCTRL_VIID_CLK_CTRL                      ((0x0034 << 2))
#define CLKCTRL_HDMI_CLK_CTRL                      ((0x0038 << 2))
#define CLKCTRL_VID_PLL_CLK_DIV                    ((0x0039 << 2))
#define CLKCTRL_VPU_CLK_CTRL                       ((0x003a << 2))
#define CLKCTRL_VPU_CLKB_CTRL                      ((0x003b << 2))
#define CLKCTRL_VPU_CLKC_CTRL                      ((0x003c << 2))
#define CLKCTRL_VID_LOCK_CLK_CTRL                  ((0x003d << 2))
#define CLKCTRL_VDIN_MEAS_CLK_CTRL                 ((0x003e << 2))
#define CLKCTRL_VAPBCLK_CTRL                       ((0x003f << 2))
#define CLKCTRL_HDCP22_CTRL                        ((0x0040 << 2))
#define CLKCTRL_MIPIDSI_PHY_CLK_CTRL               ((0x0041 << 2))
#define CLKCTRL_CDAC_CLK_CTRL                      ((0x0042 << 2))
#define CLKCTRL_MIPI_CSI_PHY_CLK_CTRL              ((0x0043 << 2))
#define CLKCTRL_CSI2_ADAPT_CLK_CTRL                ((0x0044 << 2))
#define CLKCTRL_VDEC_CLK_CTRL                      ((0x0050 << 2))
#define CLKCTRL_VDEC2_CLK_CTRL                     ((0x0051 << 2))
#define CLKCTRL_VDEC3_CLK_CTRL                     ((0x0052 << 2))
#define CLKCTRL_VDEC4_CLK_CTRL                     ((0x0053 << 2))
#define CLKCTRL_WAVE420L_CLK_CTRL                  ((0x0054 << 2))
#define CLKCTRL_WAVE420L_CLK_CTRL2                 ((0x0055 << 2))
#define CLKCTRL_TS_CLK_CTRL                        ((0x0056 << 2))
#define CLKCTRL_MALI_CLK_CTRL                      ((0x0057 << 2))
#define CLKCTRL_VIPNANOQ_CLK_CTRL                  ((0x0058 << 2))
#define CLKCTRL_ETH_CLK_CTRL                       ((0x0059 << 2))
#define CLKCTRL_NAND_CLK_CTRL                      ((0x005a << 2))
#define CLKCTRL_SD_EMMC_CLK_CTRL                   ((0x005b << 2))
#define CLKCTRL_BT656_CLK_CTRL                     ((0x005c << 2))
#define CLKCTRL_SPICC_CLK_CTRL                     ((0x005d << 2))
#define CLKCTRL_GEN_CLK_CTRL                       ((0x005e << 2))
#define CLKCTRL_SAR_CLK_CTRL                       ((0x005f << 2))
#define CLKCTRL_PWM_CLK_AB_CTRL                    ((0x0060 << 2))
#define CLKCTRL_PWM_CLK_CD_CTRL                    ((0x0061 << 2))
#define CLKCTRL_PWM_CLK_EF_CTRL                    ((0x0062 << 2))
#define CLKCTRL_PWM_CLK_GH_CTRL                    ((0x0063 << 2))
#define CLKCTRL_PWM_CLK_IJ_CTRL                    ((0x0064 << 2))
#define CLKCTRL_TIMESTAMP_CTRL                     ((0x0100 << 2))
#define CLKCTRL_TIMESTAMP_CTRL1                    ((0x0101 << 2))
#define CLKCTRL_TIMESTAMP_CTRL2                    ((0x0103 << 2))
#define CLKCTRL_TIMESTAMP_RD0                      ((0x0104 << 2))
#define CLKCTRL_TIMESTAMP_RD1                      ((0x0105 << 2))
#define CLKCTRL_TIMEBASE_CTRL0                     ((0x0106 << 2))
#define CLKCTRL_TIMEBASE_CTRL1                     ((0x0107 << 2))
#define CLKCTRL_EFUSE_CPU_CFG01                    ((0x0120 << 2))
#define CLKCTRL_EFUSE_CPU_CFG2                     ((0x0121 << 2))
#define CLKCTRL_EFUSE_ENCP_CFG0                    ((0x0122 << 2))
#define CLKCTRL_EFUSE_MALI_CFG01                   ((0x0123 << 2))
#define CLKCTRL_EFUSE_HEVCB_CFG01                  ((0x0124 << 2))
#define CLKCTRL_EFUSE_HEVCB_CFG2                   ((0x0125 << 2))
#define CLKCTRL_EFUSE_LOCK                         ((0x0126 << 2))

/*  ANA_CTRL - Registers
 *REG_BASE:  REGISTER_BASE_ADDR = 0xfe008000
 */
#define ANACTRL_SYSPLL_CTRL0                       ((0x0000 << 2))
#define ANACTRL_SYSPLL_CTRL1                       ((0x0001 << 2))
#define ANACTRL_SYSPLL_CTRL2                       ((0x0002 << 2))
#define ANACTRL_SYSPLL_CTRL3                       ((0x0003 << 2))
#define ANACTRL_SYSPLL_CTRL4                       ((0x0004 << 2))
#define ANACTRL_SYSPLL_CTRL5                       ((0x0005 << 2))
#define ANACTRL_SYSPLL_CTRL6                       ((0x0006 << 2))
#define ANACTRL_SYSPLL_STS                         ((0x0007 << 2))
#define ANACTRL_FIXPLL_CTRL0                       ((0x0010 << 2))
#define ANACTRL_FIXPLL_CTRL1                       ((0x0011 << 2))
#define ANACTRL_FIXPLL_CTRL2                       ((0x0012 << 2))
#define ANACTRL_FIXPLL_CTRL3                       ((0x0013 << 2))
#define ANACTRL_FIXPLL_CTRL4                       ((0x0014 << 2))
#define ANACTRL_FIXPLL_CTRL5                       ((0x0015 << 2))
#define ANACTRL_FIXPLL_CTRL6                       ((0x0016 << 2))
#define ANACTRL_FIXPLL_STS                         ((0x0017 << 2))
#define ANACTRL_GP0PLL_CTRL0                       ((0x0020 << 2))
#define ANACTRL_GP0PLL_CTRL1                       ((0x0021 << 2))
#define ANACTRL_GP0PLL_CTRL2                       ((0x0022 << 2))
#define ANACTRL_GP0PLL_CTRL3                       ((0x0023 << 2))
#define ANACTRL_GP0PLL_CTRL4                       ((0x0024 << 2))
#define ANACTRL_GP0PLL_CTRL5                       ((0x0025 << 2))
#define ANACTRL_GP0PLL_CTRL6                       ((0x0026 << 2))
#define ANACTRL_GP0PLL_STS                         ((0x0027 << 2))
#define ANACTRL_GP1PLL_CTRL0                       ((0x0030 << 2))
#define ANACTRL_GP1PLL_CTRL1                       ((0x0031 << 2))
#define ANACTRL_GP1PLL_CTRL2                       ((0x0032 << 2))
#define ANACTRL_GP1PLL_CTRL3                       ((0x0033 << 2))
#define ANACTRL_GP1PLL_CTRL4                       ((0x0034 << 2))
#define ANACTRL_GP1PLL_CTRL5                       ((0x0035 << 2))
#define ANACTRL_GP1PLL_CTRL6                       ((0x0036 << 2))
#define ANACTRL_GP1PLL_STS                         ((0x0037 << 2))
#define ANACTRL_HIFIPLL_CTRL0                      ((0x0040 << 2))
#define ANACTRL_HIFIPLL_CTRL1                      ((0x0041 << 2))
#define ANACTRL_HIFIPLL_CTRL2                      ((0x0042 << 2))
#define ANACTRL_HIFIPLL_CTRL3                      ((0x0043 << 2))
#define ANACTRL_HIFIPLL_CTRL4                      ((0x0044 << 2))
#define ANACTRL_HIFIPLL_CTRL5                      ((0x0045 << 2))
#define ANACTRL_HIFIPLL_CTRL6                      ((0x0046 << 2))
#define ANACTRL_HIFIPLL_STS                        ((0x0047 << 2))
#define ANACTRL_PCIEPLL_CTRL0                      ((0x0050 << 2))
#define ANACTRL_PCIEPLL_CTRL1                      ((0x0051 << 2))
#define ANACTRL_PCIEPLL_CTRL2                      ((0x0052 << 2))
#define ANACTRL_PCIEPLL_CTRL3                      ((0x0053 << 2))
#define ANACTRL_PCIEPLL_CTRL4                      ((0x0054 << 2))
#define ANACTRL_PCIEPLL_CTRL5                      ((0x0055 << 2))
#define ANACTRL_PCIEPLL_STS                        ((0x0056 << 2))
#define ANACTRL_MPLL_CTRL0                         ((0x0060 << 2))
#define ANACTRL_MPLL_CTRL1                         ((0x0061 << 2))
#define ANACTRL_MPLL_CTRL2                         ((0x0062 << 2))
#define ANACTRL_MPLL_CTRL3                         ((0x0063 << 2))
#define ANACTRL_MPLL_CTRL4                         ((0x0064 << 2))
#define ANACTRL_MPLL_CTRL5                         ((0x0065 << 2))
#define ANACTRL_MPLL_CTRL6                         ((0x0066 << 2))
#define ANACTRL_MPLL_CTRL7                         ((0x0067 << 2))
#define ANACTRL_MPLL_CTRL8                         ((0x0068 << 2))
#define ANACTRL_MPLL_STS                           ((0x0069 << 2))
#define ANACTRL_HDMIPLL_CTRL0                      ((0x0070 << 2))
#define ANACTRL_HDMIPLL_CTRL1                      ((0x0071 << 2))
#define ANACTRL_HDMIPLL_CTRL2                      ((0x0072 << 2))
#define ANACTRL_HDMIPLL_CTRL3                      ((0x0073 << 2))
#define ANACTRL_HDMIPLL_CTRL4                      ((0x0074 << 2))
#define ANACTRL_HDMIPLL_CTRL5                      ((0x0075 << 2))
#define ANACTRL_HDMIPLL_CTRL6                      ((0x0076 << 2))
#define ANACTRL_HDMIPLL_STS                        ((0x0077 << 2))
#define ANACTRL_HDMIPLL_VLOCK                      ((0x0079 << 2))
#define ANACTRL_HDMIPHY_CTRL0                      ((0x0080 << 2))
#define ANACTRL_HDMIPHY_CTRL1                      ((0x0081 << 2))
#define ANACTRL_HDMIPHY_CTRL2                      ((0x0082 << 2))
#define ANACTRL_HDMIPHY_CTRL3                      ((0x0083 << 2))
#define ANACTRL_HDMIPHY_CTRL4                      ((0x0084 << 2))
#define ANACTRL_HDMIPHY_CTRL5                      ((0x0085 << 2))
#define ANACTRL_HDMIPHY_STS                        ((0x0086 << 2))
#define ANACTRL_MIPICSI_CTRL0                      ((0x0090 << 2))
#define ANACTRL_MIPICSI_CTRL1                      ((0x0091 << 2))
#define ANACTRL_MIPICSI_CTRL2                      ((0x0092 << 2))
#define ANACTRL_MIPICSI_CTRL3                      ((0x0093 << 2))
#define ANACTRL_MIPICSI_CTRL4                      ((0x0094 << 2))
#define ANACTRL_MIPICSI_CTRL5                      ((0x0095 << 2))
#define ANACTRL_MIPIDSI_CTRL0                      ((0x00a0 << 2))
#define ANACTRL_MIPIDSI_CTRL1                      ((0x00a1 << 2))
#define ANACTRL_MIPIDSI_CTRL2                      ((0x00a2 << 2))
#define ANACTRL_MIPIDSI_STS                        ((0x00a3 << 2))
#define ANACTRL_VDAC_CTRL0                         ((0x00b0 << 2))
#define ANACTRL_VDAC_CTRL1                         ((0x00b1 << 2))
#define ANACTRL_POR_CTRL                           ((0x00b6 << 2))
#define ANACTRL_LOCK_BIT                           ((0x00b8 << 2))
#define ANACTRL_PROT_BIT                           ((0x00b9 << 2))

/* CPU_CTRL
 * REG_BASE:  REGISTER_BASE_ADDR = 0xfe00e000
 */
#define CPUCTRL_SYS_CPU_RESET_CNTL                 ((0x0050 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL                   ((0x0051 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL1                  ((0x0052 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL2                  ((0x0053 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL3                  ((0x0054 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL4                  ((0x0055 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL5                  ((0x0056 << 2))
#define CPUCTRL_SYS_CPU_CLK_CTRL6                  ((0x0057 << 2))
#define CPUCTRL_SYS_CPU_CLK_RESULT                 ((0x0058 << 2))

#if 0
/*SC2: pll: FDCO: 3G~6G FDCO = 24*(M+frac)/N
 *N: recommend is 1
 *clk_out = FDCO >> OD
 */
static const struct pll_rate_table sc2_pll_rate_table[] = {
	PLL_RATE(24000000ULL,  128, 1, 7), /*DCO=3072M*/
	PLL_RATE(48000000ULL,  128, 1, 6), /*DCO=3072M*/
	PLL_RATE(96000000ULL,  128, 1, 5), /*DCO=3072M*/
	PLL_RATE(192000000ULL,  128, 1, 4), /*DCO=3072M*/
	PLL_RATE(312000000ULL,  208, 1, 4), /*DCO=4992M*/
	PLL_RATE(408000000ULL,  136, 1, 3), /*DCO=3264M*/
	PLL_RATE(600000000ULL,  200, 1, 3), /*DCO=4800M*/
	PLL_RATE(696000000ULL,  232, 1, 3), /*DCO=5568M*/
	PLL_RATE(792000000ULL,  132, 1, 2), /*DCO=3168M*/
	PLL_RATE(846000000ULL,  141, 1, 2), /*DCO=3384M*/
	PLL_RATE(912000000ULL,  152, 1, 2), /*DCO=3648M*/
	PLL_RATE(1008000000ULL, 168, 1, 2), /*DCO=4032M*/
	PLL_RATE(1104000000ULL, 184, 1, 2), /*DCO=4416M*/
	PLL_RATE(1200000000ULL, 200, 1, 2), /*DCO=4800M*/
	PLL_RATE(1296000000ULL, 216, 1, 2), /*DCO=5184M*/
	PLL_RATE(1302000000ULL, 217, 1, 2), /*DCO=5208M*/
	PLL_RATE(1398000000ULL, 233, 1, 2), /*DCO=5592M*/
	PLL_RATE(1404000000ULL, 234, 1, 2), /*DCO=5616M*/
	PLL_RATE(1494000000ULL, 249, 1, 2), /*DCO=5976M*/
	PLL_RATE(1500000000ULL, 125, 1, 1), /*DCO=3000M*/
	PLL_RATE(1512000000ULL, 126, 1, 1), /*DCO=3024M*/
	PLL_RATE(1608000000ULL, 134, 1, 1), /*DCO=3216M*/
	PLL_RATE(1704000000ULL, 142, 1, 1), /*DCO=3408M*/
	PLL_RATE(1800000000ULL, 150, 1, 1), /*DCO=3600M*/
	PLL_RATE(1896000000ULL, 158, 1, 1), /*DCO=3792M*/
	PLL_RATE(1908000000ULL, 159, 1, 1), /*DCO=3816M*/
	PLL_RATE(1920000000ULL, 160, 1, 1), /*DCO=3840M*/
	PLL_RATE(2004000000ULL, 167, 1, 1), /*DCO=4008M*/
	PLL_RATE(2016000000ULL, 168, 1, 1), /*DCO=4032M*/
	PLL_RATE(2100000000ULL, 175, 1, 1), /*DCO=4200M*/
	PLL_RATE(2196000000ULL, 183, 1, 1), /*DCO=4392M*/
	PLL_RATE(2208000000ULL, 184, 1, 1), /*DCO=4416M*/
	PLL_RATE(2292000000ULL, 191, 1, 1), /*DCO=4584M*/
	PLL_RATE(2304000000ULL, 192, 1, 1), /*DCO=4608M*/
	PLL_RATE(2400000000ULL, 200, 1, 1), /*DCO=4800M*/
	PLL_RATE(2496000000ULL, 208, 1, 1), /*DCO=4992M*/
	PLL_RATE(2592000000ULL, 216, 1, 1), /*DCO=5184M*/
	PLL_RATE(2700000000ULL, 225, 1, 1), /*DCO=5400M*/
	PLL_RATE(2796000000ULL, 233, 1, 1), /*DCO=5592M*/
	PLL_RATE(2892000000ULL, 241, 1, 1), /*DCO=5784M*/
	PLL_RATE(3000000000ULL, 125, 1, 0), /*DCO=3000M*/
	PLL_RATE(3096000000ULL, 129, 1, 0), /*DCO=3096M*/
	PLL_RATE(3192000000ULL, 133, 1, 0), /*DCO=3192M*/
	PLL_RATE(3288000000ULL, 137, 1, 0), /*DCO=3288M*/
	PLL_RATE(3408000000ULL, 142, 1, 0), /*DCO=3408M*/
	PLL_RATE(3504000000ULL, 146, 1, 0), /*DCO=3504M*/
	PLL_RATE(3600000000ULL, 150, 1, 0), /*DCO=3600M*/
	PLL_RATE(3696000000ULL, 154, 1, 0), /*DCO=3696M*/
	PLL_RATE(3792000000ULL, 158, 1, 0), /*DCO=3792M*/
	PLL_RATE(3888000000ULL, 162, 1, 0), /*DCO=3888M*/
	PLL_RATE(4008000000ULL, 167, 1, 0), /*DCO=4008M*/
	PLL_RATE(4104000000ULL, 171, 1, 0), /*DCO=4104M*/
	PLL_RATE(4200000000ULL, 175, 1, 0), /*DCO=4200M*/
	PLL_RATE(4296000000ULL, 179, 1, 0), /*DCO=4296M*/
	PLL_RATE(4392000000ULL, 183, 1, 0), /*DCO=4392M*/
	PLL_RATE(4488000000ULL, 187, 1, 0), /*DCO=4488M*/
	PLL_RATE(4608000000ULL, 192, 1, 0), /*DCO=4608M*/
	PLL_RATE(4704000000ULL, 196, 1, 0), /*DCO=4704M*/
	PLL_RATE(4800000000ULL, 200, 1, 0), /*DCO=4800M*/
	PLL_RATE(4896000000ULL, 204, 1, 0), /*DCO=4896M*/
	PLL_RATE(4992000000ULL, 208, 1, 0), /*DCO=4992M*/
	PLL_RATE(5088000000ULL, 212, 1, 0), /*DCO=5088M*/
	PLL_RATE(5208000000ULL, 217, 1, 0), /*DCO=5208M*/
	PLL_RATE(5304000000ULL, 221, 1, 0), /*DCO=5304M*/
	PLL_RATE(5400000000ULL, 225, 1, 0), /*DCO=5400M*/
	PLL_RATE(5496000000ULL, 229, 1, 0), /*DCO=5496M*/
	PLL_RATE(5592000000ULL, 233, 1, 0), /*DCO=5592M*/
	PLL_RATE(5688000000ULL, 237, 1, 0), /*DCO=5688M*/
	PLL_RATE(5808000000ULL, 242, 1, 0), /*DCO=5808M*/
	PLL_RATE(5904000000ULL, 246, 1, 0), /*DCO=5904M*/
	PLL_RATE(6000000000ULL, 250, 1, 0), /*DCO=6000M*/

	{ /* sentinel */ }
};

/*fix pll rate table*/
static const struct fclk_rate_table fclk_pll_rate_table[] = {
	FCLK_PLL_RATE(50000000, 1, 1, 19),
	FCLK_PLL_RATE(100000000, 1, 1, 9),
	FCLK_PLL_RATE(167000000, 2, 1, 3),
	FCLK_PLL_RATE(200000000, 1, 1, 4),
	FCLK_PLL_RATE(250000000, 1, 1, 3),
	FCLK_PLL_RATE(333000000, 2, 1, 1),
	FCLK_PLL_RATE(500000000, 1, 1, 1),
	FCLK_PLL_RATE(667000000, 2, 0, 0),
	FCLK_PLL_RATE(1000000000, 1, 0, 0)
};

/*PCIE clk_out = 24M*m/2/2/OD*/
static const struct pll_rate_table sc2_pcie_pll_rate_table[] = {
	PLL_RATE(100000000, 150, 0, 9),
	{ /* sentinel */ }
};

static const struct pll_rate_table sc2_hifi_pll_rate_table[] = {
	PLL_RATE(666000000ULL,	222, 1, 3), /*DCO=5328M*/
	PLL_FRAC_RATE(1806336000ULL, 150, 1, 1, 0, 0x00010E56),
	{ /* sentinel */ }
};

/* include the CLKIDs that have been made part of the DT binding */
#endif

#endif /* __SC2_H */
