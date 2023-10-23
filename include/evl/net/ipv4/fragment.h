/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IPV4_FRAGMENT_H
#define _EVL_NET_IPV4_FRAGMENT_H

struct sk_buff;

struct sk_buff *evl_ipv4_defrag(struct sk_buff *skb);

#endif /* !_EVL_NET_IPV4_FRAGMENT_H */
