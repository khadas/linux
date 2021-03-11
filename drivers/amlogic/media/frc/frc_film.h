/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __FRC_FILM_H__
#define __FRC_FILM_H__

void frc_film_check_mode(struct frc_dev_s *frc_devp, u32 *flmGDif,
					u32 flmW6Dif[][HISDIFNUM],
					u32 flmW12Dif[][HISDIFNUM]);
void frc_film_badedit_ctrl(struct frc_dev_s *frc_devp, u32 *flmGDif,u8 mode);
void frc_film_add_7_seg(struct frc_dev_s *frc_devp);
void frc_film_param_init(struct frc_dev_s *frc_devp);
void frc_mode_check(struct frc_dev_s *frc_devp, u16 real_flag,u32 *flmGDif,u8 mode);
inline u32 frc_cycshift(u32 nflagtestraw, u8 ncycle, u32 mask_value, u8 n);
u8 frc_mix_mod_check(struct frc_dev_s *frc_devp, u32 *flmGDif, u32 flmW6Dif[][HISDIFNUM],
			u32 expect_flag, u8 mode);
void frc_film_detect_ctrl(struct frc_dev_s *frc_devp);
void frc_get_min_max_idx(struct frc_dev_s *frc_devp, u32 *flmdif,
					u32 averag_g, u8 *ab_change, u32 *flag);
#endif
