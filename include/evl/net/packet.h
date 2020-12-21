/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_PACKET_H
#define _EVL_NET_PACKET_H

#include <evl/net/socket.h>

bool evl_net_packet_deliver(struct sk_buff *skb);

extern struct evl_socket_domain evl_net_packet;

#endif /* !_EVL_NET_PACKET_H */
