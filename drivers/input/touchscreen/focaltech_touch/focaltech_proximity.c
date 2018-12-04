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
* File Name: focaltech_proximity.c
*
*    Author: Software Department, FocalTech
*
*   Created: 2016-03-17
*
*  Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include "focaltech_comm.h"

/*******************************************************************************
* 2.Private constant and macro definitions using #define
*******************************************************************************/
#define FTS_TOUCH_PROXIMITY_INFO  "File Version of  focaltech_proximity.c:  V1.0.0 2016-03-16"

#define TOUCH_PROXIMITY_MODE_REG 0xB0
#define TOUCH_PROXIMITY_STATUS_REG 0x01

/*******************************************************************************
* 3.Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* 4.Static variables
*******************************************************************************/

/*******************************************************************************
* 5.Global variable or extern global variabls/functions
*******************************************************************************/

 int  fts_proximity_flag = 0;
 int  fts_proximity_status = 0;
 int  fts_touch_proximity_enter_mode( int mode );
 int  fts_touch_proximity_get_status(int* status);

/*******************************************************************************
* 6.Static function prototypes
*******************************************************************************/

static ssize_t fts_touch_proximity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",fts_proximity_flag);
}

static ssize_t fts_touch_proximity_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int error;
    	int ret;
	error = kstrtoul(buf, 10, &val);
	if(error)
		return error;
	if(val == 1)// enter proximity mode
	{
		if(!fts_proximity_flag)
		{
			fts_proximity_flag = true;
			ret = fts_touch_proximity_enter_mode(true);
		}else
			return count;
	}else// exit proximity mode
	{
		if(fts_proximity_flag)
		{            	
		    	fts_proximity_flag = false;
			ret = fts_touch_proximity_enter_mode(false);
		}else
			return count;
	}
	return count;
}

/* read and write proximity status
*   read example: cat  fts_touch_proximity_status---read  proximity status
*   write example:echo 01 > fts_touch_proximity_status ---write proximity status to 01
*
*/

static DEVICE_ATTR (fts_touch_proximity_status,  S_IRUGO|S_IWUSR,
			fts_touch_proximity_show,
			fts_touch_proximity_store);

static struct attribute *fts_touch_proximity_attrs[] = {	
	&dev_attr_fts_touch_proximity_status.attr,
	NULL,
};

static struct attribute_group fts_touch_proximity_group = {
	.attrs = fts_touch_proximity_attrs,
};

/************************************************************************
* Name: fts_touch_proximity_enter_mode
* Brief:  change proximity mode
* Input:  proximity mode
* Output: no
* Return: success =0
***********************************************************************/
int  fts_touch_proximity_enter_mode( int mode )
{
	int ret = 0;
	static u8 buf_addr[2] = { 0 }; 
	static u8 buf_value[2] = { 0 };
	buf_addr[0]=TOUCH_PROXIMITY_MODE_REG; //proximity control
	
	if(mode)
		buf_value[0] = 0x01;
	else
		buf_value[0] = 0x00;
				
	ret = fts_write_reg( buf_addr[0], buf_value[0]);
	if (ret<0) 
	{
		FTS_COMMON_DBG("[fts][Touch] fts_touch_proximity_enter_mode write value fail \n");
	}
	else
	{
		FTS_COMMON_DBG("[fts][Touch] proximity mode :  %d \n",mode);
	}

	return ret ;

}

/************************************************************************
* Name: fts_touch_proximity_get_status
* Brief:  get proximity status from addr 0x01
* Input:  no
* Output: no
* Return: success =0
***********************************************************************/
int  fts_touch_proximity_get_status( int* status )
{
	int ret = 0;
	static u8 buf_addr[2] = { 0 }; 
	static u8 buf_value[2] = { 0 };
	buf_addr[0]=TOUCH_PROXIMITY_STATUS_REG; //proximity status
		
	ret = fts_read_reg( buf_addr[0], &buf_value[0]);

	if (ret<0) 
	{
		*status=-1;
		FTS_COMMON_DBG("[fts][Touch] fts_enter_proximity write value fail \n");
	}
	else
	{
		*status=buf_value[0];
		FTS_COMMON_DBG("[fts][Touch] proximity status :  %d \n",*status);
	}
	

	return ret ;

}

/************************************************************************
* Name: fts_touch_proximity_init
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_touch_proximity_init(void)
{
	int err=0;

	FTS_COMMON_DBG("[focal] %s \n", FTS_TOUCH_PROXIMITY_INFO);	//show version
	fts_proximity_flag = false;
	
	err = sysfs_create_group(&fts_i2c_client->dev.kobj, &fts_touch_proximity_group);
	if (0 != err) 
	{
		FTS_COMMON_DBG( "[focal] %s(void) - ERROR: fts_touch_proximity_init(void) failed.\n", __func__);
		sysfs_remove_group(&fts_i2c_client->dev.kobj, &fts_touch_proximity_group);
		return -EIO;
	} 
	else 
	{
		FTS_COMMON_DBG("[focal] %s(void) - fts_touch_proximity_init(void) succeeded.\n",__func__);
	}
	
	return err;
}
/************************************************************************
* Name: fts_touch_proximity_exit
* Brief:  remove sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/
int fts_touch_proximity_exit(void)
{
	sysfs_remove_group(&fts_i2c_client->dev.kobj, &fts_touch_proximity_group);
	return 0;
}


