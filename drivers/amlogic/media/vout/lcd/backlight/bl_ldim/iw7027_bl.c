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
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define NORMAL_MSG      (0 << 7)
#define BROADCAST_MSG   BIT(7)
#define BLOCK_DATA      (0 << 6)
#define SINGLE_DATA     BIT(6)
#define IW7027_CLASS_NAME "iw7027"

#define IW7027_REG_MAX             0x100
#define IW7027_REG_BRIGHTNESS_CHK  0x00
#define IW7027_REG_BRIGHTNESS      0x01
#define IW7027_REG_CHIPID          0x7f
#define IW7027_CHIPID              0x28

#define VSYNC_INFO_FREQUENT        300

/* for spi xfer */
static spinlock_t spi_lock;
static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

struct iw7027_s {
	unsigned int dev_on_flag;
	unsigned char dma_support;
	unsigned short vsync_cnt;
	unsigned short fault_cnt;
	unsigned int fault_cnt_save;
	unsigned int fault_check_cnt;
	unsigned int reg_buf_size;
	unsigned int tbuf_size;
	unsigned int rbuf_size;

	unsigned char *reg_buf; /* local dimming driver smr api usage */

	/* spi api internal used, don't use outside!!! */
	unsigned char *tbuf;
	unsigned char *rbuf;
};
struct iw7027_s *bl_iw7027;

/* write single device */
static int spi_wregs(struct spi_device *spi, unsigned char chip_id, unsigned int chip_cnt,
		     unsigned char reg, unsigned char *data_buf, unsigned int tlen)
{
	unsigned char *tbuf;
	int n, xlen, ret;

	if (!bl_iw7027 || !bl_iw7027->tbuf) {
		LDIMERR("%s: bl_iw7027 or tbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7027->tbuf;

	if (chip_id == 0x00 || chip_id == 0x3f) {
		LDIMERR("%s: chip_id 0x%02x is invalid\n", __func__, chip_id);
		return -1;
	}
	if (tlen == 0) {
		LDIMERR("%s: tlen is 0\n", __func__);
		return -1;
	}
	n = chip_id - 1;
	if (tlen == 1)
		xlen = 2 + 1 + n;
	else
		xlen = 3 + tlen + n;
	if (bl_iw7027->tbuf_size < xlen) {
		LDIMERR("%s: tbuf_size %d is not enough\n", __func__, bl_iw7027->tbuf_size);
		return -1;
	}

	if (tlen == 1) {
		tbuf[0] = NORMAL_MSG | SINGLE_DATA | (chip_id & 0x3f);
		tbuf[1] = reg & 0x7f;
		tbuf[2] = data_buf[0];
		memset(&tbuf[3], 0, n);
	} else {
		tbuf[0] = NORMAL_MSG | BLOCK_DATA | (chip_id & 0x3f);
		tbuf[1] = tlen;
		tbuf[2] = reg & 0x7f;
		memcpy(&tbuf[3], data_buf, tlen);
		memset(&tbuf[3 + tlen], 0, n);
	}
	ret = ldim_spi_write(spi, tbuf, xlen);

	return ret;
}

static int spi_high_reg_switch(struct spi_device *spi, unsigned char chip_id, unsigned int chip_cnt,
			       int flag)
{
	unsigned char temp;
	int ret;

	if (flag)
		temp = 0x80;
	else
		temp = 0x00;
	ret = spi_wregs(spi, chip_id, chip_cnt, 0x78, &temp, 1);
	if (ret)
		return -1;

	return 0;
}

/* read single device */
static int spi_rregs(struct spi_device *spi, unsigned char chip_id, unsigned int chip_cnt,
		     unsigned char reg, unsigned char *data_buf, unsigned int rlen)
{
	unsigned char *tbuf, *rbuf;
	int n, xlen, ret = 0;
	int i;

	if (!bl_iw7027 || !bl_iw7027->tbuf || !bl_iw7027->rbuf) {
		LDIMERR("%s: bl_iw7027 or tbuf or rbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7027->tbuf;
	rbuf = bl_iw7027->rbuf;

	if (chip_id == 0x00 || chip_id == 0x3f) {
		LDIMERR("%s: chip_id 0x%02x is invalid\n", __func__, chip_id);
		return -1;
	}
	if (rlen == 0) {
		LDIMERR("%s: rlen is 0\n", __func__);
		return -1;
	}
	n = chip_cnt + rlen;
	if (rlen == 1)
		xlen = 2 + n;
	else
		xlen = 3 + n;
	if (bl_iw7027->tbuf_size < xlen) {
		LDIMERR("%s: tbuf_size %d is not enough\n", __func__, bl_iw7027->tbuf_size);
		return -1;
	}
	if (bl_iw7027->rbuf_size < xlen) {
		LDIMERR("%s: rbuf_size %d is not enough\n", __func__, bl_iw7027->rbuf_size);
		return -1;
	}

	if (reg >= 0x80)
		ret = spi_high_reg_switch(spi, chip_id, chip_cnt, 1);
	if (ret)
		return -1;

	if (rlen == 1) {
		tbuf[0] = NORMAL_MSG | SINGLE_DATA | (chip_id & 0x3f);
		tbuf[1] = (reg & 0x7f) | 0x80;
		memset(&tbuf[2], 0, n);
	} else {
		tbuf[0] = NORMAL_MSG | BLOCK_DATA | (chip_id & 0x3f);
		tbuf[1] = rlen;
		tbuf[2] = (reg & 0x7f) | 0x80;
		memset(&tbuf[3], 0, n);
	}
	ret = ldim_spi_read_sync(spi, tbuf, rbuf, xlen);
	if (ret)
		return -1;

	memcpy(data_buf, &rbuf[xlen - rlen], rlen);

	if (reg >= 0x80)
		ret = spi_high_reg_switch(spi, chip_id, chip_cnt, 0);
	if (ret)
		return -1;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		LDIMPR("%s: reg=0x%02x, rlen=%d, data_buf:\n", __func__, reg, rlen);
		for (i = 0; i < rlen; i++)
			pr_info(" 0x%02x\n", data_buf[i]);
	}

	return ret;
}

/* write same data to all device */
static int spi_wregs_all(struct spi_device *spi, unsigned int chip_cnt,
			 unsigned char reg, unsigned char *data_buf, int tlen)
{
	unsigned char *tbuf;
	int n, xlen, ret;

	if (!bl_iw7027 || !bl_iw7027->tbuf) {
		LDIMERR("%s: bl_iw7027 or tbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7027->tbuf;

	if (tlen == 0) {
		LDIMERR("%s: tlen is 0\n", __func__);
		return -1;
	}
	n = chip_cnt - 1;
	if (tlen == 1)
		xlen = 2 + 1 + n;
	else
		xlen = 3 + tlen + n;
	if (bl_iw7027->tbuf_size < xlen) {
		LDIMERR("%s: tbuf_size %d is not enough\n", __func__, bl_iw7027->tbuf_size);
		return -1;
	}

	if (tlen == 1) {
		tbuf[0] = BROADCAST_MSG | SINGLE_DATA | 0x00;
		tbuf[1] = reg & 0x7f;
		tbuf[2] = data_buf[0];
		memset(&tbuf[3], 0, n);
	} else {
		tbuf[0] = BROADCAST_MSG | BLOCK_DATA | 0x00;
		tbuf[1] = tlen;
		tbuf[2] = reg & 0x7f;
		memcpy(&tbuf[3], data_buf, tlen);
		memset(&tbuf[3 + tlen], 0, n);
	}
	ret = ldim_spi_write(spi, tbuf, xlen);

	return ret;
}

/* write diff data to all device */
static int spi_wregs_duty(struct spi_device *spi, unsigned int chip_cnt, unsigned char spi_sync,
			  unsigned char reg, unsigned char *data_buf, int tlen)
{
	unsigned char *tbuf, *rbuf;
	int i, n, xlen, ret;

	if (!bl_iw7027 || !bl_iw7027->tbuf) {
		LDIMERR("%s: bl_iw7027 or tbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7027->tbuf;
	rbuf = bl_iw7027->rbuf;

	if (tlen == 0) {
		LDIMERR("%s: tlen is 0\n", __func__);
		return -1;
	}
	n = chip_cnt - 1;
	if (tlen == 1)
		xlen = 2 + chip_cnt + n;
	else
		xlen = 3 + tlen * chip_cnt + n;
	if (bl_iw7027->tbuf_size < xlen) {
		LDIMERR("%s: tbuf_size %d is not enough\n", __func__, bl_iw7027->tbuf_size);
		return -1;
	}
	if (bl_iw7027->rbuf_size < xlen) {
		LDIMERR("%s: rbuf_size %d is not enough\n", __func__, bl_iw7027->rbuf_size);
		return -1;
	}

	if (tlen == 1) {
		tbuf[0] = BROADCAST_MSG | SINGLE_DATA | 0x3f;
		tbuf[1] = reg & 0x7f;
		for (i = 0; i < chip_cnt; i++)
			tbuf[2 + i] = data_buf[0];
		memset(&tbuf[2 + chip_cnt], 0, n);
	} else {
		tbuf[0] = BROADCAST_MSG | BLOCK_DATA | 0x3f;
		tbuf[1] = tlen;
		tbuf[2] = reg & 0x7f;
		for (i = 0; i < chip_cnt; i++)
			memcpy(&tbuf[3 + i * tlen], &data_buf[i * tlen], tlen);
		memset(&tbuf[3 + tlen * chip_cnt], 0, n);
	}

	if (spi_sync)
		ret = ldim_spi_write(spi, tbuf,	xlen);
	else
		ret = ldim_spi_write_async(spi, tbuf, rbuf, xlen,
			bl_iw7027->dma_support, bl_iw7027->tbuf_size);

	return ret;
}

static int iw7027_reg_write(struct ldim_dev_driver_s *dev_drv, unsigned char chip_id,
			    unsigned char reg, unsigned char *buf, unsigned int len)
{
	int ret;

	mutex_lock(&spi_mutex);

	ret = spi_wregs(dev_drv->spi_dev, chip_id, dev_drv->chip_cnt, reg, buf, len);
	if (ret)
		LDIMERR("%s: chip_id[%d] reg 0x%x error\n", __func__, chip_id, reg);

	mutex_unlock(&spi_mutex);

	return ret;
}

static int iw7027_reg_read(struct ldim_dev_driver_s *dev_drv, unsigned char chip_id,
			   unsigned char reg, unsigned char *buf, unsigned int len)
{
	int ret;

	mutex_lock(&spi_mutex);

	memset(buf, 0, len);
	ret = spi_rregs(dev_drv->spi_dev, chip_id, dev_drv->chip_cnt, reg, buf, len);
	if (ret)
		LDIMERR("%s: chip_id[%d] reg 0x%x error\n", __func__, chip_id, reg);

	mutex_unlock(&spi_mutex);

	return ret;
}

static int iw7027_reg_write_all(struct ldim_dev_driver_s *dev_drv,
				unsigned char reg, unsigned char *buf, unsigned int len)
{
	int ret;

	mutex_lock(&spi_mutex);

	ret = spi_wregs_all(dev_drv->spi_dev, dev_drv->chip_cnt, reg, buf, len);
	if (ret)
		LDIMERR("%s: reg 0x%x, len %d error\n", __func__, reg, len);

	mutex_unlock(&spi_mutex);

	return ret;
}

static int iw7027_reg_write_duty(struct ldim_dev_driver_s *dev_drv,
				 unsigned char reg, unsigned char *buf, unsigned int len)
{
	int ret;
	unsigned long flags = 0;

	if (dev_drv->spi_sync)
		mutex_lock(&spi_mutex);
	else
		spin_lock_irqsave(&spi_lock, flags);

	ret = spi_wregs_duty(dev_drv->spi_dev, dev_drv->chip_cnt, dev_drv->spi_sync, reg, buf, len);
	if (ret)
		LDIMERR("%s: reg 0x%x, len %d error\n", __func__, reg, len);

	if (dev_drv->spi_sync)
		mutex_unlock(&spi_mutex);
	else
		spin_unlock_irqrestore(&spi_lock, flags);

	return ret;
}

static int ldim_power_cmd_dynamic_size(struct ldim_dev_driver_s *dev_drv)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	table = dev_drv->init_on;
	max_len = dev_drv->init_on_cnt;

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
				lcd_delay_ms(delay_ms);
		} else if (type == LCD_EXT_CMD_TYPE_CMD) {
			ret = iw7027_reg_write_all(dev_drv, table[i + 2],
						   &table[i + 3], (cmd_size - 1));
			udelay(1);
		} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			ret = iw7027_reg_write_all(dev_drv, table[i + 2],
						   &table[i + 3], (cmd_size - 2));
			udelay(1);
			if (table[i + 2 + cmd_size - 1] > 0)
				lcd_delay_ms(table[i + 2 + cmd_size - 1]);
		} else {
			LDIMERR("%s: type 0x%02x invalid\n", __func__, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int iw7027_power_on_init(struct ldim_dev_driver_s *dev_drv)
{
	int ret = 0;

	if (dev_drv->cmd_size < 1) {
		LDIMERR("%s: cmd_size %d is invalid\n",
			__func__, dev_drv->cmd_size);
		return -1;
	}
	if (!dev_drv->init_on) {
		LDIMERR("%s: init_on is null\n", __func__);
		return -1;
	}

	if (dev_drv->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
		ret = ldim_power_cmd_dynamic_size(dev_drv);

	return ret;
}

static int iw7027_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	unsigned char chip_id, reg, temp[2];
	int i, retry_cnt = 0;

	LDIMPR("%s\n", __func__);

	chip_id = 0x01;

	/* step 1: system power_on */
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);

	/* step 2: Keep Vsync signal at low level */
	/* step 3: delay for internal logic stable */
	lcd_delay_ms(10);

iw7027_hw_init_on_retry:
	/* SPI communication check */
	for (i = 1; i <= 10; i++) {
		/* step 4: Write 0X00=0X06 */
		reg = 0x00;
		temp[0] = 0x06;
		iw7027_reg_write_all(dev_drv, reg, temp, 1);
		/* step 5: Read if 0x00=0x06,go on to step 6,or step 4 loop until 0x00 = 0x06 */
		iw7027_reg_read(dev_drv, chip_id, reg, temp, 1);
		if (temp[0] == 0x06)
			break;
	}
	if (i == 10)
		LDIMERR("%s: SPI communication check error\n", __func__);

	/* step 6: configure initial registers */
	iw7027_power_on_init(dev_drv);

	/* step 7: Generate external VSYNC to VSYNC/PWM pin */
	ldim_set_duty_pwm(&dev_drv->ldim_pwm_config);
	ldim_set_duty_pwm(&dev_drv->analog_pwm_config);
	dev_drv->pinmux_ctrl(dev_drv, 1);

	/* step 8: delay for system clock and light bar PSU stable */
	lcd_delay_ms(520);

	/* start calibration */
	/* step 9: set [SOFT_RST_N] = 1 to exit reset state */
	reg = 0x00;
	temp[0] = 0x07;
	iw7027_reg_write_all(dev_drv, reg, temp, 1);
	//lcd_delay_ms(200);

	/* step 10: Set 0X78[7]=1; no need here */
	/* step 11: Wait until reg0xb3 LSB 4bit is 0101//It means iW7027 enter OTF state */
	i = 0;
	while (i++ < 500) {
		reg = 0xb3;
		iw7027_reg_read(dev_drv, chip_id, reg, temp, 1);
		/*VDAC statue reg :FB1=[0x5] FB2=[0x50]*/
		/*The current platform using FB1*/
		if ((temp[0] & 0xf) == 0x05)
			break;
		lcd_delay_ms(1);
	}
	/* step 12: Read 0x85,0x86 */
	reg = 0x85;
	iw7027_reg_read(dev_drv, chip_id, reg, temp, 2);

	/* step 13: Set 0X78[7]=0, no need */
	/* step 14: check if power on voltage exist */
	if (temp[0] || temp[1]) {
		LDIMERR("%s: 0x85,0x86 is not zero\n", __func__);
		if (retry_cnt++ < 3) {
			LDIMPR("%s: retry %d\n", __func__, retry_cnt);
			goto iw7027_hw_init_on_retry;
		}
	} else {
		/* step 15: calibration done */
		LDIMPR("%s: calibration done\n", __func__);
	}

	//reg = 0x30;
	//temp[0] = 0x1d;
	//iw7027_reg_write_all(dev_drv, chip_id, reg, temp, 1);
	//reg = 0x29;
	//temp[0] = 0x84;
	//iw7027_reg_write_all(dev_drv, chip_id, reg, temp, 1);

	return 0;
}

static int iw7027_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static int iw7027_spi_dump_reg(struct ldim_dev_driver_s *dev_drv, char *buf)
{
	unsigned char *data;
	int i, len, ret;

	if (!buf)
		return 0;

	mutex_lock(&dev_mutex);

	data = kcalloc(0x80, sizeof(unsigned char), GFP_KERNEL);
	if (!data) {
		mutex_unlock(&dev_mutex);
		return sprintf(buf, "data buf malloc failed\n");
	}

	len = sprintf(buf, "iw7027 reg dump:\n");
	ret = iw7027_reg_read(dev_drv, 0x1, 0x0, data, 0x80);
	if (ret) {
		kfree(data);
		len += sprintf(buf + len, "%s error\n", __func__);
		mutex_unlock(&dev_mutex);
		return len;
	}
	for (i = 0; i < 0x80; i++)
		len += sprintf(buf + len, "  0x%02x=0x%02x\n", i, data[i]);

	ret = iw7027_reg_read(dev_drv, 0x1, 0x80, data, 0x80);
	if (ret) {
		kfree(data);
		len += sprintf(buf + len, "%s error\n", __func__);
		mutex_unlock(&dev_mutex);
		return len;
	}
	for (i = 0; i < 0x80; i++)
		len += sprintf(buf + len, "  0x%02x=0x%02x\n", (i + 0x80), data[i]);

	len += sprintf(buf + len, "\n");

	kfree(data);
	mutex_unlock(&dev_mutex);

	return len;
}

static int iw7027_spi_dump_duty(struct ldim_dev_driver_s *dev_drv, char *buf)
{
	unsigned char *data, id;
	int i, len, ret;

	if (!buf)
		return 0;

	mutex_lock(&dev_mutex);

	data = kcalloc(0x20, sizeof(unsigned char), GFP_KERNEL);
	if (!data) {
		mutex_unlock(&dev_mutex);
		return sprintf(buf, "data buf malloc failed\n");
	}

	len = sprintf(buf, "iw7027 duty dump:\n");
	for (id = 1; id <= dev_drv->chip_cnt; id++) {
		len += sprintf(buf, "chip_id[%d]:\n", id);
		ret = iw7027_reg_read(dev_drv, id, 0x40, data, 0x20);
		if (ret) {
			kfree(data);
			len += sprintf(buf + len, "%s error\n", __func__);
			mutex_unlock(&dev_mutex);
			return len;
		}
		for (i = 0; i < 0x20; i++)
			len += sprintf(buf + len, "  0x%02x=0x%02x\n", (i + 0x40), data[i]);
	}
	len += sprintf(buf + len, "\n");

	kfree(data);
	mutex_unlock(&dev_mutex);

	return len;
}

static void ldim_vs_debug_info(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j, zone_max;

	if (bl_iw7027->vsync_cnt)
		return;
	if (ldim_debug_print != 3)
		return;

	LDIMPR("%s:\n", __func__);
	zone_max = bl_iw7027->reg_buf_size / 2;
	for (i = 0; i < dev_drv->zone_num; i++) {
		j = dev_drv->bl_mapping[i];
		if (j >= zone_max) {
			LDIMERR("mapping[%d]=%d invalid, max %d\n",
			       i, j, zone_max);
			return;
		}

		LDIMPR("zone[%d]: reg_buf:[%d]=0x%02x, [%d]=0x%02x\n",
		       i, 2 * j, bl_iw7027->reg_buf[2 * j],
			2 * j + 1, bl_iw7027->reg_buf[2 * j + 1]);
	}
	pr_info("\n");
}

static inline void ldim_data_mapping(unsigned int *duty_buf,
				     unsigned int max, unsigned int min,
				     unsigned int zone_num,
				     unsigned short *mapping)
{
	unsigned int i, j, val, zone_max;

	zone_max = bl_iw7027->reg_buf_size / 2;
	for (i = 0; i < zone_num; i++) {
		val = min + ((duty_buf[i] * (max - min)) / LD_DATA_MAX);
		j = mapping[i];
		if (j >= zone_max) {
			if (bl_iw7027->vsync_cnt == 0) {
				LDIMPR("%s: mapping[%d]=%d invalid, max %d\n",
				       __func__, i, j, zone_max);
			}
			return;
		}
		bl_iw7027->reg_buf[2 * j] = (val >> 8) & 0xf;
		bl_iw7027->reg_buf[2 * j + 1] = val & 0xff;
	}
}

static int iw7027_smr(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int value_flag = 0;
	int i;

	if (!bl_iw7027)
		return -1;

	if (bl_iw7027->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_iw7027->vsync_cnt = 0;

	if (bl_iw7027->dev_on_flag == 0) {
		if (bl_iw7027->vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, bl_iw7027->dev_on_flag);
		return 0;
	}
	if (len != dev_drv->zone_num) {
		if (bl_iw7027->vsync_cnt == 0)
			LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (!bl_iw7027->reg_buf) {
		if (bl_iw7027->vsync_cnt == 0)
			LDIMERR("%s: reg_buf is null\n", __func__);
		return -1;
	}

	/* this function will cause backlight flicker */
	/*if (ldim_drv->vsync_change_flag == 50) {
	 *	spi_wreg(dev_drv->spi_dev, 0x31, 0xd7);
	 *	ldim_drv->vsync_change_flag = 0;
	 *} else if (ldim_drv->vsync_change_flag == 60) {
	 *	spi_wreg(dev_drv->spi_dev, 0x31, 0xd3);
	 *	ldim_drv->vsync_change_flag = 0;
	 *}
	 */

	for (i = 0; i < dev_drv->zone_num; i++)
		value_flag += buf[i];
	if (value_flag == 0) {
		for (i = 0; i < (dev_drv->zone_num * 2); i++)
			bl_iw7027->reg_buf[i] = 0;
	} else {
		ldim_data_mapping(buf, dev_drv->dim_max, dev_drv->dim_min,
				  dev_drv->zone_num, dev_drv->bl_mapping);
	}
	ldim_vs_debug_info(ldim_drv);

	iw7027_reg_write_duty(dev_drv, 0x40, bl_iw7027->reg_buf, 0x20);

	return 0;
}

static int iw7027_smr_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7027)
		return -1;

	if (bl_iw7027->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_iw7027->vsync_cnt = 0;

	return 0;
}

static int iw7027_fault_handler(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;

	if (dev_drv->fault_check == 0)
		return 0;

	if (!bl_iw7027)
		return 0;
	if (bl_iw7027->dev_on_flag == 0) {
		if (bl_iw7027->vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, bl_iw7027->dev_on_flag);
		return 0;
	}

	mutex_lock(&dev_mutex);
	//step1: check dev_drv->lamp_err_gpio
	//step2: if error, change bl_iw7027->dev_on_flag to 0, and reset iw7027
	//step3: ret = -1, so main driver can refresh bl_duty after reset dev
	mutex_unlock(&dev_mutex);

	return ret;
}

static int iw7027_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7027)
		return -1;

	if (bl_iw7027->dev_on_flag) {
		LDIMPR("%s: iw7027 is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	ldim_spi_async_busy_clear();
	iw7027_hw_init_on(ldim_drv->dev_drv);
	bl_iw7027->dev_on_flag = 1;
	bl_iw7027->vsync_cnt = 0;
	bl_iw7027->fault_cnt = 0;
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7027_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7027)
		return -1;

	mutex_lock(&dev_mutex);
	bl_iw7027->dev_on_flag = 0;
	iw7027_hw_init_off(ldim_drv->dev_drv);
	ldim_spi_async_busy_clear();
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7027_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

//	LDIMPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static ssize_t iw7027_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int i, j, temp, len = 0;

	if (!bl_iw7027)
		return sprintf(buf, "bl_iw7027 is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (!strcmp(attr->attr.name, "iw7027_status")) {
		len = sprintf(buf, "iw7027 status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n"
				"fault_cnt      = %d\n"
				"reg_buf_size   = %d\n"
				"tbuf_size      = %d\n"
				"rbuf_size      = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"cs_hold_delay  = %d\n"
				"cs_clk_delay   = %d\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n",
				dev_drv->index,
				bl_iw7027->dev_on_flag,
				bl_iw7027->vsync_cnt,
				bl_iw7027->fault_cnt,
				bl_iw7027->reg_buf_size,
				bl_iw7027->tbuf_size,
				bl_iw7027->rbuf_size,
				dev_drv->en_gpio_on,
				dev_drv->en_gpio_off,
				dev_drv->cs_hold_delay,
				dev_drv->cs_clk_delay,
				dev_drv->dim_max,
				dev_drv->dim_min);
	} else if (!strcmp(attr->attr.name, "dump_buf")) {
		if (!bl_iw7027->reg_buf || bl_iw7027->reg_buf_size == 0)
			return sprintf(buf, "bl_iw7027 reg_buf is null\n");
		len = sprintf(buf, "bl_matrix spi buffer:\n");
		for (i = 0 ; i < dev_drv->chip_cnt; i++) {
			for (j = 0; j < 16; j++) {
				temp = (i * 16 + j) * 2;
				if (temp >= bl_iw7027->reg_buf_size)
					break;
				len += sprintf(buf + len, " 0x%02x", bl_iw7027->reg_buf[temp]);
				if ((temp + 1) >= bl_iw7027->reg_buf_size)
					break;
				len += sprintf(buf + len, " 0x%02x", bl_iw7027->reg_buf[temp + 1]);
			}
			len += sprintf(buf + len, "\n");
		}
	} else if (!strcmp(attr->attr.name, "dump_reg")) {
		len = iw7027_spi_dump_reg(dev_drv, buf);
	} else if (!strcmp(attr->attr.name, "dump_duty")) {
		len = iw7027_spi_dump_duty(dev_drv, buf);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t iw7027_store(struct class *class, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	unsigned int val, val2;
	unsigned char reg, data;
	int n;

	dev_drv = ldim_drv->dev_drv;
	if (!strcmp(attr->attr.name, "init")) {
		mutex_lock(&dev_mutex);
		iw7027_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(attr->attr.name, "chip")) {
		n = sscanf(buf, "all %x %x", &val, &val2);
		if (n == 2) {
			mutex_lock(&dev_mutex);
			reg = (unsigned char)val;
			data = (unsigned char)val2;
			iw7027_reg_write_all(dev_drv, reg, &data, 1);
			mutex_unlock(&dev_mutex);
		} else {
			LDIMERR("%s: invalid args\n", __func__);
		}
	} else if (!strcmp(attr->attr.name, "dma")) {
		n = sscanf(buf, "set %d", &val);
		if (n == 1)
			bl_iw7027->dma_support = val;
		LDIMPR("dma_support: %d\n", bl_iw7027->dma_support);
	} else {
		LDIMERR("argument error!\n");
	}
	return count;
}

static struct class_attribute iw7027_class_attrs[] = {
	__ATTR(init, 0644, NULL, iw7027_store),
	__ATTR(dma, 0644, NULL, iw7027_store),
	__ATTR(iw7027_status, 0644, iw7027_show, NULL),
	__ATTR(dump_buf, 0644, iw7027_show, NULL),
	__ATTR(dump_reg, 0644, iw7027_show, NULL),
	__ATTR(dump_duty, 0644, iw7027_show, NULL)
};

static int iw7027_ldim_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = iw7027_power_on;
	dev_drv->power_off = iw7027_power_off;
	dev_drv->dev_smr = iw7027_smr;
	dev_drv->dev_smr_dummy = iw7027_smr_dummy;
	dev_drv->dev_err_handler = iw7027_fault_handler;
	dev_drv->config_update = iw7027_config_update;

	dev_drv->reg_write = iw7027_reg_write;
	dev_drv->reg_read = iw7027_reg_read;
	return 0;
}

int ldim_dev_iw7027_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int n, i;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}
	if (!dev_drv->spi_dev) {
		LDIMERR("%s: spi_dev is null\n", __func__);
		return -1;
	}
	if (dev_drv->chip_cnt == 0) {
		LDIMERR("%s: chip_cnt is 0\n", __func__);
		return -1;
	}

	bl_iw7027 = kzalloc(sizeof(*bl_iw7027), GFP_KERNEL);
	if (!bl_iw7027)
		return -1;

	bl_iw7027->dev_on_flag = 0;
	bl_iw7027->vsync_cnt = 0;
	bl_iw7027->fault_cnt = 0;
	bl_iw7027->dma_support = dev_drv->dma_support;

	/* 16 each device, each zone 2 bytes */
	bl_iw7027->reg_buf_size = 16 * 2 * dev_drv->chip_cnt;
	bl_iw7027->reg_buf = kcalloc(bl_iw7027->reg_buf_size,
		sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_iw7027->reg_buf)
		goto ldim_dev_iw7027_probe_err0;

	/* spi transfer buffer: header + reg_max_cnt + chip_cnt */
	bl_iw7027->tbuf_size = 3 + IW7027_REG_MAX + dev_drv->chip_cnt;
	if (bl_iw7027->dma_support) {
		n = bl_iw7027->tbuf_size;
		bl_iw7027->tbuf_size = ldim_spi_dma_cycle_align_byte(n);
	}
	bl_iw7027->tbuf = kcalloc(bl_iw7027->tbuf_size,
		sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_iw7027->tbuf)
		goto ldim_dev_iw7027_probe_err1;

	/* spi transfer buffer: header + reg_max_cnt + chip_cnt + dev_id_max(=chip_cnt) */
	bl_iw7027->rbuf_size = 3 + IW7027_REG_MAX + dev_drv->chip_cnt * 2;
	if (bl_iw7027->rbuf_size < bl_iw7027->tbuf_size) /* for dma use */
		bl_iw7027->rbuf_size = bl_iw7027->tbuf_size;
	bl_iw7027->rbuf = kcalloc(bl_iw7027->rbuf_size,
		sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_iw7027->rbuf)
		goto ldim_dev_iw7027_probe_err2;

	iw7027_ldim_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(iw7027_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &iw7027_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
					iw7027_class_attrs[i].attr.name);
			}
		}
	}

	if (dev_drv->spi_sync == 0)
		spin_lock_init(&spi_lock);

	bl_iw7027->dev_on_flag = 1; /* default enable in uboot */

	LDIMPR("%s ok\n", __func__);
	return 0;

ldim_dev_iw7027_probe_err2:
	kfree(bl_iw7027->tbuf);
	bl_iw7027->tbuf_size = 0;
ldim_dev_iw7027_probe_err1:
	kfree(bl_iw7027->reg_buf);
	bl_iw7027->reg_buf_size = 0;
ldim_dev_iw7027_probe_err0:
	kfree(bl_iw7027);
	bl_iw7027 = NULL;
	return -1;
}

int ldim_dev_iw7027_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7027)
		return 0;

	kfree(bl_iw7027->rbuf);
	bl_iw7027->rbuf_size = 0;
	kfree(bl_iw7027->tbuf);
	bl_iw7027->tbuf_size = 0;
	kfree(bl_iw7027->reg_buf);
	bl_iw7027->reg_buf_size = 0;
	kfree(bl_iw7027);
	bl_iw7027 = NULL;

	return 0;
}
