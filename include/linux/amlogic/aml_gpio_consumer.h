#include <linux/gpio.h>
#ifndef __AML_GPIO_CONSUMER_H__
#define __AML_GPIO_CONSUMER_H__
extern int amlogic_gpio_request(unsigned int  pin, const char *label);
extern int amlogic_gpio_request_one(unsigned pin,
		unsigned long flags, const char *label);
extern int amlogic_gpio_request_array(const struct gpio *array, size_t num);
extern int amlogic_gpio_free_array(const struct gpio *array, size_t num);
extern int amlogic_gpio_direction_input(unsigned int pin, const char *owner);
extern int amlogic_gpio_direction_output(unsigned int pin,
		int value, const char *owner);
extern int amlogic_gpio_free(unsigned int  pin, const char *label);
extern int amlogic_request_gpio_to_irq(unsigned int  pin,
		const char *label, unsigned int flag);
extern int amlogic_gpio_to_irq(unsigned int  pin,
		const char *owner, unsigned int flag);
extern int amlogic_get_value(unsigned int pin, const char *owner);
extern int amlogic_set_value(unsigned int pin, int value, const char *owner);
extern int amlogic_gpio_name_map_num(const char *name);
extern int amlogic_set_pull_up_down(unsigned int pin,
		unsigned int val, const char *owner);
extern int amlogic_disable_pullup(unsigned int pin, const char *owner);
#define AML_GPIO_IRQ(irq_bank, filter, type) \
((irq_bank&0x7)|(filter&0x7)<<8|(type&0x3)<<16)

enum {
	GPIO_IRQ0 = 0,
	GPIO_IRQ1,
	GPIO_IRQ2,
	GPIO_IRQ3,
	GPIO_IRQ4,
	GPIO_IRQ5,
	GPIO_IRQ6,
	GPIO_IRQ7,
};

enum {
	GPIO_IRQ_HIGH = 0,
	GPIO_IRQ_LOW,
	GPIO_IRQ_RISING,
	GPIO_IRQ_FALLING,
};

enum {
	FILTER_NUM0 = 0,
	FILTER_NUM1,
	FILTER_NUM2,
	FILTER_NUM3,
	FILTER_NUM4,
	FILTER_NUM5,
	FILTER_NUM6,
	FILTER_NUM7,
};

#endif
