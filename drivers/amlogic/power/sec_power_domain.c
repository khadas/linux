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
#include <linux/kallsyms.h>

struct sec_pm_domain {
	struct generic_pm_domain base;
	int pd_index;
	bool pd_status;
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

#define POWER_DOMAIN(_name, index, status, flag)		\
{					\
		.base = {					\
			.name = #_name,				\
			.power_off = sec_pm_domain_power_off,	\
			.power_on = sec_pm_domain_power_on,	\
			.flags = flag, \
		},						\
		.pd_index = index,				\
		.pd_status = status,				\
}

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
