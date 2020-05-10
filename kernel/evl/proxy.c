/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/log2.h>
#include <linux/irq_work.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <evl/factory.h>
#include <evl/work.h>
#include <evl/flag.h>
#include <evl/poll.h>
#include <uapi/evl/proxy.h>

#define EVL_PROXY_CLONE_FLAGS	\
	(EVL_CLONE_PUBLIC|EVL_CLONE_OUTPUT|EVL_CLONE_INPUT)

struct proxy_ring {
	void *bufmem;
	atomic_t fillsz;
	int nesting;
	unsigned int bufsz;
	unsigned int rdoff;
	unsigned int wroff;
	unsigned int reserved;
	unsigned int granularity;
	struct evl_flag oob_wait;
	wait_queue_head_t inband_wait;
	struct evl_work relay_work;
	hard_spinlock_t lock;
	struct workqueue_struct *wq;
	struct mutex worker_lock;
};

struct proxy_out {		/* oob_write->write */
	struct proxy_ring ring;
};

struct proxy_in {		/* read->oob_read */
	struct proxy_ring ring;
	atomic_t reqsz;
	atomic_t on_eof;
	int on_error;
};

struct evl_proxy {
	struct file *filp;
	struct proxy_out output;
	struct proxy_in input;
	struct evl_element element;
	struct evl_poll_head poll_head;
};

static inline bool proxy_is_readable(struct evl_proxy *proxy)
{
	return !!(proxy->element.clone_flags & EVL_CLONE_INPUT);
}

static inline bool proxy_is_writable(struct evl_proxy *proxy)
{
	return !!(proxy->element.clone_flags & EVL_CLONE_OUTPUT);
}

static void relay_output(struct evl_proxy *proxy)
{
	struct proxy_ring *ring = &proxy->output.ring;
	unsigned int rdoff, count, len, n;
	struct file *filp = proxy->filp;
	loff_t pos, *ppos;
	ssize_t ret = 0;

	mutex_lock(&ring->worker_lock);

	count = atomic_read(&ring->fillsz);
	rdoff = ring->rdoff;

	ppos = NULL;
	if (!(filp->f_mode & FMODE_STREAM)) {
		mutex_lock(&filp->f_pos_lock);
		ppos = &pos;
		pos = filp->f_pos;
	}

	while (count > 0 && ret >= 0) {
		len = count;
		do {
			if (rdoff + len > ring->bufsz)
				n = ring->bufsz - rdoff;
			else
				n = len;

			if (ring->granularity > 0)
				n = min(n, ring->granularity);

			ret = kernel_write(filp, ring->bufmem + rdoff, n, ppos);
			if (ret >= 0 && ppos)
				filp->f_pos = *ppos;
			len -= n;
			rdoff = (rdoff + n) % ring->bufsz;
		} while (len > 0 && ret > 0);
		/*
		 * On error, the portion we failed writing is
		 * lost. Fair enough.
		 */
		count = atomic_sub_return(count, &ring->fillsz);
	}

	if (ppos)
		mutex_unlock(&filp->f_pos_lock);

	ring->rdoff = rdoff;

	mutex_unlock(&ring->worker_lock);

	/*
	 * For proxies, writability means that all pending data was
	 * sent out without error.
	 */
	if (count == 0)
		evl_signal_poll_events(&proxy->poll_head, POLLOUT|POLLWRNORM);

	/*
	 * Since we are running in-band, make sure to give precedence
	 * to oob waiters for wakeups.
	 */
	if (count < ring->bufsz) {
		evl_raise_flag(&ring->oob_wait); /* Reschedules. */
		wake_up(&ring->inband_wait);
	} else
		evl_schedule();	/* Covers evl_signal_poll_events() */
}

static void relay_output_work(struct evl_work *work)
{
	struct evl_proxy *proxy =
		container_of(work, struct evl_proxy, output.ring.relay_work);

	relay_output(proxy);
}

static bool can_write_buffer(struct proxy_ring *ring, size_t size)
{
	return atomic_read(&ring->fillsz) +
		ring->reserved + size <= ring->bufsz;
}

static ssize_t do_proxy_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_ring *ring = &proxy->output.ring;
	unsigned int wroff, wbytes, n, rsvd;
	unsigned long flags;
	ssize_t ret;
	int xret;

	if (count == 0)
		return 0;

	if (count > ring->bufsz)
		return -EFBIG;

	if (ring->granularity > 1 && count % ring->granularity > 0)
		return -EINVAL;

	raw_spin_lock_irqsave(&ring->lock, flags);

	/* No short or scattered writes. */
	if (!can_write_buffer(ring, count)) {
		ret = -EAGAIN;
		goto out;
	}

	/* Reserve a write slot into the circular buffer. */
	wroff = ring->wroff;
	ring->wroff = (wroff + count) % ring->bufsz;
	ring->nesting++;
	ring->reserved += count;
	wbytes = ret = count;

	do {
		if (wroff + wbytes > ring->bufsz)
			n = ring->bufsz - wroff;
		else
			n = wbytes;

		raw_spin_unlock_irqrestore(&ring->lock, flags);
		xret = raw_copy_from_user(ring->bufmem + wroff, u_buf, n);
		raw_spin_lock_irqsave(&ring->lock, flags);
		if (xret) {
			memset(ring->bufmem + wroff + n - xret, 0, xret);
			ret = -EFAULT;
			break;
		}

		u_buf += n;
		wbytes -= n;
		wroff = (wroff + n) % ring->bufsz;
	} while (wbytes > 0);

	if (--ring->nesting == 0) {
		n = atomic_add_return(ring->reserved, &ring->fillsz);
		rsvd = ring->reserved;
		ring->reserved = 0;
		if (n == rsvd) { /* empty -> non-empty transition */
			if (running_inband()) {
				raw_spin_unlock_irqrestore(&ring->lock, flags);
				relay_output(proxy);
				return ret;
			}
			evl_call_inband_from(&ring->relay_work, ring->wq);
		}
	}
out:
	raw_spin_unlock_irqrestore(&ring->lock, flags);

	return ret;
}

static void relay_input(struct evl_proxy *proxy)
{
	struct proxy_in *in = &proxy->input;
	struct proxy_ring *ring = &in->ring;
	unsigned int wroff, count, len, n;
	struct file *filp = proxy->filp;
	bool exception = false;
	loff_t pos, *ppos;
	ssize_t ret = 0;

	mutex_lock(&ring->worker_lock);

	count = atomic_read(&in->reqsz);
	wroff = ring->wroff;

	ppos = NULL;
	if (!(filp->f_mode & FMODE_STREAM)) {
		mutex_lock(&filp->f_pos_lock);
		ppos = &pos;
		pos = filp->f_pos;
	}

	while (count > 0) {
		len = count;
		do {
			if (wroff + len > ring->bufsz)
				n = ring->bufsz - wroff;
			else
				n = len;

			if (ring->granularity > 0)
				n = min(n, ring->granularity);

			ret = kernel_read(filp, ring->bufmem + wroff, n, ppos);
			if (ret <= 0) {
				atomic_sub(count - len, &in->reqsz);
				if (ret)
					in->on_error = ret;
				else
					atomic_set(&in->on_eof, true);
				exception = true;
				goto done;
			}

			if (ppos)
				filp->f_pos = *ppos;

			atomic_add(ret, &ring->fillsz);
			len -= ret;
			wroff = (wroff + n) % ring->bufsz;
		} while (len > 0);
		count = atomic_sub_return(count, &in->reqsz);
	}
done:
	if (ppos)
		mutex_unlock(&filp->f_pos_lock);

	ring->wroff = wroff;

	mutex_unlock(&ring->worker_lock);

	if (atomic_read(&ring->fillsz) > 0 || exception) {
		evl_signal_poll_events(&proxy->poll_head, POLLIN|POLLRDNORM);
		evl_raise_flag(&ring->oob_wait); /* Reschedules. */
		wake_up(&ring->inband_wait);
	}
}

static void relay_input_work(struct evl_work *work)
{
	struct evl_proxy *proxy =
		container_of(work, struct evl_proxy, input.ring.relay_work);

	relay_input(proxy);
}

static ssize_t do_proxy_read(struct file *filp,
			char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_in *in = &proxy->input;
	struct proxy_ring *ring = &in->ring;
	ssize_t len, ret, rbytes, n;
	unsigned int rdoff, avail;
	unsigned long flags;
	char __user *u_ptr;
	int xret;

	if (count == 0)
		return 0;

	if (count > ring->bufsz)
		return -EFBIG;

	if (ring->granularity > 1 && count % ring->granularity > 0)
		return -EINVAL;

	len = count;
retry:
	u_ptr = u_buf;

	for (;;) {
		raw_spin_lock_irqsave(&ring->lock, flags);

		avail = atomic_read(&ring->fillsz) - ring->reserved;

		if (avail < len) {
			raw_spin_unlock_irqrestore(&ring->lock, flags);

			if (in->on_error)
				return in->on_error;

			if (!(filp->f_flags & O_NONBLOCK) &&
				avail > 0 && ring->granularity <= avail) {
				len = ring->granularity > 1 ?
					ring->granularity : avail;
				goto retry;
			}

			return -EAGAIN;
		}

		rdoff = ring->rdoff;
		ring->rdoff = (rdoff + len) % ring->bufsz;
		ring->nesting++;
		ring->reserved += len;
		rbytes = ret = len;

		do {
			if (rdoff + rbytes > ring->bufsz)
				n = ring->bufsz - rdoff;
			else
				n = rbytes;

			raw_spin_unlock_irqrestore(&ring->lock, flags);
			xret = raw_copy_to_user(u_ptr, ring->bufmem + rdoff, n);
			raw_spin_lock_irqsave(&ring->lock, flags);
			if (xret) {
				ret = -EFAULT;
				break;
			}

			u_ptr += n;
			rbytes -= n;
			rdoff = (rdoff + n) % ring->bufsz;
		} while (rbytes > 0);

		if (--ring->nesting == 0) {
			atomic_sub(ring->reserved, &ring->fillsz);
			ring->reserved = 0;
		}
		break;
	}

	raw_spin_unlock_irqrestore(&ring->lock, flags);

	evl_schedule();

	return ret;
}

static ssize_t proxy_oob_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_ring *ring = &proxy->output.ring;
	ssize_t ret;

	if (!proxy_is_writable(proxy))
		return -ENXIO;

	do {
		ret = do_proxy_write(filp, u_buf, count);
		if (ret != -EAGAIN || filp->f_flags & O_NONBLOCK)
			break;
		ret = evl_wait_flag(&ring->oob_wait);
	} while (!ret);

	return ret == -EIDRM ? -EBADF : ret;
}

static ssize_t proxy_oob_read(struct file *filp,
			char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_in *in = &proxy->input;
	struct proxy_ring *ring = &in->ring;
	bool request_done = false;
	ssize_t ret;

	if (!proxy_is_readable(proxy))
		return -ENXIO;

	for (;;) {
		ret = do_proxy_read(filp, u_buf, count);
		if (ret != -EAGAIN || filp->f_flags & O_NONBLOCK)
			break;
		if (!request_done) {
			atomic_add(count, &in->reqsz);
			request_done = true;
		}
		evl_call_inband_from(&ring->relay_work, ring->wq);
		ret = evl_wait_flag(&ring->oob_wait);
		if (ret)
			break;
		if (atomic_cmpxchg(&in->on_eof, true, false) == true) {
			ret = 0;
			break;
		}
	}

	return ret == -EIDRM ? -EBADF : ret;
}

static __poll_t proxy_oob_poll(struct file *filp,
			struct oob_poll_wait *wait)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_ring *oring = &proxy->output.ring;
	struct proxy_ring *iring = &proxy->input.ring;
	__poll_t ret = 0;

	if (!(proxy_is_readable(proxy) || proxy_is_writable(proxy)))
		return POLLERR;

	evl_poll_watch(&proxy->poll_head, wait, NULL);

	if (proxy_is_writable(proxy) &&
		atomic_read(&oring->fillsz) < oring->bufsz)
		ret = POLLOUT|POLLWRNORM;

	if (proxy_is_readable(proxy) && atomic_read(&iring->fillsz) > 0)
		ret |= POLLIN|POLLRDNORM;

	return ret;
}

static ssize_t proxy_write(struct file *filp, const char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_ring *ring = &proxy->output.ring;
	ssize_t ret;

	if (!proxy_is_writable(proxy))
		return -ENXIO;

	do {
		ret = do_proxy_write(filp, u_buf, count);
		if (ret != -EAGAIN || filp->f_flags & O_NONBLOCK)
			break;
		ret = wait_event_interruptible(ring->inband_wait,
					can_write_buffer(ring, count));
	} while (!ret);

	return ret;
}

static ssize_t proxy_read(struct file *filp,
			char __user *u_buf, size_t count, loff_t *ppos)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_in *in = &proxy->input;
	bool request_done = false;
	ssize_t ret;

	if (!proxy_is_readable(proxy))
		return -ENXIO;

	for (;;) {
		ret = do_proxy_read(filp, u_buf, count);
		if (ret != -EAGAIN || filp->f_flags & O_NONBLOCK)
			break;
		if (!request_done) {
			atomic_add(count, &in->reqsz);
			request_done = true;
		}
		relay_input(proxy);
		if (atomic_cmpxchg(&in->on_eof, true, false) == true) {
			ret = 0;
			break;
		}
	}

	return ret;
}

static __poll_t proxy_poll(struct file *filp, poll_table *wait)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_ring *oring = &proxy->output.ring;
	struct proxy_ring *iring = &proxy->input.ring;
	__poll_t ret = 0;

	if (!(proxy_is_readable(proxy) || proxy_is_writable(proxy)))
		return POLLERR;

	if (proxy_is_writable(proxy)) {
		poll_wait(filp, &oring->inband_wait, wait);
		if (atomic_read(&oring->fillsz) < oring->bufsz)
			ret = POLLOUT|POLLWRNORM;
	}

	if (proxy_is_readable(proxy)) {
		poll_wait(filp, &iring->inband_wait, wait);
		if (atomic_read(&iring->fillsz) > 0)
			ret |= POLLIN|POLLRDNORM;
		else if (proxy->filp->f_op->poll) {
			ret = proxy->filp->f_op->poll(proxy->filp, wait);
			ret &= POLLIN|POLLRDNORM;
		}
	}

	return ret;
}

static int proxy_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct file *mapfilp = proxy->filp;
	int ret;

	if (mapfilp->f_op->mmap == NULL)
		return -ENODEV;

	vma->vm_file = get_file(mapfilp);

	/*
	 * Since the mapper element impersonates a different file, we
	 * need to swap references: if the mapping call fails, we have
	 * to drop the reference on the target file we just took on
	 * entry; if it succeeds, then we have to drop the reference
	 * on the mapper file do_mmap_pgoff() acquired before calling
	 * us.
	 */
	ret = call_mmap(mapfilp, vma);
	if (ret)
		fput(mapfilp);
	else
		fput(filp);

	return ret;
}

static int proxy_release(struct inode *inode, struct file *filp)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);

	if (proxy_is_writable(proxy))
		evl_flush_flag(&proxy->output.ring.oob_wait, T_RMID);

	if (proxy_is_readable(proxy))
		evl_flush_flag(&proxy->input.ring.oob_wait, T_RMID);

	return evl_release_element(inode, filp);
}

static const struct file_operations proxy_fops = {
	.open		= evl_open_element,
	.release	= proxy_release,
	.oob_write	= proxy_oob_write,
	.oob_read	= proxy_oob_read,
	.oob_poll	= proxy_oob_poll,
	.write		= proxy_write,
	.read		= proxy_read,
	.poll		= proxy_poll,
	.mmap		= proxy_mmap,
};

static int init_ring(struct proxy_ring *ring,
		struct evl_proxy *proxy,
		size_t bufsz,
		unsigned int granularity,
		bool is_output)
{
	struct workqueue_struct *wq;
	void *bufmem;

	bufmem = kzalloc(bufsz, GFP_KERNEL);
	if (bufmem == NULL)
		return -ENOMEM;

	wq = alloc_ordered_workqueue("%s.%c", 0,
				evl_element_name(&proxy->element),
				is_output ? 'O' : 'I');
	if (wq == NULL) {
		kfree(bufmem);
		return -ENOMEM;
	}

	ring->wq = wq;
	ring->bufmem = bufmem;
	ring->bufsz = bufsz;
	ring->granularity = granularity;
	raw_spin_lock_init(&ring->lock);
	evl_init_work_safe(&ring->relay_work,
			is_output ? relay_output_work : relay_input_work,
			&proxy->element);
	evl_init_flag(&ring->oob_wait);
	init_waitqueue_head(&ring->inband_wait);
	mutex_init(&ring->worker_lock);

	return 0;
}

static void destroy_ring(struct proxy_ring *ring)
{
	evl_destroy_flag(&ring->oob_wait);
	/*
	 * We cannot flush_work() since we may be called from a
	 * kworker, so we bluntly cancel any pending work instead. If
	 * output sync has to be guaranteed on closure, polling for
	 * POLLOUT before closing the target file is your friend.
	 */
	evl_cancel_work(&ring->relay_work);
	destroy_workqueue(ring->wq);
	kfree(ring->bufmem);
}

static struct evl_element *
proxy_factory_build(struct evl_factory *fac, const char __user *u_name,
		void __user *u_attrs, int clone_flags, u32 *state_offp)
{
	struct evl_proxy_attrs attrs;
	struct evl_proxy *proxy;
	struct file *filp;
	size_t bufsz;
	int ret;

	if (clone_flags & ~EVL_PROXY_CLONE_FLAGS)
		return ERR_PTR(-EINVAL);

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	bufsz = attrs.bufsz;
	if (order_base_2(bufsz) > 30) /* LART */
		return ERR_PTR(-EINVAL);

	if (bufsz == 0 && (clone_flags & (EVL_CLONE_INPUT|EVL_CLONE_OUTPUT)))
		return ERR_PTR(-EINVAL);

	/*
	 * If a granularity is set, the buffer size must be a multiple
	 * of the granule size.
	 */
	if (attrs.granularity > 1 && bufsz % attrs.granularity > 0)
		return ERR_PTR(-EINVAL);

	filp = fget(attrs.fd);
	if (filp == NULL)
		return ERR_PTR(-EBADF);

	proxy = kzalloc(sizeof(*proxy), GFP_KERNEL);
	if (proxy == NULL) {
		ret = -ENOMEM;
		goto fail_proxy;
	}

	/*
	 * No direction specified but a valid buffer size implies
	 * EVL_CLONE_OUTPUT.
	 */
	if (bufsz > 0 && !(clone_flags & (EVL_CLONE_INPUT|EVL_CLONE_OUTPUT)))
		clone_flags |= EVL_CLONE_OUTPUT;

	ret = evl_init_user_element(&proxy->element,
				&evl_proxy_factory, u_name, clone_flags);
	if (ret)
		goto fail_element;

	proxy->filp = filp;
	evl_init_poll_head(&proxy->poll_head);

	if (clone_flags & EVL_CLONE_OUTPUT) {
		ret = init_ring(&proxy->output.ring, proxy, bufsz,
				attrs.granularity, true);
		if (ret)
			goto fail_output_init;
	}

	if (clone_flags & EVL_CLONE_INPUT) {
		ret = init_ring(&proxy->input.ring, proxy, bufsz,
				attrs.granularity, false);
		if (ret)
			goto fail_input_init;
	}

	evl_index_factory_element(&proxy->element);

	return &proxy->element;

fail_input_init:
	if (clone_flags & EVL_CLONE_OUTPUT)
		destroy_ring(&proxy->output.ring);
fail_output_init:
	evl_destroy_element(&proxy->element);
fail_element:
	kfree(proxy);
fail_proxy:
	fput(filp);

	return ERR_PTR(ret);
}

static void proxy_factory_dispose(struct evl_element *e)
{
	struct evl_proxy *proxy;

	proxy = container_of(e, struct evl_proxy, element);

	if (proxy_is_writable(proxy))
		destroy_ring(&proxy->output.ring);

	if (proxy_is_readable(proxy))
		destroy_ring(&proxy->input.ring);

	fput(proxy->filp);

	evl_unindex_factory_element(&proxy->element);
	evl_destroy_element(&proxy->element);

	kfree_rcu(proxy, element.rcu);
}

struct evl_factory evl_proxy_factory = {
	.name	=	EVL_PROXY_DEV,
	.fops	=	&proxy_fops,
	.build =	proxy_factory_build,
	.dispose =	proxy_factory_dispose,
	.nrdev	=	CONFIG_EVL_NR_PROXIES,
	.flags	=	EVL_FACTORY_CLONE,
};
