/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/nsproxy.h>
#include <net/net_namespace.h>
#include <evl/factory.h>
#include <evl/uaccess.h>
#include <evl/net/qdisc.h>
#include <evl/net/packet.h>
#include <evl/net/device.h>
#include <evl/net/input.h>
#include <evl/net/output.h>
#include <evl/net/skb.h>
#include <evl/net/ipv4/arp.h>
#include <evl/net/ipv4/route.h>
#include <evl/net/ipv4.h>
#include <evl/net.h>

/*
 * Called by the in-band stack to setup the oob state which is going
 * to be maintained by EVL in a network namespace.
 */
void net_init_oob_state(struct net *net)
{
	evl_net_init_ipv4(net);
}

/*
 * Converse to net_init_oob_state(), called to cleanup the oob state
 * which is being dismanted by the in-band stack.
 */
void net_cleanup_oob_state(struct net *net)
{
	evl_net_cleanup_ipv4(net);
}

static struct notifier_block netdev_notifier = {
	.notifier_call = evl_netdev_event
};

int __init evl_net_init(void)
{
	int ret;

	evl_net_init_tx();

	evl_net_init_qdisc();

	ret = register_netdevice_notifier(&netdev_notifier);
	if (ret)
		goto fail_notifier;

	ret = evl_register_socket_domain(&evl_net_packet);
	if (ret)
		goto fail_packet;

	ret = evl_register_socket_domain(&evl_net_ipv4);
	if (ret)
		goto fail_ipv4;

	/* AF_OOB is given no dedicated socket cache. */
	ret = proto_register(&evl_af_oob_proto, 0);
	if (ret)
		goto fail_proto;

	sock_register(&evl_family_ops);

	return 0;

fail_proto:
	evl_unregister_socket_domain(&evl_net_ipv4);
fail_ipv4:
	evl_unregister_socket_domain(&evl_net_packet);
fail_packet:
	unregister_netdevice_notifier(&netdev_notifier);
fail_notifier:
	evl_net_cleanup_qdisc();

	return ret;
}

void __init evl_net_cleanup(void)
{
	sock_unregister(PF_OOB);
	proto_unregister(&evl_af_oob_proto);
	evl_unregister_socket_domain(&evl_net_packet);
	unregister_netdevice_notifier(&netdev_notifier);
	evl_net_cleanup_qdisc();
}

static long net_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct evl_net_devfd req, __user *u_req;
	const char __user *u_name;
	struct filename *devname;
	long ret;
	int ufd;

	switch (cmd) {
	case EVL_NET_GETDEVFD:
		u_req = (typeof(u_req))arg;
		ret = copy_from_user(&req, u_req, sizeof(req));
		if (ret)
			return -EFAULT;
		u_name = evl_valptr64(req.name_ptr, const char);
		devname = getname(u_name);
		if (IS_ERR(devname))
			return PTR_ERR(devname);
		ufd = evl_net_dev_allocfd(current->nsproxy->net_ns, devname->name);
		putname(devname);
		if (ufd < 0)
			return ufd;
		ret = put_user((__u32)ufd, &u_req->fd);
		if (ret)
			return -EFAULT;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static const struct file_operations net_fops = {
	.unlocked_ioctl	= net_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
#endif
};

static ssize_t vlans_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return evl_net_show_vlans(buf, PAGE_SIZE);
}

static ssize_t vlans_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return evl_net_store_vlans(buf, count);
}
static DEVICE_ATTR_RW(vlans);

static ssize_t ipv4_routes_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct net *net = current->nsproxy->net_ns;

	evl_net_flush_ipv4_routes(net, NULL);

	return count;
}
static DEVICE_ATTR_WO(ipv4_routes);

static ssize_t arp_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct net *net = current->nsproxy->net_ns;

	evl_net_flush_arp(net);

	return count;
}
static DEVICE_ATTR_WO(arp);

static struct attribute *net_attrs[] = {
	&dev_attr_vlans.attr,
	&dev_attr_ipv4_routes.attr,
	&dev_attr_arp.attr,
	NULL,
};
ATTRIBUTE_GROUPS(net);

struct evl_factory evl_net_factory = {
	.name	=	"net",
	.fops	=	&net_fops,
	.attrs	=	net_groups,
	.flags	=	EVL_FACTORY_SINGLE,
};
