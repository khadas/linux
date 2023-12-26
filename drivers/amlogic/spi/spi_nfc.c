// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched/task_stack.h>
#include <linux/dma-mapping.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/spi/spi-mem.h>
#include <linux/mtd/mtd.h>
#include "nfc.h"
#include "page_info.h"

//#define __SPI_NFC_DEBUG__

#ifdef __SPI_NFC_DEBUG__
#define SPI_NFC_DEBUG(...)	pr_info(__VA_ARGS__)
#else
#define SPI_NFC_DEBUG(...)
#endif

#define REG_MAX				0x80

#define CMD_PROG_LOAD                   0x02
#define CMD_PROG_LOAD_RDM_DATA          0x84
#define CMD_PROG_LOAD_X4                0x32
#define CMD_PROG_LOAD_RDM_DATA_X4       0x34
#define CMD_READ			0x03
#define CMD_READ_FAST			0x0b
#define CMD_READ_DUAL_OUT		0x3b
#define CMD_READ_QUAD_OUT		0x6b
#define CMD_READ_DUAL_IO		0xbb
#define CMD_READ_QUAD_IO		0xeb

#define IS_READ_CACHE_CMD(cmd)				\
	((cmd) == CMD_READ ||				\
	(cmd) == CMD_READ_FAST ||			\
	(cmd) == CMD_READ_DUAL_OUT ||			\
	(cmd) == CMD_READ_QUAD_OUT ||			\
	(cmd) == CMD_READ_DUAL_IO ||			\
	(cmd) == CMD_READ_QUAD_IO)

#define IS_WRITE_CACHE_CMD(cmd)				\
	((cmd) == CMD_PROG_LOAD ||			\
	(cmd) == CMD_PROG_LOAD_RDM_DATA ||		\
	(cmd) == CMD_PROG_LOAD_X4 ||			\
	(cmd) == CMD_PROG_LOAD_RDM_DATA_X4)

#define IS_CACHE_CMD(cmd)				\
	(IS_READ_CACHE_CMD(cmd) || IS_WRITE_CACHE_CMD(cmd))

#define DATA_BUF_SIZE		(4096)
#define OOB_BUF_SIZE		(128)
#define SPI_NFC_BUF_SIZE	(DATA_BUF_SIZE + OOB_BUF_SIZE)

struct spi_nfc {
	struct spi_master *master;
	struct regmap *regmap[2];
	struct clk *clk_gate;
	struct clk *fix_div2_pll;
	struct clk_divider divider;
	struct clk *div_clk;
	struct device *dev;

	void __iomem *nand_clk_reg;

	u8 disable_host_ecc;
	u8 *data_buf;
	u8 *info_buf;
	dma_addr_t daddr;
	dma_addr_t iaddr;
};

static const struct regmap_config spi_nfc_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = REG_MAX,
};

static int spi_nfc_buffer_init(struct spi_nfc *spi_nfc)
{
	spi_nfc->data_buf = kzalloc(SPI_NFC_BUF_SIZE, GFP_KERNEL);
	if (!spi_nfc->data_buf)
		return -1;

	spi_nfc->info_buf = spi_nfc->data_buf + DATA_BUF_SIZE;

	return 0;
}

static void spi_nfc_buffer_free(struct spi_nfc *spi_nfc)
{
	kfree(spi_nfc->data_buf);
	kfree(spi_nfc->info_buf);
}

static void spi_nfc_auto_oob_ops(u8 *info, u8 *oob_pos, int ecc_steps, bool set)
{
	u8 *src, *dst, i;

	for (i = 0; i < ecc_steps; i++) {
		src = (set) ? (info + 2 * i) : (oob_pos + i * (2 + 14));
		dst = (set) ? (oob_pos + i * (2 + 14)) : (info + 2 * i);
		memcpy(dst, src, 2);
	}
}

static void spi_nfc_covert_buf_to_user(struct spi_nfc *spi_nfc,
				       u8 *user_buf,
				       u8 *oob_pos, u8 ecc_steps,
				       bool oob_only, bool auto_oob,
				       bool oob_required)
{
	u8 *buf = spi_nfc->data_buf, *info_buf = spi_nfc->info_buf;
	u8 oob_temp[OOB_BUF_SIZE] = {0};
	u32 page_size = page_info_get_page_size();

	SPI_NFC_DEBUG("oob only %s\n", (oob_only) ? "yes" : "no");
	SPI_NFC_DEBUG("oob is %s\n", (oob_required) ? "required" : "not required");
	SPI_NFC_DEBUG("auto_oob is %s\n", (auto_oob) ? "set" : "not set");

	if (oob_only || oob_required) {
		nfc_get_user_byte(oob_temp, (u64 *)info_buf, ecc_steps);
		DUMP_BUFFER(info_buf, 64, 4, 16);
		if (auto_oob)
			spi_nfc_auto_oob_ops(oob_temp, oob_pos,
				     ecc_steps, true);
		else
			memcpy(oob_pos, oob_temp, ecc_steps * 2);
	}

	if (oob_only)
		return;

	memcpy((u8 *)user_buf, buf, page_size);
	DUMP_BUFFER(user_buf, page_size, page_size / 512, 16);
}

static void spi_nfc_covert_buf_to_host(struct spi_nfc *spi_nfc,
				       u8 *user_buf,
				       u8 *oob_pos, u8 ecc_steps,
				       bool oob_only, bool auto_oob,
				       bool oob_required)
{
	u8 *buf = spi_nfc->data_buf, *info_buf = spi_nfc->info_buf;
	u8 oob_temp[OOB_BUF_SIZE] = {0};
	u32 page_size = page_info_get_page_size();

	SPI_NFC_DEBUG("oob only %s\n", (oob_only) ? "yes" : "no");
	SPI_NFC_DEBUG("oob is %s\n", (oob_required) ? "required" : "not required");
	SPI_NFC_DEBUG("auto_oob is %s\n", (auto_oob) ? "set" : "not set");

	DUMP_BUFFER(oob_pos, OOB_BUF_SIZE, 4, 16);
	if (oob_only || oob_required) {
		if (auto_oob)
			spi_nfc_auto_oob_ops(oob_temp, oob_pos,
					     ecc_steps, false);
		else
			memcpy(oob_temp, oob_pos, ecc_steps * 2);
	}

	memset(info_buf, 0, 64);
	nfc_set_user_byte(oob_temp, (u64 *)info_buf, ecc_steps);

	if (oob_only)
		return;

	memcpy(buf, user_buf, page_size);
	DUMP_BUFFER(buf, page_size, page_size / 512, 16);
}

static int spi_nfc_ooblayout_ecc(struct mtd_info *mtd, int section,
			       struct mtd_oob_region *oobregion)
{
	if (section >= 8)
		return -ERANGE;

	oobregion->offset =  2 + (section * (2 + 14));
	oobregion->length = 14;

	return 0;
}

static int spi_nfc_ooblayout_free(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	if (section >= 8)
		return -ERANGE;

	oobregion->offset = section * (2 + 14);
	oobregion->length = 2;

	return 0;
}

static const struct mtd_ooblayout_ops spi_nfc_ecc_ooblayout = {
	.ecc = spi_nfc_ooblayout_ecc,
	.free = spi_nfc_ooblayout_free,
};

static void spi_nfc_mtd_info_prepare(struct spi_nfc *spi_nfc)
{
	struct mtd_info *mtd = spi_mem_get_mtd();
	static u8 prepared;

	if (!mtd || prepared)
		return;

	prepared = 1;
	page_info->dev_cfg0.page_size = mtd->writesize;
	if (spi_nfc->disable_host_ecc) {
		page_info->host_cfg.n2m_cmd = N2M_RAW | mtd->writesize;
	} else {
		page_info->host_cfg.n2m_cmd =
			(DEFAULT_ECC_MODE & (~0x3F)) | mtd->writesize >> 9;
		mtd_set_ooblayout(mtd, &spi_nfc_ecc_ooblayout);
		mtd->oobavail = mtd_ooblayout_count_freebytes(mtd);
	}
	SPI_NFC_DEBUG("page_size = 0x%x\n", page_info->dev_cfg0.page_size);
}

static bool spi_nfc_is_buffer_dma_safe(const void *buffer)
{
	if ((uintptr_t)buffer % 8)
		return false;

	if (virt_addr_valid(buffer) && (!object_is_on_stack(buffer)))
		return true;

	return false;
}

static u8 *spi_nfc_get_dma_safe_buf(u8 *buf, u32 len, bool read)
{
	u8 *new_buf;

	if (spi_nfc_is_buffer_dma_safe(buf))
		return buf;

	new_buf = kzalloc(len, GFP_KERNEL);
	if (!read && new_buf)
		memcpy(new_buf, buf, len);

	return new_buf;
}

static void spi_nfc_put_dma_safe_buf(u8 *user_buf, u8 *new_buf,
				     u32 len, bool read)
{
	if (user_buf == new_buf)
		return;

	if (read)
		memcpy(user_buf, new_buf, len);

	kfree(new_buf);
}

static int spi_nfc_dma_buffer_setup(struct spi_nfc *spi_nfc, void *databuf,
				    int datalen, void *infobuf, int infolen,
				    enum dma_data_direction dir)
{
	int ret = 0;

	spi_nfc->daddr = dma_map_single(spi_nfc->dev, databuf, datalen, dir);
	ret = dma_mapping_error(spi_nfc->dev, spi_nfc->daddr);
	if (ret) {
		dev_err(spi_nfc->dev, "DMA mapping error\n");
		return ret;
	}

	if (infobuf) {
		spi_nfc->iaddr = dma_map_single(spi_nfc->dev, infobuf, infolen, dir);
		ret = dma_mapping_error(spi_nfc->dev, spi_nfc->iaddr);
		if (ret) {
			dev_err(spi_nfc->dev, "DMA mapping error\n");
			dma_unmap_single(spi_nfc->dev,
					 spi_nfc->daddr, datalen, dir);
			return ret;
		}
	}

	return ret;
}

static void spi_nfc_dma_buffer_release(struct spi_nfc *spi_nfc,
				       int datalen, int infolen,
				       enum dma_data_direction dir)
{
	dma_unmap_single(spi_nfc->dev, spi_nfc->daddr, datalen, dir);
	if (infolen)
		dma_unmap_single(spi_nfc->dev, spi_nfc->iaddr, infolen, dir);
}

static int spi_nfc_dma_xfer(struct spi_nfc *spi_nfc,
			    struct spi_device *spi,
			    unsigned long flags,
			    u8 *user_buf, int len, bool read)
{
	u32 page_size, n2m_cmd;
	bool raw, oob_only, auto_oob, oob_required;
	u8 *buf, *oob_pos, ecc_steps, *temp_buf;
	int ret = 0;

	oob_only = ((flags & SPI_XFER_OOB_ONLY) != 0);
	oob_required = ((flags & SPI_XFER_OOB) != 0);
	raw = ((flags & SPI_XFER_RAW) != 0);
	auto_oob = ((flags & SPI_XFER_AUTO_OOB) != 0);

	spi_nfc_mtd_info_prepare(spi_nfc);

	SPI_NFC_DEBUG("flags = %lx user_buf = %p len = 0x%x disable_host_ecc = %d  %s\n",
		       flags, user_buf, len, spi_nfc->disable_host_ecc,
		       (read) ? "read" : "write");

	page_size = page_info_get_page_size();
	oob_pos = (oob_only) ? user_buf : (user_buf + page_size);

	if (read ? (spi->mode & SPI_RX_QUAD) : (spi->mode & SPI_TX_QUAD))
		nfc_set_data_bus_width(2);
	else if (read ? (spi->mode & SPI_RX_DUAL) : (spi->mode & SPI_TX_DUAL))
		nfc_set_data_bus_width(1);
	else
		nfc_set_data_bus_width(0);

	if (raw || spi_nfc->disable_host_ecc) {
		buf = spi_nfc_get_dma_safe_buf(user_buf, len, read);
		temp_buf = user_buf;
		nfc_raw_size_ext_convert(len);
		n2m_cmd = (read) ? N2M_RAW : M2N_RAW;
		n2m_cmd |= (len & ((1 << NFC_RAW_CHUNK_SHIFT) - 1));
		ecc_steps = 1;
	} else {
		buf = spi_nfc->data_buf;
		temp_buf = spi_nfc->data_buf;
		n2m_cmd = nfc_recalculate_n2m_command(len, 1);
		ecc_steps = n2m_cmd & 0x3F;
		len = DATA_BUF_SIZE;
		if (!read) {
			n2m_cmd &= ~(N2M_RAW ^ M2N_RAW);
			spi_nfc_covert_buf_to_host(spi_nfc, user_buf,
						   oob_pos, ecc_steps,
						   oob_only, auto_oob,
						   oob_required);
		}
	}

	SPI_NFC_DEBUG("n2m_cmd = %x ecc_steps = %x\n", n2m_cmd, ecc_steps);

	spi_nfc_dma_buffer_setup(spi_nfc, buf, len,
				 spi_nfc->info_buf, OOB_BUF_SIZE,
				 (read) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	nfc_set_dma_mem_and_info((unsigned long)spi_nfc->daddr,
				 (unsigned long)spi_nfc->iaddr);

	ret = nfc_start_dma_and_wait_done(n2m_cmd);
	if (ret || !read) {
		spi_nfc_put_dma_safe_buf(temp_buf, buf, len, read);
		spi_nfc_dma_buffer_release(spi_nfc, len,
					   OOB_BUF_SIZE,
					   DMA_TO_DEVICE);
		return ret;
	}

	ret = nfc_wait_data_and_ecc_engine_done(spi_nfc->dev, spi_nfc->iaddr,
						(u64 *)spi_nfc->info_buf,
						ecc_steps, raw);
	if (ret) {
		SPI_NFC_DEBUG("ecc error dump:\n");
		DUMP_BUFFER(spi_nfc->info_buf, 64, 4, 16);
		DUMP_BUFFER(buf, page_size, page_size / 16, 16);
		return ret;
	}

	spi_nfc_put_dma_safe_buf(temp_buf, buf, len, read);
	spi_nfc_dma_buffer_release(spi_nfc, len,
				   OOB_BUF_SIZE,
				   DMA_FROM_DEVICE);

	if (raw || spi_nfc->disable_host_ecc) {
		DUMP_BUFFER(buf, len, len / 16, 16);
		return 0;
	}

	spi_nfc_covert_buf_to_user(spi_nfc, user_buf,
				   oob_pos, ecc_steps,
				   oob_only, auto_oob,
				   oob_required);
	return 0;
}

enum TRANSFER_STATE spi_nfc_get_current_handle_state(struct spi_transfer *xfer)
{
	enum TRANSFER_STATE xfer_state = TRANSFER_STATE_NONE;

	if (xfer->rx_nbits && !xfer->rx_buf)
		xfer_state = xfer->rx_nbits;
	else if (xfer->rx_buf)
		xfer_state = TRANSFER_STATE_DATAIN;
	else
		xfer_state = TRANSFER_STATE_DATAOUT;

	return xfer_state;
}

static int spi_nfc_transfer_one(struct spi_master *master,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	struct spi_nfc *spi_nfc = spi_master_get_devdata(master);
	u32 page_size = page_info_get_page_size();
	unsigned long flags;
	enum TRANSFER_STATE xfer_state;
	static bool cache_cmd_op;
	u8 *p = (u8 *)xfer->tx_buf, addr[4] = {0};
	int i;

	xfer_state = spi_nfc_get_current_handle_state(xfer);
	switch (xfer_state) {
	case TRANSFER_STATE_CMD:
		cache_cmd_op = (IS_CACHE_CMD(p[0])) ? true : false;
		return NFC_SEND_CMD(p[0]);
	case TRANSFER_STATE_DUMMY:
	case TRANSFER_STATE_ADDR:
		SPI_NFC_DEBUG("addr: %d %x %x %x\n",
			xfer->len, p[0], p[1], p[2]);
		if (cache_cmd_op && !spi_nfc->disable_host_ecc) {
			if (fls(page_size) <= 8)
				return -1;
			p[0] = p[1] & (~(1 << (fls(page_size) - 8)));
			p[1] = 0;
		}
		p += xfer->len;
		for (i = 0; i < xfer->len; i++)
			addr[i] = *(--p);
		return NFC_SEND_ADDR(addr, xfer->len);
	case TRANSFER_STATE_DATAOUT:
		SPI_NFC_DEBUG("xfer->len: %d\n", xfer->len);
		if (xfer->len <= SPINAND_MAX_ID_LEN + 2) {
			nfc_set_data_bus_width(0);
			return NFC_SEND_DATA_OUT(p, xfer->len);
		}
		flags = spi_mem_get_xfer_flag();
		return spi_nfc_dma_xfer(spi_nfc, spi, flags, p, xfer->len, false);
	case TRANSFER_STATE_DATAIN:
		SPI_NFC_DEBUG("xfer->len: %d\n", xfer->len);
		p = (u8 *)xfer->rx_buf;
		memset(p, 0, xfer->len);
		if (xfer->len <= SPINAND_MAX_ID_LEN + 2) {
			nfc_set_data_bus_width(0);
			return NFC_SEND_DATA_IN(p, xfer->len);
		}
		flags = spi_mem_get_xfer_flag();
		return spi_nfc_dma_xfer(spi_nfc, spi, flags, p, xfer->len, true);
	default:
		pr_info("%s %d\n", __func__, __LINE__);
		break;
	}
	return 0;
}

static int spi_nfc_clk_init(struct spi_nfc *spi_nfc,
			    struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct clk_init_data init = {0};
	const char *fix_div2_pll_name[1];
	u32 nand_clk_ctrl;
	int ret;

	spi_nfc->clk_gate = devm_clk_get(spi_nfc->dev, "gate");
	if (IS_ERR(spi_nfc->clk_gate)) {
		dev_err(spi_nfc->dev, "failed to get gate\n");
		return PTR_ERR(spi_nfc->clk_gate);
	}

	ret = clk_prepare_enable(spi_nfc->clk_gate);
	if (ret) {
		dev_err(spi_nfc->dev, "failed to enable gate\n");
		return ret;
	}

	spi_nfc->fix_div2_pll = devm_clk_get(spi_nfc->dev, "fdiv2pll");
	if (IS_ERR(spi_nfc->fix_div2_pll)) {
		dev_err(spi_nfc->dev, "failed to get fix pll\n");
		return PTR_ERR(spi_nfc->fix_div2_pll);
	}

	ret = clk_prepare_enable(spi_nfc->fix_div2_pll);
	if (ret) {
		dev_err(spi_nfc->dev, "failed to enable fix pll\n");
		return ret;
	}

	ret = of_property_read_u32(np, "nand_clk_ctrl", &nand_clk_ctrl);
	if (ret) {
		pr_info("%s %d,please config para item nand_clk_ctrl in dts\n",
				__func__, __LINE__);
		return ret;
	}
	spi_nfc->nand_clk_reg = devm_ioremap_nocache(&pdev->dev,
						     nand_clk_ctrl,
						     sizeof(int));

	/* init SD_EMMC_CLOCK to sane defaults w/min clock rate */
	writel(CLK_SELECT_NAND | readl(spi_nfc->nand_clk_reg), spi_nfc->nand_clk_reg);

	init.name = devm_kstrdup(spi_nfc->dev, "nfc#div", GFP_KERNEL);
	init.ops = &clk_divider_ops;
	fix_div2_pll_name[0] = __clk_get_name(spi_nfc->fix_div2_pll);
	init.parent_names = fix_div2_pll_name;
	init.num_parents = 1;
	spi_nfc->divider.reg = spi_nfc->nand_clk_reg;
	spi_nfc->divider.shift = CLK_DIV_SHIFT;
	spi_nfc->divider.width = CLK_DIV_WIDTH;
	spi_nfc->divider.hw.init = &init;
	spi_nfc->divider.flags =  CLK_DIVIDER_ONE_BASED |
		CLK_DIVIDER_ROUND_CLOSEST | CLK_DIVIDER_ALLOW_ZERO;
	spi_nfc->div_clk = devm_clk_register(spi_nfc->dev,
				&spi_nfc->divider.hw);
	if (IS_ERR(spi_nfc->div_clk))
		return PTR_ERR(spi_nfc->div_clk);

	ret = clk_prepare_enable(spi_nfc->div_clk);
	if (ret)
		dev_err(spi_nfc->dev, "pre enable spi nfc divider fail\n");

	return ret;
}

static void spi_nfc_disable_clk(struct spi_nfc *spi_nfc)
{
	clk_disable_unprepare(spi_nfc->div_clk);
	clk_disable_unprepare(spi_nfc->fix_div2_pll);
	clk_disable_unprepare(spi_nfc->clk_gate);
}

static int spi_nfc_prepare(struct spi_nfc *spi_nfc,
			   struct platform_device *pdev)
{
	unsigned long clk_rate;
	u8 *boot_info;
	int ret;

	ret = spi_nfc_clk_init(spi_nfc, pdev);
	if (ret)
		return ret;

	ret = spi_nfc_buffer_init(spi_nfc);
	if (ret)
		return ret;

	boot_info = devm_kzalloc(spi_nfc->dev,
				 MAX_BYTES_IN_BOOTINFO,
				 GFP_KERNEL);
	if (!boot_info)
		return -ENOMEM;

	page_info_pre_init(boot_info, PAGE_INFO_V3);
	page_info_initialize(DEFAULT_ECC_MODE, 0, 0);

	nfc_set_clock_and_timing(&clk_rate);

	ret = clk_set_rate(spi_nfc->div_clk, clk_rate);
	if (ret) {
		dev_err(spi_nfc->dev, "failed to set device clock\n");
		spi_nfc_disable_clk(spi_nfc);
	}

	return ret;
}

static int spi_nfc_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct spi_nfc *spi_nfc;
	void __iomem *base;
	int ret = 0, i;

	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_nfc));
	if (!master)
		return -ENOMEM;

	platform_set_drvdata(pdev, master);

	spi_nfc = spi_master_get_devdata(master);
	spi_nfc->dev = &pdev->dev;
#ifdef CONFIG_AML_SPI_NFC_DISABLE_HOST_ECC
	spi_nfc->disable_host_ecc = 1;
#endif
	for (i = 0; i < 2; i++) {
		base = devm_platform_ioremap_resource(pdev, i);
		if (IS_ERR(base)) {
			ret = PTR_ERR(base);
			goto out_err;
		}

		spi_nfc->regmap[i] = devm_regmap_init_mmio(spi_nfc->dev, base,
						&spi_nfc_regmap_config);
		if (IS_ERR(spi_nfc->regmap[i])) {
			ret = PTR_ERR(spi_nfc->regmap[i]);
			goto out_err;
		}
		nfc_regmap[i] = spi_nfc->regmap[i];
	}

	ret = spi_nfc_prepare(spi_nfc, pdev);
	if (ret)
		goto out_err;

	master->num_chipselect = 1;
	master->dev.of_node = pdev->dev.of_node;
	master->mode_bits = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	master->auto_runtime_pm = true;
	master->transfer_one = spi_nfc_transfer_one;
	master->min_speed_hz = 1000000;
	master->max_speed_hz = 200000000;

	pm_runtime_set_autosuspend_delay(spi_nfc->dev, 500);
	pm_runtime_use_autosuspend(spi_nfc->dev);
	pm_runtime_enable(spi_nfc->dev);

	ret = devm_spi_register_master(spi_nfc->dev, master);
	if (ret) {
		dev_err(spi_nfc->dev, "failed to register spi master\n");
		goto out_clk;
	}

	return 0;
out_clk:
	spi_nfc_disable_clk(spi_nfc);
	pm_runtime_disable(spi_nfc->dev);
out_err:
	spi_master_put(master);
	return ret;
}

static int spi_nfc_remove(struct platform_device *pdev)
{
	struct spi_nfc *spi_nfc = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);
	spi_nfc_disable_clk(spi_nfc);
	spi_nfc_buffer_free(spi_nfc);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int spi_nfc_suspend(struct device *dev)
{
	struct spi_nfc *spi_nfc = dev_get_drvdata(dev);
	int ret = 0;

	if (spi_nfc->master) {
		ret = spi_master_suspend(spi_nfc->master);
		if (ret)
			return ret;
	}

	if (!pm_runtime_suspended(dev))
		clk_disable_unprepare(spi_nfc->clk_gate);

	return 0;
}

static int spi_nfc_resume(struct device *dev)
{
	struct spi_nfc *spi_nfc = dev_get_drvdata(dev);
	int ret = 0;

	if (!pm_runtime_suspended(dev)) {
		ret = clk_prepare_enable(spi_nfc->clk_gate);
		if (ret)
			return ret;
	}

	if (spi_nfc->master) {
		ret = spi_master_resume(spi_nfc->master);
		if (ret)
			clk_disable_unprepare(spi_nfc->clk_gate);
	}

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int spi_nfc_runtime_suspend(struct device *dev)
{
	struct spi_nfc *spi_nfc = dev_get_drvdata(dev);

	clk_disable_unprepare(spi_nfc->clk_gate);

	return 0;
}

static int spi_nfc_runtime_resume(struct device *dev)
{
	struct spi_nfc *spi_nfc = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(spi_nfc->clk_gate);

	return ret;
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops spi_nfc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(spi_nfc_suspend, spi_nfc_resume)
	SET_RUNTIME_PM_OPS(spi_nfc_runtime_suspend,
			   spi_nfc_runtime_resume,
			   NULL)
};

static const struct of_device_id spi_nfc_dt_match[] = {
	{ .compatible = "amlogic,spi-nfc", },
	{ },
};
MODULE_DEVICE_TABLE(of, spi_nfc_dt_match);

struct platform_driver spi_nfc_driver = {
	.probe	= spi_nfc_probe,
	.remove	= spi_nfc_remove,
	.driver	= {
		.name		= "spi-nfc",
		.of_match_table	= of_match_ptr(spi_nfc_dt_match),
		//.pm		= &spi_nfc_pm_ops,
	},
};

#ifndef MODULE
module_platform_driver(spi_nfc_driver);
#endif
MODULE_AUTHOR("Amlogic R&D");
MODULE_DESCRIPTION("Amlogic SPI NFC driver");
MODULE_LICENSE("GPL v2");
