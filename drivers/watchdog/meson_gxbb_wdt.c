// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 *
 */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/of_device.h>
#endif

#define DEFAULT_TIMEOUT	30	/* seconds */

#define GXBB_WDT_CTRL_REG			0x0
#define GXBB_WDT_CTRL1_REG			0x4
#define GXBB_WDT_TCNT_REG			0x8
#define GXBB_WDT_RSET_REG			0xc

#define GXBB_WDT_CTRL_CLKDIV_EN			BIT(25)
#define GXBB_WDT_CTRL_CLK_EN			BIT(24)
#define GXBB_WDT_CTRL_EE_RESET			BIT(21)
#define GXBB_WDT_CTRL_EN			BIT(18)
#define GXBB_WDT_CTRL_DIV_MASK			(BIT(18) - 1)

#define GXBB_WDT_TCNT_SETUP_MASK		(BIT(16) - 1)
#define GXBB_WDT_TCNT_CNT_SHIFT			16
#define GXBB_WDT_RST_SIG_EN			BIT(17)

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started default="
	 __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static unsigned int timeout = DEFAULT_TIMEOUT;
module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds="
	 __MODULE_STRING(DEFAULT_TIMEOUT) ")");

struct meson_gxbb_wdt {
	void __iomem *reg_base;
	struct watchdog_device wdt_dev;
	struct clk *clk;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned int feed_watchdog_mode;
#endif
};

static int meson_gxbb_wdt_start(struct watchdog_device *wdt_dev)
{
	struct meson_gxbb_wdt *data = watchdog_get_drvdata(wdt_dev);

	writel(readl(data->reg_base + GXBB_WDT_CTRL_REG) | GXBB_WDT_CTRL_EN,
	       data->reg_base + GXBB_WDT_CTRL_REG);

	return 0;
}

static int meson_gxbb_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct meson_gxbb_wdt *data = watchdog_get_drvdata(wdt_dev);

	writel(readl(data->reg_base + GXBB_WDT_CTRL_REG) & ~GXBB_WDT_CTRL_EN,
	       data->reg_base + GXBB_WDT_CTRL_REG);

	return 0;
}

static int meson_gxbb_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct meson_gxbb_wdt *data = watchdog_get_drvdata(wdt_dev);

	writel(0, data->reg_base + GXBB_WDT_RSET_REG);

	return 0;
}

static int meson_gxbb_wdt_set_timeout(struct watchdog_device *wdt_dev,
				      unsigned int timeout)
{
	struct meson_gxbb_wdt *data = watchdog_get_drvdata(wdt_dev);
	unsigned long tcnt = timeout * 1000;

	if (tcnt > GXBB_WDT_TCNT_SETUP_MASK)
		tcnt = GXBB_WDT_TCNT_SETUP_MASK;

	wdt_dev->timeout = timeout;

	meson_gxbb_wdt_ping(wdt_dev);

	writel(tcnt, data->reg_base + GXBB_WDT_TCNT_REG);

	return 0;
}

static unsigned int meson_gxbb_wdt_get_timeleft(struct watchdog_device *wdt_dev)
{
	struct meson_gxbb_wdt *data = watchdog_get_drvdata(wdt_dev);
	unsigned long reg;

	reg = readl(data->reg_base + GXBB_WDT_TCNT_REG);

	return ((reg & GXBB_WDT_TCNT_SETUP_MASK) -
		(reg >> GXBB_WDT_TCNT_CNT_SHIFT)) / 1000;
}

static const struct watchdog_ops meson_gxbb_wdt_ops = {
	.start = meson_gxbb_wdt_start,
	.stop = meson_gxbb_wdt_stop,
	.ping = meson_gxbb_wdt_ping,
	.set_timeout = meson_gxbb_wdt_set_timeout,
	.get_timeleft = meson_gxbb_wdt_get_timeleft,
};

static const struct watchdog_info meson_gxbb_wdt_info = {
	.identity = "Meson GXBB Watchdog",
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static int __maybe_unused meson_gxbb_wdt_resume(struct device *dev)
{
	struct meson_gxbb_wdt *data = dev_get_drvdata(dev);

#ifdef CONFIG_AMLOGIC_MODIFY
	if (watchdog_active(&data->wdt_dev) ||
	    watchdog_hw_running(&data->wdt_dev))
		meson_gxbb_wdt_start(&data->wdt_dev);
#else
	if (watchdog_active(&data->wdt_dev))
		meson_gxbb_wdt_start(&data->wdt_dev);
#endif

	return 0;
}

static int __maybe_unused meson_gxbb_wdt_suspend(struct device *dev)
{
	struct meson_gxbb_wdt *data = dev_get_drvdata(dev);

#ifdef CONFIG_AMLOGIC_MODIFY
	if (watchdog_active(&data->wdt_dev) ||
	    watchdog_hw_running(&data->wdt_dev))
		meson_gxbb_wdt_stop(&data->wdt_dev);
#else
	if (watchdog_active(&data->wdt_dev))
		meson_gxbb_wdt_stop(&data->wdt_dev);
#endif

	return 0;
}

static const struct dev_pm_ops meson_gxbb_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(meson_gxbb_wdt_suspend, meson_gxbb_wdt_resume)
};

#ifdef CONFIG_AMLOGIC_MODIFY
struct wdt_params {
	u8 rst_shift;
};

static const struct wdt_params sc2_params __initconst = {
	.rst_shift = 22,
};

static const struct wdt_params gxbb_params __initconst = {
	.rst_shift = 21,
};
#endif

static const struct of_device_id meson_gxbb_wdt_dt_ids[] = {
#ifndef CONFIG_AMLOGIC_MODIFY
	 { .compatible = "amlogic,meson-gxbb-wdt", },
#else
	 { .compatible = "amlogic,meson-gxbb-wdt", .data = &gxbb_params},
	 { .compatible = "amlogic,meson-sc2-wdt", .data = &sc2_params},
#endif
	 { /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, meson_gxbb_wdt_dt_ids);

static void meson_clk_disable_unprepare(void *data)
{
	clk_disable_unprepare(data);
}

#ifdef CONFIG_AMLOGIC_MODIFY
static void meson_gxbb_wdt_shutdown(struct platform_device *pdev)
{
	struct meson_gxbb_wdt *data = platform_get_drvdata(pdev);

	if (watchdog_active(&data->wdt_dev) ||
	    watchdog_hw_running(&data->wdt_dev))
		meson_gxbb_wdt_stop(&data->wdt_dev);
};
#endif

static int meson_gxbb_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_gxbb_wdt *data;
	int ret;
#ifdef CONFIG_AMLOGIC_MODIFY
	struct wdt_params *wdt_params;
	int reset_by_soc;
#endif

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->reg_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->reg_base))
		return PTR_ERR(data->reg_base);

	data->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(data->clk))
		return PTR_ERR(data->clk);

	ret = clk_prepare_enable(data->clk);
	if (ret)
		return ret;
	ret = devm_add_action_or_reset(dev, meson_clk_disable_unprepare,
				       data->clk);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, data);

	data->wdt_dev.parent = dev;
	data->wdt_dev.info = &meson_gxbb_wdt_info;
	data->wdt_dev.ops = &meson_gxbb_wdt_ops;
	data->wdt_dev.max_hw_heartbeat_ms = GXBB_WDT_TCNT_SETUP_MASK;
	data->wdt_dev.min_timeout = 1;
	data->wdt_dev.timeout = DEFAULT_TIMEOUT;
	watchdog_init_timeout(&data->wdt_dev, timeout, dev);
	watchdog_set_nowayout(&data->wdt_dev, nowayout);
	watchdog_set_drvdata(&data->wdt_dev, data);

#ifndef CONFIG_AMLOGIC_MODIFY
	/* Setup with 1ms timebase */
	writel(((clk_get_rate(data->clk) / 1000) & GXBB_WDT_CTRL_DIV_MASK) |
		GXBB_WDT_CTRL_EE_RESET |
		GXBB_WDT_CTRL_CLK_EN |
		GXBB_WDT_CTRL_CLKDIV_EN,
		data->reg_base + GXBB_WDT_CTRL_REG);
#else
	wdt_params = (struct wdt_params *)of_device_get_match_data(dev);

	reset_by_soc = !(readl(data->reg_base + GXBB_WDT_CTRL1_REG) &
			 GXBB_WDT_RST_SIG_EN);

	/* Setup with 1ms timebase */
	writel(((clk_get_rate(data->clk) / 1000) & GXBB_WDT_CTRL_DIV_MASK) |
		(reset_by_soc << wdt_params->rst_shift) |
		GXBB_WDT_CTRL_CLK_EN |
		GXBB_WDT_CTRL_CLKDIV_EN,
		data->reg_base + GXBB_WDT_CTRL_REG);
#endif
	meson_gxbb_wdt_set_timeout(&data->wdt_dev, data->wdt_dev.timeout);

#ifdef CONFIG_AMLOGIC_MODIFY
	ret = of_property_read_u32(pdev->dev.of_node,
				   "amlogic,feed_watchdog_mode",
				   &data->feed_watchdog_mode);
	if (ret)
		data->feed_watchdog_mode = 1;
	if (data->feed_watchdog_mode == 1) {
		set_bit(WDOG_HW_RUNNING, &data->wdt_dev.status);
		meson_gxbb_wdt_start(&data->wdt_dev);
	}
	dev_info(&pdev->dev, "feeding watchdog mode: [%s]\n",
		 data->feed_watchdog_mode ? "kernel" : "userspace");
#endif

	return devm_watchdog_register_device(dev, &data->wdt_dev);
}

static struct platform_driver meson_gxbb_wdt_driver = {
	.probe	= meson_gxbb_wdt_probe,
	.driver = {
		.name = "meson-gxbb-wdt",
		.pm = &meson_gxbb_wdt_pm_ops,
		.of_match_table	= meson_gxbb_wdt_dt_ids,
	},
#ifdef CONFIG_AMLOGIC_MODIFY
	.shutdown = meson_gxbb_wdt_shutdown,
#endif
};

module_platform_driver(meson_gxbb_wdt_driver);

MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_DESCRIPTION("Amlogic Meson GXBB Watchdog timer driver");
MODULE_LICENSE("Dual BSD/GPL");
