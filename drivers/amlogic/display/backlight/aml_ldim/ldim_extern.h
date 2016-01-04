
#ifndef _LDIM_EXTERN_H_
#define _LDIM_EXTERN_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>

/*#define LDIM_EXT_DEBUG_INFO*/
#define LDIMPR(fmt, args...)     pr_info("ldim: "fmt"", ## args)
#define LDIMERR(fmt, args...)    pr_info("ldim: error: "fmt"", ## args)

#endif

