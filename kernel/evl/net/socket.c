/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/mutex.h>
#include <linux/if_ether.h>
#include <linux/uio.h>
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
 * filp->oob_data or sock->sk->oob_data, which works for all socket
 * families.
 */
static inline struct evl_socket *evl_sk_from_file(struct file *filp)
{
	return filp->oob_data ?
		container_of(filp->oob_data, struct evl_socket, efile) :
		NULL;
}

static inline struct evl_socket *evl_sk(struct socket *sock)
{
	return sock->sk->oob_data;
}

static int load_iov_native(struct iovec *iov,
			const struct iovec __user *u_iov,
			size_t iovlen)
{
	return raw_copy_from_user(iov, u_iov, iovlen * sizeof(*u_iov)) ?
		-EFAULT : 0;
}

#ifdef CONFIG_COMPAT

int do_load_iov(struct iovec *iov,
		const struct iovec __user *u_iov,
		size_t iovlen)
{
	struct compat_iovec c_iov[UIO_FASTIOV], __user *uc_iov;
	size_t nvec = 0;
	int ret, n, i;

	if (likely(!is_compat_oob_call()))
		return load_iov_native(iov, u_iov, iovlen);

	uc_iov = (struct compat_iovec *)u_iov;

	/*
	 * Slurp compat_iovector in by chunks of UIO_FASTIOV
	 * cells. This is faster in the most likely case compared to
	 * allocating yet another in-kernel vector dynamically for
	 * such purpose.
	 */
	while (nvec < iovlen) {
		n = iovlen - nvec;
		if (n > UIO_FASTIOV)
			n = UIO_FASTIOV;
		ret = raw_copy_from_user(c_iov, uc_iov, sizeof(*uc_iov) * n);
		if (ret)
			return -EFAULT;
		for (i = 0; i < n; i++, iov++) {
			iov->iov_base = compat_ptr(c_iov[i].iov_base);
			iov->iov_len = c_iov[i].iov_len;
		}
		uc_iov += n;
		nvec += n;
	}

	return 0;
}

#else

static inline int do_load_iov(struct iovec *iov,
			const struct iovec __user *u_iov,
			size_t iovlen)
{
	return load_iov_native(iov, u_iov, iovlen);
}

#endif

static struct iovec *
load_iov(const struct iovec __user *u_iov, size_t iovlen,
	struct iovec *fast_iov)
{
	struct iovec *slow_iov;
	int ret;

	if (iovlen > UIO_MAXIOV)
		return ERR_PTR(-EINVAL);

	if (likely(iovlen <= UIO_FASTIOV)) {
		ret = do_load_iov(fast_iov, u_iov, iovlen);
		return ret ? ERR_PTR(ret) : fast_iov;
	}

	slow_iov = evl_alloc(iovlen * sizeof(*u_iov));
	if (slow_iov == NULL)
		return ERR_PTR(-ENOMEM);

	ret = do_load_iov(slow_iov, u_iov, iovlen);
	if (ret) {
		evl_free(slow_iov);
		return ERR_PTR(ret);
	}

	return slow_iov;
}

ssize_t evl_export_iov(const struct iovec *iov, size_t iovlen,
		const void *data, size_t len)
{
	ssize_t written = 0;
	size_t nbytes;
	int n, ret;

	for (n = 0; len > 0 && n < iovlen; n++, iov++) {
		if (iov->iov_len == 0)
			continue;

		nbytes = iov->iov_len;
		if (nbytes > len)
			nbytes = len;

		ret = raw_copy_to_user(iov->iov_base, data, nbytes);
		if (ret)
			return -EFAULT;

		len -= nbytes;
		data += nbytes;
		written += nbytes;
		if (written < 0)
			return -EINVAL;
	}

	return written;
}
EXPORT_SYMBOL_GPL(evl_export_iov);

ssize_t evl_import_iov(const struct iovec *iov, size_t iovlen,
		       void *data, size_t len, size_t *remainder)
{
	size_t nbytes, avail = 0;
	ssize_t read = 0;
	int n, ret;

	for (n = 0; len > 0 && n < iovlen; n++, iov++) {
		if (iov->iov_len == 0)
			continue;

		nbytes = iov->iov_len;
		avail += nbytes;
		if (nbytes > len)
			nbytes = len;

		ret = raw_copy_from_user(data, iov->iov_base, nbytes);
		if (ret)
			return -EFAULT;

		len -= nbytes;
		data += nbytes;
		read += nbytes;
		if (read < 0)
			return -EINVAL;
	}

	if (remainder) {
		for (; n < iovlen; n++, iov++)
			avail += iov->iov_len;
		*remainder = avail - read;
	}

	return read;
}
EXPORT_SYMBOL_GPL(evl_import_iov);

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

static inline bool charge_socket_wmem(struct evl_socket *esk,
				struct sk_buff *skb)
{				/* esk->wmem_wait.lock held */
	if (atomic_read(&esk->wmem_count) >= esk->wmem_max)
		return false;

	atomic_add(skb->truesize, &esk->wmem_count);
	EVL_NET_CB(skb)->tracker = esk;
	evl_down_crossing(&esk->wmem_drain);

	return true;
}

int evl_charge_socket_wmem(struct evl_socket *esk,
			struct sk_buff *skb,
			ktime_t timeout, enum evl_tmode tmode)
{
	EVL_NET_CB(skb)->tracker = NULL;

	if (!esk->wmem_max)	/* Unlimited. */
		return 0;

	return evl_wait_event_timeout(&esk->wmem_wait,
				timeout, tmode,
				charge_socket_wmem(esk, skb));
}

void evl_uncharge_socket_wmem(struct sk_buff *skb)
{
	struct evl_socket *esk = EVL_NET_CB(skb)->tracker;
	unsigned long flags;
	int count;

	if (!esk)
		return;

	/*
	 * The tracking socket cannot be stale as it has to pass the
	 * wmem_crossing first before unwinding in sock_oob_detach().
	 */
	raw_spin_lock_irqsave(&esk->wmem_wait.lock, flags);

	EVL_NET_CB(skb)->tracker = NULL;

	count = atomic_sub_return(skb->truesize, &esk->wmem_count);
	if (count < esk->wmem_max && evl_wait_active(&esk->wmem_wait))
		evl_flush_wait_locked(&esk->wmem_wait, 0);

	evl_up_crossing(&esk->wmem_drain);

	raw_spin_unlock_irqrestore(&esk->wmem_wait.lock, flags);

	EVL_WARN_ON(NET, count < 0);
}

/* in-band */
static struct evl_net_proto *
find_oob_proto(int domain, int type, __be16 protocol)
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
 * In-band call from the common network stack creating a new socket,
 * @sock is already bound to a file. We know the following:
 *
 * - the caller wants us either to attach an out-of-band extension to
 *   a common protocol (e.g. AF_PACKET over ethernet), or to set up an
 *   mere AF_OOB socket for EVL-specific protocols.
 *
 * - we have no oob extension context for @sock yet
 *   (sock->sk->oob_data is NULL)
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
	} else {
		esk = (struct evl_socket *)sk;
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
	esk->net = sock_net(sock->sk);
	mutex_init(&esk->lock);
	INIT_LIST_HEAD(&esk->input);
	INIT_LIST_HEAD(&esk->next_sub);
	evl_init_wait(&esk->input_wait, &evl_mono_clock, 0);
	evl_init_wait(&esk->wmem_wait, &evl_mono_clock, 0);
	evl_init_poll_head(&esk->poll_head);
	raw_spin_lock_init(&esk->oob_lock);
	/* Inherit the {rw}mem limits from the base socket. */
	esk->rmem_max = sk->sk_rcvbuf;
	esk->wmem_max = sk->sk_sndbuf;
	evl_init_crossing(&esk->wmem_drain);

	ret = proto->attach(esk, proto, sk->sk_protocol);
	if (ret)
		goto fail_attach;

	sk->oob_data = esk;

	return 0;

fail_attach:
	evl_release_file(&esk->efile);
fail_open:
	if (sk->sk_family != PF_OOB)
		kfree(esk);

	return ret;
}

/*
 * In-band call from the common network stack which is about to
 * release a socket (@sock is out-of-band capable).
 */
void sock_oob_detach(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct evl_socket *esk = evl_sk(sock);

	evl_release_file(&esk->efile);
	/* Wait for the stack to drain in-flight outgoing buffers. */
	evl_pass_crossing(&esk->wmem_drain);
	/* We are detaching, so rmem_count can be left out of sync. */
	evl_net_free_skb_list(&esk->input);

	if (esk->proto->detach)
		esk->proto->detach(esk);

	if (sk->sk_family != PF_OOB)
		kfree(esk);	/* i.e. sk->oob_data. */

	sk->oob_data = NULL;
}

/*
 * In-band call from the common network stack (@sock is out-of-band
 * capable). We end up here after a socket was successful bound to the
 * given address by the common network stack, so that we can do some
 * EVL-specific work on top of that.
 */
int sock_oob_bind(struct socket *sock, struct sockaddr *addr, int len)
{
	struct sock *sk = sock->sk;
	struct evl_socket *esk = evl_sk(sock);

	/*
	 * If @sock belongs to PF_OOB, then evl_sock_bind() already
	 * handled the binding. We ony care about common protocols
	 * for which we have an out-of-band extension
	 * (e.g. AF_PACKET).
	 */
	if (sk->sk_family == PF_OOB)
		return 0;

	return esk->proto->bind(esk, addr, len);
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
	iov = load_iov(u_iov, iovlen, fast_iov);
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

static int sock_inband_ioctl(struct socket *sock, unsigned int cmd,
			unsigned long arg)
{
	struct sock *sk = sock->sk;
	struct evl_socket *esk = sk->oob_data;
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
long sock_inband_ioctl_redirect(struct socket *sock, /* in-band hook */
				unsigned int cmd, unsigned long arg)
{
	long ret = sock_inband_ioctl(sock, cmd, arg);

	return ret == -ENOTTY ? -ENOIOCTLCMD : ret;
}

static int evl_sock_bind(struct socket *sock, struct sockaddr *u_addr, int len)
{
	struct evl_socket *esk = evl_sk(sock);

	return esk->proto->bind(esk, u_addr, len);
}

static int evl_sock_release(struct socket *sock)
{
	/*
	 * Cleanup happens from sock_oob_detach(), so that PF_OOB
	 * and common protocols sockets we piggybacked on are
	 * released.
	 */
	return 0;
}

static const struct proto_ops netproto_ops = {
	.family =	PF_OOB,
	.owner =	THIS_MODULE,
	.release =	evl_sock_release,
	.bind =		evl_sock_bind,
	.connect =	sock_no_connect,
	.socketpair =	sock_no_socketpair,
	.accept =	sock_no_accept,
	.getname =	sock_no_getname,
	.ioctl =	sock_inband_ioctl,
	.listen =	sock_no_listen,
	.shutdown =	sock_no_shutdown,
	.sendmsg =	sock_no_sendmsg,
	.recvmsg =	sock_no_recvmsg,
	.mmap =		sock_no_mmap,
	.sendpage =	sock_no_sendpage,
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

	sk_refcnt_debug_dec(sk);
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
	sk->sk_protocol = protocol;
	sk_refcnt_debug_inc(sk);
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
