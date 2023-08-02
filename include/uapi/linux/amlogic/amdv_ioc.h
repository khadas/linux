/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DV_IOC_H_
#define _DV_IOC_H_

#include <linux/types.h>

enum pq_item_e {
	PQ_BRIGHTNESS = 0,     /*Brightness */
	PQ_CONTRAST = 1,       /*Contrast */
	PQ_COLORSHIFT = 2,     /*ColorShift or Tint*/
	PQ_SATURATION = 3      /*Saturation or color */
};

enum pq_reset_e {
	RESET_ALL = 0,         /*reset picture mode / pq for all picture mode*/
	RESET_PQ_FOR_ALL = 1,  /*reset pq for all picture mode*/
	RESET_PQ_FOR_CUR = 2   /*reset pq for current picture mode */
};

struct pic_mode_info_s {
	int pic_mode_id;
	unsigned char name[32];
};

struct dv_pq_info_s {
	int pic_mode_id;
	enum pq_item_e item;
	__s16 value;
};

struct dv_full_pq_info_s {
	int pic_mode_id;
	__s16  brightness;  /*Brightness */
	__s16  contrast;    /*Contrast */
	__s16  colorshift;  /*ColorShift or Tint*/
	__s16  saturation;  /*Saturation or color */
};

struct dv_config_file_s {
	unsigned char bin_name[256];
	unsigned char cfg_name[256];
};

struct ambient_cfg_s {
	__u32 update_flag;
	__u32 ambient; /* 1<<16 */
	__u32 t_rearLum;
	__u32 t_frontLux;
	__u32 t_whiteX; /* 1<<15 */
	__u32 t_whiteY; /* 1<<15 */
	__u32 dark_detail;
};

struct dv_config_data_s {
	unsigned int file_name;  // 0:cfg 1:bin
	unsigned int file_size;
	union {
		void *file_data;
		long long file_table;
	};
};

struct dv_user_cfg_s {
	unsigned int cfg_size;
	union {
		void *cfg_data;
		long long cfg_table;
	};
};

struct light_sensor_s {
	int flag;
	__u32 t_frontLux;  /*lux_value = t_frontLux * tFrontLuxScale*/
};

#define DV_M 'D'

/* get Number of Picture Mode */
#define DV_IOC_GET_DV_PIC_MODE_NUM _IOR((DV_M), 0x0, int)

/* get Picture Mode Name of input pic_mode_id */
#define DV_IOC_GET_DV_PIC_MODE_NAME _IOWR((DV_M), 0x1, struct pic_mode_info_s)

/* get current active picture mode */
#define DV_IOC_GET_DV_PIC_MODE_ID _IOR((DV_M), 0x2, int)

/* select active picture mode */
#define DV_IOC_SET_DV_PIC_MODE_ID _IOW((DV_M), 0x3, int)

/* get single pq(contrast or brightness or colorshift or saturation) */
#define DV_IOC_GET_DV_SINGLE_PQ_VALUE _IOWR((DV_M), 0x4, struct dv_pq_info_s)

/* get all pq(contrast, brightness,colorshift ,saturation) */
#define DV_IOC_GET_DV_FULL_PQ_VALUE _IOWR((DV_M), 0x5, struct dv_full_pq_info_s)

/* set single pq(contrast or brightness or colorshift or saturation) */
#define DV_IOC_SET_DV_SINGLE_PQ_VALUE _IOWR((DV_M), 0x6, struct dv_pq_info_s)

/* set all pq(contrast,brightness ,colorshift , saturation) */
#define DV_IOC_SET_DV_FULL_PQ_VALUE _IOWR((DV_M), 0x7, struct dv_full_pq_info_s)

/* reset all pq item  for current picture mode */
#define DV_IOC_SET_DV_PQ_RESET _IOWR((DV_M), 0x8, enum pq_reset_e)

/* set Amlogic_cfg.txt and dv_config.bin dir */
#define DV_IOC_SET_DV_CONFIG_FILE _IOW((DV_M), 0x9, struct dv_config_file_s)

/* set ambient light */
#define DV_IOC_SET_DV_AMBIENT _IOW((DV_M), 0xa, struct ambient_cfg_s)

/*1: disable dv GD, 0: restore dv GD*/
#define DV_IOC_CONFIG_DV_BL _IOW((DV_M), 0xb, int)

/*1: enable dv dark detail, 0: disable dv GD*/
#define DV_IOC_SET_DV_DARK_DETAIL _IOW((DV_M), 0xc, int)

/* set Amlogic_cfg.txt and dv_config.bin data */
#define DV_IOC_SET_DV_CONFIG_DATA _IOW((DV_M), 0xd, struct dv_config_data_s)

/* set end-user calibration cfg.txt data */
#define DV_IOC_SET_DV_USER_CFG _IOW((DV_M), 0xe, struct dv_user_cfg_s)

/*set light sense flag(1:enable 0:disable), t_frontLux*/
#define DV_IOC_SET_DV_LIGHT_SENSE _IOW((DV_M), 0xf, struct light_sensor_s)

#endif

