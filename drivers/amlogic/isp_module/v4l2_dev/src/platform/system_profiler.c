/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "acamera_firmware_config.h"


#if defined( ACAMERA_ISP_PROFILING ) && ( ACAMERA_ISP_PROFILING == 1 )
#include "system_profiler.h"
#include "acamera_profiler.h"
//#include "system_headers.h"
#include <linux/ktime.h>
//=============Controls===========================================================

//================================================================================
uint64_t time_diff( struct timespec start, struct timespec end )
{
    struct timespec temp;
    if ( ( end.tv_nsec - start.tv_nsec ) < 0 ) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return (uint64_t)temp.tv_sec * 1000000000 + temp.tv_nsec;
}
//=============Profiler functions (calling order:  profiler.scxml)================

//-------------Platform depended functions----------------------------------------
int32_t cpu_get_freq( void )
{
    return 1000000000;
}

static struct timespec start_clk;
static struct timespec start_isr_clk;

void cpu_start_clocks( void )
{
    start_clk = current_kernel_time();
}

void cpu_start_isr_clocks( void )
{
    start_isr_clk = current_kernel_time();
}

uint64_t cpu_stop_clocks( void )
{
    struct timespec end_clk;
    end_clk = current_kernel_time();
    return time_diff( start_clk, end_clk );
}

uint64_t cpu_stop_isr_clocks( void )
{
    struct timespec end_clk;
    end_clk = current_kernel_time();
    return time_diff( start_isr_clk, end_clk );
}

void cpu_init_profiler( void )
{
}
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//================================================================================
#endif //ACAMERA_ISP_PROFILING
