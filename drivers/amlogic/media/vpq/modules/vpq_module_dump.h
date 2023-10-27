/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_MODULE_DUMP_H__
#define __VPQ_MODULE_DUMP_H__

#include <linux/amlogic/media/vpp/vpp_common_def.h>
#include "../vpq_common.h"

//picture mode
extern struct vpq_pict_mode_item_data_s pic_mode_item;

//gamma & wb
extern struct vpq_rgb_ogo_s rgb_ogo;
extern struct vpp_gamma_table_s vpp_gma_table;

//dnlp
extern bool dnlp_enable;
extern struct vpp_dnlp_curve_param_s *pdnlp_data;

//lc
extern bool lc_enable;
extern struct vpp_lc_curve_s lc_curve;
extern struct vpp_lc_param_s lc_param;

//black ext & blue stretch
extern int *pble_data;
extern bool blkext_en;
extern bool blue_str_en;
extern int *pbls_data;

//color base
extern struct pq_cm_base_parm_s *pcm_base_data;

//aipq
extern struct vpp_aipq_table_s aipq_table;

//aisr
extern struct vpp_aisr_param_s aisr_parm;
extern struct vpp_aisr_nn_param_s aisr_nn_parm;

int vpq_module_dump_pq_table(enum vpq_dump_type_e value);

#endif //__VPQ_MODULE_DUMP_H__

