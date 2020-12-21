/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>
#include <evl/sched.h>
#include <evl/thread.h>
#include <evl/crossing.h>
#include <evl/net/neighbour.h>
#include <evl/net/socket.h>
#include <evl/net/device.h>
#include <evl/net/skb.h>
#include <evl/net/qdisc.h>
#include <evl/net/input.h>
#include <evl/net/output.h>

/*
 * Since we need an EVL kthread to handle traffic from the out-of-band
 * stage without borrowing CPU time unwisely from random contexts,
 * let's have separate, per-device threads for RX and TX. This gives
 * the best flexibility for leveraging multi-core capabilities on
 * high-bandwidth systems. Kthread priority defaults to 1, chrt is our
 * friend for fine-grained tuning.
 */
#define KTHREAD_RX_PRIO  1
#define KTHREAD_TX_PRIO  1

/*
 * The default number of socket buffers which should be available on a
 * per-device basis for conveying out-of-band traffic (if not
 * specified by the EVL_SOCKIOC_DIVERT_ON request).
 */
#define EVL_DEFAULT_NETDEV_POOLSZ  128
/*
 * The default fixed payload size available in skbs for conveying
 * out-of-band traffic through the device (if not specified by the
 * EVL_SOCKIOC_DIVERT_ON request).
 */
#define EVL_DEFAULT_NETDEV_BUFSZ   2048
/*
 * Max. values for the settings above.
 */
#define EVL_MAX_NETDEV_POOLSZ  16384
#define EVL_MAX_NETDEV_BUFSZ   8192

/*
 * The list of active oob port devices (i.e. VLAN devices which can be
 * used to send/receive oob traffic).
 */
static LIST_HEAD(active_port_devices);

static DEFINE_HARD_SPINLOCK(active_port_lock);

static struct evl_kthread *
start_handler_thread(struct net_device *dev,
		void (*fn)(void *arg),
		int prio, const char *type)
{
	struct evl_kthread *kt;
	int ret;

	kt = kzalloc(sizeof(*kt), GFP_KERNEL);
	if (kt == NULL)
		return ERR_PTR(-ENOMEM);

	ret = evl_run_kthread(kt, fn, dev, prio, 0, "%s.%s",
			netdev_name(dev), type);
	if (ret) {
		kfree(kt);
		return ERR_PTR(ret);
	}

	return kt;
}

static int enable_oob_port(struct net_device *dev,
			struct evl_netdev_activation *act)
{
	struct oob_netdev_state *nds, *vnds;
	struct evl_netdev_state *est;
	struct net_device *real_dev;
	struct evl_kthread *kt;
	unsigned long flags;
	unsigned int mtu;
	int ret;

	if (EVL_WARN_ON(NET, !is_vlan_dev(dev)))
		return -ENXIO;

	/*
	 * @dev is a VLAN device which we want to enable as a port for
	 * channeling out-of-band traffic.
	 */
	if (netdev_is_oob_port(dev))
		return 0;	/* Already enabled. */

	/*
	 * Diversion is turned on for the real device a VLAN device
	 * sits on top of, so that the EVL stack is given a chance to
	 * pick the ingress traffic to be routed to the oob stage. We
	 * (only) need a valid crossing on the VLAN device, so that
	 * oob operations can hold references on it using
	 * evl_net_get_dev(). More resources are needed by the real
	 * device backing it.
	 *
	 * NOTE: the diversion flag is beared by the real device
	 * exclusively, _never_ by a VLAN device.
	 */
	real_dev = vlan_dev_real_dev(dev);
	nds = &real_dev->oob_context.dev_state;
	est = nds->estate;
	if (est == NULL) {
		est = kzalloc(sizeof(*est), GFP_KERNEL);
		if (est == NULL)
			return -ENOMEM;
		nds->estate = est;
	}

	vnds = &dev->oob_context.dev_state;
	evl_init_crossing(&vnds->crossing);

	if (est->refs++ > 0)
		goto queue;

	if (!act->poolsz)
		act->poolsz = EVL_DEFAULT_NETDEV_POOLSZ;

	if (!act->bufsz)
		act->bufsz = EVL_DEFAULT_NETDEV_BUFSZ;

	/* Silently align on the current mtu if need be. */
	mtu = READ_ONCE(real_dev->mtu);
	if (act->bufsz < mtu)
		act->bufsz = mtu;

	est->pool_free = 0;
	est->pool_max = act->poolsz;
	est->buf_size = act->bufsz;
	est->qdisc = evl_net_alloc_qdisc(&evl_net_qdisc_fifo);
	if (IS_ERR(est->qdisc))
		return PTR_ERR(est->qdisc);

	ret = evl_net_dev_build_pool(real_dev);
	if (ret)
		goto fail_build_pool;

	evl_net_init_skb_queue(&est->rx_queue);
	evl_init_flag(&est->rx_flag);

	kt = start_handler_thread(real_dev, evl_net_do_rx,
				KTHREAD_RX_PRIO, "rx");
	if (IS_ERR(kt))
		goto fail_start_rx;

	est->rx_handler = kt;

	/*
	 * We need a TX handler only for oob-capable
	 * devices. Otherwise, the traffic would go through an in-band
	 * Qdisc, like the sched_oob in-band queues.
	 */
	if (netdev_is_oob_capable(real_dev)) {
		evl_init_flag(&est->tx_flag);
		kt = start_handler_thread(real_dev, evl_net_do_tx,
					KTHREAD_TX_PRIO, "tx");
		if (IS_ERR(kt))
			goto fail_start_tx;

		est->tx_handler = kt;
	}

	evl_init_crossing(&nds->crossing);

	netif_enable_oob_diversion(real_dev);

queue:
	netdev_enable_oob_port(dev);

	raw_spin_lock_irqsave(&active_port_lock, flags);
	list_add(&vnds->next, &active_port_devices);
	raw_spin_unlock_irqrestore(&active_port_lock, flags);

	return 0;

fail_start_tx:
	/*
	 * No skb has flowed yet, no pending recycling op. Likewise,
	 * we cannot have any rxq in the cache or dump lists.
	 */
	evl_stop_kthread(est->rx_handler);
	evl_destroy_flag(&est->tx_flag);
fail_start_rx:
	evl_net_dev_purge_pool(real_dev);
	evl_destroy_flag(&est->rx_flag);
fail_build_pool:
	evl_net_free_qdisc(est->qdisc);
	kfree(est);
	nds->estate = NULL;

	return ret;
}

static void disable_oob_port(struct net_device *dev)
{
	struct oob_netdev_state *vnds;
	struct evl_netdev_state *est;
	struct net_device *real_dev;
	unsigned long flags;

	if (EVL_WARN_ON(NET, !is_vlan_dev(dev)))
		return;

	if (!netdev_is_oob_port(dev))
		return;

	/*
	 * Make sure that no evl_down_crossing() can be issued after
	 * we attempt to pass the crossing. Since the former can only
	 * happen as a result of finding the device in the active
	 * list, first unlink the latter _then_ pass the crossing
	 * next.
	 */
	vnds = &dev->oob_context.dev_state;
	raw_spin_lock_irqsave(&active_port_lock, flags);
	list_del(&vnds->next);
	raw_spin_unlock_irqrestore(&active_port_lock, flags);

	/*
	 * Ok, now we may attempt to pass the crossing, waiting until
	 * all in-flight oob operations holding a reference on the
	 * vlan device acting as an oob port have completed.
	 */
	evl_pass_crossing(&vnds->crossing);

	netdev_disable_oob_port(dev);

	real_dev = vlan_dev_real_dev(dev);
	est = real_dev->oob_context.dev_state.estate;

	if (EVL_WARN_ON(NET, est->refs <= 0))
		return;

	if (--est->refs > 0)
		return;

	/*
	 * Last ref. from a port to a real device dropped. Dismantle
	 * the extension for the latter. Start with unblocking all the
	 * waiters.
	 */

	evl_signal_poll_events(&est->poll_head, POLLERR);
	evl_flush_wait(&est->pool_wait, T_RMID);
	evl_schedule();

	netif_disable_oob_diversion(real_dev);

	evl_stop_kthread(est->rx_handler);
	if (est->tx_handler)
		evl_stop_kthread(est->tx_handler);

	evl_net_dev_purge_pool(real_dev);
	evl_net_free_qdisc(est->qdisc);
	kfree(est);
	real_dev->oob_context.dev_state.estate = NULL;
}

static int switch_oob_port(struct net_device *dev,
			struct evl_netdev_activation *act) /* in-band */
{
	int ret = 0;

	/*
	 * Turn on/off oob port for the device. When set, packets
	 * received by the device flowing through the in-band net core
	 * are diverted to netif_oob_deliver().
	 */
	if (act)
		ret = enable_oob_port(dev, act);
	else
		disable_oob_port(dev);

	return ret;
}

/*
 * in-band, switches the oob port state of the device bound to the
 * socket.
 */
int evl_net_switch_oob_port(struct evl_socket *esk,
			struct evl_netdev_activation *act)
{
	struct net_device *dev;
	int ret;

	dev = esk->proto->get_netif(esk);
	if (dev == NULL)
		return -ENXIO;

	if (act &&
		(act->poolsz > EVL_MAX_NETDEV_POOLSZ ||
		act->bufsz > EVL_MAX_NETDEV_BUFSZ)) {
		ret = -EINVAL;
	} else {
		rtnl_lock();
		ret = switch_oob_port(dev, act);
		rtnl_unlock();
	}

	evl_net_put_dev(dev);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_net_switch_oob_port);

/* Always returns a pointer to a VLAN device. */
struct net_device *evl_net_get_dev_by_index(struct net *net, int ifindex)
{
	struct net_device *dev, *ret = NULL;
	struct oob_netdev_state *nds;
	unsigned long flags;

	if (!ifindex)
		return NULL;

	raw_spin_lock_irqsave(&active_port_lock, flags);

	list_for_each_entry(nds, &active_port_devices, next) {
		dev = container_of(nds, struct net_device,
				oob_context.dev_state);
		if (dev_net(dev) == net && dev->ifindex == ifindex) {
			evl_down_crossing(&nds->crossing);
			ret = dev;
			break;
		}
	}

	raw_spin_unlock_irqrestore(&active_port_lock, flags);

	return ret;
}

void evl_net_get_dev(struct net_device *dev)
{
	struct oob_netdev_state *nds = &dev->oob_context.dev_state;

	evl_down_crossing(&nds->crossing);
}

void evl_net_put_dev(struct net_device *dev)
{
	struct oob_netdev_state *nds = &dev->oob_context.dev_state;

	EVL_WARN_ON(NET, hard_irqs_disabled());

	evl_up_crossing(&nds->crossing);
}

/**
 *	netif_oob_switch_port - switch the oob port state of a VLAN
 *	device.
 *
 *	This call is invoked from the net-sysfs interface in order to
 *	change the state of the oob port of a VLAN device. The default
 *	pool and buffer size are used when enabling.
 *
 *	@dev The network device for which the oob port should be
 *	switched on/off. The request must be issued for a VLAN device.
 *
 *	@enabled The new oob port state.
 *
 *	Returns zero on success, an error code otherwise.
 */
int netif_oob_switch_port(struct net_device *dev, bool enabled)
{
	struct evl_netdev_activation act = {
		.poolsz = 0,
		.bufsz = 0,
	};

	if (!is_vlan_dev(dev))
		return -ENXIO;

	return switch_oob_port(dev, enabled ? &act : NULL);
}

/**
 *	netif_oob_get_port - get the oob diversion state
 *
 *	Returns true if the device is currently diverting traffic to
 *	the oob stage.
 */
bool netif_oob_get_port(struct net_device *dev)
{
	return netdev_is_oob_port(dev);
}

ssize_t netif_oob_query_pool(struct net_device *dev, char *buf)
{
	struct net_device *real_dev = dev;
	struct evl_netdev_state *est;

	if (is_vlan_dev(dev))
		real_dev = vlan_dev_real_dev(dev);

	est = real_dev->oob_context.dev_state.estate;
	if (est == NULL)
		return -ENXIO;

	return sprintf(buf, "%zu %zu %zu\n",
		est->pool_free, est->pool_max, est->buf_size);
}

int evl_netdev_event(struct notifier_block *ev_block,
		     unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	/*
	 * Disable the oob port enabled on a VLAN device before the
	 * latter goes down. rtnl_lock is held.
	 */
	if (event == NETDEV_GOING_DOWN && netdev_is_oob_port(dev))
		disable_oob_port(dev);

	return NOTIFY_DONE;
}
