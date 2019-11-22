#ifndef _MESON_DRM_IDS_H_
#define _MESON_DRM_IDS_H_

#define OSD1_BLOCK 0
#define OSD2_BLOCK 1
#define OSD3_BLOCK 2
#define AFBC_OSD1_BLOCK 3
#define AFBC_OSD2_BLOCK 4
#define AFBC_OSD3_BLOCK 5
#define SCALER_OSD1_BLOCK 6
#define SCALER_OSD2_BLOCK 7
#define SCALER_OSD3_BLOCK 8
#define OSD_BLEND_BLOCK 9
#define OSD1_HDR_BLOCK 10
#define VPP_POSTBLEND_BLOCK 11
#define BLOCK_ID_MAX 12

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

#endif
