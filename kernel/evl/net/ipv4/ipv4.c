/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/net.h>
#include <linux/in.h>
#include <linux/units.h>
#include <linux/jhash.h>
#include <net/ip.h>
#include <evl/assert.h>
#include <evl/mutex.h>
#include <evl/memory.h>
#include <evl/net/socket.h>
#include <evl/net/skb.h>
#include <evl/net/ipv4.h>
#include <evl/net/ipv4/fragment.h>
#include <evl/net/ipv4/route.h>
#include <evl/net/ipv4/arp.h>
#include <evl/net/ipv4/udp.h>

/*
 * Setup the IPv4 portion of the EVL state into an in-band network
 * namespace (struct net { ... struct oob_net_state oob; ... }).
 */
int evl_net_init_ipv4(struct net *net)
{
	struct oob_net_state *nets = &net->oob;
	struct evl_net_frag_tdir *ftdir;
	struct evl_net_frag_gc *gc;
	int ret;

	ret = evl_net_init_ipv4_routing(net);
	if (ret)
		return ret;

	ret = evl_net_init_arp(net);
	if (ret)
		goto fail_arp;

	ret = evl_net_init_udp(net);
	if (ret)
		goto fail_udp;

	/* Fragment directory and friends. */
	ftdir = &nets->ipv4.ftdir;
	hash_init(ftdir->ht);
	evl_init_kmutex(&ftdir->lock);
	ftdir->timeout = (ktime_t)IP_FRAG_TIME / HZ * NANOHZ_PER_HZ;
	gc = &ftdir->gc;
	INIT_HLIST_HEAD(&gc->queue);
	raw_spin_lock_init(&gc->lock);
	might_hard_lock(&gc->lock);

	return 0;

fail_udp:
	evl_net_cleanup_arp(net);
fail_arp:
	evl_net_cleanup_ipv4_routing(net);

	return ret;
}

void evl_net_cleanup_ipv4(struct net *net)
{
	struct oob_net_state *nets = &net->oob;
	struct evl_net_frag_gc *gc = &nets->ipv4.ftdir.gc;

	evl_net_cleanup_udp(net);
	evl_net_cleanup_arp(net);
	evl_net_cleanup_ipv4_routing(net);
	EVL_WARN_ON(NET, !hlist_empty(&gc->queue));
}

/*
 * evl_net_ipv4_deliver - deliver an IPv4 packet to its final handler
 * (typically the UDP layer).
 *
 * @skb the packet to deliver to the IPv4 stack.
 *
 * On error from this routine, the caller should care of dropping
 * @skb.
 *
 * The logic of this code borrows a lot from ip_rcv_core(), with
 * EVL-specific tweaks.
 */
int evl_net_ipv4_deliver(struct sk_buff *skb)
{
	struct iphdr *iph;
	u32 len;
	int ret;

	/*
	 * Out-of-band packets are never shared on entry and always
	 * linear. Part of this routine relies on these requirements.
	 */
	if (EVL_WARN_ON(NET, skb_shared(skb)))
		return -EINVAL;

	if (EVL_WARN_ON(NET, skb_is_nonlinear(skb)))
		return -EINVAL;

	/*
	 * Do not handle packets which were not directly targeted
	 * towards the network interface. If so, eth_type_trans() has
	 * set the packet type to PACKET_OTHERHOST.
	 */
	if (skb->pkt_type == PACKET_OTHERHOST)
		return -ENOMSG;

	/*
	 * Make sure that we have enough data in there to hold an IP
	 * header (Since out-of-band packets are always linear,
	 * skb->data_len is zero, therefore skb_headlen() is actually
	 * skb->len).
	 */
	if (skb_headlen(skb) < sizeof(*iph))
		return -EINVAL;

	iph = ip_hdr(skb);

	/*
	 * Check minimum size of an IP header in 32bit words, and IPv4
	 * signature as well.
	 */
	if (iph->ihl < 5 || iph->version != 4)
		return -EINVAL;

	if (skb_headlen(skb) < iph->ihl * sizeof(u32))
		return -EINVAL;

	/* RFC 1122: silently drop packets failing the checksum. */
	if (unlikely(ip_fast_csum(iph, iph->ihl)))
		return -EINVAL;

	/* Check for truncated packet. */
	len = ntohs(iph->tot_len);
	if (skb->len < len)
		return -EINVAL;

	if (len < iph->ihl * sizeof(u32))
		return -EINVAL;

	if (pskb_trim_rcsum(skb, len))
		return -EINVAL;

	/*
	 * The IP header should not have moved because of trimming
	 * since the skb is linear and not cloned.
	 */
	if (EVL_WARN_ON(NET, iph != ip_hdr(skb)))
		return -EINVAL;

	skb->transport_header = skb->network_header + iph->ihl * sizeof(u32);

	/*
	 * If this is an IP fragment, push it to the defragmenter for
	 * reassembly. A successful return from evl_ipv4_defrag()
	 * passing back a valid heading skb means that the datagram is
	 * now complete.
	 */
	if (ip_is_fragment(iph)) {
		skb = evl_ipv4_defrag(skb);
		if (IS_ERR(skb)) {
			ret = PTR_ERR(skb);
			switch (ret) {
			case -EINPROGRESS:
			case -E2BIG:
				/*
				 * We don't want the caller to drop
				 * the skb, either because we are
				 * waiting for more data to reassemble
				 * the datagram, or we did the cleanup
				 * already.
				 */
				return 0;
			default:
				return ret;
			}
		}
	}

	/* Pass complete datagram to the next layer. */

	switch (iph->protocol) {
	case IPPROTO_UDP:
		ret = evl_net_deliver_udp(skb);
		/*
		 * Our caller does not know about fragmentation. On
		 * error, care for releasing the heading buffer by
		 * ourselves.
		 */
		if (ret)
			evl_net_free_skb(skb);
		return 0;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

/*
 * Run the garbage collection for a given net, like dropping outdated
 * IPv4 frags.
 */
void __evl_net_ipv4_gc(struct evl_net_frag_tdir *ftdir)
{
	struct evl_net_frag_tree *ft;
	struct hlist_head tmp;
	struct hlist_node *n;
	unsigned long flags;

	raw_spin_lock_irqsave(&ftdir->gc.lock, flags);
	hlist_move_list(&ftdir->gc.queue, &tmp);
	raw_spin_unlock_irqrestore(&ftdir->gc.lock, flags);

	evl_lock_kmutex(&ftdir->lock);

	hlist_for_each_entry_safe(ft, n, &tmp, gc) {
		netdev_dbg(ft->gc_dev, "free frag tree %px\n", ft);
		hlist_del(&ft->hash);
		evl_free(ft);
	}

	evl_unlock_kmutex(&ftdir->lock);
}

static struct evl_net_proto *match_ipv4_domain(int type, int protocol)
{
	switch (protocol) {
	case IPPROTO_UDP:
		if (type != SOCK_DGRAM)
			return ERR_PTR(-ESOCKTNOSUPPORT);

		return &evl_net_udp_proto;
	default:
		return NULL;
	}
}

struct evl_socket_domain evl_net_ipv4 = {
	.af_domain = AF_INET,
	.match = match_ipv4_domain,
};
