/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_SOCKET_H
#define _EVL_NET_SOCKET_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/skbuff.h>
#include <evl/wait.h>
#include <evl/file.h>
#include <evl/poll.h>
#include <evl/crossing.h>
#include <uapi/evl/net/socket.h>

struct evl_socket;
struct net;
struct net_device;

struct evl_net_proto {
	int (*attach)(struct evl_socket *esk,
		struct evl_net_proto *proto, __be16 protocol);
	void (*detach)(struct evl_socket *esk);
	int (*bind)(struct evl_socket *esk,
		struct sockaddr *addr, int len);
	int (*ioctl)(struct evl_socket *esk, unsigned int cmd,
		unsigned long arg);
	ssize_t (*oob_send)(struct evl_socket *esk,
			const struct user_oob_msghdr __user *u_msghdr,
			const struct iovec *iov,
			size_t iovlen);
	ssize_t (*oob_receive)(struct evl_socket *esk,
			struct user_oob_msghdr __user *u_msghdr,
			const struct iovec *iov,
			size_t iovlen);
	__poll_t (*oob_poll)(struct evl_socket *esk,
			struct oob_poll_wait *wait);
	struct net_device *(*get_netif)(struct evl_socket *esk);
};

struct evl_socket_domain {
	int af_domain;
	struct evl_net_proto *(*match)(int type, __be16 protocol);
	struct list_head next;
};

struct evl_socket {
	struct evl_net_proto *proto;
	struct evl_file efile;
	struct mutex lock;
	struct net *net;
	struct hlist_node hash;
	struct list_head input;
	struct evl_wait_queue input_wait;
	struct evl_poll_head poll_head; /* On input queue. */
	struct list_head next_sub;	/* evl_net_rxqueue.subscribers */
	struct sock *sk;
	atomic_t rmem_count;
	int rmem_max;
	atomic_t wmem_count;
	int wmem_max;
	struct evl_wait_queue wmem_wait;
	struct evl_crossing wmem_drain;
	__be16 protocol;
	struct {
		int real_ifindex;
		int vlan_ifindex;
		u16 vlan_id;
		u32 proto_hash;
	} binding;
	hard_spinlock_t oob_lock;
};

static inline unsigned int evl_socket_f_flags(struct evl_socket *esk)
{
	return esk->efile.filp->f_flags;
}

static inline bool evl_charge_socket_rmem(struct evl_socket *esk,
					  struct sk_buff *skb)
{
	/* An overflow of skb->truesize - 1 is allowed, not more. */
	if (atomic_read(&esk->rmem_count) >= esk->rmem_max)
		return false;

	/* We don't have to saturate, atomic_t is fine. */
	atomic_add(skb->truesize, &esk->rmem_count);

	return true;
}

static inline void evl_uncharge_socket_rmem(struct evl_socket *esk,
					    struct sk_buff *skb)
{
	atomic_sub(skb->truesize, &esk->rmem_count);
}

int evl_charge_socket_wmem(struct evl_socket *esk,
			struct sk_buff *skb,
			ktime_t timeout, enum evl_tmode tmode);

void evl_uncharge_socket_wmem(struct sk_buff *skb);

int evl_register_socket_domain(struct evl_socket_domain *domain);

void evl_unregister_socket_domain(struct evl_socket_domain *domain);

ssize_t evl_export_iov(const struct iovec *iov, size_t iovlen,
		const void *data, size_t len);

ssize_t evl_import_iov(const struct iovec *iov, size_t iovlen,
		void *data, size_t len, size_t *remainder);

int evl_charge_socket_wmem(struct evl_socket *esk,
			struct sk_buff *skb,
			ktime_t timeout, enum evl_tmode tmode);

#endif /* !_EVL_NET_SOCKET_H */
