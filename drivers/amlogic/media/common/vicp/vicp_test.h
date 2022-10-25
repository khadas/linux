/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vicp_process.h"
extern u32 input_width;
extern u32 input_height;
extern u32 output_width;
extern u32 output_height;
extern u32 input_color_format;    //0:yuv444 1:yuv422 2:yuv420
extern u32 output_color_format;   //0:yuv420 1:yuv422 2:yuv444
extern u32 input_color_dep;
extern u32 output_color_dep;
/* *********************************************************************** */
/* ************************* function definitions ****************************.*/
/* *********************************************************************** */
int vicp_test(void);
