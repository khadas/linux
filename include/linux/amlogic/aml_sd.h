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
	char cmd_retune;
	unsigned int win_start;
	u8 *blk_test;
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
};

int sdio_reset_comm(struct mmc_card *card);
void sdio_reinit(void);
const char *get_wifi_inf(void);

#endif /*__AML_SD_H__*/

