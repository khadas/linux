/*
 * Khadas Edge2 MCU control driver
 *
 * Written by: Nick <nick@khadas.com>
 *
 * Copyright (C) 2022 Wesion Technologies Ltd.
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
#define MCU_PWR_OFF_CMD_REG       0x80
#define MCU_SHUTDOWN_NORMAL_REG   0x2c

struct mcu_data {
	struct i2c_client *client;
	struct class *mcu_class;
};

struct mcu_data *g_mcu_data;

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

/*static int i2c_master_reg8_recv(const struct i2c_client *client,
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
}*/

/*static int mcu_i2c_read_regs(struct i2c_client *client,
		u8 reg, u8 buf[], unsigned len)
{
	int ret;
	ret = i2c_master_reg8_recv(client, reg, buf, len);
	return ret;
}*/

static int mcu_i2c_write_regs(struct i2c_client *client,
		u8 reg, u8 const buf[], __u16 len)
{
	int ret;

	ret = i2c_master_reg8_send(client, reg, buf, (int)len);

	return ret;
}

static ssize_t store_mcu_poweroff(struct class *cls,struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	int val;
	char reg;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	if (val != 1)
		return -EINVAL;

	reg = (char)val;

	ret = mcu_i2c_write_regs(g_mcu_data->client,MCU_PWR_OFF_CMD_REG,
							&reg, 1);
	if (ret < 0) {
		pr_debug("write poweroff cmd error\n");
		return ret;
	}

	return count;
}

static ssize_t store_mcu_rst(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	char reg;
	int ret;
	int rst;

	if (kstrtoint(buf, 0, &rst))
		return -EINVAL;

	reg = rst;
	ret = mcu_i2c_write_regs(g_mcu_data->client,MCU_SHUTDOWN_NORMAL_REG,
							&reg, 1);
	if (ret < 0) {
		pr_debug("write poweroff cmd error\n");
		return ret;
	}

	return count;
}

static struct class_attribute mcu_class_attrs[] = {
	__ATTR(poweroff, 0644, NULL, store_mcu_poweroff),
	__ATTR(rst, 0644, NULL, store_mcu_rst),
};

static void create_mcu_attrs(void) {
	int i;

	g_mcu_data->mcu_class = class_create(THIS_MODULE, "mcu");
	if (IS_ERR(g_mcu_data->mcu_class)) {
		pr_err("create mcu_class debug class fail\n");

		return;
	}
	for (i = 0; i < ARRAY_SIZE(mcu_class_attrs); i++) {
		if (class_create_file(g_mcu_data->mcu_class, &mcu_class_attrs[i]));
			pr_err("create mcu attribute %s fail\n",
							mcu_class_attrs[i].attr.name);
	}
}

static int mcu_parse_dt(struct device *dev)
{
	int ret = 0;

	if (NULL == dev) return -EINVAL;

	return ret;
}

static int mcu_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	g_mcu_data = kzalloc(sizeof(struct mcu_data), GFP_KERNEL);

	if (g_mcu_data == NULL)
		return -ENOMEM;

	mcu_parse_dt(&client->dev);

	g_mcu_data->client = client;

	create_mcu_attrs();

	return 0;
}

static int mcu_remove(struct i2c_client *client)
{
	kfree(g_mcu_data);
	return 0;
}

static void mcu_shutdown(struct i2c_client *client)
{
	kfree(g_mcu_data);
}

#ifdef CONFIG_PM_SLEEP
static int mcu_suspend(struct device *dev)
{
	return 0;
}

static int mcu_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops mcu_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mcu_suspend, mcu_resume)
};

#define MCU_PM_OPS (&(mcu_dev_pm_ops))

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
#ifdef CONFIG_PM_SLEEP
		.pm = MCU_PM_OPS,
#endif
	},
	.probe		= mcu_probe,
	.remove 	= mcu_remove,
	.shutdown = mcu_shutdown,
	.id_table	= mcu_id,
};
module_i2c_driver(mcu_driver);

MODULE_AUTHOR("Nick <nick@khadas.com>");
MODULE_DESCRIPTION("Khadas Edge2 MCU control driver");
MODULE_LICENSE("GPL");
