/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/set_hdr2_v0.h
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

#include "../amcsc.h"

#ifndef GAMUT_H
#define GAMUT_H

int gamut_convert_process(struct vinfo_s *vinfo,
			  enum hdr_type_e *source_type,
			  enum vd_path_e vd_path,
			  struct matrix_s *mtx,
			  int mtx_depth);
#endif
