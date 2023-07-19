// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/regmap.h>
#include <linux/dma-mapping.h>
#include "nfc.h"
#include "page_info.h"

struct regmap *nfc_regmap[2];

struct nfc_clk_provider clk_provider[MAX_CLK_PROVIDER] = {
	{OSC_CLK_24MHZ, CLK_6MHZ,	1, 4, 0, 1},
	{OSC_CLK_24MHZ, CLK_12MHZ,	1, 2, 0, 2},
	{FIX_PLL_DIV2,	CLK_20MHZ,	12, 4, 0, 3},
	{FIX_PLL_DIV2,	CLK_41MHZ,	6, 4, 0, 5},
	{FIX_PLL_DIV2,	CLK_83MHZ,	3, 4, 0, 7},
	{FIX_PLL_DIV2P5, CLK_100MHZ_400, 2, 4, 0, 11},
	{FIX_PLL_DIV2,	CLK_100MHZ_500,	2, 5, 0, 11},
	{FIX_PLL_DIV2,	CLK_125MHZ,	2, 4, 0, 13},
	{FIX_PLL_DIV2P5, CLK_133MHZ,	2, 3, 0, 13},
	{FIX_PLL_DIV2,	CLK_166MHZ_333,	3, 2, 0, 15},
	{FIX_PLL_DIV3,	CLK_166MHZ_666,	1, 4, 0, 15},
	{FIX_PLL_DIV2P5, CLK_200MHZ_400, 2, 2, 0, 21},
	{FIX_PLL_DIV2P5, CLK_200MHZ_800, 1, 4, 0, 21},
};

struct nfc_clk_provider *g_clk_info = &clk_provider[CLK_83MHZ];

unsigned int nfc_recalculate_n2m_command(u32 size, int no_cal)
{
	u32 n2m_cmd, page_size;
	int ecc_size;

	page_size = page_info_get_page_size();
	n2m_cmd = page_info_get_n2m_command();
	if (no_cal)
		return n2m_cmd;

	if (size == page_size)
		return n2m_cmd;
	ecc_size = (GET_BCH_MODE(n2m_cmd) == 1) ? 512 : 1024;
	size = (size + ecc_size - 1) & (~(ecc_size - 1));
	n2m_cmd &= ~0x3F;
	while (size >= ecc_size) {
		n2m_cmd++;
		size -= ecc_size;
	}
	return n2m_cmd;
}

#define NFC_CMD_GET_SIZE(x)	(((x) >> 22) & GENMASK(4, 0))
int nfc_wait_command_fifo_done(u32 timeout, int repeat_rb)
{
	u32 cmd_size = 0;
	int ret;

	/* wait cmd fifo is empty */
	ret = regmap_read_poll_timeout(nfc_regmap[NFC_IDX], NAND_CMD, cmd_size,
				       !NFC_CMD_GET_SIZE(cmd_size),
				       50, 0x100000 * 1000);
	if (ret)
		pr_info("wait for empty CMD FIFO time out\n");

	return ret;
}

void nfc_raw_size_ext_convert(u32 size)
{
	unsigned int raw_ext;

	regmap_read(nfc_regmap[NFC_IDX], SPI_CFG, &raw_ext);
	raw_ext &= ~(0xFFF << 18);
	raw_ext |= ((size >> NFC_RAW_CHUNK_SHIFT) << 18);
	regmap_write(nfc_regmap[NFC_IDX], SPI_CFG, raw_ext);

	regmap_read(nfc_regmap[NFC_IDX], SPI_CFG, &raw_ext);
	NFC_PRINT("SPI_CFG = 0x%x\n", raw_ext);
}

#define is_fixpll_locked()	(1)
#define IS_FEAT_EN_83MHZ_SPI()	(0)
#define IS_FEAT_EN_41MHZ_SPI()	(1)
#define IS_FEAT_EN_25MHZ_NAND()	(1)
#define otp_get_nfc_rxadj(x)	((*(x)) = 0)
static struct nfc_clk_provider *nfc_get_clock_provider(void)
{
	struct nfc_clk_provider *clk_info;
	unsigned char frequency_idx, cs_deselect_time;

	frequency_idx = page_info_get_frequency_index();
	if (frequency_idx == 0xFF) {
		clk_info = &clk_provider[CLK_41MHZ];
	} else {
		clk_info = &clk_provider[frequency_idx & 0x7F];
		if (frequency_idx & 0x80) {
			clk_info->div1 = page_info_get_core_div();
			clk_info->div2 = page_info_get_bus_cycle();
		}
	}

	clk_info->adj = page_info_get_adj_index();
	cs_deselect_time = page_info_get_cs_deselect_time();
	if (cs_deselect_time != 0xFF) {
		clk_info->cs_deselect_time = cs_deselect_time;
		NFC_PRINT("clk_info->cs_deselect_time = 0x%x\n",
			  clk_info->cs_deselect_time);
	}

	g_clk_info = clk_info;
	return clk_info;
}

void nfc_set_clock_and_timing(unsigned long *clk_rate)
{
	struct nfc_clk_provider *clk_info;
	unsigned char data_lanes, cmd_lanes, addr_lanes, mode;
	u32 value, line_delay1, line_delay2;

	clk_info = nfc_get_clock_provider();
	NFC_PRINT("clock_index = 0x%x\n", clk_info->clk_rate);

	line_delay1 = page_info_get_line_delay1();
	line_delay2 = page_info_get_line_delay2();
	data_lanes = page_info_get_data_lanes_mode();
	cmd_lanes = page_info_get_cmd_lanes_mode();
	addr_lanes = page_info_get_addr_lanes_mode();
	mode = page_info_get_work_mode();

	// SD_EMMC_DLY1/2
	regmap_write(nfc_regmap[EMMC_IDX], SD_EMMC_DLY1, line_delay1);
	regmap_write(nfc_regmap[EMMC_IDX], SD_EMMC_DLY2, line_delay2);
	regmap_read(nfc_regmap[EMMC_IDX], 0, &value);
	regmap_write(nfc_regmap[EMMC_IDX], 0, value | (1 << 6));
	value = data_lanes | (cmd_lanes << 2) |
		((clk_info->adj & 0x0F) << 4) |
		(3 << 12) | (mode << 14) | (addr_lanes << 16) |
		(1 << 31);
	regmap_write(nfc_regmap[NFC_IDX], SPI_CFG, value);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CFG,
		     (clk_info->div2 - 1) | (3 << 5) | (1 << 31));

	regmap_read(nfc_regmap[NFC_IDX], SPI_CFG, &value);
	NFC_PRINT("SPI_CFG = 0x%x\n", value);
	regmap_read(nfc_regmap[NFC_IDX], NAND_CFG, &value);
	NFC_PRINT("NAND_CFG = 0x%x\n", value);

	*clk_rate = 1000000000 / clk_info->div1;
}

void nfc_set_data_bus_width(int bus_width)
{
	u32 spi_cfg;

	regmap_read(nfc_regmap[NFC_IDX], SPI_CFG, &spi_cfg);
	spi_cfg &= (~0x03);
	spi_cfg |= bus_width;
	regmap_write(nfc_regmap[NFC_IDX], SPI_CFG, spi_cfg);

	regmap_read(nfc_regmap[NFC_IDX], SPI_CFG, &spi_cfg);
	NFC_PRINT("SPI_CFG = 0x%x\n", spi_cfg);
}

u32 nfc_send_cmd_addr_and_wait(unsigned char cmd_bitmap, u8 cmd,
			       u8 *addr, u8 addr_len,
			       u8 *data, u8 data_len,
			       unsigned short state_dummy_ctl)
{
	u8 *buffer = data, dummy_cycles = 0, com_cmd;
	enum CMD_TYPE cmd_type;
	enum STATE_AND_DUMMY_CTL action;
	int i = 0, j = 0, ret;

	if (((cmd_bitmap & TYPE_ADDR) && !addr) ||
	    ((cmd_bitmap & TYPE_DATA_DRD) && !buffer) ||
	    ((cmd_bitmap & TYPE_DATA_DWR) && !buffer) ||
	    cmd_bitmap == 0)
		return ERR_NFC_WRONG_PARAM;

	action = (state_dummy_ctl >> STATE_AND_DUMMY_CTL_BIT_SHIFT);
	NFC_PRINT("cs_deselect_time = 0x%x\n", g_clk_info->cs_deselect_time);
	if (cmd)
		regmap_write(nfc_regmap[NFC_IDX], NAND_CMD,
		     CEF | IDLE | g_clk_info->cs_deselect_time);

	while (cmd_bitmap != 0 && i < 8) {
		cmd_type = CMD_BITMAP_TEST_AND_GET_BIT_VALUE(cmd_bitmap, i);
		CMD_BITMAP_CLEAR_BIT(cmd_bitmap, i);
		switch (cmd_type) {
		case TYPE_NONE:
			/* skip the zero */
			break;
		case TYPE_CMD:
			NFC_PRINT("TYPE_CMD = 0x%x\n", CE0 | CLE | cmd);
			if (action & DONT_SEND_CEF) {
				com_cmd = ~cmd;
				regmap_write(nfc_regmap[NFC_IDX], NAND_CMD,
					     CE0 | CLE | com_cmd);
			}
			regmap_write(nfc_regmap[NFC_IDX], NAND_CMD,
				     CE0 | CLE | cmd);
			break;
		case TYPE_ADDR:
			if (state_dummy_ctl & 0xFF)
				dummy_cycles = (state_dummy_ctl & 0xFF) - 1;
			NFC_PRINT("dummy_cycles = 0x%x\n", dummy_cycles);
			if (action & DUMMY_BEFORE_ADDRESS)
				regmap_write(nfc_regmap[NFC_IDX], NAND_CMD,
					     CE0 | DUMMY | dummy_cycles);
			addr += addr_len - 1;
			NFC_PRINT("TYPE_ADDR: ");
			for (j = 0; j < addr_len; j++, addr--) {
				NFC_PRINT("0x%x ", CE0 | ALE | *addr);
				regmap_write(nfc_regmap[NFC_IDX], NAND_CMD,
					     CE0 | ALE | *addr);
			}
			NFC_PRINT("\n");
			if (action & DUMMY_AFTER_ADDRESS)
				regmap_write(nfc_regmap[NFC_IDX], NAND_CMD,
					     CE0 | DUMMY | dummy_cycles);
			break;
		case TYPE_DATA_DRD:
			for (j = 0; j < data_len; j++, buffer++) {
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | DRD | 0);
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | IDLE);
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | IDLE);
				ret = nfc_wait_command_fifo_done(NFC_COMMAND_FIFO_TIMEOUT, 0);
				if (ret)
					return ret;
				regmap_read(nfc_regmap[NFC_IDX], NAND_BUF, (unsigned int *)buffer);
				NFC_PRINT("TYPE_DATA_DRD: %02x\n", *buffer);
			}
			return 0;
		case TYPE_DATA_DWR:
			NFC_PRINT("TYPE_DATA_DWR\n");
			for (j = 0; j < data_len; j++, buffer++) {
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | IDLE | 10);
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | DWR | *buffer);
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | IDLE | 10);
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | IDLE);
				regmap_write(nfc_regmap[NFC_IDX],
					     NAND_CMD, CE0 | IDLE);
				ret = nfc_wait_command_fifo_done(NFC_COMMAND_FIFO_TIMEOUT, 0);
				if (ret)
					return ret;
			}
			return 0;
		default:
			break;
		}
		regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, CE0 | IDLE);
		i++;
	}

	return nfc_wait_command_fifo_done(NFC_COMMAND_FIFO_TIMEOUT, 0);
}

void nfc_set_dma_mem_and_info(u32 mem, unsigned long info)
{
	u32 cmd;

	cmd = GENCMDDADDRL(NFC_CMD_ADL, mem);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, cmd);
	cmd = GENCMDDADDRH(NFC_CMD_ADH, mem);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, cmd);
	cmd = GENCMDIADDRL(NFC_CMD_AIL, info);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, cmd);
	cmd = GENCMDIADDRH(NFC_CMD_AIH, info);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, cmd);
}

int nfc_start_dma_and_wait_done(u32 n2m_cmd)
{
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, n2m_cmd);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, CE0 | IDLE);
	regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, CE0 | IDLE);

	return nfc_wait_command_fifo_done(NFC_DATA_FIFO_TIMEOUT, 0);
}

void nfc_set_user_byte(u8 *oob_buf, u64 *info_buf, int ecc_steps)
{
	u64 *info;
	int i, count;

	for (i = 0, count = 0; i < ecc_steps; i++, count += 2) {
		info = &info_buf[i];
		*info |= oob_buf[count];
		*info |= oob_buf[count + 1] << 8;
	}
}

void nfc_get_user_byte(u8 *oob_buf, u64 *info_buf, int ecc_steps)
{
	u64 *info;
	int i, count;

	for (i = 0, count = 0; i < ecc_steps; i++, count += 2) {
		info = &info_buf[i];
		oob_buf[count] = *info;
		oob_buf[count + 1] = *info >> 8;
	}
}

int nfc_wait_data_and_ecc_engine_done(struct device *dev, dma_addr_t iaddr,
						u64 *info_buf, unsigned char nsteps, int raw)
{
	volatile u64 *info;
	int i, count = 0;
	int info_len = nsteps * 8;

	if (nsteps < 1)
		return ERR_NFC_WRONG_PARAM;

	info = info_buf + nsteps - 1;
	while (count <= 10000) {
		usleep_range(10, 15);
		/* info is updated by nfc dma engine*/
		smp_rmb();
		dma_sync_single_for_cpu(dev, iaddr, info_len, DMA_FROM_DEVICE);
		if (*info & ECC_COMPLETE)
			break;
		if (count == 10000) {
			regmap_write(nfc_regmap[NFC_IDX], NAND_CMD, 1 << 31);
			return ERR_DMA_TIMEOUT;
		}
		count++;
	}

	if (raw)
		return 0;

	for (i = 0, info = info_buf; i < nsteps; i++, info++) {
		if (ECC_ERR_CNT(*info) != ECC_UNCORRECTABLE)
			continue;

		if (ECC_ZERO_CNT(*info) < 0x0a)
			return 0;

		return ERROR_ECC;
	}

	return 0;
}
