/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <net/inet_sock.h>
#include <net/ip.h>
#include <evl/random.h>
#include <evl/net/socket.h>
#include <evl/net/device.h>
#include <evl/net/skb.h>
#include <evl/net/ipv4.h>
#include <evl/net/ipv4/route.h>
#include <evl/net/ipv4/output.h>
#include <evl/net/ipv4/arp.h>

static inline int select_ttl(struct inet_sock *inet, struct dst_entry *dst)
{
	int ttl = inet->uc_ttl;

	return ttl < 0 ? ip4_dst_hoplimit(dst) : ttl;
}

static void fill_ipv4_header(struct iphdr *iph,
			struct inet_sock *inet,
			struct dst_entry *dst,
			struct evl_net_ipv4_cookie *ipc)
{
	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = RT_TOS(inet->tos);
	iph->ttl      = select_ttl(inet, dst);
	iph->daddr    = ipc->daddr;
	iph->saddr    = ipc->saddr;
	iph->protocol = ipc->protocol;
}

/*
 * Create an IPv4 datagram from the contents referred to by a
 * user-provided I/O vector.
 *
 * @esk		emitting socket
 * @iov		source I/O vector
 * @iovlen	number of cells in vector
 * @ert		EVL route cache entry
 * @datalen	length of payload data in @iov (excluding the transport header)
 * @timeout	time limit for sleeping on congestion
 * @ipc		IPv4 cookie with misc transmit information
 *
 * We make the following assumptions:
 *
 * - no GSO for out-of-band traffic.
 * - always output frags when required (IP_DF never ignored).
 * - scatter-gather capability of the device is ignored (NETIF_F_SG).
 *
 * This routine reserves the space for a transport header in the
 * leading skb if ipc->transhdrlen > 0. The caller is expected to
 * update it eventually.
 *
 * Returns the heading socket buffer (which may have fragments) loaded
 * with the user data to transmit.
 */
struct sk_buff *evl_net_ipv4_build_datagram(struct evl_socket *esk,
				struct iovec *iov, size_t iovlen,
				struct evl_net_route *ert,
				size_t datalen,
				ktime_t timeout,
				struct evl_net_ipv4_cookie *ipc)
{
	struct net_device *dev = evl_net_route_dev(ert),
		*real_dev = evl_net_real_dev(dev);
	size_t maxfraglen, chunksz, i_offset = 0,
		offset = 0, thdrlen = 0;
	struct sk_buff *head = NULL, *skb, **skbp = NULL;
	struct dst_entry *dst = evl_net_route_dst(ert);
	struct sock *sk = esk->sk;
	struct inet_sock *inet = inet_sk(sk);
	int mtu, n = 0, ret = 0;
	__be16 id = 0, df = 0;
	struct iphdr *iph;
	__u16 frag_off;
	void *data;

	/*
	 * It's ok for us to receive packets with no payload, but not
	 * to send them.
	 */
	if (EVL_WARN_ON(NET, datalen == 0))
		return ERR_PTR(-EINVAL);

	mtu = ip_sk_use_pmtu(sk) ? dst_mtu(dst) : READ_ONCE(real_dev->mtu);
	if (!inetdev_valid_mtu(mtu))
		return ERR_PTR(-ENETUNREACH); /* Smaller than min ipv4 MTU? */

	/* Room for payload in an IP frag aligned on 8-byte boundary. */
	maxfraglen = (mtu - sizeof(*iph)) & ~7;
	if (datalen > maxfraglen) {
		id = evl_read_rng_u16();
	} else if (datalen <= IPV4_MIN_MTU || ip_dont_fragment(sk, evl_net_route_dst(ert))) {
		df = htons(IP_DF);
	}

	netdev_dbg(dev, "build dgram: src=%pI4, dst=%pI4, mtu=%d, "
		   " transhdrlen=%d, maxfraglen=%zd\n",
		   &ipc->saddr, &ipc->daddr, mtu, ipc->transhdrlen, maxfraglen);

	for (;;) {
		skb = evl_net_wget_skb(esk, real_dev, timeout);
		if (IS_ERR(skb)) {
			ret = PTR_ERR(skb);
			goto fail;
		}

		skb_reserve(skb, real_dev->hard_header_len + sizeof(*iph));

		if (skbp) {
			skb->next = NULL;
			*skbp = skb;
			skbp = &skb->next;
		} else {
			head = skb;
			skb_shinfo(skb)->frag_list = NULL;
			skbp = &skb_shinfo(skb)->frag_list;
			/*
			 * Reserve the required space to store the
			 * transport header in the first skb. As far
			 * as we are concerned, this is part of the
			 * payload.
			 */
			if (ipc->transhdrlen > 0) {
				skb_reserve(skb, ipc->transhdrlen);
				skb_push(skb, ipc->transhdrlen);
				skb_reset_transport_header(skb);
			}
		}

		skb->ip_summed = CHECKSUM_NONE;
		skb->csum = 0;
		/* To be used as the VLAN priority by the hw layer. */
		skb->priority = READ_ONCE(sk->sk_priority);
		/* Assume IP over ethernet ATM. */
		skb->protocol = htons(ETH_P_IP);
		/* We want the payload offset at head of packet. */
		frag_off = htons((offset + thdrlen) >> 3);

		do {
			if (iov->iov_len == 0) {
				if (++n >= iovlen)
					break;
				iov++;
				i_offset = 0;
				continue;
			}

			chunksz = iov->iov_len;
			if (chunksz > maxfraglen - skb->len)
				chunksz = maxfraglen - skb->len;

			data = skb_put(skb, chunksz);
			ret = raw_copy_from_user(data, iov->iov_base + i_offset, chunksz);
			if (ret)
				goto fail;

			iov->iov_len -= chunksz;
			i_offset += chunksz; /* input offset (in current vector cell) */
			offset += chunksz;   /* virtual packet offset (frag-insensitive) */
		} while (skb->len < maxfraglen);

		/* Account for the transport header past the heading packet. */
		thdrlen = ipc->transhdrlen;
		iph = skb_push(skb, sizeof(*iph));
		skb_reset_network_header(skb);
		fill_ipv4_header(iph, inet, dst, ipc);
		iph->tot_len = htons(skb->len);
		iph->id = id;
		if (offset < datalen) {
			iph->frag_off = frag_off | htons(IP_MF);
			ip_send_check(iph);
		} else {
			iph->frag_off = frag_off | df;
			ip_send_check(iph);
			break;
		}
	}

	return head;
fail:
	if (head)
		evl_net_wput_skb(head);

	return ERR_PTR(ret);
}
