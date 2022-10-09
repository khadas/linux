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

#ifndef ACAMERA_LOGGER_H
#define ACAMERA_LOGGER_H

#include "system_log.h"

//Log Level define for each system or platform
#ifdef SYSTEM_LOG_LEVEL
#define ACAMERA_LOG_LEVEL SYSTEM_LOG_LEVEL
#endif

//Log mask define for modules on each system or platform
#ifdef SYSTEM_LOG_MASK
#define ACAMERA_LOG_MASK SYSTEM_LOG_MASK
#endif

//vprintf like function should be defined for each platform
#ifndef SYSTEM_VPRINTF
#error SYSTEM_VPRINTF should be defined
#endif

//printf like function should be defined for each platform
#ifndef SYSTEM_PRINTF
#error SYSTEM_PRINTF should be defined
#endif

//snprintf like function should be defined for each platform
#ifndef SYSTEM_SNPRINTF
#error SYSTEM_SNPRINTF should be defined
#endif

//vsprintf like function should be defined for each platform
#ifndef SYSTEM_VSPRINTF
#error SYSTEM_VSPRINTF should be defined
#endif

//internal checker for acamera logger to perform special log on interupts
#define ACAMERA_LOGGER_INTERRUPT( flags ) ( flags & 1 )

//value of 1 means ISR logging is enabled otherwise disabled on the compile level
#ifdef SYSTEM_LOG_FROM_ISR
#define ACAMERA_LOG_FROM_ISR SYSTEM_LOG_FROM_ISR
#else
#define ACAMERA_LOG_FROM_ISR 0
#endif

//value of 1 means real time logging is enabled otherwise disabled on a compile level
#ifdef SYSTEM_LOG_REAL_TIME
#define ACAMERA_LOG_REAL_TIME SYSTEM_LOG_REAL_TIME
#else
#define ACAMERA_LOG_REAL_TIME 0
#endif


#if ACAMERA_LOG_REAL_TIME == 0

#if defined( ACAMERA_LOG_LEVEL )
#define _ACAMERA_LOG_OUTPUT_LEVEL ACAMERA_LOG_LEVEL
#else
#define _ACAMERA_LOG_OUTPUT_LEVEL 0 //every level
#endif
#if defined( ACAMERA_LOG_MASK )
#define _ACAMERA_LOG_OUTPUT_MASK ACAMERA_LOG_MASK
#else
#define _ACAMERA_LOG_OUTPUT_MASK 0 //none
#endif
#define ACAMERA_LOG_HAS_FIXED_MASK 1
#define ACAMERA_LOGGER_SET_LEVEL( level ) (void)0
#define ACAMERA_LOGGER_SET_MASK( mask ) (void)0
#else
extern uint32_t _acamera_output_mask;
extern uint8_t _acamera_output_level;
void acamera_logger_set_level( uint8_t level );
void acamera_logger_set_mask( uint32_t mask );
#define ACAMERA_LOG_HAS_FIXED_MASK 0
#define _ACAMERA_LOG_OUTPUT_LEVEL _acamera_output_level
#define _ACAMERA_LOG_OUTPUT_MASK _acamera_output_mask
#define ACAMERA_LOGGER_SET_LEVEL( level ) acamera_logger_set_level( level )
#define ACAMERA_LOGGER_SET_MASK( mask ) acamera_logger_set_mask( mask )
#endif

uint8_t acamera_logger_get_level( void );
uint32_t acamera_logger_get_mask( void );

#define ACAMERA_LOG_ON( level, mask ) \
    ( ( mask & _ACAMERA_LOG_OUTPUT_MASK ) && ( level >= _ACAMERA_LOG_OUTPUT_LEVEL ) )

#ifdef SYSTEM_LOG_HAS_TIME
#if defined( SYSTEM_TIME_LOG_CB ) && SYSTEM_LOG_HAS_TIME
#define ACAMERA_LOG_HAS_TIME 1
#else
#define ACAMERA_LOG_HAS_TIME 0
#endif
#else
#define ACAMERA_LOG_HAS_TIME 0
#endif

#ifdef SYSTEM_LOG_HAS_SRC
#define ACAMERA_LOG_HAS_SRC SYSTEM_LOG_HAS_SRC
#else
#define ACAMERA_LOG_HAS_SRC 0
#endif

#if ACAMERA_LOG_HAS_SRC
#if ACAMERA_LOG_FROM_ISR
void _acamera_log_write_isr( const char *const func, const char *const file, const unsigned line,
                             const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... );
#endif
void _acamera_log_write( const char *const func, const char *const file, const unsigned line,
                         const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... );

#define ACAMERA_LOG_WRITE( level, module, ... )                   \
    do {                                                          \
        if ( ACAMERA_LOG_ON( level, 1 << module ) )               \
            _acamera_log_write( __FUNCTION__, __FILE__, __LINE__, \
                                level, module, __VA_ARGS__ );     \
    } while ( 0 )
#if ACAMERA_LOG_FROM_ISR
#define ACAMERA_LOG_ISR( level, module, ... )                         \
    do {                                                              \
        if ( ACAMERA_LOG_ON( level, 1 << module ) )                   \
            _acamera_log_write_isr( __FUNCTION__, __FILE__, __LINE__, \
                                    level, module, __VA_ARGS__ );     \
    } while ( 0 )
#else
#define ACAMERA_LOG_ISR( level, module, ... ) (void)0
#endif
#else
#if ACAMERA_LOG_FROM_ISR
void _acamera_log_write_isr( const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... );
#endif
void _acamera_log_write( const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... );
#define ACAMERA_LOG_WRITE( level, module, ... )               \
    do {                                                      \
        if ( ACAMERA_LOG_ON( level, 1 << module ) )           \
            _acamera_log_write( level, module, __VA_ARGS__ ); \
    } while ( 0 )
#if ACAMERA_LOG_FROM_ISR
#define ACAMERA_LOG_ISR( level, module, ... )                     \
    do {                                                          \
        if ( ACAMERA_LOG_ON( level, 1 << module ) )               \
            _acamera_log_write_isr( level, module, __VA_ARGS__ ); \
    } while ( 0 )
#else
#define ACAMERA_LOG_ISR( level, module, ... ) (void)0
#endif

#endif

#define LOG( level, ... ) ACAMERA_LOG_WRITE( level, LOG_MODULE, __VA_ARGS__ )
#define LOG_ISR( level, ... ) ACAMERA_LOG_ISR( level, LOG_MODULE, __VA_ARGS__ )

typedef const char *( *time_log_cb_t )( void );

#define ACAMERA_BUFFER_SIZE 256
#define ACAMERA_BUFFER_TEMP 128

typedef struct acamera_logger_buf {
    char acamera_logger_buf[ACAMERA_BUFFER_SIZE];
    char temp_buf[ACAMERA_BUFFER_TEMP];
    uint16_t start;
    uint16_t end;
} acamera_logger_buf_t;

void acamera_logger_empty( void );

void acamera_logger_set_time_cb( time_log_cb_t time_log_fn );

#define LOG_MODULE 0 //always the first module, src file should undef this value

#endif // ACAMERA_LOGGER_H
