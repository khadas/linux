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
#include <linux/refcount.h>
#include <linux/skbuff.h>
#include <evl/wait.h>
#include <evl/file.h>
#include <evl/poll.h>
#include <evl/crossing.h>
#include <evl/work.h>
#include <uapi/evl/types.h>
#include <uapi/evl/fcntl.h>
#include <uapi/evl/net/socket-abi.h>

struct evl_socket;
struct net;
struct net_device;
struct evl_net_offload;
struct evl_net_udp_receiver;

struct evl_net_proto {
	int (*attach)(struct evl_socket *esk,
		struct evl_net_proto *proto, int protocol);
	void (*release)(struct evl_socket *esk);
	void (*destroy)(struct evl_socket *esk);
	int (*bind)(struct evl_socket *esk,
		struct sockaddr *addr, int len);
	int (*connect)(struct evl_socket *esk,
		struct sockaddr *addr, int len, int flags);
	int (*shutdown)(struct evl_socket *esk, int how);
	int (*ioctl)(struct evl_socket *esk, unsigned int cmd,
		unsigned long arg);
	ssize_t (*oob_send)(struct evl_socket *esk,
			const struct user_oob_msghdr __user *u_msghdr,
			struct iovec *iov,
			size_t iovlen);
	ssize_t (*oob_receive)(struct evl_socket *esk,
			struct user_oob_msghdr __user *u_msghdr,
			struct iovec *iov,
			size_t iovlen);
	__poll_t (*oob_poll)(struct evl_socket *esk,
			struct oob_poll_wait *wait);
	struct net_device *(*get_netif)(struct evl_socket *esk);
	void (*handle_offload)(struct evl_socket *esk);
};

struct evl_socket_domain {
	int af_domain;
	struct evl_net_proto *(*match)(int type, int protocol);
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
	int protocol;
	refcount_t refs;	/* release vs destroy */
	struct evl_work inband_offload;
	union {
		/* Packet interface data. */
		struct {
			int real_ifindex;
			int ifindex; /* Same as real_ifindex or vlan ifindex */
			u16 vlan_id; /* non-zero if vlan device, zero otherwise */
			u32 proto_hash;
		} packet;
		/* Used by all IP protocols we support. */
		struct {
			/* Offload descriptors. */
			struct list_head pending_output;
			/* UDP bindings. */
			union {
				struct {
					u32 rcv_addr;
					u16 rcv_port;
					struct evl_net_udp_receiver *receiver;
				} udp;
			};
		} ip;
	} u;
	hard_spinlock_t oob_lock;
};

static inline unsigned int evl_socket_f_flags(struct evl_socket *esk)
{
	return esk->efile.filp->f_flags;
}

static inline bool evl_charge_socket_rmem(struct evl_socket *esk,
					  size_t size)
{
	/* An overflow of skb->truesize - 1 is allowed, not more. */
	if (atomic_read(&esk->rmem_count) >= esk->rmem_max)
		return false;

	/* We don't have to saturate, atomic_t is fine. */
	atomic_add(size, &esk->rmem_count);

	return true;
}

static inline void evl_uncharge_socket_rmem(struct evl_socket *esk,
					    size_t size)
{
	int count = atomic_sub_return(size, &esk->rmem_count);
	EVL_WARN_ON(NET, count < 0);
}

int evl_charge_socket_wmem(struct evl_socket *esk, size_t size,
			ktime_t timeout, enum evl_tmode tmode);

void evl_uncharge_socket_wmem(struct evl_socket *esk, size_t size);

int evl_register_socket_domain(struct evl_socket_domain *domain);

void evl_unregister_socket_domain(struct evl_socket_domain *domain);

void evl_net_offload_inband(struct evl_socket *esk,
			struct evl_net_offload *ofld,
			struct list_head *q);

#endif /* !_EVL_NET_SOCKET_H */
