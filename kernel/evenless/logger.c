/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/log2.h>
#include <linux/mutex.h>
#include <linux/irq_work.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/atomic.h>
#include <evenless/factory.h>
#include <uapi/evenless/logger.h>

struct evl_logger {
	struct file *outfilp;
	struct circ_buf circ_buf;
	atomic_t write_sem;
	size_t logsz;
	struct evl_element element;
	struct irq_work irq_work;
	struct work_struct work;
	hard_spinlock_t lock;
};

static void relay_output(struct work_struct *work)
{
	struct evl_logger *logger;
	int head, tail, rem, len;
	struct circ_buf *circ;
	struct file *filp;
	ssize_t ret;
	loff_t pos;

	logger = container_of(work, struct evl_logger, work);
	filp = logger->outfilp;
	circ = &logger->circ_buf;

	mutex_lock(&filp->f_pos_lock);

	for (;;) {
		head = smp_load_acquire(&circ->head);
		tail = circ->tail;
		rem = CIRC_CNT(head, tail, logger->logsz);
		if (rem <= 0)
			break;

		len = min(CIRC_CNT_TO_END(head, tail, logger->logsz), rem);

		pos = filp->f_pos;
		ret = vfs_write(filp, circ->buf + tail, len, &pos);
		if (ret >= 0)
			filp->f_pos = pos;

		smp_store_release(&circ->tail,
				(tail + len) & (logger->logsz - 1));
	}

	mutex_unlock(&filp->f_pos_lock);
}

static void relay_output_irq(struct irq_work *work)
{
	struct evl_logger *logger;

	logger = container_of(work, struct evl_logger, irq_work);
	schedule_work(&logger->work);
}

static ssize_t logger_oob_write(struct file *filp,
				const char __user *u_buf, size_t count)
{
	struct evl_logger *logger = element_of(filp, struct evl_logger);
	struct circ_buf *circ = &logger->circ_buf;
	ssize_t rem, avail, written = 0;
	const char __user *u_ptr;
	int head, tail, len, ret;
	unsigned long flags;
	bool kick;

	if (count >= logger->logsz) /* Avail space is logsz - 1. */
		return -EFBIG;
retry:
	u_ptr = u_buf;
	rem = count;

	raw_spin_lock_irqsave(&logger->lock, flags);

	for (;;) {
		head = circ->head;
		tail = READ_ONCE(circ->tail);
		if (rem == 0 || CIRC_SPACE(head, tail, logger->logsz) <= 0)
			break;

		avail = CIRC_SPACE_TO_END(head, tail, logger->logsz);
		len = min(avail, rem);

		raw_spin_unlock_irqrestore(&logger->lock, flags);

		if (atomic_dec_return(&logger->write_sem) < 0) {
			atomic_inc(&logger->write_sem);
			goto retry;
		}

		ret = raw_copy_from_user(circ->buf + head, u_ptr, len);
		raw_spin_lock_irqsave(&logger->lock, flags);
		atomic_inc(&logger->write_sem);

		if (ret) {
			written = -EFAULT;
			break;
		}

		smp_store_release(&circ->head,
				(head + len) & (logger->logsz - 1));
		u_ptr += len;
		rem -= len;
		written += len;
	}

	kick = CIRC_CNT(head, tail, logger->logsz) > 0;

	raw_spin_unlock_irqrestore(&logger->lock, flags);

	if (kick)
		irq_work_queue(&logger->irq_work);

	return written;
}

static ssize_t logger_write(struct file *filp, const char __user *u_buf,
			size_t count, loff_t *ppos)
{
	return logger_oob_write(filp, u_buf, count);
}

static const struct file_operations logger_fops = {
	.open		= evl_open_element,
	.release	= evl_release_element,
	.oob_write	= logger_oob_write,
	.write		= logger_write,
};

static struct evl_element *
logger_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct evl_logger_attrs attrs;
	struct evl_logger *logger;
	struct file *outfilp;
	void *bufmem;
	size_t logsz;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	logsz = roundup_pow_of_two(attrs.logsz);
	if (order_base_2(logsz) > 30) /* LART */
		return ERR_PTR(-EINVAL);

	outfilp = fget(attrs.fd);
	if (outfilp == NULL)
		return ERR_PTR(-EINVAL);

	logger = kzalloc(sizeof(*logger), GFP_KERNEL);
	if (logger == NULL) {
		ret = -ENOMEM;
		goto fail_logger;
	}

	bufmem = kzalloc(attrs.logsz, GFP_KERNEL);
	if (bufmem == NULL) {
		ret = -ENOMEM;
		goto fail_bufmem;
	}

	ret = evl_init_element(&logger->element, &evl_logger_factory);
	if (ret)
		goto fail_element;

	logger->outfilp = outfilp;
	logger->circ_buf.buf = bufmem;
	logger->logsz = logsz;
	atomic_set(&logger->write_sem, 1);
	INIT_WORK(&logger->work, relay_output);
	init_irq_work(&logger->irq_work, relay_output_irq);
	raw_spin_lock_init(&logger->lock);

	return &logger->element;

fail_element:
	kfree(bufmem);
fail_bufmem:
	kfree(logger);
fail_logger:
	fput(outfilp);

	return ERR_PTR(ret);
}

static void logger_factory_dispose(struct evl_element *e)
{
	struct evl_logger *logger;

	logger = container_of(e, struct evl_logger, element);

	fput(logger->outfilp);
	kfree(logger->circ_buf.buf);
	evl_destroy_element(&logger->element);

	kfree_rcu(logger, element.rcu);
}

struct evl_factory evl_logger_factory = {
	.name	=	"logger",
	.fops	=	&logger_fops,
	.build =	logger_factory_build,
	.dispose =	logger_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_LOGGERS,
	.flags	=	EVL_FACTORY_CLONE,
};
