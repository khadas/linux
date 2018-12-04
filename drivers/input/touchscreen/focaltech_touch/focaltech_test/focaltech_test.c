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

 /************************************************************************
*
* File Name: focaltech_test.c
*
* Author:	  Software Department, FocalTech
*
* Created: 2016-03-24
*   
* Modify:
*
* Abstract: create char device and proc node for  the comm between APK and TP
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>//iic
#include <linux/delay.h>//msleep

#include "../focaltech_comm.h"
#include "focaltech_test_main.h"
#include "focaltech_test_ini.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_TEST_INFO  "File Version of  focaltech_test.c:  V1.0.0 2016-03-24"

//配置文件存放目录定义
#define FTS_INI_FILE_PATH "/mnt/sdcard/"  

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
static int fts_test_get_ini_size(char *config_name);
static int fts_test_read_ini_data(char *config_name, char *config_buf);
static int fts_test_save_test_data(char *file_name, char *data_buf, int iLen);
static int fts_test_get_testparam_from_ini(char *config_name);
static int fts_test_entry(char *ini_file_name);


/*******************************************************************************
* functions body
*******************************************************************************/

//获取配置文件大小, 用于分配内存读取配置
static int fts_test_get_ini_size(char *config_name)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	//unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	//magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);

	return fsize;
}
//读取配置到内存
static int fts_test_read_ini_data(char *config_name, char *config_buf)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	//unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	loff_t pos = 0;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	//magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, config_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}
//保存测试数据到SD卡 etc.
static int fts_test_save_test_data(char *file_name, char *data_buf, int iLen)
{
	struct file *pfile = NULL;
	
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, file_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_CREAT|O_RDWR, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(pfile, data_buf, iLen, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

//读取,解析配置文件,初始化测试变量
static int fts_test_get_testparam_from_ini(char *config_name)
{
	char *filedata = NULL;

	int inisize = fts_test_get_ini_size(config_name);

	pr_info("inisize = %d \n ", inisize);
	if (inisize <= 0) {
		pr_err("%s ERROR:Get firmware size failed\n", __func__);
		return -EIO;
	}

	filedata = kmalloc(inisize + 1, GFP_ATOMIC);
		
	if (fts_test_read_ini_data(config_name, filedata)) {
		pr_err("%s() - ERROR: request_firmware failed\n", __func__);
		kfree(filedata);
		return -EIO;
	} else {
		pr_info("fts_test_read_ini_data successful\n");
	}

	set_param_data(filedata);
	return 0;
}

/////////////////////////////////
//测试库调用总入口
///////////////////////////////////
static int fts_test_entry(char *ini_file_name)
{
	/* place holder for future use */
    	char cfgname[128];
	char *testdata = NULL;
	int iTestDataLen=0;//库中测试数据实际长度,用于保存到文件

	/*用于获取存放在库中的测试数据,注意分配空间大小.*/
	testdata = kmalloc(1024*80, GFP_ATOMIC);
	if(NULL == testdata)
	{
		printk("kmalloc failed in function:%s\n", __func__);
		return -1;
	}

	/*初始化平台相关的I2C读写函数*/
	init_i2c_write_func(fts_i2c_write);
	init_i2c_read_func(fts_i2c_read);

	/*初始化指针内存*/
	focaltech_test_main_init();

	/*读取解析配置文件*/
	memset(cfgname, 0, sizeof(cfgname));
	sprintf(cfgname, "%s", ini_file_name);	
	if(fts_test_get_testparam_from_ini(cfgname) <0)
	{
		printk("get testparam from ini failure\n");
		goto TEST_ERR;
	}

	/*根据测试配置开始测试*/
	if(true == start_test_tp())
		printk("tp test pass\n");
	else
		printk("tp test failure\n");
		
	/*获取测试库中的测试数，并保存*/
	iTestDataLen = get_test_data(testdata);
		printk("%s\n", testdata);

	fts_test_save_test_data("testdata.csv", testdata, iTestDataLen);

	/*释放内存等... */
	focaltech_test_main_exit();
	
		
	//mutex_unlock(&g_device_mutex);
	if(NULL != testdata) kfree(testdata);
	return 0;
	
TEST_ERR:
	if(NULL != testdata) kfree(testdata);
	return -1;
}


/************************************************************************
* Name: fts_test_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_test_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = fts_i2c_client;
	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';

	mutex_lock(&fts_input_dev->mutex);
	
	disable_irq(client->irq);
	fts_test_entry( fwname);
	enable_irq(client->irq);
	
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/*  upgrade from app.bin
*    example:echo "***.ini" > fts_test
*/
static DEVICE_ATTR(fts_test, S_IRUGO|S_IWUSR, fts_test_show, fts_test_store);

/* add your attr in here*/
static struct attribute *fts_test_attributes[] = {
	&dev_attr_fts_test.attr,
	NULL
};

static struct attribute_group fts_test_attribute_group = {
	.attrs = fts_test_attributes
};

int fts_test_init(void)
{
	int err=0;
	
	FTS_COMMON_DBG("[focal] %s \n", FOCALTECH_TEST_INFO);	//show version
	FTS_COMMON_DBG("");//default print: current function name and line number
	
	err = sysfs_create_group(&fts_i2c_client->dev.kobj, &fts_test_attribute_group);
	if (0 != err) 
	{
		FTS_COMMON_DBG( "[focal] %s() - ERROR: sysfs_create_group() failed.\n", __func__);
		sysfs_remove_group(&fts_i2c_client->dev.kobj, &fts_test_attribute_group);
		return -EIO;
	} 
	else 
	{
		FTS_COMMON_DBG("[focal] %s() - sysfs_create_group() succeeded.\n",__func__);
	}
	//hidi2c_to_stdi2c(client);
	return err;
}
int fts_test_exit(void)
{	
	FTS_COMMON_DBG("");//default print: current function name and line number
	sysfs_remove_group(&fts_i2c_client->dev.kobj, &fts_test_attribute_group);
	
	return 0;
}

