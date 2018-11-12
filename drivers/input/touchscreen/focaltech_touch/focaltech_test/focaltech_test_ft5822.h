/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: Test_FT5822.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-23
*
* Abstract: test item for FT5822\FT5626\FT5726\FT5826B
*
************************************************************************/
#ifndef _TEST_FT5822_H
#define _TEST_FT5822_H

#include "focaltech_test_main.h"

boolean FT5822_StartTest(void);
int FT5822_get_test_data(char *pTestData);//pTestData, External application for memory, buff size >= 1024*80

unsigned char FT5822_TestItem_EnterFactoryMode(void);
unsigned char FT5822_TestItem_RawDataTest(bool * bTestResult);
//unsigned char FT5822_TestItem_ChannelsTest(bool * bTestResult);
unsigned char FT5822_TestItem_SCapRawDataTest(bool* bTestResult);
unsigned char FT5822_TestItem_SCapCbTest(bool* bTestResult);

unsigned char FT5822_TestItem_UniformityTest(bool * bTestResult);

boolean GetWaterproofMode(int iTestType, unsigned char ucChannelValue);


#endif
