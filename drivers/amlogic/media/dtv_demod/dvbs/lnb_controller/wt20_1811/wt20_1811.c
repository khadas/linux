// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include "lnb_controller.h"
#include "wt20_1811.h"
#include "i2c_func.h"

static int wt20_1811_init(struct lnbc *lnbc);
static int wt20_1811_set_config(struct lnbc *lnbc,
		enum LNBC_CONFIG_ID config_id, int value);
static int wt20_1811_set_voltage(struct lnbc *lnbc, enum LNBC_VOLTAGE voltage);
static int wt20_1811_set_tone(struct lnbc *lnbc, enum LNBC_TONE tone);
static int wt20_1811_set_transmit_mode(struct lnbc *lnbc,
		enum LNBC_TRANSMIT_MODE transmit_mode);
static int wt20_1811_sleep(struct lnbc *lnbc);
static int wt20_1811_wake_up(struct lnbc *lnbc);
static int wt20_1811_get_diag_status(struct lnbc *lnbc, unsigned int *status,
		unsigned int *status_supported);
static int create_data(struct lnbc *lnbc, unsigned char is_enable,
		enum LNBC_VOLTAGE voltage, unsigned char *data);

int wt20_1811_create(struct lnbc *lnbc, struct i2c_adapter *i2c_adap,
		unsigned char i2c_addr)
{
	if (!lnbc || !i2c_adap || !i2c_addr)
		return -EFAULT;

	lnbc->i2c_adap = i2c_adap;
	lnbc->i2c_addr = i2c_addr;
	lnbc->init = wt20_1811_init;
	lnbc->set_config = wt20_1811_set_config;
	lnbc->set_voltage = wt20_1811_set_voltage;
	lnbc->set_tone = wt20_1811_set_tone;
	lnbc->set_transmit_mode = wt20_1811_set_transmit_mode;
	lnbc->sleep = wt20_1811_sleep;
	lnbc->wake_up = wt20_1811_wake_up;
	lnbc->get_diag_status = wt20_1811_get_diag_status;
	lnbc->is_internal_tone = 1;

	if (lnb_high_voltage == 3) {
		lnbc->low_voltage = WT20_1811_CONFIG_VOLTAGE_LOW_15_667V;
		lnbc->high_voltage = WT20_1811_CONFIG_VOLTAGE_HIGH_20_000V;
	} else if (lnb_high_voltage == 2) {
		lnbc->low_voltage = WT20_1811_CONFIG_VOLTAGE_LOW_14_333V;
		lnbc->high_voltage = WT20_1811_CONFIG_VOLTAGE_HIGH_19_667V;
	} else if (lnb_high_voltage == 1) {
		lnbc->low_voltage = WT20_1811_CONFIG_VOLTAGE_LOW_13_667V;
		lnbc->high_voltage = WT20_1811_CONFIG_VOLTAGE_HIGH_19_000V;
	} else {
		lnbc->low_voltage = WT20_1811_CONFIG_VOLTAGE_LOW_13_333V;   /* Default value */
		lnbc->high_voltage = WT20_1811_CONFIG_VOLTAGE_HIGH_18_667V; /* Default value */
	}

	lnbc->state = LNBC_STATE_UNKNOWN;
	lnbc->voltage = LNBC_VOLTAGE_LOW;
	lnbc->tone = LNBC_TONE_AUTO;
	lnbc->transmit_mode = LNBC_TRANSMIT_MODE_TX;

	return 0;
}

static int wt20_1811_init(struct lnbc *lnbc)
{
	lnbc->voltage = LNBC_VOLTAGE_OFF;
	lnbc->state = LNBC_STATE_SLEEP;

	return 0;
}

static int wt20_1811_set_config(struct lnbc *lnbc,
		enum LNBC_CONFIG_ID config_id, int value)
{
	return 0;
}

static int wt20_1811_set_voltage(struct lnbc *lnbc,
		enum LNBC_VOLTAGE voltage)
{
	unsigned char data = 0;
	unsigned short len = 0;
	unsigned char buffer[2] = { 0 };
	int ret = 0;

	if (!lnbc)
		return -EFAULT;

	if (lnbc->state != LNBC_STATE_ACTIVE && lnbc->state != LNBC_STATE_SLEEP)
		return -EINVAL;

	ret = create_data(lnbc, 1, voltage, &data);
	if (ret)
		return ret;

	buffer[0] = 0x00;
	buffer[1] = data;
	len = 2;
	ret = aml_demod_i2c_write(lnbc->i2c_adap, lnbc->i2c_addr, buffer, len);
	if (ret)
		return ret;

	if (voltage == LNBC_VOLTAGE_LOW && lnbc->voltage == LNBC_VOLTAGE_HIGH)
		usleep_range(9000, 10000);
	else if (voltage == LNBC_VOLTAGE_HIGH && lnbc->voltage == LNBC_VOLTAGE_LOW)
		usleep_range(24000, 25000);

	lnbc->voltage = voltage;
	lnbc->state = LNBC_STATE_ACTIVE;

	return 0;
}

static int wt20_1811_set_tone(struct lnbc *lnbc, enum LNBC_TONE tone)
{
	if (!lnbc)
		return -EFAULT;

	if (lnbc->state != LNBC_STATE_ACTIVE && lnbc->state != LNBC_STATE_SLEEP)
		return -2;

	switch (tone) {
	case LNBC_TONE_ON:
	case LNBC_TONE_OFF:
		/* doesn't support this function. */
		break;

	case LNBC_TONE_AUTO:
		break;

	default:
		return -EINVAL;
	}

	lnbc->tone = tone;

	return 0;
}

static int wt20_1811_set_transmit_mode(struct lnbc *lnbc,
		enum LNBC_TRANSMIT_MODE transmit_mode)
{
	if (!lnbc)
		return -EFAULT;

	if (lnbc->state != LNBC_STATE_ACTIVE && lnbc->state != LNBC_STATE_SLEEP)
		return -2;

	switch (transmit_mode) {
	case LNBC_TRANSMIT_MODE_TX:
		/* Do nothing */
		break;

	case LNBC_TRANSMIT_MODE_RX:
	case LNBC_TRANSMIT_MODE_AUTO:
		/* doesn't support this function. */
		break;

	default:
		return -EINVAL;
	}

	lnbc->transmit_mode = transmit_mode;

	return 0;
}

static int wt20_1811_sleep(struct lnbc *lnbc)
{
	unsigned char data = 0;
	unsigned short len = 0;
	unsigned char buffer[2] = { 0 };
	int ret = 0;

	if (!lnbc)
		return -EFAULT;

	switch (lnbc->state) {
	case LNBC_STATE_ACTIVE:
		/* Continue */
		break;

	case LNBC_STATE_SLEEP:
		/* Do nothing */
		return 0;

	case LNBC_STATE_UNKNOWN:
	default:
		/* Error */
		return -EINVAL;
	}

	ret = create_data(lnbc, 0, lnbc->voltage, &data);
	if (ret)
		return ret;

	buffer[0] = 0x00;
	buffer[1] = data;
	len = 2;
	ret = aml_demod_i2c_write(lnbc->i2c_adap, lnbc->i2c_addr, buffer, len);
	if (ret)
		return ret;

	lnbc->state = LNBC_STATE_SLEEP;

	return 0;
}

static int wt20_1811_wake_up(struct lnbc *lnbc)
{
	unsigned char data = 0;
	unsigned short len = 0;
	unsigned char buffer[2] = { 0 };
	int ret = 0;

	if (!lnbc)
		return -EFAULT;

	switch (lnbc->state) {
	case LNBC_STATE_ACTIVE:
		/* Do nothing */
		return 0;

	case LNBC_STATE_SLEEP:
		/* Continue */
		break;

	case LNBC_STATE_UNKNOWN:
	default:
		/* Error */
		return -EINVAL;
	}

	ret = create_data(lnbc, 1, lnbc->voltage, &data);
	if (ret)
		return ret;

	buffer[0] = 0x00;
	buffer[1] = data;
	len = 2;
	ret = aml_demod_i2c_write(lnbc->i2c_adap, lnbc->i2c_addr, buffer, len);
	if (ret)
		return ret;

	lnbc->state = LNBC_STATE_ACTIVE;

	return 0;
}

static int wt20_1811_get_diag_status(struct lnbc *lnbc, unsigned int *status,
		unsigned int *status_supported)
{
	unsigned char data = 0;
	unsigned short len = 0;
	int ret = 0;

	if (!lnbc || !status_supported)
		return -EFAULT;

	if (status_supported)
		*status_supported = LNBC_DIAG_STATUS_DIS |
		LNBC_DIAG_STATUS_CPOK |
		LNBC_DIAG_STATUS_OCP |
		LNBC_DIAG_STATUS_UNUSED1 |
		LNBC_DIAG_STATUS_PNG |
		LNBC_DIAG_STATUS_UNUSED2 |
		LNBC_DIAG_STATUS_TSD |
		LNBC_DIAG_STATUS_UVLO;

	data = 0x00;
	len = 1;
	ret = aml_demod_i2c_write(lnbc->i2c_adap, lnbc->i2c_addr, &data, len);
	if (ret)
		return ret;

	len = 1;
	ret = aml_demod_i2c_read(lnbc->i2c_adap, lnbc->i2c_addr, &data, len);
	if (ret)
		return ret;

	/* STATUS register
	 * +---------+---------+---------+---------+---------+---------+---------+---------+
	 * |  UVLO   |   TSD   |    -    |   PNG   |    -    |   OCP   |  CPOK   |   DIS   |
	 * +---------+---------+---------+---------+---------+---------+---------+---------+
	 */

	*status = 0;

	if (data & 0x1)
		*status |= LNBC_DIAG_STATUS_DIS;

	if (data & 0x2)
		*status |= LNBC_DIAG_STATUS_CPOK;

	if (data & 0x4)
		*status |= LNBC_DIAG_STATUS_OCP;

	if (data & 0x10)
		*status |= LNBC_DIAG_STATUS_PNG;

	if (data & 0x40)
		*status |= LNBC_DIAG_STATUS_TSD;

	if (data & 0x80)
		*status |= LNBC_DIAG_STATUS_UVLO;

	*status = data;

	return 0;
}

static int create_data(struct lnbc *lnbc, unsigned char is_enable,
		enum LNBC_VOLTAGE voltage, unsigned char *data)
{
	if (!lnbc || !data)
		return -EFAULT;

	*data = 0;

	if (is_enable)
		*data |= 0x08;

	switch (voltage) {
	case LNBC_VOLTAGE_OFF:
		*data &= ~0x08;
		break;

	case LNBC_VOLTAGE_LOW:
		switch (lnbc->low_voltage) {
		case WT20_1811_CONFIG_VOLTAGE_LOW_13_333V:
			/* 13.333V */
			*data |= 0x00;
			break;

		case WT20_1811_CONFIG_VOLTAGE_LOW_13_667V:
			/* 13.667V */
			*data |= 0x01;
			break;

		case WT20_1811_CONFIG_VOLTAGE_LOW_14_333V:
			/* 14.333V */
			*data |= 0x02;
			break;

		case WT20_1811_CONFIG_VOLTAGE_LOW_15_667V:
			/* 15.667V */
			*data |= 0x03;
			break;

		default:
			return -EINVAL;
		}
		break;

	case LNBC_VOLTAGE_HIGH:
		switch (lnbc->high_voltage) {
		case WT20_1811_CONFIG_VOLTAGE_HIGH_18_667V:
			/* 18.667V */
			*data |= 0x04;
			break;

		case WT20_1811_CONFIG_VOLTAGE_HIGH_19_000V:
			/* 19.000V */
			*data |= 0x05;
			break;

		case WT20_1811_CONFIG_VOLTAGE_HIGH_19_667V:
			/* 19.333V */
			*data |= 0x06;
			break;

		case WT20_1811_CONFIG_VOLTAGE_HIGH_20_000V:
			/* 19.667V */
			*data |= 0x07;
			break;

		default:
			return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}
