/*
 * drivers/amlogic/amlnf/include/phynand.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef PHYNAND_H_INCLUDED
#define PHYNAND_H_INCLUDED

#include "../include/amlnf_dev.h"

#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/io.h>

/* #define CONFIG_OF */

#define NAND_COMPATIBLE_REGION	1
#define NAND_RESERVED_REGION	1
#define NAND_ADDNEW_REGION	1
#define NAND_BUG_FIX_REGION	6

#define DRV_PHY_VERSION	 ((NAND_COMPATIBLE_REGION << 24) + \
			(NAND_RESERVED_REGION << 16) + \
			(NAND_ADDNEW_REGION << 8) + \
			(NAND_BUG_FIX_REGION))

#define NAND_MFR_USER          0x100
#define NAND_MFR_EFUSE         0x101

/* #define abs(value) (((value) < 0) ? ((value)*-1) : (value)) */
/*
 ** Max page list cnt for usrdef mode
*/

#define NAND_PAGELIST_CNT		16

/*
** nand read retry info, max equals to Zero, that means no need retry.
 */
struct nand_retry_t {
	unsigned id;
	unsigned max;
	unsigned no_rb;
};

struct nand_cmd_t {
	unsigned char type;
	unsigned char val;
};

/* read from page0, override default. */
struct nand_page0_info_t {
	unsigned nand_read_info;
	unsigned new_nand_type;
	unsigned pages_in_block;
	unsigned secure_block;
	unsigned ce_mask;
	unsigned boot_num;
	unsigned each_boot_pages;
	unsigned reserved[2];
};

/* read from page0, override default. */
struct nand_page0_cfg_t {
	/* 26:pagelist, 24:a2, 23:no_rb, 22:large. 21-0:cmd. */
	unsigned ext;
	short id;
	/* id:0x100 user, max:0 disable. */
	short max;
	unsigned char list[NAND_PAGELIST_CNT];
	struct nand_cmd_t retry_usr[32];
	struct nand_page0_info_t nand_page0_info;
};

union nand_core_clk_t {
	/* raw register data */
	uint d32;
	/*register bits */
	struct {
		unsigned clk_div:7;
		unsigned reserved0:1;
		unsigned clk_en:1;
		unsigned clk_sel:3;
		unsigned not_used:20;
	} b;
};
/**********************storage *********************/
#define STORAGE_DEV_NOSET	(0)
#define STORAGE_DEV_EMMC	(1)
#define STORAGE_DEV_NAND	(2)
#define STORAGE_DEV_SPI		(3)
#define STORAGE_DEV_SDCARD	(4)
#define STORAGE_DEV_USB		(5)

/***************ERROR CODING*******************/
#define NAND_CHIP_ID_ERR            1
#define NAND_SHIP_BAD_BLOCK_ERR     2
#define NAND_CHIP_REVB_HY_ERR       3

/*
 * NAND Flash Manufacturer ID Codes
 */
#define NAND_MFR_TOSHIBA	0x98
#define NAND_MFR_SAMSUNG	0xec
#define NAND_MFR_FUJITSU	0x04
#define NAND_MFR_NATIONAL	0x8f
#define NAND_MFR_RENESAS	0x07
#define NAND_MFR_STMICRO	0x20
#define NAND_MFR_HYNIX		0xad
#define NAND_MFR_MICRON		0x2c
#define NAND_MFR_AMD		0x01
#define NAND_MFR_INTEL		0x89
#define NAND_MFR_SANDISK	0x45


/***nand runing status***/
#define NAND_STATUS_NORMAL	0
#define NAND_STATUS_UNUSUAL	1


#define NAND_STATUS_MAX_TRY	6
#define	NAND_STATUS_FAILED	0xe1
#define	NAND_STATUS_RIGHT	0xe0

#define	BYTES_OF_USER_PER_PAGE	16

#define ADJUST_PART_SIZE	10
#define ADJUST_SIZE_NFTL	8

#define	SHIPPED_BBT_HEAD_MAGIC		"fbbt"
#define	BBT_HEAD_MAGIC			"nbbt"
#define	CONFIG_HEAD_MAGIC		"ncnf"
#define	HYNIX_DEV_HEAD_MAGIC		"nhix"
#define	KEY_INFO_HEAD_MAGIC		"nkey"
#define	SECURE_INFO_HEAD_MAGIC		"nsec"
#define	ENV_INFO_HEAD_MAGIC		"nenv"
#define PHY_PARTITION_HEAD_MAGIC	"phyp"
#define	DTD_INFO_HEAD_MAGIC		"ndtb"

#define	FBBT_COPY_NUM	1

#define CONFIG_SECURE_SIZE	(0x10000*2) /* 128k */
#define SECURE_SIZE (CONFIG_SECURE_SIZE - 2*(sizeof(uint)))

#define CONFIG_KEY_MAX_SIZE	0x40000

#define FULL_BLK	0
#define FULL_PAGE	1

#define	MAX_BAD_BLK_NUM			2048
#define	MAX_SHIPPED_BAD_BLK_NUM		512
#define	MAX_BLK_NUM			8192
#define	RESERVED_BLOCK_CNT		48

/* nand parameter for read retry, or fulture */
#define	NANS_PARA_BLOCK_CNT		1

/* for shipped bbt block, short mode, full block, never update */
/* common bbt table, rarely update */
#define	BBT_BLOCK_CNT			2
/* shipped bbt table, just only one copy first detect */
#define	SHIPPED_BBT_BLOCK_CNT		1
/* config block, short mode, full block, if update, erase whole block */
#define	CONFIG_BLOCK_CNT	1
#define	KEY_BLOCK_CNT		4

/***uboot code***/
#define	BOOT_COPY_NUM		4
#define	BOOT_PAGES_PER_COPY	256

#define	MAX_CYCLE_NUM		20

/***nand chip options***/
#define	NAND_CTRL_FORCE_WP		(1<<0)

/***nand controller options***/
#define	NAND_CTRL_NONE_RB		(1<<1)
#define	NAND_CTRL_INTERLEAVING_MODE	(1<<2)
#define	NAND_MULTI_PLANE_MODE		(1<<3)


/***nand controller ECC options***/
#define	MAX_ECC_MODE_NUM		16
#define	NAND_ECC_TYPE_MASK		(0xf<<4)

#define	NAND_ECC_SOFT_MODE		(0x0<<4)
#define	NAND_ECC_SHORT_MODE		(0x1<<4)
#define	NAND_ECC_BCH9_MODE		(0x2<<4)
#define	NAND_ECC_BCH8_MODE		(0x3<<4)
#define	NAND_ECC_BCH12_MODE		(0x4<<4)
#define	NAND_ECC_BCH16_MODE		(0x5<<4)
#define	NAND_ECC_BCH8_1K_MODE		(0x6<<4)
#define	NAND_ECC_BCH16_1K_MODE		(0x7<<4)
#define	NAND_ECC_BCH24_1K_MODE		(0x8<<4)
#define	NAND_ECC_BCH30_1K_MODE		(0x9<<4)
#define	NAND_ECC_BCH40_1K_MODE		(0xa<<4)
#define	NAND_ECC_BCH50_1K_MODE		(0xb<<4)
#define	NAND_ECC_BCH60_1K_MODE		(0xc<<4)


	/***FOR NAND CHIP TYPES ***/
#define	NAND_CHIP_TYPE_MASK		(0x3<<9)

#define	NAND_CHIP_TYPE_MLC		(0x0<<9)
#define	NAND_CHIP_TYPE_SLC		(0x1<<9)
#define	NAND_CHIP_TYPE_TLC		(0x2<<9)

#define	NAND_CHIP_AYSNC_MODE		(0x0<<12)
#define	NAND_CHIP_TOGGLE_MODE		(0x1<<12)
#define	NAND_CHIP_SLC_MODE		(0x1<<13)


	/***FOR TIMMING MODE ***/

#define	NAND_TIMING_MODE0		(0x0)
#define	NAND_TIMING_MODE1		(0x1)
#define	NAND_TIMING_MODE2		(0x2)
#define	NAND_TIMING_MODE3		(0x3)
#define	NAND_TIMING_MODE4		(0x4)
#define	NAND_TIMING_MODE5		(0x5)


#define	NAND_DEFAULT_OPTIONS	(NAND_CTRL_NONE_RB | NAND_ECC_SHORT_MODE)

#define	DEFAULT_T_REA		20
#define	DEFAULT_T_RHOH		25

/***FOR ECC ***/
#define	NAND_ECC_UNIT_SIZE		512
#define	NAND_ECC_UNIT_1KSIZE		1024
#define	NAND_ECC_UNIT_SHORT		384

#define	NAND_BCH9_ECC_SIZE		15
#define	NAND_BCH8_ECC_SIZE		14
#define	NAND_BCH12_ECC_SIZE		20
#define	NAND_BCH16_ECC_SIZE		26
#define	NAND_BCH8_1K_ECC_SIZE		14
#define	NAND_BCH16_1K_ECC_SIZE		28
#define	NAND_BCH24_1K_ECC_SIZE		42
#define	NAND_BCH30_1K_ECC_SIZE		54
#define	NAND_BCH40_1K_ECC_SIZE		70
#define	NAND_BCH50_1K_ECC_SIZE		88
#define	NAND_BCH60_1K_ECC_SIZE		106

/* 1uS , about 8mS */
#define	AML_NAND_READ_BUSY_TIMEOUT	0x2000
/* 1uS, about 20mS */
#define	AML_NAND_WRITE_BUSY_TIMEOUT	0x5000
/* 1uS, about 40mS */
#define	AML_NAND_ERASE_BUSY_TIMEOUT	0xa000


#define	AML_DMA_BUSY_TIMEOUT	0x100000

#define	MAX_ID_LEN		8

#define	NAND_TYPE_MLC		0
#define	NAND_TYPE_SLC		1
#define	NAND_TYPE_TLC		2

#define	NAND_TWB_TIME_CYCLE	10
#define	NAND_TWHR_TIME_CYCLE	20
#define	NAND_TADL_TIME_CYCLE	20
#define	NAND_TCCS_TIME_CYCLE	20
#define	NAND_TRHW_TIME_CYCLE	20


#ifdef AML_NAND_RB_IRQ
#define NAND_CHIP_UNDEFINE	200
#endif

#ifdef AML_NAND_RB_IRQ
#define DMA_TIME_CNT_220US	220000
#define DMA_TIME_CNT_20US	20000
#endif

/*
#define	NAND_TWHR2_TIME_CYCLE	20
*/

/*
 * Constants for hardware specific CLE/ALE/NCE function
 *
 * These are bits which can be or'ed to set/clear multiple
 * bits in one go.
 */
/* Select the chip by setting nCE to low */
#define	NAND_NCE	0x01
/* Select the command latch by setting CLE to high */
#define	NAND_CLE	0x02
/* Select the address latch by setting ALE to high */
#define	NAND_ALE	0x04


#define	NAND_CTRL_CLE	(NAND_NCE | NAND_CLE)
#define	NAND_CTRL_ALE	(NAND_NCE | NAND_ALE)

/*
 * Standard NAND flash commands
 */
#define	NAND_CMD_READ0		0x00
#define	NAND_CMD_READ1		0x01
#define	NAND_CMD_RNDOUT		0x05
#define	NAND_CMD_PAGEPROG	0x10
#define	NAND_CMD_READOOB	0x50
#define	NAND_CMD_ERASE1		0x60
#define	NAND_CMD_STATUS		0x70
#define	NAND_CMD_STATUS_MULTI	0x71
#define	NAND_CMD_SEQIN		0x80
#define	NAND_CMD_RNDIN		0x85
#define	NAND_CMD_READID		0x90
#define	NAND_CMD_ERASE2		0xd0
#define	NAND_CMD_PARAM		0xec
#define	NAND_CMD_RESET		0xff

#define	NAND_CMD_ID_ADDR_NORMAL	0x00
#define	NAND_CMD_ID_ADDR_ONFI	0x20


#define	NAND_CMD_NONE	-1


#define	NAND_CMD_LOCK		0x2a
#define	NAND_CMD_UNLOCK1	0x23
#define	NAND_CMD_UNLOCK2	0x24

/*
  *Extended common NAND CMD
  *
*/
#define	NAND_CMD_PLANE2_READ_START	0x06
#define	NAND_CMD_TWOPLANE_PREVIOS_READ	0x60
#define	NAND_CMD_TWOPLANE_READ1		0x5a
#define	NAND_CMD_TWOPLANE_READ2		0xa5
#define	NAND_CMD_TWOPLANE_WRITE2_MICRO	0x80
#define	NAND_CMD_TWOPLANE_WRITE2	0x81
#define	NAND_CMD_DUMMY_PROGRAM		0x11
#define	NAND_CMD_ERASE1_END		0xd1
#define	NAND_CMD_MULTI_CHIP_STATUS	0x78
#define	NAND_CMD_SET_FEATURES		0xEF
#define	NAND_CMD_GET_FEATURES		0xEE
#define	NAND_CMD_READSTART		0x30
#define	NAND_CMD_RNDOUTSTART		0xE0
#define	NAND_CMD_CACHEDPROG		0x15

#define	ONFI_TIMING_ADDR		0x01
#define	NAND_STATUS_READY_MULTI		0x20

/* Status bits */
#define	NAND_STATUS_FAIL	0x01
#define	NAND_STATUS_FAIL_N1	0x02
#define	NAND_STATUS_TRUE_READY	0x20
#define	NAND_STATUS_READY	0x40
#define	NAND_STATUS_WP		0x80

#if (AML_CFG_NEW_NAND_SUPPORT)
#define	RETRY_NAND_MAGIC	"refv"
#define	RETRY_NAND_BLK_NUM	2
#define	RETRY_NAND_COPY_NUM	4

#define	READ_RETRY_REG_NUM	8
#define	READ_RETRY_CNT      40

#define	EN_SLC_REG_NUM		8

#define	READ_RETRY_ZERO		((char)-1)

#define	NAND_CMD_HYNIX_GET_VALUE		0x37
#define	NAND_CMD_HYNIX_SET_VALUE_START		0x36
#define	NAND_CMD_HYNIX_SET_VALUE_END		0x16

#define	NAND_CMD_TOSHIBA_PRE_CON1		0x5c
#define	NAND_CMD_TOSHIBA_PRE_CON2		0xc5
#define	NAND_CMD_TOSHIBA_SET_VALUE		0x55
#define	NAND_CMD_TOSHIBA_BEF_COMMAND0		0xB3
#define	NAND_CMD_TOSHIBA_BEF_COMMAND1		0x26
#define	NAND_CMD_TOSHIBA_BEF_COMMAND2		0x5d
#define NAND_CMD_SAMSUNG_SET_VALUE		0XA1
#define NAND_CMD_MICRON_SET_VALUE		0XEF
#define	NAND_CMD_SANDISK_INIT_ONE		0x3B
#define	NAND_CMD_SANDISK_INIT_TWO		0xB9

#define	NAND_CMD_SANDISK_LOAD_VALUE_ONE		0x53
#define	NAND_CMD_SANDISK_LOAD_VALUE_TWO		0x54

#define	NAND_CMD_SANDISK_DYNAMIC_ENABLE		0xB6
#define	NAND_CMD_SANDISK_DYNAMIC_DISABLE	0xD6
#define	NAND_CMD_SANDISK_SLC			0xA2
#define NAND_CMD_SANDISK_SET_VALUE		0XEF
#define	NAND_CMD_SANDISK_DSP_OFF	0x25
#define	NAND_CMD_SANDISK_DSP_ON		0x26
#define	NAND_CMD_SANDISK_RETRY_STA	0x5D
#define	NAND_CMD_SANDISK_TEST_MODE1	0x5c
#define	NAND_CMD_SANDISK_TEST_MODE2	0xc5
#define	NAND_CMD_SANDISK_TEST_MODE_ACCESS 0x55
/* for hynix 20nm OTP */
#define	HYNIX_OTP_COPY		8
#define	HYNIX_OTP_LEN		528

/* for Hynix */
#define	HYNIX_26NM_4GB		1
#define	HYNIX_26NM_8GB		2
#define	HYNIX_20NM_4GB		3
#define	HYNIX_20NM_8GB		4
#define	HYNIX_1YNM			6
/* for Toshiba */
#define	TOSHIBA_2XNM		20
/* TC58NVG6D2GTA00 */
#define	TOSHIBA_A19NM		21
#define	TOSHIBA_15NM		22
/* for SAMSUNG */
#define	SUMSUNG_2XNM		30

/* for SANDISK */
#define	SANDISK_19NM		40
#define	SANDISK_24NM		41
#define	SANDISK_A19NM		42
#define	SANDISK_A19NM_4G	53

/* for Intel */
#define	INTEL_20NM	60
/* for Micron */
#define	MICRON_20NM	50

struct hw_controller;
struct read_retry_info {
	unsigned char flag;
	unsigned char default_flag;
	unsigned char info_save_blk;

	unsigned char reg_cnt_lp;
	unsigned char reg_cnt_up;
	unsigned char reg_cnt_tp;

	unsigned char retry_cnt_lp;
	unsigned char retry_cnt_up;
	unsigned char retry_cnt_tp;
	unsigned char retry_stage;

	unsigned char cur_cnt_lp[MAX_CHIP_NUM];
	unsigned char cur_cnt_up[MAX_CHIP_NUM];
	unsigned char cur_cnt_tp[MAX_CHIP_NUM];

	unsigned char reg_addr_lp[READ_RETRY_REG_NUM];
	unsigned char reg_addr_up[READ_RETRY_REG_NUM];
	unsigned char reg_addr_tp[READ_RETRY_REG_NUM];

	unsigned char reg_def_val[MAX_CHIP_NUM][READ_RETRY_REG_NUM];

	char reg_offs_val_lp[MAX_CHIP_NUM][READ_RETRY_CNT][READ_RETRY_REG_NUM];
	char reg_offs_val_up[MAX_CHIP_NUM][READ_RETRY_CNT][READ_RETRY_REG_NUM];
	char reg_offs_val_tp[MAX_CHIP_NUM][READ_RETRY_CNT][READ_RETRY_REG_NUM];

	int (*init)(struct hw_controller *controller);
	int (*handle)(struct hw_controller *controller, unsigned char chipnr);
	int (*exit)(struct hw_controller *controller, unsigned char chipnr);
};

struct en_slc_info {
	unsigned char flag;
	unsigned char *pagelist;
	unsigned char reg_cnt;
	unsigned char reg_addr[EN_SLC_REG_NUM];
	unsigned char reg_def_val[MAX_CHIP_NUM][EN_SLC_REG_NUM];
	char reg_offs_val[EN_SLC_REG_NUM];
	int (*init)(struct hw_controller *controller);
	int (*enter)(struct hw_controller *controller);
	int (*exit)(struct hw_controller *controller);
};

#endif

#define ECC_INFORMATION(name_a, bch_a, size_a, parity_a, user_a, index_a) \
	{\
		.name = name_a,\
		.mode = bch_a,\
		.unit_size = size_a,\
		.bytes = parity_a,\
		.usr_mode = user_a,\
		.bch_index = index_a\
	}

struct bch_desc {
	char *name;
	unsigned int mode;
	unsigned int unit_size;
	unsigned int bytes;
	unsigned int usr_mode;
	unsigned int bch_index;
};


/*** HW controller configuration ***/
struct hw_controller {
	unsigned int chip_selected;
	unsigned int rb_received;
	unsigned int ce_enable[MAX_CHIP_NUM];
	unsigned int rb_enable[MAX_CHIP_NUM];

	unsigned char chip_num;
	unsigned char flash_type;
	unsigned char mfr_type;
	unsigned int onfi_mode;

	 /* zero means no short */
	unsigned short short_pgsz;

	unsigned char max_bch;

	unsigned short ecc_unit;
	unsigned short ecc_bytes;
	unsigned short ecc_steps;

	unsigned short oobtail;

	unsigned char bch_mode;
	unsigned char user_mode;
	unsigned char ran_mode;
	unsigned char oobavail;
	unsigned char oob_mod;
	int oob_fill_data;
	int oob_fill_boot;
	unsigned char ecc_cnt_limit;
	unsigned char ecc_cnt_cur;
	unsigned char ecc_max;

	unsigned short page_shift;
	unsigned short block_shift;

	unsigned int internal_page_nums;
	unsigned int zero_cnt;

	unsigned int page_addr;

	unsigned int option;

	unsigned char *data_buf;
	unsigned int *user_buf;

	unsigned char *page_buf;
	unsigned char *oob_buf;

#if (AML_CFG_NEW_NAND_SUPPORT)
	struct en_slc_info slc_info;
	struct read_retry_info retry_info;
#endif

	dma_addr_t data_dma_addr;
	dma_addr_t info_dma_addr;

	void __iomem *reg_base;
	void __iomem *nand_clk_reg;
	void __iomem *pinmux_base;
	unsigned int irq;

	struct bch_desc *bch_desc;

#ifdef AML_NAND_DMA_POLLING
	struct hrtimer timer;
#endif

#if 0/* #ifndef AML_NAND_UBOOT */
	struct hw_ctrl hw_ctrl;
#endif
	struct amlnand_chip *aml_chip;

	/*** hw controller operation function ***/
	int (*init)(struct hw_controller *controller);
	int (*adjust_timing)(struct hw_controller *controller);
	int (*select_chip)(struct hw_controller *controller,
		unsigned char chipnr);
	int (*ecc_confirm)(struct hw_controller *controller);
	unsigned char (*readbyte)(struct hw_controller *controller);
	void (*writebyte)(struct hw_controller *controller,
		unsigned char data);
	void (*cmd_ctrl)(struct hw_controller *controller,
		unsigned int cmd,
		unsigned int ctrl);
	int (*quene_rb)(struct hw_controller *controller, unsigned char chipnr);
#ifdef AML_NAND_RB_IRQ
	int (*quene_rb_irq)(struct hw_controller *controller,
		unsigned char chipnr);
#endif
	int (*dma_read)(struct hw_controller *controller,
		unsigned int len,
		unsigned char bch_mode);
	int (*dma_write)(struct hw_controller *controller,
		unsigned char *buf,
		unsigned int len,
		unsigned char bch_mode);
	int (*hwecc_correct)(struct hw_controller *controller,
		unsigned int size,
		unsigned char *oob_buf);

	void (*get_usr_byte)(struct hw_controller *controller,
		unsigned char *oob_buf,
		unsigned char byte_num);
	void (*set_usr_byte)(struct hw_controller *controller,
		unsigned char *oob_buf,
		unsigned char byte_num);
	void (*enter_standby)(struct hw_controller *controller);

};

/*** nand chip operation function ***/
struct chip_operation {
	int (*check_wp)(struct amlnand_chip *aml_chip);

	int (*get_onfi_para)(struct amlnand_chip *aml_chip,
		unsigned char *buf,
		int addr);
	int (*set_onfi_para)(struct amlnand_chip *aml_chip,
		unsigned char *buf,
		int addr);

	int (*reset)(struct amlnand_chip *aml_chip, unsigned char chip_nr);
	int (*read_id)(struct amlnand_chip *aml_chip,
		unsigned char chip_nr,
		unsigned char id_addr,
		unsigned char *buf);

	/*
	 * Erase is an asynchronous operation.  Device drivers are supposed
	 * to call instr->callback() whenever the operation completes, even
	 * if it completes with a failure.
	 * Callers are supposed to pass a callback function and wait for it
	 * to be called before writing to the block.
	*/
	int (*erase_block)(struct amlnand_chip *aml_chip);
	int (*test_block_chip_op)(struct amlnand_chip *aml_chip);
	int (*test_block_reserved)(struct amlnand_chip *aml_chip, int tst_blk);
	/***basic data operation and included oob data****/
	int (*read_page)(struct amlnand_chip *aml_chip);
	int (*write_page)(struct amlnand_chip *aml_chip);

	int (*block_isbad)(struct amlnand_chip *aml_chip);
	int (*block_markbad)(struct amlnand_chip *aml_chip);

	int (*blk_modify_bbt_chip_op)(struct amlnand_chip *aml_chip, int value);
	int (*update_bbt_chip_op)(struct amlnand_chip *aml_chip);
};

/*** basic nand flash information  ***/
struct nand_flash {
	char *name;
	unsigned char id[MAX_ID_LEN];

	unsigned int pagesize;
	unsigned int chipsize;
	unsigned int blocksize;
	unsigned int oobsize;

	unsigned char internal_chipnr;
	unsigned char T_REA;
	unsigned char T_RHOH;
	unsigned char onfi_mode;
	unsigned char new_type;

	unsigned int option;
};

/*
*operation type as below: oob_mode data_buf oob_buf readlen
*1) read data hw ecc mode 0 available NULL 0
*2) read oob hw ecc mode 0 NULL available available
*3) read data and oob hw ecc mode 0 available available available
*4) read data/oob none ecc mode 1 available NULL available
*
* option chipnr page_addr
*mulit-chip 0
*serial-chip available
*multi-plane
*sigle-plane
*/
struct chip_ops_para {
	unsigned int page_addr;
	unsigned char chipnr;
	unsigned char *data_buf;
	unsigned char *oob_buf;
	enum oob_modes_t oob_mode;
	unsigned char bit_flip;
	unsigned char ecc_err;
	/*
	only for read oob mode, for chip operation,
	read data should be one entire page, but oob mode not.
	*/
	unsigned char ooblen;
	unsigned int option;
};


/**
 * struct platform_nand_chip - chip level device structure
 * @nr_chips:		max. number of chips to scan for
 * @chip_offset:	chip number offset
 * @nr_partitions:	number of partitions pointed to by partitions (or zero)
 * @partitions:		mtd partition list
 * @options:		Option flags, e.g. 16bit buswidth
 * @priv:		hardware controller specific settings
 */
struct dev_para {
	const char name[MAX_DEVICE_NAME_LEN];

	uint64_t offset;
	uint64_t size;
	struct amlnf_partition partitions[MAX_NAND_PART_NUM];
	unsigned char nr_partitions;

	unsigned int option;
};

struct _phy_partition {
	const char name[MAX_DEVICE_NAME_LEN];
	uint64_t phy_off;
	uint64_t phy_len;
	uint64_t logic_len;
};

#define MAX_PART_NUM	16
#define PART_NAME_LEN 16
struct partitions {
	/* identifier string */
	char name[PART_NAME_LEN];
	/* partition size, byte unit */
	uint64_t size;
	/* offset within the master space, byte unit */
	uint64_t offset;
	/* master flags to mask out for this partition */
	unsigned int mask_flags;
	/* for memcpy align; fixme, may not use.... ! */
	void *priv;
};

struct nand_config {
	unsigned int crc;
	struct dev_para dev_para[MAX_DEVICE_NUM];
	unsigned int driver_version;
	unsigned char dev_num;
	unsigned short fbbt_blk_addr;
};

struct phy_partition_info {
	unsigned int crc;
	struct _phy_partition partition[MAX_DEVICE_NUM];
	unsigned char dev_num;
};

struct nand_bbt {
	unsigned short nand_bbt[MAX_CHIP_NUM][MAX_BAD_BLK_NUM];
};

struct shipped_bbt {
	unsigned int crc;
	unsigned short shipped_bbt[MAX_CHIP_NUM][MAX_BAD_BLK_NUM];
};

struct nand_menson_key {
	uint crc;
	unsigned char data[252];
};

struct secure_t {
	/* CRC32 over data bytes */
	uint crc;
	/* Environment data */
	unsigned char data[SECURE_SIZE];
};

struct nand_arg_oobinfo {
	char name[4];
	unsigned short timestamp;
};

struct nand_arg_info {
	unsigned char arg_type;
	unsigned short valid_blk_addr;
	unsigned short valid_page_addr;
	unsigned short free_blk_addr;
	unsigned char arg_valid;
	unsigned short timestamp;
	/*
	flag indicate that: if read ecc error of any page of this block,
	should move data to another block
	*/
	unsigned char update_flag;
};

struct block_status {
	unsigned int crc;
	unsigned short blk_status[MAX_CHIP_NUM][MAX_BLK_NUM];
};

/**whole nand chip information  include hw controller and flash information **/
struct amlnand_chip {
	struct block_status *block_status;

	enum chip_state_t state;
	unsigned char nand_status;
	unsigned char init_flag;
	unsigned char key_protect;
	unsigned char secure_protect;
	unsigned char fbbt_protect;
	unsigned char ce_bit_mask;
	struct hw_controller controller;

	/* current operation parameter, should clear before used. */
	struct chip_ops_para ops_para;

	struct chip_operation operation;
	struct nand_flash flash;

	struct nand_arg_info config_msg;
	struct nand_config *config_ptr;
	struct phy_partition_info *phy_part_ptr;

	struct nand_arg_info nand_bbtinfo;
	struct nand_arg_info shipped_bbtinfo;
	struct shipped_bbt *shipped_bbt_ptr;

	struct nand_arg_info nand_key;
	struct nand_arg_info nand_secure;
	struct nand_arg_info uboot_env;
	struct nand_arg_info nand_phy_partition;
#if (AML_CFG_DTB_RSV_EN)
	struct nand_arg_info amlnf_dtb;
#endif
	struct pinctrl *nand_pinctrl;
	struct pinctrl_state *nand_pinstate;
	struct pinctrl_state *nand_rbstate;
	struct pinctrl_state *nand_norbstate;
	struct pinctrl_state *nand_idlestate;
	struct device *device;

	unsigned char reserved_blk[RESERVED_BLOCK_CNT];
	unsigned int max_ecc_per_page;
	unsigned char *user_page_buf;
	unsigned char *user_oob_buf;
	unsigned char protect;
	unsigned char debug_flag;
	unsigned char g_retry_cnt;

	void __iomem *reg_base;
	void __iomem *nand_clk_reg;

	u32 keysize;
	u32 dtbsize;
};

extern struct nand_flash flash_ids_slc[];
extern struct nand_flash flash_ids_mlc[];
extern struct bch_desc bch_list[MAX_ECC_MODE_NUM];
extern struct bch_desc bch_list_m8[MAX_ECC_MODE_NUM];
extern struct amlnand_chip *aml_chip_secure;
extern struct amlnand_chip *aml_nand_chip;

extern enum chip_state_t get_chip_state(struct amlnand_chip *aml_chip);
extern void set_chip_state(struct amlnand_chip *aml_chip,
	enum chip_state_t state);
extern int amlnand_get_device(struct amlnand_chip *aml_chip,
	enum chip_state_t new_state);
extern void amlnand_release_device(struct amlnand_chip *aml_chip);
extern int amlnand_hwcontroller_init(struct amlnand_chip *aml_chip);
extern int amlnand_init_operation(struct amlnand_chip *aml_chip);
extern int amlnand_get_dev_configs(struct amlnand_chip *aml_chip);
extern unsigned int amlnand_chip_init(struct amlnand_chip *aml_chip);
extern int amlnand_phydev_init(struct amlnand_chip *aml_chip);
extern int amlnand_update_bbt(struct amlnand_chip *aml_chip);
extern int amlnand_set_readretry_slc_para(struct amlnand_chip *aml_chip);
extern int aml_nand_scan_hynix_info(struct amlnand_chip *aml_chip);
extern int nand_reset(struct amlnand_chip *aml_chip, unsigned char chipnr);
extern void pinmux_select_chip(unsigned int ce_enable,
	unsigned int rb_enable,
	unsigned int flag);
extern int32_t nand_secure_read(struct amlnand_chip *aml_chip,
	char *buf,
	int len);
extern int32_t nand_secure_write(struct amlnand_chip *aml_chip,
	char *buf,
	int len);
extern int aml_sys_info_init(struct amlnand_chip *aml_chip);
extern void nand_boot_info_prepare(struct amlnand_phydev *phydev,
	unsigned char *page0_buf);
extern void uboot_set_ran_mode(struct amlnand_phydev *phydev);
extern void get_sys_clk_rate(struct hw_controller *controller, int *rate);
extern int aml_ubootenv_init(struct amlnand_chip *aml_chip);
extern int aml_ubootenv_reinit(struct amlnand_chip *aml_chip);

extern void nand_get_chip(void *aml_chip);
extern void nand_release_chip(void *aml_chip);
extern int aml_key_init(struct amlnand_chip *aml_chip);
extern int aml_key_reinit(struct amlnand_chip *aml_chip);
extern int aml_secure_init(struct amlnand_chip *aml_chip);

#if (AML_CFG_DTB_RSV_EN)
extern int amlnf_dtb_init(struct amlnand_chip *aml_chip);
extern int amlnf_dtb_reinit(struct amlnand_chip *aml_chip);
#endif

extern unsigned int aml_info_checksum(unsigned char *data, int lenth);
extern int amlnand_info_init(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *buf,
	unsigned char *name,
	unsigned int size);
extern int amlnand_check_info_by_name(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *name,
	unsigned int size);
extern int amlnand_save_info_by_name(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *buf,
	unsigned char *name,
	unsigned int size);
extern int amlnand_read_info_by_name(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *buf,
	unsigned char *name,
	unsigned int size);
extern int aml_sys_info_error_handle(struct amlnand_chip *aml_chip);
extern int aml_nand_update_key(struct amlnand_chip *aml_chip, char *key_ptr);
extern int aml_nand_update_secure(struct amlnand_chip *aml_chip,
	char *secure_ptr);
extern int aml_nand_update_ubootenv(struct amlnand_chip *aml_chip,
	char *env_ptr);


extern void amlchip_dumpinfo(struct amlnand_chip *aml_chip);
#if 0
extern void dump_pinmux_regs(struct hw_controller *controller);
#endif

u32 get_storage_dev(void);
#endif /* NAND_H_INCLUDED */
