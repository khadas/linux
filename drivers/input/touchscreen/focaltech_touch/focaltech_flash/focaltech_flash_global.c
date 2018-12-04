/*
 *
 * FocalTech TouchScreen driver.
 * 
 * Copyright (c) 2010-2015, Focaltech Ltd. All rights reserved.
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

 /*******************************************************************************
*
* File Name: Focaltech_flash.c
*
* Author: Xu YongFeng
*
* Created: 2015-01-29
*   
* Modify by mshl on 2015-03-20
*
* Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include "../focaltech_comm.h"
#include "focaltech_flash.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FTS_FLASH_GLOBAL_INFO  "File Version of  focaltech_apk_node.c:  V1.0.0 2016-03-16"


/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/

/************************************************************************
*   Name: hidi2c_to_stdi2c
* Brief:  HID to I2C
* Input: i2c info
* Output: no
* Return: fail =0
***********************************************************************/
int hidi2c_to_stdi2c(void)
{
	u8 auc_i2c_write_buf[5] = {0};
	int bRet = 0;
	int i=0;
	
	#if HIDTOI2C_DISABLE
		return 2;
	#endif
	auc_i2c_write_buf[0] = 0xeb;
	auc_i2c_write_buf[1] = 0xaa;
	auc_i2c_write_buf[2] = 0x09;
	
	//FTS_COMMON_DBG("Change to STDI2cValue,REG1 = 0x%x,REG2 = 0x%x,REG3 = 0x%x, iRet=%d\n", 
	//	reg_val[0], reg_val[1], reg_val[2], iRet);
	for(i = 0; i < 5; i++)
	{
		bRet =fts_i2c_write( auc_i2c_write_buf, 3);
		msleep(10);
		auc_i2c_write_buf[0] = auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = 0;
		fts_i2c_read( auc_i2c_write_buf, 0, auc_i2c_write_buf, 3)		
		if(0xeb==auc_i2c_write_buf[0] 
			&& 0xaa==auc_i2c_write_buf[1] 
			&& 0x08==auc_i2c_write_buf[2]
		)
		{
			FTS_COMMON_DBG("hidi2c_to_stdi2c successful.\n");
			bRet = 1;	
			break;
		}
		else 
		{
			FTS_COMMON_DBG("hidi2c_to_stdi2c error. time = %d, return data: 0x%02x, 0x%02x, 0x%02x\n",
				i+1, auc_i2c_write_buf[0], auc_i2c_write_buf[1], auc_i2c_write_buf[2]);
		
			bRet = 0;	
			msleep(10);
		}
	}

	return bRet;
}


/************************************************************************
* Name: fts_ctpm_fw_upgrade_ReadVendorID
* Brief:  read vendor ID
* Input: i2c info, vendor ID
* Output: no
* Return: fail <0
***********************************************************************/
int fts_ctpm_fw_upgrade_ReadVendorID( u8 *ucPVendorID)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u8 auc_i2c_write_buf[10];
	int i_ret;

	*ucPVendorID = 0;
	i_ret = hidi2c_to_stdi2c();
	if (i_ret == 0) 
	{
		FTS_COMMON_DBG("HidI2c change to StdI2c fail ! \n");
	}
	
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) 
	{
		/********* Step 1:Reset  CTPM *****/
		fts_write_reg( 0xfc, FTS_UPGRADE_AA);
		msleep(fts_updateinfo_curr.delay_aa);
		fts_write_reg( 0xfc, FTS_UPGRADE_55);
		msleep(200);
		/********* Step 2:Enter upgrade mode *****/
		i_ret = hidi2c_to_stdi2c();
		if (i_ret == 0) 
		{
			FTS_COMMON_DBG("HidI2c change to StdI2c fail ! \n");
			/*continue;*/
		}
		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write( auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			FTS_COMMON_DBG("failed writing  0x55 and 0xaa ! \n");
			continue;
		}
		/********* Step 3:check READ-ID ***********************/
		msleep(10);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read( auc_i2c_write_buf, 4, reg_val, 2);
		if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1 /*&& reg_val[1] == fts_updateinfo_curr.upgrade_id_2*/) {
			FTS_COMMON_DBG("[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
			break;
		} 
		else 
		{
			FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x, curr:0x%x\n", reg_val[0], reg_val[1], fts_updateinfo_curr.upgrade_id_1);
			continue;
		}
	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;
	/********* Step 4: read vendor id from app param area ***********************/
	msleep(10);
	auc_i2c_write_buf[0] = 0x03;
	auc_i2c_write_buf[1] = 0x00;
	auc_i2c_write_buf[2] = 0xd7;
	auc_i2c_write_buf[3] = 0x84;
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) 
	{
		fts_i2c_write( auc_i2c_write_buf, 4);		/* send param addr */
		msleep(5);
		reg_val[0] = reg_val[1] = 0x00;
		i_ret = fts_i2c_read( auc_i2c_write_buf, 0, reg_val, 2);
		if (0 == reg_val[0]) 
		{
			*ucPVendorID = 0;
			FTS_COMMON_DBG("In upgrade Vendor ID Mismatch, REG1 = 0x%x, REG2 = 0x%x, Definition:0x%x, i_ret=%d\n", reg_val[0], reg_val[1], 0, i_ret);
		} 
		else 
		{
			*ucPVendorID = reg_val[0];
			FTS_COMMON_DBG("In upgrade Vendor ID, REG1 = 0x%x, REG2 = 0x%x\n", reg_val[0], reg_val[1]);
			break;
		}
	}
	msleep(50);
	/********* Step 5: reset the new FW ***********************/
	FTS_COMMON_DBG("Step 5: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write( auc_i2c_write_buf, 1);
	msleep(200);	/*make sure CTP startup normally */
	i_ret = hidi2c_to_stdi2c();	/*Android to Std i2c.*/
	if (i_ret == 0) 
	{
		FTS_COMMON_DBG("HidI2c change to StdI2c fail ! \n");
	}
	msleep(10);
	return 0;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_ReadProjectCode
* Brief:  read project code
* Input: i2c info, project code
* Output: no
* Return: fail <0
***********************************************************************/
int fts_ctpm_fw_upgrade_ReadProjectCode( char *pProjectCode)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u8 j = 0;
	u8 auc_i2c_write_buf[10];
	int i_ret;
	u32 temp;
	i_ret = hidi2c_to_stdi2c();
	if (i_ret == 0) 
	{
		FTS_COMMON_DBG("HidI2c change to StdI2c fail ! \n");
	}	
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) 
	{
		/********* Step 1:Reset  CTPM *****/
		fts_write_reg( 0xfc, FTS_UPGRADE_AA);
		msleep(fts_updateinfo_curr.delay_aa);
		fts_write_reg( 0xfc, FTS_UPGRADE_55);
		msleep(200);
		/********* Step 2:Enter upgrade mode *****/
		i_ret = hidi2c_to_stdi2c();
		if (i_ret == 0) 
		{
			FTS_COMMON_DBG("HidI2c change to StdI2c fail ! \n");
			/*continue;*/
		}
		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write( auc_i2c_write_buf, 2);
		if (i_ret < 0) 
		{
			FTS_COMMON_DBG("failed writing  0x55 and 0xaa ! \n");
			continue;
		}
		/********* Step 3:check READ-ID ***********************/
		msleep(10);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read( auc_i2c_write_buf, 4, reg_val, 2);
		if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1 && reg_val[1] == fts_updateinfo_curr.upgrade_id_2) 
		{
			FTS_COMMON_DBG("[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
			break;
		}
		else
		{
			FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
			continue;
		}
	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;
	/********* Step 4: read vendor id from app param area ***********************/
	msleep(10);
	/*auc_i2c_write_buf[0] = 0x03;
	auc_i2c_write_buf[1] = 0x00;
	auc_i2c_write_buf[2] = 0xd7;
	auc_i2c_write_buf[3] = 0x84;
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		fts_i2c_write( auc_i2c_write_buf, 4);     //send param addr
		msleep(5);
		reg_val[0] = reg_val[1] = 0x00;
		i_ret = fts_i2c_read( auc_i2c_write_buf, 0, reg_val, 2);
		if (0 != reg_val[0]) {
			*pProjectCode=0;
			FTS_COMMON_DBG("In upgrade Vendor ID Mismatch, REG1 = 0x%x, REG2 = 0x%x, Definition:0x%x, i_ret=%d\n", reg_val[0], reg_val[1], 0, i_ret);
		} else {
			*pProjectCode=reg_val[0];
			FTS_COMMON_DBG("In upgrade Vendor ID, REG1 = 0x%x, REG2 = 0x%x\n", reg_val[0], reg_val[1]);
			break;
		}
	}
	*/
	/* read project code */
	auc_i2c_write_buf[0] = 0x03;
	auc_i2c_write_buf[1] = 0x00;
	for (j = 0; j < 33; j++) 
	{
		/*
		//if (is_5336_new_bootloader == BL_VERSION_Z7 || is_5336_new_bootloader == BL_VERSION_GZF)
			//temp = 0x07d0 + j;
		//else
		*/
		temp = 0xD7A0 + j;
		auc_i2c_write_buf[2] = (u8)(temp>>8);
		auc_i2c_write_buf[3] = (u8)temp;
		fts_i2c_read( auc_i2c_write_buf, 4, pProjectCode+j, 1);
		if (*(pProjectCode+j) == '\0')
			break;
	}
	FTS_COMMON_DBG("project code = %s \n", pProjectCode);
	msleep(50);
	/********* Step 5: reset the new FW ***********************/
	FTS_COMMON_DBG("Step 5: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write( auc_i2c_write_buf, 1);
	msleep(200);						/* make sure CTP startup normally */
	i_ret = hidi2c_to_stdi2c();	/* Android to Std i2c. */
	if (i_ret == 0) 
	{
		FTS_COMMON_DBG("HidI2c change to StdI2c fail ! \n");
	}
	msleep(10);
	return 0;
}


/************************************************************************
* Name: fts_ctpm_auto_clb
* Brief:  auto calibration
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int fts_ctpm_auto_clb(void)
{
	unsigned char uc_temp = 0x00;
	unsigned char i = 0;

	/* start auto CLB */
	msleep(200);

	fts_write_reg( 0, FTS_FACTORYMODE_VALUE);
	/* make sure already enter factory mode */
	msleep(100);
	/* write command to start calibration */
	fts_write_reg( 2, 0x4);
	msleep(300);
	if ((fts_updateinfo_curr.chip_id==0x11) 
		||(fts_updateinfo_curr.chip_id==0x12) 
		||(fts_updateinfo_curr.chip_id==0x13) 
		||(fts_updateinfo_curr.chip_id==0x14)) //5x36,5x36i
	{
		for(i=0;i<100;i++)
		{
			fts_read_reg( 0x02, &uc_temp);
			if (0x02 == uc_temp ||
				0xFF == uc_temp)
			{
				/* if 0x02, then auto clb ok, else 0xff, auto clb failure */
			    	break;
			}
			msleep(20);	    
		}
	} 
	else 
	{
		for(i=0;i<100;i++)
		{
			fts_read_reg( 0, &uc_temp);
			if (0x0 == ((uc_temp&0x70)>>4)) 
			{
				/* return to normal mode, calibration finish, auto/test mode auto switch */
			    	break;
			}
			msleep(20);	    
		}
	}
	/* calibration OK */
	fts_write_reg( 0, 0x40);  					/* goto factory mode for store */
	msleep(200);   									/* make sure already enter factory mode*/
	fts_write_reg( 2, 0x5); 	 					/* store CLB result, write 0x05 to 0x04 */
	msleep(300);
	fts_write_reg( 0, FTS_WORKMODE_VALUE);		/* return to normal mode */
	msleep(300);

	/* store CLB result OK */
	return 0;
}
/************************************************************************
* Name: fts_ctpm_update_project_setting
* Brief:  update project setting, only update these settings for COB project, or for some special case
* Input: i2c info
* Output: no
* Return: fail <0
***********************************************************************/
int fts_ctpm_update_project_setting(void)
{
	u8 uc_i2c_addr;				/* I2C slave address (7 bit address) */
	u8 uc_io_voltage;			/* IO Voltage 0---3.3v;1----1.8v */
	u8 uc_panel_factory_id;		/* TP panel factory ID */
	u8 buf[FTS_PACKET_LENGTH];//zax original:128
	u8 reg_val[2] = {0};
	u8 auc_i2c_write_buf[10] = {0};
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u32 i = 0;
	int i_ret;

	uc_i2c_addr = fts_i2c_client->addr;
	uc_io_voltage = 0x0;
	uc_panel_factory_id = 0x5a;


	/* Step 1:Reset  CTPM
	*   write 0xaa to register 0xfc
	*/
	if(fts_updateinfo_curr.chip_id==0x06 || fts_updateinfo_curr.chip_id==0x36)
	{
		fts_write_reg( 0xbc, 0xaa);
	}
	else 
	{
		fts_write_reg( 0xfc, 0xaa);
	}
	msleep(50);

	/* write 0x55 to register 0xfc */
	if(fts_updateinfo_curr.chip_id==0x06 || fts_updateinfo_curr.chip_id==0x36)
	{
		fts_write_reg( 0xbc, 0x55);
	}
	else
	{
		fts_write_reg( 0xfc, 0x55);
	}
	msleep(30);

	/********* Step 2:Enter upgrade mode *****/
	auc_i2c_write_buf[0] = 0x55;
	auc_i2c_write_buf[1] = 0xaa;
	do 
	{
		i++;
		i_ret = fts_i2c_write( auc_i2c_write_buf, 2);
		msleep(5);
	} while (i_ret <= 0 && i < 5);


	/********* Step 3:check READ-ID ***********************/
	auc_i2c_write_buf[0] = 0x90;
	auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
			0x00;

	fts_i2c_read( auc_i2c_write_buf, 4, reg_val, 2);

	if(reg_val[0] == fts_updateinfo_curr.upgrade_id_1 && reg_val[1] == fts_updateinfo_curr.upgrade_id_2)
		FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
	else
		return -EIO;

	auc_i2c_write_buf[0] = 0xcd;
	fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 1);
	FTS_COMMON_DBG( "bootloader version = 0x%x\n", reg_val[0]);

	/*--------- read current project setting  ---------- */
	/* set read start address */
	buf[0] = 0x3;
	buf[1] = 0x0;
	buf[2] = 0x78;
	buf[3] = 0x0;

	fts_i2c_read( buf, 4, buf, FTS_PACKET_LENGTH);
	FTS_COMMON_DBG( "[FTS] old setting: uc_i2c_addr = 0x%x,\
			uc_io_voltage = %d, uc_panel_factory_id = 0x%x\n",
			buf[0], buf[2], buf[4]);

	 /*--------- Step 4:erase project setting --------------*/
	auc_i2c_write_buf[0] = 0x63;
	fts_i2c_write( auc_i2c_write_buf, 1);
	msleep(100);

	/*----------  Set new settings ---------------*/
	buf[0] = uc_i2c_addr;
	buf[1] = ~uc_i2c_addr;
	buf[2] = uc_io_voltage;
	buf[3] = ~uc_io_voltage;
	buf[4] = uc_panel_factory_id;
	buf[5] = ~uc_panel_factory_id;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;
	packet_buf[2] = 0x78;
	packet_buf[3] = 0x0;
	packet_buf[4] = 0;
	packet_buf[5] = FTS_PACKET_LENGTH;

	for (i = 0; i < FTS_PACKET_LENGTH; i++)
		packet_buf[6 + i] = buf[i];

	fts_i2c_write( packet_buf, FTS_PACKET_LENGTH + 6);
	msleep(100);

	/********* reset the new FW ***********************/
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write( auc_i2c_write_buf, 1);

	msleep(200);
	return 0;
}

bool Upgrade_ReadPram(unsigned int Addr, unsigned char * pData, unsigned short Datalen)
{
	bool ReCode;
	int ret=-1;
	unsigned char pDataSend[16];
	//if (iCommMode == HY_I2C_INTERFACE)
	{
		pDataSend[0] = 0x85;
		pDataSend[1] = 0x00;
		pDataSend[2] = Addr>>8;
		pDataSend[3] = Addr;
		//HY_IIC_IO(hDevice, pDataSend, 4, NULL, 0);
		fts_i2c_write( pDataSend, 4);
		//HY_IIC_IO(hDevice, NULL, 0, pData, Datalen) == ERROR_CODE_OK ? ReCode = true : ReCode = false;

		

		ret =fts_i2c_read( NULL, 0, pData, Datalen);  
		if (ret < 0) 
		{        
			FTS_COMMON_DBG("[FTS] failed Upgrade_ReadPram \n");     
			return ret;    
		} 
		
	}

	return ReCode;
}


