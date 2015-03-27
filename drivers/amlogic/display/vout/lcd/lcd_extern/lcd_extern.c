#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/vout/lcd_extern.h>

/* #define LCD_EXT_DEBUG_INFO */
#ifdef LCD_EXT_DEBUG_INFO
#define DBG_PRINT(...)		pr_info(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

static struct aml_lcd_extern_driver_s lcd_ext_driver = {
	.type = LCD_EXTERN_MAX,
	.name = NULL,
	.reg_read = NULL,
	.reg_write = NULL,
	.power_on = NULL,
	.power_off = NULL,
	.init_on_cmd_8 = NULL,
	.init_off_cmd_8 = NULL,
};

struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(void)
{
	return &lcd_ext_driver;
}

enum Bool_check_e lcd_extern_driver_check(void)
{
	struct aml_lcd_extern_driver_s *lcd_ext;

	lcd_ext = aml_lcd_extern_get_driver();
	if (lcd_ext) {
		if (lcd_ext->type < LCD_EXTERN_MAX) {
			pr_info("[warning]: lcd_extern has already exist ");
			pr_info("(%s)\n", lcd_ext->name);
			return FALSE;
		}
	} else {
		pr_info("get lcd_extern_driver failed\n");
	}

	return TRUE;
}

int get_lcd_extern_dt_data(struct platform_device *pdev,
		struct lcd_extern_config_s *pconf)
{
	int err;
	const char *str;
	struct gpio_desc *gdesc;
	struct device_node *of_node;

	of_node = pdev->dev.of_node;
	err = of_property_read_string(of_node,
		"dev_name", (const char **)&pconf->name);
	if (err) {
		pconf->name = "aml_lcd_extern";
		pr_info("warning: get dev_name failed\n");
	}
	pr_info("load lcd_extern in dtb: %s\n", pconf->name);

	err = of_property_read_u32(of_node, "type", &pconf->type);
	if (err) {
		pconf->type = LCD_EXTERN_MAX;
		pr_info("warning: get type failed, exit\n");
		return -1;
	}
	switch (pconf->type) {
	case LCD_EXTERN_I2C:
		err = of_property_read_u32(of_node,
			"i2c_address", &pconf->i2c_addr);
		if (err) {
			pr_info("%s warning: get i2c_address failed\n",
				pconf->name);
			pconf->i2c_addr = 0;
		}
		DBG_PRINT("%s: i2c_address=0x%02x\n",
			pconf->name, pconf->i2c_addr);

		err = of_property_read_string(of_node, "i2c_bus", &str);
		if (err) {
			pr_info("%s warning: ", pconf->name);
			pr_info("get i2c_bus failed, use default i2c bus\n");
			pconf->i2c_bus = AML_I2C_MASTER_A;
		} else {
			if (strncmp(str, "i2c_bus_a", 9) == 0)
				pconf->i2c_bus = AML_I2C_MASTER_A;
			else if (strncmp(str, "i2c_bus_b", 9) == 0)
				pconf->i2c_bus = AML_I2C_MASTER_B;
			else if (strncmp(str, "i2c_bus_c", 9) == 0)
				pconf->i2c_bus = AML_I2C_MASTER_C;
			else if (strncmp(str, "i2c_bus_d", 9) == 0)
				pconf->i2c_bus = AML_I2C_MASTER_D;
			else if (strncmp(str, "i2c_bus_ao", 10) == 0)
				pconf->i2c_bus = AML_I2C_MASTER_AO;
			else
				pconf->i2c_bus = AML_I2C_MASTER_A;
		}
		DBG_PRINT("%s: i2c_bus=%s[%d]\n",
			pconf->name, str, pconf->i2c_bus);
		break;
	case LCD_EXTERN_SPI:
		err = of_property_read_string(of_node, "gpio_spi_cs", &str);
		if (err) {
			pr_info("%s warning: get spi gpio_spi_cs failed\n",
				pconf->name);
			pconf->spi_cs = NULL;
		} else {
			gdesc = lcd_extern_gpio_request(&pdev->dev, str);
			if (IS_ERR(gdesc)) {
				pr_info("failed to alloc lcd extern ");
				pr_info("gpio (%s)\n", str);
				pconf->spi_cs = NULL;
			} else {
				pconf->spi_cs = gdesc;
				DBG_PRINT("spi_cs gpio = %s\n", str);
			}
		}
		err = of_property_read_string(of_node, "gpio_spi_clk", &str);
		if (err) {
			pr_info("%s warning: get spi gpio_spi_clk failed\n",
				pconf->name);
			pconf->spi_clk = NULL;
		} else {
			gdesc = lcd_extern_gpio_request(&pdev->dev, str);
			if (IS_ERR(gdesc)) {
				pr_info("failed to alloc lcd extern ");
				pr_info("gpio (%s)\n", str);
				pconf->spi_clk = NULL;
			} else {
				pconf->spi_clk = gdesc;
				DBG_PRINT("spi_clk gpio = %s\n", str);
			}
		}
		err = of_property_read_string(of_node, "gpio_spi_data", &str);
		if (err) {
			pr_info("%s warning: get spi gpio_spi_data failed\n",
				pconf->name);
			pconf->spi_data = NULL;
		} else {
			gdesc = lcd_extern_gpio_request(&pdev->dev, str);
			if (IS_ERR(gdesc)) {
				pr_info("failed to alloc lcd extern ");
				pr_info("gpio (%s)\n", str);
				pconf->spi_data = NULL;
			} else {
				pconf->spi_data = gdesc;
				DBG_PRINT("spi_data gpio = %s\n", str);
			}
		}
		break;
	case LCD_EXTERN_MIPI:
		break;
	default:
		break;
	}

	return 0;
}

