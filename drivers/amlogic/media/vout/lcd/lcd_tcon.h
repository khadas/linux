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
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>

#define REG_LCD_TCON_MAX    0xffff
#define TCON_INTR_MASKN_VAL    0x0  /* default mask all */

struct lcd_tcon_config_s {
	unsigned char tcon_valid;

	unsigned int core_reg_ver;
	unsigned int core_reg_width;
	unsigned int reg_table_width;
	unsigned int reg_table_len;
	unsigned int core_reg_start;
	unsigned int top_reg_base;

	unsigned int reg_top_ctrl;
	unsigned int bit_en;

	unsigned int reg_core_od;
	unsigned int bit_od_en;

	unsigned int reg_ctrl_timing_base;
	unsigned int ctrl_timing_offset;
	unsigned int ctrl_timing_cnt;

	unsigned int axi_bank;

	unsigned int rsv_mem_size;
	unsigned int axi_mem_size;
	unsigned int bin_path_size;
	unsigned int secure_handle_size;
	unsigned int vac_size;
	unsigned int demura_set_size;
	unsigned int demura_lut_size;
	unsigned int acc_lut_size;

	unsigned int *axi_reg;
	void (*tcon_axi_mem_config)(void);
	void (*tcon_axi_mem_secure)(void);
	void (*tcon_global_reset)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_gamma_pattern)(struct aml_lcd_drv_s *pdrv,
				  unsigned int bit_width, unsigned int gamma_r,
				  unsigned int gamma_g, unsigned int gamma_b);
	int (*tcon_reload)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_enable)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_disable)(struct aml_lcd_drv_s *pdrv);
};

struct tcon_rmem_config_s {
	phys_addr_t mem_paddr;
	unsigned char *mem_vaddr;
	unsigned int mem_size;
};

struct tcon_rmem_s {
	unsigned char flag;
	unsigned int rsv_mem_size;
	unsigned int axi_mem_size;

	void *rsv_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	phys_addr_t axi_mem_paddr;

	struct tcon_rmem_config_s *axi_rmem;
	struct tcon_rmem_config_s bin_path_rmem;
	struct tcon_rmem_config_s secure_handle_rmem;

	struct tcon_rmem_config_s vac_rmem;
	struct tcon_rmem_config_s demura_set_rmem;
	struct tcon_rmem_config_s demura_lut_rmem;
	struct tcon_rmem_config_s acc_lut_rmem;
};

struct tcon_data_list_s {
	unsigned int id;
	char *block_name;
	unsigned char *block_vaddr;
	struct tcon_data_list_s *next;
};

struct tcon_data_multi_s {
	unsigned int block_type;
	unsigned int list_cnt;
	unsigned int bypass_flag;
	struct tcon_data_list_s *list_header;
	struct tcon_data_list_s *list_cur;
	struct tcon_data_list_s *list_dft;
	struct tcon_data_list_s *list_remove;
};

struct tcon_data_init_s {
	unsigned int block_type;
	unsigned int priority;
	unsigned int flag;
	struct tcon_data_list_s *list_header;
};

struct tcon_data_priority_s {
	unsigned int index;
	unsigned int priority;
};

struct tcon_mem_map_table_s {
	/*header*/
	unsigned int version;
	unsigned char tcon_data_flag;
	unsigned int data_load_level;
	unsigned int block_cnt;
	unsigned char init_load;

	unsigned int valid_flag;
	unsigned char demura_cnt;
	unsigned int block_bit_flag;

	unsigned int core_reg_table_size;
	struct lcd_tcon_init_block_header_s *core_reg_header;
	unsigned char *core_reg_table;

	struct tcon_data_priority_s *data_priority;
	unsigned int *data_size;
	unsigned char **data_mem_vaddr;

	unsigned int multi_lut_update;
	unsigned int data_multi_cnt;
	struct tcon_data_multi_s *data_multi;

	struct tcon_data_init_s *data_init;
};

struct tcon_mem_secure_config_s {
	unsigned int handle;
	bool protect;
};

#define TCON_BIN_VER_LEN    9
#define MEM_FLAG_MAX	    2
struct lcd_tcon_local_cfg_s {
	struct tcon_mem_secure_config_s *secure_cfg;
	char bin_ver[TCON_BIN_VER_LEN];
	spinlock_t multi_list_lock; /* for tcon multi lut list change */
};

#ifdef CONFIG_AMLOGIC_TEE
int lcd_tcon_mem_tee_get_status(void);
int lcd_tcon_mem_tee_protect(int mem_flag, int protect_en);
#endif

struct lcd_tcon_config_s *get_lcd_tcon_config(void);
struct tcon_rmem_s *get_lcd_tcon_rmem(void);
struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void);
struct lcd_tcon_local_cfg_s *get_lcd_tcon_local_cfg(void);

/* **********************************
 * tcon config
 * **********************************
 */
/* common */
#define TCON_VAC_SET_PARAM_NUM		 3
#define TCON_VAC_LUT_PARAM_NUM		 256

/* TL1 */
#define LCD_TCON_CORE_REG_WIDTH_TL1      8
#define LCD_TCON_TABLE_WIDTH_TL1         8
#define LCD_TCON_TABLE_LEN_TL1           24000
#define LCD_TCON_AXI_BANK_TL1            3

#define BIT_TOP_EN_TL1                   4

#define TCON_CORE_REG_START_TL1          0x0000
#define REG_CORE_OD_TL1                  0x247
#define BIT_OD_EN_TL1                    0
#define REG_CORE_CTRL_TIMING_BASE_TL1    0x1b
#define CTRL_TIMING_OFFSET_TL1           12
#define CTRL_TIMING_CNT_TL1              0

/* T5 */
#define LCD_TCON_CORE_REG_WIDTH_T5       32
#define LCD_TCON_TABLE_WIDTH_T5          32
#define LCD_TCON_TABLE_LEN_T5            0x18d4 /* 0x635*4 */
#define LCD_TCON_AXI_BANK_T5             2

#define BIT_TOP_EN_T5                    4

#define TCON_CORE_REG_START_T5           0x0100
#define REG_CORE_OD_T5                   0x263
#define BIT_OD_EN_T5                     31
#define REG_CORE_CTRL_TIMING_BASE_T5     0x1b
#define CTRL_TIMING_OFFSET_T5            12
#define CTRL_TIMING_CNT_T5               0

/* T5D */
#define LCD_TCON_CORE_REG_WIDTH_T5D       32
#define LCD_TCON_TABLE_WIDTH_T5D          32
#define LCD_TCON_TABLE_LEN_T5D            0x102c /* 0x40b*4 */
#define LCD_TCON_AXI_BANK_T5D             1

#define BIT_TOP_EN_T5D                    4

#define TCON_CORE_REG_START_T5D           0x0100
#define REG_CORE_OD_T5D                   0x263
#define BIT_OD_EN_T5D                     31
#define REG_CTRL_TIMING_BASE_T5D          0x1b
#define CTRL_TIMING_OFFSET_T5D            12
#define CTRL_TIMING_CNT_T5D               0

/* **********************************
 * tcon api
 * **********************************
 */
/* internal */
int lcd_tcon_valid_check(void);
struct tcon_rmem_s *get_lcd_tcon_rmem(void);
struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void);
void lcd_tcon_global_reset_t5(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_global_reset_t3(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_gamma_pattern_tl1(struct aml_lcd_drv_s *pdrv,
			       unsigned int bit_width, unsigned int gamma_r,
			       unsigned int gamma_g, unsigned int gamma_b);
int lcd_tcon_gamma_pattern_t5(struct aml_lcd_drv_s *pdrv,
			      unsigned int bit_width, unsigned int gamma_r,
			      unsigned int gamma_g, unsigned int gamma_b);
void lcd_tcon_core_reg_set(struct aml_lcd_drv_s *pdrv,
			   struct lcd_tcon_config_s *tcon_conf,
			   struct tcon_mem_map_table_s *mm_table,
			   unsigned char *core_reg_table);
int lcd_tcon_enable_tl1(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_disable_tl1(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_enable_t5(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_disable_t5(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_enable_t3(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_disable_t3(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload_t3(struct aml_lcd_drv_s *pdrv);

/* common */
int lcd_tcon_data_multi_match_find(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf);
void lcd_tcon_data_multi_current_update(struct tcon_mem_map_table_s *mm_table,
		struct lcd_tcon_data_block_header_s *block_header, unsigned int index);
void lcd_tcon_data_multi_bypass_set(struct tcon_mem_map_table_s *mm_table,
				    unsigned int block_type, int flag);
int lcd_tcon_data_common_parse_set(struct aml_lcd_drv_s *pdrv,
				   unsigned char *data_buf, int init_flag);
void lcd_tcon_init_data_version_update(char *data_buf);
int lcd_tcon_data_load(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf, int index);
int lcd_tcon_bin_load(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_reg_table_print(void);
void lcd_tcon_reg_readback_print(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_multi_lut_print(void);
int lcd_tcon_info_print(char *buf, int offset);
void lcd_tcon_axi_rmem_lut_load(unsigned int index, unsigned char *buf,
				unsigned int size);

#endif
