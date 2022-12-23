/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDIN_S5_REGS_H
#define __VDIN_S5_REGS_H

#define VIU_WR_BAK_CTRL          0x1a25
#define VPU_VIU2VDIN_HDN_CTRL    0x2780
#define VPU_VIU_VDIN_IF_MUX_CTRL 0x2783
#define VDIN_REG_BANK0_START     0x0100
#define VDIN_REG_BANK0_END       0x0167
#define VDIN_REG_BANK1_START     0x0200
#define VDIN_REG_BANK1_END       0x02ee
#define S5_VPP_POST_HOLD_CTRL    0x1d1f

enum vdin_vdi_x_e {
	VDIN_VDI0_MPEG		= 0,	/* mpeg in */
	VDIN_VDI1_BT656		= 1,	/* first bt656 input */
	VDIN_VDI2_RESERVED	= 2,	/* reserved */
	VDIN_VDI3_TV_DECODE_IN		= 3,	/* tv decode input */
	VDIN_VDI4_HDMIRX	= 4,	/* hdmi rx dual pixel input */
	VDIN_VDI5_DVI		= 5,	/* digital video input */
	VDIN_VDI6_LOOPBACK_1	= 6,	/* first internal loopback input */
	VDIN_VDI7_MIPI_CSI2	= 7,	/* mipi csi2 input */
	VDIN_VDI8_LOOPBACK_2	= 8,	/* second internal loopback input */
	VDIN_VDI9_SECOND_BT656	= 9		/* second bt656 input */
};

/* s5 new add registers bank 0/1 */

/* s5 new add registers bank 0 start */
#define VDIN_TOP_CTRL                                       0x0100 // RW
#define TOP_GCLK_CTRL_BIT		24
#define TOP_GCLK_CTRL_WID		8
#define LOCAL_ARB_GCLK_CTRL_BIT		22
#define LOCAL_ARB_GCLK_CTRL_WID		2

#define TIMING_MEAS_POL_CTRL_BIT		4
#define TIMING_MEAS_POL_CTRL_WID		6
#define TOP_RESET_BIT		0
#define TOP_RESET_WID		4
//Bit 31:24        reg_top_gclk_ctrl                   // unsigned ,    RW, default = 0
//Bit 23:22        reg_local_arb_gclk_ctrl             // unsigned ,    RW, default = 0
//Bit 21:10        reserved
//Bit  9: 4        reg_timing_meas_pol_ctrl            // unsigned ,    RW, default = 0
//Bit  3: 0        reg_reset                           // unsigned ,    RW, default = 0
#define WR_MIF_FIX_DISABLE_BIT	4
#define WR_MIF_FIX_DISABLE_WID	2
#define VDIN_TOP_MISC0                                      0x0101 // RW
//Bit 31: 6        reserved
//Bit  5: 4        reg_vdin0_wrmif_fix_disable         // unsigned ,    RW, default = 0
//Bit  3: 2        reg_vdin0_crc_ctrl                  // unsigned ,    RW, default = 0
//Bit  1: 0        reg_vdin0_slice_mode                // unsigned ,    RW, default = 0
#define VDIN_TOP_MISC1                                      0x0102 // RW
//Bit 31: 6        reserved
//Bit  5: 4        reg_vdin1_wrmif_fix_disable         // unsigned ,    RW, default = 0
//Bit  3: 2        reg_vdin1_crc_ctrl                  // unsigned ,    RW, default = 0
//Bit  1: 0        reg_vdin1_slice_mode                // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE0_COMP0                              0x0103 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_vdin0_secure_comp0              // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE0_COMP1                              0x0104 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_vdin0_secure_comp1              // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE0_COMP2                              0x0105 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_vdin0_secure_comp2              // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE1_COMP0                              0x0106 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_vdin1_secure_comp0              // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE1_COMP1                              0x0107 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_vdin1_secure_comp1              // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE1_COMP2                              0x0108 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_vdin1_secure_comp2              // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE0_CTRL                               0x0109 // RW
//Bit 31: 2        reserved
//Bit  1           reg_vdin0_sec_reg_bk_keep
// unsigned ,    RW, default = 0  secure register, need special access
//Bit  0           reg_vdin0_sec_reg_keep
// unsigned ,    RW, default = 0  secure register, need special access
#define VDIN_TOP_SECURE1_CTRL                               0x010a // RW
//Bit 31: 2        reserved
//Bit  1           reg_vdin1_sec_reg_bk_keep
// unsigned ,    RW, default = 0  secure register, need special access
//Bit  0           reg_vdin1_sec_reg_keep
// unsigned ,    RW, default = 0  secure register, need special access
#define VDIN_TOP_SECURE0_MAX_SIZE                           0x010b // RW
//Bit 31:29        reserved
//Bit 28:16        reg_vdin0_max_allow_pic_h
// unsigned ,    RW, default = 0  secure register, need special access
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin0_max_allow_pic_w
// unsigned ,    RW, default = 0  secure register, need special access
#define VDIN_TOP_SECURE1_MAX_SIZE                           0x010c // RW
//Bit 31:29        reserved
//Bit 28:16        reg_vdin1_max_allow_pic_h           // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin1_max_allow_pic_w           // unsigned ,    RW, default = 0
#define VDIN_TOP_SECURE0_REG                                0x010d // RW
//Bit 31: 0        reg_vdin0_secure_reg
// unsigned ,    RW, default = 0xffffffff  secure register, need special access
#define VDIN_TOP_SECURE1_REG                                0x010e // RW
//Bit 31: 0        reg_vdin1_secure_reg
// unsigned ,    RW, default = 0xffffffff  secure register, need special access
#define VDIN_TOP_MEAS_LINE                                  0x010f // RO
//Bit 31:16        reg_vdi4_vf_lcnt                    // unsigned ,    RO, default = 0
//Bit 15: 0        reg_vdi4_vb_lcnt                    // unsigned ,    RO, default = 0
#define VDIN_TOP_MEAS_PIXF                                  0x0110 // RO
//Bit 31: 0        reg_vdi4_vf_pcnt                    // unsigned ,    RO, default = 0
#define VDIN_TOP_MEAS_PIXB                                  0x0111 // RO
//Bit 31: 0        reg_vdi4_vb_pcnt                    // unsigned ,    RO, default = 0
#define VDIN_TOP_CRC0_OUT                                   0x0112 // RO
//Bit 31: 0        reg_vdin0_crc_out                   // unsigned ,    RO, default = 0
#define VDIN_TOP_CRC1_OUT                                   0x0113 // RO
//Bit 31: 0        reg_vdin1_crc_out                   // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MPEG_CTRL                               0x0120 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_mpeg_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI1_CTRL                               0x0121 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi1_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI2_CTRL                               0x0122 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi2_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI3_CTRL                               0x0123 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi3_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI4_CTRL                               0x0124 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi4_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI5_CTRL                               0x0125 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi5_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI6_CTRL                               0x0126 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi6_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI7_CTRL                               0x0127 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi7_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI8_CTRL                               0x0128 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi8_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDI9_CTRL                               0x0129 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_vdi9_ctrl                        // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_SIZE                                    0x012a // RW
//Bit 31:29        reserved
//Bit 28:16        reg_hsize                            // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vsize                            // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_DECIMATE_CTRL                           0x012b // RW
//Bit 31:10        reserved
//Bit  9: 0        reg_decimate_ctrl                    // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_BUF_CTRL                                0x012c // RW
#define BUF_GCLK_CTRL_BIT	30
#define BUF_GCLK_CTRL_WID	2
#define BUF_CTRL_BIT		0
#define BUF_CTRL_WID		8
//Bit 31:30        reg_buf_gclk_ctrl                    // unsigned ,    RW, default = 0
//Bit 29: 8        reserved
//Bit  7: 0        reg_buf_ctrl                         // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDIN0_SECURE_CTRL                       0x012d // RW
//Bit 31: 6        reserved
//Bit  5           reg_vdin0_secure_out                 // unsigned ,    RW, default = 0
//Bit  4: 1        reg_vdin0_secure_sel                 // unsigned ,    RW, default = 0
//Bit  0           reg_vdin0_secure_en                  // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDIN0_CTRL                              0x0131 // RW
//Bit 31: 1        reserved
//Bit  0           reg_vdin0_enable                     // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDIN0_SYNC_CTRL0                        0x0132 // RW
//Bit 31           reg_vdin0_force_go_field             // unsigned ,    RW, default = 0
//Bit 30           reg_vdin0_force_go_line              // unsigned ,    RW, default = 0
//Bit 29:24        reserved
//Bit 23           reg_vdin0_dly_go_field_en            // unsigned ,    RW, default = 0
//Bit 22:16        reg_vdin0_dly_go_field_lines         // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13           reg_vdin0_clk_cyc_cnt_clr            // unsigned ,    RW, default = 0
//Bit 12           reg_vdin0_clk_cyc_go_line_en         // unsigned ,    RW, default = 0
//Bit 11           reserved
//Bit 10: 4        reg_vdin0_hold_lines                 // unsigned ,    RW, default = 0
//Bit  3           reserved
//Bit  2           reg_vdin0_hsync_mask_en              // unsigned ,    RW, default = 0
//Bit  1           reg_vdin0_vsync_mask_en              // unsigned ,    RW, default = 0
//Bit  0           reg_vdin0_frm_rst_en                 // unsigned ,    RW, default = 1
#define VDIN_IF_TOP_VDIN0_SYNC_CTRL1                        0x0133 // RW
//Bit 31:16        reg_vdin0_clk_cyc_line_widthm1       // unsigned ,    RW, default = 0
//Bit 15: 8        reg_vdin0_hsync_mask_num             // unsigned ,    RW, default = 8'h40
//Bit  7: 0        reg_vdin0_vsync_mask_num             // unsigned ,    RW, default = 8'h40
#define VDIN_IF_TOP_VDIN0_LINE_INT_CTRL                     0x0134 // RW
//Bit 31:14        reserved
//Bit 13           reg_vdin0_line_cnt_clr               // unsigned ,    RW, default = 0
//Bit 12: 0        reg_vdin0_line_cnt_thd               // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDIN1_SECURE_CTRL                       0x0135 // RW
#define IF_VDIN1_EN_BIT                     0
#define IF_VDIN1_EN_WID                     1
#define IF_VDIN1_SEL_BIT                    1
#define IF_VDIN1_SEL_WID                    4
#define IF_VDIN1_SECURE_OUT_BIT             5
#define IF_VDIN1_SECURE_OUT_WID             1

//Bit 31: 6        reserved
//Bit  5           reg_vdin1_secure_out                 // unsigned ,    RW, default = 0
//Bit  4: 1        reg_vdin1_secure_sel                 // unsigned ,    RW, default = 0
//Bit  0           reg_vdin1_secure_en                  // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_VDIN1_CTRL                              0x0139 // RW
//Bit 31: 1        reserved
//Bit  0           reg_vdin1_enable                     // unsigned ,    RW, default = 0

#define VDIN_IF_TOP_OFFSET		0x8
#define DLY_GO_FIELD_EN_BIT		23
#define DLY_GO_FIELD_EN_WID		1
#define DLY_GO_FIELD_LINES_BIT	16
#define DLY_GO_FIELD_LINES_WID	7

#define VDIN_IF_TOP_VDIN1_SYNC_CTRL0                        0x013a // RW
//Bit 31           reg_vdin1_force_go_field             // unsigned ,    RW, default = 0
//Bit 30           reg_vdin1_force_go_line              // unsigned ,    RW, default = 0
//Bit 29:24        reserved
//Bit 23           reg_vdin1_dly_go_field_en            // unsigned ,    RW, default = 0
//Bit 22:16        reg_vdin1_dly_go_field_lines         // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13           reg_vdin1_clk_cyc_cnt_clr            // unsigned ,    RW, default = 0
//Bit 12           reg_vdin1_clk_cyc_go_line_en         // unsigned ,    RW, default = 0
//Bit 11           reserved
//Bit 10: 4        reg_vdin1_hold_lines                 // unsigned ,    RW, default = 0
//Bit  3           reserved
//Bit  2           reg_vdin1_hsync_mask_en              // unsigned ,    RW, default = 0
//Bit  1           reg_vdin1_vsync_mask_en              // unsigned ,    RW, default = 0
//Bit  0           reg_vdin1_frm_rst_en                 // unsigned ,    RW, default = 1
#define VDIN_IF_TOP_VDIN1_SYNC_CTRL1                        0x013b // RW
//Bit 31:16        reg_vdin1_clk_cyc_line_widthm1       // unsigned ,    RW, default = 0
//Bit 15: 8        reg_vdin1_hsync_mask_num             // unsigned ,    RW, default = 8'h40
//Bit  7: 0        reg_vdin1_vsync_mask_num             // unsigned ,    RW, default = 8'h40
#define VDIN_IF_TOP_VDIN1_LINE_INT_CTRL                     0x013c // RW
//Bit 31:14        reserved
//Bit 13           reg_vdin1_line_cnt_clr               // unsigned ,    RW, default = 0
//Bit 12: 0        reg_vdin1_line_cnt_thd               // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_MEAS_CTRL                               0x013d // RW
//Bit 31:20        reserved
//Bit 19           reg_meas_rst                         // unsigned ,    RW, default = 0
//Bit 18           reg_meas_scan                        // unsigned ,    RW, default = 0
//Bit 17           reg_meas_hsvs_wide_en                // unsigned ,    RW, default = 0
//Bit 16           reg_meas_total_count_accum_en        // unsigned ,    RW, default = 0
//Bit 15:12        reg_meas_sel                         // unsigned ,    RW, default = 0
//Bit 11: 4        reg_meas_sync_span                   // unsigned ,    RW, default = 0
//Bit  3: 0        reserved
#define VDIN_IF_TOP_MEAS_FIRST_RANGE                        0x013e // RW
//Bit 31:29        reserved
//Bit 28:16        reg_meas_first_count_end             // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_meas_first_count_start           // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_MEAS_SECOND_RANGE                        0x013f // RW
//Bit 31:29        reserved
//Bit 28:16        reg_meas_second_count_end             // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_meas_second_count_start           // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_MEAS_THIRD_RANGE                        0x0140 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_meas_third_count_end             // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_meas_third_count_start           // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_MEAS_FOURTH_RANGE                        0x0141 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_meas_fourth_count_end             // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_meas_fourth_count_start           // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_PTGEN_CTRL                              0x0142 // RW
//Bit 31:24        reserved
//Bit 23: 0        reg_ptgen_ctrl                       // unsigned ,    RW, default = 0
#define VDIN_IF_TOP_MEAS_IND_TOTAL_COUNT_N                  0x0143 // RO
//Bit 31: 4        reserved
//Bit  3: 0        reg_meas_ind_total_count_n           // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MEAS_IND_TOTAL_COUNT0                   0x0144 // RO
//Bit 31:24        reserved
//Bit 23: 0        reg_meas_ind_total_count0            // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MEAS_IND_TOTAL_COUNT1                   0x0145 // RO
//Bit 31:24        reserved
//Bit 23: 0        reg_meas_ind_total_count1            // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MEAS_IND_FIRST_COUNT                    0x0146 // RO
//Bit 31:24        reserved
//Bit 23: 0        reg_meas_ind_first_count             // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MEAS_IND_SECOND_COUNT                    0x0147 // RO
//Bit 31:24        reserved
//Bit 23: 0        reg_meas_ind_second_count             // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MEAS_IND_THIRD_COUNT                    0x0148 // RO
//Bit 31:24        reserved
//Bit 23: 0        reg_meas_ind_third_count             // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_MEAS_IND_FOURTH_COUNT                    0x0149 // RO
//Bit 31:24        reserved
//Bit 23: 0        reg_meas_ind_fourth_count             // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDI_INT_STATUS0                         0x014a // RO
//Bit 31           reg_vdi4_overflow                    // unsigned ,    RO, default = 0
//Bit 30           reserved
//Bit 29:24        reg_vdi4_asfifo_cnt                  // unsigned ,    RO, default = 0
//Bit 23           reg_vdi3_overflow                    // unsigned ,    RO, default = 0
//Bit 22           reserved
//Bit 21:16        reg_vdi3_asfifo_cnt                  // unsigned ,    RO, default = 0
//Bit 15: 8        reserved
//Bit  7           reg_vdi1_overflow                    // unsigned ,    RO, default = 0
//Bit  6           reserved
//Bit  5: 0        reg_vdi1_asfifo_cnt                  // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDI_INT_STATUS1                         0x014b // RO
//Bit 31           reg_vdi8_overflow                    // unsigned ,    RO, default = 0
//Bit 30           reserved
//Bit 29:24        reg_vdi8_asfifo_cnt                  // unsigned ,    RO, default = 0
//Bit 23           reg_vdi7_overflow                    // unsigned ,    RO, default = 0
//Bit 22           reserved
//Bit 21:16        reg_vdi7_asfifo_cnt                  // unsigned ,    RO, default = 0
//Bit 15           reg_vdi6_overflow                    // unsigned ,    RO, default = 0
//Bit 14           reserved
//Bit 13: 8        reg_vdi6_asfifo_cnt                  // unsigned ,    RO, default = 0
//Bit  7           reg_vdi5_overflow                    // unsigned ,    RO, default = 0
//Bit  6           reserved
//Bit  5: 0        reg_vdi5_asfifo_cnt                  // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDI_INT_STATUS2                         0x014c // RO
//Bit 31: 8        reserved
//Bit  7           reg_vdi9_overflow                    // unsigned ,    RO, default = 0
//Bit  6           reserved
//Bit  5: 0        reg_vdi9_asfifo_cnt                  // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN0_STATUS                            0x014d // RO
//Bit 31: 3        reserved
//Bit  2           reg_vdin0_secure_cur_pic             // unsigned ,    RO, default = 0
//Bit  1           reg_vdin0_secure_cur_pic_sav         // unsigned ,    RO, default = 0
//Bit  0           reg_vdin0_field_in                   // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN1_STATUS                            0x014e // RO
//Bit 31: 3        reserved
//Bit  2           reg_vdin1_secure_cur_pic             // unsigned ,    RO, default = 0
//Bit  1           reg_vdin1_secure_cur_pic_sav         // unsigned ,    RO, default = 0
//Bit  0           reg_vdin1_field_in                   // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN0_LINE_CNT_STATUS                   0x014f // RO
//Bit 31:29        reserved
//Bit 28:16        reg_vdin0_go_line_cnt                // unsigned ,    RO, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin0_active_line_cnt            // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN0_LINE_CNT_SHADOW_STATUS            0x0150 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_vdin0_go_line_cnt_shadow         // unsigned ,    RO, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin0_active_line_cnt_shadow     // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN0_ACTIVE_MAX_PIX_CNT_STATUS         0x0151 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_vdin0_active_max_pix_cnt         // unsigned ,    RO, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin0_active_max_pix_cnt_shadow  // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN1_LINE_CNT_STATUS                   0x0152 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_vdin1_go_line_cnt                // unsigned ,    RO, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin1_active_line_cnt            // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN1_LINE_CNT_SHADOW_STATUS            0x0153 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_vdin1_go_line_cnt_shadow         // unsigned ,    RO, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin1_active_line_cnt_shadow     // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN1_ACTIVE_MAX_PIX_CNT_STATUS         0x0154 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_vdin1_active_max_pix_cnt         // unsigned ,    RO, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_vdin1_active_max_pix_cnt_shadow  // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN0_SECURE_ILLEGAL                    0x0155 // RO
//Bit 31: 0        reg_vdin0_secure_illegal             // unsigned ,    RO, default = 0
#define VDIN_IF_TOP_VDIN1_SECURE_ILLEGAL                    0x0156 // RO
//Bit 31: 0        reg_vdin1_secure_illegal             // unsigned ,    RO, default = 0
#define VDIN_LOCAL_ARB_CTRL                                      0x0160 // RW
//Bit 31:30   reg_local_arb_gclk_ctrl             // unsigned ,    RW, default = 0
//Bit 29: 0   reserved
#define VDIN_LOCAL_ARB_WR_MODE                                   0x0161 // RW
//Bit 31:30   reg_local_arb_wr_gate_clk_ctrl      // unsigned ,    RW, default = 0
//Bit 29:24   reserved
//Bit 23:16   reg_local_arb_wr_sel                // unsigned ,    RW, default = 0
//Bit 15: 9   reserved
//Bit  8      reg_local_arb_wr_mode               // unsigned ,    RW, default = 0
//Bit  7: 0   reserved
#define VDIN_LOCAL_ARB_WR_REQEN_SLV                              0x0162 // RW
//Bit 31: 8   reserved
//Bit  7: 0   reg_local_arb_wr_dc_req_en          // unsigned ,    RW, default = 8'hff
#define VDIN_LOCAL_ARB_WR_WEIGH0_SLV                             0x0163 // RW
//Bit 31:30   reserved
//Bit 29: 0   reg_local_arb_wr_dc_weigh_slv0      // unsigned ,    RW, default = 30'hf3cf3cf
#define VDIN_LOCAL_ARB_WR_WEIGH1_SLV                             0x0164 // RW
//Bit 31:18   reserved
//Bit 17: 0   reg_local_arb_wr_dc_weigh_slv1      // unsigned ,    RW, default = 18'hf3cf
#define VDIN_LOCAL_ARB_DBG_CTRL                                  0x0165 // RW
//Bit 31: 0   reg_local_arb_det_cmd_ctrl          // unsigned ,    RW, default = 8
#define VDIN_LOCAL_ARB_RDWR_STATUS                               0x0166 // RO
//Bit 31: 3   reserved
//Bit  2      reg_local_arb_wr_busy               // unsigned ,    RO, default = 0
//Bit  1: 0   reserved
#define VDIN_LOCAL_ARB_DBG_STATUS                                0x0167 // RO
//Bit 31: 0   reg_local_arb_det_dbg_stat          // unsigned , RO, default = 0xaffc
/* s5 new add registers bank 0 end */

/* s5 new add registers bank 1 start */
#define VDIN_DW_TOP_GCLK_CTRL                               0x0200 // RW
#define DW_TOP_GCLK_CTRL_BIT	8
#define DW_TOP_GCLK_CTRL_WID	4
//Bit 31:12           reserved
//Bit 11: 8           reg_top_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit  7: 6           reg_dsc_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit  5: 4           reg_scb_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit  3: 2           reg_hsk_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit  1: 0           reg_vsk_gclk_ctrl       // unsigned ,    RW, default = 0
#define VDIN_DW_TOP_CTRL                                    0x0201 // RW
#define DW_HSK_EN_BIT	1
#define DW_HSK_EN_WID	1
#define DW_VSK_EN_BIT	0
#define DW_VSK_EN_WID	1
//Bit 31: 4           reserved
//Bit 3               reg_dsc_en              // unsigned ,    RW, default = 0
//Bit 2               reg_scb_en              // unsigned ,    RW, default = 0
//Bit 1               reg_hsk_en              // unsigned ,    RW, default = 0
//Bit 0               reg_vsk_en              // unsigned ,    RW, default = 0
#define VDIN_DW_TOP_IN_SIZE                                 0x0202 // RW
//Bit 31:29           reserved
//Bit 28:16           reg_in_hsize            // unsigned ,    RW, default = 0
//Bit 15:13           reserved
//Bit 12:0            reg_in_vsize            // unsigned ,    RW, default = 0
#define VDIN_DW_TOP_OUT_SIZE                                0x0203 // RW
//Bit 31:29           reserved
//Bit 28:16           reg_out_hsize           // unsigned ,    RW, default = 0
//Bit 15:13           reserved
//Bit 12:0            reg_out_vsize           // unsigned ,    RW, default = 0
#define VDIN_DW_DSC_CTRL                                    0x0204 // RW
//Bit 31:30     reg_dsc_gclk_ctrl             // unsigned ,    RW, default = 0
//Bit 29: 9     reserved
//Bit  8: 3     reg_dsc_dit_out_switch       // unsigned ,    RW, default = 36
//Bit  2        reg_dsc_detunnel_en           // unsigned ,    RW, default = 1
//Bit  1        reg_dsc_detunnel_u_start      // unsigned ,    RW, default = 0
//Bit  0        reg_dsc_comp_mode             // unsigned ,    RW, default = 1
#define VDIN_DSC_CFMT_CTRL                                  0x0205 // RW
//Bit 31: 9     reserved
//Bit  8        reg_dsc_chfmt_rpt_pix
// unsigned ,    RW, default = 0    if true, horizontal formatter use repeating to generate pixel,
//otherwise use bilinear interpolation
//Bit  7: 4     reg_dsc_chfmt_ini_phase
// unsigned ,    RW, default = 0    horizontal formatter initial phase
//Bit  3        reg_dsc_chfmt_rpt_p0_en
// unsigned ,    RW, default = 0    horizontal formatter repeat pixel 0 enable
//Bit  2: 1     reg_dsc_chfmt_yc_ratio
// unsigned ,    RW, default = 1    horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit  0        reg_dsc_chfmt_en
// unsigned ,    RW, default = 1    horizontal formatter enable
#define VDIN_DSC_CFMT_W                                     0x0206 // RW
//Bit 31:29     reserved
//Bit 28:16     reg_dsc_chfmt_w
// unsigned ,    RW, default = 1920 horizontal formatter width
//Bit 15:13     reserved
//Bit 12: 0     reg_dsc_cvfmt_w
// unsigned ,    RW, default = 960  vertical formatter width
#define VDIN_DW_DSC_HSIZE                                      0x0207 // RW
//Bit 31:29     reserved
//Bit 28:16     reg_dsc_detunnel_hsize        // unsigned ,    RW, default = 1920
//Bit 15:13     reserved
//Bit 12: 0     reg_dsc_dither_hsize          // unsigned ,    RW, default = 1920
#define VDIN_DW_DSC_DETUNNEL_SEL                               0x0208 // RW
//Bit 31:18     reserved
//Bit 17:0      reg_dsc_detunnel_sel          // unsigned ,    RW, default = 34658
#define VDIN_DW_HSK_CTRL                                    0x020c // RW
#define DW_HSK_MODE_BIT		0
#define DW_HSK_MODE_WID		7
//Bit 31:30    reg_hsk_gclk_ctrl    // unsigned,    RW, default = 0
//Bit 29: 7    reserved
//Bit  6: 0    reg_hsk_mode         // unsigned,    RW, default = 4
#define VDIN_HSK_COEF_0                                     0x020d // RW
//Bit 31: 0    reg_hsk_coef00       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_1                                     0x020e // RW
//Bit 31: 0    reg_hsk_coef01       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_2                                     0x020f // RW
//Bit 31: 0    reg_hsk_coef02       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_3                                     0x0210 // RW
//Bit 31: 0    reg_hsk_coef03       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_4                                     0x0211 // RW
//Bit 31: 0    reg_hsk_coef04       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_5                                     0x0212 // RW
//Bit 31: 0    reg_hsk_coef05       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_6                                     0x0213 // RW
//Bit 31: 0    reg_hsk_coef06       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_7                                     0x0214 // RW
//Bit 31: 0    reg_hsk_coef07       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_8                                     0x0215 // RW
//Bit 31: 0    reg_hsk_coef08       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_9                                     0x0216 // RW
//Bit 31: 0    reg_hsk_coef09       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_10                                    0x0217 // RW
//Bit 31: 0    reg_hsk_coef10       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_11                                    0x0218 // RW
//Bit 31: 0    reg_hsk_coef11       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_12                                    0x0219 // RW
//Bit 31: 0    reg_hsk_coef12       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_13                                    0x021a // RW
//Bit 31: 0    reg_hsk_coef13       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_14                                    0x021b // RW
//Bit 31: 0    reg_hsk_coef14       // unsigned,    RW, default = 32'h10101010
#define VDIN_HSK_COEF_15                                    0x021c // RW
//Bit 31: 0    reg_hsk_coef15       // unsigned,    RW, default = 32'h10101010
#define VDIN_VSK_CTRL                                       0x0224 // RW
#define DW_VSK_MODE_BIT		2
#define DW_VSK_MODE_WID		2
#define DW_VSK_LPF_MODE_BIT		1
#define DW_VSK_LPF_MODE_WID		1
//Bit 31:28           reg_vsk_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit 27: 4           reserved
//Bit  3: 2           reg_vsk_mode            // unsigned ,    RW, default = 0
//Bit  1              reg_vsk_lpf_mode        // unsigned ,    RW, default = 0
//Bit  0              reg_vsk_fix_en          // unsigned ,    RW, default = 0
#define VDIN_VSK_PTN_DATA0                                  0x0225 // RW
//Bit 31:24           reserved
//Bit 23: 0           reg_vsk_ptn_data0       // unsigned ,    RW, default = 0x8080
#define VDIN_VSK_PTN_DATA1                                  0x0226 // RW
//Bit 31:24           reserved
//Bit 23: 0           reg_vsk_ptn_data1       // unsigned ,    RW, default = 0x8080
#define VDIN_VSK_PTN_DATA2                                  0x0227 // RW
//Bit 31:24           reserved
//Bit 23: 0           reg_vsk_ptn_data2       // unsigned ,    RW, default = 0x8080
#define VDIN_VSK_PTN_DATA3                                  0x0228 // RW
//Bit 31:24           reserved
//Bit 23: 0           reg_vsk_ptn_data3       // unsigned ,    RW, default = 0x8080
#define VDIN_SCB_CTRL                                       0x022e // RW
//Bit 31:30     reg_scb_gclk_ctrl           // unsigned , RW, default = 0
//Bit 29: 5     reserved
//Bit  4        reg_scb_444c422_gofield_en  // unsigned , RW, default = 1
//Bit  3: 2     reg_scb_444c422_mode        // unsigned , RW, default = 0 0:left 1:right 2,3:avg
//Bit  1        reg_scb_444c422_bypass      // unsigned , RW, default = 0 1:bypass
//Bit  0        reg_scb_444c422_frmen       // unsigned , RW, default = 0
#define VDIN_SCB_SIZE                                       0x022f // RW
//Bit 31:29     reserved
//Bit 28:16     reg_scb_444c422_hsize       // unsigned , RW, default = 1920  horizontal size
//Bit 15:13     reserved
//Bit 12: 0     reg_scb_444c422_vsize       // unsigned , RW, default = 960   vertical size
#define VDIN_SCB_TUNNEL                                     0x0230 // RW
//Bit 31:25     reserved
//Bit 24:19     reg_tunnel_outswitch        // unsigned , RW, default = 36
//Bit 18: 1     reg_tunnel_sel              // unsigned , RW, default = 10x20110ec
//Bit  0        reg_tunnel_en               // unsigned , RW, default = 1
#define VDIN_WRMIF_CTRL                                     0x0236 // RW
//Bit 31      reg_swap_word
// unsigned ,    RW, default = 1   applicable only to reg_wr_format=0/2,
//0: Output every even pixels' CbCr;1: Output every odd pixels' CbCr;
//2: Output an average value per even&odd pair of pixels;
//3: Output all CbCr. (This does NOT apply to bit[13:12]=0 -- 4:2:2 mode.)  // RW
//Bit 30      reg_swap_cbcr
// unsigned ,    RW, default = 0   applicable only to reg_wr_format=2,
//0: Output CbCr (NV12); 1: Output CrCb (NV21).
//Bit 29:28   reg_vconv_mode
// unsigned ,    RW, default = 0   applicable only to reg_wr_format=2,
//0: Output every even lines' CbCr;1: Output every odd lines' CbCr;2: Reserved;3: Output all CbCr.
//Bit 27:26   reg_hconv_mode                // unsigned ,    RW, default = 0
//Bit 25      reg_no_clk_gate
// unsigned ,    RW, default = 0   disable vid_wr_mif clock gating function.
//Bit 24      reg_clr_wrrsp                 // unsigned ,    RW, default = 0
//Bit 23      reg_eol_sel
// unsigned ,    RW, default = 1   eol_sel, 1: use eol as the line end indication,
//0: use width as line end indication in the vdin write memory interface
//Bit 22:20   reserved
//Bit 19      reg_frame_rst_en
// unsigned ,    RW, default = 1   frame reset enable,
//if true, it will provide frame reset during go_field(vsync) to the modules after that
//Bit 18      reg_field_done_clr_bit
// unsigned ,    RW, default = 0   write done status clear bit
//Bit 17      reg_pending_ddr_wrrsp_clr_bit
// unsigned ,    RW, default = 0   clear write response counter in the vdin write memory interface
//Bit 16:15   reserved
//Bit 14      reg_dbg_sample_mode           // unsigned ,    RW, default = 0
//Bit 13:12   reg_wr_format
// unsigned ,    RW, default = 0   write format, 0: 4:2:2 to luma canvas,
//1: 4:4:4 to luma canvas, 2: Y to luma canvas, CbCr to chroma canvas.
//For NV12/21, also #define Bit 31:30, 17:16, and bit 18.
//Bit 11      reg_wr_canvas_dbuf_en
// unsigned ,    RW, default = 0   write canvas double buffer enable,
//means the canvas address will be latched by vsync before using
//Bit 10      reg_dis_ctrl_reg_w_pulse
// unsigned ,    RW, default = 0   disable ctrl_reg write pulse
//which will reset internal counter. when bit 11 is 1, this bit should be 1.
//Bit  9      reg_wr_req_urgent             // unsigned ,    RW, default = 0   write request urgent
//Bit  8      reg_wr_req_en                 // unsigned ,    RW, default = 0   write request enable
//Bit  7: 0   reg_wr_canvas_direct_luma
// unsigned ,    RW, default = 0   Write luma canvas address
#define VDIN_WRMIF_CTRL2                                    0x0237 // RW
//Bit 31      reg_debug_mode                // unsigned ,    RW, default = 0
//Bit 30:26   reserved
//Bit 25      reg_wr_little_endian          // unsigned ,    RW, default = 0
//Bit 24:22   reg_wr_field_mode
// unsigned ,    RW, default = 0    0 frame mode, 4 for field mode botton field,
//5 for field mode top field, , 6 for blank line mode
//Bit 21      reg_wr_h_rev                  // unsigned ,    RW, default = 0
//Bit 20      reg_wr_v_rev                  // unsigned ,    RW, default = 0
//Bit 19      reg_wr_bit10_mode             // unsigned ,    RW, default = 0
//Bit 18      reg_wr_data_ext_en            // unsigned ,    RW, default = 0
//Bit 17:14   reg_wr_words_lim              // unsigned ,    RW, default = 1
//Bit 13:10   reg_wr_burst_lim              // unsigned ,    RW, default = 2
//Bit  9: 8   reserved
//Bit  7: 0   reg_wr_canvas_direct_chroma
// unsigned ,    RW, default = 0    Write chroma canvas address
#define VDIN_WRMIF_H_START_END                              0x0238 // RW
//Bit 31:29   reserved
//Bit 28:16   reg_wr_h_start                // unsigned ,    RW, default = 0
//Bit 15:13   reserved
//Bit 12: 0   reg_wr_h_end                  // unsigned ,    RW, default = 13'h1fff
#define VDIN_WRMIF_V_START_END                              0x0239 // RW
//Bit 31:29   reserved
//Bit 28:16   reg_wr_v_start                // unsigned ,    RW, default = 0
//Bit 15:13   reserved
//Bit 12: 0   reg_wr_v_end                  // unsigned ,    RW, default = 13'h1fff
#define VDIN_WRMIF_URGENT_CTRL                              0x023a // RW
//Bit 31:16   reserved
//Bit 15: 0   reg_wr_req_urgent_ctrl        // unsigned ,    RW, default = 0
#define VDIN_WRMIF_BADDR_LUMA                               0x023b // RW
//Bit 31: 0   reg_canvas_baddr_luma         // unsigned ,    RW, default = 0
#define VDIN_WRMIF_BADDR_CHROMA                             0x023c // RW
//Bit 31: 0   reg_canvas_baddr_chroma       // unsigned ,    RW, default = 0
#define VDIN_WRMIF_STRIDE_LUMA                              0x023d // RW
//Bit 31: 0   reg_canvas_stride_luma        // unsigned ,    RW, default = 0
#define VDIN_WRMIF_STRIDE_CHROMA                            0x023e // RW
//Bit 31: 0   reg_canvas_stride_chroma      // unsigned ,    RW, default = 0
#define VDIN_WRMIF_DSC_CTRL                                 0x023f // RW
//Bit 31:19   reserved
//Bit 18: 0   reg_descramble_ctrl           // unsigned ,    RW, default = 0
#define VDIN_WRMIF_DBG_AXI_CMD_CNT                          0x0240 // RO
//Bit 31: 0   reg_dbg_axi_cmd_cnt           // unsigned ,    RO, default = 0
#define VDIN_WRMIF_DBG_AXI_DAT_CNT                          0x0241 // RO
//Bit 31: 0   reg_dbg_axi_dat_cnt           // unsigned ,    RO, default = 0
#define VDIN_WRMIF_RO_STATUS                                0x0242 // RO
//Bit 31: 2   reserved
//Bit  1      reg_pending_ddr_wrrsp         // unsigned ,    RO, default = 0
//Bit  0      reg_field_done                // unsigned ,    RO, default = 0
#define VDIN_PP_TOP_GCLK_CTRL                               0x0245 // RW
#define PP_TOP_GCLK_CTRL_BIT	28
#define PP_TOP_GCLK_CTRL_WID	4
#define PP_HSC_GCLK_CTRL_BIT	10
#define PP_HSC_GCLK_CTRL_WID	2
#define PP_VSC_GCLK_CTRL_BIT	8
#define PP_VSC_GCLK_CTRL_WID	2
//Bit 31:28           reg_top_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit 27:24           reserved
//Bit 23:22           reg_mat0_gclk_ctrl      // unsigned ,    RW, default = 0
//Bit 21:16           reg_cm_gclk_ctrl        // unsigned ,    RW, default = 0
//Bit 15:14           reg_bri_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit 13:12           reg_phsc_gclk_ctrl      // unsigned ,    RW, default = 0
//Bit 11:10           reg_hsc_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit  9: 8           reg_vsc_gclk_ctrl       // unsigned ,    RW, default = 0
//Bit  7: 6           reg_hdr2_mat1_gclk_ctrl // unsigned ,    RW, default = 0
//Bit  5: 4           reg_lfifo_gclk_ctrl     // unsigned ,    RW, default = 0
//Bit  3: 2           reg_hist_gclk_ctrl      // unsigned ,    RW, default = 0
//Bit  1: 0           reg_blkbar_gclk_ctrl    // unsigned ,    RW, default = 0
#define VDIN_PP_TOP_CTRL                                    0x0246 // RW
/* 00: component0_in 01: component1_in 10: component2_in */
#define PP_COMP2_SWAP_BIT               30
#define PP_COMP2_SWAP_WID               2
/* 00: component0_in 01: component1_in 10: component2_in */
#define PP_COMP1_SWAP_BIT               28
#define PP_COMP1_SWAP_WID               2
/* 00: component0_in 01: component1_in 10: component2_in */
#define PP_COMP0_SWAP_BIT               26
#define PP_COMP0_SWAP_WID               2

#define PP_WIN_EN_BIT               22
#define PP_WIN_EN_WID            1
#define PP_CM_EN_BIT                6
#define PP_CM_EN_WID             1
#define PP_BRI_EN_BIT               5
#define PP_BRI_EN_WID            1
#define PP_PHSC_EN_BIT              4
#define PP_PHSC_EN_WID           1
#define PP_HSC_EN_BIT               3
#define PP_HSC_EN_WID            1
#define PP_VSC_EN_BIT               2
#define PP_VSC_EN_WID            1
#define PP_MAT0_EN_BIT              1
#define PP_MAT0_EN_WID           1
#define PP_MAT1_EN_BIT              0
#define PP_MAT1_EN_WID           1
//Bit 31:30           reg_comp2_swap          // unsigned ,    RW, default = 2
//Bit 29:28           reg_comp1_swap          // unsigned ,    RW, default = 1
//Bit 27:26           reg_comp0_swap          // unsigned ,    RW, default = 0
//Bit 25:23           reserved
//Bit 22              reg_win_en              // unsigned ,    RW, default = 0
//Bit 21              reg_discard_data_en     // unsigned ,    RW, default = 0
//Bit 20:16           reserved
//Bit 15:14           reg_blkbar_det_sel      // unsigned ,    RW, default = 0
//Bit 13:12           reg_hist_spl_sel        // unsigned ,    RW, default = 0
//Bit 11: 9           reserved
//Bit  8              reg_blkbar_det_en       // unsigned ,    RW, default = 0
//Bit  7              reg_hist_spl_en         // unsigned ,    RW, default = 0
//Bit  6              reg_cm_en               // unsigned ,    RW, default = 0
//Bit  5              reg_bri_en              // unsigned ,    RW, default = 0
//Bit  4              reg_phsc_en             // unsigned ,    RW, default = 0
//Bit  3              reg_hsc_en              // unsigned ,    RW, default = 0
//Bit  2              reg_vsc_en              // unsigned ,    RW, default = 0
//Bit  1              reg_mat0_conv_en        // unsigned ,    RW, default = 0
//Bit  0              reg_mat1_conv_en        // unsigned ,    RW, default = 0
#define VDIN_PP_TOP_IN_SIZE                                 0x0247 // RW
//Bit 31:29           reserved
//Bit 28:16           reg_in_hsize            // unsigned ,    RW, default = 0
//Bit 15:13           reserved
//Bit 12:0            reg_in_vsize            // unsigned ,    RW, default = 0
#define VDIN_PP_TOP_OUT_SIZE                                0x0248 // RW
//Bit 31:29           reserved
//Bit 28:16           reg_out_hsize           // unsigned ,    RW, default = 0
//Bit 15:13           reserved
//Bit 12:0            reg_out_vsize           // unsigned ,    RW, default = 0
#define VDIN_PP_TOP_H_WIN                                   0x0249 // RW
//Bit 31:29           reserved
//Bit 28:16           reg_win_h_start         // unsigned ,    RW, default = 0
//Bit 15:13           reserved
//Bit 12:0            reg_win_h_end           // unsigned ,    RW, default = 13'h1fff
#define VDIN_PP_TOP_V_WIN                                   0x024a // RW
//Bit 31:29           reserved
//Bit 28:16           reg_win_v_start         // unsigned ,    RW, default = 0
//Bit 15:13           reserved
//Bit 12:0            reg_win_v_end           // unsigned ,    RW, default = 13'h1fff
#define VDIN_MAT0_CTRL                                      0x024d // RW
//Bit 31:30        reg_mat_gclk_ctrl      // unsigned ,    RW, default = 0
//Bit 29:27        reserved
//Bit 26:24        reg_mat_conv_rs        // unsigned ,    RW, default = 0
//Bit 23: 9        reserved
//Bit  8           reg_mat_hl_en          // unsigned ,    RW, default = 0
//Bit  7: 2        reserved
//Bit  1           reg_mat_probe_sel      // unsigned ,    RW, default = 1
//Bit  0           reg_mat_probe_post     // unsigned ,    RW, default = 0
#define VDIN_MAT0_HL_COLOR                                  0x024e // RW
//Bit 31:24        reserved
//Bit 23: 0        reg_mat_hl_color       // unsigned ,    RW, default = 0
#define VDIN_MAT0_PROBE_POS                                 0x024f // RW
//Bit 31:16        reg_mat_pos_x          // unsigned ,    RW, default = 0
//Bit 15: 0        reg_mat_pos_y          // unsigned ,    RW, default = 0
#define VDIN_MAT0_COEF00_01                                 0x0250 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef00         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef01         // unsigned ,    RW, default = 0
#define VDIN_MAT0_COEF02_10                                 0x0251 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef02         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef10         // unsigned ,    RW, default = 0
#define VDIN_MAT0_COEF11_12                                 0x0252 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef11         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef12         // unsigned ,    RW, default = 0
#define VDIN_MAT0_COEF20_21                                 0x0253 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef20         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef21         // unsigned ,    RW, default = 0
#define VDIN_MAT0_COEF22                                    0x0254 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_mat_coef22         // unsigned ,    RW, default = 0
#define VDIN_MAT0_OFFSET0_1                                 0x0255 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_offset0        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_offset1        // unsigned ,    RW, default = 0
#define VDIN_MAT0_OFFSET2                                   0x0256 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_mat_offset2        // unsigned ,    RW, default = 0
#define VDIN_MAT0_PRE_OFFSET0_1                             0x0257 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_pre_offset0    // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_pre_offset1    // unsigned ,    RW, default = 0
#define VDIN_MAT0_PRE_OFFSET2                               0x0258 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_mat_pre_offset2    // unsigned ,    RW, default = 0
#define VDIN_MAT0_LINE_LEN_M1                                0x0259 // RW
//Bit 31:16        reserved
//Bit 15: 0        reg_mat_line_len_m1     // unsigned ,    RW, default = 0
#define VDIN_MAT0_PROBE_COLOR                               0x025a // RO
//Bit 31:0         reg_mat_probe_color    // unsigned ,    RO, default = 0
#define VDIN_MAT1_CTRL                                      0x025d // RW
//Bit 31:30        reg_mat_gclk_ctrl      // unsigned ,    RW, default = 0
//Bit 29:27        reserved
//Bit 26:24        reg_mat_conv_rs        // unsigned ,    RW, default = 0
//Bit 23: 9        reserved
//Bit  8           reg_mat_hl_en          // unsigned ,    RW, default = 0
//Bit  7: 2        reserved
//Bit  1           reg_mat_probe_sel      // unsigned ,    RW, default = 1
//Bit  0           reg_mat_probe_post     // unsigned ,    RW, default = 0
#define VDIN_MAT1_HL_COLOR                                  0x025e // RW
//Bit 31:24        reserved
//Bit 23: 0        reg_mat_hl_color       // unsigned ,    RW, default = 0
#define VDIN_MAT1_PROBE_POS                                 0x025f // RW
//Bit 31:16        reg_mat_pos_x          // unsigned ,    RW, default = 0
//Bit 15: 0        reg_mat_pos_y          // unsigned ,    RW, default = 0
#define VDIN_MAT1_COEF00_01                                 0x0260 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef00         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef01         // unsigned ,    RW, default = 0
#define VDIN_MAT1_COEF02_10                                 0x0261 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef02         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef10         // unsigned ,    RW, default = 0
#define VDIN_MAT1_COEF11_12                                 0x0262 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef11         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef12         // unsigned ,    RW, default = 0
#define VDIN_MAT1_COEF20_21                                 0x0263 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_coef20         // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_coef21         // unsigned ,    RW, default = 0
#define VDIN_MAT1_COEF22                                    0x0264 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_mat_coef22         // unsigned ,    RW, default = 0
#define VDIN_MAT1_OFFSET0_1                                 0x0265 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_offset0        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_offset1        // unsigned ,    RW, default = 0
#define VDIN_MAT1_OFFSET2                                   0x0266 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_mat_offset2        // unsigned ,    RW, default = 0
#define VDIN_MAT1_PRE_OFFSET0_1                             0x0267 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_mat_pre_offset0    // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mat_pre_offset1    // unsigned ,    RW, default = 0
#define VDIN_MAT1_PRE_OFFSET2                               0x0268 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_mat_pre_offset2    // unsigned ,    RW, default = 0
#define VDIN_MAT1_LINE_LEN_M1                                0x0269 // RW
//Bit 31:16        reserved
//Bit 15: 0        reg_mat_line_len_m1     // unsigned ,    RW, default = 0
#define VDIN_MAT1_PROBE_COLOR                               0x026a // RO
//Bit 31:0         reg_mat_probe_color    // unsigned ,    RO, default = 0
#define VDIN_PP_CHROMA_ADDR_PORT                               0x026d // RW
//Bit 31:10   reserved
//Bit  9: 0   reg_chroma_addr_port    // unsigned ,    RW, default = 0
#define VDIN_PP_CHROMA_DATA_PORT                               0x026e // RW
//Bit 31: 0   reg_chroma_data_port    // unsigned ,    RW, default = 0
#define VDIN_BRI_CTRL                                       0x0271 // RW
//Bit 31:30        reg_bri_gclk_ctrl      // unsigned ,    RW, default = 0
//Bit 29           reserved
//Bit 28:26        reg_bri_sde_yuv_inv_en   // unsigned ,    RW, default = 0
//Bit 25:23        reserved
//Bit 22:12        reg_bri_adj_bri        // unsigned ,    RW, default = 0
//Bit 11: 0        reg_bri_adj_con        // unsigned ,    RW, default = 0
#define VDIN_PHSC_CTRL                                      0x0275 // RW
#define PP_PHSC_MODE_BIT	0
#define PP_PHSC_MODE_WID	4
//Bit 31:30        reg_phsc_gclk_ctrl     // unsigned ,    RW, default = 0
//Bit 29: 4        reserved
//Bit  3: 0        reg_phsc_mode          // unsigned ,    RW, default = 0
#define VDIN_HSC_COEF_IDX                                   0x0279 // RW
//Bit 31:16   reserved
//Bit 15      idx_inc             // unsigned ,    RW, default = 0
//Bit 14      rd_cbus_coef_en     // unsigned ,    RW, default = 0
//Bit 13:10   reserved
//Bit  9      high_res_out_en        // unsigned ,    RW, default = 0
//Bit  8      ctype               // unsigned ,    RW, default = 0
//Bit  7      reserved
//Bit  6: 0   index               // unsigned ,    RW, default = 0
#define VDIN_HSC_COEF                                       0x027a // RW
//Bit 31: 0   coef                // unsigned ,    RW, default = 0
#define VDIN_HSC_CTRL                                       0x027d // RW
#define PP_HSC_GCLK_CTL_BIT				30
#define PP_HSC_GCLK_CTL_WID				2
#define PP_HSC_NEAREST_EN_BIT			25
#define PP_HSC_NEAREST_EN_WID			1
#define PP_HSC_SHORT_LINE_O_EN_BIT		24
#define PP_HSC_SHORT_LINE_O_EN_WID		1
#define PP_HSC_SP422_MODE_BIT			16
#define PP_HSC_SP422_MODE_WID			2

#define PP_HSC_PHASE0_ALWASY_EN_BIT		8
#define PP_HSC_PHASE0_ALWASY_EN_WID		1
#define PP_HSC_BANK_LEN_BIT				0
#define PP_HSC_BANK_LEN_WID				3

//Bit 31:30        reg_hsc_gclk_ctrl        // unsigned ,    RW, default = 0
//Bit 29:26        reserved
//Bit 25           reg_hsc_nearest_en       // unsigned ,    RW, default = 0
//Bit 24           reg_hsc_short_line_o_en   // unsigned ,    RW, default = 0
//Bit 23:18        reserved
//Bit 17:16        reg_hsc_sp422_mode       // unsigned ,    RW, default = 0
//Bit 15: 9        reserved
//Bit  8           reg_hsc_phase0_always_en // unsigned ,    RW, default = 0
//Bit  7: 3        reserved
//Bit  2: 0        reg_hsc_bank_length      // unsigned ,    RW, default = 0
#define VDIN_PP_HSC_PHASE_STEP                                 0x027e // RW
//Bit 31:29        reserved
//Bit 28: 0        reg_hsc_phase_step       // unsigned ,    RW, default = 0
#define VDIN_HSC_MISC                                       0x027f // RW
#define HSC_RPT_P0_NUM_BIT	0
#define HSC_RPT_P0_NUM_WID	2
#define HSC_INI_RCV_NUM_BIT	8
#define HSC_INI_RCV_NUM_WID	5
#define HSC_INI_PIXI_PTR_BIT	16
#define HSC_INI_PIXI_PTR_WID	7
//Bit 31:23        reserved
//Bit 22:16        reg_hsc_ini_pixi_ptr     // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 8        reg_hsc_ini_rcv_num      // unsigned ,    RW, default = 0
//Bit  7: 2        reserved
//Bit  1: 0        reg_hsc_rpt_p0_num       // unsigned ,    RW, default = 0
#define VDIN_HSC_INI_PHASE                                  0x0280 // RW
//Bit 31:24        reserved
//Bit 23: 0        reg_hsc_ini_phase        // unsigned ,    RW, default = 0
#define VDIN_VSC_CTRL                                       0x0285 // RW
#define PP_VSC_SKIP_LINE_NUM_BIT		16
#define PP_VSC_SKIP_LINE_NUM_WID	5
#define PP_VSC_PHASE0_ALWAS_EN_BIT		8
#define PP_VSC_PHASE0_ALWAS_EN_WID	1
//Bit 31:28        reg_vsc_gclk_ctrl            // unsigned ,    RW, default = 0
//Bit 27:21        reserved
//Bit 20:16        reg_vsc_skip_line_num        // unsigned ,    RW, default = 0
//Bit 15: 9        reserved
//Bit  8           reg_vsc_phase0_always_en     // unsigned ,    RW, default = 0
//Bit  7: 2        reserved
//Bit  1           reg_vsc_outside_pic_pad_en   // unsigned ,    RW, default = 0
//Bit  0           reg_vsc_rpt_last_ln_mode     // unsigned ,    RW, default = 0
#define VDIN_PP_VSC_PHASE_STEP                                 0x0286 // RW
//Bit 31:25        reserved
//Bit 24: 0        reg_vsc_phase_step           // unsigned ,    RW, default = 0
#define VDIN_VSC_DUMMY_DATA                                 0x0287 // RW
//Bit 31:24        reserved
//Bit 23: 0        reg_vsc_dummy_data           // unsigned ,    RW, default = 24'h8080
#define VDIN_VSC_INI_PHASE                                  0x0288 // RW
//Bit 31:16        reserved
//Bit 15: 0        reg_vsc_ini_phase            // unsigned ,    RW, default = 0
#define VDIN_PP_LFIFO_CTRL                                     0x028d // RW
#define PP_LFIFO_GCLK_CTRL_BIT	30
#define PP_LFIFO_SOFT_RST_BIT	29
#define PP_LFIFO_BUF_SIZE_BIT	16
#define PP_LFIFO_URG_CTRL_BIT	0
//Bit 31:30        reg_lfifo_gclk_ctrl    // unsigned ,    RW, default = 0
//Bit 29           reg_lfifo_soft_rst_en  // unsigned ,    RW, default = 1
//Bit 28:16        reg_lfifo_buf_size     // unsigned ,    RW, default = 4096
//Bit 15: 0        reg_lfifo_urg_ctrl     // unsigned ,    RW, default = 0
#define VDIN_LFIFO_BUF_COUNT                                0x028e // RO
//Bit 31:13        reserved
//Bit 12: 0        reg_lfifo_buf_count    // unsigned ,    RO, default = 0
#define VDIN_PP_HDR2_CTRL                                      0x0291 // RW
#define PP_MATRIXO_TOP_EN_BIT		15
#define PP_MATRIXO_TOP_EN_WID		1
#define PP_MATRIXI_TOP_EN_BIT		14
#define PP_MATRIXI_TOP_EN_WID		1
#define PP_HDR2_TOP_EN_BIT		13
#define PP_HDR2_TOP_EN_WID		1
//Bit 31:21        reserved
//Bit 20:18        reg_din_swap           // unsigned , RW, default = 0
//Bit 17           reg_out_fmt            // unsigned , RW, default = 0
//Bit 16           reg_only_mat           // unsigned , RW, default = 0
//Bit 15           reg_matrixo_en           // unsigned , RW, default = 1
//Bit 14           reg_matrixi_en           // unsigned , RW, default = 1
//Bit 13           reg_hdr2_top_en        // unsigned , RW, default = 0
//Bit 12           reg_cgain_mode         // unsigned , RW, default = 1
//Bit 11:8         reserved
//Bit  7: 6        reg_gmut_mode          // unsigned , RW, default = 1
//Bit  5           reg_in_shift           // unsigned , RW, default = 0
//Bit  4           reg_in_fmt             // unsigned , RW, default = 1
//Bit  3           reg_eo_enable          // unsigned , RW, default = 1
//Bit  2           reg_oe_enable          // unsigned , RW, default = 1
//Bit  1           reg_ogain_enable       // unsigned , RW, default = 1
//Bit  0           reg_cgain_enable       // unsigned , RW, default = 1
#define VDIN_PP_HDR2_CLK_GATE                                  0x0292 // RW
//Bit  31:0        reg_gclk_ctrl          // unsigned , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF00_01                         0x0293 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef00       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef01       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF02_10                         0x0294 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef02       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef10       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF11_12                         0x0295 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef11       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef12       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF20_21                         0x0296 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef20       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef21       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF22                            0x0297 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_matrixi_coef22       // unsigned , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF30_31                         0x0298 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef30       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef31       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF32_40                         0x0299 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef32       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef40       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_COEF41_42                         0x029a // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixi_coef41       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixi_coef42       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_OFFSET0_1                         0x029b // RW
//Bit 31:27        reserved
//Bit 26:16        reg_matrixi_offst_oup0   // signed , RW, default = 0
//Bit 15:11        reserved
//Bit 10: 0        reg_matrixi_offst_oup1   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_OFFSET2                           0x029c // RW
//Bit 31:11        reserved
//Bit 10: 0        reg_matrixi_offst_oup2   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_PRE_OFFSET0_1                     0x029d // RW
//Bit 31:27        reserved
//Bit 26:16        reg_matrixi_offst_inp0   // signed , RW, default = 0
//Bit 15:11        reserved
//Bit 10: 0        reg_matrixi_offst_inp1   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_PRE_OFFSET2                       0x029e // RW
//Bit 31:11        reserved
//Bit 10: 0        reg_matrixi_offst_inp2   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF00_01                         0x029f // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef00       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef01       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF02_10                         0x02a0 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef02       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef10       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF11_12                         0x02a1 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef11       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef12       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF20_21                         0x02a2 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef20       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef21       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF22                            0x02a3 // RW
//Bit 31:13        reserved
//Bit 12: 0        reg_matrixo_coef22       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF30_31                         0x02a4 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef30       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef31       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF32_40                         0x02a5 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef32       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef40       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_COEF41_42                         0x02a6 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrixo_coef41       // signed , RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_matrixo_coef42       // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_OFFSET0_1                         0x02a7 // RW
//Bit 31:27        reserved
//Bit 26:16        reg_matrixo_offst_oup0   // signed , RW, default = 0
//Bit 15:11        reserved
//Bit 10: 0        reg_matrixo_offst_oup1   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_OFFSET2                           0x02a8 // RW
//Bit 31:11        reserved
//Bit 10: 0        reg_matrixo_offst_oup2   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_PRE_OFFSET0_1                     0x02a9 // RW
//Bit 31:27        reserved
//Bit 26:16        reg_matrixo_offst_inp0   // signed , RW, default = 0
//Bit 15:11        reserved
//Bit 10: 0        reg_matrixo_offst_inp1   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_PRE_OFFSET2                       0x02aa // RW
//Bit 31:11        reserved
//Bit 10: 0        reg_matrixo_offst_inp2   // signed , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_CLIP                              0x02ab // RW
//Bit 31:20        reserved
//Bit 19:8         reg_matrixi_comp_thrd    // signed , RW, default = 0
//Bit 7:5          reg_matrixi_rs           // unsigned , RW, default = 0
//Bit 4:3          reg_matrixi_clmod        // unsigned , RW, default = 0
//Bit 2:0          reserved
#define VDIN_PP_HDR2_MATRIXO_CLIP                              0x02ac // RW
//Bit 31:20        reserved
//Bit 19:8         reg_matrixo_comp_thrd    // signed , RW, default = 0
//Bit 7:5          reg_matrixo_rs           // unsigned , RW, default = 0
//Bit 4:3          reg_matrixo_clmod        // unsigned , RW, default = 0
//Bit 2:0          reserved
#define VDIN_PP_HDR2_CGAIN_OFFT                                0x02ad // RW
//Bit 31:27        reserved
//Bit 26:16        reg_cgain_oft2         // signed , RW, default = 11'h200
//Bit 15:11        reserved
//Bit 10: 0        reg_cgain_oft1         // signed , RW, default = 11'h200
#define VDIN_PP_HDR2_CGAIN_COEF0                               0x02b5 // RW
//Bit 31:28        reserved
//Bit 27:16        c_gain_lim_coef1       // unsigned , RW, default = 0
//Bit 15:12        reserved
//Bit 11: 0        c_gain_lim_coef0       // unsigned , RW, default = 0
#define VDIN_PP_HDR2_CGAIN_COEF1                               0x02b6 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_matrix_rgb             // unsigned , RW, default = 0
//Bit 15:12        reserved
//Bit 11: 0        c_gain_lim_coef2       // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_CTRL                                 0x02b9 // RW
//Bit 31:14        reserved
//Bit 13: 8        reg_adps_cl_max         // unsigned , RW, default = 0
//Bit  7           reg_adps_cl_clip_en     // unsigned , RW, default = 0
//Bit  6           reg_adps_cl_enable2     // unsigned , RW, default = 0
//Bit  5           reg_adps_cl_enable1     // unsigned , RW, default = 0
//Bit  4           reg_adps_cl_enable0     // unsigned , RW, default = 0
//Bit  3: 2        reserved
//Bit  1: 0        reg_adps_cl_mode        // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_ALPHA0                               0x02ba // RW
//Bit 31:30        reserved
//Bit 29:16        reg_adps_cl_alpha1      // unsigned , RW, default = 0
//Bit 15:14        reserved
//Bit 13: 0        reg_adps_cl_alpha0      // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_ALPHA1                               0x02bb // RW
//Bit 31:28        reserved
//Bit 27:24        reg_adps_cl_shift0      // unsigned , RW, default = 0
//Bit 23:20        reg_adps_cl_shift1      // unsigned , RW, default = 0
//Bit 19:16        reg_adps_cl_shift2      // unsigned , RW, default = 0
//Bit 15:14        reserved
//Bit 13: 0        reg_adps_cl_alpha2      // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_BETA0                                0x02bc // RW
//Bit 31:21        reserved
//Bit 20: 0        reg_adps_cl_beta0       // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_BETA1                                0x02bd // RW
//Bit 31:21        reserved
//Bit 20: 0        reg_adps_cl_beta1       // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_BETA2                                0x02be // RW
//Bit 31:21        reserved
//Bit 20: 0        reg_adps_cl_beta2       // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_COEF0                                0x02bf // RW
//Bit 31:28        reserved
//Bit 27:16        reg_adps_cl_ys_coef1    // unsigned , RW, default = 0
//Bit 15:12        reserved
//Bit 11: 0        reg_adps_cl_ys_coef0    // unsigned , RW, default = 0
#define VDIN_PP_HDR2_ADPS_COEF1                                0x02c0 // RW
//Bit 31:12        reserved
//Bit 11: 0        reg_adps_cl_ys_coef2    // unsigned , RW, default = 0
#define VDIN_PP_HDR2_GMUT_CTRL                                 0x02c1 // RW
//Bit 31: 5        reserved
//Bit  4           reg_new_mode           // unsigned , RW, default = 0
//Bit  3: 0        reg_gmut_shift         // unsigned , RW, default = 0
#define VDIN_PP_HDR2_GMUT_COEF0                                0x02c2 // RW
//Bit 31:16        reg_gmut_coef01        // unsigned , RW, default = 0
//Bit 15: 0        reg_gmut_coef00        // unsigned , RW, default = 0
#define VDIN_PP_HDR2_GMUT_COEF1                                0x02c3 // RW
//Bit 31:16        reg_gmut_coef10        // unsigned , RW, default = 0
//Bit 15: 0        reg_gmut_coef02        // unsigned , RW, default = 0
#define VDIN_PP_HDR2_GMUT_COEF2                                0x02c4 // RW
//Bit 31:16        reg_gmut_coef12        // unsigned , RW, default = 0
//Bit 15: 0        reg_gmut_coef11        // unsigned , RW, default = 0
#define VDIN_PP_HDR2_GMUT_COEF3                                0x02c5 // RW
//Bit 31:16        reg_gmut_coef21        // unsigned , RW, default = 0
//Bit 15: 0        reg_gmut_coef20        // unsigned , RW, default = 0
#define VDIN_PP_HDR2_GMUT_COEF4                                0x02c6 // RW
//Bit 31:16        reserved
//Bit 15: 0        reg_gmut_coef22        // unsigned , RW, default = 0
#define VDIN_PP_EOTF_LUT_ADDR_PORT                          0x02af // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_eotf_lut_addr      // unsigned , RW, default = 0
#define VDIN_PP_EOTF_LUT_DATA_PORT                             0x02b0 // RW
//Bit 31: 0        reg_eotf_lut_data      // unsigned , RW, default = 0
#define VDIN_PP_OETF_LUT_ADDR_PORT                             0x02b1 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_oetf_lut_addr      // unsigned , RW, default = 0
#define VDIN_PP_OETF_LUT_DATA_PORT                             0x02b2 // RW
//Bit 31: 0        reg_oetf_lut_data      // unsigned , RW, default = 0
#define VDIN_PP_OGAIN_LUT_ADDR_PORT                            0x02b7 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_ogain_lut_addr     // unsigned , RW, default = 0
#define VDIN_PP_OGAIN_LUT_DATA_PORT                            0x02b8 // RW
//Bit 31: 0        reg_ogain_lut_data     // unsigned , RW, default = 0
#define VDIN_PP_CGAIN_LUT_ADDR_PORT                            0x02b3 // RW
//Bit 31: 8        reserved
//Bit  7: 0        reg_cgain_lut_addr     // unsigned , RW, default = 0
#define VDIN_PP_CGAIN_LUT_DATA_PORT                            0x02b4 // RW
//Bit 31: 0        reg_cgain_lut_data     // unsigned , RW, default = 0
#define VDIN_PP_HDR2_PIPE_CTRL1                                0x02c7 // RW
//Bit 31:0        reg_pipe_ctrl1          // unsigned , RW, default = 32'h04040a0a
#define VDIN_PP_HDR2_PIPE_CTRL2                                0x02c8 // RW
//Bit 31:0        reg_pipe_ctrl2          // unsigned , RW, default = 32'h0a0a0b0b
#define VDIN_PP_HDR2_PIPE_CTRL3                                0x02c9 // RW
//Bit 31:0        reg_pipe_ctrl3          // unsigned , RW, default = 32'h16160404
#define VDIN_PP_HDR2_PROC_WIN1                                 0x02ca // RW
//Bit 31:0        reg_proc_win1           // unsigned , RW, default = 0
#define VDIN_PP_HDR2_PROC_WIN2                                 0x02cb // RW
//Bit 31:0        reg_proc_win2           // unsigned , RW, default = 0
#define VDIN_PP_HDR2_MATRIXI_EN_CTRL                           0x02cc // RW
#define PP_HDR2_MATRIXI_EN_BIT	0
#define PP_HDR2_MATRIXI_EN_WID	8
//Bit 31:8        reserved
//Bit 7:0         reg_matrixi_en_ctrl     // unsigned , RW, default = 0
#define VDIN_PP_HDR2_MATRIXO_EN_CTRL                           0x02cd // RW
//Bit 31:8        reserved
//Bit 7:0         reg_matrixo_en_ctrl    // unsigned , RW, default = 0
#define VDIN_PP_HDR2_HIST_CTRL                                 0x02ce // RW
//Bit 31:24       reserved
//Bit 23:16       reg_vcbus_rd_idx        // unsigned , RW, default = 0
//Bit 15:0        reg_hist_ctrl           // unsigned , RW, default = 16'h1400
#define VDIN_PP_HDR2_HIST_H_START_END                          0x02cf // RW
//Bit 31:29       reserved
//Bit 28:0        reg_hist_h_start_end    // unsigned , RW, default = 0
#define VDIN_PP_HDR2_HIST_V_START_END                          0x02d0 // RW
//Bit 31:29       reserved
//Bit 28:0        reg_hist_v_start_end    // unsigned , RW, default = 0
#define VDIN_PP_HDR2_HIST_RD                                   0x02ae // RO
//Bit 31:24       reserved
//Bit 23:0        reg_hist_status         // unsigned , RO, default = 0
#define VDIN_LUMA_HIST_GCLK_CTRL                            0x02d1 // RW
//Bit 31: 2        reserved
//Bit  1: 0        reg_luma_hist_gclk_ctrl     // unsigned ,    RW, default = 0
#define VDIN_LUMA_HIST_CTRL                                 0x02d2 // RW
//Bit 31:24        reg_luma_hist_pix_white_th
// unsigned ,    RW, default = 0    larger than this th is counted as white pixel
//Bit 23:16        reg_luma_hist_pix_black_th
// unsigned ,    RW, default = 0    less than this th is counted as black pixel
//Bit 15:12        reserved
//Bit 11           reg_luma_hist_only_34bin
// unsigned ,    RW, default = 0    34 bin only mode, including white/black
//Bit 10: 9        reg_luma_hist_spl_sft       // unsigned ,    RW, default = 0
//Bit  8           reserved
//Bit  7: 5        reg_luma_hist_dnlp_low
// unsigned ,    RW, default = 0    the real pixels in each bins got by
//VDIN_DNLP_LUMA_HISTXX should multiple with 2^(dnlp_low+3)
//Bit  4: 2        reserved
//Bit  1           reg_luma_hist_win_en
// unsigned ,    RW, default = 0    1'b0: hist used for full picture;
//1'b1: hist used for pixels within hist window
//Bit  0           reserved
#define VDIN_LUMA_HIST_H_START_END                          0x02d3 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_luma_hist_hstart
// unsigned ,    RW, default = 0    horizontal start value to #define hist window
//Bit 15:13        reserved
//Bit 12:0         reg_luma_hist_hend
// unsigned ,    RW, default = 0    horizontal end value to #define hist window
#define VDIN_LUMA_HIST_V_START_END                          0x02d4 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_luma_hist_vstart
// unsigned ,    RW, default = 0    vertical start value to #define hist window
//Bit 15:13        reserved
//Bit 12:0         reg_luma_hist_vend
// unsigned ,    RW, default = 0    vertical end value to #define hist window
#define VDIN_LUMA_HIST_DNLP_IDX                             0x02d5 // RW
//Bit 31: 6        reserved
//Bit  5: 0        reg_luma_hist_dnlp_index    // unsigned ,    RW, default = 0
#define VDIN_LUMA_HIST_DNLP_GRP                             0x02d6 // RO
//Bit 31:0         reg_luma_hist_dnlp_group    // unsigned ,    RO, default = 0
#define VDIN_LUMA_HIST_MAX_MIN                              0x02d7 // RO
//Bit 31:16        reserved
//Bit 15: 8        reg_luma_hist_max
// unsigned ,    RO, default = 0        maximum value
//Bit  7: 0        reg_luma_hist_min
// unsigned ,    RO, default = 8'hff    minimum value
#define VDIN_LUMA_HIST_SPL_VAL                              0x02d8 // RO
//Bit 31:0         reg_luma_hist_spl_val
// unsigned ,    RO, default = 0    counts for the total luma value
#define VDIN_LUMA_HIST_SPL_CNT                              0x02d9 // RO
//Bit 31:24        reserved
//Bit 23:0         reg_luma_hist_spl_cnt
// unsigned ,    RO, default = 0    counts for the total calculated pixels
#define VDIN_LUMA_HIST_CHROMA_SUM                           0x02da // RO
//Bit 31:0         reg_luma_hist_chroma_sum
// unsigned ,    RO, default = 0    counts for the total chroma value
#define VDIN_PP_BLKBAR_CTRL1                              0x02e1 // RW
//Bit 31:30        reg_blkbar_gclk_ctrl           // unsigned ,    RW, default = 0
//Bit 29: 9        reserved
//Bit  8           reg_blkbar_white_enable        // unsigned ,    RW, default = 0
//Bit  7: 0        reg_blkbar_white_level         // unsigned ,    RW, default = 0
#define VDIN_PP_BLKBAR_CTRL0                                0x02e2 // RW
//Bit 31:24        reg_blkbar_black_level
// unsigned ,    RW, default = 0    threshold to judge a black point
//Bit 23:21        reserved
//Bit 20: 8        reg_blkbar_h_width
// unsigned ,    RW, default = 0    left and right region width
//Bit  7: 5        reg_blkbar_comp_sel
// unsigned ,    RW, default = 0    select yin or uin or vin to be the valid input
//Bit  4           reg_blkbar_sw_statistic_en
// unsigned ,    RW, default = 0    enable software statistic of each block black points number
//Bit  3: 0        reserved
#define VDIN_PP_BLKBAR_H_START_END                          0x02e3 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_blkbar_hstart
// unsigned ,    RW, default = 0    horizontal Left region start
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_hend
// unsigned ,    RW, default = 0    horizontal Right region end
#define VDIN_PP_BLKBAR_V_START_END                          0x02e4 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_blkbar_vstart
// unsigned ,    RW, default = 0    vertical Left region start
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_vend
// unsigned ,    RW, default = 0    vertical Right region end
#define VDIN_PP_BLKBAR_CNT_THRESHOLD                        0x02e5 // RW
//Bit 31:20        reserved
//Bit 19: 0        reg_blkbar_cnt_threshold
// unsigned ,    RW, default = 0    threshold to judge whether a block is totally black
#define VDIN_PP_BLKBAR_ROW_TH1_TH2                          0x02e6 // RW
//Bit 31:29        reserved
//Bit 28:16        reg_blkbar_row_th1
// unsigned ,    RW, default = 0    threshold of the top blackbar
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_row_th2
// unsigned ,    RW, default = 0    threshold of the bottom blackbar
#define VDIN_PP_BLKBAR_IND_LEFT_START_END                   0x02e7 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_blkbar_ind_left_start
// unsigned ,    RO, default = 0    horizontal start of the left region in the current searching
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_ind_left_end
// unsigned ,    RO, default = 0    horizontal end of the left region in the current searching
#define VDIN_PP_BLKBAR_IND_RIGHT_START_END                  0x02e8 // RO
//Bit 31:29        reserved
//Bit 28:16        reg_blkbar_ind_right_start
// unsigned ,    RO, default = 0    horizontal start of the right region in the current searching
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_ind_right_end
// unsigned ,    RO, default = 0    horizontal end of the right region in the current searching
#define VDIN_PP_BLKBAR_IND_LEFT1_CNT                        0x02e9 // RO
//Bit 31:20        reserved
//Bit 19: 0        reg_blkbar_ind_left1_cnt
// unsigned ,    RO, default = 0    black pixel counter. left part of the left region
#define VDIN_PP_BLKBAR_IND_LEFT2_CNT                        0x02ea // RO
//Bit 31:20        reserved
//Bit 19: 0        reg_blkbar_ind_left2_cnt
// unsigned ,    RO, default = 0    black pixel counter. right part of the left region
#define VDIN_PP_BLKBAR_IND_RIGHT1_CNT                       0x02eb // RO
//Bit 31:20        reserved
//Bit 19: 0        reg_blkbar_ind_right1_cnt
// unsigned ,    RO, default = 0    black pixel counter. left part of the right region
#define VDIN_PP_BLKBAR_IND_RIGHT2_CNT                       0x02ec // RO
//Bit 31:20        reserved
//Bit 19: 0        reg_blkbar_ind_right2_cnt
// unsigned ,    RO, default = 0    black pixel counter. right part of the right region
#define VDIN_PP_BLKBAR_STATUS0                              0x02ed // RO
//Bit 31:30        reserved
//Bit 29           reg_blkbar_ind_black_det_done
// unsigned ,    RO, default = 0    left/right black detection done
//Bit 28:16        reg_blkbar_top_pos
// unsigned ,    RO, default = 0    top black bar position
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_bot_pos    // unsigned , RO, default = 0 bottom black bar position
#define VDIN_PP_BLKBAR_STATUS1                              0x02ee // RO
//Bit 31:29        reserved
//Bit 28:16        reg_blkbar_left_pos  // unsigned , RO, default = 0 left black bar position
//Bit 15:13        reserved
//Bit 12: 0        reg_blkbar_right_pos // unsigned , RO, default = 0 right black bar position

/* s5 new add registers bank 1 end */

#endif /* __VDIN_S5_REGS_H */
