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

#include <asm/io.h>
#include "system_log.h"
#include "system_i2c.h"
#include "acamera_logger.h"
#include <linux/jiffies.h>

#define FPGA_IIC_MEM_SIZE 0x1000
#define FPGA_IIC_TX_FIFO 0x108
#define FPGA_IIC_RX_FIFO_PIRQ 0x120
#define FPGA_IIC_CR 0x100
#define FPGA_IIC_SR 0x104
#define FPGA_IIC_RX_FIFO 0x10c

#define RX_EMPTY_SHIFT 6
#define TX_EMPTY_SHIFT 7
#define BUSY_BUS_SHIFT 2

#define FPGA_IIC_TIMEOUT_HZ ( HZ / 10 )
// maximum i2c contexts = 2 * FIRMWARE_CONTEXT_NUMBER - one for sensor, one for lens
#define FPGA_IIC_MASTERS_MAX ( 2 * FIRMWARE_CONTEXT_NUMBER )

typedef struct _iic_master_t {
    uint32_t phy_addr;
    void *virt_addr;
} iic_master_t;


static iic_master_t iic_master[FPGA_IIC_MASTERS_MAX];
static uint32_t bus_number = 0;


static void *iic_get_vaddr( uint32_t bus )
{
    void *result = NULL;
    uint32_t idx = 0;

    for ( idx = 0; idx < bus_number; idx++ ) {
        if ( iic_master[idx].phy_addr == bus ) {
            result = iic_master[idx].virt_addr;
            break;
        }
    }
    return result;
}


void system_i2c_init( uint32_t bus )
{
    LOG( LOG_CRIT, "I2C bus init for bus 0x%x", bus );
    // check if the i2c master was already initialized
    // it may happen if the lens driver uses the same iic as
    // the sensor driver. no need to init it twice
    void *virt_addr = iic_get_vaddr( bus );
    if ( virt_addr == NULL ) {
        if ( bus_number < FPGA_IIC_MASTERS_MAX ) {
            iic_master[bus_number].phy_addr = 0;
            iic_master[bus_number].virt_addr = NULL;

            void *virt_addr = NULL;
            virt_addr = ioremap( bus, FPGA_IIC_MEM_SIZE );

            if ( virt_addr != NULL ) {
                // start
                iowrite32( 0xf, virt_addr + FPGA_IIC_RX_FIFO_PIRQ );
                // reset
                iowrite32( 2, virt_addr + FPGA_IIC_CR );
                iowrite32( 1, virt_addr + FPGA_IIC_CR );

                iic_master[bus_number].phy_addr = bus;
                iic_master[bus_number].virt_addr = virt_addr;
                bus_number++;
                // init communication phy_addr
                LOG( LOG_CRIT, "I2C connection has been opened" );
            } else {
                LOG( LOG_CRIT, "Failed to map i2c memory 0x%x", bus );
            }
        } else {
            LOG( LOG_CRIT, "Failed to init %d iic master. Maximum supported masters are %d", bus_number, FPGA_IIC_MASTERS_MAX );
        }
    } else {
        LOG( LOG_WARNING, "Skipped iic initialization. Bus 0x%x initialized already", bus );
    }
}

void system_i2c_deinit( uint32_t bus )
{
}

void pcie_i2c_close( void )
{
}


static int32_t iic_bus_is_ready( void *virt_addr )
{
    int32_t result = 0;
    uint32_t busy_bus = 0;
    uint32_t ready = 0;
    uint32_t status_reg = 0;
    uint32_t rx_empty = 0;
    uint32_t tx_empty = 0;
    uint32_t js = jiffies;
    uint32_t jt = js;
    LOG( LOG_DEBUG, "status reg 0x%x", status_reg );
    while ( ready == 0 ) {
        status_reg = ioread32( virt_addr + FPGA_IIC_SR );

        rx_empty = ( status_reg >> RX_EMPTY_SHIFT ) & 1;
        tx_empty = ( status_reg >> TX_EMPTY_SHIFT ) & 1;
        busy_bus = ( status_reg >> BUSY_BUS_SHIFT ) & 1;

        if ( rx_empty == 1 ) {
            if ( tx_empty == 1 ) {
                if ( busy_bus == 0 ) {
                    ready = 1;
                } else {
                    LOG( LOG_DEBUG, "Error. Bus is busy" );
                }
            } else {
                LOG( LOG_DEBUG, "Error. TX FIFO is not empty" );
            }
        } else {
            LOG( LOG_DEBUG, "Error. RX FIFO is not empty" );
        }

        jt = jiffies;
        if ( ( jt - js ) > FPGA_IIC_TIMEOUT_HZ ) {
            result = -1;
            LOG( LOG_ERR, "IIC line is unavailable" );
            break;
        }
    }
    return result;
}

uint32_t read_phy_addr = 0;
uint32_t read_size = 0;

//uint8_t i2c_write(uint8_t phy_addr, uint8_t* data, uint32_t size)
uint8_t system_i2c_write( uint32_t bus, uint32_t phy_addr, uint8_t *data, uint32_t size )
{
    //AXI IIC Bus Interface v2.0 LogicCORE IP Product Guide
    //http://japan.xilinx.com/support/documentation/ip_documentation/axi_iic/v2_0/pg090-axi-iic.pdf
    //page - 37
    //
    //1.   Check all FIFOs empty and bus not busy by reading the SR.
    //2.   Write 0x134 to the TX_FIFO (set the start bit, the device phy_addr, write access).
    //3.   Write 0x33 to the TX_FIFO (EEPROM phy_addr for data).
    //4.   Write 0x89 to the TX_FIFO (byte 1).
    //5.   Write 0xAB to the TX_FIFO (byte 2).
    //6.   Write 0xCD to the TX_FIFO (byte 3).
    //7.   Write 0x2EF to the TX_FIFO (stop bit, byte 4).
    void *virt_addr = iic_get_vaddr( bus );
    uint8_t result = I2C_OK;
    uint8_t addr = ( phy_addr << 1 ) & 0xff;

    if ( virt_addr != NULL ) {
        LOG( LOG_DEBUG, "I2C Write phy_addr 0x%x  size %d", addr, size );

        uint32_t no_stop = phy_addr & 0x20000;

        if ( no_stop != 0 ) {
            uint32_t idx = 0;
            uint32_t reg_addr = 0;
            for ( idx = 0; idx < size; idx++ ) {
                reg_addr <<= 8;
                reg_addr |= data[idx];
            }
            read_phy_addr = reg_addr;
            read_size = size;
            LOG( LOG_CRIT, "Save i2c read phy_addr 0x%x, size %d", read_phy_addr, read_size );
            return I2C_OK;
        }

        uint32_t idx = 0;
        // start sequence
        //LOG(LOG_DEBUG, "I2C write phy_addr = 0x%x, size = %d", phy_addr, size ) ;
        uint32_t start_seq = 0x00000100 + addr;
        // wait until iic is ready
        result = iic_bus_is_ready( virt_addr );
        if ( result == 0 ) {
            //LOG(LOG_DEBUG, "Writing 0x%x", start_seq ) ;
            iowrite32( start_seq, virt_addr + FPGA_IIC_TX_FIFO );
            for ( idx = 0; idx < size; idx++ ) {
                uint32_t val = data[idx];
                if ( idx == size - 1 ) {
                    // add termination bit
                    //if ( no_stop == 0 ) {
                    val = 0x00000200 + val;
                    //}
                }
                //LOG(LOG_DEBUG, "Writing 0x%x", val ) ;
                iowrite32( val, virt_addr + FPGA_IIC_TX_FIFO );
            }
            LOG( LOG_DEBUG, "I2C write finished. " );
            result = I2C_OK;
        } else {
            LOG( LOG_CRIT, "I2C write failed. No connect for the bus 0x%x ", bus );
            result = I2C_NOCONNECT;
        }
    } else {
        LOG( LOG_CRIT, "IIC virtual phy_addr for bus 0x%x was not found", bus );
        result = I2C_NOCONNECT;
    }

    return result;
}

static int32_t rx_fifo_has_data( void *virt_addr )
{
    uint32_t result = 0;
    uint32_t rx_empty = 0;
    uint32_t busy_bus = 0;
    uint32_t status_reg = 0;
    uint32_t js = jiffies;
    uint32_t jt = js;

    while ( result == 0 ) {
        status_reg = ioread32( virt_addr + FPGA_IIC_SR );
        LOG( LOG_DEBUG, "status reg 0x%x", status_reg );
        rx_empty = ( status_reg >> RX_EMPTY_SHIFT ) & 1;
        busy_bus = ( status_reg >> BUSY_BUS_SHIFT ) & 1;

        if ( busy_bus == 0 ) {
            if ( rx_empty == 0 ) {
                result = 1;
            } else {
                LOG( LOG_DEBUG, "Read - RX fifo is empty" );
            }
        } else {
            LOG( LOG_DEBUG, "Read - bus is busy" );
        }

        jt = jiffies;
        if ( ( jt - js ) > FPGA_IIC_TIMEOUT_HZ ) {
            result = -1;
            LOG( LOG_ERR, "Has data timeout. " );
            break;
        }
    }
    return result;
}

//uint8_t i2c_read(uint8_t phy_addr, uint8_t* data, uint32_t size)
uint8_t system_i2c_read( uint32_t bus, uint32_t phy_addr, uint8_t *data, uint32_t size )
{
    //AXI IIC Bus Interface v2.0 LogicCORE IP Product Guide
    //http://japan.xilinx.com/support/documentation/ip_documentation/axi_iic/v2_0/pg090-axi-iic.pdf
    //page - 37
    //
    //1.   Check all FIFOs empty and bus not busy by reading the Status register.
    //2.   Write 0x134 to the TX_FIFO (set start bit, device phy_addr to 0x34, write access).
    //3.   Write 0x33 to the TX_FIFO (EEPROM phy_addr for data).
    //4.   Write 0x135 to the TX_FIFO (set start bit for repeated start, device phy_addr 0x34, read access).
    //5.   Write 0x204 to the TX_FIFO (set stop bit, four bytes to be received by the AXI IIC).
    //6.   Wait for RX_FIFO not empty.
    //a.   Read the RX_FIFO byte.
    //b.   If the fourth byte is read, exit; otherwise, continue checking RX_FIFO not empty.

    void *virt_addr = iic_get_vaddr( bus );
    uint32_t result = 0;
    uint8_t addr = ( phy_addr << 1 ) & 0xff;

    if ( virt_addr != NULL ) {
        LOG( LOG_CRIT, "I2C read phy_addr 0x%x  size %d", addr, size );

        uint32_t idx = 0;
        // start sequence
        //LOG(LOG_DEBUG, "I2C write phy_addr = 0x%x, size = %d", phy_addr, size ) ;
        uint32_t start_seq = 0x00000100 + addr;

        result = iic_bus_is_ready( virt_addr );

        if ( result == 0 ) {
            LOG( LOG_CRIT, "Reading 0x%x", start_seq );

            if ( read_size != 0 ) {
                // the read_phy_addr and read_size values are saved in the system_i2c_write call.
                // the write function is called inside sbus right before system_i2c_read is called
                // so we can be sure that the correct register phy_addr is set up.
                uint32_t idx = 0;
                LOG( LOG_CRIT, "Write read phy_addr 0x%x, size %d", read_phy_addr, read_size );
                iowrite32( start_seq, virt_addr + FPGA_IIC_TX_FIFO );
                for ( idx = 0; idx < read_size; idx++ ) {
                    uint8_t addr_byte = ( read_phy_addr >> ( ( read_size - idx - 1 ) * 8 ) ) & 0xFF;
                    iowrite32( addr_byte, virt_addr + FPGA_IIC_TX_FIFO );
                    LOG( LOG_CRIT, "Write 0x%x", addr_byte );
                }
                read_size = 0;
            }

            iowrite32( start_seq + 1, virt_addr + FPGA_IIC_TX_FIFO );
            iowrite32( 0x200 + size, virt_addr + FPGA_IIC_TX_FIFO );

            for ( idx = 0; idx < size; idx++ ) {
                // wait until the write is done
                LOG( LOG_CRIT, "Reading wait for rx fifo " );
                if ( rx_fifo_has_data( virt_addr ) != 1 ) {
                    LOG( LOG_CRIT, "Failed to get data from FIFO" );
                };
                *data = ioread32( virt_addr + FPGA_IIC_RX_FIFO );
                LOG( LOG_CRIT, "I2C read %d finished. val is 0x%x", idx, *data );
                data++;
            }
        } else {
            result = I2C_NOCONNECT;
        }
    } else {
        LOG( LOG_CRIT, "IIC virtual phy_addr for bus 0x%x was not found", bus );
        result = I2C_NOCONNECT;
    }
    return result;
}


uint32_t IORD( uint32_t BASE, uint32_t REGNUM )
{
    return 0;
}

void IOWR( uint32_t BASE, uint32_t REGNUM, uint32_t DATA )
{
}
