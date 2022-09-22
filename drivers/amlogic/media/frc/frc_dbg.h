/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __FRC_DBG_H__
#define __FRC_DBG_H__

extern const char * const frc_state_ary[];
extern u32 g_input_hsize;
extern u32 g_input_vsize;
extern int frc_dbg_en;
extern int frc_dbg_ctrl;

void frc_power_domain_ctrl(struct frc_dev_s *devp, u32 onoff);
void frc_debug_if(struct frc_dev_s *frc_devp, const char *buf, size_t count);
ssize_t frc_debug_if_help(struct frc_dev_s *devp, char *buf);
void frc_reg_io(const char *buf);
void frc_tool_dbg_store(struct frc_dev_s *devp, const char *buf);
void frc_dump_buf_data(struct frc_dev_s *devp, u32 cma_addr, u32 size);

ssize_t frc_bbd_final_line_param_show(struct class *class,
				struct class_attribute *attr, char *buf);
ssize_t frc_bbd_final_line_param_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t count);
ssize_t frc_vp_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_vp_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
ssize_t frc_logo_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_logo_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
ssize_t frc_iplogo_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_iplogo_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_melogo_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_melogo_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_sence_chg_detect_param_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t frc_sence_chg_detect_param_store(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_fb_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_fb_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_me_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_me_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_search_rang_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_search_rang_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_pixel_lpf_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_pixel_lpf_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_me_rule_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_me_rule_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_film_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_film_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_glb_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_glb_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

#endif
