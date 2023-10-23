/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_ETHER_OUTPUT_H
#define _EVL_NET_ETHER_OUTPUT_H

struct sk_buff;
struct net_device;

int evl_net_ether_transmit_raw(struct net_device *dev,
			struct sk_buff *skb);

int evl_net_ether_transmit(struct net_device *dev, struct sk_buff *skb,
			const void *hw_dst);

#endif /* !_EVL_NET_ETHER_OUTPUT_H */
