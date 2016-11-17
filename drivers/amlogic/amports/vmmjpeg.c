/*
 * drivers/amlogic/amports/vmjpeg.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>

#include "vdec_reg.h"
#include "arch/register.h"
#include "amports_priv.h"

#include <linux/amlogic/codec_mm/codec_mm.h>

#include "vdec_input.h"
#include "vdec.h"
#include "amvdec.h"

#define MEM_NAME "codec_mmjpeg"

#include "amvdec.h"

#define DRIVER_NAME "ammvdec_mjpeg"
#define MODULE_NAME "ammvdec_mjpeg"

/* protocol register usage
    AV_SCRATCH_4 : decode buffer spec
    AV_SCRATCH_5 : decode buffer index
*/

#define MREG_DECODE_PARAM   AV_SCRATCH_2	/* bit 0-3: pico_addr_mode */
/* bit 15-4: reference height */
#define MREG_TO_AMRISC      AV_SCRATCH_8
#define MREG_FROM_AMRISC    AV_SCRATCH_9
#define MREG_FRAME_OFFSET   AV_SCRATCH_A

#define PICINFO_BUF_IDX_MASK        0x0007
#define PICINFO_AVI1                0x0080
#define PICINFO_INTERLACE           0x0020
#define PICINFO_INTERLACE_AVI1_BOT  0x0010
#define PICINFO_INTERLACE_FIRST     0x0010

#define VF_POOL_SIZE          16
#define DECODE_BUFFER_NUM_MAX 4

static struct vframe_s *vmjpeg_vf_peek(void *);
static struct vframe_s *vmjpeg_vf_get(void *);
static void vmjpeg_vf_put(struct vframe_s *, void *);
static int vmjpeg_vf_states(struct vframe_states *states, void *);
static int vmjpeg_event_cb(int type, void *data, void *private_data);
static void vmjpeg_work(struct work_struct *work);

static const char vmjpeg_dec_id[] = "vmmjpeg-dev";

#define PROVIDER_NAME   "vdec.mjpeg"
static const struct vframe_operations_s vf_provider_ops = {
	.peek = vmjpeg_vf_peek,
	.get = vmjpeg_vf_get,
	.put = vmjpeg_vf_put,
	.event_cb = vmjpeg_event_cb,
	.vf_states = vmjpeg_vf_states,
};

#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2

struct buffer_spec_s {
	unsigned int y_addr;
	unsigned int u_addr;
	unsigned int v_addr;

	int y_canvas_index;
	int u_canvas_index;
	int v_canvas_index;

	struct canvas_config_s canvas_config[3];
	unsigned long cma_alloc_addr;
	int cma_alloc_count;
	unsigned int buf_adr;
};

#define spec2canvas(x)  \
	(((x)->v_canvas_index << 16) | \
	 ((x)->u_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

struct vdec_mjpeg_hw_s {
	spinlock_t lock;
	struct mutex vmjpeg_mutex;

	struct platform_device *platform_dev;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);

	struct vframe_s vfpool[VF_POOL_SIZE];
	struct buffer_spec_s buffer_spec[DECODE_BUFFER_NUM_MAX];
	s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];

	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 saved_resolution;

	u32 stat;
	u32 dec_result;
	unsigned long buf_start;
	u32 buf_size;

	struct dec_sysinfo vmjpeg_amstream_dec_info;

	struct vframe_chunk_s *chunk;
	struct work_struct work;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
};

static void set_frame_info(struct vdec_mjpeg_hw_s *hw, struct vframe_s *vf)
{
	vf->width = hw->frame_width;
	vf->height = hw->frame_height;
	vf->duration = hw->frame_dur;
	vf->ratio_control = 0;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 3;

	vf->canvas0_config[0] = hw->buffer_spec[vf->index].canvas_config[0];
	vf->canvas0_config[1] = hw->buffer_spec[vf->index].canvas_config[1];
	vf->canvas0_config[2] = hw->buffer_spec[vf->index].canvas_config[2];

	vf->canvas1_config[0] = hw->buffer_spec[vf->index].canvas_config[0];
	vf->canvas1_config[1] = hw->buffer_spec[vf->index].canvas_config[1];
	vf->canvas1_config[2] = hw->buffer_spec[vf->index].canvas_config[2];
}

static irqreturn_t vmjpeg_isr(struct vdec_s *vdec)
{
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)(vdec->private);
	u32 reg;
	struct vframe_s *vf = NULL;
	u32 index;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	if (!hw)
		return IRQ_HANDLED;

	reg = READ_VREG(MREG_FROM_AMRISC);
	index = READ_VREG(AV_SCRATCH_5);

	if (index >= DECODE_BUFFER_NUM_MAX) {
		pr_err("fatal error, invalid buffer index.");
		return IRQ_HANDLED;
	}

	if (kfifo_get(&hw->newframe_q, &vf) == 0) {
		pr_info(
		"fatal error, no available buffer slot.");
		return IRQ_HANDLED;
	}

	vf->index = index;
	set_frame_info(hw, vf);

	vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
	/* vf->pts = (pts_valid) ? pts : 0; */
	/* vf->pts_us64 = (pts_valid) ? pts_us64 : 0; */
	vf->pts = hw->chunk->pts;
	vf->pts_us64 = hw->chunk->pts64;
	vf->orientation = 0;
	hw->vfbuf_use[index]++;

	kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

	vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY,
			NULL);

	hw->dec_result = DEC_RESULT_DONE;

	schedule_work(&hw->work);

	return IRQ_HANDLED;
}

static struct vframe_s *vmjpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmjpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (kfifo_get(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static void vmjpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	hw->vfbuf_use[vf->index]--;
	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
}

static int vmjpeg_event_cb(int type, void *data, void *private_data)
{
	return 0;
}

static int vmjpeg_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	spin_lock_irqsave(&hw->lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);
	states->buf_recycle_num = 0;

	spin_unlock_irqrestore(&hw->lock, flags);

	return 0;
}

static int vmjpeg_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;
	vstatus->width = hw->frame_width;
	vstatus->height = hw->frame_height;
	if (0 != hw->frame_dur)
		vstatus->fps = 96000 / hw->frame_dur;
	else
		vstatus->fps = 96000;
	vstatus->error_count = 0;
	vstatus->status = hw->stat;

	return 0;
}

/****************************************/
static void vmjpeg_canvas_init(struct vdec_s *vdec)
{
	int i;
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;
	ulong addr;

	canvas_width = 1920;
	canvas_height = 1088;
	decbuf_y_size = 0x200000;
	decbuf_uv_size = 0x80000;
	decbuf_size = 0x300000;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		int canvas;

		canvas = vdec->get_canvas(i, 3);
		if (hw->buffer_spec[i].cma_alloc_count == 0) {
			hw->buffer_spec[i].cma_alloc_count =
				PAGE_ALIGN(decbuf_size) / PAGE_SIZE;
			hw->buffer_spec[i].cma_alloc_addr =
				codec_mm_alloc_for_dma(MEM_NAME,
					hw->buffer_spec[i].cma_alloc_count,
					16, CODEC_MM_FLAGS_FOR_VDECODER);
		}

		if (!hw->buffer_spec[i].cma_alloc_addr) {
			pr_err("CMA alloc failed, request buf size 0x%lx\n",
				hw->buffer_spec[i].cma_alloc_count * PAGE_SIZE);
			hw->buffer_spec[i].cma_alloc_count = 0;
			break;
		}

		hw->buffer_spec[i].buf_adr =
			hw->buffer_spec[i].cma_alloc_addr;
		addr = hw->buffer_spec[i].buf_adr;

		hw->buffer_spec[i].y_addr = addr;
		addr += decbuf_y_size;
		hw->buffer_spec[i].u_addr = addr;
		addr += decbuf_uv_size;
		hw->buffer_spec[i].v_addr = addr;

		hw->buffer_spec[i].y_canvas_index = canvas_y(canvas);
		hw->buffer_spec[i].u_canvas_index = canvas_u(canvas);
		hw->buffer_spec[i].v_canvas_index = canvas_v(canvas);

		canvas_config(hw->buffer_spec[i].y_canvas_index,
			hw->buffer_spec[i].y_addr,
			canvas_width,
			canvas_height,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
		hw->buffer_spec[i].canvas_config[0].phy_addr =
			hw->buffer_spec[i].y_addr;
		hw->buffer_spec[i].canvas_config[0].width =
			canvas_width;
		hw->buffer_spec[i].canvas_config[0].height =
			canvas_height;
		hw->buffer_spec[i].canvas_config[0].block_mode =
			CANVAS_BLKMODE_LINEAR;

		canvas_config(hw->buffer_spec[i].u_canvas_index,
			hw->buffer_spec[i].u_addr,
			canvas_width / 2,
			canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
		hw->buffer_spec[i].canvas_config[1].phy_addr =
			hw->buffer_spec[i].u_addr;
		hw->buffer_spec[i].canvas_config[1].width =
			canvas_width / 2;
		hw->buffer_spec[i].canvas_config[1].height =
			canvas_height / 2;
		hw->buffer_spec[i].canvas_config[1].block_mode =
			CANVAS_BLKMODE_LINEAR;

		canvas_config(hw->buffer_spec[i].v_canvas_index,
			hw->buffer_spec[i].v_addr,
			canvas_width / 2,
			canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
		hw->buffer_spec[i].canvas_config[2].phy_addr =
			hw->buffer_spec[i].v_addr;
		hw->buffer_spec[i].canvas_config[2].width =
			canvas_width / 2;
		hw->buffer_spec[i].canvas_config[2].height =
			canvas_height / 2;
		hw->buffer_spec[i].canvas_config[2].block_mode =
			CANVAS_BLKMODE_LINEAR;
	}
}

static void init_scaler(void)
{
	/* 4 point triangle */
	const unsigned filt_coef[] = {
		0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
		0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
		0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
		0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
		0x18382808, 0x18382808, 0x17372909, 0x17372909,
		0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
		0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
		0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
		0x10303010
	};
	int i;

	/* pscale enable, PSCALE cbus bmem enable */
	WRITE_VREG(PSCALE_CTRL, 0xc000);

	/* write filter coefs */
	WRITE_VREG(PSCALE_BMEM_ADDR, 0);
	for (i = 0; i < 33; i++) {
		WRITE_VREG(PSCALE_BMEM_DAT, 0);
		WRITE_VREG(PSCALE_BMEM_DAT, filt_coef[i]);
	}

	/* Y horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 37 * 2);
	/* [35]: buf repeat pix0,
	 * [34:29] => buf receive num,
	 * [28:16] => buf blk x,
	 * [15:0] => buf phase
	 */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* C horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 41 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 39 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* C vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 43 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 36 * 2 + 1);
	/* [19:0] => Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);
	/* C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 40 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);

	/* Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 38 * 2 + 1);
	/* [19:0] => Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);
	/* C vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 42 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);

	/* reset pscaler */
#if 1/*MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6*/
	WRITE_VREG(DOS_SW_RESET0, (1 << 10));
	WRITE_VREG(DOS_SW_RESET0, 0);
#else
	WRITE_MPEG_REG(RESET2_REGISTER, RESET_PSCALE);
#endif
	READ_MPEG_REG(RESET2_REGISTER);
	READ_MPEG_REG(RESET2_REGISTER);
	READ_MPEG_REG(RESET2_REGISTER);

	WRITE_VREG(PSCALE_RST, 0x7);
	WRITE_VREG(PSCALE_RST, 0x0);
}

static void vmjpeg_hw_ctx_restore(struct vdec_s *vdec, int index)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6));
	WRITE_VREG(DOS_SW_RESET0, 0);

	vmjpeg_canvas_init(vdec);

	/* find next decode buffer index */
	WRITE_VREG(AV_SCRATCH_4, spec2canvas(&hw->buffer_spec[index]));
	WRITE_VREG(AV_SCRATCH_5, index);

	init_scaler();

	/* clear buffer IN/OUT registers */
	WRITE_VREG(MREG_TO_AMRISC, 0);
	WRITE_VREG(MREG_FROM_AMRISC, 0);

	WRITE_VREG(MCPU_INTR_MSK, 0xffff);
	WRITE_VREG(MREG_DECODE_PARAM, (hw->frame_height << 4) | 0x8000);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
	/* set interrupt mapping for vld */
	WRITE_VREG(ASSIST_AMR1_INT8, 8);
#if 1/*MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6*/
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
}

static s32 vmjpeg_init(struct vdec_s *vdec)
{
	int i;
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;

	hw->frame_width = hw->vmjpeg_amstream_dec_info.width;
	hw->frame_height = hw->vmjpeg_amstream_dec_info.height;
	hw->frame_dur = hw->vmjpeg_amstream_dec_info.rate;
	hw->saved_resolution = 0;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		hw->vfbuf_use[i] = 0;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];
		hw->vfpool[i].index = -1;
		kfifo_put(&hw->newframe_q, vf);
	}

	INIT_WORK(&hw->work, vmjpeg_work);

	return 0;
}

static bool run_ready(struct vdec_s *vdec)
{
	return true;
}

static void run(struct vdec_s *vdec,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;
	int i, r;

	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->vfbuf_use[i] == 0)
			break;
	}

	if (i == DECODE_BUFFER_NUM_MAX) {
		hw->dec_result = DEC_RESULT_AGAIN;
		schedule_work(&hw->work);
		return;
	}

	r = vdec_prepare_input(vdec, &hw->chunk);
	if (r < 0) {
		hw->dec_result = DEC_RESULT_AGAIN;
		schedule_work(&hw->work);
		return;
	}

	hw->dec_result = DEC_RESULT_NONE;

	if (amvdec_vdec_loadmc_ex(vdec, "vmmjpeg_mc") < 0) {
		pr_err("%s: Error amvdec_loadmc fail\n", __func__);
		return;
	}

	vmjpeg_hw_ctx_restore(vdec, i);

	vdec_enable_input(vdec);

	amvdec_start();
}

static void vmjpeg_work(struct work_struct *work)
{
	struct vdec_mjpeg_hw_s *hw = container_of(work,
		struct vdec_mjpeg_hw_s, work);

	if (hw->dec_result == DEC_RESULT_DONE)
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);

	/* mark itself has all HW resource released and input released */
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_CONNECTED);

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
}

static int amvdec_mjpeg_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_mjpeg_hw_s *hw = NULL;

	if (pdata == NULL) {
		pr_info("amvdec_mjpeg memory resource undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_mjpeg_hw_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct vdec_mjpeg_hw_s), GFP_KERNEL);
	if (hw == NULL) {
		pr_info("\nammvdec_mjpeg device data allocation failed\n");
		return -ENOMEM;
	}

	pdata->private = hw;
	pdata->dec_status = vmjpeg_dec_status;

	pdata->run = run;
	pdata->run_ready = run_ready;
	pdata->irq_handler = vmjpeg_isr;

	pdata->id = pdev->id;

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_provider_ops, pdata);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;
	hw->buf_start = pdata->mem_start;
	hw->buf_size = pdata->mem_end - pdata->mem_start + 1;

	if (pdata->sys_info)
		hw->vmjpeg_amstream_dec_info = *pdata->sys_info;

	if (vmjpeg_init(pdata) < 0) {
		pr_info("amvdec_mjpeg init failed.\n");
		return -ENODEV;
	}

	return 0;
}

static int amvdec_mjpeg_remove(struct platform_device *pdev)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	int i;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->buffer_spec[i].cma_alloc_addr) {
			pr_info("codec_mm release buffer_spec[%d], 0x%lx\n", i,
				hw->buffer_spec[i].cma_alloc_addr);
			codec_mm_free_for_dma(MEM_NAME,
				hw->buffer_spec[i].cma_alloc_addr);
			hw->buffer_spec[i].cma_alloc_count = 0;
		}
	}

	cancel_work_sync(&hw->work);

	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);

	return 0;
}

/****************************************/

static struct platform_driver amvdec_mjpeg_driver = {
	.probe = amvdec_mjpeg_probe,
	.remove = amvdec_mjpeg_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_mjpeg_profile = {
	.name = "mmjpeg",
	.profile = ""
};

static int __init amvdec_mjpeg_driver_init_module(void)
{
	if (platform_driver_register(&amvdec_mjpeg_driver)) {
		pr_err("failed to register amvdec_mjpeg driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_mjpeg_profile);
	return 0;
}

static void __exit amvdec_mjpeg_driver_remove_module(void)
{
	platform_driver_unregister(&amvdec_mjpeg_driver);
}

/****************************************/

module_init(amvdec_mjpeg_driver_init_module);
module_exit(amvdec_mjpeg_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MJMPEG Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
