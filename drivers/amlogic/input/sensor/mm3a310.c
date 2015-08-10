/*
 *  mm3a310.c - Linux kernel modules for 3-Axis Accelerometer
 *
 *  Copyright (C) 2011-2012 MiraMEMS Sensing Technology Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/amlogic/sensor/mm3a310.h>
#include <linux/amlogic/sensor/sensor_common.h>
#if 1
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

/* driver version info*/
#define DRIVER_VERSION "2013-08-23-15:33:46_v1.4.0"

/* Debug Function */
#define MI_TAG                  "[mm3a310] "
enum {
	DEBUG_ERR = 1,
	DEBUG_MSG = 1<<1,
	DEBUG_FUNC = 1<<2,
	DEBUG_DATA = 1<<3,
};

/* this level can be modified while runtime through system attribute */
static int Log_level;

#define MI_ASSERT(expr) \
do { \
	if (!(expr)) { \
		dprintk(KERN_ERR "Assertion failed! %s,%d,%s,%s\n", \
			__FILE__, __LINE__, __func__, #expr);\
	} \
} while (0)

/* DO NOT TRY TO MODIFY FOLLOWING DEFINE */
#define PLATFORM_QUACOMM 0
#define PLATFORM_SPRD    1
#define PLATFORM_ELSE    8

#define DEVICE_CREATE_BYSELF 0   /* 1 means define device in this driver */
#define DEVICE_CREATE_BYPLATFORM 2  /* 2 means define device in system file */
/* --- DO NOT TRY TO MODIFY FOLLOWING DEFINE */


/* chip related function configure */
#define FILTER_AVERAGE_ENHANCE                  1
/* only take effect when FILTER_AVERAGE_ENHANCE open */
#define FILTER_AVERAGE_EX                       1
#define MM3A310_OFFSET_TEMP_SOLUTION            1
/* whether to suppot auto calibrate while driver insatll,
only take effect when MM3A310_OFFSET_TEMP_SOLUTION open */
#define MM3A310_AUTO_CALIBRAE			0
#define MM3A310_STK_TEMP_SOLUTION               1
/*if corwork with miramems' msensor, this compiler option should be set to 1,
and a special IOCTL will be supported */
#define COWORK_WITH_DM211                       0

/* PLATFORM_QUACOMM PLATFORM_SPRD */
#define TARGET_PLATFORM				PLATFORM_ELSE
/* DEVICE_CREATE_BYSELF DEVICE_CREATE_BYPLATFORM */
#define DEVICE_CREATE_MODE			DEVICE_CREATE_BYPLATFORM


/* --- chip related function configure */

#if COWORK_WITH_DM211
#define GSENSOR					0x85
#define GSENSOR_IOCTL_READ_SENSORDATA		_IOR(GSENSOR, 0x03, int)
static int gx, gy, gz;
#endif
/*
 * Defines
 */

#if TARGET_PLATFORM == PLATFORM_QUACOMM
/* this name should be compatible with the define in HAL */
#define MM3A310_INPUT_DEV_NAME "acc"
#else
/* this name should be compatible with the define in HAL */
#define MM3A310_INPUT_DEV_NAME "accelerometer"
#endif

#define MM3A310_MISC_NAME   MM3A310_DRV_NAME
#define MM3A310_ID                 0x13 /* WHO AM I*/

#define GRAVITY_EARTH                   9806550
#define ABSMIN_2G                       (-GRAVITY_EARTH * 2)
#define ABSMAX_2G                       (GRAVITY_EARTH * 2)

#define POLL_INTERVAL_MAX   500
#define POLL_INTERVAL       35
#define INPUT_FUZZ          32
#define INPUT_FLAT          32
static int delayMs = 50;

#if  DEVICE_CREATE_MODE == DEVICE_CREATE_BYSELF
/* #error "please confirm the I2C bus number and chip's slave address" */
#define I2C_STATIC_BUS_NUM        (0) /* define which I2C bus to connect */
#define MM3A310_I2C_ADDR    0x26 /* When SA0=1 then 27. When SA0=0 then 26*/
static struct i2c_board_info mm3a310_i2c_boardinfo = {
	I2C_BOARD_INFO(MM3A310_DRV_NAME, MM3A310_I2C_ADDR),
};
#endif

#define PAGE_ADDR(reg)      ((reg >> 8) & 0xFF)
#define REG_ADDR(reg)       (reg & 0x00FF)

/* register enum for MM3A310 registers */
enum {

	MM3A310_PAGE_NO = 0x00,
	MM3A310_OSC_REG,
	MM3A310_TEST_REG1,
	MM3A310_TEST_REG2,
	MM3A310_OTP_PG,
	MM3A310_OTP_PTM,
	MM3A310_LDO_REG,

	MM3A310_TEMP_OUT_L = 0x0d,
	MM3A310_TEMP_OUT_H,
	MM3A310_WHO_AM_I,

	MM3A310_OVRN_DURATION = 0x1e,
	MM3A310_TEMP_CFG_REG,

	MM3A310_CTRL_REG1,
	MM3A310_CTRL_REG2,
	MM3A310_CTRL_REG3,
	MM3A310_CTRL_REG4,
	MM3A310_CTRL_REG5,
	MM3A310_CTRL_REG6,

	MM3A310_REFERENCE,
	MM3A310_STATUS_REG,

	MM3A310_OUT_X_L,
	MM3A310_OUT_X_H,
	MM3A310_OUT_Y_L,
	MM3A310_OUT_Y_H,
	MM3A310_OUT_Z_L,
	MM3A310_OUT_Z_H,

	MM3A310_FIFO_CTRL_REG,
	MM3A310_FIFO_SRC,

	MM3A310_INT1_CFG,
	MM3A310_INT1_SRC,
	MM3A310_INT1_THS,
	MM3A310_INT1_DURATION,

	MM3A310_INT2_CFG,
	MM3A310_INT2_SRC,
	MM3A310_INT2_THS,
	MM3A310_INT2_DURATION,

	MM3A310_CLICK_CFG,
	MM3A310_CLICK_SRC,
	MM3A310_CLICK_THS,

	MM3A310_TIME_LIMIT,
	MM3A310_TIME_LATENCY,
	MM3A310_TIME_WINDOW = 0x3d,

	MM3A310_SOFT_RESET = 0x0105,

	MM3A310_OTP_FLAG = 0x0109,

	MM3A310_OTP_XOFF_L = 0x0110,
	MM3A310_OTP_XOFF_H,
	MM3A310_OTP_YOFF_L,
	MM3A310_OTP_YOFF_H,
	MM3A310_OTP_ZOFF_L,
	MM3A310_OTP_ZOFF_H,
	MM3A310_OTP_XSO,
	MM3A310_OTP_YSO,
	MM3A310_OTP_ZSO,
	MM3A310_OTP_TRIM_THERM_L,
	MM3A310_OTP_TRIM_THERM_H,
	MM3A310_OTP_TRIM_OSC,

	MM3A310_LPF_ABSOLUTE,
	MM3A310_LPF_COEF_A1_L,
	MM3A310_LPF_COEF_A1_H,
	MM3A310_LPF_COEF_A2_L,
	MM3A310_LPF_COEF_A2_H,
	MM3A310_LPF_COEF_B0_L,
	MM3A310_LPF_COEF_B0_H,
	MM3A310_LPF_COEF_B1_L,
	MM3A310_LPF_COEF_B1_H,
	MM3A310_LPF_COEF_B2_L,
	MM3A310_LPF_COEF_B2_H,

	MM3A310_TEMP_OFF1,
	MM3A310_TEMP_OFF2,
	MM3A310_TEMP_OFF3,

	MM3A310_OTP_SO_COEFF = 0x012a
};

/* MM3A310 G range */
enum {
	MODE_2G = 0x00,
	MODE_4G = 0x10,
	MODE_8G = 0x20,
	MODE_16G = 0x30,
};

/* MM3A310 Status */
struct mm3a310_status {
	u8 mode; /* Full Scale */
	u8 ctl_reg1; /* ODR */
	u8 temp_cfg_reg; /* Power down mode */

	/* OTP Offset */
	u8 otp_xoff_l;
	u8 otp_xoff_h;
	u8 otp_yoff_l;
	u8 otp_yoff_h;
	u8 otp_zoff_l;
	u8 otp_zoff_h;
};


/*
 * ===========================================
 *  gLobal Variable DEFINE
 * ===========================================
 */
static struct mm3a310_status mm3a_status = {
	.mode     = 0x00,
	.ctl_reg1    = 0x08,
	.temp_cfg_reg = 0x28,
	/* OTP Offset */
	.otp_xoff_l = 0x00,
	.otp_xoff_h = 0x20,
	.otp_yoff_l = 0x00,
	.otp_yoff_h = 0x20,
	.otp_zoff_l = 0x00,
	.otp_zoff_h = 0x20,
};

#if MM3A310_OFFSET_TEMP_SOLUTION
static int bCaliResult;
#define MM3A310_LSB_TO_MG       20
#define THRESHOLD                10
#define OFFSET_VAL_LEN          15
static int mm3a310_write_offset_to_file(unsigned short x,
	unsigned short y, unsigned short z);
static int mm3a310_read_offset_from_file(unsigned short *x,
	unsigned short *y, unsigned short *z);
static void manual_load_cali_file(void);
static const char OffsetFileName[] = "/data/mm3a310_offset.txt";

struct work_info {
	char tst1[20];
	int    len;
	char buffer[OFFSET_VAL_LEN];
	int    rst;
	struct workqueue_struct *wq;
	struct delayed_work read_work;
	struct delayed_work write_work;
	struct completion completion;
	char tst2[20];
};

static struct work_info m_work_info = {{0} };
static int is_cali;
static int check_linearity_offset(void);
static int mm3a310_calibrate(struct mm3a310_cali_s *mm3a310_cali_data);

#endif /* !MM3A310_OFFSET_TEMP_SOLUTION */

static struct input_polled_dev *mm3a310_idev;
/*static struct device *hwmon_dev;*/
static struct i2c_client *mm3a310_i2c_client;

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend early_suspend;
#endif

/* static void mm3a310_write_offset(unsigned short x,
unsigned short y, unsigned short z); */
static int mm3a310_read_data(short *x, short *y, short *z);
static int mm3a310_preset_register(struct i2c_client *client);
/* wrapped for MM3A310 REV2 */
static int mm3a310_read_register(struct i2c_client *client, u16 reg);
static int mm3a310_write_register(struct i2c_client *client, u16 reg, u8 data);
static int mm3a310_read_register_continuously(struct i2c_client *client,
	u16 base_reg, u8 count, u8 *data);

static DEFINE_MUTEX(mm3a310_rw_lock);

#if MM3A310_STK_TEMP_SOLUTION
#define STICK_LSB 2000
#define AIX_HISTORY_SIZE 3
static short aixHistort[AIX_HISTORY_SIZE*3] = {0};
static short aixHistoryIndex;
static bool bxstk;
static bool bystk;
static bool bzstk;

static void addAixHistory(short x, short y, short z)
{
	aixHistort[aixHistoryIndex++] = x;
	aixHistort[aixHistoryIndex++] = y;
	aixHistort[aixHistoryIndex++] = z;
	aixHistoryIndex = (aixHistoryIndex)%(AIX_HISTORY_SIZE*3);
}


static bool isXStick(void)
{
	int i;
	for (i = 0; i < AIX_HISTORY_SIZE; i++) {
		if (aixHistort[i*AIX_HISTORY_SIZE] < STICK_LSB &&
			aixHistort[aixHistoryIndex] > (0-STICK_LSB))
			break;
	}

	return i == AIX_HISTORY_SIZE;
}

static bool isYStick(void)
{
	int i;
	for (i = 0; i < AIX_HISTORY_SIZE; i++) {
		if (aixHistort[i*AIX_HISTORY_SIZE+1] < STICK_LSB &&
			aixHistort[aixHistoryIndex+1] > (0-STICK_LSB))
			break;
	}

	return i == AIX_HISTORY_SIZE;
}

static bool isZStick(void)
{
	int i;
	for (i = 0; i < AIX_HISTORY_SIZE; i++) {
		if (aixHistort[i*AIX_HISTORY_SIZE+2] < STICK_LSB &&
			aixHistort[aixHistoryIndex+2] > (0-STICK_LSB))
			break;
	}

	return i == AIX_HISTORY_SIZE;
}

static int squareRoot(int val)
{
	int r = 0;
	int shift;

	if (val < 0)
		return 0;

	for (shift = 0; shift < 32; shift += 2) {
		int x = 0x40000000l >> shift;
		if (x + r <= val) {
			val -= x + r;
			r = (r >> 1) | x;
		} else
			r = r >> 1;
	}

	return r;
}
#endif /* ! MM3A310_STK_TEMP_SOLUTION */


#if FILTER_AVERAGE_ENHANCE
#ifdef FILTER_AVERAGE_EX
#define         PEAK_LVL        800
#endif

struct FilterAverageContext {
	int sample_l;
	int sample_h;
	int filter_param_l;
	int filter_param_h;
	int filter_threhold;

	int refN_l;
	int refN_h;

};

static struct FilterAverageContext tFac[3] = {{0} };

static short filter_average(short preAve, short sample,
	int paramN, int *refNum)
{
#if FILTER_AVERAGE_EX
	if (abs(sample-preAve) > PEAK_LVL  && *refNum < 3) {
		sample = preAve;
		(*refNum)++;
	} else {
		if (*refNum == 3)
			preAve = sample;
		*refNum  = 0;
	}
#endif
	/*paramN = abs(sample) < 80 ? 16 : paramN;*/
	return preAve + (sample - preAve)/paramN;
}

static int filter_average_enhance(struct FilterAverageContext *fac,
	short sample)
{
	if (fac == NULL)
		return 0;

	if (fac->filter_param_l == fac->filter_param_h)
		fac->sample_l = fac->sample_h = filter_average(fac->sample_l,
			sample, fac->filter_param_l, &fac->refN_l);
	else {
		fac->sample_l = filter_average(fac->sample_l, sample,
			fac->filter_param_l,  &fac->refN_l);
		fac->sample_h = filter_average(fac->sample_h, sample,
			fac->filter_param_h, &fac->refN_h);
		if (abs(fac->sample_l - fac->sample_h) > fac->filter_threhold)
			fac->sample_h = fac->sample_l;
	}

	return fac->sample_h;
}

#endif /* ! FILTER_AVERAGE_ENHANCE */

/*
 * ===========================================
 *  COMMON UTITILT FUNCTION
 * ===========================================
 */

static int mm3a310_read_register(struct i2c_client *client, u16 reg)
{
	int ret = 0;
	int val;

	mutex_lock(&mm3a310_rw_lock);

	/* Get current page NO. */
	val = i2c_smbus_read_byte_data(client, MM3A310_PAGE_NO);
	if (val < 0)
		ret = -1;
	/* verify page No. */
	else if (val != PAGE_ADDR(reg))
		ret = i2c_smbus_write_byte_data(client, MM3A310_PAGE_NO,
			PAGE_ADDR(reg));
	if (!ret)
		val = i2c_smbus_read_byte_data(client, REG_ADDR(reg));
	mutex_unlock(&mm3a310_rw_lock);
	if (ret == 0)
		ret = val;
	return ret;
}

static int mm3a310_write_register(struct i2c_client *client, u16 reg, u8 data)
{
	int ret = 0;
	int val;

	mutex_lock(&mm3a310_rw_lock);

	/* Get current page NO. */
	val = i2c_smbus_read_byte_data(client, MM3A310_PAGE_NO);
	if (val < 0)
		ret = -1;
	/* verify page No. */
	else if (val != PAGE_ADDR(reg))
		ret = i2c_smbus_write_byte_data(client, MM3A310_PAGE_NO,
			PAGE_ADDR(reg));

	if (!ret)
		ret = i2c_smbus_write_byte_data(client, REG_ADDR(reg), data);

	mutex_unlock(&mm3a310_rw_lock);
	if (ret != 0)
		ret = -1;
	return ret;
}

static int mm3a310_read_register_continuously(struct i2c_client *client,
	u16 base_reg, u8 count, u8 *data)
{
	int ret = 0;
	int val;

	mutex_lock(&mm3a310_rw_lock);

	/* Get current page NO. */
	val = i2c_smbus_read_byte_data(client, MM3A310_PAGE_NO);
	if (val < 0)
		ret = -1;
	/* verify page No. */
	else if (val != PAGE_ADDR(base_reg))
		ret = i2c_smbus_write_byte_data(client,
			MM3A310_PAGE_NO, PAGE_ADDR(base_reg));

	if (!ret) {
		base_reg |= 0x80;
		if (i2c_smbus_read_i2c_block_data(client, REG_ADDR(base_reg),
			count, data) != count)
			ret = -1;
	}
	mutex_unlock(&mm3a310_rw_lock);
	if (ret == 0)
		ret = count;
	else
		ret =  0;
	return ret;
}

static int mm3a310_set_enable(bool bEnable)
{
	struct i2c_client *client;
	int ret, rst;
	u8  val;

	client = mm3a310_i2c_client;
	rst = 0;

#if MM3A310_OFFSET_TEMP_SOLUTION
	manual_load_cali_file();
#endif

	val = mm3a310_read_register(client, MM3A310_TEMP_CFG_REG);
	if (val < 0)
		rst = -1;
	else {
		if (bEnable)
			ret = mm3a310_write_register(client,
				MM3A310_TEMP_CFG_REG, val&0xDF);
		else
			ret = mm3a310_write_register(client,
				MM3A310_TEMP_CFG_REG, val|0x20);

		rst = (ret == 0?0 :  -1);
	}

	return rst;
}

static int mm3a310_get_enable(bool *bEnable)
{
	struct i2c_client *client;
	u8 val;
	int rst;


	if (bEnable == NULL)
		return -1;

	client = mm3a310_i2c_client;

	val = mm3a310_read_register(client, MM3A310_TEMP_CFG_REG);
	if (val < 0)
		rst = -1;
	else {
		*bEnable = (val&0x20)?0:1;
		rst = 0;
	}

	return rst;
}


static int mm3a310_set_odr(int odr)
{
	struct i2c_client *client;
	int ret;
	u8  val;
	int rst;

	client = mm3a310_i2c_client;

	/*check odr param*/
	if (odr > 9 || odr < 0)
		return -1;

	val = mm3a310_read_register(client, MM3A310_CTRL_REG1);
	if (val < 0)
		rst = -1;
	else {
		ret = mm3a310_write_register(client, MM3A310_CTRL_REG1,
			(val&0x0F)|(odr<<4));
		rst = (ret == 0?0 :  -1);
	}

	return rst;
}

static int mm3a310_get_odr(int *odr)
{
	int rst;
	struct i2c_client *client;

	if (odr == NULL)
		return -1;
	client = mm3a310_i2c_client;

	*odr = mm3a310_read_register(client, MM3A310_CTRL_REG1);
	if (*odr < 0)
		rst = -1;
	else {
		*odr = ((*odr)>>4)&0x0F;
		rst = 0;
	}

	return rst;
}

static int mm3a310_set_grange(int newrange)
{
	int ret;
	struct i2c_client *client;
	short range;
	int rst;

	client = mm3a310_i2c_client;

	range = mm3a310_read_register(client, MM3A310_CTRL_REG4);
	if (range < 0)
		rst = -1;
	else{
		range = (range&0xCF)|((newrange&0x03)<<4);

		ret = mm3a310_write_register(client, MM3A310_CTRL_REG4, range);
		rst = (ret == 0?0 :  -1);
	}

	return rst;
}

static int mm3a310_get_grange(int *range)
{
	struct i2c_client *client;
	int rst;

	if (range == NULL)
		return -1;
	client = mm3a310_i2c_client;

	*range = mm3a310_read_register(client, MM3A310_CTRL_REG4);
	if (*range < 0)
		rst = -1;
	else{
		*range = (*range>>4)&0x03;
		rst = 0;
	}

	return rst;
}




#if MM3A310_OFFSET_TEMP_SOLUTION
static void sensor_write_work(struct work_struct *work)
{
#if 0
	int fd = sys_open(OffsetFileName, O_CREAT | O_RDWR, 0600);
	if (fd < 0)
		dprintk("sys_open %s error!!.\n", "/data/battery.txt");
	else{
		sys_write(fd, m_work_info.buffer, m_work_info.len);
		sys_close(fd);
	}
#else
	unsigned int orgfs;
	struct file *filep;
	int ret;
	struct work_info *pWorkInfo;

	orgfs = get_fs();
	set_fs(KERNEL_DS);

	pWorkInfo = container_of((struct delayed_work *)work,
		struct work_info, write_work);
	if (pWorkInfo == NULL)
		return;

	filep = filp_open(OffsetFileName, O_RDWR|O_CREAT, 0600);
	if (IS_ERR(filep))
		ret =  -1;
	else {
		filep->f_op->write(filep, pWorkInfo->buffer, pWorkInfo->len,
			&filep->f_pos);
		filp_close(filep, NULL);
		ret = 0;
	}

	set_fs(orgfs);
	pWorkInfo->rst = ret;
	complete(&pWorkInfo->completion);
#endif
}

static void sensor_read_work(struct work_struct *work)
{
#if 0
	int fd = sys_open(OffsetFileName, O_RDONLY, 0600);
	if (fd < 0)
		dprintk("sys_open %s error!!.\n", "/data/battery.txt");
	else{
		m_work_info.len = sys_read(fd, m_work_info.buffer,
			sizeof(m_work_info.buffer));
		sys_close(fd);
	}
	complete(&m_work_info.completion);
#else
	unsigned int orgfs;
	struct file *filep;
	int ret;
	struct work_info *pWorkInfo;

	orgfs = get_fs();
	set_fs(KERNEL_DS);

	pWorkInfo = container_of((struct delayed_work *)work,
		struct work_info, read_work);
	if (pWorkInfo == NULL)
		return;

	filep = filp_open(OffsetFileName, O_RDONLY, 0600);
	if (IS_ERR(filep)) {
		set_fs(orgfs);
		ret =  -1;
	} else {

		filep->f_op->read(filep, pWorkInfo->buffer,
			sizeof(pWorkInfo->buffer), &filep->f_pos);
		filp_close(filep, NULL);
		set_fs(orgfs);
		ret = 0;
	}

	pWorkInfo->rst = ret;
	complete(&(pWorkInfo->completion));
#endif
}

static int sensor_sync_read(unsigned short *x, unsigned short *y,
	unsigned short *z)
{
	int err;
	struct work_info *pWorkInfo = &m_work_info;

	init_completion(&pWorkInfo->completion);

	queue_delayed_work(pWorkInfo->wq, &(pWorkInfo->read_work),
		msecs_to_jiffies(0));
	err = wait_for_completion_timeout(&(pWorkInfo->completion),
		msecs_to_jiffies(2000));
	if (err == 0)
		return -1;

	if (pWorkInfo->rst != 0)
		return pWorkInfo->rst;

	if (sscanf(m_work_info.buffer, "%hu %hu %hu", x, y, z) != 3)
		return -1;

	return 0;
}

static int sensor_sync_write(unsigned short x, unsigned short y,
	unsigned short z)
{
	int err;
	char data[OFFSET_VAL_LEN];
	struct work_info *pWorkInfo = &m_work_info;

	init_completion(&pWorkInfo->completion);
	sprintf(data, "%4d %4d %4d", x, y, z);
	memcpy(pWorkInfo->buffer, data, OFFSET_VAL_LEN);
	pWorkInfo->len = OFFSET_VAL_LEN;

	queue_delayed_work(pWorkInfo->wq, &pWorkInfo->write_work,
		msecs_to_jiffies(0));
	err = wait_for_completion_timeout(&pWorkInfo->completion,
		msecs_to_jiffies(2000));
	if (err == 0)
		return -1;

	if (pWorkInfo->rst != 0)
		return pWorkInfo->rst;

	if (sscanf(pWorkInfo->buffer, "%hu %hu %hu", &x, &y, &z) != 3)
		return -1;
	return 0;
}

static int mm3a310_write_offset_to_file(unsigned short x,
	unsigned short y, unsigned short z)
{
#if 1
	return sensor_sync_write(x, y, z);
#else
	char data[OFFSET_VAL_LEN];
	unsigned int orgfs;
	struct file *filep;
	int ret;

	sprintf(data, "%4d %4d %4d", x, y, z);

	orgfs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(OffsetFileName, O_WRONLY|O_CREAT, 0777);
	if (IS_ERR(filep))
		ret =  -1;
	else {
		filep->f_op->write(filep, data, OFFSET_VAL_LEN, &filep->f_pos);
		filp_close(filep, NULL);
		ret = 0;
	}

	set_fs(orgfs);
	return ret;
#endif
}

static int mm3a310_read_offset_from_file(unsigned short *x, unsigned short *y,
	unsigned short *z)
{
#if 1
	return sensor_sync_read(x, y, z);

#else
	unsigned int orgfs;
	char data[OFFSET_VAL_LEN];
	struct file *filep;



	orgfs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(OffsetFileName, O_RDONLY, 0);
	if (IS_ERR(filep)) {
		set_fs(orgfs);
		return -1;
	}

	filep->f_op->read(filep, data, OFFSET_VAL_LEN, &filep->f_pos);
	filp_close(filep, NULL);

	if (sscanf(data, "%hu %hu %hu", x, y, z) != 3) {
		set_fs(orgfs);
		return -1;
	}
	set_fs(orgfs);
	return 0;
#endif
}

static void mm3a310_write_offset(unsigned short x, unsigned short y,
	unsigned short z)
{
	u8  xl, xh, yl, yh, zl, zh;
	int result = 0;

	struct i2c_client *client = mm3a310_i2c_client;

	xl = x & 0xff;
	xh = x >> 8;
	yl = y & 0xff;
	yh = y >> 8;
	zl = z & 0xff;
	zh = z >> 8;
	result = mm3a310_write_register(client, MM3A310_OTP_XOFF_L, xl);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_XOFF_H, xh);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_YOFF_L, yl);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_YOFF_H, yh);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_ZOFF_L, zl);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_ZOFF_H, zh);
	MI_ASSERT(result == 0);
}

static void manual_load_cali_file(void)
{
	static bool bLoad;

	unsigned short  offset[3];

	if (!bLoad) {
		if (!mm3a310_read_offset_from_file(&offset[0],
			&offset[1], &offset[2])) {
			mm3a310_write_offset(offset[0], offset[1], offset[2]);
			bLoad = true;
		}
	}
}

#if MM3A310_AUTO_CALIBRAE
static bool check_califile_exist(void)
{
	unsigned int orgfs = 0;
	bool ret = true;
	struct file *filep;

	orgfs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(OffsetFileName, O_RDONLY, 0600);
	if (IS_ERR(filep)) {
		set_fs(orgfs);
		ret = false;
	} else {
		filp_close(filep, NULL);
		set_fs(orgfs);
	}

	return ret;
}

static int auto_calibrate(void)
{

	struct mm3a310_cali_s mm3a310_cali_data;

	int ret = 0;

	memset(&mm3a310_cali_data, 0, sizeof(mm3a310_cali_data));

	mm3a310_cali_data.z_dir = 0;

	if (mm3a310_calibrate(&mm3a310_cali_data))
		ret =  -1;

	return ret;

}
#endif

/**
 * @brief  read X Y Z aixs by mg, but actually it is 1000/1024 mg
 * @retval result of the initailzation, 0 means successful; else failed
 */
static int cycle_read_xyz(int *x, int *y, int *z, int ncycle)
{
	u8      tmp_data[6];
	int j;


	*x = 0;
	*y = 0;
	*z = 0;
	for (j = 0; j < ncycle; j++) {
		mdelay(10);
		if (mm3a310_read_register_continuously(mm3a310_i2c_client,
			MM3A310_OUT_X_L, 6, tmp_data) < 6)
			return -1;
		(*x) += ((short)((tmp_data[1] << 8)|tmp_data[0]) >> 4);
		(*y) += ((short)((tmp_data[3] << 8)|tmp_data[2]) >> 4);
		(*z) += ((short)((tmp_data[5] << 8)|tmp_data[4]) >> 4);
	}
	(*x) /= ncycle;
	(*y) /= ncycle;
	(*z) /= ncycle;

	switch (mm3a_status.mode) {
	case MODE_2G:
		break;
	case MODE_4G:
		(*x) = (*x)<<1;
		(*y) = (*y)<<1;
		(*z) = (*z)<<1;
		break;
	case MODE_8G:
		(*x) = (*x)<<2;
		(*y) = (*y)<<2;
		(*z) = (*z)<<2;
		break;
	case MODE_16G:
		(*x) = (*x)<<3;
		(*y) = (*y)<<3;
		(*z) = (*z)<<3;
		break;
	default:
		return -1;
	}

	return 0;
}


static int check_linearity_offset(void){
	unsigned short i;
	int result = 0;
	struct i2c_client *client = mm3a310_i2c_client;
	int     x = 0, y = 0, z = 0;

	for (i = 0; i <= 0x3ff; i++) {
		result |= mm3a310_write_register(client,
			MM3A310_OTP_XOFF_L, i & 0xFF);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_XOFF_H, (i & 0xFF00) >> 8);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_YOFF_L, i & 0xFF);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_YOFF_H, (i & 0xFF00) >> 8);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_ZOFF_L, i & 0xFF);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_ZOFF_H, (i & 0xFF00) >> 8);
		result |= cycle_read_xyz(&x, &y, &z, 5);

		if (result)
			return result;
	}

	return result;
}


struct  linearitydata {
	unsigned short  off;
	int	val;
};

static int detect_linearity_ratio(int *xr, int *yr, int *zr)
{

	unsigned short i;
	int result = 0;
	struct i2c_client *client = mm3a310_i2c_client;
	int     x = 0, y = 0, z = 0;

	struct linearitydata xdata[2] = {{0}, {0} };
	u8              xdata_count = 0;
	struct linearitydata ydata[2] = {{0}, {0} };
	u8              ydata_count = 0;
	struct linearitydata zdata[2] = {{0}, {0} };
	u8              zdata_count = 0;

	for (i = 10; i <= 0x3ff; i += 50) {
		result |= mm3a310_write_register(client,
			MM3A310_OTP_XOFF_L, i & 0xFF);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_XOFF_H, (i & 0xFF00) >> 8);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_YOFF_L, i & 0xFF);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_YOFF_H, (i & 0xFF00) >> 8);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_ZOFF_L, i & 0xFF);
		result |= mm3a310_write_register(client,
			MM3A310_OTP_ZOFF_H, (i & 0xFF00) >> 8);
		result |= cycle_read_xyz(&x, &y, &z, 20);

		if (result)
			return result;

		if (abs(x) < 1800 && xdata_count < 2) {
			xdata[xdata_count].val = x;
			xdata[xdata_count].off = i;
			xdata_count++;
		}

		if (abs(y) < 1800 && ydata_count < 2) {
			ydata[ydata_count].val = y;
			ydata[ydata_count].off = i;
			ydata_count++;
		}

		if (abs(z) < 1800 && zdata_count < 2) {
			zdata[zdata_count].val = z;
			zdata[zdata_count].off = i;
			zdata_count++;
		}

		if (xdata_count == 2 && ydata_count == 2 && zdata_count == 2) {
			*xr = (xdata[1].val - xdata[0].val)/(xdata[1].off -
				xdata[0].off);
			*yr = (ydata[1].val - ydata[0].val)/(ydata[1].off -
				ydata[0].off);
			*zr = (zdata[1].val - zdata[0].val)/(zdata[1].off -
				zdata[0].off);

			return 0;
		}
	}

	return -1;

}


static int mm3a310_calibrate(struct mm3a310_cali_s *mm3a310_cali_data)
{
	short   i;
	u8      tmp_data[6];
	int     x = 0, y = 0, z = 0;
	int    xLine = 0, yLine = 0, zLine = 0;
	short     tmp_off = 0, tmp_off2 = 0;
	u8      ncycle = 50;

#if MM3A310_STK_TEMP_SOLUTION
	short   x_off_original = 0;
	short   y_off_original = 0;
	short   z_off_original = 0;
#endif

	int result;

	struct i2c_client *client = mm3a310_i2c_client;

	if (is_cali)
		return -1;

	is_cali = 1;

	/* decide the z direction, if 0 which means auto */
	if (mm3a310_cali_data->z_dir == 0) {
		result = cycle_read_xyz(&x, &y, &z, 5);
		if (result != 0)
			goto fail_exit;

		if (z > 0)
			mm3a310_cali_data->z_dir = 1;
		else
			mm3a310_cali_data->z_dir = -1;
	}

	if (mm3a310_read_register_continuously(client,
		MM3A310_OTP_XOFF_L, 6, tmp_data) != 6)
		goto fail_exit;

	mm3a310_cali_data->x_off = (tmp_data[1] << 8) | tmp_data[0];
	mm3a310_cali_data->y_off = (tmp_data[3] << 8) | tmp_data[2];
	mm3a310_cali_data->z_off = (tmp_data[5] << 8) | tmp_data[4];

#if MM3A310_STK_TEMP_SOLUTION
	x_off_original = mm3a310_cali_data->x_off;
	y_off_original = mm3a310_cali_data->y_off;
	z_off_original = mm3a310_cali_data->z_off;
#endif


	if (0 != detect_linearity_ratio(&xLine, &yLine, &zLine))
		xLine = yLine = zLine = -20;

	result = mm3a310_write_register(client, MM3A310_OTP_XOFF_L,
		mm3a310_cali_data->x_off & 0xFF);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_XOFF_H,
		(mm3a310_cali_data->x_off & 0xFF00) >> 8);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_YOFF_L,
		mm3a310_cali_data->y_off & 0xFF);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_YOFF_H,
		(mm3a310_cali_data->y_off & 0xFF00) >> 8);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_ZOFF_L,
		mm3a310_cali_data->z_off & 0xFF);
	MI_ASSERT(result == 0);
	result = mm3a310_write_register(client, MM3A310_OTP_ZOFF_H,
		(mm3a310_cali_data->z_off & 0xFF00) >> 8);
	MI_ASSERT(result == 0);

	for (i = 0; i < 20 ; i++) {
		x = y = z = 0;

		result = cycle_read_xyz(&x, &y, &z, ncycle);
		if (result != 0)
			goto fail_exit;

		if (!mm3a310_cali_data->x_ok) {
			if (abs(x) <= THRESHOLD)
				mm3a310_cali_data->x_ok = 1;
			else {
				tmp_off = x/xLine;

				tmp_off2 = (short)mm3a310_cali_data->x_off -
					tmp_off;
				if (tmp_off2 > 0x3ff)
					tmp_off2 = 0x3ff;
				else if (tmp_off2 < 0)
					tmp_off2 = 0x01;

				mm3a310_cali_data->x_off =
					(unsigned short)tmp_off2;

				result = mm3a310_write_register(client,
					MM3A310_OTP_XOFF_L,
					mm3a310_cali_data->x_off & 0xFF);
				MI_ASSERT(result == 0);
				result = mm3a310_write_register(client,
					MM3A310_OTP_XOFF_H,
					(mm3a310_cali_data->x_off &
					0xFF00) >> 8);
				MI_ASSERT(result == 0);
			}

		}

		if (!mm3a310_cali_data->y_ok) {
			if (abs(y) <= THRESHOLD)
				mm3a310_cali_data->y_ok = 1;
			else {
				tmp_off = y/yLine;

				tmp_off2 = (short)mm3a310_cali_data->y_off -
					tmp_off;
				if (tmp_off2 > 0x3ff)
					tmp_off2 = 0x3ff;
				else if (tmp_off2 < 0)
					tmp_off2 = 0x01;

				mm3a310_cali_data->y_off =
					(unsigned short)tmp_off2;

				result = mm3a310_write_register(client,
					MM3A310_OTP_YOFF_L,
					mm3a310_cali_data->y_off & 0xFF);
				MI_ASSERT(result == 0);
				result = mm3a310_write_register(client,
					MM3A310_OTP_YOFF_H,
					(mm3a310_cali_data->y_off &
					0xFF00) >> 8);
				MI_ASSERT(result == 0);
			}

		}

		if (!mm3a310_cali_data->z_ok) {
			if (abs(z - (mm3a310_cali_data->z_dir > 0 ?
				1024 : -1024)) <= THRESHOLD)
				mm3a310_cali_data->z_ok = 1;
			else {
				tmp_off = (z - (mm3a310_cali_data->z_dir > 0 ?
					1024 : -1024)) / zLine;

				tmp_off2 = (short)mm3a310_cali_data->z_off -
					tmp_off;
				if (tmp_off2 > 0x3ff)
					tmp_off2 = 0x3ff;
				else if (tmp_off2 < 0)
					tmp_off2 = 0x01;

				mm3a310_cali_data->z_off =
					(unsigned short)tmp_off2;

				result = mm3a310_write_register(client,
					MM3A310_OTP_ZOFF_L,
					mm3a310_cali_data->z_off & 0xFF);
				MI_ASSERT(result == 0);
				result = mm3a310_write_register(client,
					MM3A310_OTP_ZOFF_H,
					(mm3a310_cali_data->z_off &
					0xFF00) >> 8);
				MI_ASSERT(result == 0);
			}

		}

		if (mm3a310_cali_data->x_ok && mm3a310_cali_data->y_ok &&
			mm3a310_cali_data->z_ok)
			goto success_exit;
	}

#if MM3A310_STK_TEMP_SOLUTION
	if (mm3a310_cali_data->x_ok + mm3a310_cali_data->y_ok +
		mm3a310_cali_data->z_ok  == 2) {

		if (mm3a310_cali_data->x_ok == 0) {
			mm3a310_cali_data->x_off = x_off_original;
			result = mm3a310_write_register(client,
				MM3A310_OTP_XOFF_L,
				mm3a310_cali_data->x_off & 0xFF);
			MI_ASSERT(result == 0);
			result = mm3a310_write_register(client,
				MM3A310_OTP_XOFF_H,
				(mm3a310_cali_data->x_off & 0xFF00) >> 8);
			MI_ASSERT(result == 0);

		} else
			if (mm3a310_cali_data->y_ok == 0) {

				mm3a310_cali_data->y_off = y_off_original;
				result = mm3a310_write_register(client,
					MM3A310_OTP_YOFF_L,
					mm3a310_cali_data->y_off & 0xFF);
				MI_ASSERT(result == 0);
				result = mm3a310_write_register(client,
					MM3A310_OTP_YOFF_H,
					(mm3a310_cali_data->y_off &
					0xFF00) >> 8);
				MI_ASSERT(result == 0);

			} else
				if (mm3a310_cali_data->z_ok == 0) {

					mm3a310_cali_data->z_off =
						z_off_original;
					result = mm3a310_write_register(client,
						MM3A310_OTP_ZOFF_L,
						mm3a310_cali_data->z_off &
						0xFF);
					MI_ASSERT(result == 0);
					result = mm3a310_write_register(client,
						MM3A310_OTP_ZOFF_H,
						(mm3a310_cali_data->z_off &
						0xFF00) >> 8);
					MI_ASSERT(result == 0);

				}

		goto success_exit;
	}
#endif

fail_exit:
	is_cali = 0;
	return -1;

success_exit:
	is_cali = 0;
	return mm3a310_write_offset_to_file(mm3a310_cali_data->x_off,
		mm3a310_cali_data->y_off, mm3a310_cali_data->z_off);
}
#endif /* !MM3A310_OFFSET_TEMP_SOLUTION */

/***************************************************************
 *
 * Initialization function
 *
 ***************************************************************/
static int mm3a310_preset_register(struct i2c_client *client)
{
	int result = 0;

	/* Full scale: 2G */
	result |= mm3a310_write_register(client, MM3A310_CTRL_REG4, MODE_2G);
	/* ODR=100Hz, X,Y,Z axis enable */
	result |= mm3a310_write_register(client, MM3A310_CTRL_REG1, 0x6f);
	/* Power on, DATA measurement enable, lDO swtich all on */
	result |= mm3a310_write_register(client, MM3A310_TEMP_CFG_REG, 0x88);
	result |= mm3a310_write_register(client, MM3A310_LDO_REG, 0x02);
	result |= mm3a310_write_register(client, MM3A310_OTP_TRIM_OSC, 0x27);
	result |= mm3a310_write_register(client, MM3A310_LPF_ABSOLUTE, 0x30);
	result |= mm3a310_write_register(client, MM3A310_TEMP_OFF1, 0x3f);
	result |= mm3a310_write_register(client, MM3A310_TEMP_OFF2, 0xff);
	result |= mm3a310_write_register(client, MM3A310_TEMP_OFF3, 0x0f);

#if MM3A310_OFFSET_TEMP_SOLUTION

#if MM3A310_AUTO_CALIBRAE
	if (check_califile_exist())
		manual_load_cali_file();
	else
		auto_calibrate();
#else
	manual_load_cali_file();
#endif

#endif

	return result;
}

static int mm3a310_otp(struct i2c_client *client)
{
	int result;

	result = mm3a310_write_register(client, MM3A310_TEMP_CFG_REG, 0x08);
	MI_ASSERT(result == 0);
	if (result != 0)
		return result;

	result = mm3a310_write_register(client, MM3A310_CTRL_REG5, 0x80);
	MI_ASSERT(result == 0);
	if (result != 0)
		return result;

	return result;
}

static int mm3a310_init_client(struct i2c_client *client)
{
	int result = 0;

#if MM3A310_OFFSET_TEMP_SOLUTION
	m_work_info.wq = create_singlethread_workqueue("oo");

	INIT_DELAYED_WORK(&m_work_info.read_work, sensor_read_work);
	INIT_DELAYED_WORK(&m_work_info.write_work, sensor_write_work);
#endif

	/* soft reset */
	result |= mm3a310_write_register(client, MM3A310_SOFT_RESET, 0xAA);
	mdelay(5);
	result |= mm3a310_write_register(client, MM3A310_SOFT_RESET, 0x00);
	mdelay(10);

	result |= mm3a310_otp(client);
	result |= mm3a310_preset_register(client);

	return result;
}

/***************************************************************
 *
 * Read sensor data from MM3A310
 *
 ***************************************************************/
static int mm3a310_read_data(short *x, short *y, short *z)
{
	u8    tmp_data[6];

#if MM3A310_OFFSET_TEMP_SOLUTION
	manual_load_cali_file();
#endif

	if (mm3a310_read_register_continuously(mm3a310_i2c_client,
		MM3A310_OUT_X_L, 6, tmp_data) < 6) {
		return -3;
	}

	*x = ((short)((tmp_data[1] << 8)) >> 4);
	*y = ((short)((tmp_data[3] << 8)) >> 4);
	*z = ((short)((tmp_data[5] << 8)) >> 4);

	switch (mm3a_status.mode) {
	case MODE_2G:
		break;
	case MODE_4G:
		(*x) = (*x)<<1;
		(*y) = (*y)<<1;
		(*z) = (*z)<<1;
		break;
	case MODE_8G:
		(*x) = (*x)<<2;
		(*y) = (*y)<<2;
		(*z) = (*z)<<2;
		break;
	case MODE_16G:
		(*x) = (*x)<<3;
		(*y) = (*y)<<3;
		(*z) = (*z)<<3;
		break;
	default:return -1;
	}


#if MM3A310_STK_TEMP_SOLUTION

	addAixHistory(*x, *y, *z);

	bxstk = isXStick();
	bystk = isYStick();
	bzstk = isZStick();

	if ((bxstk + bystk + bzstk) < 2) {
		if (bxstk)
			*x = squareRoot(1024*1024 - (*y)*(*y) - (*z)*(*z));
		if (bystk)
			*y = squareRoot(1024*1024 - (*x)*(*x) - (*z)*(*z));
		if (bzstk)
			*z = squareRoot(1024*1024 - (*x)*(*x) - (*y)*(*y));
	} else {

		return 0;
	}
#endif


#if FILTER_AVERAGE_ENHANCE
	*x = filter_average_enhance(&tFac[0], *x);
	*y = filter_average_enhance(&tFac[1], *y);
	*z = filter_average_enhance(&tFac[2], *z);
#endif

	return 0;
}

static void report_abs(void)
{
	short x = 0, y = 0, z = 0;
	int result = 0;

	/* check if any new data ready */
	result = mm3a310_read_register(mm3a310_i2c_client, MM3A310_STATUS_REG);
	if (!(result & 0x08))
		return;

	if (mm3a310_read_data(&x, &y, &z) != 0)
		return;

	/* adjust direction */
#if TARGET_PLATFORM == PLATFORM_QUACOMM
	input_report_rel(mm3a310_idev->input, REL_RX, x);
	input_report_rel(mm3a310_idev->input, REL_RY, y);
	input_report_rel(mm3a310_idev->input, REL_RZ, z);
	input_sync(mm3a310_idev->input);

#else
	aml_sensor_report_acc(mm3a310_i2c_client, mm3a310_idev->input, x, y, z);

#endif

#if COWORK_WITH_DM211
	gx = x;
	gy = y;
	gz = z;
#endif
}

static void mm3a310_dev_poll(struct input_polled_dev *dev)
{
#if MM3A310_OFFSET_TEMP_SOLUTION
	if (is_cali)
		return;
#endif

	dev->poll_interval = delayMs;
	report_abs();
}


/*
 * ============================================
 *      IOCTRL interface
 * ============================================
 */
static long mm3a310_misc_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int err = 0;
	int interval;
	bool bEnable;
	int range;
	short  xyz[3];
	struct mm3a310_cali_s mm3a310_cali_data;

	memset(&mm3a310_cali_data, 0, sizeof(struct mm3a310_cali_s));

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
			_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
			_IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	switch (cmd) {
#if COWORK_WITH_DM211
	case GSENSOR_IOCTL_READ_SENSORDATA:
		void *data = (void __user *) arg;
		char strbuf[256];

		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		sprintf(strbuf, "%04x %04x %04x", gx, gy, -gz);

		if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
			err = -EFAULT;
			break;
		}
		break;
#endif

	case MM3A310_ACC_IOCTL_GET_DELAY:
		interval = POLL_INTERVAL;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;

	case MM3A310_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < 0 || interval > 1000)
			return -EINVAL;
		if ((interval <= 30) && (interval > 10))
			interval = 10;
		delayMs = interval;
		break;

	case MM3A310_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&bEnable, argp, sizeof(bEnable)))
			return -EFAULT;

		err = mm3a310_set_enable(bEnable);
		if (err < 0)
			return -EINVAL;
		break;

	case MM3A310_ACC_IOCTL_GET_ENABLE:
		err = mm3a310_get_enable(&bEnable);
		if (err < 0)
			return -EINVAL;

		if (copy_to_user(argp, &bEnable, sizeof(EINVAL)))
			return -EINVAL;
		break;
	case MM3A310_ACC_IOCTL_SET_G_RANGE:
		if (copy_from_user(&range, argp, sizeof(range)))
			return -EFAULT;
		err = mm3a310_set_grange(range);
		if (err < 0)
			return -EINVAL;
		break;

	case MM3A310_ACC_IOCTL_GET_G_RANGE:
		err = mm3a310_get_grange(&range);
		if (err < 0)
			return -EINVAL;
		else
			if (copy_to_user(argp, &range, sizeof(range)))
				return -EINVAL;
		break;

#if MM3A310_OFFSET_TEMP_SOLUTION
	case MM3A310_ACC_IOCTL_CALIBRATION:
		if (copy_from_user(&mm3a310_cali_data,
			(struct mm3a310_cali_s *)arg,
			sizeof(struct mm3a310_cali_s)))
			return -EFAULT;

		if (mm3a310_calibrate(&mm3a310_cali_data))
			return -EFAULT;

		if (copy_to_user((struct mm3a310_cali_s *)arg,
			&mm3a310_cali_data, sizeof(struct mm3a310_cali_s)))
			return -EFAULT;
		break;

	case MM3A310_ACC_IOCTL_UPDATE_OFFSET:

		if (mm3a310_read_offset_from_file(&mm3a310_cali_data.x_off,
			&mm3a310_cali_data.y_off, &mm3a310_cali_data.z_off))
			return -EFAULT;
		/* update offset */
		mm3a310_write_offset(mm3a310_cali_data.x_off,
			mm3a310_cali_data.y_off, mm3a310_cali_data.z_off);
		break;
#endif /* !MM3A310_OFFSET_TEMP_SOLUTION */


	case MM3A310_ACC_IOCTL_GET_COOR_XYZ:

		if (mm3a310_read_data(&xyz[0], &xyz[1], &xyz[2]))
			return -EFAULT;

		if (copy_to_user((void __user *)arg, xyz, sizeof(xyz)))
			return -EFAULT;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* Misc device interface*/
static const struct file_operations mm3a310_misc_fops = {
	.owner = THIS_MODULE,
	/*.open = mm3a310_misc_open,*/
	/*.ioctl = mm3a310_misc_ioctl,*/
	.unlocked_ioctl = mm3a310_misc_ioctl,
};

static struct miscdevice misc_mm3a310 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MM3A310_MISC_NAME,
	.fops = &mm3a310_misc_fops,
};


/*
 * ============================================
 *      DEVICE ATTRIBUTE interface
 * ============================================
 */
static ssize_t mm3a310_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	bool bEnable;

	ret = mm3a310_get_enable(&bEnable);
	if (ret < 0)
		ret = -EINVAL;
	else
		ret = sprintf(buf, "%d\n", bEnable);
	return ret;
}

static ssize_t mm3a310_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	bool bEnable;
	unsigned long enable;

	if (buf == NULL)
		return -1;
	if (kstrtoul(buf, 10, &enable))
		return -EINVAL;
	bEnable = (enable > 0) ? true : false;

	ret = mm3a310_set_enable(bEnable);
	if (ret < 0)
		ret = -EINVAL;
	else
		ret = count;

	return ret;
}

static ssize_t mm3a310_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", delayMs);
}

static ssize_t mm3a310_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long interval = 0;

	if (kstrtoul(buf, 10, &interval))
		return -EINVAL;

	if (interval < 0 || interval > 1000)
		return -EINVAL;
	if ((interval <= 30) && (interval > 10))
		interval = 10;
	delayMs = interval;

	return count;
}

static ssize_t mm3a310_offset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count;
	u8 off[6];
	struct i2c_client *client;

	client = mm3a310_i2c_client;

	mm3a310_read_register_continuously(client, MM3A310_OTP_XOFF_L, 6, off);

	count = sprintf(buf, "%d,%d,%d,%d,%d,%d\n", off[0], off[1],
		off[2], off[3], off[4], off[5]);

	return count;
}

static ssize_t mm3a310_offset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	u8 off[6];
	int i;
	int result;
	struct i2c_client *client = mm3a310_i2c_client;

	result = sscanf(buf, "%c,%c,%c,%c,%c,%c\n", &off[0], &off[1], &off[2],
		&off[3], &off[4], &off[5]);
	if (result < 0)
		return -EINVAL;
	for (i = 0; i < 6; i++) {
		result = mm3a310_write_register(client,
			MM3A310_OTP_XOFF_L + i, off[i]);
		MI_ASSERT(result == 0);
	}
	return count;
}


#if FILTER_AVERAGE_ENHANCE
static ssize_t mm3a310_average_enhance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d %d %d\n", tFac[0].filter_param_l,
		tFac[0].filter_param_h, tFac[0].filter_threhold);

	return ret;
}

static ssize_t mm3a310_average_enhance_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d %d %d\n", &(tFac[0].filter_param_l),
		&(tFac[0].filter_param_h), &(tFac[0].filter_threhold));
	if (ret < 0)
		return -EINVAL;
	tFac[1].filter_param_l = tFac[2].filter_param_l =
		tFac[0].filter_param_l;
	tFac[1].filter_param_h = tFac[2].filter_param_h =
		tFac[0].filter_param_h;
	tFac[1].filter_threhold = tFac[2].filter_threhold =
		tFac[0].filter_threhold;

	return count;
}
/*FILTER_AVERAGE_ENHANCE*/
#endif

#if MM3A310_OFFSET_TEMP_SOLUTION
static ssize_t mm3a310_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d\n", bCaliResult);

	return ret;
}

static ssize_t mm3a310_calibrate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mm3a310_cali_s mm3a310_cali_data;
	unsigned long temp = 0;
	bCaliResult = 0;

	memset(&mm3a310_cali_data, 0, sizeof(mm3a310_cali_data));

	if (kstrtol(buf, 10, &temp))
		return -EINVAL;

	mm3a310_cali_data.z_dir = temp;
	bCaliResult = mm3a310_calibrate(&mm3a310_cali_data);
	return count;
}


static ssize_t mm3a310_linearity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t result = 0;
	result = sprintf(buf,
		"%s\n", "--help\n linearity data will be output by printk,so");
	result += sprintf(buf+result,
		"cat /proc/kmsg\n and write 1 linearity means offset check\n");
	return result;
}
static ssize_t mm3a310_linearity_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long  type = 0;

	if (kstrtoul(buf, 10, &type))
		return -EINVAL;

	if (type == 1)
		check_linearity_offset();

	return count;
}
#endif

static ssize_t mm3a310_aix_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result;
	short x, y, z;
	int count = 0;

	result = mm3a310_read_data(&x, &y, &z);
	if (result == 0)
		count += sprintf(buf+count, "x= %d;y=%d;z=%d", x, y, z);
	else
		count += sprintf(buf+count, "reading failed!");

	return count;
}

static ssize_t mm3a310_reg_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int addr, data;
	int result;
	struct i2c_client *client;

	client = mm3a310_i2c_client;

	result = sscanf(buf, "0x%x, 0x%x\n", &addr, &data);
	if (result < 0)
		return -EINVAL;

	result = mm3a310_write_register(client, addr, data);
	MI_ASSERT(result == 0);

	return count;
}

static ssize_t mm3a310_reg_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0;
	int i;
	short val;
	struct i2c_client *client;

	client = mm3a310_i2c_client;

	count += sprintf(buf+count, "---------page 0---------");
	for (i = 0; i <= 0x003d; i++) {
		if (i%16 == 0)
			count += sprintf(buf+count, "\n%02x\t", i);
		val = mm3a310_read_register(client, i);
		count += sprintf(buf+count, "%02X ", val);
	}

	count += sprintf(buf+count, "\n---------page 1---------");
	for (i = 0x0100; i <= 0x012a; i++) {
		if ((i&0xff)%16 == 0)
			count += sprintf(buf+count, "\n%02x\t", (i & 0xff));
		val = mm3a310_read_register(client, i);
		count += sprintf(buf+count, "%02X ", val);

	}
	count += sprintf(buf+count, "\n---------end---------\n");

	return count;
}

static ssize_t mm3a310_grange_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int range;

	ret = mm3a310_get_grange(&range);

	if (ret == 0)
		ret = sprintf(buf, "%d\n", range);
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t mm3a310_grange_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	int newrange;

	ret = sscanf(buf, "%d\n", &newrange);
	if (ret < 0)
		return -EINVAL;
	ret = mm3a310_set_grange(newrange);

	if (ret == 0)
		ret = count;
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t mm3a310_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int odr;

	ret = mm3a310_get_odr(&odr);
	if (ret < 0)
		ret = -EINVAL;
	else
		ret = sprintf(buf, "%d\n", odr);

	return ret;
}

static ssize_t mm3a310_odr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int odr;

	ret = sscanf(buf, "%d\n", &odr);
	if (ret < 0)
		return -EINVAL;

	ret = mm3a310_set_odr(odr);
	if (ret < 0)
		ret = -EINVAL;
	else
		ret = count;

	return ret;
}

static ssize_t mm3a310_log_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d\n", Log_level);

	return ret;
}

static ssize_t mm3a310_log_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long level = 0;

	if (kstrtoul(buf, 10, &level))
		return -EINVAL;

	Log_level = level;

	return count;
}


static ssize_t mm3a310_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%s\n", DRIVER_VERSION);
}



static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_enable_show, mm3a310_enable_store);
static DEVICE_ATTR(delay,      S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_delay_show, mm3a310_delay_store);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_delay_show, mm3a310_delay_store);
static DEVICE_ATTR(offset, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_offset_show, mm3a310_offset_store);
static DEVICE_ATTR(aix_data,    S_IRUGO,
	mm3a310_aix_data_show, NULL);
static DEVICE_ATTR(reg_data,     S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_reg_data_show, mm3a310_reg_data_store);
static DEVICE_ATTR(grange,      S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_grange_show, mm3a310_grange_store);
static DEVICE_ATTR(odr,         S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_odr_show, mm3a310_odr_store);
#if MM3A310_OFFSET_TEMP_SOLUTION
static DEVICE_ATTR(calibrate,   S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_calibrate_show, mm3a310_calibrate_store);
static DEVICE_ATTR(linearity,  0660,
	mm3a310_linearity_show, mm3a310_linearity_store);
#endif
#if FILTER_AVERAGE_ENHANCE
static DEVICE_ATTR(average_enhance, S_IRUGO|S_IWUSR | S_IWGRP | S_IWOTH,
	mm3a310_average_enhance_show, mm3a310_average_enhance_store);
#endif /* ! FILTER_AVERAGE_ENHANCE */

static DEVICE_ATTR(log_level,  0660,
	mm3a310_log_level_show, mm3a310_log_level_store);
static DEVICE_ATTR(version,  0660,
	mm3a310_version_show, NULL);


static struct attribute *mm3a310_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_offset.attr,
	&dev_attr_aix_data.attr,
	&dev_attr_reg_data.attr,
	&dev_attr_grange.attr,
	&dev_attr_odr.attr,
#if MM3A310_OFFSET_TEMP_SOLUTION
	&dev_attr_calibrate.attr,
	&dev_attr_linearity.attr,
#endif
#if FILTER_AVERAGE_ENHANCE
	&dev_attr_average_enhance.attr,
#endif /* ! FILTER_AVERAGE_ENHANCE */
	&dev_attr_log_level.attr,
	&dev_attr_version.attr,
	NULL
};

#if 0
static struct device_attribute *mm3a310_device_attributes[] = {
	&dev_attr_enable,
	&dev_attr_delay,
	&dev_attr_poll_delay,
	&dev_attr_offset,
	&dev_attr_aix_data,
	&dev_attr_reg_data,
	&dev_attr_grange,
	&dev_attr_odr,
#if MM3A310_OFFSET_TEMP_SOLUTION
	&dev_attr_calibrate_mm3a310,
	&dev_attr_linearity,
#endif

#if FILTER_AVERAGE_ENHANCE
	&dev_attr_average_enhance,
#endif

	&dev_attr_log_level,
	&dev_attr_version,

	NULL
};
#endif

static const struct attribute_group mm3a310_attr_group = {
	/*.name   = "mm3a310",*/
	.attrs  = mm3a310_attributes,
};

/*
 * I2C init/probing/exit functions
 */

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mm3a310_early_suspend(struct early_suspend *es);
static void mm3a310_early_resume(struct early_suspend *es);
#endif

static int mm3a310_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int result;
	struct input_dev *idev;
	struct i2c_adapter *adapter;

	mm3a310_i2c_client = client;
	adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter,
			I2C_FUNC_SMBUS_BYTE |
			I2C_FUNC_SMBUS_BYTE_DATA);
	MI_ASSERT(result);


	result = mm3a310_read_register(client, MM3A310_WHO_AM_I);

	/*compare the address value */
	if (MM3A310_ID != (result)) {
		result = -EINVAL;
		goto err_detach_client;
	}

	/* Initialize the MM3A310 chip */
	result = mm3a310_init_client(client);
	if (result != 0)
		goto err_detach_client;

	/* input poll device register */
	mm3a310_idev = input_allocate_polled_device();
	if (!mm3a310_idev) {
		result = -ENOMEM;
		goto err_hwmon_device_unregister;
	}
	mm3a310_idev->poll = mm3a310_dev_poll;
	mm3a310_idev->poll_interval = POLL_INTERVAL;
	delayMs = POLL_INTERVAL;
	mm3a310_idev->poll_interval_max = POLL_INTERVAL_MAX;
	idev = mm3a310_idev->input;

	idev->name = MM3A310_DRV_NAME;
	idev->id.bustype = BUS_I2C;
	idev->evbit[0] = BIT_MASK(EV_ABS);

#if TARGET_PLATFORM == PLATFORM_QUACOMM
	/* X */
	input_set_capability(idev, EV_REL, REL_RX);
	input_set_abs_params(idev, REL_RX, ABSMIN_2G, ABSMAX_2G, 0, 0);
	/* Y */
	input_set_capability(idev, EV_REL, REL_RY);
	input_set_abs_params(idev, REL_RY, ABSMIN_2G, ABSMAX_2G, 0, 0);
	/* Z */
	input_set_capability(idev, EV_REL, REL_RZ);
	input_set_abs_params(idev, REL_RZ, ABSMIN_2G, ABSMAX_2G, 0, 0);

#else
	input_set_abs_params(idev, ABS_X, -16384, 16383,
		INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(idev, ABS_Y, -16384, 16383,
		INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(idev, ABS_Z, -16384, 16383,
		INPUT_FUZZ, INPUT_FLAT);

#endif

	result = input_register_polled_device(mm3a310_idev);
	if (result)
		goto err_free_polled_device;

	/* Sys Attribute Register */
	result = sysfs_create_group(&idev->dev.kobj, &mm3a310_attr_group);
	if (result) {
		result = -EINVAL;
		goto err_unregister_polled_device;
	}

	/* Misc device interface Register */
	result = misc_register(&misc_mm3a310);
	if (result)
		goto err_remove_sysfs_group;

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = mm3a310_early_suspend;
	early_suspend.resume  = mm3a310_early_resume;
	early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&early_suspend);
#endif

	return result;

err_remove_sysfs_group:
	sysfs_remove_group(&idev->dev.kobj, &mm3a310_attr_group);
err_unregister_polled_device:
	input_unregister_polled_device(mm3a310_idev);
err_free_polled_device:
	input_free_polled_device(mm3a310_idev);
err_hwmon_device_unregister:
	/*hwmon_device_unregister(&client->dev);    */
err_detach_client:
	return result;
}

static int mm3a310_remove(struct i2c_client *client)
{
	int result;
	mm3a_status.temp_cfg_reg =
		mm3a310_read_register(client, MM3A310_TEMP_CFG_REG);
	result = mm3a310_write_register(client, MM3A310_TEMP_CFG_REG,
		mm3a_status.temp_cfg_reg | 0x20); /* Power down enable */
	MI_ASSERT(result == 0);

	misc_deregister(&misc_mm3a310);

	sysfs_remove_group(&mm3a310_idev->input->dev.kobj, &mm3a310_attr_group);

	input_unregister_polled_device(mm3a310_idev);

	input_free_polled_device(mm3a310_idev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif

	return result;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mm3a310_early_suspend(struct early_suspend *es)
{
	int result;
	if (is_cali)
		return;
	mm3a_status.temp_cfg_reg =
		mm3a310_read_register(mm3a310_i2c_client, MM3A310_TEMP_CFG_REG);
	result = mm3a310_write_register(mm3a310_i2c_client,
		MM3A310_TEMP_CFG_REG,
		mm3a_status.temp_cfg_reg | 0x20); /* Power down enable */
	MI_ASSERT(result == 0);
	return;
}

static void mm3a310_early_resume(struct early_suspend *es)
{
	int result;

	if (is_cali)
		return;

	result = mm3a310_write_register(mm3a310_i2c_client,
		MM3A310_TEMP_CFG_REG, mm3a_status.temp_cfg_reg);
	MI_ASSERT(result == 0);
	return;
}
#endif

#ifndef CONFIG_HAS_EARLYSUSPEND
static int mm3a310_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int result;
	mm3a_status.temp_cfg_reg =
		mm3a310_read_register(client, MM3A310_TEMP_CFG_REG);
	result = mm3a310_write_register(client, MM3A310_TEMP_CFG_REG,
		mm3a_status.temp_cfg_reg | 0x20); /* Power down enable */
	MI_ASSERT(result == 0);
	return result;
}

static int mm3a310_resume(struct i2c_client *client)
{
	int result;
	result = mm3a310_write_register(client, MM3A310_TEMP_CFG_REG,
		mm3a_status.temp_cfg_reg);
	MI_ASSERT(result == 0);
	return result;
}
#endif
#if 0
static int mm3a310_detect(struct i2c_client *new_client,
		struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = new_client->adapter;
	/*int address = new_client->addr;*/
	/*const char *name = NULL;*/
	/*int man_id, chip_id, reg_config1, reg_convrate;*/

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	strlcpy(info->type, "da311", I2C_NAME_SIZE);
	/*strlcpy(info->type, MM3A310_DRV_NAME, I2C_NAME_SIZE);*/

	return 0;
}
#endif
static const struct i2c_device_id mm3a310_id[] = {
	{ MM3A310_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mm3a310_id);


static const unsigned short normal_i2c[] = {
	0x27,  0x26, I2C_CLIENT_END };

static struct i2c_driver mm3a310_driver = {
	/*.class		= I2C_CLASS_HWMON,*/
	.driver = {
		.name    = MM3A310_DRV_NAME,
		.owner    = THIS_MODULE,
	},
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = mm3a310_suspend,
	.resume    = mm3a310_resume,
#endif
	.probe    = mm3a310_probe,
	.remove    = mm3a310_remove,
	.id_table = mm3a310_id,

	/*.detect = mm3a310_detect,*/
	/*.address_list	= normal_i2c,*/
};

/* comment this if you register this in board info */
#if  DEVICE_CREATE_MODE == DEVICE_CREATE_BYSELF
int i2c_static_add_device(struct i2c_board_info *info)
{
	struct i2c_adapter *adapter;
	struct i2c_client  *client;
	int    ret;

	adapter = i2c_get_adapter(I2C_STATIC_BUS_NUM);
	if (!adapter) {
		ret = -ENODEV;
		goto i2c_err;
	}

	client = i2c_new_device(adapter, info);
	if (!client) {
		ret = -ENODEV;
		goto i2c_err;
	}

	i2c_put_adapter(adapter);

	return 0;

i2c_err:
	return ret;
}
#endif /* MODULE */

static int __init mm3a310_init(void)
{
	int res;

#if FILTER_AVERAGE_ENHANCE
	/* configure default filter param */
	int i;
	for (i = 0; i < 3; i++) {
		tFac[i].filter_param_l = 2;
		tFac[i].filter_param_h = 8;
		tFac[i].filter_threhold = 60;

		tFac[i].refN_l = 0;
		tFac[i].refN_h = 0;
	}
#endif

	/* comment this if you register this in board info */
#if  DEVICE_CREATE_MODE == DEVICE_CREATE_BYSELF
	res = i2c_static_add_device(&mm3a310_i2c_boardinfo);
	/*res = i2c_register_board_info(I2C_STATIC_BUS_NUM,
	&mm3a310_i2c_boardinfo, 1);*/
	if (res < 0)
		return res;
#endif

	res = i2c_add_driver(&mm3a310_driver);
	if (res < 0)
		return -ENODEV;
	return res;
}

static void __exit mm3a310_exit(void)
{
#if  DEVICE_CREATE_MODE == DEVICE_CREATE_BYSELF
	i2c_unregister_device(mm3a310_i2c_client);
#endif
	i2c_del_driver(&mm3a310_driver);
}


MODULE_AUTHOR("MiraMEMS <chqian@miramems.com>");
MODULE_DESCRIPTION("MM3A310 3-Axis Accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

module_init(mm3a310_init);
module_exit(mm3a310_exit);
