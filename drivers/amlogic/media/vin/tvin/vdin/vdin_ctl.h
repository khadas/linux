/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_ctl.h
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

#ifndef __TVIN_VDIN_CTL_H
#define __TVIN_VDIN_CTL_H
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "vdin_drv.h"

#define BIT0		0x00000001
#define BIT1		0x00000002
#define BIT2		0x00000004
#define BIT3		0x00000008
#define BIT4		0x00000010
#define BIT5		0x00000020
#define BIT6		0x00000040
#define BIT7		0x00000080
#define BIT8		0x00000100
#define BIT9		0x00000200
#define BIT10		0x00000400
#define BIT11		0x00000800
#define BIT12		0x00001000
#define BIT13		0x00002000
#define BIT14		0x00004000
#define BIT15		0x00008000
#define BIT16		0x00010000
#define BIT17		0x00020000
#define BIT18		0x00040000
#define BIT19		0x00080000
#define BIT20		0x00100000
#define BIT21		0x00200000
#define BIT22		0x00400000
#define BIT23		0x00800000
#define BIT24		0x01000000
#define BIT25		0x02000000
#define BIT26		0x04000000
#define BIT27		0x08000000
#define BIT28		0x10000000
#define BIT29		0x20000000
#define BIT30		0x40000000
#define BIT31		0x80000000

#define swap32(num) \
	({typeof(num) num_ = num; \
	 (((num_) >> 24) & 0xff) | \
	 (((num_) << 8) & 0xff0000) | \
	 (((num_) >> 8) & 0xff00) | \
	 (((num_) << 24) & 0xff000000); })

#define DV_SWAP_EN			(BIT(0))
#define DV_BUF_START_RESET		(BIT(1))
#define DV_FRAME_BUF_START_RESET	(BIT(2))
#define DV_UPDATE_DATA_MODE_DELBY_WORK	(BIT(4))
#define DV_CLEAN_UP_MEM			(BIT(5))
#define DV_READ_MODE_AXI		(BIT(6))
#define DV_CRC_FORCE_TRUE		(BIT(7))
#define DV_CRC_FORCE_FALSE		(BIT(8))

#define MEM_ALLOC_DISCRETE	(BIT(8))
#define MEM_ALLOC_FROM_CODEC	(BIT(0))

enum vdin_output_mif_e {
	VDIN_OUTPUT_TO_MIF = 0,
	VDIN_OUTPUT_TO_AFBCE = 1,
};

enum wr_sel_vdin_e {
	WR_SEL_DIS = 0,
	WR_SEL_VDIN0_NOR = 1,
	WR_SEL_VDIN0_SML = 2,
	WR_SEL_VDIN1_NOR = 3,
	WR_SEL_VDIN1_SML = 4,
};

enum vdin_mif_bits {
	MIF_8BIT = 0,
	MIF_10BIT = 1,
};

enum vdin_mif_fmt {
	MIF_FMT_YUV422 = 0,
	MIF_FMT_YUV444 = 1,
	MIF_FMT_NV12_21 = 2,
	MIF_FMT_YUV422_FULL_PACK = 3
};

/* *************************************************** */
/* *** structure definitions ************************* */
/* *************************************************** */
struct vdin_matrix_lup_s {
	unsigned int pre_offset0_1;
	unsigned int pre_offset2;
	unsigned int coef00_01;
	unsigned int coef02_10;
	unsigned int coef11_12;
	unsigned int coef20_21;
	unsigned int coef22;
	unsigned int post_offset0_1;
	unsigned int post_offset2;
};

#ifdef CONFIG_AML_LOCAL_DIMMING
struct ldim_max_s {
    /* general parameters */
	int ld_pic_rowmax;
	int ld_pic_colmax;
	int ld_stamax_hidx[11];  /* U12* 9 */
	int ld_stamax_vidx[11];  /* u12x 9 */
};
#endif

#define META_PKY_TYPE_SINGLE	0x00
#define META_PKY_TYPE_START	0x40
#define META_PKY_TYPE_MID	0x80
#define META_PKY_TYPE_TAIL	0xc0

#define DV_META_PACKET_SIZE		128
#define DV_META_HEADER_LEN		2
#define DV_META_TAIL_CRC_SIZE		4
#define DV_META_PACKET_TYPE_SIZE	3

#define DV_META_SINGLE_PKT_SIZE		119
#define DV_META_NORMAL_PKT_SIZE		121

struct dv_meta_pkt {
	u8 head0;
	u8 head1;
	u8 head2;
	u8 body[121];
	u32 crc;
};

extern unsigned int game_mode;
extern unsigned int vdin_force_game_mode;
extern int vdin_dbg_en;
extern unsigned int vdin_pc_mode;
extern int irq_max_count;

/* ************************************************************************ */
/* ******** GLOBAL FUNCTION CLAIM ******** */
/* ************************************************************************ */
u8 *vdin_vmap(ulong addr, u32 size);
void vdin_unmap_phyaddr(u8 *vaddr);
void vdin_dma_flush(struct vdin_dev_s *devp, void *vaddr,
		    int size, enum dma_data_direction dir);
void vdin_set_vframe_prop_info(struct vframe_s *vf,
			       struct vdin_dev_s *devp);
void vdin_get_crc_val(struct vframe_s *vf, struct vdin_dev_s *devp);
void vdin_get_format_convert(struct vdin_dev_s *devp);
enum vdin_format_convert_e
	vdin_get_format_convert_matrix0(struct vdin_dev_s *devp);
enum vdin_format_convert_e
	vdin_get_format_convert_matrix1(struct vdin_dev_s *devp);
void vdin_set_prob_xy(unsigned int offset, unsigned int x,
		      unsigned int y, struct vdin_dev_s *devp);
void vdin_prob_get_rgb(unsigned int offset, unsigned int *r,
		       unsigned int *g, unsigned int *b);
void vdin_set_all_regs(struct vdin_dev_s *devp);
void vdin_set_double_write_regs(struct vdin_dev_s *devp);
void vdin_set_default_regmap(struct vdin_dev_s *devp);
void vdin_set_def_wr_canvas(struct vdin_dev_s *devp);
void vdin_hw_enable(struct vdin_dev_s *devp);
void vdin_hw_disable(struct vdin_dev_s *devp);
unsigned int vdin_get_field_type(unsigned int offset);
int vdin_vsync_reset_mif(int index);
bool vdin_check_vdi6_afifo_overflow(unsigned int offset);
void vdin_clear_vdi6_afifo_overflow_flg(unsigned int offset);
void vdin_set_cutwin(struct vdin_dev_s *devp);
void vdin_set_decimation(struct vdin_dev_s *devp);
void vdin_fix_nonstd_vsync(struct vdin_dev_s *devp);
unsigned int vdin_get_meas_hcnt64(unsigned int offset);
unsigned int vdin_get_meas_vstamp(unsigned int offset);
unsigned int vdin_get_active_h(unsigned int offset);
unsigned int vdin_get_active_v(unsigned int offset);
unsigned int vdin_get_total_v(unsigned int offset);
unsigned int vdin_get_canvas_id(unsigned int offset);
void vdin_set_canvas_id(struct vdin_dev_s *devp,
			unsigned int rdma_enable,
			struct vf_entry *vfe);
unsigned int vdin_get_chma_canvas_id(unsigned int offset);
void vdin_set_chma_canvas_id(struct vdin_dev_s *devp,
			     unsigned int rdma_enable,
			     struct vf_entry *vfe);
void vdin_set_crc_pulse(struct vdin_dev_s *devp);
void vdin_enable_module(struct vdin_dev_s *devp, bool enable);
void vdin_set_matrix(struct vdin_dev_s *devp);
void vdin_set_matrixs(struct vdin_dev_s *devp, unsigned char no,
		      enum vdin_format_convert_e csc);
bool vdin_check_cycle(struct vdin_dev_s *devp);
bool vdin_write_done_check(unsigned int offset,
			   struct vdin_dev_s *devp);
void vdin_calculate_duration(struct vdin_dev_s *devp);
void vdin_wr_reverse(unsigned int offset, bool hreverse,
		     bool vreverse);
void vdin_set_hvscale(struct vdin_dev_s *devp);
void vdin_set_bitdepth(struct vdin_dev_s *devp);
void vdin_set_cm2(unsigned int offset, unsigned int w,
		  unsigned int h, unsigned int *data);
void vdin_bypass_isp(unsigned int offset);
void vdin_set_mpegin(struct vdin_dev_s *devp);
void vdin_force_gofiled(struct vdin_dev_s *devp);
void vdin_adjust_tvafesnow_brightness(void);
void vdin_set_config(struct vdin_dev_s *devp);
void vdin_set_wr_mif(struct vdin_dev_s *devp);
void vdin_dolby_config(struct vdin_dev_s *devp);
void vdin_dolby_buffer_update(struct vdin_dev_s *devp,
			      unsigned int index);
void vdin_dolby_addr_update(struct vdin_dev_s *devp, unsigned int index);
void vdin_dolby_addr_alloc(struct vdin_dev_s *devp, unsigned int size);
void vdin_dolby_addr_release(struct vdin_dev_s *devp, unsigned int size);
int vdin_event_cb(int type, void *data, void *op_arg);
void vdin_hdmiin_patch(struct vdin_dev_s *devp);
void vdin_set_top(struct vdin_dev_s *devp, unsigned int offset,
		  enum tvin_port_e port,
		  enum tvin_color_fmt_e input_cfmt,
		  unsigned int h,
		  enum bt_path_e bt_path);
void vdin_set_wr_ctrl_vsync(struct vdin_dev_s *devp,
			    unsigned int offset,
			    enum vdin_format_convert_e format_convert,
			    unsigned int full_pack,
			    unsigned int source_bitdeth,
			    unsigned int rdma_enable);

void vdin_urgent_patch_resume(unsigned int offset);
int vdin_hdr_sei_error_check(struct vdin_dev_s *devp);
void vdin_set_drm_data(struct vdin_dev_s *devp,
		       struct vframe_s *vf);
void vdin_set_freesync_data(struct vdin_dev_s *devp,
			    struct vframe_s *vf);
u32 vdin_get_curr_field_type(struct vdin_dev_s *devp);
void vdin_set_source_type(struct vdin_dev_s *devp,
			  struct vframe_s *vf);
void vdin_set_source_mode(struct vdin_dev_s *devp, struct vframe_s *vf);
void vdin_set_source_bitdepth(struct vdin_dev_s *devp,
			      struct vframe_s *vf);
void vdin_set_pixel_aspect_ratio(struct vdin_dev_s *devp,
				 struct vframe_s *vf);
void vdin_set_display_ratio(struct vdin_dev_s *devp,
			    struct vframe_s *vf);
void vdin_source_bitdepth_reinit(struct vdin_dev_s *devp);
void set_invert_top_bot(bool invert_flag);
void vdin_clk_onoff(struct vdin_dev_s *devp, bool onoff);

extern enum tvin_force_color_range_e color_range_force;

//void vdin_vlock_input_sel(unsigned int type,
//			  enum vframe_source_type_e source_type);
void vdin_set_dolby_tunnel(struct vdin_dev_s *devp);
void vdin_check_hdmi_hdr(struct vdin_dev_s *devp);
void vdin_dobly_mdata_write_en(unsigned int offset, unsigned int en);
void vdin_prob_set_xy(unsigned int offset,
		      unsigned int x, unsigned int y, struct vdin_dev_s *devp);
void vdin_prob_set_before_or_after_mat(unsigned int offset,
				       unsigned int x,
				       struct vdin_dev_s *devp);
void vdin_prob_get_yuv(unsigned int offset,
		       unsigned int *rgb_yuv0,	unsigned int *rgb_yuv1,
		       unsigned int *rgb_yuv2);
void vdin_prob_matrix_sel(unsigned int offset,
			  unsigned int sel, struct vdin_dev_s *devp);
void vdin_change_matrix(unsigned int offset,
			unsigned int matrix_csc);
void vdin_dolby_desc_sc_enable(struct vdin_dev_s *devp,
			       unsigned int  onoff);
bool vdin_is_dolby_tunnel_444_input(struct vdin_dev_s *devp);
bool vdin_is_dolby_signal_in(struct vdin_dev_s *devp);
void vdin_dolby_de_tunnel_to_12bit(struct vdin_dev_s *devp,
				   unsigned int onoff);
void vdin_wr_frame_en(unsigned int ch, unsigned int onoff);
void vdin_set_mif_onoff(struct vdin_dev_s *devp, unsigned int rdma_enable);
void vdin_vs_proc_monitor(struct vdin_dev_s *devp);
enum tvin_color_fmt_range_e
	tvin_get_force_fmt_range(enum tvin_color_fmt_e color_fmt);
bool vdin_is_convert_to_444(u32 format_convert);
bool vdin_is_convert_to_422(u32 format_convert);
bool vdin_is_convert_to_nv21(u32 format_convert);
bool vdin_is_4k(struct vdin_dev_s *devp);
void vdin_set_matrix_color(struct vdin_dev_s *devp);
void vdin_set_bist_pattern(struct vdin_dev_s *devp, unsigned int onoff, unsigned int pat);

bool is_dolby_vision_enable(void);
bool is_dolby_vision_on(void);
bool is_dolby_vision_stb_mode(void);
bool for_dolby_vision_certification(void);

void vdin_change_matrix0(u32 offset, u32 matrix_csc);
void vdin_change_matrix1(u32 offset, u32 matrix_csc);
void vdin_change_matrixhdr(u32 offset, u32 matrix_csc);

void vdin_set_frame_mif_write_addr(struct vdin_dev_s *devp,
			unsigned int rdma_enable, struct vf_entry *vfe);
void vdin_dolby_pr_meta_data(void *addr, unsigned int size);
bool vdin_is_dv_meta_data_case(struct vdin_dev_s *devp);
void vdin_dv_detunel_tunel_set(struct vdin_dev_s *devp);
void vdin_dolby_de_tunnel_to_44410bit(struct vdin_dev_s *devp,
				   unsigned int onoff);
void vdin_dolby_desc_to_4448bit(struct vdin_dev_s *devp,
			       unsigned int onoff);
void vdin_get_duration_by_fps(struct vdin_dev_s *devp);
void vdin_set_to_vpp_parm(struct vdin_dev_s *devp);
void vdin_dmc_ctrl(struct vdin_dev_s *devp, bool onoff);
#endif

