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

#include "acamera_types.h"
#include "acamera_logger.h"
#include "system_log.h"
#include "system_timer.h"
#include "acamera_sbus_api.h"

//#define JUNO_HDMI_TX_IIC_MASTER 0x68004000
#define JUNO_HDMI_TX_IIC_MASTER 0x64204000


void hdmi_write( acamera_sbus_t *pbus, unsigned char raddr, unsigned char dout )
{
    acamera_sbus_write_u8( pbus, raddr, dout );
    system_timer_usleep( 200 );
}


void hdmi_init_cec( acamera_sbus_t *pbus )
{
    unsigned char addr, data;

    addr = 0xFF;
    data = 0x87;
    hdmi_write( pbus, addr, data );
    addr = 0xFF;
    data = 0x87;
    hdmi_write( pbus, addr, data );
}

void hdmi_init_hdmi( acamera_sbus_t *pbus )
{
    unsigned char addr, data;

    addr = 0xFF;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xF0;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xA0;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xA1;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xA2;
    data = 0xE5;
    hdmi_write( pbus, addr, data );

    addr = 0xA3;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xA4;
    data = 0x19;
    hdmi_write( pbus, addr, data );

    addr = 0xA5;
    data = 0x03;
    hdmi_write( pbus, addr, data );

    addr = 0xA6;
    data = 0xFE;
    hdmi_write( pbus, addr, data );

    addr = 0xA7;
    data = 0x70;
    hdmi_write( pbus, addr, data );

    addr = 0xA8;
    data = 0x70;
    hdmi_write( pbus, addr, data );

    addr = 0xA9;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xAA;
    data = 0x01;
    hdmi_write( pbus, addr, data );

    addr = 0xAB;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xAC;
    data = 0x18;

    hdmi_write( pbus, addr, data );
    addr = 0xAD;
    data = 0x00;

    hdmi_write( pbus, addr, data );

    addr = 0xAE;
    data = 0x03;
    hdmi_write( pbus, addr, data );

    addr = 0xAF;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xB0;
    data = 0x18;
    hdmi_write( pbus, addr, data );

    addr = 0xB9;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xBA;
    data = 0x18;
    hdmi_write( pbus, addr, data );

    addr = 0xBD;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xBE;
    data = 0x18;
    hdmi_write( pbus, addr, data );

    addr = 0xBF;
    data = 0x02;

    hdmi_write( pbus, addr, data );

    addr = 0xC0;
    data = 0x70;
    hdmi_write( pbus, addr, data );

    addr = 0xC5;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xC6;
    data = 0xE0;
    hdmi_write( pbus, addr, data );

    addr = 0xC7;
    data = 0x04;
    hdmi_write( pbus, addr, data );

    addr = 0xC8;
    data = 0x00;
    hdmi_write( pbus, addr, data );

    addr = 0xCB;
    data = 0x7C;
    hdmi_write( pbus, addr, data );

    addr = 0x20;
    data = 0x23;
    hdmi_write( pbus, addr, data );

    addr = 0x21;
    data = 0x45;
    hdmi_write( pbus, addr, data );

    addr = 0x22;
    data = 0x01;
    hdmi_write( pbus, addr, data );

    addr = 0x23;
    data = 0x20;
    hdmi_write( pbus, addr, data );

    addr = 0xE5;
    data = 0x10;
    hdmi_write( pbus, addr, data );
}


int32_t hdmi_init( void )
{
    int32_t result = 0;
    acamera_sbus_t *pbus = NULL;
    struct _acamera_sbus_t sbus;

    pbus = &sbus;

    sbus.bus = JUNO_HDMI_TX_IIC_MASTER;
    sbus.device = 0x68 / 2;
    sbus.mask = SBUS_MASK_SAMPLE_8BITS | SBUS_MASK_ADDR_8BITS;

    LOG( LOG_INFO, "HDMI init start" );
    acamera_sbus_init( pbus, sbus_i2c );
    hdmi_init_cec( pbus );

    sbus.bus = JUNO_HDMI_TX_IIC_MASTER;
    sbus.device = 0xE0 / 2;
    sbus.mask = SBUS_MASK_SAMPLE_8BITS | SBUS_MASK_ADDR_8BITS;

    acamera_sbus_init( pbus, sbus_i2c );
    hdmi_init_hdmi( pbus );

    LOG( LOG_INFO, "HDMI init end" );

    return result;
}
