/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_sys.h
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

#ifndef __DI_SYS_H__
#define __DI_SYS_H__

#define DEVICE_NAME		"di_multi"
#define CLASS_NAME		"deinterlace"

u8 *dim_vmap(ulong addr, u32 size, bool *bflg);
void dim_unmap_phyaddr(u8 *vaddr);
void dim_mcinfo_v_alloc(struct di_buf_s *pbuf, unsigned int bsize);
void dim_mcinfo_v_release(struct di_buf_s *pbuf);
//void dim_mcinfo_v_alloc_idat(struct dim_iat_s *idat, unsigned int bsize);
//void dim_mcinfo_v_release_idat(struct dim_iat_s *idat);
struct dim_mm_s {
	struct page	*ppage;
	unsigned long	addr;
	unsigned int	flg;/*bit0 for tvp*/
};

bool dim_mm_alloc_api(int cma_mode, size_t count, struct dim_mm_s *o,
		      bool tvp_flg);
bool dim_mm_release_api(int cma_mode,
			struct page *pages,
			int count,
			unsigned long addr);
//bool dim_cma_top_alloc(unsigned int ch);
//bool dim_cma_top_release(unsigned int ch);
bool mm_cma_alloc(struct device *dev, size_t count,
			 struct dim_mm_s *o);

int dpst_cma_re_alloc_re_alloc(unsigned int ch);
int dpst_cma_re_alloc_unreg(unsigned int ch);
int dpst_cma_r_back_unreg(unsigned int ch);

bool dim_rev_mem_check(void);
/*--Different DI versions flag---*/
//void dil_set_diff_ver_flag(unsigned int para);

//unsigned int dil_get_diff_ver_flag(void);
void blk_polling(unsigned int ch, struct mtsk_cmd_s *cmd);

/* blk and mem 2020-06*/
void bufq_blk_int(struct di_ch_s *pch);
void bufq_blk_exit(struct di_ch_s *pch);

void bufq_pat_int(struct di_ch_s *pch);
void bufq_pat_exit(struct di_ch_s *pch);
bool qpat_idle_to_ready(struct di_ch_s *pch, unsigned long addr);
struct dim_pat_s *qpat_out_ready(struct di_ch_s *pch);
bool qpat_in_ready(struct di_ch_s *pch, struct dim_pat_s *pat_buf);
bool qpat_in_ready_msk(struct di_ch_s *pch, unsigned int msk);
bool qpat_sct_2_ready(struct di_ch_s *pch, struct dim_pat_s *pat_buf);
void pat_release_vaddr(struct dim_pat_s *pat);
void pat_clear_mem(struct device *dev,
		struct di_ch_s *pch,
		struct dim_pat_s *pat_buf);
void pat_frash(struct device *dev,
		struct di_ch_s *pch,
		struct dim_pat_s *pat_buf);

void dbg_pat(struct seq_file *s, struct di_ch_s *pch);

void bufq_iat_int(struct di_ch_s *pch);
void bufq_iat_exit(struct di_ch_s *pch);
void bufq_iat_rest(struct di_ch_s *pch);
bool qiat_idle_to_ready(struct di_ch_s *pch, struct dim_iat_s **idat);
struct dim_iat_s *qiat_out_ready(struct di_ch_s *pch);
bool qiat_in_ready(struct di_ch_s *pch, struct dim_iat_s *iat_buf);
bool qiat_all_back2_ready(struct di_ch_s *pch);

void bufq_sct_int(struct di_ch_s *pch);
void bufq_sct_exit(struct di_ch_s *pch);
void bufq_sct_rest(struct di_ch_s *pch);
bool qsct_idle_to_req(struct di_ch_s *pch,
			struct dim_sct_s **sct);
bool qsct_req_to_idle(struct di_ch_s *pch,
			struct dim_sct_s **sct);

bool qsct_req_to_ready(struct di_ch_s *pch,
			struct dim_sct_s **sct);
bool qsct_ready_to_used(struct di_ch_s *pch,
			struct dim_sct_s **sct);
bool qsct_used_some_to_recycle(struct di_ch_s *pch, struct dim_sct_s *sct_buf);
bool qsct_used_some_to_keep(struct di_ch_s *pch, struct dim_sct_s *sct_buf);
//bool qsct_used_to_recycle(struct di_ch_s *pch,
//			struct dim_sct_s **sct);
bool qsct_any_to_recycle(struct di_ch_s *pch,
			enum QBF_SCT_Q_TYPE qtype,
			struct dim_sct_s **sct);

struct dim_sct_s *qsct_req_peek(struct di_ch_s *pch);
struct dim_sct_s *qsct_peek(struct di_ch_s *pch, unsigned int q);
bool qsct_recycle_to_idle(struct di_ch_s *pch,
			struct dim_sct_s **sct);

void dbg_sct_used(struct seq_file *s, struct di_ch_s *pch);

void bufq_mem_int(struct di_ch_s *pch);
void bufq_mem_exit(struct di_ch_s *pch);

bool di_pst_afbct_check(struct di_ch_s *pch);
bool di_i_dat_check(struct di_ch_s *pch);
bool mem_alloc_check(struct di_ch_s *pch);

bool mem_cfg_pre(struct di_ch_s *pch);
bool mem_cfg_2local(struct di_ch_s *pch);
bool mem_cfg_2pst(struct di_ch_s *pch);

bool mem_cfg(struct di_ch_s *pch);
void mem_release(struct di_ch_s *pch, struct dim_mm_blk_s **blks, unsigned int blk_nub);
bool mem_2_blk(struct di_ch_s *pch);
void mem_release_all_inused(struct di_ch_s *pch);
void mem_release_one_inused(struct di_ch_s *pch, struct dim_mm_blk_s *blk_buf);
unsigned int mem_release_keep_back(struct di_ch_s *pch);
void dim_post_keep_back_recycle(struct di_ch_s *pch);

bool mem_cfg_realloc(struct di_ch_s *pch); /*temp for re-alloc mem*/
unsigned int  mem_release_free(struct di_ch_s *pch);
unsigned int mem_release_sct_wait(struct di_ch_s *pch);

void mem_cfg_realloc_wait(struct di_ch_s *pch);

void bufq_mem_clear(struct di_ch_s *pch);
void pre_sec_alloc(struct di_ch_s *pch, unsigned int flg);
void pst_sec_alloc(struct di_ch_s *pch, unsigned int flg);

int codec_mm_keeper_unmask_keeper(int keep_id, int delayms);
bool dim_blk_tvp_is_sct(struct dim_mm_blk_s *blk_buf);
bool dim_blk_tvp_is_out(struct dim_mm_blk_s *blk_buf);
void di_buf_no2wait(struct di_ch_s *pch, unsigned int post_nub);
bool mem_cfg_pst(struct di_ch_s *pch);
void mem_resize_buf(struct di_ch_s *pch, struct di_buf_s *di_buf);

/*-------------------------*/
#endif	/*__DI_SYS_H__*/
