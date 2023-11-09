/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_data_l.h
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

#ifndef __DI_DD_H__
#define __DI_DD_H__

//#include "deinterlace.h"
//#include "di_data_l.h"

#define DBG_BLK		(1)
#define __CH_NUB_I	(2)
#define __CH_NUB_PLINK	(3)
#define __CH_NUB_PW	(2)
#define __CH_NUB_SC	(1)
#define __CH_NUB_IDLE	(2)
#define __CH_MAX	(4)

#define __NUB_BLK_MAX	(63) //tmp
#define __NUB_DIS_MAX	(63)

#define __SIZE_QC_4	(4)
#define __SIZE_QC_8	(8)
#define __SIZE_QC_16	(16)
#define __SIZE_QC_32	(32)
#define __SIZE_QC_64	(64)
#define __SIZE_QC_128	(128)
#define __SIZE_QC_256	(256)

#define __SIZE_DIS_FREE	(__SIZE_QC_64) //> __NUB_DIS_MAX
#define __SIZE_BLK_IDLE	(__SIZE_QC_64) //> __NUB_BLK_MAX
#define __SIZE_BLK_PLINK_HD (__SIZE_QC_16) //> __CH_NUB_PLINK
#define __SIZE_BLK_PLINK_UHD (__SIZE_QC_4) //> __CH_NUB_PLINK

#define DIM_P_LINK_DCT_NUB	20//test 8
enum MEM_FROM { //if change, check dim_mm_alloc_api2
	MEM_FROM_CODEC,	//cma mode 4
	MEM_FROM_CMA_DI, //1
	MEM_FROM_CMA_C, /* CMA COMMON*/
	MEM_FROM_SYS_K,
	MEM_FROM_SYS_V,
};

//2022-08-17
enum HD_TYPE { /*data type*/
	HD_T_NULL,
	HD_T_BLK,
	HD_T_DIS,
	HD_T_END
};

enum EBLK_TYPE {
	//BLK_T_IDLE,
	EBLK_T_HD_FULL,	/* 1080p yuv 422 10 bit */
	EBLK_T_HD_FULL_TVP,
	EBLK_T_UHD_FULL_AFBCE,	/* 4k yuv 422 10 bit */
	EBLK_T_UHD_FULL_AFBCE_TVP,
	EBLK_T_UHD_FULL,	/* 4k yuv 422 10 bit */
	EBLK_T_UHD_FULL_TVP,
	EBLK_T_HD_NV21,
	EBLK_T_HD_NV21_TVP,
	EBLK_T_LINK_DCT,
	EBLK_T_LINK_DCT_TVP,
	/* double end */
	EBLK_T_S_LINK_AFBC_T_FULL,/* no tvp */

	EBLK_T_END
};

#define K_BLK_T_NUB	(EBLK_T_END)

enum EAFBC_TYPE {
	EAFBC_T_HD_FULL,
	EAFBC_T_4K_FULL,
	EAFBC_T_4K_YUV420_10,
	EAFBC_T_END
};

#define K_AFBC_T_NUB	(EAFBC_T_END)

enum EBLK_ST {
	EBLK_ST_FREE,
	EBLK_ST_ALLOC,
	EBLK_ST_USED = 0x80
};

struct c_mm_p_cnt_in_s {
	unsigned int w;
	unsigned int h;
	enum EDPST_MODE mode;
};

union c_out_fmt_s {
	unsigned int d32;
	struct {
		unsigned char	mode	: 4,//EDPST_MODE_422_10BIT_PACK
			out_buf		: 4, //out_fmt
			afbce_fmt	: 4, //ref to enum EAFBC_TYPE
			is_afbce	: 1,
			is_sct_mem	: 1;
	} b;
};

struct c_mm_p_cnt_out_s {
	enum EDPST_MODE mode; //0: yuv 422 10 bit; 1: nv21
	unsigned int size_total;
	unsigned int size_page;
	unsigned int cvs_w;
	unsigned int cvs_h;
	unsigned int off_uv;
	unsigned int dbuf_hsize;
};

struct c_cfg_blkt_s {
	unsigned char blkt_n[K_BLK_T_NUB];
	bool	flg_update;
	struct mutex lock_cfg; //lock read and wr
};

struct c_cfg_blki_s {
	enum EBLK_TYPE blk_t;
	unsigned int	mem_size;
	unsigned int	page_size;
	bool		tvp;
	unsigned char	mem_from;
	struct c_mm_p_cnt_out_s *cnt_cfg;
};

struct c_cfg_afbc_s {
	unsigned char type;
	unsigned int	size_table;
	unsigned int	size_i;
	unsigned int	size_buffer;
};

struct c_cfg_dct_s {
	unsigned char nub;
	unsigned int size_one;
	unsigned int size_total;
	unsigned int grd_size;
	unsigned int yds_size;
	unsigned int cds_size;
};

struct mem_a_s {
	struct c_cfg_blki_s *inf;
	const char *owner;
	const char *note;
};

struct mem_r_s {
	struct c_blk_m_s *blk;
	const char *note;
};

struct c_hd_s {
	unsigned int code;
	unsigned char hd_tp; /* HD_TYPE */
	unsigned char idx;
	void *p; /* real data */
};

struct c_name_s {
	unsigned char ch;
	unsigned char o_type;
	unsigned char o_id;
	unsigned int reg_cnt;
	char *name;
	void *p;
};

/* like cma mem */
struct c_blk_m_s {
	struct c_cfg_blki_s *inf;
	bool	flg_alloc;
	unsigned long	mem_start;
	struct page	*pages;
};

/* scatter mem */
struct c_blks_s {
	unsigned int blk_typ;
	bool	flg_alloc;
	unsigned int sct_keep; //keep number
	unsigned int length;
	void *pat_buf;
	void *sct;
};

struct blk_s {
	unsigned int code; //must be first
	struct c_hd_s *hd;
	struct {
		unsigned int blk_typ;
		union {
		struct c_blk_m_s	blk_m;
		struct c_blks_s		blk_s;//?
		} b;
		unsigned char st_id; //ref to EBLK_ST
		struct c_name_s ower;
	} c;
	atomic_t get;
};

struct c_fifo_s {
	struct kfifo	f;
	bool		flg_alloc;
	unsigned char	id;
	unsigned char	size; //dbg only
	spinlock_t lock_w; //protect blk_o_put
	spinlock_t lock_r; //protect blk_o_get
	const char *name;
};

/* wr/rd data to/from file */
struct fs_ph_one_s {
	ulong addr;
	unsigned int len;
};

struct fs_ph_s {
	bool smode;
	unsigned char pl_nub;//<=3
	unsigned int offset;//save
	struct fs_ph_one_s ph[3];
};

/* vfm->private_data => dis:	*/
/* buffer->vf	=> vfm		*/
/* buffer->private_data	=> dis	*/
/* hd->idx => new->d_dis[idx]	*/
struct dis_s {
	struct c_hd_s hd;
	unsigned int dis_type;
	struct vframe_s vfm;
	struct di_buffer buffer;
	void *dis_data;
	struct c_name_s ower;
};

/* for dim_dvs_prevpp_s->dd_dvs */
struct plink_dv_s {
	struct c_cfg_blki_s blki_hd_plink;
	struct c_cfg_blki_s blki_ud_plink;
};

struct dd_s {
	/* share display */
	struct dis_s d_dis[__NUB_DIS_MAX];
	struct c_fifo_s f_dis_free;
	/* share blk */
	struct blk_s d_blk[__NUB_BLK_MAX];
	struct c_hd_s hd_blk[__NUB_BLK_MAX];
	struct c_fifo_s f_blk_idle; //not alloc buffer; 0~255
	struct c_fifo_s f_blk_t[K_BLK_T_NUB]; /* ref to enum EBLK_TYPE*/
	/* d: use this to set blk information */
	/* num for each ch's each type */
	struct c_cfg_blkt_s blkt_d[__CH_MAX];
	/* m: save blk information and use this */
	unsigned char blkt_m[__CH_MAX][K_BLK_T_NUB];
	/* m: current blk: */
	unsigned char blkt_crr[K_BLK_T_NUB];
	unsigned char blkt_tgt[K_BLK_T_NUB];
	bool	flg_blki[K_BLK_T_NUB]; //for blki enable
	struct c_cfg_blki_s blki[K_BLK_T_NUB];
	struct c_cfg_afbc_s afbc_i[K_AFBC_T_NUB];
	struct c_cfg_dct_s dct_i;
	struct c_mm_p_cnt_out_s blk_cfg[K_BLK_T_NUB];//save here only for debug
	struct plink_dv_s plink_dvs;
};

static inline bool __return_bool(bool val)
{
	return val;
}

static inline int __return_int(int val)
{
	return val;
}

static inline unsigned int __return_uint(unsigned int val)
{
	return val;
}

static inline struct dis_s *__return_dis(struct dis_s *p_dis)
{
	return p_dis;
}

static inline struct vframe_s *__return_vfm(struct vframe_s *vfm)
{
	return vfm;
}

static inline struct di_buffer *__return_buffer(struct di_buffer *buffer)
{
	return buffer;
}

/* all have hd structure can use this put */
#define qhd_put(mfifo, hd)		\
__return_bool(					\
({						\
	typeof((mfifo) + 1) __tmp2 = (mfifo);	\
	unsigned char __idx;			\
	bool __ret = false;			\
	unsigned int __ret_kput;		\
	__idx = (unsigned char)(hd)->idx;	\
	if (!__tmp2->flg_alloc) {		\
		__ret_kput = kfifo_put(&__tmp2->f, __idx);	\
		if (__ret_kput)			\
			__ret	= true;		\
	}					\
	__ret;					\
}) \
)

//----------------------------------------------------------
//2023-01-30
// hd_xx
static inline struct c_hd_s *__return_hd(struct c_hd_s *p_hd)
{
	return p_hd;
}

#define	hd_get(mfifo, base_hd)			\
__return_hd(					\
({						\
	typeof((mfifo) + 1) __f_hd = (mfifo);	\
	unsigned char __idx;			\
	unsigned int __ret_kget = 0;		\
	struct c_hd_s *__p_hd = NULL;		\
	if (__f_hd->flg_alloc) {		\
		__ret_kget = kfifo_get(&__f_hd->f, &__idx);	\
		if (!__ret_kget)		\
			__p_hd = NULL;		\
		else if (__idx >= __NUB_DIS_MAX)		\
			__p_hd = NULL;		\
		else				\
			__p_hd = (base_hd) + __idx;		\
	}					\
	__p_hd;					\
}) \
)

/* all have hd structure can use this put */
#define hd_put(mfifo, hd_st)		\
__return_bool(					\
({						\
	typeof((mfifo) + 1) __tmp2 = (mfifo);	\
	unsigned char __idx;			\
	bool __ret = false;			\
	int __ret_kput = -10;		\
	__idx = (unsigned char)(hd_st)->hd->idx;	\
	if (__tmp2->flg_alloc) {		\
		__ret_kput = kfifo_put(&__tmp2->f, __idx);	\
		if (__ret_kput)			\
			__ret	= true;		\
	}					\
	if (!__ret)				\
		PR_ERR("hd:%s:%d st:%px\n", __tmp2->name, __ret_kput, hd_st);	\
	__ret;					\
}) \
)

#define	hd_blk_get(mfifo, base_hd)	\
__return_blk(				\
({					\
	struct c_hd_s *__hd = NULL;	\
	struct blk_s *__blk	= NULL;	\
	__hd = hd_get(mfifo, base_hd);	\
	if (__hd)			\
		__blk = __hd->p;	\
	__blk;				\
}) \
)

#define blk_idle_get(dd)	\
	hd_blk_get(&(dd)->f_blk_idle, &(dd)->hd_blk[0])

#define blk_idle_put(dd, hd_st)	\
	hd_put(&(dd)->f_blk_idle, hd_st)

#define blk_o_get(dd, id)	\
	hd_blk_get(&(dd)->f_blk_t[id], &(dd)->hd_blk[0])
#define blk_o_put(dd, id, hd_st)	\
	hd_put(&(dd)->f_blk_t[id], hd_st)

#define blk_o_get_locked(dd, id) \
__return_blk( \
({ \
	typeof((dd) + 1) __new_f_hd = (dd);	\
	struct c_hd_s *__hd = NULL;	\
	struct blk_s *__blk	= NULL;	\
	int __next_idx = (id); \
	spin_lock(&__new_f_hd->f_blk_t[__next_idx].lock_r); \
	__hd = hd_get(&__new_f_hd->f_blk_t[__next_idx], &__new_f_hd->hd_blk[0]); \
	spin_unlock(&__new_f_hd->f_blk_t[__next_idx].lock_r); \
	if (__hd) \
		__blk = __hd->p; \
	__blk; \
}) \
)

#define blk_o_put_locked(dd, id, hd_st)	\
__return_bool( \
({ \
	typeof((dd) + 1) __new_tmp2 = (dd);	\
	bool __iret = false; \
	int __tmp_idx = (id); \
	spin_lock(&__new_tmp2->f_blk_t[__tmp_idx].lock_w); \
	__iret = hd_put(&__new_tmp2->f_blk_t[__tmp_idx], hd_st); \
	spin_unlock(&__new_tmp2->f_blk_t[__tmp_idx].lock_w); \
	__iret; \
}) \
)

//----------------------------------------------------------
#define qdis_put_buf(mfifo, buf)		\
__return_bool(					\
({						\
	int __ret_idx;				\
	typeof((mfifo) + 1) __tmp = (mfifo);	\
	unsigned char __idx;			\
	bool __ret = false;			\
	unsigned int __ret_kput;		\
	__ret_idx = qdis_vfm_get_hdidx(buf);	\
	if (__ret_idx != -1) {			\
		__idx = (unsigned char)__ret_idx;			\
		if (!__tmp->flg_alloc && __idx < __NUB_DIS_MAX) {	\
			__ret_kput = kfifo_put(&__tmp->f, __idx);	\
			if (__ret_kput)		\
				__ret	= true;	\
		}				\
	}					\
	__ret;					\
}) \
)

static inline struct blk_s *__return_blk(struct blk_s *p)
{
	return p;
}

/* 0: ok; other err*/
#define UCF_INIT(mfifo, sizeb, bid, bname)	\
__return_uint(					\
({						\
	unsigned int __ret = 1;			\
	int __ret_alloc;			\
	int size_l;				\
						\
	size_l = (sizeb);			\
	typeof((mfifo) + 1) __tmpc = (mfifo);	\
	spin_lock_init(&__tmpc->lock_w); \
	spin_lock_init(&__tmpc->lock_r); \
	__ret_alloc = kfifo_alloc(&__tmpc->f,	\
	    size_l,				\
	    GFP_KERNEL);			\
	if (__ret_alloc == 0) {			\
		__ret = 0;			\
		__tmpc->flg_alloc = 1;		\
		__tmpc->size = size_l;		\
		__tmpc->id	= bid;		\
		__tmpc->name	= bname;	\
	}					\
	__ret;					\
}) \
)

#define UCF_RELEASE(mfifo)			\
__return_uint(					\
({						\
	unsigned int __ret = 0;			\
	typeof((mfifo) + 1) __tmpb = (mfifo);	\
	if (__tmpb->flg_alloc)			\
		kfifo_free(&__tmpb->f);		\
	memset(&__tmpb, 0, sizeof(__tmpb));	\
	__ret;					\
}) \
)

bool dd_exit(void);
bool dd_probe(void);
int m_a_blkt_cp(unsigned int ch, unsigned char *blkt_n);
struct c_mm_p_cnt_out_s *m_rd_mm_cnt(unsigned int id);
struct c_cfg_afbc_s *m_rd_afbc(unsigned int id);
struct c_cfg_dct_s *m_rd_dct(void);

#ifdef DBG_TEST_CREATE

//only used by di_dd.c------------------------
bool dimn_que_release(struct dimn_qs_cls_s	*pq);
void dimn_que_int(struct dimn_qs_cls_s	*pq,
		  struct dimn_qs_cfg_s *cfg);

unsigned char power_of_2(unsigned int a);
bool timer_cnt(unsigned long *ptimer, unsigned int hs_nub);
int dpvpp_show_que(struct seq_file *s,
		   struct dimn_qs_cls_s *que, unsigned char level);
#endif
//---------------------------------------------
#endif	/*__DI_DD_H__*/
