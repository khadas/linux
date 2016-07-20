#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/sensor/sensor_common.h>

#include <linux/irq.h>
#include <linux/of_irq.h>
#define AML_I2C_BUS_AO 0
#define AML_I2C_BUS_A 1
#define AML_I2C_BUS_B 2
#define AML_I2C_BUS_C 3
#define AML_I2C_BUS_D 4
#define AML_I2C_BUS_NO (-1)
#define MAX_SENSOR_ONBOARD  6

static struct sensor_pdata_t sensor_pdata[MAX_SENSOR_ONBOARD];
static int curr_idx;

void aml_sensor_report_acc(struct i2c_client *client, struct input_dev *dev,
	int x, int y, int z)
{
	int i;
	for (i = 0; i < curr_idx; i++) {
		if (sensor_pdata[i].client && sensor_pdata[i].client ==
			client) {
			if (sensor_pdata[i].acc_swap_xy) {
				int temp = x;
				x = y;
				y = temp;
			}
			x *= sensor_pdata[i].acc_negate_x ? -1 : 1;
			y *= sensor_pdata[i].acc_negate_y ? -1 : 1;
			z *= sensor_pdata[i].acc_negate_z ? -1 : 1;

			input_report_abs(dev, ABS_X, x);
			input_report_abs(dev, ABS_Y, y);
			input_report_abs(dev, ABS_Z, z);
			input_sync(dev);
			return;
		}
	}
}

void aml_sensor_report_mag(struct i2c_client *client, struct input_dev *dev,
	int x, int y, int z)
{
	int i;
	for (i = 0; i < curr_idx; i++) {
		if (sensor_pdata[i].client && sensor_pdata[i].client ==
			client) {

			if (sensor_pdata[i].mag_swap_xy) {
				int temp = x;
				x = y;
				y = temp;
			}
			x *= sensor_pdata[i].mag_negate_x ? -1 : 1;
			y *= sensor_pdata[i].mag_negate_y ? -1 : 1;
			z *= sensor_pdata[i].mag_negate_z ? -1 : 1;

			input_report_abs(dev, ABS_X, x);
			input_report_abs(dev, ABS_Y, y);
			input_report_abs(dev, ABS_Z, z);
			input_sync(dev);
			return;
		}
	}
}

void aml_sensor_report_gyr(struct i2c_client *client, struct input_dev *dev,
	int x, int y, int z)
{
	int i;
	for (i = 0; i < curr_idx; i++) {
		if (sensor_pdata[i].client && sensor_pdata[i].client ==
			client) {
			if (sensor_pdata[i].gyr_swap_xy) {
				int temp = x;
				x = y;
				y = temp;
			}
			x *= sensor_pdata[i].gyr_negate_x ? -1 : 1;
			y *= sensor_pdata[i].gyr_negate_y ? -1 : 1;
			z *= sensor_pdata[i].gyr_negate_z ? -1 : 1;

			input_report_abs(dev, ABS_X, x);
			input_report_abs(dev, ABS_Y, y);
			input_report_abs(dev, ABS_Z, z);
			input_sync(dev);
			return;
		}
	}
}

int dt_sensor_setup_i2c_dev(struct device_node *node,  struct i2c_board_info
		*i2c_info, int *i2c_bus_nr, int *gpio)
{
	int ret = -1;
	int irq , interrupt_pin , irq_trigger_type;
	uint flag = 0;
	u32 addr, gpio_irq;
	u32 acc_dir, mag_dir, gyr_dir;
	const char *value;
	struct gpio_desc *desc;
	ret = of_property_read_string(node, "status", &value);
	if (ret < 0) {
		pr_info("%s: Failed to read status from device tree for dev",
			__func__);
		pr_info("%s\n", i2c_info->type);
		return ret;
	}
	if (strncmp("ok", value, 2) == 0) {

		ret = of_property_read_string(node, "dev_name", &value);
		if (ret < 0) {
			pr_info("%s: Failed to read dev_name from devicetree\n",
				__func__);
			return ret;
		}

		strncpy(i2c_info->type, value, I2C_NAME_SIZE);

		ret = of_property_read_u32(node, "address", &addr);
		if (ret < 0) {
			pr_info("%s: faild to get i2c address for dev %s\n",
				__func__, i2c_info->type);
			return -1;
		}

		ret = 0;

		i2c_info->addr = addr;

		ret = of_property_read_string(node, "i2c_bus", &value);
		if (ret) {
			pr_info("%s: faild to get i2c_bus str for dev %s\n",
				__func__, i2c_info->type);
			*i2c_bus_nr = AML_I2C_BUS_NO;
		} else {
			if (!strncmp(value, "i2c_bus_a", 9))
				*i2c_bus_nr = AML_I2C_BUS_A;
			else if (!strncmp(value, "i2c_bus_b", 9))
				*i2c_bus_nr = AML_I2C_BUS_B;
			else if (!strncmp(value, "i2c_bus_c", 9))
				*i2c_bus_nr = AML_I2C_BUS_C;
			else if (!strncmp(value, "i2c_bus_d", 9))
				*i2c_bus_nr = AML_I2C_BUS_D;
			else if (!strncmp(value, "i2c_bus_ao", 10))
				*i2c_bus_nr = AML_I2C_BUS_AO;
			else
				*i2c_bus_nr = AML_I2C_BUS_NO;
		}

		ret = of_property_read_u32(node, "acc_dir", &acc_dir);
		if (!ret) {
			sensor_pdata[curr_idx].acc_negate_x = acc_dir & 0x0001;
			sensor_pdata[curr_idx].acc_negate_y = acc_dir & 0x0010;
			sensor_pdata[curr_idx].acc_negate_z = acc_dir & 0x0100;
			sensor_pdata[curr_idx].acc_swap_xy = acc_dir & 0x1000;
		}

		ret = of_property_read_u32(node, "mag_dir", &mag_dir);
		if (!ret) {
			sensor_pdata[curr_idx].mag_negate_x = mag_dir & 0x0001;
			sensor_pdata[curr_idx].mag_negate_y = mag_dir & 0x0010;
			sensor_pdata[curr_idx].mag_negate_z = mag_dir & 0x0100;
			sensor_pdata[curr_idx].mag_swap_xy = mag_dir & 0x1000;
		}

		ret = of_property_read_u32(node, "gyo_dir", &gyr_dir);
		if (!ret) {
			sensor_pdata[curr_idx].gyr_negate_x = gyr_dir & 0x0001;
			sensor_pdata[curr_idx].gyr_negate_y = gyr_dir & 0x0010;
			sensor_pdata[curr_idx].gyr_negate_z = gyr_dir & 0x0100;
			sensor_pdata[curr_idx].gyr_swap_xy = gyr_dir & 0x1000;
		}

		ret = of_property_read_u32(node, "irq", &irq);
		if (ret < 0 || irq <= 0) {
			*gpio = -1;
			i2c_info->irq = 0;
		} else {
			ret = of_property_read_string(node, "gpio", &value);
			if (ret) {
				pr_info("%s:faild to get gpio str for dev\n",
					__func__);
				*gpio = -1;
				i2c_info->irq = 0;
			} else {
				i2c_info->irq = irq;
				desc = of_get_named_gpiod_flags(node,
					"gpio", 0, NULL);
				*gpio = desc_to_gpio(desc);
			}
		}
		/*******************GPIO_IRQ********************************/
		ret = of_property_read_u32(node, "gpio_irq", &gpio_irq);
		if (ret) {
			pr_err("%s:faild to get gpio str for dev,gpio_irq=%d\n",
							__func__, gpio_irq);
		} else {
			/*interrupt irq*/
			i2c_info->irq = irq_of_parse_and_map(node, 0);
			pr_info("irq_of_parse_and_map=%d\n", i2c_info->irq);
			desc = of_get_named_gpiod_flags(node, "interrupt_pin",
							 0, NULL);
			interrupt_pin = desc_to_gpio(desc);
			*gpio = interrupt_pin;
			ret = of_property_read_string(node, "irq_trigger_type",
							 &value);
			pr_info("irq_trigger_type=%s\n", value);
			if (strcmp(value, "GPIO_IRQ_HIGH") == 0)
				irq_trigger_type = GPIO_IRQ_HIGH;
			else if (strcmp(value, "GPIO_IRQ_LOW") == 0)
				irq_trigger_type = GPIO_IRQ_LOW;
			else if (strcmp(value, "GPIO_IRQ_RISING") == 0)
				irq_trigger_type = GPIO_IRQ_RISING;
			else if (strcmp(value, "GPIO_IRQ_FALLING") == 0)
				irq_trigger_type = GPIO_IRQ_FALLING;
			else {
				pr_info("wifi_dt:unknown irq trigger type%s\n",
							value);
				return ret;
			}
			/* setup GPIO interrupt 1*/
			ret = gpio_request(interrupt_pin, i2c_info->type);
			if (ret)
				pr_err("interrupt_pin request failed(%d)\n",
							ret);
			ret = gpio_set_pullup(interrupt_pin, 1);
			if (ret)
				pr_err("interrupt_pin  pullup failed(%d)\n",
							ret);
			ret = gpio_direction_input(interrupt_pin);
			if (ret)
				pr_err("interrupt_pin input	failed(%d)\n",
							ret);
			/* setup GPIO interrupt 2*/
			if (gpio_irq)
				flag = AML_GPIO_IRQ(gpio_irq , FILTER_NUM7,
							irq_trigger_type);
			else
				pr_err("wifi_dt :unsupported irq number %d\n",
								gpio_irq);
			ret = gpio_for_irq(interrupt_pin, flag);
			if (ret)
				pr_err("gpio_for_irq (%d)\n", ret);
		}
	}
	return 0;
}


static int aml_sensor_probe(struct platform_device *pdev)
{

	struct device_node *child;
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
	int i2c_bus_nr;
	int gpio;

	struct device_node *node = pdev->dev.of_node;

	pr_info("##############aml_sensor_probe start############\n");

	for_each_child_of_node(node, child) {

		memset(&i2c_info, 0, sizeof(i2c_info));

		if (!dt_sensor_setup_i2c_dev(child, &i2c_info, &i2c_bus_nr,
			&gpio)) {

			adapter = i2c_get_adapter(i2c_bus_nr);
			if (!adapter)
				return -1;

			sensor_pdata[curr_idx++].client =
				i2c_new_device(adapter, &i2c_info);
			if (sensor_pdata[curr_idx-1].client == 0) {
				pr_err("Failed to create new i2c dev %s\n",
					i2c_info.type);
			}

			if (curr_idx >= MAX_SENSOR_ONBOARD) {
				pr_err("%s: Warning!!! Not enough slots in",
					__func__);
				pr_err("sensor_pdata array\n");
				return -1;
			}
		}
	}
	return 0;
}

static int aml_sensor_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sensor_prober_dt_match[] = {
	{
		.compatible = "amlogic, aml_sensor",
	},
	{},
};

static struct platform_driver aml_sensor_prober_driver = {
	.probe		= aml_sensor_probe,
	.remove		= aml_sensor_remove,
	.driver		= {
		.name	= "aml_sensor",
		.owner	= THIS_MODULE,
		.of_match_table = sensor_prober_dt_match,
	},
};

static int __init aml_sensor_prober_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_sensor_prober_driver);
	if (ret) {
		pr_err("aml_cams_probre_driver register failed\n");
		return ret;
	}

	return ret;
}


static void __exit aml_sensor_prober_exit(void)
{
	platform_driver_unregister(&aml_sensor_prober_driver);
}

module_init(aml_sensor_prober_init);
module_exit(aml_sensor_prober_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic MEMS sensor prober driver");

