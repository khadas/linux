// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/amlogic/aml_thermal_hw.h>
#include <linux/amlogic/scpi_protocol.h>
#include "../../thermal/thermal_core.h"
#include "../../thermal/thermal_hwmon.h"

struct aml_thermal_sensor {
	int chip_trimmed;
	struct thermal_zone_device    *tzd;
	struct platform_device *pdev;
};

static struct aml_thermal_sensor soc_sensor;

static int thermal_firmware_init(struct platform_device *pdev)
{
	struct {
		u32 status;
		short data;
	} recv_buf;
	int ret;

	if (mbox_message_send_ao_sync(&pdev->dev, SCPI_CMD_SENSOR_CAPABILITIES,
				NULL, 0, &recv_buf, sizeof(recv_buf), 0) < 0) {
		pr_err("Get calibration value fail: com error.\n");
		return -1;
	}

	ret = recv_buf.data;
	soc_sensor.chip_trimmed = !!ret;
	return ret;
}

static int get_cpu_temp(struct platform_device *pdev)
{
	struct {
		u32 status;
		int data;
	} recv_buf;

	if (soc_sensor.chip_trimmed) {	/* only supported trimmed chips */
		if (mbox_message_send_ao_sync(&pdev->dev, SCPI_CMD_SENSOR_VALUE,
				NULL, 0, &recv_buf, sizeof(recv_buf), 0) < 0) {
			return -1000;
		}

		return recv_buf.data;
	} else {
		return -1000;
	}
}

static int get_cur_temp(void *data, int *temp)
{
	int val;
	struct aml_thermal_sensor *sen_data = data;

	val = get_cpu_temp(sen_data->pdev);
	if (val == -1000)
		return -EINVAL;

	*temp = val * 1000;

	return 0;
}

static struct thermal_zone_of_device_ops aml_thermal_ops = {
	.get_temp = get_cur_temp,
};

static int aml_thermal_probe(struct platform_device *pdev)
{
	memset(&soc_sensor, 0, sizeof(struct aml_thermal_sensor));
	soc_sensor.pdev = pdev;

	if (thermal_firmware_init(pdev) < 0) {
		dev_err(&pdev->dev, "chip is not trimmed, disable thermal\n");
		return -EINVAL;
	}

	soc_sensor.tzd = thermal_zone_of_sensor_register(&pdev->dev,
							  3,
							  &soc_sensor,
							  &aml_thermal_ops);

	if (IS_ERR(soc_sensor.tzd)) {
		dev_warn(&pdev->dev, "Error registering sensor: %p\n",
			 soc_sensor.tzd);
		return PTR_ERR(soc_sensor.tzd);
	}

	return 0;
}

static int aml_thermal_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id aml_thermal_of_match[] = {
	{ .compatible = "amlogic, aml-thermal" },
	{},
};

static struct platform_driver aml_thermal_platdrv = {
	.driver = {
		.name		= "aml-thermal",
		.owner		= THIS_MODULE,
		.of_match_table = aml_thermal_of_match,
	},
	.probe	= aml_thermal_probe,
	.remove	= aml_thermal_remove,
};

static int __init aml_thermal_platdrv_init(void)
{
	return platform_driver_register(&(aml_thermal_platdrv));
}
late_initcall(aml_thermal_platdrv_init);
