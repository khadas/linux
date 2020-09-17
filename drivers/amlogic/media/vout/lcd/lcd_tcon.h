/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_TCON_H__
#define __AML_LCD_TCON_H__
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#define REG_LCD_TCON_MAX    0xffff

struct lcd_tcon_config_s {
	unsigned char tcon_valid;

	unsigned int core_reg_width;
	unsigned int reg_table_len;

	unsigned int reg_top_ctrl;
	unsigned int bit_en;

	unsigned int reg_core_od;
	unsigned int bit_od_en;

	unsigned int reg_core_ctrl_timing_base;
	unsigned int ctrl_timing_offset;
	unsigned int ctrl_timing_cnt;

	unsigned int rsv_size;
	unsigned int axi_size;
	unsigned int bin_path_size;
	unsigned int vac_size;
	unsigned int demura_set_size;
	unsigned int demura_lut_size;
	unsigned int acc_lut_size;
	unsigned char *reg_table;

	int (*tcon_gamma_pattern)(unsigned int bit_width, unsigned int gamma_r,
				  unsigned int gamma_g, unsigned int gamma_b);
	int (*tcon_enable)(struct lcd_config_s *pconf);
};

struct tcon_rmem_s {
	unsigned char flag;
	void *rsv_mem_vaddr;
	unsigned char *axi_mem_vaddr;
	unsigned char *bin_path_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	phys_addr_t axi_mem_paddr;
	phys_addr_t bin_path_mem_paddr;
	phys_addr_t vac_mem_paddr;
	phys_addr_t demura_set_mem_paddr;
	phys_addr_t demura_lut_mem_paddr;
	phys_addr_t acc_lut_mem_paddr;

	unsigned int rsv_mem_size;
	unsigned int axi_mem_size;
	unsigned int bin_path_mem_size;
};

struct tcon_data_priority_s {
	unsigned int index;
	unsigned int priority;
};

struct tcon_mem_map_table_s {
	unsigned int version;
	unsigned char tcon_data_flag;
	unsigned int data_load_level;
	unsigned int block_cnt;
	unsigned int valid_flag;

	unsigned int vac_mem_size;
	unsigned int demura_set_mem_size;
	unsigned int demura_lut_mem_size;
	unsigned int acc_lut_mem_size;

	struct tcon_data_priority_s *data_priority;
	unsigned char *vac_mem_vaddr;
	unsigned char *demura_set_mem_vaddr;
	unsigned char *demura_lut_mem_vaddr;
	unsigned char *acc_lut_mem_vaddr;
	unsigned char **data_mem_vaddr;
};

struct tcon_rmem_s *get_lcd_tcon_rmem(void);
struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void);

/* **********************************
 * tcon config
 * **********************************
 */
/* TL1 */
#define LCD_TCON_CORE_REG_WIDTH_TL1      8
#define LCD_TCON_TABLE_LEN_TL1           24000
#define LCD_TCON_AXI_BANK_TL1            3

#define BIT_TOP_EN_TL1                   4

#define REG_CORE_OD_TL1                  0x247
#define BIT_OD_EN_TL1                    0
#define REG_CORE_CTRL_TIMING_BASE_TL1    0x1b
#define CTRL_TIMING_OFFSET_TL1           12
#define CTRL_TIMING_CNT_TL1              0
#define TCON_VAC_SET_PARAM_NUM		 3
#define TCON_VAC_LUT_PARAM_NUM		 256

#endif
