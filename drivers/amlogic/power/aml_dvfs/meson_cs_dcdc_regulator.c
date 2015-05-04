/*
 * drivers/amlogic/power/aml_dvfs/meson_cs_dcdc_regulator.c
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/amlogic/meson_cs_dcdc_regulator.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/amlogic/iomap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_dvfs.h>
#include <linux/amlogic/cpu_version.h>

#define PWM_A   0
#define PWM_B   1
#define PWM_C   2
#define PWM_D   3
#define PWM_E   4
#define PWM_F   5

struct cs_voltage {
	int pwm_value;
	int voltage;
};
static struct cs_voltage *g_table;
static int g_table_cnt;
static int use_pwm;
static int pwm_ctrl = PWM_C;
static unsigned long pmw_base[] = {
	P_PWM_PWM_A,
	P_PWM_PWM_B,
	P_PWM_PWM_C,
	P_PWM_PWM_D,
	P_PWM_PWM_E,
	P_PWM_PWM_F
};

static struct meson_cs_pdata_t *g_vcck_voltage;

static inline uint32_t read_reg(uint32_t reg)
{
	uint32_t val;
	if (g_vcck_voltage) {
		aml_reg_read(IO_CBUS_BASE, reg, &val);
		return val;
	} else {
		pr_info("%s, platfrom device not initialized\n", __func__);
		return -EINVAL;
	}
}

static inline void write_reg(uint32_t reg, uint32_t val)
{
	if (g_vcck_voltage)
		aml_reg_write(IO_CBUS_BASE, reg, val);
	else
		pr_info("%s, platfrom device not initialized\n", __func__);
}

static inline void set_reg_bits(uint32_t reg, uint32_t val,
				uint32_t start, uint32_t len)
{
	uint32_t tmp, mask;
	if (g_vcck_voltage) {
		aml_reg_read(IO_CBUS_BASE, reg, &tmp);
		mask = ~(((1 << len) - 1) << start);
		tmp &= mask;
		tmp |= (val & mask);
		aml_reg_write(IO_CBUS_BASE, reg, tmp);
	} else {
		pr_info("%s, platfrom device not initialized\n", __func__);
	}
}

static int dvfs_get_voltage_step(void)
{
	int i = 0;
	unsigned int reg_val;

	if (use_pwm) {
		reg_val = read_reg(pmw_base[pwm_ctrl]);
		for (i = 0; i < g_table_cnt; i++) {
			if (g_table[i].pwm_value == reg_val)
				return i;
		}
		if (i >= g_table_cnt)
			return -1;
	} else {
		reg_val = read_reg(P_VGHL_PWM_REG0);
		if ((reg_val>>12&3) != 1)
			return -1;
		return reg_val & 0xf;
	}
	return -1;
}

static int dvfs_set_voltage(int from, int to)
{
	int cur;

	if (to < 0 || to > g_table_cnt) {
		pr_info(KERN_ERR "%s: to(%d) out of range!\n", __func__, to);
		return -EINVAL;
	}
	if (from < 0 || from > g_table_cnt) {
		if (use_pwm) {
			/*
			 * use PMW method to adjust vcck voltage
			 */
			write_reg(pmw_base[pwm_ctrl], g_table[to].pwm_value);
		} else {
			/*
			 * use constant-current source to adjust vcck voltage
			 */
			set_reg_bits(P_VGHL_PWM_REG0, to, 0, 4);
		}
		udelay(200);
		return 0;
	}
	cur = from;
	while (cur != to) {
		/*
		 * if target step is far away from current step, don't change
		 * voltage by one-step-done. You should change voltage step by
		 * step to make sure voltage output is stable
		 */
		if (cur < to) {
			if (cur < to - 3)
				cur += 3;
			else
				cur = to;
		} else {
			if (cur > to + 3)
				cur -= 3;
			else
				cur = to;
		}
		if (use_pwm)
			write_reg(pmw_base[pwm_ctrl], g_table[cur].pwm_value);
		else
			set_reg_bits(P_VGHL_PWM_REG0, cur, 0, 4);
		udelay(100);
	}
	return 0;
}

static int meson_cs_set_voltage(uint32_t id, uint32_t min_uV, uint32_t max_uV)
{
	uint32_t vol = 0;
	int	  i;
	int	  cur;

	if (min_uV > max_uV || !g_table) {
		pr_info("%s, invalid voltage or NULL table\n", __func__);
		return -1;
	}
	vol = (min_uV + max_uV) / 2;
	for (i = 0; i < g_table_cnt; i++) {
		if (g_table[i].voltage >= vol)
			break;
	}
	if (i == g_table_cnt) {
		pr_info("%s, voltage is too large:%d\n", __func__, vol);
		return -EINVAL;
	}

	cur = dvfs_get_voltage_step();
	return dvfs_set_voltage(cur, i);
}

static int meson_cs_get_voltage(uint32_t id, uint32_t *uV)
{
	int cur;

	if (!g_table) {
		pr_info("%s, no voltage table\n", __func__);
		return -1;
	}
	cur = dvfs_get_voltage_step();
	if (cur < 0) {
		return cur;
	} else {
		*uV = g_table[cur].voltage;
		return 0;
	}
}

struct aml_dvfs_driver aml_cs_dvfs_driver = {
	.name	     = "meson-cs-dvfs",
	.id_mask     = (AML_DVFS_ID_VCCK),
	.set_voltage = meson_cs_set_voltage,
	.get_voltage = meson_cs_get_voltage,
};

#define DEBUG_PARSE 1
#define PARSE_UINT32_PROPERTY(node, prop_name, value, exception) \
do { \
	if (of_property_read_u32(node, prop_name, (u32 *)(&value))) { \
		pr_info("failed to get property: %s\n", prop_name); \
		goto exception; \
	} \
	if (DEBUG_PARSE) { \
		pr_info("get property:%25s, value:0x%08x, dec:%8d\n", \
			prop_name, value, value); \
	} \
} while (0)

#ifdef CONFIG_OF
static const struct of_device_id amlogic_meson_cs_dvfs_match[] = {
	{
		.compatible = "amlogic, meson_vcck_dvfs",
	},
	{},
};
#endif

static void dvfs_vcck_pwm_init(struct device *dev)
{
	uint32_t reg;
	uint32_t cpu_version;

	cpu_version = get_cpu_type();
	if ((cpu_version < MESON_CPU_MAJOR_ID_M8B) &&
	    (pwm_ctrl >= PWM_E)) {
		pr_err("%s, cpu type:%d %s with pwm controller:%d",
		       __func__, cpu_version,
		       "don't compatible", pwm_ctrl);
		return;
	}

	switch (pwm_ctrl) {
	case PWM_A:
		reg = read_reg(P_PWM_MISC_REG_AB);
		reg &= ~(0x7f <<  8);
		reg |= ((1 << 15) | (1 << 0));
		write_reg(P_PWM_MISC_REG_AB, reg);
		break;

	case PWM_B:
		reg = read_reg(P_PWM_MISC_REG_AB);
		reg &= ~(0x7f << 16);
		reg |= ((1 << 23) | (1 << 1));
		write_reg(P_PWM_MISC_REG_AB, reg);
		break;

	case PWM_C:
		reg = read_reg(P_PWM_MISC_REG_CD);
		reg &= ~(0x7f <<  8);
		reg |= ((1 << 15) | (1 << 0));
		write_reg(P_PWM_MISC_REG_CD, reg);
		break;

	case PWM_D:
		reg = read_reg(P_PWM_MISC_REG_CD);
		reg &= ~(0x7f << 16);
		reg |= ((1 << 23) | (1 << 1));
		write_reg(P_PWM_MISC_REG_CD, reg);
		break;

	case PWM_E:
		reg = read_reg(P_PWM_MISC_REG_EF);
		reg &= ~(0x7f <<  8);
		reg |= ((1 << 15) | (1 << 0));
		write_reg(P_PWM_MISC_REG_EF, reg);
		break;

	case PWM_F:
		reg = read_reg(P_PWM_MISC_REG_EF);
		reg &= ~(0x7f << 16);
		reg |= ((1 << 23) | (1 << 1));
		write_reg(P_PWM_MISC_REG_EF, reg);
		break;

	default:
		return;
	}
	write_reg(pmw_base[pwm_ctrl], g_table[g_table_cnt - 1].pwm_value);

	if (IS_ERR(devm_pinctrl_get_select_default(dev)))
		pr_info("did not get pins for pwm--------\n");
	else
		pr_info("get pin for pwm--------\n");
}

static void check_pwm_controller(struct device_node *np)
{
	int ret, i;
	const char *out_str = NULL;

	ret = of_property_read_string(np, "pmw_controller", &out_str);
	if (ret) {
		pr_info("no 'pwm_controller', default use pmw_c\n");
		return;
	}
	if (!strncmp(out_str, "PWM_", 4)) {
		i = out_str[4] - 'A';
		if (i <= PWM_F && i > 0)
			pwm_ctrl = i;
	}
	pr_info("%s, bad pwm controller value:%s\n", __func__, out_str);
}

static int meson_cs_dvfs_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	int i = 0, read_len;

	if (!np)
		return -ENODEV;

	PARSE_UINT32_PROPERTY(np, "use_pwm", use_pwm, out);
	if (use_pwm)
		check_pwm_controller(np);

	PARSE_UINT32_PROPERTY(np, "table_count", g_table_cnt, out);
	g_table = kzalloc(sizeof(struct cs_voltage) * g_table_cnt, GFP_KERNEL);
	if (g_table == NULL) {
		pr_info("%s, allocate memory failed\n", __func__);
		return -ENOMEM;
	}
	read_len = (sizeof(struct cs_voltage) * g_table_cnt) / sizeof(int);
	ret = of_property_read_u32_array(np, "cs_voltage_table",
					 (u32 *)g_table, read_len);
	if (ret < 0) {
		pr_info("%s, failed to read 'cs_voltage_table', ret:%d\n",
			__func__, ret);
		goto out;
	}
	pr_info("%s, table count:%d, use_pwm:%d, pwm controller:%C\n",
		__func__, g_table_cnt, use_pwm, pwm_ctrl + 'A');
	for (i = 0; i < g_table_cnt; i++) {
		pr_info("%2d, %08x, %7d\n",
			i, g_table[i].pwm_value, g_table[i].voltage);
	}

	if (use_pwm)
		dvfs_vcck_pwm_init(&pdev->dev);

	aml_dvfs_register_driver(&aml_cs_dvfs_driver);
	return 0;
out:
	kfree(g_table);
	g_table = NULL;
	return -1;
}

static int meson_cs_dvfs_remove(struct platform_device *pdev)
{
	kfree(g_table);

	aml_dvfs_unregister_driver(&aml_cs_dvfs_driver);
	return 0;
}

static struct platform_driver meson_cs_dvfs_driver = {
	.driver = {
		.name = "meson_vcck_dvfs",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = amlogic_meson_cs_dvfs_match,
#endif
	},
	.probe = meson_cs_dvfs_probe,
	.remove = meson_cs_dvfs_remove,
};


static int __init meson_cs_dvfs_init(void)
{
	return platform_driver_register(&meson_cs_dvfs_driver);
}

static void __exit meson_cs_dvfs_cleanup(void)
{
	platform_driver_unregister(&meson_cs_dvfs_driver);
}

subsys_initcall(meson_cs_dvfs_init);
module_exit(meson_cs_dvfs_cleanup);

MODULE_AUTHOR("Elvis Yu <elvis.yu@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson current source voltage regulator driver");
MODULE_LICENSE("GPL v2");
