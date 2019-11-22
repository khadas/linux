/*
 * drivers/amlogic/drm/meson_debugfs.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

#include "meson_drv.h"
#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"

#ifdef CONFIG_DEBUG_FS

static int meson_dump_show(struct seq_file *sf, void *data)
{
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);

	seq_puts(sf, "echo 1 > dump to enable the osd dump func\n");
	seq_puts(sf, "echo 0 > dump to disable the osd dump func\n");
	seq_printf(sf, "dump_enable: %d\n", amc->dump_enable);
	seq_printf(sf, "dump_counts: %d\n", amc->dump_counts);
	return 0;
}

static int meson_dump_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, meson_dump_show, crtc);
}

static ssize_t meson_dump_write(struct file *file, const char __user *ubuf,
					size_t len, loff_t *offp)
{
	char buf[8];
	int counts = 0;
	struct seq_file *sf = file->private_data;
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	buf[len] = '\0';

	if (strncmp(buf, "0", 1) == 0) {
		amc->dump_enable = 0;
		DRM_INFO("disable the osd dump\n");
	} else {
		if (kstrtoint(buf, 0, &counts) == 0) {
			amc->dump_counts = (counts > 0) ? counts : 0;
			amc->dump_enable = (counts > 0) ? 1 : 0;
		}
	}

	return len;
}

static const struct file_operations meson_dump_fops = {
	.owner = THIS_MODULE,
	.open = meson_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = meson_dump_write,
};

static int meson_regdump_show(struct seq_file *sf, void *data)
{
	int i;
	struct meson_vpu_block *mvb;
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);
	struct meson_vpu_pipeline *mvp1 = amc->pipeline;

	for (i = 0; i < mvp1->num_blocks; i++) {
		mvb = mvp1->mvbs[i];
		if (!mvb)
			continue;

		seq_printf(sf, "*************%s*************\n", mvb->name);
		if (mvb->ops && mvb->ops->dump_register)
			mvb->ops->dump_register(mvb, sf);
	}
	return 0;
}

static int meson_regdump_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, meson_regdump_show, crtc);
}

static const struct file_operations meson_regdump_fops = {
	.owner = THIS_MODULE,
	.open = meson_regdump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int meson_imgpath_show(struct seq_file *sf, void *data)
{
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);

	seq_puts(sf, "echo /tmp/osd_path > imgpath to store the osd dump path\n");
	seq_printf(sf, "imgpath: %s\n", amc->osddump_path);
	return 0;
}

static int meson_imgpath_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, meson_imgpath_show, crtc);
}

static ssize_t meson_imgpath_write(struct file *file, const char __user *ubuf,
					size_t len, loff_t *offp)
{
	struct seq_file *sf = file->private_data;
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);

	if (len > sizeof(amc->osddump_path) - 1)
		return -EINVAL;

	if (copy_from_user(amc->osddump_path, ubuf, len))
		return -EFAULT;
	amc->osddump_path[len - 1] = '\0';

	return len;
}

static const struct file_operations meson_imgpath_fops = {
	.owner = THIS_MODULE,
	.open = meson_imgpath_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = meson_imgpath_write,
};

static int meson_blank_show(struct seq_file *sf, void *data)
{
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);

	seq_puts(sf, "echo 1 > blank to blank the osd plane\n");
	seq_puts(sf, "echo 0 > blank to unblank the osd plane\n");
	seq_printf(sf, "blank_enable: %d\n", amc->blank_enable);
	return 0;
}

static int meson_blank_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, meson_blank_show, crtc);
}

static ssize_t meson_blank_write(struct file *file, const char __user *ubuf,
					size_t len, loff_t *offp)
{
	char buf[4];
	struct seq_file *sf = file->private_data;
	struct drm_crtc *crtc = sf->private;
	struct am_meson_crtc *amc = to_am_meson_crtc(crtc);


	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	buf[len] = '\0';

	if (strncmp(buf, "1", 1) == 0) {
		amc->blank_enable = 1;
		DRM_INFO("enable the osd blank\n");
	} else if (strncmp(buf, "0", 1) == 0) {
		amc->blank_enable = 0;
		DRM_INFO("disable the osd blank\n");
	}

	return len;
}

static const struct file_operations meson_blank_fops = {
	.owner = THIS_MODULE,
	.open = meson_blank_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = meson_blank_write,
};

int meson_crtc_debugfs_init(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *meson_vpu_root;
	struct dentry *entry;

	meson_vpu_root = debugfs_create_dir("vpu", root);

	entry = debugfs_create_file("dump", 0644, meson_vpu_root, crtc,
							&meson_dump_fops);
	if (!entry) {
		DRM_ERROR("create dump node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}

	entry = debugfs_create_file("reg_dump", 0400, meson_vpu_root, crtc,
							&meson_regdump_fops);
	if (!entry) {
		DRM_ERROR("create reg_dump node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}

	entry = debugfs_create_file("imgpath", 0644, meson_vpu_root, crtc,
							&meson_imgpath_fops);
	if (!entry) {
		DRM_ERROR("create imgpath node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}

	entry = debugfs_create_file("blank", 0644, meson_vpu_root, crtc,
							&meson_blank_fops);
	if (!entry) {
		DRM_ERROR("create blank node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}

	return 0;
}

static int mm_show(struct seq_file *sf, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) sf->private;
	struct drm_device *dev = node->minor->dev;

	return drm_mm_dump_table(sf,
			&dev->vma_offset_manager->vm_addr_space_mm);
}

static struct drm_info_list meson_debugfs_list[] = {
	{"mm", mm_show, 0},
};

int meson_debugfs_init(struct drm_minor *minor)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_device *dev = minor->dev;

	ret = drm_debugfs_create_files(meson_debugfs_list,
					ARRAY_SIZE(meson_debugfs_list),
					minor->debugfs_root, minor);
	if (ret) {
		DRM_ERROR("could not install meson_debugfs_list\n");
		return ret;
	}

	drm_for_each_crtc(crtc, dev) {
		meson_crtc_debugfs_init(crtc, minor->debugfs_root);
	}
	return ret;
}

void meson_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(meson_debugfs_list,
				ARRAY_SIZE(meson_debugfs_list), minor);
}
#endif
