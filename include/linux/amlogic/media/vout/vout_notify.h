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
	struct vinfo_s *(*get_vinfo)(void *data);
	int (*set_vmode)(enum vmode_e vmode, void *data);
	enum vmode_e (*validate_vmode)(char *name, unsigned int frac,
				       void *data);
	int (*check_same_vmodeattr)(char *name, void *data);
	int (*vmode_is_supported)(enum vmode_e vmode, void *data);
	int (*disable)(enum vmode_e vmode, void *data);
	int (*set_state)(int state, void *data);
	int (*clr_state)(int state, void *data);
	int (*get_state)(void *data);
	int (*get_disp_cap)(char *buf, void *data);
	int (*set_vframe_rate_hint)(int policy, void *data);
	int (*get_vframe_rate_hint)(void *data);
	void (*set_bist)(unsigned int num, void *data);
	void (*set_bl_brightness)(unsigned int brightness, void *data);
	unsigned int (*get_bl_brightness)(void *data);
	int (*vout_suspend)(void *data);
	int (*vout_resume)(void *data);
	int (*vout_shutdown)(void *data);
};

struct vout_server_s {
	struct list_head list;
	char *name;
	struct vout_op_s op;
	void *data;
};

struct vout_module_s {
	struct list_head vout_server_list;
	struct vout_server_s *curr_vout_server;
	struct vout_server_s *next_vout_server;
	unsigned int init_flag;
	/* fr_policy: 0=disable, 1=nearby, 2=force */
	unsigned int fr_policy;
};

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
int vout_register_client(struct notifier_block *p);
int vout_unregister_client(struct notifier_block *p);
int vout_notifier_call_chain(unsigned int long, void *p);
int vout_register_server(struct vout_server_s *p);
int vout_unregister_server(struct vout_server_s *p);

int get_vout_disp_cap(char *buf);
struct vinfo_s *get_current_vinfo(void);
enum vmode_e get_current_vmode(void);
int set_vframe_rate_hint(int duration);
int get_vframe_rate_hint(void);
int set_vframe_rate_policy(int policy);
int get_vframe_rate_policy(void);
void set_vout_bist(unsigned int bist);
void set_vout_bl_brightness(unsigned int brightness);
unsigned int get_vout_bl_brightness(void);
#else
static inline int vout_register_client(struct notifier_block *p)
{
	return 0;
}

static inline int vout_unregister_client(struct notifier_block *p)
{
	return 0;
}

static inline int vout_notifier_call_chain(unsigned long val, void *p)
{
	return 0;
}

static inline int vout_register_server(struct vout_server_s *p)
{
	return 0;
}

static inline int vout_unregister_server(struct vout_server_s *p)
{
	return 0;
}

static inline int get_vout_disp_cap(char *buf)
{
	return 0;
}

static inline struct vinfo_s *get_current_vinfo(void)
{
	return NULL;
}

static inline enum vmode_e get_current_vmode(void)
{
	return 0;
}

static inline int set_vframe_rate_hint(int duration)
{
	return 0;
}

static inline int get_vframe_rate_hint(void)
{
	return 0;
}

static inline int set_vframe_rate_policy(int policy)
{
	return 0;
}

static inline int get_vframe_rate_policy(void)
{
	return 0;
}

static inline void set_vout_bist(unsigned int bist)
{
	/*return;*/
}

static inline void set_vout_bl_brightness(unsigned int brightness)
{
	/*return;*/
}

static inline unsigned int get_vout_bl_brightness(void)
{
	return 0;
}

#endif

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
int vout2_register_client(struct notifier_block *p);
int vout2_unregister_client(struct notifier_block *p);
int vout2_notifier_call_chain(unsigned int long, void *p);
int vout2_register_server(struct vout_server_s *p);
int vout2_unregister_server(struct vout_server_s *p);

int get_vout2_disp_cap(char *buf);
struct vinfo_s *get_current_vinfo2(void);
enum vmode_e get_current_vmode2(void);
int set_vframe2_rate_hint(int duration);
int get_vframe2_rate_hint(void);
int set_vframe2_rate_policy(int policy);
int get_vframe2_rate_policy(void);
void set_vout2_bist(unsigned int bist);
void set_vout2_bl_brightness(unsigned int brightness);
unsigned int get_vout2_bl_brightness(void);

enum vmode_e validate_vmode2(char *name, unsigned int frac);
void set_vout2_init(enum vmode_e mode);
void update_vout2_viu(void);

int set_vout2_vmode(enum vmode_e mode);
int set_vout2_mode_pre_process(enum vmode_e mode);
int set_vout2_mode_post_process(enum vmode_e mode);

#else
static inline int vout2_register_client(struct notifier_block *p)
{
	return 0;
}

static inline int vout2_unregister_client(struct notifier_block *p)
{
	return 0;
}

static inline int vout2_notifier_call_chain(unsigned long val, void *p)
{
	return 0;
}

static inline int vout2_register_server(struct vout_server_s *p)
{
	return 0;
}

static inline int vout2_unregister_server(struct vout_server_s *p)
{
	return 0;
}

static inline int get_vout2_disp_cap(char *buf)
{
	return 0;
}

static inline struct vinfo_s *get_current_vinfo2(void)
{
	return 0;
}

static inline enum vmode_e get_current_vmode2(void)
{
	return 0;
}

static inline int set_vframe2_rate_hint(int duration)
{
	return 0;
}

static inline int get_vframe2_rate_hint(void)
{
	return 0;
}

static inline int set_vframe2_rate_policy(int policy)
{
	return 0;
}

static inline int get_vframe2_rate_policy(void)
{
	return 0;
}

static inline void set_vout2_bist(unsigned int bist)
{
	/*return;*/
}

static inline void set_vout2_bl_brightness(unsigned int brightness)
{
	/*return;*/
}

static inline unsigned int get_vout2_bl_brightness(void)
{
	return 0;
}

static inline int set_vout2_vmode(enum vmode_e mode)
{
	return 0;
}

static inline int set_vout2_mode_pre_process(enum vmode_e mode)
{
	return 0;
}

static inline int set_vout2_mode_post_process(enum vmode_e mode)
{
	return 0;
}

#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
int vout3_register_client(struct notifier_block *p);
int vout3_unregister_client(struct notifier_block *p);
int vout3_notifier_call_chain(unsigned int long, void *p);
int vout3_register_server(struct vout_server_s *p);
int vout3_unregister_server(struct vout_server_s *p);

int get_vout3_disp_cap(char *buf);
struct vinfo_s *get_current_vinfo3(void);
enum vmode_e get_current_vmode3(void);
int set_vframe3_rate_hint(int duration);
int get_vframe3_rate_hint(void);
int set_vframe3_rate_policy(int policy);
int get_vframe3_rate_policy(void);
void set_vout3_bist(unsigned int bist);
void set_vout3_bl_brightness(unsigned int brightness);
unsigned int get_vout3_bl_brightness(void);

#else
static inline int vout3_register_client(struct notifier_block *p)
{
	return 0;
}

static inline int vout3_unregister_client(struct notifier_block *p)
{
	return 0;
}

static inline int vout3_notifier_call_chain(unsigned long val, void *p)
{
	return 0;
}

static inline int vout3_register_server(struct vout_server_s *p)
{
	return 0;
}

static inline int vout3_unregister_server(struct vout_server_s *p)
{
	return 0;
}

static inline int get_vout3_disp_cap(char *buf)
{
	return 0;
}

static inline struct vinfo_s *get_current_vinfo3(void)
{
	return 0;
}

static inline enum vmode_e get_current_vmode3(void)
{
	return 0;
}

static inline int set_vframe3_rate_hint(int duration)
{
	return 0;
}

static inline int get_vframe3_rate_hint(void)
{
	return 0;
}

static inline int set_vframe3_rate_policy(int policy)
{
	return 0;
}

static inline int get_vframe3_rate_policy(void)
{
	return 0;
}

static inline void set_vout3_bist(unsigned int bist)
{
	/*return;*/
}

static inline void set_vout3_bl_brightness(unsigned int brightness)
{
	/*return;*/
}

static inline unsigned int get_vout3_bl_brightness(void)
{
	return 0;
}

#endif

#define VOUT_EVENT_MODE_CHANGE_PRE     0x00010000
#define VOUT_EVENT_MODE_CHANGE         0x00020000
#define VOUT_EVENT_OSD_BLANK           0x00030000
#define VOUT_EVENT_OSD_DISP_AXIS       0x00040000
#define VOUT_EVENT_OSD_PREBLEND_ENABLE 0x00050000
#define VOUT_EVENT_SYS_INIT            0x00060000

char *get_vout_mode_internal(void);
char *get_vout_mode_uboot(void);
char *get_vout2_mode_uboot(void);
char *get_vout3_mode_uboot(void);
int get_vout_mode_uboot_state(void);
int get_vout2_mode_uboot_state(void);
int get_vout3_mode_uboot_state(void);

int set_vout_mode(char *name);
void set_vout_init(enum vmode_e mode);
void update_vout_viu(void);
int set_vout_vmode(enum vmode_e mode);
int set_vout_mode_pre_process(enum vmode_e mode);
int set_vout_mode_post_process(enum vmode_e mode);
int set_vout_mode_name(char *name);
enum vmode_e validate_vmode(char *name, unsigned int frac);
int set_current_vmode(enum vmode_e mode);
void disable_vout_mode_set_sysfs(void);
unsigned int vout_frame_rate_measure(int index);
unsigned int vout_frame_rate_msr_high_res(int index);
#endif /* _VOUT_NOTIFY_H_ */
