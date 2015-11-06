/* linux/drivers/usb/phy/phy-aml-usb.c
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/amlogic/usb-gxbbtv.h>
#include <linux/amlogic/iomap.h>
/*#include <mach/am_regs.h>*/
#include "phy-aml-usb.h"

int amlogic_usbphy_reset(void)
{
	static int	init_count;
	int i = 0;

	if (!init_count) {
		init_count++;
		aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		for (i = 0; i < 1000; i++)
			udelay(500);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_usbphy_reset);

