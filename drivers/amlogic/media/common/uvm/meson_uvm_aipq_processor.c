// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <dev_ion.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include <linux/amlogic/media/video_processor/video_pp_common.h>

#include "meson_uvm_aipq_processor.h"
struct dma_buf *dmabuf_last;

static int uvm_aipq_debug;
module_param(uvm_aipq_debug, int, 0644);

static int uvm_open_aipq;
module_param(uvm_open_aipq, int, 0644);

static int uvm_aipq_dump;
module_param(uvm_aipq_dump, int, 0644);

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_NN_DUMP		0X0002

int aipq_print(int debug_flag, const char *fmt, ...)
{
	if ((uvm_aipq_debug & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "uvm_aipq:[%d]", 0);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

int aipq_vf_set_value(struct uvm_aipq_info *aipq_info, bool enable_aipq)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct vframe_s *vf = NULL;
	struct file_private_data *file_private_data = NULL;
	int shared_fd = aipq_info->shared_fd;
	int i = 0;

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		aipq_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aipq_print(PRINT_ERROR,
			"%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			aipq_print(PRINT_ERROR, "get_fh err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data)
			aipq_print(PRINT_ERROR, "invalid fd no uvm/v4lvideo\n");
		else
			vf = &file_private_data->vf;
	}
	if (vf) {
		for (i = 0; i < AI_PQ_TOP; i++)
			vf->nn_value[i] = aipq_info->nn_value[i];

		vf->ai_pq_enable = enable_aipq;
		if (vf->vf_ext) {
			vf = vf->vf_ext;
			for (i = 0; i < AI_PQ_TOP; i++)
				vf->nn_value[i] = aipq_info->nn_value[i];

			vf->ai_pq_enable = true;
		}
		vf->ai_pq_enable = enable_aipq;
	} else {
		aipq_print(PRINT_ERROR, "not find vf\n");
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	dma_buf_put(dmabuf);
	return 0;
}

struct vframe_s *aipq_get_dw_vf(struct uvm_aipq_info *aipq_info)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct vframe_s *vf = NULL;
	struct vframe_s *vf_tmp = NULL;
	struct file_private_data *file_private_data = NULL;
	int shared_fd = aipq_info->shared_fd;

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		aipq_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return NULL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aipq_print(PRINT_ERROR,
			"%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return NULL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		if (vf)
			aipq_print(PRINT_OTHER,
				"vf: %d*%d,flag=%x,type=%x\n",
				vf->width,
				vf->height,
				vf->flag,
				vf->type);

		if (vf && vf->vf_ext &&
			(vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vf_tmp = vf->vf_ext;
			aipq_print(PRINT_OTHER,
				"vf_ext: %d*%d,flag=%x,type=%x\n",
				vf_tmp->width,
				vf_tmp->height,
				vf_tmp->flag,
				vf_tmp->type);
			if (vf->width == 0) {/*for interlace*/
				vf = vf->vf_ext;
				aipq_print(PRINT_OTHER, "use ext vf\n");
			}
		}
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			aipq_print(PRINT_ERROR, "get_fh err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return NULL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data) {
			aipq_print(PRINT_ERROR, "invalid fd no uvm/v4lvideo\n");
		} else {
			if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR)
				vf = &file_private_data->vf_ext;
			else
				vf = &file_private_data->vf;
		}
	}
	if (!vf) {
		aipq_print(PRINT_ERROR, "not find vf\n");
		dma_buf_put(dmabuf);
		return NULL;
	}
	dma_buf_put(dmabuf);
	return vf;
}

static int aipq_canvas[4] = {-1, -1, -1, -1};
struct ge2d_context_s *context;

static int get_canvas(u32 index)
{
	const char *owner = "aipq";

	if (aipq_canvas[index] < 0)
		aipq_canvas[index] = canvas_pool_map_alloc_canvas(owner);

	if (aipq_canvas[index] < 0)
		aipq_print(PRINT_ERROR, "no canvas\n");
	return aipq_canvas[index];
}

int ge2d_224_process(struct vframe_s *vf, struct ge2d_output_t *output)
{
	struct config_para_ex_s ge2d_config_s;
	struct config_para_ex_s *ge2d_config = &ge2d_config_s;
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode;
	int input_width, input_height;
	u32 output_canvas = get_canvas(3);

	if (!context) {
		context = create_ge2d_work_queue();
		if (IS_ERR_OR_NULL(context)) {
			aipq_print(PRINT_ERROR, "creat ge2d work failed\n");
			return -1;
		}
	}

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));

	input_width = vf->width;
	input_height = vf->height;
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(get_canvas(0),
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(get_canvas(1),
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(get_canvas(1),
					&vf->canvas0_config[1]);
			canvas_config_config(get_canvas(2),
					&vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			get_canvas(0)
			| (get_canvas(1) << 8)
			| (get_canvas(2) << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	aipq_print(PRINT_OTHER, "src width: %d, height: %d\n",
		input_width, input_height);
	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM ||
	    interlace_mode == VIDTYPE_INTERLACE_TOP)
		input_height >>= 1;
	ge2d_config->src_para.format = get_ge2d_input_format(vf);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	aipq_print(PRINT_OTHER, "src width: %d, height: %d format =%x\n",
		input_width, input_height, ge2d_config->src_para.format);

	canvas_config(output_canvas, output->addr, output->width * 3,
		      output->height, CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = input_width;
	ge2d_config->src_para.height = input_height;
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = output->format | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;
	ge2d_config->dst_xy_swap = 0;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		aipq_print(PRINT_ERROR,
			      "++ge2d configing error.\n");
		return -1;
	}

	stretchblt_noalpha(context, 0, 0, input_width, input_height,
			   0, 0, output->width, output->height);

	return 0;
}

void free_aipq_data(void *arg)
{
	if (arg) {
		aipq_print(PRINT_OTHER, "%s\n", __func__);
		kfree((u8 *)arg);
	} else {
		aipq_print(PRINT_ERROR, "%s NULL\n", __func__);
	}
}

int attach_aipq_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *info)
{
	struct vf_aipq_t *nn_aipq = NULL;
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	struct uvm_handle *handle;
	bool attached = false;
	struct uvm_aipq_info *aipq_info = (struct uvm_aipq_info *)buf;
	struct vframe_s *vf = NULL;
	int ret = 0;
	bool enable_aipq = true;

	aipq_info->need_do_aipq = 1;

	if (!uvm_open_aipq) {
		aipq_info->need_do_aipq = 0;
		enable_aipq = false;
	} else {
		vf = aipq_get_dw_vf(aipq_info);
		if (IS_ERR_OR_NULL(vf)) {
			aipq_print(PRINT_ERROR, "get no vf\n");
			return -EINVAL;
		}
		if (vf->width > 3840 || vf->height > 2160) {
			aipq_print(PRINT_OTHER, "bypass %d %d\n",
				vf->width, vf->height);
			aipq_info->need_do_aipq = 0;
			enable_aipq = false;
		}
	}

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		aipq_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aipq_print(PRINT_ERROR,
			"attach:dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	handle = dmabuf->priv;
	uhmod = uvm_get_hook_mod(dmabuf, PROCESS_AIPQ);
	if (IS_ERR_OR_NULL(uhmod)) {
		nn_aipq = kzalloc(sizeof(*nn_aipq), GFP_KERNEL);
		aipq_print(PRINT_OTHER, "attach:first attach, need alloc\n");
		if (!nn_aipq) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}
	} else {
		uvm_put_hook_mod(dmabuf, PROCESS_AIPQ);
		attached = true;
		nn_aipq = uhmod->arg;
		if (!nn_aipq) {
			aipq_print(PRINT_ERROR,
				"attach:aipq is null, dmabuf=%p\n", dmabuf);
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		aipq_print(PRINT_OTHER, "nn_aipq=%px\n", nn_aipq);
		memset(nn_aipq, 0, sizeof(struct vf_aipq_t));
	}

	dma_buf_put(dmabuf);

	if (dmabuf_last == dmabuf)
		aipq_info->repert_frame = 1;
	dmabuf_last = dmabuf;

	ret = aipq_vf_set_value(aipq_info, enable_aipq);
	if (ret != 0)
		aipq_print(PRINT_ERROR, "set aipq value err\n");

	if (attached)
		return 0;

	info->type = PROCESS_AIPQ;
	info->arg = nn_aipq;
	info->free = free_aipq_data;
	info->acquire_fence = NULL;
	info->getinfo = aipq_getinfo;
	info->setinfo = aipq_setinfo;

	return 0;
}

int aipq_setinfo(void *arg, char *buf)
{
	struct uvm_aipq_info *aipq_info = NULL;
	int ret = -1;

	aipq_info = (struct uvm_aipq_info *)buf;

	ret = aipq_vf_set_value(aipq_info, true);
	if (ret != 0)
		aipq_print(PRINT_ERROR, "set aipq value err\n");

	return ret;
}

static void dump_224(struct vframe_s *vf, phys_addr_t addr)
{
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	mm_segment_t fs;
	loff_t pos;

	snprintf(name_buf, sizeof(name_buf), "/data/tmp/224.bin");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	write_size = 224 * 224 * 3;
	data = codec_mm_vmap(addr, write_size);
	if (!data)
		return;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	aipq_print(PRINT_ERROR, "aipq: write %u size to addr%p\n",
		write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);

	snprintf(name_buf, sizeof(name_buf), "/data/tmp/dec.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	write_size = vf->canvas0_config[0].width * vf->canvas0_config[0].height
		* 3 / 2;
	data = codec_mm_vmap(vf->canvas0_config[0].phy_addr, write_size);
	if (!data)
		return;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	aipq_print(PRINT_ERROR, "aipq: write %u size to addr%p\n",
		write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
}

int aipq_getinfo(void *arg, char *buf)
{
	struct uvm_aipq_info *aipq_info = NULL;
	int ret = -1;
	phys_addr_t addr = 0;
	struct vframe_s *vf = NULL;
	s32 aipq_fd;
	size_t len = 0;
	struct ge2d_output_t output;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;

	aipq_info = (struct uvm_aipq_info *)buf;

	if (aipq_info->get_info_type == AIPQ_GET_224_DATA) {
		vf = aipq_get_dw_vf(aipq_info);
		if (IS_ERR_OR_NULL(vf)) {
			aipq_print(PRINT_ERROR, "get no vf\n");
			return -EINVAL;
		}

		aipq_fd = aipq_info->aipq_fd;
		if (aipq_fd != -1) {
			ret = meson_ion_share_fd_to_phys(aipq_fd, &addr, &len);
			if (ret < 0) {
				aipq_print(PRINT_ERROR,
					"import fd %d failed\n", aipq_fd);
				return -EINVAL;
			}
		}
		memset(&output, 0, sizeof(struct ge2d_output_t));
		output.width = 224;
		output.height = 224;
		output.format = GE2D_FORMAT_S24_BGR;
		output.addr = addr;
		do_gettimeofday(&begin_time);
		ret = ge2d_224_process(vf, &output);
		if (ret < 0) {
			aipq_print(PRINT_ERROR, "ge2d err\n");
			return -EINVAL;
		}
		do_gettimeofday(&end_time);
		cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
			+ (end_time.tv_usec - begin_time.tv_usec)) / 1000;
		aipq_print(PRINT_OTHER, "ge2d cost: %d ms\n", cost_time);
		if (uvm_aipq_dump) {
			uvm_aipq_dump = 0;
			dump_224(vf, addr);
		}
	}
	return 0;
}
