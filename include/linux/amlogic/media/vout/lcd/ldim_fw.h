/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LDIM_ALG_H_
#define _INC_AML_LDIM_ALG_H_

#define LD_BLKREGNUM     1536  /* maximum support 48*32*/

enum ldc_dbg_type_e {
	LDC_DBG_ATTR		= 0x01,
	LDC_DBG_MEM			= 0x02,
	LDC_DBG_REG			= 0x03,
	LDC_DBG_DBGREG		= 0x04,
	LDC_DBG_DEBUG		= 0x05,
	LDC_DBG_PARA		= 0x06,
};

struct ldim_seg_hist_s {
	unsigned int weight_avg;
	unsigned int weight_avg_95;
	unsigned int max_index;
	unsigned int min_index;
};

struct ldim_stts_s {
	unsigned int *global_hist;
	unsigned int global_hist_sum;
	unsigned int global_hist_cnt;
	unsigned int global_apl;
	struct ldim_seg_hist_s *seg_hist;
};

struct ldim_profile_s {
	unsigned int mode;
	unsigned int profile_k;
	unsigned int profile_bits;
	char file_path[256];
};

struct ldim_rmem_s {
	unsigned char flag; //0:none, 1:ldc_cma, 2:sys_cma_pool, 3:kmalloc

	void *rsv_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	unsigned int rsv_mem_size;
};

struct ldim_fw_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char level_index;
};

struct ldim_fw_s {
	/* header */
	unsigned int para_ver;
	unsigned int para_size;
	char alg_ver[20];
	unsigned char chip_type;
	unsigned char fw_sel;
	unsigned char valid;
	unsigned char flag;

	unsigned char seg_col;
	unsigned char seg_row;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char res_update;
	unsigned char level_index;
	unsigned int fw_ctrl;
	unsigned int fw_state;

	unsigned int bl_matrix_dbg;
	unsigned char fw_hist_print;
	unsigned int fw_print_frequent;
	unsigned int fw_print_lv;

	struct ldim_fw_config_s *conf;
	struct ldim_rmem_s *rmem;
	struct ldim_stts_s *stts;
	struct ldim_profile_s *profile;

	unsigned int *bl_matrix;

	void (*fw_alg_frm)(struct ldim_fw_s *fw);
	void (*fw_alg_para_print)(struct ldim_fw_s *fw);
	void (*fw_init)(struct ldim_fw_s *fw);
	void (*fw_info_update)(struct ldim_fw_s *fw);
	void (*fw_pq_set)(unsigned char *buf, unsigned int len);
	void (*fw_profile_set)(unsigned char *buf, unsigned int len);
	void (*fw_rmem_duty_get)(struct ldim_fw_s *fw);
	void (*fw_rmem_duty_set)(unsigned int *bl_matrix);
	ssize_t (*fw_debug_show)(struct ldim_fw_s *fw,
		enum ldc_dbg_type_e type, char *buf);
	ssize_t (*fw_debug_store)(struct ldim_fw_s *fw,
		enum ldc_dbg_type_e type, char *buf, size_t len);
};

struct ldim_fw_custom_s {
	/* header */
	unsigned char valid;/*if ld firmware is ready*/

	unsigned char seg_col;/*segment column number*/
	unsigned char seg_row;/*segment row number*/
	unsigned int global_hist_bin_num;/*global hist bin number*/

	/*print frequent for debug,controlled by cmd*/
	unsigned int fw_print_frequent;
	/*print levle for debug,controlled by cmd, range at 200 - 300*/
	unsigned int fw_print_lv;

	unsigned int *param; /*custom fw parameters*/
	unsigned int *bl_matrix;/*backlight matrix output*/

	/* Soc firmware control level output
	 * bit0: Temporal filter
	 * bit1: Backlight remapping
	 * bit2: Spatial filter
	 * bit3: Backlight matrix calculation
	 * bit4~bit7: reserved
	 */
	unsigned int ctrl_level;

	/*function for backlight matrix algorithm*/
	void (*fw_alg_frm)(struct ldim_fw_custom_s *fw_cus,
		struct ldim_stts_s *stts);
	/*function for debug,controlled by cmd*/
	void (*fw_alg_para_print)(struct ldim_fw_custom_s *fw_cus);
};

/* if struct ldim_fw_s changed, FW_PARA_VER must be update */
/*20221118 version 1*/
#define FW_PARA_VER    1

struct ldim_fw_s *aml_ldim_get_fw(void);
struct ldim_fw_custom_s *aml_ldim_get_fw_cus(void);

#endif
