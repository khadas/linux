/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/mutex.h>
#include <linux/if_ether.h>
#include <linux/err.h>
#include <linux/hashtable.h>
#include <linux/jhash.h>
#include <linux/nsproxy.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <evl/file.h>
#include <evl/factory.h>
#include <evl/uaccess.h>
#include <evl/net.h>
#include <evl/poll.h>
#include <evl/memory.h>
#include <evl/uio.h>
#include <evl/net/offload.h>
#include <evl/net/skb.h>
#include <evl/net/socket.h>
#include <evl/net/device.h>

/*
 * EVL sockets are (almost) regular sockets, extended with out-of-band
 * capabilities. In theory, this would allow us to provide out-of-band
 * services on top of any common protocol already handled by the
 * in-band network stack. EVL-specific protocols belong to the generic
 * PF_OOB family, which we use as a protocol mutiplexor.
 */

#define EVL_DOMAIN_HASH_BITS	8

static DEFINE_HASHTABLE(domain_hash, EVL_DOMAIN_HASH_BITS);

static DEFINE_MUTEX(domain_lock);

struct domain_list_head {
	int af_domain;
	u32 hkey;
	struct hlist_node hash;
	struct list_head list;
};

/*
 * EVL sockets are always bound to an EVL file (see
 * sock_oob_attach()). We may access our extended socket context via
 * filp->f_oob_ctx or sock->sk->sk_oob_ctx, which works for all socket
 * families.
 */
static inline struct evl_socket *evl_sk_from_file(struct file *filp)
{
	return filp->f_oob_ctx ?
		container_of(filp->f_oob_ctx, struct evl_socket, efile) :
		NULL;
}

static inline struct evl_socket *evl_sk(struct sock *sk)
{
	return sk->sk_oob_ctx;
}

static inline u32 get_domain_hash(int af_domain)
{
	u32 hsrc = af_domain;

	return jhash2(&hsrc, 1, 0);
}

/* domain_lock held */
static struct domain_list_head *fetch_domain_list(u32 hkey)
{
	struct domain_list_head *head;

	hash_for_each_possible(domain_hash, head, hash, hkey)
		if (head->hkey == hkey)
			return head;

	return NULL;
}

int evl_register_socket_domain(struct evl_socket_domain *domain)
{
	struct domain_list_head *head;
	u32 hkey;

	inband_context_only();

	hkey = get_domain_hash(domain->af_domain);

	mutex_lock(&domain_lock);

	head = fetch_domain_list(hkey);
	if (head == NULL) {
		head = kzalloc(sizeof(*head), GFP_KERNEL);
		if (head) {
			head->af_domain = domain->af_domain;
			head->hkey = hkey;
			INIT_LIST_HEAD(&head->list);
			hash_add(domain_hash, &head->hash, hkey);
		}
	}

	if (head)	/* Add LIFO to allow for override. */
		list_add(&domain->next, &head->list);

	mutex_unlock(&domain_lock);

	return head ? 0 : -ENOMEM;
}

void evl_unregister_socket_domain(struct evl_socket_domain *domain)
{
	struct domain_list_head *head;
	u32 hkey;

	inband_context_only();

	hkey = get_domain_hash(domain->af_domain);

	mutex_lock(&domain_lock);

	head = fetch_domain_list(hkey);
	if (head) {
		list_del(&domain->next);
		if (list_empty(&head->list)) {
			hash_del(&head->hash);
			kfree(head);
		}
	} else {
		EVL_WARN_ON(NET, 1);
	}

	mutex_unlock(&domain_lock);
}

static inline bool charge_socket_wmem(struct evl_socket *esk, size_t size)
{				/* esk->wmem_wait.wchan.lock held */
	if (atomic_read(&esk->wmem_count) >= esk->wmem_max)
		return false;

	atomic_add(size, &esk->wmem_count);
	evl_down_crossing(&esk->wmem_drain);

	return true;
}

int evl_charge_socket_wmem(struct evl_socket *esk, size_t size,
		ktime_t timeout, enum evl_tmode tmode)
{
	if (!esk->wmem_max)	/* Unlimited. */
		return 0;

	return evl_wait_event_timeout(&esk->wmem_wait, timeout, tmode,
				charge_socket_wmem(esk, size));
}

void evl_uncharge_socket_wmem(struct evl_socket *esk, size_t size)
{
	unsigned long flags;
	int count;

	/*
	 * The tracking socket cannot be stale as it has to pass the
	 * wmem_crossing first before unwinding in sock_oob_destroy().
	 */
	raw_spin_lock_irqsave(&esk->wmem_wait.wchan.lock, flags);

	count = atomic_sub_return(size, &esk->wmem_count);
	if (count < esk->wmem_max && evl_wait_active(&esk->wmem_wait))
		evl_flush_wait_locked(&esk->wmem_wait, 0);

	evl_up_crossing(&esk->wmem_drain);

	raw_spin_unlock_irqrestore(&esk->wmem_wait.wchan.lock, flags);

	EVL_WARN_ON(NET, count < 0);
}

/* in-band */
static struct evl_net_proto *find_oob_proto(int domain, int type, int protocol)
{
	struct evl_net_proto *proto = NULL;
	struct domain_list_head *head;
	struct evl_socket_domain *d;
	u32 hkey;

	hkey = get_domain_hash(domain);

	mutex_lock(&domain_lock);

	head = fetch_domain_list(hkey);
	if (head) {
		list_for_each_entry(d, &head->list, next) {
			if (d->af_domain != domain)
				continue;
			proto = d->match(type, protocol);
			if (proto)
				break;
		}
	}

	mutex_unlock(&domain_lock);

	return proto;
}

/*
 * The inband offload handler. Handles packets for which we cannot
 * handle from the oob stage directly (e.g. because we don't have the
 * routing information available in our oob front-cache).
 */
static void inband_offload_handler(struct evl_work *work)
{
	struct evl_socket *esk =
		container_of(work, struct evl_socket, inband_offload);

	if (EVL_WARN_ON(NET, !esk->proto->handle_offload))
		return;

	esk->proto->handle_offload(esk);

	/* Release the ref. obtained by evl_net_offload_inband(). */
	evl_put_file(&esk->efile);
}

/*
 * Offload a protocol-specific operation to the in-band stage.
 */
void evl_net_offload_inband(struct evl_socket *esk,
			struct evl_net_offload *ofld,
			struct list_head *q)
{
	unsigned long flags;

	/*
	 * Make sure esk won't vanish until the offload handler has
	 * run.
	 */
	evl_get_fileref(&esk->efile);

	raw_spin_lock_irqsave(&esk->oob_lock, flags);
	list_add_tail(&ofld->next, q);
	raw_spin_unlock_irqrestore(&esk->oob_lock, flags);

	if (!evl_call_inband(&esk->inband_offload))
		evl_put_file(&esk->efile);
}

/*
 * In-band call from the common network stack creating a new BSD
 * socket, @sock is already bound to a file. We know the following:
 *
 * - the caller wants us either to attach an out-of-band extension to
 *   a common protocol (e.g. AF_PACKET over ethernet), or to set up an
 *   mere AF_OOB socket for EVL-specific protocols.
 *
 * - we have no oob extension context for @sock yet
 *   (sock->sk->sk_oob_ctx is NULL)
 */
int sock_oob_attach(struct socket *sock)
{
	struct evl_net_proto *proto;
	struct sock *sk = sock->sk;
	struct evl_socket *esk;
	int ret;

	/*
	 * Try finding a suitable out-of-band protocol among those
	 * registered in EVL.
	 */
	proto = find_oob_proto(sk->sk_family, sk->sk_type, sk->sk_protocol);
	if (proto == NULL)
		return -EPROTONOSUPPORT;

	/*
	 * We might support a protocol, but we might not be happy with
	 * the socket type (e.g. AF_PACKET mandates SOCK_RAW).
	 */
	if (IS_ERR(proto))
		return PTR_ERR(proto);

	/*
	 * If sk->sk_family is not PF_OOB, we have no extended oob
	 * context yet, allocate one to piggyback on a common socket.
	 */
	if (sk->sk_family != PF_OOB) {
		esk = kzalloc(sizeof(*esk), GFP_KERNEL);
		if (esk == NULL)
			return -ENOMEM;
		refcount_set(&esk->refs, 2); /* release + destroy */
	} else {
		esk = (struct evl_socket *)sk;
		refcount_set(&esk->refs, 1); /* release only */
	}

	esk->sk = sk;

	/*
	 * Bind the underlying socket file to an EVL file, which
	 * enables out-of-band I/O requests for that socket.
	 */
	ret = evl_open_file(&esk->efile, sock->file);
	if (ret)
		goto fail_open;

	/*
	 * In-band wise, the host socket is fully initialized, so the
	 * in-band network stack already holds a ref. on the net
	 * struct for that socket.
	 */
	esk->net = sock_net(sk);
	mutex_init(&esk->lock);
	INIT_LIST_HEAD(&esk->input);
	INIT_LIST_HEAD(&esk->next_sub);
	evl_init_wait(&esk->input_wait, &evl_mono_clock, 0);
	evl_init_wait(&esk->wmem_wait, &evl_mono_clock, 0);
	evl_init_poll_head(&esk->poll_head);
	raw_spin_lock_init(&esk->oob_lock);
	evl_init_work(&esk->inband_offload, inband_offload_handler);
	/* Inherit the {rw}mem limits from the base socket. */
	esk->rmem_max = sk->sk_rcvbuf;
	esk->wmem_max = sk->sk_sndbuf;
	evl_init_crossing(&esk->wmem_drain);

	ret = proto->attach(esk, proto, ntohs(sk->sk_protocol));
	if (ret)
		goto fail_attach;

	sk->sk_oob_ctx = esk;

	return 0;

fail_attach:
	evl_release_file(&esk->efile);
fail_open:
	if (sk->sk_family != PF_OOB)
		kfree(esk);

	return ret;
}

/*
 * In-band call from the common network stack releasing a BSD socket,
 * @sock is still bound to a file, but the network representation
 * sock->sk might be stale.
 */
void sock_oob_release(struct socket *sock)
{
	struct evl_socket *esk = evl_sk_from_file(sock->file);

	if (esk->proto->release)
		esk->proto->release(esk);

	evl_release_file(&esk->efile);
	/* Wait for the stack to drain in-flight outgoing buffers. */
	evl_pass_crossing(&esk->wmem_drain);

	if (refcount_dec_and_test(&esk->refs))
		kfree(esk);
}

/*
 * In-band call from the common network stack which is about to
 * destruct a socket, releasing all resources attached (@sock is
 * out-of-band capable).
 */
void sock_oob_destroy(struct sock *sk)
{
	struct evl_socket *esk = evl_sk(sk);

	/* We are detaching, so rmem_count can be left out of sync. */
	evl_net_free_skb_list(&esk->input);

	evl_destroy_wait(&esk->input_wait);
	evl_destroy_wait(&esk->wmem_wait);

	if (esk->proto->destroy)
		esk->proto->destroy(esk);

	if (sk->sk_family != PF_OOB && refcount_dec_and_test(&esk->refs))
		kfree(esk);	/* meaning sk != esk. */

	sk->sk_oob_ctx = NULL;
}

/*
 * In-band call from the common network stack to complete a binding
 * (@sock is out-of-band capable). We end up here _after_ a successful
 * binding of the network socket to the given address by the in-band
 * stack.
 */
int sock_oob_bind(struct sock *sk, struct sockaddr *addr, int len)
{
	struct evl_socket *esk = evl_sk(sk);

	/*
	 * If @sk belongs to PF_OOB, then evl_sock_bind() already
	 * handled the binding. We only care about common protocols
	 * for which we have an out-of-band extension
	 * (e.g. AF_PACKET).
	 */
	if (sk->sk_family == PF_OOB || !esk->proto->bind)
		return 0;

	return esk->proto->bind(esk, addr, len);
}

/*
 * In-band call from the common network stack to shutdown the
 * socket. We end up here _after_ the socket was successfully shut
 * down by the in-band network stack.
 */
int sock_oob_shutdown(struct sock *sk, int how)
{
	struct evl_socket *esk = evl_sk(sk);

	/*
	 * If @sk belongs to PF_OOB, then evl_sock_shutdown() already
	 * handled the connection. We only care about common protocols
	 * for which we have an out-of-band extension
	 * (e.g. AF_INET/IPPROTO_UDP).
	 */
	if (sk->sk_family == PF_OOB || !esk->proto->shutdown)
		return 0;

	return esk->proto->shutdown(esk, how);
}

/*
 * In-band call from the common network stack to connect the socket.
 * We end up here _after_ a successful connection of the network
 * socket to the given address by the in-band stack.
 */
int sock_oob_connect(struct sock *sk,
		struct sockaddr *addr, int len, int flags)
{
	struct evl_socket *esk = evl_sk(sk);

	/*
	 * If @sk belongs to PF_OOB, then evl_sock_connect() already
	 * handled the connection. We only care about common protocols
	 * for which we have an out-of-band extension
	 * (e.g. AF_INET/IPPROTO_UDP).
	 */
	if (sk->sk_family == PF_OOB || !esk->proto->connect)
		return 0;

	return esk->proto->connect(esk, addr, len, flags);
}

static int socket_send_recv(struct evl_socket *esk,
			struct user_oob_msghdr __user *u_msghdr,
			unsigned int cmd)
{
	struct iovec fast_iov[UIO_FASTIOV], *iov, __user *u_iov;
	__u64 iov_ptr;
	__u32 iovlen;
	__s32 count;
	int ret;

	ret = raw_get_user(iov_ptr, &u_msghdr->iov_ptr);
	if (ret)
		return -EFAULT;

	ret = raw_get_user(iovlen, &u_msghdr->iovlen);
	if (ret)
		return -EFAULT;

	u_iov = evl_valptr64(iov_ptr, struct iovec);
	iov = evl_load_uio(u_iov, iovlen, fast_iov);
	if (IS_ERR(iov))
		return PTR_ERR(iov);

	if (cmd == EVL_SOCKIOC_SENDMSG)
		count = esk->proto->oob_send(esk, u_msghdr, iov, iovlen);
	else
		count = esk->proto->oob_receive(esk, u_msghdr, iov, iovlen);

	if (iov != fast_iov)
		evl_free(iov);

	if (count < 0)
		return count;

	ret = raw_put_user(count, &u_msghdr->count);
	if (ret)
		return -EFAULT;

	return 0;
}

long sock_oob_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct evl_socket *esk = evl_sk_from_file(filp);
	struct user_oob_msghdr __user *u_msghdr;
	long ret;

	if (esk == NULL)
		return -EBADFD;

	switch (cmd) {
	case EVL_SOCKIOC_SENDMSG:
	case EVL_SOCKIOC_RECVMSG:
		u_msghdr = (typeof(u_msghdr))arg;
		ret = socket_send_recv(esk, u_msghdr, cmd);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

ssize_t sock_oob_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_socket *esk = evl_sk_from_file(filp);
	struct iovec iov;

	if (esk == NULL)
		return -EBADFD;

	if (!count)
		return 0;

	iov.iov_base = (void *)u_buf;
	iov.iov_len = count;

	return esk->proto->oob_send(esk, NULL, &iov, 1);
}

ssize_t sock_oob_read(struct file *filp,
			char __user *u_buf, size_t count)
{
	struct evl_socket *esk = evl_sk_from_file(filp);
	struct iovec iov;

	if (esk == NULL)
		return -EBADFD;

	if (!count)
		return 0;

	iov.iov_base = u_buf;
	iov.iov_len = count;

	return esk->proto->oob_receive(esk, NULL, &iov, 1);
}

__poll_t sock_oob_poll(struct file *filp,
			struct oob_poll_wait *wait)
{
	struct evl_socket *esk = evl_sk_from_file(filp);

	if (esk == NULL)
		return -EBADFD;

	return esk->proto->oob_poll(esk, wait);
}

static int socket_set_rmem(struct evl_socket *esk, int __user *u_val)
{
	int ret, val;

	ret = raw_get_user(val, u_val);
	if (ret)
		return -EFAULT;

	/* Same logic as __sock_set_rcvbuf(). */
	val = min_t(int, val, INT_MAX / 2);
	WRITE_ONCE(esk->rmem_max, max_t(int, val * 2, SOCK_MIN_RCVBUF));

	return 0;
}

static int socket_set_wmem(struct evl_socket *esk, int __user *u_val)
{
	int ret, val;

	ret = raw_get_user(val, u_val);
	if (ret)
		return -EFAULT;

	val = min_t(int, val, INT_MAX / 2);
	WRITE_ONCE(esk->wmem_max, max_t(int, val * 2, SOCK_MIN_SNDBUF));

	return 0;
}

static int sock_inband_ioctl(struct sock *sk, unsigned int cmd,
			unsigned long arg)
{
	struct evl_socket *esk = evl_sk(sk);
	struct evl_netdev_activation act, __user *u_act;
	int __user *u_val;
	int ret;

	switch (cmd) {
	case EVL_SOCKIOC_ACTIVATE: /* Turn oob port on. */
		u_act = (typeof(u_act))arg;
		ret = raw_copy_from_user(&act, u_act, sizeof(act));
		if (ret)
			return -EFAULT;
		ret = evl_net_switch_oob_port(esk, &act);
		break;
	case EVL_SOCKIOC_DEACTIVATE: /* Turn oob port off. */
		ret = evl_net_switch_oob_port(esk, NULL);
 		break;
	case EVL_SOCKIOC_SETRECVSZ:
		u_val = (typeof(u_val))arg;
		ret = socket_set_rmem(esk, u_val);
		break;
	case EVL_SOCKIOC_SETSENDSZ:
		u_val = (typeof(u_val))arg;
		ret = socket_set_wmem(esk, u_val);
		break;
	default:
		ret = -ENOTTY;
		if (esk->proto->ioctl)
			ret = esk->proto->ioctl(esk, cmd, arg);
	}

	return ret;
}

/*
 * Ioctl redirector for common protocols with oob extension. AF_OOB
 * jumps directly to sock_ioctl() via the netproto ops instead. If the
 * out-of-band protocol implementation was not able to handle the
 * EVL-specific command, we should return -ENOIOCTLCMD to the caller,
 * so that it tries harder to find a suitable handler.
 */
long sock_inband_ioctl_redirect(struct sock *sk, /* in-band hook */
				unsigned int cmd, unsigned long arg)
{
	long ret = sock_inband_ioctl(sk, cmd, arg);

	return ret == -ENOTTY ? -ENOIOCTLCMD : ret;
}

static int evl_sock_ioctl(struct socket *sock, unsigned int cmd,
			unsigned long arg)
{
	return sock_inband_ioctl(sock->sk, cmd, arg);
}

static int evl_sock_bind(struct socket *sock, struct sockaddr *u_addr, int len)
{
	struct evl_socket *esk = evl_sk(sock->sk);

	return esk->proto->bind(esk, u_addr, len);
}

static int evl_sock_connect(struct socket *sock,
			struct sockaddr *u_addr, int len, int flags)
{
	struct evl_socket *esk = evl_sk(sock->sk);

	return esk->proto->connect(esk, u_addr, len, flags);
}

static int evl_sock_shutdown(struct socket *sock, int how)
{
	struct evl_socket *esk = evl_sk(sock->sk);

	return esk->proto->shutdown(esk, how);
}

static int evl_sock_release(struct socket *sock)
{
	/*
	 * Cleanup happens from sock_oob_destroy(), so that PF_OOB and
	 * common protocols sockets we piggybacked on are released.
	 */
	return 0;
}

static const struct proto_ops netproto_ops = {
	.family =	PF_OOB,
	.owner =	THIS_MODULE,
	.release =	evl_sock_release,
	.bind =		evl_sock_bind,
	.connect =	evl_sock_connect,
	.shutdown =	evl_sock_shutdown,
	.ioctl =	evl_sock_ioctl,
	.socketpair =	sock_no_socketpair,
	.accept =	sock_no_accept,
	.getname =	sock_no_getname,
	.listen =	sock_no_listen,
	.sendmsg =	sock_no_sendmsg,
	.recvmsg =	sock_no_recvmsg,
	.mmap =		sock_no_mmap,
};

/*
 * A generic family for protocols implemented by the companion
 * core. user<->evl interaction is possible only through the
 * oob_read/oob_write/oob_ioctl/ioctl calls.
 */
struct proto evl_af_oob_proto = {
	.name		= "EVL",
	.owner		= THIS_MODULE,
	.obj_size	= sizeof(struct evl_socket),
};

static void destroy_evl_socket(struct sock *sk)
{
	local_bh_disable();
	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
	local_bh_enable();
}

static int create_evl_socket(struct net *net, struct socket *sock,
			     int protocol, int kern)
{
	struct sock *sk;

	if (kern)
		return -EOPNOTSUPP;

	sock->state = SS_UNCONNECTED;

	sk = sk_alloc(net, PF_OOB, GFP_KERNEL, &evl_af_oob_proto, 0);
	if (!sk)
		return -ENOBUFS;

	sock->ops = &netproto_ops;
	sock_init_data(sock, sk);

	/*
	 * Protocol is checked for validity when the socket is
	 * attached to the out-of-band core in sock_oob_attach().
	 */
	sk->sk_protocol = htons(protocol);
	sk->sk_destruct	= destroy_evl_socket;

	local_bh_disable();
	sock_prot_inuse_add(net, &evl_af_oob_proto, 1);
	local_bh_enable();

	return 0;
}

const struct net_proto_family evl_family_ops = {
	.family = PF_OOB,
	.create = create_evl_socket,
	.owner	= THIS_MODULE,
};
