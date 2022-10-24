/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VOUT_FUNC_H_
#define _VOUT_FUNC_H_
#include <linux/cdev.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#define VOUTPR(fmt, args...)     pr_info(fmt "", ## args)
#define VOUTERR(fmt, args...)    pr_err("error: " fmt "", ## args)
#define VOUTDBG(fmt, args...)    pr_debug("debug: " fmt "", ## args)

struct vout_cdev_s {
	dev_t         devno;
	struct cdev   cdev;
	struct device *dev;
};

extern int vout_debug_print;

void vout_trim_string(char *str);

struct vinfo_s *get_invalid_vinfo(int index, unsigned int flag);
struct vout_module_s *vout_func_get_vout_module(void);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
struct vout_module_s *vout_func_get_vout2_module(void);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
struct vout_module_s *vout_func_get_vout3_module(void);
#endif

void vout_vdo_meas_ctrl_init(void);
void vout_viu_mux_update(int index, unsigned int mux_sel);
void vout_viu_mux_clear(int index, unsigned int mux_sel);

void vout_func_set_state(int index, enum vmode_e mode);
void vout_func_update_viu(int index);
int vout_func_set_vmode(int index, enum vmode_e mode);
int vout_func_set_current_vmode(int index, enum vmode_e mode);
int vout_func_check_same_vmodeattr(int index, char *name);
enum vmode_e vout_func_validate_vmode(int index, char *name, unsigned int frac);
int vout_func_get_disp_cap(int index, char *buf);
int vout_func_set_vframe_rate_hint(int index, int duration);
int vout_func_get_vframe_rate_hint(int index);
void vout_func_set_test_bist(int index, unsigned int bist);
void vout_func_set_bl_brightness(int index, unsigned int brightness);
unsigned int vout_func_get_bl_brightness(int index);
int vout_func_vout_suspend(int index);
int vout_func_vout_resume(int index);
int vout_func_vout_shutdown(int index);
int vout_func_vout_register_server(int index,
				   struct vout_server_s *mem_server);
int vout_func_vout_unregister_server(int index,
				     struct vout_server_s *mem_server);
unsigned int vout_parse_vout_name(char *name);

int set_current_vmode(enum vmode_e);
int vout_check_same_vmodeattr(char *name);
enum vmode_e validate_vmode(char *name, unsigned int frac);

int vout_suspend(void);
int vout_resume(void);
int vout_shutdown(void);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
int set_current_vmode2(enum vmode_e);
int vout2_check_same_vmodeattr(char *name);
enum vmode_e validate_vmode2(char *name, unsigned int frac);

int vout2_suspend(void);
int vout2_resume(void);
int vout2_shutdown(void);
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
int set_current_vmode3(enum vmode_e);
int vout3_check_same_vmodeattr(char *name);
enum vmode_e validate_vmode3(char *name, unsigned int frac);

int vout3_suspend(void);
int vout3_resume(void);
int vout3_shutdown(void);
#endif

#endif
