/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Author: Long Yu
 */

#ifndef LINUX_AMLMMC_CQHCI_H
#define LINUX_AMLMMC_CQHCI_H

#include <linux/compiler.h>
#include <linux/bitops.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/irqreturn.h>

#include "cqhci.h"

#define SDIO_RESP_ERR_MASK 0x0000CB00

#define SD_EMMC_CQE_REG    0x100
#define SD_EMMC_CQVER    (SD_EMMC_CQE_REG + 0x0)
#define SD_EMMC_CQCAP    (SD_EMMC_CQE_REG + 0x4)
#define SD_EMMC_CQCFG    (SD_EMMC_CQE_REG + 0x8)
#define SD_EMMC_CQCTL    (SD_EMMC_CQE_REG + 0xc)

#define SD_EMMC_CQIS     (SD_EMMC_CQE_REG + 0x10)
#define SD_EMMC_CQISTE   (SD_EMMC_CQE_REG + 0x14)
#define SD_EMMC_CQISGE   (SD_EMMC_CQE_REG + 0x18)
#define SD_EMMC_CQIC     (SD_EMMC_CQE_REG + 0x1c)

#define SD_EMMC_CQTDLBA  (SD_EMMC_CQE_REG + 0x20)
#define SD_EMMC_CQTDLBAU (SD_EMMC_CQE_REG + 0x24)
#define SD_EMMC_CQTDBR   (SD_EMMC_CQE_REG + 0x28)
#define SD_EMMC_CQTCN    (SD_EMMC_CQE_REG + 0x2c)

#define SD_EMMC_CQDQS    (SD_EMMC_CQE_REG + 0x30)
#define SD_EMMC_CQDPT    (SD_EMMC_CQE_REG + 0x34)
#define SD_EMMC_CQTCLR   (SD_EMMC_CQE_REG + 0x38)

#define SD_EMMC_CQSSC1   (SD_EMMC_CQE_REG + 0x40)
#define SD_EMMC_CQSSC2   (SD_EMMC_CQE_REG + 0x44)
#define SD_EMMC_CQCRDCT  (SD_EMMC_CQE_REG + 0x48)

#define SD_EMMC_CQRMEM   (SD_EMMC_CQE_REG + 0x50)
#define SD_EMMC_CQTERRI  (SD_EMMC_CQE_REG + 0x54)
#define SD_EMMC_CQCRI    (SD_EMMC_CQE_REG + 0x58)
#define SD_EMMC_CQCRA    (SD_EMMC_CQE_REG + 0x5c)

bool aml_cqe_irq(struct meson_host *host, u32 intmask, int *cmd_error,
	int *data_error);
u32 aml_cqhci_irq(struct meson_host *host);
void aml_cqhci_writel(struct cqhci_host *host, u32 val, int reg);
u32 aml_cqhci_readl(struct cqhci_host *host, int reg);
void aml_cqe_enable(struct mmc_host *mmc);
void aml_cqe_disable(struct mmc_host *mmc, bool recovery);
int amlogic_add_host(struct meson_host *host);

#endif
