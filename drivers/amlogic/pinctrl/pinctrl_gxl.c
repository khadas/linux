/*
 * drivers/amlogic/pinctrl/pinctrl_gxl.c
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
#include <dt-bindings/gpio/gxl.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/uaccess.h>


#define EE_OFFSET 10
#define MESON_PIN(x, n) PINCTRL_PIN((x+EE_OFFSET), n)
/* Pad names for the pinmux subsystem */
#define	PIN_GPIOZ_0    (EE_OFFSET + GPIOZ_0)
#define	PIN_GPIOZ_1    (EE_OFFSET + GPIOZ_1)
#define	PIN_GPIOZ_2    (EE_OFFSET + GPIOZ_2)
#define	PIN_GPIOZ_3    (EE_OFFSET + GPIOZ_3)
#define	PIN_GPIOZ_4    (EE_OFFSET + GPIOZ_4)
#define	PIN_GPIOZ_5    (EE_OFFSET + GPIOZ_5)
#define	PIN_GPIOZ_6    (EE_OFFSET + GPIOZ_6)
#define	PIN_GPIOZ_7    (EE_OFFSET + GPIOZ_7)
#define	PIN_GPIOZ_8    (EE_OFFSET + GPIOZ_8)
#define	PIN_GPIOZ_9    (EE_OFFSET + GPIOZ_9)
#define	PIN_GPIOZ_10    (EE_OFFSET + GPIOZ_10)
#define	PIN_GPIOZ_11    (EE_OFFSET + GPIOZ_11)
#define	PIN_GPIOZ_12    (EE_OFFSET + GPIOZ_12)
#define	PIN_GPIOZ_13    (EE_OFFSET + GPIOZ_13)
#define	PIN_GPIOZ_14    (EE_OFFSET + GPIOZ_14)
#define	PIN_GPIOZ_15    (EE_OFFSET + GPIOZ_15)
#define	PIN_GPIOH_0    (EE_OFFSET + GPIOH_0)
#define	PIN_GPIOH_1    (EE_OFFSET + GPIOH_1)
#define	PIN_GPIOH_2    (EE_OFFSET + GPIOH_2)
#define	PIN_GPIOH_3    (EE_OFFSET + GPIOH_3)
#define	PIN_GPIOH_4    (EE_OFFSET + GPIOH_4)
#define	PIN_GPIOH_5    (EE_OFFSET + GPIOH_5)
#define	PIN_GPIOH_6    (EE_OFFSET + GPIOH_6)
#define	PIN_GPIOH_7    (EE_OFFSET + GPIOH_7)
#define	PIN_GPIOH_8    (EE_OFFSET + GPIOH_8)
#define	PIN_GPIOH_9    (EE_OFFSET + GPIOH_9)
#define	PIN_BOOT_0    (EE_OFFSET + BOOT_0)
#define	PIN_BOOT_1    (EE_OFFSET + BOOT_1)
#define	PIN_BOOT_2    (EE_OFFSET + BOOT_2)
#define	PIN_BOOT_3    (EE_OFFSET + BOOT_3)
#define	PIN_BOOT_4    (EE_OFFSET + BOOT_4)
#define	PIN_BOOT_5    (EE_OFFSET + BOOT_5)
#define	PIN_BOOT_6    (EE_OFFSET + BOOT_6)
#define	PIN_BOOT_7    (EE_OFFSET + BOOT_7)
#define	PIN_BOOT_8    (EE_OFFSET + BOOT_8)
#define	PIN_BOOT_9    (EE_OFFSET + BOOT_9)
#define	PIN_BOOT_10    (EE_OFFSET + BOOT_10)
#define	PIN_BOOT_11    (EE_OFFSET + BOOT_11)
#define	PIN_BOOT_12    (EE_OFFSET + BOOT_12)
#define	PIN_BOOT_13    (EE_OFFSET + BOOT_13)
#define	PIN_BOOT_14    (EE_OFFSET + BOOT_14)
#define	PIN_BOOT_15    (EE_OFFSET + BOOT_15)
#define	PIN_CARD_0    (EE_OFFSET + CARD_0)
#define	PIN_CARD_1    (EE_OFFSET + CARD_1)
#define	PIN_CARD_2    (EE_OFFSET + CARD_2)
#define	PIN_CARD_3    (EE_OFFSET + CARD_3)
#define	PIN_CARD_4    (EE_OFFSET + CARD_4)
#define	PIN_CARD_5    (EE_OFFSET + CARD_5)
#define	PIN_CARD_6    (EE_OFFSET + CARD_6)
#define	PIN_GPIODV_0    (EE_OFFSET + GPIODV_0)
#define	PIN_GPIODV_1    (EE_OFFSET + GPIODV_1)
#define	PIN_GPIODV_2    (EE_OFFSET + GPIODV_2)
#define	PIN_GPIODV_3    (EE_OFFSET + GPIODV_3)
#define	PIN_GPIODV_4    (EE_OFFSET + GPIODV_4)
#define	PIN_GPIODV_5    (EE_OFFSET + GPIODV_5)
#define	PIN_GPIODV_6    (EE_OFFSET + GPIODV_6)
#define	PIN_GPIODV_7    (EE_OFFSET + GPIODV_7)
#define	PIN_GPIODV_8    (EE_OFFSET + GPIODV_8)
#define	PIN_GPIODV_9    (EE_OFFSET + GPIODV_9)
#define	PIN_GPIODV_10    (EE_OFFSET + GPIODV_10)
#define	PIN_GPIODV_11    (EE_OFFSET + GPIODV_11)
#define	PIN_GPIODV_12    (EE_OFFSET + GPIODV_12)
#define	PIN_GPIODV_13    (EE_OFFSET + GPIODV_13)
#define	PIN_GPIODV_14    (EE_OFFSET + GPIODV_14)
#define	PIN_GPIODV_15    (EE_OFFSET + GPIODV_15)
#define	PIN_GPIODV_16    (EE_OFFSET + GPIODV_16)
#define	PIN_GPIODV_17    (EE_OFFSET + GPIODV_17)
#define	PIN_GPIODV_18    (EE_OFFSET + GPIODV_18)
#define	PIN_GPIODV_19    (EE_OFFSET + GPIODV_19)
#define	PIN_GPIODV_20    (EE_OFFSET + GPIODV_20)
#define	PIN_GPIODV_21    (EE_OFFSET + GPIODV_21)
#define	PIN_GPIODV_22    (EE_OFFSET + GPIODV_22)
#define	PIN_GPIODV_23    (EE_OFFSET + GPIODV_23)
#define	PIN_GPIODV_24    (EE_OFFSET + GPIODV_24)
#define	PIN_GPIODV_25    (EE_OFFSET + GPIODV_25)
#define	PIN_GPIODV_26    (EE_OFFSET + GPIODV_26)
#define	PIN_GPIODV_27    (EE_OFFSET + GPIODV_27)
#define	PIN_GPIODV_28    (EE_OFFSET + GPIODV_28)
#define	PIN_GPIODV_29    (EE_OFFSET + GPIODV_29)
#define	PIN_GPIOX_0    (EE_OFFSET + GPIOX_0)
#define	PIN_GPIOX_1    (EE_OFFSET + GPIOX_1)
#define	PIN_GPIOX_2    (EE_OFFSET + GPIOX_2)
#define	PIN_GPIOX_3    (EE_OFFSET + GPIOX_3)
#define	PIN_GPIOX_4    (EE_OFFSET + GPIOX_4)
#define	PIN_GPIOX_5    (EE_OFFSET + GPIOX_5)
#define	PIN_GPIOX_6    (EE_OFFSET + GPIOX_6)
#define	PIN_GPIOX_7    (EE_OFFSET + GPIOX_7)
#define	PIN_GPIOX_8    (EE_OFFSET + GPIOX_8)
#define	PIN_GPIOX_9    (EE_OFFSET + GPIOX_9)
#define	PIN_GPIOX_10    (EE_OFFSET + GPIOX_10)
#define	PIN_GPIOX_11    (EE_OFFSET + GPIOX_11)
#define	PIN_GPIOX_12    (EE_OFFSET + GPIOX_12)
#define	PIN_GPIOX_13    (EE_OFFSET + GPIOX_13)
#define	PIN_GPIOX_14    (EE_OFFSET + GPIOX_14)
#define	PIN_GPIOX_15    (EE_OFFSET + GPIOX_15)
#define	PIN_GPIOX_16    (EE_OFFSET + GPIOX_16)
#define	PIN_GPIOX_17    (EE_OFFSET + GPIOX_17)
#define	PIN_GPIOX_18    (EE_OFFSET + GPIOX_18)
#define	PIN_GPIOCLK_0    (EE_OFFSET + GPIOCLK_0)
#define	PIN_GPIOCLK_1    (EE_OFFSET + GPIOCLK_1)

#define	PIN_GPIO_TEST_N    (EE_OFFSET + GPIO_TEST_N)



struct pinctrl_pin_desc gxl_pads[] = {
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
	MESON_PIN(GPIOZ_15, "GPIOZ_15"),
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
	MESON_PIN(GPIOCLK_0, "GPIOCLK_0"),
	MESON_PIN(GPIOCLK_1, "GPIOCLK_1"),

	MESON_PIN(GPIO_TEST_N, "GPIO_TEST_N"),

};
static struct meson_bank mesongxl_banks[] = {
	/*   name    first         last
	 *   pullen  pull     dir     out     in  */
	BANK("X",    PIN_GPIOX_0,  PIN_GPIOX_18,
	     4,  0,  4,  0,  12,  0,  13,  0,  14,  0),
	BANK("DV",  PIN_GPIODV_0, PIN_GPIODV_29,
	     0,  0,  0,  0,  0,  0,  1,  0,  2,  0),
	BANK("H",    PIN_GPIOH_0,  PIN_GPIOH_9,
	     1, 20,  1, 20,  3, 20, 4, 20, 5, 20),
	BANK("Z",    PIN_GPIOZ_0,  PIN_GPIOZ_15,
	     3,  0,  3,  0,  9, 0,  10, 0,  11, 0),
	BANK("CARD", PIN_CARD_0,   PIN_CARD_6,
	     2, 20,  2, 20,  6, 20,  7, 20,  8, 20),
	BANK("BOOT", PIN_BOOT_0,   PIN_BOOT_15,
	     2,  0,  2,  0,  6,  0, 7,  0, 8,  0),
	BANK("CLK", PIN_GPIOCLK_0,   PIN_GPIOCLK_1,
	     3,  28,  3,  28,  9,  28, 10,  28, 11,  28),
};

static struct meson_bank mesongxl_ao_banks[] = {
	/*   name    first         last
	 *   pullen  pull     dir     out     in  */
	BANK("AO",   GPIOAO_0, GPIOAO_9,
	     0,  0,  0, 16,  0,  0,  0, 16,  1,  0),
};

static struct meson_domain_data mesongxl_domain_data[] = {
	{
		.name		= "banks",
		.banks		= mesongxl_banks,
		.num_banks	= ARRAY_SIZE(mesongxl_banks),
		.pin_base	= 10,
		.num_pins	= 101,
	},
	{
		.name		= "ao-bank",
		.banks		= mesongxl_ao_banks,
		.num_banks	= ARRAY_SIZE(mesongxl_ao_banks),
		.pin_base	= 0,
		.num_pins	= 10,
	},

};
#define NE 0xffffffff
#define PK(reg, bit) ((reg<<5)|bit)
static unsigned int gpio_to_pin[][6] = {
	[PIN_GPIOX_0] = {PK(5, 31), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_1] = {PK(5, 30), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_2] = {PK(5, 29), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_3] = {PK(5, 28), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_4] = {PK(5, 27), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_5] = {PK(5, 26), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_6] = {PK(5, 25), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_7] = {PK(5, 24), PK(5, 14), NE, NE, NE, NE,},
	[PIN_GPIOX_8] = {PK(5, 23), PK(5, 13), PK(5, 3), NE, NE, NE,},
	[PIN_GPIOX_9] = {PK(5, 22), PK(5, 12), PK(5, 2), NE, NE, NE,},
	[PIN_GPIOX_10] = {PK(5, 21), PK(5, 11), PK(5, 5), PK(5, 1), NE, NE,},
	[PIN_GPIOX_11] = {PK(5, 20), PK(5, 10), PK(5, 4), PK(5, 0), NE, NE,},
	[PIN_GPIOX_12] = {PK(5, 19), PK(5, 9), NE, NE, NE, NE,},
	[PIN_GPIOX_13] = {PK(5, 18), PK(5, 8), NE, NE, NE, NE,},
	[PIN_GPIOX_14] = {PK(5, 17), PK(5, 7), NE, NE, NE, NE,},
	[PIN_GPIOX_15] = {PK(5, 16), PK(5, 6), NE, NE, NE, NE,},
	[PIN_GPIOX_16] = {PK(5, 15), NE, NE, NE, NE, NE,},
	[PIN_GPIOX_17] = {NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOX_18] = {NE, NE, NE, NE, NE, NE,},

	[PIN_BOOT_0] = {PK(7, 31), NE, NE, NE, NE, NE,},
	[PIN_BOOT_1] = {PK(7, 31), NE, NE, NE, NE, NE,},
	[PIN_BOOT_2] = {PK(7, 31), NE, NE, NE, NE, NE,},
	[PIN_BOOT_3] = {PK(7, 23), PK(7, 31), NE, NE, NE, NE,},
	[PIN_BOOT_4] = {PK(7, 22), PK(7, 31), NE, NE, NE, NE,},
	[PIN_BOOT_5] = {PK(7, 21), PK(7, 31), NE, NE, NE, NE,},
	[PIN_BOOT_6] = {PK(7, 20), PK(7, 31), NE, NE, NE, NE,},
	[PIN_BOOT_7] = {PK(7, 19), PK(7, 31), NE, NE, NE, NE,},
	[PIN_BOOT_8] = {PK(7, 18), PK(7, 30), PK(7, 7), NE, NE, NE,},
	[PIN_BOOT_9] = {PK(7, 17), PK(7, 6), NE, NE, NE, NE,},
	[PIN_BOOT_10] = {PK(7, 16), PK(7, 29), PK(7, 5), NE, NE, NE,},
	[PIN_BOOT_11] = {PK(7, 15), PK(7, 4), PK(7, 13), NE, NE, NE,},
	[PIN_BOOT_12] = {PK(7, 3), PK(7, 12), NE, NE, NE, NE,},
	[PIN_BOOT_13] = {PK(7, 2), PK(7, 11), NE, NE, NE, NE,},
	[PIN_BOOT_14] = {PK(7, 1), NE, NE, NE, NE, NE,},
	[PIN_BOOT_15] = {PK(7, 28), PK(7, 0), PK(7, 10), NE, NE, NE,},

	[PIN_GPIOH_0] = {PK(6, 31), NE, NE, NE, NE, NE,},
	[PIN_GPIOH_1] = {PK(6, 30), NE, NE, NE, NE, NE,},
	[PIN_GPIOH_2] = {PK(6, 29), NE, NE, NE, NE, NE,},
	[PIN_GPIOH_3] = {NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_4] = {PK(6, 28), PK(6, 27), NE, NE, NE, NE,},
	[PIN_GPIOH_5] = {NE, NE, NE, NE, NE, NE,},
	[PIN_GPIOH_6] = {PK(6, 26), PK(6, 20), NE, NE, NE, NE,},
	[PIN_GPIOH_7] = {PK(6, 25), PK(6, 22), PK(6, 19), NE, NE, NE,},
	[PIN_GPIOH_8] = {PK(6, 24), PK(6, 21), PK(6, 18), NE, NE, NE,},
	[PIN_GPIOH_9] = {PK(6, 23), PK(6, 17), NE, NE, NE, NE,},

	[PIN_GPIOZ_0] = {PK(4, 23), PK(3, 31), PK(3, 19), PK(3, 14), NE, NE,},
	[PIN_GPIOZ_1] = {PK(4, 22), PK(3, 30), PK(3, 18), PK(3, 13), NE, NE,},
	[PIN_GPIOZ_2] = {PK(4, 21), PK(3, 29), PK(3, 17), NE, NE, NE,},
	[PIN_GPIOZ_3] = {PK(4, 20), PK(3, 28), PK(3, 16), PK(3, 12), NE, NE,},
	[PIN_GPIOZ_4] = {PK(4, 19), PK(3, 27), PK(3, 15), PK(3, 11), NE, NE,},
	[PIN_GPIOZ_5] = {PK(4, 18), PK(3, 26), PK(3, 11), NE, NE, NE,},
	[PIN_GPIOZ_6] = {PK(4, 17), PK(4, 9), PK(3, 25), PK(3, 11), NE, NE,},
	[PIN_GPIOZ_7] = {PK(4, 16), PK(4, 8), PK(3, 24), PK(3, 11), NE, NE,},
	[PIN_GPIOZ_8] = {PK(4, 15), PK(4, 7), PK(3, 23), PK(3, 11), NE, NE,},
	[PIN_GPIOZ_9] = {PK(4, 14), PK(4, 6), PK(3, 22), PK(3, 11), NE, NE,},
	[PIN_GPIOZ_10] = {PK(4, 13), PK(4, 5), PK(3, 11), NE, NE, NE,},
	[PIN_GPIOZ_11] = {PK(4, 12), PK(4, 4), PK(3, 11), NE, NE, NE,},
	[PIN_GPIOZ_12] = {PK(4, 11), PK(4, 3), NE, NE, NE, NE,},
	[PIN_GPIOZ_13] = {PK(4, 10), PK(4, 2), NE, NE, NE, NE,},
	[PIN_GPIOZ_14] = {PK(3, 21), PK(4, 25), NE, NE, NE, NE,},
	[PIN_GPIOZ_15] = {PK(3, 20), PK(4, 24), NE, NE, NE, NE,},

	[PIN_GPIODV_0] = {PK(3, 10), PK(3, 2), PK(2, 10), PK(2, 4), PK(1, 8),
				NE,},
	[PIN_GPIODV_1] = {PK(3, 10), PK(3, 1), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_2] = {PK(3, 9), PK(3, 0), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_3] = {PK(3, 9), PK(2, 31), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_4] = {PK(3, 9), PK(2, 30), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_5] = {PK(3, 9), PK(2, 29), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_6] = {PK(3, 9), PK(2, 28), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_7] = {PK(3, 9), PK(2, 27), PK(2, 10), PK(2, 3), NE, NE,},
	[PIN_GPIODV_8] = {PK(3, 8), PK(2, 26), PK(2, 10), PK(2, 2), NE, NE,},
	[PIN_GPIODV_9] = {PK(3, 8), PK(2, 25), PK(2, 10), PK(2, 1), NE, NE,},
	[PIN_GPIODV_10] = {PK(3, 7), PK(2, 24), PK(2, 10), PK(2, 0), NE, NE,},
	[PIN_GPIODV_11] = {PK(3, 7), PK(2, 23), PK(2, 10), PK(1, 31), NE, NE,},
	[PIN_GPIODV_12] = {PK(3, 7), PK(2, 22), PK(2, 10), PK(1, 30), NE, NE,},
	[PIN_GPIODV_13] = {PK(3, 7), PK(2, 21), PK(2, 10), PK(1, 29), NE, NE,},
	[PIN_GPIODV_14] = {PK(3, 7), PK(2, 20), PK(2, 10), PK(1, 28), NE, NE,},
	[PIN_GPIODV_15] = {PK(3, 7), PK(2, 19), PK(2, 10), PK(1, 27), NE, NE,},
	[PIN_GPIODV_16] = {PK(3, 6), PK(2, 9), PK(1, 26), PK(1, 24), NE, NE,},
	[PIN_GPIODV_17] = {PK(3, 6), PK(2, 8), PK(1, 25), PK(1, 23), NE, NE,},
	[PIN_GPIODV_18] = {PK(3, 5), PK(1, 25), PK(1, 17), NE, NE, NE,},
	[PIN_GPIODV_19] = {PK(3, 5), PK(1, 25), PK(1, 16), NE, NE, NE,},
	[PIN_GPIODV_20] = {PK(3, 5), PK(1, 25), NE, NE, NE, NE,},
	[PIN_GPIODV_21] = {PK(3, 5), PK(1, 25), NE, NE, NE, NE,},
	[PIN_GPIODV_22] = {PK(3, 5), PK(2, 18), PK(1, 25), NE, NE, NE,},
	[PIN_GPIODV_23] = {PK(3, 5), PK(2, 17), PK(1, 25), NE, NE, NE,},
	[PIN_GPIODV_24] = {PK(3, 4), PK(2, 16), PK(2, 7), PK(1, 22), PK(1, 15),
				NE,},
	[PIN_GPIODV_25] = {PK(3, 3), PK(2, 15), PK(2, 6), PK(1, 21), PK(1, 14),
				NE,},
	[PIN_GPIODV_26] = {PK(2, 14), PK(1, 20), PK(1, 13), NE, NE, NE,},
	[PIN_GPIODV_27] = {PK(2, 13), PK(1, 18), PK(1, 19), PK(1, 12), NE, NE,},
	[PIN_GPIODV_28] = {PK(2, 12), PK(1, 9), PK(1, 11), NE, NE, NE,},
	[PIN_GPIODV_29] = {PK(2, 11), PK(2, 5), PK(1, 10), NE, NE, NE,},

	[GPIOAO_0] = {PK(AO, 12), PK(AO, 26), NE, NE, NE, NE,},
	[GPIOAO_1] = {PK(AO, 11), PK(AO, 25), NE, NE, NE, NE,},
	[GPIOAO_2] = {PK(AO, 10), PK(AO, 8), NE, NE, NE, NE,},
	[GPIOAO_3] = {PK(AO, 9), PK(AO, 7), PK(AO, 22), NE, NE, NE,},
	[GPIOAO_4] = {PK(AO, 24), PK(AO, 6), PK(AO, 2), NE, NE, NE,},
	[GPIOAO_5] = {PK(AO, 23), PK(AO, 5), PK(AO, 1), NE, NE, NE,},
	[GPIOAO_6] = {PK(AO, 16), PK(AO, 18), NE, NE, NE, NE,},
	[GPIOAO_7] = {PK(AO, 0), PK(AO, 21), NE, NE, NE, NE,},
	[GPIOAO_8] = {PK(AO, 15), PK(AO, 14), PK(AO2, 0), PK(AO, 17), NE, NE,},
	[GPIOAO_9] = {PK(AO, 31), PK(AO, 4), PK(AO2, 1), PK(AO, 3), NE, NE,},

	[PIN_CARD_0] = {PK(6, 5), NE, NE, NE, NE, NE,},
	[PIN_CARD_1] = {PK(6, 4), NE, NE, NE, NE, NE,},
	[PIN_CARD_2] = {PK(6, 3), NE, NE, NE, NE, NE,},
	[PIN_CARD_3] = {PK(6, 2), NE, NE, NE, NE, NE,},
	[PIN_CARD_4] = {PK(6, 1), PK(6, 9), PK(6, 11), PK(6, 7
	), NE, NE,},
	[PIN_CARD_5] = {PK(6, 0), PK(6, 8), PK(6, 10), PK(6, 6
	), NE, NE,},
	[PIN_CARD_6] = {NE, NE, NE, NE, NE, NE,},

	[PIN_GPIO_TEST_N] = {PK(AO, 13), PK(AO2, 2), NE, NE, NE, NE,},
};

static int gxl_gpio_name_to_num(const char *name)
{
	int i, tmp = 100, num = 0;
	int len = 0;
	char *p = NULL;
	char *start = NULL;
	if (!name)
		return -1;
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
		num = num+10;
	else if (!strcmp(p, "GPIOH"))
		num = num+26;
	else if (!strcmp(p, "BOOT"))
		num = num+36;
	else if (!strcmp(p, "CARD"))
		num = num+52;
	else if (!strcmp(p, "GPIODV"))
		num = num+59;
	else if (!strcmp(p, "GPIOX"))
		num = num+89;
	else if (!strcmp(p, "GPIOCLK"))
		num = num+108;
	else
		num = -1;
	kzfree(start);
	return num;

	return num;
}
static int  gxl_clear_pinmux(struct amlogic_pmx *apmx, unsigned int pin)
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
static int gxl_extern_gpio_output(struct meson_domain *domain,
				   unsigned int pin,
				   int value)
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
int gxl_extern_gpio_get(struct meson_domain *domain, unsigned int pin)
{
	if (PIN_GPIO_TEST_N == pin)
		return !!(aml_read_aobus(0x24) & BIT(31));

	return -1;
}

static struct amlogic_pinctrl_soc_data gxl_pinctrl = {
	.pins = gxl_pads,
	.npins = ARRAY_SIZE(gxl_pads),
	.domain_data	= mesongxl_domain_data,
	.num_domains	= ARRAY_SIZE(mesongxl_domain_data),
	.meson_clear_pinmux = gxl_clear_pinmux,
	.name_to_pin = gxl_gpio_name_to_num,
	.soc_extern_gpio_output = gxl_extern_gpio_output,
	.soc_extern_gpio_get = gxl_extern_gpio_get,
};
static struct of_device_id gxl_pinctrl_of_table[] = {
	{
		.compatible = "amlogic, pinmux-gxl",
	},
	{},
};

static int  gxl_pmx_probe(struct platform_device *pdev)
{
	return amlogic_pmx_probe(pdev, &gxl_pinctrl);
}

static int  gxl_pmx_remove(struct platform_device *pdev)
{
	return amlogic_pmx_remove(pdev);
}

static struct platform_driver gxl_pmx_driver = {
	.driver = {
		.name = "pinmux-gxl",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gxl_pinctrl_of_table),
	},
	.probe = gxl_pmx_probe,
	.remove = gxl_pmx_remove,
};

static int __init gxl_pmx_init(void)
{
	return platform_driver_register(&gxl_pmx_driver);
}
arch_initcall(gxl_pmx_init);

static void __exit gxl_pmx_exit(void)
{
	platform_driver_unregister(&gxl_pmx_driver);
}
module_exit(gxl_pmx_exit);
MODULE_DESCRIPTION("gxl pin control driver");
MODULE_LICENSE("GPL v2");
