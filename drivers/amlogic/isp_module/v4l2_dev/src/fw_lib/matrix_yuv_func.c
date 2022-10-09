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

#include "acamera_fw.h"

#include "acamera_math.h"
#include "acamera_math.h"
#include "acamera_calibrations.h"
#include "acamera_command_api.h"
#include "matrix_yuv_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_MATRIX_YUV
#endif

static void vector_vector_add( int16_t *v1, int16_t *v2, int dim1 )
{
    int i;
    for ( i = 0; i < dim1; ++i ) {
        v2[i] = v1[i] + v2[i];
    }
}
static void matrix_vector_multiply( int16_t *m, int16_t *v )
{
    int i, j;
    const int dim1 = 3;
    const int dim2 = 3;
    int16_t result[3];
    for ( i = 0; i < dim1; ++i ) {
        int32_t temp = 0;
        for ( j = 0; j < dim2; ++j )
            temp += ( ( (int32_t)m[i * dim2 + j] * v[j] )  );
        result[i] = (uint16_t)( ( temp + ( 1 << 7 ) ) >> 8 );
    }
    for ( i = 0; i < dim1; ++i )
        v[i] = ACAMERA_MAX( -1023, ACAMERA_MIN( 1023, result[i] ) );
}
static void matrix_matrix_multiply( int16_t *a1, int16_t *a2 )
{
    int i, j, k;
    const int dim1 = 3;
    const int dim2 = 3;
    const int dim3 = 3;
    int16_t result[3 * 3];

    memset( result, 0, sizeof( result ) );

    for ( i = 0; i < dim1; ++i ) {
        for ( j = 0; j < dim3; ++j ) {
            int32_t temp = 0;
            for ( k = 0; k < dim2; ++k )
                temp += ( ( (int32_t)a1[i * dim2 + k] * a2[k * dim3 + j] ) );
            result[i * dim3 + j] = (int16_t)( ( temp + ( 1 << 7 ) ) >> 8 );
        }
    }
    for ( i = 0; i < 9; i++ )
        a2[i] = result[i]; //ACAMERA_MAX(-511,ACAMERA_MIN(512,result[i]));
}

static uint16_t color_matrix_complement_to_direct_16( int16_t v )
{
    uint16_t result;

    if ( v >= 0 )
        result = v;
    else {
        result = -v;
        result |= ( 1 << 15 );
    }
    return result;
}

static void matrix_yuv_clip( int16_t *input )
{
    uint8_t i;
    int16_t val;
    for ( i = 0; i < 9; i++ ) {
        val = ACAMERA_MAX( -511, ACAMERA_MIN( 512, input[i] ) );
        input[i] = color_matrix_complement_to_direct_16( val );
    }
    for ( i = 9; i < 12; i++ ) {
        val = ACAMERA_MAX( -1023, ACAMERA_MIN( 1023, input[i] ) );
        input[i] = val;
        if ( val < 0 ) {
            val = 2048 + val;
            input[i] = val;
        }
    }
}
static void matrix_yuv_fr_coefft_write_to_hardware( matrix_yuv_fsm_t *p_fsm )
{
    // set U and V channel to 0 in black and white mode
    int i = 0;
    if ( ( p_fsm->color_mode == BLACK_AND_WHITE ) && ( p_fsm->fr_pipe_output_format != PIPE_OUT_RGB ) ) {
       for ( i = 3; i < 9; i++ ) {
           p_fsm->fr_composite_yuv_matrix[i] = 0;
       }
       p_fsm->fr_composite_yuv_matrix[9] = 0;
       p_fsm->fr_composite_yuv_matrix[10] = 512;
       p_fsm->fr_composite_yuv_matrix[11] = 512;
    }

    int16_t *yuv_matrix = p_fsm->fr_composite_yuv_matrix;
    acamera_isp_fr_cs_conv_coefft_11_write( p_fsm->cmn.isp_base, yuv_matrix[0] );
    acamera_isp_fr_cs_conv_coefft_12_write( p_fsm->cmn.isp_base, yuv_matrix[1] );
    acamera_isp_fr_cs_conv_coefft_13_write( p_fsm->cmn.isp_base, yuv_matrix[2] );
    acamera_isp_fr_cs_conv_coefft_21_write( p_fsm->cmn.isp_base, yuv_matrix[3] );
    acamera_isp_fr_cs_conv_coefft_22_write( p_fsm->cmn.isp_base, yuv_matrix[4] );
    acamera_isp_fr_cs_conv_coefft_23_write( p_fsm->cmn.isp_base, yuv_matrix[5] );
    acamera_isp_fr_cs_conv_coefft_31_write( p_fsm->cmn.isp_base, yuv_matrix[6] );
    acamera_isp_fr_cs_conv_coefft_32_write( p_fsm->cmn.isp_base, yuv_matrix[7] );
    acamera_isp_fr_cs_conv_coefft_33_write( p_fsm->cmn.isp_base, yuv_matrix[8] );
    acamera_isp_fr_cs_conv_coefft_o1_write( p_fsm->cmn.isp_base, yuv_matrix[9] );
    acamera_isp_fr_cs_conv_coefft_o2_write( p_fsm->cmn.isp_base, yuv_matrix[10] );
    acamera_isp_fr_cs_conv_coefft_o3_write( p_fsm->cmn.isp_base, yuv_matrix[11] );
    acamera_isp_fr_cs_conv_enable_matrix_write( p_fsm->cmn.isp_base, 1 );
    switch ( p_fsm->fr_pipe_output_format ) {
    case PIPE_OUT_YUV422:
        acamera_isp_fr_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_fr_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_fr_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );

        break;
    case PIPE_OUT_YUV420:
        acamera_isp_fr_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_fr_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_fr_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 1 );

        break;
    case PIPE_OUT_RGB:
        acamera_isp_fr_cs_conv_coefft_11_write( p_fsm->cmn.isp_base, yuv_matrix[6] );
        acamera_isp_fr_cs_conv_coefft_12_write( p_fsm->cmn.isp_base, yuv_matrix[7] );
        acamera_isp_fr_cs_conv_coefft_13_write( p_fsm->cmn.isp_base, yuv_matrix[8] );
        acamera_isp_fr_cs_conv_coefft_21_write( p_fsm->cmn.isp_base, yuv_matrix[3] );
        acamera_isp_fr_cs_conv_coefft_22_write( p_fsm->cmn.isp_base, yuv_matrix[4] );
        acamera_isp_fr_cs_conv_coefft_23_write( p_fsm->cmn.isp_base, yuv_matrix[5] );
        acamera_isp_fr_cs_conv_coefft_31_write( p_fsm->cmn.isp_base, yuv_matrix[0] );
        acamera_isp_fr_cs_conv_coefft_32_write( p_fsm->cmn.isp_base, yuv_matrix[1] );
        acamera_isp_fr_cs_conv_coefft_33_write( p_fsm->cmn.isp_base, yuv_matrix[2] );
        acamera_isp_fr_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_fr_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_fr_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );

        break;
    case PIPE_OUT_YUV444:
        acamera_isp_fr_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_fr_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_fr_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );

        break;
    default:
        acamera_isp_fr_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_fr_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_fr_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );
        break;
    }
}

static void matrix_yuv_ds_write_to_hardware( matrix_yuv_fsm_t *p_fsm )
{
    // set U and V channel to 0 in black and white mode
    int i = 0;
    if ( ( p_fsm->color_mode == BLACK_AND_WHITE ) && ( p_fsm->ds1_pipe_output_format != PIPE_OUT_RGB ) ) {
       for ( i = 3; i < 9; i++ ) {
           p_fsm->ds1_composite_yuv_matrix[i] = 0;
       }
       p_fsm->ds1_composite_yuv_matrix[9] = 0;
       p_fsm->ds1_composite_yuv_matrix[10] = 512;
       p_fsm->ds1_composite_yuv_matrix[11] = 512;
    }

#if ISP_HAS_DS1
    int16_t *yuv_matrix = p_fsm->ds1_composite_yuv_matrix;
    acamera_isp_ds1_cs_conv_coefft_11_write( p_fsm->cmn.isp_base, yuv_matrix[0] );
    acamera_isp_ds1_cs_conv_coefft_12_write( p_fsm->cmn.isp_base, yuv_matrix[1] );
    acamera_isp_ds1_cs_conv_coefft_13_write( p_fsm->cmn.isp_base, yuv_matrix[2] );
    acamera_isp_ds1_cs_conv_coefft_21_write( p_fsm->cmn.isp_base, yuv_matrix[3] );
    acamera_isp_ds1_cs_conv_coefft_22_write( p_fsm->cmn.isp_base, yuv_matrix[4] );
    acamera_isp_ds1_cs_conv_coefft_23_write( p_fsm->cmn.isp_base, yuv_matrix[5] );
    acamera_isp_ds1_cs_conv_coefft_31_write( p_fsm->cmn.isp_base, yuv_matrix[6] );
    acamera_isp_ds1_cs_conv_coefft_32_write( p_fsm->cmn.isp_base, yuv_matrix[7] );
    acamera_isp_ds1_cs_conv_coefft_33_write( p_fsm->cmn.isp_base, yuv_matrix[8] );
    acamera_isp_ds1_cs_conv_coefft_o1_write( p_fsm->cmn.isp_base, yuv_matrix[9] );
    acamera_isp_ds1_cs_conv_coefft_o2_write( p_fsm->cmn.isp_base, yuv_matrix[10] );
    acamera_isp_ds1_cs_conv_coefft_o3_write( p_fsm->cmn.isp_base, yuv_matrix[11] );
    acamera_isp_ds1_cs_conv_enable_matrix_write( p_fsm->cmn.isp_base, 1 );
    switch ( p_fsm->ds1_pipe_output_format ) {
    case PIPE_OUT_YUV422:
        acamera_isp_ds1_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_ds1_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_ds1_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );
        break;
    case PIPE_OUT_YUV420:
        acamera_isp_ds1_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_ds1_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 1 );
        acamera_isp_ds1_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 1 );
        break;
    case PIPE_OUT_RGB:
        acamera_isp_ds1_cs_conv_coefft_11_write( p_fsm->cmn.isp_base, yuv_matrix[6] );
        acamera_isp_ds1_cs_conv_coefft_12_write( p_fsm->cmn.isp_base, yuv_matrix[7] );
        acamera_isp_ds1_cs_conv_coefft_13_write( p_fsm->cmn.isp_base, yuv_matrix[8] );
        acamera_isp_ds1_cs_conv_coefft_21_write( p_fsm->cmn.isp_base, yuv_matrix[3] );
        acamera_isp_ds1_cs_conv_coefft_22_write( p_fsm->cmn.isp_base, yuv_matrix[4] );
        acamera_isp_ds1_cs_conv_coefft_23_write( p_fsm->cmn.isp_base, yuv_matrix[5] );
        acamera_isp_ds1_cs_conv_coefft_31_write( p_fsm->cmn.isp_base, yuv_matrix[0] );
        acamera_isp_ds1_cs_conv_coefft_32_write( p_fsm->cmn.isp_base, yuv_matrix[1] );
        acamera_isp_ds1_cs_conv_coefft_33_write( p_fsm->cmn.isp_base, yuv_matrix[2] );
        acamera_isp_ds1_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_ds1_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_ds1_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );
        break;
    case PIPE_OUT_YUV444:
        acamera_isp_ds1_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_ds1_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_ds1_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );
        break;
    default:
        acamera_isp_ds1_cs_conv_enable_filter_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_ds1_cs_conv_enable_horizontal_downsample_write( p_fsm->cmn.isp_base, 0 );
        acamera_isp_ds1_cs_conv_enable_vertical_downsample_write( p_fsm->cmn.isp_base, 0 );
        break;
    }
#endif
}

static void matrix_compute_brightness( uint8_t brightness_strength, int16_t *p_matrix )
{

    /*{
       Chainging Brigntess in the RGB domain.So apply the same shift to all the Colors.
       In Case if the PIPE output is YUV,the YUC transform matrix will apply the offset only on to the Y channel
       and for the U and V the co-eff of you YUV transform matrix is such that it will add up to zero.
        The idea here is to change Brigtness in RGB
        by changing offset for RGB
          0x100   0          0             brightness_shift
          0     0x100     0              brightness_shift
          0       0     0x100           brightness_shift
    }
    */
    int16_t brightness_shift = 0;
    uint8_t i = 0;
    if ( brightness_strength < 128 ) {
        brightness_shift = -( ( 0x3FF * ( 127 - brightness_strength ) ) / 127 );
    } else {
        brightness_shift = ( 0x3FF * ( brightness_strength - 128 ) ) / 127;
    }
    for ( i = 0; i < 12; i++ )
        p_matrix[i] = 0x00;
    for ( i = 0; i < 9; i += 4 ) {
        p_matrix[i] = 0x100;
    }
    p_matrix[9] = brightness_shift;
    p_matrix[10] = brightness_shift;
    p_matrix[11] = brightness_shift;
}
static void matrix_compute_saturation( uint8_t saturation_strength, int16_t *p_matrix, matrix_yuv_fsm_t *p_fsm )
{
    /*{
        The idea here is to change Saturation only by changing the U anv V channels.
        But we are applying on RGB
        X25 = RGB2YUV\([1 0 0; 0 2 0; 0 0 2]*RGB2YUV)
        X = (X25-eye(3))
        That is the saturation_transfrom_mat.
    }*/
    int16_t saturation_transfrom_mat[12] = {179, -149, -28, -76, 104, -29, -75, -148, 226, 0, 0, 0};
    int16_t saturation_scale = 0;
    if ( saturation_strength < 128 )
        saturation_scale = 256 * ( saturation_strength - 128 ) / 128;
    else
        saturation_scale = ( 256 * ( (uint16_t)saturation_strength - 128 ) ) / 128;
    uint8_t i = 0;
    for ( i = 0; i < 12; i++ )
        p_matrix[i] = ( saturation_transfrom_mat[i] * saturation_scale ) >> 8;
    p_matrix[0] += 0x100;
    p_matrix[4] += 0x100;
    p_matrix[8] += 0x100;
}
static void matrix_compute_contrast( uint8_t contrast_strength, int16_t *p_matrix )
{
    /*{
        The idea here is to change Contrast,by scaling  RGB space and changing offset
          contrast_scale   0             0                contrast_shift
          0             contrast_scale     0                        0
          0                0         contrast_scale                 0
    }
    */
    int16_t contrast_scale = 0;
    int16_t contrast_shift = 0;
    uint8_t i = 0;
    if ( contrast_strength <= 128 ) {
        contrast_scale = ( 256 * contrast_strength ) / 128;
        contrast_shift = 512 - ( ( 512 * contrast_strength ) / 128 );
    } else {
        contrast_scale = 256 + ( ( 256 * ( contrast_strength - 128 ) ) / 128 );
        contrast_shift = ( 512 * ( 128 - contrast_strength ) ) / 128;
    }
    for ( i = 0; i < 12; i++ )
        p_matrix[i] = 0;

    p_matrix[0] = contrast_scale;
    p_matrix[4] = contrast_scale;
    p_matrix[8] = contrast_scale;
    p_matrix[9] = contrast_shift;
    p_matrix[10] = contrast_shift;
    p_matrix[11] = contrast_shift;
}

#define MUL_16_16( a, b ) ( ( int16_t )( ( (int32_t)a * (int32_t)b ) >> 15 ) )
static int16_t get_cosine( int16_t theta, int16_t *p_hue_COS_TABLES )
{
    int16_t result = 0;
    int8_t index = 0;
    int8_t sign = 0;
    if ( theta >= 0 && theta <= 90 ) {
        index = theta;
        sign = 1;
    } else if ( theta > 90 && theta <= 180 ) {
        sign = -1;
        index = 180 - theta;
    } else if ( theta < 0 && theta >= -90 ) {
        sign = 1;
        index = -theta;
    } else if ( theta < -90 && theta >= -180 ) {
        sign = -1;
        index = 180 + theta;
    }
    result = sign * ( p_hue_COS_TABLES[index] + ( 90 - index ) );
    return result;
}
static int16_t get_sine( int16_t theta, int16_t *p_hue_COS_TABLES )
{
    int16_t result = 0;
    int8_t index = 0;
    int8_t sign = 0;
    if ( theta >= 0 && theta <= 90 ) {
        index = 90 - theta;
        sign = 1;
    } else if ( theta > 90 && theta <= 180 ) {
        sign = 1;
        index = theta - 90;
    } else if ( theta < -90 && theta >= -180 ) {
        sign = -1;
        index = -( theta + 90 );
    } else if ( theta < 0 && theta >= -90 ) {
        sign = -1;
        index = 90 + theta;
    }
    result = sign * ( p_hue_COS_TABLES[index] + ( 90 - index ) );
    return result;
}
static void matrix_compute_hue_saturation( uint16_t value, int16_t *p_matrix )
{
    /*
  The Hue Correction is applied in YIQ domain.RGB->YIQ->HUE_COORECTION->RGB
  The entire transform matrix   YIQ2RGB*Hue_Corr*RGB2YIQ(Image),is simplified into one matrix, and given by the hue coeff which is 3x3 matrix and each column has 3 coeff
  in the form  a0+b0*cosine+c0*sine.
  All the tables used here are Q14 format.Results in Q8 format.

*/
    int16_t HUE_COSTABLE[] = {
        16294, 16293, 16286, 16275, 16258, 16237, 16210, 16179, 16143, 16101, 16055, 16004, 15948, 15887, 15821, 15751,
        15675, 15595, 15510, 15420, 15326, 15227, 15123, 15015, 14902, 14784, 14662, 14535, 14404, 14269, 14129, 13985,
        13836, 13684, 13527, 13366, 13201, 13032, 12859, 12682, 12501, 12316, 12128, 11935, 11740, 11540, 11337, 11131,
        10921, 10708, 10491, 10272, 10049, 9823, 9594, 9362, 9128, 8890, 8650, 8407, 8162, 7914, 7664, 7411, 7156, 6899,
        6640, 6379, 6116, 5851, 5584, 5315, 5045, 4773, 4500, 4225, 3950, 3673, 3394, 3115, 2835, 2554, 2272, 1990, 1707,
        1423, 1139, 854, 570, 285, 0,
    };
    int16_t hue_coeff[] = {
        4899, 11485, 2748, 9617, -9617, 5395, 1868, -1868, -8144, 4899, -4899, -5376, 9617, 6767, 581, 1868, -1868, 4795,
        4899, -4899, 20473, 9617, -9617, -17143, 1868, 14516, -3329,
    };
    int16_t theta = (int16_t)value;
    int16_t cosine = 0;
    int16_t sine = 0;
    uint8_t _index = 0;
    const int16_t identity_matrix[12] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0, 0, 0};
    for ( _index = 0; _index < 12; _index++ )
        p_matrix[_index] = identity_matrix[_index];

    theta = theta - 180;
    cosine = get_cosine( theta, HUE_COSTABLE );
    sine = get_sine( theta, HUE_COSTABLE );
    for ( _index = 0; _index < 9; _index++ ) {
        p_matrix[_index] = ( hue_coeff[0 + ( _index * 3 )] >> 1 ) + MUL_16_16( hue_coeff[1 + ( _index * 3 )], cosine ) + MUL_16_16( hue_coeff[2 + ( _index * 3 )], sine );
        p_matrix[_index] = p_matrix[_index] >> 5; //(Q13->Q8)
        if (theta == 0)
           p_matrix[_index] = identity_matrix[_index];
    }
}
static void matrix_compute_color_mode( uint16_t mode, int16_t *p_color_mode_matrix, matrix_yuv_fsm_t *p_fsm )
{
    const int16_t identity_matrix[12] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0, 0, 0};
    uint8_t i;
    for ( i = 0; i < 12; i++ )
        p_color_mode_matrix[i] = identity_matrix[i];
#ifdef COLOR_MODE_ID
    uint8_t j;

    switch ( mode ) {
    case NORMAL:
        for ( i = 0; i < 3; i++ ) {
            for ( j = 0; j < 3; j++ ) {
                if ( i == j )
                    p_color_mode_matrix[3 * i + j] = 0x100;
                else
                    p_color_mode_matrix[3 * i + j] = 0;
            }
        }
        for ( i = 9; i < 12; i++ )
            p_color_mode_matrix[i] = 0;
        break;
    case BLACK_AND_WHITE:
        for ( i = 0; i < 3; i++ ) {
            for ( j = 0; j < 3; j++ ) {
                p_color_mode_matrix[3 * i + j] = p_fsm->rgb2yuv_matrix[j];
            }
        }
        for ( i = 9; i < 12; i++ )
            p_color_mode_matrix[i] = 0;
        break;
    case NEGATIVE:
        for ( i = 0; i < 3; i++ ) {
            for ( j = 0; j < 3; j++ ) {
                if ( i == j )
                    p_color_mode_matrix[3 * i + j] = -0x100;
                else
                    p_color_mode_matrix[3 * i + j] = 0;
            }
        }
        for ( i = 9; i < 12; i++ )
            p_color_mode_matrix[i] = 0x3ff;
        break;
    case SEPIA: // get values from IQ:
        p_color_mode_matrix[0] = 0x75;
        p_color_mode_matrix[1] = 0x15;
        p_color_mode_matrix[2] = 0x05;
        p_color_mode_matrix[3] = 0x20;
        p_color_mode_matrix[4] = 0x70;
        p_color_mode_matrix[5] = 0x05;
        p_color_mode_matrix[6] = 0x20;
        p_color_mode_matrix[7] = 0x20;
        p_color_mode_matrix[8] = 0x05;
        p_color_mode_matrix[9] = 0xD3;
        p_color_mode_matrix[10] = 0x0;
        p_color_mode_matrix[11] = 0x0;
        break;
    case VIVID:
        for ( i = 0; i < 3; i++ ) {
            for ( j = 0; j < 3; j++ ) {
                if ( i == j )
                    p_color_mode_matrix[3 * i + j] = 0x100;
                else
                    p_color_mode_matrix[3 * i + j] = 0;
            }
        }
        for ( i = 9; i < 12; i++ )
            p_color_mode_matrix[i] = 0;
        break;
    default:
        break;
    }
#endif
}
static void update_composite_matrix( int16_t *inp1, int16_t *res )
{
    matrix_matrix_multiply( inp1, res );
    matrix_vector_multiply( inp1, res + 9 );

    vector_vector_add( inp1 + 9, res + 9, 3 );
}
static void compute_transfrom_matrix( matrix_yuv_fsm_t *p_fsm, int16_t *final_composite_yuv_matrix, uint8_t format )
{
    uint8_t i = 0;
    uint16_t identity_matrix[12] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0, 0, 0};
    for ( i = 0; i < 12; i++ ) {
        final_composite_yuv_matrix[i] = identity_matrix[i];
    }
    update_composite_matrix( p_fsm->color_mode_matrix, final_composite_yuv_matrix );
    update_composite_matrix( p_fsm->hue_correction_matrix, final_composite_yuv_matrix );
    update_composite_matrix( p_fsm->saturation_matrix, final_composite_yuv_matrix );
    update_composite_matrix( p_fsm->contrast_matrix, final_composite_yuv_matrix );
    update_composite_matrix( p_fsm->brightness_matrix, final_composite_yuv_matrix );
    if ( format != PIPE_OUT_RGB ) {
        uint16_t *ptr_rgb2yuv = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_RGB2YUV_CONVERSION );
        for ( i = 0; i < 12; i++ )
            p_fsm->rgb2yuv_matrix[i] = color_matrix_direct_to_complement( ptr_rgb2yuv[i] );
        update_composite_matrix( p_fsm->rgb2yuv_matrix, final_composite_yuv_matrix );
    }
    matrix_yuv_clip( final_composite_yuv_matrix );
}
void matrix_yuv_recompute( matrix_yuv_fsm_t *p_fsm )
{

    matrix_compute_color_mode( p_fsm->color_mode, p_fsm->color_mode_matrix, p_fsm );
    matrix_compute_brightness( p_fsm->brightness_strength, p_fsm->brightness_matrix );
    matrix_compute_contrast( p_fsm->contrast_strength, p_fsm->contrast_matrix );
    matrix_compute_saturation( p_fsm->saturation_strength, p_fsm->saturation_matrix, p_fsm );
    matrix_compute_hue_saturation( p_fsm->hue_theta, p_fsm->hue_correction_matrix );
    compute_transfrom_matrix( p_fsm, p_fsm->fr_composite_yuv_matrix, p_fsm->fr_pipe_output_format );
#if ISP_HAS_DS1
    compute_transfrom_matrix( p_fsm, p_fsm->ds1_composite_yuv_matrix, p_fsm->ds1_pipe_output_format );
#endif
}
void matrix_yuv_coefft_write_to_hardware( matrix_yuv_fsm_t *p_fsm )
{
    matrix_yuv_fr_coefft_write_to_hardware( p_fsm );
    matrix_yuv_ds_write_to_hardware( p_fsm );
}
void matrix_yuv_update( matrix_yuv_fsm_t *p_fsm )
{
    matrix_yuv_recompute( p_fsm );
    matrix_yuv_coefft_write_to_hardware( p_fsm );
}
void matrix_yuv_initialize( matrix_yuv_fsm_t *p_fsm )
{
    uint16_t identity_matrix[12] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000, 0x0100, 0, 0, 0};
    uint8_t i = 0;
    p_fsm->saturation_strength = 128;
    p_fsm->contrast_strength = 128;
    p_fsm->brightness_strength = 128;
    p_fsm->hue_theta = 180;
#if defined( COLOR_MODE_ID )
    p_fsm->color_mode = NORMAL;
#endif
    for ( i = 0; i < 12; i++ ) {
        p_fsm->rgb2yuv_matrix[i] = color_matrix_direct_to_complement( identity_matrix[i] );
        p_fsm->brightness_matrix[i] = color_matrix_direct_to_complement( identity_matrix[i] );
        p_fsm->contrast_matrix[i] = color_matrix_direct_to_complement( identity_matrix[i] );
        p_fsm->hue_correction_matrix[i] = color_matrix_direct_to_complement( identity_matrix[i] );
        p_fsm->fr_composite_yuv_matrix[i] = color_matrix_direct_to_complement( identity_matrix[i] );
#if ISP_HAS_DS1
        p_fsm->ds1_composite_yuv_matrix[i] = color_matrix_direct_to_complement( identity_matrix[i] );
#endif
    }
}


void matrix_yuv_set_DS1_mode( const uint16_t format )
{
}
void matrix_yuv_set_FR_mode( const uint16_t format )
{
}
