/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 *
 */

#ifndef __INC_AML_BL_H
#define __INC_AML_BL_H
#include <linux/workqueue.h>
#include <linux/cdev.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>

#define BLPR(fmt, args...)      pr_info("bl: " fmt "", ## args)
#define BLERR(fmt, args...)     pr_err("bl error: " fmt "", ## args)
#define AML_BL_NAME		"aml-bl"

#define BL_LEVEL_MAX		255
#define BL_LEVEL_MIN		10
#define BL_LEVEL_OFF		1

#define BL_LEVEL_MID		128
#define BL_LEVEL_MID_MAPPED	BL_LEVEL_MID
#define BL_LEVEL_DEFAULT	BL_LEVEL_MID

#define XTAL_FREQ_HZ		(24 * 1000 * 1000) /* unit: Hz */
#define XTAL_HALF_FREQ_HZ	(24 * 1000 * 500)  /* 24M/2 in HZ */

#define BL_FREQ_DEFAULT		1000 /* unit: HZ */
#define BL_FREQ_VS_DEFAULT	2    /* multiple 2 of vfreq */

struct bl_data_s {
	unsigned int chip_type;
	const char *chip_name;
	unsigned int pwm_vs_flag;
};

/* for lcd backlight power */
enum bl_ctrl_method_e {
	BL_CTRL_GPIO = 0,
	BL_CTRL_PWM,
	BL_CTRL_PWM_COMBO,
	BL_CTRL_LOCAL_DIMMING,
	BL_CTRL_EXTERN,
	BL_CTRL_MAX,
};

enum bl_pwm_method_e {
	BL_PWM_NEGATIVE = 0,
	BL_PWM_POSITIVE,
	BL_PWM_METHOD_MAX,
};

enum bl_pwm_port_e {
	BL_PWM_A = 0,
	BL_PWM_B,
	BL_PWM_C,
	BL_PWM_D,
	BL_PWM_E,
	BL_PWM_F,
	BL_PWM_G,
	BL_PWM_H,
	BL_PWM_I,
	BL_PWM_J,
	BL_PWM_K,
	BL_PWM_L,
	BL_PWM_M,
	BL_PWM_N,
	BL_PWM_AO_A = 0x50,
	BL_PWM_AO_B,
	BL_PWM_AO_C,
	BL_PWM_AO_D,
	BL_PWM_AO_E,
	BL_PWM_AO_F,
	BL_PWM_AO_G,
	BL_PWM_AO_H,
	BL_PWM_VS = 0xa0,
	BL_PWM_MAX = 0xff,
};

enum bl_off_policy_e {
	BL_OFF_POLICY_NONE = 0,
	BL_OFF_POLICY_ALWAYS,
	BL_OFF_POLICY_ONCE,
	BL_OFF_POLICY_MAX,
};

#define BL_LEVEL_MASK                        0xfff
#define BL_POLICY_BRIGHTNESS_BYPASS_BIT      15
#define BL_POLICY_BRIGHTNESS_BYPASS_MASK     1
#define BL_POLICY_POWER_ON_BIT               12
#define BL_POLICY_POWER_ON_MASK              3

#define BL_GPIO_MAX             0xff
#define BL_GPIO_NUM_MAX         5
struct bl_gpio_s {
	char name[LCD_CPU_GPIO_NAME_MAX];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

struct pwm_data_s {
	unsigned int meson_index;
	struct pwm_state state;
	struct pwm_device *pwm;
	struct meson_pwm *meson;
};

struct bl_pwm_config_s {
	unsigned int index;
	unsigned int drv_index;
	struct pwm_data_s pwm_data;
	enum bl_pwm_method_e pwm_method;
	enum bl_pwm_port_e pwm_port;
	unsigned int level_max;
	unsigned int level_min;
	unsigned int pwm_freq; /* pwm_vs: 1~4(vfreq), pwm: freq(unit: Hz) */
	unsigned int pwm_duty; /* unit: % */
	unsigned int pwm_duty_save; /* unit: %, for power on recovery */
	unsigned int pwm_duty_max; /* unit: % */
	unsigned int pwm_duty_min; /* unit: % */
	unsigned int pwm_cnt; /* internal used for pwm control */
	unsigned int pwm_pre_div; /* internal used for pwm control */
	unsigned int pwm_max; /* internal used for pwm control */
	unsigned int pwm_min; /* internal used for pwm control */
	unsigned int pwm_level; /* internal used for pwm control */
	unsigned int pwm_mapping[7]; /* mapping curve for pwm control */
};

#define BL_NAME_MAX    30
struct bl_config_s {
	unsigned int index;
	char name[BL_NAME_MAX];
	unsigned int level_uboot;
	unsigned int level_default;
	unsigned int level_min;
	unsigned int level_max;
	unsigned int level_mid;
	unsigned int level_mid_mapping;
	unsigned int ldim_flag;

	enum bl_ctrl_method_e method;
	unsigned int en_gpio;
	unsigned int en_gpio_on;
	unsigned int en_gpio_off;
	unsigned short power_on_delay;
	unsigned short power_off_delay;
	unsigned int dim_max;
	unsigned int dim_min;
	unsigned int en_sequence_reverse;

	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *bl_pwm_combo0;
	struct bl_pwm_config_s *bl_pwm_combo1;
	unsigned int pwm_on_delay;
	unsigned int pwm_off_delay;

	struct bl_gpio_s bl_gpio[BL_GPIO_NUM_MAX];
	unsigned int bl_extern_index;
};

#define BL_LEVEL_CNT_MAX	      3600 //default 1h
struct bl_metrics_config_s {
	unsigned int frame_rate;
	unsigned int level_count;
	unsigned int brightness_count;
	unsigned int level_metrics;
	unsigned int brightness_metrics;
	unsigned int cnt;
	unsigned int sum_cnt;
	unsigned int times;
	unsigned int *level_buf;
	unsigned int *brightness_buf;
};

/* backlight_properties: state */
/* Flags used to signal drivers of state changes */
/* Upper 4 bits in bl props are reserved for driver internal use */
#define BL_STATE_DEBUG_FORCE_EN       BIT(8)
#define BL_STATE_GD_EN                BIT(4)
#define BL_STATE_LCD_ON               BIT(3)
#define BL_STATE_BL_INIT_ON           BIT(2)
#define BL_STATE_BL_POWER_ON          BIT(1)
#define BL_STATE_BL_ON                BIT(0)
/* #define BL_POWER_ON_DELAY_WORK */
struct aml_bl_drv_s {
	unsigned int index;
	unsigned int key_valid;
	unsigned int retry_cnt;
	unsigned int config_load;
	unsigned int state;
	unsigned int level;
	unsigned int level_brightness;
	unsigned int level_gd;
	unsigned int level_init_on;

	unsigned char probe_done;
	unsigned char brightness_bypass;
	unsigned char step_on_flag;
	unsigned char on_request; /* for lcd power sequence */
	unsigned char off_policy_cnt; /* bl_off_policy support */
	unsigned char pwm_bypass; /*debug flag*/
	unsigned char pwm_duty_free; /*debug flag*/
	unsigned char debug_force;

	struct bl_metrics_config_s bl_metrics_conf;
	struct bl_config_s        bconf;
	struct cdev               cdev;
	struct bl_data_s          *data;
	struct platform_device    *pdev;
	struct device             *dev;
	struct backlight_device   *bldev;
	struct delayed_work       config_probe_dly_work;
	struct delayed_work       delayed_on_work;
	struct resource *res_ldim_vsync_irq;
	struct resource *res_ldim_pwm_vs_irq;
	struct resource *res_vsync_irq[3];
	/*struct resource *res_ldim_rdma_irq;*/

	struct pinctrl *pin;
	unsigned int pinmux_flag;
};

struct aml_bl_drv_s *aml_bl_get_driver(int index);
int aml_bl_index_add(int drv_index, int conf_index);

int aml_bl_set_level_brightness(struct aml_bl_drv_s *bdrv, unsigned int brightness);
unsigned int aml_bl_get_level_brightness(struct aml_bl_drv_s *bdrv);

void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm);
enum bl_pwm_port_e bl_pwm_str_to_pwm(const char *str);
void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status);

#define BL_GPIO_OUTPUT_LOW		0
#define BL_GPIO_OUTPUT_HIGH		1
#define BL_GPIO_INPUT			2

#endif

