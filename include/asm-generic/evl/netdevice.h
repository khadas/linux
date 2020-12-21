/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_NETDEVICE_H
#define _ASM_GENERIC_EVL_NETDEVICE_H

#ifdef CONFIG_EVL

#include <linux/list.h>
#include <evl/wait.h>
#include <evl/poll.h>
#include <evl/flag.h>
#include <evl/crossing.h>

struct evl_net_qdisc;
struct evl_kthread;

struct evl_net_skb_queue {
	struct list_head queue;
	hard_spinlock_t lock;
};

struct evl_netdev_state {
	/* Buffer pool management */
	struct list_head free_skb_pool;
	size_t pool_free;
	size_t pool_max;
	size_t buf_size;
	struct evl_wait_queue pool_wait;
	struct evl_poll_head poll_head;
	/* RX handling */
	struct evl_kthread *rx_handler;
	struct evl_flag rx_flag;
	struct evl_net_skb_queue rx_queue;
	/* TX handling */
	struct evl_net_qdisc *qdisc;
	struct evl_kthread *tx_handler;
	struct evl_flag tx_flag;
	/* Count of oob ports referring to this device. */
	int refs;
};

struct oob_netdev_state {
	struct evl_netdev_state *estate;
	struct evl_crossing crossing;
	struct list_head next;
};

#else

struct oob_netdev_state {
};

#endif	/* !CONFIG_EVL */

#endif /* !_ASM_GENERIC_EVL_NETDEVICE_H */
