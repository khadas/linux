/*
 *
 * FocalTech TouchScreen driver.
 * 
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
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
* File Name: focaltech_core.c
*
* Author: Software Department, FocalTech
*
* Created: 2016-01-20
*
*  Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include "focaltech_core.h"
#include "focaltech_comm.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
/*File Version*/
#define FOCALTECH_CORE_INFO  "File Version of  focaltech_core.c:  V1.0.0 2016-03-09"

/*i2c client name for board file*/
#define FOCALTECH_TS_NAME_FOR_BOARD 	"focaltech_ts"

/*i2c client name for DTS file*/
#define FOCALTECH_TS_NAME_FOR_DTS 	"focaltech,focaltech_ts"


/*i2c flag for write*/
#define I2C_M_WRITE		0

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/
static DEFINE_MUTEX(i2c_rw_access);
/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
struct i2c_client *fts_i2c_client = NULL;
struct fts_ts_platform_data *fts_platform_data = NULL;

/*******************************************************************************
* functions body
*******************************************************************************/
static int fts_chip_reset(void)
{
	gpio_direction_output(fts_platform_data->reset_gpio, 0);
	gpio_set_value(fts_platform_data->reset_gpio, 1);
	mdelay(20);
	gpio_set_value(fts_platform_data->reset_gpio, 0);
	mdelay(20);
	gpio_set_value(fts_platform_data->reset_gpio, 1);

	return 0;
}

static int fts_chip_init(void)
{
	fts_chip_reset();

	return 0;
}

/*******************************************************************************
* Name: fts_i2c_read
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
int fts_i2c_read(/*struct i2c_client *client,*/unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen)
{
	int ret;

	if(NULL == fts_i2c_client)
	{
		FTS_COMMON_DBG("[FTS] %s. i2c client is empty!\n", __FUNCTION__);
		return -1;
	}

	mutex_lock(&i2c_rw_access);
	
	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = fts_i2c_client->addr,
				 .flags = I2C_M_WRITE,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = fts_i2c_client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(fts_i2c_client->adapter, msgs, 2);
		if (ret < 0)
			FTS_COMMON_DBG("%s: i2c read error.\n", __func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = fts_i2c_client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(fts_i2c_client->adapter, msgs, 1);
		if (ret < 0)
			FTS_COMMON_DBG("%s:i2c read error.\n", __func__);
	}

	mutex_unlock(&i2c_rw_access);
	
	return ret;
}

/*******************************************************************************
* Name: fts_i2c_write
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
int fts_i2c_write(/*struct i2c_client *client, */unsigned char *writebuf, int writelen)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
			 .addr = fts_i2c_client->addr,
			 .flags = I2C_M_WRITE,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};

	if(NULL == fts_i2c_client)
	{
		FTS_COMMON_DBG("[FTS] %s. i2c client is empty!\n", __FUNCTION__);
		return -1;
	}

	mutex_lock(&i2c_rw_access);
	ret = i2c_transfer(fts_i2c_client->adapter, msgs, 1);
	if (ret < 0)
		FTS_COMMON_DBG("i2c write error.\n");

	mutex_unlock(&i2c_rw_access);
	
	return ret;
}

/*******************************************************************************
* Name: fts_write_reg
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
int fts_write_reg(/*struct i2c_client *client,*/ unsigned char addr, const unsigned char val)
{
	unsigned char buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return fts_i2c_write( buf, sizeof(buf));
}

/*******************************************************************************
* Name: fts_read_reg
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
int fts_read_reg(/*struct i2c_client *client, */unsigned char addr, unsigned char *val)
{
	return fts_i2c_read(&addr, 1, val, 1);
}


static int fts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	printk("FT5336: fts_probe...\n");

	FTS_COMMON_DBG();//default print: current function name and line number
	
	/*//判定适配器能力，保证IIC数据传输正常*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_COMMON_DBG("I2C not supported.\n");
		return -ENODEV;
	}
	
	/*set i2c client as global variable*/
	fts_i2c_client = client;

	/*初始化平台数据*/
	err = fts_platform_data_init(client);
	if(err < 0)
	{
		return err;
	}

	fts_chip_init();

	/*初始化报点模块*/
	err = fts_report_init(client, fts_platform_data);
	if(err < 0)
	{
		return err;
	}
	
	/*初始化手势模块在报点模块调用*/
	
#if 0
	/*初始化烧录模块*/
//	err = fts_flash_init(client);
//	if(err < 0)
//	{
//		return err;
//	}	
	/*初始化与APK通信模块*/
//	err = fts_apk_node_init();
//	if(err < 0)
//	{
//		return err;
//	}	
	/*初始化sysfs文件节点模块*/
	err = fts_sysfs_init();
	if(err < 0)
	{
		return err;
	}	
	
	/*初始化近距离感应模块*/
	err = fts_touch_proximity_init();
	if(err < 0)
	{
		return err;
	}	
	/*初始化ESD保护模块*/
	err = fts_esd_protection_init();
	if(err < 0)
	{
		return err;
	}	
	/*初始化测试模块*/
//	err = fts_test_init();
//	if(err < 0)
//	{
//		return err;
//	}	
#endif
	
	return 0;
}

/*******************************************************************************
* Name: fts_remove
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
static int fts_remove(struct i2c_client *client)
{
	FTS_COMMON_DBG("");//default print: current function name and line number

	/*退出ESD保护模块*/
//	fts_esd_protection_exit();
	
	/*退出烧录模块*/
//	fts_flash_exit();
	
	/*退出与APK通信模块*/
//	fts_apk_node_exit();

	/*退出sysfs文件节点模块*/
//	fts_sysfs_exit();
	
	/*退出近距离感应模块*/
//	fts_touch_proximity_exit();
	
	/*初始化测试模块*/
//	fts_test_exit();

	/*退出报点模块*/
//	fts_report_exit(client, fts_platform_data);

	/*退出手势模块在报点模块调用*/	

	/*平台数据要在其他模块之后释放，因为其他模块有可能还在引用平台数据*/
	fts_platform_data_exit();
	
	return 0;
}

#ifdef CONFIG_PM_SLEEP
/*******************************************************************************
* Name: fts_suspend
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
static int fts_suspend(struct device *dev)
{
	
	FTS_COMMON_DBG("");//default print: current function name and line number

	/*enter suspend for report*/
	fts_report_suspend();
	
	return 0;
}

/*******************************************************************************
* Name: fts_resume
* Brief:
* Input:
* Output: None
* Return: None
*******************************************************************************/
static int fts_resume(struct device *dev)
{
	FTS_COMMON_DBG("");//default print: current function name and line number

	/*enter resume for report*/
	fts_report_resume();
	
	return 0;
}

static SIMPLE_DEV_PM_OPS(ft_pm_ops, fts_suspend, fts_resume);

#endif

#if 0
static const struct i2c_device_id fts_id_table[] = {
	{ FOCALTECH_TS_NAME_FOR_BOARD, 0 },
	{ }//The last element is empty, which is used to mark the end of the table
};
MODULE_DEVICE_TABLE(i2c, fts_id_table);

static struct i2c_driver fts_driver = {
	.probe	=fts_probe,
	.remove	=fts_remove,
	.id_table	=fts_id_table,
	.driver	={
		.name = FOCALTECH_TS_NAME_FOR_BOARD,
		.owner = THIS_MODULE,
		},
	.suspend = fts_suspend,
	.resume	= fts_resume,
};
#else

static const struct i2c_device_id fts_id_table[] = {
	{ FOCALTECH_TS_NAME_FOR_BOARD, 0 },
	{ },//The last element is empty, which is used to mark the end of the table
};
MODULE_DEVICE_TABLE(i2c, fts_id_table);

static const struct of_device_id focaltech_of_match[] = {
		{.compatible = FOCALTECH_TS_NAME_FOR_DTS,},
		{},
};
MODULE_DEVICE_TABLE(of, focaltech_of_match);

static struct i2c_driver fts_driver = {
	.probe	=fts_probe,
	.remove	=fts_remove,
	.id_table	=fts_id_table,
	.driver	={
		.name = FOCALTECH_TS_NAME_FOR_BOARD,
		.owner = THIS_MODULE,
		.of_match_table = focaltech_of_match,
		.pm     = &ft_pm_ops,
		},
};
#endif

static int __init fts_touch_driver_init(void)
{
	FTS_COMMON_DBG("[focal] %s \n", FOCALTECH_CORE_INFO);	//show version
	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);
	return i2c_add_driver(&fts_driver);
}

static void __exit fts_touch_driver_exit(void)
{
	FTS_COMMON_DBG("[focal] Enter %s \n", __func__);
	i2c_del_driver(&fts_driver);
}


MODULE_AUTHOR("Software Department, FocalTech");
MODULE_DESCRIPTION("FocalTech TouchScreen driver");
MODULE_LICENSE("GPL");

module_init(fts_touch_driver_init);
module_exit(fts_touch_driver_exit);

