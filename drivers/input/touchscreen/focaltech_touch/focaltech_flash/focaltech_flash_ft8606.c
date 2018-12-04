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

 /*******************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Software Department, FocalTech
*
* Created: 2016-03-21
*
*  Abstract:
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
#define FOCALTECH_APK_NODE_INFO  "File Version of  focaltech_flash_ft8606.c:  V1.0.0 2016-03-21"

#define FTS_REG_FT8606_VENDOR_ID 		0xA8

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int  fts_ic_program_ft8606_write_pram( u8* pbt_buf, u32 dw_lenth);
static int fts_ic_program_ft8606_upgrade( u8 *pbt_buf, u32 dw_lenth);
static int fts_ft8606_get_i_file(u8 *file_buff, int *file_len);

/*******************************************************************************
* functions body
*******************************************************************************/
static int  fts_ic_program_ft8606_write_pram( u8* pbt_buf, u32 dw_lenth)
{

	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp,nowAddress=0,StartFlashAddr=0,FlashAddr=0;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 *pCheckBuffer = NULL;
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	int i_ret,ReCode=-1;

	//fts_get_upgrade_info(&upgradeinfo);
	FTS_COMMON_DBG("8606 dw_lenth= %d",dw_lenth);
	if(dw_lenth > 0x10000 || dw_lenth ==0)
	{		
		return -EIO;
	}
	pCheckBuffer=kmalloc(dw_lenth+1,GFP_ATOMIC);
	for (i = 0; i < 20; i++) 
	{
		/********* Step 1:Reset  CTPM *****/
		/* write 0xaa to register 0xfc */
		//ftxxxx_reset_tp(0);
		//msleep(10);
		//ftxxxx_reset_tp(1);
		
		//msleep(10);	//time (5~20ms)


		fts_write_reg( 0xfc, FTS_UPGRADE_AA);
		msleep(fts_updateinfo_curr.delay_aa);
		fts_write_reg( 0xfc, FTS_UPGRADE_55);
		msleep(200);
		/********* Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		i_ret = fts_i2c_write( auc_i2c_write_buf, 1);
		if(i_ret < 0)
		{
			FTS_COMMON_DBG("[FTS] failed writing  0x55 ! \n");
			continue;
		}
		
		/*
		auc_i2c_write_buf[0] = FT_UPGRADE_AA;
		i_ret = ftxxxx_i2c_Write( auc_i2c_write_buf, 1);
		if(i_ret < 0)
		{
			DBG("[FTS] failed writing  0xaa ! \n");
			continue;
		}
		*/	
		/********* Step 3:check READ-ID ***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
			0x00;
		reg_val[0] = reg_val[1] = 0x00;
		
		fts_i2c_read( auc_i2c_write_buf, 4, reg_val, 2);

		if ((reg_val[0] == 0x86 && reg_val[1] == 0x06) 
			|| (reg_val[0] == 0x86 && reg_val[1] == 0x07)) 
		{			
			msleep(50);
			break;
		} 
		else 
		{
			FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);
			
			continue;
		}
	}

	if (i >= FTS_UPGRADE_LOOP )
		return -EIO;

	/********* Step 4:write firmware(FW) to ctpm flash *********/
	bt_ecc = 0;
	FTS_COMMON_DBG("Step 5:write firmware(FW) to ctpm flash\n");

	//dw_lenth = dw_lenth - 8;
	
	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xae;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) 
	{
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) 
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		fts_i2c_write( packet_buf, FTS_PACKET_LENGTH + 6);
		nowAddress=nowAddress+FTS_PACKET_LENGTH;
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) 
	{
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) 
		{
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}		
		fts_i2c_write( packet_buf, temp + 6);
		nowAddress=nowAddress+temp;
	}
	/*
	temp = FT_APP_INFO_ADDR;
	packet_buf[2] = (u8) (temp >> 8);
	packet_buf[3] = (u8) temp;
	temp = 8;
	packet_buf[4] = (u8) (temp >> 8);
	packet_buf[5] = (u8) temp;
	for (i = 0; i < 8; i++) 
	{
		packet_buf[6+i] = pbt_buf[dw_lenth + i];
		bt_ecc ^= packet_buf[6+i];
	}	
	ftxxxx_i2c_Write( packet_buf, 6+8);
	*/
	/********* Step 5: read out checksum ***********************/
	/* send the opration head */
	FTS_COMMON_DBG("Step 6: read out checksum\n");
	/*auc_i2c_write_buf[0] = 0xcc;
	//msleep(2);
	ftxxxx_i2c_Read( auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) 
	{
		FTS_COMMON_DBG( "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",reg_val[0],bt_ecc);	
		return -EIO;
	}
	FTS_COMMON_DBG(KERN_WARNING "checksum %X %X \n",reg_val[0],bt_ecc);
	DBG("Read flash and compare\n");
		*/
	msleep(100);
	//-----------------------------------------------------------------------------------------------------
	FTS_COMMON_DBG( "[FTS]--nowAddress=%02x dw_lenth=%02x\n",nowAddress,dw_lenth);	
	if(nowAddress == dw_lenth)
	{

	FlashAddr=0;
	while(1)
	{
		StartFlashAddr = FlashAddr;
		if(FlashAddr == dw_lenth)
		{
			break;
		}
		else if(FlashAddr+FTS_PACKET_LENGTH > dw_lenth)
		{			
			if(!Upgrade_ReadPram(StartFlashAddr, pCheckBuffer+FlashAddr, dw_lenth-FlashAddr))
			{
				FTS_COMMON_DBG("read out checksum error\n");
				return -EIO;
				//break;
			}
			ReCode = ERROR_CODE_OK;
			FlashAddr = dw_lenth;
		}
		else
		{
			if(!Upgrade_ReadPram(StartFlashAddr, pCheckBuffer+FlashAddr, FTS_PACKET_LENGTH))
			{
				FTS_COMMON_DBG("read out checksum error\n");
				return -EIO;
				
				//break;
			}
			FlashAddr += FTS_PACKET_LENGTH;
			ReCode = ERROR_CODE_OK;
		}

		if(ReCode != ERROR_CODE_OK){
			FTS_COMMON_DBG("read out checksum error\n");
				return -EIO;
			//break;
		}
		
	}
	FTS_COMMON_DBG( "[FTS]--FlashAddr=%02x dw_lenth=%02x\n",FlashAddr,dw_lenth);
	if(FlashAddr == dw_lenth)
	{
		
		FTS_COMMON_DBG("Checking data...\n");
		for(i=0; i<dw_lenth; i++)
		{
			if(pCheckBuffer[i] != pbt_buf[i])
			{
				FTS_COMMON_DBG("read out checksum error\n");
				if(pCheckBuffer)
					kfree(pCheckBuffer);

					pCheckBuffer = NULL;
				return -EIO;
			}
		}
		if(pCheckBuffer)
					kfree(pCheckBuffer);

					pCheckBuffer = NULL;
		//COMM_FLASH_FT5422_Upgrade_StartApp(bOldProtocol, iCommMode);		//Reset
		FTS_COMMON_DBG("read out checksum successful\n");
		
	}
	else
	{
		FTS_COMMON_DBG("read out checksum error\n");
	}	

	}
	else
	{
		FTS_COMMON_DBG("read out checksum error\n");
	}	

	/********* Step 6: start app ***********************/
	FTS_COMMON_DBG("Step 6: start app\n");
	auc_i2c_write_buf[0] = 0x08;
	fts_i2c_write( auc_i2c_write_buf, 1);
	msleep(20);

	return 0;
}

/************************************************************************
*   Name: fts_ic_program_ft8606_upgrade
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ic_program_ft8606_upgrade( u8 *pbt_buf, u32 dw_lenth)
{
	u8 reg_val[4] = {0};
	u8 reg_val_id[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	int i_ret;
	unsigned char cmd[20];
	unsigned char Checksum = 0;
	u32 uCheckStart = 0x1000;
	u32 uCheckOff = 0x20;
	u32 uStartAddr = 0x00;
	
	//memcpy(m_DataBuffer, pFileData, m_FileLen);
	//pbt_buf=pFileData;
	auc_i2c_write_buf[0] = 0x05;
	reg_val_id[0] = 0x00;
		
	i_ret =fts_i2c_read( auc_i2c_write_buf, 1, reg_val_id, 1);
	if(dw_lenth == 0)
	{
		return -EIO;
	}
	if(0x81 == (int)reg_val_id[0]) 
	{
			if(dw_lenth > 1024*60) 
			{
				return -EIO;
			}
	}
	else if(0x80 == (int)reg_val_id[0]) 
	{
			if(dw_lenth > 1024*64) 
			{
				return -EIO;
			}
	}


	/*if(dw_lenth > 1024*64)
	{
		return -EIO;
	}*/

	//fts_get_upgrade_info(&upgradeinfo);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/********* Step 1:Reset  CTPM *****/
		//ftxxxx_write_reg( 0xfc, FT_UPGRADE_AA);
		//msleep(upgradeinfo.delay_aa);
		//ftxxxx_write_reg( 0xfc, FT_UPGRADE_55);
		//msleep(200);
		/********* Step 2:Enter upgrade mode *****/
		//i_ret = hidi2c_to_stdi2c();

		//if(i_ret == 0)
		//{
		//	DBG("HidI2c change to StdI2c fail ! \n");
		//	continue;
		//}
		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write( auc_i2c_write_buf, 2);
		if(i_ret < 0)
		{
			FTS_COMMON_DBG("failed writing  0x55 and 0xaa ! \n");
			continue;
		}

		/********* Step 3:check READ-ID ***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;

		reg_val[0] = reg_val[1] = 0x00;

		fts_i2c_read( auc_i2c_write_buf, 4, reg_val, 2);

		if ((reg_val[0] == fts_updateinfo_curr.upgrade_id_1 && reg_val[1] == fts_updateinfo_curr.upgrade_id_2)) {
				FTS_COMMON_DBG("[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
				break;
		} 
		else if ((reg_val[0] == 0x86 && reg_val[1] == 0xA7)) {
				FTS_COMMON_DBG("[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);

				//uCheckStart = 0x1020;
				break;
		}
		else {
			FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);

			continue;
		}
	}	
	if (i >= FTS_UPGRADE_LOOP )
		return -EIO;	
	/* Step 4:erase app and panel paramenter area */
	FTS_COMMON_DBG("Step 4:erase app and panel paramenter area\n");
		
	{		
		cmd[0] = 0x05;
		cmd[1] = reg_val_id[0];//0x80;
		cmd[2] = 0x00;
		//ReCode = HY_IIC_IO(hDevice, cmd, 2, NULL, 0);	
		fts_i2c_write( cmd, 3);
	}
	
	// Set pramboot download mode
	//COMM_FLASH_FT5422_Upgrade_SetFlashMode(0x0B, iCommMode);
	{
		cmd[0] = 0x09;
		cmd[1] = 0x0B;
		//HY_IIC_IO(hDevice, cmd, 2, NULL, 0);
		fts_i2c_write( cmd, 2);
	}	
	for(i=0; i<dw_lenth ; i++)
	{		
		Checksum ^= pbt_buf[i];
	}
	msleep(50);

	// erase app area 
	
	auc_i2c_write_buf[0] = 0x61;
	fts_i2c_write( auc_i2c_write_buf, 1);     
	msleep(1350);

	for(i = 0;i < 15;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 2);

		if(0xF0==reg_val[0] && 0xAA==reg_val[1])
		{
			break;
		}
		msleep(50);
	}
	
	/********* Step 5:write firmware(FW) to ctpm flash *********/
	bt_ecc = 0;
	FTS_COMMON_DBG("Step 5:write firmware(FW) to ctpm flash\n");
	
	//dw_lenth = dw_lenth - 8;
	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	//packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) {
		temp = uCheckStart+j * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		uStartAddr=temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;
		
		uCheckOff = uStartAddr/lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) 
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		fts_i2c_write( packet_buf, FTS_PACKET_LENGTH + 6);
		//msleep(10);

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 2);

			if ((uCheckOff +uCheckStart) == (((reg_val[0]) << 8) | reg_val[1]))
			{
				break;
			}
			msleep(1);
		}

		temp = (((reg_val[0]) << 8) | reg_val[1]);
		if( i == 30) FTS_COMMON_DBG("Query 6a reg time out value: 0x%x, driver set value:0x%x\n",  temp, uCheckOff + uCheckStart);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = uCheckStart+packet_number * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		uStartAddr=temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		uCheckOff=uStartAddr/temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}        
		fts_i2c_write( packet_buf, temp + 6);	

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 2);

			if ((uCheckOff + uCheckStart) == (((reg_val[0]) << 8) | reg_val[1]))
			{
				break;
			}
			
			msleep(1);

		}

		temp = (((reg_val[0]) << 8) | reg_val[1]);
		if( i == 30) FTS_COMMON_DBG("Query 6a reg time out value: 0x%x, driver set value:0x%x\n",  temp,  uCheckOff+ uCheckStart);
	}

	msleep(50);

	/********* Step 6: read out checksum ***********************/
	/*send the opration head */
	FTS_COMMON_DBG("Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0x64;
	fts_i2c_write( auc_i2c_write_buf, 1); 
	msleep(300);
	
	temp = uCheckStart+0;
	
	
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	
	if (dw_lenth > LEN_FLASH_ECC_MAX)
	{
		temp = LEN_FLASH_ECC_MAX;
	}
	else
	{
		temp = dw_lenth;
		FTS_COMMON_DBG("Step 6_1: read out checksum\n");
	}
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = fts_i2c_write( auc_i2c_write_buf, 6); 
	msleep(dw_lenth/256);

	for(i = 0;i < 100;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 2);

		if (0xF0==reg_val[0] && 0x55==reg_val[1])
		{
			break;
		}
		msleep(1);

	}
	//----------------------------------------------------------------------
	if (dw_lenth > LEN_FLASH_ECC_MAX)
	{
		temp = LEN_FLASH_ECC_MAX;
		auc_i2c_write_buf[0] = 0x65;
		auc_i2c_write_buf[1] = (u8)(temp >> 16);
		auc_i2c_write_buf[2] = (u8)(temp >> 8);
		auc_i2c_write_buf[3] = (u8)(temp);
		temp = dw_lenth-LEN_FLASH_ECC_MAX;
		auc_i2c_write_buf[4] = (u8)(temp >> 8);
		auc_i2c_write_buf[5] = (u8)(temp);
		i_ret = fts_i2c_write( auc_i2c_write_buf, 6); 
	

	
		msleep(dw_lenth/256);

		for(i = 0;i < 100;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 2);

			if (0xF0==reg_val[0] && 0x55==reg_val[1])
			{
				break;
			}
			msleep(1);

		}
	}
	auc_i2c_write_buf[0] = 0x66;
	fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) 
	{
		FTS_COMMON_DBG( "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
			reg_val[0],
			bt_ecc);

		return -EIO;
	}
	FTS_COMMON_DBG(KERN_WARNING "checksum %X %X \n",reg_val[0],bt_ecc);       
	/********* Step 7: reset the new FW ***********************/
	FTS_COMMON_DBG("Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write( auc_i2c_write_buf, 1);
	msleep(200);  

	return 0;
}

static int fts_ft8606_get_i_file(u8 *file_buff, int *file_len)
{
	int i_ret = 0;
	u8 uc_vender_id= 0;
	
	if(USE_I_FILES_NUMBER > 1)
	{
		i_ret = fts_read_reg(FTS_REG_FT8606_VENDOR_ID, &uc_vender_id);
		if(i_ret < 0)
		{
			i_ret = fts_ctpm_fw_upgrade_ReadVendorID( &uc_vender_id);
			if(i_ret < 0)
			{
				FTS_COMMON_DBG("Failed to get Vendor ID! error code = %d", i_ret);
				return i_ret;
			}
		}
		FTS_COMMON_DBG("[FTS] read vendor id from boot is 0x%x\n",uc_vender_id);	
		
		if(USE_I_FILES_NUMBER == 2)
		{
			switch(uc_vender_id)
			{
				case USE_I_FILE_1_BY_WENDOR_ID_1:
					file_buff = BUFFER_I_FILE_1;
					*file_len = g_len_i_file_1;	
					break;
				case USE_I_FILE_2_BY_WENDOR_ID_2:
					file_buff = BUFFER_I_FILE_2;
					*file_len = g_len_i_file_2;	
					break;			
				default:
				{
					FTS_COMMON_DBG("Failed to get i file! Invalid wendor id, wendor id = %d", uc_vender_id);
					return -1;
				}
						
			}			
		}
		else if(USE_I_FILES_NUMBER == 3)
		{
			switch(uc_vender_id)
			{
				case USE_I_FILE_1_BY_WENDOR_ID_1:
					file_buff = BUFFER_I_FILE_1;
					*file_len = g_len_i_file_1;	
					break;
				case USE_I_FILE_2_BY_WENDOR_ID_2:
					file_buff = BUFFER_I_FILE_2;
					*file_len = g_len_i_file_2;	
					break;
				case USE_I_FILE_3_BY_WENDOR_ID_3:
					file_buff = BUFFER_I_FILE_3;
					*file_len = g_len_i_file_3;	
					break;				
				default:
				{
					FTS_COMMON_DBG("Failed to get i file! Invalid wendor id, wendor id = %d", uc_vender_id);
					return -1;
				}
						
			}
		}
		else
		{
			FTS_COMMON_DBG("Failed to get i file, this is too many files, file num = %d", USE_I_FILES_NUMBER);
			return -1;
		}
		
	}
	else
	{
		file_buff = BUFFER_I_FILE_1;
		*file_len = g_len_i_file_1;		
	}
	
	return 0;
}
int fts_ft8606_upgrade_with_i_file(void)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fw_len = 0;

	/*get i file*/
	i_ret = fts_ft8606_get_i_file(pbt_buf, &fw_len);
	if(i_ret < 0)
	{
		return -1;
	}

	 /* call the write pram function */
    	i_ret = fts_ic_program_ft8606_write_pram( BUFFER_PRAM_BOOT_FILE, g_len_pram_boot_file);
	
	if (i_ret != 0)
	{
		FTS_COMMON_DBG( "%s:WritePram failed. err.\n",__func__);
		return -EIO;
	}
	
	/*call upgrade function*/
	i_ret = fts_ic_program_ft8606_upgrade( pbt_buf, fw_len);

   	if (i_ret != 0) 
	{
    		FTS_COMMON_DBG( "[FTS] upgrade failed. err=%d.\n", i_ret);
    	} 
	else 
	{
		#ifdef AUTO_CLB
    		fts_ctpm_auto_clb();  /* start auto CLB */
		#endif
    	}
				
	return i_ret;
}
int fts_ft8606_get_i_file_version(void)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fw_len = 0;

	/*get i file*/
	i_ret = fts_ft8606_get_i_file(pbt_buf, &fw_len);
	if(i_ret < 0)
	{
		return 0;
	}
	if (fw_len > 2)
	{
              return pbt_buf[0x10E];	//addr of fw ver
	}	
	
	return 0x00;						/*default value */
}

int fts_ft8606_upgrade_with_bin_file(u8 *file_buff, int file_len)
{
	u8 *pbt_buf = NULL;
	int i_ret= 0;
	int fw_len = 0;

	pbt_buf = file_buff;
	fw_len = file_len;

	 /* call the write pram function */
    	i_ret = fts_ic_program_ft8606_write_pram( BUFFER_PRAM_BOOT_FILE, g_len_pram_boot_file);
	
	if (i_ret != 0)
	{
		FTS_COMMON_DBG( "%s:WritePram failed. err.\n",__func__);
		return -EIO;
	}
	
	/*call upgrade function*/
	i_ret = fts_ic_program_ft8606_upgrade( pbt_buf, fw_len);

   	if (i_ret != 0) 
	{
    		FTS_COMMON_DBG( "[FTS] upgrade failed. err=%d.\n", i_ret);
    	} 
	else 
	{
		#ifdef AUTO_CLB
    		fts_ctpm_auto_clb();  /* start auto CLB */
		#endif
    	}
				
	return i_ret;
}
