/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_prc.h
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

#ifndef __DI_PRC_H__
#define __DI_PRC_H__

bool dip_prob(void);
void dip_exit(void);
void dip_prob_ch(void);

void dip_even_reg_init_val(unsigned int ch);
void dip_even_unreg_val(unsigned int ch);

/************************/
/* CMA */
/************************/
enum ECMA_CMD {
	ECMA_CMD_RELEASE,
	ECMA_CMD_ALLOC,
	ECMA_CMD_BACK, /*from post*/
	ECMA_CMD_ONE_RE_AL,
	ECMA_CMD_ONE_RELEAS,
};

enum ECFG_DIM {
	ECFG_DIM_BYPASS_P,
	ECFG_DIM_BYPASS_OTHER,
};

//void dip_wq_cma_run(unsigned char ch, unsigned int reg_cmd);
//bool dip_cma_st_is_ready(unsigned int ch);
//bool dip_cma_st_is_idle(unsigned int ch);
void di_cfg_set(enum ECFG_DIM idx, unsigned int data);
bool dip_cma_st_is_idl_all(void);
enum EDI_CMA_ST dip_cma_get_st(unsigned int ch);
void dip_cma_st_set_ready_all(void);
void dip_cma_close(void);
const char *di_cma_dbg_get_st_name(unsigned int ch);
//void dip_wq_check_unreg(unsigned int ch);

/*************************/
/* STATE*/
/*************************/
//bool dip_event_reg_chst(unsigned int ch);
//bool dip_event_unreg_chst(unsigned int ch);
//void dip_chst_process_reg(unsigned int ch);
bool dim_process_reg(struct di_ch_s *pch);
bool dim_process_unreg(struct di_ch_s *pch);

void dip_hw_process(void);

void dip_chst_process_ch(void);
//bool dip_chst_change_2unreg(void);

enum EDI_TOP_STATE dip_chst_get(unsigned int ch);
const char *dip_chst_get_name_curr(unsigned int ch);
const char *dip_chst_get_name(enum EDI_TOP_STATE chst);

/**************************************
 *
 * summmary variable
 *
 **************************************/
void di_sum_reg_init(unsigned int ch);
void di_sum_set(unsigned int ch, enum EDI_SUM id, unsigned int val);
unsigned int di_sum_inc(unsigned int ch, enum EDI_SUM id);
unsigned int di_sum_get(unsigned int ch, enum EDI_SUM id);
void di_sum_get_info(unsigned int ch, enum EDI_SUM id, char **name,
		     unsigned int *pval);
unsigned int di_sum_get_tab_size(void);
bool di_sum_check(unsigned int ch, enum EDI_SUM id);

/**************************************
 *
 * cfg ctr top
 *	bool
 **************************************/
bool di_cfg_top_check(unsigned int idx);
char *di_cfg_top_get_name(enum EDI_CFG_TOP_IDX idx);
void di_cfg_top_get_info(unsigned int idx, char **name);
void di_cfg_top_init_val(void);
void di_cfg_top_dts(void);
unsigned int di_cfg_top_get(enum EDI_CFG_TOP_IDX id);
void di_cfg_top_set(enum EDI_CFG_TOP_IDX id, unsigned int en);
void di_cfgt_show_item_sel(struct seq_file *s);
void di_cfgt_show_item_all(struct seq_file *s);
void di_cfgt_show_val_sel(struct seq_file *s);
void di_cfgt_show_val_all(struct seq_file *s);
void di_cfgt_set_sel(unsigned int dbg_mode, unsigned int id);
void di_cfg_cp_ch(struct di_ch_s *pch);
unsigned char di_cfg_cp_get(struct di_ch_s *pch,
			enum EDI_CFG_TOP_IDX id);
void di_cfg_cp_set(struct di_ch_s *pch,
		   enum EDI_CFG_TOP_IDX id,
		   unsigned char val);

/**************************************
 *
 * cfg ctr x
 *	bool
 **************************************/
char *di_cfgx_get_name(enum EDI_CFGX_IDX idx);
void di_cfgx_get_info(enum EDI_CFGX_IDX idx, char **name);
void di_cfgx_init_val(void);
bool di_cfgx_get(unsigned int ch, enum EDI_CFGX_IDX idx);
void di_cfgx_set(unsigned int ch, enum EDI_CFGX_IDX idx, bool en);

/**************************************
 *
 * module para top
 *	int
 **************************************/
char *di_mp_uit_get_name(enum EDI_MP_UI_T idx);
void di_mp_uit_init_val(void);
int di_mp_uit_get(enum EDI_MP_UI_T idx);
void di_mp_uit_set(enum EDI_MP_UI_T idx, int val);

/**************************************
 *
 * module para x
 *	unsigned int
 **************************************/
char *di_mp_uix_get_name(enum EDI_MP_UIX_T idx);
void di_mp_uix_init_val(void);
unsigned int di_mp_uix_get(unsigned int ch, enum EDI_MP_UIX_T idx);
void di_mp_uix_set(unsigned int ch, enum EDI_MP_UIX_T idx,
		   unsigned int val);

/****************************************
 *bit control
 ****************************************/
void bset(unsigned int *p, unsigned int bitn);
void bclr(unsigned int *p, unsigned int bitn);
bool bget(unsigned int *p, unsigned int bitn);

/****************************************/
/* do_table				*/
/****************************************/
void do_table_init(struct do_table_s *pdo,
		   const struct do_table_ops_s *ptable,
		   unsigned int size_tab);
/* now only call in same thread */
void do_talbe_cmd(struct do_table_s *pdo, enum EDO_TABLE_CMD cmd);
void do_table_working(struct do_table_s *pdo);
bool do_table_is_crr(struct do_table_s *pdo, unsigned int state);

enum EDI_SUB_ID pw_ch_next_count(enum EDI_SUB_ID channel);

/************************************************
 * bypass state
 ************************************************/
void dim_bypass_st_clear(struct di_ch_s *pch);
void dim_bypass_set(struct di_ch_s *pch, bool which, unsigned int reason);
/************************************************
 * bypass polic
 ************************************************/
void dim_polic_prob(void);
void dim_polic_unreg(struct di_ch_s *pch);
unsigned int dim_polic_is_bypass(struct di_ch_s *pch, struct vframe_s *vf);
void dim_polic_cfg_local(unsigned int cmd, bool on);

unsigned int dim_get_trick_mode(void);

/************************************************
 * reg / unreg 2020-06-12
 ************************************************/
bool dim_api_reg(enum DIME_REG_MODE rmode, struct di_ch_s *pch);
bool dim_api_unreg(enum DIME_REG_MODE rmode, struct di_ch_s *pch);
void dim_trig_unreg(unsigned int ch);

void dip_init_value_reg(unsigned int ch, struct vframe_s *vframe);
enum EDI_SGN di_vframe_2_sgn(struct vframe_s *vframe);
const struct di_mm_cfg_s *di_get_mm_tab(unsigned int is_4k,
					struct di_ch_s *pch);

/************************************************
 * sct
 ************************************************/
void tst_alloc(struct di_ch_s *pch);
void tst_resize(struct di_ch_s *pch, unsigned int used_size);
void tst_release(struct di_ch_s *pch);
void tst_unreg(struct di_ch_s *pch);
void tst_reg(struct di_ch_s *pch);

void sct_prob(struct di_ch_s *pch);
void sct_sw_on(struct di_ch_s *pch,
		unsigned int max_num,
		bool tvp,
		unsigned int buffer_size);
void sct_sw_off_rebuild(struct di_ch_s *pch);

void sct_alloc_in_poling(unsigned int ch);
void sct_mng_working(struct di_ch_s *pch);
void sct_mng_working_recycle(struct di_ch_s *pch);

void sct_mng_idle(struct di_ch_s *pch);
void sct_free_l(struct di_ch_s *pch, struct dim_sct_s *sct);
void sct_free_tail_l(struct di_ch_s *pch,
		      unsigned int buffer_used,
		      struct dim_sct_s *sct);
unsigned int sct_keep(struct di_ch_s *pch, struct dim_sct_s *sct);
void sct_sw_off(struct di_ch_s *pch);
int dim_dbg_sct_top_show(struct seq_file *s, void *what);

void bufq_nin_int(struct di_ch_s *pch);
void bufq_nin_exit(struct di_ch_s *pch);
void bufq_nin_reg(struct di_ch_s *pch);
struct dim_nins_s *nins_peek(struct di_ch_s *pch);
struct dim_nins_s *nins_get(struct di_ch_s *pch);
struct vframe_s *nins_peekvfm(struct di_ch_s *pch);
bool nins_out_some(struct di_ch_s *pch,
		   struct dim_nins_s *ins,
		   unsigned int q);
bool nins_used_some_to_recycle(struct di_ch_s *pch,
				struct dim_nins_s *ins);
struct dim_nins_s *nins_move(struct di_ch_s *pch,
			     unsigned int qf,
			     unsigned int qt);
unsigned int nins_cnt_used_all(struct di_ch_s *pch);
unsigned int nins_cnt(struct di_ch_s *pch, unsigned int q);

struct vframe_s *ndrd_qout(struct di_ch_s *pch);
void dbg_nins_log_buf(struct di_buf_s *di_buf, unsigned int dbgid);
void dbg_nins_check_id(struct di_ch_s *pch);

void bufq_ndis_int(struct di_ch_s *pch);
void bufq_ndis_exit(struct di_ch_s *pch);

bool ndis_fill_ready(struct di_ch_s *pch, struct di_buf_s *di_buf);
bool ndrd_m1_fill_ready(struct di_ch_s *pch, struct di_buf_s *di_buf);
bool ndis_is_in_display(struct di_ch_s *pch, struct dim_ndis_s *ndis);
bool ndis_move_display2idle(struct di_ch_s *pch, struct dim_ndis_s *ndis);
struct dim_ndis_s *ndis_get_fromid(struct di_ch_s *pch, unsigned int idx);
unsigned int ndis_cnt(struct di_ch_s *pch, unsigned int que);
struct dim_ndis_s *ndis_move(struct di_ch_s *pch,
			     unsigned int qf,
			     unsigned int qt);
unsigned int ndis_2keep(struct di_ch_s *pch,
		struct dim_mm_blk_s **blk,
		unsigned int len_max,
		unsigned int disable_mirror);
void bufq_ndis_unreg(struct di_ch_s *pch);
bool ndis_is_in_keep(struct di_ch_s *pch, struct dim_ndis_s *ndis);
bool ndis_move_keep2idle(struct di_ch_s *pch, struct dim_ndis_s *ndis);
void ndis_dbg_qbuf_detail(struct seq_file *s, struct di_ch_s *pch);
void ndis_dbg_print2(struct dim_ndis_s *ndis, char *name);

void ndrd_int(struct di_ch_s *pch);
void ndrd_exit(struct di_ch_s *pch);
unsigned int ndrd_cnt(struct di_ch_s *pch);

struct vframe_s *ndrd_qpeekvfm(struct di_ch_s *pch);
struct di_buf_s *ndrd_qpeekbuf(struct di_ch_s *pch);
void ndrd_dbg_list_buf(struct seq_file *s, struct di_ch_s *pch);
void ndrd_qin(struct di_ch_s *pch, void *vfm);
void ndrd_reset(struct di_ch_s *pch);

void dip_itf_ndrd_ins_m2_out(struct di_ch_s *pch);
void dip_itf_ndrd_ins_m1_out(struct di_ch_s *pch);

void ndkb_qin(struct di_ch_s *pch, struct dim_ndis_s *ndis);
struct dim_ndis_s *ndkb_qout(struct di_ch_s *pch);
unsigned int ndkb_cnt(struct di_ch_s *pch);
void ndkb_qin_byidx(struct di_ch_s *pch, unsigned int idx);
void ndkb_dbg_list(struct seq_file *s, struct di_ch_s *pch);
void npst_int(struct di_ch_s *pch);
void npst_exit(struct di_ch_s *pch);
void npst_reset(struct di_ch_s *pch);
/*  */
void npst_qin(struct di_ch_s *pch, void *buffer);
/* @ary_note:  */
struct di_buffer *npst_qpeek(struct di_ch_s *pch);
struct di_buffer *npst_qout(struct di_ch_s *pch);
unsigned int npst_cnt(struct di_ch_s *pch);
void dbg_log_pst_buffer(struct di_buf_s *di_buf, unsigned int dbgid);

void dip_itf_vf_op_polling(struct di_ch_s *pch);
void dip_itf_back_input(struct di_ch_s *pch);
struct dev_vfm_s *dip_itf_vf_sop(struct di_ch_s *pch);
bool dip_itf_is_vfm(struct di_ch_s *pch);
bool dip_itf_is_ins(struct di_ch_s *pch);
bool dip_itf_is_ins_lbuf(struct di_ch_s *pch);
bool dip_itf_is_ins_exbuf(struct di_ch_s *pch);
bool dip_itf_is_o_linear(struct di_ch_s *pch);
void dbg_itf_tmode(struct di_ch_s *pch, unsigned int pos);
void dim_dbg_buffer(struct di_buffer *buffer, unsigned int id);

unsigned int nins_cnt_used_all(struct di_ch_s *pch);

bool di_is_pause(unsigned int ch);
void di_pause_step_done(unsigned int ch);
void di_pause(unsigned int ch, bool on);

void dim_sumx_clear(unsigned int ch);
void dim_sumx_set(struct di_ch_s *pch);

void dim_mp_update_reg(void);
void dim_mp_update_post(void);

void dip_init_pq_ops(void);
bool dbg_src_change_simple(unsigned int ch);
bool dbg_checkcrc(struct di_buf_s *di_buf, unsigned int cnt);

void dbg_cp_4k(struct di_ch_s *pch, unsigned int mode);
void dbg_afbc_update_level1(struct vframe_s *vf,
			    enum EAFBC_DEC dec);//debug for copy
void dbg_afbce_update_level1(struct vframe_s *vf,
			     const struct reg_acc *op,
			     enum EAFBC_ENC enc);
void pre_cfg_cvs(struct vframe_s *vf);//debug only
void dbg_pip_func(struct di_ch_s *pch, unsigned int mode);
bool dip_is_support_4k(unsigned int ch);
bool dip_is_support_nv2110(unsigned int ch);
bool dip_is_4k_sct_mem(struct di_ch_s *pch);
bool dip_is_ponly_sct_mem(struct di_ch_s *pch);

void dim_vf_x_y(struct vframe_s *vf, unsigned int *x, unsigned int *y);

/************************************************/
void dcntr_prob(void);
void dcntr_reg(unsigned int on);
void dcntr_check(struct vframe_s *vfm);
void dcntr_dis(void);
void dcntr_set(void);
void dcntr_pq_tune(struct dim_rpt_s *rpt);
struct dim_rpt_s *dim_api_getrpt(struct vframe_s *vfm);
void dim_pqrpt_init(struct dim_rpt_s *rpt);

void di_pq_db_setting(enum DIM_DB_SV idx);

int  dbg_dct_mif_show(struct seq_file *s, void *v);
int dbg_dct_core_show(struct seq_file *s, void *v);

int dbg_dct_contr_show(struct seq_file *s, void *v);

void dbg_regs_tab(struct seq_file *s, const struct regs_t *pregtab,
		  const unsigned int *padd);//debug only
void dbg_reg_tab(struct seq_file *s, const struct reg_t *pregtab);//debug only
void dbg_q_listid(struct seq_file *s, struct buf_que_s *pqbuf);
void dbg_blk(struct seq_file *s, struct dim_mm_blk_s *blk_buf);
bool dbg_sct_used_decoder_buffer(void);
bool dbg_sct_clear_by_frame(void);
void dbg_q_list_qbuf(struct seq_file *s, struct buf_que_s *pqbuf);
void dbg_q_list_qbuf_print(struct buf_que_s *pqbuf);
void dbg_q_list_qbuf_buffer(struct seq_file *s, struct buf_que_s *pqbuf);
void vfmtst_init(void);
void vfmtst_exit(void);
void dtst_prob(void);
void dtst_exit(void);

void dbg_buffer(struct seq_file *s, void *in);
void dbg_buffer_print(void *in);
//int dim_dbg_tst_in_show(struct seq_file *s, void *what);
//void qbuf_dbg_check_in_buffer_id(unsigned int dbgid);
bool tst_tmp_is_extbuf(void);
bool dim_is_dbg_tabe(void);
void dip_sum_post_ch(void);

void dbg_hd(struct seq_file *s, struct qs_buf_s *header);
void dbg_hd_print(struct qs_buf_s *header);
void print_di_buf_seq(struct di_buf_s *di_buf, int format,
			     struct seq_file *seq);
void dbg_q_listid_print(struct buf_que_s *pqbuf);
void dim_dbg_buffer2(struct di_buffer *buffer, unsigned int id);
bool dim_dbg_new_int(unsigned int id);
#ifdef TST_NEW_INS_INTERFACE
int dim_dbg_tst_in_show(struct seq_file *s, void *what);

#endif
unsigned int dim_get_dbg_dec21(void);
bool dim_in_linear(void);
bool dim_dbg_cfg_disable_arb(void);
void dbg_vfm_w(struct vframe_s *vfm, unsigned int dbgid);
bool dbg_is_trig_eos(unsigned int ch);
void pre_inp_mif_w(struct DI_MIF_S *di_mif, struct vframe_s *vf);

#endif	/*__DI_PRC_H__*/
