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
* File Name: focaltech_apk_node.c
*
* Author:	  Software Department, FocalTech
*
* Created: 2016-03-16
*   
* Modify:
*
* Abstract: create char device and proc node for  the comm between APK and TP
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include "focaltech_comm.h"

extern int apk_debug_flag;
/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_APK_NODE_INFO  "File Version of  focaltech_apk_node.c:  V1.0.0 2016-03-16"

/* 预设的ft_rw_iic_drv的主设备号*/
#define FTS_CHAR_DEVICE_NAME  						"ft_rw_iic_drv"
#define FTS_CHAR_DEVICE_MAJOR 					210    
#define FTS_I2C_RDWR_MAX_QUEUE 				36
#define FTS_I2C_SLAVEADDR   					11
#define FTS_I2C_RW          						12

/*create apk debug channel*/
#define PROC_UPGRADE							0
#define PROC_READ_REGISTER						1
#define PROC_WRITE_REGISTER					2
#define PROC_AUTOCLB							4
#define PROC_UPGRADE_INFO						5
#define PROC_WRITE_DATA						6
#define PROC_READ_DATA							7
#define PROC_SET_TEST_FLAG						8
#define FTS_DEBUG_DIR_NAME					"fts_debug"
#define PROC_NAME								"ftxxxx-debug"
#define WRITE_BUF_SIZE							512
#define READ_BUF_SIZE							512

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
typedef struct fts_char_device
{
	u8 *buf;  	
	u8 flag;			// 0-write 1-read
	__u16 length; 	// the length of data 
}*pfts_char_device;

typedef struct fts_char_device_queue
{
	struct fts_char_device __user *i2c_queue;
	int queuenum;	
}*pfts_char_device_queue;

struct fts_char_device_dev 
{
	struct cdev cdev;
	struct mutex fts_char_device_mutex;
	struct i2c_client *client;
};


/*******************************************************************************
* Static variables
*******************************************************************************/
static int fts_char_device_major = FTS_CHAR_DEVICE_MAJOR;
static struct class *fts_class;

static unsigned char proc_operate_mode 			= PROC_UPGRADE;
static struct proc_dir_entry *fts_proc_entry;

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
struct fts_char_device_dev *fts_char_device_dev_tt;

extern int apk_debug_flag;

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int fts_char_device_init(void);
static void  fts_char_device_exit(void);
static int fts_char_device_myread(u8 *buf, int length);
static int fts_char_device_mywrite( u8 *buf, int length);
static int fts_char_device_RDWR(unsigned long arg);
static int fts_char_device_open(struct inode *inode, struct file *filp);
static int fts_char_device_release(struct inode *inode, struct file *filp);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
static ssize_t fts_proc_write(struct file *filp, const char __user *buff, size_t count, loff_t *ppos);
static int fts_proc_read( char *page, char **start, off_t off, int count, int *eof, void *data );
#else
static int fts_proc_write(struct file *filp, 
	const char __user *buff, unsigned long len, void *data);
static int fts_proc_read( char *page, char **start, off_t off, int count, int *eof, void *data );
#endif

static int fts_proc_init(void);
static void fts_proc_exit(void);


/*******************************************************************************
* functions body
*******************************************************************************/

int fts_apk_node_init(void)
{
	int err = 0;

	FTS_COMMON_DBG("[focal] %s \n", FOCALTECH_APK_NODE_INFO);	//show version
	FTS_COMMON_DBG("");//default print: current function name and line number

	/*init char device*/
	err = fts_char_device_init();
	if(err < 0)
		return err;

	/*init proc*/
	err = fts_proc_init();
	if(err < 0)
		return err;	
	return 0;
}
int fts_apk_node_exit(void)
{
	FTS_COMMON_DBG("");//default print: current function name and line number
	
	/*exit char device*/
	fts_char_device_exit();
	
	/*exit proc*/
	fts_proc_exit();
	
	return 0;
}
/************************************************************************
* Name: fts_char_device_myread
* Brief: i2c read
* Input: i2c info, data, length
* Output: get data in buf
* Return: fail <0
***********************************************************************/
static int fts_char_device_myread(u8 *buf, int length)
{
	int ret = 0;	
	ret = fts_i2c_read( NULL, 0, buf, length);

	if(ret<0)
	{
		FTS_COMMON_DBG( " IIC Read failed\n");
	}
    	return ret;
}

/************************************************************************
* Name: fts_char_device_mywrite
* Brief: i2c write
* Input: i2c info, data, length
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_char_device_mywrite( u8 *buf, int length)
{
	int ret = 0;
	ret = fts_i2c_write(buf, length);
	if(ret<0)
	{
		FTS_COMMON_DBG( " IIC Write failed\n");
	}
	return ret;
}

/************************************************************************
* Name: fts_char_device_RDWR
* Brief: get package to i2c read/write 
* Input: i2c info, package
* Output: put data in i2c_rw_msg.buf
* Return: fail <0
***********************************************************************/
static int fts_char_device_RDWR(unsigned long arg)
{
	struct fts_char_device_queue i2c_rw_queue;
	struct fts_char_device * i2c_rw_msg;
	u8 __user **data_ptrs;
	int ret = 0;
	int i = 0;

	if (!access_ok(VERIFY_READ, (struct fts_char_device_queue *)arg, sizeof(struct fts_char_device_queue)))
		return -EFAULT;

	if (copy_from_user(&i2c_rw_queue,
		(struct fts_char_device_queue *)arg, 
		sizeof(struct fts_char_device_queue)))
		return -EFAULT;

	if (i2c_rw_queue.queuenum > FTS_I2C_RDWR_MAX_QUEUE)
		return -EINVAL;


	i2c_rw_msg = (struct fts_char_device*)
		kmalloc(i2c_rw_queue.queuenum *sizeof(struct fts_char_device),
		GFP_KERNEL);
	if (!i2c_rw_msg)
		return -ENOMEM;

	if (copy_from_user(i2c_rw_msg, i2c_rw_queue.i2c_queue,
			i2c_rw_queue.queuenum*sizeof(struct fts_char_device))) 
	{
		kfree(i2c_rw_msg);
		return -EFAULT;
	}

	data_ptrs = kmalloc(i2c_rw_queue.queuenum * sizeof(u8 __user *), GFP_KERNEL);
	if (data_ptrs == NULL) 
	{
		kfree(i2c_rw_msg);
		return -ENOMEM;
	}
	
	ret = 0;
	for (i=0; i< i2c_rw_queue.queuenum; i++) 
	{
		if ((i2c_rw_msg[i].length > 8192)||
			(i2c_rw_msg[i].flag & I2C_M_RECV_LEN)) 
		{
			ret = -EINVAL;
			break;
		}
		data_ptrs[i] = (u8 __user *)i2c_rw_msg[i].buf;
		i2c_rw_msg[i].buf = kmalloc(i2c_rw_msg[i].length, GFP_KERNEL);
		if (i2c_rw_msg[i].buf == NULL) 
		{
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(i2c_rw_msg[i].buf, data_ptrs[i], i2c_rw_msg[i].length)) 
		{
			++i;
			ret = -EFAULT;
			break;
		}
	}

	if (ret < 0) 
	{
		int j;
		for (j=0; j<i; ++j)
			kfree(i2c_rw_msg[j].buf);
		kfree(data_ptrs);
		kfree(i2c_rw_msg);
		return ret;
	}

	for (i=0; i< i2c_rw_queue.queuenum; i++) 
	{
		if (i2c_rw_msg[i].flag) 
		{
   	   		ret = fts_char_device_myread(i2c_rw_msg[i].buf, i2c_rw_msg[i].length);
			if (ret>=0)
   	   			ret = copy_to_user(data_ptrs[i], i2c_rw_msg[i].buf, i2c_rw_msg[i].length);
   	   	}
		else
		{
			ret = fts_char_device_mywrite(i2c_rw_msg[i].buf, i2c_rw_msg[i].length);
		}
	}

	FTS_COMMON_DBG( " return:%d. \n", ret);
	
	return ret;
	
}

/************************************************************************
* Name: fts_char_device_open
* Brief: char device open function interface
* Input: node, file point
* Output: no
* Return: 0
***********************************************************************/
static int fts_char_device_open(struct inode *inode, struct file *filp)
{
	filp->private_data = fts_char_device_dev_tt;
	return 0;
}

/************************************************************************
* Name: fts_char_device_release
* Brief: char device close function interface
* Input: node, file point
* Output: no
* Return: 0
***********************************************************************/
static int fts_char_device_release(struct inode *inode, struct file *filp)
{

	return 0;
}

/************************************************************************
* Name: fts_char_device_ioctl
* Brief: char device I/O control function interface
* Input: file point, command, package
* Output: no
* Return: fail <0
***********************************************************************/
static long fts_char_device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct fts_char_device_dev *ftsdev = filp->private_data;
	ftsdev = filp->private_data;

	mutex_lock(&fts_char_device_dev_tt->fts_char_device_mutex);
	switch (cmd)
	{
		case FTS_I2C_RW:
			ret = fts_char_device_RDWR(arg);	
		break; 
               //#if INTEL_EN	 
		//case FTS_RESET_TP:
		//	fts_reset_tp((int)arg);
		//	break;
               //#endif
		default:
			ret =  -ENOTTY;
		break;
	}
	mutex_unlock(&fts_char_device_dev_tt->fts_char_device_mutex);
	return ret;	
}


/*    
* char device file operation which will be put to register the char device
*/
static const struct file_operations fts_char_device_fops = {
	.owner			= THIS_MODULE,
	.open			= fts_char_device_open,
	.release			= fts_char_device_release,
	.unlocked_ioctl	= fts_char_device_ioctl,
};

/************************************************************************
* Name: fts_char_device_setup
* Brief: setup char device 
* Input: device point, index number
* Output: no
* Return: no
***********************************************************************/
static void fts_char_device_setup(struct fts_char_device_dev *dev, int index)
{
	int err, devno = MKDEV(fts_char_device_major, index);

	cdev_init(&dev->cdev, &fts_char_device_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &fts_char_device_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		FTS_COMMON_DBG(KERN_NOTICE "Error %d adding LED%d", err, index);
}


/************************************************************************
* Name: fts_char_device_init
* Brief: initial char device 
* Input: i2c info
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_char_device_init(void)
{
	int err = 0;
	dev_t devno;

	FTS_COMMON_DBG("");//default print: current function name and line number
	
	/*registe device*/
	devno = MKDEV(fts_char_device_major, 0);
	if (fts_char_device_major)
		err = register_chrdev_region(devno, 1, FTS_CHAR_DEVICE_NAME);
	else 
	{
		err = alloc_chrdev_region(&devno, 0, 1, FTS_CHAR_DEVICE_NAME);
		fts_char_device_major = MAJOR(devno);
	}
	if (err < 0) 
	{
		FTS_COMMON_DBG( " ft_rw_iic_drv failed  error code=%d---\n",
				err);
		return err;
	}

	/*memory allocation and setup*/
	fts_char_device_dev_tt = kmalloc(sizeof(struct fts_char_device_dev), GFP_KERNEL);
	if (!fts_char_device_dev_tt)
	{
		err = -ENOMEM;
		unregister_chrdev_region(devno, 1);
		FTS_COMMON_DBG( "ft_rw_iic_drv failed\n");
		return err;
	}
	fts_char_device_dev_tt->client = fts_i2c_client;
	mutex_init(&fts_char_device_dev_tt->fts_char_device_mutex);
	fts_char_device_setup(fts_char_device_dev_tt, 0); 

	/*注册一个类，使mdev可以在"/dev/"目录下 面建立设备节点*/
	fts_class = class_create(THIS_MODULE, "fts_class");
	if (IS_ERR(fts_class)) 
	{
		FTS_COMMON_DBG( "failed in creating class.\n");
		return -1; 
	} 
	
	/*create device node*/
	device_create(fts_class, NULL, MKDEV(fts_char_device_major, 0), 
			NULL, FTS_CHAR_DEVICE_NAME);

	return 0;
}

/************************************************************************
* Name: fts_char_device_exit
* Brief: delete char device 
* Input: no
* Output: no
* Return: no
***********************************************************************/
static void  fts_char_device_exit(void)
{
	FTS_COMMON_DBG("");//default print: current function name and line number
	
	device_destroy(fts_class, MKDEV(fts_char_device_major, 0)); 
	/* delete class created by us */
	class_destroy(fts_class); 
	/* delet the cdev */
	cdev_del(&fts_char_device_dev_tt->cdev);
	kfree(fts_char_device_dev_tt);
	unregister_chrdev_region(MKDEV(fts_char_device_major, 0), 1); 
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
/*interface of write proc*/
/************************************************************************
*   Name: fts_proc_write
*  Brief:interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static ssize_t fts_proc_write(struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
	unsigned char writebuf[WRITE_BUF_SIZE];
	int buflen = count;
	int writelen = 0;
	int ret = 0;

	if (copy_from_user(&writebuf, buff, buflen)) {
		FTS_COMMON_DBG( "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			FTS_COMMON_DBG("%s\n", upgrade_file_path);	
						
			disable_irq(fts_i2c_client->irq);
			ret = fts_flash_upgrade_with_bin_file( upgrade_file_path);
			enable_irq(fts_i2c_client->irq);
			if (ret < 0) {
				FTS_COMMON_DBG( "%s:upgrade failed.\n", __func__);
				return ret;
			}
		}
		break;
	//case PROC_SET_TEST_FLAG:
	
	//	break;
	case PROC_SET_TEST_FLAG:
		
		/*#if FT_ESD_PROTECT
		apk_debug_flag=writebuf[1];
		if(1==apk_debug_flag)
			esd_switch(0);
		else if(0==apk_debug_flag)
			esd_switch(1);
		
		FTS_COMMON_DBG("\n zax flag=%d \n",apk_debug_flag);
		#endif	
		*/
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		ret = fts_i2c_write( writebuf + 1, writelen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = fts_i2c_write( writebuf + 1, writelen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_AUTOCLB:
		FTS_COMMON_DBG("%s: autoclb\n", __func__);
		fts_ctpm_auto_clb();
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = count - 1;
		if(writelen>0)
		{
			ret = fts_i2c_write( writebuf + 1, writelen);
			if (ret < 0) {
				FTS_COMMON_DBG( "%s:write iic error\n", __func__);
				return ret;
			}
		}
		break;
	default:
		break;
	}
	
	return count;
}

/* interface of read proc */
/************************************************************************
*   Name: fts_proc_read
*  Brief:interface of read proc
* Input: point to the data, no use, no use, read len, no use, no use 
* Output: page point to data
* Return: read char number
***********************************************************************/
static ssize_t fts_proc_read(struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
	int ret = 0;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	unsigned char buf[READ_BUF_SIZE];

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		// after calling fts_proc_write to upgrade
		regaddr = 0xA6;
		ret = fts_read_reg( regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = fts_i2c_read( NULL, 0, buf, readlen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:read iic error\n", __func__);
			return ret;
		} 
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		readlen = count;
		ret = fts_i2c_read( NULL, 0, buf, readlen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:read iic error\n", __func__);
			return ret;
		}
		
		num_read_chars = readlen;
		break;
	case PROC_WRITE_DATA:
		break;
	default:
		break;
	}
	
	if (copy_to_user(buff, buf, num_read_chars)) {
		FTS_COMMON_DBG( "%s:copy to user error\n", __func__);
		return -EFAULT;
	}
        //memcpy(buff, buf, num_read_chars);
	return num_read_chars;
}
static const struct file_operations fts_proc_fops = {
		.owner 	= THIS_MODULE,
		.read 	= fts_proc_read,
		.write 	= fts_proc_write,
		
};
#else
/* interface of write proc */
/************************************************************************
*   Name: fts_proc_write
*  Brief:interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static int fts_proc_write(struct file *filp, 
	const char __user *buff, unsigned long len, void *data)
{
	unsigned char writebuf[WRITE_BUF_SIZE];
	int buflen = len;
	int writelen = 0;
	int ret = 0;
	

	if (copy_from_user(&writebuf, buff, buflen)) {
		FTS_COMMON_DBG( "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
	
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			FTS_COMMON_DBG("%s\n", upgrade_file_path);

			disable_irq(fts_i2c_client->irq);
			ret = fts_flash_upgrade_with_bin_file( upgrade_file_path);
			enable_irq(fts_i2c_client->irq);
			if (ret < 0) {
				FTS_COMMON_DBG( "%s:upgrade failed.\n", __func__);
				return ret;
			}

		}
		break;
	//case PROC_SET_TEST_FLAG:
	
	//	break;
	case PROC_SET_TEST_FLAG:
	/*	#if FT_ESD_PROTECT
		apk_debug_flag=writebuf[1];
		if(1==apk_debug_flag)
			esd_switch(0);
		else if(0==apk_debug_flag)
			esd_switch(1);
		FTS_COMMON_DBG("\n zax flag=%d \n",apk_debug_flag);
		#endif
		*/
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		ret = fts_i2c_write( writebuf + 1, writelen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = fts_i2c_write( writebuf + 1, writelen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_AUTOCLB:
		FTS_COMMON_DBG("%s: autoclb\n", __func__);
		fts_ctpm_auto_clb();
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = len - 1;
		if(writelen>0)
		{
			ret = fts_i2c_write( writebuf + 1, writelen);
			if (ret < 0) {
				FTS_COMMON_DBG( "%s:write iic error\n", __func__);
				return ret;
			}
		}
		break;
	default:
		break;
	}
	
	return len;
}

/* interface of read proc */
/************************************************************************
*   Name: fts_proc_read
*  Brief:interface of read proc
* Input: point to the data, no use, no use, read len, no use, no use 
* Output: page point to data
* Return: read char number
***********************************************************************/
static int fts_proc_read( char *page, char **start,
	off_t off, int count, int *eof, void *data )
{
	//struct i2c_client *client = (struct i2c_client *)fts_proc_entry->data;
	int ret = 0;
	unsigned char buf[READ_BUF_SIZE];
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		// after calling fts_proc_write to upgrade
		regaddr = 0xA6;
		ret = fts_read_reg( regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = fts_i2c_read( NULL, 0, buf, readlen);
		if (ret < 0) {

			FTS_COMMON_DBG( "%s:read iic error\n", __func__);
			return ret;
		} 
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		readlen = count;
		ret = fts_i2c_read( NULL, 0, buf, readlen);
		if (ret < 0) {
			FTS_COMMON_DBG( "%s:read iic error\n", __func__);
			return ret;
		}
		
		num_read_chars = readlen;
		break;
	case PROC_WRITE_DATA:
		break;
	default:
		break;
	}
	
	memcpy(page, buf, num_read_chars);

	return num_read_chars;
}
#endif
/************************************************************************
* Name: fts_create_apk_debug_channel
* Brief:  create apk debug channel
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
static int fts_proc_init(void)
{
	FTS_COMMON_DBG("");//default print: current function name and line number
	

	#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
		fts_proc_entry = proc_create(PROC_NAME, 0777, NULL, &fts_proc_fops);		
	#else
		fts_proc_entry = create_proc_entry(PROC_NAME, 0777, NULL);
	#endif
	if (NULL == fts_proc_entry) 
	{
		FTS_COMMON_DBG( "Couldn't create proc entry!\n");
		
		return -ENOMEM;
	} 
	else 
	{
		FTS_COMMON_DBG( "Create proc entry success!\n");
		
		#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
			//fts_proc_entry->data = client;
			fts_proc_entry->write_proc = fts_proc_write;
			fts_proc_entry->read_proc = fts_proc_read;
		#endif
	}
	return 0;
}
/************************************************************************
* Name: fts_release_apk_debug_channel
* Brief:  release apk debug channel
* Input: no
* Output: no
* Return: no
***********************************************************************/
static void fts_proc_exit(void)
{
	FTS_COMMON_DBG("");//default print: current function name and line number
	
	if (fts_proc_entry)
		#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
			proc_remove(PROC_NAME);
		#else
			remove_proc_entry(PROC_NAME, NULL);
		#endif
}

