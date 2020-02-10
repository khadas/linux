/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _AML_LDIM_DRV_H_
#define _AML_LDIM_DRV_H_
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>

/*20200215: init version */
#define LDIM_DRV_VER    "20200215"

extern unsigned char ldim_debug_print;

extern int ld_remap_lut[16][32];

#define AML_LDIM_MODULE_NAME "aml_ldim"
#define AML_LDIM_DRIVER_NAME "aml_ldim"
#define AML_LDIM_DEVICE_NAME "aml_ldim"
#define AML_LDIM_CLASS_NAME  "aml_ldim"

/*========================================*/

struct ldim_operate_func_s {
	unsigned short h_region_max;
	unsigned short v_region_max;
	unsigned short total_region_max;
	void (*remap_update)(struct LDReg_s *nprm,
			     unsigned int avg_update_en,
			     unsigned int matrix_update_en);
	void (*stts_init)(unsigned int pic_h, unsigned int pic_v,
			  unsigned int blk_vnum,
			  unsigned int blk_hnum);
	void (*ldim_init)(struct LDReg_s *nprm,
			  unsigned int bl_en, unsigned int hvcnt_bypass);
};

/*========================================*/
void ld_con_ld_reg(struct LDReg_s *reg);
void ld_fw_cfg_once(struct LDReg_s *nprm);

/* ldim hw */
int ldim_hw_reg_dump(char *buf);
void ldim_stts_read_region(unsigned int nrow, unsigned int ncol);
void ldim_set_matrix_ycbcr2rgb(void);
void ldim_set_matrix_rgb2ycbcr(int mode);

void ldim_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
			   unsigned int blk_vnum, unsigned int blk_hnum);
#endif
