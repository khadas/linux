/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_drv.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _AML_LDIM_DRV_H_
#define _AML_LDIM_DRV_H_
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>

/*20180629: initial version */
/*20180725: new pwm control flow support */
/*20180730: algorithm clear up */
/*20180820: pq tooling support, espically optimize some alg parameters */
/*20181101: fix ldim_op_func null mistake, add new spi api support */
/*20181203: add 50/60hz change & iw7027 error handle support */
/*20181220: add tl1 support*/
/*20190103: add analog pwm support*/
#define LDIM_DRV_VER    "20190103"

extern unsigned char ldim_debug_print;

extern int LD_remap_lut[16][32];

#define AML_LDIM_MODULE_NAME "aml_ldim"
#define AML_LDIM_DRIVER_NAME "aml_ldim"
#define AML_LDIM_DEVICE_NAME "aml_ldim"
#define AML_LDIM_CLASS_NAME  "aml_ldim"

/*========================================*/

struct ldim_operate_func_s {
	unsigned short h_region_max;
	unsigned short v_region_max;
	unsigned short total_region_max;
	void (*remap_update)(struct LDReg_s *nPRM,
		unsigned int avg_update_en, unsigned int matrix_update_en);
	void (*stts_init)(unsigned int pic_h, unsigned int pic_v,
		unsigned int blk_vnum, unsigned int blk_hnum);
	void (*ldim_init)(struct LDReg_s *nPRM,
		unsigned int bl_en, unsigned int hvcnt_bypass);
};

/*========================================*/

extern int  ldim_round(int ix, int ib);
extern void LD_ConLDReg(struct LDReg_s *Reg);
extern void ld_fw_cfg_once(struct LDReg_s *nPRM);

/* ldim hw */
extern int ldim_hw_reg_dump(char *buf);
extern void ldim_stts_read_region(unsigned int nrow, unsigned int ncol);
extern void ldim_set_matrix_ycbcr2rgb(void);
extern void ldim_set_matrix_rgb2ycbcr(int mode);

extern void ldim_initial_txlx(struct LDReg_s *nPRM,
		unsigned int ldim_bl_en, unsigned int ldim_hvcnt_bypass);
extern void ldim_stts_initial_txlx(unsigned int pic_h, unsigned int pic_v,
		unsigned int blk_vnum, unsigned int blk_hnum);
extern void ldim_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
		unsigned int blk_vnum, unsigned int blk_hnum);
extern void ldim_remap_update_txlx(struct LDReg_s *nPRM,
		unsigned int avg_update_en, unsigned int matrix_update_en);

#endif
