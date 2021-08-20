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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 * brief Describes film grain parameters and film grain synthesis
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/sched/clock.h>
#include "vav1.h"

#define FILM_GRAIN_REG_SIZE  39

#define MID(a, min, max) ((a > max) ? max : ((a > min) ? a : min))

#define gauss_bits 11

#define luma_subblock_size_y 32
#define luma_subblock_size_x 32

#define left_pad 3
#define right_pad 3	// padding to offset for AR coefficients
#define top_pad 3
#define bottom_pad 0
#define ar_padding 3 // maximum lag used for stabilization of AR coefficients

#define luma_grain_block_offt 0x120 //18 * 16
#define cb_grain_block_offt 0x1520  //(18 + 320) * 16

struct aom_film_grain_t {
	u8 apply_grain;

	u8 update_parameters;

	// 8 bit values
	s32 scaling_points_y[14][2];
	u8 num_y_points;  // value: 0..14

	// 8 bit values
	s32 scaling_points_cb[10][2];
	u8 num_cb_points;  // value: 0..10

	// 8 bit values
	s32 scaling_points_cr[10][2];
	u8 num_cr_points;  // value: 0..10

	u8 scaling_shift;  // values : 8..11

	s32 ar_coeff_lag;  // values:  0..3

	// 8 bit values
	s32 ar_coeffs_y[24];
	s32 ar_coeffs_cb[25];
	s32 ar_coeffs_cr[25];

	// Shift value: AR coeffs range
	// 6: [-2, 2)
	// 7: [-1, 1)
	// 8: [-0.5, 0.5)
	// 9: [-0.25, 0.25)
	u8 ar_coeff_shift;  // values : 6..9

	s8 cb_mult;       // 8 bits
	s8 cb_luma_mult;  // 8 bits
	s16 cb_offset;     // 9 bits

	s8 cr_mult;       // 8 bits
	s8 cr_luma_mult;  // 8 bits
	s16 cr_offset;     // 9 bits

	u8 overlap_flag;

	u8 clip_to_restricted_range;

	u8 bit_depth;  // video bit depth

	u8 mc_identity;

	u8 chroma_scaling_from_luma;

	u8 grain_scale_shift;

	u16 random_seed;

	u16 random_register_y;
	u16 random_register_cb;
	u16 random_register_cr;

	s32 scaling_delta_y[14];
	s32 scaling_delta_cb[10];
	s32 scaling_delta_cr[10];
};


static u32 debug_fgs;
module_param(debug_fgs, uint, 0664);

// Samples with Gaussian distribution in the range of [-2048, 2047] (12 bits)
// with zero mean and standard deviation of about 512.
// should be divided by 4 for 10-bit range and 16 for 8-bit range.
static const s32 gaussian_seq[2048] = {
  56,    568,   -180,  172,   124,   -84,   172,   -64,   -900,  24,   820,
  224,   1248,  996,   272,   -8,    -916,  -388,  -732,  -104,  -188, 800,
  112,   -652,  -320,  -376,  140,   -252,  492,   -168,  44,    -788, 588,
  -584,  500,   -228,  12,    680,   272,   -476,  972,   -100,  652,  368,
  432,   -196,  -720,  -192,  1000,  -332,  652,   -136,  -552,  -604, -4,
  192,   -220,  -136,  1000,  -52,   372,   -96,   -624,  124,   -24,  396,
  540,   -12,   -104,  640,   464,   244,   -208,  -84,   368,   -528, -740,
  248,   -968,  -848,  608,   376,   -60,   -292,  -40,   -156,  252,  -292,
  248,   224,   -280,  400,   -244,  244,   -60,   76,    -80,   212,  532,
  340,   128,   -36,   824,   -352,  -60,   -264,  -96,   -612,  416,  -704,
  220,   -204,  640,   -160,  1220,  -408,  900,   336,   20,    -336, -96,
  -792,  304,   48,    -28,   -1232, -1172, -448,  104,   -292,  -520, 244,
  60,    -948,  0,     -708,  268,   108,   356,   -548,  488,   -344, -136,
  488,   -196,  -224,  656,   -236,  -1128, 60,    4,     140,   276,  -676,
  -376,  168,   -108,  464,   8,     564,   64,    240,   308,   -300, -400,
  -456,  -136,  56,    120,   -408,  -116,  436,   504,   -232,  328,  844,
  -164,  -84,   784,   -168,  232,   -224,  348,   -376,  128,   568,  96,
  -1244, -288,  276,   848,   832,   -360,  656,   464,   -384,  -332, -356,
  728,   -388,  160,   -192,  468,   296,   224,   140,   -776,  -100, 280,
  4,     196,   44,    -36,   -648,  932,   16,    1428,  28,    528,  808,
  772,   20,    268,   88,    -332,  -284,  124,   -384,  -448,  208,  -228,
  -1044, -328,  660,   380,   -148,  -300,  588,   240,   540,   28,   136,
  -88,   -436,  256,   296,   -1000, 1400,  0,     -48,   1056,  -136, 264,
  -528,  -1108, 632,   -484,  -592,  -344,  796,   124,   -668,  -768, 388,
  1296,  -232,  -188,  -200,  -288,  -4,    308,   100,   -168,  256,  -500,
  204,   -508,  648,   -136,  372,   -272,  -120,  -1004, -552,  -548, -384,
  548,   -296,  428,   -108,  -8,    -912,  -324,  -224,  -88,   -112, -220,
  -100,  996,   -796,  548,   360,   -216,  180,   428,   -200,  -212, 148,
  96,    148,   284,   216,   -412,  -320,  120,   -300,  -384,  -604, -572,
  -332,  -8,    -180,  -176,  696,   116,   -88,   628,   76,    44,   -516,
  240,   -208,  -40,   100,   -592,  344,   -308,  -452,  -228,  20,   916,
  -1752, -136,  -340,  -804,  140,   40,    512,   340,   248,   184,  -492,
  896,   -156,  932,   -628,  328,   -688,  -448,  -616,  -752,  -100, 560,
  -1020, 180,   -800,  -64,   76,    576,   1068,  396,   660,   552,  -108,
  -28,   320,   -628,  312,   -92,   -92,   -472,  268,   16,    560,  516,
  -672,  -52,   492,   -100,  260,   384,   284,   292,   304,   -148, 88,
  -152,  1012,  1064,  -228,  164,   -376,  -684,  592,   -392,  156,  196,
  -524,  -64,   -884,  160,   -176,  636,   648,   404,   -396,  -436, 864,
  424,   -728,  988,   -604,  904,   -592,  296,   -224,  536,   -176, -920,
  436,   -48,   1176,  -884,  416,   -776,  -824,  -884,  524,   -548, -564,
  -68,   -164,  -96,   692,   364,   -692,  -1012, -68,   260,   -480, 876,
  -1116, 452,   -332,  -352,  892,   -1088, 1220,  -676,  12,    -292, 244,
  496,   372,   -32,   280,   200,   112,   -440,  -96,   24,    -644, -184,
  56,    -432,  224,   -980,  272,   -260,  144,   -436,  420,   356,  364,
  -528,  76,    172,   -744,  -368,  404,   -752,  -416,  684,   -688, 72,
  540,   416,   92,    444,   480,   -72,   -1416, 164,   -1172, -68,  24,
  424,   264,   1040,  128,   -912,  -524,  -356,  64,    876,   -12,  4,
  -88,   532,   272,   -524,  320,   276,   -508,  940,   24,    -400, -120,
  756,   60,    236,   -412,  100,   376,   -484,  400,   -100,  -740, -108,
  -260,  328,   -268,  224,   -200,  -416,  184,   -604,  -564,  -20,  296,
  60,    892,   -888,  60,    164,   68,    -760,  216,   -296,  904,  -336,
  -28,   404,   -356,  -568,  -208,  -1480, -512,  296,   328,   -360, -164,
  -1560, -776,  1156,  -428,  164,   -504,  -112,  120,   -216,  -148, -264,
  308,   32,    64,    -72,   72,    116,   176,   -64,   -272,  460,  -536,
  -784,  -280,  348,   108,   -752,  -132,  524,   -540,  -776,  116,  -296,
  -1196, -288,  -560,  1040,  -472,  116,   -848,  -1116, 116,   636,  696,
  284,   -176,  1016,  204,   -864,  -648,  -248,  356,   972,   -584, -204,
  264,   880,   528,   -24,   -184,  116,   448,   -144,  828,   524,  212,
  -212,  52,    12,    200,   268,   -488,  -404,  -880,  824,   -672, -40,
  908,   -248,  500,   716,   -576,  492,   -576,  16,    720,   -108, 384,
  124,   344,   280,   576,   -500,  252,   104,   -308,  196,   -188, -8,
  1268,  296,   1032,  -1196, 436,   316,   372,   -432,  -200,  -660, 704,
  -224,  596,   -132,  268,   32,    -452,  884,   104,   -1008, 424,  -1348,
  -280,  4,     -1168, 368,   476,   696,   300,   -8,    24,    180,  -592,
  -196,  388,   304,   500,   724,   -160,  244,   -84,   272,   -256, -420,
  320,   208,   -144,  -156,  156,   364,   452,   28,    540,   316,  220,
  -644,  -248,  464,   72,    360,   32,    -388,  496,   -680,  -48,  208,
  -116,  -408,  60,    -604,  -392,  548,   -840,  784,   -460,  656,  -544,
  -388,  -264,  908,   -800,  -628,  -612,  -568,  572,   -220,  164,  288,
  -16,   -308,  308,   -112,  -636,  -760,  280,   -668,  432,   364,  240,
  -196,  604,   340,   384,   196,   592,   -44,   -500,  432,   -580, -132,
  636,   -76,   392,   4,     -412,  540,   508,   328,   -356,  -36,  16,
  -220,  -64,   -248,  -60,   24,    -192,  368,   1040,  92,    -24,  -1044,
  -32,   40,    104,   148,   192,   -136,  -520,  56,    -816,  -224, 732,
  392,   356,   212,   -80,   -424,  -1008, -324,  588,   -1496, 576,  460,
  -816,  -848,  56,    -580,  -92,   -1372, -112,  -496,  200,   364,  52,
  -140,  48,    -48,   -60,   84,    72,    40,    132,   -356,  -268, -104,
  -284,  -404,  732,   -520,  164,   -304,  -540,  120,   328,   -76,  -460,
  756,   388,   588,   236,   -436,  -72,   -176,  -404,  -316,  -148, 716,
  -604,  404,   -72,   -88,   -888,  -68,   944,   88,    -220,  -344, 960,
  472,   460,   -232,  704,   120,   832,   -228,  692,   -508,  132,  -476,
  844,   -748,  -364,  -44,   1116,  -1104, -1056, 76,    428,   552,  -692,
  60,    356,   96,    -384,  -188,  -612,  -576,  736,   508,   892,  352,
  -1132, 504,   -24,   -352,  324,   332,   -600,  -312,  292,   508,  -144,
  -8,    484,   48,    284,   -260,  -240,  256,   -100,  -292,  -204, -44,
  472,   -204,  908,   -188,  -1000, -256,  92,    1164,  -392,  564,  356,
  652,   -28,   -884,  256,   484,   -192,  760,   -176,  376,   -524, -452,
  -436,  860,   -736,  212,   124,   504,   -476,  468,   76,    -472, 552,
  -692,  -944,  -620,  740,   -240,  400,   132,   20,    192,   -196, 264,
  -668,  -1012, -60,   296,   -316,  -828,  76,    -156,  284,   -768, -448,
  -832,  148,   248,   652,   616,   1236,  288,   -328,  -400,  -124, 588,
  220,   520,   -696,  1032,  768,   -740,  -92,   -272,  296,   448,  -464,
  412,   -200,  392,   440,   -200,  264,   -152,  -260,  320,   1032, 216,
  320,   -8,    -64,   156,   -1016, 1084,  1172,  536,   484,   -432, 132,
  372,   -52,   -256,  84,    116,   -352,  48,    116,   304,   -384, 412,
  924,   -300,  528,   628,   180,   648,   44,    -980,  -220,  1320, 48,
  332,   748,   524,   -268,  -720,  540,   -276,  564,   -344,  -208, -196,
  436,   896,   88,    -392,  132,   80,    -964,  -288,  568,   56,   -48,
  -456,  888,   8,     552,   -156,  -292,  948,   288,   128,   -716, -292,
  1192,  -152,  876,   352,   -600,  -260,  -812,  -468,  -28,   -120, -32,
  -44,   1284,  496,   192,   464,   312,   -76,   -516,  -380,  -456, -1012,
  -48,   308,   -156,  36,    492,   -156,  -808,  188,   1652,  68,   -120,
  -116,  316,   160,   -140,  352,   808,   -416,  592,   316,   -480, 56,
  528,   -204,  -568,  372,   -232,  752,   -344,  744,   -4,    324,  -416,
  -600,  768,   268,   -248,  -88,   -132,  -420,  -432,  80,    -288, 404,
  -316,  -1216, -588,  520,   -108,  92,    -320,  368,   -480,  -216, -92,
  1688,  -300,  180,   1020,  -176,  820,   -68,   -228,  -260,  436,  -904,
  20,    40,    -508,  440,   -736,  312,   332,   204,   760,   -372, 728,
  96,    -20,   -632,  -520,  -560,  336,   1076,  -64,   -532,  776,  584,
  192,   396,   -728,  -520,  276,   -188,  80,    -52,   -612,  -252, -48,
  648,   212,   -688,  228,   -52,   -260,  428,   -412,  -272,  -404, 180,
  816,   -796,  48,    152,   484,   -88,   -216,  988,   696,   188,  -528,
  648,   -116,  -180,  316,   476,   12,    -564,  96,    476,   -252, -364,
  -376,  -392,  556,   -256,  -576,  260,   -352,  120,   -16,   -136, -260,
  -492,  72,    556,   660,   580,   616,   772,   436,   424,   -32,  -324,
  -1268, 416,   -324,  -80,   920,   160,   228,   724,   32,    -516, 64,
  384,   68,    -128,  136,   240,   248,   -204,  -68,   252,   -932, -120,
  -480,  -628,  -84,   192,   852,   -404,  -288,  -132,  204,   100,  168,
  -68,   -196,  -868,  460,   1080,  380,   -80,   244,   0,     484,  -888,
  64,    184,   352,   600,   460,   164,   604,   -196,  320,   -64,  588,
  -184,  228,   12,    372,   48,    -848,  -344,  224,   208,   -200, 484,
  128,   -20,   272,   -468,  -840,  384,   256,   -720,  -520,  -464, -580,
  112,   -120,  644,   -356,  -208,  -608,  -528,  704,   560,   -424, 392,
  828,   40,    84,    200,   -152,  0,     -144,  584,   280,   -120, 80,
  -556,  -972,  -196,  -472,  724,   80,    168,   -32,   88,    160,  -688,
  0,     160,   356,   372,   -776,  740,   -128,  676,   -248,  -480, 4,
  -364,  96,    544,   232,   -1032, 956,   236,   356,   20,    -40,  300,
  24,    -676,  -596,  132,   1120,  -104,  532,   -1096, 568,   648,  444,
  508,   380,   188,   -376,  -604,  1488,  424,   24,    756,   -220, -192,
  716,   120,   920,   688,   168,   44,    -460,  568,   284,   1144, 1160,
  600,   424,   888,   656,   -356,  -320,  220,   316,   -176,  -724, -188,
  -816,  -628,  -348,  -228,  -380,  1012,  -452,  -660,  736,   928,  404,
  -696,  -72,   -268,  -892,  128,   184,   -344,  -780,  360,   336,  400,
  344,   428,   548,   -112,  136,   -228,  -216,  -820,  -516,  340,  92,
  -136,  116,   -300,  376,   -244,  100,   -316,  -520,  -284,  -12,  824,
  164,   -548,  -180,  -128,  116,   -924,  -828,  268,   -368,  -580, 620,
  192,   160,   0,     -1676, 1068,  424,   -56,   -360,  468,   -156, 720,
  288,   -528,  556,   -364,  548,   -148,  504,   316,   152,   -648, -620,
  -684,  -24,   -376,  -384,  -108,  -920,  -1032, 768,   180,   -264, -508,
  -1268, -260,  -60,   300,   -240,  988,   724,   -376,  -576,  -212, -736,
  556,   192,   1092,  -620,  -880,  376,   -56,   -4,    -216,  -32,  836,
  268,   396,   1332,  864,   -600,  100,   56,    -412,  -92,   356,  180,
  884,   -468,  -436,  292,   -388,  -804,  -704,  -840,  368,   -348, 140,
  -724,  1536,  940,   372,   112,   -372,  436,   -480,  1136,  296,  -32,
  -228,  132,   -48,   -220,  868,   -1016, -60,   -1044, -464,  328,  916,
  244,   12,    -736,  -296,  360,   468,   -376,  -108,  -92,   788,  368,
  -56,   544,   400,   -672,  -420,  728,   16,    320,   44,    -284, -380,
  -796,  488,   132,   204,   -596,  -372,  88,    -152,  -908,  -636, -572,
  -624,  -116,  -692,  -200,  -56,   276,   -88,   484,   -324,  948,  864,
  1000,  -456,  -184,  -276,  292,   -296,  156,   676,   320,   160,  908,
  -84,   -1236, -288,  -116,  260,   -372,  -644,  732,   -756,  -96,  84,
  344,   -520,  348,   -688,  240,   -84,   216,   -1044, -136,  -676, -396,
  -1500, 960,   -40,   176,   168,   1516,  420,   -504,  -344,  -364, -360,
  1216,  -940,  -380,  -212,  252,   -660,  -708,  484,   -444,  -152, 928,
  -120,  1112,  476,   -260,  560,   -148,  -344,  108,   -196,  228,  -288,
  504,   560,   -328,  -88,   288,   -1008, 460,   -228,  468,   -836, -196,
  76,    388,   232,   412,   -1168, -716,  -644,  756,   -172,  -356, -504,
  116,   432,   528,   48,    476,   -168,  -608,  448,   160,   -532, -272,
  28,    -676,  -12,   828,   980,   456,   520,   104,   -104,  256,  -344,
  -4,    -28,   -368,  -52,   -524,  -572,  -556,  -200,  768,   1124, -208,
  -512,  176,   232,   248,   -148,  -888,  604,   -600,  -304,  804,  -156,
  -212,  488,   -192,  -804,  -256,  368,   -360,  -916,  -328,  228,  -240,
  -448,  -472,  856,   -556,  -364,  572,   -12,   -156,  -368,  -340, 432,
  252,   -752,  -152,  288,   268,   -580,  -848,  -592,  108,   -76,  244,
  312,   -716,  592,   -80,   436,   360,   4,     -248,  160,   516,  584,
  732,   44,    -468,  -280,  -292,  -156,  -588,  28,    308,   912,  24,
  124,   156,   180,   -252,  944,   -924,  -772,  -520,  -428,  -624, 300,
  -212,  -1144, 32,    -724,  800,   -1128, -212,  -1288, -848,  180,  -416,
  440,   192,   -576,  -792,  -76,   -1080, 80,    -532,  -352,  -132, 380,
  -820,  148,   1112,  128,   164,   456,   700,   -924,  144,   -668, -384,
  648,   -832,  508,   552,   -52,   -100,  -656,  208,   -568,  748,  -88,
  680,   232,   300,   192,   -408,  -1012, -152,  -252,  -268,  272,  -876,
  -664,  -648,  -332,  -136,  16,    12,    1152,  -28,   332,   -536, 320,
  -672,  -460,  -316,  532,   -260,  228,   -40,   1052,  -816,  180,  88,
  -496,  -556,  -672,  -368,  428,   92,    356,   404,   -408,  252,  196,
  -176,  -556,  792,   268,   32,    372,   40,    96,    -332,  328,  120,
  372,   -900,  -40,   472,   -264,  -592,  952,   128,   656,   112,  664,
  -232,  420,   4,     -344,  -464,  556,   244,   -416,  -32,   252,  0,
  -412,  188,   -696,  508,   -476,  324,   -1096, 656,   -312,  560,  264,
  -136,  304,   160,   -64,   -580,  248,   336,   -720,  560,   -348, -288,
  -276,  -196,  -500,  852,   -544,  -236,  -1128, -992,  -776,  116,  56,
  52,    860,   884,   212,   -12,   168,   1020,  512,   -552,  924,  -148,
  716,   188,   164,   -340,  -520,  -184,  880,   -152,  -680,  -208, -1156,
  -300,  -528,  -472,  364,   100,   -744,  -1056, -32,   540,   280,  144,
  -676,  -32,   -232,  -280,  -224,  96,    568,   -76,   172,   148,  148,
  104,   32,    -296,  -32,   788,   -80,   32,    -16,   280,   288,  944,
  428,   -484
};

static inline void fg_data_copy_unsigned(
	s32 *to, u8 *from, s32 size, u8 endian_swap, s32 offt)
{
	s32 i;

	if (endian_swap) {
		for (i = 0; i < size; i += 4) {
			to[i] = from[i + 3] - offt;
			to[i + 1] = from[i + 2] - offt;
			to[i + 2] = from[i + 1] - offt;
			to[i + 3] = from[i] - offt;
		}
	} else {
		for (i = 0; i < size; i++) {
			to[i] = from[i];
		}
	}
}

static inline void fg_data_copy_signed(
	s32 *to, s8 *from, s32 size, u8 endian_swap, s32 offt)
{
	s32 i;

	if (endian_swap) {
		for (i = 0; i < size; i += 4) {
			to[i] = (s32)(from[i + 3] - offt);
			to[i + 1] = (s32)(from[i + 2] - offt);
			to[i + 2] = (s32)(from[i + 1] - offt);
			to[i + 3] = (s32)(from[i] - offt);
		}
	} else {
		for (i = 0; i < size; i++) {
			to[i] = from[i];
		}
	}
}


static void fg_info_print(struct aom_film_grain_t *p)
{
	s32 i;

	pr_info("apply_grain: %d\n", p->apply_grain);
	pr_info("update_parameters: %d\n", p->update_parameters);

	if (p->num_y_points > 0) {
		for (i = 0; i < 14; i++) {
			pr_info("scaling_points_y[%d][0]: %x, [1]: %x\n", i, p->scaling_points_y[i][0], p->scaling_points_y[i][1]);
		}
	}
	pr_info("num_y_points: %d\n", p->num_y_points);

	if (p->num_cb_points > 0) {
		for (i = 0; i < 10; i++) {
			pr_info("scaling_points_cb[%d][0]: %x, [1]: %x\n", i, p->scaling_points_cb[i][0], p->scaling_points_cb[i][1]);
		}
	}
	pr_info("num_cb_points: %d\n", p->num_cb_points);

	if (p->num_cb_points > 0) {
		for (i = 0; i < 10; i++) {
			pr_info("scaling_points_cr[%d][0]: %x, [1]: %x\n", i, p->scaling_points_cr[i][0], p->scaling_points_cr[i][1]);
		}
	}
	pr_info("num_cr_points: %d\n", p->num_cr_points);
	pr_info("scaling_shift: %d\n", p->scaling_shift);
	pr_info("ar_coeff_lag: 0x%x\n", p->ar_coeff_lag);

	for (i = 0; i < 24; i++) {
		pr_info("ar_coeffs_y[%d]: %x\n", i, p->ar_coeffs_y[i]);
	}
	for (i = 0; i < 25; i++) {
		pr_info("ar_coeffs_cb[%d]: %x\n", i, p->ar_coeffs_cb[i]);
	}
	for (i = 0; i < 25; i++) {
		pr_info("ar_coeffs_cr[%d]: %x\n", i, p->ar_coeffs_cr[i]);
	}
	pr_info("ar_coeff_shift: %d\n", p->ar_coeff_shift);
	pr_info("cb_mult: 0x%x\n", p->cb_mult);
	pr_info("cb_luma_mult: 0x%x\n", p->cb_luma_mult);
	pr_info("cb_offset: 0x%x\n", p->cb_offset);

	pr_info("cr_mult: 0x%x\n", p->cr_mult);
	pr_info("cr_luma_mult: 0x%x\n", p->cr_luma_mult);
	pr_info("cr_offset: 0x%x\n", p->cr_offset);

	pr_info("overlap_flag: %d\n", p->overlap_flag);
	pr_info("clip_to_restricted_range: %d\n", p->clip_to_restricted_range);
	pr_info("bit_depth: %d\n", p->bit_depth);
	pr_info("mc_identity: %d\n", p->mc_identity);
	pr_info("chroma_scaling_from_luma: %d\n", p->chroma_scaling_from_luma);
	pr_info("grain_scale_shift: %d\n", p->grain_scale_shift);
	pr_info("random_seed: 0x%x\n", p->random_seed);
}

static void film_grain_data_parse(struct aom_film_grain_t *para, u32 fgs_ctrl, u32 *fgs_data)
{
	/* index 0
	 bit[30]   - bit_depth_10
	 bit[29]   - mc_identity
	 bit[28]   - num_pos_chroma_one_more
	 bit[27:24] - ar_coeff_shift
	 bit[23:20] - scaling_shift
	 bit[19]   - overlap_flag
	 bit[18]   - clip_to_restricted_range
	 bit[17]   - chroma_scaling_from_luma
	 */
	para->bit_depth = (fgs_data[0] & (1 << 30)) ? 10 : 8;  // video bit depth
	para->mc_identity = (fgs_data[0] & (1 << 29)) ? 1 : 0;
	para->ar_coeff_shift           = (fgs_data[0] >> 24) & 0xf;;  // values : 6..9
	para->scaling_shift            = (fgs_data[0] >> 20) & 0xf;   // values : 8..11
	para->overlap_flag             = (fgs_data[0] >> 19) & 0x1;
	para->clip_to_restricted_range = (fgs_data[0] >> 18) & 0x1;
	para->chroma_scaling_from_luma = (fgs_data[0] >> 17) & 0x1;

	/* index 1
	 bit[31:30] - grain_scale_shift
	 bit[29:28] - ar_coeff_lag
	 bit[27:20] - ar_coeffs_cr[24]
	 bit[19:12] - ar_coeffs_cb[24]
	 bit[11:8]  - num_cr_points
	 bit[7:4]   - num_cb_points
	 bit[3:0]   - num_y_points
	*/
	para->grain_scale_shift      = (fgs_data[1] >> 30) & 0x3;
	para->ar_coeff_lag           = (fgs_data[1] >> 28) & 0x3;  // values:  0..3
	para->ar_coeffs_cr[24]       = (fgs_data[1] >> 20) & 0xff;
	para->ar_coeffs_cr[24] -= 128;
	para->ar_coeffs_cb[24]       = (fgs_data[1] >> 12) & 0xff;
	para->ar_coeffs_cb[24] -= 128;
	para->num_cr_points          = (fgs_data[1] >> 8) & 0xf;   // value: 0..10
	para->num_cb_points          = (fgs_data[1] >> 4) & 0xf;   // value: 0..10
	para->num_y_points           = (fgs_data[1] >> 0) & 0xf;   // value: 0..14

	/* index 2~8 scaling_points_y[14][2] */
	fg_data_copy_unsigned(&para->scaling_points_y[0][0], (u8 *)&fgs_data[2], para->num_y_points * 2, 1, 0);  // 8 bit values // brian  swap ?

	/* index 9-13 scaling_points_cb[10][2] */
	fg_data_copy_unsigned(&para->scaling_points_cb[0][0], (u8 *)&fgs_data[9], para->num_cb_points * 2, 1, 0);  // 8 bit values // brian  chroma_scaling_from_luma and swap ?

	// 14-18 -- scaling_points_cr[10][2]
	fg_data_copy_unsigned(&para->scaling_points_cr[0][0], (u8 *)&fgs_data[14], para->num_cr_points * 2, 1, 0); // 8 bit values // brian  chroma_scaling_from_luma and swap ?

	// 19-24 -- ar_coeffs_y[0-23]
	fg_data_copy_unsigned(para->ar_coeffs_y, (u8 *)&fgs_data[19], 24, 1, 128); // brian  chroma_scaling_from_luma and swap ?

	// 25-30 -- ar_coeffs_cb[0-23]
	fg_data_copy_unsigned(para->ar_coeffs_cb, (u8 *)&fgs_data[25], 24, 1, 128); // brian  chroma_scaling_from_luma ?

	// 31-36 -- ar_coeffs_cr[0-23]
	fg_data_copy_unsigned(para->ar_coeffs_cr, (u8 *)&fgs_data[31], 24, 1, 128);  // brian  chroma_scaling_from_luma ?

	/* index 37
	 bit[31:24]  - cb_mult
	 bit[23:16]  - cb_luma_mult
	 bit[15:7]   - cb_offset
	*/
	if (para->num_cb_points > 0) {  // brian  chroma_scaling_from_luma ?
		para->cb_mult      = (fgs_data[37] >> 24) & 0xff;     // 8 bits
		para->cb_luma_mult = (fgs_data[37] >> 16) & 0xff;     // 8 bits
		para->cb_offset    = (fgs_data[37] >> 7) & 0x1ff;     // 9 bits
	} else {
		para->cb_mult	   = 0;	  // 8 bits
		para->cb_luma_mult = 0;	  // 8 bits
		para->cb_offset    = 0;	  // 9 bits
	}

	/* index 38
	 bit[31:24]  - cr_mult
	 bit[23:16]  - cr_luma_mult
	 bit[15:7]   - cr_offset
	*/
	if (para->num_cr_points > 0) {  // brian  chroma_scaling_from_luma ?
		para->cr_mult      = (fgs_data[38] >> 24) & 0xff;       // 8 bits
		para->cr_luma_mult = (fgs_data[38] >> 16) & 0xff;  // 8 bits
		para->cr_offset    = (fgs_data[38] >> 7) & 0x1ff;     // 9 bits
	} else {
		para->cr_mult      = 0;       // 8 bits
		para->cr_luma_mult = 0;  // 8 bits
		para->cr_offset    = 0;     // 9 bits
	}
	/*
	 bit[31:16] - random_seed
	 bit[06]   - apply_cr (RO) //assign  apply_cr = num_cr_points>0 | chroma_scaling_from_luma;
	 bit[05]   - apply_cb (RO) //assign  apply_cb = num_cb_points>0 | chroma_scaling_from_luma;
	 bit[04]   - apply_lu (RO) //assign  apply_lu = num_y_points>0;
	 bit[03]   - fgs_not_bypass : 0=fgs bypass 1:=fgs not bypass (default=0)
	 bit[02]   - update_parameters
	 bit[01]   - apply_grain
	 bit[00]   - film gran start
	*/
	para->apply_grain = (fgs_ctrl >> 1) & 0x1;
	para->update_parameters = (fgs_ctrl >> 2) & 0x1;
	para->random_seed = (fgs_ctrl >> 16) & 0xffff;
}

#define DEFAULT_ALIGNMENT (2 * sizeof(void *))

static inline void *fgs_alloc(u32 size)
{
	void *addr;
	size = size + DEFAULT_ALIGNMENT - 1 + sizeof(size_t);
	addr = vzalloc(size);
	if (addr == NULL)
		addr = vzalloc(size);

	return addr;
}

static void init_arrays(struct aom_film_grain_t *params,
	s32 ***pred_pos_luma_p,	s32 ***pred_pos_chroma_p,
	s32 **luma_grain_block, s32 **cb_grain_block, s32 **cr_grain_block,
	s32 luma_grain_samples, s32 luma_grain_stride,
	s32 chroma_grain_samples, s32 chroma_grain_stride)
{
	s32 num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
	s32 num_pos_chroma = num_pos_luma;
	s32 row, col;
	s32 pos_ar_index = 0;
	s32 **pred_pos_luma;
	s32 **pred_pos_chroma;

	if (params->num_y_points > 0)
		++num_pos_chroma;

	if (debug_fgs & DEBUG_FGS_DETAIL) {
		pr_info("num_pos_luma %d, sizeof(*pred_pos_luma):%zd\n", num_pos_luma, sizeof(*pred_pos_luma));
		pr_info("num_pos_chroma %d, sizeof(*pred_pos_chroma):%zd\n", num_pos_chroma, sizeof(*pred_pos_chroma));
	}
	pred_pos_luma = (s32 **)fgs_alloc(sizeof(*pred_pos_luma) * num_pos_luma);

	for (row = 0; row < num_pos_luma; row++) {
		pred_pos_luma[row] = (s32 *)fgs_alloc(sizeof(**pred_pos_luma) * 3);
	}

	pred_pos_chroma = (s32 **)fgs_alloc(sizeof(*pred_pos_chroma) * num_pos_chroma);

	for (row = 0; row < num_pos_chroma; row++) {
		pred_pos_chroma[row] = (s32 *)fgs_alloc(sizeof(**pred_pos_chroma) * 3);
	}

	for (row = -params->ar_coeff_lag; row < 0; row++) {
		for (col = -params->ar_coeff_lag;
			col < params->ar_coeff_lag + 1; col++) {
			pred_pos_luma[pos_ar_index][0] = row * luma_grain_stride;
			pred_pos_luma[pos_ar_index][1] = col;
			pred_pos_luma[pos_ar_index][2] = 0;

			pred_pos_chroma[pos_ar_index][0] = row * chroma_grain_stride;
			pred_pos_chroma[pos_ar_index][1] = col;
			pred_pos_chroma[pos_ar_index][2] = 0;
			++pos_ar_index;
		}
	}

	for (col = -params->ar_coeff_lag; col < 0; col++) {
		pred_pos_luma[pos_ar_index][0] = 0;
		pred_pos_luma[pos_ar_index][1] = col;
		pred_pos_luma[pos_ar_index][2] = 0;

		pred_pos_chroma[pos_ar_index][0] = 0;
		pred_pos_chroma[pos_ar_index][1] = col;
		pred_pos_chroma[pos_ar_index][2] = 0;

		++pos_ar_index;
	}

	if (params->num_y_points > 0) {
		pred_pos_chroma[pos_ar_index][0] = 0;
		pred_pos_chroma[pos_ar_index][1] = 0;
		pred_pos_chroma[pos_ar_index][2] = 1;
	}

	*pred_pos_luma_p = pred_pos_luma;
	*pred_pos_chroma_p = pred_pos_chroma;
	*luma_grain_block =
		(s32 *)fgs_alloc(sizeof(**luma_grain_block) * luma_grain_samples);
	if (debug_fgs & DEBUG_FGS_DETAIL) {
		pr_info("luma block size %zd, luma_grain_samples:%d\n", sizeof(**luma_grain_block) * luma_grain_samples, luma_grain_samples);
	}
	*cb_grain_block =
		(s32 *)fgs_alloc(sizeof(**cb_grain_block) * chroma_grain_samples);
	*cr_grain_block =
		(s32*)fgs_alloc(sizeof(**cr_grain_block) * chroma_grain_samples);
	if (debug_fgs & DEBUG_FGS_DETAIL) {
		pr_info("chroma block size %zd, chroma_grain_samples:%d\n", sizeof(**cb_grain_block) * chroma_grain_samples, chroma_grain_samples);
	}
}

static void dealloc_arrays(struct aom_film_grain_t *params,
	s32 ***pred_pos_luma, s32 ***pred_pos_chroma,
	s32 **luma_grain_block, s32 **cb_grain_block, s32 **cr_grain_block)
{
	s32 num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
	s32 num_pos_chroma = num_pos_luma;
	s32 row;

	if (params->num_y_points > 0)
		++num_pos_chroma;

	for (row = 0; row < num_pos_luma; row++) {
		vfree((*pred_pos_luma)[row]);
	}
	vfree(*pred_pos_luma);

	for (row = 0; row < num_pos_chroma; row++) {
		vfree((*pred_pos_chroma)[row]);
	}
	vfree((*pred_pos_chroma));

	vfree(*luma_grain_block);

	vfree(*cb_grain_block);

	vfree(*cr_grain_block);
}

// get a number between 0 and 2^bits - 1
static inline s32 get_random_number(u16 *random_register, s32 bits)
{
	u16 bit;

	bit = ((*random_register >> 0) ^ (*random_register >> 1) ^
		(*random_register >> 3) ^ (*random_register >> 12)) & 1;
	*random_register = (*random_register >> 1) | (bit << 15);
	return (*random_register >> (16 - bits)) & ((1 << bits) - 1);
}

static inline void init_random_generator(u16 *random_register, u16 seed, s32 luma_line)
{
  // same for the picture
	u16 msb = (seed >> 8) & 0xff;
	u16 lsb = seed & 0xff;
	s32 luma_num = luma_line >> 5;

	*random_register = (msb << 8) + lsb;
	*random_register ^= ((luma_num * 37 + 178) & 255) << 8;
	*random_register ^= ((luma_num * 173 + 105) & 255);
}

static void generate_luma_grain_block(struct aom_film_grain_t *params,
	s32 **pred_pos_luma, s32 *luma_grain_block,
	s32 luma_block_size_y, s32 luma_block_size_x, s32 luma_grain_stride,
	s8 *table_ptr)
{
	s32 x_start = left_pad + ar_padding * 2;
	s32 x_end = luma_block_size_x - right_pad - (ar_padding * 2) - 1;
	s32 y_start = top_pad + ar_padding * 2;
	s32 y_end = luma_block_size_y - bottom_pad - 1;
	s32 x_pad_start = left_pad;
	s32 x_pad_end = luma_block_size_x - right_pad - 1;
	s32 y_pad_start = top_pad;
	s32 y_pad_end = luma_block_size_y - bottom_pad - 1;
	u8 cnt = 0;
	s32 grain_block[4], grain_val;
	u32 bit_mask = (1 << params->bit_depth) - 1;
	s8 gauss_sec_shift = 12 - params->bit_depth + params->grain_scale_shift;
	s32 gauss_sec_val = (1 << gauss_sec_shift) >> 1;
	u32 num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
	s32 rounding_offset = (1 << (params->ar_coeff_shift - 1));
	u32 i, j, pos, block_id = 0;
	s32 random_num;
	s32 grain_center = 128 << (params->bit_depth - 8);
	s32 grain_min = 0 - grain_center;
	s32 grain_max = (256 << (params->bit_depth - 8)) - 1 - grain_center;
	s32 pred[30];

	table_ptr += luma_grain_block_offt;
	params->random_register_y = params->random_seed;

	if (params->num_y_points == 0)
		return;

	if (debug_fgs & DEBUG_FGS_DETAIL) {
		pr_info("-->gauss_sec_shift = %d \n",gauss_sec_shift);
		pr_info("-->ar_coeff_shift  = %x \n",params->ar_coeff_shift);
		pr_info("-->rounding_offset = %x \n",rounding_offset);
	}

	for (pos = 0; pos < num_pos_luma; pos++)
		pred[pos] = pred_pos_luma[pos][0] + pred_pos_luma[pos][1];

	for (i = 0; i < luma_block_size_y; i++) {
		for (j = 0; j < luma_block_size_x; j++) {
			random_num =
				get_random_number(&params->random_register_y, gauss_bits);
			grain_val = (gaussian_seq[random_num] + gauss_sec_val) >> gauss_sec_shift;
			luma_grain_block[block_id] = grain_val;
			if (i >= y_pad_start && i <= y_pad_end &&
			    j >= x_pad_start && j <= x_pad_end) {
				s32 wsum = 0;

				for (pos = 0; pos < num_pos_luma; pos++)
					wsum += params->ar_coeffs_y[pos] *
						luma_grain_block[block_id + pred[pos]];
				wsum += rounding_offset;
				wsum = wsum >> params->ar_coeff_shift;
				grain_val = MID(grain_val +	wsum, grain_min, grain_max);
				luma_grain_block[block_id] = grain_val;
				if (i >= y_start && i <= y_end &&
				    j >= x_start && j <= x_end) {
					grain_block[cnt++] = grain_val & bit_mask;
					if (cnt == 4) {
						table_ptr[0] =  grain_block[0] & 0xff;
						table_ptr[1] =  (grain_block[1] & 0x3f) << 2;
						table_ptr[1] |= (grain_block[0] >> 8) & 3;
						table_ptr[2] =  (grain_block[2] & 0xf) << 4;
						table_ptr[2] |= (grain_block[1] >> 6) & 0xf;
						table_ptr[3] =  (grain_block[3] & 3) << 6;
						table_ptr[3] |= (grain_block[2] >> 4) & 0x3f;
						table_ptr[4] =  (grain_block[3] >> 2) & 0xff;
						table_ptr += 5;
						cnt = 0;
					}
				}
			}
			block_id++;
		}
    }
}

static s32 generate_chroma_grain_blocks(struct aom_film_grain_t *params,
    s32 **pred_pos_chroma, s32 *luma_grain_block, s32 *cb_grain_block,
    s32 *cr_grain_block, s32 luma_grain_stride, s32 chroma_block_size_y,
    s32 chroma_block_size_x, s32 chroma_grain_stride,
    s32 chroma_subsamp_y, s32 chroma_subsamp_x,
    s8 *table_ptr)
{
	s8 gauss_sec_shift = 12 - params->bit_depth + params->grain_scale_shift;
	s32 gauss_sec_val = (1 << gauss_sec_shift) >> 1;
	s32 i, j, k, l, pos;
	u32 num_pos_chroma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
	s32 rounding_offset = (1 << (params->ar_coeff_shift - 1));
	s32 chroma_grain_samples = chroma_block_size_y * chroma_block_size_x;
	s32 cb_block[2], cr_block[2], cb_grain_val, cr_grain_val;
	u32 bit_mask = (1 << params->bit_depth) - 1;
	s32 x_start = left_pad + ar_padding * (2 >> chroma_subsamp_x);
	s32 x_end = chroma_block_size_x - right_pad - (ar_padding * (2 >> chroma_subsamp_x)) - 1;
	s32 y_start = top_pad + ar_padding * (2 >> chroma_subsamp_x);
	s32 y_end = chroma_block_size_y - bottom_pad - 1;
	s32 x_pad_start = left_pad;
	s32 x_pad_end = chroma_block_size_x - right_pad - 1;
	s32 y_pad_start = top_pad;
	s32 y_pad_end = chroma_block_size_y - bottom_pad - 1;
	u8 cnt;
	s32 random_num_cb, random_num_cr;
	u32 block_id;
	u8 do_cb = 1, do_cr = 1;
	s32 grain_center = 128 << (params->bit_depth - 8);
	s32 grain_min = 0 - grain_center;
	s32 grain_max = (256 << (params->bit_depth - 8)) - 1 - grain_center;
	s32 pred[30];
	u8 subsamp = chroma_subsamp_y + chroma_subsamp_x;
	u8 subsamp_val = (1 << (chroma_subsamp_y + chroma_subsamp_x)) >> 1;

	table_ptr += cb_grain_block_offt;

	if (params->num_y_points > 0)
		++num_pos_chroma;

	for (pos = 0; pos < num_pos_chroma; pos++)
		pred[pos] = pred_pos_chroma[pos][0] + pred_pos_chroma[pos][1];

	if (!params->num_cb_points && !params->chroma_scaling_from_luma) {
		memset(cb_grain_block, 0, sizeof(*cb_grain_block) * chroma_grain_samples);
		do_cb = 0;
	} else {
		init_random_generator(
			&params->random_register_cb,
			params->random_seed, 7 << 5);
	}
	if (!params->num_cr_points && !params->chroma_scaling_from_luma) {
		memset(cr_grain_block, 0, sizeof(*cr_grain_block) * chroma_grain_samples);
		do_cr = 0;
	} else {
		init_random_generator(
			&params->random_register_cr,
			params->random_seed, 11 << 5);

	}

	block_id = 0;
	for (i = 0; i < chroma_block_size_y; i++) {
		for (j = 0; j < chroma_block_size_x; j++) {
			if (do_cb) {
				random_num_cb = get_random_number(
					&params->random_register_cb, gauss_bits);
				cb_grain_val =
					(gaussian_seq[random_num_cb] + gauss_sec_val)
					>> gauss_sec_shift;
				cb_grain_block[block_id] = cb_grain_val;
			}
			if (do_cr) {
				random_num_cr = get_random_number(
					&params->random_register_cr, gauss_bits);
				cr_grain_val =
					(gaussian_seq[random_num_cr] + gauss_sec_val)
					>> gauss_sec_shift;
				cr_grain_block[block_id] = cr_grain_val;
			}
			if (i >= y_pad_start && i <= y_pad_end &&
			    j >= x_pad_start && j <= x_pad_end) {
				s32 wsum_cb = 0;
				s32 wsum_cr = 0;

				for (pos = 0; pos < num_pos_chroma; pos++) {
					if (pred_pos_chroma[pos][2] == 0) {
						wsum_cb += params->ar_coeffs_cb[pos] *
							cb_grain_block[block_id + pred[pos]];
						wsum_cr += params->ar_coeffs_cr[pos] *
							cr_grain_block[block_id + pred[pos]];
					} else if (pred_pos_chroma[pos][2] == 1) {
						s32 av_luma = 0;
						s32 luma_coord_y = ((i - top_pad) << chroma_subsamp_y) + top_pad;
						s32 luma_coord_x = ((j - left_pad) << chroma_subsamp_x) + left_pad;

						for (k = luma_coord_y;
							k < luma_coord_y + chroma_subsamp_y + 1; k++)
							for (l = luma_coord_x;
								l < luma_coord_x + chroma_subsamp_x + 1; l++) {
								av_luma += luma_grain_block[k * luma_grain_stride + l];
							}
						av_luma = (av_luma + subsamp_val) >> subsamp;
						wsum_cb += params->ar_coeffs_cb[pos] * av_luma;
						wsum_cr += params->ar_coeffs_cr[pos] * av_luma;
					} else {
						pr_info(
							"Grain synthesis: prediction between two chroma components is "
							"not supported!");
						return -1;
					}
				}
				if (do_cb) {
					wsum_cb += rounding_offset;
					wsum_cb = wsum_cb >> params->ar_coeff_shift;
					cb_grain_val =
						MID(cb_grain_val + wsum_cb, grain_min, grain_max);
					cb_grain_block[block_id] = cb_grain_val;
				}
				if (do_cr) {
					wsum_cr += rounding_offset;
					wsum_cr = wsum_cr >> params->ar_coeff_shift;
					cr_grain_val =
						MID(cr_grain_val + wsum_cr, grain_min, grain_max);
					cr_grain_block[block_id] = cr_grain_val;
				}
				if (i >= y_start && i <= y_end && j >= x_start && j <= x_end) {
					cb_block[cnt] = cb_grain_val & bit_mask;
					cr_block[cnt++] = cr_grain_val & bit_mask;
					if (cnt == 2) {
						table_ptr[0] =	cb_block[0] & 0xff;
						table_ptr[1] =	(cr_block[0] & 0x3f) << 2;
						table_ptr[1] |= (cb_block[0] >> 8) & 3;
						table_ptr[2] =	(cb_block[1] & 0xf) << 4;
						table_ptr[2] |= (cr_block[0] >> 6) & 0xf;
						table_ptr[3] =	(cr_block[1] & 3) << 6;
						table_ptr[3] |= (cb_block[1] >> 4) & 0x3f;
						table_ptr[4] =	(cr_block[1] >> 2) & 0xff;
						table_ptr += 5;
						cnt = 0;
					}
				}
			}
			block_id++;
		}
	}
	return 0;
}

static void init_scaling_function(
	s32 scaling_points[][2], u8 num_points, s32 scaling_delta[])
{
	s64 delta;
	s32 delta_y, delta_x, point;

	if (num_points == 0) return;

	for (point = 0; point < num_points - 1; point++) {
		delta_y = scaling_points[point + 1][1] - scaling_points[point][1];
		delta_x = scaling_points[point + 1][0] - scaling_points[point][0];

		delta = delta_y * ((65536 + (delta_x >> 1)) / delta_x);
		scaling_delta[point]=delta;
	}
}

void convert_to_fg_table(struct aom_film_grain_t *params, u8 *table_ptr)
{
	s32 i, j;
	u32 total = 0;

	if (params->num_y_points > 0 ||
	    params->num_cb_points > 0 ||
	    params->num_cr_points > 0) {
		table_ptr[0] =
			((params->apply_grain & 0x1) << 0) | 	//1bit [0]
			((params->overlap_flag & 0x1) << 1) | 	//1bit [1]
			((params->chroma_scaling_from_luma & 0x1) << 2) | 	//1bit [2]
			((params->clip_to_restricted_range & 0x1) << 3) | 	//1bit [3]
			((params->mc_identity & 0x1) << 4);		//1bit [4]
		table_ptr[1] = (params->random_seed >> 0) & 0xff; //random_seed: 16bits
		table_ptr[2] = (params->random_seed >> 8) & 0xff;
		table_ptr[3] =
			((params->num_y_points & 0xf) << 0) |	//4bit	0...14
			((params->num_cb_points & 0xf) << 4);	//4bit	0...10
		table_ptr[4] =
			((params->num_cr_points & 0xf) << 0) |	//4bit	0...10
			((params->scaling_shift & 0xf) << 4);	//4bit: 8...11
		table_ptr[5] =  params->cb_luma_mult;				//8bit
		table_ptr[6] =  params->cb_mult; 					//8bit
		table_ptr[7] = ((params->cb_offset >> 0) & 0xff); //cb_offset:9bit
		table_ptr[8] = ((params->cb_offset >> 8) & 0x1);
		table_ptr[9] =  params->cr_luma_mult; //8bit
		table_ptr[10] =  params->cr_mult; //8bit
		table_ptr[11] = ((params->cr_offset >> 0) & 0xff); //cr_offset:9bit
		table_ptr[12] = ((params->cr_offset >> 8) & 0x1);
		table_ptr[13] = 0;
		table_ptr[14] = 0;
		table_ptr[15] = 0;
	} else {
		memset(table_ptr, 0, 16 * sizeof(char));
	}
	total = 1;
	table_ptr += 16;
	//y_scale (7x128bit)
	if (params->num_y_points > 0) {
		for (i = 0; i < 14; i += 2) {
			j = i + 1;
			table_ptr[0] = params->scaling_points_y[i][0]; //x
			table_ptr[1] = params->scaling_points_y[i][1]; //y
			//delta, it be signed, so 25bit
			table_ptr[2] = (params->scaling_delta_y[i] >> 0) & 0xff;
			table_ptr[3] = (params->scaling_delta_y[i] >> 8) & 0xff;
			table_ptr[4] = (params->scaling_delta_y[i] >> 16) & 0xff;
			table_ptr[5] = (params->scaling_delta_y[i] >> 24) & 0xff;
			table_ptr[6] = params->scaling_points_y[j][0];
			table_ptr[7] = params->scaling_points_y[j][1];
			table_ptr[8] =
				(j == 13) ? 0 : ((params->scaling_delta_y[j] >> 0) & 0xff);
			table_ptr[9] =
				(j == 13) ? 0 : ((params->scaling_delta_y[j] >> 8) & 0xff);
			table_ptr[10] =
				(j == 13) ? 0 : ((params->scaling_delta_y[j] >> 16) & 0xff);
			table_ptr[11] =
				(j == 13) ? 0 : ((params->scaling_delta_y[j] >> 24) & 0xff);
			table_ptr[12] = 0;
			table_ptr[13] = 0;
			table_ptr[14] = 0;
			table_ptr[15] = 0;
			table_ptr += 16;
		}
	} else {
		memset(table_ptr, 0, 16 * 7 * sizeof(char));
		table_ptr += 16 * 7;
	}
	total += 7;
	if (params->num_cb_points > 0) {
		for (i = 0; i < 10; i += 2) {
			j = i + 1;
			table_ptr[0] = params->scaling_points_cb[i][0];
			table_ptr[1] = params->scaling_points_cb[i][1];
			//delta, it be signed, so 25bit
			table_ptr[2] = (params->scaling_delta_cb[i] >> 0) & 0xff;
			table_ptr[3] = (params->scaling_delta_cb[i] >> 8) & 0xff;
			table_ptr[4] = (params->scaling_delta_cb[i] >> 16) & 0xff;
			table_ptr[5] = (params->scaling_delta_cb[i] >> 24) & 0xff;
			table_ptr[6] = params->scaling_points_cb[j][0];
			table_ptr[7] = params->scaling_points_cb[j][1];
			table_ptr[8] =
				(j == 9) ? 0 : ((params->scaling_delta_cb[j] >> 0) & 0xff);
			table_ptr[9] =
				(j == 9) ? 0 : ((params->scaling_delta_cb[j] >> 8) & 0xff);
			table_ptr[10] =
				(j == 9) ? 0 : ((params->scaling_delta_cb[j] >> 16) & 0xff);
			table_ptr[11] =
				(j == 9) ? 0 : ((params->scaling_delta_cb[j] >> 24) & 0xff);
			table_ptr[12] = 0;
			table_ptr[13] = 0;
			table_ptr[14] = 0;
			table_ptr[15] = 0;
			table_ptr += 16;
		}
	} else {
		memset(table_ptr, 0, 16 * 5 * sizeof(char));
		table_ptr += 16 * 5;
	}
	total += 5;
	if (params->num_cr_points > 0) {
		for (i = 0; i < 10; i += 2) {
			j = i + 1;
			table_ptr[0] = params->scaling_points_cr[i][0];
			table_ptr[1] = params->scaling_points_cr[i][1];
			//delta, it be signed, so 25bit
			table_ptr[2] = (params->scaling_delta_cr[i] >> 0) & 0xff;
			table_ptr[3] = (params->scaling_delta_cr[i] >> 8) & 0xff;
			table_ptr[4] = (params->scaling_delta_cr[i] >> 16) & 0xff;
			table_ptr[5] = (params->scaling_delta_cr[i] >> 24) & 0xff;
			table_ptr[6] = params->scaling_points_cr[j][0];
			table_ptr[7] = params->scaling_points_cr[j][1];
			table_ptr[8] =
				(j == 9) ? 0 : ((params->scaling_delta_cr[j] >> 0) & 0xff);
			table_ptr[9] =
				(j == 9) ? 0 : ((params->scaling_delta_cr[j] >> 8) & 0xff);
			table_ptr[10] =
				(j == 9) ? 0 : ((params->scaling_delta_cr[j] >> 16) & 0xff);
			table_ptr[11] =
				(j == 9) ? 0 : ((params->scaling_delta_cr[j] >> 24) & 0xff);
			table_ptr[12] = 0;
			table_ptr[13] = 0;
			table_ptr[14] = 0;
			table_ptr[15] = 0;
			table_ptr += 16;
		}
	} else {
		memset(table_ptr, 0, 16 * 5 * sizeof(char));
		table_ptr += 16 * 5;
	}
	//total += 5;
	//total += 480;//320;
}

void av1_add_film_grain_run(char *fg_table_buf, struct aom_film_grain_t *params)
{
	s32 **pred_pos_luma;
	s32 **pred_pos_chroma;
	s32 *luma_grain_block;
	s32 *cb_grain_block;
	s32 *cr_grain_block;
	s32 luma_block_size_y, luma_block_size_x;
	s32 chroma_block_size_y, chroma_block_size_x;
	s32 chroma_subblock_size_y, chroma_subblock_size_x;
	s32 chroma_subsamp_x = 1;
	s32 chroma_subsamp_y = 1;

	ulong start_time, step_time;
	u32 time1, time2, time3, time4, time5, time6;

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME)
		start_time = step_time = local_clock();

	chroma_subblock_size_y = luma_subblock_size_y >> chroma_subsamp_y;
	chroma_subblock_size_x = luma_subblock_size_x >> chroma_subsamp_x;

	// Initial padding is only needed for generation of
	// film grain templates (to stabilize the AR process)
	// Only a 64x64 luma and 32x32 chroma part of a template
	// is used later for adding grain, padding can be discarded

	luma_block_size_y = top_pad + 2 * ar_padding +
		luma_subblock_size_y * 2 + bottom_pad;
	luma_block_size_x = left_pad + 2 * ar_padding +
		luma_subblock_size_x * 2 + 2 * ar_padding + right_pad;

	chroma_block_size_y = top_pad + (2 >> chroma_subsamp_y) * ar_padding +
		chroma_subblock_size_y * 2 + bottom_pad;
	chroma_block_size_x = left_pad + (2 >> chroma_subsamp_x) * ar_padding +
		chroma_subblock_size_x * 2 +
		(2 >> chroma_subsamp_x) * ar_padding + right_pad;

	init_arrays(params, &pred_pos_luma,
		&pred_pos_chroma, &luma_grain_block,
		&cb_grain_block, &cr_grain_block,
		luma_block_size_y * luma_block_size_x,
		luma_block_size_x,
		chroma_block_size_y * chroma_block_size_x,
		chroma_block_size_x);

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME) {
		time1 = div64_u64(local_clock() - step_time, 1000);
		step_time = local_clock();
	}

	generate_luma_grain_block(params,
		pred_pos_luma, luma_grain_block,
		luma_block_size_y, luma_block_size_x,
		luma_block_size_x, fg_table_buf);

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME) {
		time2 = div64_u64(local_clock() - step_time, 1000);
		step_time = local_clock();
	}

	generate_chroma_grain_blocks(params,
		pred_pos_chroma, luma_grain_block,
		cb_grain_block, cr_grain_block,
		luma_block_size_x,
		chroma_block_size_y, chroma_block_size_x,
		chroma_block_size_x,
		chroma_subsamp_y, chroma_subsamp_x,
		fg_table_buf);

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME) {
		time3 = div64_u64(local_clock() - step_time, 1000);
		step_time = local_clock();
	}

	init_scaling_function(
		params->scaling_points_y,
		params->num_y_points,
		params->scaling_delta_y);

	if (!params->chroma_scaling_from_luma) {
		init_scaling_function(
			params->scaling_points_cb,
			params->num_cb_points,
			params->scaling_delta_cb);
		init_scaling_function(
			params->scaling_points_cr,
			params->num_cr_points,
			params->scaling_delta_cr);
	}

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME) {
		time4 = div64_u64(local_clock() - step_time, 1000);
		step_time = local_clock();
	}

	convert_to_fg_table(params, fg_table_buf);

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME) {
		time5 = div64_u64(local_clock() - step_time, 1000);
		step_time = local_clock();
	}

	dealloc_arrays(params,
		&pred_pos_luma, &pred_pos_chroma,
		&luma_grain_block,
		&cb_grain_block, &cr_grain_block);

	if (debug_fgs & DEBUG_FGS_CONSUME_TIME) {
		time6 = div64_u64(local_clock() - step_time, 1000);
		pr_info("fgs consume time %ld (%d, %d, %d, %d, %d, %d)us\n",
			(ulong)(div64_u64(local_clock() - start_time, 1000)), time1, time2, time3, time4, time5, time6);
	}
}

/* Film Grain Entry */
#if 0
int pic_film_grain_run(char *fg_table_addr, u32 fgs_ctrl, u32 *fgs_data)
#endif
int pic_film_grain_run(u32 frame_count, char *fg_table_addr, u32 fgs_ctrl, u32 *fgs_data)

{
	struct aom_film_grain_t fg_params;

	if (debug_fgs & DEBUG_FGS_REGS) {
		int i;
		pr_info("film grain ctrl: 0x%08x\n", fgs_ctrl);
		for (i = 0; i < FILM_GRAIN_REG_SIZE; i++) {
			pr_info("film grain reg[%02d]: 0x%08x\n", i, fgs_data[i]);
		}
	}

	film_grain_data_parse(&fg_params, fgs_ctrl, fgs_data);

	if (debug_fgs & DEBUG_FGS_REGS_PARSE) {
		fg_info_print(&fg_params);
	}

	av1_add_film_grain_run(fg_table_addr, &fg_params);

	if (debug_fgs & DEBUG_FGS_TABLE_DUMP) {
		#define FGS_TABLE_SIZE  (512 * 128 / 8)
		struct file *fg_fp = NULL;
		char file[256] = {0};

		snprintf(file, sizeof(file), "/data/tmp/fgs_table_%d.bin", frame_count - 1);
		fg_fp = filp_open(file, O_CREAT | O_RDWR | O_TRUNC, 0666);
		if (IS_ERR(fg_fp)) {
			fg_fp = NULL;
			printk(KERN_ERR"open %s failed\n", file);
		} else {
			kernel_write(fg_fp, fg_table_addr, FGS_TABLE_SIZE, &fg_fp->f_pos);
			filp_close(fg_fp, current->files);
			fg_fp = NULL;
		}
	}
	return 0;
}
EXPORT_SYMBOL(pic_film_grain_run);

int get_debug_fgs(void)
{
	return debug_fgs;
}
EXPORT_SYMBOL(get_debug_fgs);

