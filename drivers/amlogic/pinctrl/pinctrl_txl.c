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
#include <dt-bindings/gpio/txl.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/uaccess.h>

#include "pinconf-amlogic.h"
#include "pinctrl_txl.h"

struct txl_od_gpio {
	int start;
	int end;
};

static const struct txl_od_gpio txl_all_od_gpios[] = {
	{GPIOW_0, GPIOW_15},
};

struct pinctrl_pin_desc pmx_pins[] = {
	PINCTRL_PIN(PIN_GPIOAO_0,	"GPIOAO_0"),
	PINCTRL_PIN(PIN_GPIOAO_1,	"GPIOAO_1"),
	PINCTRL_PIN(PIN_GPIOAO_2,	"GPIOAO_2"),
	PINCTRL_PIN(PIN_GPIOAO_3,	"GPIOAO_3"),
	PINCTRL_PIN(PIN_GPIOAO_4,	"GPIOAO_4"),
	PINCTRL_PIN(PIN_GPIOAO_5,	"GPIOAO_5"),
	PINCTRL_PIN(PIN_GPIOAO_6,	"GPIOAO_6"),
	PINCTRL_PIN(PIN_GPIOAO_7,	"GPIOAO_7"),
	PINCTRL_PIN(PIN_GPIOAO_8,	"GPIOAO_8"),
	PINCTRL_PIN(PIN_GPIOAO_9,	"GPIOAO_9"),
	PINCTRL_PIN(PIN_GPIOAO_10,	"GPIOAO_10"),
	PINCTRL_PIN(PIN_GPIOAO_11,	"GPIOAO_11"),

	PINCTRL_PIN(PIN_GPIOZ_0,	"GPIOZ_0"),
	PINCTRL_PIN(PIN_GPIOZ_1,	"GPIOZ_1"),
	PINCTRL_PIN(PIN_GPIOZ_2,	"GPIOZ_2"),
	PINCTRL_PIN(PIN_GPIOZ_3,	"GPIOZ_3"),
	PINCTRL_PIN(PIN_GPIOZ_4,	"GPIOZ_4"),
	PINCTRL_PIN(PIN_GPIOZ_5,	"GPIOZ_5"),
	PINCTRL_PIN(PIN_GPIOZ_6,	"GPIOZ_6"),
	PINCTRL_PIN(PIN_GPIOZ_7,	"GPIOZ_7"),
	PINCTRL_PIN(PIN_GPIOZ_8,	"GPIOZ_8"),
	PINCTRL_PIN(PIN_GPIOZ_9,	"GPIOZ_9"),
	PINCTRL_PIN(PIN_GPIOZ_10,	"GPIOZ_10"),
	PINCTRL_PIN(PIN_GPIOZ_11,	"GPIOZ_11"),
	PINCTRL_PIN(PIN_GPIOZ_12,	"GPIOZ_12"),
	PINCTRL_PIN(PIN_GPIOZ_13,	"GPIOZ_13"),
	PINCTRL_PIN(PIN_GPIOZ_14,	"GPIOZ_14"),
	PINCTRL_PIN(PIN_GPIOZ_15,	"GPIOZ_15"),
	PINCTRL_PIN(PIN_GPIOZ_16,	"GPIOZ_16"),
	PINCTRL_PIN(PIN_GPIOZ_17,	"GPIOZ_17"),
	PINCTRL_PIN(PIN_GPIOZ_18,	"GPIOZ_18"),
	PINCTRL_PIN(PIN_GPIOZ_19,	"GPIOZ_19"),
	PINCTRL_PIN(PIN_GPIOZ_20,	"GPIOZ_20"),
	PINCTRL_PIN(PIN_GPIOZ_21,	"GPIOZ_21"),

	PINCTRL_PIN(PIN_GPIOH_0,	"GPIOH_0"),
	PINCTRL_PIN(PIN_GPIOH_1,	"GPIOH_1"),
	PINCTRL_PIN(PIN_GPIOH_2,	"GPIOH_2"),
	PINCTRL_PIN(PIN_GPIOH_3,	"GPIOH_3"),
	PINCTRL_PIN(PIN_GPIOH_4,	"GPIOH_4"),
	PINCTRL_PIN(PIN_GPIOH_5,	"GPIOH_5"),
	PINCTRL_PIN(PIN_GPIOH_6,	"GPIOH_6"),
	PINCTRL_PIN(PIN_GPIOH_7,	"GPIOH_7"),
	PINCTRL_PIN(PIN_GPIOH_8,	"GPIOH_8"),
	PINCTRL_PIN(PIN_GPIOH_9,	"GPIOH_9"),

	PINCTRL_PIN(PIN_BOOT_0,		"BOOT_0"),
	PINCTRL_PIN(PIN_BOOT_1,		"BOOT_1"),
	PINCTRL_PIN(PIN_BOOT_2,		"BOOT_2"),
	PINCTRL_PIN(PIN_BOOT_3,		"BOOT_3"),
	PINCTRL_PIN(PIN_BOOT_4,		"BOOT_4"),
	PINCTRL_PIN(PIN_BOOT_5,		"BOOT_5"),
	PINCTRL_PIN(PIN_BOOT_6,		"BOOT_6"),
	PINCTRL_PIN(PIN_BOOT_7,		"BOOT_7"),
	PINCTRL_PIN(PIN_BOOT_8,		"BOOT_8"),
	PINCTRL_PIN(PIN_BOOT_9,		"BOOT_9"),
	PINCTRL_PIN(PIN_BOOT_10,	"BOOT_10"),
	PINCTRL_PIN(PIN_BOOT_11,	"BOOT_11"),

	PINCTRL_PIN(PIN_CARD_0,		"CARD_0"),
	PINCTRL_PIN(PIN_CARD_1,		"CARD_1"),
	PINCTRL_PIN(PIN_CARD_2,		"CARD_2"),
	PINCTRL_PIN(PIN_CARD_3,		"CARD_3"),
	PINCTRL_PIN(PIN_CARD_4,		"CARD_4"),
	PINCTRL_PIN(PIN_CARD_5,		"CARD_5"),
	PINCTRL_PIN(PIN_CARD_6,		"CARD_6"),

	PINCTRL_PIN(PIN_GPIODV_0,	"GPIODV_0"),
	PINCTRL_PIN(PIN_GPIODV_1,	"GPIODV_1"),
	PINCTRL_PIN(PIN_GPIODV_2,	"GPIODV_2"),
	PINCTRL_PIN(PIN_GPIODV_3,	"GPIODV_3"),
	PINCTRL_PIN(PIN_GPIODV_4,	"GPIODV_4"),
	PINCTRL_PIN(PIN_GPIODV_5,	"GPIODV_5"),
	PINCTRL_PIN(PIN_GPIODV_6,	"GPIODV_6"),
	PINCTRL_PIN(PIN_GPIODV_7,	"GPIODV_7"),
	PINCTRL_PIN(PIN_GPIODV_8,	"GPIODV_8"),
	PINCTRL_PIN(PIN_GPIODV_9,	"GPIODV_9"),
	PINCTRL_PIN(PIN_GPIODV_10,	"GPIODV_10"),
	PINCTRL_PIN(PIN_GPIODV_11,	"GPIODV_11"),

	PINCTRL_PIN(PIN_GPIOW_0,	"GPIOW_0"),
	PINCTRL_PIN(PIN_GPIOW_1,	"GPIOW_1"),
	PINCTRL_PIN(PIN_GPIOW_2,	"GPIOW_2"),
	PINCTRL_PIN(PIN_GPIOW_3,	"GPIOW_3"),
	PINCTRL_PIN(PIN_GPIOW_4,	"GPIOW_4"),
	PINCTRL_PIN(PIN_GPIOW_5,	"GPIOW_5"),
	PINCTRL_PIN(PIN_GPIOW_6,	"GPIOW_6"),
	PINCTRL_PIN(PIN_GPIOW_7,	"GPIOW_7"),
	PINCTRL_PIN(PIN_GPIOW_8,	"GPIOW_8"),
	PINCTRL_PIN(PIN_GPIOW_9,	"GPIOW_9"),
	PINCTRL_PIN(PIN_GPIOW_10,	"GPIOW_10"),
	PINCTRL_PIN(PIN_GPIOW_11,	"GPIOW_11"),
	PINCTRL_PIN(PIN_GPIOW_12,	"GPIOW_12"),
	PINCTRL_PIN(PIN_GPIOW_13,	"GPIOW_13"),
	PINCTRL_PIN(PIN_GPIOW_14,	"GPIOW_14"),
	PINCTRL_PIN(PIN_GPIOW_15,	"GPIOW_15"),

	PINCTRL_PIN(PIN_GPIOCLK_0,	"GPIOCLK_0"),
	PINCTRL_PIN(PIN_GPIOCLK_1,	"GPIOCLK_1"),

	PINCTRL_PIN(PIN_GPIO_TEST_N,	"GPIO_TEST_N"),

	PINCTRL_PIN(PIN_GPIO_USB_DPDM,	"GPIO_USB_DPDM"),
	PINCTRL_PIN(PIN_GPIO_USB_DMDP,	"GPIO_USB_DMDP"),

};


/* name		first			last
 *		pullen_reg_idx		start_bit
 *		pullup_reg_idx		start_bit
 *		gpio_dir_reg_idx	start_bit
 *		gpio_out_reg_idx	start_bit
 *		gpio_in_reg_idx		start_bit
 */
static struct meson_bank pmx_ao_banks[] = {
	BANK("AO",	PIN_GPIOAO_0,		PIN_GPIOAO_11,
			GPIO_AO_PULL_EN_REG0,	GPIO_AO_PULL_EN_REG0_BIT0,
			GPIO_AO_PULL_UP_REG0,	GPIO_AO_PULL_UP_REG0_BIT16,
			GPIO_AO_D_REG0,		GPIO_AO_D_REG0_BIT0,
			GPIO_AO_O_REG0,		GPIO_AO_O_REG0_BIT16,
			GPIO_AO_I_REG1,		GPIO_AO_I_REG1_BIT0),
};

static struct meson_bank pmx_ee_banks[] = {
	BANK("Z",	PIN_GPIOZ_0,	PIN_GPIOZ_21,
			PULL_EN_REG3,	0,
			PULL_UP_REG3,	0,
			GPIO_D_REG3,	0,
			GPIO_O_REG3,	0,
			GPIO_I_REG3,	0),

	BANK("H",	PIN_GPIOH_0,	PIN_GPIOH_9,
			PULL_EN_REG1,	20,
			PULL_UP_REG1,	20,
			GPIO_D_REG1,	20,
			GPIO_O_REG1,	20,
			GPIO_I_REG1,	20),

	BANK("BOOT",	PIN_BOOT_0,	PIN_BOOT_11,
			PULL_EN_REG2,	0,
			PULL_UP_REG2,	0,
			GPIO_D_REG2,	0,
			GPIO_O_REG2,	0,
			GPIO_I_REG2,	0),

	BANK("CARD",	PIN_CARD_0,	PIN_CARD_6,
			PULL_EN_REG2,	20,
			PULL_UP_REG2,	20,
			GPIO_D_REG2,	20,
			GPIO_O_REG2,	20,
			GPIO_I_REG2,	20),

	BANK("DV",	PIN_GPIODV_0,	PIN_GPIODV_11,
			PULL_EN_REG0,	0,
			PULL_UP_REG0,	0,
			GPIO_D_REG0,	0,
			GPIO_O_REG0,	0,
			GPIO_I_REG0,	0),

	BANK("W",	PIN_GPIOW_0,	PIN_GPIOW_15,
			PULL_EN_REG4,	0,
			PULL_UP_REG4,	0,
			GPIO_D_REG4,	0,
			GPIO_O_REG4,	0,
			GPIO_I_REG4,	0),

	BANK("CLK",	PIN_GPIOCLK_0,	PIN_GPIOCLK_1,
			PULL_EN_REG3,	28,
			PULL_UP_REG3,	28,
			GPIO_D_REG3,	28,
			GPIO_O_REG3,	28,
			GPIO_I_REG3,	28),
};

static struct meson_domain_data pmx_domain_data[] = {
	{
		.name		= "banks",
		.banks		= pmx_ee_banks,
		.num_banks	= ARRAY_SIZE(pmx_ee_banks),
		.pin_base	= PIN_EE_BASE,
		.num_pins	= PIN_EE_SIZE,
	},
	{
		.name		= "ao-bank",
		.banks		= pmx_ao_banks,
		.num_banks	= ARRAY_SIZE(pmx_ao_banks),
		.pin_base	= PIN_AO_BASE,
		.num_pins	= PIN_AO_SIZE,
	},

};


static unsigned int pmx_pinmuxes[][PMX_FUNCTION_NUM] = {
	[PIN_GPIOAO_0]  = {PA(0, 12), PA(0, 26), NA,        NA,        NA,},
	[PIN_GPIOAO_1]  = {PA(0, 11), PA(0, 25), NA,        NA,        NA,},
	[PIN_GPIOAO_2]  = {PA(0, 10), PA(0, 8),  PA(0, 28), NA,        NA,},
	[PIN_GPIOAO_3]  = {PA(0, 9),  PA(0, 7),  NA,        PA(0, 22), PJ(0),},
	[PIN_GPIOAO_4]  = {PA(0, 24), PA(0, 6),  PA(0, 2),  NA,        PJ(0),},
	[PIN_GPIOAO_5]  = {PA(0, 23), PA(0, 5),  PA(0, 1),  NA,        PJ(0),},
	[PIN_GPIOAO_6]  = {PA(0, 0),  PA(0, 21), NA,        NA,        NA,},
	[PIN_GPIOAO_7]  = {PA(0, 15), PA(0, 14), PA(0, 17), NA,        PJ(0),},
	[PIN_GPIOAO_8]  = {NA,        PA(0, 27), NA,        NA,        NA,},
	[PIN_GPIOAO_9]  = {NA,        PA(0, 3),  NA,        NA,        NA,},
	[PIN_GPIOAO_10] = {NA,        NA,        NA,        NA,        NA,},
	[PIN_GPIOAO_11] = {NA,        NA,        NA,        NA,        NA,},


	[PIN_GPIODV_0]  = {PE(2, 25), NA,        NA,        PE(2, 31), NA,},
	[PIN_GPIODV_1]  = {PE(2, 24), NA,        NA,        PE(2, 31), NA,},
	[PIN_GPIODV_2]  = {PE(2, 22), PE(2, 23), PE(2, 20), PE(2, 31),
								PE(2, 19),},
	[PIN_GPIODV_3]  = {PE(2, 18), PE(2, 21), NA,        PE(2, 31),
								PE(2, 17),},
	[PIN_GPIODV_4]  = {PE(2, 16), NA,        NA,        PE(2, 31), NA,},
	[PIN_GPIODV_5]  = {PE(2, 15), NA,        NA,        PE(2, 31), NA,},
	[PIN_GPIODV_6]  = {NA,        PE(2, 8),  NA,        PE(2, 31), NA,},
	[PIN_GPIODV_7]  = {NA,        PE(2, 7),  PE(2, 4),  PE(2, 30), NA,},
	[PIN_GPIODV_8]  = {PE(2, 14), PE(2, 6),	 PE(2, 3),  PE(2, 29), NA,},
	[PIN_GPIODV_9]  = {PE(2, 13), PE(2, 5),	 NA,        PE(2, 28), NA,},
	[PIN_GPIODV_10] = {PE(2, 12), PE(2, 10), PE(2, 2),  PE(2, 27), NA,},
	[PIN_GPIODV_11] = {PE(2, 11), PE(2, 9),  PE(2, 1),  PE(2, 26),
								PE(2, 0),},

	[PIN_CARD_0] = {PE(6, 5), PJ(1),    NA,        PE(6, 15), PE(6, 31),},
	[PIN_CARD_1] = {PE(6, 4), PJ(1),    NA,        PE(6, 14), PE(6, 30),},
	[PIN_CARD_2] = {PE(6, 3), PJ(1),    NA,        PE(6, 13), PE(6, 29),},
	[PIN_CARD_3] = {PE(6, 2), PJ(1),    NA,        PE(6, 12), PE(6, 28),},
	[PIN_CARD_4] = {PE(6, 0), PE(6, 9), PE(6, 11), NA,        PE(6, 27),},
	[PIN_CARD_5] = {PE(6, 1), PE(6, 8), PE(6, 10), NA,        PE(6, 26),},
	[PIN_CARD_6] = {NA,       NA,       NA,        NA,        NA,},

	[PIN_GPIOH_0] = {PE(0, 31), NA,        NA,        NA, NA,},
	[PIN_GPIOH_1] = {PE(0, 30), NA,        NA,        NA, NA,},
	[PIN_GPIOH_2] = {PE(0, 29), NA,        NA,        NA, NA,},
	[PIN_GPIOH_3] = {PE(0, 28), NA,        NA,        NA, NA,},
	[PIN_GPIOH_4] = {PE(0, 27), PE(0, 26), NA,        NA, NA,},
	[PIN_GPIOH_5] = {NA,        NA,        PE(0, 18), NA, NA,},
	[PIN_GPIOH_6] = {NA,        NA,        PE(0, 17), NA, NA,},
	[PIN_GPIOH_7] = {NA,        NA,        PE(0, 16), NA, NA,},
	[PIN_GPIOH_8] = {NA,        PE(0, 13), PE(0, 15), NA, NA,},
	[PIN_GPIOH_9] = {PE(0, 12), NA,        PE(0, 14), NA, NA,},

	[PIN_BOOT_0]  = {PE(7, 31), NA,        NA,        NA,        NA,},
	[PIN_BOOT_1]  = {PE(7, 31), NA,        NA,        NA,        NA,},
	[PIN_BOOT_2]  = {PE(7, 31), NA,        NA,        NA,        NA,},
	[PIN_BOOT_3]  = {PE(7, 31), NA,        NA,        PE(7, 23), NA,},
	[PIN_BOOT_4]  = {PE(7, 31), PE(7, 13), NA,        PE(7, 22), NA,},
	[PIN_BOOT_5]  = {PE(7, 31), PE(7, 12), NA,        PE(7, 21), NA,},
	[PIN_BOOT_6]  = {PE(7, 31), PE(7, 11), NA,        PE(7, 20), NA,},
	[PIN_BOOT_7]  = {PE(7, 31), NA,        NA,        PE(7, 19), NA,},
	[PIN_BOOT_8]  = {PE(7, 30), NA,        NA,        NA,        NA,},
	[PIN_BOOT_9]  = {NA,        NA,        NA,        NA,        NA,},
	[PIN_BOOT_10] = {PE(7, 29), NA,        NA,        NA,        NA,},
	[PIN_BOOT_11] = {PE(7, 28), PE(7, 10), NA,        NA,        NA,},


	[PIN_GPIOZ_0]  = {PE(4, 31), PE(4, 25), NA,        PE(3, 21),
								PE(3, 16),},
	[PIN_GPIOZ_1]  = {PE(4, 30), PE(4, 24), NA,        PE(3, 21),
								PE(3, 15),},
	[PIN_GPIOZ_2]  = {PE(4, 29), PE(4, 23), PE(4, 21), PE(3, 21),
								PE(3, 14),},
	[PIN_GPIOZ_3]  = {PE(4, 28), PE(4, 22), PE(4, 20), PE(3, 21),
								PE(3, 13),},
	[PIN_GPIOZ_4]  = {PE(4, 27), PE(4, 19), PE(4, 18), PE(3, 21), NA,},
	[PIN_GPIOZ_5]  = {PE(4, 26), PE(4, 17), NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_6]  = {PE(4, 16), PE(4, 15), NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_7]  = {PE(4, 14), PE(4, 13), NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_8]  = {PE(4, 12), NA,        NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_9]  = {PE(4, 11), NA,        NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_10] = {PE(4, 10), NA,        PE(3, 20), PE(3, 21), NA,},
	[PIN_GPIOZ_11] = {PE(4, 9),  PE(4, 3),  PE(3, 19), PE(3, 21), NA,},
	[PIN_GPIOZ_12] = {PE(4, 8),  PE(4, 2),  PE(3, 18), PE(3, 21), NA,},
	[PIN_GPIOZ_13] = {NA,        PE(4, 1),  PE(3, 17), PE(3, 21), NA,},
	[PIN_GPIOZ_14] = {PE(4, 7),  NA,        NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_15] = {PE(4, 6),  NA,        PE(3, 25), PE(3, 21), NA,},
	[PIN_GPIOZ_16] = {PE(4, 5),  PE(3, 31), PE(3, 24), PE(3, 21), NA,},
	[PIN_GPIOZ_17] = {PE(3, 22), NA,        NA,        PE(3, 21), NA,},
	[PIN_GPIOZ_18] = {PE(3, 22), PE(4, 4),  PE(3, 23), NA,        NA,},
	[PIN_GPIOZ_19] = {PE(3, 22), PE(3, 27), PE(3, 22), PE(3, 26), NA,},
	[PIN_GPIOZ_20] = {NA,        NA,        NA,        NA,        NA,},
	[PIN_GPIOZ_21] = {NA,        NA,        NA,        NA,        NA,},

	[PIN_GPIOW_0]  = {PE(5, 31), NA,        NA, NA, NA,},
	[PIN_GPIOW_1]  = {PE(5, 30), NA,        NA, NA, NA,},
	[PIN_GPIOW_2]  = {PE(5, 29), PE(5, 15), NA, NA, NA,},
	[PIN_GPIOW_3]  = {PE(5, 28), PE(5, 14), NA, NA, NA,},
	[PIN_GPIOW_4]  = {PE(5, 27), NA,        NA, NA, NA,},
	[PIN_GPIOW_5]  = {PE(5, 26), NA,        NA, NA, NA,},
	[PIN_GPIOW_6]  = {PE(5, 25), PE(5, 13), NA, NA, NA,},
	[PIN_GPIOW_7]  = {PE(5, 24), PE(5, 12), NA, NA, NA,},
	[PIN_GPIOW_8]  = {PE(5, 23), NA,        NA, NA, NA,},
	[PIN_GPIOW_9]  = {PE(5, 22), NA,        NA, NA, NA,},
	[PIN_GPIOW_10] = {PE(5, 21), PE(5, 11), NA, NA, NA,},
	[PIN_GPIOW_11] = {PE(5, 20), PE(5, 10), NA, NA, NA,},
	[PIN_GPIOW_12] = {PE(5, 19), NA,        NA, NA, NA,},
	[PIN_GPIOW_13] = {PE(5, 18), NA,        NA, NA, NA,},
	[PIN_GPIOW_14] = {PE(5, 17), PE(5, 9),  NA, NA, NA,},
	[PIN_GPIOW_15] = {PE(5, 16), PE(5, 8),  NA, NA, NA,},

	[PIN_GPIOCLK_0] = {PC(10), PC(11),    PE(8, 31), PE(8, 28), NA,},
	[PIN_GPIOCLK_1] = {PC(12), PE(8, 30), PE(8, 29), PE(8, 27), NA,},

	[PIN_GPIO_TEST_N] = {PA(0, 20), NA, NA, NA, NA,},
};

static int pmx_gpio_name_to_pin(const char *name)
{
	int i, tmp = 100, num = 0;
	int len = 0;
	char *p = NULL;
	char *start = NULL;

	if (!name)
		return -1;

	if (!strcmp(name, "GPIO_TEST_N"))
		return PIN_GPIO_TEST_N;
	if (!strcmp(name, "GPIO_USB_DPDM"))
		return PIN_GPIO_USB_DPDM;
	if (!strcmp(name, "GPIO_USB_DMDP"))
		return PIN_GPIO_USB_DMDP;

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
		num = num + PIN_AO_BASE;
	else if (!strcmp(p, "GPIOZ"))
		num = num + PIN_GPIOZ_BASE;
	else if (!strcmp(p, "GPIOH"))
		num = num + PIN_GPIOH_BASE;
	else if (!strcmp(p, "BOOT"))
		num = num + PIN_BOOT_BASE;
	else if (!strcmp(p, "CARD"))
		num = num + PIN_CARD_BASE;
	else if (!strcmp(p, "GPIODV"))
		num = num + PIN_GPIODV_BASE;
	else if (!strcmp(p, "GPIOW"))
		num = num + PIN_GPIOW_BASE;
	else if (!strcmp(p, "GPIOCLK"))
		num = num + PIN_GPIOCLK_BASE;
	else
		num = -1;
	kzfree(start);
	return num;
}


static int  pmx_clear_pinmux(struct amlogic_pmx *apmx, unsigned int pin)
{
	int i, reg, bit, ret = 0;
	struct meson_domain *dom = NULL;
	unsigned int v;
	unsigned int type;
	unsigned int sel;

	for (i = 0; i < PMX_FUNCTION_NUM; i++) {
		v = pmx_pinmuxes[pin][i];

		if (v == NA)
			continue;

		type = PMX_TYP(v);

		switch (type) {
		case PMX_TYPE_EE:
			reg = PMX_REG(v);
			bit = PMX_BIT(v);
			dom = &apmx->domains[0];
			ret = regmap_update_bits(dom->reg_mux,
				reg*4, BIT(bit), 0);
			break;

		case PMX_TYPE_AO:
			reg = PMX_AO_REG(v);
			bit = PMX_AO_BIT(v);
			dom = &apmx->domains[1];
			ret = regmap_update_bits(dom->reg_mux,
				reg*4, BIT(bit), 0);
			break;

		case PMX_TYPE_JTAG:
			sel = PMX_JTAG(v);
			/* @todo clear jtag reg */
			break;

		case PMX_TYPE_CLK:
			bit = PMX_CLK_BIT(v);
			/* @todo clear clk reg */
			break;

		default:
			ret = -EINVAL;
			break;
		}
	}
	return ret;
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
static int pmx_extern_gpio_output(struct meson_domain *domain,
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
int pmx_extern_gpio_get(struct meson_domain *domain, unsigned int pin)
{
	if (PIN_GPIO_TEST_N == pin)
		return !!(aml_read_aobus(0x24) & BIT(31));

	return -1;
}

static int is_txl_gpio_in_od_domain(unsigned int pin)
{
	int i = 0;
	int len = sizeof(txl_all_od_gpios)/sizeof(txl_all_od_gpios[0]);

	for (i = 0; i < len; i++) {
		if ((pin >= txl_all_od_gpios[i].start)
			&& (pin <= txl_all_od_gpios[i].end))
			return 1;
	}
	return 0;
}
static struct amlogic_pinctrl_soc_data pmx_pinctrl_data = {
	.pins = pmx_pins,
	.npins = ARRAY_SIZE(pmx_pins),
	.domain_data = pmx_domain_data,
	.num_domains = ARRAY_SIZE(pmx_domain_data),
	.meson_clear_pinmux = pmx_clear_pinmux,
	.name_to_pin = pmx_gpio_name_to_pin,
	.soc_extern_gpio_output = pmx_extern_gpio_output,
	.soc_extern_gpio_get = pmx_extern_gpio_get,
	.is_od_domain = is_txl_gpio_in_od_domain,
};

static struct of_device_id pmx_pinctrl_of_table[] = {
	{
		.compatible = "amlogic, pinmux-txl",
	},
	{},
};

static int  pmx_probe(struct platform_device *pdev)
{
	return amlogic_pmx_probe(pdev, &pmx_pinctrl_data);
}

static int  pmx_remove(struct platform_device *pdev)
{
	return amlogic_pmx_remove(pdev);
}

static struct platform_driver pmx_driver = {
	.driver = {
		.name = "pinmux-txl",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pmx_pinctrl_of_table),
	},
	.probe = pmx_probe,
	.remove = pmx_remove,
};

static int __init pmx_init(void)
{
	return platform_driver_register(&pmx_driver);
}
arch_initcall(pmx_init);

static void __exit pmx_exit(void)
{
	platform_driver_unregister(&pmx_driver);
}
module_exit(pmx_exit);

MODULE_DESCRIPTION("txl pin control driver");
MODULE_LICENSE("GPL v2");
