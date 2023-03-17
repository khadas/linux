/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_AML_LCD_TCON_FW_H
#define _INC_AML_LCD_TCON_FW_H

#define TCON_FW_PARA_VER            1

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

#define TCON_FW_DATA_HEADER_SIZE         32
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
	unsigned int tcon_state;

	/* init by fw */
	char fw_ver[20];
	unsigned char flag;
	unsigned int dbg_level;

	/* driver resource */
	void *drvdat;
	struct device *dev;
	struct tcon_fw_config_s *config;

	/* fw resource */
	unsigned char *buf_table_in;
	unsigned char *buf_table_out;

	/* API */
	int (*vsync_isr)(struct lcd_tcon_fw_s *fw);
	int (*tcon_alg)(struct lcd_tcon_fw_s *fw, unsigned int flag);
	ssize_t (*debug_show)(struct lcd_tcon_fw_s *fw, char *buf);
	int (*debug_store)(struct lcd_tcon_fw_s *fw, const char *buf);
};

struct lcd_tcon_fw_s *aml_lcd_tcon_get_fw(void);

#endif /* _INC_AML_LCD_TCON_FW_H */
