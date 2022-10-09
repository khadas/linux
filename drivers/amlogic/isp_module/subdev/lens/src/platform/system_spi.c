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

#include <linux/string.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/uio_driver.h>
#include <asm/io.h>

#include "system_spi.h"
#include "acamera_logger.h"
#include "system_timer.h"

#define PCIE_SPI_BAR_SIZE ( 1 << 19 )
#define PCIE_SPI_BAR ( 2 )
#define SPI_BASE 0x200

#define PCIE_BAR3_EXT_CFG_OFFSET ( 0x40000 )
#define PCIE_BAR3_EXT_CFG_SENSOR_RESET ( 0xC3 )

// AXI Quad SPI v3.2 LogiCore IP register set
#define FPGA_SPI_BASE_ADDR 0x64200000
#define FPGA_SPI_MASTER 0x0000018E
#define FPGA_SPI_FIFO_RST 0x00000060
#define SPI_SS_DEFAULT 0xFFFFFFFF

#define SPI_CR_LOOPBACK_MASK 0x00000001
#define SPI_CR_ENABLE_MASK 0x00000002
#define SPI_CR_MASTER_MODE_MASK 0x00000004
#define SPI_CR_CLK_POLARITY_MASK 0x00000008
#define SPI_CR_CLK_PHASE_MASK 0x00000010
#define SPI_CR_TXFIFO_RESET_MASK 0x00000020
#define SPI_CR_RXFIFO_RESET_MASK 0x00000040
#define SPI_CR_MANUAL_SS_MASK 0x00000080
#define SPI_CR_TRANS_INHIBIT_MASK 0x00000100
#define SPI_CR_LSB_FIRST_MASK 0x00000200

#define SPI_SR_RX_EMPTY_MASK 0x00000001
#define SPI_SR_RX_FULL_MASK 0x00000002
#define SPI_SR_TX_EMPTY_MASK 0x00000004
#define SPI_SR_TX_FULL_MASK 0x00000008
#define SPI_SR_MODE_FAULT_MASK 0x00000010
#define SPI_SR_SLAVE_MODE_MASK 0x00000020

// Core Registers in Legacy and Enhanced Mode (XIP mode disable)
#define FPGA_SPI_SRR 0x40
#define FPGA_SPI_SPICR 0x60
#define FPGA_SPI_SPISR 0x64
#define FPGA_SPI_DTR 0x68
#define FPGA_SPI_DRR 0x6C
#define FPGA_SPI_SPISSR 0x70
#define FPGA_SPI_TX_FIFO 0x74
#define FPGA_SPI_RX_FIFO 0x78

// Interrupt Control
#define FPGA_SPI_DGIER 0x1C
#define FPGA_SPI_IPISR 0x20
#define FPGA_SPI_IPIER 0x28


#define SPI_READ32( Name ) IORD_32DIRECT( FPGA_SPI_BASE_ADDR, Name )
#define SPI_WRITE32( Name, Value ) IOWR_32DIRECT( FPGA_SPI_BASE_ADDR, Name, Value )

// this structure must be initialized before calling pcie_init_isp_io
extern struct uio_info *info;

static void *p_base = NULL;
static int32_t base_valid = 0;


int32_t IORD_32DIRECT( uint32_t BASE, uint32_t REGNUM )
{

    return ioread32( p_base + REGNUM );
}

void IOWR_32DIRECT( uint32_t BASE, uint32_t REGNUM, uint32_t DATA )
{

    iowrite32( DATA, (void *)p_base + REGNUM );
}

static int32_t init_spi_core( void )
{
    // Reset FIFO's
    SPI_WRITE32( FPGA_SPI_SPICR, ( FPGA_SPI_MASTER | FPGA_SPI_FIFO_RST ) );
    SPI_WRITE32( FPGA_SPI_SPICR, FPGA_SPI_MASTER & ~SPI_CR_MANUAL_SS_MASK );

    // No slave is select
    SPI_WRITE32( FPGA_SPI_SPISSR, SPI_SS_DEFAULT );
    LOG( LOG_CRIT, "INIT SPI" );
    return 0;
}


int32_t system_spi_init( void )
{
    int rc = 0;
    void *virt_addr = NULL;
    virt_addr = ioremap( FPGA_SPI_BASE_ADDR, 0x1000 );

    p_base = virt_addr;

    LOG( LOG_CRIT, "INIT SPI SYSTEM - virt address 0x%x", p_base );
    if ( p_base != NULL ) {
        base_valid = 1;

        init_spi_core();

        LOG( LOG_CRIT, "SPI bus has been initialized" );
    } else {
        LOG( LOG_ERR, "failed to map spi memory" );
        p_base = NULL;
        rc = -1;
    }

    return rc;
}

//Had to add the function rw48,that is used to read/write when the CHAR_LEN of SPI is more that 32 bits.
//Since there is limitation in HW,where it cannot shift more than 32 bits,we have to do in two steps.
//Step1: Is to write the address.Step2 is for data read/write based on the command.
//Slave select has to be low throught out both the steps and hence autoslave select is diabled.
uint32_t system_spi_rw48( uint32_t sel_mask, uint32_t control_inp, uint32_t address, uint8_t addr_size, uint32_t data, uint8_t data_size )
{
    // Original code for the opencores SPI used to write 16 bit address then if SBUS_MASK_SPI_HALF_ADDR is set
    // it would send address but with 32 bit, i guess it was used for read purpose, if not set it would write data.
    uint32_t i, a;
    uint32_t ctrl;
    uint32_t size = ( CHAR_LEN_MSK & control_inp ) >> 3;
    a = address;

    //LOG(LOG_CRIT, "WRITE32 0x%x, size 0x%x, data 0x%x", control, size, data );

    // Select slave
    SPI_WRITE32( FPGA_SPI_SPISSR, ~sel_mask );

    // Set up the device in enable master mode
    ctrl = SPI_READ32( FPGA_SPI_SPICR );
    ctrl |= SPI_CR_MASTER_MODE_MASK;
    if ( control_inp & LSB_MSK )
        ctrl |= SPI_CR_LSB_FIRST_MASK;
    else
        ctrl &= ~SPI_CR_LSB_FIRST_MASK;
    if ( !( control_inp & TX_NEG_MSK ) )
        ctrl |= ( SPI_CR_CLK_POLARITY_MASK | SPI_CR_CLK_PHASE_MASK );
    else
        ctrl &= ~( SPI_CR_CLK_POLARITY_MASK | SPI_CR_CLK_PHASE_MASK );
    SPI_WRITE32( FPGA_SPI_SPICR, ctrl );

    // Fill up the transmitter with data
    for ( i = 0; i < addr_size; i++ ) {
        SPI_WRITE32( FPGA_SPI_DTR, address & 0xff );
        address >>= 8;
    }

    // double check this!!!!!:
    if ( address & 0x8000 ) // SBUS_MASK_SPI_HALF_ADDR
        //SPI_WRITE32(FPGA_SPI_DTR, address);
        for ( i = 0; i < size; i++ ) {
            SPI_WRITE32( FPGA_SPI_DTR, ~a & 0xff );
            a >>= 8;
        }
    else
        for ( i = 0; i < size; i++ ) {
            SPI_WRITE32( FPGA_SPI_DTR, ~data & 0xff );
            data >>= 8;
        }

    // Enable the device
    ctrl = SPI_READ32( FPGA_SPI_SPICR );
    ctrl |= SPI_CR_ENABLE_MASK;
    ctrl &= ~SPI_CR_TRANS_INHIBIT_MASK;
    SPI_WRITE32( FPGA_SPI_SPICR, ctrl );

    // Wait for the transmit FIFO to transition to empty
    while ( !( SPI_READ32( FPGA_SPI_SPISR ) & SPI_SR_TX_EMPTY_MASK ) )
        ;

    // Disable the device
    ctrl &= ~SPI_CR_ENABLE_MASK;
    ctrl |= SPI_CR_TRANS_INHIBIT_MASK;
    SPI_WRITE32( FPGA_SPI_SPICR, ctrl );

    return (uint32_t)SPI_READ32( FPGA_SPI_DRR );
}

uint32_t system_spi_rw32( uint32_t sel_mask, uint32_t control, uint32_t data, uint8_t data_size )
{
    uint32_t i;
    uint32_t ctrl;
    uint32_t size = ( CHAR_LEN_MSK & control ) >> 3;

    //LOG(LOG_CRIT, "WRITE32 0x%x, size 0x%x, data 0x%x", control, size, data );

    // Select slave
    SPI_WRITE32( FPGA_SPI_SPISSR, ~sel_mask );

    // Set up the device in enable master mode
    ctrl = SPI_READ32( FPGA_SPI_SPICR );
    ctrl |= SPI_CR_MASTER_MODE_MASK;
    if ( control & LSB_MSK )
        ctrl |= SPI_CR_LSB_FIRST_MASK;
    else
        ctrl &= ~SPI_CR_LSB_FIRST_MASK;
    if ( !( control & TX_NEG_MSK ) )
        ctrl |= ( SPI_CR_CLK_POLARITY_MASK | SPI_CR_CLK_PHASE_MASK );
    else
        ctrl &= ~( SPI_CR_CLK_POLARITY_MASK | SPI_CR_CLK_PHASE_MASK );
    SPI_WRITE32( FPGA_SPI_SPICR, ctrl );

    // Fill up the transmitter with data
    for ( i = 0; i < size; i++ ) {
        SPI_WRITE32( FPGA_SPI_DTR, ~data & 0xff );
        data >>= 8;
    }

    // Enable the device
    ctrl = SPI_READ32( FPGA_SPI_SPICR );
    ctrl |= SPI_CR_ENABLE_MASK;
    ctrl &= ~SPI_CR_TRANS_INHIBIT_MASK;
    SPI_WRITE32( FPGA_SPI_SPICR, ctrl );

    // Wait for the transmit FIFO to transition to empty
    while ( !( SPI_READ32( FPGA_SPI_SPISR ) & SPI_SR_TX_EMPTY_MASK ) )
        ;

    while ( ( SPI_READ32( FPGA_SPI_SPISR ) & SPI_SR_RX_EMPTY_MASK ) )
        ;

    // Disable the device
    ctrl &= ~SPI_CR_ENABLE_MASK;
    ctrl |= SPI_CR_TRANS_INHIBIT_MASK;
    SPI_WRITE32( FPGA_SPI_SPICR, ctrl );

    //return (uint32_t)SPI_READ32( FPGA_SPI_DRR );
    ctrl = 0;
    for ( i = 0; i < size; i++ ) {
        ctrl |= ( ( SPI_READ32( FPGA_SPI_DRR ) & 0xff ) << ( i * 8 ) );
    }
    return (uint32_t)ctrl;
}
