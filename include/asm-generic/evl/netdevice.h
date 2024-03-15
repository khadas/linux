/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_NETDEVICE_H
#define _ASM_GENERIC_EVL_NETDEVICE_H

#ifdef CONFIG_EVL_NET

#include <linux/list.h>
#include <linux/rcupdate.h>
#include <net/page_pool/types.h>
#include <evl/wait.h>
#include <evl/poll.h>
#include <evl/flag.h>
#include <evl/stax.h>
#include <evl/crossing.h>

struct evl_net_qdisc;
struct evl_kthread;
struct bpf_prog;

struct evl_net_skb_queue {
	struct list_head queue;
	hard_spinlock_t lock;
};

struct evl_net_ebpf_filter {
	struct rcu_head rcu;
	struct bpf_prog *prog;
};

#define EVL_NETDEV_POLL_SCHED    0
#define EVL_NETDEV_RXFILTER_BIT  1

struct evl_netdev_state {
	/* TX page pool (premapped if device is oob-capable). */
	struct page_pool *tx_pages;
	struct evl_wait_queue tx_wait;
	size_t pool_max;
	size_t buf_size;
	struct evl_poll_head poll_head;
	/* RX handling */
	struct evl_kthread *rx_handler;
	struct evl_flag rx_flag;
	struct list_head rx_poll; /* NAPI instances to poll (oob) */
	hard_spinlock_t rx_lock; /* Serializes accesses to rx_poll */
	struct evl_net_skb_queue rx_packets; /* Ingress packets to process (oob) */
	/* TX handling */
	struct evl_net_qdisc *qdisc;
	struct evl_kthread *tx_handler;
	struct evl_flag tx_flag;
	/* RX filter/redirector */
	spinlock_t filter_lock;
	struct evl_net_ebpf_filter __rcu *rx_filter;
	/* Runtime state flags. */
	unsigned long flags;
	/* Count of oob ports referring to this device. */
	int refs;
};

struct oob_netdev_state {
	struct evl_netdev_state *estate;
	struct evl_crossing crossing;
	struct list_head next;
};

struct oob_netqueue_state {
	struct evl_stax tx_lock;	/* inband vs oob exclusion lock */
};

static inline void netqueue_init_oob(struct oob_netqueue_state *qs)
{
	evl_init_stax(&qs->tx_lock, EVL_STAX_INBAND_SPIN);
}

static inline void netqueue_destroy_oob(struct oob_netqueue_state *qs)
{
	evl_destroy_stax(&qs->tx_lock);
}

#else  /* !CONFIG_EVL_NET */

struct oob_netdev_state {
};

struct oob_netqueue_state {
};

static inline void netqueue_init_oob(struct oob_netqueue_state *qs)
{
}

static inline void netqueue_destroy_oob(struct oob_netqueue_state *qs)
{
}

#endif	/* !CONFIG_EVL_NET */

#endif /* !_ASM_GENERIC_EVL_NETDEVICE_H */
