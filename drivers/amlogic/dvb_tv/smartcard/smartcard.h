#ifndef _AML_SMC_H_
#define _AML_SMC_H_
extern int amlogic_gpio_name_map_num(const char *name);
extern int amlogic_gpio_direction_output(unsigned int pin, int value,
	const char *owner);
extern int amlogic_gpio_request(unsigned int pin, const char *label);
extern unsigned long get_mpeg_clk(void);
#endif
