/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _AML_LDIM_DRV_H_
#define _AML_LDIM_DRV_H_
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>

/*20200215: init version */
/*20201120: support t7 */
#define LDIM_DRV_VER    "20201120"

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
	int (*alloc_rmem)(void);
	void (*remap_update)(struct LDReg_s *nprm,
			     unsigned int avg_update_en,
			     unsigned int matrix_update_en);
	void (*stts_init)(unsigned int pic_h, unsigned int pic_v,
			  unsigned int blk_vnum, unsigned int blk_hnum);
	void (*remap_init)(struct LDReg_s *nprm,
			   unsigned int bl_en, unsigned int hvcnt_bypass);
	void (*vs_arithmetic)(void);
};

/*========================================*/

/* ldim func */
int ldim_round(int ix, int ib);
void ld_func_cfg_ldreg(struct LDReg_s *reg);
void ld_func_fw_cfg_once(struct LDReg_s *nprm);

/* ldim hw */
#define LDIM_VPU_DMA_WR    0
#define LDIM_VPU_DMA_RD    1

void ldim_hw_vpu_dma_mif_en(int rw_sel, int flag);
void ldim_hw_remap_en(int flag);
void ldim_hw_remap_demo_en(int flag);
int ldim_hw_reg_dump(char *buf);
int ldim_hw_reg_dump_tm2(char *buf);
void ldim_hw_stts_read_zone(unsigned int nrow, unsigned int ncol);

void ldim_hw_remap_init_txlx(struct LDReg_s *nprm, unsigned int ldim_bl_en,
			     unsigned int ldim_hvcnt_bypass);
void ldim_hw_remap_init_tm2(struct LDReg_s *nprm, unsigned int ldim_bl_en,
			    unsigned int ldim_hvcnt_bypass);
void ldim_hw_stts_initial_txlx(unsigned int pic_h, unsigned int pic_v,
			       unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
			      unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_stts_initial_tm2(unsigned int pic_h, unsigned int pic_v,
			      unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_remap_update_txlx(struct LDReg_s *nprm, unsigned int avg_update_en,
			       unsigned int matrix_update_en);
void ldim_hw_remap_update_tm2(struct LDReg_s *nprm, unsigned int avg_update_en,
			      unsigned int matrix_update_en);

/*==============debug=================*/
void ldim_remap_ctrl(unsigned char status);
void ldim_func_ctrl(unsigned char status);
void ldim_stts_initial(unsigned int pic_h, unsigned int pic_v,
		       unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_initial(unsigned int pic_h, unsigned int pic_v,
		  unsigned int blk_vnum, unsigned int blk_hnum,
		  unsigned int blk_mode, unsigned int ldim_bl_en,
		  unsigned int hvcnt_bypass);
void ldim_db_para_print(struct ldim_fw_para_s *fw_para);
int aml_ldim_debug_probe(struct class *ldim_class);
void aml_ldim_debug_remove(struct class *ldim_class);

#endif
