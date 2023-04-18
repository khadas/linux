/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_VLOCK_PROC_H
#define __AM_VLOCK_PROC_H
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/vpp.h>

#include "vlock_drv.h"

#define VLOCK_VER "Ref.2023/0303: optimize 50 60hz vlock phase value"

#define VLOCK_REG_NUM					33
#define VLOCK_ALL_LOCK_CNT				400
#define VLOCK_V_FRONT_PORCH_REV_B		2	/*T3 evB chip*/

struct vdin_sts_s {
	unsigned int lcnt_sts;
	unsigned int com_sts0;
	unsigned int com_sts1;
};

struct vlock_proc_log_s {
	unsigned int pll_m;
	unsigned int pll_frac;
	signed int line_num_adj;
	unsigned int enc_line_max;
	signed int pixel_num_adj;
	unsigned int enc_pixel_max;
	signed int T0;
	signed int vdif_err;
	signed int err_sum;
	signed int margin;
	unsigned int vlock_regs[VLOCK_REG_NUM];
};

struct vlock_proc_reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
};

enum vlock_proc_regmap_e {
	VLOCK_PROC_REG_MAP_VPU = 0,
	VLOCK_PROC_REG_MAP_HIU,
	VLOCK_PROC_REG_MAP_ANACTRL,/*enc*/
	VLOCK_PROC_REG_MAP_END,
};

enum vlock_proc_src_in_e {
	VLOCK_PROC_SRC_UNUSE = 0,
	VLOCK_PROC_SRC_HDMI = 1,
	VLOCK_PROC_SRC_TV_DEC = 2,
	VLOCK_PROC_SRC_DVIN0 = 3,
	VLOCK_PROC_SRC_DVIN1 = 4,
	VLOCK_PROC_SRC_BT656 = 5,
};

enum vlock_proc_out_goes_e {
	VLOCK_PROC_OUT_ENCL = 0,
	VLOCK_PROC_OUT_ENCP = 1,
	VLOCK_PROC_OUT_ENCI = 2,
};

enum vlock_proc_enc_num_e {
	VLOCK_PROC_ENC0 = 0,
	VLOCK_PROC_ENC1,
	VLOCK_PROC_ENC2,
	VLOCK_PROC_ENC_MAX,
};

//#define VLOCK_DEBUG_ENC_IDX	VLOCK_ENC2

struct vlock_proc_reg_map_tab {
	unsigned int base;
	unsigned int size;
};

enum vlock_proc_param_e {
	VLOCK_PROC_EN = 0x0,
	VLOCK_PROC_ADAPT,
	VLOCK_PROC_MODE,
	VLOCK_PROC_DIS_CNT_LIMIT,
	VLOCK_PROC_DELTA_LIMIT,
	VLOCK_PROC_PLL_M_LIMIT,
	VLOCK_PROC_DELTA_CNT_LIMIT,
	VLOCK_PROC_DEBUG,
	VLOCK_PROC_DYNAMIC_ADJUST,
	VLOCK_PROC_STATE,
	VLOCK_PROC_SYNC_LIMIT_FLAG,
	VLOCK_PROC_DIS_CNT_NO_VF_LIMIT,
	VLOCK_PROC_LINE_LIMIT,
	VLOCK_PROC_SUPPORT,
	VLOCK_PROC_PARAM_MAX,
};

struct vlock_proc_sig_sts {
	u32 fsm_sts;
	u32 fsm_prests;
	u32 fsm_pause;
	u32 vf_sts;
	u32 vmd_chg;
	u32 frame_cnt_in;
	u32 frame_cnt_no;
	u32 input_hz;
	u32 duration;
	u32 output_hz;
	bool md_support;
	u32 video_inverse;
	u32 phlock_percent;
	u32 phlock_sts;
	u32 phlock_en;
	u32 phlock_cnt;
	u32 frqlock_sts;
	/*u32 frqlock_stable_cnt;*/
	u32 ss_sts;
	u32 pll_mode_pause;
	struct vlock_match_data_s *dtdata;
	u32 val_frac;
	u32 val_m;
	struct vdin_sts_s vdinsts;

	u32 start_chk_ph;
	u32 all_lock_cnt;

	enum vlock_proc_enc_num_e idx;
	u32 offset_encl;	/*enc0,enc1,enc2 address offset*/
	u32 offset_vlck;	/*vlock0,vlock1,vlock2 address offset*/

	u32 m_update_cnt;
	u32 f_update_cnt;
	u32 enable_cnt;
	u32 enable_auto_enc_cnt;
	/*monitor*/
	u32 pre_line;
	u32 pre_pixel;

	/*check lock sts*/
	u32 chk_lock_sts_rstflag;
	u32 chk_lock_sts_cnt;
	u32 chk_lock_sts_vs_in;
	u32 chk_lock_sts_vs_out;

	u32 hhi_pll_reg_m;
	u32 hhi_pll_reg_frac;
	u32 pre_hiu_reg_m;
	u32 pre_hiu_reg_frac;

	u32 enc_max_line_addr;
	u32 enc_max_pixel_addr;
	u32 pre_enc_max_pixel;
	u32 pre_enc_max_line;
	u32 org_enc_line_num;
	u32 org_enc_pixel_num;
	u32 enc_video_mode_addr;
	u32 enc_video_mode_adv_addr;
	u32 enc_max_line_switch_addr;
	u32 enc_frc_v_porch_addr;
	u32 enc_frc_v_porch;
	u32 enc_frc_max_line;
	u32 enc_frc_max_pixel;

	u32 last_i_vsync;
	u32 err_accum;
};

enum vlock_proc_change {
	VLOCK_PROC_CHG_NONE = 0,
	VLOCK_PROC_CHG_PH_UNCLOCK,
	VLOCK_PROC_CHG_NEED_RESET,
};

struct vlock_proc_frc_param {
	u32 max_lncnt;
	u32 max_pxcnt;
	u32 frc_v_porch;
	u32 frc_mcfixlines;
	unsigned char s2l_en;
};

#define diff(a, b) ({ \
	typeof(a) _a = a; \
	typeof(b) _b = b; \
	(((_a) > (_b)) ? ((_a) - (_b)) : ((_b) - (_a))); })

//void amve_vlock_process(struct vframe_s *vf);
//void amve_vlock_resume(void);
void vlock_proc_param_set(unsigned int val, enum vlock_proc_param_e sel);
void vlock_proc_status(struct vlock_proc_sig_sts *pvlock);
void vlock_proc_reg_dump(struct vlock_proc_sig_sts *pvlock);
void vlock_proc_log_start(void);
void vlock_proc_log_stop(void);
void vlock_proc_log_print(void);

#define VLOCK_STATE_NULL 0
#define VLOCK_STATE_ENABLE_STEP1_DONE 1
#define VLOCK_STATE_ENABLE_STEP2_DONE 2
#define VLOCK_STATE_DISABLE_STEP1_DONE 3
#define VLOCK_STATE_DISABLE_STEP2_DONE 4
#define VLOCK_STATE_ENABLE_FORCE_RESET 5

/* video lock */
enum VLOCK_PROC_MD {
	VLOCK_PROC_MODE_AUTO_ENC = 0x01,
	VLOCK_PROC_MODE_AUTO_PLL = 0x02,
	VLOCK_PROC_MODE_MANUAL_PLL = 0x04,
	VLOCK_PROC_MODE_MANUAL_ENC = 0x08,
	VLOCK_PROC_MODE_MANUAL_SOFT_ENC = 0x10,
	VLOCK_PROC_MODE_MANUAL_MIX_PLL_ENC = 0x20,
};

/* ------------------ reg ----------------------*/
/*base 0xfe008000*/
#define ANACTRL_TCON_PLL_VLOCK		(0x00f2 << 2)

#define ANACTRL_TCON_PLL0_CNTL0		(0x00e0 << 2)/*M(7:0) N(14:10)*/
#define ANACTRL_TCON_PLL0_CNTL1		(0x00e1 << 2)/*frac(18:0)*/
#define ANACTRL_TCON_PLL1_CNTL0		(0x00e5 << 2)
#define ANACTRL_TCON_PLL1_CNTL1		(0x00e6 << 2)
#define ANACTRL_TCON_PLL2_CNTL0		(0x00ea << 2)
#define ANACTRL_TCON_PLL2_CNTL1		(0x00eb << 2)
/* ------------------ reg ----------------------*/

#define IS_VLOCK_PROC_MANUAL_MODE(md)	((md) & \
				(VLOCK_PROC_MODE_MANUAL_PLL | \
				VLOCK_PROC_MODE_MANUAL_ENC |	\
				VLOCK_PROC_MODE_MANUAL_SOFT_ENC))

#define IS_VLOCK_PROC_AUTO_MODE(md)	((md) & \
				(VLOCK_PROC_MODE_AUTO_PLL | \
				VLOCK_PROC_MODE_AUTO_ENC))

#define IS_VLOCK_PROC_PLL_MODE(md)	((md) & \
				(VLOCK_PROC_MODE_MANUAL_PLL | \
				VLOCK_PROC_MODE_AUTO_PLL))

#define IS_VLOCK_PROC_ENC_MODE(md)	((md) & \
				(VLOCK_PROC_MODE_MANUAL_ENC | \
				VLOCK_PROC_MODE_MANUAL_SOFT_ENC | \
				VLOCK_PROC_MODE_AUTO_ENC))

#define IS_VLOCK_PROC_AUTO_PLL_MODE(md) ((md) & \
					VLOCK_PROC_MODE_AUTO_PLL)

#define IS_VLOCK_PROC_AUTO_ENC_MODE(md) ((md) & \
				VLOCK_PROC_MODE_AUTO_ENC)

#define IS_VLOCK_PROC_MANUAL_ENC_MODE(md) ((md) & \
				VLOCK_PROC_MODE_MANUAL_ENC)

#define IS_VLOCK_PROC_MANUAL_PLL_MODE(md) ((md) & \
				VLOCK_PROC_MODE_MANUAL_PLL)

#define IS_VLOCK_PROC_MANUAL_SOFTENC_MODE(md) ((md) & \
				VLOCK_PROC_MODE_MANUAL_SOFT_ENC)

enum vlock_proc_pll_sel {
	vlock_proc_pll_sel_tcon = 0,
	vlock_proc_pll_sel_hdmi,
	vlock_proc_pll_sel_disable = 0xf,
};

/*Freerun type ioctl enum*/
enum vlock_output_type_e {
	VLOCK_PROC_GAME_MODE = 0,
	VLOCK_PROC_FREERUN_MODE,
	VLOCK_PROC_FREERUN_MAX
};

#define VLOCK_START_CNT		2

#define VLOCK_UPDATE_M_CNT	2
#define VLOCK_UPDATE_F_CNT	2

#define XTAL_VLOCK_CLOCK   24000000/*vlock use xtal clock*/

#define VLOCK_SUPPORT_HDMI BIT(0)
#define VLOCK_SUPPORT_CVBS BIT(1)
/*25 to 50, 30 to 60*/
#define VLOCK_SUPPORT_1TO2 0x4

#define VLOCK_SUP_MODE	(VLOCK_SUPPORT_HDMI | VLOCK_SUPPORT_CVBS | \
			 VLOCK_SUPPORT_1TO2)

/*10s for 60hz input,vlock pll stable cnt limit*/
#define VLOCK_PLL_STABLE_LIMIT	600
#define VLOCK_ENC_STABLE_CNT	180/*vlock enc stable cnt limit*/
#define VLOCK_PLL_ADJ_LIMIT 9/*vlock pll adj limit(0x300a default)*/

/*vlock_debug mask*/
#define VLOCK_DEBUG_INFO (0x1)
#define VLOCK_DEBUG_FLUSH_REG_DIS (0x2)
#define VLOCK_DEBUG_ENC_LINE_ADJ_DIS (0x4)
#define VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS (0x8)
#define VLOCK_DEBUG_AUTO_MODE_LOG_EN (0x10)
#define VLOCK_DEBUG_PLL2ENC_DIS (0x20)
#define VLOCK_DEBUG_FSM_PAUSE (0x40)
#define VLOCK_DEBUG_FORCE_ON (0x80)
#define VLOCK_DEBUG_FLASH (0x100)
#define VLOCK_DEBUG_PROTECT (0x200)
#define VLOCK_DEBUG_PHASE_OPTIMIZE (0x400)

#define VLOCK_DEBUG_INFO_ERR	(BIT(15))

#define ENCL_SYNC_LINE_LENGTH			0x1c4c
#define ENCL_SYNC_PIXEL_EN			0x1c4d
#define ENCL_SYNC_TO_LINE_EN			0x1c4e
#define ENCL_FRC_CTRL				0x1cdd

/* 0:enc;1:pll;2:manual pll */
//extern unsigned int vlock_work_mode;
//extern unsigned int vlock_en;
//extern unsigned int vecm_latch_flag;
/*extern void __iomem *amvecm_hiu_reg_base;*/
extern unsigned int probe_ok;
extern unsigned int vlock_hw_clk_ok;

//extern u32 phase_en_after_frqlock;
//extern u32 vlock_ss_en;
extern enum vlock_chip_type_e	vlk_chip_type_id;
extern enum vlock_chip_cls_e	vlk_chip_cls_id;

void lcd_ss_enable(bool flag);
unsigned int lcd_ss_status(void);
int amvecm_hiu_reg_read(unsigned int reg, unsigned int *val);
int amvecm_hiu_reg_write(unsigned int reg, unsigned int val);
void vlock_proc_input_sel(struct vlock_proc_sig_sts *vlock, unsigned int type,
			  enum vframe_source_type_e source_type);
void vlock_proc_dts_config(struct device_node *node);
#ifdef CONFIG_AMLOGIC_LCD
extern struct work_struct aml_lcd_vlock_param_work;
void vlock_proc_lcd_param_work(struct work_struct *p_work);
void lcd_vlock_m_update(int index, unsigned int vlock_m);
void lcd_vlock_frac_update(int index, unsigned int vlock_frac);
#endif
int vlock_proc_notify_callback(struct notifier_block *block,
			  unsigned long cmd, void *para);
void vlock_proc_status_init(void);
void vlock_proc_dts_match_init(struct vlock_match_data_s *pdata);
void vlock_proc_set_en(bool en);
void vlock_proc_set_phase(struct vlock_proc_sig_sts *vlock, u32 percent);
void vlock_proc_set_phase_en(struct vlock_proc_sig_sts *vlock, u32 en);
int lcd_set_ss(unsigned int level, unsigned int freq, unsigned int mode);
ssize_t vlock_proc_debug_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count);
ssize_t vlock_proc_debug_show(struct class *cla,
			 struct class_attribute *attr, char *buf);
//void vlock_clk_config(struct device *dev);
int vlock_proc_frc_is_on(void);
bool vlock_proc_get_phlock_flag(void);
bool vlock_proc_get_vlock_flag(void);
int vlock_proc_sync_frc_vporch(struct vlock_proc_frc_param frc_param);
void vlock_proc_set_sts_by_frame_lock(bool en);
//void vlock_clk_config(struct device *dev);
void vlock_proc_hiu_reg_config(struct device *dev);
void vlock_proc_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts);
#endif

