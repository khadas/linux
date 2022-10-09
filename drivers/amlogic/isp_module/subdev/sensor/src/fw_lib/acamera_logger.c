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

#include "acamera_logger.h"

#if ACAMERA_LOG_HAS_FIXED_MASK == 0

#if defined( ACAMERA_LOG_LEVEL )
uint8_t _acamera_output_level = ACAMERA_LOG_LEVEL;
#else
uint8_t _acamera_output_level = 0;
#endif

#if defined( ACAMERA_LOG_MASK )
uint32_t _acamera_output_mask = ACAMERA_LOG_MASK;
#else
uint8_t _acamera_output_level = 0;
#endif

void acamera_logger_set_level( uint8_t level )
{
    _acamera_output_level = level;
}
void acamera_logger_set_mask( uint32_t mask )
{
    _acamera_output_mask = mask;
}

#endif

uint8_t acamera_logger_get_level()
{
    return _ACAMERA_LOG_OUTPUT_LEVEL;
}
uint32_t acamera_logger_get_mask()
{
    return _ACAMERA_LOG_OUTPUT_MASK;
}

static acamera_logger_buf_t _acamera_logger_buf = {.start = 0, .end = 0};

static inline char *_logger_buf_start( acamera_logger_buf_t *log_buf_ptr )
{
    return log_buf_ptr->acamera_logger_buf + log_buf_ptr->start;
}

static inline char *_logger_buf_temp( acamera_logger_buf_t *log_buf_ptr )
{
    return log_buf_ptr->temp_buf;
}

static inline char *_logger_buf_end( acamera_logger_buf_t *log_buf_ptr )
{
    return log_buf_ptr->acamera_logger_buf + log_buf_ptr->end;
}

static inline int32_t _logger_buf_remaining( acamera_logger_buf_t *log_buf_ptr )
{
    return ACAMERA_BUFFER_SIZE - log_buf_ptr->end;
}

static inline void _logger_buf_move_end( acamera_logger_buf_t *log_buf_ptr, uint32_t size )
{
    log_buf_ptr->end += size;
}

static inline void _logger_buf_move_start( acamera_logger_buf_t *log_buf_ptr, uint32_t size )
{
    log_buf_ptr->start += size;
}

static inline void _logger_buf_reset( acamera_logger_buf_t *log_buf_ptr )
{

    log_buf_ptr->start = 0;
    log_buf_ptr->end = 0;
}

static inline int32_t _logger_buf_size( acamera_logger_buf_t *log_buf_ptr )
{
    return log_buf_ptr->end - log_buf_ptr->start;
}


void acamera_logger_empty( void )
{ //print pending logs
    uint32_t size;
    while ( _logger_buf_size( &_acamera_logger_buf ) > 0 ) {
        size = SYSTEM_PRINTF( "%s", _logger_buf_start( &_acamera_logger_buf ) );
        _logger_buf_move_start( &_acamera_logger_buf, size + 1 );
    }
}

#if ACAMERA_LOG_HAS_SRC

static const char *filename_short( const char *filename )
{
    const char *short_name = filename;
    while ( *filename != 0 ) {
        if ( *filename == '/' || *filename == '\\' )
            short_name = filename + 1;
        filename++;
    }
    return short_name;
}

void _acamera_log_write_ext( const char *const func, const char *const file, const unsigned line,
                             const uint32_t log_level, const uint32_t log_module, const uint8_t flags, const char *const fmt, va_list vaa )

{

#if ACAMERA_LOG_HAS_TIME
    const char *timestamp = SYSTEM_TIME_LOG_CB();
#else
    const char *timestamp = "";
#endif
    uint32_t size = SYSTEM_SNPRINTF( _logger_buf_temp( &_acamera_logger_buf ), ACAMERA_BUFFER_TEMP, "%s%s@%s:%d %s(%s) :", timestamp, func, filename_short( file ), line, log_module_name[log_module], log_level_name[log_level] );
#else

void _acamera_log_write_ext( const uint32_t log_level, const uint32_t log_module, const uint8_t flags, const char *const fmt, va_list vaa )

{

#if ACAMERA_LOG_HAS_TIME
    const char *timestamp = SYSTEM_TIME_LOG_CB();
#else
    const char *timestamp = "";
#endif

    uint32_t size = SYSTEM_SNPRINTF( _logger_buf_temp( &_acamera_logger_buf ), ACAMERA_BUFFER_TEMP, "%s%s(%s) :", timestamp, log_module_name[log_module], log_level_name[log_level] );
#endif
    if ( _logger_buf_remaining( &_acamera_logger_buf ) < size )
        return;
    system_memcpy( _logger_buf_end( &_acamera_logger_buf ), _logger_buf_temp( &_acamera_logger_buf ), size );
    *( _logger_buf_end( &_acamera_logger_buf ) + size ) = 0;
    _logger_buf_move_end( &_acamera_logger_buf, size + 1 );

#if ACAMERA_LOG_FROM_ISR
    if ( ACAMERA_LOGGER_INTERRUPT( flags ) ) {
#if defined( SYSTEM_PRINTF_ISR ) && defined( SYSTEM_VPRINTF_ISR )
        while ( _logger_buf_size( &_acamera_logger_buf ) > 0 ) {
            size = SYSTEM_PRINTF_ISR( "%s", _logger_buf_start( &_acamera_logger_buf ) );
            //printf("size:%d\n",(int)size);
            _logger_buf_move_start( &_acamera_logger_buf, size + 1 );
        }

        SYSTEM_VPRINTF_ISR( fmt, vaa );

        _logger_buf_reset( &_acamera_logger_buf );
#else

        size = SYSTEM_VSPRINTF( _logger_buf_end( &_acamera_logger_buf ), fmt, vaa );

        *( _logger_buf_end( &_acamera_logger_buf ) + size ) = '\n';
        *( _logger_buf_end( &_acamera_logger_buf ) + size + 1 ) = 0;
        _logger_buf_move_end( &_acamera_logger_buf, size + 2 );
#endif
    } else
#endif
    {

        while ( _logger_buf_size( &_acamera_logger_buf ) > 0 ) {
            size = SYSTEM_PRINTF( "%s", _logger_buf_start( &_acamera_logger_buf ) );

            _logger_buf_move_start( &_acamera_logger_buf, size + 1 );
        }

        SYSTEM_VPRINTF( fmt, vaa );
        SYSTEM_PRINTF( "\n" );

        _logger_buf_reset( &_acamera_logger_buf );
    }
}

#if ACAMERA_LOG_HAS_SRC

void _acamera_log_write( const char *const func, const char *const file, const unsigned line,
                         const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... )
{
    va_list va;
    va_start( va, fmt );
    _acamera_log_write_ext( func, file, line, log_level, log_module, 0, fmt, va );
    va_end( va );
}
void _acamera_log_write_isr( const char *const func, const char *const file, const unsigned line,
                             const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... )
{
    va_list va;
    va_start( va, fmt );
    _acamera_log_write_ext( func, file, line, log_level, log_module, 1, fmt, va );
    va_end( va );
}


#else
void _acamera_log_write( const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... )
{
    va_list va;
    va_start( va, fmt );
    _acamera_log_write_ext( log_level, log_module, 0, fmt, va );
    va_end( va );
}

void _acamera_log_write_isr( const uint32_t log_level, const uint32_t log_module, const char *const fmt, ... )
{
    va_list va;
    va_start( va, fmt );
    _acamera_log_write_ext( log_level, log_module, 1, fmt, va );
    va_end( va );
}
#endif
