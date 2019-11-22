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

void dip_even_reg_init_val(unsigned int ch);
void dip_even_unreg_val(unsigned int ch);

/************************/
/* CMA */
/************************/
void dip_wq_cma_run(unsigned char ch, bool reg_cmd);
bool dip_cma_st_is_ready(unsigned int ch);
bool dip_cma_st_is_idle(unsigned int ch);
bool dip_cma_st_is_idl_all(void);
enum eDI_CMA_ST dip_cma_get_st(unsigned int ch);
void dip_cma_st_set_ready_all(void);
void dip_cma_close(void);
const char *di_cma_dbg_get_st_name(unsigned int ch);

/*************************/
/* STATE*/
/*************************/
bool dip_event_reg_chst(unsigned int ch);
bool dip_event_unreg_chst(unsigned int ch);
void dip_chst_process_reg(unsigned int ch);

void dip_hw_process(void);

void dip_chst_process_ch(void);
bool dip_chst_change_2unreg(void);

enum eDI_TOP_STATE dip_chst_get(unsigned int ch);
const char *dip_chst_get_name_curr(unsigned int ch);
const char *dip_chst_get_name(enum eDI_TOP_STATE chst);

/**************************************
 *
 * summmary variable
 *
 **************************************/
void di_sum_reg_init(unsigned int ch);
void di_sum_set(unsigned int ch, enum eDI_SUM id, unsigned int val);
unsigned int di_sum_inc(unsigned int ch, enum eDI_SUM id);
unsigned int di_sum_get(unsigned int ch, enum eDI_SUM id);
void di_sum_get_info(unsigned int ch, enum eDI_SUM id, char **name,
		     unsigned int *pval);
unsigned int di_sum_get_tab_size(void);
bool di_sum_check(unsigned int ch, enum eDI_SUM id);

/**************************************
 *
 * cfg ctr top
 *	bool
 **************************************/
char *di_cfg_top_get_name(enum eDI_CFG_TOP_IDX idx);
void di_cfg_top_get_info(unsigned int idx, char **name);
void di_cfg_top_init_val(void);
bool di_cfg_top_get(enum eDI_CFG_TOP_IDX id);
void di_cfg_top_set(enum eDI_CFG_TOP_IDX id, bool en);

/**************************************
 *
 * cfg ctr x
 *	bool
 **************************************/
char *di_cfgx_get_name(enum eDI_CFGX_IDX idx);
void di_cfgx_get_info(enum eDI_CFGX_IDX idx, char **name);
void di_cfgx_init_val(void);
bool di_cfgx_get(unsigned int ch, enum eDI_CFGX_IDX idx);
void di_cfgx_set(unsigned int ch, enum eDI_CFGX_IDX idx, bool en);

/**************************************
 *
 * module para top
 *	int
 **************************************/
char *di_mp_uit_get_name(enum eDI_MP_UI_T idx);
void di_mp_uit_init_val(void);
int di_mp_uit_get(enum eDI_MP_UI_T idx);
void di_mp_uit_set(enum eDI_MP_UI_T idx, int val);

/**************************************
 *
 * module para x
 *	unsigned int
 **************************************/
char *di_mp_uix_get_name(enum eDI_MP_UIX_T idx);
void di_mp_uix_init_val(void);
unsigned int di_mp_uix_get(unsigned int ch, enum eDI_MP_UIX_T idx);
void di_mp_uix_set(unsigned int ch, enum eDI_MP_UIX_T idx,
		   unsigned int val);

/****************************************/
/* do_table				*/
/****************************************/
void do_table_init(struct do_table_s *pdo,
		   const struct do_table_ops_s *ptable,
		   unsigned int size_tab);
/* now only call in same thread */
void do_talbe_cmd(struct do_table_s *pdo, enum eDO_TABLE_CMD cmd);
void do_table_working(struct do_table_s *pdo);
bool do_table_is_crr(struct do_table_s *pdo, unsigned int state);

enum eDI_SUB_ID pw_ch_next_count(enum eDI_SUB_ID channel);

void dip_init_value_reg(unsigned int ch);

bool di_is_pause(unsigned int ch);
void di_pause_step_done(unsigned int ch);
void di_pause(unsigned int ch, bool on);

#endif	/*__DI_PRC_H__*/
