/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <evl/net/socket.h>
#include <evl/net/output.h>

/**
 *	evl_net_ether_transmit - pass an ethernet packet down to the
 *	hardware
 *
 *	All ethernet traffic needs to be channeled through a VLAN, the
 *	associated network interface is called an oob port. We are
 *	called from a protocol handler running in oob context, hard
 *	irqs on.  skb is not linked to any queue.
 */
int evl_net_ether_transmit(struct net_device *dev, struct sk_buff *skb)
{
	__be16 vlan_proto;
	u16 vlan_tci;

	if (EVL_WARN_ON(NET, !is_vlan_dev(dev)))
		return -EINVAL;

	vlan_proto = vlan_dev_vlan_proto(dev);
	vlan_tci = vlan_dev_vlan_id(dev);
	vlan_tci |= vlan_dev_get_egress_qos_mask(dev, skb->priority);
	__vlan_hwaccel_put_tag(skb, vlan_proto, vlan_tci);

	return evl_net_transmit(skb);
}
