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
#include <linux/amlogic/media/vout/vout_notify.h>
#include "meson_uvm_nn_processor.h"

static int uvm_nn_debug;
module_param(uvm_nn_debug, int, 0644);

static int uvm_open_nn;
module_param(uvm_open_nn, int, 0644);

static int uvm_aisr_dump;
module_param(uvm_aisr_dump, int, 0644);

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001

int nn_print(int debug_flag, const char *fmt, ...)
{
	if ((uvm_nn_debug & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "uvm_nn:[%d]", 0);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

struct vframe_s *nn_get_vframe(int shared_fd)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct file_private_data *file_private_data = NULL;
	struct vframe_s *vframe = NULL;

	dmabuf = dma_buf_get(shared_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		nn_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return NULL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		nn_print(PRINT_ERROR,
			"%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return NULL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vframe = dmabuf_get_vframe(dmabuf);
		if (IS_ERR_OR_NULL(vframe)) {
			nn_print(PRINT_ERROR, "get vframe failed.\n");
		} else {
			if (vframe->vf_ext &&
			    (vframe->flag & VFRAME_FLAG_CONTAIN_POST_FRAME))
				vframe = vframe->vf_ext;
		}
		nn_print(PRINT_OTHER, "vframe type: %d\n", vframe->type);
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			nn_print(PRINT_OTHER, "get_fh err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return NULL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data)
			nn_print(PRINT_ERROR, "invalid fd no uvm/v4lvideo\n");
		else
			vframe = &file_private_data->vf;
	}

	dma_buf_put(dmabuf);

	if (!vframe) {
		nn_print(PRINT_ERROR, "%s: get vf failed.\n", __func__);
		return NULL;
	}

	return vframe;
}

int nn_get_hf_info(int shared_fd, struct vf_nn_sr_t *nn_sr, int *di_flag)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct vframe_s *vf = NULL;
	struct file_private_data *file_private_data = NULL;
	struct hf_info_t *hf_info;

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		nn_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		nn_print(PRINT_ERROR,
			"%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		if (vf->vf_ext && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME))
			vf = vf->vf_ext;
		nn_print(PRINT_OTHER, "vframe type: %d\n", vf->type);
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			nn_print(PRINT_OTHER, "get_fh err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data)
			nn_print(PRINT_ERROR, "invalid fd no uvm/v4lvideo\n");
		else
			vf = &file_private_data->vf;
	}
	if (vf) {
		hf_info = vf->hf_info;
		if (hf_info) {
			nn_sr->hf_phy_addr = hf_info->phy_addr;
			nn_sr->hf_width = hf_info->width;
			nn_sr->hf_height = hf_info->height;
			nn_sr->hf_align_w = hf_info->buffer_w;
			nn_sr->hf_align_h = hf_info->buffer_h;
			if (vf->type_original & VIDTYPE_INTERLACE)
				*di_flag = 1;
			else
				*di_flag = 0;
			nn_print(PRINT_OTHER,
				"%s:phy=%llx, omx_index=%d, hf_align: %d*%d.\n",
				__func__,
				hf_info->phy_addr,
				vf->omx_index,
				nn_sr->hf_align_w,
				nn_sr->hf_align_h);
		} else {
			nn_print(PRINT_OTHER, "no hf info\n");
			nn_sr->hf_phy_addr = 0;
			nn_sr->hf_width = 0;
			nn_sr->hf_height = 0;
			*di_flag = 0;
		}
	} else {
		nn_print(PRINT_ERROR, "%s: not find vf\n", __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	dma_buf_put(dmabuf);
	return 0;
}

void free_nn_data(void *arg)
{
	struct vf_nn_sr_t *nn_sr = (struct vf_nn_sr_t *)arg;

	if (arg) {
		nn_print(PRINT_OTHER, "%s: nn_out_file_count =%d, nn_sr=%px\n",
			__func__, nn_sr->nn_out_file_count, nn_sr);
		if (nn_sr->nn_out_file) {
			fput(nn_sr->nn_out_file);
			if (nn_sr->nn_out_file_count == 0)
				nn_print(PRINT_ERROR,
				"%s:  nn_out_file_count is 0!!\n", __func__);
			nn_sr->nn_out_file_count--;
		}
		kfree((u8 *)arg);
	} else {
		nn_print(PRINT_ERROR, "%s: arg is NULL\n", __func__);
	}
}

int attach_nn_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *info)
{
	struct vf_nn_sr_t *nn_sr = NULL;
	int ret = 0;
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	struct uvm_handle *handle;
	struct uvm_ai_sr_info *ai_sr_info = (struct uvm_ai_sr_info *)buf;
	struct vf_nn_sr_t nn_sr_t;
	bool attached = false;
	int src_interlace_flag = 0;
	struct vframe_s *vf = NULL;
	memset(&nn_sr_t, 0, sizeof(struct vf_nn_sr_t));

	if (!uvm_open_nn) {
		ai_sr_info->hf_phy_addr = 0;
		ai_sr_info->hf_width = 0;
		ai_sr_info->hf_height = 0;
		ai_sr_info->need_do_aisr = 0;
	} else {
		ret = nn_get_hf_info(shared_fd, &nn_sr_t, &src_interlace_flag);
		ai_sr_info->hf_phy_addr = nn_sr_t.hf_phy_addr;
		ai_sr_info->hf_width = nn_sr_t.hf_width;
		ai_sr_info->hf_height = nn_sr_t.hf_height;
		ai_sr_info->need_do_aisr = 1;
		vf = nn_get_vframe(shared_fd);
		if (vf) {
			if (vf->flag & VFRAME_FLAG_GAME_MODE) {
				nn_print(PRINT_OTHER,
					"game mode bypass ai_sr.\n");
				ai_sr_info->need_do_aisr = 0;
			}
			if (vf->flag & VFRAME_FLAG_PC_MODE) {
				nn_print(PRINT_OTHER,
					"pc mode bypass ai_sr.\n");
				ai_sr_info->need_do_aisr = 0;
			}
		}
	}

	if (ret) {
		nn_print(PRINT_OTHER, "attach:get hf info error\n");
		return -EINVAL;
	}

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		nn_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		nn_print(PRINT_ERROR,
			"attach:dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	handle = dmabuf->priv;
	uhmod = uvm_get_hook_mod(dmabuf, PROCESS_NN);
	if (IS_ERR_OR_NULL(uhmod)) {
		nn_sr = kzalloc(sizeof(*nn_sr), GFP_KERNEL);
		nn_print(PRINT_OTHER, "attach:first attach, need alloc\n");
		if (!nn_sr) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}
	} else {
		uvm_put_hook_mod(dmabuf, PROCESS_NN);
		attached = true;
		nn_sr = uhmod->arg;
		if (!nn_sr) {
			nn_print(PRINT_ERROR,
				"attach:nn_sr is null, dmabuf=%p\n", dmabuf);
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		nn_print(PRINT_OTHER, "nn_sr=%px\n", nn_sr);
		if (nn_sr->nn_out_file_count != 0) {
			fput(nn_sr->nn_out_file);
			nn_sr->nn_out_file_count--;
		} else {
			nn_print(PRINT_OTHER,
				"%s:  nn_out_file_count is 0!!\n", __func__);
		}
		memset(nn_sr, 0, sizeof(struct vf_nn_sr_t));
	}

	*nn_sr = nn_sr_t;
	nn_sr->nn_status = ai_sr_info->nn_status;

	nn_print(PRINT_OTHER,
		"%s: shared_fd=%d, fence_fd=%d, %d*%d, nn_status=%d, nn_sr=%px\n",
		__func__,
		ai_sr_info->shared_fd,
		ai_sr_info->fence_fd,
		ai_sr_info->hf_width,
		ai_sr_info->hf_height,
		nn_sr->nn_status,
		nn_sr);

	nn_sr->nn_out_dma_buf = NULL;
	nn_sr->shared_fd = shared_fd;
	if (ai_sr_info->fence_fd != -1)
		nn_sr->fence = sync_file_get_fence(ai_sr_info->fence_fd);
	else
		nn_sr->fence = NULL;

	dma_buf_put(dmabuf);

	if (attached)
		return 0;

	info->type = PROCESS_NN;
	info->arg = nn_sr;
	info->free = free_nn_data;
	info->acquire_fence = NULL;
	info->getinfo = nn_mod_getinfo;
	info->setinfo = nn_mod_setinfo;

	return 0;
}

static void dump_vf(struct vframe_s *vf)
{
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	mm_segment_t fs;
	loff_t pos;

	if (!vf)
		return;

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		nn_print(PRINT_ERROR, "%s: security vf.\n", __func__);
		return;
	}

	snprintf(name_buf, sizeof(name_buf), "/data/aisr_DI.bin");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;

	write_size = vf->canvas0_config[0].width * vf->canvas0_config[0].height
		* 2 * 10 / 8;

	data = codec_mm_vmap(vf->canvas0_config[0].phy_addr, write_size);
	if (!data)
		return;

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	nn_print(PRINT_OTHER, "nn: write %u size to addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
}

int dump_hf(struct vf_nn_sr_t *nn_sr_dst)
{
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	mm_segment_t fs;
	loff_t pos;

	snprintf(name_buf, sizeof(name_buf), "/data/aisr_hf.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return -1;
	write_size = nn_sr_dst->hf_align_w * nn_sr_dst->hf_align_h;
	data = codec_mm_vmap(nn_sr_dst->hf_phy_addr, write_size);
	if (!data)
		return -2;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	nn_print(PRINT_OTHER, "nn: write %u size to addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
	return 0;
}

int nn_mod_setinfo(void *arg, char *buf)
{
	struct uvm_ai_sr_info *nn_sr_src = NULL;
	struct vf_nn_sr_t *nn_sr_dst = NULL;
	int nn_out_fd;
	int ret = -1;
	size_t len;
	phys_addr_t addr = 0;
	struct vframe_s *vf = NULL;

	nn_sr_src = (struct uvm_ai_sr_info *)buf;
	nn_sr_dst = (struct vf_nn_sr_t *)arg;

	nn_out_fd = nn_sr_src->nn_out_fd;
	if (nn_out_fd != -1) {
		nn_sr_dst->nn_out_file = fget(nn_out_fd);
		fput(nn_sr_dst->nn_out_file);
		nn_sr_dst->nn_out_dma_buf = NULL;
		ret = meson_ion_share_fd_to_phys(nn_out_fd, &addr, &len);
		if (ret < 0) {
			nn_print(PRINT_ERROR, "import out fd %d failed\n", nn_out_fd);
			return -EINVAL;
		}
		nn_sr_dst->nn_out_phy_addr = addr;
	} else {
		nn_sr_dst->nn_out_dma_buf = NULL;
		nn_sr_dst->nn_out_phy_addr = 0;
	}

	nn_sr_dst->nn_index = nn_sr_src->nn_index;
	nn_sr_dst->nn_out_width = nn_sr_src->nn_out_width;
	nn_sr_dst->nn_out_height = nn_sr_src->nn_out_height;
	nn_sr_dst->nn_mode = nn_sr_src->nn_mode;

	if (nn_sr_src->nn_status == NN_START_DOING) {
		nn_print(PRINT_OTHER, "%s: buf_align:%d*%d.\n",
			__func__,
			nn_sr_src->buf_align_w,
			nn_sr_src->buf_align_h);
		nn_sr_dst->buf_align_w = nn_sr_src->buf_align_w;
		nn_sr_dst->buf_align_h = nn_sr_src->buf_align_h;

		if (uvm_aisr_dump != 0) {
			uvm_aisr_dump = 0;
			vf = nn_get_vframe(nn_sr_src->shared_fd);
			dump_vf(vf);
			dump_hf(nn_sr_dst);
		}

		do_gettimeofday(&nn_sr_dst->start_time);
		get_file(nn_sr_dst->nn_out_file);
		nn_sr_dst->nn_out_file_count++;
	}

	nn_sr_dst->nn_status = nn_sr_src->nn_status;/*this must at the last line of this function*/

	nn_print(PRINT_OTHER,
		"%s: shared_fd=%d,nn_fd=%d,status=%d,nn_index=%d,nn_mode=%d.\n",
		__func__,
		nn_sr_src->shared_fd, nn_out_fd,
		nn_sr_src->nn_status,
		nn_sr_src->nn_index,
		nn_sr_dst->nn_mode);
	return 0;
}

int nn_mod_getinfo(void *arg, char *buf)
{
	struct vf_nn_sr_t *vf_nn_sr = NULL;
	struct uvm_ai_sr_info *ai_sr_info = NULL;
	struct vinfo_s *vinfo = NULL;
	int ret = -1;
	int src_interlace_flag = 0;

	ai_sr_info = (struct uvm_ai_sr_info *)buf;
	vf_nn_sr = (struct vf_nn_sr_t *)arg;

	if (ai_sr_info->get_info_type != GET_HF_INFO &&
		ai_sr_info->get_info_type != GET_VINFO_SIZE) {
		nn_print(PRINT_ERROR, "%s: err get_info_type =%d\n",
			__func__, ai_sr_info->get_info_type);
		return -EINVAL;
	}

	if (ai_sr_info->get_info_type == GET_HF_INFO) {
		ret = nn_get_hf_info(ai_sr_info->shared_fd,
					vf_nn_sr,
					&src_interlace_flag);
		if (ret) {
			nn_print(PRINT_OTHER, "get hf info error\n");
			return -EINVAL;
		}

		nn_print(PRINT_OTHER, "%s: hf_phy_addr=0x%x, %d*%d, hf_align:%d*%d.\n",
			__func__,
			vf_nn_sr->hf_phy_addr,
			vf_nn_sr->hf_width,
			vf_nn_sr->hf_height,
			vf_nn_sr->hf_align_w,
			vf_nn_sr->hf_align_h);
		ai_sr_info->hf_phy_addr = vf_nn_sr->hf_phy_addr;
		ai_sr_info->hf_width = vf_nn_sr->hf_width;
		ai_sr_info->hf_height = vf_nn_sr->hf_height;
		ai_sr_info->hf_align_w = vf_nn_sr->hf_align_w;
		ai_sr_info->hf_align_h = vf_nn_sr->hf_align_h;
		ai_sr_info->nn_out_phy_addr = vf_nn_sr->nn_out_phy_addr;
		ai_sr_info->nn_status = vf_nn_sr->nn_status;
		ai_sr_info->nn_index = vf_nn_sr->nn_index;
		ai_sr_info->src_interlace_flag = src_interlace_flag;
	} else if (ai_sr_info->get_info_type == GET_VINFO_SIZE) {
		vinfo = get_current_vinfo();
		if (IS_ERR_OR_NULL(vinfo)) {
			nn_print(PRINT_OTHER, "get vinfo error\n");
			return -EINVAL;
		}

		nn_print(PRINT_OTHER, "getvinfo: width=%d, height=%d.\n",
			vinfo->width,
			vinfo->height);
		ai_sr_info->vinfo_width = vinfo->width;
		ai_sr_info->vinfo_height = vinfo->height;
	}

	return 0;
}
