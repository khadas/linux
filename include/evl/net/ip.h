/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IP_H
#define _EVL_NET_IP_H

#include <evl/net/socket.h>

static inline void evl_net_init_ip_socket(struct evl_socket *esk)
{
	INIT_LIST_HEAD(&esk->u.ip.pending_output);
}

#endif /* !_EVL_NET_IP_H */
