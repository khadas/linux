/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: Config_FT5X46.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: Set Config for FT5X46\FT5X46i\FT5526\FT3X17\FT5436\FT3X27\FT5526i\FT5416\FT5426\FT5435
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include "focaltech_test_config_ft5822.h"
#include "focaltech_test_ini.h"
#include "focaltech_test_global.h"


struct stCfg_FT5822_TestItem g_stCfg_FT5822_TestItem;
struct stCfg_FT5822_BasicThreshold g_stCfg_FT5822_BasicThreshold;

void OnInit_FT5822_TestItem(char *strIniFile)
{
	char str[512];

	//////////////////////////////////////////////////////////// FW Version
	GetPrivateProfileString("TestItem","FW_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.FW_VERSION_TEST = atoi(str);

	//////////////////////////////////////////////////////////// Factory ID
	GetPrivateProfileString("TestItem","FACTORY_ID_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.FACTORY_ID_TEST = atoi(str);

	//////////////////////////////////////////////////////////// Project Code Test
	GetPrivateProfileString("TestItem","PROJECT_CODE_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PROJECT_CODE_TEST = atoi(str);	

	//////////////////////////////////////////////////////////// IC Version
	GetPrivateProfileString("TestItem","IC_VERSION_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.IC_VERSION_TEST = atoi(str);

	//////////////////////////////////////////////////////////// LCM ID
	GetPrivateProfileString("TestItem","LCM_ID_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.LCM_ID_TEST = atoi(str);

	/////////////////////////////////// RawData Test
	GetPrivateProfileString("TestItem","RAWDATA_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.RAWDATA_TEST = atoi(str);

	/////////////////////////////////// Adc Detect Test
	GetPrivateProfileString("TestItem","ADC_DETECT_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.ADC_DETECT_TEST = atoi(str);

	/////////////////////////////////// RX_SHORT_TEST
	//GetPrivateProfileString("TestItem","RX_SHORT_TEST","1",str,strIniFile);
	//g_stCfg_FT5822_TestItem.RX_SHORT_TEST = atoi(str);

	/////////////////////////////////// TX_SHORT_TEST
	//GetPrivateProfileString("TestItem","TX_SHORT_TEST","1",str,strIniFile);
	//g_stCfg_FT5822_TestItem.TX_SHORT_TEST = atoi(str);

	/////////////////////////////////// SCAP_CB_TEST
	GetPrivateProfileString("TestItem","SCAP_CB_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.SCAP_CB_TEST = atoi(str);

	/////////////////////////////////// SCAP_RAWDATA_TEST
	GetPrivateProfileString("TestItem","SCAP_RAWDATA_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.SCAP_RAWDATA_TEST = atoi(str);

	/////////////////////////////////// CHANNEL_NUM_TEST
	GetPrivateProfileString("TestItem","CHANNEL_NUM_TEST","1",str,strIniFile);
	g_stCfg_FT5822_TestItem.CHANNEL_NUM_TEST = atoi(str);

	/////////////////////////////////// INT_PIN_TEST
	GetPrivateProfileString("TestItem","INT_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.INT_PIN_TEST = atoi(str);

	/////////////////////////////////// RESET_PIN_TEST
	GetPrivateProfileString("TestItem","RESET_PIN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.RESET_PIN_TEST = atoi(str);

	/////////////////////////////////// NOISE_TEST
	GetPrivateProfileString("TestItem","NOISE_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.NOISE_TEST = atoi(str);

	/////////////////////////////////// SCAP_DIFFER_TEST
	//GetPrivateProfileString("TestItem","SCAP_DIFFER_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.SCAP_DIFFER_TEST = atoi(str);

	/////////////////////////////////// SCAP_CLB_TEST
	//GetPrivateProfileString("TestItem","SCAP_CLB_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.SCAP_CLB_TEST = atoi(str);

	/////////////////////////////////// RX_CROSSTALK_TEST
	//GetPrivateProfileString("TestItem","RX_CROSSTALK_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.RX_CROSSTALK_TEST = atoi(str);

	/////////////////////////////////// RAWDATA_RX_DEVIATION_TEST
	//GetPrivateProfileString("TestItem","RAWDATA_RX_DEVIATION_TEST","0",str,strIniFile);
	//g_stCfg_FT5X36_TestItem.RAWDATA_RX_DEVIATION_TEST = atoi(str);

	/////////////////////////////////// RAWDATA_UNIFORMITY_TEST
	//GetPrivateProfileString("TestItem","RAWDATA_UNIFORMITY_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.RAWDATA_UNIFORMITY_TEST = atoi(str);

	/////////////////////////////////// RX_LINEARITY_TEST
	//GetPrivateProfileString("TestItem","RX_LINEARITY_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.RX_LINEARITY_TEST = atoi(str);

	/////////////////////////////////// TX_LINEARITY_TEST
	//GetPrivateProfileString("TestItem","TX_LINEARITY_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.TX_LINEARITY_TEST = atoi(str);

	/////////////////////////////////// DIFFERDATA_UNIFORMITY_TEST
	//GetPrivateProfileString("TestItem","DIFFERDATA_UNIFORMITY_TEST","0",str,strIniFile);
	//g_stCfg_FT5822_TestItem.DIFFERDATA_UNIFORMITY_TEST = atoi(str);

	/////////////////////////////////// WEAK_SHORT_CIRCUIT_TEST
	GetPrivateProfileString("TestItem","WEAK_SHORT_CIRCUIT_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.WEAK_SHORT_CIRCUIT_TEST = atoi(str);

	/////////////////////////////////// UNIFORMITY_TEST
	GetPrivateProfileString("TestItem","UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.UNIFORMITY_TEST = atoi(str);

	/////////////////////////////////// CM_TEST
	GetPrivateProfileString("TestItem","CM_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.CM_TEST = atoi(str);

	/////////////////////////////////// RAWDATA_MARGIN_TEST
	GetPrivateProfileString("TestItem","RAWDATA_MARGIN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.RAWDATA_MARGIN_TEST = atoi(str);

	/////////////////////////////////// panel differ_TEST
	GetPrivateProfileString("TestItem","PANEL_DIFFER_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PANEL_DIFFER_TEST = atoi(str);

	/////////////////////////////////// panel differ uniformity_TEST
	GetPrivateProfileString("TestItem","PANEL_DIFFER_UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PANEL_DIFFER_UNIFORMITY_TEST = atoi(str);

	///////////////////////////////////TE_TEST
	GetPrivateProfileString("TestItem","TE_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.TE_TEST = atoi(str);

	///////////////////////////////////SITO_RAWDATA_UNIFORMITY_TEST
	GetPrivateProfileString("TestItem","SITO_RAWDATA_UNIFORMITY_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.SITO_RAWDATA_UNIFORMITY_TEST = atoi(str);

	///////////////////////////////////PATTERN_TEST
	GetPrivateProfileString("TestItem","PATTERN_TEST","0",str,strIniFile);
	g_stCfg_FT5822_TestItem.PATTERN_TEST = atoi(str);
}

void OnInit_FT5822_BasicThreshold(char * strIniFile)
{
	char str[512] = {0};

	//////////////////////////////////////////////////////////// FW Version

	GetPrivateProfileString( "Basic_Threshold", "FW_VER_VALUE", "0", str, strIniFile);
	g_stCfg_FT5822_BasicThreshold.FW_VER_VALUE = atoi(str);

	//////////////////////////////////////////////////////////// Factory ID
	GetPrivateProfileString("Basic_Threshold","Factory_ID_Number","255", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Factory_ID_Number = atoi(str);

	//////////////////////////////////////////////////////////// Project Code Test
	GetPrivateProfileString("Basic_Threshold","Project_Code"," ", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.Project_Code.Format("%s", str);
	sprintf(g_stCfg_FT5822_BasicThreshold.Project_Code, "%s", str);

	//////////////////////////////////////////////////////////// IC Version
	GetPrivateProfileString("Basic_Threshold","IC_Version","3", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.IC_Version = atoi(str);

	//////////////////////////////////////////////////////////// LCM ID
	GetPrivateProfileString("Basic_Threshold","LCM_ID","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.LCM_ID = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_Low_Min","3000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_low_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_Low_Max","15000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_Low_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_High_Min","3000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_high_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_High_Max","15000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_high_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawDataTest_LowFreq","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_SetLowFreq  = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawDataTest_HighFreq","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawDataTest_SetHighFreq = atoi(str);

	GetPrivateProfileString("Basic_Threshold","Adc_Detect_Max","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.AdcDetect_Max = atoi( str ); 

	//GetPrivateProfileString("Basic_Threshold","RxShortTest_Min","1000", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RxShortTest_Min = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","RxShortTest_Max","32800", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RxShortTest_Max = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","TxShortTest_Min","300", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.TxShortTest_Min = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","TxShortTest_Max","3000", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.TxShortTest_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapCbTest_OFF_Min","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_OFF_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapCbTest_OFF_Max","240", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_OFF_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapCbTest_ON_Min","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_ON_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapCbTest_ON_Max","240", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_ON_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ScapCBTest_SetWaterproof_OFF","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_OFF = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ScapCBTest_SetWaterproof_ON","240", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_ON = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","SCapDifferTest_Min","100", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.SCapDifferTest_Min = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","SCapDifferTest_Max","10000", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.SCapDifferTest_Max = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","SCapDifferTest_CbLevel","2", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.SCapDifferTest_CbLevel = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_OFF_Min","5000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_OFF_Max","8500", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_ON_Min","5000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_ON_Max","8500", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Max = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_SetWaterproof_OFF","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF = atoi(str);

	GetPrivateProfileString("Basic_Threshold","SCapRawDataTest_SetWaterproof_ON","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_ON = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_Mapping","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.bChannelTestMapping = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_NoMapping","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.bChannelTestNoMapping = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_TxNum","13", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_TxNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_RxNum","24", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_RxNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_Tx_NP_Num","13", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_TxNpNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","ChannelNumTest_Rx_NP_Num","24", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ChannelNumTest_RxNpNum = atoi(str);

	GetPrivateProfileString("Basic_Threshold","ResetPinTest_RegAddr","136", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.ResetPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","IntPinTest_RegAddr","79", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.IntPinTest_RegAddr = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_Max","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_Max = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Frames","32", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_Frames = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_Time","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_Time = atoi(str);
	GetPrivateProfileString("Basic_Threshold","NoiseTest_SampeMode","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_SampeMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_NoiseMode","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_NoiseMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_ShowTip","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_ShowTip = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_GloveMode","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.bNoiseTest_GloveMode = atoi(str);

	GetPrivateProfileString("Basic_Threshold","NoiseTest_RawdataMin","5000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.NoiseTest_RawdataMin = atoi(str);

	GetPrivateProfileString("Basic_Threshold","GloveNoiseTest_Coefficient","100", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.GloveNoiseTest_Coefficient = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","SCapClbTest_Frame","3", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.SCapClbTest_Frame = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","SCapClbTest_Max","1000", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.SCapClbTest_Max = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","RxCrosstalkTest_Min","-100", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RxCrosstalkTest_Min = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","RxCrosstalkTest_Max","300", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RxCrosstalkTest_Max = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","RawDataRxDeviationTest_Max","500", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RawDataRxDeviationTest_Max = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","RawDataUniformityTest_Percent","90", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RawDataUniformityTest_Percent = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","RxLinearityTest_Max","50", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.RxLinearityTest_Max = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","TxLinearityTest_Max","50", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.TxLinearityTest_Max = atoi(str);

	//GetPrivateProfileString("Basic_Threshold","DifferDataUniformityTest_Percent","80", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.DifferDataUniformityTest_Percent = atoi(str);

	GetPrivateProfileString("Basic_Threshold","WeakShortTest_CG","2000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.WeakShortTest_CG = atoi(str);
	GetPrivateProfileString("Basic_Threshold","WeakShortTest_CC","2000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.WeakShortTest_CC = atoi(str);
	//GetPrivateProfileString("Basic_Threshold","WeakShortTest_ChannelNum","38", str,strIniFile);
	//g_stCfg_FT5822_BasicThreshold.WeakShortTest_ChannelNum = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Check_Tx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_CheckTx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Check_Rx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_CheckRx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Check_MinMax","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_CheckMinMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Tx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_Tx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_Rx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_Rx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","UniformityTest_MinMax_Hole","70", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole = atoi(str);

	GetPrivateProfileString("Basic_Threshold","CMTest_Check_Min","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_CheckMin = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CMTest_Check_Max","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_CheckMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CMTest_Min_Hole","0.5", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_MinHole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","CMTest_Max_Hole","5", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.CMTest_MaxHole = atoi(str);

	GetPrivateProfileString("Basic_Threshold","RawdataMarginTest_Min","10", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawdataMarginTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","RawdataMarginTest_Max","1", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.RawdataMarginTest_Max = atoi(str);

	//panel differ
	GetPrivateProfileString("Basic_Threshold","PanelDifferTest_Min","150", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDifferTest_Min = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDifferTest_Max","1000", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDifferTest_Max = atoi(str);

	//panel differ uniformity
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Check_Tx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Check_Tx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Check_Rx","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Check_Rx = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Check_MinMax","0", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Check_MinMax = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Tx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Tx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_Rx_Hole","20", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_Rx_Hole = atoi(str);
	GetPrivateProfileString("Basic_Threshold","PanelDiffer_UniformityTest_MinMax_Hole","70", str,strIniFile);
	g_stCfg_FT5822_BasicThreshold.PanelDiffer_UniformityTest_MinMax_Hole = atoi(str);
}

void SetTestItem_FT5822()
{
	//int value = 0;
	g_TestItemNum = 0;


	//////////////////////////////////////////////////RESET_PIN_TEST
	if( g_stCfg_FT5822_TestItem.RESET_PIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_RESET_PIN_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_RESET_PIN_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////FACTORY_ID_TEST
	if( g_stCfg_FT5822_TestItem.FACTORY_ID_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_FACTORY_ID_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_FACTORY_ID_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Project Code Test
	if( g_stCfg_FT5822_TestItem.PROJECT_CODE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_PROJECT_CODE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_PROJECT_CODE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////FW Version Test
	if( g_stCfg_FT5822_TestItem.FW_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_FW_VERSION_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_FW_VERSION_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////LCM ID Test
	if( g_stCfg_FT5822_TestItem.LCM_ID_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_LCM_ID_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_LCM_ID_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////IC Version Test
	if( g_stCfg_FT5822_TestItem.IC_VERSION_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_IC_VERSION_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_IC_VERSION_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Enter Factory Mode
	g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_ENTER_FACTORY_MODE;
	//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_ENTER_FACTORY_MODE];
	g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
	g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
	g_TestItemNum++;
/*
#if ENABLE_DOUBLE_CHECK
	//////////////////////////////////////////////////judge Normalize type
	//if (g_ScreenSetParam.isNormalize != None)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_JUDEG_NORMALIZE_TYPE;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_JUDEG_NORMALIZE_TYPE];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;
	}
#endif
*/
	//////////////////////////////////////////////////TE_TEST
	if( g_stCfg_FT5822_TestItem.TE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_TE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_TE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////CHANNEL_NUM_TEST
	if( g_stCfg_FT5822_TestItem.CHANNEL_NUM_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_CHANNEL_NUM_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_CHANNEL_NUM_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////NOISE_TEST
	if( g_stCfg_FT5822_TestItem.NOISE_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_NOISE_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_NOISE_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////RawData Test
	if( g_stCfg_FT5822_TestItem.RAWDATA_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_RAWDATA_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_RAWDATA_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Rawdata Uniformity Test
	if( g_stCfg_FT5822_TestItem.UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_UNIFORMITY_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_UNIFORMITY_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////Rawdata Uniformity Test
	if( g_stCfg_FT5822_TestItem.SITO_RAWDATA_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_SITO_RAWDATA_UNIFORMITY_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_SITO_RAWDATA_UNIFORMITY_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////CM Test
	if( g_stCfg_FT5822_TestItem.CM_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_CM_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_CM_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}
	
	//////////////////////////////////////////////////Adc Detect Test
	if( g_stCfg_FT5822_TestItem.ADC_DETECT_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_ADCDETECT_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_ADCDETECT_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////SCAP_CB_TEST
	if( g_stCfg_FT5822_TestItem.SCAP_CB_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_SCAP_CB_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_SCAP_CB_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////SCAP_RAWDATA_TEST
	if( g_stCfg_FT5822_TestItem.SCAP_RAWDATA_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_SCAP_RAWDATA_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_SCAP_RAWDATA_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////RAWDATA_MARGIN_TEST
	if( g_stCfg_FT5822_TestItem.RAWDATA_MARGIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_RAWDATA_MARGIN_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_RAWDATA_MARGIN_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////WEAK_SHORT_CIRCUIT_TEST
	if( g_stCfg_FT5822_TestItem.WEAK_SHORT_CIRCUIT_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_WEAK_SHORT_CIRCUIT_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_WEAK_SHORT_CIRCUIT_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}
	
	//////////////////////////////////////////////////panel differ_TEST
	if( g_stCfg_FT5822_TestItem.PANEL_DIFFER_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_PANELDIFFER_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_PANELDIFFER_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}
	//////////////////////////////////////////////////panel differ uniformity
	if( g_stCfg_FT5822_TestItem.PANEL_DIFFER_UNIFORMITY_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_PANELDIFFER_UNIFORMITY_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_PANELDIFFER_UNIFORMITY_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}

	//////////////////////////////////////////////////INT_PIN_TEST
	if( g_stCfg_FT5822_TestItem.INT_PIN_TEST == 1)
	{
		g_stTestItem[0][g_TestItemNum].ItemCode = Code_FT5822_INT_PIN_TEST;
		//g_stTestItem[0][g_TestItemNum].strItemName = g_strEnumTestItem_FT5822[Code_FT5822_INT_PIN_TEST];
		g_stTestItem[0][g_TestItemNum].TestNum = g_TestItemNum;
		g_stTestItem[0][g_TestItemNum].TestResult= RESULT_NULL;
		g_TestItemNum++;		
	}
}


