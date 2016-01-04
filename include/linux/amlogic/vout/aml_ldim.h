
#ifndef _AML_LDIM_H_
#define _AML_LDIM_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>

struct ld_config_s {
	unsigned int dim_min;
	unsigned int dim_max;
};

/*******global API******/
struct aml_ld_driver_s {
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
};

extern struct aml_ld_driver_s *aml_ld_get_driver(void);
#endif

