/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include <asm/io.h>
#include "system_log.h"
#include "system_i2c.h"
#include "acamera_logger.h"
#include <linux/jiffies.h>

#define FPGA_IIC_MEM_SIZE 0x1000
#define FPGA_IIC_TX_FIFO 0x108
#define FPGA_IIC_RX_FIFO_PIRQ 0x120
#define FPGA_IIC_CR 0x100
#define FPGA_IIC_SR 0x104
#define FPGA_IIC_RX_FIFO 0x10c
#define FPGA_IIC_RX_FIFO_OCY 0x118
#define FPGA_IIC_ISR 0x20
#define FPGA_IIC_IER 0x28

#define RX_EMPTY_SHIFT 6
#define TX_EMPTY_SHIFT 7
#define BUSY_BUS_SHIFT 2

#define FPGA_IIC_TIMEOUT_HZ ( HZ / 10 )
#define FPGA_IIC_MASTERS_MAX ( 2 * FIRMWARE_CONTEXT_NUMBER )

typedef struct _iic_master_t {
    uint32_t phy_addr;
    void *virt_addr;
} iic_master_t;

static struct arm_i2c_sensor_ctrl *g_sensor_ctrl = NULL;

static const struct i2c_device_id otp_i2c_id[] = {
	{ARM_I2C_OTP_NAME, 0},
	{ }
};

static const struct of_device_id otp_i2c_dt_match[] = {
	{.compatible = ARM_I2C_OTP_NAME},
	{}
};

static int otp_i2c_probe(struct i2c_client *client,
									const struct i2c_device_id *dev_id)
{
	int rtn = 0;
	struct arm_i2c_sensor_ctrl *sensor_ctrl = NULL;

	pr_err("%s: start to probe\n", __func__);

	if (client == NULL) {
		pr_err("%s: client is null\n", __func__);
		return -EINVAL;
	}

	rtn = of_device_is_compatible(client->dev.of_node, ARM_I2C_OTP_NAME);

	if (rtn == 0) {
		pr_err("%s:failed to match compatible\n", __func__);
		return -EINVAL;
	}

	sensor_ctrl = kzalloc(sizeof(*sensor_ctrl), GFP_KERNEL);

	if (sensor_ctrl == NULL) {
		pr_err("%s:failed to malloc isp_ctrl\n", __func__);
		return -EINVAL;
	}

	i2c_set_clientdata(client, sensor_ctrl);

	sensor_ctrl->client = client;
	sensor_ctrl->of_node = client->dev.of_node;

	rtn = of_property_read_u32(sensor_ctrl->of_node, "slave-addr",
								&sensor_ctrl->slave_addr);
	//pr_info("%s:arm isp slave addr 0x%x, rtn %d\n", __func__, sensor_ctrl->slave_addr, rtn);

	if (rtn != 0) {
		pr_err("%s: failed to get isp slave addr\n", __func__);
		goto error;
	}

	rtn = of_property_read_u32(sensor_ctrl->of_node, "reg-type",
								&sensor_ctrl->reg_addr_type);
	//pr_info("%s:arm reg_addr_type %d, rtn %d\n", __func__, sensor_ctrl->reg_addr_type, rtn);

	rtn = of_property_read_u32(sensor_ctrl->of_node, "reg-data-type",
									&sensor_ctrl->reg_data_type);
	//pr_info("%s:arm reg_data_type %d, rtn %d\n", __func__, sensor_ctrl->reg_data_type, rtn);

	g_sensor_ctrl = sensor_ctrl;

	return rtn;

error:
	kfree(sensor_ctrl);
	sensor_ctrl = NULL;

	g_sensor_ctrl = sensor_ctrl;

	return rtn;
}


static int otp_i2c_remove(struct i2c_client *client)
{
	struct arm_i2c_sensor_ctrl *s_ctrl = NULL;

	s_ctrl = i2c_get_clientdata(client);

	if (s_ctrl == NULL) {
		pr_err("%s: Error client data is NULL\n", __func__);
		return -EINVAL;
	}

	kfree(s_ctrl);
	s_ctrl = NULL;
	g_sensor_ctrl = s_ctrl;

	pr_info("%s: remove i2c sensor\n", __func__);

	return 0;
}


static struct i2c_driver otp__i2c_driver = {
	.id_table = otp_i2c_id,
	.probe  = otp_i2c_probe,
	.remove = otp_i2c_remove,
	.driver = {
		.name = "arm, i2c-otp",
		.owner = THIS_MODULE,
		.of_match_table = otp_i2c_dt_match,
	},
};

void system_i2c_init( uint32_t bus )
{
	int rtn = -1;

	if (g_sensor_ctrl != NULL) {
		pr_err("%s:isp i2c bus has been inited\n", __func__);
	    return;
	}

	rtn = i2c_add_driver(&otp__i2c_driver);

	if (rtn != 0)
		pr_err("%s:failed to add i2c driver\n", __func__);
	else
		pr_err("%s:success to add i2c driver\n", __func__);
}

void system_i2c_deinit( uint32_t bus )
{
	i2c_del_driver(&otp__i2c_driver);
}

uint8_t system_i2c_write( uint32_t bus, uint32_t phy_addr, uint8_t *data, uint32_t size )
{
	int rc = -1;
	int i = 0;
	int msg_count = 0;
	unsigned short saddr = 0;
	unsigned short addr_type = 0;
	unsigned short data_type = 0;

	if (g_sensor_ctrl == NULL) {
		pr_err("%s:Error input param\n", __func__);
		return -EINVAL;
	}

	addr_type = g_sensor_ctrl->reg_addr_type;
	data_type = g_sensor_ctrl->reg_data_type;

	if (phy_addr == 0)
		saddr = g_sensor_ctrl->slave_addr >> 1;
	else
		saddr = phy_addr >> 1;
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = size,
			.buf   = data,
		}
	};

	msg_count = sizeof(msgs) / sizeof(msgs[0]);

	for (i = 0; i < 3; i++) {
		rc = i2c_transfer(g_sensor_ctrl->client->adapter, msgs, msg_count);
		if (rc == msg_count)
			break;
	}

	if (rc < 0) {
		pr_err("%s:failed to write reg data: rc %d, saddr 0x%x\n", __func__,
					rc, saddr);
		return rc;
	}

	return I2C_OK;
}

uint8_t system_i2c_read( uint32_t bus, uint32_t phy_addr, uint8_t *data, uint32_t size )
{
	int rc = -1;
	int i = 0;
	int msg_count = 0;
	unsigned short saddr = 0;
	unsigned short addr_type = 0;
	unsigned short data_type = 0;

	if (g_sensor_ctrl == NULL || data == NULL) {
		pr_err("%s:Error input param\n", __func__);
		return -EINVAL;
	}

	addr_type = g_sensor_ctrl->reg_addr_type;
	data_type = g_sensor_ctrl->reg_data_type;

	if (phy_addr == 0)
		saddr = g_sensor_ctrl->slave_addr >> 1;
	else
		saddr = phy_addr >> 1;
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = addr_type,
			.buf   = data,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = size,
			.buf   = data,
		},
	};

	msg_count = sizeof(msgs) / sizeof(msgs[0]);

	for (i = 0; i < 3; i++) {
		rc = i2c_transfer(g_sensor_ctrl->client->adapter, msgs, msg_count);
		if (rc == msg_count)
			break;
	}

	if (rc < 0) {
		pr_err("%s:failed to read reg data: rc %d, saddr 0x%02x\n", __func__,
					rc, saddr);
		return rc;
	}

	return I2C_OK;
}

uint32_t IORD( uint32_t BASE, uint32_t REGNUM )
{
    return 0;
}

void IOWR( uint32_t BASE, uint32_t REGNUM, uint32_t DATA )
{
}
