/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_DATA_L_H__
#define __DI_DATA_L_H__

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#include <linux/kfifo.h>	/*ary add*/

#include "../deinterlace/di_pqa.h"

#define DI_CHANNEL_NUB	(2)
#define DI_CHANNEL_MAX  (4)

#define TABLE_FLG_END	(0xfffffffe)
#define TABLE_LEN_MAX	(1000)
#define F_IN(x, a, b)	(((x) > (a)) && ((x) < (b)))
#define COM_M(m, a, b)	(((a) & (m)) == ((b) & (m)))
#define COM_MV(a, m, v)	(((a) & (m)) == (v))
#define COM_ME(a, m)	(((a) & (m)) == (m))

#define DI_BIT0		0x00000001
#define DI_BIT1		0x00000002
#define DI_BIT2		0x00000004
#define DI_BIT3		0x00000008
#define DI_BIT4		0x00000010
#define DI_BIT5		0x00000020
#define DI_BIT6		0x00000040
#define DI_BIT7		0x00000080
#define DI_BIT8		0x00000100
#define DI_BIT9		0x00000200
#define DI_BIT10	0x00000400
#define DI_BIT11	0x00000800
#define DI_BIT12	0x00001000
#define DI_BIT13	0x00002000
#define DI_BIT14	0x00004000
#define DI_BIT15	0x00008000
#define DI_BIT16	0x00010000
#define DI_BIT17	0x00020000
#define DI_BIT18	0x00040000
#define DI_BIT19	0x00080000
#define DI_BIT20	0x00100000
#define DI_BIT21	0x00200000
#define DI_BIT22	0x00400000
#define DI_BIT23	0x00800000
#define DI_BIT24	0x01000000
#define DI_BIT25	0x02000000
#define DI_BIT26	0x04000000
#define DI_BIT27	0x08000000
#define DI_BIT28	0x10000000
#define DI_BIT29	0x20000000
#define DI_BIT30	0x40000000
#define DI_BIT31	0x80000000

/*****************************************
 *
 * vframe mask
 *
 *****************************************/

#define DI_VFM_T_MASK_CHANGE		\
	(VIDTYPE_VIU_422		\
	| VIDTYPE_VIU_SINGLE_PLANE	\
	| VIDTYPE_VIU_444		\
	| VIDTYPE_INTERLACE		\
	| VIDTYPE_COMPRESS		\
	| VIDTYPE_MVC)

enum EDI_MEM_M {
	EDI_MEM_M_REV = 0,
	EDI_MEM_M_CMA = 1,
	EDI_MEM_M_CMA_ALL = 2,
	EDI_MEM_M_CODEC_A = 3,
	EDI_MEM_M_CODEC_B = 4,
	EDI_MEM_M_MAX	/**/
};

/* ************************************** */
/* *************** cfg top ************** */
/* ************************************** */
/* also see: di_cfg_top_ctr*/

enum EDI_CFG_TOP_IDX {
	/* cfg for top */
	EDI_CFG_BEGIN,
	EDI_CFG_MEM_FLAG,
	EDI_CFG_FIRST_BYPASS,
	EDI_CFG_REF_2,
	EDI_CFG_KEEP_CLEAR_AUTO,
	EDI_CFG_END,

};

#define cfgeq(a, b) ((di_cfg_top_get(EDI_CFG_##a) == (b)) ? true : false)
#define cfgnq(a, b) ((di_cfg_top_get(EDI_CFG_##a) != (b)) ? true : false)
#define cfgg(a) di_cfg_top_get(EDI_CFG_##a)
#define cfgs(a, b) di_cfg_top_set(EDI_CFG_##a, b)
#define K_DI_CFG_NUB	(EDI_CFG_END - EDI_CFG_BEGIN + 1)
#define K_DI_CFG_T_FLG_NOTHING	(0x00)
#define K_DI_CFG_T_FLG_DTS	(0x01)
#define K_DI_CFG_T_FLG_NONE	(0x80)
union di_cfg_tdata_u {
	unsigned int d32;
	struct {
		unsigned int val_df:4,/**/
		val_dts:4,
		val_dbg:4,
		val_c:4,
		dts_en:1,
		dts_have:1,
		dbg_have:1,
		rev_en:1,
		en_update:4,
		reserved:8;
	} b;
};

struct di_cfg_ctr_s {
	char *dts_name;
	enum EDI_CFG_TOP_IDX id;
	unsigned char	default_val;
	unsigned char	flg;
};

/* ************************************** */
/* *************** cfg x *************** */
/* ************************************** */
/*also see di_cfgx_ctr*/
enum EDI_CFGX_IDX {
	/* cfg channel x*/
	EDI_CFGX_BEGIN,
	EDI_CFGX_BYPASS_ALL,	/*bypass_all*/
	EDI_CFGX_END,

	/* debug cfg x */
	EDI_DBG_CFGX_BEGIN,
	EDI_DBG_CFGX_IDX_VFM_IN,
	EDI_DBG_CFGX_IDX_VFM_OT,
	EDI_DBG_CFGX_END,
};

#define K_DI_CFGX_NUB	(EDI_DBG_CFGX_END - EDI_CFGX_BEGIN + 1)

struct di_cfgx_ctr_s {
	char *name;
	enum EDI_CFGX_IDX id;
	bool	default_val;
};

/* ****************************** */
enum EDI_SUB_ID {
	DI_SUB_ID_S0,	/*DI_SUB_ID_MARST,*/
	DI_SUB_ID_S1,
	DI_SUB_ID_S2,
	DI_SUB_ID_S3,
	/*DI_SUB_ID_NUB,*/
};

/*debug vframe type */
struct di_vframe_type_info {
	char *name;
	unsigned int mask;
	char *other;
};

struct di_dbg_datax_s {
	struct vframe_s vfm_input;	/*debug input vframe*/
	struct vframe_s *pfm_out;	/*debug di_get vframe*/

};

/*debug function*/
enum EDI_DBG_F {
	EDI_DBG_F_00,
	EDI_DBG_F_01,
	EDI_DBG_F_02,
	EDI_DBG_F_03,
	EDI_DBG_F_04,
	EDI_DBG_F_05,
	EDI_DBG_F_06,
	EDI_DBG_F_07,
	EDI_DBG_F_08,
};

struct di_dbg_func_s {
	enum EDI_DBG_F index;
	void (*func)(unsigned int para);
	char *name;
	char *info;
};

/*register*/
struct reg_t {
	unsigned int add;
	unsigned int bit;
	unsigned int wid;
/*	unsigned int id;*/
	unsigned int df_val;
	char *name;
	char *bname;
	char *info;
};

struct reg_acc {
	void (*wr)(unsigned int adr, unsigned int val);
	unsigned int (*rd)(unsigned int adr);
	unsigned int (*bwr)(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len);
	unsigned int (*brd)(unsigned int adr, unsigned int start,
			    unsigned int len);
};

/**************************************/
/* time out */
/**************************************/

enum EDI_TOUT_CONTR {
/*	eDI_TOUT_CONTR_INT,*/
	EDI_TOUT_CONTR_EN,
	EDI_TOUT_CONTR_FINISH,
	EDI_TOUT_CONTR_CHECK,

	EDI_TOUT_CONTR_CLEAR,
	EDI_TOUT_CONTR_RESET,
};

struct di_time_out_s {
	bool en;
	unsigned long	timer_start;
	unsigned int	timer_thd;
	unsigned int	over_flow_cnt;
	bool		flg_over;
/*	bool (*do_func)(void);*/
};

struct di_func_tab_s {
	unsigned int index;
	bool (*func)(void);
};

/****************************************/
/* do_table				*/
/****************************************/

/*for do_table_ops_s id*/
#define K_DO_TABLE_ID_PAUSE		0
#define K_DO_TABLE_ID_STOP		1
#define K_DO_TABLE_ID_START		2

/*for mark of do_table_ops_s id*/
#define K_DO_TABLE_CAN_STOP		0x01

/*for op ret*/
#define K_DO_TABLE_R_B_FINISH		0x01/*bit 0: 0:not finish, 1:finish*/
/*bit 1: 0: to other index; 1: to next*/
#define K_DO_TABLE_R_B_NEXT		0x02
#define K_DO_TABLE_R_B_OTHER		0xf0	/*bit [7:4]: other index*/
#define K_DO_TABLE_R_B_OTHER_SHIFT	4	/*bit [7:4]: other index*/

#define K_DO_R_FINISH	(K_DO_TABLE_R_B_FINISH | K_DO_TABLE_R_B_NEXT)
#define K_DO_R_NOT_FINISH	(0)
#define K_DO_R_JUMP(a)	(K_DO_TABLE_R_B_FINISH |	\
	(((a) << K_DO_TABLE_R_B_OTHER_SHIFT) & K_DO_TABLE_R_B_OTHER))

enum EDO_TABLE_CMD {
	EDO_TABLE_CMD_NONE,
	EDO_TABLE_CMD_STOP,
	EDO_TABLE_CMD_START,
	EDO_TABLE_CMD_PAUSE,
	EDO_TABLE_CMD_STEP,
	EDO_TABLE_CMD_STEP_BACK,
};

struct do_table_ops_s {
	/*bool stop_mark;*/
	unsigned int id;
	unsigned int mark;	/*stop / pause*/
	bool (*con)(void *data);/*condition*/
	unsigned int (*do_op)(void *data);
	unsigned int (*do_stop_op)(void *data);
	char *name;

};

struct do_table_s {
	const struct do_table_ops_s *ptab;
	unsigned int size;
	unsigned int op_lst;
	unsigned int op_crr;
	void *data;
	bool do_stop;	/*need do stops	*/
	bool flg_stop;	/*have stop	*/

	bool do_pause;
	bool do_step;	/*step mode*/

	bool flg_repeat;
	char *name;

};

/*************************************/

/**********************/
/* vframe info */
/**********************/
struct di_vinfo_s {
	/*use this for judge type change or not */
	unsigned int ch;
	unsigned int vtype;
	unsigned int src_type;
	unsigned int trans_fmt;
	unsigned int h;
	unsigned int v;
};

/**************************************/
/* PRE */
/**************************************/
enum EDI_PRE_ST {
	EDI_PRE_ST_EXIT,
	EDI_PRE_ST_IDLE,	/*switch to next channel?*/
	EDI_PRE_ST_CHECK,
	EDI_PRE_ST_SET,
	EDI_PRE_ST_WAIT_INT,
	EDI_PRE_ST_TIMEOUT,
};

enum EDI_PRE_ST4 {	/*use this for co work with do table*/
	EDI_PRE_ST4_EXIT,
	EDI_PRE_ST4_IDLE,	/*switch to next channel?*/
	EDI_PRE_ST4_CHECK,	/*check mode do_table and set*/
	EDI_PRE_ST4_DO_TABLE,	/* do table statue;*/
};

struct di_pre_set_s {
	/*use to remember last hw pre setting;*/
	/*cfg:	*/
	bool cfg_mcpre_en;	/*mcpre_en*/

	unsigned int in_w;
	unsigned int in_h;
	unsigned int in_type;
	unsigned int src_type;
};

struct di_hpre_s {
	enum EDI_PRE_ST4 pre_st;
	unsigned int curr_ch;
	/*set when have vframe in; clear when int have get*/
	bool hw_flg_busy_pre;
/*	bool trig_unreg;*/	/*add for unreg flow;*/
/*	enum EDI_SUB_ID hw_owner_pre;*/
	bool flg_wait_int;
	struct di_pre_stru_s *pres;
	struct di_post_stru_s *psts;
	struct di_time_out_s tout;	/*for time out*/
	bool flg_int_done;
	unsigned int check_recycle_buf_cnt;

	struct di_pre_set_s set_lst;
	struct di_pre_set_s set_curr;

	struct di_vinfo_s vinf_lst;
	struct di_vinfo_s vinf_curr;

	/* use do table to switch mode*/
	struct do_table_s sdt_mode;

	unsigned int idle_cnt;	/*use this avoid repeat idle <->check*/
	/*dbg flow:*/
	bool dbg_f_en;
	unsigned int dbg_f_lstate;
	unsigned int dbg_f_cnt;
};

/**************************************/
/* POST */
/**************************************/
enum EDI_PST_ST {
	EDI_PST_ST_EXIT,
	EDI_PST_ST_IDLE,	/*switch to next channel?*/
	EDI_PST_ST_CHECK,
	EDI_PST_ST_SET,
	EDI_PST_ST_WAIT_INT,
	EDI_PST_ST_TIMEOUT,
	EDI_PST_ST_DONE,	/*use for bypass_all*/
};

struct di_hpst_s {
	enum EDI_PST_ST state;
	unsigned int curr_ch;
	/*set when have vframe in; clear when int have get*/
	bool hw_flg_busy_post;
	struct di_pre_stru_s *pres;
	struct di_post_stru_s *psts;
	struct di_time_out_s tout;	/*for time out*/
	bool flg_int_done;

	/*dbg flow:*/
	bool dbg_f_en;
	unsigned int dbg_f_lstate;
	unsigned int dbg_f_cnt;

};

/**************************************/
/* channel status */
/**************************************/
enum EDI_TOP_STATE {
	EDI_TOP_STATE_NOPROB,
	EDI_TOP_STATE_IDLE,	/*idle not work*/
	/* STEP1
	 * till peek vframe and set irq;before this state, event reg finish
	 */
	EDI_TOP_STATE_REG_STEP1,
	EDI_TOP_STATE_REG_STEP1_P1,	/*2019-05-21*/
	EDI_TOP_STATE_REG_STEP2,	/*till alloc and ready*/
	EDI_TOP_STATE_READY,		/*can do DI*/
	EDI_TOP_STATE_BYPASS,		/*complet bypass*/
	EDI_TOP_STATE_UNREG_STEP1,	/*till pre/post is finish;*/
	/* do unreg and to IDLE.
	 * no need to wait cma release after  this unreg event finish
	 */
	EDI_TOP_STATE_UNREG_STEP2,

};

/**************************************/
/* thread and cmd */
/**************************************/
struct di_task {
	bool flg_init;
	struct semaphore sem;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	unsigned int status;

	unsigned int wakeup;
	unsigned int delay;
	bool exit;
	/*not use cmd*/
	/*local event*/
	struct kfifo	fifo_cmd;
	spinlock_t     lock_cmd; /*spinlock*/
	bool flg_cmd;
	unsigned int err_cmd_cnt;
};

#define MAX_KFIFO_L_CMD_NUB	32

union   DI_L_CMD_BITS {
	unsigned int cmd32;
	struct {
		unsigned int id:8,	/*low bit*/
			     ch:8,	/*channel*/
			     p2:8,
			     p3:8;
	} b;
};

#define LCMD1(id, ch)	((id) | ((ch) << 8))
#define LCMD2(id, ch, p2)	((id) | ((ch) << 8) | ((p2) << 16))

enum ECMD_LOCAL {
	ECMD_NONE,
	ECMD_REG,
	ECMD_UNREG,
	ECMD_READY,
	ECMD_CHG,
	ECMD_RL_KEEP,
	NR_FINISH,
};

/**************************************
 * QUE
 * keep same order as di_name_new_que
 **************************************/
enum QUE_TYPE {	/*mast start from 0 */
	QUE_IN_FREE,	/*5*/
	QUE_PRE_READY,	/*6*/
	QUE_POST_FREE,	/*7*/
	QUE_POST_READY,	/*8*/
	QUE_POST_BACK,		/*new*/
	QUE_POST_DOING,
	QUE_POST_KEEP,	/*below use pw_queue_in*/
	QUE_POST_KEEP_BACK,
	/*----------------*/
	QUE_DBG,
	QUE_NUB,
};

/*#define QUE_NUB  (5)*/
enum EDI_BUF_TYPE {
	EDI_BUF_T_IN = 1,	/*VFRAME_TYPE_IN*/
	EDI_BUF_T_LOCAL,	/*VFRAME_TYPE_LOCAL*/
	EDI_BUF_T_POST,		/*VFRAME_TYPE_POST*/
};

#define MAX_FIFO_SIZE	(32)

/**************************************
 *
 * summmary variable
 * also see:di_sum_name_tab
 **************************************/

enum EDI_SUM {
	EDI_SUM_O_PEEK_CNT,	/*video_peek_cnt*/
	EDI_SUM_REG_UNREG_CNT,	/*di_reg_unreg_cnt*/
	EDI_SUM_NUB,
};

struct di_sum_s {
	char *name;
	enum EDI_SUM	index;
	unsigned int	default_val;
};

/**************************************
 *
 * module para
 *	int
 *	EDI_MP_SUB_DI_B
 *	EDI_MP_SUB_NR_B
 *	EDI_MP_SUB_PD_B
 *	EDI_MP_SUB_MTN_B
 *	EDI_MP_SUB_3D_B
 **************************************/
enum EDI_MP_UI_T {
	/*keep same order with di_mp_ui_top*/
	EDI_MP_UI_T_BEGIN,
	/**************************************/
	EDI_MP_SUB_DI_B,

	edi_mp_force_prog,	/*force_prog bool*/
	edi_mp_combing_fix_en,	/*combing_fix_en bool*/
	edi_mp_cur_lev,		/*cur_lev*/
	edi_mp_pps_dstw,	/*pps_dstw*/
	edi_mp_pps_dsth,	/*pps_dsth*/
	edi_mp_pps_en,		/*pps_en*/
	edi_mp_pps_position,	/*pps_position*/
	edi_mp_pre_enable_mask,	/*pre_enable_mask*/
	edi_mp_post_refresh,	/*post_refresh*/
	edi_mp_nrds_en,		/*nrds_en*/
	edi_mp_bypass_3d,	/*bypass_3d*/
	edi_mp_bypass_trick_mode,	/*bypass_trick_mode*/
	edi_mp_invert_top_bot,	/*invert_top_bot */
	edi_mp_skip_top_bot,
	edi_mp_force_width,
	edi_mp_force_height,
	edi_mp_prog_proc_config,
	edi_mp_start_frame_drop_count,
	edi_mp_same_field_top_count,	/*long?*/
	edi_mp_same_field_bot_count,	/*long?*/
	edi_mp_vpp_3d_mode,
	edi_mp_force_recovery_count,
	edi_mp_pre_process_time,	/*no use?*/
	edi_mp_bypass_post,
	edi_mp_post_wr_en,
	edi_mp_post_wr_support,
	edi_mp_bypass_post_state,
	edi_mp_use_2_interlace_buff,
	edi_mp_debug_blend_mode,
	edi_mp_nr10bit_support,
	edi_mp_di_stop_reg_flag,
	edi_mp_mcpre_en,
	edi_mp_check_start_drop,
	edi_mp_overturn,			/*? in init*/
	edi_mp_full_422_pack,
	edi_mp_cma_print,
	edi_mp_pulldown_enable,
	edi_mp_di_force_bit_mode,
	edi_mp_calc_mcinfo_en,
	edi_mp_colcfd_thr,
	edi_mp_post_blend,
	edi_mp_post_ei,
	edi_mp_post_cnt,
	edi_mp_di_log_flag,
	edi_mp_di_debug_flag,
	edi_mp_buf_state_log_threshold,
	edi_mp_di_vscale_skip_enable,
	edi_mp_di_vscale_skip_count,
	edi_mp_di_vscale_skip_real,
	edi_mp_det3d_en,
	edi_mp_post_hold_line,
	edi_mp_post_urgent,
	edi_mp_di_printk_flag,
	edi_mp_force_recovery,
/*	edi_mp_debug_blend_mode,*/
	edi_mp_di_dbg_mask,
	edi_mp_nr_done_check_cnt,
	edi_mp_pre_hsc_down_en,
	edi_mp_pre_hsc_down_width,
	edi_mp_show_nrwr,
	/********deinterlace_hw.c*********/
	edi_mp_pq_load_dbg,
	edi_mp_lmv_lock_win_en,
	edi_mp_lmv_dist,
	edi_mp_pr_mcinfo_cnt,
	edi_mp_offset_lmv,
	edi_mp_post_ctrl,
	edi_mp_if2_disable,
	edi_mp_pre,
	edi_mp_pre_mif_gate,
	edi_mp_pre_urgent,
	edi_mp_pre_hold_line,
	edi_mp_pre_ctrl,
	edi_mp_line_num_post_frst,
	edi_mp_line_num_pre_frst,
	edi_mp_pd22_flg_calc_en,
	edi_mp_mcen_mode,
	edi_mp_mcuv_en,
	edi_mp_mcdebug_mode,
	edi_mp_pldn_ctrl_rflsh,

	EDI_MP_SUB_DI_E,
	/**************************************/
	EDI_MP_SUB_NR_B,
	edi_mp_dnr_en,
	edi_mp_nr2_en,
	edi_mp_cue_en,
	edi_mp_invert_cue_phase,
	edi_mp_cue_pr_cnt,
	edi_mp_cue_glb_mot_check_en,
	edi_mp_glb_fieldck_en,
	edi_mp_dnr_pr,
	edi_mp_dnr_dm_en,
	EDI_MP_SUB_NR_E,
	/**************************************/
	EDI_MP_SUB_PD_B,
	edi_mp_flm22_ratio,
	edi_mp_pldn_cmb0,
	edi_mp_pldn_cmb1,
	edi_mp_flm22_sure_num,
	edi_mp_flm22_glbpxlnum_rat,
	edi_mp_flag_di_weave,
	edi_mp_flm22_glbpxl_maxrow,
	edi_mp_flm22_glbpxl_minrow,
	edi_mp_cmb_3point_rnum,
	edi_mp_cmb_3point_rrat,
	/******film_fw1.c**/
	edi_mp_pr_pd,
	edi_mp_prt_flg,
	edi_mp_flmxx_maybe_num,
	edi_mp_flm32_mim_frms,
	edi_mp_flm22_dif01a_flag,
	edi_mp_flm22_mim_frms,
	edi_mp_flm22_mim_smfrms,
	edi_mp_flm32_f2fdif_min0,
	edi_mp_flm32_f2fdif_min1,
	edi_mp_flm32_chk1_rtn,
	edi_mp_flm32_ck13_rtn,
	edi_mp_flm32_chk2_rtn,
	edi_mp_flm32_chk3_rtn,
	edi_mp_flm32_dif02_ratio,
	edi_mp_flm22_chk20_sml,
	edi_mp_flm22_chk21_sml,
	edi_mp_flm22_chk21_sm2,
	edi_mp_flm22_lavg_sft,
	edi_mp_flm22_lavg_lg,
	edi_mp_flm22_stl_sft,
	edi_mp_flm22_chk5_avg,
	edi_mp_flm22_chk6_max,
	edi_mp_flm22_anti_chk1,
	edi_mp_flm22_anti_chk3,
	edi_mp_flm22_anti_chk4,
	edi_mp_flm22_anti_ck140,
	edi_mp_flm22_anti_ck141,
	edi_mp_flm22_frmdif_max,
	edi_mp_flm22_flddif_max,
	edi_mp_flm22_minus_cntmax,
	edi_mp_flagdif01chk,
	edi_mp_dif01_ratio,
	/*******vof_soft_top*****/
	edi_mp_cmb32_blw_wnd,
	edi_mp_cmb32_wnd_ext,
	edi_mp_cmb32_wnd_tol,
	edi_mp_cmb32_frm_nocmb,
	edi_mp_cmb32_min02_sft,
	edi_mp_cmb32_cmb_tol,
	edi_mp_cmb32_avg_dff,
	edi_mp_cmb32_smfrm_num,
	edi_mp_cmb32_nocmb_num,
	edi_mp_cmb22_gcmb_rnum,
	edi_mp_flmxx_cal_lcmb,
	edi_mp_flm2224_stl_sft,
	EDI_MP_SUB_PD_E,
	/**************************************/
	EDI_MP_SUB_MTN_B,
	edi_mp_force_lev,
	edi_mp_dejaggy_flag,
	edi_mp_dejaggy_enable,
	edi_mp_cmb_adpset_cnt,
	edi_mp_cmb_num_rat_ctl4,
	edi_mp_cmb_rat_ctl4_minthd,
	edi_mp_small_local_mtn,
	edi_mp_di_debug_readreg,
	EDI_MP_SUB_MTN_E,
	/**************************************/
	EDI_MP_SUB_3D_B,
	edi_mp_chessbd_vrate,
	edi_mp_det3d_debug,

	EDI_MP_SUB_3D_E,
	/**************************************/
	EDI_MP_UI_T_END,
};

#define K_DI_MP_UIT_NUB (EDI_MP_UI_T_END - EDI_MP_UI_T_BEGIN + 1)

struct di_mp_uit_s {
	char *name;
	enum EDI_MP_UI_T	id;
	int	default_val;
};

/*also see: di_mpx*/
enum EDI_MP_UIX_T {
	EDI_MP_UIX_BEGIN,
	EDI_MP_UIX_RUN_FLG, /*run_flag*/
	EDI_MP_UIX_END,
};

#define K_DI_MP_UIX_NUB (EDI_MP_UIX_END - EDI_MP_UIX_BEGIN + 1)

struct di_mp_uix_s {
	char *name;
	enum EDI_MP_UIX_T	id;
	unsigned int	default_val;
};

/**************************************/
/* DI WORKING MODE */
/**************************************/
enum EDI_WORK_MODE {
	EDI_WORK_MODE_NONE,
	EDI_WORK_MODE_BYPASS_COMPLET,
	EDI_WORK_MODE_BYPASS_ALL,	/*dim_is_bypass*/
	EDI_WORK_MODE_BYPASS_PRE,
	EDI_WORK_MODE_BYPASS_POST,
	EDI_WORK_MODE_I,
	EDI_WORK_MODE_P_AS_I,
	EDI_WORK_MODE_P_AS_P,
	EDI_WORK_MODE_P_USE_IBUF,
	EDI_WORK_MODE_ALL,

};

/**************************************/
/* vframe                             */
/**************************************/
struct dev_vfram_t {
	const char *name;
	/*receiver:*/
	struct vframe_receiver_s di_vf_recv;
	/*provider:*/
	struct vframe_provider_s di_vf_prov;

	unsigned int indx;
	/*status:*/
	bool bypass_complete;
	bool reg;	/*use this for vframe reg/unreg*/
/*	unsigned int data[32]; */	/*null*/

};

struct di_ores_s {
	/* same as ori */
	struct di_pre_stru_s di_pre_stru;
	struct di_post_stru_s di_post_stru;

	struct di_buf_s di_buf_local[MAX_LOCAL_BUF_NUM * 2];
	struct di_buf_s di_buf_in[MAX_IN_BUF_NUM];
	struct di_buf_s di_buf_post[MAX_POST_BUF_NUM];

	struct queue_s queue[QUEUE_NUM];
	struct di_buf_pool_s di_buf_pool[VFRAME_TYPE_NUM];

	struct vframe_s *vframe_in[MAX_IN_BUF_NUM];
	struct vframe_s vframe_in_dup[MAX_IN_BUF_NUM];
	struct vframe_s vframe_local[MAX_LOCAL_BUF_NUM * 2];
	struct vframe_s vframe_post[MAX_POST_BUF_NUM];
	/* ********** */
};

enum EDI_CMA_ST {
	EDI_CMA_ST_IDL,
	EDI_CMA_ST_ALLOC,	/*do*/
	EDI_CMA_ST_READY,
	EDI_CMA_ST_RELEASE,	/*do*/
	EDI_CMA_ST_PART,
};

/**********************************
 * mem
 *********************************/
struct di_mm_cfg_s {
	/*support di size*/
	unsigned int di_h;
	unsigned int di_w;
	/**/
	unsigned int num_local;
	unsigned int num_post;
	unsigned int size_local;
	unsigned int size_post;
	int nr_size;
	int count_size;
	int mcinfo_size;
	int mv_size;
	int mtn_size;
	unsigned char buf_alloc_mode;
};

struct dim_mm_t_s {
	/* use for reserved and alloc all*/
	unsigned long	mem_start;
	unsigned int	mem_size;
	struct page	*total_pages;
};

struct di_mm_st_s {
	unsigned long	mem_start;
	unsigned int	mem_size;
	int	num_local;
	int	num_post;	/*ppost*/
};

struct di_mm_s {
	struct di_mm_cfg_s cfg;
	struct di_mm_st_s sts;
};

struct dim_sum_s {
	/*buf*/
	unsigned int b_pre_free;
	unsigned int b_pst_ready;
	unsigned int b_recyc;
	unsigned int b_pre_ready;
	unsigned int b_pst_free;
	unsigned int b_display;
};

struct di_ch_s {
	/*struct di_cfgx_s dbg_cfg;*/
	bool cfgx_en[K_DI_CFGX_NUB];
	unsigned int mp_uix[K_DI_MP_UIX_NUB];/*module para x*/

	struct di_dbg_datax_s dbg_data;

	struct dev_vfram_t vfm;
	struct dentry *dbg_rootx;	/*dbg_fs*/

	unsigned int ch_id;
	struct di_ores_s rse_ori;
	struct kfifo	fifo[QUE_NUB];
	bool flg_fifo[QUE_NUB];	/*have ini: 1; else 0*/
/*	bool sub_act_flg;*/
	/************************/
	/*old glob*/
	/************************/
	/*bypass_state*/
	bool bypass_state;

	/*video_peek_cnt*/
	unsigned int sum[EDI_SUM_NUB + 1];
	unsigned int sum_get;
	unsigned int sum_put;
	struct dim_sum_s	sumx;

};

struct di_meson_data {
	const char *name;
	/*struct ic_ver icver;*/
	/*struct ddemod_reg_off regoff;*/
};

struct dim_wq_s {
	char *name;
	unsigned int ch;
	struct workqueue_struct *wq_cma;
	struct work_struct wq_work;
};

struct di_mng_s {
	/*workqueue*/
	struct dim_wq_s		wq;

	/*use enum EDI_CMA_ST*/
	atomic_t cma_mem_state[DI_CHANNEL_NUB];
	/*1:alloc cma, 0:release cma set by mng, read by work que*/
	unsigned char cma_reg_cmd[DI_CHANNEL_NUB];

	/*task:*/
	struct di_task		tsk;

	/*channel state: use enum eDI_TOP_STATE */
	atomic_t ch_state[DI_CHANNEL_NUB];

	bool in_flg[DI_CHANNEL_NUB];
	unsigned long	   mem_start[DI_CHANNEL_NUB];
	unsigned int	   mem_size[DI_CHANNEL_NUB];

	bool sub_act_flg[DI_CHANNEL_NUB];
	/*struct	mutex	   event_mutex[DI_CHANNEL_NUB];*/
	bool init_flg[DI_CHANNEL_NUB];	/*init_flag*/
	/*bool reg_flg[DI_CHANNEL_NUB];*/	/*reg_flag*/
	unsigned int reg_flg_ch;	/*for x ch reg/unreg flg*/
	bool trig_unreg[DI_CHANNEL_NUB];
	bool hw_reg_flg;	/*for di_reg_setting/di_unreg_setting*/
	bool act_flg		;/*active_flag*/

	bool flg_hw_int;	/*only once*/

	struct dim_mm_t_s mmt;
	struct di_mm_s	mm[DI_CHANNEL_NUB];
};

/*************************
 *debug register:
 *************************/
#define K_DI_SIZE_REG_LOG	(1000)
#define K_DI_LAB_MOD		(0xf001)
/*also see: dbg_mode_name*/
enum EDI_DBG_MOD {
	EDI_DBG_MOD_REGB,	/* 0 */
	EDI_DBG_MOD_REGE,	/* 1 */
	EDI_DBG_MOD_UNREGB,	/* 2 */
	EDI_DBG_MOD_UNREGE,	/* 3 */
	EDI_DBG_MOD_PRE_SETB,	/* 4 */
	EDI_DBG_MOD_PRE_SETE,	/* 5 */
	EDI_DBG_MOD_PRE_DONEB,	/* 6 */
	EDI_DBG_MOD_PRE_DONEE,	/* 7 */
	EDI_DBG_MOD_POST_SETB,	/* 8 */
	EDI_DBG_MOD_POST_SETE,	/* 9 */
	EDI_DBG_MOD_POST_IRQB,	/* a */
	EDI_DBG_MOD_POST_IRQE,	/* b */
	EDI_DBG_MOD_POST_DB,	/* c */
	EDI_DBG_MOD_POST_DE,	/* d */
	EDI_DBG_MOD_POST_CH_CHG,	/* e */
	EDI_DBG_MOD_POST_TIMEOUT,	/* F */

	EDI_DBG_MOD_RVB,	/*10 */
	EDI_DBG_MOD_RVE,	/*11 */

	EDI_DBG_MOD_POST_RESIZE, /*0x12 */
	EDI_DBG_MOD_END,

};

enum EDI_LOG_TYPE {
	EDI_LOG_TYPE_ALL = 1,
	EDI_LOG_TYPE_REG,
	EDI_LOG_TYPE_MOD,
};

struct di_dbg_reg {
	unsigned int addr;
	unsigned int val;
	unsigned int st_bit:8,
		b_w:8,
		res:16;
};

struct di_dbg_mod {
	unsigned int lable;	/*0xf001: mean dbg mode*/
	unsigned int ch:8,
		mod:8,
		res:16;
	unsigned int cnt;/*frame cnt*/
};

union udbg_data {
	struct di_dbg_reg reg;
	struct di_dbg_mod mod;
};

struct di_dbg_reg_log {
	bool en;
	bool en_reg;
	bool en_mod;
	bool en_all;
	bool en_notoverwrite;

	union udbg_data log[K_DI_SIZE_REG_LOG];
	unsigned int pos;
	unsigned int wsize;
	bool overflow;
};

struct di_dbg_data {
	unsigned int vframe_type;	/*use for type info*/
	unsigned int cur_channel;
	struct di_dbg_reg_log reg_log;
};

struct di_data_l_s {
	/*bool cfg_en[K_DI_CFG_NUB];*/	/*cfg_top*/
	union di_cfg_tdata_u cfg_en[K_DI_CFG_NUB];
	unsigned int cfg_sel;
	unsigned int cfg_dbg_mode; /*val or item*/
	int mp_uit[K_DI_MP_UIT_NUB];	/*EDI_MP_UI_T*/
	struct di_ch_s ch_data[DI_CHANNEL_NUB];
	int plane[DI_CHANNEL_NUB];	/*use for debugfs*/

	struct di_dbg_data dbg_data;
	struct di_mng_s mng;
	struct di_hpre_s hw_pre;
	struct di_hpst_s hw_pst;
	struct dentry *dbg_root_top;	/* dbg_fs*/
	/*pq_ops*/
	const struct pulldown_op_s *ops_pd;	/* pulldown */
	const struct detect3d_op_s *ops_3d;	/* detect_3d */
	const struct nr_op_s *ops_nr;	/* nr */
	const struct mtn_op_s *ops_mtn;	/* deinterlace_mtn */
	/*di ops for other module */
	/*struct di_ext_ops *di_api; */
	const struct di_meson_data *mdata;
};

/**************************************
 *
 * DEBUG infor
 *
 *************************************/

#define DBG_M_C_ALL		DI_BIT30	/*all debug close*/
#define DBG_M_O_ALL		DI_BIT31	/*all debug open*/

#define DBG_M_DT		DI_BIT0	/*do table work*/
#define DBG_M_REG		DI_BIT1	/*reg/unreg*/
#define DBG_M_POST_REF		DI_BIT2
#define DBG_M_TSK		DI_BIT3
#define DBG_M_INIT		DI_BIT4
#define DBG_M_EVENT		DI_BIT5
#define DBG_M_FIRSTFRAME	DI_BIT6
#define DBG_M_DBG		DI_BIT7

#define DBG_M_POLLING		DI_BIT8
#define DBG_M_ONCE		DI_BIT9
#define DBG_M_KEEP		DI_BIT10	/*keep buf*/
#define DBG_M_MEM		DI_BIT11	/*mem alloc release*/
#define DBG_M_WQ		DI_BIT14	/*work que*/

extern unsigned int di_dbg;

#define dbg_m(mark, fmt, args ...)		\
	do {					\
		if (di_dbg & DBG_M_C_ALL)	\
			break;			\
		if ((di_dbg & DBG_M_O_ALL) ||	\
		    (di_dbg & (mark))) {		\
			pr_info("dim:" fmt, ##args); \
		}				\
	} while (0)

#define PR_ERR(fmt, args ...)		pr_err("dim:err:" fmt, ##args)
#define PR_WARN(fmt, args ...)		pr_err("dim:warn:" fmt, ##args)
#define PR_INF(fmt, args ...)		pr_info("dim:" fmt, ##args)

#define dbg_dt(fmt, args ...)		dbg_m(DBG_M_DT, fmt, ##args)
#define dbg_reg(fmt, args ...)		dbg_m(DBG_M_REG, fmt, ##args)
#define dbg_post_ref(fmt, args ...)	dbg_m(DBG_M_POST_REF, fmt, ##args)
#define dbg_poll(fmt, args ...)		dbg_m(DBG_M_POLLING, fmt, ##args)
#define dbg_tsk(fmt, args ...)		dbg_m(DBG_M_TSK, fmt, ##args)

#define dbg_init(fmt, args ...)		dbg_m(DBG_M_INIT, fmt, ##args)
#define dbg_ev(fmt, args ...)		dbg_m(DBG_M_EVENT, fmt, ##args)
#define dbg_first_frame(fmt, args ...)	dbg_m(DBG_M_FIRSTFRAME, fmt, ##args)
#define dbg_dbg(fmt, args ...)		dbg_m(DBG_M_DBG, fmt, ##args)
#define dbg_once(fmt, args ...)		dbg_m(DBG_M_ONCE, fmt, ##args)
#define dbg_mem(fmt, args ...)		dbg_m(DBG_M_MEM, fmt, ##args)
#define dbg_keep(fmt, args ...)		dbg_m(DBG_M_KEEP, fmt, ##args)
#define dbg_wq(fmt, args ...)		dbg_m(DBG_M_WQ, fmt, ##args)

char *di_cfgx_get_name(enum EDI_CFGX_IDX idx);
bool di_cfgx_get(unsigned int ch, enum EDI_CFGX_IDX idx);
void di_cfgx_set(unsigned int ch, enum EDI_CFGX_IDX idx, bool en);

static inline struct di_data_l_s *get_datal(void)
{
	return (struct di_data_l_s *)get_dim_de_devp()->data_l;
}

static inline struct di_ch_s *get_chdata(unsigned int ch)
{
	return &get_datal()->ch_data[ch];
}

static inline struct di_mng_s *get_bufmng(void)
{
	return &get_datal()->mng;
}

static inline struct di_hpre_s  *get_hw_pre(void)
{
	return &get_datal()->hw_pre;
}

static inline struct di_hpst_s  *get_hw_pst(void)
{
	return &get_datal()->hw_pst;
}

/****************************************
 * flg_hw_int
 *	for hw set once
 ****************************************/
static inline bool di_get_flg_hw_int(void)
{
	return get_datal()->mng.flg_hw_int;
}

static inline void di_set_flg_hw_int(bool on)
{
	get_datal()->mng.flg_hw_int = on;
}

/**********************
 *
 *	reg log:
 *********************/
static inline struct di_dbg_reg_log *get_dbg_reg_log(void)
{
	return &get_datal()->dbg_data.reg_log;
}

/**********************
 *
 *	flg_wait_int
 *********************/
static inline void di_pre_wait_irq_set(bool on)
{
	get_hw_pre()->flg_wait_int = on;
}

static inline bool di_pre_wait_irq_get(void)
{
	return get_hw_pre()->flg_wait_int;
}

static inline struct di_ores_s *get_orsc(unsigned int ch)
{
	return &get_datal()->ch_data[ch].rse_ori;
}

static inline struct vframe_s **get_vframe_in(unsigned int ch)
{
	return &get_orsc(ch)->vframe_in[0];
}

static inline struct vframe_s *get_vframe_in_dup(unsigned int ch)
{
	return &get_orsc(ch)->vframe_in_dup[0];
}

static inline struct vframe_s *get_vframe_local(unsigned int ch)
{
	return &get_orsc(ch)->vframe_local[0];
}

static inline struct vframe_s *get_vframe_post(unsigned int ch)
{
	return &get_orsc(ch)->vframe_post[0];
}

static inline struct di_buf_s *get_buf_local(unsigned int ch)
{
	return &get_orsc(ch)->di_buf_local[0];
}

static inline struct di_buf_s *get_buf_in(unsigned int ch)
{
	return &get_orsc(ch)->di_buf_in[0];
}

static inline struct di_buf_s *get_buf_post(unsigned int ch)
{
	return &get_orsc(ch)->di_buf_post[0];
}

static inline struct queue_s *get_queue(unsigned int ch)
{
	return &get_orsc(ch)->queue[0];
}

static inline struct di_buf_pool_s *get_buf_pool(unsigned int ch)
{
	return &get_orsc(ch)->di_buf_pool[0];
}

static inline struct di_pre_stru_s *get_pre_stru(unsigned int ch)
{
	return &get_orsc(ch)->di_pre_stru;
}

static inline struct di_post_stru_s *get_post_stru(unsigned int ch)
{
	return &get_orsc(ch)->di_post_stru;
}

static inline enum EDI_SUB_ID get_current_channel(void)
{
	return get_datal()->dbg_data.cur_channel;
}

static inline void set_current_channel(unsigned int channel)
{
	get_datal()->dbg_data.cur_channel = channel;
}

static inline bool get_init_flag(unsigned char ch)
{
	return get_bufmng()->init_flg[ch];
}

static inline void set_init_flag(unsigned char ch, bool on)
{
	get_bufmng()->init_flg[ch] =  on;
}

extern const unsigned int di_ch2mask_table[DI_CHANNEL_MAX];
/******************************************
 *
 *	reg / unreg
 *
 *****************************************/
static inline bool get_reg_flag(unsigned char ch)
{
	unsigned int flg = get_bufmng()->reg_flg_ch;
	bool ret = false;

	if (di_ch2mask_table[ch] & flg)
		ret = true;

	/*dim_print("%s:%d\n", __func__, ret);*/
	return ret;
}

static inline unsigned int get_reg_flag_all(void)
{
	return get_bufmng()->reg_flg_ch;
}

static inline void set_reg_flag(unsigned char ch, bool on)
{
	unsigned int flg = get_bufmng()->reg_flg_ch;

	if (on)
		get_bufmng()->reg_flg_ch = flg | di_ch2mask_table[ch];
	else
		get_bufmng()->reg_flg_ch = flg & (~di_ch2mask_table[ch]);
	/*dim_print("%s:%d\n", __func__, get_bufmng()->reg_flg_ch);*/
}

/******************************************
 *
 *	trig unreg:
 *		when unreg: set 1
 *		when reg: set 0
 *****************************************/

static inline bool get_flag_trig_unreg(unsigned char ch)
{
	return get_bufmng()->trig_unreg[ch];
}

#ifdef MARK_HIS
static inline unsigned int get_reg_flag_all(void)
{
	return get_bufmng()->reg_flg_ch;
}
#endif

static inline void set_flag_trig_unreg(unsigned char ch, bool on)
{
	get_bufmng()->trig_unreg[ch] =  on;
}

static inline bool get_hw_reg_flg(void)
{
	return get_bufmng()->hw_reg_flg;
}

static inline void set_hw_reg_flg(bool on)
{
	get_bufmng()->hw_reg_flg =  on;
}

static inline bool get_or_act_flag(void)
{
	return get_bufmng()->act_flg;
}

static inline void set_or_act_flag(bool on)
{
	get_bufmng()->act_flg =  on;
}

/*sum*/
static inline void di_sum_set_l(unsigned int ch, enum EDI_SUM id,
				unsigned int val)
{
	get_chdata(ch)->sum[id] = val;
}

static inline unsigned int di_sum_inc_l(unsigned int ch, enum EDI_SUM id)
{
	get_chdata(ch)->sum[id]++;
	return get_chdata(ch)->sum[id];
}

static inline unsigned int di_sum_get_l(unsigned int ch, enum EDI_SUM id)
{
	return get_chdata(ch)->sum[id];
}

/*sum get and put*/
static inline unsigned int get_sum_g(unsigned int ch)
{
	return get_datal()->ch_data[ch].sum_get;
}

static inline void sum_g_inc(unsigned int ch)
{
	get_datal()->ch_data[ch].sum_get++;
}

static inline void sum_g_clear(unsigned int ch)
{
	get_datal()->ch_data[ch].sum_get = 0;
}

static inline unsigned int get_sum_p(unsigned int ch)
{
	return get_datal()->ch_data[ch].sum_put;
}

static inline void sum_p_inc(unsigned int ch)
{
	get_datal()->ch_data[ch].sum_put++;
}

static inline void sum_p_clear(unsigned int ch)
{
	get_datal()->ch_data[ch].sum_put = 0;
}

static inline struct dim_sum_s *get_sumx(unsigned int ch)
{
	return &get_datal()->ch_data[ch].sumx;
}

/*bypass_state*/

static inline bool di_bypass_state_get(unsigned int ch)
{
	return get_chdata(ch)->bypass_state;
}

static inline void di_bypass_state_set(unsigned int ch, bool on)
{
	get_chdata(ch)->bypass_state =  on;
}

#ifdef MARK_HIS
static inline struct semaphore *get_sema(void)
{
	return &get_dim_de_devp()->sema;
}
#endif

static inline struct di_task *get_task(void)
{
	return &get_bufmng()->tsk;
}

/******************************************
 *	pq ops
 *****************************************/

static inline const struct pulldown_op_s *get_ops_pd(void)
{
	return get_datal()->ops_pd;
}

static inline const struct detect3d_op_s *get_ops_3d(void)
{
	return get_datal()->ops_3d;
}

static inline const struct nr_op_s *get_ops_nr(void)
{
	return get_datal()->ops_nr;
}

static inline const struct mtn_op_s *get_ops_mtn(void)
{
	return get_datal()->ops_mtn;
}

#ifdef MARK_HIS
static inline struct di_ext_ops *get_ops_api(void)
{
	return get_datal()->di_api;
}
#endif

/******************************************
 *	module para for di
 *****************************************/

static inline int dimp_get(enum EDI_MP_UI_T idx)
{
	return get_datal()->mp_uit[idx];
}

static inline void dimp_set(enum EDI_MP_UI_T idx, int val)
{
	get_datal()->mp_uit[idx] = val;
}

static inline int dimp_inc(enum EDI_MP_UI_T idx)
{
	get_datal()->mp_uit[idx]++;
	return get_datal()->mp_uit[idx];
}

static inline int dimp_dec(enum EDI_MP_UI_T idx)
{
	get_datal()->mp_uit[idx]--;
	return get_datal()->mp_uit[idx];
}

#define di_mpr(x) dimp_get(edi_mp_##x)

/******************************************
 *	mm
 *****************************************/
static inline struct di_mm_s *dim_mm_get(unsigned int ch)
{
	return &get_datal()->mng.mm[ch];
}

static inline struct dim_mm_t_s *dim_mmt_get(void)
{
	return &get_datal()->mng.mmt;
}

static inline unsigned long di_get_mem_start(unsigned int ch)
{
	return get_datal()->mng.mm[ch].sts.mem_start;
}

static inline void di_set_mem_info(unsigned int ch,
				   unsigned long mstart,
				   unsigned int size)
{
	get_datal()->mng.mm[ch].sts.mem_start = mstart;
	get_datal()->mng.mm[ch].sts.mem_size = size;
}

static inline unsigned int di_get_mem_size(unsigned int ch)
{
	return get_datal()->mng.mm[ch].sts.mem_size;
}

void di_tout_int(struct di_time_out_s *tout, unsigned int thd);
bool di_tout_contr(enum EDI_TOUT_CONTR cmd, struct di_time_out_s *tout);

#endif	/*__DI_DATA_L_H__*/
