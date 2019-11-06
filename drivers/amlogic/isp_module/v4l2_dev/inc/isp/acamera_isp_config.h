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

#ifndef __ACAMERA_ISP_CONFIG_H__
#define __ACAMERA_ISP_CONFIG_H__


#include "acamera_isp1_config.h"
#include "system_sw_io.h"

#include "system_hw_io.h"

// ------------------------------------------------------------------------------ //
// Instance 'isp' of module 'common_config'
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_BASE_ADDR (0x0L)
#define ACAMERA_ISP_SIZE (0x100)

#define ACAMERA_ISP_MAX_ADDR (4 * 0x3ffff)

// ------------------------------------------------------------------------------ //
// Group: id
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: API
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ID_API_DEFAULT (0x0)
#define ACAMERA_ISP_ID_API_DATASIZE (32)
#define ACAMERA_ISP_ID_API_OFFSET (0x0)
#define ACAMERA_ISP_ID_API_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_id_api_read(uintptr_t base) {
    return system_hw_read_32(0x0L);
}
// ------------------------------------------------------------------------------ //
// Register: Product
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ID_PRODUCT_DEFAULT (2658)
#define ACAMERA_ISP_ID_PRODUCT_DATASIZE (32)
#define ACAMERA_ISP_ID_PRODUCT_OFFSET (0x4)
#define ACAMERA_ISP_ID_PRODUCT_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_id_product_read(uintptr_t base) {
    return system_hw_read_32(0x4L);
}
// ------------------------------------------------------------------------------ //
// Register: Version
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ID_VERSION_DEFAULT (0x0)
#define ACAMERA_ISP_ID_VERSION_DATASIZE (32)
#define ACAMERA_ISP_ID_VERSION_OFFSET (0x8)
#define ACAMERA_ISP_ID_VERSION_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_id_version_read(uintptr_t base) {
    return system_hw_read_32(0x8L);
}
// ------------------------------------------------------------------------------ //
// Register: Revision
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ID_REVISION_DEFAULT (0x0)
#define ACAMERA_ISP_ID_REVISION_DATASIZE (32)
#define ACAMERA_ISP_ID_REVISION_OFFSET (0xc)
#define ACAMERA_ISP_ID_REVISION_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_id_revision_read(uintptr_t base) {
    return system_hw_read_32(0xcL);
}
// ------------------------------------------------------------------------------ //
// Group: isp global
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Miscellaneous top-level ISP controls
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Global FSM reset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            1 = synchronous reset of FSMs in design (faster recovery after broken frame)
//            when the MCU detects a broken frame or any other abnormal condition, the global_fsm_rest is
//            expected to be used.
//            MCU needs to follow a certain sequence for the same
//                1.	Read the status register to know the exact source of the error interrupt.
//                2.	Read the details of the error status register.
//                3.	Mask all the interrupts.
//                4.	Configure the input port register ISP_COMMON:input port: mode request to safe_stop mode.
//                5.	Read back the ISP_COMMON:input port: mode status register to see the status of mode request. 
//                    And wait until it shows the correct status.
//                6.	Wait for the ISP_COMMON:isp global monitor: fr pipeline busy signal to become low.
//                7.	Assert the global fsm reset.
//                8.	Clear the global fsm reset.
//                9.	Reconfigure the ISP configuration space.
//                10.	Unmask the necessary interrupt sources.
//11.	Configure the input port in safe_start mode.
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_GLOBAL_FSM_RESET_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_GLOBAL_FSM_RESET_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_GLOBAL_FSM_RESET_OFFSET (0x10)
#define ACAMERA_ISP_ISP_GLOBAL_GLOBAL_FSM_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_global_fsm_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x10L);
    system_hw_write_32(0x10L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_global_fsm_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x10L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: scaler fsm reset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = synchronous reset of FSMs in scaler design
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_SCALER_FSM_RESET_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_SCALER_FSM_RESET_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_SCALER_FSM_RESET_OFFSET (0x10)
#define ACAMERA_ISP_ISP_GLOBAL_SCALER_FSM_RESET_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_scaler_fsm_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x10L);
    system_hw_write_32(0x10L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_isp_global_scaler_fsm_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x10L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: dma global config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        [0]    : check bid
//        [1]    : check rid
//
//        The following signals are diagnostic signals which helps identifying some of the AXI interface
//        error cases. This must be used only as debug signals. You should follow these sequence
//        - Mask the dma error interrupt
//        - clear DMA interrupt if there is an active interrupt
//        - Write appropriate values to these registers
//        - Clear DMA alarms to clear the existing alarm
//        - unmask the DMA error interrpt
//
//
//        [2]    : frame_write_cancel/frame_read_cancel
//        [10:3] : awmaxwait_limit/armaxwait_limit
//        [18:11]: wmaxwait_limit
//        [26:19]: waxct_ostand_limit/rxnfr_ostand_limit
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DMA_GLOBAL_CONFIG_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_DMA_GLOBAL_CONFIG_DATASIZE (27)
#define ACAMERA_ISP_ISP_GLOBAL_DMA_GLOBAL_CONFIG_OFFSET (0x10)
#define ACAMERA_ISP_ISP_GLOBAL_DMA_GLOBAL_CONFIG_MASK (0x7ffffff0)

// args: data (27-bit)
static __inline void acamera_isp_isp_global_dma_global_config_write(uintptr_t base, uint32_t data) {
    uint32_t curr = system_hw_read_32(0x10L);
    system_hw_write_32(0x10L, (((uint32_t) (data & 0x7ffffff)) << 4) | (curr & 0x8000000f));
}
static __inline uint32_t acamera_isp_isp_global_dma_global_config_read(uintptr_t base) {
    return (uint32_t)((system_hw_read_32(0x10L) & 0x7ffffff0) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: Flush hblank
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Horizontal blanking interval during regeneration (0=measured input interval)
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_FLUSH_HBLANK_DEFAULT (0x20)
#define ACAMERA_ISP_ISP_GLOBAL_FLUSH_HBLANK_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_FLUSH_HBLANK_OFFSET (0x14)
#define ACAMERA_ISP_ISP_GLOBAL_FLUSH_HBLANK_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_isp_global_flush_hblank_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x14L);
    system_hw_write_32(0x14L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_isp_global_flush_hblank_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x14L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: ISP monitor select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_DEFAULT (4)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_DATASIZE (3)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_OFFSET (0x14)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_MASK (0x70000)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_LINEARISED_DATA_AFTER_LINEARISED_CLUSTER_MSB_ALIGNED_DATA190_16D0 (0)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_STATIC_DPC_OUTPUT_MSB_ALIGNED_DATA150_20D0 (1)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_OUTPUT_OF_CA_CORRECTION_MSB_ALIGNED_DATA150_20D0 (2)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_CNR_OUTPUT_B110_G110_R110 (3)
#define ACAMERA_ISP_ISP_GLOBAL_ISP_MONITOR_SELECT_OUTPUT_FORCES_TO_0 (4)

// args: data (3-bit)
static __inline void acamera_isp_isp_global_isp_monitor_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x14L);
    system_hw_write_32(0x14L, (((uint32_t) (data & 0x7)) << 16) | (curr & 0xfff8ffff));
}
static __inline uint8_t acamera_isp_isp_global_isp_monitor_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x14L) & 0x70000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: interline_blanks_min
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Minimun H-blank. The frame monitor will checke the frame geometry against this value
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERLINE_BLANKS_MIN_DEFAULT (0x20)
#define ACAMERA_ISP_ISP_GLOBAL_INTERLINE_BLANKS_MIN_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_INTERLINE_BLANKS_MIN_OFFSET (0x18)
#define ACAMERA_ISP_ISP_GLOBAL_INTERLINE_BLANKS_MIN_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_isp_global_interline_blanks_min_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x18L);
    system_hw_write_32(0x18L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_isp_global_interline_blanks_min_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x18L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: interframe_blanks_min
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Minimun V-blank. The frame monitor will checke the frame geometry against this value
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERFRAME_BLANKS_MIN_DEFAULT (0x28)
#define ACAMERA_ISP_ISP_GLOBAL_INTERFRAME_BLANKS_MIN_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_INTERFRAME_BLANKS_MIN_OFFSET (0x18)
#define ACAMERA_ISP_ISP_GLOBAL_INTERFRAME_BLANKS_MIN_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_isp_isp_global_interframe_blanks_min_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x18L);
    system_hw_write_32(0x18L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_isp_isp_global_interframe_blanks_min_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x18L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: watchdog timer max count
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            Max count after which watchdog timer should give an interrupt. this count is between frame start and frame end
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_WATCHDOG_TIMER_MAX_COUNT_DEFAULT (0xFFFFFFFF)
#define ACAMERA_ISP_ISP_GLOBAL_WATCHDOG_TIMER_MAX_COUNT_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_WATCHDOG_TIMER_MAX_COUNT_OFFSET (0x1c)
#define ACAMERA_ISP_ISP_GLOBAL_WATCHDOG_TIMER_MAX_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_isp_isp_global_watchdog_timer_max_count_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x1cL, data);
}
static __inline uint32_t acamera_isp_isp_global_watchdog_timer_max_count_read(uintptr_t base) {
    return system_hw_read_32(0x1cL);
}
// ------------------------------------------------------------------------------ //
// Register: Mcu override config select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            mcu override config select. When this bit is set, MCU takes control of the ISP ping-pong config swap.
//            when is signal is set to 1, ISP core works in slave mode and selects configuration space based on MCU instruction.
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MCU_OVERRIDE_CONFIG_SELECT_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_OVERRIDE_CONFIG_SELECT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_OVERRIDE_CONFIG_SELECT_OFFSET (0x20)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_OVERRIDE_CONFIG_SELECT_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_mcu_override_config_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20L);
    system_hw_write_32(0x20L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_mcu_override_config_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Mcu ping pong config select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//                This signal is valid when Mcu_override_config_select is set to 1.
//                when mcu takes control of the config select, this signal indicated whether to use ping and pong config space
//                If this signal is changed during active active frame, the hardware makes sure that the config space is changed 
//                in the next vertical blanking
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MCU_PING_PONG_CONFIG_SELECT_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_PING_PONG_CONFIG_SELECT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_PING_PONG_CONFIG_SELECT_OFFSET (0x20)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_PING_PONG_CONFIG_SELECT_MASK (0x2)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_PING_PONG_CONFIG_SELECT_USE_PONG_ADDRESS_SPACE (0)
#define ACAMERA_ISP_ISP_GLOBAL_MCU_PING_PONG_CONFIG_SELECT_USE_PING_ADDRESS_SPACE (1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_mcu_ping_pong_config_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20L);
    system_hw_write_32(0x20L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_isp_global_mcu_ping_pong_config_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: multi context mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Multi-context control mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MULTI_CONTEXT_MODE_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MULTI_CONTEXT_MODE_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MULTI_CONTEXT_MODE_OFFSET (0x20)
#define ACAMERA_ISP_ISP_GLOBAL_MULTI_CONTEXT_MODE_MASK (0x100)
#define ACAMERA_ISP_ISP_GLOBAL_MULTI_CONTEXT_MODE_DEFAULT_MODE_THIS_IS_FOR_SINGLE_CONTEXT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MULTI_CONTEXT_MODE_MULTICONTEXT_MODE (1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_multi_context_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20L);
    system_hw_write_32(0x20L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_isp_isp_global_multi_context_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: ping locked
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// context swap status. when a address space is locked, all write to that address space will be rejected internally
//                       This signal is set when the 1st pixel comes out of input port and gets cleared when the last pixel comes out of ISp in streaming channels
//            1: ping locked
//            0: ping free
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PING_LOCKED_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_PING_LOCKED_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PING_LOCKED_OFFSET (0x24)
#define ACAMERA_ISP_ISP_GLOBAL_PING_LOCKED_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_ping_locked_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x24L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: pong locked
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// context swap status. when a address space is locked, all write to that address space will be rejected internally.
//                       This signal is set when the 1st pixel comes out of input port and gets cleared when the last pixel comes out of ISp in streaming channels
//            1: pong locked
//            0: pong free
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PONG_LOCKED_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_PONG_LOCKED_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PONG_LOCKED_OFFSET (0x24)
#define ACAMERA_ISP_ISP_GLOBAL_PONG_LOCKED_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_pong_locked_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x24L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: ping pong config select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            this signal indicates which of the PING/PONG config is being used by ISP. when MCU takes control of the config management through
//            Mcu_override_config_select signal, then this signal is just a reflection of what MCU has instructed through Mcu_ping_pong_config_select
//            signal. 
//
//            This signal is a good point to synchronize with hardware. MCU should read this signal in a regular interval to synchronize with its
//            internal state.
//
//            This signal is changed when the 1st pixel comes in. So this signal must be sampled at the frame_start interrupt.
//
//            0: pong in use by ISP
//            1: ping in use by ISP
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PING_PONG_CONFIG_SELECT_DEFAULT (1)
#define ACAMERA_ISP_ISP_GLOBAL_PING_PONG_CONFIG_SELECT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PING_PONG_CONFIG_SELECT_OFFSET (0x24)
#define ACAMERA_ISP_ISP_GLOBAL_PING_PONG_CONFIG_SELECT_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_ping_pong_config_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x24L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Group: isp global metering base addr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// pragrammable base adressed for metering stats
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: AWB
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// base address for AWB stats. Value is set for 33x33 max zones
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AWB_DEFAULT (2192)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AWB_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AWB_OFFSET (0x28)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AWB_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_isp_global_metering_base_addr_awb_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x28L);
    system_hw_write_32(0x28L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_isp_global_metering_base_addr_awb_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x28L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: AF
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// base address for AF stats. Value is set for 33x33 max zones
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AF_DEFAULT (4384)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AF_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AF_OFFSET (0x28)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_AF_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_isp_isp_global_metering_base_addr_af_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x28L);
    system_hw_write_32(0x28L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_isp_isp_global_metering_base_addr_af_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x28L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: MAX ADDR
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// max address for metering stats mem. Value is set for 33x33 max zones
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_MAX_ADDR_DEFAULT (6575)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_MAX_ADDR_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_MAX_ADDR_OFFSET (0x2c)
#define ACAMERA_ISP_ISP_GLOBAL_METERING_BASE_ADDR_MAX_ADDR_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_isp_global_metering_base_addr_max_addr_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2cL);
    system_hw_write_32(0x2cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_isp_global_metering_base_addr_max_addr_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: isp global interrupt
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// interrupt controls
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: mask_vector
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Interrupt mask vector
//        0: Interrupt is enabled
//        1: interrupt is masked
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_MASK_VECTOR_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_MASK_VECTOR_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_MASK_VECTOR_OFFSET (0x30)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_MASK_VECTOR_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_isp_isp_global_interrupt_mask_vector_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x30L, data);
}
static __inline uint32_t acamera_isp_isp_global_interrupt_mask_vector_read(uintptr_t base) {
    return system_hw_read_32(0x30L);
}
// ------------------------------------------------------------------------------ //
// Register: clear_vector
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Interrupt clear vector. its bitwise cleared. When a bit is set to 1. that interrupt
//        bit in the status register is cleared
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_VECTOR_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_VECTOR_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_VECTOR_OFFSET (0x34)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_VECTOR_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_isp_isp_global_interrupt_clear_vector_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x34L, data);
}
static __inline uint32_t acamera_isp_isp_global_interrupt_clear_vector_read(uintptr_t base) {
    return system_hw_read_32(0x34L);
}
// ------------------------------------------------------------------------------ //
// Register: shadow_disable_vector
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Interrupt shadow disable vector. 
//        To disable shadow feature for the specific interrupt event by setting the related bit to 1.
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_SHADOW_DISABLE_VECTOR_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_SHADOW_DISABLE_VECTOR_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_SHADOW_DISABLE_VECTOR_OFFSET (0x38)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_SHADOW_DISABLE_VECTOR_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_isp_isp_global_interrupt_shadow_disable_vector_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x38L, data);
}
static __inline uint32_t acamera_isp_isp_global_interrupt_shadow_disable_vector_read(uintptr_t base) {
    return system_hw_read_32(0x38L);
}
// ------------------------------------------------------------------------------ //
// Register: pulse_mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        When set to 1, the output interrupt will be a pulse. Otherwise it should be level.
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_PULSE_MODE_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_PULSE_MODE_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_PULSE_MODE_OFFSET (0x3c)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_PULSE_MODE_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_interrupt_pulse_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x3cL);
    system_hw_write_32(0x3cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_interrupt_pulse_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x3cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: clear
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        Interrupt clear vector register qualifier. First the vector must be written. Then this bit must be set to 1 and then cleared
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_OFFSET (0x40)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_CLEAR_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_interrupt_clear_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x40L);
    system_hw_write_32(0x40L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_interrupt_clear_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x40L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: status_vector
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            bit[0]  : Isp_start 
//            bit[1]  : Isp_done
//            bit[2]  : Context management error
//            bit[3]  : Broken frame error
//            bit[4]  : Metering AF done
//            bit[5]  : Metering aexp done
//            bit[6]  : Metering awb done
//            bit[7]  : Metering aexp 1024 bin hist done
//            bit[8]  : Metering antifog hist done
//            bit[9]  : Lut init done
//            bit[10] : FR y-dma write done
//            bit[11] : FR uv-dma write done
//            bit[12] : DS y-dma write done
//            bit[13] : DS uv-dma write done
//            bit[14] : Linearization done
//            bit[15] : Static dpc done
//            bit[16] : Ca correction done
//            bit[17] : Iridix done
//            bit[18] : 3d LIUT done
//            bit[19] : Watchdog timer timed out
//            bit[20] : Frame collision error
//            bit[21] : luma variance done
//            bit[22] : DMA error interrupt
//            bit[23] : Input port safely stopped
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_STATUS_VECTOR_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_STATUS_VECTOR_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_STATUS_VECTOR_OFFSET (0x44)
#define ACAMERA_ISP_ISP_GLOBAL_INTERRUPT_STATUS_VECTOR_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_isp_global_interrupt_status_vector_read(uintptr_t base) {
    return system_hw_read_32(0x44L);
}
// ------------------------------------------------------------------------------ //
// Group: isp global lp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    CLK DIS register: 
//        This register shitches off clock of certain blocks. 
//
//
//    CLK DIS register:
//        This register controls the clock gate cell which gates clock during V-blank. When this is set to 1, the v-blank clock
//        gating will be disabled. This register doesnt control the static clock gating which is controlled through CLK DIS register
//
//    
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: CLK Dis FR Y dma writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            When set, the output Y/RGB/YUV DMA writer in FR channel will be clock gated. This is applicable
//            to Y channel of the semi-planner mode and all other non-planner modes. This must be used only when the DMA writer is not used and SOC
//            environment uses the streaming outputs from ISP
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_Y_DMA_WRITER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_Y_DMA_WRITER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_Y_DMA_WRITER_OFFSET (0x48)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_Y_DMA_WRITER_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_clk_dis_fr_y_dma_writer_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x48L);
    system_hw_write_32(0x48L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_lp_clk_dis_fr_y_dma_writer_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x48L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: CLK Dis FR UV dma writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            When set, the output UV DMA writer in FR channel will be clock gated. This is applicable
//            to UV channel of the semi-planner mode . This must be used only when the DMA writer is not used and SOC
//            environment uses the streaming outputs from ISP, or format is NOT semi-planner
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_UV_DMA_WRITER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_UV_DMA_WRITER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_UV_DMA_WRITER_OFFSET (0x48)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_FR_UV_DMA_WRITER_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_clk_dis_fr_uv_dma_writer_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x48L);
    system_hw_write_32(0x48L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_isp_global_lp_clk_dis_fr_uv_dma_writer_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x48L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: CLK Dis DS Y dma writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            When set, the output Y/RGB/YUV DMA writer in DS channel will be clock gated. This is applicable
//            to Y channel of the semi-planner mode and all other non-planner modes. This must be used only when the DMA writer is not used and SOC
//            environment uses the streaming outputs from ISP
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_Y_DMA_WRITER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_Y_DMA_WRITER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_Y_DMA_WRITER_OFFSET (0x48)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_Y_DMA_WRITER_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_clk_dis_ds1_y_dma_writer_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x48L);
    system_hw_write_32(0x48L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_isp_isp_global_lp_clk_dis_ds1_y_dma_writer_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x48L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: CLK Dis DS UV dma writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            When set, the output UV DMA writer in DS channel will be clock gated. This is applicable
//            to UV channel of the semi-planner mode . This must be used only when the DMA writer is not used and SOC
//            environment uses the streaming outputs from ISP, or format is NOT semi-planner
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_UV_DMA_WRITER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_UV_DMA_WRITER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_UV_DMA_WRITER_OFFSET (0x48)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_DS1_UV_DMA_WRITER_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_clk_dis_ds1_uv_dma_writer_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x48L);
    system_hw_write_32(0x48L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_isp_isp_global_lp_clk_dis_ds1_uv_dma_writer_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x48L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: CLK Dis temper dma
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            This signal gates the clock for all temper write and read dma. This should be used only when Temper is not used.
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_TEMPER_DMA_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_TEMPER_DMA_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_TEMPER_DMA_OFFSET (0x48)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CLK_DIS_TEMPER_DMA_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_clk_dis_temper_dma_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x48L);
    system_hw_write_32(0x48L, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_isp_isp_global_lp_clk_dis_temper_dma_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x48L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis frame_stitch
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for WDR path channels.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FRAME_STITCH_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FRAME_STITCH_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FRAME_STITCH_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FRAME_STITCH_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_frame_stitch_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_frame_stitch_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis raw frontend
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for raw frontend.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RAW_FRONTEND_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RAW_FRONTEND_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RAW_FRONTEND_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RAW_FRONTEND_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_raw_frontend_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_raw_frontend_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis defect pixel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for defect pixel.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEFECT_PIXEL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEFECT_PIXEL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEFECT_PIXEL_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEFECT_PIXEL_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_defect_pixel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_defect_pixel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis Sinter
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for Sinter.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SINTER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SINTER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SINTER_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SINTER_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_sinter_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_sinter_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis Temper
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for Temper.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_TEMPER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_TEMPER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_TEMPER_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_TEMPER_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_temper_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_temper_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis CA Correction
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for CA correction.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CA_CORRECTION_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CA_CORRECTION_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CA_CORRECTION_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CA_CORRECTION_MASK (0x20)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_ca_correction_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 5) | (curr & 0xffffffdf));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_ca_correction_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis radial shading
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for radial shading.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RADIAL_SHADING_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RADIAL_SHADING_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RADIAL_SHADING_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RADIAL_SHADING_MASK (0x40)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_radial_shading_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 6) | (curr & 0xffffffbf));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_radial_shading_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis mesh shading
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for mesh shading.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_MESH_SHADING_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_MESH_SHADING_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_MESH_SHADING_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_MESH_SHADING_MASK (0x80)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_mesh_shading_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_mesh_shading_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis Iridix
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for Iridix.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_IRIDIX_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_IRIDIX_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_IRIDIX_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_IRIDIX_MASK (0x100)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_iridix_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_iridix_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis demosaic RGGB
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for Demosaic RGGB.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGGB_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGGB_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGGB_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGGB_MASK (0x200)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_demosaic_rggb_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_demosaic_rggb_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis demosaic RGBIr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for Demosaic RGBIr.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGBIR_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGBIR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGBIR_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_DEMOSAIC_RGBIR_MASK (0x400)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_demosaic_rgbir_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_demosaic_rgbir_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis PF correction
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for PF correction.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_PF_CORRECTION_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_PF_CORRECTION_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_PF_CORRECTION_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_PF_CORRECTION_MASK (0x800)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_pf_correction_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_pf_correction_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis CNR
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for CNR and pre-square root and post-square.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CNR_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CNR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CNR_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_CNR_MASK (0x1000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_cnr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_cnr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis 3D LUT
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for 3D LUT.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_3D_LUT_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_3D_LUT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_3D_LUT_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_3D_LUT_MASK (0x2000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_3d_lut_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 13) | (curr & 0xffffdfff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_3d_lut_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x2000) >> 13);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis RGB Scaler
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for RGB scaler.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_SCALER_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_SCALER_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_SCALER_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_SCALER_MASK (0x4000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_rgb_scaler_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_rgb_scaler_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis RGB Gamma FR
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for RGB gamma in FR pipeline.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_FR_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_FR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_FR_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_FR_MASK (0x8000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_rgb_gamma_fr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_rgb_gamma_fr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis RGB Gamma DS
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for RGB gamma in DS pipeline.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_DS_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_DS_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_DS_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_RGB_GAMMA_DS_MASK (0x10000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_rgb_gamma_ds_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_rgb_gamma_ds_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis Sharpen FR
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for sharpen in FR pipeline.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FR_SHARPEN_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FR_SHARPEN_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FR_SHARPEN_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_FR_SHARPEN_MASK (0x20000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_fr_sharpen_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_fr_sharpen_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: CG Dis Sharpen DS
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// when set, this will disable V-blank Clock gating for sharpen in DS pipeline.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SHARPEN_DS_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SHARPEN_DS_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SHARPEN_DS_OFFSET (0x4c)
#define ACAMERA_ISP_ISP_GLOBAL_LP_CG_DIS_SHARPEN_DS_MASK (0x40000)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_lp_cg_dis_sharpen_ds_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x4cL);
    system_hw_write_32(0x4cL, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_isp_isp_global_lp_cg_dis_sharpen_ds_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x4cL) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Group: isp global monitor
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: broken_frame status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            bit[0] : active width mismatch
//            bit[1] : active_height mismatch
//            bit[2] : minimum v-blank violated
//            bit[3] : minimum h-blank violated
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_STATUS_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_STATUS_DATASIZE (4)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_STATUS_OFFSET (0x50)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_STATUS_MASK (0xf)

// args: data (4-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_broken_frame_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x50L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: fr_pipeline_busy
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            This signal indicates if ISP pipeline is busy or Not, This signal doesnt include the metereing modules.
//            This information is expected to be used when a broken frame is detected and global_fsm_reset needs to be asserted
//            global_fsm_reset must be set when this busy signal is low.
//                0: ISP main pipeline (excluding metering) is free
//                1: ISP main pipeline (excluding metering) is busy
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_PIPELINE_BUSY_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_PIPELINE_BUSY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_PIPELINE_BUSY_OFFSET (0x50)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_PIPELINE_BUSY_MASK (0x10000)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_fr_pipeline_busy_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x50L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: FR Y DMA WFIFO fail full
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_FULL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_FULL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_FULL_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_FULL_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_fr_y_dma_wfifo_fail_full_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: FR Y DMA WFIFO fail empty
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_EMPTY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_EMPTY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_EMPTY_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_Y_DMA_WFIFO_FAIL_EMPTY_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_fr_y_dma_wfifo_fail_empty_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: FR UV DMA WFIFO fail full
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_FULL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_FULL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_FULL_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_FULL_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_fr_uv_dma_wfifo_fail_full_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: FR UV DMA WFIFO fail empty
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_EMPTY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_EMPTY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_EMPTY_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_FR_UV_DMA_WFIFO_FAIL_EMPTY_MASK (0x8)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_fr_uv_dma_wfifo_fail_empty_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: DS Y DMA WFIFO fail full
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_FULL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_FULL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_FULL_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_FULL_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_ds1_y_dma_wfifo_fail_full_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: DS Y DMA WFIFO fail empty
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_EMPTY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_EMPTY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_EMPTY_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_Y_DMA_WFIFO_FAIL_EMPTY_MASK (0x20)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_ds1_y_dma_wfifo_fail_empty_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: DS UV DMA WFIFO fail full
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_FULL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_FULL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_FULL_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_FULL_MASK (0x40)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_ds1_uv_dma_wfifo_fail_full_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: DS UV DMA WFIFO fail empty
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_EMPTY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_EMPTY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_EMPTY_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DS1_UV_DMA_WFIFO_FAIL_EMPTY_MASK (0x80)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_ds1_uv_dma_wfifo_fail_empty_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: Temper DMA WFIFO fail empty
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_EMPTY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_EMPTY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_EMPTY_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_EMPTY_MASK (0x100)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_temper_dma_wfifo_fail_empty_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: Temper DMA WFIFO fail full
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_FULL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_FULL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_FULL_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_WFIFO_FAIL_FULL_MASK (0x200)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_temper_dma_wfifo_fail_full_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: Temper DMA RFIFO fail empty
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_EMPTY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_EMPTY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_EMPTY_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_EMPTY_MASK (0x400)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_temper_dma_rfifo_fail_empty_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: Temper DMA RFIFO fail full
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_FULL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_FULL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_FULL_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_RFIFO_FAIL_FULL_MASK (0x800)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_monitor_temper_dma_rfifo_fail_full_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x54L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: DMA alarms
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            [0] : temper_dma_lsb_wtr_axi_alarm
//            [1] : temper_dma_lsb_rdr_axi_alarm
//            [2] : temper_dma_msb_wtr_axi_alarm
//            [3] : temper_dma_msb_rdr_axi_alarm
//            [4] : FR UV dma axi alarm
//            [5] : FR dma axi alarm
//            [6] : DS UV dma axi alarm
//            [7] : DS dma axi alarm
//            [8] : Temper LSB dma frame dropped
//            [9] : Temper MSB dma frame dropped
//            [10]: FR UV-DMA frame dropped
//            [11]: FR Y-DMA frame dropped
//            [12]: DS UV-DMA frame dropped
//            [13]: DS Y-DMA frame dropped
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DMA_ALARMS_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DMA_ALARMS_DATASIZE (14)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DMA_ALARMS_OFFSET (0x54)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_DMA_ALARMS_MASK (0x3fff0000)

// args: data (14-bit)
static __inline uint16_t acamera_isp_isp_global_monitor_dma_alarms_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x54L) & 0x3fff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: broken_frame error clear
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            This signal must be asserted when the MCu gets broken frame interrupt.
//            this signal must be 0->1->0 pulse. The duration of the pulse is not relevant. This rising edge will clear the
//            broken frame error status signal
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_ERROR_CLEAR_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_ERROR_CLEAR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_ERROR_CLEAR_OFFSET (0x58)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_BROKEN_FRAME_ERROR_CLEAR_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_monitor_broken_frame_error_clear_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x58L);
    system_hw_write_32(0x58L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_monitor_broken_frame_error_clear_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x58L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: context error clr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            This signal must be asserted when the MCU gets the context error interrupt.
//            this signal must be 0->1->0 pulse. The duration of the pulse is not relevant. This rising edge will clear the
//            context error status signal
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_CONTEXT_ERROR_CLR_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_CONTEXT_ERROR_CLR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_CONTEXT_ERROR_CLR_OFFSET (0x58)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_CONTEXT_ERROR_CLR_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_monitor_context_error_clr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x58L);
    system_hw_write_32(0x58L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_isp_global_monitor_context_error_clr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x58L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: output dma clr alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            This signal must be asserted when the MCU gets the DMA error interrupt.
//            This signal will clear all DMA error status signals for all the output DMA writers (Y/UV DMAs in both of the output channels)
//            MCU must follow the following sequance to clear the alarms
//            step-1: set this bit to 1
//            step-2: Read back the alarm signals
//            step-3: If the alarms are cleared, then clear the clr_alarm signal back to 0.
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_OUTPUT_DMA_CLR_ALARM_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_OUTPUT_DMA_CLR_ALARM_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_OUTPUT_DMA_CLR_ALARM_OFFSET (0x58)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_OUTPUT_DMA_CLR_ALARM_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_monitor_output_dma_clr_alarm_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x58L);
    system_hw_write_32(0x58L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_isp_isp_global_monitor_output_dma_clr_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x58L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: temper dma clr alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            This signal must be asserted when the MCU gets the DMA error interrupt.
//            This signal will clear all DMA error status signals for all the Temper DMA writers/readers
//            MCU must follow the following sequance to clear the alarms
//            step-1: set this bit to 1
//            step-2: Read back the alarm signals
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_CLR_ALARM_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_CLR_ALARM_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_CLR_ALARM_OFFSET (0x58)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_TEMPER_DMA_CLR_ALARM_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_monitor_temper_dma_clr_alarm_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x58L);
    system_hw_write_32(0x58L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_isp_isp_global_monitor_temper_dma_clr_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x58L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: MAX address delay line fr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            Delay line max address value for the full resolution ISP set outside ISP
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_FR_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_FR_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_FR_OFFSET (0x60)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_FR_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_isp_isp_global_monitor_max_address_delay_line_fr_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x60L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: MAX address delay line ds
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            Delay line max address value for the DS pipeline set outside ISP
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_DS_DEFAULT (0x0)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_DS_DATASIZE (16)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_DS_OFFSET (0x60)
#define ACAMERA_ISP_ISP_GLOBAL_MONITOR_MAX_ADDRESS_DELAY_LINE_DS_MASK (0xffff0000)

// args: data (16-bit)
static __inline uint16_t acamera_isp_isp_global_monitor_max_address_delay_line_ds_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x60L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Group: isp global chicken bit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: frame_end_select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        0: Only ISP main pipeline, ds pipeline and iridix filtering is used to generate the frame_done interrupt
//           None of the metering done signals are considered here
//        1: all metering done is taken into account to generate the frame done interrupt
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_FRAME_END_SELECT_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_FRAME_END_SELECT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_FRAME_END_SELECT_OFFSET (0x64)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_FRAME_END_SELECT_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_chicken_bit_frame_end_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x64L);
    system_hw_write_32(0x64L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_chicken_bit_frame_end_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x64L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rd_start_sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        0: temper dma reader will start reading based on the frame start
//        1: temper dma reader will start reading based on linetick of the dma reader
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_RD_START_SEL_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_RD_START_SEL_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_RD_START_SEL_OFFSET (0x64)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_RD_START_SEL_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_chicken_bit_rd_start_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x64L);
    system_hw_write_32(0x64L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_isp_global_chicken_bit_rd_start_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x64L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: input_alignment
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        0=LSB aligned 
//        1=MSB aligned
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_INPUT_ALIGNMENT_DEFAULT (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_INPUT_ALIGNMENT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_INPUT_ALIGNMENT_OFFSET (0x64)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_INPUT_ALIGNMENT_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_chicken_bit_input_alignment_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x64L);
    system_hw_write_32(0x64L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_isp_isp_global_chicken_bit_input_alignment_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x64L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: watchdog_timer_dis
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        0=Watch dog timer enabled
//        1=watch dog timer disabled
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_WATCHDOG_TIMER_DIS_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_WATCHDOG_TIMER_DIS_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_WATCHDOG_TIMER_DIS_OFFSET (0x64)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_WATCHDOG_TIMER_DIS_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_chicken_bit_watchdog_timer_dis_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x64L);
    system_hw_write_32(0x64L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_isp_isp_global_chicken_bit_watchdog_timer_dis_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x64L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: soft_rst_apply_immediately
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        0 = When this chicken bit is et to 0, the soft reset (SW or HW generated) will be stretched by 1024 cycles
//            so that it is asserted till the the pipeline delay of internal acl signals.
//        1 = When thiss set to 1, the soft reset will not be stretched internally. The SW must make sure its
//            kept high enough.
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_SOFT_RST_APPLY_IMMEDIATELY_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_SOFT_RST_APPLY_IMMEDIATELY_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_SOFT_RST_APPLY_IMMEDIATELY_OFFSET (0x64)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_SOFT_RST_APPLY_IMMEDIATELY_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_chicken_bit_soft_rst_apply_immediately_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x64L);
    system_hw_write_32(0x64L, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_isp_isp_global_chicken_bit_soft_rst_apply_immediately_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x64L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer_timeout_disable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        0=timeout enabled. At the end of the frame, if the last data is not drained out from DMA writer within 4000
//          AXI clock, cycle, DMA will flush the FIFO and ignore the remainign data.
//
//        1=timeout is disabled. If the last data is not drained out and the next frame starts coming in, DMA will drop the next frame and
//          give an interrupt.
//          If the timeout is disabled, its S/W responsibility to cancel the frame in all dma engines if frame drop interrupt comes.
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_DMA_WRITER_TIMEOUT_DISABLE_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_DMA_WRITER_TIMEOUT_DISABLE_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_DMA_WRITER_TIMEOUT_DISABLE_OFFSET (0x64)
#define ACAMERA_ISP_ISP_GLOBAL_CHICKEN_BIT_DMA_WRITER_TIMEOUT_DISABLE_MASK (0x20)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_chicken_bit_dma_writer_timeout_disable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x64L);
    system_hw_write_32(0x64L, (((uint32_t) (data & 0x1)) << 5) | (curr & 0xffffffdf));
}
static __inline uint8_t acamera_isp_isp_global_chicken_bit_dma_writer_timeout_disable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x64L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Group: isp global parameter status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        ISP provides few CHICKEN_OUT parameters through which certain modules can be removed from ISP.
//        This reggister indicates the status of that CHICKEN BIT
//      
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: dmsc_rgbir
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            0: Demosaic RGBIr is present in the design
//            1: Demodaic RGBIr is statically removed from the ISP. 
//               S/W must not access the ISP_CONFIG_PING:demosaic rgbir and the ISP_CONFIG_PONG:demosaic rgbir registers
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DMSC_RGBIR_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DMSC_RGBIR_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DMSC_RGBIR_OFFSET (0x68)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DMSC_RGBIR_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_parameter_status_dmsc_rgbir_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x68L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: cac
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            0: CA Correction is present in the design
//            1: CA Correction is statically removed from the ISP. 
//                S/W must not access the ISP_CONFIG_PING:ca correction and the ISP_CONFIG_PONG:ca correction registers
//
//                Also SW must not access the following memories
//                    -	CA_CORRECTION_FILTER_PING_MEM
//                    -	CA_CORRECTION_FILTER_PONG_MEM
//                    -	CA_CORRECTION_MESH_PING_MEM
//                    -	CA_CORRECTION_MESH_PONG_MEM
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_CAC_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_CAC_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_CAC_OFFSET (0x68)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_CAC_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_parameter_status_cac_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x68L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: ds_pipe
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            0: DS pipeline is present in the design
//            1: DS pipeline is statically removed from ISP. 
//                S/W must not access the following register spaces
//                -	ISP_CONFIG_PING:ds crop and ISP_CONFIG_PONG:ds crop
//                -	ISP_CONFIG_PING:ds scaler and ISP_CONFIG_PONG:ds scaler
//                -	ISP_CONFIG_PING:ds gamma rgb and ISP_CONFIG_PONG:ds gamma rgb
//                -	ISP_CONFIG_PING:ds sharpen and ISP_CONFIG_PONG:ds sharpen
//                -	ISP_CONFIG_PING:ds cs conv and ISP_CONFIG_PONG:ds cs conv
//                -	ISP_CONFIG_PING:ds dma writer and ISP_CONFIG_PONG:ds dma writer
//                -	ISP_CONFIG_PING:ds uv dma writer and ISP_CONFIG_PONG:ds uv dma writer
//	
//                Also the S/W must not access the following memories
//                -	DS_SCALER_HFILT_COEFMEM
//                -	DS_SCALER_VFILT_COEFMEM
//                -	DS_GAMMA_RGB_PING_MEM
//                -	DS_GAMMA_RGB_PONG_MEM
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DS1_PIPE_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DS1_PIPE_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DS1_PIPE_OFFSET (0x68)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_DS1_PIPE_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_parameter_status_ds1_pipe_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x68L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: LUT 3d
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            0: sRGB gamma and 3D LUT is present in the design
//            1: sRGB gamma and 3D LUT is statically removed from ISP. 
//                The S/W must not accees the following register spaces
//                -   ISP_CONFIG_PING:nonequ gamma and ISP_CONFIG_PING: nonequ gamma and
//
//                Also, the S/W must not access the following memories
//                -   LUT3D_MEM
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_LUT_3D_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_LUT_3D_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_LUT_3D_OFFSET (0x68)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_LUT_3D_MASK (0x8)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_parameter_status_lut_3d_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x68L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: sinter_version
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            0: SINTER2.5 used
//            1: SINTER3 used
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_SINTER_VERSION_DEFAULT (1)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_SINTER_VERSION_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_SINTER_VERSION_OFFSET (0x68)
#define ACAMERA_ISP_ISP_GLOBAL_PARAMETER_STATUS_SINTER_VERSION_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_isp_isp_global_parameter_status_sinter_version_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x68L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Group: isp global dbg
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: mode_en
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            0: debug signals are disabled
//            1: debug signals are valid
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DBG_MODE_EN_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_MODE_EN_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_MODE_EN_OFFSET (0x6c)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_MODE_EN_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_dbg_mode_en_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x6cL);
    system_hw_write_32(0x6cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_isp_global_dbg_mode_en_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x6cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: clear_frame_cnt
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//            must be 0->1->0 to clear the debug counters
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DBG_CLEAR_FRAME_CNT_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_CLEAR_FRAME_CNT_DATASIZE (1)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_CLEAR_FRAME_CNT_OFFSET (0x6c)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_CLEAR_FRAME_CNT_MASK (0x100)

// args: data (1-bit)
static __inline void acamera_isp_isp_global_dbg_clear_frame_cnt_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x6cL);
    system_hw_write_32(0x6cL, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_isp_isp_global_dbg_clear_frame_cnt_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x6cL) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: frame_cnt_ctx0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//                when debug mode is enabled, this register will show the frame count in context-0
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX0_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX0_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX0_OFFSET (0x70)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX0_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_isp_global_dbg_frame_cnt_ctx0_read(uintptr_t base) {
    return system_hw_read_32(0x70L);
}
// ------------------------------------------------------------------------------ //
// Register: frame_cnt_ctx1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//                when debug mode is enabled, this register will show the frame count in context-1
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX1_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX1_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX1_OFFSET (0x74)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX1_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_isp_global_dbg_frame_cnt_ctx1_read(uintptr_t base) {
    return system_hw_read_32(0x74L);
}
// ------------------------------------------------------------------------------ //
// Register: frame_cnt_ctx2
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//                when debug mode is enabled, this register will show the frame count in context-2
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX2_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX2_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX2_OFFSET (0x78)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX2_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_isp_global_dbg_frame_cnt_ctx2_read(uintptr_t base) {
    return system_hw_read_32(0x78L);
}
// ------------------------------------------------------------------------------ //
// Register: frame_cnt_ctx3
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//                when debug mode is enabled, this register will show the frame count in context-3
//          
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX3_DEFAULT (0)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX3_DATASIZE (32)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX3_OFFSET (0x7c)
#define ACAMERA_ISP_ISP_GLOBAL_DBG_FRAME_CNT_CTX3_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_isp_global_dbg_frame_cnt_ctx3_read(uintptr_t base) {
    return system_hw_read_32(0x7cL);
}
// ------------------------------------------------------------------------------ //
// Group: input port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Controls video input port.  
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: preset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//       Allows selection of various input port presets for standard sensor inputs.  See ISP Guide for details of available presets.
//      	2: preset mode 2, check ISP guide for details 
//      	6: preset mode 6, check ISP guide for details
//      	others: reserved
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_PRESET_DEFAULT (2)
#define ACAMERA_ISP_INPUT_PORT_PRESET_DATASIZE (4)
#define ACAMERA_ISP_INPUT_PORT_PRESET_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_PRESET_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_isp_input_port_preset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_isp_input_port_preset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vs_use field
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VS_USE_FIELD_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VS_USE_FIELD_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_VS_USE_FIELD_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_VS_USE_FIELD_MASK (0x100)
#define ACAMERA_ISP_INPUT_PORT_VS_USE_FIELD_USE_VSYNC_I_PORT_FOR_VERTICAL_SYNC (0)
#define ACAMERA_ISP_INPUT_PORT_VS_USE_FIELD_USE_FIELD_I_PORT_FOR_VERTICAL_SYNC (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_vs_use_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_isp_input_port_vs_use_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: vs toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VS_TOGGLE_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VS_TOGGLE_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_VS_TOGGLE_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_VS_TOGGLE_MASK (0x200)
#define ACAMERA_ISP_INPUT_PORT_VS_TOGGLE_VSYNC_IS_PULSETYPE (0)
#define ACAMERA_ISP_INPUT_PORT_VS_TOGGLE_VSYNC_IS_TOGGLETYPE_FIELD_SIGNAL (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_vs_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_isp_input_port_vs_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: vs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_MASK (0x400)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_RISING_EDGE (0)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_FALLING_EDGE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_vs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_isp_input_port_vs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: vs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_ACL_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_ACL_MASK (0x800)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_ACL_DONT_INVERT_POLARITY_FOR_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_VS_POLARITY_ACL_INVERT_POLARITY_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_vs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_isp_input_port_vs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: hs_use acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HS_USE_ACL_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HS_USE_ACL_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HS_USE_ACL_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HS_USE_ACL_MASK (0x1000)
#define ACAMERA_ISP_INPUT_PORT_HS_USE_ACL_USE_HSYNC_I_PORT_FOR_ACTIVELINE (0)
#define ACAMERA_ISP_INPUT_PORT_HS_USE_ACL_USE_ACL_I_PORT_FOR_ACTIVELINE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hs_use_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_isp_input_port_hs_use_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: vc_c select
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VC_C_SELECT_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VC_C_SELECT_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_VC_C_SELECT_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_VC_C_SELECT_MASK (0x4000)
#define ACAMERA_ISP_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HS (0)
#define ACAMERA_ISP_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HORIZONTAL_COUNTER_OVERFLOW_OR_RESET (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_vc_c_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_isp_input_port_vc_c_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: vc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VC_R_SELECT_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VC_R_SELECT_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_VC_R_SELECT_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_VC_R_SELECT_MASK (0x8000)
#define ACAMERA_ISP_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_EDGE_OF_VS (0)
#define ACAMERA_ISP_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_AFTER_TIMEOUT_ON_HS (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_vc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_isp_input_port_vc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: hs_xor vs
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HS_XOR_VS_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HS_XOR_VS_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HS_XOR_VS_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HS_XOR_VS_MASK (0x10000)
#define ACAMERA_ISP_INPUT_PORT_HS_XOR_VS_NORMAL_MODE (0)
#define ACAMERA_ISP_INPUT_PORT_HS_XOR_VS_HVALID__HSYNC_XOR_VSYNC (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hs_xor_vs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_isp_input_port_hs_xor_vs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_MASK (0x20000)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_DONT_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_isp_input_port_hs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_ACL_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_ACL_MASK (0x40000)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_ACL_DONT_INVERT_POLARITY_OF_HS_FOR_HS_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_ACL_INVERT_POLARITY_OF_HS_FOR_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_isp_input_port_hs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity hs
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_HS_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_HS_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_HS_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_HS_MASK (0x80000)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_VSYNC_EG_WHEN_HSYNC_IS_NOT_AVAILABLE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hs_polarity_hs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_isp_input_port_hs_polarity_hs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity vc
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_VC_DEFAULT (1)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_VC_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_VC_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_VC_MASK (0x100000)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_ISP_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hs_polarity_vc_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 20) | (curr & 0xffefffff));
}
static __inline uint8_t acamera_isp_input_port_hs_polarity_vc_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x100000) >> 20);
}
// ------------------------------------------------------------------------------ //
// Register: hc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HC_R_SELECT_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HC_R_SELECT_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HC_R_SELECT_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_HC_R_SELECT_MASK (0x800000)
#define ACAMERA_ISP_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_ISP_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_VS (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 23) | (curr & 0xff7fffff));
}
static __inline uint8_t acamera_isp_input_port_hc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x800000) >> 23);
}
// ------------------------------------------------------------------------------ //
// Register: acl polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_ACL_POLARITY_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_ACL_POLARITY_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_ACL_POLARITY_OFFSET (0x80)
#define ACAMERA_ISP_INPUT_PORT_ACL_POLARITY_MASK (0x1000000)
#define ACAMERA_ISP_INPUT_PORT_ACL_POLARITY_DONT_INVERT_ACL_I_FOR_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_ACL_POLARITY_INVERT_ACL_I_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_acl_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x80L);
    system_hw_write_32(0x80L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_isp_input_port_acl_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x80L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: field polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FIELD_POLARITY_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FIELD_POLARITY_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FIELD_POLARITY_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_FIELD_POLARITY_MASK (0x1)
#define ACAMERA_ISP_INPUT_PORT_FIELD_POLARITY_DONT_INVERT_FIELD_I_FOR_FIELD_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_FIELD_POLARITY_INVERT_FIELD_I_FOR_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_field_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_input_port_field_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: field toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FIELD_TOGGLE_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FIELD_TOGGLE_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FIELD_TOGGLE_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_FIELD_TOGGLE_MASK (0x2)
#define ACAMERA_ISP_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_PULSETYPE (0)
#define ACAMERA_ISP_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_TOGGLETYPE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_field_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_isp_input_port_field_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window0
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW0_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW0_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW0_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW0_MASK (0x100)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW0_EXCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW0_INCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_aclg_window0_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_isp_input_port_aclg_window0_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: aclg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_ACLG_HSYNC_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_HSYNC_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_ACLG_HSYNC_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_ACLG_HSYNC_MASK (0x200)
#define ACAMERA_ISP_INPUT_PORT_ACLG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_aclg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_isp_input_port_aclg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW2_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW2_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW2_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW2_MASK (0x400)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_aclg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_isp_input_port_aclg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: aclg acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_ACLG_ACL_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_ACL_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_ACLG_ACL_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_ACLG_ACL_MASK (0x800)
#define ACAMERA_ISP_INPUT_PORT_ACLG_ACL_EXCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_ACL_INCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_aclg_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_isp_input_port_aclg_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: aclg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_ACLG_VSYNC_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_VSYNC_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_ACLG_VSYNC_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_ACLG_VSYNC_MASK (0x1000)
#define ACAMERA_ISP_INPUT_PORT_ACLG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_ACLG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_aclg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_isp_input_port_aclg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window1
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW1_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW1_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW1_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW1_MASK (0x10000)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW1_EXCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW1_INCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hsg_window1_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_isp_input_port_hsg_window1_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hsg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HSG_HSYNC_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_HSYNC_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HSG_HSYNC_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_HSG_HSYNC_MASK (0x20000)
#define ACAMERA_ISP_INPUT_PORT_HSG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hsg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_isp_input_port_hsg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hsg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HSG_VSYNC_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_VSYNC_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HSG_VSYNC_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_HSG_VSYNC_MASK (0x40000)
#define ACAMERA_ISP_INPUT_PORT_HSG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hsg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_isp_input_port_hsg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW2_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW2_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW2_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW2_MASK (0x80000)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_HSG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_HS_GATE_MASK_OUT_SPURIOUS_HS_DURING_BLANK (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_hsg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_isp_input_port_hsg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FIELDG_VSYNC_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_VSYNC_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_VSYNC_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_VSYNC_MASK (0x1000000)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_fieldg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_isp_input_port_fieldg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FIELDG_WINDOW2_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_WINDOW2_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_WINDOW2_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_WINDOW2_MASK (0x2000000)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_fieldg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 25) | (curr & 0xfdffffff));
}
static __inline uint8_t acamera_isp_input_port_fieldg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x2000000) >> 25);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg field
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FIELDG_FIELD_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_FIELD_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_FIELD_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_FIELD_MASK (0x4000000)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_FIELD_EXCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_ISP_INPUT_PORT_FIELDG_FIELD_INCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_fieldg_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 26) | (curr & 0xfbffffff));
}
static __inline uint8_t acamera_isp_input_port_fieldg_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x4000000) >> 26);
}
// ------------------------------------------------------------------------------ //
// Register: field mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FIELD_MODE_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FIELD_MODE_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FIELD_MODE_OFFSET (0x84)
#define ACAMERA_ISP_INPUT_PORT_FIELD_MODE_MASK (0x8000000)
#define ACAMERA_ISP_INPUT_PORT_FIELD_MODE_PULSE_FIELD (0)
#define ACAMERA_ISP_INPUT_PORT_FIELD_MODE_TOGGLE_FIELD (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_field_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x84L);
    system_hw_write_32(0x84L, (((uint32_t) (data & 0x1)) << 27) | (curr & 0xf7ffffff));
}
static __inline uint8_t acamera_isp_input_port_field_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x84L) & 0x8000000) >> 27);
}
// ------------------------------------------------------------------------------ //
// Register: hc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// horizontal counter limit value (counts: 0,1,...hc_limit-1,hc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_ISP_INPUT_PORT_HC_LIMIT_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_HC_LIMIT_OFFSET (0x88)
#define ACAMERA_ISP_INPUT_PORT_HC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_input_port_hc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x88L);
    system_hw_write_32(0x88L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_input_port_hc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x88L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HC_START0_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HC_START0_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_HC_START0_OFFSET (0x88)
#define ACAMERA_ISP_INPUT_PORT_HC_START0_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_isp_input_port_hc_start0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x88L);
    system_hw_write_32(0x88L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_isp_input_port_hc_start0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x88L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hc size0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HC_SIZE0_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HC_SIZE0_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_HC_SIZE0_OFFSET (0x8c)
#define ACAMERA_ISP_INPUT_PORT_HC_SIZE0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_input_port_hc_size0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x8cL);
    system_hw_write_32(0x8cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_input_port_hc_size0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x8cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 start for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HC_START1_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HC_START1_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_HC_START1_OFFSET (0x8c)
#define ACAMERA_ISP_INPUT_PORT_HC_START1_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_isp_input_port_hc_start1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x8cL);
    system_hw_write_32(0x8cL, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_isp_input_port_hc_start1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x8cL) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hc size1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 size for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_HC_SIZE1_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_HC_SIZE1_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_HC_SIZE1_OFFSET (0x90)
#define ACAMERA_ISP_INPUT_PORT_HC_SIZE1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_input_port_hc_size1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x90L);
    system_hw_write_32(0x90L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_input_port_hc_size1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x90L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// vertical counter limit value (counts: 0,1,...vc_limit-1,vc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_ISP_INPUT_PORT_VC_LIMIT_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_VC_LIMIT_OFFSET (0x90)
#define ACAMERA_ISP_INPUT_PORT_VC_LIMIT_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_isp_input_port_vc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x90L);
    system_hw_write_32(0x90L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_isp_input_port_vc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x90L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: vc start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VC_START_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VC_START_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_VC_START_OFFSET (0x94)
#define ACAMERA_ISP_INPUT_PORT_VC_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_isp_input_port_vc_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x94L);
    system_hw_write_32(0x94L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_isp_input_port_vc_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x94L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc size
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_VC_SIZE_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_VC_SIZE_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_VC_SIZE_OFFSET (0x94)
#define ACAMERA_ISP_INPUT_PORT_VC_SIZE_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_isp_input_port_vc_size_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x94L);
    system_hw_write_32(0x94L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_isp_input_port_vc_size_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x94L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: frame width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame width.  Read only value.
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_WIDTH_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_WIDTH_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_FRAME_WIDTH_OFFSET (0x98)
#define ACAMERA_ISP_INPUT_PORT_FRAME_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_isp_input_port_frame_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x98L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame height.  Read only value.  
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_HEIGHT_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_HEIGHT_DATASIZE (16)
#define ACAMERA_ISP_INPUT_PORT_FRAME_HEIGHT_OFFSET (0x98)
#define ACAMERA_ISP_INPUT_PORT_FRAME_HEIGHT_MASK (0xffff0000)

// args: data (16-bit)
static __inline uint16_t acamera_isp_input_port_frame_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x98L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: freeze config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to freeze input port configuration.  Used when multiple register writes are required to change input port configuration. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FREEZE_CONFIG_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FREEZE_CONFIG_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FREEZE_CONFIG_OFFSET (0x9c)
#define ACAMERA_ISP_INPUT_PORT_FREEZE_CONFIG_MASK (0x80)
#define ACAMERA_ISP_INPUT_PORT_FREEZE_CONFIG_NORMAL_OPERATION (0)
#define ACAMERA_ISP_INPUT_PORT_FREEZE_CONFIG_HOLD_PREVIOUS_INPUT_PORT_CONFIG_STATE (1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_freeze_config_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x9cL);
    system_hw_write_32(0x9cL, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_isp_input_port_freeze_config_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x9cL) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: mode request
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to stop and start input port.  See ISP guide for further details. 
//        Only modes-0 and 1 are used. all other values are reserved
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_DATASIZE (3)
#define ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_OFFSET (0x9c)
#define ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_MASK (0x7)
#define ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP (0)
#define ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START (1)

// args: data (3-bit)
static __inline void acamera_isp_input_port_mode_request_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x9cL);
    system_hw_write_32(0x9cL, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_isp_input_port_mode_request_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x9cL) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: mode status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//      Used to monitor input port status:
//      bit 0: 1=running, 0=stopped, bits 1,2-reserved
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_MODE_STATUS_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_MODE_STATUS_DATASIZE (3)
#define ACAMERA_ISP_INPUT_PORT_MODE_STATUS_OFFSET (0xa0)
#define ACAMERA_ISP_INPUT_PORT_MODE_STATUS_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_isp_input_port_mode_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0xa0L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: input port frame stats
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: stats reset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Resets the frame statistics registers and starts the sampling period. Note all the frame statistics saturate at 2^31
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_RESET_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_RESET_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_RESET_OFFSET (0xa4)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_frame_stats_stats_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0xa4L);
    system_hw_write_32(0xa4L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_input_port_frame_stats_stats_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0xa4L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: stats hold
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Synchronously disables the update of the statistics registers. This should be used prior to reading out the register values so as to ensure consistent values
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_HOLD_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_HOLD_DATASIZE (1)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_HOLD_OFFSET (0xa8)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_STATS_HOLD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_isp_input_port_frame_stats_stats_hold_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0xa8L);
    system_hw_write_32(0xa8L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_isp_input_port_frame_stats_stats_hold_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0xa8L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active width min
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The minimum number of active pixels observed in a line within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MIN_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MIN_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MIN_OFFSET (0xb4)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_width_min_read(uintptr_t base) {
    return system_hw_read_32(0xb4L);
}
// ------------------------------------------------------------------------------ //
// Register: active width max
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The maximum number of active pixels observed in a line within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MAX_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MAX_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MAX_OFFSET (0xb8)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_width_max_read(uintptr_t base) {
    return system_hw_read_32(0xb8L);
}
// ------------------------------------------------------------------------------ //
// Register: active width sum
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of the active pixels values observed in a line within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_SUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_SUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_SUM_OFFSET (0xbc)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_width_sum_read(uintptr_t base) {
    return system_hw_read_32(0xbcL);
}
// ------------------------------------------------------------------------------ //
// Register: active width num
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The number of lines observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_NUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_NUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_NUM_OFFSET (0xc0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_WIDTH_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_width_num_read(uintptr_t base) {
    return system_hw_read_32(0xc0L);
}
// ------------------------------------------------------------------------------ //
// Register: active height min
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The minimum number of active lines in the frames observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MIN_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MIN_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MIN_OFFSET (0xc4)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_height_min_read(uintptr_t base) {
    return system_hw_read_32(0xc4L);
}
// ------------------------------------------------------------------------------ //
// Register: active height max
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The maximum number of active lines in the frames observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MAX_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MAX_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MAX_OFFSET (0xc8)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_height_max_read(uintptr_t base) {
    return system_hw_read_32(0xc8L);
}
// ------------------------------------------------------------------------------ //
// Register: active height sum
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of active lines in the frames observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_SUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_SUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_SUM_OFFSET (0xcc)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_height_sum_read(uintptr_t base) {
    return system_hw_read_32(0xccL);
}
// ------------------------------------------------------------------------------ //
// Register: active height num
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of frames observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_NUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_NUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_NUM_OFFSET (0xd0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_ACTIVE_HEIGHT_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_active_height_num_read(uintptr_t base) {
    return system_hw_read_32(0xd0L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank min
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The minimum number of horizontal blanking samples observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MIN_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MIN_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MIN_OFFSET (0xd4)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_hblank_min_read(uintptr_t base) {
    return system_hw_read_32(0xd4L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank max
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The maximum number of horizontal blanking samples observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MAX_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MAX_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MAX_OFFSET (0xd8)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_hblank_max_read(uintptr_t base) {
    return system_hw_read_32(0xd8L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank sum
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of the horizontal blanking samples observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_SUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_SUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_SUM_OFFSET (0xdc)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_hblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0xdcL);
}
// ------------------------------------------------------------------------------ //
// Register: hblank num
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of the lines observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_NUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_NUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_NUM_OFFSET (0xe0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_HBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_hblank_num_read(uintptr_t base) {
    return system_hw_read_32(0xe0L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank min
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The minimum number of vertical blanking samples observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MIN_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MIN_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MIN_OFFSET (0xe4)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_vblank_min_read(uintptr_t base) {
    return system_hw_read_32(0xe4L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank max
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The maximum number of vertical blanking samples observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MAX_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MAX_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MAX_OFFSET (0xe8)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_vblank_max_read(uintptr_t base) {
    return system_hw_read_32(0xe8L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank sum
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of the vertical blanking samples observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_SUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_SUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_SUM_OFFSET (0xec)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_vblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0xecL);
}
// ------------------------------------------------------------------------------ //
// Register: vblank num
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// The total number of frames observed within the sampling period
// ------------------------------------------------------------------------------ //

#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_NUM_DEFAULT (0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_NUM_DATASIZE (32)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_NUM_OFFSET (0xf0)
#define ACAMERA_ISP_INPUT_PORT_FRAME_STATS_VBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_isp_input_port_frame_stats_vblank_num_read(uintptr_t base) {
    return system_hw_read_32(0xf0L);
}
// ------------------------------------------------------------------------------ //
#endif //__ACAMERA_ISP_CONFIG_H__
