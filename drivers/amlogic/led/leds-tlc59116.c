// SPDX-License-Identifier: GPL-2.0+
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * BSD LICENSE
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "leds-tlc59116.h"

#define MESON_TLC59116_I2C_NAME			"tlc59116_led"
#define MESON_LEDS_CDEV_NAME			"i2c_leds"
#define MESON_TLC59116_VERSION			"v2.0.0"

#define MESON_TLC59116_I2C_RETRIES		5
#define MESON_TLC59116_I2C_RETRY_DELAY		5
#define MESON_TLC59116_COLORS_COUNT		3
#define MESON_TLC59116_MAX_IO			16
#define MESON_TLC59116_BRI_MAX			0xFF

/* tlc59116 register addr */
#define MESON_TLC59116_REG_MODE_1		0x00
#define MESON_TLC59116_REG_MODE_2		0x01
#define MESON_TLC59116_REG_PWM_0		0x02
#define MESON_TLC59116_REG_PWM_1		0x03
#define MESON_TLC59116_REG_PWM_2		0x04
#define MESON_TLC59116_REG_PWM_3		0x05
#define MESON_TLC59116_REG_PWM_4		0x06
#define MESON_TLC59116_REG_PWM_5		0x07
#define MESON_TLC59116_REG_PWM_6		0x08
#define MESON_TLC59116_REG_PWM_7		0x09
#define MESON_TLC59116_REG_PWM_8		0x0a
#define MESON_TLC59116_REG_PWM_9		0x0b
#define MESON_TLC59116_REG_PWM_10		0x0c
#define MESON_TLC59116_REG_PWM_11		0x0d
#define MESON_TLC59116_REG_PWM_12		0x0e
#define MESON_TLC59116_REG_PWM_13		0x0f
#define MESON_TLC59116_REG_PWM_14		0x10
#define MESON_TLC59116_REG_PWM_15		0x11
#define MESON_TLC59116_REG_GRPPWM		0x12
#define MESON_TLC59116_REG_GRPFREQ		0x13
#define MESON_TLC59116_REG_LEDOUT0		0x14
#define MESON_TLC59116_REG_LEDOUT1		0x15
#define MESON_TLC59116_REG_LEDOUT2		0x16
#define MESON_TLC59116_REG_LEDOUT3		0x17
#define MESON_TLC59116_REG_SUBADR1		0x18
#define MESON_TLC59116_REG_SUBADR2		0x19
#define MESON_TLC59116_REG_SUBADR3		0x1a
#define MESON_TLC59116_REG_ALLCALLADR		0x1b
#define MESON_TLC59116_REG_IREF			0x1c
#define MESON_TLC59116_REG_EFLAG1		0x1d
#define MESON_TLC59116_REG_EFLAG2		0x1e
#define MESON_TLC59116_REG_MAX			0x1f

/* tlc59116 register read/write access*/
#define MESON_TLC59116_REG_NONE_ACCESS		0
#define MESON_TLC59116_REG_RD_ACCESS		BIT(0)
#define MESON_TLC59116_REG_WR_ACCESS		BIT(1)
#define MESON_TLC59116_REG_ALL_ACCESS		(BIT(0) | BIT(1))

static int meson_tlc59116_debug;
static DEFINE_MUTEX(meson_tlc59116_lock);

const unsigned char meson_tlc59116_reg_access[MESON_TLC59116_REG_MAX] = {
	[MESON_TLC59116_REG_MODE_1] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_MODE_2] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_0] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_1] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_2] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_3] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_4] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_5] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_6] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_7] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_8] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_9] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_10] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_11] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_12] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_13] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_14] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_PWM_15] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_GRPPWM] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_GRPFREQ] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_LEDOUT0] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_LEDOUT1] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_LEDOUT2] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_LEDOUT3] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_SUBADR1] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_SUBADR2] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_SUBADR3] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_ALLCALLADR] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_IREF] = MESON_TLC59116_REG_ALL_ACCESS,
	[MESON_TLC59116_REG_EFLAG1] = MESON_TLC59116_REG_RD_ACCESS,
	[MESON_TLC59116_REG_EFLAG2] = MESON_TLC59116_REG_RD_ACCESS,
};

static int meson_tlc59116_i2c_writes(struct i2c_client *client,
				     unsigned char sreg_addr,
				     u8 length, u8 *bufs)
{
	struct i2c_msg msg;
	int i, ret;
	u8 data[64];

	if (length >= MESON_TLC59116_MAX_IO)
		length = MESON_TLC59116_MAX_IO;

	data[0] = sreg_addr;
	for (i = 1; i <= length; i++)
		data[i] = bufs[i - 1];

	msg.addr = client->addr;
	msg.flags = !I2C_M_RD;
	msg.len = length + 1;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("[tlc59116_i2c_writes]error...\n");
		return ret;
	}

	return 0;
}

static int meson_tlc59116_i2c_write(struct meson_tlc59116 *tlc59116,
				    unsigned char reg_addr,
				    unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < MESON_TLC59116_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(tlc59116->i2c,
						reg_addr,
						reg_data);
		if (ret < 0)
			pr_err("%s: i2c_write cnt = %d error = %d\n",
			       __func__, cnt, ret);
		else
			break;

		cnt++;
		msleep(MESON_TLC59116_I2C_RETRY_DELAY);
	}

	return ret;
}

static int meson_tlc59116_i2c_read(struct meson_tlc59116 *tlc59116,
				   unsigned char reg_addr,
				   unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < MESON_TLC59116_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(tlc59116->i2c, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt = %d error = %d\n",
			       __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}

		cnt++;
		msleep(MESON_TLC59116_I2C_RETRY_DELAY);
	}

	return ret;
}

static int meson_tlc59116_set_colors(struct meson_tlc59116 *tlc59116)
{
	struct meson_tlc59116_colors *color;
	struct meson_tlc59116_io *io;
	u8 color_data[MESON_TLC59116_MAX_IO];
	unsigned int i;

	if (meson_tlc59116_debug) {
		pr_info("tlc59116 set colors: 0x%x 0x%x 0x%x 0x%x\n",
			tlc59116->colors_buf[0], tlc59116->colors_buf[1],
			tlc59116->colors_buf[2], tlc59116->colors_buf[3]);
	}

	memset(color_data, 0, sizeof(color_data));
	for (i = 0; i < tlc59116->led_counts; i++) {
		io = &tlc59116->io[i];
		color = &tlc59116->colors[i];
		color->blue = tlc59116->colors_buf[i] & 0xff;
		color_data[io->b_io] = color->blue;
		color->green = (tlc59116->colors_buf[i] >> 8) & 0xff;
		color_data[io->g_io] = color->green;
		color->red = (tlc59116->colors_buf[i] >> 16) & 0xff;
		color_data[io->r_io] = color->red;
	}

	return meson_tlc59116_i2c_writes(tlc59116->i2c,
					 (MESON_TLC59116_REG_PWM_0
					 | (0x5 << 5)), MESON_TLC59116_MAX_IO,
					 color_data);
}

static ssize_t colors_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_tlc59116 *tlc59116 = container_of(led_cdev,
						       struct meson_tlc59116,
						       cdev);
	int ret;

	mutex_lock(&meson_tlc59116_lock);
	ret = sscanf(buf, "%x %x %x %x", &tlc59116->colors_buf[0],
		     &tlc59116->colors_buf[1], &tlc59116->colors_buf[2],
		     &tlc59116->colors_buf[3]);
	if (ret != 4) {
		pr_info(" enter,Line:...set led colors fail!\n");
		return count;
	}

	meson_tlc59116_set_colors(tlc59116);
	mutex_unlock(&meson_tlc59116_lock);
	return count;
}

static ssize_t colors_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_tlc59116 *tlc59116 = container_of(led_cdev,
						       struct meson_tlc59116,
						       cdev);
	int i;

	for (i = 0; i < tlc59116->led_counts; i++) {
		tlc59116->colors_buf[i] |= tlc59116->colors[i].red << 16;
		tlc59116->colors_buf[i] |= tlc59116->colors[i].green << 8;
		tlc59116->colors_buf[i] |= tlc59116->colors[i].blue;
	}

	return sprintf(buf, "0x%x 0x%x 0x%x 0x%x\n", tlc59116->colors_buf[0],
		       tlc59116->colors_buf[1], tlc59116->colors_buf[2],
		       tlc59116->colors_buf[3]);
}

static ssize_t reg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_tlc59116 *tlc59116 = container_of(led_cdev,
						       struct meson_tlc59116,
						       cdev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= MESON_TLC59116_REG_MAX) {
			dev_err(tlc59116->dev, "%s error max register addr = 0x%x\n",
				__func__, MESON_TLC59116_REG_MAX);
			return count;
		}

		if (!(meson_tlc59116_reg_access[databuf[0]] &
						MESON_TLC59116_REG_WR_ACCESS)) {
			dev_err(tlc59116->dev, "%s error register addr = 0x%x is read only!\n",
				__func__, databuf[0]);
			return count;
		}

		meson_tlc59116_i2c_write(tlc59116, (unsigned char)databuf[0],
					 (unsigned char)databuf[1]);
	}
	return count;
}

static ssize_t reg_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_tlc59116 *tlc59116 = container_of(led_cdev,
						       struct meson_tlc59116,
						       cdev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < MESON_TLC59116_REG_MAX; i++) {
		if (!(meson_tlc59116_reg_access[i] &
		    MESON_TLC59116_REG_RD_ACCESS))
			continue;

		meson_tlc59116_i2c_read(tlc59116, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02x=0x%02x\n", i, reg_val);
	}

	return len;
}

static ssize_t debug_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	unsigned int debug = 0;

	if (!kstrtoint(buf, 10, &debug)) {
		meson_tlc59116_debug = debug;
		pr_info("meson tlc59116 debug %s\n",
			meson_tlc59116_debug ? "on" : "off");
	}

	return count;
}

static ssize_t debug_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "%d\n", meson_tlc59116_debug);
}

static ssize_t meson_tlc59116_io_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_tlc59116 *tlc59116 = container_of(led_cdev,
						       struct meson_tlc59116,
						       cdev);
	ssize_t len = 0;
	int i = 0;

	for (i = 0; i < tlc59116->led_counts; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"led[%d] R: %d  G: %d  B: %d\n", i,
				tlc59116->io[i].r_io, tlc59116->io[i].g_io,
				tlc59116->io[i].b_io);

	return len;
}

static int meson_tlc59116_set_singlecolors(u32 ledid, struct meson_tlc59116 *tlc59116)
{
	struct meson_tlc59116_colors *color;
	struct meson_tlc59116_io *io;
	u8 color_data[MESON_TLC59116_MAX_IO];

	if (ledid > 3) {
		pr_info(" Exceed the maximum number of leds!\n");
		return 0;
	}

	if (meson_tlc59116_debug) {
		pr_info("tlc59116 set colors: 0x%x\n",
				tlc59116->colors_buf[0]);
	}

	memset(color_data, 0, sizeof(color_data));

	switch (ledid) {
	case LED_NUM0:
		io = &tlc59116->io[0];
		color = &tlc59116->colors[0];
		color->blue = tlc59116->colors_buf[0] & 0xff;
		color_data[io->b_io] = color->blue;
		color->green = (tlc59116->colors_buf[0] >> 8) & 0xff;
		color_data[io->g_io] = color->green;
		color->red = (tlc59116->colors_buf[0] >> 16) & 0xff;
		color_data[io->r_io] = color->red;
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[0].b_io + REGISTER_OFFSET,
				color_data[io->b_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[0].g_io + REGISTER_OFFSET,
				color_data[io->g_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[0].r_io + REGISTER_OFFSET,
				color_data[io->r_io]);
		break;

	case LED_NUM1:
		io = &tlc59116->io[1];
		color = &tlc59116->colors[1];
		color->blue = tlc59116->colors_buf[0] & 0xff;
		color_data[io->b_io] = color->blue;
		color->green = (tlc59116->colors_buf[0] >> 8) & 0xff;
		color_data[io->g_io] = color->green;
		color->red = (tlc59116->colors_buf[0] >> 16) & 0xff;
		color_data[io->r_io] = color->red;
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[1].b_io + REGISTER_OFFSET,
				color_data[io->b_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[1].g_io + REGISTER_OFFSET,
				color_data[io->g_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[1].r_io + REGISTER_OFFSET,
				color_data[io->r_io]);
		break;

	case LED_NUM2:
		io = &tlc59116->io[2];
		color = &tlc59116->colors[2];
		color->blue = tlc59116->colors_buf[0] & 0xff;
		color_data[io->b_io] = color->blue;
		color->green = (tlc59116->colors_buf[0] >> 8) & 0xff;
		color_data[io->g_io] = color->green;
		color->red = (tlc59116->colors_buf[0] >> 16) & 0xff;
		color_data[io->r_io] = color->red;
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[2].b_io + REGISTER_OFFSET,
				color_data[io->b_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[2].g_io + REGISTER_OFFSET,
				color_data[io->g_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[2].r_io + REGISTER_OFFSET,
				color_data[io->r_io]);
		break;

	case LED_NUM3:
		io = &tlc59116->io[3];
		color = &tlc59116->colors[3];
		color->blue = tlc59116->colors_buf[0] & 0xff;
		color_data[io->b_io] = color->blue;
		color->green = (tlc59116->colors_buf[0] >> 8) & 0xff;
		color_data[io->g_io] = color->green;
		color->red = (tlc59116->colors_buf[0] >> 16) & 0xff;
		color_data[io->r_io] = color->red;
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[3].b_io + REGISTER_OFFSET,
				color_data[io->b_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[3].g_io + REGISTER_OFFSET,
				color_data[io->g_io]);
		meson_tlc59116_i2c_write(tlc59116, tlc59116->io[3].r_io + REGISTER_OFFSET,
				color_data[io->r_io]);
		break;
	}
	return 0;
}

static ssize_t single_colors_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	u32 ledid;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_tlc59116 *tlc59116 = container_of(led_cdev, struct meson_tlc59116, cdev);
	int ret;

	mutex_lock(&meson_tlc59116_lock);

	ret = sscanf(buf, "%d %x", &ledid, &tlc59116->colors_buf[0]);
		if (ret != 2) {
			pr_info(" enter,Line:...set led colors fail!\n");
			return count;
		}
	meson_tlc59116_set_singlecolors(ledid, tlc59116);
	mutex_unlock(&meson_tlc59116_lock);
	return count;
}

static DEVICE_ATTR_RW(colors);
static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(debug);
static DEVICE_ATTR_RO(meson_tlc59116_io);
static DEVICE_ATTR_WO(single_colors);

static struct attribute *tlc59116_attributes[] = {
	&dev_attr_colors.attr,
	&dev_attr_reg.attr,
	&dev_attr_debug.attr,
	&dev_attr_meson_tlc59116_io.attr,
	&dev_attr_single_colors.attr,
	NULL
};

static struct attribute_group tlc59116_attribute_group = {
	.attrs = tlc59116_attributes
};

static int meson_tlc59116_init(struct meson_tlc59116 *tlc59116)
{
	int i;

	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_MODE_1, 0x00);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_MODE_2, 0x00);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT0, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT1, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT2, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT3, 0xaa);

	for (i = 0; i < tlc59116->led_counts; i++) {
		tlc59116->colors_buf[i] |= tlc59116->colors[i].red << 16;
		tlc59116->colors_buf[i] |= tlc59116->colors[i].green << 8;
		tlc59116->colors_buf[i] |= tlc59116->colors[i].blue;
	}

	return meson_tlc59116_set_colors(tlc59116);
}

static int meson_tlc59116_check(struct meson_tlc59116 *tlc59116)
{
	int i;

	for (i = 0; i < tlc59116->led_counts; i++)
		if (tlc59116->io[i].r_io >= MESON_TLC59116_MAX_IO ||
		    tlc59116->io[i].g_io >= MESON_TLC59116_MAX_IO ||
		    tlc59116->io[i].b_io >= MESON_TLC59116_MAX_IO ||
		    tlc59116->colors[i].red > MESON_TLC59116_BRI_MAX ||
		    tlc59116->colors[i].green > MESON_TLC59116_BRI_MAX ||
		    tlc59116->colors[i].blue > MESON_TLC59116_BRI_MAX)
			return -EINVAL;

	return 0;
}

static int meson_tlc59116_init_data(struct meson_tlc59116 *tlc59116)
{
	int ret;

	ret = meson_tlc59116_check(tlc59116);
	if (ret) {
		dev_err(tlc59116->dev, "%s error tlc59116 check data fail!\n",
			__func__);
		return ret;
	}

	return meson_tlc59116_init(tlc59116);
}

static void tlc59116_set_brightness(struct led_classdev *cdev,
				    enum led_brightness brightness)
{
	struct meson_tlc59116 *tlc59116 = container_of(cdev,
						       struct meson_tlc59116,
						       cdev);

	tlc59116->cdev.brightness = brightness;
}

static int meson_tlc59116_parse_led_dt(struct meson_tlc59116 *tlc59116,
				       struct device_node *np)
{
	struct device_node *temp;
	int ret = -1;
	int i = 0;

	if (of_property_read_bool(np, "ignore-led-suspend"))
		tlc59116->ignore_led_suspend = 1;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_u32_array(temp, "default_colors",
						 (unsigned int *)
						 (tlc59116->colors + i),
						 MESON_TLC59116_COLORS_COUNT);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading default colors ret = %d\n",
				ret);
			return ret;
		}

		ret = of_property_read_u32(temp, "r_io_number",
					   &tlc59116->io[i].r_io);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading imax ret = %d\n", ret);
			return ret;
		}

		ret = of_property_read_u32(temp, "g_io_number",
					   &tlc59116->io[i].g_io);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading brightness ret = %d\n", ret);
			return ret;
		}

		ret = of_property_read_u32(temp, "b_io_number",
					   &tlc59116->io[i++].b_io);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading max brightness ret = %d\n",
				ret);
			return ret;
		}
	}

	return ret;
}

static int meson_tlc59116_i2c_probe(struct i2c_client *i2c,
				    const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	struct meson_tlc59116 *tlc59116;
	int ret;

	pr_info("%s enter,Line:%d version:%s\n",
		__func__, __LINE__, MESON_TLC59116_VERSION);

	if (!i2c_check_functionality(i2c->adapter,
				     I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL)) {
		dev_err(&i2c->dev, "check_functionality failed!\n");
		return -EIO;
	}

	tlc59116 = devm_kzalloc(&i2c->dev, sizeof(struct meson_tlc59116),
				GFP_KERNEL);
	if (!tlc59116)
		return -ENOMEM;

	tlc59116->dev = &i2c->dev;
	tlc59116->i2c = i2c;
	tlc59116->led_counts = device_get_child_node_count(&i2c->dev);
	tlc59116->io = devm_kzalloc(&i2c->dev, tlc59116->led_counts *
				    sizeof(struct meson_tlc59116_io),
				    GFP_KERNEL);
	if (!tlc59116->io)
		return -ENOMEM;

	tlc59116->colors = devm_kzalloc(&i2c->dev, tlc59116->led_counts *
					sizeof(struct meson_tlc59116_colors),
					GFP_KERNEL);
	if (!tlc59116->colors)
		return -ENOMEM;

	tlc59116->colors_buf = devm_kzalloc(&i2c->dev, tlc59116->led_counts *
					    sizeof(unsigned int),
					    GFP_KERNEL);
	if (!tlc59116->colors)
		return -ENOMEM;

	ret = meson_tlc59116_parse_led_dt(tlc59116, np);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s error tlc59116 parse led dt fail!\n",
			__func__);
		return ret;
	}

	ret = meson_tlc59116_init_data(tlc59116);
	if (ret) {
		dev_err(&i2c->dev, "%s error tlc59116 init led fail!\n",
			__func__);
		return ret;
	}

	i2c_set_clientdata(i2c, tlc59116);
	dev_set_drvdata(&i2c->dev, tlc59116);
	tlc59116->cdev.name = MESON_LEDS_CDEV_NAME;
	tlc59116->cdev.brightness = 0;
	tlc59116->cdev.max_brightness = 255;
	tlc59116->cdev.brightness_set = tlc59116_set_brightness;

	ret = led_classdev_register(tlc59116->dev, &tlc59116->cdev);
	if (ret) {
		dev_err(tlc59116->dev, "unable to register led! ret = %d\n",
			ret);
		return ret;
	}

	ret = sysfs_create_group(&tlc59116->cdev.dev->kobj,
				 &tlc59116_attribute_group);
	if (ret) {
		dev_err(tlc59116->dev, "unable to create led sysfs! ret = %d\n",
			ret);
		led_classdev_unregister(&tlc59116->cdev);
		return ret;
	}

	return 0;
}

static int meson_tlc59116_i2c_remove(struct i2c_client *i2c)
{
	struct meson_tlc59116 *tlc59116 = i2c_get_clientdata(i2c);

	pr_info("%s enter\n", __func__);

	sysfs_remove_group(&tlc59116->cdev.dev->kobj,
			   &tlc59116_attribute_group);
	led_classdev_unregister(&tlc59116->cdev);

	return 0;
}

static int meson_tlc59116_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct meson_tlc59116 *tlc59116 = i2c_get_clientdata(client);

	if (!tlc59116) {
		dev_err(dev, "tlc59116 is NULL!\n");
		return -ENXIO;
	}

	if (tlc59116->ignore_led_suspend)
		return 0;

	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT0, 0x0);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT1, 0x0);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT2, 0x0);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT3, 0x0);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_MODE_1, 0x10);

	return 0;
}

static int meson_tlc59116_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct meson_tlc59116 *tlc59116 = i2c_get_clientdata(client);

	if (!tlc59116) {
		dev_err(dev, "tlc59116 is NULL!\n");
		return -ENXIO;
	}

	if (tlc59116->ignore_led_suspend)
		return 0;

	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT0, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT1, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT2, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_LEDOUT3, 0xaa);
	meson_tlc59116_i2c_write(tlc59116, MESON_TLC59116_REG_MODE_1, 0x00);

	return 0;
}

static const struct i2c_device_id meson_tlc59116_i2c_id[] = {
	{ MESON_TLC59116_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, meson_tlc59116_i2c_id);

static const struct of_device_id meson_tlc59116_dt_match[] = {
	{ .compatible = "amlogic,tlc59116_led" },
	{ }
};

static SIMPLE_DEV_PM_OPS(meson_tlc59116_pm, meson_tlc59116_suspend, meson_tlc59116_resume);

static struct i2c_driver meson_tlc59116_driver = {
	.driver = {
		.name = MESON_TLC59116_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(meson_tlc59116_dt_match),
		.pm = &meson_tlc59116_pm,
	},
	.probe = meson_tlc59116_i2c_probe,
	.remove = meson_tlc59116_i2c_remove,
	.id_table = meson_tlc59116_i2c_id,
};

module_i2c_driver(meson_tlc59116_driver);
MODULE_AUTHOR("Bichao Zheng <bichao.zheng@amlogic.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TLC59116 LED Driver");
