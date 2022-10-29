/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DSC_H__
#define __DSC_H__

#include <linux/hdmi.h>

#define DSC_EVENT_ON_MODE	BIT(0)
#define DSC_EVENT_OFF_MODE	BIT(1)

#define RC_BUF_THRESH_NUM		14
#define RC_RANGE_PARAMETERS_NUM		15

struct dsc_rc_range_parameters {
	u8 range_min_qp;
	u8 range_max_qp;
	signed char range_bpg_offset; /* only 6 bit signal variable */
};

struct dsc_rc_parameter_set {
	unsigned int rc_model_size;
	u8 rc_edge_factor;
	u8 rc_quant_incr_limit0;
	u8 rc_quant_incr_limit1;
	u8 rc_tgt_offset_hi;
	u8 rc_tgt_offset_lo;
	u8 rc_buf_thresh[RC_BUF_THRESH_NUM]; /* config value need note >> 6 */
	struct dsc_rc_range_parameters rc_range_parameters[RC_RANGE_PARAMETERS_NUM];
};

struct dsc_pps_data_s {
	u8 dsc_version_major;
	u8 dsc_version_minor;
	u8 pps_identifier;
	u8 bits_per_component;
	u8 line_buf_depth;
	u8 block_pred_enable;
	u8 convert_rgb;
	u8 simple_422;
	u8 vbr_enable;
	unsigned int bits_per_pixel;
	unsigned int pic_height;
	unsigned int pic_width;
	unsigned int slice_height;
	unsigned int slice_width;
	unsigned int chunk_size;
	unsigned int initial_xmit_delay;
	unsigned int initial_dec_delay;
	u8 initial_scale_value;
	unsigned int scale_increment_interval;
	unsigned int scale_decrement_interval;
	u8 first_line_bpg_offset;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;
	u8 flatness_min_qp;
	u8 flatness_max_qp;
	struct dsc_rc_parameter_set rc_parameter_set;
	u8 native_420;
	u8 native_422;
	u8 second_line_bpg_offset;
	unsigned int nsl_bpg_offset;
	unsigned int second_line_offset_adj;
	unsigned int hc_active_bytes;
};

struct dsc_notifier_data_s {
	unsigned int pic_width;
	unsigned int pic_height;
	unsigned int fps;
	enum hdmi_colorspace color_format;
	struct dsc_pps_data_s pps_data;
};

int aml_set_dsc_mode(bool on_off, enum hdmi_colorspace color_space);
void hdmitx_get_pps_data(struct dsc_notifier_data_s *notifier_data);

//#ifdef CONFIG_AMLOGIC_MEDIA_VRR
/* atomic notify */
int aml_dsc_atomic_notifier_register(struct notifier_block *nb);
int aml_dsc_atomic_notifier_unregister(struct notifier_block *nb);
int aml_dsc_atomic_notifier_call_chain(unsigned long event, void *v);
//#else
//static inline int aml_dsc_atomic_notifier_register(struct notifier_block *nb)
//{
//	return 0;
//}
//
//static inline int aml_dsc_atomic_notifier_unregister(struct notifier_block *nb)
//{
//	return 0;
//}
//
//static inline int aml_dsc_atomic_notifier_call_chain(unsigned long event,
//						     void *v)
//{
//	return 0;
//}
//#endif

#endif
