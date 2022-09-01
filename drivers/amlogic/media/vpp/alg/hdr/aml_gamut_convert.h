/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_GAMUT_CONVERT_H
#define _AML_GAMUT_CONVERT_H

extern uint force_primary;
extern u32 force_src_primary[8];
extern u32 force_dst_primary[8];

enum gamut_hdr_type_e {
	EN_GAMUT_HDRTYPE_NONE,
	EN_GAMUT_HDRTYPE_SDR,
	EN_GAMUT_HDRTYPE_HDR10,
	EN_GAMUT_HDRTYPE_HLG,
	EN_GAMUT_HDRTYPE_HDR10PLUS,
	EN_GAMUT_HDRTYPE_DOVI,
	EN_GAMUT_HDRTYPE_MVC,
	EN_GAMUT_HDRTYPE_PRIMESL,
};

/* matrix registers */
struct gamut_mtrx_s {
	u16 pre_offset[3];
	u16 matrix[3][3];
	u16 offset[3];
	u16 right_shift;
};

int gamut_convert_proc(struct vinfo_s *vinfo,
	enum gamut_hdr_type_e hdr_type,
	struct gamut_mtrx_s *mtx,
	int mtx_depth,
	unsigned int primary_policy);

#endif

