// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/amlogic/leds_state.h>

static ssize_t blink_off_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	u32 id, times, high_ms, low_ms;
	int res;

	res = sscanf(buf, "%d %d %d %d", &id, &times, &high_ms, &low_ms);
	if (res != 4) {
		pr_err("%s Can't parse! usage:[id times high(ms) low(ms)]\n",
		       DRIVER_NAME);
		return -EINVAL;
	}

	res = meson_led_state_set_blink_off(id, times, high_ms, low_ms, 0, 0);
	if (res) {
		pr_err("%s set blink off fail!\n", DRIVER_NAME);
		return res;
	}

	return size;
}

static DEVICE_ATTR_WO(blink_off);

static ssize_t blink_on_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	u32 id, times, high_ms, low_ms;
	int res;

	res = sscanf(buf, "%d %d %d %d", &id, &times, &high_ms, &low_ms);
	if (res != 4) {
		pr_err("%s Can't parse! usage:[id times high(ms) low(ms)]\n",
		       DRIVER_NAME);
		return -EINVAL;
	}

	res = meson_led_state_set_blink_on(id, times, high_ms, low_ms, 0, 0);
	if (res) {
		pr_err("%s set blink on fail!\n", DRIVER_NAME);
		return res;
	}

	return size;
}

static DEVICE_ATTR_WO(blink_on);

static ssize_t breath_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	u32 id, breath;
	int res;

	res = sscanf(buf, "%d %d", &id, &breath);
	if (res != 2) {
		pr_err("%s Can't parse id and breath,usage:[id breath]\n",
		       DRIVER_NAME);
		return -EINVAL;
	}

	res = meson_led_state_set_breath(id, breath);
	if (res) {
		pr_err("%s set breath fail!\n", DRIVER_NAME);
		return res;
	}

	return size;
}

static DEVICE_ATTR_WO(breath);

static ssize_t state_brightness_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	/* TODO: */
	return 0;
}

static ssize_t state_brightness_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	u32 id, brightness;
	int res;

	res = sscanf(buf, "%d %d", &id, &brightness);
	if (res != 2) {
		pr_err("%s Can't parse! usage: [id brightness]\n",
		       DRIVER_NAME);
		return -EINVAL;
	}

	res = meson_led_state_set_brightness(id, brightness);
	if (res) {
		pr_err("%s set brightness fail!\n", DRIVER_NAME);
		return res;
	}

	return size;
}

static DEVICE_ATTR_RW(state_brightness);

int meson_led_state_set_brightness(u32 led_id, u32 brightness)
{
	u32 data[3];
	int ret, count;

	if (brightness > LED_STATE_FULL) {
		pr_err("%s %s brightness setting out of range!\n",
		       DRIVER_NAME, __func__);
		return -EINVAL;
	}

	data[0] = led_id;
	data[1] = LED_STATE_BRIGHTNESS;
	data[2] = brightness;

	for (count = 0; count < MESON_LEDS_SCPI_CNT; count++) {
		ret = scpi_send_data((void *)data, sizeof(data), SCPI_AOCPU,
				     SCPI_CMD_LEDS_STATE, NULL, 0);
		if (ret == 0)
			break;
		mdelay(5);
	}

	if (count == MESON_LEDS_SCPI_CNT) {
		pr_err("%s %s Can't set led brightness count=%d\n",
		       DRIVER_NAME, __func__, count);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(meson_led_state_set_brightness);

int meson_led_state_set_breath(u32 led_id, u32 breath_id)
{
	u32 data[3];
	int ret, count;

	if (breath_id > MESON_LEDS_BREATH_MAX_COUNT) {
		pr_err("%s %s Parameter setting out of range!\n",
		       DRIVER_NAME, __func__);
		return -EINVAL;
	}

	data[0] = led_id;
	data[1] = LED_STATE_BREATH;
	data[2] = breath_id;

	for (count = 0; count < MESON_LEDS_SCPI_CNT; count++) {
		ret = scpi_send_data((void *)data, sizeof(data), SCPI_AOCPU,
				     SCPI_CMD_LEDS_STATE, NULL, 0);
		if (ret == 0)
			break;
		mdelay(5);
	}

	if (count == MESON_LEDS_SCPI_CNT) {
		pr_err("%s %s Can't set led breath count=%d\n", DRIVER_NAME,
		       __func__, count);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(meson_led_state_set_breath);

/*to do:Five and six parameters are extended parameters*/
int meson_led_state_set_blink_on(u32 led_id, u32 blink_times,
				     u32 blink_high, u32 blink_low,
				     u32 brightness_high,
				     u32 brightness_low)
{
	u32 data[5];
	int ret, count;

	if (blink_times > MESON_LEDS_MAX_BLINK_CNT ||
	    blink_high > MESON_LEDS_MAX_HIGH_MS ||
	    blink_low > MESON_LEDS_MAX_LOW_MS) {
		pr_err("%s %s Parameter setting out of range!\n",
		       DRIVER_NAME, __func__);
		return -EINVAL;
	}

	/* TODO: brightness_high brightness_low no ready! */
	if (brightness_high || brightness_low)
		pr_info("brightness high and low is no ready!\n");

	data[0] = led_id;
	data[1] = LED_STATE_BLINK_ON;
	data[2] = blink_times;
	data[3] = blink_high;
	data[4] = blink_low;

	for (count = 0; count < MESON_LEDS_SCPI_CNT; count++) {
		ret = scpi_send_data((void *)data, sizeof(data),
				     SCPI_AOCPU, SCPI_CMD_LEDS_STATE,
				     NULL, 0);
		if (ret == 0)
			break;
		mdelay(5);
	}

	if (count == MESON_LEDS_SCPI_CNT) {
		pr_err("%s %s Can't set led blink on count=%d\n",
		       DRIVER_NAME, __func__, count);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(meson_led_state_set_blink_on);

int meson_led_state_set_blink_off(u32 led_id, u32 blink_times,
				      u32 blink_high, u32 blink_low,
				      u32 brightness_high,
				      u32 brightness_low)
{
	u32 data[5];
	int ret, count;

	if (blink_times > MESON_LEDS_MAX_BLINK_CNT ||
	    blink_high > MESON_LEDS_MAX_HIGH_MS ||
	    blink_low > MESON_LEDS_MAX_LOW_MS) {
		pr_err("%s %s Parameter setting out of range!\n",
		       DRIVER_NAME, __func__);
		return -EINVAL;
	}

	/* TODO: brightness_high brightness_low no ready! */
	if (brightness_high || brightness_low)
		pr_info("brightness high and low is no ready!\n");

	data[0] = led_id;
	data[1] = LED_STATE_BLINK_OFF;
	data[2] = blink_times;
	data[3] = blink_high;
	data[4] = blink_low;

	for (count = 0; count < MESON_LEDS_SCPI_CNT; count++) {
		ret = scpi_send_data((void *)data, sizeof(data), SCPI_AOCPU,
				     SCPI_CMD_LEDS_STATE, NULL, 0);
		if (ret == 0)
			break;
		mdelay(5);
	}

	if (count == MESON_LEDS_SCPI_CNT) {
		pr_err("%s %s Can't set led blink off count=%d\n",
		       DRIVER_NAME, __func__, count);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(meson_led_state_set_blink_off);

void meson_led_device_create(struct led_classdev *led_cdev)
{
	int rc;

	rc = device_create_file(led_cdev->dev, &dev_attr_state_brightness);
	rc = device_create_file(led_cdev->dev, &dev_attr_breath);
	rc = device_create_file(led_cdev->dev, &dev_attr_blink_on);
	rc = device_create_file(led_cdev->dev, &dev_attr_blink_off);
	if (rc)
		goto err_out_led_state;

	return;

err_out_led_state:
	device_remove_file(led_cdev->dev, &dev_attr_state_brightness);
	device_remove_file(led_cdev->dev, &dev_attr_breath);
	device_remove_file(led_cdev->dev, &dev_attr_blink_on);
	device_remove_file(led_cdev->dev, &dev_attr_blink_off);
}

static int meson_led_state_probe(struct platform_device *pdev)
{
	struct led_state_data *data;
	int ret;

	pr_info("%s led state probe start!\n", DRIVER_NAME);
	data = devm_kzalloc(&pdev->dev, sizeof(struct led_state_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->cdev.name = pdev->dev.of_node->name;
	pr_info("pdev->dev.of_node->name=%s\n", pdev->dev.of_node->name);
	ret = led_classdev_register(&pdev->dev, &data->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register for %s\n",
			data->cdev.name);
		goto err;
	}

	/*should after led_classdev_register,
	 *the stateled dir should be created first
	 */
	meson_led_device_create(&data->cdev);
	platform_set_drvdata(pdev, data);
	pr_info("%s led state probe over!\n", DRIVER_NAME);

	return 0;

err:
	led_classdev_unregister(&data->cdev);

	return ret;
}

static void meson_led_state_shutdown(struct platform_device *pdev)
{
}

#ifdef CONFIG_PM
static int meson_led_state_suspend(struct platform_device *pdev,
				   pm_message_t state)
{
	return 0;
}

static int meson_led_state_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

static int meson_led_state_remove(struct platform_device *pdev)
{
	struct led_state_data *data = platform_get_drvdata(pdev);
	struct led_classdev *led_cdev = &data->cdev;

	device_remove_file(led_cdev->dev, &dev_attr_state_brightness);
	device_remove_file(led_cdev->dev, &dev_attr_breath);
	device_remove_file(led_cdev->dev, &dev_attr_blink_on);
	device_remove_file(led_cdev->dev, &dev_attr_blink_off);
	led_classdev_unregister(&data->cdev);

	return 0;
}

static const struct of_device_id of_led_state_match[] = {
	{ .compatible = "amlogic,state-led-aocpu", },
	{},
};
MODULE_DEVICE_TABLE(led, of_led_state_match);

static struct platform_driver led_state_driver = {
	.probe		= meson_led_state_probe,
	.remove		= meson_led_state_remove,
	.shutdown   = meson_led_state_shutdown,
#ifdef	CONFIG_PM
	.suspend	= meson_led_state_suspend,
	.resume		= meson_led_state_resume,
#endif
	.driver		= {
		.name	= "led_state",
		.owner	= THIS_MODULE,
		.of_match_table = of_led_state_match,
	},
};

module_platform_driver(led_state_driver);

MODULE_AUTHOR("Bichao Zheng <bichao.zheng@amlogic.com>");
MODULE_DESCRIPTION("LED STATE driver for amlogic");
MODULE_LICENSE("GPL");
