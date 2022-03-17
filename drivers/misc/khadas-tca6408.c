/*
 * Driver for keys on TCA6408 I2C IO expander
 *
 * Copyright (C) 2019 Texas Instruments
 *
 * Author : waylon <waylon@khadas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kobject.h>
#include <linux/mfd/core.h>
#include <linux/slab.h>
#include <linux/khadas-tca6408.h>

#define TCA6408_INPUT          0
#define TCA6408_OUTPUT         1
#define TCA6408_INVERT         2
#define TCA6408_DIRECTION      3

//#define TCA6408_DEBUG
#ifdef TCA6408_DEBUG
    #define debug_info(msg...) printk(msg);
#else
    #define debug_info(msg...)
#endif

static const struct i2c_device_id tca6408_id[] = {
	{ "khadas-tca6408", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tca6408_id);


struct tca6408_gpio_chip{
	struct i2c_client *client;
	uint8_t reg_output;
	uint8_t reg_direction;
};
static struct tca6408_gpio_chip *chip;

static int tca6408_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int error;

	error = i2c_smbus_write_byte_data(client, reg, val);
	if (error < 0) {
		dev_err(&client->dev,
			"%s failed, reg: %d, val: %d, error: %d\n",
			__func__, reg, val, error);
		return error;
	}

	return 0;
}

static int tca6408_read_reg(struct i2c_client *client, u8 reg, u8 *val)
{
	int retval;

	retval = i2c_smbus_read_byte_data(client, reg);
	if (retval < 0) {
		dev_err(&client->dev, "%s failed, reg: %d, error: %d\n",
			__func__, reg, retval);
		return retval;
	}

	*val = (u8)retval;
	return 0;
}

int tca6408_output_set_value(u8 value, u8 mask)
{
	int error;

	debug_info("%s, value = 0x%x, mask = 0x%x\n", __func__ ,value,mask);
	error = tca6408_read_reg(chip->client, TCA6408_OUTPUT, &chip->reg_output);
	if (error)
		return error;

	chip->reg_output &= ~(mask);
	chip->reg_output |= (value & mask);

	error = tca6408_write_reg(chip->client, TCA6408_OUTPUT, chip->reg_output);
	if (error)
		return error;

	return 0;
}
EXPORT_SYMBOL(tca6408_output_set_value);

int tca6408_output_get_value(u8 *value)
{
	int error;

	debug_info("%s\n", __func__);

	error = tca6408_read_reg(chip->client, TCA6408_OUTPUT, value);
	if (error)
		return error;

	debug_info("value = 0x%x\n", *value);

	return 0;
}
EXPORT_SYMBOL(tca6408_output_get_value);

static int tca6408_setup_registers(struct tca6408_gpio_chip *chip)
{
	int error;

	debug_info("%s\n", __func__);

	/* ensure that keypad pins are set to input */
	error = tca6408_write_reg(chip->client, TCA6408_DIRECTION, 0x00);
	if (error)
		return error;
	error = tca6408_write_reg(chip->client, TCA6408_OUTPUT, 0x27);
	if (error)
		return error;

	error = tca6408_read_reg(chip->client, TCA6408_OUTPUT, &chip->reg_output);
	if (error)
		return error;

	error = tca6408_read_reg(chip->client, TCA6408_DIRECTION, &chip->reg_direction);
	if (error)
		return error;

	debug_info("%s:direction=0x%x, output=0x%x\n", __func__, chip->reg_direction, chip->reg_output);

	return 0;
}

static int tca6408_gpio_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int error;

	debug_info("%s\n", __func__);

	/* Check functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "%s adapter not supported\n",
			dev_driver_string(&client->adapter->dev));
		return -ENODEV;
	}

	chip = kzalloc(sizeof(struct tca6408_gpio_chip), GFP_KERNEL);
	if (!chip)
            return -ENOMEM;

	chip->client = client;

	error = tca6408_setup_registers(chip);
	if (error)
		return error;

	i2c_set_clientdata(client, chip);

	debug_info("%s prob success.\n", __func__);

	return 0;
}

static int tca6408_gpio_remove(struct i2c_client *client)
{
	struct tca6408_gpio_chip *chip = i2c_get_clientdata(client);
	kfree(chip);

	return 0;
}

static struct i2c_driver tca6408_gpio_driver = {
	.driver = {
		.name	= "khadas-tca6408",
	},
	.probe		= tca6408_gpio_probe,
	.remove		= tca6408_gpio_remove,
	.id_table	= tca6408_id,
};

static int __init tca6408_gpio_init(void)
{
	return i2c_add_driver(&tca6408_gpio_driver);
}

rootfs_initcall(tca6408_gpio_init);

static void __exit tca6416_gpio_exit(void)
{
	i2c_del_driver(&tca6408_gpio_driver);
}
module_exit(tca6416_gpio_exit);

MODULE_AUTHOR("waylon@khadas.com>");
MODULE_DESCRIPTION("Tca6408 IO expander");
MODULE_LICENSE("GPL");
