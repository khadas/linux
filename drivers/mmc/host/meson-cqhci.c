// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Author: Long Yu
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/ktime.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/amlogic/aml_sd.h>
#include "meson-cqhci.h"

bool aml_cqe_irq(struct meson_host *host, u32 intmask, int *cmd_error,
		int *data_error)
{
	if (!host->mmc->cqe_on)
		return false;

	if (intmask & (IRQ_RXD_ERR_MASK | IRQ_TXD_ERR))
		*data_error = -EILSEQ;
	else if (intmask & IRQ_DESC_TIMEOUT)
		*data_error = -ETIMEDOUT;
	else if (intmask & IRQ_DESC_ERR)
		*data_error = -EIO; /* indicate all ddr issue */
	else
		*data_error = 0;

	if (intmask & (IRQ_RESP_ERR))
		*cmd_error = -EILSEQ;
	else if (intmask & IRQ_RESP_TIMEOUT)
		*cmd_error = -ETIMEDOUT;
	else
		*cmd_error = 0;

	writel(0x7fff, host->regs + SD_EMMC_STATUS);
	if (*data_error || *cmd_error) {
		dev_err(host->dev, "cmd_err[0x%x], data_err[0x%x],status[0x%x]\n",
				*cmd_error, *data_error, intmask);
		if (host->debug_flag) {
			dev_notice(host->dev, "clktree : 0x%x,host_clock: 0x%x\n",
				   readl(host->clk_tree_base),
				   readl(host->regs));
			dev_notice(host->dev, "adjust: 0x%x,cfg: 0x%x,intf3: 0x%x\n",
				   readl(host->regs + SD_EMMC_V3_ADJUST),
				   readl(host->regs + SD_EMMC_CFG),
				   readl(host->regs + SD_EMMC_INTF3));
			dev_notice(host->dev, "irq_en: 0x%x\n",
				   readl(host->regs + 0x4c));
			dev_notice(host->dev, "delay1: 0x%x,delay2: 0x%x\n",
				   readl(host->regs + SD_EMMC_DELAY1),
				   readl(host->regs + SD_EMMC_DELAY2));
			dev_notice(host->dev, "pinmux: 0x%x\n",
				   readl(host->pin_mux_base));
		}
	}
	return true;
}

u32 aml_cqhci_irq(struct meson_host *host)
{
	int cmd_error = 0;
	int data_error = 0;
	u32 irq_en, raw_status, intmask;

	irq_en = readl(host->regs + SD_EMMC_IRQ_EN);
	raw_status = readl(host->regs + SD_EMMC_STATUS);
	intmask = raw_status & irq_en;

	if (!aml_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

void aml_cqhci_writel(struct cqhci_host *host, u32 val, int reg)
{
	writel(val, host->mmio + reg);
}

u32 aml_cqhci_readl(struct cqhci_host *host, int reg)
{
	return readl(host->mmio + reg);
}

void aml_cqe_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 val;

	/*
	 * CQHCI/SDMMC design prevents write access to sdhci block size
	 * register when CQE is enabled and unhalted.
	 * CQHCI driver enables CQE prior to activation, so disable CQE before
	 * programming block size in sdhci controller and enable it back.
	 */
	if (!cq_host->activated) {
		val = cqhci_readl(cq_host, CQHCI_CFG);
		if (val & CQHCI_ENABLE)
			cqhci_writel(cq_host, (val & ~CQHCI_ENABLE), CQHCI_CFG);
		//aml_set_max_blocks(mmc);
		if (val & CQHCI_ENABLE)
			cqhci_writel(cq_host, val, CQHCI_CFG);
	}

	/*
	 * CMD CRC errors are seen sometimes with some eMMC devices when status
	 * command is sent during transfer of last data block which is the
	 * default case as send status command block counter (CBC) is 1.
	 * Recommended fix to set CBC to 0 allowing send status command only
	 * when data lines are idle.
	 * amlogic cheng recommend 1 & 1;
	 */
	//val = cqhci_readl(cq_host, CQHCI_SSC1);
	val = CQHCI_SSC1_CBC(1) | CQHCI_SSC1_CIT(1);
	cqhci_writel(cq_host, val, CQHCI_SSC1);
}

void aml_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	//struct cqhci_host *cq_host = mmc->cqe_private;
	struct meson_host *host = mmc_priv(mmc);
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);

	mmc->cqe_on = false;

	pr_debug("%s: CQE off, IRQ mask %#x, IRQ status %#x\n",
		mmc_hostname(mmc), readl(host->regs + SD_EMMC_IRQ_EN),
		readl(host->regs + SD_EMMC_STATUS));

	spin_unlock_irqrestore(&host->lock, flags);
}

struct cqhci_host_ops amlogic_cqhci_ops = {
//	.write_l = aml_cqhci_writel,
//	.read_l = aml_cqhci_readl,
	.enable	= aml_cqe_enable,
	.disable = aml_cqe_disable,
//	.dumpregs = aml_dumpregs,
//	.update_dcmd_desc = aml_update_dcmd_desc,
};

int amlogic_add_host(struct meson_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct cqhci_host *cq_host;
	bool dma64;
	int ret;

	if (!host->enable_hwcq)
		return mmc_add_host(mmc);

	mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
	mmc->max_seg_size = SD_EMMC_MAX_SEG_SIZE;

	cq_host = devm_kzalloc(host->dev,
				sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		ret = -ENOMEM;
		return ret;
	}

	cq_host->mmio = host->regs + SD_EMMC_CQE_REG;
	cq_host->ops = &amlogic_cqhci_ops;

	dma64 = host->flags & AML_USE_64BIT_DMA;
	if (dma64)
		cq_host->caps |= CQHCI_TASK_DESC_SZ_128;

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret)
		return ret;

	ret = mmc_add_host(mmc);
	if (ret)
		goto cleanup;

	return 0;

cleanup:
	devm_kfree(host->dev, cq_host);
	return ret;
}

