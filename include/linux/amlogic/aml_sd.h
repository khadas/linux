/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/aml_sd.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_SD_H__
#define __AML_SD_H__

#include <linux/mmc/card.h>
#include <linux/interrupt.h>
/* unknown */
#define CARD_TYPE_UNKNOWN		0
/* MMC card */
#define CARD_TYPE_MMC			1
/* SD card */
#define CARD_TYPE_SD			2
/* SDIO card */
#define CARD_TYPE_SDIO			3
/* SD combo (IO+mem) card */
#define CARD_TYPE_SD_COMBO		4
/* NON sdio device (means SD/MMC card) */
#define CARD_TYPE_NON_SDIO		5

#define aml_card_type_unknown(c)	((c)->card_type == CARD_TYPE_UNKNOWN)
#define aml_card_type_mmc(c)		((c)->card_type == CARD_TYPE_MMC)
#define aml_card_type_sd(c)		((c)->card_type == CARD_TYPE_SD)
#define aml_card_type_sdio(c)		((c)->card_type == CARD_TYPE_SDIO)
#define aml_card_type_non_sdio(c)	((c)->card_type == CARD_TYPE_NON_SDIO)

/* flag is "AML" */
#define TUNED_FLAG            0x004C4D41
/* version is "V2" */
#define TUNED_VERSION         0x00003256
/* magic is 0x00487e44 */
#define TUNED_MAGIC           0x00487e44

struct mmc_phase {
	unsigned int core_phase;
	unsigned int tx_phase;
	unsigned int rx_phase;
	unsigned int tx_delay;
};

struct para_e {
	struct mmc_phase init;
	struct mmc_phase hs;
	struct mmc_phase hs2;
	struct mmc_phase hs4;
};

#define LATCHING_RXPHASE 0
#define LATCHING_TXPHASE 1
#define LATCHING_FIXADJ 2

struct meson_mmc_data {
	unsigned int tx_delay_mask;
	unsigned int rx_delay_mask;
	unsigned int always_on;
	unsigned int adjust;
	u8 latching_mode;
};

enum aml_host_bus_fsm { /* Host bus fsm status */
	BUS_FSM_IDLE,           /* 0, idle */
	BUS_FSM_SND_CMD,        /* 1, send cmd */
	BUS_FSM_CMD_DONE,       /* 2, wait for cmd done */
	BUS_FSM_RESP_START,     /* 3, resp start */
	BUS_FSM_RESP_DONE,      /* 4, wait for resp done */
	BUS_FSM_DATA_START,     /* 5, data start */
	BUS_FSM_DATA_DONE,      /* 6, wait for data done */
	BUS_FSM_DESC_WRITE_BACK,/* 7, wait for desc write back */
	BUS_FSM_IRQ_SERVICE,    /* 8, wait for irq service */
};

struct sd_emmc_desc {
	u32 cmd_cfg;
	u32 cmd_arg;
	u32 cmd_data;
	u32 cmd_resp;
};

struct meson_mmc_hole {
	u8 start;
	u8 size;
};

struct hs400_para {
	unsigned int delay1;
	unsigned int delay2;
	unsigned int intf3;
	unsigned int flag;
};

struct hs200_para {
	unsigned int adjust;
};

struct hs_para {
	unsigned int adjust;
};

struct aml_tuning_para {
	unsigned int chip_id[4];
	unsigned int magic;
	unsigned int vddee;
	struct hs400_para hs4[7];
	struct hs200_para hs2;
	struct hs_para hs;
	unsigned int version;
	unsigned int busmode;
	unsigned int update;
	int temperature;
	long long checksum;

};

struct meson_host {
	struct	device		*dev;
	struct	meson_mmc_data *data;
	struct	mmc_host	*mmc;
	struct	mmc_command	*cmd;

	void __iomem *regs;
	void __iomem *pin_mux_base;
	void __iomem *clk_tree_base;
	struct resource *res[3];
	struct clk *core_clk;
	struct clk *tx_clk;
	struct clk *mmc_clk;
	struct clk *mux[2];
	struct clk *mux1_in;
	struct clk *clk[3];
	unsigned long req_rate;
	bool ddr;

	bool dram_access_quirk;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_clk_gate;

	unsigned int bounce_buf_size;
	void *bounce_buf;
	dma_addr_t bounce_dma_addr;
	struct sd_emmc_desc *descs;
	dma_addr_t descs_dma_addr;

	int irq;

	bool vqmmc_enabled;
	struct para_e sdmmc;
	char is_tuning;
	unsigned int delay_cell;
	bool needs_pre_post_req;
	int sd_sdio_switch_volat_done;
	int irq_sdio_sleep;
	int sdio_irqen;
	unsigned int emmc_boot_base;
	u32 pin_mux_val;
	u32 clk_tree_val;
	u32 host_clk_val;
	int debug_flag;
	unsigned int card_type;
	unsigned int card_insert;
	u8 fixadj_have_hole;
	struct meson_mmc_hole hole[3];
	u8 fix_hole;
	u64 align[10];
	u32 reg_bak[20];
	u32 resume_clk;
	char cmd_retune;
	unsigned int win_start;
	u8 *blk_test;
	u8 *adj_win;
	unsigned int cmd_c;
	int cd_irq;
	irqreturn_t (*cd_gpio_isr)(int irq, void *dev_id);
	int is_uart;
	int sd_uart_init;
	int first_temp_index;
	int cur_temp_index;
	int compute_cmd_delay;
	int compute_coef;
	unsigned int save_para;
	unsigned int src_clk_rate;
	struct aml_tuning_para para;
	int run_pxp_flag;
	int nwr_cnt;
	bool ignore_desc_busy;
	bool use_intf3_tuning;
	bool enable_hwcq;
	int flags;
	spinlock_t lock; /* lock for claim and bus ops */
	bool src_clk_cfg_done;
	bool ctrl_pwr_flag;
	struct dentry *debugfs_root;
};

int sdio_reset_comm(struct mmc_card *card);
void sdio_reinit(void);
const char *get_wifi_inf(void);
int sdio_get_vendor(void);
void aml_host_bus_fsm_show(struct mmc_host *mmc, int status);

#define   DRIVER_NAME "meson-gx-mmc"

#define	  SD_EMMC_CLOCK 0x0
#define   CLK_DIV_MASK GENMASK(5, 0)
#define   CLK_SRC_MASK GENMASK(7, 6)
#define   CLK_CORE_PHASE_MASK GENMASK(9, 8)
#define   CLK_TX_PHASE_MASK GENMASK(11, 10)
#define   CLK_RX_PHASE_MASK GENMASK(13, 12)
#define   CLK_PHASE_0 0
#define   CLK_PHASE_180 2
#define   CLK_V2_TX_DELAY_MASK GENMASK(19, 16)
#define   CLK_V2_RX_DELAY_MASK GENMASK(23, 20)
#define   CLK_V2_ALWAYS_ON BIT(24)

#define   CLK_V3_TX_DELAY_MASK GENMASK(21, 16)
#define   CLK_V3_RX_DELAY_MASK GENMASK(27, 22)
#define   CLK_V3_ALWAYS_ON BIT(28)
#define   CFG_IRQ_SDIO_SLEEP BIT(29)
#define   CFG_IRQ_SDIO_SLEEP_DS BIT(30)

#define   CLK_TX_DELAY_MASK(h)    ((h)->data->tx_delay_mask)
#define   CLK_RX_DELAY_MASK(h)    ((h)->data->rx_delay_mask)
#define   CLK_ALWAYS_ON(h)        ((h)->data->always_on)

#define SD_EMMC_DELAY 0x4

#define SD_EMMC_ADJUST 0x8
#define   ADJUST_ADJ_DELAY_MASK GENMASK(21, 16)
#define   ADJUST_DS_EN BIT(15)
#define   ADJUST_ADJ_EN BIT(13)

#define SD_EMMC_DELAY1 0x4
#define SD_EMMC_DELAY2 0x8
#define SD_EMMC_V3_ADJUST 0xc
#define	  CALI_SEL_MASK GENMASK(11, 8)
#define	  CALI_ENABLE BIT(12)
#define	  CFG_ADJUST_ENABLE BIT(13)
#define	  CALI_RISE BIT(14)
#define	  DS_ENABLE BIT(15)
#define	  CLK_ADJUST_DELAY GENMASK(21, 16)
#define	  ADJ_AUTO BIT(22)

#define SD_EMMC_CALOUT 0x10
#define SD_EMMC_ADJ_IDX_LOG 0x20
#define SD_EMMC_CLKTEST_LOG 0x24
#define   CLKTEST_TIMES_MASK GENMASK(30, 0)
#define   CLKTEST_DONE BIT(31)
#define SD_EMMC_CLKTEST_OUT 0x28
#define SD_EMMC_EYETEST_LOG 0x2c
#define   EYETEST_TIMES_MASK GENMASK(30, 0)
#define   EYETEST_DONE BIT(31)
#define SD_EMMC_EYETEST_OUT0 0x30
#define SD_EMMC_EYETEST_OUT1 0x34
#define SD_EMMC_INTF3 0x38
#define   CLKTEST_EXP_MASK GENMASK(4, 0)
#define   CLKTEST_ON_M BIT(5)
#define   EYETEST_EXP_MASK GENMASK(10, 6)
#define   EYETEST_ON BIT(11)
#define   DS_SHT_M_MASK GENMASK(17, 12)
#define   DS_SHT_EXP_MASK GENMASK(21, 18)
#define   SD_INTF3 BIT(22)

#define   EYETEST_SEL BIT(26)
#define   RESP_SEL BIT(27)
#define   CFG_RX_SEL BIT(26)
#define   CFG_RX_PN BIT(27)
#define   RESP_OLD BIT(28)
#define   RESP_PN BIT(29)
#define   RESP_DS BIT(30)

#define SD_EMMC_START 0x40
#define   START_DESC_INIT BIT(0)
#define   START_DESC_BUSY BIT(1)
#define   START_DESC_ADDR_MASK GENMASK(31, 2)

#define SD_EMMC_CFG 0x44
#define   CFG_BUS_WIDTH_MASK GENMASK(1, 0)
#define   CFG_BUS_WIDTH_1 0x0
#define   CFG_BUS_WIDTH_4 0x1
#define   CFG_BUS_WIDTH_8 0x2
#define   CFG_DDR BIT(2)
#define   CFG_BLK_LEN_MASK GENMASK(7, 4)
#define   CFG_RESP_TIMEOUT_MASK GENMASK(11, 8)
#define   CFG_RC_CC_MASK GENMASK(15, 12)
#define   CFG_STOP_CLOCK BIT(22)
#define   CFG_CLK_ALWAYS_ON BIT(18)
#define   CFG_CHK_DS BIT(20)
#define   CFG_AUTO_CLK BIT(23)
#define   CFG_ERR_ABORT BIT(27)

#define SD_EMMC_STATUS 0x48
#define   STATUS_BUSY BIT(31)
#define   STATUS_DESC_BUSY BIT(30)
#define   STATUS_DATI GENMASK(23, 16)

#define SD_EMMC_IRQ_EN 0x4c
#define   IRQ_RXD_ERR_MASK GENMASK(7, 0)
#define   IRQ_TXD_ERR BIT(8)
#define   IRQ_DESC_ERR BIT(9)
#define   IRQ_RESP_ERR BIT(10)
#define   IRQ_CRC_ERR \
	(IRQ_RXD_ERR_MASK | IRQ_TXD_ERR | IRQ_DESC_ERR | IRQ_RESP_ERR)
#define   IRQ_RESP_TIMEOUT BIT(11)
#define   IRQ_DESC_TIMEOUT BIT(12)
#define   IRQ_TIMEOUTS \
	(IRQ_RESP_TIMEOUT | IRQ_DESC_TIMEOUT)
#define   IRQ_END_OF_CHAIN BIT(13)
#define   IRQ_RESP_STATUS BIT(14)
#define   IRQ_SDIO BIT(15)
#define   CFG_CMD_SETUP BIT(17)
#define   BUS_FSM_MASK GENMASK(29, 26)
#define   IRQ_EN_MASK \
	(IRQ_CRC_ERR | IRQ_TIMEOUTS | IRQ_END_OF_CHAIN | IRQ_RESP_STATUS |\
	 IRQ_SDIO)

#define SD_EMMC_CMD_CFG 0x50
#define SD_EMMC_CMD_ARG 0x54
#define SD_EMMC_CMD_DAT 0x58
#define SD_EMMC_CMD_RSP 0x5c
#define SD_EMMC_CMD_RSP1 0x60
#define SD_EMMC_CMD_RSP2 0x64
#define SD_EMMC_CMD_RSP3 0x68

#define SD_EMMC_RXD 0x94
#define SD_EMMC_TXD 0x94
#define SD_EMMC_LAST_REG SD_EMMC_TXD

#define SD_EMMC_SRAM_DESC_BUF_OFF 0x200
#define SD_EMMC_SRAM_DATA_BUF_LEN 1024
#define SD_EMMC_SRAM_DATA_BUF_OFF 0x400
#define SD_EMMC_MAX_SEGS 256
#define SD_EMMC_MAX_REQ_SIZE (128 * 1024)
#define SD_EMMC_MAX_SEG_SIZE (64 * 1024)

#define SD_EMMC_CFG_BLK_SIZE 512 /* internal buffer max: 512 bytes */
#define SD_EMMC_CFG_RESP_TIMEOUT 256 /* in clock cycles */
#define SD_EMMC_CMD_TIMEOUT 1024 /* in ms */
#define SD_EMMC_CMD_TIMEOUT_DATA 4096 /* in ms */
#define SD_EMMC_CFG_CMD_GAP 16 /* in clock cycles */
#define SD_EMMC_DESC_BUF_LEN PAGE_SIZE

#define SD_EMMC_PRE_REQ_DONE BIT(0)
#define SD_EMMC_DESC_CHAIN_MODE BIT(1)

#define MUX_CLK_NUM_PARENTS 2

#define CMD_CFG_LENGTH_MASK GENMASK(8, 0)
#define CMD_CFG_BLOCK_MODE BIT(9)
#define CMD_CFG_R1B BIT(10)
#define CMD_CFG_END_OF_CHAIN BIT(11)
#define CMD_CFG_TIMEOUT_MASK GENMASK(15, 12)
#define CMD_CFG_NO_RESP BIT(16)
#define CMD_CFG_NO_CMD BIT(17)
#define CMD_CFG_DATA_IO BIT(18)
#define CMD_CFG_DATA_WR BIT(19)
#define CMD_CFG_RESP_NOCRC BIT(20)
#define CMD_CFG_RESP_128 BIT(21)
#define CMD_CFG_RESP_NUM BIT(22)
#define CMD_CFG_DATA_NUM BIT(23)
#define CMD_CFG_CMD_INDEX_MASK GENMASK(29, 24)
#define CMD_CFG_ERROR BIT(30)
#define CMD_CFG_OWNER BIT(31)

#define CMD_DATA_MASK GENMASK(31, 2)
#define CMD_DATA_BIG_ENDIAN BIT(1)
#define CMD_DATA_SRAM BIT(0)
#define CMD_RESP_MASK GENMASK(31, 1)
#define CMD_RESP_SRAM BIT(0)
#define EMMC_SDIO_CLOCK_FELD    0Xffff
#define CALI_HS_50M_ADJUST      0
#define ERROR   1
#define FIXED   2
#define		SZ_1M			0x00100000
#define	MMC_PATTERN_NAME		"pattern"
#define	MMC_PATTERN_OFFSET		((SZ_1M * (36 + 3)) / 512)
#define	MMC_MAGIC_NAME			"magic"
#define	MMC_MAGIC_OFFSET		((SZ_1M * (36 + 6)) / 512)
#define	MMC_RANDOM_NAME			"random"
#define	MMC_RANDOM_OFFSET		((SZ_1M * (36 + 7)) / 512)
#define	MMC_DTB_NAME			"dtb"
#define	MMC_DTB_OFFSET			((SZ_1M * (36 + 4)) / 512)
#define CALI_BLK_CNT	80
#define CALI_HS_50M_ADJUST	0
#define EMMC_SDIO_CLOCK_FELD	0Xffff
#define MMC_PM_TIMEOUT	(2000)
#define ERROR	1
#define FIXED	2
#define RETUNING	3
#define	DATA3_PINMUX_MASK GENMASK(15, 12)

#define NBITS_FIX_ADJ	(64)
#define TUNING_NUM_PER_POINT 40
#define MAX_TUNING_RETRY 4
#define AML_FIXED_ADJ_MAX 8
#define AML_FIXED_ADJ_MIN 5
#define AML_FIXADJ_STEP 4
#define ADJ_WIN_PRINT_MAXLEN 256
#define NO_FIXED_ADJ_MID BIT(31)
#define AML_MV_DLY2_NOMMC_CMD(x) ((x) << 24)

#define SD_EMMC_FIXED_ADJ_HS200
#define EMMC_CMD_WIN_MAX_SIZE	50
#define EMMC_CMD_WIN_FULL_SIZE	64

#define DBG_COMMON        BIT(0)
#define DBG_HS200         BIT(1)
#define Print_dbg(dbg_level, fmt, args...) do {\
		if ((dbg_level) & mmc_debug)	\
			pr_info("[%s]" fmt, __func__, ##args);	\
} while (0)

/* delay_cell=70ps,1ns/delay_cell */
#define DELAY_CELL_COUNTS 14

/* Host attributes */
#define AML_USE_64BIT_DMA        BIT(0)

#endif /*__AML_SD_H__*/

