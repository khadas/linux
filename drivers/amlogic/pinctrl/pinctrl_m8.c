/*
 * drivers/amlogic/pinctrl/pinctrl_m8.c
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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/amlogic/pinctrl_amlogic.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_address.h>
#include <linux/amlogic/gpio-amlogic.h>
#include "pinconf-amlogic.h"
#include <dt-bindings/gpio/meson8.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#define EE_OFFSET 14
#define MESON_PIN(x, n) PINCTRL_PIN((x+EE_OFFSET), n)
/* Pad names for the pinmux subsystem */
#define	PIN_GPIOZ_0    (GPIOZ_0 + EE_OFFSET)
#define	PIN_GPIOZ_1    (GPIOZ_1 + EE_OFFSET)
#define	PIN_GPIOZ_2    (GPIOZ_2 + EE_OFFSET)
#define	PIN_GPIOZ_3    (GPIOZ_3 + EE_OFFSET)
#define	PIN_GPIOZ_4    (GPIOZ_4 + EE_OFFSET)
#define	PIN_GPIOZ_5    (GPIOZ_5 + EE_OFFSET)
#define	PIN_GPIOZ_6    (GPIOZ_6 + EE_OFFSET)
#define	PIN_GPIOZ_7    (GPIOZ_7 + EE_OFFSET)
#define	PIN_GPIOZ_8    (GPIOZ_8 + EE_OFFSET)
#define	PIN_GPIOZ_9    (GPIOZ_9 + EE_OFFSET)
#define	PIN_GPIOZ_10    (GPIOZ_10 + EE_OFFSET)
#define	PIN_GPIOZ_11    (GPIOZ_11 + EE_OFFSET)
#define	PIN_GPIOZ_12    (GPIOZ_12 + EE_OFFSET)
#define	PIN_GPIOZ_13    (GPIOZ_13 + EE_OFFSET)
#define	PIN_GPIOZ_14    (GPIOZ_14 + EE_OFFSET)
#define	PIN_GPIOH_0    (GPIOH_0 + EE_OFFSET)
#define	PIN_GPIOH_1    (GPIOH_1 + EE_OFFSET)
#define	PIN_GPIOH_2    (GPIOH_2 + EE_OFFSET)
#define	PIN_GPIOH_3    (GPIOH_3 + EE_OFFSET)
#define	PIN_GPIOH_4    (GPIOH_4 + EE_OFFSET)
#define	PIN_GPIOH_5    (GPIOH_5 + EE_OFFSET)
#define	PIN_GPIOH_6    (GPIOH_6 + EE_OFFSET)
#define	PIN_GPIOH_7    (GPIOH_7 + EE_OFFSET)
#define	PIN_GPIOH_8    (GPIOH_8 + EE_OFFSET)
#define	PIN_GPIOH_9    (GPIOH_9 + EE_OFFSET)
#define	PIN_BOOT_0    (BOOT_0 + EE_OFFSET)
#define	PIN_BOOT_1    (BOOT_1 + EE_OFFSET)
#define	PIN_BOOT_2    (BOOT_2 + EE_OFFSET)
#define	PIN_BOOT_3    (BOOT_3 + EE_OFFSET)
#define	PIN_BOOT_4    (BOOT_4 + EE_OFFSET)
#define	PIN_BOOT_5    (BOOT_5 + EE_OFFSET)
#define	PIN_BOOT_6    (BOOT_6 + EE_OFFSET)
#define	PIN_BOOT_7    (BOOT_7 + EE_OFFSET)
#define	PIN_BOOT_8    (BOOT_8 + EE_OFFSET)
#define	PIN_BOOT_9    (BOOT_9 + EE_OFFSET)
#define	PIN_BOOT_10    (BOOT_10 + EE_OFFSET)
#define	PIN_BOOT_11    (BOOT_11 + EE_OFFSET)
#define	PIN_BOOT_12    (BOOT_12 + EE_OFFSET)
#define	PIN_BOOT_13    (BOOT_13 + EE_OFFSET)
#define	PIN_BOOT_14    (BOOT_14 + EE_OFFSET)
#define	PIN_BOOT_15    (BOOT_15 + EE_OFFSET)
#define	PIN_BOOT_16    (BOOT_16 + EE_OFFSET)
#define	PIN_BOOT_17    (BOOT_17 + EE_OFFSET)
#define	PIN_BOOT_18    (BOOT_18 + EE_OFFSET)
#define	PIN_CARD_0    (CARD_0 + EE_OFFSET)
#define	PIN_CARD_1    (CARD_1 + EE_OFFSET)
#define	PIN_CARD_2    (CARD_2 + EE_OFFSET)
#define	PIN_CARD_3    (CARD_3 + EE_OFFSET)
#define	PIN_CARD_4    (CARD_4 + EE_OFFSET)
#define	PIN_CARD_5    (CARD_5 + EE_OFFSET)
#define	PIN_CARD_6    (CARD_6 + EE_OFFSET)
#define	PIN_GPIODV_0    (GPIODV_0 + EE_OFFSET)
#define	PIN_GPIODV_1    (GPIODV_1 + EE_OFFSET)
#define	PIN_GPIODV_2    (GPIODV_2 + EE_OFFSET)
#define	PIN_GPIODV_3    (GPIODV_3 + EE_OFFSET)
#define	PIN_GPIODV_4    (GPIODV_4 + EE_OFFSET)
#define	PIN_GPIODV_5    (GPIODV_5 + EE_OFFSET)
#define	PIN_GPIODV_6    (GPIODV_6 + EE_OFFSET)
#define	PIN_GPIODV_7    (GPIODV_7 + EE_OFFSET)
#define	PIN_GPIODV_8    (GPIODV_8 + EE_OFFSET)
#define	PIN_GPIODV_9    (GPIODV_9 + EE_OFFSET)
#define	PIN_GPIODV_10    (GPIODV_10 + EE_OFFSET)
#define	PIN_GPIODV_11    (GPIODV_11 + EE_OFFSET)
#define	PIN_GPIODV_12    (GPIODV_12 + EE_OFFSET)
#define	PIN_GPIODV_13    (GPIODV_13 + EE_OFFSET)
#define	PIN_GPIODV_14    (GPIODV_14 + EE_OFFSET)
#define	PIN_GPIODV_15    (GPIODV_15 + EE_OFFSET)
#define	PIN_GPIODV_16    (GPIODV_16 + EE_OFFSET)
#define	PIN_GPIODV_17    (GPIODV_17 + EE_OFFSET)
#define	PIN_GPIODV_18    (GPIODV_18 + EE_OFFSET)
#define	PIN_GPIODV_19    (GPIODV_19 + EE_OFFSET)
#define	PIN_GPIODV_20    (GPIODV_20 + EE_OFFSET)
#define	PIN_GPIODV_21    (GPIODV_21 + EE_OFFSET)
#define	PIN_GPIODV_22    (GPIODV_22 + EE_OFFSET)
#define	PIN_GPIODV_23    (GPIODV_23 + EE_OFFSET)
#define	PIN_GPIODV_24    (GPIODV_24 + EE_OFFSET)
#define	PIN_GPIODV_25    (GPIODV_25 + EE_OFFSET)
#define	PIN_GPIODV_26    (GPIODV_26 + EE_OFFSET)
#define	PIN_GPIODV_27    (GPIODV_27 + EE_OFFSET)
#define	PIN_GPIODV_28    (GPIODV_28 + EE_OFFSET)
#define	PIN_GPIODV_29    (GPIODV_29 + EE_OFFSET)
#define	PIN_GPIOY_0    (GPIOY_0 + EE_OFFSET)
#define	PIN_GPIOY_1    (GPIOY_1 + EE_OFFSET)
#define	PIN_GPIOY_2    (GPIOY_2 + EE_OFFSET)
#define	PIN_GPIOY_3    (GPIOY_3 + EE_OFFSET)
#define	PIN_GPIOY_4    (GPIOY_4 + EE_OFFSET)
#define	PIN_GPIOY_5    (GPIOY_5 + EE_OFFSET)
#define	PIN_GPIOY_6    (GPIOY_6 + EE_OFFSET)
#define	PIN_GPIOY_7    (GPIOY_7 + EE_OFFSET)
#define	PIN_GPIOY_8    (GPIOY_8 + EE_OFFSET)
#define	PIN_GPIOY_9    (GPIOY_9 + EE_OFFSET)
#define	PIN_GPIOY_10    (GPIOY_10 + EE_OFFSET)
#define	PIN_GPIOY_11    (GPIOY_11 + EE_OFFSET)
#define	PIN_GPIOY_12    (GPIOY_12 + EE_OFFSET)
#define	PIN_GPIOY_13    (GPIOY_13 + EE_OFFSET)
#define	PIN_GPIOY_14    (GPIOY_14 + EE_OFFSET)
#define	PIN_GPIOY_15    (GPIOY_15 + EE_OFFSET)
#define	PIN_GPIOY_16    (GPIOY_16 + EE_OFFSET)
#define	PIN_GPIOX_0    (GPIOX_0 + EE_OFFSET)
#define	PIN_GPIOX_1    (GPIOX_1 + EE_OFFSET)
#define	PIN_GPIOX_2    (GPIOX_2 + EE_OFFSET)
#define	PIN_GPIOX_3    (GPIOX_3 + EE_OFFSET)
#define	PIN_GPIOX_4    (GPIOX_4 + EE_OFFSET)
#define	PIN_GPIOX_5    (GPIOX_5 + EE_OFFSET)
#define	PIN_GPIOX_6    (GPIOX_6 + EE_OFFSET)
#define	PIN_GPIOX_7    (GPIOX_7 + EE_OFFSET)
#define	PIN_GPIOX_8    (GPIOX_8 + EE_OFFSET)
#define	PIN_GPIOX_9    (GPIOX_9 + EE_OFFSET)
#define	PIN_GPIOX_10    (GPIOX_10 + EE_OFFSET)
#define	PIN_GPIOX_11    (GPIOX_11 + EE_OFFSET)
#define	PIN_GPIOX_12    (GPIOX_12 + EE_OFFSET)
#define	PIN_GPIOX_13    (GPIOX_13 + EE_OFFSET)
#define	PIN_GPIOX_14    (GPIOX_14 + EE_OFFSET)
#define	PIN_GPIOX_15    (GPIOX_15 + EE_OFFSET)
#define	PIN_GPIOX_16    (GPIOX_16 + EE_OFFSET)
#define	PIN_GPIOX_17    (GPIOX_17 + EE_OFFSET)
#define	PIN_GPIOX_18    (GPIOX_18 + EE_OFFSET)
#define	PIN_GPIOX_19    (GPIOX_19 + EE_OFFSET)
#define	PIN_GPIOX_20    (GPIOX_20 + EE_OFFSET)
#define	PIN_GPIOX_21    (GPIOX_21 + EE_OFFSET)
#define	PIN_GPIO_BSD_EN    (GPIO_BSD_EN + EE_OFFSET)
#define	PIN_GPIO_TEST_N    (GPIO_TEST_N + EE_OFFSET)


struct pinctrl_pin_desc m8_pads[] = {
	PINCTRL_PIN(GPIOAO_0, "GPIOAO_0"),
	PINCTRL_PIN(GPIOAO_1, "GPIOAO_1"),
	PINCTRL_PIN(GPIOAO_2, "GPIOAO_2"),
	PINCTRL_PIN(GPIOAO_3, "GPIOAO_3"),
	PINCTRL_PIN(GPIOAO_4, "GPIOAO_4"),
	PINCTRL_PIN(GPIOAO_5, "GPIOAO_5"),
	PINCTRL_PIN(GPIOAO_6, "GPIOAO_6"),
	PINCTRL_PIN(GPIOAO_7, "GPIOAO_7"),
	PINCTRL_PIN(GPIOAO_8, "GPIOAO_8"),
	PINCTRL_PIN(GPIOAO_9, "GPIOAO_9"),
	PINCTRL_PIN(GPIOAO_10, "GPIOAO_10"),
	PINCTRL_PIN(GPIOAO_11, "GPIOAO_11"),
	PINCTRL_PIN(GPIOAO_12, "GPIOAO_12"),
	PINCTRL_PIN(GPIOAO_13, "GPIOAO_13"),
	MESON_PIN(GPIOZ_0, "GPIOZ_0"),
	MESON_PIN(GPIOZ_1, "GPIOZ_1"),
	MESON_PIN(GPIOZ_2, "GPIOZ_2"),
	MESON_PIN(GPIOZ_3, "GPIOZ_3"),
	MESON_PIN(GPIOZ_4, "GPIOZ_4"),
	MESON_PIN(GPIOZ_5, "GPIOZ_5"),
	MESON_PIN(GPIOZ_6, "GPIOZ_6"),
	MESON_PIN(GPIOZ_7, "GPIOZ_7"),
	MESON_PIN(GPIOZ_8, "GPIOZ_8"),
	MESON_PIN(GPIOZ_9, "GPIOZ_9"),
	MESON_PIN(GPIOZ_10, "GPIOZ_10"),
	MESON_PIN(GPIOZ_11, "GPIOZ_11"),
	MESON_PIN(GPIOZ_12, "GPIOZ_12"),
	MESON_PIN(GPIOZ_13, "GPIOZ_13"),
	MESON_PIN(GPIOZ_14, "GPIOZ_14"),
	MESON_PIN(GPIOH_0, "GPIOH_0"),
	MESON_PIN(GPIOH_1, "GPIOH_1"),
	MESON_PIN(GPIOH_2, "GPIOH_2"),
	MESON_PIN(GPIOH_3, "GPIOH_3"),
	MESON_PIN(GPIOH_4, "GPIOH_4"),
	MESON_PIN(GPIOH_5, "GPIOH_5"),
	MESON_PIN(GPIOH_6, "GPIOH_6"),
	MESON_PIN(GPIOH_7, "GPIOH_7"),
	MESON_PIN(GPIOH_8, "GPIOH_8"),
	MESON_PIN(GPIOH_9, "GPIOH_9"),
	MESON_PIN(BOOT_0, "BOOT_0"),
	MESON_PIN(BOOT_1, "BOOT_1"),
	MESON_PIN(BOOT_2, "BOOT_2"),
	MESON_PIN(BOOT_3, "BOOT_3"),
	MESON_PIN(BOOT_4, "BOOT_4"),
	MESON_PIN(BOOT_5, "BOOT_5"),
	MESON_PIN(BOOT_6, "BOOT_6"),
	MESON_PIN(BOOT_7, "BOOT_7"),
	MESON_PIN(BOOT_8, "BOOT_8"),
	MESON_PIN(BOOT_9, "BOOT_9"),
	MESON_PIN(BOOT_10, "BOOT_10"),
	MESON_PIN(BOOT_11, "BOOT_11"),
	MESON_PIN(BOOT_12, "BOOT_12"),
	MESON_PIN(BOOT_13, "BOOT_13"),
	MESON_PIN(BOOT_14, "BOOT_14"),
	MESON_PIN(BOOT_15, "BOOT_15"),
	MESON_PIN(BOOT_16, "BOOT_16"),
	MESON_PIN(BOOT_17, "BOOT_17"),
	MESON_PIN(BOOT_18, "BOOT_18"),
	MESON_PIN(CARD_0, "CARD_0"),
	MESON_PIN(CARD_1, "CARD_1"),
	MESON_PIN(CARD_2, "CARD_2"),
	MESON_PIN(CARD_3, "CARD_3"),
	MESON_PIN(CARD_4, "CARD_4"),
	MESON_PIN(CARD_5, "CARD_5"),
	MESON_PIN(CARD_6, "CARD_6"),
	MESON_PIN(GPIODV_0, "GPIODV_0"),
	MESON_PIN(GPIODV_1, "GPIODV_1"),
	MESON_PIN(GPIODV_2, "GPIODV_2"),
	MESON_PIN(GPIODV_3, "GPIODV_3"),
	MESON_PIN(GPIODV_4, "GPIODV_4"),
	MESON_PIN(GPIODV_5, "GPIODV_5"),
	MESON_PIN(GPIODV_6, "GPIODV_6"),
	MESON_PIN(GPIODV_7, "GPIODV_7"),
	MESON_PIN(GPIODV_8, "GPIODV_8"),
	MESON_PIN(GPIODV_9, "GPIODV_9"),
	MESON_PIN(GPIODV_10, "GPIODV_10"),
	MESON_PIN(GPIODV_11, "GPIODV_11"),
	MESON_PIN(GPIODV_12, "GPIODV_12"),
	MESON_PIN(GPIODV_13, "GPIODV_13"),
	MESON_PIN(GPIODV_14, "GPIODV_14"),
	MESON_PIN(GPIODV_15, "GPIODV_15"),
	MESON_PIN(GPIODV_16, "GPIODV_16"),
	MESON_PIN(GPIODV_17, "GPIODV_17"),
	MESON_PIN(GPIODV_18, "GPIODV_18"),
	MESON_PIN(GPIODV_19, "GPIODV_19"),
	MESON_PIN(GPIODV_20, "GPIODV_20"),
	MESON_PIN(GPIODV_21, "GPIODV_21"),
	MESON_PIN(GPIODV_22, "GPIODV_22"),
	MESON_PIN(GPIODV_23, "GPIODV_23"),
	MESON_PIN(GPIODV_24, "GPIODV_24"),
	MESON_PIN(GPIODV_25, "GPIODV_25"),
	MESON_PIN(GPIODV_26, "GPIODV_26"),
	MESON_PIN(GPIODV_27, "GPIODV_27"),
	MESON_PIN(GPIODV_28, "GPIODV_28"),
	MESON_PIN(GPIODV_29, "GPIODV_29"),
	MESON_PIN(GPIOY_0, "GPIOY_0"),
	MESON_PIN(GPIOY_1, "GPIOY_1"),
	MESON_PIN(GPIOY_2, "GPIOY_2"),
	MESON_PIN(GPIOY_3, "GPIOY_3"),
	MESON_PIN(GPIOY_4, "GPIOY_4"),
	MESON_PIN(GPIOY_5, "GPIOY_5"),
	MESON_PIN(GPIOY_6, "GPIOY_6"),
	MESON_PIN(GPIOY_7, "GPIOY_7"),
	MESON_PIN(GPIOY_8, "GPIOY_8"),
	MESON_PIN(GPIOY_9, "GPIOY_9"),
	MESON_PIN(GPIOY_10, "GPIOY_10"),
	MESON_PIN(GPIOY_11, "GPIOY_11"),
	MESON_PIN(GPIOY_12, "GPIOY_12"),
	MESON_PIN(GPIOY_13, "GPIOY_13"),
	MESON_PIN(GPIOY_14, "GPIOY_14"),
	MESON_PIN(GPIOY_15, "GPIOY_15"),
	MESON_PIN(GPIOY_16, "GPIOY_16"),
	MESON_PIN(GPIOX_0, "GPIOX_0"),
	MESON_PIN(GPIOX_1, "GPIOX_1"),
	MESON_PIN(GPIOX_2, "GPIOX_2"),
	MESON_PIN(GPIOX_3, "GPIOX_3"),
	MESON_PIN(GPIOX_4, "GPIOX_4"),
	MESON_PIN(GPIOX_5, "GPIOX_5"),
	MESON_PIN(GPIOX_6, "GPIOX_6"),
	MESON_PIN(GPIOX_7, "GPIOX_7"),
	MESON_PIN(GPIOX_8, "GPIOX_8"),
	MESON_PIN(GPIOX_9, "GPIOX_9"),
	MESON_PIN(GPIOX_10, "GPIOX_10"),
	MESON_PIN(GPIOX_11, "GPIOX_11"),
	MESON_PIN(GPIOX_12, "GPIOX_12"),
	MESON_PIN(GPIOX_13, "GPIOX_13"),
	MESON_PIN(GPIOX_14, "GPIOX_14"),
	MESON_PIN(GPIOX_15, "GPIOX_15"),
	MESON_PIN(GPIOX_16, "GPIOX_16"),
	MESON_PIN(GPIOX_17, "GPIOX_17"),
	MESON_PIN(GPIOX_18, "GPIOX_18"),
	MESON_PIN(GPIOX_19, "GPIOX_19"),
	MESON_PIN(GPIOX_20, "GPIOX_20"),
	MESON_PIN(GPIOX_21, "GPIOX_21"),
	MESON_PIN(GPIO_BSD_EN, "GPIO_BSD_EN"),
	MESON_PIN(GPIO_TEST_N, "GPIO_TEST_N"),
};
static struct meson_bank meson8_banks[] = {
	/*   name    first         last
	 *   pullen  pull     dir     out     in  */
	BANK("X",    PIN_GPIOX_0,  PIN_GPIOX_21,
	     4,  0,  4,  0,  0,  0,  1,  0,  2,  0),
	BANK("Y",    PIN_GPIOY_0,  PIN_GPIOY_16,
	     3,  0,  3,  0,  3,  0,  4,  0,  5,  0),
	BANK("DV",  PIN_GPIODV_0, PIN_GPIODV_29,
	     0,  0,  0,  0,  7,  0,  8,  0,  9,  0),
	BANK("H",    PIN_GPIOH_0,  PIN_GPIOH_9,
	     1, 16,  1, 16,  9, 19, 10, 19, 11, 19),
	BANK("Z",    PIN_GPIOZ_0,  PIN_GPIOZ_14,
	     1,  0,  1,  0,  3, 17,  4, 17,  5, 17),
	BANK("CARD", PIN_CARD_0,   PIN_CARD_6,
	     2, 20,  2, 20,  0, 22,  1, 22,  2, 22),
	BANK("BOOT", PIN_BOOT_0,   PIN_BOOT_18,
	     2,  0,  2,  0,  9,  0, 10,  0, 11,  0),
};

static struct meson_bank meson8_ao_banks[] = {
	/*   name    first         last
	 *   pullen  pull     dir     out     in  */
	BANK("AO",   GPIOAO_0, GPIOAO_13,
	     0,  0,  0, 16,  0,  0,  0, 16,  1,  0),
};

static struct meson_domain_data meson8_domain_data[] = {
	{
		.name		= "banks",
		.banks		= meson8_banks,
		.num_banks	= ARRAY_SIZE(meson8_banks),
		.pin_base	= 14,
		.num_pins	= 122,
	},
	{
		.name		= "ao-bank",
		.banks		= meson8_ao_banks,
		.num_banks	= ARRAY_SIZE(meson8_ao_banks),
		.pin_base	= 0,
		.num_pins	= 14,
	},

};
#define NE 0xffffffff
#define PK(reg, bit) ((reg<<5)|bit)
static unsigned int gpio_to_pin[][5] = {
	[GPIOAO_0] = {PK(AO, 26), PK(AO, 12), NE, NE, NE,},
	[GPIOAO_1] = {PK(AO, 25), PK(AO, 11), NE, NE, NE,},
	[GPIOAO_2] = {PK(AO, AO), NE, NE, NE, NE,},
	[GPIOAO_3] = {PK(AO, 9), NE, NE, NE, NE,},
	[GPIOAO_4] = {PK(AO, 2), PK(AO, 24), PK(AO, 6), NE, NE,},
	[GPIOAO_5] = {PK(AO, 23), PK(AO, 5), PK(AO, 1), NE, NE,},
	[GPIOAO_6] = {PK(AO, 18), NE, NE, NE, NE,},
	[GPIOAO_7] = {PK(AO, 0), NE, NE, NE, NE,},
	[GPIOAO_8] = {PK(AO, 30), NE, NE, NE, NE,},
	[GPIOAO_9] = {PK(AO, 29), NE, NE, NE, NE,},
	[GPIOAO_10] = {PK(AO, 28), NE, NE, NE, NE,},
	[GPIOAO_11] = {PK(AO, 27), NE, NE, NE, NE,},
	[GPIOAO_12] = {PK(AO, 17), NE, NE, NE, NE,},
	[GPIOAO_13] = {PK(AO, 31), NE, NE, NE, NE,},
	[PIN_GPIOZ_0] = {PK(5, 31), PK(9, 18), PK(7, 25), PK(9, 16), NE,},
	[PIN_GPIOZ_1] = {PK(9, 15), PK(5, 30), PK(7, 24), NE, NE,},
	[PIN_GPIOZ_2] = {PK(5, 27), NE, NE, NE, NE,},
	[PIN_GPIOZ_3] = {PK(5, 26), NE, NE, NE, NE,},
	[PIN_GPIOZ_4] = {PK(5, 25), PK(6, 15), NE, NE, NE,},
	[PIN_GPIOZ_5] = {PK(5, 24), PK(6, 14), NE, NE, NE,},
	[PIN_GPIOZ_6] = {PK(6, 13), PK(3, 21), PK(3, 21), NE, NE,},
	[PIN_GPIOZ_7] = {PK(6, 12), PK(2, 0), PK(7, 23), NE, NE,},
	[PIN_GPIOZ_8] = {PK(2, 1), PK(6, 9), PK(6, 10), PK(7, 22), NE,},
	[PIN_GPIOZ_9] = {PK(5, 9), PK(6, 11), PK(8, 16), NE, NE,},
	[PIN_GPIOZ_10] = {PK(6, 8), PK(5, 8), PK(8, 12), NE, NE,},
	[PIN_GPIOZ_11] = {PK(6, 7), PK(8, 15), PK(5, 7), NE, NE,},
	[PIN_GPIOZ_12] = {PK(6, 6), PK(5, 6), PK(8, 14), NE, NE,},
	[PIN_GPIOZ_13] = {PK(6, 5), PK(8, 13), NE, NE, NE,},
	[PIN_GPIOZ_14] = {PK(9, 17), PK(8, 17), NE, NE, NE,},
	[PIN_GPIOH_0] = {PK(1, 26), NE, NE, NE, NE,},
	[PIN_GPIOH_1] = {PK(1, 25), NE, NE, NE, NE,},
	[PIN_GPIOH_2] = {PK(1, 24), NE, NE, NE, NE,},
	[PIN_GPIOH_3] = {PK(9, 13), PK(1, 23), NE, NE, NE,},
	[PIN_GPIOH_4] = {PK(9, 12), NE, NE, NE, NE,},
	[PIN_GPIOH_5] = {PK(9, 11), NE, NE, NE, NE,},
	[PIN_GPIOH_6] = {PK(9, 10), NE, NE, NE, NE,},
	[PIN_GPIOH_7] = {PK(4, 3), NE, NE, NE, NE,},
	[PIN_GPIOH_8] = {PK(4, 2), NE, NE, NE, NE,},
	[PIN_GPIOH_9] = {PK(4, 1), NE, NE, NE, NE,},
	[PIN_BOOT_0] = {PK(4, 30), PK(2, 26), PK(6, 29), NE, NE,},
	[PIN_BOOT_1] = {PK(4, 29), PK(2, 26), PK(6, 28), NE, NE,},
	[PIN_BOOT_2] = {PK(2, 26), PK(6, 27), PK(4, 29), NE, NE,},
	[PIN_BOOT_3] = {PK(4, 29), PK(2, 26), PK(6, 26), NE, NE,},
	[PIN_BOOT_4] = {PK(2, 26), PK(4, 28), NE, NE, NE,},
	[PIN_BOOT_5] = {PK(2, 26), PK(4, 28), NE, NE, NE,},
	[PIN_BOOT_6] = {PK(2, 26), PK(4, 28), NE, NE, NE,},
	[PIN_BOOT_7] = {PK(2, 26), PK(4, 28), NE, NE, NE,},
	[PIN_BOOT_8] = {PK(2, 25), NE, NE, NE, NE,},
	[PIN_BOOT_9] = {PK(2, 24), NE, NE, NE, NE,},
	[PIN_BOOT_10] = {PK(2, 17), NE, NE, NE, NE,},
	[PIN_BOOT_11] = {PK(2, 21), PK(5, 1), NE, NE, NE,},
	[PIN_BOOT_12] = {PK(2, 20), PK(5, 3), NE, NE, NE,},
	[PIN_BOOT_13] = {PK(2, 19), PK(5, 2), NE, NE, NE,},
	[PIN_BOOT_14] = {PK(2, 18), NE, NE, NE, NE,},
	[PIN_BOOT_15] = {PK(2, 27), NE, NE, NE, NE,},
	[PIN_BOOT_16] = {PK(6, 25), PK(2, 23), PK(4, 27), NE, NE,},
	[PIN_BOOT_17] = {PK(2, 22), PK(4, 26), PK(6, 24), NE, NE,},
	[PIN_BOOT_18] = {PK(5, 0), NE, NE, NE, NE,},
	[PIN_CARD_0] = {PK(2, 14), PK(2, 6), NE, NE, NE,},
	[PIN_CARD_1] = {PK(2, 7), PK(2, 15), NE, NE, NE,},
	[PIN_CARD_2] = {PK(2, 11), PK(2, 5), NE, NE, NE,},
	[PIN_CARD_3] = {PK(2, 4), PK(2, 10), NE, NE, NE,},
	[PIN_CARD_4] = {PK(2, 6), PK(2, 12), PK(8, 10), NE, NE,},
	[PIN_CARD_5] = {PK(2, 13), PK(8, 9), PK(2, 6), NE, NE,},
	[PIN_CARD_6] = {NE, NE, NE, NE, NE,},
	[PIN_GPIODV_0] = {PK(8, 27), PK(7, 0), PK(0, 1), PK(0, 6), NE,},
	[PIN_GPIODV_1] = {PK(0, 6), PK(7, 1), PK(0, 1), NE, NE,},
	[PIN_GPIODV_2] = {PK(0, 6), PK(7, 2), PK(0, 0), NE, NE,},
	[PIN_GPIODV_3] = {PK(0, 0), PK(0, 6), PK(7, 3), NE, NE,},
	[PIN_GPIODV_4] = {PK(0, 0), PK(0, 6), PK(7, 4), NE, NE,},
	[PIN_GPIODV_5] = {PK(7, 5), PK(0, 6), PK(0, 0), NE, NE,},
	[PIN_GPIODV_6] = {PK(0, 6), PK(7, 6), PK(0, 0), NE, NE,},
	[PIN_GPIODV_7] = {PK(0, 0), PK(0, 6), PK(7, 7), NE, NE,},
	[PIN_GPIODV_8] = {PK(8, 26), PK(0, 3), PK(7, 8), PK(0, 6), NE,},
	[PIN_GPIODV_9] = {PK(7, 28), PK(0, 6), PK(7, 9), PK(0, 3), PK(3, 24),},
	[PIN_GPIODV_10] = {PK(7, 10), PK(0, 6), PK(0, 2), NE, NE,},
	[PIN_GPIODV_11] = {PK(7, 11), PK(0, 2), PK(0, 6), NE, NE,},
	[PIN_GPIODV_12] = {PK(0, 2), PK(7, 12), PK(0, 6), NE, NE,},
	[PIN_GPIODV_13] = {PK(0, 6), PK(7, 13), PK(0, 2), NE, NE,},
	[PIN_GPIODV_14] = {PK(7, 14), PK(0, 2), PK(0, 6), NE, NE,},
	[PIN_GPIODV_15] = {PK(7, 15), PK(0, 2), PK(0, 6), NE, NE,},
	[PIN_GPIODV_16] = {PK(0, 5), PK(0, 6), PK(8, 25), PK(7, 16), NE,},
	[PIN_GPIODV_17] = {PK(7, 17), PK(0, 6), PK(0, 5), NE, NE,},
	[PIN_GPIODV_18] = {PK(0, 4), PK(0, 6), NE, NE, NE,},
	[PIN_GPIODV_19] = {PK(0, 4), PK(0, 6), NE, NE, NE,},
	[PIN_GPIODV_20] = {PK(0, 4), PK(0, 6), NE, NE, NE,},
	[PIN_GPIODV_21] = {PK(0, 4), PK(0, 6), NE, NE, NE,},
	[PIN_GPIODV_22] = {PK(0, 4), PK(0, 6), NE, NE, NE,},
	[PIN_GPIODV_23] = {PK(0, 4), PK(0, 6), NE, NE, NE,},
	[PIN_GPIODV_24] = {PK(0, 9), PK(0, 19), PK(0, 21), PK(6, 23),
		PK(8, 24),},
	[PIN_GPIODV_25] = {PK(6, 22), PK(0, 20), PK(0, 8), PK(8, 23),
		PK(0, 18),},
	[PIN_GPIODV_26] = {PK(6, 21), PK(8, 21), PK(0, 7), PK(8, 22),
		PK(8, 20),},
	[PIN_GPIODV_27] = {PK(0, 10), PK(6, 20), PK(8, 28), PK(8, 19), NE,},
	[PIN_GPIODV_28] = {PK(7, 27), PK(3, 26), NE, NE, NE,},
	[PIN_GPIODV_29] = {PK(7, 26), PK(3, 25), NE, NE, NE,},
	[PIN_GPIOY_0] = {PK(1, 19), PK(1, 10), PK(3, 2), PK(9, 9), PK(1, 15),},
	[PIN_GPIOY_1] = {PK(9, 8), PK(3, 1), PK(1, 18), PK(1, 14), PK(1, 19),},
	[PIN_GPIOY_2] = {PK(3, 18), PK(1, 8), PK(1, 17), NE, NE,},
	[PIN_GPIOY_3] = {PK(1, 16), PK(1, 7), NE, NE, NE,},
	[PIN_GPIOY_4] = {PK(4, 25), PK(3, 3), PK(1, 6), NE, NE,},
	[PIN_GPIOY_5] = {PK(4, 24), PK(3, 15), PK(1, 5), PK(9, 7), NE,},
	[PIN_GPIOY_6] = {PK(4, 23), PK(3, 16), PK(1, 3), PK(1, 4), PK(9, 6),},
	[PIN_GPIOY_7] = {PK(4, 22), PK(1, 2), PK(9, 5), PK(1, 1), PK(3, 17),},
	[PIN_GPIOY_8] = {PK(3, 0), PK(1, 0), PK(9, 4), NE, NE,},
	[PIN_GPIOY_9] = {PK(1, 11), PK(9, 3), PK(3, 4), NE, NE,},
	[PIN_GPIOY_10] = {PK(3, 5), PK(9, 3), NE, NE, NE,},
	[PIN_GPIOY_11] = {PK(9, 3), PK(3, 5), NE, NE, NE,},
	[PIN_GPIOY_12] = {PK(9, 3), PK(3, 5), NE, NE, NE,},
	[PIN_GPIOY_13] = {PK(3, 5), PK(9, 3), NE, NE, NE,},
	[PIN_GPIOY_14] = {PK(9, 2), PK(3, 5), NE, NE, NE,},
	[PIN_GPIOY_15] = {PK(9, 1), PK(3, 5), NE, NE, NE,},
	[PIN_GPIOY_16] = {PK(9, 0), PK(3, 5), PK(9, 14), PK(7, 29), NE,},
	[PIN_GPIOX_0] = {PK(8, 5), PK(5, 14), NE, NE, NE,},
	[PIN_GPIOX_1] = {PK(5, 13), PK(8, 4), NE, NE, NE,},
	[PIN_GPIOX_2] = {PK(8, 3), PK(5, 13), NE, NE, NE,},
	[PIN_GPIOX_3] = {PK(8, 2), PK(5, 13), NE, NE, NE,},
	[PIN_GPIOX_4] = {PK(5, 12), PK(3, 30), PK(4, 17), NE, NE,},
	[PIN_GPIOX_5] = {PK(3, 29), PK(5, 12), PK(4, 16), NE, NE,},
	[PIN_GPIOX_6] = {PK(5, 12), PK(4, 15), PK(3, 28), NE, NE,},
	[PIN_GPIOX_7] = {PK(5, 12), PK(4, 14), PK(3, 27), NE, NE,},
	[PIN_GPIOX_8] = {PK(8, 1), PK(5, 11), NE, NE, NE,},
	[PIN_GPIOX_9] = {PK(5, 10), PK(8, 0), NE, NE, NE,},
	[PIN_GPIOX_10] = {PK(7, 31), PK(3, 22), PK(9, 19), NE, NE,},
	[PIN_GPIOX_11] = {PK(3, 14), PK(7, 30), PK(2, 3), PK(3, 23),
		PK(3, 23),},
	[PIN_GPIOX_12] = {PK(3, 13), PK(4, 13), NE, NE, NE,},
	[PIN_GPIOX_13] = {PK(4, 12), PK(3, 12), NE, NE, NE,},
	[PIN_GPIOX_14] = {PK(3, 12), PK(4, 11), NE, NE, NE,},
	[PIN_GPIOX_15] = {PK(3, 12), PK(4, 10), NE, NE, NE,},
	[PIN_GPIOX_16] = {PK(4, 9), PK(4, 21), PK(3, 12), PK(4, 5), NE,},
	[PIN_GPIOX_17] = {PK(3, 12), PK(4, 8), PK(4, 4), PK(4, 20), NE,},
	[PIN_GPIOX_18] = {PK(4, 7), PK(4, 19), PK(3, 12), NE, NE,},
	[PIN_GPIOX_19] = {PK(4, 18), PK(4, 6), PK(3, 12), NE, NE,},
	[PIN_GPIOX_20] = {NE, NE, NE, NE, NE,},
	[PIN_GPIOX_21] = {NE, NE, NE, NE, NE,},
	[PIN_GPIO_BSD_EN] = {NE, NE, NE, NE, NE,},
	[PIN_GPIO_TEST_N] = {PK(10, 19), NE, NE, NE, NE,},

};
static unsigned int gpio_to_pin_m8m2[][5] = {
	[PIN_GPIOZ_0] = {PK(5, 31), PK(9, 18), PK(7, 25), PK(9, 16), PK(6, 0)},
	[PIN_GPIOZ_1] = {PK(9, 15), PK(5, 30), PK(7, 24), PK(6, 1), NE},
	[PIN_GPIOZ_2] = {PK(5, 27), PK(6, 2), NE, NE, NE},
	[PIN_GPIOZ_3] = {PK(5, 26), PK(6, 3), NE, NE, NE},
	[PIN_GPIOH_9] = {PK(4, 1), PK(3, 19), NE, NE, NE},
	[PIN_CARD_4] = {PK(2, 6), PK(2, 12), PK(8, 10), PK(8, 8), NE},
	[PIN_CARD_5] = {PK(2, 13), PK(8, 9), PK(2, 6), PK(8, 7), NE},
	[PIN_GPIODV_24] = {PK(0, 9), PK(0, 19), PK(6, 23), PK(8, 24), NE},
	[PIN_GPIODV_25] = {PK(6, 22), PK(0, 8), PK(8, 23), PK(0, 18), NE},
	[PIN_GPIOY_3] = {PK(1, 16), PK(1, 7), PK(3, 11), NE, NE},
};


int m8_gpio_name_to_num(const char *name)
{
	int i, tmp = 100, num = 0;
	int len = 0;
	char *p = NULL;
	char *start = NULL;
	if (!name)
		return -1;
	if (!strcmp(name, "GPIO_BSD_EN"))
		return GPIO_BSD_EN;
	if (!strcmp(name, "GPIO_TEST_N"))
		return GPIO_TEST_N;
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
	if (!strcmp(p, "GPIOAO"))
		num = num+0;
	else if (!strcmp(p, "GPIOZ"))
		num = num+14;
	else if (!strcmp(p, "GPIOH"))
		num = num+29;
	else if (!strcmp(p, "BOOT"))
		num = num+39;
	else if (!strcmp(p, "CARD"))
		num = num+58;
	else if (!strcmp(p, "GPIODV"))
		num = num+65;
	else if (!strcmp(p, "GPIOY"))
		num = num+95;
	else if (!strcmp(p, "GPIOX"))
		num = num+112;
	else
		num = -1;
	kzfree(start);
	return num;
}
static int  m8_clear_pinmux(struct amlogic_pmx *apmx, unsigned int pin)
{
	unsigned int *gpio_reg =  &gpio_to_pin[pin][0];
	int i, dom, reg, bit, ret;
	struct meson_domain *domain = NULL;
	for (i = 0;
	     i < sizeof(gpio_to_pin[pin])/sizeof(gpio_to_pin[pin][0]); i++) {
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
int m8_extern_gpio_output(struct meson_domain *domain, unsigned int pin,
						int value)
{
	if (PIN_GPIO_BSD_EN == pin) {
		int val;
		regmap_update_bits(domain->reg_gpio, (1*4), BIT(29), 0);
		val = aml_read_sec_reg(0xda004000);
		val = val | (BIT(1));
		aml_write_sec_reg(0xda004000, val);
		if (is_meson_m8_cpu())
			regmap_update_bits(domain->reg_gpio, (1*4),
					   BIT(31), value ? BIT(31) : 0);
		else
			regmap_update_bits(domain->reg_pull, (2*4),
					   BIT(1), value ? BIT(1) : 0);
		regmap_update_bits(domain->reg_gpio, (1*4), BIT(30), 0);
		return 0;
	}
	if (PIN_GPIO_TEST_N == pin) {
		int val;
		aml_aobus_update_bits(0x24, BIT(31), value ? BIT(31) : 0);
		val = aml_read_sec_reg(0xda004000);
		val = val | (BIT(1));
		aml_write_sec_reg(0xda004000, val);
		return 0;
	}
	return 1;
}
int m8_extern_gpio_get(struct meson_domain *domain, unsigned int pin)
{
	if (PIN_GPIO_BSD_EN == pin) {
		int val;
		if (is_meson_m8_cpu()) {
			regmap_read(domain->reg_gpio, (1*4), &val);
			return !!(val & BIT(31));
		} else{
			regmap_read(domain->reg_pull, (2*4), &val);
			return !!(val & BIT(1));
		}
	}
	if (PIN_GPIO_TEST_N == pin)
		return !!(aml_read_aobus(0x24) & BIT(31));

	return -1;
}

static struct amlogic_pinctrl_soc_data m8_pinctrl = {
	.pins = m8_pads,
	.npins = ARRAY_SIZE(m8_pads),
	.domain_data	= meson8_domain_data,
	.num_domains	= ARRAY_SIZE(meson8_domain_data),
	.meson_clear_pinmux = m8_clear_pinmux,
	.name_to_pin = m8_gpio_name_to_num,
	.soc_extern_gpio_output = m8_extern_gpio_output,
	.soc_extern_gpio_get = m8_extern_gpio_get,
};
static struct of_device_id m8_pinctrl_of_table[] = {
	{
		.compatible = "amlogic,pinmux-m8",
	},
	{},
};

static int  m8_pmx_probe(struct platform_device *pdev)
{
	if (is_meson_m8m2_cpu()) {
		memcpy(gpio_to_pin[GPIOZ_0],
			gpio_to_pin_m8m2[GPIOZ_0], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIOZ_1],
			gpio_to_pin_m8m2[GPIOZ_1], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIOZ_2],
			gpio_to_pin_m8m2[GPIOZ_2], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIOZ_3],
			gpio_to_pin_m8m2[GPIOZ_3], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIOH_9],
			gpio_to_pin_m8m2[GPIOH_9], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[CARD_4],
			gpio_to_pin_m8m2[CARD_4], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[CARD_5],
			gpio_to_pin_m8m2[CARD_5], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIODV_24],
			gpio_to_pin_m8m2[GPIODV_24], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIODV_25],
			gpio_to_pin_m8m2[GPIODV_25], 5*sizeof(unsigned int));
		memcpy(gpio_to_pin[GPIOY_3],
			gpio_to_pin_m8m2[GPIOY_3], 5*sizeof(unsigned int));
	}
	return amlogic_pmx_probe(pdev, &m8_pinctrl);
}

static int  m8_pmx_remove(struct platform_device *pdev)
{
	return amlogic_pmx_remove(pdev);
}

static struct platform_driver m8_pmx_driver = {
	.driver = {
		.name = "pinmux-m8",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(m8_pinctrl_of_table),
	},
	.probe = m8_pmx_probe,
	.remove = m8_pmx_remove,
};

static int __init m8_pmx_init(void)
{
	return platform_driver_register(&m8_pmx_driver);
}
arch_initcall(m8_pmx_init);

static void __exit m8_pmx_exit(void)
{
	platform_driver_unregister(&m8_pmx_driver);
}
module_exit(m8_pmx_exit);
MODULE_DESCRIPTION("m8 pin control driver");
MODULE_LICENSE("GPL v2");
