/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/irq_work.h>
#include <linux/uaccess.h>
#include <linux/hashtable.h>
#include <linux/stringhash.h>
#include <evenless/assert.h>
#include <evenless/file.h>
#include <evenless/control.h>
#include <evenless/syscall.h>
#include <evenless/factory.h>
#include <uapi/evenless/factory.h>

static struct class *evl_class;

static struct evl_factory *early_factories[] = {
	&evl_clock_factory,
};

static struct evl_factory *factories[] = {
	&evl_control_factory,
	&evl_thread_factory,
	&evl_monitor_factory,
	&evl_timerfd_factory,
	&evl_poll_factory,
	&evl_xbuf_factory,
	&evl_proxy_factory,
#ifdef CONFIG_FTRACE
	&evl_trace_factory,
#endif
};

#define NR_FACTORIES	\
	(ARRAY_SIZE(early_factories) + ARRAY_SIZE(factories))

static dev_t factory_rdev;

int evl_init_element(struct evl_element *e, struct evl_factory *fac)
{
	int minor;

	do {
		minor = find_first_zero_bit(fac->minor_map, fac->nrdev);
		if (minor >= fac->nrdev) {
			printk_ratelimited(EVL_WARNING "out of %ss",
					fac->name);
			return -EAGAIN;
		}
	} while (test_and_set_bit(minor, fac->minor_map));

	e->factory = fac;
	e->minor = minor;
	e->refs = 1;
	e->zombie = false;
	e->fundle = EVL_NO_HANDLE;
	e->devname = NULL;
	raw_spin_lock_init(&e->ref_lock);

	return 0;
}

void evl_destroy_element(struct evl_element *e)
{
	clear_bit(e->minor, e->factory->minor_map);
	if (e->devname)
		putname(e->devname);
}

void evl_get_element(struct evl_element *e)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&e->ref_lock, flags);
	e->refs++;
	raw_spin_unlock_irqrestore(&e->ref_lock, flags);
}

int evl_open_element(struct inode *inode, struct file *filp)
{
	struct evl_file_binding *fbind;
	struct evl_element *e;
	unsigned long flags;
	int ret = 0;

	e = container_of(inode->i_cdev, struct evl_element, cdev);

	rcu_read_lock();

	raw_spin_lock_irqsave(&e->ref_lock, flags);

	if (e->zombie)
		ret = -ESTALE;
	else
		e->refs++;

	raw_spin_unlock_irqrestore(&e->ref_lock, flags);

	rcu_read_unlock();

	if (ret)
		return ret;

	fbind = kmalloc(sizeof(*fbind), GFP_KERNEL);
	if (fbind == NULL)
		return -ENOMEM;

	ret = evl_open_file(&fbind->efile, filp);
	if (ret) {
		kfree(fbind);
		return ret;
	}

	fbind->element = e;
	filp->private_data = fbind;

	return 0;
}

static void __put_element(struct evl_element *e)
{
	struct evl_factory *fac = e->factory;

	/*
	 * e->minor won't be free for use until evl_destroy_element()
	 * is called from the disposal handler, so there is no risk of
	 * reusing it too early.
	 */
	evl_remove_element_device(e);
	/*
	 * Serialize with evl_open_element().
	 */
	synchronize_rcu();
	/*
	 * CAUTION: the disposal handler should delay the release of
	 * e's container at the next rcu idle period via kfree_rcu(),
	 * because the embedded e->cdev is still needed ahead for
	 * completing the file release process (see __fput()).
	 */
	fac->dispose(e);
}

static void put_element_work(struct work_struct *work)
{
	struct evl_element *e;

	e = container_of(work, struct evl_element, work);
	__put_element(e);
}

static void put_element_irq(struct irq_work *work)
{
	struct evl_element *e;

	e = container_of(work, struct evl_element, irq_work);
	INIT_WORK(&e->work, put_element_work);
	schedule_work(&e->work);
}

static void put_element(struct evl_element *e)
{
	/*
	 * These trampolines may look like a bit cheesy but we have no
	 * choice but offloading the disposal to an in-band task
	 * context. In (the rare) case the last ref. to an element was
	 * dropped from OOB context, we need to go via an
	 * irq_work->workqueue chain in order to run __put_element()
	 * eventually.
	 */
	if (unlikely(running_oob())) {
		init_irq_work(&e->irq_work, put_element_irq);
		irq_work_queue(&e->irq_work);
	} else
		__put_element(e);
}

void evl_put_element(struct evl_element *e) /* in-band or OOB */
{
	unsigned long flags;

	/*
	 * Multiple files may reference a single element on
	 * open(). The element release logic competes with
	 * evl_open_element() as follows:
	 *
	 * a) evl_put_element() grabs the ->ref_lock first and raises
	 * the zombie flag iff the refcount drops to zero, or
	 * evl_open_element() gets it first.

	 * b) evl_open_element() races with evl_put_element() and
	 * detects an ongoing deletion of @ent, returning -ESTALE.

	 * c) evl_open_element() is first and increments the refcount
	 * which should lead us to skip the whole release process
	 * in evl_put_element() when it runs next.
	 *
	 * In any case, evl_open_element() is protected against
	 * referencing stale @ent memory by a read-side RCU
	 * section. Meanwhile we wait for all read-sides to complete
	 * after calling cdev_del().  Once cdev_del() returns, the
	 * device cannot be opened anymore, without affecting the
	 * files that might still be opened on this device though.
	 *
	 * In the c) case, the last file release will dispose of the
	 * element eventually.
	 */
	raw_spin_lock_irqsave(&e->ref_lock, flags);

	if (--e->refs == 0) {
		e->zombie = true;
		raw_spin_unlock_irqrestore(&e->ref_lock, flags);
		put_element(e);
	} else
		raw_spin_unlock_irqrestore(&e->ref_lock, flags);
}

int evl_release_element(struct inode *inode, struct file *filp)
{
	struct evl_file_binding *fbind = filp->private_data;
	struct evl_element *e = fbind->element;

	evl_release_file(&fbind->efile);
	kfree(fbind);
	evl_put_element(e);

	return 0;
}

static int create_named_element_device(struct evl_element *e,
				struct evl_factory *fac)
{
	struct evl_element *n;
	struct device *dev;
	dev_t rdev;
	u64 hlen;
	int ret;

	/*
	 * Do a quick hash check on the new device name, to make sure
	 * device_create() won't trigger a kernel log splash because
	 * of a naming conflict.
	 */
	hlen = hashlen_string("EVL", e->devname->name);
	mutex_lock(&fac->hash_lock);

	hash_for_each_possible(fac->name_hash, n, hash, hlen)
		if (!strcmp(n->devname->name, e->devname->name)) {
			mutex_unlock(&fac->hash_lock);
			return -EEXIST;
		}

	hash_add(fac->name_hash, &e->hash, hlen);

	mutex_unlock(&fac->hash_lock);

	rdev = MKDEV(MAJOR(fac->sub_rdev), e->minor);
	cdev_init(&e->cdev, fac->fops);
	ret = cdev_add(&e->cdev, rdev, 1);
	if (ret)
		goto fail_add;

	dev = device_create(fac->class, NULL, rdev, e,
			"%s", evl_element_name(e));
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto fail_dev;
	}

	if (fac->attrs) {
		ret = device_add_groups(dev, fac->attrs);
		if (ret)
			goto fail_groups;
	}

	return 0;

fail_groups:
	device_destroy(fac->class, rdev);
fail_dev:
	cdev_del(&e->cdev);
fail_add:
	mutex_lock(&fac->hash_lock);
	hash_del(&e->hash);
	mutex_unlock(&fac->hash_lock);

	return ret;
}

int evl_create_element_device(struct evl_element *e,
			struct evl_factory *fac,
			const char *name)
{
	struct filename *devname;

	devname = getname_kernel(name);
	if (devname == NULL)
		return PTR_ERR(devname);

	e->devname = devname;

	return create_named_element_device(e, fac);
}

void evl_remove_element_device(struct evl_element *e)
{
	struct evl_factory *fac = e->factory;
	dev_t rdev;

	rdev = MKDEV(MAJOR(fac->sub_rdev), e->minor);
	device_destroy(fac->class, rdev);
	cdev_del(&e->cdev);
	mutex_lock(&fac->hash_lock);
	hash_del(&e->hash);
	mutex_unlock(&fac->hash_lock);
}

static long ioctl_clone_device(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_element *e = filp->private_data;
	struct evl_clone_req req, __user *u_req;
	struct filename *devname = NULL;
	__u32 val, state_offset = -1U;
	struct evl_factory *fac;
	char tmpbuf[16];
	int ret;

	if (cmd != EVL_IOC_CLONE)
		return -ENOTTY;

	if (!evl_is_running())
		return -ENXIO;

	if (e)
		return -EBUSY;

	u_req = (typeof(u_req))arg;
	ret = copy_from_user(&req, u_req, sizeof(req));
	if (ret)
		return -EFAULT;

	if (req.name) {
		devname = getname(req.name);
		if (IS_ERR(devname))
			return PTR_ERR(devname);
	}

	fac = container_of(filp->f_inode->i_cdev, struct evl_factory, cdev);
	e = fac->build(fac, devname ? devname->name : NULL,
		req.attrs, &state_offset);
	if (IS_ERR(e)) {
		if (devname)
			putname(devname);
		return PTR_ERR(e);
	}

	if (!devname) {
		/* If no name specified, default to device minor. */
		snprintf(tmpbuf, sizeof(tmpbuf), "%d", e->minor);
		devname = getname_kernel(tmpbuf);
		if (IS_ERR(devname)) {
			fac->dispose(e);
			return PTR_ERR(devname);
		}
	}

	/* The device name is valid throughout the element's lifetime. */
	e->devname = devname;

	/* This must be set before the device appears. */
	filp->private_data = e;
	barrier();

	ret = create_named_element_device(e, fac);
	if (ret) {
		/* release_clone_device() must skip cleanup. */
		filp->private_data = NULL;
		fac->dispose(e);
		return ret;
	}

	val = e->minor;
	ret |= put_user(val, &u_req->eids.minor) ? -EFAULT : 0;
	val = e->fundle;
	ret |= put_user(val, &u_req->eids.fundle) ? -EFAULT : 0;
	ret |= put_user(state_offset, &u_req->eids.state_offset) ? -EFAULT : 0;

	return ret ? -EFAULT : 0;
}

static int release_clone_device(struct inode *inode, struct file *filp)
{
	struct evl_element *e = filp->private_data;

	if (e)
		evl_put_element(e);

	return 0;
}

static const struct file_operations clone_fops = {
	.unlocked_ioctl	= ioctl_clone_device,
	.release	= release_clone_device,
};

static int index_element_at(struct evl_element *e, fundle_t fundle)
{
	struct evl_index *map = &e->factory->index;
	struct rb_node **rbp, *parent;
	struct evl_element *tmp;

	parent = NULL;
	rbp = &map->root.rb_node;
	while (*rbp) {
		tmp = rb_entry(*rbp, struct evl_element, index_node);
		parent = *rbp;
		if (fundle < tmp->fundle)
			rbp = &(*rbp)->rb_left;
		else if (fundle > tmp->fundle)
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST;
	}

	e->fundle = fundle;
	rb_link_node(&e->index_node, parent, rbp);
	rb_insert_color(&e->index_node, &map->root);

	return 0;
}

int evl_index_element_at(struct evl_element *e, fundle_t fundle)
{
	struct evl_index *map = &e->factory->index;
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&map->lock, flags);
	ret = index_element_at(e, fundle);
	raw_spin_unlock_irqrestore(&map->lock, flags);

	return ret;
}

void evl_index_element(struct evl_element *e)
{
	struct evl_index *map = &e->factory->index;
	fundle_t fundle, guard = 0;
	unsigned long flags;
	int ret;

	do {
		if (evl_get_index(++guard) == 0) { /* Paranoid. */
			e->fundle = EVL_NO_HANDLE;
			WARN_ON_ONCE("out of fundle index space");
			return;
		}

		raw_spin_lock_irqsave(&map->lock, flags);

		fundle = evl_get_index(++map->generator);
		if (!fundle)		/* Exclude EVL_NO_HANDLE */
			fundle = map->generator = 1;

		ret = index_element_at(e, fundle);

		raw_spin_unlock_irqrestore(&map->lock, flags);
	} while (ret);
}

void evl_unindex_element(struct evl_element *e)
{
	struct evl_index *map = &e->factory->index;
	unsigned long flags;

	raw_spin_lock_irqsave(&map->lock, flags);
	rb_erase(&e->index_node, &map->root);
	raw_spin_unlock_irqrestore(&map->lock, flags);
}

struct evl_element *
__evl_get_element_by_fundle(struct evl_factory *fac, fundle_t fundle)
{
	struct evl_index *map = &fac->index;
	struct evl_element *e;
	unsigned long flags;
	struct rb_node *rb;

	raw_spin_lock_irqsave(&map->lock, flags);

	rb = map->root.rb_node;
	while (rb) {
		e = rb_entry(rb, struct evl_element, index_node);
		if (fundle < e->fundle)
			rb = rb->rb_left;
		else if (fundle > e->fundle)
			rb = rb->rb_right;
		else {
			raw_spin_lock(&e->ref_lock);
			if (unlikely(e->zombie))
				e = NULL;
			else
				e->refs++;
			raw_spin_unlock(&e->ref_lock);
			raw_spin_unlock_irqrestore(&map->lock, flags);
			return e;
		}
	}

	raw_spin_unlock_irqrestore(&map->lock, flags);

	return NULL;
}

static char *factory_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "evenless/%s/%s",
			dev->class->name, dev_name(dev));
}

static int create_element_class(struct evl_factory *fac)
{
	struct class *class;
	int ret = -ENOMEM;

	fac->minor_map = bitmap_zalloc(fac->nrdev, GFP_KERNEL);
	if (fac->minor_map == NULL)
		return ret;

	class = class_create(THIS_MODULE, fac->name);
	if (IS_ERR(class)) {
		ret = PTR_ERR(class);
		goto cleanup_minor;
	}

	class->devnode = factory_devnode;

	ret = alloc_chrdev_region(&fac->sub_rdev, 0, fac->nrdev, fac->name);
	if (ret)
		goto cleanup_class;

	fac->class = class;

	return 0;

cleanup_class:
	class_destroy(class);
cleanup_minor:
	bitmap_free(fac->minor_map);

	return ret;
}

static void delete_element_class(struct evl_factory *fac)
{
	unregister_chrdev_region(fac->sub_rdev, fac->nrdev);
	class_destroy(fac->class);
	bitmap_free(fac->minor_map);
}

static int create_factory(struct evl_factory *fac, dev_t rdev)
{
	const char *idevname = "clone"; /* Initial device in factory. */
	struct device *dev;
	int ret;

	if (fac->flags & EVL_FACTORY_SINGLE) {
		idevname = fac->name;
		fac->class = evl_class;
		fac->minor_map = NULL;
		fac->sub_rdev = MKDEV(0, 0);
		cdev_init(&fac->cdev, fac->fops);
	} else {
		ret = create_element_class(fac);
		if (ret)
			return ret;
		if (fac->flags & EVL_FACTORY_CLONE)
			cdev_init(&fac->cdev, &clone_fops);
	}

	if (fac->flags & (EVL_FACTORY_CLONE|EVL_FACTORY_SINGLE)) {
		ret = cdev_add(&fac->cdev, rdev, 1);
		if (ret)
			goto fail_cdev;

		dev = device_create(fac->class, NULL, rdev, fac, idevname);
		if (IS_ERR(dev))
			goto fail_dev;

		if ((fac->flags & EVL_FACTORY_SINGLE) && fac->attrs) {
			ret = device_add_groups(dev, fac->attrs);
			if (ret)
				goto fail_groups;
		}
	}

	fac->rdev = rdev;
	raw_spin_lock_init(&fac->index.lock);
	fac->index.root = RB_ROOT;
	fac->index.generator = EVL_NO_HANDLE;
	hash_init(fac->name_hash);
	mutex_init(&fac->hash_lock);

	return 0;

fail_groups:
	device_destroy(fac->class, rdev);
fail_dev:
	cdev_del(&fac->cdev);
fail_cdev:
	if (!(fac->flags & EVL_FACTORY_SINGLE))
		delete_element_class(fac);

	return ret;
}

static void delete_factory(struct evl_factory *fac)
{
	device_destroy(fac->class, fac->rdev);

	if (fac->flags & (EVL_FACTORY_CLONE|EVL_FACTORY_SINGLE))
		cdev_del(&fac->cdev);

	if (!(fac->flags & EVL_FACTORY_SINGLE))
		delete_element_class(fac);
}

static char *evl_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "evenless/%s", dev_name(dev));
}

static int __init
create_factories(struct evl_factory **factories, int nr)
{
	int ret, n;

	for (n = 0; n < nr; n++) {
		ret = create_factory(factories[n],
				MKDEV(MAJOR(factory_rdev), n));
		if (ret)
			goto fail;
	}

	return 0;
fail:
	while (n-- > 0)
		delete_factory(factories[n]);

	return ret;
}

static void __init
remove_factories(struct evl_factory **factories, int nr)
{
	int n;

	for (n = 0; n < nr; n++)
		delete_factory(factories[n]);
}

int __init evl_early_init_factories(void)
{
	int ret;

	evl_class = class_create(THIS_MODULE, "evenless");
	if (IS_ERR(evl_class))
		return PTR_ERR(evl_class);

	evl_class->devnode = evl_devnode;

	ret = alloc_chrdev_region(&factory_rdev, 0, NR_FACTORIES,
				"evenless_factory");
	if (ret) {
		class_destroy(evl_class);
		return ret;
	}

	ret = create_factories(early_factories,
			ARRAY_SIZE(early_factories));
	if (ret) {
		unregister_chrdev_region(factory_rdev, NR_FACTORIES);
		class_destroy(evl_class);
	}

	return ret;
}

void __init evl_early_cleanup_factories(void)
{
	remove_factories(early_factories, ARRAY_SIZE(early_factories));
	unregister_chrdev_region(factory_rdev, NR_FACTORIES);
	class_destroy(evl_class);
}

int __init evl_late_init_factories(void)
{
	return create_factories(factories, ARRAY_SIZE(factories));
}

void __init evl_cleanup_factories(void)
{
	remove_factories(factories, ARRAY_SIZE(factories));
	evl_early_cleanup_factories();
}
