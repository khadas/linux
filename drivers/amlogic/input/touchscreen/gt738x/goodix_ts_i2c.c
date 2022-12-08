// SPDX-License-Identifier: GPL-2.0-only
/*
 * Goodix I2C Module
 * Hardware interface layer of touchdriver architecture.
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "goodix_ts_core.h"
#include "goodix_cfg_bin.h"

#define TS_DRIVER_NAME			"gt738x"
#define I2C_MAX_TRANSFER_SIZE		256
#define TS_ADDR_LENGTH			2
#define TS_WAIT_CFG_READY_RETRY_TIMES	20
#define TS_WAIT_CMD_FREE_RETRY_TIMES	20

#define TS_REG_COORDS_BASE		0x824E
#define TS_REG_CMD			0x8040
#define TS_REG_VERSION			0x8240
#define TS_REG_CFG_BASE			0x8050
#define TS_REG_ESD_TICK_R		0x8040
#define TS_REG_STATIC_ESD		0x8043

#define REQUEST_HANDLED			0x00
#define REQUEST_CONFIG			0x01

#define COMMAND_SLEEP			0x05
#define COMMAND_START_SEND_CFG		0x80
#define COMMAND_END_SEND_CFG		0x83
#define COMMAND_SEND_CFG_PREPARE_OK	0x82
#define COMMAND_START_READ_CFG		0x86
#define COMMAND_READ_CFG_PREPARE_OK	0x85

#define TS_CMD_REG_READY                0xFF

#define BYTES_PER_COORD			8
#define BYTES_PEN_COORD			12
#define IRQ_HEAD_LEN			2
#define PEN_FLAG			0x20

#define PEN_HIGHER_PRIORITY

/* WARNING: externs should be avoided in .c files  move below to .h*/
/* void goodix_ts_core_init(struct work_struct *work); */
#ifdef CONFIG_OF
/**
 * goodix_parse_dt_resolution - parse resolution from dt
 * @node: devicetree node
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt_resolution(struct device_node *node,
		struct goodix_ts_board_data *board_data)
{
	int r, err;

	r = of_property_read_u32(node, "goodix,panel-max-x",
				 &board_data->panel_max_x);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-y",
				 &board_data->panel_max_y);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-w",
				 &board_data->panel_max_w);
	if (r)
		err = -ENOENT;

	board_data->swap_axis = of_property_read_bool(node,
					"goodix,swap-axis");
	board_data->x2x = of_property_read_bool(node, "goodix,x2x");
	board_data->y2y = of_property_read_bool(node, "goodix,y2y");

	return 0;
}

/**
 * goodix_parse_dt - parse board data from dt
 * @dev: pointer to device
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt(struct device_node *node,
	struct goodix_ts_board_data *board_data)
{
	struct property *prop;
	const char *name_tmp;
	int r;

	if (!board_data) {
		ts_err("invalid board data");
		return -EINVAL;
	}

	r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
	if (r < 0) {
		ts_err("invalid reset-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get reset-gpio[%d] from dt", r);
	board_data->reset_gpio = r;

	r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
	if (r < 0) {
		ts_err("invalid irq-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get irq-gpio[%d] from dt", r);
	board_data->irq_gpio = r;

	r = of_property_read_u32(node, "goodix,irq-flags",
			&board_data->irq_flags);
	if (r) {
		ts_err("invalid irq-flags");
		return -EINVAL;
	}

	memset(board_data->avdd_name, 0, sizeof(board_data->avdd_name));
	r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
	if (!r) {
		ts_info("avdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->avdd_name))
			strncpy(board_data->avdd_name,
				name_tmp, sizeof(board_data->avdd_name));
		else
			ts_info("invalid avdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->avdd_name));
	}
	r = of_property_read_u32(node, "goodix,power-on-delay-us",
				&board_data->power_on_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass a wrong value */
		if (board_data->power_on_delay_us > 1000 * 1000) {
			ts_err("Power on delay time exceed 1s, please check");
			board_data->power_on_delay_us = 0;
		}
	}

	r = of_property_read_u32(node, "goodix,power-off-delay-us",
				&board_data->power_off_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass */
		if (board_data->power_off_delay_us > 1000 * 1000) {
			ts_err("Power off delay time exceed 1s, please check");
			board_data->power_off_delay_us = 0;
		}
	}

	/* get xyz resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	if (r < 0) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}

	/* key map */
	prop = of_find_property(node, "goodix,panel-key-map", NULL);
	if (prop && prop->length) {
		if (prop->length / sizeof(u32) > GOODIX_MAX_TP_KEY) {
			ts_err("Size of panel-key-map is invalid");
			return r;
		}

		board_data->panel_max_key = prop->length / sizeof(u32);
		board_data->tp_key_num = prop->length / sizeof(u32);
		r = of_property_read_u32_array(node,
				"goodix,panel-key-map",
				&board_data->panel_key_map[0],
				board_data->panel_max_key);
		if (r) {
			ts_err("failed get key map, %d", r);
			return r;
		}
	}

	/*get pen-enable switch and pen keys, must after "key map"*/
	board_data->pen_enable = of_property_read_bool(node,
					"goodix,pen-enable");
	if (board_data->pen_enable)
		ts_info("goodix pen enabled");

	ts_info("***key:%d, %d, %d, %d",
		board_data->panel_key_map[0], board_data->panel_key_map[1],
		board_data->panel_key_map[2], board_data->panel_key_map[3]);

	ts_debug("[DT]x:%d, y:%d, w:%d, p:%d", board_data->panel_max_x,
		 board_data->panel_max_y, board_data->panel_max_w,
		 board_data->panel_max_p);
	return 0;
}
#endif

int goodix_i2c_test(struct goodix_ts_device *dev)
{
#define TEST_ADDR  0x8240
#define TEST_LEN   1
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned char test_buf[TEST_LEN + 1], addr_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = TS_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = &test_buf[0],
			.len = TEST_LEN,
		}
	};

	msgs[0].buf[0] = (TEST_ADDR >> 8) & 0xFF;
	msgs[0].buf[1] = TEST_ADDR & 0xFF;

	if (likely(i2c_transfer(client->adapter, msgs, 2) == 2))
		return 0;

	/* test failed */
	return -EINVAL;
}

/* confirm current device is goodix or not.
 * If confirmed 0 will return.
 */
int goodix_ts_dev_confirm(struct goodix_ts_device *ts_dev)
{
#define DEV_CONFIRM_RETRY 3
	int retry;

	for (retry = 0; retry < DEV_CONFIRM_RETRY; retry++) {
		gpio_direction_output(ts_dev->board_data.reset_gpio, 0);
		//udelay(10000);
		mdelay(10);
		gpio_direction_output(ts_dev->board_data.reset_gpio, 1);
		msleep(120);
		if (!goodix_i2c_test(ts_dev)) {
			msleep(320);
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * goodix_i2c_read - read device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, < 0 - i2c transfer error
 */
int goodix_i2c_read(struct goodix_ts_device *dev, unsigned int reg,
	unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int transfer_length = 0;
	unsigned int pos = 0, address = reg;
	unsigned char get_buf[64], addr_buf[2];
	int retry, r = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = TS_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	if (likely(len < sizeof(get_buf))) {
		/* code optimize, use stack memory */
		msgs[1].buf = &get_buf[0];
	} else {
		msgs[1].buf = kzalloc(len > I2C_MAX_TRANSFER_SIZE
				   ? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);
		if (!msgs[1].buf)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
			transfer_length = I2C_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;

		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
						msgs, 2) == 2)) {
				memcpy(&data[pos], msgs[1].buf,
				       transfer_length);
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_info("I2c read retry[%d]:0x%x", retry + 1, reg);
			msleep(20);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_err("I2c read failed, dev:%02x, reg:%04x, size:%u",
			       client->addr, reg, len);
			r = -EBUS;
			goto read_exit;
		}
	}

read_exit:
	if (unlikely(len >= sizeof(get_buf)))
		kfree(msgs[1].buf);
	return r;
}

/**
 * goodix_i2c_write - write device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transfer error.
 */
int goodix_i2c_write(struct goodix_ts_device *dev, unsigned int reg,
		unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = reg;
	unsigned char put_buf[64];
	int retry, r = 0;
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
	};

	if (likely(len + TS_ADDR_LENGTH < sizeof(put_buf))) {
		/* code optimize, use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(len + TS_ADDR_LENGTH > I2C_MAX_TRANSFER_SIZE
				? I2C_MAX_TRANSFER_SIZE : len + TS_ADDR_LENGTH,
				GFP_KERNEL);
		if (!msg.buf)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length = I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &data[pos], transfer_length);

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
						&msg, 1) == 1)) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_debug("I2c write retry[%d]", retry + 1);
			msleep(20);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_debug("I2c write failed, dev:%02x, reg:%04x, size:%u",
				client->addr, reg, len);
			r = -EBUS;
			goto write_exit;
		}
	}

write_exit:
	if (likely(len + TS_ADDR_LENGTH >= sizeof(put_buf)))
		kfree(msg.buf);
	return r;
}

/* goodix_i2c_read_nodelay - read device register through i2c bus, no delay
 * @dev: pointer to device data
 * @addr: register address
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, < 0 - i2c transfer error
 */
int goodix_i2c_read_nodelay(struct goodix_ts_device *dev, unsigned int reg,
	unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int transfer_length = 0;
	unsigned int pos = 0, address = reg;
	unsigned char get_buf[64], addr_buf[2];
	int retry, r = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = TS_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	if (likely(len < sizeof(get_buf))) {
		/* code optimize, use stack memory */
		msgs[1].buf = &get_buf[0];
	} else {
		msgs[1].buf = kzalloc(len > I2C_MAX_TRANSFER_SIZE
				   ? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);
		if (!msgs[1].buf)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
			transfer_length = I2C_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;

		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
						msgs, 2) == 2)) {
				memcpy(&data[pos], msgs[1].buf,
				       transfer_length);
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_info("I2c read retry[%d]:0x%x", retry + 1, reg);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_err("I2c read failed, dev:%02x, reg:%04x, size:%u",
			       client->addr, reg, len);
			r = -EBUS;
			goto read_exit;
		}
	}

read_exit:
	if (unlikely(len >= sizeof(get_buf)))
		kfree(msgs[1].buf);
	return r;
}

/**
 * goodix_i2c_write_nodelay - write device register through i2c bus, no delay
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transfer error.
 */
int goodix_i2c_write_nodelay(struct goodix_ts_device *dev, unsigned int reg,
		unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = reg;
	unsigned char put_buf[64];
	int retry, r = 0;
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
	};

	if (likely(len + TS_ADDR_LENGTH < sizeof(put_buf))) {
		/* code optimize, use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(len + TS_ADDR_LENGTH > I2C_MAX_TRANSFER_SIZE
				? I2C_MAX_TRANSFER_SIZE : len + TS_ADDR_LENGTH,
				GFP_KERNEL);
		if (!msg.buf)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length = I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &data[pos], transfer_length);

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
						&msg, 1) == 1)) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_debug("I2c write retry[%d]", retry + 1);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_debug("I2c write failed, dev:%02x, reg:%04x, size:%u",
				client->addr, reg, len);
			r = -EBUS;
			goto write_exit;
		}
	}

write_exit:
	if (likely(len + TS_ADDR_LENGTH >= sizeof(put_buf)))
		kfree(msg.buf);
	return r;
}

/**
 * goodix_i2c_write_once
 * write device register through i2c bus, no retry
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transfer error.
 */
int goodix_i2c_write_once(struct goodix_ts_device *dev, unsigned int reg,
		unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = reg;
	unsigned char put_buf[64];
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
	};

	if (likely(len + TS_ADDR_LENGTH < sizeof(put_buf))) {
		/* code optimize, use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(len + TS_ADDR_LENGTH > I2C_MAX_TRANSFER_SIZE
				? I2C_MAX_TRANSFER_SIZE : len + TS_ADDR_LENGTH,
				GFP_KERNEL);
		if (!msg.buf) {
			ts_err("Malloc failed");
			return -ENOMEM;
		}
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length = I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &data[pos], transfer_length);

		i2c_transfer(client->adapter, &msg, 1);
		pos += transfer_length;
		address += transfer_length;
	}

	if (likely(len + TS_ADDR_LENGTH >= sizeof(put_buf)))
		kfree(msg.buf);
	return 0;
}

static void goodix_cmds_init(struct goodix_ts_cmd *ts_cmd,
			     u8 cmds, u16 cmd_data, u32 reg_addr)
{
	ts_cmd->initialized = false;
	if (!reg_addr || !cmds)
		return;

	ts_cmd->cmd_reg = reg_addr;
	ts_cmd->length = 3;
	ts_cmd->cmds[0] = cmds;
	ts_cmd->cmds[1] = cmd_data & 0xFF;
	ts_cmd->cmds[2] = 0 - cmds - cmd_data;
	ts_cmd->initialized = true;
}

/**
 * goodix_send_command - send cmd to firmware
 *
 * @dev: pointer to device
 * @cmd: pointer to command struct which contain command data
 * Returns 0 - succeed, <0 - failed
 */
int goodix_send_command(struct goodix_ts_device *dev,
		struct goodix_ts_cmd *cmd)
{
	int ret;

	if (!cmd || !cmd->initialized)
		return -EINVAL;

	ret = goodix_i2c_write(dev, cmd->cmd_reg, cmd->cmds, cmd->length);

	return ret;
}

static int goodix_read_version(struct goodix_ts_device *dev,
		struct goodix_ts_version *version)
{
	u8 buffer[GOODIX_PID_MAX_LEN + 1];
	u8 temp_buf[256];
	u8 checksum = 0;
	u8 pid_read_len = dev->reg.pid_len;
	u8 vid_read_len = dev->reg.vid_len;
	u8 sensor_id_mask = dev->reg.sensor_id_mask;
	int r;

	if (!version) {
		ts_err("pointer of version is NULL");
		return -EINVAL;
	}
	version->valid = false;

	/*check reg info valid*/
	if (!dev->reg.pid || !dev->reg.sensor_id || !dev->reg.vid) {
		ts_err("reg is NULL, pid:0x%04x, vid:0x%04x, sensor_id:0x%04x",
			dev->reg.pid, dev->reg.vid, dev->reg.sensor_id);
		return -EINVAL;
	}
	if (!pid_read_len || pid_read_len > GOODIX_PID_MAX_LEN ||
	    !vid_read_len || vid_read_len > GOODIX_VID_MAX_LEN) {
		ts_err("invalid pid vid length, pid_len:%d, vid_len:%d",
			pid_read_len, vid_read_len);
		return -EINVAL;
	}

	/*check checksum*/
	if (dev->reg.version_base && dev->reg.version_len < sizeof(temp_buf)) {
		r = goodix_i2c_read(dev, dev->reg.version_base,
				temp_buf, dev->reg.version_len);
		if (r < 0) {
			ts_err("Read version base failed, reg:0x%02x, len:%d",
				dev->reg.version_base, dev->reg.version_len);
			goto exit;
		}

		checksum = checksum_u8(temp_buf, dev->reg.version_len);
		if (checksum) {
			ts_err("checksum error:0x%02x, base:0x%02x, len:%d",
			       checksum, dev->reg.version_base,
			       dev->reg.version_len);
			ts_err("%*ph", (int)(dev->reg.version_len / 2),
			       temp_buf);
			ts_err("%*ph", (int)(dev->reg.version_len -
						dev->reg.version_len / 2),
				&temp_buf[dev->reg.version_len / 2]);

			if (version)
				version->valid = false;
			r = -EINVAL;
			goto exit;
		}
	}

	/*read pid*/
	memset(buffer, 0, sizeof(buffer));
	memset(version->pid, 0, sizeof(version->pid));
	r = goodix_i2c_read(dev, dev->reg.pid, buffer, pid_read_len);
	if (r < 0) {
		ts_err("Read pid failed");
		if (version)
			version->valid = false;
		goto exit;
	}

	/* check pid is digit or not, current we only support digital pid */
	if (!isdigit(buffer[0]) || !isdigit(buffer[1])) {
		ts_err("pid not digit: 0x%x, 0x%x", buffer[0], buffer[1]);
		r = -EINVAL;
		goto exit;
	}

	memcpy(version->pid, buffer, pid_read_len);

	/*read vid*/
	memset(buffer, 0, sizeof(buffer));
	memset(version->vid, 0, sizeof(version->vid));
	r = goodix_i2c_read(dev, dev->reg.vid, buffer, vid_read_len);
	if (r < 0) {
		ts_err("Read vid failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	memcpy(version->vid, buffer, vid_read_len);

	/*read sensor_id*/
	memset(buffer, 0, sizeof(buffer));
	r = goodix_i2c_read(dev, dev->reg.sensor_id, buffer, 1);
	if (r < 0) {
		ts_err("Read sensor_id failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	if (sensor_id_mask != 0) {
		version->sensor_id = buffer[0] & sensor_id_mask;
		ts_info("sensor_id_mask:0x%02x, sensor_id:0x%02x",
			sensor_id_mask, version->sensor_id);
	} else {
		version->sensor_id = buffer[0];
	}

	version->valid = true;

	ts_info("PID:%s, SensorID:%d, VID:%*ph", version->pid,
		version->sensor_id, (int)sizeof(version->vid), version->vid);
exit:
	return r;
}

static int _do_goodix_send_config(struct goodix_ts_device *dev,
		struct goodix_ts_config *config)
{
	int ret = 0;
	int try_times = 0;
	u8 buf = 0;
	u16 command_reg = dev->reg.command;
	u16 cfg_reg = dev->reg.cfg_addr;
	struct goodix_ts_cmd ts_cmd;

	/*1. Inquire command_reg until it's free*/
	for (try_times = 0; try_times < GOODIX_RETRY_NUM_20; try_times++) {
		if (!goodix_i2c_read(dev, command_reg, &buf, 1) && buf == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= GOODIX_RETRY_NUM_20) {
		ts_err("Send cfg FAILED, before send, reg:0x%04x is not 0xff\n", command_reg);
		ret = -EINVAL;
		goto exit;
	}

	/*2. send "start write cfg" command*/
	goodix_cmds_init(&ts_cmd, COMMAND_START_SEND_CFG, 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err("Send cfg FAILED, send COMMAND_START_SEND_CFG ERROR\n");
		ret = -EINVAL;
		goto exit;
	}

	usleep_range(3000, 3100);

	/*3. wait ic set command_reg to 0x82*/
	for (try_times = 0; try_times < GOODIX_RETRY_NUM_20; try_times++) {
		if (!goodix_i2c_read(dev, command_reg, &buf, 1) &&
				buf == COMMAND_SEND_CFG_PREPARE_OK)
			break;
		usleep_range(3000, 3100);
	}
	if (try_times >= GOODIX_RETRY_NUM_20) {
		ts_err("Send cfg FAILED, reg:0x%04x is not 0x82\n", command_reg);
		ret = -EINVAL;
		goto exit;
	}

	/*4. write cfg*/
	if (goodix_i2c_write(dev, cfg_reg, config->data, config->length)) {
		ts_err("Send cfg FAILED, write cfg to fw ERROR\n");
		ret = -EINVAL;
		goto exit;
	}

	/*5. send "end send cfg" command*/
	goodix_cmds_init(&ts_cmd, COMMAND_END_SEND_CFG, 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err("Send cfg FAILED, send COMMAND_END_SEND_CFG ERROR\n");
		ret = -EINVAL;
		goto exit;
	}

	/*6. wait ic set command_reg to 0xff*/
	for (try_times = 0; try_times < GOODIX_RETRY_NUM_10; try_times++) {
		if (!goodix_i2c_read(dev, command_reg, &buf, 1) && buf == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= GOODIX_RETRY_NUM_10) {
		ts_err("Send cfg FAILED, after send, reg:0x%04x is not 0xff\n", command_reg);
		ret = -EINVAL;
		goto exit;
	}

	ts_info("Send cfg SUCCESS\n");
	ret = 0;

exit:
	return ret;
}

/**
 * goodix_check_cfg_valid - check config valid.
 * @cfg : config data pointer.
 * @length : config length.
 * Returns 0 - succeed, <0 - failed
 */
static int goodix_check_cfg_valid(struct goodix_ts_device *dev, u8 *cfg, u32 length)
{
	int ret = 0;
	u8 data_sum = 0;

	ts_info("%s run\n", __func__);

	if (!cfg || length == 0) {
		ts_err("%s: cfg is invalid , length:%d\n", __func__, length);
		ret = -EINVAL;
		goto exit;
	}
	data_sum  = checksum_u8(cfg, length);
	if (data_sum == 0) {
		ret = 0;
	} else {
		ts_err("cfg sum:0x%x\n", data_sum);
		ret = -EINVAL;
	}
exit:
	ts_info("%s exit, ret:%d\n", __func__, ret);
	return ret;
}

static int goodix_send_config(struct goodix_ts_device *dev,
		struct goodix_ts_config *config)
{
	int r = 0;

	if (!config || !config->initialized) {
		ts_err("invalid config data");
		return -EINVAL;
	}

	/*check configuration valid*/
	 r = goodix_check_cfg_valid(dev, config->data, config->length);
	if (r != 0) {
		ts_err("cfg check FAILED");
		return -EINVAL;
	}

	ts_info("ver:%02xh, size:%d", config->data[0], config->length);
	mutex_lock(&config->lock);

	r = _do_goodix_send_config(dev, config);

	if (r != 0)
		ts_err("send_cfg FAILED, ic_type:%d, cfg_len:%d",
		       dev->ic_type, config->length);
	mutex_unlock(&config->lock);
	return r;
}

/* success return config_len, <= 0 failed */
static int goodix_read_config(struct goodix_ts_device *dev,
			      u8 *config_data)
{
	struct goodix_ts_cmd ts_cmd;
	u8 cmd_flag;
	u32 cmd_reg = dev->reg.command;
	u16 cfg_reg = dev->reg.cfg_addr;
	u16 config_len = GOODIX_CFG_LEN;
	int r = 0;
	int i;

	if (!config_data) {
		ts_err("Illegal params");
		return -EINVAL;
	}
	if (!cmd_reg || !cfg_reg) {
		ts_err("cmd or cfg register ERROR:0x%04x, 0x%04x", cmd_reg, cfg_reg);
		return -EINVAL;
	}

	/* wait for IC in IDLE state */
	for (i = 0; i < TS_WAIT_CMD_FREE_RETRY_TIMES; i++) {
		cmd_flag = 0;
		r = goodix_i2c_read(dev, cmd_reg, &cmd_flag, 1);
		if (r < 0 || cmd_flag == TS_CMD_REG_READY)
			break;
		usleep_range(5000, 5200);
	}
	if (cmd_flag != TS_CMD_REG_READY) {
		ts_err("Wait for IC ready IDEL state timeout:addr 0x%x\n",
		       cmd_reg);
		r = -EAGAIN;
		goto exit;
	}
	/* 0x86 read config command */
	goodix_cmds_init(&ts_cmd, COMMAND_START_READ_CFG,
			 0, cmd_reg);
	r = goodix_send_command(dev, &ts_cmd);
	if (r) {
		ts_err("Failed send read config command");
		goto exit;
	}
	usleep_range(3000, 3100);
	/* wait for config data ready */
	for (i = 0; i < GOODIX_RETRY_NUM_20; i++) {
		cmd_flag = 0;
		r = goodix_i2c_read(dev, cmd_reg, &cmd_flag, 1);
		if (r < 0 || cmd_flag == COMMAND_READ_CFG_PREPARE_OK)
			break;
		usleep_range(3000, 3100);
	}
	if (cmd_flag != COMMAND_READ_CFG_PREPARE_OK) {
		ts_err("Wait for config data ready timeout\n");
		r = -EAGAIN;
		goto exit;
	}

	r = goodix_i2c_read(dev, cfg_reg, config_data, config_len);
	if (r < 0) {
		ts_err("Failed read config data\n");
		r = -EAGAIN;
		goto exit;
	}

	r = config_len;

	ts_info("success read config, len:%d\n", config_len);
	/* clear command */
	goodix_cmds_init(&ts_cmd, TS_CMD_REG_READY, 0, cmd_reg);
	goodix_send_command(dev, &ts_cmd);

exit:

	return r;
}

/**
 * goodix_init_esd
 *
 * @dev: pointer to touch device
 * Returns 0 - succeed, <0 - failed
 */
int goodix_init_esd(struct goodix_ts_device *dev)
{
	u8 data[2] = {0x00};
	int r = 0;

	// init static esd
	data[0] = GOODIX_ESD_TICK_WRITE_DATA;
	r = goodix_i2c_write(dev, TS_REG_STATIC_ESD, data, 1);
	if (r < 0)
		ts_err("static ESD init failed\n");

	/*init dynamic esd*/
	if (dev->reg.esd) {
		r = goodix_i2c_write(dev, dev->reg.esd, data, 1);
		if (r < 0)
			ts_err("IC reset, init dynamic esd FAILED");
	} else {
		ts_debug("reg.esd is NULL, skip dynamic esd init");
	}
	return 0;
}

/**
 * goodix_hw_reset - reset device
 *
 * @dev: pointer to touch device
 * Returns 0 - succeed, <0 - failed
 */
int goodix_hw_reset(struct goodix_ts_device *dev)
{
	ts_info("HW reset");

	gpio_direction_output(dev->board_data.reset_gpio, 0);
	//udelay(10000);
	mdelay(10);
	gpio_direction_output(dev->board_data.reset_gpio, 1);

	msleep(440);

	return goodix_init_esd(dev);
}

/**
 * goodix_request_handler - handle firmware request
 *
 * @dev: pointer to touch device
 * @request_data: request information
 * Returns 0 - succeed, <0 - failed
 */
static int goodix_request_handler(struct goodix_ts_device *dev)
{
	unsigned char buffer[1];
	int r;

	r = goodix_i2c_read(dev, dev->reg.fw_request, buffer, 1);
	if (r < 0)
		return r;

	switch (buffer[0]) {
	case REQUEST_CONFIG:
		ts_info("HW request config");
		r = goodix_send_config(dev, &dev->normal_cfg);
		if (r != 0)
			ts_info("request config, send config failed");
	break;
	default:
		ts_debug("Unknown hw request:%d", buffer[0]);
	break;
	}

	buffer[0] = 0x00;
	r = goodix_i2c_write(dev, dev->reg.fw_request, buffer, 1);
	return r;
}

static int goodix_remap_trace_id(u8 *coor_buf, u32 coor_buf_len, int touch_num)
{
	static u8 remap_array[20] = {0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff};
	int i, j;
	int offset = 0;
	bool need_leave = false;
	bool need_add = false;
	u8 temp_buf[BYTES_PER_COORD] = {0x00};
	u8 *small;
	bool need_swap = false;
	int max_touch_num = GOODIX_MAX_TOUCH;

	if (touch_num > GOODIX_MAX_TOUCH) {
		ts_info("touch num error, trace id no remap:%d", touch_num);
		return 0;
	}

	if (!coor_buf || coor_buf_len > (3 + BYTES_PER_COORD *
	    max_touch_num + 4)) {
		ts_info("touch data buff error, !coor_buf:%d, len:%d",
			!coor_buf, coor_buf_len);
		return 0;
	}

	/*clear and reset remap_array*/
	if (touch_num == 0) {
		for (i = 0; i < sizeof(remap_array); i++)
			remap_array[i] = 0xff;
		return 0;
	}

	/*scan remap_array, find and remove leave point*/
	for (i = 0; i < sizeof(remap_array); i++) {
		if (remap_array[i] == 0xff) {
			continue;
		} else {
			need_leave = true;
			offset = 0;
			for (j = 0; j < touch_num; j++) {
				if (remap_array[i] == coor_buf[offset]) {
					need_leave = false;
					break;
				}
				offset += BYTES_PER_COORD;
			}
			/*if (need_leave == true) {CHECK: Using comparison to true is error prone*/
			if (need_leave) {
				/*ts_info("---leave, trace id:%d:%d", remap_array[i], i);*/
				remap_array[i] = 0xff;
			}
		}
	}

	/*find and add new point*/
	offset = 0;
	for (i = 0; i < touch_num; i++) {
		need_add = true;
		for (j = 0; j < sizeof(remap_array); j++) {
			if (coor_buf[offset] == remap_array[j]) {
				need_add = false;
				break;
			}
		}
		/*if (need_add == true) { CHECK: Using comparison to true is error prone */
		if (need_add) {
			for (j = 0; j < sizeof(remap_array); j++) {
				if (remap_array[j] == 0xff) {
					remap_array[j] = coor_buf[offset];
					break;
				}
			}
		}
		offset += BYTES_PER_COORD;
	}

	/*remap trace id*/
	offset = 0;
	for (i = 0; i < touch_num; i++) {
		/*do not remap pen's trace ID*/
		if (coor_buf[offset] >= 0x80) {
			offset += BYTES_PER_COORD;
			continue;
		} else {
			for (j = 0; j < sizeof(remap_array); j++) {
				if (remap_array[j] == coor_buf[offset]) {
					/*ts_info("***remap, %d--->%d", coor_buf[offset], j);*/
					coor_buf[offset] = j;
					break;
				}
			}
			if (j >= sizeof(remap_array)) {
				ts_info("remap ERROR!!trace id:%d", coor_buf[offset]);
				ts_info("remap_array:%*ph",
						(int)sizeof(remap_array), remap_array);
			}
			offset += BYTES_PER_COORD;
		}
	}

	/*realign coor data by new trace ID*/
	for (i = 0; i < touch_num - 1; i++) {
		small = &coor_buf[BYTES_PER_COORD * i];
		need_swap = false;
		for (j = i + 1; j < touch_num; j++) {
			if (coor_buf[BYTES_PER_COORD * j] < *small) {
				need_swap = true;
				small = &coor_buf[BYTES_PER_COORD * j];
			}
		}
		/*swap*/
		if (need_swap) {
			memcpy(temp_buf, small, BYTES_PER_COORD);
			memcpy(small,
					&coor_buf[BYTES_PER_COORD * i],
					BYTES_PER_COORD);
			memcpy(&coor_buf[BYTES_PER_COORD * i],
					temp_buf,
					BYTES_PER_COORD);
		}
	}

	return 0;
}

static void goodix_swap_coords(struct goodix_ts_device *dev,
		unsigned int *coor_x, unsigned int *coor_y)
{
	unsigned int temp;
	struct goodix_ts_board_data *bdata = &dev->board_data;

	if (bdata->swap_axis) {
		temp = *coor_x;
		*coor_x = *coor_y;
		*coor_y = temp;
	}

	if (bdata->x2x)
		*coor_x = bdata->panel_max_x - *coor_x;
	if (bdata->y2y)
		*coor_y = bdata->panel_max_y - *coor_y;
}

#define GOODIX_KEY_STATE	0x10

static void goodix_parse_key(struct goodix_ts_device *dev,
	struct goodix_ts_event *ts_event,
	struct goodix_touch_data *touch_data, unsigned char *buf)
{
	static u8 pre_key_map;
	u8 cur_key_map = 0;
	int i;
	int touch_num = 0;

	touch_num = buf[1] & 0x0F;

	if (buf[1] & GOODIX_KEY_STATE) {
		/* have key */
		cur_key_map = buf[touch_num * BYTES_PER_COORD + 2] & 0x0F;

		/* pen flag*/
		if ((buf[1] & PEN_FLAG) == PEN_FLAG)
			cur_key_map = buf[touch_num * BYTES_PER_COORD + 2 + 4] & 0x0F;
		ts_debug("key touch, cur key map:0x%02x!\n", cur_key_map);

		for (i = 0; i < GOODIX_MAX_TP_KEY; i++) {
			if (cur_key_map & (1 << i)) {
				ts_event->event_type |= EVENT_TOUCH;
				touch_data->keys[i].status = TS_TOUCH;
				touch_data->keys[i].code =
					dev->board_data.panel_key_map[i];
			}
		}
	}

	/* process key release */
	for (i = 0; i < GOODIX_MAX_TP_KEY; i++) {
		if (cur_key_map & (1 << i) || !(pre_key_map & (1 << i)))
			continue;
		ts_event->event_type |= EVENT_TOUCH;
		touch_data->keys[i].status = TS_RELEASE;
		touch_data->keys[i].code = dev->board_data.panel_key_map[i];
	}
	pre_key_map = cur_key_map;
}

static void goodix_parse_finger(struct goodix_ts_device *dev,
	struct goodix_touch_data *touch_data, unsigned char *buf, int touch_num)
{
	unsigned int id = 0, x = 0, y = 0, w = 0;
	static u32 pre_finger_map;
	u32 cur_finger_map = 0;
	int touch_num_valid = 0;
	u8 *coor_data;
	int i;

	touch_num_valid = touch_num;
	coor_data = &buf[IRQ_HEAD_LEN];
	for (i = 0; i < touch_num; i++) {
		id = coor_data[0];
		if (id >= GOODIX_MAX_TOUCH) {
			ts_info("invalid finger id =%d", id);
			break;
		}
		x = le16_to_cpup((__be16 *)(coor_data + 1));
		y = le16_to_cpup((__be16 *)(coor_data + 3));
		w = le16_to_cpup((__be16 *)(coor_data + 5));

		goodix_swap_coords(dev, &x, &y);
		if (w != 0) {
			touch_data->coords[id].status = TS_TOUCH;
			cur_finger_map |= (1 << id);
		} else {
			touch_data->coords[id].status = TS_RELEASE;
			touch_num_valid--;
		}
		touch_data->coords[id].x = x;
		touch_data->coords[id].y = y;
		touch_data->coords[id].w = w;
		coor_data += BYTES_PER_COORD;
	}

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (cur_finger_map & (1 << i))
			continue;
		if (pre_finger_map & (1 << i))
			touch_data->coords[i].status = TS_RELEASE;
	}

	pre_finger_map = cur_finger_map;
	touch_data->touch_num = touch_num_valid;
}

static unsigned int goodix_pen_btn_code[] = {BTN_STYLUS, BTN_STYLUS2};
static void goodix_parse_pen(struct goodix_ts_device *dev,
	struct goodix_pen_data *pen_data, unsigned char *buf, int touch_num)
{
	unsigned int id = 0;
	static u8 pre_key_map;
	u8 cur_key_map = 0;
	u8 *coor_data;
	s16 x_angle = 0, y_angle = 0;
	int i;

	pen_data->coords.tool_type = BTN_TOOL_PEN;

	if (touch_num) {
		pen_data->coords.status = TS_TOUCH;
	} else {
		pen_data->coords.status = TS_RELEASE;
		cur_key_map = 0;
		goto PEN_KEY_PROCESS;
	}

	/*pen data */
	coor_data = &buf[IRQ_HEAD_LEN + BYTES_PER_COORD * (touch_num - 1)];
	id = coor_data[0] & 0x80;
	pen_data->coords.x = le16_to_cpup((__be16 *)(coor_data + 1));
	pen_data->coords.y = le16_to_cpup((__be16 *)(coor_data + 3));
	pen_data->coords.p = le16_to_cpup((__be16 *)(coor_data + 5));
	x_angle = le16_to_cpup((__le16 *)&coor_data[7]);
	y_angle = le16_to_cpup((__le16 *)&coor_data[9]);
	pen_data->coords.tilt_x = x_angle / 100;
	pen_data->coords.tilt_y = y_angle / 100;
	/* currently only support one stylus */

	/* process pen button */
	cur_key_map = (coor_data[0] >> 4) & 0x03;
PEN_KEY_PROCESS:
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		if (!(cur_key_map & (1 << i)))
			continue;
		pen_data->keys[i].status = TS_TOUCH;
		pen_data->keys[i].code = goodix_pen_btn_code[i];
	}
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		if (cur_key_map & (1 << i) || !(pre_key_map & (1 << i)))
			continue;
		pen_data->keys[i].status = TS_RELEASE;
		pen_data->keys[i].code = goodix_pen_btn_code[i];
	}
	pre_key_map = cur_key_map;
}

static int goodix_touch_handler(struct goodix_ts_device *dev,
		struct goodix_ts_event *ts_event,
		u8 *pre_buf, u32 pre_buf_len)
{
	struct goodix_touch_data *touch_data = &ts_event->touch_data;
	struct goodix_pen_data *pen_data = &ts_event->pen_data;
	static u8 buffer[IRQ_HEAD_LEN + BYTES_PER_COORD *
		(GOODIX_MAX_TOUCH - 1) + BYTES_PEN_COORD + 2];
	int touch_num = 0, r = -EINVAL;
	unsigned char chksum = 0;

	static u8 pre_finger_num;
	static u8 pre_pen_num;
	bool have_pen = false;
	int chksum_len = pre_buf_len;

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
	/* copy pre-data to buffer */
	memcpy(buffer, pre_buf, pre_buf_len);

	touch_num = buffer[1] & 0x0F;

	/* pen flag*/
	if ((buffer[1] & PEN_FLAG) == PEN_FLAG)
		have_pen = true;
	else
		chksum_len = pre_buf_len - BYTES_PEN_COORD + BYTES_PER_COORD;

	if (unlikely(touch_num > GOODIX_MAX_TOUCH)) {
		touch_num = -EINVAL;
		goto exit_clean_sta;
	}

	if (unlikely(touch_num > 1)) {
		chksum_len += BYTES_PER_COORD * (touch_num - 1);
		r = goodix_i2c_read_nodelay(dev,
				dev->reg.coor + pre_buf_len,
				&buffer[pre_buf_len],
				chksum_len - pre_buf_len);
		if (unlikely(r < 0))
			goto exit_clean_sta;
	}

	if (touch_num != 0) {
		chksum = checksum_u8(&buffer[1], chksum_len - 1);
		if (unlikely(chksum != 0)) {
			ts_info("checksum error:%X, ic_type:%d", chksum, dev->ic_type);
			r = -EINVAL;
			goto exit_clean_sta;
		}
	}

	if (have_pen)
		goodix_remap_trace_id(&buffer[IRQ_HEAD_LEN],
				      BYTES_PER_COORD * GOODIX_MAX_TOUCH,
				      touch_num - 1);
	else
		goodix_remap_trace_id(&buffer[IRQ_HEAD_LEN],
				BYTES_PER_COORD * GOODIX_MAX_TOUCH, touch_num);

	if (have_pen && touch_num == 1) {
#ifdef PEN_HIGHER_PRIORITY
		if (pre_finger_num) {
			ts_event->event_type |= EVENT_TOUCH;
			goodix_parse_finger(dev, touch_data,
				    buffer, 0);
			pre_finger_num = 0;
		} else {
			ts_event->event_type |= EVENT_PEN;
			goodix_parse_pen(dev, pen_data, buffer, touch_num);
			pre_pen_num = touch_num;
		}
	} else if (have_pen && (touch_num > 1)) {
		if (pre_finger_num) {
			ts_event->event_type |= EVENT_TOUCH;
			goodix_parse_finger(dev, touch_data,
				    buffer, 0);
			pre_finger_num = 0;
		} else {
			ts_event->event_type |= EVENT_PEN;
			goodix_parse_pen(dev, pen_data, buffer, touch_num);
			pre_pen_num = 1;
		}
#else
		ts_event->event_type |= EVENT_PEN;
		goodix_parse_pen(dev, pen_data, buffer, touch_num);
		pre_pen_num = touch_num;
	} else if (have_pen && (touch_num > 1)) {
		ts_event->event_type |= EVENT_TOUCH;
		goodix_parse_finger(dev, touch_data,
				    buffer, touch_num - 1);
		pre_finger_num = touch_num - 1;
		ts_event->event_type |= EVENT_PEN;
		goodix_parse_pen(dev, pen_data, buffer, touch_num);
		pre_pen_num = 1;
#endif
	} else if ((!have_pen) && touch_num > 0) {
		ts_event->event_type |= EVENT_TOUCH;
		goodix_parse_finger(dev, touch_data, buffer, touch_num);
		pre_finger_num = touch_num;

		if (pre_pen_num != 0) {
			ts_event->event_type |= EVENT_PEN;
			goodix_parse_pen(dev, pen_data, buffer, 0);
		}
		pre_pen_num = 0;
	} else {
		if (pre_pen_num != 0) {
			ts_event->event_type |= EVENT_PEN;
			goodix_parse_pen(dev, pen_data, buffer, 0);
		}

		if (pre_finger_num != 0) {
			ts_event->event_type |= EVENT_TOUCH;
			goodix_parse_finger(dev, touch_data,
				    buffer, 0);
		}
		pre_pen_num = 0;
		pre_finger_num = 0;
	}
	goodix_parse_key(dev, ts_event, touch_data, buffer);

exit_clean_sta:
	return r;
}

static int goodix_event_handler(struct goodix_ts_device *dev,
		struct goodix_ts_event *ts_event)
{
	int pre_read_len;
	u8 pre_buf[32];
	u8 event_sta;
	int r;

	pre_read_len = 2 + BYTES_PEN_COORD + 2;

	r = goodix_i2c_read_nodelay(dev, dev->reg.coor,
				  pre_buf, pre_read_len);
	if (unlikely(r < 0))
		return r;

	/* buffer[0]: event state */
	event_sta = pre_buf[0];
	if (likely((event_sta & GOODIX_TOUCH_EVENT) == GOODIX_TOUCH_EVENT)) {
		/* handle touch event */
		goodix_touch_handler(dev, ts_event, pre_buf,
						 pre_read_len);
	} else if ((event_sta & GOODIX_GESTURE_EVENT) ==
		   GOODIX_GESTURE_EVENT) {
		/* handle gesture event */
		ts_debug("Gesture event");
	} else if (!pre_buf[1]) {
		/* maybe request event */
		r = goodix_request_handler(dev);
	} else {
		ts_debug("unknown event type:0x%x", event_sta);
		r = -EINVAL;
	}

	return r;
}

/**
 * goodix_hw_suspend - Let touch device stay in lowpower mode.
 * @dev: pointer to goodix touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int goodix_hw_suspend(struct goodix_ts_device *dev)
{
	struct goodix_ts_cmd sleep_cmd;
	int r = 0;

	goodix_cmds_init(&sleep_cmd, COMMAND_SLEEP,
			 0, dev->reg.command);
	if (sleep_cmd.initialized) {
		r = goodix_send_command(dev, &sleep_cmd);
		if (!r)
			ts_info("Chip in sleep mode");
	} else {
		ts_err("Uninitialized sleep command");
	}
	return r;
}

/**
 * goodix_hw_resume - Let touch device stay in active  mode.
 * @dev: pointer to goodix touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int goodix_hw_resume(struct goodix_ts_device *dev)
{
	goodix_hw_reset(dev);

	return 0;
}

static int goodix_esd_check(struct goodix_ts_device *dev)
{
	int r;
	u8 data = 0;

	//static esd
	r = dev->hw_ops->read(dev, TS_REG_STATIC_ESD, &data, 1);
	if (r < 0 || data != GOODIX_ESD_TICK_WRITE_DATA) {
		ts_err("hw status:Error, static{0x8043}:%d\n", data);
		return -EIO;
	}

	/*check dynamic esd*/
	if (dev->reg.esd == 0) {
		ts_debug("dynamic esd reg is NULL(skip)");
		return 0;
	}
	r = dev->hw_ops->read(dev, TS_REG_ESD_TICK_R, &data, 1);

	if (r < 0 || data == GOODIX_ESD_TICK_WRITE_DATA) {
		ts_info("dynamic esd occur, r:%d, data:0x%02x", r, data);
		r = -EINVAL;
		goto exit;
	}
exit:
	return r;
}

/* hardware operation functions */
static const struct goodix_ts_hw_ops hw_i2c_ops = {
	.dev_confirm = goodix_ts_dev_confirm,
	.read = goodix_i2c_read,
	.write = goodix_i2c_write,
	.read_nodelay = goodix_i2c_read_nodelay,
	.write_nodelay = goodix_i2c_write_nodelay,
	.reset = goodix_hw_reset,
	.event_handler = goodix_event_handler,
	.send_config = goodix_send_config,
	.read_config = goodix_read_config,
	.send_cmd = goodix_send_command,
	.read_version = goodix_read_version,
	.suspend = goodix_hw_suspend,
	.resume = goodix_hw_resume,
	.init_esd = goodix_init_esd,
	.check_hw = goodix_esd_check,
};

static struct platform_device *goodix_pdev;

static void goodix_pdev_release(struct device *dev)
{
	ts_info("goodix pdev released");
}

static void goodix_core_init_work(void)
{
	static struct delayed_work core_init_wk;

	INIT_DELAYED_WORK(&core_init_wk, goodix_ts_core_init);
	schedule_delayed_work(&core_init_wk, 2 * HZ);
	ts_info("schedule delayed core init work");
}

static int goodix_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *dev_id)
{
	struct goodix_ts_device *ts_device = NULL;
	int r = 0;

	ts_info("%s IN", __func__);

	r = i2c_check_functionality(client->adapter,
		I2C_FUNC_I2C);
	if (!r)
		return -EIO;

	/* ts device data */
	ts_device = devm_kzalloc(&client->dev,
		sizeof(struct goodix_ts_device), GFP_KERNEL);
	if (!ts_device)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF) && client->dev.of_node) {
		/* parse devicetree property */
		r = goodix_parse_dt(client->dev.of_node,
				    &ts_device->board_data);
		if (r < 0) {
			ts_err("failed parse device info form dts, %d", r);
			return -EINVAL;
		}
	} else {
		ts_err("no valid device tree node found");
		return -ENODEV;
	}

	ts_device->name = "Goodix Touch Device";
	ts_device->dev = &client->dev;
	ts_device->hw_ops = &hw_i2c_ops;

	/* ts core device */
	goodix_pdev = kzalloc(sizeof(*goodix_pdev), GFP_KERNEL);
	if (!goodix_pdev)
		return -ENOMEM;

	goodix_pdev->name = GOODIX_CORE_DRIVER_NAME;
	goodix_pdev->id = 0;
	goodix_pdev->num_resources = 0;
	/*
	 * you can find this platform dev in
	 * /sys/devices/platform/goodix_ts.0
	 * goodix_pdev->dev.parent = &client->dev;
	 */
	goodix_pdev->dev.platform_data = ts_device;
	goodix_pdev->dev.release = goodix_pdev_release;

	/* register platform device, then the goodix_ts_core
	 * module will probe the touch device.
	 */
	r = platform_device_register(goodix_pdev);
	if (r) {
		ts_err("failed register goodix platform device, %d", r);
		goto err_pdev;
	}
	goodix_core_init_work();
	ts_info("i2c probe out");
	return r;

err_pdev:
	kfree(goodix_pdev);
	goodix_pdev = NULL;
	ts_info("i2c probe out, %d", r);
	return r;
}

static int goodix_i2c_remove(struct i2c_client *client)
{
	if (goodix_pdev) {
		platform_device_unregister(goodix_pdev);
		kfree(goodix_pdev);
		goodix_pdev = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2c_matches[] = {
	{.compatible = "goodix, gt738x", },
	{},
};
MODULE_DEVICE_TABLE(of, i2c_matches);
#endif

static const struct i2c_device_id i2c_id_table[] = {
	{TS_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, i2c_id_table);

static struct i2c_driver goodix_i2c_driver = {
	.driver = {
		.name = TS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(i2c_matches),
	},
	.probe = goodix_i2c_probe,
	.remove = goodix_i2c_remove,
	.id_table = i2c_id_table,
};

/* release manually when prob failed */
void goodix_ts_dev_release(void)
{
	if (goodix_pdev) {
		platform_device_unregister(goodix_pdev);
		kfree(goodix_pdev);
		goodix_pdev = NULL;
	}
	i2c_del_driver(&goodix_i2c_driver);
}

static int __init goodix_i2c_init(void)
{
	ts_info("Goodix driver init");
	return i2c_add_driver(&goodix_i2c_driver);
}

static void __exit goodix_i2c_exit(void)
{
	i2c_del_driver(&goodix_i2c_driver);
	ts_info("Goodix driver exit");
}

module_init(goodix_i2c_init);
module_exit(goodix_i2c_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Hardware Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
