// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
/*
 * leds-aw9523.c   aw9523 led module
 *
 * Version: 1.0.0
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include "leds-aw9523b.h"
#include <linux/amlogic/pm.h>
/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW9523_I2C_NAME "aw9523_led"

#define AW9523_VERSION "v2.0.0"

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 5
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 5
#define AW9523_COLORS_COUNT	3
#define AW9523_MAX_IO	15

#define REG_INPUT_P0        0x00
#define REG_INPUT_P1        0x01
#define REG_OUTPUT_P0       0x02
#define REG_OUTPUT_P1       0x03
#define REG_CONFIG_P0       0x04
#define REG_CONFIG_P1       0x05
#define REG_INT_P0          0x06
#define REG_INT_P1          0x07
#define REG_ID              0x10
#define REG_CTRL            0x11
#define REG_WORK_MODE_P0    0x12
#define REG_WORK_MODE_P1    0x13
#define REG_DIM00           0x20
#define REG_DIM01           0x21
#define REG_DIM02           0x22
#define REG_DIM03           0x23
#define REG_DIM04           0x24
#define REG_DIM05           0x25
#define REG_DIM06           0x26
#define REG_DIM07           0x27
#define REG_DIM08           0x28
#define REG_DIM09           0x29
#define REG_DIM10           0x2a
#define REG_DIM11           0x2b
#define REG_DIM12           0x2c
#define REG_DIM13           0x2d
#define REG_DIM14           0x2e
#define REG_DIM15           0x2f
#define REG_SWRST           0x7F

/* aw9523 register read/write access*/
#define REG_NONE_ACCESS                 0
#define REG_RD_ACCESS                   BIT(0)
#define REG_WR_ACCESS                   BIT(1)
#define AW9523_REG_MAX                  (0xFF)

const unsigned char aw9523_reg_access[AW9523_REG_MAX] = {
[REG_INPUT_P0] = REG_RD_ACCESS,
[REG_INPUT_P1] = REG_RD_ACCESS,
[REG_OUTPUT_P0] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_OUTPUT_P1] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_CONFIG_P0] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_CONFIG_P1] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_INT_P0] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_INT_P1] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_ID] = REG_RD_ACCESS,
[REG_CTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_WORK_MODE_P0] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_WORK_MODE_P1] = REG_RD_ACCESS | REG_WR_ACCESS,
[REG_DIM01] = REG_WR_ACCESS,
[REG_DIM00] = REG_WR_ACCESS,
[REG_DIM02] = REG_WR_ACCESS,
[REG_DIM03] = REG_WR_ACCESS,
[REG_DIM04] = REG_WR_ACCESS,
[REG_DIM05] = REG_WR_ACCESS,
[REG_DIM06] = REG_WR_ACCESS,
[REG_DIM07] = REG_WR_ACCESS,
[REG_DIM08] = REG_WR_ACCESS,
[REG_DIM09] = REG_WR_ACCESS,
[REG_DIM10] = REG_WR_ACCESS,
[REG_DIM11] = REG_WR_ACCESS,
[REG_DIM12] = REG_WR_ACCESS,
[REG_DIM13] = REG_WR_ACCESS,
[REG_DIM14] = REG_WR_ACCESS,
[REG_DIM15] = REG_WR_ACCESS,
[REG_SWRST] = REG_WR_ACCESS,
};

#define MAX_I2C_BUFFER_SIZE 65536

#define AW9523_ID 0x23

struct meson_aw9523_io {
	unsigned int r_io;
	unsigned int g_io;
	unsigned int b_io;
};

struct meson_aw9523_colors {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

struct meson_aw9523 {
	struct i2c_client *i2c;
	struct device *dev;
	struct led_classdev cdev;
	struct work_struct brightness_work;
	struct work_struct leds_work;
	struct workqueue_struct *leds_wq;
	int reset_gpio;
	unsigned char chipid;
	int imax;
	int platform;
	struct meson_aw9523_io *io;
	struct meson_aw9523_colors *colors;
	int led_counts;
};

struct meson_aw9523 *aw9523;

static int aw9523_i2c_write(struct meson_aw9523 *aw9523,
				unsigned char reg_addr,  unsigned char reg_data);
static int aw9523_hw_reset(struct meson_aw9523 *aw9523);

static int *all_data[32] = {0};//[2+16*N]: N command,each command consists of 16 bits
static int length_data[32] = {0};

static unsigned int state_mode;

/*2:S905X2 SEI520,  3:S905X3 SEI600TID; 4:S905X3 SEI610/SEI610F,  6:SEI610FPT,  8:SEI810CPR*/
static unsigned int platform_id;
static unsigned int hwen_id = 3,  isel_id = 3;

static unsigned int led_value;

static int time_val, cycles_val, instruct_val;

static u8 tmp_w[1024][16] = {};
static unsigned int tmp_color[5] = {};

static int aw9523_i2c_writes(struct i2c_client *client,
				unsigned char sreg_addr, uint8_t length, uint8_t *bufs)
{
	int ret = -1;
	struct i2c_msg msg;
	int i, retries = 0;
	u8 data[64];

	data[0] = sreg_addr;
	for (i = 1; i <= length; i++)
		data[i] = bufs[i - 1];
	msg.addr = client->addr;
	msg.flags = !I2C_M_RD;
	msg.len = length + 1;
	msg.buf = data;

	while (retries < 3) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			ret = 1;
			break;
		}
		retries++;
	}
	if (retries >= 3)
		pr_err("%s error...retyies = %d\n", __func__, retries);
	return ret;
}

static void leds_aw9523b_work_func(struct work_struct *work)
{
	int i, j;

	if (cycles_val == 0) {
		for (j = 0; j < instruct_val; j++) {
			aw9523_i2c_writes(aw9523->i2c, REG_DIM00, 16, tmp_w[j]);
			usleep_range(1000 * (time_val - 10), 1000 * (time_val + 10));
			if (state_mode == 0)
				break;
		}
		if (state_mode != 0)
			queue_work(aw9523->leds_wq, &aw9523->leds_work);
	} else {
		for (i = 1; i <= cycles_val; i++) {
			for (j = 0; j < instruct_val; j++) {
				aw9523_i2c_writes(aw9523->i2c, REG_DIM00, 16, tmp_w[j]);
				usleep_range(1000 * (time_val - 10), 1000 * (time_val + 10));
				if (state_mode == 0)
					break;
			}
			if (state_mode == 0)
				break;
		}
	}
}

static void init_leds_mode(int leds_mode)
{
	int offset, i, j;

	if (leds_mode > MAX_LED_MODE - 1) {
		dev_err(aw9523->dev, "%s erro,out of led mode range\n", __func__);
		return;
	}
	time_val = *(all_data[leds_mode] + 0);
	cycles_val = *((all_data[leds_mode]) + 1);
	instruct_val = (length_data[leds_mode] - 2) / 16;
	state_mode = 0;
	usleep_range(80000, 100000);
	for (i = 0; i < instruct_val; i++) {
		for (j = 0; j < 16; j++) {
			offset = 16 * i + j;
			tmp_w[i][j] = *((all_data[leds_mode]) + offset + 2);
		}
	}
	state_mode = leds_mode;
	queue_work(aw9523->leds_wq, &aw9523->leds_work);
}

static void meson_aw9523_set_colors(unsigned int *colors, unsigned int counts)
{
	struct meson_aw9523_colors *color;
	struct meson_aw9523_io *io;
	unsigned char color_data[AW9523_MAX_IO];
	unsigned int i;

	memset(color_data, 0, sizeof(color_data));
	for (i = 0; i < counts; i++) {
		io = &aw9523->io[i];
		color = &aw9523->colors[i];
		color->blue = colors[i] & 0xff;
		color->green = (colors[i] >> 8) & 0xff;
		color->red = (colors[i] >> 16) & 0xff;
		color_data[io->b_io] = color->blue;
		color_data[io->g_io] = color->green;
		color_data[io->r_io] = color->red;
	}

	aw9523_i2c_writes(aw9523->i2c, REG_DIM00, AW9523_MAX_IO, color_data);
}

static void init_leds_config(void)
{
	aw9523_hw_reset(aw9523);
	//aw9523_i2c_write(aw9523, 0x7F, 0x00);//0x7F  SW_RSTN
	/*0x12 P0 mode, <0x00: all PWM mode> */
	aw9523_i2c_write(aw9523, 0x12, 0x00);
	/*0x13 P1 mode, <0x00: all PWM mode, 0xF0: P1_7~P1_4 IO mode>*/
	aw9523_i2c_write(aw9523, 0x13, 0x00);
	/*
	 * GPOMD<0x00: Open-Drain,
	 * 0x01: Push-Pull> + ISEL <0x00:Imax,
	 * 0x11: 1/4-Imax,0x10: 2/4-Imax, 0x01: 3/4-Imax>
	 */
	aw9523_i2c_write(aw9523, 0x11, 0x11);
}

static void init_leds_data(void)
{
	all_data[0] = (int *)&data_mode0;
	all_data[1] = (int *)&data_mode1;
	all_data[2] = (int *)&data_mode2;
	all_data[3] = (int *)&data_mode3;
	all_data[4] = (int *)&data_mode4;
	all_data[5] = (int *)&data_mode5;
	all_data[6] = (int *)&data_mode6;
	all_data[7] = (int *)&data_mode7;
	all_data[8] = (int *)&data_mode8;
	all_data[9] = (int *)&data_mode9;
	all_data[10] = (int *)&data_mode10;
	all_data[11] = (int *)&data_mode11;
	length_data[0] = ARRAY_SIZE(data_mode0);
	length_data[1] = ARRAY_SIZE(data_mode1);
	length_data[2] = ARRAY_SIZE(data_mode2);
	length_data[3] = ARRAY_SIZE(data_mode3);
	length_data[4] = ARRAY_SIZE(data_mode4);
	length_data[5] = ARRAY_SIZE(data_mode5);
	length_data[6] = ARRAY_SIZE(data_mode6);
	length_data[7] = ARRAY_SIZE(data_mode7);
	length_data[8] = ARRAY_SIZE(data_mode8);
	length_data[9] = ARRAY_SIZE(data_mode9);
	length_data[10] = ARRAY_SIZE(data_mode10);
	length_data[11] = ARRAY_SIZE(data_mode11);

/*add X3 SEI600TID */
	all_data[12] = (int *)&data_mode12;
	length_data[12] = ARRAY_SIZE(data_mode12);
/*add X4 SEI810CPR */
	all_data[13] = (int *)&data_mode13;
	length_data[13] = ARRAY_SIZE(data_mode13);
	all_data[14] = (int *)&data_mode14;
	length_data[14] = ARRAY_SIZE(data_mode14);
	init_leds_config();
	if (platform_id == 3) {
		init_leds_mode(12);
		aw9523_i2c_write(aw9523, REG_OUTPUT_P1, 0x00);
	} else if (platform_id == 2) {
		init_leds_mode(1);
	} else if (platform_id == 4) {
		init_leds_mode(1);
	} else if (platform_id == 8) {
		init_leds_mode(13);
	} else {
		init_leds_mode(6);
	}
}

static int aw9523_i2c_write(struct meson_aw9523 *aw9523,
			unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw9523->i2c, reg_addr, reg_data);
		if (ret < 0)
			pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw9523_i2c_read(struct meson_aw9523 *aw9523,
		unsigned char reg_addr, unsigned int *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw9523->i2c, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static void aw9523_set_brightness(struct led_classdev *cdev,
				enum led_brightness brightness)
{
	struct meson_aw9523 *aw9523 = container_of(cdev, struct meson_aw9523, cdev);

	aw9523->cdev.brightness = brightness;
}

static int aw9523_parse_dt(struct device *dev, struct meson_aw9523 *aw9523,
				struct device_node *np)
{
	int ret;

	aw9523->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw9523->reset_gpio < 0) {
		dev_err(dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
		return -1;
	}
	if (gpio_is_valid(aw9523->reset_gpio)) {
		ret = devm_gpio_request(dev, aw9523->reset_gpio, "reset-gpio");
		if (ret) {
			dev_err(dev, "failed to request gpio\n");
			return ret;
		}
	}
	dev_info(dev, "%s: reset gpio provided ok\n", __func__);
	gpio_direction_output(aw9523->reset_gpio, 1);

	return 0;
}

static int aw9523_hw_reset(struct meson_aw9523 *aw9523)
{
	if (gpio_is_valid(aw9523->reset_gpio)) {
		gpio_set_value_cansleep(aw9523->reset_gpio, 0);
		msleep(20);
		gpio_set_value_cansleep(aw9523->reset_gpio, 1);
		msleep(20);
	} else {
		//aw9523_i2c_write(aw9523, 0x7F, 0x00);   //0x7F  SW_RSTN
		dev_err(aw9523->dev, "%s:  failed\n", __func__);
	}

	return 0;
}

static int aw9523_hw_off(struct meson_aw9523 *aw9523)
{
	pr_info("Tiger]%s enter,Line:%d\n", __func__, __LINE__);
	if (gpio_is_valid(aw9523->reset_gpio)) {
		gpio_set_value_cansleep(aw9523->reset_gpio, 0);
		msleep(20);
	} else {
		//aw9523_i2c_write(aw9523, 0x7F, 0x00);   //0x7F  SW_RSTN
		dev_err(aw9523->dev, "%s:  failed\n", __func__);
	}

	return 0;
}

static int aw9523_read_chipid(struct meson_aw9523 *aw9523)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned int reg_val = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw9523_i2c_read(aw9523, REG_ID, &reg_val);
		if (ret < 0) {
			dev_err(aw9523->dev,
				"%s: failed to read register aw9523_REG_ID: %d\n",
				__func__, ret);
			return -EIO;
		}
		switch (reg_val) {
		case AW9523_ID:
			pr_info("Tiger]%s aw9523 detected\n", __func__);
			aw9523->chipid = AW9523_ID;
			return 0;
		default:
			break;
		}
		cnt++;
		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}
	return -EINVAL;
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int state_mode;
	int ret;

	cancel_work_sync(&aw9523->leds_work);
	flush_workqueue(aw9523->leds_wq);
	ret = kstrtouint(buf, 10, &state_mode);
	if (ret)
		dev_err(dev, "%s error\n", __func__);
	pr_info("Tiger]%s enter,Line:%d...state_mode=%d\n", __func__, __LINE__, state_mode);
	init_leds_mode(state_mode);

	return count;
}

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "current mode:%d\n", state_mode);

	return len;
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw9523_i2c_write(aw9523, (unsigned char)databuf[0], (unsigned char)databuf[1]);

	return count;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned int reg_val = 0;

	for (i = 0; i < AW9523_REG_MAX; i++) {
		if (!(aw9523_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw9523_i2c_read(aw9523, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}

	return len;
}

static ssize_t hwen_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	unsigned int databuf[1] = {0};

	if (kstrtoint(buf, 16, &databuf[0]) == 0) {
		if (databuf[0] == 1)
			aw9523_hw_reset(aw9523);
		else if (databuf[0] == 2)//sw-reset
			aw9523_i2c_write(aw9523, 0x7F, 0x00);//0x7F  SW_RSTN
		else if (databuf[0] == 3)//sw-init
			init_leds_data();
		else
			aw9523_hw_off(aw9523);
		hwen_id = databuf[0];
	}

	return count;
}

static ssize_t hwen_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "hwen=%d\n", hwen_id);

	return len;
}

static ssize_t isel_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	unsigned int databuf[1] = {0};

	if (kstrtoint(buf, 16, &databuf[0]) == 0) {
		isel_id = databuf[0];
		aw9523_i2c_write(aw9523, 0x11, databuf[0]);
	}

	return count;
}

static ssize_t isel_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "isel=%d\n", isel_id);

	return len;
}

static ssize_t colors_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev,
						 struct meson_aw9523, cdev);
	unsigned int colors[4];
	int ret;

	ret = sscanf(buf, "%x %x %x %x", &colors[0], &colors[1], &colors[2], &colors[3]);
	if (ret != 4)
		dev_err(dev, " enter,Line:...set led colors fail ret: %d\n", ret);
	if (ret == 4) {
		meson_aw9523_set_colors(colors, aw9523->led_counts);
		tmp_color[0] = colors[0];
		tmp_color[1] = colors[1];
		tmp_color[2] = colors[2];
		tmp_color[3] = colors[3];
	} else if (ret == 2) {    //for android 11, param: index, color
		if (colors[0] < aw9523->led_counts)
			tmp_color[colors[0]] = colors[1];
		meson_aw9523_set_colors(tmp_color, aw9523->led_counts);
	}

	return count;
}

static ssize_t colors_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "current mode:%d\n", state_mode);
	return len;
}

static ssize_t led1_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	unsigned int databuf[1] = {0};

	if (kstrtoint(buf, 16, &databuf[0]) == 0) {
		if (databuf[0] >= 1)
			databuf[0] = 1;
		if (platform_id == 3)
			databuf[0] = !databuf[0];
		aw9523_i2c_read(aw9523, REG_OUTPUT_P1, &led_value);
		led_value = ((led_value & 0xE0) | (databuf[0] << 4));
		dev_info(dev, "Tiger] databuf=%x, led_value: %d\n\n", databuf[0], led_value);
		aw9523_i2c_write(aw9523, REG_OUTPUT_P1, led_value);
	}

	return count;
}

static ssize_t led1_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t len = 0;

	aw9523_i2c_read(aw9523, REG_OUTPUT_P1, &led_value);
	led_value = ((led_value & 0x10) >> 4);
	if (platform_id == 3)
		led_value = !led_value;

	len += snprintf(buf + len, PAGE_SIZE - len, "led1_value=%d\n", led_value);

	return len;
}

static ssize_t led2_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	unsigned int databuf[1] = {0};

	if (kstrtoint(buf, 16, &databuf[0]) == 0) {
		if (databuf[0] >= 1)
			databuf[0] = 1;
		if (platform_id == 3)
			databuf[0] = !databuf[0];
		aw9523_i2c_read(aw9523, REG_OUTPUT_P1, &led_value);
		led_value = ((led_value & 0xD0) | (databuf[0] << 5));
		dev_info(dev, "Tiger] databuf=%x, led_value: %d\n\n", databuf[0], led_value);
		aw9523_i2c_write(aw9523, REG_OUTPUT_P1, led_value);
	}

	return count;
}

static ssize_t led2_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t len = 0;

	aw9523_i2c_read(aw9523, REG_OUTPUT_P1, &led_value);
	led_value = ((led_value & 0x20) >> 5);

	if (platform_id == 3)
		led_value = !led_value;

	len += snprintf(buf + len, PAGE_SIZE - len, "led2_value=%d\n", led_value);

	return len;
}

static ssize_t led3_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw9523 *aw9523 = container_of(led_cdev, struct meson_aw9523, cdev);
	unsigned int databuf[1] = {0};

	if (kstrtoint(buf, 16, &databuf[0]) == 0) {
		if (databuf[0] >= 1)
			databuf[0] = 1;
		if (platform_id == 3)
			databuf[0] = !databuf[0];
		aw9523_i2c_read(aw9523, REG_OUTPUT_P1, &led_value);
		led_value = ((led_value & 0xB0) | (databuf[0] << 6));
		dev_info(dev, "Tiger] databuf=%x, led_value: %d\n\n", databuf[0], led_value);
		aw9523_i2c_write(aw9523, REG_OUTPUT_P1, led_value);
	}

	return count;
}

static ssize_t led3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	aw9523_i2c_read(aw9523, REG_OUTPUT_P1, &led_value);
	led_value = ((led_value & 0x40) >> 6);

	if (platform_id == 3)
		led_value = !led_value;

	len += snprintf(buf + len, PAGE_SIZE - len, "led3_value=%d\n", led_value);

	return len;
}

static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(hwen);
static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RW(isel);
static DEVICE_ATTR_RW(colors);

static DEVICE_ATTR_RW(led1);
static DEVICE_ATTR_RW(led2);
static DEVICE_ATTR_RW(led3);

static struct attribute *aw9523_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_hwen.attr,
	&dev_attr_mode.attr,
	&dev_attr_isel.attr,
	&dev_attr_colors.attr,
	&dev_attr_led1.attr,
	&dev_attr_led2.attr,
	&dev_attr_led3.attr,
	NULL
};

static struct attribute_group aw9523_attribute_group = {
	.attrs = aw9523_attributes
};

/*[SEI-Tiger.Hu-2020/05/15]add for aw9523b early suspend & resume { */
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void aw9523b_early_suspend(struct early_suspend *h)
{
	if (platform_id == 3) {
		init_leds_mode(0);
		aw9523_i2c_write(aw9523, REG_OUTPUT_P1, 0xF0);
	} else if (platform_id == 2) {
		init_leds_mode(6);
	} else if (platform_id == 8) {
		init_leds_mode(14);
	}
	pr_info("Tiger]aw9523b early suspend\n");
}

static void aw9523b_late_resume(struct early_suspend *h)
{
	if (platform_id == 3) {
		init_leds_mode(12);
		aw9523_i2c_write(aw9523, REG_OUTPUT_P1, 0x00);
	} else if (platform_id == 2) {
		init_leds_mode(11);
	} else if (platform_id == 8) {
		meson_aw9523_set_colors(tmp_color, aw9523->led_counts);
	}

	pr_info("Tiger]aw9523b early resume\n");
}

static struct early_suspend aw9523b_early_suspend_handler;
#endif

static int aw9523_parse_led_cdev(struct meson_aw9523 *aw9523,
				   struct device_node *np)
{
	struct device_node *temp;
	unsigned int colors[AW9523_COLORS_COUNT];
	int ret = -1;
	int i = 0;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_u32_array(temp, "default_colors",
						colors, AW9523_COLORS_COUNT);
		if (ret < 0) {
			dev_err(aw9523->dev,
				"Failure reading default colors ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32(temp, "r_io_number", &aw9523->io[i].r_io);
		if (ret < 0) {
			dev_err(aw9523->dev,
				"Failure reading imax ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "g_io_number", &aw9523->io[i].g_io);
		if (ret < 0) {
			dev_err(aw9523->dev,
				"Failure reading brightness ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "b_io_number", &aw9523->io[i].b_io);
		if (ret < 0) {
			dev_err(aw9523->dev,
				"Failure reading max brightness ret = %d\n",
				ret);
			goto free_pdata;
		}

		aw9523->colors[i].red = colors[0];
		aw9523->colors[i].green = colors[1];
		aw9523->colors[i].blue = colors[2];
		i++;
	}
	ret = of_property_read_u32(np, "platform", &platform_id);
	if (ret < 0)
		dev_err(aw9523->dev, "Tiger] %s:%d default platform_id=%d\n", __func__,
				__LINE__, platform_id);
	dev_info(aw9523->dev, "Tiger] %s:%d set platform_id=%d\n", __func__, __LINE__, platform_id);
	aw9523->cdev.name = AW9523_I2C_NAME;
	aw9523->cdev.brightness = 0;
	aw9523->cdev.max_brightness = 255;
	aw9523->cdev.brightness_set = aw9523_set_brightness;
	ret = led_classdev_register(aw9523->dev, &aw9523->cdev);
	if (ret) {
		dev_err(aw9523->dev,
			"unable to register led ret=%d\n", ret);
		goto free_pdata;
	}
	ret = sysfs_create_group(&aw9523->cdev.dev->kobj,
			&aw9523_attribute_group);
	if (ret) {
		dev_err(aw9523->dev, "led sysfs ret: %d\n", ret);
		goto free_class;
	}

	return 0;

free_class:
	led_classdev_unregister(&aw9523->cdev);
free_pdata:
	return ret;
}

static int aw9523_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	int ret;

	pr_info("Tiger]%s enter,Line:%d\n", __func__, __LINE__);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw9523 = devm_kzalloc(&i2c->dev, sizeof(struct meson_aw9523), GFP_KERNEL);
	if (!aw9523)
		return -ENOMEM;
	aw9523_parse_dt(&i2c->dev, aw9523, np);
	aw9523->dev = &i2c->dev;
	aw9523->i2c = i2c;
	aw9523->led_counts = device_get_child_node_count(&i2c->dev);
	aw9523->io = devm_kcalloc(&i2c->dev, aw9523->led_counts, sizeof(struct meson_aw9523_io),
				GFP_KERNEL);
	if (!aw9523->io)
		return -ENOMEM;

	aw9523->colors = devm_kcalloc(&i2c->dev, aw9523->led_counts,
					sizeof(struct meson_aw9523_colors), GFP_KERNEL);
	if (!aw9523->colors)
		return -ENOMEM;
	i2c_set_clientdata(i2c, aw9523);
	/* aw9523 chip id */
	ret = aw9523_read_chipid(aw9523);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: aw9523_read_chipid failed ret=%d\n", __func__, ret);
		goto err_id;
	}
	dev_set_drvdata(&i2c->dev, aw9523);
	ret = aw9523_parse_led_cdev(aw9523, np);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s error creating led class dev\n", __func__);
		goto err_sysfs;
	}
	INIT_WORK(&aw9523->leds_work, leds_aw9523b_work_func);
	aw9523->leds_wq = create_singlethread_workqueue("leds_wq");

	if (!aw9523->leds_wq) {
		dev_err(&i2c->dev, "create leds_work->input_wq fail!\n");
		return -ENOMEM;
	}
	queue_work(aw9523->leds_wq, &aw9523->leds_work);
/*[SEI-Tiger.Hu-2020/05/15]add for aw9523b early suspend&resume */
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	aw9523b_early_suspend_handler.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
	aw9523b_early_suspend_handler.suspend = aw9523b_early_suspend;
	aw9523b_early_suspend_handler.resume = aw9523b_late_resume;
	aw9523b_early_suspend_handler.param = aw9523;
	register_early_suspend(&aw9523b_early_suspend_handler);
#endif
/*[SEI-Tiger.Hu-2020/05/15]add for aw9523b early suspend&resume  */
	init_leds_data();
	pr_info("Tiger]%s exit,Line:%d\n", __func__, __LINE__);

	return 0;

err_sysfs:
err_id:
	return ret;
}

static int aw9523_i2c_remove(struct i2c_client *i2c)
{
	struct meson_aw9523 *aw9523 = i2c_get_clientdata(i2c);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&aw9523b_early_suspend_handler);
#endif
	sysfs_remove_group(&aw9523->cdev.dev->kobj,
			&aw9523_attribute_group);
	led_classdev_unregister(&aw9523->cdev);
	aw9523_hw_reset(aw9523);
	cancel_work_sync(&aw9523->leds_work);
	destroy_workqueue(aw9523->leds_wq);

	return 0;
}

static const struct i2c_device_id aw9523_i2c_id[] = {
	{ AW9523_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw9523_i2c_id);

static const struct of_device_id aw9523_dt_match[] = {
	{ .compatible = "amlogic,aw9523b_led" },
	{ },
};

static struct i2c_driver meson_aw9523_driver = {
	.driver = {
		.name = AW9523_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw9523_dt_match),
	},
	.probe = aw9523_i2c_probe,
	.remove = aw9523_i2c_remove,
	.id_table = aw9523_i2c_id,
};

module_i2c_driver(meson_aw9523_driver);

MODULE_AUTHOR("Tiger Hu <huyh@seirobotics.net>");
MODULE_DESCRIPTION("AW9523 LED Driver");
MODULE_LICENSE("GPL v2");
