/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/rcupdate.h>
#include <linux/jhash.h>
#include <linux/err.h>
#include <net/ip.h>
#include <net/inet_frag.h>
#include <evl/memory.h>
#include <evl/net/device.h>
#include <evl/net/skb.h>
#include <evl/net/ipv4/fragment.h>

/*
 * Fragment expiration handler. Triggers when a fragmented datagram
 * could not be reassembled within the allotted time (IP_FRAG_TIME).
 */
static void frag_expired(struct evl_timer *timer)
{
	struct evl_net_frag_tdir *ftdir;
	struct evl_net_frag_tree *ft;

	ft = container_of(timer, struct evl_net_frag_tree, timer);
	ftdir = ft->tdir;
	raw_spin_lock(&ftdir->gc.lock);
	hlist_add_head(&ft->gc, &ftdir->gc.queue);
	raw_spin_unlock(&ftdir->gc.lock);
	/* Tell the RX thread to run the garbage collection. */
	evl_net_wake_rx(ft->gc_dev);
	netdev_warn(ft->gc_dev, "reassembly timed out, frag tree %px\n", ft);
}

static u32 hash_frag_key(const void *data, u32 len)
{
	return jhash2(data, len / sizeof(u32), 0);
}

static bool compare_frag_key(struct evl_net_frag_tree *ft,
			const struct frag_v4_compare_key *key)
{
	return !memcmp(&ft->key.ipv4, key, sizeof(*key));
}

static struct evl_net_frag_tree *
alloc_frag_tree(struct evl_net_frag_tdir *ftdir,
		const struct frag_v4_compare_key *key,
		struct net_device *dev)
{
	struct evl_net_frag_tree *ft;

	ft = evl_alloc(sizeof(*ft));
	if (!ft)
		return NULL;

	ft->end = 0;
	ft->len = 0;
	ft->flags = 0;
	ft->frags = RB_ROOT;
	ft->tdir = ftdir;
	ft->gc_dev = dev;
	INIT_HLIST_NODE(&ft->gc);
	ft->key.ipv4 = *key;
	evl_init_timer(&ft->timer, frag_expired);
	evl_spin_lock_init(&ft->lock);
	netdev_dbg(dev, "allocated frag tree %px\n", ft);

	return ft;
}

/* ftdir->lock held. */
static struct evl_net_frag_tree *
get_frag_tree(struct evl_net_frag_tdir *ftdir,
	const struct frag_v4_compare_key *key,
	struct net_device *dev)
{
	u32 hashval = hash_frag_key(key, sizeof(*key));
	struct evl_net_frag_tree *ft;

	hash_for_each_possible(ftdir->ht, ft, hash, hashval) {
		if (compare_frag_key(ft, key))
			return ft;
	}

	ft = alloc_frag_tree(ftdir, key, dev);
	if (!ft)
		return ERR_PTR(-ENOMEM);

	hash_add(ftdir->ht, &ft->hash, hashval);
	netdev_dbg(dev, "hashed frag tree %px\n", ft);

	/* Starts aging only once hashed. */
	evl_start_timer(&ft->timer,
			evl_abs_timeout(&ft->timer, ftdir->timeout),
			0);
	return ft;
}

/*
 * Index the new fragment into the frag tree on the fragment offset
 * found into the IP header.
 *
 * CAUTION: since ->rbnode and ->dev are unionized in sk_buff, the
 * device the indexed skbs came from can only be found in the heading
 * skb holding them (which is not indexed).
 *
 * @offset is a count of 8-byte chunks.
 *
 * ftdir->lock and ft->lock held, irqs off.
 */
static int index_frag(struct evl_net_frag_tree *ft, int offset, struct sk_buff *skb)
{
	struct rb_node **rbp, *parent;
	int ret = 0;

	parent = NULL;
	rbp = &ft->frags.rb_node;

	while (*rbp) {
		struct sk_buff *e = rb_entry(*rbp, struct sk_buff, rbnode);
		struct iphdr *iph = ip_hdr(e);
		int _offset = ntohs(iph->frag_off) & IP_OFFSET;
		parent = *rbp;
		if (offset < _offset)
			rbp = &(*rbp)->rb_left;
		else if (offset > _offset)
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST; /* Duplicate - drop it. */
	}

	rb_link_node(&skb->rbnode, parent, rbp);
	rb_insert_color(&skb->rbnode, &ft->frags);

	return ret;
}

/*
 * Reassemble the datagram, connecting all skbs indexed in the frag
 * tree as a single-linked list in logical offset order. The tree is
 * guaranteed non-empty on entry.
 *
 * @ft  the frag tree to reassemble from.
 *
 * No lock held, the frag tree is not hashed, only known to the
 * caller.
 */
static struct sk_buff *reasm_frag(struct net *net,
				struct evl_net_frag_tree *ft,
				struct net_device *dev)
{
	struct rb_node *rb = rb_first(&ft->frags);
	struct sk_buff *head, **skbp, *fskb;

	/* This has to be the heading packet at offset 0. */
	head = rb_entry(rb, struct sk_buff, rbnode);
	head->dev = dev;
	skbp = &skb_shinfo(head)->frag_list;
	rb = rb_next(rb);

	while (rb) {
		fskb = rb_entry(rb, struct sk_buff, rbnode);
		fskb->dev = dev;
		*skbp = fskb;
		skbp = &fskb->next;
		rb = rb_next(rb);
	}

	*skbp = NULL;

	return head;
}

/*
 * Push an incoming fragment to the corresponding frag tree. The main
 * logic was shamelessly lifted from ip_frag_queue().
 */
static struct sk_buff *push_frag(struct sk_buff *skb, struct net_device *dev)
{
	struct net *net = dev_net(dev);
	struct evl_net_frag_tdir *ftdir = &net->oob.ipv4.ftdir;
	struct iphdr *iph = ip_hdr(skb);
	struct frag_v4_compare_key key = {
		.saddr = iph->saddr,
		.daddr = iph->daddr,
		.user = IP_DEFRAG_LOCAL_DELIVER,
		.id = iph->id,
		.protocol = iph->protocol,
	};
	int offset, floff, end, len, ret;
	struct evl_net_frag_tree *ft;
	struct sk_buff *head;
	unsigned long flags;

	rcu_read_lock();
	key.vif = l3mdev_master_ifindex_rcu(dev);
	rcu_read_unlock();

	evl_lock_kmutex(&ftdir->lock);

	ft = get_frag_tree(ftdir, &key, dev);
	if (IS_ERR(ft)) {
		ret = PTR_ERR(ft);
		goto out_notree;
	}

	evl_spin_lock(&ft->lock);

	floff = ntohs(iph->frag_off);
	offset = (floff & IP_OFFSET) << 3; /* 8-byte chunks */
	/*
	 * The logical end offset of the packet, stripping out the
	 * l2+IP headers.
	 */
	end = offset + skb->len - skb_network_offset(skb) - ip_hdrlen(skb);

	netdev_dbg(dev, "pushing id=%d to frag tree %px, frag_off=%#x, ipfl=%#x, ihl=%d, %pI4 -> %pI4\n",
		iph->id, ft, ntohs(iph->frag_off), floff & ~IP_OFFSET,
		ip_hdrlen(skb), &iph->saddr, &iph->daddr);

	ret = -EINVAL;
	if (!(floff & IP_MF)) {	/* Last fragment in the series? */
		/*
		 * If we were already past the incoming fragment, or
		 * received a different end, this fragment is
		 * corrupted, so we reject it.
		 */
		if (end < ft->end ||
			((ft->flags & INET_FRAG_LAST_IN) && end != ft->end))
			goto out;

		ft->flags |= INET_FRAG_LAST_IN;
		ft->end = end;
		netdev_dbg(dev, "final frag id=%d\n", iph->id);
	} else {
		netdev_dbg(dev, "more frag(s) id=%d\n", iph->id);
		/*
		 * Inner frag length should be a multiple of 8
		 * bytes.
		 */
		if (end & 7) {
			end &= ~7;
			if (skb->ip_summed != CHECKSUM_UNNECESSARY)
				skb->ip_summed = CHECKSUM_NONE;
		}
		if (end > ft->end) {
			/*
			 * If receiving data beyond the final packet,
			 * the incoming packet is corrupt.
			 */
			if (ft->flags & INET_FRAG_LAST_IN)
				goto out;

			ft->end = end;
		}
	}

	if (end == offset)	/* Zero-sized? Ignore then. */
		goto out;

	if (offset == 0) {
		ft->flags |= INET_FRAG_FIRST_IN;
		netdev_dbg(dev, "first frag id=%d\n", iph->id);
	}

	/* Update the logical length received (headers stripped). */
	ft->len += end - offset;

	netdev_dbg(dev, "indexing frag id=%d\n", iph->id);
	ret = index_frag(ft, offset >> 3, skb);
	if (ret)
		goto out;

	/* If complete, reassemble the datagram. */
	if (ft->flags == (INET_FRAG_FIRST_IN | INET_FRAG_LAST_IN) &&
		ft->len == ft->end) {
		netdev_dbg(dev, "completed frag id=%d, len=%zu\n", iph->id, ft->len);
		evl_spin_unlock(&ft->lock);
		/*
		 * Stop the timer, then move the frag tree out of the
		 * gc queue if it's linked there.
		 */
		evl_stop_timer(&ft->timer);
		raw_spin_lock_irqsave(&ftdir->gc.lock, flags);
		if (!hlist_unhashed(&ft->gc))
			hlist_del(&ft->gc);
		raw_spin_unlock_irqrestore(&ftdir->gc.lock, flags);
		/* Covered by ftdir->lock. */
		hlist_del(&ft->hash);
		evl_unlock_kmutex(&ftdir->lock);
		head = reasm_frag(net, ft, dev);
		len = ip_hdrlen(skb) + ft->len;
		evl_free(ft);
		if (len > 65535) { /* RFC 791 */
			evl_net_free_skb(head); /* Timer is off, so we have to cleanup manually. */
			return ERR_PTR(-E2BIG);
		}
		return head;
	}

	/* Tell the caller to wait for more. */
	ret = -EINPROGRESS;
out:
	evl_spin_unlock(&ft->lock);
out_notree:
	evl_unlock_kmutex(&ftdir->lock);

	return ERR_PTR(ret);
}

struct sk_buff *evl_ipv4_defrag(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev ?: skb_dst(skb)->dev;

	return push_frag(skb, dev);
}
