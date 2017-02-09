/*
 * include/linux/amlogic/pinctrl_amlogic.h
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

#ifndef __PINCTRL_AMLOGIC_H
#define __PINCTRL_AMLOGIC_H
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/gpio/driver.h>
struct amlogic_reg_mask {
	unsigned int reg;
	unsigned int mask;
};
struct amlogic_pmx_mask {
	unsigned int reg;
	unsigned int setmask;
	unsigned int clrmask;
};

struct amlogic_pin_group {
	const char *name;
	unsigned int *pins;
	unsigned num_pins;
	struct amlogic_reg_mask *setmask;
	unsigned num_setmask;
	struct amlogic_reg_mask *clearmask;
	unsigned num_clearmask;
};

/**
 * struct meson_reg_desc - a register descriptor
 *
 * @reg:	register offset in the regmap
 * @bit:	bit index in register
 *
 * The structure describes the information needed to control pull,
 * pull-enable, direction, etc. for a single pin
 */
struct meson_reg_desc {
	unsigned int reg;
	unsigned int bit;
};

/**
 * enum meson_reg_type - type of registers encoded in @meson_reg_desc
 */
enum meson_reg_type {
	REG_PULLEN,
	REG_PULL,
	REG_DIR,
	REG_OUT,
	REG_IN,
	NUM_REG,
};

/**
 * struct meson bank
 *
 * @name:	bank name
 * @first:	first pin of the bank
 * @last:	last pin of the bank
 * @regs:	array of register descriptors
 *
 * A bank represents a set of pins controlled by a contiguous set of
 * bits in the domain registers. The structure specifies which bits in
 * the regmap control the different functionalities. Each member of
 * the @regs array refers to the first pin of the bank.
 */
struct meson_bank {
	const char *name;
	unsigned int first;
	unsigned int last;
	struct meson_reg_desc regs[NUM_REG];
};

/**
 * struct meson_domain_data - domain platform data
 *
 * @name:	name of the domain
 * @banks:	set of banks belonging to the domain
 * @num_banks:	number of banks in the domain
 */
struct meson_domain_data {
	const char *name;
	struct meson_bank *banks;
	unsigned int num_banks;
	unsigned int pin_base;
	unsigned int num_pins;
};

/**
 * struct meson_domain
 *
 * @reg_mux:	registers for mux settings
 * @reg_pullen:	registers for pull-enable settings
 * @reg_pull:	registers for pull settings
 * @reg_gpio:	registers for gpio settings
 * @chip:	gpio chip associated with the domain
 * @data;	platform data for the domain
 * @node:	device tree node for the domain
 *
 * A domain represents a set of banks controlled by the same set of
 * registers.
 */
struct meson_domain {
	struct regmap *reg_mux;
	struct regmap *reg_pullen;
	struct regmap *reg_pull;
	struct regmap *reg_gpio;

	struct gpio_chip chip;
	struct meson_domain_data *data;
	struct device_node *of_node;
};

#define BANK(n, f, l, per, peb, pr, pb, dr, db, or, ob, ir, ib)		\
	{								\
		.name	= n,						\
		.first	= f,						\
		.last	= l,						\
		.regs	= {						\
			[REG_PULLEN]	= { per, peb },			\
			[REG_PULL]	= { pr, pb },			\
			[REG_DIR]	= { dr, db },			\
			[REG_OUT]	= { or, ob },			\
			[REG_IN]	= { ir, ib },			\
		},							\
	 }

/**
 * struct amlogic_pmx_func - describes amlogic pinmux functions
 * @name: the name of this specific function
 * @groups: corresponding pin groups
 */
struct amlogic_pmx_func {
	const char *name;
	const char **groups;
	unsigned num_groups;
};
struct amlogic_pmx;
struct amlogic_pinctrl_soc_data {
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	struct amlogic_pmx_func *functions;
	unsigned nfunctions;
	struct amlogic_pin_group *groups;
	unsigned ngroups;
	struct meson_domain_data *domain_data;
	unsigned int num_domains;
	int (*meson_clear_pinmux)(struct amlogic_pmx *apmx, unsigned int pin);
	int (*name_to_pin)(const char *name);

	int (*soc_extern_gpio_output)(struct meson_domain *domain,
				      unsigned int pin,
				      int value);

	int (*soc_extern_gpio_get)(struct meson_domain *domain,
				   unsigned int pin);
	int (*is_od_domain)(unsigned int pin);
};

struct amlogic_pmx {
	struct device *dev;
	struct pinctrl_dev *pctl;
	struct amlogic_pinctrl_soc_data *soc;
	unsigned int pinmux_cell;
	struct meson_domain *domains;
	int (*meson_set_pullup_down)(struct gpio_chip *, int , int);
	void (*meson_gpio_to_irq)(struct gpio_chip *,
				  unsigned int , unsigned int);
};

int  amlogic_pmx_probe(struct platform_device *pdev,
		struct amlogic_pinctrl_soc_data *soc_data);
int amlogic_pmx_remove(struct platform_device *pdev);

static inline struct meson_domain *to_meson_domain(struct gpio_chip *chip)
{
	return container_of(chip, struct meson_domain, chip);
}

extern struct amlogic_pmx *gl_pmx;
#endif
