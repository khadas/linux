// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#ifdef CONFIG_AMLOGIC_BL_EXTERN
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#endif
#include "lcd_bl.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

#include <linux/amlogic/gki_module.h>

#define BL_CDEV_NAME  "aml_bl"
struct bl_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct bl_cdev_s *bl_cdev;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char bl_global_init_flag;
static unsigned int bl_drv_init_state;
static struct aml_bl_drv_s *bl_drv[LCD_MAX_DRV];
static int bl_index_lut[LCD_MAX_DRV] = {0xff, 0xff, 0xff};
static unsigned int bl_level[LCD_MAX_DRV];

static DEFINE_MUTEX(bl_status_mutex);
static DEFINE_MUTEX(bl_power_mutex);
static DEFINE_MUTEX(bl_level_mutex);

struct bl_method_match_s {
	char *name;
	enum bl_ctrl_method_e type;
};

static struct bl_method_match_s bl_method_match_table[] = {
	{"gpio",          BL_CTRL_GPIO},
	{"pwm",           BL_CTRL_PWM},
	{"pwm_combo",     BL_CTRL_PWM_COMBO},
	{"local_dimming", BL_CTRL_LOCAL_DIMMING},
	{"extern",        BL_CTRL_EXTERN},
	{"invalid",       BL_CTRL_MAX},
};

static char *bl_method_type_to_str(int type)
{
	int i;
	char *str = bl_method_match_table[BL_CTRL_MAX].name;

	for (i = 0; i < BL_CTRL_MAX; i++) {
		if (type == bl_method_match_table[i].type) {
			str = bl_method_match_table[i].name;
			break;
		}
	}
	return str;
}

static int aml_bl_check_driver(struct aml_bl_drv_s *bdrv)
{
	int ret = 0;

	if (!bdrv) {
		/*BLERR("no bl driver\n");*/
		return -1;
	}
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (!bdrv->bconf.bl_pwm) {
			ret = -1;
			BLERR("no bl_pwm struct\n");
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (!bdrv->bconf.bl_pwm_combo0) {
			ret = -1;
			BLERR("no bl_pwm_combo_0 struct\n");
		}
		if (!bdrv->bconf.bl_pwm_combo1) {
			ret = -1;
			BLERR("no bl_pwm_combo_1 struct\n");
		}
		break;
	case BL_CTRL_MAX:
		ret = -1;
		break;
	default:
		break;
	}

	return ret;
}

struct aml_bl_drv_s *aml_bl_get_driver(int index)
{
	if (index >= LCD_MAX_DRV) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLERR("%s: invalid index: %d\n", __func__, index);
		return NULL;
	}
	if (!bl_drv[index]) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLERR("no bl driver");
		return NULL;
	}

	return bl_drv[index];
}

static void bl_gpio_probe(struct aml_bl_drv_s *bdrv, int index)
{
	struct bl_gpio_s *bl_gpio;
	const char *str;
	int ret;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("gpio index %d, exit\n", index);
		return;
	}
	bl_gpio = &bdrv->bconf.bl_gpio[index];
	if (bl_gpio->probe_flag) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLPR("gpio %s[%d] is already registered\n",
			     bl_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(bdrv->dev->of_node,
					    "bl_gpio_names", index, &str);
	if (ret) {
		BLERR("failed to get bl_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(bl_gpio->name, str);

	/* init gpio flag */
	bl_gpio->probe_flag = 1;
	bl_gpio->register_flag = 0;
}

static int bl_gpio_register(struct aml_bl_drv_s *bdrv, int index, int init_value)
{
	struct bl_gpio_s *bl_gpio;
	int value;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	bl_gpio = &bdrv->bconf.bl_gpio[index];
	if (bl_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (bl_gpio->register_flag) {
		BLPR("%s: gpio %s[%d] is already registered\n",
		     __func__, bl_gpio->name, index);
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
	bl_gpio->gpio = devm_gpiod_get_index(bdrv->dev, "bl", index, value);
	if (IS_ERR(bl_gpio->gpio)) {
		BLERR("register gpio %s[%d]: %p, err: %d\n",
		      bl_gpio->name, index, bl_gpio->gpio,
		      IS_ERR(bl_gpio->gpio));
		return -1;
	}

	bl_gpio->register_flag = 1;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("register gpio %s[%d]: %p, init value: %d\n",
		     bl_gpio->name, index, bl_gpio->gpio, init_value);
	}

	return 0;
}

static void bl_gpio_set(struct aml_bl_drv_s *bdrv, int index, int value)
{
	struct bl_gpio_s *bl_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("gpio index %d, exit\n", index);
		return;
	}
	bl_gpio = &bdrv->bconf.bl_gpio[index];
	if (bl_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (bl_gpio->register_flag == 0) {
		bl_gpio_register(bdrv, index, value);
		return;
	}

	if (IS_ERR_OR_NULL(bl_gpio->gpio)) {
		BLERR("gpio %s[%d]: %p, err: %ld\n",
		      bl_gpio->name, index, bl_gpio->gpio,
		      PTR_ERR(bl_gpio->gpio));
		return;
	}

	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(bl_gpio->gpio, value);
		break;
	case BL_GPIO_INPUT:
	default:
		gpiod_direction_input(bl_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("set gpio %s[%d] value: %d\n",
		     bl_gpio->name, index, value);
	}
}

/* ****************************************************** */
#define BL_PINMUX_MAX    8
static char *bl_pinmux_str[BL_PINMUX_MAX] = {
	"pwm_on",               /* 0 */
	"pwm_vs_on",            /* 1 */
	"pwm_combo_0_1_on",     /* 2 */
	"pwm_combo_0_vs_1_on",  /* 3 */
	"pwm_combo_0_1_vs_on",  /* 4 */
	"pwm_off",              /* 5 */
	"pwm_combo_off",        /* 6 */
	"none",
};

static void bl_pwm_pinmux_set(struct aml_bl_drv_s *bdrv, int status)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	int index = 0xff;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s\n", bdrv->index, __func__);

	switch (bconf->method) {
	case BL_CTRL_PWM:
		if (status) {
			if (bconf->bl_pwm->pwm_port == BL_PWM_VS)
				index = 1;
			else
				index = 0;
		} else {
			index = 5;
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (status) {
			if (bconf->bl_pwm_combo0->pwm_port == BL_PWM_VS) {
				index = 3;
			} else {
				if (bconf->bl_pwm_combo1->pwm_port == BL_PWM_VS)
					index = 4;
				else
					index = 2;
			}
		} else {
			index = 6;
		}
		break;
	default:
		BLERR("[%d]: %s: wrong ctrl_mothod=%d\n",
		      bdrv->index, __func__, bconf->method);
		break;
	}

	if (index >= BL_PINMUX_MAX) {
		BLERR("[%d]: %s: pinmux index %d is invalid\n",
		      bdrv->index, __func__, index);
		return;
	}

	if (bdrv->pinmux_flag == index) {
		BLPR("[%d]: pinmux %s is already selected\n",
		     bdrv->index, bl_pinmux_str[index]);
		return;
	}

	/* request pwm pinmux */
	bdrv->pin = devm_pinctrl_get_select(bdrv->dev, bl_pinmux_str[index]);
	if (IS_ERR(bdrv->pin)) {
		BLERR("[%d]: set pinmux %s error\n",
		      bdrv->index, bl_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLPR("[%d]: set pinmux %s: %p\n",
			     bdrv->index, bl_pinmux_str[index], bdrv->pin);
		}
	}
	bdrv->pinmux_flag = index;
}

/* ****************************************************** */
static void bl_power_en_ctrl(struct aml_bl_drv_s *bdrv, int status)
{
	struct bl_config_s *bconf = &bdrv->bconf;

	if (status) {
		if (bconf->en_gpio < BL_GPIO_NUM_MAX)
			bl_gpio_set(bdrv, bconf->en_gpio, bconf->en_gpio_on);
	} else {
		if (bconf->en_gpio < BL_GPIO_NUM_MAX)
			bl_gpio_set(bdrv, bconf->en_gpio, bconf->en_gpio_off);
	}
}

static void bl_power_on(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret;

	if (aml_bl_check_driver(bdrv))
		return;

	mutex_lock(&bl_power_mutex);

	if ((bdrv->state & BL_STATE_LCD_ON) == 0) {
		BLPR("%s exit, for lcd is off\n", __func__);
		goto exit_power_on_bl;
	}
	if ((bdrv->state & BL_STATE_BL_POWER_ON) == 0) {
		BLPR("%s exit, for backlight power off\n", __func__);
		goto exit_power_on_bl;
	}

	if (bdrv->brightness_bypass == 0) {
		if (bdrv->level == 0 || (bdrv->state & BL_STATE_BL_ON))
			goto exit_power_on_bl;
	}

	ret = 0;
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		bl_power_en_ctrl(bdrv, 1);
		break;
	case BL_CTRL_PWM:
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on pwm */
			bl_pwm_ctrl(bconf->bl_pwm, 1);
			bl_pwm_pinmux_set(bdrv, 1);
		} else {
			/* step 1: power on pwm */
			bl_pwm_ctrl(bconf->bl_pwm, 1);
			bl_pwm_pinmux_set(bdrv, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on pwm_combo */
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 1);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 1);
			bl_pwm_pinmux_set(bdrv, 1);
		} else {
			/* step 1: power on pwm_combo */
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 1);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 1);
			bl_pwm_pinmux_set(bdrv, 1);
			if (bconf->pwm_on_delay > 0)
				msleep(bconf->pwm_on_delay);
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (!ldim_drv) {
			BLERR("no ldim driver\n");
			goto exit_power_on_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			/* step 2: power on ldim */
			if (ldim_drv->power_on) {
				ret = ldim_drv->power_on();
				if (ret)
					BLERR("ldim: power on error\n");
			} else {
				BLPR("ldim: power on is null\n");
			}
		} else {
			/* step 1: power on ldim */
			if (ldim_drv->power_on) {
				ret = ldim_drv->power_on();
				if (ret)
					BLERR("ldim: power on error\n");
			} else {
				BLPR("ldim: power on is null\n");
			}
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bext = bl_extern_get_driver(bdrv->index);
		if (!bext) {
			BLERR("[%d]: no bl_extern driver\n", bdrv->index);
			goto exit_power_on_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			/* step 2: power on bl_extern */
			if (bext->power_on) {
				ret = bext->power_on(bext);
				if (ret)
					BLERR("bl_extern: power on error\n");
			}
		} else {
			/* step 1: power on bl_extern */
			if (bext->power_on) {
				ret = bext->power_on(bext);
				if (ret)
					BLERR("bl_extern: power on error\n");
			}
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
#endif
	default:
		BLPR("invalid backlight control method\n");
		goto exit_power_on_bl;
	}
	bdrv->state |= BL_STATE_BL_ON;
	BLPR("backlight power on\n");

exit_power_on_bl:
	mutex_unlock(&bl_power_mutex);
}

static void bl_power_off(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret;

	if (aml_bl_check_driver(bdrv))
		return;
	mutex_lock(&bl_power_mutex);

	if ((bdrv->state & BL_STATE_BL_ON) == 0) {
		goto exit_power_off_bl;
		return;
	}

	ret = 0;
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		bl_power_en_ctrl(bdrv, 0);
		break;
	case BL_CTRL_PWM:
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off pwm */
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm, 0);
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off pwm */
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm, 0);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off pwm_combo */
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 0);
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off pwm_combo */
			if (bconf->pwm_off_delay > 0)
				msleep(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 0);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (!ldim_drv) {
			BLERR("no ldim driver\n");
			goto exit_power_off_bl;
		}
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off ldim */
			if (ldim_drv->power_off) {
				ret = ldim_drv->power_off();
				if (ret)
					BLERR("ldim: power off error\n");
			} else {
				BLERR("ldim: power off is null\n");
			}
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off ldim */
			if (ldim_drv->power_off) {
				ret = ldim_drv->power_off();
				if (ret)
					BLERR("ldim: power off error\n");
			} else {
				BLERR("ldim: power off is null\n");
			}
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bext = bl_extern_get_driver(bdrv->index);
		if (!bext) {
			BLERR("[%d]: no bl_extern driver\n", bdrv->index);
			goto exit_power_off_bl;
		}
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off bl_extern */
			if (bext->power_off) {
				ret = bext->power_off(bext);
				if (ret)
					BLERR("bl_extern: power off error\n");
			}
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off bl_extern */
			if (bext->power_off) {
				ret = bext->power_off(bext);
				if (ret)
					BLERR("bl_extern: power off error\n");
			}
		}
		break;
#endif
	default:
		BLPR("invalid backlight control method\n");
		goto exit_power_off_bl;
		break;
	}
	if (bconf->power_off_delay > 0)
		lcd_delay_ms(bconf->power_off_delay);

	bdrv->state &= ~BL_STATE_BL_ON;
	BLPR("backlight power off\n");

exit_power_off_bl:
	mutex_unlock(&bl_power_mutex);
}

#ifdef CONFIG_AMLOGIC_BL_EXTERN
static void bl_set_level_extern(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	struct bl_extern_driver_s *bext;
	int ret;

	bext = bl_extern_get_driver(bdrv->index);
	if (!bext) {
		BLERR("[%d]: no bl_extern driver\n", bdrv->index);
		return;
	}

	if (bext->set_level) {
		ret = bext->set_level(bext, level);
		if (ret)
			BLERR("bl_ext: set_level error\n");
	} else {
		BLERR("bl_ext: set_level is null\n");
	}
}
#endif

#ifdef CONFIG_AMLOGIC_BL_LDIM
static void bl_set_level_ldim(unsigned int level)
{
	struct aml_ldim_driver_s *ldim_drv;
	int ret = 0;

	ldim_drv = aml_ldim_get_driver();
	if (!ldim_drv) {
		BLERR("no ldim driver\n");
	} else {
		if (ldim_drv->set_level) {
			ret = ldim_drv->set_level(level);
			if (ret)
				BLERR("ldim: set_level error\n");
		} else {
			BLERR("ldim: set_level is null\n");
		}
	}
}
#endif

static void aml_bl_set_level(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	struct bl_pwm_config_s *pwm0, *pwm1;

	if (aml_bl_check_driver(bdrv))
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("bl_set_level=%u, last_level=%u, state=0x%x\n",
		     level, bdrv->level, bdrv->state);
	}

	/* level range check */
	if (level > bdrv->bconf.level_max)
		level = bdrv->bconf.level_max;
	if (level < bdrv->bconf.level_min) {
		if (level < BL_LEVEL_OFF)
			level = 0;
		else
			level = bdrv->bconf.level_min;
	}
	bdrv->level = level;

	if (level == 0)
		return;

	switch (bdrv->bconf.method) {
	case BL_CTRL_GPIO:
		break;
	case BL_CTRL_PWM:
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm, level);
		break;
	case BL_CTRL_PWM_COMBO:
		pwm0 = bdrv->bconf.bl_pwm_combo0;
		pwm1 = bdrv->bconf.bl_pwm_combo1;

		if (level >= pwm0->level_max) {
			bl_pwm_set_level(bdrv, pwm0, pwm0->level_max);
		} else if ((level > pwm0->level_min) &&
			(level < pwm0->level_max)) {
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
				BLPR("pwm0 region, level=%u\n", level);
			bl_pwm_set_level(bdrv, pwm0, level);
		} else {
			bl_pwm_set_level(bdrv, pwm0, pwm0->level_min);
		}

		if (level >= pwm1->level_max) {
			bl_pwm_set_level(bdrv, pwm1, pwm1->level_max);
		} else if ((level > pwm1->level_min) &&
			(level < pwm1->level_max)) {
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
				BLPR("pwm1 region, level=%u,\n", level);
			bl_pwm_set_level(bdrv, pwm1, level);
		} else {
			bl_pwm_set_level(bdrv, pwm1, pwm1->level_min);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bl_set_level_ldim(level);
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_set_level_extern(bdrv, level);
		break;
#endif
	default:
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("invalid backlight control method\n");
		break;
	}
}

static unsigned int aml_bl_get_level_brightness(struct aml_bl_drv_s *bdrv)
{
	if (aml_bl_check_driver(bdrv))
		return 0;

	BLPR("aml bl state: 0x%x\n", bdrv->state);
	return bdrv->level_brightness;
}

static inline unsigned int bl_brightness_level_map(struct aml_bl_drv_s *bdrv,
						   unsigned int brightness)
{
	unsigned int level;

	if (brightness == 0)
		level = 0;
	else if (brightness > bdrv->bconf.level_max)
		level = bdrv->bconf.level_max;
	else if (brightness < bdrv->bconf.level_min)
		level = bdrv->bconf.level_min;
	else
		level = brightness;

	return level;
}

static inline unsigned int bl_gd_level_map(struct aml_bl_drv_s *bdrv,
					   unsigned int gd_level)
{
	unsigned int max, min, val;

	min = bdrv->bconf.level_min;
	max = bdrv->bconf.level_max;
	val = (gd_level * (bdrv->level_brightness - min)) / 4095 + min;

	return val;
}

static unsigned int aml_bl_init_level(struct aml_bl_drv_s *bdrv,
				      unsigned int level)
{
	unsigned int bl_level = level;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
	    ((bdrv->state & BL_STATE_BL_POWER_ON) == 0))
		bl_level = 0;

	if (bl_level == 0) {
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		aml_bl_set_level(bdrv, bl_level);
		if ((bdrv->state & BL_STATE_BL_ON) == 0)
			bl_power_on(bdrv);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: %s: %u, final level: %u, state: 0x%x\n",
		     bdrv->index, __func__, level, bl_level, bdrv->state);
	}

	return 0;
}

static int aml_bl_update_status(struct backlight_device *bd)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)bl_get_data(bd);
	unsigned int level;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return 0;
	}

	mutex_lock(&bl_status_mutex);
	bdrv->level_brightness = bl_brightness_level_map(bdrv,
					bdrv->bldev->props.brightness);

	if (bdrv->level_brightness == 0) {
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		if ((bdrv->state & BL_STATE_GD_EN) == 0) {
			aml_bl_set_level(bdrv, bdrv->level_brightness);
		} else {
			level = bl_gd_level_map(bdrv, bdrv->level_gd);
			aml_bl_set_level(bdrv, level);
		}

		if ((bdrv->state & BL_STATE_BL_ON) == 0)
			bl_power_on(bdrv);
	}
	mutex_unlock(&bl_status_mutex);

	return 0;
}

static int aml_bl_get_brightness(struct backlight_device *bd)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)bl_get_data(bd);

	return aml_bl_get_level_brightness(bdrv);
}

static const struct backlight_ops aml_bl_ops = {
	.get_brightness = aml_bl_get_brightness,
	.update_status  = aml_bl_update_status,
};

static void bl_config_print(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm;

	if (bconf->method == BL_CTRL_MAX) {
		BLPR("[%d]: no backlight exist\n", bdrv->index);
		return;
	}

	BLPR("[%d]: name = %s, method = %s(%d)\n",
	     bdrv->index, bconf->name,
	     bl_method_type_to_str(bconf->method), bconf->method);

	if ((lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) == 0)
		return;

	BLPR("level_default     = %d\n", bconf->level_default);
	BLPR("level_min         = %d\n", bconf->level_min);
	BLPR("level_max         = %d\n", bconf->level_max);
	BLPR("level_mid         = %d\n", bconf->level_mid);
	BLPR("level_mid_mapping = %d\n", bconf->level_mid_mapping);

	BLPR("en_gpio           = %d\n", bconf->en_gpio);
	BLPR("en_gpio_on        = %d\n", bconf->en_gpio_on);
	BLPR("en_gpio_off       = %d\n", bconf->en_gpio_off);
	BLPR("power_on_delay    = %dms\n", bconf->power_on_delay);
	BLPR("power_off_delay   = %dms\n\n", bconf->power_off_delay);

	switch (bconf->method) {
	case BL_CTRL_PWM:
		BLPR("pwm_on_delay        = %dms\n", bconf->pwm_on_delay);
		BLPR("pwm_off_delay       = %dms\n", bconf->pwm_off_delay);
		BLPR("en_sequence_reverse = %d\n", bconf->en_sequence_reverse);
		if (bconf->bl_pwm) {
			bl_pwm = bconf->bl_pwm;
			BLPR("pwm_index     = %d\n", bl_pwm->index);
			BLPR("pwm_port      = %s(0x%x)\n",
			     bl_pwm_num_to_str(bl_pwm->pwm_port),
			     bl_pwm->pwm_port);
			BLPR("pwm_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				BLPR("pwm_freq      = %d x vfreq\n",
				     bl_pwm->pwm_freq);
			} else {
				BLPR("pwm_freq      = %uHz\n",
				     bl_pwm->pwm_freq);
			}
			BLPR("pwm_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_duty_max  = %d\n", bl_pwm->pwm_duty_max);
			BLPR("pwm_duty_min  = %d\n", bl_pwm->pwm_duty_min);
			BLPR("pwm_cnt       = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_max       = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_min       = %u\n", bl_pwm->pwm_min);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		BLPR("pwm_on_delay        = %dms\n", bconf->pwm_on_delay);
		BLPR("pwm_off_delay       = %dms\n", bconf->pwm_off_delay);
		BLPR("en_sequence_reverse = %d\n", bconf->en_sequence_reverse);
		/* pwm_combo_0 */
		if (bconf->bl_pwm_combo0) {
			bl_pwm = bconf->bl_pwm_combo0;
			BLPR("pwm_combo0_index     = %d\n", bl_pwm->index);
			BLPR("pwm_combo0_port      = %s(0x%x)\n",
			     bl_pwm_num_to_str(bl_pwm->pwm_port),
			     bl_pwm->pwm_port);
			BLPR("pwm_combo0_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				BLPR("pwm_combo0_freq      = %d x vfreq\n",
				     bl_pwm->pwm_freq);
			} else {
				BLPR("pwm_combo0_freq      = %uHz\n",
				     bl_pwm->pwm_freq);
			}
			BLPR("pwm_combo0_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_combo0_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_combo0_duty_max  = %d\n",
			     bl_pwm->pwm_duty_max);
			BLPR("pwm_combo0_duty_min  = %d\n",
			     bl_pwm->pwm_duty_min);
			BLPR("pwm_combo0_pwm_cnt   = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_combo0_pwm_max   = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_combo0_pwm_min   = %u\n", bl_pwm->pwm_min);
		}
		/* pwm_combo_1 */
		if (bconf->bl_pwm_combo1) {
			bl_pwm = bconf->bl_pwm_combo1;
			BLPR("pwm_combo1_index     = %d\n", bl_pwm->index);
			BLPR("pwm_combo1_port      = %s(0x%x)\n",
			     bl_pwm_num_to_str(bl_pwm->pwm_port),
			     bl_pwm->pwm_port);
			BLPR("pwm_combo1_method    = %d\n", bl_pwm->pwm_method);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				BLPR("pwm_combo1_freq      = %d x vfreq\n",
				     bl_pwm->pwm_freq);
			} else {
				BLPR("pwm_combo1_freq      = %uHz\n",
				     bl_pwm->pwm_freq);
			}
			BLPR("pwm_combo1_level_max = %u\n", bl_pwm->level_max);
			BLPR("pwm_combo1_level_min = %u\n", bl_pwm->level_min);
			BLPR("pwm_combo1_duty_max  = %d\n",
			     bl_pwm->pwm_duty_max);
			BLPR("pwm_combo1_duty_min  = %d\n",
			     bl_pwm->pwm_duty_min);
			BLPR("pwm_combo1_pwm_cnt   = %u\n", bl_pwm->pwm_cnt);
			BLPR("pwm_combo1_pwm_max   = %u\n", bl_pwm->pwm_max);
			BLPR("pwm_combo1_pwm_min   = %u\n", bl_pwm->pwm_min);
		}
		break;
	default:
		break;
	}
}

#ifdef CONFIG_OF
static int bl_config_load_from_dts(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	const char *str;
	unsigned int para[10];
	char bl_propname[20];
	struct device_node *child;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	int val;
	int ret = 0;

	/* select backlight by index */
	bconf->index = bl_index_lut[bdrv->index];
	if (bconf->index == 0xff) {
		bconf->method = BL_CTRL_MAX;
		return -1;
	}
	sprintf(bl_propname, "backlight_%d", bconf->index);
	BLPR("[%d]: load: %s\n", bdrv->index, bl_propname);
	child = of_get_child_by_name(bdrv->dev->of_node, bl_propname);
	if (!child) {
		BLERR("[%d]: failed to get %s\n", bdrv->index, bl_propname);
		return -1;
	}

	ret = of_property_read_string(child, "bl_name", &str);
	if (ret) {
		BLERR("[%d]: failed to get bl_name\n", bdrv->index);
		str = "backlight";
	}
	strncpy(bconf->name, str, (BL_NAME_MAX - 1));

	ret = of_property_read_u32_array(child, "bl_level_default_uboot_kernel", &para[0], 2);
	if (ret) {
		BLERR("[%d]: failed to get bl_level_default_uboot_kernel\n", bdrv->index);
		bconf->level_uboot = BL_LEVEL_DEFAULT;
		bconf->level_default = BL_LEVEL_DEFAULT;
	} else {
		bconf->level_uboot = para[0] & BL_LEVEL_MASK;
		bconf->level_default = para[1] & BL_LEVEL_MASK;

		bdrv->brightness_bypass = ((para[1] >> BL_POLICY_BRIGHTNESS_BYPASS_BIT) &
					   BL_POLICY_BRIGHTNESS_BYPASS_MASK);
		if (bdrv->brightness_bypass) {
			BLPR("[%d]: 0x%x: enable brightness_bypass\n",
			     bdrv->index, para[1]);
		}
		bdrv->step_on_flag = ((para[1] >> BL_POLICY_POWER_ON_BIT) &
				      BL_POLICY_POWER_ON_MASK);
		if (bdrv->step_on_flag) {
			BLPR("[%d]: 0x%x: bl_step_on_flag: %d\n",
			     bdrv->index, para[1], bdrv->step_on_flag);
		}
	}

	ret = of_property_read_u32_array(child, "bl_level_attr", &para[0], 4);
	if (ret) {
		BLERR("[%d]: failed to get bl_level_attr\n", bdrv->index);
		bconf->level_min = BL_LEVEL_MIN;
		bconf->level_max = BL_LEVEL_MAX;
		bconf->level_mid = BL_LEVEL_MID;
		bconf->level_mid_mapping = BL_LEVEL_MID_MAPPED;
	} else {
		bconf->level_max = para[0];
		bconf->level_min = para[1];
		bconf->level_mid = para[2];
		bconf->level_mid_mapping = para[3];
	}

	ret = of_property_read_u32(child, "bl_ctrl_method", &val);
	if (ret) {
		BLERR("[%d]: failed to get bl_ctrl_method\n", bdrv->index);
		bconf->method = BL_CTRL_MAX;
	} else {
		bconf->method = (val >= BL_CTRL_MAX) ? BL_CTRL_MAX : val;
	}
	ret = of_property_read_u32_array(child, "bl_power_attr", &para[0], 5);
	if (ret) {
		BLERR("[%d]: failed to get bl_power_attr\n", bdrv->index);
		bconf->en_gpio = BL_GPIO_MAX;
		bconf->en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		bconf->en_gpio_off = BL_GPIO_OUTPUT_LOW;
		bconf->power_on_delay = 100;
		bconf->power_off_delay = 30;
	} else {
		if (para[0] >= BL_GPIO_NUM_MAX) {
			bconf->en_gpio = BL_GPIO_MAX;
		} else {
			bconf->en_gpio = para[0];
			bl_gpio_probe(bdrv, bconf->en_gpio);
		}
		bconf->en_gpio_on = para[1];
		bconf->en_gpio_off = para[2];
		bconf->power_on_delay = para[3];
		bconf->power_off_delay = para[4];
	}
	ret = of_property_read_u32(child, "en_sequence_reverse", &val);
	if (ret) {
		bconf->en_sequence_reverse = 0;
	} else {
		bconf->en_sequence_reverse = val;
		BLPR("[%d]: find en_sequence_reverse: %d\n", bdrv->index, val);
	}

	ret = of_property_read_u32(child, "bl_ldim_region_row_col", &para[0]);
	if (ret) {
		ret = of_property_read_u32(child, "bl_ldim_zone_row_col", &para[0]);
		if (ret == 0)
			bconf->ldim_flag = 1;
	} else {
		bconf->ldim_flag = 1;
	}

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm)
			return -1;

		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		ret = of_property_read_string(child, "bl_pwm_port", &str);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_port\n", bdrv->index);
			bl_pwm->pwm_port = BL_PWM_MAX;
		} else {
			bl_pwm->pwm_port = bl_pwm_str_to_num(str);
			BLPR("[%d]: bl pwm_port: %s(0x%x)\n",
			     bdrv->index, str, bl_pwm->pwm_port);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_attr", &para[0], 4);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_attr\n", bdrv->index);
			bl_pwm->pwm_method = BL_PWM_POSITIVE;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				bl_pwm->pwm_freq = BL_FREQ_DEFAULT;
			bl_pwm->pwm_duty_max = 80;
			bl_pwm->pwm_duty_min = 20;
		} else {
			bl_pwm->pwm_method = para[0];
			bl_pwm->pwm_freq = para[1];
			bl_pwm->pwm_duty_max = para[2];
			bl_pwm->pwm_duty_min = para[3];
		}
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 4) {
				BLERR("[%d]: bl_pwm_vs wrong freq %d\n",
				      bdrv->index, bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_power", &para[0], 4);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_power\n",
			      bdrv->index);
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = para[2];
			bconf->pwm_off_delay = para[3];
		}

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		/* init pwm config */
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0)
			return -1;
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1)
			return -1;

		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;

		pwm_combo0->index = 0;
		pwm_combo1->index = 1;

		ret = of_property_read_string_index(child, "bl_pwm_combo_port", 0, &str);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_port\n", bdrv->index);
			pwm_combo0->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo0->pwm_port = bl_pwm_str_to_num(str);
		}
		ret = of_property_read_string_index(child, "bl_pwm_combo_port", 1, &str);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_port\n", bdrv->index);
			pwm_combo1->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo1->pwm_port = bl_pwm_str_to_num(str);
		}
		BLPR("[%d]: pwm_combo_port: %s(0x%x), %s(0x%x)\n",
		     bdrv->index,
		     bl_pwm_num_to_str(pwm_combo0->pwm_port),
		     pwm_combo0->pwm_port,
		     bl_pwm_num_to_str(pwm_combo1->pwm_port),
		     pwm_combo1->pwm_port);
		ret = of_property_read_u32_array(child, "bl_pwm_combo_level_mapping",
						 &para[0], 4);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_level_mapping\n",
			      bdrv->index);
			pwm_combo0->level_max = BL_LEVEL_MAX;
			pwm_combo0->level_min = BL_LEVEL_MID;
			pwm_combo1->level_max = BL_LEVEL_MID;
			pwm_combo1->level_min = BL_LEVEL_MIN;
		} else {
			pwm_combo0->level_max = para[0];
			pwm_combo0->level_min = para[1];
			pwm_combo1->level_max = para[2];
			pwm_combo1->level_min = para[3];
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_attr", &para[0], 8);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_attr\n", bdrv->index);
			pwm_combo0->pwm_method = BL_PWM_POSITIVE;
			if (pwm_combo0->pwm_port == BL_PWM_VS)
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo0->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo0->pwm_duty_max = 80;
			pwm_combo0->pwm_duty_min = 20;
			pwm_combo1->pwm_method = BL_PWM_NEGATIVE;
			if (pwm_combo1->pwm_port == BL_PWM_VS)
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo1->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo1->pwm_duty_max = 80;
			pwm_combo1->pwm_duty_min = 20;
		} else {
			pwm_combo0->pwm_method = para[0];
			pwm_combo0->pwm_freq = para[1];
			pwm_combo0->pwm_duty_max = para[2];
			pwm_combo0->pwm_duty_min = para[3];
			pwm_combo1->pwm_method = para[4];
			pwm_combo1->pwm_freq = para[5];
			pwm_combo1->pwm_duty_max = para[6];
			pwm_combo1->pwm_duty_min = para[7];
		}
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			if (pwm_combo0->pwm_freq > 4) {
				BLERR("[%d]: bl_pwm_0_vs wrong freq %d\n",
				      bdrv->index, pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo0->pwm_freq < 50)
				pwm_combo0->pwm_freq = 50;
		}
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			if (pwm_combo1->pwm_freq > 4) {
				BLERR("[%d]: bl_pwm_1_vs wrong freq %d\n",
				      bdrv->index, pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo1->pwm_freq < 50)
				pwm_combo1->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_power", &para[0], 6);
		if (ret) {
			BLERR("[%d]: failed to get bl_pwm_combo_power\n", bdrv->index);
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = para[4];
			bconf->pwm_off_delay = para[5];
		}

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		/* init pwm config */
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		/* get bl_extern_index from dts */
		ret = of_property_read_u32(child, "bl_extern_index", &para[0]);
		if (ret) {
			BLERR("[%d]: failed to get bl_extern_index\n", bdrv->index);
		} else {
			bconf->bl_extern_index = para[0];
			bl_extern_dev_index_add(bdrv->index, para[0]);
			BLPR("[%d]: get bl_extern_index = %d\n",
			     bdrv->index, bconf->bl_extern_index);
		}
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_dts(child);
		if (ret < 0)
			return -1;
	}
#endif

	return 0;
}
#endif

static int bl_config_load_from_unifykey(struct aml_bl_drv_s *bdrv, char *key_name)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	unsigned char *para;
	int key_len, len;
	unsigned char *p;
	const char *str;
	unsigned char temp;
	struct aml_lcd_unifykey_header_s bl_header;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	unsigned int level;
	int ret;

	if (!key_name)
		return -1;

	key_len = LCD_UKEY_BL_SIZE;
	para = kcalloc(key_len, (sizeof(unsigned char)), GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(key_name, para, &key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	len = LCD_UKEY_HEAD_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		BLERR("[%d]: unifykey header length is incorrect\n", bdrv->index);
		kfree(para);
		return -1;
	}

	lcd_unifykey_header_check(para, &bl_header);
	BLPR("[%d]: unifykey version: 0x%04x\n", bdrv->index, bl_header.version);
	switch (bl_header.version) {
	case 2:
		len = 10 + 30 + 12 + 8 + 32 + 10;
		break;
	default:
		len = 10 + 30 + 12 + 8 + 32;
		break;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("unifykey header:\n");
		BLPR("crc32             = 0x%08x\n", bl_header.crc32);
		BLPR("data_len          = %d\n", bl_header.data_len);
	}

	/* step 2: check backlight parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		BLERR("[%d]: unifykey length is incorrect\n", bdrv->index);
		kfree(para);
		return -1;
	}

	/* basic: 30byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strncpy(bconf->name, str, (BL_NAME_MAX - 1));
	bconf->index = 0;

	/* level: 6byte */
	bconf->level_uboot = (*(p + LCD_UKEY_BL_LEVEL_UBOOT) |
		((*(p + LCD_UKEY_BL_LEVEL_UBOOT + 1)) << 8));
	level = (*(p + LCD_UKEY_BL_LEVEL_KERNEL) |
		 ((*(p + LCD_UKEY_BL_LEVEL_KERNEL + 1)) << 8));
	bconf->level_max = (*(p + LCD_UKEY_BL_LEVEL_MAX) |
		((*(p + LCD_UKEY_BL_LEVEL_MAX + 1)) << 8));
	bconf->level_min = (*(p + LCD_UKEY_BL_LEVEL_MIN) |
		((*(p  + LCD_UKEY_BL_LEVEL_MIN + 1)) << 8));
	bconf->level_mid = (*(p + LCD_UKEY_BL_LEVEL_MID) |
		((*(p + LCD_UKEY_BL_LEVEL_MID + 1)) << 8));
	bconf->level_mid_mapping = (*(p + LCD_UKEY_BL_LEVEL_MID_MAP) |
		((*(p + LCD_UKEY_BL_LEVEL_MID_MAP + 1)) << 8));

	bconf->level_default = level & BL_LEVEL_MASK;
	bdrv->brightness_bypass =
		((level >> BL_POLICY_BRIGHTNESS_BYPASS_BIT) &
		 BL_POLICY_BRIGHTNESS_BYPASS_MASK);
	if (bdrv->brightness_bypass) {
		BLPR("[%d]: 0x%x: enable brightness_bypass\n",
		     bdrv->index, level);
	}
	bdrv->step_on_flag = ((level >> BL_POLICY_POWER_ON_BIT) &
			      BL_POLICY_POWER_ON_MASK);
	if (bdrv->step_on_flag) {
		BLPR("[%d]: 0x%x: bl_step_on_flag: %d\n",
		     bdrv->index, level, bdrv->step_on_flag);
	}

	/* method: 8byte */
	temp = *(p + LCD_UKEY_BL_METHOD);
	bconf->method = (temp >= BL_CTRL_MAX) ? BL_CTRL_MAX : temp;

	if (*(p + LCD_UKEY_BL_EN_GPIO) >= BL_GPIO_NUM_MAX) {
		bconf->en_gpio = BL_GPIO_MAX;
	} else {
		bconf->en_gpio = *(p + LCD_UKEY_BL_EN_GPIO);
		bl_gpio_probe(bdrv, bconf->en_gpio);
	}
	bconf->en_gpio_on = *(p + LCD_UKEY_BL_EN_GPIO_ON);
	bconf->en_gpio_off = *(p + LCD_UKEY_BL_EN_GPIO_OFF);
	bconf->power_on_delay = (*(p + LCD_UKEY_BL_ON_DELAY) |
		((*(p + LCD_UKEY_BL_ON_DELAY + 1)) << 8));
	bconf->power_off_delay = (*(p + LCD_UKEY_BL_OFF_DELAY) |
		((*(p + LCD_UKEY_BL_OFF_DELAY + 1)) << 8));

	if (bl_header.version == 2) {
		bconf->en_sequence_reverse = (*(p + LCD_UKEY_BL_CUST_VAL_0) |
				((*(p + LCD_UKEY_BL_CUST_VAL_0 + 1)) << 8));
	} else {
		bconf->en_sequence_reverse = 0;
	}

	/* pwm: 24byte */
	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm) {
			kfree(para);
			return -1;
		}
		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		bconf->pwm_on_delay = (*(p + LCD_UKEY_BL_PWM_ON_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_ON_DELAY + 1)) << 8));
		bconf->pwm_off_delay = (*(p + LCD_UKEY_BL_PWM_OFF_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_OFF_DELAY + 1)) << 8));
		bl_pwm->pwm_method =  *(p + LCD_UKEY_BL_PWM_METHOD);
		bl_pwm->pwm_port = *(p + LCD_UKEY_BL_PWM_PORT);
		bl_pwm->pwm_freq = (*(p + LCD_UKEY_BL_PWM_FREQ) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 3)) << 8));
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 4) {
				BLERR("bl_pwm_vs wrong freq %d\n", bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		bl_pwm->pwm_duty_max = *(p + LCD_UKEY_BL_PWM_DUTY_MAX);
		bl_pwm->pwm_duty_min = *(p + LCD_UKEY_BL_PWM_DUTY_MIN);

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0) {
			kfree(para);
			return -1;
		}
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1) {
			kfree(para);
			return -1;
		}
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		pwm_combo0->index = 0;
		pwm_combo1->index = 1;

		bconf->pwm_on_delay = (*(p + LCD_UKEY_BL_PWM_ON_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_ON_DELAY + 1)) << 8));
		bconf->pwm_off_delay = (*(p + LCD_UKEY_BL_PWM_OFF_DELAY) |
			((*(p + LCD_UKEY_BL_PWM_OFF_DELAY + 1)) << 8));

		pwm_combo0->pwm_method = *(p + LCD_UKEY_BL_PWM_METHOD);
		pwm_combo0->pwm_port = *(p + LCD_UKEY_BL_PWM_PORT);
		pwm_combo0->pwm_freq = (*(p + LCD_UKEY_BL_PWM_FREQ) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM_FREQ + 3)) << 8));
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			if (pwm_combo0->pwm_freq > 4) {
				BLERR("bl_pwm_0_vs wrong freq %d\n", pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		pwm_combo0->pwm_duty_max = *(p + LCD_UKEY_BL_PWM_DUTY_MAX);
		pwm_combo0->pwm_duty_min = *(p + LCD_UKEY_BL_PWM_DUTY_MIN);

		pwm_combo1->pwm_method = *(p + LCD_UKEY_BL_PWM2_METHOD);
		pwm_combo1->pwm_port = *(p + LCD_UKEY_BL_PWM2_PORT);
		pwm_combo1->pwm_freq = (*(p + LCD_UKEY_BL_PWM2_FREQ) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 1)) << 8) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 2)) << 8) |
			((*(p + LCD_UKEY_BL_PWM2_FREQ + 3)) << 8));
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			if (pwm_combo1->pwm_freq > 4) {
				BLERR("bl_pwm_1_vs wrong freq %d\n", pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		pwm_combo1->pwm_duty_max = *(p + LCD_UKEY_BL_PWM2_DUTY_MAX);
		pwm_combo1->pwm_duty_min = *(p + LCD_UKEY_BL_PWM2_DUTY_MIN);

		pwm_combo0->level_max = (*(p + LCD_UKEY_BL_PWM_LEVEL_MAX) |
			((*(p + LCD_UKEY_BL_PWM_LEVEL_MAX + 1)) << 8));
		pwm_combo0->level_min = (*(p + LCD_UKEY_BL_PWM_LEVEL_MIN) |
			((*(p + LCD_UKEY_BL_PWM_LEVEL_MIN + 1)) << 8));
		pwm_combo1->level_max = (*(p + LCD_UKEY_BL_PWM2_LEVEL_MAX) |
			((*(p + LCD_UKEY_BL_PWM2_LEVEL_MAX + 1)) << 8));
		pwm_combo1->level_min = (*(p + LCD_UKEY_BL_PWM2_LEVEL_MIN) |
			((*(p + LCD_UKEY_BL_PWM2_LEVEL_MIN + 1)) << 8));

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_min;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_min;
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_unifykey(para);
		if (ret < 0) {
			kfree(para);
			return -1;
		}
	}
#endif

	kfree(para);
	return 0;
}

static int bl_config_load(struct aml_bl_drv_s *bdrv, struct platform_device *pdev)
{
	unsigned int temp;
	char key_name[15];
	int load_id = 0, i;
	bool is_init;
	phandle pwm_phandle;
	int ret = 0;

	if (!bdrv->dev->of_node) {
		BLERR("no backlight[%d] of_node exist\n", bdrv->index);
		return -1;
	}

	ret = of_property_read_u32(bdrv->dev->of_node, "key_valid", &temp);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("[%d]: failed to get key_valid\n", bdrv->index);
		temp = 0;
	}
	bdrv->key_valid = temp;
	BLPR("[%d]: key_valid: %d\n", bdrv->index, bdrv->key_valid);

	if (bdrv->key_valid) {
		if (bdrv->index == 0)
			sprintf(key_name, "backlight");
		else
			sprintf(key_name, "backlight%d", bdrv->index);

		is_init = lcd_unifykey_init_get();
		i = 0;
		while (!is_init) {
			if (i++ >= LCD_UNIFYKEY_WAIT_TIMEOUT)
				break;
			lcd_delay_ms(LCD_UNIFYKEY_RETRY_INTERVAL);
			is_init = lcd_unifykey_init_get();
		}
		if (is_init) {
			ret = lcd_unifykey_check(key_name);
			if (ret < 0)
				load_id = 0;
			else
				load_id = 1;
		} else {
			load_id = 0;
			BLERR("[%d]: key_init_flag=%d\n", bdrv->index, is_init);
		}
	}
	if (load_id) {
		BLPR("[%d]: %s from unifykey\n", bdrv->index, __func__);
		bdrv->config_load = 1;
		ret = bl_config_load_from_unifykey(bdrv, key_name);
	} else {
#ifdef CONFIG_OF
		BLPR("[%d]: %s from dts\n", bdrv->index, __func__);
		bdrv->config_load = 0;
		ret = bl_config_load_from_dts(bdrv);
#endif
	}
	if (ret)
		return -1;

	if (bl_level[bdrv->index])
		bdrv->bconf.level_uboot = bl_level[bdrv->index];
	bdrv->level_init_on = (bdrv->bconf.level_uboot > 0) ?
				bdrv->bconf.level_uboot : BL_LEVEL_DEFAULT;

	bl_config_print(bdrv);

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bdrv->bconf.ldim_flag)
		aml_ldim_probe(pdev);
#endif

	bdrv->res_vsync_irq[0] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	if (!bdrv->res_vsync_irq[0]) {
		ret = -ENODEV;
		BLERR("[%d]: bl_vsync_irq[0] resource error\n", bdrv->index);
		return -1;
	}

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm->pwm_port >= BL_PWM_VS)
			break;
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR("%s: not match bl_pwm_config node\n", __func__);
			return -1;
		}
		ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle, bdrv->bconf.bl_pwm);
		if (ret)
			return -1;
		break;
	case BL_CTRL_PWM_COMBO:
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR("%s: not match bl_pwm_config node\n", __func__);
			return -1;
		}
		if (bdrv->bconf.bl_pwm_combo0->pwm_port < BL_PWM_VS) {
			ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
						      bdrv->bconf.bl_pwm_combo0);
			if (ret)
				return -1;
		}
		if (bdrv->bconf.bl_pwm_combo1->pwm_port < BL_PWM_VS) {
			ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
						      bdrv->bconf.bl_pwm_combo1);
			if (ret)
				return -1;
		}
		break;
	default:
		break;
	}

	return 0;
}

/* lcd notify */
static void bl_on_function(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;

	mutex_lock(&bl_level_mutex);

	/* lcd power on backlight flag */
	bdrv->state |= BL_STATE_LCD_ON;
	BLPR("%s: bl_step_on_flag=%d, bl_level=%u, state=0x%x\n",
	     __func__, bdrv->step_on_flag, bdrv->level, bdrv->state);

	bdrv->level_brightness = bl_brightness_level_map(bdrv,
					bdrv->bldev->props.brightness);

	bdrv->state |= BL_STATE_BL_INIT_ON;
	switch (bdrv->step_on_flag) {
	case 1:
		BLPR("bl_step_on level: %d\n", bconf->level_default);
		aml_bl_init_level(bdrv, bconf->level_default);
		msleep(120);
		if (bdrv->brightness_bypass) {
			switch (bconf->method) {
			case BL_CTRL_PWM:
				bconf->bl_pwm->pwm_duty =
					bconf->bl_pwm->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm);
				break;
			case BL_CTRL_PWM_COMBO:
				bconf->bl_pwm_combo0->pwm_duty =
					bconf->bl_pwm_combo0->pwm_duty_save;
				bconf->bl_pwm_combo1->pwm_duty =
					bconf->bl_pwm_combo1->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo0);
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo1);
				break;
			default:
				break;
			}
		} else {
			BLPR("bl_on level: %d\n", bdrv->level_brightness);
			aml_bl_init_level(bdrv, bdrv->level_brightness);
		}
		break;
	case 2:
		BLPR("bl_step_on level: %d\n", bconf->level_uboot);
		aml_bl_init_level(bdrv, bconf->level_uboot);
		msleep(120);
		if (bdrv->brightness_bypass) {
			switch (bconf->method) {
			case BL_CTRL_PWM:
				bconf->bl_pwm->pwm_duty =
					bconf->bl_pwm->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm);
				break;
			case BL_CTRL_PWM_COMBO:
				bconf->bl_pwm_combo0->pwm_duty =
					bconf->bl_pwm_combo0->pwm_duty_save;
				bconf->bl_pwm_combo1->pwm_duty =
					bconf->bl_pwm_combo1->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo0);
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo1);
				break;
			default:
				break;
			}
		} else {
			BLPR("bl_on level: %d\n", bdrv->level_brightness);
			aml_bl_init_level(bdrv, bdrv->level_brightness);
		}
		break;
	default:
		if (bdrv->brightness_bypass) {
			if ((bdrv->state & BL_STATE_BL_ON) == 0)
				bl_power_on(bdrv);
		} else {
			aml_bl_init_level(bdrv, bdrv->level_brightness);
		}
		break;
	}
	bdrv->state &= ~(BL_STATE_BL_INIT_ON);

	mutex_unlock(&bl_level_mutex);
}

static void bl_delayd_on(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_bl_drv_s *bdrv;

	d_work = container_of(p_work, struct delayed_work, work);
	bdrv = container_of(d_work, struct aml_bl_drv_s, delayed_on_work);

	if (bdrv->probe_done == 0)
		return;
	if (bdrv->on_request == 0)
		return;

	bl_on_function(bdrv);
}

static int bl_lcd_on_notifier(struct notifier_block *nb,
			      unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct aml_bl_drv_s *bdrv;

	if ((event & LCD_EVENT_BL_ON) == 0)
		return NOTIFY_DONE;

	if (!pdrv)
		return NOTIFY_DONE;
	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: 0x%lx\n", bdrv->index, __func__, event);

	bdrv->on_request = 1;
	/* lcd power on sequence control */
	if (bdrv->bconf.method < BL_CTRL_MAX) {
#ifdef BL_POWER_ON_DELAY_WORK
		lcd_queue_delayed_on_work(&bdrv->delayed_on_work,
					  bdrv->bconf.power_on_delay);
#else
		lcd_delay_ms(bdrv->bconf.power_on_delay);
		bl_on_function(bdrv);
#endif
	} else {
		BLERR("[%d]: wrong backlight control method\n", bdrv->index);
	}

	return NOTIFY_OK;
}

static int bl_lcd_off_notifier(struct notifier_block *nb,
			       unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct aml_bl_drv_s *bdrv;

	if ((event & LCD_EVENT_BL_OFF) == 0)
		return NOTIFY_DONE;

	if (!pdrv)
		return NOTIFY_DONE;
	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: 0x%lx\n", bdrv->index, __func__, event);

	bdrv->on_request = 0;
	bdrv->state &= ~BL_STATE_LCD_ON;
	mutex_lock(&bl_level_mutex);
	bdrv->state |= BL_STATE_BL_INIT_ON;
	if (bdrv->state & BL_STATE_BL_ON)
		bl_power_off(bdrv);
	bdrv->state &= ~BL_STATE_BL_INIT_ON;
	mutex_unlock(&bl_level_mutex);

	return NOTIFY_OK;
}

static struct notifier_block bl_lcd_on_nb = {
	.notifier_call = bl_lcd_on_notifier,
	.priority = LCD_PRIORITY_POWER_BL_ON,
};

static struct notifier_block bl_lcd_off_nb = {
	.notifier_call = bl_lcd_off_notifier,
	.priority = LCD_PRIORITY_POWER_BL_OFF,
};

static inline int bl_pwm_vs_lcd_update(struct aml_bl_drv_s *bdrv,
				       struct bl_pwm_config_s *bl_pwm)
{
	unsigned int cnt;

	if (!bl_pwm) {
		BLERR("[%d]: %s: bl_pwm is null\n", bdrv->index, __func__);
		return 0;
	}

	cnt = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
	if (cnt == bl_pwm->pwm_cnt)
		return 0;

	mutex_lock(&bl_level_mutex);
	bl_pwm_config_init(bl_pwm);

	if (bdrv->state & BL_STATE_GD_EN) {
		mutex_unlock(&bl_level_mutex);
		return 0;
	}

	if (bdrv->brightness_bypass)
		bl_pwm_set_duty(bdrv, bl_pwm);
	else
		aml_bl_set_level(bdrv, bdrv->bldev->props.brightness);
	mutex_unlock(&bl_level_mutex);

	return 0;
}

static int bl_lcd_update_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct aml_bl_drv_s *bdrv;
	struct bl_metrics_config_s *bl_metrics_conf;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned int frame_rate;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_UPDATE)
		return NOTIFY_DONE;

	if (!pdrv)
		return NOTIFY_DONE;
	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	bl_metrics_conf = &bdrv->bl_metrics_conf;
	frame_rate = pdrv->config.timing.frame_rate;

	bl_metrics_conf->frame_rate = frame_rate;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s for pwm_vs\n", bdrv->index, __func__);
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm->pwm_port == BL_PWM_VS) {
			bl_pwm = bdrv->bconf.bl_pwm;
			if (bl_pwm)
				bl_pwm_vs_lcd_update(bdrv, bl_pwm);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bdrv->bconf.bl_pwm_combo0->pwm_port == BL_PWM_VS)
			bl_pwm = bdrv->bconf.bl_pwm_combo0;
		else if (bdrv->bconf.bl_pwm_combo1->pwm_port == BL_PWM_VS)
			bl_pwm = bdrv->bconf.bl_pwm_combo1;
		if (bl_pwm)
			bl_pwm_vs_lcd_update(bdrv, bl_pwm);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv->vsync_change_flag = (unsigned char)(frame_rate);
		if (ldim_drv->pwm_vs_update)
			ldim_drv->pwm_vs_update();
		break;
#endif

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bl_lcd_update_nb = {
	.notifier_call = bl_lcd_update_notifier,
};

static int bl_lcd_test_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct aml_bl_drv_s *bdrv;
	int flag;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_TEST_PATTERN)
		return NOTIFY_DONE;

	if (!pdrv)
		return NOTIFY_DONE;
	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s for lcd test_pattern\n", bdrv->index, __func__);

	flag = (pdrv->test_state > 0) ? 1 : 0;
	switch (bdrv->bconf.method) {
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		if (ldim_drv->test_ctrl)
			ldim_drv->test_ctrl(flag);
		break;
#endif
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bl_lcd_test_nb = {
	.notifier_call = bl_lcd_test_notifier,
};

static int bl_gd_diming_func(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	unsigned int level_new;

	if (bdrv->brightness_bypass)
		return 0;
	if ((bdrv->state & BL_STATE_GD_EN) == 0)
		return 0;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
	    (bdrv->state & BL_STATE_BL_INIT_ON) ||
	    ((bdrv->state & BL_STATE_BL_POWER_ON) == 0) ||
	    ((bdrv->state & BL_STATE_BL_ON) == 0))
		return 0;

	/* atomic notifier, can't schedule or sleep */
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: level_gd: %d\n", bdrv->index, __func__, level);

	bdrv->level_gd = (level < 10) ? 10 : ((level > 4095) ? 4095 : level);
	level_new = bl_gd_level_map(bdrv, bdrv->level_gd);
	aml_bl_set_level(bdrv, level_new);

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: %s: %u, final level: %u, state: 0x%x\n",
		     bdrv->index, __func__, level,
		     level_new, bdrv->state);
	}

	return 0;
}

static int bl_gd_dimming_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int level;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_GD_DIM)
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return NOTIFY_DONE;
	}
	if (!data)
		return NOTIFY_DONE;

	/* atomic notifier, can't schedule or sleep */
	level = *(unsigned int *)data;
	bl_gd_diming_func(bdrv, level);

	return NOTIFY_OK;
}

static struct notifier_block bl_gd_dimming_nb = {
	.notifier_call = bl_gd_dimming_notifier,
};

static int bl_gd_sel_func(struct aml_bl_drv_s *bdrv, unsigned int sel)
{
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	if (bdrv->brightness_bypass)
		return 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: gd_sel: %d\n", bdrv->index, __func__, sel);

	if (sel) {
#ifdef CONFIG_AMLOGIC_BL_LDIM
		if (bdrv->bconf.method == BL_CTRL_LOCAL_DIMMING) {
			if (ldim_drv->ld_sel_ctrl)
				ldim_drv->ld_sel_ctrl(0);
		}
#endif
		bdrv->state |= BL_STATE_GD_EN;
	} else {
		bdrv->state &= ~BL_STATE_GD_EN;
#ifdef CONFIG_AMLOGIC_BL_LDIM
		if (bdrv->bconf.method == BL_CTRL_LOCAL_DIMMING) {
			if (ldim_drv->ld_sel_ctrl)
				ldim_drv->ld_sel_ctrl(1);
		}
#endif
	}

	return 0;
}

static int bl_gd_sel_notifier(struct notifier_block *nb,
			      unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	unsigned int sel;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_GD_SEL)
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return NOTIFY_DONE;
	}
	if (!data)
		return NOTIFY_DONE;

	sel = *(unsigned int *)data;
	bl_gd_sel_func(bdrv, sel);

	return NOTIFY_OK;
}

static struct notifier_block bl_gd_sel_nb = {
	.notifier_call = bl_gd_sel_notifier,
};

static int bl_brightness_dimming_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int level;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_BRTNESS_DIM)
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
		(bdrv->state & BL_STATE_BL_INIT_ON) ||
		((bdrv->state & BL_STATE_BL_POWER_ON) == 0) ||
		((bdrv->state & BL_STATE_BL_ON) == 0))
		return NOTIFY_DONE;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return NOTIFY_DONE;
	}
	if (!data)
		return NOTIFY_DONE;

	if (*(unsigned int *)data == 0)
		return NOTIFY_OK;

	/* atomic notifier, can't schedule or sleep */
	bdrv->bldev->props.brightness = *(unsigned int *)data;
	bdrv->level_brightness = bl_brightness_level_map(bdrv,
					bdrv->bldev->props.brightness);

	if ((bdrv->state & BL_STATE_GD_EN) == 0) {
		aml_bl_set_level(bdrv, bdrv->level_brightness);
	} else {
		level = bl_gd_level_map(bdrv, bdrv->level_gd);
		aml_bl_set_level(bdrv, level);
	}

	return NOTIFY_OK;
}

static struct notifier_block bl_bri_dimming_nb = {
	.notifier_call = bl_brightness_dimming_notifier,
};

static void bl_notifier_init(void)
{
	int ret;

	ret = aml_lcd_notifier_register(&bl_lcd_on_nb);
	if (ret)
		BLERR("register bl_lcd_on_nb failed\n");
	ret = aml_lcd_notifier_register(&bl_lcd_off_nb);
	if (ret)
		BLERR("register bl_lcd_off_nb failed\n");
	ret = aml_lcd_notifier_register(&bl_lcd_update_nb);
	if (ret)
		BLERR("register bl_lcd_update_nb failed\n");
	ret = aml_lcd_notifier_register(&bl_lcd_test_nb);
	if (ret)
		BLERR("register bl_lcd_test_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_gd_dimming_nb);
	if (ret)
		BLERR("register bl_gd_dimming_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_gd_sel_nb);
	if (ret)
		BLERR("register bl_gd_sel_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_bri_dimming_nb);
	if (ret)
		BLERR("register bl_bri_dimming_nb failed\n");
}

static void bl_notifier_remove(void)
{
	aml_lcd_atomic_notifier_unregister(&bl_gd_sel_nb);
	aml_lcd_atomic_notifier_unregister(&bl_gd_dimming_nb);
	aml_lcd_atomic_notifier_unregister(&bl_bri_dimming_nb);
	aml_lcd_notifier_unregister(&bl_lcd_test_nb);
	aml_lcd_notifier_unregister(&bl_lcd_update_nb);
	aml_lcd_notifier_unregister(&bl_lcd_on_nb);
	aml_lcd_notifier_unregister(&bl_lcd_off_nb);
}

static inline void bl_vsync_handler(struct aml_bl_drv_s *bdrv)
{
	struct bl_metrics_config_s *bl_metrics_conf;
	unsigned int level = 0;

	if ((bdrv->state & BL_STATE_BL_ON) == 0)
		return;
	if (bdrv->brightness_bypass)
		return;

	bl_metrics_conf = &bdrv->bl_metrics_conf;
	if (bl_metrics_conf && bl_metrics_conf->level_buf) {
		if (bl_metrics_conf->sum_cnt < bl_metrics_conf->frame_rate) {
			bl_metrics_conf->level_count += bdrv->level;
			bl_metrics_conf->brightness_count +=
					bdrv->level_brightness;
			bl_metrics_conf->sum_cnt++;
		} else {
			bl_metrics_conf->level_buf[bl_metrics_conf->cnt] =
			bl_metrics_conf->level_count /
			bl_metrics_conf->frame_rate;
			bl_metrics_conf->brightness_buf[bl_metrics_conf->cnt] =
			bl_metrics_conf->brightness_count /
			bl_metrics_conf->frame_rate;
			bl_metrics_conf->cnt++;
			bl_metrics_conf->sum_cnt = 0;
			bl_metrics_conf->level_count = 0;
			bl_metrics_conf->brightness_count = 0;
		}
		if (bl_metrics_conf->cnt == BL_LEVEL_CNT_MAX) {
			bl_metrics_conf->sum_cnt = 0;
			bl_metrics_conf->cnt = 0;
		}
	}

	if ((bdrv->state & BL_STATE_GD_EN) == 0) {
		if (bdrv->level_brightness == bdrv->level)
			return;
		aml_bl_set_level(bdrv, bdrv->level_brightness);
	} else {
		level = bl_gd_level_map(bdrv, bdrv->level_gd);
		if (level == bdrv->level)
			return;
		aml_bl_set_level(bdrv, level);
	}
}

static irqreturn_t bl_vsync_isr(int irq, void *data)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)data;

	bl_vsync_handler(bdrv);
	return IRQ_HANDLED;
}

static int bl_vsync_irq_init(struct aml_bl_drv_s *bdrv)
{
	if (bdrv->res_vsync_irq[0]) {
		if (request_irq(bdrv->res_vsync_irq[0]->start,
				bl_vsync_isr, IRQF_SHARED,
				"bl_vsync", (void *)bdrv)) {
			BLPR("[%d]: can't request bl_vsync_irq\n", bdrv->index);
		} else {
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: request bl_vsync_irq successful\n",
				     bdrv->index);
			}
		}
	}

	return 0;
}

static void bl_vsync_irq_remove(struct aml_bl_drv_s *bdrv)
{
	if (bdrv->res_vsync_irq[0])
		free_irq(bdrv->res_vsync_irq[0]->start, (void *)"bl_vsync");
}

/***************************************************************/
static const char *bl_debug_usage_str = {
"Usage:\n"
"    cat status ; dump backlight config\n"
"\n"
"    echo freq <index> <pwm_freq> > pwm ; set pwm frequency(unit in Hz for pwm, vfreq multiple for pwm_vs)\n"
"    echo duty <index> <pwm_duty> > pwm ; set pwm duty cycle(unit: %)\n"
"    echo pol <index> <pwm_pol> > pwm ; set pwm polarity(unit: %)\n"
"	 echo max <index> <duty_max> > pwm ; set pwm duty_max(unit: %)\n"
"	 echo min <index> <duty_min> > pwm ; set pwm duty_min(unit: %)\n"
"    cat pwm ; dump pwm state\n"
"	 echo free <0|1> > pwm ; set pwm_duty_free enable or disable\n"
"\n"
"    echo <0|1> > power ; backlight power ctrl\n"
"    cat power ; print backlight power state\n"
};

static ssize_t bl_debug_help(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", bl_debug_usage_str);
}

static ssize_t bl_status_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_config_s *bconf;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext = bl_extern_get_driver(bdrv->index);
#endif
	ssize_t len = 0;

	bconf = &bdrv->bconf;
	len = sprintf(buf, "read backlight status:\n"
		      "key_valid:          %d\n"
		      "config_load:        %d\n"
		      "index:              %d\n"
		      "name:               %s\n"
		      "state:              0x%x\n"
		      "level:              %d\n"
		      "level_brightness:   %d\n"
		      "level_gd:           %d\n"
		      "level_uboot:        %d\n"
		      "level_default:      %d\n"
		      "step_on_flag        %d\n"
		      "brightness_bypass:  %d\n\n"
		      "debug_force:        %d\n\n"
		      "level_max:          %d\n"
		      "level_min:          %d\n"
		      "level_mid:          %d\n"
		      "level_mid_mapping:  %d\n\n"
		      "method:             %s\n"
		      "en_gpio:            %s(%d)\n"
		      "en_gpio_on:         %d\n"
		      "en_gpio_off:        %d\n"
		      "power_on_delay:     %d\n"
		      "power_off_delay:    %d\n\n",
		      bdrv->key_valid, bdrv->config_load,
		      bdrv->index, bconf->name, bdrv->state,
		      bdrv->level, bdrv->level_brightness, bdrv->level_gd,
		      bconf->level_uboot, bconf->level_default,
		      bdrv->step_on_flag, bdrv->brightness_bypass,
		      bdrv->debug_force,
		      bconf->level_max, bconf->level_min,
		      bconf->level_mid, bconf->level_mid_mapping,
		      bl_method_type_to_str(bconf->method),
		      bconf->bl_gpio[bconf->en_gpio].name,
		      bconf->en_gpio, bconf->en_gpio_on, bconf->en_gpio_off,
		      bconf->power_on_delay, bconf->power_off_delay);
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		len += sprintf(buf + len, "to do\n");
		break;
	case BL_CTRL_PWM:
		bl_pwm = bconf->bl_pwm;
		len += sprintf(buf + len,
			       "pwm_method:         %d\n"
			       "pwm_port:           %s(0x%x)\n"
			       "pwm_freq:           %d\n"
			       "pwm_duty_max:       %d\n"
			       "pwm_duty_min:       %d\n"
			       "pwm_on_delay:       %d\n"
			       "pwm_off_delay:      %d\n"
			       "en_sequence_reverse: %d\n\n",
			       bl_pwm->pwm_method,
			       bl_pwm_num_to_str(bl_pwm->pwm_port),
			       bl_pwm->pwm_port,
			       bl_pwm->pwm_freq,
			       bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min,
			       bconf->pwm_on_delay, bconf->pwm_off_delay,
			       bconf->en_sequence_reverse);
		break;
	case BL_CTRL_PWM_COMBO:
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		len += sprintf(buf + len,
			       "pwm_0_level_max:    %d\n"
			       "pwm_0_level_min:    %d\n"
			       "pwm_0_method:       %d\n"
			       "pwm_0_port:         %s(0x%x)\n"
			       "pwm_0_freq:         %d\n"
			       "pwm_0_duty_max:     %d\n"
			       "pwm_0_duty_min:     %d\n"
			       "pwm_1_level_max:    %d\n"
			       "pwm_1_level_min:    %d\n"
			       "pwm_1_method:       %d\n"
			       "pwm_1_port:         %s(0x%x)\n"
			       "pwm_1_freq:         %d\n"
			       "pwm_1_duty_max:     %d\n"
			       "pwm_1_duty_min:     %d\n"
			       "pwm_on_delay:       %d\n"
			       "pwm_off_delay:      %d\n"
			       "en_sequence_reverse: %d\n\n",
			       pwm_combo0->level_max, pwm_combo0->level_min,
			       pwm_combo0->pwm_method,
			       bl_pwm_num_to_str(pwm_combo0->pwm_port),
			       pwm_combo0->pwm_port,
			       pwm_combo0->pwm_freq,
			       pwm_combo0->pwm_duty_max,
			       pwm_combo0->pwm_duty_min,
			       pwm_combo1->level_max, pwm_combo1->level_min,
			       pwm_combo1->pwm_method,
			       bl_pwm_num_to_str(pwm_combo1->pwm_port),
			       pwm_combo1->pwm_port,
			       pwm_combo1->pwm_freq,
			       pwm_combo1->pwm_duty_max,
			       pwm_combo1->pwm_duty_min,
			       bconf->pwm_on_delay, bconf->pwm_off_delay,
			       bconf->en_sequence_reverse);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		if (ldim_drv->config_print)
			ldim_drv->config_print();
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		if (bext->config_print)
			bext->config_print(bext);
		break;
#endif
	default:
		len += sprintf(buf + len, "wrong backlight control method\n");
		break;
	}
	return len;
}

static ssize_t bl_debug_pwm_info_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_pwm_config_s *bl_pwm;
	struct pwm_state pstate;
	ssize_t len = 0;

	len = sprintf(buf, "read backlight pwm info:\n");
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		len += sprintf(buf + len,
			       "pwm_bypass:      %d\n"
			       "pwm_duty_free:   %d\n",
			       bdrv->pwm_bypass, bdrv->pwm_duty_free);
		if (bdrv->bconf.bl_pwm) {
			bl_pwm = bdrv->bconf.bl_pwm;
			len += sprintf(buf + len,
				       "pwm_index:          %d\n"
				       "pwm_port:           %s(0x%x)\n"
				       "pwm_method:         %d\n"
				       "pwm_freq:           %d\n"
				       "pwm_duty_max:       %d\n"
				       "pwm_duty_min:       %d\n"
				       "pwm_cnt:            %d\n"
				       "pwm_max:            %d\n"
				       "pwm_min:            %d\n"
				       "pwm_level:          %d\n",
				       bl_pwm->index,
				       bl_pwm_num_to_str(bl_pwm->pwm_port),
				       bl_pwm->pwm_port,
				       bl_pwm->pwm_method, bl_pwm->pwm_freq,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min,
				       bl_pwm->pwm_cnt,
				       bl_pwm->pwm_max, bl_pwm->pwm_min,
				       bl_pwm->pwm_level);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					       "pwm_duty:           %d(%d%%)\n",
					       bl_pwm->pwm_duty,
					       bl_pwm->pwm_duty * 100 / 255);
			} else {
				len += sprintf(buf + len,
					       "pwm_duty:           %d%%\n",
					       bl_pwm->pwm_duty);
			}
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
			case BL_PWM_AO_A:
			case BL_PWM_AO_B:
			case BL_PWM_AO_C:
			case BL_PWM_AO_D:
			case BL_PWM_AO_E:
			case BL_PWM_AO_F:
			case BL_PWM_AO_G:
			case BL_PWM_AO_H:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					len += sprintf(buf + len,
						       "pwm invalid\n");
					break;
				}
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				len += sprintf(buf + len,
					       "pwm state:\n"
					       "  period:           %lld\n"
					       "  duty_cycle:       %lld\n"
					       "  polarity:         %d\n"
					       "  enabled:          %d\n",
					       pstate.period, pstate.duty_cycle,
					       pstate.polarity, pstate.enabled);
				break;
			case BL_PWM_VS:
				len += sprintf(buf + len,
					       "pwm_reg0:            0x%08x\n"
					       "pwm_reg1:            0x%08x\n"
					       "pwm_reg2:            0x%08x\n"
					       "pwm_reg3:            0x%08x\n",
					       lcd_vcbus_read(VPU_VPU_PWM_V0),
					       lcd_vcbus_read(VPU_VPU_PWM_V1),
					       lcd_vcbus_read(VPU_VPU_PWM_V2),
					       lcd_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				len += sprintf(buf + len,
					       "invalid pwm_port: 0x%x\n",
					       bl_pwm->pwm_port);
				break;
			}
		}
		break;
	case BL_CTRL_PWM_COMBO:
		len += sprintf(buf + len,
			       "pwm_bypass:      %d\n"
			       "pwm_duty_free:   %d\n",
			       bdrv->pwm_bypass, bdrv->pwm_duty_free);
		if (bdrv->bconf.bl_pwm_combo0) {
			bl_pwm = bdrv->bconf.bl_pwm_combo0;
			len += sprintf(buf + len,
				       "pwm_0_index:        %d\n"
				       "pwm_0_port:         %s(0x%x)\n"
				       "pwm_0_method:       %d\n"
				       "pwm_0_freq:         %d\n"
				       "pwm_0_duty_max:     %d\n"
				       "pwm_0_duty_min:     %d\n"
				       "pwm_0_cnt:          %d\n"
				       "pwm_0_max:          %d\n"
				       "pwm_0_min:          %d\n"
				       "pwm_0_level:        %d\n",
				       bl_pwm->index,
				       bl_pwm_num_to_str(bl_pwm->pwm_port),
				       bl_pwm->pwm_port,
				       bl_pwm->pwm_method, bl_pwm->pwm_freq,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min,
				       bl_pwm->pwm_cnt,
				       bl_pwm->pwm_max, bl_pwm->pwm_min,
				       bl_pwm->pwm_level);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					       "pwm_0_duty:         %d(%d%%)\n",
					       bl_pwm->pwm_duty,
					       bl_pwm->pwm_duty * 100 / 255);
			} else {
				len += sprintf(buf + len,
					       "pwm_0_duty:         %d%%\n",
					       bl_pwm->pwm_duty);
			}
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
			case BL_PWM_AO_A:
			case BL_PWM_AO_B:
			case BL_PWM_AO_C:
			case BL_PWM_AO_D:
			case BL_PWM_AO_E:
			case BL_PWM_AO_F:
			case BL_PWM_AO_G:
			case BL_PWM_AO_H:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					len += sprintf(buf + len,
						       "pwm invalid\n");
					break;
				}
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				len += sprintf(buf + len,
					       "pwm state:\n"
					       "  period:           %lld\n"
					       "  duty_cycle:       %lld\n"
					       "  polarity:         %d\n"
					       "  enabled:          %d\n",
					       pstate.period, pstate.duty_cycle,
					       pstate.polarity, pstate.enabled);
				break;
			case BL_PWM_VS:
				len += sprintf(buf + len,
					       "pwm_0_reg0:         0x%08x\n"
					       "pwm_0_reg1:         0x%08x\n"
					       "pwm_0_reg2:         0x%08x\n"
					       "pwm_0_reg3:         0x%08x\n",
					       lcd_vcbus_read(VPU_VPU_PWM_V0),
					       lcd_vcbus_read(VPU_VPU_PWM_V1),
					       lcd_vcbus_read(VPU_VPU_PWM_V2),
					       lcd_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				len += sprintf(buf + len,
					       "invalid pwm_port: 0x%x\n",
					       bl_pwm->pwm_port);
				break;
			}
		}
		if (bdrv->bconf.bl_pwm_combo1) {
			bl_pwm = bdrv->bconf.bl_pwm_combo1;
			len += sprintf(buf + len,
				       "\n"
				       "pwm_1_index:        %d\n"
				       "pwm_1_port:         %s(0x%x)\n"
				       "pwm_1_method:       %d\n"
				       "pwm_1_freq:         %d\n"
				       "pwm_1_duty_max:     %d\n"
				       "pwm_1_duty_min:     %d\n"
				       "pwm_1_cnt:          %d\n"
				       "pwm_1_max:          %d\n"
				       "pwm_1_min:          %d\n"
				       "pwm_1_level:        %d\n",
				       bl_pwm->index,
				       bl_pwm_num_to_str(bl_pwm->pwm_port),
				       bl_pwm->pwm_port,
				       bl_pwm->pwm_method, bl_pwm->pwm_freq,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min,
				       bl_pwm->pwm_cnt,
				       bl_pwm->pwm_max, bl_pwm->pwm_min,
				       bl_pwm->pwm_level);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					       "pwm_1_duty:         %d(%d%%)\n",
					       bl_pwm->pwm_duty,
					       bl_pwm->pwm_duty * 100 / 255);
			} else {
				len += sprintf(buf + len,
					       "pwm_1_duty:         %d%%\n",
					       bl_pwm->pwm_duty);
			}
			switch (bl_pwm->pwm_port) {
			case BL_PWM_A:
			case BL_PWM_B:
			case BL_PWM_C:
			case BL_PWM_D:
			case BL_PWM_E:
			case BL_PWM_F:
			case BL_PWM_AO_A:
			case BL_PWM_AO_B:
			case BL_PWM_AO_C:
			case BL_PWM_AO_D:
			case BL_PWM_AO_E:
			case BL_PWM_AO_F:
			case BL_PWM_AO_G:
			case BL_PWM_AO_H:
				if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
					len += sprintf(buf + len,
						       "pwm invalid\n");
					break;
				}
				pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
				len += sprintf(buf + len,
					       "pwm state:\n"
					       "  period:           %lld\n"
					       "  duty_cycle:       %lld\n"
					       "  polarity:         %d\n"
					       "  enabled:          %d\n",
					       pstate.period, pstate.duty_cycle,
					       pstate.polarity, pstate.enabled);
				break;
			case BL_PWM_VS:
				len += sprintf(buf + len,
					       "pwm_1_reg0:         0x%08x\n"
					       "pwm_1_reg1:         0x%08x\n"
					       "pwm_1_reg2:         0x%08x\n"
					       "pwm_1_reg3:         0x%08x\n",
					       lcd_vcbus_read(VPU_VPU_PWM_V0),
					       lcd_vcbus_read(VPU_VPU_PWM_V1),
					       lcd_vcbus_read(VPU_VPU_PWM_V2),
					       lcd_vcbus_read(VPU_VPU_PWM_V3));
				break;
			default:
				len += sprintf(buf + len,
					       "invalid pwm_port: 0x%x\n",
					       bl_pwm->pwm_port);
				break;
			}
		}
		break;
	default:
		len += sprintf(buf + len, "not pwm control method\n");
		break;
	}
	return len;
}

static ssize_t bl_debug_pwm_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_pwm_config_s *bl_pwm;
	ssize_t len = 0;

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm) {
			bl_pwm = bdrv->bconf.bl_pwm;
			len += sprintf(buf + len,
				       "pwm: freq=%d, pol=%d, duty_max=%d, duty_min=%d, ",
				       bl_pwm->pwm_freq, bl_pwm->pwm_method,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					       "duty_value=%d(%d%%)\n",
					       bl_pwm->pwm_duty,
					       bl_pwm->pwm_duty * 100 / 255);
			} else {
				len += sprintf(buf + len,
					       "duty_value=%d%%\n",
					       bl_pwm->pwm_duty);
			}
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bdrv->bconf.bl_pwm_combo0) {
			bl_pwm = bdrv->bconf.bl_pwm_combo0;
			len += sprintf(buf + len,
				       "pwm_0: freq=%d, pol=%d, duty_max=%d, duty_min=%d, ",
				       bl_pwm->pwm_freq, bl_pwm->pwm_method,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					       "duty_value=%d(%d%%)\n",
					       bl_pwm->pwm_duty,
					       bl_pwm->pwm_duty * 100 / 255);
			} else {
				len += sprintf(buf + len,
					       "duty_value=%d%%\n",
					       bl_pwm->pwm_duty);
			}
		}

		if (bdrv->bconf.bl_pwm_combo1) {
			bl_pwm = bdrv->bconf.bl_pwm_combo1;
			len += sprintf(buf + len,
				       "pwm_1: freq=%d, pol=%d, duty_max=%d, duty_min=%d, ",
				       bl_pwm->pwm_freq, bl_pwm->pwm_method,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					       "duty_value=%d(%d%%)\n",
					       bl_pwm->pwm_duty,
					       bl_pwm->pwm_duty * 100 / 255);
			} else {
				len += sprintf(buf + len,
					       "duty_value=%d%%\n",
					       bl_pwm->pwm_duty);
			}
		}
		break;
	default:
		break;
	}
	return len;
}

#define BL_DEBUG_PWM_FREQ       0
#define BL_DEBUG_PWM_DUTY       1
#define BL_DEBUG_PWM_POL        2
#define BL_DEBUG_PWM_DUTY_MAX   3
#define BL_DEBUG_PWM_DUTY_MIN   4
static void bl_debug_pwm_set(struct aml_bl_drv_s *bdrv, unsigned int index,
			     unsigned int value, int state)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned long long temp;
	unsigned int pwm_range, temp_duty;

	if (aml_bl_check_driver(bdrv))
		return;

	mutex_lock(&bl_level_mutex);

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bl_pwm = bconf->bl_pwm;
		break;
	case BL_CTRL_PWM_COMBO:
		if (index == 0)
			bl_pwm = bconf->bl_pwm_combo0;
		else
			bl_pwm = bconf->bl_pwm_combo1;
		break;
	default:
		BLERR("not pwm control method\n");
		break;
	}
	if (bl_pwm) {
		switch (state) {
		case BL_DEBUG_PWM_FREQ:
			bl_pwm->pwm_freq = value;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: set index(%d) pwm_port(0x%x) freq: %dHz\n",
				     bdrv->index, index,
				     bl_pwm->pwm_port, bl_pwm->pwm_freq);
			}
			break;
		case BL_DEBUG_PWM_DUTY:
			bl_pwm->pwm_duty = value;
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty: %d\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty);
				} else {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty: %d%%\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty);
				}
			}
			break;
		case BL_DEBUG_PWM_POL:
			bl_pwm->pwm_method = value;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: set index(%d) pwm_port(0x%x) method: %d\n",
				     bdrv->index, index, bl_pwm->pwm_port,
				     bl_pwm->pwm_method);
			}
			break;
		case BL_DEBUG_PWM_DUTY_MAX:
			bl_pwm->pwm_duty_max = value;
			if (bl_pwm->pwm_duty_max > 100)
				pwm_range = 2550;
			else
				pwm_range = 1000;
			temp = bl_pwm->pwm_level;
			temp_duty = bl_do_div((temp * pwm_range),
					      bl_pwm->pwm_cnt);
			bl_pwm->pwm_duty = (temp_duty + 5) / 10;
			temp = bl_pwm->pwm_min;
			temp_duty = bl_do_div((temp * pwm_range),
					      bl_pwm->pwm_cnt);
			bl_pwm->pwm_duty_min = (temp_duty + 5) / 10;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_max: %d\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_max);
				} else {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_max: %d%%\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_max);
				}
			}
			break;
		case BL_DEBUG_PWM_DUTY_MIN:
			bl_pwm->pwm_duty_min = value;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_min: %d\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_min);
				} else {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_min: %d%%\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_min);
				}
			}
			break;
		default:
			break;
		}
	}
	mutex_unlock(&bl_level_mutex);
}

static ssize_t bl_debug_pwm_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int index = 0, val = 0;

	switch (buf[0]) {
	case 'f':
		if (buf[3] == 'q') { /* frequency */
			ret = sscanf(buf, "freq %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_FREQ);
			} else {
				BLERR("invalid parameters\n");
			}
		} else if (buf[3] == 'e') { /* duty free */
			ret = sscanf(buf, "free %d", &val);
			if (ret == 1) {
				bdrv->pwm_duty_free = (unsigned char)val;
				/*BLPR("[d]: set pwm_duty_free: %d\n",
				 *   bdrv->index, bdrv->pwm_duty_free);
				 */
			} else {
				BLERR("invalid parameters\n");
			}
		}
		break;
	case 'd': /* duty */
		ret = sscanf(buf, "duty %d %d", &index, &val);
		if (ret == 2)
			bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_DUTY);
		else
			BLERR("invalid parameters\n");
		break;
	case 'p': /* polarity */
		ret = sscanf(buf, "pol %d %d", &index, &val);
		if (ret == 2)
			bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_POL);
		else
			BLERR("invalid parameters\n");
		break;
	case 'b': /* bypass */
		ret = sscanf(buf, "bypass %d", &val);
		if (ret == 1) {
			bdrv->pwm_bypass = (unsigned char)val;
			BLPR("[%d]: set pwm_bypass: %d\n",
			     bdrv->index, bdrv->pwm_bypass);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'm':
		if (buf[1] == 'a') { /* max */
			ret = sscanf(buf, "max %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_DUTY_MAX);
			} else {
				BLERR("invalid parameters\n");
			}
		} else if (buf[1] == 'i') { /* min */
			ret = sscanf(buf, "min %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_DUTY_MIN);
			} else {
				BLERR("invalid parameters\n");
			}
		}
		break;
	default:
		BLERR("wrong command\n");
		break;
	}

	return count;
}

static ssize_t bl_debug_power_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	int pwr_state, real_state;

	if (bdrv->state & BL_STATE_BL_POWER_ON)
		pwr_state = 1;
	else
		pwr_state = 0;
	if (bdrv->state & BL_STATE_BL_ON)
		real_state = 1;
	else
		real_state = 0;
	return sprintf(buf, "backlight power state: %d, real state: %d\n",
		       pwr_state, real_state);
}

static ssize_t bl_debug_power_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	BLPR("[%d]: power control: %u\n", bdrv->index, temp);
	if (temp == 0) {
		bdrv->state &= ~BL_STATE_BL_POWER_ON;
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		bdrv->state |= BL_STATE_BL_POWER_ON;
		if ((bdrv->state & BL_STATE_BL_ON) == 0)
			bl_power_on(bdrv);
	}

	return count;
}

static ssize_t bl_debug_step_on_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "backlight step_on: %d\n", bdrv->step_on_flag);
}

static ssize_t bl_debug_step_on_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	bdrv->step_on_flag = (unsigned char)temp;
	BLPR("[%d]: step_on: %u\n", bdrv->index, bdrv->step_on_flag);

	return count;
}

static ssize_t bl_debug_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "bl power delay: on=%dms, off=%dms\n",
		       bdrv->bconf.power_on_delay,
		       bdrv->bconf.power_off_delay);
}

static ssize_t bl_debug_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int val[2];

	ret = sscanf(buf, "%d %d", &val[0], &val[1]);
	if (ret == 2) {
		bdrv->bconf.power_on_delay = val[0];
		bdrv->bconf.power_off_delay = val[1];
		pr_info("[%d]: set bl power_delay: on=%dms, off=%dms\n",
			bdrv->index, val[0], val[1]);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t bl_debug_brightness_bypass_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bdrv->brightness_bypass);
}

static ssize_t bl_debug_brightness_bypass_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int temp;
	int ret;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}
	bdrv->brightness_bypass = (unsigned char)temp;
	return count;
}

static void bl_brightness_metrics_calc(struct aml_bl_drv_s *bdrv)
{
	struct bl_metrics_config_s *bl_metrics_conf = &bdrv->bl_metrics_conf;
	unsigned int j = BL_LEVEL_CNT_MAX;
	unsigned int i = 0;
	unsigned int level_sum = 0;
	unsigned int brightness_sum = 0;
	unsigned int cnt = 0;
	unsigned int temp;

	cnt = bl_metrics_conf->cnt;
	temp = bl_metrics_conf->times;
	memcpy(&bl_metrics_conf->level_buf[j],
	       bl_metrics_conf->level_buf,
	       (sizeof(unsigned int) * BL_LEVEL_CNT_MAX));

	memcpy(&bl_metrics_conf->brightness_buf[j],
	       bl_metrics_conf->brightness_buf,
	       (sizeof(unsigned int) * BL_LEVEL_CNT_MAX));

	for (i = cnt + j; i > (cnt + j - temp); i--) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("cnt: %d, %d: brightness_buf: %d, level_buf: %d\n",
			     cnt, i,
			     bl_metrics_conf->brightness_buf[i],
			     bl_metrics_conf->level_buf[i]);
		}
		level_sum +=  bl_metrics_conf->level_buf[i];
		brightness_sum +=  bl_metrics_conf->brightness_buf[i];
	}

	bl_metrics_conf->level_metrics = level_sum / temp;
	bl_metrics_conf->brightness_metrics = brightness_sum / temp;
}

static ssize_t bl_brightness_metrics_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_metrics_config_s *bl_metrics_conf = &bdrv->bl_metrics_conf;

	if (!bl_metrics_conf->level_buf)
		return sprintf(buf, "bl_metrics_conf have no level_buf\n");

	bl_brightness_metrics_calc(bdrv);

	return sprintf(buf, "brightness_metrics: %d, level_metrics: %d\n",
		       bl_metrics_conf->brightness_metrics,
		       bl_metrics_conf->level_metrics);
}

static ssize_t bl_brightness_metrics_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_metrics_config_s *bl_metrics_conf = &bdrv->bl_metrics_conf;
	unsigned int temp;
	int ret;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	if (!bl_metrics_conf->level_buf) {
		BLERR("get no brightness value\n");
		return  -EINVAL;
	}

	if (temp > (BL_LEVEL_CNT_MAX / 60)) {
		BLPR("max support 60min\n");
		bl_metrics_conf->times = BL_LEVEL_CNT_MAX;
	} else {
		bl_metrics_conf->times = temp * 60;
	}

	bl_brightness_metrics_calc(bdrv);

	BLPR("time: %d, brightness_metrics: %d, level_metrics: %d\n",
	     bl_metrics_conf->times,
	     bl_metrics_conf->brightness_metrics,
	     bl_metrics_conf->level_metrics);

	return count;
}

static ssize_t bl_debug_level_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bdrv->level);
}

static ssize_t bl_debug_level_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int level = 0;
	unsigned int ret;

	ret = kstrtouint(buf, 10, &level);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	if (!bdrv->debug_force)
		bdrv->debug_force = 1;

	mutex_lock(&bl_level_mutex);

	aml_bl_set_level(bdrv, level);
	BLPR("[%d]: %s: %u, state: 0x%x\n",
	     bdrv->index, __func__, level, bdrv->state);

	mutex_unlock(&bl_level_mutex);

	return count;
}

static ssize_t bl_debug_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int val, temp, ret;

	switch (buf[0]) {
	case 'g': /* gd */
		ret = sscanf(buf, "gd %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;
			if ((bdrv->state & BL_STATE_GD_EN) == 0)
				bl_gd_sel_func(bdrv, 1);
			mutex_lock(&bl_level_mutex);
			bl_gd_diming_func(bdrv, val);
			mutex_unlock(&bl_level_mutex);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'b': /* brightness */
		ret = sscanf(buf, "brightness %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;

			mutex_lock(&bl_status_mutex);
			bdrv->level_brightness = bl_brightness_level_map(bdrv, val);

			if (bdrv->level_brightness == 0) {
				if (bdrv->state & BL_STATE_BL_ON)
					bl_power_off(bdrv);
			} else {
				if ((bdrv->state & BL_STATE_GD_EN) == 0) {
					aml_bl_set_level(bdrv, bdrv->level_brightness);
					BLPR("[%d]: debug brightness: %u->%u, state: 0x%x\n",
						bdrv->index, val, bdrv->level_brightness,
						bdrv->state);
				} else {
					temp = bl_gd_level_map(bdrv, bdrv->level_gd);
					aml_bl_set_level(bdrv, temp);
					BLPR("[%d]: debug brightness: %u->%u, state: 0x%x\n",
						bdrv->index, val, temp, bdrv->state);
				}

				if ((bdrv->state & BL_STATE_BL_ON) == 0)
					bl_power_on(bdrv);
			}
			mutex_unlock(&bl_status_mutex);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'l': /* level */
		ret = sscanf(buf, "level %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;

			mutex_lock(&bl_level_mutex);

			temp = bl_brightness_level_map(bdrv, val);
			aml_bl_set_level(bdrv, temp);
			BLPR("[%d]: debug level: %u, state: 0x%x\n",
				bdrv->index, temp, bdrv->state);

			mutex_unlock(&bl_level_mutex);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'o': /* off */
		if (bdrv->state & BL_STATE_GD_EN) {
			temp = 0;
			aml_lcd_atomic_notifier_call_chain(LCD_EVENT_BACKLIGHT_GD_SEL, &temp);
		}
		bdrv->state &= ~BL_STATE_DEBUG_FORCE_EN;
		bdrv->debug_force = 0;
		break;
	default:
		BLERR("wrong command\n");
		break;
	}

	return count;
}

static struct device_attribute bl_debug_attrs[] = {
	__ATTR(help, 0444, bl_debug_help, NULL),
	__ATTR(status, 0444, bl_status_show, NULL),
	__ATTR(pwm_info, 0444, bl_debug_pwm_info_show, NULL),
	__ATTR(pwm, 0644, bl_debug_pwm_show, bl_debug_pwm_store),
	__ATTR(power_on, 0644, bl_debug_power_show, bl_debug_power_store),
	__ATTR(step_on, 0644, bl_debug_step_on_show, bl_debug_step_on_store),
	__ATTR(delay, 0644, bl_debug_delay_show, bl_debug_delay_store),
	__ATTR(brightness_bypass, 0644, bl_debug_brightness_bypass_show,
	       bl_debug_brightness_bypass_store),
	__ATTR(brightness_metrics, 0644, bl_brightness_metrics_show,
	       bl_brightness_metrics_store),
	__ATTR(debug_level, 0644, bl_debug_level_show, bl_debug_level_store),
	__ATTR(debug, 0644, bl_debug_help, bl_debug_store),
};

static int bl_debug_file_creat(struct aml_bl_drv_s *bdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_debug_attrs); i++) {
		if (device_create_file(bdrv->dev, &bl_debug_attrs[i])) {
			BLERR("[%d]: create debug attribute %s fail\n",
			       bdrv->index, bl_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

static int bl_debug_file_remove(struct aml_bl_drv_s *bdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_debug_attrs); i++)
		device_remove_file(bdrv->dev, &bl_debug_attrs[i]);

	return 0;
}

/* ************************************************************* */
static int bl_io_open(struct inode *inode, struct file *file)
{
	struct aml_bl_drv_s *bdrv;

	bdrv = container_of(inode->i_cdev, struct aml_bl_drv_s, cdev);
	file->private_data = bdrv;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);

	return 0;
}

static int bl_io_release(struct inode *inode, struct file *file)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long bl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long bl_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = bl_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations bl_fops = {
	.owner          = THIS_MODULE,
	.open           = bl_io_open,
	.release        = bl_io_release,
	.unlocked_ioctl = bl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = bl_compat_ioctl,
#endif
};

static int bl_cdev_add(struct aml_bl_drv_s *bdrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!bdrv) {
		BLERR("%s: bdrv is null\n", __func__);
		return -1;
	}
	if (!bl_cdev) {
		ret = 1;
		goto bl_cdev_add_failed;
	}

	devno = MKDEV(MAJOR(bl_cdev->devno), bdrv->index);

	cdev_init(&bdrv->cdev, &bl_fops);
	bdrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&bdrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto bl_cdev_add_failed;
	}

	bdrv->dev = device_create(bl_cdev->class, parent,
				  devno, NULL, "bl%d", bdrv->index);
	if (IS_ERR_OR_NULL(bdrv->dev)) {
		ret = 3;
		goto bl_cdev_add_failed1;
	}

	dev_set_drvdata(bdrv->dev, bdrv);
	bdrv->dev->of_node = parent->of_node;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s OK\n", bdrv->index, __func__);
	return 0;

bl_cdev_add_failed1:
	cdev_del(&bdrv->cdev);
bl_cdev_add_failed:
	BLERR("[%d]: %s: failed: %d\n", bdrv->index, __func__, ret);
	return -1;
}

static void bl_cdev_remove(struct aml_bl_drv_s *bdrv)
{
	dev_t devno;

	if (!bl_cdev || !bdrv)
		return;

	devno = MKDEV(MAJOR(bl_cdev->devno), bdrv->index);
	device_destroy(bl_cdev->class, devno);
	cdev_del(&bdrv->cdev);
}

static int bl_global_init_once(void)
{
	int ret;

	if (bl_global_init_flag) {
		bl_global_init_flag++;
		return 0;
	}
	bl_global_init_flag++;

	bl_notifier_init();

	bl_cdev = kzalloc(sizeof(*bl_cdev), GFP_KERNEL);
	if (!bl_cdev)
		return -1;

	ret = alloc_chrdev_region(&bl_cdev->devno, 0,
				  LCD_MAX_DRV, BL_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto bl_global_init_once_err;
	}

	bl_cdev->class = class_create(THIS_MODULE, "aml_bl");
	if (IS_ERR_OR_NULL(bl_cdev->class)) {
		ret = 2;
		goto bl_global_init_once_err_1;
	}

	return 0;

bl_global_init_once_err_1:
	unregister_chrdev_region(bl_cdev->devno, LCD_MAX_DRV);
bl_global_init_once_err:
	kfree(bl_cdev);
	bl_cdev = NULL;
	BLERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void bl_global_remove_once(void)
{
	if (bl_global_init_flag > 1) {
		bl_global_init_flag--;
		return;
	}
	bl_global_init_flag--;

	bl_notifier_remove();

	if (!bl_cdev)
		return;

	class_destroy(bl_cdev->class);
	unregister_chrdev_region(bl_cdev->devno, LCD_MAX_DRV);
	kfree(bl_cdev);
	bl_cdev = NULL;
}

/* **************************************** */

#ifdef CONFIG_PM
static int aml_bl_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int aml_bl_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

#ifdef CONFIG_OF
static struct bl_data_s bl_data_g12a = {
	.chip_type = LCD_CHIP_G12A,
	.chip_name = "g12a",
	.pwm_vs_flag = 0,
};

static struct bl_data_s bl_data_g12b = {
	.chip_type = LCD_CHIP_G12B,
	.chip_name = "g12b",
	.pwm_vs_flag = 0,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct bl_data_s bl_data_tl1 = {
	.chip_type = LCD_CHIP_TL1,
	.chip_name = "tl1",
	.pwm_vs_flag = 1,
};
#endif

static struct bl_data_s bl_data_sm1 = {
	.chip_type = LCD_CHIP_SM1,
	.chip_name = "sm1",
	.pwm_vs_flag = 0,
};

static struct bl_data_s bl_data_tm2 = {
	.chip_type = LCD_CHIP_TM2,
	.chip_name = "tm2",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_tm2b = {
	.chip_type = LCD_CHIP_TM2B,
	.chip_name = "tm2b",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5 = {
	.chip_type = LCD_CHIP_T5,
	.chip_name = "t5",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5d = {
	.chip_type = LCD_CHIP_T5D,
	.chip_name = "t5d",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t7 = {
	.chip_type = LCD_CHIP_T7,
	.chip_name = "t7",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t3 = {
	.chip_type = LCD_CHIP_T3,
	.chip_name = "t3",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5w = {
	.chip_type = LCD_CHIP_T5W,
	.chip_name = "t5w",
	.pwm_vs_flag = 1,
};

static const struct of_device_id bl_dt_match_table[] = {
	{
		.compatible = "amlogic, backlight-g12a",
		.data = &bl_data_g12a,
	},
	{
		.compatible = "amlogic, backlight-g12b",
		.data = &bl_data_g12b,
	},
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, backlight-tl1",
		.data = &bl_data_tl1,
	},
#endif
	{
		.compatible = "amlogic, backlight-sm1",
		.data = &bl_data_sm1,
	},
	{
		.compatible = "amlogic, backlight-tm2",
		.data = &bl_data_tm2,
	},
	{
		.compatible = "amlogic, backlight-tm2b",
		.data = &bl_data_tm2b,
	},
	{
		.compatible = "amlogic, backlight-t5",
		.data = &bl_data_t5,
	},
	{
		.compatible = "amlogic, backlight-t5d",
		.data = &bl_data_t5d,
	},
	{
		.compatible = "amlogic, backlight-t7",
		.data = &bl_data_t7,
	},
	{
		.compatible = "amlogic, backlight-t3",
		.data = &bl_data_t3,
	},
	{
		.compatible = "amlogic, backlight-t5w",
		.data = &bl_data_t5w,
	},
	{}
};
#endif

static void aml_bl_power_init(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;

	/* update bl status */
	bdrv->state = (BL_STATE_LCD_ON | BL_STATE_BL_POWER_ON);
	bdrv->on_request = 1;
	/* lcd power on sequence control */
	if (bconf->method < BL_CTRL_MAX) {
		lcd_queue_delayed_work(&bdrv->delayed_on_work,
				       bconf->power_on_delay);
	} else {
		BLERR("[%d]: wrong backlight control method\n", bdrv->index);
	}
}

static void bl_init_status_update(struct aml_bl_drv_s *bdrv)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = aml_lcd_get_driver(bdrv->index);
	if (!pdrv)
		return;

	/* default disable lcd & backlight */
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return;

	if (pdrv->boot_ctrl) {
		if (pdrv->boot_ctrl->init_level == LCD_INIT_LEVEL_KERNEL_ON) {
			BLPR("[%d]: power on for init_level %d\n",
			     bdrv->index, pdrv->boot_ctrl->init_level);
			aml_bl_power_init(bdrv);
			return;
		}
	}

	/* update bl status */
	bdrv->state = (BL_STATE_LCD_ON | BL_STATE_BL_POWER_ON | BL_STATE_BL_ON);
	bdrv->on_request = 1;

	mutex_lock(&bl_level_mutex);
	if (bdrv->brightness_bypass) {
		aml_bl_set_level(bdrv, bdrv->level_init_on);
	} else {
		bdrv->level_brightness = bl_brightness_level_map(bdrv,
						bdrv->bldev->props.brightness);
		aml_bl_init_level(bdrv, bdrv->level_brightness);
	}
	mutex_unlock(&bl_level_mutex);

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
		bl_pwm_pinmux_set(bdrv, 1);
		break;
	default:
		break;
	}
}

static void aml_bl_config_probe_work(struct work_struct *work)
{
	struct aml_bl_drv_s *bdrv;
	struct bl_metrics_config_s *bl_metrics_conf = NULL;
	struct backlight_properties props;
	struct backlight_device *bldev;
	char bl_name[10];
	int index;
	int ret;

	bdrv = container_of(work, struct aml_bl_drv_s, config_probe_work);

	index = bdrv->index;
	bdrv->pinmux_flag = 0xff;
	bdrv->bconf.level_default = 128;
	bdrv->bconf.level_mid = 128;
	bdrv->bconf.level_mid_mapping = 128;
	bdrv->bconf.level_min = 10;
	bdrv->bconf.level_max = 255;
	bdrv->bconf.power_on_delay = 100;
	bdrv->bconf.power_off_delay = 30;
	bdrv->bconf.method = BL_CTRL_MAX;
	ret = bl_config_load(bdrv, bdrv->pdev);
	if (ret)
		goto err;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = (bdrv->bconf.level_max > 0 ?
				bdrv->bconf.level_max : BL_LEVEL_MAX);
	props.brightness = bdrv->level_init_on;

	if (index == 0)
		sprintf(bl_name, "aml-bl");
	else
		sprintf(bl_name, "aml-bl%d", index);
	bldev = backlight_device_register(bl_name, &bdrv->pdev->dev,
					  bdrv, &aml_bl_ops, &props);
	if (IS_ERR(bldev)) {
		BLERR("[%d]: failed to register backlight\n", index);
		ret = PTR_ERR(bldev);
		goto err;
	}
	bdrv->bldev = bldev;

	bl_metrics_conf = &bdrv->bl_metrics_conf;
	bl_metrics_conf->times = 60;
	bl_metrics_conf->cnt = 0;
	bl_metrics_conf->sum_cnt = 0;
	bl_metrics_conf->level_count = 0;
	bl_metrics_conf->brightness_count = 0;
	bl_metrics_conf->frame_rate = 60;
	bl_metrics_conf->level_buf = kcalloc(BL_LEVEL_CNT_MAX * 2,
					     sizeof(unsigned int), GFP_KERNEL);
	if (!bl_metrics_conf->level_buf)
		goto err;

	bl_metrics_conf->brightness_buf = kcalloc(BL_LEVEL_CNT_MAX * 2,
					     sizeof(unsigned int), GFP_KERNEL);
	if (!bl_metrics_conf->brightness_buf) {
		kfree(bl_metrics_conf->level_buf);
		bl_metrics_conf->level_buf = NULL;
		goto err;
	}

	memset(bl_metrics_conf->level_buf, 0,
	       (sizeof(unsigned int)) * BL_LEVEL_CNT_MAX * 2);
	bdrv->probe_done = 1;

	/* init workqueue */
	INIT_DELAYED_WORK(&bdrv->delayed_on_work, bl_delayd_on);

	bl_init_status_update(bdrv);

	bl_vsync_irq_init(bdrv);
	bl_debug_file_creat(bdrv);

	BLPR("[%d]: %s: ok\n", index, __func__);
	return;

err:
	/* free drvdata */
	platform_set_drvdata(bdrv->pdev, NULL);
	/* free drv */
	kfree(bdrv);
	bl_drv[index] = NULL;
	bl_drv_init_state &= ~(1 << index);
	BLPR("[%d]: %s: failed\n", index, __func__);
}

int aml_bl_index_add(int drv_index, int conf_index)
{
	if (drv_index >= LCD_MAX_DRV) {
		BLERR("%s: invalid drv_index: %d\n", __func__, drv_index);
		return -1;
	}

	bl_index_lut[drv_index] = conf_index;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("%s: drv_index %d, config index: %d\n",
			__func__, drv_index, conf_index);
	}
	return 0;
}

static int aml_bl_probe(struct platform_device *pdev)
{
	struct aml_bl_drv_s *bdrv;
	const struct of_device_id *match;
	int index = 0;
	int ret;

	bl_global_init_once();

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		BLERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (bl_drv_init_state & (1 << index)) {
		BLERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	bl_drv_init_state |= (1 << index);

	match = of_match_device(bl_dt_match_table, &pdev->dev);
	if (!match) {
		BLERR("%s: no match table\n", __func__);
		goto aml_bl_probe_err;
	}

	bdrv = kzalloc(sizeof(*bdrv), GFP_KERNEL);
	if (!bdrv)
		goto aml_bl_probe_err;
	bdrv->index = index;
	bdrv->data = (struct bl_data_s *)match->data;
	bl_drv[index] = bdrv;
	BLPR("chip type: (%d-%s)\n",
	     bdrv->data->chip_type,
	     bdrv->data->chip_name);

	/* set drvdata */
	platform_set_drvdata(pdev, bdrv);
	bl_cdev_add(bdrv, &pdev->dev);
	bdrv->pdev = pdev;

	bl_pwm_init_config_probe(bdrv->data);

	INIT_WORK(&bdrv->config_probe_work, aml_bl_config_probe_work);
	lcd_queue_work(&bdrv->config_probe_work);

	BLPR("[%d]: probe OK, init_state:0x%x\n", index, bl_drv_init_state);
	return 0;

aml_bl_probe_err:
	bl_drv_init_state &= ~(1 << index);
	BLPR("[%d]: %s failed\n", index, __func__);
	return -1;
}

static int __exit aml_bl_remove(struct platform_device *pdev)
{
	struct aml_bl_drv_s *bdrv = platform_get_drvdata(pdev);
	int index;

	if (!bdrv)
		return 0;

	index = bdrv->index;

	kfree(bdrv->bl_metrics_conf.level_buf);
	kfree(bdrv->bl_metrics_conf.brightness_buf);
	cancel_delayed_work_sync(&bdrv->delayed_on_work);
	backlight_device_unregister(bdrv->bldev);

	bl_debug_file_remove(bdrv);
	bl_vsync_irq_remove(bdrv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	bl_cdev_remove(bdrv);

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bdrv->bconf.ldim_flag)
		aml_ldim_remove();
#endif
	/* platform_set_drvdata(pdev, NULL); */
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		kfree(bdrv->bconf.bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		kfree(bdrv->bconf.bl_pwm_combo0);
		kfree(bdrv->bconf.bl_pwm_combo1);
		break;
	default:
		break;
	}
	kfree(bdrv);
	bl_drv[index] = NULL;
	bl_drv_init_state &= ~(1 << index);
	bl_global_remove_once();

	return 0;
}

static struct platform_driver aml_bl_driver = {
	.driver = {
		.name  = AML_BL_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(bl_dt_match_table),
#endif
	},
	.probe   = aml_bl_probe,
	.remove  = __exit_p(aml_bl_remove),
#ifdef CONFIG_PM
	.suspend = aml_bl_suspend,
	.resume  = aml_bl_resume,
#endif
};

int __init aml_bl_init(void)
{
	if (platform_driver_register(&aml_bl_driver)) {
		BLPR("failed to register bl driver module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit aml_bl_exit(void)
{
	platform_driver_unregister(&aml_bl_driver);
}

static int aml_bl_level_setup(char *str)
{
	int ret = 0;

	if (str) {
		ret = kstrtouint(str, 10, &bl_level[0]);
		BLPR("bl_level: %d\n", bl_level[0]);
	}

	return ret;
}
__setup("bl_level=", aml_bl_level_setup);

//MODULE_DESCRIPTION("AML Backlight Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
