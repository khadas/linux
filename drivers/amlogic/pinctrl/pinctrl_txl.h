/*
 * drivers/amlogic/pinctrl/pinctrl_txl.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef __AMLOGIC_PINCTRL_TXL_H
#define __AMLOGIC_PINCTRL_TXL_H

#include <dt-bindings/gpio/txl.h>

#define PIN_AO_BASE		(0)
#define PIN_AO_SIZE		(12)
#define PIN_EE_BASE		(PIN_AO_BASE + PIN_AO_SIZE)

#define PIN_AO(g)		(PIN_AO_BASE + g)
#define PIN_EE(g)		(PIN_EE_BASE + g)

/* pin gpiozo */
#define	PIN_GPIOAO_BASE		(PIN_AO_BASE)
#define	PIN_GPIOAO_SIZE		(12)
#define	PIN_GPIOAO_0		PIN_AO(GPIOAO_0)
#define	PIN_GPIOAO_1		PIN_AO(GPIOAO_1)
#define	PIN_GPIOAO_2		PIN_AO(GPIOAO_2)
#define	PIN_GPIOAO_3		PIN_AO(GPIOAO_3)
#define	PIN_GPIOAO_4		PIN_AO(GPIOAO_4)
#define	PIN_GPIOAO_5		PIN_AO(GPIOAO_5)
#define	PIN_GPIOAO_6		PIN_AO(GPIOAO_6)
#define	PIN_GPIOAO_7		PIN_AO(GPIOAO_7)
#define	PIN_GPIOAO_8		PIN_AO(GPIOAO_8)
#define	PIN_GPIOAO_9		PIN_AO(GPIOAO_9)
#define	PIN_GPIOAO_10		PIN_AO(GPIOAO_10)
#define	PIN_GPIOAO_11		PIN_AO(GPIOAO_11)

/* pin gpioz */
/* pin gpioz based on pin gpioao, but gpioz based on zero */
#define	PIN_GPIOZ_BASE		(PIN_GPIOAO_BASE + PIN_GPIOAO_SIZE)
#define	PIN_GPIOZ_SIZE		(22)
#define	PIN_GPIOZ_0		PIN_EE(GPIOZ_0)
#define	PIN_GPIOZ_1		PIN_EE(GPIOZ_1)
#define	PIN_GPIOZ_2		PIN_EE(GPIOZ_2)
#define	PIN_GPIOZ_3		PIN_EE(GPIOZ_3)
#define	PIN_GPIOZ_4		PIN_EE(GPIOZ_4)
#define	PIN_GPIOZ_5		PIN_EE(GPIOZ_5)
#define	PIN_GPIOZ_6		PIN_EE(GPIOZ_6)
#define	PIN_GPIOZ_7		PIN_EE(GPIOZ_7)
#define	PIN_GPIOZ_8		PIN_EE(GPIOZ_8)
#define	PIN_GPIOZ_9		PIN_EE(GPIOZ_9)
#define	PIN_GPIOZ_10		PIN_EE(GPIOZ_10)
#define	PIN_GPIOZ_11		PIN_EE(GPIOZ_11)
#define	PIN_GPIOZ_12		PIN_EE(GPIOZ_12)
#define	PIN_GPIOZ_13		PIN_EE(GPIOZ_13)
#define	PIN_GPIOZ_14		PIN_EE(GPIOZ_14)
#define	PIN_GPIOZ_15		PIN_EE(GPIOZ_15)
#define	PIN_GPIOZ_16		PIN_EE(GPIOZ_16)
#define	PIN_GPIOZ_17		PIN_EE(GPIOZ_17)
#define	PIN_GPIOZ_18		PIN_EE(GPIOZ_18)
#define	PIN_GPIOZ_19		PIN_EE(GPIOZ_19)
#define	PIN_GPIOZ_20		PIN_EE(GPIOZ_20)
#define	PIN_GPIOZ_21		PIN_EE(GPIOZ_21)

/* pin gpioh */
#define	PIN_GPIOH_BASE		(PIN_GPIOZ_BASE + PIN_GPIOZ_SIZE)
#define	PIN_GPIOH_SIZE		(10)
#define	PIN_GPIOH_0		PIN_EE(GPIOH_0)
#define	PIN_GPIOH_1		PIN_EE(GPIOH_1)
#define	PIN_GPIOH_2		PIN_EE(GPIOH_2)
#define	PIN_GPIOH_3		PIN_EE(GPIOH_3)
#define	PIN_GPIOH_4		PIN_EE(GPIOH_4)
#define	PIN_GPIOH_5		PIN_EE(GPIOH_5)
#define	PIN_GPIOH_6		PIN_EE(GPIOH_6)
#define	PIN_GPIOH_7		PIN_EE(GPIOH_7)
#define	PIN_GPIOH_8		PIN_EE(GPIOH_8)
#define	PIN_GPIOH_9		PIN_EE(GPIOH_9)

/* pin boot */
#define	PIN_BOOT_BASE		(PIN_GPIOH_BASE + PIN_GPIOH_SIZE)
#define	PIN_BOOT_SIZE		(12)
#define	PIN_BOOT_0		PIN_EE(BOOT_0)
#define	PIN_BOOT_1		PIN_EE(BOOT_1)
#define	PIN_BOOT_2		PIN_EE(BOOT_2)
#define	PIN_BOOT_3		PIN_EE(BOOT_3)
#define	PIN_BOOT_4		PIN_EE(BOOT_4)
#define	PIN_BOOT_5		PIN_EE(BOOT_5)
#define	PIN_BOOT_6		PIN_EE(BOOT_6)
#define	PIN_BOOT_7		PIN_EE(BOOT_7)
#define	PIN_BOOT_8		PIN_EE(BOOT_8)
#define	PIN_BOOT_9		PIN_EE(BOOT_9)
#define	PIN_BOOT_10		PIN_EE(BOOT_10)
#define	PIN_BOOT_11		PIN_EE(BOOT_11)

/* pin card */
#define PIN_CARD_BASE		(PIN_BOOT_BASE + PIN_BOOT_SIZE)
#define PIN_CARD_SIZE		(7)
#define	PIN_CARD_0		PIN_EE(CARD_0)
#define	PIN_CARD_1		PIN_EE(CARD_1)
#define	PIN_CARD_2		PIN_EE(CARD_2)
#define	PIN_CARD_3		PIN_EE(CARD_3)
#define	PIN_CARD_4		PIN_EE(CARD_4)
#define	PIN_CARD_5		PIN_EE(CARD_5)
#define	PIN_CARD_6		PIN_EE(CARD_6)

/* pin gpiow */
#define PIN_GPIODV_BASE		(PIN_CARD_BASE + PIN_CARD_SIZE)
#define PIN_GPIODV_SIZE		(12)
#define	PIN_GPIODV_0		PIN_EE(GPIODV_0)
#define	PIN_GPIODV_1		PIN_EE(GPIODV_1)
#define	PIN_GPIODV_2		PIN_EE(GPIODV_2)
#define	PIN_GPIODV_3		PIN_EE(GPIODV_3)
#define	PIN_GPIODV_4		PIN_EE(GPIODV_4)
#define	PIN_GPIODV_5		PIN_EE(GPIODV_5)
#define	PIN_GPIODV_6		PIN_EE(GPIODV_6)
#define	PIN_GPIODV_7		PIN_EE(GPIODV_7)
#define	PIN_GPIODV_8		PIN_EE(GPIODV_8)
#define	PIN_GPIODV_9		PIN_EE(GPIODV_9)
#define	PIN_GPIODV_10		PIN_EE(GPIODV_10)
#define	PIN_GPIODV_11		PIN_EE(GPIODV_11)

/* pin gpiow */
#define PIN_GPIOW_BASE		(PIN_GPIODV_BASE + PIN_GPIODV_SIZE)
#define PIN_GPIOW_SIZE		(16)
#define	PIN_GPIOW_0		PIN_EE(GPIOW_0)
#define	PIN_GPIOW_1		PIN_EE(GPIOW_1)
#define	PIN_GPIOW_2		PIN_EE(GPIOW_2)
#define	PIN_GPIOW_3		PIN_EE(GPIOW_3)
#define	PIN_GPIOW_4		PIN_EE(GPIOW_4)
#define	PIN_GPIOW_5		PIN_EE(GPIOW_5)
#define	PIN_GPIOW_6		PIN_EE(GPIOW_6)
#define	PIN_GPIOW_7		PIN_EE(GPIOW_7)
#define	PIN_GPIOW_8		PIN_EE(GPIOW_8)
#define	PIN_GPIOW_9		PIN_EE(GPIOW_9)
#define	PIN_GPIOW_10		PIN_EE(GPIOW_10)
#define	PIN_GPIOW_11		PIN_EE(GPIOW_11)
#define	PIN_GPIOW_12		PIN_EE(GPIOW_12)
#define	PIN_GPIOW_13		PIN_EE(GPIOW_13)
#define	PIN_GPIOW_14		PIN_EE(GPIOW_14)
#define	PIN_GPIOW_15		PIN_EE(GPIOW_15)

/* pin gpioclk */
#define PIN_GPIOCLK_BASE	(PIN_GPIOW_BASE + PIN_GPIOW_SIZE)
#define PIN_GPIOCLK_SIZE	(2)
#define	PIN_GPIOCLK_0		PIN_EE(GPIOCLK_0)
#define	PIN_GPIOCLK_1		PIN_EE(GPIOCLK_1)

/* pin test_n */
#define PIN_GPIO_TEST_N_BASE	(PIN_GPIOCLK_BASE + PIN_GPIOCLK_SIZE)
#define PIN_GPIO_TEST_N_SIZE	(1)
#define	PIN_GPIO_TEST_N		PIN_EE(GPIO_TEST_N)

/* pin gpio_usb */
#define PIN_GPIO_USB_BASE	(PIN_GPIO_TEST_N_BASE + PIN_GPIO_TEST_N_SIZE)
#define PIN_GPIO_USB_SIZE	(2)
#define	PIN_GPIO_USB_DPDM	PIN_EE(GPIO_USB_DPDM)
#define	PIN_GPIO_USB_DMDP	PIN_EE(GPIO_USB_DMDP)

#define PIN_EE_SIZE	(PIN_GPIO_USB_BASE + PIN_GPIO_USB_SIZE - PIN_EE_BASE)

#define PIN_SIZE	(PIN_AO_SIZE + PIN_EE_SIZE)


/* gpio pull up enable register index */
#define PULL_EN_REG0		0
#define PULL_EN_REG1		1
#define PULL_EN_REG2		2
#define PULL_EN_REG3		3
#define PULL_EN_REG4		4

/* gpio pull up register index */
#define PULL_UP_REG0		0
#define PULL_UP_REG1		1
#define PULL_UP_REG2		2
#define PULL_UP_REG3		3
#define PULL_UP_REG4		4

/* gpio direction register index */
#define GPIO_D_REG0		0
#define GPIO_D_REG1		3
#define GPIO_D_REG2		6
#define GPIO_D_REG3		9
#define GPIO_D_REG4		12

/* gpio output register index */
#define GPIO_O_REG0		1
#define GPIO_O_REG1		4
#define GPIO_O_REG2		7
#define GPIO_O_REG3		10
#define GPIO_O_REG4		13

/* gpio input register index */
#define GPIO_I_REG0		2
#define GPIO_I_REG1		5
#define GPIO_I_REG2		8
#define GPIO_I_REG3		11
#define GPIO_I_REG4		14

/* AO_GPIO_O_EN_N */
#define GPIO_AO_D_REG0		0
#define GPIO_AO_D_REG0_BIT0	0
#define GPIO_AO_O_REG0		0
#define GPIO_AO_O_REG0_BIT16	16
/* AO_GPIO_I */
#define GPIO_AO_I_REG1		1
#define GPIO_AO_I_REG1_BIT0	0
/* AO_RTI_PULL_UP_REG */
#define GPIO_AO_PULL_EN_REG0		0
#define GPIO_AO_PULL_EN_REG0_BIT0	0
#define GPIO_AO_PULL_UP_REG0		0
#define GPIO_AO_PULL_UP_REG0_BIT16	16


#define GPIO_DV_D_REG0		GPIO_D_REG0
#define GPIO_DV_O_REG0		GPIO_O_REG0
#define GPIO_DV_I_REG0		GPIO_I_REG0
#define GPIO_DV_REG0_BIT0	0

#define GPIO_H_D_REG1		GPIO_D_REG1
#define GPIO_H_O_REG1		GPIO_O_REG1
#define GPIO_H_I_REG1		GPIO_I_REG1
#define GPIO_H_REG1_BIT20	20

#define GPIO_CARD_D_REG2	GPIO_D_REG2
#define GPIO_CARD_O_REG2	GPIO_O_REG2
#define GPIO_CARD_I_REG2	GPIO_I_REG2
#define GPIO_CARD_REG2_BIT20	20

#define GPIO_BOOT_D_REG2	GPIO_D_REG2
#define GPIO_BOOT_O_REG2	GPIO_O_REG2
#define GPIO_BOOT_I_REG2	GPIO_I_REG2
#define GPIO_BOOT_REG2_BIT0	0

#define GPIO_CLK_D_REG3		GPIO_D_REG3
#define GPIO_CLK_O_REG3		GPIO_O_REG3
#define GPIO_CLK_I_REG3		GPIO_I_REG3
#define GPIO_CLK_REG3_BIT28	28

#define GPIO_Z_D_REG3		GPIO_D_REG3
#define GPIO_Z_O_REG3		GPIO_O_REG3
#define GPIO_Z_I_REG3		GPIO_I_REG3
#define GPIO_Z_REG3_BIT0	0

#define GPIO_W_D_REG4		GPIO_D_REG4
#define GPIO_W_O_REG4		GPIO_O_REG4
#define GPIO_W_I_REG4		GPIO_I_REG4
#define GPIO_W_REG4_BIT0	0


#define PMX_FUNCTION_NUM	(5)


/*
 * [0-4]   5 bits   ee pinmux bit
 * [5-9]   5 bits   ee pinmux register
 * [10-14] 5 bits   ao pinmux bit
 * [15-18] 4 bits   ao pinmux register
 * [19]    1 bits   jtag select (sd/ao)
 * [20-24] 5 bits   clk control bit
 * [25-28] 4 bits   reserved
 * [29-31] 3 bits   class to distinguish above
 */

#define PMX_EE_BIT_SFT		0
#define PMX_EE_BIT_LEN		5
#define PMX_EE_BIT_MSK		(((1 << PMX_EE_BIT_LEN) - 1) << PMX_EE_BIT_SFT)
#define PMX_EE_REG_SFT		5
#define PMX_EE_REG_LEN		5
#define PMX_EE_REG_MSK		(((1 << PMX_EE_REG_LEN) - 1) << PMX_EE_REG_SFT)

#define PMX_BIT(v)		((v & PMX_EE_BIT_MSK) >> PMX_EE_BIT_SFT)
#define PMX_REG(v)		((v & PMX_EE_REG_MSK) >> PMX_EE_REG_SFT)


#define PMX_AO_BIT_SFT		10
#define PMX_AO_BIT_LEN		5
#define PMX_AO_BIT_MSK		(((1 << PMX_AO_BIT_LEN) - 1) << PMX_AO_BIT_SFT)
#define PMX_AO_REG_SFT		15
#define PMX_AO_REG_LEN		4
#define PMX_AO_REG_MSK		(((1 << PMX_AO_REG_LEN) - 1) << PMX_AO_REG_SFT)

#define PMX_AO_BIT(v)		((v & PMX_AO_BIT_MSK) >> PMX_AO_BIT_SFT)
#define PMX_AO_REG(v)		((v & PMX_AO_REG_MSK) >> PMX_AO_REG_SFT)

#define PMX_JTAG_SFT		19
#define PMX_JTAG_LEN		1
#define PMX_JTAG_MSK		(((1 << PMX_JTAG_LEN) - 1) << PMX_JTAG_SFT)

#define PMX_JTAG(v)		((v & PMX_JTAG_MSK) >> PMX_JTAG_SFT)

#define PMX_CLK_BIT_SFT		20
#define PMX_CLK_BIT_LEN		5
#define PMX_CLK_BIT_MSK		(((1 << PMX_CLK_BIT_LEN) - 1) \
					<< PMX_CLK_BIT_SFT)

#define PMX_CLK_BIT(v)		((v & PMX_CLK_BIT_MSK) >> PMX_CLK_BIT_SFT)

#define PMX_TYP_SFT		29
#define PMX_TYP_LEN		3
#define PMX_TYP_MSK		(((1 << PMX_TYP_LEN) - 1) << PMX_TYP_SFT)

#define PMX_TYP(v)		((v & PMX_TYP_MSK) >> PMX_TYP_SFT)


enum pmx_type {
	PMX_TYPE_EE = 1,
	PMX_TYPE_AO = 2,
	PMX_TYPE_JTAG = 3,
	PMX_TYPE_CLK = 4,
	PMX_TYPE_NA = 7,
};

#define NA		(-1U)
#define PE(reg, bit)	((PMX_TYPE_EE << PMX_TYP_SFT) | \
			(reg << PMX_EE_REG_SFT) | (bit << PMX_EE_BIT_SFT))
#define PA(reg, bit)	((PMX_TYPE_AO << PMX_TYP_SFT) | \
			(reg << PMX_AO_REG_SFT) | (bit << PMX_AO_BIT_SFT))
#define PJ(sel)		((PMX_TYPE_JTAG << PMX_TYP_SFT) | \
			(sel << PMX_JTAG_SFT))
#define PC(bit)		((PMX_TYPE_CLK << PMX_TYP_SFT) | \
			(bit << PMX_CLK_BIT_SFT))



#endif
