#include <linux/amlogic/saradc.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/printk.h>

#define NOT_WRITE_EFUSE		0x0
#define EFUEE_PRIVATE		0x4
#define EFUSE_OPS		0xa

static int chip_trimmed = -1;

int thermal_firmware_init(void)
{
	int ret;

	pr_info("%s\n", __func__);
	ret = scpi_get_sensor("aml_thermal");
	pr_info("%s: chip trimming %s\n", __func__, ret < 0 ? "false" : "true");
	chip_trimmed = ret < 0 ? 0 : 1;
	return ret;
}
EXPORT_SYMBOL(thermal_firmware_init);

int get_cpu_temp(void)
{
	unsigned int val = 0;

	if (chip_trimmed) {		/* only supported trimmed chips */
		if (scpi_get_sensor_value(0, &val) < 0)
			return -1;
		return val;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(get_cpu_temp);
