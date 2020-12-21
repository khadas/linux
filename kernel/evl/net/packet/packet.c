/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/hashtable.h>
#include <linux/jhash.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/poll.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/err.h>
#include <linux/ip.h>
#include <net/sock.h>
#include <evl/lock.h>
#include <evl/thread.h>
#include <evl/wait.h>
#include <evl/poll.h>
#include <evl/sched.h>
#include <evl/net/socket.h>
#include <evl/net/packet.h>
#include <evl/net/input.h>
#include <evl/net/output.h>
#include <evl/net/device.h>
#include <evl/net/skb.h>
#include <evl/uaccess.h>

static struct evl_net_proto *
find_packet_proto(__be16 protocol,
		struct evl_net_proto *default_proto);

/*
 * Lock nesting: protocol_lock -> rxq->lock -> esk->input_wait.lock
 * We use linear skbs only (no paged data).
 */

#define EVL_PROTO_HASH_BITS	8

static DEFINE_HASHTABLE(protocol_hash, EVL_PROTO_HASH_BITS);

/*
 * Protects protocol_hash, shared between in-band and oob contexts,
 * never accessed from oob IRQ handlers.
 */
static DEFINE_EVL_SPINLOCK(protocol_lock);

/* oob, hard irqs off */
static bool __packet_deliver(struct evl_net_rxqueue *rxq,
			struct sk_buff *skb, __be16 protocol)
{
	struct net_device *dev = skb->dev;
	bool delivered = false;
	struct evl_socket *esk;
	struct sk_buff *qskb;
	u16 vlan_id;
	int ifindex;

	evl_spin_lock(&rxq->lock);

	/*
	 * Subscribers are searched sequentially for a matching net
	 * device if specified, otherwise we accept traffic from any
	 * interface. The net_device lookup could be hashed too, but
	 * [lame excuse coming] we are not supposed to have a
	 * truckload of NICs to listen to, so keep it plain dumb which
	 * is going to be faster in the normal case.
	 */
	list_for_each_entry(esk, &rxq->subscribers, next_sub) {
		/*
		 * Revisit: we could filter on some "activation tag"
		 * value calculated at activation time
		 * (enable_oob_port) to eliminate spurious matches on
		 * a newly registered device reusing an old ifindex we
		 * initially captured at binding time.
		 */
		ifindex = READ_ONCE(esk->binding.real_ifindex);
		if (ifindex) {
			if (ifindex != dev->ifindex)
				continue;
			vlan_id = READ_ONCE(esk->binding.vlan_id);
			if (skb_vlan_tag_get_id(skb) != vlan_id)
				continue;
		}

		/*
		 * This packet may be delivered to esk, attempt to
		 * charge it to its rmem counter. If the socket may
		 * not consume more memory, skip delivery and try with
		 * the next subscriber.
		 */
		if (!evl_charge_socket_rmem(esk, skb))
			continue;

		/*
		 * All sockets bound to ETH_P_ALL receive a clone of
		 * each incoming buffer, leaving the latter unconsumed
		 * yet. A single one among the other listeners
		 * consumes the incoming buffer.
		 */
		qskb = skb;
		if (protocol == htons(ETH_P_ALL)) {
			qskb = evl_net_clone_skb(skb);
			if (qskb == NULL) {
				evl_flush_wait(&esk->input_wait, T_NOMEM);
				evl_uncharge_socket_rmem(esk, skb);
				break;
			}
		}

		raw_spin_lock(&esk->input_wait.lock);

		list_add_tail(&qskb->list, &esk->input);
		if (evl_wait_active(&esk->input_wait))
			evl_wake_up_head(&esk->input_wait);

		raw_spin_unlock(&esk->input_wait.lock);

		evl_signal_poll_events(&esk->poll_head,	POLLIN|POLLRDNORM);
		delivered = true;
		if (protocol != htons(ETH_P_ALL))
			break;
	}

	evl_spin_unlock(&rxq->lock);

	return delivered;
}

/* protocol_lock held, hard irqs off */
static struct evl_net_rxqueue *find_rxqueue(u32 hkey)
{
	struct evl_net_rxqueue *rxq;

	hash_for_each_possible(protocol_hash, rxq, hash, hkey)
		if (rxq->hkey == hkey)
			return rxq;

	return NULL;
}

static inline u32 get_protocol_hash(__be16 protocol)
{
	u32 hsrc = protocol;

	return jhash2(&hsrc, 1, 0);
}

static bool packet_deliver(struct sk_buff *skb, __be16 protocol) /* oob */
{
	struct evl_net_rxqueue *rxq;
	unsigned long flags;
	bool ret = false;
	u32 hkey;

	hkey = get_protocol_hash(protocol);

	/*
	 * Find the rx queue linking sockets attached to the protocol.
	 */
	evl_spin_lock_irqsave(&protocol_lock, flags); /* FIXME: this is utterly inefficient. */

	rxq = find_rxqueue(hkey);
	if (rxq)
		ret = __packet_deliver(rxq, skb, protocol);

	evl_spin_unlock_irqrestore(&protocol_lock, flags);

	return ret;
}

/**
 *	evl_net_packet_deliver - deliver an ethernet packet to the raw
 *	interface tap
 *
 *	Deliver a copy of @skb to every socket accepting all ethernet
 *	protocols (ETH_P_ALL) if any, and/or @skb to the heading
 *	socket waiting for skb->protocol.
 *
 *	@skb the packet to deliver, not linked to any upstream
 *	queue.
 *
 *      Returns true if @skb was queued.
 *
 *	Caller must call evl_schedule().
 */
bool evl_net_packet_deliver(struct sk_buff *skb) /* oob */
{
	packet_deliver(skb, htons(ETH_P_ALL));

	return packet_deliver(skb, skb->protocol);
}

/* in-band. */
static int attach_packet_socket(struct evl_socket *esk,
				struct evl_net_proto *proto, __be16 protocol)
{
	struct evl_net_rxqueue *rxq, *_rxq;
	unsigned long flags;
	u32 hkey;

	hkey = get_protocol_hash(protocol);

	/*
	 * We pre-allocate an rx queue then drop it if one is already
	 * hashed for the same protocol. Not pretty but we are running
	 * in-band, and this keeps the hard locked section short.
	 */
	rxq = evl_net_alloc_rxqueue(hkey);
	if (rxq == NULL)
		return -ENOMEM;

	/*
	 * From this point we cannot fail, packets might come in as
	 * soon as we queue.
	 */

	evl_spin_lock_irqsave(&protocol_lock, flags);

	esk->proto = proto;
	esk->binding.proto_hash = hkey;
	esk->protocol = protocol;

	_rxq = find_rxqueue(hkey);
	if (_rxq) {
		evl_spin_lock(&_rxq->lock);
		list_add(&esk->next_sub, &_rxq->subscribers);
		evl_spin_unlock(&_rxq->lock);
	} else {
		hash_add(protocol_hash, &rxq->hash, hkey);
		list_add(&esk->next_sub, &rxq->subscribers);
	}

	evl_spin_unlock_irqrestore(&protocol_lock, flags);

	if (_rxq)
		evl_net_free_rxqueue(rxq);

	return 0;
}

/* in-band, esk->lock held or socket_release() */
static void detach_packet_socket(struct evl_socket *esk)
{
	struct evl_net_rxqueue *rxq, *n;
	unsigned long flags;
	LIST_HEAD(tmp);

	if (list_empty(&esk->next_sub))
		return;

	evl_spin_lock_irqsave(&protocol_lock, flags);

	rxq = find_rxqueue(esk->binding.proto_hash);

	list_del_init(&esk->next_sub); /* Remove from rxq->subscribers */
	if (list_empty(&rxq->subscribers)) {
		hash_del(&rxq->hash);
		list_add(&rxq->next, &tmp);
	}

	evl_spin_unlock_irqrestore(&protocol_lock, flags);

	list_for_each_entry_safe(rxq, n, &tmp, next)
		evl_net_free_rxqueue(rxq);
}

/* in-band */
static int bind_packet_socket(struct evl_socket *esk,
			struct sockaddr *addr,
			int len)
{
	int ret, new_ifindex, real_ifindex, old_ifindex;
	static struct evl_net_proto *proto;
	struct net_device *dev = NULL;
	struct sockaddr_ll *sll;
	unsigned long flags;
	u16 vlan_id;

	if (len != sizeof(*sll))
		return -EINVAL;

	sll = (struct sockaddr_ll *)addr;
	if (sll->sll_family != AF_PACKET)
		return -EINVAL;

	proto = find_packet_proto(sll->sll_protocol, esk->proto);
	if (proto == NULL)
		return -EINVAL;

	new_ifindex = sll->sll_ifindex;

	mutex_lock(&esk->lock);

	old_ifindex = esk->binding.vlan_ifindex;
	if (new_ifindex != old_ifindex) {
		if (new_ifindex) {
			/* @dev has to be a VLAN device. */
			dev = evl_net_get_dev_by_index(esk->net, new_ifindex);
			if (dev == NULL)
				return -EINVAL;
			vlan_id = vlan_dev_vlan_id(dev);
			real_ifindex = vlan_dev_real_dev(dev)->ifindex;
		} else {
			vlan_id = 0;
			real_ifindex = 0;
		}
	}

	/*
	 * We precede the regular AF_PACKET bind handler which makes
	 * no sense of bindings to VLAN devices unlike we do. Fix up
	 * the address accordingly, pointing at the real device
	 * instead.
	 */
	sll->sll_ifindex = real_ifindex;

	if (esk->protocol != sll->sll_protocol) {
		detach_packet_socket(esk);
		/*
		 * Since the old binding was dropped, we would not
		 * receive anything if the new binding fails. This
		 * said, -ENOMEM is the only possible failure, so the
		 * root issue would be way more problematic than a
		 * dead socket.
		 */
		ret = attach_packet_socket(esk, proto, sll->sll_protocol);
		if (ret)
			goto out;
	}

	/*
	 * Ensure that all binding-related changes happen atomically
	 * from the standpoint of oob observers.
	 *
	 * Revisit: cannot race with IRQs, use preemption-disabling
	 * spinlock instead.
	 */
	raw_spin_lock_irqsave(&esk->oob_lock, flags);
	if (new_ifindex != old_ifindex) {
		/* First change the real interface, next the vid. */
		WRITE_ONCE(esk->binding.real_ifindex, real_ifindex);
		esk->binding.vlan_id = vlan_id;
		WRITE_ONCE(esk->binding.vlan_ifindex, new_ifindex);
	}
	raw_spin_unlock_irqrestore(&esk->oob_lock, flags);

 out:
	mutex_unlock(&esk->lock);

	if (dev)
		evl_net_put_dev(dev);

	return ret;
}

static struct net_device *get_netif_packet(struct evl_socket *esk)
{
	return  evl_net_get_dev_by_index(esk->net,
					esk->binding.vlan_ifindex);
}

static struct net_device *find_xmit_device(struct evl_socket *esk,
			const struct user_oob_msghdr __user *u_msghdr)
{
	struct sockaddr_ll __user *u_addr;
	__u64 name_ptr = 0, namelen = 0;
	struct sockaddr_ll addr;
	struct net_device *dev;
	int ret;

	ret = raw_get_user(name_ptr, &u_msghdr->name_ptr);
	if (ret)
		return ERR_PTR(-EFAULT);

	ret = raw_get_user(namelen, &u_msghdr->namelen);
	if (ret)
		return ERR_PTR(-EFAULT);

	if (!name_ptr) {
		if (namelen)
			return ERR_PTR(-EINVAL);

		dev = esk->proto->get_netif(esk);
	} else {
		if (namelen < sizeof(addr))
			return ERR_PTR(-EINVAL);

		u_addr = evl_valptr64(name_ptr, struct sockaddr_ll);
		ret = raw_copy_from_user(&addr, u_addr, sizeof(addr));
		if (ret)
			return ERR_PTR(-EFAULT);

		if (addr.sll_family != AF_PACKET &&
			addr.sll_family != AF_UNSPEC)
			return ERR_PTR(-EINVAL);

		dev = evl_net_get_dev_by_index(esk->net, addr.sll_ifindex);
	}

	if (dev == NULL)
		return ERR_PTR(-ENXIO);

	return dev;
}

/* oob */
static ssize_t send_packet(struct evl_socket *esk,
			const struct user_oob_msghdr __user *u_msghdr,
			const struct iovec *iov,
			size_t iovlen)
{
	struct net_device *dev, *real_dev;
	struct __evl_timespec uts;
	enum evl_tmode tmode;
	struct sk_buff *skb;
	__u32 msg_flags = 0;
	ssize_t ret, count;
	ktime_t timeout;
	size_t rem;

	ret = raw_get_user(msg_flags, &u_msghdr->flags);
	if (ret)
		return -EFAULT;

	if (msg_flags & ~MSG_DONTWAIT)
		return -EINVAL;

	if (evl_socket_f_flags(esk) & O_NONBLOCK)
		msg_flags |= MSG_DONTWAIT;

	/*
	 * Fetch the timeout on obtaining a buffer from
	 * est->free_skb_pool.
	 */
	ret = raw_copy_from_user(&uts, &u_msghdr->timeout, sizeof(uts));
	if (ret)
		return -EFAULT;

	timeout = msg_flags & MSG_DONTWAIT ? EVL_NONBLOCK :
		u_timespec_to_ktime(uts);
	tmode = timeout ? EVL_ABS : EVL_REL;

	/* Determine the xmit interface (always a VLAN device). */
	dev = find_xmit_device(esk, u_msghdr);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	/*
	 * Since @dev is a VLAN device, then @real_dev cannot be stale
	 * until @dev goes down per the in-band refcounting
	 * guarantee. Since we hold a crossing reference on @dev, it
	 * cannot go down as it would need to pass the crossing first,
	 * so @real_dev cannot go stale until we are done.
	 */
	real_dev = vlan_dev_real_dev(dev);
	skb = evl_net_dev_alloc_skb(real_dev, timeout, tmode);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		goto out;
	}

	skb_reset_mac_header(skb);
	skb->protocol = esk->protocol;
	skb->dev = real_dev;
	skb->priority = esk->sk->sk_priority;

	count = evl_import_iov(iov, iovlen, skb->data, skb_tailroom(skb), &rem);
	if (rem || count > dev->mtu + dev->hard_header_len + VLAN_HLEN)
		ret = -EMSGSIZE;
	else if (!dev_validate_header(dev, skb->data, count))
		ret = -EINVAL;

	if (ret < 0)
		goto cleanup;

	skb_put(skb, count);

	if (!skb->protocol || skb->protocol == htons(ETH_P_ALL))
		skb->protocol = dev_parse_header_protocol(skb);

	skb_set_network_header(skb, real_dev->hard_header_len);

	/*
	 * Charge the socket with the memory consumption of skb,
	 * waiting for the output to drain if needed. The latter might
	 * fail if we got forcibly unblocked while waiting for the
	 * output contention to end, or the caller asked for a
	 * non-blocking operation while such contention was ongoing.
	 */
	ret = evl_charge_socket_wmem(esk, skb, timeout, tmode);
	if (ret)
		goto cleanup;

	ret = evl_net_ether_transmit(dev, skb);
	if (ret) {
		evl_uncharge_socket_wmem(skb);
		goto cleanup;
	}

	ret = count;
out:
	evl_net_put_dev(dev);

	return ret;
cleanup:
	evl_net_free_skb(skb);
	goto out;
}

static ssize_t copy_packet_to_user(struct user_oob_msghdr __user *u_msghdr,
				const struct iovec *iov,
				size_t iovlen,
				struct sk_buff *skb)
{
	struct sockaddr_ll addr, __user *u_addr;
	__u64 name_ptr, namelen;
	__u32 msg_flags = 0;
	ssize_t ret, count;

	ret = raw_get_user(name_ptr, &u_msghdr->name_ptr);
	if (ret)
		return -EFAULT;

	ret = raw_get_user(namelen, &u_msghdr->namelen);
	if (ret)
		return -EFAULT;

	if (!name_ptr) {
		if (namelen)
			return -EINVAL;
		goto copy_data;
	}

	if (namelen < sizeof(addr))
		return -EINVAL;

	addr.sll_family = AF_PACKET;
	addr.sll_protocol = skb->protocol;
	addr.sll_ifindex = skb->dev->ifindex;
	addr.sll_hatype = skb->dev->type;
	addr.sll_pkttype = skb->pkt_type;
	addr.sll_halen = dev_parse_header(skb, addr.sll_addr);

	u_addr = evl_valptr64(name_ptr, struct sockaddr_ll);
	ret = raw_copy_to_user(u_addr, &addr, sizeof(addr));
	if (ret)
		return -EFAULT;

copy_data:
	count = evl_export_iov(iov, iovlen, skb->data, skb->len);
	if (count < skb->len)
		msg_flags |= MSG_TRUNC;

	ret = raw_put_user(msg_flags, &u_msghdr->flags);
	if (ret)
		return -EFAULT;

	return count;
}

/* oob */
static ssize_t receive_packet(struct evl_socket *esk,
			struct user_oob_msghdr __user *u_msghdr,
			const struct iovec *iov,
			size_t iovlen)
{
	struct __evl_timespec uts;
	enum evl_tmode tmode;
	struct sk_buff *skb;
	unsigned long flags;
	__u32 msg_flags = 0;
	ktime_t timeout;
	ssize_t ret;

	if (u_msghdr) {
		ret = raw_get_user(msg_flags, &u_msghdr->flags);
		if (ret)
			return -EFAULT;

		/* No MSG_TRUNC on recv, too much of a kludge. */
		if (msg_flags & ~MSG_DONTWAIT)
			return -EINVAL;

		/*
		 * Fetch the timeout on receiving a buffer from
		 * esk->input.
		 */
		ret = raw_copy_from_user(&uts, &u_msghdr->timeout,
					sizeof(uts));
		if (ret)
			return -EFAULT;

		timeout = u_timespec_to_ktime(uts);
		tmode = timeout ? EVL_ABS : EVL_REL;
	} else {
		timeout = EVL_INFINITE;
		tmode = EVL_REL;
	}

	if (evl_socket_f_flags(esk) & O_NONBLOCK)
		msg_flags |= MSG_DONTWAIT;

	do {
		raw_spin_lock_irqsave(&esk->input_wait.lock, flags);

		if (!list_empty(&esk->input)) {
			skb = list_get_entry(&esk->input, struct sk_buff, list);
			raw_spin_unlock_irqrestore(&esk->input_wait.lock, flags);
			/* Restore the MAC header. */
			skb_push(skb, skb->data - skb_mac_header(skb));
			ret = copy_packet_to_user(u_msghdr, iov, iovlen, skb);
			evl_uncharge_socket_rmem(esk, skb);
			evl_net_free_skb(skb);
			return ret;
		}

		if (msg_flags & MSG_DONTWAIT) {
			raw_spin_unlock_irqrestore(&esk->input_wait.lock, flags);
			return -EWOULDBLOCK;
		}

		evl_add_wait_queue(&esk->input_wait, timeout, tmode);
		raw_spin_unlock_irqrestore(&esk->input_wait.lock, flags);
		ret = evl_wait_schedule(&esk->input_wait);
	} while (!ret);

	return ret;
}

/* oob */
static __poll_t poll_packet(struct evl_socket *esk,
			struct oob_poll_wait *wait)
{
	struct evl_netdev_state *est;
	struct net_device *dev;
	__poll_t ret = 0;

	/*
	 * Enqueue, then test. No big deal if we race on list_empty(),
	 * at worst this would lead to a spurious wake up, which the
	 * caller would detect under lock then go back waiting.
	 */
	evl_poll_watch(&esk->poll_head, wait, NULL);
	if (!list_empty(&esk->input))
		ret = POLLIN|POLLRDNORM;

	dev = esk->proto->get_netif(esk);
	if (dev) {
		est = dev->oob_context.dev_state.estate;
		evl_poll_watch(&est->poll_head, wait, NULL);
		if (!list_empty(&est->free_skb_pool))
			ret = POLLOUT|POLLWRNORM;
		evl_net_put_dev(dev);
	}

	return ret;
}

static struct evl_net_proto ether_packet_proto = {
	.attach	= attach_packet_socket,
	.detach = detach_packet_socket,
	.bind = bind_packet_socket,
	.oob_send = send_packet,
	.oob_poll = poll_packet,
	.oob_receive = receive_packet,
	.get_netif = get_netif_packet,
};

static struct evl_net_proto *
find_packet_proto(__be16 protocol,
		struct evl_net_proto *default_proto)
{
	switch (protocol) {
	case htons(ETH_P_ALL):
	case htons(ETH_P_IP):
		return &ether_packet_proto;
	case 0:
		return default_proto;
	default:
		return NULL;
	}
}

static struct evl_net_proto *
match_packet_domain(int type, __be16 protocol)
{
	static struct evl_net_proto *proto;

	proto = find_packet_proto(protocol, &ether_packet_proto);
	if (proto == NULL)
		return NULL;

	if (type != SOCK_RAW)
		return ERR_PTR(-ESOCKTNOSUPPORT);

	return proto;
}

struct evl_socket_domain evl_net_packet = {
	.af_domain = AF_PACKET,
	.match = match_packet_domain,
};
