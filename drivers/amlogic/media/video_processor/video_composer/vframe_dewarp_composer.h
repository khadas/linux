/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VFRAME_DEWARP_COMPOSER_H
#define VFRAME_DEWARP_COMPOSER_H
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/gdc/gdc.h>
#include "vc_util.h"
#include "video_composer.h"

#define DEWARP_COM_INFO(fmt, args...)	\
	pr_info("dewarp_composer: info:" fmt "", ## args)
#define DEWARP_COM_DBG(fmt, args...)	\
	pr_debug("dewarp_composer: dbg:" fmt "", ## args)
#define DEWARP_COM_WARN(fmt, args...)	\
	pr_warn("dewarp_composer: warn:" fmt "", ## args)
#define DEWARP_COM_ERR(fmt, args...)	\
	pr_err("dewarp_composer: err:" fmt "", ## args)

struct dewarp_composer_para {
	int vc_index;
	struct gdc_context_s *context;
	struct firmware_load_s fw_load;
	struct composer_vf_para *vf_para;
};

int get_dewarp_format(struct vframe_s *vf);

bool is_dewarp_supported(struct dewarp_composer_para *param);

int init_dewarp_composer(struct dewarp_composer_para *param);

int uninit_dewarp_composer(struct dewarp_composer_para *param);

int config_dewarp_vframe(int vc_index, int rotation,
							struct vframe_s *src_vf,
							struct dst_buf_t *dst_buf,
							struct composer_vf_para *vframe_para);

int dewarp_data_composer(struct dewarp_composer_para *param);

#endif
