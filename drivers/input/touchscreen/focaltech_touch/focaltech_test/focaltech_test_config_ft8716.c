/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: Config_FT8716.c
*
* Author: Software Development Team, AE
*
* Created: 2015-12-24
*
* Abstract: Set Config for FT8716
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include "focaltech_test_config_ft8716.h"
#include "focaltech_test_ini.h"
#include "focaltech_test_global.h"
//#include "focaltech_test_main.h"

struct stCfg_FT8716_TestItem g_stCfg_FT8716_TestItem;
struct stCfg_FT8716_BasicThreshold g_stCfg_FT8716_BasicThreshold;

void OnInit_FT8716_TestItem(char *strIniFile)
{
	char str[512];

	//FTS_TEST_DBG("Enter  OnInit_FT8606_TestItem \n");


	//////////////////////////////////////////////////////////// FW Version
	GetPrivateProfileString("TestItem","FW_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.FW_VERSION_TEST = atoi(str);

	//////////////////////////////////////////////////////////// Factory ID
	GetPrivateProfileString("TestItem","FACTORY_ID_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.FACTORY_ID_TEST = atoi(str);

	//////////////////////////////////////////////////////////// Project Code Test
	GetPrivateProfileString("TestItem","PROJECT_CODE_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.PROJECT_CODE_TEST = atoi(str);	

	//////////////////////////////////////////////////////////// IC Version
	GetPrivateProfileString("TestItem","IC_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.IC_VERSION_TEST = atoi(str);

	/////////////////////////////////// RawData Test
	GetPrivateProfileString("TestItem","RAWDATA_TEST","1",str,strIniFile);
	g_stCfg_FT8716_TestItem.RAWDATA_TEST = atoi(str);

	/////////////////////////////////// CHANNEL_NUM_TEST
	GetPrivateProfileString("TestItem","CHANNEL_NUM_TEST","1",str,strIniFile);
	g_stCfg_FT8716_TestItem.CHANNEL_NUM_TEST = atoi(str);

	/////////////////////////////////// INT_PIN_TEST
	GetPrivateProfileString("TestItem","INT_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.INT_PIN_TEST = atoi(str);

	/////////////////////////////////// RESET_PIN_TEST
	GetPrivateProfileString("TestItem","RESET_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.RESET_PIN_TEST = atoi(str);

	/////////////////////////////////// NOISE_TEST
	GetPrivateProfileString("TestItem","NOISE_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.NOISE_TEST = atoi(str);

	/////////////////////////////////// CB_TEST
	GetPrivateProfileString("TestItem","CB_TEST","1",str,strIniFile);
	g_stCfg_FT8716_TestItem.CB_TEST = atoi(str);

	/////////////////////////////////// SHORT_CIRCUIT_TEST
	GetPrivateProfileString("TestItem","SHORT_CIRCUIT_TEST","1",str,strIniFile);
	g_stCfg_FT8716_TestItem.SHORT_TEST = atoi(str);

	/////////////////////////////////// OPEN_TEST
	GetPrivateProfileString("TestItem","OPEN_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.OPEN_TEST = atoi(str);

	/////////////////////////////////// CB_UNIFORMITY_TEST
	GetPrivateProfileString("TestItem","CB_UNIFORMITY_TEST","1",str,strIniFile);
	g_stCfg_FT8716_TestItem.CB_UNIFORMITY_TEST = atoi(str);

	/////////////////////////////////// DIFFER_UNIFORMITY_TEST
	GetPrivateProfileString("TestItem","DIFFER_UNIFORMITY_TEST","1",str,strIniFile);
	g_stCfg_FT8716_TestItem.DIFFER_UNIFORMITY_TEST = atoi(str);

	/////////////////////////////////// DIFFER2_UNIFORMITY_TEST
	GetPrivateProfileString("TestItem","DIFFER2_UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT8716_TestItem.DIFFER2_UNIFORMITY_TEST = atoi(str);
}

void OnInit_FT8716_BasicThreshold(char *strIniFile)
{
	char str[512];

	//////////////////////////////////////////////////////////// FW Version

	GetPrivateProfileString( "Basic_Threshold", "FW_VER_VALUE", "0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.FW_VER_VALUE = atoi(str);

	//////////////////////////////////////////////////////////// Factory ID
	GetPrivateProfileString("Basic_Threshold","Factory_ID_Number","255",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Factory_ID_Number = atoi(str);

	//////////////////////////////////////////////////////////// Project Code Test
	GetPrivateProfileString("Basic_Threshold","Project_Code"," ",str,strIniFile);
	//g_stCfg_FT8716_BasicThreshold.Project_Code.Format("%s", str);
	sprintf(g_stCfg_FT8716_BasicThreshold.Project_Code, "%s", str);

	//////////////////////////////////////////////////////////// IC Version
	GetPrivateProfileString("Basic_Threshold","IC_Version","3",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.IC_Version = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_Min","5000",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.RawDataTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_Max","11000",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.RawDataTest_Max = atoi(str);


	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_ChannelX","15",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelXNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_ChannelY","24",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ChannelNumTest_ChannelYNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_KeyNum","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ChannelNumTest_KeyNum = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ResetPinTest_RegAddr","136",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ResetPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","IntPinTest_RegAddr","175",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.IntPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_Coefficient","50",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_Coefficient = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Frames","32",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_Frames = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Time","1",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_Time = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_SampeMode","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_SampeMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_NoiseMode","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_NoiseMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_ShowTip","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.NoiseTest_ShowTip = atoi(str);

	/*GetPrivateProfileString("Basic_Threshold","CBTest_Min","3",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_Max","100",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Max = atoi(str);*/

	GetPrivateProfileString("Basic_Threshold","CBTest_VA_Check","1",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.bCBTest_VA_Check = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_Min","3",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_Max","100",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_VKey_Check","1",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.bCBTest_VKey_Check = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_Min_Vkey","3",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Min_Vkey = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBTest_Max_Vkey","100",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CbTest_Max_Vkey = atoi(str);


	GetPrivateProfileString("Basic_Threshold","ShortCircuit_ResMin","1200",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ShortCircuit_ResMin = atoi(str);
	/*GetPrivateProfileString("Basic_Threshold","ShortCircuit_K2Value","150",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.ShortTest_K2Value = atoi(str);*/

	//open test
	GetPrivateProfileString("Basic_Threshold","OpenTest_CBMin","100",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.OpenTest_CBMin = atoi(str);

	//CB_Uniformity_test
	GetPrivateProfileString("Basic_Threshold","CBUniformityTest_Check_CHX","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_Check_CHX = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBUniformityTest_Check_CHY","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_Check_CHY = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBUniformityTest_Check_MinMax","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_Check_MinMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBUniformityTest_CHX_Hole","20",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_CHX_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBUniformityTest_CHY_Hole","20",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_CHY_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CBUniformityTest_MinMax_Hole","70",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.CBUniformityTest_MinMax_Hole = atoi(str);

	//DifferUniformityTest
	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_Delta_Vol","1",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DeltaVol = atoi(str);

	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_Check_CHX","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_Check_CHX = atoi(str);
	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_Check_CHY","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_Check_CHY = atoi(str);
	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_Check_MinMax","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_Check_MinMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_CHX_Hole","20",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_CHX_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_CHY_Hole","20",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_CHY_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","DifferUniformityTest_MinMax_Hole","70",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.DifferUniformityTest_MinMax_Hole = atoi(str);

	//Differ2UniformityTest
	GetPrivateProfileString("Basic_Threshold","Differ2UniformityTest_Check_CHX","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Check_CHX = atoi(str);
	GetPrivateProfileString("Basic_Threshold","Differ2UniformityTest_Check_CHY","0",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Check_CHY = atoi(str);

	GetPrivateProfileString("Basic_Threshold","Differ2UniformityTest_CHX_Hole","20",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_CHX_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","Differ2UniformityTest_CHY_Hole","20",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_CHY_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","Differ2UniformityTest_Differ_Min","1000",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Differ_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","Differ2UniformityTest_Differ_Max","8000",str,strIniFile);
	g_stCfg_FT8716_BasicThreshold.Differ2UniformityTest_Differ_Max = atoi(str);

}
void SetTestItem_FT8716()
{
	g_TestItemNum = 0;

	//////////////////////////////////////////////////FACTORY_ID_TEST
	if( g_stCfg_FT8716_TestItem.FACTORY_ID_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_FACTORY_ID_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_FACTORY_ID_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Project Code Test
	if( g_stCfg_FT8716_TestItem.PROJECT_CODE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_PROJECT_CODE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_PROJECT_CODE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////FW Version Test
	if( g_stCfg_FT8716_TestItem.FW_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_FW_VERSION_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_FW_VERSION_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////IC Version Test
	if( g_stCfg_FT8716_TestItem.IC_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_IC_VERSION_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_IC_VERSION_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Enter Factory Mode
	g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_ENTER_FACTORY_MODE;
	//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_ENTER_FACTORY_MODE];
	g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
	g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
	g_TestItemNum++;

	//////////////////////////////////////////////////CHANNEL_NUM_TEST
	if( g_stCfg_FT8716_TestItem.CHANNEL_NUM_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_CHANNEL_NUM_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_CHANNEL_NUM_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////CB_TEST
	if( g_stCfg_FT8716_TestItem.CB_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_CB_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_CB_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////CB_UNIFORMITY_TEST
	if( g_stCfg_FT8716_TestItem.CB_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_CB_UNIFORMITY_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_CB_UNIFORMITY_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////NOISE_TEST
	if( g_stCfg_FT8716_TestItem.NOISE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_NOISE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_NOISE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}


	//////////////////////////////////////////////////RawData Test
	if( g_stCfg_FT8716_TestItem.RAWDATA_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_RAWDATA_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_RAWDATA_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////DIFFER_UNIFORMITY_TEST
	if( g_stCfg_FT8716_TestItem.DIFFER_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_DIFFER_UNIFORMITY_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_DIFFER_UNIFORMITY_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	
	//////////////////////////////////////////////////OPEN_TEST
	if( g_stCfg_FT8716_TestItem.OPEN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_OPEN_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_OPEN_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Short Test
	if( g_stCfg_FT8716_TestItem.SHORT_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_SHORT_CIRCUIT_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_SHORT_CIRCUIT_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////DIFFER2_UNIFORMITY_TEST
	if( g_stCfg_FT8716_TestItem.DIFFER2_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT8716_DIFFER2_UNIFORMITY_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT8716[Code_FT8716_DIFFER2_UNIFORMITY_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].ItemType = RESULT_NULL;
		g_TestItemNum++;		
	}


}

