/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_VENC_H__
#define __AML_LCD_VENC_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#define LCD_WAIT_VSYNC_TIMEOUT 50000

struct lcd_venc_op_s {
	int init_flag;
	void (*wait_vsync)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*get_max_lcnt)(struct aml_lcd_drv_s *pdrv);
	void (*gamma_test_en)(struct aml_lcd_drv_s *pdrv, int flag);
	int (*venc_debug_test)(struct aml_lcd_drv_s *pdrv, unsigned int num);
	void (*venc_set_timing)(struct aml_lcd_drv_s *pdrv);
	void (*venc_set)(struct aml_lcd_drv_s *pdrv);
	void (*venc_change)(struct aml_lcd_drv_s *pdrv);
	void (*venc_enable)(struct aml_lcd_drv_s *pdrv, int flag);
	void (*mute_set)(struct aml_lcd_drv_s *pdrv, unsigned char flag);
	int (*get_venc_init_config)(struct aml_lcd_drv_s *pdrv);
	void (*venc_vrr_recovery)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*get_encl_line_cnt)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*get_encl_frm_cnt)(struct aml_lcd_drv_s *pdrv);
};

int lcd_venc_op_init_dft(struct aml_lcd_drv_s *pdrv, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_t7(struct aml_lcd_drv_s *pdrv, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_a4(struct aml_lcd_drv_s *pdrv, struct lcd_venc_op_s *venc_op);

#endif
