/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/video_sink/video.h
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

#ifndef VIDEO_H
#define VIDEO_H

#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif
#define MAX_VD_LAYERS 3

#define MOSAIC_MODE   BIT(24) /* 1 bit */
#define LAYER2_VPP    22 /* 2 bits */
#define LAYER1_VPP    20 /* 2 bits */
#define LAYER0_VPP    18 /* 2 bits */
#define LAYER2_ALPHA  BIT(17)
#define LAYER2_BUSY   BIT(16)
#define LAYER2_AFBC   BIT(15)
#define LAYER2_SCALER BIT(14)
#define LAYER2_AVAIL  BIT(13)
#define LAYER1_ALPHA  BIT(12)
#define LAYER1_BUSY   BIT(11)
#define LAYER1_AFBC   BIT(10)
#define LAYER1_SCALER BIT(9)
#define LAYER1_AVAIL  BIT(8)
#define LAYER0_ALPHA  BIT(4)
#define LAYER0_BUSY   BIT(3)
#define LAYER0_AFBC   BIT(2)
#define LAYER0_SCALER BIT(1)
#define LAYER0_AVAIL  BIT(0)

#define LAYER_BITS_SHFIT 8

enum {
	VIDEO_WIDEOPTION_NORMAL = 0,
	VIDEO_WIDEOPTION_FULL_STRETCH = 1,
	VIDEO_WIDEOPTION_4_3 = 2,
	VIDEO_WIDEOPTION_16_9 = 3,
	VIDEO_WIDEOPTION_NONLINEAR = 4,
	VIDEO_WIDEOPTION_NORMAL_NOSCALEUP = 5,
	VIDEO_WIDEOPTION_4_3_IGNORE = 6,
	VIDEO_WIDEOPTION_4_3_LETTER_BOX = 7,
	VIDEO_WIDEOPTION_4_3_PAN_SCAN = 8,
	VIDEO_WIDEOPTION_4_3_COMBINED = 9,
	VIDEO_WIDEOPTION_16_9_IGNORE = 10,
	VIDEO_WIDEOPTION_16_9_LETTER_BOX = 11,
	VIDEO_WIDEOPTION_16_9_PAN_SCAN = 12,
	VIDEO_WIDEOPTION_16_9_COMBINED = 13,
	VIDEO_WIDEOPTION_CUSTOM = 14,
	VIDEO_WIDEOPTION_AFD = 15,
	VIDEO_WIDEOPTION_NONLINEAR_T = 16,
	VIDEO_WIDEOPTION_21_9 = 17,
	VIDEO_WIDEOPTION_MAX = 18
};

/* TODO: move to register headers */
#define VPP_VADJ2_BLMINUS_EN        BIT(3)
#define VPP_VADJ2_EN                BIT(2)
#define VPP_VADJ1_BLMINUS_EN        BIT(1)
#define VPP_VADJ1_EN                BIT(0)

#define VPP_VD_SIZE_MASK            0xfff
#define VPP_VD1_START_BIT           16
#define VPP_VD1_END_BIT             0

#define VPP_REGION_MASK             0xfff
#define VPP_REGION_MASK_8K          0x1fff
#define VPP_REGION1_BIT             16
#define VPP_REGION2_BIT             0
#define VPP_REGION3_BIT             16
#define VPP_REGION4_BIT             0

#define VDIF_PIC_END_BIT             16
#define VDIF_PIC_START_BIT           0

#define VD1_FMT_LUMA_WIDTH_MASK         0xfff
#define VD1_FMT_LUMA_WIDTH_BIT          16
#define VD1_FMT_CHROMA_WIDTH_MASK       0xfff
#define VD1_FMT_CHROMA_WIDTH_BIT        0

#define VIU_MISC_AFBC_VD1           BIT(20)
#define VPP_CM_ENABLE               BIT(28)

#define VPP_VD2_ALPHA_WID           9
#define VPP_VD2_ALPHA_MASK          0x1ff
#define VPP_VD2_ALPHA_BIT           18
#define VPP_OSD2_PREBLEND           BIT(17)
#define VPP_OSD1_PREBLEND           BIT(16)
#define VPP_VD2_PREBLEND            BIT(15)
#define VPP_VD1_PREBLEND            BIT(14)
#define VPP_OSD2_POSTBLEND          BIT(13)
#define VPP_OSD1_POSTBLEND          BIT(12)
#define VPP_VD2_POSTBLEND           BIT(11)
#define VPP_VD1_POSTBLEND           BIT(10)
#define VPP_OSD1_ALPHA              BIT(9)
#define VPP_OSD2_ALPHA              BIT(8)
#define VPP_POSTBLEND_EN            BIT(7)
#define VPP_PREBLEND_EN             BIT(6)
#define VPP_PRE_FG_SEL_MASK         BIT(5)
#define VPP_PRE_FG_OSD2             BIT(5)
#define VPP_PRE_FG_OSD1             (0 << 5)
#define VPP_POST_FG_SEL_MASK        BIT(4)
#define VPP_POST_FG_OSD2            BIT(4)
#define VPP_POST_FG_OSD1            (0 << 4)

#define DNLP_SR1_CM			        BIT(3)
#define SR1_AFTER_POSTBLEN		    (0 << 3)
#define VPP_FIFO_RESET_DE           BIT(2)
#define PREBLD_SR0_VD1_SCALER		BIT(1)
#define SR0_AFTER_DNLP              (0 << 1)
#define VPP_OUT_SATURATE            BIT(0)

#define VDIF_RESET_ON_GO_FIELD       BIT(29)
#define VDIF_URGENT_BIT              27
#define VDIF_CHROMA_END_AT_LAST_LINE BIT(26)
#define VDIF_LUMA_END_AT_LAST_LINE   BIT(25)
#define VDIF_HOLD_LINES_BIT          19
#define VDIF_HOLD_LINES_MASK         0x3f
#define VDIF_LAST_LINE               BIT(18)
#define VDIF_BUSY                    BIT(17)
#define VDIF_DEMUX_MODE              BIT(16)
#define VDIF_DEMUX_MODE_422          (0 << 16)
#define VDIF_DEMUX_MODE_RGB_444      BIT(16)
#define VDIF_FORMAT_BIT              14
#define VDIF_FORMAT_MASK             3
#define VDIF_FORMAT_SPLIT            (0 << 14)
#define VDIF_FORMAT_422              BIT(14)
#define VDIF_FORMAT_RGB888_YUV444    (2 << 14)
#define VDIF_BURSTSIZE_MASK          3
#define VDIF_BURSTSIZE_CR_BIT        12
#define VDIF_BURSTSIZE_CB_BIT        10
#define VDIF_BURSTSIZE_Y_BIT         8
#define VDIF_MANULE_START_FRAME      BIT(7)
#define VDIF_CHRO_RPT_LAST           BIT(6)
#define VDIF_CHROMA_HZ_AVG           BIT(3)
#define VDIF_LUMA_HZ_AVG             BIT(2)
#define VDIF_SEPARATE_EN             BIT(1)
#define VDIF_ENABLE                  BIT(0)

#define VDIF_LOOP_MASK              0xff
#define VDIF_CHROMA_LOOP1_BIT       24
#define VDIF_LUMA_LOOP1_BIT         16
#define VDIF_CHROMA_LOOP0_BIT       8
#define VDIF_LUMA_LOOP0_BIT         0

#define HFORMATTER_REPEAT                BIT(28)
#define HFORMATTER_BILINEAR              (0 << 28)
#define HFORMATTER_PHASE_MASK    0xf
#define HFORMATTER_PHASE_BIT     24
#define HFORMATTER_RRT_PIXEL0    BIT(23)
#define HFORMATTER_YC_RATIO_1_1  (0 << 21)
#define HFORMATTER_YC_RATIO_2_1  BIT(21)
#define HFORMATTER_YC_RATIO_4_1  (2 << 21)
#define HFORMATTER_EN                    BIT(20)
#define VFORMATTER_ALWAYS_RPT    BIT(19)
#define VFORMATTER_LUMA_RPTLINE0_DE     BIT(18)
#define VFORMATTER_CHROMA_RPTLINE0_DE   BIT(17)
#define VFORMATTER_RPTLINE0_EN   BIT(16)
#define VFORMATTER_SKIPLINE_NUM_MASK    0xf
#define VFORMATTER_SKIPLINE_NUM_BIT             12
#define VFORMATTER_INIPHASE_MASK        0xf
#define VFORMATTER_INIPHASE_BIT         8
#define VFORMATTER_PHASE_MASK   (0x7f)
#define VFORMATTER_PHASE_BIT    1
#define VFORMATTER_EN                   BIT(0)

#define VPP_PHASECTL_DOUBLE_LINE    BIT(17)
#define VPP_PHASECTL_TYPE           BIT(16)
#define VPP_PHASECTL_TYPE_PROG      BIT(16)
#define VPP_PHASECTL_TYPE_INTERLACE BIT(16)
#define VPP_PHASECTL_VSL0B          BIT(15)
#define VPP_PHASECTL_DOUBLELINE_BIT 17
#define VPP_PHASECTL_DOUBLELINE_WID 2
#define VPP_PHASECTL_INIRPTNUM_MASK 0x3
#define VPP_PHASECTL_INIRPTNUM_WID  2
#define VPP_PHASECTL_INIRPTNUMB_BIT 13
#define VPP_PHASECTL_INIRCVNUM_MASK 0xf
#define VPP_PHASECTL_INIRCVNUM_WID  5
#define VPP_PHASECTL_INIRCVNUMB_BIT 8
#define VPP_PHASECTL_VSL0T          BIT(7)
#define VPP_PHASECTL_INIRPTNUMT_BIT 5
#define VPP_PHASECTL_INIRCVNUMT_BIT 0

#define VPP_PPS_LAST_LINE_FIX_BIT     24
#define VPP_SC_PREHORZ_EN_BIT_OLD       23
#define VPP_LINE_BUFFER_EN_BIT          21
#define VPP_SC_PREHORZ_EN_BIT           20
#define VPP_SC_PREVERT_EN_BIT           19
#define VPP_HF_SEP_COEF_4SRNET_EN   BIT(25)
#define VPP_PPS_LAST_LINE_FIX       BIT(24)
#define VPP_LINE_BUFFER_EN          BIT(21)
#define VPP_SC_PREHORZ_EN           BIT(20)
#define VPP_SC_PREVERT_EN           BIT(19)
#define VPP_SC_VERT_EN              BIT(18)
#define VPP_SC_HORZ_EN              BIT(17)
#define VPP_SC_TOP_EN               BIT(16)
#define VPP_SC_V1OUT_EN             BIT(15)
#define VPP_SC_RGN14_HNOLINEAR      BIT(12)
#define VPP_SC_TOP_EN_WID	    1
#define VPP_SC_TOP_EN_BIT	    16
#define VPP_SC_BANK_LENGTH_WID      3
#define VPP_SC_BANK_LENGTH_MASK     0x7
#define VPP_SC_HBANK_LENGTH_BIT     8
#define VPP_SC_RGN14_VNOLINEAR      BIT(4)
#define VPP_SC_VBANK_LENGTH_BIT     0

#define VPP_HSC_INIRPT_NUM_MASK     0x3
#define VPP_HSC_INIRPT_NUM_BIT      21
#define VPP_HSC_INIRPT_NUM_WID      2
#define VPP_HSC_INIRPT_NUM_BIT_8TAP 20
#define VPP_HSC_INIRPT_NUM_WID_8TAP 4
#define VPP_HSC_INIRCV_NUM_MASK     0xf
#define VPP_HSC_INIRCV_NUM_BIT      16
#define VPP_HSC_INIRCV_NUM_WID      4
#define VPP_HSC_TOP_INI_PHASE_WID   16
#define VPP_HSC_TOP_INI_PHASE_BIT   0
#define VPP_PREHSC_FLT_NUM_BIT_T5      7
#define VPP_PREHSC_FLT_NUM_WID_T5      4
#define VPP_PREVSC_FLT_NUM_BIT_T5      4
#define VPP_PREVSC_FLT_NUM_WID_T5      3
#define VPP_PREHSC_DS_RATIO_BIT_T5     2
#define VPP_PREHSC_DS_RATIO_WID_T5     2
#define VPP_PREVSC_DS_RATIO_BIT_T5     0
#define VPP_PREVSC_DS_RATIO_WID_T5     2

#define VPP_COEF11_MODE_BIT_T7        20
#define VPP_COEF11_MODE_WID_T7         1
#define VPP_PREVSC_NOR_RD_BIT_T7      16
#define VPP_PREVSC_NOR_RD_WID_T7       4
#define VPP_PREHSC_NOR_RS_BIT_T7      12
#define VPP_PREHSC_NOR_RS_WID_T7       4
#define VPP_PREHSC_FLT_NUM_BIT_T7      8
#define VPP_PREHSC_FLT_NUM_WID_T7      4
#define VPP_PREVSC_FLT_NUM_BIT_T7      4
#define VPP_PREVSC_FLT_NUM_WID_T7      4
#define VPP_PREHSC_DS_RATIO_BIT_T7     2
#define VPP_PREHSC_DS_RATIO_WID_T7     2
#define VPP_PREVSC_DS_RATIO_BIT_T7     0
#define VPP_PREVSC_DS_RATIO_WID_T7     2

#define VPP_PREHSC_FLT_NUM_BIT      0
#define VPP_PREHSC_FLT_NUM_WID      4
#define VPP_PREHSC_COEF3_BIT        24
#define VPP_PREHSC_COEF3_WID        8
#define VPP_PREHSC_COEF2_BIT        16
#define VPP_PREHSC_COEF2_WID        8
#define VPP_PREHSC_COEF1_BIT        8
#define VPP_PREHSC_COEF1_WID        8
#define VPP_PREHSC_COEF0_BIT        0
#define VPP_PREHSC_COEF0_WID        8

#define VPP_PREHSC_8TAP_COEF3_BIT        16
#define VPP_PREHSC_8TAP_COEF3_WID        10
#define VPP_PREHSC_8TAP_COEF2_BIT        0
#define VPP_PREHSC_8TAP_COEF2_WID        10
#define VPP_PREHSC_8TAP_COEF1_BIT        16
#define VPP_PREHSC_8TAP_COEF1_WID        10
#define VPP_PREHSC_8TAP_COEF0_BIT        0
#define VPP_PREHSC_8TAP_COEF0_WID        10

#define VPP_PREVSC_COEF1_BIT        8
#define VPP_PREVSC_COEF1_WID        8
#define VPP_PREVSC_COEF0_BIT        0
#define VPP_PREVSC_COEF0_WID        8
#define VPP_PREVSC_COEF1_BIT_T7        16
#define VPP_PREVSC_COEF1_WID_T7        16
#define VPP_PREVSC_COEF0_BIT_T7        0
#define VPP_PREVSC_COEF0_WID_T7        16
#define VPP_OFIFO_LINELEN_MASK      0xfff
#define VPP_OFIFO_LINELEN_BIT       20
#define VPP_INV_VS                  BIT(19)
#define VPP_INV_HS                  BIT(18)
#define VPP_FORCE_FIELD_EN          BIT(17)
#define VPP_FORCE_FIELD_TYPE_MASK   BIT(16)
#define VPP_FORCE_FIELD_TOP         (0 << 16)
#define VPP_FORCE_FIELD_BOTTOM      BIT(16)
#define VPP_FOURCE_GO_FIELD         BIT(15)
#define VPP_FOURCE_GO_LINE          BIT(14)
#define VPP_OFIFO_SIZE_WID          13
#define VPP_OFIFO_SIZE_MASK         0xfff
#define VPP_OFIFO_SIZE_BIT          0

#define VPP_SEP_COEF            BIT(16)
#define VPP_COEF_IDXINC         BIT(15)
#define VPP_COEF_RD_CBUS        BIT(14)
#define VPP_COEF_SEP_EN	        BIT(13)
#define VPP_COEF_9BIT           BIT(9)
#define VPP_COEF_TYPE           BIT(8)
#define VPP_COEF_VERT           (0 << 8)
#define VPP_COEF_VERT_CHROMA    BIT(7)
#define VPP_COEF_HORZ           BIT(8)
#define VPP_COEF_INDEX_MASK     0x7f
#define VPP_COEF_INDEX_BIT      0

#define VIDEO_USE_4K_RAM        0x100

enum {
	VPP_SEP_COEF_VERT_LUMA         = 0x0 << 17,
	VPP_SEP_COEF_VERT_CHROMA       = 0x1 << 17,
	VPP_SEP_COEF_HORZ_LUMA         = 0x2 << 17,
	VPP_SEP_COEF_HORZ_LUMA_PARTB   = 0x3 << 17,
	VPP_SEP_COEF_HORZ_CHROMA       = 0x4 << 17,
	VPP_SEP_COEF_HORZ_CHROMA_PARTB = 0x5 << 17,
};

enum vpu_module_e {
	VD1_SCALER,
	VD2_SCALER,
	SR0,
	SR1,
	VD1_HDR_CORE,
	VD2_HDR_CORE,
	OSD1_HDR_CORE,
	OSD2_HDR_CORE,
	OSD3_HDR_CORE,
	DV_TVCORE,
};

#define AMVIDEO_UPDATE_OSD_MODE	0x00000001
#define AMVIDEO_UPDATE_PREBLEND_MODE	0x00000002
#define AMVIDEO_UPDATE_SIGNAL_MODE      0x00000003
#define AMVIDEO_UPDATE_VT_REG      0x00000004
#define AMVIDEO_UPDATE_FRC_CHAR_FLASH    0x00000005 /*fix High frequency char flicker*/

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
int amvideo_notifier_call_chain(unsigned long val, void *v);
#else
static inline int amvideo_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
}
#endif

/* 0: off, 1: vpp mute 2:dv mute */
#define VIDEO_MUTE_OFF		0
#define VIDEO_MUTE_ON_VPP	1
#define VIDEO_MUTE_ON_DV	2

#define VIDEO_TESTPATTERN_ON  0
#define VIDEO_TESTPATTERN_OFF 1

#define POST_SLICE_NUM 4
#define VD_SLICE_NUM   4
struct slice_info {
	u32 hsize;     /*slice hsize*/
	u32 vsize;     /*slice vsize*/
};

struct vpp_post_info_t {
	u32 slice_num;   /*valid slice num*/
	u32 overlap_hsize;
	u32 vpp_post_blend_hsize;   /*blend out hsize*/
	u32 vpp_post_blend_vsize;   /*blend out vsize*/
	struct slice_info slice[POST_SLICE_NUM];
};

struct vd_proc_info_t {
	bool vd2_prebld_4k120_en;
	struct slice_info slice[VD_SLICE_NUM];
};

struct vd_proc_amvecm_info_t {
	u32 slice_num;
	u32 vd1_in_hsize;
	u32 vd1_in_vsize;
	u32 vd1_dout_hsize;
	u32 vd1_dout_vsize;
	struct slice_info slice[VD_SLICE_NUM];
	u32 vd2_in_hsize;
	u32 vd2_in_vsize;
	u32 vd2_dout_hsize;
	u32 vd2_dout_vsize;
};

struct video_input_info {
	u32 width;
	u32 height;
	u32 crop_left;
	u32 crop_right;
	u32 crop_top;
	u32 crop_bottom;
};

void set_video_mute(bool on);
int get_video_mute(void);
void set_output_mute(bool on);
int get_output_mute(void);
void set_vdx_test_pattern(u32 index, bool on, u32 color);
void get_vdx_test_pattern(u32 index, bool *on, u32 *color);
void set_postblend_test_pattern(bool on, u32 color);
void get_postblend_test_pattern(bool *on, u32 *color);
u32 get_first_pic_coming(void);
u32 get_toggle_frame_count(void);

int query_video_status(int type, int *value);
u32 set_blackout_policy(int policy);
u32 get_blackout_policy(void);
u32 set_blackout_pip_policy(int policy);
u32 get_blackout_pip_policy(void);
u32 get_blackout_pip2_policy(void);
void set_video_angle(u32 s_value);
u32 get_video_angle(void);
u32 get_video_hold_state(void);
unsigned int DI_POST_REG_RD(unsigned int addr);
int DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
void DI_POST_UPDATE_MC(void);
void vsync_notify_videosync(void);
bool get_video_reverse(void);
/* int get_osd_reverse(void); */
void vsync_notify_video_composer(void);
void multi_vsync_notify_video_composer(void);
int _video_set_disable(u32 val);
int _videopip_set_disable(u32 index, u32 val);
void video_set_global_output(u32 index, u32 val);
u32 video_get_layer_capability(void);
int get_video_src_max_buffer(u8 layer_id,
	u32 *src_width, u32 *src_height);
int get_video_src_min_buffer(u8 layer_id,
	u32 *src_width, u32 *src_height);
void set_video_crop_ext(int layer_index, int *p);
void set_video_window_ext(int layer_index, int *p);
void set_video_zorder_ext(int layer_index, int zorder);
s32 set_video_path_select(const char *recv_name, u8 layer_id);
s32 set_sideband_type(s32 type, u8 layer_id);
void vpp_probe_en_set(u32 enable);
bool is_di_hf_y_reverse(void);
void set_post_blend_dummy_data(u32 vpp_index,
	u32 dummy_data, u32 dummy_alpha);
struct vpp_post_info_t *get_vpp_post_amdv_info(void);
struct vd_proc_info_t *get_vd_proc_amdv_info(void);
struct vd_proc_amvecm_info_t *get_vd_proc_amvecm_info(void);

#ifdef CONFIG_AMLOGIC_MEDIA_FRAME_SYNC
int tsync_set_tunnel_mode(int mode);
#endif

#ifdef CONFIG_AMLOGIC_VIDEOSYNC
void videosync_pcrscr_update(s32 inc, u32 base);
void videosync_pcrscr_inc(s32 inc);
void vsync_notify_videosync(void);
#endif

#ifdef CONFIG_AMLOGIC_VIDEOQUEUE
void vsync_notify_videoqueue(void);
void videoqueue_pcrscr_update(s32 inc, u32 base);
#endif

#ifdef CONFIG_AMLOGIC_VDETECT
int vdetect_get_frame_nn_info(struct vframe_s *vframe);
#endif
u32 get_tvin_delay_start(void);
void set_tvin_delay_start(u32 start);
u32 get_tvin_delay_duration(void);
void set_tvin_delay_duration(u32 time);
u32 get_tvin_delay(void);
u32 get_tvin_delay_max_ms(void);
u32 get_tvin_delay_min_ms(void);
u32 get_tvin_dv_flag(void);
void get_vdx_axis(u32 index, int *buf);

void vpu_module_clk_enable(u32 vpp_index, u32 module, bool async);
void vpu_module_clk_disable(u32 vpp_index, u32 module, bool async);

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
int frc_get_video_latency(void);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
int get_vdin_add_delay_num(void);
void vdin_start_notify_vpp(struct tvin_to_vpp_info_s *tvin_info);
#endif
u32 get_playback_delay_duration(void);
void get_video_axis_offset(s32 *x_offset, s32 *y_offset);
bool is_vpp0(u8 layer_id);
bool is_vpp1(u8 layer_id);
bool is_vpp2(u8 layer_id);
int get_receiver_id(u8 layer_id);
int proc_lowlatency_frame(u8 instance_id);
bool check_av1_hdr10p(char *p);
int get_output_pcrscr_info(s32 *inc, u32 *base);
void  get_video_input_info(struct video_input_info *input_info);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define OVER_FIELD_NORMAL 0
#define OVER_FIELD_NEW_VF 1
#define OVER_FIELD_RDMA_READY 2
#define OVER_FIELD_STATE_MAX 3
void update_over_field_states(u32 new_state, bool force);
#endif
u32 get_slice_num(u32 layer_id);
#endif /* VIDEO_H */
