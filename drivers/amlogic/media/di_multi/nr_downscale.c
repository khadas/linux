// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/nr_downscale.c
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/ppmgr/tbff.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "register.h"
#include "nr_downscale.h"
#include "di_data_l.h"
#include "di_sys.h"
#include "di_api.h"
#include "di_dbg.h"
#include "di_prc.h"
#include "di_reg_v3.h"
#include "tb_task.h"

#ifdef DIM_TB_DETECT
struct tb_core_s {
	unsigned int di_tb_quit_flag	: 1,
			first_frame	: 1,
			reset_tb	: 1,
			cur_invert	: 2,
			cur_invert_a	: 2,
			cur_invert_b	: 2,
			cur_invert_c	: 2,
			nr_dump_en	: 1,
			rev	: 20;
	u8 dim_tb_detect_last_flag;
	unsigned int di_tb_buff_wptr;
	unsigned int di_tb_buff_rptr;
	s8 di_tb_buffer_status;
	unsigned int di_tb_buffer_start;
	void	*virt;
	unsigned int di_tb_buffer_size;
	unsigned int di_tb_buffer_len;
	u8 di_tb_first_frame_type;
	int first_frame_type;
	unsigned int skip_picture;
	u8 last_type;
	unsigned int last_width;
	unsigned int last_height;
	unsigned int di_tb_init_mute;
	unsigned int init_mute;
	atomic_t di_tb_sem;
	atomic_t dim_detect_status;
	atomic_t dim_tb_detect_flag;
	atomic_t di_tb_reset_flag;
	atomic_t di_tb_skip_flag;
	atomic_t di_tb_run_flag;
	struct dim_tb_buf_s di_detect_buf[DIM_TB_BUFFER_MAX_SIZE];
	struct dim_tb_buf_s di_detect_wbuf[DIM_TB_BUFFER_MAX_SIZE];
	unsigned int nr_dump_count;
	int di_init;
	unsigned char flg_from;
};

//static struct tb_core_s dim_tbs[DI_CHANNEL_NUB];
static struct tb_core_s *dim_tbs[DI_CHANNEL_NUB];

//struct tb_core_s *get_dim_tb(void)
//{
//	return dim_tbs;
//}

static DEFINE_MUTEX(di_tb_mutex);

static s32 di_tb_canvas = -1;
static struct TB_DetectFuncPtr *digfunc;
static struct tbff_stats *di_tb_reg[DI_CHANNEL_NUB];

//static u32 di_tb_src_canvas;
#endif

#define NR_DETECT_DUMP_CNT 150

#ifdef DIM_TB_DETECT
static int diwrite_to_file(unsigned long  dump_adr, int size, unsigned int ch)
{
	int ret = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	bool bflg_vmap = false;
	void *buff = NULL;
	loff_t pos = 0;
	int nwrite = 0;
	int offset = 0;
	char parm[64];
	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	//PR_ERR("11%s: open file ok\n", __func__);
	/* open file to write */
	//fp = filp_open("/storage/A2DC-E6BA/tb.yuv",
		//O_WRONLY | O_CREAT | O_APPEND, 0777);
	sprintf(parm, "/storage/A2DC-E6BA/tb%d.yuv", ch);
	fp = filp_open(parm, O_WRONLY | O_CREAT | O_APPEND, 0777);
	//sprintf(parm, "/storage/A2DC-E6BA/TEST4/yuv00%d", didump_tb_cont++);
	//fp = filp_open(parm, O_WRONLY | O_CREAT, 0666);
	//PR_ERR("22%s: open file ok\n", __func__);
	if (IS_ERR_OR_NULL(fp)) {
		PR_ERR("%s: open file error\n", __func__);
		ret = -1;
		goto exit;
	}
	buff = dim_vmap(dump_adr, size, &bflg_vmap);
	//PR_ERR("%s:vmap:0x%x\n", __func__, (unsigned int *)buff);

	pos = (unsigned long)offset;
	//buff = codec_mm_vmap(dump_adr, size);
	//PR_ERR("%s:vmap:0x%p\n", __func__, buff);
	/* Write buf to file */
	if (IS_ERR_OR_NULL(buff))
		PR_ERR("%s: ioremap error.\n", __func__);

	nwrite = vfs_write(fp, buff, size, &pos);
	offset += nwrite;
	vfs_fsync(fp, 0);
	if (bflg_vmap)
		dim_unmap_phyaddr(buff);

	filp_close(fp, NULL);

	//PR_ERR("33%s: open file ok\n", __func__);
exit:
	set_fs(old_fs);
	return ret;
}

int dim_tb_detect(struct vframe_tb_s *vf, int data1, unsigned int ch)
{
	bool is_top;
	const struct reg_acc *op = &di_pre_regset;
	struct tb_core_s *pcfg;
	struct di_dev_s *di_devp = get_dim_de_devp();

	dbg_tb("%s :step 3 s\n", __func__);

	pcfg = dim_tbs[ch];

	if (!di_devp->tb_flag_int)
		return -1;

	if (unlikely(!vf))
		return -1;

	if (pcfg->di_tb_buff_wptr & 1)
		is_top = false;
	else
		is_top = true;

	if (data1 == 0) {
		if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM) {
			pcfg->di_detect_buf[0].paddr =
				pcfg->di_detect_wbuf[1].paddr;
			pcfg->di_detect_buf[1].paddr =
				pcfg->di_detect_wbuf[0].paddr;
			pcfg->di_detect_buf[2].paddr =
				pcfg->di_detect_wbuf[3].paddr;
			pcfg->di_detect_buf[3].paddr =
				pcfg->di_detect_wbuf[2].paddr;
			pcfg->di_detect_buf[4].paddr =
				pcfg->di_detect_wbuf[5].paddr;
			pcfg->di_detect_buf[5].paddr =
				pcfg->di_detect_wbuf[4].paddr;
			pcfg->di_detect_buf[6].paddr =
				pcfg->di_detect_wbuf[7].paddr;
			pcfg->di_detect_buf[7].paddr =
				pcfg->di_detect_wbuf[6].paddr;
			pcfg->cur_invert_a = 1;//dump
			pcfg->cur_invert_b = 1;//revert
			pcfg->cur_invert_c = 1;//normal
			dbg_tb("%s:NR01A %x\n", __func__, data1);
		} else	{
			pcfg->di_detect_buf[0].paddr =
				pcfg->di_detect_wbuf[0].paddr;
			pcfg->di_detect_buf[1].paddr =
				pcfg->di_detect_wbuf[1].paddr;
			pcfg->di_detect_buf[2].paddr =
				pcfg->di_detect_wbuf[2].paddr;
			pcfg->di_detect_buf[3].paddr =
				pcfg->di_detect_wbuf[3].paddr;
			pcfg->di_detect_buf[4].paddr =
				pcfg->di_detect_wbuf[4].paddr;
			pcfg->di_detect_buf[5].paddr =
				pcfg->di_detect_wbuf[5].paddr;
			pcfg->di_detect_buf[6].paddr =
				pcfg->di_detect_wbuf[6].paddr;
			pcfg->di_detect_buf[7].paddr =
				pcfg->di_detect_wbuf[7].paddr;
			pcfg->cur_invert_a = 0;
			pcfg->cur_invert_b = 2;
			pcfg->cur_invert_c = 2;
			dbg_tb("%s:NR01B %x\n", __func__, data1);
		}
	}
	if (pcfg->cur_invert  && pcfg->cur_invert_b) {
		if (pcfg->cur_invert_b == 1) {
			pcfg->di_detect_buf[0].paddr =
				pcfg->di_detect_wbuf[0].paddr;
			pcfg->di_detect_buf[1].paddr =
				pcfg->di_detect_wbuf[1].paddr;
			pcfg->di_detect_buf[2].paddr =
				pcfg->di_detect_wbuf[2].paddr;
			pcfg->di_detect_buf[3].paddr =
				pcfg->di_detect_wbuf[3].paddr;
			pcfg->di_detect_buf[4].paddr =
				pcfg->di_detect_wbuf[4].paddr;
			pcfg->di_detect_buf[5].paddr =
				pcfg->di_detect_wbuf[5].paddr;
			pcfg->di_detect_buf[6].paddr =
				pcfg->di_detect_wbuf[6].paddr;
			pcfg->di_detect_buf[7].paddr =
				pcfg->di_detect_wbuf[7].paddr;
			pcfg->cur_invert_a = 0;
			pcfg->cur_invert_c = 1;
			dbg_tb("%s:NR01C %x\n", __func__, data1);

		} else {
			pcfg->di_detect_buf[0].paddr =
				pcfg->di_detect_wbuf[1].paddr;
			pcfg->di_detect_buf[1].paddr =
				pcfg->di_detect_wbuf[0].paddr;
			pcfg->di_detect_buf[2].paddr =
				pcfg->di_detect_wbuf[3].paddr;
			pcfg->di_detect_buf[3].paddr =
				pcfg->di_detect_wbuf[2].paddr;
			pcfg->di_detect_buf[4].paddr =
				pcfg->di_detect_wbuf[5].paddr;
			pcfg->di_detect_buf[5].paddr =
				pcfg->di_detect_wbuf[4].paddr;
			pcfg->di_detect_buf[6].paddr =
				pcfg->di_detect_wbuf[7].paddr;
			pcfg->di_detect_buf[7].paddr =
				pcfg->di_detect_wbuf[6].paddr;
			pcfg->cur_invert_a = 1;
			pcfg->cur_invert_c = 2;
			dbg_tb("%s:NR01D %x\n", __func__, data1);
		}
		pcfg->cur_invert_b = 0;
	}
	if (!pcfg->cur_invert  && !pcfg->cur_invert_b && pcfg->cur_invert_c) {
		if (pcfg->cur_invert_c == 1) {
			pcfg->di_detect_buf[0].paddr =
				pcfg->di_detect_wbuf[1].paddr;
			pcfg->di_detect_buf[1].paddr =
				pcfg->di_detect_wbuf[0].paddr;
			pcfg->di_detect_buf[2].paddr =
				pcfg->di_detect_wbuf[3].paddr;
			pcfg->di_detect_buf[3].paddr =
				pcfg->di_detect_wbuf[2].paddr;
			pcfg->di_detect_buf[4].paddr =
				pcfg->di_detect_wbuf[5].paddr;
			pcfg->di_detect_buf[5].paddr =
				pcfg->di_detect_wbuf[4].paddr;
			pcfg->di_detect_buf[6].paddr =
				pcfg->di_detect_wbuf[7].paddr;
			pcfg->di_detect_buf[7].paddr =
				pcfg->di_detect_wbuf[6].paddr;
			pcfg->cur_invert_a = 1;
			pcfg->cur_invert_b = 1;
			dbg_tb("%s:NR01E %x\n", __func__, data1);
		} else {
			pcfg->di_detect_buf[0].paddr =
				pcfg->di_detect_wbuf[0].paddr;
			pcfg->di_detect_buf[1].paddr =
				pcfg->di_detect_wbuf[1].paddr;
			pcfg->di_detect_buf[2].paddr =
				pcfg->di_detect_wbuf[2].paddr;
			pcfg->di_detect_buf[3].paddr =
				pcfg->di_detect_wbuf[3].paddr;
			pcfg->di_detect_buf[4].paddr =
				pcfg->di_detect_wbuf[4].paddr;
			pcfg->di_detect_buf[5].paddr =
				pcfg->di_detect_wbuf[5].paddr;
			pcfg->di_detect_buf[6].paddr =
				pcfg->di_detect_wbuf[6].paddr;
			pcfg->di_detect_buf[7].paddr =
				pcfg->di_detect_wbuf[7].paddr;
			pcfg->cur_invert_a = 0;
			pcfg->cur_invert_b = 2;
			dbg_tb("%s:NR01F %x\n", __func__, data1);
		}
			pcfg->cur_invert_c = 0;
	}

	canvas_config(di_tb_canvas,
		      pcfg->di_detect_buf[pcfg->di_tb_buff_wptr].paddr,
		      DIM_TB_DETECT_W, DIM_TB_DETECT_H,
		      CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);
	op->bwr(NRDSWR_CTRL, di_tb_canvas, 0, 8);
	if (DIM_IS_IC_EF(T7))
		di_mif1_linear_wr_cfgds
			(pcfg->di_detect_buf[pcfg->di_tb_buff_wptr].paddr,
			 NRDSWR_STRIDE, NRDSWR_BADDR);
	//dim_nr_ds_hw_ctrl(true);
	dbg_tb("%s:e:ch[%d]dump %x frames start\n", __func__, ch,
		pcfg->di_tb_buff_wptr);
	return 1;
}

void dim_ds_detect(struct vframe_tb_s *vf, int data1, unsigned int ch)
{
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct tb_core_s *pcfg;
	struct di_ch_s *pch;
	int ret = 0;

	pch = get_chdata(ch);

	if (!di_devp->tb_flag_int)
		return;
	if (!pch->en_tb) {
		//dim_nr_ds_hw_ctrl(false);
		return;
	}
	dbg_tb("%s :step 2 s\n", __func__);
	pcfg = dim_tbs[ch];

	if (vf->source_type !=
		VFRAME_SOURCE_TYPE_OTHERS)
		return;
	if ((vf->width * vf->height)
		> (1920 * 1088)) {
		// greater than (1920 * 1088), do not detect
		return;
	}

	if (pcfg->di_tb_buff_wptr < pcfg->di_tb_buffer_len) {
		ret = dim_tb_detect(vf, data1, ch);
		dbg_tb("%s:TB tb detect00,data1=%X\n", __func__, data1);
	} else {
		dbg_tb("%s :TB tb detect skip case2\n", __func__);
		atomic_set(&pcfg->di_tb_skip_flag, 1);
		pcfg->skip_picture++;
	}
	if (ret > 0) {
		pcfg->di_tb_buff_wptr++;
		if (pcfg->di_tb_buff_wptr >= DIM_TB_ALGO_COUNT &&
		    (atomic_read(&pcfg->dim_detect_status)
			== di_tb_idle)) {
			atomic_set
				(&pcfg->dim_detect_status,
				di_tb_running);
			dbg_tb("%s:TB tb detect01=%d\n", __func__,
			       pcfg->di_tb_buff_wptr);
		}
		if (pcfg->di_tb_buff_wptr >= DIM_TB_ALGO_COUNT) {
			atomic_set(&pcfg->di_tb_sem, 1);
			dbg_tb("%s:TB tb detect02=%d\n", __func__,
			       pcfg->di_tb_buff_wptr);
		}
	}
}

void dim_tb_function(struct vframe_tb_s *vf, int data1, unsigned int ch)
{
	int ret = 0;
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct tb_core_s *pcfg;
	struct di_ch_s *pch;
	//const struct reg_acc *op = &di_pre_regset;

	pch = get_chdata(ch);

	if (!di_devp->tb_flag_int)
		return;
	if (!pch->en_tb) {
		//dim_nr_ds_hw_ctrl(false);
		return;
	}
	//dbg_tb("%s :s\n", __func__);
	pcfg = dim_tbs[ch];

	if (vf->source_type !=
		VFRAME_SOURCE_TYPE_OTHERS)
		goto SKIP_DETECT;
	if ((vf->width * vf->height)
		> (1920 * 1088)) {
		// greater than (1920 * 1088), do not detect
		goto SKIP_DETECT;
	}

	dbg_tb("%s:step 1 ch[%d]type=%x,cur_invert=%x,%x,%x,%x\n", __func__,
		ch, vf->type, pcfg->cur_invert, vf->width, vf->height, data1);

	//op->bwr(NR_DS_CTRL, vf->width / NR_DS_WIDTH, 16, 6);
	//op->bwr(NR_DS_CTRL, vf->height / 2 / NR_DS_HEIGHT, 24, 6);

	//if (data1 == 0)
		//pcfg->first_frame = 1;
	if (pcfg->first_frame) {
		pcfg->last_type = vf->type & VIDTYPE_TYPEMASK;
		pcfg->last_width = vf->width;
		pcfg->last_height = vf->height;
		pcfg->first_frame_type = pcfg->last_type;
		pcfg->di_tb_first_frame_type = pcfg->last_type;
		pcfg->first_frame = 0;
		pcfg->reset_tb = 0;
		pcfg->skip_picture = 0;
		pcfg->cur_invert = 0;
		pcfg->init_mute = pcfg->di_tb_init_mute;
		atomic_set(&pcfg->di_tb_skip_flag, 1);
		atomic_set(&pcfg->di_tb_reset_flag, 0);
		dbg_tb("%s:first f type:%x,first_frame_type %X\n", __func__,
		       pcfg->last_type, pcfg->first_frame_type);
	} else if ((pcfg->last_type ==
		   (vf->type & VIDTYPE_TYPEMASK)) && pcfg->last_type) {
		/* interlace seq changed */
		pcfg->first_frame_type =
			vf->type & VIDTYPE_TYPEMASK;
		pcfg->di_tb_first_frame_type =
			pcfg->first_frame_type;
		pcfg->reset_tb = 1;
		data1 = 0;
		/* keep old invert */
		dbg_tb("%s:DI:TB interlace seq chg,old:%d,new:%d,invert: %d\n",
		__func__,
		pcfg->last_type,
		pcfg->first_frame_type,
		pcfg->cur_invert);
	} else if ((pcfg->last_type == 0) &&
		((vf->type & VIDTYPE_TYPEMASK) != 0)) {
		/* prog -> interlace changed */
		pcfg->first_frame_type =
			vf->type & VIDTYPE_TYPEMASK;
		pcfg->di_tb_first_frame_type =
			pcfg->first_frame_type;
		pcfg->reset_tb = 1;
		dbg_tb("%s:TB prog -> interlace, new type: %d, invert: %d\n",
		       __func__, pcfg->first_frame_type, pcfg->cur_invert);
		/* not invert */
		pcfg->cur_invert = 0;
	} else if (((pcfg->last_width != vf->width) ||
		   (pcfg->last_height != vf->height)) &&
		   ((vf->type & VIDTYPE_TYPEMASK) != 0)) {
		/* size changed and next seq is interlace */
		pcfg->first_frame_type =
			vf->type & VIDTYPE_TYPEMASK;
		pcfg->di_tb_first_frame_type =
			pcfg->first_frame_type;
		pcfg->reset_tb = 1;
		/* keep old invert */
		dbg_tb("%s:TB tb size change new type: %d, invert: %d\n",
		__func__, pcfg->first_frame_type, pcfg->cur_invert);
	} else if ((pcfg->last_type != 0) &&
		((vf->type & VIDTYPE_TYPEMASK) == 0)) {
		/* interlace -> prog changed */
		dbg_tb("%s:TB tb interlace -> prog, invert: %d\n",
		       __func__, pcfg->cur_invert);
		/* not invert */
		pcfg->cur_invert = 0;
	}
	pcfg->last_type = vf->type & VIDTYPE_TYPEMASK;
	pcfg->last_width = vf->width;
	pcfg->last_height = vf->height;
	dbg_tb("%s:S frame type: %x,first_frame_type %X,cur_invert=%x\n", __func__,
		pcfg->last_type, pcfg->first_frame_type, pcfg->cur_invert);
	if (di_devp->tb_flag_int) {
		ret = 0;
		if (pcfg->di_tb_buffer_status <= 0)
			goto SKIP_DETECT;
		di_devp->tb_detect_buf_len = pcfg->di_tb_buffer_len;
		vf->type = (vf->type & ~TB_DETECT_MASK);
		if (pcfg->init_mute > 0) {
			pcfg->init_mute--;
			atomic_set(&pcfg->di_tb_skip_flag, 1);
			goto SKIP_DETECT;
		}
		if (pcfg->last_type == 0) {/* cur type is prog */
			pcfg->skip_picture++;
			pcfg->cur_invert = 0;
			goto SKIP_DETECT;
		}
		vf->type |=
			pcfg->cur_invert <<
			TB_DETECT_MASK_BIT;
		dbg_tb("%s: M first f type:%x,first_frame_type %X\n", __func__,
		       pcfg->last_type, pcfg->first_frame_type);

		if (pcfg->reset_tb) {
			/* wait tb task done */
			while ((pcfg->di_tb_buff_wptr >= DIM_TB_ALGO_COUNT) &&
				(pcfg->di_tb_buff_rptr <=
				 pcfg->di_tb_buff_wptr - DIM_TB_ALGO_COUNT) &&
				(atomic_read(&pcfg->di_tb_run_flag) == 1))
				usleep_range(100, 200);
			//usleep_range(4000, 5000);
			atomic_set(&pcfg->dim_detect_status, di_tb_idle);
			pcfg->di_tb_buff_wptr = 0;
			pcfg->di_tb_buff_rptr = 0;
			atomic_set(&pcfg->dim_tb_detect_flag, TB_DETECT_NC);
			atomic_set(&pcfg->di_tb_reset_flag, 1);
			atomic_set(&pcfg->di_tb_skip_flag, 1);
			pcfg->skip_picture = 0;
			pcfg->reset_tb  = 0;
			dbg_tb("%s reset once\n", __func__);
		}
		if ((atomic_read(&pcfg->dim_detect_status) == di_tb_done) &&
		    pcfg->skip_picture >=
		     di_devp->tb_detect_period &&
		    pcfg->last_type == pcfg->first_frame_type) {
			int tbf_flag =
				atomic_read(&pcfg->dim_tb_detect_flag);
			u8 old_invert = pcfg->cur_invert;

			atomic_set(&pcfg->dim_detect_status, di_tb_idle);
			pcfg->di_tb_buff_wptr = 0;
			pcfg->di_tb_buff_rptr = 0;
			pcfg->skip_picture = 0;
			if (tbf_flag == TB_DETECT_TBF &&
			    pcfg->first_frame_type == VIDTYPE_INTERLACE_TOP) {
				/* TBF sams as BFF */
				vf->type |=
					TB_DETECT_INVERT
					<< TB_DETECT_MASK_BIT;
				pcfg->cur_invert = 1;
			} else if ((tbf_flag == TB_DETECT_BFF) &&
				   (pcfg->first_frame_type ==
				    VIDTYPE_INTERLACE_TOP)) {
				vf->type |=
					TB_DETECT_INVERT
					<< TB_DETECT_MASK_BIT;
				pcfg->cur_invert = 1;
			} else if ((tbf_flag == TB_DETECT_TFF) &&
				   (pcfg->first_frame_type ==
				    VIDTYPE_INTERLACE_BOTTOM)) {
				vf->type |=
					TB_DETECT_INVERT
					<< TB_DETECT_MASK_BIT;
				pcfg->cur_invert = 1;
			} else if (tbf_flag != TB_DETECT_NC) {
				pcfg->cur_invert = 0;
			}
			vf->type = (vf->type & ~TB_DETECT_MASK);
			vf->type |=
				pcfg->cur_invert <<
				TB_DETECT_MASK_BIT;
			dbg_tb("%s process mask:vf[0x%x] type=0x%x\n",
			       __func__, vf->omx_index, vf->type);
			if (old_invert != pcfg->cur_invert)
				dbg_tb
				("%s:flag00: %d->%d, invert: %d->%d\n",
				__func__,
				pcfg->dim_tb_detect_last_flag,
				tbf_flag,
				old_invert,
				pcfg->cur_invert);
			else if (pcfg->dim_tb_detect_last_flag != tbf_flag)
				dbg_tb
				("%s:flag01:  %d->%d, invert: %d\n",
				__func__,
				pcfg->dim_tb_detect_last_flag,
				tbf_flag,
				pcfg->cur_invert);
			pcfg->dim_tb_detect_last_flag = tbf_flag;
			atomic_set(&pcfg->dim_tb_detect_flag, TB_DETECT_NC);
		}

		if (pcfg->di_tb_buff_wptr == 0 &&
		    pcfg->last_type != pcfg->first_frame_type) {
			pcfg->skip_picture++;
			atomic_set(&pcfg->di_tb_skip_flag, 1);
			dbg_tb("%s:TB tb detect skip case1\n", __func__);
			goto SKIP_DETECT;
		}
	} else {
		pcfg->reset_tb = 1;
		pcfg->skip_picture++;
		pcfg->cur_invert = 0;
		if (pcfg->init_mute > 0)
			pcfg->init_mute--;
		dbg_tb("%s :step 1 else\n", __func__);
	}
	dbg_tb("%s :step 1 e\n", __func__);

SKIP_DETECT:
	if (pcfg->skip_picture >
		di_devp->tb_detect_period) {
		pcfg->skip_picture =
			di_devp->tb_detect_period;
		dbg_tb("%s :skip\n", __func__);
	}
}

int dim_tb_buffer_init(unsigned int ch)
{
	int i;
	//int flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_CMA_CLEAR;
	//int flags = 0;
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct tb_core_s *pcfg;
	struct dim_mm_s omm;
	//struct div2_mm_s *mm;
	bool ret;
	struct di_ch_s *pch;

	PR_INF("%s :s\n", __func__);

	if (!di_devp->tb_flag_int)
		return -1;

	pch = get_chdata(ch);
	if (!pch->en_tb)
		return -1;

	pcfg = dim_tbs[ch];

	di_tb_reg[ch] = kmalloc(sizeof(*di_tb_reg[ch]), GFP_KERNEL);
	if (!di_tb_reg[ch])
		return -1;

	memset(di_tb_reg[ch], 0, sizeof(struct tbff_stats));

	if (pcfg->di_tb_buffer_status)
		return pcfg->di_tb_buffer_status;

	if (di_tb_canvas < 0)
		di_tb_canvas =
			canvas_pool_map_alloc_canvas("tb_detect_dst");

	if (di_tb_canvas < 0)
		return -1;

	if (pcfg->di_tb_buffer_start == 0 && pch->en_tb) {
		if (!di_devp->tb_detect_buf_len)
			di_devp->tb_detect_buf_len = 8;
		pcfg->di_tb_buffer_len = di_devp->tb_detect_buf_len;
		pcfg->di_tb_buffer_size = DIM_TB_DETECT_H * DIM_TB_DETECT_W
			* pcfg->di_tb_buffer_len;
		pcfg->di_tb_buffer_size = PAGE_ALIGN(pcfg->di_tb_buffer_size);
		ret = mm_cma_alloc(NULL,
				   pcfg->di_tb_buffer_size >> PAGE_SHIFT,
				   &omm);
		pcfg->flg_from = 1;
		if (!ret) {
			pcfg->flg_from = 2;
			ret = mm_cma_alloc(&di_devp->pdev->dev,
				   pcfg->di_tb_buffer_size >> PAGE_SHIFT,
				   &omm);
			if (!ret) {
				PR_ERR("%s:cma\n", __func__);
				pcfg->flg_from = 0;
				return -1;
			}
		}
		pcfg->di_tb_buffer_start = omm.addr;
		pcfg->virt = omm.ppage;

		dbg_tb("%s:ch[%d] %x, size %x, item %d\n", __func__, ch,
			(unsigned int)pcfg->di_tb_buffer_start,
			(unsigned int)pcfg->di_tb_buffer_size,
			pcfg->di_tb_buffer_len);
		if (pcfg->di_tb_buffer_start == 0) {
			PR_ERR("di:err:DI:TB cma memory config fail\n");
			pcfg->di_tb_buffer_status = -1;
			return -1;
		}
		for (i = 0; i < pcfg->di_tb_buffer_len; i++) {
			pcfg->di_detect_buf[i].paddr =
				pcfg->di_tb_buffer_start +
				DIM_TB_DETECT_H * DIM_TB_DETECT_W * i;
			pcfg->di_detect_buf[i].vaddr =
				(ulong)codec_mm_vmap
				(pcfg->di_detect_buf[i].paddr,
				DIM_TB_DETECT_H * DIM_TB_DETECT_W);
			pcfg->di_detect_wbuf[i].paddr =
				pcfg->di_detect_buf[i].paddr;
			dbg_tb("%s:TBbuf(%d)paddr:%lx,vaddr: %lx,vaddr: %lx\n",
			       __func__, i, pcfg->di_detect_buf[i].paddr,
			       pcfg->di_detect_buf[i].vaddr,
			       pcfg->di_detect_wbuf[i].paddr);
		}
	}
	pcfg->di_tb_buffer_status = 1;
	dbg_tb("%s :e\n", __func__);
	return 1;
}

int dim_tb_buffer_uninit(unsigned int ch)
{
	int i;
	struct tb_core_s *pcfg;
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct di_ch_s *pch;

	dbg_tb("%s :s\n", __func__);

	pch = get_chdata(ch);
	pcfg = dim_tbs[ch];

	if (!di_devp->tb_flag_int)
		return 0;
	if (!pch->en_tb)
		return 0;
	if (pcfg->di_tb_buffer_status <= 0)
		return 0;

	if (di_tb_canvas >= 0)
		canvas_pool_map_free_canvas(di_tb_canvas);
	di_tb_canvas = -1;
	if (pcfg->di_tb_buffer_start) {
		dbg_tb("%s:ch[%d] free addr is %x, size is %x\n", __func__, ch,
			(unsigned int)pcfg->di_tb_buffer_start,
			(unsigned int)pcfg->di_tb_buffer_size);
		for (i = 0; i < pcfg->di_tb_buffer_len; i++) {
			if (pcfg->di_detect_buf[i].vaddr) {
				codec_mm_unmap_phyaddr
					((u8 *)pcfg->di_detect_buf[i].vaddr);
				pcfg->di_detect_buf[i].vaddr = 0;
			}
		}
		if (pcfg->flg_from == 2)
			dma_release_from_contiguous
				(&di_devp->pdev->dev,
				 (struct page *)pcfg->virt,
				 pcfg->di_tb_buffer_size >> PAGE_SHIFT);
		else
			dma_release_from_contiguous
				(NULL,
				 (struct page *)pcfg->virt,
				 pcfg->di_tb_buffer_size >> PAGE_SHIFT);
		pcfg->di_tb_buffer_start = 0;
		pcfg->di_tb_buffer_size = 0;
		pcfg->virt = NULL;
	}
	pcfg->di_tb_buffer_status = 0;

	kfree(di_tb_reg[ch]);
	di_tb_reg[ch] = NULL;
	//di_devp->tb_detect = 0x8;
	dbg_tb("%s :e\n", __func__);
	return 0;
}

void dim_tb_reg_init(struct vframe_tb_s *vfm, unsigned int on, unsigned int ch)
{
	int val = 0;
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct tb_core_s *pcfg;
	struct di_ch_s *pch;

	dbg_tb("%s :s\n", __func__);

	pch = get_chdata(ch);

	if (vfm->source_type !=
		VFRAME_SOURCE_TYPE_OTHERS)
		return;
	if ((vfm->width * vfm->height) > (1920 * 1088))
		return;
	if (!di_devp->tb_flag_int)
		return;
	if (!pch->en_tb)
		return;
	if (!(vfm->type & VIDTYPE_TYPEMASK))
		return;

	pcfg = dim_tbs[ch];

	if (on) {
		atomic_set(&pcfg->di_tb_sem, val);
		memset(pcfg->di_detect_buf, 0, sizeof(pcfg->di_detect_buf));
		memset(pcfg->di_detect_wbuf, 0, sizeof(pcfg->di_detect_wbuf));
		atomic_set(&pcfg->dim_detect_status, di_tb_idle);
		atomic_set(&pcfg->dim_tb_detect_flag, TB_DETECT_NC);
		atomic_set(&pcfg->di_tb_reset_flag, 0);
		atomic_set(&pcfg->di_tb_skip_flag, 0);
		atomic_set(&pcfg->di_tb_run_flag, 1);
		//di_devp->tb_detect = 0x8;
		di_devp->tb_detect_period = 0;
		di_devp->tb_detect_buf_len = 8;
		di_devp->tb_detect_init_mute = 0;

		pcfg->dim_tb_detect_last_flag = TB_DETECT_NC;
		pcfg->di_tb_buff_wptr = 0;
		pcfg->di_tb_buff_rptr = 0;
		pcfg->di_tb_buffer_status = 0;
		pcfg->di_tb_buffer_start = 0;
		pcfg->di_tb_buffer_size = 0;
		pcfg->di_tb_first_frame_type = 0;
		pcfg->di_tb_quit_flag = 0;
		pcfg->di_tb_init_mute = di_devp->tb_detect_init_mute;
		pcfg->cur_invert_b = 0;
		pcfg->cur_invert_a = 0;
		pcfg->first_frame = 1;
		pcfg->di_tb_buffer_len = DIM_TB_BUFFER_MAX_SIZE;
		pcfg->nr_dump_en = 0;
		pcfg->nr_dump_count = 0;
		pcfg->skip_picture = 0;
		pcfg->cur_invert = 0;
		dbg_tb("%s :ch[%d] TB init\n", __func__, ch);
	}
	dbg_tb("%s :e\n", __func__);
}

bool dim_tb_t_try_alloc(struct di_ch_s *pch)
{
	if (pch->tb_busy)
		return false;

	pch->tb_busy = true;
	pch->tb_owner = pch->ch_id;
	dbg_tb("%s :a:ch[%d]:alloc:ok\n", __func__, pch->ch_id);
	return true;
}

void dim_tb_polling_active(struct di_ch_s *pch)
{
	if (pch->en_tb_buf && pch->en_tb) {
		if (dim_tb_t_try_alloc(pch))
			pch->en_tb = true;
	}
}

void dim_tb_t_release(struct di_ch_s *pch)
{
	struct di_dev_s *di_devp = get_dim_de_devp();

	dbg_tb("%s :s\n", __func__);

	if (!di_devp->tb_flag_int)
		return;

	if (!pch->en_tb)
		return;

	if (pch->en_tb) {
		if (pch->ch_id == pch->tb_owner) {
			pch->tb_owner = 0xff;
			pch->tb_busy = false;
		} else {
			PR_ERR("%s:%d->%d\n",
			       __func__, pch->ch_id, pch->tb_owner);
		}
		pch->en_tb = false;
	}

	if (pch->en_tb_buf) {
		get_datal()->tb_src_cnt--;
		pch->en_tb_buf = false;
	}
	kfree(dim_tbs[pch->ch_id]);

	dbg_tb("%s :e:r:ch[%d]:cnt[%d]\n", __func__,
		pch->ch_id,
		get_datal()->tb_src_cnt);
}

void dim_tb_alloc(struct vframe_tb_s *vf, struct di_ch_s *pch)
{
	unsigned char tb_cnt, tb_nub;
	struct di_dev_s *di_devp = get_dim_de_devp();

	dbg_tb("%s 0:s = %d\n", __func__, pch->ch_id);

	if (!di_devp->tb_flag_int)
		return;

	pch->en_tb	= false;
	pch->en_tb_buf	= false;

	if (vf->source_type !=
		VFRAME_SOURCE_TYPE_OTHERS)
		return;
	if ((vf->width * vf->height) > (1920 * 1088))
		return;
	if (!(vf->type & VIDTYPE_TYPEMASK))
		return;

	tb_nub = cfgg(TB);
	if (!tb_nub)
		return;

	tb_cnt = get_datal()->tb_src_cnt;
	tb_cnt++;
	if (tb_cnt > tb_nub)
		return;

	if (!dim_tb_t_try_alloc(pch))
		return;

	pch->en_tb = true;
	pch->en_tb_buf = true;
	get_datal()->tb_src_cnt = tb_cnt;
	dbg_tb("%s s = %d\n", __func__, get_datal()->tb_src_cnt);

	dim_tbs[pch->ch_id] = kmalloc(sizeof(*dim_tbs[pch->ch_id]), GFP_KERNEL);
	if (!dim_tbs[pch->ch_id])
		return;
	//memset(dim_tbs[pch->ch_id], 0, sizeof(struct tb_core_s));

	dbg_tb("%s :e:ch[%d]:cnt[%d]\n", __func__,
		pch->ch_id,
		get_datal()->tb_src_cnt);
}

int dim_tb_task_process(struct vframe_tb_s *vf, u32 data, unsigned int ch)
{
	int tbff_flag = 0;
	ulong y5fld[5] = {0};
	int is_top = 0;
	int i;
	int di_inter_flag = 0;
	struct di_dev_s *di_devp = get_dim_de_devp();
	static const char * const detect_type[] = {"NC", "TFF", "BFF", "TBF"};
	struct tb_core_s *pcfg;
	struct di_ch_s *pch;

	dbg_tb("%s :step 4 s\n", __func__);

	if (!di_devp->tb_flag_int)
		return 1;
	if (vf->source_type !=
		VFRAME_SOURCE_TYPE_OTHERS)
		return 1;
	if ((vf->width * vf->height) > (1920 * 1088))
		return 1;

	pch = get_chdata(ch);
	if (!pch->en_tb)
		return 1;

	if (digfunc)
		digfunc->stats_init(di_tb_reg[ch],
				    DIM_TB_DETECT_H, DIM_TB_DETECT_W);

	pcfg = dim_tbs[ch];

	if (atomic_read(&pcfg->di_tb_sem) != 0) {
		if (pcfg->di_tb_quit_flag)
			return 1;

		if (pcfg->di_tb_buff_rptr == 0) {
			if (atomic_read(&pcfg->di_tb_reset_flag) != 0)
				pcfg->di_init = 0;
			atomic_set(&pcfg->di_tb_reset_flag, 0);
			if (digfunc)
				digfunc->fwalg_init(pcfg->di_init, ch);
		}
		pcfg->di_init = 1;
		is_top = (pcfg->di_tb_buff_rptr & 1) ? 0 : 1;

	if (pcfg->di_tb_buff_rptr == 0) {
		codec_mm_dma_flush
		((void *)pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 5].vaddr,
		DIM_TB_DETECT_W * DIM_TB_DETECT_H,
		DMA_FROM_DEVICE);
		codec_mm_dma_flush
		((void *)pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 4].vaddr,
		DIM_TB_DETECT_W * DIM_TB_DETECT_H,
		DMA_FROM_DEVICE);
		codec_mm_dma_flush
		((void *)pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 3].vaddr,
		DIM_TB_DETECT_W * DIM_TB_DETECT_H,
		DMA_FROM_DEVICE);
		codec_mm_dma_flush
		((void *)pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 2].vaddr,
		DIM_TB_DETECT_W * DIM_TB_DETECT_H,
		DMA_FROM_DEVICE);
		codec_mm_dma_flush
		((void *)pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 1].vaddr,
		DIM_TB_DETECT_W * DIM_TB_DETECT_H,
		DMA_FROM_DEVICE);
		codec_mm_dma_flush
		((void *)pcfg->di_detect_buf[pcfg->di_tb_buff_rptr].vaddr,
		DIM_TB_DETECT_W * DIM_TB_DETECT_H,
		DMA_FROM_DEVICE);
		if (data >= dimp_get(edi_mp_tb_dump) &&
		    dimp_get(edi_mp_tb_dump)) {
			pcfg->nr_dump_en = 1;
			if (pcfg->cur_invert_a == 1) {
				dim_dump_tb(1, ch);
				dim_dump_tb(0, ch);
				dim_dump_tb(3, ch);
				dim_dump_tb(2, ch);
			} else {
				dim_dump_tb(0, ch);
				dim_dump_tb(1, ch);
				dim_dump_tb(2, ch);
				dim_dump_tb(3, ch);
			}
		}
	} else if (pcfg->di_tb_buff_rptr == 1) {
		codec_mm_dma_flush((void *)pcfg->di_detect_buf[6].vaddr,
			DIM_TB_DETECT_W * DIM_TB_DETECT_H,
			DMA_FROM_DEVICE);
		if (pcfg->nr_dump_en) {
			if (pcfg->cur_invert_a == 1) {
				dim_dump_tb(5, ch);
				dim_dump_tb(4, ch);
			} else {
				dim_dump_tb(4, ch);
				dim_dump_tb(5, ch);
			}
		}

	} else if (pcfg->di_tb_buff_rptr == 2) {
		codec_mm_dma_flush((void *)pcfg->di_detect_buf[7].vaddr,
			DIM_TB_DETECT_W * DIM_TB_DETECT_H,
			DMA_FROM_DEVICE);
		if (pcfg->nr_dump_en) {
			if (pcfg->cur_invert_a == 1) {
				dim_dump_tb(7, ch);
				dim_dump_tb(6, ch);
			} else {
				dim_dump_tb(6, ch);
				dim_dump_tb(7, ch);
			}
		}

	} else if (pcfg->di_tb_buff_rptr == 3) {
		codec_mm_dma_flush((void *)pcfg->di_detect_buf[7].vaddr,
			DIM_TB_DETECT_W * DIM_TB_DETECT_H,
			DMA_FROM_DEVICE);
		if (pcfg->nr_dump_en) {
			dim_dump_tb(6, ch);
			dim_dump_tb(7, ch);
		}
	}
		/* new -> old */
		y5fld[0] = pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 4].vaddr;
		y5fld[1] = pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 3].vaddr;
		y5fld[2] = pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 2].vaddr;
		y5fld[3] = pcfg->di_detect_buf[pcfg->di_tb_buff_rptr + 1].vaddr;
		y5fld[4] = pcfg->di_detect_buf[pcfg->di_tb_buff_rptr].vaddr;

		if (digfunc) {
			if (IS_ERR_OR_NULL(di_tb_reg[ch])) {
				kfree(di_tb_reg[ch]);
				PR_ERR("di:err:DI:TB di_tb_reg is NULL!\n");
				return 1;
			}
			for (i = 0; i < 5; i++) {
				if (IS_ERR_OR_NULL((void *)y5fld[i])) {
					PR_ERR("di:err:DI:TB y5fld[%d]isNULL\n",
					       i);
					di_inter_flag = 1;
					break;
				}
			}
			if (di_inter_flag) {
				di_inter_flag = 0;
				return 1;
			}
			digfunc->stats_get(y5fld, di_tb_reg[ch]);
		}
		is_top = is_top ^ 1;
		tbff_flag = -1;
		if (digfunc)
			tbff_flag = digfunc->fwalg_get(di_tb_reg[ch], is_top,
				(pcfg->di_tb_first_frame_type == 3) ? 0 : 1,
				pcfg->di_tb_buff_rptr,
				atomic_read(&pcfg->di_tb_skip_flag),
				(dimp_get(edi_mp_nrds_en)) ? 1 : 0, ch);
		dbg_tb("%s TB=%x,=%x,=%x,=%x,=%x,data=%x,ch=%x\n", __func__,
			is_top,
			pcfg->di_tb_first_frame_type, pcfg->di_tb_buff_rptr,
			atomic_read(&pcfg->di_tb_skip_flag),
			tbff_flag, data, ch);
		if (pcfg->di_tb_buff_rptr == 0)
			atomic_set(&pcfg->di_tb_skip_flag, 0);

		if ((tbff_flag < -1) || tbff_flag > 2) {
			PR_ERR("di:err:DI:TB get tb detect flag error: %d\n",
			       tbff_flag);
		}

		if ((tbff_flag == -1) && (digfunc))
			tbff_flag =
				digfunc->majority_get(ch);

		if (tbff_flag == -1)
			tbff_flag =
				TB_DETECT_NC;
		else if (tbff_flag == 0)
			tbff_flag =
				TB_DETECT_TFF;
		else if (tbff_flag == 1)
			tbff_flag =
				TB_DETECT_BFF;
		else if (tbff_flag == 2)
			tbff_flag =
				TB_DETECT_TBF;
		else
			tbff_flag =
				TB_DETECT_NC;

		//tbff_flag =
			//TB_DETECT_NC;//mark for dis di tb

		pcfg->di_tb_buff_rptr++;
		dbg_tb("%s:task %d,%d\n", __func__,
		       pcfg->di_tb_buff_rptr, pcfg->di_tb_buffer_len);

		if ((pcfg->di_tb_buff_rptr >
		     pcfg->di_tb_buffer_len - DIM_TB_ALGO_COUNT) &&
		    (atomic_read(&pcfg->dim_detect_status) == di_tb_running)) {
			atomic_set(&pcfg->dim_tb_detect_flag, tbff_flag);
			dbg_tb("%s:TB get tb detect final flag: %s\n",
			       __func__, detect_type[tbff_flag]);
			atomic_set(&pcfg->dim_detect_status, di_tb_done);
		}
		atomic_set(&pcfg->di_tb_sem, 0);
	}
	atomic_set(&pcfg->di_tb_run_flag, 0);
	dbg_tb("%s :e\n", __func__);
	return 0;
}

void dim_tb_ext_cmd(struct vframe_s *vf, int data1, unsigned int ch,
		unsigned int cmd)
{
	struct tb_task_cmd_s tb_blk_cmd;
	struct vframe_tb_s cfg_data;
	struct vframe_tb_s *cfg = &cfg_data;

	memset(cfg, 0, sizeof(struct vframe_tb_s));
	if (!IS_ERR_OR_NULL(vf)) {
		cfg->height = vf->height;
		cfg->width = vf->width;
		cfg->source_type = vf->source_type;
		cfg->type = vf->type;
	}

	dbg_tb("%s :S\n", __func__);
	if (cmd == ECMD_TB_REG)
		dim_tb_alloc(cfg, get_chdata(ch));
	tb_blk_cmd.cmd = cmd;
	tb_blk_cmd.field_count = data1;
	tb_blk_cmd.ch = ch;
	tb_blk_cmd.in_buf_vf = vf;
	tb_task_alloc_block(ch, &tb_blk_cmd);
	if (cmd == ECMD_TB_PROC)
		dim_ds_detect(cfg, data1, ch);
}
#endif

#ifdef DIM_TB_DETECT
int RegisterTB_Function(struct TB_DetectFuncPtr *func, const char *ver)
{
	int ret = -1;

	mutex_lock(&di_tb_mutex);
	pr_info("DI:TB RegDITB digfunc %p, func: %p, ver:%s\n", digfunc,
		func, ver);
	if (!digfunc && func) {
		digfunc = func;
		ret = 0;
	}
	mutex_unlock(&di_tb_mutex);

	return ret;
}
EXPORT_SYMBOL(RegisterTB_Function);

int UnRegisterTB_Function(struct TB_DetectFuncPtr *func)
{
	int ret = -1;

	mutex_lock(&di_tb_mutex);
	pr_info("DI:TB UnRegDITB: digfunc %p, func: %p\n", digfunc, func);
	if (func && func == digfunc) {
		digfunc = NULL;
		ret = 0;
	}
	mutex_unlock(&di_tb_mutex);

	return ret;
}
EXPORT_SYMBOL(UnRegisterTB_Function);

void dim_nr_ds_hw_init(unsigned int width, unsigned int height, unsigned int ch)
{
	unsigned char h_step = 0, v_step = 0;
	unsigned int width_out, height_out;
	const struct reg_acc *op = &di_pre_regset;
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct di_ch_s *pch;

	dbg_tb("%s :s\n", __func__);

	if (!di_devp->tb_flag_int)
		return;

	pch = get_chdata(ch);
	if (!pch->en_tb)
		return;

	width_out = NR_DS_WIDTH;
	height_out = NR_DS_HEIGHT;

	h_step = width / width_out;
	v_step = height / height_out;

	/*Switch MIF to NR_DS*/
	op->bwr(VIUB_MISC_CTRL0, 3, 5, 2);
	/* config dsbuf_ocol*/
	op->bwr(NR_DS_BUF_SIZE_REG, width_out, 0, 8);
	/* config dsbuf_orow*/
	op->bwr(NR_DS_BUF_SIZE_REG, height_out, 8, 8);

	op->bwr(NRDSWR_X, (width_out - 1), 0, 13);
	op->bwr(NRDSWR_Y, (height_out - 1), 0, 13);

	op->bwr(NRDSWR_CAN_SIZE, (height_out - 1), 0, 13);
	op->bwr(NRDSWR_CAN_SIZE, (width_out - 1), 16, 13);
	/* little endian */
	op->bwr(NRDSWR_CAN_SIZE, 1, 13, 1);

	op->bwr(NR_DS_CTRL, v_step, 16, 6);
	op->bwr(NR_DS_CTRL, h_step, 24, 6);
	op->bwr(NR_DS_CTRL, (dimp_get(edi_mp_blend_mode) >> 24) & 0xf, 12, 3);
	op->bwr(NR_DS_CTRL, (dimp_get(edi_mp_blend_mode) >> 24) & 0xf, 8, 3);
	//op->bwr(NR_DS_CTRL, 2, 8, 3);

	op->bwr(NR_DS_BLD_COEF, (dimp_get(edi_mp_blend_mode) >> 16) & 0xff,
		16, 8);
	op->bwr(NR_DS_BLD_COEF, (dimp_get(edi_mp_blend_mode) >> 8) & 0xff,
		8, 8);
	op->bwr(NR_DS_BLD_COEF, dimp_get(edi_mp_blend_mode) & 0xff, 0, 8);

	op->bwr(NR_DS_CTRL, (dimp_get(edi_mp_blend_mode) >> 28) & 0xf, 0, 1);
	dbg_tb("%s:e:done\n", __func__);
}

void dim_dump_tb(unsigned int data, unsigned int ch)
{
	struct tb_core_s *pcfg;

	pcfg = dim_tbs[ch];

	if (pcfg->nr_dump_en) {
		if (pcfg->nr_dump_count < NR_DETECT_DUMP_CNT) {
			diwrite_to_file(pcfg->di_detect_buf[data].paddr,
				NR_DS_WIDTH * NR_DS_HEIGHT
				* 1, ch);
			dbg_tb("%s:dump NR1 %d %d done\n", __func__,
			       pcfg->nr_dump_count, data);
			pcfg->nr_dump_count++;
		} else {
			dbg_tb("%s:dump done\n", __func__);
			pcfg->nr_dump_en = 0;
		}
	}
}

/*
 * 0x37f9 ~ 0x37fc 0x3740 ~ 0x3743 8 regs
 */
void dim_dump_nrds_reg(unsigned int base_addr)
{
	unsigned int i = 0x37f9;

	pr_info("-----nrds reg start-----\n");
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x2006 << 2), i, DIM_RDMA_RD(0x2006));
	for (i = 0x37f9; i < 0x37fd; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2), i, DIM_RDMA_RD(i));
	for (i = 0x3740; i < 0x3744; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2), i, DIM_RDMA_RD(i));
	pr_info("-----nrds reg end-----\n");
}
#endif

/*
 * enable/disable nr ds mif&hw
 */
void dim_nr_ds_hw_ctrl(bool enable)
{
	/*Switch MIF to NR_DS*/
	const struct reg_acc *op = &di_pre_regset;
#ifdef DIM_TB_DETECT
	struct di_dev_s *di_devp = get_dim_de_devp();

	if (!di_devp->tb_flag_int)
		enable = 0;
#endif
	op->bwr(VIUB_MISC_CTRL0, enable ? 3 : 2, 5, 2);
	op->bwr(NRDSWR_CTRL, enable ? 1 : 0, 12, 1);
	op->bwr(NR_DS_CTRL, enable ? 1 : 0, 30, 1);
	dbg_tb("%s TB %d\n", __func__, enable);
}

void dim_tb_prob(void)
{
#ifdef DIM_TB_DETECT
	struct di_dev_s *di_devp = get_dim_de_devp();

	if (IS_IC_SUPPORT(TB))
		di_devp->tb_flag_int = 1;
	else
		di_devp->tb_flag_int = 0;
	PR_INF("%s:end=%d\n", __func__, di_devp->tb_flag_int);
#endif
}

