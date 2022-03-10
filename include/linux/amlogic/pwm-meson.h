/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PWM_MESON_H
#define _PWM_MESON_H

#include <linux/bitops.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/of_address.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
/* for pwm channel index*/
#include <dt-bindings/pwm/meson.h>

#define MESON_NUM_PWMS		2
#define MESON_DOUBLE_NUM_PWMS	4
#define DEFAULT_EXTERN_CLK	24000000


/*a group pwm registers offset address
 * for example:
 * PWM A B
 * PWM C D
 * PWM E F
 * PWM AO A
 * PWM AO B
 */
#define REG_PWM_A				0x0
#define REG_PWM_B				0x4
#define REG_MISC_AB				0x8
#define REG_DS_AB				0xc
#define REG_TIME_AB				0x10
#define REG_PWM_A2				0x14
#define REG_PWM_B2				0x18
#define REG_BLINK_AB				0x1c

#define PWM_LOW_MASK				GENMASK(15, 0)
#define PWM_HIGH_MASK				GENMASK(31, 16)

/* pwm output enable */
#define MISC_A_EN				BIT(0)
#define MISC_B_EN				BIT(1)
#define MISC_A2_EN				BIT(25)
#define MISC_B2_EN				BIT(24)
/* pwm polarity enable */
#define MISC_A_INVERT				BIT(26)
#define MISC_B_INVERT				BIT(27)
/* when you want 0% or 100% waveform
 * constant bit should be set.
 */
#define MISC_A_CONSTANT				BIT(28)
#define MISC_B_CONSTANT				BIT(29)
/*
 * pwm a and b clock enable/disable
 */
#define MISC_A_CLK_EN				BIT(15)
#define MISC_B_CLK_EN				BIT(23)
/*
 * blink control bit
 */
#define BLINK_A					BIT(8)
#define BLINK_B					BIT(9)

#define PWM_HIGH_SHIFT				16
#define MISC_CLK_DIV_MASK			0x7f
#define MISC_B_CLK_DIV_SHIFT			16
#define MISC_A_CLK_DIV_SHIFT			8
#define MISC_B_CLK_SEL_SHIFT			6
#define MISC_A_CLK_SEL_SHIFT			4
#define MISC_CLK_SEL_WIDTH			2
#define PWM_CHANNELS_PER_GROUP			2
#define PWM_DISABLE				0
#define MISC_CLK_SEL_MASK			0x3

static const unsigned int mux_reg_shifts[] = {
	MISC_A_CLK_SEL_SHIFT,
	MISC_B_CLK_SEL_SHIFT,
	MISC_A_CLK_SEL_SHIFT,
	MISC_B_CLK_SEL_SHIFT
};

/*pwm register att*/
struct meson_pwm_variant {
	unsigned int times;
	unsigned int constant;
	unsigned int blink_enable;
	unsigned int blink_times;
};

/*for soc data:
 *double channel enable
 * double_channel = false ,could use PWM A
 * double_channel = true , could use PWM A and PWM A2
 * extern_clk = false , clk div, gate, mux in pwm controller
 * extern_clk = true , clk div, gate, mux in clktree
 */
struct meson_pwm_data {
	char **parent_names;
	unsigned int num_parents;
	unsigned int double_channel;
	unsigned int extern_clk;
};

struct meson_pwm_channel {
	unsigned int hi;
	unsigned int lo;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned int clk_rate;
	u8 clk_div;
#endif
	u8 pre_div;

	struct clk *clk_parent;
	struct clk_mux mux;
	struct clk *clk;
};

struct meson_pwm {
	struct pwm_chip chip;
	struct meson_pwm_data *data;
	struct meson_pwm_channel channels[MESON_DOUBLE_NUM_PWMS];
	struct meson_pwm_variant variant;
	void __iomem *base;
	/*
	 * Protects register (write) access to the REG_MISC_AB register
	 * that is shared between the two PWMs.
	 */
	spinlock_t lock;
	struct regmap *regmap_base;
};

/*the functions only use for meson pwm driver*/
int meson_pwm_sysfs_init(struct device *dev);
void meson_pwm_sysfs_exit(struct device *dev);

/*the functions use for special function in meson pwm driver*/
int pwm_register_debug(struct meson_pwm *meson);
struct meson_pwm *to_meson_pwm(struct pwm_chip *chip);
int pwm_constant_enable(struct meson_pwm *meson, int index);
int pwm_constant_disable(struct meson_pwm *meson, int index);
int pwm_blink_enable(struct meson_pwm *meson, int index);
int pwm_blink_disable(struct meson_pwm *meson, int index);
int pwm_set_blink_times(struct meson_pwm *meson, int index, int value);
int pwm_set_times(struct meson_pwm *meson, int index, int value);
#endif   /* _PWM_MESON_H_ */

