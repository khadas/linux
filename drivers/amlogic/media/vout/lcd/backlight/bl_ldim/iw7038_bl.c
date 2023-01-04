// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define NORMAL_MSG      (~BIT(7) & 0xff)
#define BROADCAST_MSG   BIT(7)
#define WRITE_MSG       (~BIT(7) & 0xff)
#define READ_MSG        BIT(7)
#define BLOCK_DATA      (~BIT(6) & 0xff)
#define SINGLE_DATA     BIT(6)
#define IW7038_DEV_ADDR 1

#define IW7038_REG_BRIGHTNESS_CHK  0x00
#define IW7038_REG_BRIGHTNESS      0x01
#define IW7038_REG_CHIPID          0x7f
#define IW7038_CHIPID              0x28

#define VSYNC_INFO_FREQUENT        30

static int iw7038_on_flag;
static unsigned short vsync_cnt;
static unsigned short fault_cnt;

static DEFINE_MUTEX(iw7038_spi_mutex);

struct iw7038_s {
	int test_mode;
	struct spi_device *spi;
	int cs_hold_delay;
	int cs_clk_delay;
	unsigned char cmd_size;
	unsigned char *init_data;
	unsigned int init_data_cnt;
	struct class cls;
};

struct iw7038_s *bl_iw7038;

static unsigned short *test_brightness;
static unsigned char *val_brightness;
static unsigned char *tbuf;
/*********************************************************************/
/*iw7038 register write: R/W(0/1) | reserved(00000) | regaddr[9:0]*/
/*********************************************************************/
static int iw7038_wreg(struct spi_device *spi,
		       unsigned int devaddr,
		       unsigned int addr,
		       unsigned char val)
{
	unsigned char addr_h;
	unsigned char addr_l;
	int ret, i;

	addr_h = (addr >> 8) & WRITE_MSG;
	addr_l = addr & 0xff;
	/*broadcast: */
	/*0x00: the same data for all device*/
	/*0x3f: the different data for all device*/
	if (!devaddr || devaddr == 0x3f)
		tbuf[0] = devaddr | BROADCAST_MSG | SINGLE_DATA;
	else
		tbuf[0] = (devaddr & NORMAL_MSG) | SINGLE_DATA;

	tbuf[1] = addr_h;
	tbuf[2] = addr_l;
	tbuf[3] = val;
	for (i = 0; i < 4; i++)
		tbuf[i + 4] = 0x00;

	ret = ldim_spi_write(spi, tbuf, 8);

	return ret;
}

static int iw7038_wregs(struct spi_device *spi,
			unsigned char devaddr,
			unsigned int addr,
			unsigned char *val,
			int len)
{
	int ret;
	unsigned char addr_h;
	unsigned char addr_l;
	unsigned char i, k;
	unsigned short data_len = 1;

	if (ldim_debug_print)
		LDIMPR("%s: 0x%02x = 0x%p len = %d\n",
		       __func__, addr, val, len);
	/*0x3f write one diff data to same reg */
	if (devaddr > 0x3f || !len || len > 128)
		return -1;

	if (len == 1 && devaddr == 0x3f) {
		//write single data to same reg of all spi
		tbuf[0] = devaddr | BROADCAST_MSG | SINGLE_DATA;
		addr_h = (addr >> 8) & WRITE_MSG;
		addr_l = addr & 0xff;
		tbuf[1] = addr_h;
		tbuf[2] = addr_l;
		data_len = len * 4; //spi_count is 4
		memcpy(&tbuf[3], val, data_len);
		for (i = 1; i < 4; i++) //spi_count - 1
			tbuf[3 + data_len + i] = 0x00;
	} else {
		addr_h = (addr >> 8) & WRITE_MSG;
		addr_l = addr & 0xff;
		if (devaddr == 0x3f) {
			tbuf[0] = (devaddr | BROADCAST_MSG) & BLOCK_DATA;
			data_len = len * 4;//spi_num is 4
		} else if (!devaddr) {
			tbuf[0] = (devaddr | BROADCAST_MSG) & BLOCK_DATA;
			data_len = len;
		} else {
			tbuf[0] = devaddr & NORMAL_MSG & BLOCK_DATA;
			data_len = len;
		}
		tbuf[1] = len;
		tbuf[2] = addr_h;
		tbuf[3] = addr_l;
		memcpy(&tbuf[4], val, data_len);
		for (i = 0; i < 3; i++)//spi_num - 1
			tbuf[4 + data_len + i] = 0x00;
	}

	if (ldim_debug_print) {
		for (k = 0; k < (4 + data_len + 3); k++)
			LDIMPR("%s: send tbuf[%d] = 0x%02x\n",
			       __func__, k, tbuf[k]);
	}

	ret = ldim_spi_write(spi, tbuf, 4 + data_len + 3);

	return ret;
}

static int iw7038_rreg(struct spi_device *spi,
		       unsigned char devaddr,
		       unsigned int addr,
		       unsigned char *val)
{
	unsigned char rbuf[8];
	int ret, i, k;
	unsigned char addr_h, addr_l;

	addr_h = (addr >> 8) | READ_MSG;
	addr_l = addr & 0xff;

	if (!devaddr || devaddr == 0x3f)
		tbuf[0] = devaddr | BROADCAST_MSG | SINGLE_DATA;
	else
		tbuf[0] = (devaddr & NORMAL_MSG) | SINGLE_DATA;

	tbuf[1] = addr_h;
	tbuf[2] = addr_l;

	for (i = 0; i < 5; i++)
		tbuf[i + 3] = 0x00;

	if (ldim_debug_print) {
		for (k = 0; k < 8; k++)
			LDIMPR("%s: send tbuf[%d] = 0x%02x\n",
			       __func__, k, tbuf[k]);
	}

	ret = ldim_spi_read_sync(spi, tbuf, rbuf, 8);
	*val = rbuf[7];

	/*for(k=0; k<8; k++)
	 *	LDIMPR("%s: read rbuf[%d] = 0x%02x\n",
	 *	       __func__, k, rbuf[k]);
	 */

	return ret;
}

static int iw7038_rregs(struct spi_device *spi,
			unsigned char devaddr,
			unsigned int addr,
			unsigned char *val,
			int len)
{
	int ret, i;
	unsigned char addr_h, addr_l;

	addr_h = (addr >> 8) | READ_MSG;
	addr_l = addr & 0xff;

	if (!devaddr || devaddr == 0x3f) {
		LDIMERR("%s: unsupport devaddr: %d\n", __func__, devaddr);
		return -1;
	}

	if (len < 2) {
		LDIMERR("%s: invalid len: %d\n", __func__, len);
		return -1;
	}

	tbuf[0] = devaddr & NORMAL_MSG & BLOCK_DATA;
	tbuf[1] = len;
	tbuf[2] = addr_h;
	tbuf[3] = addr_l;
	for (i = 0; i < (len + 4); i++)
		tbuf[i + 4] = 0x00;
	ret = ldim_spi_read_sync(spi, tbuf, val, (len + 8));
	return ret;
}

static int iw7038_reg_write(unsigned int dev_id,
			    unsigned char *buf,
			    unsigned int len)
{
	int ret;

	if (len > 32 || len < 1) {
		LDIMERR("%s: unsupport len: %d\n", __func__, len);
		return -1;
	}

	if (len > 1)
		ret = iw7038_wregs(bl_iw7038->spi, dev_id, buf[0], &buf[1],
				   len);
	else
		ret = iw7038_wreg(bl_iw7038->spi, dev_id, buf[0], buf[0]);
	return ret;
}

static int iw7038_reg_read(unsigned int dev_id, unsigned char *buf,
			   unsigned int len)
{
	int ret = 0;

	if (len > 1) {
		LDIMERR("%s: unsupport len: %d\n", __func__, len);
		return -1;
	}

	ret = iw7038_rreg(bl_iw7038->spi, dev_id, buf[0],
			  &buf[0]);

	return ret;
}

static int ldim_power_cmd_dynamic_size(void)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	table = bl_iw7038->init_data;
	max_len = bl_iw7038->init_data_cnt;

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (ldim_debug_print) {
			LDIMPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			       __func__, step, type, table[i + 1]);
		}
		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				ldim_delay(delay_ms);
		} else if (type == LCD_EXT_CMD_TYPE_CMD) {
			ret = iw7038_wreg(bl_iw7038->spi, 0x0,
					  table[i + 2], table[i + 3]);
			usleep_range(1, 2);
		} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			ret = iw7038_wreg(bl_iw7038->spi, 0x0,
					  table[i + 2], table[i + 3]);
			usleep_range(1, 2);
			if (table[i + 4] > 0)
				ldim_delay(table[i + 4]);
		} else {
			LDIMERR("%s: type 0x%02x invalid\n", __func__, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int ldim_power_cmd_fixed_size(void)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	cmd_size = bl_iw7038->cmd_size;
	if (cmd_size < 2) {
		LDIMERR("%s: invalid cmd_size %d\n", __func__, cmd_size);
		return -1;
	}

	table = bl_iw7038->init_data;
	max_len = bl_iw7038->init_data_cnt;

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (ldim_debug_print) {
			LDIMPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			       __func__, step, type, cmd_size);
		}
		if (type == LCD_EXT_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < (cmd_size - 1); j++)
				delay_ms += table[i + 1 + j];
			if (delay_ms > 0)
				ldim_delay(delay_ms);
		} else if (type == LCD_EXT_CMD_TYPE_CMD) {
			ret = iw7038_wreg(bl_iw7038->spi, 0x0,
					  table[i + 1], table[i + 2]);
			usleep_range(1, 2);
		} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			ret = iw7038_wreg(bl_iw7038->spi, 0x0,
					  table[i + 1], table[i + 2]);
			usleep_range(1, 2);
			if (table[i + 3] > 0)
				ldim_delay(table[i + 3]);
		} else {
			LDIMERR("%s: type 0x%02x invalid\n", __func__, type);
		}
		i += cmd_size;
		step++;
	}

	return ret;
}

static int iw7038_power_on_init(void)
{
	int ret = 0;

	if (bl_iw7038->cmd_size < 2) {
		LDIMERR("%s: cmd_size %d is invalid\n",
			__func__, bl_iw7038->cmd_size);
		return -1;
	}
	if (!bl_iw7038->init_data) {
		LDIMERR("%s: init_data is null\n", __func__);
		return -1;
	}

	if (bl_iw7038->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
		ret = ldim_power_cmd_dynamic_size();
	else
		ret = ldim_power_cmd_fixed_size();

	return ret;
}

static int iw7038_hw_init_on(void)
{
	int i;
	unsigned char reg_duty_chk = 0;
	unsigned char reg_chk = 0, done = 0;
	unsigned int device_count;
	unsigned int device_id;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	/* step 1: SPI communication check */
	device_count = ldim_drv->ldev_conf->device_count;
	iw7038_wreg(bl_iw7038->spi, 0x0, 0x0000, 0x12);

	for (device_id = 1; device_id <= device_count; device_id++) {
		for (i = 0; i <= 3; i++) {
			reg_chk = 0;
			iw7038_rreg(bl_iw7038->spi, device_id, 0x8000,
				    &reg_chk);
			if (reg_chk == 0x12)
				break;
			if (i == 4) {
				LDIMERR("%s: SPI dev%d communication error\n",
					__func__, device_id);
			}
		}
	}

	ldim_drv->pinmux_ctrl(1);

	/* step 3: bl enable */
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		      ldim_drv->ldev_conf->en_gpio_on);
	ldim_set_duty_pwm(&ldim_drv->ldev_conf->ldim_pwm_config);
	ldim_set_duty_pwm(&ldim_drv->ldev_conf->analog_pwm_config);

	/* step 4: delay for internal logic stable */
	ldim_delay(10);

	/* step 5:release protect registers */
	iw7038_wreg(bl_iw7038->spi, 0x0, 0x00a1, 0xa3);

	/* step 6: configure initial registers */
	iw7038_power_on_init();

	/* step 7: lock registers */
	iw7038_wreg(bl_iw7038->spi, 0x0, 0x00a1, 0x00);
	iw7038_wreg(bl_iw7038->spi, 0x0, 0x009a, 0x00);

	/* step 8: delay */
	ldim_delay(10);

	/* step 9: exit reset state*/
	iw7038_wreg(bl_iw7038->spi, 0x0, 0x0000, 0x13);

	/* step 10: start calibration */
	for (device_id = 1; device_id <= device_count;
	     device_id++) {
		i = 0;
		while (i++ < 20) {
			iw7038_rreg(bl_iw7038->spi, device_id, 0x8142,
				    &reg_duty_chk);
		/*VDAC statue reg :FB1=[0x5] FB2=[0x50]*/
		/*The current platform using FB1*/
			if ((reg_duty_chk & 0x7) == 0x4) {
				done = device_id;
				break;
			}
			done = 0;
			ldim_delay(1);
		}
	}
	LDIMPR("%s: calibration %s!\n", __func__, (done ? "OK" : "fail"));

	return 0;
}

static int iw7038_hw_init_off(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		      ldim_drv->ldev_conf->en_gpio_off);
	ldim_drv->pinmux_ctrl(0);
	ldim_pwm_off(&ldim_drv->ldev_conf->ldim_pwm_config);
	ldim_pwm_off(&ldim_drv->ldev_conf->analog_pwm_config);

	return 0;
}

static int iw7038_spi_dump_low(int index, char *buf)
{
	int len = 0;
	unsigned int i;
	unsigned char val[64 + 8];
	unsigned int device_count;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (!buf)
		return len;

	if (!index) {
		LDIMPR("unsupport spi index\n");
		return 0;
	}
	mutex_lock(&iw7038_spi_mutex);

	len += sprintf(buf, "iw7038 reg low:\n");
	device_count = ldim_drv->ldev_conf->device_count;
	iw7038_rregs(bl_iw7038->spi, index, 0x00, val, 64);
	for (i = 0; i <= 0x3f; i++) {
		len += sprintf(buf + len, "dev%d: 0x%02x=0x%02x\n",
			       index, i, val[i + 8]);
	}

	len += sprintf(buf + len, "\n");
	mutex_unlock(&iw7038_spi_mutex);

	return len;
}

static int iw7038_spi_dump_dim(int index, char *buf)
{
	int len = 0;
	unsigned int i, num;
	unsigned int device_count;
	unsigned char val[32 + 8];
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (!buf)
		return len;
	if (!index) {
		LDIMPR("unsupport spi index\n");
		return 0;
	}

	mutex_lock(&iw7038_spi_mutex);

	len += sprintf(buf, "iw7038 reg dimming:\n");
	num = ldim_drv->ldev_conf->bl_zone_num;
	device_count = ldim_drv->ldev_conf->device_count;
	iw7038_rregs(bl_iw7038->spi, index, 0x40, val, 32);
	for (i = 0x40; i <= 0x5f; i++) {
		len += sprintf(buf + len, "dev%d: 0x%02x=0x%02x\n",
			       index, i, val[i - 0x40 + 8]);
	}

	len += sprintf(buf + len, "\n");

	mutex_unlock(&iw7038_spi_mutex);

	return len;
}

static int fault_cnt_save;
static int fault_check_cnt;
static int iw7038_fault_handler(unsigned int gpio)
{
	unsigned int val;

	if (gpio >= BL_GPIO_NUM_MAX)
		return 0;

	if (fault_check_cnt++ > 200) { /* clear fault flag for a cycle */
		fault_check_cnt = 0;
		fault_cnt = 0;
		fault_cnt_save = 0;
	}

	if (ldim_debug_print) {
		if (!vsync_cnt) {
			LDIMPR("%s: fault_cnt: %d, fault_check_cnt: %d\n",
			       __func__, fault_cnt, fault_check_cnt);
		}
	}

	val = ldim_gpio_get(gpio);
	if (val == 0)
		return 0;

	fault_cnt++;
	if (fault_cnt_save != fault_cnt) {
		fault_cnt_save = fault_cnt;
		LDIMPR("%s: fault_cnt: %d\n", __func__, fault_cnt);
	}
	if (fault_cnt <= 10) {
		iw7038_wreg(bl_iw7038->spi, 0x0, 0x0000, 0x12);
	} else {
		iw7038_hw_init_on();
		fault_cnt = 0;
		fault_cnt_save = 0;
	}

	return -1;
}

static unsigned int dim_max, dim_min;
static inline unsigned int iw7038_get_value(unsigned int level)
{
	unsigned int val;

	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	return val;
}

static int iw7038_smr(unsigned short *buf, unsigned char len)
{
	int i;
	unsigned int value_flag = 0, temp;
	unsigned short *mapping, num;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		vsync_cnt = 0;

	if (iw7038_on_flag == 0) {
		if (vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, iw7038_on_flag);
		return 0;
	}
	num = ldim_drv->ldev_conf->bl_zone_num;
	if (len != num) {
		if (vsync_cnt == 0)
			LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (!val_brightness) {
		if (!vsync_cnt)
			LDIMERR("%s: val_brightness is null\n", __func__);
		return -1;
	}

	mutex_lock(&iw7038_spi_mutex);

	mapping = &ldim_drv->ldev_conf->bl_mapping[0];
	dim_max = ldim_drv->ldev_conf->dim_max;
	dim_min = ldim_drv->ldev_conf->dim_min;

	/* this function will cause backlight flicker */
	/* if (ldim_drv->vsync_change_flag == 50) {
	 *	iw7038_wreg(bl_iw7038->spi, 0x31, 0xd7);
	 *	ldim_drv->vsync_change_flag = 0;
	 * } else if (ldim_drv->vsync_change_flag == 60) {
	 *	iw7038_wreg(bl_iw7038->spi, 0x31, 0xd3);
	 *	ldim_drv->vsync_change_flag = 0;
	 * }
	 */
	if (bl_iw7038->test_mode) {
		if (!test_brightness) {
			if (!vsync_cnt)
				LDIMERR("test_brightness is null\n");
			mutex_unlock(&iw7038_spi_mutex);
			return 0;
		}
		for (i = 0; i < num; i++) {
			val_brightness[2 * i] = (test_brightness[i] >> 8) &
							0x1f;
			val_brightness[2 * i + 1] = test_brightness[i] &
							0xff;
		}
	} else {
		for (i = 0; i < num; i++)
			value_flag += buf[i];
		if (value_flag == 0) {
			for (i = 0; i < (num * 2); i++)
				val_brightness[i] = 0;
		} else {
			for (i = 0; i < num; i++) {
				temp = iw7038_get_value(buf[i]);
				val_brightness[2 * mapping[i]] =
						(temp >> 8) & 0x1f;
				val_brightness[2 * mapping[i] + 1] =
						temp & 0xff;
			}
		}
	}

	iw7038_wregs(bl_iw7038->spi, 0x3f, 0x40, val_brightness,
		     ((num * 2) / 4));

	if (ldim_drv->ldev_conf->fault_check)
		iw7038_fault_handler(ldim_drv->ldev_conf->lamp_err_gpio);

	mutex_unlock(&iw7038_spi_mutex);

	return 0;
}

static int iw7038_check(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	if (vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		vsync_cnt = 0;

	if (iw7038_on_flag == 0) {
		if (vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, iw7038_on_flag);
		return 0;
	}

	mutex_lock(&iw7038_spi_mutex);

	if (ldim_drv->ldev_conf->fault_check)
		ret = iw7038_fault_handler(ldim_drv->ldev_conf->lamp_err_gpio);

	mutex_unlock(&iw7038_spi_mutex);

	return ret;
}

static int iw7038_power_on(void)
{
	if (iw7038_on_flag) {
		LDIMPR("%s: iw7038 is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&iw7038_spi_mutex);
	iw7038_hw_init_on();
	iw7038_on_flag = 1;
	vsync_cnt = 0;
	fault_cnt = 0;
	mutex_unlock(&iw7038_spi_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7038_power_off(void)
{
	mutex_lock(&iw7038_spi_mutex);
	iw7038_on_flag = 0;
	iw7038_hw_init_off();
	mutex_unlock(&iw7038_spi_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t iw7038_show(struct class *class,
			   struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct iw7038_s *bl = container_of(class, struct iw7038_s, cls);
	int ret = 0;
	int i, index;

	if (!strcmp(attr->attr.name, "test")) {
		ret = sprintf(buf, "test mode=%d\n", bl->test_mode);
	} else if (!strcmp(attr->attr.name, "brightness")) {
		if (!test_brightness) {
			ret = sprintf(buf, "test_brightness is null\n");
			return ret;
		}
		ret = sprintf(buf, "test_brightness: ");
		for (i = 0; i < ldim_drv->ldev_conf->bl_zone_num; i++)
			ret += sprintf(buf + ret, " 0x%x", test_brightness[i]);
		ret += sprintf(buf + ret, "\n");
	} else if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "iw7038_bl status:\n"
				"dev_index      = %d\n"
				"zone_num         = %d\n"
				"device_count     = %d\n"
				"on_flag        = %d\n"
				"fault_cnt      = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"cs_hold_delay  = %d\n"
				"cs_clk_delay   = %d\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n",
				ldim_drv->dev_index,
				ldim_drv->ldev_conf->bl_zone_num,
				ldim_drv->ldev_conf->device_count,
				iw7038_on_flag, fault_cnt,
				ldim_drv->ldev_conf->en_gpio_on,
				ldim_drv->ldev_conf->en_gpio_off,
				ldim_drv->ldev_conf->cs_hold_delay,
				ldim_drv->ldev_conf->cs_clk_delay,
				ldim_drv->ldev_conf->dim_max,
				ldim_drv->ldev_conf->dim_min);
	} else if (!strcmp(attr->attr.name, "dump_dim")) {
		i = sscanf(buf, "dump_dim %d", &index);
		iw7038_spi_dump_dim(index, buf);
	} else if (!strcmp(attr->attr.name, "dump_low")) {
		i = sscanf(buf, "dump_low %d", &index);
		iw7038_spi_dump_low(index, buf);
	}

	return ret;
}

#define MAX_ARG_NUM 4
static ssize_t iw7038_store(struct class *class,
			    struct class_attribute *attr, const char *buf,
			    size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct iw7038_s *bl = container_of(class, struct iw7038_s, cls);
	unsigned int val, val1, val2, reg_addr;
	unsigned char reg_val = 0, temp = 0;
	unsigned int device_count;
	unsigned int device_id;
	int i;

	device_count = ldim_drv->ldev_conf->device_count;

	if (!strcmp(attr->attr.name, "init")) {
		mutex_lock(&iw7038_spi_mutex);
		iw7038_hw_init_on();
		mutex_unlock(&iw7038_spi_mutex);
	} else if (!strcmp(attr->attr.name, "reg")) {
		if (buf[0] == 'w') {
			i = sscanf(buf, "w %x %x %x", &val, &val1, &val2);
			if (i == 3) {
				mutex_lock(&iw7038_spi_mutex);
				device_id = val;
				reg_addr = val1;
				reg_val = (unsigned char)val2;
				iw7038_wreg(bl->spi, val, reg_addr, reg_val);
				for (device_id = 1; device_id <= device_count;
				     device_id++) {
					iw7038_rreg(bl->spi, device_id,
						    reg_addr, &temp);
					pr_info("reg 0x%02x = 0x%02x dev%d_readback 0x%02x\n",
						reg_addr, reg_val, device_id,
						temp);
				}
				mutex_unlock(&iw7038_spi_mutex);
			} else {
				LDIMERR("%s: invalid args\n", __func__);
			}
		} else if (buf[0] == 'r') {
			i = sscanf(buf, "r %x", &val);
			if (i == 1) {
				mutex_lock(&iw7038_spi_mutex);
				reg_addr = val;
				for (device_id = 1; device_id <= device_count;
				     device_id++) {
					iw7038_rreg(bl->spi, device_id,
						    reg_addr, &reg_val);
					pr_info("reg 0x%02x dev%d: 0x%02x",
						reg_addr, device_id, reg_val);
				}
				mutex_unlock(&iw7038_spi_mutex);
			} else {
				LDIMERR("%s: invalid args\n", __func__);
			}
		}
	} else if (!strcmp(attr->attr.name, "test")) {
		i = kstrtouint(buf, 10, &val);
		LDIMPR("set test mode to %d\n", val);
		bl->test_mode = val;
	} else if (!strcmp(attr->attr.name, "brightness")) {
		i = sscanf(buf, "%d %d", &val, &val2);
		val2 &= 0x1fff;
		if (!test_brightness) {
			LDIMERR("test_brightness is null\n");
			return count;
		}
		if (i == 2 && val < ldim_drv->ldev_conf->bl_zone_num) {
			test_brightness[val] = (unsigned short)val2;
			LDIMPR("test brightness[%d] = %d\n", val, val2);
		}
	} else {
		LDIMERR("LDIM argument error!\n");
	}
	return count;
}

static struct class_attribute iw7038_class_attrs[] = {
	__ATTR(init, 0644, NULL, iw7038_store),
	__ATTR(reg, 0644, iw7038_show, iw7038_store),
	__ATTR(test, 0644, iw7038_show, iw7038_store),
	__ATTR(brightness, 0644, iw7038_show, iw7038_store),
	__ATTR(status, 0644, iw7038_show, NULL),
	__ATTR(dump_low, 0644, iw7038_show, NULL),
	/*__ATTR(dump_high, 0644, iw7038_show, NULL),*/
	__ATTR(dump_dim, 0644, iw7038_show, NULL)
};

static int iw7038_ldim_driver_update(struct aml_ldim_driver_s *ldim_drv)
{
	ldim_drv->device_power_on = iw7038_power_on;
	ldim_drv->device_power_off = iw7038_power_off;
	ldim_drv->device_bri_update = iw7038_smr;
	ldim_drv->device_bri_check = iw7038_check;

	ldim_drv->ldev_conf->dev_reg_write = iw7038_reg_write;
	ldim_drv->ldev_conf->dev_reg_read = iw7038_reg_read;
	return 0;
}

int ldim_dev_iw7038_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct class *dev_class;
	int ret, i;
	int max_data_size;

	if (!ldim_drv->ldev_conf->spi_dev) {
		LDIMERR("%s: spi_dev is null\n", __func__);
		return -1;
	}

	iw7038_on_flag = 0;
	vsync_cnt = 0;
	fault_cnt = 0;

	bl_iw7038 = kzalloc(sizeof(*bl_iw7038), GFP_KERNEL);
	if (!bl_iw7038) {
		LDIMERR("malloc bl_iw7038 failed\n");
		return -1;
	}

	bl_iw7038->test_mode = 0;
	bl_iw7038->spi = ldim_drv->ldev_conf->spi_dev;
	bl_iw7038->cs_hold_delay = ldim_drv->ldev_conf->cs_hold_delay;
	bl_iw7038->cs_clk_delay = ldim_drv->ldev_conf->cs_clk_delay;
	bl_iw7038->cmd_size = ldim_drv->ldev_conf->cmd_size;
	bl_iw7038->init_data = ldim_drv->ldev_conf->init_on;
	bl_iw7038->init_data_cnt = ldim_drv->ldev_conf->init_on_cnt;

	max_data_size = ldim_drv->ldev_conf->bl_zone_num *
			ldim_drv->ldev_conf->device_count * 2;
	tbuf = kcalloc(max_data_size, sizeof(unsigned char), GFP_KERNEL);
	if (!tbuf) {
		LDIMERR("malloc tbuf failed\n");
		kfree(bl_iw7038);
		return -1;
	}

	val_brightness = kcalloc(ldim_drv->ldev_conf->bl_zone_num * 2,
				 sizeof(unsigned char), GFP_KERNEL);
	if (!val_brightness) {
		LDIMERR("malloc val_brightness failed\n");
		kfree(tbuf);
		kfree(bl_iw7038);
		return -1;
	}
	test_brightness = kcalloc(ldim_drv->ldev_conf->bl_zone_num,
				  sizeof(unsigned short), GFP_KERNEL);
	if (!test_brightness) {
		LDIMERR("malloc test_brightness failed\n");
	} else {
		for (i = 0; i < ldim_drv->ldev_conf->bl_zone_num; i++)
			test_brightness[i] = 0xfff;
	}

	iw7038_ldim_driver_update(ldim_drv);

	if (ldim_drv->ldev_conf->dev_class) {
		dev_class = ldim_drv->ldev_conf->dev_class;
		for (i = 0; i < ARRAY_SIZE(iw7038_class_attrs); i++) {
			if (class_create_file(dev_class,
					      &iw7038_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
				 iw7038_class_attrs[i].attr.name);
			}
		}
	}

	iw7038_on_flag = 1; /* default enable in uboot */

	LDIMPR("%s ok\n", __func__);
	return ret;
}

int ldim_dev_iw7038_remove(struct aml_ldim_driver_s *ldim_drv)
{
	kfree(tbuf);
	tbuf = NULL;
	kfree(val_brightness);
	val_brightness = NULL;
	kfree(test_brightness);
	test_brightness = NULL;

	kfree(bl_iw7038);
	bl_iw7038 = NULL;

	return 0;
}

