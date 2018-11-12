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
#define FOCALTECH_APK_NODE_INFO  "File Version of  focaltech_flash_ft5x36.c:  V1.0.0 2016-03-21"

#define FTS_REG_FT5X36_VENDOR_ID 		0xA8
/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int fts_ic_program_ft5x36_upgrade( u8 *pbt_buf, u32 dw_lenth);
static int fts_ft5x36_get_i_file(u8 *file_buff, int *file_len);

/*******************************************************************************
* functions body
*******************************************************************************/

/************************************************************************
*   Name: fts_ic_program_ft5x36_upgrade
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ic_program_ft5x36_upgrade( u8 *pbt_buf, u32 dw_lenth)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u8 is_5336_new_bootloader = 0;
	u8 is_5336_fwsize_30 = 0;
	u32  packet_number;
	u32  j;
	u32  temp;
	u32  lenght;
	u8 	packet_buf[FTS_PACKET_LENGTH + 6];
	u8  	auc_i2c_write_buf[10];
	u8  	bt_ecc;
	int	i_ret;
	int	fw_filenth = dw_lenth;

	if(pbt_buf[fw_filenth-12] == 30)
	{
		is_5336_fwsize_30 = 1;
	}
	else 
	{
		is_5336_fwsize_30 = 0;
	}

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) 
{
    		/********* Step 1:Reset  CTPM *****/
    		/* write 0xaa to register FTS_RST_CMD_REG1 */
	   	fts_write_reg( FTS_RST_CMD_REG1, FTS_UPGRADE_AA);
		msleep(fts_updateinfo_curr.delay_aa);
		
		 /* write 0x55 to register FTS_RST_CMD_REG1 */
		fts_write_reg( FTS_RST_CMD_REG1, FTS_UPGRADE_55);   
		msleep(fts_updateinfo_curr.delay_55);   


		/********* Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		
	    i_ret = fts_i2c_write( auc_i2c_write_buf, 2);
	  
	    /********* Step 3:check READ-ID ***********************/   
		msleep(fts_updateinfo_curr.delay_readid);
	   	auc_i2c_write_buf[0] = FTS_READ_ID_REG; 
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
              fts_i2c_read( auc_i2c_write_buf, 4, reg_val, 2);           
              if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1 
                              && reg_val[1] == fts_updateinfo_curr.upgrade_id_2)
              {
                      FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]); 
                      break; 
              }
              else
              {
                      FTS_COMMON_DBG( "[FTS] Step 3: CTPM ID FAILD,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
                      continue;
              }

          }

          if (i >= FTS_UPGRADE_LOOP)
                          return -EIO;
		  
         auc_i2c_write_buf[0] = 0xcd;
         fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 1);	
	if (reg_val[0] <= 4)
	{
		is_5336_new_bootloader = BL_VERSION_LZ4 ;
	}
	else if(reg_val[0] == 7)
	{
		is_5336_new_bootloader = BL_VERSION_Z7 ;
	}
	else if(reg_val[0] >= 0x0f)
	{
		is_5336_new_bootloader = BL_VERSION_GZF ;
	}

     /********* Step 4:erase app and panel paramenter area ********************/
	if(is_5336_fwsize_30)
	{
		auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
		fts_i2c_write( auc_i2c_write_buf, 1); 	/* erase app area */	
   		 msleep(fts_updateinfo_curr.delay_erase_flash); 

		 auc_i2c_write_buf[0] = FTS_ERASE_PARAMS_CMD;
		fts_i2c_write( auc_i2c_write_buf, 1); 	/* erase config area */	
   		 msleep(50);
	}
	else
	{
		auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
		fts_i2c_write( auc_i2c_write_buf, 1); 	/* erase app area */	
   		msleep(fts_updateinfo_curr.delay_erase_flash); 
	}

	/********* Step 5:write firmware(FW) to ctpm flash *********/
	bt_ecc = 0;

	if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
	{
		dw_lenth = dw_lenth - 8;
	}
	else if(is_5336_new_bootloader == BL_VERSION_GZF) 
	{
		dw_lenth = dw_lenth - 14;
	}
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;
	packet_buf[1] = 0x00;
	for (j=0;j<packet_number;j++)
	{
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8)(lenght>>8);
		packet_buf[5] = (u8)lenght;

		for (i=0;i<FTS_PACKET_LENGTH;i++)
		{
		    packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
		    bt_ecc ^= packet_buf[6+i];
		}

		fts_i2c_write( packet_buf, FTS_PACKET_LENGTH+6);
		msleep(FTS_PACKET_LENGTH/6 + 1);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
	{
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;

		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8)(temp>>8);
		packet_buf[5] = (u8)temp;

		for (i=0;i<temp;i++)
		{
		    packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
		    bt_ecc ^= packet_buf[6+i];
		}
  
		fts_i2c_write( packet_buf, temp+6);
		msleep(20);
	}
        /* send the last six byte */
        if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
        {
                for (i = 0; i<6; i++)
                {
                                if (is_5336_new_bootloader  == BL_VERSION_Z7 /*&& DEVICE_IC_TYPE==IC_FT5x36*/) 
                                {
                                                temp = 0x7bfa + i;
                                }
                                else if(is_5336_new_bootloader == BL_VERSION_LZ4)
                                {
                                                temp = 0x6ffa + i;
                                }
                                packet_buf[2] = (u8)(temp>>8);
                                packet_buf[3] = (u8)temp;
                                temp =1;
                                packet_buf[4] = (u8)(temp>>8);
                                packet_buf[5] = (u8)temp;
                                packet_buf[6] = pbt_buf[ dw_lenth + i]; 
                                bt_ecc ^= packet_buf[6];
                                fts_i2c_write( packet_buf, 7);
                                msleep(10);
                }
        }
        else if(is_5336_new_bootloader == BL_VERSION_GZF)
        {
		for (i = 0; i<12; i++)
		{
		        if (is_5336_fwsize_30 /*&& DEVICE_IC_TYPE==IC_FT5x36*/) 
		        {
		                        temp = 0x7ff4 + i;
		        }
		        else if (1/*DEVICE_IC_TYPE==IC_FT5x36*/) 
		        {
		                        temp = 0x7bf4 + i;
		        }
		        packet_buf[2] = (u8)(temp>>8);
		        packet_buf[3] = (u8)temp;
		        temp =1;
		        packet_buf[4] = (u8)(temp>>8);
		        packet_buf[5] = (u8)temp;
		        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
		        bt_ecc ^= packet_buf[6];
			fts_i2c_write( packet_buf, 7);
			msleep(10);
		}
	}

	/********* Step 6: read out checksum ***********************/
	/* send the opration head */
	auc_i2c_write_buf[0] = FTS_REG_ECC;
	fts_i2c_read( auc_i2c_write_buf, 1, reg_val, 1); 
	if(reg_val[0] != bt_ecc)
	{
		FTS_COMMON_DBG( "[FTS]--ecc error! fw_ecc=%02x flash_ecc=%02x\n", reg_val[0], bt_ecc);
	    	return -EIO;
	}
	/********* Step 7: reset the new FW ***********************/
	auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
	fts_i2c_write( auc_i2c_write_buf, 1);
	msleep(300);  /*make sure CTP startup normally*/

	return 0;
}

static int fts_ft5x36_get_i_file(u8 *file_buff, int *file_len)
{
	int i_ret = 0;
	u8 uc_vender_id= 0;
	
	if(USE_I_FILES_NUMBER > 1)
	{
		i_ret = fts_read_reg(FTS_REG_FT5X36_VENDOR_ID, &uc_vender_id);
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
int fts_ft5x36_upgrade_with_i_file(void)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fw_len = 0;

	/*get i file*/
	i_ret = fts_ft5x36_get_i_file(pbt_buf, &fw_len);
	if(i_ret < 0)
	{
		return -1;
	}

	/*call upgrade function*/
	i_ret = fts_ic_program_ft5x36_upgrade( pbt_buf, fw_len);
	
	return i_ret;
}

int fts_ft5x36_get_i_file_version(void)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fw_len = 0;

	/*get i file*/
	i_ret = fts_ft5x36_get_i_file(pbt_buf, &fw_len);
	if(i_ret < 0)
	{
		return 0;
	}
	if (fw_len > 2)
	{
              return pbt_buf[fw_len - 2];	//addr of fw ver
	}	
	
	return 0x00;						/*default value */
}
int fts_ft5x36_upgrade_with_bin_file(u8 *file_buff, int file_len)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fw_len = 0;

	pbt_buf = file_buff;
	fw_len = file_len;

	/*call upgrade function*/
	i_ret = fts_ic_program_ft5x36_upgrade( pbt_buf, fw_len);
	
	return i_ret;
}
