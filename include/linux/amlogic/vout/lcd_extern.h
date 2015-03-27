
#ifndef __AMLOGIC_LCD_EXTERN_H_
#define __AMLOGIC_LCD_EXTERN_H_

#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/vout/lcdoutc.h>

enum Lcd_Extern_Type_e {
	LCD_EXTERN_I2C = 0,
	LCD_EXTERN_SPI,
	LCD_EXTERN_MIPI,
	LCD_EXTERN_MAX,
};

/* global API */
struct aml_lcd_extern_driver_s {
	char *name;
	enum Lcd_Extern_Type_e type;
	int (*reg_read)(unsigned char reg, unsigned char *buf);
	int (*reg_write)(unsigned char reg, unsigned char value);
	int (*power_on)(void);
	int (*power_off)(void);
	unsigned char *init_on_cmd_8;
	unsigned char *init_off_cmd_8;
	/* unsigned short *init_on_cmd_16; */
	/* unsigned short *init_off_cmd_16; */
};

struct lcd_extern_config_s {
	char *name;
	enum Lcd_Extern_Type_e type;
	int status;
	int i2c_addr;
	int i2c_bus;
	struct gpio_desc *spi_cs;
	struct gpio_desc *spi_clk;
	struct gpio_desc *spi_data;
};

#define lcd_extern_gpio_request(dev, str)     gpiod_get(dev, str)
#define lcd_extern_gpio_free(gdesc)           gpiod_put(gdesc)
#define lcd_extern_gpio_input(gdesc)          gpiod_direction_input(gdesc)
#define lcd_extern_gpio_output(gdesc, val)    gpiod_direction_output(gdesc, val)
#define lcd_extern_gpio_get_value(gdesc)      gpiod_get_value(gdesc)
#define lcd_extern_gpio_set_value(gdesc, val) gpiod_set_value(gdesc, val)

extern struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(void);
extern enum Bool_check_e lcd_extern_driver_check(void);
extern int get_lcd_extern_dt_data(struct platform_device *pdev,
			struct lcd_extern_config_s *pdata);

#endif

