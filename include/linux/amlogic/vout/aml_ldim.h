
#ifndef _AML_LDIM_H_
#define _AML_LDIM_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>

struct ld_config_s {
	unsigned int dim_min;
	unsigned int dim_max;
	unsigned char cmd_size;
};

/*******global API******/
struct aml_ldim_driver_s {
	struct ld_config_s *ld_config;
	unsigned short *ldim_matrix_2_spi;
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	int (*pinmux_ctrl)(int status);
	int (*device_power_on)(void);
	int (*device_power_off)(void);
	int (*device_bri_update)(unsigned short *buf, unsigned char len);
	struct device *dev;
	struct pinctrl *pin;
};

extern struct aml_ldim_driver_s *aml_ldim_get_driver(void);
#endif

