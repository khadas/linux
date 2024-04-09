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
#include <linux/uidgid.h>
#include <linux/irq_work.h>
#include <linux/uaccess.h>
#include <linux/hashtable.h>
#include <linux/stringhash.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/dovetail.h>
#include <evl/assert.h>
#include <evl/file.h>
#include <evl/control.h>
#include <evl/factory.h>
#include <evl/uaccess.h>
#include <uapi/evl/syscall-abi.h>

static struct class *evl_class;

static struct evl_factory *early_factories[] = {
	&evl_clock_factory,
};

static struct evl_factory *factories[] = {
	&evl_control_factory,
	&evl_thread_factory,
	&evl_monitor_factory,
	&evl_poll_factory,
	&evl_xbuf_factory,
	&evl_proxy_factory,
	&evl_observable_factory,
#ifdef CONFIG_FTRACE
	&evl_trace_factory,
#endif
};

#define NR_FACTORIES	\
	(ARRAY_SIZE(early_factories) + ARRAY_SIZE(factories))

static dev_t factory_rdev;

int evl_init_element(struct evl_element *e,
		struct evl_factory *fac, int clone_flags)
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
	refcount_set(&e->refs, 1);
	e->dev = NULL;
	e->fpriv.filp = NULL;
	e->fpriv.efd = -1;
	e->fundle = EVL_NO_HANDLE;
	e->devname = NULL;
	e->clone_flags = clone_flags;

	return 0;
}

int evl_init_user_element(struct evl_element *e,
			struct evl_factory *fac,
			const char __user *u_name,
			int clone_flags)
{
	struct filename *devname;
	char tmpbuf[32];
	int ret;

	ret = evl_init_element(e, fac, clone_flags);
	if (ret)
		return ret;

	if (u_name) {
		devname = getname(u_name);
	} else {
		snprintf(tmpbuf, sizeof(tmpbuf), "%s%%%d",
			fac->name, e->minor);
		devname = getname_kernel(tmpbuf);
	}

	if (IS_ERR(devname)) {
		evl_destroy_element(e);
		return PTR_ERR(devname);
	}

	e->devname = devname;

	return 0;
}

void evl_destroy_element(struct evl_element *e)
{
	clear_bit(e->minor, e->factory->minor_map);
	if (e->devname)
		putname(e->devname);
}

static int bind_file_to_element(struct file *filp, struct evl_element *e)
{
	struct evl_file_binding *fbind;
	int ret;

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

static struct evl_element *unbind_file_from_element(struct file *filp)
{
	struct evl_file_binding *fbind = filp->private_data;
	struct evl_element *e = fbind->element;

	evl_release_file(&fbind->efile);
	kfree(fbind);

	return e;
}

/*
 * evl_open_element() might race with __do_put_element() for the same
 * element before the backing device is removed. If the refcount is
 * zero on entry, evl_open_element() knows that __evl_put_element() is
 * about to delete @e, returns -ESTALE if so.
 */
int evl_open_element(struct inode *inode, struct file *filp)
{
	struct evl_element *e;
	int ret = 0;

	e = container_of(inode->i_cdev, struct evl_element, cdev);
	if (!__evl_get_element(e))
		return -ESTALE;

	ret = bind_file_to_element(filp, e);
	if (ret) {
		evl_put_element(e);
		return ret;
	}

	stream_open(inode, filp);

	return 0;
}

static void __do_put_element(struct evl_element *e)
{
	struct evl_factory *fac = e->factory;

	/*
	 * We might get there device-less if create_element_device()
	 * failed installing a file descriptor for a private
	 * element. Go to disposal immediately if so.
	 *
	 * CAUTION: the disposal handler should delay the release of
	 * @e's container at the next rcu idle period via kfree_rcu(),
	 * because the embedded e->cdev is still needed ahead for
	 * completing the file release process of public elements (see
	 * __fput()).
	 */
	if (likely(e->dev))
		evl_remove_element_device(e);

	/*
	 * e->minor is not reusable until evl_destroy_element() is
	 * called from the disposal handler, so there is no risk of
	 * reusing it too early.
	 */
	fac->dispose(e);
}

static void do_put_element_work(struct work_struct *work)
{
	struct evl_element *e;

	e = container_of(work, struct evl_element, work);
	__do_put_element(e);
}

static void do_put_element_irq(struct irq_work *work)
{
	struct evl_element *e;

	e = container_of(work, struct evl_element, irq_work);
	INIT_WORK(&e->work, do_put_element_work);
	schedule_work(&e->work);
}

void __evl_put_element(struct evl_element *e)
{
	/*
	 * These trampolines may look like a bit cheesy but we have no
	 * choice but offloading the disposal to an in-band task
	 * context. In (the rare) case the last ref. to an element was
	 * dropped from OOB(-protected) context or while hard irqs
	 * were off, we need to go via an irq_work->workqueue chain in
	 * order to run __do_put_element() eventually.
	 *
	 * NOTE: irq_work_queue() does not synchronize the interrupt
	 * log when called with hard irqs off.
	 */
	if (unlikely(running_oob() || hard_irqs_disabled())) {
		init_irq_work(&e->irq_work, do_put_element_irq);
		irq_work_queue(&e->irq_work);
	} else {
		__do_put_element(e);
	}
}
EXPORT_SYMBOL_GPL(__evl_put_element);

int evl_release_element(struct inode *inode, struct file *filp)
{
	struct evl_element *e;

	e = unbind_file_from_element(filp);
	evl_put_element(e);

	return 0;
}

static void release_sys_device(struct device *dev)
{
	kfree(dev);
}

static struct device *create_sys_device(dev_t rdev, struct evl_factory *fac,
					void *drvdata, const char *name)
{
	struct device *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
		return ERR_PTR(-ENOMEM);

	dev->devt = rdev;
	dev->class = fac->class;
	dev->type = &fac->type;
	dev->groups = fac->attrs;
	dev->release = release_sys_device;
	dev_set_drvdata(dev, drvdata);

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto fail;

	ret = device_register(dev);
	if (ret)
		goto fail;

	return dev;

fail:
	put_device(dev); /* ->release_sys_device() */

	return ERR_PTR(ret);
}

static struct file_operations dummy_fops = {
	.owner = THIS_MODULE,
};

static int do_element_visibility(struct evl_element *e,
				struct evl_factory *fac,
				dev_t *rdev)
{
	struct file *filp;
	int ret, efd;

	if (EVL_WARN_ON(CORE, !evl_element_has_coredev(e) && !current->mm))
		e->clone_flags |= EVL_CLONE_COREDEV;

	/*
	 * Unlike a private one, a publicly visible element exports a
	 * cdev in the /dev/evl hierarchy so that any process can see
	 * it.  Both types are backed by a kernel device object so
	 * that we can export their state to userland via /sysfs.
	 */

	if (evl_element_is_public(e)) {
		*rdev = MKDEV(MAJOR(fac->sub_rdev), e->minor);
		cdev_init(&e->cdev, fac->fops);
		return cdev_add(&e->cdev, *rdev, 1);
	}

	*rdev = MKDEV(0, e->minor);

	if (evl_element_has_coredev(e))
		return 0;

	/*
	 * Create a private user element, passing the real fops so
	 * that FMODE_CAN_READ/WRITE are set accordingly by the vfs.
	 */
	filp = anon_inode_getfile(evl_element_name(e), fac->fops,
				NULL, O_RDWR);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		return ret;
	}

	/*
	 * Now switch to dummy fops temporarily, until calling
	 * evl_release_element() is safe for filp, meaning once
	 * bind_file_to_element() has returned successfully.
	 */
	replace_fops(filp, &dummy_fops);

	/*
	 * There will be no open() call for this new private element
	 * since we have no associated cdev, bind it to the anon file
	 * immediately.
	 */
	ret = bind_file_to_element(filp, e);
	if (ret) {
		filp_close(filp, current->files);
		/*
		 * evl_release_element() was not called: do a manual
		 * disposal.
		 */
		fac->dispose(e);
		return ret;
	}

	/* Back to the real fops for this element class. */
	replace_fops(filp, fac->fops);

	efd = get_unused_fd_flags(O_RDWR|O_CLOEXEC);
	if (efd < 0) {
		filp_close(filp, current->files);
		ret = efd;
		return ret;
	}

	e->fpriv.filp = filp;
	e->fpriv.efd = efd;

	return 0;
}

static int create_element_device(struct evl_element *e,
				struct evl_factory *fac)
{
	struct evl_element *n;
	struct device *dev;
	dev_t rdev;
	u64 hlen;
	int ret;

	/*
	 * Do a quick hash check on the new element name, to make sure
	 * device_register() won't trigger a kernel log splash because
	 * of a naming conflict.
	 */
	hlen = hashlen_string("EVL", e->devname->name);

	mutex_lock(&fac->hash_lock);

	hash_for_each_possible(fac->name_hash, n, hash, hlen)
		if (!strcmp(n->devname->name, e->devname->name)) {
			mutex_unlock(&fac->hash_lock);
			goto fail_hash;
		}

	hash_add(fac->name_hash, &e->hash, hlen);

	mutex_unlock(&fac->hash_lock);

	ret = do_element_visibility(e, fac, &rdev);
	if (ret)
		goto fail_visibility;

	dev = create_sys_device(rdev, fac, e, evl_element_name(e));
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto fail_device;
	}

	/*
	 * Install fd on a private user element file only when we
	 * cannot fail creating the device anymore. First take a
	 * reference then install fd (which is a membar).
	 */
	if (!evl_element_is_public(e) && !evl_element_has_coredev(e)) {
		refcount_inc(&e->refs);
		fd_install(e->fpriv.efd, e->fpriv.filp);
	}

	e->dev = dev;

	return 0;

	/*
	 * On error, public and/or core-owned elements should be
	 * discarded by the caller.  Private user elements must be
	 * disposed of in this routine if we cannot give them a
	 * device.
	 */
fail_hash:
	if (!evl_element_is_public(e) && !evl_element_has_coredev(e))
		fac->dispose(e);

	return -EEXIST;

fail_device:
	if (evl_element_is_public(e)) {
		cdev_del(&e->cdev);
	} else if (!evl_element_has_coredev(e)) {
		put_unused_fd(e->fpriv.efd);
		filp_close(e->fpriv.filp, current->files);
	}

fail_visibility:
	mutex_lock(&fac->hash_lock);
	hash_del(&e->hash);
	mutex_unlock(&fac->hash_lock);

	return ret;
}

int evl_create_core_element_device(struct evl_element *e,
				struct evl_factory *fac,
				const char *name)
{
	struct filename *devname;

	if (name) {
		devname = getname_kernel(name);
		if (devname == NULL)
			return PTR_ERR(devname);
		e->devname = devname;
	}

	e->clone_flags |= EVL_CLONE_COREDEV;

	return create_element_device(e, fac);
}

void evl_remove_element_device(struct evl_element *e)
{
	struct evl_factory *fac = e->factory;
	struct device *dev = e->dev;

	device_unregister(dev);

	if (evl_element_is_public(e))
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
	__u32 val, state_offset = -1U;
	const char __user *u_name;
	struct evl_factory *fac;
	void __user *u_attrs;
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

	u_name = evl_valptr64(req.name_ptr, const char);
	if (u_name == NULL && req.clone_flags & EVL_CLONE_PUBLIC)
		return -EINVAL;

	u_attrs = evl_valptr64(req.attrs_ptr, void);
	fac = container_of(filp->f_inode->i_cdev, struct evl_factory, cdev);
	e = fac->build(fac, u_name, u_attrs, req.clone_flags, &state_offset);
	if (IS_ERR(e))
		return PTR_ERR(e);

	/* This must be set before the device appears. */
	filp->private_data = e;
	barrier();

	ret = create_element_device(e, fac);
	if (ret) {
		/* release_clone_device() must skip cleanup. */
		filp->private_data = NULL;
		/*
		 * If we failed to create a private element,
		 * evl_release_element() did run via filp_close(), so
		 * the disposal has taken place already.
		 *
		 * NOTE: this code should never directly handle core
		 * devices, since we are running the user interface to
		 * cloning a new element. Although a thread may be
		 * associated with a coredev observable, the latter
		 * does not export any direct interface to user.
		 */
		EVL_WARN_ON(CORE, evl_element_has_coredev(e));
		/*
		 * @e might be stale if it was private, test the
		 * visibility flag from the request block instead.
		 */
		if (req.clone_flags & EVL_CLONE_PUBLIC)
			fac->dispose(e);
		return ret;
	}

	val = e->minor;
	ret |= put_user(val, &u_req->eids.minor);
	val = e->fundle;
	ret |= put_user(val, &u_req->eids.fundle);
	ret |= put_user(state_offset, &u_req->eids.state_offset);
	val = e->fpriv.efd;
	ret |= put_user(val, &u_req->efd);

	return ret ? -EFAULT : 0;
}

static int release_clone_device(struct inode *inode, struct file *filp)
{
	struct evl_element *e = filp->private_data;

	if (e)
		evl_put_element(e);

	return 0;
}

static int open_clone_device(struct inode *inode, struct file *filp)
{
	struct evl_factory *fac;

	fac = container_of(filp->f_inode->i_cdev, struct evl_factory, cdev);
	fac->kuid = inode->i_uid;
	fac->kgid = inode->i_gid;
	stream_open(inode, filp);

	return 0;
}

static const struct file_operations clone_fops = {
	.open		= open_clone_device,
	.release	= release_clone_device,
	.unlocked_ioctl	= ioctl_clone_device,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
#endif
};

static int index_element_at(struct evl_index *map,
			struct evl_element *e, fundle_t fundle)
{
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

void evl_index_element(struct evl_index *map, struct evl_element *e)
{
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

		ret = index_element_at(map, e, fundle);

		raw_spin_unlock_irqrestore(&map->lock, flags);
	} while (ret);
}

void evl_unindex_element(struct evl_index *map, struct evl_element *e)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&map->lock, flags);
	rb_erase(&e->index_node, &map->root);
	raw_spin_unlock_irqrestore(&map->lock, flags);
}

struct evl_element *
__evl_get_element_by_fundle(struct evl_index *map, fundle_t fundle)
{
	struct evl_element *e;
	unsigned long flags;
	struct rb_node *rb;

	raw_spin_lock_irqsave(&map->lock, flags);

	rb = map->root.rb_node;
	while (rb) {
		e = rb_entry(rb, struct evl_element, index_node);
		if (fundle < e->fundle) {
			rb = rb->rb_left;
		} else if (fundle > e->fundle) {
			rb = rb->rb_right;
		} else {
			if (unlikely(!refcount_inc_not_zero(&e->refs)))
				e = NULL;
			break;
		}
	}

	raw_spin_unlock_irqrestore(&map->lock, flags);

	return rb ? e : NULL;
}

static char *factory_type_devnode(const struct device *dev, umode_t *mode,
			kuid_t *uid, kgid_t *gid)
{
	struct evl_element *e;

	/*
	 * Inherit the ownership of a new element device from the
	 * clone device which has instantiated it.
	 */
	e = dev_get_drvdata(dev);
	if (e) {
		if (uid)
			*uid = e->factory->kuid;
		if (gid)
			*gid = e->factory->kgid;
	}

	return kasprintf(GFP_KERNEL, "evl/%s/%s",
			dev->type->name, dev_name(dev));
}

static int create_element_class(struct evl_factory *fac)
{
	struct class *class;
	int ret = -ENOMEM;

	fac->minor_map = bitmap_zalloc(fac->nrdev, GFP_KERNEL);
	if (fac->minor_map == NULL)
		return ret;

	class = class_create(fac->name);
	if (IS_ERR(class)) {
		ret = PTR_ERR(class);
		goto cleanup_minor;
	}

	fac->class = class;
	fac->type.name = fac->name;
	fac->type.devnode = factory_type_devnode;
	fac->kuid = GLOBAL_ROOT_UID;
	fac->kgid = GLOBAL_ROOT_GID;

	ret = alloc_chrdev_region(&fac->sub_rdev, 0, fac->nrdev, fac->name);
	if (ret)
		goto cleanup_class;

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

int evl_create_factory(struct evl_factory *fac, dev_t rdev)
{
	const char *idevname = "clone"; /* Initial device in factory. */
	struct device *dev = NULL;
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

		dev = create_sys_device(rdev, fac, NULL, idevname);
		if (IS_ERR(dev))
			goto fail_dev;
	}

	fac->dev = dev;
	raw_spin_lock_init(&fac->index.lock);
	fac->index.root = RB_ROOT;
	fac->index.generator = EVL_NO_HANDLE;
	hash_init(fac->name_hash);
	mutex_init(&fac->hash_lock);

	return 0;

fail_dev:
	cdev_del(&fac->cdev);
fail_cdev:
	if (!(fac->flags & EVL_FACTORY_SINGLE))
		delete_element_class(fac);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_create_factory);

void evl_delete_factory(struct evl_factory *fac)
{
	struct device *dev = fac->dev;

	if (dev) {
		device_unregister(dev);
		cdev_del(&fac->cdev);
	}

	if (!(fac->flags & EVL_FACTORY_SINGLE))
		delete_element_class(fac);
}
EXPORT_SYMBOL_GPL(evl_delete_factory);

bool evl_may_access_factory(struct evl_factory *fac)
{
	const struct cred *cred = current_cred();

	return capable(CAP_SYS_ADMIN) || uid_eq(cred->euid, fac->kuid);
}
EXPORT_SYMBOL_GPL(evl_may_access_factory);

static char *evl_devnode(const struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "evl/%s", dev_name(dev));
}

static int __init
create_core_factories(struct evl_factory **factories, int nr)
{
	int ret, n;

	for (n = 0; n < nr; n++) {
		ret = evl_create_factory(factories[n],
				MKDEV(MAJOR(factory_rdev), n));
		if (ret)
			goto fail;
	}

	return 0;
fail:
	while (n-- > 0)
		evl_delete_factory(factories[n]);

	return ret;
}

static void __init
delete_core_factories(struct evl_factory **factories, int nr)
{
	int n;

	for (n = 0; n < nr; n++)
		evl_delete_factory(factories[n]);
}

int __init evl_early_init_factories(void)
{
	int ret;

	evl_class = class_create("evl");
	if (IS_ERR(evl_class))
		return PTR_ERR(evl_class);

	evl_class->devnode = evl_devnode;

	ret = alloc_chrdev_region(&factory_rdev, 0, NR_FACTORIES,
				"evl_factory");
	if (ret) {
		class_destroy(evl_class);
		return ret;
	}

	ret = create_core_factories(early_factories,
			ARRAY_SIZE(early_factories));
	if (ret) {
		unregister_chrdev_region(factory_rdev, NR_FACTORIES);
		class_destroy(evl_class);
	}

	return ret;
}

void __init evl_early_cleanup_factories(void)
{
	delete_core_factories(early_factories, ARRAY_SIZE(early_factories));
	unregister_chrdev_region(factory_rdev, NR_FACTORIES);
	class_destroy(evl_class);
}

int __init evl_late_init_factories(void)
{
	return create_core_factories(factories, ARRAY_SIZE(factories));
}

void __init evl_late_cleanup_factories(void)
{
	delete_core_factories(factories, ARRAY_SIZE(factories));
}
