/*
 * drivers/amlogic/display/backlight/aml_ldim/iw7019_bl.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/vout/aml_ldim.h>
#include <linux/amlogic/vout/aml_bl.h>
#include "iw7019_lpf.h"
#include "iw7019_bl.h"
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define INT_VIU_VSYNC   35

#define NORMAL_MSG      (0<<7)
#define BROADCAST_MSG   (1<<7)
#define BLOCK_DATA      (0<<6)
#define SINGLE_DATA     (1<<6)
#define IW7019_DEV_ADDR 1

#define IW7019_REG_BRIGHTNESS_CHK  0x00
#define IW7019_REG_BRIGHTNESS      0x01
#define IW7019_REG_CHIPID          0x7f
#define IW7019_CHIPID              0x28

#define IW7019_POWER_ON       0
#define IW7019_POWER_RESET    1
static int iw7019_on_flag;
static int iw7019_spi_op_flag;
static int iw7019_wr_err_cnt;
static int iw7019_spi_err_flag;
static int iw7019_spi_force_err_flag;
static unsigned char iw7019_chipid_check_vs_cnt;
static unsigned short vsync_cnt;
static unsigned short fault_cnt;
static unsigned short check_id_cnt;
static unsigned long reset_cnt;
static int iw7019_spi_rw_test_flag;
static int iw7019_static_pic_test_flag;
static int iw7019_static_pic_test_count;

static spinlock_t iw7019_spi_lock;
static struct workqueue_struct  *iw7019_workqueue;
static struct work_struct iw7019_work;

struct iw7019 {
	int test_mode;
	struct spi_device *spi;
	int cs_hold_delay;
	int cs_clk_delay;
	unsigned char cmd_size;
	unsigned char *init_data;
	struct class cls;
};
struct iw7019 *bl_iw7019;

#if 0
static unsigned char iw7019_ini_data[LDIM_SPI_INIT_ON_SIZE] = {
#if 1
	/* step1: */
	0x00, 0x01, 0x08, 0x00,
	/* step2:disable ocp and otp */
	0x00, 0x34, 0x14, 0x00,
	0x00, 0x10, 0x53, 0x00,
	/* step3: */
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x12, 0x02, 0x00,
	0x00, 0x13, 0x00, 0x00,
	0x00, 0x14, 0x40, 0x00,
	0x00, 0x15, 0x06, 0x00,
	0x00, 0x16, 0x00, 0x00,
	0x00, 0x17, 0x80, 0x00,
	0x00, 0x18, 0x0a, 0x00,
	0x00, 0x19, 0x00, 0x00,
	0x00, 0x1a, 0xc0, 0x00,
	0x00, 0x1b, 0x0e, 0x00,
	0x00, 0x1c, 0x00, 0x00,
	0x00, 0x1d, 0x00, 0x00,
	0x00, 0x1e, 0x50, 0x00,
	0x00, 0x1f, 0x00, 0x00,
	0x00, 0x20, 0x63, 0x00,
	0x00, 0x21, 0xff, 0x00,
	0x00, 0x2a, 0xff, 0x00,
	0x00, 0x2b, 0x41, 0x00,
	0x00, 0x2c, 0x28, 0x00,
	0x00, 0x30, 0xff, 0x00,
	0x00, 0x31, 0x00, 0x00,
	0x00, 0x32, 0x0f, 0x00,
	0x00, 0x33, 0x40, 0x00,
	0x00, 0x34, 0x40, 0x00,
	0x00, 0x35, 0x00, 0x00,
	0x00, 0x3f, 0xa3, 0x00,
	0x00, 0x45, 0x00, 0x00,
	0x00, 0x47, 0x04, 0x00,
	0x00, 0x48, 0x60, 0x00,
	0x00, 0x4a, 0x0d, 0x00,
	/* step4:set 50% brightness */
	0x00, 0x01, 0x7f, 0x00,
	0x00, 0x02, 0xf7, 0x00,
	0x00, 0x03, 0xff, 0x00,
	0x00, 0x04, 0x7f, 0x00,
	0x00, 0x05, 0xf7, 0x00,
	0x00, 0x06, 0xff, 0x00,
	0x00, 0x07, 0x7f, 0x00,
	0x00, 0x08, 0xf7, 0x00,
	0x00, 0x09, 0xff, 0x00,
	0x00, 0x0a, 0x7f, 0x00,
	0x00, 0x0b, 0xf7, 0x00,
	0x00, 0x0c, 0xff, 0x00,
	/* step5: */
	0x00, 0x00, 0x09, 0x00,
	/* step6: */
	0x00, 0x34, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00,
#else
	/* step1: */
	0x00, 0x00, 0x0E, 0x00,
	0x00, 0x1D, 0x01, 0x00,
	/* step2:disable ocp and otp */
	0x00, 0x34, 0x54, 0x00,
	0x00, 0x10, 0x93, 0x00,
	/* step3: */
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x12, 0x12, 0x00,
	0x00, 0x13, 0x00, 0x00,
	0x00, 0x14, 0x40, 0x00,
	0x00, 0x15, 0x06, 0x00,
	0x00, 0x16, 0x00, 0x00,
	0x00, 0x17, 0x80, 0x00,
	0x00, 0x18, 0x0a, 0x00,
	0x00, 0x19, 0x00, 0x00,
	0x00, 0x1a, 0xc0, 0x00,
	0x00, 0x1b, 0x0e, 0x00,
	0x00, 0x1c, 0x00, 0x00,
	0x00, 0x1d, 0x01, 0x00,
	0x00, 0x1e, 0x50, 0x00,
	0x00, 0x1f, 0x00, 0x00,
	0x00, 0x20, 0x43, 0x00,
	0x00, 0x21, 0xff, 0x00,
	0x00, 0x2a, 0xff, 0x00,
	0x00, 0x2b, 0x01, 0x00,
	0x00, 0x2c, 0x28, 0x00,
	0x00, 0x30, 0xff, 0x00,
	0x00, 0x31, 0x00, 0x00,
	0x00, 0x3f, 0xa3, 0x00,
	0x00, 0x47, 0x04, 0x00,
	0x00, 0x48, 0x40, 0x00, /*use external vsync or internal vsync*/
	0x00, 0x4a, 0x45, 0x00,
	0x00, 0x4b, 0x0C, 0x00,
	/*step4:set min brightness*/
	0x00, 0x01, 0x07, 0x00,
	0x00, 0x02, 0xf0, 0x00,
	0x00, 0x03, 0x7f, 0x00,
	0x00, 0x04, 0x07, 0x00,
	0x00, 0x05, 0xf0, 0x00,
	0x00, 0x06, 0x7f, 0x00,
	0x00, 0x07, 0x07, 0x00,
	0x00, 0x08, 0xf0, 0x00,
	0x00, 0x09, 0x7f, 0x00,
	0x00, 0x0a, 0x07, 0x00,
	0x00, 0x0b, 0xf0, 0x00,
	0x00, 0x0c, 0x7f, 0x00,
	/* step5: */
	0x00, 0x00, 0x0F, 0x00,
	/* step6: */
	0x00, 0x34, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00,  /* ending */
#endif
};
#endif

static int test_brightness[] = {
	0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff
};
static unsigned char static_pic_val[13];

static int iw7019_rreg(struct spi_device *spi, u8 addr, u8 *val)
{
	u8 tbuf[3];
	int ret;

	spin_lock(&iw7019_spi_lock);
	if (bl_iw7019->cs_hold_delay)
		udelay(bl_iw7019->cs_hold_delay);
	dirspi_start(spi);
	if (bl_iw7019->cs_clk_delay)
		udelay(bl_iw7019->cs_clk_delay);
	tbuf[0] = NORMAL_MSG | SINGLE_DATA | IW7019_DEV_ADDR;
	tbuf[1] = addr | 0x80;
	tbuf[2] = 0;
	ret = dirspi_write(spi, tbuf, 3);
	ret = dirspi_read(spi, val, 1);
	if (bl_iw7019->cs_clk_delay)
		udelay(bl_iw7019->cs_clk_delay);
	dirspi_stop(spi);
	spin_unlock(&iw7019_spi_lock);

	return ret;
}

static int iw7019_wreg(struct spi_device *spi, u8 addr, u8 val)
{
	u8 tbuf[3];
	int ret;

	spin_lock(&iw7019_spi_lock);
	if (bl_iw7019->cs_hold_delay)
		udelay(bl_iw7019->cs_hold_delay);
	dirspi_start(spi);
	if (bl_iw7019->cs_clk_delay)
		udelay(bl_iw7019->cs_clk_delay);
	tbuf[0] = NORMAL_MSG | SINGLE_DATA | IW7019_DEV_ADDR;
	tbuf[1] = addr & 0x7f;
	tbuf[2] = val;
	ret = dirspi_write(spi, tbuf, 3);
	if (bl_iw7019->cs_clk_delay)
		udelay(bl_iw7019->cs_clk_delay);
	dirspi_stop(spi);
	spin_unlock(&iw7019_spi_lock);

	return ret;
}

static int iw7019_wregs(struct spi_device *spi, u8 addr, u8 *val, int len)
{
	u8 tbuf[20];
	int ret;

	spin_lock(&iw7019_spi_lock);
	if (bl_iw7019->cs_hold_delay)
		udelay(bl_iw7019->cs_hold_delay);
	dirspi_start(spi);
	if (bl_iw7019->cs_clk_delay)
		udelay(bl_iw7019->cs_clk_delay);
	tbuf[0] = NORMAL_MSG | BLOCK_DATA | IW7019_DEV_ADDR;
	tbuf[1] = len;
	tbuf[2] = addr & 0x7f;
	memcpy(&tbuf[3], val, len);
	ret = dirspi_write(spi, tbuf, len+3);
	if (bl_iw7019->cs_clk_delay)
		udelay(bl_iw7019->cs_clk_delay);
	dirspi_stop(spi);
	spin_unlock(&iw7019_spi_lock);

	return ret;
}

static int iw7019_power_on_init(int flag)
{
	unsigned char addr, val;
	int i, ret = 0;

	LDIMPR("%s: spi_op_flag=%d\n", __func__, iw7019_spi_op_flag);

	if (flag == IW7019_POWER_RESET)
		goto iw7019_power_reset_p;
	i = 1000;
	while ((iw7019_spi_op_flag) && (i > 0)) {
		i--;
		udelay(10);
	}
	if (iw7019_spi_op_flag == 1) {
		LDIMERR("%s: wait spi idle state failed\n", __func__);
		return 0;
	}

	iw7019_spi_op_flag = 1;
iw7019_power_reset_p:
	vsync_cnt = 0;
	fault_cnt = 0;

	for (i = 0; i < LDIM_SPI_INIT_ON_SIZE; i += bl_iw7019->cmd_size) {
		if (bl_iw7019->init_data[i] == 0xff) {
			if (bl_iw7019->init_data[i+3] > 0)
				mdelay(bl_iw7019->init_data[i+3]);
			break;
		} else if (bl_iw7019->init_data[i] == 0x0) {
			addr = bl_iw7019->init_data[i+1];
			val = bl_iw7019->init_data[i+2];
			ret = iw7019_wreg(bl_iw7019->spi, addr, val);
			udelay(1);
		}
		if (bl_iw7019->init_data[i+3] > 0)
			mdelay(bl_iw7019->init_data[i+3]);
	}

	iw7019_wr_err_cnt = 0;
	iw7019_spi_err_flag = 0;
	iw7019_chipid_check_vs_cnt = 0;
	if (flag == IW7019_POWER_RESET)
		return ret;

	iw7019_spi_op_flag = 0;
	return ret;
}

static int iw7019_hw_init_on(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_on);
	mdelay(2);
	ldim_set_duty_pwm(&(ldim_drv->ldev_conf->pwm_config));
	ldim_drv->pinmux_ctrl(ldim_drv->ldev_conf->pinmux_name, 1);
	mdelay(100);
	iw7019_power_on_init(IW7019_POWER_ON);

	return 0;
}

static int iw7019_hw_init_off(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int i = 1000;

	while ((iw7019_spi_op_flag) && (i > 0)) {
		i--;
		udelay(10);
	}
	if (iw7019_spi_op_flag == 1)
		LDIMERR("%s: wait spi idle state failed\n", __func__);

	ldim_drv->pinmux_ctrl(ldim_drv->ldev_conf->pinmux_name, 0);
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_off);

	return 0;
}

static int spi_rw_test_en;
static unsigned long spi_rw_test_cnt;
static unsigned long spi_rw_test_err;
static unsigned int spi_rw_test_retry_max;
#define RW_TEST_PR(err_cnt, total_cnt, addr, rval, wval, retry_n) \
	LDIMPR("%s: failed: %ld/%ld, 0x%02x=0x%02x, w_val=0x%02x, retry=%d\n", \
		__func__, err_cnt, total_cnt, addr, rval, wval, retry_n);
static void iw7019_spi_rw_test_check(unsigned char reg, unsigned char val)
{
	int i;
	unsigned char reg_chk;
	unsigned int retry_cnt = 0, retry_max = 1000;

	for (i = 0; i < retry_max; i++) {
		iw7019_rreg(bl_iw7019->spi, reg, &reg_chk);
		if (val != reg_chk) {
			spi_rw_test_err++;
			/*RW_TEST_PR(spi_rw_test_err, spi_rw_test_cnt,
				reg, reg_chk, val, retry_cnt);*/
			iw7019_wreg(bl_iw7019->spi, reg, val);
			spi_rw_test_cnt++;
			retry_cnt++;
		} else {
			break;
		}
	}
	if (spi_rw_test_retry_max < retry_cnt)
		spi_rw_test_retry_max = retry_cnt;
	if (retry_cnt > 0) {
		RW_TEST_PR(spi_rw_test_err, spi_rw_test_cnt,
				reg, reg_chk, val, retry_cnt);
	}
}

static void iw7019_spi_rw_test(struct work_struct *work)
{
	unsigned char val[13], n;
	unsigned int i, j;

	if (iw7019_on_flag == 0) {
		LDIMPR("%s: on_flag=%d\n", __func__, iw7019_on_flag);
		return;
	}
	if (iw7019_spi_rw_test_flag) {
		LDIMPR("%s: is already runing\n", __func__);
		return;
	}
	i = 1000;
	while ((iw7019_spi_op_flag) && (i > 0)) {
		i--;
		udelay(10);
	}
	if (iw7019_spi_op_flag == 1) {
		LDIMERR("%s: wait spi idle state failed\n", __func__);
		return;
	}

	iw7019_spi_rw_test_flag = 1;
	iw7019_spi_op_flag = 1;
	spi_rw_test_cnt = 0;
	spi_rw_test_err = 0;
	spi_rw_test_retry_max = 0;
	spi_rw_test_en = 1;
	LDIMPR("%s: start\n", __func__);

	val[0] = 0x0f;
	while (spi_rw_test_en) {
		for (j = 0x7f; j < 0xfff; j++) {
			if (spi_rw_test_en == 0)
				break;
			for (i = 0; i < 4; i++) {
				/* br0[11~4] */
				val[i*3 + 1] = (j >> 4) & 0xff;
				/* br0[3~0]|br1[11~8] */
				val[i*3 + 2] = ((j & 0xf) << 4) |
					((j >> 8) & 0xf);
				/* br1[7~0] */
				val[i*3 + 3] = j & 0xff;
			}
			iw7019_wregs(bl_iw7019->spi, 0x00, val, 13);
			spi_rw_test_cnt++;
			for (n = 0x00; n <= 0x0c; n++)
				iw7019_spi_rw_test_check(n, val[n]);
		}
	}

	iw7019_spi_op_flag = 0;
	iw7019_spi_rw_test_flag = 0;
	LDIMPR("%s: stop\n", __func__);
	LDIMPR("iw7019 rw_test status:\n"
		"total_test_cnt      = %ld\n"
		"total_err_cnt       = %ld\n"
		"retry_cnt_max       = %d\n"
		"spi_rw_test_en      = %d\n"
		"spi_rw_test_flag    = %d\n",
		spi_rw_test_cnt, spi_rw_test_err,
		spi_rw_test_retry_max,
		spi_rw_test_en, iw7019_spi_rw_test_flag);
}

static int iw7019_spi_dump(char *buf)
{
	int ret = 0;
	unsigned char i, val;

	for (i = 0; i <= 0x7f; i++) {
		iw7019_rreg(bl_iw7019->spi, i, &val);
		if (buf) {
			int n = sprintf(buf+ret,
					"iw7019 reg 0x%02x=0x%02x\n", i, val);
			ret += n;
		}
		pr_info("iw7019 reg 0x%02x=0x%02x\n", i, val);
	}
	if (buf)
		ret += sprintf(buf, "\n");
	pr_info("\n");

	return ret;
}

static int fault_cnt_save;
static int iw7019_fault_handler(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val;
	int ret = 0;

	if (fault_cnt >= 35) {
		if (ldim_debug_print >= 2) {
			LDIMPR("%s: bypass fault check, fault_cnt=%d\n",
				__func__, fault_cnt);
		}
		return 0;
	} else if (fault_cnt == 1) {
		vsync_cnt = 0;
	}

	if (vsync_cnt >= 200) {
		if (ldim_debug_print) {
			LDIMPR("%s: vsync_cnt=%d, fault_cnt=%d\n",
				__func__, vsync_cnt, fault_cnt);
		}
		vsync_cnt = 0;
		fault_cnt = 0;
		fault_cnt_save = 0;
	} else {
		vsync_cnt++;
	}

	if ((ldim_debug_print) && (fault_cnt_save != fault_cnt)) {
		fault_cnt_save = fault_cnt;
		LDIMPR("%s: vsync_cnt=%d, fault_cnt=%d\n",
			__func__, vsync_cnt, fault_cnt);
	}
	val = ldim_gpio_get(ldim_drv->ldev_conf->lamp_err_gpio);
	if (val) {
		fault_cnt++;
		if (fault_cnt <= 35) {
			if (ldim_debug_print) {
				LDIMPR("%s: fault detected fault_cnt %d\n",
					__func__, fault_cnt);
				if (ldim_debug_print >= 2)
					iw7019_spi_dump(NULL);
			}
			iw7019_wreg(bl_iw7019->spi, 0x35, 0x43);
			ret = -1;
		}
	}

	return ret;
}

static int iw7019_reset_handler(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	/* disable BL_ON once */
	LDIMPR("reset iw7019 BL_ON\n");
	reset_cnt++;
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_off);
	mdelay(1000);
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_on);
	mdelay(2);
	iw7019_power_on_init(IW7019_POWER_RESET);

	return 0;
}

static int iw7019_short_reset_handler(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	/* disable BL_ON once */
	LDIMPR("short reset iw7019 BL_ON\n");
	reset_cnt++;
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_off);
	mdelay(300);
	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		ldim_drv->ldev_conf->en_gpio_on);
	mdelay(2);
	iw7019_power_on_init(IW7019_POWER_RESET);

	return 0;
}

static unsigned int iw7019_get_value(unsigned int level)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val;
	unsigned int dim_max, dim_min;

	dim_max = ldim_drv->ldev_conf->dim_max;
	dim_min = ldim_drv->ldev_conf->dim_min;

	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	return val;
}

static int iw7019_smr(unsigned short *buf, unsigned char len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int i, j, offset, cmd_len;
	unsigned int value_flag = 0;
	unsigned char val[13];
	unsigned int br0, br1;
	unsigned char bri_reg;
	unsigned char temp, reg_chk, clk_sel, wr_err_flag = 0;
	int static_pic_err_flag = 0;

	if (iw7019_spi_rw_test_flag)
		return 0;
	if (iw7019_on_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: on_flag=%d\n", __func__, iw7019_on_flag);
		return 0;
	}
	if (len != 8) {
		LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (iw7019_spi_err_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: spi_err_flag=%d\n",
				__func__, iw7019_spi_err_flag);
		}
		return 0;
	}
	if (iw7019_spi_op_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: spi_op_flag=%d\n",
				__func__, iw7019_spi_op_flag);
		}
		return 0;
	}

	iw7019_spi_op_flag = 1;
	if (ldim_drv->ldev_conf->write_check) {
		offset = 1;
		val[0] = 0x0f;
		cmd_len = 13;
		bri_reg = IW7019_REG_BRIGHTNESS_CHK;
	} else {
		offset = 0;
		cmd_len = 12;
		bri_reg = IW7019_REG_BRIGHTNESS;
	}

	for (i = 0; i < 8; i++)
		value_flag = value_flag || buf[i];

	if (value_flag) {
		for (i = 0; i < 4; i++) {
			if (bl_iw7019->test_mode) {
				br0 = test_brightness[i*2+0];
				br1 = test_brightness[i*2+1];
			} else {
				br0 = iw7019_get_value(buf[i*2+0]);
				br1 = iw7019_get_value(buf[i*2+1]);
			}
			/* br0[11~4] */
			val[i*3 + offset] = (br0 >> 4) & 0xff;
			/* br0[3~0]|br1[11~8] */
			val[i*3 + offset + 1] = ((br0 & 0xf) << 4) |
					((br1 >> 8) & 0xf);
			/* br1[7~0] */
			val[i*3 + offset + 2] = br1 & 0xff;
		}
	} else {
		for (i = 0; i < 12; i++)
			val[i + offset] = 0;
	}
	iw7019_wregs(bl_iw7019->spi, bri_reg, val, cmd_len);

	if (iw7019_static_pic_test_flag == 1) {
		if (iw7019_static_pic_test_count == 1) {
			for (i = 0; i < 13; i++)
				static_pic_val[i] = val[i];
			iw7019_static_pic_test_count = 0;
		}
		for (j = 0x01; j <= 0x0c; j++) {
			iw7019_rreg(bl_iw7019->spi, j, &reg_chk);
			if (static_pic_val[j] == reg_chk) {
				ldim_drv->static_pic_flag = 0;
				break;
			} else if (val[j] != static_pic_val[j]) {
				LDIMERR(
			"wr_val 0x%02x=0x%02x, static_pic_val=0x%02x\n",
			j, val[j], static_pic_val[j]);
				static_pic_err_flag = 1;
			} else {
				LDIMERR(
			"rd_val 0x%02x=0x%02x, static_pic_val=0x%02x\n",
			j, reg_chk, static_pic_val[j]);
					ldim_drv->static_pic_flag = 0;
			}
			if (j == 0x0c && static_pic_err_flag == 1)
				ldim_drv->static_pic_flag = 1;
		}
	}

	if (ldim_drv->ldev_conf->write_check) { /* brightness write check */
		/* reg 0x00 check */
		iw7019_rreg(bl_iw7019->spi, 0x00, &reg_chk);
		for (i = 1; i < 3; i++) {
			iw7019_rreg(bl_iw7019->spi, 0x00, &temp);
			if (temp != reg_chk)
				goto iw7019_smr_end;
		}
		clk_sel = (reg_chk >> 1) & 0x3;
		if ((reg_chk == 0xff) || (clk_sel == 0x1) || (clk_sel == 0x2) ||
			(iw7019_spi_force_err_flag > 0)) {
			iw7019_spi_err_flag = 1;
			iw7019_spi_force_err_flag = 0;
			LDIMERR("%s: reg check failed, 0x00=0x%02x\n",
				__func__, reg_chk);
			iw7019_reset_handler();
			goto iw7019_smr_end;
		}
/*iw7019_smr_write_chk2:*/
		/* reg brightness check */
		for (j = 0x01; j <= 0x0c; j++) {
			for (i = 1; i < 3; i++) {
				iw7019_rreg(bl_iw7019->spi, j, &reg_chk);
				if (val[j] == reg_chk) {
					wr_err_flag = 0;
					break;
				} else {
					LDIMERR("%s: brightness write failed\n",
						__func__);
					LDIMERR("0x%02x=0x%02x, w_val=0x%02x\n",
						j, reg_chk, val[j]);
					iw7019_wreg(bl_iw7019->spi, j, val[j]);
					wr_err_flag = 1;
				}
			}
			if (wr_err_flag)
				iw7019_wr_err_cnt++;
		}
		if (iw7019_wr_err_cnt >= 60) {
			LDIMERR("%s: spi write failed\n", __func__);
			iw7019_short_reset_handler();
			goto iw7019_smr_end;
		}
	}
	if (ldim_drv->ldev_conf->fault_check) {
		if (ldim_drv->ldev_conf->lamp_err_gpio >= BL_GPIO_NUM_MAX)
			goto iw7019_smr_end;
		iw7019_fault_handler();
	}

iw7019_smr_end:
	iw7019_spi_op_flag = 0;
	return 0;
}

static int iw7019_chip_check(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;
	unsigned char chipid = 0;
	/**/
	if (ldim_drv->ldev_conf->fault_check) {
		if (ldim_drv->ldev_conf->lamp_err_gpio >= BL_GPIO_NUM_MAX)
			goto iw7019_chip_check_end;
		ret = iw7019_fault_handler();
	}
	if (iw7019_chipid_check_vs_cnt++ > 5)
		iw7019_chipid_check_vs_cnt = 0;
	else
		return 0;

	if (iw7019_spi_rw_test_flag)
		return 0;
	if (iw7019_on_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: on_flag=%d\n", __func__, iw7019_on_flag);
		return 0;
	}
	if (iw7019_spi_err_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: spi_err_flag=%d\n",
				__func__, iw7019_spi_err_flag);
		}
		return 0;
	}
	if (iw7019_spi_op_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: spi_op_flag=%d\n",
				__func__, iw7019_spi_op_flag);
		}
		return 0;
	}

	iw7019_spi_op_flag = 1;

	if (ldim_drv->ldev_conf->write_check) {
		iw7019_rreg(bl_iw7019->spi, IW7019_REG_CHIPID, &chipid);
		if (chipid != IW7019_CHIPID) {
			if (check_id_cnt >= 3) {
				iw7019_spi_err_flag = 1;
				LDIMERR("%s: read failed, 0x7f=0x%02x\n",
					__func__, chipid);
				LDIMPR("check_id_cnt=%d\n", check_id_cnt);
				iw7019_short_reset_handler();
				ret = -1;
				check_id_cnt = 0;
				goto iw7019_chip_check_end;
			} else {
				check_id_cnt++;
			}
		}
	}


iw7019_chip_check_end:
	iw7019_spi_op_flag = 0;

	return ret;
}

static int iw7019_power_on(void)
{
	if (iw7019_on_flag) {
		LDIMPR("%s: iw7019 is already on, exit\n", __func__);
		return 0;
	}
	iw7019_hw_init_on();
	iw7019_on_flag = 1;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7019_power_off(void)
{
	iw7019_on_flag = 0;
	iw7019_hw_init_off();

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t iw7019_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct iw7019 *bl = container_of(class, struct iw7019, cls);
	int ret = 0;
	int i;

	if (!strcmp(attr->attr.name, "mode")) {
		ret = sprintf(buf, "0x%02x\n", bl->spi->mode);
	} else if (!strcmp(attr->attr.name, "speed")) {
		ret = sprintf(buf, "%d\n", bl->spi->max_speed_hz);
	} else if (!strcmp(attr->attr.name, "test")) {
		ret = sprintf(buf, "test mode=%d\n", bl->test_mode);
	} else if (!strcmp(attr->attr.name, "brightness")) {
		ret = sprintf(buf, "0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
				test_brightness[0],
				test_brightness[1],
				test_brightness[2],
				test_brightness[3],
				test_brightness[4],
				test_brightness[5],
				test_brightness[6],
				test_brightness[7]);
	} else if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "iw7019 status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"cs_hold_delay  = %d\n"
				"cs_clk_delay   = %d\n"
				"spi_op_flag    = %d\n"
				"write_check    = %d\n"
				"fault_check    = %d\n"
				"vsync_cnt      = %d\n"
				"fault_cnt      = %d\n"
				"reset_cnt      = %ld\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n",
				ldim_drv->dev_index, iw7019_on_flag,
				ldim_drv->ldev_conf->en_gpio_on,
				ldim_drv->ldev_conf->en_gpio_off,
				ldim_drv->ldev_conf->cs_hold_delay,
				ldim_drv->ldev_conf->cs_clk_delay,
				iw7019_spi_op_flag,
				ldim_drv->ldev_conf->write_check,
				ldim_drv->ldev_conf->fault_check,
				vsync_cnt, fault_cnt, reset_cnt,
				ldim_drv->ldev_conf->dim_max,
				ldim_drv->ldev_conf->dim_min);
	} else if (!strcmp(attr->attr.name, "write_check")) {
		ret = sprintf(buf, "iw7019 write_check = %d\n",
			ldim_drv->ldev_conf->write_check);
	} else if (!strcmp(attr->attr.name, "fault_check")) {
		ret = sprintf(buf, "iw7019 fault_check = %d\n",
			ldim_drv->ldev_conf->fault_check);
	} else if (!strcmp(attr->attr.name, "dump")) {
		i = 1000;
		while ((iw7019_spi_op_flag) && (i > 0)) {
			i--;
			udelay(10);
		}
		if (iw7019_spi_op_flag == 0) {
			iw7019_spi_op_flag = 1;
			ret = iw7019_spi_dump(buf);
			iw7019_spi_op_flag = 0;
		} else {
			LDIMERR("%s: wait spi idle state failed\n", __func__);
		}
		ret += sprintf(buf, "\n");
	} else if (!strcmp(attr->attr.name, "fault_cnt")) {
		ret = sprintf(buf, "iw7019 fault_cnt = %d\n", fault_cnt);
	} else if (!strcmp(attr->attr.name, "rw_test")) {
		ret = sprintf(buf, "iw7019 rw_test status:\n"
				"total_test_cnt      = %ld\n"
				"total_err_cnt       = %ld\n"
				"retry_cnt_max       = %d\n"
				"spi_rw_test_en      = %d\n"
				"spi_rw_test_flag    = %d\n",
				spi_rw_test_cnt, spi_rw_test_err,
				spi_rw_test_retry_max,
				spi_rw_test_en, iw7019_spi_rw_test_flag);
	}
	return ret;
}

#define MAX_ARG_NUM 3
static ssize_t iw7019_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct iw7019 *bl = container_of(class, struct iw7019, cls);
	unsigned int val, val2;
	unsigned char reg_addr, reg_val;
	int i;

	if (!strcmp(attr->attr.name, "init")) {
		iw7019_hw_init_on();
	} else if (!strcmp(attr->attr.name, "mode")) {
		i = sscanf(buf, "%d", &val);
		if (i == 1)
			bl->spi->mode = val;
		else
			LDIMERR("%s: invalid args\n", __func__);
	} else if (!strcmp(attr->attr.name, "speed")) {
		i = sscanf(buf, "%d", &val);
		if (i == 1)
			bl->spi->max_speed_hz = val*1000;
		else
			LDIMERR("%s: invalid args\n", __func__);
	} else if (!strcmp(attr->attr.name, "reg")) {
		if (buf[0] == 'w') {
			i = sscanf(buf, "w %x %x", &val, &val2);
			if (i == 2) {
				reg_addr = (unsigned char)val;
				reg_val = (unsigned char)val2;
				iw7019_wreg(bl->spi, reg_addr, reg_val);
			} else {
				LDIMERR("%s: invalid args\n", __func__);
			}
		} else if (buf[0] == 'r') {
			i = sscanf(buf, "r %x", &val);
			if (i == 1) {
				reg_addr = (unsigned char)val;
				iw7019_rreg(bl->spi, reg_addr, &reg_val);
				pr_info("reg 0x%02x = 0x%02x\n",
					reg_addr, reg_val);
			} else {
				LDIMERR("%s: invalid args\n", __func__);
			}
		}
	} else if (!strcmp(attr->attr.name, "test")) {
		i = sscanf(buf, "%d", &val);
		LDIMPR("set test mode to %d\n", val);
		bl->test_mode = val;
	} else if (!strcmp(attr->attr.name, "brightness")) {
		i = sscanf(buf, "%d %d", &val, &val2);
		val &= 0xfff;
		LDIMPR("brightness=%d, index=%d\n", val, val2);
		if ((i == 2) && (val2 < ARRAY_SIZE(test_brightness)))
			test_brightness[val2] = val;
	} else if (!strcmp(attr->attr.name, "write_err_force")) {
		i = sscanf(buf, "%d", &val);
		iw7019_spi_force_err_flag = val;
		LDIMPR("spi_force_err_flag=%d\n", iw7019_spi_force_err_flag);
	} else if (!strcmp(attr->attr.name, "write_check")) {
		i = sscanf(buf, "%d", &val);
		ldim_drv->ldev_conf->write_check = (unsigned char)val;
		LDIMPR("write_check=%d\n", ldim_drv->ldev_conf->write_check);
	} else if (!strcmp(attr->attr.name, "fault_check")) {
		i = sscanf(buf, "%d", &val);
		ldim_drv->ldev_conf->fault_check = (unsigned char)val;
		LDIMPR("fault_check=%d\n", ldim_drv->ldev_conf->fault_check);
	} else if (!strcmp(attr->attr.name, "reset")) {
		i = 1000;
		while ((iw7019_spi_op_flag) && (i > 0)) {
			i--;
			udelay(10);
		}
		if (iw7019_spi_op_flag == 0) {
			iw7019_spi_op_flag = 1;
			iw7019_spi_err_flag = 1;
			iw7019_reset_handler();
			iw7019_spi_op_flag = 0;
		} else {
			LDIMERR("%s: wait spi idle state failed\n", __func__);
		}
	} else if (!strcmp(attr->attr.name, "fault_cnt")) {
		i = sscanf(buf, "%d", &val);
		fault_cnt = (unsigned char)val;
		LDIMPR("fault_cnt=%d\n", fault_cnt);
	} else if (!strcmp(attr->attr.name, "rw_test")) {
		i = sscanf(buf, "%d", &val);
		if (val) {
			if (iw7019_spi_rw_test_flag)
				LDIMPR("rw_test is already running\n");
			else
				queue_work(iw7019_workqueue, &iw7019_work);
		} else {
			spi_rw_test_en = 0;
		}
	} else if (!strcmp(attr->attr.name, "static_test")) {
		i = sscanf(buf, "%d", &val);
		if (val &&  bl_iw7019->test_mode == 0) {
			iw7019_static_pic_test_flag = 1;
			iw7019_static_pic_test_count = 1;
			LDIMPR("static_test is already running\n");
		} else {
			if (bl_iw7019->test_mode)
				LDIMERR("test mode open,please close it!\n");
			iw7019_static_pic_test_flag = 0;
			iw7019_static_pic_test_count = 0;
		}
	} else
		LDIMERR("LDIM argment error!\n");
	return count;
}

static struct class_attribute iw7019_class_attrs[] = {
	__ATTR(init, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(mode, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(speed, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(reg, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(test, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(brightness, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(status, S_IRUGO | S_IWUSR, iw7019_show, NULL),
	__ATTR(write_err_force, S_IRUGO | S_IWUSR, NULL, iw7019_store),
	__ATTR(write_check, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(fault_check, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(fault_cnt, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(reset, S_IRUGO | S_IWUSR, NULL, iw7019_store),
	__ATTR(dump, S_IRUGO | S_IWUSR, iw7019_show, NULL),
	__ATTR(rw_test, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(static_test, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR_NULL
};

static int iw7019_ldim_driver_update(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->device_power_on = iw7019_power_on;
	ldim_drv->device_power_off = iw7019_power_off;
	ldim_drv->device_bri_update = iw7019_smr;
	ldim_drv->device_bri_check = iw7019_chip_check;
	return 0;
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret;

	ldim_drv->spi = spi;

	dev_set_drvdata(&spi->dev, ldim_drv->ldev_conf);
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		LDIMERR("spi setup failed\n");

	/* LDIMPR("%s ok\n", __func__); */
	return ret;
}

static int ldim_spi_dev_remove(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->spi = NULL;
	return 0;
}

static struct spi_driver ldim_spi_dev_driver = {
	.probe = ldim_spi_dev_probe,
	.remove = ldim_spi_dev_remove,
	.driver = {
		.name = "ldim_dev",
		.owner = THIS_MODULE,
	},
};

int ldim_dev_iw7019_probe(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret;

	reset_cnt = 0;
	iw7019_on_flag = 0;
	iw7019_spi_op_flag = 0;
	iw7019_wr_err_cnt = 0;
	iw7019_spi_err_flag = 0;
	iw7019_spi_force_err_flag = 0;
	iw7019_chipid_check_vs_cnt = 0;
	vsync_cnt = 0;
	fault_cnt = 0;
	iw7019_spi_rw_test_flag = 0;
	spi_rw_test_en = 0;
	spin_lock_init(&iw7019_spi_lock);

	bl_iw7019 = kzalloc(sizeof(struct iw7019), GFP_KERNEL);
	if (bl_iw7019 == NULL) {
		pr_err("malloc bl_iw7019 failed\n");
		return -1;
	}

	spi_register_board_info(ldim_drv->spi_dev, 1);
	ret = spi_register_driver(&ldim_spi_dev_driver);
	if (ret) {
		LDIMERR("register ldim_dev spi driver failed\n");
		return -1;
	}

	bl_iw7019->test_mode = 0;
	bl_iw7019->spi = ldim_drv->spi;
	bl_iw7019->cs_hold_delay = ldim_drv->ldev_conf->cs_hold_delay;
	bl_iw7019->cs_clk_delay = ldim_drv->ldev_conf->cs_clk_delay;
	bl_iw7019->cmd_size = ldim_drv->ldev_conf->cmd_size;
	bl_iw7019->init_data = ldim_drv->ldev_conf->init_on;

	iw7019_ldim_driver_update();

	bl_iw7019->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)bl_iw7019->cls.name, "iw7019");
	bl_iw7019->cls.class_attrs = iw7019_class_attrs;
	ret = class_register(&bl_iw7019->cls);
	if (ret < 0)
		pr_err("register iw7019 class failed\n");

	/* init workqueue */
	INIT_WORK(&iw7019_work, iw7019_spi_rw_test);
	iw7019_workqueue = create_workqueue("iw7019_workqueue");
	if (iw7019_workqueue == NULL)
		LDIMERR("can't create iw7019_workqueue\n");

	iw7019_on_flag = 1; /* default enable in uboot */

	/* LDIMPR("%s ok\n", __func__); */
	return ret;
}

int ldim_dev_iw7019_remove(void)
{
	cancel_work_sync(&iw7019_work);
	if (iw7019_workqueue)
		destroy_workqueue(iw7019_workqueue);

	spi_unregister_driver(&ldim_spi_dev_driver);
	kfree(bl_iw7019);
	bl_iw7019 = NULL;

	return 0;
}

