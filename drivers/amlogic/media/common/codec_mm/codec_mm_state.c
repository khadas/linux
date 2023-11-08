// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/amlogic/media/codec_mm/codec_mm_state.h>

#define WRITE_BUFSIZE	(4096)

struct codec_state_mgr {
	struct list_head	cs_head;
	struct mutex		cs_lock; /* Used for cs list locked. */
	struct dentry		*cs_dir;
};

static struct codec_state_mgr *get_cs_mgr(void)
{
	static struct codec_state_mgr mgr;
	static bool inited;

	if (!inited) {
		INIT_LIST_HEAD(&mgr.cs_head);
		mutex_init(&mgr.cs_lock);

		inited = true;
	}

	return &mgr;
};

void *cs_seq_start(struct seq_file *m, loff_t *pos)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	mutex_lock(&mgr->cs_lock);
	return seq_list_start(&mgr->cs_head, *pos);
}

void *cs_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	return seq_list_next(v, &mgr->cs_head, pos);
}

void cs_seq_stop(struct seq_file *m, void *v)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	mutex_unlock(&mgr->cs_lock);
}

static int cs_seq_show(struct seq_file *m, void *v)
{
	struct codec_state_node *cs = v;

	if (cs->on && cs->ops->show)
		return cs->ops->show(m, cs);

	return 0;
}

static const struct seq_operations seq_op = {
	.start	= cs_seq_start,
	.next	= cs_seq_next,
	.stop	= cs_seq_stop,
	.show	= cs_seq_show
};

static int cs_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_op);
}

static int cs_seq_list_show(struct seq_file *m, void *v)
{
	struct codec_state_node *cs = v;

	seq_printf(m, "[ %s ] %s\n", cs->on ? "on" : "off", cs->ops->name);

	return 0;
}

static const struct seq_operations seq_list_op = {
	.start	= cs_seq_start,
	.next	= cs_seq_next,
	.stop	= cs_seq_stop,
	.show	= cs_seq_list_show
};

static int cs_seq_list_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_list_op);
}

static void do_cs_switch_only(struct codec_state_node *node)
{
	struct codec_state_mgr *mgr = get_cs_mgr();
	struct codec_state_node *cs;

	list_for_each_entry(cs, &mgr->cs_head, list) {
		cs->on = (cs == node) ? true : false;
	}
}

static int do_store(int argc, char **argv)
{
	struct codec_state_mgr *mgr = get_cs_mgr();
	struct codec_state_node *cs;
	char *module = argv[0];
	char **cmd = argv + 1;
	int clen = argc - 1;
	int ret = 0;

	mutex_lock(&mgr->cs_lock);

	list_for_each_entry(cs, &mgr->cs_head, list) {
		if (strcmp(cs->ops->name, module) &&
			strcmp("all", module))
			continue;

		if (!strcmp("off", cmd[0])) {
			cs->on = false;
		} else if (!strcmp("on", cmd[0])) {
			cs->on = true;
		} else if (!strcmp("only", cmd[0])) {
			do_cs_switch_only(cs);
			break;
		} else if (cs->on) {
			if (cs->ops->store)
				ret = cs->ops->store(clen, (const char **)cmd);
		}
	}

	mutex_unlock(&mgr->cs_lock);

	return ret;
}

static int do_run_cmd(const char *buf, int (*func)(int, char **))
{
	char **argv;
	int argc = 0, ret = 0;

	argv = argv_split(GFP_KERNEL, buf, &argc);
	if (!argv)
		return -ENOMEM;

	if (argc)
		ret = func(argc, argv);

	argv_free(argv);

	return ret;
}

static ssize_t cs_parse_cmd(struct file *file, const char __user *buffer,
				size_t count, loff_t *ppos,
				int (*func)(int, char **))
{
	char *kbuf, *buf, *tmp;
	int ret = 0;
	size_t done = 0;
	size_t size;

	kbuf = kmalloc(WRITE_BUFSIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	while (done < count) {
		size = count - done;

		if (size >= WRITE_BUFSIZE)
			size = WRITE_BUFSIZE - 1;

		if (copy_from_user(kbuf, buffer + done, size)) {
			ret = -EFAULT;
			goto out;
		}

		kbuf[size] = '\0';
		buf = kbuf;

		do {
			tmp = strchr(buf, '\n');
			if (tmp) {
				*tmp = '\0';
				size = tmp - buf + 1;
			} else {
				size = strlen(buf);
				if (done + size < count) {
					if (buf != kbuf)
						break;
					/* This can accept WRITE_BUFSIZE - 2 ('\n' + '\0') */
					pr_warn("Line length is too long: Should be less than %d\n",
						WRITE_BUFSIZE - 2);
					ret = -EINVAL;
					goto out;
				}
			}
			done += size;

			/* Remove comments */
			tmp = strchr(buf, '#');

			if (tmp)
				*tmp = '\0';

			ret = do_run_cmd(buf, func);
			if (ret)
				goto out;
			buf += size;

		} while (done < count);
	}
	ret = done;

out:
	kfree(kbuf);

	return ret;
}

static ssize_t cs_seq_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	return cs_parse_cmd(file, buffer, count, ppos, do_store);
}

void cs_seq_list_read(struct seq_file *m, void *v)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	mutex_unlock(&mgr->cs_lock);
}

int codec_state_register(struct codec_state_node *cs, struct codec_state_ops *ops)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	if (!cs || !ops)
		return -EINVAL;

	mutex_lock(&mgr->cs_lock);

	INIT_LIST_HEAD(&cs->list);
	cs->ops = ops;
	cs->on = true;

	list_add_tail(&cs->list, &mgr->cs_head);

	mutex_unlock(&mgr->cs_lock);

	return 0;
}

void codec_state_unregister(struct codec_state_node *cs)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	mutex_lock(&mgr->cs_lock);

	list_del_init(&cs->list);

	mutex_unlock(&mgr->cs_lock);
}

static const struct file_operations cs_dump_fops = {
	.owner		= THIS_MODULE,
	.open		= cs_seq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= cs_seq_write
};

static const struct file_operations cs_config_fops = {
	.write		= cs_seq_write,
	.llseek		= seq_lseek
};

static const struct file_operations cs_list_fops = {
	.open		= cs_seq_list_open,
	.read		= seq_read
};

int codec_state_debugfs_init(void)
{
	struct codec_state_mgr *mgr = get_cs_mgr();
	struct dentry *d;
	int r = 0;

	d = debugfs_create_dir("codec_state", NULL);
	if (IS_ERR(d))
		return PTR_ERR(d);

	mgr->cs_dir = d;

	d = debugfs_create_file("dump_status", 0644, mgr->cs_dir,
			NULL, &cs_dump_fops);
	if (IS_ERR(d)) {
		pr_err("Create codec state dump_status node failed\n");
		r = PTR_ERR(d);
		goto err;
	}

	d = debugfs_create_file("config", 0644, mgr->cs_dir,
			NULL, &cs_config_fops);
	if (IS_ERR(d)) {
		pr_err("Create codec state config node failed\n");
		r = PTR_ERR(d);
		goto err;
	}

	d = debugfs_create_file("list", 0644, mgr->cs_dir,
			NULL, &cs_list_fops);
	if (IS_ERR(d)) {
		pr_err("Create codec state list node failed\n");
		r = PTR_ERR(d);
		goto err;
	}

	return 0;
err:
	debugfs_remove_recursive(mgr->cs_dir);
	mgr->cs_dir = NULL;

	return r;
}

void codec_state_debugfs_release(void)
{
	struct codec_state_mgr *mgr = get_cs_mgr();

	if (!mgr->cs_dir)
		return;

	debugfs_remove_recursive(mgr->cs_dir);
}

