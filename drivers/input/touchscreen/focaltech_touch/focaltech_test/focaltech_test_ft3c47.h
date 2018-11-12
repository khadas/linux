/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: Test_FT3C47.h
*
* Author: Software Development Team, AE
*
* Created: 2015-12-02
*
* Abstract: test item for FT3C47
*
************************************************************************/
#ifndef _TEST_FT3C47_H
#define _TEST_FT3C47_H

#include "focaltech_test_main.h"

boolean FT3C47_StartTest(void);
int FT3C47_get_test_data(char *pTestData);//pTestData, External application for memory, buff size >= 1024*80

unsigned char FT3C47_TestItem_EnterFactoryMode(void);
unsigned char FT3C47_TestItem_RawDataTest(bool * bTestResult);
//unsigned char FT3C47_TestItem_ChannelsTest(bool * bTestResult);
unsigned char FT3C47_TestItem_SCapRawDataTest(bool* bTestResult);
unsigned char FT3C47_TestItem_SCapCbTest(bool* bTestResult);

unsigned char FT3C47_TestItem_ForceTouch_SCapRawDataTest(bool* bTestResult);
unsigned char FT3C47_TestItem_ForceTouch_SCapCbTest(bool* bTestResult);

boolean GetWaterproofMode(int iTestType, unsigned char ucChannelValue);


#endif
