/*
 * drivers/amlogic/display/vout/enc_clk_val.h
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



#ifndef __ENC_CLK_VAL_H__
#define __ENC_CLK_VAL_H__

#include "enc_clk_config.h"

static struct enc_clk_val_s setting_enc_clk_val_gxl[] = {
	{VMODE_480CVBS,    2970, 1, 2, VIU_ENCI,  1, 56, 2, -1,  1, 1,},
	{VMODE_576CVBS,    2970, 1, 2, VIU_ENCI,  1, 56, 2, -1,  1, 1,},
};

static struct enc_clk_val_s setting_enc_clk_val_gxbb[] = {
	{VMODE_480CVBS,    2970, 1, 2, VIU_ENCI,  1, 56, 2, -1,  1, 1,},
	{VMODE_576CVBS,    2970, 1, 2, VIU_ENCI,  1, 56, 2, -1,  1, 1,},
};

static struct enc_clk_val_s setting_enc_clk_val_m8m2[] = {
	{VMODE_480I,       2160, 8, 1, VIU_ENCI, 5,  4, 2, -1,  2, 2,},
	{VMODE_480I_RPT,   2160, 4, 1, VIU_ENCI, 5,  4, 2, -1,  4, 2,},
	{VMODE_480CVBS,    1296, 4, 1, VIU_ENCI, 6,  4, 2, -1,  2, 2,},
	{VMODE_480P,       2160, 8, 1, VIU_ENCP, 5,  4, 2,  1, -1, 1,},
	{VMODE_480P_RPT,   2160, 2, 1, VIU_ENCP, 5,  4, 1,  2, -1, 1,},
	{VMODE_576I,       2160, 8, 1, VIU_ENCI, 5,  4, 2, -1,  2, 2,},
	{VMODE_576I_RPT,   2160, 4, 1, VIU_ENCI, 5,  4, 2, -1,  4, 2,},
	{VMODE_576CVBS,    1296, 4, 1, VIU_ENCI, 6,  4, 2, -1,  2, 2,},
	{VMODE_576P,       2160, 8, 1, VIU_ENCP, 5,  4, 2,  1, -1, 1,},
	{VMODE_576P_RPT,   2160, 2, 1, VIU_ENCP, 5,  4, 1,  2, -1, 1,},
	{VMODE_720P,       2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080I,      2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080P,      2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_1080P,      2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_720P_50HZ,  2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080I_50HZ, 2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080P_50HZ, 2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_1080P_24HZ, 2970, 4, 2, VIU_ENCP, 10, 2, 1,  1, -1, 1,},
	{VMODE_4K2K_30HZ,  2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
	{VMODE_4K2K_25HZ,  2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
	{VMODE_4K2K_24HZ,  2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
	{VMODE_4K2K_SMPTE, 2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
};

static struct enc_clk_val_s setting_enc_clk_val_m8b[] = {
	{VMODE_480I,       2160, 8, 1, VIU_ENCI,  5, 4, 2, -1,  2, 2,},
	{VMODE_480CVBS,    1296, 4, 1, VIU_ENCI,  6, 4, 2, -1,  2, 2,},
	{VMODE_480P,       2160, 8, 1, VIU_ENCP,  5, 4, 2,  1, -1, 1,},
	{VMODE_576I,       2160, 8, 1, VIU_ENCI,  5, 4, 2, -1,  2, 2,},
	{VMODE_576CVBS,    1296, 4, 1, VIU_ENCI,  6, 4, 2, -1,  2, 2,},
	{VMODE_576P,       2160, 8, 1, VIU_ENCP,  5, 4, 2,  1, -1, 1,},
	{VMODE_720P,       2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080I,      2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080P,      2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_1080P,      2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_720P_50HZ,  2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080I_50HZ, 2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080P_50HZ, 2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_1080P_24HZ, 2970, 4, 2, VIU_ENCP, 10, 2, 1,  1, -1, 1,},
};

static struct enc_clk_val_s setting_enc_clk_val_m8[] = {
	{VMODE_480I,       1080, 4, 1, VIU_ENCI,  5, 4, 2, -1,  2, 2,},
	{VMODE_480I_RPT,   2160, 4, 1, VIU_ENCI,  5, 4, 2, -1,  4, 2,},
	{VMODE_480CVBS,    1296, 4, 1, VIU_ENCI,  6, 4, 2, -1,  2, 2,},
	{VMODE_480P,       1080, 4, 1, VIU_ENCP,  5, 4, 2,  1, -1, 1,},
	{VMODE_480P_RPT,   2160, 2, 1, VIU_ENCP,  5, 4, 1,  2, -1, 1,},
	{VMODE_576I,       1080, 4, 1, VIU_ENCI,  5, 4, 2, -1,  2, 2,},
	{VMODE_576I_RPT,   2160, 4, 1, VIU_ENCI,  5, 4, 2, -1,  4, 2,},
	{VMODE_576CVBS,    1296, 4, 1, VIU_ENCI,  6, 4, 2, -1,  2, 2,},
	{VMODE_576P,       1080, 4, 1, VIU_ENCP,  5, 4, 2,  1, -1, 1,},
	{VMODE_576P_RPT,   2160, 2, 1, VIU_ENCP,  5, 4, 1,  2, -1, 1,},
	{VMODE_720P,       2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080I,      2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080P,      2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_1080P,      2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_720P_50HZ,  2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080I_50HZ, 2970, 4, 2, VIU_ENCP, 10, 1, 2,  1, -1, 1,},
	{VMODE_1080P_50HZ, 2970, 2, 2, VIU_ENCP, 10, 1, 1,  1, -1, 1,},
	{VMODE_1080P_24HZ, 2970, 4, 2, VIU_ENCP, 10, 2, 1,  1, -1, 1,},
	{VMODE_4K2K_30HZ,  2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
	{VMODE_4K2K_25HZ,  2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
	{VMODE_4K2K_24HZ,  2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
	{VMODE_4K2K_SMPTE, 2971, 1, 2, VIU_ENCP,  5, 1, 1,  1, -1, 1,},
};

static struct enc_clk_val_s setting_enc_clk_val_m6[] = {
	{VMODE_480I,       1080,  4, 1, VIU_ENCI,  5,  4,  2, -1,  2, 2,  -1},
	{VMODE_480CVBS,    1080,  4, 1, VIU_ENCI,  5,  4,  2, -1,  2, 2,  -1},
	{VMODE_480P,       1080,  4, 1, VIU_ENCP,  5,  4,  2,  1, -1, 1,  -1},
	{VMODE_576I,       1080,  4, 1, VIU_ENCI,  5,  4,  2, -1,  2, 2,  -1},
	{VMODE_576CVBS,    1080,  4, 1, VIU_ENCI,  5,  4,  2, -1,  2, 2,  -1},
	{VMODE_576P,       1080,  4, 1, VIU_ENCP,  5,  4,  2,  1, -1, 1,  -1},
	{VMODE_720P,       1488,  2, 1, VIU_ENCP, 10,  1,  2,  1, -1, 1,  -1},
	{VMODE_1080I,      1488,  2, 1, VIU_ENCP, 10,  1,  2,  1, -1, 1,  -1},
	{VMODE_1080P,      1488,  1, 1, VIU_ENCP, 10,  1,  1,  1, -1, 1,  -1},
	{VMODE_1080P,      1488,  1, 1, VIU_ENCP, 10,  1,  1,  1, -1, 1,  -1},
	{VMODE_720P_50HZ,  1488,  2, 1, VIU_ENCP, 10,  1,  2,  1, -1, 1,  -1},
	{VMODE_1080I_50HZ, 1488,  2, 1, VIU_ENCP, 10,  1,  2,  1, -1, 1,  -1},
	{VMODE_1080P_50HZ, 1488,  1, 1, VIU_ENCP, 10,  1,  1,  1, -1, 1,  -1},
	{VMODE_1080P_24HZ, 1488,  2, 1, VIU_ENCP, 10,  2,  1,  1, -1, 1,  -1},
	{VMODE_4K2K_30HZ,  2970,  1, 2, VIU_ENCP,  5,  1,  1,  1, -1, 1,  -1},
	{VMODE_4K2K_25HZ,  2970,  1, 2, VIU_ENCP,  5,  1,  1,  1, -1, 1,  -1},
	{VMODE_4K2K_24HZ,  2970,  1, 2, VIU_ENCP,  5,  1,  1,  1, -1, 1,  -1},
	{VMODE_4K2K_SMPTE, 2970,  1, 2, VIU_ENCP,  5,  1,  1,  1, -1, 1,  -1},
	{VMODE_VGA,          -1, -1, 1, VIU_ENCP, -1, -1, -1,  1, -1, 1,   1},
	{VMODE_SVGA,         -1, -1, 1, VIU_ENCP, -1, -1, -1,  1, -1, 1,   1},
	{VMODE_XGA,          -1, -1, 1, VIU_ENCP, -1, -1, -1,  1, -1, 1,   1},
	{VMODE_SXGA,         -1, -1, 1, VIU_ENCP, -1, -1, -1,  1, -1, 1,   1},
	{VMODE_WSXGA,        -1, -1, 1, VIU_ENCP, -1, -1, -1,  1, -1, 1,   1},
	{VMODE_FHDVGA,       -1, -1, 1, VIU_ENCP, -1, -1, -1,  1, -1, 1,   1},
};

#endif
