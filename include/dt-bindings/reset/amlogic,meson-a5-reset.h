/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_AMLOGIC_MESON_T5_RESET_H
#define _DT_BINDINGS_AMLOGIC_MESON_T5_RESET_H

/* RESET0 */
/*						0-3 */
#define RESET_USBCTRL				4
/*						5-7 */
#define RESET_USBPHY20				8
/*						9 */
#define RESET_USB2DRD				10
/*						11-31 */

/* RESET1 */
#define RESET_AUDIO				32
#define RESET_AUDIO_VAD				33
/*                                              34 */
#define RESET_DDRAPB				35
#define RESET_DDR				36
/*						37-40 */
#define RESET_DSPA_DEBUG			41
/*                                              42 */
#define RESET_DSPA				43
/*						44-46 */
#define RESET_NNA				47
#define RESET_ETHERNET				48
/*						49-63 */

/* RESET2 */
#define RESET_ABUS_ARB				64
#define RESET_IRCTRL				65
/*						66 */
#define RESET_TEMP_PII				67
/*						68-72 */
#define RESET_SPICC_0				73
#define RESET_SPICC_1				74
#define RESET_RSA				75

/*						76-79 */
#define RESET_MSR_CLK				80
#define RESET_SPIFC				81
#define RESET_SAR_ADC				82
/*						83-90 */
#define RESET_WATCHDOG				91
/*						92-95 */

/* RESET4 */
#define RESET_RTC				96
/*						97-99 */
#define RESET_PWM_AB				100
#define RESET_PWM_CD				101
#define RESET_PWM_EF				102
#define RESET_PWM_GH				103
/*						104-105 */
#define RESET_UART_A				106
#define RESET_UART_B				107
#define RESET_UART_C				108
#define RESET_UART_D				109
#define RESET_UART_E				110
/*						111*/
#define RESET_I2C_S_A				112
#define RESET_I2C_M_A				113
#define RESET_I2C_M_B				114
#define RESET_I2C_M_C				115
#define RESET_I2C_M_D				116
/*						117-119 */
#define RESET_SDEMMC_A				120
/*						121 */
#define RESET_SDEMMC_C				122

/* RESET5 */
/*						128-143 */
#define RESET_BRG_AO_NIC_SYS			144
#define RESET_BRG_AO_NIC_DSPA			145
#define RESET_BRG_AO_NIC_MAIN			146
#define RESET_BRG_AO_NIC_AUDIO			147
/*						148-151 */
#define RESET_BRG_AO_NIC_ALL			152
#define RESET_BRG_NIC_NNA			153
#define RESET_BRG_NIC_SDIO			154
#define RESET_BRG_NIC_EMMC			155
#define RESET_BRG_NIC_DSU			156
#define RESET_BRG_NIC_SYSCLK			157
#define RESET_BRG_NIC_MAIN			158
#define RESET_BRG_NIC_ALL			159

#endif
