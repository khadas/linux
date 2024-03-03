// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip UFS Host Controller driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co.Ltd.
 */

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <ufs/ufshcd.h>
#include <ufs/unipro.h>
#include "ufshcd-pltfrm.h"
#include "ufshcd-dwc.h"
#include "ufs-rockchip.h"

static inline bool ufshcd_is_hba_active(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_CONTROLLER_ENABLE) & CONTROLLER_ENABLE;
}

static int ufs_rockchip_hce_enable_notify(struct ufs_hba *hba,
					 enum ufs_notify_change_status status)
{
	int err = 0;

	if (status == PRE_CHANGE) {
		int retry_outer = 3;
		int retry_inner;
start:
		if (ufshcd_is_hba_active(hba))
			/* change controller state to "reset state" */
			ufshcd_hba_stop(hba);

		/* UniPro link is disabled at this point */
		ufshcd_set_link_off(hba);

		/* start controller initialization sequence */
		ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);

		usleep_range(100, 200);

		/* wait for the host controller to complete initialization */
		retry_inner = 50;
		while (!ufshcd_is_hba_active(hba)) {
			if (retry_inner) {
				retry_inner--;
			} else {
				dev_err(hba->dev,
					"Controller enable failed\n");
				if (retry_outer) {
					retry_outer--;
					goto start;
				}
				return -EIO;
			}
			usleep_range(1000, 1100);
		}
	} else { /* POST_CHANGE */
		err = ufshcd_vops_phy_initialization(hba);
	}

	return err;
}

static int ufs_rockchip_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
	enum ufs_notify_change_status status)
{
	struct ufs_rockchip_host *host = ufshcd_get_variant(hba);

	if (status == PRE_CHANGE)
		return 0;

	if (pm_op == UFS_RUNTIME_PM)
		return 0;

	if (host->in_suspend) {
		WARN_ON(1);
		return 0;
	}

	/* set ref clk out disable */
	clk_disable_unprepare(host->ref_out_clk);

	host->in_suspend = true;

	return 0;
}

static int ufs_rockchip_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_rockchip_host *host = ufshcd_get_variant(hba);
	int err;

	if (!host->in_suspend)
		return 0;

	/* set ref clk out enable */
	err = clk_prepare_enable(host->ref_out_clk);
	if (err) {
		dev_err(hba->dev, "failed to enable ref out clock %d\n", err);
		return err;
	}

	host->in_suspend = false;
	return 0;
}

static void ufs_rockchip_set_pm_lvl(struct ufs_hba *hba)
{
	hba->rpm_lvl = UFS_PM_LVL_1;
	hba->spm_lvl = UFS_PM_LVL_3;
}

static const unsigned char rk3576_phy_value[15][4] = {
	{0x03, 0x38, 0x50, 0x80},
	{0x03, 0x14, 0x58, 0x80},
	{0x03, 0x26, 0x58, 0x80},
	{0x03, 0x49, 0x58, 0x80},
	{0x03, 0x5A, 0x58, 0x80},
	{0xC3, 0x38, 0x50, 0xC0},
	{0xC3, 0x14, 0x58, 0xC0},
	{0xC3, 0x26, 0x58, 0xC0},
	{0xC3, 0x49, 0x58, 0xC0},
	{0xC3, 0x5A, 0x58, 0xC0},
	{0x43, 0x38, 0x50, 0xC0},
	{0x43, 0x14, 0x58, 0xC0},
	{0x43, 0x26, 0x58, 0xC0},
	{0x43, 0x49, 0x58, 0xC0},
	{0x43, 0x5A, 0x58, 0xC0}
};

static int ufs_rockchip_rk3576_phy_init(struct ufs_hba *hba)
{
	struct ufs_rockchip_host *host = ufshcd_get_variant(hba);
	u32 try_case = host->phy_config_mode, value;

	if (try_case >= ARRAY_SIZE(rk3576_phy_value))
		try_case = 0;

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_LOCAL_TX_LCC_ENABLE, 0x0), 0x0);
	/* enable the mphy DME_SET cfg */
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x40);
	for (int i = 0; i < 2; i++) {
		/* Configuration M-TX */
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0xaa, SEL_TX_LANE0 + i), 0x06);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0xa9, SEL_TX_LANE0 + i), 0x02);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0xad, SEL_TX_LANE0 + i), 0x44);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0xac, SEL_TX_LANE0 + i), 0xe6);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0xab, SEL_TX_LANE0 + i), 0x07);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x94, SEL_TX_LANE0 + i), 0x93);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x93, SEL_TX_LANE0 + i), 0xc9);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x7f, SEL_TX_LANE0 + i), 0x00);
		/* Configuration M-RX */
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x12, SEL_RX_LANE0 + i), 0x06);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x11, SEL_RX_LANE0 + i), 0x00);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x1d, SEL_RX_LANE0 + i), 0x58);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x1c, SEL_RX_LANE0 + i), 0x8c);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x1b, SEL_RX_LANE0 + i), 0x02);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x25, SEL_RX_LANE0 + i), 0xf6);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x2f, SEL_RX_LANE0 + i), 0x69);
	}
	/* disable the mphy DME_SET cfg */
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x00);

	ufs_sys_writel(host->mphy_base, 0x80, 0x08C);
	ufs_sys_writel(host->mphy_base, 0xB5, 0x110);
	ufs_sys_writel(host->mphy_base, 0xB5, 0x250);

	value = rk3576_phy_value[try_case][0];
	ufs_sys_writel(host->mphy_base, value, 0x134);
	ufs_sys_writel(host->mphy_base, value, 0x274);

	value = rk3576_phy_value[try_case][1];
	ufs_sys_writel(host->mphy_base, value, 0x0E0);
	ufs_sys_writel(host->mphy_base, value, 0x220);

	value = rk3576_phy_value[try_case][2];
	ufs_sys_writel(host->mphy_base, value, 0x164);
	ufs_sys_writel(host->mphy_base, value, 0x2A4);

	value = rk3576_phy_value[try_case][3];
	ufs_sys_writel(host->mphy_base, value, 0x178);
	ufs_sys_writel(host->mphy_base, value, 0x2B8);

	ufs_sys_writel(host->mphy_base, 0x18, 0x1B0);
	ufs_sys_writel(host->mphy_base, 0x18, 0x2F0);

	ufs_sys_writel(host->mphy_base, 0xC0, 0x120);
	ufs_sys_writel(host->mphy_base, 0xC0, 0x260);

	ufs_sys_writel(host->mphy_base, 0x03, 0x094);

	ufs_sys_writel(host->mphy_base, 0x03, 0x1B4);
	ufs_sys_writel(host->mphy_base, 0x03, 0x2F4);

	ufs_sys_writel(host->mphy_base, 0xC0, 0x08C);
	udelay(1);
	ufs_sys_writel(host->mphy_base, 0x00, 0x08C);

	udelay(200);
	/* start link up */
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(MIB_T_DBG_CPORT_TX_ENDIAN, 0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(MIB_T_DBG_CPORT_RX_ENDIAN, 0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(N_DEVICEID, 0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(N_DEVICEID_VALID, 0), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_PEERDEVICEID, 0), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_CONNECTIONSTATE, 0), 0x1);

	return 0;
}

static int ufs_rockchip_common_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_rockchip_host *host;
	int err = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	/* system control register for hci */
	host->ufs_sys_ctrl = devm_platform_ioremap_resource_byname(pdev, "hci_grf");
	if (IS_ERR(host->ufs_sys_ctrl)) {
		dev_err(dev, "cannot ioremap for hci system control register\n");
		return PTR_ERR(host->ufs_sys_ctrl);
	}

	/* system control register for mphy */
	host->ufs_phy_ctrl = devm_platform_ioremap_resource_byname(pdev, "mphy_grf");
	if (IS_ERR(host->ufs_phy_ctrl)) {
		dev_err(dev, "cannot ioremap for mphy system control register\n");
		return PTR_ERR(host->ufs_phy_ctrl);
	}

	/* mphy base register */
	host->mphy_base = devm_platform_ioremap_resource_byname(pdev, "mphy");
	if (IS_ERR(host->mphy_base)) {
		dev_err(dev, "cannot ioremap for mphy base register\n");
		return PTR_ERR(host->mphy_base);
	}

	host->rst = devm_reset_control_array_get_exclusive(dev);
	if (IS_ERR(host->rst)) {
		dev_err(dev, "failed to get reset control\n");
		return PTR_ERR(host->rst);
	}

	reset_control_assert(host->rst);
	udelay(1);
	reset_control_deassert(host->rst);

	host->ref_out_clk = devm_clk_get(dev, "ref_out");
	if (IS_ERR(host->ref_out_clk)) {
		dev_err(dev, "ciu-drive not available\n");
		return PTR_ERR(host->ref_out_clk);
	}
	err = clk_prepare_enable(host->ref_out_clk);
	if (err) {
		dev_err(dev, "failed to enable ref out clock %d\n", err);
		return err;
	}

	host->rst_gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(host->rst_gpio)) {
		dev_err(&pdev->dev, "invalid reset-gpios property in node\n");
		err = PTR_ERR(host->rst_gpio);
		goto out;
	}
	udelay(20);
	gpiod_set_value_cansleep(host->rst_gpio, 1);

	host->clks[0].id = "core";
	host->clks[1].id = "pclk";
	host->clks[2].id = "pclk_mphy";
	err = devm_clk_bulk_get_optional(dev, UFS_MAX_CLKS, host->clks);
	if (err) {
		dev_err(dev, "failed to get clocks %d\n", err);
		goto out;
	}

	err = clk_bulk_prepare_enable(UFS_MAX_CLKS, host->clks);
	if (err) {
		dev_err(dev, "failed to enable clocks %d\n", err);
		goto out;
	}

	if (device_property_read_u32(dev, "ufs-phy-config-mode",
				     &host->phy_config_mode))
		host->phy_config_mode = 0;

	pm_runtime_set_active(&pdev->dev);

	host->hba = hba;
	ufs_rockchip_set_pm_lvl(hba);

	ufshcd_set_variant(hba, host);

	return 0;
out:
	clk_disable_unprepare(host->ref_out_clk);
	return err;
}

static int ufs_rockchip_rk3576_init(struct ufs_hba *hba)
{
	int ret = 0;
	struct device *dev = hba->dev;

	hba->quirks = UFSHCI_QUIRK_BROKEN_HCE | UFSHCD_QUIRK_SKIP_DEF_UNIPRO_TIMEOUT_SETTING;

	/* Enable runtime autosuspend */
	hba->caps |= UFSHCD_CAP_RPM_AUTOSUSPEND;
	/* Enable clock-gating */
	hba->caps |= UFSHCD_CAP_CLK_GATING;
	/* Enable BKOPS when suspend */
	hba->caps |= UFSHCD_CAP_AUTO_BKOPS_SUSPEND;
	/* Enable putting device into deep sleep */
	hba->caps |= UFSHCD_CAP_DEEPSLEEP;

	ret = ufs_rockchip_common_init(hba);
	if (ret) {
		dev_err(dev, "ufs common init fail\n");
		return ret;
	}

	return 0;
}

static int ufs_rockchip_device_reset(struct ufs_hba *hba)
{
	struct ufs_rockchip_host *host = ufshcd_get_variant(hba);

	if (!host->rst_gpio)
		return -EOPNOTSUPP;

	gpiod_set_value_cansleep(host->rst_gpio, 0);
	udelay(20);

	gpiod_set_value_cansleep(host->rst_gpio, 1);
	udelay(20);

	return 0;
}

static const struct ufs_hba_variant_ops ufs_hba_rk3576_vops = {
	.name = "rk3576",
	.init = ufs_rockchip_rk3576_init,
	.device_reset = ufs_rockchip_device_reset,
	.hce_enable_notify = ufs_rockchip_hce_enable_notify,
	.phy_initialization = ufs_rockchip_rk3576_phy_init,
	.suspend = ufs_rockchip_suspend,
	.resume = ufs_rockchip_resume,
};

static const struct of_device_id ufs_rockchip_of_match[] = {
	{ .compatible = "rockchip,rk3576-ufs", .data = &ufs_hba_rk3576_vops},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_rockchip_of_match);

static int ufs_rockchip_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	const struct ufs_hba_variant_ops *vops;

	vops = device_get_match_data(dev);
	err = ufshcd_pltfrm_init(pdev, vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

static int ufs_rockchip_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba = platform_get_drvdata(pdev);

	ufshcd_remove(hba);
	return 0;
}

static int ufs_rockchip_runtime_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_rockchip_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	ret = ufshcd_runtime_suspend(dev);
	if (ret)
		return ret;

	clk_disable_unprepare(host->ref_out_clk);

	return 0;
}

static int ufs_rockchip_runtime_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_rockchip_host *host = ufshcd_get_variant(hba);
	int err;

	err = clk_prepare_enable(host->ref_out_clk);
	if (err) {
		dev_err(hba->dev, "failed to enable ref out clock %d\n", err);
		return err;
	}

	return ufshcd_runtime_resume(dev);
}

static const struct dev_pm_ops ufs_rockchip_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ufshcd_system_suspend, ufshcd_system_resume)
	SET_RUNTIME_PM_OPS(ufs_rockchip_runtime_suspend, ufs_rockchip_runtime_resume, NULL)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
};

static struct platform_driver ufs_rockchip_pltform = {
	.probe = ufs_rockchip_probe,
	.remove = ufs_rockchip_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver = {
		.name = "ufshcd-rockchip",
		.pm = &ufs_rockchip_pm_ops,
		.of_match_table = of_match_ptr(ufs_rockchip_of_match),
	},
};
module_platform_driver(ufs_rockchip_pltform);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip UFS Host Driver");
