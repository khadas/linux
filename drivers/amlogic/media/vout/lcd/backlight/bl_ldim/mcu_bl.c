// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#define BLMCU_CLASS_NAME "blmcu"

#define VSYNC_INFO_FREQUENT        300

static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

struct blmcu_s {
	unsigned int dev_on_flag;
	unsigned char dma_support;
	unsigned short vsync_cnt;
	unsigned int rbuf_size;
	unsigned int tbuf_size;
	unsigned int header;	/*4byte default 0x55AA0014 */
	unsigned int adim;	/*1byte 0xff */
	unsigned int pdim;	/*1byte 0x4d 30%duty */
	unsigned int type;	/*1byte 0:3bytes, 1:6byte */
	unsigned int apl;

	/* local dimming driver smr api usage */
	unsigned char *rbuf;

	/* spi api internal used, don't use outside!!! */
	unsigned char *tbuf;
};

struct blmcu_s *bl_mcu;

static int blmcu_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	unsigned char chip_id;// reg, temp[2];
	//int i, retry_cnt = 0;

	LDIMPR("%s\n", __func__);

	chip_id = 0x01;

	/* step 1: system power_on */
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);

	/* step 2: delay for internal logic stable */
	lcd_delay_ms(10);

	/* step 3: Generate external VSYNC to VSYNC/PWM pin */
	ldim_set_duty_pwm(&dev_drv->ldim_pwm_config);
	ldim_set_duty_pwm(&dev_drv->analog_pwm_config);
	dev_drv->pinmux_ctrl(dev_drv, 1);

	/* step 4: delay for system clock and light bar PSU stable */
	//lcd_delay_ms(520);

	return 0;
}

static int blmcu_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static void ldim_vs_debug_info(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j;
	unsigned char datawidth;

	datawidth = (bl_mcu->header >> 3) & 0x03;

	if (bl_mcu->vsync_cnt) //300 vsync print once
		return;
	if (ldim_debug_print != 3)
		return;

	LDIMPR("%s:\n", __func__);
	if (datawidth == 3) //16bits
		j = 2 * dev_drv->zone_num;
	else //12bits
		j = (3 * dev_drv->zone_num + 1) / 2;

	if (bl_mcu->type == 1)
		j += 10;
	else
		j += 7;

	for (i = 0; i < j; i++)
		LDIMPR("tbuf[%d]: 0x%x\n", i, bl_mcu->tbuf[i]);

	pr_info("\n");
}

static inline void ldim_data_mapping(unsigned int *duty_buf,
				     unsigned int max, unsigned int min,
				     unsigned int zone_num)
{
	unsigned int i, j, val, apl, k;
	unsigned char datawidth;

	datawidth = (bl_mcu->header >> 3) & 0x03;

	j = 0;
	apl = 0;
	for (i = 0; i < zone_num; i++) {
		val = min + ((duty_buf[i] * (max - min)) / LD_DATA_MAX);
		apl += val;
		if (datawidth == 0x03) {
			//16bits
			bl_mcu->rbuf[j + 1] = (val >> 8) & 0xff;
			bl_mcu->rbuf[j] = val & 0xff;
			j += 2;
		} else {
			//12bits
			if (i % 2 == 0) {
				bl_mcu->rbuf[j] = (val >> 4) & 0xff;
				bl_mcu->rbuf[j + 1] = ((val & 0xf) << 4) & 0xff;
			} else {
				bl_mcu->rbuf[j + 1] |= (val >> 8) & 0xf;
				bl_mcu->rbuf[j + 2] = val & 0xff;
				j += 3;
			}
		}
	}

	bl_mcu->apl = apl / zone_num;

	if (datawidth == 0x03)
		k = zone_num * 2;
	else
		k = (3 * zone_num + 1) / 2;

	for (i = 0; i < k; i++)
		bl_mcu->tbuf[i + 4] = bl_mcu->rbuf[i];

	bl_mcu->tbuf[0] = (bl_mcu->header >> 24) & 0xff;	//0x55;
	bl_mcu->tbuf[1] = (bl_mcu->header >> 16) & 0xff;	//0xaa;
	bl_mcu->tbuf[2] = (bl_mcu->header >> 8) & 0xff; //0x00;
	bl_mcu->tbuf[3] = bl_mcu->header & 0xff;	//0x14;
	bl_mcu->tbuf[k + 4] = bl_mcu->pdim & 0xff;  //PDIM
	bl_mcu->tbuf[k + 5] = bl_mcu->adim & 0xff;  //ADIM

	if (bl_mcu->type == 1) {
		bl_mcu->tbuf[k + 6] = (bl_mcu->apl >> 8) & 0xff;  //apl
		bl_mcu->tbuf[k + 7] = bl_mcu->apl & 0xff;  //apl
		bl_mcu->tbuf[k + 8] = 0xff;  //reseve
		bl_mcu->tbuf[k + 9] = 0xff;  //reserve

		for (i = k + 10; i < bl_mcu->tbuf_size; i++)
			bl_mcu->tbuf[i] = 0;
	} else {
		bl_mcu->tbuf[k + 6] = (bl_mcu->apl >> 4) & 0xff;  //apl

		for (i = k + 7; i < bl_mcu->tbuf_size; i++)
			bl_mcu->tbuf[i] = 0;
	}
}

static int blmcu_smr(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;

	if (!bl_mcu)
		return -1;

	if (bl_mcu->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_mcu->vsync_cnt = 0;

	if (bl_mcu->dev_on_flag == 0) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, bl_mcu->dev_on_flag);
		return 0;
	}
	if (len != dev_drv->zone_num) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (!bl_mcu->rbuf) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMERR("%s: rbuf is null\n", __func__);
		return -1;
	}
	if (!bl_mcu->tbuf) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMERR("%s: tbuf is null\n", __func__);
		return -1;
	}

	ldim_data_mapping(buf, dev_drv->dim_max, dev_drv->dim_min,
				  dev_drv->zone_num);
	ldim_vs_debug_info(ldim_drv);

	if (dev_drv->spi_sync)
		ret = ldim_spi_write(dev_drv->spi_dev, bl_mcu->tbuf, bl_mcu->tbuf_size);
	else
		ret = ldim_spi_write_async(dev_drv->spi_dev, bl_mcu->tbuf, bl_mcu->rbuf,
		bl_mcu->tbuf_size, bl_mcu->dma_support, bl_mcu->tbuf_size);

	return ret;
}

static int blmcu_smr_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int blmcu_fault_handler(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;
	return ret;
}

static int blmcu_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	LDIMPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static int blmcu_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_mcu)
		return -1;
	if (bl_mcu->dev_on_flag) {
		LDIMPR("%s: blmcu is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	blmcu_hw_init_on(ldim_drv->dev_drv);
	bl_mcu->dev_on_flag = 1;
	bl_mcu->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int blmcu_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	LDIMPR("enter %s\n", __func__);

	if (!bl_mcu)
		return -1;

	LDIMPR("enter %s111\n", __func__);

	mutex_lock(&dev_mutex);
	bl_mcu->dev_on_flag = 0;
	blmcu_hw_init_off(ldim_drv->dev_drv);
	ldim_spi_async_busy_clear();
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t blmcu_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_mcu)
		return sprintf(buf, "bl_mcu is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (!strcmp(attr->attr.name, "blmcu_status")) {
		len = sprintf(buf, "blmcu status:\n"
				"dev_index      = %d\n"
				"dma_support      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n"
				"tbuf_size      = %d\n"
				"rbuf_size      = %d\n"
				"adim      = %d\n"
				"pdim      = %d\n"
				"type      = %d\n"
				"header      = 0x%x\n"
				"apl	      = 0x%x\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"cs_hold_delay  = %d\n"
				"cs_clk_delay   = %d\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n",
				dev_drv->index,
				bl_mcu->dev_on_flag,
				bl_mcu->dma_support,
				bl_mcu->vsync_cnt,
				bl_mcu->tbuf_size,
				bl_mcu->rbuf_size,
				bl_mcu->adim,
				bl_mcu->pdim,
				bl_mcu->type,
				bl_mcu->header,
				bl_mcu->apl,
				dev_drv->en_gpio_on,
				dev_drv->en_gpio_off,
				dev_drv->cs_hold_delay,
				dev_drv->cs_clk_delay,
				dev_drv->dim_max,
				dev_drv->dim_min);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t blmcu_store(struct class *class, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	unsigned int val;
	int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(128, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto blmcu_store_end;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	dev_drv = ldim_drv->dev_drv;
	if (!strcmp(parm[0], "init")) {
		mutex_lock(&dev_mutex);
		blmcu_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "adim")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &bl_mcu->adim) < 0)
				goto blmcu_store_err;
		}
		LDIMPR("adim: 0x%x\n", bl_mcu->adim);
	} else if (!strcmp(parm[0], "pdim")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &bl_mcu->pdim) < 0)
				goto blmcu_store_err;
		}
		LDIMPR("pdim: 0x%x\n", bl_mcu->pdim);
	} else if (!strcmp(parm[0], "type")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &bl_mcu->type) < 0)
				goto blmcu_store_err;
		}
		LDIMPR("type: 0x%x\n", bl_mcu->type);
	} else if (!strcmp(parm[0], "header")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &bl_mcu->header) < 0)
				goto blmcu_store_err;
		}
		LDIMPR("command: 0x%x\n", bl_mcu->header);
	} else if (!strcmp(parm[0], "dma")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto blmcu_store_err;
			bl_mcu->dma_support = (unsigned char)val;
		}
		LDIMPR("dma_support: %d\n", bl_mcu->dma_support);
	} else {
		LDIMERR("argument error!\n");
	}

blmcu_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

blmcu_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute blmcu_class_attrs[] = {
	__ATTR(init, 0644, NULL, blmcu_store),
	__ATTR(blmcu_status, 0644, blmcu_show, NULL),
};

static int blmcu_ldim_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = blmcu_power_on;
	dev_drv->power_off = blmcu_power_off;
	dev_drv->dev_smr = blmcu_smr;
	dev_drv->dev_smr_dummy = blmcu_smr_dummy;
	dev_drv->dev_err_handler = blmcu_fault_handler;
	dev_drv->config_update = blmcu_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_blmcu_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int i, n;
	unsigned char datawidth;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}
	if (!dev_drv->spi_dev) {
		LDIMERR("%s: spi_dev is null\n", __func__);
		return -1;
	}

	bl_mcu = kzalloc(sizeof(*bl_mcu), GFP_KERNEL);
	if (!bl_mcu) {
		LDIMERR("%s: bl_mcu is error\n", __func__);
		return -1;
	}

	bl_mcu->dev_on_flag = 0;
	bl_mcu->vsync_cnt = 0;
	bl_mcu->adim = (dev_drv->mcu_dim >> 8) & 0xff;
	bl_mcu->pdim = dev_drv->mcu_dim & 0xff;
	bl_mcu->type = (dev_drv->mcu_dim >> 16) & 0xff;
	bl_mcu->header = dev_drv->mcu_header;
	bl_mcu->dma_support = dev_drv->dma_support;

	/* each zone 2 bytes */
	bl_mcu->rbuf_size = 2 * dev_drv->zone_num;
	bl_mcu->rbuf = kcalloc(bl_mcu->rbuf_size, sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_mcu->rbuf) {
		LDIMERR("%s: bl_mcu->rbuf is error\n", __func__);
		goto ldim_dev_blmcu_probe_err0;
	}

	/* header + data + suffix */
	/* according custom backlight mcu spi spec to set tbuf_size */
	datawidth = (bl_mcu->header >> 3) & 0x03;
	if (datawidth == 3)
		bl_mcu->tbuf_size = 2 * dev_drv->zone_num;
	else
		bl_mcu->tbuf_size = (dev_drv->zone_num * 3 + 1) / 2;
	if (bl_mcu->type == 1)
		bl_mcu->tbuf_size += 10;
	else
		bl_mcu->tbuf_size += 7;
	if (bl_mcu->dma_support) {
		n = bl_mcu->tbuf_size;
		bl_mcu->tbuf_size = ldim_spi_dma_cycle_align_byte(n);
		LDIMPR("%s: bl_mcu->tbuf_size is %d --> %d\n", __func__, n, bl_mcu->tbuf_size);
	}
	bl_mcu->tbuf = kcalloc(bl_mcu->tbuf_size, sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_mcu->tbuf) {
		LDIMERR("%s: bl_mcu->tbuf is error\n", __func__);
		goto ldim_dev_blmcu_probe_err1;
	}

	blmcu_ldim_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(blmcu_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &blmcu_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
					blmcu_class_attrs[i].attr.name);
			}
		}
	}

	bl_mcu->dev_on_flag = 1; /* default enable in uboot */

	LDIMPR("%s ok\n", __func__);
	return 0;

ldim_dev_blmcu_probe_err1:
	kfree(bl_mcu->rbuf);
	bl_mcu->rbuf_size = 0;
ldim_dev_blmcu_probe_err0:
	kfree(bl_mcu);
	bl_mcu = NULL;
	return -1;
}

int ldim_dev_blmcu_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_mcu)
		return 0;

	kfree(bl_mcu->rbuf);
	bl_mcu->rbuf_size = 0;
	kfree(bl_mcu->tbuf);
	bl_mcu->tbuf_size = 0;
	kfree(bl_mcu);
	bl_mcu = NULL;

	return 0;
}
