/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-gxl.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb, phy)

int amlogic_new_usbphy_reset(struct amlogic_usb *phy);
