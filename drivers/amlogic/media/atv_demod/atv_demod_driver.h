/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_DEMOD_DRIVER_H__
#define __ATV_DEMOD_DRIVER_H__


#include <linux/amlogic/cpu_version.h>
#include <media/v4l2-device.h>
#include <dvb_frontend.h>
#include "atv_demod_v4l2.h"


struct aml_atvdemod_device {
	char *name;
	struct class cls;
	struct device *dev;

	int tuner_id;
	u8 i2c_addr;
	struct i2c_adapter i2c_adap;

	unsigned int if_freq;
	unsigned int if_inv;
	u64 std;
	unsigned int audmode;
	unsigned int sound_mode;
	int fre_offset;

	struct pinctrl *agc_pin;
	const char *pin_name;

	struct v4l2_frontend v4l2_fe;
	bool analog_attached;
	bool tuner_attached;

	int irq;

	void __iomem *demod_reg_base;
	void __iomem *audiodemod_reg_base;
	void __iomem *hiu_reg_base;
	void __iomem *periphs_reg_base;
	void __iomem *audio_reg_base;

	int btsc_sap_mode; /*0: off 1:monitor 2:auto */

#define ATVDEMOD_STATE_IDEL  0
#define ATVDEMOD_STATE_WORK  1
#define ATVDEMOD_STATE_SLEEP 2
	int atvdemod_state;

	int (*demod_reg_write)(unsigned int reg, unsigned int val);
	int (*demod_reg_read)(unsigned int reg, unsigned int *val);

	int (*audio_reg_write)(unsigned int reg, unsigned int val);
	int (*audio_reg_read)(unsigned int reg, unsigned int *val);

	int (*hiu_reg_write)(unsigned int reg, unsigned int val);
	int (*hiu_reg_read)(unsigned int reg, unsigned int *val);

	int (*periphs_reg_write)(unsigned int reg, unsigned int val);
	int (*periphs_reg_read)(unsigned int reg, unsigned int *val);
};

extern struct aml_atvdemod_device *amlatvdemod_devp;

extern int aml_atvdemod_attach_demod(struct aml_atvdemod_device *dev);
extern int aml_atvdemod_attach_tuner(struct aml_atvdemod_device *dev);

#endif /* __ATV_DEMOD_DRIVER_H__ */
