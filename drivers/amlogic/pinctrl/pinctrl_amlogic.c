/*
 * drivers/amlogic/pinctrl/pinctrl_amlogic.c
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
#include <linux/device.h>
#include <linux/amlogic/pinctrl_amlogic.h>
#include <linux/pinctrl/pinctrl-state.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/vmalloc.h>
#include "pinconf-amlogic.h"
#include "../../pinctrl/core.h"
#include <linux/amlogic/gpio-amlogic.h>
#include <linux/of_address.h>
int (*_gpio_name_map_num)(const char *name);
struct amlogic_pmx *gl_pmx;
struct regmap *int_reg;
#define GPIO_EDGE 0
#define GPIO_SELECT_0_3 1
#define GPIO_SELECT_4_7 2
#define GPIO_FILTER_NUM 3

/* #define AML_PIN_DEBUG_GUP */
const char *pctdev_name;

/**
 * meson_get_bank() - find the bank containing a given pin
 *
 * @domain:	the domain containing the pin
 * @pin:	the pin number
 * @bank:	the found bank
 *
 * Return:	0 on success, a negative value on error
 */
static int meson_get_bank(struct meson_domain *domain, unsigned int pin,
			  struct meson_bank **bank)
{
	int i;

	for (i = 0; i < domain->data->num_banks; i++) {
		if (pin >= domain->data->banks[i].first &&
		    pin <= domain->data->banks[i].last) {
			*bank = &domain->data->banks[i];
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * meson_get_domain_and_bank() - find domain and bank containing a given pin
 *
 * @pc:		Meson pin controller device
 * @pin:	the pin number
 * @domain:	the found domain
 * @bank:	the found bank
 *
 * Return:	0 on success, a negative value on error
 */
static int meson_get_domain_and_bank(struct amlogic_pmx *pc, unsigned int pin,
				     struct meson_domain **domain,
				     struct meson_bank **bank)
{
	struct meson_domain *d;
	int i;

	for (i = 0; i < pc->soc->num_domains; i++) {
		d = &pc->domains[i];
		if (pin >= d->data->pin_base &&
		    pin < d->data->pin_base + d->data->num_pins) {
			*domain = d;
			return meson_get_bank(d, pin, bank);
		}
	}

	return -EINVAL;
}

/**
 * meson_calc_reg_and_bit() - calculate register and bit for a pin
 *
 * @bank:	the bank containing the pin
 * @pin:	the pin number
 * @reg_type:	the type of register needed (pull-enable, pull, etc...)
 * @reg:	the computed register offset
 * @bit:	the computed bit
 */
static void meson_calc_reg_and_bit(struct meson_bank *bank, unsigned int pin,
				   enum meson_reg_type reg_type,
				   unsigned int *reg, unsigned int *bit)
{
	struct meson_reg_desc *desc = &bank->regs[reg_type];

	*reg = desc->reg * 4;
	*bit = desc->bit + pin - bank->first;
}


/******************************************************************/
static int amlogic_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	return apmx->soc->ngroups;
}

static const char *amloigc_get_group_name(struct pinctrl_dev *pctldev,
				       unsigned group)
{
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	return apmx->soc->groups[group].name;
}

static int amlogic_get_group_pins(struct pinctrl_dev *pctldev,
			       unsigned selector,
			       const unsigned **pins,
			       unsigned *num_pins)
{
	struct amlogic_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	*pins = pmx->soc->groups[selector].pins;
	*num_pins = pmx->soc->groups[selector].num_pins;
	return 0;
}
#if 0
static void amlogic_pin_dbg_show(struct pinctrl_dev *pctldev,
		   struct seq_file *s,
		   unsigned offset)
{
	seq_printf(s, " " DRIVER_NAME);
}
#endif
#if CONFIG_OF
void amlogic_pinctrl_dt_free_map(struct pinctrl_dev *pctldev,
			       struct pinctrl_map *map, unsigned num_maps)
{
	u32 i;
	for (i = 0; i < num_maps; i++) {
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[i].data.configs.configs);
	}
	kfree(map);
}



int amlogic_pinctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				 struct device_node *np,
				 struct pinctrl_map **map, unsigned *num_maps)
{
	struct pinctrl_map *new_map = NULL;
	unsigned new_num = 1;
	unsigned long config = 0;
	unsigned long *pconfig;
	const char *pinctrl_set = "amlogic,setmask";
	const char *pinctrl_clr = "amlogic,clrmask";
	bool purecfg = false;
	u32 val, reg;
	int ret, i = 0;

	/* Check for pin config node which has no 'reg' property */
	if (of_property_read_u32(np, pinctrl_set, &reg) &&
			of_property_read_u32(np, pinctrl_clr, &val))
		purecfg = true;

	ret = of_property_read_u32(np, "amlogic,pullup", &val);
	if (!ret)
		config = AML_PINCONF_PACK_PULL(AML_PCON_PULLUP, val);
	ret = of_property_read_u32(np, "amlogic,pullupen", &val);
	if (!ret)
		config |= AML_PINCONF_PACK_PULLEN(AML_PCON_PULLUP, val);
	ret = of_property_read_u32(np, "amlogic,enable-output", &val);
	if (!ret)
		config |= AML_PINCONF_PACK_ENOUT(AML_PCON_ENOUT, val);

	/* Check for group node which has both mux and config settings */
	if (!purecfg && config)
		new_num = 2;

	new_map = kzalloc(sizeof(*new_map) * new_num, GFP_KERNEL);
	if (!new_map) {
		pr_info("vmalloc map fail\n");
		return -ENOMEM;
	}

	if (config) {
		pconfig = kmemdup(&config, sizeof(config), GFP_KERNEL);
		if (!pconfig) {
			ret = -ENOMEM;
			goto free_group;
		}

		new_map[i].type = PIN_MAP_TYPE_CONFIGS_GROUP;
		new_map[i].data.configs.group_or_pin = np->name;
		new_map[i].data.configs.configs = pconfig;
		new_map[i].data.configs.num_configs = 1;
		i++;
	}

	if (!purecfg) {
		new_map[i].type = PIN_MAP_TYPE_MUX_GROUP;
		new_map[i].data.mux.function = np->name;
		new_map[i].data.mux.group = np->name;
	}

	*map = new_map;
	*num_maps = new_num;

	return 0;

free_group:
	kfree(new_map);
	return ret;
}
#else
int amlogic_pinctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				 struct device_node *np_config,
				 struct pinctrl_map **map, unsigned *num_maps)
{
	return 0;
}
void amlogic_pinctrl_dt_free_map(struct pinctrl_dev *pctldev,
			       struct pinctrl_map *map, unsigned num_maps)
{
	return;
}

#endif
static struct pinctrl_ops amlogic_pctrl_ops = {
	.get_groups_count = amlogic_get_groups_count,
	.get_group_name = amloigc_get_group_name,
	.get_group_pins = amlogic_get_group_pins,
	.dt_node_to_map = amlogic_pinctrl_dt_node_to_map,
	.dt_free_map = amlogic_pinctrl_dt_free_map,
};

static int amlogic_pmx_enable(struct pinctrl_dev *pctldev, unsigned selector,
			   unsigned group)
{
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	int i, dom, reg, ret;
	struct amlogic_pin_group *pin_group =  &apmx->soc->groups[group];
	struct amlogic_reg_mask *setmask = pin_group->setmask;
	struct amlogic_reg_mask *clrmask = pin_group->clearmask;
	struct meson_domain *domain = NULL;
	for (i = 0; i < pin_group->num_clearmask; i++) {
		dom = clrmask[i].reg >> 4;
		reg = clrmask[i].reg & 0xf;
		domain = &apmx->domains[dom];
		ret = regmap_update_bits(domain->reg_mux, reg * 4,
					 clrmask[i].mask, 0);
		if (ret)
			return ret;
	}
	for (i = 0; i < pin_group->num_setmask; i++) {
		dom = setmask[i].reg >> 4;
		reg = setmask[i].reg & 0xf;
		domain = &apmx->domains[dom];
		ret = regmap_update_bits(domain->reg_mux, reg * 4,
					 setmask[i].mask, setmask[i].mask);
		if (ret)
			return ret;
	}
	return 0;
}

static void amlogic_pmx_disable(struct pinctrl_dev *pctldev, unsigned selector,
			     unsigned group)
{
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	int i, dom, reg;
	struct amlogic_pin_group *pin_group =  &apmx->soc->groups[group];
	struct amlogic_reg_mask *setmask = pin_group->setmask;
	struct meson_domain *domain = NULL;

	for (i = 0; i < pin_group->num_setmask; i++) {

		dom = setmask[i].reg >> 4;
		reg = setmask[i].reg & 0xf;
		domain = &apmx->domains[dom];
		regmap_update_bits(domain->reg_mux, reg * 4,
					 setmask[i].mask, 0);
	}
	return;
}

static int amlogic_pmx_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	return apmx->soc->nfunctions;
}

static const char *amlogic_pmx_get_func_name(struct pinctrl_dev *pctldev,
					  unsigned selector)
{
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	return apmx->soc->functions[selector].name;
}

static int amlogic_pmx_get_groups(struct pinctrl_dev *pctldev,
			       unsigned selector,
			       const char * const **groups,
			       unsigned * const num_groups)
{
	struct amlogic_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	*groups = pmx->soc->functions[selector].groups;
	*num_groups = pmx->soc->functions[selector].num_groups;
	return 0;
}
static int amlogic_gpio_request_enable(struct pinctrl_dev *pctldev,
				    struct pinctrl_gpio_range *range,
				    unsigned offset)
{
	struct pin_desc *desc;
	struct amlogic_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	desc = pin_desc_get(pctldev, offset);
	if (desc->mux_owner) {
		pr_info("%s is using the pin %s as pinmux\n",
			desc->mux_owner, desc->name);
		return -EINVAL;
	}
	return	pmx->soc->meson_clear_pinmux(pmx, offset);
}
static int amlogic_pinctrl_request(struct pinctrl_dev *pctldev, unsigned offset)
{
	struct pin_desc *desc;
	desc = pin_desc_get(pctldev, offset);
	if (desc->gpio_owner) {
		pr_info("%s is using the pin %s as gpio\n",
			desc->gpio_owner, desc->name);
		return -EINVAL;
	}
	return 0;
}
static struct pinmux_ops amlogic_pmx_ops = {
	.get_functions_count = amlogic_pmx_get_funcs_count,
	.get_function_name = amlogic_pmx_get_func_name,
	.get_function_groups = amlogic_pmx_get_groups,
	.enable = amlogic_pmx_enable,
	.disable = amlogic_pmx_disable,
	.gpio_request_enable = amlogic_gpio_request_enable,
	.request = amlogic_pinctrl_request,
};

/*
 * GPIO ranges handled by the application-side amlogic GPIO controller
 * Very many pins can be converted into GPIO pins, but we only list those
 * that are useful in practice to cut down on tables.
 */

static struct pinctrl_gpio_range amlogic_gpio_ranges = {
	.name = "amlogic:gpio",
	.id = 0,
	.base = 0,
	.pin_base = 0,
};


int amlogic_pin_config_get(struct pinctrl_dev *pctldev,
			unsigned pin,
			unsigned long *config)
{
	return 0;
}

int amlogic_pin_config_set(struct pinctrl_dev *pctldev,
			       unsigned pin,
			       unsigned long *configs,
			       unsigned num_configs)
{
	return 0;
}
int meson_config_pullup(unsigned int pin,
					struct meson_domain *domain,
					struct meson_bank *bank,
					unsigned int config)
{
	int ret;
	u16 pullarg = AML_PINCONF_UNPACK_PULL_ARG(config);
	u16 pullen = AML_PINCONF_UNPACK_PULL_EN(config);
	unsigned int reg, bit;
	meson_calc_reg_and_bit(bank, pin, REG_PULLEN,
				   &reg, &bit);
	ret = regmap_update_bits(domain->reg_pullen, reg,
				 BIT(bit), pullen ? BIT(bit) : 0);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, pin, REG_PULL, &reg, &bit);
	ret = regmap_update_bits(domain->reg_pull, reg,
				 BIT(bit), pullarg ? BIT(bit) : 0);
	return ret;

}
int amlogic_pin_config_group_set(struct pinctrl_dev *pctldev,
				     unsigned group,
				     unsigned long *configs,
				     unsigned num_configs)
{
	unsigned reg, bit, ret =  -1, i;
	struct amlogic_pmx *apmx = pinctrl_dev_get_drvdata(pctldev);
	unsigned int config = configs[0];
	u16 pullparam = AML_PINCONF_UNPACK_PULL_PARA(config);
	u16 oenparam = AML_PINCONF_UNPACK_ENOUT_PARA(config);
	u16 oenarg = AML_PINCONF_UNPACK_ENOUT_ARG(config);
	const struct amlogic_pin_group *pin_group =  &apmx->soc->groups[group];
	const unsigned int *pins = pin_group->pins;
	const unsigned int num_pins = pin_group->num_pins;
	struct meson_domain *domain;
	struct meson_bank *bank;
	if (AML_PCON_PULLUP == pullparam) {
		for (i = 0; i < num_pins; i++) {
			ret = meson_get_domain_and_bank(apmx, pins[i],
							&domain, &bank);
			if (ret)
				return ret;
			ret = meson_config_pullup(pins[i],
						  domain, bank, config);
			if (ret)
				return ret;
		}
	}
	if (AML_PCON_ENOUT == oenparam) {
		for (i = 0; i < num_pins; i++) {
			ret = meson_get_domain_and_bank(apmx, pins[i],
							&domain, &bank);
			if (ret)
				return ret;
			meson_calc_reg_and_bit(bank, pins[i], REG_DIR,
				   &reg, &bit);
			ret =  regmap_update_bits(domain->reg_gpio, reg,
				BIT(bit), oenarg ? BIT(bit) : 0);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static struct pinconf_ops amlogic_pconf_ops = {
	.pin_config_set = amlogic_pin_config_set,
	.pin_config_get = amlogic_pin_config_get,
	.pin_config_group_set = amlogic_pin_config_group_set,
};

static struct pinctrl_desc amlogic_pmx_desc = {
	.name = "amlogic pinmux",
	.pctlops = &amlogic_pctrl_ops,
	.pmxops = &amlogic_pmx_ops,
	.confops = &amlogic_pconf_ops,
	.owner = THIS_MODULE,
};
static int amlogic_pinctrl_parse_group(struct platform_device *pdev,
				   struct device_node *np, int idx,
				   const char **out_name)
{
	struct amlogic_pmx *d = platform_get_drvdata(pdev);
	struct amlogic_pin_group *g = d->soc->groups;
	struct property *prop;
	const char *propname = "amlogic,pins";
	const char *pinctrl_set = "amlogic,setmask";
	const char *pinctrl_clr = "amlogic,clrmask";
	const char *gpioname;
	int length, ret = 0, i;
	u32 val;
	g = g+idx;
	g->name = np->name;
#if 0
/*read amlogic pins through num*/
	prop = of_find_property(np, propname, &length);
	if (!prop)
		return -EINVAL;
	g->num_pins = length / sizeof(u32);

	g->pins = devm_kzalloc(&pdev->dev, g->num_pins * sizeof(*g->pins),
			       GFP_KERNEL);
	if (!g->pins)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, propname, g->pins, g->num_pins);
	if (ret)
			return -EINVAL;
#endif
/*read amlogic pins through name*/
	g->num_pins = of_property_count_strings(np, propname);
	if (g->num_pins > 0) {
		g->pins = devm_kzalloc(&pdev->dev,
				       g->num_pins * sizeof(*g->pins),
				       GFP_KERNEL);
		if (!g->pins) {
			ret = -ENOMEM;
			dev_err(&pdev->dev, "malloc g->pins error\n");
			goto err;
		}
		for (i = 0; i < g->num_pins; i++) {
			ret = of_property_read_string_index(np, propname,
							    i, &gpioname);
			if (ret < 0) {
				ret = -EINVAL;
				dev_err(&pdev->dev, "read %s error\n",
					propname);
				goto err;
			}
			ret = d->soc->name_to_pin(gpioname);
			if (ret < 0) {
				ret = -EINVAL;
				dev_err(&pdev->dev,
					"%s change name to num  error\n",
					gpioname);
				goto err;
			}
			g->pins[i] = ret;
		}
	}
/*read amlogic set mask*/
	if (!of_property_read_u32(np, pinctrl_set, &val)) {
		prop = of_find_property(np, pinctrl_set, &length);
		if (!prop) {
			ret = -EINVAL;
			dev_err(&pdev->dev,
				"read %s length error\n", pinctrl_set);
			goto err;
		}
		g->num_setmask = length / sizeof(u32);
		if (g->num_setmask%d->pinmux_cell) {
			dev_err(&pdev->dev, "num_setmask error must be multiples of 2\n");
			g->num_setmask =
				(g->num_setmask/d->pinmux_cell)*d->pinmux_cell;
		}
		g->num_setmask = g->num_setmask/d->pinmux_cell;
		g->setmask = devm_kzalloc(&pdev->dev,
					g->num_setmask * sizeof(*g->setmask),
				       GFP_KERNEL);
		if (!g->setmask) {
			ret = -ENOMEM;
			dev_err(&pdev->dev, "malloc g->setmask error\n");
			goto err;
		}

		ret = of_property_read_u32_array(np, pinctrl_set,
						 (u32 *)(g->setmask),
						 length / sizeof(u32));
		if (ret) {
			ret = -EINVAL;
			dev_err(&pdev->dev, "read %s data error\n",
				pinctrl_set);
			goto err;
		}
	} else{
		g->setmask = NULL;
		g->num_setmask = 0;
	}
/*read clear mask*/
	if (!of_property_read_u32(np, pinctrl_clr, &val)) {
		prop = of_find_property(np, pinctrl_clr, &length);
		if (!prop) {
			dev_err(&pdev->dev, "read %s length error\n",
				pinctrl_clr);
			ret =  -EINVAL;
			goto err;
		}
		g->num_clearmask = length / sizeof(u32);
		if (g->num_clearmask%d->pinmux_cell) {
			dev_err(&pdev->dev,
				"num_setmask error must be multiples of 2\n");
			g->num_clearmask =
			(g->num_clearmask/d->pinmux_cell)*d->pinmux_cell;
		}
		g->num_clearmask = g->num_clearmask/d->pinmux_cell;
		g->clearmask = devm_kzalloc(&pdev->dev,
				g->num_clearmask * sizeof(*g->clearmask),
				       GFP_KERNEL);
		if (!g->clearmask) {
			ret =  -ENOMEM;
			dev_err(&pdev->dev, "malloc g->clearmask error\n");
			goto err;
		}
		ret = of_property_read_u32_array(np,
						 pinctrl_clr,
						 (u32 *)(g->clearmask),
						 length / sizeof(u32));
		if (ret) {
			ret = -EINVAL;
			dev_err(&pdev->dev, "read %s data error\n",
				pinctrl_clr);
			goto err;
		}
	} else{
		g->clearmask = NULL;
		g->num_clearmask = 0;
	}
	if (out_name)
		*out_name = g->name;
	return 0;
err:
	if (g->pins)
		devm_kfree(&pdev->dev, g->pins);
	if (g->setmask)
		devm_kfree(&pdev->dev, g->setmask);
	if (g->clearmask)
		devm_kfree(&pdev->dev, g->clearmask);
	return ret;
}

static int amlogic_pinctrl_probe_dt(struct platform_device *pdev,
				struct amlogic_pmx *d)
{
	struct amlogic_pinctrl_soc_data *soc = d->soc;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct amlogic_pmx_func *f;
	const char *pinctrl_set = "amlogic,setmask";
	const char *pinctrl_clr = "amlogic,clrmask";
	const char *fn, *fnull = "";
	int i = 0, idxf = 0, idxg = 0;
	int ret;
	u32 val;

	child = of_get_next_child(np, NULL);
	if (!child) {
		dev_err(&pdev->dev, "no group is defined\n");
		return -ENOENT;
	}

	/* Count total functions and groups */
	fn = fnull;

	for_each_child_of_node(np, child) {
		if (of_find_property(child, "gpio-controller", NULL))
			continue;
		soc->ngroups++;
		/* Skip pure pinconf node */
		if (of_property_read_u32(child, pinctrl_set, &val) &&
			of_property_read_u32(child, pinctrl_clr, &val))
			continue;
		if (strcmp(fn, child->name)) {
			fn = child->name;
			soc->nfunctions++;
		}
	}

	soc->functions = devm_kzalloc(&pdev->dev, soc->nfunctions *
				      sizeof(*soc->functions), GFP_KERNEL);
	if (!soc->functions) {
		dev_err(&pdev->dev, "malloc soc->functions error\n");
		ret =  -ENOMEM;
		goto err;
	}
	soc->groups = devm_kzalloc(&pdev->dev, soc->ngroups *
				   sizeof(*soc->groups), GFP_KERNEL);
	if (!soc->groups) {
		dev_err(&pdev->dev, "malloc soc->functions error\n");
		ret =  -ENOMEM;
		goto err;
	}
	/* Count groups for each function */
	fn = fnull;
	f = &soc->functions[idxf];
	for_each_child_of_node(np, child) {

		if (of_find_property(child, "gpio-controller", NULL))
			continue;

		if (of_property_read_u32(child, pinctrl_set, &val) &&
			of_property_read_u32(child, pinctrl_clr, &val))
			continue;
		if (strcmp(fn, child->name)) {
			f = &soc->functions[idxf++];
			f->name = fn = child->name;
		}
		f->num_groups++;
	};
	/* Get groups for each function */
	idxf = 0;
	fn = fnull;
	for_each_child_of_node(np, child) {

		if (of_find_property(child, "gpio-controller", NULL))
			continue;

		if (of_property_read_u32(child, pinctrl_set, &val) &&
			of_property_read_u32(child, pinctrl_clr, &val)) {
			ret = amlogic_pinctrl_parse_group(pdev, child,
						      idxg++, NULL);
			if (ret)
				goto err;
			continue;
		}

		if (strcmp(fn, child->name)) {
			f = &soc->functions[idxf++];
			f->groups = devm_kzalloc(&pdev->dev, f->num_groups *
						 sizeof(*f->groups),
						 GFP_KERNEL);
			if (!f->groups) {
				dev_err(&pdev->dev, "malloc f->groups error\n");
				ret =  -ENOMEM;
				goto err;
			}
			fn = child->name;
			i = 0;
		}
		ret = amlogic_pinctrl_parse_group(pdev, child, idxg++,
					      &f->groups[i++]);
		if (ret)
			goto  err;
	}
	return 0;
err:
	if (soc->groups)
		devm_kfree(&pdev->dev, soc->groups);
	if (soc->groups)
		devm_kfree(&pdev->dev, soc->groups);
	if (soc->functions)
		devm_kfree(&pdev->dev, soc->functions);
	return ret;
}
#ifdef AML_PIN_DEBUG_GUP
#include <linux/amlogic/gpio-amlogic.h>
/*extern struct pinctrl_pin_desc m8_pads[];*/
static void amlogic_dump_pinctrl_data(struct platform_device *pdev)
{
	struct amlogic_pmx *d = platform_get_drvdata(pdev);
	struct amlogic_pinctrl_soc_data *soc = d->soc;
	struct amlogic_pmx_func *func = soc->functions;
	struct amlogic_pin_group *groups = soc->groups;
	char **group;
	int i, j;
	for (i = 0; i < soc->nfunctions; func++, i++) {
		pr_info("function name:%s\n", func->name);
		group = (char **)(func->groups);
		for (j = 0; j < func->num_groups; group++, j++)
			pr_info("\tgroup in function:%s\n", group[j]);
	}
	for (i = 0; i < soc->ngroups; groups++, i++) {
		pr_info("group name:%s\n", groups->name);
		for (j = 0; j < groups->num_pins; j++) {
			pr_info("\t");
			pr_info("pin num=%s", m8_pads[groups->pins[j]].name);
		}
		pr_info("\n");
		for (j = 0; j < groups->num_setmask; j++) {
			pr_info("\t");
			pr_info("set reg=0x%x,mask=0x%x",
				groups->setmask[j].reg,
				groups->setmask[j].mask);
		}
		pr_info("\n");
		for (j = 0; j < groups->num_clearmask; j++) {
			pr_info("\t");
			pr_info("clear reg=0x%x,mask=0x%x",
				groups->clearmask[j].reg,
				groups->clearmask[j].mask);
		}
		pr_info("\n");
	}
}
#endif


static int meson_gpio_request(struct gpio_chip *chip, unsigned gpio)
{
	return pinctrl_request_gpio(chip->base + gpio);
}

static void meson_gpio_free(struct gpio_chip *chip, unsigned gpio)
{
	pinctrl_free_gpio(chip->base + gpio);
}

static int meson_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	struct meson_domain *domain = to_meson_domain(chip);
	unsigned int reg, bit, pin;
	struct meson_bank *bank;
	int ret;

	pin = domain->data->pin_base + gpio;

	ret = meson_get_bank(domain, pin, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, pin, REG_DIR, &reg, &bit);

	return regmap_update_bits(domain->reg_gpio, reg, BIT(bit), BIT(bit));
}

static int meson_gpio_direction_output(struct gpio_chip *chip, unsigned gpio,
				       int value)
{
	struct meson_domain *domain = to_meson_domain(chip);
	unsigned int reg, bit, pin;
	struct meson_bank *bank;
	int ret;
	int odval;

	if (gl_pmx->soc->is_od_domain) {
		odval = gl_pmx->soc->is_od_domain(gpio);
		if (unlikely(odval) && (value == 1)) {
			pr_info("change od high_level\n");
			meson_gpio_direction_input(chip, gpio);
			return 0;
		}
	}

	pin = domain->data->pin_base + gpio;
	ret = gl_pmx->soc->soc_extern_gpio_output(domain, pin, value);
	if (ret == 0)
		return 0;

	ret = meson_get_bank(domain, pin, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, pin, REG_DIR, &reg, &bit);
	ret = regmap_update_bits(domain->reg_gpio, reg, BIT(bit), 0);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, pin, REG_OUT, &reg, &bit);

	return regmap_update_bits(domain->reg_gpio, reg, BIT(bit),
				  value ? BIT(bit) : 0);
}

static void meson_gpio_set(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct meson_domain *domain = to_meson_domain(chip);
	unsigned int reg, bit, pin;
	struct meson_bank *bank;
	int ret;
	int odval;

	if (gl_pmx->soc->is_od_domain) {
		odval = gl_pmx->soc->is_od_domain(gpio);
		if (unlikely(odval)) {
			if (value == 1) {
				pr_info("change od high_level\n");
				meson_gpio_direction_output(chip, gpio, 1);
				return;
			} else if (value == 0) {
				pr_info("change od low_level\n");
				meson_gpio_direction_output(chip, gpio, 0);
				return;
			}
		}
	}

	pin = domain->data->pin_base + gpio;

	ret = gl_pmx->soc->soc_extern_gpio_output(domain, pin, value);
	if (ret == 0)
		return;

	ret = meson_get_bank(domain, pin, &bank);
	if (ret)
		return;

	meson_calc_reg_and_bit(bank, pin, REG_OUT, &reg, &bit);
	regmap_update_bits(domain->reg_gpio, reg, BIT(bit),
			   value ? BIT(bit) : 0);
}

static int meson_gpio_get(struct gpio_chip *chip, unsigned gpio)
{
	struct meson_domain *domain = to_meson_domain(chip);
	unsigned int reg, bit, val, pin;
	struct meson_bank *bank;
	int ret;

	pin = domain->data->pin_base + gpio;
	ret = gl_pmx->soc->soc_extern_gpio_get(domain, pin);
	if (ret != -1)
		return ret;

	ret = meson_get_bank(domain, pin, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, pin, REG_IN, &reg, &bit);
	regmap_read(domain->reg_gpio, reg, &val);
	return !!(val & BIT(bit));
}
static int meson_gpio_set_pullup_down(struct gpio_chip *chip,
				      unsigned  int gpio, int val)
{
	struct meson_domain *domain = to_meson_domain(chip);
	unsigned int pin;
	struct meson_bank *bank;
	int ret, config;

	pin = domain->data->pin_base + gpio;
	ret = meson_get_bank(domain, pin, &bank);
	if (ret)
		return ret;
	config = AML_PINCONF_PACK_PULL(AML_PCON_PULLUP, val);
	config |= AML_PINCONF_PACK_PULLEN(AML_PCON_PULLUP, 1);
	meson_config_pullup(pin, domain, bank, config);
	return 0;
}
static int meson_gpio_to_irq(struct gpio_chip *chip,
			     unsigned int gpio, unsigned gpio_flag)
{
	unsigned start_bit;
	unsigned irq_bank = gpio_flag&0x7;
	unsigned filter = (gpio_flag>>8)&0x7;
	unsigned irq_type = (gpio_flag>>16)&0x3;
	unsigned int pin;
	unsigned type[] = {0x0,	/*GPIO_IRQ_HIGH*/
				0x10000, /*GPIO_IRQ_LOW*/
				0x1,	/*GPIO_IRQ_RISING*/
				0x10001, /*GPIO_IRQ_FALLING*/
				};
	 /*set trigger type*/
	struct meson_domain *domain = to_meson_domain(chip);
	 pin = domain->data->pin_base + gpio;
	 regmap_update_bits(int_reg, (GPIO_EDGE * 4),
						0x10001<<irq_bank,
						type[irq_type]<<irq_bank);
	/*select pin*/
	start_bit = (irq_bank&3)*8;
	regmap_update_bits(int_reg,
			irq_bank < 4?(GPIO_SELECT_0_3*4):(GPIO_SELECT_4_7*4),
			0xff<<start_bit,
			pin << start_bit);
	/*set filter*/
	start_bit = (irq_bank)*4;

	regmap_update_bits(int_reg,  (GPIO_FILTER_NUM*4),
			0x7<<start_bit, filter<<start_bit);
	return 0;
}
static int meson_gpio_mask_irq(struct gpio_chip *chip,
			     unsigned int gpio, unsigned gpio_flag)
{
	unsigned start_bit;
	unsigned irq_bank = gpio_flag&0x7;
	unsigned filter = (gpio_flag>>8)&0x7;
	unsigned irq_type = (gpio_flag>>16)&0x3;
	unsigned int pin;
	unsigned type[] = {0x0,	/*GPIO_IRQ_HIGH*/
				0x10000, /*GPIO_IRQ_LOW*/
				0x1,	/*GPIO_IRQ_RISING*/
				0x10001, /*GPIO_IRQ_FALLING*/
				};
	 /*set trigger type*/
	 pin = 0xff;
	 regmap_update_bits(int_reg, (GPIO_EDGE * 4),
						0x10001<<irq_bank,
						type[irq_type]<<irq_bank);
	/*select pin*/
	start_bit = (irq_bank&3)*8;
	regmap_update_bits(int_reg,
			irq_bank < 4?(GPIO_SELECT_0_3*4):(GPIO_SELECT_4_7*4),
			0xff<<start_bit,
			pin << start_bit);
	/*set filter*/
	start_bit = (irq_bank)*4;

	regmap_update_bits(int_reg,  (GPIO_FILTER_NUM*4),
			0x7<<start_bit, filter<<start_bit);
	return 0;
}

struct pinctrl_dev *pctl;
static int meson_gpiolib_register(struct amlogic_pmx  *pc)
{
	struct meson_domain *domain;
	int i, ret;

	for (i = 0; i < pc->soc->num_domains; i++) {
		domain = &pc->domains[i];

		domain->chip.label = domain->data->name;
		domain->chip.dev = pc->dev;
		domain->chip.request = meson_gpio_request;
		domain->chip.free = meson_gpio_free;
		domain->chip.direction_input = meson_gpio_direction_input;
		domain->chip.direction_output = meson_gpio_direction_output;
		domain->chip.get = meson_gpio_get;
		domain->chip.set = meson_gpio_set;
		domain->chip.set_pullup_down = meson_gpio_set_pullup_down;
		domain->chip.set_gpio_to_irq = meson_gpio_to_irq;
		domain->chip.mask_gpio_irq = meson_gpio_mask_irq;
		domain->chip.base = -1;
		domain->chip.ngpio = domain->data->num_pins;
		domain->chip.can_sleep = false;
		domain->chip.of_node = domain->of_node;
		domain->chip.of_gpio_n_cells = 2;

		ret = gpiochip_add(&domain->chip);
		if (ret) {
			dev_err(pc->dev, "can't add gpio chip %s\n",
				domain->data->name);
			goto fail;
		}

		ret = gpiochip_add_pin_range(&domain->chip, dev_name(pc->dev),
					     0, domain->data->pin_base,
					     domain->chip.ngpio);
		if (ret) {
			dev_err(pc->dev, "can't add pin range\n");
			goto fail;
		}
	}

	return 0;
fail:
	return ret;
}

static struct meson_domain_data *meson_get_domain_data(struct amlogic_pmx *pc,
						       struct device_node *np)
{
	int i;

	for (i = 0; i < pc->soc->num_domains; i++) {
		if (!strcmp(np->name, pc->soc->domain_data[i].name))
			return &pc->soc->domain_data[i];
	}

	return NULL;
}

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static struct regmap *meson_map_resource(struct amlogic_pmx *pc,
					 struct device_node *node, char *name)
{
	struct resource res;
	void __iomem *base;
	int i;

	i = of_property_match_string(node, "reg-names", name);
	if (of_address_to_resource(node, i, &res))
		return ERR_PTR(-ENOENT);

	base = devm_ioremap_resource(pc->dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	meson_regmap_config.max_register = resource_size(&res) - 4;
	meson_regmap_config.name = kasprintf(GFP_KERNEL,
						  "%s-%s", node->name,
						  name);
	if (!meson_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(pc->dev, base, &meson_regmap_config);
}

static struct regmap *meson_map(struct device *dev,
					 struct device_node *node, char *name)
{
	struct resource res;
	void __iomem *base;
	if (of_address_to_resource(node, 0, &res))
		return ERR_PTR(-ENOENT);

	base = devm_ioremap_resource(dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	meson_regmap_config.max_register = resource_size(&res) - 4;
	meson_regmap_config.name = kasprintf(GFP_KERNEL,
						  "%s-%s", node->name,
						  name);
	if (!meson_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(dev, base, &meson_regmap_config);
}

static int meson_pinctrl_parse_dt(struct amlogic_pmx  *pc,
				  struct device_node *node)
{
	struct device_node *np;
	struct meson_domain *domain;
	int i = 0, num_domains = 0;

	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;
		num_domains++;
	}

	if (num_domains != pc->soc->num_domains) {
		dev_err(pc->dev, "wrong number of subnodes\n");
		return -EINVAL;
	}

	pc->domains = devm_kzalloc(pc->dev, num_domains *
				   sizeof(struct meson_domain), GFP_KERNEL);
	if (!pc->domains)
		return -ENOMEM;

	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;

		domain = &pc->domains[i];

		domain->data = meson_get_domain_data(pc, np);
		if (!domain->data) {
			dev_err(pc->dev, "domain data not found for node %s\n",
				np->name);
			return -ENODEV;
		}

		domain->of_node = np;

		domain->reg_mux = meson_map_resource(pc, np, "mux");
		if (IS_ERR(domain->reg_mux)) {
			dev_err(pc->dev, "mux registers not found\n");
			return PTR_ERR(domain->reg_mux);
		}

		domain->reg_pull = meson_map_resource(pc, np, "pull");
		if (IS_ERR(domain->reg_pull)) {
			dev_err(pc->dev, "pull registers not found\n");
			return PTR_ERR(domain->reg_pull);
		}

		domain->reg_pullen = meson_map_resource(pc, np, "pull-enable");
		/* Use pull region if pull-enable one is not present */
		if (IS_ERR(domain->reg_pullen))
			domain->reg_pullen = domain->reg_pull;

		domain->reg_gpio = meson_map_resource(pc, np, "gpio");
		if (IS_ERR(domain->reg_gpio)) {
			dev_err(pc->dev, "gpio registers not found\n");
			return PTR_ERR(domain->reg_gpio);
		}

		i++;
	}

	return 0;
}

int  amlogic_pmx_probe(struct platform_device *pdev,
			struct amlogic_pinctrl_soc_data *soc_data)
{
	struct amlogic_pmx *apmx;
	int ret, val;
	dev_info(&pdev->dev, "Init pinux probe!\n");
	apmx = devm_kzalloc(&pdev->dev, sizeof(*apmx), GFP_KERNEL);
	if (!apmx) {
		dev_err(&pdev->dev, "Can't alloc amlogic_pmx\n");
		return -ENOMEM;
	}
	apmx->dev = &pdev->dev;
	apmx->soc = soc_data;
	platform_set_drvdata(pdev, apmx);

	ret = meson_pinctrl_parse_dt(apmx, pdev->dev.of_node);
	if (ret)
		return ret;
	ret = of_property_read_u32(pdev->dev.of_node, "#pinmux-cells", &val);
	if (ret) {
		dev_err(&pdev->dev, "dt probe #pinmux-cells failed: %d\n", ret);
		goto err;
	}
	apmx->pinmux_cell = val;

	ret = amlogic_pinctrl_probe_dt(pdev, apmx);
	if (ret) {
		dev_err(&pdev->dev, "dt probe failed: %d\n", ret);
		goto err;
	}
#ifdef AML_PIN_DEBUG_GUP
	amlogic_dump_pinctrl_data(pdev);
#endif
	amlogic_gpio_ranges.npins = apmx->soc->npins;
	amlogic_pmx_desc.name = dev_name(&pdev->dev);
	amlogic_pmx_desc.pins = apmx->soc->pins;
	amlogic_pmx_desc.npins = apmx->soc->npins;

	apmx->pctl = pinctrl_register(&amlogic_pmx_desc, &pdev->dev, apmx);

	if (!apmx->pctl) {
		dev_err(&pdev->dev, "Couldn't register pinctrl driver\n");
		goto err;
	}
	ret = meson_gpiolib_register(apmx);
	if (ret) {
		pinctrl_unregister(apmx->pctl);
		return ret;
	}
	/* pinctrl_add_gpio_range(apmx->pctl, &amlogic_gpio_ranges); */
	pctdev_name = dev_name(&pdev->dev);
	pinctrl_provide_dummies();
	dev_info(&pdev->dev, "Probed amlogic pinctrl driver\n");
	pctl = apmx->pctl;
	int_reg = meson_map(&pdev->dev, pdev->dev.of_node, "Int");
	gl_pmx = apmx;
	return 0;
err:
	devm_kfree(&pdev->dev, apmx);
	return ret;
}
EXPORT_SYMBOL_GPL(amlogic_pmx_probe);

int amlogic_pmx_remove(struct platform_device *pdev)
{
	struct amlogic_pmx *pmx = platform_get_drvdata(pdev);
	pinctrl_unregister(pmx->pctl);
	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_pmx_remove);

