/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¨All Rights Reserved.
*
* File Name: Global.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: global function for test
*
************************************************************************/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/printk.h>

#include <linux/delay.h>//msleep

#include "focaltech_test_ini.h"
#include "focaltech_test_global.h"

#include "focaltech_test_main.h"

#define DEVIDE_MODE_ADDR	0x00

struct StruScreenSeting g_ScreenSetParam; //∆¡ƒª…Ë÷√≤Œ ˝
struct stTestItem g_stTestItem[1][MAX_TEST_ITEM];
struct structSCapConfEx g_stSCapConfEx;
//struct structSCapConf g_stSCapConf;
int g_TestItemNum = 0;
char g_strIcName[20] ={0};
char *g_pStoreAllData = NULL;

int GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile){
	char value[512] = {0};
	int len = 0;
	
	if(NULL == returnValue)
	{
		FTS_TEST_DBG("[FTS] returnValue==NULL in function %s.", __func__);
		return 0;
	}
	
	if(ini_get_key(IniFile, section, ItemName, value) < 0) 
	{
		if(NULL != defaultvalue) memcpy(value, defaultvalue, strlen(defaultvalue));
		sprintf(returnValue, "%s", value);
		return 0;
	} else {
		len = sprintf(returnValue, "%s", value);
	}
		
	return len;	
}

void focal_msleep(int ms)
{
	msleep(ms);
}
void SysDelay(int ms)
{
	msleep(ms);
}
int focal_abs(int value)
{
	if(value < 0)
		value = 0 - value;

	return value;
}

void OnInit_InterfaceCfg(char * strIniFile)
{
	char str[128];

	///////////////////////////IC_Type
	GetPrivateProfileString("Interface","IC_Type","FT5X36",str,strIniFile);
	g_ScreenSetParam.iSelectedIC = fts_ic_table_get_ic_code_from_ic_name(str);

	/////////////////////////Normalize Type
	GetPrivateProfileString("Interface","Normalize_Type",0,str,strIniFile);
	g_ScreenSetParam.isNormalize = atoi(str);	

}
/************************************************************************
* Name: ReadReg(Same function name as FT_MultipleTest)
* Brief:  Read Register
* Input: RegAddr
* Output: RegData
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
int ReadReg(unsigned char RegAddr, unsigned char *RegData)
{
	int iRet;

	if(NULL == fts_i2c_read_test)
		{
		FTS_TEST_DBG("[focal] %s fts_i2c_read_test == NULL \n", __func__);
		return (ERROR_CODE_INVALID_COMMAND);
		}
	
	iRet = fts_i2c_read_test(&RegAddr, 1, RegData, 1);

	if(iRet >= 0)
		return (ERROR_CODE_OK);
	else
		return (ERROR_CODE_COMM_ERROR);
}
/************************************************************************
* Name: WriteReg(Same function name as FT_MultipleTest)
* Brief:  Write Register
* Input: RegAddr, RegData
* Output: null
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
int WriteReg(unsigned char RegAddr, unsigned char RegData)
{
	int iRet;
	unsigned char cmd[2] = {0};

	if(NULL == fts_i2c_write_test)
		{
		FTS_TEST_DBG("[focal] %s fts_i2c_write_test == NULL \n", __func__);
		return (ERROR_CODE_INVALID_COMMAND);
		}
	
	cmd[0] = RegAddr;
	cmd[1] = RegData;
	iRet = fts_i2c_write_test(cmd, 2);

	if(iRet >= 0)
		return (ERROR_CODE_OK);
	else
		return (ERROR_CODE_COMM_ERROR);
}
/************************************************************************
* Name: Comm_Base_IIC_IO(Same function name as FT_MultipleTest)
* Brief:  Write/Read Data by IIC
* Input: pWriteBuffer, iBytesToWrite, iBytesToRead
* Output: pReadBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int  iBytesToWrite, unsigned char *pReadBuffer, int iBytesToRead)
{
	int iRet;	

	if(NULL == fts_i2c_read_test)
		{
		FTS_TEST_DBG("[focal] %s fts_i2c_read_test == NULL \n", __func__);
		return (ERROR_CODE_INVALID_COMMAND);
		}
	
	iRet = fts_i2c_read_test(pWriteBuffer, iBytesToWrite, pReadBuffer, iBytesToRead);

	if(iRet >= 0)
		return (ERROR_CODE_OK);
	else
		return (ERROR_CODE_COMM_ERROR);
}
/************************************************************************
* Name: EnterWork(Same function name as FT_MultipleTest)
* Brief:  Enter Work Mode
* Input: null
* Output: null
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char EnterWork(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
	if(ReCode == ERROR_CODE_OK)
	{
		if(((RunState>>4)&0x07) == 0x00)	//work
		{
			ReCode = ERROR_CODE_OK;
		}
		else
		{
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0);
			if(ReCode == ERROR_CODE_OK)
			{
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
				if(ReCode == ERROR_CODE_OK)
				{	
					if(((RunState>>4)&0x07) == 0x00)	ReCode = ERROR_CODE_OK;
					else	ReCode = ERROR_CODE_COMM_ERROR;
				}
			}
		}
	}

	return ReCode;
	
}
/************************************************************************
* Name: EnterFactory
* Brief:  enter Fcatory Mode
* Input: null
* Output: null
* Return: Comm Code. Code = 0 is OK, else fail.
***********************************************************************/
unsigned char EnterFactory(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
	if(ReCode == ERROR_CODE_OK)
	{
		if(((RunState>>4)&0x07) == 0x04)	//factory
		{
			ReCode = ERROR_CODE_OK;
		}
		else
		{
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0x40);
			if(ReCode == ERROR_CODE_OK)
			{
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
				if(ReCode == ERROR_CODE_OK)
				{	
					if(((RunState>>4)&0x07) == 0x04)	ReCode = ERROR_CODE_OK;
					else	ReCode = ERROR_CODE_COMM_ERROR;
				}
				else
					FTS_TEST_DBG("EnterFactory read DEVIDE_MODE_ADDR error 3.\n");
			}
			else
				FTS_TEST_DBG("EnterFactory write DEVIDE_MODE_ADDR error 2.\n");
		}
	}
	else
		FTS_TEST_DBG("EnterFactory read DEVIDE_MODE_ADDR error 1.\n");

	return ReCode;
}

