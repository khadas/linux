// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/amlogic/power_domain.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

/*HIU*/
#define REG_CLKTREE_DSPA_CLK_CTRL0		(0x0c << 2)
#define REG_CLKTREE_DSPB_CLK_CTRL0		(0x0d << 2)

/*DSP TOP*/
#define REG_DSP_CFG0			(0x0)
#define REG_DSP_CFG1			(0x4)
#define REG_DSP_CFG2			(0x8)
#define REG_DSP_RESET_VEC		(0x004 << 2)

static bool regs_iomem_is_inited;

static inline void soc_dsp_top_reg_dump(char *name,
					void __iomem *reg_base,
					u32 reg_offset)
{
	pr_info("%s (%lx) = 0x%x\n", name,
		(unsigned long)(reg_base + reg_offset),
		readl(reg_base + reg_offset));
}

void soc_dsp_regs_iounmem(void)
{
	iounmap(g_regbases.dspa_addr);
	iounmap(g_regbases.dspb_addr);
	iounmap(g_regbases.hiu_addr);

	regs_iomem_is_inited = false;

	pr_debug("%s done\n", __func__);
}

static inline void __iomem *get_hiu_addr(void)
{
	return g_regbases.hiu_addr;
}

static inline void __iomem *get_dsp_addr(int dsp_id)
{
	if (dsp_id == 1)
		return g_regbases.dspb_addr;
	else
		return g_regbases.dspa_addr;
}

unsigned long init_dsp_psci_smc(u32 id, u32 addr, u32 cfg0)
{
	struct arm_smccc_res res;

	if (id == DSPA && bootlocation == 2 && hifi4dsp_p[DSPA]->dsp->optimize_longcall) {
		addr = hifi4dsp_p[DSPA]->dsp->sram_remap_addr[0];
		arm_smccc_smc(0x82000096, id, addr,
			hifi4dsp_p[id]->dsp->sram_remap_addr[1], 2, 0, 0, 0, &res);
	}
	if (id == DSPB && bootlocation_b == DDR_SRAM && hifi4dsp_p[DSPB]->dsp->optimize_longcall) {
		addr = hifi4dsp_p[DSPB]->dsp->sram_remap_addr[0];
		arm_smccc_smc(0x82000096, id, addr,
			hifi4dsp_p[id]->dsp->sram_remap_addr[1], 2, 0, 0, 0, &res);
	}
	arm_smccc_smc(0x82000090, id, addr, cfg0,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static void start_dsp(u32 dsp_id, u32 reset_addr)
{
	u32 StatVectorSel;
	u32 strobe = 1;
	u32 tmp;
	u32 read;
	void __iomem *reg;

	reg = get_dsp_addr(dsp_id);
	StatVectorSel = (reset_addr != 0xfffa0000);

	tmp = 0x1 |  StatVectorSel << 1 | strobe << 2;
	switch (hifi4dsp_p[dsp_id]->dsp->start_mode) {
	case SCPI_START_MODE:
		scpi_init_dsp_cfg0(dsp_id, reset_addr, tmp);
		break;
	case SMC_START_MODE:
		init_dsp_psci_smc(dsp_id, reset_addr, tmp);
		break;
	default:
		pr_debug("dsp_start_mode error,start dsp failed.\n");
		break;
	}

	read = readl(reg + REG_DSP_CFG0);
	pr_debug("REG_DSP_CFG0 read=0x%x\n", read);

	if (dsp_id == 0) {
		read  = (read & ~(0xffff << 0)) | (0x2018 << 0) | (1 << 29);
		read = read & ~(1 << 31);         //dreset assert
		read = read & ~(1 << 30);     //Breset
	} else {
		read  = (read & ~(0xffff << 0)) | (0x2019 << 0) | (1 << 29);
		read = read & ~(1 << 31);         //dreset assert
		read = read & ~(1 << 30);     //Breset
	}
	pr_info("REG_DSP_CFG0 read=0x%x\n", readl(reg + REG_DSP_CFG0));
	writel(read, reg + REG_DSP_CFG0);
	soc_dsp_top_reg_dump("REG_DSP_CFG0", reg, REG_DSP_CFG0);

	pr_debug("%s\n", __func__);
}

static noinline int __invoke_dsp_power_smc(u64 function_id, u64 arg0, u64 arg1)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      0,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static void soc_dsp_power_switch(int dsp_id, int pwr_cntl)
{
	__invoke_dsp_power_smc(0x82000092, dsp_id, pwr_cntl);
}

static void soc_dsp_powerdomain_switch(int dsp_id, bool pwr_cntl)
{
	struct device *dev = hifi4dsp_p[dsp_id]->dsp->pd_dsp;

	if (!dev)
		dev = hifi4dsp_p[dsp_id]->dev_pd;
	if (!dev)
		return;
	if (pwr_cntl)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_put_sync(dev);
}

void soc_dsp_top_regs_dump(int dsp_id)
{
	void __iomem *reg;

	pr_debug("%s\n", __func__);

	reg = get_dsp_addr(dsp_id);
	pr_debug("%s base=%lx\n", __func__, (unsigned long)reg);

	soc_dsp_top_reg_dump("REG_DSP_CFG0", reg, REG_DSP_CFG0);
	soc_dsp_top_reg_dump("REG_DSP_CFG1", reg, REG_DSP_CFG1);
	soc_dsp_top_reg_dump("REG_DSP_CFG2", reg, REG_DSP_CFG2);
	soc_dsp_top_reg_dump("REG_DSP_RESET_VEC", reg, REG_DSP_RESET_VEC);
}
EXPORT_SYMBOL(soc_dsp_top_regs_dump);

static void soc_dsp_set_clk(int dsp_id, int freq_sel)
{
	//clk_util_set_dsp_clk(dsp_id, freq_sel);
}

void soc_dsp_hw_init(int dsp_id, int freq_sel)
{
	int pwr_cntl = 1;

	soc_dsp_set_clk(dsp_id, freq_sel);
	soc_dsp_power_switch(dsp_id, pwr_cntl);

	pr_debug("%s done\n", __func__);
}
EXPORT_SYMBOL(soc_dsp_hw_init);

void soc_dsp_start(int dsp_id, int reset_addr)
{
	start_dsp(dsp_id, reset_addr);
}
EXPORT_SYMBOL(soc_dsp_start);

void soc_dsp_bootup(int dsp_id, u32 reset_addr, int freq_sel)
{
	pr_debug("%s dsp_id=%d, address=0x%x\n",
		 __func__, dsp_id, reset_addr);

	soc_dsp_powerdomain_switch(dsp_id,  PWR_ON);

	soc_dsp_start(dsp_id, reset_addr);

	pr_debug("\n after boot: reg state:\n");
	soc_dsp_top_regs_dump(dsp_id);
}
EXPORT_SYMBOL(soc_dsp_bootup);

void soc_dsp_poweroff(int dsp_id)
{
	soc_dsp_powerdomain_switch(dsp_id,  PWR_OFF);
}
EXPORT_SYMBOL(soc_dsp_poweroff);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("HiFi DSP Module Driver");
MODULE_LICENSE("GPL v2");

