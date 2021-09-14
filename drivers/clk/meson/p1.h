/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __P1_H
#define __P1_H

/* base reg: 0xfe000000 */
#define CLKCTRL_OSCIN_CTRL                         (0x0001  << 2)
#define CLKCTRL_RTC_BY_OSCIN_CTRL0                 (0x0002  << 2)
#define CLKCTRL_RTC_BY_OSCIN_CTRL1                 (0x0003  << 2)
#define CLKCTRL_RTC_CTRL                           (0x0004  << 2)
#define CLKCTRL_CHECK_CLK_RESULT                   (0x0005  << 2)
#define CLKCTRL_MBIST_ATSPEED_CTRL                 (0x0006  << 2)
#define CLKCTRL_LOCK_BIT_REG0                      (0x0008  << 2)
#define CLKCTRL_LOCK_BIT_REG1                      (0x0009  << 2)
#define CLKCTRL_LOCK_BIT_REG2                      (0x000a  << 2)
#define CLKCTRL_LOCK_BIT_REG3                      (0x000b  << 2)
#define CLKCTRL_PROT_BIT_REG0                      (0x000c  << 2)
#define CLKCTRL_PROT_BIT_REG1                      (0x000d  << 2)
#define CLKCTRL_PROT_BIT_REG2                      (0x000e  << 2)
#define CLKCTRL_PROT_BIT_REG3                      (0x000f  << 2)
#define CLKCTRL_SYS_CLK_CTRL0                      (0x0010  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG0                   (0x0011  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG1                   (0x0012  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG2                   (0x0013  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG3                   (0x0014  << 2)
#define CLKCTRL_SYS_CLK_EN1_REG0                   (0x0015  << 2)
#define CLKCTRL_SYS_CLK_EN1_REG1                   (0x0016  << 2)
#define CLKCTRL_SYS_CLK_EN1_REG2                   (0x0017  << 2)
#define CLKCTRL_SYS_CLK_EN1_REG3                   (0x0018  << 2)
#define CLKCTRL_AXI_CLK_CTRL0                      (0x001b  << 2)
#define CLKCTRL_TST_CTRL0                          (0x0020  << 2)
#define CLKCTRL_TST_CTRL1                          (0x0021  << 2)
#define CLKCTRL_DSPA_CLK_CTRL0                     (0x0027  << 2)
#define CLKCTRL_DSPB_CLK_CTRL0                     (0x0028  << 2)
#define CLKCTRL_CLK12_24_CTRL                      (0x002a  << 2)
#define CLKCTRL_DEWARPA_CLK_CTRL                   (0x002d  << 2)
#define CLKCTRL_DEWARPB_CLK_CTRL                   (0x002e  << 2)
#define CLKCTRL_DEWARPC_CLK_CTRL                   (0x002f  << 2)
#define CLKCTRL_VAPBCLK_CTRL                       (0x003f  << 2)
#define CLKCTRL_GE2DCLK_CTRL                       (0x0043  << 2)
#define CLKCTRL_M4_CLK_CTRL                        (0x0044  << 2)
#define CLKCTRL_TS_CLK_CTRL                        (0x0056  << 2)
#define CLKCTRL_USB_CLK_CTRL                       (0x0057  << 2)
#define CLKCTRL_USB_CLK_CTRL1                      (0x0058  << 2)
#define CLKCTRL_ETH_CLK_CTRL                       (0x0059  << 2)
#define CLKCTRL_NAND_CLK_CTRL                      (0x005a  << 2)
#define CLKCTRL_SD_EMMC_CLK_CTRL                   (0x005b  << 2)
#define CLKCTRL_SPICC_CLK_CTRL                     (0x005d  << 2)
#define CLKCTRL_GEN_CLK_CTRL                       (0x005e  << 2)
#define CLKCTRL_SAR_CLK_CTRL0                      (0x005f  << 2)
#define CLKCTRL_PWM_CLK_AB_CTRL                    (0x0060  << 2)
#define CLKCTRL_PWM_CLK_CD_CTRL                    (0x0061  << 2)
#define CLKCTRL_PWM_CLK_EF_CTRL                    (0x0062  << 2)
#define CLKCTRL_PWM_CLK_GH_CTRL                    (0x0063  << 2)
#define CLKCTRL_PWM_CLK_IJ_CTRL                    (0x0064  << 2)
#define CLKCTRL_SPICC_CLK_CTRL1                    (0x0070  << 2)
#define CLKCTRL_SPICC_CLK_CTRL2                    (0x0071  << 2)
#define CLKCTRL_NNA_CLK_CTRL0                      (0x0082  << 2)
#define CLKCTRL_NNA_CLK_CTRL1                      (0x0083  << 2)
#define CLKCTRL_NNA_CLK_CTRL2                      (0x0084  << 2)
#define CLKCTRL_ISP0_CLK_CTRL                      (0x0090  << 2)
#define CLKCTRL_ISP1_CLK_CTRL                      (0x0091  << 2)
#define CLKCTRL_ISP2_CLK_CTRL                      (0x0092  << 2)
#define CLKCTRL_ISP3_CLK_CTRL                      (0x0093  << 2)
#define CLKCTRL_ISP4_CLK_CTRL                      (0x0094  << 2)
#define CLKCTRL_VFECLK_CTRL                        (0x00a0  << 2)
#define CLKCTRL_MOPCLK_CTRL                        (0x00a1  << 2)
#define CLKCTRL_DEPCLK_CTRL                        (0x00a2  << 2)
#define CLKCTRL_GLBCLK_CTRL                        (0x00a3  << 2)
#define CLKCTRL_TIMESTAMP_CTRL                     (0x0100  << 2)
#define CLKCTRL_TIMESTAMP_CTRL1                    (0x0101  << 2)
#define CLKCTRL_TIMESTAMP_CTRL2                    (0x0103  << 2)
#define CLKCTRL_TIMESTAMP_RD0                      (0x0104  << 2)
#define CLKCTRL_TIMESTAMP_RD1                      (0x0105  << 2)
#define CLKCTRL_TIMEBASE_CTRL0                     (0x0106  << 2)
#define CLKCTRL_TIMEBASE_CTRL1                     (0x0107  << 2)
#define CLKCTRL_EFUSE_CPU_CFG01                    (0x0120  << 2)
#define CLKCTRL_EFUSE_CPU_CFG2                     (0x0121  << 2)
#define CLKCTRL_EFUSE_LOCK                         (0x0126  << 2)
#define CLKCTRL_EFUSE_A76_CFG01                    (0x0127  << 2)
#define CLKCTRL_EFUSE_A76_CFG2                     (0x0128  << 2)
#define CLKCTRL_SYS0PLL_CTRL0                      (0x0200  << 2)
#define CLKCTRL_SYS0PLL_CTRL1                      (0x0201  << 2)
#define CLKCTRL_SYS0PLL_CTRL2                      (0x0202  << 2)
#define CLKCTRL_SYS0PLL_CTRL3                      (0x0203  << 2)
#define CLKCTRL_SYS0PLL_STS                        (0x0204  << 2)
#define CLKCTRL_SYS1PLL_CTRL0                      (0x0210  << 2)
#define CLKCTRL_SYS1PLL_CTRL1                      (0x0211  << 2)
#define CLKCTRL_SYS1PLL_CTRL2                      (0x0212  << 2)
#define CLKCTRL_SYS1PLL_CTRL3                      (0x0213  << 2)
#define CLKCTRL_SYS1PLL_STS                        (0x0214  << 2)
#define CLKCTRL_FIXPLL_CTRL0                       (0x0220  << 2)
#define CLKCTRL_FIXPLL_CTRL1                       (0x0221  << 2)
#define CLKCTRL_FIXPLL_CTRL2                       (0x0222  << 2)
#define CLKCTRL_FIXPLL_CTRL3                       (0x0223  << 2)
#define CLKCTRL_FIXPLL_CTRL4                       (0x0224  << 2)
#define CLKCTRL_FIXPLL_CTRL5                       (0x0225  << 2)
#define CLKCTRL_FIXPLL_CTRL6                       (0x0226  << 2)
#define CLKCTRL_FIXPLL_STS                         (0x0227  << 2)
#define CLKCTRL_MPLL_CTRL0                         (0x0230  << 2)
#define CLKCTRL_MPLL_CTRL1                         (0x0231  << 2)
#define CLKCTRL_MPLL_CTRL2                         (0x0232  << 2)
#define CLKCTRL_MPLL_CTRL3                         (0x0233  << 2)
#define CLKCTRL_MPLL_CTRL4                         (0x0234  << 2)
#define CLKCTRL_MPLL_CTRL5                         (0x0235  << 2)
#define CLKCTRL_MPLL_CTRL6                         (0x0236  << 2)
#define CLKCTRL_MPLL_CTRL7                         (0x0237  << 2)
#define CLKCTRL_MPLL_CTRL8                         (0x0238  << 2)
#define CLKCTRL_MPLL_STS                           (0x0239  << 2)
#define CLKCTRL_GP0PLL_CTRL0                       (0x0240  << 2)
#define CLKCTRL_GP0PLL_CTRL1                       (0x0241  << 2)
#define CLKCTRL_GP0PLL_CTRL2                       (0x0242  << 2)
#define CLKCTRL_GP0PLL_CTRL3                       (0x0243  << 2)
#define CLKCTRL_GP0PLL_CTRL4                       (0x0244  << 2)
#define CLKCTRL_GP0PLL_CTRL5                       (0x0245  << 2)
#define CLKCTRL_GP0PLL_CTRL6                       (0x0246  << 2)
#define CLKCTRL_GP0PLL_STS                         (0x0247  << 2)
#define CLKCTRL_GP1PLL_CTRL0                       (0x0280  << 2)
#define CLKCTRL_GP1PLL_CTRL1                       (0x0281  << 2)
#define CLKCTRL_GP1PLL_CTRL2                       (0x0282  << 2)
#define CLKCTRL_GP1PLL_CTRL3                       (0x0283  << 2)
#define CLKCTRL_GP1PLL_STS                         (0x0284  << 2)
#define CLKCTRL_GP2PLL_CTRL0                       (0x0290  << 2)
#define CLKCTRL_GP2PLL_CTRL1                       (0x0291  << 2)
#define CLKCTRL_GP2PLL_CTRL2                       (0x0292  << 2)
#define CLKCTRL_GP2PLL_CTRL3                       (0x0293  << 2)
#define CLKCTRL_GP2PLL_STS                         (0x0294  << 2)
#define CLKCTRL_HIFIPLL_CTRL0                      (0x02a0  << 2)
#define CLKCTRL_HIFIPLL_CTRL1                      (0x02a1  << 2)
#define CLKCTRL_HIFIPLL_CTRL2                      (0x02a2  << 2)
#define CLKCTRL_HIFIPLL_CTRL3                      (0x02a3  << 2)
#define CLKCTRL_HIFIPLL_CTRL4                      (0x02a4  << 2)
#define CLKCTRL_HIFIPLL_CTRL5                      (0x02a5  << 2)
#define CLKCTRL_HIFIPLL_CTRL6                      (0x02a6  << 2)
#define CLKCTRL_HIFIPLL_STS                        (0x02a7  << 2)
#define CLKCTRL_M4PLL_CTRL0                        (0x02b0  << 2)
#define CLKCTRL_M4PLL_CTRL1                        (0x02b1  << 2)
#define CLKCTRL_M4PLL_CTRL2                        (0x02b2  << 2)
#define CLKCTRL_M4PLL_CTRL3                        (0x02b3  << 2)
#define CLKCTRL_M4PLL_STS                          (0x02b4  << 2)
#define CLKCTRL_FDLEPLL_CTRL0                      (0x02c0  << 2)
#define CLKCTRL_FDLEPLL_CTRL1                      (0x02c1  << 2)
#define CLKCTRL_FDLEPLL_CTRL2                      (0x02c2  << 2)
#define CLKCTRL_FDLEPLL_CTRL3                      (0x02c3  << 2)
#define CLKCTRL_FDLEPLL_STS                        (0x02c4  << 2)
#define CLKCTRL_NNAPLL_CTRL0                       (0x02d0  << 2)
#define CLKCTRL_NNAPLL_CTRL1                       (0x02d1  << 2)
#define CLKCTRL_NNAPLL_CTRL2                       (0x02d2  << 2)
#define CLKCTRL_NNAPLL_CTRL3                       (0x02d3  << 2)
#define CLKCTRL_NNAPLL_STS                         (0x02d4  << 2)

/* base reg: 0xfe008000 */
#define CLKCTRL_PCIEPLL_CTRL0                      (0x0050  << 2)
#define CLKCTRL_PCIEPLL_CTRL1                      (0x0051  << 2)
#define CLKCTRL_PCIEPLL_CTRL2                      (0x0052  << 2)
#define CLKCTRL_PCIEPLL_CTRL3                      (0x0053  << 2)
#define CLKCTRL_PCIEPLL_CTRL4                      (0x0054  << 2)
#define CLKCTRL_PCIEPLL_CTRL5                      (0x0055  << 2)
#define CLKCTRL_PCIEPLL_STS                        (0x0056  << 2)
#define ANACTRL_MCLK_PLL_CNTL0                     (0x00c0  << 2)
#define ANACTRL_MCLK_PLL_CNTL1                     (0x00c1  << 2)
#define ANACTRL_MCLK_PLL_CNTL2                     (0x00c2  << 2)
#define ANACTRL_MCLK_PLL_CNTL3                     (0x00c3  << 2)
#define ANACTRL_MCLK_PLL_CNTL4                     (0x00c4  << 2)
#define ANACTRL_MCLK_PLL_CNTL5                     (0x00c5  << 2)
#define ANACTRL_MCLK_PLL_CNTL6                     (0x00c6  << 2)
#define ANACTRL_MCLK_PLL_CNTL7                     (0x00c7  << 2)
#define ANACTRL_MCLK_PLL_CNTL8                     (0x00c8  << 2)
#define ANACTRL_MCLK_PLL_CNTL9                     (0x00c9  << 2)
#define ANACTRL_MCLK_PLL_CNTL10                    (0x00ca  << 2)
#define ANACTRL_MCLK_PLL_CNTL11                    (0x00cb  << 2)
#define ANACTRL_MCLK_PLL_CNTL12                    (0x00cc  << 2)
#define ANACTRL_MCLK_PLL_CNTL13                    (0x00cd  << 2)
#define ANACTRL_MCLK_PLL_STS                       (0x00d2  << 2)

#define SECURE_PLL_CLK			0x82000098
#define SECURE_CPU_CLK			0x82000099

/* PLL secure clock index */
enum sec_pll {
	SECID_SYS0_DCO_PLL = 0,
	SECID_SYS0_DCO_PLL_DIS,
	SECID_SYS0_PLL_OD,
	SECID_SYS1_DCO_PLL,
	SECID_SYS1_DCO_PLL_DIS,
	SECID_SYS1_PLL_OD,
	SECID_GP1_DCO_PLL,
	SECID_GP1_DCO_PLL_DIS,
	SECID_GP1_PLL_OD,
	SECID_CPU_CLK_SEL,
	SECID_CPU_CLK_RD,
	SECID_CPU_CLK_DYN,
	SECID_A76_CLK_SEL,
	SECID_A76_CLK_RD,
	SECID_A76_CLK_DYN,
	SECID_DSU_CLK_SEL,
	SECID_DSU_CLK_RD,
	SECID_DSU_CLK_DYN,
	SECID_DSU_FINAL_CLK_SEL,
	SECID_DSU_FINAL_CLK_RD,
	SECID_CPU6_CLK_SEL,
	SECID_CPU6_CLK_RD,
	SECID_CPU7_CLK_SEL,
	SECID_CPU7_CLK_RD
};

#endif /* __P1_H */
