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

#ifndef __AML_P1_MISC_H__
#define __AML_P1_MISC_H__

#ifdef ANDROID_OS
#define VIDEO_NODE    60
#else
#define VIDEO_NODE    -1
#endif

#define V4L2_CID_AML_BASE            (V4L2_CID_BASE + 0x1000)
#define V4L2_CID_AML_ORIG_FPS        (V4L2_CID_AML_BASE + 0x000)
#define V4L2_CID_AML_USER_FPS        (V4L2_CID_AML_BASE + 0x001)
#define V4L2_CID_AML_ROLE            (V4L2_CID_AML_BASE + 0x002)
#define V4L2_CID_AML_STROBE          (V4L2_CID_AML_BASE + 0x003)
#define V4L2_CID_AML_MODE            (V4L2_CID_AML_BASE + 0x004)

struct emb_ops_t {
	void (*emb_cfg_buf)(void *edev, u32 eaddr);
	void (*emb_cfg_size)(void *edev, u32 esize);
	void (*emb_start)(void *edev);
	void (*emb_stop)(void *edev);
};

#endif
