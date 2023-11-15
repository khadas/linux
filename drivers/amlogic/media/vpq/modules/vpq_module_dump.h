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
extern int satbyy_base_curve[SATBYY_CURVE_LENGTH * SATBYY_CURVE_NUM];
extern int lumabyhue_base_curve[LUMABYHUE_CURVE_LENGTH * LUMABYHUE_CURVE_NUM];
extern int satbyhs_base_curve[SATBYHS_CURVE_LENGTH * SATBYHS_CURVE_NUM];
extern int huebyhue_base_curve[HUEBYHUE_CURVE_LENGTH * HUEBYHUE_CURVE_NUM];
extern int huebyhy_base_curve[HUEBYHY_CURVE_LENGTH * HUEBYHY_CURVE_NUM];
extern int huebyhs_base_curve[HUEBYHS_CURVE_LENGTH * HUEBYHS_CURVE_NUM];
extern int satbyhy_base_curve[SATBYHY_CURVE_LENGTH * SATBYHY_CURVE_NUM];

extern int satbyhs_curve[SATBYHS_CURVE_LENGTH * SATBYHS_CURVE_NUM];
extern int huebyhue_curve[HUEBYHUE_CURVE_LENGTH * HUEBYHUE_CURVE_NUM];
extern int lumabyhue_curve[LUMABYHUE_CURVE_LENGTH * LUMABYHUE_CURVE_NUM];

//tmo
extern struct vpp_tmo_param_s tmo_param;

//aipq
extern struct vpp_aipq_table_s aipq_table;

//aisr
extern struct vpp_aisr_param_s aisr_parm;
extern struct vpp_aisr_nn_param_s aisr_nn_parm;

//chroma coring
extern bool ccoring_en;
extern unsigned int *pccoring_data;

int vpq_module_dump_pq_table(enum vpq_dump_type_e value);

#endif //__VPQ_MODULE_DUMP_H__

