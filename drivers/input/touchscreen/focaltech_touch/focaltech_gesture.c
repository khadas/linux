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
* File Name: Focaltech_Gestrue.c
*
* Author: Software Department, FocalTech
*
* Created: 2016-03-17
*   
* Modify: 
*
* Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include "focaltech_comm.h"

#define  FTS_GESTRUE_EN			0//0:disable, 1: enable
//#include "ft_gesture_lib.h"
/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FTS_GESTURE_INFO  "File Version of  focaltech_gesture.c:  V1.0.0 2016-03-17"

#define  KEY_GESTURE_U								KEY_U
#define  KEY_GESTURE_UP								KEY_UP
#define  KEY_GESTURE_DOWN							KEY_DOWN
#define  KEY_GESTURE_LEFT							KEY_LEFT 
#define  KEY_GESTURE_RIGHT							KEY_RIGHT
#define  KEY_GESTURE_O								KEY_O
#define  KEY_GESTURE_E								KEY_E
#define  KEY_GESTURE_M								KEY_M 
#define  KEY_GESTURE_L								KEY_L
#define  KEY_GESTURE_W								KEY_W
#define  KEY_GESTURE_S								KEY_S 
#define  KEY_GESTURE_V								KEY_V
#define  KEY_GESTURE_Z								KEY_Z

#define GESTURE_LEFT								0x20
#define GESTURE_RIGHT								0x21
#define GESTURE_UP		    							0x22
#define GESTURE_DOWN								0x23
#define GESTURE_DOUBLECLICK						0x24
#define GESTURE_O		    							0x30
#define GESTURE_W		    							0x31
#define GESTURE_M		   	 						0x32
#define GESTURE_E		    							0x33
#define GESTURE_L		    							0x44
#define GESTURE_S		    							0x46
#define GESTURE_V		    							0x54
#define GESTURE_Z		    							0x41
#define FTS_GESTRUE_POINTS 						255
#define FTS_GESTRUE_POINTS_ONETIME  				62
#define FTS_GESTRUE_POINTS_HEADER 				8
#define FTS_GESTURE_OUTPUT_ADRESS 				0xD3
#define FTS_GESTURE_OUTPUT_UNIT_LENGTH 			4

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/


/*******************************************************************************
* Static variables
*******************************************************************************/
static short pointnum 					= 0;
static unsigned short coordinate_x[150] 	= {0};
static unsigned short coordinate_y[150] 	= {0};

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static void fts_gesture_report_key(struct input_dev *input_dev, unsigned int code);
static void fts_gesture_report(struct input_dev *input_dev,int gesture_id);
static int fts_gesture_read_data(void);

/*******************************************************************************
*   Name: fts_gesture_init
*  Brief:
*  Input:
* Output: None
* Return: None
*******************************************************************************/
int fts_gesture_init(struct input_dev *input_dev)
{
	if(0 == FTS_GESTRUE_EN)
		return 0;
	
	FTS_COMMON_DBG("[focal] %s \n", FTS_GESTURE_INFO);	//show version
	

        //init_para(480,854,60,0,0);
	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_L);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S); 
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
		
	__set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
	__set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
	__set_bit(KEY_GESTURE_UP, input_dev->keybit);
	__set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
	__set_bit(KEY_GESTURE_U, input_dev->keybit);
	__set_bit(KEY_GESTURE_O, input_dev->keybit);
	__set_bit(KEY_GESTURE_E, input_dev->keybit);
	__set_bit(KEY_GESTURE_M, input_dev->keybit);
	__set_bit(KEY_GESTURE_W, input_dev->keybit);
	__set_bit(KEY_GESTURE_L, input_dev->keybit);
	__set_bit(KEY_GESTURE_S, input_dev->keybit);
	__set_bit(KEY_GESTURE_V, input_dev->keybit);
	__set_bit(KEY_GESTURE_Z, input_dev->keybit);

	return 0;
}
/*******************************************************************************
*  Name: fts_gesture_report_key
*  Brief:	frank
*  Input:
*  Output: None
*  Return: None
*******************************************************************************/
int fts_gesture_exit(void)
{
	if(0 == FTS_GESTRUE_EN)
		return 0;
	
	return 0;
}
/*******************************************************************************
*  Name: fts_gesture_handle
*  Brief:	read gesture data, and report key event
*  Input:
*  Output: None
*  Return: <= 0: enable(<0: Error, =0: OK), >0: disable
*******************************************************************************/
int fts_gesture_handle(void)
{
	if(0 == FTS_GESTRUE_EN)
		return 1;

	if(0 < fts_gesture_read_data())
		return -1;

	return 0;
}
/*******************************************************************************
*  Name: fts_gesture_report_key
*  Brief:	frank
*  Input:
*  Output: None
*  Return: None
*******************************************************************************/
static void fts_gesture_report_key(struct input_dev *input_dev, unsigned int code)
{
	FTS_COMMON_DBG(" code = 0x%x. ",code);

	input_report_key(input_dev, code, 1);
	input_sync(input_dev);
	input_report_key(input_dev, code, 0);
	input_sync(input_dev);
}

/*******************************************************************************
*   Name: fts_gesture_report
*  Brief:
*  Input:
* Output: None
* Return: None
*******************************************************************************/
static void fts_gesture_report(struct input_dev *input_dev,int gesture_id)
{
	FTS_COMMON_DBG("fts gesture_id==0x%x\n ",gesture_id);
	
	switch(gesture_id)
	{
		case GESTURE_LEFT:
			fts_gesture_report_key(input_dev, KEY_GESTURE_LEFT);
			break;
		case GESTURE_RIGHT:
			fts_gesture_report_key(input_dev, KEY_GESTURE_RIGHT);
			break;
		case GESTURE_UP:
			fts_gesture_report_key(input_dev, KEY_GESTURE_UP);
			break;
		case GESTURE_DOWN:
			fts_gesture_report_key(input_dev, KEY_GESTURE_DOWN);
			break;
		case GESTURE_DOUBLECLICK:
			fts_gesture_report_key(input_dev, KEY_GESTURE_U);
			break;
		case GESTURE_O:
			fts_gesture_report_key(input_dev, KEY_GESTURE_O);
			break;
		case GESTURE_W:
			fts_gesture_report_key(input_dev, KEY_GESTURE_W);
			break;
		case GESTURE_M:
			fts_gesture_report_key(input_dev, KEY_GESTURE_M);
			break;
		case GESTURE_E:
			fts_gesture_report_key(input_dev, KEY_GESTURE_E);
			break;
		case GESTURE_L:
			fts_gesture_report_key(input_dev, KEY_GESTURE_L);
			break;
		case GESTURE_S:
			fts_gesture_report_key(input_dev, KEY_GESTURE_S);
			break;
		case GESTURE_V:
			fts_gesture_report_key(input_dev, KEY_GESTURE_V);
			break;
		case GESTURE_Z:
			fts_gesture_report_key(input_dev, KEY_GESTURE_Z);
			break;
		default:
			break;
	}

}

 /************************************************************************
*   Name: fts_gesture_read_data
* Brief: read data from TP register
* Input: no
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_gesture_read_data(void)
{
    unsigned char buf[FTS_GESTRUE_POINTS * 3] = { 0 };
    int ret = -1;
    int i = 0;
    int gestrue_id = 0;

	buf[0] = 0xD3;
	pointnum = 0;

	FTS_COMMON_DBG("");
	ret = fts_i2c_read(buf, 1, buf, FTS_GESTRUE_POINTS_HEADER);
        //FTS_COMMON_DBG( "tpd read FTS_GESTRUE_POINTS_HEADER.\n");

    if (ret < 0)
    {
        FTS_COMMON_DBG( "%s read touchdata failed.\n", __func__);
        return ret;
    }

    /* FW */
     if (fts_updateinfo_curr.chip_id==0x54 
	 	|| fts_updateinfo_curr.chip_id==0x58 
	 	|| fts_updateinfo_curr.chip_id==0x86 
	 	|| fts_updateinfo_curr.chip_id==0x87
	 	)
     {
	 gestrue_id = buf[0];
	 pointnum = (short)(buf[1]) & 0xff;
	 buf[0] = 0xd3;

	 if((pointnum * 4 + 2)<255)
	 {
	    	 ret = fts_i2c_read(buf, 1, buf, (pointnum * 4 + 2));
	 }
	 else
	 {
	        ret = fts_i2c_read(buf, 1, buf, 255);
	        ret = fts_i2c_read(buf, 0, buf+255, (pointnum * 4 + 2) -255);
	 }
	 if (ret < 0)
	 {
	       FTS_COMMON_DBG( "%s read touchdata failed.\n", __func__);
	       return ret;
	 }

		fts_gesture_report(fts_input_dev,gestrue_id);
	 for(i = 0;i < pointnum;i++)
	 {
	    	coordinate_x[i] =  (((s16) buf[0 + (4 * i+2)]) & 0x0F) <<
	        	8 | (((s16) buf[1 + (4 * i+2)])& 0xFF);
	    	coordinate_y[i] = (((s16) buf[2 + (4 * i+2)]) & 0x0F) <<
	        	8 | (((s16) buf[3 + (4 * i+2)]) & 0xFF);
	 }
	 return -1;
   }
   else
	{
		FTS_COMMON_DBG( "%s: IC 0x%x need gesture lib to support gestures.\n", __func__, fts_updateinfo_curr.chip_id);
	       return -1;
	}
	// other IC's gestrue in driver
	/*if (0x24 == buf[0])
	{
	    gestrue_id = 0x24;
		fts_gesture_report(fts_input_dev,gestrue_id);
		FTS_COMMON_DBG( "%d check_gesture gestrue_id.\n", gestrue_id);
	    return -1;
	}

	pointnum = (short)(buf[1]) & 0xff;
	buf[0] = 0xd3;
	if((pointnum * 4 + 8)<255)
	{
		ret = fts_i2c_read(fts_i2c_client, buf, 1, buf, (pointnum * 4 + 8));
	}
	else
	{
	     ret = fts_i2c_read(fts_i2c_client, buf, 1, buf, 255);
	     ret = fts_i2c_read(fts_i2c_client, buf, 0, buf+255, (pointnum * 4 + 8) -255);
	}
	if (ret < 0)
	{
	    FTS_COMMON_DBG( "%s read touchdata failed.\n", __func__);
	    return ret;
	}

	gestrue_id = fetch_object_sample(buf, pointnum);
	fts_gesture_report(fts_input_dev,gestrue_id);
	FTS_COMMON_DBG( "%d read gestrue_id.\n", gestrue_id);

	for(i = 0;i < pointnum;i++)
	{
	    coordinate_x[i] =  (((s16) buf[0 + (4 * i+8)]) & 0x0F) <<
	        8 | (((s16) buf[1 + (4 * i+8)])& 0xFF);
	    coordinate_y[i] = (((s16) buf[2 + (4 * i+8)]) & 0x0F) <<
	        8 | (((s16) buf[3 + (4 * i+8)]) & 0xFF);
	}
	return -1;*/
}

