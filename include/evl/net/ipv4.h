/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IPV4_H
#define _EVL_NET_IPV4_H

#include <linux/list.h>
#include <linux/inet.h>
#include <evl/net/socket.h>
#include <evl/net/ip.h>

struct sk_buff;

struct evl_net_ipv4_cookie {
	__be32 saddr;		/* Source IP */
	__be32 daddr;		/* Destination IP */
	__u8 protocol;		/* Internet protocol identifier  */
	int transhdrlen;	/* Transport header length */
};

int evl_net_ipv4_deliver(struct sk_buff *skb);

void __evl_net_ipv4_gc(struct evl_net_frag_tdir *ftdir);

static inline void evl_net_ipv4_gc(struct net *net)
{
	struct evl_net_frag_tdir *ftdir = &net->oob.ipv4.ftdir;

	if (!hlist_empty(&ftdir->gc.queue))
		__evl_net_ipv4_gc(ftdir);
}

int evl_net_init_ipv4(struct net *net);

void evl_net_cleanup_ipv4(struct net *net);

extern struct evl_socket_domain evl_net_ipv4;

#endif /* !_EVL_NET_IPV4_H */
