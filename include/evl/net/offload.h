/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_OFFLOAD_H
#define _EVL_NET_OFFLOAD_H

#include <linux/list.h>
#include <linux/uio.h>
#include <net/ip.h>

struct evl_net_offload {
	struct kvec kvec;
	size_t count;
	union {
		struct sockaddr_in in;
	} dest;
	int destlen;
	struct list_head next;
};

#endif /* !_EVL_NET_OFFLOAD_H */
