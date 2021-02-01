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

/* ldim spi api*/
int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf,
		   int wlen);
int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int wlen,
		  unsigned char *rbuf, int rlen);
int ldim_spi_read_sync(struct spi_device *spi, unsigned char *tbuf,
		       unsigned char *rbuf, int len);
int ldim_spi_driver_add(struct ldim_dev_config_s *ldev_conf);
int ldim_spi_driver_remove(struct ldim_dev_config_s *ldev_conf);

/* ldim global api */
void ldim_gpio_set(int index, int value);
unsigned int ldim_gpio_get(int index);
void ldim_set_duty_pwm(struct bl_pwm_config_s *ld_pwm);
void ldim_pwm_off(struct bl_pwm_config_s *ld_pwm);

/* ldim dev api */
int ldim_dev_iw7027_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw7027_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_iw7027_he_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw7027_he_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_iw7038_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw7038_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_iw70xx_mcu_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw70xx_mcu_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_ob3350_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_ob3350_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_global_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_global_remove(struct aml_ldim_driver_s *ldim_drv);

#endif /* __LDIM_DEV_DRV_H */
