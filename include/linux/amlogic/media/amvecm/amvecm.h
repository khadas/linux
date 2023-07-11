/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/amvecm/amvecm.h
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

#ifndef AMVECM_H
#define AMVECM_H

//#define T7_BRINGUP_MULTI_VPP

#include "linux/amlogic/media/amvecm/ve.h"
#include "linux/amlogic/media/amvecm/cm.h"
#include "linux/amlogic/media/amvecm/hdr10_tmo_alg.h"
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include <drm/drmP.h>
#include <../drivers/amlogic/media/enhancement/amvecm/vlock.h>
#include <uapi/linux/amlogic/amvecm_ext.h>


#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
bool is_amdv_enable(void);
bool is_amdv_on(void);
bool is_amdv_stb_mode(void);
bool for_amdv_certification(void);
bool is_amdv_frame(struct vframe_s *vf);
void amdv_set_toggle_flag(int flag);
#endif

#ifndef MAX
#define MAX(a, b) ({ \
	typeof(a) _a = a; \
	typeof(b) _b = b; \
	_a > _b ? _a : _b; \
	})
#endif // MAX

#ifndef MIN
#define MIN(c, d) ({ \
	typeof(c) _c = c; \
	typeof(d) _d = d; \
	_c < _d ? _c : _d; \
	})
#endif // MIN

#ifndef FMETER_SCORE
#define FMETER_SCORE(x, y, z) ({ \
			typeof(x) _x = x; \
			typeof(y) _y = y; \
			typeof(z) _z = z; \
			MAX(MIN(100, (_x * 1000) / (_x + _y + _z)), \
			MIN(100, (_x + _y) * 1000 / \
			(_x + _y + _z) / 3)); \
			})
#endif // FMETER_SCORE

/* struct ve_dnlp_s          video_ve_dnlp; */
#define FLAG_COLORPRI_LATCH     BIT(31)
#define FLAG_VADJ1_COLOR        BIT(30)
#define FLAG_VE_DNLP            BIT(29)
#define FLAG_VE_NEW_DNLP        BIT(28)
#define FLAG_VE_LC_CURV         BIT(27)
#define FLAG_HDR_OOTF_LATCH     BIT(26)
#define FLAG_3D_BLACK_DIS       BIT(25)
#define FLAG_3D_BLACK_EN        BIT(24)
#define FLAG_3D_SYNC_DIS        BIT(23)
#define FLAG_3D_SYNC_EN         BIT(22)
#define FLAG_VLOCK_DIS          BIT(21)
#define FLAG_VLOCK_EN          BIT(20)
#define FLAG_VE_DNLP_EN         BIT(19)
#define FLAG_VE_DNLP_DIS        BIT(18)
#define FLAG_VADJ1_CON			BIT(17)
#define FLAG_VADJ1_BRI			BIT(16)
#define FLAG_GAMMA_TABLE_EN     BIT(15)
#define FLAG_GAMMA_TABLE_DIS    BIT(14)
#define FLAG_GAMMA_TABLE_R      BIT(13)
#define FLAG_GAMMA_TABLE_G      BIT(12)
#define FLAG_GAMMA_TABLE_B      BIT(11)
#define FLAG_RGB_OGO            BIT(10)
#define FLAG_VADJ_EN            BIT(9)
#define FLAG_MATRIX_UPDATE      BIT(8)
#define FLAG_BRI_CON            BIT(7)
#define FLAG_LVDS_FREQ_SW       BIT(6)
#define FLAG_REG_MAP5           BIT(5)
#define FLAG_REG_MAP4           BIT(4)
#define FLAG_REG_MAP3           BIT(3)
#define FLAG_REG_MAP2           BIT(2)
#define FLAG_REG_MAP1           BIT(1)
#define FLAG_REG_MAP0           BIT(0)

/*
 *#define VPP_VADJ2_BLMINUS_EN        (1 << 3)
 *#define VPP_VADJ2_EN                (1 << 2)
 *#define VPP_VADJ1_BLMINUS_EN        (1 << 1)
 *#define VPP_VADJ1_EN                (1 << 0)
 */
#define FLAG_GAMMA_TABLE_EN_SUB     BIT(19)
#define FLAG_GAMMA_TABLE_DIS_SUB    BIT(18)
#define FLAG_GAMMA_TABLE_R_SUB      BIT(17)
#define FLAG_GAMMA_TABLE_G_SUB      BIT(16)
#define FLAG_GAMMA_TABLE_B_SUB      BIT(15)

#define CT_UPDATE                   BIT(16)
#define BS_UPDATE                   BIT(15)
#define LUT3D_UPDATE                BIT(14)
#define BLE_WHE_UPDATE              BIT(13)
#define GAMMA_CRC_FAIL              BIT(12)
#define GAMMA_CRC_PASS              BIT(11)
#define GAMMA_READ_B                BIT(10)
#define GAMMA_READ_G                BIT(9)
#define GAMMA_READ_R                BIT(8)
#define VPP_EYE_PROTECT_UPDATE      BIT(7)
#define VPP_PRE_GAMMA_UPDATE        BIT(6)
#define VPP_MARTIX_GET              BIT(5)
#define VPP_MARTIX_UPDATE           BIT(4)

/*PQ USER LATCH*/
#define PQ_USER_PQ_MODULE_CTL      BIT(26)
#define PQ_USER_OVERSCAN_RESET     BIT(25)
#define PQ_USER_CMS_SAT_HUE        BIT(24)
#define PQ_USER_CMS_CURVE_HUE_HS   BIT(23)
#define PQ_USER_CMS_CURVE_HUE      BIT(22)
#define PQ_USER_CMS_CURVE_LUMA     BIT(21)
#define PQ_USER_CMS_CURVE_SAT      BIT(20)
#define PQ_USER_SR1_DIRECTION_DIS  BIT(19)
#define PQ_USER_SR1_DIRECTION_EN   BIT(18)
#define PQ_USER_SR0_DIRECTION_DIS  BIT(17)
#define PQ_USER_SR0_DIRECTION_EN   BIT(16)
#define PQ_USER_SR1_DEJAGGY_DIS    BIT(15)
#define PQ_USER_SR1_DEJAGGY_EN     BIT(14)
#define PQ_USER_SR0_DEJAGGY_DIS    BIT(13)
#define PQ_USER_SR0_DEJAGGY_EN     BIT(12)
#define PQ_USER_SR1_DERING_DIS     BIT(11)
#define PQ_USER_SR1_DERING_EN      BIT(10)
#define PQ_USER_SR0_DERING_DIS     BIT(9)
#define PQ_USER_SR0_DERING_EN      BIT(8)
#define PQ_USER_SR1_PK_DIS         BIT(7)
#define PQ_USER_SR1_PK_EN          BIT(6)
#define PQ_USER_SR0_PK_DIS         BIT(5)
#define PQ_USER_SR0_PK_EN          BIT(4)
#define PQ_USER_BLK_SLOPE          BIT(3)
#define PQ_USER_BLK_START          BIT(2)
#define PQ_USER_BLK_DIS            BIT(1)
#define PQ_USER_BLK_EN             BIT(0)

/*white balance latch*/
#define MTX_BYPASS_RGB_OGO			BIT(0)
#define MTX_RGB2YUVL_RGB_OGO		BIT(1)

#define DNLP_PARAM_RD_UPDATE 0x1
#define DNLP_CV_RD_UPDATE 0x2
#define WB_PARAM_RD_UPDATE 0x4
#define LC_CUR_RD_UPDATE 0x8
#define LC_PARAM_RD_UPDATE 0x10
#define LC_CUR2_RD_UPDATE 0x20

#define BLK_ADJ_EN        0x40
#define BLK_END           0x80
#define BLK_SLP           0x100
#define BRT_ADJ_EN        0x200
#define BRT_START         0x400
#define BRT_SLP           0x800

#define CM_SAT_DEBUG_FLAG 0x1
#define CM_HUE_DEBUG_FLAG 0x2
#define CM_LUMA_DEBUG_FLAG 0x4
#define CM_HUE_BY_HIS_DEBUG_FLAG 0x8


#define CSC_FLAG_TOGGLE_FRAME	1
#define CSC_FLAG_CHECK_OUTPUT	2
#define CSC_FLAG_FORCE_SIGNAL	4

//48-56hz gm_tb[1][3]
//57-64hz gm_tb[2][3]
//65-72hz gm_tb[3][3]
//73-80hz gm_tb[4][3]
//81-88hz gm_tb[5][3]
//89-96hz gm_tb[6][3]
//97-104hz gm_tb[7][3]
//105-112hz gm_tb[8][3]
//112-120hz gm_tb[9][3]
//121-144hz gm_tb[10][3]

enum cm_hist_e {
	CM_HUE_HIST = 0,
	CM_SAT_HIST,
	CM_MAX_HIST
};

enum dv_pq_ctl_e {
	DV_PQ_TV_BYPASS = 0,
	DV_PQ_STB_BYPASS,
	DV_PQ_CERT,
	DV_PQ_REC,
};

enum wr_md_e {
	WR_VCB = 0,
	WR_DMA,
};

/* CMS ioctl data structure */
struct cms_data_s {
	int color;
	int value;
};

enum pd_comb_fix_lvl_e {
	PD_LOW_LVL = 0,
	PD_MID_LVL,
	PD_HIG_LVL,
	PD_DEF_LVL
};

enum vpp_transfer_characteristic_e {
	VPP_ST_NULL = 0,
	VPP_ST709 = 0x1,
	VPP_ST2084 = 0x2,
	VPP_ST2094_40 = 0x4,
};

enum ve_source_input_e {
	SOURCE_INVALID = -1,
	SOURCE_TV = 0,
	SOURCE_AV1,
	SOURCE_AV2,
	SOURCE_YPBPR1,
	SOURCE_YPBPR2,
	SOURCE_HDMI1,
	SOURCE_HDMI2,
	SOURCE_HDMI3,
	SOURCE_HDMI4,
	SOURCE_VGA,
	SOURCE_MPEG,
	SOURCE_DTV,
	SOURCE_SVIDEO,
	SOURCE_IPTV,
	SOURCE_DUMMY,
	SOURCE_SPDIF,
	SOURCE_ADTV,
	SOURCE_MAX,
};

/*pq_timing:
 *SD/HD/FHD/UHD for DTV/MEPG,
 *NTST_M/NTST_443/PAL_I/PAL_M/PAL_60/PAL_CN/SECAM/NTST_50 for AV/ATV
 */
enum ve_pq_timing_e {
	TIMING_SD_480 = 0,
	TIMING_SD_576,
	TIMING_HD,
	TIMING_FHD,
	TIMING_UHD,
	TIMING_NTST_M,
	TIMING_NTST_443,
	TIMING_PAL_I,
	TIMING_PAL_M,
	TIMING_PAL_60,
	TIMING_PAL_CN,
	TIMING_SECAM,
	TIMING_NTSC_50,
	TIMING_MAX,
};

/*overscan:
 *length 0~31bit :number of crop;
 *src_timing: bit31: on: load/save all crop
			  bit31: off: load one according to timing*
			  bit30: AFD_enable: 1 -> on; 0 -> off*
			  screen mode: bit24~bit29*
			  source: bit16~bit23 -> source*
			  timing: bit0~bit15 -> sd/hd/fhd/uhd*
 *value1: 0~15bit hs   16~31bit he*
 *value2: 0~15bit vs   16~31bit ve*
 */
struct ve_pq_overscan_s {
	unsigned int load_flag;
	unsigned int afd_enable;
	unsigned int screen_mode;
	enum ve_source_input_e source;
	enum ve_pq_timing_e timing;
	unsigned int hs;
	unsigned int he;
	unsigned int vs;
	unsigned int ve;
};

extern struct ve_pq_overscan_s overscan_table[TIMING_MAX];

enum vlk_chiptype {
	vlock_chip_null,
	vlock_chip_txl,
	vlock_chip_txlx,
	vlock_chip_txhd,
	vlock_chip_tl1,
	vlock_chip_tm2,
	vlock_chip_sm1,
	vlock_chip_t5,/*same as t5d/t5w*/
	vlock_chip_t7,
	vlock_chip_t3,
};

enum chip_type {
	chip_other = 0,
	chip_t7,
	chip_t3,
	chip_t5w,
	chip_s5,
	chip_a4
};

enum chip_cls_e {
	OTHER_CLS = 0,
	TV_CHIP,
	STB_CHIP,
	SMT_CHIP,
	AD_CHIP
};

enum vlock_hw_ver_e {
	/*gxtvbb*/
	vlock_hw_org = 0,
	/*
	 *txl
	 *txlx
	 */
	vlock_hw_ver1,
	/* tl1 later
	 * fix bug:i problem
	 * fix bug:affect ss function
	 * add: phase lock
	 * tm2: have separate pll:tcon pll and hdmitx pll
	 */
	vlock_hw_ver2,
	/* tm2 version B
	 * fix some bug
	 */
	vlock_hw_tm2verb,
};

struct vecm_match_data_s {
	enum chip_type chip_id;
	enum chip_cls_e chip_cls;
	enum vlk_chiptype vlk_chip;
	u32 vlk_support;
	u32 vlk_new_fsm;
	enum vlock_hw_ver_e vlk_hwver;
	u32 vlk_phlock_en;
	u32 vlk_pll_sel;/*independent panel pll and hdmitx pll*/
	u32 reg_addr_vlock;
	u32 reg_addr_hiu;
	u32 reg_addr_anactr;
	u32 vlk_ctl_for_frc;/*control frc flash patch*/
	u32 vrr_support_flag;/*frame lock control type 1:support 0:unsupport*/
};

enum vd_path_e {
	VD1_PATH = 0,
	VD2_PATH = 1,
	VD3_PATH = 2,
	VD_PATH_MAX = 3
};

enum vpp_index_e {
	VPP_TOP0 = 0,
	VPP_TOP1 = 1,
	VPP_TOP2 = 2,
	VPP_TOP_MAX_S = 3
};

enum vpp_slice_e {
	SLICE0 = 0,
	SLICE1,
	SLICE2,
	SLICE3,
	SLICE_MAX
};

enum vadj_index_e {
	VE_VADJ1 = 0,
	VE_VADJ2
};

/*flag:
 *bit 0: brigtness
 *bit 1: contrast
 *bit 2: saturation
 *bit 3: hue
 */
struct vdj_parm_s {
	int flag;
	int brightness;
	int contrast;
	int sat_hue;
};

struct ve_pq_table_s {
	unsigned int src_timing;
	unsigned int value1;
	unsigned int value2;
	unsigned int reserved1;
	unsigned int reserved2;
};

/*3D LUT IOCTL command list*/
struct table_3dlut_s {
	unsigned int data[17 * 17 * 17][3];
} /*table_3dlut_s */;

extern signed int vd1_brightness, vd1_contrast;
extern bool gamma_en;
extern unsigned int atv_source_flg;
extern enum hdr_type_e hdr_source_type;
extern unsigned int pd_detect_en;
extern bool wb_en;
extern struct pq_ctrl_s pq_cfg;

extern struct pq_ctrl_s pq_cfg;
extern struct pq_ctrl_s dv_cfg_bypass;
extern unsigned int lc_offset;
extern unsigned int pq_user_latch_flag;
extern struct eye_protect_s eye_protect;
extern unsigned int vecm_latch_flag2;

extern enum ecm_color_type cm_cur_work_color_md;
extern int cm2_debug;
extern int bs_3dlut_en;
extern unsigned int vecm_latch_flag2;

extern unsigned int ct_en;
void bs_latch(void);
void ct_latch(void);
void lut3d_latch(void);

extern enum chip_type chip_type_id;
extern enum chip_cls_e chip_cls_id;

int amvecm_on_vs(struct vframe_s *display_vf,
		 struct vframe_s *toggle_vf,
		 int flags,
		 unsigned int sps_h_en,
		 unsigned int sps_v_en,
		 unsigned int sps_w_in,
		 unsigned int sps_h_in,
		 unsigned int cm_in_w,
		 unsigned int cm_in_h,
		 enum vd_path_e vd_path,
		 enum vpp_index_e vpp_index);
void refresh_on_vs(struct vframe_s *vf, struct vframe_s *rpt_vf);
void pc_mode_process(void);
void pq_user_latch_process(void);
void vlock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts);
void frame_lock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts);
int frc_input_handle(struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts);
void get_hdr_process_name(int id, char *name, char *output_fmt);

void vpp_vd_adj1_saturation_hue(signed int sat_val,
				signed int hue_val, struct vframe_s *vf);
void amvecm_sharpness_enable(int sel);
int metadata_read_u32(uint32_t *value);
int metadata_wait(struct vframe_s *vf);
int metadata_sync(u32 frame_id, uint64_t pts);
void amvecm_wakeup_queue(void);
void lc_load_curve(struct ve_lc_curve_parm_s *p);

int amvecm_drm_get_gamma_size(u32 index);
void amvecm_drm_init(u32 index);
int amvecm_drm_gamma_set(u32 index,
			 struct drm_color_lut *lut, int lut_size);
int amvecm_drm_gamma_get(u32 index, u16 *red, u16 *green, u16 *blue);
int amvecm_drm_gamma_enable(u32 index);
int amvecm_drm_gamma_disable(u32 index);
int am_meson_ctm_set(u32 index, struct drm_color_ctm *ctm);
int am_meson_ctm_disable(void);

void enable_osd1_mtx(unsigned int en);
void set_cur_hdr_policy(uint policy);
bool di_api_mov_sel(unsigned int mode,
		    unsigned int *pdate);
enum hdr_type_e get_cur_source_type(enum vd_path_e vd_path,
	enum vpp_index_e vpp_index);

int amvecm_set_saturation_hue(int mab);
void amvecm_saturation_hue_update(int offset_val);

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
int frc_set_seg_display(u8 enable, u8 seg1, u8 seg2, u8 seg3);
#endif

/*ai detected scenes*/
enum detect_scene_e {
	BLUE_SCENE = 0,
	GREEN_SCENE,
	SKIN_TONE_SCENE,
	PEAKING_SCENE,
	SATURATION_SCENE,
	DYNAMIC_CONTRAST_SCENE,
	NOISE_SCENE,
	SCENE_MAX
};

/*detected single scene process*/
struct single_scene_s {
	int enable;
	int (*func)(int offset, int enable);
};
extern struct single_scene_s detected_scenes[SCENE_MAX];
extern int freerun_en;
u32 hdr_set(u32 module_sel, u32 hdr_process_select, enum vpp_index_e vpp_index);
int vinfo_lcd_support(void);
int vinfo_hdmi_out_fmt(void);
int dv_pq_ctl(enum dv_pq_ctl_e ctl);
int cm_force_update_flag(void);
int get_lum_ave(void);

enum demo_module_e {
	E_DEMO_SR = 0,/*SHARPNESS/DEJEGGAY/DNLP/LC*/
	E_DEMO_CM,
	E_DEMO_LC,
	E_DEMO_MAX
};

struct demo_size_s {
	int sr_hsize;
	int sr_vsize;
	int cm_hsize;
	int cm_vsize;
};

enum enable_state_e {
	E_DISABLE = 0,
	E_ENABLE
};

struct demo_data_s {
	int update_flag[E_DEMO_MAX];
	enum demo_module_e mod[E_DEMO_MAX];
	struct demo_size_s in_size;
	enum enable_state_e en_st[E_DEMO_MAX];
};

void bs_ct_latch(void);
int pkt_adv_chip(void);
extern unsigned int ai_color_enable;

struct venc_gamma_table_s {
	u16 gamma_r[257];
	u16 gamma_g[257];
	u16 gamma_b[257];
};

struct gamma_data_s {
	int max_idx;
	unsigned int auto_inc;
	int addr_port;
	int data_port;
	struct venc_gamma_table_s gm_tbl;
	struct venc_gamma_table_s dbg_gm_tbl;
};

struct gamma_data_s *get_gm_data(void);
extern u32 _get_cur_enc_line(void);
extern int rd_vencl;

int register_osd_status_cb(int (*get_osd_enable_status)(u32 index));
#endif /* AMVECM_H */

