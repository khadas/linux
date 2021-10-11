// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/clk-provider.h>
#include <linux/uaccess.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/kallsyms.h>
#include <linux/arm-smccc.h>

#undef pr_fmt
#define pr_fmt(fmt) "clk-debug: " fmt
static struct clk *debug_clk;

#ifdef MODULE
struct clk *__clk_lookup(const char *name)
{
	static struct clk* (*func)(const char *name);

	if (!func)
		func = (void *)kallsyms_lookup_name(__func__);

	if (!func) {
		pr_err("can't find symbol: %s\n", __func__);
		return NULL;
	}

	return func(name);
}
#endif

static ssize_t parent_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *ppos)
{
	int ret;
	char clk_name[24];
	char input[24];
	struct clk *clk;

	if (count >= sizeof(input))
		return -EINVAL;

	if (copy_from_user(input, buffer, count))
		return -EFAULT;

	input[count] = '\0';

	ret = sscanf(input, "%s", clk_name);
	if (ret != 1) {
		pr_err("wrong usage, try: echo clk_name  > clk\n");
		return -EINVAL;
	}

	clk = __clk_lookup(clk_name);
	if (!clk)
		pr_err("Can't find the clock, have a look in /sys/kernel/debug/clk\n");

	if (debug_clk) {
		ret = clk_set_parent(debug_clk, clk);
		if (ret < 0)
			pr_err("failed to set parent for %s\n",
			       __clk_get_name(debug_clk));
		else
			pr_info("now %s parent is %s\n",
				__clk_get_name(debug_clk),
				__clk_get_name(clk_get_parent(debug_clk)));
	} else {
		pr_err("try: echo clk_name > clk,echo clk_name > parent\n");
	}

	return count;
}

static ssize_t measure_write(struct file *file, const char __user *buffer,
			     size_t count, loff_t *ppos)
{
	int ret;
	char input[5];
	unsigned int id = 0, rate;

	count = min_t(size_t, count, (sizeof(input) - 1));
	if (copy_from_user(input, buffer, count))
		return -EFAULT;

	input[count] = '\0';

	ret = kstrtouint(input, 0, &id);
	if (ret)
		return -EINVAL;

	rate = meson_clk_measure(id);
	pr_info("Measure the index %u clock,its rate = %u\n", id, rate);

	return count;
}

static ssize_t enable_read(struct file *file, char __user *buffer,
			   size_t count, loff_t *ppos)
{
	char buf[4];
	unsigned int len;

	len = sprintf(buf, "%d\n", __clk_is_enabled(debug_clk) ? 1 : 0);

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t enable_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *ppos)
{
	int ret;
	char *input;
	unsigned int enable = 0;

	input = kzalloc(count, GFP_KERNEL);
	if (!input) {
		kfree(input);
		return -ENOMEM;
	}
	if (copy_from_user(input, buffer, count)) {
		kfree(input);
		return -EFAULT;
	}

	ret = kstrtouint(input, 0, &enable);
	if (ret) {
		kfree(input);
		return -EINVAL;
	}

	if (enable != 0)
		clk_prepare_enable(debug_clk);
	else
		clk_disable_unprepare(debug_clk);

	kfree(input);
	return count;
}

static ssize_t rate_read(struct file *file, char __user *buffer,
			 size_t count, loff_t *ppos)
{
	/*
	 * the rate unit is HZ, need 16 char
	 */
	char buf[16];
	unsigned int len;

	len = sprintf(buf, "%lu\n", clk_get_rate(debug_clk));

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t rate_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *ppos)
{
	int ret;
	char *input;
	unsigned long rate = 0;

	input = kzalloc(count, GFP_KERNEL);
	if (!input) {
		kfree(input);
		return -ENOMEM;
	}

	if (copy_from_user(input, buffer, count)) {
		kfree(input);
		return -EFAULT;
	}
	ret = kstrtoul(input, 0, &rate);
	if (ret) {
		kfree(input);
		return -EINVAL;
	}
	pr_info("input rate = %lu, old rate = %lu\n", rate,
		clk_get_rate(debug_clk));

	ret = clk_set_rate(debug_clk, rate);
	if (ret) {
		kfree(input);
		pr_err("failed to set rate,ret =%d\n", ret);
		return ret;
	}

	pr_info("current rate = %lu\n", clk_get_rate(debug_clk));

	kfree(input);

	return count;
}

static ssize_t clk_read(struct file *file, char __user *buffer,
			size_t count, loff_t *ppos)
{
	char buf[48];
	unsigned int len;

	len = sprintf(buf, "get %s clock, its rate = %lu\n",
		      __clk_get_name(debug_clk),
		      clk_get_rate(debug_clk));

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t clk_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *ppos)
{
	int ret;
	char clk_name[24];
	char input[24];
	struct clk *clk;

	if (count >= sizeof(input))
		return -EINVAL;

	if (copy_from_user(input, buffer, count))
		return -EFAULT;

	input[count] = '\0';

	ret = sscanf(input, "%s", clk_name);
	if (ret != 1) {
		pr_err("wrong usage, try: echo clk_name  > clk\n");
		return -EINVAL;
	}

	clk = __clk_lookup(clk_name);
	if (clk)
		pr_info("success get %s clock, its rate = %lu, its parent is %s\n",
			clk_name, clk_get_rate(clk),
			__clk_get_name(clk_get_parent(clk)));
	else
		pr_err("Can't find the clock, have a look in /sys/kernel/debug/clk.\n");

	/* store the clk pointer */
	debug_clk = clk;

	return count;
}

static ssize_t secure_reg_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *ppos)
{
	int ret;
	char *input;
	unsigned long reg = 0;
	struct arm_smccc_res res;

	input = kzalloc(count, GFP_KERNEL);
	if (!input) {
		kfree(input);
		return -ENOMEM;
	}

	if (copy_from_user(input, buffer, count)) {
		kfree(input);
		return -EFAULT;
	}
	ret = kstrtoul(input, 0, &reg);
	if (ret) {
		kfree(input);
		return -EINVAL;
	}

	arm_smccc_smc(0x82000099, 0x30,
		reg, 0, 0, 0, 0, 0, &res);

	kfree(input);

	return count;
}

static const struct file_operations parent_file_ops = {
	.open		= simple_open,
	.write		= parent_write,
};

static const struct file_operations measure_file_ops = {
	.open		= simple_open,
	.write		= measure_write,
};

static const struct file_operations enable_file_ops = {
	.open		= simple_open,
	.read		= enable_read,
	.write		= enable_write,
};

static const struct file_operations rate_file_ops = {
	.open		= simple_open,
	.read		= rate_read,
	.write		= rate_write,
};

static const struct file_operations clk_file_ops = {
	.open		= simple_open,
	.read		= clk_read,
	.write		= clk_write,
};

static const struct file_operations secure_file_ops = {
	.open		= simple_open,
	.write		= secure_reg_write,
};

static int __init clk_debug_init(void)
{
	struct dentry *root;

	root = debugfs_create_dir("clk_debug", NULL);
	debugfs_create_file("clk", 0600, root, NULL, &clk_file_ops);
	debugfs_create_file("rate", 0600, root, NULL, &rate_file_ops);
	debugfs_create_file("enable", 0600, root, NULL, &enable_file_ops);
	debugfs_create_file("parent", 0200, root, NULL, &parent_file_ops);
	debugfs_create_file("measure", 0200, root, NULL, &measure_file_ops);
	debugfs_create_file("secure_reg", 0200, root, NULL, &secure_file_ops);

	return 0;
}

late_initcall_sync(clk_debug_init);

MODULE_DESCRIPTION("Amlogic meson clock debug driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("jian hu <jian.hu@amlogic.com>");
