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
#include "acamera_get_calibration_otp.h"

#define ACAMERA_OTP_PAGE_NUM 37  // 40 pages in total but 3 not used
#define ACAMERA_OTP_PAGE_SIZE 64
#define ACAMERA_OTP_TOTAL_SIZE (ACAMERA_OTP_PAGE_NUM*ACAMERA_OTP_PAGE_SIZE)
#define ACAMERA_OTP_DUMP 0
#define ACAMERA_OTP_LSC_DIM 16
#define ACAMERA_OTP_LSC_SIZE (ACAMERA_OTP_LSC_DIM * ACAMERA_OTP_LSC_DIM)
#define ACAMERA_ARM_LSC_DIM 31
#define ACAMERA_LSC_SIZE (ACAMERA_ARM_LSC_DIM * ACAMERA_ARM_LSC_DIM)
#define ACAMERA_OTP_WB_START 12
#define ACAMERA_OTP_WB_SIZE  6
#define ACAMERA_OTP_LSC_3000_R_START 18
#define ACAMERA_OTP_LSC_3000_G_START 274
#define ACAMERA_OTP_LSC_3000_B_START 530
#define ACAMERA_OTP_LSC_4000_R_START 786
#define ACAMERA_OTP_LSC_4000_G_START 1042
#define ACAMERA_OTP_LSC_4000_B_START 1298
#define ACAMERA_OTP_LSC_5000_R_START 1554
#define ACAMERA_OTP_LSC_5000_G_START 1810
#define ACAMERA_OTP_LSC_5000_B_START 2066
#define ACAMERA_OTP_CHECKSUM_HIGH_ADDR 2350
#define ACAMERA_OTP_CHECKSUM_LOW_ADDR  2351
#define ACAMERA_OTP_PAGE_START  0x0A04
#define ACAMERA_OTP_PAGE_SELECT 0x0A02
#define ACAMERA_OTP_READ_ENABLE 0x0A00


#define ACAMERA_MAX_INT16  (65535)
#define SENSOR_CHIP_ID 0x0227

static uint8_t otp_memory[ACAMERA_OTP_TOTAL_SIZE];
extern uint32_t bus_addr[];
extern struct v4l2_subdev soc_iq;

static uint16_t sensor_get_id( acamera_sbus_t *p_sbus )
{
    uint32_t sensor_id = 0;

    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x0016) << 8;
    sensor_id |= acamera_sbus_read_u8(p_sbus, 0x0017);

    if (sensor_id != SENSOR_CHIP_ID) {
        LOG(LOG_ERR, "%s: Failed to read sensor id\n", __func__);
        return 0xFF;
    }
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
    uint16_t sensor_id = 0xff;

    if ( p_sbus != NULL ) {
        if ( (otp_start + otp_size - 1) < ACAMERA_OTP_TOTAL_SIZE ) {
            if ( buffer != NULL && buffer_size >= otp_size ) {
                uint32_t page = (uint32_t) (otp_start / ACAMERA_OTP_PAGE_SIZE);
                uint32_t start = otp_start % ACAMERA_OTP_PAGE_SIZE ;
                uint32_t read_bytes = 0 ;
                uint32_t offset = 0 ;
                while ( read_bytes < otp_size ) {
                    sensor_id = sensor_get_id(p_sbus);
                    if (sensor_id != 0) {
                        return -1;
                    }
                    acamera_sbus_write_u8( p_sbus, ACAMERA_OTP_PAGE_SELECT, page );
                    acamera_sbus_write_u8( p_sbus, ACAMERA_OTP_READ_ENABLE, 1 );
                    // read data by pages
                    for ( offset = start ; offset < ACAMERA_OTP_PAGE_SIZE; offset ++ ) {
                        uint8_t data = acamera_sbus_read_u8( p_sbus, ACAMERA_OTP_PAGE_START + offset ) ;
                        //LOG(LOG_ERR, "page : %d, offset: %d, data: %d\n", page, offset, data);
                        *buffer++ = data ;

                        read_bytes ++ ;
                        if ( read_bytes >= otp_size ) {
                            break ;
                        }
                    }
                    // go to the next page. it may happen in 2 cases
                    // 1 - when the whole packet is read
                    // 2 - after break inside the loop
                    page++;
                    start = 0 ;
                }
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
    uint32_t idx = 0;
    uint32_t checksum = 0;

    uint8_t id  = p_otp[0] ;
    uint8_t day = p_otp[1] ;
    uint8_t ym  = p_otp[2] ;


    for ( idx = 0 ; idx < size ; idx ++ ) {
        uint32_t data = p_otp[idx];
        checksum += data;
#if ACAMERA_OTP_DUMP
        LOG(LOG_DEBUG, " otp[%d] = 0x%x", idx, data);
#endif
    }

    // only lower two bytes are counted
    checksum &= 0xFFFF ;


    LOG(LOG_INFO, "OTP INFO: ID %d, Built on %d.%d", id, day, ym & 31) ;
    if ( checksum == checksum_target ) {
        result = 0 ;
        LOG(LOG_INFO, "OTP MEMORY CHECKSUM - OK" ) ;
    } else {
        result = -1;
        LOG(LOG_CRIT, "OTP MEMORY CHECKSUM - FAILED. " ) ;
        LOG(LOG_CRIT, "Expected 0x%x, calculated 0x%x", checksum_target, checksum ) ;
    }
    return result;
}



static uint8_t clip8(int32_t val) {
    uint8_t result = 0;

    val = (val < 0) ? 0 : val;
    val = (val > 255) ? 255 : val;

    result = (uint8_t)val;

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
    // we assume that OTP shading table is 16x16 and ARM Shading is 31x31
    int32_t result = 0 ;
    if ( otp_buf != NULL && otp_length == ACAMERA_OTP_LSC_SIZE ) {
        if ( lsc_buf != NULL && lsc_length >= ACAMERA_LSC_SIZE ) {
            uint32_t tmp;
            uint32_t i, j;
            uint8_t(*otp_lsc)[ACAMERA_OTP_LSC_DIM] = (uint8_t(*)[ACAMERA_OTP_LSC_DIM]) otp_buf;
            uint8_t(*arm_lsc)[ACAMERA_ARM_LSC_DIM] = (uint8_t(*)[ACAMERA_ARM_LSC_DIM]) lsc_buf;

            // copy otp samples to the output mesh
            for (i = 0; i < ACAMERA_OTP_LSC_DIM; i++) {
                for (j = 0; j < ACAMERA_OTP_LSC_DIM; j++) {
                    arm_lsc[i * 2][j * 2] = otp_lsc[i][j];
                }
            }

            for (j = 3; j <= ACAMERA_ARM_LSC_DIM - 4; j += 2) {
                //Use convolution(bicubic kernel) to fill most values
                for (i = 0; i < ACAMERA_ARM_LSC_DIM; i += 2) {
                    //Vertical interpolation, kernel = [-3 0 27 0 27 0 - 3]'/48
                    tmp = -3 * (arm_lsc[j - 3][i] + arm_lsc[j + 3][i]);
                    tmp += 27 * (arm_lsc[j - 1][i] + arm_lsc[j + 1][i]);
                    tmp += 24; //round
                    tmp /= 48;
                    arm_lsc[j][i] = clip8(tmp);

                    //Horizontal interpolation, kernel = [-3 0 27 0 27 0 - 3] / 48
                    tmp = -3 * (arm_lsc[i][j - 3] + arm_lsc[i][j + 3]);
                    tmp += 27 * (arm_lsc[i][j - 1] + arm_lsc[i][j + 1]);
                    tmp += 24; //round
                    tmp /= 48;

                    arm_lsc[i][j] = clip8(tmp);
                }

                // Diagonal interpolation, kernel = [-3 0 27 0 27 0 - 3]'*[-3 0 27 0 27 0 -3]/2304
                for (i = 3; i <= ACAMERA_ARM_LSC_DIM-4; i += 2) {
                    tmp = 9 * (arm_lsc[j - 3][i - 3] + arm_lsc[j + 3][i - 3] + arm_lsc[j - 3][i + 3] + arm_lsc[j + 3][i + 3]);
                    tmp += 729 * (arm_lsc[j - 1][i - 1] + arm_lsc[j + 1][i - 1] + arm_lsc[j - 1][i + 1] + arm_lsc[j + 1][i + 1]);
                    tmp -= 81 * (arm_lsc[j - 1][i - 3] + arm_lsc[j + 1][i - 3] + arm_lsc[j - 1][i + 3] + arm_lsc[j + 1][i + 3]);
                    tmp -= 81 * (arm_lsc[j - 3][i - 1] + arm_lsc[j + 3][i - 1] + arm_lsc[j - 3][i + 1] + arm_lsc[j + 3][i + 1]);
                    tmp += 1152; //round
                    tmp /= 2304;

                    arm_lsc[j][i] = clip8(tmp);
                }
            }

            // Use convolution(bilinear kernel) to fill values "1 away" from edges
            for (j = 1; j < ACAMERA_ARM_LSC_DIM; j += 28) {

                for (i = 0; i < ACAMERA_ARM_LSC_DIM; i += 2) {
                    //Vertical interpolation, kernel = [1 0 1]'/2
                    tmp = arm_lsc[j - 1][i] + arm_lsc[j + 1][i];
                    tmp += 1; //round
                    tmp /= 2;
                    arm_lsc[j][i] = clip8(tmp);

                    //Horizontal interpolation, kernel = [1 0 1] / 2
                    tmp = arm_lsc[i][j - 1] + arm_lsc[i][j + 1];
                    tmp += 1; //round
                    tmp /= 2;
                    arm_lsc[i][j] = clip8(tmp);
                }


                for (i = 1; i < ACAMERA_ARM_LSC_DIM; i += 2) {
                    //Diagonal interpolation, kernel = [1 0 1]'*[1 0 1]/4
                    tmp = arm_lsc[j - 1][i - 1] + arm_lsc[j - 1][i + 1] + arm_lsc[j + 1][i - 1] + arm_lsc[j + 1][i + 1];
                    tmp += 2; //round
                    tmp /= 4;
                    arm_lsc[j][i] = clip8(tmp);

                    tmp = arm_lsc[i - 1][j - 1] + arm_lsc[i - 1][j + 1] + arm_lsc[i + 1][j - 1] + arm_lsc[i + 1][j + 1];
                    tmp += 2; // round
                    tmp /= 4;
                    arm_lsc[i][j] = clip8(tmp);
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
        uint16_t otp_r = ((uint16_t)p_otp[awb_start+0] << 2) | (p_otp[awb_start+1] & 3);
        uint16_t otp_g = ((uint16_t)p_otp[awb_start+2] << 2) | (p_otp[awb_start+3] & 3);
        uint16_t otp_b = ((uint16_t)p_otp[awb_start+4] << 2) | (p_otp[awb_start+5] & 3);


        uint32_t rg = (otp_g*256)/otp_r;
        uint32_t bg = (otp_g*256)/otp_b;
        rg = rg > ACAMERA_MAX_INT16 ? ACAMERA_MAX_INT16 : rg ;
        bg = bg > ACAMERA_MAX_INT16 ? ACAMERA_MAX_INT16 : bg ;

        // apply the offset of D50 from factory (495, 440) to ARM tuning (524, 472)
        // Due to b/135211113, the offset is needed.

        wb_ptr[0] = rg*524/495;         // R/G in 8.8 format
        wb_ptr[1] = 256;                // 256 in 8.8 format
        wb_ptr[2] = 256;                // 256 in 8.8 format
        wb_ptr[3] = bg*472/440;         // B/G in 8.8 format

        LOG(LOG_DEBUG, "OWB DEBUG: input otp 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", p_otp[awb_start+0], p_otp[awb_start+1],p_otp[awb_start+2],p_otp[awb_start+3],p_otp[awb_start+4],p_otp[awb_start+5]);
        LOG(LOG_DEBUG, "OWB DEBUG: full r g b  0x%x 0x%x 0x%x ", otp_r, otp_g, otp_b);
        LOG(LOG_DEBUG, "OWB DEBUG: output WB  0x%x 0x%x 0x%x 0x%x ", wb_ptr[0], wb_ptr[1], wb_ptr[2], wb_ptr[3]);
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
int32_t acamera_calibration_imx227_otp( ACameraCalibrations *c ) {
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
    if (sensor_id != 0) {
        acamera_sbus_deinit(&sbus,  sbus_i2c);
        return -1;
    }

    result = sensor_otp_read_memory( &sbus, 0, ACAMERA_OTP_TOTAL_SIZE, otp_memory, sizeof(otp_memory) );
    if ( result == 0 ) {
        uint16_t checksum = (otp_memory[ACAMERA_OTP_CHECKSUM_HIGH_ADDR]<<8) | (otp_memory[ACAMERA_OTP_CHECKSUM_LOW_ADDR]);
        // validate only data before high byte of the checksum.
        result = sensor_otp_validate(otp_memory, ACAMERA_OTP_CHECKSUM_HIGH_ADDR, checksum);
        if ( result == 0 ) {
            LOG(LOG_INFO,"OTP Memory validation: OK") ;
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

            if ( result == 0 ) {
                LOG(LOG_INFO, "OTP MEMORY PARSING - SUCCESS");
                LOG(LOG_INFO, "Lens shading and AWB aprameters have been updated ");
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
