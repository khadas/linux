/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
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

#ifndef _ACAMERA_COMMAND_API_H_
#define _ACAMERA_COMMAND_API_H_
#include "acamera_types.h"


// ------------------------------------------------------------------------------ //
//		ERROR REASONS
// ------------------------------------------------------------------------------ //
#define ERR_UNKNOWN                                       0x00000000
#define ERR_BAD_ARGUMENT                                  0x00000001
#define ERR_WRONG_SIZE                                    0x00000002


// ------------------------------------------------------------------------------ //
//		WDR MODES
// ------------------------------------------------------------------------------ //
#define WDR_MODE_LINEAR                                   0x00000000
#define WDR_MODE_NATIVE                                   0x00000001
#define WDR_MODE_FS_LIN                                   0x00000002
#define WDR_MODE_COUNT                                    0x00000003


// ------------------------------------------------------------------------------ //
//		STATIC CALIBRATION VALUES
// ------------------------------------------------------------------------------ //
#define CALIBRATION_LIGHT_SRC                             0x00000000
#define CALIBRATION_RG_POS                                0x00000001
#define CALIBRATION_BG_POS                                0x00000002
#define CALIBRATION_MESH_RGBG_WEIGHT                      0x00000003
#define CALIBRATION_MESH_LS_WEIGHT                        0x00000004
#define CALIBRATION_MESH_COLOR_TEMPERATURE                0x00000005
#define CALIBRATION_WB_STRENGTH                           0x00000006
#define CALIBRATION_SKY_LUX_TH                            0x00000007
#define CALIBRATION_CT_RG_POS_CALC                        0x00000008
#define CALIBRATION_CT_BG_POS_CALC                        0x00000009
#define CALIBRATION_COLOR_TEMP                            0x0000000A
#define CALIBRATION_CT65POS                               0x0000000B
#define CALIBRATION_CT40POS                               0x0000000C
#define CALIBRATION_CT30POS                               0x0000000D
#define CALIBRATION_EVTOLUX_EV_LUT                        0x0000000E
#define CALIBRATION_EVTOLUX_LUX_LUT                       0x0000000F
#define CALIBRATION_BLACK_LEVEL_R                         0x00000010
#define CALIBRATION_BLACK_LEVEL_GR                        0x00000011
#define CALIBRATION_BLACK_LEVEL_GB                        0x00000012
#define CALIBRATION_BLACK_LEVEL_B                         0x00000013
#define CALIBRATION_STATIC_WB                             0x00000014
#define CALIBRATION_MT_ABSOLUTE_LS_A_CCM                  0x00000015
#define CALIBRATION_MT_ABSOLUTE_LS_D40_CCM                0x00000016
#define CALIBRATION_MT_ABSOLUTE_LS_D50_CCM                0x00000017
#define CALIBRATION_SHADING_LS_A_R                        0x00000018
#define CALIBRATION_SHADING_LS_A_G                        0x00000019
#define CALIBRATION_SHADING_LS_A_B                        0x0000001A
#define CALIBRATION_SHADING_LS_TL84_R                     0x0000001B
#define CALIBRATION_SHADING_LS_TL84_G                     0x0000001C
#define CALIBRATION_SHADING_LS_TL84_B                     0x0000001D
#define CALIBRATION_SHADING_LS_D65_R                      0x0000001E
#define CALIBRATION_SHADING_LS_D65_G                      0x0000001F
#define CALIBRATION_SHADING_LS_D65_B                      0x00000020
#define CALIBRATION_AWB_WARMING_LS_A                      0x00000021
#define CALIBRATION_AWB_WARMING_LS_D50                    0x00000022
#define CALIBRATION_AWB_WARMING_LS_D75                    0x00000023
#define CALIBRATION_NOISE_PROFILE                         0x00000024
#define CALIBRATION_DEMOSAIC                              0x00000025
#define CALIBRATION_GAMMA                                 0x00000026
#define CALIBRATION_IRIDIX_RGB2REC709                     0x00000027
#define CALIBRATION_IRIDIX_REC709TORGB                    0x00000028
#define CALIBRATION_IRIDIX_ASYMMETRY                      0x00000029
#define CALIBRATION_AWB_SCENE_PRESETS                     0x0000002A
#define CALIBRATION_SHADING_RADIAL_CENTRE_AND_MULT        0x0000002B
#define CALIBRATION_GAMMA_FE_0                            0x0000002C
#define CALIBRATION_GAMMA_FE_1                            0x0000002D
#define CALIBRATION_WDR_NP_LUT                            0x0000002E
#define CALIBRATION_CA_FILTER_MEM                         0x0000002F
#define CALIBRATION_CA_CORRECTION                         0x00000030
#define CALIBRATION_CA_CORRECTION_MEM                     0x00000031
#define CALIBRATION_LUT3D_MEM                             0x00000032
#define CALIBRATION_DECOMPANDER0_MEM                      0x00000033
#define CALIBRATION_DECOMPANDER1_MEM                      0x00000034
#define CALIBRATION_SHADING_RADIAL_R                      0x00000035
#define CALIBRATION_SHADING_RADIAL_G                      0x00000036
#define CALIBRATION_SHADING_RADIAL_B                      0x00000037


// ------------------------------------------------------------------------------ //
//		DYNAMIC CALIBRATION VALUES
// ------------------------------------------------------------------------------ //
#define CALIBRATION_STITCHING_LM_MED_NOISE_INTENSITY      0x00000038
#define AWB_COLOUR_PREFERENCE                             0x00000039
#define CALIBRATION_AWB_MIX_LIGHT_PARAMETERS              0x0000003A
#define CALIBRATION_PF_RADIAL_LUT                         0x0000003B
#define CALIBRATION_PF_RADIAL_PARAMS                      0x0000003C
#define CALIBRATION_SINTER_RADIAL_LUT                     0x0000003D
#define CALIBRATION_SINTER_RADIAL_PARAMS                  0x0000003E
#define CALIBRATION_AWB_BG_MAX_GAIN                       0x0000003F
#define CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL       0x00000040
#define CALIBRATION_CMOS_CONTROL                          0x00000041
#define CALIBRATION_STATUS_INFO                           0x00000042
#define CALIBRATION_AUTO_LEVEL_CONTROL                    0x00000043
#define CALIBRATION_DP_SLOPE                              0x00000044
#define CALIBRATION_DP_THRESHOLD                          0x00000045
#define CALIBRATION_STITCHING_LM_MOV_MULT                 0x00000046
#define CALIBRATION_STITCHING_LM_NP                       0x00000047
#define CALIBRATION_STITCHING_MS_MOV_MULT                 0x00000048
#define CALIBRATION_STITCHING_MS_NP                       0x00000049
#define CALIBRATION_STITCHING_SVS_MOV_MULT                0x0000004A
#define CALIBRATION_STITCHING_SVS_NP                      0x0000004B
#define CALIBRATION_EVTOLUX_PROBABILITY_ENABLE            0x0000004C
#define CALIBRATION_AWB_AVG_COEF                          0x0000004D
#define CALIBRATION_IRIDIX_AVG_COEF                       0x0000004E
#define CALIBRATION_IRIDIX_STRENGTH_MAXIMUM               0x0000004F
#define CALIBRATION_IRIDIX_MIN_MAX_STR                    0x00000050
#define CALIBRATION_IRIDIX_EV_LIM_FULL_STR                0x00000051
#define CALIBRATION_IRIDIX_EV_LIM_NO_STR                  0x00000052
#define CALIBRATION_AE_CORRECTION                         0x00000053
#define CALIBRATION_AE_EXPOSURE_CORRECTION                0x00000054
#define CALIBRATION_SINTER_STRENGTH                       0x00000055
#define CALIBRATION_SINTER_STRENGTH1                      0x00000056
#define CALIBRATION_SINTER_THRESH1                        0x00000057
#define CALIBRATION_SINTER_THRESH4                        0x00000058
#define CALIBRATION_SINTER_INTCONFIG                      0x00000059
#define CALIBRATION_SHARP_ALT_D                           0x0000005A
#define CALIBRATION_SHARP_ALT_UD                          0x0000005B
#define CALIBRATION_SHARP_ALT_DU                          0x0000005C
#define CALIBRATION_DEMOSAIC_NP_OFFSET                    0x0000005D
#define CALIBRATION_MESH_SHADING_STRENGTH                 0x0000005E
#define CALIBRATION_SATURATION_STRENGTH                   0x0000005F
#define CALIBRATION_CCM_ONE_GAIN_THRESHOLD                0x00000060
#define CALIBRATION_RGB2YUV_CONVERSION                    0x00000061
#define CALIBRATION_AE_ZONE_WGHT_HOR                      0x00000062
#define CALIBRATION_AE_ZONE_WGHT_VER                      0x00000063
#define CALIBRATION_AWB_ZONE_WGHT_HOR                     0x00000064
#define CALIBRATION_AWB_ZONE_WGHT_VER                     0x00000065
#define CALIBRATION_SHARPEN_FR                            0x00000066
#define CALIBRATION_SHARPEN_DS1                           0x00000067
#define CALIBRATION_CUSTOM_SETTINGS_CONTEXT               0x00000068
#define CALIBRATION_CMOS_EXPOSURE_PARTITION_LUTS          0x00000069
#define CALIBRATION_TEMPER_STRENGTH                       0x0000006A
#define CALIBRATION_NP_LUT_MEAN                           0x0000006B
#define CALIBRATION_SCALER_H_FILTER                       0x0000006C
#define CALIBRATION_SCALER_V_FILTER                       0x0000006D
#define CALIBRATION_SINTER_STRENGTH_MC_CONTRAST           0x0000006E
#define CALIBRATION_EXPOSURE_RATIO_ADJUSTMENT             0x0000006F
#define CALIBRATION_CNR_UV_DELTA12_SLOPE                  0x00000070
#define CALIBRATION_FS_MC_OFF                             0x00000071
#define CALIBRATION_SINTER_SAD                            0x00000072
#define CALIBRATION_AF_LMS                                0x00000073
#define CALIBRATION_AF_ZONE_WGHT_HOR                      0x00000074
#define CALIBRATION_AF_ZONE_WGHT_VER                      0x00000075
#define CALIBRATION_AE_CONTROL_HDR_TARGET                 0x00000076
#define CALIBRATION_AE_CONTROL                            0x00000077
#define CALIBRATION_GAMMA_EV1                             0x00000078
#define CALIBRATION_GAMMA_EV2                             0x00000079
#define CALIBRATION_GAMMA_THRESHOLD                       0x0000007A
#define CALIBRATION_DEFOG_CONTROL                         0x0000007B
#define CALIBRATION_DEMOSAIC_RGB_EXT_CONTROL              0x0000007C
#define CALIBRATION_FR_SHARPEN_EXT_CONTROL                0x0000007D
#define CALIBRATION_DS_SHARPEN_EXT_CONTROL                0x0000007E
#define CALIBRATION_CNR_EXT_CONTROL                       0x0000007F
#define CALIBRATION_IRIDIX_EXT_CONTROL                    0x00000080
#define CALIBRATION_SQRT_EXT_CONTROL                      0x00000081
#define CALIBRATION_SQUARE_BE_EXT_CONTROL                 0x00000082
#define CALIBRATION_3AALG_AE_CONTROL                      0x00000083
#define CALIBRATION_3AALG_AWB_CONTROL                     0x00000084
#define CALIBRATION_3AALG_GAMMA_CONTROL                   0x00000085
#define CALIBRATION_3AALG_IRIDIX_CONTROL                  0x00000086
#define CALIBRATION_DP_DEVTHRESHOLD                       0x00000087
#define CALIBRATION_PF_CORRECTION                         0x00000088
#define CALIBRATION_FC_CORRECTION                         0x00000089
#define CALIBRATION_DAYNIGHT_DETECT                       0x0000008A
#define CALIBRATION_TEMPER_MD_STRENGTH                    0x0000008B
#define CALIBRATION_TEMPER_MD_MODE                        0x0000008C
#define CALIBRATION_TEMPER2_STRENGTH                      0x0000008E

// ------------------------------------------------------------------------------ //
//		DYNAMIC STATE VALUES
// ------------------------------------------------------------------------------ //

#define CALIBRATION_TOTAL_SIZE 141


//------------------FILE TRANSFER-------------------


// ------------------------------------------------------------------------------ //
//		COMMAND TYPE LIST
// ------------------------------------------------------------------------------ //
#define TGENERAL                                          0x00000000
#define TSELFTEST                                         0x00000001
#define TSENSOR                                           0x00000002
#define TLENS                                             0x00000003
#define TSYSTEM                                           0x00000004
#define TISP_MODULES                                      0x00000005
#define TSTATUS                                           0x00000006
#define TIMAGE                                            0x00000007
#define TALGORITHMS                                       0x00000008
#define TSCENE_MODES                                      0x00000009
#define TREGISTERS                                        0x0000000A
#define TAML_SCALER                                       0x0000000B

// ------------------------------------------------------------------------------ //
//		BUFFER TYPES
// ------------------------------------------------------------------------------ //
#define STATIC_CALIBRATIONS_ID                            0x00000000
#define DYNAMIC_CALIBRATIONS_ID                           0x00000001
#define FILE_TRANSFER_ID                                  0x00000002
#define DYNAMIC_STATE_ID                                  0x00000003


// ------------------------------------------------------------------------------ //
//		COMMAND LIST
// ------------------------------------------------------------------------------ //
#define CONTEXT_NUMBER                                    0x00000000
#define ACTIVE_CONTEXT                                    0x00000001
#define FW_REVISION                                       0x00000002
#define SENSOR_STREAMING                                  0x00000003
#define SENSOR_SUPPORTED_PRESETS                          0x00000004
#define SENSOR_PRESET                                     0x00000005
#define SENSOR_WDR_MODE                                   0x00000006
#define SENSOR_FPS                                        0x00000007
#define SENSOR_WIDTH                                      0x00000008
#define SENSOR_HEIGHT                                     0x00000009
#define SENSOR_EXPOSURES                                  0x0000000A
#define SENSOR_INFO_PRESET                                0x0000000B
#define SENSOR_INFO_WDR_MODE                              0x0000000C
#define SENSOR_INFO_FPS                                   0x0000000D
#define SENSOR_INFO_WIDTH                                 0x0000000E
#define SENSOR_INFO_HEIGHT                                0x0000000F
#define SENSOR_INFO_EXPOSURES                             0x00000010
#define SENSOR_LINES_PER_SECOND                           0x00000011
#define SENSOR_INTEGRATION_TIME_MIN                       0x00000012
#define SENSOR_INTEGRATION_TIME_LIMIT                     0x00000013
#define SENSOR_INFO_PHYSICAL_WIDTH                        0x00000014
#define SENSOR_INFO_PHYSICAL_HEIGHT                       0x00000015
#define LENS_INFO_MINFOCUS_DISTANCE                       0x00000016
#define LENS_INFO_HYPERFOCAL_DISTANCE                     0x00000017
#define LENS_INFO_FOCAL_LENGTH                            0x00000018
#define LENS_INFO_APERTURE                                0x00000019
#define SYSTEM_LOGGER_LEVEL                               0x0000001A
#define SYSTEM_LOGGER_MASK                                0x0000001B
#define BUFFER_DATA_TYPE                                  0x0000001C
#define TEST_PATTERN_ENABLE_ID                            0x0000001D
#define TEST_PATTERN_MODE_ID                              0x0000001E
#define TEMPER_MODE_ID                                    0x0000001F
#define SYSTEM_FREEZE_FIRMWARE                            0x00000020
#define SYSTEM_MANUAL_EXPOSURE                            0x00000021
#define SYSTEM_MANUAL_EXPOSURE_RATIO                      0x00000022
#define SYSTEM_MANUAL_INTEGRATION_TIME                    0x00000023
#define SYSTEM_MANUAL_MAX_INTEGRATION_TIME                0x00000024
#define SYSTEM_MANUAL_SENSOR_ANALOG_GAIN                  0x00000025
#define SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN                 0x00000026
#define SYSTEM_MANUAL_ISP_DIGITAL_GAIN                    0x00000027
#define SYSTEM_MANUAL_AWB                                 0x00000028
#define SYSTEM_MANUAL_CCM                                 0x00000029
#define SYSTEM_MANUAL_SATURATION                          0x0000002A
#define SYSTEM_EXPOSURE                                   0x0000002B
#define SYSTEM_EXPOSURE_RATIO                             0x0000002C
#define SYSTEM_MAX_EXPOSURE_RATIO                         0x0000002D
#define SYSTEM_INTEGRATION_TIME                           0x0000002E
#define SYSTEM_LONG_INTEGRATION_TIME                      0x0000002F
#define SYSTEM_SHORT_INTEGRATION_TIME                     0x00000030
#define SYSTEM_MAX_INTEGRATION_TIME                       0x00000031
#define SYSTEM_SENSOR_ANALOG_GAIN                         0x00000032
#define SYSTEM_MAX_SENSOR_ANALOG_GAIN                     0x00000033
#define SYSTEM_SENSOR_DIGITAL_GAIN                        0x00000034
#define SYSTEM_MAX_SENSOR_DIGITAL_GAIN                    0x00000035
#define SYSTEM_ISP_DIGITAL_GAIN                           0x00000036
#define SYSTEM_MAX_ISP_DIGITAL_GAIN                       0x00000037
#define SYSTEM_AWB_RED_GAIN                               0x00000038
#define SYSTEM_AWB_GREEN_EVEN_GAIN                        0x00000039
#define SYSTEM_AWB_GREEN_ODD_GAIN                         0x0000003A
#define SYSTEM_AWB_BLUE_GAIN                              0x0000003B
#define SYSTEM_CCM_MATRIX_RR                              0x0000003C
#define SYSTEM_CCM_MATRIX_RG                              0x0000003D
#define SYSTEM_CCM_MATRIX_RB                              0x0000003E
#define SYSTEM_CCM_MATRIX_GR                              0x0000003F
#define SYSTEM_CCM_MATRIX_GG                              0x00000040
#define SYSTEM_CCM_MATRIX_GB                              0x00000041
#define SYSTEM_CCM_MATRIX_BR                              0x00000042
#define SYSTEM_CCM_MATRIX_BG                              0x00000043
#define SYSTEM_CCM_MATRIX_BB                              0x00000044
#define SYSTEM_SATURATION_TARGET                          0x00000045
#define SYSTEM_ANTIFLICKER_ENABLE                         0x00000046
#define SYSTEM_ANTI_FLICKER_FREQUENCY                     0x00000047
#define SYSTEM_DYNAMIC_GAMMA_ENABLE                       0x00000048
#define CALIBRATION_UPDATE                                0x00000049
#define ISP_MODULES_MANUAL_IRIDIX                         0x0000004A
#define ISP_MODULES_MANUAL_SINTER                         0x0000004B
#define ISP_MODULES_MANUAL_TEMPER                         0x0000004C
#define ISP_MODULES_MANUAL_AUTO_LEVEL                     0x0000004D
#define ISP_MODULES_MANUAL_FRAME_STITCH                   0x0000004E
#define ISP_MODULES_MANUAL_RAW_FRONTEND                   0x0000004F
#define ISP_MODULES_MANUAL_BLACK_LEVEL                    0x00000050
#define ISP_MODULES_MANUAL_SHADING                        0x00000051
#define ISP_MODULES_MANUAL_DEMOSAIC                       0x00000052
#define ISP_MODULES_MANUAL_CNR                            0x00000053
#define ISP_MODULES_MANUAL_SHARPEN                        0x00000054
#define STATUS_INFO_EXPOSURE_LOG2_ID                      0x00000055
#define STATUS_INFO_GAIN_ONES_ID                          0x00000056
#define STATUS_INFO_GAIN_LOG2_ID                          0x00000057
#define STATUS_INFO_AWB_MIX_LIGHT_CONTRAST                0x00000058
#define STATUS_INFO_AF_LENS_POS                           0x00000059
#define STATUS_INFO_AF_FOCUS_VALUE                        0x0000005A
#define DMA_READER_OUTPUT_ID                              0x0000005B
#define FR_FORMAT_BASE_PLANE_ID                           0x0000005C
#define DS1_FORMAT_BASE_PLANE_ID                          0x0000005D
#define ORIENTATION_VFLIP_ID                              0x0000005E
#define ORIENTATION_HFLIP_ID                              0x0000005F
#define IMAGE_RESIZE_TYPE_ID                              0x00000060
#define IMAGE_RESIZE_ENABLE_ID                            0x00000061
#define IMAGE_RESIZE_WIDTH_ID                             0x00000062
#define IMAGE_RESIZE_HEIGHT_ID                            0x00000063
#define IMAGE_CROP_XOFFSET_ID                             0x00000064
#define IMAGE_CROP_YOFFSET_ID                             0x00000065
#define AF_LENS_STATUS                                    0x00000066
#define AF_MODE_ID                                        0x00000067
#define AF_RANGE_LOW_ID                                   0x00000068
#define AF_RANGE_HIGH_ID                                  0x00000069
#define AF_STATE_ID                                       0x0000006A
#define AF_ROI_ID                                         0x0000006B
#define AF_MANUAL_CONTROL_ID                              0x0000006C
#define AE_MODE_ID                                        0x0000006D
#define AE_STATE_ID                                       0x0000006E
#define AE_SPLIT_PRESET_ID                                0x0000006F
#define AE_GAIN_ID                                        0x00000070
#define AE_EXPOSURE_ID                                    0x00000071
#define AE_ROI_ID                                         0x00000072
#define AE_COMPENSATION_ID                                0x00000073
#define AE_STATS_ID                                       0x00000074
#define AWB_MODE_ID                                       0x00000075
#define AWB_STATE_ID                                      0x00000076
#define AWB_TEMPERATURE_ID                                0x00000077
#define ANTIFLICKER_MODE_ID                               0x00000078
#define NOISE_REDUCTION_MODE_ID                           0x00000079
#define COLOR_MODE_ID                                     0x0000007A
#define BRIGHTNESS_STRENGTH_ID                            0x0000007B
#define CONTRAST_STRENGTH_ID                              0x0000007C
#define SATURATION_STRENGTH_ID                            0x0000007D
#define SHARPENING_STRENGTH_ID                            0x0000007E
#define SHADING_STRENGTH_ID                               0x0000007F
#define REGISTERS_ADDRESS_ID                              0x00000080
#define REGISTERS_SIZE_ID                                 0x00000081
#define REGISTERS_SOURCE_ID                               0x00000082
#define REGISTERS_VALUE_ID                                0x00000083
#define SCALER_WIDTH                                      0x00000084
#define SCALER_HEIGHT                                     0x00000085
#define SCALER_OUTPUT_MODE                                0x00000086
#define SCALER_STREAMING_ON                               0x00000087
#define SCALER_STREAMING_OFF                              0x00000088
#define SENSOR_NAME                                       0x00000089
#define SENSOR_TESTPATTERN                                0x0000008A
#define SENSOR_IR_CUT                                     0x0000008B
#define SCALER_SRC_WIDTH                                  0x0000008C
#define SCALER_SRC_HEIGHT                                 0x0000008D
#define AE_ZONE_WEIGHT                                    0x0000008E
#define AWB_ZONE_WEIGHT                                   0x0000008F
#define SENSOR_HWID                                       0x00000090
#define HUE_THETA_ID                                      0x00000091
#define SNR_MANUAL_ID                                     0x00000092
#define SNR_STRENGTH_ID                                   0x00000093
#define TNR_MANUAL_ID                                     0x00000094
#define TNR_OFFSET_ID                                     0x00000095
#define SENSOR_WDRMODE_ID                                 0x00000096
#define DEFOG_MODE_ID                                     0x00000097
#define DEFOG_RATIO_DELTA                                 0x00000098
#define DEFOG_BLACK_PERCENTAGE                            0x00000099
#define DEFOG_WHITE_PERCENTAGE                            0x0000009A
#define SENSOR_ANTIFLICKER_ID                             0x0000009B
#define CALIBRATION_CUSTOMER                              0x0000009C
#define STATUS_INFO_EXPOSURE_CORRECTION_LOG2_ID           0x0000009D
#define STATUS_INFO_TOTAL_GAIN_LOG2_ID                    0x0000009E
#define SENSOR_VMAX_FPS                                   0x0000009F
#define AWB_STATS_ID                                      0x000000A0
#define ISP_DPC_THRES_SLOPE                               0x000000A1
#define ISP_BLACK_LEVEL_R                                 0x000000A2
#define ISP_BLACK_LEVEL_GR                                0x000000A3
#define ISP_BLACK_LEVEL_GB                                0x000000A4
#define ISP_BLACK_LEVEL_B                                 0x000000A5
#define ISP_DEMOSAIC_SHARP                                0x000000A6
#define ISP_CNR_STRENGTH                                  0x000000A7
#define ISP_MODULES_IRIDIX_ENABLE                         0x000000A8
#define ISP_MODULES_IRIDIX_STRENGTH                       0x000000A9
#define ISP_MODULES_FR_SHARPEN_STRENGTH                   0x000000AA
#define ISP_MODULES_DS1_SHARPEN_STRENGTH                  0x000000AB
#define DAYNIGHT_DETECT_ID                                0x000000AC
#define STATUS_INFO_TOTAL_GAIN_DB_ID                      0x000000AD
#define STATUS_INFO_CMOS_AGAIN_DB_ID                      0x000000AE
#define STATUS_INFO_CMOS_DGAIN_DB_ID                      0x000000AF
#define STATUS_INFO_ISP_DGAIN_DB_ID                       0x000000B0
#define ISP_MODULES_MANUAL_PF                             0x000000B1
#define STATUS_INFO_ISO_ID                                0x000000B2
#define AF_STATS_ID                                       0x000000B3


// ------------------------------------------------------------------------------ //
//		VALUE LIST
// ------------------------------------------------------------------------------ //
#define OFF                                               0x00000000
#define ON                                                0x00000001
#define DEBUG                                             0x00000005
#define INFO                                              0x00000006
#define NOTICE                                            0x00000007
#define WARNING                                           0x00000008
#define ERROR                                             0x00000009
#define CRITICAL                                          0x0000000A
#define NOTHING                                           0x0000000B
#define TEMPER3_MODE                                      0x0000000C
#define TEMPER2_MODE                                      0x0000000D
#define UPDATE                                            0x0000000E
#define DONE                                              0x0000000F
#define DMA_READER_OUT_FR                                 0x00000010
#define DMA_READER_OUT_DS                                 0x00000011
#define DMA_DISABLE                                       0x00000012
#define RGB32                                             0x00000013
#define A2R10G10B10                                       0x00000014
#define RGB565                                            0x00000015
#define RGB24                                             0x00000016
#define GEN32                                             0x00000017
#define RAW16                                             0x00000018
#define RAW12                                             0x00000019
#define AYUV                                              0x0000001A
#define Y410                                              0x0000001B
#define YUY2                                              0x0000001C
#define UYVY                                              0x0000001D
#define Y210                                              0x0000001E
#define NV12_YUV                                          0x0000001F
#define NV12_YVU                                          0x00000020
#define YV12_YU                                           0x00000021
#define YV12_YV                                           0x00000022
#define ENABLE                                            0x00000023
#define DISABLE                                           0x00000024
#define CROP_FR                                           0x00000025
#define CROP_DS                                           0x00000026
#define SCALER                                            0x00000027
#define RUN                                               0x00000028
#define LENS_SUCCESS                                      0x00000029
#define LENS_FAILED                                       0x0000002A
#define AF_AUTO_SINGLE                                    0x0000002B
#define AF_AUTO_CONTINUOUS                                0x0000002C
#define AF_MANUAL                                         0x0000002D
#define AF_CALIBRATION                                    0x0000002E
#define AF_INACTIVE                                       0x0000002F
#define AF_SCAN                                           0x00000030
#define AF_FOCUSED                                        0x00000031
#define AF_UNFOCUSED                                      0x00000032
#define AE_AUTO                                           0x00000033
#define AE_FULL_MANUAL                                    0x00000034
#define AE_MANUAL_GAIN                                    0x00000035
#define AE_MANUAL_EXPOSURE_TIME                           0x00000036
#define AE_INACTIVE                                       0x00000037
#define AE_SEARCHING                                      0x00000038
#define AE_CONVERGED                                      0x00000039
#define AE_SPLIT_BALANCED                                 0x0000003A
#define AE_SPLIT_INTEGRATION_PRIORITY                     0x0000003B
#define AWB_AUTO                                          0x0000003C
#define AWB_MANUAL                                        0x0000003D
#define AWB_DAY_LIGHT                                     0x0000003E
#define AWB_CLOUDY                                        0x0000003F
#define AWB_INCANDESCENT                                  0x00000040
#define AWB_FLOURESCENT                                   0x00000041
#define AWB_TWILIGHT                                      0x00000042
#define AWB_SHADE                                         0x00000043
#define AWB_WARM_FLOURESCENT                              0x00000044
#define AWB_INACTIVE                                      0x00000045
#define AWB_SEARCHING                                     0x00000046
#define AWB_CONVERGED                                     0x00000047
#define NOISE_REDUCTION_OFF                               0x00000048
#define NOISE_REDUCTION_ON                                0x00000049
#define NORMAL                                            0x0000004A
#define BLACK_AND_WHITE                                   0x0000004B
#define NEGATIVE                                          0x0000004C
#define SEPIA                                             0x0000004D
#define VIVID                                             0x0000004E
#define SENSOR                                            0x0000004F
#define LENS                                              0x00000050
#define ISP                                               0x00000051
#define NV12_GREY                                         0x00000052
#define DEFOG_DISABLE                                     0x00000053
#define DEFOG_ONLY                                        0x00000054
#define DEFOG_BLEND                                       0x00000055

// ------------------------------------------------------------------------------ //
//		RETURN VALUES
// ------------------------------------------------------------------------------ //
#define SUCCESS                                           0x00000000
#define NOT_IMPLEMENTED                                   0x00000001
#define NOT_SUPPORTED                                     0x00000002
#define NOT_PERMITTED                                     0x00000003
#define NOT_EXISTS                                        0x00000004
#define FAIL                                              0x00000005
#define IMPLEMENTED                                       0x00000006


// ------------------------------------------------------------------------------ //
//		DIRECTION VALUES
// ------------------------------------------------------------------------------ //
#define COMMAND_SET                                       0x00000000
#define COMMAND_GET                                       0x00000001
#define API_VERSION                                       0x00000064

#define NOTIFY_GET_QUEUE_STATUS                           0x00000001
#define NOTIFY_GET_ISP_CLKRATE                            0x00000002
#define NOTIFY_SET_SC01_EN                                0x00000003
#define NOTIFY_SET_SC23_EN                                0x00000004
#define NOTIFY_GET_SC03_STATUS                            0x00000005
#define NOTIFY_SET_SC03_STATUS                            0x00000006
#define NOTIFY_UPDATE_SC03_CAMID                          0x00000007
#define NOTIFY_RESET_SC03_STATUS                          0x00000008

#define SCMIF_IDLE                                        0x00000000
#define SCMIF_BUSY                                        0x000000AA
#define SCMIF_ERR                                         0x000000AB
// ------------------------------------------------------------------------------ //
//		SET/GET FUNCTION
// ------------------------------------------------------------------------------ //
//set command: ret = acamera_command( ctx_id, TALGORITHMS, AF_MODE, AF_AUTO, COMMAND_SET, &RET_VALUE);
//get command: ret = acamera_command( ctx_id, TALGORITHMS, AF_MODE, AF_AUTO, COMMAND_GET, &RET_VALUE);

//The main api function to control and change the firmware state
uint8_t acamera_command( uint32_t ctx_id, uint8_t command_type, uint8_t command, uint32_t value, uint8_t direction, uint32_t *ret_value);

//The function to change firmware internal calibrations.
uint8_t acamera_api_calibration( uint32_t ctx_id, uint8_t type, uint8_t id, uint8_t direction, void* data, uint32_t data_size, uint32_t* ret_value);

uint8_t acamera_api_dma_buffer( uint32_t ctx_id, uint8_t type, void* data, uint32_t data_size, uint32_t* ret_value, uint32_t index);
void acamera_api_dma_buff_queue_reset(uint32_t ctx_id, uint8_t type);
uint8_t acamera_api_set_fps(uint32_t ctx_id, uint8_t type, uint32_t c_fps, uint32_t t_fps);
void acamera_api_dma_buff_get_next(uint32_t ctx_id, uint8_t type);

#endif//_ACAMERA_COMMAND_API_H_
