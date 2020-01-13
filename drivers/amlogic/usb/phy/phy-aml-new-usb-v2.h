// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb_v2, phy)

extern int amlogic_new_usbphy_reset_v2(struct amlogic_usb_v2 *phy);
extern int amlogic_new_usbphy_reset_phycfg_v2
	(struct amlogic_usb_v2 *phy, int cnt);

