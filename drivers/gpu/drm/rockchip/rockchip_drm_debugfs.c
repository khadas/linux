// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 * Author: Sandy Huang <hjc@rock-chips.com>
 */

#include <drm/drm_atomic_uapi.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>

#include <linux/file.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_debugfs.h"
#include "rockchip_drm_fb.h"

#define DUMP_BUF_PATH		"/data"
#define AFBC_HEADER_SIZE		16
#define AFBC_HDR_ALIGN			64
#define AFBC_SUPERBLK_PIXELS		256
#define AFBC_SUPERBLK_ALIGNMENT		128

#define to_rockchip_crtc(x) container_of(x, struct rockchip_crtc, crtc)

/**
 * struct vop_dump_info - vop dump plane info structure
 *
 * Store plane info used to write display data to /data/vop_buf/
 *
 */
struct vop_dump_info {
	/* @win_name human readable vop win name */
	const char *win_name;
	/* @fbc_enable: indicate fbc is enabled */
	bool fbc_enable;
	/* @yuv_format: indicate yuv format or not */
	bool yuv_format;
	/* @pitches: the buffer pitch size */
	u32 pitches;
	/* @height: the buffer pitch height */
	u32 height;
	/* @info: DRM format info */
	const struct drm_format_info *format;
	/* @fb: DRM frame buffer */
	struct drm_framebuffer *fb;
};

static int temp_pow(int sum, int n)
{
	int i;
	int temp = sum;

	if (n < 1)
		return 1;
	for (i = 1; i < n ; i++)
		sum *= temp;
	return sum;
}

static int get_afbc_size(uint32_t width, uint32_t height, uint32_t bpp)
{
	uint32_t h_alignment = 16;
	uint32_t n_blocks;
	uint32_t hdr_size;
	uint32_t size;

	height = ALIGN(height, h_alignment);
	n_blocks = width * height / AFBC_SUPERBLK_PIXELS;
	hdr_size = ALIGN(n_blocks * AFBC_HEADER_SIZE, AFBC_HDR_ALIGN);

	size = hdr_size + n_blocks * ALIGN(bpp * AFBC_SUPERBLK_PIXELS / 8, AFBC_SUPERBLK_ALIGNMENT);

	return size;
}

static int rockchip_drm_dump_plane_buffer(struct vop_dump_info *dump_info, int frame_count)
{
	struct iosys_map map[DRM_FORMAT_MAX_PLANES];
	struct iosys_map data[DRM_FORMAT_MAX_PLANES];
	struct drm_framebuffer *fb = dump_info->fb;
	int ret;
	int flags;
	int bpp;
	const char *ptr;
	char file_name[100];
	char format_name[5];
	int width;
	size_t size, uv_size = 0;
	void *kvaddr;
	struct file *file;
	loff_t pos = 0;

	snprintf(file_name, sizeof(file_name), "%p4cc", &dump_info->format->format);
	strscpy(format_name, file_name, 5);

	bpp = rockchip_drm_get_bpp(dump_info->format);
	if (!bpp) {
		DRM_WARN("invalid bpp %d\n", bpp);
		return 0;
	}

	if (dump_info->yuv_format) {
		u8 hsub = dump_info->format->hsub;
		u8 vsub = dump_info->format->vsub;

		width = dump_info->pitches * 8 / bpp;
		flags = O_RDWR | O_CREAT | O_APPEND;
		uv_size = (width * dump_info->height * bpp >> 3) * 2 / hsub / vsub;
		snprintf(file_name, 100, "%s/video%d_%d_%s.%s", DUMP_BUF_PATH,
			 width, dump_info->height, format_name,
			 "bin");
	} else {
		width = dump_info->pitches * 8 / bpp;
		flags = O_RDWR | O_CREAT;
		snprintf(file_name, 100, "%s/%s_%dx%d_%s%s%d.%s",
			 DUMP_BUF_PATH, dump_info->win_name, width, dump_info->height,
			 format_name, dump_info->fbc_enable ?
			 "_FBC_" : "_", frame_count, "bin");
	}

	ret = drm_gem_fb_begin_cpu_access(fb, DMA_FROM_DEVICE);
	if (ret)
		return ret;

	ret = drm_gem_fb_vmap(fb, map, data);
	if (ret) {
		DRM_ERROR("Failed to vmap() buffer\n");
		goto out_drm_gem_fb_end_cpu_access;
	}

	kvaddr = data[0].vaddr;

	if (dump_info->fbc_enable)
		size = get_afbc_size(width, dump_info->height, bpp);
	else
		size = (width * dump_info->height * bpp >> 3) + uv_size;

	ptr = file_name;
	file = filp_open(ptr, flags, 0644);
	if (!IS_ERR(file)) {
		kernel_write(file, kvaddr, size, &pos);
		DRM_INFO("dump file name is:%s\n", file_name);
		fput(file);
	} else {
		DRM_INFO("open %s failed\n", ptr);
	}

	drm_gem_fb_vunmap(fb, map);
out_drm_gem_fb_end_cpu_access:
	drm_gem_fb_end_cpu_access(fb, DMA_FROM_DEVICE);

	return 0;
}

int rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc)
{
	struct rockchip_crtc *rockchip_crtc = container_of(crtc, struct rockchip_crtc, crtc);
	struct drm_plane *plane;
	struct drm_plane_state *pstate;
	struct drm_framebuffer *fb;
	struct drm_rect *psrc;
	struct vop_dump_info dump_info;

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		pstate = plane->state;
		psrc = &pstate->src;
		fb = pstate->fb;
		if (!fb)
			continue;

		dump_info.fbc_enable = rockchip_drm_is_afbc(plane, fb->modifier) ||
			rockchip_drm_is_rfbc(plane, fb->modifier);
		dump_info.win_name = plane->name;
		dump_info.yuv_format = fb->format->is_yuv;
		dump_info.fb = fb;
		dump_info.pitches = fb->pitches[0];
		dump_info.height = drm_rect_height(psrc) >> 16;
		dump_info.format = fb->format;

		rockchip_drm_dump_plane_buffer(&dump_info, rockchip_crtc->frame_count);
	}

	return 0;
}

static int rockchip_drm_dump_buffer_show(struct seq_file *m, void *data)
{
	seq_puts(m, "  echo enable  > Enable dump feature\n");
	seq_puts(m, "  echo dump    > Immediately dump the current frame\n");
	seq_puts(m, "  echo dumpon  > dump to start vop keep dumping\n");
	seq_puts(m, "  echo dumpoff > Disable dump feature and stop keep dumping\n");
	seq_puts(m, "  echo dumpn   > dump n is the number of dump times\n");
	seq_puts(m, "  dump path is /data\n");

	return 0;
}

static int rockchip_drm_dump_buffer_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_dump_buffer_show, crtc);
}

static ssize_t
rockchip_drm_dump_buffer_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	struct seq_file *m = file->private_data;
	struct drm_crtc *crtc = m->private;
	char buf[14] = {};
	int dump_times = 0;
	int i = 0;
	struct rockchip_crtc *rockchip_crtc = to_rockchip_crtc(crtc);

	if (len > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	buf[len - 1] = '\0';
	if (strncmp(buf, "dumpon", 6) == 0) {
		rockchip_crtc->vop_dump_status = DUMP_KEEP;
		DRM_INFO("keep dumping\n");
	} else if (strncmp(buf, "dumpoff", 7) == 0) {
		rockchip_crtc->vop_dump_status = DUMP_DISABLE;
		DRM_INFO("close keep dumping\n");
	} else if (strncmp(buf, "dump", 4) == 0) {
		if (isdigit(buf[4])) {
			for (i = 4; i < strlen(buf); i++) {
				dump_times += temp_pow(10, (strlen(buf)
						       - i - 1))
						       * (buf[i] - '0');
		}
			rockchip_crtc->vop_dump_times = dump_times;
		} else {
			drm_modeset_lock_all(crtc->dev);
			rockchip_drm_crtc_dump_plane_buffer(crtc);
			rockchip_crtc->frame_count++;
			drm_modeset_unlock_all(crtc->dev);
		}
	} else if (strncmp(buf, "enable", 6) == 0) {
		rockchip_crtc->vop_dump_status = DUMP_ENABLE;
	} else {
		return -EINVAL;
	}

	return len;
}

static const struct file_operations rockchip_drm_dump_buffer_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_dump_buffer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_dump_buffer_write,
};

int rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *vop_dump_root;
	struct dentry *ent;
	struct rockchip_crtc *rockchip_crtc = to_rockchip_crtc(crtc);

	vop_dump_root = debugfs_create_dir("vop_dump", root);
	rockchip_crtc->vop_dump_status = DUMP_DISABLE;
	rockchip_crtc->vop_dump_times = 0;
	rockchip_crtc->frame_count = 0;
	ent = debugfs_create_file("dump", 0644, vop_dump_root,
				  crtc, &rockchip_drm_dump_buffer_fops);
	if (!ent) {
		DRM_ERROR("create vop_plane_dump err\n");
		debugfs_remove_recursive(vop_dump_root);
	}

	return 0;
}

static int rockchip_drm_debugfs_color_bar_show(struct seq_file *s, void *data)
{
	seq_puts(s, "  Enable horizontal color bar:\n");
	seq_puts(s, "      echo 1 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Enable vertical color bar:\n");
	seq_puts(s, "      echo 2 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");
	seq_puts(s, "  Disable color bar:\n");
	seq_puts(s, "      echo 0 > /sys/kernel/debug/dri/0/video_port0/color_bar\n");

	return 0;
}

static int rockchip_drm_debugfs_color_bar_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, rockchip_drm_debugfs_color_bar_show, crtc);
}

static ssize_t rockchip_drm_debugfs_color_bar_write(struct file *file, const char __user *ubuf,
						    size_t len, loff_t *offp)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);
	u8 mode;

	if (len != 2) {
		DRM_INFO("Unsupported color bar mode\n");
		return -EINVAL;
	}

	if (kstrtou8_from_user(ubuf, len, 0, &mode))
		return -EFAULT;

	if (priv->crtc_funcs[pipe]->crtc_set_color_bar) {
		if (priv->crtc_funcs[pipe]->crtc_set_color_bar(crtc, mode))
			return -EINVAL;
	}

	return len;
}

static const struct file_operations rockchip_drm_debugfs_color_bar_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_color_bar_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_color_bar_write,
};

int rockchip_drm_debugfs_add_color_bar(struct drm_crtc *crtc, struct dentry *root)
{
	struct dentry *ent;

	ent = debugfs_create_file("color_bar", 0644, root, crtc,
				  &rockchip_drm_debugfs_color_bar_fops);
	if (!ent)
		DRM_ERROR("Failed to add color_bar for debugfs\n");

	return 0;
}
