/*
 * drivers/amlogic/hifi4dsp/tm2_dsp_top.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/major.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <linux/amlogic/scpi_protocol.h>

#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

/*AO_RTI*/
#define AO_RTI_ADDR_BASE		(0xff800000)
#define REG_AO_RTI_GEN_PWR_SLEEP0	(0x03a << 2)
#define REG_AO_RTI_GEN_PWR_ISO0		(0x03b << 2)

/*HIU*/
#define HUI_ADDR_BASE			(0xff63c000)
#define REG_HHI_DSP_CLK_CNTL		(0x0fc << 2)
#define REG_HHI_DSP_MEM_PD_REG0		(0x044 << 2)

/*reset*/
#define RESET_ADDR_BASE			(0xffd01000)
#define REG_RESET1_LEVEL		(0x84)
#define REG_RESET4_LEVEL		(0x90)

/*DSP TOP*/
#define DSPA_REG_BASE			(0xff680000)
#define DSPB_REG_BASE			(0xff690000)

#define REG_DSP_CFG0			(0x0)
#define REG_DSP_CFG1			(0x4)
#define REG_DSP_CFG2			(0x8)
#define REG_DSP_RESET_VEC		(0x004 << 2)

struct reg_iomem_t {
	void __iomem *dsp_addr;
	void __iomem *dspa_addr;
	void __iomem *dspb_addr;
	void __iomem *hiu_addr;		/*HIU*/
	void __iomem *ao_rti_addr;	/*AO_RTI*/
	void __iomem *reset_addr;
	void __iomem *sleep_addr;
};

static struct reg_iomem_t g_regbases;
static bool regs_iomem_is_inited;

static void __iomem *reg_iomem_init(phys_addr_t paddr, u32 size)
{
	void __iomem *vaddr = NULL;

	vaddr = ioremap_nocache(paddr, size);
	pr_debug("%s phys: %llx, virt: %p, size:%x Bytes\n",
				__func__,
				(unsigned long long)paddr,
				vaddr,
				size);
	return vaddr;
}

static inline void tm2_dsp_top_reg_dump(char *name,
	void __iomem *reg_base, u32 reg_offset)
{
	pr_info("%s (%p) = 0x%x\n", name,
		(reg_base + reg_offset),
		readl(reg_base + reg_offset));
}

void tm2_dsp_regs_iomem_init(void)
{
	if (regs_iomem_is_inited == true) {
		pr_info("%s has been done\n", __func__);
		return;
	}

	g_regbases.dspa_addr = reg_iomem_init(DSPA_REG_BASE, 0x8B*4);
	g_regbases.dspb_addr = reg_iomem_init(DSPB_REG_BASE, 0x8B*4);

	g_regbases.hiu_addr = reg_iomem_init(HUI_ADDR_BASE, 0xFF*4);
	g_regbases.ao_rti_addr = reg_iomem_init(AO_RTI_ADDR_BASE, 0xFF*4);
	g_regbases.reset_addr = reg_iomem_init(RESET_ADDR_BASE, 0x40*4);

	regs_iomem_is_inited = true;

	pr_debug("%s done\n", __func__);
}

void tm2_dsp_regs_iounmem(void)
{
	iounmap(g_regbases.dspa_addr);
	iounmap(g_regbases.dspb_addr);

	iounmap(g_regbases.hiu_addr);
	iounmap(g_regbases.ao_rti_addr);
	iounmap(g_regbases.reset_addr);

	regs_iomem_is_inited = false;

	pr_debug("%s done\n", __func__);
}

static inline void __iomem *get_hiu_addr(void)
{
	return g_regbases.hiu_addr;
}

static inline void __iomem *get_ao_rti_addr(void)
{
	return g_regbases.ao_rti_addr;
}

static inline void __iomem *get_reset_addr(void)
{
	return g_regbases.reset_addr;
}

static inline void __iomem *get_dsp_addr(int dsp_id)
{
	if (dsp_id == 1)
		return g_regbases.dspb_addr;
	else
		return g_regbases.dspa_addr;
}

/*
 * clk_util_set_dsp_clk
 * freq_sel: 0:286MHz  fclk_7
 *           1:400MHz  fclk_5
 *           2:500MHz  fclk_2/2
 *           3:667MHz  fclk_3
 *           4:1000MHz fclk_2
 *           others:286MHz fclk/7
 */
//crt_clk_div_mux4_ns #(8)
//    u_crt_dspa_clk_mux_div(
//        .clk0               (fclk_div2                ),
//        .clk1               (fclk_div3                ),
//        .clk2               (fclk_div5                ),
//        .clk3               (fclk_div7                ),
//        .reset_n            (crt_reset_n              ),
//        .force_oscin_clk    (1'b0                     ),
//        .cts_oscin_clk      (1'b0                     ),
//
//        .clk_div            (hi_dsp_clk_cntl[7:0]     ),
//        .clk_en             (hi_dsp_clk_cntl[15]      ),
//        .clk_sel            (hi_dsp_clk_cntl[9:8]     ),
//        .clk_out            (cts_dspa_clk             ));

static void clk_util_set_dsp_clk(uint32_t id, uint32_t freq_sel)
{
	uint32_t clk_sel = 0;
	uint32_t clk_div = 0;
	uint32_t read;
	void __iomem *reg;

	reg = get_hiu_addr() + REG_HHI_DSP_CLK_CNTL;
	switch (freq_sel) {
	case 1:
		clk_sel = 2;
		clk_div = 0;
		pr_debug("CLK_UTIL:dsp:fclk/5:400MHz\n");
	break;
	case 2:
		clk_sel = 0;
		clk_div = 1;
		pr_debug("CLK_UTIL:dsp:fclk/4:500MHz\n");
	break;
	case 3:
		//clk_sel = 1;
		//clk_div = 0;
		//pr_debug("CLK_UTIL:dsp:fclk/3:667MHz\n");
	break;
	case 4:
		clk_sel = 1;
		clk_div = 1;
		pr_debug("CLK_UTIL:dsp:fclk/3/2:333MHz\n");
	break;
	case 5:
		clk_sel = 0;
		clk_div = 3;
		pr_debug("CLK_UTIL:dsp:fclk/2:250MHz\n");
	break;
	case 6:
		clk_sel = 2;
		clk_div = 1;
		pr_debug("CLK_UTIL:dsp:fclk/4/2:200MHz\n");
	break;
	case 7:
		clk_sel = 2;
		clk_div = 3;
		pr_debug("CLK_UTIL:dsp:fclk/4/4:100MHz\n");
	break;
	case 8:
		clk_sel = 4;
		clk_div = 0;
		pr_debug("CLK_UTIL:dsp:oscin:24MHz\n");
	break;
	case 10:
		//clk_sel = 0;
		//clk_div = 0;
		//pr_debug("CLK_UTIL:dsp:fclk/2:1000MHz\n");
	break;
	default:
		clk_sel = 3;
		clk_div = 0;
		pr_debug("CLK_UTIL:dsp:fclk/7:286MHz\n");
	break;
	}

	read = readl(reg);
	if (id == 0) {
		//read = (read & ~((0x3<<8) | (0xff<<0)));
		//read = read | ((1<<15) | (clk_sel<<8) | (clk_div<<0));
		if (read & (1 << 15)) {  //if sync_mux ==1, sel mux 0
			read &= (~((1 << 15) | (0xf << 0) | (0x7 << 4)));
			read |= (1 << 7) | (clk_div << 0) | (clk_sel << 4);
		} else {
			read &= (~((1 << 15) | (0xf << 8) | (0x7 << 12)));
			read |= (1 << 7) | (clk_div << 8);
			read |= (clk_sel << 12) | (1 << 15);
		}
	} else {
		//read = (read & ~((0x3<<24) | (0xff<<16)));
		//read = read | ((1<<31) | (clk_sel<<24) | (clk_div<<16));
		if (read & (1 << 31)) {  //if sync_mux ==1, sel mux 0
			read &= (~((1 << 31) | (0xf << 16) | (0x7 << 20)));
			read |= (1 << 23) | (clk_div << 16) | (clk_sel << 20);
		} else {
			read &= (~((1 << 31) | (0xf << 24) | (0x7 << 28)));
			read |= (1 << 23) | (clk_div << 24);
			read |= (clk_sel << 28) | (1 << 31);
		}
	}
	writel(read, reg);

	pr_debug("%s\n", __func__);

}

static void start_dsp(uint32_t dsp_id, uint32_t reset_addr)
{
	uint32_t StatVectorSel;
	uint32_t strobe = 1;
	uint32_t tmp;
	uint32_t read;
	void __iomem *reg;

	reg = get_dsp_addr(dsp_id);
	StatVectorSel = (reset_addr != 0xfffa0000);

	// the bit 0 is no use, dsp in tm2 is non-secure
	tmp = 0x1 |  StatVectorSel<<1 | strobe<<2;
	scpi_init_dsp_cfg0(dsp_id, reset_addr, tmp);


	read = readl(reg+REG_DSP_CFG0);
	pr_debug("REG_DSP_CFG0 read=0x%x\n", read);
	if (dsp_id == 0) {
		read = read & (~((1 << 31) | (1 << 30) | (0xffff << 0)));
		read = read | (1 << 29)  | (0 << 0); // 29 irq_clk_en
	} else {
		read = read & (~((1 << 31) | (1 << 30) | (0xffff << 0)));
		read = read | (1 << 29) | (1 << 0);
	}
	writel(read, reg+REG_DSP_CFG0);
	tm2_dsp_top_reg_dump("REG_DSP_CFG0", reg, REG_DSP_CFG0);

	pr_debug("%s\n", __func__);

}

static void power_switch_to_dsp_a(int pwr_cntl)
{
	uint32_t tmp;
	void __iomem *reg;

	pr_info("[PWR]: Power %s DSP A\n", pwr_cntl?"On":"Off");
	if (pwr_cntl == 1) {
		// Powerup dsp a
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_SLEEP0;
		tmp = readl(reg) & (~(0x1<<21));
		writel(tmp, reg);// power on
		udelay(5);

		// Power up memory
		reg = get_hiu_addr() + REG_HHI_DSP_MEM_PD_REG0;
		tmp = readl(reg) & (~(0xffff<<0));
		writel(tmp, reg);
		udelay(5);

		// reset
		reg = get_reset_addr() + REG_RESET4_LEVEL;
		tmp = readl(reg) & (~(0x1<<0));
		writel(tmp, reg);

		reg = get_reset_addr() + REG_RESET1_LEVEL;
		tmp = readl(reg) & (~(0x1<<20));
		writel(tmp, reg);

		// remove isolation
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_ISO0;
		tmp = readl(reg) & (~(0x1<<21));
		writel(tmp, reg);

		// pull up reset
		reg = get_reset_addr() + REG_RESET4_LEVEL;
		tmp = readl(reg) | (0x1<<0);
		writel(tmp, reg);

		reg = get_reset_addr() + REG_RESET1_LEVEL;
		tmp = readl(reg) | (0x1<<20);
		writel(tmp, reg);
	} else {
		// reset
		reg = get_reset_addr() + REG_RESET4_LEVEL;
		tmp = readl(reg) & (~(0x1<<0));
		writel(tmp, reg);

		reg = get_reset_addr() + REG_RESET1_LEVEL;
		tmp = readl(reg) & (~(0x1<<20));
		writel(tmp, reg);

		// add isolation
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_ISO0;
		tmp = readl(reg) | (0x1<<21);
		writel(tmp, reg);
		udelay(5);

		// power down memory
		reg = get_hiu_addr() + REG_HHI_DSP_MEM_PD_REG0;
		tmp = readl(reg) | (0xffff<<0);
		writel(tmp, reg);
		udelay(5);

		// power down dsp a
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_SLEEP0;
		tmp = readl(reg) | (0x1<<21);
		writel(tmp, reg);
		udelay(5);
	}

}

static void power_switch_to_dsp_b(int pwr_cntl)
{
	uint32_t tmp;
	void __iomem *reg;

	if (pwr_cntl == 1) {
		pr_info("[PWR]: Power on DSP B\n");
		// Powerup dsp a
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_SLEEP0;
		tmp = readl(reg) & (~(0x1<<22));
		writel(tmp, reg);// power on
		udelay(5);

		// Power up memory
		reg = get_hiu_addr() + REG_HHI_DSP_MEM_PD_REG0;
		tmp = readl(reg) & (~(0xffff<<16));
		writel(tmp, reg);
		udelay(5);

		// reset
		reg = get_reset_addr() + REG_RESET4_LEVEL;
		tmp = readl(reg) & (~(0x1<<1));
		writel(tmp, reg);

		reg = get_reset_addr() + REG_RESET1_LEVEL;
		tmp = readl(reg) & (~(0x1<<21));
		writel(tmp, reg);

		// remove isolation
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_ISO0;
		tmp = readl(reg) & (~(0x1<<22));
		writel(tmp, reg);

		// pull up reset
		reg = get_reset_addr() + REG_RESET4_LEVEL;
		tmp = readl(reg) | (0x1<<1);
		writel(tmp, reg);

		reg = get_reset_addr() + REG_RESET1_LEVEL;
		tmp = readl(reg) | (0x1<<21);
		writel(tmp, reg);
	} else {
		pr_info("[PWR]: Power off DSP B\n");
		// reset
		reg = get_reset_addr() + REG_RESET4_LEVEL;
		tmp = readl(reg) & (~(0x1<<1));
		writel(tmp, reg);

		reg = get_reset_addr() + REG_RESET1_LEVEL;
		tmp = readl(reg) & (~(0x1<<21));
		writel(tmp, reg);

		// add isolation
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_ISO0;
		tmp = readl(reg) | (0x1<<22);
		writel(tmp, reg);
		udelay(5);

		// power down memory
		reg = get_hiu_addr() + REG_HHI_DSP_MEM_PD_REG0;
		tmp = readl(reg) | (0xffff<<16);
		writel(tmp, reg);
		udelay(5);

		// power down dsp a
		reg = get_ao_rti_addr() + REG_AO_RTI_GEN_PWR_SLEEP0;
		tmp = readl(reg) | (0x1<<22);
		writel(tmp, reg);
		udelay(5);
	}

}

static void tm2_dsp_power_switch(int dsp_id, int pwr_cntl)
{
	if (dsp_id == 0)
		power_switch_to_dsp_a(pwr_cntl);
	else if (dsp_id == 1)
		power_switch_to_dsp_b(pwr_cntl);
	else
		pr_err("%s param: dsp_id=%d error\n", __func__, dsp_id);
}

void tm2_dsp_top_regs_dump(int dsp_id)
{
	void __iomem *reg;

	pr_debug("%s\n", __func__);

	reg = get_dsp_addr(dsp_id);
	pr_debug("%s base=%p\n", __func__, reg);

	tm2_dsp_top_reg_dump("REG_DSP_CFG0", reg, REG_DSP_CFG0);
	tm2_dsp_top_reg_dump("REG_DSP_CFG1", reg, REG_DSP_CFG1);
	tm2_dsp_top_reg_dump("REG_DSP_CFG2", reg, REG_DSP_CFG2);
	tm2_dsp_top_reg_dump("REG_DSP_RESET_VEC", reg, REG_DSP_RESET_VEC);

	reg = get_hiu_addr();
	tm2_dsp_top_reg_dump("REG_HHI_DSP_CLK_CNTL", reg,
				 REG_HHI_DSP_CLK_CNTL);
	tm2_dsp_top_reg_dump("REG_HHI_DSP_MEM_PD_REG0", reg,
				REG_HHI_DSP_MEM_PD_REG0);

	reg = get_ao_rti_addr();
	tm2_dsp_top_reg_dump("REG_AO_RTI_GEN_PWR_SLEEP0", reg,
				REG_AO_RTI_GEN_PWR_SLEEP0);
	tm2_dsp_top_reg_dump("REG_AO_RTI_GEN_PWR_ISO0", reg,
				REG_AO_RTI_GEN_PWR_ISO0);

	reg = get_reset_addr();
	tm2_dsp_top_reg_dump("REG_RESET1_LEVEL", reg, REG_RESET1_LEVEL);
	tm2_dsp_top_reg_dump("REG_RESET4_LEVEL", reg, REG_RESET4_LEVEL);

}

static void tm2_dsp_set_clk(int dsp_id, int freq_sel)
{
	clk_util_set_dsp_clk(dsp_id, freq_sel);
}

void tm2_dsp_hw_init(int dsp_id, int freq_sel)
{
	int pwr_cntl = 1;

	tm2_dsp_regs_iomem_init();
	tm2_dsp_set_clk(dsp_id, freq_sel);
	tm2_dsp_power_switch(dsp_id, pwr_cntl);

	pr_debug("%s done\n", __func__);
}

void tm2_dsp_start(int dsp_id, int freq_sel)
{
	start_dsp(dsp_id, freq_sel);
}

void tm2_dsp_bootup(int dsp_id, uint32_t reset_addr, int freq_sel)
{
	int pwr_cntl = 1;

	//reset_addr = 0x30000000;
	//dsp_id = 0;
	freq_sel = 1;

	pr_debug("%s dsp_id=%d, address=0x%x\n",
		__func__, dsp_id, reset_addr);

	tm2_dsp_set_clk(dsp_id, freq_sel);
	tm2_dsp_power_switch(dsp_id, pwr_cntl);
	tm2_dsp_start(dsp_id, reset_addr);

	msleep(5*1000);
	tm2_dsp_top_regs_dump(dsp_id);
}
