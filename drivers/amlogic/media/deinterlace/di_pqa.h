/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_PQA_H__
#define __DI_PQA_H__

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include <linux/device.h>
#include "../deinterlace/pulldown_drv.h"
#include "../deinterlace/deinterlace_mtn.h"
#if 0 /*move from pulldown_drv.h to di_pq.h*/
enum pulldown_mode_e {
	PULL_DOWN_BLEND_0	= 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2	= 1,/* buf1=dup[2] */
	PULL_DOWN_MTN		= 2,/* mtn only */
	PULL_DOWN_BUF1		= 3,/* do wave with dup[0] */
	PULL_DOWN_EI		= 4,/* ei only */
	PULL_DOWN_NORMAL	= 5,/* normal di */
	PULL_DOWN_NORMAL_2	= 6,/* di */
};

struct pulldown_vof_win_s {
	unsigned short win_vs;
	unsigned short win_ve;
	enum pulldown_mode_e blend_mode;
};

struct pulldown_detected_s {
	enum pulldown_mode_e global_mode;
	struct pulldown_vof_win_s regs[4];
};
#endif

#if 0	/*move from deinterlace_mtn.h*/
struct combing_status_s {
	unsigned int frame_diff_avg;
	unsigned int cmb_row_num;
	unsigned int field_diff_rate;
	int like_pulldown22_flag;
	unsigned int cur_level;
};
#endif
/**************************
 * pulldown
 *************************/
struct pulldown_op_s {
	unsigned char (*init)(unsigned short width, unsigned short height);
	unsigned int (*detection)(struct pulldown_detected_s *res,
				  struct combing_status_s *cmb_sts,
				  bool reverse,
				  struct vframe_s *vf);
	void (*vof_win_vshift)(struct pulldown_detected_s *wins,
			       unsigned short v_offset);
	int (*module_para)(struct seq_file *seq);
	void (*prob)(struct device *dev);
	void (*remove)(struct device *dev);
};

bool di_attach_ops_pulldown(const struct pulldown_op_s **ops);

/**************************
 * detect3d
 *************************/
struct detect3d_op_s {
	void (*det3d_config)(bool flag);
	enum tvin_trans_fmt (*det3d_fmt_detect)(void);
	int (*module_para)(struct seq_file *seq);
};

bool di_attach_ops_3d(const struct detect3d_op_s **ops);

/**************************
 * nr_drv
 *************************/
struct nr_op_s {
	void (*nr_hw_init)(void);
	void (*nr_gate_control)(bool gate);
	void (*nr_drv_init)(struct device *dev);
	void (*nr_drv_uninit)(struct device *dev);
	void (*nr_process_in_irq)(void);
	void (*nr_all_config)(unsigned short nCol, unsigned short nRow,
			      unsigned short type);
	bool (*set_nr_ctrl_reg_table)(unsigned int addr, unsigned int value);
	void (*cue_int)(struct vframe_s *vf);
	void (*adaptive_cue_adjust)(unsigned int frame_diff,
				    unsigned int field_diff);
	void (*secam_cfr_adjust)(unsigned int sig_fmt,
				 unsigned int frame_type);
	int (*module_para)(struct seq_file *seq);

};

bool di_attach_ops_nr(const struct nr_op_s **ops);

/**************************
 * deinterlace_mtn
 *************************/
struct mtn_op_s {
	void (*mtn_int_combing_glbmot)(void);
	void (*adpative_combing_exit)(void);
	void (*fix_tl1_1080i_patch_sel)(unsigned int mode);
	int (*adaptive_combing_fixing)(
		struct combing_status_s *cmb_status,
		unsigned int field_diff, unsigned int frame_diff,
		int bit_mode);
	/*adpative_combing_config*/
	struct combing_status_s *
		(*adpative_combing_config)(unsigned int width,
					   unsigned int height,
					   enum vframe_source_type_e src_type,
					   bool prog,
					   enum tvin_sig_fmt_e fmt);
	void (*com_patch_pre_sw_set)(unsigned int mode);
	int (*module_para)(struct seq_file *seq);
};

bool di_attach_ops_mtn(const struct mtn_op_s **ops);

/**********************************************************
 *
 * IC VERSION
 *
 **********************************************************/
#define DI_IC_ID_M8B		(0x01)
#define DI_IC_ID_GXBB		(0x02)
#define DI_IC_ID_GXTVBB		(0x03)
#define DI_IC_ID_GXL		(0x04)
#define DI_IC_ID_GXM		(0x05)
#define DI_IC_ID_TXL		(0x06)
#define DI_IC_ID_TXLX		(0x07)
#define DI_IC_ID_AXG		(0x08)
#define DI_IC_ID_GXLX		(0x09)
#define DI_IC_ID_TXHD		(0x0a)

#define DI_IC_ID_G12A		(0x10)
#define DI_IC_ID_G12B		(0x11)
#define DI_IC_ID_SM1		(0x13)
#define DI_IC_ID_TL1		(0x16)
#define DI_IC_ID_TM2		(0x17)
#define DI_IC_ID_TM2B		(0x18)
#define DI_IC_ID_T5			(0x19)// same with tm2b
#define DI_IC_ID_T5D		(0x1A)// same with tm2b

#define DI_IC_ID_SC2		(0x1B)
#define DI_IC_ID_T7		(0x1C)
#define DI_IC_ID_S4		(0x1D)

#define DI_IC_ID_DEINTERLACE		(0xFF)

/* is_meson_g12a_cpu */
static inline bool is_ic_named(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id == ic_id)
		return true;
	return false;
}

/*cpu_after_eq*/
static inline bool is_ic_after_eq(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id >= ic_id)
		return true;
	return false;
}

static inline bool is_ic_before(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id < ic_id)
		return true;
	return false;
}

#define IS_IC(cid, cc)		is_ic_named(cid, DI_IC_ID_##cc)
#define IS_IC_EF(cid, cc)	is_ic_after_eq(cid, DI_IC_ID_##cc)
#define IS_IC_BF(cid, cc)	is_ic_before(cid, DI_IC_ID_##cc)

struct reg_acc {
	void (*wr)(unsigned int adr, unsigned int val);
	unsigned int (*rd)(unsigned int adr);
	unsigned int (*bwr)(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len);
	unsigned int (*brd)(unsigned int adr, unsigned int start,
			    unsigned int len);

};

/************************************************
 * afbc
 ************************************************/
enum EAFBC_DEC {
	EAFBC_DEC0,
	EAFBC_DEC1,
	EAFBC_DEC2_DI,
	EAFBC_DEC3_MEM,
	EAFBC_DEC_CHAN2,
	EAFBC_DEC_IF0,
	EAFBC_DEC_IF1,
	EAFBC_DEC_IF2,
	/*sc2*/
};

enum EAFBC_ENC {
	EAFBC_ENC0,
	EAFBC_ENC1,
};

enum EAFBCV1_CFG {
	EAFBCV1_CFG_EN,
	EAFBCV1_CFG_PMODE,
	EAFBCV1_CFG_EMODE,
	EAFBCV1_CFG_ETEST,
	EAFBCV1_CFG_4K,
	EAFBCV1_CFG_PRE_LINK,
	EAFBCV1_CFG_PAUSE,
	EAFBCV1_CFG_LEVE3,
};

enum EAFBC_CFG {
	EAFBC_CFG_DISABLE,
	EAFBC_CFG_INP_AFBC,	/* < AFBCD_V4 */
	EAFBC_CFG_ETEST2,
	EAFBC_CFG_ETEST,
	EAFBC_CFG_4K,
	EAFBC_CFG_PRE_LINK,
	EAFBC_CFG_PAUSE,
	EAFBC_CFG_LEVE3,
	EAFBC_CFG_FORCE_MEM,
	EAFBC_CFG_FORCE_CHAN2,
	EAFBC_CFG_FORCE_NR,
	EAFBC_CFG_FORCE_IF1,
	EAFBC_CFG_MEM,		/* = AFBCD_V4 */
	EAFBC_CFG_6DEC_2ENC,
	EAFBC_CFG_6DEC_2ENC2, /* for none afbc in use afbc*/
	EAFBC_CFG_6DEC_1ENC3, /* base 2, out is nv21 */

};

enum EAFBC_STS {
	EAFBC_MEM_NEED,
	EAFBC_MEMI_NEED,
	EAFBC_EN_6CH,	//en afbcd x 6
	EAFBC_EN_ENC, //en afbce x 2
};

enum EAFBC_SNG_SET {
	EAFBC_SNG_CLR_NR,
	EAFBC_SNG_CLR_WR,
	EAFBC_SNG_SET_NR,
	EAFBC_SNG_SET_WR,
};

struct afbce_map_s {
	unsigned long tabadd;
	unsigned long bodyadd;
	unsigned int size_buf;
	unsigned int size_tab;
};

union afbc_blk_s {
	unsigned char d8;
	struct {
		unsigned char	inp		: 1,
			mem		: 1,
			chan2		: 1,
			if0		: 1,
			if1		: 1,
			if2		: 1,
			enc_nr		: 1,
			enc_wr		: 1;
	} b;
};

struct afbc_fb_s {
	unsigned char ver;

	union afbc_blk_s	sp;

		/* mode
		 *	0: 1 afbcd
		 *	1: p: 2afbcd + 1 afbce
		 *	2: i: 6afbcd + 2 afbce
		 *	3:
		 */
	unsigned int	mode		: 4,
		mem_alloc	: 1,
		mem_alloci	: 1,
		rev3		: 2,

		pre_dec		: 4,
		mem_dec		: 4,

		ch2_dec		: 4,
		if0_dec		: 4,

		if1_dec		: 4,
		if2_dec		: 4;

		//rev4		: 8;
};

struct afbcdv1_ctr_s {
	union {
		unsigned int f32;
		struct {
			unsigned int ver	: 8,

				sp_inp		: 1,
				sp_mem		: 1,
				/*0:inp only, 1: inp + mem*/
				mode		: 2,
				rev1		: 4,

				pre_dec		: 4,
				mem_dec		: 4,

				rev4		: 8;
		} fb;
	};
	union {
		unsigned int d32;
		struct {
		unsigned int int_flg	: 1, /*addr ini*/
			en		: 1,
			enc_err		: 1,
			rev1		: 5,

			chg_level	: 2,
			chg_mem		: 2, /*add*/
			rev2		: 4,

			rev3		: 8,
			rev4		: 8;
		} b;
	};
	unsigned int size_tab;
	unsigned int size_info;
	unsigned int l_vtype;
	unsigned int l_vt_mem;
	unsigned int l_h;
	unsigned int l_w;
	unsigned int l_bitdepth;
	unsigned int addr_h;
	unsigned int addr_b;
	unsigned int mem_addr_h;
	unsigned int mem_addr_b;
};

struct afdv1_s {
	struct afbcdv1_ctr_s ctr;
};

struct afbcd_ctr_s {
	struct afbc_fb_s fb;
	union {
		unsigned int d32;
		struct {
		unsigned int int_flg	: 1, /*addr ini*/
			en		: 1,
			enc_err		: 1,
			en_pst		: 1,

			en_pst_if1	: 1,
			en_pst_if2	: 1,
			rev1		: 2,

			chg_level	: 2,
			chg_mem		: 2, /*add*/
			pst_chg_level	: 2,
			pst_chg_if1	: 2,

			pst_chg_if2	: 2,
			chg_chan2	: 2,
			rev3		: 4,

			rev4		: 8;
		} b;
	};

	union afbc_blk_s	en_cfg;
	union afbc_blk_s	en_sgn;
	union afbc_blk_s	en_set;
	union afbc_blk_s	en_sgn_pst;
	union afbc_blk_s	en_set_pst;

	unsigned int size_tab;
	unsigned int size_info;
	unsigned int blk_nub_total;	/*2020-08-24*/
	unsigned int size_afbc_buf;	/*2020-08-24*/

	unsigned int l_vtype;
	unsigned int l_vt_mem;
	unsigned int l_vt_chan2;
	unsigned int l_h;
	unsigned int l_w;
	unsigned int l_bitdepth;
	unsigned int addr_h;
	unsigned int addr_b;
//	unsigned int mem_addr_h;
//	unsigned int mem_addr_b;
	unsigned int pst_in_vtp;
	//unsigned int pst_o_vtp;
	unsigned int pst_in_h;
	unsigned int pst_in_w;
	unsigned int pst_in_bitdepth;
	//unsigned int pst_o_h;
	//unsigned int pst_o_w;
};

struct afd_s {
	struct afbcd_ctr_s ctr;
	union hw_sc2_ctr_pre_s *top_cfg_pre;
	union hw_sc2_ctr_pst_s *top_cfg_pst;
};

enum AFBC_SGN {
	AFBC_SGN_BYPSS,
	AFBC_SGN_P_H265,
	AFBC_SGN_P,
	AFBC_SGN_P_SINP,
	AFBC_SGN_P_H265_AS_P,
	AFBC_SGN_H265_SINP,
	AFBC_SGN_I,
	AFBC_SGN_I_H265,
};

struct afdv1_ops_s {
	void (*prob)(unsigned int cid);
	bool (*is_supported)(void);
	u32 (*en_pre_set)(struct vframe_s *inp_vf,
			  struct vframe_s *mem_vf,
			  struct vframe_s *nr_vf);
	void (*inp_sw)(bool on);
	bool (*is_used)(void);
	bool (*is_free)(void);/*?*/
	bool (*is_cfg)(enum EAFBCV1_CFG cfg_cmd);
	unsigned int (*count_info_size)(unsigned int w, unsigned int h);
	unsigned int (*count_tab_size)(unsigned int buf_size);
	void (*int_tab)(struct device *dev,
			struct afbce_map_s *pcfg);
	void (*dump_reg)(void);
	void (*reg_sw)(bool on);
	void (*reg_val)(void);
	u32 (*rqst_share)(bool onoff);
	const unsigned int *(*get_d_addrp)(enum EAFBC_DEC eidx);
	const unsigned int *(*get_e_addrp)(enum EAFBC_ENC eidx);
	u32 (*dbg_rqst_share)(bool onoff);
};

struct afd_ops_s {
	void (*prob)(unsigned int cid, struct afd_s *p);
	bool (*is_supported)(void);
	u32 (*en_pre_set)(struct vframe_s *inp_vf,
			  struct vframe_s *mem_vf,
			  struct vframe_s *chan2_vf,
			  struct vframe_s *nr_vf);
	u32 (*en_pst_set)(struct vframe_s *if0_vf,
			  struct vframe_s *if1_vf,
			  struct vframe_s *if2_vf,
			  struct vframe_s *wr_vf);
	void (*inp_sw)(bool on);
	bool (*is_used)(void);
	bool (*is_used_inp)(void);
	bool (*is_used_mem)(void);
	bool (*is_used_chan2)(void);
	bool (*is_free)(void);/*?*/
	bool (*is_cfg)(enum EAFBC_CFG cfg_cmd);
	unsigned int (*cnt_info_size)(unsigned int w, unsigned int h,
				      unsigned int *blk_total);
	unsigned int (*cnt_tab_size)(unsigned int buf_size);
	unsigned int (*cnt_buf_size)(unsigned int format,
				     unsigned int blk_nub_total);
	unsigned int (*int_tab)(struct device *dev,
				struct afbce_map_s *pcfg);
	unsigned int (*tab_cnt_crc)(struct device *dev,
				    struct afbce_map_s *pcfg,
				    unsigned int check_mode,
				    unsigned int crc_old);
	void (*dump_reg)(void);
	void (*reg_sw)(bool on);
	void (*reg_val)(void *pch);
	void (*rest_val)(void);
	u32 (*rqst_share)(bool onoff);
	const unsigned int *(*get_d_addrp)(enum EAFBC_DEC eidx);
	const unsigned int *(*get_e_addrp)(enum EAFBC_ENC eidx);
	u32 (*dbg_rqst_share)(bool onoff);
	void (*dbg_afbc_blk)(struct seq_file *s,
			     union afbc_blk_s *blk, char *name);

	void (*pre_check)(struct vframe_s *vf, void *pch);/* struct di_ch_s */
	//void (*pre_check2)(struct di_buf_s *di_buf);
	void (*pst_check)(struct vframe_s *vf, void *pch);
	bool (*is_sts)(enum EAFBC_STS status);
	void (*sgn_mode_set)(unsigned char *sgn_mode, enum EAFBC_SNG_SET cmd);
	unsigned char (*cnt_sgn_mode)(unsigned int sgn);
	void (*cfg_mode_set)(unsigned int mode, union afbc_blk_s *en_cfg);
};

enum EDI_MIFSM {
	EDI_MIFSM_NR,
	EDI_MIFSM_WR,
};

int dbg_afbc_cfg_show(struct seq_file *s, void *v);/*debug*/
void dbg_afbcd_bits_show(struct seq_file *s, enum EAFBC_DEC eidx);
void dbg_mif_wr_bits_show(struct seq_file *s, enum EDI_MIFSM mifsel);
void dbg_afd_reg(struct seq_file *s, enum EAFBC_DEC eidx);
void dbg_afbce_bits_show(struct seq_file *s, enum EAFBC_ENC eidx);
void dbg_afe_reg(struct seq_file *s, enum EAFBC_ENC eidx);
void dbg_afe_reg_v3(struct seq_file *s, enum EAFBC_ENC eidx);

bool dbg_di_prelink(void);
void dbg_di_prelink_reg_check(void);

bool dbg_di_prelink_v3(void);
/**/
int dbg_afbc_cfg_v3_show(struct seq_file *s, void *v);/*debug*/
void dbg_afd_reg_v3(struct seq_file *s, enum EAFBC_DEC eidx);

bool di_attach_ops_afd(const struct afdv1_ops_s **ops);
bool di_attach_ops_afd_v3(const struct afd_ops_s **ops);

/************************************************
 * ext api for di
 ************************************************/

struct ext_ops_s {
	void (*switch_vpu_mem_pd_vmod)(unsigned int vmod, bool on);
/*	char *(*vf_get_receiver_name)(const char *provider_name);*/
	void (*switch_vpu_clk_gate_vmod)(unsigned int vmod, int flag);
	int (*get_current_vscale_skip_count)(struct vframe_s *vf);
	u32 (*cvs_alloc_table)(const char *owner, u32 *tab,
			       int size,
			       enum canvas_map_type_e type);
	u32 (*cvs_free_table)(u32 *tab, int size);
};

bool dil_attch_ext_api(const struct ext_ops_s **exp_4_di);

#endif	/*__DI_PQA_H__*/
