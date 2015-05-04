/*
 * include/linux/amlogic/aml_rtc.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef __AML_RTC_H__
#define __AML_RTC_H__

#define AML_PMU_TAG				0x00414d4c	/* 'AML' tag */

#ifdef CONFIG_AML_RTC
unsigned int aml_read_rtc_mem_reg(unsigned char reg_id);
unsigned int aml_get_rtc_counter(void);
int aml_write_rtc_mem_reg(unsigned char reg_id, unsigned int data);
#else
static inline unsigned int aml_read_rtc_mem_reg(unsigned char reg_id)
{
	return 0;
}
static inline unsigned int aml_get_rtc_counter(void)
{
	return 0;
}

static inline int aml_write_rtc_mem_reg(unsigned char reg_id, unsigned int data)
{
	return 0;
}
#endif

#endif

