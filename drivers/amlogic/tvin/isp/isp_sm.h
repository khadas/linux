/*
  * ISP 3A State Machine
  *
  * Author: Kele Bai <kele.bai@amlogic.com>
  *
  * Copyright (C) 2010 Amlogic Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  */

#ifndef __ISP_STATE_MACHINE_H
#define __ISP_STATE_MACHINE_H
#include "isp_drv.h"

enum isp_auto_exposure_state_e {
	AE_INIT,
	AE_SHUTTER_ADJUST,
	AE_GAIN_ADJUST,
	AE_REST,
};

enum isp_auto_white_balance_state_e {
	AWB_IDLE,
	AWB_INIT,
	AWB_CHECK,
};

enum af_state_e {
	AF_NULL,
	AF_DETECT_INIT,
	AF_GET_STEPS_INFO,
	AF_GET_STATUS,
	AF_SCAN_INIT,
	AF_GET_COARSE_INFO_H,
	AF_GET_COARSE_INFO_L,
	AF_CALC_GREAT,
	AF_GET_FINE_INFO,
	AF_CLIMBING,
	AF_FINE,
	AF_SUCCESS,
};
enum isp_capture_state_e {
	CAPTURE_NULL,
	CAPTURE_INIT,
	CAPTURE_PRE_WAIT,/* for time lapse */
	CAPTURE_FLASH_ON,/* turn on flash for red eye */
	CAPTURE_TR_WAIT,
	CAPTURE_TUNE_3A,
	CAPTURE_LOW_GAIN,
	CAPTURE_EYE_WAIT,
	CAPTURE_POS_WAIT,
	CAPTURE_SINGLE,
	CAPTURE_FLASHW,
	CAPTURE_MULTI,
	CAPTURE_END,
};

enum isp_ae_status_s {
	ISP_AE_STATUS_NULL = 0,
	ISP_AE_STATUS_UNSTABLE,
	ISP_AE_STATUS_STABLE,
	ISP_AE_STATUS_UNTUNEABLE,
};

struct isp_ae_sm_s {
	unsigned int pixel_sum;
	unsigned int sub_pixel_sum;
	unsigned int win_l;
	unsigned int win_r;
	unsigned int win_t;
	unsigned int win_b;
	unsigned int alert_r;
	unsigned int alert_g;
	unsigned int alert_b;
	unsigned int cur_gain;
	unsigned int pre_gain;
	unsigned int max_gain;
	unsigned int min_gain;
	unsigned int max_step;
	unsigned int cur_step;
	unsigned int countlimit_r;
	unsigned int countlimit_g;
	unsigned int countlimit_b;
	unsigned int tf_ratio;
	unsigned int change_step;
	unsigned int max_lumasum1;  /* low */
	unsigned int max_lumasum2;
	unsigned int max_lumasum3;
	unsigned int max_lumasum4;	/* high */
	int targ;

	enum isp_auto_exposure_state_e isp_ae_state;
};

enum isp_awb_status_s {
	ISP_AWB_STATUS_NULL = 0,
	ISP_AWB_STATUS_UNSTABLE,
	ISP_AWB_STATUS_STABLE,
};

struct isp_awb_sm_s {
	enum isp_awb_status_s status;
	unsigned int pixel_sum;
	unsigned int win_l;
	unsigned int win_r;
	unsigned int win_t;
	unsigned int win_b;
	unsigned int countlimitrgb;
	unsigned int countlimityh;
	unsigned int countlimitym;
	unsigned int countlimityl;

	unsigned int countlimityuv;
	unsigned char y;
	unsigned char w;
	unsigned char coun;

	enum isp_auto_white_balance_state_e isp_awb_state;

};

enum isp_flash_status_s {
	ISP_FLASH_STATUS_NULL = 0,
	ISP_FLASH_STATUS_ON,
	ISP_FLASH_STATUS_OFF,
};

enum isp_env_status_s {
	ENV_NULL = 0,
	ENV_HIGH,
	ENV_MID,
	ENV_LOW,
};

struct isp_af_sm_s {
	enum af_state_e state;
};

struct isp_capture_sm_s {
	unsigned int adj_cnt;
	unsigned int max_ac_sum;
	unsigned int tr_time;
	unsigned int fr_time;
	unsigned char flash_on;
	enum flash_mode_s  flash_mode;
	enum isp_capture_state_e capture_state;
};

struct isp_sm_s {
	enum isp_ae_status_s status;
	enum isp_flash_status_s flash;
	enum isp_env_status_s env;
	bool ae_down;
	enum af_state_e af_state;
	struct isp_ae_sm_s isp_ae_parm;
	struct isp_awb_sm_s isp_awb_parm;
	struct isp_af_sm_s af_sm;
	struct isp_capture_sm_s cap_sm;
};

struct isp_ae_to_sensor_s {
/* volatile unsigned int send; */
/* volatile unsigned int new_step; */
/* volatile unsigned int shutter; */
/* volatile unsigned int gain; */
	unsigned int send;
	unsigned int new_step;
	unsigned int shutter;
	unsigned int gain;
};

extern struct isp_ae_to_sensor_s ae_sens;

extern void isp_sm_init(struct isp_dev_s *devp);
extern void isp_sm_uninit(struct isp_dev_s *devp);
extern void af_sm_init(struct isp_dev_s *devp);
extern void capture_sm_init(struct isp_dev_s *devp);
extern void isp_set_flash_mode(struct isp_dev_s *devp);
extern void isp_ae_sm(struct isp_dev_s *devp);
extern void isp_awb_sm(struct isp_dev_s *devp);
extern void isp_af_fine_tune(struct isp_dev_s *devp);
extern void isp_af_detect(struct isp_dev_s *devp);
extern int isp_capture_sm(struct isp_dev_s *devp);
extern unsigned long long div64(unsigned long long n, unsigned long long d);
extern void isp_af_save_current_para(struct isp_dev_s *devp);
extern void isp_set_manual_exposure(struct isp_dev_s *devp);
extern unsigned int isp_tune_exposure(struct isp_dev_s *devp);
#endif


