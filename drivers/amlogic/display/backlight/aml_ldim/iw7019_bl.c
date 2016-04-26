/*
 * Amlogic iw7019 Driver for LCD Panel Backlight
 *
 * Author:
 *
 * Copyright (C) 2015 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
static int iw7019_spi_err_flag;
static int iw7019_spi_force_err_flag;
static unsigned short vsync_cnt;
static unsigned short fault_cnt;

static spinlock_t iw7019_spi_lock;

struct ld_config_s iw7019_ld_config = {
	.dim_min = 0x7f, /* min 3% duty */
	.dim_max = 0xfff,
	.cmd_size = 4,
};

#define IW7019_GPIO_ERR    0xffff
struct iw7019 {
	int test_mode;
	unsigned char write_check;
	unsigned char fault_check;
	int en_gpio;
	int lamp_err_gpio;
	int cs_hold_delay;
	int cs_clk_delay;
	struct spi_device *spi;
	struct class cls;
	u8 cur_addr;
};
struct iw7019 *bl_iw7019;

#define IW7019_INIT_ON_SIZE     300
#define IW7019_INIT_OFF_SIZE    8

static u8 iw7019_ini_data[IW7019_INIT_ON_SIZE] = {
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

static int test_brightness[] = {
	0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff
};

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
	u8 addr, val;
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

	for (i = 0; i < IW7019_INIT_ON_SIZE; i += iw7019_ld_config.cmd_size) {
		if (iw7019_ini_data[i] == 0xff) {
			if (iw7019_ini_data[i+3] > 0)
				mdelay(iw7019_ini_data[i+3]);
			break;
		} else if (iw7019_ini_data[i] == 0x0) {
			addr = iw7019_ini_data[i+1];
			val = iw7019_ini_data[i+2];
			ret = iw7019_wreg(bl_iw7019->spi, addr, val);
			udelay(1);
		}
		if (iw7019_ini_data[i+3] > 0)
			mdelay(iw7019_ini_data[i+3]);
	}

	iw7019_spi_err_flag = 0;
	if (flag == IW7019_POWER_RESET)
		return ret;

	iw7019_spi_op_flag = 0;
	return ret;
}

static int iw7019_hw_init(struct iw7019 *bl)
{
	struct aml_ldim_driver_s *ld_drv = aml_ldim_get_driver();

	gpio_direction_output(bl->en_gpio, 1);
	mdelay(2);
	ld_drv->pinmux_ctrl(1);
	mdelay(100);
	iw7019_power_on_init(IW7019_POWER_ON);

	return 0;
}

static int iw7019_hw_init_off(struct iw7019 *bl)
{
	struct aml_ldim_driver_s *ld_drv = aml_ldim_get_driver();

	ld_drv->pinmux_ctrl(0);
	gpio_direction_output(bl->en_gpio, 0);

	return 0;
}

static void iw7019_spi_read(void)
{
	unsigned char i, val;

	for (i = 0; i <= 0x7f; i++) {
		iw7019_rreg(bl_iw7019->spi, i, &val);
		pr_info("iw7019 reg 0x%02x=0x%02x\n", i, val);
	}
	pr_info("\n");
}

static int fault_cnt_save;
static void iw7019_fault_handler(void)
{
	unsigned int val;

	if (fault_cnt >= 35) {
		if (ldim_debug_print) {
			LDIMPR("%s: bypass fault check, fault_cnt=%d\n",
				__func__, fault_cnt);
		}
		return;
	}

	if (vsync_cnt >= 320) {
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
	val = gpio_get_value(bl_iw7019->lamp_err_gpio);
	if (val) {
		fault_cnt++;
		if (fault_cnt < 35) {
			if (ldim_debug_print) {
				LDIMPR("%s: fault detected\n", __func__);
				iw7019_spi_read();
			}
			iw7019_wreg(bl_iw7019->spi, 0x35, 0x43);
		}
	}
}

static int iw7019_reset_handler(void)
{
	/* disable BL_ON once */
	LDIMPR("reset iw7019 BL_ON\n");
	gpio_direction_output(bl_iw7019->en_gpio, 0);
	mdelay(1000);
	gpio_direction_output(bl_iw7019->en_gpio, 1);
	mdelay(2);
	iw7019_power_on_init(IW7019_POWER_RESET);

	return 0;
}

static unsigned int iw7019_get_value(unsigned int level)
{
	unsigned int val;
	unsigned int dim_max, dim_min;

	dim_max = iw7019_ld_config.dim_max;
	dim_min = iw7019_ld_config.dim_min;

	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	return val;
}

static int iw7019_smr(unsigned short *buf, unsigned char len)
{
	int i, offset, cmd_len;
	u8 val[13];
	unsigned int br0, br1;
	unsigned char bri_reg;
	unsigned char temp, reg_chk, clk_sel;

	if (iw7019_on_flag == 0) {
		if (ldim_debug_print) {
			LDIMPR("%s: on_flag=%d\n",
				__func__, iw7019_on_flag);
		}
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
	if (bl_iw7019->write_check) {
		offset = 1;
		val[0] = 0x0f;
		cmd_len = 13;
		bri_reg = IW7019_REG_BRIGHTNESS_CHK;
	} else {
		offset = 0;
		cmd_len = 12;
		bri_reg = IW7019_REG_BRIGHTNESS;
	}
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
		val[i*3 + offset + 1] = ((br0 & 0xf) << 4) | ((br1 >> 8) & 0xf);
		/* br1[7~0] */
		val[i*3 + offset + 2] = br1 & 0xff;
	}
	iw7019_wregs(bl_iw7019->spi, bri_reg, val, cmd_len);

	if (bl_iw7019->write_check) { /* brightness write check */
		iw7019_rreg(bl_iw7019->spi, 0x00, &reg_chk);
		for (i = 1; i < 3; i++) {
			iw7019_rreg(bl_iw7019->spi, 0x00, &temp);
			if (temp != reg_chk)
				break;
		}
		clk_sel = (reg_chk >> 1) & 0x3;
		if ((reg_chk == 0xff) || (clk_sel == 0x1) || (clk_sel == 0x2) ||
			(iw7019_spi_force_err_flag > 0)) {
			iw7019_spi_err_flag = 1;
			iw7019_spi_force_err_flag = 0;
			LDIMPR("%s: spi write failed, 0x00=0x%02x\n",
				__func__, reg_chk);
			iw7019_reset_handler();
			goto iw7019_smr_end;
		}
	}
	if (bl_iw7019->fault_check) {
		if (bl_iw7019->lamp_err_gpio == IW7019_GPIO_ERR)
			goto iw7019_smr_end;
		iw7019_fault_handler();
	}

iw7019_smr_end:
	iw7019_spi_op_flag = 0;
	return 0;
}

static int iw7019_power_on(void)
{
	if (iw7019_on_flag) {
		LDIMPR("%s: iw7019 is already on, exit\n", __func__);
		return 0;
	}
	iw7019_hw_init(bl_iw7019);
	iw7019_on_flag = 1;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7019_power_off(void)
{
	iw7019_hw_init_off(bl_iw7019);
	iw7019_on_flag = 0;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t iw7019_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct iw7019 *bl = container_of(class, struct iw7019, cls);
	int ret = 0;
	u8 val = 0;

	if (!strcmp(attr->attr.name, "cur_addr"))
		ret = sprintf(buf, "0x%02x\n", bl->cur_addr);
	else if (!strcmp(attr->attr.name, "mode"))
		ret = sprintf(buf, "0x%02x\n", bl->spi->mode);
	else if (!strcmp(attr->attr.name, "speed"))
		ret = sprintf(buf, "%d\n", bl->spi->max_speed_hz);
	else if (!strcmp(attr->attr.name, "reg")) {
		iw7019_rreg(bl->spi, bl->cur_addr, &val);
		ret = sprintf(buf, "0x%02x\n", val);
	} else if (!strcmp(attr->attr.name, "test"))
		ret = sprintf(buf, "test mode=%d\n", bl->test_mode);
	else if (!strcmp(attr->attr.name, "brightness")) {
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
				"on_flag       = %d\n"
				"cs_hold_delay = %d\n"
				"cs_clk_delay  = %d\n"
				"spi_op_flag   = %d\n"
				"write_check   = %d\n"
				"fault_check   = %d\n"
				"vsync_cnt     = %d\n"
				"fault_cnt     = %d\n"
				"dim_max       = 0x%03x\n"
				"dim_min       = 0x%03x\n",
				iw7019_on_flag,
				bl->cs_hold_delay, bl->cs_clk_delay,
				iw7019_spi_op_flag,
				bl->write_check, bl->fault_check,
				vsync_cnt, fault_cnt,
				iw7019_ld_config.dim_max,
				iw7019_ld_config.dim_min);
	} else if (!strcmp(attr->attr.name, "write_check")) {
		ret = sprintf(buf, "iw7019 write_check = %d\n",
			bl->write_check);
	} else if (!strcmp(attr->attr.name, "fault_check")) {
		ret = sprintf(buf, "iw7019 fault_check = %d\n",
			bl->fault_check);
	}
	return ret;
}

#define MAX_ARG_NUM 3
static ssize_t iw7019_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	struct iw7019 *bl = container_of(class, struct iw7019, cls);
	unsigned int val, val2;
	int i;

	if (!strcmp(attr->attr.name, "init"))
		iw7019_hw_init(bl);
	else if (!strcmp(attr->attr.name, "cur_addr")) {
		val = kstrtol(buf, 16, NULL);
		bl->cur_addr = val;
	} else if (!strcmp(attr->attr.name, "mode")) {
		val = kstrtol(buf, 16, NULL);
		bl->spi->mode = val;
	} else if (!strcmp(attr->attr.name, "speed")) {
		val = kstrtol(buf, 16, NULL);
		bl->spi->max_speed_hz = val*1000;
	} else if (!strcmp(attr->attr.name, "reg")) {
		val = kstrtol(buf, 16, NULL);
		iw7019_wreg(bl->spi, bl->cur_addr, val);
		bl->cur_addr++;
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
		bl_iw7019->write_check = (unsigned char)val;
		LDIMPR("write_check=%d\n", bl_iw7019->write_check);
	} else if (!strcmp(attr->attr.name, "fault_check")) {
		i = sscanf(buf, "%d", &val);
		bl_iw7019->fault_check = (unsigned char)val;
		LDIMPR("fault_check=%d\n", bl_iw7019->fault_check);
	} else
		LDIMPR("LDIM argment error!\n");
	return count;
}

static struct class_attribute iw7019_class_attrs[] = {
	__ATTR(init, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(cur_addr, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(mode, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(speed, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(reg, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(test, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(brightness, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(status, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(write_err_force, S_IRUGO | S_IWUSR, NULL, iw7019_store),
	__ATTR(write_check, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(fault_check, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR_NULL
};

static int iw7019_ldim_driver_update(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_drv->ld_config = &iw7019_ld_config;
	ldim_drv->device_power_on = iw7019_power_on;
	ldim_drv->device_power_off = iw7019_power_off;
	ldim_drv->device_bri_update = iw7019_smr;
	return 0;
}

static int iw7019_get_config_from_dts(struct device_node *np)
{
	unsigned int temp[3], val;
	unsigned char cmd_size;
	int i, j;
	int ret = 0;

	bl_iw7019->en_gpio = of_get_named_gpio(np, "en-gpio", 0);
	LDIMPR("en_gpio = %d\n", bl_iw7019->en_gpio);
	if (bl_iw7019->en_gpio)
		gpio_request(bl_iw7019->en_gpio, "iw7019_en");
	ret = of_property_read_u32(np, "spi_write_check", &temp[0]);
	if (ret)
		bl_iw7019->write_check = 0;
	else
		bl_iw7019->write_check = (unsigned char)temp[0];

	ret = of_property_read_u32(np, "lamp_err_check", &temp[0]);
	if (ret)
		bl_iw7019->fault_check = 0;
	else
		bl_iw7019->fault_check = (unsigned char)temp[0];
	if (bl_iw7019->fault_check) {
		ret = of_property_read_u32_array(np,
			"lamp_err-gpio", &temp[0], 3);
		if (ret) {
			bl_iw7019->lamp_err_gpio = IW7019_GPIO_ERR;
			bl_iw7019->fault_check = 0;
			LDIMERR("failed to get lamp_err-gpio\n");
		} else {
			bl_iw7019->lamp_err_gpio =
				of_get_named_gpio(np, "lamp_err-gpio", 0);
			LDIMPR("lamp_err_gpio = %d\n",
				bl_iw7019->lamp_err_gpio);
			if (bl_iw7019->lamp_err_gpio) {
				gpio_request(bl_iw7019->lamp_err_gpio,
					"iw7019_lamp_err");
				gpio_direction_input(bl_iw7019->lamp_err_gpio);
			} else {
				LDIMERR("request lamp_err-gpio error\n");
			}
		}
	} else {
		bl_iw7019->lamp_err_gpio = IW7019_GPIO_ERR;
	}

	ret = of_property_read_u32_array(np, "spi_cs_delay", &temp[0], 2);
	if (ret) {
		bl_iw7019->cs_hold_delay = 0;
		bl_iw7019->cs_clk_delay = 0;
	} else {
		bl_iw7019->cs_hold_delay = temp[0];
		bl_iw7019->cs_clk_delay = temp[1];
	}

	ret = of_property_read_u32_array(np, "dim_max_min", &temp[0], 2);
	if (ret) {
		LDIMERR("failed to get dim_max_min\n");
		iw7019_ld_config.dim_max = 0xfff;
		iw7019_ld_config.dim_min = 0x7f;
	} else {
		iw7019_ld_config.dim_max = temp[0];
		iw7019_ld_config.dim_min = temp[1];
	}

	/* get init_cmd */
	ret = of_property_read_u32(np, "cmd_size", &temp[0]);
	if (ret) {
		LDIMERR("failed to get cmd_size\n");
		iw7019_ld_config.cmd_size = 4;
	} else {
		iw7019_ld_config.cmd_size = (unsigned char)temp[0];
	}
	cmd_size = iw7019_ld_config.cmd_size;
	if (cmd_size > 1) {
		i = 0;
		while (i < IW7019_INIT_ON_SIZE) {
			for (j = 0; j < cmd_size; j++) {
				ret = of_property_read_u32_index(
					np, "init_on", (i + j), &val);
				if (ret) {
					LDIMERR("get init_on failed\n");
					iw7019_ini_data[i] = 0xff;
					goto iw7019_get_init_on_dts;
				}
				iw7019_ini_data[i + j] = (unsigned char)val;
			}
			if (iw7019_ini_data[i] == 0xff)
				break;
			i += cmd_size;
		}
	} else {
		iw7019_ld_config.cmd_size = 1;
	}
iw7019_get_init_on_dts:
	return 0;
}

static int iw7019_probe(struct spi_device *spi)
{
	const char *str;
	int ret;

	iw7019_on_flag = 1; /* default enable in uboot */
	iw7019_spi_op_flag = 0;
	iw7019_spi_err_flag = 0;
	iw7019_spi_force_err_flag = 0;
	vsync_cnt = 0;
	fault_cnt = 0;
	spin_lock_init(&iw7019_spi_lock);

	ret = of_property_read_string(spi->dev.of_node, "status", &str);
	if (ret) {
		LDIMPR("get status failed\n");
		return 0;
	} else {
		if (strncmp(str, "disabled", 2) == 0)
			return 0;
	}
	bl_iw7019 = kzalloc(sizeof(struct iw7019), GFP_KERNEL);
	bl_iw7019->test_mode = 0;
	bl_iw7019->spi = spi;
	iw7019_get_config_from_dts(spi->dev.of_node);

	dev_set_drvdata(&spi->dev, bl_iw7019);
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);

	iw7019_ldim_driver_update();

	bl_iw7019->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)bl_iw7019->cls.name, "iw7019");
	bl_iw7019->cls.class_attrs = iw7019_class_attrs;
	ret = class_register(&bl_iw7019->cls);
	if (ret < 0)
		pr_err("register class failed! (%d)\n", ret);

	pr_info("%s ok\n", __func__);
	return ret;
}

static int iw7019_remove(struct spi_device *spi)
{
	kfree(bl_iw7019);
	bl_iw7019 = NULL;
	return 0;
}

static int iw7019_suspend(struct spi_device *spi, pm_message_t mesg)
{
	/* @todo iw7019_suspend */
	return 0;
}

static int iw7019_resume(struct spi_device *spi)
{
	/* @todo iw7019_resume */
	return 0;
}

static const struct of_device_id iw7019_of_match[] = {
	{	.compatible = "amlogic, iw7019", },
};
MODULE_DEVICE_TABLE(of, iw7019_of_match);

static struct spi_driver iw7019_driver = {
	.probe = iw7019_probe,
	.remove = iw7019_remove,
	.suspend	= iw7019_suspend,
	.resume		= iw7019_resume,
	.driver = {
		.name = "iw7019",
		.owner = THIS_MODULE,
		.bus	= &spi_bus_type,
		.of_match_table = iw7019_of_match,
	},
};

static int __init iw7019_init(void)
{
	/*printk("%s start\n", __func__);*/
	LDIMPR("%s start\n", __func__);
	return spi_register_driver(&iw7019_driver);
}

static void __exit iw7019_exit(void)
{
	spi_unregister_driver(&iw7019_driver);
}

fs_initcall(iw7019_init);
module_exit(iw7019_exit);


MODULE_DESCRIPTION("iW7019 LED Driver for LCD Backlight");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");

