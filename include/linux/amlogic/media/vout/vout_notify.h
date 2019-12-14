/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VOUT_NOTIFY_H_
#define _VOUT_NOTIFY_H_

/* Linux Headers */
#include <linux/notifier.h>
#include <linux/list.h>
#include <linux/pm.h>

/* Local Headers */
#include "vinfo.h"

struct vframe_match_s {
	int fps;
	int frame_rate; /* *100 */
	unsigned int duration_num;
	unsigned int duration_den;
};

struct vout_op_s {
	struct vinfo_s *(*get_vinfo)(void);
	int (*set_vmode)(enum vmode_e vmode);
	enum vmode_e (*validate_vmode)(char *name);
	int (*vmode_is_supported)(enum vmode_e vmode);
	int (*disable)(enum vmode_e vmode);
	int (*set_state)(int state);
	int (*clr_state)(int state);
	int (*get_state)(void);
	int (*set_vframe_rate_hint)(int duration);
	int (*set_vframe_rate_end_hint)(void);
	int (*set_vframe_rate_policy)(int policy);
	int (*get_vframe_rate_policy)(void);
	void (*set_bist)(unsigned int num);
	int (*vout_suspend)(void);
	int (*vout_resume)(void);
	int (*vout_shutdown)(void);
};

struct vout_server_s {
	struct list_head list;
	char *name;
	struct vout_op_s op;
};

struct vout_module_s {
	struct list_head vout_server_list;
	struct vout_server_s *curr_vout_server;
};

int vout_register_client(struct notifier_block *p);
int vout_unregister_client(struct notifier_block *p);
int vout_notifier_call_chain(unsigned int long, void *p);
int vout_register_server(struct vout_server_s *p);
int vout_unregister_server(struct vout_server_s *p);

struct vinfo_s *get_current_vinfo(void);
enum vmode_e get_current_vmode(void);
int set_vframe_rate_hint(int duration);
int set_vframe_rate_end_hint(void);
int set_vframe_rate_policy(int pol);
int get_vframe_rate_policy(void);
void set_vout_bist(unsigned int bist);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
int vout2_register_client(struct notifier_block *p);
int vout2_unregister_client(struct notifier_block *p);
int vout2_notifier_call_chain(unsigned int long, void *p);
int vout2_register_server(struct vout_server_s *p);
int vout2_unregister_server(struct vout_server_s *p);

struct vinfo_s *get_current_vinfo2(void);
enum vmode_e get_current_vmode2(void);
int set_vframe2_rate_hint(int duration);
int set_vframe2_rate_end_hint(void);
int set_vframe2_rate_policy(int pol);
int get_vframe2_rate_policy(void);
void set_vout2_bist(unsigned int bist);

#endif

int vout_get_vsource_fps(int duration);

/* vdac ctrl,adc/dac ref signal,cvbs out signal
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
 */
void vdac_enable(bool on, unsigned int module_sel);

#define VOUT_EVENT_MODE_CHANGE_PRE     0x00010000
#define VOUT_EVENT_MODE_CHANGE         0x00020000
#define VOUT_EVENT_OSD_BLANK           0x00030000
#define VOUT_EVENT_OSD_DISP_AXIS       0x00040000
#define VOUT_EVENT_OSD_PREBLEND_ENABLE 0x00050000

/* ********** vout_ioctl ********** */
#define VOUT_IOC_TYPE            'C'
#define VOUT_IOC_NR_GET_VINFO    0x0
#define VOUT_IOC_NR_SET_VINFO    0x1

#define VOUT_IOC_CMD_GET_VINFO   \
		_IOR(VOUT_IOC_TYPE, VOUT_IOC_NR_GET_VINFO, struct vinfo_base_s)
#define VOUT_IOC_CMD_SET_VINFO   \
		_IOW(VOUT_IOC_TYPE, VOUT_IOC_NR_SET_VINFO, struct vinfo_base_s)
/* ******************************** */

char *get_vout_mode_internal(void);
char *get_vout_mode_uboot(void);

int set_vout_mode(char *name);
void set_vout_init(enum vmode_e mode);
void update_vout_viu(void);
int set_vout_vmode(enum vmode_e mode);
enum vmode_e validate_vmode(char *name);
int set_current_vmode(enum vmode_e mode);

#endif /* _VOUT_NOTIFY_H_ */
