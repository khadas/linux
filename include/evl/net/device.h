/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_DEVICE_H
#define _EVL_NET_DEVICE_H

#ifdef CONFIG_EVL_NET

struct evl_socket;
struct evl_netdev_activation;
struct notifier_block;

int evl_net_switch_oob_port(struct evl_socket *esk,
			    struct evl_netdev_activation *act);

int evl_netdev_event(struct notifier_block *ev_block,
		     unsigned long event, void *ptr);

struct net_device *
evl_net_get_dev_by_index(struct net *net, int ifindex);

void evl_net_get_dev(struct net_device *dev);

void evl_net_put_dev(struct net_device *dev);

#endif

#endif /* !_EVL_NET_DEVICE_H */
