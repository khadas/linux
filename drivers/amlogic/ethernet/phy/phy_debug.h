/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _STMMAC_MESON_PHY_DEBUG_H
#define _STMMAC_MESON_PHY_DEBUG_H

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/pinctrl/consumer.h>
#include <linux/net_tstamp.h>

//#include "eth_reg.h"
/*add this to stop checking wol,which will reset phy*/
//extern unsigned int enable_wol_check;
//extern unsigned int tx_amp_bl2;
//extern unsigned int enet_type;
//extern void __iomem *ioaddr_dbg;
#ifdef CONFIG_REALTEK_PHY
extern unsigned int support_gpio_wol;
#endif
int gmac_create_sysfs(struct phy_device *phydev, void __iomem *ioaddr);
int gmac_remove_sysfs(struct phy_device *phydev);
#endif
