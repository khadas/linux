/*
 * ISP driver
 *
 * Author: Kele Bai <kele.bai@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __TVIN_ISP_DRV_H
#define __TVIN_ISP_DRV_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/irqreturn.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/device.h>

#include <linux/amlogic/tvin/tvin_v4l2.h>
#include "isp_hw.h"
#include "../tvin_frontend.h"

#define ISP_VER					"2014.01.16a"
#define ISP_NUM					1
#define DEVICE_NAME			        "isp"

#define USE_WORK_QUEUE

#define ISP_FLAG_START				0x00000001
#define ISP_FLAG_AE				0x00000002
#define ISP_FLAG_AWB				0x00000004
#define ISP_FLAG_AF				0x00000008
#define ISP_FLAG_CAPTURE			0x00000010
#define ISP_FLAG_RECORD				0x00000020
#define ISP_WORK_MODE_MASK			0x00000030
#define ISP_FLAG_SET_EFFECT			0x00000040
#define ISP_FLAG_SET_SCENES			0x00000080
#define ISP_FLAG_AF_DBG				0x00000100
#define ISP_FLAG_MWB			        0x00000200
#define ISP_FLAG_BLNR				0x00000400
#define ISP_FLAG_SET_COMB4			0x00000800
#define ISP_TEST_FOR_AF_WIN			0x00001000
#define ISP_FLAG_TOUCH_AF			0x00002000
#define ISP_FLAG_SKIP_BUF			0x00004000
#define ISP_FLAG_TEST_WB			0x00008000
#define ISP_FLAG_RECONFIG                       0x00010000

#define ISP_AF_SM_MASK				(ISP_FLAG_AF|ISP_FLAG_TOUCH_AF)

enum bayer_fmt_e {
	RAW_BGGR = 0,
	RAW_RGGB,
	RAW_GBRG,
	RAW_GRBG,/* 3 */
};
struct isp_info_s {
	enum tvin_port_e fe_port;
	enum tvin_color_fmt_e bayer_fmt;
	enum tvin_color_fmt_e cfmt;
	enum tvin_color_fmt_e dfmt;
	unsigned int h_active;
	unsigned int v_active;
	unsigned short dest_hactive;
	unsigned short dest_vactive;
	unsigned int frame_rate;
	unsigned int skip_cnt;
};
/*config in bsp*/
struct flash_property_s {
	bool	 valid;		 /* true:have flash,false:havn't flash */
	/* false: negative correlation
	 *  true: positive correlation */
	bool     torch_pol_inv;
	 /* false: led1=>pin1 & led2=>pin2,
	  * true: led1=>pin2 & led2=>pin1 */
	bool	 pin_mux_inv;
	/* false: active high, true: active low */
	bool	 led1_pol_inv;
	bool     mode_pol_inv;   /* TORCH  FLASH */
				 /* false: low      high */
				 /* true:  high     low */
};
/*parameters used for ae in sm or driver*/
struct isp_ae_info_s {
	int manul_level;/* each step 2db */
	atomic_t writeable;
};

/*for af debug*/
struct af_debug_s {
	bool            dir;
	/* unsigned int    control; */
	unsigned int    state;
	unsigned int    step;
	unsigned int	min_step;
		 int	max_step;
	unsigned int    delay;
		 int	cur_step;
	unsigned int    pre_step;
	unsigned int	mid_step;
	unsigned int	post_step;
	unsigned int	pre_threshold;
	unsigned int	post_threshold;
	struct isp_blnr_stat_s data[1024];
};
/*for af test debug*/
struct af_debug_test_s {
	unsigned int cnt;
	unsigned int max;
	struct isp_af_stat_s   *af_win;
	struct isp_blnr_stat_s *af_bl;
	struct isp_ae_stat_s   *ae_win;
	struct isp_awb_stat_s  *awb_stat;
};
/*for af fine tune*/
struct isp_af_fine_tune_s {
	unsigned int cur_step;
	struct isp_blnr_stat_s af_data;
};

struct isp_af_info_s {
	unsigned int cur_index;
	/*for lose focus*/
	unsigned int *v_dc;
	bool	     last_move;
	struct isp_blnr_stat_s last_blnr;
	/*for climbing algorithm*/
	unsigned int great_step;
	unsigned int cur_step;
	unsigned int capture_step;
	struct isp_blnr_stat_s *af_detect;
	struct isp_blnr_stat_s af_data[FOCUS_GRIDS];
	/* unsigned char af_delay; */
	atomic_t writeable;
	/*window for full scan&detect*/
	unsigned int x0;
	unsigned int y0;
	unsigned int x1;
	unsigned int y1;
	/*touch window radius*/
	unsigned int radius;
	/* blnr tmp for isr*/
	struct isp_blnr_stat_s isr_af_data;
	unsigned int valid_step_cnt;
	struct isp_af_fine_tune_s af_fine_data[FOCUS_GRIDS];
};

/*for debug cmd*/
struct debug_s {
	unsigned int comb4_mode;
};

struct isp_dev_s {
	int             index;
	dev_t		devt;
	unsigned int    offset;
	struct cdev	cdev;
	struct device	*dev;
	unsigned int    flag;
	unsigned int	vs_cnt;
	/*add for tvin frontend*/
	struct tvin_frontend_s frontend;
	struct tvin_frontend_s *isp_fe;

	struct isp_info_s info;
#ifndef USE_WORK_QUEUE
	struct tasklet_struct isp_task;
	struct task_struct     *kthread;
#else
	struct work_struct isp_wq;
#endif
	struct isp_ae_stat_s isp_ae;
	struct isp_ae_info_s ae_info;
	struct isp_awb_stat_s isp_awb;
	struct isp_af_stat_s isp_af;
	struct isp_af_info_s af_info;
	struct isp_blnr_stat_s blnr_stat;
	struct cam_parameter_s *cam_param;
	struct xml_algorithm_ae_s *isp_ae_parm;
	struct xml_algorithm_awb_s *isp_awb_parm;
	struct xml_algorithm_af_s *isp_af_parm;
	struct xml_capture_s *capture_parm;
	struct wave_s        *wave;
	struct flash_property_s flash;
	struct af_debug_s      *af_dbg;
	struct debug_s         debug;
	/*test for af test win*/
	struct af_debug_test_s af_test;
	/*cmd state for camera*/
	enum cam_cmd_state_e cmd_state;
};

enum data_type_e {
	ISP_U8 = 0,
	ISP_U16,
	ISP_U32,
	/* ISP_FLOAT, //not use */
};

struct isp_param_s {
	const char *name;
	unsigned int *param;
	unsigned char length;
	enum data_type_e type;
};

extern void set_ae_parm(struct xml_algorithm_ae_s *ae_sw,
		char **parm);
extern void set_awb_parm(struct xml_algorithm_awb_s *awb_sw,
		char **parm);
extern void set_af_parm(struct xml_algorithm_af_s *af_sw,
		char **parm);
extern void set_cap_parm(struct xml_capture_s *cap_sw,
		char **parm);
extern void set_wave_parm(struct wave_s *wave,
		char **parm);
extern bool set_gamma_table_with_curve_ratio(unsigned int r,
		unsigned int g,
		unsigned int b);
#endif

