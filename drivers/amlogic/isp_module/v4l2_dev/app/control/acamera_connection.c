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
#include <linux/vmalloc.h>
#include "acamera_types.h"
#include "acamera_logger.h"
#include "acamera_math.h"
#include "acamera_firmware_config.h"
#include "acamera_control_config.h"

#if ISP_HAS_CONNECTION_BUFFER
#define ALIGNMENT_MASK 3
#include "acamera_buffer_manager.h"
#include "acamera_cmd_queues_config.h"
#define CMD_QUEUES_HALF_INX ( ACAMERA_CMD_QUEUES_SIZE >> 3 )
#elif ISP_HAS_CONNECTION_UART
#define ALIGNMENT_MASK 0
#include "acamera_uart.h"
#elif ISP_HAS_CONNECTION_SOCKET
#define ALIGNMENT_MASK 0
#include "acamera_socket.h"
#include "acamera_socket_strategy.h"
#elif ISP_HAS_CONNECTION_CHARDEV
#define ALIGNMENT_MASK 0
#include "acamera_chardev.h"
#endif
#include "system_hw_io.h"
#include "system_sw_io.h"
#include "acamera_command_api.h"
#include "application_command_api.h"
#include "acamera_isp_core_nomem_settings.h"
#include "acamera_fw.h"




#if ISP_HAS_CONNECTION_BUFFER
static acamera_buffer_manager_t buff_mgr;
#elif ISP_HAS_CONNECTION_UART
static acamera_uart_t uart;
#endif


#if ISP_HAS_CONNECTION_SOCKET
extern struct acamera_socket_strategy acamera_socket_init_strategy();
#endif

//#define TRACE

#ifdef TRACE
/* This printf library call put there intentionally to debug the connection
 * on the limited subset of the supported architectures
 * bypassing project loglevel settings.
 */
#define ACAMERA_CONNECTION_TRACE printf
#else
#define ACAMERA_CONNECTION_TRACE( ... )
#endif /* TRACE */

#define HEADER_SIZE 12

enum {
    API_RESET = 0,
    API_READ,
    API_WRITE
};

enum TransactionType {
    TransactionTypeRegRead = 1,
    TransactionTypeRegWrite,
    TransactionTypeRegMaskWrite,
    TransactionTypeLUTRead,
    TransactionTypeLUTWrite,

    TransactionTypeAPIRead = 10,
    TransactionTypeAPIWrite,

    TransactionTypeBUFRead = 20,
    TransactionTypeBUFWrite
};

typedef int ( *data_read_f )( void *p_ctrl, uint8_t *data, int size );
typedef int ( *data_write_f )( void *p_ctrl, const uint8_t *data, int size );

enum {
    STATE_IDLE,
    STATE_RX_DATA,
    STATE_SKIP_DATA,
    STATE_TX_PACKET
};

typedef struct {
    void *param;
    data_read_f data_read;
    data_write_f data_write;
    int state;
    uint8_t *buffer;
    uint32_t rx_buffer_inx;
    uint32_t rx_buffer_size;
    uint32_t tx_buffer_inx;
    uint32_t tx_buffer_size;
} connection_t;

#if ISP_HAS_STREAM_CONNECTION
static connection_t con;

static void reset_connection( void )
{
    con.state = STATE_IDLE;
    con.rx_buffer_inx = 0;
    con.tx_buffer_inx = 0;
    ACAMERA_CONNECTION_TRACE( "reset connection" );
}

extern void *acamera_get_api_ctx_ptr( void );

#if ISP_HAS_CONNECTION_SOCKET
static int acamera_socket_do_init( connection_t *con )
{
    struct acamera_socket_f *f;
    struct acamera_socket *socket;
    int rc;


   /* f = acamera_socket_f_impl();
    if ( !f ) {
        LOG( LOG_ERR, "Please double check your platform provide TCP/IP socket routines" );
        return -1;
    }
*/
    socket = acamera_socket_instance();


    rc = acamera_socket_init( socket, acamera_socket_init_strategy() );
    if ( rc )
        return rc;

    con->param = socket;
    con->data_read = (data_read_f)acamera_socket_read;
    con->data_write = (data_write_f)acamera_socket_write;

    return 0;
}
#endif
#endif //ISP_HAS_STREAM_CONNECTION

void acamera_connection_init( void )
{
#if ISP_HAS_STREAM_CONNECTION
    reset_connection();
#if ISP_HAS_CONNECTION_BUFFER
    acamera_buffer_manager_init( &buff_mgr, ACAMERA_CMD_QUEUES_BASE_ADDR, ACAMERA_CMD_QUEUES_SIZE );
    con.param = &buff_mgr;
    con.data_read = (data_read_f)acamera_buffer_manager_read;
    con.data_write = (data_write_f)acamera_buffer_manager_write;
    acamera_cmd_queues_array_data_write( 0, 2, API_RESET << 24 );
    acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX, API_RESET );
    ACAMERA_CONNECTION_TRACE( "Connection is buffer" );
#elif ISP_HAS_CONNECTION_UART
    con.param = &uart;
    acamera_uart_init( con.param );
    con.data_read = (data_read_f)acamera_uart_read;
    con.data_write = (data_write_f)acamera_uart_write;
    ACAMERA_CONNECTION_TRACE( "Connection is UART" );
#elif ISP_HAS_CONNECTION_SOCKET
    if ( !acamera_socket_do_init( &con ) ) {
        ACAMERA_CONNECTION_TRACE( "Connection is TCP/IP socket" );
    } else {
        LOG( LOG_ERR, "Unable to initialise TCP/IP socket" );
    }
#elif ISP_HAS_CONNECTION_CHARDEV
    if ( !acamera_chardev_init() ) {
        ACAMERA_CONNECTION_TRACE( "Connection is character device" );
        con.data_read = (data_read_f)acamera_chardev_read;
        con.data_write = (data_write_f)acamera_chardev_write;
		con.buffer = vmalloc(CONNECTION_BUFFER_SIZE);
    } else {
        LOG( LOG_ERR, "Unable to register character device" );
    }
#endif
    if ( !con.data_read || !con.data_write ) {
        LOG( LOG_WARNING, "Connection method is not defined" );
    }
#endif //ISP_HAS_STREAM_CONNECTION
}

void acamera_connection_destroy( void )
{
#if ISP_HAS_STREAM_CONNECTION
#if ISP_HAS_CONNECTION_CHARDEV
    int rc = acamera_chardev_destroy();
    if ( rc )
        LOG( LOG_ERR, "Unable to destroy character device with error %d", rc );
#else
    ACAMERA_CONNECTION_TRACE( LOG_WARNING, "Connection destroy method is not defined" );
#endif /* connection type */
    con.param = NULL;
    con.data_read = NULL;
    con.data_write = NULL;
    if (con.buffer)
        vfree(con.buffer);
#endif /* ISP_HAS_STREAM_CONNECTION */
}


#if ISP_HAS_STREAM_CONNECTION
static void write_32( uint32_t addr, uint8_t value, uint8_t msk )
{
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();
    int shift = ( addr & 3 ) << 3;
    uint32_t mask = msk << shift;
    uint32_t addr_align = addr & ~3;

    // Use SW registers for ping/pong memory, otherwise, use HW registers.
    if ( ( addr < ISP_CONFIG_LUT_OFFSET ) ||
         ( addr > ISP_CONFIG_PING_OFFSET + 2 * ISP_CONFIG_PING_SIZE ) ) {
        uint32_t data = system_hw_read_32( addr_align );
        data = ( data & ~mask ) | ( ( uint32_t )( value & msk ) << shift );
        system_hw_write_32( addr_align, data );
    } else {
        // read from software context
        uintptr_t sw_addr = context_ptr->settings.isp_base + addr_align;
        uint32_t data = system_sw_read_32( sw_addr );
        data = ( data & ~mask ) | ( ( uint32_t )( value & msk ) << shift );
        system_sw_write_32( sw_addr, data );
    }
}

static uint8_t read_32( uint32_t addr )
{
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    uint8_t result = 0;
    int shift = ( addr & 3 ) << 3;
    uint32_t addr_align = addr & ~3;
    uint32_t data = 0;

    // Use SW registers for ping/pong memory, otherwise, use HW registers.
    if ( ( addr < ISP_CONFIG_LUT_OFFSET ) ||
         ( addr > ISP_CONFIG_PING_OFFSET + 2 * ISP_CONFIG_PING_SIZE ) ) {
        data = system_hw_read_32( addr_align );
    } else {
        uintptr_t sw_addr = context_ptr->settings.isp_base + addr_align;
        data = system_sw_read_32( sw_addr );
    }

    result = ( data >> shift ) & 0xFF;
    return result;
}

static void process_request( void )
{
    uint32_t *rx_buf = (uint32_t *)&con.buffer[8];
    uint32_t *tx_buf = (uint32_t *)con.buffer;
    uint16_t type = ( *rx_buf++ ) & 0xFFFF;
    ACAMERA_CONNECTION_TRACE( "Processing packet size %ld, id %ld, type %d\n", con.rx_buffer_size, tx_buf[1], type );
    switch ( type ) {
    case TransactionTypeRegRead:
        if ( con.rx_buffer_size == HEADER_SIZE + 8 ) {
            uint32_t addr = *rx_buf++;
            uint32_t size = *rx_buf++;
            if ( size <= CONNECTION_BUFFER_SIZE - HEADER_SIZE - 4 ) {
                con.tx_buffer_size = HEADER_SIZE + 4 + size;
                tx_buf[3] = SUCCESS;
                uint8_t *b = &con.buffer[HEADER_SIZE + 4];

                while ( size-- ) {
                    *b++ = read_32( addr++ ); //system_sw_read_8(addr++);
                }
            } else {
                con.tx_buffer_size = HEADER_SIZE + 4;
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeRegWrite:
        if ( con.rx_buffer_size >= HEADER_SIZE + 8 ) {
            uint32_t addr = *rx_buf++;
            uint32_t size = *rx_buf++;
            con.tx_buffer_size = HEADER_SIZE + 4;
            if ( size <= con.rx_buffer_size - HEADER_SIZE - 8 ) {
                tx_buf[3] = SUCCESS;
                uint8_t *b = &con.buffer[HEADER_SIZE + 8];
                while ( size-- ) {
                    write_32( addr++, *b++, 0xFF );
                }
            } else {
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeRegMaskWrite:
        if ( con.rx_buffer_size >= HEADER_SIZE + 8 ) {
            uint32_t addr = *rx_buf++;
            uint32_t size = *rx_buf++;
            con.tx_buffer_size = HEADER_SIZE + 4;
            if ( 2 * size <= con.rx_buffer_size - HEADER_SIZE - 8 ) {
                tx_buf[3] = SUCCESS;
                uint8_t *b = &con.buffer[HEADER_SIZE + 8];
                uint8_t *m = &con.buffer[HEADER_SIZE + 8 + size];
                while ( size-- ) {
                    uint8_t mask = *m++;
                    uint8_t val = *b++;
                    write_32( addr, val, mask );
                    addr++;
                }
            } else {
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeLUTRead:
        if ( con.rx_buffer_size == HEADER_SIZE + 8 ) {
            uint32_t addr = *rx_buf++;
            uint32_t size = *rx_buf++;
            if ( size <= CONNECTION_BUFFER_SIZE - HEADER_SIZE - 4 && !( size & 3 ) && !( addr & 3 ) ) {
                con.tx_buffer_size = HEADER_SIZE + 4 + size;
                tx_buf[3] = SUCCESS;
                uint32_t *b = &tx_buf[4];
                uint32_t i;
                for ( i = 0; i < size; i += 4 ) {
                    system_hw_write_32( addr, i >> 2 );
                    *b++ = system_hw_read_32( addr + 8 );
                }
            } else {
                con.tx_buffer_size = HEADER_SIZE + 4;
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeLUTWrite:
        if ( con.rx_buffer_size >= HEADER_SIZE + 8 ) {
            uint32_t addr = *rx_buf++;
            uint32_t size = *rx_buf++;
            con.tx_buffer_size = HEADER_SIZE + 4;
            if ( size <= con.rx_buffer_size - HEADER_SIZE - 8 && !( size & 3 ) && !( addr & 3 ) ) {
                tx_buf[3] = SUCCESS;
                uint32_t i;
                for ( i = 0; i < size; i += 4 ) {
                    system_hw_write_32( addr, i >> 2 );
                    system_hw_write_32( addr + 4, *rx_buf++ );
                }
            } else {
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeAPIRead:
        if ( con.rx_buffer_size == HEADER_SIZE + 8 ) {
            uint8_t t = rx_buf[0] & 0xFF;
            uint8_t c = ( rx_buf[0] >> 8 ) & 0xFF;
            con.tx_buffer_size = HEADER_SIZE + 8;
            tx_buf[3] = application_command( t, c, rx_buf[1], COMMAND_GET, &tx_buf[4] );
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeAPIWrite:
        if ( con.rx_buffer_size == HEADER_SIZE + 8 ) {
            uint8_t t = rx_buf[0] & 0xFF;
            uint8_t c = ( rx_buf[0] >> 8 ) & 0xFF;
            con.tx_buffer_size = HEADER_SIZE + 8;
            tx_buf[3] = application_command( t, c, rx_buf[1], COMMAND_SET, &tx_buf[4] );
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeBUFRead:
        if ( con.rx_buffer_size == HEADER_SIZE + 8 ) {
            uint8_t id = rx_buf[0] & 0xFF;
            uint8_t buf_class = ( rx_buf[0] >> 8 ) & 0xFF;
            uint32_t size = rx_buf[1];
            uint32_t value;
            if ( size <= CONNECTION_BUFFER_SIZE - HEADER_SIZE - 4 ) {
                switch ( buf_class ) {
                case STATIC_CALIBRATIONS_ID:
                case DYNAMIC_CALIBRATIONS_ID:
                    con.tx_buffer_size = HEADER_SIZE + 4 + size;
                    tx_buf[3] = application_api_calibration( buf_class, id, COMMAND_GET, &tx_buf[4], size, &value );
                    if ( tx_buf[3] != SUCCESS ) {
                        con.tx_buffer_size = HEADER_SIZE + 4;
                    }
                    break;

                default:
                    con.tx_buffer_size = HEADER_SIZE + 4;
                    tx_buf[3] = FAIL;
                    LOG( LOG_WARNING, "Wrong buffer class %u", (unsigned int)buf_class );
                }
            } else {
                con.tx_buffer_size = HEADER_SIZE + 4;
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    case TransactionTypeBUFWrite:
        if ( con.rx_buffer_size >= HEADER_SIZE + 8 ) {
            uint8_t id = rx_buf[0] & 0xFF;
            uint8_t buf_class = ( rx_buf[0] >> 8 ) & 0xFF;
            uint32_t size = rx_buf[1];
            uint32_t value;
            con.tx_buffer_size = HEADER_SIZE + 4;
            if ( size <= con.rx_buffer_size - HEADER_SIZE - 8 ) {
                switch ( buf_class ) {
                case STATIC_CALIBRATIONS_ID:
                case DYNAMIC_CALIBRATIONS_ID:
                    tx_buf[3] = application_api_calibration( buf_class, id, COMMAND_SET, &rx_buf[2], size, &value );
                    break;

                default:
                    tx_buf[3] = FAIL;
                    LOG( LOG_WARNING, "Wrong buffer class %d", buf_class );
                }
            } else {
                tx_buf[3] = FAIL;
                LOG( LOG_WARNING, "Wrong request size %u for type %u", (unsigned int)size, (unsigned int)type );
            }
        } else {
            con.tx_buffer_size = HEADER_SIZE;
            LOG( LOG_WARNING, "Wrong packet size %u for type %u", (unsigned int)con.rx_buffer_size, (unsigned int)type );
        }
        break;
    default:
        con.tx_buffer_size = HEADER_SIZE;
        LOG( LOG_WARNING, "Wrong packet type %d", type );
    }
    *tx_buf = con.tx_buffer_size;
    con.tx_buffer_size = ( con.tx_buffer_size + ALIGNMENT_MASK ) & ~ALIGNMENT_MASK;
    con.tx_buffer_inx = 0;
    con.state = STATE_TX_PACKET;
}
#endif //ISP_HAS_STREAM_CONNECTION

#if ISP_HAS_CONNECTION_BUFFER
static void process_api_request( void )
{
    uint8_t cmd_type, cmd;
    uint32_t status;
    uint32_t value;
    uint32_t request = acamera_cmd_queues_array_data_read( 0, 2 );
    switch ( request >> 24 ) {
    case API_READ:
        if ( ( acamera_cmd_queues_array_data_read( 0, CMD_QUEUES_HALF_INX ) & 0xFF ) == API_RESET ) {
            acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX + 1, acamera_cmd_queues_array_data_read( 0, 0 ) );
            acamera_cmd_queues_array_data_write( 0, 2, API_RESET << 24 );
            cmd_type = ( uint8_t )( request & 0xFF );
            cmd = ( uint8_t )( ( request >> 8 ) & 0xFF );
            value = acamera_cmd_queues_array_data_read( 0, 1 );
            status = application_command( cmd_type, cmd, value, COMMAND_GET, &value );
            acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX + 2, value );
            acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX, ( status << 16 ) | API_READ );
            ACAMERA_CONNECTION_TRACE( "Processed API read id %ld\n", acamera_cmd_queues_array_data_read( 0, CMD_QUEUES_HALF_INX + 1 ) );
        }
        break;
    case API_WRITE:
        if ( ( acamera_cmd_queues_array_data_read( 0, CMD_QUEUES_HALF_INX ) & 0xFF ) == API_RESET ) {
            acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX + 1, acamera_cmd_queues_array_data_read( 0, 0 ) );
            value = acamera_cmd_queues_array_data_read( 0, 1 );
            acamera_cmd_queues_array_data_write( 0, 2, API_RESET << 24 );
            cmd_type = ( uint8_t )( request & 0xFF );
            cmd = ( uint8_t )( ( request >> 8 ) & 0xFF );
            status = application_command( cmd_type, cmd, value, COMMAND_SET, &value );
            acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX + 2, value );
            acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX, ( status << 16 ) | API_WRITE );
            ACAMERA_CONNECTION_TRACE( "Processed API write id %ld\n", acamera_cmd_queues_array_data_read( 0, CMD_QUEUES_HALF_INX + 1 ) );
        }
        break;
    case API_RESET:
        break;
    default:
        LOG( LOG_ERR, "Wrong request %ld\n", request >> 24 );
        acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX + 1, acamera_cmd_queues_array_data_read( 0, 0 ) );
        acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX + 2, ERR_BAD_ARGUMENT );
        acamera_cmd_queues_array_data_write( 0, CMD_QUEUES_HALF_INX, ( FAIL << 16 ) | request );
        acamera_cmd_queues_array_data_write( 0, 2, API_RESET << 24 );
        break;
    }
}
#endif

void acamera_connection_process( void )
{
#if ISP_HAS_STREAM_CONNECTION
    int res = 0;
    int cnt = 20;
    uint32_t *const buf = (uint32_t *)con.buffer;

    if ( !con.data_read || !con.data_write ) {
        return;
    }

#if ISP_HAS_CONNECTION_BUFFER
    process_api_request();
#endif

    do {
        switch ( con.state ) {
        case STATE_IDLE:
            res = con.data_read( con.param, &con.buffer[con.rx_buffer_inx], HEADER_SIZE - con.rx_buffer_inx );
            if ( res != 0 )
                ACAMERA_CONNECTION_TRACE( "state %d res %d\n", con.state, res );
            if ( res < 0 ) {
                reset_connection();
                return;
            }
            con.rx_buffer_inx += res;
            if ( con.rx_buffer_inx >= HEADER_SIZE ) {
                con.rx_buffer_size = buf[0];
                if ( con.rx_buffer_size > CONNECTION_BUFFER_SIZE ) {
                    reset_connection();
                    LOG( LOG_WARNING, "Too big request %u, maximum allowed %u", (unsigned int)con.rx_buffer_size, (unsigned int)CONNECTION_BUFFER_SIZE );
                    con.state = STATE_SKIP_DATA;
                } else {
                    con.state = STATE_RX_DATA;
                }
            }
            break;
        case STATE_SKIP_DATA:
            res = con.data_read( con.param, &con.buffer[HEADER_SIZE], MIN( CONNECTION_BUFFER_SIZE - HEADER_SIZE, con.rx_buffer_size - con.rx_buffer_inx ) );
            if ( res != 0 )
                ACAMERA_CONNECTION_TRACE( "state %d res %d\n", con.state, res );
            if ( res < 0 ) {
                reset_connection();
                return;
            }
            con.rx_buffer_inx += res;
            if ( con.rx_buffer_inx >= con.rx_buffer_size ) {
                con.tx_buffer_size = HEADER_SIZE;
                *buf = con.tx_buffer_size;
                con.tx_buffer_inx = 0;
                con.state = STATE_TX_PACKET;
            }
            break;
        case STATE_RX_DATA:
            res = con.data_read( con.param, &con.buffer[con.rx_buffer_inx], con.rx_buffer_size - con.rx_buffer_inx );
            if ( res != 0 )
                ACAMERA_CONNECTION_TRACE( "state %d res %d\n", con.state, res );
            if ( res < 0 ) {
                reset_connection();
                return;
            }
            con.rx_buffer_inx += res;
            if ( con.rx_buffer_inx >= con.rx_buffer_size ) {
                process_request();
            }
            break;
        case STATE_TX_PACKET:
            res = con.data_write( con.param, &con.buffer[con.tx_buffer_inx], con.tx_buffer_size - con.tx_buffer_inx );
            if ( res != 0 )
                ACAMERA_CONNECTION_TRACE( "state %d res %d\n", con.state, res );
            if ( res < 0 ) {
                reset_connection();
                return;
            }
            con.tx_buffer_inx += res;
            if ( con.tx_buffer_inx >= con.tx_buffer_size ) {
                ACAMERA_CONNECTION_TRACE( "packet size %ld is transferred\n", con.tx_buffer_size );
                reset_connection();
                return; // this will make sure that FW itself will work as required
            }
            break;
        default:
            res = -1;
            LOG( LOG_CRIT, "Wrong state %d", con.state );
            reset_connection();
            return;
        }
    } while ( res > 0 && --cnt );
#endif //ISP_HAS_STREAM_CONNECTION
}
