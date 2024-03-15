/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <evl/net/socket.h>
#include <evl/net/device.h>
#include <evl/net/output.h>

/**
 *	evl_net_ether_transmit_raw - pass an ethernet packet down to
 *	the hardware as is. If @dev is a VLAN device, this routine
 *	tags the outgoing packet accordingly for the device.
 *
 *	We are called from a protocol handler running in oob context,
 *	hard irqs on. @skb is not linked to any queue.
 */
int evl_net_ether_transmit_raw(struct net_device *dev, struct sk_buff *skb)
{
	__be16 vlan_proto;
	u16 vlan_tci;

	if (is_vlan_dev(dev)) {
		vlan_proto = vlan_dev_vlan_proto(dev);
		vlan_tci = vlan_dev_vlan_id(dev);
		vlan_tci |= vlan_dev_get_egress_qos_mask(dev, skb->priority);
		__vlan_insert_tag(skb, vlan_proto, vlan_tci);
	}

	netdev_dbg(dev, "transmitting %px\n", skb);

	return evl_net_transmit(skb);
}

static int ether_transmit_one(struct net_device *dev, struct sk_buff *skb,
			const void *hw_dst)
{
	struct ethhdr *eth;

	eth = skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	eth->h_proto = skb->protocol;
	ether_addr_copy(eth->h_source, evl_net_real_dev(dev)->dev_addr);
	ether_addr_copy(eth->h_dest, hw_dst);

	return evl_net_ether_transmit_raw(dev, skb);
}

/**
 *  evl_net_ether_transmit - set the l2 header of an ethernet packet
 *  before passing it down to the hardware.
 *
 *  This routine can deal with fragmented output.
 */
int evl_net_ether_transmit(struct net_device *dev, struct sk_buff *skb,
			const void *hw_dst)
{
	struct sk_buff *fskb, *nskb;
	int ret;

	ret = ether_transmit_one(dev, skb, hw_dst);
	if (ret)
		return ret;

	fskb = skb_shinfo(skb)->frag_list;
	if (fskb) {
		do {
			nskb = fskb->next;
			fskb->next = NULL;
			ret = ether_transmit_one(dev, fskb, hw_dst);
			if (ret) {
				fskb->next = nskb;
				skb_shinfo(skb)->frag_list = fskb;
				return ret;
			}
			fskb = nskb;
		} while (fskb);
		skb_shinfo(skb)->frag_list = NULL;
	}

	return 0;
}
