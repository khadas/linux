// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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

#include "meson_uvm_aicolor_processor.h"
static struct dma_buf *dmabuf_last;

static int uvm_aicolor_debug;
module_param(uvm_aicolor_debug, int, 0644);

static int uvm_open_aicolor;
module_param(uvm_open_aicolor, int, 0644);

static int uvm_aicolor_dump;
module_param(uvm_aicolor_dump, int, 0644);

static int uvm_aicolor_skip_height = 1088;
module_param(uvm_aicolor_skip_height, int, 0644);

#define PRINT_ERROR     0X0
#define PRINT_OTHER     0X0001
#define PRINT_NN_DUMP	0X0002

struct ge2d_output_t {
	int width;
	int height;
	u32 format;
	phys_addr_t addr;
};

int aicolor_print(int debug_flag, const char *fmt, ...)
{
	if ((uvm_aicolor_debug & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "uvm_aicolor:[%d]", 0);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

struct vframe_s *aicolor_get_dw_vf(struct uvm_aicolor_info *aicolor_info)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	int shared_fd = aicolor_info->shared_fd;
	int interlace_mode = 0;

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		aicolor_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return NULL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aicolor_print(PRINT_ERROR,
			"%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return NULL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		if (IS_ERR_OR_NULL(vf)) {
			aicolor_print(PRINT_ERROR, "%s: vf is NULL.\n", __func__);
			return NULL;
		}

		aicolor_print(PRINT_OTHER,
			"vf: %d*%d,flag=%x,type=%x\n",
			vf->width,
			vf->height,
			vf->flag,
			vf->type);

		di_vf = vf->vf_ext;
		interlace_mode = vf->type & VIDTYPE_TYPEMASK;
		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			if (interlace_mode != VIDTYPE_PROGRESSIVE) {
				/*for interlace*/
				aicolor_print(PRINT_OTHER,
					"use di vf: %d*%d,flag=%x,type=%x\n",
					di_vf->width,
					di_vf->height,
					di_vf->flag,
					di_vf->type);
				vf = di_vf;
			}
		}
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			aicolor_print(PRINT_OTHER, "get dw vf err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return NULL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data) {
			aicolor_print(PRINT_ERROR, "invalid fd no uvm/v4lvideo\n");
		} else {
			vf = &file_private_data->vf;
			if (vf->vf_ext)
				vf = vf->vf_ext;
		}
	}
	if (!vf) {
		aicolor_print(PRINT_ERROR, "not find vf\n");
		dma_buf_put(dmabuf);
		return NULL;
	}
	dma_buf_put(dmabuf);
	return vf;
}

static int aiface_canvas[4] = {-1, -1, -1, -1};
static struct ge2d_context_s *context;

static int get_canvas(u32 index)
{
	const char *owner = "aiface";

	if (aiface_canvas[index] < 0)
		aiface_canvas[index] = canvas_pool_map_alloc_canvas(owner);

	if (aiface_canvas[index] < 0)
		aicolor_print(PRINT_ERROR, "no canvas\n");
	return aiface_canvas[index];
}

static int ge2d_vf_process(struct vframe_s *vf, struct ge2d_output_t *output)
{
	struct config_para_ex_s ge2d_config_s;
	struct config_para_ex_s *ge2d_config = &ge2d_config_s;
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode, src_format;
	int input_width, input_height;
	u32 output_canvas = get_canvas(3);

	if (!context) {
		context = create_ge2d_work_queue();
		if (IS_ERR_OR_NULL(context)) {
			aicolor_print(PRINT_ERROR, "creat ge2d work failed\n");
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

	aicolor_print(PRINT_OTHER, "src width: %d, height: %d\n",
		input_width, input_height);

	src_format = get_ge2d_input_format(vf);
	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM ||
	    interlace_mode == VIDTYPE_INTERLACE_TOP) {
		input_height >>= 1;
	} else if (vf->height > uvm_aicolor_skip_height) {
		/*used to reduce bandwidth by change format to interlace*/
		aicolor_print(PRINT_OTHER, "use interlace format.\n");
		input_height >>= 1;
		src_format |= (GE2D_FMT_M24_YUV420T & (3 << 3));
	}

	ge2d_config->src_para.format = src_format;

	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;

	aicolor_print(PRINT_OTHER, "src width: %d, height: %d format =%x\n",
		input_width, input_height, ge2d_config->src_para.format);

	if (output->format == GE2D_FORMAT_S8_Y) {
		canvas_config(output_canvas, output->addr, output->width,
			output->height, CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	} else {
		canvas_config(output_canvas, output->addr, output->width * 3,
			output->height, CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	}

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
		aicolor_print(PRINT_ERROR,
			      "++ge2d configing error.\n");
		return -1;
	}

	stretchblt_noalpha(context, 0, 0, input_width, input_height,
			   0, 0, output->width, output->height);

	return 0;
}

void free_aicolor_data(void *arg)
{
	if (arg) {
		aicolor_print(PRINT_OTHER, "%s\n", __func__);
		vfree(arg);
	} else {
		aicolor_print(PRINT_ERROR, "%s NULL\n", __func__);
	}
}

int attach_aicolor_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *info)
{
	struct vf_aicolor_t *nn_aicolor = NULL;
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	struct uvm_handle *handle;
	bool attached = false;
	struct uvm_aicolor_info *aicolor_info = (struct uvm_aicolor_info *)buf;
	struct vframe_s *vf = NULL;
	bool enable_aicolor = true;

	aicolor_info->need_do_aicolor = 1;
	aicolor_info->dw_height = 0;
	aicolor_info->dw_width = 0;

	if (!uvm_open_aicolor) {
		aicolor_info->need_do_aicolor = 0;
		enable_aicolor = false;
	} else {
		vf = aicolor_get_dw_vf(aicolor_info);
		if (IS_ERR_OR_NULL(vf)) {
			aicolor_print(PRINT_OTHER, "get no vf\n");
			return -EINVAL;
		}
		aicolor_info->dw_height = vf->height;
		aicolor_info->dw_width = vf->width;
		if (vf->width > 3840 ||
		    vf->height > 2160 ||
		    vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
			aicolor_print(PRINT_OTHER, "bypass %d %d\n",
				vf->width, vf->height);
			aicolor_info->need_do_aicolor = 0;
			enable_aicolor = false;
		}
	}

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		aicolor_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aicolor_print(PRINT_ERROR,
			"attach:dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	handle = dmabuf->priv;
	uhmod = uvm_get_hook_mod(dmabuf, PROCESS_AICOLOR);
	if (IS_ERR_OR_NULL(uhmod)) {
		nn_aicolor = vmalloc(sizeof(*nn_aicolor));
		memset(nn_aicolor, 0, sizeof(struct vf_aicolor_t));
		aicolor_print(PRINT_OTHER, "attach:first attach, need alloc\n");
		if (!nn_aicolor) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}
	} else {
		uvm_put_hook_mod(dmabuf, PROCESS_AICOLOR);
		attached = true;
		nn_aicolor = uhmod->arg;
		if (!nn_aicolor) {
			aicolor_print(PRINT_ERROR,
				"attach:aicolor is null, dmabuf=%p\n", dmabuf);
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
	}

	dma_buf_put(dmabuf);

	if (dmabuf_last == dmabuf) {
		aicolor_info->repeat_frame = 1;
	} else {
		memset(nn_aicolor, 0, sizeof(struct vf_aicolor_t));
		aicolor_info->repeat_frame = 0;
		dmabuf_last = dmabuf;
	}
	if (vf)
		aicolor_info->omx_index = vf->omx_index;
	else
		aicolor_info->omx_index = -1;

	if (enable_aicolor)
		aicolor_print(PRINT_OTHER, "nn_aicolor=%px, omx_index=%d\n",
			nn_aicolor, aicolor_info->omx_index);

	if (attached)
		return 0;

	info->type = PROCESS_AICOLOR;
	info->arg = nn_aicolor;
	info->free = free_aicolor_data;
	info->acquire_fence = NULL;
	info->getinfo = aicolor_getinfo;
	info->setinfo = aicolor_setinfo;

	return 0;
}

int aicolor_setinfo(void *arg, char *buf)
{
	struct uvm_aicolor_info *aicolor_info = NULL;
	struct vf_aicolor_t *vf_aicolor = NULL;
	int i;

	aicolor_info = (struct uvm_aicolor_info *)buf;
	vf_aicolor = (struct vf_aicolor_t *)arg;

	if (aicolor_info->nn_status == NN_START_DOING) {
		do_gettimeofday(&vf_aicolor->start_time);
	} else if (aicolor_info->nn_status == NN_DONE) {
		for (i = 0; i < AI_COLOR_COUNT; i++) {
			vf_aicolor->color_value[i] = aicolor_info->color_value[i];
			aicolor_print(PRINT_OTHER,
				"NN_DONE: omx_index=%d: i=%d: value=%d.\n",
				aicolor_info->omx_index,
				i,
				vf_aicolor->color_value[i]);
		}
	}

	/*this must at the last line of this function*/
	vf_aicolor->nn_status = aicolor_info->nn_status;

	aicolor_print(PRINT_OTHER, "%s: omx_index=%d, status=%d\n",
		__func__, aicolor_info->omx_index, aicolor_info->nn_status);
	return 0;
}

static void dump_vf(struct vframe_s *vf, phys_addr_t addr, struct uvm_aicolor_info *info)
{
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	mm_segment_t fs;
	loff_t pos;

	if (IS_ERR_OR_NULL(vf) || IS_ERR_OR_NULL(info)) {
		aicolor_print(PRINT_ERROR, "dump param invalid.\n");
		return;
	}

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		aicolor_print(PRINT_ERROR, "%s: security vf.\n", __func__);
		return;
	}

	snprintf(name_buf, sizeof(name_buf), "/data/aicolor_ge2dOut.rgb");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	write_size = info->nn_input_frame_height * info->nn_input_frame_width * 3;
	data = codec_mm_vmap(addr, write_size);
	if (!data)
		return;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	aicolor_print(PRINT_ERROR, "aicolor: write %u size to addr%p\n",
		write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);

	snprintf(name_buf, sizeof(name_buf), "/data/aicolor_dec.yuv");
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
	aicolor_print(PRINT_ERROR, "aicolor: write %u size to addr%p\n",
		write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
}

int aicolor_getinfo(void *arg, char *buf)
{
	struct uvm_aicolor_info *aicolor_info = NULL;
	int ret = -1;
	phys_addr_t addr = 0;
	struct vframe_s *vf = NULL;
	s32 aicolor_fd;
	size_t len = 0;
	struct ge2d_output_t output;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;

	aicolor_info = (struct uvm_aicolor_info *)buf;

	if (aicolor_info->get_info_type == AICOLOR_GET_RGB_DATA) {
		vf = aicolor_get_dw_vf(aicolor_info);
		if (IS_ERR_OR_NULL(vf)) {
			aicolor_print(PRINT_ERROR, "get no vf\n");
			return -EINVAL;
		}

		aicolor_fd = aicolor_info->aicolor_fd;
		if (aicolor_fd != -1) {
			ret = meson_ion_share_fd_to_phys(aicolor_fd, &addr, &len);
			if (ret < 0) {
				aicolor_print(PRINT_ERROR,
					"import fd %d failed\n", aicolor_fd);
				return -EINVAL;
			}
		}
		memset(&output, 0, sizeof(struct ge2d_output_t));
		output.width = aicolor_info->nn_input_frame_width;
		output.height = aicolor_info->nn_input_frame_height;
		aicolor_info->omx_index = vf->omx_index;

		output.format = GE2D_FORMAT_S24_BGR;
		output.addr = addr;
		do_gettimeofday(&begin_time);
		ret = ge2d_vf_process(vf, &output);
		if (ret < 0) {
			aicolor_print(PRINT_ERROR, "ge2d err\n");
			return -EINVAL;
		}
		do_gettimeofday(&end_time);
		cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
			+ (end_time.tv_usec - begin_time.tv_usec)) / 1000;
		aicolor_print(PRINT_OTHER, "ge2d cost: %d ms\n", cost_time);
		if (uvm_aicolor_dump) {
			uvm_aicolor_dump = 0;
			dump_vf(vf, addr, aicolor_info);
		}
	}
	return 0;
}

