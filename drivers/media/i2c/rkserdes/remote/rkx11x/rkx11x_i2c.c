// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <asm/unaligned.h>

#include "rkx11x_i2c.h"

int rkx11x_i2c_reg_read(struct i2c_client *client, u32 reg, u32 *val)
{
	struct i2c_msg xfer[2];
	u32 reg_addr, reg_value;
	int ret = 0;

	reg_addr = cpu_to_le32(reg);

	/* register address */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 4;
	xfer[0].buf = (u8 *)&reg_addr;

	/* data buffer */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 4;
	xfer[1].buf = (u8 *)&reg_value;

	ret = i2c_transfer(client->adapter, xfer, 2);
	if (ret != 2) {
		dev_err(&client->dev,
			"%s: reading register 0x%08x from 0x%02x failed\n",
			__func__, reg, client->addr);
		return -EIO;
	}

	*val = le32_to_cpu(reg_value);

	dev_dbg(&client->dev, "i2c addr(0x%02x) read: 0x%08x = 0x%08x\n",
				client->addr, reg, *val);

	return 0;
}

int rkx11x_i2c_reg_write(struct i2c_client *client, u32 reg, u32 val)
{
	struct i2c_msg xfer;
	u32 reg_addr, reg_value;
	u8 buf[8];
	int ret = 0;

	dev_dbg(&client->dev, "i2c addr(0x%02x) write: 0x%08x = 0x%08x\n",
				client->addr, reg, val);

	reg_addr = cpu_to_le32(reg);
	reg_value = cpu_to_le32(val);
	memcpy(&buf[0], &reg_addr, 4);
	memcpy(&buf[4], &reg_value, 4);

	/* register address & data */
	xfer.addr = client->addr;
	xfer.flags = 0;
	xfer.len = 8;
	xfer.buf = buf;

	ret = i2c_transfer(client->adapter, &xfer, 1);
	if (ret != 1) {
		dev_err(&client->dev,
			"%s: writing register 0x%08x from 0x%02x failed\n",
			__func__, reg, client->addr);
		return -EIO;
	}

	return 0;
}

int rkx11x_i2c_reg_update(struct i2c_client *client, u32 reg, u32 mask, u32 val)
{
	u32 value = 0;
	int ret = 0;

	ret = rkx11x_i2c_reg_read(client, reg, &value);
	if (ret)
		return ret;

	value &= ~mask;
	value |= (val & mask);
	ret = rkx11x_i2c_reg_write(client, reg, value);
	if (ret)
		return ret;

	return 0;
}

static int rkx11x_i2c_write_array(struct i2c_client *client,
				const struct rkx11x_i2c_regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; (ret == 0) && (regs[i].reg_addr != RKX11X_I2C_REG_NULL); i++) {
		if (regs[i].reg_addr == RKX11X_I2C_REG_DELAY) {
			dev_info(&client->dev, "i2c array delay (%d) us\n", regs[i].reg_val);
			usleep_range(regs[i].reg_val, regs[i].reg_val + 100);
			continue;
		}

		if (regs[i].val_mask != 0)
			ret = rkx11x_i2c_reg_update(client,
					regs[i].reg_addr, regs[i].val_mask, regs[i].reg_val);
		else
			ret = rkx11x_i2c_reg_write(client,
					regs[i].reg_addr, regs[i].reg_val);
	}

	return ret;
}

static int rkx11x_i2c_parse_init_seq(struct device *dev,
		const u32 *seq_data, int data_len, struct rkx11x_i2c_init_seq *init_seq)
{
	struct rkx11x_i2c_regval *reg_val = NULL;
	u32 *data_buf = NULL, *d32 = NULL;
	u32 i = 0;

	if ((seq_data == NULL) || (init_seq == NULL) || (data_len == 0)) {
		dev_err(dev, "%s: input parameter error\n", __func__);
		return -EINVAL;
	}

	// data_len = seq_item_size * N, Unit: byte
	if ((data_len % init_seq->seq_item_size) != 0) {
		dev_err(dev, "%s: data_len = %d or seq_item_size = %d error\n",
				__func__, data_len, init_seq->seq_item_size);
		return -EINVAL;
	}

	dev_info(dev, "%s: data size = %d, item size = %d\n",
			__func__, data_len, init_seq->seq_item_size);

	data_buf = devm_kmemdup(dev, seq_data, data_len, GFP_KERNEL);
	if (!data_buf) {
		dev_err(dev, "%s: data buf error\n", __func__);
		return -ENOMEM;
	}

	d32 = data_buf;

	init_seq->reg_seq_size = data_len / init_seq->seq_item_size;
	init_seq->reg_seq_size += 1; // size add 1 for End register Flag

	init_seq->reg_init_seq = devm_kcalloc(dev, init_seq->reg_seq_size,
					sizeof(struct rkx11x_i2c_regval), GFP_KERNEL);
	if (!init_seq->reg_init_seq) {
		dev_err(dev, "%s init seq buffer error\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < init_seq->reg_seq_size - 1; i++) {
		reg_val = &init_seq->reg_init_seq[i];

		reg_val->reg_addr = get_unaligned_be32(&d32[0]);
		reg_val->reg_val = get_unaligned_be32(&d32[1]);
		reg_val->val_mask = get_unaligned_be32(&d32[2]);

		d32 += 3;
	}

	// End register Flag
	init_seq->reg_init_seq[init_seq->reg_seq_size - 1].reg_addr = RKX11X_I2C_REG_NULL;

	return 0;
}

/*
 * The structure of i2c init sequence:
 *      ser-init-sequence {
 *              status = "okay";
 *              init-sequence = [
 *                      00040000 00200000 00000000 // RESET (GPIO0_A5): active level = 0
 *                      FFFFFFFE 000003E8 00000000 // delay: 1000us
 *              ];
 *      };
 */
int rkx11x_i2c_load_init_seq(struct device *dev, struct device_node *node,
				struct rkx11x_i2c_init_seq *init_seq)
{
	const void *init_seq_data = NULL;
	u32 seq_data_len = 0;
	int ret = 0;

	if ((node == NULL) || (init_seq == NULL)) {
		dev_err(dev, "%s input parameter error\n", __func__);
		return -EINVAL;
	}

	init_seq->reg_init_seq = NULL;
	init_seq->reg_seq_size = 0;
	init_seq->seq_item_size = sizeof(struct rkx11x_i2c_regval);

	if (!of_device_is_available(node)) {
		dev_info(dev, "%pOF is disabled\n", node);

		return 0;
	}

	init_seq_data = of_get_property(node, "init-sequence", &seq_data_len);
	if (!init_seq_data) {
		dev_err(dev, "failed to get property init-sequence\n");
		return -EINVAL;
	}
	if (seq_data_len == 0) {
		dev_err(dev, "init-sequence date is empty\n");
		return -EINVAL;
	}

	ret = rkx11x_i2c_parse_init_seq(dev,
			init_seq_data, seq_data_len, init_seq);
	if (ret) {
		dev_err(dev, "failed to parse init-sequence\n");
		return ret;
	}

	return 0;
}

int rkx11x_i2c_run_init_seq(struct i2c_client *client,
				struct rkx11x_i2c_init_seq *init_seq)
{
	int ret = 0;

	if ((init_seq == NULL) || (init_seq->reg_init_seq == NULL))
		return 0;

	ret = rkx11x_i2c_write_array(client, init_seq->reg_init_seq);

	return ret;
}
