/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2024 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_NET_BPF_ABI_H
#define _EVL_UAPI_NET_BPF_ABI_H

enum evl_net_rx_action {
	EVL_RX_DROP = 0,	/* Discard */
	EVL_RX_ACCEPT,		/* Pass to oob stack */
	EVL_RX_SKIP,		/* Leave to inband stack */
	EVL_RX_VLAN,		/* Apply VLAN-based rules */
};

#endif /* !_EVL_UAPI_NET_BPF_ABI_H */
