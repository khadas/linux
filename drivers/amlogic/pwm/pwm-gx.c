/*
 * drivers/amlogic/pwm/pwm-gx.c
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
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of_address.h>

#include <linux/amlogic/cpu_version.h>

#define REG_PWM_A			0x0
#define REG_PWM_B			0x4
#define REG_MISC_AB		0x8
#define REG_DS_A_B			0xc

#define REG_PWM_C			0x100
#define REG_PWM_D			0x104
#define REG_MISC_CD		0x108
#define REG_DS_C_D			0x10c

#define REG_PWM_E			0x170
#define REG_PWM_F			0x174
#define REG_MISC_EF		0x178
#define REG_DS_E_F			0x17c

#define REG_PWM_AO_A		0x0
#define REG_PWM_AO_B		0x4
#define REG_MISC_AO_AB		0x8
#define REG_DS_AO_A_B		0xc

#define FIN_FREQ		(24 * 1000)
#define DUTY_MAX		1024

#define AML_PWM_NUM		8

enum pwm_channel {
	PWM_A = 0,
	PWM_B,
	PWM_C,
	PWM_D,
	PWM_E,
	PWM_F,
	PWM_AO_A,
	PWM_AO_B,
};

static DEFINE_SPINLOCK(aml_pwm_lock);

/*pwm att*/
struct aml_pwm_channel {
	u8 pwm_hi;
	u8 pwm_lo;
	u8 pwm_pre_div;
	u32 period;
	u32 duty;
};

/*pwm regiset att*/
struct aml_pwm_variant {
	u8 output_mask;
};

struct aml_pwm_chip {
	struct pwm_chip chip;
	void __iomem *base;
	void __iomem *ao_base;
	struct aml_pwm_variant variant;
	u8 inverter_mask;
};

static void pwm_set_reg_bits(void __iomem  *reg,
						unsigned int mask,
						const unsigned int val)
{
	unsigned int tmp, orig;
	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

static inline void pwm_write_reg(void __iomem *reg,
						const unsigned int  val)
{
	unsigned int tmp, orig;
	orig = readl(reg);
	tmp = orig & ~(0xffffffff);
	tmp |= val;
	writel(tmp, reg);
};

static inline
struct aml_pwm_chip *to_aml_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct aml_pwm_chip, chip);
}

static
struct aml_pwm_channel *pwm_aml_calc(struct aml_pwm_chip *chip,
						struct pwm_device *pwm,
						int duty, unsigned int pwm_freq)
{
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	unsigned int fout_freq = 0, pwm_pre_div;
	unsigned int i = 0;
	unsigned int temp = 0;
	unsigned int pwm_cnt;

	if ((duty < 0) || (duty > DUTY_MAX)) {
		dev_err(chip->chip.dev, "Not available duty error!\n");
		return NULL;
	}

	fout_freq = ((pwm_freq >= (FIN_FREQ * 500)) ?
					(FIN_FREQ * 500) : pwm_freq);
	for (i = 0; i < 0x7f; i++) {
		pwm_pre_div = i;
		pwm_cnt = FIN_FREQ * 1000 / (pwm_freq * (pwm_pre_div + 1)) - 2;
		if (pwm_cnt <= 0xffff)
			break;
	}

	our_chan->pwm_pre_div = pwm_pre_div;
	if (duty == 0) {
		our_chan->pwm_hi = 0;
		our_chan->pwm_lo = pwm_cnt;
		return our_chan;
	} else if (duty == DUTY_MAX) {
		our_chan->pwm_hi = pwm_cnt;
		our_chan->pwm_lo = 0;
		return our_chan;
	}

	temp = pwm_cnt * duty;
	temp /= DUTY_MAX;

	our_chan->pwm_hi = (unsigned int)temp;
	our_chan->pwm_lo = pwm_cnt - our_chan->pwm_hi;

	return our_chan;

}

static int pwm_aml_request(struct pwm_chip *chip,
						struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	struct aml_pwm_channel *our_chan;

	if (!(our_chip->variant.output_mask & BIT(pwm->hwpwm))) {
		dev_warn(chip->dev,
			"tried to request PWM channel %d without output\n",
			pwm->hwpwm);
		return -EINVAL;
	}

	our_chan = devm_kzalloc(chip->dev, sizeof(*our_chan), GFP_KERNEL);
	if (!our_chan)
		return -ENOMEM;

	pwm_set_chip_data(pwm, our_chan);

	return 0;
}

static void pwm_aml_free(struct pwm_chip *chip,
							struct pwm_device *pwm)
{
	devm_kfree(chip->dev, pwm_get_chip_data(pwm));
	pwm_set_chip_data(pwm, NULL);
}

static int pwm_aml_enable(struct pwm_chip *chip,
							struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned int id = pwm->hwpwm;
	unsigned long flags;

	spin_lock_irqsave(&aml_pwm_lock, flags);
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
		switch (id) {
		case PWM_A:
			pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
							(1 << 0), (1 << 0));
			break;
		case PWM_B:
			pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
							(1 << 1), (1 << 1));
			break;
		case PWM_C:
			pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
							(1 << 0), (1 << 0));
			break;
		case PWM_D:
			pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
							(1 << 1), (1 << 1));
			break;
		case PWM_E:
			pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
							(1 << 0), (1 << 0));
			break;
		case PWM_F:
			pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
							(1 << 1), (1 << 1));
			break;
		case PWM_AO_A:
			pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
							(1 << 0), (1 << 0));
			break;
		case PWM_AO_B:
			pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
							(1 << 1), (1 << 1));
			break;
		default:
		break;
		}
	} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) {
		switch (id) {
		case PWM_A:
		case PWM_B:
			pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
							(0x3 << 0), (0x3 << 0));
			break;
		case PWM_C:
		case PWM_D:
			pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
							(0x3 << 0), (0x3 << 0));
			break;
		case PWM_E:
		case PWM_F:
			pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
							(0x3 << 0), (0x3 << 0));
			break;
		case PWM_AO_A:
		case PWM_AO_B:
			pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
							(0x3 << 0), (0x3 << 0));
			break;
		default:
		break;
		}
	}
	spin_unlock_irqrestore(&aml_pwm_lock, flags);

	return 0;
}

static void pwm_aml_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned int id = pwm->hwpwm;
	unsigned long flags;

	spin_lock_irqsave(&aml_pwm_lock, flags);
	switch (id) {
	case PWM_A:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
						(1 << 0), (0 << 0));
		break;
	case PWM_B:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
						(1 << 1), (0 << 1));
		break;
	case PWM_C:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
						(1 << 0), (0 << 0));
		break;
	case PWM_D:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
						(1 << 1), (0 << 1));
		break;
	case PWM_E:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
						(1 << 0), (0 << 0));
		break;
	case PWM_F:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
						(1 << 1), (0 << 1));
		break;
	case PWM_AO_A:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
						(1 << 0), (0 << 0));
		break;
	case PWM_AO_B:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
						(1 << 1), (0 << 1));
		break;
	default:
	break;
	}
	spin_unlock_irqrestore(&aml_pwm_lock, flags);

}

static int pwm_aml_config(struct pwm_chip *chip,
							struct pwm_device *pwm,
							int duty,
							int pwm_freq)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	unsigned int id = pwm->hwpwm;

	if (pwm_freq == our_chan->period && duty == our_chan->duty)
		return 0;

	our_chan = pwm_aml_calc(our_chip, pwm, duty, pwm_freq);
	if (NULL == our_chan) {
		dev_err(chip->dev, "tried to calc pwm freq err\n");
		return -EINVAL;
	}
	switch (id) {
	case PWM_A:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x7 << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_A,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_B:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x7 << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_B,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_C:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x7 << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_C,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_D:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x7 << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_D,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_E:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x7 << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_E,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_F:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x7 << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_E,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_AO_A:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x7 << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->ao_base + REG_PWM_AO_A,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_AO_B:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x7 << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->ao_base + REG_PWM_AO_B,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	default:
	break;
	}

	our_chan->period = pwm_freq;
	our_chan->duty = duty;

	return 0;
}

static void pwm_aml_set_invert(struct pwm_chip *chip, struct pwm_device *pwm,
				unsigned int channel, bool invert)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned long flags;
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);

	spin_lock_irqsave(&aml_pwm_lock, flags);
	if (invert) {
		our_chip->inverter_mask |= BIT(channel);
		pwm_aml_config(chip, pwm, our_chan->duty,
						our_chan->period);

	} else {
		our_chip->inverter_mask &= ~BIT(channel);
		pwm_aml_config(chip, pwm, (DUTY_MAX - our_chan->duty),
						our_chan->period);
	}
	spin_unlock_irqrestore(&aml_pwm_lock, flags);
}


static int pwm_aml_set_polarity(struct pwm_chip *chip,
				    struct pwm_device *pwm,
				    enum pwm_polarity polarity)
{
	bool invert = (polarity == PWM_POLARITY_NORMAL);

	/* Inverted means normal in the hardware. */
	pwm_aml_set_invert(chip, pwm, pwm->hwpwm, invert);

	return 0;
}

static const struct pwm_ops pwm_aml_ops = {
	.request	= pwm_aml_request,
	.free		= pwm_aml_free,
	.enable		= pwm_aml_enable,
	.disable	= pwm_aml_disable,
	.config		= pwm_aml_config,
	.set_polarity	= pwm_aml_set_polarity,
	.owner		= THIS_MODULE,
};

#ifdef CONFIG_OF
static const struct of_device_id aml_pwm_matches[] = {
	{ .compatible = "amlogic, gx-pwm", },
	{},
};

static int pwm_aml_parse_dt(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;
	const struct of_device_id *match;
	struct clk *clk;

	struct property *prop;
	const __be32 *cur;
	u32 val;

	match = of_match_node(aml_pwm_matches, np);
	if (!match)
		return -ENODEV;

	chip->base = of_iomap(chip->chip.dev->of_node, 0);
	if (IS_ERR(chip->base))
		return PTR_ERR(chip->base);
	chip->ao_base = of_iomap(chip->chip.dev->of_node, 1);
	if (IS_ERR(chip->ao_base))
		return PTR_ERR(chip->ao_base);

	clk = clk_get(chip->chip.dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(chip->chip.dev, "failed to get timer base clk\n");
		return PTR_ERR(clk);
	}

	of_property_for_each_u32(np, "pwm-outputs", prop, cur, val) {
		if (val >= AML_PWM_NUM) {
			dev_err(chip->chip.dev,
				"%s: invalid channel index in pwm-outputs property\n",
								__func__);
			continue;
		}
		chip->variant.output_mask |= BIT(val);
	}

	return 0;
}
#else
static int pwm_aml_parse_dt(struct aml_pwm_chip *chip)
{
	return -ENODEV;
}
#endif

static int pwm_aml_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_pwm_chip *chip;
	int ret;

	pr_info("aml pwm probe\n");
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		dev_err(dev, "pwm alloc MEMORY err!!\n");
		return -ENOMEM;
	}

	chip->chip.dev = &pdev->dev;
	chip->chip.ops = &pwm_aml_ops;
	chip->chip.base = -1;
	chip->chip.npwm = AML_PWM_NUM;
	chip->inverter_mask = BIT(AML_PWM_NUM) - 1;

	if (IS_ENABLED(CONFIG_OF) && pdev->dev.of_node) {
		ret = pwm_aml_parse_dt(chip);
		if (ret)
			return ret;
	} else {
		if (!pdev->dev.platform_data) {
			dev_err(&pdev->dev, "no platform data specified\n");
			return -EINVAL;
		}
		memcpy(&chip->variant, pdev->dev.platform_data,
							sizeof(chip->variant));
	}

	platform_set_drvdata(pdev, chip);

	ret = pwmchip_add(&chip->chip);
	if (ret < 0) {
		dev_err(dev, "failed to register PWM chip\n");
		return ret;
	}

	return 0;
}

static int pwm_aml_remove(struct platform_device *pdev)
{
	struct aml_pwm_chip *chip = platform_get_drvdata(pdev);
	int ret;

	ret = pwmchip_remove(&chip->chip);
	if (ret < 0)
		return ret;
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pwm_aml_suspend(struct device *dev)
{
	struct aml_pwm_chip *chip = dev_get_drvdata(dev);
	unsigned int i;

/*
	 * No one preserves these values during suspend so reset them.
	 * Otherwise driver leaves PWM unconfigured if same values are
	 * passed to pwm_config() next time.
*/
	for (i = 0; i < AML_PWM_NUM; ++i) {
		struct pwm_device *pwm = &chip->chip.pwms[i];
		struct aml_pwm_channel *chan = pwm_get_chip_data(pwm);
		if (!chan)
			continue;

		chan->period = 0;
		chan->duty = 0;
	}

	return 0;
}

static int pwm_aml_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops pwm_aml_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pwm_aml_suspend, pwm_aml_resume)
};

static struct platform_driver pwm_aml_driver = {
	.driver		= {
		.name	= "aml-pwm",
		.owner	= THIS_MODULE,
		.pm	= &pwm_aml_pm_ops,
		.of_match_table = of_match_ptr(aml_pwm_matches),
	},
	.probe		= pwm_aml_probe,
	.remove		= pwm_aml_remove,
};
module_platform_driver(pwm_aml_driver);

MODULE_ALIAS("platform:meson-pwm");
MODULE_LICENSE("GPL");
