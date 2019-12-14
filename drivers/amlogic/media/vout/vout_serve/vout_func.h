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

/* [3: 2] cntl_viu2_sel_venc:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 * [1: 0] cntl_viu1_sel_venc:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 */
#define VPU_VIU_VENC_MUX_CTRL                      0x271a
/* [2] Enci_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 * [1] Encl_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 * [0] Encp_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 */
#define VPU_VENCX_CLK_CTRL                         0x2785
#define VPP_POSTBLEND_H_SIZE                       0x1d21
#define VPP2_POSTBLEND_H_SIZE                      0x1921
#define VPP_WRBAK_CTRL                             0x1df9

struct vout_cdev_s {
	dev_t         devno;
	struct cdev   cdev;
	struct device *dev;
};

#ifdef CONFIG_AMLOGIC_HDMITX
int get_hpd_state(void);
#endif
int vout_get_hpd_state(void);
void vout_trim_string(char *str);

struct vinfo_s *get_invalid_vinfo(int index);
struct vout_module_s *vout_func_get_vout_module(void);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
struct vout_module_s *vout_func_get_vout2_module(void);
#endif

void vout_func_set_state(int index, enum vmode_e mode);
void vout_func_update_viu(int index);
int vout_func_set_vmode(int index, enum vmode_e mode);
int vout_func_set_current_vmode(int index, enum vmode_e mode);
enum vmode_e vout_func_validate_vmode(int index, char *name);
int vout_func_set_vframe_rate_hint(int index, int duration);
int vout_func_set_vframe_rate_end_hint(int index);
int vout_func_set_vframe_rate_policy(int index, int policy);
int vout_func_get_vframe_rate_policy(int index);
void vout_func_set_test_bist(int index, unsigned int bist);
int vout_func_vout_suspend(int index);
int vout_func_vout_resume(int index);
int vout_func_vout_shutdown(int index);
int vout_func_vout_register_server(int index,
				   struct vout_server_s *mem_server);
int vout_func_vout_unregister_server(int index,
				     struct vout_server_s *mem_server);

int set_current_vmode(enum vmode_e);
enum vmode_e validate_vmode(char *p);

int vout_suspend(void);
int vout_resume(void);
int vout_shutdown(void);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
int set_current_vmode2(enum vmode_e);
enum vmode_e validate_vmode2(char *p);

int vout2_suspend(void);
int vout2_resume(void);
int vout2_shutdown(void);
#endif

#endif
