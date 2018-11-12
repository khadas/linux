/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: Config_FT8606.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: Set Config for FT8606
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include "focaltech_test_config_ft8606.h"
#include "focaltech_test_ini.h"
#include "focaltech_test_global.h"
//#include "focaltech_test_main.h"


/*
stCfg_FT8606_TestItem g_stCfg_FT8606_TestItem;
stCfg_FT8606_BasicThreshold g_stCfg_FT8606_BasicThreshold;

CString g_strEnumTestItem_FT8606[] =
{	
	" Enter Factory Mode",
	" Download",
	" Upgrade",
	" Factory ID Test",
	" Project Code Test",
	" FW Version Test",
	" IC Version Test",
	" RawData Test",
	" Channel Num Test",
	//" Channel Short Test",
	" Int Pin Test",
	" Reset pin Test",
	" Noise Test",
	" CB Test",
	//" Delta CB Test",
	//" Channels Deviation Test",
	//" Two Sides Deviation Test",
	//" FPCBA Short Test",
	//" FPCBA Open Test",
	//" SREF Open Test",
	//" TE Test",
	//" CB Deviation Test",
	" Write Config",
	//" Differ Test",
	" Short Circuit Test",
};
*/

struct stCfg_FT8606_TestItem g_stCfg_FT8606_TestItem;
struct stCfg_FT8606_BasicThreshold g_stCfg_FT8606_BasicThreshold;

void OnInit_FT8606_BasicThreshold(char *strIniFile)
{
	char str[512];

	//FTS_TEST_DBG("Enter  OnInit_FT8606_BasicThreshold \n");

	//////////////////////////////////////////////////////////// FW Version

	GetPrivateProfileString( "Basic_Threshold", "FW_VER_VALUE", "0",str, strIniFile);
	g_stCfg_FT8606_BasicThreshold.FW_VER_VALUE = atoi(str);

	//////////////////////////////////////////////////////////// Factory ID
	GetPrivateProfileString("Basic_Threshold","Factory_ID_Number","255",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.Factory_ID_Number = atoi(str);

	//////////////////////////////////////////////////////////// Project Code Test
	GetPrivateProfileString("Basic_Threshold","Project_Code"," ",str,strIniFile);
	//g_stCfg_FT8606_BasicThreshold.Project_Code.Format("%s", str);
	sprintf(g_stCfg_FT8606_BasicThreshold.Project_Code, "%s", str);

	//////////////////////////////////////////////////////////// IC Version
	GetPrivateProfileString("Basic_Threshold","IC_Version","3",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.IC_Version = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_Min","5000",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.RawDataTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_Max","11000",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.RawDataTest_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_ChannelX","15",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelXNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_ChannelY","24",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.ChannelNumTest_ChannelYNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_KeyNum","0",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.ChannelNumTest_KeyNum = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ResetPinTest_RegAddr","136",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.ResetPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","IntPinTest_RegAddr","175",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.IntPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_Coefficient","50",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.NoiseTest_Coefficient = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Frames","32",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.NoiseTest_Frames = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Time","1",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.NoiseTest_Time = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_SampeMode","0",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.NoiseTest_SampeMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_NoiseMode","0",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.NoiseTest_NoiseMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_ShowTip","0",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.NoiseTest_ShowTip = atoi(str);

	GetPrivateProfileString("Basic_Threshold","CBTest_Min","3",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.CbTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_Max","100",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.CbTest_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ShortCircuit_CBMax","120",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.ShortTest_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ShortCircuit_K2Value","150",str,strIniFile);
	g_stCfg_FT8606_BasicThreshold.ShortTest_K2Value = atoi(str);

	//FTS_TEST_DBG("Exit  OnInit_FT8606_BasicThreshold \n");
	
}

void OnInit_FT8606_TestItem(char *strIniFile)
{
	char str[512];

	//FTS_TEST_DBG("Enter  OnInit_FT8606_TestItem \n");

	//////////////////////////////////////////////////////////// FW Version
	GetPrivateProfileString("TestItem","FW_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.FW_VERSION_TEST = atoi(str);

	//////////////////////////////////////////////////////////// Factory ID
	GetPrivateProfileString("TestItem","FACTORY_ID_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.FACTORY_ID_TEST = atoi(str);

	//////////////////////////////////////////////////////////// Project Code Test
	GetPrivateProfileString("TestItem","PROJECT_CODE_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.PROJECT_CODE_TEST = atoi(str);	

	//////////////////////////////////////////////////////////// IC Version
	GetPrivateProfileString("TestItem","IC_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.IC_VERSION_TEST = atoi(str);

	/////////////////////////////////// RawData Test
	GetPrivateProfileString("TestItem","RAWDATA_TEST","1",str,strIniFile);
	g_stCfg_FT8606_TestItem.RAWDATA_TEST = atoi(str);

	/////////////////////////////////// CHANNEL_NUM_TEST
	GetPrivateProfileString("TestItem","CHANNEL_NUM_TEST","1",str,strIniFile);
	g_stCfg_FT8606_TestItem.CHANNEL_NUM_TEST = atoi(str);

	/////////////////////////////////// INT_PIN_TEST
	GetPrivateProfileString("TestItem","INT_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.INT_PIN_TEST = atoi(str);

	/////////////////////////////////// RESET_PIN_TEST
	GetPrivateProfileString("TestItem","RESET_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.RESET_PIN_TEST = atoi(str);

	/////////////////////////////////// NOISE_TEST
	GetPrivateProfileString("TestItem","NOISE_TEST","0",str,strIniFile);
	g_stCfg_FT8606_TestItem.NOISE_TEST = atoi(str);

	/////////////////////////////////// CB_TEST
	GetPrivateProfileString("TestItem","CB_TEST","1",str,strIniFile);
	g_stCfg_FT8606_TestItem.CB_TEST = atoi(str);

	/////////////////////////////////// SHORT_CIRCUIT_TEST
	GetPrivateProfileString("TestItem","SHORT_CIRCUIT_TEST","1",str,strIniFile);
	g_stCfg_FT8606_TestItem.SHORT_TEST = atoi(str);

	//FTS_TEST_DBG("Exit  OnInit_FT8606_TestItem \n");
	
}

void SetTestItem_FT8606()
{
	g_TestItemNum = 0;

	//////////////////////////////////////////////////FACTORY_ID_TEST
	if( g_stCfg_FT8606_TestItem.FACTORY_ID_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_FACTORY_ID_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_FACTORY_ID_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;		
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Project Code Test
	if( g_stCfg_FT8606_TestItem.PROJECT_CODE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_PROJECT_CODE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_PROJECT_CODE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////FW Version Test
	if( g_stCfg_FT8606_TestItem.FW_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_FW_VERSION_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_FW_VERSION_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////IC Version Test
	if( g_stCfg_FT8606_TestItem.IC_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_IC_VERSION_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_IC_VERSION_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Enter Factory Mode
	g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_ENTER_FACTORY_MODE;
	//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_ENTER_FACTORY_MODE];
	g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
	g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
	g_TestItemNum++;

	//////////////////////////////////////////////////CHANNEL_NUM_TEST
	if( g_stCfg_FT8606_TestItem.CHANNEL_NUM_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_CHANNEL_NUM_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_CHANNEL_NUM_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Short Test
	if( g_stCfg_FT8606_TestItem.SHORT_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_SHORT_CIRCUIT_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_SHORT_CIRCUIT_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////CB_TEST
	if( g_stCfg_FT8606_TestItem.CB_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_CB_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_CB_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////NOISE_TEST
	if( g_stCfg_FT8606_TestItem.NOISE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_NOISE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_NOISE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////RawData Test
	if( g_stCfg_FT8606_TestItem.RAWDATA_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_RAWDATA_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_RAWDATA_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////RESET_PIN_TEST
	if( g_stCfg_FT8606_TestItem.RESET_PIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_RESET_PIN_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_RESET_PIN_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}
	//////////////////////////////////////////////////INT_PIN_TEST
	if( g_stCfg_FT8606_TestItem.INT_PIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8606_INT_PIN_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8606[Code_FT8606_INT_PIN_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

}

