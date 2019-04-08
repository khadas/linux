/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include "../vdec_drv_if.h"
#include "../aml_vcodec_util.h"
#include "../aml_vcodec_dec.h"
//#include "../aml_vcodec_intr.h"
#include "../aml_vcodec_adapt.h"
#include "../vdec_drv_base.h"
#include "../aml_vcodec_vfm.h"
#include "h264_stream.h"
#include "h264_parse.h"
#include <uapi/linux/swab.h>

/* h264 NALU type */
#define NAL_NON_IDR_SLICE			0x01
#define NAL_IDR_SLICE				0x05
#define NAL_H264_SEI				0x06
#define NAL_H264_SPS				0x07
#define NAL_H264_PPS				0x08

#define NAL_TYPE(value)				((value) & 0x1F)

#define BUF_PREDICTION_SZ			(64 * 1024)//(32 * 1024)

#define MB_UNIT_LEN				16

/* motion vector size (bytes) for every macro block */
#define HW_MB_STORE_SZ				64

#define H264_MAX_FB_NUM				17
#define HDR_PARSING_BUF_SZ			1024

#define HEADER_BUFFER_SIZE			(32 * 1024)

/**
 * struct h264_fb - h264 decode frame buffer information
 * @vdec_fb_va  : virtual address of struct vdec_fb
 * @y_fb_dma    : dma address of Y frame buffer (luma)
 * @c_fb_dma    : dma address of C frame buffer (chroma)
 * @poc         : picture order count of frame buffer
 * @reserved    : for 8 bytes alignment
 */
struct h264_fb {
	uint64_t vdec_fb_va;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	int32_t poc;
	uint32_t reserved;
};

/**
 * struct h264_ring_fb_list - ring frame buffer list
 * @fb_list   : frame buffer arrary
 * @read_idx  : read index
 * @write_idx : write index
 * @count     : buffer count in list
 */
struct h264_ring_fb_list {
	struct h264_fb fb_list[H264_MAX_FB_NUM];
	unsigned int read_idx;
	unsigned int write_idx;
	unsigned int count;
	unsigned int reserved;
};

/**
 * struct vdec_h264_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @resolution_changed  : resoltion change happen
 * @realloc_mv_buf	: flag to notify driver to re-allocate mv buffer
 * @reserved		: for 8 bytes alignment
 * @bs_dma		: Input bit-stream buffer dma address
 * @y_fb_dma		: Y frame buffer dma address
 * @c_fb_dma		: C frame buffer dma address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 */
struct vdec_h264_dec_info {
	uint32_t dpb_sz;
	uint32_t resolution_changed;
	uint32_t realloc_mv_buf;
	uint32_t reserved;
	uint64_t bs_dma;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	uint64_t vdec_fb_va;
};

/**
 * struct vdec_h264_vsi - shared memory for decode information exchange
 *                        between VPU and Host.
 *                        The memory is allocated by VPU then mapping to Host
 *                        in vpu_dec_init() and freed in vpu_dec_deinit()
 *                        by VPU.
 *                        AP-W/R : AP is writer/reader on this item
 *                        VPU-W/R: VPU is write/reader on this item
 * @hdr_buf      : Header parsing buffer (AP-W, VPU-R)
 * @pred_buf_dma : HW working predication buffer dma address (AP-W, VPU-R)
 * @mv_buf_dma   : HW working motion vector buffer dma address (AP-W, VPU-R)
 * @list_free    : free frame buffer ring list (AP-W/R, VPU-W)
 * @list_disp    : display frame buffer ring list (AP-R, VPU-W)
 * @dec          : decode information (AP-R, VPU-W)
 * @pic          : picture information (AP-R, VPU-W)
 * @crop         : crop information (AP-R, VPU-W)
 */
struct vdec_h264_vsi {
	unsigned char hdr_buf[HDR_PARSING_BUF_SZ];
	char *header_buf;
	int sps_size;
	int pps_size;
	int sei_size;
	int head_offset;
	uint64_t pred_buf_dma;
	uint64_t mv_buf_dma[H264_MAX_FB_NUM];
	struct h264_ring_fb_list list_free;
	struct h264_ring_fb_list list_disp;
	struct vdec_h264_dec_info dec;
	struct vdec_pic_info pic;
	struct v4l2_rect crop;
};

/**
 * struct vdec_h264_inst - h264 decoder instance
 * @num_nalu : how many nalus be decoded
 * @ctx      : point to aml_vcodec_ctx
 * @pred_buf : HW working predication buffer
 * @mv_buf   : HW working motion vector buffer
 * @vpu      : VPU instance
 * @vsi      : VPU shared information
 */
struct vdec_h264_inst {
	unsigned int num_nalu;
	struct aml_vcodec_ctx *ctx;
	struct aml_vcodec_mem pred_buf;
	struct aml_vcodec_mem mv_buf[H264_MAX_FB_NUM];
	//struct vdec_vpu_inst vpu;
	struct aml_vdec_adapt vdec;
	struct vdec_h264_vsi *vsi;
	struct vcodec_vfm_s vfm;
};

#define DUMP_FILE_NAME "/data/dump/dump.tmp"
static struct file *filp;
static loff_t file_pos;

void dump_write(const char __user *buf, size_t count)
{
	mm_segment_t old_fs;

	if (!filp)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(filp, buf, count, &file_pos))
		pr_err("Failed to write file\n");

	set_fs(old_fs);
}

void dump_init(void)
{
	filp = filp_open(DUMP_FILE_NAME, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(filp)) {
		pr_err("open dump file failed\n");
		filp = NULL;
	}
}

void dump_deinit(void)
{
	if (filp) {
		filp_close(filp, current->files);
		filp = NULL;
		file_pos = 0;
	}
}

void swap_uv(void *uv, int size)
{
	int i;
	__u16 *p = uv;

	size /= 2;

	for (i = 0; i < size; i++, p++)
		*p = __swab16(*p);
}

static void get_pic_info(struct vdec_h264_inst *inst,
			 struct vdec_pic_info *pic)
{
	*pic = inst->vsi->pic;

	aml_vcodec_debug(inst, "pic(%d, %d), buf(%d, %d)",
			 pic->visible_width, pic->visible_height,
			 pic->coded_width, pic->coded_height);
	aml_vcodec_debug(inst, "Y(%d, %d), C(%d, %d)", pic->y_bs_sz,
			 pic->y_len_sz, pic->c_bs_sz, pic->c_len_sz);
}

static void get_crop_info(struct vdec_h264_inst *inst, struct v4l2_rect *cr)
{
	cr->left = inst->vsi->crop.left;
	cr->top = inst->vsi->crop.top;
	cr->width = inst->vsi->crop.width;
	cr->height = inst->vsi->crop.height;

	aml_vcodec_debug(inst, "l=%d, t=%d, w=%d, h=%d",
			 cr->left, cr->top, cr->width, cr->height);
}

static void get_dpb_size(struct vdec_h264_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = inst->vsi->dec.dpb_sz;
	aml_vcodec_debug(inst, "sz=%d", *dpb_sz);
}

static int find_start_code(unsigned char *data, unsigned int data_sz)
{
	if (data_sz > 3 && data[0] == 0 && data[1] == 0 && data[2] == 1)
		return 3;

	if (data_sz > 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 &&
	    data[3] == 1)
		return 4;

	return -1;
}

static int vdec_h264_init(struct aml_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	struct vdec_h264_inst *inst = NULL;
	int ret = -1;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->ctx = ctx;

	inst->vdec.format = VFORMAT_H264;
	inst->vdec.dev	= ctx->dev->vpu_plat_dev;
	inst->vdec.filp	= ctx->dev->filp;
	inst->vdec.ctx	= ctx;
	inst->vfm.ctx	= ctx;
	inst->vfm.ada_ctx = &inst->vdec;

	vcodec_vfm_init(&inst->vfm);

	ret = video_decoder_init(&inst->vdec);
	if (ret) {
		aml_vcodec_err(inst, "vdec_h264 init err=%d", ret);
		goto error_free_inst;
	}

	/* probe info from the stream */
	inst->vsi = kzalloc(sizeof(struct vdec_h264_vsi), GFP_KERNEL);
	if (!inst->vsi) {
		ret = -ENOMEM;
		goto error_free_inst;
	}

	/* alloc the header buffer to be used cache sps or spp etc.*/
	inst->vsi->header_buf = kzalloc(HEADER_BUFFER_SIZE, GFP_KERNEL);
	if (!inst->vsi) {
		ret = -ENOMEM;
		goto error_free_vsi;
	}

	inst->vsi->pic.visible_width	= 1920;
	inst->vsi->pic.visible_height	= 1080;
	inst->vsi->pic.coded_width	= 1920;
	inst->vsi->pic.coded_height	= 1088;
	inst->vsi->pic.y_bs_sz	= 0;
	inst->vsi->pic.y_len_sz	= (1920 * 1088);
	inst->vsi->pic.c_bs_sz	= 0;
	inst->vsi->pic.c_len_sz	= (1920 * 1088 / 2);

	aml_vcodec_debug(inst, "H264 Instance >> %p", inst);

	ctx->ada_ctx = &inst->vdec;
	*h_vdec = (unsigned long)inst;

	dump_init();

	return 0;

error_free_vsi:
	kfree(inst->vsi);
error_free_inst:
	kfree(inst);
	*h_vdec = 0;

	return ret;
}

static int refer_buffer_num(int level_idc, int poc_cnt,
	int mb_width, int mb_height)
{
	int max_ref_num = 27;
	int size, size_margin = 6;
	int pic_size = mb_width * mb_height * 384;

	switch (level_idc) {
	case 9:
		size = 152064;
		break;
	case 10:
		size = 152064;
		break;
	case 11:
		size = 345600;
		break;
	case 12:
		size = 912384;
		break;
	case 13:
		size = 912384;
		break;
	case 20:
		size = 912384;
		break;
	case 21:
		size = 1824768;
		break;
	case 22:
		size = 3110400;
		break;
	case 30:
		size = 3110400;
		break;
	case 31:
		size = 6912000;
		break;
	case 32:
		size = 7864320;
		break;
	case 40:
		size = 12582912;
		break;
	case 41:
		size = 12582912;
		break;
	case 42:
		size = 13369344;
		break;
	case 50:
		size = 42393600;
		break;
	case 51:
	case 52:
	default:
		size = 70778880;
		break;
	}

	size /= pic_size;
	size = size + 1; /* need more buffers */

	if (poc_cnt > size)
		size = poc_cnt;

	size = size + size_margin;
	if (size > max_ref_num)
		size = max_ref_num;

	return size;
}

static void fill_vdec_params(struct vdec_h264_inst *inst, struct h264_SPS_t *sps)
{
	struct vdec_pic_info *pic = &inst->vsi->pic;
	struct vdec_h264_dec_info *dec = &inst->vsi->dec;
	struct v4l2_rect *rect = &inst->vsi->crop;
	unsigned int mb_w, mb_h, width, height;
	unsigned int crop_unit_x = 0, crop_unit_y = 0;
	unsigned int poc_cnt = 0;

	mb_w = sps->pic_width_in_mbs_minus1 + 1;
	mb_h = sps->pic_height_in_map_units_minus1 + 1;

	width  = mb_w << 4; // 16
	height = (2 - sps->frame_mbs_only_flag) * (mb_h << 4);

	if (sps->frame_cropping_flag) {
		if (0 == sps->chroma_format_idc) {// monochrome
			crop_unit_x = 1;
			crop_unit_y = 2 - sps->frame_mbs_only_flag;
		} else if (1 == sps->chroma_format_idc) {// 4:2:0
			crop_unit_x = 2;
			crop_unit_y = 2 * (2 - sps->frame_mbs_only_flag);
		} else if (2 == sps->chroma_format_idc) {// 4:2:2
			crop_unit_x = 2;
			crop_unit_y = 2 - sps->frame_mbs_only_flag;
		} else {// 3 == sps.chroma_format_idc   // 4:4:4
			crop_unit_x = 1;
			crop_unit_y = 2 - sps->frame_mbs_only_flag;
		}
	}

	width  -= crop_unit_x * (sps->frame_crop_left_offset +
		sps->frame_crop_right_offset);
	height -= crop_unit_y * (sps->frame_crop_top_offset +
		sps->frame_crop_bottom_offset);

	/* fill visible area size that be used for EGL. */
	pic->visible_width	= width;
	pic->visible_height	= height;

	/* calc visible ares. */
	rect->left		= 0;
	rect->top		= 0;
	rect->width		= pic->visible_width;
	rect->height		= pic->visible_height;

	/* config canvas size that be used for decoder. */
	pic->coded_width	= ALIGN(mb_w, 4) << 4;
	pic->coded_height	= ALIGN(mb_h, 4) << 4;
	pic->y_len_sz		= pic->coded_width * pic->coded_height;
	pic->c_len_sz		= pic->y_len_sz >> 1;

	/* calc DPB size */
	poc_cnt = sps->pic_order_cnt_type;
	if (!poc_cnt)
		poc_cnt = (sps->log2_max_pic_order_cnt_lsb_minus4 + 4) << 1;

	dec->dpb_sz = refer_buffer_num(sps->level_idc, poc_cnt, mb_w, mb_h);

	pr_info("[%d] The stream infos, coded:(%d x %d), visible:(%d x %d), DPB: %d\n",
		inst->ctx->id, pic->coded_width, pic->coded_height,
		pic->visible_width, pic->visible_height, dec->dpb_sz);
}

static int vdec_h264_probe(unsigned long h_vdec,
	struct aml_vcodec_mem *bs, void *out)
{
	struct vdec_h264_inst *inst =
		(struct vdec_h264_inst *)h_vdec;
	struct h264_stream_t s;
	struct h264_SPS_t *sps;
	unsigned int nal_type;
	int nal_idx;
	int real_data_pos, real_data_size;
	unsigned char *buf = (unsigned char *)bs->va;
	unsigned int size = bs->size;

	nal_idx	= find_start_code(buf, size);
	if (nal_idx < 0)
		return -1;

	nal_type = NAL_TYPE(buf[nal_idx]);
	if (nal_type != NAL_H264_SPS)
		return -1;

	/* start code plus nal type. */
	real_data_pos = nal_idx + 1;
	real_data_size = size - real_data_pos;

	sps = kzalloc(sizeof(struct h264_SPS_t), GFP_KERNEL);
	if (sps == NULL)
		return -ENOMEM;

	h264_stream_set(&s, &buf[real_data_pos], real_data_size);
	h264_sps_parse(&s, sps);
	//h264_sps_info(sps);

	fill_vdec_params(inst, sps);

	kfree(sps);

	return 0;
}

static void vdec_h264_deinit(unsigned long h_vdec)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;

	if (!inst)
		return;

	aml_vcodec_debug_enter(inst);

	video_decoder_release(&inst->vdec);

	vcodec_vfm_release(&inst->vfm);

	dump_deinit();

	if (inst->vsi && inst->vsi->header_buf)
		kfree(inst->vsi->header_buf);

	if (inst->vsi)
		kfree(inst->vsi);

	kfree(inst);
}

static int vdec_h264_get_fb(struct vdec_h264_inst *inst, struct vdec_fb **out)
{
	return get_fb_from_queue(inst->ctx, out);
}

static void vdec_h264_get_vf(struct vdec_h264_inst *inst, struct vdec_fb **out)
{
	struct vframe_s *vf = NULL;
	struct vdec_fb *fb = NULL;

	aml_vcodec_debug(inst, "%s() [%d], vfm: %p",
		__func__, __LINE__, &inst->vfm);

	vf = peek_video_frame(&inst->vfm);
	if (!vf) {
		aml_vcodec_debug(inst, "there is no vframe.");
		*out = NULL;
		return;
	}

	vf = get_video_frame(&inst->vfm);
	if (!vf) {
		aml_vcodec_debug(inst, "the vframe is avalid.");
		*out = NULL;
		return;
	}

	aml_vcodec_debug(inst, "%s() [%d], vf: %p, v4l_mem_handle: %lx, idx: %d\n",
		__func__, __LINE__, vf, vf->v4l_mem_handle, vf->index);

	fb = (struct vdec_fb *)vf->v4l_mem_handle;
	fb->vf_handle = (unsigned long)vf;
	fb->status = FB_ST_DISPLAY;

	*out = fb;

	//pr_info("%s, %d\n", __func__, fb->base_y.bytes_used);
	//dump_write(fb->base_y.va, fb->base_y.bytes_used);
	//dump_write(fb->base_c.va, fb->base_c.bytes_used);

	/* convert yuv format. */
	//swap_uv(fb->base_c.va, fb->base_c.size);

	aml_vcodec_debug(inst, "%s() [%d], va: %p, phy: %x, size: %zu",
		__func__, __LINE__, fb->base_y.va,
		(unsigned int)virt_to_phys(fb->base_y.va), fb->base_y.size);
	aml_vcodec_debug(inst, "%s() [%d], va: %p, phy: %x, size: %zu",
		__func__, __LINE__, fb->base_c.va,
		(unsigned int)virt_to_phys(fb->base_c.va), fb->base_c.size);
}

static int vdec_h264_decode(unsigned long h_vdec, struct aml_vcodec_mem *bs,
			 unsigned long int timestamp, bool *res_chg)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;
	struct aml_vdec_adapt *vdec = &inst->vdec;
	int nal_start_idx = 0;
	int err = 0;
	unsigned int nal_start;
	unsigned int nal_type;
	unsigned char *buf;
	unsigned int buf_sz;
	int ret = 0;

	/* bs NULL means flush decoder */
	if (bs == NULL)
		return 0;//vpu_dec_reset(vpu);

	buf = (unsigned char *)bs->va;
	buf_sz = bs->size;
	nal_start_idx = find_start_code(buf, buf_sz);
	if (nal_start_idx < 0)
		goto err_free_fb_out;

	nal_start = buf[nal_start_idx];
	nal_type = NAL_TYPE(buf[nal_start_idx]);
	aml_vcodec_debug(inst, "NALU type: %d, size: %u", nal_type, buf_sz);

	if (nal_type == NAL_H264_SPS) {
		if (inst->vsi->head_offset + buf_sz > HEADER_BUFFER_SIZE) {
			err = -EILSEQ;
			goto err_free_fb_out;
		}
		inst->vsi->sps_size = buf_sz;
		memcpy(inst->vsi->header_buf + inst->vsi->head_offset, buf, buf_sz);
		inst->vsi->head_offset += inst->vsi->sps_size;
	} else if (nal_type == NAL_H264_PPS) {
			//buf_sz -= nal_start_idx;
		if (inst->vsi->head_offset + buf_sz > HEADER_BUFFER_SIZE) {
			err = -EILSEQ;
			goto err_free_fb_out;
		}
		inst->vsi->pps_size = buf_sz;
		memcpy(inst->vsi->header_buf + inst->vsi->head_offset, buf, buf_sz);
		inst->vsi->head_offset += inst->vsi->pps_size;
	} else if (nal_type == NAL_H264_SEI) {
		if (inst->vsi->head_offset + buf_sz > HEADER_BUFFER_SIZE) {
			err = -EILSEQ;
			goto err_free_fb_out;
		}
		inst->vsi->sei_size = buf_sz;
		memcpy(inst->vsi->header_buf + inst->vsi->head_offset, buf, buf_sz);
		inst->vsi->head_offset += inst->vsi->sei_size;
	} else {
		char *write_buf = vmalloc(inst->vsi->head_offset + buf_sz);

		memcpy(write_buf, inst->vsi->header_buf, inst->vsi->head_offset);
		memcpy(write_buf + inst->vsi->head_offset, buf, buf_sz);

		ret = vdec_vframe_write(vdec, write_buf,
			inst->vsi->head_offset + buf_sz, timestamp);

		aml_vcodec_debug(inst, "buf: %p, buf size: %u, write to: %d",
			write_buf, inst->vsi->head_offset + buf_sz, ret);

		memset(inst->vsi->header_buf, 0, HEADER_BUFFER_SIZE);
		inst->vsi->head_offset = 0;
		inst->vsi->sps_size = 0;
		inst->vsi->pps_size = 0;
		inst->vsi->sei_size = 0;

		vfree(write_buf);
	}

	//err = vpu_dec_start(vpu, data, 2);
	/*if (err)
		goto err_free_fb_out;*/

#if 0
	*res_chg = inst->vsi->dec.resolution_changed;
	if (*res_chg) {
		struct vdec_pic_info pic;

		aml_vcodec_debug(inst, "- resolution changed -");
		get_pic_info(inst, &pic);

		if (inst->vsi->dec.realloc_mv_buf) {
			err = alloc_mv_buf(inst, &pic);
			if (err)
			goto err_free_fb_out;
		}
	}
#endif
	if (nal_type == NAL_NON_IDR_SLICE || nal_type == NAL_IDR_SLICE) {
		/* wait decoder done interrupt */
		//err = aml_vcodec_wait_for_done_ctx(inst->ctx, aml_INST_IRQ_RECEIVED, WAIT_INTR_TIMEOUT_MS);
		/*if (err)
			goto err_free_fb_out;*/

		//vpu_dec_end(vpu);
	}
	return ret;

err_free_fb_out:
	//put_fb_to_free(inst, fb);
	aml_vcodec_err(inst, "NALU[%d] err=%d", inst->num_nalu, err);

	return err;
}

static int vdec_h264_get_param(unsigned long h_vdec,
			       enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;

	if (!inst) {
		pr_err("the h264 inst of dec is void.");
		return -1;
	}

	switch (type) {
	case GET_PARAM_DISP_FRAME_BUFFER:
		vdec_h264_get_vf(inst, out);
		break;

	case GET_PARAM_FREE_FRAME_BUFFER:
		ret = vdec_h264_get_fb(inst, out);
		break;

	case GET_PARAM_PIC_INFO:
		get_pic_info(inst, out);
		break;

	case GET_PARAM_DPB_SIZE:
		get_dpb_size(inst, out);
		break;

	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;

	default:
		aml_vcodec_err(inst, "invalid get parameter type=%d", type);
		ret = -EINVAL;
	}

	return ret;
}

static struct vdec_common_if vdec_h264_if = {
	vdec_h264_init,
	vdec_h264_probe,
	vdec_h264_decode,
	vdec_h264_get_param,
	vdec_h264_deinit,
};

struct vdec_common_if *get_h264_dec_comm_if(void);

struct vdec_common_if *get_h264_dec_comm_if(void)
{
	return &vdec_h264_if;
}
