/*
 *
 * FocalTech TouchScreen driver.
 * 
 *Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
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

 /*******************************************************************************
*
* File Name: focaltech_platform_data.c
*
* Author: Software Department, FocalTech
*
* Created: 2016-03-04
*
* Modified:
*
*  Abstract:
*
*本模块处理与平台相关的数据
*
*
* Reference:
*
*******************************************************************************/
/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

#include "focaltech_comm.h"
#include "focaltech_core.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
/*File Version*/
#define FOCALTECH_PLATFORM_DATA_INFO  "File Version of  focaltech_platform_data.c:  V1.0.0 2016-03-09"

/*Platform Type*/
#define PLATFORM_DATA_TYPE 	1//0:board file, 1:dts file, 2:used define

/*Platform Data*/
#define PLATFORM_DATA_IRQ_GPIO			0//EXYNOS4_GPX1(6)//
#define PLATFORM_DATA_IRQ_GPIO_FLAG		0//IRQF_TRIGGER_FALLING//
#define PLATFORM_DATA_RESET_GPIO		0//	EXYNOS4_GPX1(5)//
#define PLATFORM_DATA_X_RESOLUTION		1024//
#define PLATFORM_DATA_Y_RESOLUTION		800//
#define PLATFORM_DATA_PRESSURE_VALUE	255//pressure_max

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
enum enum_Platform_Data_Type
{
	platform_data_from_board_file = 0,
	platform_data_from_dts_file = 1,	
	platform_data_from_used_define = 2,
};

/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
static int fts_platform_data_parse_dts_file(struct i2c_client *client, struct fts_ts_platform_data *pdata);
static int fts_platform_data_parse_board_file(struct i2c_client *client);
static int fts_platform_data_used_define(struct i2c_client *client, struct fts_ts_platform_data *pdata);


/*******************************************************************************
* functions body
*******************************************************************************/
int fts_platform_data_init(struct i2c_client *client)
{
	int err = 0;
	
	FTS_COMMON_DBG("[focal] %s \n", FOCALTECH_PLATFORM_DATA_INFO);	//show version
	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);
	
	/*申请内存*/
	fts_platform_data = devm_kzalloc(&client->dev,
		sizeof(struct fts_ts_platform_data), GFP_KERNEL);
	if (!fts_platform_data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
		
	/*先初始化平台数据*/
	memset(fts_platform_data, 0, sizeof(*fts_platform_data));

	/*提取来自板文件或DTS文件的平台数据*/
	if(PLATFORM_DATA_TYPE == platform_data_from_board_file)
	{
		err = fts_platform_data_parse_board_file(client);
		if (err < 0 ) {
			dev_err(&client->dev, "Failed to parse board file!\n");
		}
	}
	else if(PLATFORM_DATA_TYPE == platform_data_from_dts_file)
	{
		err = fts_platform_data_parse_dts_file(client, fts_platform_data);
		if (err < 0 ) {
			dev_err(&client->dev, "Failed to parse dts file!\n");
		}			
	}
	else if(PLATFORM_DATA_TYPE == platform_data_from_used_define)
	{
		fts_platform_data_used_define(client, fts_platform_data);				
	}
	else
	{
		return -1;
	}
	client->irq = gpio_to_irq(fts_platform_data->irq_gpio);
	
/*print platform data*/
	FTS_COMMON_DBG("[focal] platform data:\n  irq_gpio = %d\n, irq_gpio_flags = %d\n, reset_gpio = %d\n,\
		x_resolution_max = %d\n, y_resolution_max = %d\n, pressure_max = %d\n,  client->irq = %d\n",
		fts_platform_data->irq_gpio,
		fts_platform_data->irq_gpio_flags,
		fts_platform_data->reset_gpio,
		fts_platform_data->x_resolution_max,
		fts_platform_data->y_resolution_max,
		fts_platform_data->pressure_max,
		client->irq
		);
	
	return err;
}
int fts_platform_data_exit(void)
{
	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);
	kfree(fts_platform_data);
	return 0;
}
static int fts_platform_data_parse_dts_file(struct i2c_client *client, struct fts_ts_platform_data *pdata)
{

	struct device *dev = &client->dev;
	struct device_node *node;
	u32 temp_val;
	int rc = 0;

	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);

	node = dev->of_node;
	if(!node){
		dev_err(dev, "The of_node of device is NULL.\n");
		return -1;
	}
	
	//get int pin for irq*
	pdata->irq_gpio = of_get_named_gpio_flags(node, "focaltech,irq-gpio", 0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
	{
		dev_err(dev, "Unable to get irq_gpio \n");
	}
	
	//get reset pin*
	pdata->reset_gpio = of_get_named_gpio_flags(node, "focaltech,reset-gpio", 0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
	{
		dev_err(dev, "Unable to get reset_gpio \n");
	}	
	
	//get x resolution*
	rc = of_property_read_u32(node, "focaltech,x-resolution", &temp_val);
	if (!rc)
		pdata->x_resolution_max= temp_val;
	else
		dev_err(dev, "Unable to get x-resolution \n");
	
	//get y resolution*
	rc = of_property_read_u32(node, "focaltech,y-resolution", &temp_val);
	if (!rc)
		pdata->y_resolution_max= temp_val;
	else
		dev_err(dev, "Unable to get y-resolution \n");

	if(0 == pdata->irq_gpio)
		    pdata->irq_gpio = PLATFORM_DATA_IRQ_GPIO;

	if(0 == pdata->irq_gpio_flags)
		    pdata->irq_gpio_flags = PLATFORM_DATA_IRQ_GPIO_FLAG;

	if(0 == pdata->reset_gpio)
		    pdata->reset_gpio = PLATFORM_DATA_RESET_GPIO;

	if(0 == pdata->x_resolution_max)
		    pdata->x_resolution_max = PLATFORM_DATA_X_RESOLUTION;

	if(0 == pdata->y_resolution_max)
		    pdata->y_resolution_max = PLATFORM_DATA_Y_RESOLUTION;

	if(0 == pdata->pressure_max)                             
		    pdata->pressure_max = PLATFORM_DATA_PRESSURE_VALUE;

	return 0;	
	
}
static int fts_platform_data_parse_board_file(struct i2c_client *client)
{
	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);
		
	if (!client->dev.platform_data) {
		dev_err(&client->dev, "failed to get platform data\n");
		return -1;
	}
	
	fts_platform_data = client->dev.platform_data;
	/*print platform data*/
	FTS_COMMON_DBG("[focal] platform data2:  irq_gpio = %d, irq_gpio_flags = %d, reset_gpio = %d,\
		x_resolution_max = %d, y_resolution_max = %d, pressure_max = %d,  \n",
		fts_platform_data->irq_gpio,
		fts_platform_data->irq_gpio_flags,
		fts_platform_data->reset_gpio,
		fts_platform_data->x_resolution_max,
		fts_platform_data->y_resolution_max,
		fts_platform_data->pressure_max
		);	
/*	if(0 == pdata->irq_gpio)
		pdata->irq_gpio = PLATFORM_DATA_IRQ_GPIO;
	
	if(0 == pdata->irq_gpio_flags)
		pdata->irq_gpio_flags = PLATFORM_DATA_IRQ_GPIO_FLAG;
	
	if(0 == pdata->reset_gpio)
		pdata->reset_gpio = PLATFORM_DATA_RESET_GPIO;
	
	if(0 == pdata->x_resolution_max)
		pdata->x_resolution_max = PLATFORM_DATA_X_RESOLUTION;
	
	if(0 == pdata->y_resolution_max)
		pdata->y_resolution_max = PLATFORM_DATA_Y_RESOLUTION;

	if(0 == pdata->pressure_max)
		pdata->pressure_max = PLATFORM_DATA_PRESSURE_VALUE;
*/	
	return 0;
}

static int fts_platform_data_used_define(struct i2c_client *client, struct fts_ts_platform_data *pdata)
{
	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);
	
	pdata->irq_gpio = PLATFORM_DATA_IRQ_GPIO;	
	pdata->irq_gpio_flags = PLATFORM_DATA_IRQ_GPIO_FLAG;	
	pdata->reset_gpio = PLATFORM_DATA_RESET_GPIO;	
	pdata->x_resolution_max = PLATFORM_DATA_X_RESOLUTION;	
	pdata->y_resolution_max = PLATFORM_DATA_Y_RESOLUTION;
	pdata->pressure_max = PLATFORM_DATA_PRESSURE_VALUE;	
	return 0;
}


