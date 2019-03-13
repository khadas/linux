/*
 * Khadas VIM2 MCU control driver
 *
 * Written by: Nick <nick@khadas.com>
 *
 * Copyright (C) 2019 Wesion Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>



/* Device registers */
#define MCU_WOL_REG		0x21
#define MCU_RST_REG             0x2c
#define MCU_FAN_CTRL	0x88

#define KHADAS_FAN_TRIG_TEMP_LEVEL0		50	// 50 degree if not set
#define KHADAS_FAN_TRIG_TEMP_LEVEL1		60	// 60 degree if not set
#define KHADAS_FAN_TRIG_TEMP_LEVEL2		70	// 70 degree if not set
#define KHADAS_FAN_TRIG_MAXTEMP		80
#define KHADAS_FAN_LOOP_SECS 		30 * HZ	// 30 seconds
#define KHADAS_FAN_TEST_LOOP_SECS   5 * HZ  // 5 seconds
#define KHADAS_FAN_LOOP_NODELAY_SECS   	0
#define KHADAS_FAN_SPEED_OFF			0
#define KHADAS_FAN_SPEED_LOW            1
#define KHADAS_FAN_SPEED_MID            2
#define KHADAS_FAN_SPEED_HIGH           3

enum khadas_fan_mode {
	KHADAS_FAN_STATE_MANUAL = 0,
	KHADAS_FAN_STATE_AUTO,
};

enum khadas_fan_level {
	KHADAS_FAN_LEVEL_0 = 0,
	KHADAS_FAN_LEVEL_1,
	KHADAS_FAN_LEVEL_2,
	KHADAS_FAN_LEVEL_3,
};

enum khadas_fan_enable {
	KHADAS_FAN_DISABLE = 0,
	KHADAS_FAN_ENABLE,
};

enum khadas_fan_hwver {
	KHADAS_FAN_HWVER_NONE = 0,
	KHADAS_FAN_HWVER_V12,
	KHADAS_FAN_HWVER_V13,
	KHADAS_FAN_HWVER_V14
};

struct khadas_fan_data {
	struct platform_device *pdev;
	struct class *fan_class;
	struct delayed_work work;
	struct delayed_work fan_test_work;
	enum    khadas_fan_enable enable;
	enum 	khadas_fan_mode mode;
	enum 	khadas_fan_level level;
	int	trig_temp_level0;
	int	trig_temp_level1;
	int	trig_temp_level2;
	enum khadas_fan_hwver hwver;
};

struct mcu_data {
	struct i2c_client *client;
	struct class *wol_class;
	int wol_enable;
	struct khadas_fan_data fan_data;
};

struct mcu_data *g_mcu_data;

extern void set_test(int flag);
extern void realtek_enable_wol(int enable, bool is_shutdown);
void mcu_enable_wol(int enable, bool is_shutdown)
{
	realtek_enable_wol(enable, is_shutdown);
}

static int i2c_master_reg8_send(const struct i2c_client *client,
		const char reg, const char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;
	char *tx_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;
	tx_buf[0] = reg;
	memcpy(tx_buf+1, buf, count);

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = count + 1;
	msg.buf = (char *)tx_buf;

	ret = i2c_transfer(adap, &msg, 1);
	kfree(tx_buf);
	return (ret == 1) ? count : ret;
}

static int i2c_master_reg8_recv(const struct i2c_client *client,
		const char reg, char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[2];
	int ret;
	char reg_buf = reg;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len = count;
	msgs[1].buf = (char *)buf;

	ret = i2c_transfer(adap, msgs, 2);

	return (ret == 2) ? count : ret;
}

static int mcu_i2c_read_regs(struct i2c_client *client,
		u8 reg, u8 buf[], unsigned len)
{
	int ret;
	ret = i2c_master_reg8_recv(client, reg, buf, len);
	return ret;
}

static int mcu_i2c_write_regs(struct i2c_client *client,
		u8 reg, u8 const buf[], __u16 len)
{
	int ret;
	ret = i2c_master_reg8_send(client, reg, buf, (int)len);
	return ret;
}

static int is_mcu_fan_control_available(void)
{
	// MCU FAN control only for Khadas VIM2 V13 and later.
	if (g_mcu_data->fan_data.hwver > KHADAS_FAN_HWVER_V12)
		return 1;
	else
		return 0;
}

static void khadas_fan_level_set(struct khadas_fan_data *fan_data, int level)
{
	if (is_mcu_fan_control_available()) {
		int ret;
		u8 data[2] = {0};

		if(0 == level) {
			data[0] = KHADAS_FAN_SPEED_OFF;
		}else if(1 == level){
			data[0] = KHADAS_FAN_SPEED_LOW;
		}else if(2 == level){
			data[0] = KHADAS_FAN_SPEED_MID;
		}else if(3 == level){
			data[0] = KHADAS_FAN_SPEED_HIGH;
		}

		ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_FAN_CTRL, data, 1);
		if (ret < 0) {
			printk("write fan control err\n");
			return;
		}
	}
}

static void fan_test_work_func(struct work_struct *_work)
{
	if (is_mcu_fan_control_available()) {
		struct khadas_fan_data *fan_data = &g_mcu_data->fan_data;
		khadas_fan_level_set(fan_data, 0);
	}
}

extern int get_cpu_temp(void);
static void fan_work_func(struct work_struct *_work)
{
	if (is_mcu_fan_control_available()) {
		int temp = -EINVAL;
		struct khadas_fan_data *fan_data = &g_mcu_data->fan_data;

		temp = get_cpu_temp();

		if(temp != -EINVAL){
			if(temp < fan_data->trig_temp_level0 ) {
				khadas_fan_level_set(fan_data, 0);
			}else if(temp < fan_data->trig_temp_level1 ) {
				khadas_fan_level_set(fan_data, 1);
			}else if(temp < fan_data->trig_temp_level2 ) {
				khadas_fan_level_set(fan_data, 2);
			}else{
				khadas_fan_level_set(fan_data, 3);
			}
		}

		schedule_delayed_work(&fan_data->work, KHADAS_FAN_LOOP_SECS);
	}
}

static void khadas_fan_set(struct khadas_fan_data  *fan_data)
{
	if (is_mcu_fan_control_available()) {

		cancel_delayed_work(&fan_data->work);

		if (fan_data->enable == KHADAS_FAN_DISABLE) {
			khadas_fan_level_set(fan_data, 0);
			return;
		}
		switch (fan_data->mode) {
		case KHADAS_FAN_STATE_MANUAL:
			switch(fan_data->level) {
				case KHADAS_FAN_LEVEL_0:
					khadas_fan_level_set(fan_data, 0);
					break;
				case KHADAS_FAN_LEVEL_1:
					khadas_fan_level_set(fan_data, 1);
					break;
				case KHADAS_FAN_LEVEL_2:
					khadas_fan_level_set(fan_data, 2);
					break;
				case KHADAS_FAN_LEVEL_3:
					khadas_fan_level_set(fan_data, 3);
					break;
				default:
					break;
			}
			break;

		case KHADAS_FAN_STATE_AUTO:
			// FIXME: achieve with a better way
			schedule_delayed_work(&fan_data->work, KHADAS_FAN_LOOP_NODELAY_SECS);
			break;

		default:
			break;
		}
	}
}

static ssize_t show_fan_enable(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "Fan enable: %d\n", g_mcu_data->fan_data.enable);
}

static ssize_t store_fan_enable(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int enable;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;

	// 0: manual, 1: auto
	if( enable >= 0 && enable < 2 ){
		g_mcu_data->fan_data.enable = enable;
		khadas_fan_set(&g_mcu_data->fan_data);
	}

	return count;
}

static ssize_t show_fan_mode(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "Fan mode: %d\n", g_mcu_data->fan_data.mode);
}

static ssize_t store_fan_mode(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int mode;

	if (kstrtoint(buf, 0, &mode))
		return -EINVAL;

	// 0: manual, 1: auto
	if( mode >= 0 && mode < 2 ){
		g_mcu_data->fan_data.mode = mode;
		khadas_fan_set(&g_mcu_data->fan_data);
	}

	return count;
}

static ssize_t show_fan_level(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "Fan level: %d\n", g_mcu_data->fan_data.level);
}

static ssize_t store_fan_level(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int level;

	if (kstrtoint(buf, 0, &level))
		return -EINVAL;

	if( level >= 0 && level < 4){
		g_mcu_data->fan_data.level = level;
		khadas_fan_set(&g_mcu_data->fan_data);
	}

	return count;
}

static ssize_t show_fan_temp(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	int temp = -EINVAL;
    temp = get_cpu_temp();

	return sprintf(buf, "cpu_temp:%d\nFan trigger temperature: level0:%d level1:%d level2:%d\n", temp, g_mcu_data->fan_data.trig_temp_level0, g_mcu_data->fan_data.trig_temp_level1, g_mcu_data->fan_data.trig_temp_level2);
}

static ssize_t store_wol_enable(struct class *cls, struct class_attribute *attr,
		        const char *buf, size_t count)
{
	u8 reg[2];
	int ret;
	int enable;
	int state;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;

	ret = mcu_i2c_read_regs(g_mcu_data->client, MCU_WOL_REG, reg, 1);
	if (ret < 0) {
		printk("write wol state err\n");
		return ret;
	}
	state = (int)reg[0];
	reg[0] = enable | (state & 0x02);
	ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_WOL_REG, reg, 1);
	if (ret < 0) {
		printk("write wol state err\n");
		return ret;
	}

	g_mcu_data->wol_enable = reg[0];
	mcu_enable_wol(g_mcu_data->wol_enable, false);

	printk("write wol state: %d\n", g_mcu_data->wol_enable);
	return count;
}

static ssize_t show_wol_enable(struct class *cls,
		        struct class_attribute *attr, char *buf)
{
	int enable;
	enable = g_mcu_data->wol_enable & 0x01;
	return sprintf(buf, "%d\n", enable);
}

static ssize_t store_rst_mcu(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	u8 reg[2];
	int ret;
	int rst;

	if (kstrtoint(buf, 0, &rst))
		return -EINVAL;

	reg[0] = rst;
	ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_RST_REG, reg, 1);
	if (ret < 0)
		printk("rst mcu err\n");
	return count;
}

static ssize_t store_test(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int flag;

	if (kstrtoint(buf, 0, &flag))
		return -EINVAL;

	set_test(flag);
	return count;
}

static struct class_attribute wol_class_attrs[] = {
	__ATTR(enable, 0644, show_wol_enable, store_wol_enable),
	__ATTR(test, 0644, NULL, store_test),
};

static struct class_attribute fan_class_attrs[] = {
	__ATTR(enable, 0644, show_fan_enable, store_fan_enable),
	__ATTR(mode, 0644, show_fan_mode, store_fan_mode),
	__ATTR(level, 0644, show_fan_level, store_fan_level),
	__ATTR(temp, 0644, show_fan_temp, NULL),
	__ATTR(rst_mcu, 0644, NULL, store_rst_mcu),
};

static void create_mcu_attrs(void)
{
	int i;
	printk("%s\n",__func__);
	g_mcu_data->wol_class = class_create(THIS_MODULE, "wol");
	if (IS_ERR(g_mcu_data->wol_class)) {
		pr_err("create wol_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(wol_class_attrs); i++) {
		if (class_create_file(g_mcu_data->wol_class, &wol_class_attrs[i]))
			pr_err("create wol attribute %s fail\n", wol_class_attrs[i].attr.name);
	}

	if (is_mcu_fan_control_available()) {
		g_mcu_data->fan_data.fan_class = class_create(THIS_MODULE, "fan");
		if (IS_ERR(g_mcu_data->fan_data.fan_class)) {
			pr_err("create fan_class debug class fail\n");
			return;
		}

		for (i=0; i<ARRAY_SIZE(fan_class_attrs); i++) {
			if (class_create_file(g_mcu_data->fan_data.fan_class, &fan_class_attrs[i]))
				pr_err("create fan attribute %s fail\n", fan_class_attrs[i].attr.name);
		}
	}
}

static int mcu_parse_dt(struct device *dev)
{
	int ret;
	const char *hwver = NULL;

	if (NULL == dev) return -EINVAL;

	// Get hardwere version
	ret = of_property_read_string(dev->of_node, "hwver", &hwver);
	if (ret < 0) {
		g_mcu_data->fan_data.hwver = KHADAS_FAN_HWVER_V12;
	} else {
		if (0 == strcmp(hwver, "VIM2.V12")) {
			g_mcu_data->fan_data.hwver = KHADAS_FAN_HWVER_V12;
		} else if (0 == strcmp(hwver, "VIM2.V13")) {
			g_mcu_data->fan_data.hwver = KHADAS_FAN_HWVER_V13;
		} else if (0 == strcmp(hwver, "VIM2.V14")) {
			g_mcu_data->fan_data.hwver = KHADAS_FAN_HWVER_V14;
		}
		else {
			g_mcu_data->fan_data.hwver = KHADAS_FAN_HWVER_NONE;
		}
	}

	ret = of_property_read_u32(dev->of_node, "fan,trig_temp_level0", &g_mcu_data->fan_data.trig_temp_level0);
	if (ret < 0)
		g_mcu_data->fan_data.trig_temp_level0 = KHADAS_FAN_TRIG_TEMP_LEVEL0;
	ret = of_property_read_u32(dev->of_node, "fan,trig_temp_level1", &g_mcu_data->fan_data.trig_temp_level1);
	if (ret < 0)
		g_mcu_data->fan_data.trig_temp_level1 = KHADAS_FAN_TRIG_TEMP_LEVEL1;
	ret = of_property_read_u32(dev->of_node, "fan,trig_temp_level2", &g_mcu_data->fan_data.trig_temp_level2);
	if (ret < 0)
		g_mcu_data->fan_data.trig_temp_level2 = KHADAS_FAN_TRIG_TEMP_LEVEL2;

	return ret;
}

int mcu_get_wol_status(void)
{
	if (g_mcu_data)
		return g_mcu_data->wol_enable;

	return 0;
}

static int mcu_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u8 reg[2];
	int ret;

	printk("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	g_mcu_data = kzalloc(sizeof(struct mcu_data), GFP_KERNEL);

	if (g_mcu_data == NULL)
		return -ENOMEM;

	mcu_parse_dt(&client->dev);

	g_mcu_data->client = client;
	ret = mcu_i2c_read_regs(client, MCU_WOL_REG, reg, 1);
	if (ret < 0)
		goto exit;
	g_mcu_data->wol_enable = (int)reg[0];

	if (is_mcu_fan_control_available()) {
		g_mcu_data->fan_data.mode = KHADAS_FAN_STATE_AUTO;
		g_mcu_data->fan_data.level = KHADAS_FAN_LEVEL_0;
		g_mcu_data->fan_data.enable = KHADAS_FAN_DISABLE;

		INIT_DELAYED_WORK(&g_mcu_data->fan_data.work, fan_work_func);
		khadas_fan_level_set(&g_mcu_data->fan_data, 1);
		INIT_DELAYED_WORK(&g_mcu_data->fan_data.fan_test_work, fan_test_work_func);
		schedule_delayed_work(&g_mcu_data->fan_data.fan_test_work, KHADAS_FAN_TEST_LOOP_SECS);
	}
	create_mcu_attrs();
	printk("%s,wol enable=%d\n",__func__ ,g_mcu_data->wol_enable);

	if (g_mcu_data->wol_enable == 3)
		mcu_enable_wol(g_mcu_data->wol_enable, false);

	reg[0] = 0x01;
	ret = mcu_i2c_write_regs(client, 0x87, reg, 1);
	if (ret < 0) {
		printk("write mcu err\n");
		goto  exit;
	}
	return 0;
exit:
	return ret;

}


static int mcu_remove(struct i2c_client *client)
{
	return 0;
}

static void khadas_fan_shutdown(struct i2c_client *client)
{
	g_mcu_data->fan_data.enable = KHADAS_FAN_DISABLE;
	khadas_fan_set(&g_mcu_data->fan_data);
}

#ifdef CONFIG_PM
static int khadas_fan_suspend(struct i2c_client *client, pm_message_t state)
{
	cancel_delayed_work(&g_mcu_data->fan_data.work);
	khadas_fan_level_set(&g_mcu_data->fan_data, 0);

	return 0;
}

static int khadas_fan_resume(struct i2c_client *client)
{
	khadas_fan_set(&g_mcu_data->fan_data);

	return 0;
}
#endif

static const struct i2c_device_id mcu_id[] = {
	{ "khadas-mcu", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcu_id);


static struct of_device_id mcu_dt_ids[] = {
	{ .compatible = "khadas-mcu" },
	{},
};
MODULE_DEVICE_TABLE(i2c, mcu_dt_ids);

struct i2c_driver mcu_driver = {
	.driver  = {
		.name   = "khadas-mcu",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mcu_dt_ids),
	},
	.probe		= mcu_probe,
	.remove     = mcu_remove,
#ifdef CONFIG_PM
    .suspend = khadas_fan_suspend,
    .resume = khadas_fan_resume,
#endif
	.shutdown = khadas_fan_shutdown,
	.id_table	= mcu_id,
};
module_i2c_driver(mcu_driver);

MODULE_AUTHOR("Nick <nick@khadas.com>");
MODULE_DESCRIPTION("Khadas VIM2 MCU control driver");
MODULE_LICENSE("GPL");
