/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vicp_process.h"

#define VICP_CRC0_CHECK_FLAG0    0x6a740299    //mif->mif
#define VICP_CRC0_CHECK_FLAG1    0x12f487cf   //mif->fbc
#define VICP_CRC0_CHECK_FLAG2    0xb1e2e03c   //fbc->mif

#define VICP_CRC1_CHECK0_FLAG    0xa7d54a84
#define VICP_CRC1_CHECK1_FLAG    0xa7d54a84

/* *********************************************************************** */
/* ************************* function definitions ****************************.*/
/* *********************************************************************** */
int vicp_test(void);
int create_rgb24_colorbar(int width, int height, int bar_count);
int rgb24_to_yuv420p(char *yuv_file, char *rgb_file, int width, int height);
int rgb24_to_nv12(char *nv12_file, char *rgb_file, int width, int height);
int create_nv12_colorbar_file(int width, int height, int bar_count);
int create_nv12_colorbar_buf(u8 *addr, int width, int height, int bar_count);
