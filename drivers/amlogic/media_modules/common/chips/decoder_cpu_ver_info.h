/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef DECODER_CPU_VER_INFO_H
#define DECODER_CPU_VER_INFO_H

enum AM_MESON_CPU_MAJOR_ID
{
	AM_MESON_CPU_MAJOR_ID_M6 = 0x16,
	AM_MESON_CPU_MAJOR_ID_M6TV = 0x17,
	AM_MESON_CPU_MAJOR_ID_M6TVL = 0x18,
	AM_MESON_CPU_MAJOR_ID_M8 = 0x19,
	AM_MESON_CPU_MAJOR_ID_MTVD = 0x1A,
	AM_MESON_CPU_MAJOR_ID_M8B =  0x1B,
	AM_MESON_CPU_MAJOR_ID_MG9TV = 0x1C,
	AM_MESON_CPU_MAJOR_ID_M8M2 = 0x1D,
	AM_MESON_CPU_MAJOR_ID_UNUSE = 0x1E,
	AM_MESON_CPU_MAJOR_ID_GXBB = 0x1F,
	AM_MESON_CPU_MAJOR_ID_GXTVBB = 0x20,
	AM_MESON_CPU_MAJOR_ID_GXL = 0x21,
	AM_MESON_CPU_MAJOR_ID_GXM = 0x22,
	AM_MESON_CPU_MAJOR_ID_TXL = 0x23,
	AM_MESON_CPU_MAJOR_ID_TXLX = 0x24,
	AM_MESON_CPU_MAJOR_ID_AXG = 0x25,
	AM_MESON_CPU_MAJOR_ID_GXLX = 0x26,
	AM_MESON_CPU_MAJOR_ID_TXHD = 0x27,
	AM_MESON_CPU_MAJOR_ID_G12A = 0x28,
	AM_MESON_CPU_MAJOR_ID_G12B = 0x29,
	AM_MESON_CPU_MAJOR_ID_GXLX2 = 0x2a,
	AM_MESON_CPU_MAJOR_ID_SM1 = 0x2b,
	AM_MESON_CPU_MAJOR_ID_TL1 = 0x2e,
	AM_MESON_CPU_MAJOR_ID_MAX,
};

enum AM_MESON_CPU_MAJOR_ID get_cpu_major_id(void);
#endif
