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
#include <linux/kallsyms.h>

struct sec_pm_domain {
	struct generic_pm_domain base;
	int pd_index;
	bool pd_status;
	int pd_parent;
};

struct sec_pm_domain_data {
	struct sec_pm_domain *domains;
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

	pwr_ctrl_psci_smc(pd->pd_index, PWR_OFF);

	return 0;
}

static int sec_pm_domain_power_on(struct generic_pm_domain *genpd)
{
	struct sec_pm_domain *pd = to_sec_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->pd_index, PWR_ON);

	return 0;
}

#define TOP_DOMAIN(_name, index, status, flag, parent)		\
{					\
		.base = {					\
			.name = #_name,				\
			.power_off = sec_pm_domain_power_off,	\
			.power_on = sec_pm_domain_power_on,	\
			.flags = flag, \
		},						\
		.pd_index = index,				\
		.pd_status = status,				\
		.pd_parent = parent,				\
}

#define POWER_DOMAIN(_name, index, status, flag)	\
	TOP_DOMAIN(_name, index, status, flag, 0)

static struct sec_pm_domain a1_pm_domains[] = {
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

static struct sec_pm_domain_data a1_pm_domain_data = {
	.domains = a1_pm_domains,
	.domains_count = ARRAY_SIZE(a1_pm_domains),
};

static struct sec_pm_domain c1_pm_domains[] = {
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

static struct sec_pm_domain_data c1_pm_domain_data = {
	.domains = c1_pm_domains,
	.domains_count = ARRAY_SIZE(c1_pm_domains),
};

static struct sec_pm_domain sc2_pm_domains[] = {
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

static struct sec_pm_domain_data sc2_pm_domain_data = {
	.domains = sc2_pm_domains,
	.domains_count = ARRAY_SIZE(sc2_pm_domains),
};

static struct sec_pm_domain t5_pm_domains[] = {
	[PDID_T5_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_T5_DOS_HEVC, DOMAIN_INIT_OFF, 0),
	[PDID_T5_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_T5_DOS_VDEC, DOMAIN_INIT_OFF, 0),
	[PDID_T5_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_T5_VPU_HDMI, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_T5_DEMOD] = POWER_DOMAIN(demod, PDID_T5_DEMOD, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data t5_pm_domain_data = {
	.domains = t5_pm_domains,
	.domains_count = ARRAY_SIZE(t5_pm_domains),
};

static struct sec_pm_domain t7_pm_domains[] = {
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
	[PDID_T7_HDMIRX] = POWER_DOMAIN(hdmirx, PDID_T7_HDMIRX, DOMAIN_INIT_OFF, 0),
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

static struct sec_pm_domain_data t7_pm_domain_data = {
	.domains = t7_pm_domains,
	.domains_count = ARRAY_SIZE(t7_pm_domains),
};

static struct sec_pm_domain s4_pm_domains[] = {
	[PDID_S4_DOS_HEVC] = POWER_DOMAIN(hevc, PDID_S4_DOS_HEVC, DOMAIN_INIT_OFF, 0),
	[PDID_S4_DOS_VDEC] = POWER_DOMAIN(vdec, PDID_S4_DOS_VDEC, DOMAIN_INIT_OFF, 0),
	[PDID_S4_VPU_HDMI] = POWER_DOMAIN(vpu, PDID_S4_VPU_HDMI, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_S4_USB_COMB] = POWER_DOMAIN(usb, PDID_S4_USB_COMB, DOMAIN_INIT_ON,
		GENPD_FLAG_ALWAYS_ON),
	[PDID_S4_GE2D] = POWER_DOMAIN(ge2d, PDID_S4_GE2D, DOMAIN_INIT_OFF, 0),
	[PDID_S4_ETH] = POWER_DOMAIN(eth, PDID_S4_ETH, DOMAIN_INIT_ON, 0),
	[PDID_S4_DEMOD] = POWER_DOMAIN(demod, PDID_S4_DEMOD, DOMAIN_INIT_OFF, 0),
	[PDID_S4_AUDIO] = POWER_DOMAIN(audio, PDID_S4_AUDIO, DOMAIN_INIT_OFF, 0),
};

static struct sec_pm_domain_data s4_pm_domain_data = {
	.domains = s4_pm_domains,
	.domains_count = ARRAY_SIZE(s4_pm_domains),
};

static int sec_pd_probe(struct platform_device *pdev)
{
	int ret, i;
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

	sec_pd_onecell_data->num_domains = match->domains_count;

#ifdef MODULE
	struct dev_power_governor *aon_gov;

	aon_gov = (void *)kallsyms_lookup_name("pm_domain_always_on_gov");
	if (!aon_gov) {
		pr_err("can't find symbol: pm_domain_always_on_gov\n");
		return -EINVAL;
	}
#endif

	for (i = 0; i < match->domains_count; i++) {
		pd = &match->domains[i];

		/* array might be sparse */
		if (!pd->base.name)
			continue;

		init_status = pwr_ctrl_status_psci_smc(pd->pd_index);

		if (init_status == -1 || pd->base.flags == GENPD_FLAG_ALWAYS_ON)
			init_status = pd->pd_status;

		/* Initialize based on pd_status */
		if (pd->base.flags & GENPD_FLAG_ALWAYS_ON)
#ifdef MODULE
			pm_genpd_init(&pd->base, aon_gov, init_status);
#else
			pm_genpd_init(&pd->base, &pm_domain_always_on_gov, init_status);
#endif
		else
			pm_genpd_init(&pd->base, NULL, init_status);

		sec_pd_onecell_data->domains[i] = &pd->base;
	}

	for (i = 0; i < match->domains_count; i++) {
		pd = &match->domains[i];

		if (!pd->pd_parent)
			continue;

		pm_genpd_add_subdomain(&match->domains[pd->pd_parent].base, &pd->base);
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
	{
		.compatible = "amlogic,a1-power-domain",
		.data = &a1_pm_domain_data,
	},
	{
		.compatible = "amlogic,c1-power-domain",
		.data = &c1_pm_domain_data,
	},
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
