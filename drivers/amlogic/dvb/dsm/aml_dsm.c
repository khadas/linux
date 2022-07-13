// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include "aml_dsm.h"

#define DEVICE_NAME "aml_dsm"
#define DEVICE_CLASS_NAME "aml_dsm"

struct dev_session;
struct dsm_session;
struct keytable_key;

enum {
	DSM_ROLE_OWNER = 1,
	DSM_ROLE_CLIENT
};

/**
 * struct dev_session
 * @dsm: pointer to dsm session data
 * @role: dsm session role
 */
struct dev_session {
	struct dsm_session *dsm;
	int role;
};

/**
 * struct dsm_session
 * @list: sessions list
 * @ref_count: reference counter
 * @key_count: key counter
 * @token: unique token
 * @properties: @properties
 * @lock: session lock
 * @keys: dscrambler keys
 */
struct dsm_session {
	struct list_head list;
	atomic_t ref_count;
	atomic_t key_count;
	__u32 token;
	__u32 properties[MAX_DSM_PROP];
	struct mutex lock; /*session lock*/
	struct list_head keys;
};

/**
 * struct keytable_key
 * @list: keys list
 * id: keyslot index id
 * @type: keyslot type
 */
struct keytable_key {
	struct list_head list;
	__u32 id;
	__u32 parity;
	__u32 algo;
	__u32 is_iv;
	__u32 is_enc;
};

static int debug = 1;
static struct list_head sessions_head;
static DEFINE_MUTEX(sessions_lock);
static dev_t dsm_devno;
static struct cdev *dsm_cdev;
static struct class *dsm_class;

module_param(debug, int, 0644);

static struct dsm_session *dsm_find_session_by_token(__u32 token)
{
	struct dsm_session *sess = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &sessions_head) {
		sess = list_entry(pos, struct dsm_session, list);
		if (sess->token == token)
			return sess;
	}
	return NULL;
}

static struct keytable_key *dsm_find_keyslot_by_id(struct dsm_session *sess,
						      __u32 id)
{
	struct keytable_key *key = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &sess->keys) {
		key = list_entry(pos, struct keytable_key, list);
		if (key->id == id)
			return key;
	}
	return NULL;
}

static long dsm_open_session(struct dev_session *dev,
				union dsm_para *para)
{
	struct dsm_session *sess = NULL;

	if (dev->dsm)
		return -EADDRINUSE;
	/**
	 * Do not allow prefix or mask in low 16 bits
	 */
	if (para->session.prefix & 0xFFFF)
		return -EINVAL;

	sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess)
		return -ENOMEM;

	atomic_set(&sess->ref_count, 1);
	atomic_set(&sess->key_count, 0);
	INIT_LIST_HEAD(&sess->keys);
	mutex_init(&sess->lock);

	mutex_lock(&sessions_lock);
	do {
		get_random_bytes(&sess->token, sizeof(sess->token));
		sess->token = para->session.prefix
				| (sess->token & (~para->session.mask));
	} while (dsm_find_session_by_token(sess->token));
	list_add_tail(&sess->list, &sessions_head);
	dev->dsm = sess;
	dev->role = DSM_ROLE_OWNER;
	para->session.token = sess->token;
	mutex_unlock(&sessions_lock);
	return 0;
}

static void dsm_close_session(struct dsm_session *sess)
{
	struct list_head *pos, *tmp;
	struct keytable_key *key;

	if (sess) {
		mutex_lock(&sess->lock);
		list_for_each_safe(pos, tmp, &sess->keys) {
			key = list_entry(pos, struct keytable_key, list);
			list_del(&key->list);
			kfree(key);
		}
		mutex_unlock(&sess->lock);
	}
}

static void dsm_unref_session(struct dev_session *dev)
{
	struct dsm_session *sess = dev->dsm;

	mutex_lock(&sessions_lock);
	if (sess) {
		if (atomic_sub_and_test(1, &sess->ref_count)) {
			dsm_close_session(sess);
			list_del(&sess->list);
			kfree(sess);
			dev->dsm = NULL;
		}
	}
	mutex_unlock(&sessions_lock);
}

static long dsm_add_keyslot(struct dev_session *dev, union dsm_para *para)
{
	long r = 0;
	struct keytable_key *key;
	struct dsm_session *sess = dev->dsm;

	if (!sess)
		return -EBADFD;

	if (dev->role != DSM_ROLE_OWNER)
		return -EPERM;

	key = kzalloc(sizeof(*key), GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	mutex_lock(&sess->lock);
	if (dsm_find_keyslot_by_id(sess, para->keyslot.id)) {
		kfree(key);
		r = -EEXIST;
	} else if (atomic_read(&sess->key_count) >= MAX_KEYSLOTS) {
		kfree(key);
		r = -EOVERFLOW;
	} else {
		key->id = para->keyslot.id;
		key->parity = para->keyslot.parity;
		key->algo = para->keyslot.algo;
		key->is_iv = para->keyslot.is_iv;
		key->is_enc = para->keyslot.is_enc;
		list_add_tail(&key->list, &sess->keys);
		atomic_inc(&sess->key_count);
	}
	mutex_unlock(&sess->lock);
	return r;
}

static long dsm_del_keyslot(struct dev_session *dev, union dsm_para *para)
{
	long r = -ENOKEY;
	struct dsm_session *sess = dev->dsm;
	struct keytable_key *key;

	if (!sess)
		return -EBADFD;

	if (dev->role != DSM_ROLE_OWNER)
		return -EPERM;

	mutex_lock(&sess->lock);
	key = dsm_find_keyslot_by_id(sess, para->keyslot.id);
	if (key) {
		list_del(&key->list);
		kfree(key);
		atomic_dec(&sess->key_count);
		r = 0;
	}
	mutex_unlock(&sess->lock);
	return r;
}

static long dsm_set_token(struct dev_session *dev, union dsm_para *para)
{
	long r = -ENXIO;
	struct dsm_session *sess;

	if (dev->dsm)
		return -EINVAL;

	mutex_lock(&sessions_lock);
	sess = dsm_find_session_by_token(para->session.token);
	if (sess) {
		atomic_inc(&sess->ref_count);
		dev->dsm = sess;
		dev->role = DSM_ROLE_CLIENT;
		r = 0;
	}
	mutex_unlock(&sessions_lock);
	return r;
}

static long dsm_get_keyslot_list(struct dev_session *dev, union dsm_para *para)
{
	struct dsm_session *sess = dev->dsm;
	__u32 i = 0;
	struct list_head *pos, *tmp;
	struct keytable_key *key;

	if (!sess)
		return -EINVAL;

	mutex_lock(&sess->lock);
	list_for_each_safe(pos, tmp, &sess->keys) {
		key = list_entry(pos, struct keytable_key, list);
		para->keyslot_list.keyslots[i].id = key->id;
		para->keyslot_list.keyslots[i].parity = key->parity;
		para->keyslot_list.keyslots[i].algo = key->algo;
		para->keyslot_list.keyslots[i].is_iv = key->is_iv;
		para->keyslot_list.keyslots[i].is_enc = key->is_enc;
		i++;
	}
	para->keyslot_list.count = atomic_read(&sess->key_count);
	mutex_unlock(&sess->lock);
	return 0;
}

static long dsm_set_property(struct dev_session *dev, union dsm_para *para)
{
	struct dsm_session *sess = dev->dsm;

	if (!sess)
		return -EINVAL;

	if (para->property.key >= MAX_DSM_PROP)
		return -EINVAL;

	mutex_lock(&sess->lock);
	sess->properties[para->property.key] = para->property.value;
	mutex_unlock(&sess->lock);
	return 0;
}

static long dsm_get_property(struct dev_session *dev, union dsm_para *para)
{
	struct dsm_session *sess = dev->dsm;

	if (!sess)
		return -EINVAL;

	if (para->property.key >= MAX_DSM_PROP)
		return -EINVAL;

	mutex_lock(&sess->lock);
	para->property.value = sess->properties[para->property.key];
	mutex_unlock(&sess->lock);
	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
int dsm_open(struct inode *inode, struct file *filp)
{
	struct dev_session *dev = NULL;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	filp->private_data = dev;
	return 0;
}

int dsm_close(struct inode *inode, struct file *filp)
{
	struct dev_session *dev;

	dev = filp->private_data;
	if (dev) {
		dsm_unref_session(dev);
		filp->private_data = NULL;
		kfree(dev);
	}
	return 0;
}

long dsm_ioctl(struct file *filp, unsigned int cmd, unsigned long userdata)
{
	long retval = 0;
	struct dev_session *dev = NULL;
	union dsm_para __user *argp = (void __user *)userdata;
	union dsm_para para;

	if (_IOC_TYPE(cmd) != DSM_IOC_MAGIC)
		return -EINVAL;

	dev = filp->private_data;
	if (!dev)
		return -EINVAL;

	if (copy_from_user((void *)&para, argp, sizeof(para)))
		return -EINVAL;

	switch (cmd) {
	case DSM_IOC_OPEN_SESSION:
		retval = dsm_open_session(dev, &para);
		break;
	case DSM_IOC_ADD_KEYSLOT:
		retval = dsm_add_keyslot(dev, &para);
		break;
	case DSM_IOC_DEL_KEYSLOT:
		retval = dsm_del_keyslot(dev, &para);
		break;
	case DSM_IOC_SET_TOKEN:
		retval = dsm_set_token(dev, &para);
		break;
	case DSM_IOC_GET_KEYSLOT_LIST:
		retval = dsm_get_keyslot_list(dev, &para);
		break;
	case DSM_IOC_SET_PROPERTY:
		retval = dsm_set_property(dev, &para);
		break;
	case DSM_IOC_GET_PROPERTY:
		retval = dsm_get_property(dev, &para);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	if (copy_to_user((void *)argp, &para, sizeof(para)))
		return -EFAULT;

	return retval;
}

#ifdef CONFIG_COMPAT
static long dsm_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = dsm_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

const struct file_operations dsm_fops = {
	.owner = THIS_MODULE,
	.open = dsm_open,
	.release = dsm_close,
	.unlocked_ioctl = dsm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dsm_compat_ioctl,
#endif
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */

#define APPEND_ATTR_BUF(format, args...) { \
	size += snprintf(buf + size, PAGE_SIZE - size, format, args); \
}

static ssize_t usage_show(struct class *class,
			  struct class_attribute *attr, char *buf)
{
	struct list_head *pos1, *tmp1;
	struct list_head *pos2, *tmp2;
	struct dsm_session *sess;
	struct keytable_key *key;
	ssize_t size = 0;

	mutex_lock(&sessions_lock);
	list_for_each_safe(pos1, tmp1, &sessions_head) {
		sess = list_entry(pos1, struct dsm_session, list);
		APPEND_ATTR_BUF("Session token: 0x%08x, keyslots: %d\n",
				sess->token,
				atomic_read(&sess->key_count))
		list_for_each_safe(pos2, tmp2, &sess->keys) {
			key = list_entry(pos2, struct keytable_key, list);
			APPEND_ATTR_BUF("\tSlot: %2d, ",
				key->id)
			APPEND_ATTR_BUF("parity: 0x%x, algo: %d, ",
				key->parity,
				key->algo)
			APPEND_ATTR_BUF("is_iv: %d, is_enc: %d\n",
				key->is_iv,
				key->is_enc)
		}
	}
	mutex_unlock(&sessions_lock);
	APPEND_ATTR_BUF("%s", "\n")
	return size;
}

#undef APPEND_ATTR_BUF

static struct class_attribute dsm_class_attrs[] = {
	__ATTR_RO(usage),
	__ATTR_NULL
};

static void remove_dsmsub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; dsm_class_attrs[i].attr.name; i++)
		class_remove_file(class, &dsm_class_attrs[i]);
}

static void create_dsmsub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; dsm_class_attrs[i].attr.name; i++) {
		if (class_create_file(class, &dsm_class_attrs[i]) < 0)
			break;
	}
}

int __init dsm_init(void)
{
	int result;
	struct device *dsm_dev;

	result = alloc_chrdev_region(&dsm_devno, 0, 1, DEVICE_NAME);

	if (result < 0) {
		result = -ENODEV;
		goto fail0;
	}

	dsm_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);

	if (IS_ERR(dsm_class)) {
		result = PTR_ERR(dsm_class);
		goto fail1;
	}
	create_dsmsub_attrs(dsm_class);

	dsm_cdev = kmalloc(sizeof(*dsm_cdev), GFP_KERNEL);
	if (!dsm_cdev) {
		result = -ENOMEM;
		goto fail2;
	}
	cdev_init(dsm_cdev, &dsm_fops);
	dsm_cdev->owner = THIS_MODULE;
	result = cdev_add(dsm_cdev, dsm_devno, 1);
	if (result)
		goto fail3;

	dsm_dev = device_create(dsm_class,
				  NULL, MKDEV(MAJOR(dsm_devno), 0),
				  NULL, DEVICE_NAME);

	if (IS_ERR(dsm_dev)) {
		result = PTR_ERR(dsm_dev);
		goto fail4;
	}

	INIT_LIST_HEAD(&sessions_head);

	return 0;
fail4:
	cdev_del(dsm_cdev);
fail3:
	kfree(dsm_cdev);
fail2:
	remove_dsmsub_attrs(dsm_class);
	class_destroy(dsm_class);
fail1:
	unregister_chrdev_region(dsm_devno, 1);
fail0:
	return result;
}

void __exit dsm_exit(void)
{
	cdev_del(dsm_cdev);
	kfree(dsm_cdev);
	device_destroy(dsm_class, MKDEV(MAJOR(dsm_devno), 0));
	remove_dsmsub_attrs(dsm_class);
	class_destroy(dsm_class);
	unregister_chrdev_region(dsm_devno, 1);
}

MODULE_AUTHOR("Tao Guo <tao.guo@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Descrambler Session Manager Driver");
MODULE_LICENSE("GPL");

module_init(dsm_init);
module_exit(dsm_exit);
