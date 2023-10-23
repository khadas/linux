/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_OUTPUT_H
#define _EVL_NET_OUTPUT_H

#include <evl/net/ether/output.h>

struct sk_buff;

void evl_net_do_tx(void *arg);

int evl_net_transmit(struct sk_buff *skb);

void evl_net_init_tx(void);

#endif /* !_EVL_NET_OUTPUT_H */
