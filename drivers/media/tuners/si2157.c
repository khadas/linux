/*
 * Silicon Labs Si2146/2147/2148/2157/2158 silicon tuner driver
 *
 * Copyright (C) 2014 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

#include "si2157_priv.h"

static const struct dvb_tuner_ops si2157_ops;

/* si2157_i2c_master_send - issue a single I2C message
in master transmit mode */
static int si2157_i2c_master_send(struct i2c_adapter *adap, u8 i2c_addr,
				  const char *buf, int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = i2c_addr;
	msg.flags = 0;
	msg.len = count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? count : ret;
}

/* si2157_i2c_master_recv - issue a single I2C message in master receive mode */
static int si2157_i2c_master_recv(struct i2c_adapter *adap, u8 i2c_addr,
				  char *buf, int count)
{
	struct i2c_msg msg;
	int ret;

	msg.addr = i2c_addr;
	msg.flags = I2C_M_RD;
	msg.len = count;
	msg.buf = buf;

	ret = i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg received), return #bytes received,
	 * else error code.
	 */
	return (ret == 1) ? count : ret;
}

/* execute firmware command */
static int si2157_cmd_execute(struct si2157_dev *dev, struct si2157_cmd *cmd)
{
	int ret;
	unsigned long timeout;

	mutex_lock(&dev->i2c_mutex);

	if (cmd->wlen) {
		/* write cmd and args for firmware */
		ret = si2157_i2c_master_send(dev->i2c_adap, dev->i2c_addr,
			cmd->args, cmd->wlen);
		if (ret < 0) {
			goto err_mutex_unlock;
		} else if (ret != cmd->wlen) {
			ret = -EREMOTEIO;
			goto err_mutex_unlock;
		}
	}

	if (cmd->rlen) {
		/* wait cmd execution terminate */
		#define TIMEOUT 80
		timeout = jiffies + msecs_to_jiffies(TIMEOUT);
		while (!time_after(jiffies, timeout)) {
			ret = si2157_i2c_master_recv(dev->i2c_adap,
			dev->i2c_addr, cmd->args, cmd->rlen);
			if (ret < 0) {
				goto err_mutex_unlock;
			} else if (ret != cmd->rlen) {
				ret = -EREMOTEIO;
				goto err_mutex_unlock;
			}

			/* firmware ready? */
			if ((cmd->args[0] >> 7) & 0x01)
				break;
		}

		dev_dbg(&dev->i2c_adap->dev, "cmd execution took %d ms\n",
				jiffies_to_msecs(jiffies) -
				(jiffies_to_msecs(timeout) - TIMEOUT));

		if (!((cmd->args[0] >> 7) & 0x01)) {
			ret = -ETIMEDOUT;
			goto err_mutex_unlock;
		}
	}

	mutex_unlock(&dev->i2c_mutex);
	return 0;

err_mutex_unlock:
	mutex_unlock(&dev->i2c_mutex);
	dev_dbg(&dev->i2c_adap->dev, "failed=%d\n", ret);
	return ret;
}

static int si2157_init(struct dvb_frontend *fe)
{
	struct si2157_dev *dev = fe->tuner_priv;
	int ret, len, remaining;
	struct si2157_cmd cmd;
	const struct firmware *fw;
	const char *fw_name;
	unsigned int chip_id;

	dev_dbg(&dev->i2c_adap->dev, "\n");

	if (dev->fw_loaded)
		goto warm;

	/* power up */
	if (dev->chiptype == SI2157_CHIPTYPE_SI2146) {
		memcpy(cmd.args, "\xc0\x05\x01\x00\x00\x0b\x00\x00\x01", 9);
		cmd.wlen = 9;
	} else {
		memcpy(cmd.args, "\xc0\x00\x0c\x00\x00\x01\x01\x01\x01\x01\x01\x02\x00\x00\x01", 15);
		cmd.wlen = 15;
	}
	cmd.rlen = 1;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	/* query chip revision */
	memcpy(cmd.args, "\x02", 1);
	cmd.wlen = 1;
	cmd.rlen = 13;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	chip_id = cmd.args[1] << 24 | cmd.args[2] << 16 | cmd.args[3] << 8 |
			cmd.args[4] << 0;

	#define SI2158_A20 ('A' << 24 | 58 << 16 | '2' << 8 | '0' << 0)
	#define SI2148_A20 ('A' << 24 | 48 << 16 | '2' << 8 | '0' << 0)
	#define SI2157_A30 ('A' << 24 | 57 << 16 | '3' << 8 | '0' << 0)
	#define SI2147_A30 ('A' << 24 | 47 << 16 | '3' << 8 | '0' << 0)
	#define SI2146_A10 ('A' << 24 | 46 << 16 | '1' << 8 | '0' << 0)

	switch (chip_id) {
	case SI2158_A20:
	case SI2148_A20:
		fw_name = SI2158_A20_FIRMWARE;
		break;
	case SI2157_A30:
	case SI2147_A30:
	case SI2146_A10:
		fw_name = NULL;
		break;
	default:
		dev_err(&dev->i2c_adap->dev,
			"unknown chip version Si21%d-%c%c%c\n", cmd.args[2],
			cmd.args[1], cmd.args[3], cmd.args[4]);
		ret = -EINVAL;
		goto err;
	}

	dev_info(&dev->i2c_adap->dev, "found a 'Silicon Labs Si21%d-%c%c%c'\n",
			cmd.args[2], cmd.args[1], cmd.args[3], cmd.args[4]);

	if (fw_name == NULL)
		goto skip_fw_download;

	/* request the firmware, this will block and timeout */
	ret = request_firmware(&fw, fw_name, &dev->i2c_adap->dev);
	if (ret) {
		dev_err(&dev->i2c_adap->dev, "firmware file '%s' not found\n",
				fw_name);
		goto err;
	}

	/* firmware should be n chunks of 17 bytes */
	if (fw->size % 17 != 0) {
		dev_err(&dev->i2c_adap->dev, "firmware file '%s' is invalid\n",
				fw_name);
		ret = -EINVAL;
		goto err_release_firmware;
	}

	dev_info(&dev->i2c_adap->dev, "downloading firmware from file '%s'\n",
			fw_name);

	for (remaining = fw->size; remaining > 0; remaining -= 17) {
		len = fw->data[fw->size - remaining];
		memcpy(cmd.args, &fw->data[(fw->size - remaining) + 1], len);
		cmd.wlen = len;
		cmd.rlen = 1;
		ret = si2157_cmd_execute(dev, &cmd);
		if (ret) {
			dev_err(&dev->i2c_adap->dev, "firmware download failed %d\n",
					ret);
			goto err_release_firmware;
		}
	}

	release_firmware(fw);

skip_fw_download:
	/* reboot the tuner with new firmware? */
	memcpy(cmd.args, "\x01\x01", 2);
	cmd.wlen = 2;
	cmd.rlen = 1;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	/* query firmware version */
	memcpy(cmd.args, "\x11", 1);
	cmd.wlen = 1;
	cmd.rlen = 10;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	dev_info(&dev->i2c_adap->dev, "firmware version: %c.%c.%d\n",
			cmd.args[6], cmd.args[7], cmd.args[8]);

	dev->fw_loaded = true;

warm:
	dev->active = true;
	return 0;

err_release_firmware:
	release_firmware(fw);
err:
	dev_dbg(&dev->i2c_adap->dev, "failed=%d\n", ret);
	return ret;
}

static int si2157_sleep(struct dvb_frontend *fe)
{
	struct si2157_dev *dev = fe->tuner_priv;
	int ret;
	struct si2157_cmd cmd;

	dev_dbg(&dev->i2c_adap->dev, "\n");

	dev->active = false;

	/* standby */
	memcpy(cmd.args, "\x16\x00", 2);
	cmd.wlen = 2;
	cmd.rlen = 1;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	return 0;
err:
	dev_dbg(&dev->i2c_adap->dev, "failed=%d\n", ret);
	return ret;
}

static int si2157_set_params(struct dvb_frontend *fe)
{
	struct si2157_dev *dev = fe->tuner_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret;
	struct si2157_cmd cmd;
	u8 bandwidth, delivery_system;
	u32 if_frequency = 5000000;

	dev_dbg(&dev->i2c_adap->dev,
			"delivery_system=%d frequency=%u bandwidth_hz=%u\n",
			c->delivery_system, c->frequency, c->bandwidth_hz);

	if (!dev->active) {
		ret = -EAGAIN;
		goto err;
	}

	if (c->bandwidth_hz <= 6000000)
		bandwidth = 0x06;
	else if (c->bandwidth_hz <= 7000000)
		bandwidth = 0x07;
	else if (c->bandwidth_hz <= 8000000)
		bandwidth = 0x08;
	else
		bandwidth = 0x0f;

	switch (c->delivery_system) {
	case SYS_ATSC:
			delivery_system = 0x00;
			if_frequency = 3250000;
			break;
	case SYS_DVBC_ANNEX_B:
			delivery_system = 0x10;
			if_frequency = 4000000;
			break;
	case SYS_DVBT:
	case SYS_DVBT2: /* it seems DVB-T and DVB-T2 both are 0x20 here */
			delivery_system = 0x20;
			break;
	case SYS_DVBC_ANNEX_A:
			delivery_system = 0x30;
			break;
	default:
			ret = -EINVAL;
			goto err;
	}

	memcpy(cmd.args, "\x14\x00\x03\x07\x00\x00", 6);
	cmd.args[4] = delivery_system | bandwidth;
	if (dev->inversion)
		cmd.args[5] = 0x01;
	cmd.wlen = 6;
	cmd.rlen = 4;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	if (dev->chiptype == SI2157_CHIPTYPE_SI2146)
		memcpy(cmd.args, "\x14\x00\x02\x07\x00\x01", 6);
	else
		memcpy(cmd.args, "\x14\x00\x02\x07\x01\x00", 6);
	cmd.wlen = 6;
	cmd.rlen = 4;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	/* set if frequency if needed */
	if (if_frequency != dev->if_frequency) {
		memcpy(cmd.args, "\x14\x00\x06\x07", 4);
		cmd.args[4] = (if_frequency / 1000) & 0xff;
		cmd.args[5] = ((if_frequency / 1000) >> 8) & 0xff;
		cmd.wlen = 6;
		cmd.rlen = 4;
		ret = si2157_cmd_execute(dev, &cmd);
		if (ret)
			goto err;

		dev->if_frequency = if_frequency;
	}

	/* set frequency */
	memcpy(cmd.args, "\x41\x00\x00\x00\x00\x00\x00\x00", 8);
	cmd.args[4] = (c->frequency >>  0) & 0xff;
	cmd.args[5] = (c->frequency >>  8) & 0xff;
	cmd.args[6] = (c->frequency >> 16) & 0xff;
	cmd.args[7] = (c->frequency >> 24) & 0xff;
	cmd.wlen = 8;
	cmd.rlen = 1;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err;

	return 0;
err:
	dev_dbg(&dev->i2c_adap->dev, "failed=%d\n", ret);
	return ret;
}

static int si2157_get_if_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct si2157_dev *dev = fe->tuner_priv;

	*frequency = dev->if_frequency;
	return 0;
}

static int si2157_release(struct dvb_frontend *fe)
{
	struct si2157_dev *dev = fe->tuner_priv;

	dev_dbg(&dev->i2c_adap->dev, "%s:\n", __func__);

	kfree(fe->tuner_priv);

	return 0;
}

static const struct dvb_tuner_ops si2157_ops = {
	.info = {
		.name           = "Silicon Labs Si2146/2147/2148/2157/2158",
		.frequency_min  = 55000000,
		.frequency_max  = 862000000,
	},

	.release = si2157_release,
	.init = si2157_init,
	.sleep = si2157_sleep,
	.set_params = si2157_set_params,
	.get_if_frequency = si2157_get_if_frequency,
};

struct dvb_frontend *si2157_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c, const struct si2157_config *cfg)
{
	struct si2157_dev *dev;
	struct si2157_cmd cmd;
	int ret;

	if (!cfg) {
		dev_err(&i2c->dev, "no configuration submitted\n");
		goto err;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		dev_err(&i2c->dev, "%s: kzalloc() failed\n", KBUILD_MODNAME);
		goto err;
	}

	dev->i2c_adap = i2c;
	dev->i2c_addr = cfg->i2c_addr;
	dev->inversion = cfg->inversion;
	dev->fw_loaded = false;
	dev->chiptype = SI2157_CHIPTYPE_SI2157; /* (u8)id->driver_data; */
	dev->if_frequency = 5000000; /* default value of property 0x0706 */
	mutex_init(&dev->i2c_mutex);

	/* check if the tuner is there */
	cmd.wlen = 0;
	cmd.rlen = 1;
	ret = si2157_cmd_execute(dev, &cmd);
	if (ret)
		goto err_kfree;

	memcpy(&fe->ops.tuner_ops, &si2157_ops, sizeof(struct dvb_tuner_ops));
	fe->tuner_priv = dev;

	dev_info(&i2c->dev, "%s: Silicon Labs %s successfully attached\n",
			KBUILD_MODNAME,
			dev->chiptype == SI2157_CHIPTYPE_SI2146 ?
			"Si2146" : "Si2147/2148/2157/2158");

	return fe;

err_kfree:
	kfree(dev);
err:
	dev_dbg(&i2c->dev, "failed=%d\n", ret);
	return NULL;
}
EXPORT_SYMBOL(si2157_attach);

MODULE_DESCRIPTION("Silicon Labs Si2146/2147/2148/2157/2158 silicon tuner driver");
MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(SI2158_A20_FIRMWARE);
