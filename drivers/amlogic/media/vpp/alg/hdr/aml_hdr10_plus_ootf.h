/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_HDR10_PLUS_OOTF_H
#define _AML_HDR10_PLUS_OOTF_H

#define N 15
#define PERCENTILE_ORDER 10
#define NUM_P (N - 1)
#define POINTS 149
#define OOLUT_NUM 149
#define PROCESSING_MAX 12
#define PROCESSING_DATA_MAX ((1 << PROCESSING_MAX) - 1)
#define PROCESSING_MAX_HALF ((1 << (PROCESSING_MAX - 1)) - 1)
#define GAIN_BIT 6
#define U16 16
#define U16_MAXI ((1 << U16) - 1)
#define MIN_LUMINANCE 200

#define U32 32
#define _U32_MAX 0xffffffff

struct ebzcurveparameters {
	int order;
	int sx, sy;
	int anchor[N + 1];
};

struct percentiles {
	int num_percentile;
	int percentilepercent[PERCENTILE_ORDER];
	int percentilevalue[PERCENTILE_ORDER];
};

struct scene2094metadata {
	int maxscenesourceluminance;
	int referenceluminance;
	int minluminance;
	struct percentiles percentiles;
	struct ebzcurveparameters ebzcurveparameters;
};

struct hdr10_plus_sei_s {
	int targeted_system_display_maximum_luminance;
	int maxscl[3][3];
	int average_maxrgb[3];
	int num_distributions[3];
	int distribution_index[3][9];
	int distribution_values[3][9];

	int tone_mapping_flag[3];
	int knee_point_x[3];
	int knee_point_y[3];
	int num_bezier_curve_anchors[3];
	int bezier_curve_anchors[3][15];
	int color_saturation_mapping_flag[3];
};

#define ORDER 10
#define NPCOEFF (ORDER - 1)
#define P1MIN (PROCESSING_DATA_MAX / ORDER)

struct basisootf_params {
	/*Knee-Point (KP) parameters*/
	/*KP ramp base thresholds (two bounds KP 1 and KP 2 are computed)*/
	int SY1_V1;
	int SY1_V2;
	int SY1_T1;
	int SY1_T2;

	int SY2_V1;
	int SY2_V2;
	int SY2_T1;
	int SY2_T2;

	/*KP mixing gain (final KP from bounds KP#1*/
	/*and KP#2 as a function of scene percentile)*/
	int KP_G_V1;
	int KP_G_V2;
	int KP_G_T1;
	int KP_G_T2;

	/* P coefficient parameters*/
	/* Thresholds of minimum bound of P1 coefficient*/
	int P1_LIMIT_V1;
	int P1_LIMIT_V2;
	int P1_LIMIT_T1;
	int P1_LIMIT_T2;

	/* Thresholds to compute relative shape of curve (P2~P9 coefficient)*/
	/* by pre-defined bounds - as a function of scene percentile*/
	int P2TO9_T1;
	int P2TO9_T2;

	/* Defined relative shape bounds (P2~P9 coefficient) for*/
	/*a given maximum TM dynamic compression (eg : 20x )*/
	int P2TOP9_MAX1[ORDER - 2];
	int P2TOP9_MAX2[ORDER - 2];

	/* Ps mixing gain (obtain all Ps coefficients) -*/
	/*as a function of TM dynamic compression ratio*/
	int PS_G_T1;
	int PS_G_T2;

	/* Post-processing : Reduce P1/P2 (to enhance mid tone)*/
	/*for high TM dynamic range compression cases*/
	int LOW_SY_T1;
	int LOW_SY_T2;
	int LOW_K_T1;
	int LOW_K_T2;
	int RED_P1_V1;
	int RED_P1_T1;
	int RED_P1_T2;
	int RED_P2_V1;
	int RED_P2_T1;
	int RED_P2_T2;
};

struct hdr10pgen_param_s {
	unsigned int shift;
	/* scale * 1000 for 3bit float point*/
	unsigned int scale_gmt;
	unsigned int gain[POINTS];
};

void hdr10p_basisootf_params_init(struct basisootf_params *basisootf_params);
int hdr10p_calcp1(int sx, int sy, int tgtl, int calcmaxl,
	struct basisootf_params *basisootf_params,
	int *p1_red_gain);
int hdr10p_ootf_gen(int panel_lumin,
	int force_source_lumin,
	struct hdr10pgen_param_s *hdr10pgen_param,
	u32 *percentile);

#endif

