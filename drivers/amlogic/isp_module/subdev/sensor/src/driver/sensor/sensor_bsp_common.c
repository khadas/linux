#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include "sensor_bsp_common.h"

int sensor_bp_init(sensor_bringup_t* sbp, struct device* dev)
{
	sbp->dev = dev;
	sbp->np = dev->of_node;
	sbp->vana = 0;
	sbp->vdig = 0;
	sbp->power = 0;
	sbp->reset = 0;

	pr_info("sensor bsp init\n");

	return 0;
}

int pwr_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int val)
{
	struct device_node *np = NULL;
	int ret = -1;

	np = sensor_bp->np;
	sensor_bp->vana = of_get_named_gpio(np, propname, 0);
	ret = sensor_bp->vana;

	if (ret >= 0) {
		devm_gpio_request(sensor_bp->dev, sensor_bp->vana, "POWER");
		if (gpio_is_valid(sensor_bp->vana)) {
			gpio_direction_output(sensor_bp->vana, val);
			pr_info("pwr_enable: power gpio init\n");
		} else {
			pr_err("pwr_enable: gpio %s is not valid\n", propname);
			return -1;
		}
	} else {
		pr_err("pwr_enable: get_named_gpio %s fail\n", propname);
	}

	return ret;
}

int pwr_am_disable(sensor_bringup_t *sensor_bp)
{
	if (gpio_is_valid(sensor_bp->vana)) {
		gpio_direction_output(sensor_bp->vana, 0);
		devm_gpio_free(sensor_bp->dev, sensor_bp->vana);
	} else {
		pr_err("Error invalid pwr gpio\n");
	}

	return 0;
}

int pwr_ir_cut_enable(sensor_bringup_t* sensor_bp, int propname, int val)
{
	int ret = -1;
	ret = propname;

	if (ret >= 0) {
		devm_gpio_request(sensor_bp->dev, propname, "POWER");
		if (gpio_is_valid(propname)) {
			gpio_direction_output(propname, val);
			pr_info("pwr_enable: power gpio init\n");
		} else {
			pr_err("pwr_enable: gpio %d is not valid\n", propname);
			return -1;
		}
	} else {
		pr_err("pwr_enable: get_named_gpio %d fail\n", propname);
	}
	return ret;
}

int reset_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int val)
{
	struct device_node *np = NULL;
	int ret = -1;

	np = sensor_bp->np;
	sensor_bp->reset = of_get_named_gpio(np, propname, 0);
	ret = sensor_bp->reset;

	if (ret >= 0) {
		devm_gpio_request(sensor_bp->dev, sensor_bp->reset, "RESET");
		if (gpio_is_valid(sensor_bp->reset)) {
			gpio_direction_output(sensor_bp->reset, val);
			pr_info("reset init\n");
		} else {
			pr_err("reset_enable: gpio %s is not valid\n", propname);
			return -1;
		}
	} else {
		pr_err("reset_enable: get_named_gpio %s fail\n", propname);
	}

	return ret;
}

int reset_am_disable(sensor_bringup_t* sensor_bp)
{
	if (gpio_is_valid(sensor_bp->reset)) {
		gpio_direction_output(sensor_bp->reset, 0);
		devm_gpio_free(sensor_bp->dev, sensor_bp->reset);
	} else {
		pr_err("Error invalid reset gpio\n");
	}

	return 0;
}

int clk_am_enable(sensor_bringup_t* sensor_bp, const char* propname)
{
	struct clk *clk;
	int clk_val;
	clk = devm_clk_get(sensor_bp->dev, propname);
	if (IS_ERR(clk)) {
	    pr_err("cannot get %s clk\n", propname);
	    clk = NULL;
	    return -1;
	}

	clk_prepare_enable(clk);
	clk_val = clk_get_rate(clk);
	pr_info("isp init clock is %d MHZ\n",clk_val/1000000);

	sensor_bp->mclk = clk;
	return 0;
}

int clk_am_disable(sensor_bringup_t *sensor_bp)
{
	struct clk *mclk = NULL;

	if (sensor_bp == NULL || sensor_bp->mclk == NULL) {
		pr_err("Error input param\n");
		return -EINVAL;
	}

	mclk = sensor_bp->mclk;

	clk_disable_unprepare(mclk);

	devm_clk_put(sensor_bp->dev, mclk);

	pr_info("Success disable mclk\n");

	return 0;
}