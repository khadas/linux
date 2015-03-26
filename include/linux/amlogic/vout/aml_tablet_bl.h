#ifndef __INCLUDE_AML_TABLET_BL_H
#define __INCLUDE_AML_TABLET_BL_H



#define BL_LEVEL_DEFAULT	BL_LEVEL_MID
#define BL_NAME			"backlight"


#define	DRV_BL_FLAG		0
#define LCD_BL_FLAG		1

#define BL_LEVEL_MAX			255
#define BL_LEVEL_MIN			10
#define BL_LEVEL_OFF			1

#define BL_LEVEL_MID			128
#define BL_LEVEL_MID_MAPPED		102

#define BL_GPIO_OUTPUT_LOW	0
#define BL_GPIO_OUTPUT_HIGH	1
#define BL_GPIO_INPUT	2

#define	bl_gpio_request(dev, gpio)	gpiod_get(dev, gpio)
#define bl_gpio_free(gpio)	gpiod_free(gpio, BL_NAME)
#define bl_gpio_input(gpio)	gpiod_direction_input(gpio)
#define bl_gpio_output(gpio, val) gpiod_direction_output(gpio, val)
#define bl_gpio_get_value(gpio)		gpiod_get_value(gpio)
#define bl_gpio_set_value(gpio, val)	gpiod_set_value(gpio, val)



extern void bl_power_on(int bl_flag);
extern void bl_power_off(int bl_flag);
extern unsigned get_backlight_level(void);

#endif

