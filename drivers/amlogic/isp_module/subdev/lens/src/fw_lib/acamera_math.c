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
#include "acamera_math.h"

static uint8_t leading_one_position( const uint32_t in )
{
    uint8_t pos = 0;
    uint32_t val = in;
    if ( val >= 1 << 16 ) {
        val >>= 16;
        pos += 16;
    }
    if ( val >= 1 << 8 ) {
        val >>= 8;
        pos += 8;
    }
    if ( val >= 1 << 4 ) {
        val >>= 4;
        pos += 4;
    }
    if ( val >= 1 << 2 ) {
        val >>= 2;
        pos += 2;
    }
    if ( val >= 1 << 1 ) {
        pos += 1;
    }
    return pos;
}

static int leading_one_position_64( uint64_t val )
{
    int pos = 0;
    if ( val >= (uint64_t)1 << 32 ) {
        val >>= 32;
        pos += 32;
    }
    if ( val >= 1 << 16 ) {
        val >>= 16;
        pos += 16;
    }
    if ( val >= 1 << 8 ) {
        val >>= 8;
        pos += 8;
    }
    if ( val >= 1 << 4 ) {
        val >>= 4;
        pos += 4;
    }
    if ( val >= 1 << 2 ) {
        val >>= 2;
        pos += 2;
    }
    if ( val >= 1 << 1 ) {
        pos += 1;
    }
    return pos;
}
//  y = log2(x)
//
//    input:  Integer: val
//    output: Fixed point x.y
//  y: out_precision
//
uint32_t acamera_log2_int_to_fixed( const uint32_t val, const uint8_t out_precision, const uint8_t shift_out )
{
    int i;
    int pos = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t in = val;
    uint32_t result = 0;
    const unsigned char precision = out_precision;

    if ( 0 == val ) {
        return 0;
    }
    // integral part
    pos = leading_one_position( val );
    // fractional part
    a = ( pos <= 15 ) ? ( in << ( 15 - pos ) ) : ( in >> ( pos - 15 ) );
    for ( i = 0; i < precision; ++i ) {
        b = a * a;
        if ( b & ( (uint32_t)1 << 31 ) ) {
            result = ( result << 1 ) + 1;
            a = b >> 16;
        } else {
            result = ( result << 1 );
            a = b >> 15;
        }
    }
    return ( ( ( pos << precision ) + result ) << shift_out ) | ( ( a & 0x7fff ) >> ( 15 - shift_out ) );
}
static uint32_t log2_int_to_fixed_64( uint64_t val, uint8_t out_precision, uint8_t shift_out )
{
    int i;
    int pos = 0;
    uint64_t a = 0;
    uint64_t b = 0;
    uint64_t in = val;
    uint64_t result = 0;
    const unsigned char precision = out_precision;

    if ( 0 == val ) {
        return 0;
    }
    // integral part
    pos = leading_one_position_64( val );
    // fractional part
    a = ( pos <= 15 ) ? ( in << ( 15 - pos ) ) : ( in >> ( pos - 15 ) );
    for ( i = 0; i < precision; ++i ) {
        b = a * a;
        if ( b & ( (uint32_t)1 << 31 ) ) {
            result = ( result << 1 ) + 1;
            a = b >> 16;
        } else {
            result = ( result << 1 );
            a = b >> 15;
        }
    }
    return ( uint32_t )( ( ( ( pos << precision ) + result ) << shift_out ) | ( ( a & 0x7fff ) >> ( 15 - shift_out ) ) );
}
//  y = log2(x)
//
//    input:  Fixed point: x1.y1
//    output: Fixed point: x2.y2
//  y1: in_fraction
//  y2: out_precision
//
uint32_t acamera_log2_fixed_to_fixed( const uint32_t val, const int in_fix_point, const uint8_t out_fix_point )
{
    return acamera_log2_int_to_fixed( val, out_fix_point, 0 ) - ( in_fix_point << out_fix_point );
}

int32_t acamera_log2_fixed_to_fixed_64( uint64_t val, int32_t in_fix_point, uint8_t out_fix_point )
{
    return log2_int_to_fixed_64( val, out_fix_point, 0 ) - ( in_fix_point << out_fix_point );
}

const unsigned int _pow2_lut[33] = {
    1073741824, 1097253708, 1121280436, 1145833280, 1170923762, 1196563654, 1222764986, 1249540052,
    1276901417, 1304861917, 1333434672, 1362633090, 1392470869, 1422962010, 1454120821, 1485961921,
    1518500250, 1551751076, 1585730000, 1620452965, 1655936265, 1692196547, 1729250827, 1767116489,
    1805811301, 1845353420, 1885761398, 1927054196, 1969251188, 2012372174, 2056437387, 2101467502,
    2147483648U};

uint32_t acamera_math_exp2( uint32_t val, const unsigned char shift_in, const unsigned char shift_out )
{
    unsigned int fract_part = ( val & ( ( 1 << shift_in ) - 1 ) );
    unsigned int int_part = val >> shift_in;
    if ( shift_in <= 5 ) {
        unsigned int lut_index = fract_part << ( 5 - shift_in );
        return _pow2_lut[lut_index] >> ( 30 - shift_out - int_part );
    } else {
        unsigned int lut_index = fract_part >> ( shift_in - 5 );
        unsigned int lut_fract = fract_part & ( ( 1 << ( shift_in - 5 ) ) - 1 );
        unsigned int a = _pow2_lut[lut_index];
        unsigned int b = _pow2_lut[lut_index + 1];
        unsigned int res = ( (unsigned long long)( b - a ) * lut_fract ) >> ( shift_in - 5 );
        res = ( res + a ) >> ( 30 - shift_out - int_part );

        return res;
    }
}

uint32_t acamera_sqrt64( uint64_t arg )
{
    uint64_t mask = (uint64_t)1 << 31;
    uint32_t res = 0;
    int i = 0;

    for ( i = 0; i < 32; i++ ) {
        if ( ( res + ( mask >> i ) ) * ( res + ( mask >> i ) ) <= arg )
            res = res + ( mask >> i );
    }
    return res;
}

uint16_t acamera_sqrt32( uint32_t arg )
{
    uint32_t mask = (uint32_t)1 << 15;
    uint16_t res = 0;
    int i = 0;

    for ( i = 0; i < 16; i++ ) {
        if ( ( res + ( mask >> i ) ) * ( res + ( mask >> i ) ) <= arg )
            res = res + ( mask >> i );
    }
    return res;
}

uint8_t acamera_sqrt16( uint16_t arg )
{
    uint8_t mask = 128;
    uint8_t res = 0;
    uint8_t i = 0;

    for ( i = 0; i < 8; i++ ) {
        if ( ( res + ( mask >> i ) ) * ( res + ( mask >> i ) ) <= arg ) {
            res = res + ( mask >> i );
        }
    }
    return res;
}
//  y = log2(x)
//
//    input:  Integer: val
//    output: Fixed point 4.4
//
uint8_t acamera_log16( uint16_t arg )
{
    uint8_t k = 0;
    uint8_t res = 0;

    for ( k = 0; k < 16; k++ ) {
        if ( arg > ( 1 << k ) ) {
            res = ( k << 4 ) + ( ( arg << 4 ) / ( 1 << ( k ) ) ) - 16; // division by zero is checked
        }
    }
    return res;
}
//     y = a * b = x.x1 (output fraction size same with "a" fraction size)
//    a: fixed x.x1
//    b: fixed x.x2
//
//  x1: a fraction size
//  x2: b fraction size
//
uint32_t acamera_multiplication_fixed_to_fixed( uint32_t a, uint32_t b, const int x1, const int x2 )
{
    return ( uint32_t )( ( (uint64_t)a * (uint64_t)b ) >> x2 );
}

// the function approximates log2(x)
// acamera_math_log2 = int(log(val)/log(2) * (1<<(out_precision+shift_out)))
// out_precision bits are correct, the shift_out bits are the approximation residual
// but each out_precision bit requires extra iteration

uint32_t acamera_math_log2(
    const uint32_t val,
    const uint8_t out_precision,
    const uint8_t shift_out )
{
    int i;
    int pos = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t in = val;
    uint32_t result = 0;
    const unsigned char precision = out_precision;

    if ( 0 == val ) {
        return 0;
    }

    // integral part
    pos = leading_one_position( val );

    // fractional part
    a = ( pos <= 15 ) ? ( in << ( 15 - pos ) ) : ( in >> ( pos - 15 ) );
    for ( i = 0; i < precision; ++i ) {
        b = a * a;
        if ( b & ( (uint32_t)1 << 31 ) ) {
            result = ( result << 1 ) + 1;
            a = b >> 16;
        } else {
            result = ( result << 1 );
            a = b >> 15;
        }
    }

    return ( ( ( pos << precision ) + result ) << shift_out ) |
           ( ( a & 0x7fff ) >> ( 15 - shift_out ) );
}
//  Linear equation solving
//
//     y1 = a * x1 + b
//     y2 = a * x2 + b
//     y1 - (a * x1) = y2 - (a * x2)
//     y1 - y2 = (a * x1) - (a * x2)
//
//     a = (y1 - y2) / (x1 - x2)
//  b = y1 - (a * x1)
//
//    Result is coefficient "a" in fixed format   x.a_fraction_size
//  y1, y2, x1, x2 - real value format
//
int32_t acamera_solving_lin_equation_a( int32_t y1, int32_t y2, int32_t x1, int32_t x2, int16_t a_fraction_size )
{
    if ( x1 == x2 ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return x1;
    }
    return ( ( y1 - y2 ) * ( 1 << a_fraction_size ) ) / ( x1 - x2 ); // division by zero is checked
}
//    Result is coefficient "b" in fixed format   x.a_fraction_size
//  y1, x1 - real value format
//  "a" in fixed format   x.a_fraction_size
//
int32_t acamera_solving_lin_equation_b( int32_t y1, int32_t a, int32_t x1, int16_t a_fraction_size )
{
    return ( y1 * ( 1 << a_fraction_size ) ) - ( a * x1 );
}
//     y = a / b
//    a: fixed xxx.fraction_size
//    b: fixed xxx.fraction_size
//
uint32_t acamera_div_fixed( uint32_t a, uint32_t b, const int16_t fraction_size )
{
    if ( b == 0 ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return a << fraction_size;
    }
    uint64_t c = ( (uint64_t)a << fraction_size );
    return ( uint32_t )( div64_u64( c, b ) ); // division by zero is checked
}
//    nth root finding y = x^0.45
//  not a precise equation - for speed issue
//    Result is coefficient "y" in fixed format   xxx.fraction_size
int32_t acamera_solving_nth_root_045( int32_t x, const int16_t fraction_size )
{
    int32_t y = ( 1 << fraction_size ) + ( ( x << fraction_size ) >> 2 ) - ( ( x << fraction_size ) >> 4 );
    return y;
}

uint16_t acamera_line_offset( uint16_t line_len, uint8_t bytes_per_pixel )
{
    const uint32_t alignment = 128; // line offset has to be aligned at 128 bytes
    return ( bytes_per_pixel * line_len + alignment - 1 ) & ~( alignment - 1 );
}

uint16_t acamera_calc_modulation_u16( uint16_t x, const modulation_entry_t *p_table, int table_len )
{
    if ( x <= p_table->x ) {
        return p_table->y;
    }
    if ( x >= p_table[table_len - 1].x ) {
        return p_table[table_len - 1].y;
    }
    {
        int i;
        for ( i = 1; i < table_len; ++i ) {
            if ( x < p_table[i].x ) {
                break;
            }
        }
        if ( ( p_table[i].x - p_table[i - 1].x ) != 0 ) {
            int alpha = ( x - p_table[i - 1].x ) * 256 / ( p_table[i].x - p_table[i - 1].x ); // division by zero is checked
            return ( p_table[i].y * alpha + p_table[i - 1].y * ( 256 - alpha ) ) >> 8;
        } else {
            LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
            return p_table[i].y;
        }
    }
}

uint32_t acamera_calc_modulation_u32( uint32_t x, const modulation_entry_32_t *p_table, int table_len )
{
    if ( x <= p_table->x ) {
        return p_table->y;
    }
    if ( x >= p_table[table_len - 1].x ) {
        return p_table[table_len - 1].y;
    }
    {
        uint16_t i;
        for ( i = 1; i < table_len; ++i ) {
            if ( x < p_table[i].x ) {
                break;
            }
        }
        if ( ( p_table[i].x - p_table[i - 1].x ) != 0 ) {
            // 32bit choosen to prevent overflows
            uint32_t alpha = ( x - p_table[i - 1].x ) * 256 / ( p_table[i].x - p_table[i - 1].x ); // division by zero is checked
            return ( p_table[i].y * alpha + p_table[i - 1].y * ( 256 - alpha ) ) >> 8;
        } else {
            LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
            return p_table[i].y;
        }
    }
}

uint16_t acamera_calc_scaled_modulation_u16( uint16_t x, uint16_t target_min_y, uint16_t target_max_y, const modulation_entry_t *p_table, int table_len )
{
    if ( x <= p_table[0].x ) {
        return target_min_y;
    }
    if ( x >= p_table[table_len - 1].x ) {
        return target_max_y;
    }
    if ( ( p_table[0].y == 0 ) || ( p_table[table_len - 1].y == 0 ) || ( ( p_table[table_len - 1].x - p_table[0].x ) == 0 ) ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return 0;
    } else {
        uint32_t scale_min_y = (uint32_t)target_min_y * 256 / p_table[0].y;                   // division by zero is checked
        uint32_t scale_max_y = (uint32_t)target_max_y * 256 / p_table[table_len - 1].y;       // division by zero is checked
        int alpha = ( x - p_table[0].x ) * 256 / ( p_table[table_len - 1].x - p_table[0].x ); // division by zero is checked
        uint32_t scale_factor = ( scale_max_y * alpha + scale_min_y * ( 256 - alpha ) ) >> 8;
        int i;
        for ( i = 1; i < table_len; ++i ) {
            if ( x < p_table[i].x ) {
                break;
            }
        }
        if ( ( p_table[i].x - p_table[i - 1].x ) != 0 ) {
            alpha = ( x - p_table[i - 1].x ) * 256 / ( p_table[i].x - p_table[i - 1].x ); // division by zero is checked
            return scale_factor * ( p_table[i].y * alpha + p_table[i - 1].y * ( 256 - alpha ) ) >> 16;
        } else {
            LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
            return p_table[i].y;
        }
    }
}

uint16_t acamera_calc_equidistant_modulation_u16( uint16_t x, const uint16_t *p_table, uint16_t table_len )
{
    if ( x == 0 ) {
        return p_table[0];
    }
    if ( x == ( ( 1 << 16 ) - 1 ) ) {
        return p_table[table_len - 1];
    }

    if ( table_len == 1 ) {
        return p_table[0];
    }

    uint16_t d = ( 1 << 16 ) / ( table_len - 1 ); // division by zero is checked
    if ( d == 0 ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return p_table[0];
    }
    uint16_t bn = x / d;                                                      // division by zero is checked
    uint16_t alpha = ( x - ( bn * d ) ) * 256 / d;                            // division by zero is checked
    return ( p_table[bn] * ( 256 - alpha ) + p_table[bn + 1] * alpha ) / 256; // division by zero is checked
}

uint32_t acamera_calc_equidistant_modulation_u32( uint32_t x, const uint32_t *p_table, uint32_t table_len )
{
    if ( x == 0 ) {
        return p_table[0];
    }
    if ( x == ( ( 1 << 16 ) - 1 ) ) {
        return p_table[table_len - 1];
    }

    if ( table_len == 1 ) {
        return p_table[0];
    }

    uint32_t d = ( 1 << 16 ) / ( table_len - 1 ); // division by zero is checked
    if ( d == 0 ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return p_table[0];
    }
    uint32_t bn = x / d;                                                      // division by zero is checked
    uint32_t alpha = ( x - ( bn * d ) ) * 256 / d;                            // division by zero is checked
    return ( p_table[bn] * ( 256 - alpha ) + p_table[bn + 1] * alpha ) / 256; // division by zero is checked
}

uint16_t acamera_calc_inv_equidistant_modulation_u16( uint16_t x, const uint16_t *p_table, uint16_t table_len )
{
    if ( x <= p_table[0] ) {
        return 0;
    }
    if ( x >= p_table[table_len - 1] ) {
        return ( ( 1 << 16 ) - 1 );
    }
    if ( table_len == 1 ) {
        return 0;
    }

    int i;
    for ( i = 1; i < table_len; ++i ) {
        if ( x < p_table[i] ) {
            break;
        }
    }


    uint16_t d = ( 1 << 16 ) / ( table_len - 1 ); // division by zero is checked

    if ( p_table[i] == p_table[i - 1] ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return d * i;
    }

    uint16_t alpha = ( x - p_table[i - 1] ) * 256 / ( p_table[i] - p_table[i - 1] ); // division by zero is checked
    return ( d * i * alpha + d * ( i - 1 ) * ( 256 - alpha ) ) / 256;                // division by zero is checked
}

uint32_t acamera_calc_inv_equidistant_modulation_u32( uint32_t x, const uint32_t *p_table, uint32_t table_len )
{
    if ( x <= p_table[0] ) {
        return 0;
    }
    if ( x >= p_table[table_len - 1] ) {
        return ( ( 1 << 16 ) - 1 );
    }
    if ( table_len == 1 ) {
        return 0;
    }

    int i;
    for ( i = 1; i < table_len; ++i ) {
        if ( x < p_table[i] ) {
            break;
        }
    }


    uint32_t d = ( 1 << 16 ) / ( table_len - 1 ); // division by zero is checked

    if ( p_table[i] == p_table[i - 1] ) {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        return d * i;
    }

    uint32_t alpha = ( x - p_table[i - 1] ) * 256 / ( p_table[i] - p_table[i - 1] ); // division by zero is checked
    return ( d * i * alpha + d * ( i - 1 ) * ( 256 - alpha ) ) / 256;                // division by zero is checked
}
