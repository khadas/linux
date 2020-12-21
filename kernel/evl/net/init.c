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
#include <evl/net/neighbour.h>
#include <evl/net/qdisc.h>
#include <evl/net/packet.h>
#include <evl/net/device.h>
#include <evl/net/input.h>
#include <evl/net/output.h>
#include <evl/net.h>

static struct notifier_block netdev_notifier = {
	.notifier_call = evl_netdev_event
};

int __init evl_net_init(void)
{
	int ret;

	ret = evl_net_init_pools();
	if (ret)
		return ret;

	evl_net_init_tx();

	evl_net_init_qdisc();

	ret = register_netdevice_notifier(&netdev_notifier);
	if (ret)
		goto fail_notifier;

	ret = evl_net_init_neighbour();
	if (ret)
		goto fail_neighbour;

	ret = evl_register_socket_domain(&evl_net_packet);
	if (ret)
		goto fail_domain;

	/* AF_OOB is given no dedicated socket cache. */
	ret = proto_register(&evl_af_oob_proto, 0);
	if (ret)
		goto fail_proto;

	sock_register(&evl_family_ops);

	return 0;

fail_proto:
	evl_unregister_socket_domain(&evl_net_packet);
fail_neighbour:
	unregister_netdevice_notifier(&netdev_notifier);
fail_domain:
	evl_net_cleanup_neighbour();
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
	evl_net_cleanup_neighbour();
	evl_net_cleanup_qdisc();
}
