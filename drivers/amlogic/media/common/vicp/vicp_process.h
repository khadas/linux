/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_process.h
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

#ifndef _VICP_PROCESS_H_
#define _VICP_PROCESS_H_

#include <linux/amlogic/media/vicp/vicp.h>
#include "vicp_rdma.h"
#include "vicp_hdr.h"

#define ADDR_VALUE_8G    0x200000000
extern u32 print_flag;
extern u32 dump_yuv_flag;
extern u32 scaler_en;
extern u32 hdr_en;
extern u32 crop_en;
extern u32 shrink_en;
extern struct mutex vicp_mutex;
extern int cgain_lut1[65];
extern int cgain_lut0[65];
extern u32 debug_axis_en;
extern struct output_axis_s axis;
extern struct vicp_hdr_data_s *vicp_hdr;
extern u32 rdma_en;
extern u32 debug_rdma_en;
extern u32 fgrain_en;
extern u32 debug_reg_en;


/* *********************************************************************** */
/* ************************* enum definitions **************************.*/
/* *********************************************************************** */
enum vicp_lossy_compress_mode_e {
	LOSSY_COMPRESS_MODE_OFF,
	LOSSY_COMPRESS_MODE_QUAN_LUMA,
	LOSSY_COMPRESS_MODE_QUAN_CHRMA,
	LOSSY_COMPRESS_MODE_QUAN_ALL,
	LOSSY_COMPRESS_MODE_CR,
	LOSSY_COMPRESS_MODE_MAX,
};
/* *********************************************************************** */
/* ************************* struct definitions **************************.*/
/* *********************************************************************** */
struct vid_cmpr_lossy_compress_s {
	u32 compress_mode; //0:close 1:luma loosy 2:chrma loosy 3:luma&&chrma loosy 4:cr loosy
	u32 quant_diff_root_leave;
	u32 rc_en;
	u32 brst_len_add_en;
	//0:brst_len_add_en=0&brst_len_add_value=2  2:en=1&value=2 3:en=1&value=3
	u32 brst_len_add_value;
	u32 ofset_brst4_en;
	u32 compress_rate;
};

struct vid_cmpr_hdr_s {
	u32 hdr2_en;
	enum vframe_signal_fmt_e input_sig_fmt;
	enum vframe_signal_fmt_e output_sig_fmt;
};

struct vid_cmpr_top_s {
	u32 src_compress;
	u32 src_hsize;//input size
	u32 src_vsize;
	u64 src_head_baddr;
	u64 src_body_baddr;
	u32 src_fmt_mode;//2 bits default = 2, 0:yuv444 1:yuv422 2:yuv420
	u32 src_compbits;//2 bits 0-8bits 1-9bits 2-10bits
	u32 src_win_bgn_h;
	u32 src_win_end_h;
	u32 src_win_bgn_v;
	u32 src_win_end_v;
	u32 src_pip_src_mode;
	struct vframe_s *src_vf;
	u32 src_endian;
	u32 src_block_mode;
	u32 src_burst_len;
	u32 src_count;
	u32 src_num;
	bool rdma_enable;
	bool security_en;
	enum vicp_skip_mode_e skip_mode;
	u32 src_need_swap_cbcr;
	// hdr
	struct vid_cmpr_hdr_s hdr_config;
	// afbce
	u32 out_afbce_enable;//open nbit of afbce
	u64 out_head_baddr;//head_addr of afbce
	u64 out_mmu_info_baddr;//mmu_linear_addr
	u32 out_reg_init_ctrl;//pip init frame flag
	u32 out_reg_pip_mode;//pip open bit
	u32 out_reg_format_mode;//0:444 1:422 2:420
	u32 out_reg_compbits;//bits num after compression
	u32 out_hsize_in;//input data hsize
	u32 out_vsize_in;//input data vsize
	u32 out_hsize_bgnd;
	u32 out_vsize_bgnd;
	u32 out_win_bgn_h;
	u32 out_win_end_h;
	u32 out_win_bgn_v;
	u32 out_win_end_v;
	u32 out_rot_en;
	u32 out_shrk_en;
	u32 out_shrk_mode;//0=2x 1=4x 2=8x
	// wrmif
	u32 wrmif_en;
	u32 wrmif_set_separate_en;// 00 : one canvas 01 : 3 canvas(old 4:2:0).10: 2 canvas. (NV21).
	u32 wrmif_fmt_mode;// 00: 4:2:0; 01: 4:2:2; 10: 4:4:4
	u32 out_endian;
	u32 out_need_swap_cbcr;
	// 0:8 bits  1:10 bits 422(old mode,12bit) 2: 10bit 444 3:10bit 422(full pack) or 444
	u32 wrmif_bits_mode;
	u64 wrmif_canvas0_addr0;//base addr
	u64 wrmif_canvas0_addr1;//base addr
	u64 wrmif_canvas0_addr2;//base add
	// rdmif
	u32 rdmif_separate_en;
	u64 rdmif_canvas0_addr0;//base addr
	u64 rdmif_canvas0_addr1;//base addr
	u64 rdmif_canvas0_addr2;//base addr
	// rot relative
	u32 rot_rev_mode;
	u32 rot_hshrk_ratio;//0:no shrink 1:1/2 shrink 2:1/4 shrink
	u32 rot_vshrk_ratio;//0:no shrink 1:1/2 shrink 2:1/4 shrink
	u32 canvas_width[3];
	// film_grain
	u32 film_grain_en;
	u64 film_lut_addr;
	// lossy
	struct vid_cmpr_lossy_compress_s src_lossy_compress;
	struct vid_cmpr_lossy_compress_s out_lossy_compress;
	// afbce mmu_page_size
	u32 src_mmu_page_size_mode; //0:mmu_page_size=4096  1:mmu_page_size=8192
};

struct vid_cmpr_mif_s {
	u32 luma_x_start0;
	u32 luma_x_end0;
	u32 luma_y_start0;
	u32 luma_y_end0;
	u32 chrm_x_start0;
	u32 chrm_x_end0;
	u32 chrm_y_start0;
	u32 chrm_y_end0;
	u32 set_separate_en : 2;// 00 : one canvas 01 : 3 canvas(old 4:2:0).  10: 2 canvas. (NV21).
	u32 src_field_mode : 1;// 1 frame . 0 field.
	u32 fmt_mode : 2;// 00: 4:2:0; 01: 4:2:2; 10: 4:4:4
	u32 output_field_num: 1;// 0 top field	1 bottom field.
	// 0:8 bits  1:10 bits 422(old mode,12bit) 2: 10bit 444  3:10bit 422(full pack) or 444
	u32 bits_mode : 2;
	u32 burst_size_y : 2;
	u32 burst_size_cb : 2;
	u32 burst_size_cr : 2;
	u32 swap_cbcr;
	u64 canvas0_addr0; //base addr
	u64 canvas0_addr1; //base addr
	u64 canvas0_addr2; //base addr
	u32 stride_y;
	u32 stride_cb;
	u32 stride_cr;
	u32 rev_x;
	u32 rev_y;
	u32 buf_crop_en;
	u32 buf_hsize;
	u32 buf_vsize;
	u32 endian;
	u32 block_mode;
	u32 burst_len;
};

struct vid_cmpr_crop_s {
	u32 enable;
	u32 hold_line;
	u32 dimm_layer_en;
	u32 dimm_data;
	u32 frame_size_h;
	u32 frame_size_v;
	u32 win_bgn_h;
	u32 win_end_h;
	u32 win_bgn_v;
	u32 win_end_v;
};

struct vid_cmpr_afbce_s {
	u64 head_baddr;//head_addr of afbce
	u64 table_baddr;//mmu_linear_addr
	u32 reg_init_ctrl;//pip init frame flag
	u32 reg_pip_mode;//pip open bit
	u32 reg_ram_comb;//ram split bit open in di mult write case
	u32 reg_format_mode;//0:444 1:422 2:420
	u32 reg_compbits_y;//bits num after compression
	u32 reg_compbits_c;//bits num after compression
	u32 hsize_in;//input data hsize
	u32 vsize_in;//input data hsize
	u32 hsize_bgnd;//hsize of background
	u32 vsize_bgnd;//hsize of background
	u32 enc_win_bgn_h;//scope in background buffer
	u32 enc_win_end_h;//scope in background buffer
	u32 enc_win_bgn_v;//scope in background buffer
	u32 enc_win_end_v;//scope in background buffer
	u32 rev_mode;//0:normal mode
	u32 def_color_0;//def_color
	u32 def_color_1;//def_color
	u32 def_color_2;//def_color
	u32 def_color_3;//def_color
	u32 force_444_comb;//def_color
	u32 rot_en;
	u32 din_swt;
	struct vid_cmpr_lossy_compress_s lossy_compress;
	u32 mmu_page_size;
};

struct vid_cmpr_afbcd_s {
	u32 blk_mem_mode;// 1 bits 1-12x128bit/blk32x4, 0-16x128bit/blk32x4
	u32 index;//3bit: 0-5 for di_m0/m5, 6:vd1 7:vd2
	u32 hsize;//input size
	u32 vsize;
	u32 head_baddr;
	u32 body_baddr;
	u32 compbits_y;//2 bits   0-8bits 1-9bits 2-10bits
	u32 compbits_u;//2 bits   0-8bits 1-9bits 2-10bits
	u32 compbits_v;//2 bits   0-8bits 1-9bits 2-10bits
	u32 fmt_mode;//2 bits   default = 2, 0:yuv444 1:yuv422 2:yuv420
	u32 ddr_sz_mode;//1 bits   1:mmu mode
	u32 fmt444_comb;//1 bits
	u32 dos_uncomp;//1 bits   0:afbce	 1:dos
	u32 h_skip_y;
	u32 v_skip_y;
	u32 h_skip_uv;
	u32 v_skip_uv;
	u32 rev_mode;
	u32 def_color_y;
	u32 def_color_u;
	u32 def_color_v;
	u32 win_bgn_h;
	u32 win_end_h;
	u32 win_bgn_v;
	u32 win_end_v;
	u32 hz_ini_phase; // 4 bits
	u32 vt_ini_phase; // 4 bits
	u32 hz_rpt_fst0_en; // 1 bits
	u32 vt_rpt_fst0_en; // 1 bits
	u32 rot_en;
	u32 rot_hbgn;
	u32 rot_vbgn;
	u32 rot_vshrk;
	u32 rot_hshrk;
	u32 rot_drop_mode;
	u32 rot_ofmt_mode;
	u32 rot_ocompbit;
	u32 pip_src_mode;
	struct vid_cmpr_lossy_compress_s lossy_compress;
};

struct vid_cmpr_scaler_s {
	u32 device_index;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 vert_bank_length;
	u32 prehsc_en;// osd_pps has no prehsc
	u32 prevsc_en;// osd_pps has no prevsc
	u32 prehsc_rate;
	u32 prevsc_rate;
	u32 high_res_coef_en;
	u32 horz_phase_step;
	u32 vert_phase_step;
	u32 slice_x_st;
	u32 slice_x_end[4];
	u32 pps_slice;
	u32 phase_step_en;
	u32 phase_step;
	u32 vphase_type_top;
	u32 vphase_type_bot;
};

struct vid_cmpr_f2v_vphase_s {
	unsigned char rcv_num; //0~15
	unsigned char rpt_num; // 0~3
	unsigned short phase;
};

struct vid_cmpr_fgrain_s {
	bool enable;
	u32 is_afbc;
	u32 start_x;
	u32 end_x;
	u32 start_y;
	u32 end_y;
	u32 color_fmt;
	u32 color_depth;
	u64 fgs_table_adr;
};

/* **************************************************************************/
/* ************************* function definitions ***************************/
/* **************************************************************************/
irqreturn_t vicp_isr_handle(int irq, void *dev_id);
irqreturn_t vicp_rdma_handle(int irq, void *dev_id);
int vicp_crc0_check(int check_val);
int vicp_crc1_check(int chroma_en, int chroma_check, int lumma_check);
void vicp_device_init(struct vicp_device_data_s device);
int vicp_process_config(struct vicp_data_config_s *data_config,
	struct vid_cmpr_top_s *vid_cmpr_top);
int vicp_process_reset(void);
int vicp_process_task(struct vid_cmpr_top_s *vid_cmpr_top);
void set_vid_cmpr_crop(struct vid_cmpr_crop_s *crop_param);
void set_mif_stride(struct vid_cmpr_mif_s *mif, int *stride_y, int *stride_cb, int *stride_cr);
void set_vid_cmpr_shrink(int is_enable, int size, int mode_h, int mode_v);
void set_vid_cmpr_afbce(int enable, struct vid_cmpr_afbce_s *afbce, bool rdma_en);
void set_vid_cmpr_wmif(struct vid_cmpr_mif_s *wr_mif, int wrmif_en);
void set_vid_cmpr_rmif(struct vid_cmpr_mif_s *rd_mif, int urgent, int hold_line);
void set_vid_cmpr_scale(int is_enable, struct vid_cmpr_scaler_s *scaler);
void set_vid_cmpr_afbcd(int hold_line_num, bool rdma_en, struct vid_cmpr_afbcd_s *afbcd);
void set_vid_cmpr_hdr(struct vid_cmpr_hdr_s config);
void vicp_dump_reg(u32 start_addr, u32 reg_len);

#endif //_VICP_PROCESS_H_
