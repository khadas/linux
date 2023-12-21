// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <linux/clk.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <linux/pm_runtime.h>
#include <linux/rk-vpss-config.h>

#include "dev.h"
#include "hw.h"
#include "regs.h"

struct rkvpss_output_ch {
	u32 ctrl;
	u32 size;
	u32 c_offs;
};

struct rkvpss_offline_buf {
	struct list_head list;
	struct vb2_buffer vb;
	struct vb2_queue vb2_queue;
	struct file *file;
	struct dma_buf *dbuf;
	void *mem;
	int dev_id;
	int fd;
	bool alloc;
};

static void init_vb2(struct rkvpss_offline_dev *ofl,
		     struct rkvpss_offline_buf *buf)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	unsigned long attrs = DMA_ATTR_NO_KERNEL_MAPPING;

	if (!buf)
		return;
	memset(&buf->vb, 0, sizeof(buf->vb));
	memset(&buf->vb2_queue, 0, sizeof(buf->vb2_queue));
	buf->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	buf->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	if (hw->is_dma_contig)
		attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	buf->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_queue = &buf->vb2_queue;
}

static void buf_del(struct file *file, int id, int fd, bool is_all, bool running)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkvpss_offline_buf *buf, *next;

	mutex_lock(&hw->dev_lock);
	list_for_each_entry_safe(buf, next, &ofl->list, list) {
		if (buf->file == file && (is_all || buf->fd == fd)) {
			if (!is_all && running && buf->alloc)
				break;
			v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
				 "%s file:%p dev_id:%d fd:%d dbuf:%p\n",
				 __func__, file, buf->dev_id, buf->fd, buf->dbuf);
			if (!buf->alloc) {
				ops->unmap_dmabuf(buf->mem);
				ops->detach_dmabuf(buf->mem);
				dma_buf_put(buf->dbuf);
			} else {
				dma_buf_put(buf->dbuf);
				ops->put(buf->mem);
			}
			buf->file = NULL;
			buf->mem = NULL;
			buf->dbuf = NULL;
			buf->fd = -1;
			list_del(&buf->list);
			kfree(buf);
			if (!is_all)
				break;
		}
	}
	mutex_unlock(&hw->dev_lock);
}

static struct rkvpss_offline_buf *buf_add(struct file *file, int id, int fd, int size)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkvpss_offline_buf *buf = NULL, *next = NULL;
	struct dma_buf *dbuf;
	void *mem = NULL;
	bool is_add = true;

	dbuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d invalid dmabuf fd:%d", id, fd);
		return buf;
	}
	if (size && dbuf->size < size) {
		v4l2_err(&ofl->v4l2_dev,
			 "dev_id:%d input fd:%d size error:%zu < %u\n",
			 id, fd, dbuf->size, size);
		dma_buf_put(dbuf);
		return buf;
	}

	mutex_lock(&hw->dev_lock);
	list_for_each_entry_safe(buf, next, &ofl->list, list) {
		if (buf->file == file && buf->fd == fd && buf->dbuf == dbuf) {
			is_add = false;
			break;
		}
	}

	if (is_add) {
		buf = kzalloc(sizeof(struct rkvpss_offline_buf), GFP_KERNEL);
		if (!buf)
			goto end;
		init_vb2(ofl, buf);
		mem = ops->attach_dmabuf(&buf->vb, hw->dev, dbuf, dbuf->size);
		if (IS_ERR(mem)) {
			v4l2_err(&ofl->v4l2_dev, "failed to attach dmabuf, fd:%d\n", fd);
			dma_buf_put(dbuf);
			kfree(buf);
			buf = NULL;
			goto end;
		}
		if (ops->map_dmabuf(mem)) {
			v4l2_err(&ofl->v4l2_dev, "failed to map, fd:%d\n", fd);
			ops->detach_dmabuf(mem);
			dma_buf_put(dbuf);
			mem = NULL;
			kfree(buf);
			buf = NULL;
			goto end;
		}
		buf->dev_id = id;
		buf->fd = fd;
		buf->file = file;
		buf->dbuf = dbuf;
		buf->mem = mem;
		buf->alloc = false;
		list_add_tail(&buf->list, &ofl->list);
		v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
			 "%s file:%p dev_id:%d fd:%d dbuf:%p\n", __func__, file, id, fd, dbuf);
	} else {
		dma_buf_put(dbuf);
	}
end:
	mutex_unlock(&hw->dev_lock);
	return buf;
}

static int internal_buf_alloc(struct file *file, struct rkvpss_buf_info *info)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkvpss_offline_buf *buf;
	struct dma_buf *dbuf;
	int fd, i, size;
	void *mem;

	for (i = 0; i < info->buf_cnt; i++) {
		info->buf_fd[i] = -1;
		size = PAGE_ALIGN(info->buf_size[i]);
		if (!size)
			continue;
		buf = kzalloc(sizeof(struct rkvpss_offline_buf), GFP_KERNEL);
		if (!buf)
			goto err;
		init_vb2(ofl, buf);
		mem = ops->alloc(&buf->vb, hw->dev, size);
		if (IS_ERR_OR_NULL(mem)) {
			kfree(buf);
			goto err;
		}
		dbuf = ops->get_dmabuf(&buf->vb, mem, O_RDWR);
		if (IS_ERR_OR_NULL(dbuf)) {
			ops->put(mem);
			kfree(buf);
			goto err;
		}
		fd = dma_buf_fd(dbuf, O_CLOEXEC);
		if (fd < 0) {
			dma_buf_put(dbuf);
			ops->put(mem);
			kfree(buf);
			goto err;
		}
		get_dma_buf(dbuf);

		info->buf_fd[i] = fd;
		buf->fd = fd;
		buf->file = file;
		buf->dbuf = dbuf;
		buf->mem = mem;
		buf->alloc = true;
		buf->dev_id = info->dev_id;
		mutex_lock(&hw->dev_lock);
		list_add_tail(&buf->list, &ofl->list);
		mutex_unlock(&hw->dev_lock);
		v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
			 "%s file:%p dev_id:%d fd:%d dbuf:%p\n",
			 __func__, file, buf->dev_id, fd, dbuf);
	}
	return 0;
err:
	for (i -= 1; i >= 0; i--)
		buf_del(file, info->dev_id, info->buf_fd[i], false, false);
	return -ENOMEM;
}

static int external_buf_add(struct file *file, struct rkvpss_buf_info *info)
{
	void *mem;
	int i;

	for (i = 0; i < info->buf_cnt; i++) {
		mem = buf_add(file, info->dev_id, info->buf_fd[i], info->buf_size[i]);
		if (!mem)
			goto err;
	}
	return 0;
err:
	for (i -= 1; i >= 0; i--)
		buf_del(file, info->dev_id, info->buf_fd[i], false, false);
	return -ENOMEM;
}

static int rkvpss_ofl_buf_add(struct file *file, struct rkvpss_buf_info *info)
{
	int ret;

	if (info->buf_alloc)
		ret = internal_buf_alloc(file, info);
	else
		ret = external_buf_add(file, info);
	return ret;
}

static void rkvpss_ofl_buf_del(struct file *file, struct rkvpss_buf_info *info)
{
	int i;

	for (i = 0; i < info->buf_cnt; i++)
		buf_del(file, info->dev_id, info->buf_fd[i], false, false);
}

static void poly_phase_scale(struct rkvpss_offline_dev *ofl,
			     struct rkvpss_output_cfg *cfg)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 in_w = cfg->crop_width, in_h = cfg->crop_height;
	u32 out_w = cfg->scl_width, out_h = cfg->scl_height;
	u32 ctrl, y_xscl_fac, y_yscl_fac, uv_xscl_fac, uv_yscl_fac;
	u32 i, j, idx, ratio, val, in_div, out_div, factor;
	bool dering_en = false, yuv420_in = false, yuv422_to_420 = false;

	if (in_w == out_w && in_h == out_w) {
		rkvpss_hw_write(hw, RKVPSS_ZME_Y_SCL_CTRL, 0);
		rkvpss_hw_write(hw, RKVPSS_ZME_UV_SCL_CTRL, 0);
		goto end;
	}

	/* TODO diff for input and output format */
	if (yuv420_in) {
		in_div = 2;
		out_div = 2;
	} else if (yuv422_to_420) {
		in_div = 1;
		out_div = 2;
	} else {
		in_div = 1;
		out_div = 1;
	}

	val = (in_w - 1) | ((in_h - 1) << 16);
	rkvpss_hw_write(hw, RKVPSS_ZME_Y_SRC_SIZE, val);
	rkvpss_hw_write(hw, RKVPSS_ZME_UV_SRC_SIZE, val);
	val = (out_w - 1) | ((out_h - 1) << 16);
	rkvpss_hw_write(hw, RKVPSS_ZME_Y_DST_SIZE, val);
	rkvpss_hw_write(hw, RKVPSS_ZME_UV_DST_SIZE, val);

	ctrl = RKVPSS_ZME_XSCL_MODE | RKVPSS_ZME_YSCL_MODE;
	if (dering_en) {
		ctrl |= RKVPSS_ZME_DERING_EN;
		rkvpss_hw_write(hw, RKVPSS_ZME_Y_DERING_PARA, 0xd10410);
		rkvpss_hw_write(hw, RKVPSS_ZME_UV_DERING_PARA, 0xd10410);
	}
	if (in_w != out_w) {
		if (in_w > out_w) {
			factor = 4096;
			ctrl |= RKVPSS_ZME_XSD_EN;
		} else {
			factor = 65536;
			ctrl |= RKVPSS_ZME_XSU_EN;
		}
		y_xscl_fac = (in_w - 1) * factor / (out_w - 1);
		uv_xscl_fac = (in_w / 2 - 1) * factor / (out_w / 2 - 1);

		ratio = y_xscl_fac * 10000 / factor;
		idx = rkvpss_get_zme_tap_coe_index(ratio);
		for (i = 0; i < 17; i++) {
			for (j = 0; j < 8; j += 2) {
				val = RKVPSS_ZME_TAP_COE(rkvpss_zme_tap8_coe[idx][i][j],
							 rkvpss_zme_tap8_coe[idx][i][j + 1]);
				rkvpss_hw_write(hw, RKVPSS_ZME_Y_HOR_COE0_10 + i * 16 + j * 2, val);
				rkvpss_hw_write(hw, RKVPSS_ZME_UV_HOR_COE0_10 + i * 16 + j * 2, val);
			}
		}
	} else {
		y_xscl_fac = 0;
		uv_xscl_fac = 0;
	}
	rkvpss_hw_write(hw, RKVPSS_ZME_Y_XSCL_FACTOR, y_xscl_fac);
	rkvpss_hw_write(hw, RKVPSS_ZME_UV_XSCL_FACTOR, uv_xscl_fac);

	if (in_h != out_h) {
		if (in_h > out_h) {
			factor = 4096;
			ctrl |= RKVPSS_ZME_YSD_EN;
		} else {
			factor = 65536;
			ctrl |= RKVPSS_ZME_YSU_EN;
		}
		y_yscl_fac = (in_h - 1) * factor / (out_h - 1);
		uv_yscl_fac = (in_h / in_div - 1) * factor / (out_h / out_div - 1);

		ratio = y_yscl_fac * 10000 / factor;
		idx = rkvpss_get_zme_tap_coe_index(ratio);
		for (i = 0; i < 17; i++) {
			for (j = 0; j < 8; j += 2) {
				val = RKVPSS_ZME_TAP_COE(rkvpss_zme_tap6_coe[idx][i][j],
							 rkvpss_zme_tap6_coe[idx][i][j + 1]);
				rkvpss_hw_write(hw, RKVPSS_ZME_Y_VER_COE0_10 + i * 16 + j * 2, val);
				rkvpss_hw_write(hw, RKVPSS_ZME_UV_VER_COE0_10 + i * 16 + j * 2, val);
			}
		}
	} else {
		y_yscl_fac = 0;
		uv_yscl_fac = 0;
	}
	rkvpss_hw_write(hw, RKVPSS_ZME_Y_YSCL_FACTOR, y_yscl_fac);
	rkvpss_hw_write(hw, RKVPSS_ZME_UV_YSCL_FACTOR, uv_yscl_fac);

	rkvpss_hw_write(hw, RKVPSS_ZME_Y_SCL_CTRL, ctrl);
	rkvpss_hw_write(hw, RKVPSS_ZME_UV_SCL_CTRL, ctrl);

	ctrl = RKVPSS_ZME_GATING_EN;
	if (yuv420_in)
		ctrl |= RKVPSS_ZME_SCL_YUV420_REAL_EN;
	if (yuv422_to_420)
		ctrl |= RKVPSS_ZME_422TO420_EN;
	rkvpss_hw_write(hw, RKVPSS_ZME_CTRL, ctrl);
end:
	val = RKVPSS_ZME_GEN_UPD | RKVPSS_ZME_FORCE_UPD;
	rkvpss_hw_write(hw, RKVPSS_ZME_UPDATE, val);
}

static void bilinear_scale(struct rkvpss_offline_dev *ofl,
			   struct rkvpss_output_cfg *cfg, int idx)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 in_w = cfg->crop_width, in_h = cfg->crop_height;
	u32 out_w = cfg->scl_width, out_h = cfg->scl_height;
	u32 reg_base, in_div, out_div, val, ctrl = 0;
	bool yuv420_in = false, yuv422_to_420 = false;

	switch (idx) {
	case RKVPSS_OUTPUT_CH1:
		reg_base = RKVPSS_SCALE1_BASE;
		break;
	case RKVPSS_OUTPUT_CH2:
		reg_base = RKVPSS_SCALE2_BASE;
		break;
	case RKVPSS_OUTPUT_CH3:
		reg_base = RKVPSS_SCALE3_BASE;
		break;
	default:
		return;
	}

	if (in_w == out_w && in_h == out_w)
		goto end;

	/* TODO diff for input and output format */
	if (yuv420_in) {
		in_div = 2;
		out_div = 2;
	} else if (yuv422_to_420) {
		in_div = 1;
		out_div = 2;
	} else {
		in_div = 1;
		out_div = 1;
	}

	val = in_w | (in_h << 16);
	rkvpss_hw_write(hw, reg_base + 0x8, val);
	val = out_w | (out_h << 16);
	rkvpss_hw_write(hw, reg_base + 0xc, val);

	if (in_w != out_w) {
		val = (in_w - 1) * 4096 / (out_w - 1);
		rkvpss_hw_write(hw, reg_base + 0x10, val);
		val = (in_w / 2 - 1) * 4096 / (out_w / 2 - 1);
		rkvpss_hw_write(hw, reg_base + 0x14, val);

		ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
	}
	if (in_h != out_h) {
		val = (in_h - 1) * 4096 / (out_h - 1);
		rkvpss_hw_write(hw, reg_base + 0x18, val);
		val = (in_h / in_div - 1) * 4096 / (out_h / out_div - 1);
		rkvpss_hw_write(hw, reg_base + 0x1c, val);

		ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
	}

end:
	rkvpss_hw_write(hw, reg_base, ctrl);
	val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
	rkvpss_hw_write(hw, reg_base + 0x4, val);
}

static void scale_config(struct rkvpss_offline_dev *ofl,
			 struct rkvpss_output_cfg *cfg, int idx)
{
	if (idx == RKVPSS_OUTPUT_CH0)
		poly_phase_scale(ofl, cfg);
	else
		bilinear_scale(ofl, cfg, idx);
}

static void cmsc_config(struct rkvpss_offline_dev *ofl,
			struct rkvpss_frame_cfg *cfg)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	struct rkvpss_cmsc_cfg *cmsc_cfg, tmp_cfg = { 0 };
	u32 i, j, k, slope, hor, win_en, win_mode, val, ctrl = 0;
	bool is_en = false;

	if (!hw->is_ofl_cmsc)
		return;

	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		if (!cfg->output[i].enable)
			continue;
		win_en = 0;
		win_mode = 0;
		cmsc_cfg = &cfg->output[i].cmsc;
		for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
			if (!cmsc_cfg->win[j].win_en)
				continue;
			win_en |= BIT(j);
			win_mode |= cmsc_cfg->win[j].mode ? BIT(j) : 0;
			if (!cmsc_cfg->win[j].mode) {
				tmp_cfg.win[j].cover_color_y = cmsc_cfg->win[j].cover_color_y;
				tmp_cfg.win[j].cover_color_u = cmsc_cfg->win[j].cover_color_u;
				tmp_cfg.win[j].cover_color_v = cmsc_cfg->win[j].cover_color_v;
				tmp_cfg.win[j].cover_color_a = cmsc_cfg->win[j].cover_color_a;
				if (tmp_cfg.win[j].cover_color_a > 15)
					tmp_cfg.win[j].cover_color_a = 15;
			}
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++)
				tmp_cfg.win[j].point[k] = cmsc_cfg->win[j].point[k];
		}
		if (win_en)
			is_en = true;
		tmp_cfg.win[i].win_en = win_en;
		tmp_cfg.win[i].mode = win_mode;
		if (tmp_cfg.mosaic_block < cmsc_cfg->mosaic_block)
			tmp_cfg.mosaic_block = cmsc_cfg->mosaic_block;
	}

	if (!is_en) {
		rkvpss_hw_write(hw, RKVPSS_CMSC_CTRL, 0);
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN0_WIN, 0);
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN1_WIN, 0);
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN2_WIN, 0);
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN3_WIN, 0);
		goto end;
	}

	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		win_en = tmp_cfg.win[i].win_en;
		if (win_en)
			ctrl |= RKVPSS_CMSC_CHN_EN(i);
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN0_WIN + i * 4, win_en);
		win_mode = tmp_cfg.win[i].mode;
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN0_MODE + i * 4, win_mode);
		for (j = 0; j < RKVPSS_CMSC_WIN_MAX && win_en; j++) {
			if (!(win_en & BIT(j)))
				continue;
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
				val = RKVPSS_CMSC_WIN_VTX(tmp_cfg.win[j].point[k].x,
							  tmp_cfg.win[j].point[k].y);
				rkvpss_hw_write(hw, RKVPSS_CMSC_WIN0_L0_VTX + k * 8 + j * 32, val);
				rkvpss_cmsc_slop(&tmp_cfg.win[j].point[k],
						 (k + 1 == RKVPSS_CMSC_POINT_MAX) ?
						 &tmp_cfg.win[j].point[0] : &tmp_cfg.win[j].point[k + 1],
						 &slope, &hor);
				val = RKVPSS_CMSC_WIN_SLP(slope, hor);
				rkvpss_hw_write(hw, RKVPSS_CMSC_WIN0_L0_SLP + k * 8 + j * 32, val);
			}

			if ((win_mode & BIT(j)))
				continue;
			val = RKVPSS_CMSK_WIN_YUV(tmp_cfg.win[j].cover_color_y,
						  tmp_cfg.win[j].cover_color_u,
						  tmp_cfg.win[j].cover_color_v);
			val |= RKVPSS_CMSC_WIN_ALPHA(tmp_cfg.win[j].cover_color_a);
			rkvpss_hw_write(hw, RKVPSS_CMSC_WIN0_PARA + j * 4, val);
		}
	}
	ctrl |= RKVPSS_CMSC_EN;
	ctrl |= RKVPSS_CMSC_BLK_SZIE(tmp_cfg.mosaic_block);
	rkvpss_hw_write(hw, RKVPSS_CMSC_CTRL, ctrl);
end:
	val = RKVPSS_CMSC_GEN_UPD | RKVPSS_CMSC_FORCE_UPD;
	rkvpss_hw_write(hw, RKVPSS_CMSC_UPDATE, val);
}

static void aspt_config(struct rkvpss_offline_dev *ofl,
			struct rkvpss_output_cfg *cfg, int idx)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 reg_base, val;

	switch (idx) {
	case RKVPSS_OUTPUT_CH0:
		reg_base = RKVPSS_RATIO0_BASE;
		break;
	case RKVPSS_OUTPUT_CH1:
		reg_base = RKVPSS_RATIO1_BASE;
		break;
	case RKVPSS_OUTPUT_CH2:
		reg_base = RKVPSS_RATIO2_BASE;
		break;
	case RKVPSS_OUTPUT_CH3:
		reg_base = RKVPSS_RATIO3_BASE;
		break;
	default:
		return;
	}

	if (!cfg->aspt.enable) {
		rkvpss_hw_write(hw, reg_base, 0);
		goto end;
	}
	val = cfg->scl_width | (cfg->scl_height << 16);
	rkvpss_hw_write(hw, reg_base + 0x10, val);
	val = cfg->aspt.width | (cfg->aspt.height << 16);
	rkvpss_hw_write(hw, reg_base + 0x14, val);
	val = cfg->aspt.h_offs | (cfg->aspt.v_offs << 16);
	rkvpss_hw_write(hw, reg_base + 0x18, val);
	val = cfg->aspt.color_y | (cfg->aspt.color_u << 16) | (cfg->aspt.color_v << 24);
	rkvpss_hw_write(hw, reg_base + 0x1c, val);
	rkvpss_hw_write(hw, reg_base, RKVPSS_RATIO_EN);
end:
	val = RKVPSS_RATIO_FORCE_UPD | RKVPSS_RATIO_GEN_UPD;
	rkvpss_hw_write(hw, reg_base + 0x4, val);
}

static int rkvpss_ofl_run(struct file *file, struct rkvpss_frame_cfg *cfg)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *mem_ops = hw->mem_ops;
	struct sg_table  *sg_tbl;
	struct rkvpss_offline_buf *buf;
	struct rkvpss_output_ch out_ch[RKVPSS_OUTPUT_MAX] = { 0 };
	u32 in_ctrl, in_size, in_c_offs, update, mi_update;
	u32 w, h, val, reg, mask, crop_en, flip_en;
	bool ch_en = false;
	ktime_t t = 0;
	s64 us = 0;
	int ret, i;

	if (rkvpss_debug >= 2) {
		v4l2_info(&ofl->v4l2_dev,
			  "%s dev_id:%d seq:%d mirror:%d input:%dx%d buffd:%d format:%c%c%c%c\n",
			  __func__, cfg->dev_id, cfg->sequence, cfg->mirror,
			  cfg->input.width, cfg->input.height, cfg->input.buf_fd,
			  cfg->input.format, cfg->input.format >> 8,
			  cfg->input.format >> 16, cfg->input.format >> 24);
		for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
			if (!cfg->output[i].enable)
				continue;
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\tch%d crop:(%d,%d)/%dx%d scl:%dx%d flip:%d buffd:%d format:%c%c%c%c\n",
				  i, cfg->output[i].crop_h_offs, cfg->output[i].crop_v_offs,
				  cfg->output[i].crop_width, cfg->output[i].crop_height,
				  cfg->output[i].scl_width, cfg->output[i].scl_height,
				  cfg->output[i].flip, cfg->output[i].buf_fd,
				  cfg->output[i].format, cfg->output[i].format >> 8,
				  cfg->output[i].format >> 16, cfg->output[i].format >> 24);
		}
		t = ktime_get();
	}
	init_completion(&ofl->cmpl);

	in_c_offs = 0;
	in_ctrl = 0;
	switch (cfg->input.format) {
	case V4L2_PIX_FMT_NV16:
		if (cfg->input.stride < ALIGN(cfg->input.width, 16))
			cfg->input.stride = ALIGN(cfg->input.width, 16);
		in_c_offs = cfg->input.stride * cfg->input.height;
		in_size = cfg->input.stride * cfg->input.height * 2;
		in_ctrl |= RKVPSS_MI_RD_INPUT_422SP;
		break;
	case V4L2_PIX_FMT_NV12:
		if (cfg->input.stride < ALIGN(cfg->input.width, 16))
			cfg->input.stride = ALIGN(cfg->input.width, 16);
		in_c_offs = cfg->input.stride * cfg->input.height;
		in_size = cfg->input.stride * cfg->input.height * 3 / 2;
		in_ctrl |= RKVPSS_MI_RD_INPUT_420SP;
		break;
	case V4L2_PIX_FMT_RGB565:
		if (cfg->input.stride < ALIGN(cfg->input.width * 2, 16))
			cfg->input.stride = ALIGN(cfg->input.width * 2, 16);
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_BGR565;
		break;
	case V4L2_PIX_FMT_RGB24:
		if (cfg->input.stride < ALIGN(cfg->input.width * 3, 16))
			cfg->input.stride = ALIGN(cfg->input.width * 3, 16);
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_BGR888;
		break;
	case V4L2_PIX_FMT_XBGR32:
		if (cfg->input.stride < ALIGN(cfg->input.width * 4, 16))
			cfg->input.stride = ALIGN(cfg->input.width * 4, 16);
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_ABGR888 | RKVPSS_MI_RD_RB_SWAP;
		break;
	default:
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d no support input format:%c%c%c%c\n",
			 cfg->dev_id, cfg->input.format, cfg->input.format >> 8,
			 cfg->input.format >> 16, cfg->input.format >> 24);
		return -EINVAL;
	}

	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		if (!hw->is_ofl_ch[i] && cfg->output[i].enable) {
			v4l2_err(&ofl->v4l2_dev,
				 "dev_id:%d ch%d no select for offline mode, set to disable\n",
				 cfg->dev_id, i);
			cfg->output[i].enable = 0;
		}

		if (!cfg->output[i].enable)
			continue;
		ch_en = true;
		cfg->output[i].crop_h_offs = ALIGN(cfg->output[i].crop_h_offs, 2);
		cfg->output[i].crop_v_offs = ALIGN(cfg->output[i].crop_v_offs, 2);
		cfg->output[i].crop_width = ALIGN(cfg->output[i].crop_width, 2);
		cfg->output[i].crop_height = ALIGN(cfg->output[i].crop_height, 2);
		if (cfg->output[i].crop_width + cfg->output[i].crop_h_offs > cfg->input.width) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ech%d inval crop(offs:%d w:%d) input width:%d\n",
				 i, cfg->dev_id, cfg->output[i].crop_h_offs,
				 cfg->output[i].crop_width, cfg->input.width);
			cfg->output[i].crop_h_offs = 0;
			cfg->output[i].crop_width = cfg->input.width;
		}
		if (cfg->output[i].crop_height + cfg->output[i].crop_v_offs > cfg->input.height) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d inval crop(offs:%d h:%d) input height:%d\n",
				 cfg->dev_id, i, cfg->output[i].crop_v_offs,
				 cfg->output[i].crop_height, cfg->input.height);
			cfg->output[i].crop_v_offs = 0;
			cfg->output[i].crop_height = cfg->input.height;
		}

		if (i == RKVPSS_OUTPUT_CH2 || i == RKVPSS_OUTPUT_CH3) {
			if (cfg->output[i].crop_width != cfg->output[i].scl_width &&
			    cfg->output[i].scl_width > 1920) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d scale max width 1920\n",
					 cfg->dev_id, i);
				cfg->output[i].scl_width = 1920;
			}
			if (cfg->output[i].crop_height != cfg->output[i].scl_height &&
			    cfg->output[i].scl_height > 1080) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d scale max height 1080\n",
					 cfg->dev_id, i);
				cfg->output[i].scl_height = 1080;
			}
		}

		if (cfg->output[i].aspt.enable) {
			w = cfg->output[i].aspt.width;
			h = cfg->output[i].aspt.height;
		} else {
			w = cfg->output[i].scl_width;
			h = cfg->output[i].scl_height;
		}
		if (i == RKVPSS_OUTPUT_CH1) {
			bool is_fmt_find = true;

			switch (cfg->output[i].format) {
			case V4L2_PIX_FMT_RGB565:
				if (cfg->output[i].stride < ALIGN(w * 2, 16))
					cfg->output[i].stride = ALIGN(w * 3, 16);
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_RGB565 | RKVPSS_MI_CHN_WR_RB_SWAP;
				break;
			case V4L2_PIX_FMT_RGB24:
				if (cfg->output[i].stride < ALIGN(w * 3, 16))
					cfg->output[i].stride = ALIGN(w * 3, 16);
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_RGB888 | RKVPSS_MI_CHN_WR_RB_SWAP;
				break;
			case V4L2_PIX_FMT_XBGR32:
				if (cfg->output[i].stride < ALIGN(w * 4, 16))
					cfg->output[i].stride = ALIGN(w * 4, 16);
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_ARGB888;
				break;
			default:
				is_fmt_find = false;
			}
			if (is_fmt_find) {
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_EN | RKVPSS_MI_CHN_WR_AUTO_UPD;
				out_ch[i].size = cfg->output[i].stride * h;
				continue;
			}
		}
		switch (cfg->output[i].format) {
		case V4L2_PIX_FMT_UYVY:
			if (cfg->output[i].stride < ALIGN(w * 2, 16))
				cfg->output[i].stride = ALIGN(w * 2, 16);
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_422P | RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_NV16:
			if (cfg->output[i].stride < ALIGN(w, 16))
				cfg->output[i].stride = ALIGN(w, 16);
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * h * 2;
			out_ch[i].c_offs = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_NV12:
			if (cfg->output[i].stride < ALIGN(w, 16))
				cfg->output[i].stride = ALIGN(w, 16);
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV420;
			out_ch[i].size = cfg->output[i].stride * h * 3 / 2;
			out_ch[i].c_offs = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_GREY:
			if (cfg->output[i].stride < ALIGN(w, 16))
				cfg->output[i].stride = ALIGN(w, 16);
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV400;
			out_ch[i].size = cfg->output[i].stride * h;
			break;
		default:
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d no support output ch%d format:%c%c%c%c\n",
				 cfg->dev_id, i,
				 cfg->output[i].format, cfg->output[i].format >> 8,
				 cfg->output[i].format >> 16, cfg->output[i].format >> 24);
			return -EINVAL;
		}
		out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_EN | RKVPSS_MI_CHN_WR_AUTO_UPD;
	}
	if (!ch_en) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d no output channel enable\n", cfg->dev_id);
		return -EINVAL;
	}

	buf = buf_add(file, cfg->dev_id, cfg->input.buf_fd, in_size);
	if (!buf)
		goto err;

	sg_tbl = mem_ops->cookie(&buf->vb, buf->mem);
	val = sg_dma_address(sg_tbl->sgl);
	rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_BASE, val);
	val += in_c_offs;
	rkvpss_hw_write(hw, RKVPSS_MI_RD_C_BASE, val);

	val = cfg->input.width;
	rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_WIDTH, val);
	val = cfg->input.height;
	rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_HEIGHT, val);
	val = cfg->input.stride;
	rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_STRIDE, val);

	mask = RKVPSS_MI_RD_GROUP_MODE(3) | RKVPSS_MI_RD_BURST16_LEN;
	rkvpss_hw_set_bits(hw, RKVPSS_MI_RD_CTRL, ~mask, in_ctrl);
	rkvpss_hw_write(hw, RKVPSS_MI_RD_INIT, RKVPSS_MI_RD_FORCE_UPD);

	cmsc_config(ofl, cfg);

	mi_update = 0;
	crop_en = 0;
	flip_en = 0;
	mask = 0;
	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		if (hw->is_ofl_ch[i])
			mask |= RKVPSS_MI_CHN_V_FLIP(i);
		if (!cfg->output[i].enable)
			continue;
		buf = buf_add(file, cfg->dev_id, cfg->output[i].buf_fd, out_ch[i].size);
		if (!buf)
			goto free_buf;

		reg = RKVPSS_CROP0_0_H_OFFS;
		val = cfg->output[i].crop_h_offs;
		rkvpss_hw_write(hw, reg + i * 0x10, val);
		reg = RKVPSS_CROP0_0_V_OFFS;
		val = cfg->output[i].crop_v_offs;
		rkvpss_hw_write(hw, reg + i * 0x10, val);
		reg = RKVPSS_CROP0_0_H_SIZE;
		val = cfg->output[i].crop_width;
		rkvpss_hw_write(hw, reg + i * 0x10, val);
		reg = RKVPSS_CROP0_0_V_SIZE;
		val = cfg->output[i].crop_height;
		rkvpss_hw_write(hw, reg + i * 0x10, val);
		crop_en |= RKVPSS_CROP_CHN_EN(i);

		scale_config(ofl, &cfg->output[i], i);
		aspt_config(ofl, &cfg->output[i], i);

		if (cfg->output[i].aspt.enable)
			h = cfg->output[i].aspt.height;
		else
			h = cfg->output[i].scl_height;

		sg_tbl = mem_ops->cookie(&buf->vb, buf->mem);
		val = sg_dma_address(sg_tbl->sgl);
		reg = RKVPSS_MI_CHN0_WR_Y_BASE;
		rkvpss_hw_write(hw, reg + i * 0x100, val);
		reg = RKVPSS_MI_CHN0_WR_CB_BASE;
		val += out_ch[i].c_offs;
		rkvpss_hw_write(hw, reg + i * 0x100, val);

		reg = RKVPSS_MI_CHN0_WR_Y_STRIDE;
		val = cfg->output[i].stride;
		rkvpss_hw_write(hw, reg + i * 0x100, val);
		reg = RKVPSS_MI_CHN0_WR_Y_SIZE;
		val = cfg->output[i].stride * h;
		rkvpss_hw_write(hw, reg + i * 0x100, val);
		reg = RKVPSS_MI_CHN0_WR_CB_SIZE;
		val = out_ch[i].size - val;
		rkvpss_hw_write(hw, reg + i * 0x100, val);

		reg = RKVPSS_MI_CHN0_WR_CTRL;
		val = out_ch[i].ctrl;
		rkvpss_hw_write(hw, reg + i * 0x100, val);

		if (cfg->output[i].flip)
			flip_en |= RKVPSS_MI_CHN_V_FLIP(i);
		mi_update |= (RKVPSS_MI_CHN0_FORCE_UPD << i);
	}
	rkvpss_hw_write(hw, RKVPSS_CROP0_CTRL, crop_en);
	rkvpss_hw_write(hw, RKVPSS_CROP0_UPDATE, RKVPSS_CROP_FORCE_UPD);
	if (flip_en)
		rkvpss_hw_set_bits(hw, RKVPSS_MI_WR_VFLIP_CTRL, mask, flip_en);
	rkvpss_hw_write(hw, RKVPSS_MI_WR_INIT, mi_update);

	mask = 0;
	val = 0;
	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		if (!hw->is_ofl_ch[i])
			continue;
		mask |= (RKVPSS_ISP2VPSS_CHN0_SEL(3) << i * 2);
		if (hw->is_ofl_cmsc)
			mask |= RKVPSS_ISP2VPSS_ONLINE2_CMSC_EN;
		if (cfg->output[i].enable)
			val |= (RKVPSS_ISP2VPSS_CHN0_SEL(1) << i * 2);
	}
	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_ONLINE, mask, val);

	update = 0;
	mask = hw->is_ofl_cmsc ? RKVPSS_MIR_EN : 0;
	val = (mask && cfg->mirror) ? RKVPSS_MIR_EN : 0;
	if (mask) {
		rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, mask, val);
		update |= RKVPSS_MIR_FORCE_UPD;
	}
	update |= RKVPSS_CHN_FORCE_UPD | RKVPSS_CFG_GEN_UPD | RKVPSS_MIR_GEN_UPD;
	rkvpss_hw_write(hw, RKVPSS_VPSS_UPDATE, update);

	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_IMSC, 0, RKVPSS_ALL_FRM_END);

	rkvpss_hw_write(hw, RKVPSS_MI_RD_START, RKVPSS_MI_RD_ST);

	ret = wait_for_completion_timeout(&ofl->cmpl, msecs_to_jiffies(500));
	if (!ret) {
		v4l2_err(&ofl->v4l2_dev, "working timeout\n");
		ret = -EAGAIN;
	} else {
		ret = 0;
	}
	if (rkvpss_debug >= 2) {
		us = ktime_us_delta(ktime_get(), t);
		v4l2_info(&ofl->v4l2_dev,
			  "%s end, time:%lldus\n", __func__, us);
	}
	return ret;
free_buf:
	for (i -= 1; i >= 0; i--) {
		if (!cfg->output[i].enable)
			continue;
		buf_del(file, cfg->dev_id, cfg->output[i].buf_fd, false, true);
	}
	buf_del(file, cfg->dev_id, cfg->input.buf_fd, false, true);
err:
	return -ENOMEM;
}

static int rkvpss_module_sel(struct file *file,
			     struct rkvpss_module_sel *sel)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	struct rkvpss_hw_dev *hw = ofl->hw;
	struct rkvpss_device *vpss;
	int i, ret = 0;

	mutex_lock(&hw->dev_lock);
	for (i = 0; i < hw->dev_num; i++) {
		vpss = hw->vpss[i];
		if (vpss && (vpss->vpss_sdev.state & VPSS_START)) {
			v4l2_err(&ofl->v4l2_dev, "no support set mode when vpss working\n");
			ret = -EINVAL;
			goto unlock;
		}
	}

	hw->is_ofl_cmsc = !!sel->mirror_cmsc_en;
	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++)
		hw->is_ofl_ch[i] = !!sel->ch_en[i];
unlock:
	mutex_unlock(&hw->dev_lock);
	return ret;
}

static long rkvpss_ofl_ioctl(struct file *file, void *fh,
			     bool valid_prio, unsigned int cmd, void *arg)
{
	long ret = 0;

	if (!arg)
		return -EINVAL;

	switch (cmd) {
	case RKVPSS_CMD_MODULE_SEL:
		ret = rkvpss_module_sel(file, arg);
		break;
	case RKVPSS_CMD_FRAME_HANDLE:
		ret = rkvpss_ofl_run(file, arg);
		break;
	case RKVPSS_CMD_BUF_ADD:
		ret = rkvpss_ofl_buf_add(file, arg);
		break;
	case RKVPSS_CMD_BUF_DEL:
		rkvpss_ofl_buf_del(file, arg);
		break;
	default:
		ret = -EFAULT;
	}

	return ret;
}

static const struct v4l2_ioctl_ops offline_ioctl_ops = {
	.vidioc_default = rkvpss_ofl_ioctl,
};

static int ofl_open(struct file *file)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	int ret;

	ret = v4l2_fh_open(file);
	if (ret)
		goto end;

	mutex_lock(&ofl->hw->dev_lock);
	ret = pm_runtime_get_sync(ofl->hw->dev);
	mutex_unlock(&ofl->hw->dev_lock);
	if (ret < 0)
		v4l2_fh_release(file);
end:
	v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
		 "%s file:%p ret:%d\n", __func__, file, ret);
	return (ret > 0) ? 0 : ret;
}

static int ofl_release(struct file *file)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);

	v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
		 "%s file:%p\n", __func__, file);

	v4l2_fh_release(file);
	buf_del(file, 0, 0, true, false);
	mutex_lock(&ofl->hw->dev_lock);
	pm_runtime_put_sync(ofl->hw->dev);
	mutex_unlock(&ofl->hw->dev_lock);
	return 0;
}

static const struct v4l2_file_operations offline_fops = {
	.owner = THIS_MODULE,
	.open = ofl_open,
	.release = ofl_release,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static const struct video_device offline_videodev = {
	.name = "rkvpss-offline",
	.vfl_dir = VFL_DIR_RX,
	.fops = &offline_fops,
	.ioctl_ops = &offline_ioctl_ops,
	.minor = -1,
	.release = video_device_release_empty,
};

void rkvpss_offline_irq(struct rkvpss_hw_dev *hw, u32 irq)
{
	struct rkvpss_offline_dev *ofl = &hw->ofl_dev;

	v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s 0x%x\n", __func__, irq);

	if (!completion_done(&ofl->cmpl))
		complete(&ofl->cmpl);
}

int rkvpss_register_offline(struct rkvpss_hw_dev *hw)
{
	struct rkvpss_offline_dev *ofl = &hw->ofl_dev;
	struct v4l2_device *v4l2_dev;
	struct video_device *vfd;
	int ret;

	ofl->hw = hw;
	v4l2_dev = &ofl->v4l2_dev;
	strscpy(v4l2_dev->name, offline_videodev.name, sizeof(v4l2_dev->name));
	ret = v4l2_device_register(hw->dev, v4l2_dev);
	if (ret)
		return ret;

	mutex_init(&ofl->apilock);
	ofl->vfd = offline_videodev;
	vfd = &ofl->vfd;
	vfd->device_caps = V4L2_CAP_STREAMING;
	vfd->lock = &ofl->apilock;
	vfd->v4l2_dev = v4l2_dev;
	ret = video_register_device(vfd, VFL_TYPE_VIDEO, 0);
	if (ret) {
		v4l2_err(v4l2_dev, "Failed to register video device\n");
		goto unreg_v4l2;
	}
	video_set_drvdata(vfd, ofl);
	INIT_LIST_HEAD(&ofl->list);
	return 0;
unreg_v4l2:
	mutex_destroy(&ofl->apilock);
	v4l2_device_unregister(v4l2_dev);
	return ret;
}

void rkvpss_unregister_offline(struct rkvpss_hw_dev *hw)
{
	mutex_destroy(&hw->ofl_dev.apilock);
	video_unregister_device(&hw->ofl_dev.vfd);
	v4l2_device_unregister(&hw->ofl_dev.v4l2_dev);
}
