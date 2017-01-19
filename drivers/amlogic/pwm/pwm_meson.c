/*
 * drivers/amlogic/pwm/pwm-meson.c
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

#undef pr_fmt
#define pr_fmt(fmt) "pwm: " fmt


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
#include <linux/amlogic/pwm_meson.h>


static DEFINE_SPINLOCK(aml_pwm_lock);


void pwm_set_reg_bits(void __iomem  *reg,
						unsigned int mask,
						const unsigned int val)
{
	unsigned int tmp, orig;
	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

void pwm_write_reg(void __iomem *reg,
						const unsigned int  val)
{
	unsigned int tmp, orig;
	orig = readl(reg);
	tmp = orig & ~(0xffffffff);
	tmp |= val;
	writel(tmp, reg);
};

void pwm_clear_reg_bits(void __iomem *reg, const unsigned int val)
{
	unsigned int tmp, orig;
	orig = readl(reg);
	tmp = orig & ~val;
	writel(tmp, reg);
}

void pwm_write_reg1(void __iomem *reg, const unsigned int  val)
{
	unsigned int tmp = 0, orig;
	orig = readl(reg);
	tmp = orig;
	tmp |= val;
	writel(tmp, reg);
};

struct aml_pwm_chip *to_aml_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct aml_pwm_chip, chip);
}

static
struct aml_pwm_channel *pwm_aml_calc(struct aml_pwm_chip *chip,
						struct pwm_device *pwm,
						unsigned int duty_ns,
						unsigned int period_ns,
						struct clk	*clk)
{
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	unsigned int fout_freq = 0, pwm_pre_div = 0;
	unsigned int i = 0;
	unsigned long  temp = 0;
	unsigned long pwm_cnt = 0;
	unsigned long rate = 0;
	unsigned int pwm_freq;
	unsigned long freq_div;

	if ((duty_ns < 0) || (duty_ns > period_ns)) {
		dev_err(chip->chip.dev, "Not available duty error!\n");
		return NULL;
	}

	if (!IS_ERR(clk))
		rate = clk_get_rate(clk);

	pwm_freq = NSEC_PER_SEC / period_ns;

	fout_freq = ((pwm_freq >= ((rate/1000) * 500)) ?
					((rate/1000) * 500) : pwm_freq);
	for (i = 0; i < 0x7f; i++) {
		pwm_pre_div = i;
		freq_div = rate / (pwm_pre_div + 1);
		if (freq_div < pwm_freq)
			continue;
		pwm_cnt = freq_div / pwm_freq;
		if (pwm_cnt <= 0xffff)
			break;
	}

	our_chan->pwm_pre_div = pwm_pre_div;
	if (duty_ns == 0) {
		our_chan->pwm_hi = 0;
		our_chan->pwm_lo = pwm_cnt;
		return our_chan;
	} else if (duty_ns == period_ns) {
		our_chan->pwm_hi = pwm_cnt;
		our_chan->pwm_lo = 0;
		return our_chan;
	}

	temp = (unsigned long)(pwm_cnt * duty_ns);
	temp /= period_ns;
	our_chan->pwm_hi = (unsigned int)temp-1;
	our_chan->pwm_lo = pwm_cnt - (unsigned int)temp-1;
	our_chan->pwm_freq = pwm_freq;

	return our_chan;

}

static int pwm_aml_request(struct pwm_chip *chip,
						struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	struct aml_pwm_channel *our_chan;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		if (!(our_chip->variant.output_mask_new & BIT(pwm->hwpwm))) {
			dev_warn(chip->dev,
			"tried to request PWM channel %d without output\n",
			pwm->hwpwm);
			return -EINVAL;
		}
	} else {
		if (!(our_chip->variant.output_mask & BIT(pwm->hwpwm))) {
			dev_warn(chip->dev,
			"tried to request PWM channel %d without output\n",
			pwm->hwpwm);
			return -EINVAL;
		}
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

static void pwm_gxbb_enable(struct aml_pwm_chip *our_chip,
								unsigned int id)
{
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

}

/*
 *do it for hardware defect,
 * PWM_A and PWM_B enable bit should be setted together.
*/
static void pwm_gxtvbb_enable(struct aml_pwm_chip *our_chip,
		unsigned int id)
{
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

	case PWM_A2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
							(1 << 25), (1 << 25));
		break;
	case PWM_B2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
							(1 << 24), (1 << 24));
		break;
	case PWM_C2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
							(1 << 25), (1 << 25));
		break;
	case PWM_D2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
							(1 << 24), (1 << 24));
		break;
	case PWM_E2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
							(1 << 25), (1 << 25));
		break;
	case PWM_F2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
							(1 << 24), (1 << 24));
		break;
	case PWM_AO_A2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
							(1 << 25), (1 << 25));
		break;
	case PWM_AO_B2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
							(1 << 24), (1 << 24));
		break;

	default:
	break;
	}

}

static void pwm_txl_enable(struct aml_pwm_chip *our_chip,
								unsigned int id)
{
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
	case PWM_A2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
						(1 << 25), (1 << 25));
		break;
	case PWM_B2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
						(1 << 24), (1 << 24));
		break;
	case PWM_C2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
						(1 << 25), (1 << 25));
		break;
	case PWM_D2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
						(1 << 24), (1 << 24));
		break;
	case PWM_E2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
						(1 << 25), (1 << 25));
		break;
	case PWM_F2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
						(1 << 24), (1 << 24));
		break;
	case PWM_AO_A2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
						(1 << 25), (1 << 25));
		break;
	case PWM_AO_B2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
						(1 << 24), (1 << 24));
		break;
	default:
	break;
	}
}


static int pwm_aml_enable(struct pwm_chip *chip,
							struct pwm_device *pwm)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned int id = pwm->hwpwm;
	unsigned long flags;

	spin_lock_irqsave(&aml_pwm_lock, flags);
	if (is_meson_gxbb_cpu())
		pwm_gxbb_enable(our_chip, id);
	else if (is_meson_gxtvbb_cpu())
		pwm_gxtvbb_enable(our_chip, id);
	else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXL))
		pwm_txl_enable(our_chip, id);

	spin_unlock_irqrestore(&aml_pwm_lock, flags);
	return 0;
}

static void pwm_aml_disable(struct pwm_chip *chip,
						struct pwm_device *pwm)
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
	case PWM_A2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
						(25 << 1), (0 << 25));
		break;
	case PWM_B2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
						(24 << 1), (0 << 24));
		break;
	case PWM_C2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
						(1 << 25), (0 << 25));
		break;
	case PWM_D2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
						(1 << 25), (0 << 24));
		break;
	case PWM_E2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
						(1 << 25), (0 << 25));
		break;
	case PWM_F2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
						(1 << 24), (0 << 24));
		break;
	case PWM_AO_A2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
						(1 << 25), (0 << 25));
		break;
	case PWM_AO_B2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
						(1 << 24), (0 << 24));
		break;
	default:
	break;
	}
	spin_unlock_irqrestore(&aml_pwm_lock, flags);

}

static int pwm_aml_clk(struct aml_pwm_chip *our_chip,
						struct pwm_device *pwm,
						unsigned int duty_ns,
						unsigned int period_ns,
						unsigned int offset)
{
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	struct clk	*clk;

	switch ((our_chip->clk_mask >> offset)&0x3) {
	case 0x0:
		clk = our_chip->xtal_clk;
		break;
	case 0x1:
		clk = our_chip->vid_pll_clk;
		break;
	case 0x2:
		clk = our_chip->fclk_div4_clk;
		break;
	case 0x3:
		clk = our_chip->fclk_div3_clk;
		break;
	default:
		clk = our_chip->xtal_clk;
		break;
	}

	our_chan = pwm_aml_calc(our_chip, pwm, duty_ns, period_ns, clk);
	if (NULL == our_chan)
		return -EINVAL;

	return 0;
}

/*
 * 8 base channels configuration for gxbb, gxtvbb, gxl, gxm and txl
 */
static void pwm_meson_config(struct aml_pwm_chip *our_chip,
			struct aml_pwm_channel *our_chan,
			unsigned int id)
{
	switch (id) {
	case PWM_A:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x3 << 4),
			(((our_chip->clk_mask)&0x3) << 4));
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_A,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_B:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x3 << 6),
			(((our_chip->clk_mask >> 2)&0x3) << 6));
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_B,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_C:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x3 << 4),
			(((our_chip->clk_mask >> 4)&0x3) << 4));
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_C,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_D:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x3 << 6),
			(((our_chip->clk_mask >> 6)&0x3) << 6));
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_D,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_E:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x3 << 4),
			(((our_chip->clk_mask >> 8)&0x3) << 4));
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_E,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_F:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x3 << 6),
			(((our_chip->clk_mask >> 10)&0x3) << 6));
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_F,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_AO_A:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x3 << 4),
			(((our_chip->clk_mask >> 12)&0x3) << 4));
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->ao_base + REG_PWM_AO_A,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_AO_B:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x3 << 6),
			(((our_chip->clk_mask >> 14)&0x3) << 6));
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->ao_base + REG_PWM_AO_B,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	default:
	break;
	}
}

/*
 * Additional 8 channels configuration for gxtvbb,gxl,gxm and txl
 */
static void pwm_meson_config_ext(struct aml_pwm_chip *our_chip,
					struct aml_pwm_channel *our_chan,
					unsigned int id)
{
	switch (id) {
	case PWM_A2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x3 << 4),
			(((our_chip->clk_mask)&0x3) << 4));
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_A2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_B2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x3 << 6),
			(((our_chip->clk_mask >> 2)&0x3) << 6));
		pwm_set_reg_bits(our_chip->base + REG_MISC_AB,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_B2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_C2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x3 << 4),
			(((our_chip->clk_mask >> 4)&0x3) << 4));
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_C2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_D2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x3 << 6),
			(((our_chip->clk_mask >> 6)&0x3) << 6));
		pwm_set_reg_bits(our_chip->base + REG_MISC_CD,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_D2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_E2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x3 << 4),
			(((our_chip->clk_mask >> 8)&0x3) << 4));
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->base + REG_PWM_E2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_F2:
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x3 << 6),
			(((our_chip->clk_mask >> 10)&0x3) << 6));
		pwm_set_reg_bits(our_chip->base + REG_MISC_EF,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->base + REG_PWM_F2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_AO_A2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x3 << 4),
			(((our_chip->clk_mask >> 12)&0x3) << 4));
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x7f << 8)|(1 << 15),
			(our_chan->pwm_pre_div << 8)|(1 << 15));
		pwm_write_reg(our_chip->ao_base + REG_PWM_AO_A2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	case PWM_AO_B2:
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x3 << 6),
			(((our_chip->clk_mask >> 14)&0x3) << 6));
		pwm_set_reg_bits(our_chip->ao_base + REG_MISC_AO_AB,
			(0x7f << 16)|(1 << 23),
			(our_chan->pwm_pre_div << 16)|(1 << 23));
		pwm_write_reg(our_chip->ao_base + REG_PWM_AO_B2,
			(our_chan->pwm_hi << 16) | (our_chan->pwm_lo));
		break;
	default:
	break;
	}

}

static int pwm_aml_config(struct pwm_chip *chip,
							struct pwm_device *pwm,
							int duty_ns,
							int period_ns)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);
	unsigned int id = pwm->hwpwm;
	unsigned int offset;
	int ret;

	if ((~(our_chip->inverter_mask >> id) & 0x1))
		duty_ns = period_ns - duty_ns;

	if (period_ns > NSEC_PER_SEC)
		return -ERANGE;

	if (period_ns == our_chan->period_ns && duty_ns == our_chan->duty_ns)
		return 0;

	offset = id * 2;
	ret = pwm_aml_clk(our_chip, pwm, duty_ns, period_ns, offset);
	if (ret) {
		dev_err(chip->dev, "tried to calc pwm freq err\n");
		return -EINVAL;
	}

	if (id < AML_PWM_NUM)
		pwm_meson_config(our_chip, our_chan, id);
	else
		pwm_meson_config_ext(our_chip, our_chan, id);

	our_chan->period_ns = period_ns;
	our_chan->duty_ns = duty_ns;

	return 0;
}

static void pwm_aml_set_invert(struct pwm_chip *chip, struct pwm_device *pwm,
				unsigned int channel, bool invert)
{
	struct aml_pwm_chip *our_chip = to_aml_pwm_chip(chip);
	unsigned long flags;
	struct aml_pwm_channel *our_chan = pwm_get_chip_data(pwm);

	spin_lock_irqsave(&aml_pwm_lock, flags);
	if (invert)
		our_chip->inverter_mask |= BIT(channel);
	else
		our_chip->inverter_mask &= ~BIT(channel);

	pwm_aml_config(chip, pwm, our_chan->duty_ns,
					our_chan->period_ns);

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
	{ .compatible = "amlogic, meson-pwm", },
	{},
};

static int pwm_aml_parse_dt(struct aml_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;
	const struct of_device_id *match;
	int i = 0;
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
	chip->ao_blink_base = of_iomap(chip->chip.dev->of_node, 2);
	if (IS_ERR(chip->ao_blink_base))
		return PTR_ERR(chip->ao_blink_base);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		of_property_for_each_u32(np,
								"pwm-outputs-new",
								prop,
								cur,
								val) {
			if (val >= AML_PWM_NUM_NEW) {
				dev_err(chip->chip.dev,
				"%s: invalid channel index in pwm-outputs-new property\n",
				__func__);
				continue;
			}
			chip->variant.output_mask_new |= BIT(val);
		}
	} else {
		of_property_for_each_u32(np, "pwm-outputs", prop, cur, val) {
			if (val >= AML_PWM_NUM) {
				dev_err(chip->chip.dev,
				"%s: invalid channel index in pwm-outputs property\n",
				__func__);
				continue;
			}
			chip->variant.output_mask |= BIT(val);
		}
	}
	chip->xtal_clk = clk_get(chip->chip.dev, "xtal");
	chip->vid_pll_clk = clk_get(chip->chip.dev, "vid_pll_clk");
	chip->fclk_div4_clk = clk_get(chip->chip.dev, "fclk_div4");
	chip->fclk_div3_clk = clk_get(chip->chip.dev, "fclk_div3");

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		of_property_for_each_u32(np,
								"clock-select-new",
								prop,
								cur,
								val) {
			if (val >= AML_PWM_NUM_NEW) {
				dev_err(chip->chip.dev,
				"%s: invalid channel index in clock-select-new property\n",
								__func__);
			continue;
			}
		chip->clk_mask |= val<<(2 * i);
		i++;
		}
	} else {
		of_property_for_each_u32(np, "clock-select", prop, cur, val) {
			if (val >= AML_PWM_NUM) {
				dev_err(chip->chip.dev,
				"%s: invalid channel index in clock-select property\n",
				__func__);
				continue;
			}
			chip->clk_mask |= val<<(2 * i);
			i++;
		}
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
	int ret_fs;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		dev_err(dev, "pwm alloc MEMORY err!!\n");
		return -ENOMEM;
	}

	chip->chip.dev = &pdev->dev;
	chip->chip.ops = &pwm_aml_ops;
	chip->chip.base = -1;
	/*add for four new node*/
	chip->variant.constant = 0;
	chip->variant.blink_enable = 0;
	chip->variant.blink_times = 0;
	chip->variant.times = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		chip->chip.npwm = AML_PWM_NUM_NEW;
	else
		chip->chip.npwm = AML_PWM_NUM;
	chip->inverter_mask = BIT(AML_PWM_NUM) - 1;
	/*chip->inverter_mask = BIT_ULL(AML_PWM_NUM) - 1;*/

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

	ret = pwmchip_add(&chip->chip);
	if (ret < 0) {
		dev_err(dev, "failed to register PWM chip\n");
		return ret;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		ret_fs = meson_pwm_sysfs_init(dev);
		if (ret_fs) {
			dev_err(dev, "pwm sysfs group creation failed\n");
			return ret_fs;
		}
	}
	platform_set_drvdata(pdev, chip);
	pr_info("probe ok\n");

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
	unsigned int num;

/*
	 * No one preserves these values during suspend so reset them.
	 * Otherwise driver leaves PWM unconfigured if same values are
	 * passed to pwm_config() next time.
*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		num = AML_PWM_NUM_NEW;
	else
		num = AML_PWM_NUM;
	for (i = 0; i < num; ++i) {
		struct pwm_device *pwm = &chip->chip.pwms[i];
		struct aml_pwm_channel *chan = pwm_get_chip_data(pwm);
		if (!chan)
			continue;

		chan->period_ns = 0;
		chan->duty_ns = 0;
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
		.name	= "meson-pwm",
		.owner	= THIS_MODULE,
		.pm	= &pwm_aml_pm_ops,
		.of_match_table = of_match_ptr(aml_pwm_matches),
	},
	.probe		= pwm_aml_probe,
	.remove		= pwm_aml_remove,
};
/*
*need to register before wifi_dt driver
*/
static int __init aml_pwm_init(void)
{
	int ret;
	ret = platform_driver_register(&pwm_aml_driver);
	return ret;
}
static void __exit aml_pwm_exit(void)
{
	platform_driver_unregister(&pwm_aml_driver);
}
fs_initcall_sync(aml_pwm_init);
module_exit(aml_pwm_exit);


MODULE_ALIAS("platform:meson-pwm");
MODULE_LICENSE("GPL");
