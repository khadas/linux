// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/mtd.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/sched/task_stack.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/aml_mtd_nand.h>
#include <linux/mtd/partitions.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/gki_module.h>
#include <linux/reboot.h>

struct mtd_info *aml_mtd_info[NAND_MAX_DEVICE];
u8 aml_mtd_devnum;

enum {
	NFC_ECC_NONE		= 0,
	NFC_ECC_BCH8_512,
	NFC_ECC_BCH8_1K,
	NFC_ECC_BCH24_1K,
	NFC_ECC_BCH30_1K,
	NFC_ECC_BCH40_1K,
	NFC_ECC_BCH50_1K,
	NFC_ECC_BCH60_1K,
	NFC_ECC_BCH_SHORT,
};

#define MESON_ECC_DATA(b, s, st)	{.bch = (b),	.strength = (s),\
					 .step_size = (st)}

static int nand_get_device(struct nand_chip *chip)
{
	mutex_lock(&chip->lock);
	if (chip->suspended) {
		mutex_unlock(&chip->lock);
		return -EBUSY;
	}

	mutex_lock(&chip->controller->lock);
	return 0;
}

static void nand_release_device(struct nand_chip *chip)
{
	/* Release the controller and the chip */
	mutex_unlock(&chip->controller->lock);
	mutex_unlock(&chip->lock);
}

static struct meson_nand_ecc meson_ecc[] = {
	MESON_ECC_DATA(NFC_ECC_BCH8_512, 8, SZ_512),
	MESON_ECC_DATA(NFC_ECC_BCH8_1K, 8, SZ_1K),
	MESON_ECC_DATA(NFC_ECC_BCH24_1K, 24, SZ_1K),
	MESON_ECC_DATA(NFC_ECC_BCH30_1K, 30, SZ_1K),
	MESON_ECC_DATA(NFC_ECC_BCH40_1K, 40, SZ_1K),
	MESON_ECC_DATA(NFC_ECC_BCH50_1K, 50, SZ_1K),
	MESON_ECC_DATA(NFC_ECC_BCH60_1K, 60, SZ_1K),
};

static int meson_nand_calc_ecc_bytes(int step_size, int strength)
{
	int ecc_bytes;

	if (step_size == 512 && strength == 8)
		return ECC_PARITY_BCH8_512B;

	ecc_bytes = DIV_ROUND_UP(strength * fls(step_size * 8), 8);
	ecc_bytes = ALIGN(ecc_bytes, 2);

	return ecc_bytes;
}

NAND_ECC_CAPS_SINGLE(meson_1K_ecc_caps,
		     meson_nand_calc_ecc_bytes, 1024, 8, 24, 30, 40, 50, 60);
NAND_ECC_CAPS_SINGLE(meson_512_ecc_caps,
		     meson_nand_calc_ecc_bytes, 512, 8);

static struct meson_nfc_nand_chip *to_meson_nand(struct nand_chip *nand)
{
	return container_of(nand, struct meson_nfc_nand_chip, nand);
}

static void meson_nfc_select_chip(struct nand_chip *nand, int chip)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	int ret, value;

	if (chip < 0 || WARN_ON_ONCE(chip >= meson_chip->nsels))
		return;

	nfc->param.chip_select = meson_chip->sels[chip] ? NAND_CE1 : NAND_CE0;
	nfc->param.rb_select = nfc->param.chip_select;
	nfc->timing.twb = meson_chip->twb;
	nfc->timing.tadl = meson_chip->tadl;
	nfc->timing.tbers_max = meson_chip->tbers_max;

	if (nfc->clk_rate != meson_chip->clk_rate) {
		ret = clk_set_rate(nfc->nand_div_clk, meson_chip->clk_rate);
		if (ret) {
			dev_err(nfc->dev, "failed to set clock rate\n");
			return;
		}

		nfc->clk_rate = meson_chip->clk_rate;
	}
	if (nfc->bus_timing != meson_chip->bus_timing) {
		value = (NFC_CLK_CYCLE - 1) | (meson_chip->bus_timing << 5);
		writel(value, nfc->reg_base + NFC_REG_CFG);
		writel((1 << 31), nfc->reg_base + NFC_REG_CMD);
		nfc->bus_timing =  meson_chip->bus_timing;
	}
}

static void meson_nfc_cmd_idle(struct meson_nfc *nfc, u32 time)
{
	writel(nfc->param.chip_select | NFC_CMD_IDLE | (time & 0x3ff),
	       nfc->reg_base + NFC_REG_CMD);
}

static void meson_nfc_cmd_seed(struct meson_nfc *nfc, u32 seed)
{
	writel(NFC_CMD_SEED | (0xc2 + (seed & 0x7fff)),
	       nfc->reg_base + NFC_REG_CMD);
}

static void meson_nfc_cmd_access(struct nand_chip *nand, int raw, bool dir,
				 int scrambler)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc *nfc = nand_get_controller_data(mtd_to_nand(mtd));
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	u32 bch = meson_chip->bch_mode, cmd;
	int len = mtd->writesize, pages;
	int dma_unit_size = nand->ecc.size;

	if (raw) {
		len = mtd->writesize + mtd->oobsize;
		cmd = (len & GENMASK(5, 0)) | (scrambler << 19) | DMA_DIR(dir);
		writel(cmd, nfc->reg_base + NFC_REG_CMD);
		return;
	}

	if (meson_chip->bch_mode == NFC_ECC_NONE) {
		pages = 1;
	} else if (meson_chip->bch_mode == NFC_ECC_BCH_SHORT) {
		dma_unit_size = (nand->ecc.size >> 3);
		pages = len / nand->ecc.size;
	} else {
		pages = len / nand->ecc.size;
	}

	if (meson_chip->bch_mode == NFC_ECC_BCH_SHORT)
		bch = meson_chip->bch_info;
	/*add infopage0 operation(shortmode)*/
	cmd = CMDRWGEN(DMA_DIR(dir), scrambler, bch,
		       ((meson_chip->bch_mode == NFC_ECC_BCH_SHORT) ?
		       NFC_CMD_SHORTMODE_ENABLE : NFC_CMD_SHORTMODE_DISABLE),
		       dma_unit_size, pages);

	writel(cmd, nfc->reg_base + NFC_REG_CMD);
}

static void meson_nfc_drain_cmd(struct meson_nfc *nfc)
{
	/*
	 * Insert two commands to make sure all valid commands are finished.
	 *
	 * The Nand flash controller is designed as two stages pipleline -
	 *  a) fetch and b) excute.
	 * There might be cases when the driver see command queue is empty,
	 * but the Nand flash controller still has two commands buffered,
	 * one is fetched into NFC request queue (ready to run), and another
	 * is actively executing. So pushing 2 "IDLE" commands guarantees that
	 * the pipeline is emptied.
	 */
	meson_nfc_cmd_idle(nfc, 0);
	meson_nfc_cmd_idle(nfc, 0);
}

static int meson_nfc_wait_cmd_finish(struct meson_nfc *nfc,
				     unsigned int timeout_ms)
{
	u32 cmd_size = 0;
	int ret;

	/* wait cmd fifo is empty */
	ret = readl_relaxed_poll_timeout(nfc->reg_base + NFC_REG_CMD, cmd_size,
					 !NFC_CMD_GET_SIZE(cmd_size),
					 50, timeout_ms * 1000);
	if (ret)
		dev_err(nfc->dev, "wait for empty CMD FIFO time out\n");

	return ret;
}

static int meson_nfc_wait_dma_finish(struct meson_nfc *nfc)
{
	meson_nfc_drain_cmd(nfc);

	return meson_nfc_wait_cmd_finish(nfc, DMA_BUSY_TIMEOUT);
}

static u8 *meson_nfc_oob_ptr(struct nand_chip *nand, int i)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	int len;

	len = nand->ecc.size * (i + 1) + (nand->ecc.bytes + 2) * i;

	return meson_chip->data_buf + len;
}

static u8 *meson_nfc_data_ptr(struct nand_chip *nand, int i)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	int len, temp;

	temp = nand->ecc.size + nand->ecc.bytes;
	len = (temp + 2) * i;

	return meson_chip->data_buf + len;
}

static void meson_nfc_get_data_oob(struct nand_chip *nand,
				   u8 *buf, u8 *oobbuf)
{
	int i, oob_len = 0;
	u8 *dsrc, *osrc;

	oob_len = nand->ecc.bytes + 2;
	for (i = 0; i < nand->ecc.steps; i++) {
		if (buf) {
			dsrc = meson_nfc_data_ptr(nand, i);
			memcpy(buf, dsrc, nand->ecc.size);
			buf += nand->ecc.size;
		}
		osrc = meson_nfc_oob_ptr(nand, i);
		memcpy(oobbuf, osrc, oob_len);
		oobbuf += oob_len;
	}
}

static void meson_nfc_set_data_oob(struct nand_chip *nand,
				   const u8 *buf, u8 *oobbuf)
{
	int i, oob_len = 0;
	u8 *dsrc, *osrc;

	oob_len = nand->ecc.bytes + 2;
	for (i = 0; i < nand->ecc.steps; i++) {
		if (buf) {
			dsrc = meson_nfc_data_ptr(nand, i);
			memcpy(dsrc, buf, nand->ecc.size);
			buf += nand->ecc.size;
		}
		osrc = meson_nfc_oob_ptr(nand, i);
		memcpy(osrc, oobbuf, oob_len);
		oobbuf += oob_len;
	}
}

static inline u8 meson_nfc_read_byte(struct meson_nfc *nfc)
{
	u32 cmd;

	cmd = nfc->param.chip_select | NFC_CMD_DRD | 0;
	writel(cmd, nfc->reg_base + NFC_REG_CMD);
	meson_nfc_wait_dma_finish(nfc);

	return readb(nfc->reg_base + NFC_REG_BUF);
}

static int meson_nfc_queue_rb(struct nand_chip *nand, int timeout_ms)
{
	u32 cmd, cfg;
	int ret = 0;
	u8 status = 0;
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	unsigned int time_out_cnt = 0;

	if (!nfc->param_from_dts.disa_irq_hand) {
		meson_nfc_cmd_idle(nfc, nfc->timing.twb);
		meson_nfc_wait_dma_finish(nfc);
		cfg = readl(nfc->reg_base + NFC_REG_CFG);
		cfg |= NFC_RB_IRQ_EN;
		writel(cfg, nfc->reg_base + NFC_REG_CFG);
		reinit_completion(&nfc->completion);

		/* use the max erase time as the maximum clock for waiting R/B */
		cmd = NFC_CMD_RB | NFC_CMD_RB_INT
			| nfc->param.chip_select | nfc->timing.tbers_max;
		writel(cmd, nfc->reg_base + NFC_REG_CMD);
		ret = wait_for_completion_timeout(&nfc->completion,
						  msecs_to_jiffies(2000));
		if (ret == 0) {
			pr_info("ERROR: waiting nand ready timeout\n");
			ret = -1;
		}
	} else {
		meson_nfc_wait_dma_finish(nfc);

		writel(nfc->param.chip_select | NFC_CMD_CLE | NAND_CMD_STATUS,
			nfc->reg_base + NFC_REG_CMD);
		writel(nfc->param.chip_select | NFC_CMD_IDLE | 2,
			nfc->reg_base + NFC_REG_CMD);

		meson_nfc_wait_dma_finish(nfc);

		do {
			status = meson_nfc_read_byte(nfc);
			if (status & NAND_STATUS_READY)
				break;
			usleep_range(20, 25);
		} while (time_out_cnt++ <= 0x2000);

		if (time_out_cnt > 0x2000) {
			pr_info("nand status error\n");
			return -1;
		}
	}

	return ret;
}

static void meson_nfc_set_user_byte(struct nand_chip *nand, u8 *oob_buf)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	__le64 *info;
	int i, count;

	for (i = 0, count = 0; i < nand->ecc.steps; i++, count += 2) {
		info = &meson_chip->info_buf[i];
		*info |= oob_buf[count];
		*info |= oob_buf[count + 1] << 8;
	}
}

static void meson_nfc_get_user_byte(struct nand_chip *nand, u8 *oob_buf)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	__le64 *info;
	int i, count;

	for (i = 0, count = 0; i < nand->ecc.steps; i++, count += 2) {
		info = &meson_chip->info_buf[i];
		oob_buf[count] = *info;
		oob_buf[count + 1] = *info >> 8;
	}
}

static int meson_nfc_ecc_correct(struct nand_chip *nand, u32 *bitflips,
				 u64 *correct_bitmap)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	__le64 *info;
	int ret = 0, i;

	for (i = 0; i < nand->ecc.steps; i++) {
		info = &meson_chip->info_buf[i];
		if (ECC_ERR_CNT(*info) != ECC_UNCORRECTABLE) {
			mtd->ecc_stats.corrected += ECC_ERR_CNT(*info);
			*bitflips = max_t(u32, *bitflips, ECC_ERR_CNT(*info));
			*correct_bitmap |= 1 >> i;
			continue;
		}
		if ((nand->options & NAND_NEED_SCRAMBLING) &&
		    ECC_ZERO_CNT(*info) < nand->ecc.strength) {
			mtd->ecc_stats.corrected += ECC_ZERO_CNT(*info);
			*bitflips = max_t(u32, *bitflips,
					  ECC_ZERO_CNT(*info));
			ret = ECC_CHECK_RETURN_FF;
		} else {
			ret = -EBADMSG;
		}
	}
	return ret;
}

static int meson_nfc_dma_buffer_setup(struct nand_chip *nand, void *databuf,
				      int datalen, void *infobuf, int infolen,
				      enum dma_data_direction dir)
{
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	u32 cmd;
	int ret = 0;

	nfc->daddr = dma_map_single(nfc->dev, databuf, datalen, dir);
	ret = dma_mapping_error(nfc->dev, nfc->daddr);
	if (ret) {
		dev_err(nfc->dev, "DMA mapping error\n");
		return ret;
	}
	cmd = GENCMDDADDRL(NFC_CMD_ADL, nfc->daddr);
	writel(cmd, nfc->reg_base + NFC_REG_CMD);

	cmd = GENCMDDADDRH(NFC_CMD_ADH, nfc->daddr);
	writel(cmd, nfc->reg_base + NFC_REG_CMD);

	if (infobuf) {
		nfc->iaddr = dma_map_single(nfc->dev, infobuf, infolen, dir);
		ret = dma_mapping_error(nfc->dev, nfc->iaddr);
		if (ret) {
			dev_err(nfc->dev, "DMA mapping error\n");
			dma_unmap_single(nfc->dev,
					 nfc->daddr, datalen, dir);
			return ret;
		}
		cmd = GENCMDIADDRL(NFC_CMD_AIL, nfc->iaddr);
		writel(cmd, nfc->reg_base + NFC_REG_CMD);

		cmd = GENCMDIADDRH(NFC_CMD_AIH, nfc->iaddr);
		writel(cmd, nfc->reg_base + NFC_REG_CMD);
	}

	return ret;
}

static void meson_nfc_dma_buffer_release(struct nand_chip *nand,
					 int datalen, int infolen,
					 enum dma_data_direction dir)
{
	struct meson_nfc *nfc = nand_get_controller_data(nand);

	dma_unmap_single(nfc->dev, nfc->daddr, datalen, dir);
	if (infolen)
		dma_unmap_single(nfc->dev, nfc->iaddr, infolen, dir);
}

static int meson_nfc_read_buf(struct nand_chip *nand, u8 *buf, int len)
{
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	int ret = 0;
	u32 cmd;
	u8 *info;

	info = kzalloc(PER_INFO_BYTE, GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ret = meson_nfc_dma_buffer_setup(nand, buf, len, info,
					 PER_INFO_BYTE, DMA_FROM_DEVICE);
	if (ret)
		goto out;

	cmd = NFC_CMD_N2M | (len & GENMASK(5, 0));
	writel(cmd, nfc->reg_base + NFC_REG_CMD);
	meson_nfc_wait_dma_finish(nfc);
	meson_nfc_dma_buffer_release(nand, len, PER_INFO_BYTE, DMA_FROM_DEVICE);

out:
	kfree(info);

	return ret;
}

static int meson_nfc_write_buf(struct nand_chip *nand, u8 *buf, int len)
{
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	int ret = 0;
	u32 cmd;

	ret = meson_nfc_dma_buffer_setup(nand, buf, len, NULL,
					 0, DMA_TO_DEVICE);
	if (ret)
		return ret;

	cmd = NFC_CMD_M2N | (len & GENMASK(5, 0));
	writel(cmd, nfc->reg_base + NFC_REG_CMD);

	meson_nfc_wait_dma_finish(nfc);
	meson_nfc_dma_buffer_release(nand, len, 0, DMA_TO_DEVICE);

	return ret;
}

static int meson_nfc_rw_cmd_prepare_and_execute(struct nand_chip *nand,
						int page, bool in)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	const struct nand_sdr_timings *sdr =
			nand_get_sdr_timings(&nand->data_interface);
	u32 *addrs = nfc->cmdfifo.rw.addrs;
	u32 cs = nfc->param.chip_select;
	u32 cmd0, cmd_num, row_start;
	int ret = 0, i;

	cmd_num = sizeof(struct nand_rw_cmd) / sizeof(int);

	cmd0 = in ? NAND_CMD_READ0 : NAND_CMD_SEQIN;
	nfc->cmdfifo.rw.cmd0 = cs | NFC_CMD_CLE | cmd0;

	addrs[0] = cs | NFC_CMD_ALE | 0;
	if (mtd->writesize <= 512) {
		cmd_num--;
		row_start = 1;
	} else {
		addrs[1] = cs | NFC_CMD_ALE | 0;
		row_start = 2;
	}

	addrs[row_start] = cs | NFC_CMD_ALE | ROW_ADDER(page, 0);
	addrs[row_start + 1] = cs | NFC_CMD_ALE | ROW_ADDER(page, 1);

	if (nand->options & NAND_ROW_ADDR_3)
		addrs[row_start + 2] =
			cs | NFC_CMD_ALE | ROW_ADDER(page, 2);
	else
		cmd_num--;

	/* subtract cmd1 */
	cmd_num--;

	for (i = 0; i < cmd_num; i++)
		writel_relaxed(nfc->cmdfifo.cmd[i],
			       nfc->reg_base + NFC_REG_CMD);

	if (in) {
		nfc->cmdfifo.rw.cmd1 = cs | NFC_CMD_CLE | NAND_CMD_READSTART;
		writel(nfc->cmdfifo.rw.cmd1, nfc->reg_base + NFC_REG_CMD);
		meson_nfc_queue_rb(nand, PSEC_TO_MSEC(sdr->tR_max));

		nfc->cmdfifo.rw.cmd1 = nfc->param.chip_select | NFC_CMD_CLE |
				NAND_CMD_READ0;
		writel(nfc->cmdfifo.rw.cmd1, nfc->reg_base + NFC_REG_CMD);
	} else {
		meson_nfc_cmd_idle(nfc, nfc->timing.tadl);
	}

	return ret;
}

static int meson_nfc_write_page_sub(struct nand_chip *nand,
				    int page, int raw)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	const struct nand_sdr_timings *sdr =
		nand_get_sdr_timings(&nand->data_interface);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	int data_len, info_len;
	u32 cmd;
	int ret;

	meson_nfc_select_chip(nand, nand->cur_cs);

	data_len =  mtd->writesize + mtd->oobsize;
	info_len = nand->ecc.steps * PER_INFO_BYTE;

	ret = meson_nfc_rw_cmd_prepare_and_execute(nand, page, DIRWRITE);
	if (ret)
		return ret;

	ret = meson_nfc_dma_buffer_setup(nand, meson_chip->data_buf,
					 data_len, meson_chip->info_buf,
					 info_len, DMA_TO_DEVICE);
	if (ret)
		return ret;

	if (nand->options & NAND_NEED_SCRAMBLING) {
		meson_nfc_cmd_seed(nfc, page);
		meson_nfc_cmd_access(nand, raw, DIRWRITE,
				     NFC_CMD_SCRAMBLER_ENABLE);
	} else {
		meson_nfc_cmd_access(nand, raw, DIRWRITE,
				     NFC_CMD_SCRAMBLER_DISABLE);
	}

	cmd = nfc->param.chip_select | NFC_CMD_CLE | NAND_CMD_PAGEPROG;
	writel(cmd, nfc->reg_base + NFC_REG_CMD);
	meson_nfc_queue_rb(nand, PSEC_TO_MSEC(sdr->tPROG_max));
	meson_nfc_dma_buffer_release(nand, data_len, info_len, DMA_TO_DEVICE);

	return ret;
}

static int meson_nfc_write_page_raw(struct nand_chip *nand, const u8 *buf,
				    int oob_required, int page)
{
	u8 *oob_buf = nand->oob_poi;

	meson_nfc_set_data_oob(nand, buf, oob_buf);

	return meson_nfc_write_page_sub(nand, page, 1);
}

static void meson_info_page0_prepare(struct nand_chip *nand, u8 *page0_buf)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct meson_nfc *nfc =
		nand_get_controller_data(mtd_to_nand(aml_mtd_info[1]));
	u32 pages_per_blk_shift, bbt_size;
	int each_boot_pages, boot_num, bbt_pages;
	u32 configure_data;
	struct _nand_page0 *p_nand_page0 = NULL;
	struct _nand_page0_sc2 *p_nand_page0_sc2 = NULL;
	struct _ext_info *p_ext_info = NULL;
	struct _fip_info *p_fip_info = NULL;
	struct _nand_setup_sc2 *p_nand_setup_sc2 = NULL;
	struct _nand_setup *p_nand_setup = NULL;
	u8 dir = 0;

	pages_per_blk_shift = nand->phys_erase_shift - nand->page_shift;
	bbt_size = nfc->rsv->bbt->size;

	if (nfc->param_from_dts.bl_mode) {
		boot_num = nfc->param_from_dts.fip_copies;
		each_boot_pages = nfc->param_from_dts.fip_size / mtd->writesize;
	} else {
		boot_num = 1;
		each_boot_pages = BOOT_TOTAL_PAGES / boot_num;
	}

	memset(page0_buf, 0x0, mtd->writesize);
	/* must be 1, rom will do security check */
	dir = 1;

	configure_data = CMDRWGEN(DMA_DIR(dir),
				  nand->options & NAND_NEED_SCRAMBLING,
				  meson_chip->bch_mode,
				  NFC_CMD_SHORTMODE_DISABLE,
				  nand->ecc.size >> 3,
				  nand->ecc.steps);
	if (nfc->data->bl2ex_mode) {
		p_nand_page0_sc2 = (struct _nand_page0_sc2 *)page0_buf;
		p_nand_setup_sc2 = &p_nand_page0_sc2->nand_setup;
		p_ext_info = &p_nand_page0_sc2->ext_info;
		p_fip_info = &p_nand_page0_sc2->fip_info;
		p_nand_setup_sc2->cfg.d32 = configure_data;
		p_nand_setup_sc2->cfg.b.page_list = 0;
		p_nand_setup_sc2->cfg.b.new_type = 0;
		pr_info("sc2 cfg.d32 0x%x\n", p_nand_setup_sc2->cfg.d32);
	} else {
		p_nand_page0 = (struct _nand_page0 *)page0_buf;
		p_nand_setup = &p_nand_page0->nand_setup;
		p_ext_info = &p_nand_page0->ext_info;
		p_fip_info = &p_nand_page0->fip_info;
		p_nand_setup->cfg.d32 = configure_data | (1 << 23) |
		(1 << 22);
		pr_info("cfg.d32 0x%x\n", p_nand_setup->cfg.d32);
		p_nand_setup->id = 0;
		p_nand_setup->max = 0;

		memset(p_nand_page0->page_list, 0, NAND_PAGELIST_CNT);
	}
	p_ext_info->read_info = meson_chip->nsels;
	p_ext_info->pages_per_blk = mtd->erasesize /
				    mtd->writesize;
	p_ext_info->ce_mask = 0x01;//only ce0 is enabled
	p_ext_info->xlc = 1;
	p_ext_info->boot_num = boot_num;
	p_ext_info->each_boot_pages = each_boot_pages;
	bbt_pages = (bbt_size + mtd->writesize - 1) /
		    mtd->writesize;
	p_ext_info->bbt_occupy_pages = bbt_pages;
	p_ext_info->bbt_start_block =
		(BOOT_TOTAL_PAGES >> pages_per_blk_shift) +
		NAND_GAP_BLOCK_NUM;
	if (nfc->param_from_dts.bl_mode) {
		p_fip_info->version = 1;
		p_fip_info->mode = NAND_FIPMODE_DISCRETE;
		p_fip_info->fip_start = BOOT_TOTAL_PAGES +
			NAND_RSV_BLOCK_NUM * p_ext_info->pages_per_blk;
		pr_info("fip ver %d, mode %d fip start 0x%x\n",
			p_fip_info->version, p_fip_info->mode,
			p_fip_info->fip_start);
	}
	pr_info("pages_per_blk: 0x%x, bbt_pages: 0x%x\n",
		p_ext_info->pages_per_blk, bbt_pages);
	pr_info("boot_num: %d each_boot_pages: %d\n",
		boot_num, each_boot_pages);
}

static int meson_nfc_write_page_hwecc(struct nand_chip *nand,
				      const u8 *buf, int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	u8 *oob_buf = nand->oob_poi;

	memcpy(meson_chip->data_buf, buf, mtd->writesize);
	memset(meson_chip->info_buf, 0, nand->ecc.steps * PER_INFO_BYTE);
	meson_nfc_set_user_byte(nand, oob_buf);

	return meson_nfc_write_page_sub(nand, page, 0);
}

static int meson_nfc_boot_write_page_hwecc(struct nand_chip *nand,
					   const u8 *buf,
					   int oob_required,
					   int page)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	u8 boot_num;
	int each_boot_pages, ecc_size, bch_mode;
	u8 *oob_buf = nand->oob_poi;
	int i;
	u64 ofs, tmp;
	u32 remainder;

	if (nfc->param_from_dts.bl_mode)
		boot_num = 8;
	else
		boot_num = 1;
	each_boot_pages = BOOT_TOTAL_PAGES / boot_num;

	if (page >= BOOT_TOTAL_PAGES)
		return 0;

	ecc_size = nand->ecc.size;
	bch_mode = meson_chip->bch_mode;

	for (i = 0; i < mtd->oobavail; i += 2) {
		oob_buf[i] = 0x55;
		oob_buf[i + 1] = 0xaa;
	}

	memset(meson_chip->info_buf, 0, nand->ecc.steps * PER_INFO_BYTE);
	meson_nfc_set_user_byte(nand, oob_buf);

	if (page % each_boot_pages == 0) {
		meson_info_page0_prepare(nand, nand->data_buf);
		nand->options |= NAND_NEED_SCRAMBLING; //setting randomizer

		if (meson_chip->bch_mode != NFC_ECC_BCH_SHORT) {
			meson_chip->bch_mode = NFC_ECC_BCH_SHORT;
			nand->ecc.size = NAND_ECC_UNIT_SHORT;
		}
		memcpy(meson_chip->data_buf, nand->data_buf, mtd->writesize);
		meson_nfc_write_page_sub(nand, page, 0);

		nand->ecc.size = ecc_size;
		meson_chip->bch_mode = bch_mode;
		nand->options &= ~NAND_NEED_SCRAMBLING;
	}

	page++;

WRITE_BAD_BLOCK:
	ofs = ((u64)page << nand->page_shift);
	tmp = ofs;
	div_u64_rem(tmp, mtd->erasesize, &remainder);
	if (!remainder) {
		//todo nand bb ckeck
		nand_release_device(nand);
		if (mtd->_block_isbad(mtd, ofs)) {
			nand_get_device(nand);
			page +=
				1 << (nand->phys_erase_shift -
				nand->page_shift);
			goto WRITE_BAD_BLOCK;
		}
		nand_get_device(nand);
	}
	memcpy(meson_chip->data_buf, buf, mtd->writesize);
	meson_nfc_write_page_sub(nand, page, 0);//fixed it
	return 0;
}

static void meson_nfc_check_ecc_pages_valid(struct meson_nfc *nfc,
					    struct nand_chip *nand, int raw)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	__le64 *info;
	u32 neccpages;
	int ret, cnt = 100000;
	int info_len = nand->ecc.steps * PER_INFO_BYTE;

	neccpages = raw ? 1 : nand->ecc.steps;
	info = &meson_chip->info_buf[neccpages - 1];
	do {
		usleep_range(10, 15);
		/* info is updated by nfc dma engine*/
		smp_rmb();
		dma_sync_single_for_cpu(nfc->dev, nfc->iaddr, info_len, DMA_FROM_DEVICE);
		ret = *info & ECC_COMPLETE;
		if (ret)
			return;

	} while (cnt--);

	dev_err(nfc->dev, "NAND ECC timeout, reboot! 0x%llx, 0x%x\n", *info, nand->ecc.steps);
	dump_stack();
	kernel_restart(NULL);
}

static int meson_nfc_read_page_sub(struct nand_chip *nand,
				   int page, int raw)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	int data_len, info_len;
	int ret;

	meson_nfc_select_chip(nand, nand->cur_cs);

	data_len =  mtd->writesize + mtd->oobsize;
	info_len = nand->ecc.steps * PER_INFO_BYTE;

	memset(meson_chip->info_buf, 0, info_len);
	ret = meson_nfc_rw_cmd_prepare_and_execute(nand, page, DIRREAD);
	if (ret)
		return ret;

	ret = meson_nfc_dma_buffer_setup(nand, meson_chip->data_buf,
					 data_len, meson_chip->info_buf,
					 info_len, DMA_FROM_DEVICE);
	if (ret)
		return ret;

	if (nand->options & NAND_NEED_SCRAMBLING) {
		meson_nfc_cmd_seed(nfc, page);
		meson_nfc_cmd_access(nand, raw, DIRREAD,
				     NFC_CMD_SCRAMBLER_ENABLE);
	} else {
		meson_nfc_cmd_access(nand, raw, DIRREAD,
				     NFC_CMD_SCRAMBLER_DISABLE);
	}

	ret = meson_nfc_wait_dma_finish(nfc);

	meson_nfc_check_ecc_pages_valid(nfc, nand, raw);
	meson_nfc_dma_buffer_release(nand, data_len, info_len, DMA_FROM_DEVICE);

	return ret;
}

static int meson_nfc_read_page_raw(struct nand_chip *nand, u8 *buf,
				   int oob_required, int page)
{
	u8 *oob_buf = nand->oob_poi;
	int ret;

	ret = meson_nfc_read_page_sub(nand, page, 1);
	if (ret)
		return ret;

	meson_nfc_get_data_oob(nand, buf, oob_buf);

	return 0;
}

/**
 * meson_nfc_read_page_hwecc -read page data and ecc check
 *
 * return a negative nuumber -ENODATA or bitflips
 * -ENODATA: read failed
 * bitflips: read success
 */
static int meson_nfc_read_page_hwecc(struct nand_chip *nand, u8 *buf,
				     int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	u64 correct_bitmap = 0;
	u32 bitflips = 0;
	u8 *oob_buf = nand->oob_poi;
	int ret, pages_per_blk_shift;

	ret = meson_nfc_read_page_sub(nand, page, 0);
	if (ret)
		return -ENODATA;

	meson_nfc_get_user_byte(nand, oob_buf);
	ret = meson_nfc_ecc_correct(nand, &bitflips, &correct_bitmap);
	if (ret == ECC_CHECK_RETURN_FF) {
		if (buf)
			memset(buf, 0xff, mtd->writesize);
		memset(oob_buf, 0xff, mtd->oobsize);
	} else if (ret < 0) {
		mtd->ecc_stats.failed++;
		pages_per_blk_shift = (nand->phys_erase_shift - nand->page_shift);
		pr_err("read ecc failed here at page:%d, blk:%d\n",
			page, (page >> pages_per_blk_shift));
	} else if (buf && buf != meson_chip->data_buf) {
		memcpy(buf, meson_chip->data_buf, mtd->writesize);
	}

	return bitflips;
}

static int meson_nfc_boot_read_page_hwecc(struct nand_chip *nand, u8 *buf,
					  int oob_required,
					  int page)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	struct _nand_page0 *p_nand_page0 = NULL;
	struct _nand_page0_sc2 *p_nand_page0_sc2 = NULL;
	struct _ext_info *p_ext_info = NULL;
	struct _nand_setup *p_nand_setup = NULL;
	struct _nand_setup_sc2 *p_nand_setup_sc2 = NULL;

	u8 boot_num, dir;
	u32 each_boot_pages, remainder;
	int ret = 0, configure_data_w, pages_per_blk_w, configure_data;
	//u32 nand_page_size = nand->ecc.size * nand->ecc.steps;
	u64 ofs, tmp;
	int ecc_size, bch_mode, pages_per_blk, real_page;

	if (nfc->param_from_dts.bl_mode)
		boot_num = 8;
	else
		boot_num = 1;

	each_boot_pages = BOOT_TOTAL_PAGES / boot_num;
	if (page >= BOOT_TOTAL_PAGES) {
		memset(buf, 0, (1 << nand->page_shift));
		pr_info("nand read out of bootloader, failed page: %d\n",
			page);
		return ret;
	}

	ecc_size = nand->ecc.size;
	bch_mode = meson_chip->bch_mode;
	dir = 1;

	if (page % each_boot_pages == 0) {
		if (meson_chip->bch_mode == NFC_ECC_BCH_SHORT)
			configure_data_w =
			CMDRWGEN(DMA_DIR(dir),
				 !!(nand->options & NAND_NEED_SCRAMBLING),
				 NFC_ECC_BCH60_1K,
				 NFC_CMD_SHORTMODE_ENABLE,
				 nand->ecc.size >> 3,
				 nand->ecc.steps);
		else
			configure_data_w =
			CMDRWGEN(DMA_DIR(dir),
				 !!(nand->options & NAND_NEED_SCRAMBLING),
				 meson_chip->bch_mode,
				 NFC_CMD_SHORTMODE_DISABLE,
				 nand->ecc.size >> 3,
				 nand->ecc.steps);

		if (meson_chip->bch_mode != NFC_ECC_BCH_SHORT) {
			meson_chip->bch_mode = NFC_ECC_BCH_SHORT;
			nand->ecc.size = NAND_ECC_UNIT_SHORT;
		}
		memset(buf, 0xff, (1 << nand->page_shift));
		nand->options |= NAND_NEED_SCRAMBLING;
		ret = meson_nfc_read_page_hwecc(nand, buf, 1, page);
		if (ret < 0)
			return ret; /*read failed*/

		nand->options &= ~NAND_NEED_SCRAMBLING;

		/*check page 0 info here*/
		if (nfc->data->bl2ex_mode) {
			p_nand_page0_sc2 = (struct _nand_page0_sc2 *)buf;
			p_nand_setup_sc2 = &p_nand_page0_sc2->nand_setup;
			p_ext_info = &p_nand_page0_sc2->ext_info;
			configure_data = p_nand_setup_sc2->cfg.b.cmd;
		} else {
			p_nand_page0 = (struct _nand_page0 *)buf;
			p_nand_setup = &p_nand_page0->nand_setup;
			p_ext_info = &p_nand_page0->ext_info;
			configure_data = p_nand_setup->cfg.b.cmd;
		}

		pages_per_blk = p_ext_info->pages_per_blk;
		pages_per_blk_w =
			(1 << (nand->phys_erase_shift -
			nand->page_shift));
		if (pages_per_blk_w != pages_per_blk ||
		    configure_data_w != configure_data) {
			pr_info("page%d fail ", page);
			pr_info("configure: 0x%x-0x%x pages per blk 0x%x-0x%x\n",
				configure_data_w, configure_data,
				pages_per_blk_w, pages_per_blk);
		}
		meson_chip->bch_mode = bch_mode;
		nand->ecc.size = ecc_size;
	}

	real_page = page;
	real_page++;
READ_BAD_BLOCK:
	ofs = ((u64)real_page << nand->page_shift);
	tmp = ofs;
	div_u64_rem(tmp, mtd->erasesize, &remainder);
	if (!remainder) {
		nand_release_device(nand);
		if (mtd->_block_isbad(mtd, ofs)) {
			nand_get_device(nand);
			real_page +=
				1 << (nand->phys_erase_shift -
				nand->page_shift);
			goto READ_BAD_BLOCK;
		}
		nand_get_device(nand);
	}

	memset(buf, 0xff, (1 << nand->page_shift));
	ret = meson_nfc_read_page_hwecc(nand, buf, 1, real_page);
	return ret;
}

static int meson_nfc_read_oob_raw(struct nand_chip *nand, int page)
{
	return meson_nfc_read_page_raw(nand, NULL, 1, page);
}

static int meson_nfc_read_oob(struct nand_chip *nand, int page)
{
	return meson_nfc_read_page_hwecc(nand, NULL, 1, page);
}

static int meson_nfc_boot_read_oob(struct nand_chip *nand, int page)
{
	int ret = 0;

	ret = meson_nfc_boot_read_page_hwecc(nand, nand->data_buf,
					     1, page);
	return ret;
}

static bool meson_nfc_is_buffer_dma_safe(const void *buffer)
{
#ifndef CONFIG_ARM
	if (virt_addr_valid(buffer) && (!object_is_on_stack(buffer)))
		return true;
#endif
	return false;
}

static void *
meson_nand_op_get_dma_safe_input_buf(const struct nand_op_instr *instr)
{
	if (WARN_ON(instr->type != NAND_OP_DATA_IN_INSTR))
		return NULL;

	if (meson_nfc_is_buffer_dma_safe(instr->ctx.data.buf.in))
		return instr->ctx.data.buf.in;

	return kzalloc(instr->ctx.data.len, GFP_KERNEL);
}

static void
meson_nand_op_put_dma_safe_input_buf(const struct nand_op_instr *instr,
				     void *buf)
{
	if (WARN_ON(instr->type != NAND_OP_DATA_IN_INSTR) ||
	    WARN_ON(!buf))
		return;

	if (buf == instr->ctx.data.buf.in)
		return;

	memcpy(instr->ctx.data.buf.in, buf, instr->ctx.data.len);
	kfree(buf);
}

static void *
meson_nand_op_get_dma_safe_output_buf(const struct nand_op_instr *instr)
{
	if (WARN_ON(instr->type != NAND_OP_DATA_OUT_INSTR))
		return NULL;

	if (meson_nfc_is_buffer_dma_safe(instr->ctx.data.buf.out))
		return (void *)instr->ctx.data.buf.out;

	return kmemdup(instr->ctx.data.buf.out,
		       instr->ctx.data.len, GFP_KERNEL);
}

static void
meson_nand_op_put_dma_safe_output_buf(const struct nand_op_instr *instr,
				      const void *buf)
{
	if (WARN_ON(instr->type != NAND_OP_DATA_OUT_INSTR) ||
	    WARN_ON(!buf))
		return;

	if (buf != instr->ctx.data.buf.out)
		kfree(buf);
}

static int meson_nfc_exec_op(struct nand_chip *nand,
			     const struct nand_operation *op, bool check_only)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	const struct nand_op_instr *instr = NULL;
	void *buf;
	u32 op_id, delay_idle, cmd;
	int i;

	meson_nfc_select_chip(nand, op->cs);

	for (op_id = 0; op_id < op->ninstrs; op_id++) {
		instr = &op->instrs[op_id];

		delay_idle = DIV_ROUND_UP(PSEC_TO_NSEC(instr->delay_ns),
					  meson_chip->level1_divider *
					  NFC_CLK_CYCLE);
		switch (instr->type) {
		case NAND_OP_CMD_INSTR:
			cmd = nfc->param.chip_select | NFC_CMD_CLE;
			cmd |= instr->ctx.cmd.opcode & 0xff;
			writel(cmd, nfc->reg_base + NFC_REG_CMD);
			meson_nfc_cmd_idle(nfc, delay_idle);
			break;

		case NAND_OP_ADDR_INSTR:
			for (i = 0; i < instr->ctx.addr.naddrs; i++) {
				cmd = nfc->param.chip_select | NFC_CMD_ALE;
				cmd |= instr->ctx.addr.addrs[i] & 0xff;
				writel(cmd, nfc->reg_base + NFC_REG_CMD);
			}
			meson_nfc_cmd_idle(nfc, delay_idle);
			break;

		case NAND_OP_DATA_IN_INSTR:
			buf = meson_nand_op_get_dma_safe_input_buf(instr);
			if (!buf)
				return -ENOMEM;
			meson_nfc_read_buf(nand, buf, instr->ctx.data.len);
			meson_nand_op_put_dma_safe_input_buf(instr, buf);
			break;

		case NAND_OP_DATA_OUT_INSTR:
			buf = meson_nand_op_get_dma_safe_output_buf(instr);
			if (!buf)
				return -ENOMEM;
			meson_nfc_write_buf(nand, buf, instr->ctx.data.len);
			meson_nand_op_put_dma_safe_output_buf(instr, buf);
			break;

		case NAND_OP_WAITRDY_INSTR:
			meson_nfc_queue_rb(nand, instr->ctx.waitrdy.timeout_ms);
			if (instr->delay_ns)
				meson_nfc_cmd_idle(nfc, delay_idle);
			break;
		}
	}
	meson_nfc_wait_cmd_finish(nfc, DMA_BUSY_TIMEOUT);
	return 0;
}

static int meson_ooblayout_ecc(struct mtd_info *mtd, int section,
			       struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand = mtd_to_nand(mtd);

	if (section < 0 || section >= nand->ecc.steps)
		return -ERANGE;

	oobregion->offset =  2 + (section * (2 + nand->ecc.bytes));
	oobregion->length = nand->ecc.bytes;

	return 0;
}

static int meson_ooblayout_free(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand = mtd_to_nand(mtd);

	if (section < 0 || section >= nand->ecc.steps)
		return -ERANGE;

	oobregion->offset = section * 2;
	oobregion->length = 2;

	return 0;
}

static const struct mtd_ooblayout_ops meson_ooblayout_ops = {
	.ecc = meson_ooblayout_ecc,
	.free = meson_ooblayout_free,
};

#define CLK_DIV_SHIFT 0
#define CLK_DIV_WIDTH 6
static int meson_nfc_clk_init(struct meson_nfc *nfc)
{
	int ret;
	struct clk_init_data init = {0};
	const char *fix_div2_pll_name[1];
	unsigned int reg_val = 0;

	nfc->clk_gate = devm_clk_get(nfc->dev, "gate");
	if (IS_ERR(nfc->clk_gate)) {
		dev_err(nfc->dev, "failed to get gate\n");
		return PTR_ERR(nfc->clk_gate);
	}

	ret = clk_prepare_enable(nfc->clk_gate);
	if (ret) {
		dev_err(nfc->dev, "failed to enable gate\n");
		return ret;
	}

	nfc->fix_div2_pll = devm_clk_get(nfc->dev, "fdiv2pll");
	if (IS_ERR(nfc->fix_div2_pll)) {
		dev_err(nfc->dev, "failed to get fix pll\n");
		return PTR_ERR(nfc->fix_div2_pll);
	}

	ret = clk_prepare_enable(nfc->fix_div2_pll);
	if (ret) {
		dev_err(nfc->dev, "failed to enable fix pll");
		return ret;
	}

	/* select fix PLL, 1000MHz(fixdiv2pll) */
	reg_val = readl(nfc->nand_clk_reg);
	writel(CLK_SELECT_SRC_FIXDIV2PLL | (reg_val & ~CLK_SELECT_SRC_MASK),
			nfc->nand_clk_reg);

	init.name = devm_kstrdup(nfc->dev, "nfc#div", GFP_KERNEL);
	init.ops = &clk_divider_ops;
	fix_div2_pll_name[0] = __clk_get_name(nfc->fix_div2_pll);
	init.parent_names = fix_div2_pll_name;
	init.num_parents = 1;
	nfc->nand_divider.reg = nfc->nand_clk_reg;
	nfc->nand_divider.shift = CLK_DIV_SHIFT;
	nfc->nand_divider.width = CLK_DIV_WIDTH;
	nfc->nand_divider.hw.init = &init;
	nfc->nand_divider.flags =  CLK_DIVIDER_ONE_BASED |
		CLK_DIVIDER_ROUND_CLOSEST | CLK_DIVIDER_ALLOW_ZERO;
	nfc->nand_div_clk = devm_clk_register(nfc->dev, &nfc->nand_divider.hw);
	if (IS_ERR(nfc->nand_div_clk))
		return PTR_ERR(nfc->nand_div_clk);

	ret = clk_prepare_enable(nfc->nand_div_clk);
	if (ret)
		dev_err(nfc->dev, "pre enable NFC divider fail\n");
	return 0;
}

static void meson_nfc_disable_clk(struct meson_nfc *nfc)
{
	clk_disable_unprepare(nfc->nand_div_clk);
	clk_disable_unprepare(nfc->fix_div2_pll);
	clk_disable_unprepare(nfc->clk_gate);
}

static void meson_nfc_free_buffer(struct nand_chip *nand)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);

	kfree(meson_chip->info_buf);
	kfree(meson_chip->data_buf);
}

static int meson_chip_buffer_init(struct nand_chip *nand)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	u32 page_bytes, info_bytes, nsectors;

	nsectors = mtd->writesize / nand->ecc.size;

	page_bytes =  mtd->writesize + mtd->oobsize;
	info_bytes = nsectors * PER_INFO_BYTE;

	meson_chip->data_buf = kmalloc(page_bytes, GFP_KERNEL);
	if (!meson_chip->data_buf)
		return -ENOMEM;

	meson_chip->info_buf = kmalloc(info_bytes, GFP_KERNEL);
	if (!meson_chip->info_buf) {
		kfree(meson_chip->data_buf);
		return -ENOMEM;
	}

	return 0;
}

static
int meson_nfc_setup_data_interface(struct nand_chip *nand, int csline,
				   const struct nand_data_interface *conf)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	const struct nand_sdr_timings *timings;
	u32 div, bt_min, bt_max, tbers_clocks;

	timings = nand_get_sdr_timings(conf);
	if (IS_ERR(timings))
		return -EOPNOTSUPP;

	if (csline == NAND_DATA_IFACE_CHECK_ONLY)
		return 0;

	div = DIV_ROUND_UP((timings->tRC_min / 1000), NFC_CLK_CYCLE);
	bt_min = (timings->tREA_max + NFC_DEFAULT_DELAY) / div;
	bt_max = (NFC_DEFAULT_DELAY + timings->tRHOH_min +
		  timings->tRC_min / 2) / div;

	meson_chip->twb = DIV_ROUND_UP(PSEC_TO_NSEC(timings->tWB_max),
				       div * NFC_CLK_CYCLE);
	meson_chip->tadl = DIV_ROUND_UP(PSEC_TO_NSEC(timings->tADL_min),
					div * NFC_CLK_CYCLE);
	tbers_clocks = DIV_ROUND_UP_ULL(PSEC_TO_NSEC(timings->tBERS_max),
					div * NFC_CLK_CYCLE);
	meson_chip->tbers_max = ilog2(tbers_clocks);

	if (!is_power_of_2(tbers_clocks))
		meson_chip->tbers_max++;

	bt_min = DIV_ROUND_UP(bt_min, 1000);
	bt_max = DIV_ROUND_UP(bt_max, 1000);

	if (bt_max < bt_min)
		return -EINVAL;

	meson_chip->level1_divider = div;
	meson_chip->clk_rate = 1000000000 / meson_chip->level1_divider;
	meson_chip->bus_timing = (bt_min + bt_max) / 2 + 1;
	pr_info("%s %d level1_divider: %ld, clk_rate: %ld, bus_timing: %d\n",
		__func__, __LINE__, meson_chip->level1_divider, meson_chip->clk_rate,
		meson_chip->bus_timing);

	return 0;
}

static int meson_nand_bch_mode(struct nand_chip *nand)
{
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	int i;

	if (nand->ecc.strength > 60 || nand->ecc.strength < 8)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(meson_ecc); i++) {
		if (meson_ecc[i].strength == nand->ecc.strength &&
		    meson_ecc[i].step_size == nand->ecc.size) {
			meson_chip->bch_mode = meson_ecc[i].bch;
			pr_info("select %d bch mode\n", meson_chip->bch_mode);
			return 0;
		}
	}

	return -EINVAL;
}

static void meson_nand_detach_chip(struct nand_chip *nand)
{
	meson_nfc_free_buffer(nand);
}

static int meson_nand_attach_chip(struct nand_chip *nand)
{
	struct meson_nfc *nfc = nand_get_controller_data(nand);
	struct meson_nfc_nand_chip *meson_chip = to_meson_nand(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	int nsectors = 0;
	int ret;

	if (mtd->writesize <= 2048 ||
	    nfc->data->ecc_caps->stepinfos->stepsize == 512) {
		nsectors = mtd->writesize / 512;
	} else {
		nsectors = mtd->writesize / 1024;
	}

	if (!mtd->name) {
		mtd->name = devm_kasprintf(nfc->dev, GFP_KERNEL,
					   "%s:nand%d",
					   dev_name(nfc->dev),
					   meson_chip->sels[0]);
		if (!mtd->name)
			return -ENOMEM;
	}

	if (nand->bbt_options & NAND_BBT_USE_FLASH)
		nand->bbt_options |= NAND_BBT_NO_OOB;

	nand->options |= NAND_SKIP_BBTSCAN;
	nand->options |= NAND_NO_SUBPAGE_WRITE;
	meson_chip->bch_info = NFC_ECC_BCH60_1K;

	ret = nand_ecc_choose_conf(nand, nfc->data->ecc_caps,
				   mtd->oobsize - 2 * nsectors);
	if (mtd->writesize <= 2048 ||
		nfc->data->ecc_caps->stepinfos->stepsize == 512) {
		nand->ecc.size = SZ_512;
		nand->ecc.strength = 8;
		nand->ecc.bytes = ECC_PARITY_BCH8_512B;
		meson_chip->bch_info = NFC_ECC_BCH8_1K;
	}

	pr_info("ecc_step size: 0x%x, ecc strength:0x%x\n",
		nand->ecc.size, nand->ecc.strength);
	nand->ecc.steps = mtd->writesize / nand->ecc.size;
	pr_info("ecc bytes:0x%x, ecc steps: 0x%x\n",
		nand->ecc.bytes, nand->ecc.steps);

	if (ret) {
		dev_err(nfc->dev, "failed to ECC init\n");
		return -EINVAL;
	}

	mtd_set_ooblayout(mtd, &meson_ooblayout_ops);

	ret = meson_nand_bch_mode(nand);
	if (ret)
		return -EINVAL;

	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.write_page_raw = meson_nfc_write_page_raw;
	nand->ecc.write_page = meson_nfc_write_page_hwecc;
	nand->ecc.write_oob_raw = nand_write_oob_std;
	nand->ecc.write_oob = nand_write_oob_std;

	nand->ecc.read_page_raw = meson_nfc_read_page_raw;
	nand->ecc.read_page = meson_nfc_read_page_hwecc;
	nand->ecc.read_oob_raw = meson_nfc_read_oob_raw;
	nand->ecc.read_oob = meson_nfc_read_oob;

	if (nand->options & NAND_BUSWIDTH_16) {
		dev_err(nfc->dev, "16bits bus width not supported");
		return -EINVAL;
	}
	ret = meson_chip_buffer_init(nand);
	if (ret)
		return -ENOMEM;

	return ret;
}

static const struct nand_controller_ops meson_nand_controller_ops = {
	.attach_chip = meson_nand_attach_chip,
	.detach_chip = meson_nand_detach_chip,
	.setup_data_interface = meson_nfc_setup_data_interface,
	.exec_op = meson_nfc_exec_op,
};

int meson_nand_scan_shipped_bbt(struct nand_chip *nand)
{
	/**todo later**/
	return 0;
}

int meson_nand_bbt_check(struct nand_chip *nand)
{
	int phys_erase_shift;
	int ret = 0;
	s8 *buf = NULL;

	struct mtd_info *mtd = nand_to_mtd(nand);
	struct meson_nfc *nfc = nand_get_controller_data(mtd_to_nand(mtd));

	phys_erase_shift = mtd->erasesize_shift;
	ret = meson_rsv_scan(nfc->rsv->bbt);
	if (ret != 0 && (ret != (-1))) {
		pr_info("scan bbt table failed\n");
		return ret;
	}

	ret = 0;
	buf = nfc->block_status;
	if (nfc->rsv->bbt->valid == 1) {
		pr_info("%s %d bbt is valid,reading\n",
			__func__, __LINE__);
		meson_rsv_read(nfc->rsv->bbt, (u_char *)buf);
	} else {
		pr_info("%s %d bbt is invalid, scanning\n",
			__func__, __LINE__);
		memset(nfc->block_status, 0,
		       mtd->size >> phys_erase_shift);
		meson_nand_scan_shipped_bbt(nand);
		meson_rsv_bbt_write((u_char *)buf, nfc->rsv->bbt->size);
	}

	if (nand->options & NAND_SKIP_BBTSCAN) {
		/* sync with nand chip bbt */
		nand->bbt = nfc->block_status;
		mtd_to_nand(aml_mtd_info[0])->bbt = nfc->block_status;
	}

	return 0;
}

int meson_nand_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct nand_pos pos;
	s8 block_status;
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct meson_nfc *nfc = nand_get_controller_data(chip);
	s8 *buf;
	int ret = 0;

	nand_get_device(chip);
	nanddev_offs_to_pos(nand, offs, &pos);
	if (nfc->block_status) {
		block_status = nfc->block_status[pos.eraseblock];
		if (block_status == NAND_BLOCK_BAD ||
			block_status == NAND_FACTORY_BAD) {
			pr_info("offs : 0x%llx already is bad blk\n",
				offs);
			ret = 0;
		} else if (block_status == NAND_BLOCK_GOOD) {
			nfc->block_status[pos.eraseblock] = NAND_BLOCK_BAD;
			buf = nfc->block_status;
			meson_rsv_bbt_write((u_char *)buf, nfc->rsv->bbt->size);
			ret = 0;
		}
	}
	nand_release_device(chip);
	return ret;
}

static const char * const meson_types[] = {
	"meson-partitions",
	NULL
};

static int
meson_nfc_nand_chip_init(struct device *dev,
			 struct meson_nfc *nfc, struct device_node *np)
{
	struct meson_nfc_nand_chip *meson_chip;
	struct nand_chip *nand;
	struct mtd_info *mtd;
	int ret, i;
	u32 tmp, nsels;
	struct nand_memory_organization *memorg;

	nsels = of_property_count_elems_of_size(np, "reg", sizeof(u32));
	if (!nsels || nsels > MAX_CE_NUM) {
		dev_err(dev, "invalid register property size\n");
		return -EINVAL;
	}
	pr_info("%s %d nsels: %d\n", __func__, __LINE__, nsels);

	meson_chip = devm_kzalloc(dev, struct_size(meson_chip, sels, nsels),
				  GFP_KERNEL);
	if (!meson_chip)
		return -ENOMEM;

	meson_chip->nsels = nsels;//info page0 chip_num value
	for (i = 0; i < nsels; i++) {
		ret = of_property_read_u32_index(np, "reg", i, &tmp);
		if (ret) {
			dev_err(dev, "could not retrieve register property: %d\n",
				ret);
			goto exit_err;
		}
	}

	nand = &meson_chip->nand;
	nand->controller = &nfc->controller;
	nand->controller->ops = &meson_nand_controller_ops;
	nand_set_flash_node(nand, np);
	nand_set_controller_data(nand, nfc);

	nand->options |= NAND_USE_BOUNCE_BUFFER;
	mtd = nand_to_mtd(nand);
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = dev;
	if (aml_mtd_devnum == 0)
		mtd->name = NAND_BOOT_NAME;
	else
		mtd->name = NAND_NORMAL_NAME;

	ret = nand_scan_with_ids(nand, nsels, aml_nand_flash_ids);
	if (ret)
		goto exit_err;

	pr_info("nand id 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		nand->id.data[0], nand->id.data[1], nand->id.data[2],
		nand->id.data[3], nand->id.data[4], nand->id.data[5],
		nand->id.data[6], nand->id.data[7]);

	memorg = nanddev_get_memorg(&nand->base);
	pr_info("memory: bits_per_cell: 0x%x\n", memorg->bits_per_cell);

	if (!nfc->rsv) {
		nfc->rsv = kzalloc(sizeof(*nfc->rsv), GFP_KERNEL);
		if (!nfc->rsv) {
			ret = -ENOMEM;
			goto exit_err1;
		}
	}

	if (is_power_of_2(mtd->erasesize))
		mtd->erasesize_shift = ffs(mtd->erasesize) - 1;
	else
		mtd->erasesize_shift = 0;

	if (is_power_of_2(mtd->writesize))
		mtd->writesize_shift = ffs(mtd->writesize) - 1;
	else
		mtd->writesize_shift = 0;

	mtd->_block_markbad = meson_nand_block_markbad;
	if (aml_mtd_devnum != 0) {
		meson_rsv_init(mtd, nfc->rsv);
		nfc->block_status = kzalloc((mtd->size >> mtd->erasesize_shift),
					    GFP_KERNEL);
		if (!nfc->block_status) {
			ret = -ENOMEM;
			pr_info("Allocated memory for block status failed\n");
			goto exit_err2;
		}

		ret = meson_nand_bbt_check(nand);
		if (ret) {
			pr_info("invalid nand bbt\n");
			goto exit_err2;
		}

		meson_rsv_check(nfc->rsv->env);
		meson_rsv_check(nfc->rsv->key);
		meson_rsv_check(nfc->rsv->dtb);
	}
	ret = mtd_device_parse_register(mtd, meson_types, NULL, NULL, 0);
	if (ret) {
		dev_err(dev, "failed to register MTD device: %d\n", ret);
		nand_cleanup(nand);
		return ret;
	}

	if (aml_mtd_devnum == 0) {
		mtd->size = BOOT_TOTAL_PAGES * mtd->writesize;
		nand->ecc.write_page = meson_nfc_boot_write_page_hwecc;
		nand->ecc.read_page = meson_nfc_boot_read_page_hwecc;
		nand->ecc.read_oob = meson_nfc_boot_read_oob;
	}
	pr_info("%s %d meson_nfc_devnum: %d, name: %s",
		__func__, __LINE__, aml_mtd_devnum, mtd->name);
	aml_mtd_info[aml_mtd_devnum] = mtd;
	aml_mtd_devnum++;

	list_add_tail(&meson_chip->node, &nfc->chips);

	return 0;

exit_err2:
	kfree(nfc->block_status);
exit_err1:
	kfree(nfc->rsv);
exit_err:
	devm_kfree(dev, meson_chip);

	return ret;
}

static int meson_nfc_nand_chip_cleanup(struct meson_nfc *nfc)
{
	struct meson_nfc_nand_chip *meson_chip;
	struct mtd_info *mtd;
	int ret;

	while (!list_empty(&nfc->chips)) {
		meson_chip = list_first_entry(&nfc->chips,
					      struct meson_nfc_nand_chip, node);
		mtd = nand_to_mtd(&meson_chip->nand);
		ret = mtd_device_unregister(mtd);
		if (ret)
			return ret;

		meson_nfc_free_buffer(&meson_chip->nand);
		nand_cleanup(&meson_chip->nand);
		list_del(&meson_chip->node);
	}

	return 0;
}

static int meson_nfc_nand_chips_init(struct device *dev,
				     struct meson_nfc *nfc)
{
	struct device_node *np = dev->of_node;
	struct device_node *nand_np;
	int ret;

	for_each_child_of_node(np, nand_np) {
		ret = meson_nfc_nand_chip_init(dev, nfc, nand_np);
		if (ret) {
			meson_nfc_nand_chip_cleanup(nfc);
			of_node_put(nand_np);
			return ret;
		}
	}

	return 0;
}

static irqreturn_t meson_nfc_irq(int irq, void *id)
{
	struct meson_nfc *nfc = id;
	u32 cfg;

	cfg = readl(nfc->reg_base + NFC_REG_CFG);
	if (!(cfg & NFC_RB_IRQ_EN))
		return IRQ_NONE;

	cfg &= ~(NFC_RB_IRQ_EN);
	writel(cfg, nfc->reg_base + NFC_REG_CFG);

	complete(&nfc->completion);
	writel((1 << 31), nfc->reg_base + NFC_REG_CMD);
	return IRQ_HANDLED;
}

const char *para_name[] = {
	"nand_clk_ctrl",
	"spi_cfg",
	"bl_mode",
	"fip_copies",
	"fip_size",
	"ship_bad_block",
	"disa_irq_flag"
};

static int prase_nand_parameter_from_dtb(struct meson_nfc *nfc,
					 struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0, i = 0;
	u32 dts_param[16];

	if (!pdev->dev.of_node)
		return 1;
	of_node_get(np);

	memset(dts_param, 0, sizeof(dts_param));
	for (i = 0; i < ARRAY_SIZE(para_name); i++) {
		ret = of_property_read_u32(np, para_name[i], &dts_param[i]);
		if (ret && strncmp(para_name[i], "spi_cfg", strlen("spi_cfg"))) {
			pr_info("%s %d,please config para item %d in dts\n",
				__func__, __LINE__, i);
			return ret;
		}
	}
	memcpy(&nfc->param_from_dts, dts_param, i * sizeof(u32));
	pr_info("bl_mode %s\n", nfc->param_from_dts.bl_mode ? "discrete" : "compact");
	pr_info("fip copies %d\n", nfc->param_from_dts.fip_copies);
	pr_info("fip size 0x%x\n", nfc->param_from_dts.fip_size);
	pr_info("nand clk ctrl: 0x%x\n", nfc->param_from_dts.clk_ctrl_base);
	pr_info("nand spi_cfg: 0x%x\n", nfc->param_from_dts.spi_cfg);
	pr_info("%s bad_block\n", nfc->param_from_dts.skip_bad_block ?
		"skip" : "not skip");
	pr_info("disa_irq_hand vale: %d\n", nfc->param_from_dts.disa_irq_hand);

	return ret;
}

static const struct meson_nfc_data meson_full_ecc_data = {
	.ecc_caps = &meson_1K_ecc_caps,
};

static const struct meson_nfc_data meson_single_ecc_data = {
	.ecc_caps = &meson_512_ecc_caps,
};

static const struct meson_nfc_data meson_full_ecc_bl2ex_data = {
	.ecc_caps = &meson_1K_ecc_caps,
	.bl2ex_mode = 1,
};

static const struct meson_nfc_data meson_single_ecc_bl2ex_data = {
	.ecc_caps = &meson_512_ecc_caps,
	.bl2ex_mode = 1,
};

static const struct of_device_id meson_nfc_id_table[] = {
	{
		.compatible = "amlogic,meson-nfc-full-ecc",
		.data = (void *)&meson_full_ecc_data,
	}, {
		.compatible = "amlogic,meson-nfc-single-ecc",
		.data = (void *)&meson_single_ecc_data,
	}, {
		.compatible = "amlogic,meson-nfc-full-ecc-bl2ex",
		.data = (void *)&meson_full_ecc_bl2ex_data,
	}, {
		.compatible = "amlogic,meson-nfc-single-ecc-bl2ex",
		.data = (void *)&meson_single_ecc_bl2ex_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, meson_nfc_id_table);

static int meson_nfc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_nfc *nfc;
	struct resource *res;
	int ret, irq;
	u32 nfc_base;
	void __iomem *spi_cfg_vaddr;

	nfc = devm_kzalloc(dev, sizeof(*nfc), GFP_KERNEL);
	if (!nfc)
		return -ENOMEM;

	nfc->data = of_device_get_match_data(&pdev->dev);
	if (!nfc->data) {
		ret = -ENODEV;
		goto err_mem;
	}

	nand_controller_init(&nfc->controller);
	INIT_LIST_HEAD(&nfc->chips);
	init_completion(&nfc->completion);

	nfc->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nfc->reg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(nfc->reg_base)) {
		ret = PTR_ERR(nfc->reg_base);
		pr_info("get nand reg base failed\n");
		goto err_mem;
	}

	nfc->nand_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(nfc->nand_pinctrl)) {
		ret = PTR_ERR(nfc->nand_pinctrl);
		pr_info("get pinctrl failed\n");
		goto err_mem;
	}

	nfc->nand_norbstate = pinctrl_lookup_state(nfc->nand_pinctrl,
						   "nand_norb_mod");
	if (IS_ERR(nfc->nand_norbstate)) {
		devm_pinctrl_put(nfc->nand_pinctrl);
		ret =  PTR_ERR(nfc->nand_norbstate);
		pr_info("get pinctrl nand_norbstate failed\n");
		goto err_mem;
	}

	nfc->nand_idlestate = pinctrl_lookup_state(nfc->nand_pinctrl,
						   "nand_cs_only");
	if (IS_ERR(nfc->nand_idlestate)) {
		devm_pinctrl_put(nfc->nand_pinctrl);
		ret =  PTR_ERR(nfc->nand_idlestate);
		pr_info("get pinctrl nand_idlestate failed\n");
		goto err_mem;
	}

	ret =
	pinctrl_select_state(nfc->nand_pinctrl,
			     nfc->nand_norbstate);
	if (ret < 0)
		pr_info("%s:%d %s can't get pinctrl",
			__func__,
			__LINE__,
			dev_name(dev));

	ret = prase_nand_parameter_from_dtb(nfc, pdev);
	if (ret) {
		pr_info("!!!dtb prase error,need check dtb\n");
		ret = -EINVAL;
		goto err_mem;
	}

	nfc_base = nfc->param_from_dts.clk_ctrl_base;
	nfc->nand_clk_reg = devm_ioremap_nocache(&pdev->dev,
						 nfc_base,
						 sizeof(int));
	ret = meson_nfc_clk_init(nfc);
	if (ret) {
		dev_err(dev, "failed to initialize NAND clock\n");
		goto err_mem;
	}

	writel(0, nfc->reg_base + NFC_REG_CFG);

	/* select slc mode */
	if (nfc->param_from_dts.spi_cfg) {
		spi_cfg_vaddr = devm_ioremap_nocache(&pdev->dev,
						 nfc->param_from_dts.spi_cfg,
						 sizeof(int));
		if (IS_ERR_OR_NULL(spi_cfg_vaddr))
			goto err_mem;
		writel(0, spi_cfg_vaddr);
	}

	if (!nfc->param_from_dts.disa_irq_hand) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0) {
			dev_err(dev, "no NFC IRQ resource\n");
			ret = -EINVAL;
			goto err_clk;
		}
		ret = devm_request_irq(dev, irq, meson_nfc_irq, 0,
			dev_name(dev), nfc);
		if (ret) {
			dev_err(dev, "failed to request NFC IRQ\n");
			ret = -EINVAL;
			goto err_clk;
		}
	}

	ret = dma_set_mask(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "failed to set DMA mask\n");
		goto err_clk;
	}

	platform_set_drvdata(pdev, nfc);

	register_mtd_parser(&ofpart_meson_parser);
	ret = meson_nfc_nand_chips_init(dev, nfc);
	if (ret) {
		dev_err(dev, "failed to init NAND chips\n");
		deregister_mtd_parser(&ofpart_meson_parser);
		goto err_clk;
	}
	ret = readl(nfc->nand_clk_reg);
	ret = clk_get_rate(nfc->nand_div_clk);
	ret = readl(nfc->reg_base + NFC_REG_CFG);

	devm_iounmap(&pdev->dev, spi_cfg_vaddr);
	return 0;

err_clk:
	devm_iounmap(&pdev->dev, spi_cfg_vaddr);
	meson_nfc_disable_clk(nfc);
err_mem:
	devm_kfree(dev, nfc);

	return ret;
}

static int meson_nfc_remove(struct platform_device *pdev)
{
	struct meson_nfc *nfc = platform_get_drvdata(pdev);
	int ret;

	ret = meson_nfc_nand_chip_cleanup(nfc);
	if (ret)
		return ret;

	meson_nfc_disable_clk(nfc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver meson_nfc_driver = {
	.probe  = meson_nfc_probe,
	.remove = meson_nfc_remove,
	.driver = {
		.name  = "meson-nand",
		.of_match_table = meson_nfc_id_table,
	},
};

#ifndef MODULE
module_platform_driver(meson_nfc_driver);
#else
int __init meson_nfc_init_module(void)
{
    return platform_driver_register(&meson_nfc_driver);
}

__exit void meson_nfc_exit_module(void)
{
	platform_driver_unregister(&meson_nfc_driver);
}

module_init(meson_nfc_init_module);
module_exit(meson_nfc_exit_module);
#endif

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Liang Yang <liang.yang@amlogic.com>");
MODULE_DESCRIPTION("Amlogic's Meson NAND Flash Controller driver");
