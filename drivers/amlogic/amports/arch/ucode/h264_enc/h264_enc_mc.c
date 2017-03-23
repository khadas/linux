/*
 * drivers/amlogic/amports/arch/ucode/h264_enc/h264_enc_mc.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include "../../firmware.h"

#undef MicroCode
#define MicroCode h264_enc_mc_gx
#include "h264_enc_gx.h"

#undef MicroCode
#define MicroCode h264_enc_mc_gxtv
#include "h264_enc_gxtv.h"

#undef MicroCode
#define MicroCode h264_enc_mc_gxl
#include "h264_enc_gxl.h"

#undef MicroCode
#define MicroCode h264_enc_mc_txl
#include "h264_enc_txl.h"


#define CODEC_VERSION "0.0.0.1"
#define FOR_VFORMAT VFORMAT_H264_ENC

#define DEF_FIRMEARE_FOR_GXBB(n) \
	REGISTER_FIRMARE_PER_CPU_VER(MESON_CPU_MAJOR_ID_GXBB, \
				FOR_VFORMAT, n, CODEC_VERSION)

#define DEF_FIRMEARE_FOR_GXTVBB(n) \
	REGISTER_FIRMARE_PER_CPU_VER(MESON_CPU_MAJOR_ID_GXTVBB, \
				FOR_VFORMAT, n, CODEC_VERSION)

#define DEF_FIRMEARE_FOR_GXL(n) \
	REGISTER_FIRMARE_PER_CPU_VER(MESON_CPU_MAJOR_ID_GXL, \
				FOR_VFORMAT, n, CODEC_VERSION)

#define DEF_FIRMEARE_FOR_GXM(n) \
	REGISTER_FIRMARE_PER_CPU_VER(MESON_CPU_MAJOR_ID_GXM, \
				FOR_VFORMAT, n, CODEC_VERSION)

#define DEF_FIRMEARE_FOR_TXL(n) \
	REGISTER_FIRMARE_PER_CPU_VER(MESON_CPU_MAJOR_ID_TXL, \
				FOR_VFORMAT, n, CODEC_VERSION)

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMEARE_FOR_GXBB(h264_enc_mc_gx);\
		DEF_FIRMEARE_FOR_GXTVBB(h264_enc_mc_gxtv);\
		DEF_FIRMEARE_FOR_GXL(h264_enc_mc_gxl);\
		DEF_FIRMEARE_FOR_GXM(h264_enc_mc_gxl);\
		DEF_FIRMEARE_FOR_TXL(h264_enc_mc_txl);\
	} while (0)

INIT_DEF_FIRMWARE();
