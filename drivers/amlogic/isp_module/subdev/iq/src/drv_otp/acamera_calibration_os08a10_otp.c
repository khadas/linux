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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera_logger.h"
#include "acamera_sbus_api.h"
#include "system_hw_io.h"
#include "system_sw_io.h"
#include "acamera_get_calibration_otp.h"

#define ACAMERA_OTP_TOTAL_SIZE 16318
#define ACAMERA_OTP_SIZE 10483
#define ACAMERA_OTP_DUMP 1
#define ACAMERA_OTP_LSC_DIM 32
#define ACAMERA_OTP_LSC_SIZE (ACAMERA_OTP_LSC_DIM * ACAMERA_OTP_LSC_DIM)
#define ACAMERA_ARM_LSC_DIM 32
#define ACAMERA_LSC_SIZE (ACAMERA_ARM_LSC_DIM * ACAMERA_ARM_LSC_DIM)
#define ACAMERA_OTP_WB_START 16
#define ACAMERA_OTP_WB_SIZE  6
#define ACAMERA_OTP_LSC_3000_R_START 6208
#define ACAMERA_OTP_LSC_3000_G_START 7232
#define ACAMERA_OTP_LSC_3000_B_START 8256
#define ACAMERA_OTP_LSC_4000_R_START 3120
#define ACAMERA_OTP_LSC_4000_G_START 4144
#define ACAMERA_OTP_LSC_4000_B_START 5168
#define ACAMERA_OTP_LSC_5000_R_START 32
#define ACAMERA_OTP_LSC_5000_G_START 1056
#define ACAMERA_OTP_LSC_5000_B_START 2080
#define ACAMERA_LRC_LEFT_START 10262
#define ACAMERA_LRC_RIGHT_START 10332
#define ACAMERA_LRC_SIZE 70
#define ACAMERA_SENSOR_LEFT_LRC_START 0x7520
#define ACAMERA_SENSOR_RIGHT_LRC_START 0x7568
#define ACAMERA_OTP_BASE_CHECKSUM_ADDR 0x0001
#define ACAMERA_OTP_WB_CHECKSUM_ADDR 0x001C
#define ACAMERA_OTP_LSC_5000_CHECKSUM_ADDR 0x0C20
#define ACAMERA_OTP_LSC_4000_CHECKSUM_ADDR 0x1830
#define ACAMERA_OTP_LSC_3000_CHECKSUM_ADDR 0x2440
#define ACAMERA_OTP_PAGE_START  0x0000
#define ACAMERA_OTP_PAGE_SELECT 0x0A02


#define ACAMERA_MAX_INT16  (65535)

#define IMX290_ID        0
#define OS08A10_ID       1

#if IMX290_ID
#define SENSOR_CHIP_ID 0xB201
#endif

#if OS08A10_ID
#define SENSOR_CHIP_ID 0x530841
#endif

static uint8_t otp_memory[ACAMERA_OTP_TOTAL_SIZE];
extern uint32_t bus_addr[];
extern struct v4l2_subdev soc_iq;

static uint16_t sensor_get_id( acamera_sbus_t *p_sbus )
{
    uint32_t sensor_id = 0;

#if IMX290_ID
    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x301e) << 8;
    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x301f);
#endif

#if OS08A10_ID
    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x300a) << 16;
    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x300b) << 8;
    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x300c);
#endif

    if (sensor_id != SENSOR_CHIP_ID) {
        LOG(LOG_ERR, "%s: Failed to read sensor id\n", __func__);
        return 0xFF;
    }

    LOG(LOG_CRIT, "read sensor id success\n", __func__);

    return 0;
}

/**
 *   Read OTP Memory from sensor with a given address range
 *
 *   @param p_sbus      - initialised sbus instance to read the data
 *   @param otp_start   - start reading offset inside otp memory
 *   @param otp_size    - how many bytes to read
 *   @param buffer      - pre allocated buffer to store otp data
 *   @param buffer_size - size of the buffer
 *
 *   @return 0 on success, otherwise return -1
 */
static int32_t sensor_otp_read_memory( acamera_sbus_t *p_sbus, uint32_t otp_start, uint32_t otp_size, uint8_t *buffer, uint32_t buffer_size ) {
    int32_t result = 0;
	acamera_sbus_t o_sbus;
    if ( p_sbus != NULL ) {
        if ( (otp_start + otp_size - 1) < ACAMERA_OTP_TOTAL_SIZE ) {
            if ( buffer != NULL && buffer_size >= otp_size ) {
                uint32_t start = otp_start;
                uint32_t read_bytes = 0 ;
                uint32_t offset = 0 ;
                uint8_t *temp_ptr;
                uint8_t check_otp_flag;
                temp_ptr = buffer;
                o_sbus = *p_sbus;
                o_sbus.device = 0;

                /*
                 * check_otp_flag   - 0: no eeprom or otp data forbidden
                                    - 1: have eeprom and otp data enable
                */
                check_otp_flag = acamera_sbus_read_u8( &o_sbus, 0 );
                LOG(LOG_CRIT, "check_flag = %d", check_otp_flag);
                if ( check_otp_flag != 2) {
                   LOG(LOG_CRIT, "--no eeprom or otp data forbidden--");
                   return -1;
                }

                while ( read_bytes < otp_size ) {
                    for ( offset = start ; offset < otp_size; offset ++ ) {
                        uint8_t data = acamera_sbus_read_u8( &o_sbus, ACAMERA_OTP_PAGE_START + offset ) ;
                        //LOG(LOG_ERR, "offset: %d, data: %d\n", offset, data);
                        *buffer++ = data ;

                        read_bytes ++ ;
                        if ( read_bytes >= otp_size ) {
                            break ;
                        }
                    start = 0 ;
                  }
                }
                //int i;
                //for (i=0; i<18; i++) {
                //LOG(LOG_ERR, "OTP[%d] = %x", i, *temp_ptr);
                //temp_ptr ++;
                //}

            } else {
                LOG(LOG_ERR, "Wrong buffer 0x%x or not enough size. Requested %d bytes but available %d", otp_size, buffer_size);
                result = -1 ;
            }
        } else {
            LOG(LOG_ERR, "Wrong data offset. Maximum supported %d bytes. Requested to read %d bytes from %d offset", ACAMERA_OTP_TOTAL_SIZE, otp_size, otp_start);
            result = -1 ;
        }
    } else {
        LOG(LOG_CRIT, "I2C bus is not initialized. ");
        result = -1;
    }
    return result;
}

static int32_t checksum_func( uint8_t* p_otp, uint32_t start_addr, uint32_t offset) {
    uint32_t idx = start_addr;
    uint32_t checksum = 0;
    uint32_t range = start_addr + offset;

    for ( ;idx < range ;idx++ ) {
        uint32_t data = p_otp[idx];
        checksum += data;
    }

    // only lower two bytes are counted
    checksum = checksum % 255;
    return checksum;
}

/**
 *   Validate OTP buffer by checksum
 *
 *   The function will read first "size" bytes from p_otp buf and calculate the checksum
 *   It compares the calculated checksum with one from the parameter list to validate the buffer
 *
 *   @param p_otp    - otp buffer previously read from the sensor
 *   @param size     - size of the otp buffer
 *   @param checksum - golden checksum
 *
 *   @return 0 on success, otherwise return -1
 */
static int32_t sensor_otp_validate( uint8_t* p_otp, uint32_t size, uint16_t checksum_target) {
    // do check sum verification here
    int32_t  result = 0;

    uint8_t id  = p_otp[1] ;
    uint8_t Lens_type = p_otp[2] ;
    uint8_t year = p_otp[3] ;
    uint8_t month = p_otp[4] ;
    uint8_t day = p_otp[5] ;

    LOG(LOG_CRIT, "OTP INFO: ID %d, Lens Type %d, Built on %d.%d.20%d", id, Lens_type, day, month, year) ;
    if ( checksum_target ) {
        result = 0 ;
        LOG(LOG_CRIT, "OTP MEMORY CHECKSUM - OK" ) ;
    } else {
        result = -1;
        LOG(LOG_CRIT, "OTP MEMORY CHECKSUM - FAILED. " ) ;
    }
    return result;
}

/**
 *   compensate PD
 *
 *   The function receives 16x16 table as input and do interpolation to get 31x31 output table
 *   All memories should be pre allocated in advance before calling the function.
 *
 *   @param otp_buf    - buffer with original shading values from otp memory.
 *   @param otp_length - size of input shading buffer. Only ACAMERA_OTP_LSC_SIZE is supported
 *   @param lsc_buf    - buffer for the output lens shading table
 *   @param lsc_length - size of the output shading table. Only ACAMRA_ARM_LSC_SIZE is supported
 *
 *   @return 0 on success, otherwise return -1
 */


static int32_t acamera_lrc_compensation(  acamera_sbus_t *p_sbus, uint8_t* otp_buf, uint32_t otp_lrc_addr_start, uint32_t lrc_addr_start, uint32_t lrc_size ) {

        int32_t result = 0 ;
        int32_t lrc_data = 0;

        if ( otp_buf != NULL ) {
            int i;
             for ( i=0; i<lrc_size; i++) {
                 lrc_data = otp_buf[otp_lrc_addr_start];

                 LOG(LOG_ERR, "otp_lrc_addr: %d  lrc_addr: 0x%x  lrc_data: %d\n", otp_lrc_addr_start, lrc_addr_start, lrc_data);

                 acamera_sbus_write_u8( p_sbus, lrc_addr_start, lrc_data );
                 otp_lrc_addr_start++;
                 lrc_addr_start++;
             }
        } else {
            result = -1 ;
            LOG(LOG_CRIT, "otp_buf is null.");
        }
        return result;
}

/**
 *   Calculate lens shading correction table
 *
 *   The function receives 16x16 table as input and do interpolation to get 31x31 output table
 *   All memories should be pre allocated in advance before calling the function.
 *
 *   @param otp_buf    - buffer with original shading values from otp memory.
 *   @param otp_length - size of input shading buffer. Only ACAMERA_OTP_LSC_SIZE is supported
 *   @param lsc_buf    - buffer for the output lens shading table
 *   @param lsc_length - size of the output shading table. Only ACAMRA_ARM_LSC_SIZE is supported
 *
 *   @return 0 on success, otherwise return -1
 */
static int32_t acamera_lsc_transform( uint8_t* otp_buf, uint32_t otp_length, uint8_t* lsc_buf, uint32_t lsc_length ) {
        int32_t result = 0 ;
        if ( otp_buf != NULL && otp_length == ACAMERA_OTP_LSC_SIZE ) {
            if ( lsc_buf != NULL && lsc_length >= ACAMERA_LSC_SIZE ) {
                uint32_t i, j;
                uint8_t(*otp_lsc)[ACAMERA_OTP_LSC_DIM] = (uint8_t(*)[ACAMERA_OTP_LSC_DIM]) otp_buf;
                uint8_t(*arm_lsc)[ACAMERA_ARM_LSC_DIM] = (uint8_t(*)[ACAMERA_ARM_LSC_DIM]) lsc_buf;

                // copy otp samples to the output mesh
                for (i = 0; i < ACAMERA_OTP_LSC_DIM; i++) {
                    for (j = 0; j < ACAMERA_OTP_LSC_DIM; j++) {
                        arm_lsc[i][j] = otp_lsc[i][j];
                    }
                }
            } else {
                result = -1 ;
                LOG(LOG_CRIT, "Wrong output shading buffer 0x%x. Given %d bytes but expected %d bytes", lsc_buf, lsc_length, ACAMERA_LSC_SIZE);
            }
        } else {
            result = -1 ;
            LOG(LOG_CRIT, "Wrong input shading buffer 0x%x. Given %d bytes but expected %d bytes", otp_buf, otp_length, ACAMERA_OTP_LSC_SIZE);
        }

        return result ;
}

static int32_t acamera_lsc_component_update( uint8_t* p_otp, uint32_t otp_start, uint32_t otp_size, uint8_t* lsc_ptr, uint32_t lsc_length) {
    int32_t result = 0 ;
    if ( p_otp != NULL && otp_size != 0 && lsc_ptr != NULL && lsc_length != 0 ) {
        result = acamera_lsc_transform( &p_otp[otp_start], otp_size, lsc_ptr, lsc_length ) ;
    } else {
        result = -1;
        LOG(LOG_ERR, "FAILED to process OTP LSC table. One or more input parameters are invalid. p_otp 0x%x, start %d, size %d, lsc_ptr 0x%x, lsc_size %d", p_otp, otp_start, otp_size, lsc_ptr, lsc_length) ;
    }
    return result ;
}


/**
 *   Calculate AWB static gains
 *
 *   The function receives the pointer to 6 byte table as input and calculate output awb gains
 *   All memories should be pre allocated in advance before calling the function.
 *
 *   @param p_otp     - buffer with original awb gains from otp memory
 *   @param awb_start - start offset of awb values
 *   @param wb_ptr    - buffer for output static awb gains
 *   @param wb_lenght - output awb buffer size
 *
 *   @return 0 on success, otherwise return -1
 */
static int32_t acamera_wb_component_update( uint8_t* p_otp, uint32_t awb_start, uint32_t awb_size, uint16_t* wb_ptr, uint32_t wb_length) {
    int32_t result = 0 ;
    if ( p_otp != NULL && awb_size != 0 && wb_ptr != NULL && wb_length != 0 ) {
        // update wb coefficients here from otp memory
        uint16_t otp_r = ((uint16_t)p_otp[awb_start+1]) << 8 | (p_otp[awb_start+0]);
        uint16_t otp_g = ((uint16_t)p_otp[awb_start+3]) << 8 | (p_otp[awb_start+2]);
        uint16_t otp_b = ((uint16_t)p_otp[awb_start+5]) << 8 | (p_otp[awb_start+4]);


        uint32_t rg = (otp_g*256)/otp_r;
        uint32_t bg = (otp_g*256)/otp_b;
        rg = rg > ACAMERA_MAX_INT16 ? ACAMERA_MAX_INT16 : rg ;
        bg = bg > ACAMERA_MAX_INT16 ? ACAMERA_MAX_INT16 : bg ;

        wb_ptr[0] = rg;                 // R/G in 8.8 format
        wb_ptr[1] = 256;                // 256 in 8.8 format
        wb_ptr[2] = 256;                // 256 in 8.8 format
        wb_ptr[3] = bg;                 // B/G in 8.8 format

        LOG(LOG_ERR, "OWB DEBUG: input otp 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", p_otp[awb_start+0], p_otp[awb_start+1],p_otp[awb_start+2],p_otp[awb_start+3],p_otp[awb_start+4],p_otp[awb_start+5]);
        LOG(LOG_ERR, "OWB DEBUG: full r g b  0x%x 0x%x 0x%x ", otp_r, otp_g, otp_b);
        LOG(LOG_ERR, "OWB DEBUG: output WB  0x%x 0x%x 0x%x 0x%x ", wb_ptr[0], wb_ptr[1], wb_ptr[2], wb_ptr[3]);
    } else {
        result = -1;
        LOG(LOG_ERR, "Failed to process awb otp table. One or more input parameters are invalid") ;
    }
    return result ;
}


/**
 *   Update ARM calibrations by Sensor OTP Memory data
 *
 *   The function reads sensor otp memory and update ARM calibration LUTs
 *
 *
 *   @param c - ARM Calibration array
 *
 *   @return 0 on success, otherwise return -1
 */
int32_t acamera_calibration_os08a10_otp( ACameraCalibrations *c ) {
    int32_t result = 0 ;
    uint16_t sensor_id = 0xff;

    acamera_sbus_t sbus;
    sbus.mask = SBUS_MASK_ADDR_16BITS |
           SBUS_MASK_SAMPLE_8BITS |SBUS_MASK_ADDR_SWAP_BYTES;
    sbus.control = 0;
    sbus.bus = 0;
    sbus.device = 0x6c;
    acamera_sbus_init(&sbus, sbus_i2c);

    // read all data from otp memory.
    sensor_id = sensor_get_id(&sbus);
    if (sensor_id != 0)
    {
        acamera_sbus_deinit(&sbus,  sbus_i2c);
        return -1;
    }

    result = sensor_otp_read_memory( &sbus, 0, ACAMERA_OTP_SIZE, otp_memory, sizeof(otp_memory) );

    uint32_t base_addr = 0x0002;
    uint32_t base_offest = 9;
    uint32_t wb_offest = 12;
    uint32_t lsc_offest = 3072;

    uint32_t base_checksum = checksum_func(otp_memory, base_addr, base_offest);
    uint32_t wb_checksum = checksum_func(otp_memory, ACAMERA_OTP_WB_START, wb_offest);
    uint32_t lsc_5000_checksum = checksum_func(otp_memory, ACAMERA_OTP_LSC_5000_R_START, lsc_offest);
    uint32_t lsc_4000_checksum = checksum_func(otp_memory, ACAMERA_OTP_LSC_4000_R_START, lsc_offest);
    uint32_t lsc_3000_checksum = checksum_func(otp_memory, ACAMERA_OTP_LSC_3000_R_START, lsc_offest);

    if ( result == 0 ) {
        uint16_t checksum = 0;
        // validate only data before high byte of the checksum.
        if ( base_checksum == otp_memory[ACAMERA_OTP_BASE_CHECKSUM_ADDR] &&
            wb_checksum == otp_memory[ACAMERA_OTP_WB_CHECKSUM_ADDR] &&
            lsc_5000_checksum == otp_memory[ACAMERA_OTP_LSC_5000_CHECKSUM_ADDR]&&
            lsc_4000_checksum == otp_memory[ACAMERA_OTP_LSC_4000_CHECKSUM_ADDR]&&
            lsc_3000_checksum == otp_memory[ACAMERA_OTP_LSC_3000_CHECKSUM_ADDR]) {
            checksum = 1;
        } else {
          checksum = 0;
        }
        result = sensor_otp_validate(otp_memory, ACAMERA_OTP_LSC_3000_CHECKSUM_ADDR, checksum);

        if ( result == 0 ) {
            // read static white balance
            result |= acamera_wb_component_update( otp_memory, ACAMERA_OTP_WB_START, ACAMERA_OTP_WB_SIZE, c->calibrations[CALIBRATION_STATIC_WB]->ptr, c->calibrations[CALIBRATION_STATIC_WB]->cols*sizeof(uint16_t)) ;

            // read LSC table for 3000k
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_3000_R_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_A_R]->ptr, c->calibrations[CALIBRATION_SHADING_LS_A_R]->cols) ;
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_3000_G_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_A_G]->ptr, c->calibrations[CALIBRATION_SHADING_LS_A_G]->cols) ;
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_3000_B_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_A_B]->ptr, c->calibrations[CALIBRATION_SHADING_LS_A_B]->cols) ;
            // read LSC table for 4000k
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_4000_R_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_TL84_R]->ptr, c->calibrations[CALIBRATION_SHADING_LS_TL84_R]->cols) ;
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_4000_G_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_TL84_G]->ptr, c->calibrations[CALIBRATION_SHADING_LS_TL84_G]->cols) ;
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_4000_B_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_TL84_B]->ptr, c->calibrations[CALIBRATION_SHADING_LS_TL84_B]->cols) ;
            // read LSC table for 5000k
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_5000_R_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_D65_R]->ptr, c->calibrations[CALIBRATION_SHADING_LS_D65_R]->cols) ;
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_5000_G_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_D65_G]->ptr, c->calibrations[CALIBRATION_SHADING_LS_D65_G]->cols) ;
            result |= acamera_lsc_component_update( otp_memory, ACAMERA_OTP_LSC_5000_B_START, ACAMERA_OTP_LSC_SIZE, c->calibrations[CALIBRATION_SHADING_LS_D65_B]->ptr, c->calibrations[CALIBRATION_SHADING_LS_D65_B]->cols) ;


            //LOG(LOG_ERR, "---- left lrc ----\n");

            acamera_lrc_compensation(&sbus, otp_memory, ACAMERA_LRC_LEFT_START, ACAMERA_SENSOR_LEFT_LRC_START, ACAMERA_LRC_SIZE );

            //LOG(LOG_ERR, "---- right lrc ----\n");
            acamera_lrc_compensation(&sbus, otp_memory, ACAMERA_LRC_RIGHT_START, ACAMERA_SENSOR_RIGHT_LRC_START, ACAMERA_LRC_SIZE );

            if ( result == 0 ) {
                LOG(LOG_CRIT, "Lens shading and AWB aprameters have been updated ");
            } else {
                result = -1 ;
                LOG(LOG_CRIT, "Failed to read calibraition data from the sensor otp memory") ;
            }
        } else {
            result = -1;
            LOG(LOG_CRIT, "FAILED to validate otp memory. OTP Memory will be skipped");
        }
    } else {
        LOG(LOG_CRIT, "FAILED to read otp memory. OTP Memory will be skipped");
    }

	acamera_sbus_deinit(&sbus,  sbus_i2c);

    return result ;
}