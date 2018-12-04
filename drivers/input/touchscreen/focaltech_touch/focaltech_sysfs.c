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
* File Name: Focaltech_sysfs.c
*
* Author:   Software Department, FocalTech
*
* Created: 2016-03-16
*   
* Modify: 
*
* Abstract: 在sysfs接口创建设备属性（DEVICE_ATTR），供adb shell 终端查看并操作接口
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include "focaltech_comm.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_SYSFS_INFO  "File Version of  focaltech_sysfs.c:  V1.0.0 2016-03-16"



/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/


/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/

/************************************************************************
* Name: fts_get_fw_version_show
* Brief:  show tp fw vwersion
* Input: device, device attribute, char buf
* Output: no
* Return: char number
***********************************************************************/
static ssize_t fts_get_fw_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	//struct i2c_client *client = container_of(dev, struct i2c_client, dev);  jacob use globle fts_wq_data data
	mutex_lock(&fts_input_dev->mutex);
	if (fts_read_reg( FTS_REG_FW_VER, &fwver) < 0)
		return -1;
	
	
	if (fwver == 255)
		num_read_chars = snprintf(buf, PAGE_SIZE,"get tp fw version fail!\n");
	else
{
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);
}
	
	mutex_unlock(&fts_input_dev->mutex);
	
	return num_read_chars;
}
/************************************************************************
* Name: fts_get_fw_version_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_get_fw_version_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}
/************************************************************************
* Name: fts_read_reg_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_read_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}
/************************************************************************
* Name: fts_read_reg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_read_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
#if 1
	ssize_t num_read_chars = 0;
	int retval;
	/*u32 wmreg=0;*/
	long unsigned int wmreg=0;
	u8 regaddr=0xff,regvalue=0xff;
	u8 valbuf[5]={0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&fts_input_dev->mutex);	
	num_read_chars = count - 1;
	if (num_read_chars != 2) 
	{
		FTS_COMMON_DBG( "please input 2 character\n");
		goto error_return;

	}

	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);
	if (0 != retval) 
	{
		FTS_COMMON_DBG( "%s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"\n", __FUNCTION__, buf);
		goto error_return;
	}
	
	/*read register*/
	regaddr = wmreg;
	FTS_COMMON_DBG("[focal][test](0x%02x)\n", regaddr);
	if (fts_read_reg( regaddr, &regvalue) < 0)
		FTS_COMMON_DBG("[Focal] %s : Could not read the register(0x%02x)\n", __func__, regaddr);
	else
		FTS_COMMON_DBG("[Focal] %s : the register(0x%02x) is 0x%02x\n", __func__, regaddr, regvalue);


	error_return:
	mutex_unlock(&fts_input_dev->mutex);
	
	return count;
#else
	ssize_t num_read_chars = 0;
	int retval;
	/*u32 wmreg=0;*/
	long unsigned int wmreg=0;
	u8 regaddr=0xff,regvalue=0xff;
	u8 valbuf[5]={0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&fts_input_dev->mutex);	
	num_read_chars = count - 1;
	if (num_read_chars != 2) 
	{
		if (num_read_chars != 4) 
		{
			FTS_COMMON_DBG( "please input 2 or 4 character\n");
			goto error_return;
		}
	}
	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);
	if (0 != retval) 
	{
		FTS_COMMON_DBG( "%s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"\n", __FUNCTION__, buf);
		goto error_return;
	}
	if (2 == num_read_chars) 
	{
		/*read register*/
		regaddr = wmreg;
		FTS_COMMON_DBG("[focal][test](0x%02x)\n", regaddr);
		if (fts_read_reg( regaddr, &regvalue) < 0)
			FTS_COMMON_DBG("[Focal] %s : Could not read the register(0x%02x)\n", __func__, regaddr);
		else
			FTS_COMMON_DBG("[Focal] %s : the register(0x%02x) is 0x%02x\n", __func__, regaddr, regvalue);
	} 
	else 
	{
		regaddr = wmreg>>8;
		regvalue = wmreg;
		if (fts_write_reg( regaddr, regvalue)<0)
			FTS_COMMON_DBG( "[Focal] %s : Could not write the register(0x%02x)\n", __func__, regaddr);
		else
			FTS_COMMON_DBG( "[Focal] %s : Write 0x%02x into register(0x%02x) successful\n", __func__, regvalue, regaddr);
	}
	error_return:
	mutex_unlock(&fts_input_dev->mutex);
	
	return count;
#endif

}
/************************************************************************
* Name: fts_write_reg_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_write_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}
/************************************************************************
* Name: fts_write_reg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_write_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	ssize_t num_read_chars = 0;
	int retval;
	/*u32 wmreg=0;*/
	long unsigned int wmreg=0;
	u8 regaddr=0xff,regvalue=0xff;
	u8 valbuf[5]={0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&fts_input_dev->mutex);	
	num_read_chars = count - 1;

	if (num_read_chars != 4) 
	{
		FTS_COMMON_DBG( "please input 2 or 4 character\n");
		goto error_return;
	}
	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);
	if (0 != retval) 
	{
		FTS_COMMON_DBG( "%s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"\n", __FUNCTION__, buf);
		goto error_return;
	}

	regaddr = wmreg>>8;
	regvalue = wmreg;
	if (fts_write_reg( regaddr, regvalue)<0)
		FTS_COMMON_DBG( "[Focal] %s : Could not write the register(0x%02x)\n", __func__, regaddr);
	else
		FTS_COMMON_DBG( "[Focal] %s : Write 0x%02x into register(0x%02x) successful\n", __func__, regvalue, regaddr);

	error_return:
	mutex_unlock(&fts_input_dev->mutex);
	
	return count;
}
/************************************************************************
* Name: fts_upgrade_i_file_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_upgrade_i_file_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_upgrade_i_file_store
* Brief:  upgrade from *.i
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_upgrade_i_file_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	//struct fts_ts_data *data = NULL;
	u8 uc_host_fm_ver;
	int i_ret;
	struct i2c_client *client = fts_i2c_client;
	//data = (struct fts_ts_data *) i2c_get_clientdata();

	mutex_lock(&fts_input_dev->mutex);
	
	disable_irq(client->irq);

	i_ret = fts_flash_upgrade_with_i_file();
	if (i_ret == 0)
	{
		msleep(300);
		uc_host_fm_ver = fts_flash_get_i_file_version();
		FTS_COMMON_DBG( "%s [FTS] upgrade to new version 0x%x\n", __func__, uc_host_fm_ver);
	}
	else
	{
		FTS_COMMON_DBG( "%s ERROR:[FTS] upgrade failed ret=%d.\n", __func__, i_ret);
	}

	
	//fts_ctpm_auto_upgrade();
	enable_irq(client->irq);
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_upgrade_bin_file_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_upgrade_bin_file_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_upgrade_bin_file_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_upgrade_bin_file_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = fts_i2c_client;
	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';

	mutex_lock(&fts_input_dev->mutex);
	
	disable_irq(client->irq);
	fts_flash_upgrade_with_bin_file( fwname);
	enable_irq(client->irq);
	
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_fts_get_project_code_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_get_project_code_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	return -EPERM;
}
/************************************************************************
* Name: fts_fts_get_project_code_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_get_project_code_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

/****************************************/
/* sysfs */
/* get the fw version
*   example:cat fts_get_fw_version
*/
static DEVICE_ATTR(fts_get_fw_version, S_IRUGO|S_IWUSR, fts_get_fw_version_show, fts_get_fw_version_store);

/* get project code of fw
*   example:cat fts_get_project_code
*/
static DEVICE_ATTR(fts_get_project_code, S_IRUGO|S_IWUSR, fts_get_project_code_show, fts_get_project_code_store);

/* upgrade from *.i
*   example: echo 1 > fts_upgrade_i_file
*/
static DEVICE_ATTR(fts_upgrade_i_file, S_IRUGO|S_IWUSR, fts_upgrade_i_file_show, fts_upgrade_i_file_store);

/*  upgrade from app.bin
*    example:echo "*_app.bin" > fts_upgrade_bin_file
*/
static DEVICE_ATTR(fts_upgrade_bin_file, S_IRUGO|S_IWUSR, fts_upgrade_bin_file_show, fts_upgrade_bin_file_store);

/* read and write register
*   read example: echo 88 > fts_rw_reg ---read register 0x88
*
*   note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(fts_read_reg, S_IRUGO|S_IWUSR, fts_read_reg_show, fts_read_reg_store);

/* read and write register
*   write example:echo 88 07 > fts_rw_reg ---write 0x07 into register 0x88
*
*   note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(fts_write_reg, S_IRUGO|S_IWUSR, fts_write_reg_show, fts_write_reg_store);

/* add your attr in here*/
static struct attribute *fts_attributes[] = {
	&dev_attr_fts_get_fw_version.attr,
	&dev_attr_fts_get_project_code.attr,		
	&dev_attr_fts_upgrade_i_file.attr,
	&dev_attr_fts_upgrade_bin_file.attr,
	&dev_attr_fts_read_reg.attr,
	&dev_attr_fts_write_reg.attr,	
	NULL
};

static struct attribute_group fts_attribute_group = {
	.attrs = fts_attributes
};

/************************************************************************
* Name: fts_sysfs_init
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_sysfs_init(void)
{
	int err=0;
	
	FTS_COMMON_DBG("[focal] %s \n", FOCALTECH_SYSFS_INFO);	//show version
	FTS_COMMON_DBG("");//default print: current function name and line number
	
	err = sysfs_create_group(&fts_i2c_client->dev.kobj, &fts_attribute_group);
	if (0 != err) 
	{
		FTS_COMMON_DBG( "[focal] %s() - ERROR: sysfs_create_group() failed.\n", __func__);
		sysfs_remove_group(&fts_i2c_client->dev.kobj, &fts_attribute_group);
		return -EIO;
	} 
	else 
	{
		FTS_COMMON_DBG("[focal] %s() - sysfs_create_group() succeeded.\n",__func__);
	}
	//hidi2c_to_stdi2c(client);
	return err;
}
/************************************************************************
* Name: fts_sysfs_exit
* Brief:  remove sysfs
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/
int fts_sysfs_exit(void)
{
	FTS_COMMON_DBG("");//default print: current function name and line number
	sysfs_remove_group(&fts_i2c_client->dev.kobj, &fts_attribute_group);
	return 0;
}
