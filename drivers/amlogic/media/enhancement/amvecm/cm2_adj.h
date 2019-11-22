/*
 * drivers/amlogic/media/enhancement/amvecm/cm2_adj.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __CM2_ADJ__
#define __CM2_ADJ__


enum ecm2colormd {
	ecm2colormd_purple = 0,
	ecm2colormd_red,
	ecm2colormd_skin,
	ecm2colormd_yellow,
	ecm2colormd_yellow_green,
	ecm2colormd_green,
	ecm2colormd_blue_green,
	ecm2colormd_cyan,
	ecm2colormd_blue,
	ecm2colormd_max,
};

/*H00 ~ H31*/
#define CM2_ENH_COEF0_H00 0x100
#define CM2_ENH_COEF1_H00 0x101
#define CM2_ENH_COEF2_H00 0x102
#define CM2_ENH_COEF3_H00 0x103
#define CM2_ENH_COEF4_H00 0x104

#define CM2_ENH_COEF0_H01 0x108
#define CM2_ENH_COEF1_H01 0x109
#define CM2_ENH_COEF2_H01 0x10a
#define CM2_ENH_COEF3_H01 0x10b
#define CM2_ENH_COEF4_H01 0x10c

#define CM2_ENH_COEF0_H02 0x110
#define CM2_ENH_COEF1_H02 0x111
#define CM2_ENH_COEF2_H02 0x112
#define CM2_ENH_COEF3_H02 0x113
#define CM2_ENH_COEF4_H02 0x114

void cm2_curve_update_hue_by_hs(enum ecm2colormd colormode);
void cm2_curve_update_hue(enum ecm2colormd colormode);
void cm2_curve_update_luma(enum ecm2colormd colormode);
void cm2_curve_update_sat(enum ecm2colormd colormode);

void cm2_hue_by_hs(enum ecm2colormd colormode, int hue_val, int lpf_en);
void cm2_hue(enum ecm2colormd colormode, int hue_val, int lpf_en);
void cm2_luma(enum ecm2colormd colormode, int luma_val, int lpf_en);
void cm2_sat(enum ecm2colormd colormode, int sat_val, int lpf_en);

#endif

