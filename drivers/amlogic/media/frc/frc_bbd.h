/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __FRC_BBD_H__
#define __FRC_BBD_H__

u16 frc_bbd_choose_edge(struct frc_dev_s *frc_devp, u16 edge_posi2, u8 edge_posi_valid2,
				u16 edge_val2, u16 edge_first_posi,
				u8 edge_first_posi_valid, u16 edge_first_val, u16 edge_line_th);
void frc_bbd_ctrl(struct frc_dev_s *frc_devp);

u16 frc_bbd_fw_valid12_sel(u8  bb_valid2, u8  bb_valid1,
				u16 bb_posi2, u16 bb_posi1, u16 bb_motion,
				u16 det_init, u16 det_st, u16 det_ed,
				u8  bb_valid_ratio, u16 lsize, u8 reg_sel2_high_en,
				u8 motion_sel1_high_en, u8 *sel2_high_mode, u8 bb_idx);
u16 frc_bbd_line_stable(struct frc_dev_s *frc_devp, u8 *stable_flag, u16 *final_line1,
				u16 pre_final_posi, u16 pre_cur_posi_delta, u16 det_st,
				u16 det_ed, u8 line_idx);
u16 frc_bbd_edge_th_gen(struct frc_dev_s *frc_devp, u32 *apl_val);
u16 frc_bbd_black_th_gen(struct frc_dev_s *frc_devp, u32 *apl_val);
void frc_bbd_param_init(struct frc_dev_s *frc_devp);
// void frc_bbd_change_resolution_set_val(structfrc_dev_s *frc_devp);
#endif

