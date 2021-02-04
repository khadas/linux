/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLMTD_NAND_H_
#define __AMLMTD_NAND_H_

#include <linux/mtd/rawnand.h>
#include <linux/clk-provider.h>

#define NAND_TIMING_MODE0  0x0
#define NAND_TIMING_MODE1  0x1
#define NAND_TIMING_MODE2  0x2
#define NAND_TIMING_MODE3  0x3
#define NAND_TIMING_MODE4  0x4
#define NAND_TIMING_MODE5  0x5

#define NFC_REG_CMD		0x00
#define NFC_CMD_IDLE		(0xc << 14)
#define NFC_CMD_CLE		(0x5 << 14)
#define NFC_CMD_ALE		(0x6 << 14)
#define NFC_CMD_ADL		((0 << 16) | (3 << 20))
#define NFC_CMD_ADH		((1 << 16) | (3 << 20))
#define NFC_CMD_AIL		((2 << 16) | (3 << 20))
#define NFC_CMD_AIH		((3 << 16) | (3 << 20))
#define NFC_CMD_SEED		((8 << 16) | (3 << 20))
#define NFC_CMD_M2N		((0 << 17) | (2 << 20))
#define NFC_CMD_N2M		((1 << 17) | (2 << 20))
#define NFC_CMD_DRD		(0x8 << 14)
#define NFC_CMD_RB		BIT(20)
#define NFC_CMD_SCRAMBLER_ENABLE	1 //BIT(19)
#define NFC_CMD_SCRAMBLER_DISABLE	0
#define NFC_CMD_SHORTMODE_DISABLE	0
#define NFC_CMD_SHORTMODE_ENABLE	1
#define NFC_CMD_RB_INT		BIT(14)

#define NFC_CMD_GET_SIZE(x)	(((x) >> 22) & GENMASK(4, 0))

#define NFC_REG_CFG		0x04
#define NFC_REG_DADR		0x08
#define NFC_REG_IADR		0x0c
#define NFC_REG_BUF		0x10
#define NFC_REG_INFO		0x14
#define NFC_REG_DC		0x18
#define NFC_REG_ADR		0x1c
#define NFC_REG_DL		0x20
#define NFC_REG_DH		0x24
#define NFC_REG_CADR		0x28
#define NFC_REG_SADR		0x2c
#define NFC_REG_PINS		0x30
#define NFC_REG_VER		0x38

#define NFC_RB_IRQ_EN		BIT(21)

#define CMDRWGEN(cmd_dir, ran, bch, short_mode, page_size, pages)	\
	(								\
		(cmd_dir)			|			\
		((ran) << 19)			|			\
		((bch) << 14)			|			\
		((short_mode) << 13)		|			\
		(((page_size) & 0x7f) << 6)	|			\
		((pages) & 0x3f)					\
	)

#define GENCMDDADDRL(adl, addr)		((adl) | ((addr) & 0xffff))
#define GENCMDDADDRH(adh, addr)		((adh) | (((addr) >> 16) & 0xffff))
#define GENCMDIADDRL(ail, addr)		((ail) | ((addr) & 0xffff))
#define GENCMDIADDRH(aih, addr)		((aih) | (((addr) >> 16) & 0xffff))

#define DMA_DIR(dir)		((dir) ? NFC_CMD_N2M : NFC_CMD_M2N)

#define ECC_CHECK_RETURN_FF	(-1)

#define NAND_CE0		(0xe << 10)
#define NAND_CE1		(0xd << 10)

#define DMA_BUSY_TIMEOUT	0x100000
#define CMD_FIFO_EMPTY_TIMEOUT	1000

#define MAX_CE_NUM		2
#define NAND_MAX_DEVICE	4

/* eMMC clock register, misc control */
#define CLK_SELECT_NAND		BIT(31)

#define NFC_CLK_CYCLE		6

/* nand flash controller delay 3 ns */
#define NFC_DEFAULT_DELAY	3000

#define ROW_ADDER(page, index)	(((page) >> (8 * (index))) & 0xff)
#define MAX_CYCLE_ADDRS		5
#define DIRREAD			1
#define DIRWRITE		0

#define ECC_PARITY_BCH8_512B	14
#define ECC_COMPLETE            BIT(31)
#define ECC_ERR_CNT(x)		(((x) >> 24) & GENMASK(5, 0))
#define ECC_ZERO_CNT(x)		(((x) >> 16) & GENMASK(5, 0))
#define ECC_UNCORRECTABLE	0x3f

#define PER_INFO_BYTE		8

#define NAND_BLOCK_GOOD	0
#define NAND_BLOCK_BAD	1
#define NAND_FACTORY_BAD	2

/* Max total is 1024 as romboot says so... */
#define BOOT_TOTAL_PAGES	(1024)
#define NAND_FIPMODE_DISCRETE   (1)
struct meson_nfc_nand_chip {
	struct list_head node;
	struct nand_chip nand;
	unsigned long clk_rate;
	unsigned long level1_divider;
	u32 bus_timing;
	u32 twb;
	u32 tadl;
	u32 tbers_max;
	u32 bch_info;
	u32 bch_mode;
	u8 *data_buf;
	__le64 *info_buf;
	u32 nsels;
	u8 sels[0];
};

struct meson_nand_ecc {
	u32 bch;
	u32 strength;
	u32 step_size;
};

struct meson_nfc_data {
	const struct nand_ecc_caps *ecc_caps;
	int bl2ex_mode;
};

struct meson_nfc_param {
	u32 chip_select;
	u32 rb_select;
};

struct nand_rw_cmd {
	u32 cmd0;
	u32 addrs[MAX_CYCLE_ADDRS];
	u32 cmd1;
};

struct nand_timing {
	u32 twb;
	u32 tadl;
	u32 tbers_max;
};

struct meson_nfc {
	struct nand_controller controller;
	struct clk *clk_gate;
	struct clk *fix_div2_pll;
	struct clk_divider nand_divider;
	struct clk *nand_div_clk;
	unsigned long clk_rate;
	u32 bus_timing;

	struct device *dev;
	void __iomem *reg_base;
	void __iomem *nand_clk_reg;

	struct completion completion;
	struct list_head chips;
	const struct meson_nfc_data *data;
	struct meson_nfc_param param;
	struct nand_timing timing;
	struct meson_rsv_handler_t *rsv;
	union {
		int cmd[32];
		struct nand_rw_cmd rw;
	} cmdfifo;

	dma_addr_t daddr;
	dma_addr_t iaddr;

	unsigned long assigned_cs;
	s8 *block_status;

	struct para_form_dts {
		u32 clk_ctrl_base;
		u32 bl_mode;
		u32 fip_copies;
		u32 fip_size;
		u32 skip_bad_block;
		u32 disa_irq_hand;
	} param_from_dts;

	struct pinctrl *nand_pinctrl;
	struct pinctrl_state *nand_norbstate;
	struct pinctrl_state *nand_idlestate;
};

/**for info page0 information**/
union sc2_cmdinfo {
	u32 d32;
	struct {
		unsigned cmd:22;
		unsigned page_list:1;
		unsigned new_type:8;
		unsigned reserved:1;
	} b;
};

struct _nand_setup_sc2 {
	union sc2_cmdinfo cfg;
	u16 id;
	u16 max;
};

union cmdinfo {
	u32 d32;
	struct {
		unsigned cmd:22;
		unsigned large_page:1;
		unsigned no_rb:1;
		unsigned a2:1;
		unsigned reserved25:1;
		unsigned page_list:1;
		unsigned sync_mode:2;
		unsigned size:2;
		unsigned active:1;
	} b;
};

struct _nand_setup {
	union cmdinfo cfg;
	u16 id;
	u16 max;
};

struct _nand_cmd {
	unsigned char type;
	unsigned char val;
};

struct _ext_info {
	u32 read_info;
	u32 new_type;
	u32 pages_per_blk;
	u32 xlc;
	u32 ce_mask;
	u32 boot_num;
	u32 each_boot_pages;
	u32 bbt_occupy_pages;
	u32 bbt_start_block;
};

struct _fip_info {
	u16 version;
	u16 mode; /*compact or discrete*/
	u32 fip_start; /*fip_start, pages*/
};

#define NAND_PAGELIST_CNT 16
#define NAND_ECC_UNIT_SHORT 384
/*max size 384 bytes*/
struct _nand_page0 {
	struct _nand_setup nand_setup;
	unsigned char page_list[NAND_PAGELIST_CNT];
	struct _nand_cmd retry_usr[32];
	struct _ext_info ext_info;
	struct _fip_info fip_info;
	u32 ddrp_start_page;
};

struct _nand_page0_sc2 {
	struct _nand_setup_sc2 nand_setup;
	unsigned char page_list[32];
	struct _nand_cmd retry_usr[32];
	struct _ext_info ext_info;
	struct _fip_info fip_info;
	u32 ddrp_start_page;
};
/**end info page0 information**/
extern struct mtd_part_parser ofpart_meson_parser;
extern struct nand_flash_dev aml_nand_flash_ids[];

/**sc2 new layout**/
struct nand_startup_parameter {
	int page_size;
	int block_size;
	int layout_reserve_size;
	int pages_per_block;
	int setup_data;
	int page0_disable;
};

#define BL2E_STORAGE_PARAM_SIZE		(0x80)
#define BOOT_FIRST_BLOB_SIZE		(254 * 1024)
#define BOOT_FILLER_SIZE	(4 * 1024)
#define BOOT_RESERVED_SIZE	(4 * 1024)
#define BOOT_RANDOM_NONCE	(16)
#define BOOT_BL2E_SIZE		(66672) //74864-8K
#define BOOT_EBL2E_SIZE		\
	(BOOT_FILLER_SIZE + BOOT_RESERVED_SIZE + BOOT_BL2E_SIZE)
#define BOOT_BL2X_SIZE		(66672)
#define MAX_BOOT_AREA_ENTRIES	(8)
#define BL2_CORE_BASE_OFFSET_EMMC	(0x200000)
#define BOOT_AREA_BB1ST             (0)
#define BOOT_AREA_BL2E              (1)
#define BOOT_AREA_BL2X              (2)
#define BOOT_AREA_DDRFIP            (3)
#define BOOT_AREA_DEVFIP            (4)
#define BOOT_AREA_INVALID           (MAX_BOOT_AREA_ENTRIES)
#define BAE_BB1ST                   "1STBLOB"
#define BAE_BL2E                    "BL2E"
#define BAE_BL2X                    "BL2X"
#define BAE_DDRFIP                  "DDRFIP"
#define BAE_DEVFIP                  "DEVFIP"

struct boot_area_entry {
	char name[11];
	unsigned char idx;
	u64 offset;
	u64 size;
};

struct boot_layout {
	struct boot_area_entry *boot_entry;
};

struct storage_boot_entry {
	unsigned int offset;
	unsigned int size;
};

union storage_independent_parameter {
	struct nand_startup_parameter nsp;
};

struct storage_startup_parameter {
	unsigned char boot_device;
	unsigned char	boot_seq;
	unsigned char	boot_backups;
	unsigned char reserved;
	struct storage_boot_entry boot_entry[MAX_BOOT_AREA_ENTRIES];
	union storage_independent_parameter sip;
};

extern struct storage_startup_parameter g_ssp;
int aml_nand_param_check_and_layout_init(void);
/**sc2 new layout**/
#endif  /* __AMLMTD_NAND_H_ */
