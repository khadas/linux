/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_OUTPUT_H
#define _EVL_NET_OUTPUT_H

struct sk_buff;
struct evl_socket;
struct net_device;

void evl_net_do_tx(void *arg);

int evl_net_transmit(struct sk_buff *skb);

void evl_net_init_tx(void);

int evl_net_ether_transmit(struct net_device *dev,
			   struct sk_buff *skb);

#endif /* !_EVL_NET_OUTPUT_H */
