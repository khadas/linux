/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */
#ifndef __AML_BL_DRV_H__
#define __AML_BL_DRV_H__
#include <linux/amlogic/media/vout/lcd/aml_bl.h>

static inline unsigned int bl_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long val = num;

	do_div(val, den);

	return (unsigned int)val;
}

int bl_pwm_init_config_probe(struct bl_data_s *bdata);
enum bl_pwm_port_e bl_pwm_str_to_num(const char *str);
char *bl_pwm_num_to_str(unsigned int num);
void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status);
void bl_pwm_set_duty(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm);
void bl_pwm_set_level(struct aml_bl_drv_s *bdrv,
		      struct bl_pwm_config_s *bl_pwm, unsigned int level);
void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm);
void bl_pwm_mapping_init(struct aml_bl_drv_s *bdrv);
int bl_pwm_channel_register(struct device *dev, phandle pwm_phandle,
			    struct bl_pwm_config_s *bl_pwm);

#endif
