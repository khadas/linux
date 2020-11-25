// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

#include "meson_drv.h"
#include "meson_crtc.h"
#include "meson_plane.h"
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

static int meson_osd_reverse_show(struct seq_file *sf, void *data)
{
	struct drm_plane *plane = sf->private;
	struct am_osd_plane *amp = to_am_osd_plane(plane);

	seq_puts(sf, "echo 1/2/3 > osd_reverse :reverse the osd xy/x/y\n");
	seq_puts(sf, "echo 0 > osd_reverse to unreverse the osd plane\n");
	seq_printf(sf, "osd_reverse: %d\n", amp->osd_reverse);
	return 0;
}

static int meson_osd_reverse_open(struct inode *inode, struct file *file)
{
	struct drm_plane *plane = inode->i_private;

	return single_open(file, meson_osd_reverse_show, plane);
}

static ssize_t meson_osd_reverse_write(struct file *file,
				       const char __user *ubuf,
				       size_t len, loff_t *offp)
{
	char buf[4];
	struct seq_file *sf = file->private_data;
	struct drm_plane *plane = sf->private;
	struct am_osd_plane *amp = to_am_osd_plane(plane);

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	buf[len] = '\0';

	if (strncmp(buf, "1", 1) == 0) {
		amp->osd_reverse = DRM_MODE_REFLECT_MASK;
		DRM_INFO("enable the osd reverse\n");
	} else if (strncmp(buf, "2", 1) == 0) {
		amp->osd_reverse = DRM_MODE_REFLECT_X;
		DRM_INFO("enable the osd reverse_x\n");
	} else if (strncmp(buf, "3", 1) == 0) {
		amp->osd_reverse = DRM_MODE_REFLECT_Y;
		DRM_INFO("enable the osd reverse_y\n");
	} else if (strncmp(buf, "0", 1) == 0) {
		amp->osd_reverse = 0;
		DRM_INFO("disable the osd reverse\n");
	}

	return len;
}

static const struct file_operations meson_osd_reverse_fops = {
	.owner = THIS_MODULE,
	.open = meson_osd_reverse_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = meson_osd_reverse_write,
};

static int meson_osd_blend_bypass_show(struct seq_file *sf, void *data)
{
	struct drm_plane *plane = sf->private;
	struct am_osd_plane *amp = to_am_osd_plane(plane);

	seq_puts(sf, "echo 1/0 > osd_blend_bypass :enable/disable\n");
	seq_printf(sf, "osd_blend_bypass: %d\n", amp->osd_blend_bypass);
	return 0;
}

static int meson_osd_blend_bypass_open(struct inode *inode, struct file *file)
{
	struct drm_plane *plane = inode->i_private;

	return single_open(file, meson_osd_blend_bypass_show, plane);
}

static ssize_t meson_osd_blend_bypass_write(struct file *file,
					    const char __user *ubuf,
					    size_t len, loff_t *offp)
{
	char buf[4];
	struct seq_file *sf = file->private_data;
	struct drm_plane *plane = sf->private;
	struct am_osd_plane *amp = to_am_osd_plane(plane);

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	buf[len] = '\0';

	if (strncmp(buf, "1", 1) == 0) {
		amp->osd_blend_bypass = 1;
		DRM_INFO("enable the osd blend bypass\n");
	} else if (strncmp(buf, "0", 1) == 0) {
		amp->osd_blend_bypass = 0;
		DRM_INFO("disable the osd blend bypass\n");
	}

	return len;
}

static const struct file_operations meson_osd_blend_bypass_fops = {
	.owner = THIS_MODULE,
	.open = meson_osd_blend_bypass_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = meson_osd_blend_bypass_write,
};

u32 overwrite_reg[256];
u32 overwrite_val[256];
int overwrite_enable;
int reg_num;

static int meson_reg_debug_show(struct seq_file *sf, void *data)
{
	int i;

	seq_puts(sf, "echo rv reg > debug to read the register\n");
	seq_puts(sf, "echo wv reg val > debug to overwrite the register\n");
	seq_puts(sf, "echo ow 1 > debug to enable overwrite register\n");
	seq_printf(sf, "\noverwrited status: %s\n", overwrite_enable ? "on" : "off");
	if (overwrite_enable) {
		for (i = 0; i < reg_num; i++)
			seq_printf(sf, "reg[0x%04x]=0x%08x\n", overwrite_reg[i],
				   overwrite_val[i]);
	}
	//seq_printf(sf, "blank_enable: %d\n", amc->blank_enable);
	return 0;
}

static int meson_reg_debug_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, meson_reg_debug_show, crtc);
}

static void parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t meson_reg_debug_write(struct file *file, const char __user *ubuf,
					size_t len, loff_t *offp)
{
	char buf[64];
	long val;
	int i;
	unsigned int reg_addr, reg_val;
	char *bufp, *parm[8] = {NULL};

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	buf[len] = '\0';

	bufp = buf;
	parse_param(bufp, (char **)&parm);
	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;

		reg_addr = val;
		DRM_INFO("reg[0x%04x]=0x%08x\n", reg_addr, meson_drm_read_reg(reg_addr));
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		reg_addr = val;

		if (kstrtoul(parm[2], 16, &val) < 0)
			return -EINVAL;

		reg_val = val;
		for (i = 0; i < reg_num; i++) {
			if (overwrite_reg[i] == reg_addr) {
				overwrite_val[i] = reg_val;
				return len;
			}
		}

		if (i == reg_num) {
			overwrite_reg[i] = reg_addr;
			overwrite_val[i] = reg_val;
			reg_num++;
		}
	} else if (!strcmp(parm[0], "ow")) {
		if (parm[1] && !strcmp(parm[1], "1"))
			overwrite_enable = 1;
		else if (parm[1] && !strcmp(parm[1], "0"))
			overwrite_enable = 0;
	}

	return len;
}

static const struct file_operations meson_reg_debug_fops = {
	.owner = THIS_MODULE,
	.open = meson_reg_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = meson_reg_debug_write,
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

	entry = debugfs_create_file("debug", 0644, meson_vpu_root, crtc,
					&meson_reg_debug_fops);
	if (!entry) {
		DRM_ERROR("create reg_debug node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}

	return 0;
}

int meson_plane_debugfs_init(struct drm_plane *plane, struct dentry *root)
{
	struct dentry *meson_vpu_root;
	struct dentry *entry;

	meson_vpu_root = debugfs_create_dir(plane->name, root);

	entry = debugfs_create_file("osd_reverse", 0644, meson_vpu_root, plane,
				    &meson_osd_reverse_fops);
	if (!entry) {
		DRM_ERROR("create osd_reverse node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}
	entry = debugfs_create_file("osd_blend_bypass", 0644,
				    meson_vpu_root, plane,
				    &meson_osd_blend_bypass_fops);
	if (!entry) {
		DRM_ERROR("create osd_blend_bypass node error\n");
		debugfs_remove_recursive(meson_vpu_root);
	}

	return 0;
}

static int mm_show(struct seq_file *sf, void *arg)
{
	return 0;
}

static struct drm_info_list meson_debugfs_list[] = {
	{"mm", mm_show, 0},
};

int meson_debugfs_init(struct drm_minor *minor)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_plane *plane;
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
	drm_for_each_plane(plane, dev) {
		meson_plane_debugfs_init(plane, minor->debugfs_root);
	}
	return ret;
}

#endif
