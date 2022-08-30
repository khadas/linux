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
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>

/*20200215: init version */
/*20210201: fix compiler mistake */
/*20210602: support t7/t3 new ldc */
/*20210730: basic function run ok */
/*20210806: add fw support */
/*20220815: correct ldim spi dma config */
#define LDIM_DRV_VER    "20220815"

#define LDIM_SPI_DUTY_VSYNC_DIRECT

extern unsigned char ldim_debug_print;

extern int ld_remap_lut[16][32];
extern unsigned int ldc_gain_lut_array[16][64];
extern unsigned int ldc_min_gain_lut[64];
extern unsigned int ldc_dither_lut[32][16];

#define AML_LDIM_MODULE_NAME "aml_ldim"
#define AML_LDIM_DRIVER_NAME "aml_ldim"
#define AML_LDIM_DEVICE_NAME "aml_ldim"
#define AML_LDIM_CLASS_NAME  "aml_ldim"

/* new ldc buf memory data mapping */
#define LDC_PROFILE_OFFSET      0x00000
#define LDC_GLOBAL_HIST_OFFSET  0xc0000
#define LDC_SEG_HIST0_OFFSET    0xc0100
#define LDC_SEG_HIST1_OFFSET    0xc2500
#define LDC_SEG_DUTY0_OFFSET    0xc4900
#define LDC_SEG_DUTY1_OFFSET    0xc5300
#define LDC_SEG_DUTY2_OFFSET    0xc5d00
#define LDC_SEG_DUTY3_OFFSET    0xc6700
#define LDC_MEM_END             0xc7100
/*========================================*/

/* ldim func */
int ldim_round(int ix, int ib);
void ld_func_cfg_ldreg(struct ld_reg_s *reg);
void ld_func_fw_cfg_once(struct ld_reg_s *nprm);

/* ldim hw */
#define LDIM_VPU_DMA_WR    0
#define LDIM_VPU_DMA_RD    1

void ldim_hw_vpu_dma_mif_en(int rw_sel, int flag);
void ldim_hw_vpu_dma_mif_en_tm2b(int rw_sel, int flag);
void ldim_hw_remap_en(struct aml_ldim_driver_s *ldim_drv, int flag);
void ldim_hw_remap_demo_en(int flag);
int ldim_hw_reg_dump(char *buf);
int ldim_hw_reg_dump_tm2(char *buf);
int ldim_hw_reg_dump_tm2b(char *buf);
void ldim_hw_stts_read_zone(unsigned int nrow, unsigned int ncol);
void ldim_func_profile_update(struct ldim_fw_s *fw,
			      struct ldim_profile_s *profile);

void ldim_hw_remap_init_tm2(struct ld_reg_s *nprm, unsigned int ldim_bl_en,
			    unsigned int ldim_hvcnt_bypass);
void ldim_hw_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
			      unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_stts_initial_tm2(unsigned int pic_h, unsigned int pic_v,
			      unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_remap_update_tm2(struct ld_reg_s *nprm, unsigned int avg_update_en,
			      unsigned int matrix_update_en);
void ldim_hw_remap_update_tm2b(struct ld_reg_s *nprm, unsigned int avg_update_en,
			       unsigned int matrix_update_en);

/*new ldc*/
void ldc_gain_lut_set_t7(void);
void ldc_gain_lut_set_t3(void);
void ldc_gain_lut_get_t3(void);
void ldc_min_gain_lut_set(void);
void ldc_dither_lut_set(void);
void ldim_hw_remap_en_t7(struct aml_ldim_driver_s *ldim_drv, int flag);
void ldim_config_update_t7(struct aml_ldim_driver_s *ldim_drv);
void ldim_vs_arithmetic_t7(struct aml_ldim_driver_s *ldim_drv);
void ldim_func_ctrl_t7(struct aml_ldim_driver_s *ldim_drv, int flag);
void ldim_drv_init_t7(struct aml_ldim_driver_s *ldim_drv);
void ldim_drv_init_t3(struct aml_ldim_driver_s *ldim_drv);
void ldc_set_t7(struct aml_ldim_driver_s *ldim_drv);

/*ldim mem*/
void ldc_mem_dump(unsigned char *vaddr, unsigned int size);
void ldc_mem_save(char *path, unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_write(char *path, unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_clear(unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_set(unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_write_profile(unsigned char *buf, unsigned long mem_paddr, unsigned int size);

/*==============debug=================*/
void ldim_remap_ctrl(unsigned char status);
int aml_ldim_debug_probe(struct class *ldim_class);
void aml_ldim_debug_remove(struct class *ldim_class);

#endif
