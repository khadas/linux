/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_DRM_IDS_H_
#define _MESON_DRM_IDS_H_

#define OSD1_BLOCK 0
#define OSD2_BLOCK 1
#define OSD3_BLOCK 2
#define OSD4_BLOCK 3
#define AFBC_OSD1_BLOCK 4
#define AFBC_OSD2_BLOCK 5
#define AFBC_OSD3_BLOCK 6
#define AFBC1_BLOCK 4
#define AFBC2_BLOCK 5
#define AFBC3_BLOCK 6
#define SCALER_OSD1_BLOCK 7
#define SCALER_OSD2_BLOCK 8
#define SCALER_OSD3_BLOCK 9
#define SCALER_OSD4_BLOCK 10
#define SCALER1_BLOCK 7
#define SCALER2_BLOCK 8
#define SCALER3_BLOCK 9
#define SCALER4_BLOCK 10
#define OSD_BLEND_BLOCK 11
#define OSD1_HDR_BLOCK 12
#define HDR1_BLOCK 12
#define HDR2_BLOCK 13
#define VPP_POSTBLEND_BLOCK 14
#define VPP1_POSTBLEND_BLOCK 14
#define VPP2_POSTBLEND_BLOCK 15
#define VPP3_POSTBLEND_BLOCK 16
#define VIDEO1_BLOCK 17
#define VIDEO2_BLOCK 18
#define VIDEO3_BLOCK 19
#define VIU2_OSD1_BLOCK 20
#define VPP2_BLOCK 21
#define HDR3_BLOCK 22
#define SLICE2PPC_BLOCK 23
#define BLOCK_ID_MAX 29

#define OSD1_PORT 0
#define OSD2_PORT 1
#define OSD3_PORT 2
#define AFBC_OSD1_PORT 3
#define AFBC_OSD2_PORT 4
#define AFBC_OSD3_PORT 5
#define SCALER_OSD1_PORT 6
#define SCALER_OSD2_PORT 7
#define SCALER_OSD3_PORT 8
#define OSDBLEND_DIN1_PORT 9
#define OSDBLEND_DIN2_PORT 10
#define OSDBLEND_DIN3_PORT 11
#define OSDBLEND_DOUT1_PORT 12
#define OSDBLEND_DOUT2_PORT 13
#define OSD1_HDR_PORT 14
#define OSD1_DOLBY_PORT 15
#define VPP_POSTBLEND_OSD1_IN_PORT 16
#define VPP_POSTBLEND_OSD2_IN_PORT 17
#define VPP_POSTBLEND_OUT_PORT 18
#define VIDEO1_PORT 19
#define VIDEO2_PORT 20

/*
 * vpu block type
 *
 */
/*
 *#define MESON_BLK_OSD 0
 *#define MESON_BLK_AFBC 1
 *#define MESON_BLK_SCALER 2
 *#define MESON_BLK_OSDBLEND 3
 *#define MESON_BLK_HDR 4
 *#define MESON_BLK_DOVI 5
 *#define MESON_BLK_VPPBLEND 6
 *#define MESON_BLK_VIDEO 7
 *#define MESON_BLK_SLICE2PPC 8
 */

/*
 *OSD VERSION
 *V1:G12A
 *V2:G12B-revA:base G12A,add repeate last line function
 *V3:G12B-RevB:add line interrupt support
 *V4:TL1:base G12A,fix shift issue,delete osd3
 *V5:SM1:base G12A,fix shift issue
 *V6:TM2,base G12A,fix shift issue,add mux for osd1-sr
 */
#define OSD_V1 1
#define OSD_V2 2
#define OSD_V3 3
#define OSD_V4 4
#define OSD_V5 5
#define OSD_V6 6
#define OSD_V7 7

#endif
