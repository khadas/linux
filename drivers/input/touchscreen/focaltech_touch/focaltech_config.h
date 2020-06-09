/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2018, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/************************************************************************
*
* File Name: focaltech_config.h
*
*    Author: Focaltech Driver Team
*
*   Created: 2016-08-08
*
*  Abstract: global configurations
*
*   Version: v1.0
*
************************************************************************/
#ifndef _LINUX_FOCLATECH_CONFIG_H_
#define _LINUX_FOCLATECH_CONFIG_H_

/**************************************************/
/****** G: A, I: B, S: C, U: D  ******************/
/****** chip type defines, do not modify *********/
#define _FT8716             0x87160805
#define _FT8736             0x87360806
#define _FT8006M            0x80060807
#define _FT7250             0x72500807
#define _FT8606             0x86060808
#define _FT8607             0x86070809
#define _FTE716             0xE716080A
#define _FT8006U            0x8006D80B
#define _FT8613             0x8613080C
#define _FT8719             0x8719080D
#define _FT8739             0x8739080E
#define _FT8615             0x8615080F
#define _FT8201             0x8201081

#define _FT5416             0x54160402
#define _FT5426             0x54260402
#define _FT5435             0x54350402
#define _FT5436             0x54360402
#define _FT5526             0x55260402
#define _FT5526I            0x5526B402
#define _FT5446             0x54460402
#define _FT5346             0x53460402
#define _FT5446I            0x5446B402
#define _FT5346I            0x5346B402
#define _FT7661             0x76610402
#define _FT7511             0x75110402
#define _FT7421             0x74210402
#define _FT7681             0x76810402
#define _FT3C47U            0x3C47D402
#define _FT3417             0x34170402
#define _FT3517             0x35170402
#define _FT3327             0x33270402
#define _FT3427             0x34270402
#define _FT7311             0x73110402

#define _FT5626             0x56260401
#define _FT5726             0x57260401
#define _FT5826B            0x5826B401
#define _FT5826S            0x5826C401
#define _FT7811             0x78110401
#define _FT3D47             0x3D470401
#define _FT3617             0x36170401
#define _FT3717             0x37170401
#define _FT3817B            0x3817B401
#define _FT3517U            0x3517D401

#define _FT6236U            0x6236D003
#define _FT6336G            0x6336A003
#define _FT6336U            0x6336D003
#define _FT6436U            0x6436D003

#define _FT3267             0x32670004
#define _FT3367             0x33670004
#define _FT5422U            0x5422D482

#define _FT3327DQQ_001      0x3327D482
#define _FT5446DQS_W01      0x5446D482
#define _FT5446DQS_W02      0x5446A482
#define _FT5446DQS_002      0x5446B482
#define _FT5446DQS_Q02      0x5446C482

#define _FT3518             0x35180481
#define _FT3558             0x35580481

/*************************************************/

/*
 * choose your ic chip type of focaltech
 */
#if defined(CONFIG_TOUCHSCREEN_FTS_3427) && !defined(CONFIG_TOUCHSCREEN_FTS_5726)
#define FTS_CHIP_TYPE   _FT3427
#elif defined(CONFIG_TOUCHSCREEN_FTS_5726) && !defined(CONFIG_TOUCHSCREEN_FTS_3427)
#define FTS_CHIP_TYPE   _FT5726
#endif

/******************* Enables *********************/
/*********** 1 to enable, 0 to disable ***********/

/*
 * show debug log info
 * enable it for debug, disable it for release
 */
#define FTS_DEBUG_EN                            0

/*
 * Linux MultiTouch Protocol
 * 1: Protocol B(default), 0: Protocol A
 */
#define FTS_MT_PROTOCOL_B_EN                    1

/*
 * Report Pressure in multitouch
 * 1:enable(default),0:disable
*/
#define FTS_REPORT_PRESSURE_EN                  1

/*
 * Gesture function enable
 * default: disable
 */
#define FTS_GESTURE_EN                          0

/*
 * ESD check & protection
 * default: disable
 */
#define FTS_ESDCHECK_EN                         0

/*
 * Production test enable
 * 1: enable, 0:disable(default)
 */
#define FTS_TEST_EN                             0

/*
 * Glove mode enable
 * 1: enable, 0:disable(default)
 */
#define FTS_GLOVE_EN                            0
/*
 * cover enable
 * 1: enable, 0:disable(default)
 */
#define FTS_COVER_EN                            0
/*
 * Charger enable
 * 1: enable, 0:disable(default)
 */
#define FTS_CHARGER_EN                          0

/*
 * Nodes for tools, please keep enable
 */
#define FTS_SYSFS_NODE_EN                       1
#define FTS_APK_NODE_EN                         1

/*
 * Pinctrl enable
 * default: disable
 */
#define FTS_PINCTRL_EN                          0

/*
 * Customer power enable
 * enable it when customer need control TP power
 * default: disable
 */
#define FTS_POWER_SOURCE_CUST_EN                0

/****************************************************/

/********************** Upgrade ****************************/
/*
 * auto upgrade, please keep enable
 */
#define FTS_AUTO_UPGRADE_EN                     1

/*
 * auto upgrade for lcd cfg
 */
#define FTS_AUTO_LIC_UPGRADE_EN                 0

/*
 * Vendor and Panel IDs
 */
#define FTS_VENDOR_ID_DEFAULT                          0x0000
#define FTS_VENDOR_ID_TOPTOUCH                         0x00A0
#define FTS_VENDOR_ID_LENSONE                          0x006D
#define FTS_VENDOR_ID_TOPGROUP                         0x003E

#define FTS_PANEL_ID_DEFAULT                           0x0000
#define FTS_PANEL_ID_TOPTOUCH_7MM                      0x00C0
#define FTS_PANEL_ID_TOPTOUCH_11MM                     0x00C1
#define FTS_PANEL_ID_LENSONE_7MM                       0x00C2
#define FTS_PANEL_ID_LENSONE_11MM                      0x00C3
#define FTS_PANEL_ID_TOPGROUP_7MM                      0x0050
#define FTS_PANEL_ID_TOPGROUP_11MM                     0x0051

/*
 * Firmware Update Files
 */
#define FTS_UPGRADE_FW_DEFAULT              "firmware/fw_sample.i"

#define FTS_TOPTOUCH_NUM_FW                 2
#define FTS_UPGRADE_FW_TOPTOUCH_7MM         "firmware/7mm/TopTouch_7mm_0x0A.i"
#define FTS_UPGRADE_FW_TOPTOUCH_11MM        "firmware/11mm/TopTouch_11mm_0x0F.i"

#define FTS_LENSONE_NUM_FW                  2
#define FTS_UPGRADE_FW_LENSONE_7MM          "firmware/7mm/LensOne_7mm_0x04.i"
#define FTS_UPGRADE_FW_LENSONE_11MM         "firmware/11mm/LensOne_11mm_0x0D.i"

#define FTS_TOPGROUP_NUM_FW                 2
#define FTS_UPGRADE_FW_TOPGROUP_7MM         "firmware/7mm/TopGroup_7mm_0x06.i"
#define FTS_UPGRADE_FW_TOPGROUP_11MM        "firmware/11mm/TopGroup_11mm_0x0B.i"

/*********************************************************/
#endif /* _LINUX_FOCLATECH_CONFIG_H_ */
