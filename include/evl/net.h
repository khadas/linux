/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_H
#define _EVL_NET_H

#ifdef CONFIG_EVL_NET

#include <linux/net.h>
#include <net/sock.h>
#include <evl/net/skb.h>
#include <evl/net/input.h>
#include <uapi/evl/net/net-abi.h>

int evl_net_init(void);

void evl_net_cleanup(void);

extern const struct net_proto_family evl_family_ops;

extern struct proto evl_af_oob_proto;

#else

static inline int evl_net_init(void)
{
	return 0;
}

static inline void evl_net_cleanup(void)
{ }

#endif

#endif /* !_EVL_NET_H */
