// SPDX-License-Identifier: GPL-2.0
/*
 * Real Time Clock driver for Amlogic A5, etc.
 * Copyright (C) 2019 BayLibre, SAS
 * Author: Yao Jie <jie.yao@amlogic.com>
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/time64.h>
#include <linux/delay.h>
#include <linux/amlogic/rtc.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/clk.h>
#include <dt-bindings/rtc/amlogic,rtc.h>

#define RTC_CALIBRATION

static void meson_set_time(struct meson_rtc_data *rtc, u32 time_sec)
{
	writel(time_sec, rtc->reg_base + RTC_COUNTER_REG);
}

static u32 meson_read_time(struct meson_rtc_data *rtc)
{
	u32 time_val;

	time_val = readl(rtc->reg_base + RTC_REAL_TIME);
	return time_val;
}

static u32 meson_read_alarm(struct meson_rtc_data *rtc)
{
	u32 alarm_val;

	alarm_val = readl(rtc->reg_base + RTC_ALARM0_REG);
	return alarm_val;
}

static void meson_set_alarm(struct meson_rtc_data *rtc, u32 alarm_sec)
{
	writel(alarm_sec, rtc->reg_base + RTC_ALARM0_REG);
}

static int meson_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);
	u32 time_sec;
	time64_t time64;

	time_sec = meson_read_time(rtc);
	time64 = (time64_t)time_sec;
	dev_dbg(dev, "%s: read time = %u\n",
		__func__, time_sec);
	rtc_time64_to_tm(time64, tm);

	return 0;
}

static int meson_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);
	unsigned long time_sec;

	rtc_tm_to_time(tm, &time_sec);
	meson_set_time(rtc, time_sec);
	dev_dbg(dev, "%s: set_time = %lu\n",
		__func__, time_sec);

	return 0;
}

static int meson_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);
	u32 alarm_sec;
	u32 reg_val;
	time64_t alarm_sec64;

	/* Disable alarm0 first */
	reg_val = readl(rtc->reg_base + RTC_CTRL);
	reg_val &= (~(1 << RTC_ALRM0_EN_BIT));
	writel(reg_val, rtc->reg_base + RTC_CTRL);

	if (alarm->enabled) {
		alarm_sec64 = rtc_tm_to_time64(&alarm->time);
		if (alarm_sec64 > 0x7fffffff) {
			dev_err(dev, "alarm value invalid!\n");
			return -EINVAL;
		}

		alarm_sec = (u32)alarm_sec64;
		meson_set_alarm(rtc, alarm_sec);

		/* Enable alarm0 */
		reg_val = readl(rtc->reg_base + RTC_CTRL);
		reg_val |= (1 << RTC_ALRM0_EN_BIT);
		writel(reg_val, rtc->reg_base + RTC_CTRL);
	}

	rtc->alarm_enabled = alarm->enabled;

	dev_dbg(dev, "%s: alarm->enabled=%d alarm_set=0x%llx alarm=0x%x\n",
		__func__, alarm->enabled, alarm_sec64, alarm_sec);

	return 0;
}

static int meson_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);
	u32 alarm_sec;
	u32 reg_val;

	alarm_sec = meson_read_alarm(rtc);
	rtc_time64_to_tm((time64_t)alarm_sec, &alarm->time);
	dev_dbg(dev, "%s: alarm->enabled=%d alarm=0x%x\n", __func__,
		alarm->enabled, alarm_sec);

	reg_val = readl(rtc->reg_base + RTC_INT_STATUS);
	alarm->enabled = rtc->alarm_enabled;
	alarm->pending = (reg_val & (1 << RTC_ALRM0_STATUS_BIT)) ? 1 : 0;

	return 0;
}

static irqreturn_t meson_rtc_handler(int irq, void *data)
{
	struct meson_rtc_data *rtc = (struct meson_rtc_data *)data;

	rtc_update_irq(rtc->rtc_dev, 1, RTC_AF | RTC_IRQF);
	return IRQ_HANDLED;
}

static int meson_rtc_alarm_enable(struct device *dev, unsigned int enabled)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);
	u32 reg_val;

	dev_dbg(dev, "%s: enabled = %d\n", __func__,
		enabled);

	/* Operate RTC alarm0 by default */
	if (enabled) {
		/* Enable alarm0 */
		reg_val = readl(rtc->reg_base + RTC_CTRL);
		reg_val |= (1 << RTC_ALRM0_EN_BIT);
		writel(reg_val, rtc->reg_base + RTC_CTRL);
	} else {
		/* Disable alarm0 */
		reg_val = readl(rtc->reg_base + RTC_CTRL);
		reg_val &= (~(1 << RTC_ALRM0_EN_BIT));
		writel(reg_val, rtc->reg_base + RTC_CTRL);
	}

	rtc->alarm_enabled = enabled;
	return 0;
}

static void meson_rtc_clk_config(struct meson_rtc_data *rtc)
{
	u32 reg_val;

	if (rtc->freq == OSC_24MHZ) {
		reg_val = readl(rtc->reg_base + RTC_CTRL);
		/* Select RTC oscillator to 24MHz clock output */
		reg_val |= (1 << RTC_OSC_SEL_BIT);
		writel(reg_val, rtc->reg_base + RTC_CTRL);

		/* Set RTC oscillator to freq_out to freq_in/((N0*M0+N1*M1)/(M0+M1)) */
		reg_val = readl(rtc->reg_base + RTC_OSCIN_CTRL0);
		reg_val &= (~(0x3 << 28));
		reg_val |= (0x1 << 28);
		/* Enable clock_in gate of oscillator 24MHz */
		reg_val |= (1 << 31);
		/* N0 is set to 733, N1 is set to 732 by default*/
		writel(reg_val, rtc->reg_base + RTC_OSCIN_CTRL0);
		/* Set M0 to 2, M1 to 3, so freq_out = 32768 Hz*/
		reg_val = readl(rtc->reg_base + RTC_OSCIN_CTRL1);
		reg_val &= ~(0xfff);
		reg_val |= (0x1 << 0);
		reg_val &= ~(0xfff << 12);
		reg_val |= (0x2 << 12);
		writel(reg_val, rtc->reg_base + RTC_OSCIN_CTRL1);
	}
}

#ifdef RTC_CALIBRATION
static int meson_rtc_adjust_sec(struct device *dev, u32 match_counter,
				enum meson_rtc_adj ops)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev->parent);
	u32 reg_val;

	if (match_counter > 0x7ffff) {
		pr_err("%s: invalid match_counter\n", __func__);
		return -EINVAL;
	}

	reg_val = readl(rtc->reg_base + RTC_SEC_ADJUST_REG);
	/* Set sec_adjust_ctrl */
	reg_val &= (~(0x3 << 19));
	if (ops == ADJUST_DROP)
		reg_val |= (0x2 << 19);
	else if (ops == ADJUST_INSERT)
		reg_val |= (0x3 << 19);
	/* Set match_counter */
	reg_val &= (~(0x7ffff << 0));
	reg_val |= match_counter;
	/* Valid adjust */
	reg_val |= (0x1 << 23);
	writel(reg_val, rtc->reg_base + RTC_SEC_ADJUST_REG);

	return 0;
}

static int meson_rtc_set_calibration(struct device *dev, u32 calibration)
{
	enum meson_rtc_adj cal_ops;
	u32 match_counter;
	u32 sec_adjust_ctrl;
	int ret;

	match_counter = calibration & 0x7ffff;
	sec_adjust_ctrl = (calibration >> 30) & 0x3;
	cal_ops = sec_adjust_ctrl;

	if (cal_ops == ADJUST_MAX) {
		pr_err("%s: calibration ops val invalid!\n", __func__);
		return -EINVAL;
	}

	ret = meson_rtc_adjust_sec(dev, match_counter, cal_ops);
	dev_dbg(dev, "%s: Success to store RTC calibration attribute\n",
		__func__);

	return ret;
}

static int meson_rtc_get_calibration(struct device *dev, u32 *calibration)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev->parent);
	enum meson_rtc_adj cal_ops;
	u32 match_counter;
	u32 reg_val;
	u32 sec_adjust_ctrl;

	reg_val = readl(rtc->reg_base + RTC_SEC_ADJUST_REG);
	match_counter = reg_val & 0x7ffff;
	sec_adjust_ctrl = (reg_val >> 19) & 3U;

	if (sec_adjust_ctrl == 0 || sec_adjust_ctrl == 1)
		cal_ops = ADJUST_NONE;
	else if (sec_adjust_ctrl == 2)
		cal_ops = ADJUST_DROP;
	else
		cal_ops = ADJUST_INSERT;

	*calibration = ((cal_ops & 0x3) << 30) | (match_counter & 0x7ffff);
	return 0;
}

/* The calibration value transferred from buf takes bit[18:0] to represent   */
/* match counter, while takes bit[31:30] to represent operation style       */
/* which can be set to: 0 represents no calibration; 1 represents drop one */
/* second every match counter; 2 represents insert one second every match counter */
static ssize_t rtc_calibration_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int retval;
	int calibration = 0;

	if (sscanf(buf, " %i ", &calibration) != 1) {
		dev_err(dev, "Failed to store RTC calibration attribute\n");
		return -EINVAL;
	}

	retval = meson_rtc_set_calibration(dev, calibration);

	return retval ? retval : count;
}

static ssize_t rtc_calibration_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int  retval = 0;
	u32  calibration = 0;

	retval = meson_rtc_get_calibration(dev, &calibration);
	if (retval < 0) {
		dev_err(dev, "Failed to read RTC calibration attribute\n");
		sprintf(buf, "0\n");
		return retval;
	}

	return sprintf(buf, "0x%x\n", calibration);
}

static DEVICE_ATTR_RW(rtc_calibration);

static struct attribute *meson_rtc_attrs[] = {
	&dev_attr_rtc_calibration.attr,
	NULL
};

static const struct attribute_group meson_rtc_sysfs_files = {
	.attrs	= meson_rtc_attrs,
};
#endif

static const struct rtc_class_ops aml_meson_rtc_ops = {
	.read_time = meson_rtc_read_time,
	.set_time = meson_rtc_set_time,
	.read_alarm = meson_rtc_read_alarm,
	.set_alarm = meson_rtc_set_alarm,
	.alarm_irq_enable = meson_rtc_alarm_enable,
};

static void meson_rtc_init(struct device *dev, struct meson_rtc_data *rtc)
{
	u32 reg_val;
	u32 rtc_enable;
	u32 rtc_time;

	reg_val = readl(rtc->reg_base + RTC_CTRL);
	rtc_enable = ((reg_val >> RTC_ENABLE_BIT) & 1);

	dev_dbg(dev, "%s: rtc enable = %u\n", __func__, rtc_enable);
	/* If rtc is not enable, enable and init it */
	if (!rtc_enable) {
		meson_rtc_clk_config(rtc);
		/* Enable RTC */
		reg_val |= (1 << RTC_ENABLE_BIT);
		writel(reg_val, rtc->reg_base + RTC_CTRL);
		usleep_range(100, 200);
	}

	/* Temporarily use: restore rtc time when reboot, prevent rtc time being */
	/* reset by reboot, it can be removed if rtc can preserve time in reboot */
	/* procedure */
	rtc->find_mboxes = false;
	if (of_find_property(dev->of_node, "mboxes", NULL))
		rtc->find_mboxes = true;
	if (rtc->find_mboxes) {
		if (mbox_message_send_ao_sync(dev, SCPI_CMD_GET_RTC,
				NULL, 0, &rtc_time, sizeof(rtc_time), 0) < 0) {
			dev_dbg(dev, "%s: mailbox get rtc time failed!\n", __func__);
			rtc_time = 0;
		} else {
			meson_set_time(rtc, rtc_time);
			dev_dbg(dev, "%s: Successfully restored rtc time %u\n",
				__func__, rtc_time);
		}
	} else {
		dev_dbg(dev, "%s: It has not mboxes node found in dts!\n",
			__func__);
	}

	/* Mask alarm0 irq */
	reg_val = readl(rtc->reg_base + RTC_INT_MASK);
	reg_val |= (1 << RTC_ALRM0_IRQ_MSK_BIT);
	writel(reg_val, rtc->reg_base + RTC_INT_MASK);

	rtc->alarm_enabled = false;
	/* Show rtc time */
	reg_val = readl(rtc->reg_base + RTC_REAL_TIME);
	dev_dbg(dev, "%s: rtc time stamp = %u\n", __func__, reg_val);
}

static int meson_rtc_probe(struct platform_device *pdev)
{
	struct meson_rtc_data *rtc;
	struct device_node *node = pdev->dev.of_node;
	int ret;
	u32 freq = 0;

	if (!node) {
		dev_err(&pdev->dev,
			"meson-rtc requires of_node!\n");
		return -EINVAL;
	}

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;

	rtc->reg_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rtc->reg_base)) {
		pr_err("resource ioremap failed\n");
		return PTR_ERR(rtc->reg_base);
	}

	rtc->clock = of_clk_get_by_name(pdev->dev.of_node, "rtc_clk");
	if (IS_ERR_OR_NULL(rtc->clock)) {
		pr_err("invalid rtc clock\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "osc_freq", &freq);
	if (!ret) {
		if (freq != OSC_24MHZ && freq != OSC_32KHZ) {
			pr_err("%s: Invalid clock configuration\n",
				__func__);
			return -EINVAL;
		}
		dev_dbg(&pdev->dev, "%s: Select %dHz oscillator as rtc source clock\n",
			__func__, freq);
		rtc->freq = freq;
	} else {
		pr_err("%s: Get rtc clock frequency failed!\n", __func__);
		return -EINVAL;
	}

	clk_set_rate(rtc->clock, rtc->freq);
	clk_prepare_enable(rtc->clock);

	meson_rtc_init(&pdev->dev, rtc);

	device_init_wakeup(&pdev->dev, 1);

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc_dev))
		return PTR_ERR(rtc->rtc_dev);

	ret = devm_request_irq(&pdev->dev, rtc->irq, meson_rtc_handler,
			       IRQF_ONESHOT, "meson-rtc alarm", rtc);
	if (ret) {
		dev_err(&pdev->dev, "IRQ%d request failed, ret = %d\n",
			rtc->irq, ret);
		return ret;
	}

	rtc->rtc_dev->ops = &aml_meson_rtc_ops;

#ifdef RTC_CALIBRATION
	ret = rtc_add_group(rtc->rtc_dev, &meson_rtc_sysfs_files);
	if (ret)
		return ret;
#endif

	ret = rtc_register_device(rtc->rtc_dev);
	if (ret)
		return ret;

	dev_dbg(&pdev->dev, "%s: probe done\n", __func__);
	return 0;
}

static int meson_rtc_suspend(struct device *dev)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);
	u32 reg_val;

	dev_dbg(dev, "%s\n", __func__);

	if (device_may_wakeup(dev)) {
		enable_irq_wake(rtc->irq);
		/* Unmask alarm0 irq */
		reg_val = readl(rtc->reg_base + RTC_INT_MASK);
		reg_val &= (~(1 << RTC_ALRM0_IRQ_MSK_BIT));
		writel(reg_val, rtc->reg_base + RTC_INT_MASK);
	}

	return 0;
}

static int meson_rtc_resume(struct device *dev)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		disable_irq_wake(rtc->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(meson_rtc_pm_ops,
			 meson_rtc_suspend, meson_rtc_resume);

static void meson_rtc_shutdown(struct platform_device *pdev)
{
	struct meson_rtc_data *rtc = dev_get_drvdata(&pdev->dev);
	u32 rtc_time;
	u32 reg_val;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	/* Temporarily use: store rtc time when reboot, prevent rtc time being   */
	/* reset by reboot, it can be removed if rtc can preserve time in reboot */
	/* procedure */
	rtc_time = meson_read_time(rtc);
	if (rtc->find_mboxes) {
		mbox_message_send_ao_sync(&pdev->dev, SCPI_CMD_SET_RTC,
					  &rtc_time, sizeof(rtc_time), NULL, 0, 0);
		dev_dbg(&pdev->dev, "%s: mailbox store rtc time success!\n",
			__func__);
	} else {
		dev_dbg(&pdev->dev, "%s: mailbox store rtc time failed!\n",
			__func__);
	}

	if (device_may_wakeup(&pdev->dev)) {
		enable_irq_wake(rtc->irq);
		/* Unmask alarm0 irq */
		reg_val = readl(rtc->reg_base + RTC_INT_MASK);
		reg_val &= (~(1 << RTC_ALRM0_IRQ_MSK_BIT));
		writel(reg_val, rtc->reg_base + RTC_INT_MASK);
	}
}

static const struct of_device_id meson_rtc_dt_match[] = {
	{ .compatible = "amlogic,meson-rtc"},
	{}
};
MODULE_DEVICE_TABLE(of, meson_rtc_dt_match);

static struct platform_driver meson_rtc_driver = {
	.probe = meson_rtc_probe,
	.driver = {
		.name = "meson-rtc",
		.of_match_table = meson_rtc_dt_match,
		.pm = &meson_rtc_pm_ops,
	},
	.shutdown = meson_rtc_shutdown,
};

module_platform_driver(meson_rtc_driver);

MODULE_DESCRIPTION("Amlogic RTC driver");
MODULE_LICENSE("GPL");
