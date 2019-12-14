/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/pm_domain.h>

#define PWR_ON    1
#define PWR_OFF   0
#define DOMAIN_INIT_ON    0
#define DOMAIN_INIT_OFF   1
#define POWER_CTRL_IRQ_SET     0x82000094

enum pm_e {
	PM_CPU_PWR0,
	PM_CPU_CORE0,
	PM_CPU_CORE1,
	PM_DSP_A = 8,
	PM_DSP_B,
	PM_UART,
	PM_MMC_DMC_COM, //A1: MMC C1:DMC
	PM_I2C,
	PM_PSRAM_SDEMMC_B_COM, //A1: PSRAM C1:SDEMMC_B
	PM_ACODEC,
	PM_AUDIO,
	PM_MKL_OTP,
	PM_DMA,
	PM_SDEMMC_SDEMMC_A_COM, //A1: SDEMMC C1:SDEMMC_A
	PM_SRAM_A,
	PM_SRAM_B,
	PM_IR,
	PM_SPICC,
	PM_SPIFC,
	PM_USB,
	PM_NIC,
	PM_PDM,
	PM_RSA,
	PM_MIPI_ISP,
	PM_HCODEC,
	PM_WAVE,
	PM_SDEMMC_C,
	PM_SRAM_C,
	PM_GDC,
	PM_GE2D,
	PM_NNA,
	PM_ETH,
	PM_GIC,
	PM_DDR,
	PM_SPICC_B
};

unsigned long pwr_ctrl_psci_smc(enum pm_e power_domain, bool power_control);
unsigned long pwr_ctrl_status_psci_smc(enum pm_e power_domain);

/*
 *irq:irq number for wakeup pwrctrl
 *irq_mask:irq mask,1:enable,0:mask
 *irq_invert:1:invert irq level,0:do not
 */
unsigned long pwr_ctrl_irq_set(u64 irq, u64 irq_mask, u64 irq_invert);

void pd_dev_create_file(struct device *dev, int cnt_start, int cnt_end,
			struct generic_pm_domain **domains);
void pd_dev_remove_file(struct device *dev);

