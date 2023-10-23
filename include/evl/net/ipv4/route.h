/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IPV4_ROUTE_H
#define _EVL_NET_IPV4_ROUTE_H

#include <evl/net/route.h>

struct sk_buff;
struct net;
struct net_device;
struct flowi4;
struct rtable;

int evl_net_init_ipv4_routing(struct net *net);

void evl_net_cleanup_ipv4_routing(struct net *net);

void evl_net_learn_ipv4_route(struct net *net,
			struct flowi4 *fl4, struct rtable *rt);

void evl_net_flush_ipv4_routes(struct net *net, struct net_device *dev);

struct evl_net_route *evl_net_get_ipv4_route(struct net *net, __be32 daddr);

#endif /* !_EVL_NET_IPV4_ROUTE_H */
