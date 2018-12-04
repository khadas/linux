/*
 *
 * FocalTech TouchScreen driver.
 * 
 * Copyright (c) 2010-2016, Focaltech Ltd. All rights reserved.
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
* File Name: focaltech_monitor.c
*
* Author: Software Department, FocalTech
*
* Created: 2016-02-01
*
* Modified:
*
*  Abstract:
*
*本模块实时监测上报给Input子系统的触摸数据，判断其有效性，
*遇到异常情况做UP 动作修补。
*
*This module real-time monitoring the touch data to report to the Input subsystem, to judge its effectiveness, 
*to do UP action to repair the abnormal situation.
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include "focaltech_comm.h"
/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FTS_MONITOR_INFO  "File Version of  focaltech_monitor.c:  V1.0.0 2016-03-16"

#define SEM_WAIT_TIME 		200//ms
#define MAX_TOUCH_POINTS	0x0F
#define DOWN_EVENT			0
#define UP_EVENT			1
#define CONTACT_EVENT		2
#define NO_EVENT			3
#define NO_TOUCH			0xffff

#define MT_MAX_TOUCH_POINTS				10
#define FTS_MAX_ID							0x0F

#define FTS_FACE_DETECT_POS				1

#define FTS_TOUCH_STEP						6
#define FTS_START_ADDR						0x00//the start addr of get touch data
#define FTS_TOUCH_X_H_POS					(3 - FTS_START_ADDR)
#define FTS_TOUCH_X_L_POS					(4 - FTS_START_ADDR)
#define FTS_TOUCH_Y_H_POS					(5 - FTS_START_ADDR)
#define FTS_TOUCH_Y_L_POS					(6 - FTS_START_ADDR)
#define FTS_TOUCH_EVENT_POS				(3 - FTS_START_ADDR)
#define FTS_TOUCH_ID_POS					(5 - FTS_START_ADDR)
#define FT_TOUCH_POINT_NUM				(2 - FTS_START_ADDR)

#define DISTANCE_BETWEEN_TWO_POINTS		100
#define DISTANCE_JUGDE_SWITCH	 			1    //0=close, 1=open, default 0
/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
struct struct_touch_data
{
	unsigned char point_id[MAX_TOUCH_POINTS];
	unsigned char event[MAX_TOUCH_POINTS];	
	unsigned int X[MAX_TOUCH_POINTS];
	unsigned int Y[MAX_TOUCH_POINTS];
	struct timeval record_time;	
};
struct struct_monitor
{
	int iReportAct;
	int bReviewTouchData;
	int current_report_num;
	int total_report_num;
	struct timeval iLastRecordTime;
	struct input_dev *dev;
	bool bUseProtoclB;
	struct struct_touch_data last_data;
	struct struct_touch_data current_data;
	struct struct_touch_data report_data;	
};

/*******************************************************************************
* Static variables
*******************************************************************************/
static struct timeval tv;	
static struct struct_monitor st_monitor;
static struct task_struct *thread_monitor = NULL;

static DECLARE_WAIT_QUEUE_HEAD(monitorwaiter);
static int monitor_waiter_flag=0;

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
//static int fts_monitor_init(void);
//static int fts_monitor_exit(void);
static int fts_monitor_timeout(void *unused);
//static int fts_monitor_record(char *readbuf, unsigned char readlen);
static int fts_monitor_compare(void);
static int fts_monitor_review(void);
static int fts_parse_touch_data(char *readbuf, unsigned char readlen);
static int clear_last_data(void);
static int clear_current_data(void);
static int clear_report_data(void);
static int fts_report_added(void);
static int fts_report_a_protocol(void);
static int fts_report_b_protocol(void);

/*calculate distance*/
static int focal_abs(int value);
static unsigned int SqrtNew(unsigned int n) ;
unsigned int DistanceToPoint(unsigned int point_x1, unsigned int point_y1,unsigned int point_x2,unsigned int point_y2);
static int fts_monitor_distance(void);

/*******************************************************************************
* functions
*******************************************************************************/
int fts_monitor_init(struct input_dev *dev,  int bUseProtocolB)
{
	int err = 0;
	
	FTS_COMMON_DBG("[focal] %s \n", FTS_MONITOR_INFO);	//show version
	
	st_monitor.iReportAct = 1;
	st_monitor.bReviewTouchData = 1;
	st_monitor.total_report_num = 0;
	st_monitor.dev = dev;
	st_monitor.bUseProtoclB = bUseProtocolB;
	clear_current_data();
	clear_last_data();
	clear_report_data();
	do_gettimeofday(&st_monitor.iLastRecordTime);
	
	thread_monitor = kthread_run(fts_monitor_timeout, 0, "focal-monitor");
	if (IS_ERR(thread_monitor))
	{
		err = PTR_ERR(thread_monitor);
		FTS_COMMON_DBG("failed to create kernel thread: %d\n", err);
	}

	return 0;	
}
int fts_monitor_exit(void)
{
	st_monitor.iReportAct = 0;
	msleep(SEM_WAIT_TIME);
	kthread_stop(thread_monitor);
	return 0;
}
static int fts_monitor_timeout(void *unused)
{
	unsigned int iDeltaTime=0;
//	int ret = 0;
	unsigned long uljiffies=0;

	struct sched_param param = { .sched_priority = 5 };
	sched_setscheduler(current, SCHED_RR, &param); 
	uljiffies=msecs_to_jiffies(SEM_WAIT_TIME+20);
	
	do
	{		
		wait_event_interruptible(monitorwaiter, 0 != monitor_waiter_flag);
		monitor_waiter_flag=0;
		FTS_COMMON_DBG("begin monitor: monitor_waiter_flag reset to %d .\n", monitor_waiter_flag);
		do_gettimeofday(&st_monitor.iLastRecordTime);

		st_monitor.bReviewTouchData=1;//first
		
		do
		{
			wait_event_interruptible_timeout(monitorwaiter,0 == st_monitor.bReviewTouchData, uljiffies);
			/*if(1 == st_monitor.bReviewTouchData)
			{
				msleep(SEM_WAIT_TIME);
				do_gettimeofday(&st_monitor.iLastRecordTime);
				continue;
			}*/
			do_gettimeofday(&tv);
			iDeltaTime = (tv.tv_sec - st_monitor.iLastRecordTime.tv_sec)*MSEC_PER_SEC + (tv.tv_usec - st_monitor.iLastRecordTime.tv_usec)/1000;
			//st_monitor.iLastRecordTime.tv_sec=tv.tv_sec;
			//st_monitor.iLastRecordTime.tv_usec=tv.tv_usec;
					
			if(SEM_WAIT_TIME < iDeltaTime)
			{
				FTS_COMMON_DBG("enter fts_monitor_review(): iDeltaTime(ms) %d .\n", iDeltaTime);

				if(0 == st_monitor.bReviewTouchData)
				{
					fts_monitor_review();
				}				

				monitor_waiter_flag=0;
				
				break;
			}

			//FTS_COMMON_DBG("monitor waiter loop: iDeltaTime(ms) %d .\n", iDeltaTime);
			st_monitor.bReviewTouchData=1;

			//msleep(SEM_WAIT_TIME);
		}while(!kthread_should_stop());
	}while(!kthread_should_stop());

	return 0;
}

int fts_monitor_record(char *readbuf, unsigned char readlen)
{
	int iret = 0;
	
	do_gettimeofday(&st_monitor.iLastRecordTime);
	//st_monitor.iLastRecordTime = tv.tv_usec;	
	st_monitor.bReviewTouchData = 0;
	iret = fts_parse_touch_data(readbuf, readlen);
	if(iret >= 0)
	{
		st_monitor.current_data.record_time = st_monitor.iLastRecordTime;
		
		#if DISTANCE_JUGDE_SWITCH
		fts_monitor_distance();
		#endif
		
		fts_monitor_compare();
	}

	monitor_waiter_flag=1;
	wake_up_interruptible(&monitorwaiter);
	
	return 0;
}

static int fts_monitor_compare(void)
{

	int i = 0;
	
	clear_report_data();	
	for(i=0; i<MAX_TOUCH_POINTS; i++)
	{
		switch(st_monitor.last_data.event[i])
		{
			case NO_EVENT:
			case UP_EVENT:
			{	
				break;
			}
			case CONTACT_EVENT:
			case DOWN_EVENT:
			{
				if((DOWN_EVENT == st_monitor.current_data.event[i])
					||(NO_EVENT == st_monitor.current_data.event[i])
					)
				{
					st_monitor.report_data.point_id[st_monitor.current_report_num] = i;
					st_monitor.report_data.event[st_monitor.current_report_num] = UP_EVENT;
					st_monitor.report_data.X[st_monitor.current_report_num] = st_monitor.last_data.X[i];
					st_monitor.report_data.Y[st_monitor.current_report_num] = st_monitor.last_data.Y[i];					
					st_monitor.current_report_num++;	
					FTS_COMMON_DBG("Lost Up Event by fts_monitor_compare() ! Point_id = %d, X = %d, Y = %d\n",
						i,  st_monitor.last_data.X[i],  st_monitor.last_data.Y[i]);
				}					
				break;
			}
			default:
				break;
		}
		st_monitor.last_data.event[i] = st_monitor.current_data.event[i];
		st_monitor.last_data.X[i] = st_monitor.current_data.X[i];
		st_monitor.last_data.Y[i] = st_monitor.current_data.Y[i];	
		
	}
	st_monitor.last_data.record_time = st_monitor.current_data.record_time;
	
	if(st_monitor.current_report_num > 0)
	{
		fts_report_added();
	}
	return 0;
}

static int fts_monitor_review(void)
{
	int i = 0;	

	st_monitor.bReviewTouchData = 1;

	clear_report_data();
	for(i=0; i<MAX_TOUCH_POINTS; i++)
	{
		switch(st_monitor.last_data.event[i])
		{
			case CONTACT_EVENT:
			case DOWN_EVENT:
			{
				st_monitor.report_data.point_id[st_monitor.current_report_num] = i;
				st_monitor.report_data.event[st_monitor.current_report_num] = UP_EVENT;
				st_monitor.report_data.X[st_monitor.current_report_num] = st_monitor.last_data.X[i];
				st_monitor.report_data.Y[st_monitor.current_report_num] = st_monitor.last_data.Y[i];					
				st_monitor.current_report_num++;		
				FTS_COMMON_DBG("Lost Up Event by fts_monitor_review() ! Point_id = %d, X = %d, Y = %d\n",
					i,  st_monitor.last_data.X[i],  st_monitor.last_data.Y[i]);
				break;
			}
			default:
				break;
		}
	}

	if(st_monitor.current_report_num > 0)
	{
		fts_report_added();
	}
	return 0;
}

static int clear_last_data(void)
{
	int i = 0;
	for(i=0; i<MAX_TOUCH_POINTS; i++)
	{
		st_monitor.last_data.event[i] = NO_EVENT;
		st_monitor.last_data.point_id[i] = i;
		st_monitor.last_data.X[i] = NO_TOUCH;
		st_monitor.last_data.Y[i] = NO_TOUCH;
	}
	do_gettimeofday(&st_monitor.last_data.record_time);

	return 0;
}

static int clear_current_data(void)
{
	int i = 0;
	for(i=0; i<MAX_TOUCH_POINTS; i++)
	{
		st_monitor.current_data.event[i] = NO_EVENT;
		st_monitor.current_data.point_id[i] = i;
		st_monitor.current_data.X [i]= NO_TOUCH;
		st_monitor.current_data.Y[i] = NO_TOUCH;
	}
	do_gettimeofday(&st_monitor.current_data.record_time);

	return 0;
}
static int clear_report_data(void)
{
	int i = 0;
	
	for(i=0; i<MAX_TOUCH_POINTS; i++)
	{
		st_monitor.report_data.event[i] = NO_EVENT;
		st_monitor.report_data.point_id[i] = i;
		st_monitor.report_data.X[i] = NO_TOUCH;
		st_monitor.report_data.Y[i] = NO_TOUCH;
	}
	do_gettimeofday(&st_monitor.report_data.record_time);
	st_monitor.current_report_num = 0;
	
	return 0;
}
static int fts_parse_touch_data(char *readbuf, unsigned char readlen)
{
	int i=0;
	unsigned char id=0;
	unsigned char point_num = 0;

	point_num = readbuf[1]&0x0f;
	if(point_num > MAX_TOUCH_POINTS)
	{
		FTS_COMMON_DBG("Error Touch Data ! \n");
		return -1;
	}
	else
	{
		clear_current_data();

		for(i=0; i<MAX_TOUCH_POINTS &&((3-FTS_START_ADDR+(i+1)*FTS_TOUCH_STEP) <= readlen); i++)
		{
			id = ((readbuf[FTS_TOUCH_ID_POS+i*FTS_TOUCH_STEP])>>4); //y轴高字节第 1-5bit
			//st_monitor.current_data.point_id[id] = id;
			if(id < MAX_TOUCH_POINTS)
			{
				st_monitor.current_data.event[id] = ((readbuf[FTS_TOUCH_EVENT_POS+i*FTS_TOUCH_STEP])>>6);  //x轴高字节2bit, event
				if(NO_EVENT !=st_monitor.current_data.event[id])
				{
					st_monitor.current_data.X[id] = ((readbuf[FTS_TOUCH_X_H_POS +i*FTS_TOUCH_STEP]&0x0f)<<8)
						+readbuf[FTS_TOUCH_X_L_POS+i*FTS_TOUCH_STEP];
					st_monitor.current_data.Y[id] = ((readbuf[FTS_TOUCH_Y_H_POS+i*FTS_TOUCH_STEP]&0x0f)<<8)
						+readbuf[FTS_TOUCH_Y_L_POS+i*FTS_TOUCH_STEP];
				}
			}	
		}
	}
	
	return 0;
}
static int fts_report_added(void)
{
	st_monitor.total_report_num += st_monitor.current_report_num;

	FTS_COMMON_DBG("Add Up Event ! Total Added Number = %d\n", st_monitor.total_report_num);
	if(st_monitor.bUseProtoclB)
		fts_report_b_protocol();
	else
		fts_report_a_protocol();	
	
	return 0;
}
static int fts_report_a_protocol(void)
{
	input_report_key(st_monitor.dev, BTN_TOUCH, 0);
	input_mt_sync(st_monitor.dev);	
	input_sync(st_monitor.dev);
	return 0;
}
static int fts_report_b_protocol(void)
{
	int i = 0;

	for(i = 0; i < st_monitor.current_report_num; i++)
	{
		input_mt_slot(st_monitor.dev, st_monitor.report_data.point_id[i]);
		input_mt_report_slot_state(st_monitor.dev, MT_TOOL_FINGER, false);
	}
	input_report_key(st_monitor.dev, BTN_TOUCH, 0);
	input_sync(st_monitor.dev);
	return 0;
}

static int focal_abs(int value)
{
	if(value < 0)
		value = 0 - value;

	return value;
}

/************************************************************************
* Name: SqrtNew
* Brief:  calculate sqrt of input.
* Input: unsigned int n
* Output: none
* Return: sqrt of n.
***********************************************************************/
static unsigned int SqrtNew(unsigned int n) 
{        
    unsigned int  val = 0, last = 0; 
    unsigned char i = 0;;
    
    if (n < 6)
    {
        if (n < 2)
        {
            return n;
        }
        return n/2;
    }   
    val = n;
    i = 0;
    while (val > 1)
    {
        val >>= 1;
        i++;
    }
    val <<= (i >> 1);
    val = (val + val + val) >> 1;
    do
    {
      last = val;
      val = ((val + n/val) >> 1);
    }while(focal_abs(val-last) > 1);
    return val; 
}

//////////////////////////////////////////////////////////////////////////
////计算两点的距离,点1 point1,点2 point2
//////////////////////////////////////////////////////////////////////////
unsigned int DistanceToPoint(unsigned int point_x1, unsigned int point_y1,unsigned int point_x2,unsigned int point_y2)//计算点与点的距离
{
	unsigned int dDistance = 0;
	if ((point_x1 == point_x2)&&(point_y1 == point_y2))
		return 0;

	dDistance = SqrtNew(((point_y1-point_y2)*(point_y1-point_y2)) + ((point_x2 -point_x1)*(point_x2 -point_x1)));
	return dDistance;
}

static int fts_monitor_distance(void)
{

	int i = 0;
	unsigned int iDistance = 0;
	unsigned int iDeltaTime = 0;	
	
	clear_report_data();	
	for(i=0; i<MAX_TOUCH_POINTS; i++)
	{
		switch(st_monitor.last_data.event[i])
		{
			case NO_EVENT:
			case UP_EVENT:
			{	
				break;
			}
			case CONTACT_EVENT:
			case DOWN_EVENT:
			{
				if((CONTACT_EVENT == st_monitor.current_data.event[i])
					//||(NO_EVENT == st_monitor.current_data.event[i])
					)
				{
					/*计算两点之间距离*/
					iDistance = DistanceToPoint(
						st_monitor.current_data.X[i],
						st_monitor.current_data.Y[i],
						st_monitor.last_data.X[i],
						st_monitor.last_data.Y[i]
						);
					/*如果距离大于预设值，则处理掉*/
					if(iDistance > DISTANCE_BETWEEN_TWO_POINTS)
					{
						/*计算两点发生的间隔时间*/
						iDeltaTime = (st_monitor.current_data.record_time.tv_sec - st_monitor.last_data.record_time.tv_sec)*MSEC_PER_SEC 
							+ (st_monitor.current_data.record_time.tv_usec - st_monitor.last_data.record_time.tv_usec)/1000;
	
					FTS_COMMON_DBG("Abnormal distance: %d ! point_id = %d, last_point = (%d, %d), current_point = (%d, %d), delta_time = %d ms\n",
						iDistance, i,  st_monitor.last_data.X[i],  st_monitor.last_data.Y[i],
						st_monitor.current_data.X[i],  st_monitor.current_data.Y[i],
						iDeltaTime
						);	
					
					st_monitor.report_data.point_id[st_monitor.current_report_num] = i;
					st_monitor.report_data.event[st_monitor.current_report_num] = UP_EVENT;
					st_monitor.report_data.X[st_monitor.current_report_num] = st_monitor.last_data.X[i];
					st_monitor.report_data.Y[st_monitor.current_report_num] = st_monitor.last_data.Y[i];					
					st_monitor.current_report_num++;	
					FTS_COMMON_DBG("Lost Up Event by fts_monitor_distance() ! Point_id = %d, X = %d, Y = %d\n",
						i,  st_monitor.last_data.X[i],  st_monitor.last_data.Y[i]);
					}
				}					
				break;
			}
			default:
				break;
		}	
	}
	
	if(st_monitor.current_report_num > 0)
	{
		fts_report_added();
	}
	return 0;
}
