/* linux/drivers/usb/phy/phy-aml-usb.h
 *
 */

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-gxbbtv.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb, phy)

extern int amlogic_usbphy_reset(void);
