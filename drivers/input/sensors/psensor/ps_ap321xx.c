/* drivers/input/sensors/access/kxtik.c
 *
 * Copyright (C) 2012-2015 Rockchip Electronics Co., Ltd.
 * Author: luowei <lw@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/sensor-dev.h>


#define AP3212B_NUM_CACHABLE_REGS	23
#define AP3216C_NUM_CACHABLE_REGS	26

#define AP3212B_RAN_COMMAND	0x10
#define AP3212B_RAN_MASK		0x30
#define AP3212B_RAN_SHIFT	(4)

#define AP3212B_MODE_COMMAND	0x00
#define AP3212B_MODE_SHIFT	(0)
#define AP3212B_MODE_MASK	0x07

#define AP3212B_INT_COMMAND	0x01
#define AP3212B_INT_SHIFT	(0)
#define AP3212B_INT_MASK		0x03
#define AP3212B_INT_PMASK		0x02
#define AP3212B_INT_AMASK		0x01

#define AP3212B_OBJ_COMMAND	0x0f
#define AP3212B_OBJ_MASK		0x80
#define AP3212B_OBJ_SHIFT	(7)


/*
 * register access helpers
 */

static int __ap321xx_read_reg(struct i2c_client *client,
			       u32 reg, u8 mask, u8 shift)
{
	u8 val;

	val = i2c_smbus_read_byte_data(client, reg);
	return (val & mask) >> shift;
}

static int __ap321xx_write_reg(struct i2c_client *client,
				u32 reg, u8 mask, u8 shift, u8 val)
{
	int ret = 0;
	u8 tmp;

	tmp = i2c_smbus_read_byte_data(client, reg);
	tmp &= ~mask;
	tmp |= val << shift;

	ret = i2c_smbus_write_byte_data(client, reg, tmp);

	return ret;
}


/*
 * internally used functions
 */
/* range */
static int ap321xx_set_range(struct i2c_client *client, int range)
{
	return __ap321xx_write_reg(client, AP3212B_RAN_COMMAND,
		AP3212B_RAN_MASK, AP3212B_RAN_SHIFT, range);;
}


/* mode */
static int ap321xx_get_mode(struct i2c_client *client)
{
	struct sensor_private_data *sensor =
	    (struct sensor_private_data *) i2c_get_clientdata(client);
	int ret;

	ret = __ap321xx_read_reg(client, sensor->ops->ctrl_reg,
			AP3212B_MODE_MASK, AP3212B_MODE_SHIFT);
	return ret;
}
static int ap321xx_set_mode(struct i2c_client *client, int mode)
{
	struct sensor_private_data *sensor =
	    (struct sensor_private_data *) i2c_get_clientdata(client);
	int ret;

	ret = __ap321xx_write_reg(client, sensor->ops->ctrl_reg,
				AP3212B_MODE_MASK, AP3212B_MODE_SHIFT, mode);
	return ret;
}
static int ap321xx_get_intstat(struct i2c_client *client)
{
	struct sensor_private_data *sensor =
	    (struct sensor_private_data *) i2c_get_clientdata(client);
	int val;

	val = i2c_smbus_read_byte_data(client, sensor->ops->int_status_reg);
	val &= AP3212B_INT_MASK;

	return val >> AP3212B_INT_SHIFT;
}

static int ap321xx_get_object(struct i2c_client *client)
{
	int val;

	val = i2c_smbus_read_byte_data(client, AP3212B_OBJ_COMMAND);
	val &= AP3212B_OBJ_MASK;

	return val >> AP3212B_OBJ_SHIFT;
}


static int ap321xx_product_detect(struct i2c_client *client)
{
	int mid = i2c_smbus_read_byte_data(client, 0x03);
	int pid = i2c_smbus_read_byte_data(client, 0x04);
	int rid = i2c_smbus_read_byte_data(client, 0x05);

	if ( mid == 0x01 && pid == 0x01 &&
	    (rid == 0x03 || rid == 0x04) )
	{
		//printk("RevID [%d], ==> DA3212 v1.5~1.8 ...... AP3212B detected\n", rid);
	}
	else if ( (mid == 0x01 && pid == 0x02 && rid == 0x00) ||
		      (mid == 0x02 && pid == 0x02 && rid == 0x01))
	{
		//printk("RevID [%d], ==> DA3212 v2.0 ...... AP3212C/AP3216C detected\n", rid);
	}
	else
	{
		printk("MakeID[%d] ProductID[%d] RevID[%d] .... can't detect ... bad reversion!!!\n", mid, pid, rid);
		return -EIO;
	}

	return 0;
}

static int ap321xx_init_client(struct i2c_client *client)
{
	/* set defaults */
	ap321xx_set_range(client, 0);
	ap321xx_set_mode(client, 0);

	return 0;
}

static int ap321xx_psensor_enable(struct i2c_client *client)
{
	int ret = 0,mode;

	mode = ap321xx_get_mode(client);
	if((mode & 0x02) == 0){
		mode |= 0x02;
		ret = ap321xx_set_mode(client,mode);
	}

	return ret;
}

static int ap321xx_psensor_disable(struct i2c_client *client)
{
	int ret = 0,mode;

	mode = ap321xx_get_mode(client);
	if(mode & 0x02){
		mode &= ~0x02;
		if(mode == 0x04)
			mode = 0x00;
		ret = ap321xx_set_mode(client,mode);
	}
	return ret;
}

/****************operate according to sensor chip:start************/

static int sensor_active(struct i2c_client *client, int enable, int rate)
{
	int result = 0;

	//register setting according to chip datasheet
	if (enable){
		result = ap321xx_psensor_enable(client);
	}
	else
		result = ap321xx_psensor_disable(client);

	if(result)
		printk("%s:fail to active sensor\n",__func__);

	return result;

}


static int sensor_init(struct i2c_client *client)
{
	struct sensor_private_data *sensor =
	    (struct sensor_private_data *) i2c_get_clientdata(client);
	int result = 0;

	result = ap321xx_product_detect(client);
	if (result)
	{
		dev_err(&client->dev, "ret: %d, product version detect failed.\n",result);
		return result;
	}

	/* initialize the AP3212B chip */
	result = ap321xx_init_client(client);
	if (result)
		return result;

	result = sensor->ops->active(client,0,0);
	if(result)
	{
		printk("%s:line=%d,error\n",__func__,__LINE__);
		return result;
	}

	sensor->status_cur = SENSOR_OFF;

	return result;
}

static int sensor_report_value(struct i2c_client *client)
{
	struct sensor_private_data *sensor =
	    (struct sensor_private_data *) i2c_get_clientdata(client);
	int result = 0;
	char value = 0;
	u8 int_stat;

	int_stat = ap321xx_get_intstat(client);
	// ALS int
	if (int_stat & AP3212B_INT_PMASK)
	{
		value = ap321xx_get_object(client);
		input_report_abs(sensor->input_dev, ABS_DISTANCE, value);
		input_sync(sensor->input_dev);
	}

	return result;
}

static struct sensor_operate proximity_ap321xx_ops = {
	.name				= "ps_ap321xx",
	.type				= SENSOR_TYPE_PROXIMITY,	//sensor type and it should be correct
	.id_i2c				= PROXIMITY_ID_AP321XX,	//i2c id number
	.read_reg			= SENSOR_UNKNOW_DATA,	//read data		//there are two regs, we fix them in code.
	.read_len			= 1,			//data length
	.id_reg				= SENSOR_UNKNOW_DATA,	//read device id from this register   //there are 3 regs, we fix them in code.
	.id_data 			= SENSOR_UNKNOW_DATA,	//device id
	.precision			= 8,			//8 bits
	.ctrl_reg 			= AP3212B_MODE_COMMAND,		//enable or disable
	.int_status_reg 		= AP3212B_INT_COMMAND,	//intterupt status register
	.range				= {0,10},		//range
	.brightness                                        ={10,255},                          // brightness
	.trig				= IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED,
	.active				= sensor_active,
	.init				= sensor_init,
	.report				= sensor_report_value,
};

/****************operate according to sensor chip:end************/
static int proximity_ap321xx_probe(struct i2c_client *client,
				   const struct i2c_device_id *devid)
{
	return sensor_register_device(client, NULL, devid, &proximity_ap321xx_ops);
}

static void proximity_ap321xx_remove(struct i2c_client *client)
{
	sensor_unregister_device(client, NULL, &proximity_ap321xx_ops);
}

static const struct i2c_device_id proximity_ap321xx_id[] = {
	{"ps_ap321xx", PROXIMITY_ID_AP321XX},
	{}
};

static struct i2c_driver proximity_ap321xx_driver = {
	.probe = proximity_ap321xx_probe,
	.remove = (void *)proximity_ap321xx_remove,
	.shutdown = sensor_shutdown,
	.id_table = proximity_ap321xx_id,
	.driver = {
		.name = "proximity_ap321xx",
	#ifdef CONFIG_PM
		.pm = &sensor_pm_ops,
	#endif
	},
};

module_i2c_driver(proximity_ap321xx_driver);

MODULE_AUTHOR("luowei <lw@rock-chips.com>");
MODULE_DESCRIPTION("ps_ap321xx proximity driver");
MODULE_LICENSE("GPL");
