/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_AML_LCD_TCON_FW_H
#define _INC_AML_LCD_TCON_FW_H
#include <linux/types.h>
#include <linux/stddef.h>

#define TCON_FW_PARA_VER            2

enum tcon_fw_chip_e {
	TCON_CHIP_TL1 = 0,
	TCON_CHIP_TM2,   /* 1 */
	TCON_CHIP_T5,    /* 2 */
	TCON_CHIP_T5D,   /* 3 */
	TCON_CHIP_T3,	 /* 4 */
	TCON_CHIP_T5W,	 /* 5 */
	TCON_CHIP_T5M,	 /* 6 */
	TCON_CHIP_T3X,	 /* 7 */
	TCON_CHIP_TXHD2, /* 8 */
	TCON_CHIP_MAX,
};

enum tcon_fw_if_type_e {
	TCON_IF_MLVDS = 0,
	TCON_IF_P2P,
	TCON_IF_TYPE_MAX,
};

struct tcon_fw_axi_rmem_s {
	phys_addr_t mem_paddr;
	unsigned int mem_size;
};

struct tcon_fw_config_s {
	unsigned int config_size;
	unsigned int chip_type;
	unsigned int if_type;
	unsigned char axi_cnt;
	struct tcon_fw_axi_rmem_s *axi_rmem;
};

struct tcon_fw_base_timing_s {
	unsigned int update_flag;

	unsigned int hsize;
	unsigned int vsize;
	unsigned int htotal;
	unsigned int vtotal;
	unsigned int frame_rate;
	unsigned int pclk;
	unsigned int bit_rate;

	unsigned int de_hstart;
	unsigned int de_hend;
	unsigned int de_vstart;
	unsigned int de_vend;
	unsigned int pre_de_h;
	unsigned int pre_de_v;

	unsigned short hsw;
	unsigned short hbp;
	unsigned short hfp;
	unsigned short vsw;
	unsigned short vbp;
	unsigned short vfp;
};

/* buf_table:
 * ....top_header
 * ........crc32: only include top_header+index
 * ........data_size: only include top_header+index
 * ....index([unsigned long], buf0~n addr pointer)
 * ....buf[0](header+data)
 * ........crc32: only buf[0] header+data
 * ........data_size: only buf[0] header+data
 * ............
 * ....buf[n]
 */
#define TCON_FW_DATA_HEADER_SIZE         64
struct tcon_fw_data_header_s {
	unsigned int crc32;
	unsigned int data_size;
	unsigned char block_cnt;
	unsigned char reserved[55];
};

#define TCON_FW_STATE_TCON_EN         0x00000001
#define TCON_FW_STATE_VAC_VALID       0x00000100
#define TCON_FW_STATE_DEMURA_VALID    0x00000200
#define TCON_FW_STATE_DITHER_VALID    0x00000400
#define TCON_FW_STATE_ACC_VALID       0x00001000
#define TCON_FW_STATE_PRE_ACC_VALID   0x00002000
#define TCON_FW_STATE_OD_VALID        0x00010000
#define TCON_FW_STATE_LOD_VALID       0x00100000

struct lcd_tcon_fw_s {
	/* init by driver */
	unsigned int para_ver;
	unsigned int para_size;
	unsigned char valid;
	unsigned int flag; //for some state update
	unsigned int tcon_state;

	/* init by fw */
	char fw_ver[20];
	unsigned char fw_ready;
	unsigned int dbg_level;

	/* driver resource */
	void *drvdat;
	struct device *dev;
	struct tcon_fw_config_s *config;
	struct tcon_fw_base_timing_s *base_timing;

	/* fw resource */
	unsigned char *buf_table_in;
	unsigned char *buf_table_out;

	/* API by driver */
	unsigned int (*reg_read)(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
			unsigned int reg);
	void (*reg_write)(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
			unsigned int reg, unsigned int value);
	void (*reg_setb)(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
			unsigned int reg, unsigned int value,
			unsigned int start, unsigned int len);
	unsigned int (*reg_getb)(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
			unsigned int reg, unsigned int start, unsigned int len);
	void (*reg_update_bits)(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
			unsigned int reg, unsigned int mask, unsigned int value);
	int (*reg_check_bits)(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
			unsigned int reg, unsigned int mask, unsigned int value);

	unsigned int (*get_encl_line_cnt)(struct lcd_tcon_fw_s *fw);
	unsigned int (*get_encl_frm_cnt)(struct lcd_tcon_fw_s *fw);

	/* API by fw */
	int (*vsync_isr)(struct lcd_tcon_fw_s *fw);
	int (*tcon_alg)(struct lcd_tcon_fw_s *fw, unsigned int flag);
	ssize_t (*debug_show)(struct lcd_tcon_fw_s *fw, char *buf);
	ssize_t (*debug_store)(struct lcd_tcon_fw_s *fw, const char *buf, size_t count);
};

struct lcd_tcon_fw_s *aml_lcd_tcon_get_fw(void);

#endif /* _INC_AML_LCD_TCON_FW_H */
