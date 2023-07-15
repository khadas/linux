// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/aml_spi.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"
#include "../lcd_bl.h"

#include <linux/amlogic/gki_module.h>

static DEFINE_MUTEX(ldim_dev_dbg_mutex);
static struct delayed_work ldim_dev_probe_dly_work;
static unsigned int ldim_dev_probe_retry_cnt;

struct bl_gpio_s ldim_gpio[BL_GPIO_NUM_MAX] = {
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
};

static struct spicc_controller_data ldim_spi_controller_data = {
	.ccxfer_en = 0,
	.timing_en = 1,
	.ss_leading_gap = 1,
	.ss_trailing_gap = 0,
	.tx_tuning = 0,
	.rx_tuning = 7,
	.dummy_ctl = 0,
};

static struct spi_board_info ldim_spi_info = {
	.modalias = "ldim_dev",
	.mode = SPI_MODE_0,
	.max_speed_hz = 1000000, /* 1MHz */
	.bus_num = 0, /* SPI bus No. */
	.chip_select = 0, /* the cs pin index on the spi bus */
	.controller_data = &ldim_spi_controller_data,
};

static int ldim_dev_probe_flag;

struct ldim_dev_driver_s ldim_dev_drv = {
	.index = 0xff,
	.type = LDIM_DEV_TYPE_NORMAL,
	.dma_support = 0,
	.spi_sync = 0,
	.cs_hold_delay = 0,
	.cs_clk_delay = 0,
	.en_gpio = LCD_EXT_GPIO_INVALID,
	.en_gpio_on = 1,
	.en_gpio_off = 0,
	.lamp_err_gpio = LCD_EXT_GPIO_INVALID,
	.fault_check = 0,
	.write_check = 0,
	.pinmux_flag = 0xff,
	.chip_cnt = 0,

	.init_loaded = 0,
	.cmd_size = 4,
	.init_on = NULL,
	.init_off = NULL,
	.init_on_cnt = 0,
	.init_off_cnt = 0,

	.ldim_pwm_config = {
		.pwm_method = BL_PWM_POSITIVE,
		.pwm_port = BL_PWM_MAX,
		.pwm_duty_max = 4095,
		.pwm_duty_min = 0,
	},
	.analog_pwm_config = {
		.pwm_method = BL_PWM_POSITIVE,
		.pwm_port = BL_PWM_MAX,
		.pwm_freq = 1000,
		.pwm_duty_max = 4095,
		.pwm_duty_min = 10,
	},

	.bl_row = 1,
	.bl_col = 1,
	.dim_min = 0x7f, /* min 3% duty */
	.dim_max = 0xfff,

	.zone_num = 1,
	.bl_mapping = NULL,

	.pin = NULL,
	.spi_dev = NULL,
	.spi_info = NULL,

	.pinmux_ctrl = NULL,
	.dim_range_update = NULL,
	.reg_write = NULL,
	.reg_read = NULL,

	.power_on = NULL,
	.power_off = NULL,
	.dev_smr = NULL,
	.dev_smr_dummy = NULL,
	.config_print = NULL,
};

static void ldim_gpio_probe(struct ldim_dev_driver_s *dev_drv, int index)
{
	struct bl_gpio_s *ld_gpio;
	const char *str;
	int ret;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag) {
		if (ldim_debug_print)
			LDIMPR("gpio %s[%d] is already registered\n", ld_gpio->name, index);
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(dev_drv->dev->of_node,
					    "ldim_dev_gpio_names", index, &str);
	if (ret) {
		LDIMERR("failed to get ldim_dev_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(ld_gpio->name, str);

	/* init gpio flag */
	ld_gpio->probe_flag = 1;
	ld_gpio->register_flag = 0;
}

static int ldim_gpio_register(struct ldim_dev_driver_s *dev_drv, int index, int init_value)
{
	struct bl_gpio_s *ld_gpio;
	int value;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		LDIMERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ld_gpio->register_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: gpio %s[%d] is already registered\n",
			       __func__, ld_gpio->name, index);
		}
		return 0;
	}

	switch (init_value) {
	case BL_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case BL_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case BL_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	ld_gpio->gpio = devm_gpiod_get_index(dev_drv->dev, "ldim_dev", index, value);
	if (IS_ERR(ld_gpio->gpio)) {
		LDIMERR("register gpio %s[%d]: %p, err: %d\n",
			ld_gpio->name, index, ld_gpio->gpio,
			IS_ERR(ld_gpio->gpio));
		return -1;
	}
	ld_gpio->register_flag = 1;
	if (ldim_debug_print) {
		LDIMPR("register gpio %s[%d]: %p, init value: %d\n",
		       ld_gpio->name, index, ld_gpio->gpio, init_value);
	}

	return 0;
}

void ldim_gpio_set(struct ldim_dev_driver_s *dev_drv, int index, int value)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (ld_gpio->register_flag == 0) {
		ldim_gpio_register(dev_drv, index, value);
		return;
	}
	if (IS_ERR_OR_NULL(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index, ld_gpio->gpio,
			PTR_ERR(ld_gpio->gpio));
		return;
	}

	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ld_gpio->gpio, value);
		break;
	case BL_GPIO_INPUT:
	default:
		gpiod_direction_input(ld_gpio->gpio);
		break;
	}
	if (ldim_debug_print) {
		LDIMPR("set gpio %s[%d] value: %d\n",
		       ld_gpio->name, index, value);
	}
}

unsigned int ldim_gpio_get(struct ldim_dev_driver_s *dev_drv, int index)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return -1;
	}

	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		LDIMERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ld_gpio->register_flag == 0) {
		LDIMERR("%s: gpio %s[%d] is not registered\n",
			__func__, ld_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index,
			ld_gpio->gpio, PTR_ERR(ld_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(ld_gpio->gpio);
}

void ldim_set_duty_pwm(struct bl_pwm_config_s *bl_pwm)
{
	unsigned long long temp;

	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return;
	if (bl_pwm->pwm_duty_max == 0)
		return;

	temp = bl_pwm->pwm_cnt;
	bl_pwm->pwm_level = bl_do_div(((temp * bl_pwm->pwm_duty) +
		((bl_pwm->pwm_duty_max + 1) >> 1)), bl_pwm->pwm_duty_max);

	if (ldim_debug_print == 2) {
		LDIMPR("pwm port %d: duty= %d / %d, pwm_max=%d, pwm_min=%d, pwm_level=%d\n",
		       bl_pwm->pwm_port, bl_pwm->pwm_duty, bl_pwm->pwm_duty_max,
		       bl_pwm->pwm_max, bl_pwm->pwm_min, bl_pwm->pwm_level);
	}

	bl_pwm_ctrl(bl_pwm, 1);
}

void ldim_pwm_off(struct bl_pwm_config_s *bl_pwm)
{
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return;

	bl_pwm_ctrl(bl_pwm, 0);
}

/* ****************************************************** */
static char *ldim_pinmux_str[] = {
	"ldim_pwm",               /* 0 */
	"ldim_pwm_vs",            /* 1 */
	"ldim_pwm_combo",         /* 2 */
	"ldim_pwm_vs_combo",      /* 3 */
	"ldim_pwm_off",           /* 4 */
	"ldim_pwm_combo_off",     /* 5 */
	"custom",
};

static int ldim_pwm_pinmux_ctrl(struct ldim_dev_driver_s *dev_drv, int status)
{
	struct bl_pwm_config_s *bl_pwm;
	char *str;
	int ret = 0, index = 0xff;

	if (dev_drv->ldim_pwm_config.pwm_port >= BL_PWM_MAX)
		return 0;

	if (status) {
		bl_pwm = &dev_drv->ldim_pwm_config;
		if (bl_pwm->pwm_port == BL_PWM_VS)
			index = 1;
		else
			index = 0;
		bl_pwm = &dev_drv->analog_pwm_config;
		if (bl_pwm->pwm_port < BL_PWM_VS)
			index += 2;
	} else {
		bl_pwm = &dev_drv->analog_pwm_config;
		if (bl_pwm->pwm_port < BL_PWM_VS)
			index = 5;
		else
			index = 4;
	}

	str = ldim_pinmux_str[index];
	if (dev_drv->pinmux_flag == index) {
		LDIMPR("pinmux %s is already selected\n", str);
		return 0;
	}

	/* request pwm pinmux */
	dev_drv->pin = devm_pinctrl_get_select(dev_drv->dev, str);
	if (IS_ERR_OR_NULL(dev_drv->pin)) {
		LDIMERR("set pinmux %s error\n", str);
		ret = -1;
	} else {
		LDIMPR("set pinmux %s: 0x%p\n", str, dev_drv->pin);
	}
	dev_drv->pinmux_flag = index;

	return ret;
}

static int ldim_pwm_vs_update(struct aml_ldim_driver_s *ldim_drv)
{
	struct bl_pwm_config_s *bl_pwm = &ldim_drv->dev_drv->ldim_pwm_config;
	unsigned int cnt;
	int ret = 0;

	if (bl_pwm->pwm_port != BL_PWM_VS)
		return 0;

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	cnt = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
	if (cnt != bl_pwm->pwm_cnt) {
		bl_pwm_config_init(bl_pwm);
		ldim_set_duty_pwm(bl_pwm);
	}

	return ret;
}

#define EXT_LEN_MAX   500
static void ldim_dev_init_table_dynamic_print(struct ldim_dev_driver_s *dev_drv,
					      int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char *str;
	unsigned char *table;

	str = kcalloc(EXT_LEN_MAX, sizeof(char), GFP_KERNEL);
	if (!str) {
		LDIMERR("%s: str malloc error\n", __func__);
		return;
	}
	if (flag) {
		pr_info("power on:\n");
		table = dev_drv->init_on;
		max_len = dev_drv->init_on_cnt;
	} else {
		pr_info("power off:\n");
		table = dev_drv->init_off;
		max_len = dev_drv->init_off_cnt;
	}
	if (max_len == 0) {
		kfree(str);
		return;
	}
	if (!table) {
		LDIMERR("init_table %d is NULL\n", flag);
		kfree(str);
		return;
	}

	i = 0;
	while ((i + 1) < max_len) {
		if (table[i] == LCD_EXT_CMD_TYPE_END) {
			pr_info("0x%02x,%d,\n", table[i], table[i + 1]);
			break;
		}
		cmd_size = table[i + 1];

		k = snprintf(str, EXT_LEN_MAX, "  0x%02x,%d,",
			     table[i], cmd_size);
		if (cmd_size == 0)
			goto init_table_dynamic_print_next;
		if (i + 2 + cmd_size > max_len) {
			pr_info("cmd_size out of support\n");
			break;
		}

		if (table[i] == LCD_EXT_CMD_TYPE_DELAY) {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"%d,", table[i + 2 + j]);
			}
		} else if (table[i] == LCD_EXT_CMD_TYPE_CMD) {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"0x%02x,", table[i + 2 + j]);
			}
		} else if (table[i] == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			for (j = 0; j < (cmd_size - 1); j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"0x%02x,", table[i + 2 + j]);
			}
			snprintf(str + k, EXT_LEN_MAX,
				 "%d,", table[i + cmd_size + 1]);
		} else {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"0x%02x,", table[i + 2 + j]);
			}
		}
init_table_dynamic_print_next:
		pr_info("%s\n", str);
		i += (cmd_size + 2);
	}

	kfree(str);
}

static void ldim_dev_config_print(struct aml_ldim_driver_s *ldim_drv)
{
	struct bl_pwm_config_s *bl_pwm;
	struct pwm_state pstate;
	int i, n, len = 0;
	char *str = NULL;

	LDIMPR("%s:\n", __func__);

	pr_info("valid_flag            = %d\n"
		"vsync_change_flag     = %d\n\n",
		ldim_drv->valid_flag,
		ldim_drv->vsync_change_flag);
	if (!ldim_drv->dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return;
	}

	pr_info("dev_index             = %d\n"
		"dev_name              = %s\n"
		"key_valid             = %d\n"
		"type                  = %d\n"
		"en_gpio               = %d\n"
		"en_gpio_on            = %d\n"
		"en_gpio_off           = %d\n"
		"chip_cnt              = %d\n"
		"dim_min               = 0x%03x\n"
		"dim_max               = 0x%03x\n"
		"zone_num              = %d\n",
		ldim_drv->dev_drv->index,
		ldim_drv->dev_drv->name,
		ldim_drv->dev_drv->key_valid,
		ldim_drv->dev_drv->type,
		ldim_drv->dev_drv->en_gpio,
		ldim_drv->dev_drv->en_gpio_on,
		ldim_drv->dev_drv->en_gpio_off,
		ldim_drv->dev_drv->chip_cnt,
		ldim_drv->dev_drv->dim_min,
		ldim_drv->dev_drv->dim_max,
		ldim_drv->dev_drv->zone_num);
	n = ldim_drv->dev_drv->zone_num;
	len = (n * 4) + 50;
	str = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!str) {
		pr_info("%s: buf malloc error\n", __func__);
	} else {
		len = sprintf(str, "zone_mapping:\n  ");
		for (i = 0; i < n; i++) {
			len += sprintf(str + len, "%d,",
				ldim_drv->dev_drv->bl_mapping[i]);
		}
		pr_info("%s\n\n", str);
		kfree(str);
	}

	switch (ldim_drv->dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		pr_info("spi_pointer           = 0x%p\n"
			"spi_modalias          = %s\n"
			"spi_mode              = %d\n"
			"spi_max_speed_hz      = %d\n"
			"spi_bus_num           = %d\n"
			"spi_chip_select       = %d\n"
			"spi_dma_support       = %d\n"
			"cs_hold_delay         = %d\n"
			"cs_clk_delay          = %d\n"
			"lamp_err_gpio         = %d\n"
			"fault_check           = %d\n"
			"write_check           = %d\n\n",
			ldim_drv->dev_drv->spi_dev,
			ldim_drv->dev_drv->spi_info->modalias,
			ldim_drv->dev_drv->spi_info->mode,
			ldim_drv->dev_drv->spi_info->max_speed_hz,
			ldim_drv->dev_drv->spi_info->bus_num,
			ldim_drv->dev_drv->spi_info->chip_select,
			ldim_drv->dev_drv->dma_support,
			ldim_drv->dev_drv->cs_hold_delay,
			ldim_drv->dev_drv->cs_clk_delay,
			ldim_drv->dev_drv->lamp_err_gpio,
			ldim_drv->dev_drv->fault_check,
			ldim_drv->dev_drv->write_check);
		break;
	case LDIM_DEV_TYPE_I2C:
		break;
	case LDIM_DEV_TYPE_NORMAL:
	default:
		break;
	}
	bl_pwm = &ldim_drv->dev_drv->ldim_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		pr_info("ldim_pwm_port:       %d\n"
			"ldim_pwm_pol:        %d\n"
			"ldim_pwm_freq:       %d\n"
			"ldim_pwm_cnt:        %d\n"
			"ldim_pwm_level:      %d\n"
			"ldim_pwm_duty:       %d / %d\n",
			bl_pwm->pwm_port, bl_pwm->pwm_method,
			bl_pwm->pwm_freq, bl_pwm->pwm_cnt,
			bl_pwm->pwm_level,
			bl_pwm->pwm_duty, bl_pwm->pwm_duty_max);
		switch (bl_pwm->pwm_port) {
		case BL_PWM_A:
		case BL_PWM_B:
		case BL_PWM_C:
		case BL_PWM_D:
		case BL_PWM_E:
		case BL_PWM_F:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				pr_info("ldim_pwm invalid\n");
				break;
			}
			pr_info("ldim_pwm_pointer:    0x%p\n",
				bl_pwm->pwm_data.pwm);
			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pr_info("ldim_pwm state:\n"
				"  period:            %lld\n"
				"  duty_cycle:        %lld\n"
				"  polarity:          %d\n"
				"  enabled:           %d\n",
				pstate.period, pstate.duty_cycle,
				pstate.polarity, pstate.enabled);
			break;
		case BL_PWM_VS:
			pr_info("ldim_pwm_reg0:       0x%08x\n"
				"ldim_pwm_reg1:       0x%08x\n"
				"ldim_pwm_reg2:       0x%08x\n"
				"ldim_pwm_reg3:       0x%08x\n",
				lcd_vcbus_read(VPU_VPU_PWM_V0),
				lcd_vcbus_read(VPU_VPU_PWM_V1),
				lcd_vcbus_read(VPU_VPU_PWM_V2),
				lcd_vcbus_read(VPU_VPU_PWM_V3));
			break;
		default:
			break;
		}
	}
	bl_pwm = &ldim_drv->dev_drv->analog_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		pr_info("\nanalog_pwm_port:     %d\n"
			"analog_pwm_pol:      %d\n"
			"analog_pwm_freq:     %d\n"
			"analog_pwm_cnt:      %d\n"
			"analog_pwm_level:    %d\n"
			"analog_pwm_duty:     %d\n"
			"analog_pwm_duty_max: %d\n"
			"analog_pwm_duty_min: %d\n",
			bl_pwm->pwm_port, bl_pwm->pwm_method,
			bl_pwm->pwm_freq, bl_pwm->pwm_cnt,
			bl_pwm->pwm_level, bl_pwm->pwm_duty,
			bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		switch (bl_pwm->pwm_port) {
		case BL_PWM_A:
		case BL_PWM_B:
		case BL_PWM_C:
		case BL_PWM_D:
		case BL_PWM_E:
		case BL_PWM_F:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				pr_info("analog_pwm invalid\n");
				break;
			}
			pr_info("analog_pwm_pointer:  0x%p\n",
				bl_pwm->pwm_data.pwm);
			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pr_info("analog_pwm state:\n"
				"  period:            %lld\n"
				"  duty_cycle:        %lld\n"
				"  polarity:          %d\n"
				"  enabled:           %d\n",
				pstate.period, pstate.duty_cycle,
				pstate.polarity, pstate.enabled);
			break;
		default:
			break;
		}
	}
	pr_info("\npinmux_flag:         %d\n"
		"pinmux_pointer:      0x%p\n\n",
		ldim_drv->dev_drv->pinmux_flag,
		ldim_drv->dev_drv->pin);

	if (ldim_drv->dev_drv->cmd_size > 0) {
		pr_info("table_loaded:        %d\n"
			"cmd_size:            %d\n"
			"init_on_cnt:         %d\n"
			"init_off_cnt:        %d\n",
			ldim_drv->dev_drv->init_loaded,
			ldim_drv->dev_drv->cmd_size,
			ldim_drv->dev_drv->init_on_cnt,
			ldim_drv->dev_drv->init_off_cnt);
		if (ldim_drv->dev_drv->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
			ldim_dev_init_table_dynamic_print(ldim_drv->dev_drv, 1);
			ldim_dev_init_table_dynamic_print(ldim_drv->dev_drv, 0);
		}
	}
}

static int ldim_dev_zone_mapping_load(struct ldim_dev_driver_s *dev_drv,
				      const char *path)
{
	unsigned int size = 0, mem_size;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	unsigned char *buf;
	int i, j;

	/* 2byte per zone */
	mem_size = dev_drv->zone_num * 2;
	buf = kcalloc(mem_size, sizeof(char), GFP_KERNEL);
	if (!buf)
		return -1;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		LDIMERR("%s: read %s error or filp is NULL.\n", __func__, path);
		set_fs(old_fs);
		goto ldim_dev_zone_mapping_load_err;
	}

	size = vfs_read(filp, buf, mem_size, &pos);
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	if (size == mem_size) {
		LDIMPR("%s: load size %u\n", __func__, size);
	} else {
		LDIMERR("%s load size %u error\n", __func__, size);
		goto ldim_dev_zone_mapping_load_err;
	}

	for (i = 0; i < dev_drv->zone_num; i++) {
		j = 2 * i;
		dev_drv->bl_mapping[i] = buf[j] | (buf[j + 1] << 8);
	}

	kfree(buf);
	LDIMPR("%s: load zone_mapping file path: %s finish\n", __func__, path);
	return 0;

ldim_dev_zone_mapping_load_err:
	kfree(buf);
	LDIMERR("%s: error\n", __func__);
	return -1;
}

static int ldim_dev_init_dynamic_load_dts(struct device_node *of_node,
					  struct ldim_dev_driver_s *dev_drv,
					  int flag)
{
	unsigned char cmd_size, type;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[20];

	if (flag) {
		max_len = LDIM_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		max_len = LDIM_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	while ((i + 1) < max_len) {
		/* type */
		ret = of_property_read_u32_index(of_node, propname, i, &val);
		if (ret) {
			LDIMERR("%s: get %s type failed, step %d\n",
				dev_drv->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_dts_err;
		}
		table[i] = (unsigned char)val;
		type = table[i];
		/* cmd_size */
		ret = of_property_read_u32_index(of_node, propname,
						 (i + 1), &val);
		if (ret) {
			LDIMERR("%s: get %s cmd_size failed, step %d\n",
				dev_drv->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_dts_err;
		}
		table[i + 1] = (unsigned char)val;
		cmd_size = table[i + 1];

		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (cmd_size == 0)
			goto init_table_dynamic_dts_next;
		if ((i + 2 + cmd_size) > max_len) {
			LDIMERR("%s: %s cmd_size out of support, step %d\n",
				dev_drv->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_dts_err;
		}

		/* data */
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(of_node, propname,
							 (i + 2 + j), &val);
			if (ret) {
				LDIMERR("%s: get %s data failed, step %d\n",
					dev_drv->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}
			table[i + 2 + j] = (unsigned char)val;
		}

init_table_dynamic_dts_next:
		i += (cmd_size + 2);
		step++;
	}
	if (flag)
		dev_drv->init_on_cnt = i + 2;
	else
		dev_drv->init_off_cnt = i + 2;

	if (flag) {
		dev_drv->init_on = kcalloc(dev_drv->init_on_cnt,
					   sizeof(unsigned char), GFP_KERNEL);
		if (!dev_drv->init_on)
			goto init_table_dynamic_dts_err;
		memcpy(dev_drv->init_on, table, dev_drv->init_on_cnt);
	} else {
		dev_drv->init_off = kcalloc(dev_drv->init_off_cnt,
					    sizeof(unsigned char), GFP_KERNEL);
		if (!dev_drv->init_off)
			goto init_table_dynamic_dts_err;
		memcpy(dev_drv->init_off, table, dev_drv->init_off_cnt);
	}

	kfree(table);
	return 0;

init_table_dynamic_dts_err:
	kfree(table);
	return -1;
}

static int ldim_dev_init_dynamic_load_ukey(struct ldim_dev_driver_s *dev_drv,
					   unsigned char *p, int key_len,
					   int len, int flag)
{
	unsigned char cmd_size = 0;
	unsigned char index;
	int i = 0, j, max_len, ret = 0;
	unsigned char *table, *buf;
	char propname[20];

	if (flag) {
		max_len = LDIM_INIT_ON_MAX;
		sprintf(propname, "init_on");
		buf = p;
	} else {
		max_len = LDIM_INIT_OFF_MAX;
		sprintf(propname, "init_off");
		buf = p + dev_drv->init_on_cnt;
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	while ((i + 1) < max_len) {
		/* type */
		len += 1;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret) {
			LDIMERR("%s: get %s failed\n", dev_drv->name, propname);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_ukey_err;
		}
		table[i] = *(buf + LCD_UKEY_LDIM_DEV_INIT + i);
		/* cmd_size */
		len += 1;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret) {
			LDIMERR("%s: get %s failed\n", dev_drv->name, propname);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_ukey_err;
		}
		table[i + 1] = *(buf + LCD_UKEY_LDIM_DEV_INIT + i + 1);
		cmd_size = table[i + 1];

		if (table[i] == LCD_EXT_CMD_TYPE_END)
			break;
		if (cmd_size == 0)
			goto init_table_dynamic_ukey_next;
		if ((i + 2 + cmd_size) > max_len) {
			LDIMERR("%s: %s cmd_size out of support\n",
				dev_drv->name, propname);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_ukey_err;
		}

		/* step3: data */
		len += cmd_size;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret) {
			LDIMERR("%s: get %s failed\n", dev_drv->name, propname);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			goto init_table_dynamic_ukey_err;
		}
		for (j = 0; j < cmd_size; j++) {
			table[i + 2 + j] =
				*(buf + LCD_UKEY_LDIM_DEV_INIT + i + 2 + j);
		}

		if (table[i] == LCD_EXT_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 2];
			if (index < BL_GPIO_NUM_MAX)
				ldim_gpio_probe(dev_drv, index);
		}
init_table_dynamic_ukey_next:
		i += (cmd_size + 2);
	}
	if (flag)
		dev_drv->init_on_cnt = i + 2;
	else
		dev_drv->init_off_cnt = i + 2;

	if (flag) {
		dev_drv->init_on = kcalloc(dev_drv->init_on_cnt,
					   sizeof(unsigned char), GFP_KERNEL);
		if (!dev_drv->init_on)
			goto init_table_dynamic_ukey_err;
		memcpy(dev_drv->init_on, table, dev_drv->init_on_cnt);
	} else {
		dev_drv->init_off = kcalloc(dev_drv->init_off_cnt,
					    sizeof(unsigned char), GFP_KERNEL);
		if (!dev_drv->init_off)
			goto init_table_dynamic_ukey_err;
		memcpy(dev_drv->init_off, table, dev_drv->init_off_cnt);
	}

	kfree(table);
	return 0;

init_table_dynamic_ukey_err:
	kfree(table);
	return -1;
}

static int ldim_dev_get_config_from_dts(struct ldim_dev_driver_s *dev_drv,
					struct device_node *np, int index,
					phandle pwm_phandle)
{
	char propname[20];
	struct device_node *child;
	const char *str;
	unsigned int *temp, val, size;
	struct bl_pwm_config_s *bl_pwm;
	struct ldim_profile_s *profile;
	struct ldim_fw_s *fw = aml_ldim_get_fw();
	struct ldim_fw_custom_s *fw_cus = aml_ldim_get_fw_cus();
	int i, ret = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		LDIMPR("load ldim_dev config from dts\n");

	size = (dev_drv->zone_num >= 10) ? dev_drv->zone_num : 10;
	temp = kcalloc(size, sizeof(unsigned int), GFP_KERNEL);
	if (!temp)
		return -1;

	/* get device config */
	sprintf(propname, "ldim_dev_%d", index);
	LDIMPR("load: %s\n", propname);
	child = of_get_child_by_name(np, propname);
	if (!child) {
		LDIMERR("failed to get %s\n", propname);
		goto ldim_get_config_err;
	}

	ret = of_property_read_string(child, "ldim_dev_name", &str);
	if (ret) {
		LDIMERR("failed to get ldim_dev_name\n");
		str = "ldim_dev";
	}
	strncpy(dev_drv->name, str, (LDIM_DEV_NAME_MAX - 1));

	ret = of_property_read_u32(child, "type", &val);
	if (ret) {
		LDIMERR("failed to get type\n");
		dev_drv->type = LDIM_DEV_TYPE_NORMAL;
	} else {
		dev_drv->type = val;
		if (ldim_debug_print)
			LDIMPR("type: %d\n", dev_drv->type);
	}
	if (dev_drv->type >= LDIM_DEV_TYPE_MAX) {
		LDIMERR("type num is out of support\n");
		goto ldim_get_config_err;
	}

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		/* get spi config */
		dev_drv->spi_info = &ldim_spi_info;
		ret = of_property_read_u32(child, "spi_bus_num", &val);
		if (ret) {
			LDIMERR("failed to get spi_bus_num\n");
		} else {
			ldim_spi_info.bus_num = val;
			if (ldim_debug_print) {
				LDIMPR("spi bus_num: %d\n",
				       ldim_spi_info.bus_num);
			}
		}

		ret = of_property_read_u32(child, "spi_chip_select", &val);
		if (ret) {
			LDIMERR("failed to get spi_chip_select\n");
		} else {
			ldim_spi_info.chip_select = val;
			if (ldim_debug_print) {
				LDIMPR("spi chip_select: %d\n",
				       ldim_spi_info.chip_select);
			}
		}

		ret = of_property_read_u32(child, "spi_max_frequency", &val);
		if (ret) {
			LDIMERR("failed to get spi_chip_select\n");
		} else {
			ldim_spi_info.max_speed_hz = val;
			if (ldim_debug_print) {
				LDIMPR("spi max_speed_hz: %d\n",
				       ldim_spi_info.max_speed_hz);
			}
		}

		ret = of_property_read_u32(child, "spi_mode", &val);
		if (ret) {
			LDIMERR("failed to get spi_mode\n");
		} else {
			ldim_spi_info.mode = val;
			if (ldim_debug_print)
				LDIMPR("spi mode: %d\n", ldim_spi_info.mode);
		}

		ret = of_property_read_u32(child, "spi_dma_support", &val);
		if (ret) {
			LDIMERR("failed to get spi_dma_support\n");
		} else {
			dev_drv->dma_support = (unsigned char)val;
			if (ldim_debug_print) {
				LDIMPR("spi_dma_support: %d\n",
				       dev_drv->dma_support);
			}
		}

		ret = of_property_read_u32_array(child, "spi_cs_delay",
						 &temp[0], 2);
		if (ret) {
			dev_drv->cs_hold_delay = 0;
			dev_drv->cs_clk_delay = 0;
		} else {
			dev_drv->cs_hold_delay = temp[0];
			dev_drv->cs_clk_delay = temp[1];
		}
		break;
	default:
		break;
	}

	/* ldim pwm config */
	bl_pwm = &dev_drv->ldim_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	ret = of_property_read_string(child, "ldim_pwm_port", &str);
	if (ret) {
		LDIMERR("failed to get ldim_pwm_port\n");
	} else {
		bl_pwm->pwm_port = bl_pwm_str_to_num(str);
		LDIMPR("ldim_pwm_port: %s(0x%x)\n", str, bl_pwm->pwm_port);
	}
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		ret = of_property_read_u32_array(child, "ldim_pwm_attr",
						 temp, 3);
		if (ret) {
			LDIMERR("failed to get ldim_pwm_attr\n");
			bl_pwm->pwm_method = BL_PWM_POSITIVE;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				bl_pwm->pwm_freq = 1;
			else
				bl_pwm->pwm_freq = 60;
			bl_pwm->pwm_duty = 50;
		} else {
			bl_pwm->pwm_method = temp[0];
			bl_pwm->pwm_freq = temp[1];
			bl_pwm->pwm_duty = temp[2];
		}
		LDIMPR("get ldim_pwm pol = %d, freq = %d, dft duty = %d%%\n",
		       bl_pwm->pwm_method, bl_pwm->pwm_freq, bl_pwm->pwm_duty);

		bl_pwm_config_init(bl_pwm);

		if (bl_pwm->pwm_port < BL_PWM_VS) {
			bl_pwm_channel_register(dev_drv->dev,
						pwm_phandle, bl_pwm);
		}
	}

	/* analog pwm config */
	bl_pwm = &dev_drv->analog_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	ret = of_property_read_string(child, "analog_pwm_port", &str);
	if (ret)
		bl_pwm->pwm_port = BL_PWM_MAX;
	else
		bl_pwm->pwm_port = bl_pwm_str_to_num(str);
	if (bl_pwm->pwm_port < BL_PWM_VS) {
		LDIMPR("find analog_pwm_port: %s(%u)\n", str, bl_pwm->pwm_port);
		ret = of_property_read_u32_array(child, "analog_pwm_attr",
						 temp, 5);
		if (ret) {
			LDIMERR("failed to get analog_pwm_attr\n");
		} else {
			bl_pwm->pwm_method = temp[0];
			bl_pwm->pwm_freq = temp[1];
			bl_pwm->pwm_duty_max = temp[2];
			bl_pwm->pwm_duty_min = temp[3];
			bl_pwm->pwm_duty = temp[4];
		}
		LDIMPR("get analog_pwm pol = %d, freq = %d\n",
			bl_pwm->pwm_method, bl_pwm->pwm_freq);
		LDIMPR("duty max = %d, min = %d, default = %d\n",
			bl_pwm->pwm_duty_max,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty);

		bl_pwm_config_init(bl_pwm);
		bl_pwm_channel_register(dev_drv->dev, pwm_phandle, bl_pwm);
	}

	ret = of_property_read_string(child, "ldim_pwm_pinmux_sel", &str);
	if (ret) {
		strcpy(dev_drv->pinmux_name, "invalid");
	} else {
		LDIMPR("find customer ldim_pwm_pinmux_sel: %s\n", str);
		strcpy(dev_drv->pinmux_name, str);
	}

	ret = of_property_read_u32_array(child, "en_gpio_on_off", temp, 3);
	if (ret) {
		LDIMERR("failed to get en_gpio_on_off\n");
		dev_drv->en_gpio = BL_GPIO_MAX;
		dev_drv->en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		dev_drv->en_gpio_off = BL_GPIO_OUTPUT_LOW;
	} else {
		if (temp[0] >= BL_GPIO_NUM_MAX) {
			dev_drv->en_gpio = BL_GPIO_MAX;
		} else {
			dev_drv->en_gpio = temp[0];
			ldim_gpio_probe(dev_drv, dev_drv->en_gpio);
		}
		dev_drv->en_gpio_on = temp[1];
		dev_drv->en_gpio_off = temp[2];
	}

	ret = of_property_read_u32(child, "lamp_err_gpio", &val);
	if (ret) {
		dev_drv->lamp_err_gpio = BL_GPIO_MAX;
		dev_drv->fault_check = 0;
	} else {
		if (val >= BL_GPIO_NUM_MAX) {
			dev_drv->lamp_err_gpio = BL_GPIO_MAX;
			dev_drv->fault_check = 0;
		} else {
			dev_drv->lamp_err_gpio = val;
			dev_drv->fault_check = 1;
			ldim_gpio_probe(dev_drv, dev_drv->lamp_err_gpio);
			ldim_gpio_set(dev_drv, dev_drv->lamp_err_gpio,
				      BL_GPIO_INPUT);
		}
	}

	ret = of_property_read_u32(child, "spi_write_check", &val);
	if (ret)
		dev_drv->write_check = 0;
	else
		dev_drv->write_check = (unsigned char)val;

	ret = of_property_read_u32_array(child, "dim_max_min", &temp[0], 2);
	if (ret) {
		LDIMERR("failed to get dim_max_min\n");
		dev_drv->dim_max = 0xfff;
		dev_drv->dim_min = 0x7f;
	} else {
		dev_drv->dim_max = temp[0];
		dev_drv->dim_min = temp[1];
	}

	ret = of_property_read_u32(child, "mcu_header", &val);
	if (ret)
		dev_drv->mcu_header = 0;
	else
		dev_drv->mcu_header = (unsigned int)val;

	ret = of_property_read_u32(child, "mcu_dim", &val);
	if (ret)
		dev_drv->mcu_dim = 0;
	else
		dev_drv->mcu_dim = (unsigned int)val;
	LDIMPR("mcu_header: 0x%x, mcu_dim: 0x%x\n",
	dev_drv->mcu_header, dev_drv->mcu_dim);

	ret = of_property_read_u32(child, "chip_count", &val);
	if (ret)
		dev_drv->chip_cnt = 1;
	else
		dev_drv->chip_cnt = val;
	LDIMPR("chip_count: %d\n", dev_drv->chip_cnt);

	ret = of_property_read_string(child, "ldim_zone_mapping_path", &str);
	if (ret == 0) {
		LDIMPR("find custom ldim_zone_mapping\n");
		strncpy(dev_drv->bl_mapping_path, str, 255);
		ret = ldim_dev_zone_mapping_load(dev_drv, str);
		if (ret) {
			for (i = 0; i < dev_drv->zone_num; i++)
				dev_drv->bl_mapping[i] = (unsigned short)i;
		}
		goto ldim_dev_get_config_from_dts_profile;
	}
	ret = of_property_read_u32_array(child, "ldim_zone_mapping",
					 &temp[0], dev_drv->zone_num);
	if (ret) {
		ret = of_property_read_u32_array(child, "ldim_region_mapping",
						 &temp[0], dev_drv->zone_num);
		if (ret) {
			for (i = 0; i < dev_drv->zone_num; i++)
				dev_drv->bl_mapping[i] = (unsigned short)i;
			goto ldim_dev_get_config_from_dts_profile;
		}
	}
	LDIMPR("find custom ldim_zone_mapping\n");
	for (i = 0; i < dev_drv->zone_num; i++)
		dev_drv->bl_mapping[i] = (unsigned short)temp[i];

ldim_dev_get_config_from_dts_profile:
	ret = of_property_read_u32(child, "ldim_bl_profile_mode", &val);
	if (ret)
		goto ldim_dev_get_config_from_dts_next;

	LDIMPR("find ldim_bl_profile_mode=%d\n", val);
	profile = kzalloc(sizeof(*profile), GFP_KERNEL);
	if (!profile) {
		LDIMERR("ld_profile malloc failed\n");
		goto ldim_dev_get_config_from_dts_next;
	}
	fw->profile = profile;
	profile->mode = val;

	if (profile->mode == 2) {
		LDIMPR("load bl_profile\n");
		ret = of_property_read_u32_array(child,
			"ldim_bl_profile_k_bits", &temp[0], 2);
		if (ret) {
			LDIMERR("failed to get ldim_bl_profile_k_bits\n");
			profile->profile_k = 24;
			profile->profile_bits = 640;
		} else {
			profile->profile_k = temp[0];
			profile->profile_bits = temp[1];
		}

		ret = of_property_read_string(child, "ldim_bl_profile_path",
					      &str);
		if (ret) {
			LDIMERR("failed to get ldim_bl_profile_path\n");
			strcpy(profile->file_path, "null");
		} else {
			strncpy(profile->file_path, str, 255);
		}
	}

ldim_dev_get_config_from_dts_next:
	/* get cus_param , maximum = 32 x 4 bytes */
	for (i = 0; i < 32; i++) {
		ret = of_property_read_u32_index(child,
			"ldim_cus_param", i, &val);
		if (ret) {
			LDIMPR("get ldim_cus_param[%d] failed, size =%d\n",
				i, i);
			i = 32;
		} else {
			if (fw_cus->fw_alg_frm) {
				fw_cus->param[i] = val;
				LDIMPR("param[%d] = %d\n",
					i, fw_cus->param[i]);
			}
		}
	}

	/* get init_cmd */
	ret = of_property_read_u32(child, "cmd_size", &val);
	if (ret) {
		LDIMPR("no cmd_size\n");
		dev_drv->cmd_size = 0;
	} else {
		dev_drv->cmd_size = (unsigned char)val;
	}
	if (ldim_debug_print)
		LDIMPR("%s: cmd_size = %d\n", dev_drv->name, dev_drv->cmd_size);
	if (dev_drv->cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
		goto ldim_dev_get_config_from_dts_end;

	ret = ldim_dev_init_dynamic_load_dts(child, dev_drv, 1);
	if (ret)
		goto ldim_get_config_err;
	ret = ldim_dev_init_dynamic_load_dts(child, dev_drv, 0);
	if (ret)
		goto ldim_get_config_err;
	dev_drv->init_loaded = 1;

ldim_dev_get_config_from_dts_end:
	kfree(temp);
	return 0;

ldim_get_config_err:
	kfree(temp);
	return -1;
}

static int ldim_dev_get_config_from_ukey(struct ldim_dev_driver_s *dev_drv,
					 phandle pwm_phandle)
{
	unsigned char *para, *p;
	int key_len, len;
	const char *str;
	unsigned int temp, temp_val;
	struct aml_lcd_unifykey_header_s ldev_header;
	struct bl_pwm_config_s *bl_pwm;
	struct ldim_profile_s *profile;
	int i, ret = 0;
	struct ldim_fw_s *fw = aml_ldim_get_fw();
	struct ldim_fw_custom_s *fw_cus = aml_ldim_get_fw_cus();

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		LDIMPR("load ldim_dev config from unifykey\n");

	key_len = LCD_UKEY_LDIM_DEV_SIZE;
	para = kcalloc(key_len, (sizeof(unsigned char)), GFP_KERNEL);
	if (!para) {
		LDIMERR("para kcalloc failed!!\n");
		return -1;
	}

	ret = lcd_unifykey_get("ldim_dev", para, &key_len);
	if (ret < 0) {
		LDIMERR("get ldim_dev unifykey failed!!\n");
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	len = LCD_UKEY_HEAD_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LDIMERR("unifykey header length is incorrect\n");
		kfree(para);
		return -1;
	}

	lcd_unifykey_header_check(para, &ldev_header);
	LDIMPR("unifykey version: 0x%04x\n", ldev_header.version);
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		LDIMPR("unifykey header:\n");
		LDIMPR("crc32             = 0x%08x\n", ldev_header.crc32);
		LDIMPR("data_len          = %d\n", ldev_header.data_len);
	}

	/* step 2: check backlight parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LDIMERR("unifykey length is incorrect\n");
		kfree(para);
		return -1;
	}

	/* basic: 30byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strncpy(dev_drv->name, str, (LDIM_DEV_NAME_MAX - 1));

	/* interface (25Byte) */
	dev_drv->type = *(p + LCD_UKEY_LDIM_DEV_IF_TYPE);
	if (dev_drv->type >= LDIM_DEV_TYPE_MAX) {
		LDIMERR("invalid type %d\n", dev_drv->type);
		kfree(para);
		return -1;
	}

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		/* get spi config */
		dev_drv->spi_info = &ldim_spi_info;

		ldim_spi_info.bus_num = *(p + LCD_UKEY_LDIM_DEV_IF_ATTR_0);
		ldim_spi_info.chip_select = *(p + LCD_UKEY_LDIM_DEV_IF_ATTR_1);
		ldim_spi_info.max_speed_hz =
			(*(p + LCD_UKEY_LDIM_DEV_IF_FREQ) |
			((*(p + LCD_UKEY_LDIM_DEV_IF_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_LDIM_DEV_IF_FREQ + 2)) << 16) |
			((*(p + LCD_UKEY_LDIM_DEV_IF_FREQ + 3)) << 24));
		ldim_spi_info.mode = *(p + LCD_UKEY_LDIM_DEV_IF_ATTR_2);
		dev_drv->dma_support = *(p + LCD_UKEY_LDIM_DEV_IF_ATTR_3);
		dev_drv->cs_hold_delay =
			(*(p + LCD_UKEY_LDIM_DEV_IF_ATTR_4) |
			((*(p + LCD_UKEY_LDIM_DEV_IF_ATTR_4 + 1)) << 8));
		dev_drv->cs_clk_delay =
			(*(p + LCD_UKEY_LDIM_DEV_IF_ATTR_5) |
			((*(p + LCD_UKEY_LDIM_DEV_IF_ATTR_5 + 1)) << 8));
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			LDIMPR("spi bus_num: %d\n", ldim_spi_info.bus_num);
			LDIMPR("spi chip_select: %d\n",
			       ldim_spi_info.chip_select);
			LDIMPR("spi max_speed_hz: %d\n",
			       ldim_spi_info.max_speed_hz);
			LDIMPR("spi mode: %d\n", ldim_spi_info.mode);
			LDIMPR("spi dma_support: %d\n", dev_drv->dma_support);
		}
		break;
	default:
		break;
	}

	/* pwm (48Byte) */
	bl_pwm = &dev_drv->ldim_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	bl_pwm->pwm_port = *(p + LCD_UKEY_LDIM_DEV_PWM_VS_PORT);
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		bl_pwm->pwm_method = *(p + LCD_UKEY_LDIM_DEV_PWM_VS_POL);
		bl_pwm->pwm_freq =
			(*(p + LCD_UKEY_LDIM_DEV_PWM_VS_FREQ) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_VS_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_VS_FREQ + 2)) << 16) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_VS_FREQ + 3)) << 24));
		bl_pwm->pwm_duty =
			(*(p + LCD_UKEY_LDIM_DEV_PWM_VS_DUTY) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_VS_DUTY + 1)) << 8));
		LDIMPR("get ldim_pwm pol = %d, freq = %d, dft duty = %d%%\n",
		       bl_pwm->pwm_method, bl_pwm->pwm_freq, bl_pwm->pwm_duty);
		bl_pwm_config_init(bl_pwm);
		if (bl_pwm->pwm_port < BL_PWM_VS) {
			bl_pwm_channel_register(dev_drv->dev,
						pwm_phandle, bl_pwm);
		}
	}

	bl_pwm = &dev_drv->analog_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	bl_pwm->pwm_port = *(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_PORT);
	if (bl_pwm->pwm_port < BL_PWM_VS) {
		bl_pwm->pwm_method = *(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_POL);
		bl_pwm->pwm_freq =
			(*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_FREQ) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_FREQ + 2)) << 16) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_FREQ + 3)) << 24));
		bl_pwm->pwm_duty =
			(*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_DUTY) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_DUTY + 1)) << 8));
		bl_pwm->pwm_duty_max =
			(*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_0) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_0 + 1)) << 8));
		bl_pwm->pwm_duty_min =
			(*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_1) |
			((*(p + LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_1 + 1)) << 8));
		LDIMPR("get analog_pwm pol = %d, freq = %d\n",
			bl_pwm->pwm_method, bl_pwm->pwm_freq);
		LDIMPR("duty max = %d, min = %d, default = %d\n",
			bl_pwm->pwm_duty_max,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty);
		bl_pwm_config_init(bl_pwm);
		bl_pwm_channel_register(dev_drv->dev, pwm_phandle, bl_pwm);
	}

	str = (const char *)(p + LCD_UKEY_LDIM_DEV_PINMUX_SEL);
	if (strlen(str) == 0) {
		strcpy(dev_drv->pinmux_name, "invalid");
	} else {
		strncpy(dev_drv->pinmux_name, str, (LDIM_DEV_NAME_MAX - 1));
		LDIMPR("find customer ldim_pwm_pinmux_sel: %s\n", str);
	}

	/* ctrl (271Byte) */
	temp = *(p + LCD_UKEY_LDIM_DEV_EN_GPIO);
	if (temp >= BL_GPIO_NUM_MAX) {
		dev_drv->en_gpio = BL_GPIO_MAX;
	} else {
		dev_drv->en_gpio = temp;
		ldim_gpio_probe(dev_drv, dev_drv->en_gpio);
	}
	dev_drv->en_gpio_on = *(p + LCD_UKEY_LDIM_DEV_EN_GPIO_ON);
	dev_drv->en_gpio_off = *(p + LCD_UKEY_LDIM_DEV_EN_GPIO_OFF);

	temp = *(p + LCD_UKEY_LDIM_DEV_ERR_GPIO);
	if (temp >= BL_GPIO_NUM_MAX) {
		dev_drv->lamp_err_gpio = BL_GPIO_MAX;
		dev_drv->fault_check = 0;
	} else {
		dev_drv->lamp_err_gpio = temp;
		dev_drv->fault_check = 1;
		ldim_gpio_probe(dev_drv, dev_drv->lamp_err_gpio);
		ldim_gpio_set(dev_drv, dev_drv->lamp_err_gpio, BL_GPIO_INPUT);
	}

	dev_drv->write_check = *(p + LCD_UKEY_LDIM_DEV_WRITE_CHECK);

	dev_drv->dim_max =
		(*(p + LCD_UKEY_LDIM_DEV_DIM_MAX) |
		((*(p + LCD_UKEY_LDIM_DEV_DIM_MAX + 1)) << 8));
	dev_drv->dim_min =
		(*(p + LCD_UKEY_LDIM_DEV_DIM_MIN) |
		((*(p + LCD_UKEY_LDIM_DEV_DIM_MIN + 1)) << 8));

	dev_drv->chip_cnt =
		(*(p + LCD_UKEY_LDIM_DEV_CHIP_COUNT) |
		((*(p + LCD_UKEY_LDIM_DEV_CHIP_COUNT + 1)) << 8));
	LDIMPR("chip_count: %d\n", dev_drv->chip_cnt);

	dev_drv->mcu_header =
		(*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_0) |
		((*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_0 + 1)) << 8) |
		((*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_0 + 2)) << 16) |
		((*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_0 + 3)) << 24));
	dev_drv->mcu_dim =
		(*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_1) |
		((*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_1 + 1)) << 8) |
		((*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_1 + 2)) << 16) |
		((*(p + LCD_UKEY_LDIM_DEV_CUST_ATTR_1 + 3)) << 24));
	LDIMPR("mcu_header: 0x%x, mcu_dim: 0x%x\n",
	dev_drv->mcu_header, dev_drv->mcu_dim);

	str = (const char *)(p + LCD_UKEY_LDIM_DEV_ZONE_MAP_PATH);
	if (strlen(str) == 0) {
		for (i = 0; i < dev_drv->zone_num; i++)
			dev_drv->bl_mapping[i] = (unsigned short)i;
	} else {
		LDIMPR("find custom ldim_zone_mapping\n");
		strncpy(dev_drv->bl_mapping_path, str, 255);
		ret = ldim_dev_zone_mapping_load(dev_drv, str);
		if (ret) {
			for (i = 0; i < dev_drv->zone_num; i++)
				dev_drv->bl_mapping[i] = (unsigned short)i;
		}
	}

	/* profile (273Byte) */
	temp = *(p + LCD_UKEY_LDIM_DEV_PROFILE_MODE);
	if (temp == 0)
		goto ldim_dev_get_config_from_ukey_next;
	LDIMPR("find ldim_bl_profile_mode=%d\n", temp);
	profile = kzalloc(sizeof(*profile), GFP_KERNEL);
	if (!profile)
		goto ldim_dev_get_config_from_ukey_next;
	fw->profile = profile;
	profile->mode = temp;

	if (profile->mode == 1) {
		LDIMPR("load bl_profile\n");
		str = (const char *)(p + LCD_UKEY_LDIM_DEV_PROFILE_PATH);
		if (strlen(str) == 0) {
			LDIMERR("failed to get ldim_bl_profile_path\n");
			strcpy(profile->file_path, "null");
		} else {
			strncpy(profile->file_path, str, 255);
		}
	} else if (profile->mode == 2) {
		LDIMPR("load bl_profile\n");
		str = (const char *)(p + LCD_UKEY_LDIM_DEV_PROFILE_PATH);
		if (strlen(str) == 0) {
			LDIMERR("failed to get ldim_bl_profile_path\n");
			strcpy(profile->file_path, "null");
		} else {
			strncpy(profile->file_path, str, 255);
		}

		profile->profile_k =
			(*(p + LCD_UKEY_LDIM_DEV_PROFILE_ATTR_0) |
			((*(p + LCD_UKEY_LDIM_DEV_PROFILE_ATTR_0 + 1)) << 8));
		profile->profile_bits =
			(*(p + LCD_UKEY_LDIM_DEV_PROFILE_ATTR_1) |
			((*(p + LCD_UKEY_LDIM_DEV_PROFILE_ATTR_1 + 1)) << 8));
	}

ldim_dev_get_config_from_ukey_next:
	/* param_data : maximum = 32 x 4 Bytes */
	temp = *(p + LCD_UKEY_LDIM_DEV_CUST_PARAM_SIZE);
	LDIMPR("custom param size = %d\n",  temp);
	for (i = 0; i < temp; i++) {
		temp_val =
			(*(p + LCD_UKEY_LDIM_DEV_CUST_PARAM
			+ 4 * i) |
			((*(p + LCD_UKEY_LDIM_DEV_CUST_PARAM
			+ 4 * i + 1)) << 8) |
			((*(p + LCD_UKEY_LDIM_DEV_CUST_PARAM
			+ 4 * i + 2)) << 16) |
			((*(p + LCD_UKEY_LDIM_DEV_CUST_PARAM
			+ 4 * i + 3)) << 24));
		if (fw_cus->fw_alg_frm)	{
			fw_cus->param[i] = temp_val;
			LDIMPR("fw_cus->param[%d] = %d =  0x%x\n",
			i, fw_cus->param[i],
			fw_cus->param[i]);
		}
	}

	dev_drv->cmd_size = *(p + LCD_UKEY_LDIM_DEV_CMD_SIZE);
	if (ldim_debug_print)
		LDIMPR("%s: cmd_size = %d\n", dev_drv->name, dev_drv->cmd_size);
	if (dev_drv->cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
		goto ldim_dev_get_config_from_ukey_end;

	ret = ldim_dev_init_dynamic_load_ukey(dev_drv, p, key_len, len, 1);
	if (ret)
		goto ldim_dev_get_config_from_ukey_err;
	ret = ldim_dev_init_dynamic_load_ukey(dev_drv, p, key_len, len, 0);
	if (ret)
		goto ldim_dev_get_config_from_ukey_err;
	dev_drv->init_loaded = 1;

ldim_dev_get_config_from_ukey_end:
	kfree(para);
	return 0;

ldim_dev_get_config_from_ukey_err:
	kfree(para);
	return -1;
}

static int ldim_dev_get_config(struct ldim_dev_driver_s *dev_drv,
			       struct device_node *np, int index)
{
	unsigned int val;
	phandle pwm_phandle;
	int ret = 0;

	ret = of_property_read_u32(np, "key_valid", &val);
	if (ret) {
		LDIMERR("failed to get key_valid\n");
		val = 0;
	}
	dev_drv->key_valid = val;
	LDIMPR("key_valid: %d\n", dev_drv->key_valid);

	ret = of_property_read_u32(np, "ldim_pwm_config", &pwm_phandle);
	if (ret) {
		LDIMERR("failed to get ldim_pwm_config node\n");
		return -1;
	}

	if (dev_drv->key_valid) {
		ret = ldim_dev_get_config_from_ukey(dev_drv, pwm_phandle);
	} else {
		ret = ldim_dev_get_config_from_dts(dev_drv, np, index,
						   pwm_phandle);
	}

	return ret;
}

static ssize_t ldim_dev_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ldim_dev_config_print(ldim_drv);

	return ret;
}

static ssize_t ldim_dev_pwm_ldim_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct bl_pwm_config_s *bl_pwm;
	ssize_t len = 0;

	bl_pwm = &ldim_dev_drv.ldim_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		len += sprintf(buf + len,
			"ldim_pwm: freq=%d, pol=%d, duty_max=%d, duty_min=%d,",
			bl_pwm->pwm_freq, bl_pwm->pwm_method,
			bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		len += sprintf(buf + len, " duty_value=%d\n",
			       bl_pwm->pwm_duty);
	}

	return len;
}

static void ldim_dev_pwm_debug(struct bl_pwm_config_s *bl_pwm, const char *buf, int dim_flag)
{
	unsigned int val = 0;
	int ret;

	switch (buf[0]) {
	case 'f': /* frequency */
		ret = sscanf(buf, "freq %d", &val);
		if (ret == 1) {
			bl_pwm->pwm_freq = val;
			bl_pwm_config_init(bl_pwm);
			ldim_set_duty_pwm(bl_pwm);
			if (ldim_debug_print) {
				LDIMPR("set ldim_pwm (port 0x%x): freq = %dHz\n",
				       bl_pwm->pwm_port, bl_pwm->pwm_freq);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
		break;
	case 'd': /* duty */
		ret = sscanf(buf, "duty %d", &val);
		if (ret == 1) {
			bl_pwm->pwm_duty = val;
			ldim_set_duty_pwm(bl_pwm);
			if (ldim_debug_print) {
				LDIMPR("set ldim_pwm (port 0x%x): duty = %d\n",
				       bl_pwm->pwm_port, bl_pwm->pwm_duty);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
		break;
	case 'p': /* polarity */
		ret = sscanf(buf, "pol %d", &val);
		if (ret == 1) {
			bl_pwm->pwm_method = val;
			bl_pwm_config_init(bl_pwm);
			ldim_set_duty_pwm(bl_pwm);
			if (ldim_debug_print) {
				LDIMPR("set ldim_pwm (port 0x%x): method = %d\n",
				       bl_pwm->pwm_port, bl_pwm->pwm_method);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
		break;
	case 'm':
		if (buf[1] == 'a') { /* max */
			ret = sscanf(buf, "max %d", &val);
			if (ret == 1) {
				bl_pwm->pwm_duty_max = val;
				if (ldim_dev_drv.dim_range_update && dim_flag)
					ldim_dev_drv.dim_range_update(&ldim_dev_drv);
				bl_pwm_config_init(bl_pwm);
				ldim_set_duty_pwm(bl_pwm);
				if (ldim_debug_print) {
					LDIMPR("set ldim_pwm (port 0x%x): duty_max = %d\n",
				 bl_pwm->pwm_port,
				 bl_pwm->pwm_duty_max);
				}
			} else {
				LDIMERR("invalid parameters\n");
			}
		} else if (buf[1] == 'i') { /* min */
			ret = sscanf(buf, "min %d", &val);
			if (ret == 1) {
				bl_pwm->pwm_duty_min = val;
				if (ldim_dev_drv.dim_range_update && dim_flag)
					ldim_dev_drv.dim_range_update(&ldim_dev_drv);
				bl_pwm_config_init(bl_pwm);
				ldim_set_duty_pwm(bl_pwm);
				if (ldim_debug_print) {
					LDIMPR("set ldim_pwm (port 0x%x): duty_min = %d\n",
				 bl_pwm->pwm_port,
				 bl_pwm->pwm_duty_min);
				}
			} else {
				LDIMERR("invalid parameters\n");
			}
		}
		break;
	default:
		LDIMERR("wrong command\n");
		break;
	}
}

static ssize_t ldim_dev_pwm_ldim_store(struct class *class, struct class_attribute *attr,
				       const char *buf, size_t count)
{
	struct bl_pwm_config_s *bl_pwm;

	bl_pwm = &ldim_dev_drv.ldim_pwm_config;
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return count;

	ldim_dev_pwm_debug(bl_pwm, buf, 1);

	return count;
}

static ssize_t ldim_dev_pwm_analog_show(struct class *class, struct class_attribute *attr,
					char *buf)
{
	struct bl_pwm_config_s *bl_pwm;
	ssize_t len = 0;

	bl_pwm = &ldim_dev_drv.analog_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_VS) {
		len += sprintf(buf + len,
			"analog_pwm: freq=%d, pol=%d, duty_max=%d, duty_min=%d,",
			bl_pwm->pwm_freq, bl_pwm->pwm_method,
			bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		len += sprintf(buf + len, " duty_value=%d\n", bl_pwm->pwm_duty);
	}

	return len;
}

static ssize_t ldim_dev_pwm_analog_store(struct class *class, struct class_attribute *attr,
					 const char *buf, size_t count)
{
	struct bl_pwm_config_s *bl_pwm;

	bl_pwm = &ldim_dev_drv.analog_pwm_config;
	if (bl_pwm->pwm_port >= BL_PWM_VS)
		return count;

	ldim_dev_pwm_debug(bl_pwm, buf, 0);

	return count;
}

struct ldim_dev_dbg_reg_s {
	unsigned char chip_id;
	unsigned char reg;
	unsigned int rd_cnt;
};

static struct ldim_dev_dbg_reg_s ldim_dev_dbg_reg;

static ssize_t ldim_dev_reg_show(struct class *class, struct class_attribute *attr, char *buf)
{
	unsigned char *data;
	ssize_t len = 0;
	int i, ret;

	if (!ldim_dev_drv.reg_read)
		return sprintf(buf, "ldim dev_drv reg_read is null\n");

	mutex_lock(&ldim_dev_dbg_mutex);
	if (ldim_dev_dbg_reg.rd_cnt == 0) {
		mutex_unlock(&ldim_dev_dbg_mutex);
		return sprintf(buf, "ldim_dev_dbg_reg rd_cnt is 0\n");
	}

	data = kcalloc(ldim_dev_dbg_reg.rd_cnt, sizeof(unsigned char), GFP_KERNEL);
	if (!data) {
		mutex_unlock(&ldim_dev_dbg_mutex);
		return sprintf(buf, "ldim data buff malloc failed\n");
	}
	ret = ldim_dev_drv.reg_read(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
				    ldim_dev_dbg_reg.reg, data, ldim_dev_dbg_reg.rd_cnt);
	if (ret) {
		len = sprintf(buf, "reg[0x%02x] read error\n", ldim_dev_dbg_reg.reg);
	} else {
		len += sprintf(buf + len, "chip_id[%d] reg read:\n",
			       ldim_dev_dbg_reg.chip_id);
		for (i = 0; i < ldim_dev_dbg_reg.rd_cnt; i++) {
			len += sprintf(buf + len, "reg[0x%02x] = 0x%02x\n",
				       ldim_dev_dbg_reg.reg + i, data[i]);
		}
	}
	kfree(data);

	mutex_unlock(&ldim_dev_dbg_mutex);
	return len;
}

static ssize_t ldim_dev_reg_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int reg = 0, val = 0, id = 0;
	unsigned char data, *rbuf;
	unsigned int i, ret;

	mutex_lock(&ldim_dev_dbg_mutex);
	if (buf[0] == 'w') {
		ret = sscanf(buf, "w %x %x %x", &id, &reg, &val);
		if (ret == 3) {
			if (!ldim_dev_drv.reg_write) {
				LDIMERR("ldim dev_drv reg_write is null\n");
				goto ldim_dev_reg_store_end;
			}
			if (reg > 0xff) {
				LDIMERR("invalid reg address: 0x%x\n", reg);
				goto ldim_dev_reg_store_end;
			}
			ldim_dev_dbg_reg.chip_id = (unsigned char)id;
			ldim_dev_dbg_reg.reg = (unsigned char)reg;
			ldim_dev_dbg_reg.rd_cnt = 1;
			data = (unsigned char)val;
			ldim_dev_drv.reg_write(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
					       ldim_dev_dbg_reg.reg, &data, 1);
			LDIMPR("write chip_id[%d] reg[0x%02x] = 0x%02x\n",
			       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg, data);
		} else {
			LDIMERR("invalid parameters\n");
		}
	} else if (buf[0] == 'r') {
		ret = sscanf(buf, "r %x %x", &id, &reg);
		if (ret == 2) {
			if (!ldim_dev_drv.reg_read) {
				LDIMERR("ldim dev_drv reg_read is null\n");
				goto ldim_dev_reg_store_end;
			}
			if (reg > 0xff) {
				LDIMERR("invalid reg address: 0x%x\n", reg);
				goto ldim_dev_reg_store_end;
			}
			ldim_dev_dbg_reg.chip_id = (unsigned char)id;
			ldim_dev_dbg_reg.reg = (unsigned char)reg;
			ldim_dev_dbg_reg.rd_cnt = 1;
			ret = ldim_dev_drv.reg_read(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
						    ldim_dev_dbg_reg.reg, &data, 1);
			if (ret) {
				LDIMPR("chip_id[%d] reg[0x%02x] read error\n",
				       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg);
			} else {
				LDIMPR("chip_id[%d] reg[0x%02x] = 0x%02x\n",
				       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg, data);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
	} else if (buf[0] == 'd') {
		ret = sscanf(buf, "d %x %x %d", &id, &reg, &val);
		if (ret == 3) {
			if (!ldim_dev_drv.reg_read) {
				LDIMERR("ldim dev_drv reg_read is null\n");
				goto ldim_dev_reg_store_end;
			}
			if (reg > 0xff) {
				LDIMERR("invalid reg address: 0x%x\n", reg);
				goto ldim_dev_reg_store_end;
			}
			ldim_dev_dbg_reg.chip_id = (unsigned char)id;
			ldim_dev_dbg_reg.reg = (unsigned char)reg;
			ldim_dev_dbg_reg.rd_cnt = val;

			rbuf = kcalloc(ldim_dev_dbg_reg.rd_cnt, sizeof(unsigned char), GFP_KERNEL);
			if (!rbuf)
				goto ldim_dev_reg_store_end;
			ret = ldim_dev_drv.reg_read(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
						    ldim_dev_dbg_reg.reg, rbuf,
						    ldim_dev_dbg_reg.rd_cnt);
			if (ret) {
				LDIMERR("chip_id[%d] reg[0x%02x] read error\n",
				       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg);
			} else {
				LDIMPR("chip_id[%d] reg dump:\n", ldim_dev_dbg_reg.chip_id);
				for (i = 0; i < ldim_dev_dbg_reg.rd_cnt; i++) {
					LDIMPR("reg[0x%02x] = 0x%02x\n",
					       ldim_dev_dbg_reg.reg, rbuf[i]);
				}
			}
			kfree(rbuf);
		} else {
			LDIMERR("invalid parameters\n");
		}
	}

ldim_dev_reg_store_end:
	mutex_unlock(&ldim_dev_dbg_mutex);
	return count;
}

static struct class_attribute ldim_dev_class_attrs[] = {
	__ATTR(status, 0644, ldim_dev_show, NULL),
	__ATTR(pwm_ldim, 0644, ldim_dev_pwm_ldim_show, ldim_dev_pwm_ldim_store),
	__ATTR(pwm_analog, 0644, ldim_dev_pwm_analog_show, ldim_dev_pwm_analog_store),
	__ATTR(reg, 0644, ldim_dev_reg_show, ldim_dev_reg_store)
};

static void ldim_dev_class_create(struct ldim_dev_driver_s *dev_drv)
{
	int i;

	dev_drv->class = class_create(THIS_MODULE, "ldim_dev");
	if (IS_ERR_OR_NULL(dev_drv->class)) {
		LDIMERR("create ldim_dev class fail\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(ldim_dev_class_attrs); i++) {
		if (class_create_file(dev_drv->class, &ldim_dev_class_attrs[i])) {
			LDIMERR("create ldim_dev class attribute %s fail\n",
				ldim_dev_class_attrs[i].attr.name);
		}
	}
}

static int ldim_dev_add_driver(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		ret = ldim_spi_driver_add(dev_drv);
		break;
	case LDIM_DEV_TYPE_I2C:
		break;
	case LDIM_DEV_TYPE_NORMAL:
	default:
		break;
	}
	if (ret)
		return ret;

	ret = -1;
	if (strcmp(dev_drv->name, "iw7027") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_IW7027
		ret = ldim_dev_iw7027_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "blmcu") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_BLMCU
		ret = ldim_dev_blmcu_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "ob3350") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_OB3350
		ret = ldim_dev_ob3350_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "global") == 0) {
		ret = ldim_dev_global_probe(ldim_drv);
	} else {
		LDIMERR("invalid device name: %s\n", dev_drv->name);
	}

	if (ret) {
		LDIMERR("add device driver failed: %s(%d)\n",
			dev_drv->name, dev_drv->index);
	} else {
		ldim_dev_probe_flag = 1;
		LDIMPR("add device driver: %s(%d)\n",
		       dev_drv->name, dev_drv->index);
	}

	return ret;
}

static int ldim_dev_remove_driver(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = -1;

	if (ldim_dev_probe_flag) {
		if (strcmp(dev_drv->name, "iw7027") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_IW7027
			ret = ldim_dev_iw7027_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "blmcu") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_BLMCU
			ret = ldim_dev_blmcu_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "ob3350") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_OB3350
			ret = ldim_dev_ob3350_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "global") == 0) {
			ret = ldim_dev_global_remove(ldim_drv);
		} else {
			LDIMERR("invalid device name: %s\n", dev_drv->name);
		}

		if (ret) {
			LDIMERR("remove device driver failed: %s(%d)\n",
				dev_drv->name, dev_drv->index);
		} else {
			ldim_dev_probe_flag = 0;
			LDIMPR("remove device driver: %s(%d)\n",
				dev_drv->name, dev_drv->index);
		}
	}

	switch (ldim_dev_drv.type) {
	case LDIM_DEV_TYPE_SPI:
		ldim_spi_driver_remove(dev_drv);
		break;
	default:
		break;
	}

	return ret;
}

#define LDIM_DEV_PROBE_WAIT_TIMEOUT    16000
static void ldim_dev_probe_func(struct work_struct *work)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val;
	int ret = -1;

	if (ldim_drv->valid_flag == 0) {
		if (ldim_dev_probe_retry_cnt++ < LDIM_DEV_PROBE_WAIT_TIMEOUT) {
			lcd_queue_delayed_work(&ldim_dev_probe_dly_work, 10);
			return;
		}
	}
	if (ldim_drv->valid_flag == 0)
		return;

	ldim_dev_drv.index = ldim_drv->conf->dev_index;
	if (ldim_dev_drv.index == 0xff) {
		if (ldim_debug_print)
			LDIMPR("%s: invalid index\n", __func__);
		return;
	}

	/* get configs */
	ldim_drv->dev_drv = &ldim_dev_drv;
	ldim_dev_drv.bl_row = ldim_drv->conf->seg_row;
	ldim_dev_drv.bl_col = ldim_drv->conf->seg_col;
	ldim_dev_drv.spi_sync = ldim_drv->data->spi_sync;
	ldim_dev_drv.ldim_pwm_config.pwm_duty_max = 4095;
	ldim_dev_drv.analog_pwm_config.pwm_duty_max = 4095;
	val = ldim_dev_drv.bl_row * ldim_dev_drv.bl_col;
	ldim_dev_drv.zone_num = val;
	ldim_dev_drv.bl_mapping = kcalloc(val, sizeof(unsigned short), GFP_KERNEL);
	if (!ldim_dev_drv.bl_mapping)
		goto ldim_dev_probe_func_fail0;

	ret = ldim_dev_get_config(&ldim_dev_drv, ldim_dev_drv.dev->of_node,
				  ldim_dev_drv.index);
	if (ret)
		goto ldim_dev_probe_func_fail1;
	ldim_dev_drv.pinmux_ctrl = ldim_pwm_pinmux_ctrl;
	ldim_dev_drv.pwm_vs_update = ldim_pwm_vs_update;
	ldim_dev_drv.config_print = ldim_dev_config_print,

	ldim_dev_class_create(&ldim_dev_drv);
	ret = ldim_dev_add_driver(ldim_drv);
	if (ret)
		goto ldim_dev_probe_func_fail1;
	ldim_pwm_pinmux_ctrl(&ldim_dev_drv, 1);

	/* init ldim function */
	ldim_drv->init();

	LDIMPR("%s: ok\n", __func__);
	return;

ldim_dev_probe_func_fail1:
	kfree(ldim_dev_drv.bl_mapping);
ldim_dev_probe_func_fail0:
	pr_info("%s: failed\n", __func__);
}

static int ldim_dev_probe(struct platform_device *pdev)
{
	ldim_dev_probe_flag = 0;
	ldim_dev_drv.dev = &pdev->dev;
	/* set drvdata */
	platform_set_drvdata(pdev, &ldim_dev_drv);

	ldim_dev_probe_retry_cnt = 0;
	INIT_DELAYED_WORK(&ldim_dev_probe_dly_work, ldim_dev_probe_func);
	lcd_queue_delayed_work(&ldim_dev_probe_dly_work, 0);

	return 0;
}

static int __exit ldim_dev_remove(struct platform_device *pdev)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	cancel_delayed_work(&ldim_dev_probe_dly_work);
	if (ldim_dev_drv.index != 0xff)
		ldim_dev_remove_driver(ldim_drv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	LDIMPR("%s\n", __func__);

	return 0;
}

static void ldim_dev_shutdown(struct platform_device *pdev)
{
	cancel_delayed_work(&ldim_dev_probe_dly_work);
}

#ifdef CONFIG_OF
static const struct of_device_id ldim_dev_dt_match[] = {
	{
		.compatible = "amlogic, ldim_dev",
	},
	{}
};
#endif

static struct platform_driver ldim_dev_platform_driver = {
	.driver = {
		.name  = "ldim_dev",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ldim_dev_dt_match,
#endif
	},
	.probe   = ldim_dev_probe,
	.remove  = __exit_p(ldim_dev_remove),
	.shutdown = ldim_dev_shutdown,
};

int __init ldim_dev_init(void)
{
	if (platform_driver_register(&ldim_dev_platform_driver)) {
		LDIMPR("failed to register ldim_dev driver module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit ldim_dev_exit(void)
{
	platform_driver_unregister(&ldim_dev_platform_driver);
}

//MODULE_DESCRIPTION("LDIM device Driver for LCD Backlight");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");

