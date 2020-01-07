// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/time64.h>

#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/amlogic/scpi_protocol.h>
/*
 * Seconds sinc epoch time
 */
static unsigned long long vrtc_init_date;
#endif

struct meson_vrtc_data {
	void __iomem *io_alarm;
	struct rtc_device *rtc;
	unsigned long alarm_time;
	bool enabled;
};

static int meson_vrtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct timespec64 time;
#ifdef CONFIG_AMLOGIC_MODIFY
	u32 vrtc_val;
#endif

	dev_dbg(dev, "%s\n", __func__);
#ifdef CONFIG_AMLOGIC_MODIFY
	ktime_get_boottime_ts64(&time);
	if (scpi_get_vrtc(&vrtc_val) == 0)
		time.tv_sec += vrtc_val;
#else
	ktime_get_raw_ts64(&time);
#endif
	rtc_time64_to_tm(time.tv_sec, tm);

	return 0;
}

#ifdef CONFIG_AMLOGIC_MODIFY
static int meson_vrtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;
	struct timespec64 boot_time;
	u32 vrtc_val;

	rtc_tm_to_time(tm, &time);
	ktime_get_boottime_ts64(&boot_time);
	vrtc_val = time - boot_time.tv_sec;

	scpi_set_vrtc(vrtc_val);
	return 0;
}
#endif

static void meson_vrtc_set_wakeup_time(struct meson_vrtc_data *vrtc,
				       unsigned long time)
{
	writel_relaxed(time, vrtc->io_alarm);
}

static int meson_vrtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: alarm->enabled=%d\n", __func__, alarm->enabled);
	if (alarm->enabled)
		vrtc->alarm_time = rtc_tm_to_time64(&alarm->time);
	else
		vrtc->alarm_time = 0;

	return 0;
}

static int meson_vrtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	vrtc->enabled = enabled;
	return 0;
}

static const struct rtc_class_ops meson_vrtc_ops = {
	.read_time = meson_vrtc_read_time,
#ifdef CONFIG_AMLOGIC_MODIFY
	.set_time = meson_vrtc_set_time,
#endif
	.set_alarm = meson_vrtc_set_alarm,
	.alarm_irq_enable = meson_vrtc_alarm_irq_enable,
};

static int meson_vrtc_probe(struct platform_device *pdev)
{
	struct meson_vrtc_data *vrtc;
	int ret;
#ifdef CONFIG_AMLOGIC_MODIFY
	u32 vrtc_val;
#endif

	vrtc = devm_kzalloc(&pdev->dev, sizeof(*vrtc), GFP_KERNEL);
	if (!vrtc)
		return -ENOMEM;

	vrtc->io_alarm = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(vrtc->io_alarm))
		return PTR_ERR(vrtc->io_alarm);

	device_init_wakeup(&pdev->dev, 1);

	platform_set_drvdata(pdev, vrtc);

	vrtc->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(vrtc->rtc))
		return PTR_ERR(vrtc->rtc);

	vrtc->rtc->ops = &meson_vrtc_ops;
	ret = rtc_register_device(vrtc->rtc);
	if (ret)
		return ret;

#ifdef CONFIG_AMLOGIC_MODIFY
	if (scpi_get_vrtc(&vrtc_val) == 0)
		vrtc_init_date = vrtc_val;
	else
		vrtc_init_date = 0;
#endif

	return 0;
}

static int __maybe_unused meson_vrtc_suspend(struct device *dev)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);
	if (vrtc->alarm_time) {
		unsigned long local_time;
		long alarm_secs;
		struct timespec64 time;

		ktime_get_raw_ts64(&time);
		local_time = time.tv_sec;

		dev_dbg(dev, "alarm_time = %lus, local_time=%lus\n",
			vrtc->alarm_time, local_time);
		alarm_secs = vrtc->alarm_time - local_time;
		if (alarm_secs > 0) {
			meson_vrtc_set_wakeup_time(vrtc, alarm_secs);
			dev_dbg(dev, "system will wakeup in %lds.\n",
				alarm_secs);
		} else {
			dev_err(dev, "alarm time already passed: %lds.\n",
				alarm_secs);
		}
	}

	return 0;
}

static int __maybe_unused meson_vrtc_resume(struct device *dev)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	vrtc->alarm_time = 0;
	meson_vrtc_set_wakeup_time(vrtc, 0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(meson_vrtc_pm_ops,
			 meson_vrtc_suspend, meson_vrtc_resume);

#ifdef CONFIG_AMLOGIC_MODIFY
static void meson_vrtc_shutdown(struct platform_device *pdev)
{
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	scpi_set_vrtc(now.tv_sec);
}
#endif

static const struct of_device_id meson_vrtc_dt_match[] = {
	{ .compatible = "amlogic,meson-vrtc"},
	{},
};
MODULE_DEVICE_TABLE(of, meson_vrtc_dt_match);

static struct platform_driver meson_vrtc_driver = {
	.probe = meson_vrtc_probe,
	.driver = {
		.name = "meson-vrtc",
		.of_match_table = meson_vrtc_dt_match,
		.pm = &meson_vrtc_pm_ops,
	},
#ifdef CONFIG_AMLOGIC_MODIFY
	.shutdown = meson_vrtc_shutdown,
#endif
};

module_platform_driver(meson_vrtc_driver);

MODULE_DESCRIPTION("Amlogic Virtual Wakeup RTC Timer driver");
MODULE_LICENSE("GPL");
