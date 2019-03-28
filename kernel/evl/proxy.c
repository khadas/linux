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
#include <linux/fs.h>
#include <linux/log2.h>
#include <linux/irq_work.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/atomic.h>
#include <evl/factory.h>
#include <uapi/evl/proxy.h>

struct evl_proxy {
	struct file *filp;
	struct circ_buf circ_buf;
	atomic_t write_sem;
	size_t bufsz;
	struct irq_work irq_work;
	struct work_struct work;
	hard_spinlock_t lock;
	struct evl_element element;
};

static void relay_output(struct work_struct *work)
{
	int head, tail, rem, len;
	struct evl_proxy *proxy;
	struct circ_buf *circ;
	struct file *outfilp;
	ssize_t ret;
	loff_t pos;

	proxy = container_of(work, struct evl_proxy, work);
	outfilp = proxy->filp;
	circ = &proxy->circ_buf;

	mutex_lock(&outfilp->f_pos_lock);

	for (;;) {
		head = smp_load_acquire(&circ->head);
		tail = circ->tail;
		rem = CIRC_CNT(head, tail, proxy->bufsz);
		if (rem <= 0)
			break;

		len = min(CIRC_CNT_TO_END(head, tail, proxy->bufsz), rem);

		pos = outfilp->f_pos;
		ret = vfs_write(outfilp, circ->buf + tail, len, &pos);
		if (ret >= 0)
			outfilp->f_pos = pos;

		smp_store_release(&circ->tail,
				(tail + len) & (proxy->bufsz - 1));
	}

	mutex_unlock(&outfilp->f_pos_lock);
}

static void relay_output_irq(struct irq_work *work)
{
	struct evl_proxy *proxy;

	proxy = container_of(work, struct evl_proxy, irq_work);
	schedule_work(&proxy->work);
}

static ssize_t proxy_oob_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_proxy *proxy = element_of(filp, struct evl_proxy);
	struct circ_buf *circ = &proxy->circ_buf;
	ssize_t rem, avail, written = 0;
	const char __user *u_ptr;
	int head, tail, len, ret;
	unsigned long flags;
	bool kick;

	if (count == 0)
		return 0;

	if (count >= proxy->bufsz) /* Avail space is bufsz - 1. */
		return -EFBIG;
retry:
	u_ptr = u_buf;
	rem = count;

	raw_spin_lock_irqsave(&proxy->lock, flags);

	for (;;) {
		head = circ->head;
		tail = READ_ONCE(circ->tail);
		if (rem == 0 || CIRC_SPACE(head, tail, proxy->bufsz) <= 0)
			break;

		avail = CIRC_SPACE_TO_END(head, tail, proxy->bufsz);
		len = min(avail, rem);

		raw_spin_unlock_irqrestore(&proxy->lock, flags);

		if (atomic_dec_return(&proxy->write_sem) < 0) {
			atomic_inc(&proxy->write_sem);
			goto retry;
		}

		ret = raw_copy_from_user(circ->buf + head, u_ptr, len);
		raw_spin_lock_irqsave(&proxy->lock, flags);
		atomic_inc(&proxy->write_sem);

		if (ret) {
			written = -EFAULT;
			break;
		}

		smp_store_release(&circ->head,
				(head + len) & (proxy->bufsz - 1));
		u_ptr += len;
		rem -= len;
		written += len;
	}

	kick = CIRC_CNT(head, tail, proxy->bufsz) > 0;

	raw_spin_unlock_irqrestore(&proxy->lock, flags);

	if (kick)
		irq_work_queue(&proxy->irq_work);

	return written;
}

static ssize_t proxy_write(struct file *filp, const char __user *u_buf,
			size_t count, loff_t *ppos)
{
	return proxy_oob_write(filp, u_buf, count);
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

static const struct file_operations proxy_fops = {
	.open		= evl_open_element,
	.release	= evl_release_element,
	.oob_write	= proxy_oob_write,
	.write		= proxy_write,
	.mmap		= proxy_mmap,
};

static struct evl_element *
proxy_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct evl_proxy_attrs attrs;
	struct evl_proxy *proxy;
	void *bufmem = NULL;
	struct file *filp;
	size_t bufsz;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	bufsz = roundup_pow_of_two(attrs.bufsz);
	if (order_base_2(bufsz) > 30) /* LART */
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
	proxy->circ_buf.buf = bufmem;
	proxy->bufsz = bufsz;
	atomic_set(&proxy->write_sem, 1);
	INIT_WORK(&proxy->work, relay_output);
	init_irq_work(&proxy->irq_work, relay_output_irq);
	raw_spin_lock_init(&proxy->lock);
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

	proxy = container_of(e, struct evl_proxy, element);
	evl_unindex_element(&proxy->element);
	fput(proxy->filp);
	evl_destroy_element(&proxy->element);

	if (proxy->circ_buf.buf)
		kfree(proxy->circ_buf.buf);

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
