/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __AML_ISP_H__
#define __AML_ISP_H__

#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <linux/delay.h>

#include "aml_common.h"
#include "aml_misc.h"

enum {
	AML_ISP_PAD_SINK_VIDEO,
	AML_ISP_PAD_SINK_PATTERN,
	AML_ISP_PAD_SINK_DDR,
	AML_ISP_PAD_SINK_PARAM,
	AML_ISP_PAD_SOURCE_STATS,
	AML_ISP_PAD_SOURCE_STREAM_0,
	AML_ISP_PAD_SOURCE_STREAM_1,
	AML_ISP_PAD_SOURCE_STREAM_2,
	AML_ISP_PAD_SOURCE_STREAM_3,
	AML_ISP_PAD_SOURCE_STREAM_RAW,
	AML_ISP_PAD_MAX
};

enum {
	AML_ISP_STREAM_DDR,
	AML_ISP_STREAM_PARAM,
	AML_ISP_STREAM_STATS,
	AML_ISP_STREAM_0,
	AML_ISP_STREAM_1,
	AML_ISP_STREAM_2,
	AML_ISP_STREAM_3,
	AML_ISP_STREAM_RAW,
	AML_ISP_STREAM_MAX
};

enum
{
	AML_MIRROR_NONE,
	AML_MIRROR_HORIZONTAL,
	AML_MIRROR_VERTICAL,
	AML_MIRROR_BOTH
};

enum {
	AML_ISP_RAW_RGGB = 0,
	AML_ISP_RAW_GRBG,
	AML_ISP_RAW_GBRG,
	AML_ISP_RAW_BGGR,
};

enum {
	AML_ISP_SCAM = 0,
	AML_ISP_MCAM,
};

enum {
	LUT_WEND = 0,
	LUT_WSTART,
};

enum {
	AE_WEIGHT_LUT_CFG = 0,
	AWB_WEIGHT_LUT_CFG,
	FED_LUT_CFG,
	LSWB_EOTF_LUT_CFG,
	LSWB_RAD_LUT_CFG,
	LSWB_MESH_LUT_CFG,
	LSWB_MESH_CRT_LUT_CFG,
	SNR_LUT_CFG,
	TNR_LUT_CFG,
	OFE_FPNR_LUT_CFG,
	OFE_DECMPR_LUT_CFG,
	GTM_LUT_CFG,
	DISP_HLUMA_LUT_CFG,
	DISP_VLUMA_LUT_CFG,
	DISP_HCHROMA_LUT_CFG,
	DISP_VCHROMA_LUT_CFG,
	CAC_TAB_LUT_CFG,
	PST_PG2_CTRST_LUT_CFG,
	PST_PG2_CTRST_LC_LUT_CFG,
	PST_PG2_CTRST_LC_ENHC_LUT_CFG,
	PST_PG2_CTRST_DHZ_LUT_CFG,
	PST_PG2_CTRST_DHZ_ENHC_LUT_CFG,
	LTM_LUT_CFG,
	LTM_ENHC_LUT_CFG,
	PST_GAMMA_LUT_CFG,
	PST_CM2_LUT_CFG,
	PST_TNR_LUT_CFG,
	PK_CNR_LUT_CFG,
	MAX_LUT_CFG,
};

struct aml_isp_lut {
	u32 lutSeq;
	u32 lutRegW[MAX_LUT_CFG];
	u32 lutRegAddr[MAX_LUT_CFG];
};

struct isp_global_info {
	u32 mode;
	u32 status;
	u32 user;
	struct aml_buffer rreg_buff;

	struct isp_dev_t *isp_dev;
};

struct isp_dev_t {
	u32 index;
	u32 irq;
	spinlock_t irq_lock;
	struct tasklet_struct irq_tasklet;
	void *emb_dev;
	void __iomem *base;
	u32 phy_base;
	u32 apb_dma;
	u32 slice;
	u32 mcnr_en;
	u32 tnr_bits;
	struct clk *isp_clk;
	struct device *dev;
	struct platform_device *pdev;

	struct aml_format fmt;
	struct aml_format lfmt;
	struct aml_format rfmt;
	u32 wreg_cnt;
	u32 fwreg_cnt;
	u32 twreg_cnt;
	u32 frm_cnt;
	u32 irq_status;
	u32 isp_status;
	u32 enWDRMode;
	struct aml_isp_lut lutWr;

	char *bus_info;
	struct media_pipeline pipe;
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev sd;
	struct media_pad pads[AML_ISP_PAD_MAX];
	struct v4l2_mbus_framefmt pfmt[AML_ISP_PAD_MAX];
	u32 fmt_cnt;
	const struct aml_format *formats;
	struct aml_subdev subdev;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *wdr;

	spinlock_t wreg_lock;
	struct aml_buffer rreg_buff;
	struct aml_buffer wreg_buff;
	struct aml_buffer ptnr_buff;
	struct aml_buffer mcnr_buff;

	const struct isp_dev_ops *ops;
	const struct emb_ops_t *emb_ops;
    struct aml_slice aslice[3];
	struct aml_video video[AML_ISP_STREAM_MAX];
};

struct isp_dev_ops {
	void (*hw_init)(struct isp_dev_t *isp_dev);
	void (*hw_reset)(struct isp_dev_t *isp_dev);
	u32 (*hw_version)(struct isp_dev_t *isp_dev);
	u32 (*hw_interrupt_status)(struct isp_dev_t *isp_dev);
	int (*hw_set_input_fmt)(struct isp_dev_t *isp_dev, struct aml_format *fmt);
	int (*hw_set_slice_fmt)(struct isp_dev_t *isp_dev, struct aml_format *fmt);
	int (*hw_cfg_slice)(struct isp_dev_t *isp_dev, int pos);
	int (*hw_set_wdr_mode)(struct isp_dev_t *isp_dev, int wdr_en);
	int (*hw_cfg_pattern)(struct isp_dev_t *isp_dev, struct aml_format *fmt);
	int (*hw_stream_set_fmt)(struct aml_video *video, struct aml_format *fmt);
	int (*hw_stream_cfg_buf)(struct aml_video *video, struct aml_buffer *buff);
	int (*hw_stream_bilateral_cfg)(struct aml_video *video, struct aml_buffer *buff);
	void (*hw_stream_crop)(struct aml_video *video);
	void (*hw_stream_on)(struct aml_video *video);
	void (*hw_stream_off)(struct aml_video *video);
	void (*hw_start)(struct isp_dev_t *isp_dev);
	void (*hw_stop)(struct isp_dev_t *isp_dev);
	int (*hw_enable_wrmif)(struct aml_video *video, int enable, int force);
	int (*hw_get_wrmif_stat)(struct aml_video *video);
	int (*hw_set_rot)(struct aml_video *video, int enable);
	u64 (*hw_timestamp)(struct isp_dev_t *isp_dev);
	int (*hw_cfg_ptnr_mif_buf)(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
	int (*hw_enable_ptnr_mif)(struct isp_dev_t *isp_dev, u32 enable);
	int (*hw_cfg_mcnr_mif_buf)(struct isp_dev_t *isp_dev, struct aml_format *fmt, struct aml_buffer *buff);
	int (*hw_enable_mcnr_mif)(struct isp_dev_t *isp_dev, int enable);
	int (*hw_start_apb_dma)(struct isp_dev_t *isp_dev);
	int (*hw_stop_apb_dma)(struct isp_dev_t *isp_dev);
	int (*hw_check_done_apb_dma)(struct isp_dev_t *isp_dev);
	int (*hw_manual_trigger_apb_dma)(struct isp_dev_t *isp_dev);
	int (*hw_auto_trigger_apb_dma)(struct isp_dev_t *isp_dev);
	int (*hw_fill_rreg_buff)(struct isp_dev_t *isp_dev);
	int (*hw_fill_gisp_rreg_buff)(struct isp_global_info *g_isp_info);
};

int isp_subdev_resume(struct isp_dev_t *isp_dev);
void isp_subdev_suspend(struct isp_dev_t *isp_dev);
struct isp_dev_t *isp_subdrv_get_dev(int index);

int aml_isp_subdev_init(void *c_dev);
void aml_isp_subdev_deinit(void *c_dev);

int aml_isp_video_register(struct isp_dev_t *isp_dev);
void aml_isp_video_unregister(struct isp_dev_t *isp_dev);

int aml_isp_subdev_register(struct isp_dev_t *isp_dev);
void aml_isp_subdev_unregister(struct isp_dev_t *isp_dev);

struct isp_global_info *isp_global_get_info(void);
int isp_global_manual_apb_dma(int vdev);
int isp_global_reset(struct isp_dev_t *isp_dev);
void isp_global_stream_on(void);
void isp_global_stream_off(void);
int isp_global_init(struct isp_dev_t *isp_dev);
int isp_global_deinit(struct isp_dev_t *isp_dev);

extern const struct isp_dev_ops isp_hw_ops;
extern const struct emb_ops_t emb_hw_ops;

#endif	/* __AML_P1_ISP_H__ */
