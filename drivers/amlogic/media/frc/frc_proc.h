/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __FRC_PROC_H__
#define __FRC_PROC_H__

#define ON	1
#define OFF	0

enum vf_sts {
	VFRAME_NO = 0,
	VFRAME_HAVE = 1,
};

irqreturn_t frc_input_isr(int irq, void *dev_id);
irqreturn_t frc_output_isr(int irq, void *dev_id);

void frc_input_tasklet_pro(unsigned long arg);
void frc_output_tasklet_pro(unsigned long arg);

void frc_hw_initial(struct frc_dev_s *frc_devp);

void frc_scene_detect_input(struct frc_fw_data_s *fw_data);
void frc_scene_detect_output(struct frc_fw_data_s *fw_data);

void frc_change_to_state(enum frc_state_e state);
void frc_state_change_finish(struct frc_dev_s *devp);
void frc_state_handle_old(struct frc_dev_s *devp);
void frc_state_handle(struct frc_dev_s *devp);
void frc_input_vframe_handle(struct frc_dev_s *devp, struct vframe_s *vf,
					struct vpp_frame_par_s *cur_video_sts);
void frc_dump_monitor_data(struct frc_dev_s *devp);
void frc_vf_monitor(struct frc_dev_s *devp);
void frc_test_mm_secure_set_off(struct frc_dev_s *devp);
void frc_test_mm_secure_set_on(struct frc_dev_s *devp, u32 start, u32 size);
int frc_memc_set_level(u8 level);
int frc_memc_set_demo(u8 setdemo);
int frc_init_out_line(void);

u32 get_video_enabled(void);

#endif
