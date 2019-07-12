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
#include <evl/flag.h>
#include <evl/poll.h>
#include <uapi/evl/proxy.h>

struct proxy_ring {
	void *bufmem;
	atomic_t fillsz;
	int wrpending;
	unsigned int bufsz;
	unsigned int rdoff;
	unsigned int wroff;
	unsigned int fillrsvd;
	unsigned int granularity;
};

struct proxy_out {		/* oob_write->write */
	struct evl_flag oob_drained;
	wait_queue_head_t inband_drained;
	bool release_pending;
	struct irq_work irq_work;
	struct work_struct work;
	hard_spinlock_t lock;
	struct evl_poll_head poll_head;
	struct proxy_ring ring;
};

struct evl_proxy {
	struct file *filp;
	struct proxy_out output;
	struct evl_element element;
};

static void relay_output(struct work_struct *work)
{
	struct evl_proxy *proxy = container_of(work, struct evl_proxy, output.work);
	struct proxy_out *out = &proxy->output;
	struct proxy_ring *ring = &out->ring;
	unsigned int rdoff, count, len, n;
	struct file *filp = proxy->filp;
	ssize_t ret = 0;
	loff_t pos = 0;

	count = atomic_read(&ring->fillsz);
	rdoff = ring->rdoff;

	 while (count > 0 && ret >= 0) {
		len = count;
		do {
			if (rdoff + len > ring->bufsz)
				n = ring->bufsz - rdoff;
			else
				n = len;

			if (ring->granularity > 0)
				n = min(n, ring->granularity);

			ret = kernel_write(filp, ring->bufmem + rdoff, n, &pos);
			len -= n;
			rdoff = (rdoff + n) % ring->bufsz;
		} while (len > 0 && ret > 0);
		count = atomic_sub_return(count, &ring->fillsz);
	}

	ring->rdoff = rdoff;

	if (count == 0)
		evl_signal_poll_events(&out->poll_head, POLLOUT|POLLWRNORM);

	/* Give precedence to oob waiters for wakeups. */
	if (count < ring->bufsz) {
		evl_raise_flag(&out->oob_drained);
		wake_up(&out->inband_drained);
	}
}

static void relay_output_irq(struct irq_work *work)
{
	struct evl_proxy *proxy;

	proxy = container_of(work, struct evl_proxy, output.irq_work);
	schedule_work(&proxy->output.work);
}

static bool can_write_buffer(struct proxy_out *out, size_t size)
{
	struct proxy_ring *ring = &out->ring;

	return atomic_read(&ring->fillsz) +
		ring->fillrsvd + size <= ring->bufsz;
}

static ssize_t do_proxy_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_out *out = &proxy->output;
	struct proxy_ring *ring = &out->ring;
	unsigned int wroff, wbytes, n;
	unsigned long flags;
	ssize_t ret;
	int xret;

	if (count == 0)
		return 0;

	if (count > ring->bufsz)
		return -EFBIG;

	if (ring->granularity > 1 && count % ring->granularity > 0)
		return -EINVAL;

	raw_spin_lock_irqsave(&out->lock, flags);

	/* No short or scattered writes. */
	if (!can_write_buffer(out, count)) {
		ret = -EAGAIN;
		goto out;
	}

	/* Reserve a write slot into the circular buffer. */
	wroff = ring->wroff;
	ring->wroff = (wroff + count) % ring->bufsz;
	ring->wrpending++;
	ring->fillrsvd += count;
	wbytes = ret = count;

	do {
		if (wroff + wbytes > ring->bufsz)
			n = ring->bufsz - wroff;
		else
			n = wbytes;

		raw_spin_unlock_irqrestore(&out->lock, flags);
		xret = raw_copy_from_user(ring->bufmem + wroff, u_buf, n);
		raw_spin_lock_irqsave(&out->lock, flags);
		if (xret) {
			memset(ring->bufmem + wroff + n - xret, 0, xret);
			ret = -EFAULT;
			break;
		}

		u_buf += n;
		wbytes -= n;
		wroff = (wroff + n) % ring->bufsz;
	} while (wbytes > 0);

	if (--ring->wrpending == 0) {
		n = atomic_add_return(ring->fillrsvd, &ring->fillsz);
		ring->fillrsvd = 0;
		if (n == count) /* empty -> non-empty transition */
			irq_work_queue(&out->irq_work);
	}
out:
	raw_spin_unlock_irqrestore(&out->lock, flags);

	return ret;
}

static ssize_t proxy_oob_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_out *out = &proxy->output;
	ssize_t ret;

	do {
		ret = do_proxy_write(filp, u_buf, count);
		if (ret != -EAGAIN || filp->f_flags & O_NONBLOCK)
			break;
		ret = evl_wait_flag(&out->oob_drained);
	} while (!ret);

	return ret == -EIDRM ? -EBADF : ret;
}

static ssize_t proxy_write(struct file *filp, const char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_out *out = &proxy->output;
	ssize_t ret;

	do {
		ret = do_proxy_write(filp, u_buf, count);
		if (ret != -EAGAIN || filp->f_flags & O_NONBLOCK)
			break;
		ret = wait_event_interruptible(out->inband_drained,
					can_write_buffer(out, count) ||
					out->release_pending);
		if (!ret && out->release_pending)
			ret = -EBADF;
	} while (ret != -ERESTARTSYS);

	return ret;
}

static __poll_t proxy_oob_poll(struct file *filp,
			struct oob_poll_wait *wait)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct proxy_out *out = &proxy->output;
	struct proxy_ring *ring = &out->ring;

	evl_poll_watch(&out->poll_head, wait, NULL);

	return atomic_read(&ring->fillsz) < ring->bufsz ?
		POLLOUT|POLLWRNORM : 0;
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
	struct proxy_out *out = &proxy->output;

	out->release_pending = true;
	evl_flush_flag(&out->oob_drained, T_RMID);
	wake_up(&out->inband_drained);

	return evl_release_element(inode, filp);
}

static const struct file_operations proxy_fops = {
	.open		= evl_open_element,
	.release	= proxy_release,
	.oob_write	= proxy_oob_write,
	.oob_poll	= proxy_oob_poll,
	.write		= proxy_write,
	.mmap		= proxy_mmap,
};

static struct evl_element *
proxy_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct evl_proxy_attrs attrs;
	struct evl_proxy *proxy;
	struct proxy_out *out;
	void *bufmem = NULL;
	struct file *filp;
	size_t bufsz;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	bufsz = attrs.bufsz;
	if (order_base_2(bufsz) > 30) /* LART */
		return ERR_PTR(-EINVAL);

	/*
	 * If a granularity is set, the buffer size must be a multiple
	 * of the granule size.
	 */
	if (attrs.granularity > 1 && bufsz % attrs.granularity > 0)
		return ERR_PTR(-EINVAL);

	filp = fget(attrs.fd);
	if (filp == NULL)
		return ERR_PTR(-EINVAL);

	proxy = kzalloc(sizeof(*proxy), GFP_KERNEL);
	if (proxy == NULL) {
		ret = -ENOMEM;
		goto fail_proxy;
	}

	/*
	 * Buffer size is optional as we may need the mapping facility
	 * only, without any provision for writing to the proxied
	 * file.
	 */
	if (bufsz > 0) {
		bufmem = kzalloc(bufsz, GFP_KERNEL);
		if (bufmem == NULL) {
			ret = -ENOMEM;
			goto fail_bufmem;
		}
	}

	ret = evl_init_element(&proxy->element, &evl_proxy_factory);
	if (ret)
		goto fail_element;

	proxy->filp = filp;
	out = &proxy->output;
	out->ring.bufmem = bufmem;
	out->ring.bufsz = bufsz;
	out->ring.granularity = attrs.granularity;
	raw_spin_lock_init(&out->lock);
	init_irq_work(&out->irq_work, relay_output_irq);
	INIT_WORK(&out->work, relay_output);
	evl_init_poll_head(&out->poll_head);
	evl_init_flag(&out->oob_drained);
	init_waitqueue_head(&out->inband_drained);
	evl_index_element(&proxy->element);

	return &proxy->element;

fail_element:
	kfree(bufmem);
fail_bufmem:
	kfree(proxy);
fail_proxy:
	fput(filp);

	return ERR_PTR(ret);
}

static void proxy_factory_dispose(struct evl_element *e)
{
	struct evl_proxy *proxy;
	struct proxy_out *out;

	proxy = container_of(e, struct evl_proxy, element);
	out = &proxy->output;
	irq_work_sync(&out->irq_work);
	cancel_work_sync(&out->work);
	fput(proxy->filp);
	evl_destroy_flag(&out->oob_drained);
	evl_unindex_element(&proxy->element);
	evl_destroy_element(&proxy->element);

	if (out->ring.bufmem)
		kfree(out->ring.bufmem);

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
