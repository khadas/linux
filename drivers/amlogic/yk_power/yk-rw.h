/*
 * Read/Write driver for YEKER
 *
 * Copyright (C) 2013 YEKER, Ltd.
 *  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#ifndef _LINUX_YK_RW_H_
#define _LINUX_YK_RW_H_

#include <linux/i2c.h>
#include "yk-mfd.h"


struct i2c_client *yk;
EXPORT_SYMBOL_GPL(yk);

static inline int __yk_read(struct i2c_client *client,
				int reg, uint8_t *val)
{
	int ret, i;

	for (i=0; i<3; i++)
	{
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret >= 0) {
			break;
		}
		msleep(20);
	}
	if (ret < 0) {
		dev_err(&client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*val = (uint8_t)ret;

	return 0;
}

static inline int __yk_reads(struct i2c_client *client, int reg,
				 int len, uint8_t *val)
{
	int ret, i;

	for (i=0; i<3; i++)
	{
		ret = i2c_smbus_read_i2c_block_data(client, reg, len, val);
		if (ret >= 0) {
			break;
		}
		msleep(20);
	}

	if (ret < 0) {
		dev_err(&client->dev, "failed reading from 0x%02x\n", reg);
		return ret;
	}
	return 0;
}

static inline int __yk_write(struct i2c_client *client,
				 int reg, uint8_t val)
{
	int ret, i;
	for (i=0; i<3; i++)
	{
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (ret >= 0) {
			break;
		}
		msleep(20);
	}
	if (ret < 0) {
		dev_err(&client->dev, "failed writing 0x%02x to 0x%02x\n",
				val, reg);
		return ret;
	}

	return 0;
}


static inline int __yk_writes(struct i2c_client *client, int reg,
				  int len, uint8_t *val)
{
	int ret, i;
	for (i=0; i<3; i++)
	{
		ret = i2c_smbus_write_i2c_block_data(client, reg, len, val);
		if (ret >= 0) {
			break;
		}
		msleep(20);
	}
	if (ret < 0) {
		dev_err(&client->dev, "failed writings to 0x%02x\n", reg);
		return ret;
	}

	return 0;
}

int yk_write(struct device *dev, int reg, uint8_t val)
{
	return __yk_write(to_i2c_client(dev), reg, val);
}
EXPORT_SYMBOL_GPL(yk_write);

int yk_writes(struct device *dev, int reg, int len, uint8_t *val)
{
	return  __yk_writes(to_i2c_client(dev), reg, len, val);
}
EXPORT_SYMBOL_GPL(yk_writes);

int yk_read(struct device *dev, int reg, uint8_t *val)
{
	return __yk_read(to_i2c_client(dev), reg, val);
}
EXPORT_SYMBOL_GPL(yk_read);

int yk_reads(struct device *dev, int reg, int len, uint8_t *val)
{
	return __yk_reads(to_i2c_client(dev), reg, len, val);
}
EXPORT_SYMBOL_GPL(yk_reads);

int yk_set_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret = 0;

	ret = __yk_read(yk, reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & bit_mask) != bit_mask) {
		reg_val |= bit_mask;
		ret = __yk_write(yk, reg, reg_val);
	}
out:
	return ret;
}
EXPORT_SYMBOL_GPL(yk_set_bits);

int yk_clr_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret = 0;

	ret = __yk_read(yk, reg, &reg_val);
	if (ret)
		goto out;

	if (reg_val & bit_mask) {
		reg_val &= ~bit_mask;
		ret = __yk_write(yk, reg, reg_val);
	}
out:
	return ret;
}
EXPORT_SYMBOL_GPL(yk_clr_bits);

int yk_update(struct device *dev, int reg, uint8_t val, uint8_t mask)
{
	uint8_t reg_val;
	int ret = 0;

	ret = __yk_read(yk, reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & mask) != val) {
		reg_val = (reg_val & ~mask) | val;
		ret = __yk_write(yk, reg, reg_val);
	}
out:
	return ret;
}
EXPORT_SYMBOL_GPL(yk_update);

struct device *yk_get_dev(void)
{
	return &yk->dev;
}
EXPORT_SYMBOL_GPL(yk_get_dev);

#endif
