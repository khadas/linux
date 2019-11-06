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

#if !defined( __ACAMERA_LENS_API_H__ )
#define __ACAMERA_LENS_API_H__

#include "acamera_types.h"

//type assignment for lens identified for parameter lens_type
enum {
    LENS_VCM_DRIVER_UNKNOWN,  //cannot find lens driver
    LENS_VCM_DRIVER_DONGWOON, //lens type for dongwoon
    LENS_VCM_DRIVER_DW9800,   //lens type for dw9800
    LENS_VCM_DRIVER_AD5821,   //lens type for AD5821
    LENS_VCM_DRIVER_ROHM,     //lens type for rhom
    LENS_VCM_DRIVER_LC898201, //lens type for LC898201
    LENS_VCM_DRIVER_FP5510A,  //lens type for FP5510A
    LENS_VCM_DRIVER_BU64748,  //lens type for BU64748
    LENS_VCM_DRIVER_AN41908A, //lens type for AN41908A
    LENS_VCM_DRIVER_MODEL,    //lens type for C++ Model
    LENS_VCM_DRIVER_NULL      //null lens driver
};


//useful information and state of the lens
typedef struct _lens_param_t {
    uint16_t lens_type; //lens type which assigns one of the enum type after probing
    uint16_t min_step;  //lens step resolution
    uint16_t next_zoom; //next assigned zoom if zoom if available
    uint16_t curr_zoom; //current zoom positon if zoom if available
    uint16_t next_pos;  //lens position

} lens_param_t;


//lens API implementation
typedef struct _lens_control_t {

    /**
     *   Move lens to the desired postion
     *
     *   @param position - value of which will be assigned to parameter next_pos
     *          ctx - pointer to the lens internal context or data
     *
     */
    void ( *move )( void *ctx, uint16_t position );


    /**
     *   Stop lens move
     *
     *   @param ctx - pointer to the lens internal context or data
     *
     */
    void ( *stop )( void *ctx );


    /**
     *   Api to check if lens is moving
     *
     *   @param ctx - pointer to the lens internal context or data
     *
     *   @return true if lens is moving
     */
    uint8_t ( *is_moving )( void *ctx );


    /**
     *   Get curent position of lens
     *
     *   @param ctx - pointer to the lens internal context or data
     *
     *   @return lens position
     */
    uint16_t ( *get_pos )( void *ctx );


    /**
     *   Write to lens register
     *
     *   @param ctx - pointer to the lens internal context or data
     *      address - address of register
     *         data - data to write to register location
     *
     */
    void ( *write_lens_register )( void *ctx, uint32_t address, uint32_t data );


    /**
     *   Read on lens register
     *
     *   @param ctx - pointer to the lens internal context or data
     *      address - address of register
     *
     *   @return the register value
     */
    uint32_t ( *read_lens_register )( void *ctx, uint32_t address );


    /**
     *   Get lens parameters
     *
     *   This function returns a pointer to a lens parameter structure
     *
     *   @param ctx - pointer to the lens internal context or data
     */
    const lens_param_t *( *get_parameters )( void *ctx );


    /**
     *   Move zoom to the next zoom
     *
     *   @param next_zoom - value of which will be assigned to parameter next_zoom
     *          ctx - pointer to the lens internal context or data
     *
     */
    void ( *move_zoom )( void *ctx, uint16_t next_zoom );


    /**
     *   Api to check if lens is zooming
     *
     *   @param ctx - pointer to the lens internal context or data
     *
     *   @return true if zooming
     */
    uint8_t ( *is_zooming )( void *ctx );

} lens_control_t;


#endif /* __ACAMERA_LENS_API_H__ */
