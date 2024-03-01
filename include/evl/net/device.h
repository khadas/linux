/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_DEVICE_H
#define _EVL_NET_DEVICE_H

#ifdef CONFIG_EVL_NET

#include <linux/if_vlan.h>
#include <uapi/evl/net/device-abi.h>
#include <uapi/evl/net/bpf-abi.h>

struct evl_socket;
struct evl_netdev_activation;
struct notifier_block;
struct sk_buff;

int evl_net_switch_oob_port(struct evl_socket *esk,
			    struct evl_netdev_activation *act);

int evl_netdev_event(struct notifier_block *ev_block,
		     unsigned long event, void *ptr);

struct net_device *
evl_net_get_dev_by_index(struct net *net, int ifindex);

struct net_device *
evl_net_get_dev_by_name(struct net *net, const char *name);

void evl_net_get_dev(struct net_device *dev);

void evl_net_put_dev(struct net_device *dev);

void evl_net_wake_rx(struct net_device *dev);

int evl_net_dev_allocfd(struct net *net, const char *devname);

enum evl_net_rx_action
__evl_net_filter_rx(struct evl_netdev_state *est, struct sk_buff *skb);

static inline enum evl_net_rx_action
evl_net_filter_rx(struct net_device *dev, struct sk_buff *skb)
{
	struct evl_netdev_state *est = dev->oob_state.estate;

	if (test_bit(EVL_NETDEV_RXFILTER_BIT, &est->flags))
		return __evl_net_filter_rx(est, skb);

	return EVL_RX_VLAN;
}

static inline struct net_device *evl_net_real_dev(struct net_device *dev)
{
	if (is_vlan_dev(dev))
		return vlan_dev_real_dev(dev);

	return dev;
}

#endif

#endif /* !_EVL_NET_DEVICE_H */
