/* linux/drivers/usb/phy/phy-aml-usb.h
 *
 */

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-gxl.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb, phy)

extern int amlogic_new_usbphy_reset(void);
