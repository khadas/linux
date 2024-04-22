/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __LNB_CONTROLLER_H__
#define __LNB_CONTROLLER_H__

#include <linux/i2c.h>
#include <linux/gpio.h>

enum LNBC_STATE {
	LNBC_STATE_UNKNOWN, /* Unknown */
	LNBC_STATE_SLEEP,   /* Sleep */
	LNBC_STATE_ACTIVE   /* Active */
};

enum LNBC_VOLTAGE {
	LNBC_VOLTAGE_OFF,  /* OFF output voltage. */
	LNBC_VOLTAGE_LOW,  /* Low voltage. */
	LNBC_VOLTAGE_HIGH, /* High voltage. */
	LNBC_VOLTAGE_AUTO, /* Control by demodulator. (For single cable, using TXEN) */
};

enum LNBC_TONE {
	LNBC_TONE_OFF,  /* Not output tone signal. */
	LNBC_TONE_ON,   /* Output tone signal. */
	LNBC_TONE_AUTO, /* Control tone by demodulator. (Default) */
};

enum LNBC_TRANSMIT_MODE {
	LNBC_TRANSMIT_MODE_TX,   /* TX mode. */
	LNBC_TRANSMIT_MODE_RX,   /* RX mode. */
	LNBC_TRANSMIT_MODE_AUTO, /* Control by demodulator. (For receiving DiSEqC2.x reply) */
};

enum LNBC_CONFIG_ID {
	LNBC_CONFIG_ID_TONE_INTERNAL, /* Tone mode (0: External tone, 1: Internal tone). */
	LNBC_CONFIG_ID_LOW_VOLTAGE,   /* Definition of voltage for "Low voltage". */
	LNBC_CONFIG_ID_HIGH_VOLTAGE   /* Definition of voltage for "High voltage". */
};

/*
 * LNB controller diagnostic status bit definitions used for get status return value.
 */
#define LNBC_DIAG_STATUS_THERMALSHUTDOWN 0x00000001 /* Thermal shutdown triggered. */
#define LNBC_DIAG_STATUS_OVERCURRENT     0x00000002 /* Over current protection triggered. */
#define LNBC_DIAG_STATUS_CABLEOPEN       0x00000004 /* Cable is not connected. */
#define LNBC_DIAG_STATUS_VOUTOUTOFRANGE  0x00000008 /* Output voltage is out of range. */
#define LNBC_DIAG_STATUS_VINOUTOFRANGE   0x00000010 /* Input voltage is out of range. */
#define LNBC_DIAG_STATUS_BACKBIAS        0x00000020 /* Back bias is detected. */
#define LNBC_DIAG_STATUS_LNBCDISABLE     0x00010000 /* LNB controller is now disabled. */

struct lnbc {
	struct i2c_adapter *i2c_adap; /* i2c adapter. */
	unsigned char i2c_addr;       /* i2c address. */
	int is_internal_tone;         /* Tone mode flag. (0: External tone, 1: Internal tone) */
	int low_voltage;              /* Definition of low voltage. */
	int high_voltage;             /* Definition of high voltage. */
	void *user;                   /* User defined data. */

	enum LNBC_STATE state;                 /* The state of this driver */
	enum LNBC_VOLTAGE voltage;             /* Current voltage. */
	enum LNBC_TONE tone;                   /* Current tone status. */
	enum LNBC_TRANSMIT_MODE transmit_mode; /* Current transmit mode. */

	struct gpio_desc *gpio_lnb_en;
	struct gpio_desc *gpio_lnb_sel;

	int (*init)(struct lnbc *lnbc);
	int (*set_config)(struct lnbc *lnbc, enum LNBC_CONFIG_ID config_id, int value);
	int (*set_voltage)(struct lnbc *lnbc, enum LNBC_VOLTAGE voltage);
	int (*set_tone)(struct lnbc *lnbc, enum LNBC_TONE tone);
	int (*set_transmit_mode)(struct lnbc *lnbc, enum LNBC_TRANSMIT_MODE transmit_mode);
	int (*sleep)(struct lnbc *lnbc);
	int (*wake_up)(struct lnbc *lnbc);
	int (*get_diag_status)(struct lnbc *lnbc, unsigned int *status,
			unsigned int *status_supported);
};

#endif /* __LNB_H__ */
