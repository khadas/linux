/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _BL_EXTERN_H_
#define _BL_EXTERN_H_
#include <linux/i2c.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include "../../lcd_common.h"

#define BLEX(fmt, args...)	pr_info("bl_ext: " fmt "", ##args)
#define BLEXERR(fmt, args...)	pr_err("bl_ext: error: " fmt "", ##args)

#define BL_EXTERN_DRIVER	"bl_extern"

struct bl_extern_i2c_dev_s *bl_extern_i2c_get_dev(unsigned char addr);

int bl_extern_i2c_write(struct i2c_client *i2client,
			unsigned char *buff, unsigned int len);
int bl_extern_i2c_read(struct i2c_client *i2client,
		       unsigned char *buff, unsigned int len);

#ifdef CONFIG_AMLOGIC_BL_EXTERN_I2C_LP8556
int i2c_lp8556_probe(struct bl_extern_driver_s *bext);
int i2c_lp8556_remove(struct bl_extern_driver_s *bext);
#endif

#ifdef CONFIG_AMLOGIC_BL_EXTERN_MIPI_LT070ME05
int mipi_lt070me05_probe(struct bl_extern_driver_s *bext);
int mipi_lt070me05_remove(struct bl_extern_driver_s *bext);
#endif

#endif
