/*
 * sound/soc/aml/m8/aml_g9tv.h
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

#ifndef AML_G9TV_H
#define AML_G9TV_H

#include <sound/soc.h>
#include <linux/gpio/consumer.h>

#define AML_I2C_BUS_AO 0
#define AML_I2C_BUS_A 1
#define AML_I2C_BUS_B 2
#define AML_I2C_BUS_C 3
#define AML_I2C_BUS_D 4

struct aml_audio_private_data {
	int clock_en;
	bool suspended;
	void *data;

	int hp_last_state;
	bool hp_det_status;
	unsigned int hp_val_h;
	unsigned int hp_val_l;
	unsigned int mic_val;
	unsigned int hp_detal;
	unsigned int hp_adc_ch;
	bool mic_det;
	int hp_disable;
	int timer_en;
	int detect_flag;
	struct work_struct work;
	struct mutex lock;
	struct snd_soc_jack jack;
	struct gpio_desc *hp_det_desc;

	struct pinctrl *pin_ctl;
	struct timer_list timer;
	struct gpio_desc *mute_desc;
	struct gpio_desc *av_mute_desc;
	struct clk *clk;
	int sleep_time;

	struct switch_dev sdev;	/* for android */
	struct switch_dev mic_sdev;	/* for android */
};

struct aml_audio_codec_info {
	const char *name;
	const char *status;
	struct device_node *p_node;
	unsigned i2c_bus_type;
	unsigned i2c_addr;
	unsigned id_reg;
	unsigned id_val;
	unsigned capless;
};

struct codec_info {
	char name[I2C_NAME_SIZE];
	char name_bus[I2C_NAME_SIZE];
};

struct codec_probe_priv {
	int num_eq;
	struct tas57xx_eq_cfg *eq_configs;
};

extern struct device *spdif_dev;
extern void aml_spdif_pinmux_init(struct device *pdev);
extern void aml_spdif_pinmux_deinit(struct device *pdev);

#endif
