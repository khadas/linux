/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
#ifndef _VICP_H_
#define _VICP_H_

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/completion.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/cpu_version.h>

#define MAX_PLANE_MUM		4
#define RDMA_CMD_BUF_LEN	1024
#define RDMA_LOAD_BUF_LEN	(1024 * 7)
#define MAX_INPUTSOURCE_COUNT	9

/* *********************************************************************** */
/* ************************* enum definitions ****************************.*/
/* *********************************************************************** */
enum vicp_rotation_mode_e {
	VICP_ROTATION_0 = 0,
	VICP_ROTATION_90,
	VICP_ROTATION_180,
	VICP_ROTATION_270,
	VICP_ROTATION_MIRROR_V,
	VICP_ROTATION_MIRROR_H,
	VICP_ROTATION_MAX,
};

enum vicp_color_format_e {
	VICP_COLOR_FORMAT_YUV444 = 0,
	VICP_COLOR_FORMAT_YUV422,
	VICP_COLOR_FORMAT_YUV420,
	VICP_COLOR_FORMAT_MAX,
};

enum vicp_shrink_mode_e {
	VICP_SHRINK_MODE_2X = 0,
	VICP_SHRINK_MODE_4X,
	VICP_SHRINK_MODE_8X,
	VICP_SHRINK_MODE_MAX,
};

enum vicp_skip_mode_e {
	VICP_SKIP_MODE_OFF = 0,
	VICP_SKIP_MODE_HORZ,
	VICP_SKIP_MODE_VERT,
	VICP_SKIP_MODE_ALL,
	VICP_SKIP_MODE_MAX,
};

/* *********************************************************************** */
/* ************************* struct definitions **************************.*/
/* *********************************************************************** */
struct output_axis_t {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct crop_info_t {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct data_option_t {
	struct crop_info_t crop_info;
	enum vicp_rotation_mode_e rotation_mode;
	struct output_axis_t output_axis;
	enum vicp_shrink_mode_e shrink_mode;
	enum vicp_skip_mode_e skip_mode;
	bool rdma_enable;
	u32 input_source_count;
	u32 input_source_number;
	bool security_enable;
};

struct dma_data_config_t {
	u32 buf_addr;
	u32 buf_stride;
	u32 data_width;
	u32 data_height;
	u32 plane_count;
	enum vicp_color_format_e color_format;
	u32 color_depth;
};

struct input_data_param_t {
	bool is_vframe;
	struct vframe_s *data_vf;
	struct dma_data_config_t *data_dma;
};

struct output_data_param_t {
	ulong phy_addr[MAX_PLANE_MUM];
	u32 stride[MAX_PLANE_MUM];
	u32 width;
	u32 height;
	u32 fbc_out_en;
	enum vicp_color_format_e fbc_color_fmt;
	u32 fbc_color_dep;
	u32 fbc_init_ctrl;
	u32 fbc_pip_mode;
	u32 mif_out_en;
	enum vicp_color_format_e mif_color_fmt;
	u32 mif_color_dep;
};

struct vicp_data_config_t {
	struct input_data_param_t input_data;
	struct output_data_param_t output_data;
	struct data_option_t data_option;
};

/* *********************************************************************** */
/* ************************* function definitions **************************.*/
/* *********************************************************************** */
int  vicp_process(struct vicp_data_config_t *data_config);

#endif
