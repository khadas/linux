// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "demod_func.h"
#include "amlfrontend.h"
#include "dvbt_func.h"
#include "aml_demod.h"

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/delay.h>
/*#include "acf_filter_coefficient.h"*/
#include <linux/mutex.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>


MODULE_PARM_DESC(debug_demod, "\n\t\t Enable frontend debug information");
static int debug_demod;
module_param(debug_demod, int, 0644);

MODULE_PARM_DESC(demod_mobile_power, "\n\t\t demod_mobile_power debug information");
static int demod_mobile_power = 100;
module_param(demod_mobile_power, int, 0644);

/* protect register access */
static struct mutex mp;
static struct mutex dtvpll_init_lock;
static int dtvpll_init;
/*static int dtmb_spectrum = 2;*/
#if defined DEMOD_FPGA_VERSION
static int fpga_version = 1;
#else
static int fpga_version = -1;
#endif

void dtvpll_lock_init(void)
{
	mutex_init(&dtvpll_init_lock);
}

void dtvpll_init_flag(int on)
{
	mutex_lock(&dtvpll_init_lock);
	dtvpll_init = on;
	mutex_unlock(&dtvpll_init_lock);
	pr_err("%s %d\n", __func__, on);
}

int get_dtvpll_init_flag(void)
{
	int val;

	/*mutex_lock(&dtvpll_init_lock);*/
	val = dtvpll_init;
	/*mutex_unlock(&dtvpll_init_lock);*/
	if (!val)
		pr_err("%s: %d\n", __func__, val);
	return val;
}


void adc_dpll_setup(int clk_a, int clk_b, int clk_sys, struct aml_demod_sta *demod_sta)
{
	int unit, found, ena, enb, div2;
	int pll_m, pll_n, pll_od_a, pll_od_b, pll_xd_a, pll_xd_b;
	long freq_osc, freq_dco, freq_b, freq_a, freq_sys;
	long freq_b_act, freq_a_act, freq_sys_act, err_tmp, best_err;
	union adc_pll_cntl adc_pll_cntl;
	union adc_pll_cntl2 adc_pll_cntl2;
	union adc_pll_cntl3 adc_pll_cntl3;
	union adc_pll_cntl4 adc_pll_cntl4;
	union demod_dig_clk dig_clk_cfg;

	struct dfe_adcpll_para ddemod_pll;
	int sts_pll = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		dtvpll_init_flag(1);
		return;
	}

	adc_pll_cntl.d32 = 0;
	adc_pll_cntl2.d32 = 0;
	adc_pll_cntl3.d32 = 0;
	adc_pll_cntl4.d32 = 0;

	PR_DBG("target clk_a %d  clk_b %d\n", clk_a, clk_b);

	unit = 10000;		/* 10000 as 1 MHz, 0.1 kHz resolution. */
	freq_osc = 24 * unit;

	if (clk_a < 1000)
		freq_a = clk_a * unit;
	else
		freq_a = clk_a * (unit / 1000);

	if (clk_b < 1000)
		freq_b = clk_b * unit;
	else
		freq_b = clk_b * (unit / 1000);

	ena = clk_a > 0 ? 1 : 0;
	enb = clk_b > 0 ? 1 : 0;

	if (ena || enb)
		adc_pll_cntl3.b.enable = 1;
	adc_pll_cntl3.b.reset = 1;

	found = 0;
	best_err = 100 * unit;
	pll_od_a = 1;
	pll_od_b = 1;
	pll_n = 1;
	for (pll_m = 1; pll_m < 512; pll_m++) {
		/* for (pll_n=1; pll_n<=5; pll_n++) { */
		#if 0
		if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
			|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu())) {
			freq_dco = freq_osc * pll_m / pll_n / 2;
			/*txl add div2*/
			if (freq_dco < 700 * unit || freq_dco > 1000 * unit)
				continue;
		} else {
			freq_dco = freq_osc * pll_m / pll_n;
			if (freq_dco < 750 * unit || freq_dco > 1550 * unit)
				continue;
		}
		#else
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			/*txl add div2*/
			freq_dco = freq_osc * pll_m / pll_n / 2;
			if (freq_dco < 700 * unit || freq_dco > 1000 * unit)
				continue;

		} else {
			freq_dco = freq_osc * pll_m / pll_n;
			if (freq_dco < 750 * unit || freq_dco > 1550 * unit)
				continue;

		}
		#endif
		pll_xd_a = freq_dco / (1 << pll_od_a) / freq_a;
		pll_xd_b = freq_dco / (1 << pll_od_b) / freq_b;

		freq_a_act = freq_dco / (1 << pll_od_a) / pll_xd_a;
		freq_b_act = freq_dco / (1 << pll_od_b) / pll_xd_b;

		err_tmp = (freq_a_act - freq_a) * ena + (freq_b_act - freq_b) *
		    enb;

		if (err_tmp >= best_err)
			continue;

		adc_pll_cntl.b.pll_m = pll_m;
		adc_pll_cntl.b.pll_n = pll_n;
		adc_pll_cntl.b.pll_od0 = pll_od_b;
		adc_pll_cntl.b.pll_od1 = pll_od_a;
		adc_pll_cntl.b.pll_xd0 = pll_xd_b;
		adc_pll_cntl.b.pll_xd1 = pll_xd_a;
		#if 0
		if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
			|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu())) {
			adc_pll_cntl4.b.pll_od3 = 0;
			adc_pll_cntl.b.pll_od2 = 0;
		} else {
			adc_pll_cntl2.b.div2_ctrl =
				freq_dco > 1000 * unit ? 1 : 0;
		}
		#else
		/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {*/
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			adc_pll_cntl4.b.pll_od3 = 0;
			adc_pll_cntl.b.pll_od2 = 0;

		} else {
			adc_pll_cntl2.b.div2_ctrl =
				freq_dco > 1000 * unit ? 1 : 0;

		}
		#endif
		found = 1;
		best_err = err_tmp;
		/* } */
	}

	pll_m = adc_pll_cntl.b.pll_m;
	pll_n = adc_pll_cntl.b.pll_n;
	pll_od_b = adc_pll_cntl.b.pll_od0;
	pll_od_a = adc_pll_cntl.b.pll_od1;
	pll_xd_b = adc_pll_cntl.b.pll_xd0;
	pll_xd_a = adc_pll_cntl.b.pll_xd1;

	#if 0
	if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
		|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu()))
		div2 = 1;
	else
		div2 = adc_pll_cntl2.b.div2_ctrl;

	#else
	/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		div2 = 1;
	else
		div2 = adc_pll_cntl2.b.div2_ctrl;

	#endif
	/*
	 * p_adc_pll_cntl  =  adc_pll_cntl.d32;
	 * p_adc_pll_cntl2 = adc_pll_cntl2.d32;
	 * p_adc_pll_cntl3 = adc_pll_cntl3.d32;
	 * p_adc_pll_cntl4 = adc_pll_cntl4.d32;
	 */
	adc_pll_cntl3.b.reset = 0;
	/* *p_adc_pll_cntl3 = adc_pll_cntl3.d32; */
	if (!found) {
		PR_DBG(" ERROR can't setup %7ld kHz %7ld kHz\n",
		       freq_b / (unit / 1000), freq_a / (unit / 1000));
	} else {
	#if 0
		if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
			|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu()))
			freq_dco = freq_osc * pll_m / pll_n / 2;
		else
			freq_dco = freq_osc * pll_m / pll_n;
	#else
		/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))*/
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			freq_dco = freq_osc * pll_m / pll_n / 2;
		else
			freq_dco = freq_osc * pll_m / pll_n;
	#endif
		PR_DBG(" ADC PLL  M %3d   N %3d\n", pll_m, pll_n);
		PR_DBG(" ADC PLL DCO %ld kHz\n", freq_dco / (unit / 1000));

		PR_DBG(" ADC PLL XD %3d  OD %3d\n", pll_xd_b, pll_od_b);
		PR_DBG(" ADC PLL XD %3d  OD %3d\n", pll_xd_a, pll_od_a);

		freq_a_act = freq_dco / (1 << pll_od_a) / pll_xd_a;
		freq_b_act = freq_dco / (1 << pll_od_b) / pll_xd_b;

		PR_DBG(" B %7ld kHz %7ld kHz\n",
		       freq_b / (unit / 1000), freq_b_act / (unit / 1000));
		PR_DBG(" A %7ld kHz %7ld kHz\n",
		       freq_a / (unit / 1000), freq_a_act / (unit / 1000));

		if (clk_sys > 0) {
			dig_clk_cfg.b.demod_clk_en = 1;
			dig_clk_cfg.b.demod_clk_sel = 3;
			if (clk_sys < 1000)
				freq_sys = clk_sys * unit;
			else
				freq_sys = clk_sys * (unit / 1000);

			dig_clk_cfg.b.demod_clk_div = freq_dco / (1 + div2) /
			    freq_sys - 1;
			freq_sys_act = freq_dco / (1 + div2) /
			    (dig_clk_cfg.b.demod_clk_div + 1);
			PR_DBG(" SYS %7ld kHz div %d+1  %7ld kHz\n",
			       freq_sys / (unit / 1000),
			       dig_clk_cfg.b.demod_clk_div,
			       freq_sys_act / (unit / 1000));
		} else {
			dig_clk_cfg.b.demod_clk_en = 0;
		}

		/* *p_demod_dig_clk = dig_clk_cfg.d32; */
	}

	ddemod_pll.adcpllctl = adc_pll_cntl.d32;
	ddemod_pll.demodctl = dig_clk_cfg.d32;
	ddemod_pll.atsc = 0;

	switch (demod_sta->delsys) {
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ddemod_pll.atsc = 1;
		break;

	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	sts_pll = adc_set_pll_cntl(1, ADC_DTV_DEMODPLL, &ddemod_pll);
#endif
	if (sts_pll < 0) {
		/*set pll fail*/
		PR_ERR("%s:set pll fail! please check!\n", __func__);
	} else {
		dtvpll_init_flag(1);
	}

	debug_adc_pll();

}

void demod_set_adc_core_clk(int adc_clk, int sys_clk, struct aml_demod_sta *demod_sta)
{
	adc_dpll_setup(25, adc_clk, sys_clk, demod_sta);
}

void demod_set_cbus_reg(unsigned int data, unsigned int addr)
{
	void __iomem *vaddr;

	PR_DBG("[cbus][write]%x\n", (IO_CBUS_PHY_BASE + (addr << 2)));
	vaddr = ioremap((IO_CBUS_PHY_BASE + (addr << 2)), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
}

unsigned int demod_read_cbus_reg(unsigned int addr)
{
/* return __raw_readl(CBUS_REG_ADDR(addr)); */
	unsigned int tmp;
	void __iomem *vaddr;

	vaddr = ioremap((IO_CBUS_PHY_BASE + (addr << 2)), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
/* tmp = aml_read_cbus(addr); */
	PR_DBG("[cbus][read]%x,data is %x\n",
	(IO_CBUS_PHY_BASE + (addr << 2)), tmp);
	return tmp;
}

void demod_set_ao_reg(unsigned int data, unsigned int addr)
{
	writel(data, gbase_aobus() + addr);
}

unsigned int demod_read_ao_reg(unsigned int addr)
{
	unsigned int tmp;

	tmp = readl(gbase_aobus() + addr);

	return tmp;
}


void demod_set_demod_reg(unsigned int data, unsigned int addr)
{
	void __iomem *vaddr;

	mutex_lock(&mp);
	vaddr = ioremap((addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);
}

void demod_set_tvfe_reg(unsigned int data, unsigned int addr)
{
	void __iomem *vaddr;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/
	vaddr = ioremap((addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);
}

unsigned int demod_read_demod_reg(unsigned int addr)
{
	unsigned int tmp;
	void __iomem *vaddr;

	mutex_lock(&mp);
	vaddr = ioremap((addr), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);

	return tmp;
}

void power_sw_hiu_reg(int on)
{
	if (on == PWR_ON) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG, 0);
		else
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
			(dd_tvafe_hiu_reg_read(HHI_DEMOD_MEM_PD_REG)
			& (~0x2fff)));
	} else {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
				0xffffffff);
		else
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
			(dd_tvafe_hiu_reg_read
			(HHI_DEMOD_MEM_PD_REG) | 0x2fff));

	}
}
void power_sw_reset_reg(int en)
{
	if (en) {
		reset_reg_write(RESET_RESET0_LEVEL,
			(reset_reg_read(RESET_RESET0_LEVEL) & (~(0x1 << 8))));
	} else {
		reset_reg_write(RESET_RESET0_LEVEL,
			(reset_reg_read(RESET_RESET0_LEVEL) | (0x1 << 8)));

	}
}

void demod_power_switch(int pwr_cntl)
{
	int reg_data;
	unsigned int pwr_slp_bit;
	unsigned int pwr_iso_bit;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return;
	}

	/* DO NOT set any power related regs in T5 */
	if (devp->data->hw_ver >= DTVDEMOD_HW_T5) {
		if (devp->data->hw_ver == DTVDEMOD_HW_T5D) {
			/* UART pin mux for RISCV debug */
			/* demod_set_ao_reg(0x104411, 0x14); */
		}

		return;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		pwr_slp_bit = 23;
		pwr_iso_bit = 23;
	} else {
		pwr_slp_bit = 10;
		pwr_iso_bit = 14;
	}

	if (is_meson_gxlx_cpu()) {
		PR_DBG("[PWR]: GXLX not support power switch,power mem\n");
		power_sw_hiu_reg(PWR_ON);
	} else if (pwr_cntl == PWR_ON) {
		PR_DBG("[PWR]: Power on demod_comp %x,%x\n",
		       AO_RTI_GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_ISO0);
		/* Powerup demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data & (~(0x1 << pwr_slp_bit))),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* Power up memory */
		power_sw_hiu_reg(PWR_ON);
		/* reset */
		power_sw_reset_reg(1);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			/* remove isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) &
				  (~(0x1 << pwr_iso_bit))),
				  AO_RTI_GEN_PWR_ISO0);
		else
			/* remove isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) &
				  (~(0x3 << pwr_iso_bit))),
				  AO_RTI_GEN_PWR_ISO0);

		/* pull up reset */
		power_sw_reset_reg(0);
	} else {
		PR_DBG("[PWR]: Power off demod_comp\n");

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			/* add isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) |
				  (0x1 << pwr_iso_bit)), AO_RTI_GEN_PWR_ISO0);
		else
			/* add isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) |
				  (0x3 << pwr_iso_bit)), AO_RTI_GEN_PWR_ISO0);

		/* power down memory */
		power_sw_hiu_reg(PWR_OFF);
		/* power down demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data | (0x1 << pwr_slp_bit)),
				 AO_RTI_GEN_PWR_SLEEP0);
	}
}

/* 0:DVBC/J.83B, 1:DVBT/ISDBT, 2:ATSC, 3:DTMB */
void demod_set_mode_ts(enum fe_delivery_system delsys)
{
	union demod_cfg0 cfg0;

	cfg0.b.adc_format = 1;
	cfg0.b.adc_regout = 1;

	switch (delsys) {
	case SYS_DTMB:
		cfg0.b.ts_sel = 1;
		cfg0.b.mode = 1;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			cfg0.b.adc_format = 0;
			cfg0.b.adc_regout = 0;
		}
		break;

	case SYS_ISDBT:
		cfg0.b.ts_sel = 1<<1;
		cfg0.b.mode = 1<<1;
		cfg0.b.adc_format = 0;
		cfg0.b.adc_regout = 0;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		cfg0.b.ts_sel = 1<<2;
		cfg0.b.mode = 1<<2;
		cfg0.b.adc_format = 0;
		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			cfg0.b.adc_regout = 1;
			cfg0.b.adc_regadj = 2;
		}
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu()) {
			cfg0.b.ts_sel = 2;
			cfg0.b.mode = 7;
			cfg0.b.adc_format = 1;
			cfg0.b.adc_regout = 0;
		} else {
			cfg0.b.ts_sel = 1<<3;
			cfg0.b.mode = 1<<3;
			cfg0.b.adc_format = 0;
			cfg0.b.adc_regout = 0;
		}
		break;

	default:
		break;
	}

	demod_top_write_reg(DEMOD_TOP_REG0, cfg0.d32);
}

void clocks_set_sys_defaults(struct dvb_frontend *fe, struct aml_demod_sta *demod_sta,
				unsigned int adc_clk)
{
	union demod_cfg2 cfg2;
	int sts_pll = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	struct dfe_adcpll_para ddemod_pll;
#endif

	demod_power_switch(PWR_ON);

#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	adc_set_ddemod_default(demod_sta->delsys);
#endif
	demod_set_demod_default();
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	ddemod_pll.delsys = demod_sta->delsys;
	ddemod_pll.adc_clk = adc_clk;
	sts_pll = adc_set_pll_cntl(1, ADC_DTV_DEMOD, &ddemod_pll);
#endif
	if (sts_pll < 0) {
		/*set pll fail*/
		PR_ERR("%s:set pll default fail! please check!\n", __func__);
	}

	demod_set_mode_ts(demod_sta->delsys);
	cfg2.b.biasgen_en = 1;
	cfg2.b.en_adc = 1;
	demod_top_write_reg(DEMOD_TOP_REG8, cfg2.d32);
	debug_check_reg_val(REG_M_DEMOD, DEMOD_TOP_REG0);

}

void dtmb_write_reg(int reg_addr, int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(reg_data, gbase_dtmb() + (reg_addr << 2));

	mutex_unlock(&mp);
}

unsigned int dtmb_read_reg(unsigned int reg_addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	/*mutex_lock(&mp);*/

	tmp = readl(gbase_dtmb() + (reg_addr << 2));

	/*mutex_unlock(&mp);*/

	return tmp;
}

void dvbt_isdbt_wr_reg(unsigned int addr, unsigned int data)
{
	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/
	writel(data, gbase_dvbt_isdbt() + addr);
	/*mutex_unlock(&mp);*/
}

void dvbt_isdtb_wr_reg_new(unsigned int addr, unsigned int data)
{
	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/

	writel(data, gbase_dvbt_isdbt() + (addr << 2));

	/*mutex_unlock(&mp);*/
}

void dvbt_isdbt_wr_bits_new(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len)
{
	unsigned int val;

	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/
	val = readl(gbase_dvbt_isdbt() + (reg_addr << 2));
	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((reg_data) & ((1L << (len)) - 1)) << (start));
	writel(val, gbase_dvbt_isdbt() + (reg_addr << 2));
	/*mutex_unlock(&mp);*/
}

unsigned int dvbt_isdbt_rd_reg(unsigned int addr)
{
	unsigned int tmp = 0;

	if (!get_dtvpll_init_flag())
		return 0;

	/*mutex_lock(&mp);*/

	tmp = readl(gbase_dvbt_isdbt() + addr);

	/*mutex_unlock(&mp);*/

	return tmp;
}

unsigned int dvbt_isdbt_rd_reg_new(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_dvbt_isdbt() + (addr << 2));

	mutex_unlock(&mp);

	return tmp;
}

void dvbt_t2_wrb(unsigned int addr, char data)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag() || unlikely(!devp))
		return;

	mutex_lock(&mp);
	__raw_writeb(data, gbase_dvbt_t2() + addr);

	if (devp->print_on)
		PR_INFO("t2 wrB 0x%x=0x%x\n", addr, data);

	mutex_unlock(&mp);
}

void dvbt_t2_wr_byte_bits(u32 addr, const u32 data, const u32 start, const u32 len)
{
	unsigned int val;

	if (!get_dtvpll_init_flag())
		return;

	val = dvbt_t2_rdb(addr);
	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((data) & ((1L << (len)) - 1)) << (start));
	dvbt_t2_wrb(addr, val);
}

void dvbt_t2_write_w(unsigned int addr, unsigned int data)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag() || unlikely(!devp))
		return;

	writel(data, gbase_dvbt_t2() + addr);

	if (devp->print_on)
		PR_INFO("t2 wrW 0x%x=0x%x\n", addr, data);
}

void dvbt_t2_wr_word_bits(u32 addr, const u32 data, const u32 start, const u32 len)
{
	unsigned int val;

	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/
	val = readl(gbase_dvbt_t2() + addr);
	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((data) & ((1L << (len)) - 1)) << (start));
	writel(val, gbase_dvbt_t2() + addr);
	/*mutex_unlock(&mp);*/
}

unsigned int dvbt_t2_read_w(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	tmp = readl(gbase_dvbt_t2() + addr);

	return tmp;
}

char dvbt_t2_rdb(unsigned int addr)
{
	char tmp = 0;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);
	tmp = __raw_readb(gbase_dvbt_t2() + addr);
	mutex_unlock(&mp);

	return tmp;
}

/* only for T5D T2 use, have to set top 0x10 = 0x97 before any access */
void riscv_ctl_write_reg(unsigned int addr, unsigned int data)
{
	dvbt_t2_write_w(addr, data);
}

void dvbs_wr_byte(unsigned int addr, char data)
{
	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/

	__raw_writeb(data, gbase_dvbs() + addr);

	/*mutex_unlock(&mp);*/
}

char dvbs_rd_byte(unsigned int addr)
{
	char tmp = 0;

	if (!get_dtvpll_init_flag())
		return 0;

	/*mutex_lock(&mp);*/

	tmp = __raw_readb(gbase_dvbs() + addr);

	/*mutex_unlock(&mp);*/

	return tmp;
}

void dvbs_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len)
{
	unsigned int val;

	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/
	val = readl(gbase_dvbs() + (reg_addr << 2));
	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((reg_data) & ((1L << (len)) - 1)) << (start));
	writel(val, gbase_dvbs() + (reg_addr << 2));
	/*mutex_unlock(&mp);*/
}

void dvbs_write_reg(unsigned int addr, unsigned int data)
{
	if (!get_dtvpll_init_flag())
		return;

	/*mutex_lock(&mp);*/

	writel(data, gbase_dvbs() + (addr << 2));

	/*mutex_unlock(&mp);*/
}

unsigned int dvbs_read_reg(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	/*mutex_lock(&mp);*/

	tmp = readl(gbase_dvbs() + (addr << 2));

	/*mutex_unlock(&mp);*/

	return tmp;
}

void atsc_write_reg(unsigned int reg_addr, unsigned int reg_data)
{
	unsigned int data;

	if (!get_dtvpll_init_flag())
		return;

	data = (reg_addr & 0xffff) << 8 | (reg_data & 0xff);

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/
	writel(data, gbase_atsc());
	mutex_unlock(&mp);
}

unsigned int atsc_read_reg(unsigned int reg_addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	writel((reg_addr & 0xffff) << 8, gbase_atsc() + 4);
	tmp = readl(gbase_atsc());

	mutex_unlock(&mp);

	return tmp & 0xff;
}

/*TL1*/
void atsc_write_reg_v4(unsigned int addr, unsigned int data)
{
	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_atsc() + (addr << 2));

	mutex_unlock(&mp);
}

unsigned int atsc_read_reg_v4(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_atsc() + (addr << 2));

	mutex_unlock(&mp);

	return tmp;
}

unsigned int atsc_read_iqr_reg(void)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_atsc() + 8);

	mutex_unlock(&mp);

	PR_DBG("[atsc irq] is %x\n", tmp);
	return tmp & 0xffffffff;
}

void demod_init_mutex(void)
{
	mutex_init(&mp);
}

int demod_set_sys(struct amldtvdemod_device_s *devp, struct aml_demod_sta *demod_sta,
		  struct aml_demod_sys *demod_sys)
{
	unsigned int clk_adc, clk_dem;
	int nco_rate;
	struct dvb_frontend *fe = aml_get_fe();

	clk_adc = demod_sys->adc_clk;
	clk_dem = demod_sys->demod_clk;
	nco_rate = (clk_adc * 256) / clk_dem + 2;
	PR_DBG("%s,clk_adc is %d,clk_demod is %d\n", __func__, clk_adc, clk_dem);
	clocks_set_sys_defaults(fe, demod_sta, clk_adc);

	/* set adc clk */
	demod_set_adc_core_clk(clk_adc, clk_dem, demod_sta);

	switch (demod_sta->delsys) {
	case SYS_DTMB:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x10);
			usleep_range(1000, 1001);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			front_write_bits(AFIFO_ADC, nco_rate,
					 AFIFO_NCO_RATE_BIT, AFIFO_NCO_RATE_WID);
			front_write_bits(AFIFO_ADC, 1, ADC_2S_COMPLEMENT_BIT,
					 ADC_2S_COMPLEMENT_WID);
			front_write_bits(TEST_BUS, 1, DC_ARB_EN_BIT, DC_ARB_EN_WID);
		} else {
			demod_top_write_reg(DEMOD_TOP_REGC, 0x8);
			PR_DBG("[open arbit]dtmb\n");
		}
		break;

	case SYS_ISDBT:
		if (is_meson_txlx_cpu()) {
			demod_top_write_reg(DEMOD_TOP_REGC, 0x8);
		} else if (dtvdd_devp->data->hw_ver == DTVDEMOD_HW_T5D) {
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x10);
			usleep_range(1000, 1001);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			front_write_bits(AFIFO_ADC, nco_rate, AFIFO_NCO_RATE_BIT,
					 AFIFO_NCO_RATE_WID);
			front_write_bits(AFIFO_ADC, 1, ADC_2S_COMPLEMENT_BIT,
					 ADC_2S_COMPLEMENT_WID);
			front_write_reg(SFIFO_OUT_LENS, 1);
			front_write_bits(TEST_BUS, 1, DC_ARB_EN_BIT, DC_ARB_EN_WID);
		}
		PR_DBG("[open arbit]dvbt,txlx\n");
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x10);
			usleep_range(1000, 1001);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			front_write_bits(AFIFO_ADC, nco_rate, AFIFO_NCO_RATE_BIT,
					 AFIFO_NCO_RATE_WID);
			front_write_bits(AFIFO_ADC, 1, ADC_2S_COMPLEMENT_BIT,
					 ADC_2S_COMPLEMENT_WID);
		}
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (devp->data->hw_ver == DTVDEMOD_HW_S4) {
				demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
				demod_top_write_reg(DEMOD_TOP_REGC, 0x10);
				usleep_range(1000, 1001);
				demod_top_write_reg(DEMOD_TOP_REGC, 0xcc0011);
				front_write_bits(0x6c, nco_rate, AFIFO_NCO_RATE_BIT,
						 AFIFO_NCO_RATE_WID);
			} else {
				demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
				demod_top_write_reg(DEMOD_TOP_REGC, 0x10);
				usleep_range(1000, 1001);
				demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
				front_write_bits(AFIFO_ADC, nco_rate, AFIFO_NCO_RATE_BIT,
						 AFIFO_NCO_RATE_WID);
			}

			front_write_bits(AFIFO_ADC, 1, ADC_2S_COMPLEMENT_BIT,
					 ADC_2S_COMPLEMENT_WID);
		}
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		if (devp->data->hw_ver == DTVDEMOD_HW_T5D) {
			nco_rate = 0x80;
			demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x110010);
			usleep_range(1000, 1001);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
			front_write_bits(AFIFO_ADC, nco_rate, AFIFO_NCO_RATE_BIT,
					 AFIFO_NCO_RATE_WID);
			front_write_reg(0x22, 0x7200a06);
			front_write_reg(0x2f, 0x0);

			if (demod_sta->delsys == SYS_DVBT2) {
				front_write_reg(0x39, 0xc0002000);
				front_write_reg(TEST_BUS_VLD, 0x80000000);
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
				riscv_ctl_write_reg(0x30, 5);
				riscv_ctl_write_reg(0x30, 4);
				/* t2 top test bus addr */
				dvbt_t2_wr_word_bits(0x38, 0, 16, 4);
			} else {
				front_write_reg(0x39, 0x40001000);
			}

			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);

			if (demod_sta->delsys == SYS_DVBT2) {
				write_riscv_ram();
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
			}
		}
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		if (devp->data->hw_ver == DTVDEMOD_HW_T5D) {
			nco_rate = 0x0;
			demod_top_write_reg(DEMOD_TOP_REGC, 0x220011);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x220010);
			usleep_range(1000, 1001);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x220011);
			front_write_bits(AFIFO_ADC, nco_rate, AFIFO_NCO_RATE_BIT,
					 AFIFO_NCO_RATE_WID);
			front_write_reg(SFIFO_OUT_LENS, 0x0);
			front_write_reg(0x22, 0x7200a06);
		}
		break;

	default:
		break;
	}

	demod_sta->adc_freq = clk_adc;
	demod_sta->clk_freq = clk_dem;
	return 0;
}

/*TL1*/
void set_j83b_filter_reg_v4(void)
{
	//j83_1
	qam_write_reg(0x40, 0x36333c0d);
	qam_write_reg(0x41, 0xa110d01);
	qam_write_reg(0x42, 0xf0e4ea7a);
	qam_write_reg(0x43, 0x3c0010);
	qam_write_reg(0x44, 0x7e0065);


	//j83_2
	qam_write_reg(0x45, 0xb3a1905);
	qam_write_reg(0x46, 0x1c396e07);
	qam_write_reg(0x47, 0x3801cc08);
	qam_write_reg(0x48, 0x10800a2);


	qam_write_reg(0x49, 0x53b1f03);
	qam_write_reg(0x4a, 0x18377407);
	qam_write_reg(0x4b, 0x3401cf0b);
	qam_write_reg(0x4c, 0x10d00a1);
}

void demod_set_reg(struct aml_demod_reg *demod_reg)
{
	if (fpga_version == 1) {
#if defined DEMOD_FPGA_VERSION
		fpga_write_reg(demod_reg->mode, demod_reg->addr,
				demod_reg->val);
#endif
	} else {
		switch (demod_reg->mode) {
		case REG_MODE_CFG:
			demod_reg->addr = demod_reg->addr * 4
				+ gphybase_demod() + DEMOD_CFG_BASE;
			break;
		case REG_MODE_BASE:
			demod_reg->addr = demod_reg->addr + gphybase_demod();
			break;
		default:
			break;
		}

		switch (demod_reg->mode) {
		case REG_MODE_DTMB:
			dtmb_write_reg(demod_reg->addr, demod_reg->val);
			break;

		case REG_MODE_DVBT_ISDBT:
			if (demod_reg->access_mode == ACCESS_WORD)
				dvbt_isdtb_wr_reg_new(demod_reg->addr, demod_reg->val);
			else if (demod_reg->access_mode == ACCESS_BITS)
				dvbt_isdbt_wr_bits_new(demod_reg->addr, demod_reg->val,
						    demod_reg->start_bit, demod_reg->bit_width);
			break;

		case REG_MODE_DVBT_T2:
			if (demod_reg->access_mode == ACCESS_BYTE)
				dvbt_t2_wrb(demod_reg->addr, demod_reg->val);
			break;

		case REG_MODE_ATSC:
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				atsc_write_reg_v4(demod_reg->addr, demod_reg->val);
			else
				atsc_write_reg(demod_reg->addr, demod_reg->val);
			break;

		case REG_MODE_OTHERS:
			demod_set_cbus_reg(demod_reg->val, demod_reg->addr);
			break;

		case REG_MODE_DVBC_J83B:
			qam_write_reg(demod_reg->addr, demod_reg->val);
			break;

		case REG_MODE_FRONT:
			if (demod_reg->access_mode == ACCESS_BITS)
				front_write_bits(demod_reg->addr, demod_reg->val,
						 demod_reg->start_bit, demod_reg->bit_width);
			else if (demod_reg->access_mode == ACCESS_WORD)
				front_write_reg(demod_reg->addr, demod_reg->val);
			break;

		case REG_MODE_TOP:
			if (demod_reg->access_mode == ACCESS_BITS)
				demod_top_write_bits(demod_reg->addr, demod_reg->val,
						 demod_reg->start_bit, demod_reg->bit_width);
			else if (demod_reg->access_mode == ACCESS_WORD)
				demod_top_write_reg(demod_reg->addr, demod_reg->val);
			break;

		case REG_MODE_COLLECT_DATA:
			apb_write_reg_collect(demod_reg->addr, demod_reg->val);
			break;

		default:
			demod_set_demod_reg(demod_reg->val, demod_reg->addr);
			break;
		}

	}
}

void demod_get_reg(struct aml_demod_reg *demod_reg)
{
	if (fpga_version == 1) {
		#if defined DEMOD_FPGA_VERSION
		demod_reg->val = fpga_read_reg(demod_reg->mode,
			demod_reg->addr);
		#endif
	} else {
		if (demod_reg->mode == REG_MODE_CFG)
			demod_reg->addr = demod_reg->addr * 4
				+ gphybase_demod() + DEMOD_CFG_BASE;
		else if (demod_reg->mode == REG_MODE_BASE)
			demod_reg->addr = demod_reg->addr + gphybase_demod();

		switch (demod_reg->mode) {
		case REG_MODE_DTMB:
			demod_reg->val = dtmb_read_reg(demod_reg->addr);
			break;

		case REG_MODE_DVBT_ISDBT:
			demod_reg->val = dvbt_isdbt_rd_reg_new(demod_reg->addr);
			break;

		case REG_MODE_DVBT_T2:
			if (demod_reg->access_mode == ACCESS_BYTE)
				demod_reg->val = dvbt_t2_rdb(demod_reg->addr);

			break;

		case REG_MODE_ATSC:
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				demod_reg->val = atsc_read_reg_v4(demod_reg->addr);
			else
				demod_reg->val = atsc_read_reg(demod_reg->addr);
			break;

		case REG_MODE_DVBC_J83B:
			demod_reg->val = qam_read_reg(demod_reg->addr);
			break;

		case REG_MODE_FRONT:
			demod_reg->val = front_read_reg(demod_reg->addr);
			break;

		case REG_MODE_TOP:
			demod_reg->val = demod_top_read_reg(demod_reg->addr);
			break;

		case REG_MODE_OTHERS:
			demod_reg->val = demod_read_cbus_reg(demod_reg->addr);
			break;

		case REG_MODE_COLLECT_DATA:
			demod_reg->val = apb_read_reg_collect(demod_reg->addr);
			break;

		default:
			demod_reg->val = demod_read_demod_reg(demod_reg->addr);
			break;
		}
	}
}

void apb_write_reg_collect(unsigned int addr, unsigned int data)
{
	writel(data, ((void __iomem *)(phys_to_virt(addr))));
/* *(volatile unsigned int*)addr = data; */
}

unsigned long apb_read_reg_collect(unsigned long addr)
{
	unsigned long tmp;

	tmp = readl((void __iomem *)(phys_to_virt(addr)));

	return tmp & 0xffffffff;
}

void apb_write_reg(unsigned int addr, unsigned int data)
{
	demod_set_demod_reg(data, addr);
}

unsigned long apb_read_reg_high(unsigned long addr)
{
	return 0;
}

unsigned long apb_read_reg(unsigned long addr)
{
	return demod_read_demod_reg(addr);
}




int app_apb_read_reg(int addr)
{
	addr = DTMB_TOP_ADDR(addr);
	return (int)demod_read_demod_reg(addr);
}

int app_apb_write_reg(int addr, int data)
{
	addr = DTMB_TOP_ADDR(addr);
	demod_set_demod_reg(data, addr);
	return 0;
}
#if 0
void monitor_isdbt(void)
{
	int SNR;
	int SNR_SP = 500;
	int SNR_TPS = 0;
	int SNR_CP = 0;
	int timeStamp = 0;
	int SFO_residual = 0;
	int SFO_esti = 0;
	int FCFO_esti = 0;
	int FCFO_residual = 0;
	int AGC_Gain = 0;
	int RF_AGC = 0;
	int Signal_power = 0;
	int FECFlag = 0;
	int EQ_seg_ratio = 0;
	int tps_0 = 0;
	int tps_1 = 0;
	int tps_2 = 0;

	int time_stamp;
	int SFO;
	int FCFO;
	int timing_adj;
	int RS_CorrectNum;

	int cnt;
	int tmpAGCGain;

	tmpAGCGain = 0;
	cnt = 0;

/* app_apb_write_reg(0x8, app_apb_read_reg(0x8) & ~(1 << 17));*/
/* // TPS symbol index update : active high */
	time_stamp = app_apb_read_reg(0x07) & 0xffff;
	SNR = app_apb_read_reg(0x0a);
	FECFlag = (app_apb_read_reg(0x00) >> 11) & 0x3;
	SFO = app_apb_read_reg(0x47) & 0xfff;
	SFO_esti = app_apb_read_reg(0x60) & 0xfff;
	FCFO_esti = (app_apb_read_reg(0x60) >> 11) & 0xfff;
	FCFO = (app_apb_read_reg(0x26)) & 0xffffff;
	RF_AGC = app_apb_read_reg(0x0c) & 0x1fff;
	timing_adj = app_apb_read_reg(0x6f) & 0x1fff;
	RS_CorrectNum = app_apb_read_reg(0xc1) & 0xfffff;
	Signal_power = (app_apb_read_reg(0x1b)) & 0x1ff;
	EQ_seg_ratio = app_apb_read_reg(0x6e) & 0x3ffff;
	tps_0 = app_apb_read_reg(0x64);
	tps_1 = app_apb_read_reg(0x65);
	tps_2 = app_apb_read_reg(0x66) & 0xf;

	timeStamp = (time_stamp >> 8) * 68 + (time_stamp & 0x7f);
	SFO_residual = (SFO > 0x7ff) ? (SFO - 0x1000) : SFO;
	FCFO_residual = (FCFO > 0x7fffff) ? (FCFO - 0x1000000) : FCFO;
	/* RF_AGC          = (RF_AGC>0x3ff)? (RF_AGC - 0x800): RF_AGC; */
	FCFO_esti = (FCFO_esti > 0x7ff) ? (FCFO_esti - 0x1000) : FCFO_esti;
	SNR_CP = (SNR) & 0x3ff;
	SNR_TPS = (SNR >> 10) & 0x3ff;
	SNR_SP = (SNR >> 20) & 0x3ff;
	SNR_SP = (SNR_SP > 0x1ff) ? SNR_SP - 0x400 : SNR_SP;
	SNR_TPS = (SNR_TPS > 0x1ff) ? SNR_TPS - 0x400 : SNR_TPS;
	SNR_CP = (SNR_CP > 0x1ff) ? SNR_CP - 0x400 : SNR_CP;
	AGC_Gain = tmpAGCGain >> 4;
	tmpAGCGain = (AGC_Gain > 0x3ff) ? AGC_Gain - 0x800 : AGC_Gain;
	timing_adj = (timing_adj > 0xfff) ? timing_adj - 0x2000 : timing_adj;
	EQ_seg_ratio =
	    (EQ_seg_ratio > 0x1ffff) ? EQ_seg_ratio - 0x40000 : EQ_seg_ratio;

	PR_DBG
	    ("T %4x SP %3d TPS %3d CP %3d EQS %8x RSC %4d",
	     app_apb_read_reg(0xbf)
	     , SNR_SP, SNR_TPS, SNR_CP
/* ,EQ_seg_ratio */
	     , app_apb_read_reg(0x62)
	     , RS_CorrectNum);
	PR_DBG
	    ("SFO %4d FCFO %4d Vit %4x Timing %3d SigP %3x",
	    SFO_residual, FCFO_residual, RF_AGC, timing_adj,
	     Signal_power);
	PR_DBG
	    ("FEC %x RSErr %8x ReSyn %x tps %03x%08x",
	    FECFlag, app_apb_read_reg(0x0b)
	     , (app_apb_read_reg(0xc0) >> 20) & 0xff,
	     app_apb_read_reg(0x05) & 0xfff, app_apb_read_reg(0x04)
	    );
	PR_DBG("\n");
}
#endif
/* new api */
unsigned int demod_reg_get_abs_ddr(unsigned int reg_mode, unsigned int  reg_add)
{
	unsigned int base_add = 0;
	int ret = 0;

	switch (reg_mode) {
	case REG_M_DEMOD:
		/*base_add = ddemod_reg_base;*/
		base_add = gphybase_demodcfg();
		break;
	case REG_M_HIU:
		/*base_add  = dd_hiu_reg_base;*/
		base_add = gphybase_hiu();
		break;
#if 0
	case REG_M_TVAFE:
		base_add = dtvafe_reg_base;
		break;
#endif
	case REG_M_NONE:
	default:
		ret = -1;
		break;
	}

	if (ret < 0)
		return 0xffffffff;
	else
		return base_add + reg_add;
}


/*dvbc_write_reg -> apb_write_reg in dvbc_func*/
/*dvbc_read_reg -> apb_read_reg in dvbc_func*/
#if 0
void dvbc_write_reg(unsigned int addr, unsigned int data)
{
	demod_set_demod_reg(data, ddemod_reg_base + addr);
}
unsigned int dvbc_read_reg(unsigned int addr)
{
	return demod_read_demod_reg(ddemod_reg_base + addr);
}
#else
void dvbc_write_reg(unsigned int addr, unsigned int data)
{
	/*mutex_lock(&mp);*/
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_dvbc() + addr);

	/*mutex_unlock(&mp);*/
}
unsigned int dvbc_read_reg(unsigned int addr)
{
	unsigned int tmp;

	/*mutex_lock(&mp);*/

	tmp = readl(gbase_dvbc() + addr);

	/*mutex_unlock(&mp);*/

	return tmp;
}
#endif

void demod_top_write_reg(unsigned int addr, unsigned int data)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp))
		return;

	mutex_lock(&mp);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		writel(data, gbase_demod() + (addr << 2));
	else
		writel(data, gbase_demod() + addr);

	mutex_unlock(&mp);
	if (devp->print_on)
		PR_INFO("top wrW 0x%x=0x%x\n", addr, data);
}

void demod_top_write_bits(u32 reg_addr, const u32 reg_data, const u32 start, const u32 len)
{
	unsigned int val;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag() || unlikely(!devp))
		return;

	mutex_lock(&mp);
	val = readl(gbase_demod() + (reg_addr << 2));
	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((reg_data) & ((1L << (len)) - 1)) << (start));
	writel(val, gbase_demod() + (reg_addr << 2));

	if (devp->print_on)
		PR_INFO("top wrBit 0x%x=0x%x,s:%d,l:%d\n", reg_addr, reg_data, start, len);
	mutex_unlock(&mp);
}

unsigned int demod_top_read_reg(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		tmp = readl(gbase_demod() + (addr << 2));
	else
		tmp = readl(gbase_demod() + addr);

	mutex_unlock(&mp);

	return tmp;
}

/*TL1*/
void front_write_reg(unsigned int addr, unsigned int data)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag() || unlikely(!devp))
		return;

	mutex_lock(&mp);

	writel(data, gbase_front() + (addr << 2));

	if (devp->print_on)
		PR_INFO("front wrW 0x%x=0x%x\n", addr, data);

	mutex_unlock(&mp);
}

void front_write_bits(u32 reg_addr, const u32 reg_data, const u32 start, const u32 len)
{
	unsigned int val;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag() || unlikely(!devp))
		return;

	mutex_lock(&mp);
	val = readl(gbase_front() + (reg_addr << 2));
	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((reg_data) & ((1L << (len)) - 1)) << (start));
	writel(val, gbase_front() + (reg_addr << 2));

	if (devp->print_on)
		PR_INFO("front wrBit 0x%x=0x%x, s:%d,l:%d\n", reg_addr, reg_data, start, len);
	mutex_unlock(&mp);

}

unsigned int front_read_reg(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_front() + (addr << 2));

	mutex_unlock(&mp);

	return tmp;

}

void  isdbt_write_reg_v4(unsigned int addr, unsigned int data)
{
	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_isdbt() + addr);

	mutex_unlock(&mp);
}

unsigned int isdbt_read_reg_v4(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_isdbt() + addr);

	mutex_unlock(&mp);

	return tmp;

}

/*dvbc v3:*/
void qam_write_reg(unsigned int reg_addr, unsigned int reg_data)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag())
		return;

	if (devp && devp->stop_reg_wr)
		return;

	mutex_lock(&mp);
	if (devp->dvbc_sel)
		writel(reg_data, gbase_dvbc_2() + (reg_addr << 2));
	else
		writel(reg_data, gbase_dvbc() + (reg_addr << 2));

	mutex_unlock(&mp);

}

unsigned int qam_read_reg(unsigned int reg_addr)
{

	unsigned int tmp;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag())
		return 0;


	mutex_lock(&mp);
	if (devp->dvbc_sel)
		tmp = readl(gbase_dvbc_2() + (reg_addr << 2));
	else
		tmp = readl(gbase_dvbc() + (reg_addr << 2));

	mutex_unlock(&mp);

	return tmp;

}

void qam_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len)
{
	unsigned int val;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!get_dtvpll_init_flag())
		return;

	if (devp && devp->stop_reg_wr)
		return;

	mutex_lock(&mp);

	if (devp->dvbc_sel)
		val = readl(gbase_dvbc_2() + (reg_addr << 2));
	else
		val = readl(gbase_dvbc() + (reg_addr << 2));

	val &= ~(((1L << (len)) - 1) << (start));
	val |= (((reg_data) & ((1L << (len)) - 1)) << (start));

	if (devp->dvbc_sel)
		writel(val, gbase_dvbc_2() + (reg_addr << 2));
	else
		writel(val, gbase_dvbc() + (reg_addr << 2));

	mutex_unlock(&mp);

}

int dd_tvafe_hiu_reg_write(unsigned int reg, unsigned int val)
{
	mutex_lock(&mp);

	writel(val, gbase_iohiu() + reg);

	mutex_unlock(&mp);

	return 0;
}
EXPORT_SYMBOL(dd_tvafe_hiu_reg_write);

unsigned int dd_tvafe_hiu_reg_read(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_iohiu() + addr);

	mutex_unlock(&mp);

	return tmp;
}
EXPORT_SYMBOL(dd_tvafe_hiu_reg_read);

int reset_reg_write(unsigned int reg, unsigned int val)
{
	mutex_lock(&mp);

	writel(val, gbase_reset() + reg);

	mutex_unlock(&mp);

	return 0;
}
unsigned int reset_reg_read(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_reset() + addr);

	mutex_unlock(&mp);

	return tmp;
}

void dtvdemod_dmc_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, gbase_dmc() + reg);
}

unsigned int dtvdemod_dmc_reg_read(unsigned int addr)
{
	unsigned int tmp;

	tmp = readl(gbase_dmc() + addr);

	return tmp;
}

#if 0
int dd_tvafe_reg_write(unsigned int reg, unsigned int val)
{
	demod_set_demod_reg(val, dtvafe_reg_base + reg);

	return 0;
}
unsigned int dd_tvafe_reg_read(unsigned int addr)
{
	return demod_read_demod_reg(dtvafe_reg_base + addr);
}
#endif

void demod_set_demod_default(void)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		return;

	demod_top_write_reg(DEMOD_TOP_REG0, DEMOD_REG0_VALUE);
	demod_top_write_reg(DEMOD_TOP_REG4, DEMOD_REG4_VALUE);
	demod_top_write_reg(DEMOD_TOP_REG8, DEMOD_REG8_VALUE);
}


void debug_check_reg_val(unsigned int reg_mode, unsigned int reg)
{
	unsigned int regAddr;
	unsigned int val;
#if 0
	regAddr = demod_reg_get_abs_ddr(reg_mode, reg);

	if (regAddr == 0xffffffff) {
		PR_DBG("[reg=%x][mode=%d] is onthing!\n", reg, reg_mode);
		return;
	}
	val = demod_read_demod_reg(regAddr);
#endif
	switch (reg_mode) {
	case REG_M_DEMOD:
		val = demod_top_read_reg(reg);
		regAddr = gphybase_demodcfg() + reg;
		PR_DBG("[demod][%x]%x\n", regAddr, val);
		break;
#if 0
	case REG_M_TVAFE:
		regAddr = demod_reg_get_abs_ddr(reg_mode, reg);
		val = demod_read_demod_reg(regAddr);
		PR_DBG("[tafe][%x]%x\n", regAddr, val);
		break;
#endif
	case REG_M_HIU:
		val = dd_tvafe_hiu_reg_read(reg);
		regAddr = gphybase_hiu() + reg;
		PR_DBG("[adc][%x]%x\n", regAddr, val);
		break;
	default:
		PR_DBG("[reg=%x][mode=%d] is onthing!\n", reg, reg_mode);
		break;
	}
}

const unsigned int adc_check_tab_hiu[] = {
	REG_M_HIU, D_HHI_ADC_PLL_CNTL,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL2,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL3,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL4,
	REG_M_HIU, D_HHI_HDMI_CLK_CNTL,
	REG_M_HIU, D_HHI_DEMOD_CLK_CNTL,
	REG_M_HIU, D_HHI_DADC_CNTL,
	REG_M_HIU, D_HHI_DADC_CNTL2,
	REG_M_HIU, D_HHI_DADC_CNTL3,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL1,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL5,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL6,
	REG_M_HIU, D_HHI_VDAC_CNTL0,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,

};
const unsigned int adc_check_tab_gxtvbb[] = {
	REG_M_HIU, D_HHI_ADC_PLL_CNTL,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL2,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL3,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL4,
/*	REG_M_HIU, D_HHI_HDMI_CLK_CNTL, */

	REG_M_DEMOD, DEMOD_TOP_REG0,
	REG_M_DEMOD, DEMOD_TOP_REG4,
	REG_M_DEMOD, DEMOD_TOP_REG8,
	REG_M_DEMOD, DEMOD_TOP_REGC,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,

};

const unsigned int adc_check_tab_demod_txlx[] = {
	REG_M_DEMOD, DEMOD_TOP_REG0,
	REG_M_DEMOD, DEMOD_TOP_REG4,
	REG_M_DEMOD, DEMOD_TOP_REG8,
	REG_M_DEMOD, DEMOD_TOP_REGC,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,
};
const unsigned int adc_check_tab_demod_gxbb[] = {
	REG_M_DEMOD, DEMOD_TOP_REG0,
	REG_M_DEMOD, DEMOD_TOP_REG4,
	REG_M_DEMOD, DEMOD_TOP_REG8,
	REG_M_DEMOD, DEMOD_TOP_REGC,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,
};
void debug_check_reg_table(const unsigned int *pTab)
{

	unsigned int cnt = 0;
	unsigned int add;
	unsigned int reg_mode;

	unsigned int pretect = 0;

	reg_mode = pTab[cnt++];
	add = pTab[cnt++];

	while ((reg_mode != TABLE_FLG_END) && (pretect < 100)) {
		debug_check_reg_val(reg_mode, add);
		reg_mode = pTab[cnt++];
		add = pTab[cnt++];
		pretect++;
	}
}
void debug_adc_pll(void)
{
	if (is_meson_txl_cpu()) {
		debug_check_reg_table(&adc_check_tab_hiu[0]);
		debug_check_reg_table(&adc_check_tab_demod_gxbb[0]);

	} else if (is_meson_txlx_cpu() || is_meson_gxlx_cpu() || is_meson_txhd_cpu()) {
		debug_check_reg_table(&adc_check_tab_hiu[0]);
		debug_check_reg_table(&adc_check_tab_demod_txlx[0]);

	} else {
		/* gxtvbb */
		debug_check_reg_table(&adc_check_tab_gxtvbb[0]);
	}

}

