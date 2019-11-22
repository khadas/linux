/*
 * drivers/amlogic/media/enhancement/amvecm/amcsc_pip.h
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

#ifndef AM_CSC_PIP_H
#define AM_CSC_PIP_H
extern int hdr_policy_process(
	struct vinfo_s *vinfo,
	enum hdr_type_e *source_format,
	enum vd_path_e vd_path);
extern void hdmi_packet_process(
	int signal_change_flag,
	struct vinfo_s *vinfo,
	struct vframe_master_display_colour_s *p,
	struct hdr10plus_para *hdmitx_hdr10plus_param,
	enum vd_path_e vd_path);
extern void video_post_process(
	enum vpp_matrix_csc_e csc_type,
	struct vinfo_s *vinfo,
	enum vd_path_e vd_path);
#endif
