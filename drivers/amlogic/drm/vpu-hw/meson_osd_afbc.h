/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_AFBC_H_
#define _MESON_OSD_AFBC_H_

#include "meson_vpu_pipeline.h"

/*VPU_MAFBC_FORMAT_SPECIFIER_S0 define as follow*/
#define AFBC_EN			BIT(31)
#define TILED_HEADER_EN		BIT(18)
#define SUPER_BLOCK_ASPECT	BIT(16)
#define BLOCK_SPLIT		BIT(9)
#define YUV_TRANSFORM		BIT(8)

/* osd afbc control*/
#define MALI_AFBCD_TOP_CTRL 0x1a0f

/* osd afbc on g12a */
#define VPU_MAFBC_BLOCK_ID 0x3a00
#define VPU_MAFBC_IRQ_RAW_STATUS 0x3a01
#define VPU_MAFBC_IRQ_CLEAR 0x3a02
#define VPU_MAFBC_IRQ_MASK 0x3a03
#define VPU_MAFBC_IRQ_STATUS 0x3a04
#define VPU_MAFBC_COMMAND 0x3a05
#define VPU_MAFBC_STATUS 0x3a06
#define VPU_MAFBC_SURFACE_CFG 0x3a07

/* osd afbc on g12a */
#define VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0 0x3a10
#define VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S0 0x3a11
#define VPU_MAFBC_FORMAT_SPECIFIER_S0 0x3a12
#define VPU_MAFBC_BUFFER_WIDTH_S0 0x3a13
#define VPU_MAFBC_BUFFER_HEIGHT_S0 0x3a14
#define VPU_MAFBC_BOUNDING_BOX_X_START_S0 0x3a15
#define VPU_MAFBC_BOUNDING_BOX_X_END_S0 0x3a16
#define VPU_MAFBC_BOUNDING_BOX_Y_START_S0 0x3a17
#define VPU_MAFBC_BOUNDING_BOX_Y_END_S0 0x3a18
#define VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S0 0x3a19
#define VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S0 0x3a1a
#define VPU_MAFBC_OUTPUT_BUF_STRIDE_S0 0x3a1b
#define VPU_MAFBC_PREFETCH_CFG_S0 0x3a1c

#define VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1 0x3a30
#define VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S1 0x3a31
#define VPU_MAFBC_FORMAT_SPECIFIER_S1 0x3a32
#define VPU_MAFBC_BUFFER_WIDTH_S1 0x3a33
#define VPU_MAFBC_BUFFER_HEIGHT_S1 0x3a34
#define VPU_MAFBC_BOUNDING_BOX_X_START_S1 0x3a35
#define VPU_MAFBC_BOUNDING_BOX_X_END_S1 0x3a36
#define VPU_MAFBC_BOUNDING_BOX_Y_START_S1 0x3a37
#define VPU_MAFBC_BOUNDING_BOX_Y_END_S1 0x3a38
#define VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S1 0x3a39
#define VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S1 0x3a3a
#define VPU_MAFBC_OUTPUT_BUF_STRIDE_S1 0x3a3b
#define VPU_MAFBC_PREFETCH_CFG_S1 0x3a3c

#define VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2 0x3a50
#define VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S2 0x3a51
#define VPU_MAFBC_FORMAT_SPECIFIER_S2 0x3a52
#define VPU_MAFBC_BUFFER_WIDTH_S2 0x3a53
#define VPU_MAFBC_BUFFER_HEIGHT_S2 0x3a54
#define VPU_MAFBC_BOUNDING_BOX_X_START_S2 0x3a55
#define VPU_MAFBC_BOUNDING_BOX_X_END_S2 0x3a56
#define VPU_MAFBC_BOUNDING_BOX_Y_START_S2 0x3a57
#define VPU_MAFBC_BOUNDING_BOX_Y_END_S2 0x3a58
#define VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S2 0x3a59
#define VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S2 0x3a5a
#define VPU_MAFBC_OUTPUT_BUF_STRIDE_S2 0x3a5b
#define VPU_MAFBC_PREFETCH_CFG_S2 0x3a5c

#define VPU_MAFBC_HEADER_BUF_ADDR_LOW_S3 0x3a70
#define VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S3 0x3a71
#define VPU_MAFBC_FORMAT_SPECIFIER_S3 0x3a72
#define VPU_MAFBC_BUFFER_WIDTH_S3 0x3a73
#define VPU_MAFBC_BUFFER_HEIGHT_S3 0x3a74
#define VPU_MAFBC_BOUNDING_BOX_X_START_S3 0x3a75
#define VPU_MAFBC_BOUNDING_BOX_X_END_S3 0x3a76
#define VPU_MAFBC_BOUNDING_BOX_Y_START_S3 0x3a77
#define VPU_MAFBC_BOUNDING_BOX_Y_END_S3 0x3a78
#define VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S3 0x3a79
#define VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S3 0x3a7a
#define VPU_MAFBC_OUTPUT_BUF_STRIDE_S3 0x3a7b
#define VPU_MAFBC_PREFETCH_CFG_S3 0x3a7c

struct afbc_osd_reg_s {
	u32 vpu_mafbc_header_buf_addr_low_s;
	u32 vpu_mafbc_header_buf_addr_high_s;
	u32 vpu_mafbc_format_specifier_s;
	u32 vpu_mafbc_buffer_width_s;
	u32 vpu_mafbc_buffer_height_s;
	u32 vpu_mafbc_bounding_box_x_start_s;
	u32 vpu_mafbc_bounding_box_x_end_s;
	u32 vpu_mafbc_bounding_box_y_start_s;
	u32 vpu_mafbc_bounding_box_y_end_s;
	u32 vpu_mafbc_output_buf_addr_low_s;
	u32 vpu_mafbc_output_buf_addr_high_s;
	u32 vpu_mafbc_output_buf_stride_s;
	u32 vpu_mafbc_prefetch_cfg_s;
	u32 vpu_mafbc_payload_min_low_s;
	u32 vpu_mafbc_payload_min_high_s;
	u32 vpu_mafbc_payload_max_low_s;
	u32 vpu_mafbc_payload_max_high_s;
};

struct afbc_status_reg_s {
	u32 vpu_mafbc_block_id;
	u32 vpu_mafbc_irq_raw_status;
	u32 vpu_mafbc_irq_clear;
	u32 vpu_mafbc_irq_mask;
	u32 vpu_mafbc_irq_status;
	u32 vpu_mafbc_command;
	u32 vpu_mafbc_status;
	u32 vpu_mafbc_surface_cfg;
};

enum afbc_pix_format_e {
	RGB565 = 0,
	RGBA5551,
	RGBA1010102,
	YUV420_10B,
	RGB888,
	RGBA8888,
	RGBA4444,
	R8,
	RG88,
	YUV420_8B,
	YUV422_8B = 11,
	YUV422_10B = 14,
};

void arm_fbc_start(struct meson_vpu_pipeline_state *pipeline_state);
void arm_fbc_check_error(void);

#endif
