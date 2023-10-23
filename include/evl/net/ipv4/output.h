/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IPV4_OUTPUT_H
#define _EVL_NET_IPV4_OUTPUT_H

#include <linux/types.h>
#include <linux/time.h>
#include <evl/net/output.h>

struct evl_socket;
struct evl_net_route;
struct sk_buff_head;
struct iovec;

struct sk_buff *evl_net_ipv4_build_datagram(struct evl_socket *esk,
					struct iovec *iov, size_t iovlen,
					struct evl_net_route *ert,
					size_t datalen,
					ktime_t timeout,
					struct evl_net_ipv4_cookie *ipc);

#endif /* !_EVL_NET_IPV4_OUTPUT_H */
