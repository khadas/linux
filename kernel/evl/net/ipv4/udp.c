/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/inetdevice.h>
#include <net/inet_sock.h>
#include <net/inet_common.h>
#include <net/udp.h>
#include <evl/memory.h>
#include <evl/uaccess.h>
#include <evl/uio.h>
#include <evl/net/offload.h>
#include <evl/net/skb.h>
#include <evl/net/device.h>
#include <evl/net/ip.h>
#include <evl/net/ipv4.h>
#include <evl/net/ipv4/output.h>
#include <evl/net/ipv4/route.h>
#include <evl/net/ipv4/arp.h>
#include <evl/net/ipv4/udp.h>

#define EVL_NET_UDP_CACHE_SHIFT  8

/* in-band. */
static int attach_udp_socket(struct evl_socket *esk,
			struct evl_net_proto *proto, int protocol)
{
	esk->proto = proto;
	esk->protocol = protocol;
	evl_net_init_ip_socket(esk);

	return 0;
}

/*
 * set_receive_slot - install/update a receive slot for the socket to
 * wait on later. We are called whenever bind() is issued on an UDP
 * socket from the inband stack: this enables our generic cache
 * mechanism to deal with this information since it supports
 * inband-only updates, inband/oob lookups.
 *
 * @esk->sk is locked on entry.
 */
static int add_receive_slot(struct evl_socket *esk) /* inband */
{
	struct evl_cache *cache = &esk->net->oob.ipv4.udp;
	struct inet_sock *inet = inet_sk(esk->sk);
	struct evl_net_udp_receiver *new, *old;
	struct evl_cache_entry *entry;
	struct __evl_net_udp_key key;
	int ret;

	/*
	 * Allocate without holding any lock to eliminate any risk of
	 * lock order inversion. In most cases (i.e. without
	 * SO_REUSEPORT), we won't find a pre-existing slot for the
	 * [addr, port] pair, so this new slot will be used.
	 */
	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	/*
	 * Binding for oob[-extended] protocols always and only
	 * happens after the inband-side binding operation was
	 * successful, so we may use the port and receive address the
	 * inband stack already parsed and checked, dealing with the
	 * hairy reuseport logic as well. Yummie.
	 */
	key.dport = inet->inet_num;
	key.daddr = inet->inet_rcv_saddr;

	new->key = key;
	INIT_LIST_HEAD(&new->queue);
	evl_init_wait(&new->wait, &evl_mono_clock, 0);
	refcount_set(&new->refs, 1);

	/* Lookup and insertion must be seen as atomic. */
	evl_lock_cache(cache);
	entry = evl_lookup_cache(cache, &key);
	if (entry) {
		/* Unlock prior to freeing the slot (see above). */
		evl_unlock_cache(cache);
		old = container_of(entry, struct evl_net_udp_receiver, entry);
		/* One user more via reuseport, account for it. */
		refcount_inc(&old->refs);
		evl_put_cache_entry(entry);
		kfree(new);
		new = old;
	} else {
		ret = evl_add_cache_entry_locked(cache, &new->entry);
		evl_unlock_cache(cache);
		if (ret) {
			kfree(new);
			return ret;
		}
	}

	WRITE_ONCE(esk->u.ip.udp.receiver, new);

	return 0;
}

/*
 * drop_receive_slot - remove a receive slot previously installed by
 * add_receive_slot(). Slots a refcounted, so that SO_REUSEPORT is
 * dealt with.
 *
 * @esk->sk is either locked on entry, or not known from anyone else
 * (i.e. zombie state).
 */
static void drop_receive_slot(struct evl_socket *esk) /* inband */
{
	struct evl_cache *cache = &esk->net->oob.ipv4.udp;
	struct evl_net_udp_receiver *e;
	struct __evl_net_udp_key key;

	e = READ_ONCE(esk->u.ip.udp.receiver);
	if (e && refcount_dec_and_test(&e->refs)) {
		key.dport = e->key.dport;
		key.daddr = e->key.daddr;
		evl_del_cache_entry(cache, &key);
		WRITE_ONCE(esk->u.ip.udp.receiver, NULL);
	}
}

/*
 * destroy_udp_socket - perform cleanup on UDP socket
 * closure. This routine runs as an RCU callback.
 *
 * NOTE: sk_common_release() already ran for esk->sk, which means the
 * socket is not hashed on any inband receive port anymore. We only
 * have to release the receive slot @esk might be referring to.
 */
static void destroy_udp_socket(struct evl_socket *esk) /* inband */
{
	drop_receive_slot(esk);
}

/*
 * @esk->sk is locked by the inband stack on entry. In addition, the
 * latter denies double bindings for AF_INET sockets, so we know for
 * sure that @esk does not reference any receive slot yet. Likewise,
 * multiple bindings to the same destination is denied by the inband
 * stack as well, so we may assume that we are always going to create
 * a new cache entry on a unique key. Unbinding happens when the
 * socket is either shut down or destroyed on the inband side, which
 * is paired with our shutdown() and destroy() handlers.
 */
static int bind_udp_socket(struct evl_socket *esk,
			struct sockaddr *addr,
			int len)
{
	return add_receive_slot(esk);
}

/*
 * @esk->sk is locked by the inband stack on entry.
 */
static int shutdown_udp_socket(struct evl_socket *esk, int how)
{
	drop_receive_slot(esk);
	return 0;
}

static ssize_t offload_send_udp(struct evl_socket *esk,
				struct kvec *kvec, size_t count,
				struct sockaddr_in *in_dest)
{
	struct evl_net_offload *ofld;

	ofld = evl_alloc(sizeof(*ofld));
	if (!ofld)
		return -ENOMEM;

	ofld->kvec = *kvec;
	ofld->count = count;
	ofld->dest.in = in_dest ? *in_dest : (struct sockaddr_in){};
	ofld->destlen = in_dest ? sizeof(*in_dest) : 0;
	evl_net_offload_inband(esk, ofld, &esk->u.ip.pending_output);

	return count;
}

/*
 * Given an IPv4 address, look into our oob route and ARP front caches
 * to find an egress path. If we cannot find a route to the next hop
 * through an oob-enabled device, or we don't know the hardware
 * address of this peer, then the caller will have to pass on the
 * datagram to the in-band stack.
 */
static bool find_egress_path(struct evl_socket *esk, __be32 daddr,
			struct evl_net_route **ertp, struct evl_net_arp_entry **earpp)
{
	struct evl_net_arp_entry *_earp;
	struct evl_net_route *_ert;
	struct net_device *dev;

	_ert = evl_net_get_ipv4_route(sock_net(esk->sk), daddr);
	if (likely(_ert)) {
		dev = _ert->rt->dst.dev;
		if (netif_oob_port(dev)) {
			_earp = evl_net_get_arp_entry(dev, daddr);
			if (likely(_earp))  {
				*ertp = _ert;
				*earpp = _earp;
				return true;
			}
		}
		evl_net_put_route(_ert);
	}

	return false;
}

/*
 * Calculate the UDP checksum, starting from the IP header up to the
 * full payload, including the UDP header. @skb contains the IP frame
 * heading a valid UDP datagram, which might be fragmented over
 * additional IP frames.
 */
static inline __wsum checksum_datagram(struct sk_buff *skb)
{
	__wsum csum = csum_partial(skb->data, skb->len, 0);
	struct sk_buff *fskb;

	skb_walk_frags(skb, fskb) {
		csum = csum_partial(fskb->data, fskb->len, csum);
	}

	return csum;
}

/*
 * Send a datagram - which might be fragmented - to the peer we have
 * an ARP entry for.
 */
static int send_datagram(struct sk_buff *skb, struct net_device *dev,
			struct evl_net_arp_entry *earp,
			struct evl_net_ipv4_cookie *ipc,
			__be16 dport, __be16 sport,
			size_t datalen)
{
	size_t ulen = datalen + sizeof(struct udphdr);
	struct udphdr *uh;
	int ret;

	/*
	 * Set up our transport header. evl_net_ipv4_build_datagram()
	 * reserved the required space in the heading skb for us.
	 */
	uh = udp_hdr(skb);
	uh->source = sport;
	uh->dest = dport;
	uh->len = htons(ulen);
	uh->check = 0; /* Caution: checksum_datagram() reads this too. */
	uh->check = csum_tcpudp_magic(ipc->saddr, ipc->daddr,
				ulen, IPPROTO_UDP, checksum_datagram(skb));
	if (uh->check == 0)
		uh->check = CSUM_MANGLED_0;

	skb->ip_summed = CHECKSUM_NONE;

	ret = evl_net_ether_transmit(dev, skb, earp->ha);
	if (ret)
		evl_net_wput_skb(skb);

	return ret;
}

/* oob */
static ssize_t send_udp(struct evl_socket *esk,
			const struct user_oob_msghdr __user *u_msghdr,
			struct iovec *iov,
			size_t iovlen)
{
	struct sockaddr_in in_addr, *u_in_addr;
	struct sock *sk = esk->sk;
	struct inet_sock *inet = inet_sk(sk);
	struct evl_net_ipv4_cookie ipc;
	struct evl_net_arp_entry *earp;
	struct evl_net_route *ert;
	struct msghdr msg = { 0 };
	struct __evl_timespec uts;
	ssize_t datalen, ret;
	enum evl_tmode tmode;
	__u32 msg_flags = 0;
	struct sk_buff *skb;
	__be32 daddr, saddr;
	__u32 namelen = 0;
	struct kvec kvec;
	ktime_t timeout;
	__u64 name_ptr;
	__be16 dport;

	ret = raw_get_user(msg_flags, &u_msghdr->flags);
	if (ret)
		return -EFAULT;

	/*
	 * Unlike BSD, we accept MSG_DONTWAIT to decline waiting on
	 * skb contention, or offloading to the in-band stage.
	 */
	if (msg_flags & ~MSG_DONTWAIT)
		return -EINVAL;

	if (evl_socket_f_flags(esk) & O_NONBLOCK)
		msg_flags |= MSG_DONTWAIT;

	ret = raw_copy_from_user(&uts, &u_msghdr->timeout, sizeof(uts));
	if (ret)
		return -EFAULT;

	timeout = msg_flags & MSG_DONTWAIT ? EVL_NONBLOCK :
		u_timespec_to_ktime(uts);
	tmode = timeout ? EVL_ABS : EVL_REL;

	ret = raw_get_user(name_ptr, &u_msghdr->name_ptr);
	if (ret)
		return -EFAULT;

	if (name_ptr) {
		ret = raw_get_user(namelen, &u_msghdr->namelen);
		if (ret)
			return -EFAULT;
		if (namelen < sizeof(in_addr))
			return -EINVAL;
		u_in_addr = evl_valptr64(name_ptr, struct sockaddr_in);
		ret = raw_copy_from_user(&in_addr, u_in_addr, sizeof(in_addr));
		if (ret)
			return -EFAULT;
		daddr = in_addr.sin_addr.s_addr;
		dport = in_addr.sin_port;
		if (!daddr || !dport)
			return -EINVAL;
		msg.msg_name = (struct sockaddr *)&in_addr;
		msg.msg_namelen = namelen;
	} else {
		if (sk->sk_state != TCP_ESTABLISHED)
			return -EDESTADDRREQ;
		daddr = inet->inet_daddr;
		dport = inet->inet_dport;
	}

	datalen = evl_iov_flat_length(iov, iovlen);
	if (datalen == 0)
		return 0;

	/* UDP datagram cannot exceed 64k. */
	if (datalen > 65535)
		return -EMSGSIZE;

	/*
	 * Try finding an oob path for the datagram based on the
	 * routing information collected into our front caches. If
	 * none, then offload the packet to the inband stack (as a
	 * result, we may receive the missing information eventually).
	 */
	if (!find_egress_path(esk, daddr, &ert, &earp)) {
		/*
		 * We charge the socket for the offloaded data
		 * although we won't consume any oob skb for
		 * transmit. This allows for contention management.
		 */
		ret = evl_charge_socket_wmem(esk, datalen, timeout, tmode);
		if (ret)
			return ret;

		ret = evl_copy_from_uio_to_kvec(iov, iovlen, datalen, &kvec);
		if (ret < 0) {
			evl_uncharge_socket_wmem(esk, datalen);
			return ret;
		}

		ret = offload_send_udp(esk, &kvec, ret, namelen ? &in_addr : NULL);
		if (ret < 0)
			return ret;
		/*
		 * EVL-specific: we had to pass on the request to the
		 * in-band stage for routing and/or MAC address
		 * resolution. So the datagram is indeed in-flight,
		 * but we cannot guarantee a bounded delay before it
		 * is written to the wire. In such a case, provided
		 * the caller asked for non-blocking I/O, return
		 * -EINPROGRESS. This is a way for the caller to
		 * detect a missing peer solicitation before the
		 * latter is sent oob data.
		 */
		return unlikely(msg_flags & MSG_DONTWAIT) ? -EINPROGRESS : 0;
	}

	/* Ok, we have an oob path for that datagram. */

	saddr = inet->inet_saddr;
	if (!saddr) {
		if (ipv4_is_multicast(daddr)) {
			saddr = inet->mc_addr;
		} else {
			rcu_read_lock();
			saddr = inet_select_addr(ert->rt->dst.dev, daddr, RT_SCOPE_LINK);
			rcu_read_unlock();
		}
	}

	ipc.saddr = saddr;
	ipc.daddr = daddr;
	ipc.protocol = IPPROTO_UDP;
	ipc.transhdrlen = sizeof(struct udphdr);
	skb = evl_net_ipv4_build_datagram(esk, iov, iovlen, ert,
					datalen, timeout, &ipc);
	if (IS_ERR_OR_NULL(skb)) {
		ret = PTR_ERR(skb);
		goto out;
	}

	ret = send_datagram(skb, ert->rt->dst.dev, earp, &ipc,
			dport, inet->inet_sport, datalen);
out:
	evl_net_put_arp_entry(earp);
	evl_net_put_route(ert);

	return ret ?: datalen;
}

static ssize_t copy_datagram_to_user(struct user_oob_msghdr __user *u_msghdr,
				const struct iovec *iov,
				size_t iovlen,
				struct sk_buff *skb)
{
	struct sockaddr_in addr, __user *u_addr;
	__u64 name_ptr, namelen;
	__u32 msg_flags = 0;
	ssize_t ret, count;
	bool short_write;

	ret = raw_get_user(name_ptr, &u_msghdr->name_ptr);
	if (ret)
		return -EFAULT;

	ret = raw_get_user(namelen, &u_msghdr->namelen);
	if (ret)
		return -EFAULT;

	if (name_ptr) {
		if (namelen != sizeof(addr)) {
			if (namelen < sizeof(addr))
				return -EINVAL;
			ret = raw_put_user(sizeof(addr), &u_msghdr->namelen);
			if (ret)
				return -EFAULT;
		}
		addr.sin_family = AF_INET;
		addr.sin_port = udp_hdr(skb)->source;
		addr.sin_addr.s_addr = ip_hdr(skb)->saddr;
		memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
		u_addr = evl_valptr64(name_ptr, struct sockaddr_in);
		ret = raw_copy_to_user(u_addr, &addr, sizeof(addr));
		if (ret)
			return -EFAULT;
	} else {
		if (namelen)
			return -EINVAL;
	}

	skb_pull_inline(skb, sizeof(struct udphdr));

	count = evl_net_skb_to_uio(iov, iovlen, skb, sizeof(struct iphdr), &short_write);
	if (short_write)
		msg_flags |= MSG_TRUNC;

	ret = raw_put_user(msg_flags, &u_msghdr->flags);

	return ret ? -EFAULT : count;
}

/* oob */
static ssize_t receive_udp(struct evl_socket *esk,
			struct user_oob_msghdr __user *u_msghdr,
			struct iovec *iov,
			size_t iovlen)
{
	struct evl_net_udp_receiver *e;
	struct __evl_timespec uts;
	enum evl_tmode tmode;
	struct sk_buff *skb;
	unsigned long flags;
	__u32 msg_flags = 0;
	ktime_t timeout;
	ssize_t ret;

	/*
	 * The cache entry may be freed only from a RCU callback, get
	 * a safe reference on it from a RCU read side to prevent
	 * stale access.
	 */
again:
	rcu_read_lock();

	e = READ_ONCE(esk->u.ip.udp.receiver);
	if (!e) {
		/*
		 * If not bound prior to calling oob_recvmsg(), force
		 * a binding to 0.0.0.0:0, which means that no receipt
		 * will ever happen for this socket, causing this call
		 * to hang indefinitely until interrupted.
		 */
		rcu_read_unlock();
		lock_sock(esk->sk);
		/* Recheck binding under lock. */
		e = READ_ONCE(esk->u.ip.udp.receiver);
		ret = e ? 0 : add_receive_slot(esk);
		release_sock(esk->sk);
		if (!ret)
			goto again;
		return ret;
	}

	evl_get_cache_entry(&e->entry);

	rcu_read_unlock();

	if (u_msghdr) {
		ret = raw_get_user(msg_flags, &u_msghdr->flags);
		if (ret) {
			ret = -EFAULT;
			goto out;
		}

		/* We only support MSG_DONTWAIT at the moment. */
		if (msg_flags & ~MSG_DONTWAIT) {
			ret = -EINVAL;
			goto out;
		}

		ret = raw_copy_from_user(&uts, &u_msghdr->timeout,
					sizeof(uts));
		if (ret) {
			ret = -EFAULT;
			goto out;
		}

		timeout = u_timespec_to_ktime(uts);
		tmode = timeout ? EVL_ABS : EVL_REL;
	} else {
		timeout = EVL_INFINITE;
		tmode = EVL_REL;
	}

	if (evl_socket_f_flags(esk) & O_NONBLOCK)
		msg_flags |= MSG_DONTWAIT;

	do {
		raw_spin_lock_irqsave(&e->wait.wchan.lock, flags);

		if (!list_empty(&e->queue)) {
			skb = list_get_entry(&e->queue, struct sk_buff, list);
			raw_spin_unlock_irqrestore(&e->wait.wchan.lock, flags);
			ret = copy_datagram_to_user(u_msghdr, iov, iovlen, skb);
			evl_net_rput_skb(skb); /* Uncharge rmem and free. */
			goto out;
		}

		if (msg_flags & MSG_DONTWAIT) {
			raw_spin_unlock_irqrestore(&e->wait.wchan.lock, flags);
			ret = -EWOULDBLOCK;
			goto out;
		}

		evl_add_wait_queue(&e->wait, timeout, tmode);
		raw_spin_unlock_irqrestore(&e->wait.wchan.lock, flags);
		ret = evl_wait_schedule(&e->wait);
	} while (!ret);
out:
	evl_put_cache_entry(&e->entry);

	return ret;
}

/* oob */
static __poll_t poll_udp(struct evl_socket *esk,
			struct oob_poll_wait *wait)
{
	return 0;
}

/* in-band */
static struct net_device *get_netif_udp(struct evl_socket *esk)
{
	int ifindex;

	ifindex = READ_ONCE(esk->sk->sk_bound_dev_if);
	if (ifindex)
		return evl_net_get_dev_by_index(esk->net, ifindex);

	return NULL;
}

/* in-band */
static void handle_udp_inband(struct evl_socket *esk)
{
	struct evl_net_offload *ofld, *n;
	struct sock *sk = esk->sk;
	unsigned long flags;
	LIST_HEAD(tmp);
	int ret;

	/* Process pending output. */

	raw_spin_lock_irqsave(&esk->oob_lock, flags);
	list_splice_init(&esk->u.ip.pending_output, &tmp);
	raw_spin_unlock_irqrestore(&esk->oob_lock, flags);

	list_for_each_entry_safe(ofld, n, &tmp, next) {
		struct msghdr msg = { 0 };
		list_del(&ofld->next);
		msg.msg_namelen = ofld->destlen;
		if (msg.msg_namelen)
			msg.msg_name = (struct sockaddr *)&ofld->dest.in;
		ret = kernel_sendmsg(sk->sk_socket, &msg,
				&ofld->kvec, 1, ofld->count);
		evl_free(ofld->kvec.iov_base);
		evl_uncharge_socket_wmem(esk, ofld->count);
		evl_free(ofld);
	}
}

static inline bool __validate_checksum(struct sk_buff *skb, u16 ulen, __sum16 check)
{
	__sum16 sum;

	/*
	 * If the hw computed the checksum, combine and check with the
	 * pseudo-header.
	 */
	if (skb->ip_summed == CHECKSUM_COMPLETE) {
		if (!csum_tcpudp_magic(ip_hdr(skb)->saddr, ip_hdr(skb)->daddr,
					ulen, IPPROTO_UDP, skb->csum)) {
			skb->csum_valid = 1;
			return true;
		}
		/* If the hw produced a bad checksum, check by ourselves. */
	}

	sum = csum_tcpudp_magic(ip_hdr(skb)->saddr, ip_hdr(skb)->daddr,
				ulen, IPPROTO_UDP, checksum_datagram(skb));
	if (!sum)
		sum = CSUM_MANGLED_0;
	/*
	 * Tricky: checksumming here although CHECKSUM_COMPLETE was
	 * set means that we've just found out that the hardware
	 * checksum was invalid. If our software-computed checksum is
	 * valid instead, then we disagree with the hardware. This
	 * means either the original hardware checksum is incorrect or
	 * we screwed up skb->csum when moving skb->data around, which
	 * is quite bad news either way.
	 */
	sum -= check;
	if (!sum && skb->ip_summed == CHECKSUM_COMPLETE)
		netdev_rx_csum_fault(skb->dev, skb);

	skb->csum = sum;
	skb->ip_summed = CHECKSUM_COMPLETE;
	skb->csum_complete_sw = 1;
	skb->csum_valid = !sum;

	return skb->csum_valid;
}

static inline bool validate_checksum(struct sk_buff *skb, u16 ulen, __sum16 check)
{
	skb->csum_valid = 0;

	if (__skb_checksum_validate_needed(skb, true, check))
		return __validate_checksum(skb, ulen, check);

	return true;
}

static inline bool verify_checksum(struct sk_buff *skb)
{
	struct udphdr *uh = udp_hdr(skb);
	unsigned short ulen;
	__sum16 check;

	ulen = ntohs(uh->len);

	if (ulen < sizeof(*uh))
		return false;	/* Short packet, drop it. */

	check = uh->check;
	uh->check = 0;

	return validate_checksum(skb, ulen, check);
}

static bool __queue_for_receiver(struct evl_cache *cache,
				struct sk_buff *skb,
				const struct __evl_net_udp_key *key)
{
	struct evl_cache_entry *entry = evl_lookup_cache(cache, key);
	struct evl_net_udp_receiver *e;
	unsigned long flags;

	/*
	 * If an entry is found, queue the incoming skb then wake up
	 * the receiver.
	 */
	if (entry) {
		e = container_of(entry, struct evl_net_udp_receiver, entry);
		raw_spin_lock_irqsave(&e->wait.wchan.lock, flags);
		list_add(&skb->list, &e->queue);
		if (evl_wait_active(&e->wait))
			evl_wake_up_head(&e->wait);
		raw_spin_unlock_irqrestore(&e->wait.wchan.lock, flags);
		evl_schedule();
		return true;
	}

	return false;
}

/*
 * queue_for_receiver - push an incoming datagram to the proper
 * receive slot. @skb is not part of any queue, however it might have
 * a frag list. We may reuse skb->list only for the heading @skb, but
 * not for its frags, this is ok.
 */
static bool queue_for_receiver(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);
	struct net *net = dev_net(skb->dev);
	struct udphdr *uh = udp_hdr(skb);
	struct __evl_net_udp_key key;
	struct evl_cache *cache = &net->oob.ipv4.udp;

	/*
	 * First try a direct hit to the destination address and port
	 * number.
	 */
	key.dport = ntohs(uh->dest);
	key.daddr = iph->daddr;
	if (__queue_for_receiver(cache, skb, &key))
		return true;

	/* Nope, try a waiter on the wildcard address then. */
	key.daddr = htonl(INADDR_ANY);
	return __queue_for_receiver(cache, skb, &key);
}

int evl_net_deliver_udp(struct sk_buff *skb)
{
	if (unlikely(sizeof(struct udphdr) > skb->len))
		return -EINVAL;	/* Obviously garbled, drop that. */

	if (!verify_checksum(skb))
		return -EINVAL;

	return queue_for_receiver(skb) ? 0 : -ESRCH;
}

static u32 hash_udp_slot(const void *key)
{
	const struct __evl_net_udp_key *k = key;

	return jhash2((const u32 *)k, sizeof(*k) / sizeof(u32), 0);
}

static bool eq_udp_slot(const struct evl_cache_entry *entry,
			const void *key)
{
	const struct __evl_net_udp_key *k = key;
	const struct evl_net_udp_receiver *e =
		container_of(entry, struct evl_net_udp_receiver, entry);

	return e->key.dport == k->dport && e->key.daddr == k->daddr;
}

static char *format_udp_key(const struct evl_cache_entry *entry)
{
	const struct evl_net_udp_receiver *e =
		container_of(entry, struct evl_net_udp_receiver, entry);

	return kasprintf(GFP_ATOMIC, "%pI4:%u", &e->key.daddr, e->key.dport);
}

static const void *get_udp_key(const struct evl_cache_entry *entry)
{
	const struct evl_net_udp_receiver *e =
		container_of(entry, struct evl_net_udp_receiver, entry);

	return &e->key;
}

static void drop_udp_slot(struct evl_cache_entry *entry) /* in-band */
{
	struct evl_net_udp_receiver *e =
		container_of(entry, struct evl_net_udp_receiver, entry);
	struct sk_buff *skb, *tmp;

	list_for_each_entry_safe(skb, tmp, &e->queue, list) {
		list_del(&skb->list);
		evl_net_free_skb(skb);
	}

	kfree(e);
}

static struct evl_cache_ops udp_cache_ops = {
	.hash		= hash_udp_slot,
	.eq		= eq_udp_slot,
	.get_key	= get_udp_key,
	.format_key	= format_udp_key,
	.drop		= drop_udp_slot,
};

int evl_net_init_udp(struct net *net)
{
	struct oob_net_state *nets = &net->oob;
	struct evl_cache *cache;

	/* Cache of active UDP4 receivers. */
	cache = &nets->ipv4.udp;
	cache->ops = &udp_cache_ops;
	cache->init_shift = EVL_NET_UDP_CACHE_SHIFT;
	cache->name = "udp_receivers";

	return evl_init_cache(cache);
}

void evl_net_cleanup_udp(struct net *net)
{
	struct oob_net_state *nets = &net->oob;

	evl_flush_cache(&nets->ipv4.udp);
}

struct evl_net_proto evl_net_udp_proto = {
	.attach	= attach_udp_socket,
	.destroy = destroy_udp_socket,
	.bind = bind_udp_socket,
	.shutdown = shutdown_udp_socket,
	.oob_send = send_udp,
	.oob_poll = poll_udp,
	.oob_receive = receive_udp,
	.get_netif = get_netif_udp,
	.handle_offload = handle_udp_inband,
};
