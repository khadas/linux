// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/ctype.h>
#if defined(CONFIG_AMLOGIC_EFUSE)
#include <linux/amlogic/efuse.h>
#endif
#include "unifykey.h"
#include "amlkey_if.h"
#include "security_key.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

#define UNIFYKEYS_DEVICE_NAME    "unifykeys"
#define UNIFYKEYS_CLASS_NAME     "unifykeys"

#define KEY_EMPTY	0
#define KEY_BURNED	1

#define SHA256_SUM_LEN	32
#define DBG_DUMP_DATA	(0)

typedef int (*key_unify_dev_init)(struct key_info_t *uk_info,
				  char *buf, unsigned int len);

static int unifykey_initialized;
static struct aml_uk_dev *ukdev_global;

/*
 * This strange API is for LCD driver to call my internal APIs,
 * And I donnot understand why they need to do this
 */
void  *get_ukdev(void)
{
	return ukdev_global;
}
EXPORT_SYMBOL(get_ukdev);

static struct key_item_t *uk_find_item_by_name(struct list_head *list,
					       char *name)
{
	struct key_item_t *item;

	list_for_each_entry(item, list, node)
		if (!strncmp(item->name, name,
			     ((strlen(item->name) > strlen(name)) ?
			     strlen(item->name) : strlen(name))))
			return item;

	return NULL;
}

static struct key_item_t *uk_find_item_by_id(struct list_head *list, int id)
{
	struct key_item_t *item;

	list_for_each_entry(item, list, node)
		if (item->id == id)
			return item;

	return NULL;
}

static int uk_count_key(struct list_head *list)
{
	int		 count = 0;
	struct list_head *node;

	list_for_each(node, list)
		count++;

	return count;
}

static int key_storage_read(char *name, unsigned char *data,
			    unsigned int len, unsigned int *real,
			    int flag)
{
	int	ret = 0;
	unsigned int read_len = 0;
	u32	encrypt;
	u32	encrypt_dts;

	if (amlkey_get_attr((u8 *)name) & KEY_ATTR_SECURE) {
		pr_err("key[%s] can't read, is configured secured?\n", name);
		return -EINVAL;
	}

	/* make sure attr in storage & dts are the same! */
	encrypt = amlkey_get_attr((u8 *)name) & KEY_ATTR_ENCRYPT;

	encrypt_dts = (u32)(flag & KEY_ATTR_ENCRYPT) ? 1 : 0;
	if (encrypt != encrypt_dts) {
		pr_err("key[%s] can't read, encrypt?\n", name);
		return -EINVAL;
	}

	read_len = amlkey_read((u8 *)name, (u8 *)data, len);
	if (read_len != len) {
		pr_err("key[%s], want read %u Bytes, but %u bytes\n",
		       name, len, read_len);
		return -EINVAL;
	}
	*real = read_len;
	return ret;
}

static int key_efuse_write(char *name, unsigned char *data, unsigned int len)
{
#if defined(CONFIG_AMLOGIC_EFUSE)
	struct efusekey_info info;

	if (efuse_getinfo(name, &info) < 0)
		return -EINVAL;

	if (efuse_user_attr_store(name, (const char *)data, (size_t)len) < 0) {
		pr_err("efuse write fail.\n");
		return -1;
	}

	pr_info("%s written done.\n", info.keyname);
	return 0;
#else
	pr_err("efusekey not supported!\n");
	return -EINVAL;
#endif
}

static int key_efuse_read(char *name, unsigned char *data,
			  unsigned int len, unsigned int *real)
{
#if defined(CONFIG_AMLOGIC_EFUSE)
	struct efusekey_info info;
	int		     err = 0;
	char		     *buf;

	if (efuse_getinfo(name, &info) < 0)
		return -EINVAL;

	buf = kzalloc(info.size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	err = efuse_user_attr_read(name, buf);
	if (err >= 0) {
		*real = info.size;
		if (len > info.size)
			len = info.size;
		memcpy(data, buf, len);
	}
	kfree(buf);
	return err;
#else
	pr_err("efusekey not supported!\n");
	return -EINVAL;
#endif
}

static int key_efuse_query(char *name, unsigned int *state)
{
#if defined(CONFIG_AMLOGIC_EFUSE)
	int		     i;
	int		     err = 0;
	struct efusekey_info info;
	char		     *buf;

	if (efuse_getinfo(name, &info) < 0)
		return -EINVAL;
	buf = kzalloc(info.size, GFP_KERNEL);
	if (!buf) {
		pr_err("%s:%d,kzalloc mem fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	err = efuse_user_attr_read(name, buf);
	*state = KEY_EMPTY;
	if (err > 0) {
		for (i = 0; i < info.size; i++) {
			if (buf[i] != 0) {
				*state = KEY_BURNED;
				break;
			}
		}
	}
	kfree(buf);
	return err;
#else
	pr_err("efusekey not supported!\n");
	return -EINVAL;
#endif
}

static int key_unify_init(struct aml_uk_dev *ukdev, char *buf,
			  unsigned int len)
{
	int ret;
	int enc_type;

	if (unlikely(!ukdev))
		return -EINVAL;

	if (ukdev->init_flag == 1) {
		pr_info("already inited!\n");
		return 0;
	}

	enc_type = ukdev->uk_info.encrypt_type;
	ret = amlkey_init((u8 *)buf, len, enc_type);
	if (ret < 0) {
		pr_err("device init fail\n");
		return ret;
	}

	ukdev->init_flag = 1;
	return 0;
}

int key_unify_write(struct aml_uk_dev *ukdev, char *name,
		    unsigned char *data, unsigned int len)
{
	int		  ret = 0;
	int		  attr;
	struct key_item_t *key;
	ssize_t write_len = 0;

	key = uk_find_item_by_name(&ukdev->uk_hdr, name);
	if (!key) {
		pr_err("can not find key %s\n", name);
		return -EINVAL;
	}

	if ((key->perm & KEY_PERM_WRITE) != KEY_PERM_WRITE) {
		pr_err("no write permission for key %s\n", name);
		return -EPERM;
	}

	switch (key->dev) {
	case KEY_EFUSE:
		ret = key_efuse_write(name, data, len);
		break;
	case KEY_SECURE:
	case KEY_NORMAL:
		attr = ((key->dev == KEY_SECURE) ? KEY_ATTR_SECURE : 0);
		attr |= (key->attr & KEY_ATTR_ENCRYPT) ? KEY_ATTR_ENCRYPT : 0;
		write_len = amlkey_write((u8 *)name, (u8 *)data, len, attr);
		if (write_len != len) {
			pr_err("%u != %zd bytes\n", len, write_len);
			ret = -EINVAL;
		}
		break;
	case KEY_UNKNOWN_DEV:
	default:
		pr_err("unknown device for key %s\n", name);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(key_unify_write);

int key_unify_read(struct aml_uk_dev *ukdev, char *name,
		   unsigned char *data, unsigned int len,
		   unsigned int *real)
{
	int ret = 0;
	struct key_item_t *key;

	if (!data) {
		pr_err("key data is NULL\n");
		return -EINVAL;
	}

	key = uk_find_item_by_name(&ukdev->uk_hdr, name);
	if (!key) {
		pr_err("key %s does not exist\n", name);
		return -EINVAL;
	}

	if ((key->perm & KEY_PERM_READ) != KEY_PERM_READ) {
		pr_err("no read permission for key %s\n", name);
		return -EPERM;
	}

	switch (key->dev) {
	case KEY_EFUSE:
		ret = key_efuse_read(name, data, len, real);
			break;
	case KEY_SECURE:
		ret = amlkey_hash((u8 *)name, data);
		if (ret)
			pr_err("fail to obtain hash for key %s\n", name);
		*real = SHA256_SUM_LEN;
		break;
	case KEY_NORMAL:
		ret = key_storage_read(name, data, len, real, key->attr);
		break;
	case KEY_UNKNOWN_DEV:
	default:
		pr_err("unknown device for key %s\n", name);
		break;
	}
	return ret;
}
EXPORT_SYMBOL(key_unify_read);

int key_unify_size(struct aml_uk_dev *ukdev, char *name, unsigned int *real)
{
	int		  ret = 0;
	struct key_item_t *key;

	key = uk_find_item_by_name(&ukdev->uk_hdr, name);
	if (!key) {
		pr_err("key %s does not exist\n", name);
		return -EINVAL;
	}

	if ((key->perm & KEY_PERM_READ) != KEY_PERM_READ) {
		pr_err("no read permission for key %s\n", name);
		return -EPERM;
	}

	switch (key->dev) {
	case KEY_EFUSE:
#if defined(CONFIG_AMLOGIC_EFUSE)
	{
		struct efusekey_info info;

		ret = efuse_getinfo(name, &info);
		if (ret == 0)
			*real = info.size;
	}
#else
		pr_err("does not support efusekey\n");
#endif
		break;
	case KEY_SECURE:
	case KEY_NORMAL:
		*real = amlkey_size((u8 *)name);
		break;
	case KEY_UNKNOWN_DEV:
	default:
		pr_err("unknown device for key %s\n", name);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(key_unify_size);

int key_unify_query(struct aml_uk_dev *ukdev, char *name,
		    unsigned int *state, unsigned int *perm)
{
	int		  ret = 0;
	struct key_item_t *key;

	key = uk_find_item_by_name(&ukdev->uk_hdr, name);
	if (!key) {
		pr_err("can not find key %s\n", name);
		return -EINVAL;
	}

	if ((key->perm & KEY_PERM_READ) != KEY_PERM_READ) {
		pr_err("no read permission for key %s\n", name);
		return -EINVAL;
	}

	switch (key->dev) {
	case KEY_EFUSE:
		ret = key_efuse_query(name, state);
		*perm = key->perm;
		break;
	case KEY_SECURE:
	case KEY_NORMAL:
		if (state)
			*state = amlkey_exsit((u8 *)name) ?
					KEY_BURNED : KEY_EMPTY;
		if (perm)
			*perm = key->perm;
		break;
	case KEY_UNKNOWN_DEV:
	default:
		pr_err("unknown device for key %s\n", name);
		break;
	}
	return ret;
}
EXPORT_SYMBOL(key_unify_query);

int key_unify_secure(struct aml_uk_dev *ukdev, char *name,
		     unsigned int *secure)
{
	int		  ret = 0;
	struct key_item_t *key;
	unsigned int	  state;

	key = uk_find_item_by_name(&ukdev->uk_hdr, name);
	if (!key) {
		pr_err("can not find key %s\n", name);
		return -EINVAL;
	}

	/* check key burned or not */
	ret = key_unify_query(ukdev, key->name, &state, NULL);
	if (ret != 0)
		return ret;

	*secure = 0;
	if (state == KEY_BURNED) {
		*secure = amlkey_get_attr((u8 *)(key->name)) & KEY_ATTR_SECURE;
	} else {
		if (key->dev == KEY_SECURE)
			*secure = 1;
		else
			*secure = 0;
	}

	return ret;
}
EXPORT_SYMBOL(key_unify_secure);

int key_unify_encrypt(struct aml_uk_dev *ukdev, char *name, unsigned int *enc)
{
	int		  ret = 0;
	struct key_item_t *key;
	unsigned int	  state;

	key = uk_find_item_by_name(&ukdev->uk_hdr, name);
	if (!key) {
		pr_err("can not find key %s\n", name);
		return -EINVAL;
	}

	/* check key burned or not */
	ret = key_unify_query(ukdev, key->name, &state, NULL);
	if (ret != 0)
		return ret;

	*enc = 0;

	if (state == KEY_BURNED)
		*enc = amlkey_get_attr((u8 *)(key->name)) & KEY_ATTR_ENCRYPT;
	else
		if (key->attr & KEY_ATTR_ENCRYPT)
			*enc = 1;

	return ret;
}
EXPORT_SYMBOL(key_unify_encrypt);

int key_unify_get_init_flag(void)
{
	return unifykey_initialized;
}
EXPORT_SYMBOL(key_unify_get_init_flag);

static int unifykey_open(struct inode *inode, struct file *file)
{
	struct aml_uk_dev *ukdev;

	ukdev = container_of(inode->i_cdev, struct aml_uk_dev, cdev);
	file->private_data = ukdev;

	return 0;
}

static int unifykey_release(struct inode *inode, struct file *file)
{
	return 0;
}

static loff_t unifykey_llseek(struct file *filp, loff_t off, int whence)
{
	struct aml_uk_dev *ukdev;
	loff_t			newpos;

	ukdev = filp->private_data;

	switch (whence) {
	case 0: /* SEEK_SET (start postion)*/
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = (loff_t)uk_count_key(&ukdev->uk_hdr) - 1;
		newpos = newpos - off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	if (newpos >= (loff_t)uk_count_key(&ukdev->uk_hdr))
		return -EINVAL;
	filp->f_pos = newpos;

	return newpos;
}

static long unifykey_unlocked_ioctl(struct file *file,
				    unsigned int cmd,
				    unsigned long arg)
{
	struct aml_uk_dev *ukdev;
	void __user		*argp = (void __user *)arg;
	struct key_item_t *key;
	struct key_item_info_t *item;
	unsigned int perm, state;
	int ret;

	ukdev = file->private_data;

	switch (cmd) {
	case KEYUNIFY_ATTACH:
		key = kmalloc(sizeof(*key), GFP_KERNEL);
		if (!key)
			return -ENOMEM;
		ret = copy_from_user(key, argp, sizeof(*key));
		if (ret != 0) {
			pr_err("copy_from_user fail\n");
			kfree(key);
			return ret;
		}
		ret = key_unify_init(ukdev, key->name, KEY_UNIFY_NAME_LEN);
		kfree(key);
		if (ret != 0)
			return ret;
		break;
	case KEYUNIFY_GET_INFO:
		item = kmalloc(sizeof(*item), GFP_KERNEL);
		if (!item)
			return -ENOMEM;
		ret = copy_from_user(item, argp, sizeof(*item));
		if (ret != 0) {
			pr_err("copy_from_user fail\n");
			kfree(item);
			return ret;
		}

		item->name[KEY_UNIFY_NAME_LEN - 1] = '\0';
		if (strlen(item->name))
			key = uk_find_item_by_name(&ukdev->uk_hdr, item->name);
		else
			key = uk_find_item_by_id(&ukdev->uk_hdr, item->id);
		if (!key) {
			pr_err("can not find key\n");
			kfree(item);
			return -EINVAL;
		}
		pr_err("%d, %s\n", key->id, key->name);

		ret = key_unify_query(ukdev, key->name, &state, &perm);
		if (ret != 0) {
			pr_err("fail to query key: %s\n", key->name);
			kfree(item);
			return ret;
		}
		item->perm = perm;
		item->flag = state;
		item->id = key->id;
		strncpy(item->name, key->name, (KEY_UNIFY_NAME_LEN - 1));
		item->name[KEY_UNIFY_NAME_LEN - 1] = '\0';
		ret = key_unify_size(ukdev, key->name, &item->size);
		if (ret != 0) {
			pr_err("fail to obtain key size of %s\n", key->name);
			kfree(item);
			return -EFAULT;
		}

		ret = copy_to_user(argp, item, sizeof(*item));
		if (ret != 0) {
			pr_err("copy_to_user fail\n");
			kfree(item);
			return ret;
		}

		kfree(item);
		return 0;
	default:
		pr_err("unsupported ioctrl cmd\n");
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long unifykey_compat_ioctl(struct file *file,
				  unsigned int cmd,
				  unsigned long arg)
{
	return unifykey_unlocked_ioctl(file, cmd,
				(unsigned long)compat_ptr(arg));
}
#endif

static ssize_t unifykey_read(struct file *file,
			     char __user *buf,
			     size_t count,
			     loff_t *ppos)
{
	struct aml_uk_dev *ukdev;
	int			ret;
	int			id;
	unsigned int		reallen;
	struct key_item_t	*item;
	char			*local_buf = NULL;

	ukdev = file->private_data;

	id = (int)(*ppos);
	item = uk_find_item_by_id(&ukdev->uk_hdr, id);
	if (!item) {
		ret =  -EINVAL;
		goto exit;
	}

	if (item->dev == KEY_SECURE)
		count = SHA256_SUM_LEN;
	local_buf = kzalloc(count, GFP_KERNEL);
	if (!local_buf) {
		pr_err("memory not enough,%s:%d\n",
		       __func__, __LINE__);
		return -ENOMEM;
	}

	ret = key_unify_read(ukdev, item->name, local_buf, count, &reallen);
	if (ret < 0)
		goto exit;
	if (count > reallen)
		count = reallen;
	if (copy_to_user((void *)buf, (void *)local_buf, count)) {
		ret =  -EFAULT;
		goto exit;
	}
	ret = count;
exit:
	kfree(local_buf);
	return ret;
}

static ssize_t unifykey_write(struct file *file,
			      const char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct aml_uk_dev *ukdev;
	int			ret;
	int			id;
	struct key_item_t	*item;
	char			*local_buf;

	ukdev = file->private_data;

	local_buf = kzalloc(count, GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	id = (int)(*ppos);
	item = uk_find_item_by_id(&ukdev->uk_hdr, id);
	if (!item) {
		ret =  -EINVAL;
		goto exit;
	}
	if (copy_from_user(local_buf, buf, count)) {
		ret =  -EFAULT;
		goto exit;
	}

	ret = key_unify_write(ukdev, item->name, local_buf, count);
	if (ret < 0)
		goto exit;
	ret = count;
exit:
	kfree(local_buf);
	return ret;
}

static ssize_t version_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	ssize_t n = 0;

	n = sprintf(buf, "version:1.0\n");
	buf[n] = 0;

	return n;
}

static ssize_t list_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct aml_uk_dev *ukdev;
	ssize_t			n = 0;
	int			index;
	int			key_cnt;
	struct key_item_t	*unifykey;
	const char *const keydev[] = {
		"unknown", "efuse", "normal", "secure"};

	ukdev = container_of(cla, struct aml_uk_dev, cls);

	key_cnt = uk_count_key(&ukdev->uk_hdr);
	n = sprintf(&buf[0], "%d keys installed\n", key_cnt);

	for (index = 0; index < key_cnt; index++) {
		unifykey = uk_find_item_by_id(&ukdev->uk_hdr, index);
		if (unifykey)
			n += sprintf(&buf[n], "%02d: %s, %s, %x\n",
				index, unifykey->name,
				keydev[unifykey->dev], unifykey->perm);
	}
	buf[n] = 0;

	return n;
}

static ssize_t exist_show(struct class *cla,
			  struct class_attribute *attr,
			  char *buf)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	ssize_t			n = 0;
	int			ret;
	unsigned int		keystate;
	unsigned int		keypermit;
	static const char *const state[] = {"none", "exist", "unknown"};

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;

	if (!curkey) {
		pr_err("please set key name first, %s:%d\n",
		       __func__, __LINE__);
		return -EINVAL;
	}
	/* using current key*/
	ret = key_unify_query(ukdev, curkey->name, &keystate, &keypermit);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
		       __func__, __LINE__);
		return 0;
	}

	if (keystate > 2)
		keystate = 2;

	n = sprintf(buf, "%s\n", state[keystate]);
	buf[n] = 0;

	return n;
}

static ssize_t secure_show(struct class *cla,
			   struct class_attribute *attr,
			   char *buf)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	ssize_t			n = 0;
	int			ret;
	unsigned int		secure = 0;
	static const char * const state[] = {"false", "true", "error"};

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;
	if (!curkey) {
		pr_err("please set key name first, %s:%d\n",
		       __func__, __LINE__);
		return -EINVAL;
	}

	/* using current key*/
	ret = key_unify_secure(ukdev, curkey->name, &secure);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_secure failed!\n",
		       __func__, __LINE__);
		secure = 2;
		goto _out;
	}

	if (secure > 1)
		secure = 1;
_out:
	n = sprintf(buf, "%s\n", state[secure]);
	buf[n] = 0;

	return n;
}

static ssize_t encrypt_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	ssize_t			n = 0;
	int			ret;
	unsigned int		encrypt;
	static const char * const state[] = {"false", "true", "error"};

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;
	if (!curkey) {
		pr_err("please set key name first, %s:%d\n",
		       __func__, __LINE__);
		return -EINVAL;
	}

	/* using current key*/
	ret = key_unify_encrypt(ukdev, curkey->name, &encrypt);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
		       __func__, __LINE__);
		encrypt = 2;
		goto _out;
	}

	if (encrypt > 1)
		encrypt = 1;
_out:
	n = sprintf(buf, "%s\n", state[encrypt]);
	buf[n] = 0;

	return n;
}

static ssize_t size_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	ssize_t			n = 0;
	int			ret;
	unsigned int		reallen;

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;
	if (!curkey) {
		pr_err("please set key name first, %s:%d\n",
		       __func__, __LINE__);
		return -EINVAL;
	}
	/* using current key*/
	ret = key_unify_size(ukdev, curkey->name, &reallen);
	if (ret < 0) {
		pr_err("%s:%d, key_unify_query failed!\n",
		       __func__, __LINE__);
		return 0;
	}

	n = sprintf(buf, "%d\n", reallen);
	buf[n] = 0;

	return n;
}

static ssize_t name_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	ssize_t			n = 0;

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;
	if (!curkey) {
		pr_err("please set cur key name,%s:%d\n", __func__, __LINE__);
		return 0;
	}

	n = sprintf(buf, "%s\n", curkey->name);
	buf[n] = 0;

	return n;
}

static ssize_t name_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	char			*name;
	int			index;
	int			key_cnt;
	struct key_item_t	*unifykey = NULL;
	size_t			query_name_len;
	size_t			reval;

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;

	if (count >= KEY_UNIFY_NAME_LEN)
		count = KEY_UNIFY_NAME_LEN - 1;

	if (count <= 0) {
		pr_err(" count=%zd is invalid in %s\n", count, __func__);
		return -EINVAL;
	}

	key_cnt = uk_count_key(&ukdev->uk_hdr);
	name = kzalloc(count, GFP_KERNEL);
	if (!name) {
		pr_err("can't kzalloc mem,%s:%d\n",
		       __func__, __LINE__);
		return -EINVAL;
	}
	/* check '\n' and del */
	if (buf[count - 1] == '\n')
		memcpy(name, buf, count - 1);
	else
		memcpy(name, buf, count);

	query_name_len = strlen(name);
	pr_info("%s() %d, name %s, %d\n", __func__, __LINE__,
		name, (int)query_name_len);

	curkey = NULL;
	for (index = 0; index < key_cnt; index++) {
		unifykey = uk_find_item_by_id(&ukdev->uk_hdr, index);
		if (unifykey) {
			if (!strncmp(name, unifykey->name,
				     ((strlen(unifykey->name) > query_name_len)
				     ? strlen(unifykey->name) : query_name_len))
			) {
				pr_info("%s() %d\n", __func__, __LINE__);
				curkey = unifykey;
				break;
			}
		}
	}
	reval = count;
	if (!curkey) {
		pr_err("could not found key %s\n", name);
		kfree(name);
		return -EINVAL;
	}
	ukdev->curkey = curkey;

	kfree(name);
	return reval;
}

static ssize_t read_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	ssize_t			n = 0;
	unsigned int		keysize;
	unsigned int		reallen;
	int			ret;
	unsigned char		*keydata = NULL;

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;
	if (curkey) {
		/* get key value */
		ret = key_unify_size(ukdev, curkey->name, &keysize);
		if (ret < 0) {
			pr_err("%s() %d: get key size fail\n",
			       __func__, __LINE__);
			goto _out;
		}
		if (keysize == 0) {
			pr_err("%s() %d: key %s may not burned yet!\n",
			       __func__, __LINE__, curkey->name);
			goto _out;
		}
		if (curkey->dev == KEY_SECURE)
			keysize = SHA256_SUM_LEN;
		pr_err("name: %s, size %d\n", curkey->name, keysize);
		keydata = kzalloc(keysize, GFP_KERNEL);
		if (!keydata) {
			pr_err("%s() %d: no enough memory\n",
			       __func__, __LINE__);
			goto _out;
		}
		ret = key_unify_read(ukdev, curkey->name,
				     keydata, keysize, &reallen);
		if (ret < 0) {
			pr_err("%s() %d: get key size fail\n",
			       __func__, __LINE__);
			goto _out;
		}

		memcpy(buf, keydata, keysize);
		n = keysize;
		buf[n] = 0;
	}
_out:
	if (!IS_ERR_OR_NULL(keydata))
		kfree(keydata);

	return n;
}

static ssize_t write_store(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	struct aml_uk_dev *ukdev;
	struct key_item_t	*curkey;
	int			ret;
	unsigned char		*keydata = NULL;
	size_t			key_len = 0;

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	curkey = ukdev->curkey;

	if (count <= 0) {
		pr_err(" count=%zd is invalid in %s\n", count, __func__);
		return -EINVAL;
	}

	if (curkey) {
		keydata = kzalloc(count, GFP_KERNEL);

		if (!keydata)
			goto _out;

		/* check string */
		for (key_len = 0; key_len < count; key_len++)
			if (!isascii(buf[key_len]))
				break;
		/* check '\n' and del while string */
		if (key_len == count && (buf[count - 1] == '\n')) {
			pr_err("%s()  is a string\n", __func__);
			memcpy(keydata, buf, count - 1);
			key_len = count - 1;
		} else {
			memcpy(keydata, buf, count);
			key_len = count;
		}

		ret = key_unify_write(ukdev, curkey->name, keydata, key_len);
		if (ret < 0) {
			pr_err("%s() %d: key write fail\n",
			       __func__, __LINE__);
			goto _out;
		}
	}
_out:
	kfree(keydata);
	keydata = NULL;

	return count;
}

static ssize_t attach_show(struct class *cla,
			   struct class_attribute *attr,
			   char *buf)
{
	struct aml_uk_dev *ukdev;
	ssize_t n = 0;

	/* show attach status. */
	ukdev = container_of(cla, struct aml_uk_dev, cls);
	n = sprintf(buf, "%s\n", (ukdev->init_flag == 0 ? "no" : "yes"));
	buf[n] = 0;

	return n;
}

static ssize_t attach_store(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_uk_dev *ukdev;

	ukdev = container_of(cla, struct aml_uk_dev, cls);
	key_unify_init(ukdev, NULL, KEY_UNIFY_NAME_LEN);

	return count;
}

static ssize_t lock_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct aml_uk_dev *ukdev;
	ssize_t n = 0;

	/* show lock state. */
	ukdev = container_of(cla, struct aml_uk_dev, cls);
	n = sprintf(buf, "%d\n", ukdev->lock_flag);
	buf[n] = 0;
	pr_info("%s\n", (ukdev->lock_flag == 1 ? "locked" : "unlocked"));

	return n;
}

static ssize_t lock_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	struct aml_uk_dev *ukdev;
	unsigned int state, len;

	ukdev = container_of(cla, struct aml_uk_dev, cls);

	if (count <= 0) {
		pr_err("count=%zd is invalid in %s\n", count, __func__);
		return -EINVAL;
	}

	/* check '\n' and del */
	if (buf[count - 1] == '\n')
		len = count - 1;
	else
		len = count;

	if (!strncmp(buf, "1", len)) {
		state = 1;
	} else if (!strncmp(buf, "0", len)) {
		state = 0;
	} else {
		pr_info("unifykey lock set fail\n");
		return -EINVAL;
	}

	ukdev->lock_flag = state;
	pr_info("unifykey is %s\n",
		(ukdev->lock_flag == 1 ? "locked" : "unlocked"));
	return count;
}

static const char *unifykeys_help_str = {
"Usage:\n"
"echo 1 > attach //initialise unifykeys\n"
"cat lock //get lock status\n"
"//if lock=1,you must wait, lock=0, you can go on\n"
"//so you must set unifykey lock first, then do other operations\n"
"echo 1 > lock //1:locked, 0:unlocked\n"
"echo \"key name\" > name //set current key name->\"key name\"\n"
"cat name //get current key name\n"
"echo \"key value\" > write //set current key value->\"key value\"\n"
"cat read //get current key value\n"
"cat size //get current key value\n"
"cat exist //get whether current key is exist or not\n"
"cat list //get all unifykeys\n"
"cat version //get unifykeys versions\n"
"//at last, you must set lock=0 when you has done all operations\n"
"echo 0 > lock //set unlock\n"
};

static ssize_t help_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	ssize_t n = 0;

	n = sprintf(buf, "%s", unifykeys_help_str);
	buf[n] = 0;

	return n;
}

static const struct of_device_id unifykeys_dt_match[];

static char *get_unifykeys_drv_data(struct platform_device *pdev)
{
	char *key_dev = NULL;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_node(unifykeys_dt_match,
				      pdev->dev.of_node);
		if (match)
			key_dev = (char *)match->data;
	}

	return key_dev;
}

static const struct file_operations unifykey_fops = {
	.owner      = THIS_MODULE,
	.llseek     = unifykey_llseek,
	.open       = unifykey_open,
	.release    = unifykey_release,
	.read       = unifykey_read,
	.write      = unifykey_write,
	.unlocked_ioctl  = unifykey_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= unifykey_compat_ioctl,
#endif
};

static CLASS_ATTR_RO(version);
static CLASS_ATTR_RO(list);
static CLASS_ATTR_RO(exist);
static CLASS_ATTR_RO(encrypt);
static CLASS_ATTR_RO(secure);
static CLASS_ATTR_RO(size);
static CLASS_ATTR_RO(help);
static CLASS_ATTR_RW(name);
static CLASS_ATTR_WO(write);
static CLASS_ATTR_RO(read);
static CLASS_ATTR_RW(attach);
static CLASS_ATTR_RW(lock);

static struct attribute *unifykey_class_attrs[] = {
	&class_attr_version.attr,
	&class_attr_list.attr,
	&class_attr_exist.attr,
	&class_attr_encrypt.attr,
	&class_attr_secure.attr,
	&class_attr_size.attr,
	&class_attr_help.attr,
	&class_attr_name.attr,
	&class_attr_write.attr,
	&class_attr_read.attr,
	&class_attr_attach.attr,
	&class_attr_lock.attr,
	NULL,
};
ATTRIBUTE_GROUPS(unifykey_class);

static int __init aml_unifykeys_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct aml_uk_dev *ukdev;

	if (security_key_init(pdev) != 0) {
		pr_err("fail to initialize security key\n");
		ret = -ENODEV;
		goto out;
	}

	ukdev = devm_kzalloc(&pdev->dev, sizeof(*ukdev), GFP_KERNEL);
	if (unlikely(!ukdev)) {
		pr_err("fail to alloc enough mem for ukdev\n");
		ret = -ENOMEM;
		goto out;
	}
	ukdev_global = ukdev;
	ukdev->pdev = pdev;
	platform_set_drvdata(pdev, ukdev);

	INIT_LIST_HEAD(&ukdev->uk_hdr);

	ret = uk_dt_create(pdev);
	if (ret != 0) {
		pr_err("fail to obtain necessary info from dts\n");
		goto error1;
	}

	ret = alloc_chrdev_region(&ukdev->uk_devno, 0, 1,
				  UNIFYKEYS_DEVICE_NAME);
	if (ret != 0) {
		pr_err("fail to allocate major number\n ");
		goto error1;
	}
	pr_info("unifykey_devno: %x\n", ukdev->uk_devno);

	ukdev->cls.name = UNIFYKEYS_CLASS_NAME;
	ukdev->cls.owner = THIS_MODULE;
	ukdev->cls.class_groups = unifykey_class_groups;
	ret = class_register(&ukdev->cls);
	if (ret != 0)
		goto error2;

	/* connect the file operations with cdev */
	cdev_init(&ukdev->cdev, &unifykey_fops);
	ukdev->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&ukdev->cdev, ukdev->uk_devno, 1);
	if (ret != 0) {
		pr_err("fail to add device\n");
		goto error3;
	}

	devp = device_create(&ukdev->cls, NULL,
			     ukdev->uk_devno, NULL, UNIFYKEYS_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}

	devp->platform_data = get_unifykeys_drv_data(pdev);

	pr_info("device %s created ok\n", UNIFYKEYS_DEVICE_NAME);
	unifykey_initialized = 1;
	return 0;

error4:
	cdev_del(&ukdev->cdev);
error3:
	class_unregister(&ukdev->cls);
error2:
	unregister_chrdev_region(ukdev->uk_devno, 1);
error1:
	devm_kfree(&pdev->dev, ukdev);
out:
	return ret;
}

static int aml_unifykeys_remove(struct platform_device *pdev)
{
	struct aml_uk_dev *ukdev = platform_get_drvdata(pdev);

	if (pdev->dev.of_node)
		uk_dt_release(pdev);

	unregister_chrdev_region(ukdev->uk_devno, 1);
	device_destroy(&ukdev->cls, ukdev->uk_devno);
	cdev_del(&ukdev->cdev);
	class_unregister(&ukdev->cls);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id unifykeys_dt_match[] = {
	{	.compatible = "amlogic,unifykey",
	},
	{},
};

static struct platform_driver unifykey_platform_driver = {
	.remove = aml_unifykeys_remove,
	.driver = {
		.name = UNIFYKEYS_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = unifykeys_dt_match,
	},
};

int __init aml_unifykeys_init(void)
{
	return platform_driver_probe(&unifykey_platform_driver,
				    aml_unifykeys_probe);
}

void aml_unifykeys_exit(void)
{
	platform_driver_unregister(&unifykey_platform_driver);
}
