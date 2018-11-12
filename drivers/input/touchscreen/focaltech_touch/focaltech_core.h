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

#ifndef __FOCALTECH_CORE_H__
#define __FOCALTECH_CORE_H__
 /*******************************************************************************
*
* File Name: focaltech_core.h
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
* Included header files
*******************************************************************************/
#include "focaltech_comm.h"

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*
#define FTS_DBG_EN 0
#if FTS_DBG_EN
#define FTS_DBG(fmt, args...) 				printk("[FTS]" fmt, ## args)
#else
#define FTS_DBG(fmt, args...) 				do{}while(0)
#endif
*/
/*与平台相关数据，可从外部配置*/
struct fts_ts_platform_data {
	u32 x_resolution_max;
	u32 y_resolution_max;
	u32 pressure_max;
	
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	
};

/*驱动应用到的数据，不应由外部来修改*/
/*struct fts_ts_inside_data {
	//struct i2c_client *client;
	//struct input_dev *input_dev;
	struct ts_event event;
	const struct ts_platform_data *pdata;
	struct work_struct 	touch_event_work;
	struct workqueue_struct *ts_workqueue;
};*/

/////////////////////////////////////////////
/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
//extern struct i2c_client *fts_i2c_client;
//extern struct input_dev *fts_input_dev;
extern struct fts_ts_platform_data *fts_platform_data;

//Base functions
extern int fts_i2c_read(/*struct i2c_client *client,*/unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen);
extern int fts_i2c_write(/*struct i2c_client *client,*/unsigned char *writebuf, int writelen);
extern int fts_read_reg(/*struct i2c_client *client,*/unsigned char addr, unsigned char *val);
extern int fts_write_reg(/*struct i2c_client *client,*/ unsigned char addr, const unsigned char val);


#endif
