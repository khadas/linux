/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Header providing constants for Rockchip suspend bindings.
 *
 * Copyright (C) 2022, Rockchip Electronics Co., Ltd.
 * Author: XiaoDong.Huang
 */

#ifndef __DT_BINDINGS_RK3588_PM_H__
#define __DT_BINDINGS_RK3588_PM_H__
/******************************bits ops************************************/

#ifndef BIT
#define BIT(nr)				(1 << (nr))
#endif

#define RKPM_SLP_ARMPD			BIT(0)
#define RKPM_SLP_ARMOFF			BIT(1)
#define RKPM_SLP_ARMOFF_DDRPD		BIT(2)
#define RKPM_SLP_ARMOFF_LOGOFF		BIT(3)
#define RKPM_SLP_ARMOFF_PMUOFF		BIT(4)

/* all plls except ddr's pll*/
#define RKPM_SLP_PMU_HW_PLLS_PD		BIT(8)
#define RKPM_SLP_PMU_PMUALIVE_32K	BIT(9)
#define RKPM_SLP_PMU_DIS_OSC		BIT(10)

#define RKPM_SLP_CLK_GT			BIT(16)
#define RKPM_SLP_PMIC_LP		BIT(17)

#define RKPM_SLP_32K_EXT		BIT(24)
#define RKPM_SLP_TIME_OUT_WKUP		BIT(25)
#define RKPM_SLP_PMU_DBG		BIT(26)
#define RKPM_SLP_ARCH_TIMER_RESET	BIT(27)

/* the wake up source */
#define RKPM_CPU0_WKUP_EN		BIT(0)
#define RKPM_CPU1_WKUP_EN		BIT(1)
#define RKPM_CPU2_WKUP_EN		BIT(2)
#define RKPM_CPU3_WKUP_EN		BIT(3)
#define RKPM_CPU4_WKUP_EN		BIT(4)
#define RKPM_CPU5_WKUP_EN		BIT(5)
#define RKPM_CPU6_WKUP_EN		BIT(6)
#define RKPM_CPU7_WKUP_EN		BIT(7)
#define RKPM_GPIO_WKUP_EN		BIT(8)
#define RKPM_SDMMC_WKUP_EN		BIT(9)
#define RKPM_SDIO_WKUP_EN		BIT(10)
#define RKPM_USB_WKUP_EN		BIT(11)
#define RKPM_UART0_WKUP_EN		BIT(12)
#define RKPM_VAD_WKUP_EN		BIT(13)
#define RKPM_TIMER_WKUP_EN		BIT(14)
#define RKPM_SYSINT_WKUP_EN		BIT(15)
#define RKPM_TIME_OUT_WKUP_EN		BIT(16)
#define RKPM_PMUMCU_CEC_WKUP_EN		BIT(20)
#define RKPM_PMUMCU_VAD_WKUP_EN		BIT(21)

/* io retention config */
#define RKPM_EMMCIO_RET_EN		BIT(0)
#define RKPM_VCCIO1_RET_EN		BIT(1)
#define RKPM_VCCIO2_RET_EN		BIT(2)
#define RKPM_VCCIO3_RET_EN		BIT(3)
#define RKPM_VCCIO4_RET_EN		BIT(4)
#define RKPM_VCCIO5_RET_EN		BIT(5)
#define RKPM_VCCIO6_RET_EN		BIT(6)
#define RKPM_PMUIO2_RET_EN		BIT(7)

/* sleep pin */
#define RKPM_SLEEP_PIN0_ACT_LOW		BIT(0)	/* GPIO0_A2 */
#define RKPM_SLEEP_PIN1_ACT_LOW		BIT(1)	/* GPIO0_A3 */

#define RKPM_SLEEP_PIN_SRC_VD_NPU	0x1
#define RKPM_SLEEP_PIN_SRC_VD_GPU	0x2
#define RKPM_SLEEP_PIN_SRC_VD_BIGCORE0	0x3
#define RKPM_SLEEP_PIN_SRC_VD_BIGCORE1	0x4
#define RKPM_SLEEP_PIN_SRC_VD_DSU	0x5
#define RKPM_SLEEP_PIN_SRC_VD_VCODEC	0x6
#define RKPM_SLEEP_PIN_SRC_VD_DDR	0x7
#define RKPM_SLEEP_PIN_SRC_LP_DEEP_LP	0x8
#define RKPM_SLEEP_PIN_SRC_SFT_EN	0xf

#define RKPM_SLEEP_PIN0_SRC(n)		(((n) & 0xf) << 0)
#define RKPM_SLEEP_PIN1_SRC(n)		(((n) & 0xf) << 4)
#define RKPM_SLEEP_PIN2_SRC(n)		(((n) & 0xf) << 8)
#define RKPM_SLEEP_PIN3_SRC(n)		(((n) & 0xf) << 12)
#define RKPM_SLEEP_PIN4_SRC(n)		(((n) & 0xf) << 16)
#define RKPM_SLEEP_PIN5_SRC(n)		(((n) & 0xf) << 20)

#endif
