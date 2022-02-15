/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_CLK_H__
#define __FRC_CLK_H__

#define HEIGHT_4K 2160
#define WIDTH_4K  3840
#define HEIGHT_2K 1080
#define WIDTH_2K  1920

#define  HME_ME_OFFSET 0x100

/*osd only always support 960x540  0: 960x540, 1: 1920x1080*/
#define OSD_MC_RATIO 0  //0->1:1; 1->1:2

/*ME max size 960x540*/
#define ME_MVX_BIT		12									//include sign bit for ME@960x540
#define ME_MVY_BIT		11									//include sign bit for ME@960x540
#define	MAX_ME_MVX		511                                 // for ME@960x540  => means +/-128 ME pixels for MVx
#define	MAX_ME_MVY		160                                 // for ME@960x540  => means (40+2)+(40+4+2) line memory

#define HME_MVX_BIT		10									//include sign bit for HME@960x540
#define HME_MVY_BIT		9									//include sign bit for HME@960x540
#define MAX_HME_MVX     127                                 // for HME@960x540
#define MAX_HME_MVY     40                                  // for HME@960x540

#define ME_FINER_HIST_BIT     4 //
#define ME_ROUGH_X_HIST_BIT   4 // include sign bit for ME@960x540
#define ME_ROUGH_Y_HIST_BIT   3 // include sign bit for ME@960x540

#define HME_FINER_HIST_BIT     4 // actually not used for HME
#define HME_ROUGH_X_HIST_BIT   4 // include sign bit for HME@960x540
#define HME_ROUGH_Y_HIST_BIT   3 // include sign bit for HME@960x540

#define MAX_MC_Y_VRANG   64  // one side of Luma range(under MC scale)
#define MAX_MC_C_VRANG   64  // one side of Chroma range

#define MAX_INP_UNDONE_CNT         1
#define MAX_ME_UNDONE_CNT          1
#define MAX_MC_UNDONE_CNT          1
#define MAX_VP_UNDONE_CNT          1

extern void __iomem *frc_clk_base;
extern void __iomem *vpu_base;
void frc_clk_init(struct frc_dev_s *frc_devp);
void set_frc_clk_disable(void);
void frc_init_config(struct frc_dev_s *devp);
void set_frc_enable(u32 en);
void set_frc_bypass(u32 en);
void frc_pattern_on(u32 en);
void frc_set_buf_num(u32 frc_fb_num);
void frc_top_init(struct frc_dev_s *frc_devp);
void frc_inp_init(u32 frc_fb_num, u32 film_hwfw_sel);
void config_phs_lut(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode);
void config_phs_regs(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode);
void config_me_top_hw_reg(void);
void init_bb_xyxy(u32 hsize ,u32 vsize, u32 is_me1mc4);
void fw_param_bbd_init(u32 hsize ,u32 vsize);
void sys_fw_param_frc_init(u32 frm_hsize, u32 frm_vsize, u32 is_me1mc4);
void config_loss_out(u32 fmt422);
void enable_nr(void);
void set_mc_lbuf_ext(void);
void cfg_me_loss(u32 me_loss_en);
void cfg_mc_loss(u32 mc_loss_en);
void recfg_memc_mif_base_addr(u32 base_ofst);
void frc_internal_initial(struct frc_dev_s *frc_devp);
void frc_dump_reg_tab(void);
void frc_mtx_set(struct frc_dev_s *frc_devp);
void frc_crc_enable(struct frc_dev_s *frc_devp);
void frc_me_crc_read(struct frc_dev_s *frc_devp);
void frc_mc_crc_read(struct frc_dev_s *frc_devp);
void me_undone_read(struct frc_dev_s *frc_devp);
void mc_undone_read(struct frc_dev_s *frc_devp);
void frc_dump_fixed_table(void);
void frc_reset(u32 onoff);
void frc_mc_reset(u32 onoff);
void frc_force_secure(u32 onoff);
void frc_osdbit_setfalsecolor(u32 falsecolor);
u8 frc_frame_forcebuf_enable(u8 enable);
void frc_frame_forcebuf_count(u8 forceidx);
void inp_undone_read(struct frc_dev_s *frc_devp);
void vp_undone_read(struct frc_dev_s *frc_devp);
u32 vpu_reg_read(u32 addr);

#endif
