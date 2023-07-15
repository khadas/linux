/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __LDIM_DEV_DRV_H
#define __LDIM_DEV_DRV_H
#include <linux/spi/spi.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include "../lcd_bl.h"

int ldim_spi_dma_cycle_align_byte(int size);

/* ldim spi api*/
int ldim_spi_write_async(struct spi_device *spi, unsigned char *tbuf,
			 unsigned char *rbuf, int tlen, int dma_mode, int max_len);
int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen);
int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int wlen,
		  unsigned char *rbuf, int rlen);
int ldim_spi_read_sync(struct spi_device *spi, unsigned char *tbuf,
		       unsigned char *rbuf, int len);
void ldim_spi_async_busy_clear(void);
int ldim_spi_driver_add(struct ldim_dev_driver_s *dev_drv);
int ldim_spi_driver_remove(struct ldim_dev_driver_s *dev_drv);

/* ldim global api */
void ldim_gpio_set(struct ldim_dev_driver_s *dev_drv, int index, int value);
unsigned int ldim_gpio_get(struct ldim_dev_driver_s *dev_drv, int index);
void ldim_set_duty_pwm(struct bl_pwm_config_s *ld_pwm);
void ldim_pwm_off(struct bl_pwm_config_s *ld_pwm);

/* ldim dev api */
int ldim_dev_iw7027_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw7027_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_blmcu_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_blmcu_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_ob3350_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_ob3350_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_global_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_global_remove(struct aml_ldim_driver_s *ldim_drv);

#endif /* __LDIM_DEV_DRV_H */
