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
void di_cfg_cp_ch(/*struct di_ch_s *pch*/ unsigned char *cfg_cp);
unsigned char di_cfg_cp_get(struct di_ch_s *pch,
			enum EDI_CFG_TOP_IDX id);
void di_cfg_cp_set(struct di_ch_s *pch,
		   enum EDI_CFG_TOP_IDX id,
		   unsigned char val);
#ifdef __HIS_CODE__
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
#endif
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
unsigned int dim_polic_is_prvpp_bypass(void);//for pvpp link

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

bool dim_config_crc_icl(void);

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

//void sct_alloc_in_poling(unsigned int ch);
//void sct_mng_working(struct di_ch_s *pch);
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
struct dim_nins_s *nins_peek_pre(struct di_ch_s *pch);
struct dim_nins_s *nins_get(struct di_ch_s *pch);
struct vframe_s *nins_peekvfm(struct di_ch_s *pch);
struct vframe_s *nins_peekvfm_pre(struct di_ch_s *pch);
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
void dcntr_check_bypass(struct vframe_s *vfm);
void dcntr_dis(void);
void dcntr_set(void);
void dcntr_pq_tune(struct dim_rpt_s *rpt);
struct dim_rpt_s *dim_api_getrpt(struct vframe_s *vfm);
void dim_pqrpt_init(struct dim_rpt_s *rpt);

void di_pq_db_setting(enum DIM_DB_SV idx);

int  dbg_dct_mif_show(struct seq_file *s, void *v);
int dbg_dct_core_show(struct seq_file *s, void *v);

int dbg_dct_contr_show(struct seq_file *s, void *v);

void dim_dbg_dct_info(struct dcntr_mem_s *pprecfg);
/* dct pre */
void dct_pre_prob(struct platform_device *pdev);
void dct_pre_revome(struct platform_device *pdev);

int dct_pre_ch_show(struct seq_file *s, void *v);
int dct_pre_reg_show(struct seq_file *s, void *v);
int dct_pre_show(struct seq_file *s, void *v);

/* for dct pre move */
struct dim_nins_s *nins_dct_get(struct di_ch_s *pch);
struct dim_nins_s *nins_dct_get_bypass(struct di_ch_s *pch);
bool nins_dct_2_done(struct di_ch_s *pch, struct dim_nins_s *nins);

/* hdr */
void dim_hdr_prob(void);
void dim_hdr_remove(void);
const struct di_hdr_ops_s *dim_hdr_ops(void);
int dim_dbg_hdr_reg1(struct seq_file *s, void *v, unsigned int indx);
int dim_dbg_hdr_para_show(struct seq_file *s, void *v);

/* double write and dvf */
void dim_dvf_cp(struct dvfm_s *dvfm, struct vframe_s *vfm, unsigned int indx);
void dim_dvf_type_p_chage(struct dvfm_s *dvfm, unsigned int type);
void dim_dvf_config_canvas(struct dvfm_s *dvfm);

struct dw_s *dim_getdw(void);
void dim_dw_prob(void);
void dim_dw_reg(struct di_ch_s *pch);
void dim_dw_unreg_setting(void);
void dim_dw_pre_para_init(struct di_ch_s *pch, struct dim_nins_s *nins);
void dw_pre_sync_addr(struct dvfm_s *wdvfm, struct di_buf_s *di_buf);

/*************************************************/
struct vframe_s *vf_get_dpost_by_indx(unsigned int ch, unsigned int indx);
struct vframe_s *vf_get_nin_by_indx(unsigned int ch, unsigned int indx);
bool vf_2_subvf(struct dsub_vf_s *vfms, struct vframe_s *vfm);
bool vf_from_subvf(struct vframe_s *vfm, struct dsub_vf_s *vfms);
void vfs_print(struct dsub_vf_s *pvfs, char *name);

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
bool di_buf_clear(struct di_ch_s *pch, struct di_buf_s *di_buf);

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
bool cma_alloc_blk_block(struct dim_mm_blk_s *blk_buf,
		      unsigned int cma_type,
		      unsigned int tvp,
		      unsigned int size_page);
void cma_release_blk_block(struct dim_mm_blk_s *blk_buf,
				  unsigned int cma_type);
unsigned char dim_pre_bypass(struct di_ch_s *pch);

unsigned int dim_get_dbg_dec21(void);
bool dim_in_linear(void);
bool dim_dbg_cfg_disable_arb(void);
void dbg_vfm_w(struct vframe_s *vfm, unsigned int dbgid);
bool dbg_is_trig_eos(unsigned int ch);
void pre_inp_mif_w(struct DI_MIF_S *di_mif, struct vframe_s *vf);
unsigned int get_intr_mode(void);
void pp_unreg_hw(void);
void di_reg_setting_working(struct di_ch_s *pch,
			    struct vframe_s *vfm);
void dim_slt_init(void);
bool dim_is_slt_mode(void);
unsigned int dim_int_tab(struct device *dev,
				 struct afbce_map_s *pcfg);
void di_decontour_disable(bool on);
void dip_pps_cnt_hv(unsigned int *w_in, unsigned int *h_in);
bool dip_cfg_is_pps_4k(unsigned int ch);

unsigned int di_hf_cnt_size(unsigned int w, unsigned int h, bool is_4k);
bool di_hf_set_buffer(struct di_buf_s *di_buf, struct div2_mm_s *mm);
bool di_hf_hw_try_alloc(unsigned int who);
void di_hf_hw_release(unsigned int who);
bool di_hf_hw_is_busy(void);
bool di_hf_buf_size_set(struct DI_SIM_MIF_S *hf_mif);
bool di_hf_t_try_alloc(struct di_ch_s *pch);
void di_hf_t_release(struct di_ch_s *pch);
void di_hf_reg(struct di_ch_s *pch);
bool di_hf_size_check(struct DI_SIM_MIF_S *w_mif);
void di_hf_polling_active(struct di_ch_s *pch);

void di_hf_lock_blend_buffer_pre(struct di_buf_s *di_buf);
void di_hf_lock_blend_buffer_pst(struct di_buf_s *di_buf);
void di_hf_lock_irq_flg(void);
bool dim_mng_hf_release_all(struct di_ch_s *pch, struct dim_mm_blk_s *blk_buf);
bool dim_mng_hf_s_act(void);
bool dim_mng_hf_release(struct di_ch_s *pch, struct dim_mm_blk_s *blk_buf);
bool dim_mng_hf_alloc(struct di_ch_s *pch,
		      struct dim_mm_blk_s *blk_buf,
		      unsigned int hf_size);
unsigned int dim_mng_hf_err(void);
unsigned int dim_mng_hf_sum_alloc_get(void);
unsigned int dim_mng_hf_sum_free_get(void);
unsigned int dim_mng_hf_sum_idle_get(void);
void dim_mng_hf_prob(void);
void dim_mng_hf_exit(void);

void dim_print_hf(struct hf_info_t *phf);
void mtask_wake_m(void);
void dim_dbg_seq_hf(struct hf_info_t *hf, struct seq_file *seq);

bool dim_dbg_is_force_later(void);

void sct_polling(struct di_ch_s *pch, unsigned int pos);
void dip_out_ch(void);
bool dip_cfg_afbc_skip(void);
bool dip_cfg_afbc_pps(void);

void afbcskip_reg_variable(struct di_ch_s *pch, struct vframe_s *vframe);
bool s4dw_test_ins(void);
//extern const struct di_mm_cfg_s c_mm_cfg_s4_cp;//check no need;
extern const struct dim_s4dw_data_s dim_s4dw_def;
//unsigned char s4dw_pre_buf_config(struct di_ch_s *pch);
enum DI_ERRORTYPE s4dw_empty_input(struct di_ch_s *pch, struct di_buffer *buffer);

void dim_pps_disable(void);

unsigned char is_source_change(vframe_t *vframe, unsigned int channel);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
void pre_inp_canvas_config(struct vframe_s *vf);
#endif
void config_di_mif(struct DI_MIF_S *di_mif, struct di_buf_s *di_buf,
			  unsigned int channel);
void dim_canvas_set2(struct vframe_s *vf, u32 *index);
void config_canvas_idx(struct di_buf_s *di_buf, int nr_canvas_idx,
			      int mtn_canvas_idx);
void config_di_wr_mif(struct DI_SIM_MIF_S *di_nrwr_mif,
		 struct DI_SIM_MIF_S *di_mtnwr_mif,
		 struct di_buf_s *di_buf, unsigned int channel);
void config_di_mtnwr_mif(struct DI_SIM_MIF_S *di_mtnwr_mif,
				struct di_buf_s *di_buf);

void di_pre_size_change(unsigned short width,
			       unsigned short height,
			       unsigned short vf_type,
			       unsigned int channel);
void dim_nr_ds_hw_ctrl(bool enable);
void config_di_cnt_mif(struct DI_SIM_MIF_S *di_cnt_mif,
			      struct di_buf_s *di_buf);
#ifdef S4D_OLD_SETTING_KEEP
void config_canvas_idx_mtn(struct di_buf_s *di_buf,
				  int mtn_canvas_idx);
void config_cnt_canvas_idx(struct di_buf_s *di_buf,
				  unsigned int cnt_canvas_idx);
#endif
#ifdef TMP_S4DW_MC_EN
void config_mcinfo_canvas_idx(struct di_buf_s *di_buf,
				     int mcinfo_canvas_idx);
void config_mcvec_canvas_idx(struct di_buf_s *di_buf,
				    int mcvec_canvas_idx);
void config_di_mcinford_mif(struct DI_MC_MIF_s *di_mcinford_mif,
		       struct di_buf_s *di_buf);
void config_di_pre_mc_mif(struct DI_MC_MIF_s *di_mcinfo_mif,
		     struct DI_MC_MIF_s *di_mcvec_mif,
		     struct di_buf_s *di_buf);
#endif /* TMP_S4DW_MC_EN */

void dpre_recyc(unsigned int ch);

extern const struct do_table_ops_s s4dw_hw_processt[4];
void s4dw_parser_infor(struct di_ch_s *pch);
void check_tvp_state(struct di_ch_s *pch);

void s4dw_hpre_check_pps(void);
void dim_dbg_buffer_flow(struct di_ch_s *pch,
			 unsigned long addr,
			 unsigned long addr2,
			 unsigned int pos);
void dim_dbg_buffer_ext(struct di_ch_s *pch,
			struct di_buffer *buffer,
			unsigned int pos);
void dim_dbg_vf_cvs(struct di_ch_s *pch,
			struct vframe_s *vfm,
			unsigned int pos);
int new_create_instance(struct di_init_parm parm);

int new_destroy_instance(int index);
enum DI_ERRORTYPE new_empty_input_buffer(int index, struct di_buffer *buffer);
enum DI_ERRORTYPE new_fill_output_buffer(int index, struct di_buffer *buffer);
int new_release_keep_buf(struct di_buffer *buffer);
int new_get_output_buffer_num(int index);
int new_get_input_buffer_num(int index);
bool dim_get_overturn(void);
int dim_pre_vpp_link_display(struct vframe_s *vfm,
			  struct pvpp_dis_para_in_s *in_para, void *out_para);
enum DI_ERRORTYPE dpvpp_fill_output_buffer(int index, struct di_buffer *buffer);
enum DI_ERRORTYPE dpvpp_empty_input_buffer(int index, struct di_buffer *buffer);
int dpvpp_destroy_instance(int index);
int dpvpp_create_instance(struct di_init_parm *parm);
int dpvpp_check_vf(struct vframe_s *vfm);
int dpvpp_check_di_act(void);
int dpvpp_sw(bool on);
unsigned int dpvpp_get_ins_id(void);

void dpvpp_prob(void);
bool dpvpp_is_allowed(void);
bool dpvpp_is_insert(void);
bool dpvpp_is_en_polling(void);
bool dpvpp_try_reg(struct di_ch_s *pch, struct vframe_s *vfm);
int dpvpp_destroy_internal(int index);
const struct vframe_operations_s *dpvpp_vf_ops(void);

const struct dimn_pvpp_ops_s *dpvpp_ops(void);
const struct dimn_pvpp_ops_api_s *dpvpp_ops_api(void);
void cvs_link(struct dim_cvspara_s *pcvsi, char *name);
unsigned int cvs_nub_get(unsigned int idx, char *name);
bool dim_check_exit_process(void);
bool dim_is_creat_p_vpp_link(void);
void dvpp_dbg_trig_sw(unsigned int cmd);
int di_ls_bypass_ch(int index, bool on);
#endif	/*__DI_PRC_H__*/
