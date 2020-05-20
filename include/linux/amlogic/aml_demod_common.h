/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DEMOD_COMMON_H__
#define __AML_DEMOD_COMMON_H__

#include <linux/i2c.h>
#include <uapi/linux/videodev2.h>
#include <linux/module.h>
#include <linux/of_gpio.h>

enum tuner_type {
	AM_TUNER_NONE = 0,
	AM_TUNER_SI2176 = 1,
	AM_TUNER_SI2196 = 2,
	AM_TUNER_FQ1216 = 3,
	AM_TUNER_HTM = 4,
	AM_TUNER_CTC703 = 5,
	AM_TUNER_SI2177 = 6,
	AM_TUNER_R840 = 7,
	AM_TUNER_SI2157 = 8,
	AM_TUNER_SI2151 = 9,
	AM_TUNER_MXL661 = 10,
	AM_TUNER_MXL608 = 11,
	AM_TUNER_SI2159 = 12,
	AM_TUNER_R842 = 13,
	AM_TUNER_ATBM2040 = 14,
	AM_TUNER_ATBM253 = 15,
	AM_TUNER_SI2124 = 16,
	AM_TUNER_AV2011 = 17,
	AM_TUNER_AV2012 = 18,
	AM_TUNER_AV2018 = 19,
	AM_TUNER_R836 = 20,
	AM_TUNER_R848 = 21,
	AM_TUNER_MXL603 = 22
};

enum atv_demod_type {
	AM_ATV_DEMOD_NONE   = 0,
	AM_ATV_DEMOD_SI2176 = 1,
	AM_ATV_DEMOD_SI2196 = 2,
	AM_ATV_DEMOD_FQ1216 = 3,
	AM_ATV_DEMOD_HTM    = 4,
	AM_ATV_DEMOD_CTC703 = 5,
	AM_ATV_DEMOD_SI2177 = 6,
	AM_ATV_DEMOD_AMLATV = 7
};

enum dtv_demod_type {
	AM_DTV_DEMOD_NONE     = 0,
	AM_DTV_DEMOD_AMLDTV   = 1,
	AM_DTV_DEMOD_M1       = 2,
	AM_DTV_DEMOD_MXL101   = 3,
	AM_DTV_DEMOD_AVL6211  = 4,
	AM_DTV_DEMOD_SI2168   = 5,
	AM_DTV_DEMOD_ITE9133  = 6,
	AM_DTV_DEMOD_ITE9173  = 7,
	AM_DTV_DEMOD_DIB8096  = 8,
	AM_DTV_DEMOD_ATBM8869 = 9,
	AM_DTV_DEMOD_MXL241   = 10,
	AM_DTV_DEMOD_AVL68xx  = 11,
	AM_DTV_DEMOD_MXL683   = 12,
	AM_DTV_DEMOD_ATBM8881 = 13,
	AM_DTV_DEMOD_ATBM7821 = 14,
	AM_DTV_DEMOD_AVL6762  = 15,
	AM_DTV_DEMOD_CXD2856  = 16,
	AM_DTV_DEMOD_MXL248   = 17
};

enum aml_fe_dev_type {
	AM_DEV_TUNER,
	AM_DEV_ATV_DEMOD,
	AM_DEV_DTV_DEMOD
};

/* For configure different tuners */
/* It can add fields as extensions */
struct tuner_config {
	u32 code; /* tuner chip code */
	u8 id; /* enum tuner type */
	u8 i2c_addr;
	u8 i2c_id;
	struct i2c_adapter *i2c_adap;
	u8 xtal; /* 0: 16MHz, 1: 24MHz, 3: 27MHz */
	u32 xtal_cap; /* load capacitor */
	u8 xtal_mode;
	u8 lt_out; /* loop through out, 0: off, 1: on. */

	u32 reserved0;
	u32 reserved1;
};

/* For configure gpio */
struct gpio_config {
	s32 pin; /* gpio pin */
	u8 dir; /* direction, 0: out, 1: in. */
	u32 value; /* input or output active value */
	u32 reserved0;
	u32 reserved1;
};

/* For configure different demod */
struct demod_config {
	u32 code; /* demod chip code */
	u8 id; /* enum demod type */
	u8 mode; /* 0: internal, 1: external */
	u8 ts; /* ts port */
	u8 ts_out_mode; /* serial or parallel; 0:serial, 1:parallel */

	u8 i2c_addr;
	u8 i2c_id;
	struct i2c_adapter *i2c_adap;

	struct tuner_config tuner0;
	struct tuner_config tuner1;

	struct gpio_config reset;
	struct gpio_config ant_power;

	u8 reserved0;
	u8 reserved1;
};

/* For configure multi-tuner */
struct aml_tuner {
	struct tuner_config cfg;
	unsigned int i2c_adapter_id;
};

/** generic AML DVB attach function. */
#ifdef CONFIG_MEDIA_ATTACH
#define aml_dvb_attach(FUNCTION, ARGS...) ({ \
	void *__r = NULL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (void *) __a(ARGS); \
		if (__r == NULL) \
			symbol_put(FUNCTION); \
	} else { \
		printk(KERN_ERR "AML DVB: Unable to find symbol " \
			#FUNCTION"()\n"); \
	} \
	__r; \
})

#define aml_dvb_detach(FUNC) symbol_put_addr(FUNC)

#else
#define aml_dvb_attach(FUNCTION, ARGS...) ({ \
	FUNCTION(ARGS); \
})

#define aml_dvb_detach(FUNC) {}
#endif

static __maybe_unused int aml_demod_gpio_set(int gpio, int dir, int value,
		const char *label)
{
	if (gpio_is_valid(gpio)) {
		gpio_request(gpio, label);
		if (dir == GPIOF_DIR_OUT)
			gpio_direction_output(gpio, value);
		else {
			gpio_direction_input(gpio);
			gpio_set_value(gpio, value);
		}
	} else
		return -1;

	return 0;
}

static __maybe_unused int aml_demod_gpio_config(struct gpio_config *cfg,
		const char *label)
{
	return aml_demod_gpio_set(cfg->pin, cfg->dir, cfg->value, label);
}

#endif /* __AML_DEMOD_COMMON_H__ */
