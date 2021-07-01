// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb_v2, phy)

int amlogic_new_usbphy_reset_v2(struct amlogic_usb_v2 *phy);
int amlogic_new_usbphy_reset_phycfg_v2
	(struct amlogic_usb_v2 *phy, int cnt);
int amlogic_crg_drd_usbphy_reset(struct amlogic_usb_v2 *phy);
int amlogic_crg_drd_usbphy_reset_phycfg
	(struct amlogic_usb_v2 *phy, int cnt);
void crg_exit(void);
int crg_init(void);
void crg_gadget_exit(void);
int crg_gadget_init(void);
#ifdef CONFIG_AMLOGIC_USB
int crg_otg_write_UDC(const char *udc_name);
#endif

