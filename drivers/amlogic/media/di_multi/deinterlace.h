/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/deinterlace.h
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

#ifndef _DI_H
#define _DI_H
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/di/di.h>
#include <linux/amlogic/media/di/di_interface.h>

#include "../di_local/di_local.h"
//#include "di_local.h"
#include <linux/clk.h>
#include <linux/atomic.h>
#include "deinterlace_hw.h"
#include "../deinterlace/di_pqa.h"
//#include "di_pqa.h"

/************************************************
 * config define
 ***********************************************/
#define DIM_OUT_NV21	(1)
//#define TEST_PIP	(1)
//#define DBG_POST_SETTING	(1)
/************************************************
 * vframe use ud meta data
 ************************************************/
#define DIM_EN_UD_USED	(1)

/************************************************
 * function:hdr
 ************************************************/
#define DIM_HAVE_HDR	(1)

/************************************************
 * function:decontour use detect border
 *	char aml_ldim_get_bbd_state(void) in
 *	include/linux/amlogic/media/vout/lcd/aml_ldim.h
 ************************************************/
//#define DIM_DCT_BORDER_DETECT	(1)
#define	DIM_DCT_BORDER_DBG	(1)
//#define DIM_DCT_BORDER_SIMULATION	(1)
/************************************************
 * pre-vpp link
 ************************************************/
//#define VPP_LINK_USED_FUNC	(1)

/************************************************
 * ext function:hf
 *	function:is_di_hf_y_reverse not define
 ************************************************/
//#define DIM_EXT_NO_HF	(1)

//#define DIM_TB_DETECT
/*trigger_pre_di_process param*/
#define TRIGGER_PRE_BY_PUT			'p'
#define TRIGGER_PRE_BY_DE_IRQ			'i'
#define TRIGGER_PRE_BY_UNREG			'u'
/*di_timer_handle*/
#define TRIGGER_PRE_BY_TIMER			't'
#define TRIGGER_PRE_BY_FORCE_UNREG		'f'
#define TRIGGER_PRE_BY_VFRAME_READY		'r'
#define TRIGGER_PRE_BY_PROVIDER_UNREG		'n'
#define TRIGGER_PRE_BY_DEBUG_DISABLE		'd'
#define TRIGGER_PRE_BY_PROVIDER_REG		'R'

#define DI_RUN_FLAG_RUN				0
#define DI_RUN_FLAG_PAUSE			1
#define DI_RUN_FLAG_STEP			2
#define DI_RUN_FLAG_STEP_DONE			3

#define USED_LOCAL_BUF_MAX			3
#define BYPASS_GET_MAX_BUF_NUM			9//4

/* buffer management related */
#define MAX_IN_BUF_NUM				(15)	/*change 4 to 8*/
#define MAX_LOCAL_BUF_NUM			(5)//(7)
#define MAX_LOCAL_BUF_NUM_REAL			(MAX_LOCAL_BUF_NUM << 1)
//#define MAX_POST_BUF_NUM			(20)//(11)	/*(5)*/ /* 16 */
#define POST_BUF_NUM				(20)
#define MAX_POST_BUF_NUM			(POST_BUF_NUM + 3)//(11)	/*(5)*/ /* 16 */
#define POST_BUF_NUM_DEF			(11)	//test only (POST_BUF_NUM)//
#define VFRAME_TYPE_IN				1
#define VFRAME_TYPE_LOCAL			2
#define VFRAME_TYPE_POST			3
#define VFRAME_TYPE_NUM				3

#define DI_POST_GET_LIMIT			8
#define DI_PRE_READY_LIMIT			4

#define LOCAL_META_BUFF_SIZE 0x800 /* 2K size */
#define LOCAL_UD_BUFF_SIZE VF_UD_MAX_SIZE /* 5K size */

/*vframe define*/
#define vframe_t struct vframe_s

#define is_from_vdin(vframe) ((vframe)->type & VIDTYPE_VIU_422)

/* canvas defination */
#define DI_USE_FIXED_CANVAS_IDX
/*#define	DET3D */
#undef SUPPORT_MPEG_TO_VDIN
#define CLK_TREE_SUPPORT
#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)	\
		aml_vcbus_update_bits((adr),		\
		((1 << (len)) - 1) << (start), (val) << (start))

#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif
#define DIM_BYPASS_VF_TYPE	(VIDTYPE_MVC | VIDTYPE_VIU_444 | \
				 VIDTYPE_PIC | VIDTYPE_RGB_444)

#define IS_I_SRC(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM)
#define IS_FIELD_I_SRC(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM && \
				(vftype) & VIDTYPE_VIU_FIELD)

#define IS_COMP_MODE(vftype) ((vftype) & VIDTYPE_COMPRESS)
#define IS_PROG(vftype) ((vftype & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE)

#define IS_420P_SRC(vftype) (((vftype) & VIDTYPE_INTERLACE_BOTTOM) == 0	&& \
			     ((vftype) & VIDTYPE_VIU_422) == 0		&& \
			     ((vftype) & VIDTYPE_VIU_444) == 0)

#define IS_VDIN_SRC(src) (				\
	((src) == VFRAME_SOURCE_TYPE_TUNER)	||	\
	((src) == VFRAME_SOURCE_TYPE_CVBS)	||	\
	((src) == VFRAME_SOURCE_TYPE_COMP)	||	\
	((src) == VFRAME_SOURCE_TYPE_HDMI))

#define VFMT_IS_I(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM)
#define VFMT_IS_P(vftype) (((vftype) & VIDTYPE_INTERLACE_BOTTOM) == 0)

#define VFMT_IS_I_FIELD(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM	&& \
			      (vftype) & VIDTYPE_VIU_FIELD)

#define VFMT_SET_TOP(vftype)	(((vftype) & (~VIDTYPE_TYPEMASK)) | \
				 VIDTYPE_INTERLACE_TOP)

#define VFMT_SET_BOTTOM(vftype)	(((vftype) & (~VIDTYPE_TYPEMASK)) | \
				 VIDTYPE_INTERLACE_BOTTOM)

#define VFMT_IS_TOP(vfm)	(((vfm) & VIDTYPE_TYPEMASK) ==	\
				VIDTYPE_INTERLACE_TOP)
#define VFMT_IS_EOS(vftype) ((vftype) & VIDTYPE_V4L_EOS)

#define IS_NV21_12(vftype) ((vftype) & (VIDTYPE_VIU_NV12 | VIDTYPE_VIU_NV21))

#define VFMT_MASK_ALL		(VIDTYPE_TYPEMASK	|	\
				 VIDTYPE_VIU_NV12	|	\
				 VIDTYPE_VIU_422	|	\
				 VIDTYPE_VIU_444	|	\
				 VIDTYPE_VIU_NV21	|	\
				 VIDTYPE_MVC		|	\
				 VIDTYPE_VIU_SINGLE_PLANE |	\
				 VIDTYPE_VIU_FIELD	|	\
				 VIDTYPE_PIC		|	\
				 VIDTYPE_RGB_444	|	\
				 VIDTYPE_SCATTER	|	\
				 VIDTYPE_COMB_MODE	|	\
				 VIDTYPE_COMPRESS)

#define VFMT_COLOR_MSK		(VIDTYPE_VIU_NV12	|	\
				 VIDTYPE_VIU_444	|	\
				 VIDTYPE_VIU_NV21	|	\
				 VIDTYPE_VIU_422	|	\
				 VIDTYPE_RGB_444)

#define DIM_BYPASS_VF_TYPE	(VIDTYPE_MVC | VIDTYPE_VIU_444 | \
				 VIDTYPE_PIC | VIDTYPE_RGB_444)
#define VFMT_DIPVPP_CHG_MASK	(VIDTYPE_TYPEMASK	|	\
				 VIDTYPE_VIU_422	|	\
				 VIDTYPE_VIU_444	|	\
				 VIDTYPE_VIU_NV21	|	\
				 VIDTYPE_VIU_NV12	|	\
				 VIDTYPE_VIU_SINGLE_PLANE |	\
				 VIDTYPE_VIU_FIELD	|	\
				 VIDTYPE_COMPRESS	|	\
				 VIDTYPE_SCATTER	|	\
				 VIDTYPE_COMB_MODE	|	\
				 VIDTYPE_DI_PW)

//need add :VFRAME_FLAG_DI_BYPASS
#define VFMT_FLG_CHG_MASK	(VFRAME_FLAG_DI_PW_VFM	|	\
				 VFRAME_FLAG_DI_PW_N_LOCAL |	\
				 VFRAME_FLAG_DI_PW_N_EXT |	\
				 VFRAME_FLAG_HF	|		\
				 VFRAME_FLAG_DI_DW	|	\
				 VFRAME_FLAG_VIDEO_LINEAR)

enum process_fun_index_e {
	PROCESS_FUN_NULL = 0,
	PROCESS_FUN_DI,
	PROCESS_FUN_PD,
	PROCESS_FUN_PROG,
	PROCESS_FUN_BOB
};

#define process_fun_index_t enum process_fun_index_e

enum canvas_idx_e {
	NR_CANVAS,
	MTN_CANVAS,
	MV_CANVAS,
};

enum EDI_SGN {
	EDI_SGN_SD,
	EDI_SGN_HD,
	EDI_SGN_4K,
	EDI_SGN_OTHER,
};

#ifdef HIS_CODE	// move to di_interlace.h
struct di_win_s {
	unsigned int x_size;
	unsigned int y_size;
	unsigned int x_st;
	unsigned int y_st;
};
#endif
#define pulldown_mode_t enum pulldown_mode_e

struct dsub_bufv_s {
	void *in; /* struct dim_nins_s */
	bool in_noback; /* this is for dec reset */
	bool src_is_i;
	bool en_hdr;
	void *buffer;	/*for new interface */
	struct dvfm_s dvframe;
};

struct di_buf_s {
	struct vframe_s *vframe;
	int index; /* index in vframe_in_dup[] or vframe_in[],
		    * only for type of VFRAME_TYPE_IN
		    */
	int post_proc_flag; /* 0,no post di; 1, normal post di;
			     * 2, edge only; 3, dummy
			     */
	int new_format_flag;
	int type;
	int throw_flag;
	int invert_top_bot_flag;
	int seq;
	unsigned int field_count;
	unsigned int seq_post; /*ary add 2020-07-21*/
	int pre_ref_count; /* none zero, is used by mem_mif,
			    * chan2_mif, or wr_buf
			    */
	int post_ref_count; /* none zero, is used by post process */
	int queue_index;
	/*below for type of VFRAME_TYPE_LOCAL */
	struct dim_mm_blk_s *blk_buf; /* tmp */
	unsigned int nr_size;
	unsigned int tab_size;
	unsigned long nr_adr;
	int nr_canvas_idx;
	unsigned long mtn_adr;
	int mtn_canvas_idx;
	unsigned long cnt_adr;
	int cnt_canvas_idx;
	unsigned long mcinfo_adr;
	unsigned short	*mcinfo_adr_v;/**/
	bool		mcinfo_alloc_flg; /**/
	int mcinfo_canvas_idx;
	unsigned long mcvec_adr;
	int mcvec_canvas_idx;
	unsigned int afbc_crc;
	unsigned long adr_start; /*2020-10-04*/
	unsigned long afbc_adr;
	unsigned long afbct_adr;
	unsigned long dw_adr;
	unsigned long insert_adr;
	unsigned long hf_adr;
	struct mcinfo_pre_s {
		unsigned int highvertfrqflg;
		unsigned int motionparadoxflg;
		unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
	} curr_field_mcinfo;
	/* blend window */
	struct pulldown_detected_s
	pd_config;
	/* tff bff check result bit[1:0]*/
	unsigned int privated;
	unsigned int canvas_config_flag;
	/* 0,configed; 1,config type 1 (prog);
	 * 2, config type 2 (interlace)
	 */
	unsigned int canvas_height;
	unsigned int canvas_height_mc;	/*ary add for mc h is diff*/
	unsigned int canvas_width[3];/* nr/mtn/mv */
	process_fun_index_t process_fun_index;
	int early_process_fun_index;
	int left_right;/*1,left eye; 0,right eye in field alternative*/
	/*below for type of VFRAME_TYPE_POST*/
	struct di_buf_s *di_buf[2];
	struct di_buf_s *di_buf_dup_p[5];
	/* 0~4: n-2, n-1, n, n+1, n+2;	n is the field to display*/
	/*0: n-2*/
	/*1: n-1*/
	/*2: n*/
	/*3: n+1*/
	/*4: n+2*/
	struct di_buf_s *di_wr_linked_buf;
	/* debug for di-vf-get/put
	 * 1: after get
	 * 0: after put
	 */
	struct di_buf_s *in_buf; /*keep dec vf: link in buf*/
	unsigned int dec_vf_state;	/*keep dec vf:*/
	atomic_t di_cnt;
	struct page	*pages;
	/*ary add */
	unsigned int channel;
	unsigned int width_bk; /*move from ppre*/
	unsigned int dw_width_bk;
	unsigned int dw_height_bk;
	unsigned int buf_hsize;	/* for t7 */
//	unsigned int flg_tvp;
	unsigned int afbc_info; /*bit 0: src is i; bit 1: src is real i */
	unsigned char afbc_sgn_cfg;
	struct di_win_s win; /*post write*/
	struct di_buf_s *di_buf_post; /*07-27 */
	struct hf_info_t hf;	/* struct hf_info_t */
	unsigned long jiff; // for wait
	struct dim_rpt_s pq_rpt;
	enum EDI_SGN	sgn_lv;
//	void *pat_buf; /*2020-10-05*/
	void *iat_buf; /*2020-10-13*/
	unsigned int buf_is_i	: 1; /* 1: i; 0: p */
	unsigned int flg_nr	: 1;
	unsigned int flg_null	: 1;
	unsigned int flg_nv21	: 2;
	unsigned int is_4k	: 1;
	unsigned int is_eos	: 1;
	unsigned int is_lastp	: 1;
	unsigned int flg_afbce_set	: 1;

	unsigned int trig_post_update	: 1;
	unsigned int afbce_out_yuv420_10	: 1; /* 2020-11-26*/
	unsigned int is_nbypass	: 1; /*2020-12-07*/
	unsigned int is_bypass_pst : 1; /*2021-01-07*/
	unsigned int is_bypass_mem : 1;
	unsigned int en_hf	: 1; //only flg post buffer
	unsigned int hf_done	: 1; //
	unsigned int rev1	: 1;

	unsigned int rev2	: 16;
	struct dsub_bufv_s	c;
	unsigned int datacrc;
	unsigned int nrcrc;
	unsigned int mtncrc;
	/* local meta buffer */
	u8 *local_meta;
	u32 local_meta_used_size;
	u32 local_meta_total_size;
	/* local ud buffer */
	u8 *local_ud;
	u32 local_ud_used_size;
	u32 local_ud_total_size;
	bool hf_irq;
	bool dw_have;
	bool flg_dummy;
#ifdef CONFIG_AMLOGIC_MEDIA_THERMAL
	bool bit_8_flag;
#endif
};

#define RDMA_DET3D_IRQ			0x20
/* vdin0 rdma irq */
#define RDMA_DEINT_IRQ			0x2
#define RDMA_TABLE_SIZE			((PAGE_SIZE) << 1)

#define MAX_CANVAS_WIDTH		1920
#define MAX_CANVAS_HEIGHT		1088

/* #define DI_BUFFER_DEBUG */

#define DI_LOG_MTNINFO			0x02
#define DI_LOG_PULLDOWN			0x10
#define DI_LOG_BUFFER_STATE		0x20
#define DI_LOG_TIMESTAMP		0x100
#define DI_LOG_PRECISE_TIMESTAMP	0x200
#define DI_LOG_QUEUE			0x40
#define DI_LOG_VFRAME			0x80

#define QUEUE_LOCAL_FREE		0
#define QUEUE_RECYCLE			1	/* 5 */
#define QUEUE_DISPLAY			2	/* 6 */
#define QUEUE_TMP			3	/* 7 */
#define QUEUE_POST_DOING		4	/* 8 */

#define QUEUE_IN_FREE			5	/* 1 */
#define QUEUE_PRE_READY			6	/* 2 */
#define QUEUE_POST_FREE			7	/* 3 */
#define QUEUE_POST_READY		8	/* 4	QUE_POST_READY */

/*new use this for put back control*/
#define QUEUE_POST_PUT_BACK		(9)
#define QUEUE_POST_DOING2		(10)
#define QUEUE_POST_KEEP			(11)/*below use pw_queue_in*/
#define QUEUE_POST_KEEP_BACK		(12)
#define QUEUE_PST_NO_BUF		(17)//(15)

#define QUEUE_NUM			5	/* 9 */
#define QUEUE_NEW_THD_MIN		(QUEUE_IN_FREE - 1)
#define QUEUE_NEW_THD_MAX		(QUEUE_PST_NO_BUF + 1)
//(QUEUE_POST_KEEP_BACK + 1)


#define queue_t struct queue_s

#define VFM_NAME		"deinterlace"

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
void enable_rdma(int enable_flag);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
bool is_vsync_rdma_enable(void);
#else
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)		\
			       aml_vcbus_update_bits((adr),	\
			       ((1 << (len)) - 1) << (start), (val) << (start))

#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif

#define DI_COUNT   1
#define DI_MAP_FLAG	0x1
#define DI_SUSPEND_FLAG 0x2
#define DI_LOAD_REG_FLAG 0x4
#define DI_VPU_CLKB_SET 0x8

struct di_dev_s {
	dev_t			   devt;
	struct cdev		   cdev; /* The cdev structure */
	struct device	   *dev;
	struct platform_device	*pdev;
	dev_t	devno;
	struct class	*pclss;

	bool sema_flg;	/*di_sema_init_flag*/

	struct task_struct *task;
	struct clk	*vpu_clkb;
	struct clk	*vpu_clk_mux;
	unsigned long clkb_max_rate;
	unsigned long clkb_min_rate;
	struct list_head   pq_table_list;
#ifdef HIS_CODE
	atomic_t	       pq_flag; /* 1: idle; 0: busy */
	atomic_t	       pq_io; /* 1: idle; 0: busy */
#endif
	struct mutex lock_pq; /* for pq load */
	unsigned char	   di_event;
	unsigned int	   pre_irq;
	unsigned int	   post_irq;
	unsigned int	aisr_irq;
	unsigned int	   flags;
	unsigned long	   jiffy;
	bool	mem_flg;	/*ary add for make sure mem is ok*/
	unsigned int	   buf_num_avail;
	int		rdma_handle;
	/* is support nr10bit */
	unsigned int	   nr10bit_support;
	/* is DI support post wr to mem for OMX */
	unsigned int       post_wr_support;
	//unsigned int nrds_enable;
	unsigned int pps_enable;
	unsigned int h_sc_down_en;/*sm1, tm2 ...*/
	/*struct	mutex      cma_mutex;*/
	struct dentry *dbg_root;	/*dbg_fs*/
	/***************************/
	/*struct di_data_l_s	data_l;*/
	void *data_l;
	u8 *local_meta_addr;
	u32 local_meta_size;

	u8 *local_ud_addr;
	u32 local_ud_size;

	struct vpu_dev_s *dim_vpu_clk_gate_dev;
	struct vpu_dev_s *dim_vpu_pd_dec;
	struct vpu_dev_s *dim_vpu_pd_dec1;
	struct vpu_dev_s *dim_vpu_pd_vd1;
	struct vpu_dev_s *dim_vpu_pd_post;
	bool is_crc_ic;
#ifdef DIM_TB_DETECT
	//unsigned int tb_detect;
	unsigned int tb_detect_period;
	unsigned int tb_detect_buf_len;
	unsigned int tb_detect_init_mute;
	unsigned int tb_flag_int;
#endif
};

struct di_pre_stru_s {
/* pre input */
	struct DI_MIF_S	di_inp_mif;
	struct DI_MIF_S	di_mem_mif;
	struct DI_MIF_S	di_chan2_mif;
	struct di_buf_s *di_inp_buf;
	struct di_buf_s *di_post_inp_buf;
	struct di_buf_s *di_inp_buf_next;
	/* p_asi_next: ary:add for p */
	struct di_buf_s *p_asi_next;
	struct di_buf_s *di_mem_buf_dup_p;
	struct di_buf_s *di_chan2_buf_dup_p;
/* pre output */
	struct DI_SIM_MIF_S	di_nrwr_mif;
	struct DI_SIM_MIF_S	di_mtnwr_mif;
	struct di_buf_s *di_wr_buf;
	struct di_buf_s *di_post_wr_buf;
	struct DI_SIM_MIF_S	di_contp2rd_mif;
	struct DI_SIM_MIF_S	di_contprd_mif;
	struct DI_SIM_MIF_S	di_contwr_mif;
	struct DI_SIM_MIF_S	hf_mif;
	int		field_count_for_cont;
/*
 * 0 (f0,null,f0)->nr0,
 * 1 (f1,nr0,f1)->nr1_cnt,
 * 2 (f2,nr1_cnt,nr0)->nr2_cnt
 * 3 (f3,nr2_cnt,nr1_cnt)->nr3_cnt
 */
	struct DI_MC_MIF_s		di_mcinford_mif;
	struct DI_MC_MIF_s		di_mcvecwr_mif;
	struct DI_MC_MIF_s		di_mcinfowr_mif;
/* pre state */
	int	in_seq;
	int	recycle_seq;
	int	pre_ready_seq;

	int	pre_de_busy;            /* 1 if pre_de is not done */
	int	pre_de_process_flag;    /* flag when dim_pre_de_process done */
	int	pre_de_clear_flag;
	/* flag is set when VFRAME_EVENT_PROVIDER_UNREG*/
	int	unreg_req_flag_cnt;

	int	reg_req_flag_cnt;
	int	force_unreg_req_flag;
	int	disable_req_flag;
	/* current source info */
	int	cur_width;
	int	cur_height;
	int	cur_inp_type;
	int	cur_source_type;
	int	cur_sig_fmt;
	unsigned int orientation;
	int	cur_prog_flag; /* 1 for progressive source */
/* valid only when prog_proc_type is 0, for
 * progressive source: top field 1, bot field 0
 */
	int	source_change_flag;
/* input size change flag, 1: need reconfig pre/nr/dnr size */
/* 0: not need config pre/nr/dnr size*/
	bool input_size_change_flag;
/* true: bypass di all logic, false: not bypass */
	bool bypass_flag;
	/* bit0 for cfg, bit1 for t5dvb*/
	unsigned int is_bypass_mem	: 4;
	unsigned int is_bypass_all	: 1;
	unsigned int is_bypass_fg	: 1;
	unsigned int is_disable_chan2	: 1;
	unsigned int is_disable_nr	: 1;	//for t5d vb
	unsigned int rev1		: 24;
	unsigned char prog_proc_type;
/* set by prog_proc_config when source is vdin,0:use 2 i
 * serial buffer,1:use 1 p buffer,3:use 2 i paralleling buffer
 */
/* ary: local play p mode is 0
 * local play i mode is 0
 */
/* alloc di buf as p or i;0: alloc buf as i;
 * 1: alloc buf as p;
 */
	unsigned char madi_enable;
	unsigned char mcdi_enable;
	unsigned int  pps_dstw;	/*no use ?*/
	unsigned int  pps_dsth;	/*no use ?*/
	int	left_right;/*1,left eye; 0,right eye in field alternative*/
/*input2pre*/
	int	bypass_start_count;
/* need discard some vframe when input2pre => bypass */
	unsigned char vdin2nr;
	enum tvin_trans_fmt	source_trans_fmt;
	enum tvin_trans_fmt	det3d_trans_fmt;
	unsigned int det_lr;
	unsigned int det_tp;
	unsigned int det_la;
	unsigned int det_null;
	unsigned int width_bk;
#ifdef DET3D
	int	vframe_interleave_flag;
#endif
/**/
	int	pre_de_irq_timeout_count;
	int	pre_throw_flag;
	int	bad_frame_throw_count;
/*for static pic*/
	int	static_frame_count;
	bool force_interlace;
	bool bypass_pre;
	bool invert_flag;
//	bool vdin_source; /* ary 2020-06-12: no*/
	int cma_release_req;
	/* for performance debug */
	u64 irq_time[2];
	/* combing adaptive */
	struct combing_status_s *mtn_status;
	bool combing_fix_en;
	unsigned int comb_mode;
	enum EDI_SGN sgn_lv;
	union hw_sc2_ctr_pre_s	pre_top_cfg;
	union afbc_blk_s	en_cfg;
	union afbc_blk_s	en_set;
	struct vframe_s		vfm_cpy;
	unsigned int		h_size; //real di h_size
	unsigned int		v_size;	//real di v_size
	struct SHRK_S shrk_cfg;
	struct dvfm_s dw_wr_dvfm;
	bool timeout_check;
	bool used_pps;
	unsigned int afbc_skip_w;
	unsigned int afbc_skip_h;
	unsigned int pps_width;
	unsigned int pps_height;
};

struct dim_fmt_s;

struct di_post_stru_s {
	struct DI_MIF_S	di_buf0_mif;
	struct DI_MIF_S	di_buf1_mif;
	struct DI_MIF_S	di_buf2_mif;
	struct DI_SIM_MIF_S di_diwr_mif;
	struct DI_SIM_MIF_S hf_mif;
	struct DI_SIM_MIF_S	di_mtnprd_mif;
	struct DI_MC_MIF_s	di_mcvecrd_mif;
	/*post doing buf and write buf to post ready*/
	struct di_buf_s *cur_post_buf;
	struct di_buf_s *keep_buf;
	struct di_buf_s *keep_buf_post;	/*ary add for keep post buf*/
	int		update_post_reg_flag;
	int		run_early_proc_fun_flag;
	int		cur_disp_index;
	int		canvas_id;
	int		next_canvas_id;
	bool		toggle_flag;
	bool		vscale_skip_flag;
	uint		start_pts;
	u64		start_pts64;
	int		buf_type;
	int de_post_process_done;
	int post_de_busy;
	bool process_doing;/* for pre-vpp-link sw */
	int di_post_num;
	unsigned int post_peek_underflow;
	unsigned int di_post_process_cnt;
	unsigned int check_recycle_buf_cnt;/*cp to di_hpre_s*/
	/* performance debug */
	unsigned int  post_wr_cnt;
	unsigned long irq_time;

	struct pst_cfg_afbc_s afbc_cfg;/* ary add for afbc dec*/
	/*frame cnt*/
	unsigned int frame_cnt;	/*cnt for post process*/
	unsigned int seq; /* 2021-01-13 */
//	unsigned int last_pst_size;
	union hw_sc2_ctr_pst_s	pst_top_cfg;
	union afbc_blk_s	en_cfg;
	union afbc_blk_s	en_set;
#ifdef TEST_PIP
	/* mem cpy */
	struct mem_cpy_s	cfg_cpy;
	struct AFBCD_S		in_afbcd;
	struct AFBCE_S		out_afbce;
	struct DI_MIF_S		out_wrmif;
	struct dim_fmt_s	fmt_in;
	struct dim_fmt_s	fmt_out;
	struct dim_cvsi_s	cvsi_in;
	struct dim_cvsi_s	cvsi_out;
	struct di_buf_s		in_buf;
	struct vframe_s		in_buf_vf;
#endif
	struct di_win_s win_dis;
};

#define MAX_QUEUE_POOL_SIZE   256
struct queue_s {
	unsigned int num;
	unsigned int in_idx;
	unsigned int out_idx;
	unsigned int type;
/* 0, first in first out;
 * 1, general;2, fix position for di buf
 */
	unsigned int pool[MAX_QUEUE_POOL_SIZE];
};

struct di_buf_pool_s {
	struct di_buf_s *di_buf_ptr;
	unsigned int size;
};

unsigned char dim_is_bypass(vframe_t *vf_in, unsigned int channel);
//bool dim_bypass_first_frame(unsigned int ch);

int di_cnt_buf(int width, int height, int prog_flag, int mc_mm,
	       int bit10_support, int pack422);

/*---get di state parameter---*/
struct di_dev_s *get_dim_de_devp(void);

const char *dim_get_version_s(void);
int dim_get_dump_state_flag(void);

int dim_get_blocking(void);

struct di_buf_s *dim_get_recovery_log_di_buf(void);

unsigned long dim_get_reg_unreg_timeout_cnt(void);
struct vframe_s **dim_get_vframe_in(unsigned int ch);
int dim_check_recycle_buf(unsigned int channel);

int dim_seq_file_module_para_di(struct seq_file *seq);
int dim_seq_file_module_para_hw(struct seq_file *seq);

int dim_seq_file_module_para_film_fw1(struct seq_file *seq);
int dim_seq_file_module_para_mtn(struct seq_file *seq);

int dim_seq_file_module_para_pps(struct seq_file *seq);

int dim_seq_file_module_para_(struct seq_file *seq);

/***********************/

unsigned int di_get_dts_nrds_en(void);
int di_get_disp_cnt(void);
void dbg_vfm(struct vframe_s *vf, unsigned int cnt);

/*---------------------*/
long dim_pq_load_io(unsigned long arg);
int dim_get_canvas(void);
void dim_release_canvas(void);

unsigned int dim_cma_alloc_total(struct di_dev_s *de_devp);
irqreturn_t dim_irq(int irq, void *dev_instance);
irqreturn_t dim_post_irq(int irq, void *dev_instance);
irqreturn_t dim_aisr_irq(int irq, void *dev_instance);

void dim_rdma_init(void);
void dim_rdma_exit(void);

void dim_set_di_flag(void);
void dim_get_vpu_clkb(struct device *dev, struct di_dev_s *pdev);
unsigned int dim_get_vpu_clk_ext(void);
bool dim_pre_link_state(void);
void dim_log_buffer_state(unsigned char *tag, unsigned int channel);

unsigned char dim_pre_de_buf_config(unsigned int channel);
void dim_pre_de_process(unsigned int channel);
void dim_pre_de_done_buf_config(unsigned int channel, bool flg_timeout);
void dim_pre_de_done_buf_clear(unsigned int channel);

void di_reg_setting(unsigned int channel, struct vframe_s *vframe);
void di_reg_variable(unsigned int channel, struct vframe_s *vframe);

void di_unreg_variable(unsigned int channel);
void di_unreg_setting(bool plink);

void dim_uninit_buf(unsigned int disable_mirror, unsigned int channel);

int dim_process_post_vframe(unsigned int channel);
unsigned char dim_check_di_buf(struct di_buf_s *di_buf, int reason,
			       unsigned int channel);
int dim_do_post_wr_fun(void *arg, vframe_t *disp_vf);
int dim_post_process(void *arg, unsigned int zoom_start_x_lines,
		     unsigned int zoom_end_x_lines,
		     unsigned int zoom_start_y_lines,
		     unsigned int zoom_end_y_lines, vframe_t *disp_vf);
void dim_post_de_done_buf_config(unsigned int channel);
void dim_recycle_post_back(unsigned int channel);
void recycle_post_ready_local(struct di_buf_s *di_buf,
			      unsigned int channel);
void dim_post_keep_cmd_release2_local(struct vframe_s *vframe);
/*--------------------------*/
unsigned char dim_vcry_get_flg(void);
void dim_vcry_flg_inc(void);
void dim_vcry_set_flg(unsigned char val);
/*--------------------------*/
unsigned int dim_vcry_get_log_reason(void);
void dim_vcry_set_log_reason(unsigned int val);
/*--------------------------*/
unsigned char dim_vcry_get_log_q_idx(void);
void dim_vcry_set_log_q_idx(unsigned int val);
/*--------------------------*/
struct di_buf_s **dim_vcry_get_log_di_buf(void);
void dim_vcry_set_log_di_buf(struct di_buf_s *di_bufp);
void dim_vcry_set(unsigned int reason, unsigned int idx,
		  struct di_buf_s *di_bufp);

const char *dim_get_vfm_type_name(unsigned int nub);
bool dim_get_mcmem_alloc(void);

int dim_get_reg_unreg_cnt(void);
void dim_reg_timeout_inc(void);

unsigned int is_bypass2(struct vframe_s *vf_in, unsigned int ch);
void dim_post_keep_cmd_proc(unsigned int ch, unsigned int index);
bool dim_need_bypass(unsigned int ch, struct vframe_s *vf);

/*--------------------------*/
//int di_ori_event_qurey_vdin2nr(unsigned int channel);
//int di_ori_event_reset(unsigned int channel);
//int di_ori_event_light_unreg(unsigned int channel);
//int di_ori_event_light_unreg_revframe(unsigned int channel);
//int di_ori_event_ready(unsigned int channel);
//int di_ori_event_qurey_state(unsigned int channel);
//void  di_ori_event_set_3D(int type, void *data, unsigned int channel);
void di_block_set(int val);
int di_block_get(void);
void di_lock_irqfiq_save(ulong irq_flag);
void di_unlock_irqfiq_restore(ulong irq_flag);
void di_lock_irq(void);
void di_unlock_irq(void);

int dump_state_flag_get(void);
int di_get_kpi_frame_num(void);

/*--------------------------*/
extern int pre_run_flag;
extern unsigned int dbg_first_cnt_pre;
extern spinlock_t plist_lock;

void dim_dbg_pre_cnt(unsigned int channel, char *item);

int di_vf_l_states(struct vframe_states *states, unsigned int channel);
struct vframe_s *di_vf_l_peek(unsigned int channel);
void di_vf_l_put(struct vframe_s *vf, unsigned char channel);
struct vframe_s *di_vf_l_get(unsigned int channel);

unsigned char pre_p_asi_de_buf_config(unsigned int ch);
void dim_dbg_release_keep_all(unsigned int ch);
//void dim_post_keep_back_recycle(unsigned int ch);
void dim_post_re_alloc(unsigned int ch);
//void dim_post_release(unsigned int ch);
unsigned int dim_cfg_nv21(void);
bool dim_cfg_pre_nv21(unsigned int cmd);

void dim_get_default(unsigned int *h, unsigned int *w);
void dbg_h_w(unsigned int ch, unsigned int nub);
void di_set_default(unsigned int ch);
//bool dim_dbg_is_cnt(void);
bool pre_dbg_is_run(void);
irqreturn_t dpvpp_irq(int irq, void *dev_instance);

/*---------------------*/
const struct afd_ops_s *dim_afds(void);
ssize_t
store_config(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf, size_t count);
ssize_t
store_dbg(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count);
ssize_t
store_dump_mem(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len);
ssize_t
store_log(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count);
ssize_t
show_vframe_status(struct device *dev,
		   struct device_attribute *attr,
		   char *buf);
ssize_t
show_kpi_frame_num(struct device *dev,
		   struct device_attribute *attr,
		   char *buf);
ssize_t
store_kpi_frame_num(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t len);

ssize_t dim_read_log(char *buf);
void di_load_pq_table(void);

/*---------------------*/
void test_display(void);
/*---------------------*/

struct di_buf_s *dim_get_buf(unsigned int channel,
			     int queue_idx, int *start_pos);

#define queue_for_each_entry(di_buf, channel, queue_idx, list)	\
	for (itmp = 0;						\
	    ((di_buf = dim_get_buf(channel, queue_idx, &itmp)) != NULL);)

#define di_dev_t struct di_dev_s

#define di_pr_info(fmt, args ...)   pr_info("DI: " fmt, ## args)

#define pr_dbg(fmt, args ...)       pr_debug("DI: " fmt, ## args)

#define pr_error(fmt, args ...)     pr_err("DI: " fmt, ## args)

/*this is debug for buf*/
/*#define DI_DEBUG_POST_BUF_FLOW	(1)*/
#define OPS_LV1		(1)
//#define TEST_DISABLE_BYPASS_P	(1)

void sc2_dbg_set(unsigned int val);
bool sc2_dbg_is_en_pre_irq(void);
bool sc2_dbg_is_en_pst_irq(void);
void sc2_dbg_pre_info(unsigned int val);
void sc2_dbg_pst_info(unsigned int val);
void hpre_timeout_read(void);
void hpst_timeout_read(void);
void dpre_vdoing(unsigned int ch);

#define TEST_4K_NR	(1)
//#define DBG_TEST_CRC	(1)
//#define DBG_TEST_CRC_P	(1)

//#define PRINT_BASIC	(1)

/*split buffer alloc to i, p , idata, iafbc xxx */
#define CFG_BUF_ALLOC_SP (1)
/*@ary_note: 2020-12-17*/
/* */
//#define TST_NEW_INS_INTERFACE		(1)

//#define TMP_TEST	(1)
#define VPP_LINK_NEED_CHECK	(1)
//#define TMP_MASK_FOR_T7 (1)
#define TMP_FOR_S4DW	(1)
/* dimp_get(edi_mp_mcpre_en) */
//#define TMP_S4DW_MC_EN	(1)
//#define DBG_BUFFER_FLOW	(1)
//#define DBG_CLEAR_MEM	(1)
#define DBG_BUFFER_EXT	(1)
#define DBG_VFM_CVS	(1)
//#define TMP_EN_PLINK	(1)
//#define DBG_EXTBUFFER_ONLY_ADDR	(1)
//#define S4D_OLD_SETTING_KEEP (1)
//#define S4D_OLD_PQ_KEEP (1)

#endif
