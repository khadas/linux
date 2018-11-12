/*
 *
 * FocalTech TouchScreen driver.
 * 
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
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

#ifndef __FOCALTECH_FLASH_H__
#define __FOCALTECH_FLASH_H__
 /*******************************************************************************
*
* File Name: focaltech_flash.h
*
*    Author: Software Department, FocalTech
*
*   Created: 2016-03-16
*
*  Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/

#include "../focaltech_global/focaltech_global.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FTS_COMMON_LIB_INFO  "Common_Lib_Version  V1.0.0 2016-02-24"

/**/
//#define SUPPORT_MULTIPLE_I_FILES_EN						0//Support multiple I files, 0:disable, 1:enable
#define USE_I_FILES_NUMBER									1//Support multiple I files, 0:disable, 1:enable
#define USE_I_FILE_1_BY_WENDOR_ID_1						0x01//must change to actual value
#define USE_I_FILE_2_BY_WENDOR_ID_2						0x02//must change to actual value
#define USE_I_FILE_3_BY_WENDOR_ID_3						0x03//must change to actual value

/*Set defualt IC Name*/
#define FTS_IC_NAME											"FT5X46"

#define FTS_FW_MIN_SIZE										8
#define FTS_FW_MAX_SIZE										(54 * 1024)
/* Firmware file is not supporting minor and sub minor so use 0 */
#define FTS_FW_FILE_MAJ_VER(x)									((x)->data[(x)->size - 2])
#define FTS_FW_FILE_MIN_VER(x)									0
#define FTS_FW_FILE_SUB_MIN_VER(x) 							0
#define FTS_FW_FILE_VENDOR_ID(x)								((x)->data[(x)->size - 1])
#define FTS_FW_FILE_MAJ_VER_FT6X36(x)							((x)->data[0x10a])
#define FTS_FW_FILE_VENDOR_ID_FT6X36(x)						((x)->data[0x108])
#define FTS_MAX_TRIES											5
#define FTS_RETRY_DLY											20
#define FTS_MAX_WR_BUF										10
#define FTS_MAX_RD_BUF											2
#define FTS_FW_PKT_META_LEN									6
#define FTS_FW_PKT_DLY_MS										20
#define FTS_FW_LAST_PKT										0x6ffa
#define FTS_EARSE_DLY_MS										100
#define FTS_55_AA_DLY_NS										5000
#define FTS_CAL_START											0x04
#define FTS_CAL_FIN												0x00
#define FTS_CAL_STORE											0x05
#define FTS_CAL_RETRY											100
#define FTS_REG_CAL												0x00
#define FTS_CAL_MASK											0x70
#define FTS_BLOADER_SIZE_OFF									12
#define FTS_BLOADER_NEW_SIZE									30
#define FTS_DATA_LEN_OFF_OLD_FW								8
#define FTS_DATA_LEN_OFF_NEW_FW								14
#define FTS_FINISHING_PKT_LEN_OLD_FW							6
#define FTS_FINISHING_PKT_LEN_NEW_FW							12
#define FTS_MAGIC_BLOADER_Z7									0x7bfa
#define FTS_MAGIC_BLOADER_LZ4									0x6ffa
#define FTS_MAGIC_BLOADER_GZF_30								0x7ff4
#define FTS_MAGIC_BLOADER_GZF									0x7bf4
#define FTS_REG_ECC												0xCC
#define FTS_RST_CMD_REG2										0xBC
#define FTS_READ_ID_REG										0x90
#define FTS_ERASE_APP_REG										0x61
#define FTS_ERASE_PARAMS_CMD									0x63
#define FTS_FW_WRITE_CMD										0xBF
#define FTS_REG_RESET_FW										0x07
#define FTS_RST_CMD_REG1										0xFC
#define FTS_FACTORYMODE_VALUE									0x40
#define FTS_WORKMODE_VALUE									0x00
#define FTS_APP_INFO_ADDR										0xd7f8
#define BL_VERSION_LZ4        										0
#define BL_VERSION_Z7        										1
#define BL_VERSION_GZF       						 				2
#define FTS_REG_FW_VENDOR_ID 									0xA8
//#define MAX_R_FLASH_SIZE										120
//#define FTS_SETTING_BUF_LEN      								120
#define ERROR_CODE_OK 											0
#define FTS_UPGRADE_LOOP										30

#define FTS_MAX_POINTS_2                        							2
#define FTS_MAX_POINTS_5                        							5
#define FTS_MAX_POINTS_10                        						10
#define AUTO_CLB_NEED                           							1
#define AUTO_CLB_NONEED                         							0
#define FTS_UPGRADE_AA											0xAA
#define FTS_UPGRADE_55											0x55

#define HIDTOI2C_DISABLE										0
#define FTXXXX_INI_FILEPATH_CONFIG 							"/sdcard/"

#define LEN_FLASH_ECC_MAX 					0xFFFE

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/


/*******************************************************************************
* Global variable 
*******************************************************************************/
#define FILE_LEN(arr) 	(sizeof(arr)/sizeof(arr[0]))

extern unsigned char BUFFER_PRAM_BOOT_FILE[];
extern unsigned char BUFFER_I_FILE_1[];
extern unsigned char BUFFER_I_FILE_2[];
extern unsigned char BUFFER_I_FILE_3[];

extern unsigned int g_len_pram_boot_file;
extern unsigned int g_len_i_file_1;
extern unsigned int g_len_i_file_2;
extern unsigned int g_len_i_file_3;
/*
static unsigned char BUFFER_PRAM_BOOT_FILE[] 	= {
	
};
unsigned char BUFFER_I_FILE_1[] 				= {
	
};

unsigned char BUFFER_I_FILE_2[] 				= {
	
};
unsigned char BUFFER_I_FILE_3[] 				= {
	
};*/
/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
extern int hidi2c_to_stdi2c(void);

extern int fts_ctpm_fw_upgrade_ReadVendorID( u8 *ucPVendorID);
extern int fts_ctpm_fw_upgrade_ReadProjectCode( char *pProjectCode);
extern int fts_ctpm_update_project_setting(void);
extern bool Upgrade_ReadPram(unsigned int Addr, unsigned char * pData, unsigned short Datalen);

/*export function for External call*/


/* export function of ft5x06*/
extern int fts_ft5x06_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft5x06_upgrade_with_i_file(void);
extern int fts_ft5x06_get_i_file_version(void);

/* export function of ft5x36*/
extern int fts_ft5x36_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft5x36_upgrade_with_i_file(void);
extern int fts_ft5x36_get_i_file_version(void);

/* export function of ft6x06*/
extern int fts_ft6x06_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft6x06_upgrade_with_i_file(void);
extern int fts_ft6x06_get_i_file_version(void);

/* export function of ft6x36*/
extern int fts_ft6x36_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft6x36_upgrade_with_i_file(void);
extern int fts_ft6x36_get_i_file_version(void);

/* export function of ft5x46*/
extern int fts_ft5x46_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft5x46_upgrade_with_i_file(void);
extern int fts_ft5x46_get_i_file_version(void);

/* export function of ft5822*/
extern int fts_ft5822_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft5822_upgrade_with_i_file(void);
extern int fts_ft5822_get_i_file_version(void);

/* export function of ft8606*/
extern int fts_ft8606_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft8606_upgrade_with_i_file(void);
extern int fts_ft8606_get_i_file_version(void);

/* export function of ft8716*/
extern int fts_ft8716_upgrade_with_bin_file(u8 *file_buff, int file_len);
extern int fts_ft8716_upgrade_with_i_file(void);
extern int fts_ft8716_get_i_file_version(void);

#endif

