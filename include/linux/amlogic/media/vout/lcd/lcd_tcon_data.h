/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_AML_LCD_TCON_DATA_H
#define _INC_AML_LCD_TCON_DATA_H

/* for data block header flag */
#define LCD_TCON_DATA_VALID_VAC      BIT(0)
#define LCD_TCON_DATA_VALID_DEMURA   BIT(1)
#define LCD_TCON_DATA_VALID_ACC      BIT(2)
#define LCD_TCON_DATA_VALID_DITHER   BIT(3)
#define LCD_TCON_DATA_VALID_OD       BIT(4)
#define LCD_TCON_DATA_VALID_LOD      BIT(5)

/* for tconless data format */
/* for tconless data block type */
#define LCD_TCON_DATA_BLOCK_TYPE_BASIC_INIT     0x00 /* for basic init setting */
#define LCD_TCON_DATA_BLOCK_TYPE_DEMURA_SET     0x01
#define LCD_TCON_DATA_BLOCK_TYPE_DEMURA_LUT     0x02
#define LCD_TCON_DATA_BLOCK_TYPE_ACC_LUT        0x03
#define LCD_TCON_DATA_BLOCK_TYPE_DITHER_LUT     0x04
#define LCD_TCON_DATA_BLOCK_TYPE_OD_LUT         0x05
#define LCD_TCON_DATA_BLOCK_TYPE_LOD_LUT        0x06
#define LCD_TCON_DATA_BLOCK_TYPE_VAC            0x11
#define LCD_TCON_DATA_BLOCK_TYPE_EXT            0xe0 /* pmu */
#define LCD_TCON_DATA_BLOCK_TYPE_MAX            0xff

/* for tconless data block part type */
#define LCD_TCON_DATA_PART_TYPE_WR_N            0xd0
#define LCD_TCON_DATA_PART_TYPE_WR_DDR          0xdd
#define LCD_TCON_DATA_PART_TYPE_WR_MASK         0xb0
#define LCD_TCON_DATA_PART_TYPE_RD_MASK         0xab
#define LCD_TCON_DATA_PART_TYPE_CHK_WR_MASK     0xcb
#define LCD_TCON_DATA_PART_TYPE_CHK_EXIT        0xce
#define LCD_TCON_DATA_PART_TYPE_PARAM           0xf0 /* only for tool */
#define LCD_TCON_DATA_PART_TYPE_CONTROL         0xfc
#define LCD_TCON_DATA_PART_TYPE_DELAY           0xfd

#define LCD_TCON_DATA_PART_FLAG_TUNINTG_LUT     0x00
#define LCD_TCON_DATA_PART_FLAG_FIXED_LUT       0x10
#define LCD_TCON_DATA_PART_FLAG_TUNINTG_REG     0x01
#define LCD_TCON_DATA_PART_FLAG_FIXED_REG       0x11
#define LCD_TCON_DATA_PART_FLAG_TUNINTG_PARAM   0x0f
#define LCD_TCON_DATA_PART_FLAG_FIXED_PARAM     0x1f
#define LCD_TCON_DATA_PART_FLAG_CMD_IGNORE_ISR  0x80

#define LCD_TCON_DATA_BLOCK_HEADER_SIZE         64
#define LCD_TCON_DATA_BLOCK_NAME_SIZE           36
#define LCD_TCON_DATA_PART_NAME_SIZE            48
#define LCD_TCON_INIT_BIN_NAME_SIZE             28
#define LCD_TCON_INIT_BIN_VERSION_SIZE          8

/* tcon data control defaine */
/* block_ctrl */
#define LCD_TCON_DATA_CTRL_FLAG_MULTI           0x01
#define LCD_TCON_DATA_CTRL_FLAG_DLG             0xd0
/* ctrl_method */
#define LCD_TCON_DATA_CTRL_DEFAULT              0x00
#define LCD_TCON_DATA_CTRL_MULTI_VFREQ_DIRECT   0x01
#define LCD_TCON_DATA_CTRL_MULTI_VFREQ_NOTIFY   0x02
#define LCD_TCON_DATA_CTRL_MULTI_BL_LEVEL       0x11
#define LCD_TCON_DATA_CTRL_MULTI_BL_PWM_DUTY    0x12
#define LCD_TCON_DATA_CTRL_MULTI_MAX            0xff

struct lcd_tcon_init_block_header_s {
	unsigned int crc32;
	unsigned short h_active;
	unsigned short v_active;
	unsigned int block_size;
	unsigned short header_size;
	unsigned short reserved1;
	unsigned short block_type;
	unsigned short block_ctrl;
	unsigned char reserved2[5];
	unsigned char data_byte_width;
	unsigned short chipid;
	unsigned char name[LCD_TCON_INIT_BIN_NAME_SIZE];
	char version[LCD_TCON_INIT_BIN_VERSION_SIZE];
};

struct lcd_tcon_data_block_header_s {
	unsigned int crc32;
	unsigned int raw_data_check;/* crc */
	unsigned int block_size;
	unsigned short header_size;
	unsigned short ext_header_size;
	unsigned short block_type;
	unsigned short block_ctrl;
	unsigned int block_flag;
	unsigned short init_priority;
	unsigned short chipid;
	unsigned char name[LCD_TCON_DATA_BLOCK_NAME_SIZE];
};

struct lcd_tcon_data_block_ext_header_s {
	unsigned short part_cnt;
	unsigned char part_mapping_byte;
	unsigned char reserved[13];
};

#define LCD_TCON_DATA_PART_CTRL_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 12)
struct lcd_tcon_data_part_ctrl_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned short ctrl_data_flag;
	unsigned short ctrl_method;
	unsigned short data_byte_width;
	unsigned short data_cnt;
};

#define LCD_TCON_DATA_PART_WR_N_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 12)
struct lcd_tcon_data_part_wr_n_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned char reg_addr_byte;
	unsigned char reg_data_byte;
	unsigned char reg_inc;
	unsigned char reg_cnt;
	unsigned int data_cnt;
};

#define LCD_TCON_DATA_PART_WR_DDR_SIZE_PRE  (LCD_TCON_DATA_PART_NAME_SIZE + 12)
struct lcd_tcon_data_part_wr_ddr_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned short axi_buf_id;
	unsigned short data_byte;
	unsigned int data_cnt;
};

#define LCD_TCON_DATA_PART_WR_MASK_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 6)
struct lcd_tcon_data_part_wr_mask_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned char reg_addr_byte;
	unsigned char reg_data_byte;
};

#define LCD_TCON_DATA_PART_RD_MASK_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 6)
struct lcd_tcon_data_part_rd_mask_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned char reg_addr_byte;
	unsigned char reg_data_byte;
};

#define LCD_TCON_DATA_PART_CHK_WR_MASK_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 9)
struct lcd_tcon_data_part_chk_wr_mask_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned char reg_chk_addr_byte;
	unsigned char reg_chk_data_byte;
	unsigned char reg_wr_addr_byte;
	unsigned char reg_wr_data_byte;
	unsigned char data_chk_cnt;
};

#define LCD_TCON_DATA_PART_CHK_EXIT_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 6)
struct lcd_tcon_data_part_chk_exit_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned char reg_addr_byte;
	unsigned char reg_data_byte;
};

#define LCD_TCON_DATA_PART_DELAY_SIZE    (LCD_TCON_DATA_PART_NAME_SIZE + 8)
struct lcd_tcon_data_part_delay_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned int delay_us;
};

#define LCD_TCON_DATA_PART_PARAM_SIZE_PRE    (LCD_TCON_DATA_PART_NAME_SIZE + 8)
struct lcd_tcon_data_part_param_s {
	char name[LCD_TCON_DATA_PART_NAME_SIZE];
	unsigned short part_id;
	unsigned char tuning_flag;
	unsigned char part_type;
	unsigned int param_size;
};

union lcd_tcon_data_part_u {
	struct lcd_tcon_data_part_ctrl_s *ctrl;
	struct lcd_tcon_data_part_wr_n_s *wr_n;
	struct lcd_tcon_data_part_wr_ddr_s *wr_ddr;
	struct lcd_tcon_data_part_wr_mask_s *wr_mask;
	struct lcd_tcon_data_part_rd_mask_s *rd_mask;
	struct lcd_tcon_data_part_chk_wr_mask_s *chk_wr_mask;
	struct lcd_tcon_data_part_chk_exit_s *chk_exit;
	struct lcd_tcon_data_part_delay_s *delay;
	struct lcd_tcon_data_part_param_s *param;
};

#define LCD_UKEY_TCON_SPI_BLOCK_SIZE_PRE          20
struct lcd_tcon_spi_block_s {
	unsigned short data_type;
	unsigned short data_index; /* tcon_data_index, equal to init priority */
	unsigned int data_flag;
	unsigned int spi_offset;
	unsigned int spi_size;
	unsigned int param_cnt;

	unsigned int data_raw_check; //crc...
	unsigned int data_temp_size;
	unsigned int data_new_size;

	unsigned int *param;
	unsigned char *raw_buf;
	unsigned char *temp_buf;
	unsigned char *new_buf;
};

struct lcd_tcon_spi_s {
	unsigned short version;
	unsigned int block_cnt;
	unsigned int init_flag;
	struct lcd_tcon_spi_block_s **spi_block;

	unsigned char *ext_buf;
	unsigned int ext_init_on_cnt;
	unsigned int ext_init_off_cnt;

	int (*data_read)(struct lcd_tcon_spi_block_s *spi_block);
	int (*data_conv)(struct lcd_tcon_spi_block_s *spi_block);
};

struct lcd_tcon_spi_s *lcd_tcon_spi_get(void);

unsigned int lcd_tcon_data_size_align(unsigned int size);
unsigned char lcd_tcon_checksum(unsigned char *buf, unsigned int len);
unsigned char lcd_tcon_lrc(unsigned char *buf, unsigned int len);

#endif /* _INC_AML_LCD_TCON_DATA_H */
