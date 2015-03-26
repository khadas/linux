#include <linux/amlogic/wifi_dt.h>

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>

#define OWNER_NAME "sdio_wifi"

#define WIFI_INFO(fmt, args...) \
		pr_info("[%s] " fmt, __func__, ##args);

int wifi_power_gpio = 0;
int wifi_power_gpio2 = 0;

struct wifi_plat_info {
	int interrupt_pin;
	int irq_num;
	int irq_trigger_type;

	int power_on_pin;
	int power_on_pin_level;
	int power_on_pin2;

	int clock_32k_pin;

	int plat_info_valid;
};

static struct wifi_plat_info wifi_info;

#ifdef CONFIG_OF
static const struct of_device_id wifi_match[] = {
	{	.compatible = "amlogic, aml_broadcm_wifi",
		.data		= (void *)&wifi_info
	},
	{},
};

static struct wifi_plat_info *wifi_get_driver_data
	(struct platform_device *pdev)
{
	const struct of_device_id *match;
	match = of_match_node(wifi_match, pdev->dev.of_node);
	return (struct wifi_plat_info *)match->data;
}
#else
#define wifi_match NULL
#endif

#define CHECK_PROP(ret, msg, value)	\
{	\
	if (ret) { \
		WIFI_INFO("wifi_dt : no prop for %s\n", msg);	\
		return -1;	\
	} else {	\
		WIFI_INFO("wifi_dt : %s=%s\n", msg, value);	\
	}	\
}	\

/*
#define CHECK_RET(ret) \
	if (ret) \
		WIFI_INFO("wifi_dt : gpio op failed(%d)
			at line %d\n", ret, __LINE__)
*/

/* extern const char *amlogic_cat_gpio_owner(unsigned int pin); */

#define SHOW_PIN_OWN(pin_str, pin_num)	\
	WIFI_INFO("%s(%d)\n", pin_str, pin_num)

static int set_wifi_power(int value)
{
	return gpio_direction_output(wifi_info.power_on_pin,
				value);
}

static int set_wifi_power2(int value)
{
	return gpio_direction_output(wifi_info.power_on_pin2,
				value);
}

static int wifi_dev_probe(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_OF
	struct wifi_plat_info *plat;
	const char *value;
	struct gpio_desc *desc;
#else
	struct wifi_plat_info *plat =
	 (struct wifi_plat_info *)(pdev->dev.platform_data);
#endif

	WIFI_INFO("wifi_dev_probe\n");

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		plat = wifi_get_driver_data(pdev);
		plat->plat_info_valid = 0;

		ret = of_property_read_string(pdev->dev.of_node,
			"interrupt_pin", &value);
		CHECK_PROP(ret, "interrupt_pin", value);
		desc = of_get_named_gpiod_flags(pdev->dev.of_node,
			"interrupt_pin", 0, NULL);
		plat->interrupt_pin = desc_to_gpio(desc);

		/* amlogic_gpio_name_map_num(value); */

		plat->irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);
		/*
		ret = of_property_read_u32(pdev->dev.of_node,
			"irq_num", &plat->irq_num);
		*/
		CHECK_PROP(ret, "irq_num", "null");


		ret = of_property_read_string(pdev->dev.of_node,
		"irq_trigger_type", &value);
		CHECK_PROP(ret, "irq_trigger_type", value);
		if (strcmp(value, "GPIO_IRQ_HIGH") == 0)
			plat->irq_trigger_type = GPIO_IRQ_HIGH;
		else if (strcmp(value, "GPIO_IRQ_LOW") == 0)
			plat->irq_trigger_type = GPIO_IRQ_LOW;
		else if (strcmp(value, "GPIO_IRQ_RISING") == 0)
			plat->irq_trigger_type = GPIO_IRQ_RISING;
		else if (strcmp(value, "GPIO_IRQ_FALLING") == 0)
			plat->irq_trigger_type = GPIO_IRQ_FALLING;
		else {
			WIFI_INFO("wifi_dt:unknown irq trigger type-%s\n",
			 value);
			return -1;
		}


		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin", &value);
		if (!ret) {
			CHECK_PROP(ret, "power_on_pin", value);
			wifi_power_gpio = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
			"power_on_pin", 0, NULL);
			plat->power_on_pin = desc_to_gpio(desc);
			/* amlogic_gpio_name_map_num(value); */
		}

		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_level", &plat->power_on_pin_level);

		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin2", &value);
		if (!ret) {
			CHECK_PROP(ret, "power_on_pin2", value);
			wifi_power_gpio2 = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"power_on_pin2", 0, NULL);
			plat->power_on_pin2 = desc_to_gpio(desc);
			/* amlogic_gpio_name_map_num(value); */
		}

		ret = of_property_read_string(pdev->dev.of_node,
			"clock_32k_pin", &value);
		/* CHECK_PROP(ret, "clock_32k_pin", value); */
		if (ret)
			plat->clock_32k_pin = 0;
		else
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"clock_32k_pin", 0, NULL);
			plat->clock_32k_pin = desc_to_gpio(desc);

			/* amlogic_gpio_name_map_num(value); */

		plat->plat_info_valid = 1;

		WIFI_INFO("interrupt_pin=%d\n", plat->interrupt_pin);
		WIFI_INFO("irq_num=%d, irq_trigger_type=%d\n",
			plat->irq_num, plat->irq_trigger_type);
		WIFI_INFO("power_on_pin=%d\n", plat->power_on_pin);
		WIFI_INFO("clock_32k_pin=%d\n", plat->clock_32k_pin);
	}
#endif

	return 0;
}

static int wifi_dev_remove(struct platform_device *pdev)
{
	WIFI_INFO("wifi_dev_remove\n");
	return 0;
}

static struct platform_driver wifi_plat_driver = {
	.probe = wifi_dev_probe,
	.remove = wifi_dev_remove,
	.driver = {
	.name = "aml_broadcm_wifi",
	.owner = THIS_MODULE,
	.of_match_table = wifi_match
	},
};

static int __init wifi_dt_init(void)
{
	int ret;
	ret = platform_driver_register(&wifi_plat_driver);
	return ret;
}
/* module_init(wifi_dt_init); */
fs_initcall_sync(wifi_dt_init);

static void __exit wifi_dt_exit(void)
{
	platform_driver_unregister(&wifi_plat_driver);
}
module_exit(wifi_dt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");

#ifdef CONFIG_OF

int wifi_setup_dt(void)
{
	int ret;
	uint flag;

	WIFI_INFO("wifi_setup_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_setup_dt : invalid device tree setting\n");
		return -1;
	}

	/* setup 32k clock */
	wifi_request_32k_clk(1, OWNER_NAME);

/*
#if ((!(defined CONFIG_ARCH_MESON8))
	&& (!(defined CONFIG_ARCH_MESON8B)))
	//setup sdio pullup
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG4,
		0xf|1<<8|1<<9|1<<11|1<<12);
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG2,1<<7|1<<8|1<<9);
#endif
*/

	/* setup irq */
	ret = gpio_request(wifi_info.interrupt_pin,
		OWNER_NAME);
	if (ret)
		WIFI_INFO("interrupt_pin request failed(%d) n", ret);
	ret = gpio_set_pullup(wifi_info.interrupt_pin, 1);
	if (ret)
		WIFI_INFO("interrupt_pin disable pullup failed(%d) n", ret)
	ret = gpio_direction_input(wifi_info.interrupt_pin);
	if (ret)
		WIFI_INFO("set interrupt_pin input failed(%d) n", ret);
	if (wifi_info.irq_num) {
		flag = AML_GPIO_IRQ(wifi_info.irq_num,
			FILTER_NUM4, wifi_info.irq_trigger_type);
	} else {
		WIFI_INFO("wifi_dt : unsupported irq number - %d\n",
			wifi_info.irq_num);
		return -1;
	}
	ret = gpio_for_irq(wifi_info.interrupt_pin, flag);
	if (ret)
		WIFI_INFO("gpio to irq failed(%d) n", ret)
	SHOW_PIN_OWN("interrupt_pin", wifi_info.interrupt_pin);

	/* setup power */
	if (wifi_power_gpio) {
		ret = gpio_request(wifi_info.power_on_pin, OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin request failed(%d) n", ret);
		if (wifi_info.power_on_pin_level)
			ret = set_wifi_power(1);
		else
			ret = set_wifi_power(0);
		if (ret)
			WIFI_INFO("power_on_pin output failed(%d) n", ret);
		SHOW_PIN_OWN("power_on_pin", wifi_info.power_on_pin);
	}

	if (wifi_power_gpio2) {
		ret = gpio_request(wifi_info.power_on_pin2,
			OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin2 request failed(%d) n", ret);
		ret = set_wifi_power2(0);
		if (ret)
			WIFI_INFO("power_on_pin2 output failed(%d) n", ret);
		SHOW_PIN_OWN("power_on_pin2", wifi_info.power_on_pin2);
	}

	return 0;
}
EXPORT_SYMBOL(wifi_setup_dt);

void wifi_teardown_dt(void)
{

	WIFI_INFO("wifi_teardown_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_teardown_dt : invalid device tree setting\n");
		return;
	}

	if (wifi_power_gpio)
		gpio_free(wifi_info.power_on_pin);

	if (wifi_power_gpio2)
		gpio_free(wifi_info.power_on_pin2);

	gpio_free(wifi_info.interrupt_pin);

	wifi_request_32k_clk(0, OWNER_NAME);
}
EXPORT_SYMBOL(wifi_teardown_dt);

static int clk_32k_on;

void wifi_request_32k_clk(int is_on, const char *requestor)
{
	int ret;
	unsigned int pinmux_value = 0;

	if (!wifi_info.clock_32k_pin) {
		WIFI_INFO("wifi_request_32k_clk : no 32k pin");
		return;
	}
	WIFI_INFO("wifi_request_32k_clk : %s-->%s for %s\n",
		clk_32k_on > 0 ? "ON" : "OFF",
			is_on ? "ON" : "OFF", requestor);

	if (is_on) {
		if (clk_32k_on == 0) {
			ret =
			gpio_request(wifi_info.clock_32k_pin,
				OWNER_NAME);
			if (ret)
				WIFI_INFO("32K_pin request failed(%d) n", ret)
			gpio_direction_output(wifi_info.clock_32k_pin,
				0);
			SHOW_PIN_OWN("clock_32k_pin", wifi_info.clock_32k_pin);
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
				pinmux_value = aml_read_cbus(0x202f);
				aml_write_cbus(0x202f,
					pinmux_value | (0x1<<22));
				/* 202f:PERIPHS_PIN_MUX_3*/
				/* set mode GPIOX_10-->CLK_OUT3 */
			}
			udelay(200);
		}
		++clk_32k_on;
	} else {
		--clk_32k_on;
	if (clk_32k_on < 0)
		clk_32k_on = 0;
		if (clk_32k_on == 0) {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
				pinmux_value = aml_read_cbus(0x202f);
				aml_write_cbus(0x202f,
					pinmux_value & (~(0x1<<22)));
			}
			gpio_free(wifi_info.clock_32k_pin);
		}
	}
}
EXPORT_SYMBOL(wifi_request_32k_clk);

void extern_wifi_set_enable(int is_on)
{
	int ret = 0;

	if (is_on) {
		if (wifi_power_gpio) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power(0);
			else
				ret = set_wifi_power(1);

			if (ret)
				WIFI_INFO("wifi power failed(%d) n", ret)
		}
		if (wifi_power_gpio2) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power2(0);
			else
				ret = set_wifi_power2(1);
			if (ret)
				WIFI_INFO("wifi power2 failed(%d) n", ret)
		}
		WIFI_INFO("WIFI  Enable! %d\n", wifi_info.power_on_pin);
	} else {
		if (wifi_power_gpio) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power(1);
			else
				ret = set_wifi_power(0);

			if (ret)
				WIFI_INFO("wifi power down failed(%d) n", ret)
		}
		if (wifi_power_gpio2) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power2(1);
			else
				ret = set_wifi_power2(0);
			if (ret)
				WIFI_INFO("wifi power2 down  failed(%d) n", ret)
		}
		WIFI_INFO("WIFI  Disable! %d\n", wifi_info.power_on_pin);

	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

int wifi_irq_num(void)
{
	return wifi_info.irq_num;
}
EXPORT_SYMBOL(wifi_irq_num);
#else

int wifi_setup_dt(void)
{
	return 0;
}
EXPORT_SYMBOL(wifi_setup_dt);

void wifi_teardown_dt(void)
{
}
EXPORT_SYMBOL(wifi_teardown_dt);

void wifi_request_32k_clk(int is_on, const char *requestor)
{
}
EXPORT_SYMBOL(wifi_request_32k_clk);

#endif
