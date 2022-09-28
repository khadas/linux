/*
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/pid.h>
#include <linux/compat.h>
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <uapi/linux/amlogic/media_info.h>

enum {
	LOG_ERR = 0,
	LOG_WARN = 1,
	LOG_INFO = 2,
	LOG_DEBUG = 3,
	LOG_TRACE = 4,
};

#define minfo_dbg(level, x...) \
	do { \
		if ((level) <= log_level) \
			pr_info(x); \
	} while (0)

struct minfo {
	s32 major;
	struct mutex info_mutex;
	struct device *minfo_dev;

	/* records */
	u32 cur_id;
	struct list_head rhead;
	/* keys for poll */
	struct list_head khead;

	/* wait queue for poll */
	wait_queue_head_t poll_wait;
	u32  event_pending;
};

struct record {
	struct list_head node;
	char *kv;
	u32 id;
	pid_t pid; /* belongs to pid, -1 means from kernel */
	u64 ts; /* timestamp of the record */
};

/* key of interest */
struct koi {
	struct list_head node;
	char *key;
	pid_t pid; /* belongs to pid, -1 means from kernel */
	bool trigger;
	struct notifier_block *nb; /* for pid -1 only */
};

struct read_priv {
	char *content;
	size_t size;
	size_t rsize;
};

#define DEF_RECORD_SIZE (8 * 1024)
static struct minfo info;
static int log_level = 2;
static BLOCKING_NOTIFIER_HEAD(minfo_notifier_list);

static void minfo_clean_up_by_pid(pid_t pid);
static int minfo_notifier_call_chain(unsigned long val, void *v);

static int minfo_open(struct inode *inode, struct file *file)
{
	minfo_dbg(LOG_DEBUG, "minfo opened by pid %d\n",
		task_tgid_nr(current));
	file->private_data = NULL;
	return 0;
}

static int minfo_release(struct inode *inode, struct file *file)
{
	pid_t pid = task_tgid_nr(current);

	minfo_dbg(LOG_DEBUG, "minfo closed by pid %d\n", pid);
	minfo_clean_up_by_pid(pid);
	if (file->private_data) {
		struct read_priv *r =
			(struct read_priv *)file->private_data;
		vfree(r->content);
		vfree(r);
	}
	return 0;
}

static unsigned int minfo_poll(struct file *file, poll_table *wait_table)
{
	poll_wait(file, &info.poll_wait, wait_table);
	if (info.event_pending)
		return POLLIN;
	return 0;
}

static void wakeup_poll(void)
{
	info.event_pending = true;
	wake_up_interruptible(&info.poll_wait);
}

static bool minfo_koi_locked(char *key, pid_t pid)
{
	struct list_head *p;
	struct koi *k = NULL;
	bool ret = false;

	list_for_each(p, &info.khead) {
		k = list_entry(p, struct koi, node);
		if (!strncmp(key, k->key, strlen(k->key))) {
			if (k->pid != -1)
				k->trigger = true;
			ret = true;
			break;
		}
	}
	return ret;
}

static void minfo_koi_clear(pid_t pid)
{
	struct list_head *p;
	struct koi *k = NULL;
	bool clear_event = true;

	mutex_lock(&info.info_mutex);
	list_for_each(p, &info.khead) {
		k = list_entry(p, struct koi, node);
		if (k->pid == pid && k->trigger) {
			k->trigger = false;
			minfo_dbg(LOG_DEBUG, "minfo pid %d clear key %s\n",
					pid, k->key);
		} else if (k->trigger) {
			clear_event = false;
		}
	}
	if (clear_event)
		info.event_pending = false;
	mutex_unlock(&info.info_mutex);
}

static bool minfo_koi_triggerd(pid_t pid)
{
	struct list_head *p;
	struct koi *k = NULL;
	bool ret = false;

	mutex_lock(&info.info_mutex);
	list_for_each(p, &info.khead) {
		k = list_entry(p, struct koi, node);
		if (k->pid == pid && k->trigger) {
			ret = true;
			goto exit;
		}
	}
exit:
	mutex_unlock(&info.info_mutex);
	return ret;
}

static void minfo_clean_up_by_pid(pid_t pid)
{
	struct list_head *p, *n;
	struct record *r = NULL;
	struct koi *k = NULL;
	bool trigger_poll = false;

	mutex_lock(&info.info_mutex);
	/* remove from koi first */
	list_for_each_safe(p, n, &info.khead) {
		k = list_entry(p, struct koi, node);
		if (k->pid == pid || pid == -1) {
			list_del(&k->node);
			minfo_dbg(LOG_INFO, "minfo pid %d remove key %s\n",
					pid, k->key);
			vfree(k->key);
			vfree(k);
		}
	}

	/* remove all the records from this pid */
	list_for_each_safe(p, n, &info.rhead) {
		r = list_entry(p, struct record, node);
		if (r->pid == pid || pid == -1) {
			list_del(&r->node);
			if (minfo_koi_locked(r->kv, pid))
				trigger_poll = true;
			minfo_dbg(LOG_INFO, "minfo pid %d remove [%d]%s\n",
					pid, r->id, r->kv);
			vfree(r->kv);
			vfree(r);
		}
	}
	mutex_unlock(&info.info_mutex);

	if (trigger_poll)
		wakeup_poll();
	else
		minfo_koi_clear(pid);
}

static int minfo_record_add_check_locked(struct record *r)
{
	struct list_head *p;
	struct record *rr, *rf = NULL;

	/* remove old record */
	list_for_each(p, &info.rhead) {
		rr = list_entry(p, struct record, node);
		if (rr->pid == r->pid && rr->id == r->id) {
			minfo_dbg(LOG_DEBUG, "minfo pid %d update kv [%d]%s\n",
					r->pid, rr->id, rr->kv);
			rf = rr;
			break;
		}
	}
	if (rf) {
		list_del(&rf->node);
		vfree(rf->kv);
		vfree(rf);
	}

	/* add new record */
	list_add(&r->node, &info.rhead);

	if (r->pid != -1 && minfo_koi_locked(r->kv, r->pid))
		wakeup_poll();
	else if (r->pid == -1 && minfo_koi_locked(r->kv, r->pid))
		minfo_notifier_call_chain(0, r->kv);
	minfo_dbg(LOG_INFO, "minfo pid %d add [%d]%s\n",
			r->pid, r->id, r->kv);
	return 0;
}

int media_info_post_kv(const char *kv, uint32_t *id)
{
	struct record *r;
	char *v = vzalloc(AMEDIA_INFO_KV_MAX_LEN + 1);
	int rc;

	if (!v)
		return -ENOMEM;
	if (strncpy(v, kv, AMEDIA_INFO_KV_MAX_LEN + 1))
		return -EFAULT;
	r = vzalloc(sizeof(*r));
	if (!r) {
		vfree(v);
		return -ENOMEM;
	}
	r->kv = v;
	r->pid = -1;
	r->ts = ktime_get_raw_ns();

	mutex_lock(&info.info_mutex);
	if (*id == -1) {
		r->id = info.cur_id++;
		*id = r->id;
		if (info.cur_id == -1)
			info.cur_id = 0;
	}
	rc = minfo_record_add_check_locked(r);
	mutex_unlock(&info.info_mutex);
	return rc;
}

static int minfo_record_del_check_locked(const u32 rid, pid_t pid)
{
	struct list_head *p;
	struct record *r, *rr = NULL;

	list_for_each(p, &info.rhead) {
		r = list_entry(p, struct record, node);
		if (rid == r->id && pid == r->pid) {
			rr = r;
			break;
		}
	}

	if (!rr)
		return -EFAULT;

	list_del(&rr->node);
	if (minfo_koi_locked(rr->kv, pid))
		wakeup_poll();
	minfo_dbg(LOG_INFO, "minfo pid %d remove %s\n", task_tgid_nr(current), rr->kv);
	vfree(rr->kv);
	vfree(rr);
	return 0;
}

static int minfo_key_add_check_locked(struct koi *k)
{
	struct list_head *p;
	struct koi *kk;

	list_for_each(p, &info.khead) {
		kk = list_entry(p, struct koi, node);
		if (kk->pid == k->pid &&
				!strncmp(kk->key, k->key, strlen(k->key))) {
			minfo_dbg(LOG_DEBUG, "minfo pid %d key %s existed\n",
					k->pid, k->key);
			return 0;
		}
	}

	list_add(&k->node, &info.khead);
	minfo_dbg(LOG_DEBUG, "minfo k %s monitored by pid %d\n",
			k->key, k->pid);
	return 0;
}
static long minfo_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	int rc = 0;
	void __user *argp = (void __user *)arg;
	pid_t pid;

	switch (cmd) {
	case AMI_IOC_SET:
	{
		struct info_record ruser;
		struct record *r;
		char *kv;
		u32 id;

		if (copy_from_user(&ruser, argp, sizeof(ruser)))
			return -EFAULT;

		kv = vzalloc(AMEDIA_INFO_KV_MAX_LEN + 1);
		if (!kv)
			return -ENOMEM;
		strncpy(kv, ruser.kv, AMEDIA_INFO_KV_MAX_LEN);
		r = vzalloc(sizeof(*r));
		if (!r) {
			vfree(kv);
			return -ENOMEM;
		}
		pid = task_tgid_nr(current);
		r->kv = kv;
		r->pid = pid;
		r->ts = ktime_get_raw_ns();

		mutex_lock(&info.info_mutex);
		if (ruser.id == -1) {
			r->id = info.cur_id++;
			id = r->id;
			if (info.cur_id == -1)
				info.cur_id = 0;
		}
		rc = minfo_record_add_check_locked(r);
		mutex_unlock(&info.info_mutex);

		if (!rc && ruser.id == -1)
			put_user(id, (u32 __user *)argp);
		break;
	}
	case AMI_IOC_DEL:
	{
		struct info_record ruser;

		if (copy_from_user(&ruser, argp, sizeof(ruser)))
			return -EFAULT;

		pid = task_tgid_nr(current);
		mutex_lock(&info.info_mutex);
		rc = minfo_record_del_check_locked(ruser.id, pid);
		mutex_unlock(&info.info_mutex);
		break;
	}
	case AMI_IOC_POLL_SET_TYPE:
	{
		struct koi *key;
		struct info_key info_k;
		char *v = vzalloc(AMEDIA_INFO_KEY_MAX_LEN + 1);

		if (!v)
			return -ENOMEM;

		if (copy_from_user(&info_k, argp, sizeof(info_k)))
			return -EFAULT;
		strncpy(v, info_k.k, AMEDIA_INFO_KEY_MAX_LEN + 1);

		key = vzalloc(sizeof(*key));
		if (!key) {
			vfree(v);
			return -ENOMEM;
		}
		key->key = v;
		key->pid = task_tgid_nr(current);

		mutex_lock(&info.info_mutex);
		rc = minfo_key_add_check_locked(key);
		mutex_unlock(&info.info_mutex);
		break;
	}

	default:
		minfo_dbg(LOG_ERR, "minfo unsupported ioctl %u\n", cmd);
		rc = -1;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long minfo_compat_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	long ret;

	arg = (ulong)compat_ptr(arg);
	ret = minfo_ioctl(filp, cmd, arg);
	return ret;
}
#endif

static size_t minfo_lookup_by_pid(pid_t pid, struct read_priv *rp)
{
	struct list_head *p, *q;
	struct record *r = NULL;
	struct koi *k = NULL;

	mutex_lock(&info.info_mutex);
	list_for_each(p, &info.rhead) {
		r = list_entry(p, struct record, node);

		list_for_each(q, &info.khead) {
			k = list_entry(q, struct koi, node);
			if (pid == k->pid &&
					!strncmp(r->kv, k->key, strlen(k->key))) {
				size_t len = strlen(r->kv);

				/* realloc */
				if (rp->rsize + len + 1 > rp->size) {
					void *tmp = vzalloc(rp->size + DEF_RECORD_SIZE);

					if (!tmp) {
						mutex_unlock(&info.info_mutex);
						minfo_dbg(LOG_ERR, "%d OOM", __LINE__);
						return -1;
					}
					memcpy(tmp, rp->content, rp->size);
					vfree(rp->content);
					rp->content = tmp;
					rp->size = rp->size + DEF_RECORD_SIZE;
				}
				memcpy(rp->content + rp->rsize, r->kv, len);
				rp->rsize += len + 1;
				rp->content[rp->rsize] = ';';
			}
		}
	}
	mutex_unlock(&info.info_mutex);
	return rp->rsize;
}

ssize_t minfo_read(struct file *file, char __user *buf,
			size_t size, loff_t *off)
{
	struct read_priv *rp = file->private_data;
	pid_t pid = task_tgid_nr(current);
	ssize_t ret_size = 0;
	int rc;

	if (!minfo_koi_triggerd(pid))
		return -EIO;

	if (!rp) {
		rp = vzalloc(sizeof(*rp));
		if (!rp)
			return -ENOMEM;
		rp->content = vzalloc(DEF_RECORD_SIZE);
		if (!rp->content) {
			vfree(rp);
			return -ENOMEM;
		}
		rp->size = DEF_RECORD_SIZE;
		rp->rsize = 0;
		file->private_data = rp;
		ret_size = minfo_lookup_by_pid(pid, rp);
		minfo_dbg(LOG_DEBUG, "minfo read init by pid %d size %zd\n",
					pid, ret_size);
		if (ret_size <= 0) {
			vfree(rp->content);
			vfree(rp);
			file->private_data = NULL;
			*off = 0;
			minfo_koi_clear(pid);
			return -EIO;
		}
	} else {
		ret_size = rp->rsize - *off;
	}
	ret_size = size > ret_size ? ret_size : size;
	rc = copy_to_user(buf, rp->content + *off, ret_size);
	if (rc) {
		ret_size = -EFAULT;
	} else {
		*off += ret_size;
		if (*off == rp->rsize) {
			vfree(rp->content);
			vfree(rp);
			file->private_data = NULL;
			*off = 0;
			minfo_koi_clear(pid);
			minfo_dbg(LOG_DEBUG,
					"minfo read done by pid %d event_pending %d\n",
					pid, info.event_pending);
		}
	}
	return ret_size;
}

static const struct file_operations minfo_fops = {
	.owner = THIS_MODULE,
	.open = minfo_open,
	.release = minfo_release,
	.unlocked_ioctl = minfo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = minfo_compat_ioctl,
#endif
	.poll = minfo_poll,
	.read = minfo_read,
};

int media_info_register_event(const char *key, struct notifier_block *nb)
{
	struct koi *k;
	char *v = vzalloc(AMEDIA_INFO_KEY_MAX_LEN + 1);

	if (!v)
		return -ENOMEM;
	strncpy(v, key, AMEDIA_INFO_KEY_MAX_LEN + 1);
	k = vzalloc(sizeof(*k));
	if (!k) {
		vfree(v);
		return -ENOMEM;
	}
	k->key = v;
	k->pid = -1;
	k->nb = nb;

	mutex_lock(&info.info_mutex);
	minfo_key_add_check_locked(k);
	mutex_unlock(&info.info_mutex);

	return blocking_notifier_chain_register(&minfo_notifier_list, nb);
}
EXPORT_SYMBOL(media_info_register_event);

int media_info_unregister_event(struct notifier_block *nb)
{
	struct list_head *p, *n;
	struct koi *k = NULL;

	mutex_lock(&info.info_mutex);
	list_for_each_safe(p, n, &info.khead) {
		k = list_entry(p, struct koi, node);
		if (k->pid == -1 && k->nb == nb) {
			list_del(&k->node);
			vfree(k->key);
			vfree(k);
		}
	}
	mutex_unlock(&info.info_mutex);

	return blocking_notifier_chain_unregister(&minfo_notifier_list, nb);
}
EXPORT_SYMBOL(media_info_unregister_event);

static int minfo_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&minfo_notifier_list, val, v);
}

static ssize_t log_level_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", log_level);
}

static ssize_t log_level_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &log_level);
	if (r < 0)
		return -EINVAL;

	return count;
}

static ssize_t list_all_record_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct list_head *p;
	struct record *r = NULL;

	mutex_lock(&info.info_mutex);
	list_for_each(p, &info.rhead) {
		r = list_entry(p, struct record, node);
		pr_info("kv: %s pid: %d ts: %llu id:%d\n",
				r->kv, r->pid, r->ts, r->id);
	}
	mutex_unlock(&info.info_mutex);

	return sprintf(buf, "print to kernel log\n");
}

static ssize_t list_all_trigger_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct list_head *p;
	struct koi *k = NULL;

	mutex_lock(&info.info_mutex);
	list_for_each(p, &info.khead) {
		k = list_entry(p, struct koi, node);
		pr_info("k: %s pid: %d\n",
				k->key, k->pid);
	}
	mutex_unlock(&info.info_mutex);

	return sprintf(buf, "print to kernel log\n");
}
static struct class_attribute minfo_attrs[] = {
	__ATTR_RW(log_level),
	__ATTR_RO(list_all_record),
	__ATTR_RO(list_all_trigger),
	__ATTR_NULL
};

static struct attribute *minfo_class_attrs[] = {
	&minfo_attrs[0].attr,
	&minfo_attrs[1].attr,
	&minfo_attrs[2].attr,
	NULL
};
ATTRIBUTE_GROUPS(minfo_class);

static struct class minfo_class = {
	.name = "aml_minfo",
	.class_groups = minfo_class_groups,
};

int __init minfo_init(void)
{
	int r = 0;

	r = register_chrdev(AMEDIA_INFO_MAJOR, "aml_minfo", &minfo_fops);
	if (r < 0) {
		minfo_dbg(LOG_ERR,
			"Can't register major for aml_minfo device\n");
		return r;
	}
	info.major = r;

	r = class_register(&minfo_class);
	if (r) {
		minfo_dbg(LOG_ERR, "minfo class fail.\n");
		goto err;
	}

	info.minfo_dev = device_create(&minfo_class, NULL,
			MKDEV(AMEDIA_INFO_MAJOR, 0), NULL, "aml_minfo");

	if (IS_ERR(info.minfo_dev)) {
		minfo_dbg(LOG_ERR, "Can't create aml_info device\n");
		r = -ENXIO;
		goto err2;
	}
	mutex_init(&info.info_mutex);
	INIT_LIST_HEAD(&info.rhead);
	INIT_LIST_HEAD(&info.khead);
	init_waitqueue_head(&info.poll_wait);

	return 0;
err2:
	class_unregister(&minfo_class);
err:
	unregister_chrdev(AMEDIA_INFO_MAJOR, "aml_minfo");
	return r;
}

void __exit minfo_exit(void)
{
	minfo_clean_up_by_pid(-1);
	if (info.minfo_dev) {
		class_unregister(&minfo_class);
		device_destroy(&minfo_class, MKDEV(AMEDIA_INFO_MAJOR, 0));
	}
	unregister_chrdev(AMEDIA_INFO_MAJOR, "aml_minfo");
}

#ifndef MODULE
module_init(minfo_init);
module_exit(minfo_exit);
#endif

MODULE_LICENSE("GPL");
