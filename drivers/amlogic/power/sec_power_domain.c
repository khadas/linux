// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/amlogic/power_domain.h>
#include <dt-bindings/power/a1-pd.h>
#include <dt-bindings/power/c1-pd.h>
#include <dt-bindings/power/sc2-pd.h>
#include <dt-bindings/power/t5-pd.h>
#include <dt-bindings/power/t7-pd.h>
#include <dt-bindings/power/s4-pd.h>
#include <dt-bindings/power/t3-pd.h>
#include <dt-bindings/power/p1-pd.h>
#include <dt-bindings/power/a5-pd.h>
#include <linux/kallsyms.h>

struct sec_pm_private_domain {
	const char *name;
	unsigned int flags;
	int pd_index;
	bool pd_status;
	int pd_parent;
};

struct sec_pm_domain {
	struct generic_pm_domain base;
	struct sec_pm_private_domain *private_domain;
};

struct sec_pm_domain_data {
	struct sec_pm_private_domain *domains;
	unsigned int domains_count;
};

static inline struct sec_pm_domain *
to_sec_pm_domain(struct generic_pm_domain *genpd)
{
	return container_of(genpd, struct sec_pm_domain, base);
}

static int sec_pm_domain_power_off(struct generic_pm_domain *genpd)
{
	struct sec_pm_domain *pd = to_sec_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->private_domain->pd_index, PWR_OFF);

	return 0;
}

static int sec_pm_domain_power_on(struct generic_pm_domain *genpd)
{
	struct sec_pm_domain *pd = to_sec_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->private_domain->pd_index, PWR_ON);

	return 0;
}

#define TOP_DOMAIN(_name, index, status, flag, parent)		\
{					\
		.name = #_name,					\
		.flags = flag,					\
		.pd_index = index,				\
		.pd_status = status,				\
		.pd_parent = parent,				\
}

#define POWER_DOMAIN(_name, index, status, flag)	\
	TOP_DOMAIN(_name, index, status, flag, 0)

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct sec_pm_private_domain a1_pm_domains[] __initdata = {
	[PDID_A1_CPU_PWR0] = POWER_DOMAIN(cpu_pwr0, PDID_A1_CPU_PWR0, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_CPU_CORE0] = POWER_DOMAIN(cpu_core0, PDID_A1_CPU_CORE0, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_CPU_CORE1]		= POWER_DOMAIN(cpu_core1, PDID_A1_CPU_CORE1, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_DSP_A] = POWER_DOMAIN(dsp_a, PDID_A1_DSP_A, DOMAIN_INIT_OFF, 0),
	[PDID_A1_DSP_B] = POWER_DOMAIN(dsp_b, PDID_A1_DSP_B, DOMAIN_INIT_OFF, 0),
	[PDID_A1_UART] = POWER_DOMAIN(uart, PDID_A1_UART, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_MMC] = POWER_DOMAIN(mmc, PDID_A1_MMC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_I2C] = POWER_DOMAIN(i2c, PDID_A1_I2C, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_PSRAM] = POWER_DOMAIN(psram, PDID_A1_PSRAM, DOMAIN_INIT_OFF, 0),
	[PDID_A1_ACODEC] = POWER_DOMAIN(acodec, PDID_A1_ACODEC, DOMAIN_INIT_ON, 0),
	[PDID_A1_AUDIO] = POWER_DOMAIN(audio, PDID_A1_AUDIO, DOMAIN_INIT_ON, 0),
	[PDID_A1_MKL_OTP] = POWER_DOMAIN(mkl_otp, PDID_A1_MKL_OTP, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_DMA] = POWER_DOMAIN(dma, PDID_A1_DMA, DOMAIN_INIT_ON, GENPD_FLAG_IRQ_SAFE),
	[PDID_A1_SDEMMC] = POWER_DOMAIN(sdemmc, PDID_A1_SDEMMC, DOMAIN_INIT_OFF, 0),
	[PDID_A1_SRAM_A] = POWER_DOMAIN(sram_a, PDID_A1_SRAM_A, DOMAIN_INIT_OFF, 0),
	[PDID_A1_SRAM_B] = POWER_DOMAIN(sram_b, PDID_A1_SRAM_B, DOMAIN_INIT_OFF, 0),
	[PDID_A1_IR] = POWER_DOMAIN(ir, PDID_A1_IR, DOMAIN_INIT_ON, 0),
	[PDID_A1_SPICC] = POWER_DOMAIN(spicc, PDID_A1_SPICC, DOMAIN_INIT_OFF, 0),
	[PDID_A1_SPIFC] = POWER_DOMAIN(spifc, PDID_A1_SPIFC, DOMAIN_INIT_ON, 0),
	[PDID_A1_USB] = POWER_DOMAIN(usb, PDID_A1_USB, DOMAIN_INIT_ON, 0),
	[PDID_A1_NIC] = POWER_DOMAIN(nic, PDID_A1_NIC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A1_PDM] = POWER_DOMAIN(pdm, PDID_A1_PDM, DOMAIN_INIT_ON, 0),
	[PDID_A1_RSA] = POWER_DOMAIN(rsa, PDID_A1_RSA, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data a1_pm_domain_data __initdata = {
	.domains = a1_pm_domains,
	.domains_count = ARRAY_SIZE(a1_pm_domains),
};

static struct sec_pm_private_domain c1_pm_domains[] __initdata = {
	[PDID_C1_CPU_PWR0] = POWER_DOMAIN(cpu_pwr0, PDID_C1_CPU_PWR0, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_CPU_CORE0] = POWER_DOMAIN(cpu_core0, PDID_C1_CPU_CORE0, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_CPU_CORE1] = POWER_DOMAIN(cpu_core1, PDID_C1_CPU_CORE1, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_DSP_A] = POWER_DOMAIN(dsp_a, PDID_C1_DSP_A, DOMAIN_INIT_OFF, 0),
	[PDID_C1_DSP_B] = POWER_DOMAIN(dsp_b, PDID_C1_DSP_B, DOMAIN_INIT_OFF, 0),
	[PDID_C1_UART] = POWER_DOMAIN(uart, PDID_C1_UART, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_DMC] = POWER_DOMAIN(dmc, PDID_C1_DMC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
		/* If there is GENPD_FLAG_ALWAYS_ON, the domian must be initialized to on */
	[PDID_C1_I2C] = POWER_DOMAIN(i2c, PDID_C1_I2C, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_SDEMMC_B] = POWER_DOMAIN(sdemmc_b, PDID_C1_SDEMMC_B, DOMAIN_INIT_ON, 0),
	[PDID_C1_ACODEC] = POWER_DOMAIN(acodec, PDID_C1_ACODEC, DOMAIN_INIT_ON, 0),
	[PDID_C1_AUDIO] = POWER_DOMAIN(audio, PDID_C1_AUDIO, DOMAIN_INIT_ON, 0),
	[PDID_C1_MKL_OTP] = POWER_DOMAIN(mkl_otp, PDID_C1_MKL_OTP, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_DMA] = POWER_DOMAIN(dma, PDID_C1_DMA, DOMAIN_INIT_ON, GENPD_FLAG_IRQ_SAFE),
	[PDID_C1_SDEMMC_A] = POWER_DOMAIN(sdemmc_a, PDID_C1_SDEMMC_A, DOMAIN_INIT_ON, 0),
	[PDID_C1_SRAM_A] = POWER_DOMAIN(sram_a, PDID_C1_SRAM_A, DOMAIN_INIT_OFF, 0),
	[PDID_C1_SRAM_B] = POWER_DOMAIN(sram_b, PDID_C1_SRAM_B, DOMAIN_INIT_OFF, 0),
	[PDID_C1_IR] = POWER_DOMAIN(ir, PDID_C1_IR, DOMAIN_INIT_ON, 0),
	[PDID_C1_SPICC] = POWER_DOMAIN(spicc, PDID_C1_SPICC, DOMAIN_INIT_OFF, 0),
	[PDID_C1_SPIFC] = POWER_DOMAIN(spifc, PDID_C1_SPIFC, DOMAIN_INIT_ON, 0),
	[PDID_C1_USB] = POWER_DOMAIN(usb, PDID_C1_USB, DOMAIN_INIT_ON, 0),
	[PDID_C1_NIC] = POWER_DOMAIN(nic, PDID_C1_NIC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_PDM] = POWER_DOMAIN(pdm, PDID_C1_PDM, DOMAIN_INIT_ON, 0),
	[PDID_C1_RSA] = POWER_DOMAIN(rsa, PDID_C1_RSA, DOMAIN_INIT_OFF, 0),
	[PDID_C1_MIPI_ISP] = POWER_DOMAIN(mipi_isp, PDID_C1_MIPI_ISP, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_HCODEC] = POWER_DOMAIN(hcodec, PDID_C1_HCODEC, DOMAIN_INIT_ON, 0),
	[PDID_C1_WAVE] = POWER_DOMAIN(wave, PDID_C1_WAVE, DOMAIN_INIT_OFF, 0),
	[PDID_C1_SDEMMC_C] = POWER_DOMAIN(sdemmc_c, PDID_C1_SDEMMC_C, DOMAIN_INIT_ON, 0),
	[PDID_C1_SRAM_C] = POWER_DOMAIN(sram_c, PDID_C1_SRAM_C, DOMAIN_INIT_OFF, 0),
	[PDID_C1_GDC] = POWER_DOMAIN(gdc, PDID_C1_GDC, DOMAIN_INIT_OFF, 0),
	[PDID_C1_GE2D] = POWER_DOMAIN(ge2d, PDID_C1_GE2D, DOMAIN_INIT_OFF, 0),
	[PDID_C1_NNA] = POWER_DOMAIN(nna, PDID_C1_NNA, DOMAIN_INIT_OFF, 0),
	[PDID_C1_ETH] = POWER_DOMAIN(eth, PDID_C1_ETH, DOMAIN_INIT_ON, 0),
	[PDID_C1_GIC] = POWER_DOMAIN(gic, PDID_C1_GIC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_DDR] = POWER_DOMAIN(ddr, PDID_C1_DDR, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_C1_SPICC_B] = POWER_DOMAIN(spicc_b, PDID_C1_SPICC_B, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data c1_pm_domain_data __initdata = {
	.domains = c1_pm_domains,
	.domains_count = ARRAY_SIZE(c1_pm_domains),
};
#endif

static struct sec_pm_private_domain sc2_pm_domains[] __initdata = {
	[PDID_SC2_DSP] = POWER_DOMAIN(dsp, PDID_SC2_DSP, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_DOS_HCODEC] = POWER_DOMAIN(hcodec, PDID_SC2_DOS_HCODEC, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_SC2_DOS_HEVC, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_SC2_DOS_VDEC, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_DOS_WAVE] = POWER_DOMAIN(wave, PDID_SC2_DOS_WAVE, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_SC2_VPU_HDMI, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_SC2_USB_COMB] = POWER_DOMAIN(usb, PDID_SC2_USB_COMB, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_SC2_PCIE] = POWER_DOMAIN(pcie, PDID_SC2_PCIE, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_GE2D] = POWER_DOMAIN(ge2d, PDID_SC2_GE2D, DOMAIN_INIT_OFF, 0),
	[PDID_SC2_ETH] = POWER_DOMAIN(eth, PDID_SC2_ETH, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_SC2_AUDIO] = POWER_DOMAIN(audio, PDID_SC2_AUDIO, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data sc2_pm_domain_data __initdata = {
	.domains = sc2_pm_domains,
	.domains_count = ARRAY_SIZE(sc2_pm_domains),
};

static struct sec_pm_private_domain t5_pm_domains[] __initdata = {
	[PDID_T5_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_T5_DOS_HEVC, DOMAIN_INIT_OFF, 0),
	[PDID_T5_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_T5_DOS_VDEC, DOMAIN_INIT_OFF, 0),
	[PDID_T5_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_T5_VPU_HDMI, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_T5_DEMOD] = POWER_DOMAIN(demod, PDID_T5_DEMOD, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data t5_pm_domain_data __initdata = {
	.domains = t5_pm_domains,
	.domains_count = ARRAY_SIZE(t5_pm_domains),
};

static struct sec_pm_private_domain t7_pm_domains[] __initdata = {
	[PDID_T7_DSPA] = POWER_DOMAIN(dspa, PDID_T7_DSPA, DOMAIN_INIT_OFF, 0),
	[PDID_T7_DSPB] = POWER_DOMAIN(dspb, PDID_T7_DSPB, DOMAIN_INIT_OFF, 0),
	[PDID_T7_DOS_HCODEC] = TOP_DOMAIN(hcodec, PDID_T7_DOS_HCODEC, DOMAIN_INIT_OFF, 0,
					  PDID_T7_NIC3),
	[PDID_T7_DOS_HEVC] = TOP_DOMAIN(hevc, PDID_T7_DOS_HEVC, DOMAIN_INIT_OFF, 0, PDID_T7_NIC3),
	[PDID_T7_DOS_VDEC] = TOP_DOMAIN(vdec, PDID_T7_DOS_VDEC, DOMAIN_INIT_OFF, 0, PDID_T7_NIC3),
	[PDID_T7_DOS_WAVE] = TOP_DOMAIN(wave, PDID_T7_DOS_WAVE, DOMAIN_INIT_OFF, 0, PDID_T7_NIC3),
	[PDID_T7_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_T7_VPU_HDMI, DOMAIN_INIT_ON,
					  GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_USB_COMB] = POWER_DOMAIN(usb, PDID_T7_USB_COMB, DOMAIN_INIT_ON,
					  GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_PCIE] = POWER_DOMAIN(pcie, PDID_T7_PCIE, DOMAIN_INIT_OFF, 0),
	[PDID_T7_GE2D] = TOP_DOMAIN(ge2d, PDID_T7_GE2D, DOMAIN_INIT_OFF, 0, PDID_T7_NIC3),
	[PDID_T7_SRAMA] = POWER_DOMAIN(srama, PDID_T7_SRAMA, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_SRAMB] = POWER_DOMAIN(sramb, PDID_T7_SRAMB, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_HDMIRX] = POWER_DOMAIN(hdmirx, PDID_T7_HDMIRX, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_VI_CLK1] = POWER_DOMAIN(vi_clk1, PDID_T7_VI_CLK1,
					 DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_VI_CLK2] = POWER_DOMAIN(vi_clk2, PDID_T7_VI_CLK2,
					 DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_ETH] = POWER_DOMAIN(eth, PDID_T7_ETH, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_ISP] = POWER_DOMAIN(isp, PDID_T7_ISP, DOMAIN_INIT_OFF, 0),
	[PDID_T7_MIPI_ISP] = POWER_DOMAIN(mipi_isp, PDID_T7_MIPI_ISP, DOMAIN_INIT_OFF, 0),
	[PDID_T7_GDC] = TOP_DOMAIN(gdc, PDID_T7_GDC, DOMAIN_INIT_OFF, 0, PDID_T7_NIC3),
	[PDID_T7_DEWARP] = TOP_DOMAIN(dewarp, PDID_T7_DEWARP, DOMAIN_INIT_OFF, 0, PDID_T7_NIC3),
	[PDID_T7_SDIO_A] = POWER_DOMAIN(sdio_a, PDID_T7_SDIO_A,
					DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_SDIO_B] = POWER_DOMAIN(sdio_b, PDID_T7_SDIO_B,
					DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_EMMC] = POWER_DOMAIN(emmc, PDID_T7_EMMC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_MALI_SC0] = TOP_DOMAIN(mali_sc0, PDID_T7_MALI_SC0, DOMAIN_INIT_OFF, 0,
					PDID_T7_MALI_TOP),
	[PDID_T7_MALI_SC1] = TOP_DOMAIN(mali_sc1, PDID_T7_MALI_SC1, DOMAIN_INIT_OFF, 0,
					PDID_T7_MALI_TOP),
	[PDID_T7_MALI_SC2] = TOP_DOMAIN(mali_sc2, PDID_T7_MALI_SC2, DOMAIN_INIT_OFF, 0,
					PDID_T7_MALI_TOP),
	[PDID_T7_MALI_SC3] = TOP_DOMAIN(mali_sc3, PDID_T7_MALI_SC3, DOMAIN_INIT_OFF, 0,
					PDID_T7_MALI_TOP),
	[PDID_T7_MALI_TOP] = POWER_DOMAIN(mali_top, PDID_T7_MALI_TOP, DOMAIN_INIT_OFF, 0),
	[PDID_T7_NNA_CORE0] = TOP_DOMAIN(nna_core0, PDID_T7_NNA_CORE0, DOMAIN_INIT_OFF, 0,
					 PDID_T7_NNA_TOP),
	[PDID_T7_NNA_CORE1] = TOP_DOMAIN(nna_core1, PDID_T7_NNA_CORE1, DOMAIN_INIT_OFF, 0,
					 PDID_T7_NNA_TOP),
	[PDID_T7_NNA_CORE2] = TOP_DOMAIN(nna_core2, PDID_T7_NNA_CORE2, DOMAIN_INIT_OFF, 0,
					 PDID_T7_NNA_TOP),
	[PDID_T7_NNA_CORE3] = TOP_DOMAIN(nna_core3, PDID_T7_NNA_CORE3, DOMAIN_INIT_OFF, 0,
					 PDID_T7_NNA_TOP),
	[PDID_T7_NNA_TOP] = POWER_DOMAIN(nna_top, PDID_T7_NNA_TOP, DOMAIN_INIT_OFF, 0),
	[PDID_T7_DDR0] = POWER_DOMAIN(ddr0, PDID_T7_DDR0, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_DDR1] = POWER_DOMAIN(ddr1, PDID_T7_DDR1, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_DMC0] = POWER_DOMAIN(dmc0, PDID_T7_DMC0, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_DMC1] = POWER_DOMAIN(dmc1, PDID_T7_DMC1, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_NOC] = POWER_DOMAIN(noc, PDID_T7_NOC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_NIC2] = POWER_DOMAIN(nic2, PDID_T7_NIC2, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_NIC3] = POWER_DOMAIN(nic3, PDID_T7_NIC3, DOMAIN_INIT_OFF, 0),
	[PDID_T7_CCI] = POWER_DOMAIN(cci, PDID_T7_CCI, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_MIPI_DSI0] = POWER_DOMAIN(mipi_dsi0, PDID_T7_MIPI_DSI0,
					   DOMAIN_INIT_ON,
					   GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_SPICC0] = POWER_DOMAIN(spicc0, PDID_T7_SPICC0, DOMAIN_INIT_OFF, 0),
	[PDID_T7_SPICC1] = POWER_DOMAIN(spicc1, PDID_T7_SPICC1, DOMAIN_INIT_OFF, 0),
	[PDID_T7_SPICC2] = POWER_DOMAIN(spicc2, PDID_T7_SPICC2, DOMAIN_INIT_OFF, 0),
	[PDID_T7_SPICC3] = POWER_DOMAIN(spicc3, PDID_T7_SPICC3, DOMAIN_INIT_OFF, 0),
	[PDID_T7_SPICC4] = POWER_DOMAIN(spicc4, PDID_T7_SPICC4, DOMAIN_INIT_OFF, 0),
	[PDID_T7_SPICC5] = POWER_DOMAIN(spicc5, PDID_T7_SPICC5, DOMAIN_INIT_OFF, 0),
	[PDID_T7_EDP0] = POWER_DOMAIN(edp0, PDID_T7_EDP0, DOMAIN_INIT_ON,
				      GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_EDP1] = POWER_DOMAIN(edp1, PDID_T7_EDP1, DOMAIN_INIT_ON,
				      GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_MIPI_DSI1] = POWER_DOMAIN(mipi_dsi1, PDID_T7_MIPI_DSI1,
					   DOMAIN_INIT_ON,
					   GENPD_FLAG_ALWAYS_ON),
	[PDID_T7_AUDIO] = POWER_DOMAIN(audio, PDID_T7_AUDIO, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data t7_pm_domain_data __initdata = {
	.domains = t7_pm_domains,
	.domains_count = ARRAY_SIZE(t7_pm_domains),
};

static struct sec_pm_private_domain s4_pm_domains[] __initdata = {
	[PDID_S4_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_S4_DOS_HEVC, DOMAIN_INIT_OFF, 0),
	[PDID_S4_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_S4_DOS_VDEC, DOMAIN_INIT_OFF, 0),
	[PDID_S4_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_S4_VPU_HDMI, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_S4_USB_COMB] = POWER_DOMAIN(usb, PDID_S4_USB_COMB, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_S4_GE2D] = POWER_DOMAIN(ge2d, PDID_S4_GE2D, DOMAIN_INIT_OFF, 0),
	[PDID_S4_ETH] = POWER_DOMAIN(eth, PDID_S4_ETH, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_S4_DEMOD] = POWER_DOMAIN(demod, PDID_S4_DEMOD, DOMAIN_INIT_OFF, 0),
	[PDID_S4_AUDIO] = POWER_DOMAIN(audio, PDID_S4_AUDIO, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data s4_pm_domain_data __initdata = {
	.domains = s4_pm_domains,
	.domains_count = ARRAY_SIZE(s4_pm_domains),
};

static struct sec_pm_private_domain t3_pm_domains[] = {
	[PDID_T3_DSPA] = POWER_DOMAIN(dspa, PDID_T3_DSPA, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_DOS_HCODEC] = POWER_DOMAIN(hcodec, PDID_T3_DOS_HCODEC, DOMAIN_INIT_OFF, 0),
	[PDID_T3_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_T3_DOS_HEVC, DOMAIN_INIT_OFF, 0),
	[PDID_T3_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_T3_DOS_VDEC, DOMAIN_INIT_OFF, 0),
	[PDID_T3_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_T3_VPU_HDMI, DOMAIN_INIT_ON,
					  GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_USB_COMB] = POWER_DOMAIN(usb, PDID_T3_USB_COMB, DOMAIN_INIT_ON,
					  GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_PCIE] = POWER_DOMAIN(pcie, PDID_T3_PCIE, DOMAIN_INIT_OFF, 0),
	[PDID_T3_GE2D] = POWER_DOMAIN(ge2d, PDID_T3_GE2D, DOMAIN_INIT_OFF, 0),
	[PDID_T3_SRAMA] = POWER_DOMAIN(srama, PDID_T3_SRAMA, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_HDMIRX] = POWER_DOMAIN(hdmirx, PDID_T3_HDMIRX, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_VI_CLK1] = POWER_DOMAIN(vi_clk1, PDID_T3_VI_CLK1,
					 DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_VI_CLK2] = POWER_DOMAIN(vi_clk2, PDID_T3_VI_CLK2,
					 DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_ETH] = POWER_DOMAIN(eth, PDID_T3_ETH, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_NNA] = POWER_DOMAIN(nna, PDID_T3_NNA, DOMAIN_INIT_OFF, 0),
	[PDID_T3_DEMOD] = POWER_DOMAIN(demod, PDID_T3_DEMOD, DOMAIN_INIT_OFF, 0),
	[PDID_T3_FRCTOP] = POWER_DOMAIN(frctop, PDID_T3_FRCTOP, DOMAIN_INIT_OFF, 0),
	[PDID_T3_FRCME] = POWER_DOMAIN(frcme, PDID_T3_FRCME, DOMAIN_INIT_OFF, 0),
	[PDID_T3_FRCMC] = POWER_DOMAIN(frcmc, PDID_T3_FRCMC, DOMAIN_INIT_OFF, 0),
	[PDID_T3_SDEMMC_B] = POWER_DOMAIN(sdemmc_b, PDID_T3_SDEMMC_B,
					  DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_SDEMMC_C] = POWER_DOMAIN(sdemmc_c, PDID_T3_SDEMMC_C,
					  DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_NOC_DEV] = POWER_DOMAIN(noc_dev, PDID_T3_NOC_DEV, DOMAIN_INIT_ON,
					 GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_NOC_VPU] = POWER_DOMAIN(noc_vpu, PDID_T3_NOC_VPU, DOMAIN_INIT_ON,
					 GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_SPICC0] = POWER_DOMAIN(spicc0, PDID_T3_SPICC0, DOMAIN_INIT_OFF, 0),
	[PDID_T3_SPICC1] = POWER_DOMAIN(spicc1, PDID_T3_SPICC1, DOMAIN_INIT_ON,
					GENPD_FLAG_ALWAYS_ON),
	[PDID_T3_SPICC2] = POWER_DOMAIN(spicc2, PDID_T3_SPICC2, DOMAIN_INIT_OFF, 0),
	[PDID_T3_AUDIO] = POWER_DOMAIN(audio, PDID_T3_AUDIO, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
};

static struct sec_pm_domain_data t3_pm_domain_data = {
	.domains = t3_pm_domains,
	.domains_count = ARRAY_SIZE(t3_pm_domains),
};

static struct sec_pm_private_domain p1_pm_domains[] = {
	[PDID_P1_DSPA] = POWER_DOMAIN(dspa, PDID_P1_DSPA, DOMAIN_INIT_OFF, 0),
	[PDID_P1_DSPB] = POWER_DOMAIN(dspb, PDID_P1_DSPB, DOMAIN_INIT_OFF, 0),
	[PDID_P1_M4A] = POWER_DOMAIN(m4a, PDID_P1_M4A, DOMAIN_INIT_OFF, 0),
	[PDID_P1_M4B] = POWER_DOMAIN(m4b, PDID_P1_M4B, DOMAIN_INIT_OFF, 0),
	[PDID_P1_ISP_A] = POWER_DOMAIN(ispa, PDID_P1_ISP_A, DOMAIN_INIT_OFF, 0),
	[PDID_P1_ISP_B] = POWER_DOMAIN(ispb, PDID_P1_ISP_B, DOMAIN_INIT_OFF, 0),
	[PDID_P1_ISP_C] = POWER_DOMAIN(ispc, PDID_P1_ISP_C, DOMAIN_INIT_OFF, 0),
	[PDID_P1_ISP_D] = POWER_DOMAIN(ispd, PDID_P1_ISP_D, DOMAIN_INIT_OFF, 0),
	[PDID_P1_MIPI_ISP_TOP] = POWER_DOMAIN(mipiisptop, PDID_P1_MIPI_ISP_TOP,
			DOMAIN_INIT_OFF, 0),

	[PDID_P1_USB_COMB] = POWER_DOMAIN(usbcomb, PDID_P1_USB_COMB,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_P1_PCIE] = POWER_DOMAIN(pcie, PDID_P1_PCIE,
			DOMAIN_INIT_ON, 0),
	[PDID_P1_ETH] = POWER_DOMAIN(eth, PDID_P1_ETH,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_P1_SDIO] = POWER_DOMAIN(sdio, PDID_P1_SDIO,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),

	[PDID_P1_NAND_EMMC] = POWER_DOMAIN(nandemmc, PDID_P1_NAND_EMMC,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_P1_NNA_A] = TOP_DOMAIN(nnaa, PDID_P1_NNA_A, DOMAIN_INIT_OFF,
			0, PDID_P1_NNA_TOP),
	[PDID_P1_NNA_B] = TOP_DOMAIN(nnab, PDID_P1_NNA_B, DOMAIN_INIT_OFF,
			0, PDID_P1_NNA_TOP),
	[PDID_P1_NNA_C] = TOP_DOMAIN(nnac, PDID_P1_NNA_C, DOMAIN_INIT_OFF,
			0, PDID_P1_NNA_TOP),
	[PDID_P1_NNA_D] = TOP_DOMAIN(nnad, PDID_P1_NNA_D, DOMAIN_INIT_OFF,
			0, PDID_P1_NNA_TOP),
	[PDID_P1_NNA_E] = TOP_DOMAIN(nnae, PDID_P1_NNA_E, DOMAIN_INIT_OFF,
			0, PDID_P1_NNA_TOP),
	[PDID_P1_NNA_F] = TOP_DOMAIN(nnaf, PDID_P1_NNA_F, DOMAIN_INIT_OFF,
			0, PDID_P1_NNA_TOP),
	[PDID_P1_NNA_TOP] = POWER_DOMAIN(nnatop, PDID_P1_NNA_TOP,
			DOMAIN_INIT_OFF, 0),

	[PDID_P1_GE2D] = TOP_DOMAIN(ge2d, PDID_P1_GE2D, DOMAIN_INIT_OFF,
			0, PDID_P1_FDLE),
	[PDID_P1_DEWA] = TOP_DOMAIN(dewa, PDID_P1_DEWA, DOMAIN_INIT_OFF,
			0, PDID_P1_FDLE),
	[PDID_P1_DEWB] = TOP_DOMAIN(dewb, PDID_P1_DEWB, DOMAIN_INIT_OFF,
			0, PDID_P1_FDLE),
	[PDID_P1_DEWC] = TOP_DOMAIN(dewc, PDID_P1_DEWC, DOMAIN_INIT_OFF,
			0, PDID_P1_FDLE),
	[PDID_P1_FDLE] = POWER_DOMAIN(fdle, PDID_P1_FDLE,
			DOMAIN_INIT_OFF, 0),

	[PDID_P1_DMC0] = TOP_DOMAIN(dmc0, PDID_P1_DMC0,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON, PDID_P1_NOC_DMC_TOP),
	[PDID_P1_DMC1] = TOP_DOMAIN(dmc1, PDID_P1_DMC1,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON, PDID_P1_NOC_DMC_TOP),
	[PDID_P1_NOC_DMC_TOP] = POWER_DOMAIN(nocdmctop, PDID_P1_NOC_DMC_TOP,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_P1_SMMU] = POWER_DOMAIN(smmu, PDID_P1_SMMU,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_P1_DDR0] = POWER_DOMAIN(ddr0, PDID_P1_DDR0,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_P1_DDR1] = POWER_DOMAIN(ddr1, PDID_P1_DDR1,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
};

static struct sec_pm_domain_data p1_pm_domain_data = {
	.domains = p1_pm_domains,
	.domains_count = ARRAY_SIZE(p1_pm_domains),
};

static struct sec_pm_private_domain t5w_pm_domains[] __initdata = {
	[PDID_T5_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_T5_DOS_HEVC,
		DOMAIN_INIT_OFF, 0),
	[PDID_T5_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_T5_DOS_VDEC,
		DOMAIN_INIT_OFF, 0),
	[PDID_T5_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_T5_VPU_HDMI, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_T5_DEMOD] = POWER_DOMAIN(demod, PDID_T5_DEMOD, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data t5w_pm_domain_data __initdata = {
	.domains = t5w_pm_domains,
	.domains_count = ARRAY_SIZE(t5w_pm_domains),
};
static struct sec_pm_private_domain a5_pm_domains[] = {
	[PDID_A5_NNA] = POWER_DOMAIN(nna, PDID_A5_NNA,
			DOMAIN_INIT_OFF, 0),
	[PDID_A5_AUDIO] = POWER_DOMAIN(audio, PDID_A5_AUDIO,
			DOMAIN_INIT_ON, 0),
	[PDID_A5_SDIOA] = POWER_DOMAIN(sdioa, PDID_A5_SDIOA,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A5_EMMC] = POWER_DOMAIN(emmc, PDID_A5_EMMC,
			DOMAIN_INIT_ON, 0),
	[PDID_A5_USB_COMB] = POWER_DOMAIN(usb_comb, PDID_A5_USB_COMB,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A5_ETH] = POWER_DOMAIN(eth,
			PDID_A5_ETH, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A5_RSA] = POWER_DOMAIN(rsa, PDID_A5_RSA,
			DOMAIN_INIT_ON, 0),
	[PDID_A5_AUDIO_PDM] = POWER_DOMAIN(audio_pdm, PDID_A5_AUDIO_PDM,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A5_DMC] = POWER_DOMAIN(dmc, PDID_A5_DMC,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A5_SYS_WRAP] = POWER_DOMAIN(sys_wrap, PDID_A5_SYS_WRAP,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
	[PDID_A5_DSPA] = POWER_DOMAIN(dspa, PDID_A5_DSPA,
			DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON),
};

static struct sec_pm_domain_data a5_pm_domain_data = {
	.domains = a5_pm_domains,
	.domains_count = ARRAY_SIZE(a5_pm_domains),
};

static int sec_pd_probe(struct platform_device *pdev)
{
	int ret, i;
	struct sec_pm_private_domain *private_pd, *pri_pd;
	struct sec_pm_domain *pd;
	int init_status;
	const struct sec_pm_domain_data *match;
	struct genpd_onecell_data *sec_pd_onecell_data;

	match = of_device_get_match_data(&pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "failed to get match data\n");
		return -ENODEV;
	}

	sec_pd_onecell_data = devm_kzalloc(&pdev->dev, sizeof(*sec_pd_onecell_data), GFP_KERNEL);
	if (!sec_pd_onecell_data)
		return -ENOMEM;

	sec_pd_onecell_data->domains = devm_kcalloc(&pdev->dev, match->domains_count,
						    sizeof(*sec_pd_onecell_data->domains),
						    GFP_KERNEL);
	if (!sec_pd_onecell_data->domains)
		return -ENOMEM;

	sec_pd_onecell_data->num_domains = match->domains_count;

	pd = devm_kcalloc(&pdev->dev, match->domains_count, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;
	pri_pd = devm_kcalloc(&pdev->dev, match->domains_count, sizeof(*private_pd), GFP_KERNEL);
	if (!pri_pd)
		return -ENOMEM;

#ifdef MODULE
	struct dev_power_governor *aon_gov;

	aon_gov = (void *)kallsyms_lookup_name("pm_domain_always_on_gov");
	if (!aon_gov) {
		pr_err("can't find symbol: pm_domain_always_on_gov\n");
		return -EINVAL;
	}
#endif
	for (i = 0; i < match->domains_count; i++) {
		private_pd = &match->domains[i];
		/* array might be sparse */
		if (!private_pd->name)
			continue;

		pd[i].base.name = private_pd->name;
		pd[i].base.power_on = sec_pm_domain_power_on;
		pd[i].base.power_off = sec_pm_domain_power_off;
		pd[i].base.flags = private_pd->flags;
		pd[i].private_domain = &pri_pd[i];
		pri_pd[i].pd_index = private_pd->pd_index;
		pri_pd[i].pd_status = private_pd->pd_status;
		pri_pd[i].pd_parent = private_pd->pd_parent;

		init_status = pwr_ctrl_status_psci_smc(private_pd->pd_index);

		if (init_status == DOMAIN_INIT_OFF && private_pd->pd_status == DOMAIN_INIT_ON) {
			if (pd[i].base.flags == GENPD_FLAG_ALWAYS_ON)
				pwr_ctrl_psci_smc(i, PWR_ON);
		}

		if (init_status == -1 || pd[i].base.flags == GENPD_FLAG_ALWAYS_ON)
			init_status = private_pd->pd_status;

		/* Initialize based on pd_status */
		if (pd[i].base.flags & GENPD_FLAG_ALWAYS_ON)
#ifdef MODULE
			pm_genpd_init(&pd[i].base, aon_gov, init_status);
#else
			pm_genpd_init(&pd[i].base, &pm_domain_always_on_gov, init_status);
#endif
		else
			pm_genpd_init(&pd[i].base, NULL, init_status);

		sec_pd_onecell_data->domains[i] = &pd[i].base;
	}

	for (i = 0; i < match->domains_count; i++) {
		private_pd = &match->domains[i];

		if (!private_pd->pd_parent)
			continue;

		pm_genpd_add_subdomain(&pd[private_pd->pd_parent].base, &pd[i].base);
	}

	pd_dev_create_file(&pdev->dev, 0, sec_pd_onecell_data->num_domains,
			   sec_pd_onecell_data->domains);

	ret = of_genpd_add_provider_onecell(pdev->dev.of_node,
					    sec_pd_onecell_data);

	if (ret)
		goto out;

	return 0;

out:
	pd_dev_remove_file(&pdev->dev);
	return ret;
}

static const struct of_device_id pd_match_table[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,a1-power-domain",
		.data = &a1_pm_domain_data,
	},
	{
		.compatible = "amlogic,c1-power-domain",
		.data = &c1_pm_domain_data,
	},
#endif
	{
		.compatible = "amlogic,sc2-power-domain",
		.data = &sc2_pm_domain_data,
	},
	{
		.compatible = "amlogic,t5-power-domain",
		.data = &t5_pm_domain_data,
	},
	{
		.compatible = "amlogic,t7-power-domain",
		.data = &t7_pm_domain_data,
	},
	{
		.compatible = "amlogic,s4-power-domain",
		.data = &s4_pm_domain_data,
	},
	{
		.compatible = "amlogic,t3-power-domain",
		.data = &t3_pm_domain_data,
	},
	{
		.compatible = "amlogic,p1-power-domain",
		.data = &p1_pm_domain_data,
	},
	{
		.compatible = "amlogic,t5w-power-domain",
		.data = &t5w_pm_domain_data,
	},
	{
		.compatible = "amlogic,a5-power-domain",
		.data = &a5_pm_domain_data,
	},
	{}
};

static struct platform_driver sec_pd_driver = {
	.probe		= sec_pd_probe,
	.driver		= {
		.name	= "sec_pd",
		.of_match_table = pd_match_table,
	},
};

#ifdef MODULE
int __init sec_pd_init(void)
{
	return platform_driver_register(&sec_pd_driver);
}

void sec_pd_exit(void)
{
	platform_driver_unregister(&sec_pd_driver);
}
#else
static int sec_pd_init(void)
{
	return platform_driver_register(&sec_pd_driver);
}
arch_initcall_sync(sec_pd_init);
#endif
