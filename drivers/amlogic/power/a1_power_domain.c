// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/amlogic/pwr_ctrl.h>
#include <dt-bindings/power/a1-pd.h>

struct a1_pm_domain {
	struct generic_pm_domain base;
	int pd_index;
	bool pd_status;
};

static inline struct a1_pm_domain *
to_a1_pm_domain(struct generic_pm_domain *genpd)
{
	return container_of(genpd, struct a1_pm_domain, base);
}

static int a1_pm_domain_power_off(struct generic_pm_domain *genpd)
{
	struct a1_pm_domain *pd = to_a1_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->pd_index, PWR_OFF);

	return 0;
}

static int a1_pm_domain_power_on(struct generic_pm_domain *genpd)
{
	struct a1_pm_domain *pd = to_a1_pm_domain(genpd);

	pwr_ctrl_psci_smc(pd->pd_index, PWR_ON);

	return 0;
}

#define POWER_DOMAIN(_name, index, status, flag)		\
struct a1_pm_domain _name = {					\
		.base = {					\
			.name = #_name,				\
			.power_off = a1_pm_domain_power_off,	\
			.power_on = a1_pm_domain_power_on,	\
			.flags = flag, \
		},						\
		.pd_index = index,				\
		.pd_status = status,				\
}

static POWER_DOMAIN(cpu_pwr0, PDID_CPU_PWR0, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(cpu_core0, PDID_CPU_CORE0, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(cpu_core1, PDID_CPU_CORE1, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(dsp_a, PDID_DSP_A, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(dsp_b, PDID_DSP_B, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(uart, PDID_UART, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(mmc, PDID_MMC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON);
/* If there is GENPD_FLAG_ALWAYS_ON, the domian must be initialized to on */
static POWER_DOMAIN(i2c, PDID_I2C, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(psram, PDID_PSRAM, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(acodec, PDID_ACODEC, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(audio, PDID_AUDIO, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(mkl_otp, PDID_MKL_OTP, DOMAIN_INIT_ON,
		    GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(dma, PDID_DMA, DOMAIN_INIT_ON, GENPD_FLAG_IRQ_SAFE);
static POWER_DOMAIN(sdemmc, PDID_SDEMMC, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(sram_a, PDID_SRAM_A, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(sram_b, PDID_SRAM_B, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(ir, PDID_IR, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(spicc, PDID_SPICC, DOMAIN_INIT_OFF, 0);
static POWER_DOMAIN(spifc, PDID_SPIFC, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(usb, PDID_USB, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(nic, PDID_NIC, DOMAIN_INIT_ON, GENPD_FLAG_ALWAYS_ON);
static POWER_DOMAIN(pdm, PDID_PDM, DOMAIN_INIT_ON, 0);
static POWER_DOMAIN(rsa, PDID_RSA, DOMAIN_INIT_OFF, 0);

static struct generic_pm_domain *a1_onecell_domains[] = {
		[PDID_CPU_PWR0]			= &cpu_pwr0.base,
		[PDID_CPU_CORE0]		= &cpu_core0.base,
		[PDID_CPU_CORE1]		= &cpu_core1.base,
		[3]						= NULL,
		[4]						= NULL,
		[5]						= NULL,
		[6]						= NULL,
		[7]						= NULL,
		[PDID_DSP_A]			= &dsp_a.base,
		[PDID_DSP_B]			= &dsp_b.base,
		[PDID_UART]				= &uart.base,
		[PDID_MMC]				= &mmc.base,
		[PDID_I2C]				= &i2c.base,
		[PDID_PSRAM]			= &psram.base,
		[PDID_ACODEC]			= &acodec.base,
		[PDID_AUDIO]			= &audio.base,
		[PDID_MKL_OTP]			= &mkl_otp.base,
		[PDID_DMA]				= &dma.base,
		[PDID_SDEMMC]			= &sdemmc.base,
		[PDID_SRAM_A]			= &sram_a.base,
		[PDID_SRAM_B]			= &sram_b.base,
		[PDID_IR]				= &ir.base,
		[PDID_SPICC]			= &spicc.base,
		[PDID_SPIFC]			= &spifc.base,
		[PDID_USB]				= &usb.base,
		[PDID_NIC]				= &nic.base,
		[PDID_PDM]				= &pdm.base,
		[PDID_RSA]				= &rsa.base,
};

static struct genpd_onecell_data a1_pd_onecell_data = {
	.domains = a1_onecell_domains,
	.num_domains = 28,
};

static int a1_pd_probe(struct platform_device *pdev)
{
	int ret, i;
	struct a1_pm_domain *pd;

	for (i = 0; i < a1_pd_onecell_data.num_domains; i++) {
		/* array might be sparse */
		if (!a1_pd_onecell_data.domains[i])
			continue;

		/* Initialize based on pd_status */
		pd = to_a1_pm_domain(a1_pd_onecell_data.domains[i]);
		if (pd->base.flags & GENPD_FLAG_ALWAYS_ON)
			pm_genpd_init(a1_pd_onecell_data.domains[i],
				      &pm_domain_always_on_gov, pd->pd_status);
		else
			pm_genpd_init(a1_pd_onecell_data.domains[i],
				      NULL, pd->pd_status);
	}

	pd_dev_create_file(&pdev->dev, PDID_DSP_A, PDID_MAX,
			   a1_onecell_domains);

	ret = of_genpd_add_provider_onecell(pdev->dev.of_node,
					    &a1_pd_onecell_data);
	if (ret)
		goto out;

	return 0;

out:
	pd_dev_remove_file(&pdev->dev);
	return ret;
}

static const struct of_device_id pd_match_table[] = {
	{ .compatible = "amlogic,a1-power-domain" },
	{}
};

static struct platform_driver a1_pd_driver = {
	.probe		= a1_pd_probe,
	.driver		= {
		.name	= "a1_pd",
		.of_match_table = pd_match_table,
	},
};

static int a1_pd_init(void)
{
	return platform_driver_register(&a1_pd_driver);
}
arch_initcall_sync(a1_pd_init);
