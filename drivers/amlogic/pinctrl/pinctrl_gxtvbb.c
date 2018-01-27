/*
 * drivers/amlogic/pinctrl/pinctrl_gxtvbb.c
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


#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <dt-bindings/gpio/gxtvbb.h>
#include <linux/amlogic/pinctrl_amlogic.h>
#include <linux/amlogic/gpio-amlogic.h>
#include <linux/amlogic/iomap.h>
#include <linux/uaccess.h>

#include "pinctrl_gxtvbb.h"

struct gxtb_od_gpio {
	int start;
	int end;
};

static const struct gxtb_od_gpio gxtvbb_all_od_gpios[] = {
	{GPIOW_5, GPIOW_20},
	{GPIOH_3, GPIOH_5},
};

struct pinctrl_pin_desc gxtvbb_pads[] = {

	PINCTRL_PIN(GPIOAO_0,	"GPIOAO_0"),
	PINCTRL_PIN(GPIOAO_1,	"GPIOAO_1"),
	PINCTRL_PIN(GPIOAO_2,	"GPIOAO_2"),
	PINCTRL_PIN(GPIOAO_3,	"GPIOAO_3"),
	PINCTRL_PIN(GPIOAO_4,	"GPIOAO_4"),
	PINCTRL_PIN(GPIOAO_5,	"GPIOAO_5"),
	PINCTRL_PIN(GPIOAO_6,	"GPIOAO_6"),
	PINCTRL_PIN(GPIOAO_7,	"GPIOAO_7"),
	PINCTRL_PIN(GPIOAO_8,	"GPIOAO_8"),
	PINCTRL_PIN(GPIOAO_9,	"GPIOAO_9"),
	PINCTRL_PIN(GPIOAO_10,	"GPIOAO_10"),
	PINCTRL_PIN(GPIOAO_11,	"GPIOAO_11"),
	PINCTRL_PIN(GPIOAO_12,	"GPIOAO_12"),
	PINCTRL_PIN(GPIOAO_13,	"GPIOAO_13"),

	MESON_PIN(GPIOZ_0,	"GPIOZ_0"),
	MESON_PIN(GPIOZ_1,	"GPIOZ_1"),
	MESON_PIN(GPIOZ_2,	"GPIOZ_2"),
	MESON_PIN(GPIOZ_3,	"GPIOZ_3"),
	MESON_PIN(GPIOZ_4,	"GPIOZ_4"),
	MESON_PIN(GPIOZ_5,	"GPIOZ_5"),
	MESON_PIN(GPIOZ_6,	"GPIOZ_6"),
	MESON_PIN(GPIOZ_7,	"GPIOZ_7"),
	MESON_PIN(GPIOZ_8,	"GPIOZ_8"),
	MESON_PIN(GPIOZ_9,	"GPIOZ_9"),
	MESON_PIN(GPIOZ_10,	"GPIOZ_10"),
	MESON_PIN(GPIOZ_11,	"GPIOZ_11"),
	MESON_PIN(GPIOZ_12,	"GPIOZ_12"),
	MESON_PIN(GPIOZ_13,	"GPIOZ_13"),
	MESON_PIN(GPIOZ_14,	"GPIOZ_14"),
	MESON_PIN(GPIOZ_15,	"GPIOZ_15"),
	MESON_PIN(GPIOZ_16,	"GPIOZ_16"),
	MESON_PIN(GPIOZ_17,	"GPIOZ_17"),
	MESON_PIN(GPIOZ_18,	"GPIOZ_18"),
	MESON_PIN(GPIOZ_19,	"GPIOZ_19"),
	MESON_PIN(GPIOZ_20,	"GPIOZ_20"),

	MESON_PIN(GPIOH_0,	"GPIOH_0"),
	MESON_PIN(GPIOH_1,	"GPIOH_1"),
	MESON_PIN(GPIOH_2,	"GPIOH_2"),
	MESON_PIN(GPIOH_3,	"GPIOH_3"),
	MESON_PIN(GPIOH_4,	"GPIOH_4"),
	MESON_PIN(GPIOH_5,	"GPIOH_5"),
	MESON_PIN(GPIOH_6,	"GPIOH_6"),
	MESON_PIN(GPIOH_7,	"GPIOH_7"),
	MESON_PIN(GPIOH_8,	"GPIOH_8"),
	MESON_PIN(GPIOH_9,	"GPIOH_9"),
	MESON_PIN(GPIOH_10,	"GPIOH_10"),

	MESON_PIN(BOOT_0,	"BOOT_0"),
	MESON_PIN(BOOT_1,	"BOOT_1"),
	MESON_PIN(BOOT_2,	"BOOT_2"),
	MESON_PIN(BOOT_3,	"BOOT_3"),
	MESON_PIN(BOOT_4,	"BOOT_4"),
	MESON_PIN(BOOT_5,	"BOOT_5"),
	MESON_PIN(BOOT_6,	"BOOT_6"),
	MESON_PIN(BOOT_7,	"BOOT_7"),
	MESON_PIN(BOOT_8,	"BOOT_8"),	/* undefined no interrupt */
	MESON_PIN(BOOT_9,	"BOOT_9"),
	MESON_PIN(BOOT_10,	"BOOT_10"),	/* undefined no interrupt */
	MESON_PIN(BOOT_11,	"BOOT_11"),
	MESON_PIN(BOOT_12,	"BOOT_12"),
	MESON_PIN(BOOT_13,	"BOOT_13"),
	MESON_PIN(BOOT_14,	"BOOT_14"),
	MESON_PIN(BOOT_15,	"BOOT_15"),
	MESON_PIN(BOOT_16,	"BOOT_16"),
	MESON_PIN(BOOT_17,	"BOOT_17"),
	MESON_PIN(BOOT_18,	"BOOT_18"),

	MESON_PIN(CARD_0,	"CARD_0"),
	MESON_PIN(CARD_1,	"CARD_1"),
	MESON_PIN(CARD_2,	"CARD_2"),
	MESON_PIN(CARD_3,	"CARD_3"),
	MESON_PIN(CARD_4,	"CARD_4"),
	MESON_PIN(CARD_5,	"CARD_5"),
	MESON_PIN(CARD_6,	"CARD_6"),

	MESON_PIN(GPIOW_0,	"GPIOW_0"),
	MESON_PIN(GPIOW_1,	"GPIOW_1"),
	MESON_PIN(GPIOW_2,	"GPIOW_2"),
	MESON_PIN(GPIOW_3,	"GPIOW_3"),
	MESON_PIN(GPIOW_4,	"GPIOW_4"),
	MESON_PIN(GPIOW_5,	"GPIOW_5"),
	MESON_PIN(GPIOW_6,	"GPIOW_6"),
	MESON_PIN(GPIOW_7,	"GPIOW_7"),
	MESON_PIN(GPIOW_8,	"GPIOW_8"),
	MESON_PIN(GPIOW_9,	"GPIOW_9"),
	MESON_PIN(GPIOW_10,	"GPIOW_10"),
	MESON_PIN(GPIOW_11,	"GPIOW_11"),
	MESON_PIN(GPIOW_12,	"GPIOW_12"),
	MESON_PIN(GPIOW_13,	"GPIOW_13"),
	MESON_PIN(GPIOW_14,	"GPIOW_14"),
	MESON_PIN(GPIOW_15,	"GPIOW_15"),
	MESON_PIN(GPIOW_16,	"GPIOW_16"),
	MESON_PIN(GPIOW_17,	"GPIOW_17"),
	MESON_PIN(GPIOW_18,	"GPIOW_18"),
	MESON_PIN(GPIOW_19,	"GPIOW_19"),
	MESON_PIN(GPIOW_20,	"GPIOW_20"),

	MESON_PIN(GPIOY_0,	"GPIOY_0"),
	MESON_PIN(GPIOY_1,	"GPIOY_1"),
	MESON_PIN(GPIOY_2,	"GPIOY_2"),
	MESON_PIN(GPIOY_3,	"GPIOY_3"),
	MESON_PIN(GPIOY_4,	"GPIOY_4"),
	MESON_PIN(GPIOY_5,	"GPIOY_5"),
	MESON_PIN(GPIOY_6,	"GPIOY_6"),
	MESON_PIN(GPIOY_7,	"GPIOY_7"),
	MESON_PIN(GPIOY_8,	"GPIOY_8"),
	MESON_PIN(GPIOY_9,	"GPIOY_9"),
	MESON_PIN(GPIOY_10,	"GPIOY_10"),
	MESON_PIN(GPIOY_11,	"GPIOY_11"),
	MESON_PIN(GPIOY_12,	"GPIOY_12"),
	MESON_PIN(GPIOY_13,	"GPIOY_13"),

	MESON_PIN(GPIOX_0,	"GPIOX_0"),
	MESON_PIN(GPIOX_1,	"GPIOX_1"),
	MESON_PIN(GPIOX_2,	"GPIOX_2"),
	MESON_PIN(GPIOX_3,	"GPIOX_3"),
	MESON_PIN(GPIOX_4,	"GPIOX_4"),
	MESON_PIN(GPIOX_5,	"GPIOX_5"),
	MESON_PIN(GPIOX_6,	"GPIOX_6"),
	MESON_PIN(GPIOX_7,	"GPIOX_7"),
	MESON_PIN(GPIOX_8,	"GPIOX_8"),
	MESON_PIN(GPIOX_9,	"GPIOX_9"),
	MESON_PIN(GPIOX_10,	"GPIOX_10"),
	MESON_PIN(GPIOX_11,	"GPIOX_11"),
	MESON_PIN(GPIOX_12,	"GPIOX_12"),
	MESON_PIN(GPIOX_13,	"GPIOX_13"),
	MESON_PIN(GPIOX_14,	"GPIOX_14"),
	MESON_PIN(GPIOX_15,	"GPIOX_15"),
	MESON_PIN(GPIOX_16,	"GPIOX_16"),
	MESON_PIN(GPIOX_17,	"GPIOX_17"),
	MESON_PIN(GPIOX_18,	"GPIOX_18"),
	MESON_PIN(GPIOX_19,	"GPIOX_19"),
	MESON_PIN(GPIOX_20,	"GPIOX_20"),
	MESON_PIN(GPIOX_21,	"GPIOX_21"),
	MESON_PIN(GPIOX_22,	"GPIOX_22"),
	MESON_PIN(GPIOX_23,	"GPIOX_23"),
	MESON_PIN(GPIOX_24,	"GPIOX_24"),
	MESON_PIN(GPIOX_25,	"GPIOX_25"),
	MESON_PIN(GPIOX_26,	"GPIOX_26"),
	MESON_PIN(GPIOX_27,	"GPIOX_27"),

	MESON_PIN(GPIOCLK_0,	"GPIOCLK_0"),

	MESON_PIN(GPIO_TEST_N,	"GPIO_TEST_N"),

};


/*
 * see APB4 register of Periphs-Registers document
 * name		first pin,	last pin
 *		pullen reg,	start bit
 *		pullup reg,	start bit
 *		gpio dir reg,	start bit
 *		gpio out reg,	start bit
 *		gpio in reg,	start bit
 */
static struct meson_bank gxtvbb_ee_banks[] = {

	BANK("W",	PIN_GPIOW_0,	PIN_GPIOW_20,
			0,		0, /* PULL_UP_EN_REG0	[20-0] */
			0,		0, /* PULL_UP_REG0	[20-0] */
			0,		0, /* gpioW_20~0 : GPIO_REG0[20:0] */
			1,		0,
			2,		0),

	BANK("H",	PIN_GPIOH_0,	PIN_GPIOH_10,
			1,		20, /* PULL_UP_EN_REG1	[30-20] */
			1,		20, /* PULL_UP_REG1	[30-20] */
			3,		20, /* gpioH_10~0 : GPIO_REG1[30:20] */
			4,		20,
			5,		20),

	BANK("Y",	PIN_GPIOY_0,	PIN_GPIOY_13,
			1,		0, /* PULL_UP_EN_REG1	[13-0] */
			1,		0, /* PULL_UP_REG1	[13-0] */
			3,		0, /* gpioY_13~0 : GPIO_REG1[13:0] */
			4,		0,
			5,		0),

	BANK("CARD",	PIN_CARD_0,	PIN_CARD_6,
			2,		20, /* PULL_UP_EN_REG2	[26-20] */
			2,		20, /* PULL_UP_REG2	[26-20] */
			6,		20, /* card_6~0 : GPIO_REG2[26:20] */
			7,		20,
			8,		20),

	BANK("BOOT",	PIN_BOOT_0,	PIN_BOOT_18,
			2,		0, /* PULL_UP_EN_REG2	[18-0] */
			2,		0, /* PULL_UP_REG2	[18-0] */
			6,		0, /* boot_18~0 : GPIO_REG2[18:0] */
			7,		0, /* (no boot_8 and boot_10) */
			8,		0),

	BANK("CLK",	PIN_GPIOCLK_0,	PIN_GPIOCLK_0,
			3,		24, /* PULL_UP_EN_REG3	[24] */
			3,		24, /* PULL_UP_REG3	[24] */
			9,		24, /* gpioCLK0 : GPIO_REG3[24] */
			10,		24,
			11,		24),

	BANK("Z",	PIN_GPIOZ_0,	PIN_GPIOZ_20,
			3,		0, /* PULL_UP_EN_REG3	[20-0] */
			3,		0, /* PULL_UP_REG3	[20-0] */
			9,		0, /* gpioZ_20~0 : GPIO_REG3[20:0] */
			10,		0,
			11,		0),

	BANK("X",	PIN_GPIOX_0,	PIN_GPIOX_27,
			4,		0, /* PULL_UP_EN_REG4	[27-0] */
			4,		0, /* PULL_UP_REG4	[27-0] */
			12,		0, /* gpioX_27~0 : GPIO_REG4[27:0] */
			13,		0,
			14,		0),
};


/*
 * see AO Registers document.
 */
static struct meson_bank gxtvbb_ao_banks[] = {
	/*   name    first         last
	 *   pullen  pull     dir     out     in  */
	BANK("AO",	GPIOAO_0,	GPIOAO_13,
			0,		0,  /* AO_RTI_PULL_UP_REG [13-0] */
			0,		16, /* AO_RTI_PULL_UP_REG [29-16] */
			0,		0,  /* O_GPIO_O_EN_N [13-0] */
			0,		16, /* O_GPIO_O_EN_N [29-16] */
			1,		0), /* AO_GPIO_I [13-0] */
};


static struct meson_domain_data gxtvbb_domain_data[] = {
	{
		.name		= "banks",
		.banks		= gxtvbb_ee_banks,
		.num_banks	= ARRAY_SIZE(gxtvbb_ee_banks),
		.pin_base	= 14,
		.num_pins	= 125,
	},
	{
		.name		= "ao-bank",
		.banks		= gxtvbb_ao_banks,
		.num_banks	= ARRAY_SIZE(gxtvbb_ao_banks),
		.pin_base	= 0,
		.num_pins	= 14,
	},

};


#define NE		(0xffffffff)
#define PK(reg, bit)	((reg<<5)|bit)

#define PIN_SIZE	11
static unsigned int gxtvbb_gpio_to_pin[][PIN_SIZE] = {

	[GPIOAO_0] = {PK(AO, 0), PK(AO, 4), NE, NE, NE,
			NE, NE, NE, NE, NE, NE,},
	[GPIOAO_1] = {PK(AO, 1), PK(AO, 5), NE, NE, NE,
			NE, NE, NE, NE, NE, NE,},
	[GPIOAO_2] = {PK(AO, 2), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[GPIOAO_3] = {PK(AO, 3), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[GPIOAO_4] = {NE, PK(AO, 6), PK(AO, 8), PK(AO, 10), NE,
			NE, NE, NE, NE, NE, NE,},
	[GPIOAO_5] = {NE, PK(AO, 7), PK(AO, 9), PK(AO, 11), NE, NE,
			NE, NE, NE, NE, NE,},
	[GPIOAO_6] = {NE, NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[GPIOAO_7] = {PK(AO, 12), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[GPIOAO_8] = {NE, NE, PK(AO, 13), PK(AO, 15), PK(AO, 14),
			NE, NE, NE, NE, NE, NE,},
	[GPIOAO_9] = {NE, NE, PK(AO, 16), PK(AO, 18), PK(AO, 17),
			NE, NE, NE, NE, NE, NE,},
	[GPIOAO_10] = {NE, NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[GPIOAO_11] = {NE, NE, PK(AO, 19), PK(AO, 20), NE, NE, NE,
			NE, NE, NE, NE,},
	[GPIOAO_12] = {NE, NE, PK(AO, 21), PK(AO, 22), NE, NE, NE, NE, NE, NE,
		NE,},
	[GPIOAO_13] = {PK(AO, 23), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},


	[PIN_GPIO_TEST_N] = {PK(AO, 24), PK(AO, 25), PK(AO, 26), PK(AO, 27),
	NE, NE, NE, NE, NE, NE, NE,},


	[PIN_BOOT_0] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_1] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_2] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(9, 18), PK(10, 9), NE,},
	[PIN_BOOT_3] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(9, 19), PK(10, 9), NE,},
	[PIN_BOOT_4] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(9, 20), PK(10, 9), NE,},
	[PIN_BOOT_5] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(9, 21), PK(10, 9), NE,},
	[PIN_BOOT_6] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(9, 22), PK(10, 9), NE,},
	[PIN_BOOT_7] = {PK(5, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(9, 23), PK(10, 9), NE,},
	/* no boot_8 */
	[PIN_BOOT_9] = {NE, NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	/* no boot_10 */
	[PIN_BOOT_11] = {NE, PK(5, 16), NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_12] = {NE, PK(5, 17), NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_13] = {NE, PK(5, 18), NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_14] = {NE, NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_15] = {PK(5, 13), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_16] = {PK(5, 14), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_17] = {PK(5, 15), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_BOOT_18] = {NE, PK(5, 19), NE, NE, NE, NE, NE, NE, NE, NE, NE,},


	[PIN_CARD_0] = {PK(5, 1), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_CARD_1] = {PK(5, 2), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_CARD_2] = {PK(5, 3), NE, PK(5, 7), NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_CARD_3] = {PK(5, 4), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_CARD_4] = {PK(5, 5), NE, NE, PK(5, 8), PK(5, 9), NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_CARD_5] = {PK(5, 6), NE, NE, PK(5, 10), PK(5, 11), NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_CARD_6] = {NE, NE, NE, NE, NE, NE, NE, NE, NE, PK(10, 9), NE,},
	/* no card_7 */
	/* no card_8 */

	[PIN_GPIOW_0] = {PK(5, 20), PK(5, 22), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_1] = {PK(5, 21), PK(5, 23), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_2] = {NE, NE, PK(5, 25), PK(5, 26), PK(5, 24),
			PK(5, 27), NE, NE, NE, NE, NE,},
	[PIN_GPIOW_3] = {NE, NE, PK(6, 1), PK(6, 2), PK(6, 0), PK(6, 3),
			PK(6, 4), PK(9, 30), NE, NE, NE,},
	[PIN_GPIOW_4] = {PK(6, 6), PK(6, 5), NE, NE, NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOW_5] = {PK(6, 7), NE, NE, NE, NE, NE, NE, NE,
			NE, NE, NE,},
	[PIN_GPIOW_6] = {PK(6, 8), NE, NE, NE, NE, NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOW_7] = {PK(6, 9), PK(10, 1), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_8] = {PK(6, 10), PK(6, 11), NE, NE, NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOW_9] = {PK(6, 12), NE, NE, NE, NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOW_10] = {PK(6, 13), NE, NE, NE, NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOW_11] = {PK(6, 14), PK(10, 2), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_12] = {PK(6, 15), PK(6, 16), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_13] = {PK(6, 17), NE, NE, NE, NE, NE, NE, NE,
			NE, NE, NE,},
	[PIN_GPIOW_14] = {PK(6, 18), NE, NE, NE, NE, NE, NE, NE,
			NE, NE, NE,},
	[PIN_GPIOW_15] = {PK(6, 19), PK(10, 3), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_16] = {PK(6, 20), PK(6, 21), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_17] = {PK(6, 22), NE, NE, NE, NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOW_18] = {PK(6, 23), NE, NE, NE, NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOW_19] = {PK(6, 24), PK(10, 4), NE, NE, NE, NE,
			NE, NE, NE, NE, NE,},
	[PIN_GPIOW_20] = {PK(6, 25), PK(6, 26), NE, NE, NE, NE,
			NE, NE,	NE, NE, NE,},


	[PIN_GPIOZ_0] = {PK(8, 5), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_1] = {PK(8, 6), NE, PK(8, 13), NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_2] = {PK(8, 7), PK(8, 11), NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_3] = {PK(8, 8), PK(8, 12), NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_4] = {PK(8, 9), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_5] = {NE, PK(8, 10), NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_6] = {PK(8, 14), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_7] = {PK(8, 15), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_8] = {PK(8, 16), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_9] = {PK(8, 17), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_10] = {PK(8, 18), NE, NE, NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_11] = {PK(8, 19), NE, PK(8, 29), NE, NE, NE, NE, NE, NE,
			PK(10, 9), NE,},
	[PIN_GPIOZ_12] = {PK(8, 20), NE, PK(8, 30), PK(9, 0), NE, NE, NE,
			NE, NE, NE, NE,},
	[PIN_GPIOZ_13] = {NE, PK(8, 21), NE, NE, NE, NE, NE, NE,
			PK(9, 24), NE, NE,},
	[PIN_GPIOZ_14] = {NE, PK(8, 22), NE, NE, PK(9, 1), NE, NE, NE,
			PK(9, 25), NE, NE,},
	[PIN_GPIOZ_15] = {NE, PK(8, 23), NE, NE, NE, NE, NE, NE,
			PK(9, 26), NE, NE,},
	[PIN_GPIOZ_16] = {NE, PK(8, 24), NE, NE, PK(9, 2), NE, NE, NE,
			PK(9, 27), NE, NE,},
	[PIN_GPIOZ_17] = {NE, PK(8, 25), PK(9, 9), PK(9, 3), NE, PK(9, 4),
			PK(9, 7), NE, PK(9, 28), PK(10, 0), NE,},
	[PIN_GPIOZ_18] = {NE, PK(8, 26), PK(9, 10), PK(9, 5), NE, PK(9, 6),
			PK(9, 8), NE, PK(9, 29), PK(10, 0), NE,},
	[PIN_GPIOZ_19] = {NE, PK(8, 27), PK(9, 11), NE, NE, NE, NE, NE, NE,
			PK(10, 0), NE,},
	[PIN_GPIOZ_20] = {NE, PK(8, 28), PK(9, 12), NE, NE, NE, NE, NE, NE,
			PK(10, 0), NE,},


	[PIN_GPIOH_0] = {PK(7, 0), PK(7, 4), PK(7, 15), PK(7, 8), NE,
			NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_1] = {PK(7, 1), NE, NE, NE, NE,
			PK(7, 9), PK(7, 10), PK(7, 11), NE, NE, NE,},
	[PIN_GPIOH_2] = {PK(7, 2), NE, NE, NE, NE, NE, NE,
			PK(7, 12), NE, NE, NE,},
	[PIN_GPIOH_3] = {NE, NE, NE, PK(7, 19), PK(7, 6),
			NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_4] = {NE, NE, NE, PK(7, 20), PK(7, 7),
			NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_5] = {NE, NE, NE, PK(7, 21), PK(7, 23),
			NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_6] = {PK(7, 3), PK(7, 5), PK(7, 16), PK(7, 22), PK(7, 24),
			NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_7] = {NE, PK(7, 25), PK(7, 17), NE, NE,
			PK(7, 27), PK(7, 28), NE, NE, NE, NE,},
	[PIN_GPIOH_8] = {NE, PK(7, 26), PK(7, 18), NE, NE,
			PK(7, 29), PK(7, 30), NE, NE, NE, NE,},
	[PIN_GPIOH_9] = {PK(8, 3), NE, PK(7, 13), NE, NE,
			PK(7, 31), PK(8, 0), PK(8, 1), NE, NE, NE,},
	[PIN_GPIOH_10] = {PK(8, 4), NE, PK(7, 14), NE, NE, NE, NE,
			PK(8, 2), NE, NE, NE,},


	[PIN_GPIOX_0] = {PK(0, 0), PK(1, 0), PK(0, 23), PK(2, 0), PK(1, 10),
			PK(0, 10), PK(1, 22), PK(1, 24), NE, NE, NE,},
	[PIN_GPIOX_1] = {PK(0, 1), PK(1, 0), PK(0, 23), PK(2, 1), NE,
			PK(0, 11), PK(1, 23), PK(1, 25), NE, NE, NE,},
	[PIN_GPIOX_2] = {PK(0, 2), PK(1, 1), PK(0, 23), PK(2, 2), NE,
			PK(0, 12), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_3] = {PK(0, 3), PK(1, 1), PK(0, 23), PK(2, 3), NE,
			PK(0, 13), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_4] = {PK(0, 4), PK(1, 1), PK(0, 23), PK(2, 4), NE,
			PK(0, 14), PK(1, 18), PK(1, 26), NE, NE, NE,},
	[PIN_GPIOX_5] = {PK(0, 5), PK(1, 1), PK(0, 23), PK(2, 5), NE,
			PK(0, 15), PK(1, 19), PK(1, 27), NE, NE, NE,},
	[PIN_GPIOX_6] = {PK(0, 6), PK(1, 1), PK(0, 23), PK(2, 6), NE,
			PK(0, 16), PK(1, 20), PK(1, 28), NE, NE, NE,},
	[PIN_GPIOX_7] = {PK(0, 7), PK(1, 1), PK(0, 23), PK(2, 7), NE,
			PK(0, 17), PK(1, 21), PK(1, 29), NE, NE, NE,},
	[PIN_GPIOX_8] = {PK(0, 8), PK(1, 2), PK(0, 23), PK(2, 8), PK(1, 11),
			PK(0, 18), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_9] = {PK(0, 9), PK(1, 2), PK(0, 23), PK(2, 9), NE,
			PK(0, 19), PK(2, 18), NE, NE, NE, NE,},
	[PIN_GPIOX_10] = {NE, PK(1, 3), PK(0, 23), PK(2, 10), NE, PK(0, 20),
			PK(2, 20), PK(2, 21), PK(2, 19), PK(3, 29), NE,},
	[PIN_GPIOX_11] = {NE, PK(1, 3), PK(0, 23), PK(2, 11), NE, PK(0, 21),
			PK(2, 23), NE, NE, PK(3, 29), NE,},
	[PIN_GPIOX_12] = {PK(2, 24), PK(1, 3), PK(0, 23), PK(2, 12), NE,
			PK(3, 0), PK(0, 28), NE, NE, PK(3, 29), NE,},
	[PIN_GPIOX_13] = {PK(2, 25), PK(1, 3), PK(0, 23), PK(2, 13), NE,
			PK(3, 1), PK(0, 29), NE, NE, PK(3, 29), NE,},
	[PIN_GPIOX_14] = {PK(2, 26), PK(1, 3), PK(0, 23), PK(2, 14), NE,
			PK(3, 2), PK(0, 30), NE, NE, PK(3, 29), NE,},
	[PIN_GPIOX_15] = {PK(2, 27), PK(1, 3), PK(0, 23), PK(2, 15), NE,
			PK(3, 3), PK(0, 31), NE, NE, PK(3, 29), NE,},
	[PIN_GPIOX_16] = {NE, PK(1, 4), PK(0, 23), PK(2, 16), PK(1, 12),
			PK(3, 4), NE, PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_17] = {NE, PK(1, 4), PK(0, 23), PK(2, 17), NE,
			PK(3, 5), NE, PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_18] = {NE, PK(1, 5), PK(0, 23), NE, NE, PK(3, 6), NE,
			PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_19] = {NE, PK(1, 5), PK(0, 23), NE, NE, PK(3, 7), NE,
			PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_20] = {PK(3, 20), PK(1, 5), PK(0, 23), PK(3, 12), NE,
			PK(3, 8), NE, PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_21] = {PK(3, 21), PK(1, 5), PK(0, 23), PK(3, 13), NE,
			PK(3, 9), PK(3, 17), PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_22] = {NE, PK(1, 5), PK(0, 23), PK(3, 14), NE, PK(3, 10),
			PK(3, 18), PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_23] = {NE, PK(1, 5), PK(0, 23), PK(3, 15), NE,
			PK(3, 11), NE, PK(2, 28), NE, PK(3, 29), NE,},
	[PIN_GPIOX_24] = {NE, PK(1, 6), PK(0, 24), PK(3, 16), PK(1, 13),
			NE, NE, NE, PK(3, 22), PK(3, 29), NE,},
	[PIN_GPIOX_25] = {NE, PK(1, 7), PK(0, 25), NE, PK(1, 14), NE,
			PK(3, 19), NE, PK(3, 23), PK(3, 29), NE,},
	[PIN_GPIOX_26] = {PK(3, 27), NE, PK(0, 26), NE, PK(1, 15), NE,  NE,
			PK(3, 24), PK(3, 25), PK(3, 30), PK(3, 26),},
	[PIN_GPIOX_27] = {PK(3, 28), NE, PK(0, 27), NE, PK(1, 16),
			PK(1, 17), NE, NE, NE, PK(3, 31), NE,},


	[PIN_GPIOY_0] = {NE, NE, NE, PK(10, 26), NE,
			PK(4, 4), NE, NE, NE, NE, NE,},
	[PIN_GPIOY_1] = {NE, NE, NE, PK(10, 27), NE,
			PK(4, 5), NE, PK(4, 0), NE, NE, NE,},
	[PIN_GPIOY_2] = {PK(4, 16), NE, NE, NE, NE, PK(4, 6), NE,
			PK(4, 1), NE, NE, NE,},
	[PIN_GPIOY_3] = {PK(4, 17), NE, NE, NE, NE, PK(4, 7), NE,
			PK(4, 2), PK(4, 18), PK(4, 19), NE,},
	[PIN_GPIOY_4] = {NE, PK(4, 20), NE, NE, NE, PK(4, 8), NE,
			PK(4, 3), NE, NE, NE,},
	[PIN_GPIOY_5] = {NE, PK(4, 21), NE, NE, NE, PK(4, 9), NE,
			PK(4, 3), NE, NE, NE,},
	[PIN_GPIOY_6] = {NE, PK(4, 22), NE, NE, NE, PK(4, 10), NE,
			PK(4, 3), NE, NE, NE,},
	[PIN_GPIOY_7] = {PK(4, 26), PK(4, 23), PK(4, 28), NE, NE,
			PK(4, 11), NE, PK(4, 3), PK(10, 16), PK(10, 17), NE,},
	[PIN_GPIOY_8] = {PK(4, 27), PK(4, 24), PK(4, 29), NE, NE,
			PK(4, 12), NE, PK(4, 3), PK(10, 18), NE, NE,},
	[PIN_GPIOY_9] = {NE, PK(4, 25), NE, NE, NE, PK(4, 13), NE,
			PK(4, 3), PK(10, 19), PK(10, 20), NE,},
	[PIN_GPIOY_10] = {PK(4, 30), NE, PK(10, 10), NE, NE,
			PK(4, 14), NE, PK(4, 3), NE, NE, NE,},
	[PIN_GPIOY_11] = {PK(4, 31), NE, PK(10, 11), NE, NE,
			PK(4, 15), NE, PK(4, 3), NE, NE, NE,},
	[PIN_GPIOY_12] = {PK(10, 12), NE, PK(10, 14), NE, NE, NE, NE, NE,
			PK(10, 21), PK(10, 22), NE,},
	[PIN_GPIOY_13] = {PK(10, 13), NE, PK(10, 15), NE, NE, NE, NE, NE,
			PK(10, 23), PK(10, 24), NE,},


	[PIN_GPIOCLK_0] = {NE, NE, PK(10, 6), NE, NE, NE, NE, NE, NE, NE, NE,},
};


static int gxtvbb_gpio_name_to_num(const char *name)
{
	int i, tmp = 100, num = 0;
	int len = 0;
	char *p = NULL;
	char *start = NULL;
	if (!name)
		return -1;
	if (!strcmp(name, "GPIO_TEST_N"))
		return GPIO_TEST_N;
	if (!strcmp(name, "GPIOCLK_0"))
		return GPIOCLK_0;

	len = strlen(name);
	p = kzalloc(len+1, GFP_KERNEL);
	start = p;
	if (!p) {
		pr_info("%s:malloc error\n", __func__);
		return -1;
	}
	p = strcpy(p, name);

	for (i = 0; i < len; p++, i++) {
		if (*p == '_') {
			*p = '\0';
			tmp = i;
		}
		if (i > tmp && *p >= '0' && *p <= '9')
			num = num*10 + *p-'0';
	}
	p = start;
	if (!strcmp(p, "GPIOAO"))	/* [0-13] 14 */
		num = num + 0;
	else if (!strcmp(p, "GPIOZ"))	/* [0-20] 21 */
		num = num + 14;
	else if (!strcmp(p, "GPIOH"))	/* [0-10] 11 */
		num = num + 35;
	else if (!strcmp(p, "BOOT"))	/* [0-18] 19 */
		num = num + 46;
	else if (!strcmp(p, "CARD"))	/* [0-8]  9  */
		num = num + 65;
	else if (!strcmp(p, "GPIOW"))	/* [0-20] 21 */
		num = num + 74;
	else if (!strcmp(p, "GPIOY"))	/* [0-13] 14 */
		num = num + 95;
	else if (!strcmp(p, "GPIOX"))	/* [0-27] 28 */
		num = num + 109;
	else
		num = -1;

	kzfree(start);
	return num;
}


static int  gxtvbb_clear_pinmux(struct amlogic_pmx *apmx, unsigned int pin)
{
	unsigned int *gpio_reg =  &gxtvbb_gpio_to_pin[pin][0];
	int i, dom, reg, bit, ret;
	struct meson_domain *domain = NULL;
	for (i = 0; i < PIN_SIZE; i++) {
		if (gpio_reg[i] != NE) {
			reg = GPIO_REG(gpio_reg[i])&0xf;
			bit = GPIO_BIT(gpio_reg[i]);
			dom = GPIO_REG(gpio_reg[i])>>4;
			domain = &apmx->domains[dom];
			ret = regmap_update_bits(domain->reg_mux, reg * 4,
						 BIT(bit), 0);
			if (ret)
				return ret;
		}
	}
	return 0;
}


#ifdef CONFIG_ARM64

/*
 * function id 0x82000046 is to set AO_SEC_REG0 bit[0] in bl31.
 * bit[0] is set with arg0.
 * 0: gpio input 1: gpio output
 *
 * this function supported by xing.xu@amlogic.com
 */
static noinline int __invoke_psci_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	register long x0 asm("x0") = function_id;
	register long x1 asm("x1") = arg0;
	register long x2 asm("x2") = arg1;
	register long x3 asm("x3") = arg2;
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3));

	return function_id;
}

#else

static noinline int __invoke_psci_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	return 0;
}

#endif /* CONFIG_ARM64 */


/*
 * AO_GPIO_O_EN_N	0x09<<2=0x24	bit[31]		output level
 * AO_GPIO_I		0x0a<<2=0x28	bit[31]		input level
 * AO_SEC_REG0		0x50<<2=0x140	bit[0]		input enable
 * AO_SEC_REG0		0xda100000+0x140=0xda100140
 * AO_RTI_PULL_UP_REG	0x0b<<2=0x2c	bit[30]		pull-up/down
 * AO_RTI_PULL_UP_REG	0x0b<<2=0x2c	bit[14]		pull-up enable
 */
static int gxtvbb_extern_gpio_output(struct meson_domain *domain,
				   unsigned int pin, int value)
{
	if (PIN_GPIO_TEST_N == pin) {
		pr_info("%s PIN_GPIO_TEST_N\n", __func__);

		/* set TEST_N to gpio output */
		/* AO_SEC_REG0 bit[0] = 1 */
		__invoke_psci_fn_smc(0x82000046, 1, 0, 0);

		/* set TEST_N output level to #value */
		aml_aobus_update_bits(0x24, BIT(31), value ? BIT(31) : 0);
		return 0;
	}
	return 1;
}


int gxtvbb_extern_gpio_get(struct meson_domain *domain, unsigned int pin)
{
	if (PIN_GPIO_TEST_N == pin)
		return !!(aml_read_aobus(0x24) & BIT(31));

	return -1;
}

static int is_gxtvbb_gpio_in_od_domain(unsigned int pin)
{
	int i = 0;
	int len = sizeof(gxtvbb_all_od_gpios)/sizeof(gxtvbb_all_od_gpios[0]);
	for (i = 0; i < len; i++) {
		if ((pin >= gxtvbb_all_od_gpios[i].start)
			&& (pin <= gxtvbb_all_od_gpios[i].end))
			return 1;
	}
	return 0;
}

static struct amlogic_pinctrl_soc_data gxtvbb_pinctrl_data = {
	.pins = gxtvbb_pads,
	.npins = ARRAY_SIZE(gxtvbb_pads),
	.domain_data	= gxtvbb_domain_data,
	.num_domains	= ARRAY_SIZE(gxtvbb_domain_data),
	.meson_clear_pinmux = gxtvbb_clear_pinmux,
	.name_to_pin = gxtvbb_gpio_name_to_num,
	.soc_extern_gpio_output = gxtvbb_extern_gpio_output,
	.soc_extern_gpio_get = gxtvbb_extern_gpio_get,
	.is_od_domain = is_gxtvbb_gpio_in_od_domain,
};


static struct of_device_id gxtvbb_pinctrl_of_table[] = {
	{
		.compatible = "amlogic, pinmux-gxtvbb",
	},
	{},
};


static int gxtvbb_pinctrl_probe(struct platform_device *pdev)
{
	return amlogic_pmx_probe(pdev, &gxtvbb_pinctrl_data);
}


static int gxtvbb_pinctrl_remove(struct platform_device *pdev)
{
	return amlogic_pmx_remove(pdev);
}


static struct platform_driver gxtvbb_pinctrl_driver = {
	.driver = {
		.name = "pinmux-gxtvbb",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gxtvbb_pinctrl_of_table),
	},
	.probe = gxtvbb_pinctrl_probe,
	.remove = gxtvbb_pinctrl_remove,
};


static int __init gxtvbb_pinctrl_init(void)
{
	return platform_driver_register(&gxtvbb_pinctrl_driver);
}


static void __exit gxtvbb_pinctrl_exit(void)
{
	platform_driver_unregister(&gxtvbb_pinctrl_driver);
}


arch_initcall(gxtvbb_pinctrl_init);
module_exit(gxtvbb_pinctrl_exit);
MODULE_DESCRIPTION("gxtvbb pin control driver");
MODULE_LICENSE("GPL v2");
