// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/dma-direction.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>

/* Amlogic Headers */
#include <linux/meson_ion.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/amvecm/amvecm.h>

/* Local Headers */
#include "video_priv.h"
#include "dummy_provider.h"

static struct video_provider_device_s video_provider_device;
static struct vframe_provider_s vp_vfm_prov;
static struct vp_pool_s vp_pool[VP_VFM_POOL_SIZE];
static s32 fill_ptr, get_ptr, put_ptr;
static int vp_canvas_table[VP_VFM_POOL_SIZE];
static DEFINE_SPINLOCK(lock);
unsigned int dummy_video_log_level;

#define INCPTR(p) ptr_wrap_inc(&(p))

static inline void ptr_wrap_inc(u32 *ptr)
{
	u32 i = *ptr;

	i++;
	if (i >= VP_VFM_POOL_SIZE)
		i = 0;
	*ptr = i;
}

static inline u32 index2canvas(u32 index)
{
	return vp_canvas_table[index];
}

static int has_unused_pool(void)
{
	int i;

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		if (vp_pool[i].used == 0)
			return i;
	}
	return -1;
}

static ssize_t log_level_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", dummy_video_log_level);
}

static ssize_t log_level_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	if (ret) {
		vp_err("kstrtoint err\n");
		return -EINVAL;
	}

	vp_info("log_level: %d->%d\n", dummy_video_log_level, res);
	dummy_video_log_level = res;

	return count;
}

static CLASS_ATTR_RW(log_level);

static struct attribute *video_provider_class_attrs[] = {
	&class_attr_log_level.attr,
	NULL
};
ATTRIBUTE_GROUPS(video_provider_class);

static struct class video_provider_class = {
	.name = VIDEO_PROVIDER_NAME,
	.class_groups = video_provider_class_groups,
};

static struct vframe_s *vp_vfm_peek(void *op_arg)
{
	vp_dbg2("%s (%d).\n", __func__, get_ptr);
	if (get_ptr == fill_ptr)
		return NULL;
	return &vp_pool[get_ptr].vfm;
}

static struct vframe_s *vp_vfm_get(void *op_arg)
{
	struct vframe_s *vf;

	vp_dbg2("%s (%d).\n", __func__, get_ptr);

	if (get_ptr == fill_ptr)
		return NULL;
	vf = &vp_pool[get_ptr].vfm;
	INCPTR(get_ptr);

	return vf;
}

static void vp_vfm_put(struct vframe_s *vf, void *op_arg)
{
	int i;
	int canvas_addr;

	vp_dbg2("%s.\n", __func__);

	if (!vf)
		return;
	INCPTR(put_ptr);

	if (put_ptr == fill_ptr) {
		vp_info("buffer%d is being in use, skip\n", fill_ptr);
		return;
	}

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		canvas_addr = index2canvas(i);
		if (vf->canvas0Addr == (canvas_addr & 0xff)) {
			vp_pool[i].used = 0;
			vp_dbg("******recycle buffer index : %d ******\n", i);
		}
	}
}

static int vp_event_cb(int type, void *data, void *private_data)
{
	vp_dbg2("%s type(0x%x).\n", __func__, type);

	return 0;
}

static int vp_vfm_states(struct vframe_states *states, void *op_arg)
{
	int i;
	unsigned long flags;

	vp_dbg2("%s.\n", __func__);

	if (!states) {
		vp_err("vframe_states is NULL");
		return -EINVAL;
	}

	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VP_VFM_POOL_SIZE;
	i = fill_ptr - get_ptr;
	if (i < 0)
		i += VP_VFM_POOL_SIZE;
	states->buf_avail_num = i;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

static const struct vframe_operations_s vp_vfm_ops = {
	.peek = vp_vfm_peek,
	.get = vp_vfm_get,
	.put = vp_vfm_put,
	.event_cb = vp_event_cb,
	.vf_states = vp_vfm_states,
};

static void video_provider_release_path(void)
{
	vf_unreg_provider(&vp_vfm_prov);
	vfm_map_remove(VP_VFPATH_ID);
}

static int video_provider_creat_path(void)
{
	int ret = -1;
	char path_id[] = VP_VFPATH_ID;
	char path_chain[] = VP_VFPATH_CHAIN;

	if (vfm_map_add(path_id, path_chain) < 0) {
		vp_err("video_provider map creation failed\n");
		return -ENOMEM;
	}

	vf_provider_init(&vp_vfm_prov, VIDEO_PROVIDER_NAME,
		&vp_vfm_ops, NULL);
	ret = vf_reg_provider(&vp_vfm_prov);
	if (ret < 0)
		vp_info("vfm path is already created\n");
	ret = vf_notify_receiver(VIDEO_PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_START, NULL);
	if (ret < 0) {
		vp_err("notify receiver error\n");
		video_provider_release_path();
	}

	return ret;
}

static int canvas_table_alloc(void)
{
	int i;

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		if (vp_canvas_table[i])
			break;
	}

	/* alloc 2 * VP_VFM_POOL_SIZE for multi planes */
	if (i == VP_VFM_POOL_SIZE) {
		u32 canvas_table[VP_VFM_POOL_SIZE * 2];

		if (canvas_pool_alloc_canvas_table("video_provider",
						canvas_table,
						VP_VFM_POOL_SIZE * 2,
						CANVAS_MAP_TYPE_1)) {
			pr_err("%s allocate canvas error.\n", __func__);
			return -ENOMEM;
		}
		for (i = 0; i < VP_VFM_POOL_SIZE; i++)
			vp_canvas_table[i] = (canvas_table[2 * i] |
						(canvas_table[2 * i + 1] << 8));
	} else {
		vp_info("canvas_table is already alloced");
	}

	return 0;
}

static void canvas_table_release(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vp_canvas_table); i++) {
		if (vp_canvas_table[i]) {
			if (vp_canvas_table[i] & 0xff)
				canvas_pool_map_free_canvas
					(vp_canvas_table[i] & 0xff);
			if ((vp_canvas_table[i] >> 8) & 0xff)
				canvas_pool_map_free_canvas
					((vp_canvas_table[i] >> 8) & 0xff);
			if ((vp_canvas_table[i] >> 16) & 0xff)
				canvas_pool_map_free_canvas
					((vp_canvas_table[i] >> 16) & 0xff);
		}
		vp_canvas_table[i] = 0;
	}
}

static int video_provider_open(struct inode *inode, struct file *file)
{
	int ret = -1;

	_video_set_disable(VIDEO_DISABLE_FORNEXT);

	ret = video_provider_creat_path();
	if (ret < 0) {
		vp_err("video_provider_creat_path failed\n");
		return -ENOMEM;
	}

	ret = canvas_table_alloc();
	if (ret < 0)
		return ret;

	fill_ptr = 0;
	get_ptr = 0;
	put_ptr = 0;

	return 0;
}

static int vp_dma_buf_get_phys(int fd, unsigned long *addr)
{
	long ret = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	struct device *dev = video_provider_device.dev;
	enum dma_data_direction dir = DMA_TO_DEVICE;
	struct page *page;

	if (fd < 0 || !dev) {
		vp_err("error input param or dev is null\n");
		return -EINVAL;
	}

	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		vp_err("failed to get dma buffer\n");
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (IS_ERR(d_att)) {
		vp_err("failed to set dma attach\n");
		ret = -EINVAL;
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (IS_ERR(sg)) {
		vp_err("failed to get dma sg\n");
		ret = -EINVAL;
		goto map_attach_err;
	} else {
		page = sg_page(sg->sgl);
		*addr = PFN_PHYS(page_to_pfn(page));
		ret = 0;
	}

	dma_buf_unmap_attachment(d_att, sg, dir);

map_attach_err:
	dma_buf_detach(dbuf, d_att);

attach_err:
	dma_buf_put(dbuf);

	return ret;
}

static int get_fram_phyaddr(struct vp_frame_s *frame_info, unsigned long *addr)
{
	int ret = -1;
	size_t len = 0;

	if (!frame_info || !addr) {
		vp_err("%s-%d frame_info is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (frame_info->mem_type) {
	case VP_MEM_ION:
		ret = meson_ion_share_fd_to_phys(frame_info->shared_fd,
						 (phys_addr_t *)addr, &len);
		if (ret != 0)
			return ret;
		vp_dbg("ion frame addr 0x%lx, len %zu\n", *addr, len);
		break;
	case VP_MEM_DMABUF:
		ret = vp_dma_buf_get_phys(frame_info->shared_fd, addr);
		if (ret != 0)
			return ret;
		vp_dbg("dma frame addr 0x%lx, len %zu\n", *addr, len);
		break;
	default:
		vp_info("%s-%d mem type error\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int set_vfm_type(struct vp_frame_s *frame_info,
				struct vframe_s *vf, int *bpp)
{
	if (!frame_info || !vf || !bpp) {
		vp_info("%s-%d vf error\n", __func__, __LINE__);
		return -EINVAL;
	}
	switch (frame_info->format) {
	case VP_FMT_NV21:
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
		*bpp = 8;
		break;
	case VP_FMT_NV12:
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV12;
		*bpp = 8;
		break;
	case VP_FMT_RGB888:
		*bpp = 24;
		vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE |
				VIDTYPE_VIU_FIELD;
		break;
	case VP_FMT_YUV444_PACKED:
		vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE |
				VIDTYPE_VIU_FIELD;
		*bpp = 24;
		break;
	default:
		vp_info("%s-%d vf error\n", __func__, __LINE__);
		return -EINVAL;
	}
	switch (frame_info->endian) {
	case VP_BIG_ENDIAN:
		vf->flag &= ~VFRAME_FLAG_VIDEO_LINEAR;
		break;
	case VP_LITTLE_ENDIAN:
		vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		break;
	default:
		vp_info("%s-%d vf error\n", __func__, __LINE__);
		return -EINVAL;
	}

	video_provider_device.vinfo = get_current_vinfo();

	/* indicate the vframe is a limited range frame */
	vf->signal_type =
		  (1 << 29)  /* video available */
		| (5 << 26)  /* unspecified */
		| (0 << 25)  /* limited */
		| (1 << 24); /* color available */
	if (video_provider_device.vinfo->width >= 1280 &&
		video_provider_device.vinfo->height >= 720) {
		/* >= 720p, use 709 */
		vf->signal_type |=
			(1 << 16) /* bt709 */
			| (1 << 8)  /* bt709 */
			| (1 << 0); /* bt709 */
	} else {
		/* < 720p, use 709 */
		vf->signal_type |=
			(3 << 16) /* bt601 */
			| (3 << 8)  /* bt601 */
			| (3 << 0); /* bt601 */
	}

	return 0;
}

static int set_vfm_info_from_frame(struct vp_frame_s *frame_info)
{
	int ret = -1;
	int index;
	unsigned long addr = 0;
	struct vframe_s *new_vf;
	int bpp;
	unsigned int canvas_width;

	index = has_unused_pool();
	if (index < 0) {
		vp_info("no buffer available, need post ASAP\n");
		return -ENOMEM;
	}
	memset(&vp_pool[fill_ptr], 0, sizeof(struct vp_pool_s));

	ret = get_fram_phyaddr(frame_info, &addr);
	if (ret < 0)
		return ret;

	new_vf = &vp_pool[fill_ptr].vfm;
	ret = set_vfm_type(frame_info, new_vf, &bpp);
	if (ret < 0)
		return ret;
	vp_dbg("vf type val=0x%x\n", new_vf->type);

	/* 256bit align, then calc bytes*/
	canvas_width = ((frame_info->width * bpp + 0xff) & ~0xff) / 8;
	vp_dbg("canvas_width val=%u\n", canvas_width);

	switch (frame_info->format) {
	case VP_FMT_NV21:
	case VP_FMT_NV12:
		canvas_config(vp_canvas_table[fill_ptr] & 0xff,
				addr,
				canvas_width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		addr += canvas_width * frame_info->height;
		canvas_config((vp_canvas_table[fill_ptr] >> 8) & 0xff,
				addr,
				canvas_width, frame_info->height / 2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		new_vf->canvas0Addr = vp_canvas_table[fill_ptr];
		break;
	case VP_FMT_RGB888:
	case VP_FMT_YUV444_PACKED:
		canvas_config(vp_canvas_table[fill_ptr] & 0xff,
				addr,
				canvas_width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		new_vf->canvas0Addr = vp_canvas_table[fill_ptr] & 0xff;
		break;
	default:
		vp_err("unsupported format to canvas_config\n");
		return -EINVAL;
	}
	new_vf->width  = frame_info->width;
	new_vf->height = frame_info->height;
	new_vf->index = fill_ptr;
	new_vf->duration_pulldown = 0;
	new_vf->pts = 0;
	new_vf->pts_us64 = 0;
	new_vf->ratio_control = 0;

	INCPTR(fill_ptr);

	return 0;
}

static void post_frame(void)
{
	vf_notify_receiver(VIDEO_PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
}

static long video_provider_ioctl(struct file *filp, unsigned int cmd,
						 unsigned long args)
{
	long ret = 0;
	void __user *argp = (void __user *)args;
	struct vp_frame_s frame_info;

	switch (cmd) {
	case VIDEO_PROVIDER_IOCTL_RENDER:
		if (!copy_from_user(&frame_info, argp, sizeof(frame_info))) {
			vp_dbg("render: canvas index 0x%x\n",
			       vp_canvas_table[fill_ptr]);
			vp_dbg("   frame format: %d\n", frame_info.format);
			vp_dbg("    frame width: %d\n", frame_info.width);
			vp_dbg("   frame height: %d\n", frame_info.height);
			vp_dbg(" frame mem_type: %d\n", frame_info.mem_type);
			vp_dbg("frame shared_fd: %d\n", frame_info.shared_fd);
			vp_dbg("   frame endian: %d\n", frame_info.endian);
			ret = set_vfm_info_from_frame(&frame_info);
		} else {
			ret = -EINVAL;
		}
		break;
	case VIDEO_PROVIDER_IOCTL_POST:
		vp_dbg("post: canvas index 0x%x\n", vp_canvas_table[get_ptr]);
		post_frame();
		break;
	default:
		vp_err("%s-%d, para err\n", __func__, __LINE__);
		ret = -EINVAL;
	}
	return ret;
}

static int video_provider_release(struct inode *inode, struct file *file)
{
	video_provider_release_path();
	canvas_table_release();

	return 0;
}

#ifdef CONFIG_COMPAT
static long video_provider_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long args)
{
	long ret = 0;

	ret = video_provider_ioctl(filp, cmd, (ulong)compat_ptr(args));
	return ret;
}
#endif

static const struct file_operations video_provider_fops = {
	.owner = THIS_MODULE,
	.open = video_provider_open,
	.unlocked_ioctl = video_provider_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = video_provider_compat_ioctl,
#endif
	.release = video_provider_release,
};

static int video_provider_probe(struct platform_device *pdev)
{
	int  ret = 0;

	strcpy(video_provider_device.name, VIDEO_PROVIDER_NAME);
	ret = register_chrdev(0, video_provider_device.name,
				&video_provider_fops);
	if (ret <= 0) {
		vp_err("register video provider device error\n");
		return  ret;
	}
	video_provider_device.major = ret;
	vp_info("video provider major:%d\n", ret);
	ret = class_register(&video_provider_class);
	if (ret < 0) {
		vp_err("error create video provider class\n");
		return ret;
	}
	video_provider_device.cla = &video_provider_class;
	video_provider_device.dev = device_create(video_provider_device.cla,
					NULL, MKDEV(video_provider_device.major,
					0), NULL, video_provider_device.name);
	if (IS_ERR(video_provider_device.dev)) {
		vp_err("create video provider device error\n");
		class_unregister(video_provider_device.cla);
		return -1;
	}

	return ret;
}

static int video_provider_remove(struct platform_device *pdev)
{
	vp_info("%s\n", __func__);
	if (!video_provider_device.cla)
		return 0;

	if (video_provider_device.dev)
		device_destroy(video_provider_device.cla,
				MKDEV(video_provider_device.major, 0));
	class_unregister(video_provider_device.cla);
	unregister_chrdev(video_provider_device.major,
				video_provider_device.name);

	return 0;
}

static const struct of_device_id amlogic_video_provider_dt_match[] = {
	{
		.compatible = "amlogic, dummy_video_provider",
	},
	{},
};

static struct platform_driver video_provider_drv = {
	.probe = video_provider_probe,
	.remove = video_provider_remove,
	.driver = {
		.name = "video_provider",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_video_provider_dt_match,
	}
};

static int __init video_provider_init_module(void)
{
	vp_info("%s\n", __func__);

	if (platform_driver_register(&video_provider_drv)) {
		pr_err("Failed to register video provider driver error\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit video_provider_remove_module(void)
{
	platform_driver_unregister(&video_provider_drv);
	vp_info("video provider module removed.\n");
}

module_init(video_provider_init_module);
module_exit(video_provider_remove_module);

MODULE_DESCRIPTION("Amlogic dummy video provider driver");
MODULE_LICENSE("GPL");
